/* Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
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

#include "zen_cpu_info.h"
#include "libmem_impls.h"

/* A Zen arch is only a resolvable target when its object was built
 * (ALMEM_ZEN<N>_ENABLED, driven by ALMEM_DISPATCH_ARCH). Archs that were not
 * built expand to nothing so the resolver falls through to the next-oldest
 * enabled arch, and ultimately to the system implementation. */

/* __builtin_cpu_is("znver6") is only recognized starting with GCC 16.
 * On older compilers fall back to the manual CPUID-based resolver below,
 * which detects Zen6 via the AVX512_FP16 feature bit. */
#if defined(__GNUC__) && (__GNUC__ >= 16)

#ifdef ALMEM_ZEN6_ENABLED
#define _ALMEM_RESOLVE_zen6(func)                                                                                      \
    if (__builtin_cpu_is("znver6"))                                                                                    \
        return __ALMEM_CONCAT(__ALMEM_PREFIX(func), zen6);
#else
#define _ALMEM_RESOLVE_zen6(func)
#endif
#ifdef ALMEM_ZEN5_ENABLED
#define _ALMEM_RESOLVE_zen5(func)                                                                                      \
    if (__builtin_cpu_is("znver5"))                                                                                    \
        return __ALMEM_CONCAT(__ALMEM_PREFIX(func), zen5);
#else
#define _ALMEM_RESOLVE_zen5(func)
#endif
#ifdef ALMEM_ZEN4_ENABLED
#define _ALMEM_RESOLVE_zen4(func)                                                                                      \
    if (__builtin_cpu_is("znver4"))                                                                                    \
        return __ALMEM_CONCAT(__ALMEM_PREFIX(func), zen4);
#else
#define _ALMEM_RESOLVE_zen4(func)
#endif
#ifdef ALMEM_ZEN3_ENABLED
#define _ALMEM_RESOLVE_zen3(func)                                                                                      \
    if (__builtin_cpu_is("znver3"))                                                                                    \
        return __ALMEM_CONCAT(__ALMEM_PREFIX(func), zen3);
#else
#define _ALMEM_RESOLVE_zen3(func)
#endif
#ifdef ALMEM_ZEN2_ENABLED
#define _ALMEM_RESOLVE_zen2(func)                                                                                      \
    if (__builtin_cpu_is("znver2"))                                                                                    \
        return __ALMEM_CONCAT(__ALMEM_PREFIX(func), zen2);
#else
#define _ALMEM_RESOLVE_zen2(func)
#endif
#ifdef ALMEM_ZEN1_ENABLED
#define _ALMEM_RESOLVE_zen1(func)                                                                                      \
    if (__builtin_cpu_is("znver1"))                                                                                    \
        return __ALMEM_CONCAT(__ALMEM_PREFIX(func), zen1);
#else
#define _ALMEM_RESOLVE_zen1(func)
#endif

#define AMD_ZEN_CPU_RESOLVER(func)                                                                                     \
    __builtin_cpu_init();                                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        _ALMEM_RESOLVE_zen6(func) _ALMEM_RESOLVE_zen5(func) _ALMEM_RESOLVE_zen4(func) _ALMEM_RESOLVE_zen3(func)        \
            _ALMEM_RESOLVE_zen2(func) _ALMEM_RESOLVE_zen1(func) return __ALMEM_CONCAT(__ALMEM_PREFIX(func), system);   \
    } while (0)

#else // Compilers that do not detect the latest architecture: 'znver6'

#ifdef ALMEM_ZEN6_ENABLED
#define _ALMEM_CPUID_zen6(func)                                                                                        \
    if (edx & AVX512_FP16_MASK)                                                                                        \
        return __ALMEM_CONCAT(__ALMEM_PREFIX(func), zen6);
#else
#define _ALMEM_CPUID_zen6(func)
#endif
#ifdef ALMEM_ZEN5_ENABLED
#define _ALMEM_CPUID_zen5(func)                                                                                        \
    if (ecx & MOVDIRI_MASK)                                                                                            \
        return __ALMEM_CONCAT(__ALMEM_PREFIX(func), zen5);
#else
#define _ALMEM_CPUID_zen5(func)
#endif
#ifdef ALMEM_ZEN4_ENABLED
#define _ALMEM_CPUID_zen4(func) return __ALMEM_CONCAT(__ALMEM_PREFIX(func), zen4);
#else
#define _ALMEM_CPUID_zen4(func)
#endif
#ifdef ALMEM_ZEN3_ENABLED
#define _ALMEM_CPUID_zen3(func)                                                                                        \
    if (ecx & VPCLMULQDQ_MASK)                                                                                         \
        return __ALMEM_CONCAT(__ALMEM_PREFIX(func), zen3);
