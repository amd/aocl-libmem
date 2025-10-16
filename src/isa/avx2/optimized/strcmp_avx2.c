/* Copyright (C) 2023-25 Advanced Micro Devices, Inc. All rights reserved.
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
#include "almem_defs.h"
#include "zen_cpu_info.h"

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

#ifdef STRNCMP
static inline int __attribute__((flatten)) _strncmp_avx2(const char *str1, const char *str2, size_t size)
#else
static inline int __attribute__((flatten)) _strcmp_avx2(const char *str1, const char *str2)
#endif
{
#ifdef STRNCMP
    if (unlikely(size == 0))
        return 0;
#endif
    size_t offset1, offset2, offset, cmp_idx =0;
    __m256i y0, y1, y2, y3, y4, y_cmp, y_null;
    int32_t  ret;

    y0 = _mm256_setzero_si256();

    // Handle cases where start of either of the string is close to the end of a memory page
    if (unlikely(((PAGE_SZ - (2 * YMM_SZ)) < ((PAGE_SZ - 1) & ((uintptr_t)str1 | (uintptr_t)str2)))))
    {
        offset1 = (uintptr_t)str1 & (2 * YMM_SZ - 1);
        offset2 = (uintptr_t)str2 & (2 * YMM_SZ - 1);
        offset = (offset1 >= offset2) ? offset1 : offset2;

        if (offset < YMM_SZ)
        {
            y1 =  _mm256_loadu_si256((void *)str1);
            y2 =  _mm256_loadu_si256((void *)str2);
            y_cmp = _mm256_cmpeq_epi8(y1, y2);
            y_null = _mm256_cmpgt_epi8(y1, y0);
            ret = _mm256_movemask_epi8(_mm256_and_si256(y_cmp, y_null)) + 1;
            if (ret)
            {
                cmp_idx = _tzcnt_u32(ret);
#ifdef STRNCMP
                if (cmp_idx >= size)
                    return 0;
#endif
                return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
            }
#ifdef STRNCMP
            else if (size <= (YMM_SZ - offset))
                return 0;
#endif
            cmp_idx = _strcmp_ble_ymm(str1 + YMM_SZ, str2 + YMM_SZ, YMM_SZ - offset);
            if (cmp_idx != YMM_SZ)
            {
                cmp_idx += YMM_SZ;
#ifdef STRNCMP
                if (cmp_idx >= size)
                    return 0;
#endif
                return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
            }
        }
        else
        {
            cmp_idx = _strcmp_ble_ymm(str1, str2, (2 * YMM_SZ) - offset);
            if (cmp_idx != YMM_SZ)
            {
#ifdef STRNCMP
                if (cmp_idx >= size)
                    return 0;
#endif
                return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
            }
#ifdef STRNCMP
            else if (size <= (2 * YMM_SZ - offset))
                return 0;
#endif
        }
    }
    else
    {
        y1 =  _mm256_loadu_si256((void *)str1);
        y2 =  _mm256_loadu_si256((void *)str2);
        y_cmp = _mm256_cmpeq_epi8(y1, y2);
        y_null = _mm256_cmpgt_epi8(y1, y0);
        ret = _mm256_movemask_epi8(_mm256_and_si256(y_cmp, y_null)) + 1;
        if (ret)
        {
            cmp_idx = _tzcnt_u32(ret);
#ifdef STRNCMP
            if (cmp_idx >= size)
                return 0;
#endif
            return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
        }
#ifdef STRNCMP
        else if (size <= YMM_SZ)
        {
            return 0;
        }
#endif

        y3 =  _mm256_loadu_si256((void *)str1 + YMM_SZ);
        y4 =  _mm256_loadu_si256((void *)str2 + YMM_SZ);
        y_cmp = _mm256_cmpeq_epi8(y3, y4);
        y_null = _mm256_cmpgt_epi8(y3, y0);
        ret = _mm256_movemask_epi8(_mm256_and_si256(y_cmp, y_null)) + 1;
        if (ret)
        {
            cmp_idx = _tzcnt_u32(ret) + YMM_SZ;
#ifdef STRNCMP
            if (cmp_idx >= size)
                return 0;
#endif
            return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
        }
#ifdef STRNCMP
        else if (size <= 2 * YMM_SZ)
        {
            return 0;
        }
#endif
        offset1 = (uintptr_t)str1 & ((2 * YMM_SZ) - 1);
        offset2 = (uintptr_t)str2 & ((2 * YMM_SZ) - 1);
        offset = (offset1 >= offset2) ? offset1 : offset2;
    }
    // Adjust the offset to align with cache line for the next load operation
    offset = (2 * YMM_SZ) - offset;

    // Alignement of both the strings is same
    if (unlikely(offset1 == offset2))
    {
#ifdef STRNCMP
        while (offset < (size - 4 * YMM_SZ))
#else
        while (1)
#endif
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
#ifdef STRNCMP
            if (offset >= size)
                return 0;
#endif
        } // end of while for 4xVEC
        while (1)
        {
            y1 =  _mm256_load_si256((void *)str1 + offset);
            y2 =  _mm256_load_si256((void *)str2 + offset);
            y_cmp = _mm256_cmpeq_epi8(y1, y2);
            y_null = _mm256_cmpgt_epi8(y1, y0);
            ret = _mm256_movemask_epi8(_mm256_and_si256(y_cmp, y_null)) + 1;
            if (ret)
                break;
            offset += YMM_SZ;
#ifdef STRNCMP
            if (size <= offset)
                return 0;
#endif
        } // end of while for 1xVEC
    }
    else
    {
        char *aligned_str, *unaligned_str;
        if ((((uintptr_t)str1 + offset) & ((2 * YMM_SZ) - 1)) == 0)
        {
            aligned_str = (char *)str1;
            unaligned_str = (char *)str2;
        }
        else
        {
            aligned_str = (char *)str2;
            unaligned_str = (char *)str1;
        }

        uint16_t vecs_in_page = (PAGE_SZ - ((PAGE_SZ - 1) & ((uintptr_t)unaligned_str + offset))) >> 5;
        while (1)
        {
#ifdef STRNCMP
            while ((vecs_in_page >= 4) && (offset < (size - 4 * YMM_SZ)))
#else
            while (vecs_in_page >= 4)
#endif
            {
                y1 = _mm256_load_si256((void *)aligned_str + offset);
                y2 = _mm256_loadu_si256((void *)unaligned_str + offset);
                y_cmp = _mm256_cmpeq_epi8(y1, y2);
                y_null = _mm256_cmpgt_epi8(y1, y0);
                ret = _mm256_movemask_epi8(_mm256_and_si256(y_cmp, y_null)) + 1;

                if (!ret)
                {
                    offset += YMM_SZ;
                    y3 =  _mm256_load_si256((void *)aligned_str + offset);
                    y4 =  _mm256_loadu_si256((void *)unaligned_str + offset);
                    y_cmp = _mm256_cmpeq_epi8(y3, y4);
                    y_null = _mm256_cmpgt_epi8(y3, y0);
                    ret = _mm256_movemask_epi8(_mm256_and_si256(y_cmp, y_null)) + 1;

                    if (!ret)
                    {
                        offset += YMM_SZ;
                        y1 = _mm256_load_si256((void *)aligned_str + offset);
                        y2 = _mm256_loadu_si256((void *)unaligned_str + offset);
                        y_cmp = _mm256_cmpeq_epi8(y1, y2);
                        y_null = _mm256_cmpgt_epi8(y1, y0);
                        ret = _mm256_movemask_epi8(_mm256_and_si256(y_cmp, y_null)) + 1;

                        if (!ret)
                        {
                            offset += YMM_SZ;
                            y3 =  _mm256_load_si256((void *)aligned_str + offset);
                            y4 =  _mm256_loadu_si256((void *)unaligned_str + offset);
                            y_cmp = _mm256_cmpeq_epi8(y3, y4);
                            y_null = _mm256_cmpgt_epi8(y3, y0);
                            ret = _mm256_movemask_epi8(_mm256_and_si256(y_cmp, y_null)) + 1;

                            if (!ret)
                            {
                                offset += YMM_SZ;
                                vecs_in_page -= 4;
                                continue;
                            }
                        }
                    }
                }
                cmp_idx = _tzcnt_u32(ret) + offset;
#ifdef STRNCMP
                if (cmp_idx >= size)
                    return 0;
#endif
                return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
            }
            while (vecs_in_page --)
            {
                y1 = _mm256_load_si256((void *)aligned_str + offset);
                y2 = _mm256_loadu_si256((void *)unaligned_str + offset);
                y_cmp = _mm256_cmpeq_epi8(y1, y2);
                y_null = _mm256_cmpgt_epi8(y1, y0);
                ret = _mm256_movemask_epi8(_mm256_and_si256(y_cmp, y_null)) + 1;
                if (ret)
                {
                    cmp_idx = _tzcnt_u32(ret) + offset;
#ifdef STRNCMP
                    if (cmp_idx >= size)
                        return 0;
#endif
                    return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
                }
                offset += YMM_SZ;
#ifdef STRNCMP
                if (size <= offset)
                    return 0;
#endif
            }

            // Handle the case where we are crossing a page boundary for the unaligned string
            uint32_t mask_offset = (uintptr_t)(unaligned_str + offset) & (YMM_SZ - 1);
            cmp_idx = _strcmp_ble_ymm(aligned_str + offset, unaligned_str + offset, YMM_SZ - mask_offset);
            if (cmp_idx != YMM_SZ)
            {
                cmp_idx += offset;
#ifdef STRNCMP
                if (cmp_idx >= size)
                    return 0;
#endif
                return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
            }
#ifdef STRNCMP
            if (size <= (offset + YMM_SZ - mask_offset))
                return 0;
#endif
            vecs_in_page += PAGE_SZ / YMM_SZ;
        }
    }
    cmp_idx = _tzcnt_u32(ret) + offset;
#ifdef STRNCMP
    if (cmp_idx >= size)
        return 0;
#endif
    return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
}
