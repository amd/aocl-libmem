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

static int memcmp_below_64(const void *mem1, const void *mem2, size_t size)
{
    __m256i y0, y1, y2, y3;
    __m128i x0, x1, x2, x3;
    uint32_t ret = 0;

    if (size == 0)
        return 0;

    if (size == 1)
    {
        return *((uint8_t *)mem1) != *((uint8_t *)mem2);
    }
    if (size <= 4)
    {
        return ((*((uint16_t *)mem1) != *((uint16_t *)mem2)) | 
        (*((uint16_t *)(mem1 + size - 2)) != *((uint16_t *)(mem2 + size -2))));
    }
    if (size <= 8)
    {
        return ((*((uint32_t *)mem1) != *((uint32_t *)mem2)) | 
        (*((uint32_t *)(mem1 + size - 4)) != *((uint32_t *)(mem2 + size - 4))));
    }
    if (size <= 16)
    {
        return ((*((uint64_t*)mem1)) != (*((uint64_t*)mem2))) | 
        ((*((uint64_t *)(mem1 + size - 8))) != (*((uint64_t*)(mem2 + size - 8))));
    }
    if (size <= 32)
    {
        x0 = _mm_loadu_si128(mem1);
        x1 = _mm_loadu_si128(mem2);
        x0 = _mm_cmpeq_epi8(x0, x1);
        ret = _mm_movemask_epi8(x0);
        x2 = _mm_loadu_si128(mem1 + size - 16);
        x3 = _mm_loadu_si128(mem2 + size - 16);
        x2 = _mm_cmpeq_epi8(x2, x3);
        ret &= _mm_movemask_epi8(x2);
        return (uint16_t)ret ^ 0xffff; 
    }
    else
    {
        y0 = _mm256_loadu_si256(mem1);
        y1 = _mm256_loadu_si256(mem2);
        y0 = _mm256_cmpeq_epi8(y0, y1);
        ret =  _mm256_movemask_epi8(y0);
        y2 = _mm256_loadu_si256(mem1 + size - 32);
        y3 = _mm256_loadu_si256(mem2 + size - 32);
        y2 = _mm256_cmpeq_epi8(y2, y3);
        ret &=  _mm256_movemask_epi8(y2);
        return (int32_t)ret ^ 0xffffffff; 
    }
    return -1;
}

int __memcmp_avx2_unaligned(const void *mem1, const void *mem2, size_t size)
{
    __m256i y0, y1, y2, y3, y4, y5, y6, y7;
    size_t offset = 0;
    int32_t ret = 0;

    LOG_INFO("\n");

    if (size <= 64)
        return memcmp_below_64(mem1, mem2, size);

    if (size <= 128)
    {
        y0 = _mm256_load_si256(mem2 + 0 * 32);
        y1 = _mm256_load_si256(mem2 + 1 * 32);
        y4 = _mm256_load_si256(mem1 + 0 * 32);
        y5 = _mm256_load_si256(mem1 + 1 * 32);
        y2 = _mm256_loadu_si256(mem2 + size - 1 * 32);
        y3 = _mm256_loadu_si256(mem2 + size - 2 * 32);
        y6 = _mm256_loadu_si256(mem1 + size - 1 * 32);
        y7 = _mm256_loadu_si256(mem1 + size - 2 * 32);
        y0 = _mm256_cmpeq_epi8(y0, y4);
        y1 = _mm256_cmpeq_epi8(y1, y5);
        y2 = _mm256_cmpeq_epi8(y2, y6);
        y3 = _mm256_cmpeq_epi8(y3, y7);
        ret = _mm256_movemask_epi8(y0) & _mm256_movemask_epi8(y1);
        ret &= _mm256_movemask_epi8(y2) & _mm256_movemask_epi8(y3);
        return (ret ^ 0xffffffff);
    }
    while (size > offset)
    {
        y0 = _mm256_load_si256(mem2 + offset + 0 * 32);
        y1 = _mm256_load_si256(mem2 + offset + 1 * 32);
        y2 = _mm256_load_si256(mem2 + offset + 2 * 32);
        y3 = _mm256_load_si256(mem2 + offset + 3 * 32);
        y4 = _mm256_load_si256(mem1 + offset + 0 * 32);
        y5 = _mm256_load_si256(mem1 + offset + 1 * 32);
        y6 = _mm256_load_si256(mem1 + offset + 2 * 32);
        y7 = _mm256_load_si256(mem1 + offset + 3 * 32);
        y0 = _mm256_cmpeq_epi8(y0, y4);
        y1 = _mm256_cmpeq_epi8(y1, y5);
        y2 = _mm256_cmpeq_epi8(y2, y6);
        y3 = _mm256_cmpeq_epi8(y3, y7);
        ret = _mm256_movemask_epi8(y0) & _mm256_movemask_epi8(y1);
        ret &= _mm256_movemask_epi8(y2) & _mm256_movemask_epi8(y3);
        if (ret ^ 0xffffffff)
            return -1;
        y0 = _mm256_loadu_si256(mem2 + size - 1 * 32);
        y1 = _mm256_loadu_si256(mem2 + size - 2 * 32);
        y2 = _mm256_loadu_si256(mem2 + size - 3 * 32);
        y3 = _mm256_loadu_si256(mem2 + size - 4 * 32);
        y4 = _mm256_loadu_si256(mem1 + size - 1 * 32);
        y5 = _mm256_loadu_si256(mem1 + size - 2 * 32);
        y6 = _mm256_loadu_si256(mem1 + size - 3 * 32);
        y7 = _mm256_loadu_si256(mem1 + size - 4 * 32);
        y0 = _mm256_cmpeq_epi8(y0, y4);
        y1 = _mm256_cmpeq_epi8(y1, y5);
        y2 = _mm256_cmpeq_epi8(y2, y6);
        y3 = _mm256_cmpeq_epi8(y3, y7);
        ret = _mm256_movemask_epi8(y0) & _mm256_movemask_epi8(y1);
        ret &= _mm256_movemask_epi8(y2) & _mm256_movemask_epi8(y3);
        if (ret ^ 0xffffffff)
            return -1;
        offset +=128;
        size -= 128;
    }
    return (ret ^ 0xffffffff);
}

