#if defined(ESP_PLATFORM)

#include "signal_source.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>

#include "esp_log.h"

#ifndef SIGNAL_SOURCE_KIND
#define SIGNAL_SOURCE_KIND 0
#endif

#ifndef SD_INPUT_FILE_PRIMARY
#define SD_INPUT_FILE_PRIMARY "/sdcard/test_data.csv"
#endif

#ifndef SD_INPUT_FILE_FALLBACK
#define SD_INPUT_FILE_FALLBACK "/sdcard/TEST_D~1.CSV"
#endif

#if SIGNAL_SOURCE_KIND == 0

namespace {
static const char *TAG = "signal_source_sd";

bool parse_csv_float(char *line, float &value_out) {
  if (line == nullptr) {
    return false;
  }

  char *token = std::strtok(line, ",;\r\n\t ");
  if (token == nullptr) {
    return false;
  }

  char *end_ptr = nullptr;
  errno = 0;
  float const parsed = std::strtof(token, &end_ptr);
  if (errno != 0 || end_ptr == token) {
    return false;
  }

  value_out = parsed;
  return true;
}
} // namespace

SdCsvSignalSource::SdCsvSignalSource(SdCardService &sd_service) : sd_service_(sd_service), input_file_(nullptr) {}

SdCsvSignalSource::~SdCsvSignalSource() {
  if (input_file_ != nullptr) {
    fclose(input_file_);
    input_file_ = nullptr;
  }
}

esp_err_t SdCsvSignalSource::init() { return open_input_file_() ? ESP_OK : ESP_FAIL; }

esp_err_t SdCsvSignalSource::read_sample(float &sample_out) {
  if (input_file_ == nullptr) {
    return ESP_ERR_INVALID_STATE;
  }

  char line[128] = {0};

  while (true) {
    if (fgets(line, static_cast<int>(sizeof(line)), input_file_) == nullptr) {
      if (ferror(input_file_) != 0) {
        ESP_LOGE(TAG, "Read error while reading SD CSV");
        return ESP_FAIL;
      }

      clearerr(input_file_);
      rewind(input_file_);
      continue;
    }

    if (parse_csv_float(line, sample_out)) {
      return ESP_OK;
    }
  }
}

const char *SdCsvSignalSource::name() const { return "sd-csv"; }

bool SdCsvSignalSource::open_input_file_() {
  input_file_ = sd_service_.open_read(SD_INPUT_FILE_PRIMARY);
  if (input_file_ != nullptr) {
    ESP_LOGI(TAG, "Using SD input file: %s", SD_INPUT_FILE_PRIMARY);
    return true;
  }

  input_file_ = sd_service_.open_read(SD_INPUT_FILE_FALLBACK);
  if (input_file_ != nullptr) {
    ESP_LOGW(TAG, "Using fallback SD input file: %s", SD_INPUT_FILE_FALLBACK);
    return true;
  }

  ESP_LOGE(TAG, "Could not open SD input file (%s or %s)", SD_INPUT_FILE_PRIMARY, SD_INPUT_FILE_FALLBACK);
  return false;
}

#endif // SIGNAL_SOURCE_KIND == 0

#endif // ESP_PLATFORM
