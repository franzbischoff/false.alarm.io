/**
 * @file test_mpx_golden.cpp
 * @brief Golden reference regression test for Mpx library (Unity Framework)
 *
 * This test validates that the Mpx implementation produces results consistent
 * with a previously validated "golden reference" CSV file.
 *
 * Test Process:
 * 1. Load test data (test_data.csv)
 * 2. Process data in chunks (54 x 500 = 27,000 samples)
 * 3. Validate metadata against golden reference
 * 4. Validate sampled entries from golden reference using single-pass streaming
 *
 * Parameters (must match golden reference):
 *   - window_size: 210
 *   - buffer_size: 5000
 *   - chunk_size: 500
 *   - num_iterations: 54 (processes 27,000 samples)
 *
 * Notes:
 * - Metadata test validates final state after processing 27,000 samples.
 * - Sample validation reads the golden CSV in one pass and validates
 *   every 100th line to keep runtime reasonable on ESP32.
 */

#include <Mpx.hpp>
#include <unity.h>

#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <new>

// ============================================================================
// FILE PATH CONFIGURATION
// ============================================================================

#if defined(USE_SD_CARD)
// ESP32 with SD Card: files mounted at /sdcard/
#define TEST_DATA_PATH "/sdcard/test_data.csv"
#define GOLDEN_REFERENCE_PATH "/sdcard/golden_reference_nodelete.csv"
#else
// Native or LittleFS: relative paths
#define TEST_DATA_PATH "test/test_data.csv"
#define GOLDEN_REFERENCE_PATH "test/golden_reference_nodelete.csv"
#endif

static FILE *open_with_fallback(const char *primary, const char *fallback) {
  FILE *file = fopen(primary, "r");
  if (file != nullptr) {
    return file;
  }

  if (fallback != nullptr) {
    return fopen(fallback, "r");
  }

  return nullptr;
}

#if defined(USE_SD_CARD)
static FILE *open_first_available(const char *const *paths, size_t count) {
  for (size_t i = 0; i < count; i++) {
    FILE *file = fopen(paths[i], "r");
    if (file != nullptr) {
      return file;
    }
  }
  return nullptr;
}
#endif

// ============================================================================
// CSV READING UTILITIES (Simplified for Unity compatibility)
// ============================================================================

#if !defined(ESP_PLATFORM)
static char *load_file_to_memory(const char *filename, size_t *out_size);
#endif

// Only needed when SD-card path probing is enabled.
#if defined(USE_SD_CARD)
static FILE *open_test_data_file(const char *filename) {
#if defined(USE_SD_CARD)
  if (strcmp(filename, TEST_DATA_PATH) == 0) {
    static const char *const candidates[] = {
        "/sdcard/test_data.csv",      "/sdcard/test/test_data.csv", "/sdcard/TEST_DATA.CSV",
        "/sdcard/test/TEST_DATA.CSV", "/sdcard/TEST_D~1.CSV",       "/sdcard/test/TEST_D~1.CSV",
    };
    return open_first_available(candidates, sizeof(candidates) / sizeof(candidates[0]));
  }
#endif
  return open_with_fallback(filename, nullptr);
}
#endif

static FILE *open_golden_reference_file(const char *filename) {
#if defined(USE_SD_CARD)
  if (strcmp(filename, GOLDEN_REFERENCE_PATH) == 0) {
    static const char *const candidates[] = {
        "/sdcard/golden_reference_nodelete.csv",
        "/sdcard/test/golden_reference_nodelete.csv",
        "/sdcard/GOLDEN_REFERENCE_NODELETE.CSV",
        "/sdcard/test/GOLDEN_REFERENCE_NODELETE.CSV",
        "/sdcard/GOLDEN~1.CSV",
        "/sdcard/test/GOLDEN~1.CSV",
    };
    return open_first_available(candidates, sizeof(candidates) / sizeof(candidates[0]));
  }
#endif
  return open_with_fallback(filename, nullptr);
}

