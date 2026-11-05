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
#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>
#include "almem_defs.h"
#include "logger.h"
#include <zen_cpu_info.h>

/* This function is an optimized version of strlen/strnlen using AVX-512 instructions.
It calculates the length of the given string and returns it */

#ifdef STRNLEN_AVX512
static inline size_t __attribute__((flatten)) _strnlen_avx512(const char *str, size_t maxlen)
#else
static inline size_t __attribute__((flatten)) _strlen_avx512(const char *str)
#endif
{
#ifdef STRNLEN_AVX512
    if (maxlen == 0)
        return 0;
#endif

    size_t offset;
    __m512i z0, z1, z2, z3, z4, z5, z6;
    uint64_t ret, ret1, ret2;

    // Calculate the offset to align the source pointer to 64 bytes
    offset = (uintptr_t)str & (ZMM_SZ - 1);

    // Initialize a zeroed AVX-512 register for comparisons
    z0 = _mm512_setzero_epi32();

#ifdef STRNLEN_AVX512
    /* Masked ZMM load for small maxlen. The mask only suppresses faults for
     * masked-OFF bytes; masked-ON bytes on an unmapped page still fault.
     * So we need a page-cross check when str+maxlen would cross a page. */
    if (likely(maxlen <= ZMM_SZ))
    {
        if (likely((((uintptr_t)str & (PAGE_SZ - 1)) + maxlen) <= PAGE_SZ))
        {
            /* Mask the load to maxlen bytes. BZHI takes its index from bits
             * 7:0 and only clears when index < 64, so maxlen == ZMM_SZ
             * correctly yields an all-ones mask. Note the flip side: any
             * index >= 256 would truncate to a wrong mask, so every BZHI
             * below must be fed a count already bounded by ZMM_SZ. */
            __mmask64 mask = _bzhi_u64(UINT64_MAX, maxlen);
            z1 = _mm512_maskz_loadu_epi8(mask, str);
            ret = _mm512_cmpeq_epu8_mask(z1, z0);
            return _tzcnt_u64(ret);
        }
        /* Page-cross: split into two safe loads across the page boundary.
         * Must AND compare results with masks to ignore false zeros from maskz.
         * Both BZHI counts stay inside the 8-bit index: reaching here needs
         * str to cross the page end within maxlen <= ZMM_SZ bytes, which
         * bounds page_rem to [1, ZMM_SZ - 1] and rem to [1, ZMM_SZ - 1]. */
        size_t page_rem = PAGE_SZ - ((uintptr_t)str & (PAGE_SZ - 1));
        __mmask64 mask = _bzhi_u64(UINT64_MAX, page_rem);
        z1 = _mm512_maskz_loadu_epi8(mask, str);
        ret = _mm512_cmpeq_epu8_mask(z1, z0) & mask;
        if (ret)
            return _tzcnt_u64(ret);
        /* Null not in first part; load the remainder on the next page. */
        size_t rem = maxlen - page_rem;
        mask = _bzhi_u64(UINT64_MAX, rem);
        z1 = _mm512_maskz_loadu_epi8(mask, str + page_rem);
        ret = _mm512_cmpeq_epu8_mask(z1, z0) & mask;
        return ret ? _tzcnt_u64(ret) + page_rem : maxlen;
    }

    /* The maxlen <= ZMM_SZ case returned above, so maxlen > ZMM_SZ from here on.
     * That makes any null found within the first vector unconditionally in
     * range, and makes the page end the only limit the first load can hit. */

    // Handle cases where start of string is close to the end of a memory page
    if (unlikely(((PAGE_SZ - ZMM_SZ) < ((PAGE_SZ - 1) & (uintptr_t) str))))
    {
        z6 = _mm512_set1_epi8((char) 0xff);

        // Mask to load only the valid bytes from the string without crossing the page boundary
        __mmask64 mask = UINT64_MAX >> offset;
        /* Masked-off lanes merge with 0xff so they never match the null byte. */
        z1 = _mm512_mask_loadu_epi8(z6, mask, str);
        ret = _mm512_cmpeq_epu8_mask(z1, z0);
        if (ret)
            return _tzcnt_u64(ret);
    }
    else
    {
        /* Aligned load + mask shift: never crosses cache line or page. */
        z1 = _mm512_load_si512((const __m512i *) (str - offset));
        ret = _mm512_cmpeq_epu8_mask(z1, z0) >> offset;
        if (likely(ret != 0))
            return _tzcnt_u64(ret);
    }
#else
    /* strlen path: no maxlen bound, scan until the null terminator. This one
     * check guards both the YMM probe and the ZMM load below, since a pointer
     * safe for a 64B load is necessarily safe for a 32B load. */
    // Handle cases where start of string is close to the end of a memory page
    if (unlikely(((PAGE_SZ - ZMM_SZ) < ((PAGE_SZ - 1) & (uintptr_t) str))))
    {
        /* Aligned load + mask shift: never crosses cache line or page. */
        z1 = _mm512_load_si512((const __m512i *) (str - offset));
        ret = _mm512_cmpeq_epu8_mask(z1, z0) >> offset;
        if (likely(ret))
            return _tzcnt_u64(ret);
    }
    else
    {
        /* YMM probe first: VEX 32-bit ops resolve short strings in the
         * fewest instructions, and cover a full 32B from str. */
        __m256i y1 = _mm256_loadu_si256((const __m256i *) str);
        ret = _mm256_cmpeq_epu8_mask(y1, _mm256_setzero_si256());
        if (likely(ret))
            return _tzcnt_u64(ret);
        // Load first 64 bytes from `str` into z1 and check for null terminator
        z1 = _mm512_loadu_si512((const __m512i *) str);
        ret = _mm512_cmpeq_epu8_mask(z1, z0);
        if (likely(ret))
            return _tzcnt_u64(ret);
    }
#endif

    // Adjust the offset to align with cache line for the next load operation
    offset = ZMM_SZ - offset;
#ifdef STRNLEN_AVX512
    if (offset >= maxlen)
        return maxlen;
#endif

    // Process the next 7 vectors (7 * 64B) with offset adjustments
    uint8_t cnt_vec = 7;
#ifdef STRNLEN_AVX512
    while (cnt_vec-- && offset + ZMM_SZ <= maxlen)
#else
    while (cnt_vec--)
#endif
    {
        z1 = _mm512_load_si512(str + offset);
        ret = _mm512_cmpeq_epu8_mask(z1, z0);
        if (ret)
            return _tzcnt_u64(ret) + offset;
        offset += ZMM_SZ;
    }

    // Handle remaining bytes if less than full vector
#ifdef STRNLEN_AVX512
    if (offset < maxlen && offset + ZMM_SZ > maxlen)
    {
        size_t rem = maxlen - offset;
        __mmask64 mask = _bzhi_u64(UINT64_MAX, rem);
        z1 = _mm512_maskz_loadu_epi8(mask, str + offset);
        ret = _mm512_cmpeq_epu8_mask(z1, z0) & mask;
        if (ret)
            return _tzcnt_u64(ret) + offset;
        return maxlen;
    }
#endif

    // Ensure the next 4 * 64B vector loads in the main loop
    // do not cross a page boundary to prevent potential page faults.
    cnt_vec = (AVX512_VEC_4_SZ / ZMM_SZ)
              - ((((uintptr_t) str + offset) & (AVX512_VEC_4_SZ - 1)) / ZMM_SZ);
#ifdef STRNLEN_AVX512
    while (cnt_vec-- && offset + ZMM_SZ <= maxlen)
#else
    while (cnt_vec--)
#endif
    {
        z1 = _mm512_load_si512(str + offset);
        ret = _mm512_cmpeq_epu8_mask(z1, z0);
        if (ret)
            return _tzcnt_u64(ret) + offset;
        offset += ZMM_SZ;
    }

    // Main loop to process 4 vectors at a time for larger sizes(> 512B)
#ifdef STRNLEN_AVX512
    while (offset + AVX512_VEC_4_SZ <= maxlen)
    {
        // Load 4 vectors from `str`
        z1 = _mm512_load_si512(str + offset);
        z2 = _mm512_load_si512(str + offset + ZMM_SZ);
        z3 = _mm512_load_si512(str + offset + 2 * ZMM_SZ);
        z4 = _mm512_load_si512(str + offset + 3 * ZMM_SZ);

        // Find the minimum of the loaded vectors to check for null terminator
        z5 = _mm512_min_epu8(z1, z2);
        z6 = _mm512_min_epu8(z3, z4);

        // Compare the minimums to find the null terminator
        ret = _mm512_cmpeq_epu8_mask(_mm512_min_epu8(z5, z6), z0);

        if (ret)
            break;

        offset += AVX512_VEC_4_SZ;
    }

    // Check if we found null in the 4-vector block
    if (ret)
    {
#else
    offset -= AVX512_VEC_4_SZ;
    do
    {
        offset += AVX512_VEC_4_SZ;
        // Load 4 vectors from `str`
        z1 = _mm512_load_si512(str + offset);
        z2 = _mm512_load_si512(str + offset + ZMM_SZ);
        z3 = _mm512_load_si512(str + offset + 2 * ZMM_SZ);
        z4 = _mm512_load_si512(str + offset + 3 * ZMM_SZ);

        // Find the minimum of the loaded vectors to check for null terminator
        z5 = _mm512_min_epu8(z1, z2);
        z6 = _mm512_min_epu8(z3, z4);

        // Compare the minimums to find the null terminator
        ret = _mm512_cmpeq_epu8_mask(_mm512_min_epu8(z5, z6), z0);
    } while (!ret);
#endif

        // Check for null terminator in the first two vectors (z1, z2)
        if ((ret1 = _mm512_cmpeq_epu8_mask(z5, z0)))
        {
            if ((ret2 = _mm512_cmpeq_epu8_mask(z1, z0)))
            {
                return (_tzcnt_u64(ret2) + offset);
            }
            return (_tzcnt_u64(ret1) + ZMM_SZ + offset);
        }
        // Check for null terminator in the last two vectors (z3, z4)
        else
        {
            if ((ret2 = _mm512_cmpeq_epu8_mask(z3, z0)))
            {
                return (_tzcnt_u64(ret2) + 2 * ZMM_SZ + offset);
            }
            return (_tzcnt_u64(ret) + 3 * ZMM_SZ + offset);
        }
#ifdef STRNLEN_AVX512
    }

    // Handle any remaining full vectors
    while (offset + ZMM_SZ <= maxlen)
    {
        z1 = _mm512_load_si512(str + offset);
        ret = _mm512_cmpeq_epu8_mask(z1, z0);
        if (ret)
            return _tzcnt_u64(ret) + offset;
        offset += ZMM_SZ;
    }

    // Handle final partial vector
    if (offset < maxlen)
    {
        size_t rem = maxlen - offset;
        __mmask64 mask = _bzhi_u64(UINT64_MAX, rem);
        z1 = _mm512_maskz_loadu_epi8(mask, str + offset);
        ret = _mm512_cmpeq_epu8_mask(z1, z0) & mask;
        if (ret)
            return _tzcnt_u64(ret) + offset;
    }

    return maxlen;
#endif
}
