/*
 * Copyright © 2012 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

#include <pthread.h>

struct NineSwapChain9;

#define MAXTHREADS 1

struct threadpool {
    pthread_mutex_t m;
    pthread_cond_t new_work;

    HANDLE wthread;
    pthread_t pthread;
    struct threadpool_task *workqueue;
    BOOL shutdown;
};

typedef void (*threadpool_task_func)(void *data);

struct threadpool_task {
    threadpool_task_func work;
    void *data;
    struct threadpool_task *next;
    pthread_cond_t finish;
    BOOL finished;
};

struct threadpool *_mesa_threadpool_create(struct NineSwapChain9 *swapchain);
void _mesa_threadpool_destroy(struct NineSwapChain9 *swapchain, struct threadpool *pool);
struct threadpool_task *_mesa_threadpool_queue_task(struct threadpool *pool,
                                                    threadpool_task_func func,
                                                    void *data);
void _mesa_threadpool_wait_for_task(struct threadpool *pool,
                                    struct threadpool_task **task);
#endif
