/* Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
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


#include <stdlib.h>
#include "../include/zen_cpu_info.h"

#define     REQ_IDX         1

#define     REQ_TYPE_ARCH   1
/* ZEN Architecture flags */
#define     ARCH_ZEN1       1
#define     ARCH_ZEN2       2
#define     ARCH_ZEN3       3
#define     ARCH_ZEN4       4
#define     ARCH_ZEN5       5
#define     ARCH_UNDF       255

#define     REQ_TYPE_FEATURE    2

#define     REQ_FEATURE_IDX     2
/* ZEN CPU feature flags */
#define     HAS_AVX2        0
#define     HAS_ERMS        1
#define     HAS_FSRM        2
#define     HAS_AVX512      3
#define     HAS_MOVDIRI     4

#define     REQ_TYPE_CACHE  3

#define     REQ_CACHE_IDX   2
#define     L1_DCACHE_CORE  1
#define     L2_CACHE_CORE   2
#define     L3_CACHE_CCX    3

void __get_cpu_id(cpuid_registers *cpuid_regs)
{
    asm volatile
    (
     "cpuid"
     :"=a"(cpuid_regs->eax), "=b"(cpuid_regs->ebx), "=c"(cpuid_regs->ecx), "=d"(cpuid_regs->edx)
     :"0"(cpuid_regs->eax), "2"(cpuid_regs->ecx)
    );
}

int main(int argc, char **argv)
{
    cpuid_registers cpuid_regs;
    uint8_t req_feature, req_cache;

    uint8_t req_type = atoi(argv[REQ_IDX]);

    switch (req_type)
    {
        case REQ_TYPE_ARCH:
        cpuid_regs.eax= 0x7;
        cpuid_regs.ecx = 0;
        __get_cpu_id(&cpuid_regs);

        if (cpuid_regs.ebx & AVX512_MASK)
        {
            if (cpuid_regs.ecx & MOVDIRI_MASK)
                return ARCH_ZEN5;
            return ARCH_ZEN4;
        }
        else if (cpuid_regs.ebx & AVX2_MASK)
        {
            if (cpuid_regs.ecx & VPCLMULQDQ_MASK)
                return ARCH_ZEN3;
            if (cpuid_regs.ecx & RDPID_MASK)
                return ARCH_ZEN2;
            if (cpuid_regs.ebx & RDSEED_MASK)
                return ARCH_ZEN1;
        }
        return ARCH_UNDF;

        case REQ_TYPE_FEATURE:
        req_feature = atoi(argv[REQ_FEATURE_IDX]);
        cpuid_regs.eax= 0x7;
        cpuid_regs.ecx = 0;
        __get_cpu_id(&cpuid_regs);

        switch (req_feature)
        {
            case HAS_AVX512:
                return !!(cpuid_regs.ebx & AVX512_MASK);
            case HAS_AVX2:
                return !!(cpuid_regs.ebx & AVX2_MASK);
            case HAS_ERMS:
                return !!(cpuid_regs.ebx & ERMS_MASK);
            case HAS_FSRM:
                return !!(cpuid_regs.edx & FSRM_MASK);
            case HAS_MOVDIRI:
                return !!(cpuid_regs.ecx & MOVDIRI_MASK);
            default:
                return -1;
        }

        case REQ_TYPE_CACHE:
        req_cache = atoi(argv[REQ_CACHE_IDX]);
        cpuid_regs.eax = 0x8000001D;
        uint8_t shifter = 0;
        switch (req_cache)
        {
            case L1_DCACHE_CORE:
                cpuid_regs.ecx = 0x0;
                shifter = 10; // size reported in multiples of KB
                break;
            case L2_CACHE_CORE:
                cpuid_regs.ecx = 0x2;
                shifter = 19; // size reported in multiple of 512 KB
                break;
            case L3_CACHE_CCX:
                cpuid_regs.ecx = 0x3;
                shifter = 20; // size reported in multiple of MB
                break;
            default:
                return -1;
        }
        __get_cpu_id(&cpuid_regs);

        return ((((cpuid_regs.ebx>>22) & 0x3ff) + 1) * \
                  (((cpuid_regs.ebx & 0xfff) + 1) * (cpuid_regs.ecx + 1))) >> shifter;
    }
    return -1;
}
