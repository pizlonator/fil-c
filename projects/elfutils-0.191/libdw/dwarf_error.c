/* Retrieve ELF descriptor used for DWARF access.
   Copyright (C) 2002-2005, 2009, 2014, 2015, 2017, 2018 Red Hat, Inc.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <stddef.h>

#include "libdwP.h"


/* The error number.  */
static __thread int global_error;


int
dwarf_errno (void)
{
  int result = global_error;
  global_error = DWARF_E_NOERROR;
  return result;
}
INTDEF(dwarf_errno)


/* XXX For now we use string pointers.  Once the table stablelizes
   make it more DSO-friendly.  */
static const char *errmsgs[] =
  {
    [DWARF_E_NOERROR] = N_("no error"),
    [DWARF_E_UNKNOWN_ERROR] = N_("unknown error"),
    [DWARF_E_INVALID_ACCESS] = N_("invalid access"),
    [DWARF_E_NO_REGFILE] = N_("no regular file"),
    [DWARF_E_IO_ERROR] = N_("I/O error"),
    [DWARF_E_INVALID_ELF] = N_("invalid ELF file"),
    [DWARF_E_NO_DWARF] = N_("no DWARF information"),
    [DWARF_E_COMPRESSED_ERROR] = N_("cannot decompress DWARF"),
    [DWARF_E_NOELF] = N_("no ELF file"),
    [DWARF_E_GETEHDR_ERROR] = N_("cannot get ELF header"),
    [DWARF_E_NOMEM] = N_("out of memory"),
    [DWARF_E_UNIMPL] = N_("not implemented"),
    [DWARF_E_INVALID_CMD] = N_("invalid command"),
    [DWARF_E_INVALID_VERSION] = N_("invalid version"),
    [DWARF_E_INVALID_FILE] = N_("invalid file"),
    [DWARF_E_NO_ENTRY] = N_("no entries found"),
    [DWARF_E_INVALID_DWARF] = N_("invalid DWARF"),
    [DWARF_E_NO_STRING] = N_("no string data"),
    [DWARF_E_NO_DEBUG_STR] = N_(".debug_str section missing"),
    [DWARF_E_NO_DEBUG_LINE_STR] = N_(".debug_line_str section missing"),
    [DWARF_E_NO_STR_OFFSETS] = N_(".debug_str_offsets section missing"),
    [DWARF_E_NO_ADDR] = N_("no address value"),
    [DWARF_E_NO_CONSTANT] = N_("no constant value"),
    [DWARF_E_NO_REFERENCE] = N_("no reference value"),
    [DWARF_E_INVALID_REFERENCE] = N_("invalid reference value"),
    [DWARF_E_NO_DEBUG_LINE] = N_(".debug_line section missing"),
    [DWARF_E_INVALID_DEBUG_LINE] = N_("invalid .debug_line section"),
    [DWARF_E_TOO_BIG] = N_("debug information too big"),
    [DWARF_E_VERSION] = N_("invalid DWARF version"),
    [DWARF_E_INVALID_DIR_IDX] = N_("invalid directory index"),
    [DWARF_E_ADDR_OUTOFRANGE] = N_("address out of range"),
    [DWARF_E_NO_DEBUG_LOC] = N_(".debug_loc section missing"),
    [DWARF_E_NO_DEBUG_LOCLISTS] = N_(".debug_loclists section missing"),
    [DWARF_E_NO_LOC_VALUE] = N_("not a location list value"),
    [DWARF_E_NO_BLOCK] = N_("no block data"),
    [DWARF_E_INVALID_LINE_IDX] = N_("invalid line index"),
    [DWARF_E_INVALID_ARANGE_IDX] = N_("invalid address range index"),
    [DWARF_E_NO_MATCH] = N_("no matching address range"),
    [DWARF_E_NO_FLAG] = N_("no flag value"),
    [DWARF_E_INVALID_OFFSET] = N_("invalid offset"),
    [DWARF_E_NO_DEBUG_RANGES] = N_(".debug_ranges section missing"),
    [DWARF_E_NO_DEBUG_RNGLISTS] = N_(".debug_rnglists section missing"),
    [DWARF_E_INVALID_CFI] = N_("invalid CFI section"),
    [DWARF_E_NO_ALT_DEBUGLINK] = N_("no alternative debug link found"),
    [DWARF_E_INVALID_OPCODE] = N_("invalid opcode"),
    [DWARF_E_NOT_CUDIE] = N_("not a CU (unit) DIE"),
    [DWARF_E_UNKNOWN_LANGUAGE] = N_("unknown language code"),
    [DWARF_E_NO_DEBUG_ADDR] = N_(".debug_addr section missing"),
    [DWARF_E_UNKNOWN_SECTION] = N_("unknown section"),
  };
#define nerrmsgs (sizeof (errmsgs) / sizeof (errmsgs[0]))


void
internal_function
__libdw_seterrno (int value)
{
  global_error = (value >= 0 && value < (int) nerrmsgs
		  ? value : DWARF_E_UNKNOWN_ERROR);
}


const char *
dwarf_errmsg (int error)
{
  int last_error = global_error;

  if (error == 0)
    return last_error != 0 ? _(errmsgs[last_error]) : NULL;
  else if (error < -1 || error >= (int) nerrmsgs)
    return _(errmsgs[DWARF_E_UNKNOWN_ERROR]);

  return _(errmsgs[error == -1 ? last_error : error]);
}
INTDEF(dwarf_errmsg)
