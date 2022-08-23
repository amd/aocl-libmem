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
    char *src, *src_alnd;
    char *dst, *dst_alnd;
    char test_mode = 'v';
    unsigned int offset, src_alignment = 64, dst_alignment = 64;

    if (argc < 3 || argv[1] == NULL || argv[2] == NULL)
    {
        printf("ERROR: length of memcpy not provided\n");
        exit(0);
    }
    test_mode = *argv[1];
    len = strtoul(argv[2], &ptr, 10);
    if (argv[3] != NULL)
    {
        src_alignment = atoi(argv[3]);
    }
    if (argv[4] != NULL)
        dst_alignment = atoi(argv[4]);

    src = (char *) malloc(len + src_alignment);
    if (src == NULL)
    {
        perror("SRC allocation failure\n");
        exit(-1);
    }
    // compute aligned address for src
    if (src_alignment == 0)
        src_alignment = 64;
    if (dst_alignment == 0)
        dst_alignment = 64;
    offset = (uint64_t)src & (src_alignment-1);
    src_alnd = (offset==0)?src:(src+src_alignment-offset);

    dst = (char *) malloc(len + dst_alignment);
    if (dst == NULL)
    {
        perror("DST allocation failure\n");
        free(src);
        exit(-1);
    }
    // compute aligned address for dst
    offset = (uint64_t)dst & (dst_alignment-1);
    dst_alnd = (offset==0)?dst:(dst+dst_alignment-offset);

    if (test_mode == 'v')
    {
        uint64_t i;

        for (i=0; i< len; i++)
            *(src_alnd + i) = 'a' + rand()%26;

        memcpy(dst_alnd , src_alnd, len);

        for (i=0; i<len; i++)
        {
            if (*(src_alnd + i) != *(dst_alnd + i))
            {
                printf("ERROR: Validation failed at index %lu : src = %c, "
                        "dst = %c\n", i, *(src_alnd + i), *(dst_alnd + i));
                break;
            }
        }
        if (i == len)
        {
            printf("Validation successfull for size = %lu with SRC alignment = %u,"
                " DST alignment = %u\n", len, src_alignment, dst_alignment);
        }
    }
    else //Latency Measure test
    {   //dummy call to avoid resolver latency on first call
        memcpy(dst_alnd,src_alnd, 0);

        //rdtscp
        clk_start = rdtscp_start();
        memcpy(dst_alnd , src_alnd, len);
        clk_end = rdtscp_end();
        diff = clk_end - clk_start;//nano secs
        printf("%lu,%u,%u,%lu\n", len, src_alignment, dst_alignment, diff);
    }

    free(src);
    free(dst);
}
