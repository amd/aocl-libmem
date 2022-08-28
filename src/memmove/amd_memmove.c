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

static inline void *memmove_le_2ymm(void *dst, const void *src, size_t size)
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
    if (size < 2 * WORD_SZ)
    {
        temp = *((uint16_t *)src);
        *((uint16_t *)(dst + size - WORD_SZ)) = *((uint16_t *)(src + size - WORD_SZ));
        *((uint16_t *)dst) = temp;
        return dst;
    }
    if (size < DWORD_SZ)
    {
        temp = *((uint32_t *)src);
        *((uint32_t *)(dst + size - DWORD_SZ)) = *((uint32_t *)(src + size - DWORD_SZ));
        *((uint32_t *)dst) = temp;
        return dst;
    }
    if (size < 2 * QWORD_SZ)
    {
        temp = *((uint64_t *)src);
        *((uint64_t *)(dst + size - QWORD_SZ)) = *((uint64_t *)(src + size - QWORD_SZ));
        *((uint64_t *)dst) = temp;
        return dst;
    }
    if (size < 2 * XMM_SZ)
    {
        x0 = _mm_loadu_si128(src);
        x1 = _mm_loadu_si128(src + size - XMM_SZ);
        _mm_storeu_si128(dst, x0);
        _mm_storeu_si128(dst + size - XMM_SZ, x1);
        return dst;
    }
    y0 = _mm256_loadu_si256(src);
    y1 = _mm256_loadu_si256(src + size - YMM_SZ);
    _mm256_storeu_si256(dst, y0);
    _mm256_storeu_si256(dst + size - YMM_SZ, y1);

    return dst;
}

