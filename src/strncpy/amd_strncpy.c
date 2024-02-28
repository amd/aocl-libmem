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
#include "../base_impls/memset_erms_impls.h"
#include "zen_cpu_info.h"

#define PAGE_SZ         4096
#define CACHELINE_SZ    64

extern cpu_info zen_info;

static inline void *_fill_null_avx2(void * mem, size_t size)
{
    __m256i y0;
    __m128i x0;
    size_t offset = 0;

    if (size <= 2 * YMM_SZ)
    {
    if (size >= XMM_SZ)
    {
        if (size >= YMM_SZ)
        {
            y0 = _mm256_set1_epi8(0);
            _mm256_storeu_si256(mem, y0);
            _mm256_storeu_si256(mem + size - YMM_SZ, y0);
            return mem;
        }
        x0 = _mm_set1_epi8(0);
        _mm_storeu_si128(mem, x0);
        _mm_storeu_si128(mem + size - XMM_SZ, x0);
        return mem;
    }
    if (size > QWORD_SZ)
    {
        *((uint64_t*)mem) = 0;
        *((uint64_t*)(mem + size - QWORD_SZ)) = 0;
        return mem;
    }
    if (size > DWORD_SZ)
    {
        *((uint32_t*)mem) = 0;
        *((uint32_t*)(mem + size - 4)) = 0;
        return mem;
    }
    if (size >= WORD_SZ)
    {
        *((uint16_t*)mem) = 0;
        *((uint16_t*)(mem + size - 2)) = 0;
        return mem;
    }
    *((uint8_t*)mem) = 0;
    return mem;
    }
    y0 = _mm256_set1_epi8(0);
    if (size <= 4 * YMM_SZ)
    {
        _mm256_storeu_si256(mem, y0);
        _mm256_storeu_si256(mem + YMM_SZ, y0);
        _mm256_storeu_si256(mem + size - 2 * YMM_SZ, y0);
        _mm256_storeu_si256(mem + size - YMM_SZ, y0);
        return mem;
    }
    _mm256_storeu_si256(mem + 0 * YMM_SZ, y0);
    _mm256_storeu_si256(mem + 1 * YMM_SZ, y0);
    _mm256_storeu_si256(mem + 2 * YMM_SZ, y0);
    _mm256_storeu_si256(mem + 3 * YMM_SZ, y0);
    _mm256_storeu_si256(mem + size - 4 * YMM_SZ, y0);
    _mm256_storeu_si256(mem + size - 3 * YMM_SZ, y0);
    _mm256_storeu_si256(mem + size - 2 * YMM_SZ, y0);
    _mm256_storeu_si256(mem + size - 1 * YMM_SZ, y0);

    offset += 4 * YMM_SZ;
    size -= 4 * YMM_SZ;
    offset -= ((size_t)mem & (YMM_SZ - 1));
    while( offset < size )
    {
        _mm256_store_si256(mem + offset + 0 * YMM_SZ, y0);
        _mm256_store_si256(mem + offset + 1 * YMM_SZ, y0);
        _mm256_store_si256(mem + offset + 2 * YMM_SZ, y0);
        _mm256_store_si256(mem + offset + 3 * YMM_SZ, y0);
        offset += 4 * YMM_SZ;
    }
    return mem;
}

