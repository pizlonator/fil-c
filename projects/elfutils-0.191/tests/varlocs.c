/* Test program for dwarf location functions.
   Copyright (C) 2013, 2015, 2017, 2018 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <config.h>
#include <assert.h>
#include <argp.h>
#include <inttypes.h>
#include <errno.h>
#include ELFUTILS_HEADER(dw)
#include ELFUTILS_HEADER(dwfl)
#include <dwarf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "system.h"
#include "../libdw/known-dwarf.h"

// The Dwarf, Dwarf_CFIs and address bias of
// cfi table to adjust DWARF addresses against.
// Needed for DW_OP_call_frame_cfa.
static Dwarf *dw;
Dwarf_CFI *cfi_debug;
Dwarf_Addr cfi_debug_bias;
Dwarf_CFI *cfi_eh;
Dwarf_Addr cfi_eh_bias;

bool is_ET_REL;
bool is_debug;

// Whether the current function has a DW_AT_frame_base defined.
// Needed for DW_OP_fbreg.
bool has_frame_base;

static void
print_die (Dwarf_Die *die, const char *what, int indent)
{
  Dwarf_Addr entrypc;
  const char *name = dwarf_diename (die) ?: "<unknown>";
  if (dwarf_entrypc (die, &entrypc) == 0)
    printf ("%*s[%" PRIx64 "] %s '%s'@%" PRIx64 "\n", indent * 2, "",
	    dwarf_dieoffset (die), what, name, entrypc);
  else
    printf ("%*s[%" PRIx64 "] %s '%s'\n", indent * 2, "",
	    dwarf_dieoffset (die), what, name);
}

static const char *
dwarf_encoding_string (unsigned int code)
{
  static const char *const known[] =
    {
#define DWARF_ONE_KNOWN_DW_ATE(NAME, CODE) [CODE] = #NAME,
      DWARF_ALL_KNOWN_DW_ATE
#undef DWARF_ONE_KNOWN_DW_ATE
    };

  if (likely (code < sizeof (known) / sizeof (known[0])))
    return known[code];

  return "<unknown encoding>";
}

static const char *
dwarf_tag_string (unsigned int tag)
{
  switch (tag)
    {
#define DWARF_ONE_KNOWN_DW_TAG(NAME, CODE) case CODE: return #NAME;
      DWARF_ALL_KNOWN_DW_TAG
#undef DWARF_ONE_KNOWN_DW_TAG
    default:
      return "<unknown tag>";
    }
}

static const char *
dwarf_attr_string (unsigned int attrnum)
{
  switch (attrnum)
    {
#define DWARF_ONE_KNOWN_DW_AT(NAME, CODE) case CODE: return #NAME;
      DWARF_ALL_KNOWN_DW_AT
#undef DWARF_ONE_KNOWN_DW_AT
    default:
      return "<unknown attr>";
    }
}

static const char *
dwarf_form_string (unsigned int form)
{
  switch (form)
    {
#define DWARF_ONE_KNOWN_DW_FORM(NAME, CODE) case CODE: return #NAME;
      DWARF_ALL_KNOWN_DW_FORM
#undef DWARF_ONE_KNOWN_DW_FORM
    default:
      return "<unknown form>";
    }
}

/* BASE must be a base type DIE referenced by a typed DWARF expression op.  */
static void
print_base_type (Dwarf_Die *base)
{
  if (dwarf_tag (base) != DW_TAG_base_type)
    error (EXIT_FAILURE, 0, "not a base type");

  Dwarf_Attribute encoding;
  Dwarf_Word enctype = 0;
  if (dwarf_attr (base, DW_AT_encoding, &encoding) == NULL
      || dwarf_formudata (&encoding, &enctype) != 0)
    error (EXIT_FAILURE, 0, "base type without encoding");

  Dwarf_Attribute bsize;
  Dwarf_Word bits;
  if (dwarf_attr (base, DW_AT_byte_size, &bsize) != NULL
      && dwarf_formudata (&bsize, &bits) == 0)
    bits *= 8;
  else if (dwarf_attr (base, DW_AT_bit_size, &bsize) == NULL
	   || dwarf_formudata (&bsize, &bits) != 0)
    error (EXIT_FAILURE, 0, "base type without byte or bit size");

  printf ("{%s,%s,%" PRIu64 "@[%" PRIx64 "]}",
	  dwarf_diename (base),
	  dwarf_encoding_string (enctype),
	  bits,
	  dwarf_dieoffset (base));
}

static const char *
dwarf_opcode_string (unsigned int code)
{
  static const char *const known[] =
    {
#define DWARF_ONE_KNOWN_DW_OP(NAME, CODE) [CODE] = #NAME,
      DWARF_ALL_KNOWN_DW_OP
#undef DWARF_ONE_KNOWN_DW_OP
    };

  if (likely (code < sizeof (known) / sizeof (known[0])))
    return known[code];

  return "<unknown opcode>";
}

// Forward reference for print_expr_block.
static void print_expr (Dwarf_Attribute *, Dwarf_Op *, Dwarf_Addr, int);

