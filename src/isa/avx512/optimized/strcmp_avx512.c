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
/* This function is an optimized version of strcmp and strncmp using AVX-512 instructions.
It compares the two strings str1 and str2 and returns an integer
indicating the result of the comparison. */

/*
 * Fused null+compare for YMM (32B): vptestmb builds a non-zero mask, then
 * vpcmpeqb{k} checks equality only where bytes are non-zero.  If all 32
 * mask bits are set (all bytes match and none are null), (uint32_t)eq + 1
 * overflows to 0; any mismatch or null leaves a non-zero value whose
 * tzcnt gives the first differing/null position.
 */
#define ALMEM_STRCMP_CHECK_YMM(s1, s2, idx)                             \
    do {                                                                \
        __m256i _y1 = _mm256_loadu_si256((const __m256i *)(s1));        \
        __m256i _y2 = _mm256_loadu_si256((const __m256i *)(s2));        \
        __mmask32 _nz = _mm256_test_epi8_mask(_y1, _y1);               \
        __mmask32 _eq = _mm256_mask_cmpeq_epu8_mask(_nz, _y1, _y2);    \
        (idx) = (uint32_t)_eq + 1;                                     \
    } while (0)

/* Fused null+compare for ZMM (64B), same logic as YMM but 64-bit mask. */
#define ALMEM_STRCMP_CHECK_ZMM(s1, s2, idx)                             \
    do {                                                                \
        __m512i _z1 = _mm512_loadu_si512((const __m512i *)(s1));        \
        __m512i _z2 = _mm512_loadu_si512((const __m512i *)(s2));        \
        __mmask64 _nz = _mm512_test_epi8_mask(_z1, _z1);               \
        __mmask64 _eq = _mm512_mask_cmpeq_epu8_mask(_nz, _z1, _z2);    \
        (idx) = (uint64_t)_eq + 1;                                     \
    } while (0)

