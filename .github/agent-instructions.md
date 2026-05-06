# false.alarm.io — Agent Instructions (Summary)

## 1) Session discipline
- Read `.github/copilot-memory.md` at session start.
- Update `.github/copilot-memory.md` at session end with what changed and what remains.

## 2) Project overview
- ESP-IDF 5.5.3 / PlatformIO firmware on ESP32.
- Entry point: `src/main.cpp`.
- Algorithm core: `lib/Mpx/Mpx.hpp` and `lib/Mpx/Mpx.cpp`.
- Runtime tasks:
  - `task_acquire_signal` (producer)
  - `task_process_signal` (consumer, strict `MPX_BATCH_SIZE` batching)
  - `task_monitor` (runtime metrics)

## 3) Build and test commands
- `esp32_prod`
  - Build: `platformio run -e esp32_prod`
  - Upload: `platformio run -e esp32_prod -t upload`
- `esp32_prod_fast`
  - Build: `platformio run -e esp32_prod_fast`
  - Upload: `platformio run -e esp32_prod_fast -t upload`
- `esp32_prod_stats`
  - Build: `platformio run -e esp32_prod_stats`
  - Upload: `platformio run -e esp32_prod_stats -t upload`
- `esp32_systemview`
  - Build: `platformio run -e esp32_systemview`
  - Upload: `platformio run -e esp32_systemview -t upload`
- `esp32_demo`
  - Build: `platformio run -e esp32_demo`
  - Upload: `platformio run -e esp32_demo -t upload`
- `native`
  - Test: `platformio test -e native`
- `esp32_test`
  - Test: `platformio test -e esp32_test`

## 4) Data source and runtime assumptions
- Active runtime source is SD CSV (`SIGNAL_SOURCE_KIND=0`) unless changed.
- Other source modes:
  - `1`: ADC
  - `2`: I2C
- SD/FATFS is the active storage path for runtime/golden workflows.

## 5) Coding conventions
- C++17, 2-space indentation.
- Preserve `lib/Mpx` private trailing underscore naming.
- Avoid high-frequency loop allocations.
- Keep embedded global runtime state unless user requests refactor.
- Keep deterministic behavior in MPX/FLOSS internals.

## 6) Profiling guidance
- Use `esp32_prod_stats` for runtime CPU/task metrics.
- Use `esp32_systemview` for trace captures.
- SystemView marker instrumentation must stay behind `CONFIG_APPTRACE_SV_ENABLE` guards.

## 7) Documentation and quality
- Prefer updating existing docs; do not create new Markdown docs without explicit user request.
- Keep technical tone, concise and objective.
- Prefer small and focused patches.

## 8) Tooling path
- PlatformIO binary: `C:\.platformio\penv\Scripts\platformio.exe`
