// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include <Mpx.hpp>

#ifndef NATIVE_PLATFORM
#include <freertos/queue.h>
#include <SparkFun_Bio_Sensor_Hub_Library.hpp>
#if defined(FILE_DATA)
#include <FS.h>
#include <LittleFS.h>

#define FORMAT_LITTLEFS_IF_FAILED false
#endif
#else
#include <fstream>
#include <iostream>
#include <string>
#endif

//>> Matrix Profile settings are defined on compile time
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

//>> multitask configurations
// define two tasks for Blink & AnalogRead
void task_compute(void *pv_parameters);
#ifndef NATIVE_PLATFORM
void task_read_signal(void *pv_parameters);
void mon_task(void *pv_parameters);
void idle_task(void *pv_parameters);

enum { CORE_0 = 0, CORE_1 = 1 };

QueueHandle_t queue; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

#else
float values[QUEUE_SIZE]; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
uint16_t recv_count = 0; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
uint32_t read_line = 0; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
MatrixProfile::Mpx mpx(WIN_SIZE, 0.5F, 0, HIST_SIZE); // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
#endif
bool buffer_init = false; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

#ifndef NATIVE_PLATFORM

//>> Usual setup function
// the setup function runs once when you press reset or power the board
// cppcheck-suppress unusedFunction
void setup() {

  uint16_t queue_size = QUEUE_SIZE;
  // initialize serial communication at 115200 bits per second:
  Serial.begin(115200);
  Serial.setDebugOutput(true);

#if defined(FILE_DATA)
  if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
    printf("LittleFS Mount Failed");
    return;
  }
#endif

  queue = xQueueCreate(queue_size, sizeof(float));

  if (queue == nullptr) {
    printf("[Setup] Error creating the queue. Aborting.");
    ESP.restart();
  }

#if defined(LOG_CPU_LOAD)
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(idle_task,  // Task function
                          "Idle CPU", // Just a name
                          1700, // This stack size in `word`s can be checked & adjusted by reading the Stack Highwater
                          nullptr, // Parameter passed as input of the task (can be NULL)
                          0, // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
                          nullptr, // Task Handle (can be NULL)
                          CORE_0);
  xTaskCreatePinnedToCore(mon_task,  // Task function
                          "Mon CPU", // Just a name
                          1800, // This stack size in `word`s can be checked & adjusted by reading the Stack Highwater
                          nullptr, // Parameter passed as input of the task (can be NULL)
                          10, // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
                          nullptr, // Task Handle (can be NULL)
                          CORE_1);
#endif

  xTaskCreatePinnedToCore(task_compute, // Task function
                          "Compute",    // Just a name
                          5000, // This stack size in `word`s can be checked & adjusted by reading the Stack Highwater
                          nullptr, // Parameter passed as input of the task (can be NULL)
                          4, // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
                          nullptr, // Task Handle (can be NULL)
                          CORE_0);

  xTaskCreatePinnedToCore(task_read_signal, // Task function
                          "ReadSignal",     // Just a name
                          5000,             // Stack size in `word`s
                          nullptr,          // Parameter passed as input of the task (can be NULL)
                          3, // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
                          nullptr, // Task Handle (can be NULL)
                          CORE_1);

  // Now the task scheduler, which takes over control of scheduling individual tasks, is automatically started.
}

//>> Usual loop function
// cppcheck-suppress unusedFunction
void loop() {
  // Empty. Things are done in Tasks.
}

#endif
/*--------------------------------------------------*/
/*---------------------- Tasks ---------------------*/
/*--------------------------------------------------*/

#if defined(LOG_CPU_LOAD)

uint32_t idle_cnt = 0; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void idle_task(void *pv_parameters) {
  (void)pv_parameters;
  for (;;) // -H776 A Task shall never return or exit.
  {
    vTaskDelay(portTICK_RATE_MS * 0);
    // printf("%u\n", uxTaskGetStackHighWaterMark(NULL)); // 17316
    idle_cnt++;
  }
}

