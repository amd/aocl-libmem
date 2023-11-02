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
#define BOUNDARY_BYTES  8
typedef struct
{
    const char *func_name;
    void  (*func)(size_t, uint32_t , uint32_t);
} libmem_func;

#define OVERLAP_BUFFER        0
#define NON_OVERLAP_BUFFER    1
#define DEFAULT               2

#define implicit_func_decl_push_ignore \
    _Pragma("GCC diagnostic push") \
    _Pragma("GCC diagnostic ignored \"-Wimplicit-function-declaration\"") \

#define implicit_func_decl_pop \
    _Pragma("GCC diagnostic pop") \

typedef uint8_t alloc_mode;

//Function for verifying string functions
//Passing size = -1 for strcmp (Until Null is found)
int string_cmp(const char *str1, const char *str2, size_t size)
{
    int i;
    for(i=0; (str1[i] != '\0') && (str2[i] != '\0') && (str2[i] == str1[i]) && (i < (size -1)); i++);
    return (unsigned char)str1[i] - (unsigned char)str2[i];
}

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

static inline void prepare_boundary(uint8_t *dst, size_t size)
{
    for (size_t index = 1; index <= BOUNDARY_BYTES; index++)
    {
        *(dst - index) = '#';
        *(dst + size + index - 1) = '#';
    }
}

static inline void boundary_check(uint8_t *dst, size_t size)
{
    for (size_t index = 1; index <= BOUNDARY_BYTES; index++)
    {
        if (*(dst - index) != '#')
        {
            printf("ERROR: Out of bound Data corruption @pre_index:%ld for size: %ld\n", index, size);
            break;
        }
        if (*(dst + size + index - 1) != '#')
        {
            printf("ERROR: Out of bound Data corruption @post_index:%ld for size: %ld\n", index, size);
            break;
        }
    }

}

static inline void memcpy_validator(size_t size, uint32_t dst_alnmnt,\
                                                 uint32_t src_alnmnt)
{
    uint8_t *buff = NULL, *buff_head, *buff_tail;
    uint8_t *dst_alnd_addr = NULL, *src_alnd_addr = NULL;
    size_t index;
    void *ret = NULL;

    //special case to handle size ZERO with NULL buff.
    if (size == 0)
    {
        ret = memcpy(buff, buff, size);
        if (ret != NULL)
            printf("ERROR: Return value mismatch for size(%lu): expected - %p"\
                        ", actual - %p\n", size, buff, ret);
        return;
    }

    buff = alloc_buffer(&buff_head, &buff_tail, size + BOUNDARY_BYTES, NON_OVERLAP_BUFFER);

    if (buff == NULL)
    {
        perror("Failed to allocate memory");
        exit(-1);
    }
    dst_alnd_addr = buff_tail + dst_alnmnt;
    src_alnd_addr = buff_head + src_alnmnt;

    prepare_boundary(dst_alnd_addr, size);

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

    boundary_check(dst_alnd_addr, size);

    free(buff);
}

static inline void mempcpy_validator(size_t size, uint32_t dst_alnmnt,\
                                                 uint32_t src_alnmnt)
{
    uint8_t *buff = NULL, *buff_head, *buff_tail;
    uint8_t *dst_alnd_addr = NULL, *src_alnd_addr = NULL;
    size_t index;
    void *ret = NULL;

    //special case to handle size ZERO with NULL buffer inputs.
    if (size == 0)
    {
        implicit_func_decl_push_ignore
        ret = mempcpy(buff, buff, size);
        implicit_func_decl_pop
        if (ret != buff)
            printf("ERROR: Return value mismatch for size(%lu): expected - %p"\
                        ", actual - %p\n", size, buff, ret);
        return;
    }

    buff = alloc_buffer(&buff_head, &buff_tail, size + BOUNDARY_BYTES, NON_OVERLAP_BUFFER);

    if (buff == NULL)
    {
        perror("Failed to allocate memory");
        exit(-1);
    }

    dst_alnd_addr = buff_tail + dst_alnmnt;
    src_alnd_addr = buff_head + src_alnmnt;

    prepare_boundary(dst_alnd_addr, size);

    //intialize src memory
    for (index = 0; index < size; index++)
        *(src_alnd_addr +index) = 'a' + rand()%26;

    implicit_func_decl_push_ignore
    ret = mempcpy(dst_alnd_addr, src_alnd_addr, size);
    implicit_func_decl_pop

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

    boundary_check(dst_alnd_addr, size);

    free(buff);
}


