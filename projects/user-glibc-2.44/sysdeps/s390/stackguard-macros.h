#include <stdint.h>

#define STACK_CHK_GUARD \
  ({ uintptr_t x; __asm__ ("ear %0,%%a0; sllg %0,%0,32; ear %0,%%a1; lg %0,0x28(%0)" : "=a" (x)); x; })

#ifdef PTRGUARD_LOCAL
extern uintptr_t __pointer_chk_guard_local;
# define POINTER_CHK_GUARD __pointer_chk_guard_local
#else
extern uintptr_t __pointer_chk_guard;
# define POINTER_CHK_GUARD __pointer_chk_guard
#endif
