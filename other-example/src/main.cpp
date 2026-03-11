#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "ppg_sensor.h"
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <InfluxDbClient.h>

#ifdef ENABLE_SD_LOGGING
#include "sd_logger.h"
#endif
// Simplificado: sem certificados cloud/TLS, usando HTTP para reduzir memória

// Debug macros
#ifdef DEBUG
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTF(...)
#endif

// ============================================================================
// Thread-safe Circular Buffer for PPG Data
// ============================================================================
#define PPG_BUFFER_SIZE 200 // Buffer para ~2 segundos a 100Hz

struct PPGBufferItem
{
  PPGData data;
  unsigned long long unixTimestampNs;
};

static PPGBufferItem ppgBuffer[PPG_BUFFER_SIZE];
static volatile uint16_t bufferWriteIndex = 0;
static volatile uint16_t bufferReadIndex = 0;
static volatile uint16_t bufferCount = 0;
static SemaphoreHandle_t bufferMutex = NULL;
static volatile uint32_t ppgDroppedSamples = 0; // Contador de amostras perdidas

// Thread-safe: Add item to buffer
bool pushPPGData(const PPGData &data, unsigned long long timestamp)
{
  if (xSemaphoreTake(bufferMutex, pdMS_TO_TICKS(10)) == pdTRUE)
  {
    if (bufferCount >= PPG_BUFFER_SIZE)
    {
      ppgDroppedSamples++;
      xSemaphoreGive(bufferMutex);
      return false; // Buffer full
    }

    ppgBuffer[bufferWriteIndex].data = data;
    ppgBuffer[bufferWriteIndex].unixTimestampNs = timestamp;
    bufferWriteIndex = (bufferWriteIndex + 1) % PPG_BUFFER_SIZE;
    bufferCount++;

    xSemaphoreGive(bufferMutex);
    return true;
  }
  ppgDroppedSamples++;
  return false; // Could not acquire mutex
}

// Thread-safe: Get item from buffer
bool popPPGData(PPGBufferItem &item)
{
  if (xSemaphoreTake(bufferMutex, pdMS_TO_TICKS(10)) == pdTRUE)
  {
    if (bufferCount == 0)
    {
      xSemaphoreGive(bufferMutex);
      return false; // Buffer empty
    }

    item = ppgBuffer[bufferReadIndex];
    bufferReadIndex = (bufferReadIndex + 1) % PPG_BUFFER_SIZE;
    bufferCount--;

    xSemaphoreGive(bufferMutex);
    return true;
  }
  return false; // Could not acquire mutex
}

// Get buffer status (thread-safe read)
uint16_t getPPGBufferCount()
{
  uint16_t count = 0;
  if (xSemaphoreTake(bufferMutex, pdMS_TO_TICKS(5)) == pdTRUE)
  {
    count = bufferCount;
    xSemaphoreGive(bufferMutex);
  }
  return count;
}

// Forward declarations
void startPPGTask();
void stopPPGTask();
void ppgReadingTask(void *parameter);

// Time synchronization
bool timeIsSynced = false;
unsigned long long ntpTimestampBaseNs = 0; // Timestamp NTP base em nanosegundos
unsigned long microsBase = 0;              // micros() no momento da sincronização NTP

// Cliente InfluxDB sem certificado (HTTP)
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);

// Declare Data points
Point ppgData("ppg_sensor");

// PPG Sensor instance
PPGSensor ppgSensor;

// PPG Task control
static TaskHandle_t ppgTaskHandle = NULL;
static volatile bool ppgTaskRunning = false;

#ifdef ENABLE_SD_LOGGING
// SD Logger instance
SDLogger sdLogger;
#endif

// Estado do sistema
enum SystemState
{
  WAITING_FOR_TRIGGER, // Esperando sensor ser acionado
  WIFI_CONNECTED,      // WiFi conectado e loop ativo
  WIFI_CONNECTING,     // Conectando ao WiFi
  GRACEFUL_SHUTDOWN    // Desligando WiFi
};

