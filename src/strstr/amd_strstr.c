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
#include "alm_defs.h"


static inline size_t transition_index (const char *needle)
{
  size_t idx = 0;
  while (needle[idx + NULL_BYTE] != STR_TERM_CHAR &&
        needle[idx] == needle[idx + 1]) {
        idx ++;
  }
  return (needle[idx + 1] == STR_TERM_CHAR) ? 0 : idx;
}

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

static inline int cmp_needle_avx2(const char *str1, const char *str2, size_t size)
{
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
        case 2:
            offset = size - 3 * YMM_SZ;
            y3 = _mm256_loadu_si256((void *)str1 + offset);
            y4 = _mm256_loadu_si256((void *)str2 + offset);
            y_cmp = _mm256_cmpeq_epi8(y3, y4);
            ret = _mm256_movemask_epi8(y_cmp) + 1;

            if(ret)
                break;
        case 1:
            offset = size - 2 * YMM_SZ;
            y1 = _mm256_loadu_si256((void *)str1 + offset);
            y2 = _mm256_loadu_si256((void *)str2 + offset);
            y_cmp = _mm256_cmpeq_epi8(y1, y2);
            ret = _mm256_movemask_epi8(y_cmp) + 1;

            if(ret)
                break;
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

static inline char* find_needle_chr_avx2(const char *str, const char ch)
{
    size_t  offset;
    __m256i y0, y1, y2, y3;
    __mmask32 match_mask, null_mask;
    uint32_t  match_idx, null_idx = 0;

    y0 = _mm256_setzero_si256 ();

    offset = (uintptr_t)str & (YMM_SZ - 1); //str alignment

    y2 =  _mm256_set1_epi8(ch);
    if  (offset)
    {
        if ((PAGE_SZ - YMM_SZ) < ((PAGE_SZ - 1) & (uintptr_t)str))
        {
            y1 =  _mm256_loadu_si256((void *)str - offset);
            match_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y1, y2));
            null_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y1, y0));
            match_mask = match_mask >> offset;
            null_mask = null_mask >> offset;
        }
        else
        {
            y1 = _mm256_loadu_si256((void *)str);
            match_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y1, y2));
            null_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y1, y0));
        }

        if (match_mask) {
            match_idx = _tzcnt_u32(match_mask);
            if (null_mask) {
                null_idx = _tzcnt_u32(null_mask);
                if (null_idx <= match_idx)
                    return NULL;
            }
            return (char*)str + match_idx;
        }
        offset = YMM_SZ - offset;
    }

    while (1)
    {
        y1 = _mm256_load_si256((void *)str + offset);
        //Check for NULL
        match_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y1, y2));
        null_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y1, y0));
        if (!(match_mask | null_mask))
        {
            offset += YMM_SZ;
            y3 = _mm256_load_si256((void *)str + offset);
            //Check for NULL
            match_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y3, y2));
            null_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y3, y0));
            if (!(match_mask | null_mask))
            {
                offset += YMM_SZ;
                y1 = _mm256_load_si256((void *)str + offset);
                //Check for NULL
                match_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y1, y2));
                null_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y1, y0));
                if (!(match_mask | null_mask))
                {
                    offset += YMM_SZ;
                    y3 = _mm256_load_si256((void *)str + offset);
                    //Check for NULL
                    match_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y3, y2));
                    null_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y3, y0));
                }
            }
        }
        if ((match_mask | null_mask))
            break;
        offset += YMM_SZ;
    }

    match_idx = _tzcnt_u32(match_mask);
    if (null_mask) {
        null_idx = _tzcnt_u32(null_mask);
        if (null_idx <= match_idx)
            return NULL;
    }
    return (char*)str + match_idx;
}


