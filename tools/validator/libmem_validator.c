/* Copyright (C) 2023-24 Advanced Micro Devices, Inc. All rights reserved.
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
#define PAGE_SZ 4096
#define YMM_SZ 32
#define ZMM_SZ 64

#ifdef AVX512_FEATURE_ENABLED
    #define VEC_SZ ZMM_SZ
#else
    #define VEC_SZ YMM_SZ
#endif

typedef struct
{
    const char *func_name;
    void  (*func)(size_t, uint32_t , uint32_t);
} libmem_func;

//uncomment the line below for verbosity of passed logs
//#define ALM_VERBOSE

#ifdef ALM_VERBOSE
#define ALM_VERBOSE_LOG(fmt, ...)    do {   \
                printf(fmt, ##__VA_ARGS__); \
                                } while(0);
#else
#define ALM_VERBOSE_LOG(fmt, ...) do {      \
                             } while (0)

#endif

#define OVERLAP_BUFFER        0
#define NON_OVERLAP_BUFFER    1
#define DEFAULT               2
#define NON_OVERLAP_BUFFER_EXTRA 3

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

char *test_memchr( const char *src, int ch, size_t len)
{
    size_t n;
    for(n=0;n<len;n++)
        if(src[n] == ch)
            return (void*)&src[n];
    return NULL;
}

char *test_strcat( char *dst, char *src)
{
    char *ret = dst;
    while(*dst++ != '\0');
    --dst;

    while((*dst++ = *src++) != '\0');
    return ret;
}

char *test_strstr(const char *str1, const char *str2)
{
    if (*str2 == '\0') {
        return (char*)str1;
    }
    const char *p1 = str1;
    const char *p2 = str2;
    while (*p1 != '\0') {
        if (*p1 == *p2) {
            const char *q1 = p1;
            const char *q2 = p2;
            while (*q1 != '\0' && *q2 != '\0' && *q1 == *q2) {
                ++q1;
                ++q2;
            }
            if (*q2 == '\0') {
                return (char*)p1;
            }
        }
        ++p1;
    }
    return NULL;
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

        case NON_OVERLAP_BUFFER_EXTRA:
            posix_memalign(&buff_addr, CACHE_LINE_SZ, \
                            3 * (size + 2 * CACHE_LINE_SZ));
            * head_buff = (uint8_t *)buff_addr;
            * tail_buff = (uint8_t *)(((uint64_t)buff_addr +\
                     2*size + 2*CACHE_LINE_SZ) & ~(CACHE_LINE_SZ - 1));
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

static inline int boundary_check(uint8_t *dst, size_t size)
{
    for (size_t index = 1; index <= BOUNDARY_BYTES; index++)
    {
        if (*(dst - index) != '#')
        {
            printf("ERROR:[BOUNDARY] Out of bound Data corruption @pre_index:%ld for size: %ld ", index, size);
            return -1;
        }
        if (*(dst + size + index - 1) != '#')
        {
            printf("ERROR:[BOUNDARY] Out of bound Data corruption @post_index:%ld for size: %ld ", index, size);
            return -1;
        }
    }
    return 0;
}

static inline void init_buffer(char* src, size_t size)
{
    size_t index = 0;
    for (index = 0; index < size; index++)
    {
        *(src + index) = ((char) ((rand() % 92) +36));//[ASCII value 36 ($) -> 127 (DEL)]
    }
    //Random needle char at index, index<size/2, index> size/2
    index = rand()%size;
    *(src + index) = (int)'!';
    if (size != 1)
    {
        int pos = size/2;
        index = rand()%pos;
        *(src + index) = (int)'!';
        index = rand()%(size - pos +1) +pos;
        index = rand()%pos;
        *(src + index) = (int)'!';
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
            printf("ERROR:[RETURN] value mismatch for size(%lu): expected - %p"\
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
        printf("ERROR:[VALIDATION] failed for size: %lu @index:%lu" \
                    "[src: %p(alignment = %u), dst:%p(alignment = %u)]\n", \
                 size, index, src_alnd_addr, src_alnmnt, dst_alnd_addr, dst_alnmnt);
    else
        ALM_VERBOSE_LOG("Data Validation passed for size: %lu\n", size);
    //validation of return value
    if (ret != dst_alnd_addr)
        printf("ERROR:[RETURN] value mismatch: expected - %p, actual - %p\n", dst_alnd_addr, ret);

    if (boundary_check(dst_alnd_addr, size))
        printf("[src: %p(alignment = %u), dst:%p(alignment = %u)]\n",src_alnd_addr, src_alnmnt, dst_alnd_addr, dst_alnmnt);


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
            printf("ERROR:[RETURN] value mismatch for size(%lu): expected - %p"\
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
        printf("ERROR:[VALIDATION] failed for size: %lu @index:%lu "\
                             "[src: %p(alignment = %u), dst:%p(alignment = %u)]\n",\
                 size, index, src_alnd_addr, src_alnmnt, dst_alnd_addr, dst_alnmnt);
    else
        ALM_VERBOSE_LOG("Data Validation passed for size: %lu\n", size);
    //validation of return value
    if (ret != (dst_alnd_addr + size))
        printf("ERROR:[RETURN] value mismatch: expected - %p, actual - %p\n",\
                                                 dst_alnd_addr + size, ret);

    if (boundary_check(dst_alnd_addr, size))
        printf("[src: %p(alignment = %u), dst:%p(alignment = %u)]\n",src_alnd_addr, src_alnmnt, dst_alnd_addr, dst_alnmnt);


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
            printf("ERROR:[RETURN] value mismatch for size(%lu): expected - %p"\
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
        printf("ERROR:[VALIDATION] Forward failed for size: %lu @index:%lu "\
                             "[src: %p(alignment = %u), dst:%p(alignment = %u)]\n",\
                 size, index, src_alnd_addr, src_alnmnt, dst_alnd_addr, dst_alnmnt);
    else
        ALM_VERBOSE_LOG("Forward Data Validation passed for size: %lu\n", size);
    //validation of return value
    if (ret != dst_alnd_addr)
        printf("ERROR:[RETURN] Forward value mismatch: expected - %p, actual - %p\n",\
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
        printf("ERROR:[VALIDATION] Backward failed for size: %lu @index:%lu "\
                             "[src: %p(alignment = %u), dst:%p(alignment = %u)]\n",\
                 size, index, src_alnd_addr, src_alnmnt, dst_alnd_addr, dst_alnmnt);
    else
        ALM_VERBOSE_LOG("Backward Data Validation passed for size: %lu\n", size);
    //validation of return value
    if (ret != dst_alnd_addr)
        printf("ERROR:[RETURN] Backward value mismatch: expected - %p, actual - %p\n",\
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
        printf("ERROR:[VALIDATION] Non-Overlap failed for size: %lu"\
            " @index:%lu [src: %p(alignment = %u), dst:%p(alignment = %u)]\n",\
            size, index, src_alnd_addr, src_alnmnt, dst_alnd_addr, dst_alnmnt);
    else
        ALM_VERBOSE_LOG("Non-Overlapping Data Validation passed for size: %lu\n", size);
    //validation of return value
    if (ret != dst_alnd_addr)
        printf("ERROR:[RETURN] Non-Overlap value mismatch: expected - %p,"\
                                        " actual - %p\n", dst_alnd_addr, ret);

    if (boundary_check(dst_alnd_addr, size))
        printf("[src: %p(alignment = %u), dst:%p(alignment = %u)]\n",src_alnd_addr, src_alnmnt, dst_alnd_addr, dst_alnmnt);


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
            printf("ERROR:[RETURN] value mismatch for size(%lu): expected - %p"\
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
        printf("ERROR:[VALIDATION] failed for size: %lu @index:%lu "\
                             "[dst:%p(alignment = %u)]\n",\
                 size, index, dst_alnd_addr, dst_alnmnt);
    else
        ALM_VERBOSE_LOG("Data Validation passed for size: %lu\n", size);
    //validation of return value
    if (ret != dst_alnd_addr)
        printf("ERROR:[RETURN] value mismatch: expected - %p, actual - %p\n",\
                                                         dst_alnd_addr, ret);

    if (boundary_check(dst_alnd_addr, size))
        printf("[dst:%p(alignment = %u)]\n",dst_alnd_addr, dst_alnmnt);


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
            printf("ERROR:[RETURN] value mismatch for size(%lu): expected - 0"\
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
        printf("ERROR:[VALIDATION] failed for matching data of size: %lu,"\
                                    " return_value = %d\n", size, ret);
    }
    else
    {
        ALM_VERBOSE_LOG("Validation passed for matching memory of size: %lu\n", size);
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
           printf("ERROR:[VALIDATION] Non-Matching failed for string of "\
                    "size: %lu(index = %lu), return_value [actual= %d,"\
                                " expected = %d]\n", size, index, ret, exp_ret);
           validation_passed = 0;
       }
       *(mem1_alnd_addr + index) = *(mem2_alnd_addr + index);
    }
    if (validation_passed)
         ALM_VERBOSE_LOG("Validation successfull for non-matching data of size: %lu\n",\
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

    //special case to handle size ZERO with NULL buff.
    if (size == 0)
    {
      return ;
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
        *(str1_alnd_addr + index) = 'a' + (char)(rand() % 26);
    }
    //Appending Null Charachter at the end of str1 string
    *(str1_alnd_addr + size - 1) = '\0';
    ret = strcpy((char *)str2_alnd_addr, (char *)str1_alnd_addr);

    //validation of str2 memory
    for (index = 0; (index < size) && (*(str2_alnd_addr + index) == \
                            *(str1_alnd_addr + index)); index ++);

    if (index != (size))
        printf("ERROR:[VALIDATION] failed for size: %lu @index:%lu" \
                    "[str1: %p(alignment = %u), str2:%p(alignment = %u)]\n", \
              size, index, str1_alnd_addr, str1_alnmnt, str2_alnd_addr, str2_alnmnt);
    else
        ALM_VERBOSE_LOG("Data Validation passed for size: %lu\n", size);
    //validation of return value
    if (ret != str2_alnd_addr)
        printf("ERROR:[RETURN] value mismatch: expected - %p, actual - %p\n", \
                                                             str2_alnd_addr, ret);

    //Multi-Null check
    size_t more_null_idx = rand() % (size);
    *(str1_alnd_addr + more_null_idx) = '\0';

    ret = strcpy((char *)str2_alnd_addr, (char *)str1_alnd_addr);
    for (index = 0; (index <= more_null_idx) && (*(str2_alnd_addr + index) == \
                            *(str1_alnd_addr + index)); index ++);

    if (index != (more_null_idx + 1))
        printf("ERROR:[VALIDATION] Multi-NULL failed for size: %lu @index:%lu" \
                    "[str1: %p(alignment = %u), str2:%p(alignment = %u)]\n", \
              size, index, str1_alnd_addr, str1_alnmnt, str2_alnd_addr, str2_alnmnt);
    else
        ALM_VERBOSE_LOG("Multi-NULL check Validation passed for size: %lu\n", size);
    //validation of return value
    if (ret != str2_alnd_addr)
        printf("ERROR: [RETURN] Multi-NULL value mismatch: expected - %p, actual - %p\n", \
                                                             str2_alnd_addr, ret);

    if (boundary_check(str2_alnd_addr, size))
        printf("[str1: %p(alignment = %u), str2:%p(alignment = %u)]\n",str1_alnd_addr, str1_alnmnt, str2_alnd_addr, str2_alnmnt);

    //Page_check
    void *page_buff = NULL;
    posix_memalign(&page_buff, PAGE_SZ, size + PAGE_SZ + CACHE_LINE_SZ);

    if (page_buff == NULL)
    {
        perror("Failed to allocate memory");
        exit(-1);
    }

    str1_alnd_addr = (uint8_t *)page_buff + PAGE_SZ - VEC_SZ + str1_alnmnt;

    for (index = 0; index < size; index++)
    {
        *(str1_alnd_addr +index) = ((char) 'a' + rand() % 26);
    }
    *(str1_alnd_addr + size -1) ='\0';

    ret = strcpy((char *)str2_alnd_addr, (char *)str1_alnd_addr);

    for (index = 0; (index < size) && (*(str2_alnd_addr + index) == \
                            *(str1_alnd_addr + index)); index ++);

    if (index != (size))
        printf("ERROR:[PAGE-CROSS] validation failed for size: %lu @index:%lu" \
                    "[str1: %p(alignment = %u), str2:%p(alignment = %u)]\n", \
              size, index, str1_alnd_addr, str1_alnmnt, str2_alnd_addr, str2_alnmnt);
    else
        ALM_VERBOSE_LOG("Page-cross validation passed for size: %lu\n", size);
    //validation of return value
    if (ret != str2_alnd_addr)
        printf("ERROR:[PAGE-CROSS] Return value mismatch: expected - %p, actual - %p\n", \
                                                             str2_alnd_addr, ret);
    free(page_buff);
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
            printf("ERROR:[RETURN] value mismatch for size(%lu): expected - %p"\
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
        *(str1_alnd_addr + index) = 'a' + (char)(rand() % 26);
    }
    for(index = size; index <= size + BOUNDARY_BYTES; index++)
        *(str2_alnd_addr + index) = '#';

    //CASE 1:validation when NULL terminating char is beyond strlen
    ret = strncpy((char *)str2_alnd_addr, (char *)str1_alnd_addr, size);
    //validation of str2 memory
    for (index = 0; (index < size) && (*(str2_alnd_addr + index) == \
                            *(str1_alnd_addr + index)); index ++);

    if (index != size)
        printf("ERROR:[VALIDATION] (strlen > n) failed for size: %lu @index:%lu" \
                    "[str1: %p(alignment = %u), str2:%p(alignment = %u)]\n", \
              size, index, str1_alnd_addr, str1_alnmnt, str2_alnd_addr, str2_alnmnt);
    else
        ALM_VERBOSE_LOG("[strlen > n] Data Validation passed for size: %lu\n", size);
    //validation of return value
    if (ret != str2_alnd_addr)
        printf("ERROR:[RETURN] (strlen > n) value mismatch: expected - %p, actual - %p\n",
                                                             str2_alnd_addr, ret);


    //Check if the str2 buffer was modified after 'n bytes' copy
    if (boundary_check(str2_alnd_addr, size))
        printf("[str1: %p(alignment = %u), str2:%p(alignment = %u)]\n",str1_alnd_addr, str1_alnmnt, str2_alnd_addr, str2_alnmnt);


    //CASE 2:validation when index of NULL char is equal to strlen
    //Appending Null Charachter at the end of str1 string
    *(str1_alnd_addr + size - 1) = '\0';

    //Passing the size including the NULL char(size+1)
    ret = strncpy((char *)str2_alnd_addr, (char *)str1_alnd_addr, size);

    //validation of str2 memory
    for (index = 0; (index < size) && (*(str2_alnd_addr + index) == \
                            *(str1_alnd_addr + index)); index ++);

    if (index != size)
        printf("ERROR:[VALIDATION] (strlen = n) failed for size: %lu @index:%lu" \
                    "[str1: %p(alignment = %u), str2:%p(alignment = %u)]\n", \
              size, index, str1_alnd_addr, str1_alnmnt, str2_alnd_addr, str2_alnmnt);
    else
        ALM_VERBOSE_LOG("[strlen = n] Data Validation passed for size: %lu\n", size);
    //validation of return value
    if (ret != str2_alnd_addr)
        printf("ERROR:[RETURN] (strlen = n) Return value mismatch: expected - %p, actual - %p\n", \
                                                             str2_alnd_addr, ret);

    //Check if the str2 buffer was modified after 'n bytes' copy
    if (boundary_check(str2_alnd_addr, size))
        printf("[str1: %p(alignment = %u), str2:%p(alignment = %u)]\n",str1_alnd_addr, str1_alnmnt, str2_alnd_addr, str2_alnmnt);


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
        printf("ERROR:[VALIDATION] (strlen < n) failed for size: %lu @index:%lu" \
                    "[str1: %p(alignment = %u), str2:%p(alignment = %u)] (strlen = %lu)\n", \
              size, index, str1_alnd_addr, str1_alnmnt, str2_alnd_addr, str2_alnmnt, str1_len);
    //Checking for NULL after str1_len in str2 buffer
    for (;index < size; index++)
    {
        if (str2_alnd_addr[index] != '\0')
        {
            printf("ERROR:[VALIDATION] (strlen < n) NULL Validation failed at index:%lu" \
                        " for size: %ld(strlen = %ld)\n", index, size, str1_len);
            break;
        }
    }
    if (index == size)
        ALM_VERBOSE_LOG("[strlen < n] Data Validation passed for size: %lu\n", size);
    //validation of return value
    if (ret != str2_alnd_addr)
        printf("ERROR:[RETURN] (strlen < n) value mismatch: expected - %p, actual - %p\n", \
                                                             str2_alnd_addr, ret);

    //Check if the str2 buffer was modified after 'n bytes' copy
    if (boundary_check(str2_alnd_addr, size))
        printf("[str1: %p(alignment = %u), str2:%p(alignment = %u)]\n",str1_alnd_addr, str1_alnmnt, str2_alnd_addr, str2_alnmnt);


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
            printf("ERROR:[RETURN] value mismatch for size(%lu): expected - %d"\
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
        printf("ERROR:[VALIDATION] failed for matching data of str1_aln:%u str2_aln:%u size: %lu,"\
                                    " return_value = %d\n",str1_alnmnt,str2_alnmnt, size, ret);
    }
    else
    {
        ALM_VERBOSE_LOG("Validation passed for matching memory of size: %lu\n", size);
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
           printf("ERROR:[VALIDATION] (str1<str2) failed for Non-Matching @index:%lu str1_aln:%u str2_aln:%u size: %lu,"\
                                    " return_value = %d exp=%d\n", index, str1_alnmnt, str2_alnmnt, size, ret, exp_ret);
           validation1_passed = 0;
       }

       //Case2(b): s1 > s2
       ret = strcmp((char *)str2_alnd_addr, (char *)str1_alnd_addr); // +ve value expected
       exp_ret = (*(uint8_t *)(str2_alnd_addr + index) - \
                        *(uint8_t *)(str1_alnd_addr + index));
       if (ret != exp_ret)
       {
           printf("ERROR:[VALIDATION] (str1>str2) failed for Non-Matching @index:%lu str1_aln:%u str2_aln:%u size: %lu,"\
                                    " return_value = %d exp=%d\n", index, str1_alnmnt, str2_alnmnt, size, ret, exp_ret);
           validation2_passed = 0;
       }
       //copying back the mismatching byte
       *(str1_alnd_addr + index) = *(str2_alnd_addr + index);
    }

    if (validation1_passed && validation2_passed)
         ALM_VERBOSE_LOG("Validation successfull for non-matching data of size: %lu\n",\
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
            printf("ERROR:[VALIDATION] Multi-NULL failed for size: %lu @Mismatching index:%lu" \
                    "[str1: %p(alignment = %u), str2:%p(alignment = %u)]\n", \
                size, more_null_idx + 1, str1_alnd_addr, str1_alnmnt, str2_alnd_addr, str2_alnmnt);

        }
        else
            ALM_VERBOSE_LOG("Multi-NULL check Validation passed for size: %lu\n", size);
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
        printf("ERROR:[VALIDATION] (str1(%lu) & str2(%lu) >size) failed for str1_aln:%u str2_aln:%u size: %lu,"\
                                    " return_value = %d, exp=%d",strlen((char*)str1_alnd_addr),strlen((char*)str2_alnd_addr),str1_alnmnt,str2_alnmnt, size, ret, exp_ret);
    }

    //case5: strlen(str1) = size and strlen(str2) > size

    *(str1_alnd_addr + size -1) = '\0';
    ret = strcmp((char *)str2_alnd_addr, (char *)str1_alnd_addr);
    exp_ret = (*(uint8_t *)(str2_alnd_addr + size - 1) - \
                        *(uint8_t *)(str1_alnd_addr + size - 1));
    if (ret != exp_ret)
    {
        printf("ERROR:[VALIDATION] (str1=size, str2(%lu) >size) failed for string @index:%lu str1_aln:%u str2_aln:%u size: %lu,"\
                                    " return_value = %d\n",strlen((char*)str2_alnd_addr), size-1,str1_alnmnt,str2_alnmnt, size, ret);
    }

    //case6:strlen(str2) = size and strlen(str1) > size

    ret = strcmp((char *)str1_alnd_addr,(char *)str2_alnd_addr);
    exp_ret = (*(uint8_t *)(str1_alnd_addr + size - 1) - \
                        *(uint8_t *)(str2_alnd_addr + size - 1));

    if (ret != exp_ret)
    {
        printf("ERROR:[VALIDATION] (str2=size, str1(%lu) >size) failed for string @index:%lu str1_aln:%u str2_aln:%u size: %lu,"\
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
           printf("ERROR:[VALIDATION] (str1_sz(%lu) < str2_sz(%lu)) failed for Non-Matching string str1_aln:%u str2_aln:%u,"\
                                    " return_value = %d, exp_value =%d\n",s1_sz, s2_sz, str1_alnmnt,str2_alnmnt, ret,exp_ret);
        }

        //case7(b): strlen(str1) > strlen(str2)
        ret = strcmp((char *)str1_alnd_addr, (char *)str2_alnd_addr);
        exp_ret = string_cmp((char *)str1_alnd_addr, (char *)str2_alnd_addr, -1);

        if (ret != exp_ret)
        {
           printf("ERROR:[VALIDATION] (str1_sz(%lu) > str2_sz(%lu)) failed for Non-Matching string str1_aln:%u str2_aln:%u size: %lu,"\
                                    " return_value = %d, exp_value=%d\n",s1_sz, s2_sz, str1_alnmnt,str2_alnmnt, size, ret,exp_ret);
        }
    }

    //Page_check
    void *page_buff = NULL;
    posix_memalign(&page_buff, PAGE_SZ, size + PAGE_SZ + 2 * CACHE_LINE_SZ);

    if (page_buff == NULL)
    {
        perror("Failed to allocate memory");
        exit(-1);
    }

    str1_alnd_addr = (uint8_t *)page_buff + PAGE_SZ - VEC_SZ + str1_alnmnt;

    srand(time(0));

    for (index = 0; index < size; index++)
    {
        *(str1_alnd_addr +index) = *(str2_alnd_addr +index) = ((char) 'a' + rand() % 26);
    }
    *(str1_alnd_addr + size -1) = *(str2_alnd_addr + size -1) ='\0';

    ret = strcmp((char *)str2_alnd_addr, (char *)str1_alnd_addr);
    if (ret != 0 )
    {
        printf("ERROR:[PAGE-CROSS] failure for str1_aln:%u size: %lu\n", str1_alnmnt, size);
    }
    free(page_buff);
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
            printf("ERROR:[VALIDATION] failure for size(%lu): expected - %d"\
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
        printf("ERROR:[VALIDATION] Matching failure for str1_aln:%u str2_aln:%u size: %lu,"\
                                    " return_value = %d\n",str1_alnmnt,str2_alnmnt, size, ret);
    }
    else
    {
        ALM_VERBOSE_LOG("Validation passed for matching memory of size: %lu\n", size);
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
           printf("ERROR:[VALIDATION] (str1<str2) failure for Non-Matching @index:%lu str1_aln:%u str2_aln:%u size: %lu,"\
                                    " return_value = %d exp=%d\n", index, str1_alnmnt, str2_alnmnt, size, ret, exp_ret);
           validation1_passed = 0;
       }

       //Case2(b): s1 > s2
       ret = strncmp((char *)str2_alnd_addr, (char *)str1_alnd_addr, size); // +ve value expected
       exp_ret = (*(uint8_t *)(str2_alnd_addr + index) - \
                        *(uint8_t *)(str1_alnd_addr + index));
       if (ret != exp_ret)
       {
           printf("ERROR:[VALIDATION] (str1>str2) failure for Non-Matching @index:%lu str1_aln:%u str2_aln:%u size: %lu,"\
                                    " return_value = %d exp=%d\n", index, str1_alnmnt, str2_alnmnt, size, ret, exp_ret);
           validation2_passed = 0;
       }
       //copying back the mismatching byte
       *(str1_alnd_addr + index) = *(str2_alnd_addr + index);
    }

    if (validation1_passed && validation2_passed)
         ALM_VERBOSE_LOG("Validation successfull for non-matching data of size: %lu\n",\
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
            printf("ERROR:[VALIDATION] Multi-NULL failed for size: %lu @Mismatching index:%lu" \
                    "[str1: %p(alignment = %u), str2:%p(alignment = %u)]\n", \
                size, more_null_idx + 1, str1_alnd_addr, str1_alnmnt, str2_alnd_addr, str2_alnmnt);

        }
        else
            ALM_VERBOSE_LOG("Multi-NULL check Validation passed for size: %lu\n", size);
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
        printf("ERROR:[VALIDATION] (str1(%lu) & str2(%lu) > size) failure for str1_aln:%u str2_aln:%u size: %lu,"\
                                    " return_value = %d, exp=%d",strlen((char*)str1_alnd_addr),strlen((char*)str2_alnd_addr),str1_alnmnt,str2_alnmnt, size, ret, exp_ret);
    }

    //case5: strlen(str1) = size and strlen(str2) > size

    *(str1_alnd_addr + size -1) = '\0';
    ret = strncmp((char *)str2_alnd_addr, (char *)str1_alnd_addr, size);
    exp_ret = (*(uint8_t *)(str2_alnd_addr + size - 1) - \
                        *(uint8_t *)(str1_alnd_addr + size - 1));
    if (ret != exp_ret)
    {
        printf("ERROR:[VALIDATION] (str1=size, str2(%lu) > size) failure for string @index:%lu str1_aln:%u str2_aln:%u size: %lu,"\
                                    " return_value = %d\n", strlen((char*)str2_alnd_addr), size-1,str1_alnmnt,str2_alnmnt, size, ret);
    }

    //case6:strlen(str2) = size and strlen(str1) > size

    ret = strncmp((char *)str1_alnd_addr,(char *)str2_alnd_addr, size);
    exp_ret = (*(uint8_t *)(str1_alnd_addr + size - 1) - \
                        *(uint8_t *)(str2_alnd_addr + size - 1));

    if (ret != exp_ret)
    {
        printf("ERROR:[VALIDATION] (str2=size, str1(%lu) > size) failure for string @index:%lu str1_aln:%u str2_aln:%u size: %lu,"\
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
           printf("ERROR:[VALIDATION] (str1(%lu) < str2(%lu)) failure for Non-Matching string str1_aln:%u str2_aln:%u ,"\
                                    " return_value = %d, exp_value =%d\n", s1_sz, s2_sz, str1_alnmnt, str2_alnmnt, ret, exp_ret);
        }

        //case7(b): strlen(str1) > strlen(str2)
        ret = strncmp((char *)str1_alnd_addr, (char *)str2_alnd_addr, size);
        exp_ret = string_cmp((char *)str1_alnd_addr, (char *)str2_alnd_addr, size);

        if (ret != exp_ret)
        {
           printf("ERROR:[VALIDATION] (str1_sz(%lu) > str2_sz(%lu)) failure for Non-Matching string str1_aln:%u str2_aln:%u size: %lu,"\
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
        printf("ERROR:[VALIDATION] failure for strlen of str1_aln:%u size: %lu,"\
                                    " return_value = %d\n",str1_alnmnt, size, ret);
    }
    else
    {
        ALM_VERBOSE_LOG("Validation passed for strlen: %lu\n", size);
    }

    //Page_check
    void *page_buff = NULL;
    posix_memalign(&page_buff, PAGE_SZ, size + PAGE_SZ + CACHE_LINE_SZ);

    if (page_buff == NULL)
    {
        perror("Failed to allocate memory");
        exit(-1);
    }

    str_alnd_addr = (uint8_t *)page_buff + PAGE_SZ - VEC_SZ + str1_alnmnt;

    srand(time(0));

    for (index = 0; index < size; index++)
    {
        *(str_alnd_addr +index) = ((char) 'a' + rand() % 26);
    }
    *(str_alnd_addr + size) ='\0';

    ret = strlen((char *)str_alnd_addr);
    if (ret != size )
    {
        printf("ERROR:[PAGE-CROSS] failure for str1_aln:%u size: %lu\n", str1_alnmnt, size);
    }

    free(page_buff);
    free(buff);
}

static inline void memchr_validator(size_t size, uint32_t str2_alnmnt,\
                                                 uint32_t str1_alnmnt)
{
    uint8_t *buff = NULL, *buff_head, *buff_tail;
    uint8_t *str_alnd_addr = NULL;
    size_t index, pos = 0;
    char* res;
    int find = '#';
    buff = alloc_buffer(&buff_head, &buff_tail, size + BOUNDARY_BYTES, NON_OVERLAP_BUFFER);

    if (buff == NULL)
    {
        perror("Failed to allocate memory");
        exit(-1);
    }
    srand(time(0));
    str_alnd_addr = buff_tail + str1_alnmnt;

    if (size == 0)
    {
        res = memchr((char *)str_alnd_addr, find, size);
        if (res != NULL)
            printf("ERROR:[RETURN] value mismatch for size(%lu): expected - NULL"\
                        ", actual - %p\n", size, res);
        return;
    }

    init_buffer((char*)str_alnd_addr, size);
    prepare_boundary(str_alnd_addr, size);

    find = '#'; //ASCII value 35
    res = memchr((char *)str_alnd_addr, find, size);

    if(res != NULL)
    {
        printf("ERROR:[BOUNDARY] Out of bound Data failure for memchr of str1_aln:%u size: %lu, find:%c\n"\
                                    " return_value =%s\n EXP:NULL\n STR:%s\n",str1_alnmnt, size, find, res, str_alnd_addr);
    }

    init_buffer((char*)str_alnd_addr, size);
    find = '!'; // ASCII value 33
    res = memchr((char *)str_alnd_addr, find, size);

    if (res != test_memchr((char *)str_alnd_addr, find, size))
    {
        printf("ERROR:[VALIDATION] failure for memchr of str1_aln:%u size: %lu, find:%c\n"\
                                    " return_value =%s\n STR:%s\n",str1_alnmnt, size, find, res, str_alnd_addr);
    }
    else
    {
        ALM_VERBOSE_LOG("Validation passed for memchr: %lu\n", size);
    }

    //Modifying the needle char
    find = ' '; //ASCII value 32 (SPACE)
    res = memchr((char *)str_alnd_addr, find, size);

    if (res != NULL)
    {
        printf("ERROR:[VALIDATION] failure for memchr of str1_aln:%u size: %lu, find:%c\n"\
                                    " return_value =%s\n EXP:NULL\n STR:%s\n",str1_alnmnt, size, find, res, str_alnd_addr);
    }
    else
    {
        ALM_VERBOSE_LOG("Validation passed for memchr: %lu\n", size);
    }

    //PageCross_check
    void *page_buff = NULL;
    posix_memalign(&page_buff, PAGE_SZ, size + PAGE_SZ + CACHE_LINE_SZ);

    if (page_buff == NULL)
    {
        perror("Failed to allocate memory");
        exit(-1);
    }

    str_alnd_addr = (uint8_t *)page_buff + PAGE_SZ - VEC_SZ + str1_alnmnt;

    init_buffer((char*)str_alnd_addr, size);
    prepare_boundary(str_alnd_addr, size);
    find = '#';
    res = memchr((char *)str_alnd_addr, find, size);

    if(res != NULL)
    {
        printf("ERROR:[PAGE_BOUNDARY] Out of bound Data failure for memchr of str1_aln:%u size: %lu, find:%c\n"\
                                    " return_value =%s\n EXP:NULL\n STR:%s\n",str1_alnmnt, size, find, res, str_alnd_addr);
    }

    *(str_alnd_addr + size -1) = find; //pos of the needle at the end for page_check

    res = memchr((char *)str_alnd_addr, find, size);
    if (res != test_memchr((char *)str_alnd_addr, find, size))
    {
        printf("ERROR:[PAGE-CROSS] failure for str1_aln:%u size: %lu, find:%c\n"\
                                    " return_value =%s\n STR:%s\n", str1_alnmnt, size, find, res, str_alnd_addr);
    }

    init_buffer((char*)str_alnd_addr, size);
    find = '!';
    res = memchr((char *)str_alnd_addr, find, size);

    if (res != test_memchr((char *)str_alnd_addr, find, size))
    {
        printf("ERROR:[PAGE-CROSS] failure for str1_aln:%u size: %lu, find:%c\n"\
                                    " return_value =%s\n STR:%s\n", str1_alnmnt, size, find, res, str_alnd_addr);
    }

    free(page_buff);
    free(buff);
}

static inline void strcat_validator(size_t size, uint32_t str2_alnmnt,\
                                                 uint32_t str1_alnmnt)
{
    uint8_t *buff = NULL, *temp_buff = NULL, *buff_head, *buff_tail;
    uint8_t *str2_alnd_addr = NULL, *str1_alnd_addr = NULL;
    size_t index;
    void *ret = NULL;
    srand(time(0));

    buff = alloc_buffer(&buff_head, &buff_tail, size, NON_OVERLAP_BUFFER_EXTRA);

    if (buff == NULL)
    {
        perror("Failed to allocate memory");
        exit(-1);
    }
    if(size == 0) //For Null expected behaviour is SEGFAULT
    {
        return;
    }

    str2_alnd_addr = buff_tail + str2_alnmnt;
    str1_alnd_addr = buff_head + str1_alnmnt; // String size big enough to accommodate str2

    //intialize str1 & str2 memory
    //TODO: Need to add /n in the string
    for (index = 0; index < size; index++)
    {
        *(str1_alnd_addr + index) = 'a' + (char)(rand() % 26);
        *(str2_alnd_addr + index) = 'a' + (char)(rand() % 26);
    }
    //Appending Null Charachter at the end of str1 string
    *(str1_alnd_addr + size - 1) = '\0';
    *(str2_alnd_addr + (rand()%size)) = '\0';
    //Source Corruption
    temp_buff = alloc_buffer(&buff_head, &buff_tail, 2 * size, DEFAULT);

    if (temp_buff == NULL)
    {
        perror("Failed to allocate memory");
        free(buff);
        exit(-1);
    }
    uint8_t *tmp_alnd_addr = NULL;
    tmp_alnd_addr = buff_tail + str1_alnmnt;

    strcpy((char *)tmp_alnd_addr, (char *)str1_alnd_addr);
    strcat((char *)str1_alnd_addr, (char *)str2_alnd_addr);

    if(strcmp(test_strcat((char *)tmp_alnd_addr, (char *)str2_alnd_addr),(char *)str1_alnd_addr) != 0)
    {
        printf("ERROR: [VALIDATION] failed\n str1:%s\n str2:%s\n str1+str2:%s\n",tmp_alnd_addr, str2_alnd_addr,str1_alnd_addr);
    }

    //Multi-Null check
    size_t more_null_idx = rand() % (size);
    *(tmp_alnd_addr + more_null_idx) = '\0';
    strcpy((char *)str1_alnd_addr, (char *)tmp_alnd_addr);

    *(str2_alnd_addr + more_null_idx) = '\0';
    strcat((char *)str1_alnd_addr, (char *)str2_alnd_addr);

    if(strcmp(test_strcat((char *)tmp_alnd_addr, (char *)str2_alnd_addr),(char *)str1_alnd_addr ) != 0)
    {
        printf("ERROR: [VALIDATION] MultiNull check failed\n str1:%s\n str2:%s\n str1+str2:%s\n",tmp_alnd_addr, str2_alnd_addr,str1_alnd_addr);
    }

    free(buff);
    //Page_check
    void *str1_page_buff = NULL, *str2_page_buff = NULL, *temp_page_buff = NULL;

    posix_memalign(&str1_page_buff, PAGE_SZ,  2 * size + PAGE_SZ + CACHE_LINE_SZ);
    if (str1_page_buff == NULL)
    {
        perror("Failed to allocate memory");
        exit(-1);
    }

    posix_memalign(&temp_page_buff, PAGE_SZ,  2 * size + PAGE_SZ + CACHE_LINE_SZ);
    if (temp_page_buff == NULL)
    {
        perror("Failed to allocate memory");
        exit(-1);
    }

    posix_memalign(&str2_page_buff, PAGE_SZ,  size + PAGE_SZ + CACHE_LINE_SZ);
    if (str2_page_buff == NULL)
    {
        perror("Failed to allocate memory");
        exit(-1);
    }

    str1_alnd_addr = (uint8_t *)str1_page_buff + PAGE_SZ - VEC_SZ + str1_alnmnt;
    str2_alnd_addr = (uint8_t *)str2_page_buff + PAGE_SZ - VEC_SZ + str2_alnmnt;
    tmp_alnd_addr = (uint8_t *)temp_page_buff + PAGE_SZ - VEC_SZ + str1_alnmnt;

    for (index = 0; index < size; index++)
    {
        *(str1_alnd_addr +index) = ((char) 'a' + rand() % 26);
        *(str2_alnd_addr +index) = ((char) 'a' + rand() % 26);
    }
    *(str1_alnd_addr + size -1) = *(str2_alnd_addr + (rand()%size)) = '\0';

    strcpy((char *)tmp_alnd_addr, (char *)str1_alnd_addr);
    strcat((char *)str1_alnd_addr, (char *)str2_alnd_addr);

    if(strcmp(test_strcat((char *)tmp_alnd_addr, (char *)str2_alnd_addr), (char *)str1_alnd_addr) != 0)
    {
        printf("ERROR: [PAGE-CROSS] failed\n str1:%s\n str2:%s\n str1+str2:%s\n",tmp_alnd_addr, str2_alnd_addr,str1_alnd_addr);
    }

    //Page_check with Multi-NULL
    more_null_idx = rand() % size;
    *(tmp_alnd_addr + more_null_idx) = '\0';
    strcpy((char *)str1_alnd_addr, (char *)tmp_alnd_addr);

    *(str2_alnd_addr + more_null_idx) = '\0';
    strcat((char *)str1_alnd_addr, (char *)str2_alnd_addr);

    if(strcmp(test_strcat((char *)tmp_alnd_addr, (char *)str2_alnd_addr),(char *)str1_alnd_addr) != 0)
    {
        printf("ERROR: [PAGE-CROSS] MultiNull check failed\n str1:%s\n str2:%s\n str1+str2:%s\n",tmp_alnd_addr, str2_alnd_addr,str1_alnd_addr);
    }

    free(temp_buff);
    free(str1_page_buff);
    free(str2_page_buff);
    free(temp_page_buff);
}

static inline void strstr_validator(size_t size, uint32_t str2_alnmnt,\
                                                 uint32_t str1_alnmnt)
{
    uint8_t *buff = NULL, *buff_head, *buff_tail;
    uint8_t *str_alnd_addr = NULL, *find_alnd_addr = NULL;
    char *find, *res;
    size_t modified_size;
    void *page_buff = NULL;

    buff = alloc_buffer(&buff_head, &buff_tail, size + BOUNDARY_BYTES, NON_OVERLAP_BUFFER);
    if (buff == NULL)
    {
        perror("Failed to allocate memory");
        exit(-1);
    }
    srand(time(0));
    str_alnd_addr = buff_tail + str1_alnmnt;

    if (size == 0)
    {
        res = strstr((char *)str_alnd_addr, find);
        if (res != NULL)
            printf("ERROR:[RETURN] value mismatch for size(%lu): expected - NULL"\
                        ", actual - %p\n", size, res);
        return;
    }


    //case - 1 : zero length strings, expect to return NULL
    find = " ";
    str_alnd_addr = buff_tail + str1_alnmnt; // Reset to valid buffer location
    *str_alnd_addr = '\0'; // Set the buffer to an empty string
    res = strstr((char *)str_alnd_addr, find);
    if (res != NULL) {
        printf("ERROR:[BOUNDARY] Out of bound Data failure for strstr of str1_aln:%u size: %lu, find:%s\n"
               " return_value = %p\n EXP: NULL\n STR: %s\n", str1_alnmnt, size, find, res, str_alnd_addr);
    }

    //case - 2 : empty needle and non-empty haystack, expect to return pointer to haystack
    init_buffer((char*)str_alnd_addr, size);
    prepare_boundary(str_alnd_addr, size);
    find = " ";
    res = strstr((char *)str_alnd_addr, find);
    if(res != test_strstr((char *)str_alnd_addr, find))
    {
        printf("ERROR:[VALIDATION] failure for strstr of str1_aln:%u size: %lu, find:%s\n"\
                                    " return_value =%s\n STR:%s\n",str1_alnmnt, size, find, res, str_alnd_addr);
    }

    //case - 3 : empty haystack and non-empty needle, expect to return NULL
    find = "needle";
    str_alnd_addr = (uint8_t *)"";
    res = strstr((char *)str_alnd_addr,find);
    if(res != NULL)
    {
        printf("ERROR:[BOUNDARY] Out of bound Data failure for strstr of str1_aln:%u size: %lu, find:%s\n"\
                                    " return_value =%s\n EXP:NULL\n STR:%s\n",str1_alnmnt, size, find, res, str_alnd_addr);
    }

    //case - 4 : needle longer than haystack, expect to return NULL
    str_alnd_addr = buff_tail + str1_alnmnt;
    init_buffer((char*)str_alnd_addr, size);
    prepare_boundary(str_alnd_addr, size);
    modified_size = size + 1;
    uint8_t *find_alnd_addr_original = (uint8_t*)malloc(modified_size + BOUNDARY_BYTES * 2);
    if (find_alnd_addr_original == NULL)
    {
        perror("Failed to allocate memory for find_alnd_addr");
        free(buff);
        exit(-1);
    }

    find_alnd_addr = find_alnd_addr_original + BOUNDARY_BYTES; // Make space for boundaries
    init_buffer((char*)find_alnd_addr, modified_size);
    prepare_boundary(find_alnd_addr, modified_size);
    res = strstr((char *)str_alnd_addr,(char *)find_alnd_addr);
    if(res != NULL)
    {
        printf("ERROR:[BOUNDARY] Out of bound Data failure for strstr of str1_aln:%u size: %lu, find:%s\n"\
                                    " return_value =%s\n EXP:NULL\n STR:%s\n",str1_alnmnt, size, find, res, str_alnd_addr);
    }

    //case - 5 : Multiple occurrences of the needle

    str_alnd_addr = buff_tail + str1_alnmnt;
    size_t num_needles = (size < 5) ? size : 5;
    // Initialize buffer with 'a's and null-terminate it
    init_buffer((char *)str_alnd_addr, size);
    prepare_boundary(str_alnd_addr, size);

    // Append multiple needles to the haystack
    find = "needle";
    size_t needle_len = strlen(find);
    if (size == 1) {
        str_alnd_addr[size - 1] = 'n'; // Append "n" for size 1
        find = "n";
    } else if (size >= 2 && size <= 6) {
        str_alnd_addr[size - 2] = 'n'; // Append "ne" for sizes 2 to 6
        str_alnd_addr[size - 1] = 'e';
        find = "ne";
    } else {
        for (size_t i = 0; i < num_needles; ++i) {
            size_t pos = rand() % (size - needle_len); // Random position in the haystack
            for (size_t j = 0; j < needle_len; ++j) {
                str_alnd_addr[pos + j] = find[j]; // Append needle at the position
            }
        }
    }
    res = strstr((char *)str_alnd_addr, find);
    if(res != test_strstr((char *)str_alnd_addr, find))
    {
        printf("ERROR:[VALIDATION] failure for strstr of str1_aln:%u size: %lu, find:%s\n"\
                                    " return_value =%s\n STR:%s\n",str1_alnmnt, size, find, res, str_alnd_addr);
    } 
    //case - 6 : PageCross_check
    posix_memalign(&page_buff, PAGE_SZ, size + PAGE_SZ + CACHE_LINE_SZ);

    if (page_buff == NULL)
    {
        perror("Failed to allocate memory");
        exit(-1);
    }

    str_alnd_addr = (uint8_t *)page_buff + PAGE_SZ - VEC_SZ + str1_alnmnt;

    init_buffer((char*)str_alnd_addr, size);
    prepare_boundary(str_alnd_addr, size);
    find = " ";
    res = strstr((char *)str_alnd_addr, find);

    if(res != NULL)
    {
        printf("ERROR:[PAGE_BOUNDARY] Out of bound Data failure for strstr of str1_aln:%u size: %lu, find:%s\n"\
                                    " return_value =%s\n EXP:NULL\n STR:%s\n",str1_alnmnt, size, find, res, str_alnd_addr);
    }

    *(uint8_t**)(str_alnd_addr + size -1) = (uint8_t *)find; //pos of the needle at the end for page_check

    res = strstr((char *)str_alnd_addr, find);
    if (res != test_strstr((char *)str_alnd_addr, find))
    {
        printf("ERROR:[PAGE-CROSS] failure for str1_aln:%u size: %lu, find:%s\n"\
                                    " return_value =%s\n STR:%s\n", str1_alnmnt, size, find, res, str_alnd_addr);
    }

    //case - 7 : testing with randomly generated needles
    str_alnd_addr = buff_tail + str1_alnmnt; // Reset to valid buffer location
    init_buffer((char*)str_alnd_addr, size);
    prepare_boundary(str_alnd_addr, size);
    // Generating a random number between 0 and size
    size_t start = rand() % size;  // Random start position of needle
    size_t end = start + rand() % (size - start);  // Random end position of needle
    char* needle = (char*)malloc((end - start + 1) * sizeof(char));
    if (!needle) {
        fprintf(stderr, "Memory allocation failed\n");
        free(str_alnd_addr);
        exit(1);
    }
    for (size_t i = 0; i < (end-start); ++i) {
        needle[i] = str_alnd_addr[start + i];
        if (str_alnd_addr[start + i] == '\0') {
            break;  // Stop copying if end of string (null terminator) is encountered
        }
    }
    needle[end - start] = '\0';
    res = strstr((char *)str_alnd_addr, (char *)needle);
    if(res != test_strstr((char *)str_alnd_addr,(char *)needle) )
    {
        printf("ERROR:[VALIDATION] failure for strstr of str1_aln:%u size: %lu, find:%s\n"\
                                    " return_value =%s\n STR:%s\n",str1_alnmnt, size, find, res, (char *)str_alnd_addr);
    }

    //case 8: Testing with haystack which contains needle substrings
    size_t length = 1; //needle length
    while (length * length <= size) {
        length++;
    }
    length--;
    char* str = (char*)malloc((length + BOUNDARY_BYTES) * 2); //memory allocation for needle
    init_buffer((char*)str, length);
    char* haystack = (char*)malloc(size + BOUNDARY_BYTES * 2);
    if (!haystack) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    haystack[0] = '\0';
    size_t haystack_len = 0;
    for (size_t i = 0; i < length; i++) {
        // Append i characters of needle to haystack
        for (size_t j = 0; j < i; j++) {
            if (haystack_len < size) {
                haystack[haystack_len++] = str[j];
            }
        }

        if (haystack_len <= size) {
            haystack[haystack_len++] = 'A' + rand() % 26;
        }
    }

    while (haystack_len + length <= size) {
        haystack[haystack_len++] = 'A' + rand() % 26;
    }

    for (size_t i = 0; i < length; i++) {
        haystack[haystack_len++] = str[i];
    }
    haystack[haystack_len] = '\0';

    res = strstr(haystack, str);
    if (res != test_strstr(haystack, str))
    {
        printf("ERROR:[VALIDATION] failure for strstr of str1_aln:%u size:%lu, find:%s\n"
               " return_value =%s\n STR:%s\n", str1_alnmnt, size, str, res, haystack);
    }
    // removing needle from the haystack
    haystack_len -= length;
    haystack[haystack_len] = '\0';
    res = strstr(haystack, str);
    if (res != test_strstr(haystack, str))
    {
        printf("ERROR:[VALIDATION] failure for strstr of str1_aln:%u size:%lu, find:%s\n"
               " return_value =%s\n STR:%s\n", str1_alnmnt, size, str, res, haystack);
    }

    free(needle);
    free(page_buff);
    free(haystack);
    free(str);
    free(find_alnd_addr_original); // Free the original allocated address
    free(buff);
}


libmem_func supp_funcs[]=
{
    {"memcpy",  memcpy_validator},
    {"mempcpy", mempcpy_validator},
    {"memmove", memmove_validator},
    {"memset",  memset_validator},
    {"memcmp",  memcmp_validator},
    {"memchr",  memchr_validator},
    {"strcpy",  strcpy_validator},
    {"strncpy", strncpy_validator},
    {"strcmp",  strcmp_validator},
    {"strncmp", strncmp_validator},
    {"strlen",  strlen_validator},
    {"strcat",  strcat_validator},
    {"strstr",  strstr_validator},
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
        src_alignment = atoi(argv[3]) % VEC_SZ;

    if (argv[4] != NULL)
        dst_alignment = atoi(argv[4]) % VEC_SZ;

    srand(time(0));

    if (argv[5] != NULL)
        al_check = atoi(argv[5]); //Check for alignment validation test.

    if (al_check == 0)
    {
        lm_func_validator->func(size, dst_alignment, src_alignment);
    }

    else
    {
        for(unsigned int aln_src  = 0; aln_src < VEC_SZ; aln_src++)
        {
            for(unsigned int aln_dst = 0; aln_dst < VEC_SZ; aln_dst++)
            {
                lm_func_validator->func(size, aln_dst, aln_src);
            }
        }
    }
}
