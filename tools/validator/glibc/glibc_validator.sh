# Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
# 3. Neither the name of the copyright holder nor the names of its contributors
#    may be used to endorse or promote products derived from this software without
#    specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
# OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

#!/bin/bash
#
# Comprehensive script to download, patch, compile and run glibc tests with LD_PRELOAD
# Downloads fresh glibc tests and applies all necessary fixes

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Determine paths relative to script location
# Script is in: build/tools/validator/glibc/
# We need to go up to build/ directory
BUILD_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"

# Set paths relative to build directory
GLIBC_TESTS_DIR="$BUILD_DIR/test/glibc-functionality-tests"
GLIBC_EXECUTABLES_DIR="$GLIBC_TESTS_DIR/bin"
LIB_DIR="$BUILD_DIR/lib"

# Find repository root for CMakeLists.txt (one level up from build)
REPO_ROOT="$(cd "$BUILD_DIR/.." && pwd)"

# Find the shared library (version-agnostic - finds versioned or symlink)
SHARED_LIB=$(find "$LIB_DIR" -name "libaocl-libmem.so*" -type f | head -1)

echo "Comprehensive glibc test integration with LD_PRELOAD"
echo "=================================================="

# Check if shared library exists
if [ ! -f "$SHARED_LIB" ]; then
    echo "ERROR: Shared library not found in $LIB_DIR/"
    echo "Please build the library first with: cmake --build ."
    exit 1
fi

echo "Found shared library: $SHARED_LIB"

# Create tests and executables directories
mkdir -p "$GLIBC_TESTS_DIR"
mkdir -p "$GLIBC_EXECUTABLES_DIR"

# Read supported functions from src/CMakeLists.txt
CMAKE_FILE="$REPO_ROOT/src/CMakeLists.txt"
if [ -f "$CMAKE_FILE" ]; then
    # Extract SUPP_MEM_FUNCS from CMakeLists.txt
    SUPP_MEM_FUNCS=$(grep "set(SUPP_MEM_FUNCS" "$CMAKE_FILE" | sed 's/set(SUPP_MEM_FUNCS//' | sed 's/)//' | tr -d '\n')
    # Extract SUPP_STR_FUNCS from CMakeLists.txt
    SUPP_STR_FUNCS=$(grep "set(SUPP_STR_FUNCS" "$CMAKE_FILE" | sed 's/set(SUPP_STR_FUNCS//' | sed 's/)//' | tr -d '\n')
    
    # Combine both arrays
    read -ra MEM_FUNCTIONS <<< "$SUPP_MEM_FUNCS"
    read -ra STR_FUNCTIONS <<< "$SUPP_STR_FUNCS"
    FUNCTIONS=("${MEM_FUNCTIONS[@]}" "${STR_FUNCTIONS[@]}")
    
    echo "Read ${#MEM_FUNCTIONS[@]} memory functions from $CMAKE_FILE: ${MEM_FUNCTIONS[*]}"
    echo "Read ${#STR_FUNCTIONS[@]} string functions from $CMAKE_FILE: ${STR_FUNCTIONS[*]}"
    echo "Total ${#FUNCTIONS[@]} functions to test: ${FUNCTIONS[*]}"
else
    echo "WARNING: $CMAKE_FILE not found, using fallback list"
    # Fallback list if CMakeLists.txt is not found
    FUNCTIONS=(
        "memcpy" "mempcpy" "memmove" "memset" "memcmp" "memchr"
        "strcpy" "strncpy" "strcmp" "strncmp" "strcat" "strncat" "strstr" "strlen" "strchr"
    )
fi

echo ""
echo "Downloading fresh glibc test files..."
echo "========================================="

# Download test files from glibc repository
for func in "${FUNCTIONS[@]}"; do
    test_file="${GLIBC_TESTS_DIR}/test-${func}.c"
    if [ -f "$test_file" ]; then
        echo "test-${func}.c already exists, skipping download"
    else
        echo "Downloading test-${func}.c..."
        curl -s "https://sourceware.org/git/?p=glibc.git;a=blob_plain;hb=HEAD;f=string/test-${func}.c" \
             -o "$test_file"
        
        if [ -f "$test_file" ]; then
            echo "Downloaded test-${func}.c"
        else
            echo "Failed to download test-${func}.c"
        fi
    fi
