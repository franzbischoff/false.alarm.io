#if defined(ESP_PLATFORM)

#include "sd_card_service.hpp"

#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"

#ifndef SD_INPUT_MOUNT_POINT
#define SD_INPUT_MOUNT_POINT "/sdcard"
#endif

#ifndef SD_SPI_MISO_PIN
#define SD_SPI_MISO_PIN GPIO_NUM_19
#endif

#ifndef SD_SPI_MOSI_PIN
#define SD_SPI_MOSI_PIN GPIO_NUM_23
#endif

#ifndef SD_SPI_SCK_PIN
#define SD_SPI_SCK_PIN GPIO_NUM_18
#endif

#ifndef SD_SPI_CS_PIN
#define SD_SPI_CS_PIN GPIO_NUM_5
#endif

#ifndef SD_SPI_HOST
#define SD_SPI_HOST SPI2_HOST
#endif

namespace {
static const char *TAG = "sd_service";
}

SdCardService::SdCardService() : mounted_(false), card_(nullptr) {}

SdCardService::~SdCardService() {
  if (mounted_) {
    (void)unmount();
  }
}

esp_err_t SdCardService::mount() {
  if (mounted_) {
    return ESP_OK;
  }

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
  mount_config.format_if_mount_failed = false;
  mount_config.max_files = 5;
  mount_config.allocation_unit_size = 16 * 1024;

  spi_bus_config_t bus_cfg = {};
  bus_cfg.mosi_io_num = SD_SPI_MOSI_PIN;
  bus_cfg.miso_io_num = SD_SPI_MISO_PIN;
  bus_cfg.sclk_io_num = SD_SPI_SCK_PIN;
  bus_cfg.quadwp_io_num = -1;
  bus_cfg.quadhd_io_num = -1;
  bus_cfg.max_transfer_sz = 4000;

  esp_err_t ret = spi_bus_initialize(SD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "Failed to initialize SPI bus (%s)", esp_err_to_name(ret));
    return ret;
  }

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.slot = SD_SPI_HOST;
  host.max_freq_khz = SDMMC_FREQ_DEFAULT;

  sdspi_device_config_t slot_cfg = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot_cfg.gpio_cs = SD_SPI_CS_PIN;
  slot_cfg.host_id = SD_SPI_HOST;

  ret = esp_vfs_fat_sdspi_mount(SD_INPUT_MOUNT_POINT, &host, &slot_cfg, &mount_config, &card_);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to mount SD card (%s)", esp_err_to_name(ret));
    (void)spi_bus_free(SD_SPI_HOST);
    return ret;
  }

  mounted_ = true;
  ESP_LOGI(TAG, "SD mounted at %s", SD_INPUT_MOUNT_POINT);
  sdmmc_card_print_info(stdout, card_);

  return ESP_OK;
}

esp_err_t SdCardService::unmount() {
  if (!mounted_) {
    return ESP_OK;
  }

  esp_err_t const ret = esp_vfs_fat_sdcard_unmount(SD_INPUT_MOUNT_POINT, card_);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to unmount SD card (%s)", esp_err_to_name(ret));
    return ret;
  }

  card_ = nullptr;
  mounted_ = false;

  esp_err_t const bus_ret = spi_bus_free(SD_SPI_HOST);
  if (bus_ret != ESP_OK) {
    ESP_LOGW(TAG, "Failed to free SPI bus (%s)", esp_err_to_name(bus_ret));
  }

  ESP_LOGI(TAG, "SD unmounted");
  return ESP_OK;
}

bool SdCardService::is_mounted() const noexcept { return mounted_; }

FILE *SdCardService::open_read(const char *path) const {
  if (!mounted_ || path == nullptr) {
    return nullptr;
  }
  return fopen(path, "r");
}

esp_err_t SdCardService::append_line(const char *path, const char *line) const {
  if (!mounted_ || path == nullptr || line == nullptr) {
    return ESP_ERR_INVALID_ARG;
  }

  FILE *file = fopen(path, "a");
  if (file == nullptr) {
    return ESP_FAIL;
  }

  int const write_chars = fprintf(file, "%s\n", line);
  fclose(file);

  return (write_chars > 0) ? ESP_OK : ESP_FAIL;
}

#endif // ESP_PLATFORM
