/* Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
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
 #include "zen_cpu_info.h"
 
 /* Zen4-optimized strcmp/strncmp using AVX-512. */
 
 #define STRCMP_BLK_SZ AVX2_VEC_4_SZ
 
 #define STRCMP_PAGE_LIMIT(p)                                                                                           \
     ((int32_t) (((uint32_t) (PAGE_SZ - STRCMP_BLK_SZ) - ((uint32_t) (uintptr_t) (p) & (PAGE_SZ - 1))) & (PAGE_SZ - 1)))
 
 #ifdef STRNCMP
 #define STRCMP_CLAMP_LIMIT()                                                                                           \
     do                                                                                                                 \
     {                                                                                                                  \
         size_t rem_sz = (size_t) (end - (uintptr_t) s1) - 1;                                                           \
         if (rem_sz < (size_t) limit)                                                                                   \
             limit = (int32_t) rem_sz;                                                                                  \
     } while (0)
 #else
 #define STRCMP_CLAMP_LIMIT() ((void) 0)
 #endif
 #ifdef STRNCMP
 static inline int __attribute__((flatten)) _strncmp_zen4(const char *str1, const char *str2, size_t size)
 #else
 HIDDEN_SYMBOL int __attribute__((flatten)) __strcmp_zen4(const char *str1, const char *str2)
 #endif
 {
     const char *s1, *s2;
     size_t offset, index, rem_data, vec_len, page_off, page_off1, page_off2;
     uint64_t cmp_idx, ret;
     uint32_t yret;
     int32_t limit;
     __m256i y0, y1, y2, y3, y4, y5, y6, y7, y8, y9;
     __m512i z0, z1, z2, z3, z4, z5;
     __mmask32 match, match1, match2, match3, match4;
     __mmask64 mask, mask1;
 #ifdef STRNCMP
     uintptr_t end;
 #endif
 
     LOG_INFO("\n");
 #ifdef STRNCMP
     if (unlikely(size == 0))
         return 0;
 #endif
 
 restart:
     if (unlikely((PAGE_SZ - STRCMP_BLK_SZ) < (((uintptr_t) str1 | (uintptr_t) str2) & (PAGE_SZ - 1))))
     {
         page_off1 = (uintptr_t) str1 & (PAGE_SZ - 1);
         page_off2 = (uintptr_t) str2 & (PAGE_SZ - 1);
         page_off = (page_off1 >= page_off2) ? page_off1 : page_off2;
 
         if (page_off > PAGE_SZ - STRCMP_BLK_SZ)
             goto page_cross_head;
     }
 
     y0 = _mm256_loadu_si256((const __m256i *) str1);
     match = _mm256_test_epi8_mask(y0, y0);
     match1 = _mm256_mask_cmpeq_epu8_mask(match, y0, _mm256_loadu_si256((const __m256i *) str2));
     yret = (uint32_t) match1 + 1;
     if (unlikely(yret != 0))
     {
         cmp_idx = _tzcnt_u32(yret);
 #ifdef STRNCMP
         if (unlikely(cmp_idx >= size))
             return 0;
 #endif
         return (unsigned char) str1[cmp_idx] - (unsigned char) str2[cmp_idx];
     }
 
     y0 = _mm256_loadu_si256((const __m256i *) (str1 + YMM_SZ));
     match = _mm256_test_epi8_mask(y0, y0);
     match1 = _mm256_mask_cmpeq_epu8_mask(match, y0, _mm256_loadu_si256((const __m256i *) (str2 + YMM_SZ)));
     yret = (uint32_t) match1 + 1;
     if (unlikely(yret != 0))
     {
         cmp_idx = _tzcnt_u32(yret) + (size_t) YMM_SZ;
 #ifdef STRNCMP
         if (unlikely(cmp_idx >= size))
             return 0;
 #endif
         return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
     }
 
     y0 = _mm256_loadu_si256((const __m256i *) (str1 + 2 * YMM_SZ));
     match = _mm256_test_epi8_mask(y0, y0);
     match1 = _mm256_mask_cmpeq_epu8_mask(match, y0, _mm256_loadu_si256((const __m256i *) (str2 + 2 * YMM_SZ)));
     yret = (uint32_t) match1 + 1;
     if (unlikely(yret != 0))
     {
         cmp_idx = _tzcnt_u32(yret) + (size_t) (2 * YMM_SZ);
 #ifdef STRNCMP
         if (unlikely(cmp_idx >= size))
             return 0;
 #endif
         return (unsigned char)str1[cmp_idx] - (unsigned char)str2[cmp_idx];
     }
 
     y0 = _mm256_loadu_si256((const __m256i *) (str1 + 3 * YMM_SZ));
     match = _mm256_test_epi8_mask(y0, y0);
     match1 = _mm256_mask_cmpeq_epu8_mask(match, y0, _mm256_loadu_si256((const __m256i *) (str2 + 3 * YMM_SZ)));
     yret = (uint32_t) match1 + 1;
     if (likely(yret != 0))
     {
         cmp_idx = _tzcnt_u32(yret) + (size_t) (3 * YMM_SZ);
 #ifdef STRNCMP
         if (unlikely(cmp_idx >= size))
             return 0;
 #endif
         return (unsigned char) str1[cmp_idx] - (unsigned char) str2[cmp_idx];
     }
 
 #ifdef STRNCMP
     if (unlikely(size <= STRCMP_BLK_SZ))
         return 0;
     end = (uintptr_t) str1 + size;
 #endif
 
     s1 = str1 + STRCMP_BLK_SZ;
     s2 = str2 + STRCMP_BLK_SZ;
     page_off1 = (uintptr_t) str1 & (PAGE_SZ - 1);
     page_off2 = (uintptr_t) str2 & (PAGE_SZ - 1);
     page_off = (page_off1 >= page_off2) ? page_off1 : page_off2;
 
     if (likely(page_off <= PAGE_SZ - 2 * STRCMP_BLK_SZ))
     {
         z0 = _mm512_loadu_si512((const void *) s1);
         z1 = _mm512_loadu_si512((const void *) (s1 + ZMM_SZ));
 
         z2 = _mm512_xor_si512(z0, _mm512_loadu_si512((const void *) s2));
         z3 = _mm512_xor_si512(z1, _mm512_loadu_si512((const void *) (s2 + ZMM_SZ)));
 
         z4 = _mm512_min_epu8(z0, z1);
         z5 = _mm512_or_si512(z2, z3);
         mask = _mm512_testn_epi8_mask(z4, z4);
         mask1 = _mm512_test_epi8_mask(z5, z5);
         if (unlikely(!_kortestz_mask64_u8(mask, mask1)))
         {
             ret = _cvtmask64_u64(_kor_mask64(mask, mask1));
             offset = ZMM_SZ;
 
             mask = _kor_mask64(_mm512_testn_epi8_mask(z0, z0), _mm512_test_epi8_mask(z2, z2));
             if (!_ktestz_mask64_u8(mask, mask))
             {
                 offset = 0;
                 ret = _cvtmask64_u64(mask);
             }
             goto return_val;
         }
         str1 = s1;
         str2 = s2;
     }
 
 #ifdef STRNCMP
     if (unlikely(end < (uintptr_t) str1))
         end = UINTPTR_MAX;
 #endif
 
     s1 = (const char *)((uintptr_t)str1 & ~(uintptr_t)(STRCMP_BLK_SZ - 1));
     s2 = str2 + (s1 - str1);
     limit = STRCMP_PAGE_LIMIT(s2);
     STRCMP_CLAMP_LIMIT();
 
     while (1)
     {
         s1 += STRCMP_BLK_SZ;
         s2 += STRCMP_BLK_SZ;
         limit -= STRCMP_BLK_SZ;
         if (unlikely(limit < 0))
         {
 #ifdef STRNCMP
             if ((uintptr_t) s1 >= end)
                 return 0;
 #endif
             if (limit != -STRCMP_BLK_SZ)
             {
                 rem_data = PAGE_SAFE_VECS_BYTES(s2);
                 index = 0;
 
                 do
                 {
                     vec_len = rem_data - index;
                     if (vec_len > ZMM_SZ)
                         vec_len = ZMM_SZ;
 
                     mask = _cvtu64_mask64(_bzhi_u64(UINT64_MAX, vec_len));
                     z0 = _mm512_maskz_loadu_epi8(mask, s1 + index);
                     z1 = _mm512_maskz_loadu_epi8(mask, s2 + index);
                     mask = _kor_mask64(_mm512_mask_testn_epi8_mask(mask, z0, z0), _mm512_cmpneq_epu8_mask(z0, z1));
                     if (!_ktestz_mask64_u8(mask, mask))
                     {
                         offset = index;
                         ret = _cvtmask64_u64(mask);
                         goto return_val;
                     }
                     index += vec_len;
                 } while (index < rem_data);
 #ifdef STRNCMP
                 if ((uintptr_t) s1 + rem_data >= end)
                     return 0;
 #endif
             }
             limit = STRCMP_PAGE_LIMIT(s2);
             STRCMP_CLAMP_LIMIT();
         }
 
         y0 = _mm256_load_si256((const __m256i *) s1);
         y1 = _mm256_load_si256((const __m256i *) (s1 + YMM_SZ));
         y2 = _mm256_load_si256((const __m256i *)(s1 + 2 * YMM_SZ));
         y3 = _mm256_load_si256((const __m256i *)(s1 + 3 * YMM_SZ));
 
         y4 = _mm256_and_si256(y0, _mm256_cmpeq_epi8(y0, _mm256_loadu_si256((const __m256i *) s2)));
         y5 = _mm256_and_si256(y1, _mm256_cmpeq_epi8(y1, _mm256_loadu_si256((const __m256i *) (s2 + YMM_SZ))));
         y6 = _mm256_and_si256(y2, _mm256_cmpeq_epi8(y2, _mm256_loadu_si256((const __m256i *) (s2 + 2 * YMM_SZ))));
         y7 = _mm256_and_si256(y3, _mm256_cmpeq_epi8(y3, _mm256_loadu_si256((const __m256i *) (s2 + 3 * YMM_SZ))));
 
         y8 = _mm256_min_epu8(y4, y5);
         y9 = _mm256_min_epu8(y8, _mm256_min_epu8(y6, y7));
 
         match = _mm256_testn_epi8_mask(y9, y9);
         if (unlikely(!_ktestz_mask32_u8(match, match)))
             break;
     }
 
     /* Find which of the four vectors stopped the compare: half first, then pair. */
     match2 = _mm256_testn_epi8_mask(y8, y8);
     match3 = _mm256_testn_epi8_mask(y4, y4);
     match4 = _mm256_testn_epi8_mask(y6, y6);
 
     if (!_ktestz_mask32_u8(match2, match2))
     {
         ret = _ktestz_mask32_u8(match3, match3) ? (uint64_t) match2 : (uint64_t) match3;
         offset = _ktestz_mask32_u8(match3, match3) ? YMM_SZ : 0;
     }
     else
     {
         ret = _ktestz_mask32_u8(match4, match4) ? (uint64_t) match : (uint64_t) match4;
         offset = _ktestz_mask32_u8(match4, match4) ? 3 * YMM_SZ : 2 * YMM_SZ;
     }
 
 return_val:
     cmp_idx = _tzcnt_u64(ret) + offset;
 #ifdef STRNCMP
     if (unlikely(cmp_idx >= (size_t) (end - (uintptr_t) s1)))
         return 0;
 #endif
     return (unsigned char) s1[cmp_idx] - (unsigned char) s2[cmp_idx];
 
 page_cross_head:
     vec_len = PAGE_SAFE_VECS_BYTES(str1);
     rem_data = PAGE_SAFE_VECS_BYTES(str2);
     if (vec_len > rem_data)
         vec_len = rem_data;
     if (vec_len > ZMM_SZ)
         vec_len = ZMM_SZ;
 
     mask = _cvtu64_mask64(_bzhi_u64(UINT64_MAX, vec_len));
     z0 = _mm512_maskz_loadu_epi8(mask, str1);
     z1 = _mm512_maskz_loadu_epi8(mask, str2);
     mask = _kor_mask64(_mm512_mask_testn_epi8_mask(mask, z0, z0), _mm512_cmpneq_epu8_mask(z0, z1));
     if (!_ktestz_mask64_u8(mask, mask))
     {
         cmp_idx = _tzcnt_u64(_cvtmask64_u64(mask));
 #ifdef STRNCMP
         if (unlikely(cmp_idx >= size))
             return 0;
 #endif
         return (unsigned char) str1[cmp_idx] - (unsigned char) str2[cmp_idx];
     }
 #ifdef STRNCMP
     if (size <= vec_len)
         return 0;
     size -= vec_len;
 #endif
     str1 += vec_len;
     str2 += vec_len;
     goto restart;
 }
 
 #ifndef STRNCMP
 #ifndef ALMEM_DYN_DISPATCH
 int strcmp(const char *, const char *) __attribute__((weak, alias("__strcmp_zen4")));
 #endif
 #endif
 