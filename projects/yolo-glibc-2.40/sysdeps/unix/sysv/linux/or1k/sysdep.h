/* Assembler and syscall macros.  OpenRISC version.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

#ifndef _LINUX_OR1K_SYSDEP_H
#define _LINUX_OR1K_SYSDEP_H 1

#include <sysdeps/unix/sysv/linux/sysdep.h>
#include <sysdeps/or1k/sysdep.h>
#include <sysdeps/unix/sysdep.h>
#include <tls.h>

/* "workarounds" for generic code needing to handle 64-bit time_t.  */

#undef __NR_clock_getres
#undef __NR_futex
#undef __NR_ppoll
#undef __NR_pselect6
#undef __NR_recvmmsg
#undef __NR_rt_sigtimedwait
#undef __NR_semtimedop
#undef __NR_utimensat

/* Fix sysdeps/unix/sysv/linux/clock_getcpuclockid.c.  */
#define __NR_clock_getres	__NR_clock_getres_time64
/* Fix sysdeps/nptl/lowlevellock-futex.h.  */
#define __NR_futex		__NR_futex_time64
/* Fix sysdeps/unix/sysv/linux/pause.c.  */
#define __NR_ppoll		__NR_ppoll_time64
/* Fix sysdeps/unix/sysv/linux/select.c.  */
#define __NR_pselect6		__NR_pselect6_time64
/* Fix sysdeps/unix/sysv/linux/recvmmsg.c.  */
#define __NR_recvmmsg		__NR_recvmmsg_time64
/* Fix sysdeps/unix/sysv/linux/sigtimedwait.c.  */
#define __NR_rt_sigtimedwait	__NR_rt_sigtimedwait_time64
/* Fix sysdeps/unix/sysv/linux/semtimedop.c.  */
#define __NR_semtimedop		__NR_semtimedop_time64
/* Hack sysdeps/unix/sysv/linux/generic/utimes.c.  */
#define __NR_utimensat		__NR_utimensat_time64

#undef SYS_ify
#define SYS_ify(syscall_name)   (__NR_##syscall_name)

/* Linux uses a negative return value to indicate syscall errors,
   unlike most Unices, which use the condition codes' carry flag.

   Since version 2.1 the return value of a system call might be
   negative even if the call succeeded.  E.g., the lseek system call
   might return a large offset.  Therefore we must not anymore test
   for < 0, but test for a real error by making sure the value in R0
   is a real error number.  Linus said he will make sure the no syscall
   returns a value in -1 .. -4095 as a valid result so we can safely
   test with -4095.  */

#ifdef __ASSEMBLER__

/* Macros used in syscall-template.S */
#define ret          l.jr r9; l.nop
#define ret_NOERRNO  l.jr r9; l.nop

#undef	DO_CALL
#define DO_CALL(syscall_name) \
  l.addi r11, r0, SYS_ify (syscall_name); \
  l.sys 1; \
   l.nop

#undef	PSEUDO
#define PSEUDO(name, syscall_name, args) \
  ENTRY (name); \
  DO_CALL(syscall_name); \
  /* if -4096 < ret < 0 holds, it's an error */ \
  l.sfgeui r11, 0xf001; \
  l.bf L(pseudo_end); \
   l.nop

#undef	PSEUDO_NOERRNO
#define PSEUDO_NOERRNO(name, syscall_name, args)  \
  ENTRY (name);           \
  DO_CALL(syscall_name)

#undef	PSEUDO_END
#define PSEUDO_END(name) \
L(pseudo_end): \
  l.j SYSCALL_ERROR_NAME; \
  l.ori r3,r11,0; \
  END (name)

#undef	PSEUDO_END_NOERRNO
#define PSEUDO_END_NOERRNO(name) \
  END (name)

#ifndef PIC
/* For static code, on error jump to __syscall_error directly.  */
# define SYSCALL_ERROR_NAME __syscall_error
#elif !IS_IN (libc)
/* Use the internal name for libc shared objects.  */
# define SYSCALL_ERROR_NAME __GI___syscall_error
#else
/* Otherwise, on error do a full PLT jump.  */
# define SYSCALL_ERROR_NAME plt(__syscall_error)
#endif