static inline char* strstr_avx2(const char* haystack, const char* needle)
{
    size_t needle_len = 0;
    __m256i needle_char_t0, needle_char_t1;
    __m256i y0, y1, y2;
    __mmask32 match_mask, null_mask, null_pfx_mask;
    uint32_t  match_idx, null_idx = 0;
    size_t offset;
    size_t transit_idx;
    uint8_t check_unaligned = 1;

    if (needle[0] == STR_TERM_CHAR)
        return (char*)haystack;

    if (haystack[0] == STR_TERM_CHAR)
        return NULL;

    if (needle[1] == STR_TERM_CHAR)
        return find_needle_chr_avx2(haystack, needle[0]);

    transit_idx = transition_index(needle);

    needle_char_t0 = _mm256_set1_epi8(needle[transit_idx]);
    needle_char_t1 = _mm256_set1_epi8(needle[transit_idx + 1]);

    y0 = _mm256_setzero_si256();
    offset = (uintptr_t)haystack & (YMM_SZ - 1);
    if (unlikely ((PAGE_SZ - YMM_SZ) < ((PAGE_SZ -1) & (uintptr_t)haystack)))
    {
        y1 = _mm256_load_si256((void *)haystack - offset);
        null_mask = (uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(y0, y1)) >> offset;
        if (null_mask)
        {
            null_idx = _tzcnt_u32(null_mask);
            if (null_idx <= transit_idx)
                return NULL;
            match_mask = (uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(needle_char_t0, y1)) >> offset;
            check_unaligned = 0;
        }
    }
    if (check_unaligned)
    {
        y1 = _mm256_loadu_si256((void*)haystack);
        null_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y1, y0));
        if (null_mask)
        {
            null_idx = _tzcnt_u32(null_mask);
            if (null_idx <= transit_idx)
                return NULL;
        }
        match_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y1, needle_char_t0));
    }

    //Occurence of NEEDLE's first char
    if (match_mask)
    {
        needle_len = _strlen_avx2(needle);
        if (null_mask)
        {
            match_idx = _tzcnt_u32(match_mask);
            //if match index is beyond haystack size
            if (match_idx > null_idx)
                return NULL;
            match_mask &= (null_mask ^ (null_mask - 1));
        }
        //Loop until match_mask is clear
        do{
            match_idx = _tzcnt_u32(match_mask);
            if ((*(haystack + match_idx + transit_idx + 1) == *(needle + transit_idx + 1)))
            {
                if (!(cmp_needle_avx2(((char*)(haystack + match_idx)), needle, needle_len)))
                    return (char*)(haystack + match_idx);
            }
            //clears the LOWEST SET BIT
            match_mask = _blsr_u32(match_mask);
        }while (match_mask);

    }
    if (null_mask)
        return NULL;

    offset = YMM_SZ - offset;
    //Aligned case:
    if (!needle_len)
        needle_len = _strlen_avx2(needle);

    while(!null_mask)
    {
        y1 = _mm256_loadu_si256((void*)haystack + offset - 1);
        y2 = _mm256_load_si256((void*)haystack + offset);
        match_mask = _mm256_movemask_epi8(_mm256_and_si256(_mm256_cmpeq_epi8(y1, needle_char_t0), _mm256_cmpeq_epi8(y2, needle_char_t1)));
        null_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y2, y0));
        null_pfx_mask = (null_mask ^ (null_mask - 1));
        match_mask = match_mask & null_pfx_mask;
        //Occurence of NEEDLE's first char
        while (match_mask)
        {
            match_idx = _tzcnt_u32(match_mask);
            if ((match_idx + offset) >= (transit_idx + 1)) {
                match_idx += offset - transit_idx - 1;
                if (unlikely((PAGE_SZ - YMM_SZ) < ((PAGE_SZ - 1) & (uintptr_t)(haystack + match_idx))))
                {
                    size_t idx = 0;
                    for (; (*(haystack + match_idx + idx) == *(needle + idx)) && (idx < needle_len); idx++);
                    if (idx == needle_len)
                        return (char*)(haystack + match_idx);
                }
                else
                {
                    if (!(cmp_needle_avx2(((char*)(haystack + match_idx)), needle, needle_len)))
                        return (char*)(haystack + match_idx);
                }
            }
            //clears the LOWEST SET BIT
            match_mask = _blsr_u32(match_mask);
        }
        offset += YMM_SZ;
    }
    return NULL;
}

