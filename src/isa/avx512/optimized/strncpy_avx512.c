/* Copyright (C) 2023-25 Advanced Micro Devices, Inc. All rights reserved.
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
#include <stddef.h>
#include "logger.h"
#include <stdint.h>
#include <immintrin.h>
#include "../../../base_impls/memset_erms_impls.h"
#include "zen_cpu_info.h"
#include "alm_defs.h"

extern cpu_info zen_info;

/* This is an optimized function to fill a memory region with
null bytes ('\0') using AVX-512 instructions */
static inline void *_fill_null_avx512(void *mem, size_t size)
{
    __m512i z0;

    if (size < 2 * ZMM_SZ)
    {
        // Use the ERMS feature to fill memory with zeros
        // This is likely more efficient for small sizes
        return __erms_stosb(mem, 0, size);
    }

    z0 = _mm512_set1_epi8(0);
    if (size <= 4 * ZMM_SZ)
    {
        _mm512_storeu_si512(mem , z0);
        _mm512_storeu_si512(mem + ZMM_SZ, z0);
        _mm512_storeu_si512(mem + size - 2 * ZMM_SZ, z0);
        _mm512_storeu_si512(mem + size - ZMM_SZ, z0);
        return mem;
    }
    _mm512_storeu_si512(mem + 0 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + 1 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + 2 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + 3 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + size - 4 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + size - 3 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + size - 2 * ZMM_SZ, z0);
    _mm512_storeu_si512(mem + size - 1 * ZMM_SZ, z0);

    if (size <= 8 * ZMM_SZ)
        return mem;

    size_t offset = 4 * ZMM_SZ;
    size -= 4 * ZMM_SZ;
    offset -= ((uint64_t)mem & (ZMM_SZ-1));

    while( offset < size )
    {
        _mm512_store_si512(mem + offset + 0 * ZMM_SZ, z0);
        _mm512_store_si512(mem + offset + 1 * ZMM_SZ, z0);
        _mm512_store_si512(mem + offset + 2 * ZMM_SZ, z0);
        _mm512_store_si512(mem + offset + 3 * ZMM_SZ, z0);
        offset += 4 * ZMM_SZ;
    }
    return mem;
}