static inline char * strncpy_ble_ymm(void *dst, const void *src, uint32_t size)
{
    __m128i x0, x1, x2;
    __m256i y0, y1, y2;
    uint64_t lo_val = 0, hi_val = 0;
    uint64_t lo_idx = 0, hi_idx = 0;

    if (size == 1)
    {
        *((uint8_t *)dst) = *((uint8_t *)src);
        return dst;
    }
    if (size <= 2 * WORD_SZ)
    {
        lo_val = *((uint16_t *)dst) = *((uint16_t*)src);
        lo_idx = (!(lo_val & (0xff))) | (!(lo_val & (0xff00)))<<1;
        hi_val = *((uint16_t*)(dst + size - WORD_SZ)) =
                        *((uint16_t*)(src + size - WORD_SZ));
        lo_idx |= !(hi_val & (0xff))<<(size - 1);
        lo_idx = _tzcnt_u32(lo_idx) + 1;
        if (lo_idx < size)
        {
            _fill_null_avx2(dst + lo_idx, size - lo_idx);
        }
        return dst;
    }
    if (size <= 2 * DWORD_SZ)
    {
        lo_val = *((uint32_t*)dst) = *((uint32_t*)src);
        hi_val = *((uint32_t*)(dst + size - DWORD_SZ)) =
                *((uint32_t*)(src + size - DWORD_SZ));

        lo_idx = (lo_val - 0x01010101UL) & ~(lo_val) & 0x80808080UL;
        lo_idx |= ((hi_val - 0x010101UL) & ~(hi_val) & 0x808080UL) << (size - DWORD_SZ)*8;

        if (lo_idx)
        {
            lo_idx = (_tzcnt_u64(lo_idx)>>3) + 1;
            _fill_null_avx2(dst + lo_idx, size - lo_idx);
        }
        return dst;
    }
    if (size <= 2 * QWORD_SZ)
    {
        lo_val = *((uint64_t*)dst) = *((uint64_t*)src);
        hi_val = *((uint64_t*)(dst + size - QWORD_SZ)) =
                *((uint64_t*)(src + size - QWORD_SZ));

        lo_idx = (lo_val - 0x0101010101010101UL) & ~(lo_val) & 0x8080808080808080UL;
        if (lo_idx)
        {
            lo_idx = (_tzcnt_u64(lo_idx)>>3) + 1;
            _fill_null_avx2(dst + lo_idx, size - lo_idx);
            return dst;
        }
        hi_idx = ((hi_val - 0x01010101010101UL) & ~(hi_val) & 0x80808080808080UL);
        if (hi_idx)
        {
            hi_idx = (_tzcnt_u64(hi_idx)>>3) + (size - QWORD_SZ) + 1;
            _fill_null_avx2(dst + hi_idx, size - hi_idx);
        }
        return dst;
    }
    if (size <= 2 * XMM_SZ)
    {
        x1 = _mm_loadu_si128((void*)src);
        _mm_storeu_si128((void*)dst, x1);
        x2 = _mm_loadu_si128((void*)(src + size - XMM_SZ));
        _mm_storeu_si128((void*)(dst + size - XMM_SZ), x2);

        x0 = _mm_setzero_si128();
        lo_idx = _mm_movemask_epi8(_mm_cmpeq_epi8(x0, x1));

        if (!lo_idx)
        {
            hi_idx = _mm_movemask_epi8(_mm_cmpeq_epi8(x0, x2));
            lo_idx = size - XMM_SZ + __tzcnt_u16(hi_idx) + 1;
        }
        else
        {
            lo_idx = __tzcnt_u16(lo_idx) + 1;
        }
        if (lo_idx < size)
        {
            _fill_null_avx2(dst + lo_idx, size - lo_idx);
        }

        return dst;
    }
    if (size <= 2 * YMM_SZ)
    {
        y1 = _mm256_loadu_si256((void*)src);
        _mm256_storeu_si256((void*)dst, y1);
        y2 = _mm256_loadu_si256((void*)(src + size - YMM_SZ));
        _mm256_storeu_si256((void*)(dst + size - YMM_SZ), y2);

        y0 = _mm256_setzero_si256();
        lo_idx = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y0, y1));
        hi_idx=  _mm256_movemask_epi8(_mm256_cmpeq_epi8(y0, y2));

        lo_idx = _tzcnt_u64(lo_idx | (hi_idx <<(size - YMM_SZ))) + 1;
        if (lo_idx < size)
        {
            _fill_null_avx2(dst + lo_idx, size - lo_idx);
        }
    }

    return dst;
}

