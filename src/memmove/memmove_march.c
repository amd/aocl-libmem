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


static inline void *memmove_below_64(void *dst, const void *src, size_t size)
{
    __m256i y0, y1;
    __m128i x0, x1;
    uint64_t temp = 0;

    if (size == 0)
        return dst;

    if (size == 1)
    {
        *((uint8_t *)dst) = *((uint8_t *)src);
        return dst;
    }
    if (size <= 4)
    {
        temp = *((uint16_t *)src);
        *((uint16_t *)(dst + size - 2)) = *((uint16_t *)(src + size - 2));
        *((uint16_t *)dst) = temp;
        return dst;
    }
    if (size <= 8)
    {
        temp = *((uint32_t *)src);
        *((uint32_t *)(dst + size - 4)) = *((uint32_t *)(src + size - 4));
        *((uint32_t *)dst) = temp;
        return dst;
    }
    if (size <= 16)
    {
        temp = *((uint64_t *)src);
        *((uint64_t *)(dst + size - 8)) = *((uint64_t *)(src + size - 8));
        *((uint64_t *)dst) = temp;
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
    __m256i y0, y1, y2, y3, y4;
    size_t offset = 0;

    if ((dst < src + size) && (src < dst))
        goto BACKWARD_COPY; 
        
    y4 = _mm256_loadu_si256(src + size - 32);
    while ((size) > 127)
    {
        y0 = _mm256_loadu_si256(src + offset + 0 * 32);
        y1 = _mm256_loadu_si256(src + offset + 1 * 32);
        y2 = _mm256_loadu_si256(src + offset + 2 * 32);
        y3 = _mm256_loadu_si256(src + offset + 3 * 32);

        _mm256_storeu_si256(dst + offset + 0 * 32, y0);
        _mm256_storeu_si256(dst + offset + 1 * 32, y1);
        _mm256_storeu_si256(dst + offset + 2 * 32, y2);
        _mm256_storeu_si256(dst + offset + 3 * 32, y3);

        size -= 4 * 32;
        offset += 4 * 32 ;
    }
    if ((size) > 63)
    {
        y0 = _mm256_loadu_si256(src + offset + 0 * 32);
        y1 = _mm256_loadu_si256(src + offset + 1 * 32);

        _mm256_storeu_si256(dst + offset + 0 * 32, y0);
        _mm256_storeu_si256(dst + offset + 1 * 32, y1);

        size -= 2 * 32;
        offset += 2 * 32 ;
    }

    if ((size > 32))
    {
        y0 = _mm256_loadu_si256(src + offset);
        _mm256_storeu_si256(dst + offset, y0);
    }
    //copy last 32B
    if (size != 0)
        _mm256_storeu_si256(dst + size - 32 + offset, y4);
    return dst;

BACKWARD_COPY:

    y4 = _mm256_loadu_si256(src);
    while ((size) > 127)
    {
        y0 = _mm256_loadu_si256(src + size - 32);
        y1 = _mm256_loadu_si256(src + size - 64);
        y2 = _mm256_loadu_si256(src + size - 96);
        y3 = _mm256_loadu_si256(src + size - 128);

        _mm256_storeu_si256(dst + size - 32, y0);
        _mm256_storeu_si256(dst + size - 64, y1);
        _mm256_storeu_si256(dst + size - 96, y2);
        _mm256_storeu_si256(dst + size - 128, y3);

        size -= 4 * 32;
    }
    if ((size) > 63)
    {
        y0 = _mm256_loadu_si256(src + size - 32);
        y1 = _mm256_loadu_si256(src + size - 64);

        _mm256_storeu_si256(dst + size -32, y0);
        _mm256_storeu_si256(dst + size -64 , y1);

        size -= 2 * 32;
    }

    if ((size > 32))
    {
        y0 = _mm256_loadu_si256(src + size - 32);
        _mm256_storeu_si256(dst + size - 32, y0);
    }
    //copy first 32B
    if (size != 0)
        _mm256_storeu_si256(dst, y4);
    return dst;
}

static inline void *nt_store(void *dst, const void *src, size_t size)
{
    __m256i y0, y1, y2, y3, y4;
    size_t offset = 0;

    y4 = _mm256_loadu_si256(src + size - 32);
    //compute the offset to align the dst to 32B boundary
    offset = 0x20 - ((size_t)dst & 0x1F);
    y0 = _mm256_loadu_si256(src);
    _mm256_storeu_si256(dst, y0);
    size -= offset;

    while ((size) > 127)
    {
        y0 = _mm256_loadu_si256(src + offset + 0 * 32);
        y1 = _mm256_loadu_si256(src + offset + 1 * 32);
        y2 = _mm256_loadu_si256(src + offset + 2 * 32);
        y3 = _mm256_loadu_si256(src + offset + 3 * 32);

        _mm256_stream_si256(dst + offset + 0 * 32, y0);
        _mm256_stream_si256(dst + offset + 1 * 32, y1);
        _mm256_stream_si256(dst + offset + 2 * 32, y2);
        _mm256_stream_si256(dst + offset + 3 * 32, y3);

        size -= 4 * 32;
        offset += 4 * 32;
    }
    if ((size) > 63)
    {
        y0 = _mm256_loadu_si256(src + offset + 0 * 32);
        y1 = _mm256_loadu_si256(src + offset + 1 * 32);

        _mm256_stream_si256(dst + offset + 0 * 32, y0);
        _mm256_stream_si256(dst + offset + 1 * 32, y1);

        size -= 2 * 32;
        offset += 2 * 32;
    }

    if ((size > 32))
    {
        y0 = _mm256_loadu_si256(src + offset);
        _mm256_stream_si256(dst + offset, y0);
    }
    //copy last 32B
    if (size != 0)
        _mm256_storeu_si256(dst + size - 32 + offset, y4);
    return dst;
}


void *__memmove_zen1(void *dst, const void *src, size_t size)
{
    LOG_INFO("\n");
    if (size <= 64)
        return memmove_below_64(dst, src, size);
    if (size < 6*1024*1024)
        return unaligned_ld_st(dst, src, size);
    if (((src + size) < dst) || ((dst + size) < src))
        return nt_store(dst, src, size);
    return unaligned_ld_st(dst, src, size);
}
void *__memmove_zen2(void *dst, const void *src, size_t size)
{
    LOG_INFO("\n");
    if (size <= 64)
        return memmove_below_64(dst, src, size);
    if (size < 12*1024*1024)
        return unaligned_ld_st(dst, src, size);
    if (((src + size) < dst) || ((dst + size) < src))
        return nt_store(dst, src, size);
    return unaligned_ld_st(dst, src, size);
}

void *__memmove_zen3(void *dst, const void *src, size_t size)
{
    LOG_INFO("\n");
    if (size <= 64)
        return memmove_below_64(dst, src, size);
#ifdef ERMS_MICROCODE_FIXED //Disabling erms due to microcode bug:SWDEV-289143
    if (size < __repmove_stop_threshold)
        return __memmove_repmove_unaligned(dst, src, size);
#endif
    if (size < 24*1024*1024)//__nt_start_threshold)
        return unaligned_ld_st(dst, src, size);
    if (((src + size) < dst) || ((dst + size) < src))
        return nt_store(dst, src, size);
    return unaligned_ld_st(dst, src, size);
}
