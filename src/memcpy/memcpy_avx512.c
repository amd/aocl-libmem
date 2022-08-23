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

static inline void *memcpy_below_128(void *dst, const void *src, size_t size)
{
    __m512i z0, z1;
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
    if (size <= 64)
    {
        y0 = _mm256_loadu_si256(src);
        y1 = _mm256_loadu_si256(src + size - 32);
        _mm256_storeu_si256(dst, y0);
        _mm256_storeu_si256(dst + size - 32, y1);
    }
    // above 64B uses ZMM registers
    z0 = _mm512_loadu_si512(src);
    z1 = _mm512_loadu_si512(src + size - 64);
    _mm512_storeu_si512(dst, z0);
    _mm512_storeu_si512(dst + size - 64, z1);

    return dst;
}

void *__memcpy_avx512_unaligned(void *dst, const void *src, size_t size)
{
    __m512i z0, z1, z2, z3;
    size_t offset = 0;
    LOG_INFO("\n");

    if (size <= 128)
        return memcpy_below_128(dst, src, size);

    while ((size) > 255)
    {
        z0 = _mm512_loadu_si512(src + offset + 0 * 64);
        z1 = _mm512_loadu_si512(src + offset + 1 * 64);
        z2 = _mm512_loadu_si512(src + offset + 2 * 64);
        z3 = _mm512_loadu_si512(src + offset + 3 * 64);

        _mm512_storeu_si512(dst + offset + 0 * 64, z0);
        _mm512_storeu_si512(dst + offset + 1 * 64, z1);
        _mm512_storeu_si512(dst + offset + 2 * 64, z2);
        _mm512_storeu_si512(dst + offset + 3 * 64, z3);

        size -= 4 * 64;
        offset += 4 * 64;
    }
    if ((size) > 127)
    {
        z0 = _mm512_loadu_si512(src + offset + 0 * 64);
        z1 = _mm512_loadu_si512(src + offset + 1 * 64);

        _mm512_storeu_si512(dst + offset + 0 * 64, z0);
        _mm512_storeu_si512(dst + offset + 1 * 64, z1);

        size -= 2 * 64;
        offset += 2 * 64;
    }

    if ((size > 64))
    {
        z0 = _mm512_loadu_si512(src + offset);
        _mm512_storeu_si512(dst + offset, z0);
    }
    //copy last 64B
    z0 = _mm512_loadu_si512(src + size + offset - 64);
    _mm512_storeu_si512(dst + size - 64 + offset , z0);
    return dst;
}

void *__memcpy_avx512_aligned(void *dst, const void *src, size_t size)
{
    __m512i z0, z1, z2, z3;
    size_t offset = 0;

    LOG_INFO("\n");
    if (size <= 128)
        return memcpy_below_128(dst, src, size);

    while ((size) > 255)
    {
        z0 = _mm512_load_si512(src + offset + 0 * 64);
        z1 = _mm512_load_si512(src + offset + 1 * 64);
        z2 = _mm512_load_si512(src + offset + 2 * 64);
        z3 = _mm512_load_si512(src + offset + 3 * 64);

        _mm512_store_si512(dst + offset + 0 * 64, z0);
        _mm512_store_si512(dst + offset + 1 * 64, z1);
        _mm512_store_si512(dst + offset + 2 * 64, z2);
        _mm512_store_si512(dst + offset + 3 * 64, z3);

        size -= 4 * 64;
        offset += 4 * 64;
    }
    if ((size) > 127)
    {
        z0 = _mm512_load_si512(src + offset + 0 * 64);
        z1 = _mm512_load_si512(src + offset + 1 * 64);

        _mm512_store_si512(dst + offset + 0 * 64, z0);
        _mm512_store_si512(dst + offset + 1 * 64, z1);

        size -= 2 * 64;
        offset += 2 * 64;
    }

    if ((size > 64))
    {
        z0 = _mm512_load_si512(src + offset);
        _mm512_store_si512(dst + offset, z0);
    }
    //copy last 64B
    z0 = _mm512_loadu_si512(src + size + offset - 64);
    _mm512_storeu_si512(dst + size - 64 + offset , z0);
    return dst;
}

