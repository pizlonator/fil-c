/* High resolution time gathering
(C) 2024 Niall Douglas <http://www.nedproductions.biz/>
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

#pragma once

#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef _WIN32
#include <Windows.h>
#include <intrin.h>
#else
#include <time.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

  typedef uint64_t cpu_ticks_count;
  typedef uint64_t ns_count;

  static inline cpu_ticks_count get_ticks_count(memory_order rel)
  {
#if defined(__APPLE__) || DISABLE_INLINE_ASM
    (void) rel;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
#elif defined(__i386__) || defined(_M_IX86) || defined(__x86_64__) ||          \
defined(_M_X64)
#ifdef _MSC_VER
  unsigned int x = 0;
  return (uint64_t) __rdtscp(&x);
#else
  unsigned lo, hi, aux;
  switch(rel)
  {
  case memory_order_acquire:
    __asm__ __volatile__("rdtsc\nlfence" : "=a"(lo), "=d"(hi));
    break;
  case memory_order_release:
    __asm__ __volatile__("mfence\nrdtscp\n" : "=a"(lo), "=d"(hi), "=c"(aux));
    break;
  case memory_order_acq_rel:
  case memory_order_seq_cst:
    __asm__ __volatile__("mfence\nrdtscp\nlfence"
                         : "=a"(lo), "=d"(hi), "=c"(aux));
    break;
  default:
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    break;
  }
  return (uint64_t) lo | ((uint64_t) hi << 32);
#endif
#elif defined(_MSC_VER) && !defined(__clang__)
  (void) rel;
  LARGE_INTEGER val;
  if(!QueryPerformanceCounter(&val))
  {
#if _WIN32_WINNT >= 0x600
    return (cpu_ticks_count) GetTickCount64() * 1000000;
#else
    return (cpu_ticks_count) GetTickCount() * 1000000;
#endif
  }
  return (cpu_ticks_count) val.QuadPart;
#elif defined(__aarch64__) || defined(_M_ARM64)
  uint64_t value = 0;
  switch(rel)
  {
  case memory_order_acquire:
    __asm__ __volatile__("mrs %0, PMCCNTR_EL0; dsb sy"
                         : "=r"(value));  // NOLINT
    break;
  case memory_order_release:
    __asm__ __volatile__("dsb sy; mrs %0, PMCCNTR_EL0"
                         : "=r"(value));  // NOLINT
    break;
  case memory_order_acq_rel:
  case memory_order_seq_cst:
    __asm__ __volatile__("dsb sy; mrs %0 PMCCNTR_EL0; dsb sy"
                         : "=r"(value));  // NOLINT
    break;
  default:
    __asm__ __volatile__("mrs %0, PMCCNTR_EL0" : "=r"(value));  // NOLINT
    break;
  }
  return value;
#else
#error "Unsupported platform"
#endif
  }

  static inline ns_count get_ns_count(void)
  {
#ifdef _WIN32
    static double scalefactor;
    if(scalefactor == 0.0)
    {
      LARGE_INTEGER ticksPerSec;
      if(QueryPerformanceFrequency(&ticksPerSec))
        scalefactor = 1000000000.0 / ticksPerSec.QuadPart;
      else
        scalefactor = 1.0;
    }
    LARGE_INTEGER val;
    if(!QueryPerformanceCounter(&val))
    {
#if _WIN32_WINNT >= 0x600
      return (ns_count) GetTickCount64() * 1000000;
#else
      return (ns_count) GetTickCount() * 1000000;
#endif
    }
    return (ns_count) (val.QuadPart * scalefactor);
#else
#ifdef CLOCK_MONOTONIC
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ((ns_count) ts.tv_sec * 1000000000LL) + ts.tv_nsec;
#else
  struct timeval tv;
  gettimeofday(&tv, 0);
  return ((ns_count) tv.tv_sec * 1000000000LL) + tv.tv_usec * 1000LL;
#endif
#endif
  }

  static int ticks_per_second_comp(const void *_a, const void *_b)
  {
    const double a = *(double *) _a;
    const double b = *(double *) _b;
    if(a < b)
    {
      return -1;
    }
    if(a > b)
    {
      return 1;
    }
    return 0;
  }

  static cpu_ticks_count ticks_per_second(void)
  {
    static uint64_t v;
    if(v != 0)
    {
      return v;
    }
    double results[10];
    for(size_t n = 0; n < 10; n++)
    {
      cpu_ticks_count count1a = get_ticks_count(memory_order_acq_rel);
      ns_count ts1 = get_ns_count();
      cpu_ticks_count count1b = get_ticks_count(memory_order_acq_rel);
      while(get_ns_count() - ts1 < 100000000)
      {
      }
      cpu_ticks_count count2a = get_ticks_count(memory_order_acq_rel);
      ns_count ts2 = get_ns_count();
      cpu_ticks_count count2b = get_ticks_count(memory_order_acq_rel);
      results[n] = (double) (count2a + count2b - count1a - count1b) / 2.0 /
                   ((double) ts2 - ts1) * 1000000000.0;
    }
    qsort(results, 10, sizeof(double), ticks_per_second_comp);
    v = (cpu_ticks_count) ((results[4] + results[5]) / 2);
    return v;
  }

#ifdef __cplusplus
}
#endif
