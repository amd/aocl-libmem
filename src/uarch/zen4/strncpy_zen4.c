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

#ifndef STRNCPY_AVX512
#define STRNCPY_AVX512

#include "logger.h"
#include "../../isa/avx512/optimized/strcpy_avx512.c"

#ifdef STRNCPY_AVX512
#undef STRNCPY_AVX512
#endif

#ifndef STRNCPY_INLINE_MAX
#define STRNCPY_INLINE_MAX  (4 * ZMM_SZ)
#endif

#define STRNCPY_COPY_VEC(v, base, tail)                                        \
    do {                                                                       \
        __mmask64 m_ = _mm512_testn_epi8_mask((v), (v));                       \
        if ((tail) || unlikely(m_))                                            \
        {                                                                      \
            _mm512_storeu_si512((__m512i *) (dst + (base)),                    \
                _mm512_maskz_mov_epi8(_bzhi_u64(UINT64_MAX, _tzcnt_u64(m_)),   \
                                      (v)));                                   \
            if (!(tail) && (base) + ZMM_SZ < size)                             \
            {                                                                  \
                _fill_null_avx512(dst + (base) + ZMM_SZ,                       \
                                  size - (base) - ZMM_SZ);                     \
            }                                                                  \
            return dst;                                                        \
        }                                                                      \
        _mm512_storeu_si512((__m512i *) (dst + (base)), (v));                  \
    } while (0)

static char * __attribute__((noinline, flatten))
_strncpy_large_zen4(char * __restrict dst, const char * __restrict src,
                    size_t size)
{
    return _strncpy_avx512(dst, src, size);
}

HIDDEN_SYMBOL char * __attribute__((flatten)) __strncpy_zen4(char * __restrict dst,
                  const char * __restrict src, size_t size)
{
    __m512i z0, z1, z2, z3;

    LOG_INFO("\n");

    if (size <= STRNCPY_INLINE_MAX &&
        likely((PAGE_SZ - ((uintptr_t) src & (PAGE_SZ - 1))) >= size))
    {
        if (size <= ZMM_SZ)
        {

            __mmask64 w = _bzhi_u64(UINT64_MAX, size);
            z0 = _mm512_maskz_loadu_epi8(w, src);
            __mmask64 m = _mm512_testn_epi8_mask(z0, z0) & w;

            _mm512_mask_storeu_epi8(dst, w,
                _mm512_maskz_mov_epi8(_bzhi_u64(UINT64_MAX, _tzcnt_u64(m)), z0));
            return dst;
        }
        size_t last = size - ZMM_SZ;
        z0 = _mm512_loadu_si512((const __m512i *) src);
        z3 = _mm512_loadu_si512((const __m512i *) (src + last));

        if (size <= 2 * ZMM_SZ)
        {
            STRNCPY_COPY_VEC(z0, 0, 0);
            STRNCPY_COPY_VEC(z3, last, 1);
        }

        z1 = _mm512_loadu_si512((const __m512i *) (src + ZMM_SZ));

        if (size <= 3 * ZMM_SZ)
        {
            STRNCPY_COPY_VEC(z0, 0, 0);
            STRNCPY_COPY_VEC(z1, ZMM_SZ, 0);
            STRNCPY_COPY_VEC(z3, last, 1);
        }

        z2 = _mm512_loadu_si512((const __m512i *) (src + 2 * ZMM_SZ));

        STRNCPY_COPY_VEC(z0, 0, 0);
        STRNCPY_COPY_VEC(z1, ZMM_SZ, 0);
        STRNCPY_COPY_VEC(z2, 2 * ZMM_SZ, 0);
        STRNCPY_COPY_VEC(z3, last, 1);
    }

    return _strncpy_large_zen4(dst, src, size);
}

#ifndef ALMEM_DYN_DISPATCH
char *strncpy(char *, const char *, size_t) __attribute__((weak,
                        alias("__strncpy_zen4")));
#endif

#endif // STRNCPY_AVX512