void mon_task(void *pv_parameters) {
  (void)pv_parameters;
  TickType_t last_wake_time;

  for (;;) // -V776 A Task shall never return or exit.
  {
    vTaskDelayUntil(&last_wake_time, (portTICK_PERIOD_MS * 3000));
    uint32_t new_cnt = idle_cnt; // Save the count for printing it ...
    // printf("CPU frequency: %u MHz\n", ESP.getCpuFreqMHz());
    // printf("Available memory: %u/327680 bytes\n", ESP.getFreeHeap());
    printf("CPU usage: %.3f\n", (float)(3000 - new_cnt) / 30.0F);
    idle_cnt = 0;
    // printf("%u\n", uxTaskGetStackHighWaterMark(NULL)); // 17316
  }
}

#endif

void task_compute(void *pv_parameters) // This is a task.
{
  (void)pv_parameters;

  const uint16_t floss_landmark = FLOSS_LANDMARK;
  float buffer[BUFFER_SIZE];
#ifndef NATIVE_PLATFORM
  const uint16_t win_size = WIN_SIZE;
  const uint16_t hist_size = HIST_SIZE;
  uint16_t recv_count;
  int16_t delay_adjust = 100;
  float data = 0.0F;
#endif

  for (uint16_t i = 0; i < BUFFER_SIZE; i++) { // NOLINT(modernize-loop-convert)
    buffer[i] = 0.0F;
  }

#ifndef NATIVE_PLATFORM
  MatrixProfile::Mpx mpx(win_size, 0.5F, 0, hist_size);
  mpx.prune_buffer();

  for (;;) // -H776 A Task shall never return or exit.
  {
#endif
    if (buffer_init) { // wait for buffer to be initialized
#ifndef NATIVE_PLATFORM
      recv_count = 0;

      for (uint16_t i = 0; i < BUFFER_SIZE; i++) {
        if (xQueueReceive(queue, &data, 0)) {
          buffer[i] = data; // read data from queue
          recv_count = i + 1;
        } else {
          break; // no more data in queue
        }
      }
#else
    for (uint16_t i = 0; i < recv_count; i++) {
      buffer[i] = values[i];
    }
#endif

      if (recv_count > 0) {
        // printf("Compute %u\n", uxTaskGetStackHighWaterMark(NULL));

        mpx.compute(buffer, recv_count); /////////////////
        mpx.floss();

        // const uint16_t profile_len = mpx.get_profile_len();
        float *floss = mpx.get_floss();
        float *matrix = mpx.get_matrix();
        // int16_t *indexes = mpx.get_indexes();
        // float *vmmu = mpx.get_vmmu();
        // float *vsig = mpx.get_vsig();
        // float *ddf = mpx.get_ddf();
        // float *ddg = mpx.get_ddg();
        // const float last_mov2sum = mpx.get_last_mov2sum();
        // 3076.35815; 2856.01782; 1586.00159; 994.676636 516.238647 1314.97229 1708.46899 16.04523
        // 2318.72559; 1960.73865; 1300.25232; 661.98584; 443.209381; 823.406555; 938.791565
        // 826.757019; 300.426056; 153.044556; 492.913391; 1809.8042; 2651.82349; 3068.31763
        // 1791.32373; 1003.05811; 349.738037; 371.356781; 596.308411; 1272.40576; 1586.12268
        // 1672.79907; 915.265381; 495.762756; 94.2086945; 489.184814; 791.40918; 1512.28516
        // 1335.75464; 1062.51929; 414.154449; 437.533112; 741.733887; 1126.24634; 2045.8927;
        // 1766.2041; 1548.69617; 551.398743; 309.887634; 445.998322; 652.136719 578.875549
        // 769.482056; 556.67865 903.001709 950.161377 1266.453 1248.04871 1026.78845 741.639587
        // 451.705811; 324.240173 565.615051 767.656128 982.322754 694.380493 462.135345
        // 107.598633 95.2946625 15.2094345 67.5365067, 75.9099045, 819.320435, 1036.26453,
        // 1034.60144, 652.909058, 988.230103, 3970.94995, 4499.20459, 4784.39648, 2391.40015,
        // 8817.62793,  71558.25, 406204.906, 613322.062, 1951099.38, 2475905.5, 2333115.5, 1855079.12
        // 4162497.25, 6179072.5, 6823374.5, 4492077, 1577478.62, 562556.062, 240837.156, 12876.641, 60801.793
        // 52322.2969 42445.2539 35416.9648 136693.031 330022.5 2876578.25 12545874 15420299 15186808
        // 5865206 1647932 1491935.62 1527195.88 620016.5 296991.5 233162.656 243589.984 17006.609
        // 2550801.5 6002749.5 6333334.5 6078726.5 5050615 6023107 8588083 17385128 20681866 21138936 1717056
        // 127312749 11454504 4863423 3195388.5 2994941.75 3035440 2031340.88 385834.75 4011829 407499.5
        // 3293068.75 703413.438 1693812.12 2352266.5 327993.5 2897976.75 1626859.75 815760.125 456107.094
        // 672710.688 1232217.5 18466747.38 1533687.62 1287871.38 401132.719 425417.281 466348.5

        // std::ofstream file_out;

        // if (read_line > 8800 && read_line < 8900) {
        //   file_out.open("dev/last_out.csv", std::fstream::app);

        //   if (!file_out.is_open()) {
        //     std::cout << "Unable to open file_out" << std::endl;
        //   } else {
        //     for (uint16_t i = 0; i < profile_len; i++) {
        //       const std::string line =
        //           std::to_string(read_line) + "," + std::to_string(floss[i]) + "," + std::to_string(matrix[i]) + ","
        //           + std::to_string(indexes[i]) + "," + std::to_string(vmmu[i]) + "," + std::to_string(vsig[i]) + ","
        //           + std::to_string(ddf[i]) + "," + std::to_string(ddg[i]) + "," + std::to_string(last_mov2sum) +
        //           "\n";
        //       file_out.write(line.c_str(), line.length());
        //     }
        //     file_out.close();
        //   }
        // }

        for (uint16_t i = 0; i < recv_count; i++) {
          printf("%.1f %.2f %.2f\n", buffer[i], floss[floss_landmark + i],
                 matrix[floss_landmark + i]); // indexes[floss_landmark + i]);
        }
        // printf("[Consumer] %d\n", recv_count); // handle about 400 samples per second
#ifndef NATIVE_PLATFORM
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
#endif
      }
#ifndef NATIVE_PLATFORM
    } else {
      // printf("-1, -1\n");
      vTaskDelay((portTICK_PERIOD_MS * 2)); // for stability
#endif
    }

#ifndef NATIVE_PLATFORM
    vTaskDelay((portTICK_PERIOD_MS * delay_adjust)); // for stability
  }
#endif
}

