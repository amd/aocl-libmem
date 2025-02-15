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

static inline uint8_t __strlen_ble_ymm(const char *str1, uint8_t size)
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

static inline size_t __strlen_avx2(const char *str)
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
            ret = __strlen_ble_ymm(str, YMM_SZ - offset);
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

static inline void _strcpy_ble_ymm(void *dst, const void *src, uint32_t size)
{
    __m128i x0,x1;

    if (size == 1)
    {
        *((uint8_t *)dst) = *((uint8_t *)src);
        return;
    }
    if (size <= 2 * WORD_SZ)
    {
        *((uint16_t*)dst) = *((uint16_t*)src);
        *((uint16_t*)(dst + size - WORD_SZ)) =
                *((uint16_t*)(src + size - WORD_SZ));
        return;
    }
    if (size <= 2 * DWORD_SZ)
    {
        *((uint32_t*)dst) = *((uint32_t*)src);
        *((uint32_t*)(dst + size - DWORD_SZ)) =
                *((uint32_t*)(src + size - DWORD_SZ));
        return;
    }
    if (size <= 2 * QWORD_SZ)
    {
        *((uint64_t*)dst) = *((uint64_t*)src);
        *((uint64_t*)(dst + size - QWORD_SZ)) =
                *((uint64_t*)(src + size - QWORD_SZ));
        return;
    }
    x0 = _mm_loadu_si128((void*)src);
    _mm_storeu_si128((void*)dst, x0);
    x1 = _mm_loadu_si128((void*)(src + size - XMM_SZ));
    _mm_storeu_si128((void*)(dst + size - XMM_SZ), x1);
    return;
}

