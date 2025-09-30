/* Copyright (C) 2024-25 Advanced Micro Devices, Inc. All rights reserved.
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

#include "strlen_avx512.c"
#include "strchr_avx512.c"

/* This function compares two strings using AVX-512 vector instructions.
   It returns 0 if the strings are equal up to the specified size, or -1 if they differ */
static inline int cmp_needle_page_safe(const char *str1, const char *str2, size_t size)
{
    size_t offset = 0;
    __m512i z1, z2, z3, z4, z5, z6, z7, z8;
    
    if (likely(size <= ZMM_SZ)) {
        __mmask64 mask = _bzhi_u64(UINT64_MAX, size);
        z1 = _mm512_maskz_loadu_epi8(mask, str1);
        z2 = _mm512_maskz_loadu_epi8(mask, str2);
        return !!(_mm512_cmpneq_epu8_mask(z1, z2));
    }
    if (likely(size <= 2 * ZMM_SZ))
    {

        z1 = _mm512_loadu_si512(str1);
        z2 = _mm512_loadu_si512(str2);
        if (_mm512_cmpneq_epu8_mask(z1, z2))
            return -1;
        
        offset = size - ZMM_SZ;
        z3 = _mm512_loadu_si512(str1 + offset);
        z4 = _mm512_loadu_si512(str2 + offset);
        return !!(_mm512_cmpneq_epu8_mask(z3, z4));
    }

    // Handle the case where the size lies between 129B-2565B
    if (likely(size <= 4 * ZMM_SZ)) 
    {
        z1 = _mm512_loadu_si512(str1);
        z5 = _mm512_loadu_si512(str2);
        z2 = _mm512_loadu_si512(str1 + ZMM_SZ);
        z6 = _mm512_loadu_si512(str2 + ZMM_SZ);
        if (_mm512_cmpneq_epu8_mask(z1, z5) | _mm512_cmpneq_epu8_mask(z2, z6))
            return -1;

        z3 = _mm512_loadu_si512(str1 + size - 2 * ZMM_SZ);
        z7 = _mm512_loadu_si512(str2 + size - 2 * ZMM_SZ);
        z4 = _mm512_loadu_si512(str1 + size - ZMM_SZ);
        z8 = _mm512_loadu_si512(str2 + size - ZMM_SZ);
        
        return !!(_mm512_cmpneq_epu8_mask(z3, z7) | _mm512_cmpneq_epu8_mask(z4, z8));
    }

    // For sizes larger than 256B, process in chunks of 4 ZMM registers
    while ((size - offset) >= 4 * ZMM_SZ)
    {
        z1 = _mm512_loadu_si512(str1 + offset);
        z5 = _mm512_loadu_si512(str2 + offset);
        z2 = _mm512_loadu_si512(str1 + offset + 1 * ZMM_SZ);
        z6 = _mm512_loadu_si512(str2 + offset + 1 * ZMM_SZ);

        if( _mm512_cmpneq_epu8_mask(z1, z5) | _mm512_cmpneq_epu8_mask(z2, z6))
            return -1;

        z3 = _mm512_loadu_si512(str1 + offset + 2 * ZMM_SZ);
        z7 = _mm512_loadu_si512(str2 + offset + 2 * ZMM_SZ);

        z4 = _mm512_loadu_si512(str1 + offset + 3 * ZMM_SZ);
        z8 = _mm512_loadu_si512(str2 + offset + 3 * ZMM_SZ);

        if( _mm512_cmpneq_epu8_mask(z3, z7) | _mm512_cmpneq_epu8_mask(z4, z8))
            return -1;

        offset += 4 * ZMM_SZ;
    }
    // Handle any remaining bytes that were not compared in the above loop
    uint8_t left_out = size - offset;
    if (!left_out)
        return 0;

    switch(left_out >> 6)
    {
        case 3:
            offset = size - 4 * ZMM_SZ;
            z1 = _mm512_loadu_si512(str1 + offset);
            z2 = _mm512_loadu_si512(str2 + offset);
            if (_mm512_cmpneq_epu8_mask(z1, z2))
                break;
            __attribute__ ((fallthrough));
        case 2:
            offset = size - 3 * ZMM_SZ;
            z3 = _mm512_loadu_si512(str1 + offset);
            z4 = _mm512_loadu_si512(str2 + offset);
            if (_mm512_cmpneq_epu8_mask(z3, z4))
                break;
            __attribute__ ((fallthrough));
        case 1:
            offset = size - 2 * ZMM_SZ;
            z1 = _mm512_loadu_si512(str1 + offset);
            z2 = _mm512_loadu_si512(str2 + offset);
            if (_mm512_cmpneq_epu8_mask(z1, z2))
                break;
            __attribute__ ((fallthrough));
        case 0:
            offset = size - ZMM_SZ;
            z3 = _mm512_loadu_si512(str1 + offset);
            z4 = _mm512_loadu_si512(str2 + offset);
            return !!(_mm512_cmpneq_epu8_mask(z3, z4));
    }
    return -1;  
}

