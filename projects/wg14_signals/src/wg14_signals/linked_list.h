/* Proposed WG14 improved signals support
(C) 2025 Niall Douglas <http://www.nedproductions.biz/>
File Created: Feb 2025


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

#ifndef WG14_SIGNALS_LINKED_LIST_H
#define WG14_SIGNALS_LINKED_LIST_H

#include "wg14_signals/config.h"

#include <assert.h>

#define LIST_INSERT_FRONT(lst, item)                                           \
  if((lst).front == WG14_SIGNALS_NULLPTR)                                      \
  {                                                                            \
    assert((lst).back == WG14_SIGNALS_NULLPTR);                                \
    (lst).front = (lst).back = (item);                                         \
  }                                                                            \
  else                                                                         \
  {                                                                            \
    assert((lst).front != WG14_SIGNALS_NULLPTR);                               \
    assert((lst).back != WG14_SIGNALS_NULLPTR);                                \
    (item)->prev = WG14_SIGNALS_NULLPTR;                                       \
    (item)->next = (lst).front;                                                \
    (lst).front = (item);                                                      \
  }

#define LIST_INSERT_BACK(lst, item)                                            \
  if((lst).front == WG14_SIGNALS_NULLPTR)                                      \
  {                                                                            \
    assert((lst).back == WG14_SIGNALS_NULLPTR);                                \
    (lst).front = (lst).back = (item);                                         \
  }                                                                            \
  else                                                                         \
  {                                                                            \
    assert((lst).front != WG14_SIGNALS_NULLPTR);                               \
    assert((lst).back != WG14_SIGNALS_NULLPTR);                                \
    (item)->next = WG14_SIGNALS_NULLPTR;                                       \
    (item)->prev = (lst).back;                                                 \
    (lst).back = (item);                                                       \
  }

#define LIST_REMOVE(lst, item)                                                 \
  if((lst).front == (item) && (lst).back == (item))                            \
  {                                                                            \
    (lst).front = (lst).back = WG14_SIGNALS_NULLPTR;                           \
  }                                                                            \
  else if((lst).front == (item))                                               \
  {                                                                            \
    (lst).front = (item)->next;                                                \
    (lst).front->prev = WG14_SIGNALS_NULLPTR;                                  \
  }                                                                            \
  else if((lst).back == (item))                                                \
  {                                                                            \
    (lst).back = (item)->prev;                                                 \
    (lst).back->next = WG14_SIGNALS_NULLPTR;                                   \
  }                                                                            \
  else                                                                         \
  {                                                                            \
    (item)->next->prev = (item)->prev;                                         \
    (item)->prev->next = (item)->next;                                         \
  }                                                                            \
  (item)->next = (item)->prev = WG14_SIGNALS_NULLPTR;

#endif
