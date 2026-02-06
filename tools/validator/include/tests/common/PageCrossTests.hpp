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
 * @brief Comprehensive page boundary crossing tests
 *
 * All tests inherit from PageCrossTestBase and provide:
 * - Multi-page spanning buffers
 * - Full byte range (0-255) to catch signed comparison bugs
 * - Multiple alignment combinations
 * - Positions at/near page boundaries
 */

#include "core/PageCrossTestBase.hpp"
#include <cstring>

namespace libmem {
namespace validator {

// ============================================================================
// Page-Cross Copy Test
// ============================================================================

/**
 * PageCrossCopyTest - Comprehensive copy function test with multi-page spanning
 *
 * Tests: memcpy, memmove, mempcpy
 * - Source and destination span multiple pages
 * - Full byte range data
 * - Various alignment combinations
 */
template<typename Tag>
class PageCrossCopyTest : public PageCrossTestBase<Tag> {
protected:
    bool usePositions() const override { return false; }

    TestResult runPageCrossTest(size_t size, size_t /*pos*/,
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

        if (src + size > src_buf.base() + src_buf.usableSize() ||
            dst + size > dst_buf.base() + dst_buf.usableSize()) {
            return TestResult::success();  // Skip this combination
        }

        if (FunctionTest<Tag>::Traits::is_string_func) {
            src[size - 1] = '\0';
        }

        this->invoke(dst, src, size);

        for (size_t i = 0; i < size; ++i) {
            if (dst[i] != src[i]) {
                return this->errorAt(i, "copy mismatch at offset %zu", i);
            }
        }

        return TestResult::success();
    }
};

// ============================================================================
// Page-Cross Compare Test
// ============================================================================

/**
 * PageCrossCompareTest - Comprehensive compare function test
 *
 * Tests: strcmp, strncmp, memcmp
 * - Full byte range (catches signed comparison bug)
 * - Mismatch at various positions including page boundaries
 * - Different alignments trigger misaligned code path
 */
template<typename Tag>
class PageCrossCompareTest : public PageCrossTestBase<Tag> {
protected:
    bool requiresDifferentOffsets() const override { return true; }

    TestResult runPageCrossTest(size_t size, size_t mismatch_pos,
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
            return TestResult::success();  // Skip this combination
        }

        this->copyData(str2, str1, size);

        // Create mismatch
        uint8_t val1 = str1[mismatch_pos];
        uint8_t val2 = (val1 > 140) ? (val1 - 20) : (val1 + 20);
        str2[mismatch_pos] = val2;

        int expected = static_cast<int>(val1) - static_cast<int>(val2);
        int expected_sign = (expected > 0) ? 1 : ((expected < 0) ? -1 : 0);

        if (FunctionTest<Tag>::Traits::is_string_func) {
            str1[size - 1] = '\0';
            str2[size - 1] = '\0';
        }

        int ret = invokeCompare(str1, str2, size);
        int ret_sign = (ret > 0) ? 1 : ((ret < 0) ? -1 : 0);

        if (ret_sign != expected_sign) {
            return this->error(
                "compare mismatch: size=%zu pos=%zu off1=%u off2=%u "
                "val1=0x%02x val2=0x%02x expected=%d got=%d",
                size, mismatch_pos, off1, off2, val1, val2, expected_sign, ret_sign);
        }

        return TestResult::success();
    }

private:
    template<typename T = Tag>
    typename std::enable_if<traits::FunctionTraits<T>::has_size_param &&
                           !traits::FunctionTraits<T>::is_string_func, int>::type
    invokeCompare(uint8_t* s1, uint8_t* s2, size_t n) {
        return FunctionTest<Tag>::Traits::invoke(s1, s2, n);
    }

    template<typename T = Tag>
    typename std::enable_if<traits::FunctionTraits<T>::has_size_param &&
                           traits::FunctionTraits<T>::is_string_func, int>::type
    invokeCompare(uint8_t* s1, uint8_t* s2, size_t n) {
        return FunctionTest<Tag>::Traits::invoke(
            reinterpret_cast<const char*>(s1),
            reinterpret_cast<const char*>(s2), n);
    }

    template<typename T = Tag>
    typename std::enable_if<!traits::FunctionTraits<T>::has_size_param &&
                           traits::FunctionTraits<T>::is_string_func, int>::type
    invokeCompare(uint8_t* s1, uint8_t* s2, size_t /*n*/) {
        return FunctionTest<Tag>::Traits::invoke(
            reinterpret_cast<const char*>(s1),
            reinterpret_cast<const char*>(s2));
    }
};

// ============================================================================
// Page-Cross Fill Test
// ============================================================================

/**
 * PageCrossFillTest - Comprehensive fill function test
 *
 * Tests: memset
 * - Destination spans multiple pages
 * - Various fill values including high-bytes
 */
template<typename Tag>
class PageCrossFillTest : public PageCrossTestBase<Tag> {
protected:
    bool usePositions() const override { return false; }

