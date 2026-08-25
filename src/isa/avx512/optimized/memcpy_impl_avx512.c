/* Copyright (C) 2024-26 Advanced Micro Devices, Inc. All rights reserved.
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

#include "../../../base_impls/load_store_erms_impls.h"
#include "../../../base_impls/load_store_impls.h"
#include "almem_defs.h"
#include "threshold.h"
#include "zen_cpu_info.h"

extern cpu_info zen_info;

/* Every memmove variant shares the overlap check and the 16-vec tier. */
#if defined(MEMMOVE_AVX512_ERMS) || defined(MEMMOVE_AVX512) || defined(MEMMOVE_AVX512_ZEN6)
#define ALM_AVX512_MEMMOVE
#endif

/*
 * AVX-512 memcpy/memmove size ladder:
 *   <=64B:              masked zmm (memcpy), scalar/SSE/AVX head-tail (memmove),
 *                       or scalar head/mid/tail + branchless 4-xmm (zen6)
 *   65..512B:           2-, 4-, 8-vec head/tail
 *   513..1024B (move):  16-vec head/tail
 *   larger:             overlap check (memmove only), then dst-aligned 4-vec
 *                       temporal loop / ERMS / NT stores, per threshold
 *
 * memcpy is __restrict; memmove adds the overlap check and the 16-vec tier.
 * The ZEN6 variants write-prefetch the temporal loop and run rep-movsb out to
 * __repmov_stop_threshold, which reverses the ERMS/NT check order.
 */

