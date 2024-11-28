/* Copyright (C) 2022-24 Advanced Micro Devices, Inc. All rights reserved.
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
#include "alm_defs.h"

static inline int  __attribute__((flatten)) _memcmp_avx512(const void *mem1, const void *mem2, size_t size)
{
    __m512i z0, z1, z2, z3, z4, z5, z6, z7;
    size_t offset = 0, index = 0;
    int64_t ret;

    if (size <= 1)
    {
        if (size != 0)
            return *(uint8_t*)mem1 - *(uint8_t*)mem2;
        return 0;
    }
    if (size <= ZMM_SZ)
    {
        __mmask64 mask;

        mask = ((uint64_t)-1) >> (64 - size);
        z0 = _mm512_setzero_epi32();
        z1 =  _mm512_mask_loadu_epi8(z0 ,mask, mem1);
        z2 =  _mm512_mask_loadu_epi8(z0 ,mask, mem2);

        ret = _mm512_cmpeq_epu8_mask(z1, z2) + 1;
        if (ret != 0)
        {
            index = _tzcnt_u64(ret);
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        return 0;
    }
    if (size <= 2 * ZMM_SZ)
    {
        z0 = _mm512_loadu_si512(mem2 + 0 * ZMM_SZ);
        z2 = _mm512_loadu_si512(mem1 + 0 * ZMM_SZ);

        ret = _mm512_cmpeq_epu8_mask(z0, z2) + 1;
        if (ret == 0)
        {
            index = size - ZMM_SZ;
            z1 = _mm512_loadu_si512(mem2 + index);
            z3 = _mm512_loadu_si512(mem1 + index);
            ret = _mm512_cmpeq_epu8_mask(z1, z3) + 1;
            if (ret == 0)
                return 0;
        }
        index += _tzcnt_u64(ret);
        return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
    }

    if (size <= 4 * ZMM_SZ)
    {
        z0 = _mm512_loadu_si512(mem2 + 0 * ZMM_SZ);
        z1 = _mm512_loadu_si512(mem2 + 1 * ZMM_SZ);
        z4 = _mm512_loadu_si512(mem1 + 0 * ZMM_SZ);
        z5 = _mm512_loadu_si512(mem1 + 1 * ZMM_SZ);
        ret = _mm512_cmpeq_epu8_mask(z0, z4) + 1;
        if (ret != 0)
        {
            index = _tzcnt_u64(ret);
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        ret = _mm512_cmpeq_epu8_mask(z1, z5) + 1;
        if (ret != 0)
        {
            index = _tzcnt_u64(ret) + ZMM_SZ;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        z2 = _mm512_loadu_si512(mem2 + size - 2 * ZMM_SZ);
        z3 = _mm512_loadu_si512(mem2 + size - 1 * ZMM_SZ);
        z6 = _mm512_loadu_si512(mem1 + size - 2 * ZMM_SZ);
        z7 = _mm512_loadu_si512(mem1 + size - 1 * ZMM_SZ);
        ret = _mm512_cmpeq_epu8_mask(z2, z6) + 1;
        if (ret != 0)
        {
            index = _tzcnt_u64(ret) + size - 2 * ZMM_SZ;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        ret = _mm512_cmpeq_epu8_mask(z3, z7) + 1;
        if (ret != 0)
        {
            index = _tzcnt_u64(ret) + size - ZMM_SZ;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        return 0;
    }
    size -= 4 * ZMM_SZ;
    while (size > offset)
    {
        z0 = _mm512_loadu_si512(mem2 + offset + 0 * ZMM_SZ);
        z4 = _mm512_loadu_si512(mem1 + offset + 0 * ZMM_SZ);
        ret = _mm512_cmpeq_epu8_mask(z0, z4) + 1;
        if (ret != 0)
        {
            index = _tzcnt_u64(ret) + offset;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        z1 = _mm512_loadu_si512(mem2 + offset + 1 * ZMM_SZ);
        z5 = _mm512_loadu_si512(mem1 + offset + 1 * ZMM_SZ);
        ret = _mm512_cmpeq_epu8_mask(z1, z5) + 1;
        if (ret != 0)
        {
            index = _tzcnt_u64(ret) + offset + ZMM_SZ;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        z2 = _mm512_loadu_si512(mem2 + offset + 2 * ZMM_SZ);
        z6 = _mm512_loadu_si512(mem1 + offset + 2 * ZMM_SZ);
        ret = _mm512_cmpeq_epu8_mask(z2, z6) + 1;
        if (ret != 0)
        {
            index = _tzcnt_u64(ret) + offset + 2 * ZMM_SZ;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        z3 = _mm512_loadu_si512(mem2 + offset + 3 * ZMM_SZ);
        z7 = _mm512_loadu_si512(mem1 + offset + 3 * ZMM_SZ);
        ret = _mm512_cmpeq_epu8_mask(z3, z7) + 1;
        if (ret != 0)
        {
            index = _tzcnt_u64(ret) + offset + 3 * ZMM_SZ;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        offset += 4 * ZMM_SZ;
    }
    size += 4 * ZMM_SZ;
    if ( offset == size)
        return 0;

    z0 = _mm512_loadu_si512(mem2 + size - 4 * ZMM_SZ);
    z4 = _mm512_loadu_si512(mem1 + size - 4 * ZMM_SZ);
    ret = _mm512_cmpeq_epu8_mask(z0, z4) + 1;
    if (ret != 0)
    {
        index = _tzcnt_u64(ret) + size - 4 * ZMM_SZ;
        return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
    }
    z1 = _mm512_loadu_si512(mem2 + size - 3 * ZMM_SZ);
    z5 = _mm512_loadu_si512(mem1 + size - 3 * ZMM_SZ);
    ret = _mm512_cmpeq_epu8_mask(z1, z5) + 1;
    if (ret != 0)
    {
        index = _tzcnt_u64(ret) + size - 3 * ZMM_SZ;
        return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
    }
    z2 = _mm512_loadu_si512(mem2 + size - 2 * ZMM_SZ);
    z6 = _mm512_loadu_si512(mem1 + size - 2 * ZMM_SZ);
    ret = _mm512_cmpeq_epu8_mask(z2, z6) + 1;
    if (ret != 0)
    {
        index = _tzcnt_u64(ret) + size - 2 * ZMM_SZ;
        return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
    }
    z3 = _mm512_loadu_si512(mem2 + size - 1 * ZMM_SZ);
    z7 = _mm512_loadu_si512(mem1 + size - 1 * ZMM_SZ);
    ret = _mm512_cmpeq_epu8_mask(z3, z7) + 1;
    if (ret != 0)
    {
        index = _tzcnt_u64(ret) + size - 1 * ZMM_SZ;
        return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
    }
    return 0;
}
