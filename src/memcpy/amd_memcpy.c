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
    void * src_index = (void*)src + size;
    void * dst_index = (void*)dst + size;

    if (size == 0)
        return dst;
    if (size == 1)
    {
        *((uint8_t *)dst) = *((uint8_t *)src);
        return dst;
    }
    if (size < 4)
    {
        *((uint16_t *)dst) = *((uint16_t *)src);
        *((uint16_t *)(dst_index - 2)) = *((uint16_t *)(src_index - 2));
        return dst;
    }
    if (size < 8)
    {
        *((uint32_t *)dst) = *((uint32_t *)src);
        *((uint32_t *)(dst_index - 4)) = *((uint32_t *)(src_index - 4));
        return dst;
    }
    if (size < 16)
    {
        *((uint64_t *)dst) = *((uint64_t *)src);
        *((uint64_t *)(dst_index - 8)) = *((uint64_t *)(src_index - 8));
        return dst;
    }
    if (size < 32)
    {
        x0 = _mm_loadu_si128(src);
        x1 = _mm_loadu_si128(src_index - 16);
        _mm_storeu_si128(dst, x0);
        _mm_storeu_si128(dst_index - 16, x1);
        return dst;
    }
    y0 = _mm256_loadu_si256(src);
    y1 = _mm256_loadu_si256(src_index - 32);
    _mm256_storeu_si256(dst, y0);
    _mm256_storeu_si256(dst_index - 32, y1);
    return dst;
}

#ifdef AVX512_FEATURE_ENABLED
static inline void *memcpy_below_128(void *dst, const void *src, size_t size)
{
    __m512i z0, z1;

    if (size < 64)
        return memcpy_below_64(dst, src, size);
    // above 64B uses ZMM registers
    z0 = _mm512_loadu_si512(src);
    z1 = _mm512_loadu_si512(src + size - 64);
    _mm512_storeu_si512(dst, z0);
    _mm512_storeu_si512(dst + size - 64, z1);

    return dst;
}
#endif

static inline void *unaligned_ld_st_avx2(void *dst, const void *src, size_t size)
{
    __m256i y0, y1, y2, y3;
    __m256i y4, y5, y6, y7;
    void * stop_addr;
    void * src_index = (void*)src + size;
    void * dst_index = (void*)dst + size;

    if (size <= 128)
    {
        y0 = _mm256_loadu_si256(src + 0 * 32);
        y1 = _mm256_loadu_si256(src + 1 * 32);
        y2 = _mm256_loadu_si256(src_index - 2 * 32);
        y3 = _mm256_loadu_si256(src_index - 1 * 32);

        _mm256_storeu_si256 (dst + 0 * 32, y0);
        _mm256_storeu_si256 (dst + 1 * 32, y1);
        _mm256_storeu_si256 (dst_index - 2 * 32, y2);
        _mm256_storeu_si256 (dst_index - 1 * 32, y3);
        return dst;
    }
    y0 = _mm256_loadu_si256(src + 0 * 32);
    y1 = _mm256_loadu_si256(src + 1 * 32);
    y2 = _mm256_loadu_si256(src + 2 * 32);
    y3 = _mm256_loadu_si256(src + 3 * 32);
    y4 = _mm256_loadu_si256(src_index - 4 * 32);
    y5 = _mm256_loadu_si256(src_index - 3 * 32);
    y6 = _mm256_loadu_si256(src_index - 2 * 32);
    y7 = _mm256_loadu_si256(src_index - 1 * 32);

    _mm256_storeu_si256 (dst + 0 * 32, y0);
    _mm256_storeu_si256 (dst + 1 * 32, y1);
    _mm256_storeu_si256 (dst + 2 * 32, y2);
    _mm256_storeu_si256 (dst + 3 * 32, y3);
    _mm256_storeu_si256 (dst_index - 4 * 32, y4);
    _mm256_storeu_si256 (dst_index - 3 * 32, y5);
    _mm256_storeu_si256 (dst_index - 2 * 32, y6);
    _mm256_storeu_si256 (dst_index - 1 * 32, y7);

    stop_addr = src_index - 128;
    src_index = (void*)src + 128;
    dst_index = (void*)dst + 128;

    while(src_index < stop_addr)
    {
        y0 = _mm256_loadu_si256(src_index + 0 * 32);
        y1 = _mm256_loadu_si256(src_index + 1 * 32);
        y2 = _mm256_loadu_si256(src_index + 2 * 32);
        y3 = _mm256_loadu_si256(src_index + 3 * 32);
        _mm256_storeu_si256 (dst_index + 0 * 32, y0);
        _mm256_storeu_si256 (dst_index + 1 * 32, y1);
        _mm256_storeu_si256 (dst_index + 2 * 32, y2);
        _mm256_storeu_si256 (dst_index + 3 * 32, y3);
        src_index += 128;
        dst_index += 128;
    }
    return dst;
}

