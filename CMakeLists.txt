cmake_minimum_required(VERSION 3.10)

# avoid build in root directory
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
	message(FATAL_ERROR “In-source build detected!”)
endif()


# set the project name and version
set(LIBMEM_VERSION_STRING 4.0)

project(aocl-libmem VERSION ${LIBMEM_VERSION_STRING} LANGUAGES C DESCRIPTION
                        "Library of AMD optimized string/memory functions")

string(TIMESTAMP BUILD_DATE "%Y%m%d")

set(LIBMEM_BUILD_VERSION_STR
    "AOCL-LibMem ${LIBMEM_VERSION_STRING} Build ${BUILD_DATE}")

add_definitions(-DLIBMEM_BUILD_VERSION="${LIBMEM_BUILD_VERSION_STR}")

set(DEFAULT_BUILD_TYPE "Release")

option(ENABLE_LOGGING "Enable Logger" OFF)

option(ENABLE_TUNABLES "Enable user input" OFF)

if (ENABLE_LOGGING)
add_definitions(-DENABLE_LOGGER)
# uncomment the below for debug logs, LOG_LEVEL=DEBUG.
#add_definitions(-DLOG_LEVEL=4)
endif ()

execute_process(COMMAND bash "-c" "lscpu | grep erms"
        RESULT_VARIABLE ERMS_FEATURE OUTPUT_QUIET)

if (NOT ERMS_FEATURE)
    #uncomment after addng ERMS support for all funcs
    #add_definitions(-DERMS_FEATURE_ENABLED)
    message("ERMS CPU Feature Enabled.")
endif ()

option(USE_AVX512 "Use AVX512 Feature" OFF)

execute_process(COMMAND bash "-c" "lscpu | grep avx512"
        RESULT_VARIABLE AVX512_STATUS OUTPUT_QUIET)

if (USE_AVX512 OR NOT AVX512_STATUS)
    set(AVX512_FEATURE_AVAILABLE true)
    add_definitions(-DAVX512_FEATURE_ENABLED)
    message("AVX512 CPU Feature Enabled.")
endif ()

# option for building shared lib
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

# let the build system know the source directory
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src)

# let the build system know the test directory
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test)

file(WRITE ${CMAKE_BINARY_DIR}/version.h ${LIBMEM_BUILD_VERSION_STR})
