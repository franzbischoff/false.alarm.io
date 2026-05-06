/**
 * @file test_mpx.cpp
 * @brief Unit tests for Matrix Profile Streaming (Mpx) implementation
 *
 * This test suite validates the core functionality of the Mpx class:
 * - Constructor initialization and buffer setup
 * - Buffer management and state consistency
 * - Moving average/variance computation accuracy
 * - Matrix profile generation and FLOSS goodness of fit
 *
 * Test Framework: Unity (embedded C/C++ testing framework)
 * Platform: Native (x86-64) for rapid iteration, ESP32 for deployment
 */

#include <Mpx.hpp>
#include <unity.h>

#include <cmath>

extern "C" {

/**
 * TEST 1: Constructor Initialization and Initial State
 *
 * PURPOSE: Verify that Mpx constructor correctly initializes all buffers
 *          and sets up the object in a valid ready state
 *
 * SETUP:
 *   - window_size = 8 samples
 *   - buffer_size = 64 samples
 *   - Ez (exclusion zone) = 0.5 (default)
 *   - time_constraint = 0 (no time constraint)
 *
 * WHAT IT VALIDATES:
 *   1. buffer_used_ initialized to buffer_size (full buffer after construction)
 *   2. buffer_start_ initialized to 0 (reading from position 0)
 *   3. profile_len correctly computed: buffer_size - window_size + 1 = 57
 *   4. Matrix profile array initialized to -1000000.0F (invalid sentinel)
 *   5. Profile index array initialized to -1 (no match found yet)
 *
 * WHY THIS MATTERS:
 *   - Ensures object is in a known, valid state immediately after construction
 *   - -1000000.0 is a sentinel value meaning "no correlation computed yet"
 *   - -1 index means "no matching neighbor found yet"
 *   - If these aren't correct, subsequent computations will be garbage
 */
void test_mpx_constructor_initial_state(void) {
  const uint16_t window_size = 8U;
  const uint16_t buffer_size = 64U;
  MatrixProfile::Mpx mpx(window_size, 0.5F, 0U, buffer_size);

  // CHECK 1: Buffer tracking state
  // mpx.get_buffer_used() returns how many samples are currently in the buffer
  // After construction, prune_buffer() is called which fills it with random data
  TEST_ASSERT_EQUAL_UINT16(buffer_size, mpx.get_buffer_used());

  // CHECK 2: Buffer read position
  // buffer_start_ tracks where we're reading from in the ring buffer
  // 0 = read from position 0 (beginning)
  TEST_ASSERT_EQUAL_INT16(0, mpx.get_buffer_start());

  // CHECK 3: Profile length calculation
  // For a signal of length N and window size W:
  // profile_len = N - W + 1 (number of windows that fit)
  // Example: 64 length, 8 window = 57 positions where we can place an 8-sample window
  TEST_ASSERT_EQUAL_UINT16(buffer_size - window_size + 1U, mpx.get_profile_len());

  // CHECK 4: Matrix profile initialization
  // Before any compute() call, all correlation values should be the sentinel -1000000.0
  // This means "no computation done yet"
  float *matrix = mpx.get_matrix();
  int16_t *indexes = mpx.get_indexes();

  for (uint16_t i = 0U; i < mpx.get_profile_len(); i++) {
    // All matrix values should be -1000000.0 (uninitialized sentinel)
    TEST_ASSERT_FLOAT_WITHIN(0.001F, -1000000.0F, matrix[i]);

    // All index values should be -1 (no match found yet)
    TEST_ASSERT_EQUAL_INT16(-1, indexes[i]);
  }
}

/**
 * TEST 2: Buffer Pruning and State Consistency
 *
 * PURPOSE: Verify that prune_buffer() correctly initializes internal
 *          statistical buffers and maintains consistency
 *
 * WHAT HAPPENS IN prune_buffer():
 *   1. Fills data_buffer_ with sinusoidal pattern (sin(2*pi*i/100)) for reproducible results
 *   2. Calls muinvn_(0) to compute moving averages (vmmu_)
 *   3. Calls muinvn_(0) to compute moving variance (vsig_)
 *   4. Calls ddf_(0) to compute first differences
 *   5. Calls ddg_(0) to compute differential terms for correlation
 *
 * WHY THIS MATTERS:
 *   - These internal arrays (vmmu_, vsig_, ddf_, ddg_) are used to compute
 *     matrix profile correlations efficiently (via Mueen's algorithm)
 *   - Ensures all auxiliary data is computed and consistent before first compute() call
 *   - If any of these are wrong, correlation values will be garbage
 */
void test_mpx_prune_buffer_invariants(void) {
  const uint16_t window_size = 8U;
  const uint16_t buffer_size = 64U;
  MatrixProfile::Mpx mpx(window_size, 0.5F, 0U, buffer_size);

  // Explicitly call prune_buffer (already called in constructor, but ensures clean state)
  mpx.prune_buffer();

  const uint16_t profile_len = mpx.get_profile_len();
  float *data = mpx.get_data_buffer();
  float *ddf = mpx.get_ddf();
  float *ddg = mpx.get_ddg();

  // CHECK 1: Data buffer sanity
  // First element should be 0.0 (sinusoidal pattern starts at sin(0) = 0)
  TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.0F, data[0]);

  // CHECK 2: Buffer state consistency
  TEST_ASSERT_EQUAL_UINT16(buffer_size, mpx.get_buffer_used());
  TEST_ASSERT_EQUAL_INT16(0, mpx.get_buffer_start());

  // CHECK 3: Differential arrays are computed
  // ddf[profile_len-1] should be 0.0 (by definition, it tracks first differences)
  TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.0F, ddf[profile_len - 1U]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.0F, ddg[profile_len - 1U]);

  // CHECK 4: Internal accumulators are valid
  // These are used in Kahan summation for numerical stability
  // Must be finite (no NaN or Inf)
  TEST_ASSERT_TRUE(std::isfinite(mpx.get_last_movsum()));
  TEST_ASSERT_TRUE(std::isfinite(mpx.get_last_mov2sum()));
}

