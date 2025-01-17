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

/*This function finds the transition index in a string */
static inline size_t transition_index (const char *needle)
{
  size_t idx = 0;
  // Loop through the string to find where consecutive characters are not the same
  while (needle[idx + NULL_BYTE] != STR_TERM_CHAR &&
        needle[idx] == needle[idx + 1]) {
        idx ++;
  }
  // check if the character at the next position from transition index is the string terminator
  return (needle[idx + 1] == STR_TERM_CHAR) ? 0 : idx;
}

/* This function compares two strings using AVX-512 vector instructions.
   It returns 0 if the strings are equal up to the specified size, or -1 if they differ */
static inline int cmp_needle_avx512(const char *str1, const char *str2, size_t size)
{
    size_t offset = 0;
    __m512i z1, z2, z3, z4, z5, z6, z7, z8;
    uint64_t  ret, ret1, ret2;

    // Handle the case where the size is less than or equal 64B
    if (size <= ZMM_SZ)
    {
        z7 = _mm512_set1_epi8(0xff);
        __mmask64 mask = ((uint64_t)-1) >> (ZMM_SZ - size);
        z1 =  _mm512_mask_loadu_epi8(z7 ,mask, str1);
        z2 =  _mm512_mask_loadu_epi8(z7 ,mask, str2);
        ret = _mm512_cmpneq_epu8_mask(z1,z2);
        if (ret)
            return -1;
        return 0;
    }

    // Handle the case where the size lies between 65B-128B
    if (size <= 2 * ZMM_SZ)
    {
        z1 = _mm512_loadu_si512(str1);
        z2 = _mm512_loadu_si512(str2);

        ret =  _mm512_cmpneq_epu8_mask(z1,z2);
        if (!ret)
        {
            offset = size - ZMM_SZ;
            z3 = _mm512_loadu_si512(str1 + offset);
            z4 = _mm512_loadu_si512(str2 + offset);
            //Check for NULL
            ret = _mm512_cmpneq_epu8_mask(z3, z4);
            if(!ret)
                return 0;
        }
        return -1;
    }

    // Handle the case where the size lies between 129B-2565B
    if (size <= 4* ZMM_SZ)
    {
        z1 = _mm512_loadu_si512(str1);
        z2 = _mm512_loadu_si512(str1 + ZMM_SZ);

        z5 = _mm512_loadu_si512(str2);
        z6 = _mm512_loadu_si512(str2 + ZMM_SZ);

        ret1 = _mm512_cmpneq_epu8_mask(z1,z5) | _mm512_cmpneq_epu8_mask(z2,z6);
        if (ret1)
            return -1;

        z3 = _mm512_loadu_si512(str1 + size - 2 * ZMM_SZ);
        z4 = _mm512_loadu_si512(str1 + size - ZMM_SZ);
        z7 = _mm512_loadu_si512(str2 + size - 2 * ZMM_SZ);
        z8 = _mm512_loadu_si512(str2 + size - ZMM_SZ);

        ret2 =  _mm512_cmpneq_epu8_mask(z3,z7) | _mm512_cmpneq_epu8_mask(z4,z8);
        if (ret2)
            return -1;

        return 0;
    }

    // For sizes larger than 256B, process in chunks of 4 ZMM registers
    while ((size - offset) >= 4 * ZMM_SZ)
    {
        z1 = _mm512_loadu_si512(str1 + offset);
        z2 = _mm512_loadu_si512(str1 + offset + 1 * ZMM_SZ);

        z5 = _mm512_loadu_si512(str2 + offset);
        z6 = _mm512_loadu_si512(str2 + offset + 1 * ZMM_SZ);

        ret1 = _mm512_cmpneq_epu8_mask(z1,z5) | _mm512_cmpneq_epu8_mask(z2,z6);

        if (ret1)
            return -1;

        z3 = _mm512_loadu_si512(str1 + offset + 2 * ZMM_SZ);
        z4 = _mm512_loadu_si512(str1 + offset + 3 * ZMM_SZ);
        z7 = _mm512_loadu_si512(str2 + offset + 2 * ZMM_SZ);
        z8 = _mm512_loadu_si512(str2 + offset + 3 * ZMM_SZ);

        ret2 = _mm512_cmpneq_epu8_mask(z3,z7)| _mm512_cmpneq_epu8_mask(z4,z8);

        if (ret2)
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
            ret = _mm512_cmpneq_epu8_mask(z1,z2);
            if (ret)
                break;
            __attribute__ ((fallthrough));
        case 2:
            offset = size - 3 * ZMM_SZ;
            z3 = _mm512_loadu_si512(str1 + offset);
            z4 = _mm512_loadu_si512(str2 + offset);
            ret = _mm512_cmpneq_epu8_mask(z3, z4);
            if(ret)
                break;
            __attribute__ ((fallthrough));
        case 1:
            offset = size - 2 * ZMM_SZ;
            z1 = _mm512_loadu_si512(str1 + offset);
            z2 = _mm512_loadu_si512(str2 + offset);
            ret = _mm512_cmpneq_epu8_mask(z1,z2);
            if(ret)
                break;
            __attribute__ ((fallthrough));
        case 0:
            offset = size - ZMM_SZ;
            z3 = _mm512_loadu_si512(str1 + offset);
            z4 = _mm512_loadu_si512(str2 + offset);
            ret = _mm512_cmpneq_epu8_mask(z3, z4);

            if(!ret)
                return 0;
    }
    return -1;
}

