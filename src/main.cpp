#define TIMER_INTERRUPT_DEBUG 0
#define _TIMERINTERRUPT_LOGLEVEL_ 0

#define USE_TIMER_1 true

#define BUFFER_SIZE 160 // must be at least 2x the window_size

#include <Mpx.hpp>
#include <freertos/queue.h>
#include <SparkFun_Bio_Sensor_Hub_Library.hpp>
#include <TimerInterrupt_Generic.h>

bool int_ready = true; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

enum { CORE_0 = 0, CORE_1 = 1 };

// define two tasks for Blink & AnalogRead
void task_compute(void *pv_parameters);
void task_read_signal(void *pv_parameters);

QueueHandle_t queue;      // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
bool buffer_init = false; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// the setup function runs once when you press reset or power the board
void setup() {

  uint16_t queue_size = BUFFER_SIZE + 20;
  // initialize serial communication at 115200 bits per second:
  Serial.begin(115200);

  queue = xQueueCreate(queue_size, sizeof(float));

  if (queue == nullptr) {
    Serial.println("Error creating the queue");
  }

  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(task_compute, // Task function
                          "Compute",    // Just a name
                          20000, // This stack size in `word`s can be checked & adjusted by reading the Stack Highwater
                          nullptr, // Parameter passed as input of the task (can be NULL)
                          2, // Priority, with 3 (config MAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
                          nullptr, // Task Handle (can be NULL)
                          CORE_0);

  xTaskCreatePinnedToCore(task_read_signal, // Task function
                          "ReadSignal",     // Just a name
                          20000,            // Stack size in `word`s
                          nullptr,          // Parameter passed as input of the task (can be NULL)
                          2, // Priority, with 3 (config MAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
                          nullptr, // Task Handle (can be NULL)
                          CORE_1);

  // Now the task scheduler, which takes over control of scheduling individual tasks, is automatically started.
}

void loop() {
  // Empty. Things are done in Tasks.
}

/*--------------------------------------------------*/
/*---------------------- Tasks ---------------------*/
/*--------------------------------------------------*/

void task_compute(void *pv_parameters) // This is a task.
{
  (void)pv_parameters;

  // const uint16_t win_size = 75;
  // const uint16_t ez = 38;
  const uint16_t win_size = 75;
  const uint16_t ez = 38;
  float buffer[BUFFER_SIZE];
  float data = 0.0F;
  uint8_t recv_count = 0;

  for (uint8_t i = 0; i < BUFFER_SIZE; i++) {
    buffer[i] = 0.0F;
  }

  MatrixProfile::Mpx mpx(win_size, 0.5F, 0, 5000);

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
        const float *matrix = mpx.get_matrix();
        const float *floss = mpx.get_floss();

        for (uint8_t i = 0; i < recv_count; i++) {
          printf("%.3f, %.3f\n", buffer[i], /*matrix[5000 - win_size - ez - recv_count + i],*/
                 floss[3750]/*, recv_count*/);
        }
        // for (uint8_t i = 0; i < 50; i++) {
        //   printf("%.3f\n", matrix[5000 - win_size - 100 + i]);
        // }
        // printf("-----\n");
      }
    } else {
      // printf("-1, -1\n");
      vTaskDelay((portTICK_PERIOD_MS * 2)); // for stability
    }
    // mpx.floss();
    vTaskDelay((portTICK_PERIOD_MS * 1)); // for stability
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
  const uint16_t width = 411;
  // Possible samples: 50, 100, 200, 400, 800, 1000, 1600, 3200 samples/second
  // Not every sample amount is possible with every width; check out our hookup
  // guide for more information.
  const uint16_t samples = 400;
  const uint32_t timer_interval = 4000; // 4ms = 250Hz

  // short period filter
  const float s_window = 25.0F;
  const float eps_f = 0.05F;
  const float alpha = powf(eps_f, 1.0F / s_window);

  // large (wander) period filter
  const float l_window = 125.0F;
  const float l_alpha = powf(eps_f, 1.0F / l_window);

  ESP32Timer timer1(0);

  // Takes address, reset pin, and MFIO pin.
  SparkFun_Bio_Sensor_Hub bio_hub(res_pin, mfio_pin);
  bioData body;

  uint8_t initial_counter = 0;
  uint32_t ir_led = 0;

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
              printf("[Producer] Start\n");
            }
          }
        } else {
          printf("[Producer] Error sending to the queue\n");
        }
      } else {
        printf("[Producer] IR: %d\n", body.irLed);
        vTaskDelay((portTICK_PERIOD_MS * 1)); // for stability
      }
      int_ready = false;
    }
    vTaskDelay((portTICK_PERIOD_MS * 1)); // for stability
  }
}
