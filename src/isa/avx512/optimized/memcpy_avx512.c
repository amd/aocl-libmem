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
#include "../../../base_impls/load_store_impls.h"
#include "zen_cpu_info.h"
#include "alm_defs.h"

extern cpu_info zen_info;

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
        __load_store_le_4zmm_vec(dst, src, (uint16_t)size);
        if (size <= 4 * ZMM_SZ) //256B
        {
            return ret;
        }
        __load_store_le_4zmm_vec(dst + 2 * ZMM_SZ, src + 2 * ZMM_SZ, (uint16_t)size - 4 * ZMM_SZ);
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
                offset = __aligned_load_and_store_4zmm_vec_loop_pftch(dst, src, size - 8 * ZMM_SZ, offset);
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
                offset = __unaligned_load_aligned_store_4zmm_vec_loop(dst, src, size - 8 * ZMM_SZ, offset);
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
