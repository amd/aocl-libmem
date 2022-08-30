/* Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
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
#ifndef _MEMCMP_H_
#define _MEMCMP_H_
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//Micro architecture specifc implementations.
extern int __memcmp_zen1(const void *mem1,const void *mem2, size_t size);
extern int __memcmp_zen2(const void *mem1,const void *mem2, size_t size);
extern int __memcmp_zen3(const void *mem1,const void *mem2, size_t size);
extern int __memcmp_zen4(const void *mem1,const void *mem2, size_t size);

//System solution which takes in system config and  threshold values.
extern int __memcmp_system(const void *mem1,const void *mem2, size_t size);

//Generic solution which takes in user threshold values.
extern int __memcmp_threshold(const void *mem1,const void *mem2, size_t size);

//CPU Feature:AVX2 and Alignment specifc implementations.
extern int __memcmp_avx2_unaligned(const void *mem1,const void *mem2, size_t size);
extern int __memcmp_avx2_aligned(const void *mem1,const void *mem2, size_t size);
extern int __memcmp_avx2_nt(const void *mem1,const void *mem2, size_t size);

#ifdef AVX512_FEATURE_ENABLED
//CPU Feature:AVX512 and Alignment specifc implementations.
extern int __memcmp_avx512_unaligned(const void *mem1,const void *mem2, size_t size);
extern int __memcmp_avx512_aligned(const void *mem1,const void *mem2, size_t size);
extern int __memcmp_avx512_nt(const void *mem1,const void *mem2, size_t size);
#endif

//CPU Feature:ERMS and Alignment specifc implementations.
extern int __memcmp_erms_b_aligned(const void *mem1,const void *mem2, size_t size);
extern int __memcmp_erms_w_aligned(const void *mem1,const void *mem2, size_t size);
extern int __memcmp_erms_d_aligned(const void *mem1,const void *mem2, size_t size);
extern int __memcmp_erms_q_aligned(const void *mem1,const void *mem2, size_t size);

extern int (*_memcmp_variant)(const void *, const void *, size_t);


#ifdef __cplusplus
}
#endif

#endif
