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

#ifndef LIBMEM_VALIDATOR_FUNCTION_VALIDATOR_HPP
#define LIBMEM_VALIDATOR_FUNCTION_VALIDATOR_HPP

#include "core/FunctionTest.hpp"
#include "traits/FunctionTraits.hpp"
#include "config/Constants.hpp"
#include <vector>
#include <memory>
#include <functional>
#include <cstdio>

// ============================================================================
// Debug Logging Infrastructure
// ============================================================================

#ifdef LIBMEM_VALIDATOR_VERBOSE
    // Debug level logging - detailed information for developers
    #define DEBUG_LOG(fmt, ...) std::printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)

    // Info level logging - key events and state changes
    #define INFO_LOG(fmt, ...) std::printf("[INFO]  " fmt "\n", ##__VA_ARGS__)

    // Test result logging - pass/fail with context
    #define PASS_LOG(fmt, ...) std::printf("[PASS]  " fmt "\n", ##__VA_ARGS__)

    // Section separator for readability (major sections use =, subsections use -)
    #define DEBUG_SEPARATOR() std::printf("[DEBUG] ========================================\n")
    #define DEBUG_SUBSEPARATOR() std::printf("[DEBUG] ----------------------------------------\n")

    // Legacy macro for backward compatibility
    #define UNIFIED_LOG_VERBOSE(fmt, ...) std::printf(fmt, ##__VA_ARGS__)
#else
    #define DEBUG_LOG(fmt, ...) do {} while(0)
    #define INFO_LOG(fmt, ...) do {} while(0)
    #define PASS_LOG(fmt, ...) do {} while(0)
    #define DEBUG_SEPARATOR() do {} while(0)
    #define DEBUG_SUBSEPARATOR() do {} while(0)
    #define UNIFIED_LOG_VERBOSE(fmt, ...) do {} while(0)
#endif

namespace libmem {
namespace validator {

// ============================================================================
// TestRegistry - Interface for test registration
// ============================================================================

/**
 * TestEntry - Entry in the test registry
 */
struct TestEntry {
    std::function<std::unique_ptr<IFunctionTest>()> factory;
    const char* description;
    bool is_zero_size;

    TestEntry(std::function<std::unique_ptr<IFunctionTest>()> f, const char* desc, bool zero)
        : factory(f), description(desc), is_zero_size(zero) {}
};

/**
 * TestRegistry - Interface for registering tests
 */
class TestRegistry {
    std::vector<TestEntry> entries_;

public:
    /**
     * Register a regular test
     */
    template<typename TestType>
    TestRegistry& add(const char* description) {
        entries_.push_back(TestEntry(
            []() { return std::unique_ptr<IFunctionTest>(new FunctionTestWrapper<TestType>()); },
            description,
            false
        ));
        return *this;
    }

    /**
     * Register a zero-size test
     */
    template<typename TestType>
    TestRegistry& zeroSize(const char* description) {
        entries_.push_back(TestEntry(
            []() { return std::unique_ptr<IFunctionTest>(new FunctionTestWrapper<TestType>()); },
            description,
            true
        ));
        return *this;
    }

    /**
     * Check if registry has any tests
     */
    bool empty() const { return entries_.empty(); }

    /**
     * Get number of tests
     */
    size_t size() const { return entries_.size(); }

    /**
     * Get all entries
     */
    const std::vector<TestEntry>& entries() const { return entries_; }

    /**
     * Get test names
     */
    std::vector<std::string> getTestNames() const {
        std::vector<std::string> names;
        for (const auto& e : entries_) {
            auto test = e.factory();
            names.push_back(test->name());
        }
        return names;
    }
};

// ============================================================================
// TestStats - Statistics from running tests
// ============================================================================

struct TestStats {
    size_t passed;
    size_t failed;
    size_t skipped;

    TestStats() : passed(0), failed(0), skipped(0) {}

    size_t total() const { return passed + failed + skipped; }
    bool allPassed() const { return failed == 0; }

    void merge(const TestStats& other) {
        passed += other.passed;
        failed += other.failed;
        skipped += other.skipped;
    }

