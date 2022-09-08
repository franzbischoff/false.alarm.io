// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "Mpx.hpp"

namespace MatrixProfile {
Mpx::Mpx(const uint16_t window_size, float ez, uint16_t time_constraint, const uint16_t buffer_size)
    : window_size_(window_size), ez_(ez), time_constraint_(time_constraint), buffer_size_(buffer_size),
      buffer_start_(buffer_size), profile_len_(buffer_size - window_size_ + 1), range_(profile_len_ - 1),
      exclusion_zone_(roundf(window_size_ * ez_ + __FLT_EPSILON__) + 1),
      data_buffer_((float *)calloc(buffer_size_ + 1, sizeof(float))),
      vmatrix_profile_((float *)calloc(profile_len_ + 1, sizeof(float))),
      vprofile_index_((int16_t *)calloc(profile_len_ + 1, sizeof(int16_t))),
      floss_((float *)calloc(profile_len_ + 1, sizeof(float))), vmmu_((float *)calloc(profile_len_ + 1, sizeof(float))),
      vsig_((float *)calloc(profile_len_ + 1, sizeof(float))), vddf_((float *)calloc(profile_len_ + 1, sizeof(float))),
      vddg_((float *)calloc(profile_len_ + 1, sizeof(float))), vww_((float *)calloc(window_size_ + 1, sizeof(float))) {

  // change the default value to 0

  for (uint16_t i = 0; i < profile_len_; i++) {
    vmatrix_profile_[i] = 0.0;
    vprofile_index_[i] = -1;
  }
}

void Mpx::movmean() {

  float accum = this->data_buffer_[buffer_start_];
  float resid = 0.0;
  float movsum = 0.0;

  for (uint16_t i = 1; i < this->window_size_; i++) {
    float const m = this->data_buffer_[buffer_start_ + i];
    float const p = accum;
    accum = accum + m;
    float const q = accum - p;
    resid = resid + ((p - (accum - q)) + (m - q));
  }

  if (resid > 0.001) {
    printf("movsum: Residual value is large. Some precision may be lost. res = %.2f\n", resid);
  }

  movsum = accum + resid;
  this->vmmu_[buffer_start_] = (float)(movsum / (float)this->window_size_);

  for (uint16_t i = (this->window_size_ + buffer_start_); i < this->buffer_size_; i++) {
    float const m = this->data_buffer_[i - this->window_size_];
    float const n = this->data_buffer_[i];
    float const p = accum - m;
    float const q = p - accum;
    float const r = resid + ((accum - (p - q)) - (m + q));
    accum = p + n;
    float const t = accum - p;
    resid = r + ((p - (accum - t)) + (n - t));
    movsum = accum + resid;
    this->vmmu_[i - this->window_size_ + 1] = (float)(movsum / (float)this->window_size_);
  }

  this->last_movsum_ = movsum;
}

void Mpx::movsig() {

  float accum = this->data_buffer_[buffer_start_] * this->data_buffer_[buffer_start_];
  float resid = 0.0;
  float mov2sum = 0.0;

  for (uint16_t i = 1; i < this->window_size_; i++) {
    float const m = this->data_buffer_[buffer_start_ + i] * this->data_buffer_[buffer_start_ + i];
    float const p = accum;
    accum = accum + m;
    float const q = accum - p;
    resid = resid + ((p - (accum - q)) + (m - q));
  }

  if (resid > 0.001) {
    printf("mov2sum: Residual value is large. Some precision may be lost. res = %.2f\n", resid);
  }

  mov2sum = accum + resid;
  this->vsig_[buffer_start_] =
      1 / sqrtf(mov2sum - this->vmmu_[buffer_start_] * this->vmmu_[buffer_start_] * this->window_size_);

  for (uint16_t i = (this->window_size_ + buffer_start_); i < this->buffer_size_; i++) {
    float const m = this->data_buffer_[i - this->window_size_] * this->data_buffer_[i - this->window_size_];
    float const n = this->data_buffer_[i] * this->data_buffer_[i];
    float const p = accum - m;
    float const q = p - accum;
    float const r = resid + ((accum - (p - q)) - (m + q));
    accum = p + n;
    float const t = accum - p;
    resid = r + ((p - (accum - t)) + (n - t));
    mov2sum = accum + resid;
    this->vsig_[i - this->window_size_ + 1] =
        1 / sqrtf(mov2sum - this->vmmu_[i - this->window_size_ + 1] * this->vmmu_[i - this->window_size_ + 1] *
                                this->window_size_);
  }

  this->last_mov2sum_ = mov2sum;
}

void Mpx::muinvn(uint16_t size) {

  if (size == 0) {
    movmean();
    movsig();
    return;
  }

  uint16_t j = this->profile_len_ - size;

  // update 1 step
  for (uint16_t i = 0; i < j; i++) {
    vmmu_[i] = vmmu_[i + size];
    vsig_[i] = vsig_[i + size];
  }

  // compute new mmu sig
  float accum = this->last_movsum_;
  float accum2 = this->last_mov2sum_;

  for (uint16_t i = j; i < profile_len_; i++) {
    float const m = data_buffer_[i - 1];
    float const n = data_buffer_[i - 1 + window_size_];
    accum2 = accum2 - m * m + n * n;
    accum = accum - m + n;
    vmmu_[i] = (float)(accum / (float)window_size_);
    vsig_[i] = 1 / sqrtf(accum2 - vmmu_[i] * vmmu_[i] * window_size_);
  }

  this->last_movsum_ = accum;
  this->last_mov2sum_ = accum2;
}

bool Mpx::new_data(const float *data, uint16_t size) {

  bool first = true;

  if ((2 * size) > buffer_size_) {
    printf("Data size is too large\n");
    return false;
  } else if (size < (2 * window_size_)) {
    printf("Data size is too small\n");
    return false;
  } else {
    if (buffer_start_ != buffer_size_ || buffer_used_ >= 0) {
      first = false;
      // we must shift data
      for (uint16_t i = 0; i < (buffer_size_ - size); i++) {
        this->data_buffer_[i] = this->data_buffer_[size + i];
      }
      // then copy
      for (uint16_t i = 0; i < size; i++) {
        this->data_buffer_[(buffer_size_ - size + i)] = data[i];
      }
    } else {
      // fresh start, buffer must be already filled with zeroes
      for (uint16_t i = 0; i < size; i++) {
        this->data_buffer_[(buffer_size_ - size + i)] = data[i];
      }
    }

    buffer_used_ += size;
    buffer_start_ -= size;

    if (buffer_used_ > buffer_size_) {
      buffer_used_ = buffer_size_;
    }

    if (buffer_start_ < 0) {
      buffer_start_ = 0;
    }
  }

  return first;
}

void Mpx::mp_next(uint16_t size) {

  uint16_t j = this->profile_len_ - size;

  // update 1 step
  for (uint16_t i = 0; i < j; i++) {
    vmatrix_profile_[i] = vmatrix_profile_[i + size];
    vprofile_index_[i] = vprofile_index_[i + size] - size; // the index must be reduced
  }

  for (uint16_t i = j; i < profile_len_; i++) {
    vmatrix_profile_[i] = 0.0;
    vprofile_index_[i] = -1;
  }
}

void Mpx::ddf(uint16_t size) {
  // differentials have 0 as their first entry. This simplifies index
  // calculations slightly and allows us to avoid special "first line"
  // handling.

  uint16_t start = buffer_start_;

  if (size > 0) {
    // shift data
    for (uint16_t i = buffer_start_; i < (range_ - size); i++) {
      this->vddf_[i] = this->vddf_[i + size];
    }

    start = (range_ - size);
  }

  for (uint16_t i = start; i < range_; i++) {
    this->vddf_[i] = 0.5 * (this->data_buffer_[i] - this->data_buffer_[i + this->window_size_]);
  }

  // DEBUG: this should already be zero
  this->vddf_[range_] = 0.0;
}

void Mpx::ddg(uint16_t size) {
  // ddg: (data[(w+1):data_len] - mov_avg[2:(data_len - w + 1)]) + (data[1:(data_len - w)] - mov_avg[1:(data_len -
  // w)]) (subtract the mov_mean of all data, but the first window) + (subtract the mov_mean of all data, but the last
  // window)

  uint16_t start = buffer_start_;

  if (size > 0) {
    // shift data
    for (uint16_t i = buffer_start_; i < (range_ - size); i++) {
      this->vddg_[i] = this->vddg_[i + size];
    }

    start = (range_ - size);
  }

  for (uint16_t i = start; i < range_; i++) {
    this->vddg_[i] =
        (this->data_buffer_[i + this->window_size_] - this->vmmu_[i + 1]) + (this->data_buffer_[i] - this->vmmu_[i]);
  }

  this->vddg_[range_] = 0.0;
}

void Mpx::ww_s() {
  for (uint16_t i = 0; i < window_size_; i++) {
    this->vww_[i] = (this->data_buffer_[range_ + i] - this->vmmu_[range_]);
  }
}

void Mpx::floss() {
  for (uint16_t i = 0; i < this->profile_len_; i++) {
    int16_t j = vprofile_index_[i];

    if (j < 0 || j >= this->profile_len_) {
      continue;
    }

    floss_[MIN(i, j)] += 1.0;
    floss_[MAX(i, j)] -= 1.0;
  }

  const float a = 1.939274;
  const float b = 1.69815;
  const float c = 4.035477;
  const float len = (float)this->profile_len_;
  const float x = 1.0 / len;
  float iac = 0.001;

  // cumsum
  for (uint16_t i = 0; i < this->range_; i++) {
    floss_[i + 1] += floss_[i];
    if (i < this->window_size_ || i > (this->profile_len_ - this->window_size_)) {
      floss_[i] = 1.0;
    } else {
      iac = a * b * powf(i * x, a - 1.0) * powf(1.0 - powf(i * x, a), b - 1.0) * len / c;
      floss_[i] = floss_[i] / iac;
    }
  }

  // x <- seq(0, 1, length.out = cac_size)
  //  mode <- 0.6311142 # best point to analyze the segment change
  //  iac <- a * b * x^(a - 1) * (1 - x^a)^(b - 1) * cac_size / 4.035477
}

uint16_t Mpx::compute(const float *data, uint16_t size) {

  bool first = new_data(data, size); // store new data on buffer

  if (first) {
    muinvn();
    ddf();
    ddg();
  } else {
    muinvn(size);  // compute next mean and sig
    ddf(size);     // compute next ddf
    ddg(size);     // compute next ddg
    mp_next(size); // shift MP
  }

  ww_s();

  uint16_t off_min;

  // if (time_constraint_ > 0) {
  //   diag_start = buffer_size_ - time_constraint_ - window_size_;
  // }

  uint16_t diag_start = buffer_start_;
  uint16_t diag_end = this->profile_len_ - this->exclusion_zone_;

  for (uint16_t i = diag_start; i < diag_end; i++) {
    // this mess is just the inner_product but data_buffer_ needs to be minus vmmu_[i] before multiply

    if (!(i % 1000)) {
      printf("%u\n", i);
    }

    float c = 0.0;

    // inner product demeaned
    for (uint16_t j = 0; j < window_size_; j++) {
      c += (data_buffer_[i + j] - vmmu_[i]) * vww_[j];
    }

    if (size == 0) {
      off_min = range_ - i - 1;
    } else {
      off_min = MAX(range_ - size, range_ - i - 1);
    }

    uint16_t const off_start = range_;

    for (uint16_t offset = off_start; offset > off_min; offset--) {
      // min is offset + diag; max is (profile_len - 1); each iteration has the size of off_max
      uint16_t const off_diag = offset - (range_ - i);

      c += vddf_[offset] * vddg_[off_diag] + vddf_[off_diag] * vddg_[offset];

      if ((vsig_[offset] > 60) || (vsig_[off_diag] > 60)) { // wild sig, misleading
        continue;
      }

      float const c_cmp = c * vsig_[offset] * vsig_[off_diag];

      // RMP
      if (c_cmp > vmatrix_profile_[off_diag]) {
        // printf("%f\n", c_cmp);
        vmatrix_profile_[off_diag] = c_cmp;
        vprofile_index_[off_diag] = offset + 1;
      }
    }
  }

  return (this->buffer_size_ - this->buffer_used_);
}

Mpx::~Mpx() {
  // free arrays
  free(this->vww_);
  free(this->vddg_);
  free(this->vddf_);
  free(this->vsig_);
  free(this->vmmu_);
  free(this->vprofile_index_);
  free(this->vmatrix_profile_);
  free(this->data_buffer_);
}

} // namespace MatrixProfile
