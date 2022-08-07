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
#include "logger.h"
#include "zen_cpu_info.h"


static void get_thread_info(thread_info *zen_thread_info)
{
    cpuid_registers cpuid_regs;
    //Logical threads in a CCD
    cpuid_regs.eax = 0x00000001;
    cpuid_regs.ecx = 0;
    __get_cpu_features(&cpuid_regs);

    zen_thread_info->threads_per_ccd = (cpuid_regs.ebx >> 16) & 0xff;

    //Logical threads sharing L3 cache(CCX)
    cpuid_regs.eax = 0x8000001D;
    cpuid_regs.ecx = 0x3;
    __get_cpu_features(&cpuid_regs);
    zen_thread_info->threads_per_ccx = ((cpuid_regs.eax >> 14) & 0xfff) + 1;

    LOG_DEBUG("Threads per CCX:%u, CCD:%u\n", zen_thread_info->threads_per_ccx, zen_thread_info->threads_per_ccd);
}

void get_cache_info(cpu_info *zen_info)
{
    cpuid_registers cpuid_regs;

    cache_info *zen_cache = &(zen_info->zen_cache_info);
    thread_info *zen_thread = &(zen_info->zen_thread_info);

    cpuid_regs.eax = 0x80000006;
    cpuid_regs.ecx = 0;
    __get_cpu_features(&cpuid_regs);

    //Compute L2 Cache Info
    zen_cache->l2_per_core = ((cpuid_regs.ecx & 0xf000) == 0 ? 0 : (cpuid_regs.ecx >> 6) & 0x3fffc00);

    //Compute L3 Cache info
    zen_cache->l3_per_ccd =  (((cpuid_regs.edx & 0xf000) == 0) ? 0 : (cpuid_regs.edx & 0x3ffc0000) << 1);

    LOG_DEBUG("L2 Cache/CORE: %lu, L3 Cache/CCD: %lu\n", zen_cache->l2_per_core, zen_cache->l3_per_ccd);

    get_thread_info(zen_thread);

    zen_cache->l3_per_ccx = ((zen_cache->l3_per_ccd)/(zen_thread->threads_per_ccd))*(zen_thread->threads_per_ccx);
    LOG_DEBUG("L3 Cache/CCX: %lu\n", zen_cache->l3_per_ccx);
}