#ifdef AVX512_FEATURE_ENABLED
static inline void *unaligned_ld_st_avx512(void *dst, const void *src, size_t size)
{
    __m512i z0, z1, z2, z3;
    __m512i z4, z5, z6, z7;
    void * stop_addr;
    void * src_index = (void*)src + size;
    void * dst_index = (void*)dst + size;

    if (size <= 256)
    {
        z0 = _mm512_loadu_si512(src + 0 * 64);
        z1 = _mm512_loadu_si512(src + 1 * 64);
        z2 = _mm512_loadu_si512(src_index - 2 * 64);
        z3 = _mm512_loadu_si512(src_index - 1 * 64);

        _mm512_storeu_si512 (dst + 0 * 64, z0);
        _mm512_storeu_si512 (dst + 1 * 64, z1);
        _mm512_storeu_si512 (dst_index - 2 * 64, z2);
        _mm512_storeu_si512 (dst_index - 1 * 64, z3);
        return dst;
    }
    z0 = _mm512_loadu_si512(src + 0 * 64);
    z1 = _mm512_loadu_si512(src + 1 * 64);
    z2 = _mm512_loadu_si512(src + 2 * 64);
    z3 = _mm512_loadu_si512(src + 3 * 64);
    z4 = _mm512_loadu_si512(src_index - 4 * 64);
    z5 = _mm512_loadu_si512(src_index - 3 * 64);
    z6 = _mm512_loadu_si512(src_index - 2 * 64);
    z7 = _mm512_loadu_si512(src_index - 1 * 64);

    _mm512_storeu_si512 (dst + 0 * 64, z0);
    _mm512_storeu_si512 (dst + 1 * 64, z1);
    _mm512_storeu_si512 (dst + 2 * 64, z2);
    _mm512_storeu_si512 (dst + 3 * 64, z3);
    _mm512_storeu_si512 (dst_index - 4 * 64, z4);
    _mm512_storeu_si512 (dst_index - 3 * 64, z5);
    _mm512_storeu_si512 (dst_index - 2 * 64, z6);
    _mm512_storeu_si512 (dst_index - 1 * 64, z7);

    stop_addr = src_index - 256;
    src_index = (void*)src + 256;
    dst_index = (void*)dst + 256;

    while(src_index < stop_addr)
    {
        z0 = _mm512_loadu_si512(src_index + 0 * 64);
        z1 = _mm512_loadu_si512(src_index + 1 * 64);
        z2 = _mm512_loadu_si512(src_index + 2 * 64);
        z3 = _mm512_loadu_si512(src_index + 3 * 64);
        _mm512_storeu_si512 (dst_index + 0 * 64, z0);
        _mm512_storeu_si512 (dst_index + 1 * 64, z1);
        _mm512_storeu_si512 (dst_index + 2 * 64, z2);
        _mm512_storeu_si512 (dst_index + 3 * 64, z3);
        src_index += 128;
        dst_index += 128;
    }
    return dst;
}
#endif

static inline void *nt_store_avx2(void *dst, const void *src, size_t size)
{
    __m256i y0, y1, y2, y3;
    size_t offset = 0;
    void * src_index = (void*)src;
    void * dst_index = (void*)dst;

    //compute the offset to align the dst to 32B boundary
    offset = 0x20 - ((size_t)dst & 0x1F);
    y0 = _mm256_loadu_si256(src);
    _mm256_storeu_si256 (dst, y0);
    size -= offset;

    src_index += offset;
    dst_index += offset;

    while ((size) > 127)
    {
        y0 = _mm256_loadu_si256(src_index + 0 * 32);
        y1 = _mm256_loadu_si256(src_index + 1 * 32);
        y2 = _mm256_loadu_si256(src_index + 2 * 32);
        y3 = _mm256_loadu_si256(src_index + 3 * 32);

        _mm256_stream_si256 (dst_index + 0 * 32, y0);
        _mm256_stream_si256 (dst_index + 1 * 32, y1);
        _mm256_stream_si256 (dst_index + 2 * 32, y2);
        _mm256_stream_si256 (dst_index + 3 * 32, y3);

        size -= 4 * 32;
        src_index += 4 * 32;
        dst_index += 4 * 32;
    }
    if ((size) > 63)
    {
        y0 = _mm256_loadu_si256(src_index + 0 * 32);
        y1 = _mm256_loadu_si256(src_index + 1 * 32);

        _mm256_stream_si256 (dst_index + 0 * 32, y0);
        _mm256_stream_si256 (dst_index + 1 * 32, y1);

        size -= 2 * 32;
        src_index += 2 * 32;
        dst_index += 2 * 32;
    }

    if ((size > 32))
    {
        y0 = _mm256_loadu_si256(src_index);
        _mm256_stream_si256 (dst_index, y0);
    }
    //copy last 32B
    y0 = _mm256_loadu_si256(src_index + size - 32);
    _mm256_storeu_si256 (dst_index - 32 + size , y0);

    return dst;
}

void *amd_memcpy(void *dst, const void *src, size_t size)
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
        return unaligned_ld_st_avx2(dst, src, size);
    else
        return nt_store_avx2(dst, src, size);
#endif
}

void *memcpy(void *, const void *, size_t) __attribute__((weak, alias("amd_memcpy"), visibility("default")));
