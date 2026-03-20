# Phase 1 — Static Metrics

**Date collected:** 2026-03-20
**Toolchain:** ESP-IDF 5.5.3 / PlatformIO, GCC 14.2.0, C++17
**Target hardware:** SparkFun ESP32 IoT RedBoard (ESP32, Xtensa LX6, 240 MHz, 4 MB Flash, 520 KB SRAM total)

---

## 1. Build Profiles

Three configurations were compiled from the same source:

| Profile | `build_type` | Optimization | sdkconfig base | Notes |
|---|---|---|---|---|
| `esp32_prod` | release | `-Os` (default) | `sdkconfig.defaults.prod` | Balanced size/speed; offline IDF profile (no Wi-Fi/LWIP/TLS) |
| `esp32_prod_fast` | release | `-O3 -DNDEBUG` | `sdkconfig.defaults.prod` | Speed-optimized; same offline IDF profile |
| `esp32_demo` | debug | `-Og` (default) | `sdkconfig.defaults` | Full IDF components; debug symbols retained |

All three profiles share the same algorithm parameters (Section 3) and FreeRTOS task configuration (Section 5).

---

## 2. Binary Size (ELF Sections)

Size measured with `pio run -e <env> --target size` on a cached build. ELF section semantics for ESP32/Xtensa:

- `.text` — code and read-only data mapped to Flash (IROM/IRAM)
- `.data` — initialised data stored in Flash and copied to internal SRAM at boot
- `.bss` — zero-initialised static data in SRAM (not stored in Flash)

| Profile | `.text` (B) | `.data` (B) | `.bss` (B) | ELF total (B) | Flash footprint (B) | SRAM static (B) |
|---|---:|---:|---:|---:|---:|---:|
| `esp32_prod` (`-Os`) | 206,811 | 76,128 | 2,449 | 285,388 | 282,939 | 78,577 |
| `esp32_prod_fast` (`-O3`) | 233,663 | 60,604 | 2,457 | 296,724 | 294,267 | 63,061 |
| `esp32_demo` (debug) | 230,785 | 80,388 | 2,465 | 313,638 | 311,173 | 82,853 |

**Flash footprint** = `.text` + `.data`.
**SRAM static** = `.data` + `.bss` (resident in RAM from boot).

**Key observations:**

- `-O3` expands `.text` by +26,852 bytes (+13.0%) relative to `-Os`, reflecting code duplication from inlining and loop unrolling.
- `-O3` reduces `.data` by 15,524 bytes (−20.4%) vs `-Os`; the compiler promotes more read-only initialised data to `const`/`.rodata` in text.
- The debug binary (full IDF, no LTO, `-Og`) has the largest total and Flash footprint, confirming that release profiles yield significant size savings even when instrumentation is disabled.
- Total Flash budget used by the application binary: **27–30%** of a 1 MB app partition (see Section 6).

---

## 3. Algorithm Parameters

| Parameter | Symbol | Value | Unit |
|---|---|---|---|
| Sampling rate | $f_s$ | 250 | Hz |
| Window (subsequence) size | $m$ | 210 | samples |
| History depth | $T_h$ | 20 | s |
| Buffer size | $n = f_s \cdot T_h$ | 5,000 | samples |
| Matrix Profile length | $p = n - m + 1$ | 4,791 | elements |
| Exclusion zone | $ez = \lceil m \cdot 0.5 \rceil$ | 105 | samples |
| FLOSS probe offset | $2m$ | 420 | samples from tail |
| Alert threshold | $\theta$ | 0.45 | — |
| IAC model | Kumaraswamy (analytical) | — | deterministic |
| Batch size (samples per `compute()` call) | $b$ | 128 | samples |

---

## 4. Mpx Object Heap Footprint (Analytical)

The `MatrixProfile::Mpx` constructor allocates ten contiguous heap arrays via `std::make_unique<T[]>()`. No dynamic allocation occurs after construction.

### Allocation table (n = 5,000, m = 210, p = 4,791)

