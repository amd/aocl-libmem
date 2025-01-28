# Copyright (C) 2022-25 Advanced Micro Devices, Inc. All rights reserved.
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

cmake_minimum_required(VERSION 3.22)

# avoid build in root directory
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
	message(FATAL_ERROR “In-source build detected!”)
endif()

# set the project name and version
set(LIBMEM_VERSION_STRING 5.0.1)

project(aocl-libmem VERSION ${LIBMEM_VERSION_STRING} LANGUAGES C CXX DESCRIPTION
                        "Library of AMD optimized string/memory functions")

string(TIMESTAMP BUILD_DATE "%Y%m%d")

set(LIBMEM_BUILD_VERSION_STR
    "AOCL-LibMem ${LIBMEM_VERSION_STRING} Build ${BUILD_DATE}")

add_definitions(-DLIBMEM_BUILD_VERSION="${LIBMEM_BUILD_VERSION_STR}")

set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)

option(ALMEM_DYN_DISPATCH "CPU Dispatching" OFF)

option(ALMEM_ISA "ISA_TYPE" OFF)

option(ALMEM_LOGGING "Enable Logger" OFF)

option(ALMEM_TUNABLES "Enable Tunable Options" OFF)

option(ALMEM_GCOV "Enable Code Coverage" OFF)

if (ALMEM_LOGGING)
add_definitions(-DALMEM_LOGGER)
# uncomment the below for debug logs, LOG_LEVEL=DEBUG.
#add_definitions(-DLOG_LEVEL=4)
endif ()

if (ENABLE_TUNABLES) #retaining for backward compatibility
    set(ALMEM_TUNABLES ON)
endif()

#reset ISA for invalid options
if (NOT (${ALMEM_ISA} STREQUAL "avx2" OR ${ALMEM_ISA} STREQUAL "avx512"))
    if (${ALMEM_ISA} MATCHES "avx")
        message("InValid ISA option; Going ahead with next feasible option")
    endif ()
    set(ALMEM_ISA "")
endif ()

if (ALMEM_ISA) #ISA has precedence over DYN_DISPATCH
    if (ALMEM_DYN_DISPATCH)
        message(WARNING "ISA has precedence over DYN_DISPATCH; Disabling Dynamic CPU dispatching...")
        set(ALMEM_DYN_DISPATCH OFF)
    endif ()
    if (ALMEM_TUNABLES)
        message(WARNING "ISA has precedence over TUNABLES; Disabling TUNABLES option...")
        set(ALMEM_TUNABLES OFF)
    endif()
endif()

if (ALMEM_TUNABLES)
    set(ALMEM_DYN_DISPATCH ON)
endif()

# let the build system know the source directory
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src)

# let the build system know the tools directory
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tools)

file(WRITE ${CMAKE_BINARY_DIR}/version.h ${LIBMEM_BUILD_VERSION_STR})
