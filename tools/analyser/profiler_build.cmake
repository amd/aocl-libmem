# Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
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

# =============================================================================
# profiler_build.cmake — C++ profiler (prof_libmem) build configuration
#
# This file is included by CMakeLists.txt when ALMEM_TOOLS=ON.
# It handles:
#   - BPF-capable clang detection and validation
#   - Kernel UAPI header resolution (system or libbpf fallback)
#   - BPF program compilation and embedding
#   - C++ profiler executable build (prof_libmem)
#   - Correctness test application build (profiler_test)
# =============================================================================

message(STATUS "Configuring C++ profiler (prof_libmem) build...")

# =============================================================================
# Required build tools: clang (BPF compiler) and xxd (binary embedder)
# =============================================================================
find_program(CLANG clang REQUIRED)
find_program(XXD xxd REQUIRED)

if(NOT CLANG)
    message(FATAL_ERROR "clang not found. Install clang package (build tool only).")
endif()

if(NOT XXD)
    message(FATAL_ERROR "xxd not found. Install xxd (usually part of vim-common package).")
endif()

# Validate that the found clang supports the BPF target
execute_process(
    COMMAND ${CLANG} --print-targets
    OUTPUT_VARIABLE CLANG_TARGETS
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(FIND "${CLANG_TARGETS}" "bpf" _bpf_idx)
if(_bpf_idx EQUAL -1)
    message(STATUS "Found clang (${CLANG}) does NOT support BPF target (vendor compiler?)")
    message(STATUS "Searching for a BPF-capable system clang...")

    # Search for a BPF-capable clang: try versioned binaries (newest first)
    set(_BPF_CLANG_FOUND OFF)
    set(_BPF_CLANG_CANDIDATES "")

    foreach(_ver 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7)
        foreach(_dir /usr/local/bin /usr/bin /bin)
            if(EXISTS "${_dir}/clang-${_ver}")
                list(APPEND _BPF_CLANG_CANDIDATES "${_dir}/clang-${_ver}")
            endif()
        endforeach()
    endforeach()

    foreach(_dir /usr/local/bin /usr/bin /bin)
        if(EXISTS "${_dir}/clang")
            list(APPEND _BPF_CLANG_CANDIDATES "${_dir}/clang")
        endif()
    endforeach()

    foreach(_clang_path ${_BPF_CLANG_CANDIDATES})
        execute_process(
            COMMAND ${_clang_path} --print-targets
            OUTPUT_VARIABLE _sys_targets
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        string(FIND "${_sys_targets}" "bpf" _sys_bpf_idx)
        if(NOT _sys_bpf_idx EQUAL -1)
            file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/_bpf_test.c"
                "int test(void *ctx) { return 0; }\n")
            execute_process(
                COMMAND ${_clang_path} -target bpf -O2 -c
                    "${CMAKE_CURRENT_BINARY_DIR}/_bpf_test.c"
                    -o "${CMAKE_CURRENT_BINARY_DIR}/_bpf_test.o"
                RESULT_VARIABLE _bpf_test_rc
                ERROR_QUIET
            )
            file(REMOVE "${CMAKE_CURRENT_BINARY_DIR}/_bpf_test.c"
                        "${CMAKE_CURRENT_BINARY_DIR}/_bpf_test.o")
            if(_bpf_test_rc EQUAL 0)
                set(CLANG "${_clang_path}")
                set(_BPF_CLANG_FOUND ON)
                message(STATUS "Found BPF-capable clang: ${CLANG}")
                break()
            endif()
        endif()
    endforeach()

    if(NOT _BPF_CLANG_FOUND)
        message(FATAL_ERROR
            "No BPF-capable clang found. The BPF compiler backend is required.\n"
            "Install a standard LLVM/Clang package:\n"
            "  Debian/Ubuntu: sudo apt-get install clang\n"
            "  RHEL/Fedora:   sudo dnf install clang\n"
            "  SUSE:          sudo zypper install clang")
    endif()
else()
    # Verify found clang can actually compile BPF
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/_bpf_test.c"
        "int test(void *ctx) { return 0; }\n")
    execute_process(
        COMMAND ${CLANG} -target bpf -O2 -c
            "${CMAKE_CURRENT_BINARY_DIR}/_bpf_test.c"
            -o "${CMAKE_CURRENT_BINARY_DIR}/_bpf_test.o"
        RESULT_VARIABLE _bpf_verify_rc
        ERROR_QUIET
    )
    file(REMOVE "${CMAKE_CURRENT_BINARY_DIR}/_bpf_test.c"
                "${CMAKE_CURRENT_BINARY_DIR}/_bpf_test.o")
    if(_bpf_verify_rc EQUAL 0)
        message(STATUS "Found clang: ${CLANG} (BPF target verified)")
    else()
        message(STATUS "Found clang (${CLANG}) reports BPF but fails to compile, searching alternatives...")
        set(_bpf_idx -1)
    endif()
