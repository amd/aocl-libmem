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
#include <immintrin.h>
#include "logger.h"
#include <stdint.h>

static inline void *memset_below_64(void *mem, int val, size_t size)
{
    __m256i y0;
    __m128i x0;

    if (size > 15)
    {
        x0 = _mm_set1_epi8(val);
        if (size > 31)
        {
            y0 = _mm256_broadcastb_epi8(x0);
            _mm256_storeu_si256(mem, y0);
            _mm256_storeu_si256(mem + size - 32, y0);
            return mem;
        }
        _mm_storeu_si128(mem, x0);
        _mm_storeu_si128(mem + size - 16, x0);
        return mem;
    }
    if (size > 8)
    {
        uint64_t shft_val = ((uint8_t)val << 8) | (uint8_t)val;
        shft_val = shft_val | (shft_val << 16);
        shft_val = shft_val | (shft_val << 32);

        *((uint64_t*)mem) = shft_val;
        *((uint64_t*)(mem + size - 8)) = shft_val;
        return mem;
    }
    if (size > 4)
    {
        uint32_t shft_val = ((uint8_t)val << 8) | (uint8_t)val;
        shft_val = shft_val | (shft_val << 16);
        *((uint32_t*)mem) = shft_val;
        *((uint32_t*)(mem + size - 4)) = shft_val;
        return mem;
    }
    if (size > 1)
    {
        uint16_t shft_val = ((uint8_t)val << 8) | (uint8_t)val;
        *((uint16_t*)mem) = shft_val;
        *((uint16_t*)(mem + size - 2)) = shft_val;
        return mem;
    }
    *((uint8_t*)mem) = (uint8_t)val;
    return mem;
}

void *__memset_avx2_unaligned(void *mem, int val, size_t size)
{
    __m256i y0;
    __m128i x0;
    size_t offset = 0;

    LOG_INFO("\n");

    if (size < 64)
        return memset_below_64(mem, val, size);

    x0 = _mm_set1_epi8(val);
    y0 = _mm256_broadcastb_epi8(x0);
    if (size < 128)
    {
        _mm256_storeu_si256 (mem , y0);
        _mm256_storeu_si256 (mem + 32, y0);
        _mm256_storeu_si256 (mem + size - 64, y0);
        _mm256_storeu_si256 (mem + size - 32, y0);
        return mem;
    }
    _mm256_storeu_si256 (mem + 0 * 32, y0);
    _mm256_storeu_si256 (mem + 1 * 32, y0);
    _mm256_storeu_si256 (mem + 2 * 32, y0);
    _mm256_storeu_si256 (mem + 3 * 32, y0);
    _mm256_storeu_si256 (mem + size - 4 * 32, y0);
    _mm256_storeu_si256 (mem + size - 3 * 32, y0);
    _mm256_storeu_si256 (mem + size - 2 * 32, y0);
    _mm256_storeu_si256 (mem + size - 1 * 32, y0);

    offset += 128;
    size -= 128;
    while( offset < size )
    {
        _mm256_storeu_si256 (mem + offset + 0 * 32, y0);
        _mm256_storeu_si256 (mem + offset + 1 * 32, y0);
        _mm256_storeu_si256 (mem + offset + 2 * 32, y0);
        _mm256_storeu_si256 (mem + offset + 3 * 32, y0);
        offset += 128;
    }
    return mem;
}


void *__memset_avx2_aligned(void *mem, int val, size_t size)
{
    __m256i y0;
    __m128i x0;
    size_t offset = 0;

    LOG_INFO("\n");

    if (size < 64)
        return memset_below_64(mem, val, size);

    x0 = _mm_set1_epi8(val);
    y0 = _mm256_broadcastb_epi8(x0);

    //compute the offset to align the mem to 32B boundary
    offset = 0x20 - ((size_t)mem & 0x1F);
    _mm256_storeu_si256 (mem, y0);
    size -= offset;

    while ((size) > 127)
    {
        _mm256_store_si256 (mem + offset + 0 * 32, y0);
        _mm256_store_si256 (mem + offset + 1 * 32, y0);
        _mm256_store_si256 (mem + offset + 2 * 32, y0);
        _mm256_store_si256 (mem + offset + 3 * 32, y0);

        size -= 4 * 32;
        offset += 4 * 32;
    }
    if ((size) > 63)
    {
        _mm256_store_si256 (mem + offset + 0 * 32, y0);
        _mm256_store_si256 (mem + offset + 1 * 32, y0);

        size -= 2 * 32;
        offset += 2 * 32;
    }

    if ((size > 32))
    {
        _mm256_store_si256 (mem + offset, y0);
    }
    //copy last 32B

    _mm256_storeu_si256 (mem + size - 32 + offset , y0);
    return mem;
}

void *__memset_avx2_nt(void *mem, int val, size_t size)
{
    __m256i y0;
    __m128i x0;
    size_t offset = 0;

    LOG_INFO("\n");

    if (size <= 64)
        return memset_below_64(mem, val, size);

    x0 = _mm_set1_epi8(val);
    y0 = _mm256_broadcastb_epi8(x0);
    //compute the offset to align the mem to 32B boundary
    offset = 0x20 - ((size_t)mem & 0x1F);
    _mm256_storeu_si256 (mem, y0);
    size -= offset;

    while ((size) > 127)
    {
        _mm256_stream_si256 (mem + offset + 0 * 32, y0);
        _mm256_stream_si256 (mem + offset + 1 * 32, y0);
        _mm256_stream_si256 (mem + offset + 2 * 32, y0);
        _mm256_stream_si256 (mem + offset + 3 * 32, y0);

        size -= 4 * 32;
        offset += 4 * 32;
    }
    if ((size) > 63)
    {
        _mm256_stream_si256 (mem + offset + 0 * 32, y0);
        _mm256_stream_si256 (mem + offset + 1 * 32, y0);

        size -= 2 * 32;
        offset += 2 * 32;
    }

    if ((size > 32))
    {
        _mm256_stream_si256 (mem + offset, y0);
    }
    //copy last 32B
    _mm256_storeu_si256 (mem + size - 32 + offset , y0);
    return mem;
}
