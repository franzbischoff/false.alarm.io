// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#if defined(RASPBERRYPI)
#include <Mpx.hpp>
#include <stdio.h>
// #include <wiringPi.h>
#elif defined(ARDUINO_ESP32_DEV) || defined(ARDUINO_RASPBERRY_PI_PICO)
// #include <Arduino.h>
#include <Mpx.hpp>
#include <chrono>
// #include <cstdlib>
#include <random>
// #include "avr8-stub.h"
// #include "app_api.h" // only needed with flash breakpoints
// #include <unity.h>
#else
#include <Mpx.hpp>
#include <chrono>
#include <iostream>
#include <cstdlib>
#include <cmath>
#include <random>
#include <ctime>
#endif
// // Type your code here, or load an example.
// int main(void) {
// #define LO -5
// #define HI 5

// float rand_num;
// srand (static_cast <unsigned> (time(0)));
// for(int i = 0; i < 20; i++) {

//   rand_num = LO + static_cast<float>(rand()) /
//                             (static_cast<float>(RAND_MAX / (HI - LO)));

// 	printf("%0.2f\n", rand_num);
// }
//   return rand_num;
// }

enum { WIN_SIZE = 75 };

float get_random(float min_val, float max_val, float div_val, std::default_random_engine &generator) {
  std::uniform_real_distribution<float> distribution(min_val, max_val);
  float const rand_num = distribution(generator) / div_val;
  return rand_num;
}

#if defined(RASPBERRYPI)
int done = 0;

void setup() {
  // debug_init();
  Serial.begin(115200);
}

void loop() {

  if (done) {
    return;
  }

  MPX.compute_stream();
  float *res = MPX.get_matrix();

  for (uint16_t i = 0; i < DATA_SIZE - WIN_SIZE + 1; i++) {
    Serial.printf("%.2f, ", (float)res[i]);
  }
  Serial.println();

  int16_t *idxs = MPX.get_indexes();

  for (uint16_t i = 0; i < DATA_SIZE - WIN_SIZE + 1; i++) {
    Serial.printf("%d, ", (int16_t)idxs[i]);
  }
  Serial.println();

  done = 1;
}

int main(int argc, char **argv) {
  MPX.compute_stream();

  return 0;
}
#elif defined(ARDUINO_ESP32_DEV) || defined(ARDUINO_RASPBERRY_PI_PICO)

// pio remote run -e esp32 -t upload
// pio remote device monitor -b 115200

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  // analogWriteFreq(1);
  // analogReadResolution(2);
}

