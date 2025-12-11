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

#ifndef LIBMEM_VALIDATOR_GTEST_RUNNER_HPP
#define LIBMEM_VALIDATOR_GTEST_RUNNER_HPP


#include "core/FunctionTest.hpp"
#include "traits/MemoryTraits.hpp"
#include "traits/StringTraits.hpp"
#include "tests/common/ZeroSizeTests.hpp"
#include "tests/common/PageCrossTests.hpp"
#include "tests/copy/BasicCopyTests.hpp"
#include "tests/copy/OverlapTests.hpp"
#include "tests/compare/CompareTests.hpp"
#include "tests/fill/FillTests.hpp"
#include "tests/search/SearchTests.hpp"
#include "tests/concat/ConcatTests.hpp"
#include "tests/length/LengthTests.hpp"
#include "validators/string/StrncpyValidator.hpp"
#include "validators/string/StrncmpValidator.hpp"

#include <map>
#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <cstdio>
#include <algorithm>
#include <cctype>

namespace libmem {
namespace validator {
namespace testing {

/**
 * ITestFactory - Abstract base for test factories
 */
class ITestFactory {
public:
    virtual ~ITestFactory() = default;
    virtual TestResult execute(const TestContext& ctx) = 0;
    virtual std::string name() const = 0;
};

/**
 * TestFactory - Creates and executes a specific test type
 */
template<typename TestType>
class TestFactory : public ITestFactory {
public:
    TestResult execute(const TestContext& ctx) override {
        TestType test;
        return test.execute(ctx);
    }

    std::string name() const override {
        TestType test;
        return test.name();
    }
};

/**
 * DynamicTestRegistry - Registry of all available tests for dynamic execution
 */
class DynamicTestRegistry {
public:
    using FactoryPtr = std::unique_ptr<ITestFactory>;
    using FactoryMap = std::map<std::string, FactoryPtr>;
    using FunctionTestMap = std::map<std::string, FactoryMap>;

    static DynamicTestRegistry& instance() {
        static DynamicTestRegistry registry;
        return registry;
    }

    template<typename TestType>
    void registerTest(const std::string& function, const std::string& testName) {
        tests_[toLower(function)][toLower(testName)] =
            std::unique_ptr<ITestFactory>(new TestFactory<TestType>());
    }

    ITestFactory* getTest(const std::string& function, const std::string& testName) {
        auto funcIt = tests_.find(toLower(function));
        if (funcIt == tests_.end()) return nullptr;

        auto testIt = funcIt->second.find(toLower(testName));
        if (testIt == funcIt->second.end()) return nullptr;

        return testIt->second.get();
    }

    std::vector<std::string> getTestNames(const std::string& function) {
        std::vector<std::string> names;
        auto funcIt = tests_.find(toLower(function));
        if (funcIt != tests_.end()) {
            for (const auto& pair : funcIt->second) {
                names.push_back(pair.first);
            }
        }
        return names;
    }

    std::vector<std::string> getFunctions() {
        std::vector<std::string> funcs;
        for (const auto& pair : tests_) {
            funcs.push_back(pair.first);
        }
        return funcs;
    }

    const FunctionTestMap& getAllTests() const { return tests_; }

private:
    FunctionTestMap tests_;

    static std::string toLower(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return result;
    }

    DynamicTestRegistry() {
        registerAllTests();
    }

