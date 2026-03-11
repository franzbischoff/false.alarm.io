#ifndef CONFIG_H
#define CONFIG_H

// WiFi Configuration
#define WIFI_SSID "lsdpi01-ap"
#define WIFI_PASSWORD "lsdconfig2024"

// InfluxDB Configuration
#define INFLUXDB_URL "http://10.42.0.1:8086"
#define INFLUXDB_TOKEN "y1vdIbqiCZ9rEGwnTVTajysdy9YgLpEiji1t08jSEYi4g1SSjGqJsNPoPOCAqkkQ1DaFZWmw2-5aa-6z1HRFEg=="
#define INFLUXDB_ORG "21c00626990a1077"
#define INFLUXDB_BUCKET "health_metrics"

// Configurar batching para reduzir overhead de rede
// Buffer de 50 pontos ou enviar a cada 1000ms
#define BATCH_SIZE 50
#define BATCH_FLUSH_INTERVAL_MS 1000

// Device Configuration
#define DEVICE "ESP32"

// NTP Configuration
// Para rede sem internet, configure um servidor NTP local:
// - Use o IP do seu servidor InfluxDB se ele tiver NTP configurado
// - Ou use um Raspberry Pi/roteador como servidor NTP
// - Ou deixe "pool.ntp.org" se tiver internet
#define NTP_SERVER "10.42.0.1" // Altere para seu servidor NTP local ou "pool.ntp.org"
#define GMT_OFFSET_SEC 0       // GMT+0 (ajuste conforme seu fuso horário)
#define DAYLIGHT_OFFSET_SEC 0  // Sem daylight saving

// Digital Input Configuration
#define DIGITAL_INPUT_PIN 26     // GPIO 26 para trigger
#define INPUT_READ_TIMEOUT 100   // 100 ms de timeout entre leituras
#define INPUT_DEBOUNCE_TIME 2000 // 2 segundos de debounce após transição

// LED Configuration
#define LED_STATE 27           // GPIO do LED de estado
#define LED_BLINK_INTERVAL 500 // 500 ms para piscar
#define LED_ON HIGH
#define LED_OFF LOW

// SD Card Logging Configuration
#define ENABLE_SD_LOGGING       // Comment this line to disable SD logging
#define SD_CS_PIN 5             // Chip Select pin for SD card
#define SD_REPLAY_MAX_LINES 500 // Linhas por ciclo para reenviar backlog

#endif // CONFIG_H
