#if defined(ESP_PLATFORM)

#include "signal_source.hpp"

#ifndef SIGNAL_SOURCE_KIND
#define SIGNAL_SOURCE_KIND 0
#endif

#if SIGNAL_SOURCE_KIND == 1

#include "driver/adc.h"

#ifndef ANALOG_INPUT_CHANNEL
#define ANALOG_INPUT_CHANNEL ADC1_CHANNEL_6
#endif

esp_err_t AnalogSignalSource::init() {
  esp_err_t const ret = adc1_config_width(ADC_WIDTH_BIT_12);
  if (ret != ESP_OK) {
    return ret;
  }
  return adc1_config_channel_atten(ANALOG_INPUT_CHANNEL, ADC_ATTEN_DB_11);
}

esp_err_t AnalogSignalSource::read_sample(float &sample_out) {
  int const raw = adc1_get_raw(ANALOG_INPUT_CHANNEL);
  sample_out = static_cast<float>(raw);
  return ESP_OK;
}

const char *AnalogSignalSource::name() const { return "analog-adc"; }

#endif // SIGNAL_SOURCE_KIND == 1

#endif // ESP_PLATFORM
