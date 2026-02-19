/* Copyright (C) 2022-26 Advanced Micro Devices, Inc. All rights reserved.
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
#include <stdint.h>
#include "almem_defs.h"
#include "logger.h"
#include "threshold.h"
#include "zen_cpu_info.h"
#include <stddef.h>


extern cpu_info zen_info;

/* Zen5 L1D is 48 KB; memcmp touches two buffers, so per-buffer budget is
   ~24 KB.Threshold is set to 2/3 of L1D (32 KB), leaving headroom for
   the HW prefetcher to keep the sequential stream warm.
   Below this: XOR+OR+VPTEST loop (1 branch/256 B, reload on mismatch from L1D).
   At or above: direct vpcmpnequb loop (4 branches/256 B, immediate early-exit).
*/
#define MEMCMP_LOOP_STRATEGY_THRESHOLD (((zen_info.zen_cache_info.l1d_per_core) / 3) << 1)

static inline int __attribute__((flatten)) _memcmp_avx512(const void *mem1, const void *mem2, size_t size)
{
    __m512i z0, z1, z2, z3, z4, z5, z6, z7;
    size_t offset, index = 0;
    __mmask64 mask;

    if (unlikely(size == 0 || mem1 == mem2))
        return 0;

    if (likely(size <= ZMM_SZ))
    {
        __mmask64 mask;
        mask = _bzhi_u64((uint64_t) -1, (uint8_t) size);
        z0 = _mm512_setzero_epi32();
        z1 = _mm512_mask_loadu_epi8(z0, mask, mem1);
        z2 = _mm512_mask_loadu_epi8(z0, mask, mem2);

        mask = _mm512_cmpneq_epu8_mask(z1, z2);
        if (mask != 0)
        {
            index = _tzcnt_u64(mask);
            return ((*(uint8_t *) (mem1 + index)) - (*(uint8_t *) (mem2 + index)));
        }
        return 0;
    }

    z0 = _mm512_loadu_si512(mem2 + 0 * ZMM_SZ);
    z2 = _mm512_loadu_si512(mem1 + 0 * ZMM_SZ);
    mask = _mm512_cmpneq_epu8_mask(z0, z2);

    if (likely(size <= 2 * ZMM_SZ))
    {
        if (mask == 0)
        {
            index = size - ZMM_SZ;
            z1 = _mm512_loadu_si512(mem2 + index);
            z3 = _mm512_loadu_si512(mem1 + index);
            mask = _mm512_cmpneq_epu8_mask(z1, z3);
            if (mask == 0)
                return 0;
        }
        index += _tzcnt_u64(mask);
        return ((*(uint8_t *) (mem1 + index)) - (*(uint8_t *) (mem2 + index)));
    }

    else if (likely(size <= 4 * ZMM_SZ))
    {
        if (mask != 0)
        {
            index = _tzcnt_u64(mask);
            return ((*(uint8_t *) (mem1 + index)) - (*(uint8_t *) (mem2 + index)));
        }
        z1 = _mm512_loadu_si512(mem2 + 1 * ZMM_SZ);
        z5 = _mm512_loadu_si512(mem1 + 1 * ZMM_SZ);
        mask = _mm512_cmpneq_epu8_mask(z1, z5);
        if (mask != 0)
        {
            index = _tzcnt_u64(mask) + ZMM_SZ;
            return ((*(uint8_t *) (mem1 + index)) - (*(uint8_t *) (mem2 + index)));
        }
        z2 = _mm512_loadu_si512(mem2 + size - 2 * ZMM_SZ);
        z3 = _mm512_loadu_si512(mem2 + size - 1 * ZMM_SZ);
        z6 = _mm512_loadu_si512(mem1 + size - 2 * ZMM_SZ);
        z7 = _mm512_loadu_si512(mem1 + size - 1 * ZMM_SZ);
        mask = _mm512_cmpneq_epu8_mask(z2, z6);
        if (mask != 0)
        {
            index = _tzcnt_u64(mask) + size - 2 * ZMM_SZ;
            return ((*(uint8_t *) (mem1 + index)) - (*(uint8_t *) (mem2 + index)));
        }
        mask = _mm512_cmpneq_epu8_mask(z3, z7);
        if (mask != 0)
        {
            index = _tzcnt_u64(mask) + size - ZMM_SZ;
            return ((*(uint8_t *) (mem1 + index)) - (*(uint8_t *) (mem2 + index)));
        }
        return 0;
    }

    /* First 64B already compared above — early-exit on mismatch,
     * then start the loop from offset = ZMM_SZ. */
    if (mask != 0)
    {
        index = _tzcnt_u64(mask);
        return ((*(uint8_t *) (mem1 + index)) - (*(uint8_t *) (mem2 + index)));
    }
    offset = ZMM_SZ;

    // Hybrid approach: Choose strategy based on cache hierarchy
    // - XOR+VPTEST for 256B-32KB (data fits in L1D, high locality)
    if (size >= MEMCMP_LOOP_STRATEGY_THRESHOLD)
    {
        // Direct compare loop for large sizes (≥MEMCMP_LOOP_STRATEGY_THRESHOLD)
        // Shorter critical path, early exit advantage
        while ((size - (4 * ZMM_SZ)) > offset)
        {
            z0 = _mm512_loadu_si512(mem2 + offset + 0 * ZMM_SZ);
            z4 = _mm512_loadu_si512(mem1 + offset + 0 * ZMM_SZ);
            mask = _mm512_cmpneq_epu8_mask(z0, z4);
            if (mask != 0)
            {
                index = _tzcnt_u64(mask) + offset;
                return ((*(uint8_t *) (mem1 + index)) - (*(uint8_t *) (mem2 + index)));
            }
            z1 = _mm512_loadu_si512(mem2 + offset + 1 * ZMM_SZ);
            z5 = _mm512_loadu_si512(mem1 + offset + 1 * ZMM_SZ);
            mask = _mm512_cmpneq_epu8_mask(z1, z5);
            if (mask != 0)
            {
                index = _tzcnt_u64(mask) + offset + ZMM_SZ;
                return ((*(uint8_t *) (mem1 + index)) - (*(uint8_t *) (mem2 + index)));
            }
            z2 = _mm512_loadu_si512(mem2 + offset + 2 * ZMM_SZ);
            z6 = _mm512_loadu_si512(mem1 + offset + 2 * ZMM_SZ);
            mask = _mm512_cmpneq_epu8_mask(z2, z6);
            if (mask != 0)
            {
                index = _tzcnt_u64(mask) + offset + 2 * ZMM_SZ;
                return ((*(uint8_t *) (mem1 + index)) - (*(uint8_t *) (mem2 + index)));
            }
            z3 = _mm512_loadu_si512(mem2 + offset + 3 * ZMM_SZ);
            z7 = _mm512_loadu_si512(mem1 + offset + 3 * ZMM_SZ);
            mask = _mm512_cmpneq_epu8_mask(z3, z7);
            if (mask != 0)
            {
                index = _tzcnt_u64(mask) + offset + 3 * ZMM_SZ;
                return ((*(uint8_t *) (mem1 + index)) - (*(uint8_t *) (mem2 + index)));
            }
            offset += 4 * ZMM_SZ;
        }
    } else
    {
        // XOR+VPTEST loop for medium sizes (256B to MEMCMP_LOOP_STRATEGY_THRESHOLD)
        // Reduces branch overhead
        while ((size - (4 * ZMM_SZ)) > offset)
        {
            const uint8_t *p1 = (const uint8_t *) mem1 + offset;
            const uint8_t *p2 = (const uint8_t *) mem2 + offset;

            // Load and XOR all four vectors
            z0 = _mm512_loadu_si512(p2);
            z4 = _mm512_loadu_si512(p1);
            z0 = _mm512_xor_si512(z0, z4);

            z1 = _mm512_loadu_si512(p2 + ZMM_SZ);
            z5 = _mm512_loadu_si512(p1 + ZMM_SZ);
            z1 = _mm512_xor_si512(z1, z5);

            z2 = _mm512_loadu_si512(p2 + 2 * ZMM_SZ);
            z6 = _mm512_loadu_si512(p1 + 2 * ZMM_SZ);
            z2 = _mm512_xor_si512(z2, z6);

            z3 = _mm512_loadu_si512(p2 + 3 * ZMM_SZ);
            z7 = _mm512_loadu_si512(p1 + 3 * ZMM_SZ);
            z3 = _mm512_xor_si512(z3, z7);

            // OR all XOR results together
            z0 = _mm512_or_si512(z0, z1);
            z2 = _mm512_or_si512(z2, z3);
            z0 = _mm512_or_si512(z0, z2);

            // Single test: check if result is all-zeros (match)
            __mmask64 test_mask = _mm512_test_epi8_mask(z0, z0);

            if (test_mask != 0)
            {
                // Mismatch found - reload and compare individually
                z0 = _mm512_loadu_si512(p2);
                z4 = _mm512_loadu_si512(p1);
                mask = _mm512_cmpneq_epu8_mask(z0, z4);
                if (mask != 0)
                {
                    index = _tzcnt_u64(mask) + offset;
                    return ((*(uint8_t *) (mem1 + index)) - (*(uint8_t *) (mem2 + index)));
                }

                z1 = _mm512_loadu_si512(p2 + ZMM_SZ);
                z5 = _mm512_loadu_si512(p1 + ZMM_SZ);
                mask = _mm512_cmpneq_epu8_mask(z1, z5);
                if (mask != 0)
                {
                    index = _tzcnt_u64(mask) + offset + ZMM_SZ;
                    return ((*(uint8_t *) (mem1 + index)) - (*(uint8_t *) (mem2 + index)));
                }

                z2 = _mm512_loadu_si512(p2 + 2 * ZMM_SZ);
                z6 = _mm512_loadu_si512(p1 + 2 * ZMM_SZ);
                mask = _mm512_cmpneq_epu8_mask(z2, z6);
                if (mask != 0)
                {
                    index = _tzcnt_u64(mask) + offset + 2 * ZMM_SZ;
                    return ((*(uint8_t *) (mem1 + index)) - (*(uint8_t *) (mem2 + index)));
                }

                z3 = _mm512_loadu_si512(p2 + 3 * ZMM_SZ);
                z7 = _mm512_loadu_si512(p1 + 3 * ZMM_SZ);
                mask = _mm512_cmpneq_epu8_mask(z3, z7);
                index = _tzcnt_u64(mask) + offset + 3 * ZMM_SZ;
                return ((*(uint8_t *) (mem1 + index)) - (*(uint8_t *) (mem2 + index)));
            }
            offset += 4 * ZMM_SZ;
        }
    }
    if (offset == size)
        return 0;

    // remaining data to be compared
    uint16_t rem_data = size - offset;

    // data vector blocks to be compared
    uint8_t rem_vecs = ((rem_data) >> 6) + !!(rem_data & (0x3F));

    switch (rem_vecs)
    {
    case 4:
        z0 = _mm512_loadu_si512(mem2 + size - 4 * ZMM_SZ);
        z4 = _mm512_loadu_si512(mem1 + size - 4 * ZMM_SZ);
        mask = _mm512_cmpneq_epu8_mask(z0, z4);
        if (mask != 0)
        {
            index = _tzcnt_u64(mask) + size - 4 * ZMM_SZ;
            return ((*(uint8_t *) (mem1 + index)) - (*(uint8_t *) (mem2 + index)));
        }
        __attribute__((fallthrough));
    case 3:
        z1 = _mm512_loadu_si512(mem2 + size - 3 * ZMM_SZ);
        z5 = _mm512_loadu_si512(mem1 + size - 3 * ZMM_SZ);
        mask = _mm512_cmpneq_epu8_mask(z1, z5);
        if (mask != 0)
        {
            index = _tzcnt_u64(mask) + size - 3 * ZMM_SZ;
            return ((*(uint8_t *) (mem1 + index)) - (*(uint8_t *) (mem2 + index)));
        }
        __attribute__((fallthrough));
    case 2:
        z2 = _mm512_loadu_si512(mem2 + size - 2 * ZMM_SZ);
        z6 = _mm512_loadu_si512(mem1 + size - 2 * ZMM_SZ);
        mask = _mm512_cmpneq_epu8_mask(z2, z6);
        if (mask != 0)
        {
            index = _tzcnt_u64(mask) + size - 2 * ZMM_SZ;
            return ((*(uint8_t *) (mem1 + index)) - (*(uint8_t *) (mem2 + index)));
        }
        __attribute__((fallthrough));
    case 1:
        z3 = _mm512_loadu_si512(mem2 + size - 1 * ZMM_SZ);
        z7 = _mm512_loadu_si512(mem1 + size - 1 * ZMM_SZ);
        mask = _mm512_cmpneq_epu8_mask(z3, z7);
        if (mask != 0)
        {
            index = _tzcnt_u64(mask) + size - 1 * ZMM_SZ;
            return ((*(uint8_t *) (mem1 + index)) - (*(uint8_t *) (mem2 + index)));
        }
    }
    return 0;
}