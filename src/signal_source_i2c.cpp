#if defined(ESP_PLATFORM)

#include "signal_source.hpp"

#ifndef SIGNAL_SOURCE_KIND
#define SIGNAL_SOURCE_KIND 0
#endif

#if SIGNAL_SOURCE_KIND == 2

#include "esp_log.h"

namespace {
static const char *TAG = "signal_source_i2c";
}

esp_err_t I2cSensorSignalSource::init() {
  ESP_LOGW(TAG, "I2C sensor source is a placeholder for now");
  return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t I2cSensorSignalSource::read_sample(float &sample_out) {
  (void)sample_out;
  return ESP_ERR_NOT_SUPPORTED;
}

const char *I2cSensorSignalSource::name() const { return "i2c-sensor"; }

#endif // SIGNAL_SOURCE_KIND == 2

#endif // ESP_PLATFORM
