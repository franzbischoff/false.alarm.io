# false.alarm.io — AI coding agent instructions

## Session Management & Context (Persistent Memory)
**IMPORTANT**: Information from previous sessions, detailed context, decisions made, and project architecture are stored in `.github/copilot-memory.md`.
- **Start of session**: Whenever the user indicates they are starting the day's work, consult this file immediately for the latest project state.
- **End of session**: When the user informs you that the day's work is finished, update this file with the most recent information regarding the project's status, architectural changes, and pending tasks.

## Big picture
- ESP-IDF 5.5.3/PlatformIO firmware project for streaming matrix-profile analysis on ESP32 hardware.
- Runtime entrypoint is `src/main.cpp`; processing logic lives in `lib/Mpx` (`Mpx.hpp`/`Mpx.cpp`).
- The app uses two pinned FreeRTOS tasks with producer/consumer flow:
  - `task_read_signal` produces `float` samples at `SAMPLING_RATE_HZ`.
  - `task_compute` consumes samples from a ring buffer and runs `MatrixProfile::Mpx::compute()` + `floss()`.
- Compile-time macros in `platformio.ini` define algorithm behavior (`WINDOW_SIZE`, `HISTORY_SIZE_S`, `FLOSS_LANDMARK_S`, etc.). Treat these as the primary tuning interface.

## Build and test environments
- **esp32idf** (default): Main firmware for SparkFun ESP32 IoT RedBoard on COM4
  - Build: `platformio run -e esp32idf`
  - Upload: `platformio run -e esp32idf -t upload`
  - Monitor: `platformio device monitor -b 115200`
- **native**: Desktop x86-64 tests (Unity framework, 20 tests, ~25s)
  - Test: `platformio test -e native`
- **esp32_test**: Hardware tests with SD card (SDSPI, Unity framework, 20 tests, ~1m44s)
  - Test: `platformio test -e esp32_test`
  - Requires SD card with `test_data.csv` and `golden_reference.csv` (FATFS 8.3 names: `TEST_D~1.CSV`, `GOLDEN~1.CSV`)
  - Default pins: MISO=19, MOSI=23, SCK=18, CS=5

## Data flow and storage
- Default build uses `FILE_DATA=1` to read sensor input from LittleFS file `/littlefs/floss.csv`.
- LittleFS mounted in `app_main()` via `esp_vfs_littlefs_register()` with partition label `littlefs` (`partitions.csv`).
- Test environment (`esp32_test`) uses SD card via SDSPI for golden reference validation.
- Vendored component: `components/esp_littlefs` (joltwallet v1.5.0).



## Project-specific coding patterns
- C++17 standard (GCC 14.2.0) with 2-space indentation (`.editorconfig`, `.clang-format`).
- `lib/Mpx` uses trailing underscores for private members/methods (e.g., `data_buffer_`, `movmean_()`); preserve this convention.
- RAII with `std::make_unique<T[]>()` for memory safety; avoid dynamic allocations in high-frequency task loops.
- Global state in embedded paths (task handles, ring buffer, flags) is intentional with explicit analyzer suppressions; do not "clean up" unless requested.
- Deterministic behavior: `prune_buffer()` uses sinusoidal pattern; `floss_iac_()` uses analytical Kumaraswamy distribution (not Monte Carlo).

## Testing strategy
- Unity framework with 20 tests: 3 functional, 15 robustness, 2 golden reference.
- Golden tests validate against `test_data.csv` (27,000 samples processed) with 1e-5 tolerance.
- Test files: `test/test_mpx.cpp`, `test/test_mpx_robustness.cpp`, `test/test_mpx_golden.cpp`.
- Documentation: `test/TEST_STRATEGY.md`, `test/README_ESP32_TESTING.md`.

## Documentation and Communication Style
- **Markdown Management**: Do not create additional Markdown documentation files without first asking the user. All documentation changes must be made to existing files unless explicitly requested otherwise.
- **Writing Style**: Do not use emojis or emoticons in tutorials, documentation, or pedagogical content.
- **Tone**: Maintain a professional, clean, and academic appearance. Avoid any formatting that suggests automatic AI generation. Use simple, direct, and credible text.

## Quality and checks
- Static analysis configured in `platformio.ini` (`cppcheck`, `clangtidy`, `pvs-studio`) and `.clang-tidy`.
- Prefer small, surgical changes in `src/main.cpp` and `lib/Mpx/*`; avoid broad refactors across vendored `components/` unless fixing a dependency directly.
