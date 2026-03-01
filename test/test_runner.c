#include <unity.h>

void test_mpx_constructor_initial_state(void);
void test_mpx_prune_buffer_invariants(void);
void test_mpx_compute_and_floss_produce_valid_output(void);

void setUp(void) {
  // set stuff up here
}

void tearDown(void) {
  // clean stuff up here
}

static void run_all_tests(void) {
  UNITY_BEGIN();

  RUN_TEST(test_mpx_constructor_initial_state);
  RUN_TEST(test_mpx_prune_buffer_invariants);
  RUN_TEST(test_mpx_compute_and_floss_produce_valid_output);

  UNITY_END();
}

#if defined(ESP_PLATFORM)
void app_main() { run_all_tests(); }
#else
int main(void) {
  run_all_tests();
  return 0;
}
#endif
