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
#include "logger.h"
#include "threshold.h"
#include "zen_cpu_info.h"
#include "alm_defs.h"
#include <immintrin.h>
#include <stddef.h>
#include "../../../base_impls/memset_erms_impls.h"

static inline void *_memset_avx512_erms(void *mem, int val, size_t size)
{
    __m512i z0;
    size_t offset;
    __mmask64 mask;
    register void *ret asm("rax");
    ret = mem;

    z0 = _mm512_set1_epi8(val);
    if (likely(size <= 2 * ZMM_SZ))
    {
        if (likely(size <= ZMM_SZ))
        {
            if (size)
            {
                mask = ((uint64_t)-1) >> (ZMM_SZ - size);
                _mm512_mask_storeu_epi8(mem, mask, z0);
            }
            return ret;
        }
        _mm512_storeu_si512(mem , z0);
        _mm512_storeu_si512(mem + size - ZMM_SZ, z0);
        return ret;
    }
    else if (likely(size <= 8 * ZMM_SZ))
    {
        _mm512_storeu_si512(mem , z0);
        _mm512_storeu_si512(mem + ZMM_SZ, z0);
        if (likely(size <= 4 * ZMM_SZ))
        {
            _mm512_storeu_si512(mem + size - 2 * ZMM_SZ, z0);
            _mm512_storeu_si512(mem + size - ZMM_SZ, z0);
            return ret;
        }

        _mm512_storeu_si512(mem + 2 * ZMM_SZ, z0);
        _mm512_storeu_si512(mem + 3 * ZMM_SZ, z0);

        _mm512_storeu_si512(mem + size - 4 * ZMM_SZ, z0);
        _mm512_storeu_si512(mem + size - 3 * ZMM_SZ, z0);
        _mm512_storeu_si512(mem + size - 2 * ZMM_SZ, z0);
        _mm512_storeu_si512(mem + size - ZMM_SZ, z0);
        return ret;
    }
    // store first 4xVECs irrespective of alignment
    _mm512_storeu_si512(mem , z0);
    _mm512_storeu_si512(mem + ZMM_SZ, z0);
    _mm512_storeu_si512(mem + 2 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + 3 * ZMM_SZ, z0);

    // adjust offset to vector alignment
    offset = 4 * ZMM_SZ - ((uintptr_t)mem & (ZMM_SZ - 1));

    // temporal aligned stores upto L1 cache per Core
    if (size <= __repstore_start_threshold)
    {
        while (offset < (size - 4 * ZMM_SZ))
        {
            _mm512_store_si512(mem + offset + 0 * ZMM_SZ, z0);
            _mm512_store_si512(mem + offset + 1 * ZMM_SZ, z0);
            _mm512_store_si512(mem + offset + 2 * ZMM_SZ, z0);
            _mm512_store_si512(mem + offset + 3 * ZMM_SZ, z0);
            offset += 4 * ZMM_SZ;
        }
    }

    // rep-stores for sizes upto L3 cache per CCX
    else if (size <= __repstore_stop_threshold)
    {
        __erms_stosb(mem + offset, val, size - offset);
        return ret;
    }
    // non-temporal stores for sizes above L3 cache per CCX
    else
    {
        while (offset < (size - 4 * ZMM_SZ))
        {
            _mm512_stream_si512(mem + offset + 0 * ZMM_SZ, z0);
            _mm512_stream_si512(mem + offset + 1 * ZMM_SZ, z0);
            _mm512_stream_si512(mem + offset + 2 * ZMM_SZ, z0);
            _mm512_stream_si512(mem + offset + 3 * ZMM_SZ, z0);
            offset += 4 * ZMM_SZ;
        }
    }
    // unaligned stores of the tail 4xVECs
    _mm512_storeu_si512(mem + size - 4 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + size - 3 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + size - 2 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + size - ZMM_SZ, z0);

    return ret;
}
