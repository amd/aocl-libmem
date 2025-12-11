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

#ifndef LIBMEM_VALIDATOR_PAGE_CROSS_TESTS_HPP
#define LIBMEM_VALIDATOR_PAGE_CROSS_TESTS_HPP

/**
 * @file PageCrossTests.hpp
 * @brief Page boundary and page overrun test implementations
 *
 * PageBoundary tests: correctness when data crosses page boundaries
 * PageOverrun tests: bounds safety with a trap page at buffer end
 */

#include "core/PageCrossTestBase.hpp"
#include <cstring>

namespace libmem {
namespace validator {

// ============================================================================
// ============================================================================
//
//  PAGE BOUNDARY TESTS
//
//  Tests correctness when buffers cross internal page boundaries.
//  Inherits from PageBoundaryTestBase (offset x position iteration).
//
// ============================================================================
// ============================================================================

// ============================================================================
// PageBoundaryCopyTest
// ============================================================================

/**
 * Tests: memcpy, memmove, mempcpy, strcpy, strncpy
 * - Source and destination span multiple pages
 * - Uses invoke_helper::copy for unified SFINAE dispatch
 */
template<typename Tag>
class PageBoundaryCopyTest : public PageBoundaryTestBase<Tag> {
protected:
    bool usePositions() const override { return false; }

    TestResult runPageBoundaryTest(size_t size, size_t /*pos*/,
                            uint32_t off1, uint32_t off2) override {
        PageCrossBuffer src_buf, dst_buf;

        if (!src_buf.allocate(size) || !dst_buf.allocate(size)) {
            return this->error("allocation failed");
        }

        this->seedRandom(size, 0, off1, off2);
        this->fillFullByteRange(src_buf.base(), src_buf.usableSize());

        uint8_t* src = src_buf.getAddressNearBoundary(off1);
        uint8_t* dst = dst_buf.getAddressNearBoundary(off2);

        if (!src || !dst) {
            return this->error("invalid boundary offset");
        }

        if (!this->fitsInBuffer(src, size, src_buf) ||
            !this->fitsInBuffer(dst, size, dst_buf)) {
            return TestResult::success();
        }

        if (FunctionTest<Tag>::Traits::is_string_func) {
            for (size_t i = 0; i < size - 1; ++i) {
                if (src[i] == 0) src[i] = 1;
            }
            src[size - 1] = '\0';
        }

        invoke_helper::copy<Tag>(dst, src, size);

        for (size_t i = 0; i < size; ++i) {
            if (dst[i] != src[i]) {
                return this->errorAt(i, "copy mismatch at offset %zu", i);
            }
        }

        return TestResult::success();
    }
};

// ============================================================================
// PageBoundaryCompareTest
// ============================================================================

/**
 * Tests: memcmp, strcmp, strncmp
 * - Full byte range (catches signed comparison bug)
 * - Mismatch at various positions including page boundaries
 * - Different alignments trigger misaligned code path
 */
template<typename Tag>
class PageBoundaryCompareTest : public PageBoundaryTestBase<Tag> {
protected:
    bool requiresDifferentOffsets() const override { return true; }

    TestResult runPageBoundaryTest(size_t size, size_t mismatch_pos,
                            uint32_t off1, uint32_t off2) override {
        PageCrossBuffer buf1, buf2;

        if (!buf1.allocate(size) || !buf2.allocate(size)) {
            return this->error("allocation failed");
        }

        this->seedRandom(size, mismatch_pos, off1, off2);
        this->fillFullByteRange(buf1.base(), buf1.usableSize());
        this->copyData(buf2.base(), buf1.base(), buf1.usableSize());

        uint8_t* str1 = buf1.getAddressNearBoundary(off1);
        uint8_t* str2 = buf2.getAddressNearBoundary(off2);

        if (!str1 || !str2) {
            return this->error("invalid boundary offset");
        }

        if (str1 + size > buf1.base() + buf1.usableSize() ||
            str2 + size > buf2.base() + buf2.usableSize()) {
            return TestResult::success();
        }

        this->copyData(str2, str1, size);

        uint8_t val1 = str1[mismatch_pos];
        uint8_t val2 = (val1 > 140) ? (val1 - 20) : (val1 + 20);
        str2[mismatch_pos] = val2;

        int expected = static_cast<int>(val1) - static_cast<int>(val2);
        int expected_sign = (expected > 0) ? 1 : ((expected < 0) ? -1 : 0);

        if (FunctionTest<Tag>::Traits::is_string_func) {
            str1[size - 1] = '\0';
            str2[size - 1] = '\0';
        }

        int ret = invoke_helper::compare<Tag>(str1, str2, size);
        int ret_sign = (ret > 0) ? 1 : ((ret < 0) ? -1 : 0);

        if (ret_sign != expected_sign) {
            return this->error(
                "compare mismatch: size=%zu pos=%zu off1=%u off2=%u "
                "val1=0x%02x val2=0x%02x expected=%d got=%d",
                size, mismatch_pos, off1, off2, val1, val2, expected_sign, ret_sign);
        }

        return TestResult::success();
    }
};

// ============================================================================
// PageBoundaryFillTest
// ============================================================================

/**
 * Tests: memset
 * - Destination spans multiple pages
 * - Various fill values including high-bytes
 */
template<typename Tag>
class PageBoundaryFillTest : public PageBoundaryTestBase<Tag> {
protected:
    bool usePositions() const override { return false; }

