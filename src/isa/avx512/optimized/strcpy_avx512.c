/* Copyright (C) 2023-26 Advanced Micro Devices, Inc. All rights reserved.
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
#include "../../../base_impls/memset_erms_impls.h"

#define SINGLE_VEC_THRESHOLD (16 * 1024)

#ifdef STRNCPY_AVX512
/* Store vector v at dst_off, clipped to strncpy size, fill remainder with nulls */
#define STRNCPY_STORE_AND_FILL(vec, dst_off, vec_off)                        \
    do {                                                                      \
        unsigned _np = _tzcnt_u64(_mm512_testn_epi8_mask((vec), (vec)));     \
        size_t _abs = (dst_off) + _np;                                       \
        size_t _cl = (_abs + 1 < size) ? _np + 1 : size - (dst_off);        \
        if (_cl >= ZMM_SZ)                                                   \
            _mm512_storeu_si512((__m512i *) (dst + (dst_off)), (vec));        \
        else {                                                               \
            __mmask64 _m = _bzhi_u64(UINT64_MAX, _cl);                       \
            _mm512_mask_storeu_epi8(dst + (dst_off), _m, (vec));             \
        }                                                                    \
        if (_abs + 1 < size)                                                 \
            _fill_null_avx512(dst + _abs + 1, size - (_abs + 1));            \
        return ret;                                                          \
    } while (0)
#endif

#ifdef STRNCPY_AVX512
/* This is an optimized function to fill a memory region with null bytes ('\0').
 * Uses AVX-512 stores for large sizes, falls back to ERMS for small sizes. */
static inline void *_fill_null_avx512(void *mem, size_t size)
{
    __m512i z0;
    // Use ERMS stosb for small sizes (< 2*ZMM_SZ)
    if (size < 2 * ZMM_SZ)
        return __erms_stosb(mem, 0, size);

    z0 = _mm512_setzero_si512();
    if (size <= 4 * ZMM_SZ)
    {
        _mm512_storeu_si512(mem , z0);
        _mm512_storeu_si512(mem + ZMM_SZ, z0);
        _mm512_storeu_si512(mem + size - 2 * ZMM_SZ, z0);
        _mm512_storeu_si512(mem + size - ZMM_SZ, z0);
        return mem;
    }
    _mm512_storeu_si512(mem + 0 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + 1 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + 2 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + 3 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + size - 4 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + size - 3 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + size - 2 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + size - 1 * ZMM_SZ, z0);

    if (size <= 8 * ZMM_SZ)
        return mem;

    size_t offset = 4 * ZMM_SZ;
    size -= 4 * ZMM_SZ;
    offset -= ((uintptr_t)mem & (ZMM_SZ-1));

    while (offset < size)
    {
        _mm512_store_si512(mem + offset + 0 * ZMM_SZ, z0);
        _mm512_store_si512(mem + offset + 1 * ZMM_SZ, z0);
        _mm512_store_si512(mem + offset + 2 * ZMM_SZ, z0);
        _mm512_store_si512(mem + offset + 3 * ZMM_SZ, z0);
        offset += 4 * ZMM_SZ;
    }
    return mem;
}
#endif

/* This function is an optimized version of strcpy/strncpy using AVX-512 instructions.
 * strcpy: Uses 2x unrolled main loop with back-copy technique for null termination
 *         to avoid expensive masked-store latency on Zen5.
 * strncpy: Uses 4x unrolled main loop with min_epu8 reduction for batch null detection
 *          across 256 bytes before unpacking individual masks. */