done

# Download test-string.h
test_string_file="${GLIBC_TESTS_DIR}/test-string.h"
DOWNLOADED_FRESH=false
if [ -f "$test_string_file" ]; then
    echo "test-string.h already exists, skipping download"
else
    echo "Downloading test-string.h..."
    curl -s "https://sourceware.org/git/?p=glibc.git;a=blob_plain;hb=HEAD;f=string/test-string.h" \
         -o "$test_string_file"

    if [ -f "$test_string_file" ]; then
        echo "Downloaded test-string.h"
        DOWNLOADED_FRESH=true
    else
        echo "Failed to download test-string.h"
        exit 1
    fi
fi

# Download test-memcpy-support.h
test_memcpy_support_file="${GLIBC_TESTS_DIR}/test-memcpy-support.h"
if [ -f "$test_memcpy_support_file" ]; then
    echo "test-memcpy-support.h already exists, skipping download"
else
    echo "Downloading test-memcpy-support.h..."
    curl -s "https://sourceware.org/git/?p=glibc.git;a=blob_plain;hb=HEAD;f=string/test-memcpy-support.h" \
         -o "$test_memcpy_support_file"

    if [ -f "$test_memcpy_support_file" ]; then
        echo "Downloaded test-memcpy-support.h"
    else
        echo "Failed to download test-memcpy-support.h"
    fi
fi

echo ""
echo "Applying patches to test-string.h..."
echo "======================================"

# Only apply patches if we downloaded fresh files
if [ "$DOWNLOADED_FRESH" = true ]; then
    echo "Applying patches to freshly downloaded test-string.h..."
    
    # Remove problematic includes
    sed -i '/#include <support\/support.h>/d' "${GLIBC_TESTS_DIR}/test-string.h"
    sed -i '/#include <config.h>/d' "${GLIBC_TESTS_DIR}/test-string.h"
    sed -i '/#include <libc-misc.h>/d' "${GLIBC_TESTS_DIR}/test-string.h"

    # Add necessary includes after sys/cdefs.h
    sed -i '/#include <sys\/cdefs.h>/a #include <stddef.h>\n#include <stdbool.h>' "${GLIBC_TESTS_DIR}/test-string.h"

    # Comment out ifunc-impl-list.h and add replacements
    sed -i 's/#include <ifunc-impl-list.h>/\/* #include <ifunc-impl-list.h> *\/  \/* Comment out glibc-internal header *\//' "${GLIBC_TESTS_DIR}/test-string.h"

    # Add forward declarations after _GNU_SOURCE
    sed -i '/^#define _GNU_SOURCE$/a\\n/* Forward declarations */\nint test_main(void);\nvoid *mempcpy(void *dest, const void *src, size_t n);' "${GLIBC_TESTS_DIR}/test-string.h"

    # Add missing definitions after the forward declarations
    sed -i '/^void \*mempcpy(void \*dest, const void \*src, size_t n);$/a\\n/* Define MAP_ANON if not available */\n#ifndef MAP_ANON\n#define MAP_ANON MAP_ANONYMOUS\n#endif\n\n/* Define missing glibc structures and functions */\nstruct libc_ifunc_impl {\n  const char *name;\n  void (*fn) (void);\n  bool usable;\n};\n\n/* Mock xmalloc to use regular malloc */\n#define xmalloc malloc\n\n/* Mock glibc ifunc function */\nstatic size_t __libc_ifunc_impl_list(const char *name, struct libc_ifunc_impl *array, size_t max) {\n  return 0; \/\* No ifunc implementations for our test *\/\n}\n\n/* Define missing constants */\n#ifndef EXIT_UNSUPPORTED\n#define EXIT_UNSUPPORTED 77\n#endif' "${GLIBC_TESTS_DIR}/test-string.h"

    # Add main function at the very end of the file
    echo '
/* Main function to run the tests */
int main(void) {
  test_init();
  return test_main();
}' >> "${GLIBC_TESTS_DIR}/test-string.h"
    
    echo "Applied patches to test-string.h"
else
    echo "Using existing test-string.h, skipping patch application"
fi


