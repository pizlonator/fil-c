#ifndef _SETJMP_H
#include <setjmp/setjmp.h>

extern void __libc_siglongjmp (sigjmp_buf env, int val)
	  __attribute__ ((noreturn)) attribute_hidden;
extern void __libc_longjmp (sigjmp_buf env, int val)
     __attribute__ ((noreturn)) attribute_hidden;

#endif
