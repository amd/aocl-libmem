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

#ifndef LIBMEM_VALIDATOR_STRING_TRAITS_HPP
#define LIBMEM_VALIDATOR_STRING_TRAITS_HPP

/**
 * @file StringTraits.hpp
 * @brief FunctionTraits specializations for string functions
 */

#include "traits/FunctionTraits.hpp"
#include <cstring>

namespace libmem {
namespace validator {
namespace traits {

// ============================================================================
// String Function Tag Types
// ============================================================================

struct StrcpyTag {};
struct StrncpyTag {};
struct StrcatTag {};
struct StrncatTag {};
struct StrcmpTag {};
struct StrncmpTag {};
struct StrlenTag {};
struct StrnlenTag {};
struct StrchrTag {};
struct StrstrTag {};
struct StrspnTag {};

// ============================================================================
// String Copy Function Traits
// ============================================================================

/**
 * strcpy traits
 * char* strcpy(char* dest, const char* src)
 */
template<>
struct FunctionTraits<StrcpyTag> : StringTraitsBase<FunctionTraits<StrcpyTag>> {
    using ReturnType = char*;
    using FunctionPtr = char*(*)(char*, const char*);

    static const char* name() { return "strcpy"; }

    static constexpr FunctionCategory category = FunctionCategory::COPY;
    static constexpr ReturnCategory return_category = ReturnCategory::DEST_PTR;
    static constexpr bool returns_dest = true;
    static constexpr size_t arg_count = 2;

    static char* invoke(char* dst, const char* src) {
        return std::strcpy(dst, src);
    }
};

/**
 * strncpy traits
 * char* strncpy(char* dest, const char* src, size_t n)
 */
template<>
struct FunctionTraits<StrncpyTag> : StringTraitsBase<FunctionTraits<StrncpyTag>> {
    using ReturnType = char*;
    using FunctionPtr = char*(*)(char*, const char*, size_t);

    static const char* name() { return "strncpy"; }

    static constexpr FunctionCategory category = FunctionCategory::COPY;
    static constexpr ReturnCategory return_category = ReturnCategory::DEST_PTR;
    static constexpr bool has_size_param = true;
    static constexpr bool is_bounded = true;
    static constexpr bool returns_dest = true;
    static constexpr size_t arg_count = 3;

    static char* invoke(char* dst, const char* src, size_t n) {
        return std::strncpy(dst, src, n);
    }
};

// ============================================================================
// String Concatenation Function Traits
// ============================================================================

/**
 * strcat traits
 * char* strcat(char* dest, const char* src)
 */
template<>
struct FunctionTraits<StrcatTag> : StringTraitsBase<FunctionTraits<StrcatTag>> {
    using ReturnType = char*;
    using FunctionPtr = char*(*)(char*, const char*);

    static const char* name() { return "strcat"; }

    static constexpr FunctionCategory category = FunctionCategory::CONCAT;
    static constexpr ReturnCategory return_category = ReturnCategory::DEST_PTR;
    static constexpr bool returns_dest = true;
    static constexpr size_t arg_count = 2;

    static char* invoke(char* dst, const char* src) {
        return std::strcat(dst, src);
    }
};

/**
 * strncat traits
 * char* strncat(char* dest, const char* src, size_t n)
 */
template<>
struct FunctionTraits<StrncatTag> : StringTraitsBase<FunctionTraits<StrncatTag>> {
    using ReturnType = char*;
    using FunctionPtr = char*(*)(char*, const char*, size_t);

    static const char* name() { return "strncat"; }

    static constexpr FunctionCategory category = FunctionCategory::CONCAT;
    static constexpr ReturnCategory return_category = ReturnCategory::DEST_PTR;
    static constexpr bool has_size_param = true;
    static constexpr bool is_bounded = true;
    static constexpr bool returns_dest = true;
    static constexpr size_t arg_count = 3;

    static char* invoke(char* dst, const char* src, size_t n) {
        return std::strncat(dst, src, n);
    }
};

// ============================================================================
// String Comparison Function Traits
// ============================================================================

/**
 * strcmp traits
 * int strcmp(const char* s1, const char* s2)
 */
template<>
struct FunctionTraits<StrcmpTag> : StringTraitsBase<FunctionTraits<StrcmpTag>> {
    using ReturnType = int;
    using FunctionPtr = int(*)(const char*, const char*);

    static const char* name() { return "strcmp"; }

    static constexpr FunctionCategory category = FunctionCategory::COMPARE;
    static constexpr ReturnCategory return_category = ReturnCategory::COMPARISON;
    static constexpr size_t arg_count = 2;

