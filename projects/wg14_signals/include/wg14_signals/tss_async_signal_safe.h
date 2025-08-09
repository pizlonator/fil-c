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

#ifndef WG14_SIGNALS_ASYNC_SIGNAL_SAFE_TLS_H
#define WG14_SIGNALS_ASYNC_SIGNAL_SAFE_TLS_H

#include "config.h"

#ifdef __cplusplus
extern "C"
{
#endif

  //! \brief The type of an async signal safe thread local
  typedef struct WG14_SIGNALS_PREFIX(tss_async_signal_safe) *
  WG14_SIGNALS_PREFIX(tss_async_signal_safe);

  //! \brief The attributes for creating an async signal safe thread local
  struct WG14_SIGNALS_PREFIX(tss_async_signal_safe_attr)
  {
    int (*const create)(void **dest);  //!< Create an instance
    int (*const destroy)(void *v);     //!< Destroy an instance
  };

  //! \brief Create an async signal safe thread local instance
  WG14_SIGNALS_EXTERN int WG14_SIGNALS_PREFIX(tss_async_signal_safe_create)(
  WG14_SIGNALS_PREFIX(tss_async_signal_safe) * val,
  const struct WG14_SIGNALS_PREFIX(tss_async_signal_safe_attr) * attr);

  //! \brief Destroy an async signal safe thread local instance
  WG14_SIGNALS_EXTERN int WG14_SIGNALS_PREFIX(tss_async_signal_safe_destroy)(
  WG14_SIGNALS_PREFIX(tss_async_signal_safe) val);

  /*! \brief THREADSAFE Initialise an async signal safe thread local instance
  for a specific thread

  The initialisation and registration of a thread local state for a specific
  thread is not async signal safe, and so must be manually performed by each
  thread before use of an async signal safe thread local instance. Call this
  function somewhere near the beginning of your thread instance.

  It is safe to call this functions many times in a thread.
  */
  WG14_SIGNALS_EXTERN int WG14_SIGNALS_PREFIX(
  tss_async_signal_safe_thread_init)(WG14_SIGNALS_PREFIX(tss_async_signal_safe)
                                     val);

  /*! \brief THREADSAFE ASYNC-SIGNAL-SAFE Get the thread local value for the
   * current thread.
   */
  WG14_SIGNALS_EXTERN void *WG14_SIGNALS_PREFIX(tss_async_signal_safe_get)(
  WG14_SIGNALS_PREFIX(tss_async_signal_safe) val);

#ifdef __cplusplus
}
#endif

#endif