# Function to remove problematic includes from a file
clean_file() {
    local file="$1"
    if [ ! -f "$file" ]; then
        echo "WARNING: File not found: $file"
        return
    fi
    
    echo "Cleaning $file..."
    
    # Remove problematic includes - we don't need test-driver
    sed -i '/#include <support\/test-driver\.c>/d' "$file"
    sed -i '/#include <support\/test-driver\.h>/d' "$file"
    sed -i '/#include "support\/test-driver\.h"/d' "$file"
    sed -i '/^#.*include.*string\/.*\.c/d' "$file"
    sed -i '/^#.*include.*wcsmbs\/.*\.c/d' "$file"
    sed -i '/^#.*include.*".*\.c"/d' "$file"  # Remove any .c file includes
    
    # Remove IMPL declarations for missing default implementations
    sed -i '/IMPL (__[a-z]*_default, 1)/d' "$file"
    sed -i '/IMPL ([A-Z]*_DEFAULT, 1)/d' "$file"
    sed -i '/IMPL (MEMCHR_DEFAULT, 1)/d' "$file"
    sed -i '/IMPL (WMEMCHR_DEFAULT, 1)/d' "$file"
    sed -i '/IMPL (STRCHR_DEFAULT, 1)/d' "$file"
    sed -i '/IMPL (STRCHRNUL_DEFAULT, 1)/d' "$file"
    sed -i '/IMPL (C_IMPL, 1)/d' "$file"  # Remove C_IMPL for strstr
    
    # Fix inhibit_loop_to_libcall macro usage on separate lines
    sed -i '/^test_cc_inhibit_loop_to_libcall$/d' "$file"
    
    # Fix c_strstr to use actual strstr function
    sed -i 's/# define STRSTR c_strstr/# define STRSTR strstr/' "$file"
    
    # Fix test_main declaration conflicts (remove static)
    sed -i 's/^static int$/int/' "$file"
    
    # Special handling for test-strnlen.c - add IMPL registration
    if [[ "$file" == *"test-strnlen.c" ]]; then
        # Use the IMPL macro (section-based) so the linker auto-generates
        # correct __start_impls/__stop_impls section boundaries.
        sed -i '/#include "test-string.h"/a\\nIMPL (strnlen, 1)' "$file"
    fi
    
    echo "Cleaned $file"
}

echo ""
echo "Cleaning test files..."
echo "=========================="

# Only clean files if we downloaded fresh content
if [ "$DOWNLOADED_FRESH" = true ]; then
    echo "Cleaning freshly downloaded test files..."
    # Clean all test files
    for test_file in "${GLIBC_TESTS_DIR}"/test-*.c; do
        if [ -f "$test_file" ]; then
            clean_file "$test_file"
        fi
    done
    
    if [ -f "${GLIBC_TESTS_DIR}/test-mempcpy.c" ]; then
        # Add the include at the end of the file if not already present
        if ! grep -q "test-memcpy.c" "${GLIBC_TESTS_DIR}/test-mempcpy.c"; then
            echo '#include "test-memcpy.c"' >> "${GLIBC_TESTS_DIR}/test-mempcpy.c"
            echo "Added test-memcpy.c include to test-mempcpy.c"
        fi
    fi
else
    echo "Using existing test files, skipping cleaning"
fi

echo ""
echo "Compiling all tests..."
echo "========================"

COMPILED_TESTS=()
FAILED_TESTS=()
PASSED_TESTS=()
FAILED_RUNTIME_TESTS=()

for func in "${FUNCTIONS[@]}"; do
    test_file="${GLIBC_TESTS_DIR}/test-${func}.c"
    if [ -f "$test_file" ]; then
        echo "Compiling test-${func}..."
        if gcc -I"${GLIBC_TESTS_DIR}" -I"${REPO_ROOT}/include" -o "${GLIBC_EXECUTABLES_DIR}/test-${func}" "$test_file" -lm 2>/dev/null; then
            echo "Compiled test-${func} successfully"
            COMPILED_TESTS+=("test-${func}")
        else
            echo "Failed to compile test-${func}"
            FAILED_TESTS+=("test-${func}")
            # Show compilation error for debugging
            echo "    Error details:"
            gcc -I"${GLIBC_TESTS_DIR}" -I"${REPO_ROOT}/include" -o "${GLIBC_EXECUTABLES_DIR}/test-${func}" "$test_file" -lm 2>&1 | head -5 | sed 's/^/    /'
        fi
    fi
