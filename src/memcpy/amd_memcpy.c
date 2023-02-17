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
static inline void *memcpy_le_2ymm(void *dst, const void *src, size_t size)
{
    __m256i y0, y1;
    __m128i x0, x1;
    void * src_end = (void*)src + size;
    void * dst_end = (void*)dst + size;

    switch (_lzcnt_u32(size))
    {
        case 32:
            return dst;
        case 31:
            *((uint8_t *)dst) = *((uint8_t *)src);
            return dst;
        case 30:
            *((uint16_t *)dst) = *((uint16_t *)src);
            *((uint16_t *)(dst_end - WORD_SZ)) = \
                    *((uint16_t *)(src_end - WORD_SZ));
            return dst;
        case 29:
            *((uint32_t *)dst) = *((uint32_t *)src);
            *((uint32_t *)(dst_end - DWORD_SZ)) = \
                    *((uint32_t *)(src_end - DWORD_SZ));
            return dst;
        case 28:
            *((uint64_t *)dst) = *((uint64_t *)src);
            *((uint64_t *)(dst_end - QWORD_SZ)) = \
                    *((uint64_t *)(src_end - QWORD_SZ));
            return dst;
        case 27:
            x0 = _mm_loadu_si128(src);
            x1 = _mm_loadu_si128(src_end - XMM_SZ);
            _mm_storeu_si128(dst, x0);
            _mm_storeu_si128(dst_end - XMM_SZ, x1);
            return dst;
        default:
            y0 = _mm256_loadu_si256(src);
            y1 = _mm256_loadu_si256(src_end - YMM_SZ);
            _mm256_storeu_si256(dst, y0);
            _mm256_storeu_si256(dst_end - YMM_SZ, y1);
    }
    return dst;
}

static inline void *unaligned_ld_st_avx2(void *dst, const void *src, size_t size)
{
    __m256i y0, y1, y2, y3;
    __m256i y4, y5, y6, y7;
    void * src_index = (void*)src + size;
    void * dst_index = (void *)dst + size;
    size_t offset = 0, dst_align = 0;

    y0 = _mm256_loadu_si256(src + 0 * YMM_SZ);
    y1 = _mm256_loadu_si256(src + 1 * YMM_SZ);
    y2 = _mm256_loadu_si256(src_index - 2 * YMM_SZ);
    y3 = _mm256_loadu_si256(src_index - 1 * YMM_SZ);

    _mm256_storeu_si256 (dst + 0 * YMM_SZ, y0);
    _mm256_storeu_si256 (dst + 1 * YMM_SZ, y1);
    _mm256_storeu_si256 (dst_index - 2 * YMM_SZ, y2);
    _mm256_storeu_si256 (dst_index - 1 * YMM_SZ, y3);
    if (size <= 4 * YMM_SZ) //128B
        return dst;
    y4 = _mm256_loadu_si256(src + 2 * YMM_SZ);
    y5 = _mm256_loadu_si256(src + 3 * YMM_SZ);
    y6 = _mm256_loadu_si256(src_index - 4 * YMM_SZ);
    y7 = _mm256_loadu_si256(src_index - 3 * YMM_SZ);

    _mm256_storeu_si256 (dst + 2 * YMM_SZ, y4);
    _mm256_storeu_si256 (dst + 3 * YMM_SZ, y5);
    _mm256_storeu_si256 (dst_index - 4 * YMM_SZ, y6);
    _mm256_storeu_si256 (dst_index - 3 * YMM_SZ, y7);
    if (size <= 8 * YMM_SZ) //256B
        return dst;

    offset += 4 * YMM_SZ;
    size -= 4 * YMM_SZ;

    dst_align = ((size_t)dst & (YMM_SZ - 1));

    if ((((size_t)src & (YMM_SZ - 1)) == 0) && (dst_align == 0))
    {
        while(offset < size)
        {
            y0 = _mm256_load_si256(src + offset + 0 * YMM_SZ);
            y1 = _mm256_load_si256(src + offset + 1 * YMM_SZ);
            y2 = _mm256_load_si256(src + offset + 2 * YMM_SZ);
            y3 = _mm256_load_si256(src + offset + 3 * YMM_SZ);
            _mm256_store_si256 (dst + offset + 0 * YMM_SZ, y0);
            _mm256_store_si256 (dst + offset + 1 * YMM_SZ, y1);
            _mm256_store_si256 (dst + offset + 2 * YMM_SZ, y2);
            _mm256_store_si256 (dst + offset + 3 * YMM_SZ, y3);
            offset += 4 * YMM_SZ;
        }
        return dst;
    }
    else
    {
        offset -= (2*YMM_SZ + (dst_align & (YMM_SZ-1)));

        while(offset < size)
        {
            y0 = _mm256_loadu_si256(src + offset + 0 * YMM_SZ);
            y1 = _mm256_loadu_si256(src + offset + 1 * YMM_SZ);
            y2 = _mm256_loadu_si256(src + offset + 2 * YMM_SZ);
            y3 = _mm256_loadu_si256(src + offset + 3 * YMM_SZ);
            _mm256_store_si256 (dst + offset + 0 * YMM_SZ, y0);
            _mm256_store_si256 (dst + offset + 1 * YMM_SZ, y1);
            _mm256_store_si256 (dst + offset + 2 * YMM_SZ, y2);
            _mm256_store_si256 (dst + offset + 3 * YMM_SZ, y3);
            offset += 4 * YMM_SZ;
        }
    }
    return dst;
}

