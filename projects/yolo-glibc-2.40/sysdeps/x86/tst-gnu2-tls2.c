#ifndef __x86_64__
#include <sys/platform/x86.h>

#define IS_SUPPORTED() CPU_FEATURE_ACTIVE (SSE2)
#endif

/* Clear XMM0...XMM7  */
#define PREPARE_MALLOC()				\
{							\
  asm volatile ("xorps %%xmm0, %%xmm0" : : : "xmm0" );	\
  asm volatile ("xorps %%xmm1, %%xmm1" : : : "xmm1" );	\
  asm volatile ("xorps %%xmm2, %%xmm2" : : : "xmm2" );	\
  asm volatile ("xorps %%xmm3, %%xmm3" : : : "xmm3" );	\
  asm volatile ("xorps %%xmm4, %%xmm4" : : : "xmm4" );	\
  asm volatile ("xorps %%xmm5, %%xmm5" : : : "xmm5" );	\
  asm volatile ("xorps %%xmm6, %%xmm6" : : : "xmm6" );	\
  asm volatile ("xorps %%xmm7, %%xmm7" : : : "xmm7" );	\
}

#include <elf/tst-gnu2-tls2.c>