    TestResult runPageBoundaryTest(size_t size, size_t /*pos*/,
                            uint32_t off1, uint32_t /*off2*/) override {
        auto setup = this->setupBuffer(size, off1);
        if (setup.should_skip) return TestResult::success();
        if (!setup.valid) return this->error("allocation failed");

        uint8_t* dst = setup.ptr;

        static const int test_values[] = {0x00, 0x41, 0x7F, 0x80, 0xFF};
        for (int value : test_values) {
            this->invoke(dst, value, size);

            for (size_t i = 0; i < size; ++i) {
                if (dst[i] != static_cast<uint8_t>(value)) {
                    return this->errorAt(i, "fill mismatch: expected=0x%02x got=0x%02x",
                                         value, dst[i]);
                }
            }
        }

        return TestResult::success();
    }
};

// ============================================================================
// PageBoundaryMemchrTest
// ============================================================================

/**
 * Tests: memchr
 * - Buffer spans multiple pages
 * - Search character at various positions including page boundaries
 */
template<typename Tag>
class PageBoundaryMemchrTest : public PageBoundaryTestBase<Tag> {
protected:
    TestResult runPageBoundaryTest(size_t size, size_t search_pos,
                            uint32_t off1, uint32_t /*off2*/) override {
        auto setup = this->setupBuffer(size, off1);
        if (setup.should_skip) return TestResult::success();
        if (!setup.valid) return this->error("allocation failed");

        this->seedRandom(size, search_pos, off1, 0);
        this->fillFullByteRange(setup.pcb.base(), setup.pcb.usableSize());

        uint8_t* str = setup.ptr;

        const uint8_t search_char = 0xFF;
        for (size_t i = 0; i < size; ++i) {
            if (str[i] == search_char) str[i] = 0xFE;
        }
        str[search_pos] = search_char;

        void* ret = this->invoke(str, static_cast<int>(search_char), size);
        void* expected = str + search_pos;

        if (ret != expected) {
            return this->error("search mismatch: pos=%zu expected=%p got=%p",
                               search_pos, expected, ret);
        }

        return TestResult::success();
    }
};

// ============================================================================
// PageBoundaryStrlenTest
// ============================================================================

/**
 * Tests: strlen
 * - String spans multiple pages
 * - Null terminator at various positions including page boundaries
 */
template<typename Tag>
class PageBoundaryStrlenTest : public PageBoundaryTestBase<Tag> {
protected:
    TestResult runPageBoundaryTest(size_t size, size_t null_pos,
                            uint32_t off1, uint32_t /*off2*/) override {
        PageCrossBuffer buf;

        if (!buf.allocate(size + 1)) {
            return this->error("allocation failed");
        }

        this->seedRandom(size, null_pos, off1, 0);
        this->fillNonNull(buf.base(), buf.usableSize());

        uint8_t* str = buf.getAddressNearBoundary(off1);

        if (!str || str + null_pos + 1 > buf.base() + buf.usableSize()) {
            return TestResult::success();
        }

        str[null_pos] = '\0';

        size_t ret = FunctionTest<Tag>::Traits::invoke(
            reinterpret_cast<const char*>(str));

        if (ret != null_pos) {
            return this->error("length mismatch: expected=%zu got=%zu", null_pos, ret);
        }

        return TestResult::success();
    }
};

// ============================================================================
// PageBoundaryStrnlenTest
// ============================================================================

/**
 * Tests: strnlen
 * - String spans multiple pages
 * - Validates both clamped (maxlen < strlen) and unclamped cases
 */
template<typename Tag>
class PageBoundaryStrnlenTest : public PageBoundaryTestBase<Tag> {
protected:
    TestResult runPageBoundaryTest(size_t size, size_t null_pos,
                            uint32_t off1, uint32_t /*off2*/) override {
        PageCrossBuffer buf;

        if (!buf.allocate(size + 1)) {
            return this->error("allocation failed");
        }

        this->seedRandom(size, null_pos, off1, 0);
        this->fillNonNull(buf.base(), buf.usableSize());

        uint8_t* str = buf.getAddressNearBoundary(off1);

        if (!str || str + null_pos + 1 > buf.base() + buf.usableSize()) {
            return TestResult::success();
        }

        str[null_pos] = '\0';

        size_t maxlen = size;
        size_t ret = FunctionTest<Tag>::Traits::invoke(
            reinterpret_cast<const char*>(str), maxlen);

        if (ret != null_pos) {
            return this->error("unclamped mismatch: expected=%zu got=%zu (maxlen=%zu)",
                               null_pos, ret, maxlen);
        }

        if (null_pos > 1) {
            size_t clamped_max = null_pos / 2;
            ret = FunctionTest<Tag>::Traits::invoke(
                reinterpret_cast<const char*>(str), clamped_max);

            if (ret != clamped_max) {
                return this->error("clamped mismatch: expected=%zu got=%zu (maxlen=%zu)",
                                   clamped_max, ret, clamped_max);
            }
        }

        return TestResult::success();
    }
};

// ============================================================================
// PageBoundaryConcatTest
// ============================================================================

/**
 * Tests: strcat, strncat
 * - Source spans multiple pages
 */
template<typename Tag>
class PageBoundaryConcatTest : public PageBoundaryTestBase<Tag> {
protected:
    size_t minTestSize() const override { return 16; }
    bool usePositions() const override { return false; }