static inline void *nt_store_avx2(void *dst, const void *src, size_t size)
{
    __m256i y0, y1, y2, y3;
    size_t offset = 0;
    void * src_index = (void*)src;
    void * dst_index = (void*)dst;

    //compute the offset to align the dst to YMM_SZB boundary
    offset = YMM_SZ - ((size_t)dst & (YMM_SZ-1));
    y0 = _mm256_loadu_si256(src);
    _mm256_storeu_si256 (dst, y0);
    size -= offset;

    src_index += offset;
    dst_index += offset;

    while (size >= 4 * YMM_SZ)
    {
        y0 = _mm256_loadu_si256(src_index + 0 * YMM_SZ);
        y1 = _mm256_loadu_si256(src_index + 1 * YMM_SZ);
        y2 = _mm256_loadu_si256(src_index + 2 * YMM_SZ);
        y3 = _mm256_loadu_si256(src_index + 3 * YMM_SZ);

        _mm256_stream_si256 (dst_index + 0 * YMM_SZ, y0);
        _mm256_stream_si256 (dst_index + 1 * YMM_SZ, y1);
        _mm256_stream_si256 (dst_index + 2 * YMM_SZ, y2);
        _mm256_stream_si256 (dst_index + 3 * YMM_SZ, y3);

        size -= 4 * YMM_SZ;
        src_index += 4 * YMM_SZ;
        dst_index += 4 * YMM_SZ;
    }
    if (size >= 2 * YMM_SZ)
    {
        y0 = _mm256_loadu_si256(src_index + 0 * YMM_SZ);
        y1 = _mm256_loadu_si256(src_index + 1 * YMM_SZ);

        _mm256_stream_si256 (dst_index + 0 * YMM_SZ, y0);
        _mm256_stream_si256 (dst_index + 1 * YMM_SZ, y1);

        size -= 2 * YMM_SZ;
        src_index += 2 * YMM_SZ;
        dst_index += 2 * YMM_SZ;
    }

    if (size > YMM_SZ)
    {
        y0 = _mm256_loadu_si256(src_index);
        _mm256_stream_si256 (dst_index, y0);
    }
    //copy last YMM_SZ Bytes
    y0 = _mm256_loadu_si256(src_index + size - YMM_SZ);
    _mm256_storeu_si256 (dst_index - YMM_SZ + size , y0);

    return dst;
}

