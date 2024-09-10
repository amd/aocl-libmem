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
#include "logger.h"
#include "threshold.h"
#include "../base_impls/load_store_impls.h"
#include "zen_cpu_info.h"

extern cpu_info zen_info;

static inline void *_memcpy_avx2(void *dst, const void *src, size_t size)
{
    register void *ret asm("rax");
    ret = dst;

    if (likely(size <= 2 * YMM_SZ))
        return __load_store_le_2ymm_vec(dst, src, (uint8_t)size);

    if (size <= 4 * YMM_SZ) //128B
    {
        __load_store_le_4ymm_vec(dst, src, size);
        return ret;
    }
    if (size <= 8 * YMM_SZ) //256B
    {
        __load_store_le_8ymm_vec(dst, src, size);
        return ret;
    }
    if (size <= zen_info.zen_cache_info.l1d_per_core)
    {
        __unaligned_load_and_store_4ymm_vec_loop(dst, src, size & ~(4 * YMM_SZ - 1), 0);
        size_t offset = size & (4 * YMM_SZ - 1);
        if (offset)
        {
             __m256i y0, y1, y2, y3;
            switch ((offset >> 5))
            {
                case 3:
                    y3 = _mm256_loadu_si256(src + size - 4 * YMM_SZ);
                    _mm256_storeu_si256(dst + size - 4 * YMM_SZ, y3);
                case 2:
                    y2 = _mm256_loadu_si256(src + size - 3 * YMM_SZ);
                    _mm256_storeu_si256(dst + size - 3 * YMM_SZ, y2);
                case 1:
                    y1 = _mm256_loadu_si256(src + size - 2 * YMM_SZ);
                    _mm256_storeu_si256(dst + size - 2 * YMM_SZ, y1);
                default:
                    y0 = _mm256_loadu_si256(src + size - 1 * YMM_SZ);
                    _mm256_storeu_si256(dst + size - 1 * YMM_SZ, y0);
            }
        }
        return ret;
    }
    // Load-Store first 4 VECs
    __load_store_le_4ymm_vec(dst, src, 4 * YMM_SZ);

    // Compute alignment of destination address
    uint8_t dst_align = ((size_t)dst & (YMM_SZ - 1));

    // Matching alignments of load & store addresses;
    if (unlikely((((size_t)src & (YMM_SZ - 1)) == dst_align)))
    {
        // Adjust alignment to 4 VEC alignment
        dst_align = 4 * YMM_SZ - dst_align;
        if (size < __nt_start_threshold)
        {
           __aligned_load_and_store_4ymm_vec_loop(dst, src, size - 4 * YMM_SZ, dst_align);
        }
        else
        {
           __aligned_load_nt_store_4ymm_vec_loop(dst, src, size - 4 * YMM_SZ, dst_align);
        }
    }
    // Mismatching alignments of load & store addresses;
    else
    {
        dst_align = 4 * YMM_SZ - dst_align;
        if (size < __nt_start_threshold)
        {
           __unaligned_load_aligned_store_4ymm_vec_loop_pftch(dst, src, size - 4 * YMM_SZ, dst_align);
        }
        else
        {
           __unaligned_load_nt_store_4ymm_vec_loop(dst, src, size - 4 * YMM_SZ, dst_align);
        }
    }

    // Load-Store Last 4 VECs
    __load_store_le_4ymm_vec(dst + size - 4 * YMM_SZ, src + size - 4 * YMM_SZ, 4 * YMM_SZ);

    return ret;
}