#ifdef ALM_AVX512_MEMMOVE
static inline void *_memcpy_avx512_erms(void *dst, const void *src, size_t size)
#else
static inline void *_memcpy_avx512_erms(void *__restrict dst, const void *__restrict src, size_t size)
#endif
{
    register void *ret __asm__("rax") = dst;

#if defined(MEMMOVE_AVX512_ZEN6)
    if (likely(size <= XMM_SZ))
    {
        if (likely(size != 0))
            __load_store_ble_scalar_head_mid_tail(dst, src, size);
        return ret;
    }
    if (likely(size <= ZMM_SZ))
    {
        __load_store_ble_4sse_head_tail(dst, src, size);
        return ret;
    }
#elif defined(ALM_AVX512_MEMMOVE)
    if (likely(size <= XMM_SZ))
    {
        __load_store_ble_scalar_head_tail(dst, src, size);
        return ret;
    }
    if (likely(size <= YMM_SZ))
    {
        __load_store_ble_sse_head_tail(dst, src, size);
        return ret;
    }
    if (likely(size <= ZMM_SZ))
    {
        __load_store_ble_avx_head_tail(dst, src, size);
        return ret;
    }
#else
    if (likely(size <= 1 * ZMM_SZ))
    {
        __load_store_ble_zmm_vec(dst, src, size);
        return ret;
    }
#endif
    if (likely(size <= 2 * ZMM_SZ))
    {
        __load_store_le_2zmm_vec(dst, src, size);
        return ret;
    }
    if (size <= 4 * ZMM_SZ)
    {
        __load_store_le_4zmm_vec(dst, src, size);
        return ret;
    }
    if (size <= 8 * ZMM_SZ)
    {
        __load_store_le_8zmm_vec(dst, src, size);
        return ret;
    }

#ifdef ALM_AVX512_MEMMOVE
    if (size <= 16 * ZMM_SZ)
    {
        __load_store_le_16zmm_vec(dst, src, size);
        return ret;
    }

    if (unlikely(REGIONS_OVERLAP(dst, src, size)))
    {
        if ((const char *) src < (char *) dst)
        {
            OVERLAP_SAVE_HEAD_4VEC(AVX512, src);
            OVERLAP_SAVE_TAIL_1VEC(AVX512, src, size);

            size_t tail_misalign = ((size_t) dst + size) & (ZMM_SZ - 1);
            if (tail_misalign == 0)
                tail_misalign = ZMM_SZ;
            size_t bk = size - tail_misalign;

            if (unlikely(size >= __nt_start_threshold))
            {
                while (bk > 4 * ZMM_SZ)
                {
                    bk -= 4 * ZMM_SZ;
                    __unaligned_load_nt_store_4zmm_vec((char *) dst + bk, (const char *) src + bk);
                }
                ALM_STORE_FENCE()
            }
            else
            {
                while (bk > 4 * ZMM_SZ)
                {
                    bk -= 4 * ZMM_SZ;
                    __unaligned_load_aligned_store_4zmm_vec((char *) dst + bk, (const char *) src + bk);
                }
            }
            OVERLAP_RESTORE_TAIL1_HEAD4(AVX512, dst, size);
        }
        else
        {
            OVERLAP_SAVE_HEAD_1VEC(AVX512, src);
            OVERLAP_SAVE_TAIL_4VEC(AVX512, src, size);

            const char *sp = (const char *) src;
            char *dp = (char *) dst;
            size_t rem;
            ALIGN_ADVANCE_64(sp, dp, size, rem);

            if (unlikely(size >= __nt_start_threshold))
            {
                while (rem >= 4 * ZMM_SZ)
                {
                    __unaligned_load_nt_store_4zmm_vec(dp, sp);
                    ADVANCE_4VEC(AVX512, sp, dp, rem);
                }
                ALM_STORE_FENCE()
            }
            else
            {
                while (rem >= 4 * ZMM_SZ)
                {
                    __unaligned_load_aligned_store_4zmm_vec(dp, sp);
                    ADVANCE_4VEC(AVX512, sp, dp, rem);
                }
            }
            OVERLAP_RESTORE_TAIL4_HEAD1(AVX512, dst, size);
        }
        return ret;
    }
#endif

#ifdef MEMMOVE_AVX512_ZEN6
    /* rep-movsb copies forward */
    if (unlikely(size >= __repmov_start_threshold && size < __repmov_stop_threshold))
    {
        __erms_movsb((void *) dst, src, size);
        return ret;
    }
#endif

    /* dst-aligned temporal loop */
#ifdef MEMMOVE_AVX512_ZEN6
    if (likely(size < __repmov_stop_threshold))
#else
    if (likely(size < __repmov_start_threshold))
#endif
    {
        const char *sp = (const char *) src;
        char *dp = (char *) dst;

        __unaligned_load_store_1zmm_vec(dp, sp);

        size_t rem;
        ALIGN_ADVANCE_64(sp, dp, size, rem);

        while (rem > 4 * ZMM_SZ)
        {
#if defined(MEMCPY_AVX512_ZEN6) || defined(MEMMOVE_AVX512_ZEN6)
            if (rem > 8 * ZMM_SZ)
            {
                PREFETCH_WRITE(dp + 4 * ZMM_SZ);
                PREFETCH_WRITE(dp + 5 * ZMM_SZ);
                PREFETCH_WRITE(dp + 6 * ZMM_SZ);
                PREFETCH_WRITE(dp + 7 * ZMM_SZ);
            }
#endif
            __unaligned_load_aligned_store_4zmm_vec(dp, sp);
            ADVANCE_4VEC(AVX512, sp, dp, rem);
        }

        __unaligned_load_store_4zmm_vec((char *) dst + size - 4 * ZMM_SZ, (const char *) src + size - 4 * ZMM_SZ);
        return ret;
    }

#if defined(MEMCPY_AVX512_ZEN6)
    if (likely(size < __repmov_stop_threshold))
    {
        __erms_movsb((void *) dst, src, size);
        return ret;
    }

    {
#elif defined(MEMMOVE_AVX512_ZEN6)
    /* Only copies past __repmov_stop_threshold get here. */
    {
#else
    /* NT streaming tier */
    if (unlikely(size >= __nt_start_threshold))
    {
#endif
        const char *sp = (const char *) src;
        char *dp = (char *) dst;

        __unaligned_load_store_1zmm_vec(dp, sp);

        size_t rem;
        ALIGN_ADVANCE_64(sp, dp, size, rem);

        while (rem > 4 * ZMM_SZ)
        {
            __unaligned_load_nt_store_4zmm_vec(dp, sp);
            ADVANCE_4VEC(AVX512, sp, dp, rem);
        }
        ALM_STORE_FENCE()

        __unaligned_load_store_4zmm_vec((char *) dst + size - 4 * ZMM_SZ, (const char *) src + size - 4 * ZMM_SZ);
#if !defined(MEMCPY_AVX512_ZEN6) && !defined(MEMMOVE_AVX512_ZEN6)
        return ret;
    }

    /* ERMS tier (rep movsb) - repmov_start <= size < nt_start */
    __erms_movsb((void *) dst, src, size);
#else
    }
#endif
    return ret;
}

/* The rep-movsb tier is threshold-gated at runtime, so the ERMS and non-ERMS
 * entry points share one body; alias the plain name for generic dispatch. */
#define _memcpy_avx512 _memcpy_avx512_erms

#ifdef ALM_AVX512_MEMMOVE
#undef ALM_AVX512_MEMMOVE
#endif

#ifdef MEMMOVE_AVX512
#undef MEMMOVE_AVX512
#endif

#ifdef MEMMOVE_AVX512_ERMS
#undef MEMMOVE_AVX512_ERMS
#endif

#ifdef MEMMOVE_AVX512_ZEN6
#undef MEMMOVE_AVX512_ZEN6
#endif

#ifdef MEMCPY_AVX512_ZEN6
#undef MEMCPY_AVX512_ZEN6
#endif

#endif /* MEMCPY_IMPL_AVX512_H */
