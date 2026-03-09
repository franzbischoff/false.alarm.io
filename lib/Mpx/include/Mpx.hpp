#ifndef Mpx_h
#define Mpx_h

#if defined(ESP_PLATFORM)
// ESP-IDF framework (espressif32)
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <esp_log.h>
#define LOG_DEBUG(tag, format, ...) ESP_LOGD(tag, format, ##__VA_ARGS__)
#else
// Generic desktop/native build
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#ifdef NDEBUG
#define LOG_DEBUG(tag, format, ...) (void)0 // No-op in release mode
#else
#define LOG_DEBUG(tag, format, ...) std::printf("[%s] " format "\n", tag, ##__VA_ARGS__)
#endif
#endif

// uint16_t = 0 to 65535
// int16_t = -32768 to +32767

namespace MatrixProfile {
class Mpx {
public:
  // ppcheck-suppress noExplicitConstructor
  // Initialize MPX state and pre-allocate fixed buffers for streaming processing.
  Mpx(uint16_t window_size, float ez = 0.5F, uint16_t time_constraint = 0U, uint16_t buffer_size = 5000U);
  ~Mpx(); // destructor

  // Ingest new samples and update matrix profile state; returns remaining buffer capacity.
  [[nodiscard]] uint16_t compute(const float *data, uint16_t size);
  // Reinitialize internal signal buffer and derived vectors.
  void prune_buffer();
  // Compute FLOSS normalized arc counts from the current matrix profile indexes.
  void floss();

  // Raw views over internal buffers (mutable and const overloads).
  [[nodiscard]] float *get_data_buffer() noexcept { return data_buffer_.get(); };
  [[nodiscard]] const float *get_data_buffer() const noexcept { return data_buffer_.get(); };
  [[nodiscard]] float *get_matrix() noexcept { return vmatrix_profile_.get(); };
  [[nodiscard]] const float *get_matrix() const noexcept { return vmatrix_profile_.get(); };
  [[nodiscard]] int16_t *get_indexes() noexcept { return vprofile_index_.get(); };
  [[nodiscard]] const int16_t *get_indexes() const noexcept { return vprofile_index_.get(); };
  [[nodiscard]] float *get_floss() noexcept { return floss_.get(); };
  [[nodiscard]] const float *get_floss() const noexcept { return floss_.get(); };
  [[nodiscard]] float *get_iac() noexcept { return iac_.get(); };
  [[nodiscard]] const float *get_iac() const noexcept { return iac_.get(); };
  [[nodiscard]] float *get_vmmu() noexcept { return vmmu_.get(); };
  [[nodiscard]] const float *get_vmmu() const noexcept { return vmmu_.get(); };
  [[nodiscard]] float *get_vsig() noexcept { return vsig_.get(); };
  [[nodiscard]] const float *get_vsig() const noexcept { return vsig_.get(); };
  [[nodiscard]] float *get_ddf() noexcept { return vddf_.get(); };
  [[nodiscard]] const float *get_ddf() const noexcept { return vddf_.get(); };
  [[nodiscard]] float *get_ddg() noexcept { return vddg_.get(); };
  [[nodiscard]] const float *get_ddg() const noexcept { return vddg_.get(); };
  [[nodiscard]] float *get_vww() noexcept { return vww_.get(); };
  [[nodiscard]] const float *get_vww() const noexcept { return vww_.get(); };

  // Lightweight scalar state accessors.
  [[nodiscard]] uint16_t get_buffer_used() const noexcept { return buffer_used_; };
  [[nodiscard]] int16_t get_buffer_start() const noexcept { return buffer_start_; };
  [[nodiscard]] uint16_t get_profile_len() const noexcept { return profile_len_; };
  [[nodiscard]] float get_last_movsum() const noexcept { return last_accum_ + last_resid_; };
  [[nodiscard]] float get_last_mov2sum() const noexcept { return last_accum2_ + last_resid2_; };

private:
  bool new_data_(const float *data, uint16_t size);
  void floss_iac_();
  void movmean_();
  void movsig_();
  void muinvn_(uint16_t size = 0U);
  void mp_next_(uint16_t size = 0U);
  void ddf_(uint16_t size = 0U);
  void ddg_(uint16_t size = 0U);
  void ww_s_();

  const uint16_t window_size_;
  const float ez_;
  const uint16_t time_constraint_;
  const uint16_t buffer_size_;
  uint16_t buffer_used_ = 0U;
  int16_t buffer_start_ = 0;

  uint16_t profile_len_;
  uint16_t range_; // profile length - 1

  uint16_t exclusion_zone_;

  float last_accum_ = 0.0F;
  float last_resid_ = 0.0F;
  float last_accum2_ = 0.0F;
  float last_resid2_ = 0.0F;

  // arrays
  std::unique_ptr<float[]> data_buffer_;
  std::unique_ptr<float[]> vmatrix_profile_;
  std::unique_ptr<int16_t[]> vprofile_index_;
  std::unique_ptr<float[]> floss_;
  std::unique_ptr<float[]> iac_;
  std::unique_ptr<float[]> vmmu_;
  std::unique_ptr<float[]> vsig_;
  std::unique_ptr<float[]> vddf_;
  std::unique_ptr<float[]> vddg_;
  std::unique_ptr<float[]> vww_;
};

} // namespace MatrixProfile
#endif // Mpx_h