static inline char *_strncpy_avx2(char *dst, const char *src, size_t size)
{
    size_t offset = 0;
    uint32_t ret = 0, ret1 = 0, ret2 = 0;
    uint64_t lo_idx = 0, hi_idx = 0, idx = 0;
    __m256i y0, y1, y2, y3, y4, y5, y6, y_cmp;

    if (size <= 2 * YMM_SZ)
    {
        if (size)
            return strncpy_ble_ymm(dst, src, size);
        else
            return dst;
    }
    y0 = _mm256_setzero_si256();

    if (size <= 4 * YMM_SZ)
    {
        y1 = _mm256_loadu_si256((void *)src);
        y2 = _mm256_loadu_si256((void *)src + YMM_SZ);
        y3 = _mm256_loadu_si256((void *)src + size - 2 * YMM_SZ);
        y4 = _mm256_loadu_si256((void *)src + size - YMM_SZ);

       _mm256_storeu_si256((void*)dst, y1);
       _mm256_storeu_si256((void*)dst + YMM_SZ, y2);
       _mm256_storeu_si256((void*)dst + size - 2 * YMM_SZ, y3);
       _mm256_storeu_si256((void*)dst + size - YMM_SZ, y4);

        lo_idx = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y0, y1));
        hi_idx = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y0, y2));
        lo_idx = lo_idx | (hi_idx << YMM_SZ);
        idx += _tzcnt_u64(lo_idx);
        if (!lo_idx)
        {
            lo_idx =  _mm256_movemask_epi8(_mm256_cmpeq_epi8(y0, y3));
            hi_idx =  _mm256_movemask_epi8(_mm256_cmpeq_epi8(y0, y4));
            hi_idx = lo_idx | (hi_idx << YMM_SZ);
            idx += _tzcnt_u64(hi_idx >> (4*YMM_SZ - size)) + 1;
        }

        if (idx < size)
        {
            _fill_null_avx2(dst + idx, size - idx);
        }
        return dst;
    }

    y1 = _mm256_loadu_si256((void *)src + offset);
    y2 = _mm256_loadu_si256((void *)src + offset + YMM_SZ);
    y3 = _mm256_loadu_si256((void *)src + offset + 2 * YMM_SZ);
    y4 = _mm256_loadu_si256((void *)src + offset + 3 * YMM_SZ);

    y5 = _mm256_min_epi8(y1, y2);
    y6 = _mm256_min_epi8(y3, y4);

    y_cmp = _mm256_cmpeq_epi8(_mm256_min_epi8(y5, y6), y0);
    ret = _mm256_movemask_epi8(y_cmp);

    if (!ret)
    {
        _mm256_storeu_si256((void*)dst + offset, y1);
        _mm256_storeu_si256((void*)dst + offset + YMM_SZ, y2);
        _mm256_storeu_si256((void*)dst + offset + 2 * YMM_SZ, y3);
        _mm256_storeu_si256((void*)dst + offset + 3 * YMM_SZ, y4);

        if (size > 8*YMM_SZ)
        {
            offset = 4 * YMM_SZ - ((uintptr_t)dst & (YMM_SZ - 1));
            while ((size - offset) >= 4*YMM_SZ)
            {
                y1 = _mm256_loadu_si256((void *)src + offset);
                y2 = _mm256_loadu_si256((void *)src + offset + YMM_SZ);
                y3 = _mm256_loadu_si256((void *)src + offset + 2 * YMM_SZ);
                y4 = _mm256_loadu_si256((void *)src + offset + 3 * YMM_SZ);

                y5 = _mm256_min_epi8(y1, y2);
                y6 = _mm256_min_epi8(y3, y4);

                y_cmp = _mm256_cmpeq_epi8(_mm256_min_epi8(y5, y6), y0);
                ret = _mm256_movemask_epi8(y_cmp);
                if (ret)
                    break;
                _mm256_store_si256((void*)dst + offset, y1);
                _mm256_store_si256((void*)dst + offset + YMM_SZ, y2);
                _mm256_store_si256((void*)dst + offset + 2 * YMM_SZ, y3);
                _mm256_store_si256((void*)dst + offset + 3 * YMM_SZ, y4);

                offset += 4 * YMM_SZ;
            }
        }
        if (!ret)
        {
            offset = size - 4*YMM_SZ;
            y1 = _mm256_loadu_si256((void *)src + offset);
            y2 = _mm256_loadu_si256((void *)src + offset + YMM_SZ);
            y3 = _mm256_loadu_si256((void *)src + offset + 2 * YMM_SZ);
            y4 = _mm256_loadu_si256((void *)src + offset + 3 * YMM_SZ);

            y5 = _mm256_min_epi8(y1, y2);
            y6 = _mm256_min_epi8(y3, y4);

            y_cmp = _mm256_cmpeq_epi8(_mm256_min_epi8(y5, y6), y0);
            ret = _mm256_movemask_epi8(y_cmp);
            if (ret == 0)
            {
                _mm256_storeu_si256((void*)dst + offset, y1);
                _mm256_storeu_si256((void*)dst + offset + YMM_SZ, y2);
                _mm256_storeu_si256((void*)dst + offset + 2 * YMM_SZ, y3);
                _mm256_storeu_si256((void*)dst + offset + 3 * YMM_SZ, y4);

                return dst;
            }
        }
    }

    if ((ret2 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y5, y0))))
    {
        _mm256_storeu_si256((void*)dst + offset, y1);
        if (!(ret1 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y1, y0))))
        {
            _mm256_storeu_si256((void*)dst + offset + YMM_SZ, y2);
            idx += YMM_SZ;
            ret = ret2;
        }
        else
            ret = ret1;
    }
    else
    {
        _mm256_storeu_si256((void*)dst + offset, y1);
        _mm256_storeu_si256((void*)dst + offset + YMM_SZ, y2);
        _mm256_storeu_si256((void*)dst + offset + 2 * YMM_SZ, y3);

        idx += 2 * YMM_SZ;
        if (!(ret1 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y3, y0))))
        {
                _mm256_storeu_si256((void*)dst + offset + 3 * YMM_SZ, y4);
            idx += YMM_SZ;
        }
        else
            ret = ret1;
    }
    idx += offset + _tzcnt_u32(ret) + 1;

    if (idx < size)
        _fill_null_avx2(dst + idx , size - idx);

    return dst;
}