#ifdef STRNCMP
static inline int __attribute__((flatten)) _strncmp_avx512(const char *str1, const char *str2, size_t size)
#else
static inline int __attribute__((flatten)) _strcmp_avx512(const char *str1, const char *str2)
#endif
{
#ifdef STRNCMP
    if (unlikely(size == 0))
        return 0;
#endif

    size_t offset1, offset2, offset;
    __m512i z1, z2, z3, z4;
    uint64_t cmp_idx, ret, mask;

    /* Check whether either pointer is within 2*ZMM_SZ (128B) of a page
     * boundary.  The fast path below issues up to two unaligned loads
     * (2×YMM covering bytes 0-63, then 1×ZMM covering bytes 64-127),
     * so we need 128B of safe headroom to avoid crossing into an
     * unmapped page. */
    if (unlikely(((PAGE_SZ - 2 * ZMM_SZ) < ((PAGE_SZ - 1) & ((uintptr_t) str1 | (uintptr_t) str2)))))
    {
        offset1 = (uintptr_t) str1 & (ZMM_SZ - 1);
        offset2 = (uintptr_t) str2 & (ZMM_SZ - 1);
        offset = (offset1 >= offset2) ? offset1 : offset2;

        // Create a mask that will ignore the first 'offset' bytes when loading data
        mask = UINT64_MAX >> offset;

        z1 = _mm512_maskz_loadu_epi8(mask, str1);
        z2 = _mm512_maskz_loadu_epi8(mask, str2);

        // Compare the vectors for equality and check for null terminators
        ret = (_mm512_testn_epi8_mask(z1, z1) | _mm512_cmpneq_epu8_mask(z1, z2)) & mask;

        // If there is a difference or a null terminator, find the index of the first diff/null
        if (ret)
        {
            cmp_idx = _tzcnt_u64(ret);
#ifdef STRNCMP
            if (cmp_idx >= size)
                return 0;
#endif
            return (unsigned char) str1[cmp_idx] - (unsigned char) str2[cmp_idx];
        }
#ifdef STRNCMP
        else if (size <= (ZMM_SZ - offset))
            return 0;
#endif
        /* Verified [0, ZMM_SZ - offset); align the next load to a cache line. */
        offset = ZMM_SZ - offset;
    }
    // If the strings are not close to the end of a memory page, load the first 64 bytes
    else
    {
        /* First 32 bytes via YMM (1 uop/load): resolves small strings fast. */
        {
            uint32_t yret;
            ALMEM_STRCMP_CHECK_YMM(str1, str2, yret);
            if (likely(yret))
            {
                cmp_idx = _tzcnt_u32(yret);
#ifdef STRNCMP
                if (cmp_idx >= size)
                    return 0;
#endif
                return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
            }
#ifdef STRNCMP
            if (size <= YMM_SZ)
                return 0;
#endif
        }

        /* Bytes 32-63 via YMM. */
        {
            uint32_t yret;
            ALMEM_STRCMP_CHECK_YMM(str1 + YMM_SZ, str2 + YMM_SZ, yret);
            if (likely(yret))
            {
                cmp_idx = _tzcnt_u32(yret) + YMM_SZ;
#ifdef STRNCMP
                if (cmp_idx >= size)
                    return 0;
#endif
                return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
            }
#ifdef STRNCMP
            if (size <= ZMM_SZ)
                return 0;
#endif
        }

        /* Second ZMM check (bytes 64-127) avoids alignment setup for <=128B strings */
        ALMEM_STRCMP_CHECK_ZMM(str1 + ZMM_SZ, str2 + ZMM_SZ, ret);
        if (likely(ret))
        {
            cmp_idx = _tzcnt_u64(ret) + ZMM_SZ;
#ifdef STRNCMP
            if (cmp_idx >= size)
                return 0;
#endif
            return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
        }
#ifdef STRNCMP
        if (size <= 2 * ZMM_SZ)
            return 0;
#endif

        // Adjust the offset to align with cache line for the next load operation
        offset1 = (uintptr_t)str1 & (ZMM_SZ - 1);
        offset2 = (uintptr_t)str2 & (ZMM_SZ - 1);
        offset = (offset1 >= offset2) ? offset1 : offset2;
        /* Head verified [0, 2*ZMM_SZ); compute offset to the next ZMM-aligned
         * boundary for the more-offset pointer. May overlap the tail of the
         * head check when pointers are not ZMM-aligned, which is harmless. */
        offset = 2 * ZMM_SZ - offset;
    }

    // Alignement of both the strings is same
    if (unlikely(offset1 == offset2))
    {
#ifdef STRNCMP
        while (offset < (size - 4 * ZMM_SZ))
#else
        while (1)
#endif
        {
            // Load 64 bytes from each string and compare till we find diff/null
            z1 = _mm512_load_si512(str1 + offset);
            z2 = _mm512_load_si512(str2 + offset);
            //Check for NULL
            ret = _mm512_testn_epi8_mask(z1, z1) | _mm512_cmpneq_epu8_mask(z1,z2);
            if (!ret)
            {
                offset += ZMM_SZ;
                z3 = _mm512_load_si512(str1 + offset);
                z4 = _mm512_load_si512(str2 + offset);
                ret = _mm512_testn_epi8_mask(z3, z3) | _mm512_cmpneq_epu8_mask(z3, z4);
                if (!ret)
                {
                    offset += ZMM_SZ;
                    z1 = _mm512_load_si512(str1 + offset);
                    z2 = _mm512_load_si512(str2 + offset);
                    ret = _mm512_testn_epi8_mask(z1, z1) | _mm512_cmpneq_epu8_mask(z1,z2);
                    if (!ret)
                    {
                        offset += ZMM_SZ;
                        z3 = _mm512_load_si512(str1 + offset);
                        z4 = _mm512_load_si512(str2 + offset);
                        ret = _mm512_testn_epi8_mask(z3, z3) | _mm512_cmpneq_epu8_mask(z3, z4);
                    }
                }
            }
            if (ret)
                break;
            offset += ZMM_SZ;
#ifdef STRNCMP
            if (size <= offset)
                return 0;
#endif
        } //end of while for 4xVEC
        while (1)
        {
            z1 = _mm512_load_si512(str1 + offset);
            z2 = _mm512_load_si512(str2 + offset);
            //Check for NULL and mismatch
            ret = _mm512_testn_epi8_mask(z1, z1) | _mm512_cmpneq_epu8_mask(z1,z2);
            if (ret)
            {
                goto return_val;
            }
            offset += ZMM_SZ;
#ifdef STRNCMP
            if (size <= offset)
                return 0;
#endif
        } // end of while for 1xVEC
    }
    // Handle the case where alignments of the strings are different
    else
    {
        while (1)
        {
            // Check if either string is aligned at current offset
            char *aligned_str, *unaligned_str;

            if ((((uintptr_t)str1 + offset) & (ZMM_SZ - 1)) == 0)
            {
                aligned_str = (char *)str1;
                unaligned_str = (char *)str2;
            }
            else if ((((uintptr_t)str2 + offset) & (ZMM_SZ - 1)) == 0)
            {
                aligned_str = (char *)str2;
                unaligned_str = (char *)str1;
            }
            else
            {
                // Use unaligned loads for both strings
                z1 = _mm512_loadu_si512(str1 + offset);
                z2 = _mm512_loadu_si512(str2 + offset);
                ret = _mm512_testn_epi8_mask(z1, z1) | _mm512_cmpneq_epu8_mask(z1, z2);
                if (ret) {
                    goto return_val;
                }
                offset += ZMM_SZ;
#ifdef STRNCMP
                if (size <= offset)
                    return 0;
#endif
                continue; // Go back and check alignment again
            }
            // Calculate safe vectors based on page boundaries for BOTH strings
            uint16_t vecs_in_page  = PAGE_SAFE_VECS(unaligned_str + offset);

#ifdef STRNCMP
            while ((vecs_in_page >= 4) && (offset + 4 * ZMM_SZ <= size))
#else
            while (vecs_in_page >= 4)
#endif
            {
                z1 = _mm512_load_si512(aligned_str + offset);
                z2 = _mm512_loadu_si512(unaligned_str + offset);
                //Check for NULL
                ret = _mm512_testn_epi8_mask(z1, z1) | _mm512_cmpneq_epu8_mask(z1,z2);
                if (!ret)
                {
                    offset += ZMM_SZ;
                    z3 = _mm512_load_si512(aligned_str + offset);
                    z4 = _mm512_loadu_si512(unaligned_str + offset);
                    //Check for NULL
                    ret = _mm512_testn_epi8_mask(z3, z3) | _mm512_cmpneq_epu8_mask(z3, z4);
                    if (!ret)
                    {
                        offset += ZMM_SZ;
                        z1 = _mm512_load_si512(aligned_str + offset);
                        z2 = _mm512_loadu_si512(unaligned_str + offset);
                        //Check for NULL
                        ret = _mm512_testn_epi8_mask(z1, z1) | _mm512_cmpneq_epu8_mask(z1,z2);
                        if (!ret)
                        {
                            offset += ZMM_SZ;
                            z3 = _mm512_load_si512(aligned_str + offset);
                            z4 = _mm512_loadu_si512(unaligned_str + offset);
                            //Check for NULL
                            ret = _mm512_testn_epi8_mask(z3, z3) | _mm512_cmpneq_epu8_mask(z3, z4);
                            if (!ret)
                            {
                                offset += ZMM_SZ;
                                vecs_in_page -= 4;
                                continue;
                            }
                        }
                    }
                }
                goto return_val;
            } //end of while
#ifdef STRNCMP
            if (size <= offset)
                return 0;
#endif
            while (vecs_in_page--)
            {
                z1 = _mm512_load_si512(aligned_str + offset);
                z2 = _mm512_loadu_si512(unaligned_str + offset);
                //Check for NULL
                ret = _mm512_testn_epi8_mask(z1, z1) | _mm512_cmpneq_epu8_mask(z1,z2);
                if (ret)
                {
                    goto return_val;
                }
                offset += ZMM_SZ;
#ifdef STRNCMP
                if (size <= offset)
                    return 0;
#endif
            }
            // Handle the case where we are crossing a page boundary for the unaligned string
            mask = UINT64_MAX >> ((uintptr_t)(unaligned_str + offset)& (ZMM_SZ - 1));

            z1 = _mm512_maskz_loadu_epi8(mask, aligned_str + offset);
            z2 = _mm512_maskz_loadu_epi8(mask, unaligned_str + offset);

            ret = (_mm512_testn_epi8_mask(z1, z1) | _mm512_cmpneq_epu8_mask(z1,z2)) & mask;

            if (ret)
            {
                goto return_val;
            }
#ifdef STRNCMP
            if (size <= (offset + (ZMM_SZ - ((uintptr_t)(unaligned_str + offset)& (ZMM_SZ - 1)))))
                return 0;
#endif

            // Advance offset after processing page boundary crossing
            offset += ZMM_SZ - ((uintptr_t)(unaligned_str + offset) & (ZMM_SZ - 1));
        } //end of top level while
    }
return_val:
    cmp_idx = _tzcnt_u64(ret) + offset;
#ifdef STRNCMP
    if (cmp_idx >= size)
        return 0;
#endif
    return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
}
