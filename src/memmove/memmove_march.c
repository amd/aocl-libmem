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


static inline void *_memmove_avx2(void *dst, const void *src, size_t size)
{
    __m256i y4, y5, y6, y7;
    register void *ret asm("rax");
    ret = dst;

    if (likely(size <= 2 * YMM_SZ))
        return __load_store_le_2ymm_vec_overlap(dst, src, size);

    if (size <= 4 * YMM_SZ)
    {
        __load_store_le_4ymm_vec(dst, src, size);
        return ret;
    }
    if (size <= 8 * YMM_SZ)
    {
        __load_store_le_8ymm_vec(dst, src, size);
        return ret;
    }
    if (((dst + size) < src) || ((src + size) < dst))
    {
        size_t offset = 0;
        uint32_t dst_align = ((size_t)dst & (YMM_SZ - 1));

        __load_store_le_8ymm_vec(dst, src, size);

        offset = 4 * YMM_SZ;

        //Aligned Load and Store addresses
        if (!((uintptr_t)src ^ dst_align))
        {
            if (size < __nt_start_threshold)
               __aligned_load_and_store_4ymm_vec_loop(dst, src, size - offset, offset - dst_align);
            else
               __aligned_load_nt_store_4ymm_vec_loop_pftch(dst, src, size - offset - dst_align, offset - dst_align);
        }
        else
        {
            offset -= dst_align;

            if (size < __nt_start_threshold)
               __unaligned_load_and_store_4ymm_vec_loop(dst, src, size - 4 * YMM_SZ, offset);
            else
               __unaligned_load_nt_store_4ymm_vec_loop(dst, src, size - 4 * YMM_SZ, offset);
        }
        return ret;
    }

    // Handle overlapping memory blocks
    if ((dst < (src + size)) && (src < dst)) //Backward Copy
    {
        y7 = _mm256_loadu_si256(src + 3 * YMM_SZ);
        y6 = _mm256_loadu_si256(src + 2 * YMM_SZ);
        y5 = _mm256_loadu_si256(src + 1 * YMM_SZ);
        y4 = _mm256_loadu_si256(src + 0 * YMM_SZ);
        __unaligned_load_and_store_4ymm_vec_loop_bkwd(dst, src, size, 4 * YMM_SZ);
        //copy first 4VECs to avoid override
        _mm256_storeu_si256(dst +  3 * YMM_SZ, y7);
        _mm256_storeu_si256(dst +  2 * YMM_SZ, y6);
        _mm256_storeu_si256(dst +  1 * YMM_SZ, y5);
        _mm256_storeu_si256(dst +  0 * YMM_SZ, y4);
    }
    else //Forward Copy
    {
        y4 = _mm256_loadu_si256(src + size - 4 * YMM_SZ);
        y5 = _mm256_loadu_si256(src + size - 3 * YMM_SZ);
        y6 = _mm256_loadu_si256(src + size - 2 * YMM_SZ);
        y7 = _mm256_loadu_si256(src + size - 1 * YMM_SZ);
        __unaligned_load_and_store_4ymm_vec_loop(dst, src, size - 4 * YMM_SZ, 0);
        //copy last 4VECs to avoid override
        _mm256_storeu_si256(dst + size - 4 * YMM_SZ, y4);
        _mm256_storeu_si256(dst + size - 3 * YMM_SZ, y5);
        _mm256_storeu_si256(dst + size - 2 * YMM_SZ, y6);
        _mm256_storeu_si256(dst + size - 1 * YMM_SZ, y7);
    }
    return ret;
}

