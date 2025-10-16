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

#include "strlen_avx2.c"
#include "strchr_avx2.c"

static inline int  cmp_needle_ble_ymm(const char *str1, const char *str2, uint8_t size)
{
    __m128i x0, x1, x2, x3, x4, x_cmp, x_null;
    uint16_t ret = 0;
    uint8_t cmp_idx = 0;

    if (size == 1)
    {
        return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
    }
    x0 = _mm_setzero_si128();
    if (size <= 2 * WORD_SZ)
    {
        x1 = _mm_loadu_si16((void *)str1);
        x2 = _mm_loadu_si16((void *)str2);
        x_cmp = _mm_cmpeq_epi8(x1, x2);
        x_null = _mm_cmpgt_epi8(x1, x0);
        ret = _mm_movemask_epi8(_mm_and_si128(x_cmp, x_null)) + 1;
        if (!(ret & 0x3))
        {
            cmp_idx = size - WORD_SZ;
            x3 = _mm_loadu_si16((void *)str1 + cmp_idx);
            x4 = _mm_loadu_si16((void *)str2 + cmp_idx);
            x_cmp = _mm_cmpeq_epi8(x3, x4);
            x_null = _mm_cmpgt_epi8(x3, x0);
            ret = _mm_movemask_epi8(_mm_and_si128(x_cmp, x_null)) + 1;
        }
        return (ret & 0x3);
    }

    if (size <= 2 * DWORD_SZ)
    {
        x1 = _mm_loadu_si32((void *)str1);
        x2 = _mm_loadu_si32((void *)str2);
        x_cmp = _mm_cmpeq_epi8(x1, x2);
        x_null = _mm_cmpgt_epi8(x1, x0);
        ret = _mm_movemask_epi8(_mm_and_si128(x_cmp, x_null)) + 1;
        if (!(ret & 0xf))
        {
            cmp_idx = size - DWORD_SZ;
            x3 = _mm_loadu_si32((void *)str1 + cmp_idx);
            x4 = _mm_loadu_si32((void *)str2 + cmp_idx);
            x_cmp = _mm_cmpeq_epi8(x3, x4);
            x_null = _mm_cmpgt_epi8(x3, x0);
            ret = _mm_movemask_epi8(_mm_and_si128(x_cmp, x_null)) + 1;
        }
        return (ret & 0xf);
    }
    if (size <= 2 * QWORD_SZ)
    {
        x1 = _mm_loadu_si64((void *)str1);
        x2 = _mm_loadu_si64((void *)str2);
        x_cmp = _mm_cmpeq_epi8(x1, x2);
        x_null = _mm_cmpgt_epi8(x1, x0);
        ret = _mm_movemask_epi8(_mm_and_si128(x_cmp, x_null)) + 1;
        if (!(ret & 0xff))
        {
            cmp_idx = size - QWORD_SZ;
            x3 = _mm_loadu_si64((void *)str1 + cmp_idx);
            x4 = _mm_loadu_si64((void *)str2 + cmp_idx);
            x_cmp = _mm_cmpeq_epi8(x3, x4);
            x_null = _mm_cmpgt_epi8(x3, x0);
            ret = _mm_movemask_epi8(_mm_and_si128(x_cmp, x_null)) + 1;
        }
        return (ret & 0xff);
    }
    if (size <= 2 * XMM_SZ)
    {
        x1 = _mm_loadu_si128((void *)str1);
        x2 = _mm_loadu_si128((void *)str2);
        x_cmp = _mm_cmpeq_epi8(x1, x2);
        x_null = _mm_cmpgt_epi8(x1, x0);
        ret = _mm_movemask_epi8(_mm_and_si128(x_cmp, x_null)) + 1;
        if (!ret)
        {
            cmp_idx = size - XMM_SZ;
            x3 = _mm_loadu_si128((void *)str1 + cmp_idx);
            x4 = _mm_loadu_si128((void *)str2 + cmp_idx);
            x_cmp = _mm_cmpeq_epi8(x3, x4);
            x_null = _mm_cmpgt_epi8(x3, x0);
            ret = _mm_movemask_epi8(_mm_and_si128(x_cmp, x_null)) + 1;
        }
        return ret;
    }
    return 0;
}


/* This function compares two strings using AVX2 vector instructions.
   It returns 0 if the strings are equal up to the specified size, or -1 if they differ */
