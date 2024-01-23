/* Copyright (C) 2022-24 Advanced Micro Devices, Inc. All rights reserved.
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
#ifndef _LIBMEM_H_
#define _LIBMEM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "libmem_impls.h"
typedef enum{
    MEMCPY = 0,
    MEMPCPY = 1,
    MEMMOVE = 2,
    MEMSET = 0,
    MEMCMP = 0
}func_index;

// A maximum of 16 supported variants
typedef enum{
/*System feature & Threshold*/
    SYSTEM,
/*User Threshold with avx2, erms, non-temporal*/
    THRESHOLD,
/*User AVX2 operation based*/
    AVX2_UNALIGNED,
    AVX2_ALIGNED,
    AVX2_ALIGNED_LOAD,
    AVX2_ALIGNED_STORE,
    AVX2_NON_TEMPORAL,
    AVX2_NON_TEMPORAL_LOAD,
    AVX2_NON_TEMPORAL_STORE,
#ifdef AVX512_FEATURE_ENABLED
/*User AVX512 operation based*/
    AVX512_UNALIGNED,
    AVX512_ALIGNED,
    AVX512_ALIGNED_LOAD,
    AVX512_ALIGNED_STORE,
    AVX512_NON_TEMPORAL,
    AVX512_NON_TEMPORAL_LOAD,
    AVX512_NON_TEMPORAL_STORE,
#endif
/*User ERMS operation based*/
    ERMS_MOVSB,
    ERMS_MOVSW,
    ERMS_MOVSD,
    ERMS_MOVSQ,
/*uArch based*/
    ARC_ZEN1,
    ARC_ZEN2,
    ARC_ZEN3,
    ARC_ZEN4,
    ARC_ZEN5,
    VAR_COUNT
}variant_index;

void * (*libmem_impls_1[][VAR_COUNT])(void *, const void *, size_t)=
{
    {
        __memcpy_system,
        __memcpy_threshold,
        __memcpy_avx2_unaligned,
        __memcpy_avx2_aligned,
        __memcpy_avx2_aligned_load,
        __memcpy_avx2_aligned_store,
        __memcpy_avx2_nt,
        __memcpy_avx2_nt_load,
        __memcpy_avx2_nt_store,
#ifdef AVX512_FEATURE_ENABLED
        __memcpy_avx512_unaligned,
        __memcpy_avx512_aligned,
        __memcpy_avx512_aligned_load,
        __memcpy_avx512_aligned_store,
        __memcpy_avx512_nt,
        __memcpy_avx512_nt_load,
        __memcpy_avx512_nt_store,
#endif
        __memcpy_erms_b_aligned,
        __memcpy_erms_w_aligned,
        __memcpy_erms_d_aligned,
        __memcpy_erms_q_aligned,
        __memcpy_zen1,
        __memcpy_zen2,
        __memcpy_zen3,
        __memcpy_zen4,
        __memcpy_zen5
    },
    {
        __mempcpy_system,
        __mempcpy_threshold,
        __mempcpy_avx2_unaligned,
        __mempcpy_avx2_aligned,
        __mempcpy_avx2_aligned_load,
        __mempcpy_avx2_aligned_store,
        __mempcpy_avx2_nt,
        __mempcpy_avx2_nt_load,
        __mempcpy_avx2_nt_store,
#ifdef AVX512_FEATURE_ENABLED
        __mempcpy_avx512_unaligned,
        __mempcpy_avx512_aligned,
        __mempcpy_avx512_aligned_load,
        __mempcpy_avx512_aligned_store,
        __mempcpy_avx512_nt,
        __mempcpy_avx512_nt_load,
        __mempcpy_avx512_nt_store,
#endif
        __mempcpy_erms_b_aligned,
        __mempcpy_erms_w_aligned,
        __mempcpy_erms_d_aligned,
        __mempcpy_erms_q_aligned,
        __mempcpy_zen1,
        __mempcpy_zen2,
        __mempcpy_zen3,
        __mempcpy_zen4,
        __mempcpy_zen5
    },
    {
        __memmove_system,
        __memmove_threshold,
        __memmove_avx2_unaligned,
        __memmove_avx2_aligned,
        __memmove_avx2_aligned_load,
        __memmove_avx2_aligned_store,
        __memmove_avx2_nt,
        __memmove_avx2_nt_load,
        __memmove_avx2_nt_store,
#ifdef AVX512_FEATURE_ENABLED
        __memmove_avx512_unaligned,
        __memmove_avx512_aligned,
        __memmove_avx512_aligned_load,
        __memmove_avx512_aligned_store,
        __memmove_avx512_nt,
        __memmove_avx512_nt_load,
        __memmove_avx512_nt_store,
#endif
        __memmove_erms_b_aligned,
        __memmove_erms_w_aligned,
        __memmove_erms_d_aligned,
        __memmove_erms_q_aligned,
        __memmove_zen1,
        __memmove_zen2,
        __memmove_zen3,
        __memmove_zen4,
        __memmove_zen5
    }
};

void * (*libmem_impls_2[][VAR_COUNT])(void *, int, size_t)=
{
    {
        __memset_system,
        __memset_threshold,
        __memset_avx2_unaligned,
        __memset_avx2_aligned,
        __memset_avx2_unaligned,
        __memset_avx2_aligned,
        __memset_avx2_nt,
        __memset_avx2_unaligned,
        __memset_avx2_nt,
#ifdef AVX512_FEATURE_ENABLED
        __memset_avx512_unaligned,
        __memset_avx512_aligned,
        __memset_avx512_unaligned,
        __memset_avx512_aligned,
        __memset_avx512_nt,
        __memset_avx512_unaligned,
        __memset_avx512_nt,
#endif
        __memset_erms_b_aligned,
        __memset_erms_w_aligned,
        __memset_erms_d_aligned,
        __memset_erms_q_aligned,
        __memset_zen1,
        __memset_zen2,
        __memset_zen3,
        __memset_zen4,
        __memset_zen5
    }
};

int (*libmem_impls_3[][VAR_COUNT])(const void *, const void *, size_t)=
{
    {
        __memcmp_system,
        __memcmp_threshold,
        __memcmp_avx2_unaligned,
        __memcmp_avx2_aligned,
        __memcmp_avx2_unaligned,
        __memcmp_avx2_unaligned,
        __memcmp_avx2_nt,
        __memcmp_avx2_unaligned,
        __memcmp_avx2_unaligned,
#ifdef AVX512_FEATURE_ENABLED
        __memcmp_avx512_unaligned,
        __memcmp_avx512_aligned,
        __memcmp_avx512_unaligned,
        __memcmp_avx512_unaligned,
        __memcmp_avx512_nt,
        __memcmp_avx512_unaligned,
        __memcmp_avx512_unaligned,
#endif
        __memcmp_erms_b_aligned,
        __memcmp_erms_w_aligned,
        __memcmp_erms_d_aligned,
        __memcmp_erms_q_aligned,
        __memcmp_zen1,
        __memcmp_zen2,
        __memcmp_zen3,
        __memcmp_zen4,
        __memcmp_zen5
    }
};


#ifdef __cplusplus
}
#endif

#endif

