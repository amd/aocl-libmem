/* Copyright (C) 2022-25 Advanced Micro Devices, Inc. All rights reserved.
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

extern cpu_info zen_info;

static inline void *_memmove_avx512_erms(void *dst, const void *src, size_t size)
{
    __m512i z8;
    register void *ret asm("rax");

    LOG_INFO("\n");

    ret = dst;

    if (likely(size <= 2 * ZMM_SZ)) //128B
    {
        if (likely(size <= ZMM_SZ)) //64B
            return __load_store_ble_zmm_vec(dst, src, size);

        __load_store_le_2zmm_vec(dst, src, size);
        return ret;
    }
    else if (likely(size <= 8 * ZMM_SZ)) //512B
    {
        if (likely(size <= 4 * ZMM_SZ)) //256B
        {
            __load_store_le_4zmm_vec(dst, src, size);
            return ret;
        }
        __load_store_le_8zmm_vec(dst, src, size);
        return ret;
    }
    // check for memory overlap of src and dst buffers
    else if (((dst + size) < src) || ((src + size) < dst))
    {
        size_t offset;
        if (likely(size < 26 * 1024)) //(zen_info.zen_cache_info.l1d_per_core >> 1) + 2 * 1024
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
            uint8_t rem_vecs = ((rem_data) >> 6) + !!(rem_data & (ZMM_SZ - 1));
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
    // handle overlapping memory buffers
    if (src < dst) //Backward Copy
    {
        size_t off = ((size_t)dst + size)& (ZMM_SZ - 1);
        size_t size_temp = size;

        //load the last VEC to handle alignment not multiple of the vec.
        z8 = _mm512_loadu_si512(src + size - ZMM_SZ);

        if (((size_t)dst & (ZMM_SZ-1)) == ((size_t)src & (ZMM_SZ-1)))
        {
            size = __aligned_load_and_store_4zmm_vec_loop_bkwd_pftch(dst, src, size - off, 4 * ZMM_SZ);
        }
        else
        {
            size = __unaligned_load_aligned_store_4zmm_vec_loop_bkwd_pftch(dst, src, size - off, 4 * ZMM_SZ);
        }

        // data vector blocks to be copied
        uint8_t rem_vecs = ((size) >> 6) + !!(size & (ZMM_SZ - 1));

        // load-store blocks based on leftout vectors
        switch (rem_vecs)
        {
            case 4:
            case 3:
                // handle the tail with atmost 4xVEC load-stores
                __load_store_le_4zmm_vec(dst, src, size);
                break;
            case 2:
                // handle the tail with atmost 2xVEC load-stores
                __load_store_le_2zmm_vec(dst, src, size);
                break;
            case 1:
                // handle the tail with atmost 1xVEC load-stores
                __load_store_ble_zmm_vec(dst, src, size);
        }
        //store the last VEC
        _mm512_storeu_si512(dst + size_temp - ZMM_SZ, z8);
    }
    else //Forward Copy
    {
        void * dst_temp = dst;
        size_t offset = ZMM_SZ - ((size_t)dst & (ZMM_SZ - 1));

        //load the first VEC
        z8 = _mm512_loadu_si512(src);

        // Temporal aligned stores
        if (size <= 26 * 1024)
        {
            if (((size_t)dst & (ZMM_SZ-1)) == ((size_t)src & (ZMM_SZ-1)))
                offset = __aligned_load_and_store_4zmm_vec_loop_pftch(dst, src, size - offset, offset);
            else
                offset = __unaligned_load_aligned_store_4zmm_vec_loop_pftch(dst, src, size - offset, offset);
            // remaining data to be copied
            uint16_t rem_data = size - offset;

            // data vector blocks to be copied
            uint8_t rem_vecs = (rem_data >> 6) + !!(rem_data & (ZMM_SZ - 1));
            // load-store blocks based on leftout vectors
            switch (rem_vecs)
            {
                case 4:
                case 3:
                    // handle the tail with atmost 4xVEC load-stores
                    __load_store_le_4zmm_vec(dst + offset, src + offset, rem_data);
                    break;
                case 2:
                    // handle the tail with atmost 2xVEC load-stores
                    __load_store_le_2zmm_vec(dst + offset, src + offset, rem_data);
                    break;
                case 1:
                    // handle the tail with atmost 1xVEC load-stores
                    __load_store_ble_zmm_vec(dst + offset, src + offset, rem_data);
            }
        }
        // rep-movs
        else if (size <= (zen_info.zen_cache_info.l2_per_core))
        {
            __erms_movsb(dst + offset, src + offset, size - offset);
        }
        else //Temporal stores with 8zmm loops
        {
            if (((size_t)dst & (ZMM_SZ-1)) == ((size_t)src & (ZMM_SZ-1)))
                offset = __aligned_load_and_store_8zmm_vec_loop_pftch(dst, src, size - offset, offset);
            else
                offset = __unaligned_load_aligned_store_8zmm_vec_loop_pftch(dst, src, size - offset, offset);

            // remaining data to be copied
            uint16_t rem_data = size - offset;

            // data vector blocks to be copied
            uint8_t rem_vecs = (rem_data >> 6) + !!(rem_data & (ZMM_SZ - 1));
            // load-store blocks based on leftout vectors
            switch (rem_vecs)
            {
                case 8:
                case 7:
                case 6:
                case 5:
                    // handle the tail with upto 4xVEC load-stores
                    __load_store_le_8zmm_vec(dst + offset, src + offset, rem_data);
                    break;
                case 4:
                case 3:
                    // handle the tail with upto 4xVEC load-stores
                    __load_store_le_4zmm_vec(dst + offset, src + offset, rem_data);
                    break;
                case 2:
                    // handle the tail with upto 2xVEC load-stores
                    __load_store_le_2zmm_vec(dst + offset, src + offset, rem_data);
                    break;
                case 1:
                    // handle the tail with upto 1xVEC load-stores
                    __load_store_ble_zmm_vec(dst + offset, src + offset, rem_data);
                    //__load_store_zmm_vec(dst, src, size - ZMM_SZ);
            }
        }
        //store the first VEC
         _mm512_storeu_si512(dst_temp, z8);
    }
    return ret;
}
