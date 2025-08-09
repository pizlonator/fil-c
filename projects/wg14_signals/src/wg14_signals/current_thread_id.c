/* Proposed WG14 improved signals support
(C) 2024 Niall Douglas <http://www.nedproductions.biz/>
File Created: Nov 2024


Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License in the accompanying file
Licence.txt or at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#define _GNU_SOURCE

#include "wg14_signals/current_thread_id.h"

#ifdef _WIN32
extern __declspec(dllimport) unsigned long __stdcall GetCurrentThreadId(void);
#endif

#ifdef __linux__
#include <unistd.h>  // for syscall()
#endif

#if defined(__APPLE__)
#include <mach/mach_init.h>  // for mach_thread_self
#include <mach/mach_port.h>  // for mach_port_deallocate
#endif

#ifdef __FreeBSD__
#include <pthread_np.h>  // for pthread_getthreadid_np
#endif

#if WG14_SIGNALS_HAVE_ASYNC_SAFE_THREAD_LOCAL
#ifdef _WIN32
static
#endif
WG14_SIGNALS_ASYNC_SAFE_THREAD_LOCAL WG14_SIGNALS_PREFIX(thread_id_t)
WG14_SIGNALS_PREFIX(current_thread_id_cached) =
#ifdef _WIN32
0;
#else
WG14_SIGNALS_PREFIX(thread_id_t_tombstone);
#endif
#endif

static inline WG14_SIGNALS_PREFIX(thread_id_t) get_current_thread_id(void)
{
#ifdef _WIN32
  return (WG14_SIGNALS_PREFIX(thread_id_t)) GetCurrentThreadId();
#elif defined(__linux__)
  return (WG14_SIGNALS_PREFIX(thread_id_t)) gettid();
#elif defined(__APPLE__)
  thread_port_t tid = mach_thread_self();
  mach_port_deallocate(mach_task_self(), tid);
  return (WG14_SIGNALS_PREFIX(thread_id_t)) tid;
#else
  return (WG14_SIGNALS_PREFIX(thread_id_t)) pthread_getthreadid_np();
#endif
}

WG14_SIGNALS_PREFIX(thread_id_t)
WG14_SIGNALS_PREFIX(internal_current_thread_id_cached_set)(void)
{
#if WG14_SIGNALS_HAVE_ASYNC_SAFE_THREAD_LOCAL
  WG14_SIGNALS_PREFIX(current_thread_id_cached) = get_current_thread_id();
  return WG14_SIGNALS_PREFIX(current_thread_id_cached);
#else
  return get_current_thread_id();
#endif
}
