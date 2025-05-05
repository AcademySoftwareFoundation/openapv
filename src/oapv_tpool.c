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

#include <stdio.h>
#include <stdlib.h>
#include "oapv_tpool.h"
#if defined(WIN32) || defined(WIN64)
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#endif

#define WINDOWS_MUTEX_SYNC 0

#if !defined(WIN32) && !defined(WIN64)

typedef struct thread_ctx {
    // synchronization members
    pthread_t              t_handle;   // worker thread handle
    pthread_attr_t         tAttribute; // worker thread attribute
    pthread_cond_t         w_event;    // wait event for worker thread
    pthread_cond_t         r_event;    // wait event for main thread
    pthread_mutex_t        c_section;  // for synchronization

    // member field to run  a task
    oapv_fn_thread_entry_t task;
    void                  *t_arg;
    tpool_status_t         t_status;
    tpool_result_t         t_result;
    int                    thread_id;
    int                    task_ret; // return value of task function
} thread_ctx_t;

typedef struct thread_mutex {
    pthread_mutex_t lmutex;
} thread_mutex_t;

static void *tpool_worker_thread(void *arg)
{
    /********************* main routine for thread pool worker thread *************************
    ********************** worker thread can remain in suspended or running state *************
    ********************* control the synchronization with help of thread context members *****/

    // member Initialization section
    thread_ctx_t *t_context = (thread_ctx_t *)arg;
    int           ret;

    if(!t_context) {
        return 0; // error handling, more like a fail safe mechanism
    }

    while(1) {
        // worker thread loop
        // remains suspended/sleep waiting for an event

        // get the mutex and check the state
        pthread_mutex_lock(&t_context->c_section);
        while(t_context->t_status == TPOOL_SUSPENDED) {
            // wait for the event
            pthread_cond_wait(&t_context->w_event, &t_context->c_section);
        }

        if(t_context->t_status == TPOOL_TERMINATED) {
            t_context->t_result = TPOOL_SUCCESS;
            pthread_mutex_unlock(&t_context->c_section);
            break; // exit the routine
        }

        t_context->t_status = TPOOL_RUNNING;
        pthread_mutex_unlock(&t_context->c_section);

        // run the routine
        // worker thread state is running with entry function and arg set
        ret = t_context->task(t_context->t_arg);

        // signal the thread waiting on the result
        pthread_mutex_lock(&t_context->c_section);
        t_context->t_status = TPOOL_SUSPENDED;
        t_context->t_result = TPOOL_SUCCESS;
        t_context->task_ret = ret;
        pthread_cond_signal(&t_context->r_event);
        pthread_mutex_unlock(&t_context->c_section);
    }

    return 0;
}

static oapv_thread_t tpool_create_thread(oapv_tpool_t *tp, int thread_id)
{
    if(!tp) {
        return NULL; // error management
    }

    thread_ctx_t *tctx = NULL;

    tctx = (thread_ctx_t *)malloc(sizeof(thread_ctx_t));

    if(!tctx) {
        return NULL; // error management, bad alloc
    }

    int result = 1;

    // intialize conditional variable and mutexes
    result = pthread_mutex_init(&tctx->c_section, NULL);
    if(result) {
        goto TERROR; // error handling
    }
    result = pthread_cond_init(&tctx->w_event, NULL);
    if(result) {
        goto TERROR;
    }
    result = pthread_cond_init(&tctx->r_event, NULL);
    if(result) {
        goto TERROR;
    }

    // initialize the worker thread attribute and set the type to joinable
    result = pthread_attr_init(&tctx->tAttribute);
    if(result) {
        goto TERROR;
    }

    result = pthread_attr_setdetachstate(&tctx->tAttribute, PTHREAD_CREATE_JOINABLE);
    if(result) {
        goto TERROR;
    }

    tctx->task = NULL;
    tctx->t_arg = NULL;
    tctx->t_status = TPOOL_SUSPENDED;
    tctx->t_result = TPOOL_INVALID_STATE;
    tctx->thread_id = thread_id;

    // create the worker thread
    result = pthread_create(&tctx->t_handle, &tctx->tAttribute, tpool_worker_thread, (void *)(tctx));
    if(result) {
        goto TERROR;
    }

    // deinit the attribue
    pthread_attr_destroy(&tctx->tAttribute);
    return (oapv_thread_t)tctx;

TERROR:
    pthread_mutex_destroy(&tctx->c_section);
    pthread_cond_destroy(&tctx->w_event);
    pthread_cond_destroy(&tctx->r_event);
    pthread_attr_destroy(&tctx->tAttribute);
    free(tctx);

    return NULL; // error handling, can't create a worker thread with proper initialization
}