#ifdef AVX512_FEATURE_ENABLED
static inline char* strchr_avx512(const char *str, const char ch)
{
    size_t  offset;
    __m512i z0, z1, z2, z3, z7;
    uint64_t  cmp_idx , ret;
    __mmask64 mask;

    z0 = _mm512_setzero_epi32 ();

    offset = (uintptr_t)str & (ZMM_SZ - 1); //str alignment

    z2 =  _mm512_set1_epi8(ch);
    if  (offset)
    {
        if ((PAGE_SZ - ZMM_SZ) < ((PAGE_SZ - 1) & (uintptr_t)str))
        {
            z7 = _mm512_set1_epi8(0xff);
            mask = ((uint64_t)-1) >> offset;
            z1 =  _mm512_mask_loadu_epi8(z7 ,mask, str);
        }
        else
        {
            z1 = _mm512_loadu_si512(str);
        }

        ret = _mm512_cmpeq_epu8_mask(z1, z0) | _mm512_cmpeq_epu8_mask(z1, z2);
        if (ret)
        {
            cmp_idx = _tzcnt_u64(ret);
            if(str[cmp_idx] == ch)
                return (char*)str + cmp_idx;
            else
                return NULL;
        }
        offset = ZMM_SZ - offset;
    }
    while (1)
    {
        z1 = _mm512_load_si512(str + offset);
        //Check for NULL
        ret = _mm512_cmpeq_epu8_mask(z1, z0) | _mm512_cmpeq_epu8_mask(z1,z2);
        if (!ret)
        {
            offset += ZMM_SZ;
            z3 = _mm512_load_si512(str + offset);
            //Check for NULL
            ret = _mm512_cmpeq_epu8_mask(z3, z0) | _mm512_cmpeq_epu8_mask(z3, z2);
            if (!ret)
            {
                offset += ZMM_SZ;
                z1 = _mm512_load_si512(str + offset);
                //Check for NULL
                ret = _mm512_cmpeq_epu8_mask(z1, z0) | _mm512_cmpeq_epu8_mask(z1,z2);
                if (!ret)
                {
                    offset += ZMM_SZ;
                    z3 = _mm512_load_si512(str + offset);
                    //Check for NULL
                    ret = _mm512_cmpeq_epu8_mask(z3, z0) | _mm512_cmpeq_epu8_mask(z3, z2);
                }
            }
        }
        if (ret)
            break;
        offset += ZMM_SZ;
    }

    cmp_idx = _tzcnt_u64(ret) + offset;
    if (str[cmp_idx] == ch)
        return (char*)str + cmp_idx;
    return NULL;
}

static inline bool is_match_at_index (const char *haystack, const size_t haystack_idx, const char *needle,  size_t idx)
{
    for ( ; needle[idx] != STR_TERM_CHAR; idx++)
    {
        if (needle[idx] != haystack [haystack_idx + idx])
            return false;
    }
    return true;
}
static inline bool substring_check (const char *haystack, const size_t haystack_idx,
                const char *needle, const __mmask64 needle_mask, const __m512i zneedle)
{
  __m512i zhaystack = _mm512_loadu_si512 (haystack + haystack_idx);
  __mmask64 match = _mm512_mask_cmpneq_epi8_mask (needle_mask, zhaystack, zneedle);
  if (match != NULL_MASK )
    return false;
  else if (needle_mask == ALL_BITS_SET)
    return is_match_at_index (haystack, haystack_idx, needle, ZMM_SZ);
  return true;
}

