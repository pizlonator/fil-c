/* Return location expression list.
   Copyright (C) 2000-2010, 2013-2015, 2017, 2018 Red Hat, Inc.
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
#include <search.h>
#include <stdlib.h>
#include <assert.h>

#include <libdwP.h>


static bool
attr_ok (Dwarf_Attribute *attr)
{
  if (attr == NULL)
    return false;

  /* If it is an exprloc, it is obviously OK.  */
  if (dwarf_whatform (attr) == DW_FORM_exprloc)
    return true;

  if (attr->cu->version >= 4)
    {
      /* Must be an exprloc (or constant), just not any block form.  */
      switch (dwarf_whatform (attr))
	{
	case DW_FORM_block:
	case DW_FORM_block1:
	case DW_FORM_block2:
	case DW_FORM_block4:
	  __libdw_seterrno (DWARF_E_NO_LOC_VALUE);
	  return false;
	default:
	  break;
	}
    }

  /* Otherwise must be one of the attributes listed below.  Older
     DWARF versions might have encoded the exprloc as block, and we
     cannot easily distinguish attributes in the loclist class because
     the same forms are used for different classes.  */
  switch (attr->code)
    {
    case DW_AT_location:
    case DW_AT_byte_size:
    case DW_AT_bit_offset:
    case DW_AT_bit_size:
    case DW_AT_lower_bound:
    case DW_AT_bit_stride:
    case DW_AT_upper_bound:
    case DW_AT_count:
    case DW_AT_allocated:
    case DW_AT_associated:
    case DW_AT_data_location:
    case DW_AT_byte_stride:
    case DW_AT_rank:
    case DW_AT_call_value:
    case DW_AT_call_target:
    case DW_AT_call_target_clobbered:
    case DW_AT_call_data_location:
    case DW_AT_call_data_value:
    case DW_AT_data_member_location:
    case DW_AT_vtable_elem_location:
    case DW_AT_string_length:
    case DW_AT_use_location:
    case DW_AT_frame_base:
    case DW_AT_return_addr:
    case DW_AT_static_link:
    case DW_AT_segment:
    case DW_AT_GNU_call_site_value:
    case DW_AT_GNU_call_site_data_value:
    case DW_AT_GNU_call_site_target:
    case DW_AT_GNU_call_site_target_clobbered:
      break;

    default:
      __libdw_seterrno (DWARF_E_NO_LOC_VALUE);
      return false;
    }

  return true;
}


struct loclist
{
  uint8_t atom;
  Dwarf_Word number;
  Dwarf_Word number2;
  Dwarf_Word offset;
  struct loclist *next;
};


static int
loc_compare (const void *p1, const void *p2)
{
  const struct loc_s *l1 = (const struct loc_s *) p1;
  const struct loc_s *l2 = (const struct loc_s *) p2;

  if ((uintptr_t) l1->addr < (uintptr_t) l2->addr)
    return -1;
  if ((uintptr_t) l1->addr > (uintptr_t) l2->addr)
    return 1;

  return 0;
}

/* For each DW_OP_implicit_value, we store a special entry in the cache.
   This points us directly to the block data for later fetching.
   Returns zero on success, -1 on bad DWARF or 1 if tsearch failed.  */
static int
store_implicit_value (Dwarf *dbg, void **cache, Dwarf_Op *op)
{
  if (dbg == NULL)
    return -1;
  struct loc_block_s *block = libdw_alloc (dbg, struct loc_block_s,
					   sizeof (struct loc_block_s), 1);
  const unsigned char *data = (const unsigned char *) (uintptr_t) op->number2;
  /* Skip the block length.  */
  Dwarf_Word length;
  get_uleb128_unchecked (length, data);
  if (length != op->number)
    return -1;
  block->addr = op;
  block->data = (unsigned char *) data;
  block->length = op->number;
  if (unlikely (tsearch (block, cache, loc_compare) == NULL))
    return 1;
  return 0;
}

