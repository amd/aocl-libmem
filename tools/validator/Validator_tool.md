# Testing Guide for **_AOCL-LibMem Validator Tools_**

## Description
- AOCL-LibMem Validator Framework is used for functional validation of LibMem supported functions. It performs data validation check, which checks the data integrity of the source and destination memory.
- The framework provides three validator implementations:
  - **Legacy Validator** (`libmem_validator`): Original C implementation
  - **Unified Validator** (`libmem_validator_unified`): Refactored C++ framework with template-based architecture and registry for dynamic dispatch
  - **GTest Validator** (`libmem_validator_gtest`): GTest-style test runner for interactive debugging and fine-grained test control

- Users can perform validation testing using any of the methods mentioned below:
   1. Testing with Validator script (`build_dir/test/validator.py`)
   2. Testing with validator binaries directly (`libmem_validator`, `libmem_validator_unified`, or `libmem_validator_gtest`)
   3. Testing with ctest utility (`build_dir/test $ ctest`)

---

# Legacy Validator (`libmem_validator`)

## Debug Mode

The legacy validator supports a debug mode that provides detailed information about the testing process, including vector sizes, function names, and alignment combinations being tested.

### Enabling Debug Mode

To build the validator with debug mode enabled, use the CMake option:

```bash
# Navigate to your build directory
cd /path/to/build/directory

# Configure with debug mode enabled
cmake -DLIBMEM_VALIDATOR_DEBUG=ON .

# Build the project
make
```

### Disabling Debug Mode

To build the validator without debug output (normal mode):

```bash
# Navigate to your build directory
cd /path/to/build/directory

# Configure with debug mode disabled
cmake -DLIBMEM_VALIDATOR_DEBUG=OFF .

# Build the project
make
```

### Debug Output Information

When debug mode is enabled, the validator will output the following information:

- **VEC_SZ**: Vector size in bytes (32 for AVX2, 64 for AVX512)
- **Function name**: Which function is being tested
- **Test size**: The size parameter being validated
- **Alignment mode**: Whether testing single alignment or all alignment combinations
- **Individual alignment tests**: Each source/destination alignment combination (when using alignment check mode)

### Example Debug Output

```
[DEBUG] libmem_validator started
[DEBUG] VEC_SZ = 64 bytes
[DEBUG] Function: memcpy
[DEBUG] Size: 1024
[DEBUG] Alignment check mode: All alignments
[DEBUG] Testing alignment - src: 0, dst: 0
[DEBUG] Testing alignment - src: 0, dst: 1
[DEBUG] Testing alignment - src: 0, dst: 2
...
[DEBUG] Testing alignment - src: 63, dst: 63
```

### Usage Examples

```bash
# Test with debug output enabled
./tools/validator/libmem_validator memcpy 1024 0 0 1

# Test without debug output (clean run)
./tools/validator/libmem_validator memcpy 1024 0 0 0
```

**Note**: Debug mode is used only for debugging validator code.

## Validator Script

- By default the tool checks for validation of standard sizes with source and destination alignment as cache line SIZE. However, for non-standard sizes the user can pass the size range along with the -t <iterator> value for validating the target sizes with different combinations of source and destination alignment.
- For the given Input SIZE the tool shows the Validation result as Passed / Failed.

### Report Description
  - Results will be generated in the format of csv file.
  - Validation report will be stored in this PATH
```
build_dir/test/out/<libmem_function>/<time-stamp-counter>/<validation_report.csv>
```

### Arguments for the Script

```
$ ./validator.py -r [start] [end] -a[src] [dst] -t[iterator count] <mem_function>
    -r Range       = [Start] and [End] range in Bytes.
    -a alignment   = [src] and [dst] alignments. Default alignment is 64B for both source and destination.
    -t <iterator>  = specify the iteration pattern. Default is "2x" of starting size - '<<1'
    <function>     = memcpy,memset,memcmp,memmove,mempcpy,memchr,
                     strcpy,strncpy,strcmp,strncmp,strlen,strnlen,
                     strcat,strncat,strstr,strspn,strchr
```

### Example
```
Sample Run: $./validator.py -r 8 64 -a 5 8 -t"+1" memcpy
Performs the Validation testing starting from 8 Bytes --> 64 Bytes
with Src Alignment: 5 & Dst Alignment: 8
and iterator pattern as "+1".

> Validation for size [8] in progress...
> Validation for size [9] in progress...
> Validation for size [10] in progress...
...
```

