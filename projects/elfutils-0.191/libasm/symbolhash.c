/* Symbol hash table implementation.
   Copyright (C) 2001, 2002, 2016 Red Hat, Inc.
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

#include <string.h>

#include <libasmP.h>
#include <libebl.h>

/* Definitions for the symbol hash table.  */
#define TYPE AsmSym_t *
#define NAME asm_symbol_tab
#define ITERATE 1
#define REVERSE 1
#define COMPARE(a, b) \
  strcmp (dwelf_strent_str ((a)->strent), dwelf_strent_str ((b)->strent))

#define next_prime __libasm_next_prime
extern size_t next_prime (size_t) attribute_hidden;

#include "../lib/dynamicsizehash.c"

#undef next_prime
#define next_prime attribute_hidden __libasm_next_prime
#include "../lib/next_prime.c"
