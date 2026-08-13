/* Copyright (C) 2024-26 Advanced Micro Devices, Inc. All rights reserved.
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

#include "../../base_impls/load_compare_impls.h"
#include "almem_defs.h"
#include "logger.h"
#include "zen_cpu_info.h"

HIDDEN_SYMBOL int __attribute__((flatten)) __memcmp_zen4(const void *mem1, const void *mem2, size_t size)
{
    LOG_INFO("\n");
    if (unlikely(size == 0 || mem1 == mem2))
        return 0;

    if (size <= YMM_SZ)
    {
        if (size <= QWORD_SZ)
        {
            if (size < DWORD_SZ)
            {
                /* 1-3B: byte-wise compare. */
                for (size_t i = 0; i < size; i++)
                {
                    int diff = (*(uint8_t *) (mem1 + i)) - (*(uint8_t *) (mem2 + i));
                    if (diff)
                        return diff;
                }
                return 0;
            }
            uint32_t diff = *(const uint32_t *) mem1 ^ *(const uint32_t *) mem2;
            if (diff)
            {
                size_t pos = _tzcnt_u32(diff) >> 3;
                return ((*(uint8_t *) (mem1 + pos)) - (*(uint8_t *) (mem2 + pos)));
            }
            diff = *(const uint32_t *) (mem1 + size - DWORD_SZ) ^ *(const uint32_t *) (mem2 + size - DWORD_SZ);
            if (diff)
            {
                size_t pos = (size - DWORD_SZ) + (_tzcnt_u32(diff) >> 3);
                return ((*(uint8_t *) (mem1 + pos)) - (*(uint8_t *) (mem2 + pos)));
            }
            return 0;
        }
        if (size <= XMM_SZ)
        {
            uint64_t diff = *(const uint64_t *) mem1 ^ *(const uint64_t *) mem2;
            if (diff)
            {
                size_t pos = _tzcnt_u64(diff) >> 3;
                return ((*(uint8_t *) (mem1 + pos)) - (*(uint8_t *) (mem2 + pos)));
            }
            diff = *(const uint64_t *) (mem1 + size - QWORD_SZ) ^ *(const uint64_t *) (mem2 + size - QWORD_SZ);
            if (diff)
            {
                size_t pos = (size - QWORD_SZ) + (_tzcnt_u64(diff) >> 3);
                return ((*(uint8_t *) (mem1 + pos)) - (*(uint8_t *) (mem2 + pos)));
            }
            return 0;
        }
        __m128i x0 = _mm_loadu_si128((const __m128i *) mem1);
        __m128i x1 = _mm_loadu_si128((const __m128i *) mem2);
        int mask = (~_mm_movemask_epi8(_mm_cmpeq_epi8(x0, x1))) & 0xFFFF;
        if (mask)
        {
            size_t pos = _tzcnt_u32(mask);
            return ((*(uint8_t *) (mem1 + pos)) - (*(uint8_t *) (mem2 + pos)));
        }
        x0 = _mm_loadu_si128((const __m128i *) (mem1 + size - XMM_SZ));
        x1 = _mm_loadu_si128((const __m128i *) (mem2 + size - XMM_SZ));
        mask = (~_mm_movemask_epi8(_mm_cmpeq_epi8(x0, x1))) & 0xFFFF;
        if (mask)
        {
            size_t pos = (size - XMM_SZ) + _tzcnt_u32(mask);
            return ((*(uint8_t *) (mem1 + pos)) - (*(uint8_t *) (mem2 + pos)));
        }
        return 0;
    }

    if (size > 8 * ZMM_SZ)
    {
        __m512i z0, z1, z2, z3, z4, z5, z6, z7;
        z0 = _mm512_loadu_si512(mem1);
        z1 = _mm512_loadu_si512(mem2);
        __mmask64 mask = _mm512_cmpneq_epu8_mask(z0, z1);
        if (mask != 0)
        {
            size_t index = _tzcnt_u64(mask);
            return ((*(uint8_t *) (mem1 + index)) - (*(uint8_t *) (mem2 + index)));
        }
        size_t offset = ZMM_SZ - ((uintptr_t) mem1 & (ZMM_SZ - 1));
        while (offset + 4 * ZMM_SZ <= size)
        {
            z0 = _mm512_loadu_si512(mem1 + offset);
            z1 = _mm512_loadu_si512(mem2 + offset);
            z2 = _mm512_loadu_si512(mem1 + offset + ZMM_SZ);
            z3 = _mm512_loadu_si512(mem2 + offset + ZMM_SZ);
            z4 = _mm512_loadu_si512(mem1 + offset + 2 * ZMM_SZ);
            z5 = _mm512_loadu_si512(mem2 + offset + 2 * ZMM_SZ);
            z6 = _mm512_loadu_si512(mem1 + offset + 3 * ZMM_SZ);
            z7 = _mm512_loadu_si512(mem2 + offset + 3 * ZMM_SZ);
            __mmask64 m0 = _mm512_cmpneq_epu8_mask(z0, z1);
            __mmask64 m1 = _mm512_cmpneq_epu8_mask(z2, z3);
            __mmask64 m2 = _mm512_cmpneq_epu8_mask(z4, z5);
            __mmask64 m3 = _mm512_cmpneq_epu8_mask(z6, z7);
            if ((m0 | m1 | m2 | m3) != 0)
            {
                size_t index;
                if (m0)
                    index = _tzcnt_u64(m0);
                else if (m1)
                    index = ZMM_SZ + _tzcnt_u64(m1);
                else if (m2)
                    index = 2 * ZMM_SZ + _tzcnt_u64(m2);
                else
                    index = 3 * ZMM_SZ + _tzcnt_u64(m3);
                index += offset;
                return ((*(uint8_t *) (mem1 + index)) - (*(uint8_t *) (mem2 + index)));
            }
            offset += 4 * ZMM_SZ;
        }
        while (offset + ZMM_SZ <= size)
        {
            z0 = _mm512_loadu_si512(mem1 + offset);
            z1 = _mm512_loadu_si512(mem2 + offset);
            mask = _mm512_cmpneq_epu8_mask(z0, z1);
            if (mask != 0)
            {
                size_t index = _tzcnt_u64(mask) + offset;
                return ((*(uint8_t *) (mem1 + index)) - (*(uint8_t *) (mem2 + index)));
            }
            offset += ZMM_SZ;
        }
        if (offset < size)
        {
            z0 = _mm512_loadu_si512(mem1 + size - ZMM_SZ);
            z1 = _mm512_loadu_si512(mem2 + size - ZMM_SZ);
            mask = _mm512_cmpneq_epu8_mask(z0, z1);
            if (mask != 0)
            {
                size_t index = (size - ZMM_SZ) + _tzcnt_u64(mask);
                return ((*(uint8_t *) (mem1 + index)) - (*(uint8_t *) (mem2 + index)));
            }
        }
        return 0;
    }

    VEC_1X_LOAD_COMPARE_MEMCMP(AVX2, UNALIGNED, 0);
    size_t offset = YMM_SZ;
    while (offset + 4 * YMM_SZ <= size)
    {
        VEC_1X_LOAD_COMPARE_MEMCMP(AVX2, UNALIGNED, offset);
        VEC_1X_LOAD_COMPARE_MEMCMP(AVX2, UNALIGNED, offset + YMM_SZ);
        VEC_1X_LOAD_COMPARE_MEMCMP(AVX2, UNALIGNED, offset + 2 * YMM_SZ);
        VEC_1X_LOAD_COMPARE_MEMCMP(AVX2, UNALIGNED, offset + 3 * YMM_SZ);
        offset += 4 * YMM_SZ;
    }
    while (offset + YMM_SZ <= size)
    {
        VEC_1X_LOAD_COMPARE_MEMCMP(AVX2, UNALIGNED, offset);
        offset += YMM_SZ;
    }
    if (offset < size)
        VEC_1X_LOAD_COMPARE_MEMCMP(AVX2, UNALIGNED, size - YMM_SZ);
    return 0;
}

#ifndef ALMEM_DYN_DISPATCH
int memcmp(const void *, const void *, size_t) __attribute__((weak, alias("__memcmp_zen4")));
#endif
