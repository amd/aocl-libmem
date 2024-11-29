/* Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
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
#include "logger.h"
#include "zen_cpu_info.h"
#include "alm_defs.h"

extern cpu_info zen_info;

static inline void * __attribute__((flatten)) _memchr_avx512(const void *mem, int val, size_t size)
{
    __m512i z0, z1, z2, z3, z4;
    uint64_t ret = 0, ret1 = 0, ret2 = 0, ret3 = 0;
    size_t index = 0, offset = 0;
    z0 = _mm512_set1_epi8((char)val);
    void *ret_ptr = (void *)mem;

    if (size == 0)
        return NULL;

    if (size <= ZMM_SZ)
    {
       __m512i z0, z1, z2;
        __mmask64 mask;
        mask = ((uint64_t)-1) >> (ZMM_SZ - size);
        z0 = _mm512_set1_epi8(val);
        z1 = _mm512_set1_epi8(val + 1);
        z2 =  _mm512_mask_loadu_epi8(z1, mask, mem);
        ret = _mm512_cmpeq_epu8_mask(z0, z2);
        if (ret == 0)
            return NULL;
        index = _tzcnt_u64(ret);
        return ret_ptr + index;
    }

    if (size <= 2 * ZMM_SZ)
    {
        z1 = _mm512_loadu_si512(mem);
        ret = _mm512_cmpeq_epu8_mask(z0, z1);

        if (ret == 0)
        {
            index = size - ZMM_SZ;
            z1 = _mm512_loadu_si512(ret_ptr + index);
            ret = _mm512_cmpeq_epu8_mask(z0, z1);

            if (ret == 0)
                return NULL;
        }
        index += _tzcnt_u64(ret);
        return ret_ptr + index;
    }

    if (size <= 4 * ZMM_SZ)
    {
        z1 = _mm512_loadu_si512(mem);
        z2 = _mm512_loadu_si512(mem + ZMM_SZ);
        z3 = _mm512_loadu_si512(mem + (size - 2 * ZMM_SZ));
        z4 = _mm512_loadu_si512(mem + (size - ZMM_SZ));

        ret = _mm512_cmpeq_epu8_mask(z0, z1);
        ret1 = ret | _mm512_cmpeq_epu8_mask(z0, z2);
        ret2 = _mm512_cmpeq_epu8_mask(z0, z3);
        ret3 = ret2 | _mm512_cmpeq_epu8_mask(z0, z4);

        if (ret1 | ret3)
        {
            if (ret1)
            {
                if (ret)
                {
                    index = _tzcnt_u64(ret);
                    return ret_ptr + index;
                }
                index = _tzcnt_u64(ret1) + ZMM_SZ;
                return ret_ptr + index;
            }
            if (ret2)
            {
                index = _tzcnt_u64(ret2) + (size - 2 * ZMM_SZ);
                return ret_ptr + index;
            }
            index = _tzcnt_u64(ret3) + (size - ZMM_SZ);
            return ret_ptr + index;
        }
        return NULL;
    }

    z1 = _mm512_loadu_si512(mem);
    ret = _mm512_cmpeq_epu8_mask(z0, z1);
    if (ret)
    {
        index = _tzcnt_u64(ret);
        return ret_ptr + index;
    }

    z2 = _mm512_loadu_si512(mem + ZMM_SZ);
    ret = _mm512_cmpeq_epu8_mask(z0, z2);
    if (ret)
    {
        index = _tzcnt_u64(ret) + ZMM_SZ;
        return ret_ptr + index;
    }
    z3 = _mm512_loadu_si512(mem + 2 * ZMM_SZ);
    ret = _mm512_cmpeq_epu8_mask(z0, z3);
    if (ret)
    {
        index = _tzcnt_u64(ret) + 2 * ZMM_SZ;
        return ret_ptr + index;
    }
    z4 = _mm512_loadu_si512(mem + 3 * ZMM_SZ);
    ret = _mm512_cmpeq_epu8_mask(z0, z4);
    if (ret)
    {
        index = _tzcnt_u64(ret) + 3 * ZMM_SZ;
        return ret_ptr + index;
    }

    offset = 4 * ZMM_SZ - ((uintptr_t)mem & (ZMM_SZ - 1));
    size -= 4 * ZMM_SZ;
    while (size >= offset)
    {
        z1 = _mm512_load_si512(mem + offset);
        z2 = _mm512_load_si512(mem + offset + ZMM_SZ);
        z3 = _mm512_load_si512(mem + offset + 2 * ZMM_SZ);
        z4 = _mm512_load_si512(mem + offset + 3 * ZMM_SZ);

        ret = _mm512_cmpeq_epu8_mask(z0, z1);
        ret1 = ret | _mm512_cmpeq_epu8_mask(z0, z2);
        ret2 = ret1 | _mm512_cmpeq_epu8_mask(z0, z3);
        ret3 = ret2 | _mm512_cmpeq_epu8_mask(z0, z4);

        if (ret3 != 0)
        {
            if (ret != 0)
            {
                index = _tzcnt_u64(ret) + offset;
                return ret_ptr + index;
            }
            if (ret1 != 0)
            {
                index = _tzcnt_u64(ret1) + offset + ZMM_SZ;
                return ret_ptr + index;
            }
            if (ret2 != 0)
            {
                index = _tzcnt_u64(ret2) + offset + 2 * ZMM_SZ;
                return ret_ptr + index;
            }
            index = _tzcnt_u64(ret3) + offset + 3 * ZMM_SZ;
            return ret_ptr + index;
        }
        offset += 4 * ZMM_SZ;
    }

    z1 = _mm512_loadu_si512(mem + size);
    ret = _mm512_cmpeq_epu8_mask(z0, z1);
    if (ret)
    {
        index = _tzcnt_u64(ret) + size;
        return ret_ptr + index;
    }

    z2 = _mm512_loadu_si512(mem + size + ZMM_SZ);
    ret = _mm512_cmpeq_epu8_mask(z0, z2);
    if (ret)
    {
        index = _tzcnt_u64(ret) + size + ZMM_SZ;
        return ret_ptr + index;
    }
    z3 = _mm512_loadu_si512(mem + size + 2 * ZMM_SZ);
    ret = _mm512_cmpeq_epu8_mask(z0, z3);
    if (ret)
    {
        index = _tzcnt_u64(ret) + size + 2 * ZMM_SZ;
        return ret_ptr + index;
    }
    z4 = _mm512_loadu_si512(mem + size + 3 * ZMM_SZ);
    ret = _mm512_cmpeq_epu8_mask(z0, z4);
    if (ret)
    {
        index = _tzcnt_u64(ret) + size + 3 * ZMM_SZ;
        return ret_ptr + index;
    }
    return NULL;
}
