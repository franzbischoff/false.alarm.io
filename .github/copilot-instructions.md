# false.alarm.io — AI coding agent instructions

## Session Management & Context (Persistent Memory)
**IMPORTANT**: Information from previous sessions, detailed context, decisions made, and project architecture are stored in `.github/copilot-memory.md`.
- **Start of session**: Whenever the user indicates they are starting the day's work, consult this file immediately for the latest project state.
- **End of session**: When the user informs you that the day's work is finished, update this file with the most recent information regarding the project's status, architectural changes, and pending tasks.

## Big picture
- This is an ESP-IDF/PlatformIO firmware project centered on streaming matrix-profile analysis for signal data.
- Runtime entrypoint is `src/main.cpp`; processing logic lives in `lib/Mpx` (`Mpx.hpp`/`Mpx.cpp`).
- The app uses two pinned FreeRTOS tasks with a producer/consumer flow:
  - `task_read_signal` produces `float` samples at `SAMPLING_RATE_HZ`.
  - `task_compute` consumes samples from a ring buffer and runs `MatrixProfile::Mpx::compute()` + `floss()`.
- Compile-time macros in `platformio.ini` define algorithm behavior (`WINDOW_SIZE`, `HISTORY_SIZE_S`, `FLOSS_LANDMARK_S`, etc.). Treat these as the primary tuning interface.

## Data flow and integration points
- Current default build enables `FILE_DATA=1` (see `platformio.ini` `build_src_flags`), so sensor input is read from LittleFS file `/littlefs/floss.csv`.
- LittleFS is mounted in `app_main()` via `esp_vfs_littlefs_register()` with partition label `littlefs`; partition table is `partitions.csv`.
- `partitions.csv` must keep a `littlefs` data partition; changing partition names/sizes can break runtime file input.
- `components/esp_littlefs` is vendored (joltwallet `v1.5.0`) and includes `mklittlefs` tooling docs.

## Build, run, and debug workflow
- Primary environment is `esp32idf` (`platformio.ini`, default env).
- Typical CLI loop:
  - `platformio run -e esp32idf`
  - `platformio run -e esp32idf -t upload`
  - `platformio device monitor -b 115200`
- VS Code tasks already define:
  - `PlatformIO: Remote Upload (esp32idf)`
  - `PlatformIO: Remote Monitor` (depends on upload)
- `upload_port`, `monitor_port`, and `debug_port` are pinned to `COM4` in `platformio.ini`; keep this in mind when reproducing issues.
- Debug launch configs are auto-generated in `.vscode/launch.json` and target `build/esp32idf/firmware.elf`.

## Project-specific coding patterns
- C/C++ style is 2-space indentation (`.editorconfig`, `.clang-format`), with C++11 (`.clang-format` `Standard: c++11`).
- `lib/Mpx` uses trailing underscores for private members/methods (e.g., `data_buffer_`, `movmean_()`); preserve this naming convention.
- The codebase intentionally uses some global state in embedded paths (task handles, ring buffer, flags) with explicit analyzer suppressions; do not “clean up” these patterns unless requested.
- Keep memory behavior predictable: `Mpx` allocates fixed buffers up front; avoid adding dynamic allocations in high-frequency task loops.

## Documentation and Communication Style
- **Markdown Management**: Do not create additional Markdown documentation files without first asking the user. All documentation changes must be made to existing files unless explicitly requested otherwise.
- **Writing Style**: Do not use emojis or emoticons in tutorials, documentation, or pedagogical content.
- **Tone**: Maintain a professional, clean, and academic appearance. Avoid any formatting that suggests automatic AI generation. Use simple, direct, and credible text.

## Quality and checks
- Static analysis is configured in `platformio.ini` (`cppcheck`, `clangtidy`, `pvs-studio`) and tuned via `.clang-tidy`.
- Unit-test scaffolding exists under `test/` (Unity example with calculator library), but app behavior is mainly validated through target runs/monitor output.
- Prefer small, surgical changes in `src/main.cpp` and `lib/Mpx/*`; avoid broad refactors across vendored `components/` unless fixing that dependency directly.
