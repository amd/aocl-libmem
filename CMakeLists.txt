cmake_minimum_required(VERSION 3.10)

# avoid build in root directory
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
	message(FATAL_ERROR “In-source build detected!”)
endif()

# set the project name and version
project(aocl-libmem VERSION 3.2.1 LANGUAGES C DESCRIPTION "Library of AMD optimized string/memory functions")

set(DEFAULT_BUILD_TYPE "Release")

option(ENABLE_LOGGING "Enable Logger" OFF)

if (ENABLE_LOGGING)
add_definitions(-DENABLE_LOGGER)
# uncomment the below for debug logs, LOG_LEVEL=DEBUG.
#add_definitions(-DLOG_LEVEL=4)
endif ()

# option for building shared lib
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

# let the build system know the source directory
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src)

# let the build system know the test directory
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test)
