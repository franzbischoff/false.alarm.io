#include "Mpx.hpp"
#include <algorithm>

namespace MatrixProfile {

Mpx::Mpx(std::vector<float> data, uint16_t window_size, float ez, uint16_t mp_time_constraint) {
  this->_data = data;
  this->_window_size = window_size;
  this->_ez = ez;
  this->_mp_time_constraint = mp_time_constraint;
  this->_data_len = data.size();
  this->_profile_len = _data_len - window_size + 1;
  this->_exclusion_zone = round(window_size * ez + __DBL_EPSILON__) + 1;
  this->_diag_start = _exclusion_zone;
  this->_diag_end = _profile_len;
  this->_matrix_profile = std::vector<float>(_profile_len, -1);
  this->_profile_index = std::vector<int>(_profile_len, -1);

  if (_mp_time_constraint > 0) {
    _diag_end = _mp_time_constraint;
  }
}

void Mpx::Compute() {
  float c, c_cmp;

  ogita_t meansig;
  muinvn(_data, _window_size, meansig);
  _mmu = meansig.avg;
  _sig = meansig.sig;

  std::vector<float> df = Ddf();
  std::vector<float> dg = Ddg();
  std::vector<float> ww = Ww();

  for (uint32_t i = _diag_start; i < _diag_end; i++) {
    // this mess is just the inner_product but _data needs to be minus _mmu[i] before multiply
    float m = _mmu[i];
    c = std::inner_product(_data.begin() + i, _data.begin() + (i + _window_size - 1), ww.begin(), 0.0,
                           std::plus<float>(), [m](float &x, float &y) { return ((x - m) * y); });

    uint32_t off_max = (_data_len - _window_size - i + 1);

    for (uint32_t offset = 0; offset < off_max; offset++) {
      uint32_t off_diag = offset + i;
      c = c + df[offset] * dg[off_diag] + df[off_diag] * dg[offset];
      c_cmp = c * _sig[offset] * _sig[off_diag];

      // MP
      if (c_cmp > _matrix_profile[offset]) {
        _matrix_profile[offset] = c_cmp;
        _profile_index[offset] = off_diag + 1;
      }
      if (c_cmp > _matrix_profile[off_diag]) {
        _matrix_profile[off_diag] = c_cmp;
        _profile_index[off_diag] = offset + 1;
      }
    }
  }
}

std::vector<float> Mpx::movsum(std::vector<float> data, uint32_t window_size) {

  std::vector<float> res(data.size() - window_size + 1, 0);
  double accum = data[0];
  double resid = 0.0;

  for (uint16_t i = 1; i < window_size; i++) {
    double m = data[i];
    double p = accum;
    accum = accum + m;
    double q = accum - p;
    resid = resid + ((p - (accum - q)) + (m - q));
  }

  if (resid > 0.001) {
    std::cout << "Residual value is large. Some precision may be lost. res = " << resid << std::endl;
  }

  res[0] = accum + resid;

  for (int32_t i = window_size; i < data.size(); i++) {
    double m = data[i - window_size];
    double n = data[i];
    double p = accum - m;
    double q = p - accum;
    double r = resid + ((accum - (p - q)) - (m + q));
    accum = p + n;
    double t = accum - p;
    resid = r + ((p - (accum - t)) + (n - t));
    res[i - window_size + 1] = accum + resid;
  }

  return (res);
}

void Mpx::muinvn(const std::vector<float> data, uint32_t window_size, ogita_t &result) {
  std::vector<float> mu = movsum(data, window_size);
  transform(mu.begin(), mu.end(), mu.begin(), [window_size](float &c) { return c / (float)window_size; });
  std::vector<float> data2(data);
  transform(data2.begin(), data2.end(), data2.begin(), data2.begin(), std::multiplies<float>());
  std::vector<float> data2_sum = movsum(data2, window_size);

  std::vector<float> sig(data.size() - window_size + 1, 0);

  for (uint32_t i = 0; i < sig.size(); i++) {
    sig[i] = 1 / sqrt(data2_sum[i] - mu[i] * mu[i] * window_size);
  }

  result.avg = mu;
  result.sig = sig;
}

std::vector<float> Mpx::Ddf() {
  uint32_t range = this->_data_len - this->_window_size;
  std::vector<float> ddf(range + 1, 0);

  // differentials have 0 as their first entry. This simplifies index
  // calculations slightly and allows us to avoid special "first line"
  // handling.

  for (uint32_t i = 0; i < range; i++) {
    ddf[i + 1] = 0.5 * this->_data[i + this->_window_size] - this->_data[i];
  }

  return ddf;
}

std::vector<float> Mpx::Ddg() {
  uint32_t range = this->_data_len - this->_window_size;
  std::vector<float> ddg(range + 1, 0);

  // differentials have 0 as their first entry. This simplifies index
  // calculations slightly and allows us to avoid special "first line"
  // handling.

  for (uint32_t i = 0; i < range; i++) {
    ddg[i + 1] = (this->_data[i + this->_window_size] - this->_mmu[i + 1]) + (this->_data[i] - this->_mmu[i]);
  }

  return ddg;
}

std::vector<float> Mpx::Ww() {
  uint32_t range = this->_window_size;
  std::vector<float> ww(range, 0);

  for (uint32_t i = 0; i < range; i++) {
    ww[i] = (this->_data[i] - this->_mmu[0]);
  }
  return ww;
}

} // namespace MatrixProfile