int
dwarf_getlocation_implicit_value (Dwarf_Attribute *attr, const Dwarf_Op *op,
				  Dwarf_Block *return_block)
{
  if (attr == NULL)
    return -1;

  struct loc_block_s fake = { .addr = (void *) op };
  struct loc_block_s **found = tfind (&fake, &attr->cu->locs, loc_compare);
  if (unlikely (found == NULL))
    {
      __libdw_seterrno (DWARF_E_NO_BLOCK);
      return -1;
    }

  return_block->length = (*found)->length;
  return_block->data = (*found)->data;
  return 0;
}

/* If the given attribute is DW_AT_data_member_location and it has constant
   form then create a fake location using DW_OP_plus_uconst and the offset
   value.  On success returns zero and fills in llbuf (when not NULL) and
   sets listlen to 1.  Returns 1 when this isn't a DW_AT_data_member_location
   offset.  Returns -1 and sets dwarf_errno on failure (bad DWARF data).  */
static int
is_constant_offset (Dwarf_Attribute *attr,
		    Dwarf_Op **llbuf, size_t *listlen)
{
  if (attr->code != DW_AT_data_member_location)
    return 1;

  switch (attr->form)
    {
      /* Punt for any non-constant form.  */
    default:
      return 1;

      /* Note, we don't regard DW_FORM_data16 as a constant form,
	 even though technically it is according to the standard.  */
    case DW_FORM_data1:
    case DW_FORM_data2:
    case DW_FORM_data4:
    case DW_FORM_data8:
    case DW_FORM_sdata:
    case DW_FORM_udata:
    case DW_FORM_implicit_const:
      break;
    }

  /* Check whether we already cached this location.  */
  struct loc_s fake = { .addr = attr->valp };
  struct loc_s **found = tfind (&fake, &attr->cu->locs, loc_compare);

  if (found == NULL)
    {
      Dwarf_Word offset;
      if (INTUSE(dwarf_formudata) (attr, &offset) != 0)
	return -1;

      Dwarf_Op *result = libdw_alloc (attr->cu->dbg,
				      Dwarf_Op, sizeof (Dwarf_Op), 1);

      result->atom = DW_OP_plus_uconst;
      result->number = offset;
      result->number2 = 0;
      result->offset = 0;

      /* Insert a record in the search tree so we can find it again later.  */
      struct loc_s *newp = libdw_alloc (attr->cu->dbg,
					struct loc_s, sizeof (struct loc_s),
					1);
      newp->addr = attr->valp;
      newp->loc = result;
      newp->nloc = 1;

      found = tsearch (newp, &attr->cu->locs, loc_compare);
    }

  assert ((*found)->nloc == 1);

  if (llbuf != NULL)
    {
      *llbuf = (*found)->loc;
      *listlen = 1;
    }

  return 0;
}

