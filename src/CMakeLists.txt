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

include(CheckCCompilerFlag)
check_c_compiler_flag("-march=znver1"    CONFIG_COMPILER_HAS_ZEN1)
check_c_compiler_flag("-march=znver2"    CONFIG_COMPILER_HAS_ZEN2)
check_c_compiler_flag("-march=znver3"    CONFIG_COMPILER_HAS_ZEN3)
check_c_compiler_flag("-march=znver4"    CONFIG_COMPILER_HAS_ZEN4)
check_c_compiler_flag("-march=znver5"    CONFIG_COMPILER_HAS_ZEN5)

set(DETECT_ARCH 1)
set(ARCH_UNDF 255)

set(DETECT_FEATURE 2)
set(AVX2 0)
set(ERMS 1)
set(FSRM 2)
set(AVX512 3)
set(MOVIDR 4)

set(cpu_detect_src_files "${CMAKE_SOURCE_DIR}/tools/detect_cpu_info.c")
set(cpu_include "${CMAKE_SOURCE_DIR}/include")

set(CPU_FEATURES AVX512 AVX2 ERMS)

# Detect CPU features
foreach(FEATURE ${CPU_FEATURES})

    try_run(HAS_${FEATURE} COMPILERESULT "${CMAKE_BINARY_DIR}/temp" SOURCES ${cpu_detect_src_files}
            COMPILE_DEFINITIONS -I${cpu_include} ARGS ${DETECT_FEATURE} ${${FEATURE}})

    if (HAS_${FEATURE})
        message(STATUS "HAS_${FEATURE}: True")
    else ()
        message(STATUS "HAS_${FEATURE}: False")
    endif()
endforeach()

#Detect CPU type if dispatching is enabled
if (ENABLE_TUNABLES)
    message(STATUS "Dynamic CPU Dispatching with TUNABLES...")
elseif (ALMEM_DISP MATCHES "dynamic")
    message(STATUS "Dynamic CPU Dispatching...")
    set(DYNAMIC_DISPATCH 1)
elseif (ALMEM_DISP MATCHES "static")
    set(DYNAMIC_DISPATCH 0)

    try_run(ARCH_VER COMPILERESULT "${CMAKE_BINARY_DIR}/temp" SOURCES ${cpu_detect_src_files}
        COMPILE_DEFINITIONS -I${cpu_include} ARGS ${DETECT_ARCH})

    if (${ARCH_VER} LESS ${ARCH_UNDF})
        message(STATUS "Detected CPU: ZEN${ARCH_VER}")
        message(STATUS "Static CPU Dispatching for ZEN${ARCH_VER} cpu...")
        set(CPU_ARCH_DETECTED 1)
        add_definitions(-DARCH_ZEN${ARCH_VER})
        if (${CONFIG_COMPILER_HAS_ZEN${ARCH_VER}})
            message(STATUS "CPU Flag: znver${ARCH_VER}")
            add_compile_options(-march=znver${ARCH_VER})
        else ()
            message(WARNING " *** Compiler doesn't support the detected CPU arch flag: znver${ARCH_VER}")
            add_compile_options(-mavx2)
            if (${HAS_AVX512})
                set(AVX512_FEATURE_AVAILABLE true)
                add_definitions(-DAVX512_FEATURE_ENABLED)
                add_compile_options(-mavx512f -mavx512bw)
            endif ()
        endif()
    endif()
else () # No CPU Dispatching
    if (ALMEM_DISP)
        message(WARNING " *** Invalid CPU dispatch option. valid options: 'static' or 'dynamic' ")
    endif()
    message(STATUS "Static CPU Dispatching for generic ZEN cpu...")

    if (${HAS_AVX512})
        set(AVX512_FEATURE_AVAILABLE true)
        add_definitions(-DAVX512_FEATURE_ENABLED)
        add_compile_options(-mavx512f -mavx512bw)
    elseif (${HAS_AVX2})
        add_compile_options(-mavx2)
    endif()
endif()


if (ALMEM_ARCH)
    if (${HAS_AVX512})
        if (${ALMEM_ARCH} MATCHES "avx2")
            add_compile_options(-mavx2)
            message("Cross-Compiling for AVX2 CPUs...")
        else ()
            set(AVX512_FEATURE_AVAILABLE true)
            add_definitions(-DAVX512_FEATURE_ENABLED)
            add_compile_options(-mavx512f -mavx512bw)
            message("Native-Compiling for AVX512 CPUs...")
        endif ()
    else ()
        if (${ALMEM_ARCH} MATCHES "avx512")
            set(AVX512_FEATURE_AVAILABLE true)
            add_definitions(-DAVX512_FEATURE_ENABLED)
            add_compile_options(-mavx512f -mavx512bw)
            message("Cross-Compiling for AVX512 CPUs...")
        else ()
            add_compile_options(-mavx2)
            message("Native-Compiling for AVX2 CPUs...")
        endif ()
    endif ()
endif ()

# Generic CC flags
add_compile_options(-Wall -Wextra -Werror -Wno-unused-function -fno-common -Wundef -fvisibility=hidden -O3 -mcmodel=small -Wno-implicit-fallthrough)

# CPU specific flags
add_compile_options(-mbmi -mlzcnt)

# Compiler exceptions
if ((${CMAKE_C_COMPILER_ID}  STREQUAL "Clang") AND ENABLE_TUNABLES)
    add_definitions(-DALMEM_CMPSD_CLANG_WORKAROUND)
endif ()

if(ENABLE_GCOV AND ENABLE_LOGGING)
  target_compile_options(${PROJECT_NAME} PUBLIC -fprofile-arcs  -ftest-coverage)
  target_link_options(${PROJECT_NAME} PUBLIC -fprofile-arcs  -ftest-coverage)
endif()

# Add source files
set(SOURCE_FILES cache_info.c cpu_features.c threshold.c libmem.c)

if (ENABLE_LOGGING)
    list(APPEND SOURCE_FILES logger.c)
endif ()

if (ENABLE_TUNABLES)
    add_definitions(-DENABLE_TUNABLES)
    set(DYNAMIC_DISPATCH 1)
    list(APPEND SOURCE_FILES env_parser.c)
endif ()

if (DYNAMIC_DISPATCH)
    add_compile_options(-mavx512f -mavx512bw -mavx2)
    add_definitions(-DDYNAMIC_DISPATCHING)
    if (${HAS_AVX512})
        add_definitions(-DAVX512_FEATURE_ENABLED)
        set(AVX512_FEATURE_AVAILABLE true)
    endif ()
    list(APPEND SOURCE_FILES libmem_iface.c)
endif ()

# common object lib
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
