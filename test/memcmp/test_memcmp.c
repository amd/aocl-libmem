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
    uint64_t len, clk_start, clk_end, i;
    char *ptr;
    char *src, *src_alnd;
    char *dst, *dst_alnd;
    char test_mode = 'v';
    unsigned int offset, src_alignment = 32, dst_alignment = 32;
    unsigned int validation_passed = 1;
    int ret = 0;

    if (argc < 3 || argv[1] == NULL || argv[2] == NULL)
    {
        printf("ERROR: length of memcmp not provided\n");
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
        src_alignment = 32;
    if (dst_alignment == 0)
        dst_alignment = 32;
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
        srand(time(0));
        for (i=0; i< len; i++)
        {
            *(dst_alnd + i) = *(src_alnd + i) = 'a' + rand()%26;
        }
        ret = memcmp(dst_alnd , src_alnd, len);
        if (ret != 0)
        {
            printf("ERROR: Validation failed for matching strings of length: %lu, return_value = %d\n", len, ret);
        }
        else
        {
            printf("Validation successfull for matching string of length: %lu\n", len);
        }
        for (i=0; i< len; i++)
        {
           int expected;
	   //set a byte of source different from destination
           while(*(src_alnd + i) == *(dst_alnd + i))
                *(src_alnd + i) = (char)rand();
	   //set one more byte from the end
	   *(src_alnd + len - 1) = '$';
           ret = memcmp(src_alnd , dst_alnd, len);
           expected = (*(uint8_t *)(src_alnd + i) - *(uint8_t *)(dst_alnd + i));
           if (ret != expected)
           {
               printf("ERROR: Validation failed for non-matching string of length: %lu(index = %lu), return_value (actual= %d, expected = %d)\n", len, i, ret, expected);
	       validation_passed = 0;
           }
           *(src_alnd + i) = *(dst_alnd + i);
        }
	if (validation_passed)
               printf("Validation successfull for non-matching string of length: %lu\n", len);
    }
    else //Latency Measure test
    {   //dummy call to avoid resolver latency on first call
        int ret = memcmp(dst_alnd,src_alnd, 0);
        *(dst_alnd + len -1) = '@';
        clk_start = rdtscp_start();
        ret = memcmp(dst_alnd , src_alnd, len);
        clk_end = rdtscp_end();
        diff = clk_end - clk_start;//nano secs
        printf("%lu,%u,%u,%lu\n", len, src_alignment, dst_alignment, diff);
    }

    free(src);
    free(dst);
}