void loop() {

  static int done = 0;
  static MatrixProfile::Mpx mpx(WIN_SIZE, 0.5, 0, 5000);

  digitalWrite(LED_BUILTIN, HIGH); // turn the LED on (HIGH is the voltage level)
  delay(5000);                     // wait for a second
  digitalWrite(LED_BUILTIN, LOW);  // turn the LED off by making the voltage LOW
  delay(1000);

  if (done == 0) {

    auto start = std::chrono::system_clock::now();

    float inbuf[1000];

    std::default_random_engine generator(2002);

    for (float &i : inbuf) {
      i = get_random(-100000.0F, 100000.0F, 20000.0F, generator);
    }

    mpx.compute(inbuf, 1000);

    for (float &i : inbuf) {
      i = get_random(-100000.0F, 100000.0F, 20000.0F, generator);
    }

    mpx.compute(inbuf, 1000);

    for (float &i : inbuf) {
      i = get_random(-100000.0F, 100000.0F, 20000.0F, generator);
    }

    mpx.compute(inbuf, 1000);

    for (float &i : inbuf) {
      i = get_random(-100000.0F, 100000.0F, 20000.0F, generator);
    }

    mpx.compute(inbuf, 1000);

    for (float &i : inbuf) {
      i = get_random(-100000.0F, 100000.0F, 20000.0F, generator);
    }

    mpx.compute(inbuf, 1000);

    for (float &i : inbuf) {
      i = get_random(-100000.0F, 100000.0F, 20000.0F, generator);
    }

    mpx.compute(inbuf, 1000);

    for (float &i : inbuf) {
      i = get_random(-100000.0F, 100000.0F, 20000.0F, generator);
    }

    mpx.compute(inbuf, 1000);

    for (float &i : inbuf) {
      i = get_random(-100000.0F, 100000.0F, 20000.0F, generator);
    }

    mpx.compute(inbuf, 1000);

    auto end = std::chrono::system_clock::now();

    std::chrono::duration<float> diff2 = end - start;

    Serial.printf("%.2f seconds to compute\n", diff2.count());

    start = std::chrono::system_clock::now();

    mpx.floss();

    end = std::chrono::system_clock::now();

    diff2 = end - start;

    Serial.printf("%.2f seconds to compute floss\n", diff2.count());

    digitalWrite(LED_BUILTIN, HIGH); // turn the LED on (HIGH is the voltage level)
    delay(1000);                     // wait for a second
    digitalWrite(LED_BUILTIN, LOW);  // turn the LED off by making the voltage LOW

    start = std::chrono::system_clock::now();

    float *res = mpx.get_floss();
    float sum = 0;

    for (uint16_t i = 0; i < 5000 - WIN_SIZE + 1; i++) {
      sum += res[i]; // < 0 ? 0 : res[i];
    }

    end = std::chrono::system_clock::now();

    diff2 = end - start;

    Serial.printf("%.2f seconds to sum up\n", diff2.count());

    digitalWrite(LED_BUILTIN, HIGH); // turn the LED on (HIGH is the voltage level)
    delay(1000);                     // wait for a second
    digitalWrite(LED_BUILTIN, LOW);  // turn the LED off by making the voltage LOW

    Serial.printf("MP sum is %.7f\n", sum);
    Serial.println();
    done = 1;
  } else {
    float inbuf[1000];

    std::default_random_engine generator(2002);

    for (float &i : inbuf) {
      i = get_random(-100000.0F, 100000.0F, 20000.0F, generator);
    }

    mpx.compute(inbuf, 1000);

    auto start = std::chrono::system_clock::now();

    mpx.floss();

    auto end = std::chrono::system_clock::now();

    std::chrono::duration<float> diff2 = end - start;

    Serial.printf("%.2f seconds to compute floss\n", diff2.count());
  }
}
#else
int main(int argc, char **argv) {

  // auto start = std::chrono::system_clock::now();

  std::cout << "Tick" << std::endl;

  static MatrixProfile::Mpx mpx(WIN_SIZE, 0.5, 0, 5000);

  // for (uint16_t i = 0; i < 200; i++) {
  // mpx.initial_data(TEST_DATA, 2000); // 2: 1799.1505127; 3: 2799.3811035; 4: 3800.0820312; 5: 4799.7426758
  // mpx.compute(TEST_DATA, 2000); //              2: 93599.4921875; 3: 142240.1250000; 4: 190884.9218750; 5:
  // 239518.7656250
  float inbuf[1000];

  std::default_random_engine generator(2002);

  for (float &i : inbuf) {
    i = get_random(-100000.0F, 100000.0F, 20000.0F, generator);
  }

  mpx.compute(inbuf, 1000);

  for (float &i : inbuf) {
    i = get_random(-100000.0F, 100000.0F, 20000.0F, generator);
  }

  mpx.compute(inbuf, 1000);

  for (float &i : inbuf) {
    i = get_random(-100000.0F, 100000.0F, 20000.0F, generator);
  }

  mpx.compute(inbuf, 1000);

  for (float &i : inbuf) {
    i = get_random(-100000.0F, 100000.0F, 20000.0F, generator);
  }

  mpx.compute(inbuf, 1000);

  for (float &i : inbuf) {
    i = get_random(-100000.0F, 100000.0F, 20000.0F, generator);
  }

  mpx.compute(inbuf, 1000);

  for (float &i : inbuf) {
    i = get_random(-100000.0F, 100000.0F, 20000.0F, generator);
  }

  mpx.compute(inbuf, 1000);

  for (float &i : inbuf) {
    i = get_random(-100000.0F, 100000.0F, 20000.0F, generator);
  }

  mpx.compute(inbuf, 1000);

  for (float &i : inbuf) {
    i = get_random(-100000.0F, 100000.0F, 20000.0F, generator);
  }

  mpx.compute(inbuf, 1000);

  for (float &i : inbuf) {
    i = get_random(-100000.0F, 100000.0F, 20000.0F, generator);
  }

  mpx.compute(inbuf, 1000);

  for (float &i : inbuf) {
    i = get_random(-100000.0F, 100000.0F, 20000.0F, generator);
  }

  float testbuf[2] = {0.1, 0.3};

  mpx.compute(testbuf, 2);

  // std::cout << "Tick" << std::endl;
  std::chrono::system_clock::time_point const start = std::chrono::system_clock::now();

  mpx.floss();

  std::chrono::system_clock::time_point const end = std::chrono::system_clock::now();
  std::chrono::duration<float> const diff2 = end - start;
  printf("%.7f seconds to compute floss\n", diff2.count());
  // std::cout << "Tick" << std::endl;
  // mpx.compute(&TEST_DATA[3000], 1500);
  // std::cout << "Tick" << std::endl; // 4298.9072266
  // mpx.compute(&TEST_DATA[4500], 500);
  // std::cout << "Tick" << std::endl;
  // // }

  float *res = mpx.get_matrix();
  float sum = 0;

  for (uint16_t i = 0; i < 5000 - WIN_SIZE + 1; i++) {
    sum += res[i]; // < 0 ? 0 : res[i];
    // printf("%.7f, ", res[i]);
  }

  printf("Dono: %.7f\n", sum);

  return 0;
}
#endif // RASPBERRYPI