    std::string summary() const {
        char buf[128];
        if (allPassed()) {
            std::snprintf(buf, sizeof(buf), "PASS (%zu/%zu tests)", passed, total());
        } else {
            std::snprintf(buf, sizeof(buf), "FAIL (%zu passed, %zu failed)", passed, failed);
        }
        return std::string(buf);
    }
};

// ============================================================================
// IValidator - Interface for all validators
// ============================================================================

class IValidator {
public:
    virtual ~IValidator() = default;
    virtual const char* getName() const = 0;
    virtual void validate(size_t size, uint32_t align1, uint32_t align2) = 0;
    virtual const TestStats& getStats() const = 0;
    virtual std::vector<std::string> getTestNames() const = 0;
};

// ============================================================================
// FunctionValidator - Unified validator base
// ============================================================================

/**
 * FunctionValidator - Unified base class for all validators
 */
template<typename Tag, typename Derived>
class FunctionValidator : public IValidator {
public:
    using Traits = traits::FunctionTraits<Tag>;

    FunctionValidator() {
        // Use CRTP to call derived registerTests at compile-time
        static_cast<Derived*>(this)->registerTests();
    }

    virtual ~FunctionValidator() = default;

    // ========================================================================
    // IValidator interface
    // ========================================================================

    const char* getName() const override {
        return Traits::name();
    }

    void validate(size_t size, uint32_t align1, uint32_t align2) final override {
        // Normalize alignments
        uint32_t src_align = align1 % VEC_SZ;
        uint32_t dst_align = align2 % VEC_SZ;

        DEBUG_LOG("Function: %s", Traits::name());
        DEBUG_LOG("  Size: %zu bytes", size);
        DEBUG_LOG("  Src alignment offset: %u (original: %u)", src_align, align1);
        DEBUG_LOG("  Dst alignment offset: %u (original: %u)", dst_align, align2);
        DEBUG_LOG("  VEC_SZ: %zu bytes", static_cast<size_t>(VEC_SZ));

        // Handle size==0 with zero-size tests
        if (size == 0) {
            DEBUG_LOG("  Mode: Zero-size tests");
            runZeroSizeTests(dst_align, src_align);
            return;
        }

        DEBUG_LOG("  Mode: Regular tests");
        DEBUG_LOG("  Registered tests: %zu", registry_.size());

        // Run regular tests
        runRegularTests(size, dst_align, src_align);
    }

    const TestStats& getStats() const override {
        return stats_;
    }

    // ========================================================================
    // Test introspection
    // ========================================================================

    std::vector<std::string> getTestNames() const override {
        return registry_.getTestNames();
    }

protected:
    /**
     * Get the test registry
     */
    TestRegistry& tests() { return registry_; }

private:
    TestRegistry registry_;
    TestStats stats_;

    /**
     * Run all regular tests
     */
    void runRegularTests(size_t size, uint32_t dst_align, uint32_t src_align) {
        TestContext ctx(size, dst_align, src_align);

        size_t test_num = 0;
        size_t total_regular = 0;

        // Count regular tests
        for (const auto& entry : registry_.entries()) {
            if (!entry.is_zero_size) total_regular++;
        }

        DEBUG_SUBSEPARATOR();
        DEBUG_LOG("Running %zu regular tests for %s", total_regular, Traits::name());

        for (const auto& entry : registry_.entries()) {
            if (entry.is_zero_size) continue;

            auto test = entry.factory();
            test_num++;

            DEBUG_LOG("Test [%zu/%zu]: %s::%s", test_num, total_regular,
                     Traits::name(), test->name());
            DEBUG_LOG("  Parameters: size=%zu, dst_align=%u, src_align=%u",
                     size, dst_align, src_align);

            TestResult result = test->execute(ctx);

            if (result.passed) {
                stats_.passed++;
                PASS_LOG("%s::%s [size=%zu, dst=%u, src=%u]",
                        Traits::name(), test->name(), size, dst_align, src_align);
            } else {
                stats_.failed++;
                std::printf("[FAIL]  %s::%s [size=%zu, dst=%u, src=%u]\n",
                           Traits::name(), test->name(), size, dst_align, src_align);
                std::printf("        %s\n", result.message.c_str());
            }
        }

        DEBUG_LOG("Completed: %zu passed, %zu failed",
                 stats_.passed, stats_.failed);
    }