static inline void __strcpy_avx2(char *dst, const char *src)
{
    size_t offset = 0, index = 0;
    uint32_t ret = 0, ret1 = 0, ret2 = 0;
    __m256i y0, y1, y2, y3, y4, y5, y6, y_cmp;

    y0 = _mm256_setzero_si256();
    offset = (size_t)src & (YMM_SZ - 1);
    if (offset)
    {
        //check for offset falling in the last vec of the page
        if ((PAGE_SZ - YMM_SZ) < ((PAGE_SZ - 1) & (uintptr_t)src))
        {
            y1 = _mm256_load_si256((void *)src - offset);
            y_cmp = _mm256_cmpeq_epi8(y0, y1);

            ret = (uint32_t)_mm256_movemask_epi8(y_cmp) >> offset;
            if (ret)
            {
                index = _tzcnt_u32(ret) + 1;
                _strcpy_ble_ymm(dst, src, index);
                return;
            }
        }
        y1 = _mm256_loadu_si256((void *)src);
        y_cmp = _mm256_cmpeq_epi8(y0, y1);

        ret = _mm256_movemask_epi8(y_cmp);
        if (ret)
        {
            index =  _tzcnt_u32(ret) + 1;
            _strcpy_ble_ymm(dst, src, index);
            return;
        }
        _mm256_storeu_si256((void *)dst, y1);
        offset = YMM_SZ - offset;
    }

    y1 = _mm256_load_si256((void *)src + offset);
    y_cmp = _mm256_cmpeq_epi8(y0, y1);

    ret = _mm256_movemask_epi8(y_cmp);
    if (ret)
    {
        index = _tzcnt_u32(ret) + 1;
        _strcpy_ble_ymm(dst + offset, src + offset, index);
        return;
    }
    y2 = _mm256_load_si256((void *)src + YMM_SZ + offset);
    y_cmp = _mm256_cmpeq_epi8(y0, y2);
    ret = _mm256_movemask_epi8(y_cmp);

    if (ret != 0)
    {
        index = _tzcnt_u32(ret) + 1;
        y2 = _mm256_loadu_si256((void*)src + offset + index);
        _mm256_storeu_si256((void *)dst + offset, y1);
        _mm256_storeu_si256((void*)dst + offset + index, y2);
        return;
    }

    y3 = _mm256_load_si256((void *)src + 2 * YMM_SZ + offset);
    y_cmp = _mm256_cmpeq_epi8(y3, y0);
    ret = _mm256_movemask_epi8(y_cmp);
    if (ret != 0)
    {
        index = _tzcnt_u32(ret) + 1;
        y3 = _mm256_loadu_si256((void*)src + index + YMM_SZ + offset);
        _mm256_storeu_si256((void*)dst + offset, y1);
        _mm256_storeu_si256((void*)dst + YMM_SZ + offset, y2);
        _mm256_storeu_si256((void*)dst + YMM_SZ + index + offset, y3);
        return;
    }

    y4 = _mm256_load_si256((void *)src + 3 * YMM_SZ + offset);
    y_cmp = _mm256_cmpeq_epi8(y4, y0);
    ret = _mm256_movemask_epi8(y_cmp);
    if (ret != 0)
    {
        index = _tzcnt_u32(ret) + 1;
        y4 = _mm256_loadu_si256((void*)src + index + 2 * YMM_SZ + offset);
        _mm256_storeu_si256((void*)dst + offset, y1);
        _mm256_storeu_si256((void*)dst + YMM_SZ + offset, y2);
        _mm256_storeu_si256((void*)dst + 2 * YMM_SZ + offset, y3);
        _mm256_storeu_si256((void*)dst + 2 * YMM_SZ + index + offset, y4);
        return;
    }

    _mm256_storeu_si256((void*)dst + offset, y1);
    _mm256_storeu_si256((void*)dst + offset + YMM_SZ, y2);
    _mm256_storeu_si256((void*)dst + offset + 2 * YMM_SZ, y3);
    _mm256_storeu_si256((void*)dst + offset + 3 * YMM_SZ, y4);

    offset += 4 * YMM_SZ;

    while (1)
    {
        y1 = _mm256_load_si256((void *)src + offset);
        y2 = _mm256_load_si256((void *)src + offset + YMM_SZ);
        y3 = _mm256_load_si256((void *)src + offset + 2 * YMM_SZ);
        y4 = _mm256_load_si256((void *)src + offset + 3 * YMM_SZ);

        y5 = _mm256_min_epu8(y1, y2);
        y6 = _mm256_min_epu8(y3, y4);

        y_cmp = _mm256_cmpeq_epi8(_mm256_min_epu8(y5, y6), y0);
        ret = _mm256_movemask_epi8(y_cmp);

        if (ret)
            break;

        _mm256_storeu_si256((void*)dst + offset, y1);
        _mm256_storeu_si256((void*)dst + offset + YMM_SZ, y2);
        _mm256_storeu_si256((void*)dst + offset + 2 * YMM_SZ, y3);
        _mm256_storeu_si256((void*)dst + offset + 3 * YMM_SZ, y4);

        offset += 4 * YMM_SZ;
    }
    if (!ret)
    {
        offset = (offset & (-YMM_SZ)) - ((uintptr_t)(dst) & (YMM_SZ - 1));
        while(1)
        {
            y1 = _mm256_loadu_si256((void *)src + offset);
            y2 = _mm256_loadu_si256((void *)src + offset + YMM_SZ);
            y3 = _mm256_loadu_si256((void *)src + offset + 2 * YMM_SZ);
            y4 = _mm256_loadu_si256((void *)src + offset + 3 * YMM_SZ);

            y5 = _mm256_min_epu8(y1, y2);
            y6 = _mm256_min_epu8(y3, y4);

            y_cmp = _mm256_cmpeq_epi8(_mm256_min_epu8(y5, y6), y0);
            ret = _mm256_movemask_epi8(y_cmp);

            if (ret != 0)
                break;

            _mm256_store_si256((void*)dst + offset, y1);
            _mm256_store_si256((void*)dst + offset + YMM_SZ, y2);
            _mm256_store_si256((void*)dst + offset + 2 * YMM_SZ, y3);
            _mm256_store_si256((void*)dst + offset + 3 * YMM_SZ, y4);

            offset += 4 * YMM_SZ;
        }
    }
   //check for zero in regs: Y1, Y2
    if ((ret1 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y5, y0))))
    {
        if ((ret2 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y1, y0))))
        {
            index = _tzcnt_u32(ret2) + 1 + offset - YMM_SZ;
            y1 = _mm256_loadu_si256((void*)src + index);
            _mm256_storeu_si256((void*)dst + index, y1);
            return;
        }
        _mm256_storeu_si256((void*)dst + offset, y1);
        index = _tzcnt_u32(ret1) + 1 + offset;
        y1 = _mm256_loadu_si256((void*)src + index);
        _mm256_storeu_si256((void*)dst + index, y1);
        return;
    }
    //check for zero in regs: Y3, Y4
    else
    {
        _mm256_storeu_si256((void*)dst + offset, y1);
        _mm256_storeu_si256((void*)dst + offset + YMM_SZ, y2);
        if ((ret1 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y3, y0))))
        {
            index = _tzcnt_u32(ret1) + 1 + offset + YMM_SZ;
            y1 = _mm256_loadu_si256((void*)src + index);
            _mm256_storeu_si256((void*)dst + index, y1);
            return;
        }
        _mm256_storeu_si256((void*)dst + offset + 2 * YMM_SZ, y3);
        index = _tzcnt_u32(ret) + 1 + offset + 2 * YMM_SZ;
        y1 = _mm256_loadu_si256((void*)src + index);
        _mm256_storeu_si256((void*)dst + index, y1);
    }
    return;
}

