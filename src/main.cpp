#define TIMER_INTERRUPT_DEBUG 0
#define _TIMERINTERRUPT_LOGLEVEL_ 0

#define USE_TIMER_1 true

#include <Mpx.hpp>
#include <freertos/queue.h>
#include <SparkFun_Bio_Sensor_Hub_Library.hpp>
#include <TimerInterrupt_Generic.h>

bool ready = true;
// Reset pin, MFIO pin
const uint16_t RES_PIN = RESPIN;
const uint16_t MFIO_PIN = MFIOPIN;
// Possible widths: 69, 118, 215, 411us
const uint16_t WIDTH = 411;
// Possible samples: 50, 100, 200, 400, 800, 1000, 1600, 3200 samples/second
// Not every sample amount is possible with every width; check out our hookup
// guide for more information.
const uint16_t SAMPLES = 400;

const uint32_t TIMER_INTERVAL = 4000; // 4ms = 250Hz

#define CORE_0 0
#define CORE_1 1

enum { WIN_SIZE = 75 };

int16_t irRes = -3000;

// define two tasks for Blink & AnalogRead
void TaskCompute(void *pvParameters);
void TaskReadSignal(void *pvParameters);

QueueHandle_t queue;
uint8_t queue_size = 255;
bool buffer_init = false;
float buffer[200];

// the setup function runs once when you press reset or power the board
void setup() {

  // initialize serial communication at 115200 bits per second:
  Serial.begin(115200);

  queue = xQueueCreate(queue_size, sizeof(float));

  if (queue == NULL) {
    Serial.println("Error creating the queue");
  }

  for (uint8_t i = 0; i < 200; i++) {
    buffer[i] = 0.0F;
  }

  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskCompute, // Task function
                          "Compute",   // Just a name
                          20000, // This stack size in `word`s can be checked & adjusted by reading the Stack Highwater
                          NULL,  // Parameter passed as input of the task (can be NULL)
                          2, // Priority, with 3 (config MAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
                          NULL, // Task Handle (can be NULL)
                          CORE_0);

  xTaskCreatePinnedToCore(TaskReadSignal, // Task function
                          "ReadSignal",   // Just a name
                          20000,          // Stack size in `word`s
                          NULL,           // Parameter passed as input of the task (can be NULL)
                          2, // Priority, with 3 (config MAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
                          NULL, // Task Handle (can be NULL)
                          CORE_1);

  // Now the task scheduler, which takes over control of scheduling individual tasks, is automatically started.
}

void loop() {
  // Empty. Things are done in Tasks.
}

/*--------------------------------------------------*/
/*---------------------- Tasks ---------------------*/
/*--------------------------------------------------*/

void TaskCompute(void *pvParameters) // This is a task.
{
  (void)pvParameters;

  static MatrixProfile::Mpx mpx(WIN_SIZE, 0.5, 0, 5000);
  uint8_t size = 0;

  for (;;) // A Task shall never return or exit.
  {
    if (buffer_init) {
      float data = 0.0F;
      size = 0;

      for (uint8_t i = 0; i < 200; i++) {
        if (xQueueReceive(queue, &data, 0)) {
          buffer[i] = data;
          size = i + 1;
        } else {
          break;
        }
      }

      if (size > 0) {
        mpx.compute(buffer, size); /////////////////
        float *matrix = mpx.get_matrix();
        for (uint8_t i = 0; i < size; i++) {
          printf("%.3f, %.3f\n", buffer[i], matrix[5000 - WIN_SIZE * 3 - size + i]);
        }
      }
    } else {
      printf("-1, -1\n");
      vTaskDelay((portTICK_PERIOD_MS * 2));
    }
    // mpx.floss();
    vTaskDelay((portTICK_PERIOD_MS * 1)); // one tick delay (15ms) in between reads for stability
  }
}

bool IRAM_ATTR control_irq(void *t) {
  (void)t;
  ready = true;
  return true;
}

void TaskReadSignal(void *pvParameters) // This is a task.
{
  (void)pvParameters;

  ESP32Timer ITimer1(0);

  // Takes address, reset pin, and MFIO pin.
  SparkFun_Bio_Sensor_Hub bioHub(RES_PIN, MFIO_PIN);
  bioData body;

  // short period filter
  const float F_WINDOW = 25.0;
  const float EPSF = 0.05;
  const float ALPHA = powf(EPSF, 1.0F / F_WINDOW);

  // large (wander) period filter
  const float F_WINDOW2 = 125.0;
  const float ALPHAL = powf(EPSF, 1.0F / F_WINDOW2);

  float irSum = 0.0;
  float irNum = 0.0;
  float irSum2 = 0.0;
  float irNum2 = 0.0;

  Wire.begin();
  bioHub.begin();

  bioHub.configSensor();
  bioHub.setPulseWidth(WIDTH);   // 18 bits resolution (0-262143)
  bioHub.setSampleRate(SAMPLES); // 18 bits resolution (0-262143)

  ITimer1.attachInterruptInterval(TIMER_INTERVAL, control_irq);
  delay(1000); // Wait for sensor to stabilize

  uint8_t initial_counter = 0;
  float irLed = 0.0F;

  for (;;) {
    body = bioHub.readSensor(); // Read the sensor outside the IRQ, to avoid overload
    if (ready) {
      irLed = (float)body.irLed;

      if (irLed > 10000.0F) {
        irSum = irSum * ALPHA + irLed;
        irNum = irNum * ALPHA + 1.0F;
        irSum2 = irSum2 * ALPHAL + irLed;
        irNum2 = irNum2 * ALPHAL + 1.0F;
        irRes = (int16_t)(10.0F * (irSum / irNum - irSum2 / irNum2));
        // if (irRes > 2047 || irRes < -2048) {
        //   irRes = -3000;
        // } else {
          irLed = (float)irRes / 200.0F;

          if (xQueueSend(queue, &irLed, (portTICK_PERIOD_MS * 200))) { // SUCCESS
            if (!buffer_init) {
              if (++initial_counter >= 200) {
                buffer_init = true; // this is read by the receiver task
                printf("Start\n");
              }
            }
          } else {
            printf("Error sending to the queue\n");
          }
        // }
      } else {
        printf("IR: %d\n", body.irLed);
        vTaskDelay((portTICK_PERIOD_MS * 1));
      }
      ready = false;
    }
    vTaskDelay((portTICK_PERIOD_MS * 1)); // one tick delay (15ms) in between reads for stability
  }
}
