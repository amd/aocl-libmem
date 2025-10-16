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

#include <stddef.h>
#include "logger.h"
#include <stdint.h>
#include <immintrin.h>
#include "almem_defs.h"
#include "zen_cpu_info.h"
#include "../../isa/avx2/optimized/strcmp_avx2.c"

HIDDEN_SYMBOL int __attribute__((flatten)) __strcmp_zen4(const char *str1, const char *str2)
{
    LOG_INFO("\n");
    size_t offset1, offset2, offset;
    uint64_t  cmp_idx, ret;
    __m256i y0, y1, y2, y3, y4, y_cmp, y_null;

    y0 = _mm256_setzero_si256();

    // Handle cases where start of either of the string is close to the end of a memory page
    if (unlikely(((PAGE_SZ - ZMM_SZ) < ((PAGE_SZ - 1) & ((uintptr_t)str1 | (uintptr_t)str2)))))
    {
        offset1 = (uintptr_t)str1 & (ZMM_SZ - 1);
        offset2 = (uintptr_t)str2 & (ZMM_SZ - 1);
        offset = (offset1 >= offset2) ? offset1 : offset2;

        if (offset < YMM_SZ)
        {
            y1 =  _mm256_loadu_si256((void *)str1);
            y2 =  _mm256_loadu_si256((void *)str2);
            y_cmp = _mm256_cmpeq_epi8(y1, y2);
            y_null = _mm256_cmpgt_epi8(y1, y0);
            ret = _mm256_movemask_epi8(_mm256_and_si256(y_cmp, y_null)) + 1;
            if (ret)
            {
                cmp_idx = _tzcnt_u32(ret);
                return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
            }
            cmp_idx = _strcmp_ble_ymm(str1 + YMM_SZ, str2 + YMM_SZ, YMM_SZ - offset);
            if (cmp_idx != YMM_SZ)
            {
                cmp_idx += YMM_SZ;
                return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
            }
        }
        else
        {
            cmp_idx = _strcmp_ble_ymm(str1, str2, (ZMM_SZ) - offset);
            if (cmp_idx != YMM_SZ)
            {
                return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
            }
        }
    }
    // If the strings are not close to the end of a memory page, check the first 64 bytes
    else
    {
        y1 =  _mm256_loadu_si256((void *)str1);
        y2 =  _mm256_loadu_si256((void *)str2);
        y_cmp = _mm256_cmpeq_epi8(y1, y2);
        y_null = _mm256_cmpgt_epi8(y1, y0);
        ret = _mm256_movemask_epi8(_mm256_and_si256(y_cmp, y_null)) + 1;
        if (ret)
        {
            cmp_idx = _tzcnt_u32(ret);
            return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
        }

        y3 =  _mm256_loadu_si256((void *)str1 + YMM_SZ);
        y4 =  _mm256_loadu_si256((void *)str2 + YMM_SZ);
        y_cmp = _mm256_cmpeq_epi8(y3, y4);
        y_null = _mm256_cmpgt_epi8(y3, y0);
        ret = _mm256_movemask_epi8(_mm256_and_si256(y_cmp, y_null)) + 1;
        if (ret)
        {
            cmp_idx = _tzcnt_u32(ret) + YMM_SZ;
            return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
        }
        offset1 = (uintptr_t)str1 & (ZMM_SZ - 1);
        offset2 = (uintptr_t)str2 & (ZMM_SZ - 1);
        offset = (offset1 >= offset2) ? offset1 : offset2;
    }
    // Adjust the offset to align with cache line for the next load operation
    offset = ZMM_SZ - offset;

    __m512i z0, z1, z2, z3, z4, z_mask;
    z0 = _mm512_setzero_epi32 ();

    // Alignement of both the strings is same
    if (offset1 == offset2)
    {
        // Adjust the offset to align with cache line for the next load operation
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
        char *aligned_str, *unaligned_str;
        if ((((uintptr_t)str1 + offset) & (ZMM_SZ - 1)) == 0)
        {
            aligned_str = (char *)str1;
            unaligned_str = (char *)str2;
        }
        else
        {
            aligned_str = (char *)str2;
            unaligned_str = (char *)str1;
        }

        uint16_t vecs_in_page  = (PAGE_SZ - ((PAGE_SZ - 1) & ((uintptr_t)unaligned_str + offset))) >> 6;
        while (1)
        {
            while (vecs_in_page >= 4)
            {
                _mm_prefetch(str1 + offset + 4 * ZMM_SZ, _MM_HINT_NTA);
                _mm_prefetch(str2 + offset + 4 * ZMM_SZ, _MM_HINT_NTA);
                z1 = _mm512_load_si512(aligned_str + offset);
                z2 = _mm512_loadu_si512(unaligned_str + offset);
                //Check for NULL
                ret = _mm512_cmpeq_epu8_mask(z1, z0) | _mm512_cmpneq_epu8_mask(z1,z2);
                if (!ret)
                {
                    offset += ZMM_SZ;
                    z3 = _mm512_load_si512(aligned_str + offset);
                    z4 = _mm512_loadu_si512(unaligned_str + offset);
                    //Check for NULL
                    ret = _mm512_cmpeq_epu8_mask(z3, z0) | _mm512_cmpneq_epu8_mask(z3, z4);
                    if (!ret)
                    {
                        offset += ZMM_SZ;
                        z1 = _mm512_load_si512(aligned_str + offset);
                        z2 = _mm512_loadu_si512(unaligned_str + offset);
                        //Check for NULL
                        ret = _mm512_cmpeq_epu8_mask(z1, z0) | _mm512_cmpneq_epu8_mask(z1,z2);
                        if (!ret)
                        {
                            offset += ZMM_SZ;
                            z3 = _mm512_load_si512(aligned_str + offset);
                            z4 = _mm512_loadu_si512(unaligned_str + offset);
                            //Check for NULL
                            ret = _mm512_cmpeq_epu8_mask(z3, z0) | _mm512_cmpneq_epu8_mask(z3, z4);
                            if (!ret)
                            {
                                offset += ZMM_SZ;
                                vecs_in_page -= 4;
                                continue;
                            }
                        }
                    }
                }
                cmp_idx = _tzcnt_u64(ret) + offset;
                return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
            }
            while (vecs_in_page--)
            {
                _mm_prefetch(str1 + offset + ZMM_SZ, _MM_HINT_NTA);
                _mm_prefetch(str2 + offset + ZMM_SZ, _MM_HINT_NTA);
                z1 = _mm512_load_si512(aligned_str + offset);
                z2 = _mm512_loadu_si512(unaligned_str + offset);
                //Check for NULL
                ret = _mm512_cmpeq_epu8_mask(z1, z0) | _mm512_cmpneq_epu8_mask(z1,z2);
                if (ret)
                {
                    cmp_idx = _tzcnt_u64(ret) + offset;
                    return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
                }
                offset += ZMM_SZ;
            }
            // Handle the case where we are crossing a page boundary for the unaligned string
            z_mask = _mm512_set1_epi8(0xff);
            uint64_t mask = UINT64_MAX >> ((uintptr_t)(unaligned_str + offset)& (ZMM_SZ - 1));

            z1 =  _mm512_mask_loadu_epi8(z_mask, mask, aligned_str + offset);
            z2 =  _mm512_mask_loadu_epi8(z_mask, mask, unaligned_str + offset);

            ret = _mm512_cmpeq_epu8_mask(z1, z0) | _mm512_cmpneq_epu8_mask(z1,z2);

            if (ret)
            {
                cmp_idx = _tzcnt_u64(ret) + offset;
                return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
            }
            vecs_in_page  += PAGE_SZ / ZMM_SZ;
        } //end of top level while
    }
    cmp_idx = _tzcnt_u64(ret) + offset;
    return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
}

#ifndef ALMEM_DYN_DISPATCH
int strcmp(const char *, const char *) __attribute__((weak,
                        alias("__strcmp_zen4")));
#endif
