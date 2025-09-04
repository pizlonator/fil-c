/* Code to enable profiling at program startup.
   Copyright (C) 1995-2024 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   In addition to the permissions in the GNU Lesser General Public
   License, the Free Software Foundation gives you unlimited
   permission to link the compiled version of this file with other
   programs, and to distribute those programs without any restriction
   coming from the use of this file.  (The GNU Lesser General Public
   License restrictions do apply in other respects; for example, they
   cover modification of the file, and distribution when not linked
   into another program.)

   Note that people who make modified versions of this file are not
   obligated to grant this special exception for their modified
   versions; it is their choice whether to do so.  The GNU Lesser
   General Public License gives permission to release a modified
   version without this exception; this exception also makes it
   possible to release a modified version which carries forward this
   exception.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <sys/types.h>
#include <sys/gmon.h>
#include <stdlib.h>
#include <unistd.h>
#include <elf-initfini.h>

/* Use __executable_start as the lowest address to keep profiling records
   if it provided by the linker.  */
extern const char __executable_start[] __attribute__ ((visibility ("hidden")));

extern char etext[];

#if !ELF_INITFINI
/* Instead of defining __gmon_start__ globally in gcrt1.o, we make it
   static and just put a pointer to it into the .preinit_array
   section.  */
# define GMON_START_ARRAY_SECTION ".preinit_array"
#endif

#ifdef GMON_START_ARRAY_SECTION
static void __gmon_start__ (void);
static void (*const gmon_start_initializer) (void)
  __attribute__ ((used, section (GMON_START_ARRAY_SECTION))) = &__gmon_start__;
static
#else
/* We cannot use the normal constructor mechanism to call
   __gmon_start__ because gcrt1.o appears before crtbegin.o in the link.
   Instead crti.o calls it specially.  */
extern void __gmon_start__ (void);
#endif

void
__gmon_start__ (void)
{
  /* Protect from being called more than once.  Since crti.o is linked
     into every shared library, each of their init functions will call us.  */
  static int called;

  if (called)
    return;

  called = 1;

  /* Start keeping profiling records.  */
  __monstartup ((u_long) &__executable_start, (u_long) &etext);

  /* Call _mcleanup before exiting; it will write out gmon.out from the
     collected data.  */
  atexit (&_mcleanup);
}