static inline int cmp_needle_page_safe_avx2(const char *str1, const char *str2, size_t size) {
     size_t offset = 0;
    __m256i y1, y2, y3, y4, y5, y6, y7, y8, y_cmp, y_cmp1, y_cmp2;
    int32_t  ret = 0, ret1 = 0, ret2 =0;

    if (size <= YMM_SZ)
    {
        return cmp_needle_ble_ymm(str1, str2, size);
    }

    if (size <= 2 * YMM_SZ)
    {
        y1 = _mm256_loadu_si256((void *)str1);
        y2 = _mm256_loadu_si256((void *)str2);
        y_cmp = _mm256_cmpeq_epi8(y1, y2);
        ret = _mm256_movemask_epi8((y_cmp)) + 1;
        if (!ret)
        {
            offset = size - YMM_SZ;
            y3 =  _mm256_loadu_si256((void *)str1 + offset);
            y4 =  _mm256_loadu_si256((void *)str2 + offset);

            y_cmp = _mm256_cmpeq_epi8(y3, y4);
            ret = _mm256_movemask_epi8(y_cmp) + 1;
        }
        return ret;
    }
    if ( size <= 4* YMM_SZ)
    {
        y1 = _mm256_loadu_si256((void*)str1);
        y2 = _mm256_loadu_si256((void*)str1 + YMM_SZ);
        y3 = _mm256_loadu_si256((void*)str1 + size - 2 * YMM_SZ);
        y4 = _mm256_loadu_si256((void*)str1 + size - YMM_SZ);

        y5 = _mm256_loadu_si256((void*)str2);
        y6 = _mm256_loadu_si256((void*)str2 + YMM_SZ);
        y7 = _mm256_loadu_si256((void*)str2 + size - 2 * YMM_SZ);
        y8 = _mm256_loadu_si256((void*)str2 + size - YMM_SZ);

        y_cmp1 = _mm256_cmpeq_epi8(y1, y5);
        y_cmp2 = _mm256_cmpeq_epi8(y2, y6);
        ret1 = _mm256_movemask_epi8(_mm256_and_si256(y_cmp1, y_cmp2)) + 1;

        if(ret1)
            return -1;

        y_cmp1 = _mm256_cmpeq_epi8(y3, y7);
        y_cmp2 = _mm256_cmpeq_epi8(y4, y8);
        ret2 = _mm256_movemask_epi8(_mm256_and_si256(y_cmp1, y_cmp2)) + 1;

        if(ret2)
            return -1;

        return 0;
    }

    while((size - offset) >= 4 * YMM_SZ)
    {
        y1 = _mm256_loadu_si256((void*)str1 + offset);
        y2 = _mm256_loadu_si256((void*)str1 + offset + 1 * YMM_SZ);

        y5 = _mm256_loadu_si256((void*)str2 + offset);
        y6 = _mm256_loadu_si256((void*)str2 + offset + 1 * YMM_SZ);

        y_cmp1 = _mm256_cmpeq_epi8(y1, y5);
        y_cmp2 = _mm256_cmpeq_epi8(y2, y6);

        ret1 = _mm256_movemask_epi8(_mm256_and_si256(y_cmp1, y_cmp2)) + 1;

        if(ret1)
            return -1;

        y3 = _mm256_loadu_si256((void*)str1 + offset + 2 * YMM_SZ);
        y4 = _mm256_loadu_si256((void*)str1 + offset + 3 * YMM_SZ);

        y7 = _mm256_loadu_si256((void*)str2 + offset + 2 * YMM_SZ);
        y8 = _mm256_loadu_si256((void*)str2 + offset + 3 * YMM_SZ);

        y_cmp1 = _mm256_cmpeq_epi8(y3, y7);
        y_cmp2 = _mm256_cmpeq_epi8(y4, y8);
        ret2 = _mm256_movemask_epi8(_mm256_and_si256(y_cmp1, y_cmp2)) + 1;

        if(ret2)
            return -1;

        offset += 4 * YMM_SZ;
    }
    uint8_t left_out = size - offset;
    if (!left_out)
        return 0;

    switch(left_out >> 5)
    {
        case 3:
            offset = size - 4 * YMM_SZ;
            y1 = _mm256_loadu_si256((void *)str1 + offset);
            y2 = _mm256_loadu_si256((void *)str2 + offset);
            y_cmp = _mm256_cmpeq_epi8(y1, y2);
            ret = _mm256_movemask_epi8(y_cmp) + 1;

            if (ret)
                break;
            __attribute__ ((fallthrough));
        case 2:
            offset = size - 3 * YMM_SZ;
            y3 = _mm256_loadu_si256((void *)str1 + offset);
            y4 = _mm256_loadu_si256((void *)str2 + offset);
            y_cmp = _mm256_cmpeq_epi8(y3, y4);
            ret = _mm256_movemask_epi8(y_cmp) + 1;

            if(ret)
                break;
            __attribute__ ((fallthrough));
        case 1:
            offset = size - 2 * YMM_SZ;
            y1 = _mm256_loadu_si256((void *)str1 + offset);
            y2 = _mm256_loadu_si256((void *)str2 + offset);
            y_cmp = _mm256_cmpeq_epi8(y1, y2);
            ret = _mm256_movemask_epi8(y_cmp) + 1;

            if(ret)
                break;
            __attribute__ ((fallthrough));
        case 0:
            offset = size - YMM_SZ;
            y3 = _mm256_loadu_si256((void *)str1 + offset);
            y4 = _mm256_loadu_si256((void *)str2 + offset);
            y_cmp = _mm256_cmpeq_epi8(y3, y4);
            ret = _mm256_movemask_epi8(y_cmp) + 1;

            if(!ret)
                return 0;
    }
    return -1;
}

