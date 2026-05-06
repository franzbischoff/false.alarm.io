# Examples - false.alarm.io

This directory contains example programs and utilities for the false.alarm.io project.

## Files

### generate_golden_reference.cpp

**Purpose**: Generate CSV reference file for regression testing.

**Usage**:
```bash
# Option 1: Compile and run through PlatformIO
# (Note: Requires additional configuration in platformio.ini)
pio run -e native

# Option 2: Compile manually (recommended)
g++ -std=c++17 -I../lib/Mpx/include -o generate_golden examples/generate_golden_reference.cpp -L. -lMpx

# Run
./generate_golden
```

**Input**: `test/test_data.csv` (minimum 27,000 samples)

**Output**: `test/golden_reference.csv` (~43K lines)

**Parameters**:
- `window_size`: 210
- `buffer_size`: 5000
- `chunk_size`: 500
- `num_iterations`: 54 (processes 27,000 samples)

**Update Workflow**:
1. Run the generator: `./generate_golden`
2. Validate the generated file: `platformio test -e native`
3. If tests pass (100% match), update the reference:
   ```bash
   cp test/golden_reference.csv test/golden_reference_nodelete.csv
   ```
4. Commit the new reference if necessary

**Note**: The `golden_reference_nodelete.csv` file is used by Unity tests and should not be automatically overwritten. Only update after full validation.

## How to Add New Examples

1. Create a `.cpp` file in this folder
2. Include `#include <Mpx.hpp>` to use the library
3. Document the purpose and usage in the file header
4. Update this README.md with information about the new example

## Compilation

All examples should:
- Use C++17 (`-std=c++17`)
- Include `lib/Mpx/include` in the include path
- Link with the compiled Mpx library
