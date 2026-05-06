# ESP32 Testing with SD Card

This guide explains how to run Unity tests on ESP32 using the SparkFun ESP32 IoT RedBoard with SD card.

## Required Hardware

- **SparkFun ESP32 IoT RedBoard** (or compatible with SD slot)
- **SD/microSD Card** (minimum 2GB, recommended FAT32)
- **USB Cable** for upload and serial monitor

## SD Card Preparation

### 1. Format the Card

```powershell
# Windows: Format as FAT32
# GUI: Right-click on drive → Format → FAT32
```

### 2. Copy Test Files

Copy the following files to the SD card root:

```
SD Card Root/
├── test_data.csv                      (~234 KB)
└── golden_reference_nodelete.csv      (~1.2 MB)
```

Note: with FATFS on ESP-IDF, names may appear as 8.3 at runtime
(for example `TEST_D~1.CSV` and `GOLDEN~1.CSV`). Tests already have fallback
for these short names.

**Copy command:**
```powershell
Copy-Item test\test_data.csv E:\
Copy-Item test\golden_reference_nodelete.csv E:\
```
*(Replace `E:` with your SD card letter)*

### 3. Verify Files

```powershell
# Verify that files were copied
Get-ChildItem E:\ -Include *.csv
```

## Build and Upload

### Using PlatformIO CLI

```powershell
# 1. Build tests
platformio test -e esp32_test --without-testing

# 2. Upload to ESP32
platformio test -e esp32_test --without-uploading

# 3. Run tests (build + upload + execute)
platformio test -e esp32_test
```

### Using VS Code

1. Open **PlatformIO** sidebar
2. Select **esp32_test** → **Advanced** → **Test**
3. Monitor serial output

## Environment Configuration

The `esp32_test` environment is configured in [platformio.ini](../platformio.ini):

```ini
[env:esp32_test]
platform = espressif32
framework = espidf
board = sparkfun_esp32_iot_redboard
test_framework = unity
build_flags = -DUSE_SD_CARD=1
```

### Important Flags

- `USE_SD_CARD=1`: Activates `/sdcard/` paths for test files
- No `FILE_DATA`: this environment is dedicated to Unity tests

## SD Card Mounting on ESP32

The test code mounts the SD automatically during `app_main()`.

Current implementation:

- In [test/test_runner.c](test/test_runner.c), tests run in a dedicated task
    with larger stack (`unity_test`, 32768 bytes), avoiding stack overflow.
- In [test/sd_card_mount.h](test/sd_card_mount.h), mounting uses `SDSPI`
    (not `SDMMC`) and tries to detect CSVs in root and in `/sdcard/test`.

If needed, SPI pins can be overridden by build flags:

```ini
-DSD_SPI_MISO_PIN=GPIO_NUM_19
-DSD_SPI_MOSI_PIN=GPIO_NUM_23
-DSD_SPI_SCK_PIN=GPIO_NUM_18
-DSD_SPI_CS_PIN=GPIO_NUM_5
```

## Tests Executed

When running on ESP32 with `USE_SD_CARD=1`, all **20 tests** are executed:

### Sanity Tests (3)
- Constructor initial state
- Prune buffer invariants
- Compute and FLOSS integration

### Robustness Tests (15)
- Numerical stability (3 tests)
- Matrix Profile invariants (2 tests)
- FLOSS output (2 tests)
- Edge cases (3 tests)
- Data patterns (3 tests)
- Sequential processing (2 tests)

### Golden Reference Tests (2)
- Metadata validation
- Sample validation (~434 samples)

Current golden details:
- `metadata`: processes 27,000 samples (`54 x 500`) and validates the 5 metadata lines.
- `sample_validation`: reads `golden_reference_nodelete.csv` in a single pass
    (streaming), maintaining coverage by sampling without repeated read cost.

## Troubleshooting

### Error: "Failed to load test data"
**Cause**: CSV files not found on SD card
**Solution**:
- Verify that SD card is inserted
- Confirm files in SD root
- Confirm FATFS short names (e.g.: `TEST_D~1.CSV` and `GOLDEN~1.CSV`)

### Error: "SD card mount failed"
**Cause**: Problem mounting the SD
**Solution**:
- Reformat SD card as FAT32
- Verify ESP32 pinout
- Test SD card in another device

### Tests Too Slow
**Cause**: SD card reading is slower than native
**Solution**:
- Reduce `sample_interval` in [test_mpx_golden.cpp](test_mpx_golden.cpp)
- Or create minimal version of golden reference

## Comparison: Native vs ESP32

| Aspect | Native | ESP32 + SD |
|---------|--------|------------|
| **Speed** | ~26s | ~1-3 min (typical) |
| **RAM Memory** | Unlimited | ~520 KB |
| **Storage** | Local filesystem | SD Card |
| **Float Precision** | x86-64 double | ARM float32 |
| **Usage** | CI/CD, development | Hardware validation |

## Next Steps

1. Periodically validate `native` and `esp32_test` to avoid regression between platforms.
2. If changing `num_iterations`, regenerate golden and update test expectations.
3. If changing board/pins, adjust `SD_SPI_*` flags in test environment.

## References

- [ESP-IDF SD/MMC API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/sdmmc.html)
- [Unity Test Framework](https://github.com/ThrowTheSwitch/Unity)
- [PlatformIO Testing](https://docs.platformio.org/en/latest/advanced/unit-testing/)
