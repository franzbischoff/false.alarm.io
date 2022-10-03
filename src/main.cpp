#define TIMER_INTERRUPT_DEBUG 0
#define _TIMERINTERRUPT_LOGLEVEL_ 0

#define USE_TIMER_1 true

#include <Mpx.hpp>
#include <Pipe.h>
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

float buffer[200];
float data[200];
uint8_t write_index = 0;
uint8_t read_index = 0;
bool buffer_ok = false;
bool new_data = false;

// define two tasks for Blink & AnalogRead
void TaskCompute(void *pvParameters);
void TaskReadSignal(void *pvParameters);

QueueHandle_t queue;
int queueSize = 10;

// the setup function runs once when you press reset or power the board
void setup() {

  queue = xQueueCreate(queueSize, sizeof(int));

  if (queue == NULL) {
    Serial.println("Error creating the queue");
  }

  // initialize serial communication at 115200 bits per second:
  Serial.begin(115200);

  for (int i = 0; i < 200; i++) {
    buffer[i] = 0;
  }

  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(TaskCompute, // Task function
                          "Compute",  // Just a name
                          10000,     // This stack size in `word`s can be checked & adjusted by reading the Stack Highwater
                          NULL,      // Parameter passed as input of the task (can be NULL)
                          2, // Priority, with 3 (config MAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
                          NULL, // Task Handle (can be NULL)
                          CORE_0);

  xTaskCreatePinnedToCore(TaskReadSignal, // Task function
                          "ReadSignal", // Just a name
                          10000,        // Stack size in `word`s
                          NULL,         // Parameter passed as input of the task (can be NULL)
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
  bool init = false;
  uint8_t size = 200;

  for (;;) // A Task shall never return or exit.
  {
    if (init) {
      if (!ready && new_data) {
        for (size = 0; size < 200; size++) {
          data[size] = buffer[read_index++];
          if (read_index >= 200) {
            read_index = 0;
          }
          if (read_index == write_index) {
            break;
          }
        }
        mpx.compute(data, size);
        for (int i = 0; i < size; i++) {
          Serial.println(size);
        }
      }
    } else if (buffer_ok) {
      mpx.compute(buffer, size);
      read_index = 0;
      init = true;
    }

    // mpx.floss();
    vTaskDelay(1); // one tick delay (15ms) in between reads for stability
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

  bioHub.configSensorBpm(MODE_ONE);
  bioHub.setPulseWidth(WIDTH);   // 18 bits resolution (0-262143)
  bioHub.setSampleRate(SAMPLES); // 18 bits resolution (0-262143)

  ITimer1.attachInterruptInterval(TIMER_INTERVAL, control_irq);
  delay(4000); // Wait for sensor to stabilize

  for (;;) {
    body = bioHub.readSensorBpm(); // Read the sensor outside the IRQ, to avoid overload
    if (ready) {
      float irLed = (float)body.irLed;
      new_data = false;

      if (irLed > 20000.0F) {
        irSum = irSum * ALPHA + irLed;
        irNum = irNum * ALPHA + 1.0F;
        irSum2 = irSum2 * ALPHAL + irLed;
        irNum2 = irNum2 * ALPHAL + 1.0F;
        irRes = (int16_t)(10.0F * (irSum / irNum - irSum2 / irNum2));
        if (irRes > 2047 || irRes < -2048) {
          irRes = -3000;
        } else {
          if (!buffer_ok) {
            buffer[write_index] = (float)irRes / 100.0F;
            write_index++;
            if (write_index == 200) {
              buffer_ok = true;
              new_data = true;
              write_index = 0;
            }
          } else {
            buffer[write_index] = (float)irRes / 100.0F;
            write_index++;
            if (write_index == 200) {
              write_index = 0;
            }
            new_data = true;
          }
        }
      }
      ready = false;
    }
    vTaskDelay(1); // one tick delay (15ms) in between reads for stability
  }
}