    TestResult runPageBoundaryTest(size_t size, size_t /*pos*/,
                            uint32_t off1, uint32_t off2) override {
        size_t src_len = size / 2;
        PageCrossBuffer src_buf, dst_buf;

        if (!src_buf.allocate(src_len + 1) || !dst_buf.allocate(size + 2)) {
            return this->error("allocation failed");
        }

        this->seedRandom(size, 0, off1, off2);
        this->fillFullByteRange(src_buf.base(), src_buf.usableSize());

        uint8_t* src = src_buf.getAddressNearBoundary(off1);
        uint8_t* dst = dst_buf.getAddressNearBoundary(off2);

        if (!src || !dst) {
            return this->error("invalid boundary offset");
        }

        if (src + src_len + 1 > src_buf.base() + src_buf.usableSize() ||
            dst + size + 2 > dst_buf.base() + dst_buf.usableSize()) {
            return TestResult::success();
        }

        for (size_t i = 0; i < src_len; ++i) {
            if (src[i] == 0) src[i] = 1;
        }
        src[src_len] = '\0';

        dst[0] = 'X';
        dst[1] = '\0';

        char* ret = invoke_helper::concat<Tag>(dst, src, src_len);

        if (dst[0] != 'X') {
            return this->error("prefix modified");
        }

        for (size_t i = 0; i < src_len; ++i) {
            if (dst[1 + i] != src[i]) {
                return this->errorAt(i, "concat mismatch");
            }
        }

        if (ret != reinterpret_cast<char*>(dst)) {
            return this->error("return value mismatch");
        }

        return TestResult::success();
    }
};

// ============================================================================
// PageBoundaryStrchrTest
// ============================================================================

/**
 * Tests: strchr
 * - String spans multiple pages
 * - Search character at various positions
 */
template<typename Tag>
class PageBoundaryStrchrTest : public PageBoundaryTestBase<Tag> {
protected:
    TestResult runPageBoundaryTest(size_t size, size_t search_pos,
                            uint32_t off1, uint32_t /*off2*/) override {
        PageCrossBuffer buf;

        if (!buf.allocate(size + 1)) {
            return this->error("allocation failed");
        }

        this->seedRandom(size, search_pos, off1, 0);
        this->fillFullByteRange(buf.base(), buf.usableSize());

        uint8_t* str = buf.getAddressNearBoundary(off1);

        if (!str || str + size + 1 > buf.base() + buf.usableSize()) {
            return TestResult::success();
        }

        const char search_char = '!';
        for (size_t i = 0; i < size; ++i) {
            if (str[i] == 0 || str[i] == search_char) {
                str[i] = 'a' + (i % 26);
            }
        }
        str[size] = '\0';
        str[search_pos] = search_char;

        char* ret = FunctionTest<Tag>::Traits::invoke(
            reinterpret_cast<char*>(str), static_cast<int>(search_char));

        char* expected = reinterpret_cast<char*>(str + search_pos);
        if (ret != expected) {
            return this->error("strchr mismatch: pos=%zu expected=%p got=%p",
                               search_pos, expected, ret);
        }

        return TestResult::success();
    }
};

// ============================================================================
// PageBoundaryStrstrTest
// ============================================================================

/**
 * Tests: strstr
 * - Haystack spans multiple pages
 * - Needle at various positions
 */
template<typename Tag>
class PageBoundaryStrstrTest : public PageBoundaryTestBase<Tag> {
protected:
    size_t minTestSize() const override { return 16; }

