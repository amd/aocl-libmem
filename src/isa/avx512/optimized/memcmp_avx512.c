/* Copyright (C) 2022-25 Advanced Micro Devices, Inc. All rights reserved.
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
#include <immintrin.h>
#include <stdint.h>
#include "zen_cpu_info.h"
#include "almem_defs.h"

static inline int  __attribute__((flatten)) _memcmp_avx512(const void *mem1, const void *mem2, size_t size)
{
    __m512i z0, z1, z2, z3, z4, z5, z6, z7;
    size_t offset = 0, index = 0;
    int64_t mask;

    if (likely(size <= ZMM_SZ))
    {
        __mmask64 mask;
        mask = _bzhi_u64((uint64_t)-1, (uint8_t)size);
        z0 = _mm512_setzero_epi32();
        z1 =  _mm512_mask_loadu_epi8(z0 ,mask, mem1);
        z2 =  _mm512_mask_loadu_epi8(z0 ,mask, mem2);

        mask = _mm512_cmpneq_epu8_mask(z1, z2);
        if (mask != 0)
        {
            index = _tzcnt_u64(mask);
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        return 0;
    }
    else if (likely(size <= 2 * ZMM_SZ))
    {
        z0 = _mm512_loadu_si512(mem2 + 0 * ZMM_SZ);
        z2 = _mm512_loadu_si512(mem1 + 0 * ZMM_SZ);

        mask = _mm512_cmpneq_epu8_mask(z0, z2);
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
        return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
    }

    else if (likely(size <= 4 * ZMM_SZ))
    {
        z0 = _mm512_loadu_si512(mem2 + 0 * ZMM_SZ);
        z1 = _mm512_loadu_si512(mem2 + 1 * ZMM_SZ);
        z4 = _mm512_loadu_si512(mem1 + 0 * ZMM_SZ);
        z5 = _mm512_loadu_si512(mem1 + 1 * ZMM_SZ);
        mask = _mm512_cmpneq_epu8_mask(z0, z4);
        if (mask != 0)
        {
            index = _tzcnt_u64(mask);
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        mask = _mm512_cmpneq_epu8_mask(z1, z5);
        if (mask != 0)
        {
            index = _tzcnt_u64(mask) + ZMM_SZ;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        z2 = _mm512_loadu_si512(mem2 + size - 2 * ZMM_SZ);
        z3 = _mm512_loadu_si512(mem2 + size - 1 * ZMM_SZ);
        z6 = _mm512_loadu_si512(mem1 + size - 2 * ZMM_SZ);
        z7 = _mm512_loadu_si512(mem1 + size - 1 * ZMM_SZ);
        mask = _mm512_cmpneq_epu8_mask(z2, z6);
        if (mask != 0)
        {
            index = _tzcnt_u64(mask) + size - 2 * ZMM_SZ;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        mask = _mm512_cmpneq_epu8_mask(z3, z7);
        if (mask != 0)
        {
            index = _tzcnt_u64(mask) + size - ZMM_SZ;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        return 0;
    }

    while ((size - (4 * ZMM_SZ)) > offset)
    {
        z0 = _mm512_loadu_si512(mem2 + offset + 0 * ZMM_SZ);
        z4 = _mm512_loadu_si512(mem1 + offset + 0 * ZMM_SZ);
        mask = _mm512_cmpneq_epu8_mask(z0, z4);
        if (mask != 0)
        {
            index = _tzcnt_u64(mask) + offset;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        z1 = _mm512_loadu_si512(mem2 + offset + 1 * ZMM_SZ);
        z5 = _mm512_loadu_si512(mem1 + offset + 1 * ZMM_SZ);
        mask = _mm512_cmpneq_epu8_mask(z1, z5);
        if (mask != 0)
        {
            index = _tzcnt_u64(mask) + offset + ZMM_SZ;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        z2 = _mm512_loadu_si512(mem2 + offset + 2 * ZMM_SZ);
        z6 = _mm512_loadu_si512(mem1 + offset + 2 * ZMM_SZ);
        mask = _mm512_cmpneq_epu8_mask(z2, z6);
        if (mask != 0)
        {
            index = _tzcnt_u64(mask) + offset + 2 * ZMM_SZ;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        z3 = _mm512_loadu_si512(mem2 + offset + 3 * ZMM_SZ);
        z7 = _mm512_loadu_si512(mem1 + offset + 3 * ZMM_SZ);
        mask = _mm512_cmpneq_epu8_mask(z3, z7);
        if (mask != 0)
        {
            index = _tzcnt_u64(mask) + offset + 3 * ZMM_SZ;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        offset += 4 * ZMM_SZ;
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
                return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
            }
            __attribute__ ((fallthrough));
        case 3:
            z1 = _mm512_loadu_si512(mem2 + size - 3 * ZMM_SZ);
            z5 = _mm512_loadu_si512(mem1 + size - 3 * ZMM_SZ);
            mask = _mm512_cmpneq_epu8_mask(z1, z5);
            if (mask != 0)
            {
                index = _tzcnt_u64(mask) + size - 3 * ZMM_SZ;
                return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
            }
            __attribute__ ((fallthrough));
        case 2:
            z2 = _mm512_loadu_si512(mem2 + size - 2 * ZMM_SZ);
            z6 = _mm512_loadu_si512(mem1 + size - 2 * ZMM_SZ);
            mask = _mm512_cmpneq_epu8_mask(z2, z6);
            if (mask != 0)
            {
                index = _tzcnt_u64(mask) + size - 2 * ZMM_SZ;
                return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
            }
            __attribute__ ((fallthrough));
        case 1:
            z3 = _mm512_loadu_si512(mem2 + size - 1 * ZMM_SZ);
            z7 = _mm512_loadu_si512(mem1 + size - 1 * ZMM_SZ);
            mask = _mm512_cmpneq_epu8_mask(z3, z7);
            if (mask != 0)
            {
                index = _tzcnt_u64(mask) + size - 1 * ZMM_SZ;
                return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
            }
    }
    return 0;
}
