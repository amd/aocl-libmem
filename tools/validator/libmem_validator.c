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
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#define CACHE_LINE_SZ   64

typedef struct
{
    const char *func_name;
    void  (*func)(size_t, uint32_t , uint32_t);
} libmem_func;

#define OVERLAP_BUFFER        0
#define NON_OVERLAP_BUFFER    1
#define DEFAULT               2

typedef uint8_t alloc_mode;

static uint8_t * alloc_buffer(uint8_t **head_buff, uint8_t **tail_buff,\
                                         size_t size, alloc_mode mode)
{
    void *buff_addr = NULL;

    switch (mode)
    {
        case OVERLAP_BUFFER:
            posix_memalign(&buff_addr, CACHE_LINE_SZ, \
                            2 * (size + 2 * CACHE_LINE_SZ));
            * head_buff = (uint8_t *)buff_addr;
            * tail_buff = (uint8_t *)(((uint64_t)buff_addr +\
             CACHE_LINE_SZ + rand()%(size + 1)) & ~(CACHE_LINE_SZ - 1));
            break;
        case NON_OVERLAP_BUFFER:
            posix_memalign(&buff_addr, CACHE_LINE_SZ, \
                            2 * (size + 2 * CACHE_LINE_SZ));
            * head_buff = (uint8_t *)buff_addr;
            * tail_buff = (uint8_t *)(((uint64_t)buff_addr +\
                     size + 2*CACHE_LINE_SZ) & ~(CACHE_LINE_SZ - 1));
            break;
        default:
            posix_memalign(&buff_addr, CACHE_LINE_SZ, size + 2 * CACHE_LINE_SZ);
            * tail_buff = (uint8_t *)buff_addr;
            * head_buff = NULL;
    }
    return (uint8_t *)buff_addr;
}

static inline void memcpy_validator(size_t size, uint32_t dst_alnmnt,\
                                                 uint32_t src_alnmnt)
{
    uint8_t *buff, *buff_head, *buff_tail;
    uint8_t *dst_alnd_addr = NULL, *src_alnd_addr = NULL;
    size_t index;
    void *ret = NULL;

    buff = alloc_buffer(&buff_head, &buff_tail, size, NON_OVERLAP_BUFFER);

    if(buff == NULL)
    {
        perror("Failed to allocate memory");
        exit(-1);
    }
    dst_alnd_addr = buff_tail + dst_alnmnt;
    src_alnd_addr = buff_head + src_alnmnt;

    //intialize src memory
    for (index = 0; index < size; index++)
        *(src_alnd_addr +index) = 'a' + rand()%26;
    ret = memcpy(dst_alnd_addr, src_alnd_addr, size);

    //validation of dst memory
    for (index = 0; (index < size) && (*(dst_alnd_addr + index) == \
                            *(src_alnd_addr + index)); index ++);

    if (index != size)
        printf("ERROR: Data Validation failed for size: %lu @index:%lu" \
                    "[src: %p(alignment = %u), dst:%p(alignment = %u)]\n", \
                 size, index, src_alnd_addr, src_alnmnt, dst_alnd_addr, dst_alnmnt);
    else
        printf("Data Validation passed for size: %lu\n", size);
    //validation of return value
    if (ret != dst_alnd_addr)
        printf("ERROR: Return value mismatch: expected - %p, actual - %p\n", dst_alnd_addr, ret);

    free(buff);
}

