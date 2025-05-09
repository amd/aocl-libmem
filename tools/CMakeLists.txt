# Copyright (C) 2023-25 Advanced Micro Devices, Inc. All rights reserved.
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

# libmem tools

add_subdirectory(benchmarks)
add_subdirectory(analyser)
add_subdirectory(validator)
add_subdirectory(graph_generator)

# generating a file to parse the library path for tools
file(WRITE ${CMAKE_BINARY_DIR}/test/libmem_defs.py "LIBMEM_BIN_PATH='${CMAKE_INSTALL_PREFIX}/lib/lib${PROJECT_NAME}.so'\n")
file(APPEND ${CMAKE_BINARY_DIR}/test/libmem_defs.py "LIBMEM_ARCHIVE_PATH='${CMAKE_INSTALL_PREFIX}/lib'\n")

set(CPU_DETECT_SRC_FILES "${CMAKE_SOURCE_DIR}/tools/detect_cpu_info.c")
set(CPU_INCLUDE "${CMAKE_SOURCE_DIR}/include")
set(DETECT_FEATURE 2)
set(AVX512 3)
set(AVX512_FEATURE_ENABLED "False")
# Detect CPU features
set(FEATURE AVX512)
try_run(HAS_${FEATURE} COMPILERESULT "${CMAKE_BINARY_DIR}/temp" SOURCES ${CPU_DETECT_SRC_FILES}
        COMPILE_DEFINITIONS -I${CPU_INCLUDE} ARGS ${DETECT_FEATURE} ${${FEATURE}})
if (HAS_${FEATURE})
    #AVX512 supported
    set(AVX512_FEATURE_ENABLED "True")
endif()

# Append the AVX512_FEATURE_ENABLED variable to the libmem_defs script
file(APPEND ${CMAKE_BINARY_DIR}/test/libmem_defs.py "AVX512_FEATURE_ENABLED = ${AVX512_FEATURE_ENABLED}\n")