SystemState systemState = WAITING_FOR_TRIGGER;
unsigned long lastInputReadTime = 0;
unsigned long lastStateChangeTime = 0; // Tempo da última mudança de estado
bool lastInputState = false;           // false = LOW, true = HIGH
bool ssidTagAdded = false;             // Flag para adicionar SSID tag apenas uma vez
unsigned long lastLedToggleTime = 0;   // Tempo do último toggle do LED
bool ledState = false;                 // Estado atual do LED (false = off, true = on)

void updateLED()
{
  if (systemState == WAITING_FOR_TRIGGER)
  {
    // LED ligado em HOLD
    digitalWrite(LED_STATE, LED_ON);
    ledState = true;
  }
  else if (systemState == WIFI_CONNECTED)
  {
    // LED piscando em RUN
    if (millis() - lastLedToggleTime >= LED_BLINK_INTERVAL)
    {
      lastLedToggleTime = millis();
      ledState = !ledState;
      digitalWrite(LED_STATE, ledState ? LED_ON : LED_OFF);
    }
  }
  else if (systemState == WIFI_CONNECTING)
  {
    ledState = !ledState;
    digitalWrite(LED_STATE, ledState ? LED_ON : LED_OFF);
  }
}

// Conexão Wi‑Fi minimalista (para resets forçados)
bool connectWiFi()
{
  Serial.printf("Conectando ao Wi‑Fi: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); // estabilidade
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const unsigned long maxWait = 20000; // 20s
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < maxWait)
  {
    updateLED();
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("Conectado.");
    Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
  }
  Serial.println("Falha ao conectar.");
  return false;
}

void disconnectWiFi()
{
  Serial.println("Desconectando do Wi‑Fi...");

  // Stop PPG task first
  stopPPGTask();

#ifdef ENABLE_SD_LOGGING
  // Close SD file to save buffered data
  sdLogger.close();
#endif

  WiFi.disconnect(true); // true = desligar WiFi radio
  systemState = WAITING_FOR_TRIGGER;
  timeIsSynced = false;
  Serial.println("WiFi desconectado. Aguardando novo acionamento...");
}

void syncTime()
{
  Serial.printf("Sincronizando tempo com NTP (%s)...\n", NTP_SERVER);
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

  struct tm timeinfo;
  int attempts = 0;
  while (!getLocalTime(&timeinfo) && attempts < 10)
  {
    Serial.print(".");
    delay(500);
    attempts++;
  }

  if (getLocalTime(&timeinfo))
  {
    Serial.println(" OK!");
    Serial.printf("Tempo sincronizado: %02d:%02d:%02d %02d/%02d/%04d\n",
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                  timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);

    // Capturar timestamp base e micros() no momento da sincronização
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ntpTimestampBaseNs = ((unsigned long long)tv.tv_sec * 1000000000ULL) +
                         ((unsigned long long)tv.tv_usec * 1000ULL);
    microsBase = micros();

    Serial.printf("Base timestamp: %llu ns, micros base: %lu\n", ntpTimestampBaseNs, microsBase);
    timeIsSynced = true;
  }
  else
  {
    Serial.println(" Falhou!");
    timeIsSynced = false;
  }
}

// ============================================================================
// PPG Reading Task (runs on Core 0)
// ============================================================================
void ppgReadingTask(void *parameter)
{
  Serial.println("[PPG Task] Started on core " + String(xPortGetCoreID()));

  const TickType_t xDelay = pdMS_TO_TICKS(1); // Check every 1ms

  while (ppgTaskRunning)
  {
    // Read PPG sensor if ready
    if (ppgSensor.isReady() && timeIsSynced)
    {
      PPGData data = ppgSensor.readSample();

      if (data.valid)
      {
        // Calculate precise timestamp based on micros() since NTP sync
        unsigned long currentMicros = micros();
        unsigned long elapsedMicros = currentMicros - microsBase;
        unsigned long long unixTimestampNs = ntpTimestampBaseNs +
                                             ((unsigned long long)elapsedMicros * 1000ULL);

        // Push to buffer (non-blocking)
        if (!pushPPGData(data, unixTimestampNs))
        {
          // Buffer full or mutex timeout - sample will be dropped
          // Counter is incremented inside pushPPGData
        }
      }
    }

    vTaskDelay(xDelay); // Yield to other tasks
  }

  Serial.println("[PPG Task] Stopped");
  vTaskDelete(NULL);
}