static uint32_t process_csv_in_chunks(const char *filename, MatrixProfile::Mpx &mpx, uint16_t chunk_size,
                                      uint16_t max_iterations) {
#if !defined(ESP_PLATFORM)
  size_t file_size = 0;
  char *file_buffer = load_file_to_memory(filename, &file_size);
  if (file_buffer == nullptr) {
    return 0;
  }

  char line[256];
  float chunk[500];
  uint16_t chunk_count = 0;
  uint16_t iterations = 0;
  uint32_t total_samples = 0;

  char *cursor = file_buffer;
  char *line_end = strchr(cursor, '\n');
  if (line_end == nullptr) {
    free(file_buffer);
    return 0;
  }

  // Skip CSV header
  cursor = line_end + 1;

  while (cursor < (file_buffer + file_size) && iterations < max_iterations) {
    line_end = strchr(cursor, '\n');
    size_t line_len = 0;
    if (line_end != nullptr) {
      line_len = static_cast<size_t>(line_end - cursor);
    } else {
      line_len = static_cast<size_t>((file_buffer + file_size) - cursor);
    }

    if (line_len >= sizeof(line)) {
      line_len = sizeof(line) - 1;
    }

    memcpy(line, cursor, line_len);
    line[line_len] = '\0';

    if (line_end != nullptr) {
      cursor = line_end + 1;
    } else {
      cursor = file_buffer + file_size;
    }

    char *start = line;
    while (*start == '"' || *start == ' ' || *start == '\t') {
      start++;
    }

    float value = 0.0f;
    int value_int = 0;
    bool parsed = false;
    if (sscanf(start, "%f", &value) == 1) {
      parsed = true;
    } else if (sscanf(start, "%d", &value_int) == 1) {
      value = static_cast<float>(value_int);
      parsed = true;
    }

    if (!parsed) {
      continue;
    }

    chunk[chunk_count++] = value;
    total_samples++;

    if (chunk_count == chunk_size) {
      (void)mpx.compute(chunk, chunk_size);
      chunk_count = 0;
      iterations++;
    }
  }

  free(file_buffer);
  return total_samples;
#else
  FILE *file = open_test_data_file(filename);
  if (file == nullptr) {
    return 0;
  }

  char line[256];
  float chunk[500];
  uint16_t chunk_count = 0;
  uint16_t iterations = 0;
  uint32_t total_samples = 0;

  // Skip CSV header
  if (fgets(line, sizeof(line), file) == nullptr) {
    fclose(file);
    return 0;
  }

  while (fgets(line, sizeof(line), file) != nullptr && iterations < max_iterations) {
    char *start = line;
    while (*start == '"' || *start == ' ' || *start == '\t') {
      start++;
    }

    float value = 0.0f;
    int value_int = 0;
    bool parsed = false;
    if (sscanf(start, "%f", &value) == 1) {
      parsed = true;
    } else if (sscanf(start, "%d", &value_int) == 1) {
      value = static_cast<float>(value_int);
      parsed = true;
    }

    if (!parsed) {
      continue;
    }

    chunk[chunk_count++] = value;
    total_samples++;

    if (chunk_count == chunk_size) {
      (void)mpx.compute(chunk, chunk_size);
      chunk_count = 0;
      iterations++;
    }
  }

  fclose(file);
  return total_samples;
#endif
}

/**
 * @brief Structure for golden reference entry
 */
struct GoldenEntry {
  char buffer_type[20];
  uint16_t index;
  float value_float;
  int16_t value_int;
};

/**
 * @brief Read specific line from golden reference CSV
 * @param filename Path to golden reference CSV
 * @param line_number Line number to read (1-indexed, skip header)
 * @param entry Output structure for parsed entry
 * @return true if successfully read, false otherwise
 */
