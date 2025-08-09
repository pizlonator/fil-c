#ifndef I386_TIMER_H
#define I386_TIMER_H

#ifdef _MSC_VER
#include <intrin.h>
#pragma intrinsic(__rdtsc)
#pragma intrinsic(_BitScanReverse)
#endif

#define cycle_counter_units "cyc"

typedef unsigned long long int timestamp_t;

static inline timestamp_t read_cycle_counter () {
#ifdef __GNUC__
  timestamp_t ts;
#ifdef __x86_64__
  unsigned int eax, edx;
  asm volatile("rdtsc" : "=a" (eax), "=d" (edx));
  ts = ((timestamp_t) eax) | (((timestamp_t) edx) << 32);
#else
  asm volatile("rdtsc\n" : "=A" (ts));  
#endif
  return(ts);
#endif
#ifdef _MSC_VER
  return __rdtsc();
#endif
}

static inline int binary_order_of_magnitude(timestamp_t ts) {
#if __GNUC__
  return 64 - __builtin_clzll(ts);
#endif
#ifdef _MSC_VER
   unsigned long index;
   if (_BitScanReverse(&index, (uint32_t)(ts&0xFFFFFFFF)))
	return index;
   else if(_BitScanReverse(&index, (uint32_t)(ts>>32)))
	return 32 + index;
   else return 64;
#endif
}
#endif

