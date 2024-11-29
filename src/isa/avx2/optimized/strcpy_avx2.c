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
#include <zen_cpu_info.h>

static inline void strcpy_ble_ymm(void *dst, const void *src, uint32_t size)
{
    __m128i x0,x1;

    if (size == 1)
    {
        *((uint8_t *)dst) = *((uint8_t *)src);
        return;
    }
    if (size <= 2 * WORD_SZ)
    {
        *((uint16_t*)dst) = *((uint16_t*)src);
        *((uint16_t*)(dst + size - WORD_SZ)) =
                *((uint16_t*)(src + size - WORD_SZ));
        return;
    }
    if (size <= 2 * DWORD_SZ)
    {
        *((uint32_t*)dst) = *((uint32_t*)src);
        *((uint32_t*)(dst + size - DWORD_SZ)) =
                *((uint32_t*)(src + size - DWORD_SZ));
        return;
    }
    if (size <= 2 * QWORD_SZ)
    {
        *((uint64_t*)dst) = *((uint64_t*)src);
        *((uint64_t*)(dst + size - QWORD_SZ)) =
                *((uint64_t*)(src + size - QWORD_SZ));
        return;
    }
    x0 = _mm_loadu_si128((void*)src);
    _mm_storeu_si128((void*)dst, x0);
    x1 = _mm_loadu_si128((void*)(src + size - XMM_SZ));
    _mm_storeu_si128((void*)(dst + size - XMM_SZ), x1);
    return;
}

