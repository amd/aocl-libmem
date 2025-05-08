/* Copyright (C) 2023-24 Advanced Micro Devices, Inc. All rights reserved.
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
#include <zen_cpu_info.h>

static inline size_t __attribute__((flatten)) _strlen_avx2(const char *str)
{
    size_t offset;
    __m256i y0, y1, y2, y3, y4, y5, y6;
    int32_t  ret, ret1, ret2;

    y0 = _mm256_setzero_si256();

    if (unlikely(((PAGE_SZ - YMM_SZ) < ((PAGE_SZ - 1) & (uintptr_t)str))))
    {
        offset = (uintptr_t)str & (YMM_SZ - 1);
        y1 = _mm256_loadu_si256((void *)str - offset);
        ret = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y1, y0));
        ret = ret >> offset;
        if (ret)
            return _tzcnt_u32(ret);
    }
    else
    {
        y1 = _mm256_loadu_si256((void *)str);
        ret = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y1, y0));
        if (ret)
            return _tzcnt_u32(ret);
    }

    // cache-line alignment
    if (!((uintptr_t)str & CACHE_LINE_OFFSET))
    {
        offset = ((uintptr_t)str & -(CACHE_LINE_OFFSET)) + YMM_SZ;
        y1 = _mm256_load_si256((void *)offset);
        ret = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y1, y0));
        if (ret)
            return _tzcnt_u32(ret) + offset - (uintptr_t)str;
    }

    // 4-vec alignment
    if (!((uintptr_t)str & AVX2_VEC_4_OFFSET))
    {
        offset = ((uintptr_t)str & -(AVX2_VEC_4_OFFSET)) + (2 * YMM_SZ);
        y1 = _mm256_load_si256((void*)offset);
        y2 = _mm256_load_si256((void*)offset + YMM_SZ);
        y3 = _mm256_cmpeq_epi8(y1, y0);
        y4 = _mm256_cmpeq_epi8(y2, y0);
        ret = _mm256_movemask_epi8(_mm256_or_si256(y3, y4));
        if (ret)
        {
            ret1 = _mm256_movemask_epi8(y3);
            if (ret1)
                return _tzcnt_u32(ret1) + offset - (uintptr_t)str;
            return _tzcnt_u32(ret) + offset + YMM_SZ - (uintptr_t)str;
        }
    }

    offset = (uintptr_t)str & -(AVX2_VEC_4_SZ);
    do
    {
        offset += AVX2_VEC_4_SZ;
        y1 = _mm256_load_si256((void*)offset);
        y2 = _mm256_load_si256((void*)offset + YMM_SZ);
        y3 = _mm256_load_si256((void*)offset + 2 * YMM_SZ);
        y4 = _mm256_load_si256((void*)offset + 3 * YMM_SZ);

        y5 = _mm256_min_epu8(y1, y2);
        y6 = _mm256_min_epu8(y3, y4);

        ret = _mm256_movemask_epi8(_mm256_cmpeq_epi8(_mm256_min_epu8(y5, y6), y0));
    } while(!ret);

    if ((ret1 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y5, y0))))
    {
        if ((ret2 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y1, y0))))
        {
            return (_tzcnt_u32(ret2) + offset - (uintptr_t)str);
        }
        return (_tzcnt_u32(ret1) + YMM_SZ + offset  - (uintptr_t)str);
    }
    else
    {
        if ((ret2 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y3, y0))))
        {
            return (_tzcnt_u32(ret2) + 2 * YMM_SZ + offset - (uintptr_t)str);
        }
        return (_tzcnt_u32(ret) + 3 * YMM_SZ + offset - (uintptr_t)str);
    }
}
