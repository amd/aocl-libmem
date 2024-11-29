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
#include "threshold.h"
#include <stdint.h>
#include <immintrin.h>
#include "alm_defs.h"
#include "zen_cpu_info.h"

static inline int __attribute__((flatten)) _strncmp_avx512(const char *str1, const char *str2, size_t size)
{
    size_t offset = 0;
    __m512i z0, z1, z2, z3, z4, z5, z6, z7, z8;
    uint64_t  cmp_idx = 0, ret = 0, ret1 = 0, ret2 =0;
    uint64_t mask1 = 0, mask2 = 0;
    __mmask64 mask;

    z0 = _mm512_setzero_epi32 ();

    if (size <= ZMM_SZ)
    {
        if (size)
        {
            z7 = _mm512_set1_epi8(0xff);
            mask = ((uint64_t)-1) >> (ZMM_SZ - size);
            z1 =  _mm512_mask_loadu_epi8(z7 ,mask, str1);
            z2 =  _mm512_mask_loadu_epi8(z7 ,mask, str2);

            ret = _mm512_cmpneq_epu8_mask(z1,z2);

            if (ret)
            {
                cmp_idx = _tzcnt_u64(ret | _mm512_cmpeq_epu8_mask(z1, z0));
                return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
            }
        }
        return 0;
    }

    if (size <= 2 * ZMM_SZ)
    {
        z1 = _mm512_loadu_si512(str1);
        z2 = _mm512_loadu_si512(str2);

        ret = _mm512_cmpeq_epu8_mask(z1, z0) | _mm512_cmpneq_epu8_mask(z1,z2);
        if (!ret)
        {
                offset = size - ZMM_SZ;
                z3 = _mm512_loadu_si512(str1 + offset);
                z4 = _mm512_loadu_si512(str2 + offset);
                //Check for NULL
                ret = _mm512_cmpeq_epu8_mask(z3, z0) | _mm512_cmpneq_epu8_mask(z3, z4);
                if(!ret)
                    return 0;
        }
        cmp_idx = _tzcnt_u64(ret) + offset;
        return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
    }

    if (size <= 4* ZMM_SZ)
    {
        uint64_t mask1 = 0, mask2 = 0;
        z1 = _mm512_loadu_si512(str1);
        z2 = _mm512_loadu_si512(str1 + ZMM_SZ);

        z5 = _mm512_loadu_si512(str2);
        z6 = _mm512_loadu_si512(str2 + ZMM_SZ);

        mask1 = _mm512_cmpneq_epu8_mask(z1,z5);

        ret1 = _mm512_cmpeq_epu8_mask(_mm512_min_epu8(z1,z2), z0) | mask1 | _mm512_cmpneq_epu8_mask(z2,z6);
        if (ret1)
        {
            ret = _mm512_cmpeq_epu8_mask(z1,z0) | mask1;
            if (!ret)
            {
                offset += ZMM_SZ;
                ret = ret1;
            }
            cmp_idx = _tzcnt_u64(ret) + offset;
            return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
        }
        z3 = _mm512_loadu_si512(str1 + size - 2 * ZMM_SZ);
        z4 = _mm512_loadu_si512(str1 + size - ZMM_SZ);
        z7 = _mm512_loadu_si512(str2 + size - 2 * ZMM_SZ);
        z8 = _mm512_loadu_si512(str2 + size - ZMM_SZ);

        mask2 = _mm512_cmpneq_epu8_mask(z3,z7);
        ret2 = _mm512_cmpeq_epu8_mask(_mm512_min_epu8(z3,z4), z0) | mask2 | _mm512_cmpneq_epu8_mask(z4,z8);
        if (ret2)
        {
            offset = - 2 * ZMM_SZ;
            ret = _mm512_cmpeq_epu8_mask(z3,z0) | mask2;
            if (!ret)
            {
                offset += ZMM_SZ;
                ret = ret2;
            }
            cmp_idx = _tzcnt_u64(ret) + size + offset;
            return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
        }
        return 0;
    }

    while ((size - offset) >= 4 * ZMM_SZ)
    {
        z1 = _mm512_loadu_si512(str1 + offset);
        z2 = _mm512_loadu_si512(str1 + offset + 1 * ZMM_SZ);

        z5 = _mm512_loadu_si512(str2 + offset);
        z6 = _mm512_loadu_si512(str2 + offset + 1 * ZMM_SZ);

        mask1 = _mm512_cmpneq_epu8_mask(z1,z5);
        ret1 = _mm512_cmpeq_epu8_mask(_mm512_min_epu8(z1,z2), z0) | mask1 | _mm512_cmpneq_epu8_mask(z2,z6);

        if (ret1)
        {
            ret = _mm512_cmpeq_epu8_mask(z1,z0) | mask1;
            if (!ret)
            {
                offset += ZMM_SZ;
                ret = ret1;
            }
            cmp_idx = _tzcnt_u64(ret) + offset;
            return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
        }

        z3 = _mm512_loadu_si512(str1 + offset + 2 * ZMM_SZ);
        z4 = _mm512_loadu_si512(str1 + offset + 3 * ZMM_SZ);
        z7 = _mm512_loadu_si512(str2 + offset + 2 * ZMM_SZ);
        z8 = _mm512_loadu_si512(str2 + offset + 3 * ZMM_SZ);
        mask2 = _mm512_cmpneq_epu8_mask(z3,z7);
        ret2 = _mm512_cmpeq_epu8_mask(_mm512_min_epu8(z3,z4), z0) | mask2 | _mm512_cmpneq_epu8_mask(z4,z8);

        if (ret2)
        {
            offset +=  2 * ZMM_SZ;
            ret = _mm512_cmpeq_epu8_mask(z3,z0) | mask2;
            if (!ret)
            {
                offset += ZMM_SZ;
                ret = ret2;
            }
            cmp_idx = _tzcnt_u64(ret) + offset;
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
            ret = _mm512_cmpeq_epu8_mask(z1, z0) | _mm512_cmpneq_epu8_mask(z1,z2);
            if (ret)
                break;
        case 2:
            offset = size - 3 * ZMM_SZ;
            z3 = _mm512_loadu_si512(str1 + offset);
            z4 = _mm512_loadu_si512(str2 + offset);
            ret = _mm512_cmpeq_epu8_mask(z3, z0) | _mm512_cmpneq_epu8_mask(z3, z4);
            if(ret)
                break;
        case 1:
            offset = size - 2 * ZMM_SZ;
            z1 = _mm512_loadu_si512(str1 + offset);
            z2 = _mm512_loadu_si512(str2 + offset);
            ret = _mm512_cmpeq_epu8_mask(z1, z0) | _mm512_cmpneq_epu8_mask(z1,z2);
            if(ret)
                break;
        case 0:
            offset = size - ZMM_SZ;
            z3 = _mm512_loadu_si512(str1 + offset);
            z4 = _mm512_loadu_si512(str2 + offset);
            ret = _mm512_cmpeq_epu8_mask(z3, z0) | _mm512_cmpneq_epu8_mask(z3, z4);

            if(!ret)
                return 0;
    }
    cmp_idx = _tzcnt_u64(ret) + offset;
    return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
}
