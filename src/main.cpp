#if !defined(ESP_PLATFORM)

#include <cstdio>

int main() {
  std::puts("native build: ESP-IDF app_main is disabled");
  return 0;
}

#else

#include <array>
#include <cstdio>
#include <memory>

#include "Mpx.hpp"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sd_card_service.hpp"
#include "signal_source.hpp"

#ifndef MAIN_LOG_LEVEL
#define MAIN_LOG_LEVEL ESP_LOG_INFO
#endif

#ifndef SAMPLING_RATE_HZ
#define SAMPLING_RATE_HZ 250
#endif

#ifndef WINDOW_SIZE
#define WINDOW_SIZE 100
#endif

#ifndef HISTORY_SIZE_S
#define HISTORY_SIZE_S 20
#endif

#ifndef FLOSS_ALERT_THRESHOLD
#define FLOSS_ALERT_THRESHOLD 0.45F
#endif

#ifndef SIGNAL_SOURCE_KIND
#define SIGNAL_SOURCE_KIND 0
#endif

#ifndef APP_DEBUG_OUTPUT
#define APP_DEBUG_OUTPUT 1
#endif

#ifndef DEBUG_LOG_EVERY_N_SAMPLES
#define DEBUG_LOG_EVERY_N_SAMPLES 25
#endif

#ifndef LOG_TO_SD_ENABLED
#define LOG_TO_SD_ENABLED 0
#endif

#ifndef RING_BUFFER_CAPACITY_SAMPLES
#define RING_BUFFER_CAPACITY_SAMPLES 500
#endif

#ifndef MPX_BATCH_SIZE
#define MPX_BATCH_SIZE 16
#endif

#ifndef TASK_ACQ_CORE
#define TASK_ACQ_CORE 0
#endif

#ifndef TASK_PROC_CORE
#define TASK_PROC_CORE 1
#endif

#ifndef TASK_MON_CORE
#define TASK_MON_CORE 1
#endif

#ifndef TASK_ACQ_PRIORITY
#define TASK_ACQ_PRIORITY (tskIDLE_PRIORITY + 4)
#endif

#ifndef TASK_PROC_PRIORITY
#define TASK_PROC_PRIORITY (tskIDLE_PRIORITY + 3)
#endif

#ifndef TASK_MON_PRIORITY
#define TASK_MON_PRIORITY (tskIDLE_PRIORITY + 1)
#endif

#ifndef TASK_ACQ_STACK_BYTES
#define TASK_ACQ_STACK_BYTES 8192
#endif

#ifndef TASK_PROC_STACK_BYTES
#define TASK_PROC_STACK_BYTES 16384
#endif

#ifndef TASK_MON_STACK_BYTES
#define TASK_MON_STACK_BYTES 6144
#endif

#ifndef ENABLE_MONITOR_TASK
#define ENABLE_MONITOR_TASK 1
#endif

#ifndef DEBUG_MONITOR_PERIOD_MS
#define DEBUG_MONITOR_PERIOD_MS 2000
#endif

#ifndef SERIAL_PLOT_MODE
#define SERIAL_PLOT_MODE 0
#endif

#ifndef SERIAL_PLOT_EVERY_N
#define SERIAL_PLOT_EVERY_N 1
#endif

#ifndef SERIAL_PLOT_TELEPLOT_FORMAT
#define SERIAL_PLOT_TELEPLOT_FORMAT 0
#endif

#ifndef PROCESS_TASK_COOPERATIVE_DELAY_MS
#define PROCESS_TASK_COOPERATIVE_DELAY_MS 0
#endif

