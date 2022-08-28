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
#include "zen_cpu_info.h"

static inline void *mempcpy_le_2ymm(void *dst, const void *src, size_t size)
{
    __m256i y0, y1;
    __m128i x0, x1;

    if (size == 0)
        return (dst + size);

    if (size == 1)
    {
        *((uint8_t *)dst) = *((uint8_t *)src);
        return (dst + size);
    }
    if (size <= 2 * WORD_SZ)
    {
        *((uint16_t *)dst) = *((uint16_t *)src);
        *((uint16_t *)(dst + size - WORD_SZ)) = *((uint16_t *)(src + size - WORD_SZ));
        return (dst + size);
    }
    if (size <= 2 * DWORD_SZ)
    {
        *((uint32_t *)dst) = *((uint32_t *)src);
        *((uint32_t *)(dst + size - DWORD_SZ)) = *((uint32_t *)(src + size - DWORD_SZ));
        return (dst + size);
    }
    if (size <= 2 * QWORD_SZ)
    {
        *((uint64_t *)dst) = *((uint64_t *)src);
        *((uint64_t *)(dst + size - QWORD_SZ)) = *((uint64_t *)(src + size - QWORD_SZ));
        return (dst + size);
    }
    if (size <= 2 * XMM_SZ)
    {
        x0 = _mm_loadu_si128(src);
        x1 = _mm_loadu_si128(src + size - XMM_SZ);
        _mm_storeu_si128(dst, x0);
        _mm_storeu_si128(dst + size - XMM_SZ, x1);
        return (dst + size);
    }
    y0 = _mm256_loadu_si256(src);
    y1 = _mm256_loadu_si256(src + size - YMM_SZ);
    _mm256_storeu_si256(dst, y0);
    _mm256_storeu_si256(dst + size - YMM_SZ, y1);


    return (dst + size);
}

void *__mempcpy_avx2_unaligned(void *dst, const void *src, size_t size)
{
    __m256i y0, y1, y2, y3;
    size_t offset = 0;
    LOG_INFO("\n");

    if (size <= 2 * YMM_SZ)
        return mempcpy_le_2ymm(dst, src, size);

    while (size >= 4 * YMM_SZ)
    {
        y0 = _mm256_loadu_si256(src + offset + 0 * YMM_SZ);
        y1 = _mm256_loadu_si256(src + offset + 1 * YMM_SZ);
        y2 = _mm256_loadu_si256(src + offset + 2 * YMM_SZ);
        y3 = _mm256_loadu_si256(src + offset + 3 * YMM_SZ);

        _mm256_storeu_si256 (dst + offset + 0 * YMM_SZ, y0);
        _mm256_storeu_si256 (dst + offset + 1 * YMM_SZ, y1);
        _mm256_storeu_si256 (dst + offset + 2 * YMM_SZ, y2);
        _mm256_storeu_si256 (dst + offset + 3 * YMM_SZ, y3);

        size -= 4 * YMM_SZ;
        offset += 4 * YMM_SZ;
        if (size == 0)
            return (dst + size + offset);
    }
    if (size >= 2 * YMM_SZ)
    {
        y0 = _mm256_loadu_si256(src + offset + 0 * YMM_SZ);
        y1 = _mm256_loadu_si256(src + offset + 1 * YMM_SZ);

        _mm256_storeu_si256 (dst + offset + 0 * YMM_SZ, y0);
        _mm256_storeu_si256 (dst + offset + 1 * YMM_SZ, y1);

        size -= 2 * YMM_SZ;
        offset += 2 * YMM_SZ;
        if (size == 0)
            return (dst + size + offset);
    }

    if ((size > YMM_SZ))
    {
        y0 = _mm256_loadu_si256(src + offset);
        _mm256_storeu_si256 (dst + offset, y0);
    }
    //copy last YMM_SZ Bytes

    y0 = _mm256_loadu_si256(src + size + offset - YMM_SZ);
    _mm256_storeu_si256 (dst + size - YMM_SZ + offset , y0);
    return (dst + size + offset);
}