endif()

# If initial clang failed BPF verification, search for alternatives
if(_bpf_idx EQUAL -1 AND NOT _BPF_CLANG_FOUND)
    set(_BPF_CLANG_FOUND OFF)
    set(_BPF_CLANG_CANDIDATES "")

    foreach(_ver 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7)
        foreach(_dir /usr/local/bin /usr/bin /bin)
            if(EXISTS "${_dir}/clang-${_ver}")
                list(APPEND _BPF_CLANG_CANDIDATES "${_dir}/clang-${_ver}")
            endif()
        endforeach()
    endforeach()

    foreach(_dir /usr/local/bin /usr/bin /bin)
        if(EXISTS "${_dir}/clang")
            list(APPEND _BPF_CLANG_CANDIDATES "${_dir}/clang")
        endif()
    endforeach()

    foreach(_clang_path ${_BPF_CLANG_CANDIDATES})
        if("${_clang_path}" STREQUAL "${CLANG}")
            continue()
        endif()
        execute_process(
            COMMAND ${_clang_path} --print-targets
            OUTPUT_VARIABLE _sys_targets
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        string(FIND "${_sys_targets}" "bpf" _sys_bpf_idx)
        if(NOT _sys_bpf_idx EQUAL -1)
            file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/_bpf_test.c"
                "int test(void *ctx) { return 0; }\n")
            execute_process(
                COMMAND ${_clang_path} -target bpf -O2 -c
                    "${CMAKE_CURRENT_BINARY_DIR}/_bpf_test.c"
                    -o "${CMAKE_CURRENT_BINARY_DIR}/_bpf_test.o"
                RESULT_VARIABLE _bpf_test_rc
                ERROR_QUIET
            )
            file(REMOVE "${CMAKE_CURRENT_BINARY_DIR}/_bpf_test.c"
                        "${CMAKE_CURRENT_BINARY_DIR}/_bpf_test.o")
            if(_bpf_test_rc EQUAL 0)
                set(CLANG "${_clang_path}")
                set(_BPF_CLANG_FOUND ON)
                message(STATUS "Found BPF-capable clang: ${CLANG}")
                break()
            endif()
        endif()
    endforeach()

    if(NOT _BPF_CLANG_FOUND)
        message(FATAL_ERROR
            "No working BPF-capable clang found. The BPF compiler backend is required.\n"
            "Install a standard LLVM/Clang (7+) package:\n"
            "  Debian/Ubuntu: sudo apt-get install clang\n"
            "  RHEL/Fedora:   sudo dnf install clang\n"
            "  SUSE:          sudo zypper install clang")
    endif()
endif()

# =============================================================================
# Kernel UAPI headers (<linux/bpf.h>, <linux/perf_event.h>)
# =============================================================================
include(CheckIncludeFile)

