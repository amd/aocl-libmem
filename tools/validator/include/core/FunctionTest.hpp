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

#ifndef LIBMEM_VALIDATOR_FUNCTION_TEST_HPP
#define LIBMEM_VALIDATOR_FUNCTION_TEST_HPP

#include "traits/FunctionTraits.hpp"
#include "config/Constants.hpp"
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

// Debug logging macros
#ifdef LIBMEM_VALIDATOR_VERBOSE
    #define TEST_DEBUG_LOG(fmt, ...) std::printf("[DEBUG]   " fmt "\n", ##__VA_ARGS__)
#else
    #define TEST_DEBUG_LOG(fmt, ...) do {} while(0)
#endif

namespace libmem {
namespace validator {

// ============================================================================
// TestResult - Unified result from test execution
// ============================================================================

struct TestResult {
    bool passed;
    std::string message;
    size_t error_index;

    TestResult() : passed(true), error_index(SIZE_MAX) {}

    static TestResult success() {
        TestResult r;
        r.passed = true;
        return r;
    }

    static TestResult failure(const std::string& msg, size_t idx = SIZE_MAX) {
        TestResult r;
        r.passed = false;
        r.message = msg;
        r.error_index = idx;
        return r;
    }

    static TestResult failure(const char* msg, size_t idx = SIZE_MAX) {
        return failure(std::string(msg), idx);
    }
};

// ============================================================================
// TestContext - Test execution context
// ============================================================================

struct TestContext {
    size_t size;
    uint32_t dst_align;
    uint32_t src_align;
    bool verbose;

    TestContext() : size(0), dst_align(0), src_align(0), verbose(false) {}

    TestContext(size_t sz, uint32_t dst, uint32_t src)
        : size(sz), dst_align(dst), src_align(src), verbose(false) {}
};

// ============================================================================
// BufferPair - Simple buffer management
// ============================================================================

struct BufferPair {
    uint8_t* base;      // Base allocation
    uint8_t* dst;       // Aligned destination
    uint8_t* src;       // Aligned source
    size_t size;        // Usable size

    BufferPair() : base(nullptr), dst(nullptr), src(nullptr), size(0) {}

    ~BufferPair() {
        if (base) {
            std::free(base);
        }
    }

    // Non-copyable
    BufferPair(const BufferPair&) = delete;
    BufferPair& operator=(const BufferPair&) = delete;

    // Movable
    BufferPair(BufferPair&& other)
        : base(other.base), dst(other.dst), src(other.src), size(other.size) {
        other.base = nullptr;
        other.dst = nullptr;
        other.src = nullptr;
        other.size = 0;
    }

    BufferPair& operator=(BufferPair&& other) {
        if (this != &other) {
            if (base) std::free(base);
            base = other.base;
            dst = other.dst;
            src = other.src;
            size = other.size;
            other.base = nullptr;
            other.dst = nullptr;
            other.src = nullptr;
            other.size = 0;
        }
        return *this;
    }
};

// ============================================================================
// FunctionTest - Base class for all tests
// ============================================================================

/**
 * FunctionTest - Unified base class for all test cases
 *
 * Every test in the framework inherits from this class. It provides:
 * - Type-safe function invocation via FunctionTraits<Tag>
 * - Buffer allocation and management
 * - Common validation helpers
 * - Consistent error reporting
 */
template<typename Tag>
class FunctionTest {
public:
    using Traits = traits::FunctionTraits<Tag>;
    using ReturnType = typename Traits::ReturnType;

    virtual ~FunctionTest() = default;

    /**
     * Get the test name (override in derived classes)
     */
    virtual const char* name() const = 0;

    /**
     * Execute the test (override in derived classes)
     *
     * @param ctx Test context with size and alignments
     * @return TestResult indicating pass/fail
     */
    virtual TestResult execute(const TestContext& ctx) = 0;

protected:
    // Current context (set during execute)
    TestContext ctx_;

    // ========================================================================
    // Buffer Allocation
    // ========================================================================

    /**
     * Allocate non-overlapping buffers
     */
    BufferPair allocate(const TestContext& ctx) {
        ctx_ = ctx;
        return allocateNonOverlap(ctx.size, ctx.dst_align, ctx.src_align);
    }