void *__mempcpy_avx2_aligned(void *dst, const void *src, size_t size)
{
    __m256i y0, y1, y2, y3;
    size_t offset = 0;

    LOG_INFO("\n");
    if (size <= 2 * YMM_SZ)
        return mempcpy_le_2ymm(dst, src, size);

    while (size >= 4 * YMM_SZ)
    {
        y0 = _mm256_load_si256(src + offset + 0 * YMM_SZ);
        y1 = _mm256_load_si256(src + offset + 1 * YMM_SZ);
        y2 = _mm256_load_si256(src + offset + 2 * YMM_SZ);
        y3 = _mm256_load_si256(src + offset + 3 * YMM_SZ);

        _mm256_store_si256 (dst + offset + 0 * YMM_SZ, y0);
        _mm256_store_si256 (dst + offset + 1 * YMM_SZ, y1);
        _mm256_store_si256 (dst + offset + 2 * YMM_SZ, y2);
        _mm256_store_si256 (dst + offset + 3 * YMM_SZ, y3);

        size -= 4 * YMM_SZ;
        offset += 4 * YMM_SZ;
        if (size == 0)
            return (dst + size + offset);
    }
    if (size >= 2 * YMM_SZ)
    {
        y0 = _mm256_load_si256(src + offset + 0 * YMM_SZ);
        y1 = _mm256_load_si256(src + offset + 1 * YMM_SZ);

        _mm256_store_si256 (dst + offset + 0 * YMM_SZ, y0);
        _mm256_store_si256 (dst + offset + 1 * YMM_SZ, y1);

        size -= 2 * YMM_SZ;
        offset += 2 * YMM_SZ;
        if (size == 0)
            return (dst + size + offset);
    }

    if ((size > YMM_SZ))
    {
        y0 = _mm256_load_si256(src + offset);
        _mm256_store_si256 (dst + offset, y0);
    }
    //copy last YMM_SZ Bytes

    y0 = _mm256_loadu_si256(src + size + offset - YMM_SZ);
    _mm256_storeu_si256 (dst + size - YMM_SZ + offset , y0);
    return (dst + size + offset);
}

void *__mempcpy_avx2_aligned_load(void *dst, const void *src, size_t size)
{
    __m256i y0, y1, y2, y3;
    size_t offset = 0;

    LOG_INFO("\n");
    if (size <= 2 * YMM_SZ)
        return mempcpy_le_2ymm(dst, src, size);

    //compute the offset to align the src to YMM_SZ Bytes boundary
    offset = YMM_SZ - ((size_t)src & (YMM_SZ - 1));
    y0 = _mm256_loadu_si256(src);
    _mm256_storeu_si256 (dst, y0);
    size -= offset;

    while (size >= 4 * YMM_SZ)
    {
        y0 = _mm256_load_si256(src + offset + 0 * YMM_SZ);
        y1 = _mm256_load_si256(src + offset + 1 * YMM_SZ);
        y2 = _mm256_load_si256(src + offset + 2 * YMM_SZ);
        y3 = _mm256_load_si256(src + offset + 3 * YMM_SZ);

        _mm256_storeu_si256 (dst + offset + 0 * YMM_SZ, y0);
        _mm256_storeu_si256 (dst + offset + 1 * YMM_SZ, y1);
        _mm256_storeu_si256 (dst + offset + 2 * YMM_SZ, y2);
        _mm256_storeu_si256 (dst + offset + 3 * YMM_SZ, y3);

        size -= 4 * YMM_SZ;
        offset += 4 * YMM_SZ;
        if (size == 0)
            return (dst + size + offset);
    }
    if (size >= 2 * YMM_SZ)
    {
        y0 = _mm256_load_si256(src + offset + 0 * YMM_SZ);
        y1 = _mm256_load_si256(src + offset + 1 * YMM_SZ);

        _mm256_storeu_si256 (dst + offset + 0 * YMM_SZ, y0);
        _mm256_storeu_si256 (dst + offset + 1 * YMM_SZ, y1);

        size -= 2 * YMM_SZ;
        offset += 2 * YMM_SZ;
        if (size == 0)
            return (dst + size + offset);
    }

    if ((size > YMM_SZ))
    {
        y0 = _mm256_load_si256(src + offset);
        _mm256_storeu_si256 (dst + offset, y0);
    }
    //copy last YMM_SZ Bytes

    y0 = _mm256_loadu_si256(src + size + offset - YMM_SZ);
    _mm256_storeu_si256 (dst + size - YMM_SZ + offset , y0);
    return (dst + size + offset);
}

