/* Copyright (C) 2023-25 Advanced Micro Devices, Inc. All rights reserved.
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

#ifndef _LIBMEM_LOAD_STORE_IMPLS_H_
#define _LIBMEM_LOAD_STORE_IMPLS_H_

#include <immintrin.h>
#include "alm_defs.h"

#define CACHE_LINE_SZ 64

//Generic definitions
#define VEC_DECL(vec)           VEC_DECL_##vec

#define ALM_LOAD_INSTR(vec, type)   vec##_LOAD_##type

#define ALM_STORE_INSTR(vec, type)  vec##_STORE_##type

#define VEC_SZ(vec)             VEC_SZ_##vec

#define PFTCH_ZERO_CL

#define PFTCH_ONE_CL       \
  _mm_prefetch(load_addr + offset +  CACHE_LINE_SZ, _MM_HINT_NTA);


#define PFTCH_TWO_CL_ONE_STEP       \
  _mm_prefetch(load_addr + offset + 0 * CACHE_LINE_SZ, _MM_HINT_NTA);   \
  _mm_prefetch(load_addr + offset + 1 * CACHE_LINE_SZ, _MM_HINT_NTA);


#define PFTCH_TWO_CL_TWO_STEP       \
  _mm_prefetch(load_addr + offset + 1 * CACHE_LINE_SZ, _MM_HINT_NTA); \
  _mm_prefetch(load_addr + offset + 3 * CACHE_LINE_SZ, _MM_HINT_NTA);

#define PFTCH_FOUR_CL_ONE_STEP      \
  _mm_prefetch(load_addr + offset + 0 * CACHE_LINE_SZ, _MM_HINT_NTA); \
  _mm_prefetch(load_addr + offset + 1 * CACHE_LINE_SZ, _MM_HINT_NTA); \
  _mm_prefetch(load_addr + offset + 2 * CACHE_LINE_SZ, _MM_HINT_NTA); \
  _mm_prefetch(load_addr + offset + 3 * CACHE_LINE_SZ, _MM_HINT_NTA);

#define PFTCH_FOUR_CL_TWO_STEP      \
  _mm_prefetch(load_addr + offset + 1 * CACHE_LINE_SZ, _MM_HINT_NTA); \
  _mm_prefetch(load_addr + offset + 3 * CACHE_LINE_SZ, _MM_HINT_NTA);

#define PFTCH_EIGHT_CL_ONE_STEP      \
  _mm_prefetch(load_addr + offset + 0 * CACHE_LINE_SZ, _MM_HINT_NTA); \
  _mm_prefetch(load_addr + offset + 1 * CACHE_LINE_SZ, _MM_HINT_NTA); \
  _mm_prefetch(load_addr + offset + 2 * CACHE_LINE_SZ, _MM_HINT_NTA); \
  _mm_prefetch(load_addr + offset + 3 * CACHE_LINE_SZ, _MM_HINT_NTA); \
  _mm_prefetch(load_addr + offset + 4 * CACHE_LINE_SZ, _MM_HINT_NTA); \
  _mm_prefetch(load_addr + offset + 5 * CACHE_LINE_SZ, _MM_HINT_NTA); \
  _mm_prefetch(load_addr + offset + 6 * CACHE_LINE_SZ, _MM_HINT_NTA); \
  _mm_prefetch(load_addr + offset + 7 * CACHE_LINE_SZ, _MM_HINT_NTA);

#define PFTCH_EIGHT_CL_TWO_STEP      \
  _mm_prefetch(load_addr + offset + 1 * CACHE_LINE_SZ, _MM_HINT_NTA); \
  _mm_prefetch(load_addr + offset + 3 * CACHE_LINE_SZ, _MM_HINT_NTA); \
  _mm_prefetch(load_addr + offset + 5 * CACHE_LINE_SZ, _MM_HINT_NTA); \
  _mm_prefetch(load_addr + offset + 7 * CACHE_LINE_SZ, _MM_HINT_NTA);


#define VEC_1X_LOAD_STORE(vec, load_type, store_type)                    \
  VEC_DECL(vec) vec(0) ;                                                 \
  vec(0) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset);  \
  ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset, vec(0));


#define VEC_2X_LOAD_STORE(vec, load_type, store_type)                                    \
  VEC_DECL(vec) vec(0), vec(1) ;                                                         \
    vec(0) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset);                \
    vec(1) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset + VEC_SZ(vec));  \
    ALM_MEM_BARRIER();                                                                   \
    ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset, vec(0));              \
    ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset + VEC_SZ(vec), vec(1));

#define VEC_3X_LOAD_STORE(vec, load_type, store_type)                                         \
  VEC_DECL(vec) vec(0), vec(1), vec(2);                                                       \
    vec(0) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset);                     \
    vec(1) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset + VEC_SZ(vec));       \
    vec(2) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset + 2 * VEC_SZ(vec));   \
    ALM_MEM_BARRIER();                                                                        \
    ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset , vec(0));                  \
    ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset + VEC_SZ(vec), vec(1));     \
    ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset + 2 * VEC_SZ(vec), vec(2));

#define VEC_4X_LOAD_STORE(vec, load_type, store_type)                                         \
  VEC_DECL(vec) vec(0), vec(1), vec(2), vec(3);                                               \
    vec(0) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset);                     \
    vec(1) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset + VEC_SZ(vec));       \
    vec(2) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset + 2 * VEC_SZ(vec));   \
    vec(3) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset + 3 * VEC_SZ(vec));   \
    ALM_MEM_BARRIER();                                                                        \
    ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset , vec(0));                  \
    ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset + VEC_SZ(vec), vec(1));     \
    ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset + 2 * VEC_SZ(vec), vec(2)); \
    ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset + 3 * VEC_SZ(vec), vec(3));


#define VEC_8X_LOAD_STORE(vec, load_type, store_type)                          \
  VEC_DECL(vec) vec(0), vec(1), vec(2), vec(3);                                               \
  VEC_DECL(vec) vec(4), vec(5), vec(6), vec(7);                                               \
    vec(0) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset);                     \
    vec(1) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset + VEC_SZ(vec));       \
    vec(2) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset + 2 * VEC_SZ(vec));   \
    vec(3) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset + 3 * VEC_SZ(vec));   \
    vec(4) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset + 4 * VEC_SZ(vec));   \
    vec(5) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset + 5 * VEC_SZ(vec));   \
    vec(6) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset + 6 * VEC_SZ(vec));   \
    vec(7) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset + 7 * VEC_SZ(vec));   \
    ALM_MEM_BARRIER();                                                                        \
    ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset, vec(0));                   \
    ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset +  VEC_SZ(vec), vec(1));    \
    ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset + 2 * VEC_SZ(vec), vec(2)); \
    ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset + 3 * VEC_SZ(vec), vec(3)); \
    ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset + 4 * VEC_SZ(vec), vec(4)); \
    ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset + 5 * VEC_SZ(vec), vec(5)); \
    ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset + 6 * VEC_SZ(vec), vec(6)); \
    ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset + 7 * VEC_SZ(vec), vec(7));


#define VEC_2X_LOAD_STORE_LOOP(vec, pftch_cl,load_type, store_type)                      \
  VEC_DECL(vec) vec(0), vec(1) ;                                                         \
  while (offset < size) {                                                                \
    pftch_cl                                                                             \
    vec(0) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset);                \
    vec(1) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset + VEC_SZ(vec));  \
    ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset, vec(0));              \
    ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset + VEC_SZ(vec), vec(1));\
    offset += 2 * VEC_SZ(vec);                                                           \
  }                                                                                      \
  return offset;

#define VEC_2X_LOAD_STORE_LOOP_BKWD(vec, pftch_cl, load_type, store_type)           \
  VEC_DECL(vec) vec(0), vec(1);                                                     \
  while (offset < size) {                                                           \
    pftch_cl                                                                        \
    vec(0) = ALM_LOAD_INSTR(vec, load_type) (load_addr + size - 1 * VEC_SZ(vec));   \
    vec(1) = ALM_LOAD_INSTR(vec, load_type) (load_addr + size - 2 * VEC_SZ(vec));   \
    ALM_STORE_INSTR(vec, store_type) (store_addr + size - 1 * VEC_SZ(vec), vec(0)); \
    ALM_STORE_INSTR(vec, store_type) (store_addr + size - 2 * VEC_SZ(vec), vec(1)); \
    size -= 2 *  VEC_SZ(vec);                                                       \
  }                                                                                 \
  return offset;



#define VEC_4X_LOAD_STORE_LOOP(vec, pftch_cl, load_type, store_type)                          \
  VEC_DECL(vec) vec(0), vec(1), vec(2), vec(3);                                               \
  while (offset < size) {                                                                     \
    pftch_cl                                                                                  \
    vec(0) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset);                     \
    vec(1) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset + VEC_SZ(vec));       \
    vec(2) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset + 2 * VEC_SZ(vec));   \
    vec(3) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset + 3 * VEC_SZ(vec));   \
    ALM_MEM_BARRIER();                                                                        \
    ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset , vec(0));                  \
    ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset + VEC_SZ(vec), vec(1));     \
    ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset + 2 * VEC_SZ(vec), vec(2)); \
    ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset + 3 * VEC_SZ(vec), vec(3)); \
    offset += 4 *  VEC_SZ(vec);                                                               \
  }                                                                                           \
  return offset;

#define VEC_4X_LOAD_STORE_LOOP_BKWD(vec, pftch_cl, load_type, store_type)           \
  VEC_DECL(vec) vec(0), vec(1), vec(2), vec(3);                                     \
  while (offset < size) {                                                           \
    pftch_cl                                                                        \
    vec(0) = ALM_LOAD_INSTR(vec, load_type) (load_addr + size - 1 * VEC_SZ(vec));   \
    vec(1) = ALM_LOAD_INSTR(vec, load_type) (load_addr + size - 2 * VEC_SZ(vec));   \
    vec(2) = ALM_LOAD_INSTR(vec, load_type) (load_addr + size - 3 * VEC_SZ(vec));   \
    vec(3) = ALM_LOAD_INSTR(vec, load_type) (load_addr + size - 4 * VEC_SZ(vec));   \
    ALM_MEM_BARRIER();                                                              \
    ALM_STORE_INSTR(vec, store_type) (store_addr + size - 1 * VEC_SZ(vec), vec(0)); \
    ALM_STORE_INSTR(vec, store_type) (store_addr + size - 2 * VEC_SZ(vec), vec(1)); \
    ALM_STORE_INSTR(vec, store_type) (store_addr + size - 3 * VEC_SZ(vec), vec(2)); \
    ALM_STORE_INSTR(vec, store_type) (store_addr + size - 4 * VEC_SZ(vec), vec(3)); \
    size -= 4 *  VEC_SZ(vec);                                                       \
  }                                                                                 \
  return offset;


#define VEC_8X_LOAD_STORE_LOOP(vec, pftch_cl, load_type, store_type)                          \
  VEC_DECL(vec) vec(0), vec(1), vec(2), vec(3);                                               \
  VEC_DECL(vec) vec(4), vec(5), vec(6), vec(7);                                               \
  while (offset < size) {                                                                     \
    pftch_cl                                                                                  \
    vec(0) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset);                     \
    vec(1) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset + VEC_SZ(vec));       \
    vec(2) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset + 2 * VEC_SZ(vec));   \
    vec(3) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset + 3 * VEC_SZ(vec));   \
    vec(4) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset + 4 * VEC_SZ(vec));   \
    vec(5) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset + 5 * VEC_SZ(vec));   \
    vec(6) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset + 6 * VEC_SZ(vec));   \
    vec(7) = ALM_LOAD_INSTR(vec, load_type) ((void *)load_addr + offset + 7 * VEC_SZ(vec));   \
    ALM_MEM_BARRIER();                                                                        \
    ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset, vec(0));                   \
    ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset +  VEC_SZ(vec), vec(1));    \
    ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset + 2 * VEC_SZ(vec), vec(2)); \
    ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset + 3 * VEC_SZ(vec), vec(3)); \
    ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset + 4 * VEC_SZ(vec), vec(4)); \
    ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset + 5 * VEC_SZ(vec), vec(5)); \
    ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset + 6 * VEC_SZ(vec), vec(6)); \
    ALM_STORE_INSTR(vec, store_type) ((void *)store_addr + offset + 7 * VEC_SZ(vec), vec(7)); \
    offset += 8 * VEC_SZ(vec);                                                                \
  }                                                                                           \
  return offset;


#define VEC_2X_LOAD_STORE_HEAD_TAIL(vec, load_type, store_type)                \
  VEC_DECL(vec)  vec(0) , vec(1) ;                                             \
  vec(0) = ALM_LOAD_INSTR(vec, load_type) (load_addr);                         \
  vec(1) = ALM_LOAD_INSTR(vec, load_type) (load_addr + size - VEC_SZ(vec));    \
  ALM_MEM_BARRIER();                                                           \
  ALM_STORE_INSTR(vec, store_type) (store_addr, vec(0));                       \
  ALM_STORE_INSTR(vec, store_type) (store_addr + size - VEC_SZ(vec), vec(1));


#define VEC_4X_LOAD_STORE_HEAD_TAIL(vec, load_type, store_type)                  \
  VEC_DECL(vec) vec(0), vec(1), vec(2), vec(3);                                  \
  vec(0) = ALM_LOAD_INSTR(vec, load_type) (load_addr);                           \
  vec(1) = ALM_LOAD_INSTR(vec, load_type) (load_addr + VEC_SZ(vec));             \
  vec(2) = ALM_LOAD_INSTR(vec, load_type) (load_addr + size - 2 * VEC_SZ(vec));  \
  vec(3) = ALM_LOAD_INSTR(vec, load_type) (load_addr + size - VEC_SZ(vec));      \
  ALM_MEM_BARRIER();                                                             \
  ALM_STORE_INSTR(vec, store_type) (store_addr , vec(0));                        \
  ALM_STORE_INSTR(vec, store_type) (store_addr + VEC_SZ(vec), vec(1));           \
  ALM_STORE_INSTR(vec, store_type) (store_addr + size - 2 * VEC_SZ(vec), vec(2));\
  ALM_STORE_INSTR(vec, store_type) (store_addr + size - VEC_SZ(vec), vec(3));


#define VEC_8X_LOAD_STORE_HEAD_TAIL(vec, load_type, store_type)                  \
  VEC_DECL(vec) vec(0), vec(1), vec(2), vec(3);                                  \
  VEC_DECL(vec) vec(4), vec(5), vec(6), vec(7);                                  \
  vec(0) = ALM_LOAD_INSTR(vec, load_type) (load_addr);                           \
  vec(1) = ALM_LOAD_INSTR(vec, load_type) (load_addr + VEC_SZ(vec));             \
  vec(2) = ALM_LOAD_INSTR(vec, load_type) (load_addr + 2 * VEC_SZ(vec));         \
  vec(3) = ALM_LOAD_INSTR(vec, load_type) (load_addr + 3 * VEC_SZ(vec));         \
  vec(4) = ALM_LOAD_INSTR(vec, load_type) (load_addr + size - 4 * VEC_SZ(vec));  \
  vec(5) = ALM_LOAD_INSTR(vec, load_type) (load_addr + size - 3 * VEC_SZ(vec));  \
  vec(6) = ALM_LOAD_INSTR(vec, load_type) (load_addr + size - 2 * VEC_SZ(vec));  \
  vec(7) = ALM_LOAD_INSTR(vec, load_type) (load_addr + size - 1 * VEC_SZ(vec));  \
  ALM_MEM_BARRIER();                                                             \
  ALM_STORE_INSTR(vec, store_type) (store_addr , vec(0));                        \
  ALM_STORE_INSTR(vec, store_type) (store_addr + VEC_SZ(vec), vec(1));           \
  ALM_STORE_INSTR(vec, store_type) (store_addr + 2 * VEC_SZ(vec), vec(2));       \
  ALM_STORE_INSTR(vec, store_type) (store_addr + 3 * VEC_SZ(vec), vec(3));       \
  ALM_STORE_INSTR(vec, store_type) (store_addr + size - 4 * VEC_SZ(vec), vec(4));\
  ALM_STORE_INSTR(vec, store_type) (store_addr + size - 3 * VEC_SZ(vec), vec(5));\
  ALM_STORE_INSTR(vec, store_type) (store_addr + size - 2 * VEC_SZ(vec), vec(6));\
  ALM_STORE_INSTR(vec, store_type) (store_addr + size - 1 * VEC_SZ(vec), vec(7));


#include "load_store_sse2_impls.h"
#include "load_store_avx2_impls.h"
#include "load_store_avx512_impls.h"

#endif //HEADER