// Start PPG reading task on Core 0
void startPPGTask()
{
  if (ppgTaskHandle == NULL)
  {
    ppgTaskRunning = true;
    xTaskCreatePinnedToCore(
        ppgReadingTask, // Task function
        "PPG_Reader",   // Task name
        4096,           // Stack size (bytes)
        NULL,           // Parameters
        2,              // Priority (higher than loop)
        &ppgTaskHandle, // Task handle
        0               // Core 0 (WiFi/BT stack also runs here)
    );
    Serial.println("[Main] PPG task created on Core 0");
  }
}

// Stop PPG reading task
void stopPPGTask()
{
  if (ppgTaskHandle != NULL)
  {
    Serial.println("[Main] Stopping PPG task...");
    ppgTaskRunning = false;
    vTaskDelay(pdMS_TO_TICKS(100)); // Give time to clean up
    ppgTaskHandle = NULL;
  }
}

void runMainLoop()
{
  // Process buffered PPG data
  PPGBufferItem item;
  while (popPPGData(item))
  {
    const PPGData &data = item.data;
    const unsigned long long unixTimestampNs = item.unixTimestampNs;

    // Clear fields for PPG data
    ppgData.clearFields();

    // Adicionar timestamp em nanosegundos (já calculado na task)
    ppgData.setTime(unixTimestampNs);

    ppgData.addField("ir_led", (int32_t)data.irLed);
    ppgData.addField("red_led", (int32_t)data.redLed);
    ppgData.addField("heart_rate", (int32_t)data.heartRate);
    ppgData.addField("confidence", (int32_t)data.confidence);
    ppgData.addField("oxygen", (int32_t)data.oxygen);
    ppgData.addField("status", (int32_t)data.status);

    String lineProtocol = ppgData.toLineProtocol();
    DEBUG_PRINT("PPG: ");
    DEBUG_PRINTLN(lineProtocol);

    if (WiFi.status() == WL_CONNECTED)
    {
      // Usar writePoint com batching automático
      if (!client.writePoint(ppgData))
      {
        Serial.print("InfluxDB PPG write failed: ");
        Serial.println(client.getLastErrorMessage());

#ifdef ENABLE_SD_LOGGING
        // Enqueue for replay when offline
        sdLogger.enqueueLine(lineProtocol);
#endif
      }
    }
    else
    {
#ifdef ENABLE_SD_LOGGING
      // WiFi desconectado: enfileirar direto
      sdLogger.enqueueLine(lineProtocol);
#endif
    }

#ifdef ENABLE_SD_LOGGING
    // Log to SD card
    sdLogger.logData(unixTimestampNs, data.irLed, data.redLed,
                     data.heartRate, data.confidence, data.oxygen, data.status);
#endif
  } // End while (popPPGData)

  // Report dropped samples periodically
  static unsigned long lastDropReport = 0;
  if (ppgDroppedSamples > 0 && (millis() - lastDropReport) >= 5000)
  {
    Serial.printf("[Warning] PPG dropped samples: %u (buffer full or timeout)\n", ppgDroppedSamples);
    ppgDroppedSamples = 0;
    lastDropReport = millis();
  }

#ifdef ENABLE_SD_LOGGING
  // Handle SD periodic flushes
  sdLogger.loop();

  // Replay queued data when connected
  if (systemState == WIFI_CONNECTED && WiFi.status() == WL_CONNECTED && timeIsSynced)
  {
    static unsigned long lastReplayAttempt = 0;
    if (millis() - lastReplayAttempt >= 2000 && sdLogger.hasQueue())
    {
      lastReplayAttempt = millis();
      sdLogger.replayQueue(client, SD_REPLAY_MAX_LINES);
    }
  }
#endif
}