static inline void *unaligned_ld_st(void *dst, const void *src, size_t size)
{
    __m256i y0, y1, y2, y3, y4, y5, y6, y7, y8, y9, y10, y11;
    size_t offset = 0, len = size;
    if (size <= 4 * YMM_SZ)
    {
        y0 = _mm256_loadu_si256(src + 0 * YMM_SZ);
        y1 = _mm256_loadu_si256(src + 1 * YMM_SZ);
        y3 = _mm256_loadu_si256(src + len - 2 * YMM_SZ);
        y2 = _mm256_loadu_si256(src + len - 1 * YMM_SZ);

        _mm256_storeu_si256(dst + 0 * YMM_SZ, y0);
        _mm256_storeu_si256(dst + 1 * YMM_SZ, y1);
        _mm256_storeu_si256(dst + len - 2 * YMM_SZ, y3);
        _mm256_storeu_si256(dst + len - 1 * YMM_SZ, y2);
        return dst;
    }
    y0 = _mm256_loadu_si256(src + 0 * YMM_SZ);
    y1 = _mm256_loadu_si256(src + 1 * YMM_SZ);
    y2 = _mm256_loadu_si256(src + 2 * YMM_SZ);
    y3 = _mm256_loadu_si256(src + 3 * YMM_SZ);
    y7 = _mm256_loadu_si256(src + len - 4 * YMM_SZ);
    y6 = _mm256_loadu_si256(src + len - 3 * YMM_SZ);
    y5 = _mm256_loadu_si256(src + len - 2 * YMM_SZ);
    y4 = _mm256_loadu_si256(src + len - 1 * YMM_SZ);

    if (size <= 8 * YMM_SZ)
    {
    _mm256_storeu_si256(dst +  0 * YMM_SZ, y0);
    _mm256_storeu_si256(dst +  1 * YMM_SZ, y1);
    _mm256_storeu_si256(dst +  2 * YMM_SZ, y2);
    _mm256_storeu_si256(dst +  3 * YMM_SZ, y3);
    _mm256_storeu_si256(dst + len - 4 * YMM_SZ, y7);
    _mm256_storeu_si256(dst + len - 3 * YMM_SZ, y6);
    _mm256_storeu_si256(dst + len - 2 * YMM_SZ, y5);
    _mm256_storeu_si256(dst + len - 1 * YMM_SZ, y4);
    return dst;
    }
    size -= 4 * YMM_SZ;
    offset += 4 * YMM_SZ;

    if ((dst < (src + size + 4 * YMM_SZ)) && (src < dst))
        goto BACKWARD_COPY;

    while (offset <= size)
    {
        y8 = _mm256_loadu_si256(src + offset + 0 * YMM_SZ);
        y9 = _mm256_loadu_si256(src + offset + 1 * YMM_SZ);
        y10 = _mm256_loadu_si256(src + offset + 2 * YMM_SZ);
        y11 = _mm256_loadu_si256(src + offset + 3 * YMM_SZ);

        _mm256_storeu_si256(dst + offset + 0 * YMM_SZ, y8);
        _mm256_storeu_si256(dst + offset + 1 * YMM_SZ, y9);
        _mm256_storeu_si256(dst + offset + 2 * YMM_SZ, y10);
        _mm256_storeu_si256(dst + offset + 3 * YMM_SZ, y11);
        offset += 4 * YMM_SZ;
    }
    _mm256_storeu_si256(dst +  0 * YMM_SZ, y0);
    _mm256_storeu_si256(dst +  1 * YMM_SZ, y1);
    _mm256_storeu_si256(dst +  2 * YMM_SZ, y2);
    _mm256_storeu_si256(dst +  3 * YMM_SZ, y3);
    _mm256_storeu_si256(dst + len - 4 * YMM_SZ, y7);
    _mm256_storeu_si256(dst + len - 3 * YMM_SZ, y6);
    _mm256_storeu_si256(dst + len - 2 * YMM_SZ, y5);
    _mm256_storeu_si256(dst + len - 1 * YMM_SZ, y4);
    return dst;
BACKWARD_COPY:
    while (offset <= size)
    {
        y8 = _mm256_loadu_si256(src + size - 1 * YMM_SZ);
        y9 = _mm256_loadu_si256(src + size - 2 * YMM_SZ);
        y10 = _mm256_loadu_si256(src + size - 3 * YMM_SZ);
        y11 = _mm256_loadu_si256(src + size - 4 * YMM_SZ);

        _mm256_storeu_si256(dst + size - 1 * YMM_SZ, y8);
        _mm256_storeu_si256(dst + size - 2 * YMM_SZ, y9);
        _mm256_storeu_si256(dst + size - 3 * YMM_SZ, y10);
        _mm256_storeu_si256(dst + size - 4 * YMM_SZ, y11);
        size -= 4 * YMM_SZ;
    }
    _mm256_storeu_si256(dst +  0 * YMM_SZ, y0);
    _mm256_storeu_si256(dst +  1 * YMM_SZ, y1);
    _mm256_storeu_si256(dst +  2 * YMM_SZ, y2);
    _mm256_storeu_si256(dst +  3 * YMM_SZ, y3);
    _mm256_storeu_si256(dst + len - 4 * YMM_SZ, y7);
    _mm256_storeu_si256(dst + len - 3 * YMM_SZ, y6);
    _mm256_storeu_si256(dst + len - 2 * YMM_SZ, y5);
    _mm256_storeu_si256(dst + len - 1 * YMM_SZ, y4);
    return dst;
}