static inline char * __attribute__((flatten)) _strcpy_avx2(char *dst, const char *src)
{
    size_t offset = 0, index = 0;
    uint32_t ret = 0, ret1 = 0, ret2 = 0;
    __m256i y0, y1, y2, y3, y4, y5, y6, y_cmp;

    y0 = _mm256_setzero_si256();
    offset = (size_t)src & (YMM_SZ - 1);
    if (offset)
    {
        //check for offset falling in the last vec of the page
        if ((PAGE_SZ - YMM_SZ) < ((PAGE_SZ - 1) & (uintptr_t)src))
        {
            y1 = _mm256_load_si256((void *)src - offset);
            y_cmp = _mm256_cmpeq_epi8(y0, y1);

            ret = (uint32_t)_mm256_movemask_epi8(y_cmp) >> offset;
            if (ret)
            {
                index = _tzcnt_u32(ret) + 1;
                strcpy_ble_ymm(dst, src, index);
                return dst;
            }
        }
        y1 = _mm256_loadu_si256((void *)src);
        y_cmp = _mm256_cmpeq_epi8(y0, y1);

        ret = _mm256_movemask_epi8(y_cmp);
        if (ret)
        {
            index =  _tzcnt_u32(ret) + 1;
            strcpy_ble_ymm(dst, src, index);
            return dst;
        }
        _mm256_storeu_si256((void *)dst, y1);
        offset = YMM_SZ - offset;
    }

    y1 = _mm256_load_si256((void *)src + offset);
    y_cmp = _mm256_cmpeq_epi8(y0, y1);

    ret = _mm256_movemask_epi8(y_cmp);
    if (ret)
    {
        index = _tzcnt_u32(ret) + 1;
        strcpy_ble_ymm(dst + offset, src + offset, index);
        return dst;
    }
    y2 = _mm256_load_si256((void *)src + YMM_SZ + offset);
    y_cmp = _mm256_cmpeq_epi8(y0, y2);
    ret = _mm256_movemask_epi8(y_cmp);

    if (ret != 0)
    {
        index = _tzcnt_u32(ret) + 1;
        y2 = _mm256_loadu_si256((void*)src + offset + index);
        _mm256_storeu_si256((void *)dst + offset, y1);
        _mm256_storeu_si256((void*)dst + offset + index, y2);
        return dst;
    }

    y3 = _mm256_load_si256((void *)src + 2 * YMM_SZ + offset);
    y_cmp = _mm256_cmpeq_epi8(y3, y0);
    ret = _mm256_movemask_epi8(y_cmp);
    if (ret != 0)
    {
        index = _tzcnt_u32(ret) + 1;
        y3 = _mm256_loadu_si256((void*)src + index + YMM_SZ + offset);
        _mm256_storeu_si256((void*)dst + offset, y1);
        _mm256_storeu_si256((void*)dst + YMM_SZ + offset, y2);
        _mm256_storeu_si256((void*)dst + YMM_SZ + index + offset, y3);
        return dst;
    }

    y4 = _mm256_load_si256((void *)src + 3 * YMM_SZ + offset);
    y_cmp = _mm256_cmpeq_epi8(y4, y0);
    ret = _mm256_movemask_epi8(y_cmp);
    if (ret != 0)
    {
        index = _tzcnt_u32(ret) + 1;
        y4 = _mm256_loadu_si256((void*)src + index + 2 * YMM_SZ + offset);
        _mm256_storeu_si256((void*)dst + offset, y1);
        _mm256_storeu_si256((void*)dst + YMM_SZ + offset, y2);
        _mm256_storeu_si256((void*)dst + 2 * YMM_SZ + offset, y3);
        _mm256_storeu_si256((void*)dst + 2 * YMM_SZ + index + offset, y4);
        return dst;
    }

    _mm256_storeu_si256((void*)dst + offset, y1);
    _mm256_storeu_si256((void*)dst + offset + YMM_SZ, y2);
    _mm256_storeu_si256((void*)dst + offset + 2 * YMM_SZ, y3);
    _mm256_storeu_si256((void*)dst + offset + 3 * YMM_SZ, y4);

    offset += 4 * YMM_SZ;

    while ((PAGE_SZ - 4 * YMM_SZ) < ((PAGE_SZ - 1) & ((uintptr_t)src + offset)))
    {
        y1 = _mm256_load_si256((void *)src + offset);
        y2 = _mm256_load_si256((void *)src + offset + YMM_SZ);
        y3 = _mm256_load_si256((void *)src + offset + 2 * YMM_SZ);
        y4 = _mm256_load_si256((void *)src + offset + 3 * YMM_SZ);

        y5 = _mm256_min_epu8(y1, y2);
        y6 = _mm256_min_epu8(y3, y4);

        y_cmp = _mm256_cmpeq_epi8(_mm256_min_epu8(y5, y6), y0);
        ret = _mm256_movemask_epi8(y_cmp);

        if (ret)
            break;

        _mm256_storeu_si256((void*)dst + offset, y1);
        _mm256_storeu_si256((void*)dst + offset + YMM_SZ, y2);
        _mm256_storeu_si256((void*)dst + offset + 2 * YMM_SZ, y3);
        _mm256_storeu_si256((void*)dst + offset + 3 * YMM_SZ, y4);

        offset += 4 * YMM_SZ;
    }
    if (!ret)
    {
        offset = (offset & (-YMM_SZ)) - ((uintptr_t)(dst) & (YMM_SZ - 1));
        while(1)
        {
            y1 = _mm256_loadu_si256((void *)src + offset);
            y2 = _mm256_loadu_si256((void *)src + offset + YMM_SZ);
            y3 = _mm256_loadu_si256((void *)src + offset + 2 * YMM_SZ);
            y4 = _mm256_loadu_si256((void *)src + offset + 3 * YMM_SZ);

            y5 = _mm256_min_epu8(y1, y2);
            y6 = _mm256_min_epu8(y3, y4);

            y_cmp = _mm256_cmpeq_epi8(_mm256_min_epu8(y5, y6), y0);
            ret = _mm256_movemask_epi8(y_cmp);

            if (ret != 0)
                break;

            _mm256_store_si256((void*)dst + offset, y1);
            _mm256_store_si256((void*)dst + offset + YMM_SZ, y2);
            _mm256_store_si256((void*)dst + offset + 2 * YMM_SZ, y3);
            _mm256_store_si256((void*)dst + offset + 3 * YMM_SZ, y4);

            offset += 4 * YMM_SZ;
        }
    }
   //check for zero in regs: Y1, Y2
    if ((ret1 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y5, y0))))
    {
        if ((ret2 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y1, y0))))
        {
            index = _tzcnt_u32(ret2) + 1 + offset - YMM_SZ;
            y1 = _mm256_loadu_si256((void*)src + index);
            _mm256_storeu_si256((void*)dst + index, y1);
            return dst;
        }
        _mm256_storeu_si256((void*)dst + offset, y1);
        index = _tzcnt_u32(ret1) + 1 + offset;
        y1 = _mm256_loadu_si256((void*)src + index);
        _mm256_storeu_si256((void*)dst + index, y1);
        return dst;
    }
    //check for zero in regs: Y3, Y4
    else
    {
        _mm256_storeu_si256((void*)dst + offset, y1);
        _mm256_storeu_si256((void*)dst + offset + YMM_SZ, y2);
        if ((ret1 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y3, y0))))
        {
            index = _tzcnt_u32(ret1) + 1 + offset + YMM_SZ;
            y1 = _mm256_loadu_si256((void*)src + index);
            _mm256_storeu_si256((void*)dst + index, y1);
            return dst;
        }
        _mm256_storeu_si256((void*)dst + offset + 2 * YMM_SZ, y3);
        index = _tzcnt_u32(ret) + 1 + offset + 2 * YMM_SZ;
        y1 = _mm256_loadu_si256((void*)src + index);
        _mm256_storeu_si256((void*)dst + index, y1);
    }
    return dst;
}