#ifdef AVX512_FEATURE_ENABLED
static inline void *_memcpy_avx512(void *dst, const void *src, size_t size)
{
    register void *ret asm("rax");
    ret = dst;

    if (likely(size <= 2 * ZMM_SZ)) //128B
    {
        if ((size < ZMM_SZ))
        {
            return __load_store_ble_zmm_vec(dst, src, (uint8_t)size);
        }
        __load_store_le_2zmm_vec(dst, src, (uint8_t)size);
        return ret;
    }

    if (size <= 8 * ZMM_SZ) //512B
    {
        __load_store_le_8ymm_vec(dst, src, (uint16_t)size);
        if (size <= 4 * ZMM_SZ) //256B
        {
            return ret;
        }
        __load_store_le_8ymm_vec(dst + 4 * YMM_SZ, src + 4 * YMM_SZ, (uint16_t)size - 8 * YMM_SZ);
        return ret;
    }
    __load_store_le_8zmm_vec(dst, src, 8 * ZMM_SZ);

    size_t offset = 8 * ZMM_SZ;

    if (size > 16 * ZMM_SZ)
    {
        uint8_t dst_align = ((size_t)dst & (ZMM_SZ - 1));
        offset -= dst_align;

        //Aligned Load and Store addresses
        if ((((size_t)src & (ZMM_SZ - 1)) == dst_align))
        {
            // 4-ZMM registers
            if (size < zen_info.zen_cache_info.l2_per_core)//L2 Cache Size
            {
                offset = __aligned_load_and_store_4zmm_vec_loop(dst, src, size - 8 * ZMM_SZ, offset);
            }
            // 4-YMM registers with prefetch
            else if (size < __nt_start_threshold)
            {
                offset = __aligned_load_and_store_4ymm_vec_loop_pftch(dst, src, size - 8 * ZMM_SZ, offset);
            }
            // Non-temporal 8-ZMM registers with prefetch
            else
            {
                offset = __aligned_load_nt_store_8zmm_vec_loop_pftch(dst, src, size - 8 * ZMM_SZ, offset);
            }
        }
        //Unalgined Load/Store addresses: force-align store address to ZMM size
        else
        {
            if (size < __nt_start_threshold)
            {
                offset = __unaligned_load_aligned_store_8ymm_vec_loop(dst, src, size - 8 * ZMM_SZ, offset);
            }
            else
            {
                offset = __unaligned_load_nt_store_4zmm_vec_loop_pftch(dst, src, size - 8 * ZMM_SZ, offset);
            }
        }
    }
    uint16_t rem_data = size - offset;
    uint8_t rem_vecs = ((rem_data & 0x3C0) >> 6) + !!(rem_data & (0x3F));
    if (rem_vecs > 4)
            __load_store_le_8zmm_vec(dst + size - 8 * ZMM_SZ, src + size - 8 * ZMM_SZ, 8 * ZMM_SZ);
    else if (rem_vecs > 2)
            __load_store_le_4zmm_vec(dst + size - 4 * ZMM_SZ, src + size - 4 * ZMM_SZ, 4 * ZMM_SZ);
    else if (rem_vecs == 2)
            __load_store_le_2zmm_vec(dst + size - 2 * ZMM_SZ, src + size - 2 * ZMM_SZ, 2 * ZMM_SZ);
    else
            __load_store_zmm_vec(dst + size - ZMM_SZ, src + size -  ZMM_SZ, 0);

    return ret;
}
#endif


void *__memcpy_zen1(void *dst, const void *src, size_t size)
{
    LOG_INFO("\n");
    return _memcpy_avx2(dst, src, size);
}

void * __attribute__((flatten)) __memcpy_zen2(void * __restrict dst, \
                        const void * __restrict src, size_t size)
{
    LOG_INFO("\n");
    return _memcpy_avx2(dst, src, size);
}

void * __attribute__((flatten)) __memcpy_zen3(void * __restrict dst, \
                        const void * __restrict src, size_t size)
{
    LOG_INFO("\n");
    return _memcpy_avx2(dst, src, size);
}

void * __attribute__((flatten)) __memcpy_zen4(void * __restrict dst, \
                        const void * __restrict src, size_t size)
{
    LOG_INFO("\n");
#ifdef AVX512_FEATURE_ENABLED
    return _memcpy_avx512(dst, src, size);
#else
    return _memcpy_avx2(dst, src, size);
#endif
}

void * __memcpy_zen5 (void * __restrict dst, const void * __restrict src,
                     size_t size) __attribute__((alias("__memcpy_zen4")));
