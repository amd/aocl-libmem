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
#include "../../../base_impls/memset_erms_impls.h"
#include "zen_cpu_info.h"
#include "alm_defs.h"

extern cpu_info zen_info;

static inline void *_fill_null_avx512(void *mem, size_t size)
{
    __m512i z0;
    __m256i y0;
    size_t offset = 0;

    if (size < 2 * ZMM_SZ)
    {
        return __erms_stosb(mem, 0, size);
    }
    z0 = _mm512_set1_epi8(0);

    if (size <= 4 * ZMM_SZ)
    {
        _mm512_storeu_si512(mem , z0);
        _mm512_storeu_si512(mem + ZMM_SZ, z0);
        _mm512_storeu_si512(mem + size - 2 * ZMM_SZ, z0);
        _mm512_storeu_si512(mem + size - ZMM_SZ, z0);
        return mem;
    }
    _mm512_storeu_si512(mem + 0 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + 1 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + 2 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + 3 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + size - 4 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + size - 3 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + size - 2 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + size - 1 * ZMM_SZ, z0);


    if (size <= 8 * ZMM_SZ)
        return mem;

    offset += 4 * ZMM_SZ;
    size -= 4 * ZMM_SZ;
    offset -= ((uint64_t)mem & (ZMM_SZ-1));

    if (size < zen_info.zen_cache_info.l2_per_core)//L2 Cache Size
    {
        y0 = _mm256_set1_epi8(0);
        while( offset < size )
        {
        _mm256_store_si256(mem + offset + 0 * YMM_SZ, y0);
        _mm256_store_si256(mem + offset + 1 * YMM_SZ, y0);
        _mm256_store_si256(mem + offset + 2 * YMM_SZ, y0);
        _mm256_store_si256(mem + offset + 3 * YMM_SZ, y0);
        _mm256_store_si256(mem + offset + 4 * YMM_SZ, y0);
        _mm256_store_si256(mem + offset + 5 * YMM_SZ, y0);
        _mm256_store_si256(mem + offset + 6 * YMM_SZ, y0);
        _mm256_store_si256(mem + offset + 7 * YMM_SZ, y0);
        offset += 4 * ZMM_SZ;
        }
        return mem;
    }
    while( offset < size )
    {
        _mm512_store_si512(mem + offset + 0 * ZMM_SZ, z0);
        _mm512_store_si512(mem + offset + 1 * ZMM_SZ, z0);
        _mm512_store_si512(mem + offset + 2 * ZMM_SZ, z0);
        _mm512_store_si512(mem + offset + 3 * ZMM_SZ, z0);
        offset += 4 * ZMM_SZ;
    }

    return mem;
}