set(FALLBACK_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/fallback_include")
set(LIBBPF_VERSION "1.4.0")
set(LIBBPF_DIR_NAME "libbpf-${LIBBPF_VERSION}")
set(LIBBPF_TARBALL_NAME "${LIBBPF_DIR_NAME}.tar.gz")
set(LIBBPF_DOWNLOAD_URL "https://github.com/libbpf/libbpf/archive/refs/tags/v${LIBBPF_VERSION}.tar.gz")

check_include_file("linux/bpf.h" HAVE_LINUX_BPF_H)
check_include_file("linux/perf_event.h" HAVE_LINUX_PERF_EVENT_H)
check_include_file("elf.h" HAVE_ELF_H)

set(NEED_FALLBACK_HEADERS OFF)
set(EXTRA_INCLUDE_DIRS "")

if(NOT HAVE_LINUX_BPF_H OR NOT HAVE_LINUX_PERF_EVENT_H)
    set(NEED_FALLBACK_HEADERS ON)
endif()

if(NOT HAVE_ELF_H)
    message(FATAL_ERROR
        "<elf.h> not found. Install libc development headers:\n"
        "  Debian/Ubuntu: sudo apt-get install libc6-dev\n"
        "  RHEL/Fedora:   sudo dnf install glibc-devel\n"
        "  SUSE:          sudo zypper install glibc-devel")
endif()

if(NEED_FALLBACK_HEADERS)
    set(LIBBPF_LOCAL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/${LIBBPF_DIR_NAME}")
    set(LIBBPF_LOCAL_TARBALL "${CMAKE_CURRENT_SOURCE_DIR}/${LIBBPF_TARBALL_NAME}")
    set(LIBBPF_BUILD_SRC "${CMAKE_CURRENT_BINARY_DIR}/${LIBBPF_DIR_NAME}")
    set(LIBBPF_BUILD_TARBALL "${CMAKE_CURRENT_BINARY_DIR}/${LIBBPF_TARBALL_NAME}")
    set(LIBBPF_UAPI_DIR "")

    if(EXISTS "${LIBBPF_LOCAL_SRC}/include/uapi/linux/bpf.h")
        set(LIBBPF_UAPI_DIR "${LIBBPF_LOCAL_SRC}/include/uapi")
    elseif(EXISTS "${LIBBPF_LOCAL_TARBALL}")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar xzf "${LIBBPF_LOCAL_TARBALL}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
            RESULT_VARIABLE TAR_RESULT
        )
        if(TAR_RESULT EQUAL 0 AND EXISTS "${LIBBPF_BUILD_SRC}/include/uapi/linux/bpf.h")
            set(LIBBPF_UAPI_DIR "${LIBBPF_BUILD_SRC}/include/uapi")
        endif()
    elseif(EXISTS "${LIBBPF_BUILD_SRC}/include/uapi/linux/bpf.h")
        set(LIBBPF_UAPI_DIR "${LIBBPF_BUILD_SRC}/include/uapi")
    elseif(EXISTS "${LIBBPF_BUILD_TARBALL}")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar xzf "${LIBBPF_BUILD_TARBALL}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
            RESULT_VARIABLE TAR_RESULT
        )
        if(TAR_RESULT EQUAL 0 AND EXISTS "${LIBBPF_BUILD_SRC}/include/uapi/linux/bpf.h")
            set(LIBBPF_UAPI_DIR "${LIBBPF_BUILD_SRC}/include/uapi")
        endif()
    endif()

    # Download if nothing found locally
    if("${LIBBPF_UAPI_DIR}" STREQUAL "")
        file(DOWNLOAD
            "${LIBBPF_DOWNLOAD_URL}"
            "${LIBBPF_BUILD_TARBALL}"
            STATUS DOWNLOAD_STATUS
            TIMEOUT 60
            TLS_VERIFY ON
        )
        list(GET DOWNLOAD_STATUS 0 DOWNLOAD_RC)

        if(NOT DOWNLOAD_RC EQUAL 0)
            find_program(WGET wget)
            if(WGET)
                execute_process(
                    COMMAND ${WGET} -q -O "${LIBBPF_BUILD_TARBALL}" "${LIBBPF_DOWNLOAD_URL}"
                    RESULT_VARIABLE WGET_RC TIMEOUT 60
                )
                if(WGET_RC EQUAL 0)
                    set(DOWNLOAD_RC 0)
                endif()
            endif()
        endif()

        if(NOT DOWNLOAD_RC EQUAL 0)
            find_program(CURL curl)
            if(CURL)
                execute_process(
                    COMMAND ${CURL} -sL -o "${LIBBPF_BUILD_TARBALL}" "${LIBBPF_DOWNLOAD_URL}"
                    RESULT_VARIABLE CURL_RC TIMEOUT 60
                )
                if(CURL_RC EQUAL 0)
                    set(DOWNLOAD_RC 0)
                endif()
            endif()
        endif()

        if(DOWNLOAD_RC EQUAL 0 AND EXISTS "${LIBBPF_BUILD_TARBALL}")
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E tar xzf "${LIBBPF_BUILD_TARBALL}"
                WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
                RESULT_VARIABLE TAR_RESULT
            )
            if(TAR_RESULT EQUAL 0 AND EXISTS "${LIBBPF_BUILD_SRC}/include/uapi/linux/bpf.h")
                set(LIBBPF_UAPI_DIR "${LIBBPF_BUILD_SRC}/include/uapi")
            else()
                message(FATAL_ERROR
                    "Failed to extract libbpf tarball.\n"
                    "Install kernel headers: sudo apt-get install linux-libc-dev\n"
                    "Or place libbpf-${LIBBPF_VERSION}.tar.gz in ${CMAKE_CURRENT_SOURCE_DIR}/")
            endif()
        else()
            message(FATAL_ERROR
                "Failed to download libbpf source. Cannot build without kernel UAPI headers.\n"
                "Install kernel headers or place libbpf-${LIBBPF_VERSION}.tar.gz in source directory.")
        endif()
    endif()

    if(NOT "${LIBBPF_UAPI_DIR}" STREQUAL "")
        list(APPEND EXTRA_INCLUDE_DIRS "${LIBBPF_UAPI_DIR}")
    endif()
