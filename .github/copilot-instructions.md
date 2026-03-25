# false.alarm.io — AI coding agent instructions

## Session Management & Context (Persistent Memory)
**IMPORTANT**: Information from previous sessions, detailed context, decisions made, and project architecture are stored in `.github/copilot-memory.md`.
- **Start of session**: Whenever the user indicates they are starting the day's work, consult this file immediately for the latest project state.
- **End of session**: When the user informs you that the day's work is finished, update this file with the most recent information regarding the project's status, architectural changes, and pending tasks.

## Big picture
- ESP-IDF 5.5.3/PlatformIO firmware project for streaming matrix-profile analysis on ESP32 hardware.
- Runtime entrypoint is `src/main.cpp`; processing logic lives in `lib/Mpx` (`Mpx.hpp`/`Mpx.cpp`).
- Runtime pipeline is a producer/consumer design with pinned FreeRTOS tasks:
  - `task_acquire_signal` acquires samples at `SAMPLING_RATE_HZ`.
  - `task_process_signal` accumulates a strict batch (`MPX_BATCH_SIZE`) and runs `MatrixProfile::Mpx::compute()` + `floss()`.
  - `task_monitor` publishes runtime telemetry (`mon:` logs).
- Compile-time macros in `platformio.ini` are the primary tuning interface (`WINDOW_SIZE`, `HISTORY_SIZE_S`, `MPX_BATCH_SIZE`, queue size, task priorities/cores).

## Build and test environments
- **esp32_prod**: release profile for baseline runtime.
  - Build: `platformio run -e esp32_prod`
  - Upload: `platformio run -e esp32_prod -t upload`
  - Monitor: `platformio device monitor -p COM10 -b 115200 --filter time`
- **esp32_prod_fast**: speed-focused release (`-O3 -DNDEBUG`).
  - Build: `platformio run -e esp32_prod_fast`
  - Upload: `platformio run -e esp32_prod_fast -t upload`
- **esp32_prod_stats**: release profile with FreeRTOS runtime stats enabled.
  - Build: `platformio run -e esp32_prod_stats`
  - Upload: `platformio run -e esp32_prod_stats -t upload`
- **esp32_systemview**: tracing profile (AppTrace/SystemView instrumentation).
  - Build: `platformio run -e esp32_systemview`
  - Upload: `platformio run -e esp32_systemview -t upload`
- **esp32_demo**: debug-oriented profile.
  - Build: `platformio run -e esp32_demo`
  - Upload: `platformio run -e esp32_demo -t upload`
- **native**: desktop Unity tests.
  - Test: `platformio test -e native`
- **esp32_test**: hardware Unity tests with SD card golden reference.
  - Test: `platformio test -e esp32_test`
  - Requires SD card with `test_data.csv` and `golden_reference.csv` (8.3 names accepted: `TEST_D~1.CSV`, `GOLDEN~1.CSV`).

## Data flow and storage
- Default signal source in current profiles is SD CSV (`SIGNAL_SOURCE_KIND=0`), read through `sd_card_service` + `signal_source_*` backends.
- Source selection is compile-time (`SIGNAL_SOURCE_KIND`):
  - `0` = SD CSV
  - `1` = Analog ADC
  - `2` = I2C sensor
- SD/FATFS is the active input path for runtime and golden-reference workflows.
- LittleFS is not the active storage backend in the current runtime pipeline.

## Project-specific coding patterns
- C++17 standard (GCC 14.2.0) with 2-space indentation (`.editorconfig`, `.clang-format`).
- `lib/Mpx` uses trailing underscores for private members/methods (for example `data_buffer_`, `movmean_()`); preserve this convention.
- RAII with `std::make_unique<T[]>()` for memory safety; avoid dynamic allocations inside high-frequency loops.
- Global state in embedded paths (task handles, queue, runtime counters) is intentional with explicit analyzer suppressions; do not refactor this pattern unless requested.
- Deterministic behavior: `prune_buffer()` uses sinusoidal pattern; `floss_iac_()` uses analytical Kumaraswamy distribution (not Monte Carlo).
- Keep batch semantics explicit: process task should not run partial batches unless the user asks for that behavior.

## Profiling and instrumentation notes
- Runtime telemetry (`mon:`) includes throughput, queue occupancy, batch/e2e latency, and stack/heap indicators.
- `esp32_prod_stats` and `esp32_systemview` are the preferred profiles for CPU/load and trace analysis.
- SystemView instrumentation is conditionally compiled with `CONFIG_APPTRACE_SV_ENABLE`; user markers around compute are intentionally guarded for tracing-only builds.

## Testing strategy
- Unity framework with 20 tests: 3 functional, 15 robustness, 2 golden reference.
- Golden tests validate against `test_data.csv` (27,000 samples processed) with 1e-5 tolerance.
- Test files: `test/test_mpx.cpp`, `test/test_mpx_robustness.cpp`, `test/test_mpx_golden.cpp`.
- Documentation: `test/TEST_STRATEGY.md`, `test/README_ESP32_TESTING.md`.

## Documentation and communication style
- **Markdown Management**: Do not create additional Markdown documentation files without first asking the user. Prefer updating existing docs.
- **Writing Style**: Do not use emojis or emoticons in tutorials, documentation, or pedagogical content.
- **Tone**: Keep text professional, concise, and technically grounded.

## Quality and checks
- Static analysis is configured in `platformio.ini` (`cppcheck`, `clangtidy`, `pvs-studio`) and `.clang-tidy`.
- Prefer small, surgical changes in `src/main.cpp` and `lib/Mpx/*`.
- Avoid broad refactors in `components/` unless explicitly required.

## PlatformIO path
- The PlatformIO binary is located at `C:\\.platformio\\penv\\Scripts\\platformio.exe`.
