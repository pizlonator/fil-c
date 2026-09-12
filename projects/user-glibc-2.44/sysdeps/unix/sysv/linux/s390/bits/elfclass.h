/* Copyright (C) 2001-2026 Free Software Foundation, Inc.
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

/* This file specifies the native word size of the machine, which indicates
   the ELF file class used for executables and shared objects on this
   machine.  */

#ifndef _LINK_H
# error "Never use <bits/elfclass.h> directly; include <link.h> instead."
#endif

#include <bits/wordsize.h>

#define __ELF_NATIVE_CLASS __WORDSIZE

/* 64 bit Linux for S/390 is exceptional as it has .hash section with
   64 bit entries.  */
typedef uint64_t Elf_Symndx;
