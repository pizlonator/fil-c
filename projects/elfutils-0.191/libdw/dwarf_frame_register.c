/* Get register location expression for frame.
   Copyright (C) 2009-2010, 2014 Red Hat, Inc.
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

#include "cfi.h"
#include <dwarf.h>

int
dwarf_frame_register (Dwarf_Frame *fs, int regno, Dwarf_Op ops_mem[3],
		      Dwarf_Op **ops, size_t *nops)
{
  /* Maybe there was a previous error.  */
  if (fs == NULL)
    return -1;

  if (unlikely (regno < 0))
    {
      __libdw_seterrno (DWARF_E_INVALID_ACCESS);
      return -1;
    }

  *ops = ops_mem;
  *nops = 0;

  if (unlikely ((size_t) regno >= fs->nregs))
    goto default_rule;

  const struct dwarf_frame_register *reg = &fs->regs[regno];

  switch (reg->rule)
    {
    case reg_unspecified:
    default_rule:
      /* Use the default rule for registers not yet mentioned in CFI.  */
      if (fs->cache->default_same_value)
	goto same_value;
      FALLTHROUGH;
    case reg_undefined:
      /* The value is known to be unavailable.  */
      break;

    case reg_same_value:
    same_value:
      /* The location is not known here, but the caller might know it.  */
      *ops = NULL;
      break;

    case reg_offset:
    case reg_val_offset:
      ops_mem[(*nops)++] = (Dwarf_Op) { .atom = DW_OP_call_frame_cfa };
      if (reg->value != 0)
	ops_mem[(*nops)++] = (Dwarf_Op) { .atom = DW_OP_plus_uconst,
					  .number = reg->value };
      if (reg->rule == reg_val_offset)
	/* A value, not a location.  */
	ops_mem[(*nops)++] = (Dwarf_Op) { .atom = DW_OP_stack_value };
      *ops = ops_mem;
      break;

    case reg_register:
      ops_mem[(*nops)++] = (Dwarf_Op) { .atom = DW_OP_regx,
					.number = reg->value };
      break;

    case reg_val_expression:
    case reg_expression:
      {
	unsigned int address_size = (fs->cache->e_ident[EI_CLASS] == ELFCLASS32
				     ? 4 : 8);

	Dwarf_Block block;
	const uint8_t *p = fs->cache->data->d.d_buf + reg->value;
	const uint8_t *end = (fs->cache->data->d.d_buf
			      + fs->cache->data->d.d_size);
	if (p >= end)
	  {
	    __libdw_seterrno (DWARF_E_INVALID_DWARF);
	    return -1;
	  }
	get_uleb128 (block.length, p, end);
	block.data = (void *) p;

	/* Parse the expression into internal form.  */
	if (__libdw_intern_expression (NULL,
				       fs->cache->other_byte_order,
				       address_size, 4,
				       &fs->cache->expr_tree, &block,
				       true, reg->rule == reg_val_expression,
				       ops, nops, IDX_debug_frame) < 0)
	  return -1;
	break;
      }
    }

  return 0;
}