static inline char *  __attribute__((flatten)) _strncpy_avx512(char *dst, const char *src, size_t size)
{
    size_t offset = 0, index = 0;
    __m512i z0, z1, z2, z3, z4, z5, z6;
    uint64_t ret = 0, ret1 = 0, ret2 = 0;
    __mmask64 mask, mask2;

    z0 = _mm512_set1_epi8(0);

    if (size <= ZMM_SZ)
    {
        if (size == 0)
            return dst;

        mask = ((uint64_t)-1) >> (ZMM_SZ - size);
        z1 =  _mm512_mask_loadu_epi8(z0 ,mask, src);
        _mm512_mask_storeu_epi8(dst, mask, z1);
        ret = _mm512_cmpeq_epu8_mask(z0, z1) << 1;
        if (ret != 0)
        {
            index =  _tzcnt_u64(ret);
            mask2 = ((uint64_t)-1) <<  index;
            _mm512_mask_storeu_epi8(dst, mask2 & mask, z0);
        }
        return dst;
    }

    if (size <= 2 * ZMM_SZ)
    {
        z1 = _mm512_loadu_si512(src);
        _mm512_storeu_si512(dst, z1);

        ret = _mm512_cmpeq_epu8_mask(z0, z1);
        if (ret == 0)
        {
            offset = size - ZMM_SZ;
            z2 = _mm512_loadu_si512(src + offset);
            _mm512_storeu_si512(dst + offset, z2);
            ret = _mm512_cmpeq_epu8_mask(z0, z2) << 1;
            if (ret == 0)
                return dst;
        }
        index =  _tzcnt_u64(ret);
        if (index < ZMM_SZ)
            _fill_null_avx512(dst + index + offset, size - (offset + index));
        return dst;
    }

    if (size <= 4 * ZMM_SZ)
    {
        z1 = _mm512_loadu_si512(src);
        z2 = _mm512_loadu_si512(src + ZMM_SZ);
        z3 = _mm512_loadu_si512(src + size - 2 * ZMM_SZ);
        z4 = _mm512_loadu_si512(src + size - ZMM_SZ);

        _mm512_storeu_si512(dst, z1);
        _mm512_storeu_si512(dst + ZMM_SZ, z2);
        _mm512_storeu_si512(dst + size - 2 * ZMM_SZ, z3);
        _mm512_storeu_si512(dst + size - ZMM_SZ, z4);

        if ((ret1 = _mm512_cmpeq_epu8_mask(z0, _mm512_min_epu8(z1,z2))))
        {
            ret = _mm512_cmpeq_epu8_mask(z0, z1);
            if (ret == 0)
            {
                index = ZMM_SZ;
                ret = ret1;
            }
            index +=  _tzcnt_u64(ret);
            _fill_null_avx512(dst + index, size - index);
        }
        else if ((ret2 = _mm512_cmpeq_epu8_mask(z0, _mm512_min_epu8(z3,z4))))
        {
            index = size - 2 * ZMM_SZ;
            ret = _mm512_cmpeq_epu8_mask(z0, z3);
            if (ret == 0)
            {
                // last byte null case
                if ((ret2 << 1) == 0)
                     return dst;
                index = size - ZMM_SZ;
                ret = ret2;
            }
            index +=  _tzcnt_u64(ret);
            _fill_null_avx512(dst + index, size - index);
        }
        return dst;
    }

    z1 = _mm512_loadu_si512((void *)src + offset);
    z2 = _mm512_loadu_si512((void *)src + offset + ZMM_SZ);
    z3 = _mm512_loadu_si512((void *)src + offset + 2 * ZMM_SZ);
    z4 = _mm512_loadu_si512((void *)src + offset + 3 * ZMM_SZ);

    z5 = _mm512_min_epu8(z1, z2);
    z6 = _mm512_min_epu8(z3, z4);

    ret = _mm512_cmpeq_epu8_mask(_mm512_min_epu8(z5, z6), z0);

    if (!ret)
    {
        _mm512_storeu_si512((void*)dst + offset, z1);
        _mm512_storeu_si512((void*)dst + offset + ZMM_SZ, z2);
        _mm512_storeu_si512((void*)dst + offset + 2 * ZMM_SZ, z3);
        _mm512_storeu_si512((void*)dst + offset + 3 * ZMM_SZ, z4);

        if (size > 8*ZMM_SZ)
        {
            offset = 4 * ZMM_SZ - ((uintptr_t)dst & (ZMM_SZ - 1));
            while ((size - offset) >= 4*ZMM_SZ)
            {
                z1 = _mm512_loadu_si512((void *)src + offset);
                z2 = _mm512_loadu_si512((void *)src + offset + ZMM_SZ);
                z3 = _mm512_loadu_si512((void *)src + offset + 2 * ZMM_SZ);
                z4 = _mm512_loadu_si512((void *)src + offset + 3 * ZMM_SZ);

                z5 = _mm512_min_epu8(z1, z2);
                z6 = _mm512_min_epu8(z3, z4);

                ret = _mm512_cmpeq_epu8_mask(_mm512_min_epu8(z5, z6), z0);
                if (ret)
                    break;
                _mm512_store_si512((void*)dst + offset, z1);
                _mm512_store_si512((void*)dst + offset + ZMM_SZ, z2);
                _mm512_store_si512((void*)dst + offset + 2 * ZMM_SZ, z3);
                _mm512_store_si512((void*)dst + offset + 3 * ZMM_SZ, z4);

                offset += 4 * ZMM_SZ;
            }
        }
        if (!ret)
        {
            offset = size - 4*ZMM_SZ;
            z1 = _mm512_loadu_si512((void *)src + offset);
            z2 = _mm512_loadu_si512((void *)src + offset + ZMM_SZ);
            z3 = _mm512_loadu_si512((void *)src + offset + 2 * ZMM_SZ);
            z4 = _mm512_loadu_si512((void *)src + offset + 3 * ZMM_SZ);

            z5 = _mm512_min_epu8(z1, z2);
            z6 = _mm512_min_epu8(z3, z4);

            ret = _mm512_cmpeq_epu8_mask(_mm512_min_epu8(z5, z6), z0);
            if (ret == 0)
            {
                _mm512_storeu_si512((void*)dst + offset, z1);
                _mm512_storeu_si512((void*)dst + offset + ZMM_SZ, z2);
                _mm512_storeu_si512((void*)dst + offset + 2 * ZMM_SZ, z3);
                _mm512_storeu_si512((void*)dst + offset + 3 * ZMM_SZ, z4);

                return dst;
            }
        }
    }

    if ((ret2 = _mm512_cmpeq_epu8_mask(z5, z0)))
    {
        _mm512_storeu_si512((void*)dst + offset, z1);
        if (!(ret1 = _mm512_cmpeq_epu8_mask(z1, z0)))
        {
            _mm512_storeu_si512((void*)dst + offset + ZMM_SZ, z2);
            index += ZMM_SZ;
            ret = ret2;
        }
        else
            ret = ret1;
    }
    else
    {
        _mm512_storeu_si512((void*)dst + offset, z1);
        _mm512_storeu_si512((void*)dst + offset + ZMM_SZ, z2);
        _mm512_storeu_si512((void*)dst + offset + 2 * ZMM_SZ, z3);

        index += 2 * ZMM_SZ;
        if (!(ret1 = _mm512_cmpeq_epu8_mask(z3, z0)))
        {
                _mm512_storeu_si512((void*)dst + offset + 3 * ZMM_SZ, z4);
            index += ZMM_SZ;
        }
        else
            ret = ret1;
    }
    index += offset + _tzcnt_u64(ret) + 1;

    if (index < size)
        _fill_null_avx512(dst + index , size - index);

    return dst;
}
