#include "ppg_sensor.h"
#include <Wire.h>

// 18-bit output range (0-262143)
static const uint32_t RAW_FULL_SCALE = 262143UL;

PPGSensor::PPGSensor() : bio_hub(nullptr), initialized(false), readyFlag(false)
{
}

PPGSensor::~PPGSensor()
{
  if (bio_hub)
  {
    delete bio_hub;
  }
}

bool PPGSensor::begin()
{
  if (initialized)
  {
    return true;
  }

  Wire.begin();

  bio_hub = new SparkFun_Bio_Sensor_Hub(PPG_RESPIN, PPG_MFIOPIN);

  int result = bio_hub->begin();
  if (result != 0)
  {
    Serial.println("PPG: Could not communicate with sensor!");
    return false;
  }

  Serial.println("PPG: Sensor started!");

  bio_hub->configSensor();
  bio_hub->setPulseWidth(PPG_WIDTH);
  bio_hub->setSampleRate(PPG_SAMPLES);

  int error = bio_hub->configSensorBpm(MODE_ONE);
  if (error != 0)
  {
    Serial.print("PPG: Error configuring sensor: ");
    Serial.println(error);
    return false;
  }

  Serial.println("PPG: Sensor configured.");

  // Initialize timing
  sampleIntervalUs = 1000000UL / PPG_FS_HZ;
  nextSampleUs = micros() + sampleIntervalUs;
  lastSampleMicros = 0;
  readyFlag = false;

  // Initialize DC blocking filter
  // Alpha = 0.98 gives cutoff frequency ~0.3 Hz at 100 Hz sampling rate
  filterAlpha = 0.98f;
  irPrev = 0.0f;
  redPrev = 0.0f;
  irFilteredPrev = 0.0f;
  redFilteredPrev = 0.0f;

  initialized = true;

  // Give sensor time to stabilize
  delay(4000);

  return true;
}

bool PPGSensor::isReady()
{
  if (!initialized)
  {
    return false;
  }

  // Só retorna true UMA VEZ quando o intervalo é atingido
  if (!readyFlag && (int32_t)(micros() - nextSampleUs) >= 0)
  {
    readyFlag = true;
    return true;
  }

  return false;
}

PPGData PPGSensor::readSample()
{
  PPGData data;
  data.valid = false;

  if (!initialized)
  {
    return data;
  }

  unsigned long currentMicros = micros();

  lastSampleMicros = currentMicros;
  // Agendar próxima leitura a partir do tempo real atual
  nextSampleUs = currentMicros + sampleIntervalUs;
  readyFlag = false; // Resetar flag para próxima leitura

  // Capturar timestamp ANTES da leitura para máxima precisão
  data.timestamp_us = (unsigned long long)currentMicros;

  bioData body = bio_hub->readSensorBpm();

  // Clamp and invert raw values
  uint32_t ir_clamped = min((uint32_t)body.irLed, RAW_FULL_SCALE);
  uint32_t red_clamped = min((uint32_t)body.redLed, RAW_FULL_SCALE);
  float irRaw = (float)(RAW_FULL_SCALE - ir_clamped);
  float redRaw = (float)(RAW_FULL_SCALE - red_clamped);

  // Apply DC blocking filter: y[n] = x[n] - x[n-1] + alpha * y[n-1]
  // This removes baseline wander while preserving AC component (heartbeat)
  float irFiltered = irRaw - irPrev + filterAlpha * irFilteredPrev;
  float redFiltered = redRaw - redPrev + filterAlpha * redFilteredPrev;

  // Update filter state
  irPrev = irRaw;
  redPrev = redRaw;
  irFilteredPrev = irFiltered;
  redFilteredPrev = redFiltered;

  // Convert filtered signals back to uint32_t (offset to positive range)
  // Add half of raw scale to center the signal
  data.irLed = (uint32_t)max(0.0f, min((float)RAW_FULL_SCALE, irFiltered + (RAW_FULL_SCALE / 2.0f)));
  data.redLed = (uint32_t)max(0.0f, min((float)RAW_FULL_SCALE, redFiltered + (RAW_FULL_SCALE / 2.0f)));

  data.heartRate = body.heartRate;
  data.confidence = body.confidence;
  data.oxygen = body.oxygen;
  data.status = body.status;
  data.valid = true;

  return data;
}
