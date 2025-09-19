/* Perform initialization and invoke main.
   Copyright (C) 1998-2024 Free Software Foundation, Inc.
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
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

/* Note: This code is only part of the startup code proper for
   statically linked binaries.  For dynamically linked binaries, it
   resides in libc.so.  */

/* Mark symbols hidden in static PIE for early self relocation to work.  */
#if BUILD_PIE_DEFAULT
# pragma GCC visibility push(hidden)
#endif

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ldsodefs.h>
#include <libc-diag.h>
#include <libc-internal.h>
#include <elf/libc-early-init.h>
#include <stdbool.h>
#include <elf-initfini.h>
#include <shlib-compat.h>
#include <dlfcn/dlerror.h>
#include <errno.h>

#include <elf/dl-tunables.h>

#include <pizlonated_runtime.h>

extern void __libc_init_first (int argc, char **argv, char **envp);

#include <tls.h>
#ifndef SHARED
# include <dl-osinfo.h>
# ifndef THREAD_SET_STACK_GUARD
/* Only exported for architectures that don't store the stack guard canary
   in thread local area.  */
uintptr_t __stack_chk_guard attribute_relro;
# endif
# ifndef  THREAD_SET_POINTER_GUARD
/* Only exported for architectures that don't store the pointer guard
   value in thread local area.  */
uintptr_t __pointer_chk_guard_local attribute_relro attribute_hidden;
# endif
#endif


#ifdef MAIN_AUXVEC_ARG
/* main gets passed a pointer to the auxiliary.  */
# define MAIN_AUXVEC_DECL	, void *
# define MAIN_AUXVEC_PARAM	, auxvec
#else
# define MAIN_AUXVEC_DECL
# define MAIN_AUXVEC_PARAM
#endif

#ifndef ARCH_INIT_CPU_FEATURES
# define ARCH_INIT_CPU_FEATURES()
#endif

/* Obtain the definition of __libc_start_call_main.  */
#include <libc_start_call_main.h>

#include <libc-start.h>

int __libc_start_main (int (*main) (int, char **, char **
					 MAIN_AUXVEC_DECL),
                       int argc,
                       char **argv,
                       char **ev,
                       ElfW(auxv_t) *auxvec)
     __attribute__ ((noreturn));

static void errno_handler(int errno_value)
{
  errno = errno_value;
}

static void dlerror_handler(const char* str)
{
  __libc_dlerror_result_string = (char*)str;
}

int
__libc_start_main (int (*main) (int, char **, char ** MAIN_AUXVEC_DECL),
                   int argc, char **argv,
                   char **ev,
                   ElfW(auxv_t) *auxvec)
{
  zmake_setjmp_save_sigmask(true);
  
  __environ = ev;
  GLRO(dl_auxv) = auxvec;

  /* Initialize libpthread if linked in.  */
  if (__pthread_initialize_minimal != NULL)
    __pthread_initialize_minimal ();

  ARCH_INIT_CPU_FEATURES ();

  ARCH_SETUP_TLS ();
  
  /* Perform early initialization.  In the shared case, this function
     is called from the dynamic loader as early as possible.  */
  __libc_early_init (true);

  zregister_sys_errno_handler(errno_handler);
  zregister_sys_dlerror_handler(dlerror_handler);

#ifndef SHARED
  /* Call the initializer of the libc.  This is only needed here if we
     are compiling for the static library in which case we haven't
     run the constructors in `_dl_start_user'.  */
  __libc_init_first (argc, argv, __environ);

  /* Some security at this point.  Prevent starting a SUID binary where
     the standard file descriptors are not opened.  We have to do this
     only for statically linked applications since otherwise the dynamic
     loader did the work already.  */
  if (__builtin_expect (__libc_enable_secure, 0))
    __libc_check_standard_fds ();
#endif /* !SHARED */

  /* Call the initializer of the program, if any.  */
#ifdef SHARED
  if (__builtin_expect (GLRO(dl_debug_mask) & DL_DEBUG_IMPCALLS, 0))
    zprintf ("\ninitialize program: %s\n\n", argv[0]);

  zrun_deferred_global_ctors();

  if (__glibc_unlikely (GLRO(dl_debug_mask) & DL_DEBUG_IMPCALLS))
    zprintf ("\ntransferring control: %s\n\n", argv[0]);
#else /* !SHARED */
  zrun_deferred_global_ctors();
#endif

  __libc_start_call_main (main, argc, argv MAIN_AUXVEC_PARAM);
}

