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

#ifndef WG14_SIGNALS_LOCK_UNLOCK_H
#define WG14_SIGNALS_LOCK_UNLOCK_H

#include "wg14_signals/config.h"

#define LOCK(x)                                                                \
  for(;;)                                                                      \
  {                                                                            \
    if(atomic_load_explicit(&(x), memory_order_relaxed))                       \
    {                                                                          \
      continue;                                                                \
    }                                                                          \
    unsigned expected = 0;                                                     \
    if(atomic_compare_exchange_weak_explicit(                                  \
       &(x), &expected, 1, memory_order_acq_rel, memory_order_relaxed))        \
    {                                                                          \
      break;                                                                   \
    }                                                                          \
  }

#define UNLOCK(x) atomic_store_explicit(&(x), 0, memory_order_release)

#endif