    TestResult runPageCrossTest(size_t size, size_t /*pos*/,
                            uint32_t off1, uint32_t /*off2*/) override {
        auto setup = this->setupBuffer(size, off1);
        if (setup.should_skip) return TestResult::success();
        if (!setup.valid) return this->error("allocation failed");

        uint8_t* dst = setup.ptr;

        // Test with various values including high-bytes
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
// Page-Cross Search Test
// ============================================================================

/**
 * PageCrossSearchTest - Comprehensive search function test
 *
 * Tests: memchr
 * - Buffer spans multiple pages
 * - Search character at various positions including page boundaries
 */
template<typename Tag>
class PageCrossSearchTest : public PageCrossTestBase<Tag> {
protected:
    TestResult runPageCrossTest(size_t size, size_t search_pos,
                            uint32_t off1, uint32_t /*off2*/) override {
        auto setup = this->setupBuffer(size, off1);
        if (setup.should_skip) return TestResult::success();
        if (!setup.valid) return this->error("allocation failed");

        this->seedRandom(size, search_pos, off1, 0);
        this->fillFullByteRange(setup.buffer.base(), setup.buffer.usableSize());

        uint8_t* str = setup.ptr;

        // Place unique search character at position
        const uint8_t search_char = 0xFF;  // Unlikely to appear naturally
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
// Page-Cross Length Test
// ============================================================================

/**
 * PageCrossLengthTest - Comprehensive length function test
 *
 * Tests: strlen
 * - String spans multiple pages
 * - Null terminator at various positions including page boundaries
 */
template<typename Tag>
class PageCrossLengthTest : public PageCrossTestBase<Tag> {
protected:
    TestResult runPageCrossTest(size_t size, size_t null_pos,
                            uint32_t off1, uint32_t /*off2*/) override {
        PageCrossBuffer buf;

        if (!buf.allocate(size + 1)) {
            return this->error("allocation failed");
        }

        this->seedRandom(size, null_pos, off1, 0);
        this->fillNonNull(buf.base(), buf.usableSize());

        uint8_t* str = buf.getAddressNearBoundary(off1);

        if (!str || str + null_pos + 1 > buf.base() + buf.usableSize()) {
            return TestResult::success();  // Skip
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
// Page-Cross String Copy Test
// ============================================================================

/**
 * PageCrossStringCopyTest - Comprehensive string copy test
 *
 * Tests: strcpy, strncpy
 * - Source spans multiple pages
 * - Full byte range data
 */
template<typename Tag>
class PageCrossStringCopyTest : public PageCrossTestBase<Tag> {
protected:
    bool usePositions() const override { return false; }

    TestResult runPageCrossTest(size_t size, size_t /*pos*/,
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

        if (src + size > src_buf.base() + src_buf.usableSize() ||
            dst + size > dst_buf.base() + dst_buf.usableSize()) {
            return TestResult::success();  // Skip
        }

        // Ensure no nulls except at end
        for (size_t i = 0; i < size - 1; ++i) {
            if (src[i] == 0) src[i] = 1;
        }
        src[size - 1] = '\0';

        char* ret = invokeCopy(
            reinterpret_cast<char*>(dst),
            reinterpret_cast<const char*>(src),
            size);

        for (size_t i = 0; i < size; ++i) {
            if (dst[i] != src[i]) {
                return this->errorAt(i, "string copy mismatch");
            }
        }

        if (ret != reinterpret_cast<char*>(dst)) {
            return this->error("return value mismatch");
        }

        return TestResult::success();
    }

private:
    template<typename T = Tag>
    typename std::enable_if<traits::FunctionTraits<T>::has_size_param, char*>::type
    invokeCopy(char* dst, const char* src, size_t n) {
        return FunctionTest<Tag>::Traits::invoke(dst, src, n);
    }

    template<typename T = Tag>
    typename std::enable_if<!traits::FunctionTraits<T>::has_size_param, char*>::type
    invokeCopy(char* dst, const char* src, size_t /*n*/) {
        return FunctionTest<Tag>::Traits::invoke(dst, src);
    }
};

// ============================================================================
// Page-Cross Concat Test
// ============================================================================

/**
 * PageCrossConcatTest - Comprehensive concat test
 *
 * Tests: strcat, strncat
 * - Source spans multiple pages
 */
template<typename Tag>
class PageCrossConcatTest : public PageCrossTestBase<Tag> {
protected:
    size_t minTestSize() const override { return 16; }
    bool usePositions() const override { return false; }

    TestResult runPageCrossTest(size_t size, size_t /*pos*/,
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
            return TestResult::success();  // Skip
        }

        // Prepare source (no internal nulls)
        for (size_t i = 0; i < src_len; ++i) {
            if (src[i] == 0) src[i] = 1;
        }
        src[src_len] = '\0';

        // Prepare destination with prefix
        dst[0] = 'X';
        dst[1] = '\0';

        char* ret = invokeConcat(
            reinterpret_cast<char*>(dst),
            reinterpret_cast<const char*>(src),
            src_len);

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

private:
    template<typename T = Tag>
    typename std::enable_if<traits::FunctionTraits<T>::has_size_param, char*>::type
    invokeConcat(char* dst, const char* src, size_t n) {
        return FunctionTest<Tag>::Traits::invoke(dst, src, n);
    }

    template<typename T = Tag>
    typename std::enable_if<!traits::FunctionTraits<T>::has_size_param, char*>::type
    invokeConcat(char* dst, const char* src, size_t /*n*/) {
        return FunctionTest<Tag>::Traits::invoke(dst, src);
    }
};

// ============================================================================
// Page-Cross Strchr Test
// ============================================================================

/**
 * PageCrossStrchrTest - Comprehensive strchr test
 *
 * Tests: strchr, strrchr
 * - String spans multiple pages
 * - Search character at various positions
 */
template<typename Tag>
class PageCrossStrchrTest : public PageCrossTestBase<Tag> {
protected:
    TestResult runPageCrossTest(size_t size, size_t search_pos,
                            uint32_t off1, uint32_t /*off2*/) override {
        PageCrossBuffer buf;

        if (!buf.allocate(size + 1)) {
            return this->error("allocation failed");
        }

        this->seedRandom(size, search_pos, off1, 0);
        this->fillFullByteRange(buf.base(), buf.usableSize());

        uint8_t* str = buf.getAddressNearBoundary(off1);

        if (!str || str + size + 1 > buf.base() + buf.usableSize()) {
            return TestResult::success();  // Skip
        }

        // Ensure no nulls except at end, and no search char except at position
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
// Page-Cross Strstr Test
// ============================================================================

/**
 * PageCrossStrstrTest - Comprehensive strstr test
 *
 * Tests: strstr
 * - Haystack spans multiple pages
 * - Needle at various positions
 */
template<typename Tag>
class PageCrossStrstrTest : public PageCrossTestBase<Tag> {
protected:
    size_t minTestSize() const override { return 16; }

    TestResult runPageCrossTest(size_t size, size_t needle_pos,
                            uint32_t off1, uint32_t /*off2*/) override {
        PageCrossBuffer buf;

        if (!buf.allocate(size + 1)) {
            return this->error("allocation failed");
        }

        this->seedRandom(size, needle_pos, off1, 0);

        uint8_t* haystack = buf.getAddressNearBoundary(off1);

        if (!haystack || haystack + size + 1 > buf.base() + buf.usableSize()) {
            return TestResult::success();  // Skip
        }

        // Fill with characters that won't match needle
        for (size_t i = 0; i < size; ++i) {
            haystack[i] = 'a' + (i % 13);  // Only a-m
        }
        haystack[size] = '\0';

        // Place needle at position
        const char* needle = "xyz";
        size_t needle_len = 3;

        if (needle_pos + needle_len >= size) {
            return TestResult::success();  // Skip invalid position
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
// Page-Cross Strspn Test
// ============================================================================

/**
 * PageCrossStrspnTest - Comprehensive strspn test
 *
 * Tests: strspn, strcspn
 * - String spans multiple pages
 */
template<typename Tag>
class PageCrossStrspnTest : public PageCrossTestBase<Tag> {
protected:
    TestResult runPageCrossTest(size_t size, size_t span_len,
                            uint32_t off1, uint32_t /*off2*/) override {
        PageCrossBuffer buf;

        if (!buf.allocate(size + 1)) {
            return this->error("allocation failed");
        }

        uint8_t* str = buf.getAddressNearBoundary(off1);

        if (!str || str + size + 1 > buf.base() + buf.usableSize()) {
            return TestResult::success();  // Skip
        }

        // Fill with accepted characters up to span_len
        const char* accept = "abcdefghij";
        for (size_t i = 0; i < span_len && i < size; ++i) {
            str[i] = 'a' + (i % 10);  // Only a-j (in accept set)
        }

        // Fill rest with rejected character
        for (size_t i = span_len; i < size; ++i) {
            str[i] = 'z';  // Not in accept set
        }
        str[size] = '\0';

        size_t expected = (span_len < size) ? span_len : size;
        size_t ret = FunctionTest<Tag>::Traits::invoke(
            reinterpret_cast<const char*>(str), accept);

        if (ret != expected) {
            return this->error("strspn mismatch: expected=%zu got=%zu", expected, ret);
        }

        return TestResult::success();
    }
};

} // namespace validator
} // namespace libmem

#endif // LIBMEM_VALIDATOR_PAGE_CROSS_TESTS_HPP