static void
print_expr_block (Dwarf_Attribute *attr, Dwarf_Op *exprs, int len,
		  Dwarf_Addr addr, int depth)
{
  printf ("{");
  for (int i = 0; i < len; i++)
    {
      print_expr (attr, &exprs[i], addr, depth);
      printf ("%s", (i + 1 < len ? ", " : ""));
    }
  printf ("}");
}

static void
print_expr_block_addrs (Dwarf_Attribute *attr,
			Dwarf_Addr begin, Dwarf_Addr end,
			Dwarf_Op *exprs, int len)
{
  printf ("      [%" PRIx64 ",%" PRIx64 ") ", begin, end);
  print_expr_block (attr, exprs, len, begin, 0);
  printf ("\n");
}

static void
print_expr (Dwarf_Attribute *attr, Dwarf_Op *expr, Dwarf_Addr addr, int depth)
{
#define MAX_DEPTH 64
  if (depth++ > MAX_DEPTH)
    error (EXIT_FAILURE, 0, "print_expr recursion depth exceeded");

  uint8_t atom = expr->atom;
  const char *opname = dwarf_opcode_string (atom);

  switch (atom)
    {
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
    case DW_OP_stack_value:
      /* No arguments. */
      printf ("%s", opname);
      break;

    case DW_OP_form_tls_address:
      /* No arguments. Special. Pops an address and pushes the
	 corresponding address in the current thread local
	 storage. Uses the thread local storage block of the defining
	 module (executable, shared library). */
      printf ("%s", opname);
      break;

    case DW_OP_GNU_push_tls_address:
      /* No arguments. Special. Not the same as DW_OP_form_tls_address.
	 Pops an offset into the current thread local strorage and
	 pushes back the actual address. */
      printf ("%s", opname);
      break;

    case DW_OP_GNU_uninit:
      /* No arguments. Special. It means the expression describes
	 an value which hasn't been initialized (yet).  */
      printf ("%s", opname);
      break;

    case DW_OP_call_frame_cfa:
      /* No arguments. Special. Pushes Call Frame Address as computed
	 by CFI data (dwarf_cfi_addrframe will fetch that info (either from
	 the .eh_frame or .debug_frame CFI) and dwarf_frame_cfa translatesr
         the CFI instructions into a plain DWARF expression.
	 Never used in CFI itself. */

      if (attr == NULL)
	error (EXIT_FAILURE, 0, "%s used in CFI", opname);

      printf ("%s ", opname);
      if (cfi_eh == NULL && cfi_debug == NULL && !is_debug)
	error (EXIT_FAILURE, 0, "DW_OP_call_frame_cfa used but no cfi found.");

      Dwarf_Frame *frame;
      if (dwarf_cfi_addrframe (cfi_eh, addr + cfi_eh_bias, &frame) == 0
	  || dwarf_cfi_addrframe (cfi_debug, addr + cfi_debug_bias,
				  &frame) == 0)
	{
	  Dwarf_Op *cfa_ops;
	  size_t cfa_nops;
	  if (dwarf_frame_cfa (frame, &cfa_ops, &cfa_nops) != 0)
	    error (EXIT_FAILURE, 0, "dwarf_frame_cfa 0x%" PRIx64 ": %s",
		   addr, dwarf_errmsg (-1));
	  if (cfa_nops < 1)
	    error (EXIT_FAILURE, 0, "dwarf_frame_cfa no ops");
	  print_expr_block (NULL, cfa_ops, cfa_nops, 0, depth);
	  free (frame);
	}
      else if (is_ET_REL || is_debug)
	{
	  /* XXX In ET_REL files there might be an .eh_frame with relocations
	     we don't handle (e.g. X86_64_PC32). Maybe we should?  */
	  printf ("{...}");
	}
      else
	error (EXIT_FAILURE, 0, "dwarf_cfi_addrframe 0x%" PRIx64 ": %s",
	       addr, dwarf_errmsg (-1));
      break;

    case DW_OP_push_object_address:
      /* No arguments. Special. Pushes object address explicitly.
       Normally only done implicitly by DW_AT_data_member_location.
       Never used in CFI. */
      if (attr == NULL)
	error (EXIT_FAILURE, 0, "%s used in CFI", opname);
      printf ("%s", opname);
      break;

    case DW_OP_addr:
      /* 1 address argument. */
      printf ("%s(0x%" PRIx64 ")", opname, (Dwarf_Addr) expr->number);
      break;

    case DW_OP_const1u:
    case DW_OP_const2u:
    case DW_OP_const4u:
    case DW_OP_const8u:
    case DW_OP_constu:
    case DW_OP_pick:
    case DW_OP_plus_uconst:
    case DW_OP_regx:
    case DW_OP_piece:
    case DW_OP_deref_size:
    case DW_OP_xderef_size:
      /* 1 numeric unsigned argument. */
      printf ("%s(%" PRIu64 ")", opname, expr->number);
      break;

    case DW_OP_call2:
    case DW_OP_call4:
    case DW_OP_call_ref:
      /* 1 DIE offset argument for more ops in location attribute of DIE.
         Never used in CFI.  */
      {
	if (attr == NULL)
	  error (EXIT_FAILURE, 0, "%s used in CFI", opname);

	Dwarf_Attribute call_attr;
	if (dwarf_getlocation_attr (attr, expr, &call_attr) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation_attr for %s error %s",
		 opname, dwarf_errmsg (-1));

	Dwarf_Die call_die;
	if (dwarf_getlocation_die (attr, expr, &call_die) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation_die for %s error %s",
		 opname, dwarf_errmsg (-1));

	Dwarf_Op *call_ops;
	size_t call_len;
	if (dwarf_getlocation (&call_attr, &call_ops, &call_len) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation for entry: %s",
		 dwarf_errmsg (-1));

	printf ("%s([%" PRIx64 "]) ", opname, dwarf_dieoffset (&call_die));
	print_expr_block (&call_attr, call_ops, call_len, addr, depth);
      }
      break;

    case DW_OP_const1s:
    case DW_OP_const2s:
    case DW_OP_const4s:
    case DW_OP_const8s:
    case DW_OP_consts:
    case DW_OP_skip:
    case DW_OP_bra:
    case DW_OP_breg0 ... DW_OP_breg31:
      /* 1 numeric signed argument. */
      printf ("%s(%" PRId64 ")", opname, (Dwarf_Sword) expr->number);
      break;

    case DW_OP_fbreg:
      /* 1 numeric signed argument. Offset from frame base. */
      if (attr == NULL)
	  error (EXIT_FAILURE, 0, "%s used in CFI", opname);

      if (! has_frame_base)
	error (EXIT_FAILURE, 0, "DW_OP_fbreg used without a frame base");

      printf ("%s(%" PRId64 ")", opname, (Dwarf_Sword) expr->number);
      break;

    case DW_OP_bregx:
      /* 2 arguments, unsigned register number, signed offset. */
      printf ("%s(%" PRIu64 ",%" PRId64 ")", opname,
	      expr->number, (Dwarf_Sword) expr->number2);
      break;

    case DW_OP_bit_piece:
      /* 2 arguments, unsigned size, unsigned offset. */
      printf ("%s(%" PRIu64 ",%" PRIu64 ")", opname,
	      expr->number, expr->number2);
      break;

    case DW_OP_implicit_value:
      /* Special, unsigned size plus block. */
      {
	Dwarf_Attribute const_attr;
	Dwarf_Block block;
	if (dwarf_getlocation_attr (attr, expr, &const_attr) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation_attr: %s",
		 dwarf_errmsg (-1));

	if (dwarf_formblock (&const_attr, &block) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_formblock: %s",
		 dwarf_errmsg (-1));

	/* This is the "old" way. Check they result in the same.  */
	Dwarf_Block block_impl;
	if (dwarf_getlocation_implicit_value (attr, expr, &block_impl) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation_implicit_value: %s",
		 dwarf_errmsg (-1));

	assert (expr->number == block.length);
	assert (block.length == block_impl.length);
	printf ("%s(%" PRIu64 "){", opname, block.length);
	for (size_t i = 0; i < block.length; i++)
	  {
	    printf ("%02x", block.data[i]);
	    assert (block.data[i] == block_impl.data[i]);
	  }
	printf("}");
      }
      break;

    case DW_OP_implicit_pointer:
    case DW_OP_GNU_implicit_pointer:
      /* Special, DIE offset, signed offset. Referenced DIE has a
	 location or const_value attribute. */
      {
	if (attr == NULL)
	  error (EXIT_FAILURE, 0, "%s used in CFI", opname);

	Dwarf_Attribute attrval;
	if (dwarf_getlocation_implicit_pointer (attr, expr, &attrval) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation_implicit_pointer: %s",
		 dwarf_errmsg (-1));

	// Sanity check, results should be the same.
	Dwarf_Attribute attrval2;
	if (dwarf_getlocation_attr (attr, expr, &attrval2) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation_attr: %s",
		 dwarf_errmsg (-1));

	assert (dwarf_whatattr (&attrval) == dwarf_whatattr (&attrval2));
	assert (dwarf_whatform (&attrval) == dwarf_whatform (&attrval2));
	// In theory two different valp pointers could point to the same
	// value. But here we really expect them to be the equal.
	assert (attrval.valp == attrval2.valp);

	Dwarf_Die impl_die;
	if (dwarf_getlocation_die (attr, expr, &impl_die) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation_die: %s",
		 dwarf_errmsg (-1));

	printf ("%s([%" PRIx64 "],%" PRId64 ") ", opname,
		dwarf_dieoffset (&impl_die), expr->number2);

	if (dwarf_whatattr (&attrval) == DW_AT_const_value)
	  printf ("<constant value>"); // Lookup type...
	else
	  {
	    // Lookup the location description at the current address.
	    Dwarf_Op *exprval;
	    size_t exprval_len;
	    int locs = dwarf_getlocation_addr (&attrval, addr,
					       &exprval, &exprval_len, 1);
	    if (locs == 0)
	      printf ("<no location>"); // This means "optimized out".
	    else if (locs == 1)
	      print_expr_block (&attrval, exprval, exprval_len, addr, depth);
	    else
	      error (EXIT_FAILURE, 0,
		     "dwarf_getlocation_addr attrval at addr 0x%" PRIx64
		     ", locs (%d): %s", addr, locs, dwarf_errmsg (-1));
	  }
      }
      break;

    case DW_OP_GNU_variable_value:
      /* Special, DIE offset. Referenced DIE has a location or const_value
	 attribute. */
      {
	if (attr == NULL)
	  error (EXIT_FAILURE, 0, "%s used in CFI", opname);

	Dwarf_Attribute attrval;
	if (dwarf_getlocation_attr (attr, expr, &attrval) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation_attr: %s",
		 dwarf_errmsg (-1));

	Dwarf_Die impl_die;
	if (dwarf_getlocation_die (attr, expr, &impl_die) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation_die: %s",
		 dwarf_errmsg (-1));

	printf ("%s([%" PRIx64 "]) ", opname, dwarf_dieoffset (&impl_die));

	if (dwarf_whatattr (&attrval) == DW_AT_const_value)
	  printf ("<constant value>"); // Lookup type...
	else
	  {
	    // Lookup the location description at the current address.
	    Dwarf_Op *exprval;
	    size_t exprval_len;
	    int locs = dwarf_getlocation_addr (&attrval, addr,
					       &exprval, &exprval_len, 1);
	    if (locs == 0)
	      printf ("<no location>"); // This means "optimized out".
	    else if (locs == 1)
	      print_expr_block (&attrval, exprval, exprval_len, addr, depth);
	    else
	      error (EXIT_FAILURE, 0,
		     "dwarf_getlocation_addr attrval at addr 0x%" PRIx64
		     ", locs (%d): %s", addr, locs, dwarf_errmsg (-1));
	  }
      }
      break;

    case DW_OP_entry_value:
    case DW_OP_GNU_entry_value:
      /* Special, unsigned size plus expression block. All registers
	 inside the block should be interpreted as they had on
	 entering the function. dwarf_getlocation_attr will return an
	 attribute containing the block as locexpr which can be
	 retrieved with dwarf_getlocation.  */
      {
	Dwarf_Attribute entry_attr;
	if (dwarf_getlocation_attr (attr, expr, &entry_attr) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation_attr: %s",
		 dwarf_errmsg (-1));

	Dwarf_Op *entry_ops;
	size_t entry_len;
	if (dwarf_getlocation (&entry_attr, &entry_ops, &entry_len) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation for entry: %s",
		 dwarf_errmsg (-1));

	printf ("%s(%zd) ", opname, entry_len);
	print_expr_block (attr, entry_ops, entry_len, addr, depth);
      }
      break;

    case DW_OP_GNU_parameter_ref:
      /* Special, unsigned CU relative DIE offset pointing to a
	 DW_TAG_formal_parameter. The value that parameter had at the
	 call site of the current function will be put on the DWARF
	 stack. The value can be retrieved by finding the
	 DW_TAG_GNU_call_site_parameter which has as
	 DW_AT_abstract_origin the same formal parameter DIE. */
      {
	Dwarf_Die param;
	if (dwarf_getlocation_die (attr, expr, &param) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation_die: %s",
		 dwarf_errmsg (-1));
	// XXX actually lookup DW_TAG_GNU_call_site_parameter
	printf ("%s[%" PRIx64 "]", opname, dwarf_dieoffset (&param));
	assert (expr->number == dwarf_cuoffset (&param));
	if (dwarf_tag (&param) != DW_TAG_formal_parameter)
	  error (EXIT_FAILURE, 0, "Not a formal parameter");
      }
      break;

    case DW_OP_convert:
    case DW_OP_GNU_convert:
    case DW_OP_reinterpret:
    case DW_OP_GNU_reinterpret:
      /* Special, unsigned CU relative DIE offset pointing to a
	 DW_TAG_base_type. Pops a value, converts or reinterprets the
	 value to the given type. When the argument is zero the value
	 becomes untyped again. */
      {
	Dwarf_Die type;
	Dwarf_Off off = expr->number;
	if (off != 0)
	  {
	    if (dwarf_getlocation_die (attr, expr, &type) != 0)
	      error (EXIT_FAILURE, 0, "dwarf_getlocation_die: %s",
		     dwarf_errmsg (-1));
	    off = dwarf_dieoffset (&type);
	    assert (expr->number == dwarf_cuoffset (&type));
	    printf ("%s", opname);
	    print_base_type (&type);
	  }
	else
	  printf ("%s[%" PRIu64 "]", opname, off);

      }
      break;

    case DW_OP_regval_type:
    case DW_OP_GNU_regval_type:
      /* Special, unsigned register number plus unsigned CU relative
         DIE offset pointing to a DW_TAG_base_type. */
      {
	Dwarf_Die type;
	if (dwarf_getlocation_die (attr, expr, &type) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation_die: %s",
		 dwarf_errmsg (-1));
	assert (expr->number2 == dwarf_cuoffset (&type));
	// XXX check size against base_type size?
	printf ("%s(reg%" PRIu64 ")", opname, expr->number);
	print_base_type (&type);
      }
      break;

    case DW_OP_deref_type:
    case DW_OP_GNU_deref_type:
      /* Special, unsigned size plus unsigned CU relative DIE offset
	 pointing to a DW_TAG_base_type. */
      {
	Dwarf_Die type;
	if (dwarf_getlocation_die (attr, expr, &type) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation_die: %s",
		 dwarf_errmsg (-1));
	assert (expr->number2 == dwarf_cuoffset (&type));
	// XXX check size against base_type size?
	printf ("%s(%" PRIu64 ")", opname, expr->number);
	print_base_type (&type);
      }
      break;

    case DW_OP_xderef_type:
      /* Special, unsigned size plus unsigned DIE offset
	 pointing to a DW_TAG_base_type. */
      {
	Dwarf_Die type;
	if (dwarf_getlocation_die (attr, expr, &type) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation_die: %s",
		 dwarf_errmsg (-1));
	// XXX check size against base_type size?
	printf ("%s(%" PRIu64 ")", opname, expr->number);
	print_base_type (&type);
      }
      break;

    case DW_OP_const_type:
    case DW_OP_GNU_const_type:
      /* Special, unsigned CU relative DIE offset pointing to a
	 DW_TAG_base_type, an unsigned size length plus a block with
	 the constant value. */
      {
	Dwarf_Die type;
	if (dwarf_getlocation_die (attr, expr, &type) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation_die: %s",
		 dwarf_errmsg (-1));
	assert (expr->number == dwarf_cuoffset (&type));

	Dwarf_Attribute const_attr;
	if (dwarf_getlocation_attr (attr, expr, &const_attr) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation_attr for type: %s",
		 dwarf_errmsg (-1));
	  
	Dwarf_Block block;
	if (dwarf_formblock (&const_attr, &block) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_formblock for type: %s",
		 dwarf_errmsg (-1));

	printf ("%s", opname);
	print_base_type (&type);
	printf ("(%" PRIu64 ")[", block.length);
	for (size_t i = 0; i < block.length; i++)
	  printf ("%02x", block.data[i]);
	printf("]");
      }
      break;

    case DW_OP_GNU_addr_index:
    case DW_OP_addrx:
      /* Address from the .debug_addr section (indexed based on CU).  */
      {
	Dwarf_Attribute addr_attr;
	if (dwarf_getlocation_attr (attr, expr, &addr_attr) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation_attr for addr: %s",
		 dwarf_errmsg (-1));

	Dwarf_Addr address;
	if (dwarf_formaddr (&addr_attr, &address) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_formaddr address failed: %s",
		 dwarf_errmsg (-1));

	printf ("addr: 0x%" PRIx64, address);
      }
      break;

    case DW_OP_GNU_const_index:
    case DW_OP_constx:
      /* Constant from the .debug_addr section (indexed based on CU).  */
      {
	Dwarf_Attribute addr_attr;
	if (dwarf_getlocation_attr (attr, expr, &addr_attr) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_getlocation_attr for addr: %s",
		 dwarf_errmsg (-1));

	Dwarf_Word constant;
	if (dwarf_formudata (&addr_attr, &constant) != 0)
	  error (EXIT_FAILURE, 0, "dwarf_formudata constant failed: %s",
		 dwarf_errmsg (-1));

	printf ("const: 0x%" PRIx64, constant);
      }
      break;

    default:
      error (EXIT_FAILURE, 0, "unhandled opcode: DW_OP_%s (0x%x)",
	     opname, atom);
    }
}