static tpool_result_t tpool_assign_task(oapv_thread_t thread_id, oapv_fn_thread_entry_t entry, void *arg)
{
    // assign the task function and argument
    // worker thread may be in running state or suspended state
    // if worker thread is in suspended state, it can be waiting for first run or it has finished one task and is waiting again
    // if worker thread is in running state, it will come to waiting state
    // in any case, waiting on read event will always work

    thread_ctx_t *tctx = (thread_ctx_t *)(thread_id);
    if(!tctx) {
        return TPOOL_INVALID_ARG;
    }

    // lock the mutex and wait on read event
    pthread_mutex_lock(&tctx->c_section);
    while(tctx->t_status == TPOOL_RUNNING) {
        pthread_cond_wait(&tctx->r_event, &tctx->c_section);
    }

    // thread is in suspended state
    tctx->t_status = TPOOL_RUNNING;
    tctx->task = entry;
    tctx->t_arg = arg;
    // signal the worker thread to wake up and run the task
    pthread_cond_signal(&tctx->w_event);
    pthread_mutex_unlock(&tctx->c_section); // release the lock

    return TPOOL_SUCCESS;
}

static tpool_result_t tpool_retrieve_result(oapv_thread_t thread_id, int *ret)
{
    // whatever task has been assigned to worker thread
    // wait for it to finish get the result

    thread_ctx_t *t_context = (thread_ctx_t *)(thread_id);
    if(!t_context) {
        return TPOOL_INVALID_ARG;
    }

    tpool_result_t result = TPOOL_SUCCESS;

    pthread_mutex_lock(&t_context->c_section);
    while(TPOOL_RUNNING == t_context->t_status) {
        pthread_cond_wait(&t_context->r_event, &t_context->c_section);
    }
    result = t_context->t_result;
    if(ret != NULL)
        *ret = t_context->task_ret;
    pthread_mutex_unlock(&t_context->c_section);
    return result;
}

static tpool_result_t tpool_terminate_thread(oapv_thread_t *thread_id)
{
    // handler to close the thread
    // close the thread handle
    // release all the resource
    //  delete the thread context object

    thread_ctx_t *t_context = (thread_ctx_t *)(*thread_id);
    if(!t_context) {
        return TPOOL_INVALID_ARG;
    }

    // The worker thread might be in suspended state or may be processing a task
    pthread_mutex_lock(&t_context->c_section);
    while(TPOOL_RUNNING == t_context->t_status) {
        pthread_cond_wait(&t_context->r_event, &t_context->c_section);
    }

    t_context->t_status = TPOOL_TERMINATED;
    pthread_cond_signal(&t_context->w_event);

    pthread_mutex_unlock(&t_context->c_section);

    // join the worker thread
    pthread_join(t_context->t_handle, NULL);

    // clean all the synchronization memebers
    pthread_mutex_destroy(&t_context->c_section);
    pthread_cond_destroy(&t_context->w_event);
    pthread_cond_destroy(&t_context->r_event);

    // delete the thread context memory
    free(t_context);
    (*thread_id) = NULL;
    return TPOOL_SUCCESS;
}

static int tpool_threadsafe_decrement(oapv_sync_obj_t sobj, volatile int *pcnt)
{
    thread_mutex_t *imutex = (thread_mutex_t *)(sobj);
    int             temp = 0;

    // lock the mutex, decrement the count and release the mutex
    pthread_mutex_lock(&imutex->lmutex);
    temp = *pcnt;
    *pcnt = --temp;
    pthread_mutex_unlock(&imutex->lmutex);

    return temp;
}

