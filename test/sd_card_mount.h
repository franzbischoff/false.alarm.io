/**
 * @file sd_card_mount.h
 * @brief Helper functions for mounting SD card on ESP32 for testing
 *
 * This file provides SD card mounting functionality specifically for
 * Unity tests on ESP32 with SD card slot.
 *
 * Usage:
 *   #include "sd_card_mount.h"
 *
 *   void app_main() {
 *     #if defined(USE_SD_CARD)
 *     mount_sd_card_for_tests();
 *     #endif
 *     run_all_tests();
 *   }
 */

#ifndef SD_CARD_MOUNT_H
#define SD_CARD_MOUNT_H

#if defined(ESP_PLATFORM) && defined(USE_SD_CARD)

#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/gpio.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include <dirent.h>

static const char *TAG_SD = "sd_card";
static sdmmc_card_t *g_sd_card = NULL;

static void log_dir_entries(const char *path) {
  DIR *dir = opendir(path);
  if (dir == NULL) {
    ESP_LOGW(TAG_SD, "Cannot open dir: %s", path);
    return;
  }

  ESP_LOGI(TAG_SD, "Listing %s", path);
  struct dirent *entry;
  int shown = 0;
  while ((entry = readdir(dir)) != NULL && shown < 20) {
    ESP_LOGI(TAG_SD, "  %s", entry->d_name);
    shown++;
  }
  closedir(dir);
}

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

/**
 * @brief Mount SD card for test file access
 *
 * Mounts SD card at /sdcard using SDSPI interface.
 * Default pinout can be overridden via build flags:
 * -DSD_SPI_MISO_PIN=GPIO_NUM_X
 * -DSD_SPI_MOSI_PIN=GPIO_NUM_X
 * -DSD_SPI_SCK_PIN=GPIO_NUM_X
 * -DSD_SPI_CS_PIN=GPIO_NUM_X
 *
 * @return ESP_OK on success, error code otherwise
 */
static esp_err_t mount_sd_card_for_tests(void) {
  ESP_LOGI(TAG_SD, "Mounting SD card for Unity tests (SDSPI)...");
  ESP_LOGI(TAG_SD, "Pins: MISO=%d MOSI=%d SCK=%d CS=%d", SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_SCK_PIN,
           SD_SPI_CS_PIN);

  const char mount_point[] = "/sdcard";

  // Configure mount options
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024,
  };

  spi_bus_config_t bus_cfg = {
      .mosi_io_num = SD_SPI_MOSI_PIN,
      .miso_io_num = SD_SPI_MISO_PIN,
      .sclk_io_num = SD_SPI_SCK_PIN,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 4000,
  };

  esp_err_t ret = spi_bus_initialize(SD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG_SD, "Failed to initialize SPI bus (%s)", esp_err_to_name(ret));
    return ret;
  }

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.slot = SD_SPI_HOST;
  host.max_freq_khz = SDMMC_FREQ_DEFAULT;

  sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot_config.gpio_cs = SD_SPI_CS_PIN;
  slot_config.host_id = SD_SPI_HOST;

  ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &g_sd_card);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(TAG_SD, "Failed to mount filesystem at /sdcard");
    } else {
      ESP_LOGE(TAG_SD, "Failed to initialize SD card (%s)", esp_err_to_name(ret));
    }
    spi_bus_free(SD_SPI_HOST);
    return ret;
  }

  ESP_LOGI(TAG_SD, "SD card mounted successfully at %s", mount_point);
  sdmmc_card_print_info(stdout, g_sd_card);

  FILE *test_data = fopen("/sdcard/test_data.csv", "r");
  if (test_data) {
    ESP_LOGI(TAG_SD, "Found test_data.csv");
    fclose(test_data);
  } else {
    ESP_LOGW(TAG_SD, "test_data.csv not found on SD card");
  }

  FILE *golden_ref = fopen("/sdcard/golden_reference_nodelete.csv", "r");
  if (golden_ref) {
    ESP_LOGI(TAG_SD, "Found golden_reference_nodelete.csv");
    fclose(golden_ref);
  } else {
    ESP_LOGW(TAG_SD, "golden_reference_nodelete.csv not found on SD card");
  }

  log_dir_entries("/sdcard");
  log_dir_entries("/sdcard/test");

  return ESP_OK;
}

/**
 * @brief Unmount SD card (optional, call at end of tests)
 *
 * @return ESP_OK on success
 */
static esp_err_t unmount_sd_card_for_tests(void) {
  const char mount_point[] = "/sdcard";
  esp_err_t ret = esp_vfs_fat_sdcard_unmount(mount_point, g_sd_card);

  if (ret == ESP_OK) {
    ESP_LOGI(TAG_SD, "SD card unmounted");
    g_sd_card = NULL;
    esp_err_t bus_ret = spi_bus_free(SD_SPI_HOST);
    if (bus_ret != ESP_OK) {
      ESP_LOGW(TAG_SD, "Failed to free SPI bus (%s)", esp_err_to_name(bus_ret));
    }
  } else {
    ESP_LOGE(TAG_SD, "Failed to unmount SD card (%s)", esp_err_to_name(ret));
  }

  return ret;
}

#endif // ESP_PLATFORM && USE_SD_CARD

#endif // SD_CARD_MOUNT_H
