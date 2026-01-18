/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 * All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * - Neither the name of the copyright owner, nor the names of its contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _OAPV_PORT_H_
#define _OAPV_PORT_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>

/*****************************************************************************
 * types
 *****************************************************************************/
typedef int8_t   s8;
typedef uint8_t  u8;
typedef int16_t  s16;
typedef uint16_t u16;
typedef int32_t  s32;
typedef uint32_t u32;
typedef int64_t  s64;
typedef uint64_t u64;

typedef s16      pel;
typedef s32      dpel;

/*****************************************************************************
 * inline attribute
 *****************************************************************************/
#ifndef force_inline
#if defined(_MSC_VER)
#define force_inline __forceinline
#elif defined(__GNUC__)
#define force_inline __attribute__((always_inline)) inline
#else
#define force_inline inline
#endif
#endif

/*****************************************************************************
 * trace and assert
 *****************************************************************************/
void oapv_trace0(char *filename, int line, const char *fmt, ...);
void oapv_trace_line(char *pre);
#ifndef OAPV_TRACE
#define OAPV_TRACE 0
#endif

/* trace function */
#if OAPV_TRACE
#if defined(__GNUC__)
#define __FILENAME__ \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define oapv_trace(args...) oapv_trace0(__FILENAME__, __LINE__, args)
#else
#define __FILENAME__ \
    (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#define oapv_trace(args, ...) oapv_trace0(__FILENAME__, __LINE__, args, __VA_ARGS__)
#endif
#else
#define oapv_trace(args, ...) \
    {}
#endif
#if defined(__GNUC__)
#define oapv_print(args, ...) oapv_trace0(NULL, -1, args)
#else
#define oapv_print(args, ...) oapv_trace0(NULL, -1, args, __VA_ARGS__)
#endif

/* assert function */
#include <assert.h>
#define oapv_assert(x) assert(x)
#define oapv_assert_r(x)     \
    {                        \
        if(!(x)) {           \
            oapv_assert(x);  \
            return;          \
        }                    \
    }
#define oapv_assert_rv(x, r) \
    {                        \
        if(!(x)) {           \
            oapv_assert(x);  \
            return (r);      \
        }                    \
    }
#define oapv_assert_g(x, g)  \
    {                        \
        if(!(x)) {           \
            oapv_assert(x);  \
            goto g;          \
        }                    \
    }
#define oapv_assert_gv(x, r, v, g) \
    {                              \
        if(!(x)) {                 \
            oapv_assert(x);        \
            (r) = (v);             \
            goto g;                \
        }                          \
    }

#if defined(__x86_64) || defined(__x86_64__) || defined(__amd64) || \
    defined(_M_X64) || defined(__amd64__) || defined(_M_AMD64) ||   \
    defined(__i386__)
#define X86_SSE 1
#elif defined(__aarch64__)
#define ARM_NEON 1
#endif

#if X86_SSE
#ifdef _WIN32
#include <emmintrin.h>
#include <xmmintrin.h>
#include <tmmintrin.h>
#include <smmintrin.h>
#else
#include <x86intrin.h>
#endif
#endif

#if ARM_NEON
#include <arm_neon.h>
#endif

/*****************************************************************************
 * memory operations
 *****************************************************************************/
#define oapv_malloc(size)         malloc((size))
#define oapv_mcalloc(count, size) calloc((count), (size))
#define oapv_mrealloc(ptr, size)  realloc((ptr), (size))

#define oapv_mfree(m) \
    {                 \
        if(m) {       \
            free(m);  \
        }             \
    }

/* Fill 'dst' with the default standard-C-library allocator interface. */
void oapv_ops_mem_default(oapv_ops_mem_t *dst);

/* Resolve caller-supplied 'src' into 'dst'. NULL or an all-NULL interface
   selects the default; a partially-filled one is rejected. */
int oapv_ops_mem_set(oapv_ops_mem_t *dst, const oapv_ops_mem_t *src);

/* allocate/free through the instance's resolved memory interface */
#define oapv_ops_malloc(ctx, size) \
    ((ctx)->ops_mem.malloc((ctx)->ops_mem.udata, (size)))
#define oapv_ops_free(ctx, ptr) \
    ((ctx)->ops_mem.free((ctx)->ops_mem.udata, (ptr)))

#define oapv_mcpy(dst, src, size)    memcpy((dst), (src), (size))
#define oapv_mset(dst, v, size)      memset((dst), (v), (size))
#define oapv_mset_x64a(dst, v, size) memset((dst), (v), (size))
#define oapv_mset_x128(dst, v, size) memset((dst), (v), (size))
#define oapv_mcmp(dst, src, size)    memcmp((dst), (src), (size))

static __inline void oapv_mset_16b(s16 *dst, s16 v, int cnt)
{
    int i;
    for(i = 0; i < cnt; i++)
        dst[i] = v;
}

/* Buffer Alignement */
#if defined(_WIN32) && !defined(__GNUC__)
#define DECLARE_ALIGNED(var, n) __declspec(align(n)) var
#else
#define DECLARE_ALIGNED(var, n) var __attribute__((aligned(n)))
#endif

#define ALIGNED_16(var)  DECLARE_ALIGNED(var, 16)
#define ALIGNED_32(var)  DECLARE_ALIGNED(var, 32)
#define ALIGNED_128(var) DECLARE_ALIGNED(var, 128)


/* CPU information */
int oapv_get_num_cpu_cores(void);

/* Endianness handling */
#if defined(__GNUC__) || defined(__clang__)
#define OAPV_SWAP16(x) __builtin_bswap16(x)
#define OAPV_SWAP32(x) __builtin_bswap32(x)
#define OAPV_SWAP64(x) __builtin_bswap64(x)
#elif defined(_MSC_VER)
#define OAPV_SWAP16(x) _byteswap_ushort(x)
#define OAPV_SWAP32(x) _byteswap_ulong(x)
#define OAPV_SWAP64(x) _byteswap_uint64(x)
#else
#define OAPV_SWAP16(x) ((((x) & 0xff00) >> 8) | (((x) & 0x00ff) << 8))
#define OAPV_SWAP32(x) ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >> 8) | \
                        (((x) & 0x0000ff00) << 8)  | (((x) & 0x000000ff) << 24))
