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
#include "oapv_port.h"

void *oapv_malloc_align32(int size)
{
    void *p = NULL;

    // p variable is covered under ap. It's user rosponsibility to free it using funcion oapv_mfree_align32.
    p = oapv_malloc(size + 32 + sizeof(void *));
    if(p) {

#ifdef _IS64BIT // for 64bit CPU
        void **ap = (void **)(((u64)(p) + 32 + sizeof(void *) - 1) & (~0x1F));
#else // for 32bit CPU
        void **ap = (void **)(((u32)(p) + 32 + sizeof(void *) - 1) & (~0x1F));
#endif
        ap[-1] = (void *)p;
        return (void *)ap;
    }
    return NULL;
}

void oapv_mfree_align32(void *p)
{
    if(p) {
        oapv_mfree(((void **)p)[-1]);
    }
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
