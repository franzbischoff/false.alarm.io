#define TIMER_INTERRUPT_DEBUG 0
#define _TIMERINTERRUPT_LOGLEVEL_ 0

#define USE_TIMER_1 true

#include <Mpx.hpp>
#include <freertos/queue.h>
#include <SparkFun_Bio_Sensor_Hub_Library.hpp>
#include <TimerInterrupt_Generic.h>

//>> Matrix Profile settings are defined on compile time
#define WIN_SIZE WINDOW_SIZE
#define HIST_SIZE HISTORY_SIZE
#define BUFFER_SIZE (WINDOW_SIZE + WINDOW_SIZE) // must be at least 2x the window_size
#define QUEUE_SIZE (BUFFER_SIZE + 20)           // queue must have a little more room

#define S_SAMPLES SENSOR_SAMPLES
#define S_WIDTH SENSOR_WIDTH
#define SAMPLING_HZ SAMPLING_RATE_HZ

#ifndef SHORT_FILTER
#define SHORT_FILTER (SAMPLING_HZ / 10)
#endif
#ifndef WANDER_FILTER
#define WANDER_FILTER (SAMPLING_HZ / 2)
#endif

//>> interrupt variables
bool int_ready = true;     // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
bool int_mon_ready = true; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

//>> multitask configurations
enum { CORE_0 = 0, CORE_1 = 1 };
// define two tasks for Blink & AnalogRead
void task_compute(void *pv_parameters);
void task_read_signal(void *pv_parameters);
void mon_task(void *pv_parameters);
void idle_task(void *pv_parameters);
QueueHandle_t queue;      // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
bool buffer_init = false; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void cpu_task(void *parm) {
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

//>> Usual setup function
// the setup function runs once when you press reset or power the board
void setup() {

  uint16_t queue_size = QUEUE_SIZE;
  // initialize serial communication at 115200 bits per second:
  Serial.begin(115200);

  Serial.println("[Setup] Wait, don't use the sensor yet!!!");

  queue = xQueueCreate(queue_size, sizeof(float));

  if (queue == nullptr) {
    Serial.println("[Setup] Error creating the queue. Aborting.");
    ESP.restart();
  }
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(idle_task,  // Task function
                          "Idle CPU", // Just a name
                          2000, // This stack size in `word`s can be checked & adjusted by reading the Stack Highwater
                          nullptr, // Parameter passed as input of the task (can be NULL)
                          0, // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
                          nullptr, // Task Handle (can be NULL)
                          CORE_0);
  // xTaskCreatePinnedToCore(cpu_task,  // Task function
  //                         "CPU", // Just a name
  //                         2000, // This stack size in `word`s can be checked & adjusted by reading the Stack
  //                         Highwater nullptr, // Parameter passed as input of the task (can be NULL) 5, // Priority,
  //                         with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest. nullptr, //
  //                         Task Handle (can be NULL)
  // CORE_0);
  xTaskCreatePinnedToCore(mon_task,  // Task function
                          "Mon CPU", // Just a name
                          2000, // This stack size in `word`s can be checked & adjusted by reading the Stack Highwater
                          nullptr, // Parameter passed as input of the task (can be NULL)
                          10, // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
                          nullptr, // Task Handle (can be NULL)
                          CORE_1);
  xTaskCreatePinnedToCore(task_compute, // Task function
                          "Compute",    // Just a name
                          20000, // This stack size in `word`s can be checked & adjusted by reading the Stack Highwater
                          nullptr, // Parameter passed as input of the task (can be NULL)
                          2, // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
                          nullptr, // Task Handle (can be NULL)
                          CORE_0);

  xTaskCreatePinnedToCore(task_read_signal, // Task function
                          "ReadSignal",     // Just a name
                          20000,            // Stack size in `word`s
                          nullptr,          // Parameter passed as input of the task (can be NULL)
                          2, // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
                          nullptr, // Task Handle (can be NULL)
                          CORE_1);

  // Now the task scheduler, which takes over control of scheduling individual tasks, is automatically started.
}

//>> Usual loop function
void loop() {
  // Empty. Things are done in Tasks.
}

/*--------------------------------------------------*/
/*---------------------- Tasks ---------------------*/
/*--------------------------------------------------*/

bool IRAM_ATTR control_irq_mon(void *t) {
  (void)t;
  int_mon_ready = true;
  return true;
}

static uint32_t idle_cnt = 0;

void idle_task(void *pv_parameters) {
  (void)pv_parameters;
  for (;;) {
    vTaskDelay(0 / portTICK_RATE_MS);
    idle_cnt++;
  }
}

void mon_task(void *pv_parameters) {
  (void)pv_parameters;

  // const uint32_t timer_interval_mon = 1000000; // 4ms = 250Hz

  // ESP32Timer timer2(1);
  // timer2.attachInterruptInterval(timer_interval_mon, control_irq_mon);
  // int64_t now = esp_timer_get_time(); // time anchor
  for (;;) {

    // if (int_mon_ready) {

    // Note the trick of saving it on entry, so print time
    // is not added to our timing.

    uint32_t new_cnt = (uint32_t)idle_cnt; // Save the count for printing it ...
    printf("CPU usage: %.3f\n", (float)(1000 - new_cnt)/10.0F);
    printf("Available memory: %u/327680 bytes\n", ESP.getFreeHeap());
    printf("CPU frequency: %u MHz\n", ESP.getCpuFreqMHz());
    fflush(stdout);
    idle_cnt = 0; // Reset variable
    // } else {
    // int64_t now2 = esp_timer_get_time(); // time anchor

    vTaskDelay((portTICK_PERIOD_MS * 1000)); // 1073475208% 1073475208%

    // }
  }
}

void task_compute(void *pv_parameters) // This is a task.
{
  (void)pv_parameters;

  const uint16_t win_size = WIN_SIZE;
  const uint16_t hist_size = HIST_SIZE;
  const uint16_t floss_landmark = (HISTORY_SIZE - (SAMPLING_HZ * FLOSS_LANDMARK_S));
  float buffer[BUFFER_SIZE];
  float data = 0.0F;
  uint8_t recv_count = 0;

  float last_floss = 0;

  for (uint8_t i = 0; i < BUFFER_SIZE; i++) {
    buffer[i] = 0.0F;
  }

  MatrixProfile::Mpx mpx(win_size, 0.5F, 0, hist_size);
  mpx.floss_iac();
  mpx.prune_buffer();

  /* Print chip information */
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  printf("ESP32, %d CPU cores, WiFi%s%s, ", chip_info.cores, (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
         (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

  printf("silicon revision %d, ", chip_info.revision);

  printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
         (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

  for (;;) // A Task shall never return or exit.
  {
    if (buffer_init) { // wait for buffer to be initialized
      recv_count = 0;

      for (uint8_t i = 0; i < BUFFER_SIZE; i++) {
        if (xQueueReceive(queue, &data, 0)) {
          buffer[i] = data; // read data from queue
          recv_count = i + 1;
        } else {
          break; // no more data in queue
        }
      }

      if (recv_count > 0) {

        mpx.compute(buffer, recv_count); /////////////////
        mpx.floss();

        float *floss = mpx.get_floss();
        float *matrix = mpx.get_matrix();
        int16_t *indexes = mpx.get_indexes();

        // for (uint8_t i = 0; i < recv_count; i++) {
        //   printf("%.3f, %.3f, %.3f, %d\n", buffer[i], /*matrix[5000 - win_size - ez - recv_count + i],*/
        //          floss[floss_landmark + i], matrix[floss_landmark + i], indexes[floss_landmark + i]);
        // }
        // printf("[Consumer] %d\n", recv_count);
      }

    } else {
      // printf("-1, -1\n");
      vTaskDelay((portTICK_PERIOD_MS * 2)); // for stability
    }
    // mpx.floss();
    vTaskDelay((portTICK_PERIOD_MS * 2)); // for stability
  }
}

bool IRAM_ATTR control_irq(void *t) {
  (void)t;
  int_ready = true;
  return true;
}

void task_read_signal(void *pv_parameters) // This is a task.
{
  (void)pv_parameters;

  // Reset pin, MFIO pin
  const uint16_t res_pin = RESPIN;
  const uint16_t mfio_pin = MFIOPIN;
  // Possible widths: 69, 118, 215, 411us
  const uint16_t width = S_WIDTH;
  // Possible samples: 50, 100, 200, 400, 800, 1000, 1600, 3200 samples/second
  // Not every sample amount is possible with every width; check out our hookup
  // guide for more information.
  const uint16_t samples = S_SAMPLES;
  const uint32_t timer_interval = 1000000 / SAMPLING_HZ; // 4ms = 250Hz

  // short period filter
  const float s_window = SHORT_FILTER;
  const float eps_f = 0.05F;
  const float alpha = powf(eps_f, 1.0F / s_window);

  // large (wander) period filter
  const float l_window = WANDER_FILTER;
  const float l_alpha = powf(eps_f, 1.0F / l_window);

  ESP32Timer timer1(0);

  // Takes address, reset pin, and MFIO pin.
  SparkFun_Bio_Sensor_Hub bio_hub(res_pin, mfio_pin);
  bioData body;

  uint8_t initial_counter = 0;
  uint32_t ir_led = 0;
  bool sensor_started = false;

  float ir_res = 0.0F;
  float ir_sum = 0.0F;
  float ir_num = 0.0F;
  float ir_sum2 = 0.0F;
  float ir_num2 = 0.0F;

  Wire.begin();
  bio_hub.begin();

  bio_hub.configSensor();
  bio_hub.setPulseWidth(width);   // 18 bits resolution (0-262143)
  bio_hub.setSampleRate(samples); // 18 bits resolution (0-262143)

  timer1.attachInterruptInterval(timer_interval, control_irq);
  delay(1000); // Wait for sensor to stabilize

  for (;;) {
    if (int_ready) {
      body = bio_hub.readSensor(); // Read the sensor outside the IRQ, to avoid overload
      ir_led = body.irLed;

      if (!sensor_started) {
        if (ir_led > 1) {
          sensor_started = true;
          initial_counter = 0;
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
        ir_sum = ir_sum * alpha + ir_led;
        ir_num = ir_num * alpha + 1.0F;
        ir_sum2 = ir_sum2 * l_alpha + ir_led;
        ir_num2 = ir_num2 * l_alpha + 1.0F;
        ir_res = (ir_sum / ir_num - ir_sum2 / ir_num2);
        ir_res /= 20.0F;

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
      } else {
        // printf("[Producer] IR: %d\n", body.irLed);
        vTaskDelay((portTICK_PERIOD_MS * 1)); // for stability
      }
      int_ready = false;
    }
    vTaskDelay((portTICK_PERIOD_MS * 1)); // for stability
  }
}