namespace {
static const char *TAG = "main";

constexpr TickType_t kLoopTick = pdMS_TO_TICKS(1000U / SAMPLING_RATE_HZ);
constexpr uint16_t kWindowSize = WINDOW_SIZE;
constexpr uint16_t kHistorySamples = static_cast<uint16_t>(SAMPLING_RATE_HZ * HISTORY_SIZE_S);

struct SignalPacket {
  float sample;
  uint64_t timestamp_us;
};

struct RuntimeContext {
  QueueHandle_t queue;
  ISignalSource *source;
#if LOG_TO_SD_ENABLED
  SdCardService *sd_service;
#endif
};

TaskHandle_t g_task_acq = nullptr;
TaskHandle_t g_task_proc = nullptr;
TaskHandle_t g_task_mon = nullptr;

volatile uint32_t g_dropped_samples = 0U;
volatile uint32_t g_produced_samples = 0U;
volatile uint32_t g_processed_samples = 0U;

uint16_t compute_floss_probe_index(uint16_t profile_len) {
  uint16_t const probe_offset = static_cast<uint16_t>(2U * kWindowSize);
  if (profile_len > probe_offset) {
    return static_cast<uint16_t>(profile_len - probe_offset);
  }
  return 0U;
}

void task_acquire_signal(void *pv_parameters) {
  auto *ctx = static_cast<RuntimeContext *>(pv_parameters);
  TickType_t last_wake_time = xTaskGetTickCount();

  for (;;) {
    SignalPacket packet = {0.0F, 0U};

    esp_err_t const read_ret = ctx->source->read_sample(packet.sample);
    if (read_ret == ESP_OK) {
      packet.timestamp_us = static_cast<uint64_t>(esp_timer_get_time());
      if (xQueueSend(ctx->queue, &packet, 0) != pdTRUE) {
        g_dropped_samples++;
      } else {
        g_produced_samples++;
      }
    } else {
      ESP_LOGW(TAG, "Acquisition read failed (%s)", esp_err_to_name(read_ret));
    }

    vTaskDelayUntil(&last_wake_time, kLoopTick);
  }
}

void task_process_signal(void *pv_parameters) {
  auto *ctx = static_cast<RuntimeContext *>(pv_parameters);
  MatrixProfile::Mpx mpx(kWindowSize, 0.5F, 0U, kHistorySamples);
  mpx.prune_buffer();

  std::array<float, MPX_BATCH_SIZE> samples{};
  SignalPacket packet = {0.0F, 0U};

#if APP_DEBUG_OUTPUT
  uint32_t debug_counter = 0U;
#endif
#if SERIAL_PLOT_MODE
  uint32_t serial_plot_counter = 0U;
#endif
#if PROCESS_TASK_COOPERATIVE_DELAY_MS == 0
  uint32_t wdt_yield_counter = 0U;
#endif

  for (;;) {
    if (xQueueReceive(ctx->queue, &packet, portMAX_DELAY) != pdTRUE) {
      continue;
    }

    uint16_t recv_count = 0U;
    samples[recv_count++] = packet.sample;

    while (recv_count < static_cast<uint16_t>(samples.size())) {
      SignalPacket next_packet = {0.0F, 0U};
      if (xQueueReceive(ctx->queue, &next_packet, 0) != pdTRUE) {
        break;
      }
      packet = next_packet;
      samples[recv_count++] = next_packet.sample;
    }

    (void)mpx.compute(samples.data(), recv_count);
    mpx.floss();
    g_processed_samples += recv_count;

    uint16_t const profile_len = mpx.get_profile_len();
    uint16_t const floss_probe_index = compute_floss_probe_index(profile_len);
    float const *floss_profile = mpx.get_floss();

    uint16_t min_floss_index = 0U;
    float min_floss_value = 0.0F;
    uint16_t const min_search_len = (profile_len > kWindowSize) ? static_cast<uint16_t>(profile_len - kWindowSize) : 0U;
    uint16_t const data_buffer_mid = static_cast<uint16_t>(mpx.get_buffer_size() / 2U);
    uint16_t const min_search_start = (data_buffer_mid < min_search_len) ? data_buffer_mid : 0U;
    if (min_search_len > 0U) {
      min_floss_index = min_search_start;
      min_floss_value = floss_profile[min_search_start];
      for (uint16_t i = static_cast<uint16_t>(min_search_start + 1U); i < min_search_len; ++i) {
        if (floss_profile[i] < min_floss_value) {
          min_floss_value = floss_profile[i];
          min_floss_index = i;
        }
      }
    }

    float const floss_value = (profile_len > 0U) ? floss_profile[floss_probe_index] : 0.0F;
    float const latest_sample = samples[recv_count - 1U];

    if (floss_value <= FLOSS_ALERT_THRESHOLD) {
      ESP_LOGW(TAG, "ALERT: floss[%u]=%.5f <= %.5f (ts=%llu)", floss_probe_index, floss_value, FLOSS_ALERT_THRESHOLD,
               static_cast<unsigned long long>(packet.timestamp_us));
    }

#if APP_DEBUG_OUTPUT
    debug_counter += recv_count;
    if (debug_counter >= DEBUG_LOG_EVERY_N_SAMPLES) {
      debug_counter = 0U;
      ESP_LOGI(TAG, "dbg: source=%s sample=%.5f floss[%u]=%.5f ts=%llu", ctx->source->name(), latest_sample,
               floss_probe_index, floss_value, static_cast<unsigned long long>(packet.timestamp_us));
    }
#endif

#if SERIAL_PLOT_MODE
    for (uint16_t i = 0U; i < recv_count; ++i) {
      serial_plot_counter++;
      if (serial_plot_counter >= SERIAL_PLOT_EVERY_N) {
        serial_plot_counter = 0U;
#if SERIAL_PLOT_TELEPLOT_FORMAT
        std::printf(">sample:%.6f\n", samples[i]);
        std::printf(">floss:%.6f\n", floss_value);
        std::printf(">min_floss_index:%u\n", static_cast<unsigned>(min_floss_index));
        std::printf(">min_floss_value:%.6f\n", min_floss_value);
#else
        std::printf("%.6f,%.6f,%u,%.6f\n", samples[i], floss_value, static_cast<unsigned>(min_floss_index),
                    min_floss_value);
#endif
      }
    }
#endif

#if LOG_TO_SD_ENABLED
    if (ctx->sd_service->is_mounted()) {
      char log_line[160] = {0};
      std::snprintf(log_line, sizeof(log_line), "ts_us=%llu,sample=%.6f,floss[%u]=%.6f",
                    static_cast<unsigned long long>(packet.timestamp_us), latest_sample, floss_probe_index,
                    floss_value);
      (void)ctx->sd_service->append_line("/sdcard/floss_debug.log", log_line);
    }
#endif

#if PROCESS_TASK_COOPERATIVE_DELAY_MS > 0
    // Give IDLE task time to run when processing+UART output keeps this loop hot.
    vTaskDelay(pdMS_TO_TICKS(PROCESS_TASK_COOPERATIVE_DELAY_MS));
#else
    // taskYIELD() only yields to tasks of equal or higher priority, so IDLE
    // (priority 0) never runs. Instead, block for 1 tick every 10 iterations
    // (~0.64s at 250 Hz / batch-16) — well within the 5s WDT timeout, with
    // < 0.16% CPU overhead.
    if (++wdt_yield_counter >= 3U) {
      wdt_yield_counter = 0U;
      vTaskDelay(1);
    }
#endif
  }
}

void task_monitor(void *pv_parameters) {
  auto *ctx = static_cast<RuntimeContext *>(pv_parameters);
  (void)ctx;

  for (;;) {
    UBaseType_t const queue_waiting = uxQueueMessagesWaiting(ctx->queue);
    UBaseType_t const queue_available = uxQueueSpacesAvailable(ctx->queue);
    ESP_LOGI(TAG, "mon: q_used=%u q_free=%u produced=%u processed=%u dropped=%u stack(acq/proc/mon)=%u/%u/%u heap8=%u",
             static_cast<unsigned>(queue_waiting), static_cast<unsigned>(queue_available),
             static_cast<unsigned>(g_produced_samples), static_cast<unsigned>(g_processed_samples),
             static_cast<unsigned>(g_dropped_samples), static_cast<unsigned>(uxTaskGetStackHighWaterMark(g_task_acq)),
             static_cast<unsigned>(uxTaskGetStackHighWaterMark(g_task_proc)),
             static_cast<unsigned>(uxTaskGetStackHighWaterMark(g_task_mon)),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_8BIT)));