/* This function is an optimized version of strncpy using AVX-512 instructions.
It copies atmost `size` bytes from `src` to `dst`, returning the original value of `dst` */
static inline char *  __attribute__((flatten)) _strncpy_avx512(char *dst, const char *src, size_t size)
{
    if (size == 0)
        return dst;

    size_t offset = 0, index = 0;
    __m512i z0, z1, z2, z3, z4, z5, z6;
    uint64_t match, match1, match2;
    __mmask64 mask, mask2;

    // Store the original destination pointer to return at the end
    register void *ret asm("rax");
    ret = dst;

    // Initialize a zeroed AVX-512 register for comparisons
    z0 = _mm512_set1_epi8(0);

    // Handle cases where size to copy is less than or equal to 64B
    if (size <= ZMM_SZ)
    {
        mask = ((uint64_t)-1) >> (ZMM_SZ - size);
        z1 =  _mm512_mask_loadu_epi8(z0 ,mask, src);
        _mm512_mask_storeu_epi8(dst, mask, z1);
        match = _mm512_cmpeq_epu8_mask(z0, z1) << 1;
        if (match)
        {
            index =  _tzcnt_u64(match);
            mask2 = ((uint64_t)-1) <<  index;
            _mm512_mask_storeu_epi8(dst, mask2 & mask, z0);
        }
        return ret;
    }

    // Handle cases where size to copy lies between 65B-128B
    if (size <= 2 * ZMM_SZ)
    {
        z1 = _mm512_loadu_si512(src);
        _mm512_storeu_si512(dst, z1);

        match = _mm512_cmpeq_epu8_mask(z0, z1);
        if (!match)
        {
            offset = size - ZMM_SZ;
            z2 = _mm512_loadu_si512(src + offset);
            _mm512_storeu_si512(dst + offset, z2);
            match = _mm512_cmpeq_epu8_mask(z0, z2) << 1;
            if (match == 0)
                return ret;
        }
        index =  _tzcnt_u64(match);
        if (index < ZMM_SZ)
            _fill_null_avx512(dst + index + offset, size - (offset + index));
        return ret;
    }

    // Handle cases where size to copy lies between 129B-256B
    if (size <= 4 * ZMM_SZ)
    {
        z1 = _mm512_loadu_si512(src);
        z2 = _mm512_loadu_si512(src + ZMM_SZ);
        z3 = _mm512_loadu_si512(src + size - 2 * ZMM_SZ);
        z4 = _mm512_loadu_si512(src + size - ZMM_SZ);

        _mm512_storeu_si512(dst, z1);
        _mm512_storeu_si512(dst + ZMM_SZ, z2);
        _mm512_storeu_si512(dst + size - 2 * ZMM_SZ, z3);
        _mm512_storeu_si512(dst + size - ZMM_SZ, z4);

        // Check for null bytes in the first two chunks
        if ((match1 = _mm512_cmpeq_epu8_mask(z0, _mm512_min_epu8(z1,z2))))
        {
            match = _mm512_cmpeq_epu8_mask(z0, z1);
            if (match == 0)
            {
                index = ZMM_SZ;
                match = match1;
            }
            index +=  _tzcnt_u64(match);
            _fill_null_avx512(dst + index, size - index);
        }
        // Check for null bytes in the last two chunks
        else if ((match2 = _mm512_cmpeq_epu8_mask(z0, _mm512_min_epu8(z3,z4))))
        {
            index = size - 2 * ZMM_SZ;
            match = _mm512_cmpeq_epu8_mask(z0, z3);
            if (match == 0)
            {
                // Handle the case where the last byte is null
                if ((match2 << 1) == 0)
                     return ret;
                index = size - ZMM_SZ;
                match = match2;
            }
            index +=  _tzcnt_u64(match);
            _fill_null_avx512(dst + index, size - index);
        }
        return ret;
    }

    // Handle cases where size to copy is larger than 256B
    z1 = _mm512_loadu_si512((void *)src + offset);
    z2 = _mm512_loadu_si512((void *)src + offset + ZMM_SZ);
    z3 = _mm512_loadu_si512((void *)src + offset + 2 * ZMM_SZ);
    z4 = _mm512_loadu_si512((void *)src + offset + 3 * ZMM_SZ);

    z5 = _mm512_min_epu8(z1, z2);
    z6 = _mm512_min_epu8(z3, z4);

    match = _mm512_cmpeq_epu8_mask(_mm512_min_epu8(z5, z6), z0);

    // If no null byte is found in the minimum of the combined chunks,
    // store the chunks into the dst
    if (!match)
    {
        _mm512_storeu_si512((void*)dst + offset, z1);
        _mm512_storeu_si512((void*)dst + offset + ZMM_SZ, z2);
        _mm512_storeu_si512((void*)dst + offset + 2 * ZMM_SZ, z3);
        _mm512_storeu_si512((void*)dst + offset + 3 * ZMM_SZ, z4);

        // If the size is greater than 512B, continue copying in chunks
        if (size > 8*ZMM_SZ)
        {
            offset = 4 * ZMM_SZ - ((uintptr_t)dst & (ZMM_SZ - 1));
            // Copy chunks until the end of the buffer or until a null byte is found
            while ((size - offset) >= 4*ZMM_SZ)
            {
                z1 = _mm512_loadu_si512((void *)src + offset);
                z2 = _mm512_loadu_si512((void *)src + offset + ZMM_SZ);
                z3 = _mm512_loadu_si512((void *)src + offset + 2 * ZMM_SZ);
                z4 = _mm512_loadu_si512((void *)src + offset + 3 * ZMM_SZ);

                z5 = _mm512_min_epu8(z1, z2);
                z6 = _mm512_min_epu8(z3, z4);

                match = _mm512_cmpeq_epu8_mask(_mm512_min_epu8(z5, z6), z0);
                if (match)
                    break;
                _mm512_store_si512((void*)dst + offset, z1);
                _mm512_store_si512((void*)dst + offset + ZMM_SZ, z2);
                _mm512_store_si512((void*)dst + offset + 2 * ZMM_SZ, z3);
                _mm512_store_si512((void*)dst + offset + 3 * ZMM_SZ, z4);

                offset += 4 * ZMM_SZ;
            }
        }
        // If no null byte is found after copying all chunks,
        // copy the remaining chunks at the end of the dst.
        if (!match)
        {
            offset = size - 4*ZMM_SZ;
            z1 = _mm512_loadu_si512((void *)src + offset);
            z2 = _mm512_loadu_si512((void *)src + offset + ZMM_SZ);
            z3 = _mm512_loadu_si512((void *)src + offset + 2 * ZMM_SZ);
            z4 = _mm512_loadu_si512((void *)src + offset + 3 * ZMM_SZ);

            z5 = _mm512_min_epu8(z1, z2);
            z6 = _mm512_min_epu8(z3, z4);

            match = _mm512_cmpeq_epu8_mask(_mm512_min_epu8(z5, z6), z0);
            if (match == 0)
            {
                _mm512_storeu_si512((void*)dst + offset, z1);
                _mm512_storeu_si512((void*)dst + offset + ZMM_SZ, z2);
                _mm512_storeu_si512((void*)dst + offset + 2 * ZMM_SZ, z3);
                _mm512_storeu_si512((void*)dst + offset + 3 * ZMM_SZ, z4);

                return ret;
            }
        }
    }

    // If a null byte is found in the combined minimum of the first two chunks,
    // store the first chunk and handle the second chunk based on whether it contains null bytes
    if ((match2 = _mm512_cmpeq_epu8_mask(z5, z0)))
    {
        _mm512_storeu_si512((void*)dst + offset, z1);
        if (!(match1 = _mm512_cmpeq_epu8_mask(z1, z0)))
        {
            _mm512_storeu_si512((void*)dst + offset + ZMM_SZ, z2);
            index += ZMM_SZ;
            match = match2;
        }
        else
            match = match1;
    }
    else
    {
        // If no null byte is found in the first two chunks, store the first three chunks
        _mm512_storeu_si512((void*)dst + offset, z1);
        _mm512_storeu_si512((void*)dst + offset + ZMM_SZ, z2);
        _mm512_storeu_si512((void*)dst + offset + 2 * ZMM_SZ, z3);

        index += 2 * ZMM_SZ;
        if (!(match1 = _mm512_cmpeq_epu8_mask(z3, z0)))
        {
            _mm512_storeu_si512((void*)dst + offset + 3 * ZMM_SZ, z4);
            index += ZMM_SZ;
        }
        else
            match = match1;
    }
    // Calculate the final index where a null byte was found
    index += offset + _tzcnt_u64(match) + 1;
    // If the index is less than the size, fill the rest of the destination with null bytes
    if (index < size)
        _fill_null_avx512(dst + index , size - index);

    return ret;
}