static inline int cmp_needle_page_cross_avx2(const char *str1, const char *str2, size_t size, size_t safe_bytes) {
    size_t offset = 0;
    if (safe_bytes > 0) {
        for (size_t i = 0; i < safe_bytes; i++) {
            if (str1[i] != str2[i]) return -1;
        }
        offset = safe_bytes;
    }
    size_t tail = size - offset;
    if (tail > 0) {
        int cmp = cmp_needle_page_safe_avx2(str1 + offset, str2 + offset, tail);
        if (cmp) return -1;
    }
    return 0;
}

static inline int cmp_needle_avx2(const char *haystack, const char *needle, size_t hay_idx, size_t needle_len) {
    size_t bytes_to_page_end = PAGE_SZ - ((uintptr_t)(haystack + hay_idx) & (PAGE_SZ - 1));
    if (bytes_to_page_end < needle_len) {
        return cmp_needle_page_cross_avx2(haystack + hay_idx, needle, needle_len, bytes_to_page_end) == 0;
    }
    return cmp_needle_page_safe_avx2(haystack + hay_idx, needle, needle_len) == 0;
}

/* Page check for initial block load and null check */
static inline void load_and_check_first_block_avx2(const char* haystack, 
                                                   __m256i* y1, uint32_t* null_mask, uint32_t* null_idx) {
    // Calculate the offset based on the alignment of the haystack pointer
    size_t offset = (uintptr_t)haystack & (YMM_SZ - 1);
    int check_unaligned = 1;
    
    // Check for potential read beyond the page boundary
    if (unlikely((PAGE_SZ - YMM_SZ) < ((PAGE_SZ - 1) & (uintptr_t)haystack))) {
        // Use aligned load from previous address to prevent reading beyond page boundary
        *y1 = _mm256_loadu_si256((const __m256i*)(haystack - offset));
        // Shift masks by offset to get correct positions for null detection
        *null_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(_mm256_setzero_si256(), *y1)) >> offset;
        
        // Set flag to skip unaligned check
        check_unaligned = 0;
    }
    
    // Unaligned check is needed, if no page cross
    if (check_unaligned) {
        *y1 = _mm256_loadu_si256((const __m256i*)haystack);
        *null_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(*y1, _mm256_setzero_si256()));
    }
    
    *null_idx = *null_mask ? _tzcnt_u32(*null_mask) : YMM_SZ;
}

/* Full search processing */
static inline char* process_full_search_avx2(const char* haystack, const char* needle, 
                                            size_t needle_len, size_t base_offset, 
                                            uint32_t match_mask, uint32_t null_mask) {
    // Null boundary check
    match_mask &= null_mask ? (null_mask ^ (null_mask - 1)) : UINT32_MAX;
    
    while (match_mask) {
        uint32_t match_idx = _tzcnt_u32(match_mask);
        if (cmp_needle_avx2(haystack, needle, match_idx + base_offset, needle_len))
            return (char*)(haystack + match_idx + base_offset);
        match_mask = _blsr_u32(match_mask);
    }
    return NULL;
}

