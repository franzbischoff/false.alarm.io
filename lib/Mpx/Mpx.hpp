#ifndef Mpx_h
#define Mpx_h

#if defined(RASPBERRYPI)
#include <stdio.h>
#include <stdint.h>
#include <memory>
#include <cmath>
#elif defined(ARDUINO_ESP32_DEV) || defined(ARDUINO_RASPBERRY_PI_PICO)
#include <Arduino.h>
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
  Mpx(uint16_t window_size, float ez = 0.5, uint16_t time_constraint = 0, uint16_t buffer_size = 5000);
  ~Mpx(); // destructor
  // void Compute();
  void initial_data(const float *data, uint16_t size);
  void new_data(const float *data, uint16_t size);
  void compute_stream();
  void compute_next(const float *data, uint16_t size);
  void movmean();
  void movsig();
  void muinvn();
  void muinvn_next(uint16_t size);
  void mp_next(uint16_t size);
  void ddf_s();
  void ddf_next(uint16_t size);
  void ddg_s();
  void ddg_next(uint16_t size);
  void ww_s();
  void ww_next(uint16_t size);

  // Getters
  // float *get_ww() { return _vww; };
  // float *get_ddf() { return _vddf; };
  // float *get_ddg() { return _vddg; };
  // float *get_mmu() { return _vmmu; };
  // float *get_sig() { return _vsig; };
  float *get_matrix() { return vmatrix_profile_; };
  int16_t *get_indexes() { return vprofile_index_; };
  float *get_mmu() { return vmmu_; };
  float *get_sig() { return vsig_; };
  float *get_ddf() { return vddf_; };
  float *get_ddg() { return vddg_; };

private:
  uint16_t buffer_size_;
  uint16_t buffer_used_ = 0;
  uint16_t buffer_start_ = 0;
  uint16_t window_size_;
  float ez_;
  uint16_t time_constraint_;

  uint16_t profile_len_;
  uint16_t range_; // profile lenght - 1

  uint16_t exclusion_zone_;

  // debug arrays
  // float *_movmean = nullptr;
  // float *_movsig = nullptr;
  float last_movsum_ = 0.0;
  float last_mov2sum_ = 0.0;

  // arrays
  float *vww_ = nullptr;
  float *vddf_ = nullptr;
  float *vddg_ = nullptr;
  float *vmmu_ = nullptr;
  float *vsig_ = nullptr;
  float *data_buffer_ = nullptr;
  float *vmatrix_profile_ = nullptr;
  int16_t *vprofile_index_ = nullptr;
};

} // namespace MatrixProfile
#endif // Mpx_h
