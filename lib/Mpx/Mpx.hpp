#ifndef Mpx_h
#define Mpx_h

#if defined(USE_STL)
#if defined(RASPBERRYPI)
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#elif defined(ESP32_IDF_DEV)
#include <stdint.h>
#elif defined(ARDUINO_ARCH_AVR) || defined(ARDUINO_ESP32_DEV)
#include <ArduinoSTL.h>
#include <vector>
#include <numeric>
#else
#include <stdint.h>
#endif // ARDUINO_ARCH_AVR
#else
#if defined(RASPBERRYPI)
#include <stdio.h>
#include <stdint.h>
#include <memory>
#include <cmath>
#elif defined(ESP32_IDF_DEV)
#include <stdint.h>
#elif defined(ARDUINO_ARCH_AVR) || defined(ARDUINO_ESP32_DEV)
#include <Arduino.h>
#else
#include <stdint.h>
#include <memory>
#include <cmath>
#include <cstdio>
#endif // ARDUINO_ARCH_AVR
#endif // USE_STL

#if !defined(NULL)
#define NULL 0
#endif

#if !defined(MIN)
#define MIN(y, x) ((x) < (y) && (x) == (x) ? (x) : (y))
#endif
#if !defined(MAX)
#define MAX(y, x) ((x) > (y) && (x) == (x) ? (x) : (y))
#endif

namespace MatrixProfile {

#if defined(USE_STL)
typedef struct ogita {
  std::vector<float> avg;
  std::vector<float> sig;
} ogita_t;

class Mpx {
public:
  Mpx(std::vector<float> data, uint32_t debug_data_size, uint16_t window_size, float ez, uint16_t mp_time_constraint,
      uint16_t history);
  void Compute();
  void ComputeStream();
  std::vector<float> movsum(std::vector<float> data, uint32_t window_size);
  void muinvn(const std::vector<float> data, uint32_t window_size, ogita_t &result);
  std::vector<float> Ddf();
  std::vector<float> Ddg();
  std::vector<float> Ww();
  std::vector<float> Ddf_s();
  std::vector<float> Ddg_s();
  std::vector<float> Ww_s();
  std::vector<float> get_matrix() { return _vmatrix_profile; };
  std::vector<int> get_indexes() { return _vprofile_index; };

private:
  uint32_t _profile_len;
  uint32_t _data_len;
  uint32_t _diag_start;
  uint32_t _diag_end;
  uint16_t _window_size;
  uint16_t _exclusion_zone;
  float _ez;
  uint16_t _mp_time_constraint;
  std::vector<float> _vmmu;
  std::vector<float> _vsig;
  std::vector<float> _vdata;
  std::vector<float> _vmatrix_profile;
  std::vector<int> _vprofile_index;
};
#else
class Mpx {
public:
  // cppcheck-suppress noExplicitConstructor
  Mpx(float *data, uint32_t debug_data_size = 100, uint16_t window_size = 200, float ez = 0.5,
      uint16_t mp_time_constraint = 0, uint16_t history = 5000);
  ~Mpx(); // destructor
  // void Compute();
  void ComputeStream();
  void movsum();
  void mov2sum();
  void muinvn();
  void Ddf_s();
  void Ddg_s();
  void Ww_s();

  void ComputeStream2();
  float get_ddf(uint32_t idx);
  float get_ddg(uint32_t idx);
  float get_ww(uint32_t idx);
  float get_sig(uint32_t idx);
  float get_mu(uint32_t idx);

  // Getters
  // float *get_ww() { return _vww; };
  // float *get_ddf() { return _vddf; };
  // float *get_ddg() { return _vddg; };
  // float *get_mmu() { return _vmmu; };
  // float *get_sig() { return _vsig; };
  float *get_matrix() { return _vmatrix_profile; };
  int16_t *get_indexes() { return _vprofile_index; };

private:
  uint32_t _profile_len;
  uint32_t _range;
  uint32_t _data_len;
  uint32_t _diag_start;
  uint32_t _diag_end;
  uint16_t _window_size;
  uint16_t _exclusion_zone;
  float _ez;
  uint16_t _mp_time_constraint;

  // debug arrays
  float *_movsum = NULL;
  float *_mov2sum = NULL;

  // arrays
  float *_vww = NULL;
  float *_vddf = NULL;
  float *_vddg = NULL;
  float *_vmmu = NULL;
  float *_vsig = NULL;
  float *_vmatrix_profile = NULL;
  int16_t *_vprofile_index = NULL;
  float *_vdata = NULL;
};

#endif // USE_STL

} // namespace MatrixProfile
#endif // Mpx_h
