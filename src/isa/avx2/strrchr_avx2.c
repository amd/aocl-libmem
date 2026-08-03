/* AVX2 wrapper for strrchr, using the optimized kernel implementation.
 */
/* Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved. */

#include "logger.h"
#include "almem_defs.h"

#include "./optimized/strrchr_avx2.c"

HIDDEN_SYMBOL char * __attribute__((flatten)) __strrchr_avx2(const char *str, int c)
{
    LOG_INFO("\n");
    return _strrchr_avx2(str, c);
}

char *strrchr(const char *, int) __attribute__((weak,
                            alias("__strrchr_avx2")));