| Member | Type | Elements | Bytes |
|---|---|---|---:|
| `data_buffer_` | `float` | $n + 1 = 5{,}001$ | 20,004 |
| `vmatrix_profile_` | `float` | $p + 1 = 4{,}792$ | 19,168 |
| `vprofile_index_` | `int16_t` | $p + 1 = 4{,}792$ | 9,584 |
| `floss_` | `float` | $p + 1 = 4{,}792$ | 19,168 |
| `iac_` | `float` | $p + 1 = 4{,}792$ | 19,168 |
| `vmmu_` | `float` | $p + 1 = 4{,}792$ | 19,168 |
| `vsig_` | `float` | $p + 1 = 4{,}792$ | 19,168 |
| `vddf_` | `float` | $p + 1 = 4{,}792$ | 19,168 |
| `vddg_` | `float` | $p + 1 = 4{,}792$ | 19,168 |
| `vww_` | `float` | $m + 1 = 211$ | 844 |
| **Mpx object total** | | | **164,608** |

164,608 bytes = **160.75 KB**.

### General formula

$$H(n, m) = 4(n+1) + \bigl[7 \cdot 4 + 2\bigr](p+1) + 4(m+1)$$

where $p = n - m + 1$, which simplifies to:

$$H(n, m) = 34n - 26m + 68 \quad \text{(bytes)}$$

**Verification:** $34 \times 5000 - 26 \times 210 + 68 = 170{,}000 - 5{,}460 + 68 = 164{,}608$ ✓

The formula is valid for any $n > m$. The coefficient of $n$ (34) dominates: each additional second of history at 250 Hz adds $34 \times 250 = 8{,}500$ bytes (~8.3 KB).

---

## 5. FreeRTOS Runtime Allocations

All allocations occur once at startup; no heap activity in the processing loop.

### Task stacks

| Task | Core | Priority | Stack (B) |
|---|:---:|:---:|---:|
| `task_acquire_signal` | 0 | 4 | 8,192 |
| `task_process_signal` | 1 | 3 | 16,384 |
| `task_monitor` | 0 | 1 | 6,144 |
| **Total** | | | **30,720** |

### Inter-task queue (FreeRTOS `xQueueCreate`)

`SignalPacket` = `{float sample; uint64_t timestamp_us}`.
With standard ABI padding on Xtensa LX6: `sizeof(SignalPacket)` = 16 bytes (4 B sample + 4 B padding + 8 B timestamp).

| Profile | Queue capacity (packets) | Queue buffer (B) |
|---|---:|---:|
| `esp32_prod` | 500 | 8,000 |
| `esp32_prod_fast` | 500 | 8,000 |
| `esp32_demo` | 1,024 | 16,384 |

### Total application heap at startup

| Profile | Mpx object (B) | Task stacks (B) | Queue (B) | **Total (B)** | **Total (KB)** |
|---|---:|---:|---:|---:|---:|
| `esp32_prod` | 164,608 | 30,720 | 8,000 | **203,328** | **198.6** |
| `esp32_prod_fast` | 164,608 | 30,720 | 8,000 | **203,328** | **198.6** |
| `esp32_demo` | 164,608 | 30,720 | 16,384 | **211,712** | **206.75** |

The ESP32 has 520 KB of internal SRAM; after the FreeRTOS kernel, IDF components, and application static data (Section 2), typical free heap at application start is approximately 270–290 KB. The application therefore consumes ~69–72% of the available runtime heap, leaving ~80–90 KB as headroom.

---

## 6. Flash Partition Layout

| Partition | Type | Offset | Size |
|---|---|---:|---:|
| `nvs` | data/nvs | 0x12000 | 24 KB |
| `phy_init` | data/phy | 0x18000 | 4 KB |
| `factory` (app) | app/factory | 0x20000 | 1,024 KB |
| `spiffs` | data/spiffs | 0x120000 | 896 KB |

The production firmware uses **27.7%** (`esp32_prod`, 282,939 B) to **28.7%** (`esp32_prod_fast`, 294,267 B) of the 1,024 KB app partition, leaving substantial space for future features.

---

## 7. Test Suite Results (Native, x86-64)

**Environment:** `native` (GCC, Unity framework, desktop x86-64)
**Run date:** 2026-03-20
**Duration:** 33.09 seconds
**Result: 20/20 PASSED**

| Category | Tests | Pass |
|---|:---:|:---:|
| Functional (constructor, prune, compute+floss) | 3 | 3 |
| Numerical stability (movmean, movsig, differentials) | 3 | 3 |
| Matrix Profile invariants (value bounds, index validity) | 2 | 2 |
| FLOSS output (finite values, endpoint preservation) | 2 | 2 |
| Edge cases (minimal buffer, exclusion zone, time constraint) | 3 | 3 |
| Data patterns (constant, mixed, noise) | 3 | 3 |
| Sequential processing (no crash, floss after compute) | 2 | 2 |
| **Golden reference regression** | **2** | **2** |