    TestResult runPageBoundaryTest(size_t size, size_t needle_pos,
                            uint32_t off1, uint32_t /*off2*/) override {
        PageCrossBuffer buf;

        if (!buf.allocate(size + 1)) {
            return this->error("allocation failed");
        }

        this->seedRandom(size, needle_pos, off1, 0);

        uint8_t* haystack = buf.getAddressNearBoundary(off1);

        if (!haystack || haystack + size + 1 > buf.base() + buf.usableSize()) {
            return TestResult::success();
        }

        for (size_t i = 0; i < size; ++i) {
            haystack[i] = 'a' + (i % 13);
        }
        haystack[size] = '\0';

        const char* needle = "xyz";
        size_t needle_len = 3;

        if (needle_pos + needle_len >= size) {
            return TestResult::success();
        }

        for (size_t i = 0; i < needle_len; ++i) {
            haystack[needle_pos + i] = needle[i];
        }

        char* ret = FunctionTest<Tag>::Traits::invoke(
            reinterpret_cast<char*>(haystack), needle);

        char* expected = reinterpret_cast<char*>(haystack + needle_pos);
        if (ret != expected) {
            return this->error("strstr mismatch: pos=%zu expected=%p got=%p",
                               needle_pos, expected, ret);
        }

        return TestResult::success();
    }
};

// ============================================================================
// Page-Cross Strspn Test (short accept, PCMPISTRI path)
// ============================================================================

/**
 * PageCrossStrspnTest - s near page boundary, short accept (<= 16 chars)
 *
 * Tests: strspn
 * - String spans multiple pages with randomized accept
 */
template<typename Tag>
class PageBoundaryStrspnTest : public PageBoundaryTestBase<Tag> {
protected:
    TestResult runPageBoundaryTest(size_t size, size_t span_len,
                            uint32_t off1, uint32_t /*off2*/) override {
        PageCrossBuffer buf;

        if (!buf.allocate(size + 1)) {
            return this->error("allocation failed");
        }

        uint8_t* str = buf.getAddressNearBoundary(off1);

        if (!str || str + size + 1 > buf.base() + buf.usableSize()) {
            return TestResult::success();
        }

        char alpha[27];
        for (int i = 0; i < 26; ++i) alpha[i] = 'a' + i;
        alpha[26] = '\0';
        this->shuffleChars(alpha, 26);
        const int accept_len = 10;
        char reject = alpha[accept_len];
        alpha[accept_len] = '\0';

        for (size_t i = 0; i < span_len && i < size; ++i)
            str[i] = alpha[rand() % accept_len];

        for (size_t i = span_len; i < size; ++i)
            str[i] = reject;
        str[size] = '\0';

        size_t expected = (span_len < size) ? span_len : size;
        size_t ret = FunctionTest<Tag>::Traits::invoke(
            reinterpret_cast<const char*>(str), alpha);

        if (ret != expected) {
            return this->error("strspn page-cross: expected=%zu got=%zu", expected, ret);
        }

        return TestResult::success();
    }
};


// ============================================================================
// Page-Cross Accept Strspn Test
// ============================================================================

/**
 * AcceptPageCrossStrspnTest - accept string straddles a page boundary
 *
 * Tests the masked load handling in the accept loading path. Catches bugs
 * where accept is silently truncated when its null terminator falls on the
 * next page.
 */
template<typename Tag>
class AcceptPageCrossStrspnTest : public PageBoundaryTestBase<Tag> {
public:
    const char* name() const override { return "AcceptPageCross"; }

protected:
    bool usePositions() const override { return false; }

