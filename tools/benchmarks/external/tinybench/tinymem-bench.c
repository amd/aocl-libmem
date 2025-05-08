/*
 * Copyright © 2011 Siarhei Siamashka <siarhei.siamashka@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>

#include <unistd.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>


#include "util.h"
#include "version.h"

#define BLOCKSIZE               2048
#ifndef MAXREPEATS
# define MAXREPEATS             10
#endif
# define NULL_BYTE              1
#define MIN_PRINTABLE_ASCII     32
#define MAX_PRINTABLE_ASCII     127
#define NULL_TERM_CHAR          '\0'

#ifndef __ASM_OPT_H__
#define __ASM_OPT_H__

typedef struct
{
    const char *description;
    int use_tmpbuf;
    int (*f)(int64_t *, int64_t *, int);
} bench_info;

bench_info *get_asm_benchmarks(void);
bench_info *get_asm_framebuffer_benchmarks(void);

#endif

static void *mmap_framebuffer(size_t *fbsize)
{
    int fd;
    void *p;
    struct fb_fix_screeninfo finfo;

    if ((fd = open("/dev/fb0", O_RDWR)) == -1)
        if ((fd = open("/dev/graphics/fb0", O_RDWR)) == -1)
            return NULL;

    if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo)) {
        close(fd);
        return NULL;
    }

    p = mmap(0, finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    if (p == (void *)-1)
        return NULL;

    *fbsize = finfo.smem_len;
    return p;
}


static double bandwidth_bench_helper(int64_t *dstbuf, int64_t *srcbuf,
                                     int64_t *tmpbuf,
                                     int size, int blocksize,
                                     const char *indent_prefix,
                                     int use_tmpbuf,
                                     int (*f)(int64_t *, int64_t *, int),
                                     const char *description)
{
    int i, j, loopcount, innerloopcount, n;
    double t1, t2;
    double speed, maxspeed;
    double s, s0, s1, s2;

    /* do up to MAXREPEATS measurements */
    s = s0 = s1 = s2 = 0;
    maxspeed   = 0;
    for (n = 0; n < MAXREPEATS; n++)
    {
        loopcount = 0;
        innerloopcount = 1;
        t1 = gettime();
        do
        {
            loopcount += innerloopcount;
            if (use_tmpbuf)
            {
                for (i = 0; i < innerloopcount; i++)
                {
                    for (j = 0; j < size; j += blocksize)
                        {
                        f(tmpbuf, srcbuf + j / sizeof(int64_t), blocksize);
                        f(dstbuf + j / sizeof(int64_t), tmpbuf, blocksize);
                    }
                }
            }
            else
            {
               if(strstr(description,"cat") == NULL)
                {   for (i = 0; i < innerloopcount; i++)
                    {
                        (void)f(dstbuf, srcbuf, size);
                    }
                }
                else
                {
                    for (i = 0; i < innerloopcount; i++)
                    {
                        *(dstbuf + size - 1) = '\0';
                        f(dstbuf, srcbuf, size);
                    }

                }
            }
            innerloopcount *= 2;
            t2 = gettime();
        } while (t2 - t1 < 0.5);

        speed = (double)size * loopcount / (t2 - t1) / 1000000.;
        s0 += 1;
        s1 += speed;
        s2 += speed * speed;

        if (speed > maxspeed)
            maxspeed = speed;

        if (s0 > 2)
        {
            s = sqrt((s0 * s2 - s1 * s1) / (s0 * (s0 - 1)));
            if (s < maxspeed / 1000.)
                break;
        }
    }

    if (maxspeed > 0 && s / maxspeed * 100. >= 0.1)
    {
        printf("%s%-52s : %8.1f MB/s (%.1f%%)\n", indent_prefix, description,
                                               maxspeed, s / maxspeed * 100.);
    }
    else
    {
        printf("%s%-52s : %8.1f MB/s\n", indent_prefix, description, maxspeed);
    }
    return maxspeed;
}

int memcpy_wrapper(int64_t *dst, int64_t *src, int size)
{
    memcpy(dst, src, size);
}

int memset_wrapper(int64_t *dst, int64_t *src, int size)
{
    memset(dst, src[0], size);
}

int memmove_wrapper(int64_t *dst, int64_t *src, int size)
{
    memmove(dst, src, size);
}

int memcmp_wrapper(int64_t *dst, int64_t *src, int size)
{
    return memcmp(dst, src, size);
}

int strcpy_wrapper(int64_t *dst, int64_t *src, int size)
{
    strcpy((char *)dst, (char *) src);
}

int strncpy_wrapper(int64_t *dst, int64_t *src, int size)
{
    strncpy((char *)dst, (char *) src, size);
}

int strcmp_wrapper(int64_t *dst, int64_t *src, int size)
{
    return strcmp((char *)dst, (char *) src);
}

int strncmp_wrapper(int64_t *dst, int64_t *src, int size)
{
    return strncmp((char *)dst, (char *) src, size);
}

int strlen_wrapper(int64_t *dst, int64_t *src, int size)
{
    return strlen((char *)src);
}

int strcat_wrapper(int64_t *dst, int64_t *src, int size)
{
    strcat((char *)dst, (char *)src);
}

int strncat_wrapper(int64_t *dst, int64_t *src, int size)
{
    strncat((char *)dst, (char *)src, size);
}

int strspn_wrapper(int64_t *dst, int64_t *src, int size)
{
    strspn((char *)dst, (char *)src);
}