static inline char *_strcat_avx2(char *dst, const char * src)
{
    size_t offset = __strlen_avx2(dst);
    __strcpy_avx2(dst + offset, src);
    return dst;
}

#ifdef AVX512_FEATURE_ENABLED
static inline size_t __strlen_avx512(const char *str)
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


static inline void __strcpy_avx512(char *dst, const char *src)
{
    size_t offset = 0, index = 0, offset_dst = 0;
    __m512i z0, z1, z2, z3, z4, z5, z6, z7;
    uint64_t ret = 0, ret1 = 0, ret2 = 0;
    __mmask64 mask, mask2;

    z0 = _mm512_setzero_epi32 ();
    offset = (uintptr_t)src & (ZMM_SZ - 1);
    if (offset)
    {
        if ((PAGE_SZ - ZMM_SZ) < ((PAGE_SZ - 1) & (uintptr_t)src))
        {
            z7 = _mm512_set1_epi8(0xff);
            mask = ((uint64_t)-1) >> offset;
            z1 =  _mm512_mask_loadu_epi8(z7 ,mask, src);
            ret =_mm512_cmpeq_epu8_mask(z0, z1);
            if (ret)
            {
                index =  _tzcnt_u64(ret);
                mask2 = ((uint64_t)-1) >> (63 - index);
                _mm512_mask_storeu_epi8(dst, mask2, z1);
                return;
            }
        }
        z1 = _mm512_loadu_si512(src);
        ret =_mm512_cmpeq_epu8_mask(z0, z1);
        if (ret)
        {
            index =  _tzcnt_u64(ret);
            mask = ((uint64_t)-1) >> (63 - index);
            _mm512_mask_storeu_epi8(dst, mask, z1);
            return;
        }
        _mm512_storeu_si512(dst, z1);
        offset = ZMM_SZ - offset;
    }
    z1 = _mm512_load_si512(src + offset);
    ret =_mm512_cmpeq_epu8_mask(z0, z1);
    if (ret != 0)
    {
        index =  _tzcnt_u64(ret);
        mask = ((uint64_t)-1) >> (63 - index);
        _mm512_mask_storeu_epi8(dst + offset, mask, z1);
        return;
    }
    z2 = _mm512_load_si512(src + ZMM_SZ + offset);
    ret = _mm512_cmpeq_epu8_mask(z2, z0);
    if (ret != 0)
    {
        index = _tzcnt_u64(ret) + 1;
        z2 = _mm512_loadu_si512(src + offset + index);
        _mm512_storeu_si512(dst + offset, z1);
        _mm512_storeu_si512(dst + offset + index, z2);
        return;
    }
    z3 = _mm512_load_si512(src + 2 * ZMM_SZ + offset);
    ret1 = _mm512_cmpeq_epu8_mask(z3, z0);
    if (ret1)
    {
        index = _tzcnt_u64(ret1) + 1;
        z3 = _mm512_loadu_si512(src + ZMM_SZ + offset + index);
        _mm512_storeu_si512(dst + offset, z1);
        _mm512_storeu_si512(dst + ZMM_SZ + offset, z2);
        _mm512_storeu_si512(dst + ZMM_SZ + offset + index, z3);
        return;
    }
    z4 = _mm512_load_si512(src + 3 * ZMM_SZ + offset);
    ret = _mm512_cmpeq_epu8_mask(z4, z0);
    if (ret)
    {
        index = _tzcnt_u64(ret) + 1;
        z4 = _mm512_loadu_si512(src + 2 * ZMM_SZ + offset + index);
        _mm512_storeu_si512(dst + offset, z1);
        _mm512_storeu_si512(dst + ZMM_SZ + offset, z2);
        _mm512_storeu_si512(dst + 2 * ZMM_SZ + offset, z3);
        _mm512_storeu_si512(dst + 2 * ZMM_SZ + offset + index, z4);
        return;
    }
    _mm512_storeu_si512(dst + offset, z1);
    _mm512_storeu_si512(dst + offset + ZMM_SZ, z2);
    _mm512_storeu_si512(dst + offset + 2 * ZMM_SZ, z3);
    _mm512_storeu_si512(dst + offset + 3 * ZMM_SZ, z4);
    offset += 4*ZMM_SZ;

    offset_dst = (((uintptr_t)dst + offset) & (ZMM_SZ - 1));
    offset -= offset_dst;
    do
    {
        if ((PAGE_SZ - 4 * ZMM_SZ) >= ((PAGE_SZ - 1) & ((uintptr_t)src + offset)))
        {
            z1 = _mm512_loadu_si512(src + offset);
            ret =_mm512_cmpeq_epu8_mask(z0, z1);
            if (ret)
            {
                index =  _tzcnt_u64(ret);
                mask = ((uint64_t)-1) >> (63 - index);
                _mm512_mask_storeu_epi8(dst + offset, mask, z1);
                return;
            }
            z2 = _mm512_loadu_si512(src + ZMM_SZ + offset);
            ret = _mm512_cmpeq_epu8_mask(z2, z0);
            if (ret)
            {
                index = _tzcnt_u64(ret) + 1;
                z2 = _mm512_loadu_si512(src + offset + index);
                _mm512_store_si512(dst + offset, z1);
                _mm512_storeu_si512(dst + offset + index, z2);
                return;
            }
            z3 = _mm512_loadu_si512(src + 2 * ZMM_SZ + offset);
            ret = _mm512_cmpeq_epu8_mask(z3, z0);
            if (ret)
            {
                index = _tzcnt_u64(ret) + 1;
                z3 = _mm512_loadu_si512(src + ZMM_SZ + offset + index);
                _mm512_store_si512(dst + offset, z1);
                _mm512_store_si512(dst + ZMM_SZ + offset, z2);
                _mm512_storeu_si512(dst + ZMM_SZ + offset + index, z3);
                return;
            }
            z4 = _mm512_loadu_si512(src + 3 * ZMM_SZ + offset);
            ret = _mm512_cmpeq_epu8_mask(z4, z0);
            if (ret)
            {
                index = _tzcnt_u64(ret) + 1;
                z4 = _mm512_loadu_si512(src + 2 * ZMM_SZ + offset + index);
                _mm512_store_si512(dst + offset, z1);
                _mm512_store_si512(dst + ZMM_SZ + offset, z2);
                _mm512_store_si512(dst + 2 * ZMM_SZ + offset, z3);
                _mm512_storeu_si512(dst + 2 * ZMM_SZ + offset + index, z4);
                return;
            }
            _mm512_store_si512(dst + offset, z1);
            _mm512_store_si512(dst + offset + ZMM_SZ, z2);
            _mm512_store_si512(dst + offset + 2 * ZMM_SZ, z3);
            _mm512_store_si512(dst + offset + 3 * ZMM_SZ, z4);
            offset += 4 * ZMM_SZ;
            if (offset > (PAGE_SZ - 4 * ZMM_SZ))
            offset = (offset & (-ZMM_SZ)) - ((uintptr_t)(dst) & (ZMM_SZ - 1));
        }

        z1 = _mm512_loadu_si512(src + offset);
        z2 = _mm512_loadu_si512(src + offset + ZMM_SZ);
        z3 = _mm512_loadu_si512(src + offset + 2 * ZMM_SZ);
        z4 = _mm512_loadu_si512(src + offset + 3 * ZMM_SZ);

        z5 = _mm512_min_epu8(z1,z2);
        z6 = _mm512_min_epu8(z3,z4);

        ret = _mm512_cmpeq_epu8_mask(_mm512_min_epu8(z5, z6), z0);
        if (ret != 0)
          break;

        _mm512_store_si512(dst + offset, z1);
        _mm512_store_si512(dst + offset + ZMM_SZ, z2);
        _mm512_store_si512(dst + offset + 2 * ZMM_SZ, z3);
        _mm512_store_si512(dst + offset + 3 * ZMM_SZ, z4);

        offset += 4 * ZMM_SZ;
    } while(1);

    //check for zero in regs: Z1, Z2
    if ((ret1 = _mm512_cmpeq_epu8_mask(z5, z0)))
    {
        if ((ret2 = _mm512_cmpeq_epu8_mask(z1, z0)))
        {
            index = _tzcnt_u64(ret2);
            mask = ((uint64_t)-1) >> (63 - index);
            _mm512_mask_storeu_epi8(dst + offset, mask, z1);
            return;
        }
        index = _tzcnt_u64(ret1);
        mask = ((uint64_t)-1) >> (63 - index);
        _mm512_storeu_si512(dst + offset, z1);
        _mm512_mask_storeu_epi8(dst + offset + ZMM_SZ, mask, z2);
        return;
    }
    //check for zero in regs: Z3, Z4
    else
    {
        _mm512_storeu_si512(dst + offset, z1);
        _mm512_storeu_si512(dst + offset + ZMM_SZ, z2);
        if ((ret2 =_mm512_cmpeq_epu8_mask(z3, z0)))
        {
            index = _tzcnt_u64(ret2);
            mask = ((uint64_t)-1) >> (63 - index);
            _mm512_mask_storeu_epi8(dst + offset + 2 * ZMM_SZ, mask, z3);
            return;
        }
        index = _tzcnt_u64(ret);
        mask = ((uint64_t)-1) >> (63 - index);
        _mm512_storeu_si512(dst + offset + 2 * ZMM_SZ, z3);
        _mm512_mask_storeu_epi8(dst + offset + 3 * ZMM_SZ, mask, z4);
    }
    return;
}

static inline char *_strcat_avx512(char *dst, const char *src)
{
    size_t offset = __strlen_avx512(dst);
    __strcpy_avx512(dst + offset, src);
    return dst;
}
#endif

char * __attribute__((flatten)) amd_strcat(char * __restrict dst,
                                           const char * __restrict src)
{
    LOG_INFO("\n");
#ifdef AVX512_FEATURE_ENABLED
    return _strcat_avx512(dst, src);
#else
    return _strcat_avx2(dst, src);
#endif
}

char *strcat(char *, const char *) __attribute__((weak, alias("amd_strcat"),
                                                        visibility("default")));