void *__mempcpy_avx2_aligned_store(void *dst, const void *src, size_t size)
{
    __m256i y0, y1, y2, y3;
    size_t offset = 0;

    LOG_INFO("\n");
    if (size <= 2 * YMM_SZ)
        return mempcpy_le_2ymm(dst, src, size);

    //compute the offset to align the dst to YMM_SZ Bytes boundary
    offset = YMM_SZ - ((size_t)dst & (YMM_SZ - 1));
    y0 = _mm256_loadu_si256(src);
    _mm256_storeu_si256 (dst, y0);
    size -= offset;

    while (size >= 4 * YMM_SZ)
    {
        y0 = _mm256_loadu_si256(src + offset + 0 * YMM_SZ);
        y1 = _mm256_loadu_si256(src + offset + 1 * YMM_SZ);
        y2 = _mm256_loadu_si256(src + offset + 2 * YMM_SZ);
        y3 = _mm256_loadu_si256(src + offset + 3 * YMM_SZ);

        _mm256_store_si256 (dst + offset + 0 * YMM_SZ, y0);
        _mm256_store_si256 (dst + offset + 1 * YMM_SZ, y1);
        _mm256_store_si256 (dst + offset + 2 * YMM_SZ, y2);
        _mm256_store_si256 (dst + offset + 3 * YMM_SZ, y3);

        size -= 4 * YMM_SZ;
        offset += 4 * YMM_SZ;
        if (size == 0)
            return (dst + size + offset);
    }
    if (size >= 2 * YMM_SZ)
    {
        y0 = _mm256_loadu_si256(src + offset + 0 * YMM_SZ);
        y1 = _mm256_loadu_si256(src + offset + 1 * YMM_SZ);

        _mm256_store_si256 (dst + offset + 0 * YMM_SZ, y0);
        _mm256_store_si256 (dst + offset + 1 * YMM_SZ, y1);

        size -= 2 * YMM_SZ;
        offset += 2 * YMM_SZ;
        if (size == 0)
            return (dst + size + offset);
    }

    if ((size > YMM_SZ))
    {
        y0 = _mm256_loadu_si256(src + offset);
        _mm256_storeu_si256 (dst + offset, y0);
    }
    //copy last YMM_SZ Bytes

    y0 = _mm256_loadu_si256(src + size + offset - YMM_SZ);
    _mm256_storeu_si256 (dst + size - YMM_SZ + offset , y0);
    return (dst + size + offset);
}

void *__mempcpy_avx2_nt(void *dst, const void *src, size_t size)
{
    __m256i y0, y1, y2, y3;
    size_t offset = 0;

    LOG_INFO("\n");
    if (size <= 2 * YMM_SZ)
        return mempcpy_le_2ymm(dst, src, size);

    while (size >= 4 * YMM_SZ)
    {
        y0 = _mm256_stream_load_si256(src + offset + 0 * YMM_SZ);
        y1 = _mm256_stream_load_si256(src + offset + 1 * YMM_SZ);
        y2 = _mm256_stream_load_si256(src + offset + 2 * YMM_SZ);
        y3 = _mm256_stream_load_si256(src + offset + 3 * YMM_SZ);

        _mm256_stream_si256 (dst + offset + 0 * YMM_SZ, y0);
        _mm256_stream_si256 (dst + offset + 1 * YMM_SZ, y1);
        _mm256_stream_si256 (dst + offset + 2 * YMM_SZ, y2);
        _mm256_stream_si256 (dst + offset + 3 * YMM_SZ, y3);

        size -= 4 * YMM_SZ;
        offset += 4 * YMM_SZ;
        if (size == 0)
            return (dst + size + offset);
    }
    if (size >= 2 * YMM_SZ)
    {
        y0 = _mm256_stream_load_si256(src + offset + 0 * YMM_SZ);
        y1 = _mm256_stream_load_si256(src + offset + 1 * YMM_SZ);

        _mm256_stream_si256 (dst + offset + 0 * YMM_SZ, y0);
        _mm256_stream_si256 (dst + offset + 1 * YMM_SZ, y1);

        size -= 2 * YMM_SZ;
        offset += 2 * YMM_SZ;
        if (size == 0)
            return (dst + size + offset);
    }

    if ((size >= YMM_SZ))
    {
        y0 = _mm256_stream_load_si256(src + offset);
        _mm256_stream_si256 (dst + offset, y0);
    }
    //copy last YMM_SZ Bytes

    y0 = _mm256_loadu_si256(src + size + offset - YMM_SZ);
    _mm256_storeu_si256 (dst + size - YMM_SZ + offset , y0);
    return (dst + size + offset);

}
void *__mempcpy_avx2_nt_load(void *dst, const void *src, size_t size)
{
    __m256i y0, y1, y2, y3;
    size_t offset = 0;

    LOG_INFO("\n");
    if (size <= 2 * YMM_SZ)
        return mempcpy_le_2ymm(dst, src, size);

    //compute the offset to align the src to YMM_SZ Bytes boundary
    offset = YMM_SZ - ((size_t)src & (YMM_SZ - 1));
    y0 = _mm256_loadu_si256(src);
    _mm256_storeu_si256 (dst, y0);
    size -= offset;

    while (size >= 4 * YMM_SZ)
    {
        y0 = _mm256_stream_load_si256(src + offset + 0 * YMM_SZ);
        y1 = _mm256_stream_load_si256(src + offset + 1 * YMM_SZ);
        y2 = _mm256_stream_load_si256(src + offset + 2 * YMM_SZ);
        y3 = _mm256_stream_load_si256(src + offset + 3 * YMM_SZ);

        _mm256_storeu_si256 (dst + offset + 0 * YMM_SZ, y0);
        _mm256_storeu_si256 (dst + offset + 1 * YMM_SZ, y1);
        _mm256_storeu_si256 (dst + offset + 2 * YMM_SZ, y2);
        _mm256_storeu_si256 (dst + offset + 3 * YMM_SZ, y3);

        size -= 4 * YMM_SZ;
        offset += 4 * YMM_SZ;
        if (size == 0)
            return (dst + size + offset);
    }
    if (size >= 2 * YMM_SZ)
    {
        y0 = _mm256_stream_load_si256(src + offset + 0 * YMM_SZ);
        y1 = _mm256_stream_load_si256(src + offset + 1 * YMM_SZ);

        _mm256_storeu_si256 (dst + offset + 0 * YMM_SZ, y0);
        _mm256_storeu_si256 (dst + offset + 1 * YMM_SZ, y1);

        size -= 2 * YMM_SZ;
        offset += 2 * YMM_SZ;
        if (size == 0)
            return (dst + size + offset);
    }

    if ((size > YMM_SZ))
    {
        y0 = _mm256_stream_load_si256(src + offset);
        _mm256_storeu_si256 (dst + offset, y0);
    }
    //copy last YMM_SZ Bytes
    y0 = _mm256_loadu_si256(src + size + offset - YMM_SZ);
    _mm256_storeu_si256 (dst + size - YMM_SZ + offset , y0);
    return (dst + size + offset);
}