static inline void mempcpy_validator(size_t size, uint32_t dst_alnmnt,\
                                                 uint32_t src_alnmnt)
{
    uint8_t *buff = NULL, *buff_head, *buff_tail;
    uint8_t *dst_alnd_addr = NULL, *src_alnd_addr = NULL;
    size_t index;
    void *ret = NULL;

    buff = alloc_buffer(&buff_tail, &buff_head, size, NON_OVERLAP_BUFFER);

    if(buff == NULL)
    {
        perror("Failed to allocate memory");
        exit(-1);
    }

    dst_alnd_addr = buff_tail + dst_alnmnt;
    src_alnd_addr = buff_head + src_alnmnt;

    //intialize src memory
    for (index = 0; index < size; index++)
        *(src_alnd_addr +index) = 'a' + rand()%26;
    ret = mempcpy(dst_alnd_addr, src_alnd_addr, size);
    //validation of dst memory
    for (index = 0; (index < size) && (*(dst_alnd_addr + index) == \
                            *(src_alnd_addr + index)); index ++);

    if (index != size)
        printf("ERROR: Data Validation failed for size: %lu @index:%lu "\
                             "[src: %p(alignment = %u), dst:%p(alignment = %u)]\n",\
                 size, index, src_alnd_addr, src_alnmnt, dst_alnd_addr, dst_alnmnt);
    else
        printf("Data Validation passed for size: %lu\n", size);
    //validation of return value
    if (ret != (dst_alnd_addr + size))
        printf("ERROR: Return value mismatch: expected - %p, actual - %p\n",\
                                                 dst_alnd_addr + size, ret);

    free(buff);
}


static inline void memmove_validator(size_t size, uint32_t dst_alnmnt, uint32_t src_alnmnt)
{
    uint8_t *buff = NULL, *buff_head, *buff_tail, *validation_buff, *temp;
    uint8_t *dst_alnd_addr = NULL, *src_alnd_addr = NULL, *validation_addr = NULL;
    size_t index;
    void *ret = NULL;
    // Overlapping Memory Validation
    buff = alloc_buffer(&buff_head, &buff_tail, size, OVERLAP_BUFFER);

    if(buff == NULL)
    {
        perror("Failed to allocate memory");
        exit(-1);
    }

    validation_buff = alloc_buffer(&temp, &validation_addr, size, DEFAULT);
    if (validation_buff == NULL)
    {
        perror("Failed to allocate validation buffer\n");
        free(buff);
        exit(-1);
    }

    //Forward Validation
    src_alnd_addr = buff_tail + src_alnmnt;
    dst_alnd_addr = buff_head + dst_alnmnt;

    //intialize src memory
    for (index = 0; index < size; index++)
        *(validation_addr + index) = *(src_alnd_addr + index) = 'a' + rand()%26;

    ret = memmove(dst_alnd_addr, src_alnd_addr, size);
    //validation of dst memory
    for (index = 0; (index < size) && (*(dst_alnd_addr + index) == \
                            *(validation_addr + index)); index ++);

    if (index != size)
        printf("ERROR: Forward Data Validation failed for size: %lu @index:%lu "\
                             "[src: %p(alignment = %u), dst:%p(alignment = %u)]\n",\
                 size, index, src_alnd_addr, src_alnmnt, dst_alnd_addr, dst_alnmnt);
    else
        printf("Forward Data Validation passed for size: %lu\n", size);
    //validation of return value
    if (ret != dst_alnd_addr)
        printf("ERROR: Forward Return value mismatch: expected - %p, actual - %p\n",\
                                                         dst_alnd_addr, ret);
 
    //Backward Validation
    src_alnd_addr = buff_head + src_alnmnt;
    dst_alnd_addr = buff_tail + dst_alnmnt;

    //intialize src memory
    for (index = 0; index < size; index++)
        *(validation_addr + index) = *(src_alnd_addr + index) = 'a' + rand()%26;

    ret = memmove(dst_alnd_addr, src_alnd_addr, size);
    //validation of dst memory
    for (index = 0; (index < size) && (*(dst_alnd_addr + index) == \
                            *(validation_addr + index)); index ++);

    if (index != size)
        printf("ERROR: Backward Data Validation failed for size: %lu @index:%lu "\
                             "[src: %p(alignment = %u), dst:%p(alignment = %u)]\n",\
                 size, index, src_alnd_addr, src_alnmnt, dst_alnd_addr, dst_alnmnt);
    else
        printf("Backward Data Validation passed for size: %lu\n", size);
    //validation of return value
    if (ret != dst_alnd_addr)
        printf("ERROR: Return value mismatch: expected - %p, actual - %p\n",\
                                                         dst_alnd_addr, ret);

    free(buff);
    free(validation_buff);
    buff = NULL;
    // Non Over-lapping memory validation
    buff = alloc_buffer(&buff_head, &buff_tail, size, NON_OVERLAP_BUFFER);

    if(buff == NULL)
    {
        perror("Failed to allocate memory");
        exit(-1);
    }
    dst_alnd_addr = buff_tail + dst_alnmnt;
    src_alnd_addr = buff_head + src_alnmnt;

    //intialize src memory
    for (index = 0; index < size; index++)
        *(src_alnd_addr +index) = 'a' + rand()%26;
    ret = memmove(dst_alnd_addr, src_alnd_addr, size);

    //validation of dst memory
    for (index = 0; (index < size) && (*(dst_alnd_addr + index) == \
                            *(src_alnd_addr + index)); index ++);

    if (index != size)
        printf("ERROR: Non-Overlapping Data Validation failed for size: %lu"\
            " @index:%lu [src: %p(alignment = %u), dst:%p(alignment = %u)]\n",\
            size, index, src_alnd_addr, src_alnmnt, dst_alnd_addr, dst_alnmnt);
    else
        printf("Non-Overlapping Data Validation passed for size: %lu\n", size);
    //validation of return value
    if (ret != dst_alnd_addr)
        printf("ERROR: Non-Overlapping Return value mismatch: expected - %p,"\
                                        " actual - %p\n", dst_alnd_addr, ret);

    free(buff);

}

