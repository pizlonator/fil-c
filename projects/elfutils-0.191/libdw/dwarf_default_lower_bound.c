/* Get the default subrange lower bound for a given language.
   Copyright (C) 2016 Red Hat, Inc.
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

#include <dwarf.h>
#include "libdwP.h"

/* Determine default lower bound from language, as per the DWARF5
   "Subrange Type Entries" table.  */
int
dwarf_default_lower_bound (int lang, Dwarf_Sword *result)
{
  switch (lang)
    {
    case DW_LANG_C:
    case DW_LANG_C89:
    case DW_LANG_C99:
    case DW_LANG_C11:
    case DW_LANG_C_plus_plus:
    case DW_LANG_C_plus_plus_03:
    case DW_LANG_C_plus_plus_11:
    case DW_LANG_C_plus_plus_14:
    case DW_LANG_ObjC:
    case DW_LANG_ObjC_plus_plus:
    case DW_LANG_Java:
    case DW_LANG_D:
    case DW_LANG_Python:
    case DW_LANG_UPC:
    case DW_LANG_OpenCL:
    case DW_LANG_Go:
    case DW_LANG_Haskell:
    case DW_LANG_OCaml:
    case DW_LANG_Rust:
    case DW_LANG_Swift:
    case DW_LANG_Dylan:
    case DW_LANG_RenderScript:
    case DW_LANG_BLISS:
      *result = 0;
      return 0;

    case DW_LANG_Ada83:
    case DW_LANG_Ada95:
    case DW_LANG_Cobol74:
    case DW_LANG_Cobol85:
    case DW_LANG_Fortran77:
    case DW_LANG_Fortran90:
    case DW_LANG_Fortran95:
    case DW_LANG_Fortran03:
    case DW_LANG_Fortran08:
    case DW_LANG_Pascal83:
    case DW_LANG_Modula2:
    case DW_LANG_Modula3:
    case DW_LANG_PLI:
    case DW_LANG_Julia:
      *result = 1;
      return 0;

    default:
      __libdw_seterrno (DWARF_E_UNKNOWN_LANGUAGE);
      return -1;
    }
}
INTDEF (dwarf_default_lower_bound)