done

echo ""
echo "Compilation Summary:"
echo "======================"
echo "Successfully compiled: ${#COMPILED_TESTS[@]} tests"
echo "Failed to compile: ${#FAILED_TESTS[@]} tests"

if [ ${#FAILED_TESTS[@]} -gt 0 ]; then
    echo "Failed tests: ${FAILED_TESTS[*]}"
fi

echo ""
echo "Running tests with LD_PRELOAD..."
echo "===================================="

# Function to run a test with LD_PRELOAD
run_test() {
    local test_name="$1"
    echo ""
    echo "Running $test_name with LD_PRELOAD..."
    echo "LD_PRELOAD=$SHARED_LIB ${GLIBC_EXECUTABLES_DIR}/$test_name"
    echo "----------------------------------------"
    
    local output
    output=$(env LD_PRELOAD="$SHARED_LIB" "${GLIBC_EXECUTABLES_DIR}/$test_name" 2>&1)
    local exit_code=$?
    
    # Display output for debugging
    echo "$output"
    
    # Check for various failure conditions
    if echo "$output" | grep -q "dumped core"; then
        echo ""
        echo "CRASHED: $test_name (core dumped) - Segmentation fault detected"
        echo "  This indicates a memory access violation in AOCL LibMem"
        FAILED_RUNTIME_TESTS+=("$test_name")
    elif [ $exit_code -eq 139 ]; then
        echo ""
        echo "CRASHED: $test_name (exit code 139 - SIGSEGV)"
        echo "  Segmentation fault: Invalid memory access"
        echo "  Debug with: LD_PRELOAD=$SHARED_LIB gdb ${GLIBC_EXECUTABLES_DIR}/$test_name"
        FAILED_RUNTIME_TESTS+=("$test_name")
    elif [ $exit_code -eq 134 ]; then
        echo ""
        echo "CRASHED: $test_name (exit code 134 - SIGABRT)"
        echo "  Aborted: Assertion failure or abort() called"
        FAILED_RUNTIME_TESTS+=("$test_name")
    elif [ $exit_code -eq 0 ]; then
        echo ""
        echo "PASSED: $test_name completed successfully"
        PASSED_TESTS+=("$test_name")
    else
        echo ""
        echo "FAILED: $test_name (exit code $exit_code)"
        # Try to extract error details from output
        if echo "$output" | grep -qE "(Wrong result|Error|error)"; then
            echo "  Error details found in output above"
        fi
        FAILED_RUNTIME_TESTS+=("$test_name")
    fi
}

# Run all compiled tests
for test_name in "${COMPILED_TESTS[@]}"; do
    run_test "$test_name"
done

echo ""
echo "Test execution completed!"
echo "============================"
echo "Summary:"
echo "   - Downloaded ${#FUNCTIONS[@]} glibc test files from internet"
echo "   - Applied minimal patches to test-string.h"
echo "   - Cleaned all test files by removing problematic includes"
echo "   - Compiled ${#COMPILED_TESTS[@]} tests successfully"
echo "   - Executed ${#COMPILED_TESTS[@]} tests with LD_PRELOAD of $SHARED_LIB"
echo ""
echo "Runtime Test Results:"
echo "   Passed: ${#PASSED_TESTS[@]} tests"
echo "   Crashed (Core Dump): ${#FAILED_RUNTIME_TESTS[@]} tests"

if [ ${#PASSED_TESTS[@]} -gt 0 ]; then
    echo "   Passing tests: ${PASSED_TESTS[*]}"
fi

if [ ${#FAILED_RUNTIME_TESTS[@]} -gt 0 ]; then
    echo "   Failing tests (AOCL LibMem bugs): ${FAILED_RUNTIME_TESTS[*]}"
fi
echo ""
echo "Individual test execution example:"
echo "LD_PRELOAD=$SHARED_LIB ${GLIBC_EXECUTABLES_DIR}/test-strlen"
echo ""
echo "Verify function replacement with:"
echo "LD_DEBUG=bindings LD_PRELOAD=$SHARED_LIB ${GLIBC_EXECUTABLES_DIR}/test-strlen 2>&1 | grep -E '(strlen|binding)'"