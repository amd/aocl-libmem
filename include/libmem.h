/* Copyright (C) 2022-26 Advanced Micro Devices, Inc. All rights reserved.
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

#include "almem_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Single source of truth for the supported functions.
 *
 * The build enables a function by defining ALMEM_FUNC_<NAME>=1 (see
 * ALMEM_FUNCTIONS). ALMEM_IF_ENABLED(FN, ...) expands to its trailing arguments
 * when ALMEM_FUNC_<FN> is set, else to nothing - the one place the per-function
 * selection is tested (IS_ENABLED()-style, so no per-function #ifdef needed).
 *
 * ALMEM_FOREACH_CPU_FUNC(X) / ALMEM_FOREACH_TUN_FUNC(X) invoke X(fn, FN) for
 * each selected function, where 'fn' is the lower-case name and 'FN' its
 * func_index enumerator. */

/* IS_ENABLED()-style test: expands to 1 when ALMEM_FUNC_<FN> is defined to 1,
 * else 0. Relies on the concatenation with the placeholder yielding an extra
 * leading argument ("0,") only for the enabled ("1") case. */
#define _ALMEM_ARG_PLACEHOLDER_1 0,
#define _ALMEM_TAKE_SECOND(a, b, ...) b
#define _ALMEM_IS_DEF(val) _ALMEM_IS_DEF_(_ALMEM_ARG_PLACEHOLDER_##val)
#define _ALMEM_IS_DEF_(arg) _ALMEM_TAKE_SECOND(arg 1, 0)
#define _ALMEM_EXPAND_DEF(cfg) _ALMEM_IS_DEF(cfg)
#define ALMEM_FUNC_ENABLED(FN) _ALMEM_EXPAND_DEF(ALMEM_FUNC_##FN)

/* Emit the trailing args when FN is enabled. Variadic so the payload may itself
 * contain commas (e.g. a table-initialiser row). */
#define _ALMEM_CAT(a, b) _ALMEM_CAT_(a, b)
#define _ALMEM_CAT_(a, b) a##b
#define _ALMEM_SEL_1(...) __VA_ARGS__
#define _ALMEM_SEL_0(...)
#define ALMEM_IF_ENABLED(FN, ...) _ALMEM_CAT(_ALMEM_SEL_, ALMEM_FUNC_ENABLED(FN))(__VA_ARGS__)

/* One list entry: invoke X(fn, FN) iff the function is enabled. */
#define _ALMEM_ENTRY(fn, FN, X) ALMEM_IF_ENABLED(FN, X(fn, FN))

#define ALMEM_FOREACH_CPU_FUNC(X)                                                                                      \
    _ALMEM_ENTRY(memcpy, MEMCPY, X)                                                                                    \
    _ALMEM_ENTRY(mempcpy, MEMPCPY, X)                                                                                  \
    _ALMEM_ENTRY(memmove, MEMMOVE, X)                                                                                  \
    _ALMEM_ENTRY(memset, MEMSET, X)                                                                                    \
    _ALMEM_ENTRY(memcmp, MEMCMP, X)                                                                                    \
    _ALMEM_ENTRY(memchr, MEMCHR, X)                                                                                    \
    _ALMEM_ENTRY(strcpy, STRCPY, X)                                                                                    \
    _ALMEM_ENTRY(strncpy, STRNCPY, X)                                                                                  \
    _ALMEM_ENTRY(strcmp, STRCMP, X)                                                                                    \
    _ALMEM_ENTRY(strncmp, STRNCMP, X)                                                                                  \
    _ALMEM_ENTRY(strcat, STRCAT, X)                                                                                    \
    _ALMEM_ENTRY(strncat, STRNCAT, X)                                                                                  \
    _ALMEM_ENTRY(strstr, STRSTR, X)                                                                                    \
    _ALMEM_ENTRY(strlen, STRLEN, X)                                                                                    \
    _ALMEM_ENTRY(strnlen, STRNLEN, X)                                                                                  \
    _ALMEM_ENTRY(strchr, STRCHR, X)                                                                                    \
    _ALMEM_ENTRY(strspn, STRSPN, X)                                                                                    \
    _ALMEM_ENTRY(strrchr, STRRCHR, X)

/* Tunable-capable subset (memcpy..memcmp), in func_index order. */
#define ALMEM_FOREACH_TUN_FUNC(X)                                                                                      \
    _ALMEM_ENTRY(memcpy, MEMCPY, X)                                                                                    \
    _ALMEM_ENTRY(mempcpy, MEMPCPY, X)                                                                                  \
    _ALMEM_ENTRY(memmove, MEMMOVE, X)                                                                                  \
    _ALMEM_ENTRY(memset, MEMSET, X)                                                                                    \
    _ALMEM_ENTRY(memcmp, MEMCMP, X)

