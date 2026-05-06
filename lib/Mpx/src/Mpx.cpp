// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "Mpx.hpp"

static const char TAG[] = "mpx";

namespace MatrixProfile {
Mpx::Mpx(const uint16_t window_size, float ez, uint16_t time_constraint, const uint16_t buffer_size)
    : window_size_(window_size), ez_(ez), time_constraint_(time_constraint), buffer_size_(buffer_size),
      buffer_start_(static_cast<int16_t>(buffer_size)), profile_len_(buffer_size - window_size_ + 1U),
      range_(profile_len_ - 1U),
      exclusion_zone_(
          static_cast<uint16_t>(roundf(static_cast<float>(window_size_) * ez_ + __FLT_EPSILON__) + 1.0F)), // -V2004
      data_buffer_(std::make_unique<float[]>(buffer_size_ + 1U)),
      vmatrix_profile_(std::make_unique<float[]>(profile_len_ + 1U)),
      vprofile_index_(std::make_unique<int16_t[]>(profile_len_ + 1U)),
      floss_(std::make_unique<float[]>(profile_len_ + 1U)), iac_(std::make_unique<float[]>(profile_len_ + 1U)),
      vmmu_(std::make_unique<float[]>(profile_len_ + 1U)), vsig_(std::make_unique<float[]>(profile_len_ + 1U)),
      vddf_(std::make_unique<float[]>(profile_len_ + 1U)), vddg_(std::make_unique<float[]>(profile_len_ + 1U)),
      vww_(std::make_unique<float[]>(window_size_ + 1U)) {

  // change the default value to 0

  if (vmatrix_profile_ && vprofile_index_) {
    for (uint16_t i = 0U; i < profile_len_; i++) {
      vmatrix_profile_[i] = -1000000.0F;
      vprofile_index_[i] = -1;
    }
  }

  this->floss_iac_();
  this->prune_buffer();
}

void Mpx::movmean_() {

  float accum = this->data_buffer_[buffer_start_];
  float resid = 0.0F;
  float movsum;

  for (uint16_t i = 1U; i < this->window_size_; i++) {
    float const m = this->data_buffer_[buffer_start_ + i];
    float const p = accum;
    accum = accum + m;
    float const q = accum - p;
    resid = resid + ((p - (accum - q)) + (m - q));
  }

  if (resid > 0.001F) {
    LOG_DEBUG(TAG, "movsum: Residual value is large. Some precision may be lost. res = %.2f", resid);
  }

  movsum = accum + resid;
  this->vmmu_[buffer_start_] = movsum / static_cast<float>(this->window_size_);

  for (uint16_t i = (this->window_size_ + buffer_start_); i < this->buffer_size_; i++) {
    float const m = this->data_buffer_[i - this->window_size_];
    float const n = this->data_buffer_[i];
    float const p = accum - m;
    float const q = p - accum;
    resid = resid + ((accum - (p - q)) - (m + q));

    accum = p + n;
    float const t = accum - p;
    resid = resid + ((p - (accum - t)) + (n - t));

    movsum = accum + resid;
    this->vmmu_[i - this->window_size_ + 1U] = movsum / static_cast<float>(this->window_size_);
  }

  this->last_accum_ = accum;
  this->last_resid_ = resid;
}

void Mpx::movsig_() {

  float accum = this->data_buffer_[buffer_start_] * this->data_buffer_[buffer_start_];
  float resid = 0.0F;
  float mov2sum;

  for (uint16_t i = 1U; i < this->window_size_; i++) {
    float const m = this->data_buffer_[buffer_start_ + i] * this->data_buffer_[buffer_start_ + i];
    float const p = accum;
    accum = accum + m;
    float const q = accum - p;
    resid = resid + ((p - (accum - q)) + (m - q));
  }

  if (resid > 0.001F) {
    LOG_DEBUG(TAG, "mov2sum: Residual value is large. Some precision may be lost. res = %.2f", resid);
  }

  mov2sum = accum + resid;
  float const psig =
      mov2sum - this->vmmu_[buffer_start_] * this->vmmu_[buffer_start_] * static_cast<float>(this->window_size_);

  // For sd > 1.19e-7; window 25 -> sig will be <= 1.68e+6 (psig >= 3.54e-13) and for window 350 -> sig will be
  // <= 4.5e+5 (psig >= 4.94e-12) For sd < 100; window 25 -> sig will be >= 0.002 (psig <= 25e4) and for window 350 ->
  // sig will be >= 0.0005 (psig <= 4e6)

  if (psig > __FLT_EPSILON__) {
    this->vsig_[buffer_start_] = 1.0F / sqrtf(psig);
  } else {
    LOG_DEBUG(TAG, "DEBUG: psig1 precision, %.3f", psig);
    this->vsig_[buffer_start_] = -1.0F;
  }

  for (uint16_t i = (this->window_size_ + buffer_start_); i < this->buffer_size_; i++) {
    float const m = this->data_buffer_[i - this->window_size_] * this->data_buffer_[i - this->window_size_];
    float const n = this->data_buffer_[i] * this->data_buffer_[i];
    float const p = accum - m;
    float const q = p - accum;
    resid = resid + ((accum - (p - q)) - (m + q));
    accum = p + n;
    float const t = accum - p;
    resid = resid + ((p - (accum - t)) + (n - t));
    mov2sum = accum + resid;
    float const ppsig = mov2sum - this->vmmu_[i - this->window_size_ + 1U] * this->vmmu_[i - this->window_size_ + 1U] *
                                      static_cast<float>(this->window_size_);

    if (ppsig > __FLT_EPSILON__) {
      this->vsig_[i - this->window_size_ + 1U] = 1.0F / sqrtf(ppsig);
    } else {
      LOG_DEBUG(TAG, "DEBUG: ppsig precision, %.3f", ppsig);
      this->vsig_[i - this->window_size_ + 1U] = -1.0F;
    }
  }

  this->last_accum2_ = accum;
  this->last_resid2_ = resid;
}

void Mpx::muinvn_(uint16_t size) {

  if (size == 0U) {
    movmean_();
    movsig_();
    return;
  }

  uint16_t const j = this->profile_len_ - size;

  // update 1 step - use memmove for optimized bulk copy
  std::memmove(vmmu_.get(), vmmu_.get() + size, j * sizeof(float));
  std::memmove(vsig_.get(), vsig_.get() + size, j * sizeof(float));

  // compute new mmu sig
  float accum = this->last_accum_;   // OLINT(misc-const-correctness) - this variable can't be const
  float accum2 = this->last_accum2_; // OLINT(misc-const-correctness) - this variable can't be const
  float resid = this->last_resid_;   // OLINT(misc-const-correctness) - this variable can't be const
  float resid2 = this->last_resid2_; // OLINT(misc-const-correctness) - this variable can't be const

  for (uint16_t i = j; i < profile_len_; i++) {
    /* mean */
    float m = data_buffer_[i - 1];
    float n = data_buffer_[i - 1 + window_size_];
    float p = accum - m;
    float q = p - accum;
    resid = resid + ((accum - (p - q)) - (m + q));

    accum = p + n;
    float t = accum - p;
    resid = resid + ((p - (accum - t)) + (n - t));
    vmmu_[i] = (accum + resid) / static_cast<float>(window_size_);

    /* sig */
    m = data_buffer_[i - 1] * data_buffer_[i - 1];
    n = data_buffer_[i - 1 + window_size_] * data_buffer_[i - 1 + window_size_];
    p = accum2 - m;
    q = p - accum2;
    resid2 = resid2 + ((accum2 - (p - q)) - (m + q));

    accum2 = p + n;
    t = accum2 - p;
    resid2 = resid2 + ((p - (accum2 - t)) + (n - t));

    float const psig = (accum2 + resid2) - vmmu_[i] * vmmu_[i] * static_cast<float>(window_size_);
    if (psig > __FLT_EPSILON__) {
      vsig_[i] = 1.0F / sqrtf(psig);
    } else {
      LOG_DEBUG(TAG, "DEBUG: psig precision, %.3f", psig);
      vsig_[i] = -1.0F;
    }
  }

  this->last_accum_ = accum;
  this->last_accum2_ = accum2;
  this->last_resid_ = resid;
  this->last_resid2_ = resid2;
}

bool Mpx::new_data_(const float *data, uint16_t size) {

  bool first = true;

  if ((2U * size) > buffer_size_) {
    LOG_DEBUG(TAG, "%s", "Data size is too large");
    return false;
  } else if (size < (window_size_) && buffer_used_ < window_size_) {
    LOG_DEBUG(TAG, "%s", "Data size is too small");
    return false;
  } else {
    if ((buffer_start_ != buffer_size_) || buffer_used_ > 0U) {
      first = false;
      // we must shift data - use memmove for optimized bulk copy
      std::memmove(this->data_buffer_.get(), this->data_buffer_.get() + size, (buffer_size_ - size) * sizeof(float));
      // then copy
      for (uint16_t i = 0U; i < size; i++) {
        this->data_buffer_[(buffer_size_ - size + i)] = data[i];
      }
    } else {
      // fresh start, buffer must be already filled with zeroes
      for (uint16_t i = 0U; i < size; i++) {
        this->data_buffer_[(buffer_size_ - size + i)] = data[i];
      }
    }

    buffer_used_ += size;
    buffer_start_ = static_cast<int16_t>(buffer_start_ - size);

    if (buffer_used_ > buffer_size_) {
      buffer_used_ = buffer_size_;
    }

    if (buffer_start_ < 0) {
      buffer_start_ = 0;
    }
  }

  return first;
}

void Mpx::mp_next_(uint16_t size) {

  uint16_t const j = this->profile_len_ - size;

  // update 1 step - use memmove for optimized bulk copy
  std::memmove(vmatrix_profile_.get(), vmatrix_profile_.get() + size, j * sizeof(float));
  std::memmove(vprofile_index_.get(), vprofile_index_.get() + size, j * sizeof(int16_t));

  // adjust indexes after shift
  for (uint16_t i = 0; i < j; i++) {
    vprofile_index_[i] = static_cast<int16_t>(vprofile_index_[i] - size);

    // avoid too negative values
    if (vprofile_index_[i] < -1) {
      vprofile_index_[i] = -1;
    }
  }

  for (uint16_t i = j; i < profile_len_; i++) {
    vmatrix_profile_[i] = -1000000.0F;
    vprofile_index_[i] = -1;
  }
}

void Mpx::ddf_(uint16_t size) {
  // differentials have 0 as their first entry. This simplifies index
  // calculations slightly and allows us to avoid special "first line"
  // handling.

  uint16_t start = buffer_start_;

  if (size > 0U) {
    // shift data - use memmove for optimized bulk copy
    std::memmove(this->vddf_.get() + buffer_start_, this->vddf_.get() + buffer_start_ + size,
                 (range_ - size - buffer_start_) * sizeof(float));

    start = (range_ - size);
  }

  for (uint16_t i = start; i < range_; i++) {
    this->vddf_[i] = 0.5F * (this->data_buffer_[i] - this->data_buffer_[i + this->window_size_]);
  }

  // DEBUG: this should already be zero
  this->vddf_[range_] = 0.0F;
}

void Mpx::ddg_(uint16_t size) {
  // ddg: (data[(w+1):data_len] - mov_avg[2:(data_len - w + 1)]) + (data[1:(data_len - w)] - mov_avg[1:(data_len -
  // w)]) (subtract the mov_mean of all data, but the first window) + (subtract the mov_mean of all data, but the last
  // window)

  uint16_t start = buffer_start_;

  if (size > 0U) {
    // shift data - use memmove for optimized bulk copy
    std::memmove(this->vddg_.get() + buffer_start_, this->vddg_.get() + buffer_start_ + size,
                 (range_ - size - buffer_start_) * sizeof(float));

    start = (range_ - size);
  }

  for (uint16_t i = start; i < range_; i++) {
    this->vddg_[i] =
        (this->data_buffer_[i + this->window_size_] - this->vmmu_[i + 1U]) + (this->data_buffer_[i] - this->vmmu_[i]);
  }

  this->vddg_[range_] = 0.0F;
}

void Mpx::ww_s_() {
  for (uint16_t i = 0U; i < window_size_; i++) {
    this->vww_[i] = (this->data_buffer_[range_ + i] - this->vmmu_[range_]);
  }
}

void Mpx::prune_buffer() {
  // prune buffer
  // data_buffer_[0] = 0.001F;

  // for (uint16_t i = 1U; i < buffer_size_; i++) {
  //   float mock = static_cast<float>((RAND() % 1000) - 500);
  //   mock /= 1000.0F;
  //   data_buffer_[i] = data_buffer_[i - 1] + mock;
  // }

  // prune buffer - Initialize with sinusoidal pattern for reproducible results
  // Period of 100 samples matches typical window_size
  const float period = 100.0F;
  const float two_pi = 2.0F * 3.14159265358979323846F; // M_PI replacement

  for (uint16_t i = 0U; i < buffer_size_; i++) {
    data_buffer_[i] = sinf(two_pi * static_cast<float>(i) / period);
  }

  buffer_used_ = buffer_size_;
  buffer_start_ = 0;
  muinvn_(0U);
  ddf_(0U);
  ddg_(0U);
}

/**
 * @brief Compute Ideal Arc Counts (IAC) using analytical Kumaraswamy distribution
 *
 * Generates the expected arc count distribution used to normalize FLOSS and
 * correct edge effects.
 *
 * @note Implementation approach:
 * - Uses the analytical Kumaraswamy distribution for ideal arc counts
 * - Produces deterministic output (no pseudo-random sampling noise)
 * - Keeps legacy Monte Carlo code below as a commented reference
 *
 * @note Difference from R reference (fluss.R):
 * R uses analytical Kumaraswamy distribution: a * b * x^(a-1) * (1 - x^a)^(b-1) * cac_size / 4.035477
 *   where a = 1.939274, b = 1.698150 (for mp_offset > 0, the streaming case)
 * C++ also uses the analytical form in this implementation.
 */
void Mpx::floss_iac_() {

  // uint16_t *mpi = nullptr;

  // mpi = static_cast<uint16_t*>(calloc(this->profile_len_ + 1U, sizeof(uint16_t)));

  // if (mpi == nullptr) {
  //   LOG_DEBUG(TAG, "Memory allocation failed");
  //   return;
  // }

  // for (uint16_t i = 0U; i < this->profile_len_; i++) {
  //   this->iac_[i] = 0.0F;
  // }

  // ========== KUMARASWAMY DISTRIBUTION (Analytical) ==========
  // Instead of Monte Carlo simulation, use the analytical Kumaraswamy distribution
  // which provides the theoretical ideal arc counts distribution
  const float a = 1.939274f;
  const float b = 1.698150f;
  const float cac_size = static_cast<float>(this->profile_len_);
  const float normalization = 4.035477f;

  for (uint16_t i = 0U; i < this->profile_len_; i++) {
    float x = static_cast<float>(i) / cac_size;

    // Kumaraswamy distribution formula:
    // iac = a * b * x^(a-1) * (1 - x^a)^(b-1) * cac_size / 4.035477
    float x_a_minus_1 = powf(x, a - 1.0f);
    float one_minus_x_a = 1.0f - powf(x, a);
    float one_minus_x_a_b_minus_1 = powf(one_minus_x_a, b - 1.0f);

    this->iac_[i] = a * b * x_a_minus_1 * one_minus_x_a_b_minus_1 * cac_size / normalization;
  }

  // ========== OLD MONTE CARLO IMPLEMENTATION (COMMENTED OUT) ==========
  // Previous approach using Monte Carlo simulation with random indices
  // Results should be very similar to the Kumaraswamy distribution above
  /*
  for (uint16_t k = 0U; k < 10; k++) { // repeat 10 times to smooth the result
    for (uint16_t i = 0U; i < (this->profile_len_ - this->exclusion_zone_ - 1); i++) {
      mpi[i] = (RAND() % (this->range_ - (i + this->exclusion_zone_))) + (i + this->exclusion_zone_);
    }

    for (uint16_t i = 0U; i < (this->profile_len_ - this->exclusion_zone_ - 1); i++) {
      uint16_t const j = mpi[i];

      if (j >= this->profile_len_) {
        LOG_DEBUG(TAG, "%s", "j >= this->profile_len_");
        continue;
      }
      // RMP, i is always < j
      this->iac_[i] += 0.1F;
      this->iac_[j] -= 0.1F;
    }
  }
  */
  // // cumsum
  // for (uint16_t i = 0U; i < this->range_; i++) {
  //   this->iac_[i + 1U] += this->iac_[i];
  // }

  // free(mpi); // No longer needed with Kumaraswamy distribution
}

/**
 * @brief FLOSS - Fast Low-cost Online Semantic Segmentation
 *
 * Computes corrected arc counts for semantic segmentation based on the matrix profile index.
 *
 * @note Implementation differences from R reference (fluss.R):
 *
 * 1. Arc Counting: This implementation assumes RMP (Right Matrix Profile) only, where i < j always.
 *    The R reference uses min(i,j) and max(i,j) to handle both LMP and RMP.
 *    Since this project only implements RMP, we can assume i < j and save CPU cycles.
 *
 * 2. Ideal Arc Counts: Uses analytical Kumaraswamy distribution in floss_iac_.
 *    Formula: a * b * x^(a-1) * (1 - x^a)^(b-1) * cac_size / 4.035477
 *    where a = 1.939274, b = 1.698150 (for streaming case with mp_offset > 0).
 *    A legacy Monte Carlo variant is kept commented for historical reference.
 *
 * 3. Edge Correction: Uses window_size for boundary detection instead of exclusion_zone.
 *    R: corrected_arc_counts[1:min(exclusion_zone, cac_size)] <- 1
 *    C++: if (i < window_size_ || i > (profile_len_ - window_size_))
 *    Behavior is similar for typical configurations where exclusion_zone ≈ window_size * ez.
 */
// ppcheck-suppress unusedFunction
void Mpx::floss() {

  for (uint16_t i = 0U; i < this->profile_len_; i++) {
    this->floss_[i] = 0.0F;
  }

  for (uint16_t i = 0U; i < (this->profile_len_ - this->exclusion_zone_ - 1); i++) {
    int16_t const j = vprofile_index_[i];

    if (j >= this->profile_len_) {
      LOG_DEBUG(TAG, "%s", "DEBUG: j >= this->profile_len_");
      continue;
    }

    if (j < 0) {
      if (j < -1) {
        LOG_DEBUG(TAG, "%s", "DEBUG: j < -1");
      }
      // LOG_DEBUG(TAG, "DEBUG: j < 0");
      // j = (rand() % (this->range_ - (i + this->exclusion_zone_))) + (i + this->exclusion_zone_);
      // vprofile_index_[i] = j;
      continue;
    }

    if (j < i) {
      LOG_DEBUG(TAG, "DEBUG: i = %d ; j = %d ", i, j);
    }
    // RMP, i is always < j
    this->floss_[i] += 1.0F;
    this->floss_[j] -= 1.0F;
  }

  // cumsum
  for (uint16_t i = 0U; i < this->range_; i++) {
    this->floss_[i + 1U] += this->floss_[i];
    if (i < this->window_size_ || i > (this->profile_len_ - this->window_size_)) {
      this->floss_[i] = 1.0F;
    } else {
      if (this->floss_[i] > this->iac_[i]) {
        this->floss_[i] = 1.0F;
      } else {
        this->floss_[i] /= this->iac_[i];
      }
    }
  }
}

// ppcheck-suppress unusedFunction
uint16_t Mpx::compute(const float *data, uint16_t size) {

  bool const first = new_data_(data, size); // store new data on buffer

  if (first) {
    muinvn_(0U);
    ddf_(0U);
    ddg_(0U);
  } else {
    muinvn_(size);  // compute next mean and sig
    ddf_(size);     // compute next ddf
    ddg_(size);     // compute next ddg
    mp_next_(size); // shift MP
  }

  ww_s_();

  // if (time_constraint_ > 0) {
  //   diag_start = buffer_size_ - time_constraint_ - window_size_;
  // }

  uint16_t const diag_start = buffer_start_;
  uint16_t const diag_end = this->profile_len_ - this->exclusion_zone_;

  uint32_t debug_wild_sig = 0U;

  for (uint16_t i = diag_start; i < diag_end; i++) {
    // this mess is just the inner_product but data_buffer_ needs to be minus vmmu_[i] before multiply

    float c = 0.0F;

    // inner product demeaned
    for (uint16_t j = 0U; j < window_size_; j++) {
      c += (data_buffer_[i + j] - vmmu_[i]) * vww_[j];
    }

    uint16_t off_min = 0U;

    if (first) {
      off_min = range_ - i - 1;
    } else {
      // ppcheck-suppress duplicateExpression
      off_min = std::max(range_ - size, range_ - i - 1); // -V501
    }

    uint16_t const off_start = range_;

    for (uint16_t offset = off_start; offset > off_min; offset--) {
      // min is offset + diag; max is (profile_len - 1); each iteration has the size of off_max
      uint16_t const off_diag = offset - (range_ - i);
      // 3586 = i, offset = 4900

      c += vddf_[offset] * vddg_[off_diag] + vddf_[off_diag] * vddg_[offset];

      if ((vsig_[offset] < 0.0F) || (vsig_[off_diag] < 0.0F)) { // wild sig, misleading
        debug_wild_sig++;
        continue;
      }

      float const c_cmp = c * vsig_[offset] * vsig_[off_diag];

      // RMP
      // min off_diag is 0; max off_diag is (diag_end-1) == (profile_len_ - exclusion_zone_ - 1)
      if (c_cmp > vmatrix_profile_[off_diag]) {
        // LOG_DEBUG(TAG, "%f", c_cmp);
        vmatrix_profile_[off_diag] = c_cmp;
        vprofile_index_[off_diag] = static_cast<int16_t>(offset); // + 1U);
      }
    }
  }

  if (debug_wild_sig > 0U) {
    LOG_DEBUG(TAG, "DEBUG: wild sig: %u", debug_wild_sig);
  }

  return (this->buffer_size_ - this->buffer_used_);
}

Mpx::~Mpx() {
  // std::unique_ptr automatically releases memory
}

} // namespace MatrixProfile