// Handles page boundary, uses head/tail logic
static inline int cmp_needle_page_cross(const char *str1, const char *str2, size_t size, size_t safe_bytes) {
    size_t offset = 0;
    // Compare initial page-crossing region with masked loads
    if (safe_bytes > 0) {
        __mmask64 mask = _bzhi_u64(UINT64_MAX, safe_bytes);
        __m512i z1 = _mm512_maskz_loadu_epi8(mask, str1);
        __m512i z2 = _mm512_maskz_loadu_epi8(mask, str2);
        if (_mm512_cmpneq_epu8_mask(z1, z2)) return -1;
        offset = safe_bytes;
    }
    // Use full needle compare for the remainder
    size_t tail = size - offset;
    if (tail > 0) {
        int cmp = cmp_needle_page_safe(str1 + offset, str2 + offset, tail);
        if (cmp) return -1;
    }
    return 0;
}

static inline int cmp_needle_avx512(const char *haystack, const char *needle, 
                                            size_t hay_idx, size_t needle_len)
{
    size_t bytes_to_page_end = PAGE_SZ - ((uintptr_t)(haystack + hay_idx) & (PAGE_SZ - 1));
    if (bytes_to_page_end < needle_len)
        return cmp_needle_page_cross(haystack + hay_idx, needle, needle_len, bytes_to_page_end) == 0;
    // Page safe compare for remaining
    return cmp_needle_page_safe(haystack + hay_idx, needle, needle_len) == 0;
}

/* Page check for initial block load and null check */
static inline void load_and_check_first_block(const char* haystack, size_t offset, 
                                             __m512i* z1, __mmask64* null_mask, uint64_t* null_idx)
{
    // Branch-free page boundary check and load
    int needs_masked_load = (PAGE_SZ - ZMM_SZ) < ((PAGE_SZ - 1) & (uintptr_t)haystack);
    
    if (unlikely(needs_masked_load)) {
        __m512i z7 = _mm512_set1_epi8(0xff);
        __mmask64 haystack_load_mask = UINT64_MAX >> offset;
        *z1 = _mm512_mask_loadu_epi8(z7, haystack_load_mask, haystack);
    } else {
        *z1 = _mm512_loadu_si512((void*)haystack);
    }
    
    // Check for null terminator
    __m512i z0 = _mm512_setzero_si512();
    *null_mask = _mm512_cmpeq_epi8_mask(*z1, z0);
    *null_idx = *null_mask ? _tzcnt_u64(*null_mask) : ZMM_SZ;
}

/* Full search */
static inline char* process_full_search(const char* haystack, const char* needle, 
                                               size_t needle_len, size_t base_offset,
                                               __mmask64 match_mask, __mmask64 null_mask)
{
    // Null boundary check
    match_mask &= null_mask ? (null_mask ^ (null_mask - 1)) : UINT64_MAX;
    
    while (match_mask) {
        uint64_t match_idx = _tzcnt_u64(match_mask);
        if (cmp_needle_avx512(haystack, needle, match_idx + base_offset, needle_len))
            return (char*)(haystack + match_idx + base_offset);
        match_mask = _blsr_u64(match_mask);
    }
    return NULL;
}

