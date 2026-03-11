#ifndef SD_LOGGER_H
#define SD_LOGGER_H

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

class InfluxDBClient;

// SD Logger Configuration
#define SD_CS_PIN 5               // Chip Select pin for SD card
#define SD_BUFFER_SIZE 100        // Number of samples to buffer before writing
#define SD_FLUSH_INTERVAL_MS 1000 // Flush to SD every 1 second
#define SD_QUEUE_FILE "/queue.lp" // Line protocol queue file for offline backlog

// PPG data structure for SD logging
struct SDLogEntry
{
  unsigned long long timestamp_ns;
  uint32_t irLed;
  uint32_t redLed;
  uint16_t heartRate;
  uint8_t confidence;
  uint16_t oxygen;
  uint8_t status;
};

class SDLogger
{
private:
  File logFile;
  String currentFileName;
  SDLogEntry buffer[SD_BUFFER_SIZE];
  uint16_t bufferIndex;
  unsigned long lastFlushTime;
  bool initialized;
  bool fileOpen;

  bool appendLineToFile(const char *path, const String &line);
  bool rewriteQueueSkippingSent(InfluxDBClient &client, File &inputFile, uint16_t maxLines);

  String generateFileName();
  bool openNewFile();
  void flushBuffer();

public:
  SDLogger();
  ~SDLogger();

  bool begin();
  void logData(unsigned long long timestamp_ns, uint32_t irLed, uint32_t redLed,
               uint16_t heartRate, uint8_t confidence, uint16_t oxygen, uint8_t status);
  void loop(); // Call periodically to handle timed flushes
  void close();
  bool enqueueLine(const String &lineProtocol);
  bool replayQueue(InfluxDBClient &client, uint16_t maxLines = 20);
  bool hasQueue();
};

#endif // SD_LOGGER_H
