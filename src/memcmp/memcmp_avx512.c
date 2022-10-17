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

#include <stddef.h>
#include "logger.h"
#include <immintrin.h>
#include <stdint.h>
#include "zen_cpu_info.h"

static int memcmp_le_2zmm(const void *mem1, const void *mem2, size_t size)
{
    __m512i z0, z1, z2, z3;
    __m256i y0, y1, y2, y3;
    __m128i x0, x1, x2, x3;
    uint32_t index = 0;
    int32_t ret = 0;
    int64_t ret64 = 0;
    if (size == 0)
        return 0;

    if (size == 1)
    {
        return *((uint8_t *)mem1) - *((uint8_t *)mem2);
    }
    if (size <= 2 * WORD_SZ)
    {
        ret = _tzcnt_u32((*(uint16_t*)mem1)^(*(uint16_t*)mem2));
        if (ret == 32)
        {
            index = size - WORD_SZ;
            ret = _tzcnt_u32((*(uint16_t*)(mem1 + index))^(*(uint16_t*)(mem2 + index)));
            if (ret == 32)
                return 0;
        }
        index += ret/8;
        return *(uint8_t*)(mem1 + index) -  *(uint8_t*)(mem2 + index);
    }
    if (size <= 2 * DWORD_SZ)
    {
        ret = _tzcnt_u32((*(uint32_t*)mem1)^(*(uint32_t*)mem2));
        if (ret == 32)
        {
            index = size - DWORD_SZ;
            ret = _tzcnt_u32((*(uint32_t*)(mem1 + index))^(*(uint32_t*)(mem2 + index)));
            if (ret == 32)
                return 0;
        }
        index += ret/8;
        return *(uint8_t*)(mem1 + index) -  *(uint8_t*)(mem2 + index);
    }
    if (size <= 2 *  QWORD_SZ)
    {
        ret = _tzcnt_u64((*(uint64_t*)mem1)^(*(uint64_t*)mem2));
        if (ret == 64)
        {
            index = size - QWORD_SZ;
            ret = _tzcnt_u64((*(uint64_t*)(mem1 + index))^(*(uint64_t*)(mem2 + index)));
            if (ret == 64)
                 return 0;
        }
        index += ret >> 3;
        return *(uint8_t*)(mem1 + index) -  *(uint8_t*)(mem2 + index);
    }
    if (size <= 2 * XMM_SZ)
    {
        x0 = _mm_loadu_si128(mem1);
        x1 = _mm_loadu_si128(mem2);
        x0 = _mm_cmpeq_epi8(x0, x1);
        ret = 0xffff - _mm_movemask_epi8(x0);
        if (ret == 0)
        {
            index = size - XMM_SZ;
            x2 = _mm_loadu_si128(mem1 + index);
            x3 = _mm_loadu_si128(mem2 + index);
            x2 = _mm_cmpeq_epi8(x2, x3);
            ret = 0xffff - _mm_movemask_epi8(x2);
            if (ret == 0)
                return 0;
        }
        index += _tzcnt_u32(ret);
        return (*(uint8_t*)(mem1 + index) - *(uint8_t*)(mem2 + index));
    }
    if (size <= 2 * YMM_SZ)
    {
        y0 = _mm256_loadu_si256(mem1);
        y1 = _mm256_loadu_si256(mem2);
        y0 = _mm256_cmpeq_epi8(y0, y1);
        ret = _mm256_movemask_epi8(y0) + 1;
        if (ret == 0)
        {
            index = size - YMM_SZ;
            y2 = _mm256_loadu_si256(mem1 + index);
            y3 = _mm256_loadu_si256(mem2 + index);
            y2 = _mm256_cmpeq_epi8(y2, y3);
            ret = _mm256_movemask_epi8(y2) + 1;
            if (ret == 0)
                return 0;
        }
        index += _tzcnt_u32(ret);
        return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
    }
    z0 = _mm512_loadu_si512(mem2 + 0 * ZMM_SZ);
    z2 = _mm512_loadu_si512(mem1 + 0 * ZMM_SZ);

    ret64 = _mm512_cmpeq_epu8_mask(z0, z2) + 1;
    if (ret64 == 0)
        {
        index = size - ZMM_SZ;
        z1 = _mm512_loadu_si512(mem2 + index);
        z3 = _mm512_loadu_si512(mem1 + index);
            ret64 = _mm512_cmpeq_epu8_mask(z1, z3) + 1;
        if (ret64 == 0)
            return 0;
    }
    index += _tzcnt_u64(ret64);
    return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
}

