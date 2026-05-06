/**
 * @file generate_golden_reference.cpp
 * @brief Generate golden reference CSV for regression testing
 *
 * This program processes test data using specific parameters and saves
 * all internal buffer states to a CSV file. This CSV serves as the
 * "golden reference" (ground truth) for regression testing.
 *
 * USAGE:
 *   1. Compile: pio run -e native -t clean && pio run -e native
 *   2. Run manually or copy to test/ folder
 *
 * CONFIGURATION:
 *   Parameters match the golden reference test expectations:
 *   - window_size: 210
 *   - buffer_size: 5000
 *   - chunk_size: 500
 *   - num_iterations: 54 (processes 27,000 samples)
 *
 * INPUT:  test/test_data.csv (must contain at least 27,000 samples)
 * OUTPUT: test/golden_reference.csv
 *
 * @note Update test/golden_reference_nodelete.csv after verification
 */

#include <Mpx.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>

/**
 * @brief Read CSV file with float data (one value per line after header)
 * @param filename Path to CSV file
 * @return Vector of float values
 */
std::vector<float> read_csv_data(const char *filename) {
  std::vector<float> data;

  FILE *file = fopen(filename, "r");
  if (file == nullptr) {
    std::cerr << "ERROR: Could not open " << filename << std::endl;
    return data;
  }

  char line[256];
  bool skip_header = true;

  while (fgets(line, sizeof(line), file) != nullptr) {
    if (skip_header) {
      skip_header = false;
      continue;
    }

    // Remove quotes and whitespace
    char *start = line;
    while (*start == '"' || *start == ' ' || *start == '\t') {
      start++;
    }

    // Parse float value
    float value = 0.0f;
    if (sscanf(start, "%f", &value) == 1) {
      data.push_back(value);
    }
  }

  fclose(file);
  return data;
}

/**
 * @brief Write golden reference CSV file
 */
bool write_golden_reference(const char *filename, MatrixProfile::Mpx &mpx) {
  FILE *file = fopen(filename, "w");
  if (file == nullptr) {
    std::cerr << "ERROR: Could not create " << filename << std::endl;
    return false;
  }

  // Get final state
  uint16_t buffer_used = mpx.get_buffer_used();
  int16_t buffer_start = mpx.get_buffer_start();
  uint16_t profile_len = mpx.get_profile_len();
  float last_movsum = mpx.get_last_movsum();
  float last_mov2sum = mpx.get_last_mov2sum();

  // Get all buffers
  float *data_buffer = mpx.get_data_buffer();
  float *matrix = mpx.get_matrix();
  int16_t *indexes = mpx.get_indexes();
  float *floss = mpx.get_floss();
  float *iac = mpx.get_iac();
  float *vmmu = mpx.get_vmmu();
  float *vsig = mpx.get_vsig();
  float *vddf = mpx.get_ddf();
  float *vddg = mpx.get_ddg();

  // Write header
  fprintf(file, "buffer_type,index,value_float,value_int\n");

  // Write metadata
  fprintf(file, "metadata,buffer_used,%u,0\n", buffer_used);
  fprintf(file, "metadata,buffer_start,%d,0\n", buffer_start);
  fprintf(file, "metadata,profile_len,%u,0\n", profile_len);
  fprintf(file, "metadata,last_movsum,%.10f,0\n", last_movsum);
  fprintf(file, "metadata,last_mov2sum,%.10f,0\n", last_mov2sum);

  // Write data_buffer (only used portion)
  for (uint16_t i = 0; i < buffer_used; i++) {
    fprintf(file, "data_buffer,%u,%.10f,0\n", i, data_buffer[i]);
  }

  // Write matrix profile
  for (uint16_t i = 0; i < profile_len; i++) {
    fprintf(file, "matrix_profile,%u,%.10f,0\n", i, matrix[i]);
  }

  // Write profile indexes
  for (uint16_t i = 0; i < profile_len; i++) {
    fprintf(file, "profile_indexes,%u,0.0,%d\n", i, indexes[i]);
  }

  // Write FLOSS
  for (uint16_t i = 0; i < profile_len; i++) {
    fprintf(file, "floss,%u,%.10f,0\n", i, floss[i]);
  }

  // Write IAC (Ideal Arc Counts)
  for (uint16_t i = 0; i < profile_len; i++) {
    fprintf(file, "iac,%u,%.10f,0\n", i, iac[i]);
  }

  // Write VMMU (moving mean)
  for (uint16_t i = 0; i < profile_len; i++) {
    fprintf(file, "vmmu,%u,%.10f,0\n", i, vmmu[i]);
  }

  // Write VSIG (moving 1/sigma)
  for (uint16_t i = 0; i < profile_len; i++) {
    fprintf(file, "vsig,%u,%.10f,0\n", i, vsig[i]);
  }

  // Write VDDF (first differential)
  for (uint16_t i = 0; i < profile_len; i++) {
    fprintf(file, "vddf,%u,%.10f,0\n", i, vddf[i]);
  }

  // Write VDDG (second differential)
  for (uint16_t i = 0; i < profile_len; i++) {
    fprintf(file, "vddg,%u,%.10f,0\n", i, vddg[i]);
  }

  fclose(file);

  // Count total entries
  uint32_t total_entries = 5 + buffer_used + (profile_len * 9);
  std::cout << "   ✓ Total entries written: " << total_entries << std::endl;

  return true;
}

