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

#ifndef WG14_SIGNALS_GET_TID_H
#define WG14_SIGNALS_GET_TID_H

#include "config.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  //! \brief The type of a thread id
  typedef uintptr_t WG14_SIGNALS_PREFIX(thread_id_t);

  static const WG14_SIGNALS_PREFIX(thread_id_t)
  WG14_SIGNALS_PREFIX(thread_id_t_tombstone) = 0;

#if WG14_SIGNALS_HAVE_ASYNC_SAFE_THREAD_LOCAL
#ifdef _WIN32
  static
#else
  WG14_SIGNALS_EXTERN
#endif
  WG14_SIGNALS_ASYNC_SAFE_THREAD_LOCAL WG14_SIGNALS_PREFIX(thread_id_t)
  WG14_SIGNALS_PREFIX(current_thread_id_cached);
#endif

  WG14_SIGNALS_EXTERN WG14_SIGNALS_PREFIX(thread_id_t)
  WG14_SIGNALS_PREFIX(internal_current_thread_id_cached_set)(void);

  //! \brief THREADSAFE; ASYNC SIGNAL SAFE; Retrieve the current thread id
  static WG14_SIGNALS_INLINE WG14_SIGNALS_PREFIX(thread_id_t)
  WG14_SIGNALS_PREFIX(current_thread_id)(void)
  {
#if WG14_SIGNALS_HAVE_ASYNC_SAFE_THREAD_LOCAL
    if(WG14_SIGNALS_PREFIX(current_thread_id_cached) ==
       WG14_SIGNALS_PREFIX(thread_id_t_tombstone))
    {
      WG14_SIGNALS_PREFIX(current_thread_id_cached) =
      WG14_SIGNALS_PREFIX(internal_current_thread_id_cached_set)();
    }
    return WG14_SIGNALS_PREFIX(current_thread_id_cached);
#else
  return WG14_SIGNALS_PREFIX(internal_current_thread_id_cached_set)();
#endif
  }

#ifdef __cplusplus
}
#endif

#endif