    TestResult runPageBoundaryTest(size_t size, size_t /*pos*/,
                            uint32_t off1, uint32_t /*off2*/) override {
        static constexpr size_t ACCEPT_OFFSETS[] = {11, 12, 13, 14, 15};

        char alpha[27];
        for (int i = 0; i < 26; ++i) alpha[i] = 'a' + i;
        alpha[26] = '\0';
        this->shuffleChars(alpha, 26);
        const int accept_len = 10;
        char reject = alpha[accept_len];
        alpha[accept_len] = '\0';

        PageCrossBuffer s_buf;
        if (!s_buf.allocate(size + 1)) {
            return this->error("allocation failed for s");
        }

        uint8_t* str = s_buf.getAddressNearBoundary(off1);
        if (!str || str + size + 1 > s_buf.base() + s_buf.usableSize()) {
            return TestResult::success();
        }

        PageCrossBuffer accept_buf;
        if (!accept_buf.allocate(accept_len + 1)) {
            return this->error("allocation failed for accept");
        }

        for (size_t a_off : ACCEPT_OFFSETS) {
            uint8_t* accept_ptr = accept_buf.base() +
                                  accept_buf.usableSize() - a_off;

            if (accept_ptr < accept_buf.base()) continue;

            for (int i = 0; i < accept_len; ++i)
                accept_ptr[i] = (uint8_t)alpha[i];
            accept_ptr[accept_len] = '\0';

            for (size_t i = 0; i < size; ++i)
                str[i] = alpha[rand() % accept_len];
            str[size] = '\0';

            size_t ret = FunctionTest<Tag>::Traits::invoke(
                reinterpret_cast<const char*>(str),
                reinterpret_cast<const char*>(accept_ptr));

            if (ret != size) {
                return this->error(
                    "accept page-cross: a_off=%zu expected=%zu got=%zu",
                    a_off, size, ret);
            }

            if (size > 1) {
                size_t mid = 1 + (rand() % (size - 1));
                char orig = str[mid];
                str[mid] = reject;

                ret = FunctionTest<Tag>::Traits::invoke(
                    reinterpret_cast<const char*>(str),
                    reinterpret_cast<const char*>(accept_ptr));

                str[mid] = orig;

                if (ret != mid) {
                    return this->error(
                        "accept page-cross mismatch: a_off=%zu expected=%zu got=%zu",
                        a_off, mid, ret);
                }
            }
        }

        return TestResult::success();
    }
};

// ============================================================================
// Both s AND accept Page-Cross Strspn Test
// ============================================================================

/**
 * BothPageCrossStrspnTest - both s and accept straddle page boundaries
 *
 * Catches interaction bugs where page-boundary handling in the s scan loop
 * and the accept load path interfere with each other (e.g., register
 * clobbering, flag corruption between the two masked-load paths).
 */
template<typename Tag>
class BothPageCrossStrspnTest : public PageBoundaryTestBase<Tag> {
public:
    const char* name() const override { return "BothPageCross"; }

protected:
    bool usePositions() const override { return false; }
    size_t minTestSize() const override { return 8; }