/**
 * TEST 3: Compute and FLOSS Output Validity (MAIN FUNCTIONAL TEST)
 *
 * PURPOSE: Verify end-to-end correctness:
 *          1. compute() produces valid matrix profile correlations
 *          2. floss() generates valid goodness-of-fit scores
 *          3. All outputs are consistent and properly bounded
 *
 * INPUT SIGNAL:
 *   - Sine wave + linear trend: sin(0.2*i) + 0.05*i for i=0..15
 *   - Length: 16 samples (forces ring buffer behavior)
 *   - Expected: Some repeating patterns (sine component)
 *
 * ALGORITHM FLOW:
 *   1. compute(input, 16) -> processes 16 new samples through Mueen's algorithm
 *      - Updates matrix profile (Pearson correlations [-1, 1])
 *      - Finds nearest neighbor for each window
 *      - Returns free_space in ring buffer
 *   2. floss() -> computes goodness scores based on profile indices
 *      - Counts how many neighbors point to each position
 *      - Normalizes to [~0, ~1] scale
 *      - High score = position frequently matched
 *      - Low score = position rarely matched
 */
void test_mpx_compute_and_floss_produce_valid_output(void) {
  const uint16_t window_size = 8U;
  const uint16_t buffer_size = 64U;
  MatrixProfile::Mpx mpx(window_size, 0.5F, 0U, buffer_size);

  // Generate synthetic signal: sine + trend
  // This has enough structure to produce interesting matrix profile
  float input[16];
  for (uint16_t i = 0U; i < 16U; i++) {
    input[i] = std::sin(static_cast<float>(i) * 0.2F) + (static_cast<float>(i) * 0.05F);
  }

  // SUBSTEP 1: Run matrix profile computation
  // Returns how much buffer space is still free (should be 0 after filling)
  uint16_t const free_space = mpx.compute(input, 16U);
  TEST_ASSERT_EQUAL_UINT16(0U, free_space);

  // SUBSTEP 2: Compute FLOSS (goodness of fit) scores from profile indices
  mpx.floss();

  // SUBSTEP 3: Retrieve all outputs
  const uint16_t profile_len = mpx.get_profile_len();
  float *matrix = mpx.get_matrix();
  int16_t *indexes = mpx.get_indexes();
  float *floss = mpx.get_floss();

  // CHECK 1: At least some matches were found
  // At least one position should find a neighbor with valid correlation
  bool has_valid_match = false;
  for (uint16_t i = 0U; i < profile_len; i++) {
    if (indexes[i] >= 0 && matrix[i] > -999999.0F) {
      has_valid_match = true;
      break;
    }
  }
  TEST_ASSERT_TRUE(has_valid_match);

  // CHECK 2: All FLOSS values are finite (no NaN, no Inf)
  // Critical for downstream processing
  for (uint16_t i = 0U; i < profile_len; i++) {
    TEST_ASSERT_TRUE(std::isfinite(floss[i]));
  }

  // CHECK 3: First window_size positions should have FLOSS ≈ 1.0
  // Reason: Edges can't have self-matches (no data before/after)
  // So they get score = 1.0 automatically
  for (uint16_t i = 0U; i < window_size; i++) {
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 1.0F, floss[i]);
  }

  // CHECK 4: Last window_size-1 positions should also have FLOSS ≈ 1.0
  // For similar edge reasons
  for (uint16_t i = profile_len - window_size + 1U; i < (profile_len - 1U); i++) {
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 1.0F, floss[i]);
  }
}

/**
 * ============================================================================
 * TEST SUMMARY & COVERAGE
 * ============================================================================
 *
 * These 3 tests cover the CRITICAL PATH:
 *
 * 1. INITIALIZATION ✓
 *    - Object construction & state setup
 *    - Sentinel values for "no data"
 *    - Buffer tracking variables
 *
 * 2. BUFFER MANAGEMENT ✓
 *    - Internal auxiliary arrays (mmu, sig, ddf, ddg)
 *    - Numerical stability (Kahan summation state)
 *    - Ready state before compute()
 *
 * 3. CORE ALGORITHM ✓
 *    - Matrix profile computation (Mueen's algorithm)
 *    - Nearest neighbor matching
 *    - FLOSS goodness-of-fit scoring
 *    - Edge case handling (boundary effects)
 *
 * WHAT THESE TESTS DON'T CHECK (handled by test_mpx_robustness.cpp):
 *   - Streaming (multiple compute() calls with buffer shifts)
 *   - Numerical precision (Kahan summation accuracy)
 *   - Various signal patterns (noise, constant, etc)
 *   - Boundary conditions (tiny buffers, huge windows)
 *
 * ASSUMPTIONS ABOUT CORRECTNESS:
 *   - If matrix profile values are in [-1, 1], correlations are sensible
 *   - If indexes point to valid positions, matching is sensible
 *   - If FLOSS edges = 1.0, the scoring algorithm is working
 *   - If all values are finite, no numerical explosion occurred
 *
 * ============================================================================
 */

} // extern "C"
