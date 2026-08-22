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

#include <stddef.h>
#include "logger.h"
#include <stdint.h>
#include <immintrin.h>
#include "almem_defs.h"
#include "zen_cpu_info.h"

#ifndef UNALIGNED_SPAN
#define UNALIGNED_SPAN  (5 * ZMM_SZ)
#endif

#ifndef PREFETCH_DIST
#define PREFETCH_DIST   (4 * ZMM_SZ)
#endif

#ifndef PREFETCH_HINT
#define PREFETCH_HINT   _MM_HINT_NTA
#endif

#define COPY_TAIL(vec, off, m)                                              \
    do {                                                                    \
        _mm512_mask_storeu_epi8(dst + (off), TAIL_MASK64(m), (vec));        \
        return ret;                                                         \
    } while (0)

#define STRCPY_HEAD_VEC()                                                    \
    do {                                                                     \
        z0 = _mm512_loadu_si512(src);                                        \
        match = _mm512_testn_epi8_mask(z0, z0);                              \
        if (match)                                                           \
        {                                                                    \
            if (likely((uint32_t)match))                                     \
            {                                                                \
                _mm256_mask_storeu_epi8(dst, TAIL_MASK32((uint32_t)match),   \
                                        _mm512_castsi512_si256(z0));         \
                return ret;                                                  \
            }                                                                \
            _mm256_storeu_si256((__m256i *)dst,                              \
                                _mm512_castsi512_si256(z0));                 \
            _mm256_mask_storeu_epi8(dst + YMM_SZ,                            \
                                    TAIL_MASK32((uint32_t)(match >> 32)),    \
                                    _mm512_extracti64x4_epi64(z0, 1));       \
            return ret;                                                      \
        }                                                                    \
        _mm512_storeu_si512(dst, z0);                                        \
    } while (0)

HIDDEN_SYMBOL char * __attribute__((flatten)) __strcpy_zen4(char *dst, const char *src)
{
    size_t offset;
    uint64_t match;
    __m512i z0, z1, z2, z3, z4, z5;

    register void *ret asm("rax");
    ret = dst;

    LOG_INFO("\n");

    if (unlikely((PAGE_SZ - ZMM_SZ) < ((PAGE_SZ - 1) & (uintptr_t)src)))
    {
        __mmask64 mask = UINT64_MAX >> ((uintptr_t)src & (ZMM_SZ - 1));
        z1 = _mm512_mask_loadu_epi8(_mm512_set1_epi8(0xff), mask, src);
        match = _mm512_testn_epi8_mask(z1, z1);
        if (match)
        {
            _mm512_mask_storeu_epi8(dst, TAIL_MASK64(match), z1);
            return ret;
        }
        /* No NUL before the page end. */
        STRCPY_HEAD_VEC();
    }
    else
    {
        STRCPY_HEAD_VEC();
    }

    offset = ZMM_SZ;

    /* Medium strings */
    if (likely(((uintptr_t)src & (PAGE_SZ - 1)) <= (PAGE_SZ - UNALIGNED_SPAN)))
    {
        do
        {
            z0 = _mm512_loadu_si512(src + offset);
            uint64_t mm = _mm512_testn_epi8_mask(z0, z0);
            if (mm)
            {
                COPY_TAIL(z0, offset, mm);
            }

            _mm512_storeu_si512(dst + offset, z0);
            offset += ZMM_SZ;
        } while (offset < UNALIGNED_SPAN);
    }
    offset -= (uintptr_t)(src + offset) & (ZMM_SZ - 1);
    while (1)
    {
        size_t page_rem = PAGE_SZ - ((uintptr_t)(src + offset) & (PAGE_SZ - 1));

        if (unlikely(page_rem < 4 * ZMM_SZ))
        {
            do
            {
                z0 = _mm512_load_si512(src + offset);
                uint64_t am = _mm512_testn_epi8_mask(z0, z0);
                if (am)
                {
                    COPY_TAIL(z0, offset, am);
                }

                _mm512_storeu_si512(dst + offset, z0);
                offset += ZMM_SZ;
                page_rem -= ZMM_SZ;
            } while (page_rem);
            continue;
        }

#if PREFETCH_DIST
        _mm_prefetch(src + offset + PREFETCH_DIST, PREFETCH_HINT);
#endif
        z0 = _mm512_load_si512(src + offset);
        z1 = _mm512_load_si512(src + offset + ZMM_SZ);
        z2 = _mm512_load_si512(src + offset + 2 * ZMM_SZ);
        z3 = _mm512_load_si512(src + offset + 3 * ZMM_SZ);

        z4 = _mm512_min_epu8(z0, z1);
        z5 = _mm512_min_epu8(z2, z3);
        z5 = _mm512_min_epu8(z4, z5);

        if (unlikely(_mm512_testn_epi8_mask(z5, z5)))
        {
            uint64_t lm = _mm512_testn_epi8_mask(z4, z4);
            if (lm)
            {
                uint64_t first = _mm512_testn_epi8_mask(z0, z0);
                if (first)
                {
                    COPY_TAIL(z0, offset, first);
                }

                _mm512_storeu_si512(dst + offset, z0);
                COPY_TAIL(z1, offset + ZMM_SZ, lm);
            }

            _mm512_storeu_si512(dst + offset, z0);
            _mm512_storeu_si512(dst + offset + ZMM_SZ, z1);
            uint64_t m3 = _mm512_testn_epi8_mask(z2, z2);
            if (m3)
            {
                COPY_TAIL(z2, offset + 2 * ZMM_SZ, m3);
            }

            _mm512_storeu_si512(dst + offset + 2 * ZMM_SZ, z2);
            uint64_t m4 = _mm512_testn_epi8_mask(z3, z3);
            COPY_TAIL(z3, offset + 3 * ZMM_SZ, m4);
        }

        _mm512_storeu_si512(dst + offset, z0);
        _mm512_storeu_si512(dst + offset + ZMM_SZ, z1);
        _mm512_storeu_si512(dst + offset + 2 * ZMM_SZ, z2);
        _mm512_storeu_si512(dst + offset + 3 * ZMM_SZ, z3);
        offset += 4 * ZMM_SZ;
    }
}

#ifndef ALMEM_DYN_DISPATCH
char *strcpy(char *, const char *) __attribute__((weak,
                        alias("__strcpy_zen4")));
#endif