    void registerAllTests() {
        // --- MEMCPY ---
        registerTest<ZeroSizeCopyTest<traits::MemcpyTag>>("memcpy", "ZeroSize");
        registerTest<BasicCopyTest<traits::MemcpyTag>>("memcpy", "BasicCopy");
        registerTest<PageBoundaryCopyTest<traits::MemcpyTag>>("memcpy", "PageBoundary");
        registerTest<PageOverrunCopyTest<traits::MemcpyTag>>("memcpy", "PageOverrun");

        // --- MEMCMP ---
        registerTest<ZeroSizeCompareTest<traits::MemcmpTag>>("memcmp", "ZeroSize");
        registerTest<EqualCompareTest<traits::MemcmpTag>>("memcmp", "EqualCompare");
        registerTest<MismatchCompareTest<traits::MemcmpTag>>("memcmp", "MismatchCompare");
        registerTest<ByteByByteMismatchTest<traits::MemcmpTag>>("memcmp", "ByteByByteMismatch");
        registerTest<PageBoundaryCompareTest<traits::MemcmpTag>>("memcmp", "PageBoundary");
        registerTest<PageOverrunCompareTest<traits::MemcmpTag>>("memcmp", "PageOverrun");

        // --- MEMSET ---
        registerTest<ZeroSizeFillTest<traits::MemsetTag>>("memset", "ZeroSize");
        registerTest<BasicFillTest<traits::MemsetTag>>("memset", "BasicFill");
        registerTest<PageBoundaryFillTest<traits::MemsetTag>>("memset", "PageBoundary");
        registerTest<PageOverrunFillTest<traits::MemsetTag>>("memset", "PageOverrun");

        // --- MEMMOVE ---
        registerTest<ZeroSizeCopyTest<traits::MemmoveTag>>("memmove", "ZeroSize");
        registerTest<BasicCopyTest<traits::MemmoveTag>>("memmove", "BasicCopy");
        registerTest<ForwardOverlapTest<traits::MemmoveTag>>("memmove", "ForwardOverlap");
        registerTest<BackwardOverlapTest<traits::MemmoveTag>>("memmove", "BackwardOverlap");
        registerTest<InPlaceCopyTest<traits::MemmoveTag>>("memmove", "InPlaceCopy");
        registerTest<PageBoundaryCopyTest<traits::MemmoveTag>>("memmove", "PageBoundary");
        registerTest<PageOverrunCopyTest<traits::MemmoveTag>>("memmove", "PageOverrun");

        // --- MEMPCPY ---
        registerTest<ZeroSizeCopyTest<traits::MempcpyTag>>("mempcpy", "ZeroSize");
        registerTest<BasicCopyTest<traits::MempcpyTag>>("mempcpy", "BasicCopy");
        registerTest<PageBoundaryCopyTest<traits::MempcpyTag>>("mempcpy", "PageBoundary");
        registerTest<PageOverrunCopyTest<traits::MempcpyTag>>("mempcpy", "PageOverrun");

        // --- MEMCHR ---
        registerTest<ZeroSizeSearchTest<traits::MemchrTag>>("memchr", "ZeroSize");
        registerTest<FoundSearchTest<traits::MemchrTag>>("memchr", "FoundSearch");
        registerTest<NotFoundSearchTest<traits::MemchrTag>>("memchr", "NotFoundSearch");
        registerTest<SearchAtStartTest<traits::MemchrTag>>("memchr", "SearchAtStart");
        registerTest<SearchAtEndTest<traits::MemchrTag>>("memchr", "SearchAtEnd");
        registerTest<SearchNullCharTest<traits::MemchrTag>>("memchr", "SearchNullChar");
        registerTest<PageBoundaryMemchrTest<traits::MemchrTag>>("memchr", "PageBoundary");
        registerTest<PageOverrunMemchrTest<traits::MemchrTag>>("memchr", "PageOverrun");

        // --- STRCPY ---
        registerTest<ZeroSizeStringCopyTest<traits::StrcpyTag>>("strcpy", "ZeroSize");
        registerTest<BasicCopyNoSizeTest<traits::StrcpyTag>>("strcpy", "BasicCopy");
        registerTest<MultiNullCopyTest<traits::StrcpyTag>>("strcpy", "MultiNull");
        registerTest<PageBoundaryCopyTest<traits::StrcpyTag>>("strcpy", "PageBoundary");
        registerTest<PageOverrunCopyTest<traits::StrcpyTag>>("strcpy", "PageOverrun");

        // --- STRNCPY ---
        registerTest<ZeroSizeBoundedCopyTest<traits::StrncpyTag>>("strncpy", "ZeroSize");
        registerTest<StrncpyCopyTest>("strncpy", "BasicCopy");
        registerTest<StrncpyStrlenGreaterTest<traits::StrncpyTag>>("strncpy", "StrlenGreater");
        registerTest<StrncpyStrlenEqualTest<traits::StrncpyTag>>("strncpy", "StrlenEqual");
        registerTest<StrncpyStrlenLessTest<traits::StrncpyTag>>("strncpy", "StrlenLess");
        registerTest<PageBoundaryCopyTest<traits::StrncpyTag>>("strncpy", "PageBoundary");
        registerTest<PageOverrunCopyTest<traits::StrncpyTag>>("strncpy", "PageOverrun");

        // --- STRCMP ---
        registerTest<ZeroSizeStringCompareTest<traits::StrcmpTag>>("strcmp", "ZeroSize");
        registerTest<StringCompareEqualTest<traits::StrcmpTag>>("strcmp", "EqualStrings");
        registerTest<StringCompareLessTest<traits::StrcmpTag>>("strcmp", "LessString");
        registerTest<StringCompareGreaterTest<traits::StrcmpTag>>("strcmp", "GreaterString");
        registerTest<ByteByByteMismatchTest<traits::StrcmpTag>>("strcmp", "ByteByByteMismatch");
        registerTest<CompareFirstShorterTest<traits::StrcmpTag>>("strcmp", "FirstShorter");
        registerTest<CompareSecondShorterTest<traits::StrcmpTag>>("strcmp", "SecondShorter");
        registerTest<MultiNullCompareTest<traits::StrcmpTag>>("strcmp", "MultiNull");
        registerTest<PageBoundaryCompareTest<traits::StrcmpTag>>("strcmp", "PageBoundary");
        registerTest<HighByteCompareTest<traits::StrcmpTag>>("strcmp", "HighByte");
        registerTest<ExtendedAsciiCompareTest<traits::StrcmpTag>>("strcmp", "ExtendedAscii");
        registerTest<PageOverrunCompareTest<traits::StrcmpTag>>("strcmp", "PageOverrun");

        // --- STRNCMP ---
        registerTest<StrncmpZeroSizeTest>("strncmp", "ZeroSize");
        registerTest<StringCompareEqualTest<traits::StrncmpTag>>("strncmp", "EqualStrings");
        registerTest<StringCompareLessTest<traits::StrncmpTag>>("strncmp", "LessString");
        registerTest<StringCompareGreaterTest<traits::StrncmpTag>>("strncmp", "GreaterString");
        registerTest<ByteByByteMismatchTest<traits::StrncmpTag>>("strncmp", "ByteByByteMismatch");
        registerTest<StrncmpBeyondNullTest<traits::StrncmpTag>>("strncmp", "StopAtNull");
        registerTest<StrncmpNoNullInRangeTest<traits::StrncmpTag>>("strncmp", "NoNullInRange");
        registerTest<CompareStrlenBothGreaterTest<traits::StrncmpTag>>("strncmp", "BothLonger");
        registerTest<MultiNullCompareTest<traits::StrncmpTag>>("strncmp", "MultiNull");
        registerTest<HighByteCompareTest<traits::StrncmpTag>>("strncmp", "HighByte");
        registerTest<ExtendedAsciiCompareTest<traits::StrncmpTag>>("strncmp", "ExtendedAscii");
        registerTest<PageBoundaryCompareTest<traits::StrncmpTag>>("strncmp", "PageBoundary");
        registerTest<PageOverrunCompareTest<traits::StrncmpTag>>("strncmp", "PageOverrun");

        // --- STRLEN ---
        registerTest<ZeroSizeLengthTest<traits::StrlenTag>>("strlen", "ZeroSize");
        registerTest<BasicLengthTest<traits::StrlenTag>>("strlen", "BasicLength");
        registerTest<PageBoundaryStrlenTest<traits::StrlenTag>>("strlen", "PageBoundary");
        registerTest<PageOverrunLengthTest<traits::StrlenTag>>("strlen", "PageOverrun");

        // --- STRNLEN ---
        registerTest<ZeroSizeBoundedLengthTest<traits::StrnlenTag>>("strnlen", "ZeroSize");
        registerTest<BasicStrnlenTest<traits::StrnlenTag>>("strnlen", "BasicStrnlen");
        registerTest<PageBoundaryStrnlenTest<traits::StrnlenTag>>("strnlen", "PageBoundary");
        registerTest<PageOverrunBoundedLengthTest<traits::StrnlenTag>>("strnlen", "PageOverrun");

        // --- STRCAT ---
        registerTest<ZeroSizeConcatTest<traits::StrcatTag>>("strcat", "ZeroSize");
        registerTest<BasicConcatTest<traits::StrcatTag>>("strcat", "BasicConcat");
        registerTest<MultiNullConcatTest<traits::StrcatTag>>("strcat", "MultiNull");
        registerTest<PageBoundaryConcatTest<traits::StrcatTag>>("strcat", "PageBoundary");
        registerTest<PageOverrunConcatTest<traits::StrcatTag>>("strcat", "PageOverrun");

        // --- STRNCAT ---
        registerTest<ZeroSizeBoundedConcatTest<traits::StrncatTag>>("strncat", "ZeroSize");
        registerTest<StrncatCopyTest<traits::StrncatTag>>("strncat", "BasicConcat");
        registerTest<PageBoundaryConcatTest<traits::StrncatTag>>("strncat", "PageBoundary");
        registerTest<PageOverrunConcatTest<traits::StrncatTag>>("strncat", "PageOverrun");

        // --- STRCHR ---
        registerTest<StringSearchFoundTest<traits::StrchrTag>>("strchr", "CharFound");
        registerTest<StringSearchNotFoundTest<traits::StrchrTag>>("strchr", "CharNotFound");
        registerTest<StringSearchNullCharTest<traits::StrchrTag>>("strchr", "SearchNullChar");
        registerTest<PageBoundaryStrchrTest<traits::StrchrTag>>("strchr", "PageBoundary");
        registerTest<PageOverrunStrchrTest<traits::StrchrTag>>("strchr", "PageOverrun");

        // --- STRSTR ---
        registerTest<StrstrFoundTest<traits::StrstrTag>>("strstr", "SubstringFound");
        registerTest<StrstrNotFoundTest<traits::StrstrTag>>("strstr", "SubstringNotFound");
        registerTest<StrstrEmptyNeedleTest<traits::StrstrTag>>("strstr", "EmptyNeedle");
        registerTest<PageBoundaryStrstrTest<traits::StrstrTag>>("strstr", "PageBoundary");
        registerTest<PageOverrunStrstrTest<traits::StrstrTag>>("strstr", "PageOverrun");

        // --- STRSPN ---
        registerTest<ZeroSizeSpanTest<traits::StrspnTag>>("strspn", "ZeroSize");
        registerTest<StrspnTest<traits::StrspnTag>>("strspn", "BasicSpan");
        registerTest<PageBoundaryStrspnTest<traits::StrspnTag>>("strspn", "PageBoundary");
        registerTest<PageOverrunLengthTest<traits::StrspnTag>>("strspn", "PageOverrun");
    }
};

/**
 * DynamicTestRunner - Runs tests dynamically with any parameters
 */
class DynamicTestRunner {
public:
    struct RunConfig {
        std::string function;
        size_t size;
        uint32_t src_align;
        uint32_t dst_align;
        std::string test_filter;  // Empty = run all tests for function
        bool all_alignments;