#ifdef AVX512_FEATURE_ENABLED
static inline void *_fill_null_avx512(void *mem, size_t size)
{
    __m512i z0;
    __m256i y0;
    size_t offset = 0;

    if (size < 2 * ZMM_SZ)
    {
        return __erms_stosb(mem, 0, size);
    }
    z0 = _mm512_set1_epi8(0);

    if (size <= 4 * ZMM_SZ)
    {
        _mm512_storeu_si512(mem , z0);
        _mm512_storeu_si512(mem + ZMM_SZ, z0);
        _mm512_storeu_si512(mem + size - 2 * ZMM_SZ, z0);
        _mm512_storeu_si512(mem + size - ZMM_SZ, z0);
        return mem;
    }
    _mm512_storeu_si512(mem + 0 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + 1 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + 2 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + 3 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + size - 4 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + size - 3 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + size - 2 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + size - 1 * ZMM_SZ, z0);


    if (size <= 8 * ZMM_SZ)
        return mem;

    offset += 4 * ZMM_SZ;
    size -= 4 * ZMM_SZ;
    offset -= ((uint64_t)mem & (ZMM_SZ-1));

    if (size < zen_info.zen_cache_info.l2_per_core)//L2 Cache Size
    {
        y0 = _mm256_set1_epi8(0);
        while( offset < size )
        {
        _mm256_store_si256(mem + offset + 0 * YMM_SZ, y0);
        _mm256_store_si256(mem + offset + 1 * YMM_SZ, y0);
        _mm256_store_si256(mem + offset + 2 * YMM_SZ, y0);
        _mm256_store_si256(mem + offset + 3 * YMM_SZ, y0);
        _mm256_store_si256(mem + offset + 4 * YMM_SZ, y0);
        _mm256_store_si256(mem + offset + 5 * YMM_SZ, y0);
        _mm256_store_si256(mem + offset + 6 * YMM_SZ, y0);
        _mm256_store_si256(mem + offset + 7 * YMM_SZ, y0);
        offset += 4 * ZMM_SZ;
        }
        return mem;
    }
    while( offset < size )
    {
        _mm512_store_si512(mem + offset + 0 * ZMM_SZ, z0);
        _mm512_store_si512(mem + offset + 1 * ZMM_SZ, z0);
        _mm512_store_si512(mem + offset + 2 * ZMM_SZ, z0);
        _mm512_store_si512(mem + offset + 3 * ZMM_SZ, z0);
        offset += 4 * ZMM_SZ;
    }

    return mem;
}


