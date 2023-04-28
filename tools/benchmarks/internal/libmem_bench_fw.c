/* Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
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
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>

#define CACHE_LINE_SZ   64
#define PAGE_SZ         4 * 1024
#define PAGE_SZ_BITS    12

#define WALK_STEPS      100
#define PAGE_WALK_STEPS 10

#define SMALL_SIZED_ITERATIONS 100000000
#define MID_SIZED_ITERATIONS 10000
#define LARGE_SIZED_ITERATIONS 1000

typedef enum {
    UNCACHED=0,
    CACHED,
    WALK,
    PAGE_WALK
} bench_mode;

typedef struct
{
    const char  mode;
    const char  *bench_mode_name;
    bench_mode  b_mode;
} libmem_bench;

typedef struct
{
    const char *func_name;
    void  (*func)(uint8_t *, uint8_t *, size_t);
} libmem_func;


static inline void memcpy_wrapper(uint8_t *dst, uint8_t *src, size_t size)
{
    memcpy(dst, src, size);
}

static inline void mempcpy_wrapper(uint8_t * dst, uint8_t * src, size_t size)
{
    mempcpy(dst, src, size);
}

static inline void memmove_wrapper(uint8_t * dst, uint8_t * src, size_t size)
{
    memmove(dst, src, size);
}

static inline void memset_wrapper(uint8_t * dst, uint8_t * src, size_t size)
{
    memset(dst, (int)src[0], size);
}

static inline void memcmp_wrapper(uint8_t * dst, uint8_t * src, size_t size)
{
    memcmp(dst, src, size);
}

static inline void strcpy_wrapper(uint8_t *dst, uint8_t *src, size_t size)
{
    *(src + size -1) = '\0';
    strcpy(dst, src);
}


libmem_bench supp_modes[]=
{
    {'u',   "uncached",   UNCACHED},
    {'c',   "cached",     CACHED},
    {'w',   "walk",       WALK},
    {'p',   "page_walk",  PAGE_WALK}
};

libmem_func supp_funcs[]=
{
    {"memcpy",  memcpy_wrapper},
    {"mempcpy", mempcpy_wrapper},
    {"memmove", memmove_wrapper},
    {"memset",  memset_wrapper},
    {"memcmp",  memcmp_wrapper},
    {"strcpy",  strcpy_wrapper},
    {"none",    NULL}
};

#define BENCHMARK_FUNC(func, dst, src, size)    \
        clk_start = rdtscp_start();             \
        func(dst, src, size);                   \
        clk_end = rdtscp_end();                 \
        diff += clk_end - clk_start;

/*
 * RDTSCP instruction is more deterministic than RDTSC.
 * Use rdtsc_start before the function/api to be benchmarked.
 */
static inline int64_t rdtscp_start(void)
{
    unsigned a, d;
    asm volatile("cpuid" ::: "%rax", "%rbx", "%rcx", "%rdx");
    asm volatile("rdtsc" : "=a" (a), "=d" (d));
    return ((uint64_t)a) | (((uint64_t)d) << 32);
}

static inline int64_t rdtscp_end(void)
{
    unsigned a, d;
    asm volatile("rdtscp" : "=a" (a), "=d" (d));
    asm volatile("cpuid" ::: "%rax", "%rbx", "%rcx", "%rdx");
    return ((uint64_t)a) | (((uint64_t)d) << 32);
}

static inline uint8_t * alloc_buffer(size_t size, bench_mode b_mode)
{
    void *buff_addr;
    switch (b_mode)
    {
        case WALK: //size below 4KB
            posix_memalign(&buff_addr, CACHE_LINE_SZ, WALK_STEPS * \
                   (size + CACHE_LINE_SZ));
            break;
        case PAGE_WALK: // size below 4KB
            posix_memalign(&buff_addr, CACHE_LINE_SZ, \
                    PAGE_WALK_STEPS * ((size >> PAGE_SZ_BITS) + 1) * PAGE_SZ);
            break;
        default:
            posix_memalign(&buff_addr, CACHE_LINE_SZ, size + CACHE_LINE_SZ);
    }
    return (uint8_t *)buff_addr;
}

static inline uint8_t * align_addr(uint8_t alignment, uint8_t * addr)
{
    unsigned int offset;
    uint8_t * aligned_addr;

    alignment = alignment?alignment:CACHE_LINE_SZ;

    offset = (uint64_t)addr & (CACHE_LINE_SZ - 1);
    aligned_addr = (offset == 0) ? addr : (addr + alignment - offset);

    return aligned_addr;
}

