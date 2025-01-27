/* Copyright (C) 2023-25 Advanced Micro Devices, Inc. All rights reserved.
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
#include <stddef.h>
#include "logger.h"
#include "threshold.h"
#include <stdint.h>
#include <immintrin.h>
#include "alm_defs.h"
#include "zen_cpu_info.h"

/* This function is an optimized version of strncmp using AVX-512 instructions.
It compares atmost `size` bytes of str1 and str2, and returns an integer
indicating the result of the comparison.*/

static inline int __attribute__((flatten)) _strncmp_avx512(const char *str1, const char *str2, size_t size)
{
    // If the size is zero, the strings are considered equal
    if (!size)
        return 0;

    size_t offset = 0;
    __m512i z0, z1, z2, z3, z4, z5, z6, z7, z8;
    uint64_t  cmp_idx, match, match1, match2;
    uint64_t mask1, mask2;
    __mmask64 mask;

    // Initialize a zeroed AVX-512 register for comparisons against null terminators
    z0 = _mm512_setzero_epi32 ();

    // Handle cases where size to compare is less than or equal to 64B
    if (size <= ZMM_SZ)
    {
        z7 = _mm512_set1_epi8(0xff);
        // Create a mask based on the size of the string to compare
        mask = ((uint64_t)-1) >> (ZMM_SZ - size);
        z1 =  _mm512_mask_loadu_epi8(z7, mask, str1);
        z2 =  _mm512_mask_loadu_epi8(z7, mask, str2);

        // Compare the two vectors for inequality
        match = _mm512_cmpneq_epu8_mask(z1,z2);

        // If there is a difference, find the first differing byte and return the difference
        if (match)
        {
            cmp_idx = _tzcnt_u64(match | _mm512_cmpeq_epu8_mask(z1, z0));
            return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
        }
        return 0;
    }

    // Handle cases where size to compare lies between 65B-128B
    if (size <= 2 * ZMM_SZ)
    {
        // Load the first 64B of the strings
        z1 = _mm512_loadu_si512(str1);
        z2 = _mm512_loadu_si512(str2);

        match = _mm512_cmpeq_epu8_mask(z1, z0) | _mm512_cmpneq_epu8_mask(z1,z2);
        // If the first 64B is equal, compare the tail part
        if (!match)
        {
            // Adjust the offset for next 64B
            offset = size - ZMM_SZ;
            z3 = _mm512_loadu_si512(str1 + offset);
            z4 = _mm512_loadu_si512(str2 + offset);
            //Check for NULL
            match = _mm512_cmpeq_epu8_mask(z3, z0) | _mm512_cmpneq_epu8_mask(z3, z4);
            if(!match)
                return 0;
        }
        cmp_idx = _tzcnt_u64(match) + offset;
        return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
    }

    // Handle cases where size to compare lies between 129B-256B
    if (size <= 4 * ZMM_SZ)
    {
        z1 = _mm512_loadu_si512(str1);
        z2 = _mm512_loadu_si512(str1 + ZMM_SZ);

        z5 = _mm512_loadu_si512(str2);
        z6 = _mm512_loadu_si512(str2 + ZMM_SZ);

        mask1 = _mm512_cmpneq_epu8_mask(z1,z5);

        match1 = _mm512_cmpeq_epu8_mask(_mm512_min_epu8(z1,z2), z0) | mask1 | _mm512_cmpneq_epu8_mask(z2,z6);
        if (match1)
        {
            match = _mm512_cmpeq_epu8_mask(z1,z0) | mask1;
            if (!match)
            {
                offset += ZMM_SZ;
                match = match1;
            }
            cmp_idx = _tzcnt_u64(match) + offset;
            return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
        }
        z3 = _mm512_loadu_si512(str1 + size - 2 * ZMM_SZ);
        z4 = _mm512_loadu_si512(str1 + size - ZMM_SZ);
        z7 = _mm512_loadu_si512(str2 + size - 2 * ZMM_SZ);
        z8 = _mm512_loadu_si512(str2 + size - ZMM_SZ);

        mask2 = _mm512_cmpneq_epu8_mask(z3,z7);
        match2 = _mm512_cmpeq_epu8_mask(_mm512_min_epu8(z3,z4), z0) | mask2 | _mm512_cmpneq_epu8_mask(z4,z8);
        if (match2)
        {
            offset = - 2 * ZMM_SZ;
            match = _mm512_cmpeq_epu8_mask(z3,z0) | mask2;
            if (!match)
            {
                offset += ZMM_SZ;
                match = match2;
            }
            cmp_idx = _tzcnt_u64(match) + size + offset;
            return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
        }
        return 0;
    }

    // Handle cases where size to compare is greater than 256B
    while ((size - offset) >= 4 * ZMM_SZ)
    {
        z1 = _mm512_loadu_si512(str1 + offset);
        z2 = _mm512_loadu_si512(str1 + offset + 1 * ZMM_SZ);

        z5 = _mm512_loadu_si512(str2 + offset);
        z6 = _mm512_loadu_si512(str2 + offset + 1 * ZMM_SZ);

        mask1 = _mm512_cmpneq_epu8_mask(z1,z5);
        match1 = _mm512_cmpeq_epu8_mask(_mm512_min_epu8(z1,z2), z0) | mask1 | _mm512_cmpneq_epu8_mask(z2,z6);

        if (match1)
        {
            match = _mm512_cmpeq_epu8_mask(z1,z0) | mask1;
            if (!match)
            {
                offset += ZMM_SZ;
                match = match1;
            }
            cmp_idx = _tzcnt_u64(match) + offset;
            return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
        }

        z3 = _mm512_loadu_si512(str1 + offset + 2 * ZMM_SZ);
        z4 = _mm512_loadu_si512(str1 + offset + 3 * ZMM_SZ);
        z7 = _mm512_loadu_si512(str2 + offset + 2 * ZMM_SZ);
        z8 = _mm512_loadu_si512(str2 + offset + 3 * ZMM_SZ);
        mask2 = _mm512_cmpneq_epu8_mask(z3,z7);
        match2 = _mm512_cmpeq_epu8_mask(_mm512_min_epu8(z3,z4), z0) | mask2 | _mm512_cmpneq_epu8_mask(z4,z8);

        if (match2)
        {
            offset +=  2 * ZMM_SZ;
            match = _mm512_cmpeq_epu8_mask(z3,z0) | mask2;
            if (!match)
            {
                offset += ZMM_SZ;
                match = match2;
            }
            cmp_idx = _tzcnt_u64(match) + offset;
            return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
        }
        offset += 4 * ZMM_SZ;
    }
    uint8_t left_out = size - offset;
    if (!left_out)
        return 0;

    switch(left_out >> 6)
    {
        case 3:
            offset = size - 4 * ZMM_SZ;
            z1 = _mm512_loadu_si512(str1 + offset);
            z2 = _mm512_loadu_si512(str2 + offset);
            match = _mm512_cmpeq_epu8_mask(z1, z0) | _mm512_cmpneq_epu8_mask(z1,z2);
            if (match)
                break;
        case 2:
            offset = size - 3 * ZMM_SZ;
            z3 = _mm512_loadu_si512(str1 + offset);
            z4 = _mm512_loadu_si512(str2 + offset);
            match = _mm512_cmpeq_epu8_mask(z3, z0) | _mm512_cmpneq_epu8_mask(z3, z4);
            if(match)
                break;
        case 1:
            offset = size - 2 * ZMM_SZ;
            z1 = _mm512_loadu_si512(str1 + offset);
            z2 = _mm512_loadu_si512(str2 + offset);
            match = _mm512_cmpeq_epu8_mask(z1, z0) | _mm512_cmpneq_epu8_mask(z1,z2);
            if(match)
                break;
        case 0:
            offset = size - ZMM_SZ;
            z3 = _mm512_loadu_si512(str1 + offset);
            z4 = _mm512_loadu_si512(str2 + offset);
            match = _mm512_cmpeq_epu8_mask(z3, z0) | _mm512_cmpneq_epu8_mask(z3, z4);

            if(!match)
                return 0;
    }
    cmp_idx = _tzcnt_u64(match) + offset;
    return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
}
