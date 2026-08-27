/* Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
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

#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>

#include "almem_defs.h"
#include "logger.h"
#include "threshold.h"
#include "zen_cpu_info.h"

extern cpu_info zen_info;

#define MEMCMP_PREFETCH_DISTANCE (16 * ZMM_SZ) /* 1024B */
#define MEMCMP_PREFETCH_MIN_SIZE (1 << 20)     /* 1MB  */
#define MEMCMP_PREFETCH_MAX_SIZE (12 << 20)    /* 12MB */
#define BLOCK_HAS_DIFF(d) (_mm512_test_epi8_mask((d), (d)) != 0)

// XOR four 64B pairs and OR the results; nonzero means at least one byte differs
static inline __m512i _memcmp_diff_block_avx512(const uint8_t *mem1, const uint8_t *mem2)
{
    __m512i z0 = _mm512_xor_si512(_mm512_loadu_si512(mem1),
            _mm512_loadu_si512(mem2));
    __m512i z1 = _mm512_xor_si512(_mm512_loadu_si512(mem1 + ZMM_SZ),
            _mm512_loadu_si512(mem2 + ZMM_SZ));
    __m512i z2 = _mm512_xor_si512(_mm512_loadu_si512(mem1 + 2 * ZMM_SZ),
            _mm512_loadu_si512(mem2 + 2 * ZMM_SZ));
    __m512i z3 = _mm512_xor_si512(_mm512_loadu_si512(mem1 + 3 * ZMM_SZ),
            _mm512_loadu_si512(mem2 + 3 * ZMM_SZ));

    z0 = _mm512_ternarylogic_epi32(z0, z1, z2, 0xFE);
    return _mm512_or_si512(z0, z3);
}

// Compare the final AVX512_VEC_4_SZ bytes and return the index of the first differing byte
// or -1 if no differences.
static inline size_t _memcmp_last_block_diff_avx512(const uint8_t *mem1, const uint8_t *mem2,
                                                size_t size)
{
    size_t size0 = size - 4 * ZMM_SZ;
    size_t size1 = size - 3 * ZMM_SZ;
    size_t size2 = size - 2 * ZMM_SZ;
    size_t size3 = size - ZMM_SZ;

    __mmask64 m0 = _mm512_cmpneq_epu8_mask(_mm512_loadu_si512(mem1 + size0),
                _mm512_loadu_si512(mem2 + size0));
    __mmask64 m1 = _mm512_cmpneq_epu8_mask(_mm512_loadu_si512(mem1 + size1),
                _mm512_loadu_si512(mem2 + size1));
    __mmask64 m2 = _mm512_cmpneq_epu8_mask(_mm512_loadu_si512(mem1 + size2),
                _mm512_loadu_si512(mem2 + size2));
    __mmask64 m3 = _mm512_cmpneq_epu8_mask(_mm512_loadu_si512(mem1 + size3),
                _mm512_loadu_si512(mem2 + size3));

    if (likely((m0 | m1 | m2 | m3) == 0))
        return -1;
    if (m0 != 0)
        return size0 + _tzcnt_u64(m0);
    if (m1 != 0)
        return size1 + _tzcnt_u64(m1);
    if (m2 != 0)
        return size2 + _tzcnt_u64(m2);
    return size3 + _tzcnt_u64(m3);
}

// Cold path: Find and return the first mismatching byte starting from offset.
static int __attribute__((noinline, cold)) _memcmp_locate_avx512(const uint8_t *mem1,
                                        const uint8_t *mem2, size_t offset, size_t size)
{
    size_t index;
    __mmask64 mask;

    while ((offset + ZMM_SZ) <= size)
    {
        mask = _mm512_cmpneq_epu8_mask(_mm512_loadu_si512(mem1 + offset),
            _mm512_loadu_si512(mem2 + offset));
        if (mask != 0)
        {
            index = offset + _tzcnt_u64(mask);
            return (int) mem1[index] - (int) mem2[index];
        }
        offset += ZMM_SZ;
    }

    if (offset < size)
    {
        // Overlapping final vector
        index = size - ZMM_SZ;
        mask = _mm512_cmpneq_epu8_mask(_mm512_loadu_si512(mem1 + index),
               _mm512_loadu_si512(mem2 + index));
        if (mask != 0)
        {
            index += _tzcnt_u64(mask);
            return (int) mem1[index] - (int) mem2[index];
        }
    }
    return 0;
}