#ifndef NATIVE_PLATFORM
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
  SparkFun_Bio_Sensor_Hub bio_hub(res_pin, mfio_pin);
  bioData body;

  uint32_t ir_led;
  bool sensor_started = false;

  float ir_sum = 0.0F;
  float ir_num = 0.0F;
  float ir_sum2 = 0.0F;
  float ir_num2 = 0.0F;

  Wire.begin();
  bio_hub.begin();

  bio_hub.configSensor();
  bio_hub.setPulseWidth(width);   // 18 bits resolution (0-262143)
  bio_hub.setSampleRate(samples); // 18 bits resolution (0-262143)
#else
  File file = LittleFS.open("/floss.csv"); // NOLINT(misc-const-correctness) - this variable can't be const
  if (!file || file.isDirectory()) {
    printf("XXX failed to open file for reading\n.");
    return;
  }
#endif

  vTaskDelay((portTICK_PERIOD_MS * 1000)); // Wait for sensor to stabilize

  last_wake_time = xTaskGetTickCount();

  for (;;) // -H776 A Task shall never return or exit.
  {
    vTaskDelayUntil(&last_wake_time, (portTICK_PERIOD_MS * timer_interval)); // for stability

#ifndef FILE_DATA
    body = bio_hub.readSensor(); // Read the sensor outside the IRQ, to avoid overload
    ir_led = body.irLed;

    if (!sensor_started) {
      if (ir_led > 1) {
        sensor_started = true;
        initial_counter = 0;
        /* Print chip information */
        esp_chip_info_t chip_info;
        esp_chip_info(&chip_info);
        printf("[Producer] ESP32%s rev %d, %d CPU cores, WiFi%s%s%s, ",
               (chip_info.model & CHIP_ESP32S2) ? "-S2" : "",
               chip_info.revision,
                chip_info.cores,
               (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
               (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "",
               (chip_info.features & CHIP_FEATURE_IEEE802154) ? "/802.15.4" : "");

        printf("%uMB %s flash", spi_flash_get_chip_size() / (uint32_t)(1024 * 1024),
               (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

        if(psramFound()) {
          printf(", %uMB %s PSRAM\n", esp_spiram_get_size() / (uint32_t)(1024 * 1024),
               (chip_info.features & CHIP_FEATURE_EMB_PSRAM) ? "embedded" : "external");
        } else {
          printf(", No PSRAM found\n");
        }

        printf("[Producer] Sensor started, now it can be used.\n");
      } else {
        initial_counter++;
        if (initial_counter > 2500) {
          printf("[Producer] Sensor not properly started, rebooting...\n");
          ESP.restart();
        }
      }
    }

    // 70744 // t 318384 > 363368 // 277,849/1,310,720 ;; 16,976/327,680
    // getMaxAllocHeap: 69620

    if (ir_led > 10000) {
      // printf("Read %u\n", uxTaskGetStackHighWaterMark(NULL)); // 176

      ir_sum = ir_sum * alpha + (float)ir_led;
      ir_num = ir_num * alpha + 1.0F;
      ir_sum2 = ir_sum2 * l_alpha + (float)ir_led;
      ir_num2 = ir_num2 * l_alpha + 1.0F;
      ir_res = (ir_sum / ir_num - ir_sum2 / ir_num2);
      ir_res /= 20.0F;
#else
    if (file.available()) {
      ir_res = file.parseFloat();
    } else {
      ir_res = 0.0F;
      file.close();
    }
#endif

      if (ir_res > 50.0F || ir_res < -50.0F) {
        continue;
      }

      if (xQueueSend(queue, &ir_res, (portTICK_PERIOD_MS * 200))) { // SUCCESS
        if (!buffer_init) {
          if (++initial_counter >= BUFFER_SIZE) {
            buffer_init = true; // this is read by the receiver task
            printf("[Producer] DEBUG: Buffer filled, starting to compute\n");
          }
        }
      } else {
        printf("[Producer] Error sending data to the queue\n");
      }
#ifndef FILE_DATA
    } else {
      // printf("[Producer] IR: %d\n", body.irLed);
      vTaskDelay((portTICK_PERIOD_MS * 1)); // for stability
    }
#endif
  }
}
#endif

#if defined(NATIVE_PLATFORM)
int main() {
  std::ifstream file;
  file.open("data/floss.csv", std::ios::in);

  if (!file.is_open()) {
    std::cout << "Unable to open file";
    return 1;
  }

  uint16_t k = 0;
  for (std::string line; std::getline(file, line);) // read stream line by line
  {
    read_line++;
    float const v = std::stof(line);

    if(v > 50.0F || v < -50.0F) {
      continue;
    }

    values[k++] = v;

    if (!buffer_init) {
      if (k >= ((BUFFER_SIZE)-1)) {
        buffer_init = true;
        recv_count = k;
        k = 0;
        task_compute(nullptr);
      }
    } else {
      if (k >= 30) {
        recv_count = k;
        k = 0;
        task_compute(nullptr);
      }
    }

    if (read_line > 8900) {
      break;
    }

    // std::cout << values[0] << std::endl;
  }
  file.close();
}
#endif