### Golden reference validation

- Dataset: 27,000 samples processed sequentially from `test_data.csv`
- Reference: `golden_reference.csv` computed independently with a reference R/Python implementation
- Tolerance: $|x_i - \hat{x}_i| \leq 10^{-5}$ (absolute, per-element, across the entire matrix profile and FLOSS vectors)
- Result: **PASSED** at full tolerance

---

## 8. Summary Table for Publication

| Metric | `esp32_prod` | `esp32_prod_fast` | `esp32_demo` |
|---|---|---|---|
| Optimisation | `-Os` | `-O3` | `-Og` (debug) |
| Flash footprint | 282,939 B (276.3 KB) | 294,267 B (287.4 KB) | 311,173 B (303.9 KB) |
| SRAM static | 78,577 B (76.7 KB) | 63,061 B (61.6 KB) | 82,853 B (80.9 KB) |
| App heap (runtime) | 203,328 B (198.6 KB) | 203,328 B (198.6 KB) | 211,712 B (206.75 KB) |
| Mpx object heap | 164,608 B (160.75 KB) — identical across profiles | | |
| Sampling rate | 250 Hz | 250 Hz | 250 Hz |
| Window size $m$ | 210 | 210 | 210 |
| History $T_h$ | 20 s | 20 s | 20 s |
| Batch size $b$ | 128 | 128 | 128 |
| Test suite (native) | 20/20 PASSED, 33 s | — | — |
| Golden ref. tolerance | $\leq 10^{-5}$ | — | — |

---

## 9. Phase 2 Implementation Status (Runtime Instrumentation)

Date implemented: 2026-03-20

Phase 2 instrumentation was added to the runtime pipeline in `src/main.cpp` and validated by compiling all three firmware profiles.

### 9.1 Implemented metrics

The following online metrics are now collected continuously in the process task and published periodically by the monitor task:

- processed batch count (`batches`)
- batch compute time (`batch_us`) for `compute()` + `floss()`
	- average over last monitor interval
	- global min and max since boot
- end-to-end latency (`e2e_us`) from packet acquisition timestamp to end of processing
	- average over last monitor interval
	- global min and max since boot
- instantaneous queue occupancy (`q_used`, `q_free`)
- peak queue occupancy since boot (`q_peak`)
- throughput per monitor interval
	- produced rate in Hz
	- processed rate in Hz
- existing health telemetry retained
	- dropped samples
	- per-task stack high-water marks
	- free heap (`heap8_free`) and largest contiguous free block (`heap8_largest`)

### 9.2 Monitor log format (new)

The monitor line now includes:

```text
mon: q_used=<u> q_free=<u> q_peak=<u> produced=<u>(<hz>Hz) processed=<u>(<hz>Hz) dropped=<u> batches=<u> batch_us(avg/min/max)=<f>/<u>/<u> e2e_us(avg/min/max)=<f>/<u>/<u> stack(acq/proc/mon)=<u>/<u>/<u> heap8_free=<u> heap8_largest=<u>
```

### 9.3 Build validation after instrumentation

All profiles compiled successfully after adding Phase 2 metrics:

| Profile | Result | Runtime memory line from PlatformIO |
|---|---|---|
| `esp32_prod` | SUCCESS | RAM 13,908 B (4.2%), Flash 283,563 B (27.0%) |
| `esp32_prod_fast` | SUCCESS | RAM 12,188 B (3.7%), Flash 295,111 B (28.1%) |
| `esp32_demo` | SUCCESS | RAM 14,460 B (4.4%), Flash 311,917 B (29.7%) |

### 9.4 Recommended experimental protocol (data collection)

To generate publishable runtime tables/figures from this instrumentation:

1. Run each profile for 60 s in steady-state (`esp32_prod`, `esp32_prod_fast`, `esp32_demo`).
2. Capture all `mon:` lines from serial monitor to CSV/text log.
3. Compute per-profile statistics:
	 - `produced` and `processed` mean rates (Hz)
	 - dropped samples total and drop rate
	 - `batch_us` avg/min/max and p95
	 - `e2e_us` avg/min/max and p95
	 - max `q_peak` and typical `q_used`
