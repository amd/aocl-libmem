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
#include "threshold.h"
#include <immintrin.h>
#include <stdint.h>

static inline void *memcpy_below_64(void *dst, const void *src, size_t size)
{
    __m256i y0, y1;
    __m128i x0, x1;

    if (size == 0)
        return dst;

    if (size == 1)
    {
        *((uint8_t *)dst) = *((uint8_t *)src);
        return dst;
    }
    if (size <= 4)
    {
        *((uint16_t *)dst) = *((uint16_t *)src);
        *((uint16_t *)(dst + size - 2)) = *((uint16_t *)(src + size - 2));
        return dst;
    }
    if (size <= 8)
    {
        *((uint32_t *)dst) = *((uint32_t *)src);
        *((uint32_t *)(dst + size - 4)) = *((uint32_t *)(src + size - 4));
        return dst;
    }
    if (size <= 16)
    {
        *((uint64_t *)dst) = *((uint64_t *)src);
        *((uint64_t *)(dst + size - 8)) = *((uint64_t *)(src + size - 8));
        return dst;
    }
    if (size <= 32)
    {
        x0 = _mm_loadu_si128(src);
        x1 = _mm_loadu_si128(src + size - 16);
        _mm_storeu_si128(dst, x0);
        _mm_storeu_si128(dst + size - 16, x1);
        return dst;
    }
    y0 = _mm256_loadu_si256(src);
    y1 = _mm256_loadu_si256(src + size - 32);
    _mm256_storeu_si256(dst, y0);
    _mm256_storeu_si256(dst + size - 32, y1);


    return dst;
}

static inline void *unaligned_ld_st(void *dst, const void *src, size_t size)
{
    __m256i y0, y1, y2, y3;
    size_t offset = 0;

    while ((size) > 127)
    {
        y0 = _mm256_loadu_si256(src + offset + 0 * 32);
        y1 = _mm256_loadu_si256(src + offset + 1 * 32);
        y2 = _mm256_loadu_si256(src + offset + 2 * 32);
        y3 = _mm256_loadu_si256(src + offset + 3 * 32);

        _mm256_storeu_si256 (dst + offset + 0 * 32, y0);
        _mm256_storeu_si256 (dst + offset + 1 * 32, y1);
        _mm256_storeu_si256 (dst + offset + 2 * 32, y2);
        _mm256_storeu_si256 (dst + offset + 3 * 32, y3);

        size -= 4 * 32;
        if (size == 0)
            return dst;
        offset += 4 * 32;
    }
    if ((size) > 63)
    {
        y0 = _mm256_loadu_si256(src + offset + 0 * 32);
        y1 = _mm256_loadu_si256(src + offset + 1 * 32);

        _mm256_storeu_si256 (dst + offset + 0 * 32, y0);
        _mm256_storeu_si256 (dst + offset + 1 * 32, y1);

        size -= 2 * 32;
        if (size == 0)
            return dst;
        offset += 2 * 32;
    }
    if ((size > 32))
    {
        y0 = _mm256_loadu_si256(src + offset);
        _mm256_storeu_si256 (dst + offset, y0);
    }
    //copy last 32B

    y0 = _mm256_loadu_si256(src + size + offset - 32);
    _mm256_storeu_si256 (dst + size - 32 + offset , y0);
    return dst;
}

static inline void *nt_store(void *dst, const void *src, size_t size)
{
    __m256i y0, y1, y2, y3;
    size_t offset = 0;

    //compute the offset to align the dst to 32B boundary
    offset = 0x20 - ((size_t)dst & 0x1F);
    y0 = _mm256_loadu_si256(src);
    _mm256_storeu_si256 (dst, y0);
    size -= offset;

    while ((size) > 127)
    {
        y0 = _mm256_loadu_si256(src + offset + 0 * 32);
        y1 = _mm256_loadu_si256(src + offset + 1 * 32);
        y2 = _mm256_loadu_si256(src + offset + 2 * 32);
        y3 = _mm256_loadu_si256(src + offset + 3 * 32);

        _mm256_stream_si256 (dst + offset + 0 * 32, y0);
        _mm256_stream_si256 (dst + offset + 1 * 32, y1);
        _mm256_stream_si256 (dst + offset + 2 * 32, y2);
        _mm256_stream_si256 (dst + offset + 3 * 32, y3);

        size -= 4 * 32;
        offset += 4 * 32;
    }
    if ((size) > 63)
    {
        y0 = _mm256_loadu_si256(src + offset + 0 * 32);
        y1 = _mm256_loadu_si256(src + offset + 1 * 32);

        _mm256_stream_si256 (dst + offset + 0 * 32, y0);
        _mm256_stream_si256 (dst + offset + 1 * 32, y1);

        size -= 2 * 32;
        offset += 2 * 32;
    }

    if ((size > 32))
    {
        y0 = _mm256_loadu_si256(src + offset);
        _mm256_stream_si256 (dst + offset, y0);
    }
    //copy last 32B

    y0 = _mm256_loadu_si256(src + size + offset - 32);
    _mm256_storeu_si256 (dst + size - 32 + offset , y0);
    return dst;
}