        RunConfig() : size(0), src_align(0), dst_align(0), all_alignments(false) {}
    };

    struct RunResult {
        int total;
        int passed;
        int failed;
        std::vector<std::string> failures;

        RunResult() : total(0), passed(0), failed(0) {}
    };

    // Run tests dynamically based on configuration
    static int run(const RunConfig& config) {
        DynamicTestRegistry& registry = DynamicTestRegistry::instance();
        RunResult result;

        std::vector<std::string> testNames;
        if (config.test_filter.empty()) {
            testNames = registry.getTestNames(config.function);
        } else {
            testNames.push_back(config.test_filter);
        }

        if (testNames.empty()) {
            std::printf("[==========] No tests found for function '%s'\n",
                       config.function.c_str());
            return 1;
        }

        // Generate alignment combinations
        std::vector<std::pair<uint32_t, uint32_t>> alignments;
        if (config.all_alignments) {
            for (uint32_t d = 0; d < 64; ++d) {
                for (uint32_t s = 0; s < 64; ++s) {
                    alignments.push_back({d, s});
                }
            }
        } else {
            alignments.push_back({config.dst_align, config.src_align});
        }

        int totalTests = static_cast<int>(testNames.size() * alignments.size());

        std::printf("[==========] Running %d test%s from 1 test suite.\n",
                   totalTests, totalTests == 1 ? "" : "s");
        std::printf("[----------] Global test environment set-up.\n");
        std::printf("[----------] %d test%s from %s\n",
                   totalTests, totalTests == 1 ? "" : "s",
                   config.function.c_str());

        for (const auto& testName : testNames) {
            ITestFactory* factory = registry.getTest(config.function, testName);
            if (!factory) {
                std::printf("[ WARNING  ] Test '%s' not found for function '%s'\n",
                           testName.c_str(), config.function.c_str());
                continue;
            }

            for (const auto& align : alignments) {
                TestContext ctx(config.size, align.first, align.second);

                std::string fullName = formatTestName(config.function, testName, ctx);
                std::printf("[ RUN      ] %s\n", fullName.c_str());

                TestResult testResult = factory->execute(ctx);
                result.total++;

                if (testResult.passed) {
                    result.passed++;
                    std::printf("[       OK ] %s (0 ms)\n", fullName.c_str());
                } else {
                    result.failed++;
                    result.failures.push_back(fullName + ": " + testResult.message);
                    std::printf("[  FAILED  ] %s\n", fullName.c_str());
                    std::printf("             Error: %s\n", testResult.message.c_str());
                    if (testResult.error_index != static_cast<size_t>(-1)) {
                        std::printf("             Error at index: %zu\n", testResult.error_index);
                    }
                }
            }
        }

        std::printf("[----------] %d test%s from %s (0 ms total)\n",
                   result.total, result.total == 1 ? "" : "s",
                   config.function.c_str());
        std::printf("\n");
        std::printf("[----------] Global test environment tear-down\n");
        std::printf("[==========] %d test%s from 1 test suite ran. (0 ms total)\n",
                   result.total, result.total == 1 ? "" : "s");

        if (result.passed > 0) {
            std::printf("[  PASSED  ] %d test%s.\n",
                       result.passed, result.passed == 1 ? "" : "s");
        }

        if (result.failed > 0) {
            std::printf("[  FAILED  ] %d test%s, listed below:\n",
                       result.failed, result.failed == 1 ? "" : "s");
            for (const auto& failure : result.failures) {
                std::printf("[  FAILED  ] %s\n", failure.c_str());
            }
            std::printf("\n %d FAILED TEST%s\n",
                       result.failed, result.failed == 1 ? "" : "S");
        }

        return result.failed > 0 ? 1 : 0;
    }

    // List available tests
    static void listTests() {
        DynamicTestRegistry& registry = DynamicTestRegistry::instance();

        std::printf("Available tests:\n");
        for (const auto& func : registry.getFunctions()) {
            std::printf("\n%s:\n", func.c_str());
            for (const auto& test : registry.getTestNames(func)) {
                std::printf("  - %s\n", test.c_str());
            }
        }
    }

private:
    static std::string formatTestName(const std::string& func,
                                       const std::string& test,
                                       const TestContext& ctx) {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "%s/%s.Run/Size%zu_DstAlign%u_SrcAlign%u",
                     func.c_str(), test.c_str(),
                     ctx.size, ctx.dst_align, ctx.src_align);
        return std::string(buf);
    }
};

} // namespace testing
} // namespace validator
} // namespace libmem

#endif // LIBMEM_VALIDATOR_GTEST_RUNNER_HPP
