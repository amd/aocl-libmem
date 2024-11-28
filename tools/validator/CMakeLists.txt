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
target_link_libraries(libmem_validator m)

# Enable testing
enable_testing()

set(DETECT_FEATURE 2)
set(AVX2 0)
set(AVX512 3)
set(DETECT_CPU_VENDOR  4)
set(REQ_TYPE_CACHE 3)
set(L2_CACHE_CORE  2)
set(L3_CACHE_CCX   3)
set(AMD_CPU 1)
set(CPU_FEATURES AVX512)

set(CPU_DETECT_SRC_FILES "${CMAKE_SOURCE_DIR}/tools/detect_cpu_info.c")
set(CPU_INCLUDE "${CMAKE_SOURCE_DIR}/include")

#Detect the CPU Vendor
try_run(CPU_VENDOR COMPILERESULT "${CMAKE_BINARY_DIR}/temp" SOURCES ${CPU_DETECT_SRC_FILES}
COMPILE_DEFINITIONS -I${CPU_INCLUDE} ARGS ${DETECT_CPU_VENDOR})

if (${CPU_VENDOR} MATCHES ${AMD_CPU})
    set(CPU "AMD")
endif()

# Detect CPU features
set(FEATURE AVX512)
try_run(HAS_${FEATURE} COMPILERESULT "${CMAKE_BINARY_DIR}/temp" SOURCES ${CPU_DETECT_SRC_FILES}
        COMPILE_DEFINITIONS -I${CPU_INCLUDE} ARGS ${DETECT_FEATURE} ${${FEATURE}})
if (HAS_${FEATURE})
    #AVX512 supported
    SET(ISA "AVX512")
    #Passing AVX512_FEATURE_ENABLED for VEC_SZ computation
    target_compile_definitions(libmem_validator PRIVATE AVX512_FEATURE_ENABLED)
else()
    SET(FEATURE AVX2)
    try_run(HAS_${FEATURE} COMPILERESULT "${CMAKE_BINARY_DIR}/temp" SOURCES ${CPU_DETECT_SRC_FILES}
        COMPILE_DEFINITIONS -I${CPU_INCLUDE} ARGS ${DETECT_FEATURE} ${${FEATURE}})
        if (HAS_${FEATURE})
            #AVX2 supported
            SET(ISA "AVX2")
        endif()
endif()

# Define test values
set(PAGE_SIZE 4096)    #4KB
set(MAX_SIZE 33554432) #32MB

set(MAX_TIMEOUT 12000) #Timout set to 200Mins for Memcmp
set(LIBMEM_PATH ${CMAKE_INSTALL_PREFIX}/lib/libaocl-libmem.so)
set(VALIDATOR_EXEC ${CMAKE_BINARY_DIR}/tools/validator/libmem_validator)
set(supp_func "memcpy" "memcmp" "memset" "memmove" "mempcpy" "memchr"
    "strcpy" "strncpy" "strcmp" "strncmp" "strlen" "strcat" "strstr" "strspn" "strchr")

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

