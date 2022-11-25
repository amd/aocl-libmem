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

# let the build system know the test directory
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test)

file(WRITE ${CMAKE_BINARY_DIR}/version.h ${LIBMEM_BUILD_VERSION_STR})
