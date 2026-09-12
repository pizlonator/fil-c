#include <stdint.h>

#define STACK_CHK_GUARD \
  ({ uintptr_t x; asm ("lwz %0,-28680(2)" : "=r" (x)); x; })

#ifdef PTRGUARD_LOCAL
extern uintptr_t __pointer_chk_guard_local;
# define POINTER_CHK_GUARD __pointer_chk_guard_local
#else
extern uintptr_t __pointer_chk_guard;
# define POINTER_CHK_GUARD __pointer_chk_guard
#endif