#else
#define _ALMEM_CPUID_zen3(func)
#endif
#ifdef ALMEM_ZEN2_ENABLED
#define _ALMEM_CPUID_zen2(func)                                                                                        \
    if (ecx & RDPID_MASK)                                                                                              \
        return __ALMEM_CONCAT(__ALMEM_PREFIX(func), zen2);
#else
#define _ALMEM_CPUID_zen2(func)
#endif
#ifdef ALMEM_ZEN1_ENABLED
#define _ALMEM_CPUID_zen1(func) if (ecx & RDSEED_MASK) return __ALMEM_CONCAT(__ALMEM_PREFIX(func), zen1);
#else
#define _ALMEM_CPUID_zen1(func)
#endif

#define AMD_ZEN_CPU_RESOLVER(func)                                                                                     \
    do                                                                                                                 \
    {                                                                                                                  \
        uint32_t eax = 0x7;                                                                                            \
        uint32_t ebx = 0;                                                                                              \
        uint32_t ecx = 0;                                                                                              \
        uint32_t edx = 0;                                                                                              \
        asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "0"(eax), "2"(ecx));                       \
        if (ebx & AVX512_MASK)                                                                                         \
        {                                                                                                              \
            _ALMEM_CPUID_zen6(func) _ALMEM_CPUID_zen5(func)                                                            \
                _ALMEM_CPUID_zen4(func) return __ALMEM_CONCAT(__ALMEM_PREFIX(func), system);                           \
        }                                                                                                              \
        if (ebx & AVX2_MASK)                                                                                           \
        {                                                                                                              \
            _ALMEM_CPUID_zen3(func) _ALMEM_CPUID_zen2(func) _ALMEM_CPUID_zen1(func)                                    \
        }                                                                                                              \
        return __ALMEM_CONCAT(__ALMEM_PREFIX(func), system);                                                           \
    } while (0)

#endif

/* Macro to define ifunc resolver functions using predefined typedefs */
#define DEF_IFUNC_RESOLVER(func)                                                                                       \
    __attribute__((ifunc("libmem_" #func "_resolver"))) extern typeof(*(amd_##func##_fn) 0) func;                      \
    amd_##func##_fn libmem_##func##_resolver(void)                                                                     \
    {                                                                                                                  \
        AMD_ZEN_CPU_RESOLVER(func);                                                                                    \
    }

/* Define an IFUNC resolver (and thus the public symbol) only for functions
 * selected via ALMEM_FUNCTIONS; others fall back to the libc implementation. */
#ifdef ALMEM_FUNC_MEMCPY
DEF_IFUNC_RESOLVER(memcpy)
#endif
#ifdef ALMEM_FUNC_MEMPCPY
DEF_IFUNC_RESOLVER(mempcpy)
#endif
#ifdef ALMEM_FUNC_MEMMOVE
DEF_IFUNC_RESOLVER(memmove)
#endif
#ifdef ALMEM_FUNC_MEMSET
DEF_IFUNC_RESOLVER(memset)
#endif
#ifdef ALMEM_FUNC_MEMCMP
DEF_IFUNC_RESOLVER(memcmp)
#endif
#ifdef ALMEM_FUNC_MEMCHR
DEF_IFUNC_RESOLVER(memchr)
#endif
#ifdef ALMEM_FUNC_STRCPY
DEF_IFUNC_RESOLVER(strcpy)
#endif
#ifdef ALMEM_FUNC_STRNCPY
DEF_IFUNC_RESOLVER(strncpy)
#endif
#ifdef ALMEM_FUNC_STRCMP
DEF_IFUNC_RESOLVER(strcmp)
#endif
#ifdef ALMEM_FUNC_STRNCMP
DEF_IFUNC_RESOLVER(strncmp)
#endif
#ifdef ALMEM_FUNC_STRCAT
DEF_IFUNC_RESOLVER(strcat)
#endif
#ifdef ALMEM_FUNC_STRNCAT
DEF_IFUNC_RESOLVER(strncat)
#endif
#ifdef ALMEM_FUNC_STRSTR
DEF_IFUNC_RESOLVER(strstr)
#endif
#ifdef ALMEM_FUNC_STRLEN
DEF_IFUNC_RESOLVER(strlen)
#endif
#ifdef ALMEM_FUNC_STRNLEN
DEF_IFUNC_RESOLVER(strnlen)
#endif
#ifdef ALMEM_FUNC_STRCHR
DEF_IFUNC_RESOLVER(strchr)
#endif
#ifdef ALMEM_FUNC_STRRCHR
DEF_IFUNC_RESOLVER(strrchr)
#endif
#ifdef ALMEM_FUNC_STRSPN
DEF_IFUNC_RESOLVER(strspn)
#endif
