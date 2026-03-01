#include <Mpx.hpp>
#include <unity.h>

#include <cmath>

extern "C" {

void test_mpx_constructor_initial_state(void) {
  const uint16_t window_size = 8U;
  const uint16_t buffer_size = 64U;
  MatrixProfile::Mpx mpx(window_size, 0.5F, 0U, buffer_size);

  TEST_ASSERT_EQUAL_UINT16(buffer_size, mpx.get_buffer_used());
  TEST_ASSERT_EQUAL_INT16(0, mpx.get_buffer_start());
  TEST_ASSERT_EQUAL_UINT16(buffer_size - window_size + 1U, mpx.get_profile_len());

  float *matrix = mpx.get_matrix();
  int16_t *indexes = mpx.get_indexes();

  for (uint16_t i = 0U; i < mpx.get_profile_len(); i++) {
    TEST_ASSERT_FLOAT_WITHIN(0.001F, -1000000.0F, matrix[i]);
    TEST_ASSERT_EQUAL_INT16(-1, indexes[i]);
  }
}

void test_mpx_prune_buffer_invariants(void) {
  const uint16_t window_size = 8U;
  const uint16_t buffer_size = 64U;
  MatrixProfile::Mpx mpx(window_size, 0.5F, 0U, buffer_size);

  mpx.prune_buffer();

  const uint16_t profile_len = mpx.get_profile_len();
  float *data = mpx.get_data_buffer();
  float *ddf = mpx.get_ddf();
  float *ddg = mpx.get_ddg();

  TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.001F, data[0]);
  TEST_ASSERT_EQUAL_UINT16(buffer_size, mpx.get_buffer_used());
  TEST_ASSERT_EQUAL_INT16(0, mpx.get_buffer_start());
  TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.0F, ddf[profile_len - 1U]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.0F, ddg[profile_len - 1U]);
  TEST_ASSERT_TRUE(std::isfinite(mpx.get_last_movsum()));
  TEST_ASSERT_TRUE(std::isfinite(mpx.get_last_mov2sum()));
}

void test_mpx_compute_and_floss_produce_valid_output(void) {
  const uint16_t window_size = 8U;
  const uint16_t buffer_size = 64U;
  MatrixProfile::Mpx mpx(window_size, 0.5F, 0U, buffer_size);

  float input[16];
  for (uint16_t i = 0U; i < 16U; i++) {
    input[i] = std::sin(static_cast<float>(i) * 0.2F) + (static_cast<float>(i) * 0.05F);
  }

  uint16_t const free_space = mpx.compute(input, 16U);
  TEST_ASSERT_EQUAL_UINT16(0U, free_space);

  mpx.floss();

  const uint16_t profile_len = mpx.get_profile_len();
  float *matrix = mpx.get_matrix();
  int16_t *indexes = mpx.get_indexes();
  float *floss = mpx.get_floss();

  bool has_valid_match = false;
  for (uint16_t i = 0U; i < profile_len; i++) {
    if (indexes[i] >= 0 && matrix[i] > -999999.0F) {
      has_valid_match = true;
      break;
    }
  }
  TEST_ASSERT_TRUE(has_valid_match);

  for (uint16_t i = 0U; i < profile_len; i++) {
    TEST_ASSERT_TRUE(std::isfinite(floss[i]));
  }

  for (uint16_t i = 0U; i < window_size; i++) {
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 1.0F, floss[i]);
  }

  for (uint16_t i = profile_len - window_size + 1U; i < (profile_len - 1U); i++) {
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 1.0F, floss[i]);
  }
}

} // extern "C"