/* Get all variables and print their value expressions. */
static void
print_varlocs (Dwarf_Die *funcdie)
{
  // Display frame base for function if it exists.
  // Should be used for DW_OP_fbreg.
  has_frame_base = dwarf_hasattr (funcdie, DW_AT_frame_base);
  if (has_frame_base)
    {
      Dwarf_Attribute fb_attr;
      if (dwarf_attr (funcdie, DW_AT_frame_base, &fb_attr) == NULL)
	error (EXIT_FAILURE, 0, "dwarf_attr fb: %s", dwarf_errmsg (-1));

      Dwarf_Op *fb_expr;
      size_t fb_exprlen;
      if (dwarf_getlocation (&fb_attr, &fb_expr, &fb_exprlen) == 0)
	{
	  // Covers all of function.
	  Dwarf_Addr entrypc;
	  if (dwarf_entrypc (funcdie, &entrypc) != 0)
	    error (EXIT_FAILURE, 0, "dwarf_entrypc: %s", dwarf_errmsg (-1));

	  printf ("    frame_base: ");
	  if (entrypc == 0)
	    printf ("XXX zero address"); // XXX bad DWARF?
	  else
	    print_expr_block (&fb_attr, fb_expr, fb_exprlen, entrypc, 0);
	  printf ("\n");
	}
      else
	{
	  Dwarf_Addr base, start, end;
	  ptrdiff_t off = 0;
	  printf ("    frame_base:\n");
          while ((off = dwarf_getlocations (&fb_attr, off, &base,
					    &start, &end,
					    &fb_expr, &fb_exprlen)) > 0)
	    {
	      printf ("      (%" PRIx64 ",%" PRIx64 ") ", start, end);
	      print_expr_block (&fb_attr, fb_expr, fb_exprlen, start, 0);
	      printf ("\n");
	    }

	  if (off < 0)
	    error (EXIT_FAILURE, 0, "dwarf_getlocations fb: %s",
		   dwarf_errmsg (-1));
	}
    }
  else if (dwarf_tag (funcdie) == DW_TAG_inlined_subroutine)
    {
      // See whether the subprogram we are inlined into has a frame
      // base we should use.
      Dwarf_Die *scopes;
      int n = dwarf_getscopes_die (funcdie, &scopes);
      if (n <= 0)
	error (EXIT_FAILURE, 0, "dwarf_getscopes_die: %s", dwarf_errmsg (-1));

      while (n-- > 0)
	if (dwarf_tag (&scopes[n]) == DW_TAG_subprogram
	    && dwarf_hasattr (&scopes[n], DW_AT_frame_base))
	  {
	    has_frame_base = true;
	    break;
	  }
      free (scopes);
    }

  if (! dwarf_haschildren (funcdie))
    return;

  Dwarf_Die child;
  int res = dwarf_child (funcdie, &child);
  if (res < 0)
    error (EXIT_FAILURE, 0, "dwarf_child: %s", dwarf_errmsg (-1));

  /* We thought there was a child, but the child list was actually
     empty. This isn't technically an error in the DWARF, but it is
     certainly non-optimimal.  */
  if (res == 1)
    return;

  do
    {
      int tag = dwarf_tag (&child);
      if (tag == DW_TAG_variable || tag == DW_TAG_formal_parameter)
	{
	  const char *what = tag == DW_TAG_variable ? "variable" : "parameter";
	  print_die (&child, what, 2);

	  if (dwarf_hasattr (&child, DW_AT_location))
	    {
	      Dwarf_Attribute attr;
	      if (dwarf_attr (&child, DW_AT_location, &attr) == NULL)
		error (EXIT_FAILURE, 0, "dwarf_attr: %s", dwarf_errmsg (-1));

	      Dwarf_Op *expr;
	      size_t exprlen;
	      if (dwarf_getlocation (&attr, &expr, &exprlen) == 0)
		{
		  // Covers all ranges of the function.
		  // Evaluate the expression block for each range.
		  ptrdiff_t offset = 0;
		  Dwarf_Addr base, begin, end;
		  do
		    {
		      offset = dwarf_ranges (funcdie, offset, &base,
					     &begin, &end);
		      if (offset < 0)
			error (EXIT_FAILURE, 0, "dwarf_ranges: %s",
			       dwarf_errmsg (-1));

		      if (offset > 0)
			{
			  if (exprlen == 0)
			    printf ("      (%"
				    PRIx64 ",%" PRIx64
				    ") <empty expression>\n", begin, end);
			  else
			    print_expr_block_addrs (&attr, begin, end,
						    expr, exprlen);
			}
		    }
		  while (offset > 0);

		  if (offset < 0)
		    error (EXIT_FAILURE, 0, "dwarf_ranges: %s",
			   dwarf_errmsg (-1));
		}
	      else
		{
		  Dwarf_Addr base, begin, end;
		  ptrdiff_t offset = 0;
		  while ((offset = dwarf_getlocations (&attr, offset,
						       &base, &begin, &end,
						       &expr, &exprlen)) > 0)
		    if (begin >= end)
		      printf ("      (%" PRIx64 ",%" PRIx64
			      ") <empty range>\n", begin, end); // XXX report?
		    else
		      {
			print_expr_block_addrs (&attr, begin, end,
						expr, exprlen);

			// Extra sanity check for dwarf_getlocation_addr
			// Must at least find one range for begin and end-1.
			Dwarf_Op *expraddr;
			size_t expraddr_len;
			int locs = dwarf_getlocation_addr (&attr, begin,
							   &expraddr,
							   &expraddr_len, 1);
			assert (locs == 1);
			locs = dwarf_getlocation_addr (&attr, end - 1,
						       &expraddr,
						       &expraddr_len, 1);
			assert (locs == 1);
		      }

		  if (offset < 0)
		    error (EXIT_FAILURE, 0, "dwarf_getlocations: %s",
			   dwarf_errmsg (-1));
		}
	    }
	  else if (dwarf_hasattr (&child, DW_AT_const_value))
	    {
	      printf ("      <constant value>\n"); // Lookup type and print.
	    }
	  else
	    {
	      printf ("      <no value>\n");
	    }
	}
    }
  while (dwarf_siblingof (&child, &child) == 0);
}