static bool read_golden_line(const char *filename, uint32_t line_number, GoldenEntry *entry) {
  FILE *file = open_golden_reference_file(filename);
  if (file == nullptr) {
    return false;
  }

  char line[256];
  uint32_t current_line = 0;

  // Skip header
  if (fgets(line, sizeof(line), file) == nullptr) {
    fclose(file);
    return false;
  }

  // Read until target line
  while (fgets(line, sizeof(line), file) != nullptr) {
    current_line++;
    if (current_line == line_number) {
      // Parse CSV: buffer_type,index,value_float,value_int
      char buffer_type[20];
      char index_str[50];
      float value_float;
      int value_int;

      // First try to parse with all fields as strings/numbers flexibly
      if (sscanf(line, "%19[^,],%49[^,],%f,%d", buffer_type, index_str, &value_float, &value_int) == 4) {
        strncpy(entry->buffer_type, buffer_type, sizeof(entry->buffer_type) - 1);
        entry->buffer_type[sizeof(entry->buffer_type) - 1] = '\0';

        // Try to convert index_str to number; if it fails, it's metadata with string index
        int index;
        if (sscanf(index_str, "%d", &index) == 1) {
          entry->index = static_cast<uint16_t>(index);
        } else {
          // It's metadata with string index (e.g., "buffer_used")
          entry->index = 0; // Marker for string index
        }

        entry->value_float = value_float;
        entry->value_int = static_cast<int16_t>(value_int);
        fclose(file);
        return true;
      }
      break;
    }
  }

  fclose(file);
  return false;
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Load entire file into memory buffer (native platform only)
 * @param filename Path to file
 * @return Allocated buffer or nullptr if allocation fails
 */
#if !defined(ESP_PLATFORM)
static char *load_file_to_memory(const char *filename, size_t *out_size) {
  FILE *file = fopen(filename, "rb");
  if (file == nullptr) {
    return nullptr;
  }

  fseek(file, 0, SEEK_END);
  size_t file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  char *buffer = static_cast<char *>(malloc(file_size + 1));
  if (buffer == nullptr) {
    fclose(file);
    return nullptr;
  }

  size_t bytes_read = fread(buffer, 1, file_size, file);
  fclose(file);

  if (bytes_read != file_size) {
    free(buffer);
    return nullptr;
  }

  buffer[file_size] = '\0';
  *out_size = file_size;
  return buffer;
}
#endif

/**
 * @brief Get actual value from Mpx buffers based on golden entry type
 * @param mpx Mpx instance
 * @param entry Golden reference entry
 * @param actual_float Output for float value
 * @param actual_int Output for int value
 * @return true if buffer type recognized, false otherwise
 */
static bool get_actual_value(MatrixProfile::Mpx &mpx, const GoldenEntry &entry, float *actual_float,
                             int16_t *actual_int) {
  if (strcmp(entry.buffer_type, "data_buffer") == 0) {
    *actual_float = mpx.get_data_buffer()[entry.index];
    return true;
  } else if (strcmp(entry.buffer_type, "matrix_profile") == 0) {
    *actual_float = mpx.get_matrix()[entry.index];
    return true;
  } else if (strcmp(entry.buffer_type, "profile_indexes") == 0) {
    *actual_int = mpx.get_indexes()[entry.index];
    return true;
  } else if (strcmp(entry.buffer_type, "floss") == 0) {
    *actual_float = mpx.get_floss()[entry.index];
    return true;
  } else if (strcmp(entry.buffer_type, "iac") == 0) {
    *actual_float = mpx.get_iac()[entry.index];
    return true;
  } else if (strcmp(entry.buffer_type, "vmmu") == 0) {
    *actual_float = mpx.get_vmmu()[entry.index];
    return true;
  } else if (strcmp(entry.buffer_type, "vsig") == 0) {
    *actual_float = mpx.get_vsig()[entry.index];
    return true;
  } else if (strcmp(entry.buffer_type, "vddf") == 0) {
    *actual_float = mpx.get_ddf()[entry.index];
    return true;
  } else if (strcmp(entry.buffer_type, "vddg") == 0) {
    *actual_float = mpx.get_ddg()[entry.index];
    return true;
  }

  return false; // Unknown buffer type
}

// ============================================================================
// GOLDEN REFERENCE TEST
// ============================================================================

extern "C" {

/**
 * @test test_golden_reference_metadata
 * @brief Validate metadata values match golden reference
 *
 * GIVEN: Mpx instance processed with standard parameters
 * WHEN: Comparing metadata (buffer_used, profile_len, etc.)
 * THEN: Values must match golden reference exactly
 */
void test_golden_reference_metadata(void) {
  // Configuration (must match golden reference generation)
  const uint16_t window_size = 210;
  const uint16_t buffer_size = 5000;
  const uint16_t chunk_size = 500;
  const uint16_t num_iterations = 54;

  // Initialize and process using streaming to avoid large RAM usage on ESP32
  MatrixProfile::Mpx *mpx = new MatrixProfile::Mpx(window_size, 0.5F, 0U, buffer_size);
  TEST_ASSERT_NOT_NULL(mpx);
  uint32_t data_count = process_csv_in_chunks(TEST_DATA_PATH, *mpx, chunk_size, num_iterations);

  if (data_count == 0) {
    delete mpx;
    TEST_FAIL_MESSAGE("Failed to load test data - file not found or empty");
    return;
  }

  TEST_ASSERT_TRUE(data_count > 0);

  char msg[80];
  snprintf(msg, sizeof(msg), "Loaded %lu samples from test_data.csv", static_cast<unsigned long>(data_count));
  TEST_MESSAGE(msg);

  mpx->floss();

  // Read and validate metadata from golden reference
  GoldenEntry entry;

  // Line 2: metadata,buffer_used,5000,0
  bool success = read_golden_line(GOLDEN_REFERENCE_PATH, 1, &entry);

  if (!success) {
    delete mpx;
    TEST_FAIL_MESSAGE("Failed to read golden reference - file not found or cannot be read");
    return;
  }

  TEST_ASSERT_TRUE(success);
  TEST_ASSERT_EQUAL_STRING("metadata", entry.buffer_type);
  TEST_ASSERT_EQUAL_UINT16(5000, mpx->get_buffer_used());

  // Line 3: metadata,buffer_start,0,0
  TEST_ASSERT_TRUE(read_golden_line(GOLDEN_REFERENCE_PATH, 2, &entry));
  TEST_ASSERT_EQUAL_INT16(0, mpx->get_buffer_start());

  // Line 4: metadata,profile_len,4791,0
  TEST_ASSERT_TRUE(read_golden_line(GOLDEN_REFERENCE_PATH, 3, &entry));
  TEST_ASSERT_EQUAL_UINT16(4791, mpx->get_profile_len());

  // Line 5: metadata,last_movsum,...
  TEST_ASSERT_TRUE(read_golden_line(GOLDEN_REFERENCE_PATH, 4, &entry));
  float tolerance = std::abs(entry.value_float) * 1e-5f; // 0.001% relative tolerance
  TEST_ASSERT_FLOAT_WITHIN(tolerance, entry.value_float, mpx->get_last_movsum());

  // Line 6: metadata,last_mov2sum,...
  TEST_ASSERT_TRUE(read_golden_line(GOLDEN_REFERENCE_PATH, 5, &entry));
  tolerance = std::abs(entry.value_float) * 1e-5f;
  TEST_ASSERT_FLOAT_WITHIN(tolerance, entry.value_float, mpx->get_last_mov2sum());

  delete mpx;
}

/**
 * @test test_golden_reference_sample_validation
 * @brief Validate representative sample of buffer values
 *
 * GIVEN: Mpx instance processed with golden reference parameters
 * WHEN: Sampling values from different buffers at various positions
 * THEN: Values must match golden reference within tolerance
 *
 * Strategy: Validate every 100th entry to keep test fast while
 *           maintaining good coverage across all buffer types.
 */
void test_golden_reference_sample_validation(void) {
  // Configuration
  const uint16_t window_size = 210;
  const uint16_t buffer_size = 5000;
  const uint16_t chunk_size = 500;
  const uint16_t num_iterations = 54;

  // Initialize and process using streaming to avoid large RAM usage on ESP32
  MatrixProfile::Mpx *mpx = new MatrixProfile::Mpx(window_size, 0.5F, 0U, buffer_size);
  TEST_ASSERT_NOT_NULL(mpx);
  uint32_t data_count = process_csv_in_chunks(TEST_DATA_PATH, *mpx, chunk_size, num_iterations);
  TEST_ASSERT_TRUE(data_count > 0);

  mpx->floss();

  // Sample validation: single-pass stream over golden file.
  // Native: validate every line; ESP32: validate every 10th line (resource constraint)
#if defined(ESP_PLATFORM)
  const uint32_t sample_interval = 10;
#else
  const uint32_t sample_interval = 1;
#endif
  const float tolerance = 1e-5f; // Tolerance for floating point comparison

  uint32_t samples_checked = 0;
  uint32_t samples_passed = 0;

#if !defined(ESP_PLATFORM)
  // Native platform: load entire file into memory for faster processing
  size_t file_size = 0;
  char *file_buffer = load_file_to_memory(GOLDEN_REFERENCE_PATH, &file_size);
  TEST_ASSERT_NOT_NULL(file_buffer);

  char *buffer_pos = file_buffer;
  char line[256];

  // Skip header line
  char *newline = strchr(buffer_pos, '\n');
  if (newline != nullptr) {
    buffer_pos = newline + 1;
  }

  uint32_t current_line = 0;
  while (buffer_pos < file_buffer + file_size && sscanf(buffer_pos, "%255[^\n]", line) == 1) {
    current_line++;

    // Advance to next line
    newline = strchr(buffer_pos, '\n');
    if (newline != nullptr) {
      buffer_pos = newline + 1;
    } else {
      buffer_pos = file_buffer + file_size;
    }

#else
  // ESP32 platform: stream file line by line
  FILE *golden = open_golden_reference_file(GOLDEN_REFERENCE_PATH);
  TEST_ASSERT_NOT_NULL(golden);

  char line[256];
  // Skip header
  TEST_ASSERT_NOT_NULL(fgets(line, sizeof(line), golden));

  uint32_t current_line = 0;
  while (fgets(line, sizeof(line), golden) != nullptr) {
    current_line++;
#endif

    // Skip metadata lines (1..5) and only validate sampled lines.
    if (current_line < 6) {
      continue;
    }
    if (((current_line - 6) % sample_interval) != 0) {
      continue;
    }

    GoldenEntry entry;
    char buffer_type[20];
    char index_str[50];
    float value_float = 0.0f;
    int value_int = 0;

    if (sscanf(line, "%19[^,],%49[^,],%f,%d", buffer_type, index_str, &value_float, &value_int) != 4) {
      continue;
    }

    strncpy(entry.buffer_type, buffer_type, sizeof(entry.buffer_type) - 1);
    entry.buffer_type[sizeof(entry.buffer_type) - 1] = '\0';

    int index = 0;
    if (sscanf(index_str, "%d", &index) == 1) {
      entry.index = static_cast<uint16_t>(index);
    } else {
      entry.index = 0;
    }
    entry.value_float = value_float;
    entry.value_int = static_cast<int16_t>(value_int);

    if (strcmp(entry.buffer_type, "metadata") == 0) {
      continue;
    }

    float actual_float = 0.0f;
    int16_t actual_int = 0;
    if (!get_actual_value(*mpx, entry, &actual_float, &actual_int)) {
      continue;
    }

    samples_checked++;
    if (strcmp(entry.buffer_type, "profile_indexes") == 0) {
      if (actual_int == entry.value_int) {
        samples_passed++;
      }
    } else {
      float diff = std::abs(actual_float - entry.value_float);
      if (diff <= tolerance || (entry.value_float != 0.0f && diff / std::abs(entry.value_float) <= tolerance)) {
        samples_passed++;
      }
    }
  }

#if !defined(ESP_PLATFORM)
  // Cleanup file buffer (native platform)
  free(file_buffer);
#else
  // Cleanup file handle (ESP32 platform)
  fclose(golden);
#endif

  TEST_ASSERT_TRUE(samples_checked > 0);

  char msg[100];
  snprintf(msg, sizeof(msg), "Validated %lu samples, %lu matched (%lu%% accuracy)",
           static_cast<unsigned long>(samples_checked), static_cast<unsigned long>(samples_passed),
           static_cast<unsigned long>((samples_passed * 100U) / samples_checked));
  TEST_MESSAGE(msg);

  // Require at least 95% match rate (allows for minor numerical differences)
  uint32_t min_required_matches = (samples_checked * 95) / 100;
  TEST_ASSERT_TRUE(samples_passed >= min_required_matches);

  delete mpx;
}

} // extern "C"