int
internal_function
__libdw_intern_expression (Dwarf *dbg, bool other_byte_order,
			   unsigned int address_size, unsigned int ref_size,
			   void **cache, const Dwarf_Block *block,
			   bool cfap, bool valuep,
			   Dwarf_Op **llbuf, size_t *listlen, int sec_index)
{
  /* Empty location expressions don't have any ops to intern.  */
  if (block->length == 0)
    {
      *listlen = 0;
      return 0;
    }

  /* Check whether we already looked at this list.  */
  struct loc_s fake = { .addr = block->data };
  struct loc_s **found = tfind (&fake, cache, loc_compare);
  if (found != NULL)
    {
      /* We already saw it.  */
      *llbuf = (*found)->loc;
      *listlen = (*found)->nloc;

      if (valuep)
	{
	  assert (*listlen > 1);
	  assert ((*llbuf)[*listlen - 1].atom == DW_OP_stack_value);
	}

      return 0;
    }

  const unsigned char *data = block->data;
  const unsigned char *const end_data = data + block->length;

  const struct { bool other_byte_order; } bo = { other_byte_order };

  struct loclist *loclist = NULL;
  unsigned int n = 0;

  /* Stack allocate at most this many locs.  */
#define MAX_STACK_LOCS 256
  struct loclist stack_locs[MAX_STACK_LOCS];
#define NEW_LOC() ({ struct loclist *ll;			\
		     ll = (likely (n < MAX_STACK_LOCS)		\
			   ? &stack_locs[n]			\
			   : malloc (sizeof (struct loclist)));	\
		     if (unlikely (ll == NULL))			\
		       goto nomem;				\
		     n++;					\
		     ll->next = loclist;			\
		     loclist = ll;				\
		     ll; })

  if (cfap)
    {
      /* Synthesize the operation to push the CFA before the expression.  */
      struct loclist *newloc = NEW_LOC ();
      newloc->atom = DW_OP_call_frame_cfa;
      newloc->number = 0;
      newloc->number2 = 0;
      newloc->offset = -1;
    }

  /* Decode the opcodes.  It is possible in some situations to have a
     block of size zero.  */
  while (data < end_data)
    {
      struct loclist *newloc;
      newloc = NEW_LOC ();
      newloc->number = 0;
      newloc->number2 = 0;
      newloc->offset = data - block->data;

      switch ((newloc->atom = *data++))
	{
	case DW_OP_addr:
	  /* Address, depends on address size of CU.  */
	  if (dbg == NULL)
	    {
	      // XXX relocation?
	      if (address_size == 4)
		{
		  if (unlikely (data + 4 > end_data))
		    goto invalid;
		  else
		    newloc->number = read_4ubyte_unaligned_inc (&bo, data);
		}
	      else
		{
		  if (unlikely (data + 8 > end_data))
		    goto invalid;
		  else
		    newloc->number = read_8ubyte_unaligned_inc (&bo, data);
		}
	    }
	  else if (__libdw_read_address_inc (dbg, sec_index, &data,
					     address_size, &newloc->number))
	    goto invalid;
	  break;

	case DW_OP_call_ref:
	case DW_OP_GNU_variable_value:
	  /* DW_FORM_ref_addr, depends on offset size of CU.  */
	  if (dbg == NULL || __libdw_read_offset_inc (dbg, sec_index, &data,
						      ref_size,
						      &newloc->number,
						      IDX_debug_info, 0))
	    goto invalid;
	  break;

	case DW_OP_deref:
	case DW_OP_dup:
	case DW_OP_drop:
	case DW_OP_over:
	case DW_OP_swap:
	case DW_OP_rot:
	case DW_OP_xderef:
	case DW_OP_abs:
	case DW_OP_and:
	case DW_OP_div:
	case DW_OP_minus:
	case DW_OP_mod:
	case DW_OP_mul:
	case DW_OP_neg:
	case DW_OP_not:
	case DW_OP_or:
	case DW_OP_plus:
	case DW_OP_shl:
	case DW_OP_shr:
	case DW_OP_shra:
	case DW_OP_xor:
	case DW_OP_eq:
	case DW_OP_ge:
	case DW_OP_gt:
	case DW_OP_le:
	case DW_OP_lt:
	case DW_OP_ne:
	case DW_OP_lit0 ... DW_OP_lit31:
	case DW_OP_reg0 ... DW_OP_reg31:
	case DW_OP_nop:
	case DW_OP_push_object_address:
	case DW_OP_call_frame_cfa:
	case DW_OP_form_tls_address:
	case DW_OP_GNU_push_tls_address:
	case DW_OP_stack_value:
	case DW_OP_GNU_uninit:
	  /* No operand.  */
	  break;

	case DW_OP_const1u:
	case DW_OP_pick:
	case DW_OP_deref_size:
	case DW_OP_xderef_size:
	  if (unlikely (data >= end_data))
	    {
	    invalid:
	      __libdw_seterrno (DWARF_E_INVALID_DWARF);
	    returnmem:
	      /* Free any dynamically allocated loclists, if any.  */
	      while (n > MAX_STACK_LOCS)
		{
		  struct loclist *loc = loclist;
		  loclist = loc->next;
		  free (loc);
		  n--;
		}
	      return -1;
	    }

	  newloc->number = *data++;
	  break;

	case DW_OP_const1s:
	  if (unlikely (data >= end_data))
	    goto invalid;

	  newloc->number = *((int8_t *) data);
	  ++data;
	  break;

	case DW_OP_const2u:
	  if (unlikely (data + 2 > end_data))
	    goto invalid;

	  newloc->number = read_2ubyte_unaligned_inc (&bo, data);
	  break;

	case DW_OP_const2s:
	case DW_OP_skip:
	case DW_OP_bra:
	case DW_OP_call2:
	  if (unlikely (data + 2 > end_data))
	    goto invalid;

	  newloc->number = read_2sbyte_unaligned_inc (&bo, data);
	  break;

	case DW_OP_const4u:
	  if (unlikely (data + 4 > end_data))
	    goto invalid;

	  newloc->number = read_4ubyte_unaligned_inc (&bo, data);
	  break;

	case DW_OP_const4s:
	case DW_OP_call4:
	case DW_OP_GNU_parameter_ref:
	  if (unlikely (data + 4 > end_data))
	    goto invalid;

	  newloc->number = read_4sbyte_unaligned_inc (&bo, data);
	  break;

	case DW_OP_const8u:
	  if (unlikely (data + 8 > end_data))
	    goto invalid;

	  newloc->number = read_8ubyte_unaligned_inc (&bo, data);
	  break;

	case DW_OP_const8s:
	  if (unlikely (data + 8 > end_data))
	    goto invalid;

	  newloc->number = read_8sbyte_unaligned_inc (&bo, data);
	  break;

	case DW_OP_constu:
	case DW_OP_plus_uconst:
	case DW_OP_regx:
	case DW_OP_piece:
	case DW_OP_convert:
	case DW_OP_GNU_convert:
	case DW_OP_reinterpret:
	case DW_OP_GNU_reinterpret:
	case DW_OP_addrx:
	case DW_OP_GNU_addr_index:
	case DW_OP_constx:
	case DW_OP_GNU_const_index:
	  get_uleb128 (newloc->number, data, end_data);
	  break;

	case DW_OP_consts:
	case DW_OP_breg0 ... DW_OP_breg31:
	case DW_OP_fbreg:
	  get_sleb128 (newloc->number, data, end_data);
	  break;

	case DW_OP_bregx:
	  get_uleb128 (newloc->number, data, end_data);
	  if (unlikely (data >= end_data))
	    goto invalid;
	  get_sleb128 (newloc->number2, data, end_data);
	  break;

	case DW_OP_bit_piece:
	case DW_OP_regval_type:
	case DW_OP_GNU_regval_type:
	  get_uleb128 (newloc->number, data, end_data);
	  if (unlikely (data >= end_data))
	    goto invalid;
	  get_uleb128 (newloc->number2, data, end_data);
	  break;

	case DW_OP_implicit_value:
	case DW_OP_entry_value:
	case DW_OP_GNU_entry_value:
	  /* This cannot be used in a CFI expression.  */
	  if (unlikely (dbg == NULL))
	    goto invalid;

	  /* start of block inc. len.  */
	  newloc->number2 = (Dwarf_Word) (uintptr_t) data;
	  get_uleb128 (newloc->number, data, end_data); /* Block length.  */
	  if (unlikely ((Dwarf_Word) (end_data - data) < newloc->number))
	    goto invalid;
	  data += newloc->number;		/* Skip the block.  */
	  break;

	case DW_OP_implicit_pointer:
	case DW_OP_GNU_implicit_pointer:
	  /* DW_FORM_ref_addr, depends on offset size of CU.  */
	  if (dbg == NULL || __libdw_read_offset_inc (dbg, sec_index, &data,
						      ref_size,
						      &newloc->number,
						      IDX_debug_info, 0))
	    goto invalid;
	  if (unlikely (data >= end_data))
	    goto invalid;
	  get_uleb128 (newloc->number2, data, end_data); /* Byte offset.  */
	  break;

	case DW_OP_deref_type:
	case DW_OP_GNU_deref_type:
	case DW_OP_xderef_type:
	  if (unlikely (data + 1 >= end_data))
	    goto invalid;
	  newloc->number = *data++;
	  get_uleb128 (newloc->number2, data, end_data);
	  break;

	case DW_OP_const_type:
	case DW_OP_GNU_const_type:
	  {
	    size_t size;
	    get_uleb128 (newloc->number, data, end_data);
	    if (unlikely (data >= end_data))
	      goto invalid;

	    /* start of block inc. len.  */
	    newloc->number2 = (Dwarf_Word) (uintptr_t) data;
	    size = *data++;
	    if (unlikely ((Dwarf_Word) (end_data - data) < size))
	      goto invalid;
	    data += size;		/* Skip the block.  */
	  }
	  break;

	default:
	  goto invalid;
	}
    }

  if (unlikely (n == 0))
    {
      /* This is not allowed.
	 It would mean an empty location expression, which we handled
	 already as a special case above.  */
      goto invalid;
    }

  if (valuep)
    {
      struct loclist *newloc = NEW_LOC ();
      newloc->atom = DW_OP_stack_value;
      newloc->number = 0;
      newloc->number2 = 0;
      newloc->offset = data - block->data;
    }

  /* Allocate the array.  */
  Dwarf_Op *result;
  if (dbg != NULL)
    result = libdw_alloc (dbg, Dwarf_Op, sizeof (Dwarf_Op), n);
  else
    {
      result = malloc (sizeof *result * n);
      if (result == NULL)
	{
	nomem:
	  __libdw_seterrno (DWARF_E_NOMEM);
	  goto returnmem;
	}
    }

  /* Store the result.  */
  *llbuf = result;
  *listlen = n;

  do
    {
      /* We populate the array from the back since the list is backwards.  */
      --n;
      result[n].atom = loclist->atom;
      result[n].number = loclist->number;
      result[n].number2 = loclist->number2;
      result[n].offset = loclist->offset;

      if (result[n].atom == DW_OP_implicit_value)
	{
	  int store = store_implicit_value (dbg, cache, &result[n]);
	  if (unlikely (store != 0))
	    {
	      if (store < 0)
	        goto invalid;
	      else
		goto nomem;
	    }
	}

      struct loclist *loc = loclist;
      loclist = loclist->next;
      if (unlikely (n + 1 > MAX_STACK_LOCS))
	free (loc);
    }
  while (n > 0);

  /* Insert a record in the search tree so that we can find it again later.  */
  struct loc_s *newp;
  if (dbg != NULL)
    newp = libdw_alloc (dbg, struct loc_s, sizeof (struct loc_s), 1);
  else
    {
      newp = malloc (sizeof *newp);
      if (newp == NULL)
	{
	  free (result);
	  goto nomem;
	}
    }

  newp->addr = block->data;
  newp->loc = result;
  newp->nloc = *listlen;
  (void) tsearch (newp, cache, loc_compare);

  /* We did it.  */
  return 0;
}

