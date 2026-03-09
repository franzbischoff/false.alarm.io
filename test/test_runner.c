#include <unity.h>

#if defined(ESP_PLATFORM)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

#if defined(ESP_PLATFORM) && defined(USE_SD_CARD)
#include "sd_card_mount.h"
#endif

void test_mpx_constructor_initial_state(void);
void test_mpx_prune_buffer_invariants(void);
void test_mpx_compute_and_floss_produce_valid_output(void);

// Robustness test suite - validates Mpx against golden properties
void test_movmean_returns_finite_values(void);
void test_movsig_returns_valid_values(void);
void test_differential_arrays_are_finite(void);
void test_mp_profile_values_bounded(void);
void test_mp_indexes_valid_or_empty(void);
void test_floss_returns_finite_values(void);
void test_floss_endpoints_preserve_value_one(void);
void test_minimal_buffer_size(void);
void test_large_exclusion_zone(void);
void test_time_constraint_accepted(void);
void test_constant_signal_no_crash(void);
void test_mixed_pattern_signal(void);
void test_noise_signal_stability(void);
void test_sequential_compute_does_not_crash(void);
void test_floss_after_sequential_compute(void);

// Golden reference regression tests
void test_golden_reference_metadata(void);
void test_golden_reference_sample_validation(void);

void setUp(void) {
  // set stuff up here
}

void tearDown(void) {
  // clean stuff up here
}

static void run_all_tests(void) {
  UNITY_BEGIN();

  // Original sanity tests
  RUN_TEST(test_mpx_constructor_initial_state);
  RUN_TEST(test_mpx_prune_buffer_invariants);
  RUN_TEST(test_mpx_compute_and_floss_produce_valid_output);

  // Robustness tests: Numerical Stability
  RUN_TEST(test_movmean_returns_finite_values);
  RUN_TEST(test_movsig_returns_valid_values);
  RUN_TEST(test_differential_arrays_are_finite);

  // Robustness tests: Matrix Profile Invariants
  RUN_TEST(test_mp_profile_values_bounded);
  RUN_TEST(test_mp_indexes_valid_or_empty);

  // Robustness tests: FLOSS Output
  RUN_TEST(test_floss_returns_finite_values);
  RUN_TEST(test_floss_endpoints_preserve_value_one);

  // Robustness tests: Edge Cases
  RUN_TEST(test_minimal_buffer_size);
  RUN_TEST(test_large_exclusion_zone);
  RUN_TEST(test_time_constraint_accepted);

  // Robustness tests: Data Patterns
  RUN_TEST(test_constant_signal_no_crash);
  RUN_TEST(test_mixed_pattern_signal);
  RUN_TEST(test_noise_signal_stability);

  // Robustness tests: Sequential Processing
  RUN_TEST(test_sequential_compute_does_not_crash);
  RUN_TEST(test_floss_after_sequential_compute);

  // Golden Reference Regression Tests
  RUN_TEST(test_golden_reference_metadata);
  RUN_TEST(test_golden_reference_sample_validation);

  UNITY_END();
}

#if defined(ESP_PLATFORM)
static void unity_test_task(void *param) {
  (void)param;

#if defined(USE_SD_CARD)
  // Mount SD card for test file access
  esp_err_t ret = mount_sd_card_for_tests();
  if (ret != ESP_OK) {
    printf("ERROR: Failed to mount SD card. Cannot run tests.\n");
    vTaskDelete(NULL);
    return;
  }
#endif

  run_all_tests();

#if defined(USE_SD_CARD)
  // Unmount SD card after tests
  unmount_sd_card_for_tests();
#endif

  vTaskDelete(NULL);
}

void app_main() {
  // Run tests in a larger stack task to avoid overflow in filesystem-heavy golden tests.
  xTaskCreatePinnedToCore(unity_test_task, "unity_test", 32768, NULL, 5, NULL, 1);
}
#else
int main(void) {
  run_all_tests();
  return 0;
}
#endif
