/*
 * Copyright 2023 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#include "thread_sched.h"
#include "u_cpu_detect.h"
#include "u_debug.h"

DEBUG_GET_ONCE_BOOL_OPTION(pin_threads, "mesa_pin_threads", false)

bool
util_thread_scheduler_enabled(void)
{
#if DETECT_ARCH_X86 || DETECT_ARCH_X86_64
   return util_get_cpu_caps()->num_L3_caches > 1 ||
          debug_get_option_pin_threads();
#else
   return false;
#endif
}

void
util_thread_scheduler_init_state(unsigned *state)
{
   *state = UINT32_MAX;

   util_thread_sched_apply_policy(thrd_current(), UTIL_THREAD_APP_CALLER, 0,
                                  NULL); /* keep as NULL */
}

/**
 * Apply the optimal thread scheduling policy for the given thread.
 *
 * "name" determines which thread the policy is being applied to.
 *
 * "app_thread_cpu" is the CPU where the application thread currently
 * resides.
 *
 * "sched_state" is a per-gl_context state that this function uses to track
 * what happened in previous invocations.
 */
bool
util_thread_sched_apply_policy(thrd_t thread, enum util_thread_name name,
                               unsigned app_thread_cpu, unsigned *sched_state)
{
#if DETECT_ARCH_X86 || DETECT_ARCH_X86_64
   if (debug_get_option_pin_threads()) {
      /* Pin threads to a specific CPU. This is done only once. *sched_state
       * is true if this is the first time we are doing it.
       */
      if (sched_state && !*sched_state)
         return false;

      /* Each thread is assigned to a different CPU. */
      unsigned mask = BITFIELD_BIT(name);
      if (sched_state)
         *sched_state = 0;
      return util_set_thread_affinity(thread, &mask, NULL, 32);
   }

   /* Don't do anything for the app thread with the L3 chasing policy. */
   if (name == UTIL_THREAD_APP_CALLER)
      return false;

   /* Move Mesa threads to the L3 core complex where the app thread
    * resides. We call this "L3 chasing".
    *
    * This improves multithreading performance by up to 33% on Ryzen 3900X.
    */
   const struct util_cpu_caps_t *caps = util_get_cpu_caps();
   int L3_cache = caps->cpu_to_L3[app_thread_cpu];

   /* Don't do anything if the app thread hasn't moved to a different
    * core complex. (*sched_state contains the last set L3 index)
    */
   if (L3_cache == U_CPU_INVALID_L3 ||
       (sched_state && L3_cache == *sched_state))
      return false;

   /* Apply the policy. */
   if (sched_state)
      *sched_state = L3_cache;

   return util_set_thread_affinity(thread, caps->L3_affinity_mask[L3_cache],
                                   NULL, caps->num_cpu_mask_bits);
#else
   return false;
#endif
}
