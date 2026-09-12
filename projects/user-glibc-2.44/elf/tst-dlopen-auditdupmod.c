/* Directly opened test module that gets reopened from the auditor.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

#include <stdio.h>
#include <stdlib.h>
#include <support/xdlfcn.h>

int auditdupmod_status;

/* Used to check for successful relocation processing.  */
int *auditdupmod_status_address = &auditdupmod_status;

static void __attribute__ ((constructor))
init (void)
{
  ++auditdupmod_status;
  printf ("info: tst-dlopen-auditdupmod.so constructor called (status %d)\n",
          auditdupmod_status);
}

static void __attribute__ ((destructor))
fini (void)
{
  /* The tst-dlopen-auditdup-auditmod.so auditor incremented
     auditdupmod_status.  */
  printf ("info: tst-dlopen-auditdupmod.so destructor called (status %d)\n",
          auditdupmod_status);
  if (auditdupmod_status != 2)
    {
      puts ("error: auditdupmod_status == 2 expected");
      exit (1);
    }
}
