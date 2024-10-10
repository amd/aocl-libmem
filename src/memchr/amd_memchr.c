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

#include <immintrin.h>
#include "logger.h"
#include "threshold.h"
#include "zen_cpu_info.h"
#include "alm_defs.h"

extern cpu_info zen_info;

static inline void *memchr_le_2ymm(void *mem, int val, uint8_t size)
{
    __m256i y0, y1, y2, y3, y4;
    __m128i x0, x1, x2, x3, x4;
    uint32_t ret, ret1, index = 0;

    if (likely(size >= QWORD_SZ))
    {
        if (size > XMM_SZ)
        {
            if (likely(size >= YMM_SZ))
            {
                y0 = _mm256_set1_epi8((char)val);
                y1 = _mm256_loadu_si256(mem);
                y2 = _mm256_loadu_si256(mem + size - YMM_SZ);
                y3 = _mm256_cmpeq_epi8(y0, y1);
                y4 = _mm256_cmpeq_epi8(y0, y2);
                ret = _mm256_movemask_epi8(_mm256_or_si256(y3, y4));

                if (ret)
                {
                    ret1 = _mm256_movemask_epi8(y3);
                    if (ret1)
                        return mem + _tzcnt_u32(ret1);
                    else
                        return mem + size - YMM_SZ +  _tzcnt_u32(ret);
                }
                return NULL;
            }

            x0 = _mm_set1_epi8((char)val);
            x1 = _mm_loadu_si128(mem);
            x2 = _mm_loadu_si128(mem + size - XMM_SZ);
            x3 = _mm_cmpeq_epi8(x0, x1);
            x4 = _mm_cmpeq_epi8(x0, x2);
            ret = _mm_movemask_epi8(_mm_or_si128(x3, x4));

            if (ret)
            {
                ret1 = _mm_movemask_epi8(x3);

                if (ret1)
                    return mem + _tzcnt_u32(ret1);
                else
                    return mem + size - XMM_SZ +  _tzcnt_u32(ret);
            }
            return NULL;
        }

        x0 = _mm_set1_epi8((char)val);
        x1 = _mm_loadu_si64(mem);
        x2 = _mm_loadu_si64(mem + size - QWORD_SZ);
        x3 = _mm_cmpeq_epi8(x0, x1);
        x4 = _mm_cmpeq_epi8(x0, x2);
        ret = _mm_movemask_epi8(_mm_or_si128(x3, x4)) & 0xffff;

        if (ret)
        {
            ret1 = _mm_movemask_epi8(x3) & 0xffff;

            if (ret1)
                return mem + _tzcnt_u32(ret1);
            else
                return mem + size - QWORD_SZ +  _tzcnt_u32(ret);
        }
        return NULL;
    }

    if (size < 2 * DWORD_SZ)
    {
        if (size < DWORD_SZ)
        {
            uint8_t cmp = 0;
            cmp = (*(uint8_t*)(mem))^((uint8_t)val);
            if (cmp == 0)
                return mem;
            if (size > 1)
            {
                cmp = (*(uint8_t*)(mem + 1))^((uint8_t)val);
                if (cmp == 0)
                    return mem + 1;
                if (size > 2)
                {
                    cmp = (*(uint8_t*)(mem + 2))^((uint8_t)val);
                    if (cmp == 0)
                        return mem + 2;
                }
            }
            return NULL;
        }

        x0 = _mm_set1_epi8((char)val);
        x1 = _mm_loadu_si32(mem);
        x3 = _mm_cmpeq_epi8(x0, x1);
        ret = _mm_movemask_epi8(x3);

        if (ret > 16 || ret == 0)
        {
            index = size - DWORD_SZ;
            x1 = _mm_loadu_si32(mem + index);
            x3 = _mm_cmpeq_epi8(x0, x1);
            ret = _mm_movemask_epi8(x3);
            if (ret > 16 || ret == 0)
                return NULL;
        }
        index += _tzcnt_u32(ret);
        return (uint8_t*)(mem + index);
    }
    return NULL;
}

