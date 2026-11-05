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
#include <zen_cpu_info.h>

#ifdef STRNLEN_ZEN4
static inline size_t __attribute__((flatten)) _strnlen_zen4(const char *str, size_t maxlen)
#else
HIDDEN_SYMBOL size_t __attribute__((flatten)) __strlen_zen4(const char *str)
#endif
{
#ifdef STRNLEN_ZEN4
    // Bytes below maxlen are always accessed: keep the window in-page.
    if ((maxlen <= YMM_SZ) &&
        ((((uintptr_t) str & (PAGE_SZ - 1)) + maxlen) <= PAGE_SZ))
    {
        __mmask32 mask = _bzhi_u32(UINT32_MAX, maxlen);
        __m256i v = _mm256_maskz_loadu_epi8(mask, str);
        __mmask32 m = _mm256_testn_epi8_mask(v, v);
        return (size_t) _tzcnt_u32(m);
    }
    if ((maxlen < ZMM_SZ) &&
        (((uintptr_t) str & (PAGE_SZ - 1)) <= (PAGE_SZ - ZMM_SZ)))
    {
        __m256i v0 = _mm256_loadu_si256((__m256i *) str);
        __mmask32 m0 = _mm256_testn_epi8_mask(v0, v0);
        if (m0)
        {
            return (size_t) _tzcnt_u32(m0);
        }
        __mmask32 mask = _bzhi_u32(UINT32_MAX, maxlen - YMM_SZ);
        __m256i v1 = _mm256_maskz_loadu_epi8(mask, str + YMM_SZ);
        __mmask32 m1 = _mm256_testn_epi8_mask(v1, v1);
        return m1 ? YMM_SZ + (size_t) _tzcnt_u32(m1) : maxlen;
    }
#endif

    const char *p = str;
    __m512i z1, z2, z3, z4, z5, z6;
    __mmask64 match;

    if (unlikely((PAGE_SZ - ZMM_SZ) < ((uintptr_t) p & (PAGE_SZ - 1))))
    {
        size_t safe = PAGE_SZ - ((uintptr_t) p & (PAGE_SZ - 1));
#ifdef STRNLEN_ZEN4
        safe = (safe > maxlen) ? maxlen : safe;
#endif
        __mmask64 smask = _bzhi_u64(UINT64_MAX, safe);
        z1 = _mm512_mask_loadu_epi8(_mm512_set1_epi8((char) 0xff), smask, p);
        match = _mm512_testn_epi8_mask(z1, z1);
        if (match)
        {
            size_t pos = (size_t) (p - str) + _tzcnt_u64(match);
#ifdef STRNLEN_ZEN4
            return (pos < maxlen) ? pos : maxlen;
#else
            return pos;
#endif
        }
#ifdef STRNLEN_ZEN4
        if (safe >= maxlen)
        {
            return maxlen;
        }
#endif
        p += safe;
    }
    else
    {
        z1 = _mm512_loadu_si512(p);
        match = _mm512_testn_epi8_mask(z1, z1);
        if (match)
        {
            size_t pos = (size_t) (p - str) + _tzcnt_u64(match);
#ifdef STRNLEN_ZEN4
            return (pos < maxlen) ? pos : maxlen;
#else
            return pos;
#endif
        }
#ifdef STRNLEN_ZEN4
        if (maxlen <= ZMM_SZ)
        {
            return maxlen;
        }
#endif
        p = (const char *) ((uintptr_t) p & ~(uintptr_t) (ZMM_SZ - 1)) + ZMM_SZ;
    }

    /* Second aligned probe (33-128 B). */
    z1 = _mm512_load_si512(p);
    match = _mm512_testn_epi8_mask(z1, z1);
    if (match)
    {
        size_t pos = (size_t) (p - str) + _tzcnt_u64(match);
#ifdef STRNLEN_ZEN4
        return (pos < maxlen) ? pos : maxlen;
#else
        return pos;
#endif
    }
    p += ZMM_SZ;
#ifdef STRNLEN_ZEN4
    if ((size_t)(p - str) >= maxlen)
    {
        return maxlen;
    }
#endif

    /* Third and fourth aligned probes (129-256 B). */
    z1 = _mm512_load_si512(p);
    match = _mm512_testn_epi8_mask(z1, z1);
    if (match)
    {
        size_t pos = (size_t) (p - str) + _tzcnt_u64(match);
#ifdef STRNLEN_ZEN4
        return (pos < maxlen) ? pos : maxlen;
#else
        return pos;
#endif
    }
    p += ZMM_SZ;
#ifdef STRNLEN_ZEN4
    if ((size_t)(p - str) >= maxlen)
    {
        return maxlen;
    }
#endif

    z1 = _mm512_load_si512(p);
    match = _mm512_testn_epi8_mask(z1, z1);
    if (match)
    {
        size_t pos = (size_t) (p - str) + _tzcnt_u64(match);
#ifdef STRNLEN_ZEN4
        return (pos < maxlen) ? pos : maxlen;
#else
        return pos;
#endif
    }
    p += ZMM_SZ;
#ifdef STRNLEN_ZEN4
    if ((size_t)(p - str) >= maxlen)
    {
        return maxlen;
    }
#endif

    while ((uintptr_t) p & (4 * ZMM_SZ - 1))
    {
        z1 = _mm512_load_si512(p);
        match = _mm512_testn_epi8_mask(z1, z1);
        if (match)
        {
            size_t pos = (size_t) (p - str) + _tzcnt_u64(match);
#ifdef STRNLEN_ZEN4
            return (pos < maxlen) ? pos : maxlen;
#else
            return pos;
#endif
        }
        p += ZMM_SZ;
#ifdef STRNLEN_ZEN4
        if ((size_t)(p - str) >= maxlen)
        {
            return maxlen;
        }
#endif
    }

#ifdef STRNLEN_ZEN4
    size_t remaining = maxlen - (size_t)(p - str);
#endif

    while (1)
    {
#ifdef STRNLEN_ZEN4
        if (unlikely(remaining < AVX512_VEC_4_SZ))
        {
            while (remaining > 0)
            {
                z1 = _mm512_load_si512(p);
                match = _mm512_testn_epi8_mask(z1, z1);
                if (match)
                {
                    size_t pos = (size_t) (p - str) + _tzcnt_u64(match);
                    return (pos < maxlen) ? pos : maxlen;
                }
                p += ZMM_SZ;
                remaining = (remaining > ZMM_SZ) ? remaining - ZMM_SZ : 0;
            }
            return maxlen;
        }
#endif
        PREFETCH_T0(p + 12 * CACHELINE_SZ);
        z1 = _mm512_load_si512(p);
        z2 = _mm512_load_si512(p + ZMM_SZ);
        z3 = _mm512_load_si512(p + 2 * ZMM_SZ);
        z4 = _mm512_load_si512(p + 3 * ZMM_SZ);
        z5 = _mm512_min_epu8(z1, z2);
        z6 = _mm512_min_epu8(z3, z4);
        z5 = _mm512_min_epu8(z5, z6);
        match = _mm512_testn_epi8_mask(z5, z5);
        if (unlikely(match))
            break;
        p += AVX512_VEC_4_SZ;
#ifdef STRNLEN_ZEN4
        remaining -= AVX512_VEC_4_SZ;
#endif
    }

    __mmask64 m0 = _mm512_testn_epi8_mask(z1, z1);
    __mmask64 m1 = _mm512_testn_epi8_mask(z2, z2);
    __mmask64 m2 = _mm512_testn_epi8_mask(z3, z3);
    size_t pos;
    if (m0)
        pos = (size_t) (p - str) + _tzcnt_u64(m0);
    else if (m1)
        pos = (size_t) (p - str) + ZMM_SZ + _tzcnt_u64(m1);
    else if (m2)
        pos = (size_t) (p - str) + 2 * ZMM_SZ + _tzcnt_u64(m2);
    else
    {
        __mmask64 m3 = _mm512_testn_epi8_mask(z4, z4);
        pos = (size_t) (p - str) + 3 * ZMM_SZ + _tzcnt_u64(m3);
    }
#ifdef STRNLEN_ZEN4
    return (pos < maxlen) ? pos : maxlen;
#else
    return pos;
#endif
}

#ifndef STRNLEN_ZEN4
#ifndef ALMEM_DYN_DISPATCH
size_t strlen(const char *) __attribute__((weak, alias("__strlen_zen4")));
#endif
#endif