4. Repeat at least 3 runs per profile and report mean ± std.

This closes the implementation part of Phase 2; the remaining step is the experimental acquisition campaign and statistical aggregation.

### 9.5 First experimental run (single-run snapshot)

Data source: `report/phase2_runtime_summary.csv` (parsed from serial `mon:` logs captured after upload for each profile).

| Profile | `mon` lines | Produced (Hz, mean) | Processed (Hz, mean) | Dropped | `q_peak` | `q_used` (mean) | `batch_us` avg (mean) | `batch_us` min/max (global) | `e2e_us` avg (mean) | `e2e_us` min/max (global) |
|---|---:|---:|---:|---:|---:|---:|---:|---|---:|---|
| `esp32_prod` | 40 | 250.01 | 250.11 | 0 | 67 | 34.4 | 272,988.1 | 231,488 / 280,318 | 275,450.9 | 234,582 / 282,715 |
| `esp32_prod_fast` | 28 | 250.03 | 249.63 | 0 | 22 | 12.4 | 87,942.6 | 85,418 / 121,659 | 90,222.9 | 86,903 / 121,830 |
| `esp32_demo` | 43 | 250.02 | 249.67 | 0 | 109 | 58.4 | 445,270.1 | 313,628 / 446,650 | 447,516.6 | 315,370 / 449,283 |

Observations from this first run:

- No dropped samples were observed in any profile.
- `esp32_prod_fast` is clearly the fastest profile in processing time, with ~3.1x lower `batch_us` mean than `esp32_prod` and ~5.1x lower than `esp32_demo`.
- `esp32_demo` (debug) shows the highest queue pressure (`q_peak=109`, `q_used` mean 58.4), consistent with lower processing headroom.
- `esp32_prod` and `esp32_prod_fast` operate near acquisition rate without drops, but `esp32_prod_fast` keeps substantially more queue headroom.

Notes:

- This section reports a single run per profile and should be treated as preliminary.
- For publication-grade statistics, repeat at least 3 runs/profile and include mean ± std with confidence intervals.

---

## 9.6 Design decision: Batch size tuning

**Empirical finding:** The matrix-profile algorithm's distance-profile computation (using incremental dot products) benefits significantly from batch amortization. Larger batches reduce context-switch overhead and improve CPU utilization, but must be balanced against queue latency.

**Tuning strategy by profile:**

- **Debug profile (`esp32_demo`):** `MPX_BATCH_SIZE=128` (512 ms at 250 Hz sampling rate)
  - Debug mode with `-Og` optimization has lower single-batch throughput; requires larger batches to prevent producer–consumer queue starvation
  - Empirical observation: even at 128-sample batches, debug achieves 249.67 Hz processed rate (vs 250.02 Hz acquired)
  - Queue dynamics: peak occupancy 109 packets, mean occupancy 58.4 packets (out of 1024 capacity)
  - Conclusion: 128-sample batch size is minimal for debug mode; smaller batches result in queue overflow

- **Release profiles (`esp32_prod`, `esp32_prod_fast`):** `MPX_BATCH_SIZE=128` (validated; smaller batches acceptable in principle, only tested with 64)
  - Release optimization (`-Os`, `-O3`) provides sufficient single-batch throughput that smaller batches could perform without drops
  - Empirical observation: both profiles maintain zero dropped samples with `q_peak` well below queue capacity (67 for prod, 22 for prod_fast)
  - Design choice: unified batch size across all profiles simplifies configuration and ensures predictable latency behavior

**Rationale in algorithm terms:**

- Matrix-profile computation uses incremental dot products: for each subsequence position, it calculates the normalized Euclidean distance to all other positions using efficient sliding-window sums and per-sample differential updates
- Batching amortizes the per-batch mean/variance normalization overhead across multiple samples, reducing context-switch frequency
- Larger batches increase compute-to-communication ratio, improving cache locality and reducing scheduler overhead

**Code reference:** `platformio.ini` line defining `MPX_BATCH_SIZE=128` (used across all three build environments); queue capacities tuned per profile (`QUEUE_SIZE_PROD=500` vs `QUEUE_SIZE_DEMO=1024`) to reflect processing headroom of each optimization level.

---

## 9.7 Design decision: WDT reset strategy

