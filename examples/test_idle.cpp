// #define TIMER_INTERRUPT_DEBUG 0
// #define _TIMERINTERRUPT_LOGLEVEL_ 0

// #define USE_TIMER_1 true

#include <Arduino.h>
// #include <freertos/queue.h>

/*--------------------------------------------------*/
/*---------------------- Tasks ---------------------*/
/*--------------------------------------------------*/

static int64_t idle_cnt = 0;

static void idle_task(void *parm) {
  while (1 == 1) {
    int64_t now = esp_timer_get_time(); // time anchor
    vTaskDelay(0 / portTICK_RATE_MS);
    int64_t now2 = esp_timer_get_time();
    idle_cnt += (now2 - now); // diff
  }
}

static void mon_task(void *parm) {
  while (1 == 1) {
    // Note the trick of saving it on entry, so print time
    // is not added to our timing.

    int64_t new_cnt = idle_cnt; // Save the count for printing it ...

    // Compensate for the 100 ms delay artifact: 900 ms = 100%
    float cpu_percent = (float)(new_cnt); // - 999.0F;//((99.9F / 90.0F) * (float)(new_cnt)) / 10.0F;
    cpu_percent = 100.0F - (cpu_percent / 9990.0F);
    if(cpu_percent < 0.0F) cpu_percent = -0.001F;
    printf("%.3f ", cpu_percent);       // - 999000) / 10.0F); // 100 - cpu_percent);
    fflush(stdout);
    idle_cnt = 0;                            // Reset variable
    vTaskDelay((portTICK_PERIOD_MS * 1000)); // 1073475208% 1073475208%
  }
}

static void cpu_task(void *parm) {
  int cnt = 0;
  while (true) {
    // Every 10 seconds, we put the processor to work.
    if (++cnt % 100 == 0) {
      int cnt_dummy = 0;

      printf(" work [[ ");
      fflush(stdout);

      // Make sure the watchdog is not triggered ...
      for (int aa = 0; aa < 30000000; aa++) {
        for (int bb = 0; bb < 3; bb++) {
          cnt_dummy += 22;
        }
      }

      printf(" ]] rest ");
      fflush(stdout);
    }
    vTaskDelay((portTICK_PERIOD_MS * 100));
  }
}

void setup() {
  Serial.begin(115200);
  delay(5000);
  /* Print chip information */
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  printf("ESP32, %d CPU cores, WiFi%s%s, ", chip_info.cores, (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
         (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

  printf("silicon revision %d, ", chip_info.revision);

  printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
         (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

  xTaskCreatePinnedToCore(idle_task, "idle_task", 1024 * 2, NULL, 0, NULL, 0);
  xTaskCreatePinnedToCore(mon_task, "mon_task", 1024 * 2, NULL, 10, NULL, 1);
  xTaskCreatePinnedToCore(cpu_task, "cpu_task", 1024 * 2, NULL, 9, NULL, 0);

  printf("CPU Load Demo.\n");
  while (true) {
    // Every 10 seconds, we put the processor to work.
    vTaskDelay(1000 / portTICK_RATE_MS);
  }
}

//>> Usual loop function
void loop() {
  // Empty. Things are done in Tasks.
}
