/* Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
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
#include <immintrin.h>
#include <stdint.h>
#include "zen_cpu_info.h"

static int memcmp_le_2ymm(const void *mem1, const void *mem2, size_t size)
{
    __m256i y0, y1, y2, y3;
    __m128i x0, x1, x2, x3;
    uint32_t cmp_mask = 0;

    if (size == 0)
        return 0;

    if (size == 1)
    {
        return *((uint8_t *)mem1) != *((uint8_t *)mem2);
    }
    if (size <= 2 * WORD_SZ)
    {
        return ((*((uint16_t *)mem1) != *((uint16_t *)mem2)) |
        (*((uint16_t *)(mem1 + size - WORD_SZ)) != \
                        *((uint16_t *)(mem2 + size -WORD_SZ))));
    }
    if (size <= 2 * DWORD_SZ)
    {
        return ((*((uint32_t *)mem1) != *((uint32_t *)mem2)) |
        (*((uint32_t *)(mem1 + size - DWORD_SZ)) != \
                    *((uint32_t *)(mem2 + size - DWORD_SZ))));
    }
    if (size <= 2 * QWORD_SZ)
    {
        return ((*((uint64_t*)mem1) != *((uint64_t*)mem2)) |
        (*((uint64_t *)(mem1 + size - QWORD_SZ)) != \
                    *((uint64_t*)(mem2 + size - QWORD_SZ))));
    }
    if (size <= 2 * XMM_SZ)
    {
        x0 = _mm_loadu_si128(mem1);
        x1 = _mm_loadu_si128(mem2);
        x0 = _mm_cmpeq_epi8(x0, x1);
        cmp_mask = _mm_movemask_epi8(x0);
        x2 = _mm_loadu_si128(mem1 + size - XMM_SZ);
        x3 = _mm_loadu_si128(mem2 + size - XMM_SZ);
        x2 = _mm_cmpeq_epi8(x2, x3);
        cmp_mask &= _mm_movemask_epi8(x2);
        return (cmp_mask != (uint16_t)-1);
    }
    y0 = _mm256_loadu_si256(mem1);
    y1 = _mm256_loadu_si256(mem2);
    y0 = _mm256_cmpeq_epi8(y0, y1);
    cmp_mask =  _mm256_movemask_epi8(y0);
    y2 = _mm256_loadu_si256(mem1 + size - YMM_SZ);
    y3 = _mm256_loadu_si256(mem2 + size - YMM_SZ);
    y2 = _mm256_cmpeq_epi8(y2, y3);
    cmp_mask &=  _mm256_movemask_epi8(y2);
    return (cmp_mask != (uint32_t)-1);
}

int __memcmp_avx2_unaligned(const void *mem1, const void *mem2, size_t size)
{
    __m256i y0, y1, y2, y3, y4, y5, y6, y7;
    size_t offset = 0;
    uint32_t cmp_mask = 0;

    if (size <= 2 * YMM_SZ)
        return memcmp_le_2ymm(mem1, mem2, size);

    if (size <= 4 * YMM_SZ)
    {
        y0 = _mm256_loadu_si256(mem2 + 0 * YMM_SZ);
        y1 = _mm256_loadu_si256(mem2 + 1 * YMM_SZ);
        y4 = _mm256_loadu_si256(mem1 + 0 * YMM_SZ);
        y5 = _mm256_loadu_si256(mem1 + 1 * YMM_SZ);
        y2 = _mm256_loadu_si256(mem2 + size - 1 * YMM_SZ);
        y3 = _mm256_loadu_si256(mem2 + size - 2 * YMM_SZ);
        y6 = _mm256_loadu_si256(mem1 + size - 1 * YMM_SZ);
        y7 = _mm256_loadu_si256(mem1 + size - 2 * YMM_SZ);
        y0 = _mm256_cmpeq_epi8(y0, y4);
        y1 = _mm256_cmpeq_epi8(y1, y5);
        y2 = _mm256_cmpeq_epi8(y2, y6);
        y3 = _mm256_cmpeq_epi8(y3, y7);
        cmp_mask = _mm256_movemask_epi8(y0) & _mm256_movemask_epi8(y1);
        cmp_mask &= _mm256_movemask_epi8(y2) & _mm256_movemask_epi8(y3);
        return (cmp_mask != (uint32_t)-1);
    }
    while (size > offset)
    {
        y0 = _mm256_loadu_si256(mem2 + offset + 0 * YMM_SZ);
        y1 = _mm256_loadu_si256(mem2 + offset + 1 * YMM_SZ);
        y2 = _mm256_loadu_si256(mem2 + offset + 2 * YMM_SZ);
        y3 = _mm256_loadu_si256(mem2 + offset + 3 * YMM_SZ);
        y4 = _mm256_loadu_si256(mem1 + offset + 0 * YMM_SZ);
        y5 = _mm256_loadu_si256(mem1 + offset + 1 * YMM_SZ);
        y6 = _mm256_loadu_si256(mem1 + offset + 2 * YMM_SZ);
        y7 = _mm256_loadu_si256(mem1 + offset + 3 * YMM_SZ);
        y0 = _mm256_cmpeq_epi8(y0, y4);
        y1 = _mm256_cmpeq_epi8(y1, y5);
        y2 = _mm256_cmpeq_epi8(y2, y6);
        y3 = _mm256_cmpeq_epi8(y3, y7);
        cmp_mask = _mm256_movemask_epi8(y0) & _mm256_movemask_epi8(y1);
        cmp_mask &= _mm256_movemask_epi8(y2) & _mm256_movemask_epi8(y3);
        if (cmp_mask != (uint32_t)-1)
            return -1;
        y0 = _mm256_loadu_si256(mem2 + size - 1 * YMM_SZ);
        y1 = _mm256_loadu_si256(mem2 + size - 2 * YMM_SZ);
        y2 = _mm256_loadu_si256(mem2 + size - 3 * YMM_SZ);
        y3 = _mm256_loadu_si256(mem2 + size - 4 * YMM_SZ);
        y4 = _mm256_loadu_si256(mem1 + size - 1 * YMM_SZ);
        y5 = _mm256_loadu_si256(mem1 + size - 2 * YMM_SZ);
        y6 = _mm256_loadu_si256(mem1 + size - 3 * YMM_SZ);
        y7 = _mm256_loadu_si256(mem1 + size - 4 * YMM_SZ);
        y0 = _mm256_cmpeq_epi8(y0, y4);
        y1 = _mm256_cmpeq_epi8(y1, y5);
        y2 = _mm256_cmpeq_epi8(y2, y6);
        y3 = _mm256_cmpeq_epi8(y3, y7);
        cmp_mask = _mm256_movemask_epi8(y0) & _mm256_movemask_epi8(y1);
        cmp_mask &= _mm256_movemask_epi8(y2) & _mm256_movemask_epi8(y3);
        if (cmp_mask != (uint32_t)-1)
            return -1;
        offset += 4 * YMM_SZ;
        size -= 4 * YMM_SZ;
    }
    return (cmp_mask != (uint32_t)-1);
}

int __memcmp_avx2_nt(const void *mem1, const void *mem2, size_t size)
{
    __m256i y0, y1, y2, y3, y4, y5, y6, y7;
    size_t offset = 0, len = size;
    uint32_t cmp_mask = 0;

    LOG_INFO("\n");

    if (size <= 2 * YMM_SZ)
        return memcmp_le_2ymm(mem1, mem2, size);

    if (size <=  4 * YMM_SZ)
    {
        y0 = _mm256_stream_load_si256(mem2 + 0 * YMM_SZ);
        y1 = _mm256_stream_load_si256(mem2 + 1 * YMM_SZ);
        y4 = _mm256_stream_load_si256(mem1 + 0 * YMM_SZ);
        y5 = _mm256_stream_load_si256(mem1 + 1 * YMM_SZ);
        y2 = _mm256_loadu_si256(mem2 + size - 2 * YMM_SZ);
        y3 = _mm256_loadu_si256(mem2 + size - 1 * YMM_SZ);
        y6 = _mm256_loadu_si256(mem1 + size - 2 * YMM_SZ);
        y7 = _mm256_loadu_si256(mem1 + size - 1 * YMM_SZ);
        y0 = _mm256_cmpeq_epi8(y0, y4);
        y1 = _mm256_cmpeq_epi8(y1, y5);
        y2 = _mm256_cmpeq_epi8(y2, y6);
        y3 = _mm256_cmpeq_epi8(y3, y7);
        cmp_mask = _mm256_movemask_epi8(y0) & _mm256_movemask_epi8(y1);
        cmp_mask &= _mm256_movemask_epi8(y2) & _mm256_movemask_epi8(y3);
        return (cmp_mask != (uint32_t)-1);
    }
    //make adjustments for last YMM_SZ
    while (size > offset)
    {
        y0 = _mm256_stream_load_si256(mem2 + offset + 0 * YMM_SZ);
        y1 = _mm256_stream_load_si256(mem2 + offset + 1 * YMM_SZ);
        y2 = _mm256_stream_load_si256(mem2 + offset + 2 * YMM_SZ);
        y3 = _mm256_stream_load_si256(mem2 + offset + 3 * YMM_SZ);
        y4 = _mm256_stream_load_si256(mem1 + offset + 0 * YMM_SZ);
        y5 = _mm256_stream_load_si256(mem1 + offset + 1 * YMM_SZ);
        y6 = _mm256_stream_load_si256(mem1 + offset + 2 * YMM_SZ);
        y7 = _mm256_stream_load_si256(mem1 + offset + 3 * YMM_SZ);
        y0 = _mm256_cmpeq_epi8(y0, y4);
        y1 = _mm256_cmpeq_epi8(y1, y5);
        y2 = _mm256_cmpeq_epi8(y2, y6);
        y3 = _mm256_cmpeq_epi8(y3, y7);
        cmp_mask = _mm256_movemask_epi8(y0) & _mm256_movemask_epi8(y1);
        cmp_mask &= _mm256_movemask_epi8(y2) & _mm256_movemask_epi8(y3);
        if (cmp_mask != (uint32_t)-1)
            return -1;
        offset += 4 * YMM_SZ;
        size -=  4 * YMM_SZ;
    }
    //cmp last  4 * YMM_SZB
    y0 = _mm256_loadu_si256(mem2 + len - 4 * YMM_SZ);
    y1 = _mm256_loadu_si256(mem2 + len - 3 * YMM_SZ);
    y2 = _mm256_loadu_si256(mem2 + len - 2 * YMM_SZ);
    y3 = _mm256_loadu_si256(mem2 + len - 1 * YMM_SZ);
    y4 = _mm256_loadu_si256(mem1 + len - 4 * YMM_SZ);
    y5 = _mm256_loadu_si256(mem1 + len - 3 * YMM_SZ);
    y6 = _mm256_loadu_si256(mem1 + len - 2 * YMM_SZ);
    y7 = _mm256_loadu_si256(mem1 + len - 1 * YMM_SZ);
    y0 = _mm256_cmpeq_epi8(y0, y4);
    y1 = _mm256_cmpeq_epi8(y1, y5);
    y2 = _mm256_cmpeq_epi8(y2, y6);
    y3 = _mm256_cmpeq_epi8(y3, y7);
    cmp_mask = _mm256_movemask_epi8(y0) & _mm256_movemask_epi8(y1);
    cmp_mask &= _mm256_movemask_epi8(y2) & _mm256_movemask_epi8(y3);
    return (cmp_mask != (uint32_t)-1);
}

int __memcmp_avx2_aligned(const void *mem1, const void *mem2, size_t size)
{
    __m256i y0, y1, y2, y3, y4, y5, y6, y7;
    size_t offset = 0, len = size;
    uint32_t cmp_mask = 0;

    LOG_INFO("\n");

    if (size <= 2 * YMM_SZ)
        return memcmp_le_2ymm(mem1, mem2, size);

    if (size <=  4 * YMM_SZ)
    {
        y0 = _mm256_load_si256(mem2 + 0 * YMM_SZ);
        y1 = _mm256_load_si256(mem2 + 1 * YMM_SZ);
        y4 = _mm256_load_si256(mem1 + 0 * YMM_SZ);
        y5 = _mm256_load_si256(mem1 + 1 * YMM_SZ);
        y2 = _mm256_loadu_si256(mem2 + size - 2 * YMM_SZ);
        y3 = _mm256_loadu_si256(mem2 + size - 1 * YMM_SZ);
        y6 = _mm256_loadu_si256(mem1 + size - 2 * YMM_SZ);
        y7 = _mm256_loadu_si256(mem1 + size - 1 * YMM_SZ);
        y0 = _mm256_cmpeq_epi8(y0, y4);
        y1 = _mm256_cmpeq_epi8(y1, y5);
        y2 = _mm256_cmpeq_epi8(y2, y6);
        y3 = _mm256_cmpeq_epi8(y3, y7);
        cmp_mask = _mm256_movemask_epi8(y0) & _mm256_movemask_epi8(y1);
        cmp_mask &= _mm256_movemask_epi8(y2) & _mm256_movemask_epi8(y3);
        return (cmp_mask != (uint32_t)-1);
    }
    while (size >  4 * YMM_SZ)
    {
        y0 = _mm256_load_si256(mem2 + offset + 0 * YMM_SZ);
        y1 = _mm256_load_si256(mem2 + offset + 1 * YMM_SZ);
        y2 = _mm256_load_si256(mem2 + offset + 2 * YMM_SZ);
        y3 = _mm256_load_si256(mem2 + offset + 3 * YMM_SZ);
        y4 = _mm256_load_si256(mem1 + offset + 0 * YMM_SZ);
        y5 = _mm256_load_si256(mem1 + offset + 1 * YMM_SZ);
        y6 = _mm256_load_si256(mem1 + offset + 2 * YMM_SZ);
        y7 = _mm256_load_si256(mem1 + offset + 3 * YMM_SZ);
        y0 = _mm256_cmpeq_epi8(y0, y4);
        y1 = _mm256_cmpeq_epi8(y1, y5);
        y2 = _mm256_cmpeq_epi8(y2, y6);
        y3 = _mm256_cmpeq_epi8(y3, y7);
        cmp_mask = _mm256_movemask_epi8(y0) & _mm256_movemask_epi8(y1);
        cmp_mask &= _mm256_movemask_epi8(y2) & _mm256_movemask_epi8(y3);
        if (cmp_mask != (uint32_t)-1)
            return -1;
        offset += 4 * YMM_SZ;
        size -=  4 * YMM_SZ;
    }
    //cmp last  4 * YMM_SZB
    y0 = _mm256_loadu_si256(mem2 + len - 4 * YMM_SZ);
    y1 = _mm256_loadu_si256(mem2 + len - 3 * YMM_SZ);
    y2 = _mm256_loadu_si256(mem2 + len - 2 * YMM_SZ);
    y3 = _mm256_loadu_si256(mem2 + len - 1 * YMM_SZ);
    y4 = _mm256_loadu_si256(mem1 + len - 4 * YMM_SZ);
    y5 = _mm256_loadu_si256(mem1 + len - 3 * YMM_SZ);
    y6 = _mm256_loadu_si256(mem1 + len - 2 * YMM_SZ);
    y7 = _mm256_loadu_si256(mem1 + len - 1 * YMM_SZ);
    y0 = _mm256_cmpeq_epi8(y0, y4);
    y1 = _mm256_cmpeq_epi8(y1, y5);
    y2 = _mm256_cmpeq_epi8(y2, y6);
    y3 = _mm256_cmpeq_epi8(y3, y7);
    cmp_mask = _mm256_movemask_epi8(y0) & _mm256_movemask_epi8(y1);
    cmp_mask &= _mm256_movemask_epi8(y2) & _mm256_movemask_epi8(y3);
    return (cmp_mask != (uint32_t)-1);
}