oapv_sync_obj_t oapv_tpool_sync_obj_create()
{
    thread_mutex_t *imutex = (thread_mutex_t *)malloc(sizeof(thread_mutex_t));
    if(0 == imutex) {
        return 0; // failure case
    }

    // intialize the mutex
    int result = pthread_mutex_init(&imutex->lmutex, NULL);
    if(result) {
        if(imutex) {
            free(imutex);
        }
        imutex = 0;
    }

    return imutex;
}

tpool_result_t oapv_tpool_sync_obj_delete(oapv_sync_obj_t *sobj)
{

    thread_mutex_t *imutex = (thread_mutex_t *)(*sobj);

    // delete the mutex
    pthread_mutex_destroy(&imutex->lmutex);

    // free the memory
    free(imutex);
    *sobj = NULL;

    return TPOOL_SUCCESS;
}

void oapv_tpool_enter_cs(oapv_sync_obj_t sobj)
{
    thread_mutex_t *imutex = (thread_mutex_t *)(sobj);
    pthread_mutex_lock(&imutex->lmutex);
}

void oapv_tpool_leave_cs(oapv_sync_obj_t sobj)
{
    thread_mutex_t *imutex = (thread_mutex_t *)(sobj);
    pthread_mutex_unlock(&imutex->lmutex);
}

#else
typedef struct thread_ctx {
    // synchronization members
    HANDLE                 t_handle;  // worker thread handle
    HANDLE                 w_event;   // worker thread waiting event handle
    HANDLE                 r_event;   // signalling thread read event handle
    CRITICAL_SECTION       c_section; // critical section for fast synchronization

    // member field to run  a task
    oapv_fn_thread_entry_t task;
    void                  *t_arg;
    tpool_status_t         t_status;
    tpool_result_t         t_result;
    int                    task_ret;
    int                    thread_id;

} thread_ctx_t;

typedef struct thread_mutex {
#if WINDOWS_MUTEX_SYNC
    HANDLE lmutex;
#else
    CRITICAL_SECTION c_section; // critical section for fast synchronization
#endif

} thread_mutex_t;

static unsigned int __stdcall tpool_worker_thread(void *arg)
{
    /********************* main routine for thread pool worker thread *************************
    ********************** worker thread can remain in suspended or running state *************
    ********************* control the synchronization with help of thread context members *****/

    // member Initialization section
    thread_ctx_t *t_context = (thread_ctx_t *)arg;
    if(!t_context) {
        return 0; // error handling, more like a fail safe mechanism
    }

    while(1) {
        // worker thread loop
        // remains suspended/sleep waiting for an event
        WaitForSingleObject(t_context->w_event, INFINITE);

        // worker thread has received the event to wake up and perform operation
        EnterCriticalSection(&t_context->c_section);
        if(t_context->t_status == TPOOL_TERMINATED) {
            // received signal to terminate
            t_context->t_result = TPOOL_SUCCESS;
            LeaveCriticalSection(&t_context->c_section);
            break;
        }
        LeaveCriticalSection(&t_context->c_section);

        // worker thread state is running with entry function and arg set
        t_context->task_ret = t_context->task(t_context->t_arg);

        // change the state to suspended/waiting
        EnterCriticalSection(&t_context->c_section);
        t_context->t_status = TPOOL_SUSPENDED;
        t_context->t_result = TPOOL_SUCCESS;
        LeaveCriticalSection(&t_context->c_section);

        // send an event to thread, waiting for it to finish it's task
        SetEvent(t_context->r_event);
    }

    return 0;
}