static inline void memset_validator(size_t size, uint32_t dst_alnmnt,\
                                                    uint32_t src_alnmnt)
{
    uint8_t *buff = NULL, *buff_head, *buff_tail;
    uint8_t *dst_alnd_addr = NULL;
    int value;
    size_t index;
    void *ret = NULL;

    buff = alloc_buffer(&buff_head, &buff_tail, size, DEFAULT);

    if(buff == NULL)
    {
        perror("Failed to allocate memory");
        exit(-1);
    }

    dst_alnd_addr = buff_tail + dst_alnmnt;

    //choose value to set
    value = rand()%256;

    ret = memset(dst_alnd_addr, value, size);
    //validation of dst memory
    for (index = 0; (index < size) && (*(dst_alnd_addr + index) == value);\
                                                     index ++);

    if (index != size)
        printf("ERROR: Data Validation failed for size: %lu @index:%lu "\
                             "[dst:%p(alignment = %u)]\n",\
                 size, index, dst_alnd_addr, dst_alnmnt);
    else
        printf("Data Validation passed for size: %lu\n", size);
    //validation of return value
    if (ret != dst_alnd_addr)
        printf("ERROR: Return value mismatch: expected - %p, actual - %p\n",\
                                                         dst_alnd_addr, ret);

    free(buff);
}

static inline void memcmp_validator(size_t size, uint32_t mem2_alnmnt,\
                                                 uint32_t mem1_alnmnt)
{
    uint8_t *buff, *buff_head, *buff_tail;
    uint8_t *mem2_alnd_addr = NULL, *mem1_alnd_addr = NULL;
    size_t index;
    int ret, exp_ret, validation_passed = 1;

    buff = alloc_buffer(&buff_head, &buff_tail, size, NON_OVERLAP_BUFFER);

    if(buff == NULL)
    {
        perror("Failed to allocate memory");
        exit(-1);
    }

    mem2_alnd_addr = buff_tail + mem2_alnmnt;
    mem1_alnd_addr = buff_head + mem1_alnmnt;

    //intialize mem1 and mem2 buffers
    for (index = 0; index < size; index++)
        *(mem2_alnd_addr + index) =  *(mem1_alnd_addr +index) = 'a' + rand()%26;

    ret = memcmp(mem2_alnd_addr , mem1_alnd_addr, size);

    if (ret != 0)
    {
        printf("ERROR: Data Validation failed for matching data of size: %lu,"\
                                    " return_value = %d\n", size, ret);
    }
    else
    {
        printf("Data  Validation passed for matching memory of size: %lu\n", size);
    }

    // Validation of Byte by BYte mismtach
    for (index = 0; index < size; index++)
    {
       int exp_ret;
       //set a byte of source different from destination
       while(*(mem1_alnd_addr + index) == *(mem2_alnd_addr + index))
            *(mem1_alnd_addr + index) = (char)rand();
       //set one more byte from the end
       *(mem1_alnd_addr + size - 1) = '$';
       ret = memcmp(mem1_alnd_addr , mem2_alnd_addr, size);
       exp_ret = (*(uint8_t *)(mem1_alnd_addr + index) - \
                        *(uint8_t *)(mem2_alnd_addr + index));
       if (ret != exp_ret)
       {
           printf("ERROR: Data Validation failed for non-matching string of "\
                    "size: %lu(index = %lu), return_value [actual= %d,"\
                                " expected = %d]\n", size, index, ret, exp_ret);
           validation_passed = 0;
       }
       *(mem1_alnd_addr + index) = *(mem2_alnd_addr + index);
    }
    if (validation_passed)
         printf("Validation successfull for non-matching data of size: %lu\n",\
                                                                          size);

}