    /**
     * Allocate non-overlapping buffers with explicit alignments
     */
    BufferPair allocateNonOverlap(size_t size, uint32_t dst_align, uint32_t src_align) {
        BufferPair buf;
        buf.size = size;

        size_t alloc_size = 2 * (size + 2 * CACHE_LINE_SZ);
        void* mem = nullptr;
        if (posix_memalign(&mem, CACHE_LINE_SZ, alloc_size) != 0) {
            TEST_DEBUG_LOG("Buffer allocation FAILED: size=%zu", alloc_size);
            return buf;  // Allocation failed
        }

        buf.base = static_cast<uint8_t*>(mem);
        buf.src = buf.base + src_align;
        buf.dst = buf.base + size + 2 * CACHE_LINE_SZ + dst_align;

        TEST_DEBUG_LOG("Allocated non-overlap buffers:");
        TEST_DEBUG_LOG("  base=%p, alloc_size=%zu", static_cast<void*>(buf.base), alloc_size);
        TEST_DEBUG_LOG("  src=%p (offset=%u), dst=%p (offset=%u)",
                      static_cast<void*>(buf.src), src_align,
                      static_cast<void*>(buf.dst), dst_align);
        TEST_DEBUG_LOG("  usable_size=%zu, gap=%zu bytes",
                      size, static_cast<size_t>(buf.dst - buf.src - size));

        return buf;
    }

    /**
     * Allocate single buffer
     */
    BufferPair allocateSingle(size_t size, uint32_t align) {
        BufferPair buf;
        buf.size = size;

        size_t alloc_size = size + 2 * CACHE_LINE_SZ;
        void* mem = nullptr;
        if (posix_memalign(&mem, CACHE_LINE_SZ, alloc_size) != 0) {
            TEST_DEBUG_LOG("Single buffer allocation FAILED: size=%zu", alloc_size);
            return buf;
        }

        buf.base = static_cast<uint8_t*>(mem);
        buf.dst = buf.base + CACHE_LINE_SZ + align;
        buf.src = buf.dst;  // Same as dst

        TEST_DEBUG_LOG("Allocated single buffer:");
        TEST_DEBUG_LOG("  base=%p, ptr=%p (offset=%u), size=%zu",
                      static_cast<void*>(buf.base), static_cast<void*>(buf.dst), align, size);

        return buf;
    }

    /**
     * Allocate overlapping buffers
     */
    BufferPair allocateOverlap(size_t size, uint32_t dst_align, uint32_t src_align) {
        BufferPair buf;
        buf.size = size;

        size_t alloc_size = 2 * (size + 2 * CACHE_LINE_SZ);
        void* mem = nullptr;
        if (posix_memalign(&mem, CACHE_LINE_SZ, alloc_size) != 0) {
            TEST_DEBUG_LOG("Overlap buffer allocation FAILED: size=%zu", alloc_size);
            return buf;
        }

        buf.base = static_cast<uint8_t*>(mem);
        // For overlap, src and dst share memory
        buf.src = buf.base + src_align;
        buf.dst = buf.base + CACHE_LINE_SZ + dst_align;

        // Calculate overlap: how many bytes of src+size extend past dst
        ptrdiff_t src_end = static_cast<ptrdiff_t>(reinterpret_cast<uintptr_t>(buf.src + size));
        ptrdiff_t dst_start = static_cast<ptrdiff_t>(reinterpret_cast<uintptr_t>(buf.dst));
        ptrdiff_t overlap = src_end - dst_start;
        TEST_DEBUG_LOG("Allocated overlapping buffers:");
        TEST_DEBUG_LOG("  base=%p, src=%p, dst=%p",
                      static_cast<void*>(buf.base), static_cast<void*>(buf.src),
                      static_cast<void*>(buf.dst));
        TEST_DEBUG_LOG("  overlap_bytes=%td, size=%zu", overlap > 0 ? overlap : 0, size);

        return buf;
    }

    // ========================================================================
    // Data Filling Helpers
    // ========================================================================

    /**
     * Fill buffer with random lowercase letters ('a'-'z')
     */
    void fillLowercaseLetters(uint8_t* buf, size_t size) {
        for (size_t i = 0; i < size; ++i) {
            buf[i] = 'a' + (rand() % LOWER_CHARS);
        }
    }

    /**
     * Fill buffer with random printable ASCII (32-126)
     */
    void fillPrintableAscii(uint8_t* buf, size_t size) {
        for (size_t i = 0; i < size; ++i) {
            buf[i] = MIN_PRINTABLE_ASCII + (rand() % (MAX_PRINTABLE_ASCII - MIN_PRINTABLE_ASCII + 1));
        }
    }

    /**
     * Fill buffer with a single repeated byte value
     */
    void fillWithByte(uint8_t* buf, size_t size, uint8_t value) {
        std::memset(buf, value, size);
    }