The tester can provide different options as needed. Please refer to the help message of the python script to set up the validator config.
```sh
$ ./validator.py -h
```

## Ctest Utility (Legacy)

LibMem ctest for unit testing and validation of LibMem functions. It can be invoked from BUILD_DIR/test directory.

The following test categories are supported:
- Iterator testing (increments by 1B till Page_Size)  [0B-4KB]
- Shift testing (Tests for standard sizes)  [8B,16B,32B...32MB]
- Alignment testing (Modifies the src and dst alignment)  [1B-4KB]   src [0-63]   dst [0-63]
- Threshold testing (AMD-specific) for L2/NT boundaries
- ZEN5 aligned vector threshold testing (when applicable)

### Report Description
  - ctest validation report will be stored in this PATH
    ```
    build_dir/test/Testing/Temporary/LastTest.log
    ```
  - storing the test logs with Timestamp
  ```sh
  $ ctest -R "memcpy_shift" -O ctest_results_$(date +%Y_%m_%d_%H%M%S).log
  ```

### Arguments for ctest

```
$ ctest -R <Matching test regex> -E <Exclude regex> -j [<jobs>]
<regex>:      function_[MODE]
              "memcpy","memset","memmove","memcmp","mempcpy",
              "strcpy","strncpy","strcmp","strncmp","strlen",
              "strnlen","strspn"                     (Runs all the test cases iter,shift and alignment)
              "iter"                                  (Runs Iterator test cases on all the functions)
              "shift"                                 (Runs Shift test cases on all the functions)
              "align"                                 (Runs Alignment test cases on all the functions)
              "threshold"                             (Runs threshold specific test cases based on L2 and threshold_nt values)
              "aligned_vector"                        (Runs ZEN5 specific aligned vector threshold checks)
              "function_MODE"                         (Runs the specified MODE:iter,shift,align, and threshold on the input function)

jobs:         Optional parameter for maximum no. of concurrent processes to be used. [0...$(nproc)]
```

### For Minimized logs
Storing only failure logs
```sh
$ ctest --progress --output-on-failure -j $(nproc)
```

### Examples

1. Running ctest for all modes and all functions (default config)
```sh
$ ctest
```
2. Running ctest for memcpy all modes
```sh
$ ctest -R "memcpy" -j $(nproc)
```
3. Running ctest for memcmp align mode
```sh
$ ctest -R "memcmp_align" -j $(nproc)
```
4. Running ctest for memmove iterative mode
```sh
$ ctest -R "memmove_iter" -j $(nproc)
```
5. Running ctest for memset shift mode
```sh
$ ctest -R "memset_shift" -j $(nproc)
```
6. Running with multiple inputs
```sh
$ ctest -R "memcpy|memmove|strcpy_iter"
```
7. Running threshold checks
```sh
$ ctest -R "memcpy_threshold"
```
8. Using Exclude regex
```sh
$ ctest -R "memcpy|strcpy" -E "iter|shift"   //Runs memcpy_align and strcpy_align; ignores iter and shift test cases.
```
9. Test log reports with timestamp
```sh
$ ctest -R "memcpy_shift" -O ctest_results_$(date +%Y_%m_%d_%H%M%S).log
```

Notes:
1. Running ctest on Memcmp, Strcmp and Strncmp takes some extra time (as we are trying to compare all the Bytes).

---

# Unified Validator (`libmem_validator_unified`)

## Verbose Mode

The unified validator supports a verbose mode that provides detailed information about the testing process, including vector sizes, function names, and alignment combinations being tested.

### Enabling Verbose Mode

To build the unified validator with verbose output enabled:

```bash
# Navigate to your build directory
cd /path/to/build/directory

# Configure with verbose mode enabled (unified validator)
cmake -DUNIFIED_VALIDATOR_VERBOSE=ON .

# Build the project
make
```

### Disabling Verbose Mode

To build without verbose output (normal mode):

```bash
# Navigate to your build directory
cd /path/to/build/directory

# Configure with verbose mode disabled (unified validator)
cmake -DUNIFIED_VALIDATOR_VERBOSE=OFF .

# Build the project
make
```

### Verbose Output Information

When verbose mode is enabled, the validator will output the following information:

