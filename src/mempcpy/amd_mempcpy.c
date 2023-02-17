/* Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
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

__attribute__((target("lzcnt")))
static inline void *mempcpy_le_2ymm(void *dst, const void *src, size_t size)
{
    __m256i y0, y1;
    __m128i x0, x1;
    void * src_end = (void*)src + size;
    void * dst_end = (void*)dst + size;

    switch (_lzcnt_u32(size))
    {
        case 32:
            return dst_end;
        case 31:
            *((uint8_t *)dst) = *((uint8_t *)src);
            return dst_end;
        case 30:
            *((uint16_t *)dst) = *((uint16_t *)src);
            *((uint16_t *)(dst_end - WORD_SZ)) = \
                    *((uint16_t *)(src_end - WORD_SZ));
            return dst_end;
        case 29:
            *((uint32_t *)dst) = *((uint32_t *)src);
            *((uint32_t *)(dst_end - DWORD_SZ)) = \
                    *((uint32_t *)(src_end - DWORD_SZ));
            return dst_end;
        case 28:
            *((uint64_t *)dst) = *((uint64_t *)src);
            *((uint64_t *)(dst_end - QWORD_SZ)) = \
                    *((uint64_t *)(src_end - QWORD_SZ));
            return dst_end;
        case 27:
            x0 = _mm_loadu_si128(src);
            x1 = _mm_loadu_si128(src_end - XMM_SZ);
            _mm_storeu_si128(dst, x0);
            _mm_storeu_si128(dst_end - XMM_SZ, x1);
            return dst_end;
        default:
            y0 = _mm256_loadu_si256(src);
            y1 = _mm256_loadu_si256(src_end - YMM_SZ);
            _mm256_storeu_si256(dst, y0);
            _mm256_storeu_si256(dst_end - YMM_SZ, y1);
    }
    return dst_end;
}


static inline void *unaligned_ld_st(void *dst, const void *src, size_t size)
{
    __m256i y0, y1, y2, y3;
    size_t offset = 0;

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
            return dst + offset;
    }
    if (size >= 2 * YMM_SZ)
    {
        y0 = _mm256_loadu_si256(src + offset + 0 * YMM_SZ);
        y1 = _mm256_loadu_si256(src + offset + 1 * YMM_SZ);

        _mm256_storeu_si256 (dst + offset + 0 * YMM_SZ, y0);
        _mm256_storeu_si256 (dst + offset + 1 * YMM_SZ, y1);

        size -= 2 * YMM_SZ;
        offset += 2 * YMM_SZ;
    }

    if (size > YMM_SZ)
    {
        y0 = _mm256_loadu_si256(src + offset);
        _mm256_storeu_si256 (dst + offset, y0);
    }
    //copy last YMM_SZ Bytes
    y0 = _mm256_loadu_si256(src + offset +  size - YMM_SZ);
    _mm256_storeu_si256 (dst + offset + size - YMM_SZ, y0);
    return (dst + offset + size);
}

static inline void *nt_store(void *dst, const void *src, size_t size)
{
    __m256i y0, y1, y2, y3;
    size_t offset = 0;

    //compute the offset to align the dst to YMM_SZ boundary
    offset = 0x20 - ((size_t)dst & 0x1F);
    y0 = _mm256_loadu_si256(src);
    _mm256_storeu_si256 (dst, y0);
    size -= offset;

    while ((size) >= 4 * YMM_SZ)
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
    }
    if (size >= 2 * YMM_SZ)
    {
        y0 = _mm256_loadu_si256(src + offset + 0 * YMM_SZ);
        y1 = _mm256_loadu_si256(src + offset + 1 * YMM_SZ);

        _mm256_stream_si256 (dst + offset + 0 * YMM_SZ, y0);
        _mm256_stream_si256 (dst + offset + 1 * YMM_SZ, y1);

        size -= 2 * YMM_SZ;
        offset += 2 * YMM_SZ;
    }

    if (size > YMM_SZ)
    {
        y0 = _mm256_loadu_si256(src + offset);
        _mm256_stream_si256 (dst + offset, y0);
    }
    //copy last YMM_SZ Bytes
    y0 = _mm256_loadu_si256(src + size + offset - YMM_SZ);
    _mm256_storeu_si256 (dst + size - YMM_SZ + offset , y0);
    return (dst + offset + size);
}

#ifdef AVX512_FEATURE_ENABLED
static inline void *mempcpy_le_2zmm(void *dst, const void *src, size_t size)
{
    __m512i z0, z1;

    if (size <= 2 * YMM_SZ)
        return mempcpy_le_2ymm(dst, src, size);
    // above ZMM_SZ Bytes uses ZMM registers
    z0 = _mm512_loadu_si512(src);
    z1 = _mm512_loadu_si512(src + size - ZMM_SZ);
    _mm512_storeu_si512(dst, z0);
    _mm512_storeu_si512(dst + size - ZMM_SZ, z1);

    return (dst + size);
}

static inline void *unaligned_ld_st_avx512(void *dst, const void *src, size_t size)
{
    __m512i z0, z1, z2, z3;
    size_t offset = 0;

    while ((size) >= 4 * ZMM_SZ)
    {
        z0 = _mm512_loadu_si512(src + offset + 0 * ZMM_SZ);
        z1 = _mm512_loadu_si512(src + offset + 1 * ZMM_SZ);
        z2 = _mm512_loadu_si512(src + offset + 2 * ZMM_SZ);
        z3 = _mm512_loadu_si512(src + offset + 3 * ZMM_SZ);

        _mm512_storeu_si512 (dst + offset + 0 * ZMM_SZ, z0);
        _mm512_storeu_si512 (dst + offset + 1 * ZMM_SZ, z1);
        _mm512_storeu_si512 (dst + offset + 2 * ZMM_SZ, z2);
        _mm512_storeu_si512 (dst + offset + 3 * ZMM_SZ, z3);

        size -= 4 * ZMM_SZ;
        offset += 4 * ZMM_SZ;
    }
    if ((size) >= 2 * ZMM_SZ)
    {
        z0 = _mm512_loadu_si512(src + offset + 0 * ZMM_SZ);
        z1 = _mm512_loadu_si512(src + offset + 1 * ZMM_SZ);

        _mm512_storeu_si512 (dst + offset + 0 * ZMM_SZ, z0);
        _mm512_storeu_si512 (dst + offset + 1 * ZMM_SZ, z1);

        size -= 2 * ZMM_SZ;
        offset += 2 * ZMM_SZ;
    }

    if ((size > ZMM_SZ))
    {
        z0 = _mm512_loadu_si512(src + offset);
        _mm512_storeu_si512 (dst + offset, z0);
    }
    //copz last ZMM_SZ Bytes
    z0 = _mm512_loadu_si512(src + size + offset - ZMM_SZ);
    _mm512_storeu_si512 (dst + size - ZMM_SZ + offset , z0);
    return (dst + size + offset);
}

#endif


void *amd_mempcpy(void *dst, const void *src, size_t size)
{
    LOG_INFO("\n");
#ifdef AVX512_FEATURE_ENABLED
    if (size <= 2 * ZMM_SZ)
        return mempcpy_le_2zmm(dst, src, size);
    return unaligned_ld_st_avx512(dst, src, size);
#else
    if (size <= 2 * YMM_SZ)
        return mempcpy_le_2ymm(dst, src, size);
    if (size < __nt_start_threshold)
        return unaligned_ld_st(dst, src, size);
    else
        return nt_store(dst, src, size);
#endif
}

void *mempcpy(void *, const void *, size_t) __attribute__((weak, alias("amd_mempcpy"), visibility("default")));
