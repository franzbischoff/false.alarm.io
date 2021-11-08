#ifndef Mpx_h
#define Mpx_h

#ifdef ARDUINO_ARCH_AVR
#include <ArduinoSTL.h>
#include <vector>
#include <numeric>
#else
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#endif

#ifndef MIN
#define MIN(y, x) ((x) < (y) && (x) == (x) ? (x) : (y))
#endif
#ifndef MAX
#define MAX(y, x) ((x) > (y) && (x) == (x) ? (x) : (y))
#endif

namespace MatrixProfile
{

  typedef struct ogita
  {
    std::vector<float> avg;
    std::vector<float> sig;
  } ogita_t;

  class Mpx
  {
  public:
    Mpx(std::vector<float> data, uint16_t window_size, float ez, uint16_t mp_time_constraint);
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

} // namespace MatrixProfile
#endif
