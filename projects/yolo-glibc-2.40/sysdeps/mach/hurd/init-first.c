/* Initialization code run first thing by the ELF startup code.  For Hurd.
   Copyright (C) 1995-2024 Free Software Foundation, Inc.
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

#include <assert.h>
#include <hurd.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sysdep.h>
#include <dl-sysdep.h>
#include <set-hooks.h>
#include "hurdstartup.h"

#include <ldsodefs.h>
#include <fpu_control.h>
#include <libc-diag.h>
#include <libc-internal.h>

extern void __mach_init (void);
extern void __init_misc (int, char **, char **);

extern int __libc_argc attribute_hidden;
extern char **__libc_argv attribute_hidden;
extern char **_dl_argv;

#if !defined (SHARED) && (defined (THREAD_SET_STACK_GUARD) || defined (THREAD_SET_POINTER_GUARD))
/* In the static case, we need to set up TLS early so that the stack
   protection guard can be read at from TLS by the GCC-generated snippets,
   on architectures that store the guard in TLS and not globally.  */
# define USE_INIT1_TCBHEAD 1
#else
# define USE_INIT1_TCBHEAD 0
#endif

#if USE_INIT1_TCBHEAD
static tcbhead_t __init1_tcbhead;
#endif

/* Things that want to be run before _hurd_init or much anything else.
   Importantly, these are called before anything tries to use malloc.  */
DEFINE_HOOK (_hurd_preinit_hook, (void));


/* We call this once the Hurd magic is all set up and we are ready to be a
   Posixoid program.  This does the same things the generic version does.  */
static void
posixland_init (int argc, char **argv, char **envp)
{
  /* Now we have relocations etc. we can start signals etc.  */
  _hurd_libc_proc_init (argv);

#ifdef SHARED
  /* Make sure we don't initialize twice.  */
  if (__libc_initial)
    {
      /* Set the FPU control word to the proper default value.  */
      __setfpucw (__fpu_control);
    }
  else
    {
      /* Initialize data structures so the additional libc can do RPCs.  */
      __mach_init ();
    }
#else /* !SHARED */
  __setfpucw (__fpu_control);
#endif

  /* Save the command-line arguments.  */
  __libc_argc = argc;
  __libc_argv = argv;
  __environ = envp;

#ifndef SHARED
  _dl_non_dynamic_init ();
#endif
  __init_misc (argc, argv, envp);
}

static void
init (void **data)
{
  int argc = (int) (uintptr_t) *data;
  char **argv = (void *) (data + 1);
  char **envp = &argv[argc + 1];
  struct hurd_startup_data *d;

  /* Since the cthreads initialization code uses malloc, and the
     malloc initialization code needs to get at the environment, make
     sure we can find it.  We'll need to do this again later on since
     switching stacks changes the location where the environment is
     stored.  */
  __environ = envp;

  while (*envp)
    ++envp;
  d = (void *) ++envp;

#ifndef SHARED

  /* If we are the bootstrap task started by the kernel,
     then after the environment pointers there is no Hurd
     data block; the argument strings start there.  */
  if ((void *) d == argv[0] || d->phdr == 0)
    {
      __libc_enable_secure = 0;
      /* With a new enough linker (binutils-2.23 or better),
         the magic __ehdr_start symbol will be available and
         __libc_start_main will have done this that way already.  */
      if (_dl_phdr == NULL)
        {
          /* We may need to see our own phdrs, e.g. for TLS setup.
             Try the usual kludge to find the headers without help from
             the exec server.  */
          extern const void __executable_start;
          const ElfW(Ehdr) *const ehdr = &__executable_start;
          _dl_phdr = (const void *) ehdr + ehdr->e_phoff;
          _dl_phnum = ehdr->e_phnum;
          assert (ehdr->e_phentsize == sizeof (ElfW(Phdr)));
        }
    }
  else
    {
      __libc_enable_secure = d->flags & EXEC_SECURE;
      _dl_phdr = (ElfW(Phdr) *) d->phdr;
      _dl_phnum = d->phdrsz / sizeof (ElfW(Phdr));
      assert (d->phdrsz % sizeof (ElfW(Phdr)) == 0);
    }
#endif

  if ((void *) d == argv[0])
    return;

  _hurd_init_dtable = d->dtable;
  _hurd_init_dtablesize = d->dtablesize;

  if (d->portarray || d->intarray)
    /* Initialize library data structures, start signal processing, etc.  */
    _hurd_init (d->flags, argv,
		d->portarray, d->portarraysize,
		d->intarray, d->intarraysize);
}

