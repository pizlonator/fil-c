/* Zero byte detection, define whether to use stdbit.h
   Copyright (C) 2026 Free Software Foundation, Inc.
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
   <http://www.gnu.org/licenses/>.  */

/* These macros define whether index_first/index_last macro can use the
   stdbit.h routines, otherwise fallback that do not call libgcc is used
   instead.

   An architecture can override it is it uses a generic string routines
   on process startup and it can not use the libgcc routines for find
   trailing/leading zeros.  */
#define HAVE_BITOPTS_WORKING 1
