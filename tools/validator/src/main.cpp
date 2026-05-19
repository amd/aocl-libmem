/* Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file main.cpp
 * @brief Main entry point in the test framework
 */

#include "validators/AllValidators.hpp"
#include "config/Constants.hpp"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>

using namespace libmem::validator;

// ============================================================================
// Usage and Help
// ============================================================================

void printUsage(const char* program_name) {
    std::printf("Usage: %s <function> <size> [src_align] [dst_align] [all_alignments]\n", program_name);
    std::printf("\n");
    std::printf("Arguments:\n");
    std::printf("  function       - Function to validate (e.g., memcpy, strcmp)\n");
    std::printf("  size           - Size parameter for the function\n");
    std::printf("  src_align      - Source alignment offset (optional, default: 0)\n");
    std::printf("  dst_align      - Destination alignment offset (optional, default: 0)\n");
    std::printf("  all_alignments - If 1, test all alignment combinations (optional)\n");
    std::printf("\n");
    std::printf("Options:\n");
    std::printf("  --list-functions    List all supported functions\n");
    std::printf("  --list-tests <fn>   List all tests for a function\n");
    std::printf("  --help              Show this help message\n");
    std::printf("\n");
    std::printf("Supported functions:\n");

    auto names = ValidatorRegistry::instance().getFunctionNames();
    for (const auto& name : names) {
        std::printf("  %s\n", name.c_str());
    }
}

void listFunctions() {
    std::printf("Supported functions:\n");
    auto names = ValidatorRegistry::instance().getFunctionNames();
    for (const auto& name : names) {
        std::printf("  %s\n", name.c_str());
    }
}

void listTests(const char* function_name) {
    auto validator = ValidatorRegistry::instance().create(function_name);
    if (!validator) {
        std::printf("ERROR: Unknown function '%s'\n", function_name);
        listFunctions();
        return;
    }

    std::printf("Registered tests for '%s':\n", function_name);
    auto test_names = validator->getTestNames();
    if (test_names.empty()) {
        std::printf("  (no tests registered)\n");
    } else {
        for (const auto& name : test_names) {
            std::printf("  - %s\n", name.c_str());
        }
        std::printf("\nTotal: %zu tests\n", test_names.size());
    }
}

int main(int argc, char* argv[]) {
    // Initialize random seed
    srand(static_cast<unsigned int>(time(nullptr)));
    
    // Handle special commands
    if (argc >= 2) {
        if (std::strcmp(argv[1], "--help") == 0 || std::strcmp(argv[1], "-h") == 0) {
            printUsage(argv[0]);
            return 0;
        }
        if (std::strcmp(argv[1], "--list-functions") == 0) {
            listFunctions();
            return 0;
        }
        if (std::strcmp(argv[1], "--list-tests") == 0) {
            if (argc < 3) {
                std::printf("ERROR: --list-tests requires a function name\n");
                return 1;
            }
            listTests(argv[2]);
            return 0;
        }
    }

    // Validate arguments
    if (argc < 3) {
        std::printf("ERROR: Function name and size are required\n");
        printUsage(argv[0]);
        return 1;
    }

    // Startup debug info
    DEBUG_SEPARATOR();
    DEBUG_LOG("libmem_validator_unified started");
    DEBUG_LOG("VEC_SZ = %zu bytes", static_cast<size_t>(VEC_SZ));
    DEBUG_LOG("CACHE_LINE_SZ = %zu bytes", static_cast<size_t>(CACHE_LINE_SZ));
    DEBUG_SEPARATOR();

    // Parse arguments
    const char* function_name = argv[1];
    size_t size = std::strtoul(argv[2], nullptr, 10);
    uint32_t src_align = (argc > 3) ? static_cast<uint32_t>(std::atoi(argv[3])) : 0;
    uint32_t dst_align = (argc > 4) ? static_cast<uint32_t>(std::atoi(argv[4])) : 0;
    int all_alignments = (argc > 5) ? std::atoi(argv[5]) : 0;

    // Log parsed parameters
    INFO_LOG("Validating function: %s", function_name);
    INFO_LOG("Size: %zu bytes", size);
    if (all_alignments) {
        INFO_LOG("Alignment mode: All combinations (0-%zu x 0-%zu = %zu tests)",
                static_cast<size_t>(VEC_SZ - 1), static_cast<size_t>(VEC_SZ - 1),
                static_cast<size_t>(VEC_SZ * VEC_SZ));
    } else {
        INFO_LOG("Alignment mode: Single (src=%u, dst=%u)", src_align, dst_align);
    }
    DEBUG_SEPARATOR();

    // Create validator
    auto validator = ValidatorRegistry::instance().create(function_name);
    if (!validator) {
        std::printf("ERROR: Unknown function '%s'\n", function_name);
        listFunctions();
        return 1;
    }

    DEBUG_LOG("Validator created for: %s", validator->getName());

    // Run validation
    if (all_alignments) {
        // Test all alignment combinations
        INFO_LOG("Starting alignment sweep test...");
        for (uint32_t src = 0; src < VEC_SZ; ++src) {
            for (uint32_t dst = 0; dst < VEC_SZ; ++dst) {
                DEBUG_LOG("Testing alignment combination: src=%u, dst=%u", src, dst);
                validator->validate(size, src, dst);
            }
        }
    } else {
        // Single alignment test
        validator->validate(size, src_align, dst_align);
    }

    // Report results
    DEBUG_SEPARATOR();
    const TestStats& stats = validator->getStats();

    INFO_LOG("Validation complete for: %s", function_name);
    INFO_LOG("Results: %zu passed, %zu failed, %zu skipped (total: %zu)",
            stats.passed, stats.failed, stats.skipped, stats.total());

    if (!stats.allPassed()) {
        std::printf("\n%s: %s\n", function_name, stats.summary().c_str());
        return 1;
    }

    INFO_LOG("Status: ALL TESTS PASSED");
    return 0;
}

