/* Copyright (C) 2022-23 Advanced Micro Devices, Inc. All rights reserved.
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
    size_t offset = 0, dst_align = 0;

    if (size <= 4 * YMM_SZ) //128B
    {
        __load_store_le_4ymm_vec(dst, src, size);
        return dst;
    }
    __load_store_le_8ymm_vec(dst, src, size);
    if (size <= 8 * YMM_SZ) //256B
    {
        return dst;
    }

    offset = 4 * YMM_SZ;
    size -= 4 * YMM_SZ;
    dst_align = ((size_t)dst & (YMM_SZ - 1));

    //Aligned Load and Store addresses
    if ((((size_t)src & (YMM_SZ - 1)) | dst_align) == 0)
    {
        if (size < __nt_start_threshold)
           __aligned_load_and_store_4ymm_vec_loop(dst, src, size, offset);
	    else
           __aligned_load_nt_store_4ymm_vec_loop_pftch(dst, src, size, offset);
    }
    else
    {
        offset -= dst_align;
        if (size < __nt_start_threshold)
           __unaligned_load_and_store_4ymm_vec_loop(dst, src, size, offset);
	    else
           __unaligned_load_nt_store_4ymm_vec_loop(dst, src, size, offset);
    }
    return dst;
}

#ifdef AVX512_FEATURE_ENABLED
static inline void *_memcpy_avx512(void *dst, const void *src, size_t size)
{
    size_t offset = 0, dst_align = 0;

    if (size <= 2 * ZMM_SZ) //128B
    {
        __load_store_le_2zmm_vec(dst, src, size);
        return dst;
    }

    if (size <= 4 * ZMM_SZ) //256B
    {
        __load_store_le_4zmm_vec(dst, src, size);
        return dst;
    }

    __load_store_le_8zmm_vec(dst, src, size);

    if (size <= 8 * ZMM_SZ) //512B
        return dst;

    offset += 4 * ZMM_SZ;
    size -= 4 * ZMM_SZ;
    dst_align = ((size_t)dst & (ZMM_SZ - 1));

    //Aligned Load and Store addresses
    if ((((size_t)src & (ZMM_SZ - 1)) | dst_align) == 0)
    {
        // 4-ZMM registers
        if (size < zen_info.zen_cache_info.l2_per_core)//L2 Cache Size
        {
            __aligned_load_and_store_4zmm_vec_loop(dst, src, size, offset);
        }
        // 4-YMM registers with prefetch
        else if (size < __nt_start_threshold)
        {
            __aligned_load_and_store_4ymm_vec_loop_pftch(dst, src, size, offset);
        }
        // Non-temporal 8-ZMM registers with prefetch
        else
        {
            __aligned_load_nt_store_8zmm_vec_loop_pftch(dst, src, size, offset);
        }
    }
    //Unalgined Load/Store addresses: force-align store address to ZMM size
    else
    {
        offset -= dst_align;
        if (size < __nt_start_threshold)
        {
            __unaligned_load_aligned_store_8ymm_vec_loop(dst, src, size, offset);
        }
        else
        {
            __unaligned_load_nt_store_4zmm_vec_loop_pftch(dst, src, size, offset);
        }
    }
    return dst;
}
#endif

void * __attribute__((flatten)) amd_memcpy(void * __restrict dst,
                        const void * __restrict src, size_t size)
{
    LOG_INFO("\n");

#ifdef AVX512_FEATURE_ENABLED
    if (size <= ZMM_SZ)
    {
        return __load_store_ble_zmm_vec(dst, src, size);
    }
    return _memcpy_avx512(dst, src, size);
#else
    if (size <= 2 * YMM_SZ)
    {
        return __load_store_le_2ymm_vec(dst, src, size);
    }
    return _memcpy_avx2(dst, src, size);
#endif
}

void *memcpy(void *, const void *, size_t) __attribute__((weak,
                        alias("amd_memcpy"), visibility("default")));
