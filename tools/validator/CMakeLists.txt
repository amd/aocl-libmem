# Copyright (C) 2023-24 Advanced Micro Devices, Inc. All rights reserved.

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

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/Validator_tool.md
DESTINATION ${CMAKE_BINARY_DIR}/tools/validator/)

install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/validator.py
  DESTINATION ${CMAKE_BINARY_DIR}/test)

install(PROGRAMS ${CMAKE_BINARY_DIR}/tools/validator/CTestTestfile.cmake
  DESTINATION ${CMAKE_BINARY_DIR}/test)

add_executable(libmem_validator ${CMAKE_CURRENT_SOURCE_DIR}/libmem_validator.c)

# Enable testing
enable_testing()

# Check L2cache sizes using a command line utility
execute_process(
    COMMAND bash -c "lscpu --caches | grep 'L2' | awk '{print $2}'"
    OUTPUT_VARIABLE LSCPU_OUTPUT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(LSCPU_OUTPUT MATCHES "([0-9]+)M")
    string(REGEX REPLACE "([0-9]+)M" "\\1" LSCPU_OUTPUT ${LSCPU_OUTPUT})
    math(EXPR L2_CACHE_SIZE "${LSCPU_OUTPUT} * 1024 * 1024")

elseif(LSCPU_OUTPUT MATCHES "([0-9]+)K")
    string(REGEX REPLACE "([0-9]+)K" "\\1" LSCPU_OUTPUT ${LSCPU_OUTPUT})
    math(EXPR L2_CACHE_SIZE "${LSCPU_OUTPUT} * 1024")

else()
    set(L2_CACHE_SIZE 1048320)
endif()

# Convert LSCPU_OUTPUT to an integer
math(EXPR LSCPU_OUTPUT_INT "${L2_CACHE_SIZE}")

# Calculate start and end sizes
#1MB +- 256 L2 Cahce [Zen3 and Zen4]
math(EXPR L2TH_START "${LSCPU_OUTPUT_INT} - 256")
math(EXPR L2TH_END "${LSCPU_OUTPUT_INT} + 256")

# Check L3cache sizes using a command line utility
execute_process(
    COMMAND bash -c "lscpu --caches | grep 'L3' | awk '{print $2}'"
    OUTPUT_VARIABLE LSCPU_OUTPUT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(LSCPU_OUTPUT MATCHES "([0-9]+)M")
    string(REGEX REPLACE "([0-9]+)M" "\\1" LSCPU_OUTPUT ${LSCPU_OUTPUT})
    math(EXPR L3_CACHE_SIZE "${LSCPU_OUTPUT} * 1024 * 1024")

elseif(LSCPU_OUTPUT MATCHES "([0-9]+)K")
    string(REGEX REPLACE "([0-9]+)K" "\\1" LSCPU_OUTPUT ${LSCPU_OUTPUT})
    math(EXPR L3_CACHE_SIZE "${LSCPU_OUTPUT} * 1024")

else()
    set(L3_CACHE_SIZE 33554432)
endif()

# Convert LSCPU_OUTPUT to an integer
math(EXPR LSCPU_OUTPUT_INT "${L3_CACHE_SIZE}")

# Calculate start and end sizes
#24MB +- 256 3/4th of L3 Cache [Zen4]
math(EXPR L3TH_START "${LSCPU_OUTPUT_INT} * 3 / 4 - 256")
math(EXPR L3TH_END "${LSCPU_OUTPUT_INT} * 3 / 4  + 256")

# Define test values
set(PAGE_SIZE 4096)    #4KB
set(MAX_SIZE 33554432) #32MB

set(MAX_TIMEOUT 12000) #Timout set to 200Mins for Memcmp
set(LIBMEM_PATH ${CMAKE_INSTALL_PREFIX}/lib/libaocl-libmem.so)
set(VALIDATOR_EXEC ${CMAKE_BINARY_DIR}/tools/validator/libmem_validator)
set(supp_func "memcpy" "memcmp" "memset" "memmove" "mempcpy" "memchr"
    "strcpy" "strncpy" "strcmp" "strncmp" "strlen" "strcat" "strstr")

set(memory_threshold_func "memcpy" "memmove" "mempcpy")

##ITERATOR TESTING
#[0,1,2...4096B]

set(iter_start_sz 0)
set(iter_end_sz ${PAGE_SIZE})

set(size_values "")
while(${iter_start_sz} LESS_EQUAL ${iter_end_sz})
    list(APPEND size_values ${iter_start_sz})
    math(EXPR iter_start_sz "${iter_start_sz} + 1")
endwhile()

foreach(pattern IN LISTS supp_func)
    string(REGEX REPLACE "[^A-Za-z0-9]" "_" sanitized_pattern "${pattern}")
    foreach(size ${size_values})
        set(test_name "${sanitized_pattern}_iter_${size}B")
        add_test(
            NAME "${test_name}"
            COMMAND env LD_PRELOAD=${LIBMEM_PATH}
            ${VALIDATOR_EXEC} ${pattern} ${size}
        )
        set_tests_properties(
            "${sanitized_pattern}_iter_${size}B"
            PROPERTIES
            FAIL_REGULAR_EXPRESSION "ERROR"
        )
    endforeach()
endforeach()

##SHIFT TESTING
#Standard Sizes (Power of 2) [1B,2B,4B,8B...32MB]
set(shift_start_sz 1)
set(shift_end_sz ${MAX_SIZE})

set(size_values "")
while(${shift_start_sz} LESS_EQUAL ${shift_end_sz})
    list(APPEND size_values ${shift_start_sz})
    math(EXPR shift_start_sz "${shift_start_sz} * 2")
endwhile()

foreach(pattern IN LISTS supp_func)
    string(REGEX REPLACE "[^A-Za-z0-9]" "_" sanitized_pattern "${pattern}")
    foreach(size ${size_values})
        if(${size} LESS 1024)
            set(value ${size})
            set(unit "B")
        elseif(${size} LESS 1048576)
            math(EXPR value "${size}/1024")
            set(unit "KB")
        else()
            math(EXPR value "${size}/1048576")
            set(unit "MB")
        endif()

        set(test_name "${sanitized_pattern}_shift_${value}${unit}")
        add_test(
            NAME "${test_name}"
            COMMAND env LD_PRELOAD=${LIBMEM_PATH}
            ${VALIDATOR_EXEC} ${pattern} ${size}
        )
        set_tests_properties(
            "${sanitized_pattern}_shift_${value}${unit}"
                PROPERTIES
                TIMEOUT ${MAX_TIMEOUT}
                FAIL_REGULAR_EXPRESSION "ERROR"
        )
    endforeach()
endforeach()

##ALIGNMENT TESTING
#size[0-4096B] , src[0-63] & dst[0-63] Alignments
set(align_start_sz 0)
set(align_end_sz ${PAGE_SIZE})

set(size_values "")
while(${align_start_sz} LESS_EQUAL ${align_end_sz})
    list(APPEND size_values ${align_start_sz})
    math(EXPR align_start_sz "${align_start_sz} + 1")
endwhile()

foreach(pattern IN LISTS supp_func)
    string(REGEX REPLACE "[^A-Za-z0-9]" "_" sanitized_pattern "${pattern}")
    foreach(size ${size_values})
        set(test_name "${sanitized_pattern}_align_${size}B")
        add_test(
            NAME "${test_name}"
            COMMAND env LD_PRELOAD=${LIBMEM_PATH}
            ${VALIDATOR_EXEC} ${pattern} ${size} 0 0 1
        )
        set_tests_properties(
            "${sanitized_pattern}_align_${size}B"
            PROPERTIES
            FAIL_REGULAR_EXPRESSION "ERROR"
        )
    endforeach()
endforeach()

#Threshold boundary checks for memory functions
set(size_values "")
while(${L2TH_START} LESS_EQUAL ${L2TH_END})
    list(APPEND size_values ${L2TH_START})
    math(EXPR L2TH_START "${L2TH_START} + 1")
endwhile()

foreach(pattern IN LISTS memory_threshold_func)
    string(REGEX REPLACE "[^A-Za-z0-9]" "_" sanitized_pattern "${pattern}")
    foreach(size ${size_values})
        set(test_name "${sanitized_pattern}_thresholdL2_${size}B")
        add_test(
            NAME "${test_name}"
            COMMAND env LD_PRELOAD=${LIBMEM_PATH}
            ${VALIDATOR_EXEC} ${pattern} ${size} 0 0 1
        )
        set_tests_properties(
            "${sanitized_pattern}_thresholdL2_${size}B"
            PROPERTIES
            FAIL_REGULAR_EXPRESSION "ERROR"
        )
    endforeach()
endforeach()

set(size_values "")
while(${L3TH_START} LESS_EQUAL ${L3TH_END})
    list(APPEND size_values ${L3TH_START})
    math(EXPR L3TH_START "${L3TH_START} + 1")
endwhile()

foreach(pattern IN LISTS memory_threshold_func)
    string(REGEX REPLACE "[^A-Za-z0-9]" "_" sanitized_pattern "${pattern}")
    foreach(size ${size_values})
        set(test_name "${sanitized_pattern}_thresholdL3_${size}B")
        add_test(
            NAME "${test_name}"
            COMMAND env LD_PRELOAD=${LIBMEM_PATH}
            ${VALIDATOR_EXEC} ${pattern} ${size} 0 0 1
        )
        set_tests_properties(
            "${sanitized_pattern}_thresholdL3_${size}B"
            PROPERTIES
            FAIL_REGULAR_EXPRESSION "ERROR"
        )
    endforeach()
endforeach()