    /**
     * Run all zero-size tests
     */
    void runZeroSizeTests(uint32_t dst_align, uint32_t src_align) {
        TestContext ctx(0, dst_align, src_align);

        size_t test_num = 0;
        size_t total_zero = 0;

        // Count zero-size tests
        for (const auto& entry : registry_.entries()) {
            if (entry.is_zero_size) total_zero++;
        }

        if (total_zero == 0) {
            DEBUG_LOG("No zero-size tests registered for %s", Traits::name());
            return;
        }

        DEBUG_SUBSEPARATOR();
        DEBUG_LOG("Running %zu zero-size tests for %s", total_zero, Traits::name());

        for (const auto& entry : registry_.entries()) {
            if (!entry.is_zero_size) continue;

            auto test = entry.factory();
            test_num++;

            DEBUG_LOG("Test [%zu/%zu]: %s::%s (zero-size)", test_num, total_zero,
                     Traits::name(), test->name());
            DEBUG_LOG("  Parameters: size=0, dst_align=%u, src_align=%u",
                     dst_align, src_align);

            TestResult result = test->execute(ctx);

            if (result.passed) {
                stats_.passed++;
                PASS_LOG("%s::%s [size=0, dst=%u, src=%u]",
                        Traits::name(), test->name(), dst_align, src_align);
            } else {
                stats_.failed++;
                std::printf("[FAIL]  %s::%s [size=0, dst=%u, src=%u]\n",
                           Traits::name(), test->name(), dst_align, src_align);
                std::printf("        %s\n", result.message.c_str());
            }
        }

        DEBUG_LOG("Completed: %zu passed, %zu failed",
                 stats_.passed, stats_.failed);
    }
};

// ============================================================================
// ValidatorRegistry - Global registry for all validators
// ============================================================================

/**
 * ValidatorRegistry - Singleton registry for validator factories
 */
class ValidatorRegistry {
public:
    using FactoryFn = std::function<std::unique_ptr<IValidator>()>;

    static ValidatorRegistry& instance() {
        static ValidatorRegistry reg;
        return reg;
    }

    /**
     * Register a validator factory
     */
    void registerValidator(const char* name, FactoryFn factory) {
        factories_.push_back(std::make_pair(std::string(name), factory));
    }

    /**
     * Create a validator by name
     */
    std::unique_ptr<IValidator> create(const char* name) const {
        for (const auto& entry : factories_) {
            if (entry.first == name) {
                return entry.second();
            }
        }
        return nullptr;
    }

    /**
     * Get all registered function names
     */
    std::vector<std::string> getFunctionNames() const {
        std::vector<std::string> names;
        for (const auto& entry : factories_) {
            names.push_back(entry.first);
        }
        return names;
    }

    /**
     * Check if a function is registered
     */
    bool isRegistered(const char* name) const {
        for (const auto& entry : factories_) {
            if (entry.first == name) return true;
        }
        return false;
    }

private:
    ValidatorRegistry() = default;
    std::vector<std::pair<std::string, FactoryFn>> factories_;
};

/**
 * Helper macro to auto-register validators
 */
#define REGISTER_VALIDATOR(ValidatorClass) \
    namespace { \
        struct ValidatorClass##Registrar { \
            ValidatorClass##Registrar() { \
                ValidatorRegistry::instance().registerValidator( \
                    ValidatorClass().getName(), \
                    []() { return std::unique_ptr<IValidator>(new ValidatorClass()); } \
                ); \
            } \
        } g_##ValidatorClass##Registrar; \
    }

} // namespace validator
} // namespace libmem

#endif // LIBMEM_VALIDATOR_FUNCTION_VALIDATOR_HPP