- **VEC_SZ**: Vector size in bytes (32 for AVX2, 64 for AVX512)
- **Function name**: Which function is being tested
- **Test size**: The size parameter being validated
- **Alignment mode**: Whether testing single alignment or all alignment combinations
- **Individual alignment tests**: Each source/destination alignment combination (when using alignment check mode)

### Example Verbose Output

```
[DEBUG] libmem_validator started
[DEBUG] VEC_SZ = 64 bytes
[DEBUG] Function: memcpy
[DEBUG] Size: 1024
[DEBUG] Alignment check mode: All alignments
[DEBUG] Testing alignment - src: 0, dst: 0
[DEBUG] Testing alignment - src: 0, dst: 1
[DEBUG] Testing alignment - src: 0, dst: 2
...
[DEBUG] Testing alignment - src: 63, dst: 63
```

## Command Line Interface

```bash
libmem_validator_unified <function> <size> [src_align] [dst_align] [all_alignments]
```

Arguments:

- **function**: `memcpy`, `mempcpy`, `memmove`, `memset`, `memcmp`, `memchr`,
  `strcpy`, `strncpy`, `strcmp`, `strncmp`, `strlen`, `strnlen`, `strcat`, `strncat`,
  `strstr`, `strspn`, `strchr`
- **size**: Size parameter for the function
- **src_align**: Source alignment offset (default: `0`)
- **dst_align**: Destination alignment offset (default: `0`)
- **all_alignments**: `1` to test all src/dst alignment combinations

Options:

- `--list-functions`: List all supported functions
- `--list-tests <function>`: List tests available for a function
- `--help`: Show usage

### Usage Examples

```bash
# Test with verbose output enabled
./tools/validator/libmem_validator_unified memcpy 1024 0 0 1

# Test without verbose output (clean run)
./tools/validator/libmem_validator_unified memcpy 1024 0 0 0

# List supported functions
./tools/validator/libmem_validator_unified --list-functions

# Show registered tests for a function
./tools/validator/libmem_validator_unified --list-tests memcpy

# Run all alignment combinations for a small size
./tools/validator/libmem_validator_unified memset 64 0 0 1
```

## Ctest Utility (Unified)

LibMem ctest can execute unified validator tests using the `unified_` prefix.

The following test categories are supported:
- Iterator testing (increments by 1B till Page_Size)  [0B-4KB]
- Shift testing (Tests for standard sizes)  [1B,2B,4B...32MB]
- Alignment testing (Modifies the src and dst alignment)  [0B-4KB]   src [0-63]   dst [0-63]
- Threshold testing (AMD-specific) for L2/NT boundaries
- ZEN5 aligned vector threshold testing (when applicable)

### Examples

```sh
# Unified tests for memcpy (all modes)
$ ctest -R "unified_memcpy" -j $(nproc)

# Unified iterator tests across all functions
$ ctest -R "unified_.*_iter" -j $(nproc)

# Unified alignment tests for memcmp
$ ctest -R "unified_memcmp_align" -j $(nproc)

# Unified threshold checks
$ ctest -R "unified_memcpy_thresholdL2|unified_memcpy_threshold_nt"

# Unified shift tests with timestamped logs
$ ctest -R "unified_memcpy_shift" -O ctest_results_$(date +%Y_%m_%d_%H%M%S).log
```
Notes:
1. Running ctest on Memcmp, Strcmp and Strncmp takes some extra time (as we are trying to compare all the Bytes).

---

# GTest Validator (`libmem_validator_gtest`)

The GTest validator provides a GTest-style command-line interface for running tests dynamically. It allows running specific tests with any function, size, and alignment combination, producing output in a familiar GTest format.

## Command Line Interface

```bash
libmem_validator_gtest <function> [size] [src_align] [dst_align] [all_alignments] [--test=<name>]
```

Options:

- `--test=<name>`: Run only the specified test (e.g., `--test=BasicCopy`)
- `--list-tests`: List all available tests for all functions
- `--help`: Show usage

## Usage Examples

```bash
# Run all memcpy tests at size 64
./tools/validator/libmem_validator_gtest memcpy 64

# Run specific test for memmove
./tools/validator/libmem_validator_gtest memmove 40 --test=BackwardOverlap

# Run with specific alignments
./tools/validator/libmem_validator_gtest memcpy 128 5 3

# Run all alignment combinations (4096 tests per test case)
./tools/validator/libmem_validator_gtest memcpy 128 0 0 1

# List all available tests
./tools/validator/libmem_validator_gtest --list-tests
```