static inline void memmove_validator(size_t size, uint32_t dst_alnmnt, uint32_t src_alnmnt)
{
    uint8_t *buff = NULL, *buff_head, *buff_tail, *validation_buff, *temp;
    uint8_t *dst_alnd_addr = NULL, *src_alnd_addr = NULL, *validation_addr = NULL;
    size_t index;
    void *ret = NULL;

    //special case to handle size ZERO with NULL buffer inputs.
    if (size == 0)
    {
        ret = memmove(buff, buff, size);
        if (ret != buff)
            printf("ERROR: Return value mismatch for size(%lu): expected - %p"\
                        ", actual - %p\n", size, buff, ret);
        return;
    }

    // Overlapping Memory Validation
    buff = alloc_buffer(&buff_head, &buff_tail, size, OVERLAP_BUFFER);

    if (buff == NULL)
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
    buff = alloc_buffer(&buff_head, &buff_tail, size + BOUNDARY_BYTES, NON_OVERLAP_BUFFER);

    if (buff == NULL)
    {
        perror("Failed to allocate memory");
        exit(-1);
    }
    dst_alnd_addr = buff_tail + dst_alnmnt;
    src_alnd_addr = buff_head + src_alnmnt;

    prepare_boundary(dst_alnd_addr, size);

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

    boundary_check(dst_alnd_addr, size);

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
    //special case to handle size ZERO with NULL buff inputs.
    if (size == 0)
    {
        ret = memset(buff, value, size);
        if (ret != buff)
            printf("ERROR: Return value mismatch for size(%lu): expected - %p"\
                        ", actual - %p\n", size, buff, ret);
        return;
    }

    buff = alloc_buffer(&buff_head, &buff_tail, size + 2 * BOUNDARY_BYTES, DEFAULT);

    if (buff == NULL)
    {
        perror("Failed to allocate memory");
        exit(-1);
    }
    //Adding offset of CACHE_LINE_SZ to fit in Boundary bytes
    dst_alnd_addr = buff_tail + dst_alnmnt + CACHE_LINE_SZ;

    prepare_boundary(dst_alnd_addr, size);
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

    boundary_check(dst_alnd_addr, size);

    free(buff);
}