static inline char * strstr_avx512 (const char *haystack, const char *needle)
{
  if (!needle[0])
    return (char *)haystack;

  if (needle[1] == STR_TERM_CHAR)
    return (char*)strchr_avx512(haystack, needle[0]);

  size_t repeat = transition_index(needle);

  /* if repeat > haystack */
  for (size_t chk_len = 0; chk_len < repeat; chk_len++)
    {
      if (haystack[chk_len] == STR_TERM_CHAR)
        return NULL;
    }

   __m512i z7, z0;
   __mmask64 needle_load_mask;
   z7 = _mm512_set1_epi8(0xff);
   z0 = _mm512_setzero_epi32 ();
   size_t offset = 0;
   offset = (uintptr_t)needle & (ZMM_SZ - 1);
   needle_load_mask = ALL_BITS_SET >> offset;
   __m512i zneedle = _mm512_mask_loadu_epi8(z7 ,needle_load_mask, needle);
   __mmask64 needle_null_mask = _mm512_cmpeq_epi8_mask(zneedle, z0) & needle_load_mask;

    if (unlikely (needle_null_mask == NULL_MASK ))
    {
      zneedle = _mm512_loadu_si512 (needle);
      needle_null_mask = _mm512_testn_epi8_mask (zneedle, zneedle);
      needle_load_mask = needle_null_mask ^ (needle_null_mask - LOWER_BIT_SET);
      if (needle_null_mask != NULL_MASK )
        needle_load_mask = needle_load_mask >> 1;
    }
    else
    {
      needle_load_mask = needle_null_mask ^ (needle_null_mask - LOWER_BIT_SET);
      needle_load_mask = needle_load_mask >> 1;
    }

    const __m512i zneedle_main_byte = _mm512_set1_epi8 (needle[repeat]);
    const __m512i zneedle_follow_byte = _mm512_set1_epi8 (needle[repeat + 1]);

    size_t haystack_idx = repeat;

    offset = (uintptr_t)(haystack + haystack_idx) & (ZMM_SZ - 1);

    __mmask64 haystack_load_mask = ALL_BITS_SET >> offset;

    __m512i zhaystack_0 = _mm512_maskz_loadu_epi8 (haystack_load_mask, haystack + haystack_idx);

    uint64_t null_mask
        = (uint64_t) (_mm512_mask_testn_epi8_mask (haystack_load_mask, zhaystack_0, zhaystack_0));
    uint64_t cmp_mask = null_mask ^ (null_mask - LOWER_BIT_SET);
    cmp_mask = cmp_mask & ((uint64_t) (haystack_load_mask));

    __mmask64 first_mask = _mm512_cmpeq_epi8_mask (zhaystack_0, zneedle_main_byte);
    __mmask64 second_mask = _mm512_cmpeq_epi8_mask (zhaystack_0, zneedle_follow_byte);
    second_mask = second_mask >> 1;

    uint64_t both_mask = ((uint64_t) (second_mask & first_mask)) & cmp_mask;

    while (both_mask)
    {
      uint64_t cnt_bit = _tzcnt_u64 (both_mask);
      both_mask = _blsr_u64 (both_mask);
      size_t found_index = haystack_idx + cnt_bit - repeat;
      if (((uintptr_t) (haystack + found_index) & (PAGE_SZ - 1)) < PAGE_SZ - 1 - ZMM_SZ)
        {
            if (substring_check (haystack, found_index, needle,needle_load_mask, zneedle))
                return (char *)haystack + found_index;
        }
      else
        {
            if (is_match_at_index (haystack, found_index, needle, 0))
                return (char *)haystack + found_index;
        }
    }

    haystack = (const char *)(((uintptr_t) (haystack + haystack_idx) | (ZMM_SZ - 1)));
    haystack_idx = 0;

    __m512i zhaystack1;
    while (null_mask == 0)
    {
      zhaystack_0 = _mm512_loadu_si512 (haystack + haystack_idx);
      zhaystack1 = _mm512_load_si512 (haystack + haystack_idx + 1);
      null_mask = (uint64_t) (_mm512_testn_epi8_mask (zhaystack1, zhaystack1));

      cmp_mask = null_mask ^ (null_mask - LOWER_BIT_SET);
      first_mask = _mm512_cmpeq_epi8_mask (zhaystack_0, zneedle_main_byte);
      second_mask = _mm512_cmpeq_epi8_mask (zhaystack1, zneedle_follow_byte);

      both_mask = ((uint64_t) (second_mask & first_mask)) & cmp_mask;

      while (both_mask)
        {
          uint64_t cnt_bit = _tzcnt_u64 (both_mask);
          both_mask = _blsr_u64 (both_mask);
          size_t found_index = haystack_idx + cnt_bit - repeat;
          if (((uintptr_t) (haystack + found_index) & (PAGE_SZ - 1)) < PAGE_SZ - 1 - ZMM_SZ)
          {
            if (substring_check (haystack, found_index, needle, needle_load_mask, zneedle))
                return (char *)haystack + found_index;
          }
          else
          {
                if (is_match_at_index (haystack, found_index, needle, 0))
                return (char *)haystack + found_index;
          }
        }
      haystack_idx += ZMM_SZ;
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
