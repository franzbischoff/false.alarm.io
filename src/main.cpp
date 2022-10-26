// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include <esp_log.h>
#include <esp_system.h>
#include <esp_littlefs.h>
// #include <esp_dsp.h>
// #include "sdkconfig.h"
// #include <nvs_flash.h>
// #include <sys/param.h>
// #include <cstring>
// #include <fstream>
// #include <iostream>
#include <cmath>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/ringbuf.h>

#include <Mpx.hpp>

// #include <SparkFun_Bio_Sensor_Hub_Library.hpp>

static char *TAG = "main";

//>> Matrix Profile settings are defined on compile time
#define RESPIN 4
#define MFIOPIN 2
#define MYSDA 21
#define MYSCL 22
#define WIN_SIZE WINDOW_SIZE
#define S_SAMPLES SENSOR_SAMPLES
#define S_WIDTH SENSOR_WIDTH
#define SAMPLING_HZ SAMPLING_RATE_HZ
#define HIST_SIZE (HISTORY_SIZE_S * SAMPLING_HZ)
#define FLOSS_LANDMARK (HIST_SIZE - (SAMPLING_HZ * FLOSS_LANDMARK_S))
#define BUFFER_SIZE (WIN_SIZE + WIN_SIZE) // must be at least 2x the window_size
#define QUEUE_SIZE (BUFFER_SIZE + 20)     // queue must have a little more room

#ifndef SHORT_FILTER
#define SHORT_FILTER (SAMPLING_HZ / 10)
#endif
#ifndef WANDER_FILTER
#define WANDER_FILTER (SAMPLING_HZ / 2)
#endif

// ESP_PLATFORM

//>> multitask configurations
void task_compute(void *pv_parameters);
void task_read_signal(void *pv_parameters);
void mon_task(void *pv_parameters);
void idle_task(void *pv_parameters);

enum { CORE_0 = 0, CORE_1 = 1 };

// float values[QUEUE_SIZE]; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
// uint16_t recv_count = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
// uint32_t read_line = 0;   // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
// MatrixProfile::Mpx mpx(WIN_SIZE, 0.5F, 0, HIST_SIZE); // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
bool buffer_init = false; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

FILE *file;
RingbufHandle_t ring_buf; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void task_compute(void *pv_parameters) // This is a task.
{
  (void)pv_parameters;

  const uint16_t floss_landmark = FLOSS_LANDMARK;
  float buffer[BUFFER_SIZE];
  uint16_t recv_count;
  int16_t delay_adjust = 100;

  for (uint16_t i = 0; i < BUFFER_SIZE; i++) { // NOLINT(modernize-loop-convert)
    buffer[i] = 0.0F;
  }

  MatrixProfile::Mpx mpx(WIN_SIZE, 0.5F, 0, HIST_SIZE);
  mpx.prune_buffer();

  for (;;) // -H776 A Task shall never return or exit.
  {

    if (buffer_init) { // wait for buffer to be initialized
      recv_count = 0;

      for (uint16_t i = 0; i < BUFFER_SIZE; i++) {

        size_t item_size; // size in bytes (usually 4 bytes)
        float *data = (float *)xRingbufferReceive(ring_buf, &item_size, 0);

        if (item_size > 4) {
          ESP_LOGD(TAG, "Item_size: %u", item_size);
        }

        if (data != nullptr) {
          buffer[i] = *data; // read data from the ring buffer
          recv_count = i + 1;
          vRingbufferReturnItem(ring_buf, (void *)data);
        } else {
          break; // no more data in the ring buffer
        }
      }

      if (recv_count > 0) {
        // ESP_LOGD(TAG, "Compute %u\n", uxTaskGetStackHighWaterMark(NULL));

        mpx.compute(buffer, recv_count); /////////////////
        mpx.floss();

        // const uint16_t profile_len = mpx.get_profile_len();
        float *floss = mpx.get_floss();
        float *matrix = mpx.get_matrix();

        // for (uint16_t i = 0; i < recv_count; i++) {
        //   ESP_LOGI(TAG, "%.1f %.2f %.2f", buffer[i], floss[floss_landmark + i],
        //            matrix[floss_landmark + i]); // indexes[floss_landmark + i]);
        // }
        // char log_buf[64];
        // for (uint16_t i = 0; i < recv_count; i++) {
        //   // sprintf(log_buf, "%.1f %.2f %.2f", buffer[i], floss[floss_landmark + i],
        //   //                matrix[floss_landmark + i]);
        //   sprintf(log_buf, "%.1f %.2f", buffer[i], matrix[floss_landmark + i]);
        //   esp_rom_printf("%s\n", log_buf); // indexes[floss_landmark + i]);
        // }
        // ESP_LOGD(TAG, "[Consumer] %d\n", recv_count); // handle about 400 samples per second
        if (recv_count > 40) {
          delay_adjust -= 5;
          if (delay_adjust <= 0) {
            delay_adjust = 1;
          }
        } else {
          if (recv_count < 20) {
            delay_adjust += 5;
          }
        }
      }
    } else {
      // ESP_LOGD(TAG, "-1, -1\n");
      vTaskDelay((portTICK_PERIOD_MS * 2)); // for stability
    }

    vTaskDelay((portTICK_PERIOD_MS * delay_adjust)); // for stability
  }
}