static inline void *_memchr_avx2(void *mem, int val, size_t size)
{
    __m256i y0, y1, y2, y3, y4, y5, y6, y7, y8;
    size_t index = 0, offset = 0;
    int ret, ret1, ret2, ret3;

    if (likely(size <= 2 * YMM_SZ))
        return memchr_le_2ymm(mem, val, (uint8_t) size);

    y0 = _mm256_set1_epi8((char)val);
    if (size <= 4 * YMM_SZ)
    {
        y1 = _mm256_loadu_si256(mem);
        y2 = _mm256_loadu_si256(mem + YMM_SZ);
        y5 = _mm256_loadu_si256(mem + (size - 2 * YMM_SZ));
        y6 = _mm256_loadu_si256(mem + (size - YMM_SZ));

        y3 = _mm256_cmpeq_epi8(y0, y1);
        y4 = _mm256_cmpeq_epi8(y0, y2);
        y7 = _mm256_cmpeq_epi8(y0, y5);
        y8 = _mm256_cmpeq_epi8(y0, y6);

        y1 = _mm256_or_si256(y3, y4);
        y5 = _mm256_or_si256(y7, y8);
        y2 = _mm256_or_si256(y1, y5);
        ret = _mm256_movemask_epi8(y2);
        if (ret)
        {
            ret1 = _mm256_movemask_epi8(y1);
            if (ret1)
            {
                ret2 = _mm256_movemask_epi8(y3);
                if (ret2)
                {
                    index = _tzcnt_u32(ret2);
                    return mem + index;
                }
                index = _tzcnt_u32(ret1) + YMM_SZ;
                return mem + index;
            }
            ret3 = _mm256_movemask_epi8(y7);
            if (ret3)
            {
                index = _tzcnt_u32(ret3) + (size - 2 * YMM_SZ);
                return mem + index;
            }
            index = _tzcnt_u32(ret) + (size - YMM_SZ);
            return mem + index;
        }
        return NULL;
    }

    y1 = _mm256_loadu_si256(mem);
    y2 = _mm256_loadu_si256(mem + YMM_SZ);
    y3 = _mm256_loadu_si256(mem + 2 * YMM_SZ);
    y4 = _mm256_loadu_si256(mem + 3 * YMM_SZ);

    y5 = _mm256_cmpeq_epi8(y0, y1);
    y6 = _mm256_cmpeq_epi8(y0, y2);
    y7 = _mm256_cmpeq_epi8(y0, y3);
    y8 = _mm256_cmpeq_epi8(y0, y4);

    y1 = _mm256_or_si256(y5, y6);
    y2 = _mm256_or_si256(y7, y8);
    y3 = _mm256_or_si256(y1, y2);
    ret = _mm256_movemask_epi8(y3);

    if (ret)
    {
        ret1 = _mm256_movemask_epi8(y1);
        if (ret1)
        {
            ret2 = _mm256_movemask_epi8(y5);
            if (ret2)
            {
                index = _tzcnt_u32(ret2);
                return mem + index;
            }
            index = _tzcnt_u32(ret1) + YMM_SZ;
            return mem + index;
        }
        ret3 = _mm256_movemask_epi8(y7);
        if (ret3)
        {
            index = _tzcnt_u32(ret3) + 2 * YMM_SZ;
            return mem + index;
        }
        index = _tzcnt_u32(ret) + 3 * YMM_SZ;
        return mem + index;
    }

    if (size > 8 * YMM_SZ)
    {
        size -= 4 * YMM_SZ;
        offset = 4 * YMM_SZ - ((uintptr_t)mem & (YMM_SZ - 1));
        while (size >= offset)
        {
            y1 = _mm256_load_si256(mem + offset);
            y2 = _mm256_load_si256(mem + offset + YMM_SZ);
            y3 = _mm256_load_si256(mem + offset + 2 * YMM_SZ);
            y4 = _mm256_load_si256(mem + offset + 3 * YMM_SZ);

            y5 = _mm256_cmpeq_epi8(y0, y1);
            y6 = _mm256_cmpeq_epi8(y0, y2);
            y7 = _mm256_cmpeq_epi8(y0, y3);
            y8 = _mm256_cmpeq_epi8(y0, y4);

            y1 = _mm256_or_si256(y5, y6);
            y2 = _mm256_or_si256(y7, y8);
            y3 = _mm256_or_si256(y1, y2);
            ret = _mm256_movemask_epi8(y3);

            if (ret)
            {
                ret1 = _mm256_movemask_epi8(y1);
                if (ret1)
                {
                    ret2 = _mm256_movemask_epi8(y5);
                    if (ret2)
                    {
                        index = _tzcnt_u32(ret2) + offset;
                        return mem + index;
                    }
                    index = _tzcnt_u32(ret1) + offset + YMM_SZ;
                    return mem + index;
                }
                ret3 = _mm256_movemask_epi8(y7);
                if (ret3)
                {
                    index = _tzcnt_u32(ret3) + offset + 2 * YMM_SZ;
                    return mem + index;
                }
                index = _tzcnt_u32(ret) + offset + 3 * YMM_SZ;
                return mem + index;
            }
            offset += 4 * YMM_SZ;
        }
        size += 4 * YMM_SZ;
        if (size == offset)
            return NULL;
    }

    y1 = _mm256_loadu_si256(mem + size - 4 * YMM_SZ);
    y2 = _mm256_loadu_si256(mem + size - 3 * YMM_SZ);
    y3 = _mm256_loadu_si256(mem + size - 2 * YMM_SZ);
    y4 = _mm256_loadu_si256(mem + size - 1 * YMM_SZ);

    y5 = _mm256_cmpeq_epi8(y0, y1);
    y6 = _mm256_cmpeq_epi8(y0, y2);
    y7 = _mm256_cmpeq_epi8(y0, y3);
    y8 = _mm256_cmpeq_epi8(y0, y4);

    y1 = _mm256_or_si256(y5, y6);
    y2 = _mm256_or_si256(y7, y8);
    y3 = _mm256_or_si256(y1, y2);
    ret = _mm256_movemask_epi8(y3);

    if (ret)
    {
        ret1 = _mm256_movemask_epi8(y1);
        if (ret1)
        {
            ret2 = _mm256_movemask_epi8(y5);
            if (ret2)
            {
                index = _tzcnt_u32(ret2) + size - 4 * YMM_SZ;
                return mem + index;
            }
            index = _tzcnt_u32(ret1) + size - 3 * YMM_SZ;
            return mem + index;
        }
        ret3 = _mm256_movemask_epi8(y7);
        if (ret3)
        {
            index = _tzcnt_u32(ret3) + size - 2 * YMM_SZ;
            return mem + index;
        }
        index = _tzcnt_u32(ret) + size - 1 * YMM_SZ;
        return mem + index;
    }
    return NULL;
}

