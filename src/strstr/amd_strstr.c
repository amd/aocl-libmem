/* Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
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
#include <string.h>
#include "alm_defs.h"

static inline uint8_t _strlen_ble_ymm(const char *str1, uint8_t size)
{
    __m128i x0, x1, x2, x_null;
    uint16_t ret = 0;
    uint8_t cmp_idx = 0;

    if (size == 1)
    {
        if (*str1)
            return YMM_SZ;
        return 0;
    }
    x0 = _mm_setzero_si128();
    if (size <= 2 * WORD_SZ)
    {
        x1 = _mm_loadu_si16((void *)str1);
        x_null = _mm_cmpeq_epi8(x1, x0);
        ret = _mm_movemask_epi8(x_null);
        if (!(ret & 0x3))
        {
            cmp_idx = size - WORD_SZ;
            x2 = _mm_loadu_si16((void *)str1 + cmp_idx);
            x_null= _mm_cmpeq_epi8(x2, x0);
            ret = _mm_movemask_epi8(x_null);
            if (!(ret & 0x3))
                 return YMM_SZ;
        }
        cmp_idx += ALM_TZCNT_U16(ret & 0x3);
        return cmp_idx;
    }
    if (size <= 2 * DWORD_SZ)
    {
        x1 = _mm_loadu_si32((void *)str1);
        x_null = _mm_cmpeq_epi8(x1, x0);
        ret = _mm_movemask_epi8(x_null);
        if (!(ret & 0xf))
        {
            cmp_idx = size - DWORD_SZ;
            x2 = _mm_loadu_si32((void *)str1 + cmp_idx);
            x_null = _mm_cmpeq_epi8(x2, x0);
            ret = _mm_movemask_epi8(x_null);
            if (!(ret & 0xf))
                 return YMM_SZ;
        }
        cmp_idx += ALM_TZCNT_U16(ret & 0xf);
        return cmp_idx;
    }
    if (size <= 2 * QWORD_SZ)
    {
        x1 = _mm_loadu_si64((void *)str1);
        x_null = _mm_cmpeq_epi8(x1, x0);
        ret = _mm_movemask_epi8(x_null);
        if (!(ret & 0xff))
        {
            cmp_idx = size - QWORD_SZ;
            x2 = _mm_loadu_si64((void *)str1 + cmp_idx);
            x_null = _mm_cmpeq_epi8(x2, x0);
            ret = _mm_movemask_epi8(x_null);
            if (!(ret & 0xff))
                 return YMM_SZ;
        }
        cmp_idx += ALM_TZCNT_U16(ret & 0xff);
        return cmp_idx;
    }
    if (size <= 2 * XMM_SZ)
    {
        x1 = _mm_loadu_si128((void *)str1);
        x_null = _mm_cmpeq_epi8(x1, x0);
        ret = _mm_movemask_epi8(x_null);
        if (!ret)
        {
            cmp_idx = size - XMM_SZ;
            x2 = _mm_loadu_si128((void *)str1 + cmp_idx);
            x_null = _mm_cmpeq_epi8(x2, x0);
            ret = _mm_movemask_epi8(x_null);
            if (!ret)
                return YMM_SZ;
        }
        cmp_idx += ALM_TZCNT_U16(ret);
        return cmp_idx;
    }
    return YMM_SZ;
}

static inline size_t _strlen_avx2(const char *str)
{
    size_t offset = 0;
    __m256i y0, y1, y2, y3, y4, y5, y6, y_null;
    int32_t  ret = 0, ret1 = 0, ret2 = 0;

    y0 = _mm256_setzero_si256();
    offset = (uintptr_t)str & (YMM_SZ - 1); //str alignment

    if (offset)
    {
        if ((PAGE_SZ - YMM_SZ) < ((PAGE_SZ - 1) & (uintptr_t)str))
        {
            ret = _strlen_ble_ymm(str, YMM_SZ - offset);
            if (ret != YMM_SZ)
            {
                return ret;
            }
        }
        else
        {
            y1 = _mm256_loadu_si256((void *)str);
            ret = (uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(y1, y0));
            if (ret)
                return _tzcnt_u32(ret);
        }
        offset = YMM_SZ - offset;
    }

    y1 = _mm256_load_si256((void *)str + offset);
    y_null = _mm256_cmpeq_epi8(y1, y0);
    ret = _mm256_movemask_epi8(y_null);

    if(!ret)
    {
        offset += YMM_SZ;
        y2 = _mm256_load_si256((void *)str + offset);

        y_null = _mm256_cmpeq_epi8(y2, y0);
        ret = _mm256_movemask_epi8(y_null);
        if (!ret)
        {
            offset += YMM_SZ;
            y3 = _mm256_load_si256((void *)str + offset);
            y_null = _mm256_cmpeq_epi8(y3, y0);
            ret = _mm256_movemask_epi8(y_null);

            if (!ret)
            {
                offset += YMM_SZ;
                y4 = _mm256_load_si256((void *)str + offset);
                y_null = _mm256_cmpeq_epi8(y4, y0);
                ret = _mm256_movemask_epi8(y_null);
            }
        }
    }
    if (ret)
        return _tzcnt_u32(ret) + offset;

    offset += YMM_SZ;
    uint32_t vec_offset = ((uintptr_t)str + offset) & (4 * YMM_SZ - 1);
    if (vec_offset)
    {
        switch(vec_offset)
        {
            case YMM_SZ:
                y1 = _mm256_load_si256((void *)str + offset);
                y_null = _mm256_cmpeq_epi8(y1, y0);
                ret = _mm256_movemask_epi8(y_null);
                if (ret)
                    return _tzcnt_u32(ret) + offset;
                offset += YMM_SZ;
            case (2 * YMM_SZ):
                y2 = _mm256_load_si256((void *)str + offset);
                y_null = _mm256_cmpeq_epi8(y2, y0);
                ret = _mm256_movemask_epi8(y_null);
                if (ret)
                    return _tzcnt_u32(ret) + offset;
                offset += YMM_SZ;
            case (3 * YMM_SZ):
                y3 = _mm256_load_si256((void *)str + offset);
                y_null = _mm256_cmpeq_epi8(y3, y0);
                ret = _mm256_movemask_epi8(y_null);
                if (ret)
                    return _tzcnt_u32(ret) + offset;
                offset += YMM_SZ;
        }
    }
    while(1)
    {
        y1 = _mm256_load_si256((void *)str + offset);
        y2 = _mm256_load_si256((void *)str + offset + YMM_SZ);
        y3 = _mm256_load_si256((void *)str + offset + 2 * YMM_SZ);
        y4 = _mm256_load_si256((void *)str + offset + 3 * YMM_SZ);

        y5 = _mm256_min_epu8(y1, y2);
        y6 = _mm256_min_epu8(y3, y4);

        ret = _mm256_movemask_epi8(_mm256_cmpeq_epi8(_mm256_min_epu8(y5, y6), y0));
        if (ret != 0)
            break;

        offset += 4 * YMM_SZ;
    }

    if (ret)
    {
        if ((ret1 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y5, y0))))
        {
            if ((ret2 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y1, y0))))
            {
                return (_tzcnt_u32(ret2) + offset);
            }
            return (_tzcnt_u32(ret1) + YMM_SZ + offset);
        }

        else
        {
            if ((ret2 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y3, y0))))
            {
                return (_tzcnt_u32(ret2) + 2 * YMM_SZ + offset);
            }
            return (_tzcnt_u32(ret) + 3 * YMM_SZ + offset);
        }
    }
    return 0;
}

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

static inline int _strncmp_avx2(const char *str1, const char *str2, size_t size)
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

static inline char* strstr_avx2(const char* haystack, const char* needle) {
    size_t needle_len = 0;
    __m256i needle_char;
    __m256i y0, y1;
    __mmask32 match_mask, null_mask;
    uint32_t  match_idx, null_idx;
    size_t offset = 0;

    if (needle[0] == STR_TERM_CHAR)
        return (char*)haystack;

    if (haystack[0] == STR_TERM_CHAR)
        return NULL;

    //Broadcast needle[0]--> needle first char
    needle_char = _mm256_set1_epi8(needle[0]);

    y0 = _mm256_setzero_si256();
    offset = (uintptr_t)haystack & (YMM_SZ - 1);
    if (offset != 0)
    {
        if ((PAGE_SZ - YMM_SZ) < ((PAGE_SZ -1) & (uintptr_t)haystack))
        {
            y1 = _mm256_load_si256((void *)haystack - offset);
            null_mask = (uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(y0, y1)) >> offset;
            match_mask = (uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(needle_char, y1)) >> offset;
        }
        else
        {
            y1 = _mm256_loadu_si256((void*)haystack);
            match_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y1, needle_char));
            null_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y1, y0));
        }
        //Occurence of NEEDLE's first char
        if (match_mask)
        {
            needle_len = _strlen_avx2(needle);
            match_idx = _tzcnt_u32(match_mask);
            if (null_mask)
            {
                //if match index is beyond haystack size
                null_idx = _tzcnt_u32(null_mask);
                if (match_idx > null_idx)
                    return NULL;

                if (needle_len > (null_idx - match_idx))
                    return NULL;
            }
            //Loop until match_mask is clear
            do{
                match_idx = _tzcnt_u32(match_mask);
                if ((*(haystack + match_idx + 1) == *(needle + 1)))
                {
                    if (!(_strncmp_avx2(((char*)(haystack + match_idx)), needle, needle_len)))
                        return (char*)(haystack + match_idx);
                }
                   //clears the LOWEST SET BIT
                match_mask = _blsr_u32(match_mask);
            }while (match_mask);

        }
        if (null_mask)
            return NULL;

        offset = YMM_SZ - offset;
    }
    //Aligned case:
    if (!needle_len)
        needle_len = _strlen_avx2(needle);
    while(1)
    {
        y1 = _mm256_load_si256((void*)haystack + offset);
        match_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y1, needle_char));
        null_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y1, y0));
        //Occurence of NEEDLE's first char
        if (match_mask)
        {
            if (null_mask)
            {
                match_idx = _tzcnt_u32(match_mask);
                //if match index is beyond haystack size
                null_idx = _tzcnt_u32(null_mask);
                if (match_idx > null_idx)
                    return NULL;

                if (needle_len > (null_idx - match_idx))
                    return NULL;
            }
            do{
                match_idx = _tzcnt_u32(match_mask);
                if (*(haystack + match_idx + offset + 1) == *(needle + 1))
                {
                    if (!(_strncmp_avx2(((char*)(haystack + offset + match_idx)), needle, needle_len)))
                        return (char*)(haystack + offset + match_idx);
                }
                    //clears the LOWEST SET BIT
                match_mask = _blsr_u32(match_mask);
            }while (match_mask);
        }
        //null_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y1, y0));
        if (null_mask)
            return NULL;
        offset += YMM_SZ;
    }
    return NULL;
}

#ifdef AVX512_FEATURE_ENABLED
static inline size_t __strlen_avx512(const char *str)
{
    size_t offset;
    __m512i z0, z1, z2, z3, z4, z5, z6, z7;
    uint64_t ret, ret1, ret2;
    __mmask64 mask;
    z0 = _mm512_setzero_epi32 ();

    offset = (uintptr_t)str & (ZMM_SZ - 1);
    if (offset != 0)
    {
        if ((PAGE_SZ - ZMM_SZ) < ((PAGE_SZ -1) & (uintptr_t)str))
        {
            z7 = _mm512_set1_epi8(0xff);
            mask = ((uint64_t)-1) >> offset;
            z1 = _mm512_mask_loadu_epi8(z7 ,mask, str);
        }
        else
        {
            z1 = _mm512_loadu_si512(str);
        }
        ret = _mm512_cmpeq_epu8_mask(z1, z0);
        if (ret)
            return _tzcnt_u64(ret);
        offset = ZMM_SZ - offset;
    }

    z1 = _mm512_load_si512(str + offset);
    ret = _mm512_cmpeq_epu8_mask(z1, z0);

    if (!ret)
    {
        offset += ZMM_SZ;
        z2 = _mm512_load_si512(str + offset);
        //Check for NULL
        ret = _mm512_cmpeq_epu8_mask(z2, z0);
        if (!ret)
        {
            offset += ZMM_SZ;
            z3 = _mm512_load_si512(str + offset);
            //Check for NULL
            ret = _mm512_cmpeq_epu8_mask(z3, z0);
            if (!ret)
            {
                offset += ZMM_SZ;
                z4 = _mm512_load_si512(str + offset);
                //Check for NULL
                ret = _mm512_cmpeq_epu8_mask(z4, z0);
            }
        }
    }
    if (ret)
        return _tzcnt_u64(ret) + offset;

    offset +=  ZMM_SZ;
    uint64_t vec_offset = ((uintptr_t)str + offset) & (4 * ZMM_SZ - 1);
   if (vec_offset)
    {
        switch(vec_offset)
        {
            case ZMM_SZ:
                z1 = _mm512_load_si512(str + offset);
                ret = _mm512_cmpeq_epu8_mask(z1, z0);
                if (ret)
                    return _tzcnt_u64(ret) + offset;
                offset += ZMM_SZ;
            case (2 * ZMM_SZ):
                z2 = _mm512_load_si512(str + offset);
                ret = _mm512_cmpeq_epu8_mask(z2, z0);
                if (ret)
                    return _tzcnt_u64(ret) + offset;
                offset += ZMM_SZ;
            case (3 * ZMM_SZ):
                z3 = _mm512_load_si512(str + offset);
                ret = _mm512_cmpeq_epu8_mask(z3, z0);
                if (ret)
                    return _tzcnt_u64(ret) + offset;
                offset += ZMM_SZ;
        }
    }

    while (1)
    {
        z1 = _mm512_load_si512(str + offset);
        z2 = _mm512_load_si512(str + offset + ZMM_SZ);
        z3 = _mm512_load_si512(str + offset + 2 * ZMM_SZ);
        z4 = _mm512_load_si512(str + offset + 3 * ZMM_SZ);

        z5 = _mm512_min_epu8(z1,z2);
        z6 = _mm512_min_epu8(z3,z4);

        ret = _mm512_cmpeq_epu8_mask(_mm512_min_epu8(z5, z6), z0);
        if (ret != 0)
          break;

        offset += 4 * ZMM_SZ;
    }

    if  (ret)
    {
        if ((ret1 = _mm512_cmpeq_epu8_mask(z5, z0)))
        {
            if ((ret2 = _mm512_cmpeq_epu8_mask(z1, z0)))
            {
                return (_tzcnt_u64(ret2) + offset);
            }
            return (_tzcnt_u64(ret1) + ZMM_SZ + offset);
        }
        else
        {
            if ((ret2 =_mm512_cmpeq_epu8_mask(z3, z0)))
            {
                return (_tzcnt_u64(ret2) + 2 * ZMM_SZ + offset);
            }
            return  (_tzcnt_u64(ret) + 3 * ZMM_SZ + offset);
        }
    }
    return 0;
}
static inline int _strncmp_avx512(const char *str1, const char *str2, size_t size)
{
    size_t offset = 0;
    __m512i z0, z1, z2, z3, z4, z5, z6, z7, z8;
    uint64_t  cmp_idx, ret, ret1, ret2;
    uint64_t mask1, mask2;
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

static inline char* strstr_avx512(const char* haystack, const char* needle) {
    size_t needle_len = 0;
    __m512i zneedle_char;
    __m512i z0, z1, z7;
    __mmask64 match_mask, null_mask, mask;
    uint64_t  match_idx, null_idx;
    size_t offset = 0;

    if (needle[0] == STR_TERM_CHAR)
        return (char*)haystack;

    if (haystack[0] == STR_TERM_CHAR)
        return NULL;

    //Broadcast needle[0]--> needle first char
    zneedle_char = _mm512_set1_epi8(needle[0]);

    z0 = _mm512_setzero_epi32 ();
    offset = ((uintptr_t)haystack & (ZMM_SZ - 1));
    //Unaligned CASE:
    if (offset != 0)
    {
        if ((PAGE_SZ - ZMM_SZ) < ((PAGE_SZ - 1) & (uintptr_t)haystack))
        {
            z7 = _mm512_set1_epi8(0xff);
            mask = ((uint64_t)-1) >> offset;
            z1 = _mm512_mask_loadu_epi8(z7, mask, haystack);
        }
        else
        {
            z1 = _mm512_loadu_si512(haystack);
        }

        match_mask = _mm512_cmpeq_epi8_mask(z1, zneedle_char);
        null_mask = _mm512_cmpeq_epu8_mask(z1, z0);
        //Occurence of NEEDLE's first char
        if (match_mask)
        {
            needle_len = __strlen_avx512(needle);
            match_idx = _tzcnt_u64(match_mask);
            //NULL-check for HAYSTACK
            if (null_mask)
            {
                //if match index is beyond haystack size
                null_idx = _tzcnt_u64(null_mask);
                if (match_idx > null_idx)
                    return NULL;

                if (needle_len > (null_idx - match_idx))
                    return NULL;
            }
            //Loop until match_mask is clear
            do{
                match_idx = _tzcnt_u64(match_mask);
                if ((*(haystack + match_idx + 1) == *(needle + 1)))
                {
                    if (!(_strncmp_avx512(((char*)(haystack + match_idx)), needle, needle_len)))
                        return (char*)(haystack + match_idx);
                }
                   //clears the LOWEST SET BIT
                match_mask = _blsr_u64(match_mask);
            }while (match_mask);

        }
        if (null_mask)
            return NULL;

        offset = ZMM_SZ - offset;
    }
    //Aligned case:
    if (!needle_len)
        needle_len = __strlen_avx512(needle);
    while(1)
    {
        z1 = _mm512_load_si512(haystack + offset);
        match_mask = _mm512_cmpeq_epi8_mask(z1, zneedle_char);
        null_mask = _mm512_cmpeq_epu8_mask(z1, z0);
        //Occurence of NEEDLE's first char
        if (match_mask)
        {
            //NULL-check for HAYSTACK
            if (null_mask)
            {
                match_idx = _tzcnt_u64(match_mask);
                //if match index is beyond haystack size
                null_idx = _tzcnt_u64(null_mask);
                if (match_idx > null_idx)
                    return NULL;

                if (needle_len > (null_idx - match_idx))
                    return NULL;
            }
            do{
                match_idx = _tzcnt_u64(match_mask);
                if (*(haystack + match_idx + offset + 1) == *(needle + 1))
                {
                    if (!(_strncmp_avx512(((char*)(haystack + offset + match_idx)), needle, needle_len)))
                        return (char*)(haystack + offset + match_idx);
                }
                match_mask = _blsr_u64(match_mask);
            }while (match_mask);
        }
        //null_mask = _mm512_cmpeq_epu8_mask(z1, z0);
        if (null_mask)
            return NULL;
        offset += ZMM_SZ;
    }
    return NULL;
}
#endif
char * __attribute__((flatten)) amd_strstr(const char * __restrict haystack,
                                           const char * __restrict needle)
{
    LOG_INFO("\n");

#ifdef AVX512_FEATURE_ENABLED
    return strstr_avx512(haystack, needle);
#else
    return strstr_avx2(haystack, needle);
#endif
}
char *strstr(const char *, const char *) __attribute__((weak, alias("amd_strstr"),
                                                        visibility("default")));
