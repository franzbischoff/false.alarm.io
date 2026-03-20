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

*Next step: Phase 2 — Runtime instrumentation (per-batch compute time, end-to-end latency, FreeRTOS CPU load by task).*