#ifdef AVX512_FEATURE_ENABLED
static inline void * _memmove_avx512(void *dst, const void *src, size_t size)
{
    __m512i z4, z5, z6, z7, z8;
    size_t offset = 0;

    if (size <= 2 * ZMM_SZ)
    {
        __load_store_le_2zmm_vec(dst, src, size);
        return dst;
    }
    if (size <= 4 * ZMM_SZ)
    {
        __load_store_le_4zmm_vec(dst, src, size);
        return dst;
    }
    if (size <= 8 * ZMM_SZ)
    {
        __load_store_le_8zmm_vec(dst, src, size);
        return dst;
    }
    if (((dst + size) < src) || ((src + size) < dst))
    {
        uint32_t dst_align = ((uintptr_t)dst & (ZMM_SZ - 1));

        __load_store_le_8zmm_vec(dst, src, size);
        offset = 4 * ZMM_SZ;

        //Aligned Load and Store addresses
        if (!((uintptr_t)src ^ dst_align))
        {
            // 4-ZMM registers
            if (size < zen_info.zen_cache_info.l2_per_core)//L2 Cache Size
            {
                __aligned_load_and_store_8ymm_vec_loop(dst, src, size - offset, offset);
            }
            // 4-YMM registers with prefetch
            else if (size < __nt_start_threshold)
            {
                __aligned_load_and_store_4ymm_vec_loop_pftch(dst, src, size - offset, offset);
            }
            // Non-temporal 4-ZMM registers with prefetch
            else
            {
                __aligned_load_nt_store_4zmm_vec_loop_pftch(dst, src, size - offset, offset);
            }
        }
        //Unalgined Load/Store addresses: force-align store address to ZMM size
        else
        {
            offset -= dst_align;

            if (size < __nt_start_threshold)
            {
                __unaligned_load_aligned_store_4zmm_vec_loop(dst, src, size - 4 * ZMM_SZ, offset);
            }
            else
            {
                __unaligned_load_nt_store_4zmm_vec_loop(dst, src, size - 4 * ZMM_SZ, offset);
            }
        }
        return dst;
    }
    // Handle overlapping memory blocks
    if (src < dst) //Backward Copy
    {
         z4 = _mm512_loadu_si512(src + 3 * ZMM_SZ);
         z5 = _mm512_loadu_si512(src + 2 * ZMM_SZ);
         z6 = _mm512_loadu_si512(src + 1 * ZMM_SZ);
         z7 = _mm512_loadu_si512(src + 0 * ZMM_SZ);
        if ((((size_t)dst & (ZMM_SZ-1)) == 0) && (((size_t)src & (ZMM_SZ-1)) == 0))
        {
            //load the last VEC to handle size not multiple of the vec.
            z8 = _mm512_loadu_si512(src + size - ZMM_SZ);
            __aligned_load_and_store_4zmm_vec_loop_bkwd(dst, src, size & ~(ZMM_SZ-1), 3 * ZMM_SZ);
            //store the last VEC to handle size not multiple of the vec.
            _mm512_storeu_si512(dst + size - ZMM_SZ, z8);
        }
        else
            __unaligned_load_and_store_4zmm_vec_loop_bkwd(dst, src, size, 4 * ZMM_SZ);
         _mm512_storeu_si512(dst +  3 * ZMM_SZ, z4);
         _mm512_storeu_si512(dst +  2 * ZMM_SZ, z5);
         _mm512_storeu_si512(dst +  1 * ZMM_SZ, z6);
         _mm512_storeu_si512(dst +  0 * ZMM_SZ, z7);
    }
    else //Forward Copy
    {
         z4 = _mm512_loadu_si512(src + size - 4 * ZMM_SZ);
         z5 = _mm512_loadu_si512(src + size - 3 * ZMM_SZ);
         z6 = _mm512_loadu_si512(src + size - 2 * ZMM_SZ);
         z7 = _mm512_loadu_si512(src + size - 1 * ZMM_SZ);
        if ((((size_t)dst & (ZMM_SZ-1)) == 0) && (((size_t)src & (ZMM_SZ-1)) == 0))
            __aligned_load_and_store_4zmm_vec_loop(dst, src, size - 4 * ZMM_SZ, 0);
        else
            __unaligned_load_and_store_4zmm_vec_loop(dst, src, size - 4 * ZMM_SZ, 0);

         _mm512_storeu_si512(dst + size - 4 * ZMM_SZ, z4);
         _mm512_storeu_si512(dst + size - 3 * ZMM_SZ, z5);
         _mm512_storeu_si512(dst + size - 2 * ZMM_SZ, z6);
         _mm512_storeu_si512(dst + size - 1 * ZMM_SZ, z7);
    }
    return dst;
}

#endif


void * __attribute__((flatten)) __memmove_zen1(void * __restrict dst, \
                        const void * __restrict src, size_t size)
{
    LOG_INFO("\n");
    if (size <= 2 * YMM_SZ)
        return __load_store_le_2ymm_vec_overlap(dst, src, size);

    return _memmove_avx2(dst, src, size);
}

void * __attribute__((flatten)) __memmove_zen2(void * __restrict dst, \
                        const void * __restrict src, size_t size)
{
    LOG_INFO("\n");
    if (size <= 2 * YMM_SZ)
        return __load_store_le_2ymm_vec_overlap(dst, src, size);

    return _memmove_avx2(dst, src, size);
}

void * __attribute__((flatten)) __memmove_zen3(void * __restrict dst, \
                        const void * __restrict src, size_t size)
{
    LOG_INFO("\n");
    if (size <= 2 * YMM_SZ)
        return __load_store_le_2ymm_vec_overlap(dst, src, size);

    return _memmove_avx2(dst, src, size);
}

void * __attribute__((flatten)) __memmove_zen4(void * __restrict dst, \
                        const void * __restrict src, size_t size)
{
    LOG_INFO("\n");
#ifdef AVX512_FEATURE_ENABLED
    if (size <= ZMM_SZ)
        return __load_store_ble_zmm_vec(dst, src, size);

    return _memmove_avx512(dst, src, size);
#else
    if (size <= 2 * YMM_SZ)
        return __load_store_le_2ymm_vec_overlap(dst, src, size);

    return _memmove_avx2(dst, src, size);
#endif

}

void * __memmove_zen5 (void * __restrict dst, const void * __restrict src,
                     size_t size) __attribute__((alias("__memmove_zen4")));

