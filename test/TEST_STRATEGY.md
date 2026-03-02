# COMPARATIVE TESTING GUIDE: C++ Mpx vs Rcpp Reference

**Objective:** Use Rcpp implementation as golden standard to validate C++ backend

## KEY FUNCTIONS TO VALIDATE

### 1. MOVING AVERAGE (movmean_)
- **C++ Implementation:** Uses Kahan summation for numerical stability
- **Rcpp Reference:** muinvn_rcpp() computes mean vector
- **Validation:**
  - Compare output within numerical tolerance (1e-5)
  - Test with large values + small perturbations
  - Verify Kahan residual handling

### 2. MOVING SIGMA (movsig_)
- **C++ Implementation:** Computes 1/sqrt(variance) with Kahan summation
- **Rcpp Reference:** muinvn_rcpp() computes sigma (variance normalization)
- **Validation:**
  - Check sigma > 0 for normal variance
  - Verify sigma = -1 for constant/low-variance windows
  - Compare with naive reference implementation

### 3. INCREMENTAL UPDATE (muinvn_)
- **C++ Implementation:** Maintains running statistics across buffer shifts
- **Rcpp Reference:** mpxi_rcpp() updates mmu/ssig arrays
- **Validation:**
  - Overlap regions should match after shift
  - Accumulator/residual state should be consistent

### 4. MATRIX PROFILE COMPUTATION
- **C++ Implementation:** compute() -> mp_next_() with streaming
- **Rcpp Reference:** mpxiright_rcpp() / mpxileft_rcpp()
- **Validation:**
  - Profile indices should stay within bounds
  - Correlation values in [-1, 1]
  - No NaN/Inf values

### 5. FLOSS COMPUTATION
- **C++ Implementation:** floss() with random Monte Carlo
- **Rcpp Reference:** floss() [if available in wrapper]
- **Validation:**
  - Output is always finite
  - Endpoints = 1.0 (no other pattern to compare)

## NUMERICAL PRECISION STRATEGY

The implementations differ in platform/library:
- **C++ (ESP32):** Single-precision float (32-bit), esp_log, FreeRTOS
- **Rcpp (R):** Double-precision (64-bit), optimized linear algebra

### TOLERANCE LEVELS

1. **Direct value comparison** (e.g., moving average):
   - Relative tolerance: max(abs(val), 1e-5) * 1e-4
   - Absolute tolerance: 1e-6

2. **Correlation-like values** (matrix profile):
   - Absolute tolerance: 0.01 (1 percent of [-1, 1] range)

3. **Count-based metrics** (indices, FLOSS):
   - Exact match OR within ±1 position

## TEST WORKFLOW

For each test case:

### 1. SETUP
- Initialize C++ Mpx instance with known parameters
- Generate test signal (sine, step, trend, constant, noise)
- Manually compute reference values using naive algorithms

### 2. EXECUTE
- Feed signal to C++ compute()
- Extract outputs: mmu, sig, ddf, ddg, matrix_profile, floss

### 3. VALIDATE AGAINST REFERENCE
- Compare with naive manual calculation
- Check bounds, ranges, special values
- Verify numerical stability with Kahan checks

### 4. DOCUMENT FINDINGS
- Log any tolerances needed
- Note platform-specific precision issues
- Flag potential bugs or divergences

## HOW TO ADD Rcpp VALIDATION

Currently, C++ tests use NAIVE REFERENCE implementations.
To integrate Rcpp golden standard:

### 1. Create Rcpp harness (rcpp/validate.cpp):
- Load Rcpp functions
- Export function to compute reference results given input signal

### 2. Add test helper (test/test_helper_rcpp.cpp):
- Link to Rcpp harness
- Call reference functions, return NumericVector to C++

### 3. Modify test macros:
- TEST_AGAINST_RCPP(expected_rcpp, actual_cpp, tolerance)
- Automatically log discrepancies

### 4. Run comparison tests:
- platformio test -e esp32idf
- Review any tolerance misses

## CRITICAL BUGS TO CATCH

Based on code review of Mpx.cpp:

### 1. Buffer Ring
- data_buffer_ starts at buffer_start_ position
- new_data_() shifts old data out and brings new data in
- **BUG RISK:** Off-by-one in shift or boundary conditions
- **TEST:** Verify muinvn_() handles edge indices correctly

### 2. Kahan Summation
- last_accum_, last_resid_ maintain running sum state
- movsig_() and movsig_() use residuals
- **BUG RISK:** Residual not properly carried across iterations
- **TEST:** Large value + many tiny additions case

