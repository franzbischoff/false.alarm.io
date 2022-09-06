// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "Mpx.hpp"

#include <cmath>
#include <cstring>

namespace MatrixProfile {
Mpx::Mpx(uint16_t window_size, float ez, uint16_t time_constraint, uint16_t buffer_size)
    : _window_size(window_size), _ez(ez), _time_constraint(time_constraint), _buffer_size(buffer_size),
      _profile_len(buffer_size - _window_size + 1), _range(_profile_len - 1),
      _exclusion_zone(roundf(_window_size * _ez + __FLT_EPSILON__) + 1),
      _vmatrix_profile((float *)calloc(_profile_len + 1, sizeof(float))),
      _data_buffer((float *)calloc(_buffer_size + 1, sizeof(float))),
      _vprofile_index((int16_t *)calloc(_profile_len + 1, sizeof(int16_t))),
      _vmmu((float *)calloc(_profile_len + 1, sizeof(float))), _vsig((float *)calloc(_profile_len + 1, sizeof(float))),
      _vddf((float *)calloc(_profile_len + 1, sizeof(float))), _vddg((float *)calloc(_profile_len + 1, sizeof(float))),
      _vww((float *)calloc(_window_size + 1, sizeof(float))) {

  // change the default value to -1
  memset(_vmatrix_profile, -1, _profile_len);
  memset(_vprofile_index, -1, _profile_len);
}

void Mpx::movmean() {

  float accum = this->_data_buffer[0];
  float resid = 0.0;
  float movsum = 0.0;

  for (uint16_t i = 1; i < this->_window_size; i++) {
    float const m = this->_data_buffer[i];
    float const p = accum;
    accum = accum + m;
    float const q = accum - p;
    resid = resid + ((p - (accum - q)) + (m - q));
  }

  if (resid > 0.001) {
    printf("movsum: Residual value is large. Some precision may be lost. res = %.2f\n", resid);
  }

  movsum = accum + resid;
  this->_vmmu[0] = (float)(movsum / (float)this->_window_size);

  for (uint16_t i = this->_window_size; i < this->_buffer_size; i++) {
    float const m = this->_data_buffer[i - this->_window_size];
    float const n = this->_data_buffer[i];
    float const p = accum - m;
    float const q = p - accum;
    float const r = resid + ((accum - (p - q)) - (m + q));
    accum = p + n;
    float const t = accum - p;
    resid = r + ((p - (accum - t)) + (n - t));
    movsum = accum + resid;
    this->_vmmu[i - this->_window_size + 1] = (float)(movsum / (float)this->_window_size);
  }

  this->_last_movsum = movsum;
}

void Mpx::movsig() {

  /* double */ float accum = this->_data_buffer[0] * this->_data_buffer[0];
  /* double */ float resid = 0.0;
  float mov2sum = 0.0;

  for (uint16_t i = 1; i < this->_window_size; i++) {
    /* double */ float const m = this->_data_buffer[i] * this->_data_buffer[i];
    /* double */ float const p = accum;
    accum = accum + m;
    /* double */ float const q = accum - p;
    resid = resid + ((p - (accum - q)) + (m - q));
  }

  if (resid > 0.001) {
    printf("mov2sum: Residual value is large. Some precision may be lost. res = %.2f\n", resid);
  }

  mov2sum = accum + resid;
  this->_vsig[0] = 1 / sqrtf(mov2sum - this->_vmmu[0] * this->_vmmu[0] * this->_window_size);

  for (uint16_t i = this->_window_size; i < this->_buffer_size; i++) {
    float const m = this->_data_buffer[i - this->_window_size] * this->_data_buffer[i - this->_window_size];
    float const n = this->_data_buffer[i] * this->_data_buffer[i];
    float const p = accum - m;
    float const q = p - accum;
    float const r = resid + ((accum - (p - q)) - (m + q));
    accum = p + n;
    float const t = accum - p;
    resid = r + ((p - (accum - t)) + (n - t));
    mov2sum = accum + resid;
    this->_vsig[i - this->_window_size + 1] =
        1 / sqrtf(mov2sum - this->_vmmu[i - this->_window_size + 1] * this->_vmmu[i - this->_window_size + 1] *
                                this->_window_size);
  }

  this->_last_mov2sum = mov2sum;
}

void Mpx::muinvn() {
  movmean();
  movsig();
}

void Mpx::new_data(const float *data, uint16_t size) {

  for (uint16_t i = 0; i < (_buffer_size - size); i++) {
    this->_data_buffer[i] = this->_data_buffer[size + i];
  }

  for (uint16_t i = 0; i < size; i++) {
    this->_data_buffer[(_buffer_size - size + i)] = data[i];
  }
}

void Mpx::muinvn_next(uint16_t size) {

  uint16_t j = this->_profile_len - size;

  // update 1 step
  for (uint16_t i = 0; i < j; i++) {
    _vmmu[i] = _vmmu[i + size];
    _vsig[i] = _vsig[i + size];
  }

  // compute new mmu sig
  float accum = this->_last_movsum;
  float accum2 = this->_last_mov2sum;

  for (uint16_t i = j; i < _profile_len; i++) {
    float const m = _data_buffer[i - 1];
    float const n = _data_buffer[i - 1 + _window_size];
    accum2 = accum2 - m * m + n * n;
    accum = accum - m + n;
    _vmmu[i] = (float)(accum / (float)_window_size);
    _vsig[i] = 1 / sqrtf(accum2 - _vmmu[i] * _vmmu[i] * _window_size);
  }
}

void Mpx::ddf_next(uint16_t size) {
  // differentials have 0 as their first entry. This simplifies index
  // calculations slightly and allows us to avoid special "first line"
  // handling.

  uint16_t j = _range - size;

  // update 1 step
  for (uint16_t i = 0; i < j; i++) {
    this->_vddf[i] = this->_vddf[i + size];
  }

  // compute new ddf
  for (uint16_t i = j; i < _range; i++) {
    this->_vddf[i] = 0.5 * (this->_data_buffer[i] - this->_data_buffer[i + this->_window_size]);
  }

  // DEBUG: this should already be zero
  this->_vddf[_range] = 0.0;
}

void Mpx::ddg_next(uint16_t size) {
  // ddg: (data[(w+1):data_len] - mov_avg[2:(data_len - w + 1)]) + (data[1:(data_len - w)] - mov_avg[1:(data_len -
  // w)]) (subtract the mov_mean of all data, but the first window) + (subtract the mov_mean of all data, but the last
  // window)

  uint16_t j = _range - size;

  // update 1 step
  for (uint16_t i = 0; i < j; i++) {
    this->_vddg[i] = this->_vddg[i + size];
  }

  // compute new ddg
  for (uint16_t i = j; i < _range; i++) {
    this->_vddg[i] =
        (this->_data_buffer[i + this->_window_size] - this->_vmmu[i + 1]) + (this->_data_buffer[i] - this->_vmmu[i]);
  }

  this->_vddg[_range] = 0.0;
}

void Mpx::ddf_s() {
  // differentials have 0 as their first entry. This simplifies index
  // calculations slightly and allows us to avoid special "first line"
  // handling.

  for (uint16_t i = 0; i < _range; i++) {
    this->_vddf[i] = 0.5 * (this->_data_buffer[i] - this->_data_buffer[i + this->_window_size]);
  }

  this->_vddf[_range] = 0.0;
}

void Mpx::ddg_s() {
  // ddg: (data[(w+1):data_len] - mov_avg[2:(data_len - w + 1)]) + (data[1:(data_len - w)] - mov_avg[1:(data_len -
  // w)]) (subtract the mov_mean of all data, but the first window) + (subtract the mov_mean of all data, but the last
  // window)

  for (uint16_t i = 0; i < _range; i++) {
    this->_vddg[i] =
        (this->_data_buffer[i + this->_window_size] - this->_vmmu[i + 1]) + (this->_data_buffer[i] - this->_vmmu[i]);
  }

  this->_vddg[_range] = 0.0;
}

void Mpx::ww_s() {
  for (uint16_t i = 0; i < _window_size; i++) {
    this->_vww[i] = (this->_data_buffer[_range + i] - this->_vmmu[_range]);
  }
}

void Mpx::compute_stream() {                                     // uint16_t new_data_len) {
  uint16_t const new_data_len = _buffer_size - _window_size * 2; // incoming data

  bool initial = (new_data_len == this->_buffer_size);
  uint16_t off_min;

  // if (_time_constraint > 0) {
  //   diag_start = _buffer_size - _time_constraint - _window_size;
  // }

  uint16_t diag_start = 0;
  uint16_t diag_end = this->_profile_len - this->_exclusion_zone;

  muinvn();
  ddf_s();
  ddg_s();
  ww_s();

  for (uint16_t i = diag_start; i < diag_end; i++) {
    // this mess is just the inner_product but _data_buffer needs to be minus _vmmu[i] before multiply

    if (!(i % 1000)) {
      printf("%u\n", i);
    }

    float c = 0.0;

    // inner product demeaned
    for (uint16_t j = 0; j < _window_size; j++) {
      c += (_data_buffer[i + j] - _vmmu[i]) * _vww[j];
    }

    if (initial) {
      off_min = _range - i - 1;
    } else {
      off_min = MAX(_range - new_data_len, _range - i - 1);
    }
    uint16_t const off_start = _range;

    // loop if more than 1 new data
    if(off_start <= off_min) {
      printf("Unnecessary loop\n");
    }

    for (uint16_t offset = off_start; offset > off_min; offset--) {
      // min is offset + diag; max is (profile_len - 1); each iteration has the size of off_max
      uint16_t const off_diag = offset - (_range - i);

      c += _vddf[offset] * _vddg[off_diag] + _vddf[off_diag] * _vddg[offset];
      float const c_cmp = c * _vsig[offset] * _vsig[off_diag];

      // RMP
      if (c_cmp > _vmatrix_profile[off_diag]) {
        _vmatrix_profile[off_diag] = c_cmp;
        _vprofile_index[off_diag] = offset + 1;
      }
    }
  }
}

void Mpx::compute_next(float *data, uint16_t size) { // uint16_t new_data_len) {
  uint16_t const new_data_len = size;                // incoming data

  bool initial = (new_data_len == this->_buffer_size);
  uint16_t off_min;

  // if (_time_constraint > 0) {
  //   diag_start = _buffer_size - _time_constraint - _window_size;
  // }

  uint16_t diag_start = 0;
  uint16_t diag_end = this->_profile_len - this->_exclusion_zone;

  new_data(data, size); // store new data on buffer
  muinvn_next(size); // compute next mean and sig
  ddf_next(size); // compute next ddf
  ddg_next(size); // compute next ddg
  ww_s(); // recompute ww

  for (uint16_t i = diag_start; i < diag_end; i++) {
    // this mess is just the inner_product but _data_buffer needs to be minus _vmmu[i] before multiply

    if (!(i % 1000)) {
      printf("%u\n", i);
    }

    float c = 0.0;

    // inner product demeaned
    for (uint16_t j = 0; j < _window_size; j++) {
      c += (_data_buffer[i + j] - _vmmu[i]) * _vww[j];
    }

    if (initial) {
      off_min = _range - i - 1;
    } else {
      off_min = MAX(_range - new_data_len, _range - i - 1);
    }
    uint16_t const off_start = _range;

    // loop if more than 1 new data
    for (uint16_t offset = off_start; offset > off_min; offset--) {
      // min is offset + diag; max is (profile_len - 1); each iteration has the size of off_max
      uint16_t const off_diag = offset - (_range - i);

      c += _vddf[offset] * _vddg[off_diag] + _vddf[off_diag] * _vddg[offset];
      float const c_cmp = c * _vsig[offset] * _vsig[off_diag];

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
  free(this->_vsig);
  free(this->_vmmu);
  free(this->_vprofile_index);
  free(this->_vmatrix_profile);
  free(this->_data_buffer);

  // free debug arrays
  // free(this->_movmean);
  // free(this->_movsig);
}

} // namespace MatrixProfile
