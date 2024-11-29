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

static inline int  _strncmp_ble_ymm(const char *str1, const char *str2, uint8_t size)
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
            if (!(ret & 0x3))
                 return 0;
        }
        cmp_idx += ALM_TZCNT_U16(ret & 0x3);
        return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
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
            if (!(ret & 0xf))
                 return 0;
        }
        cmp_idx += ALM_TZCNT_U16(ret & 0xf);
        return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
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
            if (!(ret & 0xff))
                 return 0;
        }
        cmp_idx += ALM_TZCNT_U16(ret & 0xff);
        return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
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
            if (!ret)
                return 0;
        }
        cmp_idx += ALM_TZCNT_U16(ret);
        return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
    }
    return 0;
}

static inline int __attribute__((flatten)) _strncmp_avx2(const char *str1, const char *str2, size_t size)
{
    size_t offset = 0;
    size_t cmp_idx =0;
    __m256i y0, y1, y2, y3, y4, y5, y6, y7, y8, y_cmp, y_cmp1, y_cmp2, y_null;
    int32_t  ret = 0, ret1 = 0, ret2 =0;

    if (size <= YMM_SZ)
    {
        if (size)
            return _strncmp_ble_ymm(str1, str2, size);
        return 0;
    }

    y0 = _mm256_setzero_si256();

    if (size <= 2 * YMM_SZ)
    {
        y1 = _mm256_loadu_si256((void *)str1);
        y2 = _mm256_loadu_si256((void *)str2);
        y_cmp = _mm256_cmpeq_epi8(y1, y2);
        y_null = _mm256_cmpgt_epi8(y1, y0);
        ret = _mm256_movemask_epi8(_mm256_and_si256(y_cmp, y_null)) + 1;
        if (!ret)
        {
            offset = size - YMM_SZ;
            y3 =  _mm256_loadu_si256((void *)str1 + offset);
            y4 =  _mm256_loadu_si256((void *)str2 + offset);

            y_cmp = _mm256_cmpeq_epi8(y3, y4);
            y_null = _mm256_cmpgt_epi8(y3, y0);
            ret = _mm256_movemask_epi8(_mm256_and_si256(y_cmp, y_null)) + 1;
            if(!ret)
                return 0;
        }
        cmp_idx = _tzcnt_u32(ret);
        return (unsigned char)str1[cmp_idx + offset] - (unsigned char)str2[cmp_idx + offset];
    }

    if( size <= 4* YMM_SZ)
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
        y_null = _mm256_cmpgt_epi8(_mm256_min_epu8(y1,y2), y0);

        ret1 = _mm256_movemask_epi8(_mm256_and_si256(_mm256_and_si256(y_cmp1, y_cmp2), y_null)) + 1;

        if(ret1)
        {
            ret = _mm256_movemask_epi8(_mm256_and_si256(y_cmp1,  _mm256_cmpgt_epi8(y1, y0))) + 1;
            if(!ret)
            {
                offset += YMM_SZ;
                ret = ret1;
            }
            cmp_idx = _tzcnt_u32(ret) + offset;
            return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
        }
        y_cmp1 = _mm256_cmpeq_epi8(y3, y7);
        y_cmp2 = _mm256_cmpeq_epi8(y4, y8);
        y_null = _mm256_cmpgt_epi8(_mm256_min_epu8(y3, y4), y0);
        ret2 = _mm256_movemask_epi8(_mm256_and_si256(_mm256_and_si256(y_cmp1, y_cmp2), y_null)) + 1;

        if(ret2)
        {
            offset = - 2 * YMM_SZ;
            ret = _mm256_movemask_epi8(_mm256_and_si256(y_cmp1,  _mm256_cmpgt_epi8(y3, y0))) + 1;
            if(!ret)
            {
                offset += YMM_SZ;
                ret = ret2;
            }
            cmp_idx = _tzcnt_u32(ret) + size + offset;
            return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
        }
        return 0;
    }

    while((size - offset) >= 4 * YMM_SZ)
    {
        y1 = _mm256_loadu_si256((void*)str1 + offset);
        y2 = _mm256_loadu_si256((void*)str1 + offset + 1 * YMM_SZ);
        y3 = _mm256_loadu_si256((void*)str1 + offset + 2 * YMM_SZ);
        y4 = _mm256_loadu_si256((void*)str1 + offset + 3 * YMM_SZ);

        y5 = _mm256_loadu_si256((void*)str2 + offset);
        y6 = _mm256_loadu_si256((void*)str2 + offset + 1 * YMM_SZ);
        y7 = _mm256_loadu_si256((void*)str2 + offset + 2 * YMM_SZ);
        y8 = _mm256_loadu_si256((void*)str2 + offset + 3 * YMM_SZ);

        y_cmp1 = _mm256_cmpeq_epi8(y1, y5);
        y_cmp2 = _mm256_cmpeq_epi8(y2, y6);
        y_null = _mm256_cmpgt_epi8(_mm256_min_epu8(y1,y2), y0);

        ret1 = _mm256_movemask_epi8(_mm256_and_si256(_mm256_and_si256(y_cmp1, y_cmp2), y_null)) + 1;

        if(ret1)
        {
            ret = _mm256_movemask_epi8(_mm256_and_si256(y_cmp1,  _mm256_cmpgt_epi8(y1, y0))) + 1;
            if(!ret)
            {
                offset += YMM_SZ;
                ret = ret1;
            }
            cmp_idx = _tzcnt_u32(ret) + offset;
            return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
        }
        y_cmp1 = _mm256_cmpeq_epi8(y3, y7);
        y_cmp2 = _mm256_cmpeq_epi8(y4, y8);
        y_null = _mm256_cmpgt_epi8(_mm256_min_epu8(y3, y4), y0);
        ret2 = _mm256_movemask_epi8(_mm256_and_si256(_mm256_and_si256(y_cmp1, y_cmp2), y_null)) + 1;

        if(ret2)
        {
            offset += 2 * YMM_SZ;
            ret = _mm256_movemask_epi8(_mm256_and_si256(y_cmp1,  _mm256_cmpgt_epi8(y3, y0))) + 1;
            if(!ret)
            {
                offset += YMM_SZ;
                ret = ret2;
            }
            cmp_idx = _tzcnt_u32(ret) + offset;
            return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
        }
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
            y_null = _mm256_cmpgt_epi8(y1, y0);
            ret = _mm256_movemask_epi8(_mm256_and_si256(y_cmp, y_null)) + 1;

            if (ret)
                break;
        case 2:
            offset = size - 3 * YMM_SZ;
            y3 = _mm256_loadu_si256((void *)str1 + offset);
            y4 = _mm256_loadu_si256((void *)str2 + offset);
            y_cmp = _mm256_cmpeq_epi8(y3, y4);
            y_null = _mm256_cmpgt_epi8(y3, y0);
            ret = _mm256_movemask_epi8(_mm256_and_si256(y_cmp, y_null)) + 1;
            if(ret)
                break;
        case 1:
            offset = size - 2 * YMM_SZ;
            y1 = _mm256_loadu_si256((void *)str1 + offset);
            y2 = _mm256_loadu_si256((void *)str2 + offset);
            y_cmp = _mm256_cmpeq_epi8(y1, y2);
            y_null = _mm256_cmpgt_epi8(y1, y0);
            ret = _mm256_movemask_epi8(_mm256_and_si256(y_cmp, y_null)) + 1;

            if(ret)
                break;
        case 0:
            offset = size - YMM_SZ;
            y3 = _mm256_loadu_si256((void *)str1 + offset);
            y4 = _mm256_loadu_si256((void *)str2 + offset);
            y_cmp = _mm256_cmpeq_epi8(y3, y4);
            y_null = _mm256_cmpgt_epi8(y3, y0);
            ret = _mm256_movemask_epi8(_mm256_and_si256(y_cmp, y_null)) + 1;

            if(!ret)
                return 0;
    }
    cmp_idx = _tzcnt_u32(ret) + offset;
    return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
}
