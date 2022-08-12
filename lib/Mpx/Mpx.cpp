#include "Mpx.hpp"

namespace MatrixProfile {

#if defined(USE_STL)
Mpx::Mpx(std::vector<float> data, uint32_t debug_data_size, uint16_t window_size, float ez, uint16_t mp_time_constraint,
         uint16_t history) {
  this->_vdata = data;
  this->_window_size = window_size;
  this->_ez = ez;
  this->_mp_time_constraint = mp_time_constraint;
  this->_data_len = data.size();
  this->_profile_len = _data_len - window_size + 1;
  this->_exclusion_zone = round(window_size * ez + __DBL_EPSILON__) + 1;
  this->_diag_start = _exclusion_zone;
  this->_diag_end = _profile_len;
  this->_vmatrix_profile = std::vector<float>(_profile_len, -1);
  this->_vprofile_index = std::vector<int>(_profile_len, -1);

  if (_mp_time_constraint > 0) {
    _diag_end = _mp_time_constraint;
  }
}

void Mpx::Compute() {
  float c, c_cmp;

  ogita_t meansig;
  muinvn(_vdata, _window_size, meansig);
  _vmmu = meansig.avg;
  _vsig = meansig.sig;

  std::vector<float> df = Ddf();
  std::vector<float> dg = Ddg();
  std::vector<float> ww = Ww();

  for (uint32_t i = _diag_start; i < _diag_end; i++) {
    // this mess is just the inner_product but _vdata needs to be minus _vmmu[i] before multiply
    float m = _vmmu[i];
    c = std::inner_product(_vdata.begin() + i, _vdata.begin() + (i + _window_size), ww.begin(), 0.0, std::plus<float>(),
                           [m](float &x, float &y) { return ((x - m) * y); });

    uint32_t off_max = (_data_len - _window_size - i + 1);

    for (uint32_t offset = 0; offset < off_max; offset++) {
      uint32_t off_diag = offset + i;
      c = c + df[offset] * dg[off_diag] + df[off_diag] * dg[offset];
      c_cmp = c * _vsig[offset] * _vsig[off_diag];

      // RMP
      if (c_cmp > _vmatrix_profile[offset]) {
        _vmatrix_profile[offset] = c_cmp;
        _vprofile_index[offset] = off_diag + 1;
      }
      // LMP
      if (c_cmp > _vmatrix_profile[off_diag]) {
        _vmatrix_profile[off_diag] = c_cmp;
        _vprofile_index[off_diag] = offset + 1;
      }
    }
  }
}

void Mpx::ComputeStream() {
  float c, c_cmp;

  uint16_t new_data_len = 80; // incoming data

  this->_diag_start = 0;
  this->_diag_end = this->_profile_len - this->_exclusion_zone;

  ogita_t meansig;
  muinvn(_vdata, _window_size, meansig);
  _vmmu = meansig.avg;
  _vsig = meansig.sig;

  std::vector<float> df = Ddf_s();
  std::vector<float> dg = Ddg_s();
  std::vector<float> ww = Ww_s();

  for (uint32_t i = _diag_start; i < _diag_end; i++) {
    // this mess is just the inner_product but _vdata needs to be minus _vmmu[i] before multiply
    float m = _vmmu[i];
    c = std::inner_product(_vdata.begin() + i, _vdata.begin() + (i + _window_size), ww.begin(), 0.0, std::plus<float>(),
                           [m](float &x, float &y) { return ((x - m) * y); });

    uint32_t off_min = MAX(_data_len - _window_size - new_data_len, _data_len - _window_size - i - 1);
    uint32_t off_start = _data_len - _window_size;

    for (uint32_t offset = off_start; offset > off_min; offset--) {
      // min is offset + diag; max is (profile_len - 1); each iteration has the size of off_max
      uint32_t off_diag = offset - (_data_len - _window_size - i);

      c = c + df[offset] * dg[off_diag] + df[off_diag] * dg[offset];
      c_cmp = c * _vsig[offset] * _vsig[off_diag];

      // RMP
      if (c_cmp > _vmatrix_profile[off_diag]) {
        _vmatrix_profile[off_diag] = c_cmp;
        _vprofile_index[off_diag] = offset + 1;
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

  // if (resid > 0.001)
  // {
  //   std::cout << "Residual value is large. Some precision may be lost. res = " << resid << std::endl;
  // }

  res[0] = accum + resid;

  for (uint32_t i = window_size; i < data.size(); i++) {
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
  std::transform(mu.begin(), mu.end(), mu.begin(), [window_size](float &c) { return c / (float)window_size; });
  std::vector<float> data2(data);
  std::transform(data2.begin(), data2.end(), data2.begin(), data2.begin(), std::multiplies<float>());
  std::vector<float> data2_sum = movsum(data2, window_size);

  std::vector<float> sig(data.size() - window_size + 1, 0);

  for (uint32_t i = 0; i < sig.size(); i++) {
    sig[i] = 1 / sqrt(data2_sum[i] - mu[i] * mu[i] * window_size);
  }

  result.avg = mu;
  result.sig = sig;
}

std::vector<float> Mpx::Ddf_s() {
  uint32_t range = this->_data_len - this->_window_size;
  std::vector<float> ddf(range + 1, 0);

  // differentials have 0 as their first entry. This simplifies index
  // calculations slightly and allows us to avoid special "first line"
  // handling.

  for (uint32_t i = 0; i < range; i++) {
    ddf[i] = 0.5 * (this->_vdata[i] - this->_vdata[i + this->_window_size]);
  }

  return ddf;
}

std::vector<float> Mpx::Ddg_s() {
  uint32_t range = this->_data_len - this->_window_size;
  std::vector<float> ddg(range + 1, 0);

  // differentials have 0 as their first entry. This simplifies index
  // calculations slightly and allows us to avoid special "first line"
  // handling.

  for (uint32_t i = 0; i < range; i++) {
    ddg[i] = (this->_vdata[i + this->_window_size] - this->_vmmu[i + 1]) + (this->_vdata[i] - this->_vmmu[i]);
  }

  return ddg;
}

std::vector<float> Mpx::Ww_s() {
  uint32_t range = this->_window_size;
  std::vector<float> ww(range, 0);

  for (uint32_t i = 0; i < range; i++) {
    ww[i] =
        (this->_vdata[this->_data_len - this->_window_size + i] - this->_vmmu[this->_data_len - this->_window_size]);
  }
  return ww;
}

std::vector<float> Mpx::Ddf() {
  uint32_t range = this->_data_len - this->_window_size;
  std::vector<float> ddf(range + 1, 0);

  // differentials have 0 as their first entry. This simplifies index
  // calculations slightly and allows us to avoid special "first line"
  // handling.

  for (uint32_t i = 0; i < range; i++) {
    ddf[i + 1] = 0.5 * (this->_vdata[i + this->_window_size] - this->_vdata[i]);
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
    ddg[i + 1] = (this->_vdata[i + this->_window_size] - this->_vmmu[i + 1]) + (this->_vdata[i] - this->_vmmu[i]);
  }

  return ddg;
}

std::vector<float> Mpx::Ww() {
  uint32_t range = this->_window_size;
  std::vector<float> ww(range, 0);

  for (uint32_t i = 0; i < range; i++) {
    ww[i] = (this->_vdata[i] - this->_vmmu[0]);
  }
  return ww;
}
#else
Mpx::Mpx(float *data, uint32_t debug_data_size, uint16_t window_size, float ez, uint16_t mp_time_constraint,
         uint16_t history) {

  this->_window_size = window_size;
  this->_ez = ez;
  this->_mp_time_constraint = mp_time_constraint;
  this->_data_len = debug_data_size; // history
  this->_profile_len = _data_len - window_size + 1;
  this->_range = _data_len - window_size;
  this->_exclusion_zone = round(window_size * ez + __DBL_EPSILON__) + 1;
  this->_diag_start = _exclusion_zone;
  this->_diag_end = _profile_len;

  // allocate debug arrays
  this->_movsum = (float *)calloc(_profile_len + 1, sizeof(float));
  this->_mov2sum = (float *)calloc(_profile_len + 1, sizeof(float));

  // allocate arrays
  this->_vdata = data;
  this->_vmatrix_profile = (float *)malloc(sizeof(float) * _profile_len + 1);
  this->_vprofile_index = (int16_t *)malloc(sizeof(int16_t) * _profile_len + 1);
  // change the default value to -1
  for (uint32_t i = 0; i < _profile_len; i++) {
    this->_vmatrix_profile[i] = -1;
    this->_vprofile_index[i] = -1;
  }

  // this->_vmmu = (float *)calloc(_profile_len + 1, sizeof(float));
  // this->_vsig = (float *)calloc(_profile_len + 1, sizeof(float));
  this->_vddf = (float *)calloc(_profile_len + 1, sizeof(float));
  this->_vddg = (float *)calloc(_profile_len + 1, sizeof(float));
  this->_vww = (float *)calloc(_window_size + 1, sizeof(float));

  if (_mp_time_constraint > 0) {
    _diag_end = _mp_time_constraint;
  }
}

void Mpx::movsum() {

  double accum = this->_vdata[0];
  double resid = 0.0;

  for (uint16_t i = 1; i < this->_window_size; i++) {
    double m = this->_vdata[i];
    double p = accum;
    accum = accum + m;
    double q = accum - p;
    resid = resid + ((p - (accum - q)) + (m - q));
  }

  if (resid > 0.001) {
    printf("Residual value is large. Some precision may be lost. res = %.2f\n", resid);
  }

  this->_movsum[0] = accum + resid;

  for (uint32_t i = this->_window_size; i < this->_data_len; i++) {
    double m = this->_vdata[i - this->_window_size];
    double n = this->_vdata[i];
    double p = accum - m;
    double q = p - accum;
    double r = resid + ((accum - (p - q)) - (m + q));
    accum = p + n;
    double t = accum - p;
    resid = r + ((p - (accum - t)) + (n - t));
    this->_movsum[i - this->_window_size + 1] = accum + resid;
  }
}

void Mpx::mov2sum() {

  double accum = this->_vdata[0] * this->_vdata[0];
  double resid = 0.0;

  for (uint16_t i = 1; i < this->_window_size; i++) {
    double m = this->_vdata[i] * this->_vdata[i];
    double p = accum;
    accum = accum + m;
    double q = accum - p;
    resid = resid + ((p - (accum - q)) + (m - q));
  }

  if (resid > 0.001) {
    printf("Residual value is large. Some precision may be lost. res = %.2f\n", resid);
  }

  this->_mov2sum[0] = accum + resid;

  for (uint32_t i = this->_window_size; i < this->_data_len; i++) {
    double m = this->_vdata[i - this->_window_size] * this->_vdata[i - this->_window_size];
    double n = this->_vdata[i] * this->_vdata[i];
    double p = accum - m;
    double q = p - accum;
    double r = resid + ((accum - (p - q)) - (m + q));
    accum = p + n;
    double t = accum - p;
    resid = r + ((p - (accum - t)) + (n - t));
    this->_mov2sum[i - this->_window_size + 1] = accum + resid;
  }
}

void Mpx::muinvn() {
  for (uint32_t i = 0; i < this->_profile_len; i++) {
    this->_vmmu[i] = (float)(this->_movsum[i] / (float)this->_window_size);
  }

  for (uint32_t i = 0; i < this->_profile_len; i++) {
    // this->_vsig[i] = 1 / sqrt(this->_mov2sum[i] - this->_vmmu[i] * this->_vmmu[i] * this->_window_size);
    this->_vsig[i] = 1 / sqrt(this->_mov2sum[i] - this->_movsum[i] * this->_movsum[i] / (float)this->_window_size);
  }
}

void Mpx::Ddf_s() {
  // differentials have 0 as their first entry. This simplifies index
  // calculations slightly and allows us to avoid special "first line"
  // handling.

  for (uint32_t i = 0; i < _range; i++) {
    this->_vddf[i] = 0.5 * (this->_vdata[i] - this->_vdata[i + this->_window_size]);
  }

  this->_vddf[_range] = 0.0;
}

void Mpx::Ddg_s() {
  // differentials have 0 as their first entry. This simplifies index
  // calculations slightly and allows us to avoid special "first line"
  // handling.

  for (uint32_t i = 0; i < _range; i++) {
    this->_vddg[i] = (this->_vdata[i + this->_window_size] - this->_vmmu[i + 1]) + (this->_vdata[i] - this->_vmmu[i]);
  }

  this->_vddg[_range] = 0.0;
}

void Mpx::Ww_s() {
  for (uint32_t i = 0; i < _window_size; i++) {
    this->_vww[i] = (this->_vdata[_range + i] - this->_vmmu[_range]);
  }
}

void Mpx::ComputeStream() {
  uint16_t new_data_len = _data_len - _window_size * 2; // incoming data

  // if (mp_time_constraint > 0) {
  //   diag_start = _data_len - mp_time_constraint - _window_size;
  // }

  this->_diag_start = 0;
  this->_diag_end = this->_profile_len - this->_exclusion_zone;

  uint16_t debug = 0;

  Serial.println(debug++);

  movsum();

  Serial.println(debug++);

  mov2sum();

  Serial.println(debug++);

  // muinvn();

  Serial.println(debug++);

  Ddf_s();

  Serial.println(debug++);

  Ddg_s();

  Serial.println(debug++);

  Ww_s();

  Serial.println(debug++);

  for (uint32_t i = _diag_start; i < _diag_end; i++) {
    // this mess is just the inner_product but _vdata needs to be minus _vmmu[i] before multiply
    float c = 0.0;

    if (!(i % 100)) {
      Serial.println(i);
    }

    // inner product demeaned
    for (uint32_t j = 0; j < _window_size; j++) {
      c += (_vdata[i + j] - _vmmu[i]) * _vww[j];
    }

    uint32_t off_min = MAX(_range - new_data_len, _range - i - 1);
    uint32_t off_start = _range;

    // loop if more than 1 new data
    for (uint32_t offset = off_start; offset > off_min; offset--) {
      // min is offset + diag; max is (profile_len - 1); each iteration has the size of off_max
      uint32_t off_diag = offset - (_range - i);

      c += _vddf[offset] * _vddg[off_diag] + _vddf[off_diag] * _vddg[offset];
      float c_cmp = c * _vsig[offset] * _vsig[off_diag];

      // RMP
      if (c_cmp > _vmatrix_profile[off_diag]) {
        _vmatrix_profile[off_diag] = c_cmp;
        _vprofile_index[off_diag] = offset + 1;
      }
    }
  }
}

float Mpx::get_ddf(uint32_t idx) {
  if (idx < _range) {
    if (this->_vddf[idx] == 0.0)
      this->_vddf[idx] = 0.5 * (this->_vdata[idx] - this->_vdata[idx + this->_window_size]);
    return this->_vddf[idx];
  }

  return 0.0;
}

float Mpx::get_ddg(uint32_t idx) {
  if (idx < _range) {
    if (this->_vddg[idx] == 0.0)
      this->_vddg[idx] = (this->_vdata[idx + this->_window_size] - get_mu(idx + 1)) + (this->_vdata[idx] - get_mu(idx));
    return this->_vddg[idx];
  }

  return 0.0;
}

float Mpx::get_sig(uint32_t idx) {
  if (idx < _profile_len) {
    // if (this->_vsig[idx] == 0.0)
    //   this->_vsig[idx] =
    //       1 / sqrt(this->_mov2sum[idx] - this->_movsum[idx] * this->_movsum[idx] / (float)this->_window_size);
    // return this->_vsig[idx];

    return (1 / sqrt(this->_mov2sum[idx] - this->_movsum[idx] * this->_movsum[idx] / (float)this->_window_size));
  }

  return 0.0;
}

float Mpx::get_mu(uint32_t idx) {
  if (idx < _profile_len) {
    // if (this->_vmmu[idx] == 0.0)
    //   this->_vmmu[idx] = (float)(this->_movsum[idx] / (float)this->_window_size);
    // return this->_vmmu[idx];

    return (float)(this->_movsum[idx] / (float)this->_window_size);
  }

  return 0.0;
}

float Mpx::get_ww(uint32_t idx) {
  if (idx < _window_size) {
    if (this->_vww[idx] == 0.0)
      this->_vww[idx] = (this->_vdata[_range + idx] - get_mu(_range));
    return this->_vww[idx];
  }

  return 0.0;
}

void Mpx::ComputeStream2() {
  uint16_t new_data_len = _data_len - _window_size * 2; // incoming data

  // if (mp_time_constraint > 0) {
  //   diag_start = _data_len - mp_time_constraint - _window_size;
  // }

  this->_diag_start = 0;
  this->_diag_end = this->_profile_len - this->_exclusion_zone;

  movsum();
  mov2sum();
  // muinvn();

  for (uint32_t i = _diag_start; i < _diag_end; i++) {
    // this mess is just the inner_product but _vdata needs to be minus _vmmu[i] before multiply

    if (!(i % 100)) {
      Serial.println(i);
    }

    float c = 0.0;

    // inner product demeaned
    for (uint32_t j = 0; j < _window_size; j++) {
      c += (_vdata[i + j] - get_mu(i)) * get_ww(j);
    }

    uint32_t off_min = MAX(_range - new_data_len, _range - i - 1);
    uint32_t off_start = _range;

    // loop if more than 1 new data
    for (uint32_t offset = off_start; offset > off_min; offset--) {
      // min is offset + diag; max is (profile_len - 1); each iteration has the size of off_max
      uint32_t off_diag = offset - (_range - i);

      c += get_ddf(offset) * get_ddg(off_diag) + get_ddf(off_diag) * get_ddg(offset);
      float c_cmp = c * get_sig(offset) * get_sig(off_diag);

      // RMP
      if (c_cmp > _vmatrix_profile[off_diag]) {
        _vmatrix_profile[off_diag] = c_cmp;
        _vprofile_index[off_diag] = offset + 1;
      }
    }
  }
}

Mpx::~Mpx() {
  // free arrays
  free(this->_vww);
  free(this->_vddg);
  free(this->_vddf);
  // free(this->_vsig);
  // free(this->_vmmu);
  free(this->_vprofile_index);
  free(this->_vmatrix_profile);
  this->_vdata = NULL;

  // free debug arrays
  free(this->_movsum);
  free(this->_mov2sum);
}
#endif // USE_STL

} // namespace MatrixProfile
