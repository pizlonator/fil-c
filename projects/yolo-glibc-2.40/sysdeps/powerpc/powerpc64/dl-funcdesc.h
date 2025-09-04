/* PowerPC ELFv1 function descriptor definition.
   Copyright (C) 2009-2024 Free Software Foundation, Inc.
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

#ifndef _DL_FUNCDESC_H
#define _DL_FUNCDESC_H

#if _CALL_ELF != 2
/* A PowerPC64 function descriptor.  The .plt (procedure linkage
   table) and .opd (official procedure descriptor) sections are
   arrays of these.  */
typedef struct
{
  Elf64_Addr fd_func;
  Elf64_Addr fd_toc;
  Elf64_Addr fd_aux;
} Elf64_FuncDesc;
#endif

#endif