static inline void *nt_store(void *dst, const void *src, size_t size)
{
    __m256i y0, y1, y2, y3;
    size_t offset = 0;

    //compute the offset to align the dst to YMM_SZBytes boundary
    offset = YMM_SZ- ((size_t)dst & (YMM_SZ- 1));
    y0 = _mm256_loadu_si256(src);
    _mm256_storeu_si256(dst, y0);
    size -= offset;

    while ((size) >= 4 * YMM_SZ)
    {
        y0 = _mm256_loadu_si256(src + offset + 0 * YMM_SZ);
        y1 = _mm256_loadu_si256(src + offset + 1 * YMM_SZ);
        y2 = _mm256_loadu_si256(src + offset + 2 * YMM_SZ);
        y3 = _mm256_loadu_si256(src + offset + 3 * YMM_SZ);

        _mm256_stream_si256(dst + offset + 0 * YMM_SZ, y0);
        _mm256_stream_si256(dst + offset + 1 * YMM_SZ, y1);
        _mm256_stream_si256(dst + offset + 2 * YMM_SZ, y2);
        _mm256_stream_si256(dst + offset + 3 * YMM_SZ, y3);

        size -= 4 * YMM_SZ;
        offset += 4 * YMM_SZ;
    }
    if ((size) >= 2 * YMM_SZ)
    {
        y0 = _mm256_loadu_si256(src + offset + 0 * YMM_SZ);
        y1 = _mm256_loadu_si256(src + offset + 1 * YMM_SZ);

        _mm256_stream_si256(dst + offset + 0 * YMM_SZ, y0);
        _mm256_stream_si256(dst + offset + 1 * YMM_SZ, y1);

        size -= 2 * YMM_SZ;
        offset += 2 * YMM_SZ;
    }

    if ((size > YMM_SZ))
    {
        y0 = _mm256_loadu_si256(src + offset);
        _mm256_stream_si256(dst + offset, y0);
    }
    //copy last YMM_SZBytes
    y0 = _mm256_loadu_si256(src + size + offset - YMM_SZ);
    _mm256_storeu_si256(dst + size - YMM_SZ+ offset , y0);
    return dst;
}

#ifdef AVX512_FEATURE_ENABLED
static inline void *memmove_le_2zmm(void *dst, const void *src, size_t size)
{
    __m512i z0, z1;

    if (size <= 2 * YMM_SZ)
        return memmove_le_2ymm(dst, src, size);
    // above ZMM_SZBytes use ZMM registers
    z0 = _mm512_loadu_si512(src);
    z1 = _mm512_loadu_si512(src + size - ZMM_SZ);
    _mm512_storeu_si512(dst, z0);
    _mm512_storeu_si512(dst + size - ZMM_SZ, z1);

    return dst;
}

