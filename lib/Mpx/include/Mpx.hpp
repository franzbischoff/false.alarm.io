#ifndef Mpx_h
#define Mpx_h

#if defined(RASPBERRYPI)
#include <stdio.h>
#include <stdint.h>
#include <memory>
#include <cmath>
#elif !defined(ESP_PLATFORM) && (defined(ARDUINO_ESP32_DEV) || defined(ARDUINO_RASPBERRY_PI_PICO))
#include <Arduino.h>
// #include <CircularBuffer.h>
#define RAND() rand()
#define LOG_DEBUG(format, ...) printf(format, ##__VA_ARGS__)
#else
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <esp_random.h>
#include <esp_log.h>

#define RAND() (int32_t)(esp_random())
#define LOG_DEBUG(tag, format, ...) ESP_LOGD(tag, format, ##__VA_ARGS__)
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
  Mpx(uint16_t window_size, float ez = 0.5F, uint16_t time_constraint = 0U, uint16_t buffer_size = 5000U);
  ~Mpx(); // destructor
  // void Compute();
  bool new_data(const float *data, uint16_t size);
  uint16_t compute(const float *data, uint16_t size);
  void prune_buffer();
  void floss();
  void movmean();
  void movsig();
  void muinvn(uint16_t size = 0U);
  void mp_next(uint16_t size = 0U);
  void ddf(uint16_t size = 0U);
  void ddg(uint16_t size = 0U);
  void ww_s();

  // Getters
  float *get_data_buffer() { return data_buffer_; };
  float *get_matrix() { return vmatrix_profile_; };
  int16_t *get_indexes() { return vprofile_index_; };
  float *get_floss() { return floss_; };
  float *get_iac() { return iac_; };
  float *get_vmmu() { return vmmu_; };
  float *get_vsig() { return vsig_; };
  float *get_ddf() { return vddf_; };
  float *get_ddg() { return vddg_; };
  float *get_vww() { return vww_; };
  const uint16_t get_buffer_used() { return buffer_used_; };
  const int16_t get_buffer_start() { return buffer_start_; };
  const uint16_t get_profile_len() { return profile_len_; };
  const float get_last_movsum() { return last_accum_ + last_resid_; };
  const float get_last_mov2sum() { return last_accum2_ + last_resid2_; };

private:
  void floss_iac_();

  const uint16_t window_size_;
  const float ez_;
  const uint16_t time_constraint_;
  const uint16_t buffer_size_;
  uint16_t buffer_used_ = 0U;
  int16_t buffer_start_ = 0;

  uint16_t profile_len_;
  uint16_t range_; // profile lenght - 1

  uint16_t exclusion_zone_;

  float last_accum_ = 0.0F;
  float last_resid_ = 0.0F;
  float last_accum2_ = 0.0F;
  float last_resid2_ = 0.0F;

  // arrays
  float *data_buffer_ = nullptr;
  float *vmatrix_profile_ = nullptr;
  int16_t *vprofile_index_ = nullptr;
  float *floss_ = nullptr;
  float *iac_ = nullptr;
  float *vmmu_ = nullptr;
  float *vsig_ = nullptr;
  float *vddf_ = nullptr;
  float *vddg_ = nullptr;
  float *vww_ = nullptr;
};

} // namespace MatrixProfile
#endif // Mpx_h
