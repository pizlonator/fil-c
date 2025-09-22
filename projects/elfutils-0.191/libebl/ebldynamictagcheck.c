/* Check dynamic tag.
   Copyright (C) 2001, 2002, 2006 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2001.

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

#include <inttypes.h>
#include <libeblP.h>


bool
ebl_dynamic_tag_check (Ebl *ebl, int64_t tag)
{
  bool res = ebl != NULL ? ebl->dynamic_tag_check (tag) : false;

  if (!res
      && ((tag >= 0 && tag < DT_NUM)
	  || (tag >= DT_GNU_PRELINKED && tag <= DT_SYMINENT)
	  || (tag >= DT_GNU_HASH && tag <= DT_SYMINFO)
	  || tag == DT_VERSYM
	  || (tag >= DT_RELACOUNT && tag <= DT_VERNEEDNUM)
	  || tag == DT_AUXILIARY
	  || tag == DT_FILTER))
    res = true;

  return res;
}