static int
getlocation (struct Dwarf_CU *cu, const Dwarf_Block *block,
	     Dwarf_Op **llbuf, size_t *listlen, int sec_index)
{
  /* Empty location expressions don't have any ops to intern.
     Note that synthetic empty_cu doesn't have an associated DWARF dbg.  */
  if (block->length == 0)
    {
      *listlen = 0;
      return 0;
    }

  return __libdw_intern_expression (cu->dbg, cu->dbg->other_byte_order,
				    cu->address_size, (cu->version == 2
						       ? cu->address_size
						       : cu->offset_size),
				    &cu->locs, block,
				    false, false,
				    llbuf, listlen, sec_index);
}

int
dwarf_getlocation (Dwarf_Attribute *attr, Dwarf_Op **llbuf, size_t *listlen)
{
  if (! attr_ok (attr))
    return -1;

  int result = is_constant_offset (attr, llbuf, listlen);
  if (result != 1)
    return result; /* Either success 0, or -1 to indicate error.  */

  /* If it has a block form, it's a single location expression.
     Except for DW_FORM_data16, which is a 128bit constant.  */
  if (attr->form == DW_FORM_data16)
    {
      __libdw_seterrno (DWARF_E_NO_BLOCK);
      return -1;
    }
  Dwarf_Block block;
  if (INTUSE(dwarf_formblock) (attr, &block) != 0)
    return -1;

  return getlocation (attr->cu, &block, llbuf, listlen, cu_sec_idx (attr->cu));
}