static inline void *unaligned_ld_st_avx512(void *dst, const void *src, size_t size)
{
    __m512i z0, z1, z2, z3, z4, z5, z6, z7, z8, z9, z10, z11;
    size_t offset = 0, len = size;
    if (size <= 4 * ZMM_SZ)
    {
        z0 = _mm512_loadu_si512(src + 0 * ZMM_SZ);
        z1 = _mm512_loadu_si512(src + 1 * ZMM_SZ);
        z3 = _mm512_loadu_si512(src + len - 2 * ZMM_SZ);
        z2 = _mm512_loadu_si512(src + len - 1 * ZMM_SZ);

        _mm512_storeu_si512(dst + 0 * ZMM_SZ, z0);
        _mm512_storeu_si512(dst + 1 * ZMM_SZ, z1);
        _mm512_storeu_si512(dst + len - 2 * ZMM_SZ, z3);
        _mm512_storeu_si512(dst + len - 1 * ZMM_SZ, z2);
        return dst;
    }
    z0 = _mm512_loadu_si512(src + 0 * ZMM_SZ);
    z1 = _mm512_loadu_si512(src + 1 * ZMM_SZ);
    z2 = _mm512_loadu_si512(src + 2 * ZMM_SZ);
    z3 = _mm512_loadu_si512(src + 3 * ZMM_SZ);
    z7 = _mm512_loadu_si512(src + len - 4 * ZMM_SZ);
    z6 = _mm512_loadu_si512(src + len - 3 * ZMM_SZ);
    z5 = _mm512_loadu_si512(src + len - 2 * ZMM_SZ);
    z4 = _mm512_loadu_si512(src + len - 1 * ZMM_SZ);

    if (size <= 8 * ZMM_SZ)
    {
    _mm512_storeu_si512(dst +  0 * ZMM_SZ, z0);
    _mm512_storeu_si512(dst +  1 * ZMM_SZ, z1);
    _mm512_storeu_si512(dst +  2 * ZMM_SZ, z2);
    _mm512_storeu_si512(dst +  3 * ZMM_SZ, z3);
    _mm512_storeu_si512(dst + len - 4 * ZMM_SZ, z7);
    _mm512_storeu_si512(dst + len - 3 * ZMM_SZ, z6);
    _mm512_storeu_si512(dst + len - 2 * ZMM_SZ, z5);
    _mm512_storeu_si512(dst + len - 1 * ZMM_SZ, z4);
    return dst;
    }
    size -= 4 * ZMM_SZ;
    offset += 4 * ZMM_SZ;

    if ((dst < (src + size + 4 * ZMM_SZ)) && (src < dst))
        goto BACKWARD_COPY;

    while (offset <= size)
    {
        z8 = _mm512_loadu_si512(src + offset + 0 * ZMM_SZ);
        z9 = _mm512_loadu_si512(src + offset + 1 * ZMM_SZ);
        z10 = _mm512_loadu_si512(src + offset + 2 * ZMM_SZ);
        z11 = _mm512_loadu_si512(src + offset + 3 * ZMM_SZ);

        _mm512_storeu_si512(dst + offset + 0 * ZMM_SZ, z8);
        _mm512_storeu_si512(dst + offset + 1 * ZMM_SZ, z9);
        _mm512_storeu_si512(dst + offset + 2 * ZMM_SZ, z10);
        _mm512_storeu_si512(dst + offset + 3 * ZMM_SZ, z11);
        offset +=4 * ZMM_SZ;
    }
    _mm512_storeu_si512(dst +  0 * ZMM_SZ, z0);
    _mm512_storeu_si512(dst +  1 * ZMM_SZ, z1);
    _mm512_storeu_si512(dst +  2 * ZMM_SZ, z2);
    _mm512_storeu_si512(dst +  3 * ZMM_SZ, z3);
    _mm512_storeu_si512(dst + len - 4 * ZMM_SZ, z7);
    _mm512_storeu_si512(dst + len - 3 * ZMM_SZ, z6);
    _mm512_storeu_si512(dst + len - 2 * ZMM_SZ, z5);
    _mm512_storeu_si512(dst + len - 1 * ZMM_SZ, z4);
    return dst;
BACKWARD_COPY:
    while (offset <= size)
    {
        z8 = _mm512_loadu_si512(src + size - 1 * ZMM_SZ);
        z9 = _mm512_loadu_si512(src + size - 2 * ZMM_SZ);
        z10 = _mm512_loadu_si512(src + size - 3 * ZMM_SZ);
        z11 = _mm512_loadu_si512(src + size - 4 * ZMM_SZ);

        _mm512_storeu_si512(dst + size - 1 * ZMM_SZ, z8);
        _mm512_storeu_si512(dst + size - 2 * ZMM_SZ, z9);
        _mm512_storeu_si512(dst + size - 3 * ZMM_SZ, z10);
        _mm512_storeu_si512(dst + size - 4 * ZMM_SZ, z11);
        size -=4 * ZMM_SZ;
    }
    _mm512_storeu_si512(dst +  0 * ZMM_SZ, z0);
    _mm512_storeu_si512(dst +  1 * ZMM_SZ, z1);
    _mm512_storeu_si512(dst +  2 * ZMM_SZ, z2);
    _mm512_storeu_si512(dst +  3 * ZMM_SZ, z3);
    _mm512_storeu_si512(dst + len - 4 * ZMM_SZ, z7);
    _mm512_storeu_si512(dst + len - 3 * ZMM_SZ, z6);
    _mm512_storeu_si512(dst + len - 2 * ZMM_SZ, z5);
    _mm512_storeu_si512(dst + len - 1 * ZMM_SZ, z4);
    return dst;
}
#endif

void *amd_memmove(void *dst, const void *src, size_t size)
{
    LOG_INFO("\n");
#ifdef AVX512_FEATURE_ENABLED
    if (size <= 2 * ZMM_SZ)
        return memmove_le_2zmm(dst, src, size);
    return unaligned_ld_st_avx512(dst, src, size);
#endif
    if (size <= 2 * YMM_SZ)
        return memmove_le_2ymm(dst, src, size);
    if (size > __nt_start_threshold)
    {
        if (((src + size) < dst) || ((dst + size) < src))
            return nt_store(dst, src, size);
    }
    return unaligned_ld_st(dst, src, size);
}

void *memmove(void *, const void *, size_t) __attribute__((weak, alias("amd_memmove"), visibility("default")));
