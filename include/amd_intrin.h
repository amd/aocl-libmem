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
#ifndef _AMD_INTRIN_H_
#define _AMD_INTRIN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

inline void * __erms_movsb(void *dst, const void * src, size_t len)
{
    asm volatile (
    "cld\n\t"
    "rep movsb"
    :
    : "D"(dst), "S"(src), "c"(len)
    : "memory"
    );
    return dst;
}

inline void * __erms_movsw(void *dst, const void * src, size_t len)
{
    asm volatile (
    "sar $1, %%rcx\n\t"
    "cld\n\t"
    "rep movsw"
    :
    : "D"(dst), "S"(src), "c"(len)
    : "memory"
    );
    return dst;
}

inline void * __erms_movsd(void *dst, const void * src, size_t len)
{
    asm volatile (
    "sar $2, %%rcx\n\t"
    "cld\n\t"
    "rep movsd"
    :
    : "D"(dst), "S"(src), "c"(len)
    : "memory"
    );
    return dst;
}

inline void * __erms_movsq(void *dst, const void * src, size_t len)
{
    asm volatile (
    "sar $3, %%rcx\n\t"
    "cld\n\t"
    "rep movsq"
    :
    : "D"(dst), "S"(src), "c"(len)
    : "memory"
    );
    return dst;
}

inline void * __erms_movsb_last_byte(void *dst, const void * src, size_t len)
{
    asm volatile (
    "cld\n\t"
    "rep movsb"
    : "+D"(dst)
    : "S"(src), "c"(len)
    : "memory"
    );
    return dst;
}

inline void * __erms_movsw_last_byte(void *dst, const void * src, size_t len)
{
    asm volatile (
    "sar $1, %%rcx\n\t"
    "cld\n\t"
    "rep movsw"
    : "+D"(dst)
    : "S"(src), "c"(len)
    : "memory"
    );
    return dst;
}

inline void * __erms_movsd_last_byte(void *dst, const void * src, size_t len)
{
    asm volatile (
    "sar $2, %%rcx\n\t"
    "cld\n\t"
    "rep movsd"
    : "+D"(dst)
    : "S"(src), "c"(len)
    : "memory"
    );
    return dst;
}

inline void * __erms_movsq_last_byte(void *dst, const void * src, size_t len)
{
    asm volatile (
    "sar $3, %%rcx\n\t"
    "cld\n\t"
    "rep movsq"
    : "+D"(dst)
    : "S"(src), "c"(len)
    : "memory"
    );
    return dst;
}

inline void * __erms_movsb_back(void *dst, const void * src, size_t len)
{
    asm volatile (
    "std\n\t"
    "rep movsb"
    :
    : "D"(dst + len - 1), "S"(src + len - 1), "c"(len)
    : "memory"
    );
    return dst;
}

inline void * __erms_movsw_back(void *dst, const void * src, size_t len)
{
    asm volatile (
    "sar $1, %%rcx\n\t"
    "std\n\t"
    "rep movsw"
    :
    : "D"(dst + len - 2), "S"(src + len - 2), "c"(len)
    : "memory"
    );
    return dst;
}

inline void * __erms_movsd_back(void *dst, const void * src, size_t len)
{
    asm volatile (
    "sar $2, %%rcx\n\t"
    "std\n\t"
    "rep movsd"
    :
    : "D"(dst + len - 4), "S"(src + len - 4), "c"(len)
    : "memory"
    );
    return dst;
}

inline void * __erms_movsq_back(void *dst, const void * src, size_t len)
{
    asm volatile (
    "sar $3, %%rcx\n\t"
    "std\n\t"
    "rep movsq"
    :
    : "D"(dst + len - 8), "S"(src + len - 8), "c"(len)
    : "memory"
    );
    return dst;
}

inline void * __erms_stosb(void *mem, int val, size_t size)
{
    asm volatile (
    "cld\n\t"
    "rep stosb"
    :
    : "D"(mem), "a"(val), "c"(size)
    : "memory"
    );
    return mem;
}

inline void * __erms_stosw(void *mem, uint16_t val, size_t size)
{
    val = val | val << 8;
    size = size >> 1;

    asm volatile (
    "cld\n\t"
    "rep stosw"
    :
    : "D"(mem), "a"(val), "c"(size)
    : "memory"
    );
    return mem;
}

inline void * __erms_stosd(void *mem, uint32_t val, size_t size)
{
    val = val | val << 8;
    size = size >> 1;

    asm volatile (
    "cld\n\t"
    "rep stosw"
    :
    : "D"(mem), "a"(val), "c"(size)
    : "memory"
    );
    return mem;
}

inline void * __erms_stosq(void *mem, uint64_t val, size_t size)
{
    val = val | val << 8;
    val = val | val << 16;
    val = val | val << 32;

    size = size >> 3;

    asm volatile (
    "cld\n\t"
    "rep stosq"
    :
    : "D"(mem), "a"(val), "c"(size)
    : "memory"
    );
    return mem;
}

inline int __erms_cmpsb(const void * mem1, const void * mem2, size_t size)
{
    int ret = 1;

    asm volatile (
    "repz cmpsb\n\t"
    "cmovz %%rcx, %%rax"
    : "+a"(ret)
    : "S"(mem1), "D"(mem2), "c"(size)
    : "memory"
    );
    return ret;
}

inline int __erms_cmpsw(const void * mem1, const void * mem2, size_t size)
{
    int ret = 1;

    size = size >> 1;

    asm volatile (
    "repz cmpsw\n\t"
    "cmovz %%rcx, %%rax"
    : "+a"(ret)
    : "S"(mem1), "D"(mem2), "c"(size)
    : "memory"
    );
    return ret;
}


inline int __erms_cmpsd(const void * mem1, const void * mem2, size_t size)
{
    int ret = 1;

    size = size >> 2;

    asm volatile (
    "repz cmpsd\n\t"
    "cmovz %%rcx, %%rax"
    : "+a"(ret)
    : "S"(mem1), "D"(mem2), "c"(size)
    : "memory"
    );
    return ret;
}

inline int __erms_cmpsq(const void * mem1, const void * mem2, size_t size)
{
    int ret = 1;

    size = size >> 3;

    asm volatile (
    "repz cmpsq\n\t"
    "cmovz %%rcx, %%rax"
    : "+a"(ret)
    : "S"(mem1), "D"(mem2), "c"(size)
    : "memory"
    );
    return ret;
}

#ifdef __cplusplus
}
#endif

#endif

