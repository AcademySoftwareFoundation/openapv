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

#include <stdarg.h>
#include "oapv_def.h"

static void *oapv_ops_mem_malloc_libc(void *udata, unsigned int size)
{
    (void)udata;
    return malloc(size);
}

static void *oapv_ops_mem_calloc_libc(void *udata, unsigned int count, unsigned int size)
{
    (void)udata;
    return calloc(count, size);
}

static void *oapv_ops_mem_realloc_libc(void *udata, void *ptr, unsigned int size)
{
    (void)udata;
    return realloc(ptr, size);
}

static void oapv_ops_mem_free_libc(void *udata, void *ptr)
{
    (void)udata;
    free(ptr);
}

void oapv_ops_mem_default(oapv_ops_mem_t *dst)
{
    dst->magic   = OAPV_OPS_MAGIC_CODE_MEM;
    dst->malloc  = oapv_ops_mem_malloc_libc;
    dst->calloc  = oapv_ops_mem_calloc_libc;
    dst->realloc = oapv_ops_mem_realloc_libc;
    dst->free    = oapv_ops_mem_free_libc;
    dst->udata   = NULL;
}

void oapv_trace0(char *filename, int line, const char *fmt, ...)
{
    char str[1024] = { '\0' };

    if(filename != NULL && line >= 0) {
        snprintf(str, sizeof(str), "[%s:%d] ", filename, line);
    }
    va_list args;
    va_start(args, fmt);
    vsnprintf(str + strlen(str), sizeof(str) - strlen(str), fmt, args);
    va_end(args);
    printf("%s", str);
}

void oapv_trace_line(char *pre)
{
    const int chars = 80;
    char      str[128] = { '\0' };
    int       len = (pre == NULL) ? 0 : (int)strlen(pre);

    if(len > 0) {
        snprintf(str, sizeof(str), "%s ", pre);
        len = (int)strlen(str);
    }
    for(int i = len; i < chars; i++) {
        str[i] = '=';
    }
    str[chars] = '\0';
    printf("%s\n", str);
}

#if defined(WIN32) || defined(WIN64) || defined(_WIN32)
#include <windows.h>
#include <sysinfoapi.h>
#else /* LINUX, MACOS, Android */
#include <unistd.h>
#endif

int oapv_get_num_cpu_cores(void)
{
    int num_cores = 1; // default
#if defined(WIN32) || defined(WIN64) || defined(_WIN32)
    {
        SYSTEM_INFO si;
        GetNativeSystemInfo(&si);
        num_cores = si.dwNumberOfProcessors;
    }
#elif defined(_SC_NPROCESSORS_ONLN)
    {
        num_cores = (int)sysconf(_SC_NPROCESSORS_ONLN);
    }
#elif defined(CPU_COUNT)
    {
        cpu_set_t cset;
        memset(&cset, 0, sizeof(cset));
        if(!sched_getaffinity(0, sizeof(cset), &cset)) {
            num_cores = CPU_COUNT(&cset);
        }
    }
#endif
    return num_cores;
}
