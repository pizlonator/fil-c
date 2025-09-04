/* Dynamic linker system dependencies for Linux.
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

#include <_itoa.h>
#include <assert.h>
#include <dl-auxv.h>
#include <dl-osinfo.h>
#include <dl-parse_auxv.h>
#include <dl-procinfo.h>
#include <dl-tunables.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <ldsodefs.h>
#include <libc-internal.h>
#include <libintl.h>
#include <not-cancel.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <tls.h>
#include <unistd.h>
#include <dl-symbol-redir-ifunc.h>

#include <dl-machine.h>
#include <dl-hwcap-check.h>

#ifdef SHARED
extern char **_environ attribute_hidden;
extern char _end[] attribute_hidden;

/* Protect SUID program against misuse of file descriptors.  */
extern void __libc_check_standard_fds (void);

int __libc_enable_secure attribute_relro = 0;
rtld_hidden_data_def (__libc_enable_secure)
/* This variable contains the lowest stack address ever used.  */
void *__libc_stack_end attribute_relro = NULL;
rtld_hidden_data_def(__libc_stack_end)
void *_dl_random attribute_relro = NULL;

#ifndef DL_STACK_END
# define DL_STACK_END(cookie) ((void *) (cookie))
#endif

/* Arguments passed to dl_main.  */
struct dl_main_arguments
{
  const ElfW(Phdr) *phdr;
  ElfW(Word) phnum;
  ElfW(Addr) user_entry;
};

/* Separate function, so that dl_main can be called without the large
   array on the stack.  */
static void
_dl_sysdep_parse_arguments (void **start_argptr,
			    struct dl_main_arguments *args)
{
  _dl_argc = (intptr_t) *start_argptr;
  _dl_argv = (char **) (start_argptr + 1); /* Necessary aliasing violation.  */
  _environ = _dl_argv + _dl_argc + 1;
  for (char **tmp = _environ; ; ++tmp)
    if (*tmp == NULL)
      {
	/* Another necessary aliasing violation.  */
	GLRO(dl_auxv) = (ElfW(auxv_t) *) (tmp + 1);
	break;
      }

  dl_parse_auxv_t auxv_values = { 0, };
  _dl_parse_auxv (GLRO(dl_auxv), auxv_values);

  args->phdr = (const ElfW(Phdr) *) auxv_values[AT_PHDR];
  args->phnum = auxv_values[AT_PHNUM];
  args->user_entry = auxv_values[AT_ENTRY];
}

ElfW(Addr)
_dl_sysdep_start (void **start_argptr,
		  void (*dl_main) (const ElfW(Phdr) *phdr, ElfW(Word) phnum,
				   ElfW(Addr) *user_entry, ElfW(auxv_t) *auxv))
{
  __libc_stack_end = DL_STACK_END (start_argptr);

  struct dl_main_arguments dl_main_args;
  _dl_sysdep_parse_arguments (start_argptr, &dl_main_args);

  dl_hwcap_check ();

  __tunables_init (_environ);

  /* Initialize DSO sorting algorithm after tunables.  */
  _dl_sort_maps_init ();

  __brk (0);			/* Initialize the break.  */

#ifdef DL_PLATFORM_INIT
  DL_PLATFORM_INIT;
#endif

  /* Determine the length of the platform name.  */
  if (GLRO(dl_platform) != NULL)
    GLRO(dl_platformlen) = strlen (GLRO(dl_platform));

  if (__sbrk (0) == _end)
    /* The dynamic linker was run as a program, and so the initial break
       starts just after our bss, at &_end.  The malloc in dl-minimal.c
       will consume the rest of this page, so tell the kernel to move the
       break up that far.  When the user program examines its break, it
       will see this new value and not clobber our data.  */
    __sbrk (GLRO(dl_pagesize)
	    - (((uintptr_t) _end) & (GLRO(dl_pagesize) - 1)));

  /* If this is a SUID program we make sure that FDs 0, 1, and 2 are
     allocated.  If necessary we are doing it ourself.  If it is not
     possible we stop the program.  */
  if (__builtin_expect (__libc_enable_secure, 0))
    __libc_check_standard_fds ();

  (*dl_main) (dl_main_args.phdr, dl_main_args.phnum,
              &dl_main_args.user_entry, GLRO(dl_auxv));
  return dl_main_args.user_entry;
}

void
_dl_sysdep_start_cleanup (void)
{
}

