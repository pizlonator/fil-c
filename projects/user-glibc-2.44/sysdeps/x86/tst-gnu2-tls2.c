#ifndef TEST_AMX
# ifndef __x86_64__
# include <sys/platform/x86.h>

# define IS_SUPPORTED() CPU_FEATURE_ACTIVE (SSE2)
# endif

/* Set XMM0...XMM7 to all 1s.  */
# define PREPARE_MALLOC()					\
{								\
  asm volatile ("pcmpeqd %%xmm0, %%xmm0" : : : "xmm0" );	\
  asm volatile ("pcmpeqd %%xmm1, %%xmm1" : : : "xmm1" );	\
  asm volatile ("pcmpeqd %%xmm2, %%xmm2" : : : "xmm2" );	\
  asm volatile ("pcmpeqd %%xmm3, %%xmm3" : : : "xmm3" );	\
  asm volatile ("pcmpeqd %%xmm4, %%xmm4" : : : "xmm4" );	\
  asm volatile ("pcmpeqd %%xmm5, %%xmm5" : : : "xmm5" );	\
  asm volatile ("pcmpeqd %%xmm6, %%xmm6" : : : "xmm6" );	\
  asm volatile ("pcmpeqd %%xmm7, %%xmm7" : : : "xmm7" );	\
}
#endif

#include <elf/tst-gnu2-tls2.c>

#ifndef TEST_AMX
v2di v1, v2, v3;
#endif
