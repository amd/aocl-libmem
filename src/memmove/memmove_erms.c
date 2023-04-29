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
#include <stddef.h>
#include "../base_impls/load_store_erms_impls.h"
#include "logger.h"
#include <stdint.h>
// memmove with byte rep move instruciton:REP MOVSB
void * __memmove_erms_b_aligned(void *dst, const void *src, size_t size)
{
    LOG_INFO("\n");
    if (src > dst)
        __erms_movsb(dst, src, size);
    else
        __erms_movsb_back(dst, src, size);
    return dst;
}

// memmove with word rep move instruciton:REP MOVSW
void * __memmove_erms_w_aligned(void *dst, const void *src, size_t size)
{
    LOG_INFO("\n");
    if (src > dst)
        __erms_movsw(dst, src, size);
    else
        __erms_movsw_back(dst, src, size);
    return dst;
}

// memmove with double word rep move instruciton:REP MOVSD
void * __memmove_erms_d_aligned(void *dst, const void *src, size_t size)
{
    LOG_INFO("\n");
    if (src > dst)
        __erms_movsd(dst, src, size);
    else
        __erms_movsd_back(dst, src, size);
    return dst;
}

// memmove with quad word rep move instruciton:REP MOVSQ
void * __memmove_erms_q_aligned(void *dst, const void *src, size_t size)
{
    LOG_INFO("\n");
    if (src > dst)
        __erms_movsq(dst, src, size);
    else
        __erms_movsq_back(dst, src, size);
    return dst;
}

