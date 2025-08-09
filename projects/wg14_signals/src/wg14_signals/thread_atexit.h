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

#ifndef WG14_SIGNALS_THREAD_ATEXIT_H
#define WG14_SIGNALS_THREAD_ATEXIT_H

#include "wg14_signals/config.h"

#ifdef __cplusplus
extern "C"
{
#endif

  extern int WG14_SIGNALS_PREFIX(thread_atexit)(void (*)(void *obj), void *obj);

#ifdef __cplusplus
}
#endif

#endif
