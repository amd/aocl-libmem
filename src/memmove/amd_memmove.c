/* Copyright (C) 2022-23 Advanced Micro Devices, Inc. All rights reserved.
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
#include "logger.h"
#include "threshold.h"
#include "../base_impls/load_store_impls.h"


static inline void *_memmove_avx2(void *dst, const void *src, size_t size)
{
    __m256i y4, y5, y6, y7;

    if (size <= 4 * YMM_SZ)
    {
        __load_store_le_4ymm_vec(dst, src, size);
        return dst;
    }
    if (size <= 8 * YMM_SZ)
    {
        __load_store_le_8ymm_vec(dst, src, size);
        return dst;
    }

    if ((dst < (src + size)) && (src < dst))
    {
        y7 = _mm256_loadu_si256(src + 3 * YMM_SZ);
        y6 = _mm256_loadu_si256(src + 2 * YMM_SZ);
        y5 = _mm256_loadu_si256(src + 1 * YMM_SZ);
        y4 = _mm256_loadu_si256(src + 0 * YMM_SZ);
        __unaligned_load_and_store_4ymm_vec_loop_bkwd(dst, src, size, 4 * YMM_SZ);
        //copy first 4VECs to avoid override
        _mm256_storeu_si256(dst +  3 * YMM_SZ, y7);
        _mm256_storeu_si256(dst +  2 * YMM_SZ, y6);
        _mm256_storeu_si256(dst +  1 * YMM_SZ, y5);
        _mm256_storeu_si256(dst +  0 * YMM_SZ, y4);
    }
    else
    {
        y4 = _mm256_loadu_si256(src + size - 4 * YMM_SZ);
        y5 = _mm256_loadu_si256(src + size - 3 * YMM_SZ);
        y6 = _mm256_loadu_si256(src + size - 2 * YMM_SZ);
        y7 = _mm256_loadu_si256(src + size - 1 * YMM_SZ);
        __unaligned_load_and_store_4ymm_vec_loop(dst, src, size - 4 * YMM_SZ, 0);
        //copy last 4VECs to avoid override
        _mm256_storeu_si256(dst + size - 4 * YMM_SZ, y4);
        _mm256_storeu_si256(dst + size - 3 * YMM_SZ, y5);
        _mm256_storeu_si256(dst + size - 2 * YMM_SZ, y6);
        _mm256_storeu_si256(dst + size - 1 * YMM_SZ, y7);
    }
    return dst;
}

static inline void *nt_store(void *dst, const void *src, size_t size)
{
    size_t offset = 0;

    __load_store_ymm_vec(dst, src, offset);
    //compute the offset to align the dst to YMM_SZB boundary
    offset = YMM_SZ - ((size_t)dst & (YMM_SZ-1));

    size -= offset;

    offset = __unaligned_load_nt_store_4ymm_vec_loop_pftch(dst, src, size, offset);

    if ((size - offset) >= 2 * YMM_SZ)
        offset = __unaligned_load_nt_store_2ymm_vec_loop(dst, src, size, offset);
     
    if ((size - offset) > YMM_SZ)
        __load_store_ymm_vec(dst, src, offset);
    //copy last YMM_SZ Bytes
    __load_store_ymm_vec(dst, src, size - YMM_SZ);

    return dst;
}


#ifdef AVX512_FEATURE_ENABLED
static inline void *_memmove_avx512(void *dst, const void *src, size_t size)
{
    __m512i z4, z5, z6, z7, z8;
    if (size <= 2 * ZMM_SZ)
    {
        __load_store_le_2zmm_vec(dst, src, size);    
        return dst;
    }
    if (size <= 4 * ZMM_SZ)
    {
        __load_store_le_4zmm_vec(dst, src, size);    
        return dst;
    }
    if ((dst < (src + size)) && (src < dst))
    {
         z4 = _mm512_loadu_si512(src + 3 * ZMM_SZ);
         z5 = _mm512_loadu_si512(src + 2 * ZMM_SZ);
         z6 = _mm512_loadu_si512(src + 1 * ZMM_SZ);
         z7 = _mm512_loadu_si512(src + 0 * ZMM_SZ);
        if ((((size_t)dst & (ZMM_SZ-1)) == 0) && (((size_t)src & (ZMM_SZ-1)) == 0))
        {
            //load the last VEC to handle size not multiple of the vec.
            z8 = _mm512_loadu_si512(src + size - ZMM_SZ);
            __aligned_load_and_store_4zmm_vec_loop_bkwd(dst, src, size & ~(ZMM_SZ-1), 3 * ZMM_SZ);
            //store the last VEC to handle size not multiple of the vec.
            _mm512_storeu_si512(dst + size - ZMM_SZ, z8);
        }
        else
            __unaligned_load_and_store_4zmm_vec_loop_bkwd(dst, src, size, 4 * ZMM_SZ);
         _mm512_storeu_si512(dst +  3 * ZMM_SZ, z4);
         _mm512_storeu_si512(dst +  2 * ZMM_SZ, z5);
         _mm512_storeu_si512(dst +  1 * ZMM_SZ, z6);
         _mm512_storeu_si512(dst +  0 * ZMM_SZ, z7);
    }
    else
    {
         z4 = _mm512_loadu_si512(src + size - 4 * ZMM_SZ);
         z5 = _mm512_loadu_si512(src + size - 3 * ZMM_SZ);
         z6 = _mm512_loadu_si512(src + size - 2 * ZMM_SZ);
         z7 = _mm512_loadu_si512(src + size - 1 * ZMM_SZ);
        if ((((size_t)dst & (ZMM_SZ-1)) == 0) && (((size_t)src & (ZMM_SZ-1)) == 0))
            __aligned_load_and_store_4zmm_vec_loop(dst, src, size - 4 * ZMM_SZ, 0);
        else
            __unaligned_load_and_store_4zmm_vec_loop(dst, src, size - 4 * ZMM_SZ, 0);

         _mm512_storeu_si512(dst + size - 4 * ZMM_SZ, z4);
         _mm512_storeu_si512(dst + size - 3 * ZMM_SZ, z5);
         _mm512_storeu_si512(dst + size - 2 * ZMM_SZ, z6);
         _mm512_storeu_si512(dst + size - 1 * ZMM_SZ, z7);
    }
    return dst;
}
#endif

void * __attribute__((flatten)) amd_memmove(void * __restrict dst,
                        const void * __restrict src, size_t size)
{
    LOG_INFO("\n");
#ifdef AVX512_FEATURE_ENABLED
    if (size == 0)
        return dst;
    if (size <= ZMM_SZ)
    {
       __m512i z0;
        z0 = _mm512_loadu_si512(src);
       __mmask64 mask = ((uint64_t)-1) >> (64 - size);
        _mm512_mask_storeu_epi8(dst, mask, z0);
        return dst;
    }
#else
    if (size <= 2 * YMM_SZ)
        return __load_store_le_2ymm_vec_overlap(dst, src, size);
#endif
    if (size >= __nt_start_threshold)
    {
        if (((src + size) < dst) || ((dst + size) < src))
        {
            return nt_store(dst, src, size);
        }
    }
#ifdef AVX512_FEATURE_ENABLED
    return _memmove_avx512(dst, src, size);
#else
    return _memmove_avx2(dst, src, size);
#endif
}

void *memmove(void *, const void *, size_t) __attribute__((weak,
                    alias("amd_memmove"), visibility("default")));