Dwarf_Addr
__libdw_cu_base_address (Dwarf_CU *cu)
{
  if (cu->base_address == (Dwarf_Addr) -1)
    {
      Dwarf_Addr base;

      /* Fetch the CU's base address.  */
      Dwarf_Die cudie = CUDIE (cu);

      /* Find the base address of the compilation unit.  It will
	 normally be specified by DW_AT_low_pc.  In DWARF-3 draft 4,
	 the base address could be overridden by DW_AT_entry_pc.  It's
	 been removed, but GCC emits DW_AT_entry_pc and not DW_AT_lowpc
	 for compilation units with discontinuous ranges.  */
      Dwarf_Attribute attr_mem;
      if (INTUSE(dwarf_lowpc) (&cudie, &base) != 0
	  && INTUSE(dwarf_formaddr) (INTUSE(dwarf_attr) (&cudie,
							 DW_AT_entry_pc,
							 &attr_mem),
				     &base) != 0)
	{
	  /* The compiler provided no base address when it should
	     have.  Buggy GCC does this when it used absolute
	     addresses in the location list and no DW_AT_ranges.  */
	   base = 0;
	}
      cu->base_address = base;
    }

  return cu->base_address;
}

static int
initial_offset (Dwarf_Attribute *attr, ptrdiff_t *offset)
{
  size_t secidx = (attr->cu->version < 5
		   ? IDX_debug_loc : IDX_debug_loclists);

  Dwarf_Word start_offset;
  if (attr->form == DW_FORM_loclistx)
    {
      Dwarf_Word idx;
      Dwarf_CU *cu = attr->cu;
      const unsigned char *datap = attr->valp;
      const unsigned char *endp = cu->endp;
      if (datap >= endp)
	{
	  __libdw_seterrno (DWARF_E_INVALID_DWARF);
	  return -1;
	}
      get_uleb128 (idx, datap, endp);

      Elf_Data *data = cu->dbg->sectiondata[secidx];
      if (data == NULL && cu->unit_type == DW_UT_split_compile)
	{
	  cu = __libdw_find_split_unit (cu);
	  if (cu != NULL)
	    data = cu->dbg->sectiondata[secidx];
	}

      if (data == NULL)
	{
	  __libdw_seterrno (secidx == IDX_debug_loc
                            ? DWARF_E_NO_DEBUG_LOC
                            : DWARF_E_NO_DEBUG_LOCLISTS);
	  return -1;
	}

      Dwarf_Off loc_base_off = __libdw_cu_locs_base (cu);

      /* The section should at least contain room for one offset.  */
      size_t sec_size = cu->dbg->sectiondata[secidx]->d_size;
      size_t offset_size = cu->offset_size;
      if (offset_size > sec_size)
	{
	invalid_offset:
	  __libdw_seterrno (DWARF_E_INVALID_OFFSET);
	  return -1;
	}

      /* And the base offset should be at least inside the section.  */
      if (loc_base_off > (sec_size - offset_size))
	goto invalid_offset;

      size_t max_idx = (sec_size - offset_size - loc_base_off) / offset_size;
      if (idx > max_idx)
	goto invalid_offset;

      datap = (cu->dbg->sectiondata[secidx]->d_buf
	       + loc_base_off + (idx * offset_size));
      if (offset_size == 4)
	start_offset = read_4ubyte_unaligned (cu->dbg, datap);
      else
	start_offset = read_8ubyte_unaligned (cu->dbg, datap);

      start_offset += loc_base_off;
    }
  else
    {
      if (__libdw_formptr (attr, secidx,
			   (secidx == IDX_debug_loc
			    ? DWARF_E_NO_DEBUG_LOC
			    : DWARF_E_NO_DEBUG_LOCLISTS),
			    NULL, &start_offset) == NULL)
	return -1;

      Dwarf_Off loc_off;
      if (INTUSE(dwarf_cu_dwp_section_info) (attr->cu, DW_SECT_LOCLISTS,
					     &loc_off, NULL) != 0)
	return -1;
      start_offset += loc_off;
    }

  *offset = start_offset;
  return 0;
}