static int
handle_instance (Dwarf_Die *funcdie, void *arg __attribute__ ((unused)))
{
  print_die (funcdie, "inlined function", 1);
  print_varlocs (funcdie);

  return DWARF_CB_OK;
}

static int
handle_function (Dwarf_Die *funcdie, void *arg __attribute__((unused)))
{
  if (dwarf_func_inline (funcdie) > 0)
    {
      // abstract inline definition, find all inlined instances.

      // Note this is convenient for listing all instances together
      // so you can easily compare the location expressions describing
      // the variables and parameters, but it isn't very efficient
      // since it will walk the DIE tree multiple times.
      if (dwarf_func_inline_instances (funcdie, &handle_instance, NULL) != 0)
	error (EXIT_FAILURE, 0, "dwarf_func_inline_instances: %s",
	       dwarf_errmsg (-1));
    }
  else
    {
      // Contains actual code, not just a declaration?
      Dwarf_Addr entrypc;
      if (dwarf_entrypc (funcdie, &entrypc) == 0)
	{
	  print_die (funcdie, "function", 1);
	  print_varlocs (funcdie);
	}
    }

  return DWARF_CB_OK;
}

struct attr_arg
{
  int depth;
  Dwarf_Addr entrypc;
};

static int
handle_attr (Dwarf_Attribute *attr, void *arg)
{
  int depth = ((struct attr_arg *) arg)->depth;
  Dwarf_Addr entrypc = ((struct attr_arg *) arg)->entrypc;

  unsigned int code = dwarf_whatattr (attr);
  unsigned int form = dwarf_whatform (attr);

  printf ("%*s%s (%s)", depth * 2, "",
	  dwarf_attr_string (code), dwarf_form_string (form));

  /* If we can get an DWARF expression (or location lists) from this
     attribute we'll print it, otherwise we'll ignore it.  But if
     there is an error while the attribute has the "correct" form then
     we'll report an error (we can only really check DW_FORM_exprloc
     other forms can be ambiguous).  */
  Dwarf_Op *expr;
  size_t exprlen;
  bool printed = false;
  int res = dwarf_getlocation (attr, &expr, &exprlen);
  if (res == 0)
    {
      printf (" ");
      print_expr_block (attr, expr, exprlen, entrypc, 0);
      printf ("\n");
      printed = true;
    }
  else if (form == DW_FORM_exprloc)
    {
      error (0, 0, "%s dwarf_getlocation failed: %s",
	     dwarf_attr_string (code), dwarf_errmsg (-1));
      return DWARF_CB_ABORT;
    }
  else
    {
      Dwarf_Addr base, begin, end;
      ptrdiff_t offset = 0;
      while ((offset = dwarf_getlocations (attr, offset,
					   &base, &begin, &end,
					   &expr, &exprlen)) > 0)
	{
	  if (! printed)
	    printf ("\n");
	  printf ("%*s", depth * 2, "");
	  print_expr_block_addrs (attr, begin, end, expr, exprlen);
	  printed = true;
	}
    }

  if (! printed)
    printf ("\n");

  return DWARF_CB_OK;
}

