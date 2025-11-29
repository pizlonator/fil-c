/*
 * Copyright 2023 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef CPU_SCHED_H
#define CPU_SCHED_H

#include "compiler.h"
#include "u_thread.h"

enum util_thread_name
{
   UTIL_THREAD_APP_CALLER,
   UTIL_THREAD_GLTHREAD,
   UTIL_THREAD_THREADED_CONTEXT,
   UTIL_THREAD_DRIVER_SUBMIT,
};

bool
util_thread_scheduler_enabled(void);

void
util_thread_scheduler_init_state(unsigned *state);

bool
util_thread_sched_apply_policy(thrd_t thread, enum util_thread_name name,
                               unsigned app_thread_cpu, unsigned *sched_state);

#endif
