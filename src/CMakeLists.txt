# Copyright (C) 2022-24 Advanced Micro Devices, Inc. All rights reserved.
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


add_compile_options(-Wall -Wextra -Werror -Wno-unused-function -fno-common -Wundef -fvisibility=hidden -mavx2 -O3 -mbmi -mcmodel=small -mlzcnt -Wno-implicit-fallthrough -mno-vzeroupper)

if (AVX512_FEATURE_AVAILABLE)
    add_compile_options(-mavx512f -mavx512bw)
endif ()

if ((${CMAKE_C_COMPILER_ID}  STREQUAL "Clang") AND ENABLE_TUNABLES)
    add_definitions(-DALMEM_CMPSD_CLANG_WORKAROUND)
endif ()

if(ENABLE_GCOV AND ENABLE_LOGGING)
  target_compile_options(${PROJECT_NAME} PUBLIC -fprofile-arcs  -ftest-coverage)
  target_link_options(${PROJECT_NAME} PUBLIC -fprofile-arcs  -ftest-coverage)
endif()

set(SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/cache_info.c   ${CMAKE_CURRENT_SOURCE_DIR}/cpu_features.c  ${CMAKE_CURRENT_SOURCE_DIR}/threshold.c  ${CMAKE_CURRENT_SOURCE_DIR}/libmem.c)

if (ENABLE_LOGGING)
set(SOURCE_FILES ${SOURCE_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/logger.c)
endif ()

if (ENABLE_TUNABLES)
add_definitions(-DENABLE_TUNABLES)
set(SOURCE_FILES ${SOURCE_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/env_parser.c  ${CMAKE_CURRENT_SOURCE_DIR}/libmem_iface.c)
endif ()

#common object lib
add_library(libmem_objlib OBJECT ${SOURCE_FILES})
target_include_directories(libmem_objlib PUBLIC ${CMAKE_SOURCE_DIR}/include)
set_property(TARGET libmem_objlib PROPERTY POSITION_INDEPENDENT_CODE 1)

# shared lib
set(CMAKE_SHARED_LINKER_FLAGS "-flto -fpic")
add_library(${PROJECT_NAME} SHARED)
target_link_libraries(${PROJECT_NAME} libmem_objlib)

# static lib
add_library("${PROJECT_NAME}_static" STATIC)
target_link_libraries("${PROJECT_NAME}_static" libmem_objlib)

# renaming static binary
set_target_properties("${PROJECT_NAME}_static" PROPERTIES OUTPUT_NAME ${PROJECT_NAME})

set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION})

# Memory Functions
add_subdirectory(memcpy)
add_subdirectory(mempcpy)
add_subdirectory(memmove)
add_subdirectory(memset)
add_subdirectory(memcmp)
add_subdirectory(memchr)

# String Functions
add_subdirectory(strcpy)
add_subdirectory(strncpy)
add_subdirectory(strcmp)
add_subdirectory(strncmp)
add_subdirectory(strlen)
add_subdirectory(strcat)
add_subdirectory(strstr)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR} CACHE PATH "..." FORCE)
endif()

# installing the target libraries
install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(TARGETS "${PROJECT_NAME}_static" ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
