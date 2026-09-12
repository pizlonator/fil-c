#include <stdint.h>

#define STACK_CHK_GUARD (((tcbhead_t __seg_fs *)0)->stack_guard)

#ifdef PTRGUARD_LOCAL
extern uintptr_t __pointer_chk_guard_local;
# define POINTER_CHK_GUARD __pointer_chk_guard_local
#else
extern uintptr_t __pointer_chk_guard;
# define POINTER_CHK_GUARD __pointer_chk_guard
#endif
