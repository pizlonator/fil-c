/* Helper functions for form handling.
   Copyright (C) 2003-2009, 2014 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2003.

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
#include <string.h>

#include "libdwP.h"


size_t
internal_function
__libdw_form_val_compute_len (struct Dwarf_CU *cu, unsigned int form,
			      const unsigned char *valp)
{
  const unsigned char *startp = valp;
  const unsigned char *endp = cu->endp;
  Dwarf_Word u128;
  size_t result;

  /* NB: This doesn't cover constant form lengths, which are
     already handled by the inlined __libdw_form_val_len.  */
  switch (form)
    {
    case DW_FORM_addr:
      result = cu->address_size;
      break;

    case DW_FORM_ref_addr:
      result = cu->version == 2 ? cu->address_size : cu->offset_size;
      break;

    case DW_FORM_strp:
    case DW_FORM_strp_sup:
    case DW_FORM_line_strp:
    case DW_FORM_sec_offset:
    case DW_FORM_GNU_ref_alt:
    case DW_FORM_GNU_strp_alt:
      result = cu->offset_size;
      break;

    case DW_FORM_block1:
      if (unlikely ((size_t) (endp - startp) < 1))
	goto invalid;
      result = *valp + 1;
      break;

    case DW_FORM_block2:
      if (unlikely ((size_t) (endp - startp) < 2))
	goto invalid;
      result = read_2ubyte_unaligned (cu->dbg, valp) + 2;
      break;

    case DW_FORM_block4:
      if (unlikely ((size_t) (endp - startp) < 4))
	goto invalid;
      result = read_4ubyte_unaligned (cu->dbg, valp) + 4;
      break;

    case DW_FORM_block:
    case DW_FORM_exprloc:
      if (valp >= endp)
       goto invalid;
      get_uleb128 (u128, valp, endp);
      result = u128 + (valp - startp);
      break;

    case DW_FORM_string:
      {
	const unsigned char *endstrp = memchr (valp, '\0',
					       (size_t) (endp - startp));
	if (unlikely (endstrp == NULL))
	  goto invalid;
	result = (size_t) (endstrp - startp) + 1;
	break;
      }

    case DW_FORM_sdata:
    case DW_FORM_udata:
    case DW_FORM_ref_udata:
    case DW_FORM_addrx:
    case DW_FORM_loclistx:
    case DW_FORM_rnglistx:
    case DW_FORM_strx:
    case DW_FORM_GNU_addr_index:
    case DW_FORM_GNU_str_index:
      if (valp >= endp)
       goto invalid;
      get_uleb128 (u128, valp, endp);
      result = valp - startp;
      break;

    case DW_FORM_indirect:
      /* The amount of data to skip in the DIE is the size of the actual
	 FORM data (which is __libdw_form_val_len) plus the size of the
	 uleb128 encoding that FORM (which is valp - startp).  */
      if (valp >= endp)
	goto invalid;
      get_uleb128 (u128, valp, endp);
      if (*valp == DW_FORM_indirect || *valp == DW_FORM_implicit_const)
	return (size_t) -1;
      result = __libdw_form_val_len (cu, u128, valp);
      if (result != (size_t) -1)
	result += valp - startp;
      else
        return (size_t) -1;
      break;

    default:
      goto invalid;
    }

  if (unlikely (result > (size_t) (endp - startp)))
    {
    invalid:
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      result = (size_t) -1;
    }

  return result;
}
