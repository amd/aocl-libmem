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

#include "logger.h"
#include "threshold.h"
#include "zen_cpu_info.h"
#include "almem_defs.h"
#include <immintrin.h>
#include <stddef.h>
#include "../../base_impls/memset_erms_impls.h"

/* noinline+cold: flatten must not pull stosb/NT clobbers into the small-size path. */
static void __attribute__((noinline, cold))
_memset_zen6_rep(void *mem, int val, size_t size)
{
    /* Skip the 4-vec head already stored; stosb continues through the tail. */
    size_t offset = 4 * ZMM_SZ - ((uintptr_t)mem & (ZMM_SZ - 1));
    __erms_stosb((char *)mem + offset, val, size - offset);
}

static void __attribute__((noinline, cold))
_memset_zen6_nt(void *mem, int val, size_t size)
{
    __m512i z0 = _mm512_set1_epi8((char)val);
    /* Aligned interior only; head/tail already stored by the caller. */
    size_t offset = 4 * ZMM_SZ - ((uintptr_t)mem & (ZMM_SZ - 1));
    while (offset < size - 4 * ZMM_SZ)
    {
        _mm512_stream_si512((__m512i *)((char *)mem + offset + 0 * ZMM_SZ), z0);
        _mm512_stream_si512((__m512i *)((char *)mem + offset + 1 * ZMM_SZ), z0);
        _mm512_stream_si512((__m512i *)((char *)mem + offset + 2 * ZMM_SZ), z0);
        _mm512_stream_si512((__m512i *)((char *)mem + offset + 3 * ZMM_SZ), z0);
        offset += 4 * ZMM_SZ;
    }
    _mm_sfence();
}

/* <=64B masked, <=512B overlap stores, then temporal / stosb / NT by threshold. */
HIDDEN_SYMBOL void *__attribute__((flatten)) __memset_zen6(void *mem, int val, size_t size)
{
    LOG_INFO("\n");
    register void *ret asm("rax") = mem;
    __m512i z0 = _mm512_set1_epi8((char)val);

    if (likely(size <= ZMM_SZ)) {
        if (size <= YMM_SZ) {
            __mmask32 mask = _bzhi_u32((uint32_t)-1, (uint32_t)size);
            /* Masked YMM: less store-port traffic than a masked ZMM. */
            _mm256_mask_storeu_epi8(mem, mask, _mm512_castsi512_si256(z0));
            return ret;
        }
        __mmask64 mask = _bzhi_u64((uint64_t)-1, (uint8_t)size);
        _mm512_mask_storeu_epi8(mem, mask, z0);
        return ret;
    }

    /* Overlapping head/tail stores; complete by 512B. Later tiers reuse these. */
    _mm512_storeu_si512((__m512i *)mem, z0);
    _mm512_storeu_si512((__m512i *)((char *)mem + size - ZMM_SZ), z0);
    if (size <= 2 * ZMM_SZ)
        return ret;

    _mm512_storeu_si512((__m512i *)((char *)mem + ZMM_SZ), z0);
    _mm512_storeu_si512((__m512i *)((char *)mem + size - 2 * ZMM_SZ), z0);
    if (size <= 4 * ZMM_SZ)
        return ret;

    _mm512_storeu_si512((__m512i *)((char *)mem + 2 * ZMM_SZ), z0);
    _mm512_storeu_si512((__m512i *)((char *)mem + 3 * ZMM_SZ), z0);
    _mm512_storeu_si512((__m512i *)((char *)mem + size - 4 * ZMM_SZ), z0);
    _mm512_storeu_si512((__m512i *)((char *)mem + size - 3 * ZMM_SZ), z0);
    if (size <= 8 * ZMM_SZ)
        return ret;

    if (size < __repstore_start_threshold)
    {
        /* Temporal aligned interior; stop before the already-stored tail. */
        size_t offset = 4 * ZMM_SZ - ((uintptr_t)mem & (ZMM_SZ - 1));
        while (offset < size - 4 * ZMM_SZ)
        {
            _mm512_store_si512((__m512i *)((char *)mem + offset + 0 * ZMM_SZ), z0);
            _mm512_store_si512((__m512i *)((char *)mem + offset + 1 * ZMM_SZ), z0);
            _mm512_store_si512((__m512i *)((char *)mem + offset + 2 * ZMM_SZ), z0);
            _mm512_store_si512((__m512i *)((char *)mem + offset + 3 * ZMM_SZ), z0);
            offset += 4 * ZMM_SZ;
        }
        return ret;
    }

    /* NT at/above repstore_stop, else stosb. */
    if (size >= __repstore_stop_threshold)
    {
        _memset_zen6_nt(mem, val, size);
        return ret;
    }

    _memset_zen6_rep(mem, val, size);
    return ret;
}

#ifndef ALMEM_DYN_DISPATCH
void *memset(void *, int, size_t) __attribute__((weak, alias("__memset_zen6")));
#endif
