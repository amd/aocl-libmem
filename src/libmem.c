/* Copyright (C) 2022-24 Advanced Micro Devices, Inc. All rights reserved.
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
#ifdef ALMEM_DYN_DISPATCH
#include "libmem_impls.h"
#include "libmem.h"
#endif

cpu_info zen_info;
config active_operation_cfg = SYS_CFG;
config active_threshold_cfg = SYS_CFG;
extern user_cfg user_config;

static bool is_amd()
{
    cpuid_registers cpuid_regs;

    cpuid_regs.eax= 0x0;
    cpuid_regs.ecx = 0;
    __get_cpu_features(&cpuid_regs);
    if ((cpuid_regs.ebx ^ 0x68747541) |
            (cpuid_regs.edx ^ 0x69746E65) |
                 (cpuid_regs.ecx ^ 0x444D4163))
    {
        return false;
    }
    return true;
}

static void get_cpu_capabilities()
{
    cpuid_registers cpuid_regs;

    cpuid_regs.eax= 0x7;
    cpuid_regs.ecx = 0;
    __get_cpu_features(&cpuid_regs);

    if (cpuid_regs.ebx & AVX512_MASK)
    {
        zen_info.zen_cpu_features.avx512 = ENABLED;
        LOG_INFO("CPU feature AVX512 Enabled\n");
    }
    if (cpuid_regs.ebx & AVX2_MASK)
    {
        zen_info.zen_cpu_features.avx2 = ENABLED;
        LOG_INFO("CPU feature AVX2 Enabled\n");
    }
    if (cpuid_regs.ebx & ERMS_MASK)
    {
        zen_info.zen_cpu_features.erms = ENABLED;
        LOG_INFO("CPU feature ERMS Enabled\n");
    }
    if (cpuid_regs.edx & FSRM_MASK)
    {
        zen_info.zen_cpu_features.fsrm = ENABLED;
        LOG_INFO("CPU feature FSRM Enabled\n");
    }
    if (cpuid_regs.ecx & MOVDIRI_MASK)
    {
        zen_info.zen_cpu_features.movdiri = ENABLED;
        LOG_INFO("CPU feature MOVDIRI Enabled\n");
    }
    if (cpuid_regs.ecx & VPCLMULQDQ_MASK)
    {
        zen_info.zen_cpu_features.vpclmul = ENABLED;
        LOG_INFO("CPU feature VPCLMULQDQ Enabled\n");
    }
    if (cpuid_regs.ecx & RDPID_MASK)
    {
        zen_info.zen_cpu_features.rdpid = ENABLED;
        LOG_INFO("CPU feature RDPID Enabled\n");
    }
    if (cpuid_regs.ecx & RDSEED_MASK)
    {
        zen_info.zen_cpu_features.rdseed = ENABLED;
        LOG_INFO("CPU feature RDSEED Enabled\n");
    }

}

#ifdef ALMEM_DYN_DISPATCH
/* Resolver to identify the zen cpu version
 * returns: cpu version index
 */
static variant_index libmem_cpu_resolver(void)
{
    variant_index cpu_ver_idx = SYSTEM;
    // Zen4 and above cpu detection
    if (zen_info.zen_cpu_features.avx512 == ENABLED)
    {
        if (zen_info.zen_cpu_features.movdiri == ENABLED)// Zen5
        {
            cpu_ver_idx = ARCH_ZEN5;
            LOG_INFO("Detected CPU uArch: Zen5\n");
        }
        else
        {
            cpu_ver_idx = ARCH_ZEN4;
            LOG_INFO("Detected CPU uArch: Zen4\n");
        }
    }
    // Zen3 and below cpu detection
    else if (zen_info.zen_cpu_features.avx2 == ENABLED)
    {
        if (zen_info.zen_cpu_features.vpclmul == ENABLED) // Zen3
        {
            cpu_ver_idx = ARCH_ZEN3;
            LOG_INFO("Detected CPU uArch: Zen3\n");
        }
        else if (zen_info.zen_cpu_features.rdpid == ENABLED) // Zen2
        {
            cpu_ver_idx = ARCH_ZEN2;
            LOG_INFO("Detected CPU uArch: Zen2\n");
        }
        else if (zen_info.zen_cpu_features.rdseed == ENABLED) // Zen1
        {
            cpu_ver_idx = ARCH_ZEN1;
            LOG_INFO("Detected CPU uArch: Zen1\n");
        }
        else
        {
            //System operation Config
            LOG_INFO("System Operation CFG\n");
            cpu_ver_idx = SYSTEM;
        }
    }
    return cpu_ver_idx;
}
#endif // end of cpu resolver

#ifdef ALMEM_TUNABLES
/* Resolver to identify the tunable config
 * returns: tunable varaint index
 */