static ptrdiff_t
getlocations_addr (Dwarf_Attribute *attr, ptrdiff_t offset,
		   Dwarf_Addr *basep, Dwarf_Addr *startp, Dwarf_Addr *endp,
		   Dwarf_Addr address, const Elf_Data *locs, Dwarf_Op **expr,
		   size_t *exprlen)
{
  Dwarf_CU *cu = attr->cu;
  Dwarf *dbg = cu->dbg;
  size_t secidx = cu->version < 5 ? IDX_debug_loc : IDX_debug_loclists;
  const unsigned char *readp = locs->d_buf + offset;
  const unsigned char *readendp = locs->d_buf + locs->d_size;

  Dwarf_Addr begin;
  Dwarf_Addr end;

 next:
  switch (__libdw_read_begin_end_pair_inc (cu, secidx,
					   &readp, readendp,
					   cu->address_size,
					   &begin, &end, basep))
    {
    case 0: /* got location range. */
      break;
    case 1: /* base address setup. */
      goto next;
    case 2: /* end of loclist */
      return 0;
    default: /* error */
      return -1;
    }

  /* We have a location expression.  */
  Dwarf_Block block;
  if (secidx == IDX_debug_loc)
    {
      if (readendp - readp < 2)
	{
	invalid:
	  __libdw_seterrno (DWARF_E_INVALID_DWARF);
	  return -1;
	}
      block.length = read_2ubyte_unaligned_inc (dbg, readp);
    }
  else
    {
      if (readendp - readp < 1)
	goto invalid;
      get_uleb128 (block.length, readp, readendp);
    }
  block.data = (unsigned char *) readp;
  if (readendp - readp < (ptrdiff_t) block.length)
    goto invalid;
  readp += block.length;

  /* Note these addresses include any base (if necessary) already.  */
  *startp = begin;
  *endp = end;

  /* If address is minus one we want them all, otherwise only matching.  */
  if (address != (Dwarf_Word) -1 && (address < *startp || address >= *endp))
    goto next;

  if (getlocation (cu, &block, expr, exprlen, secidx) != 0)
    return -1;

  return readp - (unsigned char *) locs->d_buf;
}

