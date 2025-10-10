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

#ifndef MEMCPY_IMPL_AVX512_H
#define MEMCPY_IMPL_AVX512_H

#include "threshold.h"
#include "../../../base_impls/load_store_impls.h"
#include "../../../base_impls/load_store_erms_impls.h"
#include "zen_cpu_info.h"
#include "almem_defs.h"

extern cpu_info zen_info;

static inline void *_memcpy_avx512(void *__restrict dst,
                                       const void *__restrict src, size_t size)
{
    register void *ret asm("rax");
    ret = dst;

    if (likely(size <= 2 * ZMM_SZ)) //128B
    {
        if (likely(size < ZMM_SZ))
        {
            return __load_store_ble_zmm_vec(dst, src, (uint8_t)size);
        }
        __load_store_le_2zmm_vec(dst, src, (uint8_t)size);
        return ret;
    }

#ifdef MEMMOVE_AVX512
    if (likely(size <= 8 * ZMM_SZ)) //512B
    {
        if (likely(size <= 4 * ZMM_SZ)) //256B
        {
            __load_store_le_4zmm_vec(dst, src, (uint16_t)size);
            return ret;
        }
        __load_store_le_8zmm_vec(dst, src, (uint16_t)size);
        return ret;
    }
#else
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
#endif

#ifdef MEMMOVE_AVX512
    // Handle overlapping memory blocks
    if (unlikely(!(((dst + size) < src) || ((src + size) < dst))))
    {
        __m512i z4, z5, z6, z7, z8;
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
        return ret;
    }
#endif

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

static inline void *_memcpy_avx512_erms(void *__restrict dst,
                                       const void *__restrict src, size_t size)
{
    size_t offset;
    register void *ret asm("rax");
    ret = dst;

    if (likely(size <= 2 * ZMM_SZ)) //128B
    {
	    if (likely(size <= ZMM_SZ))
            return __load_store_ble_zmm_vec(dst, src, (uint8_t)size);

        __load_store_le_2zmm_vec(dst, src, (uint8_t)size);
        return ret;
    }
    // head-tail load-stores
    else if (likely(size <= 8 * ZMM_SZ)) //512B
    {
        if (likely(size <= 4 * ZMM_SZ)) //256B
        {
	        __load_store_le_4zmm_vec(dst, src, (uint16_t)size);
            return ret;
        }
        __load_store_le_8zmm_vec(dst, src, (uint16_t)size);
        return ret;
    }

#ifdef MEMMOVE_AVX512_ERMS
    // Handle overlapping memory blocks
    if (unlikely(!(((dst + size) < src) || ((src + size) < dst))))
    {
        __m512i z8;
        if (src < dst) //Backward Copy
        {
            size_t off = ((size_t)dst + size)& (ZMM_SZ - 1);
            size_t size_temp = size;

            //load the last VEC to handle alignment not multiple of the vec.
            z8 = _mm512_loadu_si512(src + size - ZMM_SZ);

            if (((size_t)dst & (ZMM_SZ - 1)) == ((size_t)src & (ZMM_SZ - 1)))
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
                    offset = __aligned_load_and_store_4zmm_vec_loop_pftch(dst, src, size - 4 * ZMM_SZ, offset);
                else
                    offset = __unaligned_load_aligned_store_4zmm_vec_loop_pftch(dst, src, size - 4 * ZMM_SZ, offset);
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
            else //Temporal stores with 8zmm loops
            {
                if (((size_t)dst & (ZMM_SZ - 1)) == ((size_t)src & (ZMM_SZ - 1)))
                {
                    offset = __aligned_load_and_store_8zmm_vec_loop_pftch(dst, src, size - 8 * ZMM_SZ, offset);
                }
                else
                    offset = __unaligned_load_aligned_store_8zmm_vec_loop_pftch(dst, src, size - 8 * ZMM_SZ, offset);

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
                }
            }
            //store the first VEC
            _mm512_storeu_si512(dst_temp, z8);
        }
        return ret;
    }
#endif

    // aligned vector stores
    if (likely(size < (zen_info.zen_cache_info.l1d_per_core >> 1) + 2 * 1024))
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
	    return ret;
    }
    // rep-movs
    else if (size < (zen_info.zen_cache_info.l3_per_ccx))
    {
        __erms_movsb(dst, src, size);
	    return ret;
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

#ifdef MEMMOVE_AVX512
#undef MEMMOVE_AVX512
#endif

#ifdef MEMMOVE_AVX512_ERMS
#undef MEMMOVE_AVX512_ERMS
#endif

#endif // MEMCPY_IMPL_AVX512_H