/* Filtering function */
static inline void apply_filter(const char *haystack, size_t offset, size_t needle_len,
                                __m512i needle_second, __m512i needle_last, __mmask64 *match_mask)
{
    if (unlikely(!*match_mask)) return;
    
    uintptr_t haystack_ptr = (uintptr_t)(haystack + offset);
    size_t bytes_to_page_end = PAGE_SZ - (haystack_ptr & (PAGE_SZ - 1));
    
    // Only apply second character filter if completely safe
    if (bytes_to_page_end > (1 + ZMM_SZ)) {
        __m512i z_second = _mm512_loadu_si512((const void *)(haystack + offset + 1));
        *match_mask &= _mm512_cmpeq_epi8_mask(needle_second, z_second);
    }
    
    // For last character filter, be very conservative for large needles
    if (needle_len <= 512 &&  bytes_to_page_end > (needle_len + ZMM_SZ)) {
        // Only use last character filter for small needles with plenty of page space
        uintptr_t last_ptr = haystack_ptr + needle_len - 1;
        __m512i z_last = _mm512_loadu_si512((const void *)last_ptr);
        *match_mask &= _mm512_cmpeq_epi8_mask(needle_last, z_last);
    }
}

/* Broadcasting approach for smaller needles (≤ 2KB) */
static inline char * _strstr_avx512_broadcasting(const char* haystack, const char* needle, size_t needle_len)
{
    __m512i needle_first = _mm512_set1_epi8(needle[0]);
    __m512i needle_second = _mm512_set1_epi8(needle[1]);  // Back to second char for safety
    __m512i needle_last = _mm512_set1_epi8(needle[needle_len - 1]);  // Hoist last char broadcast
    
    size_t offset = (uintptr_t)haystack & (ZMM_SZ - 1);
    
    __m512i z1;
    __mmask64 null_mask;
    uint64_t null_idx;
    
    // First block processing with page check
    load_and_check_first_block(haystack, offset, &z1, &null_mask, &null_idx);
    
    // Early termination check 
    if (null_mask && (null_idx < needle_len))
        return NULL;
    
    // Process first block matches
    __mmask64 match_mask = _mm512_cmpeq_epi8_mask(needle_first, z1);
    
    // Apply filtering for first block
    if (likely( match_mask & ~null_mask)) {
        apply_filter(haystack, 0, needle_len,needle_second, needle_last, &match_mask);
    }
    
    if (match_mask) {
        char* result = process_full_search(haystack, needle, needle_len, 0, match_mask, null_mask);
        if (result) return result;
    }
    
    if (null_mask) return NULL;
    
    // Adjust offset for aligned loads
    offset = ZMM_SZ - offset;
    
    // Main search loop
    while (1) {
        z1 = _mm512_loadu_si512((void*)(haystack + offset));
        match_mask = _mm512_cmpeq_epi8_mask(z1, needle_first);
        null_mask = _mm512_cmpeq_epi8_mask(z1, _mm512_setzero_si512());
        
        // Apply filtering
        if (likely(match_mask & ~null_mask)) {
            apply_filter(haystack, offset, needle_len,needle_second, needle_last, &match_mask);
        }
        if (match_mask) {
            char* result = process_full_search(haystack, needle, needle_len, offset, match_mask, null_mask);
            if (result) return result;
        }
        
        // Termination check
        if (null_mask) return NULL;
        
        offset += ZMM_SZ;
    }
}


/* This function is an optimized version of strstr using AVX-512 instructions.
It finds the first occurrence of the substring `needle` in the string `haystack`q. */
static inline char * __attribute__((flatten)) _strstr_avx512(const char* haystack, const char* needle)
{
    size_t needle_len = 0;

    // If the first character of the needle is the string terminator,
    // return the haystack (empty needle case)
    if (unlikely(needle[0] == STR_TERM_CHAR))
        return (char*)haystack;

    // If the first character of the haystack is the string terminator,
    // return NULL (empty haystack case)
    if (unlikely(haystack[0] == STR_TERM_CHAR))
        return NULL;

    // If the second character of the needle is the string terminator,
    // it is similar to finding a char in a string
    if (needle[1] == STR_TERM_CHAR)
        return (char*)_strchr_avx512(haystack, needle[0]);

    // Get needle length to decide which approach to use
    needle_len = _strlen_avx512(needle);

    // For smaller needles, use simple broadcasting approach
    return _strstr_avx512_broadcasting(haystack, needle, needle_len);
}
