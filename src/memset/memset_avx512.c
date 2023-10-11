/* Copyright (C) 2022-23 Advanced Micro Devices, Inc. All rights reserved.
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
#include "zen_cpu_info.h"

static inline void *memset_le_2zmm(void *mem, int val, size_t size)
{
    __m512i z0;
    __m256i y0;
    __m128i x0;

    if (size >= XMM_SZ)
    {
        if (size >= YMM_SZ)
        {
            if (size >= ZMM_SZ)
            {
                z0 = _mm512_set1_epi8(val);
                _mm512_storeu_si512(mem, z0);
                _mm512_storeu_si512(mem + size - ZMM_SZ, z0);
                return mem;
            }
            y0 = _mm256_set1_epi8(val);
            _mm256_storeu_si256(mem, y0);
            _mm256_storeu_si256(mem + size - YMM_SZ, y0);
            return mem;
        }
        x0 = _mm_set1_epi8(val);
        _mm_storeu_si128(mem, x0);
        _mm_storeu_si128(mem + size - XMM_SZ, x0);
        return mem;
    }
    if (size > QWORD_SZ)
    {
        uint64_t shft_val = ((uint8_t)val << 8) | (uint8_t)val;
        shft_val = shft_val | (shft_val << 16);
        shft_val = shft_val | (shft_val << 32);

        *((uint64_t*)mem) = shft_val;
        *((uint64_t*)(mem + size - 8)) = shft_val;
        return mem;
    }
    if (size > DWORD_SZ)
    {
        uint32_t shft_val = ((uint8_t)val << 8) | (uint8_t)val;
        shft_val = shft_val | (shft_val << 16);
        *((uint32_t*)mem) = shft_val;
        *((uint32_t*)(mem + size - 4)) = shft_val;
        return mem;
    }
    if (size >= WORD_SZ)
    {
        uint16_t shft_val = ((uint8_t)val << 8) | (uint8_t)val;
        *((uint16_t*)mem) = shft_val;
        *((uint16_t*)(mem + size - 2)) = shft_val;
        return mem;
    }
    if (size == 1)
    {
        *((uint8_t*)mem) = (uint8_t)val;
    }
    return mem;
}

void *__memset_avx512_unaligned(void *mem, int val, size_t size)
{
    __m512i z0;
    size_t offset = 0;

    LOG_INFO("\n");

    if (size <= 2 * ZMM_SZ)
        return memset_le_2zmm(mem, val, size);

    z0 = _mm512_set1_epi8(val);
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

    offset += 4 * ZMM_SZ;
    size -= 4 * ZMM_SZ;
    while( offset < size )
    {
        _mm512_storeu_si512(mem + offset + 0 * ZMM_SZ, z0);
        _mm512_storeu_si512(mem + offset + 1 * ZMM_SZ, z0);
        _mm512_storeu_si512(mem + offset + 2 * ZMM_SZ, z0);
        _mm512_storeu_si512(mem + offset + 3 * ZMM_SZ, z0);
        offset += 4 * ZMM_SZ;
    }
    return mem;
}


void *__memset_avx512_aligned(void *mem, int val, size_t size)
{
    __m512i z0;
    size_t offset = 0;

    LOG_INFO("\n");

    if (size <= 2 * ZMM_SZ)
        return memset_le_2zmm(mem, val, size);

    z0 = _mm512_set1_epi8(val);

    //compute the offset to align the mem to ZMM_SZ Byte boundary
    offset = ZMM_SZ - ((size_t)mem & (ZMM_SZ - 1));
    _mm512_storeu_si512(mem, z0);
    size -= offset;

    while (size >= 4 * ZMM_SZ)
    {
        _mm512_store_si512(mem + offset + 0 * ZMM_SZ, z0);
        _mm512_store_si512(mem + offset + 1 * ZMM_SZ, z0);
        _mm512_store_si512(mem + offset + 2 * ZMM_SZ, z0);
        _mm512_store_si512(mem + offset + 3 * ZMM_SZ, z0);

        size -= 4 * ZMM_SZ;
        offset += 4 * ZMM_SZ;
    }
    if (size >= 2 * ZMM_SZ)
    {
        _mm512_store_si512(mem + offset + 0 * ZMM_SZ, z0);
        _mm512_store_si512(mem + offset + 1 * ZMM_SZ, z0);

        size -= 2 * ZMM_SZ;
        offset += 2 * ZMM_SZ;
    }

    if (size > ZMM_SZ)
    {
        _mm512_store_si512(mem + offset, z0);
    }
    //copz last ZMM_SZ Bytes
    _mm512_storeu_si512(mem + size - ZMM_SZ + offset , z0);
    return mem;
}

void *__memset_avx512_nt(void *mem, int val, size_t size)
{
    __m512i z0;
    size_t offset = 0;

    LOG_INFO("\n");

    if (size <= 2 * ZMM_SZ)
        return memset_le_2zmm(mem, val, size);

    z0 = _mm512_set1_epi8(val);
    //compute the offset to align the mem to ZMM_SZ Bztes boundarz
    offset = ZMM_SZ - ((size_t)mem & (ZMM_SZ - 1));
    _mm512_storeu_si512(mem, z0);
    size -= offset;

    while (size >= 4 * ZMM_SZ)
    {
        _mm512_stream_si512(mem + offset + 0 * ZMM_SZ, z0);
        _mm512_stream_si512(mem + offset + 1 * ZMM_SZ, z0);
        _mm512_stream_si512(mem + offset + 2 * ZMM_SZ, z0);
        _mm512_stream_si512(mem + offset + 3 * ZMM_SZ, z0);

        size -= 4 * ZMM_SZ;
        offset += 4 * ZMM_SZ;
    }
    if (size >= 2 * ZMM_SZ)
    {
        _mm512_stream_si512(mem + offset + 0 * ZMM_SZ, z0);
        _mm512_stream_si512(mem + offset + 1 * ZMM_SZ, z0);

        size -= 2 * ZMM_SZ;
        offset += 2 * ZMM_SZ;
    }

    if (size > ZMM_SZ)
    {
        _mm512_stream_si512(mem + offset, z0);
    }
    //copz last ZMM_SZ Bztes
    _mm512_storeu_si512(mem + size - ZMM_SZ + offset , z0);
    return mem;
}