    /**
     * Fill buffer with full byte range (0-255)
     * @param avoid_null If true, replace 0 with non-zero value (default: true)
     */
    void fillFullByteRange(uint8_t* buf, size_t size, bool avoid_null = true) {
        for (size_t i = 0; i < size; ++i) {
            uint8_t val = rand() & 255;
            if (avoid_null && val == 0) {
                val = 1 + (rand() & 127);  // Replace with 1-128
            }
            buf[i] = val;
        }
    }

    /**
     * Fill buffer with random non-null bytes (1-255)
     * Useful for string tests where null terminates early
     */
    void fillNonNull(uint8_t* buf, size_t size) {
        for (size_t i = 0; i < size; ++i) {
            buf[i] = static_cast<uint8_t>((std::rand() % 255) + 1);
        }
    }

    /**
     * Shuffle of a char array
     */
    void shuffleChars(char* buf, int len) {
        for (int i = len - 1; i > 0; --i) {
            int j = rand() % (i + 1);
            char t = buf[i]; buf[i] = buf[j]; buf[j] = t;
        }
    }

    /**
     * Add NULL terminator
     */
    void nullTerminate(uint8_t* buf, size_t pos) {
        buf[pos] = '\0';
    }

    /**
     * Copy data between buffers
     */
    void copyData(uint8_t* dst, const uint8_t* src, size_t size) {
        std::memcpy(dst, src, size);
    }

    // ========================================================================
    // Boundary Checking
    // ========================================================================

    /**
     * Prepare boundary markers before and after buffer
     */
    void prepareBoundary(uint8_t* buf, size_t size) {
        for (size_t i = 1; i <= BOUNDARY_BYTES; ++i) {
            *(buf - i) = '#';
            *(buf + size + i - 1) = '#';
        }
    }

    /**
     * Check boundary markers are intact
     */
    bool checkBoundary(uint8_t* buf, size_t size) {
        for (size_t i = 1; i <= BOUNDARY_BYTES; ++i) {
            if (*(buf - i) != '#' || *(buf + size + i - 1) != '#') {
                return false;
            }
        }
        return true;
    }

    // ========================================================================
    // Function Invocation
    // ========================================================================

    /**
     * Invoke the function with 3 arguments
     */
    template<typename Arg1, typename Arg2, typename Arg3>
    ReturnType invoke(Arg1 a1, Arg2 a2, Arg3 a3) {
        return Traits::invoke(a1, a2, a3);
    }

    /**
     * Invoke the function with 2 arguments
     */
    template<typename Arg1, typename Arg2>
    ReturnType invoke(Arg1 a1, Arg2 a2) {
        return Traits::invoke(a1, a2);
    }

    /**
     * Invoke the function with 1 argument
     */
    template<typename Arg1>
    ReturnType invoke(Arg1 a1) {
        return Traits::invoke(a1);
    }

    // ========================================================================
    // Common Validation Helpers
    // ========================================================================

    /**
     * Validate copy: check data matches and return value
     */
    TestResult validateCopy(const BufferPair& buf, void* ret_value) {
        // Check data match
        for (size_t i = 0; i < buf.size; ++i) {
            if (buf.dst[i] != buf.src[i]) {
                return error("data mismatch at index %zu, expected=0x%02x, actual=0x%02x",
                            i, buf.src[i], buf.dst[i]);
            }
        }

        // Check return value
        void* expected = Traits::returns_dest
            ? static_cast<void*>(buf.dst)
            : (Traits::return_category == traits::ReturnCategory::END_PTR
                ? static_cast<void*>(buf.dst + buf.size)
                : static_cast<void*>(buf.dst));

        if (ret_value != expected) {
            return error("return value mismatch, expected=%p, actual=%p", expected, ret_value);
        }

        return TestResult::success();
    }

    /**
     * Validate fill: check all bytes are the expected value
     */
    TestResult validateFill(const BufferPair& buf, uint8_t expected_value, void* ret_value) {
        for (size_t i = 0; i < buf.size; ++i) {
            if (buf.dst[i] != expected_value) {
                return error("fill mismatch at index %zu, expected=0x%02x, actual=0x%02x",
                            i, expected_value, buf.dst[i]);
            }
        }

        if (ret_value != buf.dst) {
            return error("return value mismatch, expected=%p, actual=%p", buf.dst, ret_value);
        }

        return TestResult::success();
    }

    /**
     * Validate comparison result
     */
    TestResult validateCompare(int actual, int expected) {
        // Normalize to sign
        int sign_actual = (actual > 0) ? 1 : (actual < 0) ? -1 : 0;
        int sign_expected = (expected > 0) ? 1 : (expected < 0) ? -1 : 0;

        if (sign_actual != sign_expected) {
            return error("comparison mismatch, expected sign=%d, actual sign=%d (actual=%d)",
                        sign_expected, sign_actual, actual);
        }
        return TestResult::success();
    }

