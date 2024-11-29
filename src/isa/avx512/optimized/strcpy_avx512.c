/* Copyright (C) 2023-24 Advanced Micro Devices, Inc. All rights reserved.
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
#include <stdint.h>
#include <immintrin.h>
#include "alm_defs.h"
#include "zen_cpu_info.h"

static inline char * __attribute__((flatten)) _strcpy_avx512(char *dst, const char *src)
{
    size_t offset = 0, index = 0;
    __m512i z0, z1, z2, z3, z4, z5, z6, z7;
    uint64_t ret = 0, ret1 = 0, ret2 = 0;
    __mmask64 mask, mask2;

    z0 = _mm512_setzero_epi32 ();

    offset = (uintptr_t)src & (ZMM_SZ - 1);

    if (offset)
    {
        if ((PAGE_SZ - ZMM_SZ) < ((PAGE_SZ - 1) & (uintptr_t)src))
        {
            z7 = _mm512_set1_epi8(0xff);
            mask = ((uint64_t)-1) >> offset;
            z1 =  _mm512_mask_loadu_epi8(z7 ,mask, src);
            ret =_mm512_cmpeq_epu8_mask(z0, z1);
            if (ret)
            {
                index =  _tzcnt_u64(ret);
                mask2 = ((uint64_t)-1) >> (63 - index);
                _mm512_mask_storeu_epi8(dst, mask2, z1);
                return dst;
            }
        }
        z1 = _mm512_loadu_si512(src);
        ret =_mm512_cmpeq_epu8_mask(z0, z1);
        if (ret)
        {
            index =  _tzcnt_u64(ret);
            mask = ((uint64_t)-1) >> (63 - index);
            _mm512_mask_storeu_epi8(dst, mask, z1);
            return dst;
        }
        _mm512_storeu_si512(dst, z1);
        offset = ZMM_SZ - offset;
    }
    z1 = _mm512_load_si512(src + offset);
    ret =_mm512_cmpeq_epu8_mask(z0, z1);
    if (ret != 0)
    {
        index =  _tzcnt_u64(ret);
        mask = ((uint64_t)-1) >> (63 - index);
        _mm512_mask_storeu_epi8(dst + offset, mask, z1);
        return dst;
    }
    z2 = _mm512_load_si512(src + ZMM_SZ + offset);

    ret = _mm512_cmpeq_epu8_mask(z2, z0);
    if (ret != 0)
    {
        index = _tzcnt_u64(ret);
        mask = ((uint64_t)-1) >> (63 - index);
        _mm512_storeu_si512(dst + offset, z1);
        _mm512_mask_storeu_epi8(dst + ZMM_SZ + offset, mask, z2);
        return dst;
    }
    z3 = _mm512_load_si512(src + 2 * ZMM_SZ + offset);
    ret1 = _mm512_cmpeq_epu8_mask(z3, z0);
    if (ret1)
    {
        index = _tzcnt_u64(ret1);
        mask = ((uint64_t)-1) >> (63 - index);
        _mm512_storeu_si512(dst + offset, z1);
        _mm512_storeu_si512(dst + ZMM_SZ + offset, z2);
        _mm512_mask_storeu_epi8(dst + 2 * ZMM_SZ + offset, mask, z3);
        return dst;
    }
    z4 = _mm512_load_si512(src + 3 * ZMM_SZ + offset);
    ret = _mm512_cmpeq_epu8_mask(z4, z0);
    if (ret)
    {
        index = _tzcnt_u64(ret);
        mask = ((uint64_t)-1) >> (63 - index);
        _mm512_storeu_si512(dst + offset, z1);
        _mm512_storeu_si512(dst + ZMM_SZ + offset, z2);
        _mm512_storeu_si512(dst + 2 * ZMM_SZ + offset, z3);
        _mm512_mask_storeu_epi8(dst + 3 * ZMM_SZ + offset, mask, z4);
        return dst;
    }
    _mm512_storeu_si512(dst + offset, z1);
    _mm512_storeu_si512(dst + offset + ZMM_SZ, z2);
    _mm512_storeu_si512(dst + offset + 2 * ZMM_SZ, z3);
    _mm512_storeu_si512(dst + offset + 3 * ZMM_SZ, z4);
    offset += 4*ZMM_SZ;

    do
    {
        if ((PAGE_SZ - 4 * ZMM_SZ) >= ((PAGE_SZ - 1) & ((uintptr_t)src + offset)))
        {
            z1 = _mm512_loadu_si512(src + offset);
            ret =_mm512_cmpeq_epu8_mask(z0, z1);
            if (ret)
            {
                index =  _tzcnt_u64(ret);
                mask = ((uint64_t)-1) >> (63 - index);
                _mm512_mask_storeu_epi8(dst + offset, mask, z1);
                return dst;
            }
            z2 = _mm512_loadu_si512(src + ZMM_SZ + offset);

            ret = _mm512_cmpeq_epu8_mask(z2, z0);
            if (ret)
            {
                index = _tzcnt_u64(ret);
                mask = ((uint64_t)-1) >> (63 - index);
                _mm512_storeu_si512(dst + offset, z1);
                _mm512_mask_storeu_epi8(dst + ZMM_SZ + offset, mask, z2);
                return dst;
            }
            z3 = _mm512_loadu_si512(src + 2 * ZMM_SZ + offset);
            ret = _mm512_cmpeq_epu8_mask(z3, z0);
            if (ret)
            {
                index = _tzcnt_u64(ret);
                mask = ((uint64_t)-1) >> (63 - index);
                _mm512_storeu_si512(dst + offset, z1);
                _mm512_storeu_si512(dst + ZMM_SZ + offset, z2);
                _mm512_mask_storeu_epi8(dst + 2 * ZMM_SZ + offset, mask, z3);
                return dst;
            }
            z4 = _mm512_loadu_si512(src + 3 * ZMM_SZ + offset);
            ret = _mm512_cmpeq_epu8_mask(z4, z0);
            if (ret)
            {
                index = _tzcnt_u64(ret);
                mask = ((uint64_t)-1) >> (63 - index);
                _mm512_storeu_si512(dst + offset, z1);
                _mm512_storeu_si512(dst + ZMM_SZ + offset, z2);
                _mm512_storeu_si512(dst + 2 * ZMM_SZ + offset, z3);
                _mm512_mask_storeu_epi8(dst + 3 * ZMM_SZ + offset, mask, z4);
                return dst;
            }
            _mm512_storeu_si512(dst + offset, z1);
            _mm512_storeu_si512(dst + offset + ZMM_SZ, z2);
            _mm512_storeu_si512(dst + offset + 2 * ZMM_SZ, z3);
            _mm512_storeu_si512(dst + offset + 3 * ZMM_SZ, z4);
            offset += 4 * ZMM_SZ;
            if (offset > (PAGE_SZ - 4 * ZMM_SZ))
            offset = (offset & (-ZMM_SZ)) - ((uintptr_t)(dst) & (ZMM_SZ - 1));
        }

        z1 = _mm512_loadu_si512(src + offset);
        z2 = _mm512_loadu_si512(src + offset + ZMM_SZ);
        z3 = _mm512_loadu_si512(src + offset + 2 * ZMM_SZ);
        z4 = _mm512_loadu_si512(src + offset + 3 * ZMM_SZ);

        z5 = _mm512_min_epu8(z1,z2);
        z6 = _mm512_min_epu8(z3,z4);

        ret = _mm512_cmpeq_epu8_mask(_mm512_min_epu8(z5, z6), z0);
        if (ret != 0)
          break;

        _mm512_storeu_si512(dst + offset, z1);
        _mm512_storeu_si512(dst + offset + ZMM_SZ, z2);
        _mm512_storeu_si512(dst + offset + 2 * ZMM_SZ, z3);
        _mm512_storeu_si512(dst + offset + 3 * ZMM_SZ, z4);

        offset += 4 * ZMM_SZ;
    } while(1);

    //check for zero in regs: Z1, Z2
    if ((ret1 = _mm512_cmpeq_epu8_mask(z5, z0)))
    {
        if ((ret2 = _mm512_cmpeq_epu8_mask(z1, z0)))
        {
            index = _tzcnt_u64(ret2);
            mask = ((uint64_t)-1) >> (63 - index);
            _mm512_mask_storeu_epi8(dst + offset, mask, z1);
            return dst;
        }
        index = _tzcnt_u64(ret1);
        mask = ((uint64_t)-1) >> (63 - index);
        _mm512_storeu_si512(dst + offset, z1);
        _mm512_mask_storeu_epi8(dst + offset + ZMM_SZ, mask, z2);
        return dst;
    }
    //check for zero in regs: Z3, Z4
    else
    {
        _mm512_storeu_si512(dst + offset, z1);
        _mm512_storeu_si512(dst + offset + ZMM_SZ, z2);
        if ((ret2 =_mm512_cmpeq_epu8_mask(z3, z0)))
        {
            index = _tzcnt_u64(ret2);
            mask = ((uint64_t)-1) >> (63 - index);
            _mm512_mask_storeu_epi8(dst + offset + 2 * ZMM_SZ, mask, z3);
            return dst;
        }
        index = _tzcnt_u64(ret);
        mask = ((uint64_t)-1) >> (63 - index);
        _mm512_storeu_si512(dst + offset + 2 * ZMM_SZ, z3);
        _mm512_mask_storeu_epi8(dst + offset + 3 * ZMM_SZ, mask, z4);
    }
    return dst;
}