#ifdef AVX512_FEATURE_ENABLED
static inline void *_memchr_avx512(void *mem, int val, size_t size)
{
    __m512i z0, z1, z2, z3, z4;
    uint64_t ret = 0, ret1 = 0, ret2 = 0, ret3 = 0;
    size_t index = 0, offset = 0;
    z0 = _mm512_set1_epi8((char)val);

    if (size <= 2 * ZMM_SZ)
    {
        z1 = _mm512_loadu_si512(mem);
        ret = _mm512_cmpeq_epu8_mask(z0, z1);

        if (ret == 0)
        {
            index = size - ZMM_SZ;
            z1 = _mm512_loadu_si512(mem + index);
            ret = _mm512_cmpeq_epu8_mask(z0, z1);

            if (ret == 0)
                return NULL;
        }
        index += _tzcnt_u64(ret);
        return mem + index;
    }

    if (size <= 4 * ZMM_SZ)
    {
        z1 = _mm512_loadu_si512(mem);
        z2 = _mm512_loadu_si512(mem + ZMM_SZ);
        z3 = _mm512_loadu_si512(mem + (size - 2 * ZMM_SZ));
        z4 = _mm512_loadu_si512(mem + (size - ZMM_SZ));

        ret = _mm512_cmpeq_epu8_mask(z0, z1);
        ret1 = ret | _mm512_cmpeq_epu8_mask(z0, z2);
        ret2 = _mm512_cmpeq_epu8_mask(z0, z3);
        ret3 = ret2 | _mm512_cmpeq_epu8_mask(z0, z4);

        if (ret1 | ret3)
        {
            if (ret1)
            {
                if (ret)
                {
                    index = _tzcnt_u64(ret);
                    return mem + index;
                }
                index = _tzcnt_u64(ret1) + ZMM_SZ;
                return mem + index;
            }
            if (ret2)
            {
                index = _tzcnt_u64(ret2) + (size - 2 * ZMM_SZ);
                return mem + index;
            }
            index = _tzcnt_u64(ret3) + (size - ZMM_SZ);
            return mem + index;
        }
        return NULL;
    }

    z1 = _mm512_loadu_si512(mem);
    ret = _mm512_cmpeq_epu8_mask(z0, z1);
    if (ret)
    {
        index = _tzcnt_u64(ret);
        return mem + index;
    }

    z2 = _mm512_loadu_si512(mem + ZMM_SZ);
    ret = _mm512_cmpeq_epu8_mask(z0, z2);
    if (ret)
    {
        index = _tzcnt_u64(ret) + ZMM_SZ;
        return mem + index;
    }
    z3 = _mm512_loadu_si512(mem + 2 * ZMM_SZ);
    ret = _mm512_cmpeq_epu8_mask(z0, z3);
    if (ret)
    {
        index = _tzcnt_u64(ret) + 2 * ZMM_SZ;
        return mem + index;
    }
    z4 = _mm512_loadu_si512(mem + 3 * ZMM_SZ);
    ret = _mm512_cmpeq_epu8_mask(z0, z4);
    if (ret)
    {
        index = _tzcnt_u64(ret) + 3 * ZMM_SZ;
        return mem + index;
    }

    offset = 4 * ZMM_SZ - ((uintptr_t)mem & (ZMM_SZ - 1));
    size -= 4 * ZMM_SZ;
    while (size >= offset)
    {
        z1 = _mm512_load_si512(mem + offset);
        z2 = _mm512_load_si512(mem + offset + ZMM_SZ);
        z3 = _mm512_load_si512(mem + offset + 2 * ZMM_SZ);
        z4 = _mm512_load_si512(mem + offset + 3 * ZMM_SZ);

        ret = _mm512_cmpeq_epu8_mask(z0, z1);
        ret1 = ret | _mm512_cmpeq_epu8_mask(z0, z2);
        ret2 = ret1 | _mm512_cmpeq_epu8_mask(z0, z3);
        ret3 = ret2 | _mm512_cmpeq_epu8_mask(z0, z4);

        if (ret3 != 0)
        {
            if (ret != 0)
            {
                index = _tzcnt_u64(ret) + offset;
                return mem + index;
            }
            if (ret1 != 0)
            {
                index = _tzcnt_u64(ret1) + offset + ZMM_SZ;
                return mem + index;
            }
            if (ret2 != 0)
            {
                index = _tzcnt_u64(ret2) + offset + 2 * ZMM_SZ;
                return mem + index;
            }
            index = _tzcnt_u64(ret3) + offset + 3 * ZMM_SZ;
            return mem + index;
        }
        offset += 4 * ZMM_SZ;
    }

    z1 = _mm512_loadu_si512(mem + size);
    ret = _mm512_cmpeq_epu8_mask(z0, z1);
    if (ret)
    {
        index = _tzcnt_u64(ret) + size;
        return mem + index;
    }

    z2 = _mm512_loadu_si512(mem + size + ZMM_SZ);
    ret = _mm512_cmpeq_epu8_mask(z0, z2);
    if (ret)
    {
        index = _tzcnt_u64(ret) + size + ZMM_SZ;
        return mem + index;
    }
    z3 = _mm512_loadu_si512(mem + size + 2 * ZMM_SZ);
    ret = _mm512_cmpeq_epu8_mask(z0, z3);
    if (ret)
    {
        index = _tzcnt_u64(ret) + size + 2 * ZMM_SZ;
        return mem + index;
    }
    z4 = _mm512_loadu_si512(mem + size + 3 * ZMM_SZ);
    ret = _mm512_cmpeq_epu8_mask(z0, z4);
    if (ret)
    {
        index = _tzcnt_u64(ret) + size + 3 * ZMM_SZ;
        return mem + index;
    }
    return NULL;
}
#endif

void * __attribute__((flatten)) amd_memchr(void *mem, int val, size_t size)
{
    LOG_INFO("\n");
    if (size == 0) return NULL;

#ifdef AVX512_FEATURE_ENABLED
    if (size <= ZMM_SZ)
    {
       __m512i z0, z1, z2;
        __mmask64 mask;
        uint64_t ret = 0;
        uint32_t index = 0;

        mask = ((uint64_t)-1) >> (ZMM_SZ - size);
        z0 = _mm512_set1_epi8(val);
        z1 = _mm512_set1_epi8(val + 1);
        z2 =  _mm512_mask_loadu_epi8(z1, mask, mem);
        ret = _mm512_cmpeq_epu8_mask(z0, z2);

        if (ret == 0)
            return NULL;
        index = _tzcnt_u64(ret);
        return mem + index;
    }
    return _memchr_avx512(mem, val, size);
#else
    return _memchr_avx2(mem, val, size);
#endif
}

void *memchr(const void *, int, size_t) __attribute__((weak,
                    alias("amd_memchr"), visibility("default")));
