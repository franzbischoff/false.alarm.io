#pragma once

#if defined(ESP_PLATFORM)

#include <cstdio>

#include "esp_err.h"
#include "sdmmc_cmd.h"

class SdCardService {
public:
  SdCardService();
  ~SdCardService();

  esp_err_t mount();
  esp_err_t unmount();

  bool is_mounted() const noexcept;

  // Open helper for read-only streams on mounted VFS.
  FILE *open_read(const char *path) const;

  // Prepared for future persistent logging.
  esp_err_t append_line(const char *path, const char *line) const;

private:
  bool mounted_;
  sdmmc_card_t *card_;
};

#endif // ESP_PLATFORM