static variant_index libmem_tunable_resolver(void)
{
    variant_index tunable_var_idx = SYSTEM;

    if (active_operation_cfg == USR_CFG) //User Operation Config
    {
        LOG_INFO("User Operation Config\n");
        if (user_config.user_operation.avx2) //AVX2 operations
        {
            LOG_DEBUG("AVX2 config\n");
            if (user_config.src_aln == n_align)
            {
                if (user_config.dst_aln == n_align)
                    tunable_var_idx = AVX2_NON_TEMPORAL;
                else
                    tunable_var_idx = AVX2_NON_TEMPORAL_LOAD;
            }
            else if (user_config.dst_aln == n_align)
                tunable_var_idx = AVX2_NON_TEMPORAL_STORE;
            else if (user_config.src_aln == y_align)
            {
                if (user_config.dst_aln == y_align)
                    tunable_var_idx = AVX2_ALIGNED;
                else
                    tunable_var_idx = AVX2_ALIGNED_LOAD;
            }
            else if (user_config.dst_aln == y_align)
                tunable_var_idx = AVX2_ALIGNED_STORE;
            else
                tunable_var_idx = AVX2_UNALIGNED;
        }
        else if (user_config.user_operation.avx512) //AVX512 operations
        {
            LOG_DEBUG("AVX512 config\n");
            if (user_config.src_aln == n_align)
            {
                if (user_config.dst_aln == n_align)
                    tunable_var_idx = AVX512_NON_TEMPORAL;
                else
                    tunable_var_idx = AVX512_NON_TEMPORAL_LOAD;
            }
            else if (user_config.dst_aln == n_align)
                tunable_var_idx = AVX512_NON_TEMPORAL_STORE;
            else if (user_config.src_aln == y_align)
            {
                if (user_config.dst_aln == y_align)
                    tunable_var_idx = AVX512_ALIGNED;
                else
                    tunable_var_idx = AVX512_ALIGNED_LOAD;
            }
            else if (user_config.dst_aln == y_align)
                tunable_var_idx = AVX512_ALIGNED_STORE;
            else
                tunable_var_idx = AVX512_UNALIGNED;
        }
        else if (user_config.user_operation.erms) //ERMS operations
        {
            LOG_DEBUG("ERMS config\n");
            tunable_var_idx = ERMS_MOVSB;
            if (user_config.src_aln == user_config.dst_aln)
            {
                if (user_config.src_aln == q_align \
                    || user_config.src_aln == x_align \
                    || user_config.src_aln == y_align)
                    tunable_var_idx = ERMS_MOVSQ;
                else if (user_config.src_aln == d_align)
                    tunable_var_idx = ERMS_MOVSD;
                else if (user_config.src_aln == w_align)
                    tunable_var_idx = ERMS_MOVSW;
            }
        }
    }
    else if (active_threshold_cfg == USR_CFG) // User Threshold Config
    {
        LOG_INFO("User Threshold CFG.\n");
        tunable_var_idx = THRESHOLD;
    }
    return tunable_var_idx;
}
#endif // end of tunable resolver


/* Constructor for libmem library
 * returns: void
 */
static __attribute__((constructor)) void libmem_init()
{
    LOG_INFO("aocl-libmem Version: %s\n", LIBMEM_BUILD_VERSION);
    bool is_amd_cpu = is_amd();
    if (is_amd_cpu == true)
    {
        LOG_INFO("Is AMD CPU\n");
#ifdef ALMEM_TUNABLES
        parse_env_operation_cfg();
        if (active_operation_cfg == SYS_CFG)
            parse_env_threshold_cfg();
#endif
        if (active_operation_cfg == SYS_CFG && active_threshold_cfg == SYS_CFG)
            compute_sys_thresholds(&zen_info);

        configure_thresholds();
        get_cpu_capabilities();
    }

#ifdef ALMEM_DYN_DISPATCH
    variant_index cpu_ver_idx = SYSTEM;
    if (is_amd_cpu == true)
        cpu_ver_idx  = libmem_cpu_resolver();

    _memcpy_variant = ((void* (*)(void *, const void *, size_t))libmem_impls[MEMCPY][cpu_ver_idx]);
    _mempcpy_variant = ((void* (*)(void *, const void *, size_t))libmem_impls[MEMPCPY][cpu_ver_idx]);
    _memmove_variant = ((void* (*)(void *, const void *, size_t))libmem_impls[MEMMOVE][cpu_ver_idx]);
#endif //end of dynamic dispatching
#ifdef ALMEM_TUNABLES
    variant_index tunable_var_idx = libmem_tunable_resolver();
    //if tunables are not valid go ahead with detected cpu implementation
    if (tunable_var_idx == SYSTEM)
        tunable_var_idx = cpu_ver_idx;

    _memcpy_variant = ((void* (*)(void *, const void *, size_t))libmem_impls[MEMCPY][tunable_var_idx]);
    _mempcpy_variant = ((void* (*)(void *, const void *, size_t))libmem_impls[MEMPCPY][tunable_var_idx]);
    _memmove_variant = ((void* (*)(void *, const void *, size_t))libmem_impls[MEMMOVE][tunable_var_idx]);
    _memset_variant = ((void* (*)(void *, int, size_t))libmem_impls[MEMSET][tunable_var_idx]);
    _memcmp_variant =((int (*)(const void *, const void *, size_t))libmem_impls[MEMCMP][tunable_var_idx]);
#endif //end of tunables
}
