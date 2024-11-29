/* Copyright (C) 2022-24 Advanced Micro Devices, Inc. All rights reserved.
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
#ifdef ALMEM_DYN_DISPATCH

#include <stddef.h>
#include "libmem_iface.h"

__attribute__((visibility("default")))void * (*_memcpy_variant)(void *, const void *, size_t);
// memcpy mapping
LIBMEM_FN_MAP(memcpy);
WEAK_ALIAS(memcpy, MK_FN_NAME(memcpy));

__attribute__((visibility("default")))void * (*_mempcpy_variant)(void *, const void *, size_t);
// mempcpy mapping
LIBMEM_FN_MAP(mempcpy);
WEAK_ALIAS(mempcpy, MK_FN_NAME(mempcpy));

__attribute__((visibility("default")))void * (*_memmove_variant)(void *, const void *, size_t);
// memmove mapping
LIBMEM_FN_MAP(memmove);
WEAK_ALIAS(memmove, MK_FN_NAME(memmove));

__attribute__((visibility("default")))void * (*_memset_variant)(void *, int , size_t);
// memset mapping
LIBMEM_FN_MAP(memset);
WEAK_ALIAS(memset, MK_FN_NAME(memset));

__attribute__((visibility("default")))int (*_memcmp_variant)(const void *, const void * , size_t);
// memcmp mapping
LIBMEM_FN_MAP(memcmp);
WEAK_ALIAS(memcmp, MK_FN_NAME(memcmp));

__attribute__((visibility("default")))void * (*_memchr_variant)(void *, int , size_t);
// memchr mapping
LIBMEM_FN_MAP(memchr);
WEAK_ALIAS(memchr, MK_FN_NAME(memchr));

__attribute__((visibility("default")))char * (*_strcpy_variant)(char *, const char *);
// strcpy mapping
LIBMEM_FN_MAP(strcpy);
WEAK_ALIAS(strcpy, MK_FN_NAME(strcpy));

__attribute__((visibility("default")))char * (*_strncpy_variant)(char *, const char *, size_t);
// strncpy mapping
LIBMEM_FN_MAP(strncpy);
WEAK_ALIAS(strncpy, MK_FN_NAME(strncpy));

__attribute__((visibility("default")))int (*_strcmp_variant)(const char *, const char *);
// strcmp mapping
LIBMEM_FN_MAP(strcmp);
WEAK_ALIAS(strcmp, MK_FN_NAME(strcmp));

__attribute__((visibility("default")))int (*_strncmp_variant)(const char *, const char *, size_t);
// strncmp mapping
LIBMEM_FN_MAP(strncmp);
WEAK_ALIAS(strncmp, MK_FN_NAME(strncmp));

__attribute__((visibility("default")))char * (*_strcat_variant)(char *, const char *);
// strcat mapping
LIBMEM_FN_MAP(strcat);
WEAK_ALIAS(strcat, MK_FN_NAME(strcat));

__attribute__((visibility("default")))char * (*_strstr_variant)(const char *, const char *);
// strstr mapping
LIBMEM_FN_MAP(strstr);
WEAK_ALIAS(strstr, MK_FN_NAME(strstr));

#endif