void
_dl_show_auxv (void)
{
  char buf[64];
  ElfW(auxv_t) *av;

  /* Terminate string.  */
  buf[63] = '\0';

  /* The following code assumes that the AT_* values are encoded
     starting from 0 with AT_NULL, 1 for AT_IGNORE, and all other values
     close by (otherwise the array will be too large).  In case we have
     to support a platform where these requirements are not fulfilled
     some alternative implementation has to be used.  */
  for (av = GLRO(dl_auxv); av->a_type != AT_NULL; ++av)
    {
      static const struct
      {
	const char label[22];
	enum { unknown = 0, dec, hex, str, ignore } form : 8;
      } auxvars[] =
	{
	  [AT_EXECFD - 2] =		{ "EXECFD:            ", dec },
	  [AT_EXECFN - 2] =		{ "EXECFN:            ", str },
	  [AT_PHDR - 2] =		{ "PHDR:              0x", hex },
	  [AT_PHENT - 2] =		{ "PHENT:             ", dec },
	  [AT_PHNUM - 2] =		{ "PHNUM:             ", dec },
	  [AT_PAGESZ - 2] =		{ "PAGESZ:            ", dec },
	  [AT_BASE - 2] =		{ "BASE:              0x", hex },
	  [AT_FLAGS - 2] =		{ "FLAGS:             0x", hex },
	  [AT_ENTRY - 2] =		{ "ENTRY:             0x", hex },
	  [AT_NOTELF - 2] =		{ "NOTELF:            ", hex },
	  [AT_UID - 2] =		{ "UID:               ", dec },
	  [AT_EUID - 2] =		{ "EUID:              ", dec },
	  [AT_GID - 2] =		{ "GID:               ", dec },
	  [AT_EGID - 2] =		{ "EGID:              ", dec },
	  [AT_PLATFORM - 2] =		{ "PLATFORM:          ", str },
	  [AT_HWCAP - 2] =		{ "HWCAP:             ", hex },
	  [AT_CLKTCK - 2] =		{ "CLKTCK:            ", dec },
	  [AT_FPUCW - 2] =		{ "FPUCW:             ", hex },
	  [AT_DCACHEBSIZE - 2] =	{ "DCACHEBSIZE:       0x", hex },
	  [AT_ICACHEBSIZE - 2] =	{ "ICACHEBSIZE:       0x", hex },
	  [AT_UCACHEBSIZE - 2] =	{ "UCACHEBSIZE:       0x", hex },
	  [AT_IGNOREPPC - 2] =		{ "IGNOREPPC", ignore },
	  [AT_SECURE - 2] =		{ "SECURE:            ", dec },
	  [AT_BASE_PLATFORM - 2] =	{ "BASE_PLATFORM:     ", str },
	  [AT_SYSINFO - 2] =		{ "SYSINFO:           0x", hex },
	  [AT_SYSINFO_EHDR - 2] =	{ "SYSINFO_EHDR:      0x", hex },
	  [AT_RANDOM - 2] =		{ "RANDOM:            0x", hex },
	  [AT_HWCAP2 - 2] =		{ "HWCAP2:            0x", hex },
	  [AT_HWCAP3 - 2] =		{ "HWCAP3:            0x", hex },
	  [AT_HWCAP4 - 2] =		{ "HWCAP4:            0x", hex },
	  [AT_MINSIGSTKSZ - 2] =	{ "MINSIGSTKSZ:       ", dec },
	  [AT_L1I_CACHESIZE - 2] =	{ "L1I_CACHESIZE:     ", dec },
	  [AT_L1I_CACHEGEOMETRY - 2] =	{ "L1I_CACHEGEOMETRY: 0x", hex },
	  [AT_L1D_CACHESIZE - 2] =	{ "L1D_CACHESIZE:     ", dec },
	  [AT_L1D_CACHEGEOMETRY - 2] =	{ "L1D_CACHEGEOMETRY: 0x", hex },
	  [AT_L2_CACHESIZE - 2] =	{ "L2_CACHESIZE:      ", dec },
	  [AT_L2_CACHEGEOMETRY - 2] =	{ "L2_CACHEGEOMETRY:  0x", hex },
	  [AT_L3_CACHESIZE - 2] =	{ "L3_CACHESIZE:      ", dec },
	  [AT_L3_CACHEGEOMETRY - 2] =	{ "L3_CACHEGEOMETRY:  0x", hex },
	};
      unsigned int idx = (unsigned int) (av->a_type - 2);

      if ((unsigned int) av->a_type < 2u
	  || (idx < sizeof (auxvars) / sizeof (auxvars[0])
	      && auxvars[idx].form == ignore))
	continue;

      assert (AT_NULL == 0);
      assert (AT_IGNORE == 1);

      /* Some entries are handled in a special way per platform.  */
      if (_dl_procinfo (av->a_type, av->a_un.a_val) == 0)
	continue;

      if (idx < sizeof (auxvars) / sizeof (auxvars[0])
	  && auxvars[idx].form != unknown)
	{
	  const char *val = (char *) av->a_un.a_val;

	  if (__builtin_expect (auxvars[idx].form, dec) == dec)
	    val = _itoa ((unsigned long int) av->a_un.a_val,
			 buf + sizeof buf - 1, 10, 0);
	  else if (__builtin_expect (auxvars[idx].form, hex) == hex)
	    val = _itoa ((unsigned long int) av->a_un.a_val,
			 buf + sizeof buf - 1, 16, 0);

	  _dl_printf ("AT_%s%s\n", auxvars[idx].label, val);

	  continue;
	}

      /* Unknown value: print a generic line.  */
      char buf2[17];
      buf2[sizeof (buf2) - 1] = '\0';
      const char *val2 = _itoa ((unsigned long int) av->a_un.a_val,
				buf2 + sizeof buf2 - 1, 16, 0);
      const char *val =  _itoa ((unsigned long int) av->a_type,
				buf + sizeof buf - 1, 16, 0);
      _dl_printf ("AT_??? (0x%s): 0x%s\n", val, val2);
    }
}

#endif /* SHARED */