    vTaskDelay(pdMS_TO_TICKS(DEBUG_MONITOR_PERIOD_MS));
  }
}
} // namespace

extern "C" void app_main(void) {
  esp_log_level_set(TAG, MAIN_LOG_LEVEL);

  ESP_LOGI(TAG, "Booting false.alarm production pipeline");
  ESP_LOGI(TAG, "Sampling=%d Hz, window=%u, history=%u", SAMPLING_RATE_HZ, kWindowSize, kHistorySamples);

#if (SIGNAL_SOURCE_KIND == 0) || (LOG_TO_SD_ENABLED == 1)
  SdCardService sd_service;
#endif

  std::unique_ptr<ISignalSource> signal_source;

#if SIGNAL_SOURCE_KIND == 0
  signal_source = std::make_unique<SdCsvSignalSource>(sd_service);
#elif SIGNAL_SOURCE_KIND == 1
  signal_source = std::make_unique<AnalogSignalSource>();
#elif SIGNAL_SOURCE_KIND == 2
  signal_source = std::make_unique<I2cSensorSignalSource>();
#else
#error "Invalid SIGNAL_SOURCE_KIND. Valid values: 0=SD CSV, 1=Analog ADC, 2=I2C Sensor"
#endif

  if (signal_source == nullptr) {
    ESP_LOGE(TAG, "Signal source factory returned null");
    return;
  }

#if (SIGNAL_SOURCE_KIND == 0) || (LOG_TO_SD_ENABLED == 1)
  {
    esp_err_t const mount_ret = sd_service.mount();
    if (mount_ret != ESP_OK) {
      ESP_LOGE(TAG, "Cannot continue without SD card (%s)", esp_err_to_name(mount_ret));
      return;
    }
  }
#endif

  esp_err_t const source_init_ret = signal_source->init();
  if (source_init_ret != ESP_OK) {
    ESP_LOGE(TAG, "Signal source init failed for %s (%s)", signal_source->name(), esp_err_to_name(source_init_ret));
    return;
  }

  QueueHandle_t const sample_queue =
      xQueueCreate(static_cast<UBaseType_t>(RING_BUFFER_CAPACITY_SAMPLES), sizeof(SignalPacket));
  if (sample_queue == nullptr) {
    ESP_LOGE(TAG, "Failed to create sample queue");
    return;
  }

  RuntimeContext runtime_ctx;
  runtime_ctx.queue = sample_queue;
  runtime_ctx.source = signal_source.get();
#if LOG_TO_SD_ENABLED
  runtime_ctx.sd_service = &sd_service;
#endif

  BaseType_t const acq_res = xTaskCreatePinnedToCore(task_acquire_signal, "AcquireSignal", TASK_ACQ_STACK_BYTES,
                                                     &runtime_ctx, TASK_ACQ_PRIORITY, &g_task_acq, TASK_ACQ_CORE);
  if (acq_res != pdPASS) {
    ESP_LOGE(TAG, "Failed to create acquisition task");
    return;
  }

  BaseType_t const proc_res = xTaskCreatePinnedToCore(task_process_signal, "ProcessSignal", TASK_PROC_STACK_BYTES,
                                                      &runtime_ctx, TASK_PROC_PRIORITY, &g_task_proc, TASK_PROC_CORE);
  if (proc_res != pdPASS) {
    ESP_LOGE(TAG, "Failed to create processing task");
    return;
  }

#if ENABLE_MONITOR_TASK
  BaseType_t const mon_res = xTaskCreatePinnedToCore(task_monitor, "MonitorRuntime", TASK_MON_STACK_BYTES, &runtime_ctx,
                                                     TASK_MON_PRIORITY, &g_task_mon, TASK_MON_CORE);
  if (mon_res != pdPASS) {
    ESP_LOGW(TAG, "Monitor task not created");
  }
#endif

  ESP_LOGI(TAG, "Pipeline started: acquisition(core=%d) processing(core=%d)", TASK_ACQ_CORE, TASK_PROC_CORE);

  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}

#endif