    static int invoke(const char* s1, const char* s2) {
        return std::strcmp(s1, s2);
    }
};

/**
 * strncmp traits
 * int strncmp(const char* s1, const char* s2, size_t n)
 */
template<>
struct FunctionTraits<StrncmpTag> : StringTraitsBase<FunctionTraits<StrncmpTag>> {
    using ReturnType = int;
    using FunctionPtr = int(*)(const char*, const char*, size_t);

    static const char* name() { return "strncmp"; }

    static constexpr FunctionCategory category = FunctionCategory::COMPARE;
    static constexpr ReturnCategory return_category = ReturnCategory::COMPARISON;
    static constexpr bool has_size_param = true;
    static constexpr bool is_bounded = true;
    static constexpr size_t arg_count = 3;

    static int invoke(const char* s1, const char* s2, size_t n) {
        return std::strncmp(s1, s2, n);
    }
};

// ============================================================================
// String Length Function Traits
// ============================================================================

/**
 * strlen traits
 * size_t strlen(const char* s)
 */
template<>
struct FunctionTraits<StrlenTag> : StringTraitsBase<FunctionTraits<StrlenTag>> {
    using ReturnType = size_t;
    using FunctionPtr = size_t(*)(const char*);

    static const char* name() { return "strlen"; }

    static constexpr FunctionCategory category = FunctionCategory::LENGTH;
    static constexpr ReturnCategory return_category = ReturnCategory::SIZE;
    static constexpr size_t arg_count = 1;

    static size_t invoke(const char* s) {
        return std::strlen(s);
    }
};

/**
 * strnlen traits
 * size_t strnlen(const char* s, size_t maxlen)
 */
template<>
struct FunctionTraits<StrnlenTag> : StringTraitsBase<FunctionTraits<StrnlenTag>> {
    using ReturnType = size_t;
    using FunctionPtr = size_t(*)(const char*, size_t);

    static const char* name() { return "strnlen"; }

    static constexpr FunctionCategory category = FunctionCategory::LENGTH;
    static constexpr ReturnCategory return_category = ReturnCategory::SIZE;
    static constexpr bool has_size_param = true;
    static constexpr bool is_bounded = true;
    static constexpr size_t arg_count = 2;

    static size_t invoke(const char* s, size_t maxlen) {
        return ::strnlen(s, maxlen);
    }
};

/**
 * strspn traits
 * size_t strspn(const char* s, const char* accept)
 */
template<>
struct FunctionTraits<StrspnTag> : StringTraitsBase<FunctionTraits<StrspnTag>> {
    using ReturnType = size_t;
    using FunctionPtr = size_t(*)(const char*, const char*);

    static const char* name() { return "strspn"; }

    static constexpr FunctionCategory category = FunctionCategory::LENGTH;
    static constexpr ReturnCategory return_category = ReturnCategory::SIZE;
    static constexpr size_t arg_count = 2;

    static size_t invoke(const char* s, const char* accept) {
        return std::strspn(s, accept);
    }
};

// ============================================================================
// String Search Function Traits
// ============================================================================

/**
 * strchr traits
 * char* strchr(const char* s, int c)
 */
template<>
struct FunctionTraits<StrchrTag> : StringTraitsBase<FunctionTraits<StrchrTag>> {
    using ReturnType = char*;
    using FunctionPtr = char*(*)(const char*, int);

    static const char* name() { return "strchr"; }

    static constexpr FunctionCategory category = FunctionCategory::SEARCH;
    static constexpr ReturnCategory return_category = ReturnCategory::FOUND_PTR;
    static constexpr size_t arg_count = 2;

    static char* invoke(char* s, int c) {
        return std::strchr(s, c);
    }
};

/**
 * strstr traits
 * char* strstr(const char* haystack, const char* needle)
 */
template<>
struct FunctionTraits<StrstrTag> : StringTraitsBase<FunctionTraits<StrstrTag>> {
    using ReturnType = char*;
    using FunctionPtr = char*(*)(const char*, const char*);

    static const char* name() { return "strstr"; }

    static constexpr FunctionCategory category = FunctionCategory::SEARCH;
    static constexpr ReturnCategory return_category = ReturnCategory::FOUND_PTR;
    static constexpr size_t arg_count = 2;

    static char* invoke(char* haystack, const char* needle) {
        return std::strstr(haystack, needle);
    }
};

} // namespace traits
} // namespace validator
} // namespace libmem

#endif // LIBMEM_VALIDATOR_STRING_TRAITS_HPP