#ifdef STRNCPY_AVX512
static inline char * __attribute__((flatten)) _strncpy_avx512(char *dst, const char *src, size_t size)
#else
static inline char * __attribute__((flatten)) _strcpy_avx512(char *dst, const char *src)
#endif
{
#ifdef STRNCPY_AVX512
    if (unlikely(size == 0))
        return dst;
#endif

    register char *ret __asm__("rax") = dst;
    __m512i z1;
    __mmask64 match;
    size_t offset;

    // Calculate the page offset to determine if src is close to a page boundary
    size_t page_off = (uintptr_t) src & (PAGE_SZ - 1);
    if (likely(page_off <= (PAGE_SZ - ZMM_SZ)))
    {
#ifdef STRNCPY_AVX512
        // For strncpy with size <= YMM_SZ, use masked YMM load/store
        if (size <= YMM_SZ)
        {
            __mmask32 ymask = _bzhi_u32(UINT32_MAX, (unsigned int) size);
            __m256i y0 = _mm256_maskz_loadu_epi8(ymask, src);
            __mmask32 match1 = _mm256_testn_epi8_mask(y0, y0) & ymask;
            if (match1)
            {
                unsigned int null_pos = _tzcnt_u32(match1);
                unsigned int copy_len = null_pos + 1;
                __mmask32 mask = _bzhi_u32(UINT32_MAX, copy_len);
                _mm256_mask_storeu_epi8(dst, mask, y0);
                if (copy_len < size)
                    _fill_null_avx512(dst + copy_len, size - copy_len);
            }
            else
            {
                _mm256_mask_storeu_epi8(dst, ymask, y0);
            }
            return ret;
        }
        // For strncpy with YMM_SZ < size < ZMM_SZ, use masked ZMM load/store
        if (size < ZMM_SZ)
        {
            __mmask64 mask = _bzhi_u64(UINT64_MAX, size);
            z1 = _mm512_maskz_loadu_epi8(mask, src);
            match = _mm512_testn_epi8_mask(z1, z1) & mask;
            if (match)
            {
                size_t null_pos = _tzcnt_u64(match);
                size_t copy_len = null_pos + 1;
                mask = _bzhi_u64(UINT64_MAX, copy_len);
                _mm512_mask_storeu_epi8(dst, mask, z1);
                if (copy_len < size)
                    _fill_null_avx512(dst + copy_len, size - copy_len);
            }
            else
            {
                _mm512_mask_storeu_epi8(dst, mask, z1);
            }
            return ret;
        }
#endif
#ifndef STRNCPY_AVX512
        /* First 64B as two YMM loads: YMM masked stores have lower
         * latency than ZMM on Zen5 for short strings (<= 64B). */
        {
            __m256i y0 = _mm256_loadu_si256((const __m256i *) src);
            __mmask32 match1 = _mm256_testn_epi8_mask(y0, y0);
            if (match1)
            {
                unsigned np = _tzcnt_u32(match1);
                __mmask32 mask = _bzhi_u32(UINT32_MAX, np + 1);
                _mm256_mask_storeu_epi8(dst, mask, y0);
                return ret;
            }
            __m256i y1 = _mm256_loadu_si256((const __m256i *) (src + YMM_SZ));
            __mmask32 match2 = _mm256_testn_epi8_mask(y1, y1);
            if (match2)
            {
                unsigned np = _tzcnt_u32(match2);
                _mm256_storeu_si256((__m256i *) dst, y0);
                __mmask32 mask = _bzhi_u32(UINT32_MAX, np + 1);
                _mm256_mask_storeu_epi8(dst + YMM_SZ, mask, y1);
                return ret;
            }
            _mm256_storeu_si256((__m256i *) dst, y0);
            _mm256_storeu_si256((__m256i *) (dst + YMM_SZ), y1);
        }
        offset = ZMM_SZ - ((uintptr_t) src & (ZMM_SZ - 1));
#else
        // For strncpy: load first 64 bytes and check for null terminator
        z1 = _mm512_loadu_si512((const __m512i *) src);
        match = _mm512_testn_epi8_mask(z1, z1);
        if (match)
        {
            // Null found: copy up to null or size bytes, then null-fill remainder
            unsigned null_pos = _tzcnt_u64(match);
            size_t copy_len = (null_pos + 1 < size) ? null_pos + 1 : size;
            if (copy_len >= ZMM_SZ)
                _mm512_storeu_si512((__m512i *) dst, z1);
            else
            {
                __mmask64 mask = _bzhi_u64(UINT64_MAX, copy_len);
                _mm512_mask_storeu_epi8(dst, mask, z1);
            }
            if (null_pos + 1 < size)
                _fill_null_avx512(dst + null_pos + 1, size - (null_pos + 1));
            return ret;
        }
        if (size <= ZMM_SZ)
        {
            __mmask64 mask = _bzhi_u64(UINT64_MAX, size);
            _mm512_mask_storeu_epi8(dst, mask, z1);
            return ret;
        }
        // No null in first 64 bytes: store them and calculate alignment offset
        _mm512_storeu_si512((__m512i *) dst, z1);
        offset = ZMM_SZ - ((uintptr_t) src & (ZMM_SZ - 1));
#endif
    }
    else
    {
        /* Page-crossing fallback: copy byte-by-byte until src is aligned. */
        offset = 0;
        while ((uintptr_t) (src + offset) & (ZMM_SZ - 1))
        {
#ifdef STRNCPY_AVX512
            if (offset >= size)
                return ret;
#endif
            char c = src[offset];
            dst[offset] = c;
            if (c == '\0')
            {
#ifdef STRNCPY_AVX512
                if (offset + 1 < size)
                    _fill_null_avx512(dst + offset + 1, size - (offset + 1));
#endif
                return ret;
            }
            offset++;
        }
    }

#ifdef STRNCPY_AVX512
    const size_t safe_4vec = (size > 4 * ZMM_SZ) ? size - 4 * ZMM_SZ : 0;
#endif

    /* Single-vector warmup: the aligned load may read past 'size' bytes but
     * this is safe because we verified above that we are not near a page
     * boundary and the semantics are preserved by clipping the copy/null-fill
     * to 'size'. */
    {
#ifdef STRNCPY_AVX512
        if (offset >= size)
            return ret;
#endif
        z1 = _mm512_load_si512((const __m512i *) (src + offset));
        match = _mm512_testn_epi8_mask(z1, z1);
        if (match)
        {
            unsigned null_pos = _tzcnt_u64(match);
#ifdef STRNCPY_AVX512
            size_t abs_null = offset + null_pos;
            size_t cl = (abs_null + 1 < size) ? null_pos + 1 : size - offset;
            if (cl >= ZMM_SZ)
                _mm512_storeu_si512((__m512i *) (dst + offset), z1);
            else
            {
                __mmask64 m = _bzhi_u64(UINT64_MAX, cl);
                _mm512_mask_storeu_epi8(dst + offset, m, z1);
            }
            if (abs_null + 1 < size)
                _fill_null_avx512(dst + abs_null + 1, size - (abs_null + 1));
#else
            {
                size_t end = offset + null_pos + 1;
                if (end >= ZMM_SZ) {
                    __m512i tail = _mm512_loadu_si512((const __m512i *) (src + end - ZMM_SZ));
                    _mm512_storeu_si512((__m512i *) (dst + end - ZMM_SZ), tail);
                }
                else
                {
                    __mmask64 mask = _bzhi_u64(UINT64_MAX, null_pos + 1);
                    _mm512_mask_storeu_epi8(dst + offset, mask, z1);
                }
            }
#endif
            return ret;
        }
#ifdef STRNCPY_AVX512
        if (offset + ZMM_SZ > size)
        {
            __mmask64 rm = _bzhi_u64(UINT64_MAX, size - offset);
            _mm512_mask_storeu_epi8(dst + offset, rm, z1);
            return ret;
        }
#endif
        _mm512_storeu_si512((__m512i *) (dst + offset), z1);
        offset += ZMM_SZ;
#ifdef STRNCPY_AVX512
        if (offset >= size)
            return ret;
#endif
    }

#ifndef STRNCPY_AVX512
    /* strcpy main loop: 2x unrolled with back-copy technique.
     * On null detection, load the final 64B ending at the null position and
     * do a full unmasked store to avoid expensive masked-store latency on Zen5. */
    while (1)
    {
        if (unlikely(offset >= SINGLE_VEC_THRESHOLD))
            goto large_str_loop;

        // Page-boundary guard: fall back to single-vector processing
        // when within 2*ZMM_SZ of a page boundary to avoid faulting
        if (unlikely(((uintptr_t) (src + offset) & (PAGE_SZ - 1)) > (PAGE_SZ - 2 * ZMM_SZ)))
        {
            z1 = _mm512_load_si512((const __m512i *) (src + offset));
            match = _mm512_testn_epi8_mask(z1, z1);
            if (match)
            {
                unsigned null_pos = _tzcnt_u64(match);
                size_t end = offset + null_pos + 1;
                __m512i tail = _mm512_loadu_si512((const __m512i *) (src + end - ZMM_SZ));
                _mm512_storeu_si512((__m512i *) (dst + end - ZMM_SZ), tail);
                return ret;
            }
            _mm512_storeu_si512((__m512i *) (dst + offset), z1);
            offset += ZMM_SZ;
            continue;
        }

        // Load 2 vectors and check for null terminator
        {
            __m512i z2 = _mm512_load_si512((const __m512i *) (src + offset));
            __m512i z3 = _mm512_load_si512((const __m512i *) (src + offset + ZMM_SZ));
            __mmask64 match1 = _mm512_testn_epi8_mask(z2, z2);
            __mmask64 match2 = _mm512_testn_epi8_mask(z3, z3);

            if (unlikely(match1 | match2))
            {
                if (match1)
                {
                    unsigned null_pos = _tzcnt_u64(match1);
                    size_t end = offset + null_pos + 1;
                    __m512i tail = _mm512_loadu_si512((const __m512i *) (src + end - ZMM_SZ));
                    _mm512_storeu_si512((__m512i *) (dst + end - ZMM_SZ), tail);
                    return ret;
                }
                _mm512_storeu_si512((__m512i *) (dst + offset), z2);
                {
                    unsigned null_pos = _tzcnt_u64(match2);
                    size_t end = offset + ZMM_SZ + null_pos + 1;
                    __m512i tail = _mm512_loadu_si512((const __m512i *) (src + end - ZMM_SZ));
                    _mm512_storeu_si512((__m512i *) (dst + end - ZMM_SZ), tail);
                }
                return ret;
            }

            _mm_prefetch((const char *) src + offset + 6 * ZMM_SZ, _MM_HINT_T0);
            _mm512_storeu_si512((__m512i *) (dst + offset), z2);
            _mm512_storeu_si512((__m512i *) (dst + offset + ZMM_SZ), z3);
        }
        offset += 2 * ZMM_SZ;
    }
#else
    /* strncpy main loop: 4x unrolled with vectorized null detection.
     * Uses min_epu8 reduction across 4 vectors to batch-check 256 bytes
     * for null before unpacking individual masks.  Same page-boundary
     * guard as strcpy but widened to 4*ZMM_SZ. */
    while (1)
    {
        if (unlikely(offset >= SINGLE_VEC_THRESHOLD))
            goto large_str_loop;

        // Page-boundary guard: fall back to single-vector processing
        // when within 4*ZMM_SZ of a page boundary to avoid faulting
        if (unlikely(((uintptr_t) (src + offset) & (PAGE_SZ - 1)) > (PAGE_SZ - 4 * ZMM_SZ)))
        {
            z1 = _mm512_load_si512((const __m512i *) (src + offset));
            match = _mm512_testn_epi8_mask(z1, z1);
            if (match)
            {
                unsigned null_pos = _tzcnt_u64(match);
                size_t abs_null = offset + null_pos;
                size_t cl = (abs_null + 1 < size) ? null_pos + 1 : size - offset;
                if (cl >= ZMM_SZ)
                    _mm512_storeu_si512((__m512i *) (dst + offset), z1);
                else
                {
                    __mmask64 m = _bzhi_u64(UINT64_MAX, cl);
                    _mm512_mask_storeu_epi8(dst + offset, m, z1);
                }
                if (abs_null + 1 < size)
                    _fill_null_avx512(dst + abs_null + 1, size - (abs_null + 1));
                return ret;
            }
            {
                size_t rem = size - offset;
                if (rem >= ZMM_SZ)
                    _mm512_storeu_si512((__m512i *) (dst + offset), z1);
                else
                {
                    __mmask64 m = _bzhi_u64(UINT64_MAX, rem);
                    _mm512_mask_storeu_epi8(dst + offset, m, z1);
                    return ret;
                }
            }
            offset += ZMM_SZ;
            continue;
        }

        // Handle remaining bytes when fewer than 4 vectors remain
        if (unlikely(offset > safe_4vec))
        {
            if (offset >= size)
                return ret;
            size_t remaining = size - offset;
            while (remaining >= ZMM_SZ)
            {
                z1 = _mm512_load_si512((const __m512i *) (src + offset));
                match = _mm512_testn_epi8_mask(z1, z1);
                if (match)
                {
                    unsigned np = _tzcnt_u64(match);
                    size_t abs_null = offset + np;
                    __mmask64 mask = _bzhi_u64(UINT64_MAX, np + 1);
                    _mm512_mask_storeu_epi8(dst + offset, mask, z1);
                    if (abs_null + 1 < size)
                        _fill_null_avx512(dst + abs_null + 1, size - (abs_null + 1));
                    return ret;
                }
                _mm512_storeu_si512((__m512i *) (dst + offset), z1);
                offset += ZMM_SZ;
                remaining -= ZMM_SZ;
            }
            if (remaining > 0)
            {
                __mmask64 lm = _bzhi_u64(UINT64_MAX, remaining);
                z1 = _mm512_maskz_loadu_epi8(lm, src + offset);
                match = _mm512_testn_epi8_mask(z1, z1) & lm;
                if (match)
                {
                    unsigned np = _tzcnt_u64(match);
                    __mmask64 mask = _bzhi_u64(UINT64_MAX, np + 1);
                    _mm512_mask_storeu_epi8(dst + offset, mask, z1);
                    size_t abs_null = offset + np;
                    if (abs_null + 1 < size)
                        _fill_null_avx512(dst + abs_null + 1, size - (abs_null + 1));
                }
                else
                {
                    _mm512_mask_storeu_epi8(dst + offset, lm, z1);
                }
            }
            return ret;
        }

        // Load 4 vectors from src
        {
            __m512i z2 = _mm512_load_si512((const __m512i *) (src + offset));
            __m512i z3 = _mm512_load_si512((const __m512i *) (src + offset + ZMM_SZ));
            __m512i z4 = _mm512_load_si512((const __m512i *) (src + offset + 2 * ZMM_SZ));
            __m512i z5 = _mm512_load_si512((const __m512i *) (src + offset + 3 * ZMM_SZ));

            // Find the minimum of the loaded vectors to check for null terminator
            __m512i z6 = _mm512_min_epu8(z2, z3);
            __m512i z7 = _mm512_min_epu8(z4, z5);
            __m512i z8 = _mm512_min_epu8(z6, z7);
            __mmask64 match_all = _mm512_testn_epi8_mask(z8, z8);

            // If null found in any of the 4 vectors, determine which one
            if (unlikely(match_all))
            {
                __mmask64 match1 = _mm512_testn_epi8_mask(z2, z2);
                if (match1)
                    STRNCPY_STORE_AND_FILL(z2, offset, 0);
                _mm512_storeu_si512((__m512i *) (dst + offset), z2);

                __mmask64 match2 = _mm512_testn_epi8_mask(z3, z3);
                if (match2)
                    STRNCPY_STORE_AND_FILL(z3, offset + ZMM_SZ, ZMM_SZ);
                _mm512_storeu_si512((__m512i *) (dst + offset + ZMM_SZ), z3);

                __mmask64 match3 = _mm512_testn_epi8_mask(z4, z4);
                if (match3)
                    STRNCPY_STORE_AND_FILL(z4, offset + 2 * ZMM_SZ, 2 * ZMM_SZ);
                _mm512_storeu_si512((__m512i *) (dst + offset + 2 * ZMM_SZ), z4);

                STRNCPY_STORE_AND_FILL(z5, offset + 3 * ZMM_SZ, 3 * ZMM_SZ);
            }

            // No null found: store all 4 vectors to dst
            _mm512_storeu_si512((__m512i *) (dst + offset), z2);
            _mm512_storeu_si512((__m512i *) (dst + offset + ZMM_SZ), z3);
            _mm512_storeu_si512((__m512i *) (dst + offset + 2 * ZMM_SZ), z4);
            _mm512_storeu_si512((__m512i *) (dst + offset + 3 * ZMM_SZ), z5);
        }
        offset += 4 * ZMM_SZ;
    }
#endif

// Large-string single-vector loop: reduces icache pressure for strings
// exceeding SINGLE_VEC_THRESHOLD by processing one vector at a time
// with page-boundary awareness for safe memory access.
large_str_loop:
    while (1)
    {
        if (unlikely(((uintptr_t) (src + offset) & (PAGE_SZ - 1)) > (PAGE_SZ - ZMM_SZ)))
        {
            size_t page_rem = PAGE_SZ - ((uintptr_t) (src + offset) & (PAGE_SZ - 1));
            {
                __mmask64 lm = _bzhi_u64(UINT64_MAX, page_rem);
                z1 = _mm512_maskz_loadu_epi8(lm, src + offset);
                match = _mm512_testn_epi8_mask(z1, z1) & lm;
                if (match)
                {
                    unsigned null_pos = _tzcnt_u64(match);
#ifdef STRNCPY_AVX512
                    size_t abs_null = offset + null_pos;
                    size_t cl = (abs_null + 1 < size) ? null_pos + 1 : size - offset;
                    __mmask64 mask = _bzhi_u64(UINT64_MAX, cl);
                    _mm512_mask_storeu_epi8(dst + offset, mask, z1);
                    if (abs_null + 1 < size)
                        _fill_null_avx512(dst + abs_null + 1, size - (abs_null + 1));
#else
                    size_t end = offset + null_pos + 1;
                    __m512i tail = _mm512_loadu_si512((const __m512i *) (src + end - ZMM_SZ));
                    _mm512_storeu_si512((__m512i *) (dst + end - ZMM_SZ), tail);
#endif
                    return ret;
                }
                _mm512_mask_storeu_epi8(dst + offset, lm, z1);
                offset += page_rem;
#ifdef STRNCPY_AVX512
                if (offset >= size) return ret;
#endif
                continue;
            }
        }
#ifdef STRNCPY_AVX512
        if (offset >= size) return ret;
        if (offset + ZMM_SZ > size) {
            size_t rem = size - offset;
            __mmask64 lm = _bzhi_u64(UINT64_MAX, rem);
            z1 = _mm512_maskz_loadu_epi8(lm, src + offset);
            match = _mm512_testn_epi8_mask(z1, z1) & lm;
            if (match) {
                unsigned np = _tzcnt_u64(match);
                __mmask64 mask = _bzhi_u64(UINT64_MAX, np + 1);
                _mm512_mask_storeu_epi8(dst + offset, mask, z1);
                size_t abs_null = offset + np;
                if (abs_null + 1 < size)
                    _fill_null_avx512(dst + abs_null + 1, size - (abs_null + 1));
            } else {
                _mm512_mask_storeu_epi8(dst + offset, lm, z1);
            }
            return ret;
        }
#endif
        z1 = _mm512_load_si512((const __m512i *)(src + offset));
        match = _mm512_testn_epi8_mask(z1, z1);
        if (match) {
            unsigned null_pos = _tzcnt_u64(match);
#ifdef STRNCPY_AVX512
            size_t abs_null = offset + null_pos;
            size_t cl = (abs_null + 1 < size) ? null_pos + 1 : size - offset;
            if (cl >= ZMM_SZ)
                _mm512_storeu_si512((__m512i *)(dst + offset), z1);
            else {
                __mmask64 m = _bzhi_u64(UINT64_MAX, cl);
                _mm512_mask_storeu_epi8(dst + offset, m, z1);
            }
            if (abs_null + 1 < size)
                _fill_null_avx512(dst + abs_null + 1, size - (abs_null + 1));
#else
            {
                size_t end = offset + null_pos + 1;
                __m512i tail = _mm512_loadu_si512((const __m512i *)(src + end - ZMM_SZ));
                _mm512_storeu_si512((__m512i *)(dst + end - ZMM_SZ), tail);
            }
#endif
            return ret;
        }
        _mm512_storeu_si512((__m512i *)(dst + offset), z1);
        offset += ZMM_SZ;
    }
}
