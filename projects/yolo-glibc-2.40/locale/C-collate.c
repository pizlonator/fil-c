/* Copyright (C) 1995-2024 Free Software Foundation, Inc.
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

#include <endian.h>
#include <stdint.h>
#include "localeinfo.h"

#include "C-collate-seq.c"

const struct __locale_data _nl_C_LC_COLLATE attribute_hidden =
{
  _nl_C_name,
  NULL, 0, 0,			/* no file mapped */
  NULL,				/* No cached data.  */
  UNDELETABLE,
  0,
  19,
  {
    /* _NL_COLLATE_NRULES */
    { .word = 0 },
    /* _NL_COLLATE_RULESETS */
    { .string = NULL },
    /* _NL_COLLATE_TABLEMB */
    { .string = NULL },
    /* _NL_COLLATE_WEIGHTMB */
    { .string = NULL },
    /* _NL_COLLATE_EXTRAMB */
    { .string = NULL },
    /* _NL_COLLATE_INDIRECTMB */
    { .string = NULL },
    /* _NL_COLLATE_GAP1 */
    { .string = NULL },
    /* _NL_COLLATE_GAP2 */
    { .string = NULL },
    /* _NL_COLLATE_GAP3 */
    { .string = NULL },
    /* _NL_COLLATE_TABLEWC */
    { .string = NULL },
    /* _NL_COLLATE_WEIGHTWC */
    { .string = NULL },
    /* _NL_COLLATE_EXTRAWC */
    { .string = NULL },
    /* _NL_COLLATE_INDIRECTWC */
    { .string = NULL },
    /* _NL_COLLATE_SYMB_HASH_SIZEMB */
    { .string = NULL },
    /* _NL_COLLATE_SYMB_TABLEMB */
    { .string = NULL },
    /* _NL_COLLATE_SYMB_EXTRAMB */
    { .string = NULL },
    /* _NL_COLLATE_COLLSEQMB */
    { .string = collseqmb },
    /* _NL_COLLATE_COLLSEQWC */
    { .string = (const char *) collseqwc },
    /* _NL_COLLATE_CODESET */
    { .string = _nl_C_codeset }
  }
};