#ifdef AVX512_FEATURE_ENABLED
static inline void *unaligned_ld_st_avx512(void *dst, const void *src, size_t size)
{
    __m512i z0, z1, z2, z3;
    __m512i z4, z5, z6, z7;
    __m256i y0, y1, y2, y3;
    __m256i y4, y5, y6, y7;
   size_t offset = 0, dst_align = 0;

    z0 = _mm512_loadu_si512(src + 0 * ZMM_SZ);
    z1 = _mm512_loadu_si512(src + size - 1 * ZMM_SZ);

    _mm512_storeu_si512 (dst + 0 * ZMM_SZ, z0);
    _mm512_storeu_si512 (dst + size - 1 * ZMM_SZ, z1);

    if (size <= 2 * ZMM_SZ) //128B
        return dst;

    z2 = _mm512_loadu_si512(src + 1 * ZMM_SZ);
    z3 = _mm512_loadu_si512(src + size - 2 * ZMM_SZ);
    _mm512_storeu_si512 (dst + 1 * ZMM_SZ, z2);
    _mm512_storeu_si512 (dst + size - 2 * ZMM_SZ, z3);

    if (size <= 4 * ZMM_SZ) //256B
        return dst;

    z4 = _mm512_loadu_si512(src + 2 * ZMM_SZ);
    z5 = _mm512_loadu_si512(src + 3 * ZMM_SZ);
    z6 = _mm512_loadu_si512(src + size - 4 * ZMM_SZ);
    z7 = _mm512_loadu_si512(src + size - 3 * ZMM_SZ);

    _mm512_storeu_si512 (dst + 2 * ZMM_SZ, z4);
    _mm512_storeu_si512 (dst + 3 * ZMM_SZ, z5);
    _mm512_storeu_si512 (dst + size - 4 * ZMM_SZ, z6);
    _mm512_storeu_si512 (dst + size - 3 * ZMM_SZ, z7);

    if (size <= 8 * ZMM_SZ) //512B
        return dst;

    offset += 4 * ZMM_SZ;

    dst_align = ((size_t)dst & (ZMM_SZ - 1));

    //Aligned SRC/DST addresses
    if ((((size_t)src & (ZMM_SZ - 1)) == 0) && (dst_align == 0))
    {
        // 4-ZMM registers
        if (size <= 1024*1024) //L2 Cache Size
        {
            size -= 4 * ZMM_SZ;
            while(offset < size)
            {
                z0 = _mm512_load_si512(src + offset + 0 * ZMM_SZ);
                z1 = _mm512_load_si512(src + offset + 1 * ZMM_SZ);
                z2 = _mm512_load_si512(src + offset + 2 * ZMM_SZ);
                z3 = _mm512_load_si512(src + offset + 3 * ZMM_SZ);
                _mm512_store_si512 (dst + offset + 0 * ZMM_SZ, z0);
                _mm512_store_si512 (dst + offset + 1 * ZMM_SZ, z1);
                _mm512_store_si512 (dst + offset + 2 * ZMM_SZ, z2);
                _mm512_store_si512 (dst + offset + 3 * ZMM_SZ, z3);
                offset += 4 * ZMM_SZ;
            }
            return dst;
        }
        // 4-YMM registers with 2 - prefetch
        else if (size <= 32*1024*1024) //L3 Cache Size
        {
            size -= 4 * ZMM_SZ;
            while(offset < size)
            {
                _mm_prefetch(src + offset + 2 * YMM_SZ, 0);
                _mm_prefetch(src + offset + 2 * YMM_SZ + 64, 0);
                y0 = _mm256_load_si256(src + offset + 0 * YMM_SZ);
                y1 = _mm256_load_si256(src + offset + 1 * YMM_SZ);
                y2 = _mm256_load_si256(src + offset + 2 * YMM_SZ);
                y3 = _mm256_load_si256(src + offset + 3 * YMM_SZ);
                _mm256_store_si256 (dst + offset + 0 * YMM_SZ, y0);
                _mm256_store_si256 (dst + offset + 1 * YMM_SZ, y1);
                _mm256_store_si256 (dst + offset + 2 * YMM_SZ, y2);
                _mm256_store_si256 (dst + offset + 3 * YMM_SZ, y3);
                offset += 4 * YMM_SZ;
            }
            return dst;
        }
        // Non-temporal 8-ZMM registers with prefetch
        else
        {
            size -= 4 * ZMM_SZ;
            while(offset < size)
            {
                _mm_prefetch(src + offset + 4 * ZMM_SZ, 0);
                _mm_prefetch(src + offset + 4 * ZMM_SZ + 128, 0);
                z0 = _mm512_load_si512(src + offset + 0 * ZMM_SZ);
                z1 = _mm512_load_si512(src + offset + 1 * ZMM_SZ);
                z2 = _mm512_load_si512(src + offset + 2 * ZMM_SZ);
                z3 = _mm512_load_si512(src + offset + 3 * ZMM_SZ);
                z4 = _mm512_load_si512(src + offset + 4 * ZMM_SZ);
                z5 = _mm512_load_si512(src + offset + 5 * ZMM_SZ);
                z6 = _mm512_load_si512(src + offset + 6 * ZMM_SZ);
                z7 = _mm512_load_si512(src + offset + 7 * ZMM_SZ);
                _mm512_stream_si512 (dst + offset + 0 * ZMM_SZ, z0);
                _mm512_stream_si512 (dst + offset + 1 * ZMM_SZ, z1);
                _mm512_stream_si512 (dst + offset + 2 * ZMM_SZ, z2);
                _mm512_stream_si512 (dst + offset + 3 * ZMM_SZ, z3);
                _mm512_stream_si512 (dst + offset + 4 * ZMM_SZ, z4);
                _mm512_stream_si512 (dst + offset + 5 * ZMM_SZ, z5);
                _mm512_stream_si512 (dst + offset + 6 * ZMM_SZ, z6);
                _mm512_stream_si512 (dst + offset + 7 * ZMM_SZ, z7);
                offset += 8 * ZMM_SZ;
            }
            return dst;
        }
    }
    // Unalgined SRC/DST addresses.
    else
    {
        offset -= (dst_align & (YMM_SZ-1));
        size -= 4 * ZMM_SZ;
        if (size <= 32*1024*1024) // L3-Cache size
        {
            while(offset < size)
            {
                y0 = _mm256_loadu_si256(src + offset + 0 * YMM_SZ);
                y1 = _mm256_loadu_si256(src + offset + 1 * YMM_SZ);
                y2 = _mm256_loadu_si256(src + offset + 2 * YMM_SZ);
                y3 = _mm256_loadu_si256(src + offset + 3 * YMM_SZ);
                y4 = _mm256_loadu_si256(src + offset + 4 * YMM_SZ);
                y5 = _mm256_loadu_si256(src + offset + 5 * YMM_SZ);
                y6 = _mm256_loadu_si256(src + offset + 6 * YMM_SZ);
                y7 = _mm256_loadu_si256(src + offset + 7 * YMM_SZ);
                _mm256_store_si256 (dst + offset + 0 * YMM_SZ, y0);
                _mm256_store_si256 (dst + offset + 1 * YMM_SZ, y1);
                _mm256_store_si256 (dst + offset + 2 * YMM_SZ, y2);
                _mm256_store_si256 (dst + offset + 3 * YMM_SZ, y3);
                _mm256_store_si256 (dst + offset + 4 * YMM_SZ, y4);
                _mm256_store_si256 (dst + offset + 5 * YMM_SZ, y5);
                _mm256_store_si256 (dst + offset + 6 * YMM_SZ, y6);
                _mm256_store_si256 (dst + offset + 7 * YMM_SZ, y7);
                offset += 8 * YMM_SZ;
            }
        }
        else
        {
            while(offset < size)
            {
                _mm_prefetch(src + offset + 2 * ZMM_SZ, 0);
                _mm_prefetch(src + offset + 2 * ZMM_SZ + 128, 0);
                z0 = _mm512_loadu_si512(src + offset + 0 * ZMM_SZ);
                z1 = _mm512_loadu_si512(src + offset + 1 * ZMM_SZ);
                z2 = _mm512_loadu_si512(src + offset + 2 * ZMM_SZ);
                z3 = _mm512_loadu_si512(src + offset + 3 * ZMM_SZ);
                _mm512_stream_si512 (dst + offset + 0 * ZMM_SZ, z0);
                _mm512_stream_si512 (dst + offset + 1 * ZMM_SZ, z1);
                _mm512_stream_si512 (dst + offset + 2 * ZMM_SZ, z2);
                _mm512_stream_si512 (dst + offset + 3 * ZMM_SZ, z3);
                offset += 4 * ZMM_SZ;
            }
            return dst;
        }
    }
    return dst;
}
#endif

void *amd_memcpy(void *dst, const void *src, size_t size)
{
    LOG_INFO("\n");

    if (size <= 2 * YMM_SZ)
        return memcpy_le_2ymm(dst, src, size);
#ifdef AVX512_FEATURE_ENABLED
        return unaligned_ld_st_avx512(dst, src, size);
#else
    if (size < __nt_start_threshold)
        return unaligned_ld_st_avx2(dst, src, size);
    else
        return nt_store_avx2(dst, src, size);
#endif
}

void *memcpy(void *, const void *, size_t) __attribute__((weak, alias("amd_memcpy"), visibility("default")));
