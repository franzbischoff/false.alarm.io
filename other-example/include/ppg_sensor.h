#ifndef PPG_SENSOR_H
#define PPG_SENSOR_H

#include <Arduino.h>
#include <SparkFun_Bio_Sensor_Hub_Library.h>

// PPG Sensor Configuration
#define PPG_DEF_ADDR 0x55
#define PPG_RESPIN 16
#define PPG_MFIOPIN 17
#define PPG_SAMPLES 400
#define PPG_WIDTH 411
#define PPG_FS_HZ 100

// PPG data structure
struct PPGData
{
  uint32_t irLed;
  uint32_t redLed;
  uint16_t heartRate;
  uint8_t confidence;
  uint16_t oxygen;
  uint8_t status;
  unsigned long long timestamp_us; // Timestamp em microssegundos
  bool valid;
};

class PPGSensor
{
private:
  SparkFun_Bio_Sensor_Hub *bio_hub;
  uint32_t sampleIntervalUs;
  uint32_t nextSampleUs;
  uint32_t lastSampleMicros;
  bool initialized;
  bool readyFlag; // Flag para controlar uma leitura por intervalo

  // DC blocking filter state variables
  float irPrev;
  float redPrev;
  float irFilteredPrev;
  float redFilteredPrev;
  float filterAlpha; // High-pass filter coefficient (0.95-0.99)

public:
  PPGSensor();
  ~PPGSensor();

  bool begin();
  bool isReady();
  PPGData readSample();
};

#endif // PPG_SENSOR_H
