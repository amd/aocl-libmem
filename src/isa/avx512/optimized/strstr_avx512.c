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
#include "threshold.h"
#include <stdint.h>
#include <immintrin.h>
#include "alm_defs.h"


static inline size_t transition_index (const char *needle)
{
  size_t idx = 0;
  while (needle[idx + NULL_BYTE] != STR_TERM_CHAR &&
        needle[idx] == needle[idx + 1]) {
        idx ++;
  }
  return (needle[idx + 1] == STR_TERM_CHAR) ? 0 : idx;
}

static inline int cmp_needle_avx512(const char *str1, const char *str2, size_t size)
{
    size_t offset = 0;
    __m512i z1, z2, z3, z4, z5, z6, z7, z8;
    uint64_t  ret = 0, ret1 = 0, ret2 =0;
    __mmask64 mask;

    if (size <= ZMM_SZ)
    {
        if (size)
        {
            z7 = _mm512_set1_epi8(0xff);
            mask = ((uint64_t)-1) >> (ZMM_SZ - size);
            z1 =  _mm512_mask_loadu_epi8(z7 ,mask, str1);
            z2 =  _mm512_mask_loadu_epi8(z7 ,mask, str2);

            ret = _mm512_cmpneq_epu8_mask(z1,z2);

            if (ret)
                return -1;

        }
        return 0;
    }

    if (size <= 2 * ZMM_SZ)
    {
        z1 = _mm512_loadu_si512(str1);
        z2 = _mm512_loadu_si512(str2);

        ret =  _mm512_cmpneq_epu8_mask(z1,z2);
        if (!ret)
        {
            offset = size - ZMM_SZ;
            z3 = _mm512_loadu_si512(str1 + offset);
            z4 = _mm512_loadu_si512(str2 + offset);
            //Check for NULL
            ret = _mm512_cmpneq_epu8_mask(z3, z4);
            if(!ret)
                return 0;
        }
        return -1;
    }

    if (size <= 4* ZMM_SZ)
    {
        z1 = _mm512_loadu_si512(str1);
        z2 = _mm512_loadu_si512(str1 + ZMM_SZ);

        z5 = _mm512_loadu_si512(str2);
        z6 = _mm512_loadu_si512(str2 + ZMM_SZ);

        ret1 = _mm512_cmpneq_epu8_mask(z1,z5) | _mm512_cmpneq_epu8_mask(z2,z6);
        if (ret1)
            return -1;

        z3 = _mm512_loadu_si512(str1 + size - 2 * ZMM_SZ);
        z4 = _mm512_loadu_si512(str1 + size - ZMM_SZ);
        z7 = _mm512_loadu_si512(str2 + size - 2 * ZMM_SZ);
        z8 = _mm512_loadu_si512(str2 + size - ZMM_SZ);

        ret2 =  _mm512_cmpneq_epu8_mask(z3,z7) | _mm512_cmpneq_epu8_mask(z4,z8);
        if (ret2)
            return -1;

        return 0;
    }

    while ((size - offset) >= 4 * ZMM_SZ)
    {
        z1 = _mm512_loadu_si512(str1 + offset);
        z2 = _mm512_loadu_si512(str1 + offset + 1 * ZMM_SZ);

        z5 = _mm512_loadu_si512(str2 + offset);
        z6 = _mm512_loadu_si512(str2 + offset + 1 * ZMM_SZ);

        ret1 = _mm512_cmpneq_epu8_mask(z1,z5) | _mm512_cmpneq_epu8_mask(z2,z6);

        if (ret1)
            return -1;

        z3 = _mm512_loadu_si512(str1 + offset + 2 * ZMM_SZ);
        z4 = _mm512_loadu_si512(str1 + offset + 3 * ZMM_SZ);
        z7 = _mm512_loadu_si512(str2 + offset + 2 * ZMM_SZ);
        z8 = _mm512_loadu_si512(str2 + offset + 3 * ZMM_SZ);

        ret2 = _mm512_cmpneq_epu8_mask(z3,z7)| _mm512_cmpneq_epu8_mask(z4,z8);

        if (ret2)
            return -1;

        offset += 4 * ZMM_SZ;
    }
    uint8_t left_out = size - offset;
    if (!left_out)
        return 0;

    switch(left_out >> 6)
    {
        case 3:
            offset = size - 4 * ZMM_SZ;
            z1 = _mm512_loadu_si512(str1 + offset);
            z2 = _mm512_loadu_si512(str2 + offset);
            ret = _mm512_cmpneq_epu8_mask(z1,z2);
            if (ret)
                break;
        case 2:
            offset = size - 3 * ZMM_SZ;
            z3 = _mm512_loadu_si512(str1 + offset);
            z4 = _mm512_loadu_si512(str2 + offset);
            ret = _mm512_cmpneq_epu8_mask(z3, z4);
            if(ret)
                break;
        case 1:
            offset = size - 2 * ZMM_SZ;
            z1 = _mm512_loadu_si512(str1 + offset);
            z2 = _mm512_loadu_si512(str2 + offset);
            ret = _mm512_cmpneq_epu8_mask(z1,z2);
            if(ret)
                break;
        case 0:
            offset = size - ZMM_SZ;
            z3 = _mm512_loadu_si512(str1 + offset);
            z4 = _mm512_loadu_si512(str2 + offset);
            ret = _mm512_cmpneq_epu8_mask(z3, z4);

            if(!ret)
                return 0;
    }
    return -1;
}
static inline size_t _strlen_avx512(const char *str)
{
    size_t offset = 0;
    __m512i z0, z1, z2, z3, z4, z5, z6, z7;
    uint64_t ret = 0, ret1 = 0, ret2 = 0;
    __mmask64 mask;
    z0 = _mm512_setzero_epi32 ();

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
        ret = _mm512_cmpeq_epu8_mask(z1, z0);
        if (ret)
            return _tzcnt_u64(ret);
        offset = ZMM_SZ - offset;
    }

    z1 = _mm512_load_si512(str + offset);
    ret = _mm512_cmpeq_epu8_mask(z1, z0);

    if (!ret)
    {
        offset += ZMM_SZ;
        z2 = _mm512_load_si512(str + offset);
        //Check for NULL
        ret = _mm512_cmpeq_epu8_mask(z2, z0);
        if (!ret)
        {
            offset += ZMM_SZ;
            z3 = _mm512_load_si512(str + offset);
            //Check for NULL
            ret = _mm512_cmpeq_epu8_mask(z3, z0);
            if (!ret)
            {
                offset += ZMM_SZ;
                z4 = _mm512_load_si512(str + offset);
                //Check for NULL
                ret = _mm512_cmpeq_epu8_mask(z4, z0);
            }
        }
    }
    if (ret)
        return _tzcnt_u64(ret) + offset;

    offset +=  ZMM_SZ;
    uint64_t vec_offset = ((uintptr_t)str + offset) & (4 * ZMM_SZ - 1);
   if (vec_offset)
    {
        switch(vec_offset)
        {
            case ZMM_SZ:
                z1 = _mm512_load_si512(str + offset);
                ret = _mm512_cmpeq_epu8_mask(z1, z0);
                if (ret)
                    return _tzcnt_u64(ret) + offset;
                offset += ZMM_SZ;
            case (2 * ZMM_SZ):
                z2 = _mm512_load_si512(str + offset);
                ret = _mm512_cmpeq_epu8_mask(z2, z0);
                if (ret)
                    return _tzcnt_u64(ret) + offset;
                offset += ZMM_SZ;
            case (3 * ZMM_SZ):
                z3 = _mm512_load_si512(str + offset);
                ret = _mm512_cmpeq_epu8_mask(z3, z0);
                if (ret)
                    return _tzcnt_u64(ret) + offset;
                offset += ZMM_SZ;
        }
    }

    while (1)
    {
        z1 = _mm512_load_si512(str + offset);
        z2 = _mm512_load_si512(str + offset + ZMM_SZ);
        z3 = _mm512_load_si512(str + offset + 2 * ZMM_SZ);
        z4 = _mm512_load_si512(str + offset + 3 * ZMM_SZ);

        z5 = _mm512_min_epu8(z1,z2);
        z6 = _mm512_min_epu8(z3,z4);

        ret = _mm512_cmpeq_epu8_mask(_mm512_min_epu8(z5, z6), z0);
        if (ret != 0)
          break;

        offset += 4 * ZMM_SZ;
    }

    if  (ret)
    {
        if ((ret1 = _mm512_cmpeq_epu8_mask(z5, z0)))
        {
            if ((ret2 = _mm512_cmpeq_epu8_mask(z1, z0)))
            {
                return (_tzcnt_u64(ret2) + offset);
            }
            return (_tzcnt_u64(ret1) + ZMM_SZ + offset);
        }
        else
        {
            if ((ret2 =_mm512_cmpeq_epu8_mask(z3, z0)))
            {
                return (_tzcnt_u64(ret2) + 2 * ZMM_SZ + offset);
            }
            return  (_tzcnt_u64(ret) + 3 * ZMM_SZ + offset);
        }
    }
    return 0;
}