int __memcmp_avx512_unaligned(const void *mem1, const void *mem2, size_t size)
{
    __m512i z0, z1, z2, z3, z4, z5, z6, z7;
    size_t offset = 0, index = 0;
    int64_t ret;

    LOG_INFO("\n");

    if (size <= 2 * ZMM_SZ)
	return memcmp_le_2zmm(mem1, mem2, size);

    if (size <= 4 * ZMM_SZ)
    {
        z0 = _mm512_loadu_si512(mem2 + 0 * ZMM_SZ);
        z1 = _mm512_loadu_si512(mem2 + 1 * ZMM_SZ);
        z4 = _mm512_loadu_si512(mem1 + 0 * ZMM_SZ);
        z5 = _mm512_loadu_si512(mem1 + 1 * ZMM_SZ);
        ret = _mm512_cmpeq_epu8_mask(z0, z4) + 1;
        if (ret != 0)
            {
                index = _tzcnt_u64(ret);
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        ret = _mm512_cmpeq_epu8_mask(z1, z5) + 1;
        if (ret != 0)
            {
                index = _tzcnt_u64(ret) + ZMM_SZ;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        z2 = _mm512_loadu_si512(mem2 + size - 2 * ZMM_SZ);
        z3 = _mm512_loadu_si512(mem2 + size - 1 * ZMM_SZ);
        z6 = _mm512_loadu_si512(mem1 + size - 2 * ZMM_SZ);
        z7 = _mm512_loadu_si512(mem1 + size - 1 * ZMM_SZ);
        ret = _mm512_cmpeq_epu8_mask(z2, z6) + 1;
        if (ret != 0)
            {
                index = _tzcnt_u64(ret) + size - 2 * ZMM_SZ;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        ret = _mm512_cmpeq_epu8_mask(z3, z7) + 1;
        if (ret != 0)
            {
                index = _tzcnt_u64(ret) + size - ZMM_SZ;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        return 0;
    }
    size -= 4 * ZMM_SZ;
    while (size > offset)
    {
        z0 = _mm512_loadu_si512(mem2 + offset + 0 * ZMM_SZ);
        z4 = _mm512_loadu_si512(mem1 + offset + 0 * ZMM_SZ);
        ret = _mm512_cmpeq_epu8_mask(z0, z4) + 1;
        if (ret != 0)
            {
                index = _tzcnt_u64(ret) + offset;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        z1 = _mm512_loadu_si512(mem2 + offset + 1 * ZMM_SZ);
        z5 = _mm512_loadu_si512(mem1 + offset + 1 * ZMM_SZ);
        ret = _mm512_cmpeq_epu8_mask(z1, z5) + 1;
        if (ret != 0)
            {
                index = _tzcnt_u64(ret) + offset + ZMM_SZ;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        z2 = _mm512_loadu_si512(mem2 + offset + 2 * ZMM_SZ);
        z6 = _mm512_loadu_si512(mem1 + offset + 2 * ZMM_SZ);
        ret = _mm512_cmpeq_epu8_mask(z2, z6) + 1;
        if (ret != 0)
            {
                index = _tzcnt_u64(ret) + offset + 2 * ZMM_SZ;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        z3 = _mm512_loadu_si512(mem2 + offset + 3 * ZMM_SZ);
        z7 = _mm512_loadu_si512(mem1 + offset + 3 * ZMM_SZ);
        ret = _mm512_cmpeq_epu8_mask(z3, z7) + 1;
        if (ret != 0)
            {
                index = _tzcnt_u64(ret) + offset + 3 * ZMM_SZ;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        offset += 4 * ZMM_SZ;
    }
    size += 4 * ZMM_SZ;
    if ( offset == size)
        return 0;

    z0 = _mm512_loadu_si512(mem2 + size - 4 * ZMM_SZ);
    z4 = _mm512_loadu_si512(mem1 + size - 4 * ZMM_SZ);
    ret = _mm512_cmpeq_epu8_mask(z0, z4) + 1;
    if (ret != 0)
    {
        index = _tzcnt_u64(ret) + size - 4 * ZMM_SZ;
        return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
    }
    z1 = _mm512_loadu_si512(mem2 + size - 3 * ZMM_SZ);
    z5 = _mm512_loadu_si512(mem1 + size - 3 * ZMM_SZ);
    ret = _mm512_cmpeq_epu8_mask(z1, z5) + 1;
    if (ret != 0)
    {
        index = _tzcnt_u64(ret) + size - 3 * ZMM_SZ;
        return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
    }
    z2 = _mm512_loadu_si512(mem2 + size - 2 * ZMM_SZ);
    z6 = _mm512_loadu_si512(mem1 + size - 2 * ZMM_SZ);
    ret = _mm512_cmpeq_epu8_mask(z2, z6) + 1;
    if (ret != 0)
    {
        index = _tzcnt_u64(ret) + size - 2 * ZMM_SZ;
        return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
    }
    z3 = _mm512_loadu_si512(mem2 + size - 1 * ZMM_SZ);
    z7 = _mm512_loadu_si512(mem1 + size - 1 * ZMM_SZ);
    ret = _mm512_cmpeq_epu8_mask(z3, z7) + 1;
    if (ret != 0)
    {
        index = _tzcnt_u64(ret) + size - 1 * ZMM_SZ;
        return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
    }
    return 0;
}

int __memcmp_avx512_nt(const void *mem1, const void *mem2, size_t size)
{
    __m512i z0, z1, z2, z3, z4, z5, z6, z7;
    size_t offset = 0, index = 0;
    int64_t ret;

    LOG_INFO("\n");

    if (size <= 2 * ZMM_SZ)
	return memcmp_le_2zmm(mem1, mem2, size);

    if (size <= 4 * ZMM_SZ)
    {
        z0 = _mm512_stream_load_si512((void *)mem2 + 0 * ZMM_SZ);
        z1 = _mm512_stream_load_si512((void *)mem2 + 1 * ZMM_SZ);
        z4 = _mm512_stream_load_si512((void *)mem1 + 0 * ZMM_SZ);
        z5 = _mm512_stream_load_si512((void *)mem1 + 1 * ZMM_SZ);
        ret = _mm512_cmpeq_epu8_mask(z0, z4) + 1;
        if (ret != 0)
            {
                index = _tzcnt_u64(ret);
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        ret = _mm512_cmpeq_epu8_mask(z1, z5) + 1;
        if (ret != 0)
            {
                index = _tzcnt_u64(ret) + ZMM_SZ;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        z2 = _mm512_loadu_si512(mem2 + size - 2 * ZMM_SZ);
        z3 = _mm512_loadu_si512(mem2 + size - 1 * ZMM_SZ);
        z6 = _mm512_loadu_si512(mem1 + size - 2 * ZMM_SZ);
        z7 = _mm512_loadu_si512(mem1 + size - 1 * ZMM_SZ);
        ret = _mm512_cmpeq_epu8_mask(z2, z6) + 1;
        if (ret != 0)
            {
                index = _tzcnt_u64(ret) + size - 2 * ZMM_SZ;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        ret = _mm512_cmpeq_epu8_mask(z3, z7) + 1;
        if (ret != 0)
            {
                index = _tzcnt_u64(ret) + size - ZMM_SZ;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        return 0;
    }
    size -= 4 * ZMM_SZ;
    while (size > offset)
    {
        z0 = _mm512_stream_load_si512((void *)mem2 + offset + 0 * ZMM_SZ);
        z4 = _mm512_stream_load_si512((void *)mem1 + offset + 0 * ZMM_SZ);
        ret = _mm512_cmpeq_epu8_mask(z0, z4) + 1;
        if (ret != 0)
            {
                index = _tzcnt_u64(ret) + offset;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        z1 = _mm512_stream_load_si512((void *)mem2 + offset + 1 * ZMM_SZ);
        z5 = _mm512_stream_load_si512((void *)mem1 + offset + 1 * ZMM_SZ);
        ret = _mm512_cmpeq_epu8_mask(z1, z5) + 1;
        if (ret != 0)
            {
                index = _tzcnt_u64(ret) + offset + ZMM_SZ;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        z2 = _mm512_stream_load_si512((void *)mem2 + offset + 2 * ZMM_SZ);
        z6 = _mm512_stream_load_si512((void *)mem1 + offset + 2 * ZMM_SZ);
        ret = _mm512_cmpeq_epu8_mask(z2, z6) + 1;
        if (ret != 0)
            {
                index = _tzcnt_u64(ret) + offset + 2 * ZMM_SZ;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        z3 = _mm512_stream_load_si512((void *)mem2 + offset + 3 * ZMM_SZ);
        z7 = _mm512_stream_load_si512((void *)mem1 + offset + 3 * ZMM_SZ);
        ret = _mm512_cmpeq_epu8_mask(z3, z7) + 1;
        if (ret != 0)
            {
                index = _tzcnt_u64(ret) + offset + 3 * ZMM_SZ;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        offset += 4 * ZMM_SZ;
    }
    size += 4 * ZMM_SZ;
    if ( offset == size)
        return 0;

    z0 = _mm512_loadu_si512(mem2 + size - 4 * ZMM_SZ);
    z4 = _mm512_loadu_si512(mem1 + size - 4 * ZMM_SZ);
    ret = _mm512_cmpeq_epu8_mask(z0, z4) + 1;
    if (ret != 0)
    {
        index = _tzcnt_u64(ret) + size - 4 * ZMM_SZ;
        return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
    }
    z1 = _mm512_loadu_si512(mem2 + size - 3 * ZMM_SZ);
    z5 = _mm512_loadu_si512(mem1 + size - 3 * ZMM_SZ);
    ret = _mm512_cmpeq_epu8_mask(z1, z5) + 1;
    if (ret != 0)
    {
        index = _tzcnt_u64(ret) + size - 3 * ZMM_SZ;
        return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
    }
    z2 = _mm512_loadu_si512(mem2 + size - 2 * ZMM_SZ);
    z6 = _mm512_loadu_si512(mem1 + size - 2 * ZMM_SZ);
    ret = _mm512_cmpeq_epu8_mask(z2, z6) + 1;
    if (ret != 0)
    {
        index = _tzcnt_u64(ret) + size - 2 * ZMM_SZ;
        return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
    }
    z3 = _mm512_loadu_si512(mem2 + size - 1 * ZMM_SZ);
    z7 = _mm512_loadu_si512(mem1 + size - 1 * ZMM_SZ);
    ret = _mm512_cmpeq_epu8_mask(z3, z7) + 1;
    if (ret != 0)
    {
        index = _tzcnt_u64(ret) + size - 1 * ZMM_SZ;
        return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
    }
    return 0;
}

int __memcmp_avx512_aligned(const void *mem1, const void *mem2, size_t size)
{
    __m512i z0, z1, z2, z3, z4, z5, z6, z7;
    size_t offset = 0, index = 0;
    int64_t ret;

    LOG_INFO("\n");

    if (size <= 2 * ZMM_SZ)
	return memcmp_le_2zmm(mem1, mem2, size);

    if (size <= 4 * ZMM_SZ)
    {
        z0 = _mm512_load_si512(mem2 + 0 * ZMM_SZ);
        z1 = _mm512_load_si512(mem2 + 1 * ZMM_SZ);
        z4 = _mm512_load_si512(mem1 + 0 * ZMM_SZ);
        z5 = _mm512_load_si512(mem1 + 1 * ZMM_SZ);
        ret = _mm512_cmpeq_epu8_mask(z0, z4) + 1;
        if (ret != 0)
            {
                index = _tzcnt_u64(ret);
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        ret = _mm512_cmpeq_epu8_mask(z1, z5) + 1;
        if (ret != 0)
            {
                index = _tzcnt_u64(ret) + ZMM_SZ;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        z2 = _mm512_loadu_si512(mem2 + size - 2 * ZMM_SZ);
        z3 = _mm512_loadu_si512(mem2 + size - 1 * ZMM_SZ);
        z6 = _mm512_loadu_si512(mem1 + size - 2 * ZMM_SZ);
        z7 = _mm512_loadu_si512(mem1 + size - 1 * ZMM_SZ);
        ret = _mm512_cmpeq_epu8_mask(z2, z6) + 1;
        if (ret != 0)
            {
                index = _tzcnt_u64(ret) + size - 2 * ZMM_SZ;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        ret = _mm512_cmpeq_epu8_mask(z3, z7) + 1;
        if (ret != 0)
            {
                index = _tzcnt_u64(ret) + size - ZMM_SZ;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        return 0;
    }
    size -= 4 * ZMM_SZ;
    while (size > offset)
    {
        z0 = _mm512_load_si512(mem2 + offset + 0 * ZMM_SZ);
        z4 = _mm512_load_si512(mem1 + offset + 0 * ZMM_SZ);
        ret = _mm512_cmpeq_epu8_mask(z0, z4) + 1;
        if (ret != 0)
            {
                index = _tzcnt_u64(ret) + offset;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        z1 = _mm512_load_si512(mem2 + offset + 1 * ZMM_SZ);
        z5 = _mm512_load_si512(mem1 + offset + 1 * ZMM_SZ);
        ret = _mm512_cmpeq_epu8_mask(z1, z5) + 1;
        if (ret != 0)
            {
                index = _tzcnt_u64(ret) + offset + ZMM_SZ;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        z2 = _mm512_load_si512(mem2 + offset + 2 * ZMM_SZ);
        z6 = _mm512_load_si512(mem1 + offset + 2 * ZMM_SZ);
        ret = _mm512_cmpeq_epu8_mask(z2, z6) + 1;
        if (ret != 0)
            {
                index = _tzcnt_u64(ret) + offset + 2 * ZMM_SZ;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        z3 = _mm512_load_si512(mem2 + offset + 3 * ZMM_SZ);
        z7 = _mm512_load_si512(mem1 + offset + 3 * ZMM_SZ);
        ret = _mm512_cmpeq_epu8_mask(z3, z7) + 1;
        if (ret != 0)
            {
                index = _tzcnt_u64(ret) + offset + 3 * ZMM_SZ;
            return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
        }
        offset += 4 * ZMM_SZ;
    }
    size += 4 * ZMM_SZ;
    if ( offset == size)
        return 0;

    z0 = _mm512_loadu_si512(mem2 + size - 4 * ZMM_SZ);
    z4 = _mm512_loadu_si512(mem1 + size - 4 * ZMM_SZ);
    ret = _mm512_cmpeq_epu8_mask(z0, z4) + 1;
    if (ret != 0)
    {
        index = _tzcnt_u64(ret) + size - 4 * ZMM_SZ;
        return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
    }
    z1 = _mm512_loadu_si512(mem2 + size - 3 * ZMM_SZ);
    z5 = _mm512_loadu_si512(mem1 + size - 3 * ZMM_SZ);
    ret = _mm512_cmpeq_epu8_mask(z1, z5) + 1;
    if (ret != 0)
    {
        index = _tzcnt_u64(ret) + size - 3 * ZMM_SZ;
        return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
    }
    z2 = _mm512_loadu_si512(mem2 + size - 2 * ZMM_SZ);
    z6 = _mm512_loadu_si512(mem1 + size - 2 * ZMM_SZ);
    ret = _mm512_cmpeq_epu8_mask(z2, z6) + 1;
    if (ret != 0)
    {
        index = _tzcnt_u64(ret) + size - 2 * ZMM_SZ;
        return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
    }
    z3 = _mm512_loadu_si512(mem2 + size - 1 * ZMM_SZ);
    z7 = _mm512_loadu_si512(mem1 + size - 1 * ZMM_SZ);
    ret = _mm512_cmpeq_epu8_mask(z3, z7) + 1;
    if (ret != 0)
    {
        index = _tzcnt_u64(ret) + size - 1 * ZMM_SZ;
        return ((*(uint8_t*)(mem1 + index)) - (*(uint8_t*)(mem2 + index)));
    }
    return 0;
}
