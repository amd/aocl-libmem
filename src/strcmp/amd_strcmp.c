/* Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
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

#define PAGE_SZ         4096
#define CACHELINE_SZ    64

static inline uint8_t _strcmp_ble_ymm(const char *str1, const char *str2, uint8_t size)
{
    __m128i x0, x1, x2, x3, x4, x_cmp, x_null;
    uint16_t ret = 0;
    uint8_t cmp_idx = 0;

    if (size == 1)
    {
        if ((*str1 == *str2) && (*str1 != '\0'))
            return YMM_SZ;
        return 0;
    }
    x0 = _mm_setzero_si128();
    if (size <= 2 * WORD_SZ)
    {
        x1 = _mm_loadu_si16((void *)str1);
        x2 = _mm_loadu_si16((void *)str2);
        x_cmp = _mm_cmpeq_epi8(x1, x2);
        x_null = _mm_cmpgt_epi8(x1, x0);
        ret = _mm_movemask_epi8(_mm_and_si128(x_cmp, x_null)) + 1;
        if (!(ret & 0x3))
        {
            cmp_idx = size - WORD_SZ;
            x3 = _mm_loadu_si16((void *)str1 + cmp_idx);
            x4 = _mm_loadu_si16((void *)str2 + cmp_idx);
            x_cmp = _mm_cmpeq_epi8(x3, x4);
            x_null = _mm_cmpgt_epi8(x3, x0);
            ret = _mm_movemask_epi8(_mm_and_si128(x_cmp, x_null)) + 1;
            if (!(ret & 0x3))
                 return YMM_SZ;
        }
        cmp_idx += ALM_TZCNT_U16(ret & 0x3);
        return cmp_idx;
    }
    if (size <= 2 * DWORD_SZ)
    {
        x1 = _mm_loadu_si32((void *)str1);
        x2 = _mm_loadu_si32((void *)str2);
        x_cmp = _mm_cmpeq_epi8(x1, x2);
        x_null = _mm_cmpgt_epi8(x1, x0);
        ret = _mm_movemask_epi8(_mm_and_si128(x_cmp, x_null)) + 1;
        if (!(ret & 0xf))
        {
            cmp_idx = size - DWORD_SZ;
            x3 = _mm_loadu_si32((void *)str1 + cmp_idx);
            x4 = _mm_loadu_si32((void *)str2 + cmp_idx);
            x_cmp = _mm_cmpeq_epi8(x3, x4);
            x_null = _mm_cmpgt_epi8(x3, x0);
            ret = _mm_movemask_epi8(_mm_and_si128(x_cmp, x_null)) + 1;
            if (!(ret & 0xf))
                 return YMM_SZ;
        }
        cmp_idx += ALM_TZCNT_U16(ret & 0xf);
        return cmp_idx;
    }
    if (size <= 2 * QWORD_SZ)
    {
        x1 = _mm_loadu_si64((void *)str1);
        x2 = _mm_loadu_si64((void *)str2);
        x_cmp = _mm_cmpeq_epi8(x1, x2);
        x_null = _mm_cmpgt_epi8(x1, x0);
        ret = _mm_movemask_epi8(_mm_and_si128(x_cmp, x_null)) + 1;
        if (!(ret & 0xff))
        {
            cmp_idx = size - QWORD_SZ;
            x3 = _mm_loadu_si64((void *)str1 + cmp_idx);
            x4 = _mm_loadu_si64((void *)str2 + cmp_idx);
            x_cmp = _mm_cmpeq_epi8(x3, x4);
            x_null = _mm_cmpgt_epi8(x3, x0);
            ret = _mm_movemask_epi8(_mm_and_si128(x_cmp, x_null)) + 1;
            if (!(ret & 0xff))
                 return YMM_SZ;
        }
        cmp_idx += ALM_TZCNT_U16(ret & 0xff);
        return cmp_idx;
    }
    if (size <= 2 * XMM_SZ)
    {
        x1 = _mm_loadu_si128((void *)str1);
        x2 = _mm_loadu_si128((void *)str2);
        x_cmp = _mm_cmpeq_epi8(x1, x2);
        x_null = _mm_cmpgt_epi8(x1, x0);
        ret = _mm_movemask_epi8(_mm_and_si128(x_cmp, x_null)) + 1;
        if (!ret)
        {
            cmp_idx = size - XMM_SZ;
            x3 = _mm_loadu_si128((void *)str1 + cmp_idx);
            x4 = _mm_loadu_si128((void *)str2 + cmp_idx);
            x_cmp = _mm_cmpeq_epi8(x3, x4);
            x_null = _mm_cmpgt_epi8(x3, x0);
            ret = _mm_movemask_epi8(_mm_and_si128(x_cmp, x_null)) + 1;
            if (!ret)
                return YMM_SZ;
        }
        cmp_idx += ALM_TZCNT_U16(ret);
        return cmp_idx;
    }
    return YMM_SZ;
}

static inline int _strcmp_avx2(const char *str1, const char *str2)
{
    size_t offset1 = 0, offset2 = 0, offset = 0, cmp_idx =0;
    __m256i y0, y1, y2, y3, y4, y_cmp, y_null;
    int32_t  ret = 0;

    y0 = _mm256_setzero_si256();
    offset1 = (uintptr_t)str1 & (YMM_SZ - 1); //str1 alignment
    offset2 = (uintptr_t)str2 & (YMM_SZ - 1); //str2 alignment
    //Both str2 and str1 are aligned

    if (offset1 == offset2)
    {
        offset = offset1;

        if (offset)
        {
            offset = YMM_SZ - offset;
            cmp_idx = _strcmp_ble_ymm(str1, str2, offset);
            if (cmp_idx != YMM_SZ)
            {
                return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
            }
        }

        while(1)
        { //sure that no NUll in the next
            y1 =  _mm256_load_si256((void *)str1 + offset);
            y2 =  _mm256_load_si256((void *)str2 + offset);

            y_cmp = _mm256_cmpeq_epi8(y1, y2);
            y_null = _mm256_cmpgt_epi8(y1, y0);
            ret = _mm256_movemask_epi8(_mm256_and_si256(y_cmp, y_null)) + 1;
            if (ret)
                break;

            offset += YMM_SZ;
            y3 =  _mm256_load_si256((void *)str1 + offset);
            y4 =  _mm256_load_si256((void *)str2 + offset);

            y_cmp = _mm256_cmpeq_epi8(y3, y4);
            y_null = _mm256_cmpgt_epi8(y3, y0);
            ret = _mm256_movemask_epi8(_mm256_and_si256(y_cmp, y_null)) + 1;
            if (ret)
                break;

            offset += YMM_SZ;
            y1 =  _mm256_load_si256((void *)str1 + offset);
            y2 =  _mm256_load_si256((void *)str2 + offset);

            y_cmp = _mm256_cmpeq_epi8(y1, y2);
            y_null = _mm256_cmpgt_epi8(y1, y0);
            ret = _mm256_movemask_epi8(_mm256_and_si256(y_cmp, y_null)) + 1;
            if (ret)
                break;

            offset += YMM_SZ;
            y3 =  _mm256_load_si256((void *)str1 + offset);
            y4 =  _mm256_load_si256((void *)str2 + offset);

            y_cmp = _mm256_cmpeq_epi8(y3, y4);
            y_null = _mm256_cmpgt_epi8(y3, y0);
            ret = _mm256_movemask_epi8(_mm256_and_si256(y_cmp, y_null)) + 1;
            if (ret)
                break;
            offset+=YMM_SZ;
        }
    }

    else
    {
        if (offset1 && offset2)
        {
            if (offset1 > offset2)
                offset = YMM_SZ - offset1;
            else
                offset = YMM_SZ - offset2;

            cmp_idx = _strcmp_ble_ymm(str1, str2, offset);
            if (cmp_idx != YMM_SZ)
            {
                return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
            }
        }

        while(1)
        {
            y1 =  _mm256_loadu_si256((void *)str1 + offset);
            y2 =  _mm256_loadu_si256((void *)str2 + offset);

            y_cmp = _mm256_cmpeq_epi8(y1, y2);
            y_null = _mm256_cmpgt_epi8(y1, y0);
            ret = _mm256_movemask_epi8(_mm256_and_si256(y_cmp, y_null)) + 1;
            if (ret)
                break;

            offset += YMM_SZ;
            y3 =  _mm256_loadu_si256((void *)str1 + offset);
            y4 =  _mm256_loadu_si256((void *)str2 + offset);

            y_cmp = _mm256_cmpeq_epi8(y3, y4);
            y_null = _mm256_cmpgt_epi8(y3, y0);
            ret = _mm256_movemask_epi8(_mm256_and_si256(y_cmp, y_null)) + 1;
            if (ret)
                break;

            offset += YMM_SZ;
            y1 =  _mm256_loadu_si256((void *)str1 + offset);
            y2 =  _mm256_loadu_si256((void *)str2 + offset);

            y_cmp = _mm256_cmpeq_epi8(y1, y2);
            y_null = _mm256_cmpgt_epi8(y1, y0);
            ret = _mm256_movemask_epi8(_mm256_and_si256(y_cmp, y_null)) + 1;
            if (ret)
                break;

            offset += YMM_SZ;
            y3 =  _mm256_loadu_si256((void *)str1 + offset);
            y4 =  _mm256_loadu_si256((void *)str2 + offset);

            y_cmp = _mm256_cmpeq_epi8(y3, y4);
            y_null = _mm256_cmpgt_epi8(y3, y0);
            ret = _mm256_movemask_epi8(_mm256_and_si256(y_cmp, y_null)) + 1;
            if (ret)
                break;
            offset+=YMM_SZ;
        }
    }
    cmp_idx = _tzcnt_u32(ret) + offset;
    return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
}

#ifdef AVX512_FEATURE_ENABLED
static inline int _strcmp_avx512(const char *str1, const char *str2)
{
    size_t offset1 = 0, offset2 = 0, offset = 0;
    __m512i z0, z1, z2, z3, z4, z7;
    uint64_t  cmp_idx = 0, ret = 0;
    __mmask64 mask;

    z0 = _mm512_setzero_epi32 ();

    offset1 = (uintptr_t)str1 & (ZMM_SZ - 1); //str1 alignment
    offset2 = (uintptr_t)str2 & (ZMM_SZ - 1); //str2 alignment
    //Both str2 and str1 are aligned

    if (offset1 == offset2)
    {
        offset = offset1;
        if (offset !=0)
        {
            z7 = _mm512_set1_epi8(0xff);
            mask = ((uint64_t)-1) >> offset;
            z1 =  _mm512_mask_loadu_epi8(z7 ,mask, str1);
            z2 =  _mm512_mask_loadu_epi8(z7 ,mask, str2);

            ret = _mm512_cmpeq_epu8_mask(z1, z0) | _mm512_cmpneq_epu8_mask(z1,z2);

            if (ret)
            {
                cmp_idx = _tzcnt_u64(ret);
                return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
            }
            offset = ZMM_SZ - offset;
        }
        while (1)
        {
            z1 = _mm512_load_si512(str1 + offset);
            z2 = _mm512_load_si512(str2 + offset);
            //Check for NULL
            ret = _mm512_cmpeq_epu8_mask(z1, z0) | _mm512_cmpneq_epu8_mask(z1,z2);
            if (!ret)
            {
                offset += ZMM_SZ;
                z3 = _mm512_load_si512(str1 + offset);
                z4 = _mm512_load_si512(str2 + offset);
                //Check for NULL
                ret = _mm512_cmpeq_epu8_mask(z3, z0) | _mm512_cmpneq_epu8_mask(z3, z4);
                if (!ret)
                {
                    offset += ZMM_SZ;
                    z1 = _mm512_load_si512(str1 + offset);
                    z2 = _mm512_load_si512(str2 + offset);
                    //Check for NULL
                    ret = _mm512_cmpeq_epu8_mask(z1, z0) | _mm512_cmpneq_epu8_mask(z1,z2);
                    if (!ret)
                    {
                        offset += ZMM_SZ;
                        z3 = _mm512_load_si512(str1 + offset);
                        z4 = _mm512_load_si512(str2 + offset);
                        //Check for NULL
                        ret = _mm512_cmpeq_epu8_mask(z3, z0) | _mm512_cmpneq_epu8_mask(z3, z4);
                    }
                }
            }
            if (ret)
                break;
            offset += ZMM_SZ;
        }
    }
    //mismatching alignments
    else
    {
        size_t mix_offset = 0;
        z7 = _mm512_set1_epi8(0xff);
        mix_offset = (offset1 >= offset2);
        offset = (mix_offset)*offset1 | (!mix_offset)*offset2;
        mask = ((uint64_t)-1) >> offset;

        z1 =  _mm512_mask_loadu_epi8(z7 ,mask, str1);
        z2 =  _mm512_mask_loadu_epi8(z7 ,mask, str2);

        ret = _mm512_cmpeq_epu8_mask(z1, z0) | _mm512_cmpneq_epu8_mask(z1,z2);

        if (ret)
        {
            cmp_idx = _tzcnt_u64(ret);
            return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
        }
        offset = ZMM_SZ - offset;
        while(1)
        {
            z1 = _mm512_loadu_si512(str1 + offset);
            z2 = _mm512_loadu_si512(str2 + offset);
            //Check for NULL
            ret = _mm512_cmpeq_epu8_mask(z1, z0) | _mm512_cmpneq_epu8_mask(z1,z2);
            if (!ret)
            {
                offset += ZMM_SZ;
                z3 = _mm512_loadu_si512(str1 + offset);
                z4 = _mm512_loadu_si512(str2 + offset);
                //Check for NULL
                ret = _mm512_cmpeq_epu8_mask(z3, z0) | _mm512_cmpneq_epu8_mask(z3, z4);
                if (!ret)
                {
                    offset += ZMM_SZ;
                    z1 = _mm512_loadu_si512(str1 + offset);
                    z2 = _mm512_loadu_si512(str2 + offset);
                    //Check for NULL
                    ret = _mm512_cmpeq_epu8_mask(z1, z0) | _mm512_cmpneq_epu8_mask(z1,z2);
                    if (!ret)
                    {
                        offset += ZMM_SZ;
                        z3 = _mm512_loadu_si512(str1 + offset);
                        z4 = _mm512_loadu_si512(str2 + offset);
                        //Check for NULL
                        ret = _mm512_cmpeq_epu8_mask(z3, z0) | _mm512_cmpneq_epu8_mask(z3, z4);
                    }
                }
            }
            if (ret)
                break;
            offset += ZMM_SZ;
        }
    }
    cmp_idx = _tzcnt_u64(ret) + offset;
    return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
}
#endif

int __attribute__((flatten)) amd_strcmp(char * __restrict mem1,
                                           const char * __restrict mem2)
{
    LOG_INFO("\n");

#ifdef AVX512_FEATURE_ENABLED
    return _strcmp_avx512(mem1, mem2);
#else
    return _strcmp_avx2(mem1, mem2);
#endif
}

int strcmp(const char *, const char *) __attribute__((weak, alias("amd_strcmp"),
                                                        visibility("default")));
