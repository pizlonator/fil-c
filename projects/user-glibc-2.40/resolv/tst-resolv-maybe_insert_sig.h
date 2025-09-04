/* Code snippet for optionally inserting ignored SIG records in resolver tests.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

/* Set to true for an alternative pass that inserts (ignored) SIG
   records.  This does not alter the response, so this property is not
   encoded in the QNAME.  The variable needs to be volatile because
   leaf attributes tell GCC that the response function is not
   called.  */
static volatile bool insert_sig;

static void
maybe_insert_sig (struct resolv_response_builder *b, const char *owner)
{
  resolv_response_open_record (b, owner, C_IN, T_SIG, 60);
  resolv_response_add_data (b, "", 1);
  resolv_response_close_record (b);
}