void task_read_signal(void *pv_parameters) // This is a task.
{
  (void)pv_parameters;

  TickType_t last_wake_time;

  uint16_t initial_counter = 0;
  float ir_res;
  const uint32_t timer_interval = 1000U / SAMPLING_HZ; // 4ms = 250Hz

#ifndef FILE_DATA
  // Reset pin, MFIO pin
  const uint16_t res_pin = RESPIN;
  const uint16_t mfio_pin = MFIOPIN;
  // Possible widths: 69, 118, 215, 411us
  const uint16_t width = S_WIDTH;
  // Possible samples: 50, 100, 200, 400, 800, 1000, 1600, 3200 samples/second
  // Not every sample amount is possible with every width; check out our hookup
  // guide for more information.
  const uint16_t samples = S_SAMPLES;

  // short period filter
  const float s_window = SHORT_FILTER;
  const float eps_f = 0.05F;
  const float alpha = powf(eps_f, 1.0F / s_window);

  // large (wander) period filter
  const float l_window = WANDER_FILTER;
  const float l_alpha = powf(eps_f, 1.0F / l_window);

  // Takes address, reset pin, and MFIO pin.
  // SparkFun_Bio_Sensor_Hub bio_hub(res_pin, mfio_pin);
  // bioData body;

  uint32_t ir_led;
  bool sensor_started = false;

  float ir_sum = 0.0F;
  float ir_num = 0.0F;
  float ir_sum2 = 0.0F;
  float ir_num2 = 0.0F;

  // Wire.begin();
  // bio_hub.begin();

  // bio_hub.configSensor();
  // bio_hub.setPulseWidth(width);   // 18 bits resolution (0-262143)
  // bio_hub.setSampleRate(samples); // 18 bits resolution (0-262143)
#endif

  ESP_LOGD(TAG, "task_read_signal started");

  vTaskDelay((portTICK_PERIOD_MS * 1000)); // Wait for sensor to stabilize

  last_wake_time = xTaskGetTickCount();

  for (;;) // -H776 A Task shall never return or exit.
  {
    vTaskDelayUntil(&last_wake_time, (portTICK_PERIOD_MS * timer_interval)); // for stability

#ifndef FILE_DATA
    // body = bio_hub.readSensor(); // Read the sensor outside the IRQ, to avoid overload
    // ir_led = body.irLed;
    ir_led = 0;

    if (!sensor_started) {
      if (ir_led == 0) {
        sensor_started = true;
        initial_counter = 0;

        ESP_LOGD(TAG, "[Producer] Sensor started, now it can be used.\n");
      } else {
        initial_counter++;
        if (initial_counter > 2500) {
          ESP_LOGD(TAG, "[Producer] Sensor not properly started, rebooting...\n");
          esp_restart();
        }
      }
    }

    // 70744 // t 318384 > 363368 // 277,849/1,310,720 ;; 16,976/327,680
    // getMaxAllocHeap: 69620

    if (ir_led > 10000) {
      // ESP_LOGD(TAG, "Read %u\n", uxTaskGetStackHighWaterMark(NULL)); // 176

      ir_sum = ir_sum * alpha + (float)ir_led;
      ir_num = ir_num * alpha + 1.0F;
      ir_sum2 = ir_sum2 * l_alpha + (float)ir_led;
      ir_num2 = ir_num2 * l_alpha + 1.0F;
      ir_res = (ir_sum / ir_num - ir_sum2 / ir_num2);
      ir_res /= 20.0F;
#else

    ir_res = 0.0F;

    if (file != nullptr) {

      char line[20];

      if (fgets(line, sizeof(line), file) == nullptr) {
        ESP_LOGI(TAG, "End of file reached, rewind");
        rewind(file);
        // file = nullptr;
        // All done, unmount partition and disable LittleFS

        // ESP_LOGI(TAG, "LittleFS unmounted");

      } else {
        float v = std::stof(line);
        ir_res = v;
      }
    }

#endif

      if (ir_res > 50.0F || ir_res < -50.0F) {
        continue;
      }

      UBaseType_t res = xRingbufferSend(ring_buf, &ir_res, sizeof(ir_res), (portTICK_PERIOD_MS * 100));

      if (res == pdTRUE) {
        if (!buffer_init) {
          if (++initial_counter >= BUFFER_SIZE) {
            buffer_init = true; // this is read by the receiver task
            ESP_LOGD(TAG, "[Producer] DEBUG: Buffer started, starting to compute\n");
          }
        }
      } else {
        ESP_LOGD(TAG, "Failed to send item (timeout), %u", initial_counter);
      }
#ifndef FILE_DATA
    } else {
      // ESP_LOGD(TAG, "[Producer] IR: %d\n", body.irLed);
      vTaskDelay((portTICK_PERIOD_MS * 1)); // for stability
    }
#endif
  }
}