void setup()
{
  Serial.begin(115200);
  delay(1000); // Dar tempo para o serial inicializar

  // Criar mutex para buffer circular
  bufferMutex = xSemaphoreCreateMutex();
  if (bufferMutex == NULL)
  {
    Serial.println("[ERROR] Failed to create buffer mutex!");
    while (1)
    {
      delay(1000);
    } // Halt
  }
  Serial.println("[Setup] Buffer mutex created");

  // Inicializar LED
  pinMode(LED_STATE, OUTPUT);
  digitalWrite(LED_STATE, LED_OFF); // LED desligado até mensagem de espera
  ledState = false;

  // Inicializar Digital Input
  pinMode(DIGITAL_INPUT_PIN, INPUT_PULLUP);
  Serial.printf("Digital Input configurado no GPIO %d\n", DIGITAL_INPUT_PIN);

  Serial.println("\n=== ESP32 InfluxDB Test ===");
  Serial.printf("SSID configurado: %s\n", WIFI_SSID);

  // Configurar batching do InfluxDB para reduzir overhead de rede
  client.setWriteOptions(WriteOptions().batchSize(BATCH_SIZE).flushInterval(BATCH_FLUSH_INTERVAL_MS));
  Serial.printf("InfluxDB batching: %d pontos ou %d ms\n", BATCH_SIZE, BATCH_FLUSH_INTERVAL_MS);

  // Initialize PPG sensor
  if (!ppgSensor.begin())
  {
    Serial.println("PPG sensor initialization failed!");
  }

#ifdef ENABLE_SD_LOGGING
  // Initialize SD logger (before WiFi/NTP for early logging capability)
  if (!sdLogger.begin())
  {
    Serial.println("Warning: SD logging disabled (card not found or failed to mount)");
  }
#endif

  Serial.println("Aguardando acionamento do Digital Input (D26) para iniciar...");

  // Add tags to the data points
  ppgData.addTag("device", DEVICE);
  // Garantir que LED está ligado no estado HOLD
  digitalWrite(LED_STATE, LED_ON);
  ledState = true;
}
void loop()
{
  // Ler digital input apenas periodicamente (não bloquear o loop principal)
  if (millis() - lastInputReadTime >= INPUT_READ_TIMEOUT)
  {
    bool currentInputState = digitalRead(DIGITAL_INPUT_PIN);
    lastInputReadTime = millis();

    DEBUG_PRINTF("Digital Input (D26) state: %s\n", currentInputState ? "HIGH" : "LOW");

    // Detectar acionamento do sensor (apenas quando HIGH->LOW)
    if (!currentInputState && lastInputState && (millis() - lastStateChangeTime) >= INPUT_DEBOUNCE_TIME)
    {
      Serial.println(">> DIGITAL INPUT ACIONADO! Alternando estado...");
      lastInputState = currentInputState;
      lastStateChangeTime = millis();

      // Alternar entre WAITING e WIFI_CONNECTED
      if (systemState == WAITING_FOR_TRIGGER)
      {
        Serial.println("Iniciando WiFi e loop principal...");

        systemState = WIFI_CONNECTING;

        if (connectWiFi())
        {
          // Sincronizar tempo via NTP
          syncTime();

          if (client.validateConnection())
          {
            Serial.print("Connected to InfluxDB: ");
            Serial.println(client.getServerUrl());
            if (!ssidTagAdded)
            {
              ppgData.addTag("SSID", WiFi.SSID());
              ssidTagAdded = true;
            }
            systemState = WIFI_CONNECTED;

            // Start PPG reading task on Core 0
            startPPGTask();
          }
          else
          {
            Serial.print("InfluxDB connection failed: ");
            Serial.println(client.getLastErrorMessage());
            disconnectWiFi();
            return;
          }
        }
        else
        {
          Serial.println("Falha ao conectar ao WiFi");
          systemState = WAITING_FOR_TRIGGER;
        }
      }
      else if (systemState == WIFI_CONNECTED)
      {
        Serial.println("Desconectando e voltando ao hold...");
        disconnectWiFi();
        return;
      }
    }
    else if (currentInputState)
    {
      lastInputState = currentInputState;
    }
  } // Fim do bloco if digital input

  // Controlar LED baseado no estado do sistema
  updateLED();

  // Executar loop principal quando o sistema estiver ativo
  if (systemState == WIFI_CONNECTED)
  {
    runMainLoop();
  }
}