/* func_index enumerates only the selected functions, numbered contiguously.
 * The dispatch tables are generated from the same ALMEM_FOREACH_* lists, so the
 * enumerators and table rows stay index-aligned without placeholder rows. */
typedef enum {
#define _ALMEM_FN_ENUM(fn, FN) FN,
    ALMEM_FOREACH_CPU_FUNC(_ALMEM_FN_ENUM)
#undef _ALMEM_FN_ENUM
        FUNC_COUNT
} func_index;

/* Count of selected tunable-capable functions (replaces the old MEMCMP + 1,
 * which broke when memcmp was de-selected). Expands to a pure integer constant
 * expression, e.g. (0 + 1 + 1), usable as an array bound and in #if. */
#define _ALMEM_PLUS1(fn, FN) +1
#define TUN_FUNC_COUNT (0 ALMEM_FOREACH_TUN_FUNC(_ALMEM_PLUS1))

#ifdef ALMEM_TUNABLES
typedef enum{
/*User AVX2 operation based*/
    AVX2_UNALIGNED,
    AVX2_ALIGNED,
    AVX2_ALIGNED_LOAD,
    AVX2_ALIGNED_STORE,
    AVX2_NON_TEMPORAL,
    AVX2_NON_TEMPORAL_LOAD,
    AVX2_NON_TEMPORAL_STORE,
/*User AVX512 operation based*/
    AVX512_UNALIGNED,
    AVX512_ALIGNED,
    AVX512_ALIGNED_LOAD,
    AVX512_ALIGNED_STORE,
    AVX512_NON_TEMPORAL,
    AVX512_NON_TEMPORAL_LOAD,
    AVX512_NON_TEMPORAL_STORE,
/*User ERMS operation based*/
    ERMS_MOVSB,
    ERMS_MOVSW,
    ERMS_MOVSD,
    ERMS_MOVSQ,
/*User Threshold with vec-temporal, erms, vec-non-temporal*/
    THRESHOLD,
    TUN_VARIANT_COUNT,
    UNKNOWN
} tunable_variant_idx;
#endif //end of tunables

typedef enum {
    /*uArch based*/
    ARCH_ZEN1,
    ARCH_ZEN2,
    ARCH_ZEN3,
    ARCH_ZEN4,
    ARCH_ZEN5,
    ARCH_ZEN6,
    /*System feature & Threshold*/
    SYSTEM,
    CPU_VARIANT_COUNT
} cpu_variant_idx;

typedef void (*func_ptr)(void);

#ifdef ALMEM_TUNABLES
#define generate_avx2_variants(func) \
    (func_ptr) __ALMEM_CONCAT(__ALMEM_PREFIX(func), avx2_unaligned), \
    (func_ptr) __ALMEM_CONCAT(__ALMEM_PREFIX(func), avx2_aligned), \
    (func_ptr) __ALMEM_CONCAT(__ALMEM_PREFIX(func), avx2_aligned_load), \
    (func_ptr) __ALMEM_CONCAT(__ALMEM_PREFIX(func), avx2_aligned_store), \
    (func_ptr) __ALMEM_CONCAT(__ALMEM_PREFIX(func), avx2_nt), \
    (func_ptr) __ALMEM_CONCAT(__ALMEM_PREFIX(func), avx2_nt_load), \
    (func_ptr) __ALMEM_CONCAT(__ALMEM_PREFIX(func), avx2_nt_store),


#define generate_avx512_variants(func) \
    (func_ptr) __ALMEM_CONCAT(__ALMEM_PREFIX(func), avx512_unaligned), \
    (func_ptr) __ALMEM_CONCAT(__ALMEM_PREFIX(func), avx512_aligned), \
    (func_ptr) __ALMEM_CONCAT(__ALMEM_PREFIX(func), avx512_aligned_load), \
    (func_ptr) __ALMEM_CONCAT(__ALMEM_PREFIX(func), avx512_aligned_store), \
    (func_ptr) __ALMEM_CONCAT(__ALMEM_PREFIX(func), avx512_nt), \
    (func_ptr) __ALMEM_CONCAT(__ALMEM_PREFIX(func), avx512_nt_load), \
    (func_ptr) __ALMEM_CONCAT(__ALMEM_PREFIX(func), avx512_nt_store),

