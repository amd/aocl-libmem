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

/* This function is an optimized version of strcpy using AVX2 instructions.
It copies a null-terminated string from `src` to `dst`, returning the original value of `dst` */
static inline char * __attribute__((flatten)) _strcpy_avx2(char *dst, const char *src)
{
    size_t offset , index;
    uint32_t ret = 0, ret1 = 0, ret2 = 0;
    __m256i y0, y1, y2, y3, y4, y5, y6, y_cmp;

    // Store the original destination pointer to return at the end
    register void *ret_dst asm("rax");
    ret_dst = dst;

    // Initialize a zeroed AVX2 register for comparisons
    y0 = _mm256_setzero_si256();

    // Calculate the offset to align the source pointer to 32 bytes
    offset = (uintptr_t)src & (YMM_SZ - 1);

    // Handle cases where `src` is close to the end of a memory page
    if (unlikely((PAGE_SZ - YMM_SZ) < ((PAGE_SZ - 1) & (uintptr_t)src)))
    {
        y1 = _mm256_load_si256((void *)src - offset);
        y_cmp = _mm256_cmpeq_epi8(y0, y1);

        ret = (uint32_t)_mm256_movemask_epi8(y_cmp) >> offset;

        // If a null terminator is found, calculate the index and store the result
        if (ret)
        {
            index = _tzcnt_u32(ret) + 1;
            strcpy_ble_ymm(dst, src, index);
            return ret_dst;
        }
    }
    y1 = _mm256_loadu_si256((void *)src);
    y_cmp = _mm256_cmpeq_epi8(y0, y1);

    ret = _mm256_movemask_epi8(y_cmp);
    if (ret)
    {
        index =  _tzcnt_u32(ret) + 1;
        strcpy_ble_ymm(dst, src, index);
        return ret_dst;
    }
    // Store the first 32 bytes to `dst`
    _mm256_storeu_si256((void *)dst, y1);

    // Adjust the offset for the next load operation
    offset = YMM_SZ - offset;

    // Initialize a counter to process the next 192 bytes (6 * 32B) from the source
    uint8_t cnt_vec = 5;
    do
    {
        y1 = _mm256_load_si256((void *)src + offset);
        y_cmp = _mm256_cmpeq_epi8(y0, y1);
        ret = _mm256_movemask_epi8(y_cmp);

        // If a null terminator is found within the loaded bytes, copy the tail vector.
        // Load and copy the remaining bytes up to and including the null terminator
        if (ret)
        {
            index = offset + _tzcnt_u32(ret) - YMM_SZ + 1;
            y2 = _mm256_loadu_si256((void *)src + index);
            _mm256_storeu_si256((void*)dst + index, y2);
            return ret_dst;
        }

        _mm256_storeu_si256((void*)dst + offset, y1);
        offset += YMM_SZ;
    } while (cnt_vec--);

    // Determine if the next four 32-byte blocks (128 bytes total) are within the same memory
    // page , this is to avoid crossing a page boundary, which could lead to a page fault if
    // the memory is not mapped. Here it calculates and processes the maximum number of 32-byte
    // vectors that can be handled without the risk of crossing into an adjacent memory page.
    cnt_vec = 4 - (((uintptr_t)src + offset) & (4 * YMM_SZ - 1) >> 5);
    while (cnt_vec--)
    {
        y2 = _mm256_load_si256((void *)src + offset);
        y_cmp = _mm256_cmpeq_epi8(y0, y2);
        ret = _mm256_movemask_epi8(y_cmp);

        if (ret)
        {
            index = offset + _tzcnt_u32(ret) - YMM_SZ + 1;
            y3 = _mm256_loadu_si256((void *)src + index);
            _mm256_storeu_si256((void*)dst + index, y3);
            return ret_dst;
        }

        _mm256_storeu_si256((void*)dst + offset, y2);
        offset += YMM_SZ;
    }

    // Main loop to process 4 vectors at a time for larger sizes(> 256B)
    while(1)
    {
        y1 = _mm256_load_si256((void *)src + offset);
        y2 = _mm256_load_si256((void *)src + offset + YMM_SZ);
        y3 = _mm256_load_si256((void *)src + offset + 2 * YMM_SZ);
        y4 = _mm256_load_si256((void *)src + offset + 3 * YMM_SZ);

        // Find the minimum of the loaded vectors to check for null terminator
        y5 = _mm256_min_epu8(y1, y2);
        y6 = _mm256_min_epu8(y3, y4);

        // Compare the minimums to find the null terminator
        y_cmp = _mm256_cmpeq_epi8(_mm256_min_epu8(y5, y6), y0);
        ret = _mm256_movemask_epi8(y_cmp);

        if (ret != 0)
            break;// If found, exit the loop

        // Store the 4 vectors to `dst`
        _mm256_storeu_si256((void*)dst + offset, y1);
        _mm256_storeu_si256((void*)dst + offset + YMM_SZ, y2);
        _mm256_storeu_si256((void*)dst + offset + 2 * YMM_SZ, y3);
        _mm256_storeu_si256((void*)dst + offset + 3 * YMM_SZ, y4);

        offset += 4 * YMM_SZ;
    }

   //Check for null terminator in the first two vectors (y1, y2)
    if ((ret1 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y5, y0))))
    {
        if ((ret2 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y1, y0))))
        {
            ret = ret2;
            goto copy_tail_vec;

        }
        _mm256_storeu_si256((void*)dst + offset, y1);
        offset += YMM_SZ;
        ret = ret1;
    }
    // Check for null terminator in the last two vectors (y3, y4)
    else
    {
        _mm256_storeu_si256((void*)dst + offset, y1);
        _mm256_storeu_si256((void*)dst + offset + YMM_SZ, y2);
        if ((ret1 = _mm256_movemask_epi8(_mm256_cmpeq_epi8(y3, y0))))
        {
            ret = ret1;
            offset += 2 * YMM_SZ;
            goto copy_tail_vec;
        }
        _mm256_storeu_si256((void*)dst + offset + 2 * YMM_SZ, y3);
        offset += 3 * YMM_SZ;
    }

    // Label for copying the tail of the string where the null terminator was found
    copy_tail_vec:
    {
        index = offset + _tzcnt_u32(ret) - YMM_SZ + 1;
        y1 = _mm256_loadu_si256((void*)src + index);
        _mm256_storeu_si256((void*)dst + index, y1);
        return ret_dst;

    }

}