static void
handle_die (Dwarf_Die *die, int depth, bool outer_has_frame_base,
	    Dwarf_Addr outer_entrypc)
{
  /* CU DIE already printed.  */
  if (depth > 0)
    {
      const char *name = dwarf_diename (die);
      if (name != NULL)
	printf ("%*s[%" PRIx64 "] %s \"%s\"\n", depth * 2, "",
		dwarf_dieoffset (die), dwarf_tag_string (dwarf_tag (die)),
		name);
      else
	printf ("%*s[%" PRIx64 "] %s\n", depth * 2, "",
		dwarf_dieoffset (die), dwarf_tag_string (dwarf_tag (die)));
    }

  struct attr_arg arg;
  arg.depth = depth + 1;

  /* The (lowest) address to use for (looking up) operands that depend
     on address.  */
  Dwarf_Addr die_entrypc;
  if (dwarf_entrypc (die, &die_entrypc) != 0 || die_entrypc == 0)
    {
      /* Try to get the lowest address of the first range covered.  */
      Dwarf_Addr base, start, end;
      if (dwarf_ranges (die, 0, &base, &start, &end) <= 0 || start == 0)
	die_entrypc = outer_entrypc;
      else
	die_entrypc = start;
    }
  arg.entrypc = die_entrypc;

  /* Whether this or the any outer DIE has a frame base. Used as
     sanity check when printing expressions that use DW_OP_fbreg.  */
  bool die_has_frame_base = dwarf_hasattr (die, DW_AT_frame_base);
  die_has_frame_base |= outer_has_frame_base;
  has_frame_base = die_has_frame_base;

  /* Look through all attributes to find those that contain DWARF
     expressions and print those.  We expect to handle all attributes,
     anything else is an error.  */
  if (dwarf_getattrs (die, handle_attr, &arg, 0) != 1)
    error (EXIT_FAILURE, 0, "Couldn't get all attributes: %s",
	   dwarf_errmsg (-1));

  /* Handle children and siblings recursively depth first.  */
  Dwarf_Die child;
  if (dwarf_haschildren (die) != 0 && dwarf_child (die, &child) == 0)
    handle_die (&child, depth + 1, die_has_frame_base, die_entrypc);

  Dwarf_Die sibling;
  if (dwarf_siblingof (die, &sibling) == 0)
    handle_die (&sibling, depth, outer_has_frame_base, outer_entrypc);
}