#define generate_erms_variants(func) \
    (func_ptr) __ALMEM_CONCAT(__ALMEM_PREFIX(func), erms_b_aligned), \
    (func_ptr) __ALMEM_CONCAT(__ALMEM_PREFIX(func), erms_w_aligned), \
    (func_ptr) __ALMEM_CONCAT(__ALMEM_PREFIX(func), erms_d_aligned), \
    (func_ptr) __ALMEM_CONCAT(__ALMEM_PREFIX(func), erms_q_aligned),

#define generate_threshold_variant(func) \
    (func_ptr) __ALMEM_CONCAT(__ALMEM_PREFIX(func), threshold),

#define add_tun_func_variants(func) \
    { \
        generate_avx2_variants(func) \
        generate_avx512_variants(func) \
        generate_erms_variants(func) \
        generate_threshold_variant(func) \
    }

// Tunable Dispatching Table: one row per selected tunable-capable function.
#if TUN_FUNC_COUNT > 0
HIDDEN_SYMBOL func_ptr libmem_tun_impls[TUN_FUNC_COUNT][TUN_VARIANT_COUNT] = {
#define _ALMEM_TUN_ROW(fn, FN) add_tun_func_variants(fn),
    ALMEM_FOREACH_TUN_FUNC(_ALMEM_TUN_ROW)
#undef _ALMEM_TUN_ROW
};
#endif

#endif //end of tunable variants

/* A Zen arch object is only linked when its ALMEM_ZEN<N>_ENABLED macro is
 * defined by the build (see ALMEM_DISPATCH_ARCH). Per arch, select the target
 * suffix: the real zen<N> when built, else the always-present system impl (so
 * the table never references an undefined __<func>_zen<N>). The #ifdefs are
 * unavoidable - the preprocessor cannot derive an #ifdef from a pasted token -
 * but the dispatch-slot expression itself is defined only once below. */
#ifdef ALMEM_ZEN1_ENABLED
#define _ALMEM_ARCH_TGT_zen1 zen1
#else
#define _ALMEM_ARCH_TGT_zen1 system
#endif
#ifdef ALMEM_ZEN2_ENABLED
#define _ALMEM_ARCH_TGT_zen2 zen2
#else
#define _ALMEM_ARCH_TGT_zen2 system
#endif
#ifdef ALMEM_ZEN3_ENABLED
#define _ALMEM_ARCH_TGT_zen3 zen3
#else
#define _ALMEM_ARCH_TGT_zen3 system
#endif
#ifdef ALMEM_ZEN4_ENABLED
#define _ALMEM_ARCH_TGT_zen4 zen4
#else
#define _ALMEM_ARCH_TGT_zen4 system
#endif
#ifdef ALMEM_ZEN5_ENABLED
#define _ALMEM_ARCH_TGT_zen5 zen5
#else
#define _ALMEM_ARCH_TGT_zen5 system
#endif
#ifdef ALMEM_ZEN6_ENABLED
#define _ALMEM_ARCH_TGT_zen6 zen6
#else
#define _ALMEM_ARCH_TGT_zen6 system
#endif

/* Single dispatch-slot definition: builds __<func>_<resolved target> where the
 * target is _ALMEM_ARCH_TGT_<arch> (zen<N> or system). */
#define _ALMEM_ARCH_SLOT(func, arch) (func_ptr) __ALMEM_CONCAT(__ALMEM_PREFIX(func), _ALMEM_ARCH_TGT_##arch)

#define generate_arch_variants(func)                                                                                   \
    _ALMEM_ARCH_SLOT(func, zen1), _ALMEM_ARCH_SLOT(func, zen2), _ALMEM_ARCH_SLOT(func, zen3),                          \
        _ALMEM_ARCH_SLOT(func, zen4), _ALMEM_ARCH_SLOT(func, zen5), _ALMEM_ARCH_SLOT(func, zen6),

#define generate_system_variant(func) (func_ptr) __ALMEM_CONCAT(__ALMEM_PREFIX(func), system)

#define add_cpu_func_variants(func)                                                                                    \
    {                                                                                                                  \
        generate_arch_variants(func) generate_system_variant(func)                                                     \
    }

// CPU Dispatching Table: one row per selected function, in func_index order.
HIDDEN_SYMBOL func_ptr libmem_cpu_impls[FUNC_COUNT][CPU_VARIANT_COUNT] = {
#define _ALMEM_CPU_ROW(fn, FN) add_cpu_func_variants(fn),
    ALMEM_FOREACH_CPU_FUNC(_ALMEM_CPU_ROW)
#undef _ALMEM_CPU_ROW
};

#ifdef __cplusplus
}
#endif

#endif
