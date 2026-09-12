/* Copyright (C) 2005-2026 Free Software Foundation, Inc.
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

#include <ldsodefs.h>
#include <link.h>

/* Find the exception index table containing PC.  */

_Unwind_Ptr
__gnu_Unwind_Find_exidx (_Unwind_Ptr pc, int * pcount)
{
  struct dl_find_object data;
  if (GLRO(dl_find_object) ((void *) pc, &data) < 0)
    return 0;
  *pcount = data.dlfo_eh_count;
  return (_Unwind_Ptr) data.dlfo_eh_frame;
}