static inline char* find_needle_chr_avx512(const char *str, const char ch)
{
    size_t  offset;
    __m512i z0, z1, z2, z3, z7;
    uint64_t  cmp_idx , ret;
    __mmask64 mask;

    z0 = _mm512_setzero_epi32 ();

    offset = (uintptr_t)str & (ZMM_SZ - 1); //str alignment

    z2 =  _mm512_set1_epi8(ch);
    if  (offset)
    {
        if ((PAGE_SZ - ZMM_SZ) < ((PAGE_SZ - 1) & (uintptr_t)str))
        {
            z7 = _mm512_set1_epi8(0xff);
            mask = ((uint64_t)-1) >> offset;
            z1 =  _mm512_mask_loadu_epi8(z7 ,mask, str);
        }
        else
        {
            z1 = _mm512_loadu_si512(str);
        }

        ret = _mm512_cmpeq_epu8_mask(z1, z0) | _mm512_cmpeq_epu8_mask(z1, z2);
        if (ret)
        {
            cmp_idx = _tzcnt_u64(ret);
            if(str[cmp_idx] == ch)
                return (char*)str + cmp_idx;
            else
                return NULL;
        }
        offset = ZMM_SZ - offset;
    }
    while (1)
    {
        z1 = _mm512_load_si512(str + offset);
        //Check for NULL
        ret = _mm512_cmpeq_epu8_mask(z1, z0) | _mm512_cmpeq_epu8_mask(z1,z2);
        if (!ret)
        {
            offset += ZMM_SZ;
            z3 = _mm512_load_si512(str + offset);
            //Check for NULL
            ret = _mm512_cmpeq_epu8_mask(z3, z0) | _mm512_cmpeq_epu8_mask(z3, z2);
            if (!ret)
            {
                offset += ZMM_SZ;
                z1 = _mm512_load_si512(str + offset);
                //Check for NULL
                ret = _mm512_cmpeq_epu8_mask(z1, z0) | _mm512_cmpeq_epu8_mask(z1,z2);
                if (!ret)
                {
                    offset += ZMM_SZ;
                    z3 = _mm512_load_si512(str + offset);
                    //Check for NULL
                    ret = _mm512_cmpeq_epu8_mask(z3, z0) | _mm512_cmpeq_epu8_mask(z3, z2);
                }
            }
        }
        if (ret)
            break;
        offset += ZMM_SZ;
    }

    cmp_idx = _tzcnt_u64(ret) + offset;
    if (str[cmp_idx] == ch)
        return (char*)str + cmp_idx;
    return NULL;
}

