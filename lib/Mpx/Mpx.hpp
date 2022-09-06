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
  void new_data(const float *data, uint16_t size);
  void compute_stream();
  void compute_next(float *data, uint16_t size);
  void movmean();
  void movsig();
  void muinvn();
  void muinvn_next(uint16_t size);
  void ddf_s();
  void ddf_next(uint16_t size);
  void ddg_s();
  void ddg_next(uint16_t size);
  void ww_s();
  void ww_next(uint16_t size);

  // void compute_stream2();
  float get_ddf(uint16_t idx);
  float get_ddg(uint16_t idx);
  float get_ww(uint16_t idx);
  float get_sig(uint16_t idx);
  float get_mu(uint16_t idx);

  // Getters
  // float *get_ww() { return _vww; };
  // float *get_ddf() { return _vddf; };
  // float *get_ddg() { return _vddg; };
  // float *get_mmu() { return _vmmu; };
  // float *get_sig() { return _vsig; };
  float *get_matrix() { return _vmatrix_profile; };
  int16_t *get_indexes() { return _vprofile_index; };

private:
  uint16_t _buffer_size;
  uint16_t _window_size;
  float _ez;
  uint16_t _time_constraint;

  uint16_t _profile_len;
  uint16_t _range; // profile lenght - 1

  uint16_t _exclusion_zone;

  // debug arrays
  // float *_movmean = nullptr;
  // float *_movsig = nullptr;
  float _last_movsum = 0.0;
  float _last_mov2sum = 0.0;

  // arrays
  float *_vww = nullptr;
  float *_vddf = nullptr;
  float *_vddg = nullptr;
  float *_vmmu = nullptr;
  float *_vsig = nullptr;
  float *_data_buffer = nullptr;
  float *_vmatrix_profile = nullptr;
  int16_t *_vprofile_index = nullptr;
};

} // namespace MatrixProfile
#endif // Mpx_h