int main() {
  std::cout << "\n╔════════════════════════════════════════════════════╗" << std::endl;
  std::cout << "║   Golden Reference Generator                       ║" << std::endl;
  std::cout << "║   Matrix Profile Regression Testing                ║" << std::endl;
  std::cout << "╚════════════════════════════════════════════════════╝\n" << std::endl;

  // Configuration (must match test expectations)
  const uint16_t window_size = 210;
  const uint16_t buffer_size = 5000;
  const uint16_t chunk_size = 500;
  const uint16_t num_iterations = 54;
  const uint32_t min_samples_needed = num_iterations * chunk_size;

  std::cout << "Configuration:" << std::endl;
  std::cout << "  - Window size: " << window_size << std::endl;
  std::cout << "  - Buffer size: " << buffer_size << std::endl;
  std::cout << "  - Chunk size: " << chunk_size << std::endl;
  std::cout << "  - Iterations: " << num_iterations << std::endl;
  std::cout << "  - Samples needed: " << min_samples_needed << std::endl;
  std::cout << std::endl;

  // Load test data
  std::cout << "1. Loading test data from test/test_data.csv..." << std::endl;
  std::vector<float> data = read_csv_data("test/test_data.csv");

  if (data.empty()) {
    std::cerr << "   ERROR: No data loaded from test/test_data.csv" << std::endl;
    std::cerr << "   Please ensure the file exists and is properly formatted." << std::endl;
    return 1;
  }
  std::cout << "   ✓ Loaded " << data.size() << " samples" << std::endl;

  if (data.size() < min_samples_needed) {
    std::cerr << "   WARNING: Dataset has only " << data.size() << " samples" << std::endl;
    std::cerr << "            Need at least " << min_samples_needed << " for full processing" << std::endl;
    std::cerr << "            Will process what's available..." << std::endl;
  }

  // Initialize Mpx
  std::cout << "\n2. Initializing Matrix Profile (Mpx)..." << std::endl;
  MatrixProfile::Mpx mpx(window_size, 0.5f, 0, buffer_size);
  std::cout << "   ✓ Mpx initialized" << std::endl;
  std::cout << "   - Profile length will be: " << mpx.get_profile_len() << std::endl;

  // Process data in chunks
  std::cout << "\n3. Processing data in chunks..." << std::endl;
  uint16_t chunks_processed = 0;

  for (uint16_t iter = 0; iter < num_iterations; iter++) {
    uint32_t offset = static_cast<uint32_t>(iter) * chunk_size;

    if (offset + chunk_size > data.size()) {
      std::cout << "   ⚠ Reached end of data at iteration " << (iter + 1) << std::endl;
      break;
    }

    uint16_t free_space = mpx.compute(&data[offset], chunk_size);
    chunks_processed++;

    if ((iter + 1) % 10 == 0 || (iter + 1) == num_iterations) {
      std::cout << "   ✓ Processed " << (iter + 1) << "/" << num_iterations << " chunks (buffer free: " << free_space
                << ")" << std::endl;
    }
  }

  if (chunks_processed < num_iterations) {
    std::cout << "   ⚠ Only processed " << chunks_processed << "/" << num_iterations << " chunks" << std::endl;
  } else {
    std::cout << "   ✓ All " << chunks_processed << " chunks processed successfully" << std::endl;
  }

  // Compute FLOSS
  std::cout << "\n4. Computing FLOSS (Fast Low-cost Online Semantic Segmentation)..." << std::endl;
  mpx.floss();
  std::cout << "   ✓ FLOSS computed" << std::endl;

  // Display final state
  std::cout << "\n5. Final state:" << std::endl;
  std::cout << "   - Buffer used: " << mpx.get_buffer_used() << "/" << buffer_size << std::endl;
  std::cout << "   - Buffer start: " << mpx.get_buffer_start() << std::endl;
  std::cout << "   - Profile length: " << mpx.get_profile_len() << std::endl;
  std::cout << "   - Last movsum: " << std::fixed << std::setprecision(6) << mpx.get_last_movsum() << std::endl;
  std::cout << "   - Last mov2sum: " << std::fixed << std::setprecision(6) << mpx.get_last_mov2sum() << std::endl;

  // Write golden reference
  std::cout << "\n6. Writing golden reference to test/golden_reference.csv..." << std::endl;
  bool success = write_golden_reference("test/golden_reference.csv", mpx);

  if (!success) {
    std::cerr << "   ERROR: Failed to write golden reference file" << std::endl;
    return 1;
  }
  std::cout << "   ✓ Golden reference saved to test/golden_reference.csv" << std::endl;

  std::cout << "\n╔════════════════════════════════════════════════════╗" << std::endl;
  std::cout << "║   Golden reference generated successfully!         ║" << std::endl;
  std::cout << "╠════════════════════════════════════════════════════╣" << std::endl;
  std::cout << "║   NEXT STEPS:                                      ║" << std::endl;
  std::cout << "║   1. Review test/golden_reference.csv              ║" << std::endl;
  std::cout << "║   2. Run tests to validate:                        ║" << std::endl;
  std::cout << "║      platformio test -e native                     ║" << std::endl;
  std::cout << "║   3. If validated, update:                         ║" << std::endl;
  std::cout << "║      test/golden_reference_nodelete.csv            ║" << std::endl;
  std::cout << "╚════════════════════════════════════════════════════╝\n" << std::endl;

  return 0;
}