/* Filtering function for AVX2 */
static inline void apply_filter_avx2(const char* haystack, size_t offset, size_t needle_len,
                                     __m256i needle_second, __m256i needle_last, uint32_t* match_mask) {
    if (unlikely(!*match_mask)) return;
    
    uintptr_t haystack_ptr = (uintptr_t)(haystack + offset);
    size_t bytes_to_page_end = PAGE_SZ - (haystack_ptr & (PAGE_SZ - 1));
    
    // Only apply second character filter if completely safe
    if (bytes_to_page_end > (1 + YMM_SZ)) {
        __m256i y_second = _mm256_loadu_si256((const __m256i*)(haystack + offset + 1));
        *match_mask &= _mm256_movemask_epi8(_mm256_cmpeq_epi8(needle_second, y_second));
    }
    
    // For last character filter, be very conservative for large needles
    if (needle_len <= 512 && bytes_to_page_end > (needle_len + YMM_SZ)) {
        // Only use last character filter for small needles with plenty of page space
        uintptr_t last_ptr = haystack_ptr + needle_len - 1;
        __m256i y_last = _mm256_loadu_si256((const __m256i*)last_ptr);
        *match_mask &= _mm256_movemask_epi8(_mm256_cmpeq_epi8(needle_last, y_last));
    }
}

/* Broadcasting approach for smaller needles (≤ 2KB) */
static inline char * _strstr_avx2_broadcasting(const char* haystack, const char* needle, size_t needle_len) {
    __m256i needle_first = _mm256_set1_epi8(needle[0]);
    __m256i needle_second = _mm256_set1_epi8(needle[1]);
    __m256i needle_last = _mm256_set1_epi8(needle[needle_len - 1]);
    
    size_t offset = (uintptr_t)haystack & (YMM_SZ - 1);
    int check_unaligned = 1;
    
    __m256i y1;
    uint32_t null_mask;
    uint32_t null_idx;
    uint32_t match_mask = 0;
    
    // Check for potential read beyond the page boundary 
    if (unlikely((PAGE_SZ - YMM_SZ) < ((PAGE_SZ - 1) & (uintptr_t)haystack))) {
        // Use aligned load from previous address to prevent reading beyond page boundary
        y1 = _mm256_loadu_si256((const __m256i*)(haystack - offset));
        // Create masks for null characters and matches
        __m256i null_cmp = _mm256_cmpeq_epi8(_mm256_setzero_si256(), y1);
        __m256i match_cmp = _mm256_cmpeq_epi8(needle_first, y1);
        
        // Extract masks and shift by offset to get correct positions
        null_mask = _mm256_movemask_epi8(null_cmp) >> offset;
        match_mask = _mm256_movemask_epi8(match_cmp) >> offset;
        
        // Set flag to skip unaligned check
        check_unaligned = 0;
    }
    
    // Unaligned check is needed, if no page cross
    if (check_unaligned) {
        y1 = _mm256_loadu_si256((const __m256i*)haystack);
        null_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y1, _mm256_setzero_si256()));
        match_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(needle_first, y1));
    }
    
    null_idx = null_mask ? _tzcnt_u32(null_mask) : YMM_SZ;
    
    // Early termination check 
    if (null_mask && (null_idx < needle_len))
        return NULL;
    
    // Apply filtering
    if (match_mask && (!null_mask || null_idx > 1)) {
        apply_filter_avx2(haystack, 0, needle_len, needle_second, needle_last, &match_mask);
    }
    
    if (match_mask) {
        char* result = process_full_search_avx2(haystack, needle, needle_len, 0, match_mask, null_mask);
        if (result) return result;
    }
    
    if (null_mask) return NULL;
    
    // Adjust offset for aligned loads
    offset = YMM_SZ - offset;
    
    // Main search loop
    while (1) {
        y1 = _mm256_loadu_si256((const __m256i*)(haystack + offset));
        match_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y1, needle_first));
        null_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y1, _mm256_setzero_si256()));
        
        // Apply filtering
        if (likely(match_mask & ~null_mask)) {
            apply_filter_avx2(haystack, offset, needle_len, needle_second, needle_last, &match_mask);
        }
        if (match_mask) {
            char* result = process_full_search_avx2(haystack, needle, needle_len, offset, match_mask, null_mask);
            if (result) return result;
        }
        
        // Termination check
        if (null_mask) return NULL;
        
        offset += YMM_SZ;
    }
}

/* This function is an optimized version of strstr using AVX2 instructions.
   It finds the first occurrence of the substring `needle` in the string `haystack`. */
static inline char * __attribute__((flatten)) _strstr_avx2(const char* haystack, const char* needle) {
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
        return (char*)_strchr_avx2(haystack, needle[0]);

    // Get needle length to decide which approach to use
    needle_len = _strlen_avx2(needle);

    // For smaller needles, use simple broadcasting approach
    return _strstr_avx2_broadcasting(haystack, needle, needle_len);
}