#else /* not __ASSEMBLER__ */

#include <errno.h>

extern long int __syscall_error (long int neg_errno);

#undef INTERNAL_SYSCALL
#define INTERNAL_SYSCALL(name, nr, args...) \
	INTERNAL_SYSCALL_NCS (SYS_ify (name), nr, args)

/* The _NCS variant allows non-constant syscall numbers.  */
#undef INTERNAL_SYSCALL_NCS
#define INTERNAL_SYSCALL_NCS(number, nr, args...) \
	({ unsigned long int __sys_result;				\
	  {								\
	    long int _sc_ret = (long int) number;			\
	    LOAD_ARGS_##nr (args)					\
	    register long int __sc_ret __asm__ ("r11") = _sc_ret;	\
	    __asm__ __volatile__ ("l.sys 1\n\t"				\
				  " l.nop\n\t"				\
				  : "+r" (__sc_ret)			\
				  : ASM_ARGS_##nr			\
				  : ASM_CLOBBERS_##nr			\
				    "r12", "r13", "r15", "r17", "r19",	\
				    "r21", "r23", "r25", "r27", "r29",	\
				    "r31", "memory");			\
	    __sys_result = __sc_ret;					\
	  }								\
	  (long int) __sys_result; })

/* From here on we have nested macros that generate code for
   setting up syscall arguments.  */

#define LOAD_ARGS_0()

#define ASM_ARGS_0
#define ASM_CLOBBERS_0  "r3", ASM_CLOBBERS_1

#define LOAD_ARGS_1(a) \
  long int _a = (long int)(a);                             \
  register long int __a __asm__ ("r3") = _a;
#define ASM_ARGS_1 "r" (__a)
#define ASM_CLOBBERS_1  "r4", ASM_CLOBBERS_2

#define LOAD_ARGS_2(a, b) \
  long int _b = (long int)(b);                             \
  LOAD_ARGS_1 (a)                                          \
  register long int __b __asm__ ("r4") = _b;
#define ASM_ARGS_2 ASM_ARGS_1, "r" (__b)
#define ASM_CLOBBERS_2  "r5", ASM_CLOBBERS_3

#define LOAD_ARGS_3(a, b, c) \
  long int _c = (long int)(c);                             \
  LOAD_ARGS_2 (a, b)                                       \
  register long int __c __asm__ ("r5") = _c;
#define ASM_ARGS_3 ASM_ARGS_2, "r" (__c)
#define ASM_CLOBBERS_3  "r6", ASM_CLOBBERS_4

#define LOAD_ARGS_4(a, b, c, d) \
    LOAD_ARGS_3 (a, b, c)                                  \
  long int _d = (long int)(d);                             \
  register long int __d __asm__ ("r6") = _d;
#define ASM_ARGS_4 ASM_ARGS_3, "r" (__d)
#define ASM_CLOBBERS_4  "r7", ASM_CLOBBERS_5

#define LOAD_ARGS_5(a, b, c, d, e) \
  long int _e = (long int)(e);                             \
  LOAD_ARGS_4 (a, b, c, d)                                 \
  register long int __e __asm__ ("r7") = _e;
#define ASM_ARGS_5 ASM_ARGS_4, "r" (__e)
#define ASM_CLOBBERS_5  "r8", ASM_CLOBBERS_6

#define LOAD_ARGS_6(a, b, c, d, e, f) \
  long int _f = (long int)(f);                             \
  LOAD_ARGS_5 (a, b, c, d, e)                              \
  register long int __f __asm__ ("r8") = _f;
#define ASM_ARGS_6 ASM_ARGS_5, "r" (__f)
#define ASM_CLOBBERS_6

#endif /* not __ASSEMBLER__ */

#endif /* linux/or1k/sysdep.h */