static inline void memcmp_validator(size_t size, uint32_t mem2_alnmnt,\
                                                 uint32_t mem1_alnmnt)
{
    uint8_t *buff = NULL, *buff_head, *buff_tail;
    uint8_t *mem2_alnd_addr = NULL, *mem1_alnd_addr = NULL;
    size_t index;
    int ret, exp_ret, validation_passed = 1;

    //special case to handle size ZERO with NULL buff inputs.
    if (size == 0)
    {
        ret = memcmp(buff, buff, size);
        if (ret != 0)
            printf("ERROR: Return value mismatch for size(%lu): expected - 0"\
                        ", actual - %d\n", size, ret);
        return;
    }

    buff = alloc_buffer(&buff_head, &buff_tail, size + BOUNDARY_BYTES, NON_OVERLAP_BUFFER);

    if (buff == NULL)
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
        printf("ERROR: Validation failed for matching data of size: %lu,"\
                                    " return_value = %d\n", size, ret);
    }
    else
    {
        printf("Validation passed for matching memory of size: %lu\n", size);
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
           printf("ERROR: Validation failed for non-matching string of "\
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

static inline void strcpy_validator(size_t size, uint32_t str2_alnmnt,\
                                                 uint32_t str1_alnmnt)
{
    uint8_t *buff = NULL, *buff_head, *buff_tail;
    uint8_t *str2_alnd_addr = NULL, *str1_alnd_addr = NULL;
    size_t index;
    void *ret = NULL;
    srand(time(0));

    //special case to handle size ZERO with NULL buff inputs.
    if (size == 0)
    {
        ret = strcpy((char*)buff, (char*)buff);
        if (ret != buff)
            printf("ERROR: Return value mismatch for size(%lu): expected - %p"\
                        ", actual - %p\n", size, buff, ret);
        return;
    }

    buff = alloc_buffer(&buff_head, &buff_tail, size + BOUNDARY_BYTES, NON_OVERLAP_BUFFER);

    if (buff == NULL)
    {
        perror("Failed to allocate memory");
        exit(-1);
    }
    str2_alnd_addr = buff_tail + str2_alnmnt;
    str1_alnd_addr = buff_head + str1_alnmnt;

    prepare_boundary(str2_alnd_addr, size);

    //intialize str1 memory
    for (index = 0; index < size; index++)
    {
        *(str1_alnd_addr + index) = (char)(rand() % 128);
        if (*(str1_alnd_addr + index) == '\0')
            *(str1_alnd_addr + index) = 'a';
    }
    //Appending Null Charachter at the end of str1 string
    *(str1_alnd_addr + size - 1) = '\0';
    ret = strcpy((char *)str2_alnd_addr, (char *)str1_alnd_addr);

    //validation of str2 memory
    for (index = 0; (index <= size) && (*(str2_alnd_addr + index) == \
                            *(str1_alnd_addr + index)); index ++);

    if (index != (size))
        printf("ERROR: Data Validation failed for size: %lu @index:%lu" \
                    "[str1: %p(alignment = %u), str2:%p(alignment = %u)]\n", \
              size, index, str1_alnd_addr, str1_alnmnt, str2_alnd_addr, str2_alnmnt);
    else
        printf("Data Validation passed for size: %lu\n", size);
    //validation of return value
    if (ret != str2_alnd_addr)
        printf("ERROR: Return value mismatch: expected - %p, actual - %p\n", \
                                                             str2_alnd_addr, ret);

    //Multi-Null check
    size_t more_null_idx = rand() % (size);
    *(str1_alnd_addr + more_null_idx) = '\0';

    ret = strcpy((char *)str2_alnd_addr, (char *)str1_alnd_addr);
    for (index = 0; (index <= more_null_idx) && (*(str2_alnd_addr + index) == \
                            *(str1_alnd_addr + index)); index ++);

    if (index != (more_null_idx + 1))
        printf("ERROR: Multi-NULL check Validation failed for size: %lu @index:%lu" \
                    "[str1: %p(alignment = %u), str2:%p(alignment = %u)]\n", \
              size, index, str1_alnd_addr, str1_alnmnt, str2_alnd_addr, str2_alnmnt);
    else
        printf("Multi-NULL check Validation passed for size: %lu\n", size);
    //validation of return value
    if (ret != str2_alnd_addr)
        printf("ERROR: Multi-NULL check Return value mismatch: expected - %p, actual - %p\n", \
                                                             str2_alnd_addr, ret);

    boundary_check(str2_alnd_addr, size);

    free(buff);
}

static inline void strncpy_validator(size_t size, uint32_t str2_alnmnt,\
                                                 uint32_t str1_alnmnt)
{
    uint8_t *buff = NULL, *buff_head, *buff_tail;
    uint8_t *str2_alnd_addr = NULL, *str1_alnd_addr = NULL;
    size_t index, str1_len;
    void *ret = NULL;
    srand(time(0));

    //special case to handle size ZERO with NULL buff inputs.
    if (size == 0)
    {
        ret = strncpy((char*)buff, (char*)buff, size);
        if (ret != buff)
            printf("ERROR: Return value mismatch for size(%lu): expected - %p"\
                        ", actual - %p\n", size, buff, ret);
        return;
    }

    buff = alloc_buffer(&buff_head, &buff_tail, size + BOUNDARY_BYTES, NON_OVERLAP_BUFFER);

    if (buff == NULL)
    {
        perror("Failed to allocate memory");
        exit(-1);
    }
    str2_alnd_addr = buff_tail + str2_alnmnt;
    str1_alnd_addr = buff_head + str1_alnmnt;

    prepare_boundary(str2_alnd_addr, size);

    for (index = 0; index <= size; index++)
    {
        *(str1_alnd_addr + index) = (char)(rand()%128);
        if (*(str1_alnd_addr + index) == '\0')
            *(str1_alnd_addr + index) = 'a';
    }
    for(index = size; index <= size + BOUNDARY_BYTES; index++)
        *(str2_alnd_addr + index) = '#';

    //CASE 1:validation when NULL terminating char is beyond strlen
    ret = strncpy((char *)str2_alnd_addr, (char *)str1_alnd_addr, size);
    //validation of str2 memory
    for (index = 0; (index < size) && (*(str2_alnd_addr + index) == \
                            *(str1_alnd_addr + index)); index ++);

    if (index != size)
        printf("ERROR:[strlen > n] Data Validation failed for size: %lu @index:%lu" \
                    "[str1: %p(alignment = %u), str2:%p(alignment = %u)]\n", \
              size, index, str1_alnd_addr, str1_alnmnt, str2_alnd_addr, str2_alnmnt);
    else
        printf("[strlen > n] Data Validation passed for size: %lu\n", size);
    //validation of return value
    if (ret != str2_alnd_addr)
        printf("ERROR:[strlen > n] Return value mismatch: expected - %p, actual - %p\n",
                                                             str2_alnd_addr, ret);


    //Check if the str2 buffer was modified after 'n bytes' copy
    boundary_check(str2_alnd_addr, size);

    //CASE 2:validation when index of NULL char is equal to strlen
    //Appending Null Charachter at the end of str1 string
    *(str1_alnd_addr + size - 1) = '\0';

    //Passing the size including the NULL char(size+1)
    ret = strncpy((char *)str2_alnd_addr, (char *)str1_alnd_addr, size);

    //validation of str2 memory
    for (index = 0; (index < size) && (*(str2_alnd_addr + index) == \
                            *(str1_alnd_addr + index)); index ++);

    if (index != size)
        printf("ERROR:[strlen = n] Data Validation failed for size: %lu @index:%lu" \
                    "[str1: %p(alignment = %u), str2:%p(alignment = %u)]\n", \
              size, index, str1_alnd_addr, str1_alnmnt, str2_alnd_addr, str2_alnmnt);
    else
        printf("[strlen = n] Data Validation passed for size: %lu\n", size);
    //validation of return value
    if (ret != str2_alnd_addr)
        printf("ERROR:[strlen = n] Return value mismatch: expected - %p, actual - %p\n", \
                                                             str2_alnd_addr, ret);

    //Check if the str2 buffer was modified after 'n bytes' copy
    boundary_check(str2_alnd_addr, size);

    //CASE 3:validation when str1 length is less than that of 'n'(no.of bytes to be copied)
    //Generating random str1 buffer of size less than n

    //Multi-Null check
    srand(time(0));
    size_t null_idx = rand() % (size);
    size_t more_null_idx = rand() % (size - null_idx);
    str1_len = null_idx;

    if (str1_len > 0)
        str1_len--;

    *(str1_alnd_addr + str1_len) = '\0';
    *(str1_alnd_addr + str1_len + more_null_idx) = '\0';

    ret = strncpy((char *)str2_alnd_addr, (char *)str1_alnd_addr, size);
    //validation of str2 memory
    for (index = 0; (index <= str1_len) && (*(str2_alnd_addr + index) == \
                            *(str1_alnd_addr + index)); index ++);

    if (index != (str1_len + 1))
        printf("ERROR:[strlen < n] Data Validation failed for size: %lu @index:%lu" \
                    "[str1: %p(alignment = %u), str2:%p(alignment = %u)] (strlen = %lu)\n", \
              size, index, str1_alnd_addr, str1_alnmnt, str2_alnd_addr, str2_alnmnt, str1_len);
    //Checking for NULL after str1_len in str2 buffer
    for (;index < size; index++)
    {
        if (str2_alnd_addr[index] != '\0')
        {
            printf("ERROR:[strlen < n] NULL Validation failed at index:%lu" \
                        " for size: %ld(strlen = %ld)\n", index, size, str1_len);
            break;
        }
    }
    if (index == size)
        printf("[strlen < n] Data Validation passed for size: %lu\n", size);
    //validation of return value
    if (ret != str2_alnd_addr)
        printf("ERROR:[strlen < n] Return value mismatch: expected - %p, actual - %p\n", \
                                                             str2_alnd_addr, ret);

    //Check if the str2 buffer was modified after 'n bytes' copy
    boundary_check(str2_alnd_addr, size);

    free(buff);
}

static inline void strcmp_validator(size_t size, uint32_t str2_alnmnt,\
                                                 uint32_t str1_alnmnt)
{
    uint8_t *buff = NULL, *buff_head, *buff_tail;
    uint8_t *str2_alnd_addr = NULL, *str1_alnd_addr = NULL;
    size_t index;
    int ret = 0;
    int validation1_passed = 1, validation2_passed = 1;
    int exp_ret = 0;

    //special case to handle size ZERO with NULL buff inputs.
    if (size == 0)
    {
        ret = strcmp((char*)buff, (char*)buff);
        if (ret != exp_ret)
            printf("ERROR: Return value mismatch for size(%lu): expected - %d"\
                        ", actual - %d\n", size, exp_ret, ret);
        return;
    }

    buff = alloc_buffer(&buff_head, &buff_tail, size , NON_OVERLAP_BUFFER);

    if (buff == NULL)
    {
        perror("Failed to allocate memory");
        exit(-1);
    }
    str2_alnd_addr = buff_tail + str2_alnmnt;
    str1_alnd_addr = buff_head + str1_alnmnt;
    srand(time(0));

    //Case1: Equal strings
    //intialize str1 and str2 memory
    for (index = 0; index < size; index++)
    {
        *(str1_alnd_addr + index) = *(str2_alnd_addr + index) = ((char) 'a' + rand() % 26);
    }
    //Appending Null Charachter at the end of str1 and str2
    *(str1_alnd_addr + size - 1) = *(str2_alnd_addr + size - 1) = '\0';
    ret = strcmp((char *)str2_alnd_addr, (char *)str1_alnd_addr);

    if (ret != 0)
    {
        printf("ERROR: Validation failed for matching data of str1_aln:%u str2_aln:%u size: %lu,"\
                                    " return_value = %d\n",str1_alnmnt,str2_alnmnt, size, ret);
    }
    else
    {
        printf("Validation passed for matching memory of size: %lu\n", size);
    }

    //Case2:  Validation of Byte by Byte mismtach for MATCHING string
    for (index = 0; index < size; index++)
    {
       //set a byte of source different from destination @index
       //Case2(a): s1 < s2
           *(str1_alnd_addr + index) = '$';

       ret = strcmp((char *)str1_alnd_addr, (char *)str2_alnd_addr); // -ve value expected
       exp_ret = string_cmp((char *)str1_alnd_addr, (char *)str2_alnd_addr, -1);

       if (ret != exp_ret)
       {
           printf("ERROR:(str1<str2) Validation failed for non-matching @index:%lu str1_aln:%u str2_aln:%u size: %lu,"\
                                    " return_value = %d exp=%d\n", index, str1_alnmnt, str2_alnmnt, size, ret, exp_ret);
           validation1_passed = 0;
       }

       //Case2(b): s1 > s2
       ret = strcmp((char *)str2_alnd_addr, (char *)str1_alnd_addr); // +ve value expected
       exp_ret = (*(uint8_t *)(str2_alnd_addr + index) - \
                        *(uint8_t *)(str1_alnd_addr + index));
       if (ret != exp_ret)
       {
           printf("ERROR:(str1>str2) Validation failed for non-matching @index:%lu str1_aln:%u str2_aln:%u size: %lu,"\
                                    " return_value = %d exp=%d\n", index, str1_alnmnt, str2_alnmnt, size, ret, exp_ret);
           validation2_passed = 0;
       }
       //copying back the mismatching byte
       *(str1_alnd_addr + index) = *(str2_alnd_addr + index);
    }

    if (validation1_passed && validation2_passed)
         printf("Validation successfull for non-matching data of size: %lu\n",\
                                                                          size);
    //Case3: Multi-NULL check
    //Can't have multiple NULL for size < 2
    if (size >= 2)
    {
        size_t more_null_idx = rand() % (size - 1);
        *(str1_alnd_addr + more_null_idx) = *(str2_alnd_addr + more_null_idx) = '\0';

        //Set a Mismatching byte after NULL @more_null_idx + 1
        *(str1_alnd_addr + more_null_idx + 1) = '@';

        ret = strcmp((char *)str2_alnd_addr, (char *)str1_alnd_addr);
        exp_ret = string_cmp((char *)str2_alnd_addr, (char *)str1_alnd_addr, -1);

        if (ret != exp_ret)
        {
            printf("ERROR: Multi-NULL check Validation failed for size: %lu @Mismatching index:%lu" \
                    "[str1: %p(alignment = %u), str2:%p(alignment = %u)]\n", \
                size, more_null_idx + 1, str1_alnd_addr, str1_alnmnt, str2_alnd_addr, str2_alnmnt);

        }
        else
            printf("Multi-NULL check Validation passed for size: %lu\n", size);
    }

    //case4: strlen(str1), strlen(str2) > size and are matching

    for (index = 0; index < size; index++)
    {
        *(str1_alnd_addr + index) = *(str2_alnd_addr + index) = ((char) 'A'+ rand() % 26);
    }

    ret = strcmp((char *)str1_alnd_addr, (char *)str2_alnd_addr);
    exp_ret = string_cmp((char *)str1_alnd_addr, (char *)str2_alnd_addr, -1);

    if (ret!=exp_ret)
    {
        printf("ERROR [str1(%lu) & str2(%lu) >size]: Validation failed for str1_aln:%u str2_aln:%u size: %lu,"\
                                    " return_value = %d, exp=%d",strlen((char*)str1_alnd_addr),strlen((char*)str2_alnd_addr),str1_alnmnt,str2_alnmnt, size, ret, exp_ret);
    }

    //case5: strlen(str1) = size and strlen(str2) > size

    *(str1_alnd_addr + size -1) = '\0';
    ret = strcmp((char *)str2_alnd_addr, (char *)str1_alnd_addr);
    exp_ret = (*(uint8_t *)(str2_alnd_addr + size - 1) - \
                        *(uint8_t *)(str1_alnd_addr + size - 1));
    if (ret != exp_ret)
    {
        printf("ERROR [str1=size, str2(%lu) >size]: Validation failed for string @index:%lu str1_aln:%u str2_aln:%u size: %lu,"\
                                    " return_value = %d\n",strlen((char*)str2_alnd_addr), size-1,str1_alnmnt,str2_alnmnt, size, ret);
    }

    //case6:strlen(str2) = size and strlen(str1) > size

    ret = strcmp((char *)str1_alnd_addr,(char *)str2_alnd_addr);
    exp_ret = (*(uint8_t *)(str1_alnd_addr + size - 1) - \
                        *(uint8_t *)(str2_alnd_addr + size - 1));

    if (ret != exp_ret)
    {
        printf("ERROR [str2=size, str1(%lu) >size]: Validation failed for string @index:%lu str1_aln:%u str2_aln:%u size: %lu,"\
                                    " return_value = %d\n",strlen((char*)str2_alnd_addr), size-1,str1_alnmnt,str2_alnmnt, size, ret);
    }

    //Case7: strlen(str1), strlein(str2) < size
    if (size >=2)
    {
        //case7(a): strlen(str1) < strlen(str2)
        *(str1_alnd_addr + size - 1) = *(str2_alnd_addr + size - 1); //Removing Null from str1
        size_t s1_sz = rand() % (size / 2 ); //strlen(str1) between [0, size/2 -1]
        size_t s2_sz = rand() % (size / 2) + size / 2; //strlen(str2) between [size/2 + 1, size-1]

        *(str1_alnd_addr + s1_sz) = '\0';
        *(str2_alnd_addr + s2_sz) = '\0';

        ret = strcmp((char *)str2_alnd_addr, (char *)str1_alnd_addr);
        exp_ret = string_cmp((char *)str2_alnd_addr, (char *)str1_alnd_addr, -1);

        if (ret != exp_ret)
        {
           printf("ERROR[str1_sz(%lu) < str2_sz(%lu)]: Validation failed for non-matching string str1_aln:%u str2_aln:%u,"\
                                    " return_value = %d, exp_value =%d\n",s1_sz, s2_sz, str1_alnmnt,str2_alnmnt, ret,exp_ret);
        }

        //case7(b): strlen(str1) > strlen(str2)
        ret = strcmp((char *)str1_alnd_addr, (char *)str2_alnd_addr);
        exp_ret = string_cmp((char *)str1_alnd_addr, (char *)str2_alnd_addr, -1);

        if (ret != exp_ret)
        {
           printf("ERROR [str1_sz(%lu) > str2_sz(%lu)]: Validation failed for non-matching string str1_aln:%u str2_aln:%u size: %lu,"\
                                    " return_value = %d, exp_value=%d\n",s1_sz, s2_sz, str1_alnmnt,str2_alnmnt, size, ret,exp_ret);
        }
    }
    free(buff);
}

static inline void strncmp_validator(size_t size, uint32_t str2_alnmnt,\
                                                 uint32_t str1_alnmnt)
{
    uint8_t *buff = NULL, *buff_head, *buff_tail;
    uint8_t *str2_alnd_addr = NULL, *str1_alnd_addr = NULL;
    size_t index;
    int ret = 0;
    int validation1_passed = 1, validation2_passed = 1;
    int exp_ret = 0;

    //special case to handle size ZERO with NULL buff inputs.
    if (size == 0)
    {
        ret = strncmp((char*)buff, (char*)buff, size);
        if (ret != exp_ret)
            printf("ERROR: Validation failure for size(%lu): expected - %d"\
                        ", actual - %d\n", size, exp_ret, ret);
        return;
    }

    buff = alloc_buffer(&buff_head, &buff_tail, size, NON_OVERLAP_BUFFER);

    if (buff == NULL)
    {
        perror("Failed to allocate memory");
        exit(-1);
    }
    str2_alnd_addr = buff_tail + str2_alnmnt;
    str1_alnd_addr = buff_head + str1_alnmnt;
    srand(time(0));

    //Case1: Equal strings with NULL
    //intialize str1 and str2 memory
    for (index = 0; index < size; index++)
    {
        *(str1_alnd_addr + index) = *(str2_alnd_addr + index) = ((char) 'a' + rand() % 26);
    }
    //Appending Null Charachter at the end of str1 and str2
    *(str1_alnd_addr + size - 1) = *(str2_alnd_addr + size - 1) = '\0';
    ret = strncmp((char *)str2_alnd_addr, (char *)str1_alnd_addr, size);

    if (ret != 0)
    {
        printf("ERROR: Validation failure for matching data of str1_aln:%u str2_aln:%u size: %lu,"\
                                    " return_value = %d\n",str1_alnmnt,str2_alnmnt, size, ret);
    }
    else
    {
        printf("Validation passed for matching memory of size: %lu\n", size);
    }

    //Case2:  Validation of Byte by Byte mismtach for MATCHING string
    for (index = 0; index < size; index++)
    {
       //set a byte of source different from destination @index
       //Case2(a): s1 < s2
           *(str1_alnd_addr + index) = '$';

       ret = strncmp((char *)str1_alnd_addr, (char *)str2_alnd_addr, size); // -ve value expected
       exp_ret = (*(uint8_t *)(str1_alnd_addr + index) - \
                        *(uint8_t *)(str2_alnd_addr + index));

       if (ret != exp_ret)
       {
           printf("ERROR: (str1<str2) Validation failure for non-matching @index:%lu str1_aln:%u str2_aln:%u size: %lu,"\
                                    " return_value = %d exp=%d\n", index, str1_alnmnt, str2_alnmnt, size, ret, exp_ret);
           validation1_passed = 0;
       }

       //Case2(b): s1 > s2
       ret = strncmp((char *)str2_alnd_addr, (char *)str1_alnd_addr, size); // +ve value expected
       exp_ret = (*(uint8_t *)(str2_alnd_addr + index) - \
                        *(uint8_t *)(str1_alnd_addr + index));
       if (ret != exp_ret)
       {
           printf("ERROR: (str1>str2) Validation failure for non-matching @index:%lu str1_aln:%u str2_aln:%u size: %lu,"\
                                    " return_value = %d exp=%d\n", index, str1_alnmnt, str2_alnmnt, size, ret, exp_ret);
           validation2_passed = 0;
       }
       //copying back the mismatching byte
       *(str1_alnd_addr + index) = *(str2_alnd_addr + index);
    }

    if (validation1_passed && validation2_passed)
         printf("Validation successfull for non-matching data of size: %lu\n",\
                                                                          size);
    //Case3: Multi-NULL check
    //Can't have multiple NULL for size < 2
    if (size >= 2)
    {
        size_t more_null_idx = rand() % (size - 1);
        *(str1_alnd_addr + more_null_idx) = *(str2_alnd_addr + more_null_idx) = '\0';

        //Set a Mismatching byte after NULL @more_null_idx + 1
        *(str1_alnd_addr + more_null_idx + 1) = '@';

        ret = strncmp((char *)str2_alnd_addr, (char *)str1_alnd_addr, size);
        exp_ret = string_cmp((char *)str2_alnd_addr, (char *)str1_alnd_addr, size);

        if (ret != exp_ret)
        {
            printf("ERROR: Multi-NULL check Validation failed for size: %lu @Mismatching index:%lu" \
                    "[str1: %p(alignment = %u), str2:%p(alignment = %u)]\n", \
                size, more_null_idx + 1, str1_alnd_addr, str1_alnmnt, str2_alnd_addr, str2_alnmnt);

        }
        else
            printf("Multi-NULL check Validation passed for size: %lu\n", size);
    }

    //case4: strlen(str1), strlen(str2) > size and are matching

    for (index = 0; index < size; index++)
    {
        *(str1_alnd_addr + index) = *(str2_alnd_addr + index) = ((char) 'A'+ rand() % 26);
    }

    ret = strncmp((char *)str1_alnd_addr, (char *)str2_alnd_addr, size);
    exp_ret = string_cmp((char *)str1_alnd_addr, (char *)str2_alnd_addr, size);

    if (ret!=exp_ret)
    {
        printf("ERROR [str1(%lu) & str2(%lu) > size]: Validation failure for str1_aln:%u str2_aln:%u size: %lu,"\
                                    " return_value = %d, exp=%d",strlen((char*)str1_alnd_addr),strlen((char*)str2_alnd_addr),str1_alnmnt,str2_alnmnt, size, ret, exp_ret);
    }

    //case5: strlen(str1) = size and strlen(str2) > size

    *(str1_alnd_addr + size -1) = '\0';
    ret = strncmp((char *)str2_alnd_addr, (char *)str1_alnd_addr, size);
    exp_ret = (*(uint8_t *)(str2_alnd_addr + size - 1) - \
                        *(uint8_t *)(str1_alnd_addr + size - 1));
    if (ret != exp_ret)
    {
        printf("ERROR [str1=size, str2(%lu) > size] Validation failure for string @index:%lu str1_aln:%u str2_aln:%u size: %lu,"\
                                    " return_value = %d\n", strlen((char*)str2_alnd_addr), size-1,str1_alnmnt,str2_alnmnt, size, ret);
    }

    //case6:strlen(str2) = size and strlen(str1) > size

    ret = strncmp((char *)str1_alnd_addr,(char *)str2_alnd_addr, size);
    exp_ret = (*(uint8_t *)(str1_alnd_addr + size - 1) - \
                        *(uint8_t *)(str2_alnd_addr + size - 1));

    if (ret != exp_ret)
    {
        printf("ERROR [str2=size, str1(%lu) > size] Validation failure for string @index:%lu str1_aln:%u str2_aln:%u size: %lu,"\
                                    " return_value = %d\n", strlen((char*)str2_alnd_addr), size-1,str1_alnmnt,str2_alnmnt, size, ret);
    }

    //Case7: strlen(str1), strlein(str2) < size
    if (size >=2)
    {
        //case7(a): strlen(str1) < strlen(str2)
        *(str1_alnd_addr + size - 1) = *(str2_alnd_addr + size - 1); //Removing Null from str1
        size_t s1_sz = rand() % (size / 2 ); //strlen(str1) between [0, size/2 -1]
        size_t s2_sz = rand() % (size / 2) + size / 2; //strlen(str2) between [size/2 + 1, size-1]

        *(str1_alnd_addr + s1_sz) = '\0';
        *(str2_alnd_addr + s2_sz) = '\0';

        ret = strncmp((char *)str2_alnd_addr, (char *)str1_alnd_addr, size);
        exp_ret = string_cmp((char *)str2_alnd_addr, (char *)str1_alnd_addr, size);

        if (ret != exp_ret)
        {
           printf("ERROR[str1(%lu) < str2(%lu)] Validation failure for non-matching string str1_aln:%u str2_aln:%u ,"\
                                    " return_value = %d, exp_value =%d\n", s1_sz, s2_sz, str1_alnmnt, str2_alnmnt, ret, exp_ret);
        }

        //case7(b): strlen(str1) > strlen(str2)
        ret = strncmp((char *)str1_alnd_addr, (char *)str2_alnd_addr, size);
        exp_ret = string_cmp((char *)str1_alnd_addr, (char *)str2_alnd_addr, size);

        if (ret != exp_ret)
        {
           printf("ERROR [str1_sz(%lu) > str2_sz(%lu)] Validation failure for non-matching string str1_aln:%u str2_aln:%u size: %lu,"\
                                    " return_value = %d, exp_value=%d\n", s2_sz, s1_sz, str1_alnmnt, str2_alnmnt, size, ret,exp_ret);
        }
    }
    free(buff);
}

static inline void strlen_validator(size_t size, uint32_t str2_alnmnt,\
                                                 uint32_t str1_alnmnt)
{
    uint8_t *buff = NULL, *buff_head, *buff_tail;
    uint8_t *str_alnd_addr = NULL;
    size_t index;
    int ret = 0;

    buff = alloc_buffer(&buff_head, &buff_tail, size + 1, DEFAULT);

    if (buff == NULL)
    {
        perror("Failed to allocate memory");
        exit(-1);
    }

    str_alnd_addr = buff_tail + str1_alnmnt;
    srand(time(0));

    for (index = 0; index < size; index++)
    {
        *(str_alnd_addr + index) = ((char) 'a' + rand() % 26);
    }
    //Appending Null Charachter at the end of str
    *(str_alnd_addr + size ) = '\0';

    //Adding Additional NULL char after size
    *(str_alnd_addr + size + rand() % 8 ) = '\0';
    ret = strlen((char *)str_alnd_addr);

    if (ret != size)
    {
        printf("ERROR: Validation failure for strlen of str1_aln:%u size: %lu,"\
                                    " return_value = %d\n",str1_alnmnt, size, ret);
    }
    else
    {
        printf("Validation passed for strlen: %lu\n", size);
    }

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
    {"strncpy", strncpy_validator},
    {"strcmp",  strcmp_validator},
    {"strncmp", strncmp_validator},
    {"strlen",  strlen_validator},
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

    int al_check = 0;

    if (argc < 6)
    {
        if (argv[1] == NULL)
        {
            printf("ERROR: Function name not provided\n");
            exit(0);
        }
        if (argv[2] == NULL)
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

    if (argv[5] != NULL)
        al_check = atoi(argv[5]); //Check for alignment validation test.

    if (al_check == 0)
    {
        lm_func_validator->func(size, dst_alignment, src_alignment);
    }

    else
    {
        for(unsigned int aln_src  = 0; aln_src < CACHE_LINE_SZ; aln_src++)
        {
            for(unsigned int aln_dst = 0; aln_dst < CACHE_LINE_SZ; aln_dst++)
            {
                lm_func_validator->func(size, aln_dst, aln_src);
            }
        }
    }
}