static inline char *_strncpy_avx512(char *dst, const char *src, size_t size)
{
    size_t offset = 0, index = 0;
    __m512i z0, z1, z2, z3, z4, z5, z6;
    uint64_t ret = 0, ret1 = 0, ret2 = 0;
    __mmask64 mask, mask2;

    z0 = _mm512_set1_epi8(0);

    if (size <= ZMM_SZ)
    {
        if (size == 0)
            return dst;

        mask = ((uint64_t)-1) >> (ZMM_SZ - size);
        z1 =  _mm512_mask_loadu_epi8(z0 ,mask, src);
        _mm512_mask_storeu_epi8(dst, mask, z1);
        ret = _mm512_cmpeq_epu8_mask(z0, z1) << 1;
        if (ret != 0)
        {
            index =  _tzcnt_u64(ret);
            mask2 = ((uint64_t)-1) <<  index;
            _mm512_mask_storeu_epi8(dst, mask2 & mask, z0);
        }
        return dst;
    }

    if (size <= 2 * ZMM_SZ)
    {
        z1 = _mm512_loadu_si512(src);
        _mm512_storeu_si512(dst, z1);

        ret = _mm512_cmpeq_epu8_mask(z0, z1);
        if (ret == 0)
        {
            offset = size - ZMM_SZ;
            z2 = _mm512_loadu_si512(src + offset);
            _mm512_storeu_si512(dst + offset, z2);
            ret = _mm512_cmpeq_epu8_mask(z0, z2) << 1;
            if (ret == 0)
                return dst;
        }
        index =  _tzcnt_u64(ret);
        if (index < ZMM_SZ)
            _fill_null_avx512(dst + index + offset, size - (offset + index));
        return dst;
    }

    if (size <= 4 * ZMM_SZ)
    {
        z1 = _mm512_loadu_si512(src);
        z2 = _mm512_loadu_si512(src + ZMM_SZ);
        z3 = _mm512_loadu_si512(src + size - 2 * ZMM_SZ);
        z4 = _mm512_loadu_si512(src + size - ZMM_SZ);

        _mm512_storeu_si512(dst, z1);
        _mm512_storeu_si512(dst + ZMM_SZ, z2);
        _mm512_storeu_si512(dst + size - 2 * ZMM_SZ, z3);
        _mm512_storeu_si512(dst + size - ZMM_SZ, z4);

        if ((ret1 = _mm512_cmpeq_epu8_mask(z0, _mm512_min_epi8(z1,z2))))
        {
            ret = _mm512_cmpeq_epu8_mask(z0, z1);
            if (ret == 0)
            {
                index = ZMM_SZ;
                ret = ret1;
            }
            index +=  _tzcnt_u64(ret);
            _fill_null_avx512(dst + index, size - index);
        }
        else if ((ret2 = _mm512_cmpeq_epu8_mask(z0, _mm512_min_epi8(z3,z4))))
        {
            index = size - 2 * ZMM_SZ;
            ret = _mm512_cmpeq_epu8_mask(z0, z3);
            if (ret == 0)
            {
                // last byte null case
                if ((ret2 << 1) == 0)
                     return dst;
                index = size - ZMM_SZ;
                ret = ret2;
            }
            index +=  _tzcnt_u64(ret);
            _fill_null_avx512(dst + index, size - index);
        }
        return dst;
    }

    z1 = _mm512_loadu_si512((void *)src + offset);
    z2 = _mm512_loadu_si512((void *)src + offset + ZMM_SZ);
    z3 = _mm512_loadu_si512((void *)src + offset + 2 * ZMM_SZ);
    z4 = _mm512_loadu_si512((void *)src + offset + 3 * ZMM_SZ);

    z5 = _mm512_min_epi8(z1, z2);
    z6 = _mm512_min_epi8(z3, z4);

    ret = _mm512_cmpeq_epu8_mask(_mm512_min_epi8(z5, z6), z0);

    if (!ret)
    {
        _mm512_storeu_si512((void*)dst + offset, z1);
        _mm512_storeu_si512((void*)dst + offset + ZMM_SZ, z2);
        _mm512_storeu_si512((void*)dst + offset + 2 * ZMM_SZ, z3);
        _mm512_storeu_si512((void*)dst + offset + 3 * ZMM_SZ, z4);

        if (size > 8*ZMM_SZ)
        {
            offset = 4 * ZMM_SZ - ((uintptr_t)dst & (ZMM_SZ - 1));
            while ((size - offset) >= 4*ZMM_SZ)
            {
                z1 = _mm512_loadu_si512((void *)src + offset);
                z2 = _mm512_loadu_si512((void *)src + offset + ZMM_SZ);
                z3 = _mm512_loadu_si512((void *)src + offset + 2 * ZMM_SZ);
                z4 = _mm512_loadu_si512((void *)src + offset + 3 * ZMM_SZ);

                z5 = _mm512_min_epi8(z1, z2);
                z6 = _mm512_min_epi8(z3, z4);

                ret = _mm512_cmpeq_epu8_mask(_mm512_min_epi8(z5, z6), z0);
                if (ret)
                    break;
                _mm512_store_si512((void*)dst + offset, z1);
                _mm512_store_si512((void*)dst + offset + ZMM_SZ, z2);
                _mm512_store_si512((void*)dst + offset + 2 * ZMM_SZ, z3);
                _mm512_store_si512((void*)dst + offset + 3 * ZMM_SZ, z4);

                offset += 4 * ZMM_SZ;
            }
        }
        if (!ret)
        {
            offset = size - 4*ZMM_SZ;
            z1 = _mm512_loadu_si512((void *)src + offset);
            z2 = _mm512_loadu_si512((void *)src + offset + ZMM_SZ);
            z3 = _mm512_loadu_si512((void *)src + offset + 2 * ZMM_SZ);
            z4 = _mm512_loadu_si512((void *)src + offset + 3 * ZMM_SZ);

            z5 = _mm512_min_epi8(z1, z2);
            z6 = _mm512_min_epi8(z3, z4);

            ret = _mm512_cmpeq_epu8_mask(_mm512_min_epi8(z5, z6), z0);
            if (ret == 0)
            {
                _mm512_storeu_si512((void*)dst + offset, z1);
                _mm512_storeu_si512((void*)dst + offset + ZMM_SZ, z2);
                _mm512_storeu_si512((void*)dst + offset + 2 * ZMM_SZ, z3);
                _mm512_storeu_si512((void*)dst + offset + 3 * ZMM_SZ, z4);

                return dst;
            }
        }
    }

    if ((ret2 = _mm512_cmpeq_epu8_mask(z5, z0)))
    {
        _mm512_storeu_si512((void*)dst + offset, z1);
        if (!(ret1 = _mm512_cmpeq_epu8_mask(z1, z0)))
        {
            _mm512_storeu_si512((void*)dst + offset + ZMM_SZ, z2);
            index += ZMM_SZ;
            ret = ret2;
        }
        else
            ret = ret1;
    }
    else
    {
        _mm512_storeu_si512((void*)dst + offset, z1);
        _mm512_storeu_si512((void*)dst + offset + ZMM_SZ, z2);
        _mm512_storeu_si512((void*)dst + offset + 2 * ZMM_SZ, z3);

        index += 2 * ZMM_SZ;
        if (!(ret1 = _mm512_cmpeq_epu8_mask(z3, z0)))
        {
                _mm512_storeu_si512((void*)dst + offset + 3 * ZMM_SZ, z4);
            index += ZMM_SZ;
        }
        else
            ret = ret1;
    }
    index += offset + _tzcnt_u64(ret) + 1;

    if (index < size)
        _fill_null_avx2(dst + index , size - index);

    return dst;
}
#endif

char * __attribute__((flatten)) amd_strncpy(char * __restrict dst,
                                 const char * __restrict src, size_t size)
{
    LOG_INFO("\n");

#ifdef AVX512_FEATURE_ENABLED
    return _strncpy_avx512(dst, src, size);
#else
    return _strncpy_avx2(dst, src, size);
#endif
}

char *strncpy(char *, const char *, size_t) __attribute__((weak, alias("amd_strncpy"),
                                                        visibility("default")));
