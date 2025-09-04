#ifndef _SETJMP_H
#include <setjmp/setjmp.h>

#ifndef _ISOMAC
/* Now define the internal interfaces.  */

/* Internal machine-dependent function to restore context sans signal mask.  */
extern void __longjmp (__jmp_buf __env, int __val)
     __attribute__ ((__noreturn__)) attribute_hidden;

extern void ____longjmp_chk (__jmp_buf __env, int __val)
     __attribute__ ((__noreturn__)) attribute_hidden;

extern void __longjmp_chk (sigjmp_buf env, int val)
	  __attribute__ ((noreturn)) attribute_hidden;
/* The redirection in the installed header does not work with
   libc_hidden_proto.  */
#define longjmp __longjmp_chk
#define siglongjmp __longjmp_chk
libc_hidden_proto (__longjmp_chk)

/* Internal function to possibly save the current mask of blocked signals
   in ENV, and always set the flag saying whether or not it was saved.
   This is used by the machine-dependent definition of `__sigsetjmp'.
   Always returns zero, for convenience.  */
extern int __sigjmp_save (jmp_buf __env, int __savemask);

extern void _longjmp_unwind (jmp_buf env, int val);

extern void __libc_siglongjmp (sigjmp_buf env, int val)
	  __attribute__ ((noreturn)) attribute_hidden;
extern void __libc_longjmp (sigjmp_buf env, int val)
     __attribute__ ((noreturn)) attribute_hidden;

libc_hidden_proto (_setjmp)
libc_hidden_proto (__sigsetjmp)

# if IS_IN (rtld)
extern __typeof (__sigsetjmp) __sigsetjmp attribute_hidden;
# endif

#endif

#endif
