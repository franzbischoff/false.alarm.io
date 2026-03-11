#include "sd_logger.h"
#include <time.h>
#include <InfluxDbClient.h>

SDLogger::SDLogger() : bufferIndex(0), lastFlushTime(0), initialized(false), fileOpen(false)
{
}

SDLogger::~SDLogger()
{
  close();
}

bool SDLogger::begin()
{
  Serial.println("Initializing SD card...");

  if (!SD.begin(SD_CS_PIN))
  {
    Serial.println("SD card initialization failed!");
    return false;
  }

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE)
  {
    Serial.println("No SD card attached!");
    return false;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC)
  {
    Serial.println("MMC");
  }
  else if (cardType == CARD_SD)
  {
    Serial.println("SDSC");
  }
  else if (cardType == CARD_SDHC)
  {
    Serial.println("SDHC");
  }
  else
  {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %llu MB\n", cardSize);

  initialized = true;
  return openNewFile();
}

String SDLogger::generateFileName()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    // If NTP not synced, use millis as fallback
    unsigned long ms = millis();
    return "/PPG_" + String(ms) + ".csv";
  }

  char filename[32];
  strftime(filename, sizeof(filename), "/PPG_%Y%m%d_%H%M%S.csv", &timeinfo);
  return String(filename);
}

bool SDLogger::openNewFile()
{
  if (fileOpen)
  {
    logFile.close();
  }

  currentFileName = generateFileName();
  logFile = SD.open(currentFileName, FILE_WRITE);

  if (!logFile)
  {
    Serial.println("Failed to open SD file for writing!");
    fileOpen = false;
    return false;
  }

  Serial.printf("SD: Logging to %s\n", currentFileName.c_str());

  // Write CSV header
  logFile.println("timestamp_ns,ir_led,red_led,heart_rate,confidence,oxygen,status");
  logFile.flush();

  fileOpen = true;
  bufferIndex = 0;
  lastFlushTime = millis();

  return true;
}

void SDLogger::logData(unsigned long long timestamp_ns, uint32_t irLed, uint32_t redLed,
                       uint16_t heartRate, uint8_t confidence, uint16_t oxygen, uint8_t status)
{
  if (!initialized || !fileOpen)
  {
    return;
  }

  // Add to buffer
  buffer[bufferIndex].timestamp_ns = timestamp_ns;
  buffer[bufferIndex].irLed = irLed;
  buffer[bufferIndex].redLed = redLed;
  buffer[bufferIndex].heartRate = heartRate;
  buffer[bufferIndex].confidence = confidence;
  buffer[bufferIndex].oxygen = oxygen;
  buffer[bufferIndex].status = status;

  bufferIndex++;

  // Flush if buffer is full
  if (bufferIndex >= SD_BUFFER_SIZE)
  {
    flushBuffer();
  }
}

void SDLogger::flushBuffer()
{
  if (!initialized || !fileOpen || bufferIndex == 0)
  {
    return;
  }

  // Write all buffered entries to SD
  for (uint16_t i = 0; i < bufferIndex; i++)
  {
    logFile.printf("%llu,%lu,%lu,%u,%u,%u,%u\n",
                   buffer[i].timestamp_ns,
                   buffer[i].irLed,
                   buffer[i].redLed,
                   buffer[i].heartRate,
                   buffer[i].confidence,
                   buffer[i].oxygen,
                   buffer[i].status);
  }

  logFile.flush();
  bufferIndex = 0;
  lastFlushTime = millis();
}

void SDLogger::loop()
{
  if (!initialized || !fileOpen)
  {
    return;
  }

  // Periodic flush based on time
  if (millis() - lastFlushTime >= SD_FLUSH_INTERVAL_MS)
  {
    flushBuffer();
  }
}

void SDLogger::close()
{
  if (fileOpen)
  {
    flushBuffer(); // Flush any remaining data
    logFile.close();
    fileOpen = false;
    Serial.println("SD: File closed");
  }
}

bool SDLogger::appendLineToFile(const char *path, const String &line)
{
  if (!initialized)
  {
    return false;
  }

  File f = SD.open(path, FILE_APPEND);
  if (!f)
  {
    return false;
  }

  f.println(line);
  f.close();
  return true;
}

bool SDLogger::enqueueLine(const String &lineProtocol)
{
  if (!initialized)
  {
    return false;
  }

  bool ok = appendLineToFile(SD_QUEUE_FILE, lineProtocol);
  if (!ok)
  {
    Serial.println("SD Queue: falha ao armazenar linha");
  }

  return ok;
}

bool SDLogger::rewriteQueueSkippingSent(InfluxDBClient &client, File &inputFile, uint16_t maxLines)
{
  File temp = SD.open("/queue.tmp", FILE_WRITE);
  if (!temp)
  {
    inputFile.close();
    return false;
  }

  uint16_t sentCount = 0;
  bool failed = false;

  while (inputFile.available())
  {
    String line = inputFile.readStringUntil('\n');
    line.trim();
    if (line.length() == 0)
    {
      continue;
    }

    if (!failed && sentCount < maxLines)
    {
      if (client.writeRecord(line))
      {
        sentCount++;
        continue; // Sent, skip writing to temp
      }

      failed = true;
    }

    temp.println(line);
  }

  inputFile.close();
  temp.close();

  SD.remove(SD_QUEUE_FILE);
  SD.rename("/queue.tmp", SD_QUEUE_FILE);

  return !failed;
}

bool SDLogger::replayQueue(InfluxDBClient &client, uint16_t maxLines)
{
  if (!initialized)
  {
    return false;
  }

  if (!SD.exists(SD_QUEUE_FILE))
  {
    return true; // Nothing to replay
  }

  Serial.println("SD Queue: iniciando replay de backlog...");

  File inputFile = SD.open(SD_QUEUE_FILE, FILE_READ);
  if (!inputFile)
  {
    Serial.println("SD Queue: falha ao abrir arquivo de backlog");
    return false;
  }

  bool ok = rewriteQueueSkippingSent(client, inputFile, maxLines);
  Serial.println(ok ? "SD Queue: replay concluido" : "SD Queue: replay interrompido (falha no Influx)");
  return ok;
}

bool SDLogger::hasQueue()
{
  if (!initialized)
  {
    return false;
  }

  if (!SD.exists(SD_QUEUE_FILE))
  {
    return false;
  }

  File f = SD.open(SD_QUEUE_FILE, FILE_READ);
  if (!f)
  {
    return false;
  }

  bool hasData = f.size() > 0;
  f.close();
  return hasData;
}