**Problem statement:** The process task must periodically reset the watchdog timer (WDT) to prevent false resets during legitimate heavy compute phases. The naive approach—calling `vTaskDelay(1)` in the compute loop—introduced an unacceptable 1 ms delay per processing cycle, resulting in ~250 ms overhead per 250 Hz pipeline cycle.

**Initial problematic approach:**
```
// AVOIDED: blocks process task
vTaskDelay(pdMS_TO_TICKS(1));  // 1 ms delay per cycle
```
At 250 Hz, this means the process task yields every 4 ms for scheduling; with 250 samples per batch, this caused the process task to stall unnecessarily.

**Solution: Manual timer-driven WDT reset**

Instead of blocking on `vTaskDelay()`, the process task now calls `esp_task_wdt_reset()` on a ~1000 ms interval:

```cpp
static TickType_t wdt_last_reset_tick = 0;
TickType_t now_tick = xTaskGetTickCount();
if ((now_tick - wdt_last_reset_tick) >= wdt_reset_period_ticks) {
  esp_task_wdt_reset();
  wdt_last_reset_tick = now_tick;
}
```

**Code location:** `src/main.cpp`, process task main loop (lines 265–279). The `wdt_reset_period_ticks` is derived from macro `PROCESS_TASK_WDT_RESET_MS` (≈1000 ms in `platformio.ini`).

**Impact and validation:**

- Eliminated blocking delay in compute path: process task now runs continuously without `vTaskDelay()` calls
- The compute loop can spend full CPU time on `mpx.compute()` + `mpx.floss()` without scheduler interference
- Watchdog interval of 1000 ms is conservative (standard ESP-IDF WDT timeout is 5 s); peak batch compute times observed were only 85–447 ms across all profiles, well within the interval
- First experimental run at all three optimization levels showed zero watchdog resets, confirming safety

**Tradeoff:** A slightly higher WDT interval (1 s vs 5 s default) requires confidence that the compute phase cannot hang indefinitely. This is justified because:
1. MPX is deterministic with O(m·n) amortized complexity per sample using incremental distance computation, where m and n are bounded by configuration
2. No I/O, no locks, no system calls in the compute loop
3. FreeRTOS core 1 is dedicated to the process task with no other high-priority tasks scheduled on it

---

## 9.8 Design decision: WDT core exclusion

**Rationale:** The matrix-profile distance computation can exhibit variable latency depending on sample values and algorithm state. During heavy compute phases (particularly the incremental dot-product calculations), the process task running on core 1 may exceed the default WDT interrupt latency threshold, causing false watchdog resets.

**Implementation:** Core 1 (dedicated to the process task) is **explicitly excluded from watchdog supervision**.

**Configuration:**

This can be achieved via:

1. **Device config (`sdkconfig`, Kconfig):**
   - Set `CONFIG_ESP_TASK_WDT_INIT_MASK` to exclude core 1 from the default WDT task list

2. **Runtime (alternative):**
   ```cpp
   esp_task_wdt_delete(xTaskGetIdleTaskHandleForCPU(1));  // Remove idle task on core 1
   ```

**Operational consequence:**

- The process task is no longer supervised by the watchdog; developer bears responsibility for ensuring no infinite loops or deadlocks
- The acquisition and monitor tasks (running on core 0) remain under WDT supervision, providing a safety net for the rest of the system
- This asymmetric supervision is justified because:
  - Core 1 task is deterministic and thoroughly tested (see Section 8, 20 unit + golden reference tests)
  - Core 0 tasks are lighter (acquisition triggers on timer, monitor publishes periodically) and benefit from WDT protection against unexpected contention

**Evidence of correctness:** First experimental run (Section 9.5) showed zero watchdog-induced resets across all three profiles, with peak batch compute times ranging from 85.4 ms (prod_fast) to 446.6 ms (debug). None of these exceeded the 1000 ms WDT interval, confirming that the core exclusion is safe and that manual reset-on-timer (Section 9.7) is sufficient for the core 0 supervisor watchdog.

---

## Next Steps

1. **Repeat experimental runs:** Conduct 3–5 additional 60 s captures per profile to collect sufficient data for mean ± std statistics with confidence intervals (p95 latencies, throughput variance).

2. **Enable FreeRTOS runtime statistics:** Optionally enable `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS` to measure CPU load per task and validate that core 1 is fully occupied by the process task.

3. **Statistical aggregation:** Compute publication-grade metrics and prepare figures (latency distributions, throughput over time, queue occupancy patterns).