#ifdef __cplusplus
extern "C" {
#endif
void app_main(void) {

  esp_log_level_set(TAG, ESP_LOG_DEBUG);
  esp_log_level_set("mpx", ESP_LOG_ERROR);

  ESP_LOGD(TAG, "Heap: %u", esp_get_free_heap_size());
  ESP_LOGD(TAG, "Max alloc Heap: %u", esp_get_minimum_free_heap_size());
  ESP_LOGD(TAG, "SDK version: %s", esp_get_idf_version());

  /* Print chip information */
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  ESP_LOGD(TAG, "[Producer] ESP32%s rev %d, %d CPU cores, WiFi%s%s%s, ", (chip_info.model & CHIP_ESP32S2) ? "-S2" : "",
           chip_info.revision, chip_info.cores, (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? "/802.15.4" : "");

  // ESP_LOGD(TAG, "%uMB %s flash", spi_flash_get_chip_size() / (uint32_t)(1024 * 1024),
  //        (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

  // if(psramFound()) {
  //   ESP_LOGD(TAG, ", %uMB %s PSRAM\n", esp_spiram_get_size() / (uint32_t)(1024 * 1024),
  //        (chip_info.features & CHIP_FEATURE_EMB_PSRAM) ? "embedded" : "external");
  // } else {
  //   ESP_LOGD(TAG, ", No PSRAM found\n");
  // }

  ring_buf = xRingbufferCreate(QUEUE_SIZE * (sizeof(float) + 8) /*overhead*/, RINGBUF_TYPE_NOSPLIT);

  if (ring_buf == nullptr) {
    ESP_LOGE(TAG, "[Setup] Error creating the ring_buf. Aborting.");
    esp_restart();
  }

  ESP_LOGI(TAG, "Initializing LittleFS");

  esp_vfs_littlefs_conf_t const conf = {
      .base_path = "/littlefs",
      .partition_label = "littlefs",
      .format_if_mount_failed = false,
      .dont_mount = false,
  };

  // Use settings defined above to initialize and mount LittleFS filesystem.
  // Note: esp_vfs_littlefs_register is an all-in-one convenience function.
  esp_err_t ret = esp_vfs_littlefs_register(&conf);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(TAG, "Failed to mount or format filesystem");
    } else if (ret == ESP_ERR_NOT_FOUND) {
      ESP_LOGE(TAG, "Failed to find LittleFS partition");
    } else {
      ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
    }
    return;
  }

  size_t total = 0, used = 0;
  ret = esp_littlefs_info(conf.partition_label, &total, &used);

  ESP_LOGI(TAG, "LittleFS: %u / %u", used, total);

  if (esp_littlefs_mounted("littlefs")) {
    ESP_LOGI(TAG, "LittleFS mounted");
  } else {
    ESP_LOGE(TAG, "LittleFS not mounted");
  }

  file = fopen("/littlefs/floss.csv", "r");

  if (file == nullptr) {
    ESP_LOGE(TAG, "Failed to open file for reading");
    esp_vfs_littlefs_unregister(conf.partition_label);
    return;
  }

  ESP_LOGD(TAG, "Creating task 0");

  xTaskCreatePinnedToCore(task_read_signal, // Task function
                          "ReadSignal",     // Just a name
                          40000,            // Stack size in `word`s
                          nullptr,          // Parameter passed as input of the task (can be NULL)
                          3, // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
                          nullptr, // Task Handle (can be NULL)
                          CORE_0);

  ESP_LOGD(TAG, "Creating task 1");

  xTaskCreatePinnedToCore(task_compute, // Task function
                          "Compute",    // Just a name
                          40000, // This stack size in `word`s can be checked & adjusted by reading the Stack Highwater
                          nullptr, // Parameter passed as input of the task (can be NULL)
                          3, // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
                          nullptr, // Task Handle (can be NULL)
                          CORE_1);

  ESP_LOGD(TAG, "Main is done");
  while (1) {

    char taskbuffer[100];

    vTaskGetRunTimeStats(taskbuffer);
    ESP_LOGI(TAG, "\n%s", taskbuffer);

    vTaskDelay((portTICK_PERIOD_MS * 5000));
  }
}
#ifdef __cplusplus
}
#endif