int __memcmp_avx2_nt(const void *mem1, const void *mem2, size_t size)
{
    __m256i y0, y1, y2, y3, y4, y5, y6, y7;
    size_t offset = 0, len = size;
    int32_t ret = 0;

    LOG_INFO("\n");

    if (size <= 64)
        return memcmp_below_64(mem1, mem2, size);

    if (size <= 128)
    {
        y0 = _mm256_stream_load_si256(mem2 + 0 * 32);
        y1 = _mm256_stream_load_si256(mem2 + 1 * 32);
        y4 = _mm256_stream_load_si256(mem1 + 0 * 32);
        y5 = _mm256_stream_load_si256(mem1 + 1 * 32);
        y2 = _mm256_loadu_si256(mem2 + size - 2 * 32);
        y3 = _mm256_loadu_si256(mem2 + size - 1 * 32);
        y6 = _mm256_loadu_si256(mem1 + size - 2 * 32);
        y7 = _mm256_loadu_si256(mem1 + size - 1 * 32);
        y0 = _mm256_cmpeq_epi8(y0, y4);
        y1 = _mm256_cmpeq_epi8(y1, y5);
        y2 = _mm256_cmpeq_epi8(y2, y6);
        y3 = _mm256_cmpeq_epi8(y3, y7);
        ret = _mm256_movemask_epi8(y0) & _mm256_movemask_epi8(y1);
        ret &= _mm256_movemask_epi8(y2) & _mm256_movemask_epi8(y3);
        return (ret ^ 0xffffffff);
    }
    //make adjustments for last 32
    while (size > offset)
    {
        y0 = _mm256_stream_load_si256(mem2 + offset + 0 * 32);
        y1 = _mm256_stream_load_si256(mem2 + offset + 1 * 32);
        y2 = _mm256_stream_load_si256(mem2 + offset + 2 * 32);
        y3 = _mm256_stream_load_si256(mem2 + offset + 3 * 32);
        y4 = _mm256_stream_load_si256(mem1 + offset + 0 * 32);
        y5 = _mm256_stream_load_si256(mem1 + offset + 1 * 32);
        y6 = _mm256_stream_load_si256(mem1 + offset + 2 * 32);
        y7 = _mm256_stream_load_si256(mem1 + offset + 3 * 32);
        y0 = _mm256_cmpeq_epi8(y0, y4);
        y1 = _mm256_cmpeq_epi8(y1, y5);
        y2 = _mm256_cmpeq_epi8(y2, y6);
        y3 = _mm256_cmpeq_epi8(y3, y7);
        ret = _mm256_movemask_epi8(y0) & _mm256_movemask_epi8(y1);
        ret &= _mm256_movemask_epi8(y2) & _mm256_movemask_epi8(y3);
        if (ret ^ 0xffffffff)
            return -1;
        offset +=128;
        size -= 128;
    }
    //cmp last 128B
    y0 = _mm256_loadu_si256(mem2 + len - 4 * 32);
    y1 = _mm256_loadu_si256(mem2 + len - 3 * 32);
    y2 = _mm256_loadu_si256(mem2 + len - 2 * 32);
    y3 = _mm256_loadu_si256(mem2 + len - 1 * 32);
    y4 = _mm256_loadu_si256(mem1 + len - 4 * 32);
    y5 = _mm256_loadu_si256(mem1 + len - 3 * 32);
    y6 = _mm256_loadu_si256(mem1 + len - 2 * 32);
    y7 = _mm256_loadu_si256(mem1 + len - 1 * 32);
    y0 = _mm256_cmpeq_epi8(y0, y4);
    y1 = _mm256_cmpeq_epi8(y1, y5);
    y2 = _mm256_cmpeq_epi8(y2, y6);
    y3 = _mm256_cmpeq_epi8(y3, y7);
    ret = _mm256_movemask_epi8(y0) & _mm256_movemask_epi8(y1);
    ret &= _mm256_movemask_epi8(y2) & _mm256_movemask_epi8(y3);
    return (ret ^ 0xffffffff);
}

