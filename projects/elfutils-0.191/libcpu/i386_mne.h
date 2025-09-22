/* Disassembler for x86, MNE enums.
   Copyright (C) 2007, 2008, 2009, 2011 Red Hat, Inc.
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

#ifndef _I386_MNE_H
#define _I386_MNE_H	1

#ifndef MNEFILE
# define MNEFILE "i386.mnemonics"
#endif

/* The index can be stored in the instrtab.  */
enum
  {
#define MNE(name) MNE_##name,
#include MNEFILE
#undef MNE
    MNE_INVALID,
    MNE_COUNT = MNE_INVALID,
  };

#endif /* i386_mne.h */