    TestResult runPageBoundaryTest(size_t size, size_t /*pos*/,
                            uint32_t off1, uint32_t /*off2*/) override {
        if (size > PAGE_SZ) return TestResult::success();

        char alpha[27];
        for (int i = 0; i < 26; ++i) alpha[i] = 'a' + i;
        alpha[26] = '\0';
        this->shuffleChars(alpha, 26);
        const int accept_len = 10;
        char reject = alpha[accept_len];
        alpha[accept_len] = '\0';

        // Offsets must be >= accept_len+1 so the full string (incl. null)
        // fits in usable memory; the SIMD over-read is what crosses into
        // the guard page, exercising the masked-load path.
        static constexpr size_t ACCEPT_OFFSETS[] = {11, 12, 13, 14, 15};

        // s near page boundary
        PageCrossBuffer s_buf;
        if (!s_buf.allocate(size + 1))
            return this->error("allocation failed for s");

        uint8_t* str = s_buf.getAddressNearBoundary(off1);
        if (!str || str + size + 1 > s_buf.base() + s_buf.usableSize())
            return TestResult::success();

        // accept near page boundary (separate allocation)
        PageCrossBuffer accept_buf;
        if (!accept_buf.allocate(accept_len + 1))
            return this->error("allocation failed for accept");

        for (size_t a_off : ACCEPT_OFFSETS) {
            uint8_t* accept_ptr = accept_buf.base() +
                                  accept_buf.usableSize() - a_off;
            if (accept_ptr < accept_buf.base()) continue;

            for (int i = 0; i < accept_len; ++i)
                accept_ptr[i] = (uint8_t)alpha[i];
            accept_ptr[accept_len] = '\0';

            // Full-span test: all chars in accept → expect size
            for (size_t i = 0; i < size; ++i)
                str[i] = alpha[rand() % accept_len];
            str[size] = '\0';

            size_t ret = FunctionTest<Tag>::Traits::invoke(
                reinterpret_cast<const char*>(str),
                reinterpret_cast<const char*>(accept_ptr));

            if (ret != size) {
                return this->error(
                    "both page-cross: a_off=%zu off1=%u expected=%zu got=%zu",
                    a_off, off1, size, ret);
            }

            // Mismatch test: inject reject at midpoint
            if (size > 1) {
                size_t mid = 1 + (rand() % (size - 1));
                char orig = str[mid];
                str[mid] = reject;

                ret = FunctionTest<Tag>::Traits::invoke(
                    reinterpret_cast<const char*>(str),
                    reinterpret_cast<const char*>(accept_ptr));

                str[mid] = orig;

                if (ret != mid) {
                    return this->error(
                        "both page-cross mismatch: a_off=%zu off1=%u expected=%zu got=%zu",
                        a_off, off1, mid, ret);
                }
            }
        }

        return TestResult::success();
    }
};

// ============================================================================
// ============================================================================
//
//  PAGE OVERRUN TESTS
//
//  Tests bounds safety by positioning buffers so the operation region ends
//  exactly at a trap page (PROT_NONE). Any read or write past the region
//  causes an immediate SEGFAULT.
//
//  Inherits from PageOverrunTestBase.
//
// ============================================================================
// ============================================================================

// ============================================================================
// PageOverrunCopyTest
// ============================================================================

/**
 * Tests: memcpy, memmove, mempcpy, strcpy, strncpy
 */
template<typename Tag>
class PageOverrunCopyTest : public PageOverrunTestBase<Tag> {
protected:
    TestResult runPageOverrunTest(size_t size) override {
        auto src = this->setupAtGuard(size);
        if (!src.valid) return this->error("src allocation failed");
        this->fillForTest(src.ptr, size);

        auto dst = this->setupAtGuard(size);
        if (!dst.valid) return this->error("dst allocation failed");
        std::memset(dst.ptr, 0, size);

        invoke_helper::copy<Tag>(dst.ptr, src.ptr, size);

        for (size_t i = 0; i < size; ++i) {
            if (dst.ptr[i] != src.ptr[i])
                return this->errorAt(i, "copy mismatch");
        }

        if (FunctionTest<Tag>::Traits::is_bounded &&
            FunctionTest<Tag>::Traits::is_string_func && size > 0) {
            auto src_nn = this->setupAtGuard(size);
            if (!src_nn.valid) return this->error("src no-null allocation failed");
            this->fillNonNull(src_nn.ptr, size);

            auto dst_nn = this->setupAtGuard(size);
            if (!dst_nn.valid) return this->error("dst no-null allocation failed");
            std::memset(dst_nn.ptr, 0, size);

            invoke_helper::copy<Tag>(dst_nn.ptr, src_nn.ptr, size);

            for (size_t i = 0; i < size; ++i) {
                if (dst_nn.ptr[i] != src_nn.ptr[i])
                    return this->errorAt(i, "no-null copy mismatch");
            }
        }

        return TestResult::success();
    }
};

// ============================================================================
// PageOverrunFillTest
// ============================================================================

/**
 * Tests: memset
 */
template<typename Tag>
class PageOverrunFillTest : public PageOverrunTestBase<Tag> {
protected:
    TestResult runPageOverrunTest(size_t size) override {
        auto buf = this->setupAtGuard(size);
        if (!buf.valid) return this->error("allocation failed");

        static const int test_values[] = {0x00, 0x41, 0x7F, 0x80, 0xFF};
        for (int value : test_values) {
            this->invoke(buf.ptr, value, size);

            for (size_t i = 0; i < size; ++i) {
                if (buf.ptr[i] != static_cast<uint8_t>(value))
                    return this->errorAt(i, "fill mismatch: expected=0x%02x got=0x%02x",
                                         value, buf.ptr[i]);
            }
        }
        return TestResult::success();
    }
};

// ============================================================================
// PageOverrunCompareTest
// ============================================================================

/**
 * Tests: memcmp, strcmp, strncmp
 * - Both buffers end at their respective trap pages
 * - Equal data forces full scan to the boundary
 */
template<typename Tag>
class PageOverrunCompareTest : public PageOverrunTestBase<Tag> {
protected:
    TestResult runPageOverrunTest(size_t size) override {
        auto buf1 = this->setupAtGuard(size);
        if (!buf1.valid) return this->error("buf1 allocation failed");
        this->fillForTest(buf1.ptr, size);

        auto buf2 = this->setupAtGuard(size);
        if (!buf2.valid) return this->error("buf2 allocation failed");
        std::memcpy(buf2.ptr, buf1.ptr, size);

        int ret = invoke_helper::compare<Tag>(buf1.ptr, buf2.ptr, size);
        if (ret != 0)
            return this->error("equal compare returned %d, expected 0", ret);

        size_t last = FunctionTest<Tag>::Traits::is_string_func ? size - 2 : size - 1;
        if (last < size) {
            uint8_t orig = buf2.ptr[last];
            buf2.ptr[last] = (orig > 140) ? (orig - 20) : (orig + 20);

            ret = invoke_helper::compare<Tag>(buf1.ptr, buf2.ptr, size);
            int expected_sign = (buf1.ptr[last] > buf2.ptr[last]) ? 1 : -1;
            int ret_sign = (ret > 0) ? 1 : ((ret < 0) ? -1 : 0);

            if (ret_sign != expected_sign)
                return this->error("mismatch compare: expected sign=%d got sign=%d",
                                   expected_sign, ret_sign);

            buf2.ptr[last] = orig;
        }

        return TestResult::success();
    }
};

// ============================================================================
// PageOverrunMemchrTest
// ============================================================================

/**
 * Tests: memchr
 * - Buffer ends at trap page, search char not present forces full scan
 */
template<typename Tag>
class PageOverrunMemchrTest : public PageOverrunTestBase<Tag> {
protected:
    TestResult runPageOverrunTest(size_t size) override {
        auto buf = this->setupAtGuard(size);
        if (!buf.valid) return this->error("allocation failed");

        std::memset(buf.ptr, 0x42, size);

        const uint8_t not_found = 0xFF;
        void* ret = this->invoke(buf.ptr, static_cast<int>(not_found), size);
        if (ret != nullptr)
            return this->error("expected NULL for not-found, got %p", ret);

        if (size > 0) {
            buf.ptr[size - 1] = 0xAA;
            ret = this->invoke(buf.ptr, 0xAA, size);
            if (ret != buf.ptr + size - 1)
                return this->error("search at end failed: expected=%p got=%p",
                                   static_cast<void*>(buf.ptr + size - 1), ret);
        }

        return TestResult::success();
    }
};

// ============================================================================
// PageOverrunStrchrTest
// ============================================================================

/**
 * Tests: strchr
 * - String ends at trap page with '\0' as last byte, search char not present
 */
template<typename Tag>
class PageOverrunStrchrTest : public PageOverrunTestBase<Tag> {
protected:
    TestResult runPageOverrunTest(size_t size) override {
        auto buf = this->setupAtGuard(size);
        if (!buf.valid) return this->error("allocation failed");

        const char search_char = '!';
        for (size_t i = 0; i < size - 1; ++i)
            buf.ptr[i] = 'a' + (i % 26);
        buf.ptr[size - 1] = '\0';

        char* ret = FunctionTest<Tag>::Traits::invoke(
            reinterpret_cast<char*>(buf.ptr), static_cast<int>(search_char));
        if (ret != nullptr)
            return this->error("expected NULL for not-found");

        if (size > 1) {
            buf.ptr[size - 2] = search_char;
            ret = FunctionTest<Tag>::Traits::invoke(
                reinterpret_cast<char*>(buf.ptr), static_cast<int>(search_char));
            char* expected = reinterpret_cast<char*>(buf.ptr + size - 2);
            if (ret != expected)
                return this->error("search at end failed: expected=%p got=%p",
                                   static_cast<void*>(expected), static_cast<void*>(ret));
        }

        return TestResult::success();
    }
};

// ============================================================================
// PageOverrunStrstrTest
// ============================================================================

/**
 * Tests: strstr
 * - Haystack ends at trap page, needle not present forces full scan
 */
template<typename Tag>
class PageOverrunStrstrTest : public PageOverrunTestBase<Tag> {
protected:
    TestResult runPageOverrunTest(size_t size) override {
        if (size < 4) return TestResult::success();

        auto buf = this->setupAtGuard(size);
        if (!buf.valid) return this->error("allocation failed");

        for (size_t i = 0; i < size - 1; ++i)
            buf.ptr[i] = 'a' + (i % 13);
        buf.ptr[size - 1] = '\0';

        const char* needle = "xyz";
        char* ret = FunctionTest<Tag>::Traits::invoke(
            reinterpret_cast<char*>(buf.ptr), needle);
        if (ret != nullptr)
            return this->error("expected NULL for not-found needle");

        size_t needle_len = 3;
        if (size > needle_len + 1) {
            size_t pos = size - 1 - needle_len;
            buf.ptr[pos] = 'x';
            buf.ptr[pos + 1] = 'y';
            buf.ptr[pos + 2] = 'z';

            ret = FunctionTest<Tag>::Traits::invoke(
                reinterpret_cast<char*>(buf.ptr), needle);
            char* expected = reinterpret_cast<char*>(buf.ptr + pos);
            if (ret != expected)
                return this->error("needle near end: expected=%p got=%p",
                                   static_cast<void*>(expected), static_cast<void*>(ret));
        }

        return TestResult::success();
    }
};

// ============================================================================
// PageOverrunLengthTest
// ============================================================================

/**
 * Tests: strlen, strspn
 * - String's '\0' is the last byte before trap page
 */
template<typename Tag>
class PageOverrunLengthTest : public PageOverrunTestBase<Tag> {
protected:
    TestResult runPageOverrunTest(size_t size) override {
        auto buf = this->setupAtGuard(size);
        if (!buf.valid) return this->error("allocation failed");

        this->fillForTest(buf.ptr, size);

        size_t expected = size - 1;
        size_t ret = invokeLength(buf.ptr, size);

        if (ret != expected)
            return this->error("length mismatch: expected=%zu got=%zu", expected, ret);

        return TestResult::success();
    }

private:
    template<typename T = Tag, typename std::enable_if<
        traits::FunctionTraits<T>::arg_count == 1, int>::type = 0>
    size_t invokeLength(uint8_t* str, size_t /*size*/) {
        return FunctionTest<Tag>::Traits::invoke(
            reinterpret_cast<const char*>(str));
    }

