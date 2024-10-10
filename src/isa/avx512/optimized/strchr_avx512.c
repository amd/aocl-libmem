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
#include <stddef.h>
#include "logger.h"
#include <immintrin.h>
#include "alm_defs.h"
#include <zen_cpu_info.h>

static inline char* __attribute__((flatten)) _strchr_avx512(const char *str, int c)
{
    size_t  offset = 0,  ret;
    __m512i z0, z1, z2, z3, z4, z7, zch, zxor, zmin;
    __mmask64  match_mask, match_mask1, match_mask2, match_mask3, match_mask4, mask;
    char* result;
    char ch = (char)c;
    z0 = _mm512_setzero_epi32 ();
    zch = _mm512_set1_epi8(ch);

    offset = (uintptr_t)str & (ZMM_SZ - 1);
    if (offset != 0)
    {
        if ((PAGE_SZ - ZMM_SZ) < ((PAGE_SZ -1) & (uintptr_t)str))
        {
            z7 = _mm512_set1_epi8(0xff);
            mask = ((uint64_t)-1) >> offset;
            z1 = _mm512_mask_loadu_epi8(z7 ,mask, str);
        }
        else
        {
            z1 = _mm512_loadu_si512(str);
        }
        ret = _mm512_cmpeq_epu8_mask(_mm512_min_epu8(_mm512_xor_si512(z1, zch),z1), z0);
        if (ret)
            {
                if(*((char*)str + _tzcnt_u64(ret)) ==ch)
                    return ((char*)str + _tzcnt_u64(ret));
                return NULL;
            }
        offset = ZMM_SZ - offset;
    }

    z1 = _mm512_load_si512(str + offset);
    zxor = _mm512_xor_si512(z1, zch);
    zmin = _mm512_min_epu8(zxor, z1);
    ret = _mm512_testn_epi8_mask(zmin,zmin);
    if (!ret)
    {
        offset += ZMM_SZ;
        z2 = _mm512_load_si512(str + offset);
        zxor = _mm512_xor_si512(z2, zch);
        zmin = _mm512_min_epu8(zxor, z2);
        ret = _mm512_testn_epi8_mask(zmin,zmin);
        if (!ret)
        {
            offset += ZMM_SZ;
            z3 = _mm512_load_si512(str + offset);
            zxor = _mm512_xor_si512(z3, zch);
            zmin = _mm512_min_epu8(zxor, z3);
            ret = _mm512_testn_epi8_mask(zmin,zmin);
            if (!ret)
            {
                offset += ZMM_SZ;
                z4 =  _mm512_load_si512(str + offset);
                zxor = _mm512_xor_si512(z4, zch);
                zmin = _mm512_min_epu8(zxor, z4);
                ret = _mm512_testn_epi8_mask(zmin,zmin);
            }
        }
    }

    if (ret)
    {
        if(*((char*)str + _tzcnt_u64(ret) + offset) == ch)
            return (char*)str + _tzcnt_u64(ret) + offset;
        return NULL;
    }

    offset +=  ZMM_SZ;
    uint64_t vec_offset = ((uintptr_t)str + offset) & (4 * ZMM_SZ - 1);
    if (vec_offset)
    {
        switch(vec_offset)
        {
            case ZMM_SZ:
                z1 = _mm512_load_si512(str + offset);
                ret = _mm512_cmpeq_epu8_mask(_mm512_min_epu8(_mm512_xor_si512(z1, zch),z1), z0);
                if (ret)
                {
                    if(*((char*)str + _tzcnt_u64(ret) + offset) == ch )
                        return (char*)str + _tzcnt_u64(ret) + offset;
                    return NULL;
                }
                offset += ZMM_SZ;
            case (2 * ZMM_SZ):
                z2 = _mm512_load_si512(str + offset);
                ret = _mm512_cmpeq_epu8_mask(_mm512_min_epu8(_mm512_xor_si512(z2, zch),z2), z0);
                if (ret)
                {
                    if(*((char*)str + _tzcnt_u64(ret) + offset) == ch )
                        return (char*)str + _tzcnt_u64(ret) + offset;
                    return NULL;
                }
                offset += ZMM_SZ;
            case (3 * ZMM_SZ):
                z3 = _mm512_load_si512(str + offset);
                ret = _mm512_cmpeq_epu8_mask(_mm512_min_epu8(_mm512_xor_si512(z3, zch),z3), z0);
                if (ret)
                {
                    if(*((char*)str + _tzcnt_u64(ret) + offset) == ch )
                        return (char*)str + _tzcnt_u64(ret) + offset;
                    return NULL;
                }
                offset += ZMM_SZ;
        }
    }

    while(1)
    {
        z1 = _mm512_load_si512(str + offset);
        z2 = _mm512_load_si512(str + offset + 1 * ZMM_SZ);
        z3 = _mm512_load_si512(str + offset + 2 * ZMM_SZ);
        z4 = _mm512_load_si512(str + offset + 3 * ZMM_SZ);
        match_mask1 = _mm512_cmpneq_epu8_mask(z1, zch);
        match_mask2 = _mm512_cmpneq_epu8_mask(z2, zch);

        z2 = _mm512_min_epu8(z1, z2);
        match_mask3 = _mm512_mask_cmpneq_epu8_mask(match_mask1, z3, zch);
        match_mask4 = _mm512_mask_cmpneq_epu8_mask(match_mask2, z4, zch);
        z4 = _mm512_min_epu8(z3, z4);
        z4 = _mm512_maskz_min_epu8(match_mask3, z2, z4);
        match_mask = _mm512_mask_test_epi8_mask(match_mask4, z4, z4);

        if (match_mask != ALL_BITS_SET_64 )
            break;
        offset += 4 * ZMM_SZ;
    }
    match_mask1 = _mm512_mask_test_epi8_mask(match_mask1, z1, z1);
    if (match_mask1 != ALL_BITS_SET_64 )
    {
        result = (char*)(str + _tzcnt_u64(~match_mask1) + offset);
        if (!(*result ^ ch))
            return result;
        return NULL;
    }
    match_mask2 = _mm512_mask_test_epi8_mask(match_mask2, z2, z2);
    if (match_mask2 != ALL_BITS_SET_64)
    {
        result = (char*)(str + _tzcnt_u64(~match_mask2) + offset + 1 * ZMM_SZ);
        if (!(*result ^ ch))
            return result;
        return NULL;
    }
    match_mask3 = _mm512_mask_test_epi8_mask(match_mask3, z3, z3);
    if (match_mask3 != ALL_BITS_SET_64)
    {
        result = (char*)(str + _tzcnt_u64(~match_mask3) + offset + 2 * ZMM_SZ);
        if (!(*result ^ ch))
            return result;
        return NULL;
    }
        result = (char*)(str + _tzcnt_u64(~match_mask) + offset + 3 * ZMM_SZ);
        if (!(*result ^ ch))
            return result;
    return NULL;
}
