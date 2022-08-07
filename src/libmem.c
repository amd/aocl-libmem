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
#include "zen_cpu_info.h"
#include "logger.h"
#include "threshold.h"

cpu_info zen_info;
config active_operation_cfg = SYS_CFG;
config active_threshold_cfg = SYS_CFG;
extern user_cfg user_config;

static void get_cpu_capabilities()
{
    cpuid_registers cpuid_regs;

    cpuid_regs.eax= 0x7;
    cpuid_regs.ecx = 0;
    __get_cpu_features(&cpuid_regs);

    if (cpuid_regs.ebx & AVX2_MASK)
    {
        zen_info.zen_cpu_features.avx2 = 1;
        LOG_INFO("AVX2 Enabled\n");
    }
    if (cpuid_regs.ebx & ERMS_MASK)
    {
        zen_info.zen_cpu_features.erms = 1;
        LOG_INFO("ERMS Enabled\n");
    }
    if (cpuid_regs.edx & FSRM_MASK)
    {
        zen_info.zen_cpu_features.fsrm = 1;
        LOG_INFO("FSRM Enabled\n");
    }
}

/* Constructor for libmem library
 * returns: void
 */
static __attribute__((constructor)) void libmem_init()
{
    LOG_INFO("aocl-libmem Version: 3.2.1\n");
    if (active_operation_cfg == SYS_CFG && active_threshold_cfg == SYS_CFG)
    {
        //set variant index (uArch) based on L3 cache (_zen1, _zen2, _zen3)
        //if failed to get uArch, then call system capabilities based(CPU flags)
        compute_sys_thresholds(&zen_info);
        configure_thresholds();
        get_cpu_capabilities(); // call this on failure to configure SYS or USER cfg.
    }
}