### 3. Profile Index Management
- vprofile_index_ tracks matched neighbor position
- mp_next_() decrements by buffer_size on shift
- **BUG RISK:** Index wraps to negative, not clamped to -1
- **TEST:** Verify indexes always -1 or positive

### 4. Exclusion Zone
- exclusion_zone = ceil(window_size * ez) + 1
- Prevents trivial matches (comparisons too close in time)
- **BUG RISK:** Zone size wrong, or not applied in mp computation
- **TEST:** Set ez = 0.9, verify zone is large

### 5. Division by Zero / Low Variance
- vsig_[i] = 1/sqrt(psig) where psig is variance
- If psig < epsilon, sigma = -1 (invalid marker)
- **BUG RISK:** Crashes on sqrt(0), or incorrect -1 detection
- **TEST:** Constant signal case

## GOLDEN DATA SETS

Recommended test inputs (with pre-computed expected outputs):

### DATASET A: SINE WAVE (amplitude=1, freq=0.1)
- **Properties:** Periodic, smooth, no edge discontinuities
- **Length:** 64 samples
- **Window:** 8 samples
- **Expected:** High self-similarity at periodic intervals

### DATASET B: STEP FUNCTION (amplitude=1, step at middle)
- **Properties:** Sharp transition, constant sections
- **Length:** 32 samples
- **Window:** 8 samples
- **Expected:** Different correlation before/after step

### DATASET C: CONSTANT SIGNAL (amplitude=5)
- **Properties:** All identical samples
- **Length:** 32 samples
- **Window:** 8 samples
- **Expected:** Matrix profile all ~1.0 (perfect correlation)

### DATASET D: LINEARLY INCREASING (0 to 1)
- **Properties:** Monotonic trend, no repeats
- **Length:** 48 samples
- **Window:** 8 samples
- **Expected:** Low correlation (no repeats), decreasing trend

### DATASET E: NOISE (uniform random, amplitude=1)
- **Properties:** No structure, high entropy
- **Length:** 32 samples
- **Window:** 8 samples
- **Expected:** Random profile values, low confidence

## LOG_DEBUG MACRO VALIDATION (Native Platform)

After updates to LOG_DEBUG conditional compilation:

### PLATFORM-SPECIFIC BEHAVIOR

#### 1. ESP-IDF (ESP32)
- LOG_DEBUG always expands to ESP_LOGD (uses esp_log.h framework)
- Output controlled by CONFIG_LOG_DEFAULT_LEVEL in sdkconfig
- No conditional compilation change needed

#### 2. Native (Desktop)

**BUILD TYPE = DEBUG:**
- LOG_DEBUG → `std::printf()` outputs messages
- Macro NDEBUG is NOT defined
- Messages printed to stdout during test runs
- Useful for debugging computation flow, intermediate values

**BUILD TYPE = RELEASE:**
- LOG_DEBUG → `(void)0` (no-op)
- Macro NDEBUG is automatically defined
- Zero runtime overhead
- All debug messages completely removed by compiler

### TEST VALIDATION APPROACH

#### A. DEBUG MODE VERIFICATION
- **Command:** `platformio test -e native` (build_type defaults to debug)
- **Expected:** LOG_DEBUG messages appear in test output
- **Validation:** Grep stdout for "[Mpx]" markers
- **Sample output:** `[Mpx] DEBUG: j >= this->profile_len_`

#### B. RELEASE MODE VERIFICATION
- **Command:** `platformio test -e native --release`
- **Expected:** NO LOG_DEBUG messages in output
- **Validation:** Compile-time check (compiler optimizes away calls)
- **Binary size:** RELEASE should be smaller (no printf code)

#### C. MACRO EXPANSION CHECKS
- Verify macro conditionals:
  - `#ifdef NDEBUG`: Release-only flag
  - `#else`: Debug mode flag
- Use preprocessor to validate expansion:
  ```
  platformio run -vvv | grep "LOG_DEBUG"
  ```

#### D. INTEGRATION TESTS
- Run full test suite in debug mode: captures all debug output
- Verify no performance degradation from printf() overhead
- Confirm critical test assertions still pass

### EXPECTED NDEBUG BEHAVIOR (GCC/Clang Standard)

- **Debug builds:** `-O0` or `-O2` without `-DNDEBUG`
- **Release builds:** `-O3` and automatically `-DNDEBUG`
- **PlatformIO native:** Follows platform standard conventions

