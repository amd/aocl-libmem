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
#include "threshold.h"
#include <stdint.h>
#include <immintrin.h>
#include "alm_defs.h"


uint8_t _strlen_ble_ymm(const char *str1, uint8_t size)
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

#ifdef AVX512_FEATURE_ENABLED
static inline size_t _strlen_avx512(const char *str)
{
    size_t offset = 0;
    __m512i z0, z1, z2, z3, z4, z5, z6, z7;
    uint64_t ret = 0, ret1 = 0, ret2 = 0;
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
#endif

size_t __attribute__((flatten)) amd_strlen(const char * __restrict str)
{
    LOG_INFO("\n");

#ifdef AVX512_FEATURE_ENABLED
    return _strlen_avx512(str);
#else
    return _strlen_avx2(str);
#endif
}

size_t strlen(const char *) __attribute__((weak, alias("amd_strlen"),
                                                        visibility("default")));