int
dwarf_getlocation_addr (Dwarf_Attribute *attr, Dwarf_Addr address,
			Dwarf_Op **llbufs, size_t *listlens, size_t maxlocs)
{
  if (! attr_ok (attr))
    return -1;

  if (llbufs == NULL)
    maxlocs = SIZE_MAX;

  /* If it has a block form, it's a single location expression.
     Except for DW_FORM_data16, which is a 128bit constant.  */
  Dwarf_Block block;
  if (attr->form != DW_FORM_data16
      && INTUSE(dwarf_formblock) (attr, &block) == 0)
    {
      if (maxlocs == 0)
	return 0;
      if (llbufs != NULL &&
	  getlocation (attr->cu, &block, &llbufs[0], &listlens[0],
		       cu_sec_idx (attr->cu)) != 0)
	return -1;
      return listlens[0] == 0 ? 0 : 1;
    }

  if (attr->form != DW_FORM_data16)
    {
      int error = INTUSE(dwarf_errno) ();
      if (unlikely (error != DWARF_E_NO_BLOCK))
	{
	  __libdw_seterrno (error);
	  return -1;
	}
    }

  /* If is_constant_offset is successful, we are done with 1 result.  */
  int result = is_constant_offset (attr, llbufs, listlens);
  if (result != 1)
    return result ?: 1;

  Dwarf_Addr base, start, end;
  Dwarf_Op *expr;
  size_t expr_len;
  ptrdiff_t off = 0;
  size_t got = 0;

  /* This is a true loclistptr, fetch the initial base address and offset.  */
  base = __libdw_cu_base_address (attr->cu);
  if (base == (Dwarf_Addr) -1)
    return -1;

  if (initial_offset (attr, &off) != 0)
    return -1;

  size_t secidx = attr->cu->version < 5 ? IDX_debug_loc : IDX_debug_loclists;
  const Elf_Data *d = attr->cu->dbg->sectiondata[secidx];

  while (got < maxlocs
         && (off = getlocations_addr (attr, off, &base, &start, &end,
				      address, d, &expr, &expr_len)) > 0)
    {
      /* This one matches the address.  */
      if (llbufs != NULL)
	{
	  llbufs[got] = expr;
	  listlens[got] = expr_len;
	}
      ++got;
    }

  /* We might stop early, so off can be zero or positive on success.  */
  if (off < 0)
    return -1;

  return got;
}