#ifdef AVX512_FEATURE_ENABLED
static inline void *memcpy_below_128(void *dst, const void *src, size_t size)
{
    __m512i z0, z1;

    if (size <= 64)
        return memcpy_below_64(dst, src, size);
    // above 64B uses ZMM registers
    z0 = _mm512_loadu_si512(src);
    z1 = _mm512_loadu_si512(src + size - 64);
    _mm512_storeu_si512(dst, z0);
    _mm512_storeu_si512(dst + size - 64, z1);

    return dst;
}

static inline void *unaligned_ld_st_avx512(void *dst, const void *src, size_t size)
{
    __m512i z0, z1, z2, z3;
    __m512i z4, z5, z6, z7;
    size_t offset = 0;

    if (size <= 256)
    {
        z0 = _mm512_loadu_si512(src + 0 * 64);
        z1 = _mm512_loadu_si512(src + 1 * 64);
        z2 = _mm512_loadu_si512(src + size - 2 * 64);
        z3 = _mm512_loadu_si512(src + size - 1 * 64);

        _mm512_storeu_si512 (dst + 0 * 64, z0);
        _mm512_storeu_si512 (dst + 1 * 64, z1);
        _mm512_storeu_si512 (dst + size - 2 * 64, z2);
        _mm512_storeu_si512 (dst + size - 1 * 64, z3);
        return dst;
    }
        z0 = _mm512_loadu_si512(src + 0 * 64);
        z1 = _mm512_loadu_si512(src + 1 * 64);
        z2 = _mm512_loadu_si512(src + 2 * 64);
        z3 = _mm512_loadu_si512(src + 3 * 64);
        z4 = _mm512_loadu_si512(src + size - 4 * 64);
        z5 = _mm512_loadu_si512(src + size - 3 * 64);
        z6 = _mm512_loadu_si512(src + size - 2 * 64);
        z7 = _mm512_loadu_si512(src + size - 1 * 64);

        _mm512_storeu_si512 (dst + 0 * 64, z0);
        _mm512_storeu_si512 (dst + 1 * 64, z1);
        _mm512_storeu_si512 (dst + 2 * 64, z2);
        _mm512_storeu_si512 (dst + 3 * 64, z3);
        _mm512_storeu_si512 (dst + size - 4 * 64, z4);
        _mm512_storeu_si512 (dst + size - 3 * 64, z5);
        _mm512_storeu_si512 (dst + size - 2 * 64, z6);
        _mm512_storeu_si512 (dst + size - 1 * 64, z7);

        size -= 256;
        offset +=256;

        while(offset < size)
        {
            z0 = _mm512_loadu_si512(src + offset + 0 * 64);
            z1 = _mm512_loadu_si512(src + offset + 1 * 64);
            z2 = _mm512_loadu_si512(src + offset + 2 * 64);
            z3 = _mm512_loadu_si512(src + offset + 3 * 64);
            _mm512_storeu_si512 (dst + offset + 0 * 64, z0);
            _mm512_storeu_si512 (dst + offset + 1 * 64, z1);
            _mm512_storeu_si512 (dst + offset + 2 * 64, z2);
            _mm512_storeu_si512 (dst + offset + 3 * 64, z3);
            offset += 256;
        }
    return dst;
}
#endif

void *__memcpy_zen1(void *dst, const void *src, size_t size)
{
    LOG_INFO("\n");
    if (size <= 64)
        return memcpy_below_64(dst, src, size);
    if (size < __nt_start_threshold)
        return unaligned_ld_st(dst, src, size);
    else
        return nt_store(dst, src, size);
}

void *__memcpy_zen2(void *dst, const void *src, size_t size)
{
    LOG_INFO("\n");
    if (size <= 64)
        return memcpy_below_64(dst, src, size);
    if (size < __nt_start_threshold)
        return unaligned_ld_st(dst, src, size);
    else
        return nt_store(dst, src, size);
}

void *__memcpy_zen3(void *dst, const void *src, size_t size)
{
    LOG_INFO("\n");
    if (size <= 64)
        return memcpy_below_64(dst, src, size);
    if (size < __nt_start_threshold)
        return unaligned_ld_st(dst, src, size);
    else
        return nt_store(dst, src, size);
}

void *__memcpy_zen4(void *dst, const void *src, size_t size)
{
    LOG_INFO("\n");
#ifdef AVX512_FEATURE_ENABLED
    if (size <= 128)
        return memcpy_below_128(dst, src, size);
    return unaligned_ld_st_avx512(dst, src, size);
#else
    if (size <= 64)
        return memcpy_below_64(dst, src, size);
    if (size < __nt_start_threshold)
        return unaligned_ld_st(dst, src, size);
    else
        return nt_store(dst, src, size);
#endif
}