HIDDEN_SYMBOL int __attribute__((flatten)) __memcmp_zen6(const void *mem1, const void *mem2,
                                        size_t size)
{
    LOG_INFO("\n");

    __m256i y0, y1;
    __mmask32 m0, m1;

    if (unlikely(size > YMM_SZ))
        goto gt_ymm;

    m0 = _bzhi_u32((uint32_t) -1, (uint32_t) size);
    y0 = _mm256_maskz_loadu_epi8(m0, mem1);
    y1 = _mm256_maskz_loadu_epi8(m0, mem2);
    m1 = _mm256_cmpneq_epu8_mask(y0, y1);
    if (m1 == 0)
        return 0;
    uint32_t pos = _tzcnt_u32(m1);
    return ((*(uint8_t *) (mem1 + pos)) - (*(uint8_t *) (mem2 + pos)));

gt_ymm:
    if (unlikely(mem1 == mem2))
        return 0;

    __m512i z0, z1;
    size_t offset = 0, index = 0;
    __mmask64 mask;

    if (size <= ZMM_SZ)
    {
        // 33-64B: two overlapping YMM compares
        y0 = _mm256_loadu_si256((const __m256i *) mem1);
        y1 = _mm256_loadu_si256((const __m256i *) mem2);
        m1 = _mm256_cmpneq_epu8_mask(y0, y1);
        if (m1 != 0)
        {
            uint32_t pos = _tzcnt_u32(m1);
            return ((*(uint8_t *) (mem1 + pos)) - (*(uint8_t *) (mem2 + pos)));
        }
        y0 = _mm256_loadu_si256((const __m256i *) (mem1 + size - YMM_SZ));
        y1 = _mm256_loadu_si256((const __m256i *) (mem2 + size - YMM_SZ));
        m1 = _mm256_cmpneq_epu8_mask(y0, y1);
        if (m1 != 0)
        {
            uint32_t pos = (size - YMM_SZ) + _tzcnt_u32(m1);
            return ((*(uint8_t *) (mem1 + pos)) - (*(uint8_t *) (mem2 + pos)));
        }
        return 0;
    }

    // >64B: compare the first ZMM, early-exit on mismatch.
    z0 = _mm512_loadu_si512(mem1);
    z1 = _mm512_loadu_si512(mem2);
    mask = _mm512_cmpneq_epu8_mask(z0, z1);
    if (unlikely(mask != 0))
    {
        index = _tzcnt_u64(mask);
        return ((*(uint8_t *) (mem1 + index)) - (*(uint8_t *) (mem2 + index)));
    }

    if (size <= 2 * ZMM_SZ)
    {
        // 65-128B: overlapping final ZMM.
        index = size - ZMM_SZ;
        z0 = _mm512_loadu_si512(mem1 + index);
        z1 = _mm512_loadu_si512(mem2 + index);
        mask = _mm512_cmpneq_epu8_mask(z0, z1);
        if (likely(mask == 0))
            return 0;
        index += _tzcnt_u64(mask);
        return ((*(uint8_t *) (mem1 + index)) - (*(uint8_t *) (mem2 + index)));
    }

    const uint8_t *src1 = (const uint8_t *) mem1;
    const uint8_t *src2 = (const uint8_t *) mem2;

    if (size <= AVX512_VEC_4_SZ)
    {
        // 129-256B: the three remaining ZMM blocks.
        __mmask64 m0 = _mm512_cmpneq_epu8_mask(_mm512_loadu_si512(src1 + ZMM_SZ),
                                            _mm512_loadu_si512(src2 + ZMM_SZ));
        __mmask64 m1 = _mm512_cmpneq_epu8_mask(_mm512_loadu_si512(src1 + size - 2 * ZMM_SZ),
                                            _mm512_loadu_si512(src2 + size - 2 * ZMM_SZ));
        __mmask64 m2 = _mm512_cmpneq_epu8_mask(_mm512_loadu_si512(src1 + size - ZMM_SZ),
                                            _mm512_loadu_si512(src2 + size - ZMM_SZ));

        if (likely((m0 | m1 | m2) == 0))
            return 0;
        if (m0 != 0)
            index = ZMM_SZ + _tzcnt_u64(m0);
        else if (m1 != 0)
            index = (size - 2 * ZMM_SZ) + _tzcnt_u64(m1);
        else
            index = (size - ZMM_SZ) + _tzcnt_u64(m2);
        return (int) src1[index] - (int) src2[index];
    }

    if (size <= 2 * AVX512_VEC_4_SZ)
    {
        // 257-512B: the seven remaining ZMM blocks in two groups.
        __mmask64 m0 = _mm512_cmpneq_epu8_mask(_mm512_loadu_si512(src1 + ZMM_SZ),
                                            _mm512_loadu_si512(src2 + ZMM_SZ));
        __mmask64 m1 = _mm512_cmpneq_epu8_mask(_mm512_loadu_si512(src1 + 2 * ZMM_SZ),
                                            _mm512_loadu_si512(src2 + 2 * ZMM_SZ));
        __mmask64 m2 = _mm512_cmpneq_epu8_mask(_mm512_loadu_si512(src1 + 3 * ZMM_SZ),
                                            _mm512_loadu_si512(src2 + 3 * ZMM_SZ));

        if (unlikely((m0 | m1 | m2) != 0))
        {
            if (m0 != 0)
                index = ZMM_SZ + _tzcnt_u64(m0);
            else if (m1 != 0)
                index = 2 * ZMM_SZ + _tzcnt_u64(m1);
            else
                index = 3 * ZMM_SZ + _tzcnt_u64(m2);
            return (int) src1[index] - (int) src2[index];
        }

        index = _memcmp_last_block_diff_avx512(src1, src2, size);
        if (likely(index == (size_t) -1))
            return 0;
        return (int) src1[index] - (int) src2[index];
    }

    // >512B: 4x unrolled 256B blocks
    if (unlikely((size >= MEMCMP_PREFETCH_MIN_SIZE) && (size <= MEMCMP_PREFETCH_MAX_SIZE)))
    {
        while ((offset + AVX512_VEC_4_SZ) <= size)
        {
            PREFETCH_NTA(src1 + offset + MEMCMP_PREFETCH_DISTANCE);
            PREFETCH_NTA(src1 + offset + MEMCMP_PREFETCH_DISTANCE + ZMM_SZ);
            PREFETCH_NTA(src1 + offset + MEMCMP_PREFETCH_DISTANCE + 2 * ZMM_SZ);
            PREFETCH_NTA(src1 + offset + MEMCMP_PREFETCH_DISTANCE + 3 * ZMM_SZ);
            PREFETCH_NTA(src2 + offset + MEMCMP_PREFETCH_DISTANCE);
            PREFETCH_NTA(src2 + offset + MEMCMP_PREFETCH_DISTANCE + ZMM_SZ);
            PREFETCH_NTA(src2 + offset + MEMCMP_PREFETCH_DISTANCE + 2 * ZMM_SZ);
            PREFETCH_NTA(src2 + offset + MEMCMP_PREFETCH_DISTANCE + 3 * ZMM_SZ);

            __m512i diff = _memcmp_diff_block_avx512(src1 + offset, src2 + offset);
            if (unlikely(BLOCK_HAS_DIFF(diff)))
                return _memcmp_locate_avx512(src1, src2, offset, size);
            offset += AVX512_VEC_4_SZ;
        }
    }
    else
    {
        while ((offset + AVX512_VEC_4_SZ) <= size)
        {
            __m512i diff = _memcmp_diff_block_avx512(src1 + offset, src2 + offset);
            if (unlikely(BLOCK_HAS_DIFF(diff)))
                return _memcmp_locate_avx512(src1, src2, offset, size);
            offset += AVX512_VEC_4_SZ;
        }
    }

    // Tail: a single overlapping 256B block.
    if (offset < size)
    {
        index = _memcmp_last_block_diff_avx512(src1, src2, size);
        if (unlikely(index != (size_t) -1))
            return (int) src1[index] - (int) src2[index];
    }
    return 0;
}

#ifndef ALMEM_DYN_DISPATCH
int memcmp(const void *, const void *, size_t) __attribute__((weak, alias("__memcmp_zen6")));
#endif
