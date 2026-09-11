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

#ifndef __OAPV_TPOOL_H__
#define __OAPV_TPOOL_H__

typedef void *oapv_thread_t;
typedef int (*oapv_fn_thread_entry_t)(void *arg);
typedef struct oapv_tpool oapv_tpool_t;
typedef void *oapv_sync_obj_t;

//  Salient points  ****************************************************
//  Thread Controller object will create, run and destroy***************
//  threads. Thread Controller has to be initialised *******************
//  before invoking handler functions.  Thread controller***************
//  should be de-initialized to release handler functions***************
//

typedef enum {
    TPOOL_SUCCESS = 0,
    TPOOL_OUT_OF_MEMORY,
    TPOOL_INVALID_ARG,
    TPOOL_INVALID_STATE,
    TPOOL_UNKNOWN_ERROR
} tpool_result_t;

typedef enum {
    TPOOL_SUSPENDED = 0,
    TPOOL_RUNNING,
    TPOOL_TERMINATED
} tpool_status_t;

struct oapv_tpool {
    // Handler function to create requested thread, thread created is in suspended state
    oapv_thread_t (*create)(oapv_tpool_t *tp, int thread_id);
    // Handler function to wake up suspended thread and assign task to complete
    tpool_result_t (*run)(oapv_thread_t thread_id, oapv_fn_thread_entry_t entry, void *arg);
    // Handler function to get result from the task assigned to the thread in consideration
    tpool_result_t (*join)(oapv_thread_t thread_id, int *res);
    // Handler function to terminate a thread in consideration
    tpool_result_t (*release)(oapv_thread_t *thread_id);
    // handle for mask number of allowed thread
    int max_task_cnt;
    // memory allocator of the owning codec instance
    oapv_ops_mem_t ops_mem;
};

tpool_result_t oapv_tpool_init(oapv_tpool_t *tp, const oapv_ops_mem_t *ops, int maxtask);
tpool_result_t oapv_tpool_deinit(oapv_tpool_t *tp);

oapv_sync_obj_t oapv_tpool_sync_obj_create(const oapv_ops_mem_t *ops);
tpool_result_t oapv_tpool_sync_obj_delete(oapv_sync_obj_t *sobj);
int oapv_tpool_spinlock_wait(volatile int *addr, int val);

void oapv_tpool_enter_cs(oapv_sync_obj_t sobj);
void oapv_tpool_leave_cs(oapv_sync_obj_t sobj);

/* Atomically increments *pcnt under 'sobj' and returns the incremented value.
   Used to hand out work items to competing worker threads. */
int oapv_tpool_atomic_inc(oapv_sync_obj_t sobj, volatile int *pcnt);

#endif // __OAPV_TPOOL_H__
