/* Add unsigned integer to a section.
   Copyright (C) 2002 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <libasmP.h>

#ifndef SIZE
# define SIZE 8
#endif

#define UFCT(size) _UFCT(size)
#define _UFCT(size) asm_adduint##size
#define FCT(size) _FCT(size)
#define _FCT(size) asm_addint##size
#define UTYPE(size) _UTYPE(size)
#define _UTYPE(size) uint##size##_t
#define TYPE(size) _TYPE(size)
#define _TYPE(size) int##size##_t


int
UFCT(SIZE) (AsmScn_t *asmscn, UTYPE(SIZE) num)
{
  return INTUSE(FCT(SIZE)) (asmscn, (TYPE(SIZE)) num);
}
