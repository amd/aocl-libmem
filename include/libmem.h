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

#define CONCAT2(x,y)    x##_##y
#define CONCAT(x,y)     CONCAT2(x,y)

#define PREFIX(x)       __##x

typedef enum{
    MEMCPY,
    MEMPCPY,
    MEMMOVE,
    MEMSET,
    MEMCMP,
    FUNC_COUNT
}func_index;

// A maximum of 16 supported variants
typedef enum{
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
/*User Threshold with avx2, erms, non-temporal*/
    THRESHOLD,
/*uArch based*/
    ARCH_ZEN1,
    ARCH_ZEN2,
    ARCH_ZEN3,
    ARCH_ZEN4,
    ARCH_ZEN5,
/*System feature & Threshold*/
    SYSTEM,
    VARIANT_COUNT
}variant_index;

typedef void (*func_ptr)(void);

#define generate_avx2_variants(func) \
    (func_ptr) CONCAT(PREFIX(func), avx2_unaligned), \
    (func_ptr) CONCAT(PREFIX(func), avx2_aligned), \
    (func_ptr) CONCAT(PREFIX(func), avx2_aligned_load), \
    (func_ptr) CONCAT(PREFIX(func), avx2_aligned_store), \
    (func_ptr) CONCAT(PREFIX(func), avx2_nt), \
    (func_ptr) CONCAT(PREFIX(func), avx2_nt_load), \
    (func_ptr) CONCAT(PREFIX(func), avx2_nt_store),


#ifdef AVX512_FEATURE_ENABLED
#define generate_avx512_variants(func) \
    (func_ptr) CONCAT(PREFIX(func), avx512_unaligned), \
    (func_ptr) CONCAT(PREFIX(func), avx512_aligned), \
    (func_ptr) CONCAT(PREFIX(func), avx512_aligned_load), \
    (func_ptr) CONCAT(PREFIX(func), avx512_aligned_store), \
    (func_ptr) CONCAT(PREFIX(func), avx512_nt), \
    (func_ptr) CONCAT(PREFIX(func), avx512_nt_load), \
    (func_ptr) CONCAT(PREFIX(func), avx512_nt_store),
#else
#define generate_avx512_variants(func)
#endif //end of avx-512 variants

#define generate_erms_variants(func) \
    (func_ptr) CONCAT(PREFIX(func), erms_b_aligned), \
    (func_ptr) CONCAT(PREFIX(func), erms_w_aligned), \
    (func_ptr) CONCAT(PREFIX(func), erms_d_aligned), \
    (func_ptr) CONCAT(PREFIX(func), erms_q_aligned),

#define generate_threshold_variant(func) \
    (func_ptr) CONCAT(PREFIX(func), threshold),

#define generate_arch_variants(func) \
    (func_ptr) CONCAT(PREFIX(func), zen1), \
    (func_ptr) CONCAT(PREFIX(func), zen2), \
    (func_ptr) CONCAT(PREFIX(func), zen3), \
    (func_ptr) CONCAT(PREFIX(func), zen4), \
    (func_ptr) CONCAT(PREFIX(func), zen5),

#define generate_system_variant(func) \
    (func_ptr) CONCAT(PREFIX(func), system)

#define add_func_variants(func) \
    { \
        generate_avx2_variants(func) \
        generate_avx512_variants(func) \
        generate_erms_variants(func) \
        generate_threshold_variant(func) \
        generate_arch_variants(func) \
        generate_system_variant(func) \
    }

func_ptr libmem_impls[FUNC_COUNT][VARIANT_COUNT] =
{
    add_func_variants(memcpy),
    add_func_variants(mempcpy),
    add_func_variants(memmove),
    add_func_variants(memset),
    add_func_variants(memcmp)
};

#ifdef __cplusplus
}
#endif

#endif
