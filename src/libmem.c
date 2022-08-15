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
#include "libmem_impls.h"
#ifdef ENABLE_TUNABLES
#include "libmem.h"
#endif

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

#ifdef ENABLE_TUNABLES
/* Resolver to identify the implementation variant
 * returns: variant index
 */
static variant_index amd_libmem_resolver(void)
{
    variant_index var_idx = SYSTEM;
    if (active_operation_cfg == SYS_CFG && active_threshold_cfg == SYS_CFG)
    {
        active_operation_cfg = SYS_CFG;
        LOG_INFO("System Operation CFG\n");
        switch(zen_info.zen_cache_info.l3_per_ccx)
        {
            case ZEN1_L3:
                var_idx = ARC_ZEN1;
                break;
            case ZEN2_L3:
                var_idx = ARC_ZEN2;
                break;
            case ZEN3_L3:
                var_idx = ARC_ZEN3;
                break;
            default:
                //System operation/feature & computerd threshold Config
                LOG_DEBUG("Operation & Threshold CFG\n");
                var_idx = SYSTEM;
        }
    }
    return var_idx;
}
#endif


/* Constructor for libmem library
 * returns: void
 */
static __attribute__((constructor)) void libmem_init()
{
    LOG_INFO("aocl-libmem Version: 3.2.1\n");
#ifdef ENABLE_TUNABLES
    parse_env_operation_cfg();
    if (active_operation_cfg == SYS_CFG)
        parse_env_threshold_cfg();
    if (active_threshold_cfg == USR_CFG)
        configure_thresholds();
#endif
    if (active_operation_cfg == SYS_CFG && active_threshold_cfg == SYS_CFG)
    {
        //set variant index (uArch) based on L3 cache (_zen1, _zen2, _zen3, _zen4, _zen5)
        //if failed to get uArch, then call system capabilities based(CPU flags)
        compute_sys_thresholds(&zen_info);
        configure_thresholds();
        get_cpu_capabilities(); // call this on failure to configure SYS or USER cfg.
        //set variant index to system capability variant (_syscap)
    }
#ifdef ENABLE_TUNABLES
    //Call resolver to intialize the entry_fn_ptr;
    variant_index impl_idx;
    impl_idx  = amd_libmem_resolver();
    
    _memcpy_variant = libmem_impls_1[MEMCPY][impl_idx];
#endif
}
