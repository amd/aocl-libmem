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
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>

#define BILLION 1000000000L

/*
 * RDTSCP instruction is more deterministic than RDTSC.
 * Use rdtsc_start before the function/api to be benchmarked.
 */
static __inline__ int64_t rdtscp_start(void)
{
    unsigned a, d;
    asm volatile("cpuid" ::: "%rax", "%rbx", "%rcx", "%rdx");
    asm volatile("rdtsc" : "=a" (a), "=d" (d));
    return ((uint64_t)a) | (((uint64_t)d) << 32);
}

static __inline__ int64_t rdtscp_end(void)
{
    unsigned a, d;
    asm volatile("rdtscp" : "=a" (a), "=d" (d));
    asm volatile("cpuid" ::: "%rax", "%rbx", "%rcx", "%rdx");
    return ((uint64_t)a) | (((uint64_t)d) << 32);
}

int main(int argc, char **argv)
{
    uint64_t diff;
    uint64_t len, clk_start, clk_end;
    char *ptr;
    char *mem, *mem_alnd;
    int val = 'c';
    char test_mode = 'v';
    unsigned int offset, mem_alignment = 32;

    if (argc < 3 || argv[1] == NULL || argv[2] == NULL)
    {
        printf("ERROR: length of memset not provided\n");
        exit(0);
    }
    test_mode = *argv[1];
    len = strtoul(argv[2], &ptr, 10);
    if (argv[3] != NULL)
    {
        mem_alignment = atoi(argv[3])%32;
    }

    mem = (char *) malloc(len + 32);
    if (mem == NULL)
    {
        perror("SRC allocation failure\n");
        exit(-1);
    }
    // compute aligned address for mem
    mem_alnd = (char *)(mem + mem_alignment);
    offset = (uint64_t)mem & 32;
    if (offset != 0)
        mem_alnd += 32 - offset;

    if (test_mode == 'v')
    {
        uint64_t i;
        
        for (i=0; i< len; i++)
            *(mem_alnd + i) = 'a' + rand()%26;

        memset(mem_alnd, val, len);

        for (i=0; i<len; i++)
            if (*(mem_alnd + i) != val)
            {
                printf("ERROR: Validation failed at index %lu : mem = %c, "
                        "dst = %c\n", i, *(mem_alnd + i), val);
                break;
            }
        if (i == len)
            printf("Validation successfull for size = %lu with MEM alignment = %u,"
                " value = %c\n", len, mem_alignment, val);
    }
    else //Latency Measure test
    {   //dummy call to avoid resolver latency on first call
        memset(mem_alnd, val, 0);

        //rdtscp
        clk_start = rdtscp_start();
        memset(mem_alnd, val, len);
        clk_end = rdtscp_end();
        diff = clk_end - clk_start;//nano secs
        printf("%lu,%u,%d,%lu\n", len, mem_alignment, val, diff);
    }

    free(mem);
}
