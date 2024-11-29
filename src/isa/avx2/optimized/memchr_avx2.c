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
        ret = _mm_movemask_epi8(_mm_or_si128(x3, x4)) & 0xff;

        if (ret)
        {
            ret1 = _mm_movemask_epi8(x3) & 0xff;

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

static inline void * __attribute__((flatten)) _memchr_avx2(const void *mem, int val, size_t size)
{
    __m256i y0, y1, y2, y3, y4, y5, y6, y7, y8;
    size_t index = 0, offset = 0;
    int ret, ret1, ret2, ret3;
    void *ret_ptr = (void *)mem;

    if (likely(size <= 2 * YMM_SZ))
        return memchr_le_2ymm((void *)mem, val, (uint8_t) size);

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
                    return ret_ptr + index;
                }
                index = _tzcnt_u32(ret1) + YMM_SZ;
                return ret_ptr + index;
            }
            ret3 = _mm256_movemask_epi8(y7);
            if (ret3)
            {
                index = _tzcnt_u32(ret3) + (size - 2 * YMM_SZ);
                return ret_ptr + index;
            }
            index = _tzcnt_u32(ret) + (size - YMM_SZ);
            return ret_ptr + index;
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
                return ret_ptr + index;
            }
            index = _tzcnt_u32(ret1) + YMM_SZ;
            return ret_ptr + index;
        }
        ret3 = _mm256_movemask_epi8(y7);
        if (ret3)
        {
            index = _tzcnt_u32(ret3) + 2 * YMM_SZ;
            return ret_ptr + index;
        }
        index = _tzcnt_u32(ret) + 3 * YMM_SZ;
        return ret_ptr + index;
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
                        return ret_ptr + index;
                    }
                    index = _tzcnt_u32(ret1) + offset + YMM_SZ;
                    return ret_ptr + index;
                }
                ret3 = _mm256_movemask_epi8(y7);
                if (ret3)
                {
                    index = _tzcnt_u32(ret3) + offset + 2 * YMM_SZ;
                    return ret_ptr + index;
                }
                index = _tzcnt_u32(ret) + offset + 3 * YMM_SZ;
                return ret_ptr + index;
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
                return ret_ptr + index;
            }
            index = _tzcnt_u32(ret1) + size - 3 * YMM_SZ;
            return ret_ptr + index;
        }
        ret3 = _mm256_movemask_epi8(y7);
        if (ret3)
        {
            index = _tzcnt_u32(ret3) + size - 2 * YMM_SZ;
            return ret_ptr + index;
        }
        index = _tzcnt_u32(ret) + size - 1 * YMM_SZ;
        return ret_ptr + index;
    }
    return NULL;
}
