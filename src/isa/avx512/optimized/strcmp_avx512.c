/* Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
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

static inline int __attribute__((flatten)) _strcmp_avx512(const char *str1, const char *str2)
{
    size_t offset1 = 0, offset2 = 0, offset = 0;
    __m512i z0, z1, z2, z3, z4, z7;
    uint64_t  cmp_idx = 0, ret = 0;
    __mmask64 mask;

    z0 = _mm512_setzero_epi32 ();

    offset1 = (uintptr_t)str1 & (ZMM_SZ - 1); //str1 alignment
    offset2 = (uintptr_t)str2 & (ZMM_SZ - 1); //str2 alignment
    //Both str2 and str1 are aligned

    if (offset1 == offset2)
    {
        offset = offset1;
        if (offset !=0)
        {
            z7 = _mm512_set1_epi8(0xff);
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
        }
        while (1)
        {
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
    //mismatching alignments
    else
    {
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