void *__memcpy_avx512_aligned_load(void *dst, const void *src, size_t size)
{
    __m512i z0, z1, z2, z3;
    size_t offset = 0;

    LOG_INFO("\n");
    if (size <= 128)
        return memcpy_below_128(dst, src, size);

    //compute the offset to align the src to 64B boundary
    offset = 0x40 - ((size_t)src & 0x3F);
    z0 = _mm512_loadu_si512(src);
    _mm512_storeu_si512 (dst, z0);
    size -= offset;

    while ((size) > 255)
    {
        z0 = _mm512_load_si512(src + offset + 0 * 64);
        z1 = _mm512_load_si512(src + offset + 1 * 64);
        z2 = _mm512_load_si512(src + offset + 2 * 64);
        z3 = _mm512_load_si512(src + offset + 3 * 64);

        _mm512_storeu_si512(dst + offset + 0 * 64, z0);
        _mm512_storeu_si512(dst + offset + 1 * 64, z1);
        _mm512_storeu_si512(dst + offset + 2 * 64, z2);
        _mm512_storeu_si512(dst + offset + 3 * 64, z3);

        size -= 4 * 64;
        offset += 4 * 64;
    }
    if ((size) > 127)
    {
        z0 = _mm512_load_si512(src + offset + 0 * 64);
        z1 = _mm512_load_si512(src + offset + 1 * 64);

        _mm512_storeu_si512(dst + offset + 0 * 64, z0);
        _mm512_storeu_si512(dst + offset + 1 * 64, z1);

        size -= 2 * 64;
        offset += 2 * 64;
    }

    if ((size > 64))
    {
        z0 = _mm512_load_si512(src + offset);
        _mm512_storeu_si512 (dst + offset, z0);
    }
    //copy last 64B

    z0 = _mm512_loadu_si512(src + size + offset - 64);
    _mm512_storeu_si512 (dst + size - 64 + offset , z0);
    return dst;
}

void *__memcpy_avx512_aligned_store(void *dst, const void *src, size_t size)
{
    __m512i z0, z1, z2, z3;
    size_t offset = 0;

    LOG_INFO("\n");
    if (size <= 128)
        return memcpy_below_128(dst, src, size);

    //compute the offset to align the dst to 64B boundary
    offset = 0x40 - ((size_t)dst & 0x3F);
    z0 = _mm512_loadu_si512(src);
    _mm512_storeu_si512 (dst, z0);
    size -= offset;

    while ((size) > 255)
    {
        z0 = _mm512_loadu_si512(src + offset + 0 * 64);
        z1 = _mm512_loadu_si512(src + offset + 1 * 64);
        z2 = _mm512_loadu_si512(src + offset + 2 * 64);
        z3 = _mm512_loadu_si512(src + offset + 3 * 64);

        _mm512_store_si512(dst + offset + 0 * 64, z0);
        _mm512_store_si512(dst + offset + 1 * 64, z1);
        _mm512_store_si512(dst + offset + 2 * 64, z2);
        _mm512_store_si512(dst + offset + 3 * 64, z3);

        size -= 4 * 64;
        offset += 4 * 64;
    }
    if ((size) > 127)
    {
        z0 = _mm512_loadu_si512(src + offset + 0 * 64);
        z1 = _mm512_loadu_si512(src + offset + 1 * 64);

        _mm512_store_si512(dst + offset + 0 * 64, z0);
        _mm512_store_si512(dst + offset + 1 * 64, z1);

        size -= 2 * 64;
        offset += 2 * 64;
    }

    if ((size > 64))
    {
        z0 = _mm512_loadu_si512(src + offset);
        _mm512_storeu_si512(dst + offset, z0);
    }
    //copy last 64B

    z0 = _mm512_loadu_si512(src + size + offset - 64);
    _mm512_storeu_si512(dst + size - 64 + offset , z0);
    return dst;
}

