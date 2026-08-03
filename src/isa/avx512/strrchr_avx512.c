/* AVX-512 wrapper for strrchr, using the optimized kernel.
 * The optimized kernel currently contains only a skeleton implementation.
 */
/* Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved. */

#include "logger.h"
#include "almem_defs.h"

#include "./optimized/strrchr_avx512.c"

HIDDEN_SYMBOL char * __attribute__((flatten)) __strrchr_avx512(const char *str, int c)
{
    LOG_INFO("\n");
    return _strrchr_avx512(str, c);
}

char *strrchr(const char *, int) __attribute__((weak,
                            alias("__strrchr_avx512")));
