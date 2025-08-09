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

// #define WG14_SIGNALS_HAVE_ASYNC_SAFE_THREAD_LOCAL 0

#ifndef WG14_SIGNALS_CONFIG_H
#define WG14_SIGNALS_CONFIG_H

#ifndef WG14_SIGNALS_PREFIX
#define WG14_SIGNALS_PREFIX(x) x
#endif

#ifndef WG14_SIGNALS_INLINE
#define WG14_SIGNALS_INLINE inline
#endif

#ifndef WG14_SIGNALS_HAVE_ASYNC_SAFE_THREAD_LOCAL
/* https://maskray.me/blog/2021-02-14-all-about-thread-local-storage
will tell you all you need to know about TLS implementations and
which are async signal safe, and which are not.
*/
#if(defined(__GNUC__) || defined(_MSC_VER)) && !defined(__APPLE__)
#define WG14_SIGNALS_HAVE_ASYNC_SAFE_THREAD_LOCAL 1
#else
#define WG14_SIGNALS_HAVE_ASYNC_SAFE_THREAD_LOCAL 0
#endif
#endif

#ifndef WG14_SIGNALS_ASYNC_SAFE_THREAD_LOCAL
#if WG14_SIGNALS_HAVE_ASYNC_SAFE_THREAD_LOCAL
#ifdef __GNUC__
// ELF needs to use the initial or local exec TLS model to be async signal safe
//
// WARNING: This can cause issues with this library being loaded dynamically as
// part of a runtime loaded shared library!
#define WG14_SIGNALS_ASYNC_SAFE_THREAD_LOCAL                                   \
  _Thread_local __attribute__((tls_model("initial-exec")))
#elif defined(_MSC_VER)
// MSVC's thread locals are always async signal safe
#define WG14_SIGNALS_ASYNC_SAFE_THREAD_LOCAL _Thread_local
#endif
#endif
#endif

#ifndef WG14_SIGNALS_NULLPTR
#if __STDC_VERSION__ >= 202300L
#define WG14_SIGNALS_NULLPTR nullptr
#else
#define WG14_SIGNALS_NULLPTR NULL
#endif
#endif

#ifndef WG14_SIGNALS_DEFAULT_VISIBILITY
#ifdef _WIN32
#define WG14_SIGNALS_DEFAULT_VISIBILITY
#else
#define WG14_SIGNALS_DEFAULT_VISIBILITY __attribute__((visibility("default")))
#endif
#endif

#ifndef WG14_SIGNALS_EXTERN
#if WG14_SIGNALS_SOURCE
#ifdef _WIN32
#define WG14_SIGNALS_EXTERN extern __declspec(dllexport)
#else
#define WG14_SIGNALS_EXTERN extern __attribute__((visibility("default")))
#endif
#else
#define WG14_SIGNALS_EXTERN extern
#endif
#endif

#ifndef WG14_SIGNALS_STDERR_PRINTF
#include <stdio.h>
#define WG14_SIGNALS_STDERR_PRINTF(...) fprintf(stderr, __VA_ARGS__)
#endif

#ifdef __cplusplus
extern "C"
{
#endif


#ifdef __cplusplus
}
#endif

#endif
