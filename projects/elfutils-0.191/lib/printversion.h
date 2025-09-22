/* Common argp_print_version_hook for all tools.
   Copyright (C) 2017 The Qt Company Ltd.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifndef PRINTVERSION_H
#define PRINTVERSION_H 1

#include <argp.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif
/* Defined in version.c.  Common ARGP_PROGRAM_VERSION_HOOK_DEF.  */
void print_version (FILE *stream, struct argp_state *state);

/* We need define two variables, argp_program_version_hook and
   argp_program_bug_address, in all programs.  argp.h declares these
   variables as non-const (which is correct in general).  But we can
   do better, it is not going to change.  So we want to move them into
   the .rodata section.  Define macros to do the trick.  The default
   linkage for consts in C++ is internal, so declare them extern.  */
extern void (*const apvh) (FILE *, struct argp_state *);
#define ARGP_PROGRAM_VERSION_HOOK_DEF \
  void (*const apvh) (FILE *, struct argp_state *) \
   __asm ("argp_program_version_hook")
extern const char *const apba__;
#define ARGP_PROGRAM_BUG_ADDRESS_DEF \
  const char *const apba__ __asm ("argp_program_bug_address")

#ifdef __cplusplus
}
#endif

#endif // PRINTVERSION_H
