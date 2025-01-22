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
#include <stdint.h>
#include <immintrin.h>
#include "alm_defs.h"
#include "zen_cpu_info.h"

/* This function is an optimized version of strcmp using AVX-512 instructions.
It compares the two strings str1 and str2 and returns returns an integer
indicating the result of the comparison. */

static inline int __attribute__((flatten)) _strcmp_avx512(const char *str1, const char *str2)
{
    size_t offset1, offset2, offset, mix_offset;
    __m512i z0, z1, z2, z3, z4, z7;
    uint64_t  cmp_idx, ret;
    __mmask64 mask;

    // Initialize a zeroed AVX-512 register for comparisons against null terminators
    z0 = _mm512_setzero_epi32 ();

    // Handle cases where start of either of the string is close to the end of a memory page
    if (unlikely(((PAGE_SZ - ZMM_SZ) < ((PAGE_SZ -1) & ((uintptr_t)str1 | (uintptr_t)str2)))))
    {
        offset1 = (uintptr_t)str1 & (ZMM_SZ - 1); //str1 alignment
        offset2 = (uintptr_t)str2 & (ZMM_SZ - 1); //str2 alignment
        z7 = _mm512_set1_epi8(0xff);
        // Determine which offset is larger and use it for the mixed offset
        mix_offset = (offset1 >= offset2);
        offset = (mix_offset)*offset1 | (!mix_offset)*offset2;
         // Create a mask that will ignore the first 'offset' bytes when loading data
        mask = ((uint64_t)-1) >> offset;

        z1 =  _mm512_mask_loadu_epi8(z7 ,mask, str1);
        z2 =  _mm512_mask_loadu_epi8(z7 ,mask, str2);

        // Compare the vectors for equality and inequality
        ret = _mm512_cmpeq_epu8_mask(z1, z0) | _mm512_cmpneq_epu8_mask(z1,z2);

        // If there is a difference or a null terminator, find the index of the first diff/null
        if (ret)
        {
            cmp_idx = _tzcnt_u64(ret);
            return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
        }
    }
    else
    {
        // If the strings are not close to the end of a memory page, load the first 64 bytes
        z1 = _mm512_loadu_si512(str1);
        z2 = _mm512_loadu_si512(str2);
        ret = _mm512_cmpeq_epu8_mask(z1, z0) | _mm512_cmpneq_epu8_mask(z1,z2);
        if (ret)
        {
            cmp_idx = _tzcnt_u64(ret);
            return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
        }
    }

    offset1 = (uintptr_t)str1 & (ZMM_SZ - 1); //str1 alignment
    offset2 = (uintptr_t)str2 & (ZMM_SZ - 1); //str2 alignment

    //Both str2 and str1 are aligned
    if (offset1 == offset2)
    {
        // Adjust the offset to align with cache line for the next load operation
        offset = ZMM_SZ - offset1;
        while (1)
        {
            // Load 64 bytes from each string and compare till we find diff/null
            z1 = _mm512_load_si512(str1 + offset);
            z2 = _mm512_load_si512(str2 + offset);
            //Check for NULL
            ret = _mm512_cmpeq_epu8_mask(z1, z0) | _mm512_cmpneq_epu8_mask(z1,z2);
            if (!ret)
            {
                offset += ZMM_SZ;
                z3 = _mm512_load_si512(str1 + offset);
                z4 = _mm512_load_si512(str2 + offset);
                //Check for NULL
                ret = _mm512_cmpeq_epu8_mask(z3, z0) | _mm512_cmpneq_epu8_mask(z3, z4);
                if (!ret)
                {
                    offset += ZMM_SZ;
                    z1 = _mm512_load_si512(str1 + offset);
                    z2 = _mm512_load_si512(str2 + offset);
                    //Check for NULL
                    ret = _mm512_cmpeq_epu8_mask(z1, z0) | _mm512_cmpneq_epu8_mask(z1,z2);
                    if (!ret)
                    {
                        offset += ZMM_SZ;
                        z3 = _mm512_load_si512(str1 + offset);
                        z4 = _mm512_load_si512(str2 + offset);
                        //Check for NULL
                        ret = _mm512_cmpeq_epu8_mask(z3, z0) | _mm512_cmpneq_epu8_mask(z3, z4);
                    }
                }
            }
            if (ret)
                break;
            offset += ZMM_SZ;
        }
    }
    // Handle the case where alignments of the strings are different
    else
    {
        // Use masked loads to handle the misalignment
        size_t mix_offset = 0;
        z7 = _mm512_set1_epi8(0xff);
        mix_offset = (offset1 >= offset2);
        offset = (mix_offset)*offset1 | (!mix_offset)*offset2;
        mask = ((uint64_t)-1) >> offset;

        z1 =  _mm512_mask_loadu_epi8(z7 ,mask, str1);
        z2 =  _mm512_mask_loadu_epi8(z7 ,mask, str2);

        ret = _mm512_cmpeq_epu8_mask(z1, z0) | _mm512_cmpneq_epu8_mask(z1,z2);

        if (ret)
        {
            cmp_idx = _tzcnt_u64(ret);
            return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
        }
        offset = ZMM_SZ - offset;
        while(1)
        {
            z1 = _mm512_loadu_si512(str1 + offset);
            z2 = _mm512_loadu_si512(str2 + offset);
            //Check for NULL
            ret = _mm512_cmpeq_epu8_mask(z1, z0) | _mm512_cmpneq_epu8_mask(z1,z2);
            if (!ret)
            {
                offset += ZMM_SZ;
                z3 = _mm512_loadu_si512(str1 + offset);
                z4 = _mm512_loadu_si512(str2 + offset);
                //Check for NULL
                ret = _mm512_cmpeq_epu8_mask(z3, z0) | _mm512_cmpneq_epu8_mask(z3, z4);
                if (!ret)
                {
                    offset += ZMM_SZ;
                    z1 = _mm512_loadu_si512(str1 + offset);
                    z2 = _mm512_loadu_si512(str2 + offset);
                    //Check for NULL
                    ret = _mm512_cmpeq_epu8_mask(z1, z0) | _mm512_cmpneq_epu8_mask(z1,z2);
                    if (!ret)
                    {
                        offset += ZMM_SZ;
                        z3 = _mm512_loadu_si512(str1 + offset);
                        z4 = _mm512_loadu_si512(str2 + offset);
                        //Check for NULL
                        ret = _mm512_cmpeq_epu8_mask(z3, z0) | _mm512_cmpneq_epu8_mask(z3, z4);
                    }
                }
            }
            if (ret)
                break;
            offset += ZMM_SZ;
        }
    }
    cmp_idx = _tzcnt_u64(ret) + offset;
    return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
}
