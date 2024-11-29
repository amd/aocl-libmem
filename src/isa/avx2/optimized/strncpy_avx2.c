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
#include "zen_cpu_info.h"
#include "alm_defs.h"

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

static inline char * __attribute__((flatten)) _strncpy_avx2(char *dst, const char *src, size_t size)
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

    y5 = _mm256_min_epu8(y1, y2);
    y6 = _mm256_min_epu8(y3, y4);

    y_cmp = _mm256_cmpeq_epi8(_mm256_min_epu8(y5, y6), y0);
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

                y5 = _mm256_min_epu8(y1, y2);
                y6 = _mm256_min_epu8(y3, y4);

                y_cmp = _mm256_cmpeq_epi8(_mm256_min_epu8(y5, y6), y0);
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

            y5 = _mm256_min_epu8(y1, y2);
            y6 = _mm256_min_epu8(y3, y4);

            y_cmp = _mm256_cmpeq_epi8(_mm256_min_epu8(y5, y6), y0);
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
