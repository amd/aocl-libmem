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

#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>

#include "../../base_impls/load_store_impls.h"
#include "logger.h"

/* Fixed size thresholds for the large-copy tiers, tuned for DC-Perf wdl
 * traffic (kept as constants rather than cache-derived to preserve the
 * tuned switch points). */
#define MEMCPY_ERMS_START (4 * 1024)       /* 4 KB  */
#define MEMCPY_ERMS_STOP (4 * 1024 * 1024) /* 4 MB  */
#define MEMCPY_NT_START (16 * 1024 * 1024) /* 16 MB */

HIDDEN_SYMBOL void *__attribute__((flatten)) __memcpy_zen4(void *__restrict dst, const void *__restrict src,
                                                           size_t size)
{
    LOG_INFO("\n");
    register void *ret asm("rax");
    ret = dst;

    __m512i z0, z1;
    __m256i y0, y1, y2, y3, y4, y5, y6, y7;
    __mmask64 mask;
    size_t offset;

    if (likely(size <= 2 * ZMM_SZ))
    {
        if (size < ZMM_SZ)
        {
            if (size <= XMM_SZ)
            {
                __mmask16 mask16 = (__mmask16) _bzhi_u32((uint32_t) -1, (uint32_t) size);
                _mm_mask_storeu_epi8(dst, mask16, _mm_maskz_loadu_epi8(mask16, src));
                return ret;
            }
            if (size <= YMM_SZ)
            {
                __mmask32 mask32 = _bzhi_u32((uint32_t) -1, (uint32_t) size);
                _mm256_mask_storeu_epi8(dst, mask32, _mm256_maskz_loadu_epi8(mask32, src));
                return ret;
            }
            mask = _bzhi_u64((uint64_t) -1, (uint8_t) size);
            z0 = _mm512_maskz_loadu_epi8(mask, src);
            _mm512_mask_storeu_epi8(dst, mask, z0);
            return ret;
        }
        /* 64-128B: overlapping head/tail ZMM */
        z0 = _mm512_loadu_si512((const char *) src);
        z1 = _mm512_loadu_si512((const char *) src + size - ZMM_SZ);
        _mm512_storeu_si512((char *) dst, z0);
        _mm512_storeu_si512((char *) dst + size - ZMM_SZ, z1);
        return ret;
    }
    if (size <= 4 * ZMM_SZ)
    {
        __load_store_le_4zmm_vec(dst, src, size);
        return ret;
    }

    /* >=16MB: non-temporal 8-YMM (256B/iteration) stream loop with an
     * overlapping head and tail. */
    if (size >= MEMCPY_NT_START)
    {
        __load_store_8ymm_vec(dst, src, 0);

        for (offset = 8 * YMM_SZ - ((uintptr_t) dst & (YMM_SZ - 1)); offset < size - 8 * YMM_SZ; offset += 8 * YMM_SZ)
        {
            PREFETCH_NTA(src + offset + 2 * YMM_SZ);
            PREFETCH_NTA(src + offset + 6 * YMM_SZ);
            y0 = _mm256_loadu_si256((const __m256i *) (src + offset));
            y1 = _mm256_loadu_si256((const __m256i *) (src + offset + YMM_SZ));
            y2 = _mm256_loadu_si256((const __m256i *) (src + offset + 2 * YMM_SZ));
            y3 = _mm256_loadu_si256((const __m256i *) (src + offset + 3 * YMM_SZ));
            y4 = _mm256_loadu_si256((const __m256i *) (src + offset + 4 * YMM_SZ));
            y5 = _mm256_loadu_si256((const __m256i *) (src + offset + 5 * YMM_SZ));
            y6 = _mm256_loadu_si256((const __m256i *) (src + offset + 6 * YMM_SZ));
            y7 = _mm256_loadu_si256((const __m256i *) (src + offset + 7 * YMM_SZ));
            ALM_MEM_BARRIER();
            _mm256_stream_si256((__m256i *) (dst + offset), y0);
            _mm256_stream_si256((__m256i *) (dst + offset + YMM_SZ), y1);
            _mm256_stream_si256((__m256i *) (dst + offset + 2 * YMM_SZ), y2);
            _mm256_stream_si256((__m256i *) (dst + offset + 3 * YMM_SZ), y3);
            _mm256_stream_si256((__m256i *) (dst + offset + 4 * YMM_SZ), y4);
            _mm256_stream_si256((__m256i *) (dst + offset + 5 * YMM_SZ), y5);
            _mm256_stream_si256((__m256i *) (dst + offset + 6 * YMM_SZ), y6);
            _mm256_stream_si256((__m256i *) (dst + offset + 7 * YMM_SZ), y7);
        }
        _mm_sfence();
        __load_store_8ymm_vec(dst, src, size - 8 * YMM_SZ);
        return ret;
    }

    /* 4KB-4MB: rep-movsb (ERMS). */
    if (size >= MEMCPY_ERMS_START && size < MEMCPY_ERMS_STOP)
    {
        void *d = dst;
        const void *s = src;
        size_t count = size;
        __asm__ __volatile__("rep movsb" : "+D"(d), "+S"(s), "+c"(count) : : "memory");
        return ret;
    }

    /* 257B-4KB: 4-YMM head, dst-aligned 4-YMM (128B/iteration) store loop,
     * and an overlapping 4-YMM tail. */
    __load_store_4ymm_vec(dst, src, 0);

    for (offset = 4 * YMM_SZ - ((uintptr_t) dst & (YMM_SZ - 1)); offset + 4 * YMM_SZ <= size; offset += 4 * YMM_SZ)
    {
        y0 = _mm256_loadu_si256((const __m256i *) (src + offset));
        y1 = _mm256_loadu_si256((const __m256i *) (src + offset + YMM_SZ));
        y2 = _mm256_loadu_si256((const __m256i *) (src + offset + 2 * YMM_SZ));
        y3 = _mm256_loadu_si256((const __m256i *) (src + offset + 3 * YMM_SZ));
        ALM_MEM_BARRIER();
        _mm256_store_si256((__m256i *) (dst + offset), y0);
        _mm256_store_si256((__m256i *) (dst + offset + YMM_SZ), y1);
        _mm256_store_si256((__m256i *) (dst + offset + 2 * YMM_SZ), y2);
        _mm256_store_si256((__m256i *) (dst + offset + 3 * YMM_SZ), y3);
    }

    __load_store_4ymm_vec(dst, src, size - 4 * YMM_SZ);
    return ret;
}

#ifndef ALMEM_DYN_DISPATCH
void *memcpy(void *, const void *, size_t) __attribute__((weak, alias("__memcpy_zen4")));
#endif