    /**
     * Validate comparison result (exact match)
     */
    TestResult validateCompareExact(int actual, int expected) {
        if (actual != expected) {
            return error("comparison mismatch, expected=%d, actual=%d", expected, actual);
        }
        return TestResult::success();
    }

    /**
     * Validate found search result
     */
    TestResult validateFound(void* actual, void* expected) {
        if (actual != expected) {
            return error("search result mismatch, expected=%p, actual=%p", expected, actual);
        }
        return TestResult::success();
    }

    /**
     * Validate not found search result
     */
    TestResult validateNotFound(void* actual) {
        if (actual != nullptr) {
            return error("expected NULL but got %p", actual);
        }
        return TestResult::success();
    }

    /**
     * Validate size/length result
     */
    TestResult validateSize(size_t actual, size_t expected) {
        if (actual != expected) {
            return error("size mismatch, expected=%zu, actual=%zu", expected, actual);
        }
        return TestResult::success();
    }

    /**
     * Validate boundary check passed
     */
    TestResult validateBoundary(uint8_t* buf, size_t size) {
        if (!checkBoundary(buf, size)) {
            return error("boundary check failed (buffer overrun detected)");
        }
        return TestResult::success();
    }

    // ========================================================================
    // Error Formatting
    // ========================================================================

    /**
     * Create formatted error message with no format arguments
     */
    TestResult error(const char* message) {
        char full_msg[1024];
        std::snprintf(full_msg, sizeof(full_msg),
            "ERROR:[%s:%s] %s [size=%zu, dst_align=%u, src_align=%u]",
            Traits::name(), name(), message, ctx_.size, ctx_.dst_align, ctx_.src_align);
        return TestResult::failure(full_msg);
    }

    /**
     * Create formatted error message with format arguments
     */
    template<typename T, typename... Args>
    TestResult error(const char* format, T first, Args... args) {
        char buf[512];
        std::snprintf(buf, sizeof(buf), format, first, args...);
        char full_msg[1024];
        std::snprintf(full_msg, sizeof(full_msg),
            "ERROR:[%s:%s] %s [size=%zu, dst_align=%u, src_align=%u]",
            Traits::name(), name(), buf, ctx_.size, ctx_.dst_align, ctx_.src_align);
        return TestResult::failure(full_msg);
    }

    /**
     * Create formatted error at specific index with no format arguments
     */
    TestResult errorAt(size_t index, const char* message) {
        char full_msg[1024];
        std::snprintf(full_msg, sizeof(full_msg),
            "ERROR:[%s:%s] %s @index=%zu [size=%zu, dst_align=%u, src_align=%u]",
            Traits::name(), name(), message, index, ctx_.size, ctx_.dst_align, ctx_.src_align);
        return TestResult::failure(full_msg, index);
    }

    /**
     * Create formatted error at specific index with format arguments
     */
    template<typename T, typename... Args>
    TestResult errorAt(size_t index, const char* format, T first, Args... args) {
        char buf[512];
        std::snprintf(buf, sizeof(buf), format, first, args...);

        char full_msg[1024];
        std::snprintf(full_msg, sizeof(full_msg),
            "ERROR:[%s:%s] %s @index=%zu [size=%zu, dst_align=%u, src_align=%u]",
            Traits::name(), name(), buf, index, ctx_.size, ctx_.dst_align, ctx_.src_align);
        return TestResult::failure(full_msg, index);
    }
};

// ============================================================================
// IFunctionTest - Type-erased interface for test registration
// ============================================================================

/**
 * IFunctionTest - Interface for all tests
 *
 * Allows storing different FunctionTest<Tag> types in a container.
 */
class IFunctionTest {
public:
    virtual ~IFunctionTest() = default;
    virtual const char* name() const = 0;
    virtual TestResult execute(const TestContext& ctx) = 0;
};

/**
 * Wrapper to adapt FunctionTest<Tag> to IFunctionTest
 */
template<typename TestType>
class FunctionTestWrapper : public IFunctionTest {
    TestType test_;

public:
    FunctionTestWrapper() = default;

    const char* name() const override {
        return test_.name();
    }

    TestResult execute(const TestContext& ctx) override {
        return test_.execute(ctx);
    }
};

} // namespace validator
} // namespace libmem

#endif // LIBMEM_VALIDATOR_FUNCTION_TEST_HPP

