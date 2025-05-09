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

#include "logger.h"
#include "threshold.h"
#include "../../base_impls/load_store_impls.h"
#include "zen_cpu_info.h"
#include "alm_defs.h"

extern cpu_info zen_info;

void * __attribute__((flatten)) __mempcpy_zen4(void * __restrict dst, \
                        const void * __restrict src, size_t size)
{
    LOG_INFO("\n");

    size_t offset, dst_align;

    if (size <= ZMM_SZ)
    {
        return __load_store_ble_zmm_vec(dst, src, size) + size;
    }

    if (size <= 2 * ZMM_SZ) //128B
    {
        __load_store_le_2zmm_vec(dst, src, size);
        return dst + size;
    }

    if (size <= 4 * ZMM_SZ) //256B
    {
        __load_store_le_4zmm_vec(dst, src, size);
        return dst + size;
    }

    __load_store_le_8zmm_vec(dst, src, size);

    if (size <= 8 * ZMM_SZ) //512B
        return dst + size;

    dst_align = ((size_t)dst & (ZMM_SZ - 1));

    offset = 4 * ZMM_SZ - dst_align;

    //Aligned SRC & DST addresses
    if ((((size_t)src & (ZMM_SZ - 1)) | dst_align) == 0)
    {
        // 4-ZMM registers
        if (size < zen_info.zen_cache_info.l2_per_core)//L2 Cache Size
        {
            __aligned_load_and_store_4zmm_vec_loop(dst, src, size - 4 * ZMM_SZ, offset);
        }
        // 4-YMM registers with SW - prefetch
        else if (size < zen_info.zen_cache_info.l3_per_ccx)//L3 Cache Size
        {
            __aligned_load_and_store_4ymm_vec_loop_pftch(dst, src, size - 4 * ZMM_SZ, offset);
        }
        // Non-temporal 8-ZMM registers with SW - prefetch
        else
        {
            __aligned_load_nt_store_8zmm_vec_loop_pftch(dst, src, size - 4 * ZMM_SZ, offset);
        }
    }
    // Unalgined SRC/DST addresses: force-align store
    else
    {
        if (size < zen_info.zen_cache_info.l2_per_core)//L2 Cache Size
        {
            __unaligned_load_aligned_store_8ymm_vec_loop(dst, src, size - 4 * ZMM_SZ, offset);
        }
        else
        {
            __unaligned_load_nt_store_4zmm_vec_loop_pftch(dst, src, size - 4 * ZMM_SZ, offset);
        }
    }
    return dst + size;
}

#ifndef ALMEM_DYN_DISPATCH
void *mempcpy(void *, const void *, size_t) __attribute__((weak,
                        alias("__mempcpy_zen4"), visibility("default")));
#endif