int main(int argc, char **argv)
{
    uint64_t diff = 0, clk_start, clk_end;
    uint64_t size, start_size, end_size;
    char *ptr;
    uint8_t *src = NULL, *src_alnd = NULL;
    uint8_t *dst = NULL, *dst_alnd = NULL;
    unsigned int offset, src_alignment = 0, dst_alignment = 0;
    libmem_func *lm_func = &supp_funcs[0]; //default func is memcpy
    libmem_bench *lm_bench = &supp_modes[0];//default mode is uncached
    size_t iterator= 0;

    if (argc < 8)
    {
        if(argv[1] == NULL)
        {
            printf("ERROR: Function name not provided\n");
            exit(0);
        }
        if(argv[2] == NULL | argv[3] == NULL)
        {
            printf("ERROR: Size Range provided\n");
            exit(0);
        }
    }

    for (int idx = 0; idx <= sizeof(supp_funcs)/sizeof(supp_funcs[0]); idx++)
    {
        if (!strcmp(supp_funcs[idx].func_name, argv[1]))
        {
            lm_func = &supp_funcs[idx];
            break;
        }
    }

    start_size = strtoul(argv[2], &ptr, 10);
    end_size = strtoul(argv[3], &ptr, 10);

    if (argv[4] != NULL)
        src_alignment = atoi(argv[4]);

    if (argv[5] != NULL)
        dst_alignment = atoi(argv[5]);

    if (argv[6] != NULL)
    for (int idx = 0; idx <= sizeof(supp_modes)/sizeof(supp_modes[0]); idx++)
    {
        if (*argv[6] == supp_modes[idx].mode)
        {
            lm_bench = &supp_modes[idx];
            break;
        }
    }

    if (argv[7] != NULL)
        iterator = strtoul(argv[7], &ptr, 10);

    size_t max_iters;

    for (size_t size = start_size; size <= end_size; size =
            (size + iterator)*(iterator != 0) + (size << 1)*(iterator == 0))
    {
        src = alloc_buffer(size, lm_bench->b_mode);
        if (src == NULL)
        {
            perror("SRC allocation failure\n");
            exit(-1);
        }
        dst = alloc_buffer(size, lm_bench->b_mode);
        if (dst == NULL)
        {
            perror("DST allocation failure\n");
            free(src);
            exit(-1);
        }
        // get aligned addresses
        src_alnd = src + src_alignment;
        dst_alnd = dst + dst_alignment;
        //dummy call to avoid resolver latency on first call
        lm_func->func(dst_alnd, src_alnd, 0);

        switch(lm_bench->b_mode)
        {
        case UNCACHED:
            BENCHMARK_FUNC(lm_func->func, src_alnd, dst_alnd, size);
            printf("%lu,", diff);
            break;
        case CACHED:
            if (size < 1*1024*1024)
         max_iters = SMALL_SIZED_ITERATIONS;
            else if (size < 32*1024*1024)
             max_iters = MID_SIZED_ITERATIONS;
            else
             max_iters = LARGE_SIZED_ITERATIONS;
            for (size_t iter = 0; iter < max_iters; iter++)
               BENCHMARK_FUNC(lm_func->func, src_alnd, dst_alnd, size);
            printf("%lu,", diff/max_iters);
            break;
        case WALK:
            for (int steps = 0; steps < WALK_STEPS; steps++)
            {
                src_alnd = align_addr(src_alignment, src + \
                                    steps*(size + CACHE_LINE_SZ));
                dst_alnd = align_addr(dst_alignment, dst + \
                                    steps*(size + CACHE_LINE_SZ));

                BENCHMARK_FUNC(lm_func->func, src_alnd, dst_alnd, size);
            }
            printf("%lu,", diff/WALK_STEPS);
            break;
        case PAGE_WALK:
            diff = 0;
            for (int steps = 0; steps < PAGE_WALK_STEPS; steps++)
            {
                src_alnd = src + steps * ((size >> PAGE_SZ_BITS) + 1) * PAGE_SZ;
                dst_alnd = dst + steps * ((size >> PAGE_SZ_BITS) + 1) * PAGE_SZ;

                BENCHMARK_FUNC(lm_func->func, src_alnd, dst_alnd, size);
            }
            printf("%lu,", diff/PAGE_WALK_STEPS);
            break;
        }
        free(src);
        free(dst);
    }
        printf("\n");
}
