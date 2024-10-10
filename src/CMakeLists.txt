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
set(MIN_ZEN_VER 1)
set(MAX_ZEN_VER 5)

set(DETECT_FEATURE 2)
set(AVX2 0)
set(ERMS 1)
set(FSRM 2)
set(AVX512 3)
set(MOVIDR 4)

set(DETECT_CPU_VENDOR  4)
set(AMD_CPU 1)

set(CPU_DETECT_SRC_FILES "${CMAKE_SOURCE_DIR}/tools/detect_cpu_info.c")
set(CPU_INCLUDE "${CMAKE_SOURCE_DIR}/include")

set(CPU_FEATURES AVX512 AVX2 ERMS)

# Detect CPU features
foreach(FEATURE ${CPU_FEATURES})

    try_run(HAS_${FEATURE} COMPILERESULT "${CMAKE_BINARY_DIR}/temp" SOURCES ${CPU_DETECT_SRC_FILES}
            COMPILE_DEFINITIONS -I${CPU_INCLUDE} ARGS ${DETECT_FEATURE} ${${FEATURE}})

    if (HAS_${FEATURE})
        message(STATUS "HAS_${FEATURE}: True")
    else ()
        message(STATUS "HAS_${FEATURE}: False")
    endif()
endforeach()

if (ALMEM_ISA)
        if (${ALMEM_ISA} MATCHES "avx512")
            message(STATUS "## Compiling for AVX512 CPUs...")
        elseif (${ALMEM_ISA} MATCHES "avx2")
            message(STATUS "## Compiling for AVX2 CPUs...")
        else ()
            message(WARNING " *** Invalid ISA options")
            message(STATUS "## Compiling for system default...")
            set(ALMEM_ISA "system")
        endif ()
elseif (ALMEM_DYN_DISPATCH)
    add_definitions(-DALMEM_DYN_DISPATCH)
    if (ALMEM_TUNABLES)
        add_definitions(-DALMEM_TUNABLES)
        add_definitions(-DAVX512_FEATURE_ENABLED)
        set(AVX512_FEATURE_AVAILABLE ON)
        message(STATUS "## Compiling for Tunable options with Dynamic CPU Dispatching...")
    else ()
        message(STATUS "## Compiling for Dynamic CPU Dispatching...")
    endif ()
else ()
    #Detect the CPU Vendor
    try_run(CPU_VENDOR COMPILERESULT "${CMAKE_BINARY_DIR}/temp" SOURCES ${CPU_DETECT_SRC_FILES}
        COMPILE_DEFINITIONS -I${CPU_INCLUDE} ARGS ${DETECT_CPU_VENDOR})

    if (${CPU_VENDOR} MATCHES ${AMD_CPU})
        #Detect the CPU architecture
        try_run(ARCH_VER COMPILERESULT "${CMAKE_BINARY_DIR}/temp" SOURCES ${CPU_DETECT_SRC_FILES}
            COMPILE_DEFINITIONS -I${CPU_INCLUDE} ARGS ${DETECT_ARCH})


        if (${ARCH_VER} LESS ${ARCH_UNDF})
            message(STATUS "## Compiling for Native CPU - ZEN${ARCH_VER} ...")
        endif()
    else ()
        message(STATUS "Non AMD CPU detected. Going ahead with system defaults ...")
    endif()
endif()


# Generic CC flags
add_compile_options(-Wall -Wextra -Werror -Wno-unused-function -fno-common -Wundef -fvisibility=hidden -O3 -mcmodel=small -Wno-implicit-fallthrough)

# Compiler exceptions
if ((${CMAKE_C_COMPILER_ID}  STREQUAL "Clang") AND ALMEM_TUNABLES)
    add_definitions(-DALMEM_CMPSD_CLANG_WORKAROUND)
endif ()

if(ALMEM_GCOV AND ALMEM_LOGGING)
  target_compile_options(${PROJECT_NAME} PUBLIC -fprofile-arcs  -ftest-coverage)
  target_link_options(${PROJECT_NAME} PUBLIC -fprofile-arcs  -ftest-coverage)
endif()

# Add source files
set(SOURCE_FILES cache_info.c cpu_features.c threshold.c libmem.c)

# Memory Functions
set(SUPP_MEM_FUNCS memcpy mempcpy memmove memset memcmp memchr)

# String Functions
set(SUPP_STR_FUNCS strcpy strncpy strcmp strncmp strcat strstr strlen strchr)

# Dynamic Dispatching Supported Functions
set(DYN_DISP_SUPP_FUNCS ${SUPP_MEM_FUNCS} ${SUPP_STR_FUNCS})

if (ALMEM_LOGGING)
    list(APPEND SOURCE_FILES logger.c)
endif ()

if (ALMEM_ISA)
    if (ALMEM_ISA MATCHES "avx")
        add_subdirectory(isa)
    endif ()
else ()
    add_subdirectory(uarch)
    if (ALMEM_DYN_DISPATCH)
        list(APPEND SOURCE_FILES libmem_iface.c)
        add_subdirectory(system)
    endif ()
    if (ALMEM_TUNABLES)
        list(APPEND SOURCE_FILES env_parser.c)
        add_subdirectory(tunable)
    endif ()
endif ()


# common object lib
add_library(libmem_objlib OBJECT ${SOURCE_FILES})
target_include_directories(libmem_objlib PUBLIC ${CMAKE_SOURCE_DIR}/include)
set_property(TARGET libmem_objlib PROPERTY POSITION_INDEPENDENT_CODE 1)
if (ALMEM_TUNABLES)
    set(OBJLIB_CMPLR_FLAGS -mavx512f -mavx512bw)
else ()
    if (${HAS_AVX512})
        set(OBJLIB_CMPLR_FLAGS -mavx512f -mavx512bw)
    else ()
        set(OBJLIB_CMPLR_FLAGS -mavx2)
    endif ()
endif ()
list(APPEND OBJLIB_CMPLR_FLAGS -mbmi -mlzcnt)

target_compile_options(libmem_objlib PRIVATE ${OBJLIB_CMPLR_FLAGS})

# shared lib
add_library(${PROJECT_NAME} SHARED)
target_link_libraries(${PROJECT_NAME} libmem_objlib)

if (ALMEM_ISA)
    if (ALMEM_ISA MATCHES "avx")
        target_link_libraries(${PROJECT_NAME} isa_objlib)
    endif ()
else ()
    target_link_libraries(${PROJECT_NAME} uarch_objlib)
    if (ALMEM_DYN_DISPATCH)
        target_link_libraries(${PROJECT_NAME} system_objlib)
    endif ()
    if (ALMEM_TUNABLES)
        target_link_libraries(${PROJECT_NAME} tunable_objlib)
    endif()
endif ()


# static lib
add_library("${PROJECT_NAME}_static" STATIC)
target_link_libraries("${PROJECT_NAME}_static" libmem_objlib)

# renaming static binary
set_target_properties("${PROJECT_NAME}_static" PROPERTIES OUTPUT_NAME ${PROJECT_NAME})

set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION})

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR} CACHE PATH "..." FORCE)
endif()

# installing the target libraries
install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(TARGETS "${PROJECT_NAME}_static" ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