void *__memcpy_avx512_nt(void *dst, const void *src, size_t size)
{
    __m512i z0, z1, z2, z3;
    size_t offset = 0;

    LOG_INFO("\n");
    if (size <= 128)
        return memcpy_below_128(dst, src, size);

    while ((size) > 255)
    {
        z0 = _mm512_stream_load_si512((void*)src + offset + 0 * 64);
        z1 = _mm512_stream_load_si512((void*)src + offset + 1 * 64);
        z2 = _mm512_stream_load_si512((void*)src + offset + 2 * 64);
        z3 = _mm512_stream_load_si512((void*)src + offset + 3 * 64);

        _mm512_stream_si512(dst + offset + 0 * 64, z0);
        _mm512_stream_si512(dst + offset + 1 * 64, z1);
        _mm512_stream_si512(dst + offset + 2 * 64, z2);
        _mm512_stream_si512(dst + offset + 3 * 64, z3);

        size -= 4 * 64;
        offset += 4 * 64;
    }
    if ((size) > 127)
    {
        z0 = _mm512_stream_load_si512((void*)src + offset + 0 * 64);
        z1 = _mm512_stream_load_si512((void*)src + offset + 1 * 64);

        _mm512_stream_si512(dst + offset + 0 * 64, z0);
        _mm512_stream_si512(dst + offset + 1 * 64, z1);

        size -= 2 * 64;
        offset += 2 * 64;
    }

    if ((size >= 64))
    {
        z0 = _mm512_stream_load_si512((void*)src + offset);
        _mm512_stream_si512(dst + offset, z0);
    }
    //copy last 64B

    z0 = _mm512_loadu_si512(src + size + offset - 64);
    _mm512_storeu_si512(dst + size - 64 + offset , z0);
    return dst;

}
void *__memcpy_avx512_nt_load(void *dst, const void *src, size_t size)
{
    __m512i z0, z1, z2, z3;
    size_t offset = 0;

    LOG_INFO("\n");
    if (size <= 128)
        return memcpy_below_128(dst, src, size);

    //compute the offset to align the src to 64B boundary
    offset = 0x40 - ((size_t)src & 0x3F);
    z0 = _mm512_loadu_si512(src);
    _mm512_storeu_si512 (dst, z0);
    size -= offset;

    while ((size) > 255)
    {
        z0 = _mm512_stream_load_si512((void*)src + offset + 0 * 64);
        z1 = _mm512_stream_load_si512((void*)src + offset + 1 * 64);
        z2 = _mm512_stream_load_si512((void*)src + offset + 2 * 64);
        z3 = _mm512_stream_load_si512((void*)src + offset + 3 * 64);

        _mm512_storeu_si512(dst + offset + 0 * 64, z0);
        _mm512_storeu_si512(dst + offset + 1 * 64, z1);
        _mm512_storeu_si512(dst + offset + 2 * 64, z2);
        _mm512_storeu_si512(dst + offset + 3 * 64, z3);

        size -= 4 * 64;
        offset += 4 * 64;
    }
    if ((size) > 127)
    {
        z0 = _mm512_stream_load_si512((void*)src + offset + 0 * 64);
        z1 = _mm512_stream_load_si512((void*)src + offset + 1 * 64);

        _mm512_storeu_si512(dst + offset + 0 * 64, z0);
        _mm512_storeu_si512(dst + offset + 1 * 64, z1);

        size -= 2 * 64;
        offset += 2 * 64;
    }

    if ((size > 64))
    {
        z0 = _mm512_stream_load_si512((void*)src + offset);
        _mm512_storeu_si512(dst + offset, z0);
    }
    //copy last 64B
    z0 = _mm512_loadu_si512(src + size + offset - 64);
    _mm512_storeu_si512(dst + size - 64 + offset , z0);
    return dst;
}


void *__memcpy_avx512_nt_store(void *dst, const void *src, size_t size)
{
    __m512i z0, z1, z2, z3;
    size_t offset = 0;

    LOG_INFO("\n");
    if (size <= 128)
        return memcpy_below_128(dst, src, size);

    //compute the offset to align the dst to 64B boundary
    offset = 0x40 - ((size_t)dst & 0x3F);
    z0 = _mm512_loadu_si512(src);
    _mm512_storeu_si512 (dst, z0);
    size -= offset;

    while ((size) > 255)
    {
        z0 = _mm512_loadu_si512(src + offset + 0 * 64);
        z1 = _mm512_loadu_si512(src + offset + 1 * 64);
        z2 = _mm512_loadu_si512(src + offset + 2 * 64);
        z3 = _mm512_loadu_si512(src + offset + 3 * 64);

        _mm512_stream_si512 (dst + offset + 0 * 64, z0);
        _mm512_stream_si512 (dst + offset + 1 * 64, z1);
        _mm512_stream_si512 (dst + offset + 2 * 64, z2);
        _mm512_stream_si512 (dst + offset + 3 * 64, z3);

        size -= 4 * 64;
        offset += 4 * 64;
    }
    if ((size) > 127)
    {
        z0 = _mm512_loadu_si512(src + offset + 0 * 64);
        z1 = _mm512_loadu_si512(src + offset + 1 * 64);

        _mm512_stream_si512 (dst + offset + 0 * 64, z0);
        _mm512_stream_si512 (dst + offset + 1 * 64, z1);

        size -= 2 * 64;
        offset += 2 * 64;
    }

    if ((size > 64))
    {
        z0 = _mm512_loadu_si512(src + offset);
        _mm512_stream_si512 (dst + offset, z0);
    }
    //copy last 64B

    z0 = _mm512_loadu_si512(src + size + offset - 64);
    _mm512_storeu_si512 (dst + size - 64 + offset , z0);
    return dst;
}