/* Do the first essential initializations that must precede all else.  */
static inline void
first_init (void)
{
  /* Initialize data structures so we can do RPCs.  */
  __mach_init ();

#ifndef SHARED
  GLRO(dl_pagesize) = __vm_page_size;
#endif

#if USE_INIT1_TCBHEAD
  _hurd_tls_init (&__init1_tcbhead, 0);
#endif

  RUN_RELHOOK (_hurd_preinit_hook, ());
}

#ifdef SHARED
/* This function is called specially by the dynamic linker to do early
   initialization of the shared C library before normal initializers
   expecting a Posixoid environment can run.  */

void
_dl_init_first (void *data)
{
  first_init ();
  init (data);
}

/* The regular posixland initialization is what goes into libc's
   normal initializer.  */
/* NOTE!  The linker notices the magical name `_init' and sets the DT_INIT
   pointer in the dynamic section based solely on that.  It is convention
   for this function to be in the `.init' section, but the symbol name is
   the only thing that really matters!!  */
strong_alias (posixland_init, _init);

void
__libc_init_first (int argc, char **argv, char **envp)
{
  /* Everything was done in the shared library initializer, _init.  */
}

#else /* SHARED */

strong_alias (posixland_init, __libc_init_first);

/* XXX This is all a crock and I am not happy with it.
   This poorly-named function is called by static-start.S,
   which should not exist at all.  */
void
inhibit_stack_protector
_hurd_stack_setup (void **argptr)
{
  /* This is the very first C code that runs in a statically linked
     executable -- calling this function is the first thing that _start in
     static-start.S does.  Once this function returns, the unusual way that it
     does (see below), _start jumps to _start1, the regular start-up code.

     _start1 expects the arguments, environment, and a Hurd data block to be
     located at the top of the stack.  The data may already be located there,
     or we may need to receive it from the exec server.  If the data is located
     on the stack (just above our call frame), argptr points to it.  Note that
     this may not be a valid pointer in case we're supposed to receive the
     arguments from the exec server, so we can not dereference it yet.  */

  void *caller = RETURN_ADDRESS (0);
  /* Init the essential things.  */
  first_init ();

  void doinit (intptr_t *data)
    {
      init ((void **) data);
      RETURN_TO (data, caller, 0);
      __builtin_unreachable ();
    }

  /* _hurd_startup () will attempt to receive the data block from the exec
     server; or if that is not possible, will take the data from the pointer
     we pass it here.  The important point here is that the data
     _hurd_startup () collects may be allocated in its stack frame (with
     alloca), which is why _hurd_startup () does not return the normal way.
     Instead, it invokes a callback (which is not expected to return normally
     either).

     Our callback not only passes the data pointer to init (), but also jumps
     out of the call stack back to our caller (i.e. to _start1), while setting
     the stack pointer to the data (which is somewhere on the current stack
     anyway).  This way, _start1 find the data on the top of the stack, just as
     it expects to.  */
  _hurd_startup (argptr, &doinit);
  __builtin_unreachable ();
}
#endif


/* This function is defined here so that if this file ever gets into
   ld.so we will get a link error.  Having this file silently included
   in ld.so causes disaster, because the _init definition above will
   cause ld.so to gain an init function, which is not a cool thing. */

void
_dl_start (void)
{
  abort ();
}
