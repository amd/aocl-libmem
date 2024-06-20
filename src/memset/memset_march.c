/* Copyright (C) 2022-24 Advanced Micro Devices, Inc. All rights reserved.
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
#include <immintrin.h>
#include <stdint.h>
#include "zen_cpu_info.h"

extern cpu_info zen_info;


static inline void *memset_le_2ymm(void *mem, int val, size_t size)
{
    if (size >= YMM_SZ)
    {
        __m256i y0;
        y0 = _mm256_set1_epi8(val);
        _mm256_storeu_si256(mem, y0);
        _mm256_storeu_si256(mem + size - YMM_SZ, y0);
        return mem;
    }
    if (size >= XMM_SZ)
    {
        __m128i x0;
        x0 = _mm_set1_epi8(val);
        _mm_storeu_si128(mem, x0);
        _mm_storeu_si128(mem + size - XMM_SZ, x0);
        return mem;
    }
    if (size > QWORD_SZ)
    {
        uint64_t shft_val = (uint64_t)_mm_set1_pi8(val);
        *((uint64_t*)mem) = shft_val;
        *((uint64_t*)(mem + size - QWORD_SZ)) = shft_val;
        return mem;
    }
    if (size >= WORD_SZ)
    {
        uint32_t shft_val = ((uint8_t)val << 8) | (uint8_t)val;
        if (size >= DWORD_SZ)
        {
            shft_val = shft_val | (shft_val << 16);
            *((uint32_t*)mem) = shft_val;
            *((uint32_t*)(mem + size - DWORD_SZ)) = shft_val;
            return mem;
        }
        *((uint16_t*)mem) = (uint16_t)shft_val;
        *((uint16_t*)(mem + size - WORD_SZ)) = (uint16_t)shft_val;
        return mem;
    }
    if (size == 1)
    {
        *((uint8_t*)mem) = (uint8_t)val;
    }
    return mem;
}

static inline void *_memset_avx2(void *mem, int val, size_t size)
{
    __m256i y0;
    size_t offset = 0;

    y0 = _mm256_set1_epi8(val);
    if (size < 4 * YMM_SZ)
    {
        _mm256_storeu_si256(mem , y0);
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

    size -= 4 * YMM_SZ;
    offset = 4 * YMM_SZ - ((uintptr_t)mem & (YMM_SZ - 1));

    if (size <= zen_info.zen_cache_info.l3_per_ccx)
    {
        while(offset < size)
        {
            _mm256_store_si256(mem + offset + 0 * YMM_SZ, y0);
            _mm256_store_si256(mem + offset + 1 * YMM_SZ, y0);
            _mm256_store_si256(mem + offset + 2 * YMM_SZ, y0);
            _mm256_store_si256(mem + offset + 3 * YMM_SZ, y0);
            offset += 4 * YMM_SZ;
        }
        return mem;
    }
    while (offset < size)
    {
        _mm256_stream_si256(mem + offset + 0 * YMM_SZ, y0);
        _mm256_stream_si256(mem + offset + 1 * YMM_SZ, y0);
        _mm256_stream_si256(mem + offset + 2 * YMM_SZ, y0);
        _mm256_stream_si256(mem + offset + 3 * YMM_SZ, y0);
        offset += 4 * YMM_SZ;
    }
    return mem;
}

static inline void *nt_store(void *mem, int val, size_t size)
{
    __m256i y0;
    size_t offset = 0;

    y0 = _mm256_set1_epi8(val);
    offset = YMM_SZ - ((uintptr_t)mem & (YMM_SZ - 1));
    _mm256_storeu_si256(mem, y0);
    size -= offset;

    while (size >= 4 * YMM_SZ)
    {
        _mm256_stream_si256(mem + offset + 0 * YMM_SZ, y0);
        _mm256_stream_si256(mem + offset + 1 * YMM_SZ, y0);
        _mm256_stream_si256(mem + offset + 2 * YMM_SZ, y0);
        _mm256_stream_si256(mem + offset + 3 * YMM_SZ, y0);

        size -= 4 * YMM_SZ;
        offset += 4 * YMM_SZ;
    }
    if (size >= 2 * YMM_SZ)
    {
        _mm256_stream_si256(mem + offset + 0 * YMM_SZ, y0);
        _mm256_stream_si256(mem + offset + 1 * YMM_SZ, y0);

        size -= 2 * YMM_SZ;
        offset += 2 * YMM_SZ;
    }

    if ((size > YMM_SZ))
    {
        _mm256_stream_si256(mem + offset, y0);
    }
    //copy last YMM_SZB
    _mm256_storeu_si256(mem + size - YMM_SZ + offset , y0);

    return mem;
}

#ifdef AVX512_FEATURE_ENABLED
static inline void *_memset_avx512(void *mem, int val, size_t size)
{
    __m512i z0;
    __m256i y0;
    size_t offset = 0;
    __mmask64 mask;

    z0 = _mm512_set1_epi8(val);
    if (size <= ZMM_SZ)
    {
        if (size)
        {
            mask = ((uint64_t)-1) >> (ZMM_SZ - size);
            _mm512_mask_storeu_epi8(mem, mask, z0);
        }
        return mem;
    }

    _mm512_storeu_si512(mem , z0);
    _mm512_storeu_si512(mem + size - ZMM_SZ, z0);

    if (size <= 2 * ZMM_SZ)
        return mem;

    _mm512_storeu_si512(mem + ZMM_SZ, z0);
    _mm512_storeu_si512(mem + size - 2 * ZMM_SZ, z0);

    if (size <= 4 * ZMM_SZ)
        return mem;

    _mm512_storeu_si512(mem + 2 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + 3 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + size - 4 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + size - 3 * ZMM_SZ, z0);

    if (size <= 8 * ZMM_SZ)
        return mem;

    size -= 4 * ZMM_SZ;
    offset =  4 * ZMM_SZ - ((uintptr_t)mem & (ZMM_SZ - 1));

    if (size < zen_info.zen_cache_info.l2_per_core)//L2 Cache Size
    {
        y0 = _mm256_set1_epi8(val);
        while (offset < size)
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
    while (offset < size)
    {
        _mm512_store_si512(mem + offset + 0 * ZMM_SZ, z0);
        _mm512_store_si512(mem + offset + 1 * ZMM_SZ, z0);
        _mm512_store_si512(mem + offset + 2 * ZMM_SZ, z0);
        _mm512_store_si512(mem + offset + 3 * ZMM_SZ, z0);
        offset += 4 * ZMM_SZ;
    }

    return mem;
}
#endif

void *__memset_zen1(void *mem, int val, size_t size)
{
    LOG_INFO("\n");
    if (size <= 2 * YMM_SZ)
        return memset_le_2ymm(mem, val, size);
    return _memset_avx2(mem, val, size);
}

void *__memset_zen2(void *mem, int val, size_t size)
{
    LOG_INFO("\n");
    if (size <= 2 * YMM_SZ)
        return memset_le_2ymm(mem, val, size);
    return _memset_avx2(mem, val, size);
}

void *__memset_zen3(void *mem, int val, size_t size)
{
    LOG_INFO("\n");
    if (size <= 2 * YMM_SZ)
        return memset_le_2ymm(mem, val, size);
    return _memset_avx2(mem, val, size);
}

void *__memset_zen4(void *mem, int val, size_t size)
{
    LOG_INFO("\n");
#ifdef AVX512_FEATURE_ENABLED
    return _memset_avx512(mem, val, size);
#else
    if (size <= 2 * YMM_SZ)
        return memset_le_2ymm(mem, val, size);
    return _memset_avx2(mem, val, size);
#endif
}

void * __memset_zen5 (void * mem, int val, size_t size)
                     __attribute__((alias("__memset_zen4")));