/* This function is an optimized version of strstr using AVX-512 instructions.
It finds the first occurrence of the substring `needle` in the string `haystack`q. */
static inline char * __attribute__((flatten)) _strstr_avx512(const char* haystack, const char* needle)
{
    size_t needle_len = 0;
    __m512i needle_char_t0, needle_char_t1;
    __m512i z0, z1, z2, z7;
    __mmask64 match_mask, null_mask, null_pfx_mask;
    uint64_t  match_idx, null_idx = 0;
    size_t offset;
    size_t transit_idx;
    uint8_t check_unaligned = 1;

    // If the first character of the needle is the string terminator,
    // return the haystack (empty needle case)
    if (needle[0] == STR_TERM_CHAR)
        return (char*)haystack;

    // If the first character of the haystack is the string terminator,
    // return NULL (empty haystack case)
    if (haystack[0] == STR_TERM_CHAR)
        return NULL;

    // If the second character of the needle is the string terminator,
    // it is similar to finding a char in a string
    if (needle[1] == STR_TERM_CHAR)
        return (char*)_strchr_avx512(haystack, needle[0]);

    // Calculate the transition index for the needle
    // The transition index is the position where consecutive characters stop being identical
    transit_idx = transition_index(needle);

    // Set up vectors with repeated characters from the needle at the transition index
    needle_char_t0 = _mm512_set1_epi8(needle[transit_idx]);
    needle_char_t1 = _mm512_set1_epi8(needle[transit_idx + 1]);

    // Initialize a zeroed AVX-512 register for comparisons against null terminators
    z0 = _mm512_setzero_si512 ();

    // Calculate the offset based on the alignment of the haystack pointer
    offset = (uintptr_t)haystack & (ZMM_SZ - 1);

    // Check for potential read beyond the page boundary
    if (unlikely ((PAGE_SZ - ZMM_SZ) < ((PAGE_SZ -1) & (uintptr_t)haystack)))
    {
        z7 = _mm512_set1_epi8(0xff);
        __mmask64 haystack_load_mask = ALL_BITS_SET >> offset;
        // Load the haystack with masking
        z1 = _mm512_mask_loadu_epi8(z7 , haystack_load_mask, haystack);
        // Create a mask for null characters
        null_mask = (uint64_t)_mm512_cmpeq_epi8_mask(z1, z0);
        // Find the first null character if null_mask is non-zero
        if (null_mask)
        {
            null_idx = _tzcnt_u64(null_mask);
            // If null character is before the transition index, return NULL
            if (null_idx <= transit_idx)
                return NULL;
            // Create a mask for matching characters
            match_mask = (uint64_t)_mm512_cmpeq_epi8_mask(needle_char_t0, z1);
            // Set flag to skip unaligned check
            check_unaligned = 0;
        }
    }
    // Unaligned check is needed, if no page cross
    if (check_unaligned)
    {
        z1 = _mm512_loadu_si512((void*)haystack);
        null_mask = _mm512_cmpeq_epi8_mask(z1, z0);
        if (null_mask)
        {
            null_idx = _tzcnt_u64(null_mask);

            if (null_idx <= transit_idx)
                return NULL;
        }
        match_mask = _mm512_cmpeq_epi8_mask(needle_char_t0, z1);
    }

    // If there is a match
    if (match_mask)
    {
        // Get the length of the needle
        needle_len = _strlen_avx512(needle);

        if (null_mask)
        {
            match_idx = _tzcnt_u64(match_mask);

            // If the match is beyond the null character, return NULL
            if (match_idx > null_idx)
                return NULL;
            // Update the match mask to exclude matches beyond the null character
            match_mask &= (null_mask ^ (null_mask - 1));
        }
        //Loop until match_mask is clear
        do{
            match_idx = _tzcnt_u64(match_mask);
            // Check if the following character in the haystack matches
            // the corresponding character in the needle
            if ((*(haystack + match_idx + transit_idx + 1) == *(needle + transit_idx + 1)))
            {
                // Compare the rest of the needle with the haystack
                if (!(cmp_needle_avx512(((char*)(haystack + match_idx)), needle, needle_len)))
                    return (char*)(haystack + match_idx);
            }
            //clears the LOWEST SET BIT
            match_mask = _blsr_u64(match_mask);

        }while (match_mask);

    }

    // If there is a null character in the checked part, return NULL
    if (null_mask)
        return NULL;

    // Adjust the offset to align further loads
    offset = ZMM_SZ - offset;
    //Aligned case:
    if (!needle_len)
        needle_len = _strlen_avx512(needle);

    // Loop until the end of the haystack or a null character is found
    while(!null_mask)
    {
        z1 = _mm512_loadu_si512((void*)haystack + offset - 1);
        z2 = _mm512_load_si512((void*)haystack + offset);
        // Create masks for matching the first and second characters of the needle
        match_mask = _mm512_cmpeq_epi8_mask(z1, needle_char_t0) & _mm512_cmpeq_epi8_mask(z2, needle_char_t1);
        null_mask = (uint64_t)_mm512_cmpeq_epi8_mask(z2, z0);
        null_pfx_mask = (null_mask ^ (null_mask - 1));
        // Update the match mask to exclude matches beyond the null character
        match_mask = match_mask & null_pfx_mask;
        //Occurence of NEEDLE's first char
        while (match_mask)
        {
            match_idx = _tzcnt_u64(match_mask);
            if ((match_idx + offset) >= (transit_idx + 1)) {
                match_idx += offset - transit_idx - 1;
                // If the needle matches, return the start of the match
                if (!(cmp_needle_avx512(((char*)(haystack + match_idx)), needle, needle_len)))
                    return (char*)(haystack + match_idx);
            }
            //clears the LOWEST SET BIT
            match_mask = _blsr_u64(match_mask);
        }
        // Move to the next part of the haystack
        offset += ZMM_SZ;
    }
    // If no match is found, return NULL
    return NULL;
}