endif()

# =============================================================================
# Compile BPF program to ELF object file at build time
# =============================================================================
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/profiler.bpf.o
    COMMAND ${CLANG} -target bpf -O2 -g -Wall
            -c ${CMAKE_CURRENT_SOURCE_DIR}/profiler.bpf.c
            -o ${CMAKE_CURRENT_BINARY_DIR}/profiler.bpf.o
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/profiler.bpf.c
    COMMENT "Compiling BPF program with clang"
    VERBATIM
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/profiler.bpf.h
    COMMAND ${XXD} -i profiler.bpf.o > ${CMAKE_CURRENT_BINARY_DIR}/profiler.bpf.h
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/profiler.bpf.o
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Embedding BPF object as byte array"
    VERBATIM
)

add_custom_target(bpf_object
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/profiler.bpf.h
)

# =============================================================================
# C++ profiler executable (prof_libmem)
# =============================================================================
add_executable(prof_libmem EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/prof_libmem.cpp)

set_target_properties(prof_libmem PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools/analyser
)

add_dependencies(prof_libmem bpf_object)

target_compile_features(prof_libmem PRIVATE cxx_std_14)

target_include_directories(prof_libmem PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${EXTRA_INCLUDE_DIRS}
)

target_compile_options(prof_libmem PRIVATE
    -Wall -Wextra
    -fno-exceptions -fno-rtti
    -fno-asynchronous-unwind-tables -fno-unwind-tables
)

target_link_options(prof_libmem PRIVATE
    -static-libstdc++ -static-libgcc
    -Wl,--no-as-needed
)

target_link_libraries(prof_libmem PRIVATE dl)

install(CODE "
    set(_prof \"${CMAKE_BINARY_DIR}/tools/analyser/prof_libmem\")
    if(EXISTS \"\${_prof}\")
        file(INSTALL DESTINATION \"${CMAKE_BINARY_DIR}/tools/analyser\"
             TYPE EXECUTABLE FILES \"\${_prof}\")
        # Install profiler helper scripts alongside the built binary
        file(INSTALL DESTINATION \"${CMAKE_BINARY_DIR}/tools/analyser\"
             TYPE PROGRAM
             FILES \"${CMAKE_CURRENT_SOURCE_DIR}/prof_libmem.py\"
                   \"${CMAKE_CURRENT_SOURCE_DIR}/profiler_test_runner.sh\"
                   \"${CMAKE_CURRENT_SOURCE_DIR}/profiler_verifier.py\")
    else()
        message(STATUS \"prof_libmem not built - skipping install (build with 'make tools')\")
    endif()
")

# =============================================================================
# Correctness test application (profiler_test)
# =============================================================================
add_executable(profiler_test EXCLUDE_FROM_ALL
    ${CMAKE_CURRENT_SOURCE_DIR}/profiler_test.c)

set_target_properties(profiler_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools/analyser
)

target_link_libraries(profiler_test PRIVATE pthread)

target_compile_options(profiler_test PRIVATE
    -Wall -Wextra -O0 -fno-inline -fno-builtin)

message(STATUS "C++ profiler targets configured (build with: make tools)")
