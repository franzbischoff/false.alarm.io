#ifndef Mpx_h
#define Mpx_h

#define USE_STL2

#ifdef USE_STL
#if defined(ARDUINO_ARCH_AVR) || defined(ARDUINO_ESP32_DEV)
#include <ArduinoSTL.h>
#include <vector>
#include <numeric>
#else
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#endif // ARDUINO_ARCH_AVR
#else
#if defined(ARDUINO_ARCH_AVR) || defined(ARDUINO_ESP32_DEV)
#include <Arduino.h>
#elif RASPBERRYPI2
#include <stdio.h>
#include <stdint.h>
#include <memory>
#include <cmath>
#else
#include <stdint.h>
#include <memory>
#include <cmath>
#endif // ARDUINO_ARCH_AVR
#endif // USE_STL

#ifndef MIN
#define MIN(y, x) ((x) < (y) && (x) == (x) ? (x) : (y))
#endif
#ifndef MAX
#define MAX(y, x) ((x) > (y) && (x) == (x) ? (x) : (y))
#endif

namespace MatrixProfile {

#ifdef USE_STL
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
  std::vector<float> get_matrix() { return _matrix_profile; };
  std::vector<int> get_indexes() { return _profile_index; };

private:
  uint32_t _profile_len;
  uint32_t _data_len;
  uint32_t _diag_start;
  uint32_t _diag_end;
  uint16_t _window_size;
  uint16_t _exclusion_zone;
  float _ez;
  uint16_t _mp_time_constraint;
  std::vector<float> _mmu;
  std::vector<float> _sig;
  std::vector<float> _data;
  std::vector<float> _matrix_profile;
  std::vector<int> _profile_index;
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

  // Getters
  float *get_ww() { return _ww; };
  float *get_ddf() { return _ddf; };
  float *get_ddg() { return _ddg; };
  float *get_mmu() { return _mmu; };
  float *get_sig() { return _sig; };
  float *get_matrix() { return _matrix_profile; };
  int16_t *get_indexes() { return _profile_index; };

private:
  uint32_t _profile_len;
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
  float *_ww = NULL;
  float *_ddf = NULL;
  float *_ddg = NULL;
  float *_mmu = NULL;
  float *_sig = NULL;
  float *_matrix_profile = NULL;
  int16_t *_profile_index = NULL;
  float *_data = NULL;
};

#endif // USE_STL

} // namespace MatrixProfile
#endif // Mpx_h