ptrdiff_t
dwarf_getlocations (Dwarf_Attribute *attr, ptrdiff_t offset, Dwarf_Addr *basep,
		    Dwarf_Addr *startp, Dwarf_Addr *endp, Dwarf_Op **expr,
		    size_t *exprlen)
{
  if (! attr_ok (attr))
    return -1;

  /* 1 is an invalid offset, meaning no more locations. */
  if (offset == 1)
    return 0;

  if (offset == 0)
    {
      /* If it has a block form, it's a single location expression.
	 Except for DW_FORM_data16, which is a 128bit constant.  */
      Dwarf_Block block;
      if (attr->form != DW_FORM_data16
	  && INTUSE(dwarf_formblock) (attr, &block) == 0)
	{
	  if (getlocation (attr->cu, &block, expr, exprlen,
			   cu_sec_idx (attr->cu)) != 0)
	    return -1;

	  /* This is the one and only location covering everything. */
	  *startp = 0;
	  *endp = -1;
	  return 1;
	}

      if (attr->form != DW_FORM_data16)
	{
	  int error = INTUSE(dwarf_errno) ();
	  if (unlikely (error != DWARF_E_NO_BLOCK))
	    {
	      __libdw_seterrno (error);
	      return -1;
	    }
	}

      int result = is_constant_offset (attr, expr, exprlen);
      if (result != 1)
	{
	  if (result == 0)
	    {
	      /* This is the one and only location covering everything. */
	      *startp = 0;
	      *endp = -1;
	      return 1;
	    }
	  return result; /* Something bad, dwarf_errno has been set.  */
	}

      /* We must be looking at a true loclistptr, fetch the initial
	 base address and offset.  */
      *basep = __libdw_cu_base_address (attr->cu);
      if (*basep == (Dwarf_Addr) -1)
	return -1;

      if (initial_offset (attr, &offset) != 0)
	return -1;
    }

  size_t secidx = attr->cu->version < 5 ? IDX_debug_loc : IDX_debug_loclists;
  const Elf_Data *d = attr->cu->dbg->sectiondata[secidx];

  return getlocations_addr (attr, offset, basep, startp, endp,
			    (Dwarf_Word) -1, d, expr, exprlen);
}