static inline void strcpy_validator(size_t size, uint32_t dst_alnmnt,\
                                                 uint32_t src_alnmnt)
{
    uint8_t *buff, *buff_head, *buff_tail;
    uint8_t *dst_alnd_addr = NULL, *src_alnd_addr = NULL;
    size_t index;
    void *ret = NULL;

    buff = alloc_buffer(&buff_head, &buff_tail, size, NON_OVERLAP_BUFFER);

    if(buff == NULL)
    {
        perror("Failed to allocate memory");
        exit(-1);
    }
    dst_alnd_addr = buff_tail + dst_alnmnt;
    src_alnd_addr = buff_head + src_alnmnt;

    //intialize src memory
    for (index = 0; index < size; index++)
        *(src_alnd_addr +index) = 'a' + rand()%26;

    //Appending Null Charachter at the end of src string
    *(src_alnd_addr + size -1) = '\0';
    ret = strcpy((char *)dst_alnd_addr, (char *)src_alnd_addr);

    //validation of dst memory
    for (index = 0; (index < size) && (*(dst_alnd_addr + index) == \
                            *(src_alnd_addr + index)); index ++);

    if (index != size)
        printf("ERROR: Data Validation failed for size: %lu @index:%lu" \
                    "[src: %p(alignment = %u), dst:%p(alignment = %u)]\n", \
                 size, index, src_alnd_addr, src_alnmnt, dst_alnd_addr, dst_alnmnt);
    else
        printf("Data Validation passed for size: %lu\n", size);
    //validation of return value
    if (ret != dst_alnd_addr)
        printf("ERROR: Return value mismatch: expected - %p, actual - %p\n", dst_alnd_addr, ret);

    free(buff);
}


libmem_func supp_funcs[]=
{
    {"memcpy",  memcpy_validator},
    {"mempcpy", mempcpy_validator},
    {"memmove", memmove_validator},
    {"memset",  memset_validator},
    {"memcmp",  memcmp_validator},
    {"strcpy",  strcpy_validator},
    {"none",    NULL}
};


int main(int argc, char **argv)
{
    uint64_t diff = 0;
    uint64_t size;
    char *ptr;
    uint8_t *src = NULL, *src_alnd = NULL;
    uint8_t *dst = NULL, *dst_alnd = NULL;
    unsigned int offset, src_alignment = 0, dst_alignment = 0;
    libmem_func *lm_func_validator = &supp_funcs[0]; //default func is memcpy

    if (argc < 6)
    {
        if(argv[1] == NULL)
        {
            printf("ERROR: Function name not provided\n");
            exit(0);
        }
        if(argv[2] == NULL)
        {
            printf("ERROR: Size not provided\n");
            exit(0);
        }
    }

    for (int idx = 0; idx <= sizeof(supp_funcs)/sizeof(supp_funcs[0]); idx++)
    {
        if (!strcmp(supp_funcs[idx].func_name, argv[1]))
        {
            lm_func_validator = &supp_funcs[idx];
            break;
        }
    }

    size = strtoul(argv[2], &ptr, 10);

    if (argv[3] != NULL)
        src_alignment = atoi(argv[3]) % CACHE_LINE_SZ;

    if (argv[4] != NULL)
        dst_alignment = atoi(argv[4]) % CACHE_LINE_SZ;

    srand(time(0));

    lm_func_validator->func(size, dst_alignment, src_alignment);
}
