#include <stdint.h>

#define STACK_CHK_GUARD \
  ({ uintptr_t x; asm ("ld %0,-28688(13)" : "=r" (x)); x; })

#ifdef PTRGUARD_LOCAL
extern uintptr_t __pointer_chk_guard_local;
# define POINTER_CHK_GUARD __pointer_chk_guard_local
#else
extern uintptr_t __pointer_chk_guard;
# define POINTER_CHK_GUARD __pointer_chk_guard
#endif
