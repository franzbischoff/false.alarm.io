#pragma once

#if defined(ESP_PLATFORM)

#include "esp_err.h"
#include "sd_card_service.hpp"

enum class SignalSourceKind : int {
  kSdCsv = 0,
  kAnalog = 1,
  kI2cSensor = 2,
};

class ISignalSource {
public:
  virtual ~ISignalSource() = default;

  virtual esp_err_t init() = 0;
  virtual esp_err_t read_sample(float &sample_out) = 0;
  virtual const char *name() const = 0;
};

class SdCsvSignalSource final : public ISignalSource {
public:
  explicit SdCsvSignalSource(SdCardService &sd_service);
  ~SdCsvSignalSource() override;

  esp_err_t init() override;
  esp_err_t read_sample(float &sample_out) override;
  const char *name() const override;

private:
  bool open_input_file_();

  SdCardService &sd_service_;
  FILE *input_file_;
};

class AnalogSignalSource final : public ISignalSource {
public:
  esp_err_t init() override;
  esp_err_t read_sample(float &sample_out) override;
  const char *name() const override;
};

class I2cSensorSignalSource final : public ISignalSource {
public:
  esp_err_t init() override;
  esp_err_t read_sample(float &sample_out) override;
  const char *name() const override;
};

#endif // ESP_PLATFORM
