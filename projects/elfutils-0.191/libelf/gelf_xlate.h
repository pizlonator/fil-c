/* Helper file for type conversion function generation.
   Copyright (C) 1998, 1999, 2000, 2002, 2004, 2007 Red Hat, Inc.
   This file is part of elfutils.
   Contributed by Ulrich Drepper <drepper@redhat.com>, 1998.

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


/* Simple types.  */
FUNDAMENTAL (ADDR, Addr, LIBELFBITS);
FUNDAMENTAL (OFF, Off, LIBELFBITS);
FUNDAMENTAL (HALF, Half, LIBELFBITS);
FUNDAMENTAL (WORD, Word, LIBELFBITS);
FUNDAMENTAL (SWORD, Sword, LIBELFBITS);
FUNDAMENTAL (XWORD, Xword, LIBELFBITS);
FUNDAMENTAL (SXWORD, Sxword, LIBELFBITS);
FUNDAMENTAL (RELR, Relr, LIBELFBITS);

/* The structured types.  */
TYPE (Ehdr, LIBELFBITS)
TYPE (Phdr, LIBELFBITS)
TYPE (Shdr, LIBELFBITS)
TYPE (Sym, LIBELFBITS)
TYPE (Rel, LIBELFBITS)
TYPE (Rela, LIBELFBITS)
TYPE (Note, LIBELFBITS)
TYPE (Dyn, LIBELFBITS)
TYPE (Syminfo, LIBELFBITS)
TYPE (Move, LIBELFBITS)
TYPE (Lib, LIBELFBITS)
TYPE (auxv_t, LIBELFBITS)
TYPE (Chdr, LIBELFBITS)


/* Prepare for the next round.  */
#undef LIBELFBITS