static oapv_thread_t tpool_create_thread(oapv_tpool_t *tp, int thread_id)
{
    if(!tp) {
        return NULL; // error management
    }

    thread_ctx_t *thread_context = NULL;
    thread_context = (thread_ctx_t *)malloc(sizeof(thread_ctx_t));

    if(!thread_context) {
        return NULL; // error management, bad alloc
    }

    // create waiting event
    // create waiting event as automatic reset, only one thread can come out of waiting state
    // done intentionally ... signally happens from different thread and only worker thread should be able to respond
    thread_context->w_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if(!thread_context->w_event) {
        goto TERROR; // error handling, can't create event handler
    }

    thread_context->r_event = CreateEvent(NULL, TRUE, TRUE, NULL); // read event is enabled by default
    if(!thread_context->r_event) {
        goto TERROR;
    }

    InitializeCriticalSection(&(thread_context->c_section)); // This section for fast data retrieval

    // intialize the state variables for the thread context object
    thread_context->task = NULL;
    thread_context->t_arg = NULL;
    thread_context->t_status = TPOOL_SUSPENDED;
    thread_context->t_result = TPOOL_INVALID_STATE;
    thread_context->thread_id = thread_id;

    thread_context->t_handle = (HANDLE)_beginthreadex(NULL, 0, tpool_worker_thread, (void *)thread_context, 0, NULL); // create a thread store the handle and pass the handle to context
    if(!thread_context->t_handle) {
        goto TERROR;
    }

    // Everything created and intialized properly
    // return the created thread_context;
    return (oapv_thread_t)thread_context;

TERROR:
    if(thread_context->w_event) {
        CloseHandle(thread_context->w_event);
    }
    if(thread_context->r_event) {
        CloseHandle(thread_context->r_event);
    }
    DeleteCriticalSection(&thread_context->c_section);
    if(thread_context) {
        free(thread_context);
    }

    return NULL; // error handling, can't create a worker thread with proper initialization
}

static tpool_result_t tpool_assign_task(oapv_thread_t thread_id, oapv_fn_thread_entry_t entry, void *arg)
{
    // assign the task function and argument
    // worker thread may be in running state or suspended state
    // if worker thread is in suspended state, it can be waiting for first run or it has finished one task and is waiting again
    // if worker thread is in running state, it will come to waiting state
    // in any case, waiting on read event will always work

    thread_ctx_t *t_context = (thread_ctx_t *)(thread_id);
    if(!t_context) {
        return TPOOL_INVALID_ARG;
    }

    WaitForSingleObject(t_context->r_event, INFINITE);

    // worker thread is in waiting state
    EnterCriticalSection(&t_context->c_section);
    t_context->t_status = TPOOL_RUNNING;
    t_context->task = entry;
    t_context->t_arg = arg;
    // signal the worker thread to wake up and run the task
    ResetEvent(t_context->r_event);
    SetEvent(t_context->w_event);
    LeaveCriticalSection(&t_context->c_section);

    return TPOOL_SUCCESS;
}

static tpool_result_t tpool_retrieve_result(oapv_thread_t thread_id, int *ret)
{
    // whatever task has been assigned to worker thread
    // wait for it to finish get the result
    thread_ctx_t *t_context = (thread_ctx_t *)(thread_id);
    if(!t_context) {
        return TPOOL_INVALID_ARG;
    }

    tpool_result_t result = TPOOL_SUCCESS;

    WaitForSingleObject(t_context->r_event, INFINITE);

    // worker thread has finished it's job and now it is in waiting state
    EnterCriticalSection(&t_context->c_section);
    result = t_context->t_result;
    if(ret != NULL)
        *ret = t_context->task_ret;
    LeaveCriticalSection(&t_context->c_section);

    return result;
}

tpool_result_t tpool_terminate_thread(oapv_thread_t *thread_id)
{
    // handler to close the thread
    // close the thread handle
    // release all the resource
    //  delete the thread context object

    // the thread may be running or it is in suspended state
    // if it is in suspended state, read event will be active
    // if it is in running state, read event will be active after sometime

    thread_ctx_t *t_context = (thread_ctx_t *)(*thread_id);
    if(!t_context) {
        return TPOOL_INVALID_ARG;
    }

    WaitForSingleObject(t_context->r_event, INFINITE);

    // worker thread is in waiting state
    EnterCriticalSection(&t_context->c_section);
    t_context->t_status = TPOOL_TERMINATED;
    LeaveCriticalSection(&t_context->c_section);

    // signal the worker thread to wake up and run the task
    SetEvent(t_context->w_event);

    // wait for worker thread to finish it's routine
    WaitForSingleObject(t_context->t_handle, INFINITE);
    CloseHandle(t_context->t_handle); // freed all the resources for the thread

    CloseHandle(t_context->w_event);
    CloseHandle(t_context->r_event);
    DeleteCriticalSection(&t_context->c_section);

    // delete the thread context memory
    free(t_context);
    (*thread_id) = NULL;

    return TPOOL_SUCCESS;
}