int __memcmp_avx2_aligned(const void *mem1, const void *mem2, size_t size)
{
    __m256i y0, y1, y2, y3, y4, y5, y6, y7;
    size_t offset = 0, len = size;
    int32_t ret = 0;

    LOG_INFO("\n");

    if (size <= 64)
        return memcmp_below_64(mem1, mem2, size);

    if (size <= 128)
    {
        y0 = _mm256_load_si256(mem2 + 0 * 32);
        y1 = _mm256_load_si256(mem2 + 1 * 32);
        y4 = _mm256_load_si256(mem1 + 0 * 32);
        y5 = _mm256_load_si256(mem1 + 1 * 32);
        y2 = _mm256_loadu_si256(mem2 + size - 2 * 32);
        y3 = _mm256_loadu_si256(mem2 + size - 1 * 32);
        y6 = _mm256_loadu_si256(mem1 + size - 2 * 32);
        y7 = _mm256_loadu_si256(mem1 + size - 1 * 32);
        y0 = _mm256_cmpeq_epi8(y0, y4);
        y1 = _mm256_cmpeq_epi8(y1, y5);
        y2 = _mm256_cmpeq_epi8(y2, y6);
        y3 = _mm256_cmpeq_epi8(y3, y7);
        ret = _mm256_movemask_epi8(y0) & _mm256_movemask_epi8(y1);
        ret &= _mm256_movemask_epi8(y2) & _mm256_movemask_epi8(y3);
        return (ret ^ 0xffffffff);
    }
    while (size > 128)
    {
        y0 = _mm256_load_si256(mem2 + offset + 0 * 32);
        y1 = _mm256_load_si256(mem2 + offset + 1 * 32);
        y2 = _mm256_load_si256(mem2 + offset + 2 * 32);
        y3 = _mm256_load_si256(mem2 + offset + 3 * 32);
        y4 = _mm256_load_si256(mem1 + offset + 0 * 32);
        y5 = _mm256_load_si256(mem1 + offset + 1 * 32);
        y6 = _mm256_load_si256(mem1 + offset + 2 * 32);
        y7 = _mm256_load_si256(mem1 + offset + 3 * 32);
        y0 = _mm256_cmpeq_epi8(y0, y4);
        y1 = _mm256_cmpeq_epi8(y1, y5);
        y2 = _mm256_cmpeq_epi8(y2, y6);
        y3 = _mm256_cmpeq_epi8(y3, y7);
        ret = _mm256_movemask_epi8(y0) & _mm256_movemask_epi8(y1);
        ret &= _mm256_movemask_epi8(y2) & _mm256_movemask_epi8(y3);
        if (ret ^ 0xffffffff)
            return -1;
        offset +=128;
        size -= 128;
    }
    //cmp last 128B
    y0 = _mm256_loadu_si256(mem2 + len - 4 * 32);
    y1 = _mm256_loadu_si256(mem2 + len - 3 * 32);
    y2 = _mm256_loadu_si256(mem2 + len - 2 * 32);
    y3 = _mm256_loadu_si256(mem2 + len - 1 * 32);
    y4 = _mm256_loadu_si256(mem1 + len - 4 * 32);
    y5 = _mm256_loadu_si256(mem1 + len - 3 * 32);
    y6 = _mm256_loadu_si256(mem1 + len - 2 * 32);
    y7 = _mm256_loadu_si256(mem1 + len - 1 * 32);
    y0 = _mm256_cmpeq_epi8(y0, y4);
    y1 = _mm256_cmpeq_epi8(y1, y5);
    y2 = _mm256_cmpeq_epi8(y2, y6);
    y3 = _mm256_cmpeq_epi8(y3, y7);
    ret = _mm256_movemask_epi8(y0) & _mm256_movemask_epi8(y1);
    ret &= _mm256_movemask_epi8(y2) & _mm256_movemask_epi8(y3);
    return (ret ^ 0xffffffff);
}