##THRESHOLD TESTING FOR AMD-CPUS
if(CPU STREQUAL "AMD")
    #L2 Cache info
    try_run(L2_CACHE COMPILERESULT "${CMAKE_BINARY_DIR}/temp" SOURCES ${CPU_DETECT_SRC_FILES}
    COMPILE_DEFINITIONS -I${CPU_INCLUDE} ARGS ${REQ_TYPE_CACHE} ${L2_CACHE_CORE})
    #L2_CACHE returned is a multiple of 512KB
    math(EXPR L2_CACHE "${L2_CACHE} * 512 *1024")

    #L3 Cache info
    try_run(L3_CACHE COMPILERESULT "${CMAKE_BINARY_DIR}/temp" SOURCES ${CPU_DETECT_SRC_FILES}
    COMPILE_DEFINITIONS -I${CPU_INCLUDE} ARGS ${REQ_TYPE_CACHE} ${L3_CACHE_CCX})
    #L3_CACHE returned is a multiple of MB
    math(EXPR L3_CACHE "${L3_CACHE} * 1024 * 1024")
    #3/4th of L3 Cache = NT_THRSHOLD
    math(EXPR NT_TH "${L3_CACHE} * 3/4")

    if(ISA STREQUAL "AVX512")
        math(EXPR L2_CACHE_TH_STRT "${L2_CACHE} - 512")
        math(EXPR L2_CACHE_TH_END "${L2_CACHE} + 512")
        math(EXPR NT_TH_STRT "${NT_TH} - 512")
        math(EXPR NT_TH_END "${NT_TH} + 512")
        #Generating Threshold Value sizes using shift
        set(SHIFT 0 1 2 3 4 5 6 7 8 9)

    elseif (ISA STREQUAL "AVX2")
        math(EXPR L2_CACHE_TH_STRT "${L2_CACHE} - 128")
        math(EXPR L2_CACHE_TH_END "${L2_CACHE} + 128")
        math(EXPR NT_TH_STRT "${NT_TH} - 128")
        math(EXPR NT_TH_END "${NT_TH} + 128")
        set(SHIFT 0 1 2 3 4 5 6 7)

    else()
        message(FALTAL_ERROR "SKIPPING Threshold validation")
    endif()

    set(VALUES "")
    # Generate powers of two using left shift and append to VALUES
    foreach(shift IN LISTS SHIFT)
        math(EXPR value "1 << ${shift}")
        list(APPEND VALUES ${value})
    endforeach()
    # Initialize the list to hold the size values
    set(size_values "")
    # Increment L2_CACHE_TH_STRT by each value in VALUES
    foreach(value IN LISTS VALUES)
        # Use a temporary variable to hold the incremented value
        math(EXPR temp_value "${L2_CACHE_TH_STRT} + ${value}")
        # Check if the incremented value is within the threshold range
        if(temp_value LESS_EQUAL ${L2_CACHE})
            list(APPEND size_values ${temp_value})
        endif()
    endforeach()

    foreach(value IN LISTS VALUES)
    # Use a temporary variable to hold the incremented value
        math(EXPR temp_value "${L2_CACHE} + ${value}")
        if(temp_value LESS_EQUAL ${L2_CACHE_TH_END})
            list(APPEND size_values ${temp_value})
        endif()
    endforeach()

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
    # Increment NT_TH_STRT by each value in VALUES
    foreach(value IN LISTS VALUES)
        # Use a temporary variable to hold the incremented value
        math(EXPR temp_value "${NT_TH_STRT} + ${value}")
        # Check if the incremented value is within the threshold range
        if(temp_value LESS_EQUAL ${NT_TH})
            list(APPEND size_values ${temp_value})
        endif()
    endforeach()

    foreach(value IN LISTS VALUES)
    # Use a temporary variable to hold the incremented value
        math(EXPR temp_value "${NT_TH} + ${value}")
        if(temp_value LESS_EQUAL ${NT_TH_END})
            list(APPEND size_values ${temp_value})
        endif()
    endforeach()

    foreach(pattern IN LISTS memory_threshold_func)
        string(REGEX REPLACE "[^A-Za-z0-9]" "_" sanitized_pattern "${pattern}")
        foreach(size ${size_values})
            set(test_name "${sanitized_pattern}_threshold_nt_${size}B")
            add_test(
                NAME "${test_name}"
                COMMAND env LD_PRELOAD=${LIBMEM_PATH}
                ${VALIDATOR_EXEC} ${pattern} ${size} 0 0 1
            )
            set_tests_properties(
                "${sanitized_pattern}_threshold_nt_${size}B"
                PROPERTIES
                FAIL_REGULAR_EXPRESSION "ERROR"
            )
        endforeach()
    endforeach()

endif()