static inline char * __attribute__((flatten)) _strstr_avx512(const char* haystack, const char* needle)
{
    size_t needle_len = 0;
    __m512i needle_char_t0, needle_char_t1;
    __m512i z0, z1, z2, z7;
    __mmask64 match_mask, null_mask, null_pfx_mask;
    uint64_t  match_idx, null_idx = 0;
    size_t offset;
    size_t transit_idx;
    uint8_t check_unaligned = 1;

    if (needle[0] == STR_TERM_CHAR)
        return (char*)haystack;

    if (haystack[0] == STR_TERM_CHAR)
        return NULL;

    if (needle[1] == STR_TERM_CHAR)
        return (char*)find_needle_chr_avx512(haystack, needle[0]);

    transit_idx = transition_index(needle);

    needle_char_t0 = _mm512_set1_epi8(needle[transit_idx]);
    needle_char_t1 = _mm512_set1_epi8(needle[transit_idx + 1]);

    z0 = _mm512_setzero_si512 ();
    offset = (uintptr_t)haystack & (ZMM_SZ - 1);
    if (unlikely ((PAGE_SZ - ZMM_SZ) < ((PAGE_SZ -1) & (uintptr_t)haystack)))
    {
        z7 = _mm512_set1_epi8(0xff);
        __mmask64 haystack_load_mask = ALL_BITS_SET >> offset;
        z1 = _mm512_mask_loadu_epi8(z7 , haystack_load_mask, haystack);
        null_mask = (uint64_t)_mm512_cmpeq_epi8_mask(z1, z0);
        if (null_mask)
        {
            null_idx = _tzcnt_u64(null_mask);
            if (null_idx <= transit_idx)
                return NULL;
            match_mask = (uint64_t)_mm512_cmpeq_epi8_mask(needle_char_t0, z1);
            check_unaligned = 0;
        }
    }
    if (check_unaligned)
    {
        z1 = _mm512_loadu_si512((void*)haystack);
        null_mask = _mm512_cmpeq_epi8_mask(z1, z0);
        if (null_mask)
        {
            null_idx = _tzcnt_u64(null_mask);

            if (null_idx <= transit_idx)
                return NULL;
        }
        match_mask = _mm512_cmpeq_epi8_mask(needle_char_t0, z1);
    }

    if (match_mask)
    {
        needle_len = _strlen_avx512(needle);

        if (null_mask)
        {
            match_idx = _tzcnt_u64(match_mask);

            if (match_idx > null_idx)
                return NULL;
            match_mask &= (null_mask ^ (null_mask - 1));
        }
        //Loop until match_mask is clear
        do{
            match_idx = _tzcnt_u64(match_mask);
            if ((*(haystack + match_idx + transit_idx + 1) == *(needle + transit_idx + 1)))
            {

                if (!(cmp_needle_avx512(((char*)(haystack + match_idx)), needle, needle_len)))
                    return (char*)(haystack + match_idx);
            }
            //clears the LOWEST SET BIT
            match_mask = _blsr_u64(match_mask);

        }while (match_mask);

    }

    if (null_mask)
        return NULL;

    offset = ZMM_SZ - offset;
    //Aligned case:
    if (!needle_len)
        needle_len = _strlen_avx512(needle);

    while(!null_mask)
    {
        z1 = _mm512_loadu_si512((void*)haystack + offset - 1);
        z2 = _mm512_load_si512((void*)haystack + offset);
        match_mask = _mm512_cmpeq_epi8_mask(z1, needle_char_t0) & _mm512_cmpeq_epi8_mask(z2, needle_char_t1);
        null_mask = (uint64_t)_mm512_cmpeq_epi8_mask(z2, z0);
        null_pfx_mask = (null_mask ^ (null_mask - 1));
        match_mask = match_mask & null_pfx_mask;
        //Occurence of NEEDLE's first char
        while (match_mask)
        {
            match_idx = _tzcnt_u64(match_mask);
            if ((match_idx + offset) >= (transit_idx + 1)) {
                match_idx += offset - transit_idx - 1;
                if (!(cmp_needle_avx512(((char*)(haystack + match_idx)), needle, needle_len)))
                    return (char*)(haystack + match_idx);
            }
            //clears the LOWEST SET BIT
            match_mask = _blsr_u64(match_mask);
        }
        offset += ZMM_SZ;
    }
    return NULL;
}