int strstr_wrapper(int64_t *dst, int64_t *src, int size)
{
    return strstr((char *)dst, (char *)src);
}

int memchr_wrapper(int64_t *dst, int64_t *src, int size)
{
    return memchr(src, 'x' , size);
}

int strchr_wrapper(int64_t *dst, int64_t *src, int size)
{
    return strchr((char *)src,'!');
}



void bandwidth_bench(int64_t *dstbuf, int64_t *srcbuf, int64_t *tmpbuf,
                     int size, int blocksize, const char *indent_prefix,
                     bench_info *bi)
{
    while (bi->f)
    {
        bandwidth_bench_helper(dstbuf, srcbuf, tmpbuf, size, blocksize,
                               indent_prefix, bi->use_tmpbuf,
                               bi->f,
                               bi->description);
        bi++;
    }
}

bench_info supp_funcs[]=
{
    {"memcpy", 0, memcpy_wrapper},
    {"memmove", 0, memmove_wrapper},
    {"memset", 0, memset_wrapper},
    {"memcmp", 0, memcmp_wrapper},
    {"memchr", 0, memchr_wrapper},
    {"strcpy", 0, strcpy_wrapper},
    {"strncpy", 0, strncpy_wrapper},
    {"strcmp", 0, strcmp_wrapper},
    {"strncmp", 0, strncmp_wrapper},
    {"strlen", 0, strlen_wrapper},
    {"strcat", 0, strcat_wrapper},
    {"strncat", 0, strncat_wrapper},
    {"strspn", 0, strspn_wrapper},
    {"strstr", 0, strstr_wrapper},
    {"strchr", 0, strchr_wrapper},
    {"none", 0,  NULL}
};

void generate_uniq_random_string(char * str, size_t length) {
    size_t i;
    for (i = 0; i < length ; i++) {
        str[i] = MIN_PRINTABLE_ASCII + (i % (MAX_PRINTABLE_ASCII - MIN_PRINTABLE_ASCII)) ; // printable ASCII chars from 32 to 126
    }
    str[i] = NULL_TERM_CHAR;
}

int main(int argc, char **argv)
{
    srand(0);
    int64_t *srcbuf, *dstbuf, *tmpbuf;
    void *poolbuf;
    size_t start, end, bufsize = 0;
    unsigned int iter = 0;
    int idx;
    size_t fbsize = 0;
    int64_t *fbbuf = mmap_framebuffer(&fbsize);
    fbsize = (fbsize / BLOCKSIZE) * BLOCKSIZE;
    static bench_info bench_func[] = {{NULL, 0,  NULL}, {NULL, 0,  NULL}};

    start = atoi(argv[2]);
    end = atoi(argv[3]);

    //iterator choice - 0 stands for shift left by 1
    iter = atoi(argv[4]);

    for (idx = 0; idx <= sizeof(supp_funcs)/sizeof(supp_funcs[0]); idx++)
    {
        if (!strcmp(supp_funcs[idx].description, argv[1]))
        {
           break;
        }
    }

    bench_func[0].description = supp_funcs[idx].description;
    bench_func[0].use_tmpbuf = supp_funcs[idx].use_tmpbuf;
    bench_func[0].f = supp_funcs[idx].f;

    for (bufsize = start; bufsize<= end; bufsize = (bufsize + iter) * (iter != 0) + (bufsize << 1) * (iter == 0))
    {
        //Bigger dst buffer for strcat and strncat operation
        if (strstr(bench_func[0].description,"cat"))
        {
            poolbuf = alloc_four_nonaliased_buffers((void **)&srcbuf, bufsize,
                                                    (void **)&dstbuf, MAXREPEATS * bufsize,
                                                    (void **)&tmpbuf, BLOCKSIZE,
                                                    NULL, 0);
        }
        else
        {
            poolbuf = alloc_four_nonaliased_buffers((void **)&srcbuf, bufsize,
                                                    (void **)&dstbuf, bufsize,
                                                    (void **)&tmpbuf, BLOCKSIZE,
                                                    NULL, 0);
        }

        printf("SIZE: %zu B \n",bufsize);
        memset(srcbuf, 'c', bufsize);
        //For handling string functions
        if (strstr(bench_func[0].description, "str") || strstr(bench_func[0].description, "memchr"))
        {
            if (strstr(bench_func[0].description, "strspn"))
            {
                size_t accept_len = ceil(sqrt(bufsize));

                generate_uniq_random_string((char*)srcbuf , accept_len);
                size_t i ;
                for (i = 0 ; i < (bufsize - NULL_BYTE); i++)
                {
                    *((char *)dstbuf + i) = *((char *)srcbuf + rand() % (accept_len));
                }
                *((char *)dstbuf + i) = NULL_TERM_CHAR;
            }
            else
            {
                *((char *)srcbuf + bufsize - NULL_BYTE) = NULL_TERM_CHAR;
                if (strstr(bench_func[0].description, "cmp") || strstr(bench_func[0].description, "cat") || strstr(bench_func[0].description, "strstr"))
                {
                    memcpy(dstbuf, srcbuf, bufsize);
                    *((char *)srcbuf + bufsize - 2) = NULL_TERM_CHAR;
                    *((char *)dstbuf + bufsize - NULL_BYTE) = NULL_TERM_CHAR;
                }
            }
        }
        bandwidth_bench(dstbuf, srcbuf, tmpbuf, bufsize, BLOCKSIZE, " ", bench_func);

        free(poolbuf);
    }
    return 0;
}
