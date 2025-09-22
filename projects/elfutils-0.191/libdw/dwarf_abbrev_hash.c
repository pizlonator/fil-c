/* Implementation of hash table for DWARF .debug_abbrev section content.
   Copyright (C) 2000-2010 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

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

#include "dwarf_sig8_hash.h"
#define NO_UNDEF
#include "libdwP.h"

#define next_prime __libdwarf_next_prime
extern size_t next_prime (size_t) attribute_hidden;

#include <dynamicsizehash_concurrent.c>

#undef next_prime
#define next_prime attribute_hidden __libdwarf_next_prime
#include "../lib/next_prime.c"
