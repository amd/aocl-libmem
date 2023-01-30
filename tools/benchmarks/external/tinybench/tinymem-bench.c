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
#include "asm-opt.h"
#include "version.h"

#define BLOCKSIZE        2048
#ifndef MAXREPEATS
# define MAXREPEATS      10
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
                                     void (*f)(int64_t *, int64_t *, int),
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
                for (i = 0; i < innerloopcount; i++)
                {
                    f(dstbuf, srcbuf, size);
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

void memcpy_wrapper(int64_t *dst, int64_t *src, int size)
{
    memcpy(dst, src, size);
}

void memset_wrapper(int64_t *dst, int64_t *src, int size)
{
    memset(dst, src[0], size);
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


int main(int argc, char **argv)
{
    int64_t *srcbuf, *dstbuf, *tmpbuf;
    void *poolbuf;
    size_t start, end;

   start = atoi(argv[2]);
   end = atoi(argv[3]);

    size_t fbsize = 0;
    int64_t *fbbuf = mmap_framebuffer(&fbsize);
    fbsize = (fbsize / BLOCKSIZE) * BLOCKSIZE;


   for(size_t bufsize = start; bufsize<= end; bufsize<<=1)
{

    poolbuf = alloc_four_nonaliased_buffers((void **)&srcbuf, bufsize,
                                            (void **)&dstbuf, bufsize,
                                            (void **)&tmpbuf, BLOCKSIZE,
                                            NULL, 0);

    printf("SIZE: %zu B \n",bufsize);

    if(!strcmp(argv[1],"memcpy"))
    {
        static bench_info mem_copy[]={{ "standard memcpy", 0, memcpy_wrapper },{ NULL, 0, NULL }};
        bandwidth_bench(dstbuf, srcbuf, tmpbuf, bufsize, BLOCKSIZE, " ",mem_copy);

    }

    else if(!strcmp(argv[1],"memset"))
    {
        static bench_info mem_set[]={{ "standard memset", 0, memset_wrapper },{ NULL, 0, NULL }};
        bandwidth_bench(dstbuf, srcbuf, tmpbuf, bufsize, BLOCKSIZE, " ", mem_set);

    }



}
    return 0;
}