#define OAPV_SWAP64(x) ((((x) & 0xff00000000000000ull) >> 56) | \
                        (((x) & 0x00ff000000000000ull) >> 40) | \
                        (((x) & 0x0000ff0000000000ull) >> 24) | \
                        (((x) & 0x000000ff00000000ull) >> 8)  | \
                        (((x) & 0x00000000ff000000ull) << 8)  | \
                        (((x) & 0x0000000000ff0000ull) << 24) | \
                        (((x) & 0x000000000000ff00ull) << 40) | \
                        (((x) & 0x00000000000000ffull) << 56))
#endif

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define OAPV_BIG_ENDIAN 1
#define OAPV_CPU_TO_LE32(x) OAPV_SWAP32(x)
#define OAPV_CPU_TO_LE64(x) OAPV_SWAP64(x)
#define OAPV_LE32_TO_CPU(x) OAPV_SWAP32(x)
#define OAPV_LE64_TO_CPU(x) OAPV_SWAP64(x)
#define OAPV_CPU_TO_BE32(x) (x)
#define OAPV_CPU_TO_BE64(x) (x)
#define OAPV_BE32_TO_CPU(x) (x)
#define OAPV_BE64_TO_CPU(x) (x)
#else
// Little Endian (Default for x86/ARM)
#define OAPV_LITTLE_ENDIAN 1
#define OAPV_CPU_TO_LE32(x) (x)
#define OAPV_CPU_TO_LE64(x) (x)
#define OAPV_LE32_TO_CPU(x) (x)
#define OAPV_LE64_TO_CPU(x) (x)
#define OAPV_CPU_TO_BE32(x) OAPV_SWAP32(x)
#define OAPV_CPU_TO_BE64(x) OAPV_SWAP64(x)
#define OAPV_BE32_TO_CPU(x) OAPV_SWAP32(x)
#define OAPV_BE64_TO_CPU(x) OAPV_SWAP64(x)
#endif

#endif /* _OAPV_PORT_H_ */

