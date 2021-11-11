#if defined(ARDUINO_ARCH_AVR) || defined(ARDUINO_ESP32_DEV)
#include <Arduino.h>
// #include "avr8-stub.h"
// #include "app_api.h" // only needed with flash breakpoints
// #include <unity.h>
#elif RASPBERRYPI2
#include <stdio.h>
// #include <wiringPi.h>
#endif

#include <Mpx.hpp>

#ifdef USE_STL
std::vector<float> test_data = {
#else
#define DATA_SIZE 100
#define WIN_SIZE 10
float test_data[] = {
#endif
    55.43, 78.76, 43.83, 1.14,  98.50, 2.79,  29.80, 90.06, 65.44, 92.80, 87.86, 62.11, 72.78, 14.68, 31.69,
    48.14, 57.24, 70.79, 93.75, 9.28,  18.05, 8.68,  6.23,  73.46, 45.41, 32.59, 28.32, 57.32, 8.73,  61.63,
    2.70,  16.51, 53.18, 58.40, 22.92, 30.94, 40.17, 7.66,  17.23, 9.61,  41.01, 64.25, 92.30, 82.79, 19.08,
    96.84, 31.22, 33.42, 89.03, 34.07, 92.16, 15.32, 92.70, 95.81, 39.43, 27.46, 39.96, 71.79, 67.74, 73.43,
    42.42, 42.71, 80.19, 3.88,  38.81, 80.11, 28.13, 31.69, 29.31, 97.22, 78.79, 25.01, 36.73, 12.52, 80.24,
    17.95, 87.75, 96.40, 91.93, 22.08, 31.38, 94.84, 17.10, 46.22, 5.18,  80.72, 18.91, 42.84, 88.48, 20.10,
    97.17, 53.87, 18.40, 94.31, 3.36,  40.19, 76.92, 57.16, 13.82, 63.26};

// float mp[4900];
// int16_t pi[4900];
// float mu[4900];
// float si[4900];
// float df[4900];
// float dg[4900];
// float www[300];

MatrixProfile::Mpx mpx(test_data, DATA_SIZE, WIN_SIZE, 0.5, 0, 5000);

#if defined(ARDUINO_ARCH_AVR) || defined(ARDUINO_ESP32_DEV)
int done = 0;
// cppcheck-suppress unusedFunction
void setup() {
  // debug_init();
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600);
  delay(100);
  Serial.println("Teste");
}

// cppcheck-suppress unusedFunction
void loop() {

  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);
  if (done)
    return;

  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);

  mpx.ComputeStream();
  // Serial.println("Teste3");
  float *res = mpx.get_matrix();

  for (uint32_t i = 0; i < DATA_SIZE - WIN_SIZE + 1; i++) {
    // printf("%.2f\n", (double)res[i]);
    Serial.println((double)res[i], 2);
  }

  // for (unsigned int i = 0; i < res.size(); i++) {
  //   Serial.println(res[i]);
  // }
  // Serial.println("Done");
  done = 1;

  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(3000);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
}
#else
int main(int argc, char **argv) {
  mpx.ComputeStream();

  return 0;
}
#endif

// int main(int argc, char **argv) {

// #ifdef ARDUINO_ARCH_AVR
//   // UNITY_BEGIN();
//   setup();

//   while (!done)
//     loop();

//     // RUN_TEST(test_connect);

//     // return UNITY_END();

// #else
//   MatrixProfile::Mpx mpx(test_data, 100, 10, 0.5, 0, 5000);

// #ifdef USE_STL
//   std::vector<float> res;
//   std::vector<int> idx;
// #else
//   float *res;
//   int16_t *idx;
// #endif // USE_STL

//   // mpx.Compute();
//   mpx.ComputeStream();

//   res = mpx.get_matrix();

//   printf("\n");

//   for (uint32_t i = 0; i < 100 - 10 + 1; i++) {
//     printf("%.2f, ", res[i]);
//   }

//   printf("\n");
//   printf("\n");

//   idx = mpx.get_indexes();

//   for (uint32_t i = 0; i < 100 - 10 + 1; i++) {
//     printf("%d, ", idx[i]);
//   }
//   printf("\n");

//   return 0;
// #endif
// }
