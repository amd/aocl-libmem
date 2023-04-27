# Copyright (C) 2022-23 Advanced Micro Devices, Inc. All rights reserved.
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

cmake_minimum_required(VERSION 3.10)

# avoid build in root directory
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
	message(FATAL_ERROR “In-source build detected!”)
endif()


# set the project name and version
set(LIBMEM_VERSION_STRING 4.0.1)

project(aocl-libmem VERSION ${LIBMEM_VERSION_STRING} LANGUAGES C DESCRIPTION
                        "Library of AMD optimized string/memory functions")

string(TIMESTAMP BUILD_DATE "%Y%m%d")

set(LIBMEM_BUILD_VERSION_STR
    "AOCL-LibMem ${LIBMEM_VERSION_STRING} Build ${BUILD_DATE}")

add_definitions(-DLIBMEM_BUILD_VERSION="${LIBMEM_BUILD_VERSION_STR}")

set(DEFAULT_BUILD_TYPE "Release")

option(BUILD_SHARED_LIBS "Build shared libraries" ON)

option(ENABLE_LOGGING "Enable Logger" OFF)

option(ENABLE_TUNABLES "Enable user input" OFF)

if (ENABLE_LOGGING)
add_definitions(-DENABLE_LOGGER)
# uncomment the below for debug logs, LOG_LEVEL=DEBUG.
#add_definitions(-DLOG_LEVEL=4)
endif ()

option(ALMEM_ARCH "ISA_ARCH_TYPE" ON)

execute_process(COMMAND bash "-c" "lscpu | grep erms"
        RESULT_VARIABLE ERMS_FEATURE OUTPUT_QUIET)

if (NOT ${ERMS_FEATURE})
    #uncomment after addng ERMS support for all funcs
    #add_definitions(-DERMS_FEATURE_ENABLED)
    message("ERMS Feature Enabled on Build machine.")
endif ()

execute_process(COMMAND bash "-c" "lscpu | grep avx512"
        RESULT_VARIABLE AVX512_FEATURE OUTPUT_QUIET)

if (NOT ${AVX512_FEATURE})
    message("AVX512 Feature Enabled on Build machine.")
    if (NOT ${ALMEM_ARCH} STREQUAL "avx2")
        message("Setting Arch to AVX512")
        set(ALMEM_ARCH  "avx512")
    endif ()
endif ()

if (ALMEM_ARCH MATCHES "avx512")
    set(AVX512_FEATURE_AVAILABLE true)
    add_definitions(-DAVX512_FEATURE_ENABLED)
    if (${AVX512_FEATURE})
        message("Cross-Compiling for AVX512 Arch...")
    else ()
        message("Native-Compiling for AVX512 Arch...")
    endif ()
else ()
    if (NOT ${AVX512_FEATURE})
        message("Cross-Compiling for AVX2 Arch...")
    else ()
        message("Native-Compiling for AVX2 Arch...")
    endif ()
endif ()

# option for building shared lib
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

# let the build system know the source directory
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src)

# let the build system know the tools directory
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tools)

file(WRITE ${CMAKE_BINARY_DIR}/version.h ${LIBMEM_BUILD_VERSION_STR})