int
main (int argc, char *argv[])
{
  /* With --exprlocs we process all DIEs looking for any attribute
     which contains an DWARF expression (but not location lists) and
     print those.  Otherwise we process all function DIEs and print
     all DWARF expressions and location lists associated with
     parameters and variables). It must be the first argument,
     or the second, after --debug.  */
  bool exprlocs = false;

  /* With --debug we ignore not being able to find .eh_frame.
     It must come as first argument.  */
  is_debug = false;
  if (argc > 1)
    {
      if (strcmp ("--exprlocs", argv[1]) == 0)
	{
	  exprlocs = true;
	  argv[1] = "";
	}
      else if (strcmp ("--debug", argv[1]) == 0)
	{
	  is_debug = true;
	  argv[1] = "";
	}
    }

  if (argc > 2)
    {
      if (strcmp ("--exprlocs", argv[2]) == 0)
	{
	  exprlocs = true;
	  argv[2] = "";
	}
    }

  int remaining;
  Dwfl *dwfl;
  (void) argp_parse (dwfl_standard_argp (), argc, argv, 0, &remaining,
                     &dwfl);
  assert (dwfl != NULL);

  Dwarf_Die *cu = NULL;
  Dwarf_Addr dwbias;
  bool found_cu = false;
  while ((cu = dwfl_nextcu (dwfl, cu, &dwbias)) != NULL)
    {
      /* Only walk actual compile units (not partial units) that
	 contain code if we are only interested in the function variable
	 locations.  */
      Dwarf_Die cudie;
      Dwarf_Die subdie;
      uint8_t unit_type;
      if (dwarf_cu_info (cu->cu, NULL, &unit_type, &cudie, &subdie,
		         NULL, NULL, NULL) != 0)
	error (EXIT_FAILURE, 0, "dwarf_cu_info: %s", dwarf_errmsg (-1));
      if (unit_type == DW_UT_skeleton)
	cudie = subdie;

      Dwarf_Addr cubase;
      if (dwarf_tag (&cudie) == DW_TAG_compile_unit
	  && (exprlocs || dwarf_lowpc (&cudie, &cubase) == 0))
	{
	  found_cu = true;

	  Dwfl_Module *mod = dwfl_cumodule (cu);
	  Dwarf_Addr modbias;
	  dw = dwfl_module_getdwarf (mod, &modbias);
	  assert (dwbias == modbias);

	  const char *mainfile;
	  const char *modname = dwfl_module_info (mod, NULL,
						  NULL, NULL,
						  NULL, NULL,
						  &mainfile,
						  NULL);
	  if (modname == NULL)
	    error (EXIT_FAILURE, 0, "dwfl_module_info: %s", dwarf_errmsg (-1));

	  const char *name = (modname[0] != '\0'
			      ? modname
			      :  xbasename (mainfile));
	  printf ("module '%s'\n", name);
	  print_die (&cudie, "CU", 0);

	  Dwarf_Addr elfbias;
	  Elf *elf = dwfl_module_getelf (mod, &elfbias);

	  // CFI. We need both since sometimes neither is complete.
	  cfi_debug = dwfl_module_dwarf_cfi (mod, &cfi_debug_bias);
	  cfi_eh = dwfl_module_eh_cfi (mod, &cfi_eh_bias);

	  // No bias needed, same file.
	  assert (cfi_debug == NULL || cfi_debug_bias == 0);

	  // We are a bit forgiving for object files.  There might be
	  // relocations we don't handle that are needed in some
	  // places...
	  GElf_Ehdr ehdr_mem, *ehdr = gelf_getehdr (elf, &ehdr_mem);
	  is_ET_REL = ehdr->e_type == ET_REL;

	  if (exprlocs)
	    {
	      Dwarf_Addr entrypc;
	      if (dwarf_entrypc (&cudie, &entrypc) != 0)
		entrypc = 0;

	      /* XXX - Passing true for has_frame_base is not really true.
		 We do it because we want to resolve all DIEs and all
		 attributes. Technically we should check that the DIE
		 (types) are referenced from variables that are defined in
		 a context (function) that has a frame base.  */
	      handle_die (&cudie, 0, true /* Should be false */, entrypc);
	    }
	  else if (dwarf_getfuncs (&cudie, handle_function, NULL, 0) != 0)
	    error (EXIT_FAILURE, 0, "dwarf_getfuncs %s",
		   dwarf_errmsg (-1));
	}
    }

  if (! found_cu)
    error (EXIT_FAILURE, 0, "No DWARF CU found?");

  dwfl_end (dwfl);
  return 0;
}
