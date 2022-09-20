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
    char *validator, *overlap_mem;
    char *src, *back_src, *src_alnd, *back_src_alnd;
    char *dst, *back_dst, *dst_alnd, *back_dst_alnd;
    char test_mode = 'v';
    unsigned int offset, src_alignment = 64, dst_alignment = 64;
    int64_t back_overlap = 0, fwd_overlap = 0;

    srand(time(0));
    if (argc < 3 || argv[1] == NULL || argv[2] == NULL)
    {
        printf("ERROR: length of memmove not provided\n");
        exit(0);
    }
    test_mode = *argv[1];
    len = strtoul(argv[2], &ptr, 10);
    if (argv[3] != NULL)
        src_alignment = atoi(argv[3]);
    if (argv[4] != NULL)
        dst_alignment = atoi(argv[4]);

    src_alignment = src_alignment%64;
    dst_alignment = dst_alignment%64;

    overlap_mem = (char *) malloc(len + 256);
    if (overlap_mem == NULL)
    {
        perror("Overlap memory allocation failure\n");
        exit(-1);
    }
    //compute src address from overlap memory based on src alignment
    back_src = overlap_mem;
    back_src_alnd = back_src + src_alignment;
    offset = (uint64_t)back_src & 63;
    if (offset != 0)
        back_src_alnd += 64 - offset;

    //compute dst address from src_alnd memory based on dst alignment + overlap
    dst = overlap_mem + 64;
    dst_alnd = dst + dst_alignment;
    offset = (uint64_t)dst & 63;
    if (offset != 0)
        dst_alnd += 64 - offset;

    back_dst_alnd = dst_alnd - 64;
    src_alnd = back_src_alnd + 64;

    //allocate memory for validator
    validator = (char *) malloc(len);
    if (validator == NULL)
    {
        perror("Validator memory allocation failure\n");
        free(overlap_mem);
        exit(-1);
    }
    fwd_overlap = len - (uint64_t)(src_alnd - back_dst_alnd);
    back_overlap = len - (uint64_t)(dst_alnd - back_src_alnd);

    if (fwd_overlap < 0)
        fwd_overlap = 0;
    if (back_overlap < 0)
        back_overlap = 0;

    if (test_mode == 'v')
    {
        uint64_t i;

	// backward copy
        for (i=0; i< len; i++)
	{
            *(back_src_alnd + i) = *(validator + i) = 'a' + rand()%26;
	}
        memmove(dst_alnd , back_src_alnd, len);

        for (i=0; i<len; i++)
        {
            if (*(dst_alnd + i) != *(validator + i))
            {
                printf("ERROR: Backward Validation failed at index %lu : "
                "src(aln=%u, %p) = %c, dst(aln=%u, %p) = %c, overlap = %lu\n",\
                 i, src_alignment, back_src_alnd, *(validator + i), \
                dst_alignment, dst_alnd, *(dst_alnd + i), back_overlap);
                break;
            }
        }
        if (i == len)
        {
            printf("Backward Copy Validation successfull for size = %luB with SRC "
                "alignment = %u, DST alignment = %u and Overlap = %luB\n", \
                 len, src_alignment, dst_alignment, back_overlap);
	}

        //forward copy
	for (i=0; i<len; i++)
	{
	    *(src_alnd + i) = *(validator + i);
	}

        memmove(back_dst_alnd , src_alnd, len);

        for (i=0; i<len; i++)
        {
            if (*(back_dst_alnd + i) != *(validator + i))
            {
                printf("ERROR: Forward Validation failed at index %lu : "
                "src(aln=%u, %p) = %c, dst(aln=%u, %p) = %c, overlap = %lu\n",\
                 i, src_alignment, src_alnd, *(validator + i), \
                dst_alignment, back_dst_alnd, *(back_dst_alnd + i), fwd_overlap);
                break;
            }
        }
        if (i == len)
        {
            printf("Forward Copy Validation successfull for size = %luB with "
                "SRC alignment = %u, DST alignment = %u and Overlap = %luB\n",\
                 len, src_alignment, dst_alignment, fwd_overlap);
	}
    }
    else //Latency Measure test
    {   //dummy call to avoid resolver latency on first call
        memmove(dst_alnd, src_alnd, 0);

        //rdtscp
        clk_start = rdtscp_start();
        memmove(back_dst_alnd , src_alnd, len);
        clk_end = rdtscp_end();
        diff = clk_end - clk_start;//nano secs
        printf("%lu,%u,%u,%lu\n", len, src_alignment, dst_alignment, diff);
    }

    free(validator);
    free(overlap_mem);
}
