#ifndef Mpx_h
#define Mpx_h

#if defined(RASPBERRYPI)
#include <stdio.h>
#include <stdint.h>
#include <memory>
#include <cmath>
#elif defined(ARDUINO_ESP32_DEV) || defined(ARDUINO_RASPBERRY_PI_PICO)
#include <Arduino.h>
// #include <CircularBuffer.h>
#else
#include <stdint.h>
#include <memory>
#include <cmath>
#include <cstdio>
#endif

#if !defined(NULL)
#define NULL 0
#endif

#if !defined(MIN)
#define MIN(y, x) ((x) < (y) && (x) == (x) ? (x) : (y))
#endif
#if !defined(MAX)
#define MAX(y, x) ((x) > (y) && (x) == (x) ? (x) : (y))
#endif

// uint16_t = 0 to 65535
// int16_t = -32768 to +32767

namespace MatrixProfile {
class Mpx {
public:
  // cppcheck-suppress noExplicitConstructor
  Mpx(const uint16_t window_size, const float ez = 0.5, const uint16_t time_constraint = 0, const uint16_t buffer_size = 5000);
  ~Mpx(); // destructor
  // void Compute();
  bool new_data(const float *data, uint16_t size);
  uint16_t compute(const float *data, uint16_t size = 0);
  void movmean();
  void movsig();
  void muinvn(uint16_t size = 0);
  void mp_next(uint16_t size = 0);
  void ddf(uint16_t size = 0);
  void ddg(uint16_t size = 0);
  void ww_s();

  // Getters
  float *get_matrix() { return vmatrix_profile_; };
  int16_t *get_indexes() { return vprofile_index_; };

private:
  const uint16_t window_size_;
  const float ez_;
  const uint16_t time_constraint_;
  const uint16_t buffer_size_;
  int16_t buffer_used_ = 0;
  int16_t buffer_start_ = 0;

  uint16_t profile_len_;
  uint16_t range_; // profile lenght - 1

  uint16_t exclusion_zone_;

  float last_movsum_ = 0.0;
  float last_mov2sum_ = 0.0;

  // arrays
  float *data_buffer_ = nullptr;
  float *vmatrix_profile_ = nullptr;
  int16_t *vprofile_index_ = nullptr;
  float *vmmu_ = nullptr;
  float *vsig_ = nullptr;
  float *vddf_ = nullptr;
  float *vddg_ = nullptr;
  float *vww_ = nullptr;
};

} // namespace MatrixProfile
#endif // Mpx_h