void *__mempcpy_avx2_nt_store(void *dst, const void *src, size_t size)
{
    __m256i y0, y1, y2, y3;
    size_t offset = 0;

    LOG_INFO("\n");
    if (size <= 2 * YMM_SZ)
        return mempcpy_le_2ymm(dst, src, size);

    //compute the offset to align the dst to YMM_SZ Bytes boundary
    offset = YMM_SZ - ((size_t)dst & (YMM_SZ - 1));
    y0 = _mm256_loadu_si256(src);
    _mm256_storeu_si256 (dst, y0);
    size -= offset;

    while (size >= 4 * YMM_SZ)
    {
        y0 = _mm256_loadu_si256(src + offset + 0 * YMM_SZ);
        y1 = _mm256_loadu_si256(src + offset + 1 * YMM_SZ);
        y2 = _mm256_loadu_si256(src + offset + 2 * YMM_SZ);
        y3 = _mm256_loadu_si256(src + offset + 3 * YMM_SZ);

        _mm256_stream_si256 (dst + offset + 0 * YMM_SZ, y0);
        _mm256_stream_si256 (dst + offset + 1 * YMM_SZ, y1);
        _mm256_stream_si256 (dst + offset + 2 * YMM_SZ, y2);
        _mm256_stream_si256 (dst + offset + 3 * YMM_SZ, y3);

        size -= 4 * YMM_SZ;
        offset += 4 * YMM_SZ;
        if (size == 0)
            return (dst + size + offset);
    }
    if (size >= 2 * YMM_SZ)
    {
        y0 = _mm256_loadu_si256(src + offset + 0 * YMM_SZ);
        y1 = _mm256_loadu_si256(src + offset + 1 * YMM_SZ);

        _mm256_stream_si256 (dst + offset + 0 * YMM_SZ, y0);
        _mm256_stream_si256 (dst + offset + 1 * YMM_SZ, y1);

        size -= 2 * YMM_SZ;
        offset += 2 * YMM_SZ;
        if (size == 0)
            return (dst + size + offset);
    }

    if ((size > YMM_SZ))
    {
        y0 = _mm256_loadu_si256(src + offset);
        _mm256_stream_si256 (dst + offset, y0);
    }
    //copy last YMM_SZ Bytes
    y0 = _mm256_loadu_si256(src + size + offset - YMM_SZ);
    _mm256_storeu_si256 (dst + size - YMM_SZ + offset , y0);
    return (dst + size + offset);
}