    template<typename T = Tag, typename std::enable_if<
        traits::FunctionTraits<T>::arg_count == 2, int>::type = 0>
    size_t invokeLength(uint8_t* str, size_t size) {
        const char* accept = "abcdefghijklmnopqrstuvwxyz";
        for (size_t i = 0; i < size - 1; ++i)
            str[i] = 'a' + (i % 26);
        str[size - 1] = '\0';
        return FunctionTest<Tag>::Traits::invoke(
            reinterpret_cast<const char*>(str), accept);
    }
};

// ============================================================================
// PageOverrunBoundedLengthTest
// ============================================================================

/**
 * Tests: strnlen
 * - String's '\0' is the last byte before the trap page
 * - Tests both clamped and unclamped cases at the guard boundary
 */
template<typename Tag>
class PageOverrunBoundedLengthTest : public PageOverrunTestBase<Tag> {
protected:
    TestResult runPageOverrunTest(size_t size) override {
        auto buf = this->setupAtGuard(size);
        if (!buf.valid) return this->error("allocation failed");

        this->fillNonNull(buf.ptr, size);
        buf.ptr[size - 1] = '\0';

        size_t expected = size - 1;
        size_t ret = FunctionTest<Tag>::Traits::invoke(
            reinterpret_cast<const char*>(buf.ptr), size);

        if (ret != expected)
            return this->error("unclamped length mismatch: expected=%zu got=%zu", expected, ret);


        if (size > 1) {
            size_t clamped_max = size / 2;
            ret = FunctionTest<Tag>::Traits::invoke(
                reinterpret_cast<const char*>(buf.ptr), clamped_max);

            if (ret != clamped_max)
                return this->error("clamped length mismatch: expected=%zu got=%zu", clamped_max, ret);
        }

        auto buf_nn = this->setupAtGuard(size);
        if (!buf_nn.valid) return this->error("no-null allocation failed");

        this->fillNonNull(buf_nn.ptr, size);

        ret = FunctionTest<Tag>::Traits::invoke(
            reinterpret_cast<const char*>(buf_nn.ptr), size);

        if (ret != size)
            return this->error("no-null bounded mismatch: expected=%zu got=%zu", size, ret);

        return TestResult::success();
    }
};

// ============================================================================
// PageOverrunConcatTest
// ============================================================================

/**
 * Tests: strcat, strncat
 * - Source string ends at trap page
 */
template<typename Tag>
class PageOverrunConcatTest : public PageOverrunTestBase<Tag> {
protected:
    TestResult runPageOverrunTest(size_t size) override {
        if (size < 4) return TestResult::success();

        size_t src_len = size / 2;

        auto src_buf = this->setupAtGuard(src_len + 1);
        if (!src_buf.valid) return this->error("src allocation failed");

        for (size_t i = 0; i < src_len; ++i)
            src_buf.ptr[i] = 'a' + (i % 26);
        src_buf.ptr[src_len] = '\0';

        size_t dst_size = src_len + 2;
        auto dst_buf = this->setupAtGuard(dst_size);
        if (!dst_buf.valid) return this->error("dst allocation failed");
        std::memset(dst_buf.ptr, 0, dst_size);
        uint8_t* dst = dst_buf.ptr;
        dst[0] = 'X';
        dst[1] = '\0';

        char* ret = invoke_helper::concat<Tag>(dst, src_buf.ptr, src_len);

        if (dst[0] != 'X')
            return this->error("prefix modified");

        for (size_t i = 0; i < src_len; ++i) {
            if (dst[1 + i] != src_buf.ptr[i])
                return this->errorAt(i, "concat mismatch");
        }

        if (ret != reinterpret_cast<char*>(dst))
            return this->error("return value mismatch");

        if (FunctionTest<Tag>::Traits::is_bounded &&
            FunctionTest<Tag>::Traits::is_string_func && size >= 4) {
            size_t cat_len = size / 2;

            auto src_nn = this->setupAtGuard(cat_len);
            if (!src_nn.valid) return this->error("src no-null concat allocation failed");
            this->fillNonNull(src_nn.ptr, cat_len);

            size_t dst_nn_size = cat_len + 2;
            auto dst_nn_buf = this->setupAtGuard(dst_nn_size);
            if (!dst_nn_buf.valid) return this->error("dst no-null concat allocation failed");
            std::memset(dst_nn_buf.ptr, 0, dst_nn_size);
            dst_nn_buf.ptr[0] = 'X';
            dst_nn_buf.ptr[1] = '\0';

            invoke_helper::concat<Tag>(dst_nn_buf.ptr, src_nn.ptr, cat_len);

            if (dst_nn_buf.ptr[0] != 'X')
                return this->error("no-null concat prefix modified");
            for (size_t i = 0; i < cat_len; ++i) {
                if (dst_nn_buf.ptr[1 + i] != src_nn.ptr[i])
                    return this->errorAt(i, "no-null concat mismatch");
            }
        }

        return TestResult::success();
    }
};

} // namespace validator
} // namespace libmem

#endif // LIBMEM_VALIDATOR_PAGE_CROSS_TESTS_HPP