static int tpool_threadsafe_decrement(oapv_sync_obj_t sobj, volatile int *pcnt)
{
    thread_mutex_t *imutex = (thread_mutex_t *)(sobj);
    int             temp = 0;

#if WINDOWS_MUTEX_SYNC
    // let's lock the mutex
    DWORD dw_wait_result = WaitForSingleObject(imutex->lmutex, INFINITE); // wait for infinite time

    switch(dw_wait_result) {
        // The thread got ownership of the mutex
    case WAIT_OBJECT_0:
        temp = *pcnt;
        *pcnt = --temp;
        // Release ownership of the mutex object
        ReleaseMutex(imutex->lmutex);
        break;
        // The thread got ownership of an abandoned mutex
        // The database is in an indeterminate state
    case WAIT_ABANDONED:
        temp = *pcnt;
        temp--;
        *pcnt = temp;
        break;
    }
#else
    EnterCriticalSection(&imutex->c_section);
    temp = *pcnt;
    *pcnt = --temp;
    LeaveCriticalSection(&imutex->c_section);
#endif
    return temp;
}

oapv_sync_obj_t oapv_tpool_sync_obj_create()
{
    thread_mutex_t *imutex = (thread_mutex_t *)malloc(sizeof(thread_mutex_t));
    if(0 == imutex) {
        return 0; // failure case
    }

#if WINDOWS_MUTEX_SYNC
    // initialize the created mutex instance
    imutex->lmutex = CreateMutex(NULL, FALSE, NULL);
    if(0 == imutex->lmutex) {
        if(imutex) {
            free(imutex);
        }
        return 0;
    }
#else
    // initialize the critical section
    InitializeCriticalSection(&(imutex->c_section));
#endif
    return imutex;
}

tpool_result_t oapv_tpool_sync_obj_delete(oapv_sync_obj_t *sobj)
{
    thread_mutex_t *imutex = (thread_mutex_t *)(*sobj);
#if WINDOWS_MUTEX_SYNC
    // release the mutex
    CloseHandle(imutex->lmutex);
#else
    // delete critical section
    DeleteCriticalSection(&imutex->c_section);
#endif

    // free the memory
    free(imutex);
    *sobj = NULL;

    return TPOOL_SUCCESS;
}

void oapv_tpool_enter_cs(oapv_sync_obj_t sobj)
{
    thread_mutex_t *imutex = (thread_mutex_t *)(sobj);
    EnterCriticalSection(&imutex->c_section);
}
void oapv_tpool_leave_cs(oapv_sync_obj_t sobj)
{
    thread_mutex_t *imutex = (thread_mutex_t *)(sobj);
    LeaveCriticalSection(&imutex->c_section);
}

#endif

tpool_result_t oapv_tpool_init(oapv_tpool_t *tp, int maxtask)
{
    // assign handles to threadcontroller object
    // handles for create, run, join and terminate will be given to controller  object

    tp->create = tpool_create_thread;
    tp->run = tpool_assign_task;
    tp->join = tpool_retrieve_result;
    tp->release = tpool_terminate_thread;
    tp->max_task_cnt = maxtask;

    return TPOOL_SUCCESS;
}

tpool_result_t oapv_tpool_deinit(oapv_tpool_t *tp)
{
    // reset all the handler to NULL
    tp->create = NULL;
    tp->run = NULL;
    tp->join = NULL;
    tp->release = NULL;
    tp->max_task_cnt = 0;

    return TPOOL_SUCCESS;
}

int oapv_tpool_spinlock_wait(volatile int *addr, int val)
{
    int temp;

    while(1) {
        temp = *addr; // thread safe volatile read
        if(temp == val || temp == -1) {
            break;
        }
    }
    return temp;
}

void threadsafe_assign_oapv(volatile int *addr, int val)
{
    // thread safe volatile assign
    *addr = val;
}
