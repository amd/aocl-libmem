/* Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
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
#include "../../../base_impls/load_store_impls.h"
#include "../../../base_impls/load_store_erms_impls.h"
#include "zen_cpu_info.h"
#include "alm_defs.h"

extern cpu_info zen_info;

static inline void * __attribute__((flatten))_memcpy_avx512_erms(void *dst, const void *src, size_t size)
{
    size_t offset;
    register void *ret asm("rax");

    LOG_INFO("\n");

    ret = dst;

    if (likely(size <= 2 * ZMM_SZ)) //128B
    {
        if (likely(size <= ZMM_SZ))
        {
            if (likely(size < YMM_SZ))
                return __load_store_le_2ymm_vec(dst, src, (uint8_t)size);
            // masked load-store
            return __load_store_ble_zmm_vec(dst, src, (uint8_t)size);
        }
        // head-tail load-stores
        __load_store_le_2zmm_vec(dst, src, (uint8_t)size);
        return ret;
    }
    // head-tail load-stores
    else if (likely(size <= 8 * ZMM_SZ)) //512B
    {
    if (size <= 4 * ZMM_SZ) //256B
        {
            __load_store_le_4zmm_vec(dst, src, (uint16_t)size);
            return ret;
        }
        __load_store_le_8zmm_vec(dst, src, (uint16_t)size);
        return ret;
    }
    // aligned vector stores
    else if (likely(size < 26 * 1024)) //(zen_info.zen_cache_info.l1d_per_core >> 1) + 2 * 1024
    {
        // handle the first 4xVEC irrespective of alignment
        __load_store_4zmm_vec(dst, src, 0);

        // align the store address
        offset = 4 * ZMM_SZ - ((size_t)dst & (ZMM_SZ - 1));

        // loop over 4xVEC temporal aligned stores with prefetch
        offset = __unaligned_load_aligned_store_4zmm_vec_loop_pftch(dst, src, size - 4 * ZMM_SZ, offset);

        // remaining data to be copied
        uint16_t rem_data = size - offset;

        // data vector blocks to be copied
        uint8_t rem_vecs = ((rem_data) >> 6) + !!(rem_data & (0x3F));
        // load-store blocks based on leftout vectors
        switch (rem_vecs)
        {
            case 4:
                // handle the tail with last 4xVEC load-stores
                __load_store_4zmm_vec(dst, src, size - 4 * ZMM_SZ);
                break;
            case 3:
                // handle the tail with last 3xVEC load-stores
                __load_store_3zmm_vec(dst, src, size - 3 * ZMM_SZ);
                break;
            case 2:
                // handle the tail with last 2xVEC load-stores
                __load_store_2zmm_vec(dst, src, size - 2 * ZMM_SZ);
                break;
            case 1:
                // handle the tail with last 1xVEC load-stores
                __load_store_zmm_vec(dst, src, size - ZMM_SZ);
        }
    }
    // rep-movs
    else if (size <= (zen_info.zen_cache_info.l2_per_core))
    {
        __erms_movsb(dst, src, size);
    }
    // aligned vector stores
    else if (size < __nt_start_threshold)
    {
        // handle the first 8xVEC irrespective of alignment
        __load_store_8zmm_vec(dst, src, 0);

        // align the store address
        offset =  8 * ZMM_SZ -  ((size_t)dst & (ZMM_SZ - 1));

        // loop over 4xVEC temporal aligned stores with prefetch
        __unaligned_load_aligned_store_4zmm_vec_loop_pftch(dst, src, size - 4 * ZMM_SZ, offset);

        // handle the tail with last 4-vec load-stores
        __load_store_4zmm_vec(dst, src, size - 4 * ZMM_SZ);
    }
    else
    {
        // handle the first 8xVEC irrespective of alignment
        __load_store_8zmm_vec(dst, src, 0);

        // align the store address
        offset =  8 * ZMM_SZ -  ((size_t)dst & (ZMM_SZ - 1));

        // loop over 8xVEC non-temporal stores with prefetch
        __unaligned_load_nt_store_8zmm_vec_loop_pftch(dst, src, size - 8 * ZMM_SZ, offset);

        // handle the tail with last 8-vec load-stores
        __load_store_le_8zmm_vec(dst + size - 8 * ZMM_SZ, src + size - 8 * ZMM_SZ, 8 * ZMM_SZ);
    }
    return ret;
}
