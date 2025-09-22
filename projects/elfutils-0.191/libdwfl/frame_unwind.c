/* Get previous frame state for an existing frame state.
   Copyright (C) 2013, 2014, 2016 Red Hat, Inc.
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
#include <stdlib.h>
#include "libdwflP.h"
#include "dwarf.h"
#include <system.h>

/* Maximum number of DWARF expression stack slots before returning an error.  */
#define DWARF_EXPR_STACK_MAX 0x100

/* Maximum number of DWARF expression executed operations before returning an
   error.  */
#define DWARF_EXPR_STEPS_MAX 0x1000

int
internal_function
__libdwfl_frame_reg_get (Dwfl_Frame *state, unsigned regno, Dwarf_Addr *val)
{
  Ebl *ebl = state->thread->process->ebl;
  if (! ebl_dwarf_to_regno (ebl, &regno))
    return -1;
  if (regno >= ebl_frame_nregs (ebl))
    return -1;
  if ((state->regs_set[regno / sizeof (*state->regs_set) / 8]
       & ((uint64_t) 1U << (regno % (sizeof (*state->regs_set) * 8)))) == 0)
    return 1;
  if (val)
    *val = state->regs[regno];
  return 0;
}

bool
internal_function
__libdwfl_frame_reg_set (Dwfl_Frame *state, unsigned regno, Dwarf_Addr val)
{
  Ebl *ebl = state->thread->process->ebl;
  if (! ebl_dwarf_to_regno (ebl, &regno))
    return false;
  if (regno >= ebl_frame_nregs (ebl))
    return false;
  /* For example i386 user_regs_struct has signed fields.  */
  if (ebl_get_elfclass (ebl) == ELFCLASS32)
    val &= 0xffffffff;
  state->regs_set[regno / sizeof (*state->regs_set) / 8] |=
		((uint64_t) 1U << (regno % (sizeof (*state->regs_set) * 8)));
  state->regs[regno] = val;
  return true;
}

static int
bra_compar (const void *key_voidp, const void *elem_voidp)
{
  Dwarf_Word offset = (uintptr_t) key_voidp;
  const Dwarf_Op *op = elem_voidp;
  return (offset > op->offset) - (offset < op->offset);
}

struct eval_stack {
  Dwarf_Addr *addrs;
  size_t used;
  size_t allocated;
};

static bool
do_push (struct eval_stack *stack, Dwarf_Addr val)
{
  if (stack->used >= DWARF_EXPR_STACK_MAX)
    {
      __libdwfl_seterrno (DWFL_E_INVALID_DWARF);
      return false;
    }
  if (stack->used == stack->allocated)
    {
      stack->allocated = MAX (stack->allocated * 2, 32);
      Dwarf_Addr *new_addrs;
      new_addrs = realloc (stack->addrs,
			   stack->allocated * sizeof (*stack->addrs));
      if (new_addrs == NULL)
        {
          __libdwfl_seterrno (DWFL_E_NOMEM);
          return false;
        }
      stack->addrs = new_addrs;
    }
  stack->addrs[stack->used++] = val;
  return true;
}

static bool
do_pop (struct eval_stack *stack, Dwarf_Addr *val)
{
  if (stack->used == 0)
    {
      __libdwfl_seterrno (DWFL_E_INVALID_DWARF);
      return false;
    }
  *val = stack->addrs[--stack->used];
  return true;
}

/* If FRAME is NULL is are computing CFI frame base.  In such case another
   DW_OP_call_frame_cfa is no longer permitted.  */

static bool
expr_eval (Dwfl_Frame *state, Dwarf_Frame *frame, const Dwarf_Op *ops,
	   size_t nops, Dwarf_Addr *result, Dwarf_Addr bias)
{
  Dwfl_Process *process = state->thread->process;
  if (nops == 0)
    {
      __libdwfl_seterrno (DWFL_E_INVALID_DWARF);
      return false;
    }
  struct eval_stack stack =
    {
      .addrs = NULL,
      .used = 0,
      .allocated = 0
    };

#define pop(x) do_pop(&stack, x)
#define push(x) do_push(&stack, x)

  Dwarf_Addr val1, val2;
  bool is_location = false;
  size_t steps_count = 0;
  for (const Dwarf_Op *op = ops; op < ops + nops; op++)
    {
      if (++steps_count > DWARF_EXPR_STEPS_MAX)
	{
	  __libdwfl_seterrno (DWFL_E_INVALID_DWARF);
	  return false;
	}
      switch (op->atom)
      {
	/* DW_OP_* order matches libgcc/unwind-dw2.c execute_stack_op:  */
	case DW_OP_lit0 ... DW_OP_lit31:
	  if (! push (op->atom - DW_OP_lit0))
	    {
	      free (stack.addrs);
	      return false;
	    }
	  break;
	case DW_OP_addr:
	  if (! push (op->number + bias))
	    {
	      free (stack.addrs);
	      return false;
	    }
	  break;
	case DW_OP_GNU_encoded_addr:
	  /* Missing support in the rest of elfutils.  */
	  __libdwfl_seterrno (DWFL_E_UNSUPPORTED_DWARF);
	  return false;
	case DW_OP_const1u:
	case DW_OP_const1s:
	case DW_OP_const2u:
	case DW_OP_const2s:
	case DW_OP_const4u:
	case DW_OP_const4s:
	case DW_OP_const8u:
	case DW_OP_const8s:
	case DW_OP_constu:
	case DW_OP_consts:
	  if (! push (op->number))
	    {
	      free (stack.addrs);
	      return false;
	    }
	  break;
	case DW_OP_reg0 ... DW_OP_reg31:
	  if (INTUSE (dwfl_frame_reg) (state, op->atom - DW_OP_reg0, &val1) != 0
	      || ! push (val1))
	    {
	      free (stack.addrs);
	      return false;
	    }
	  break;
	case DW_OP_regx:
	  if (INTUSE (dwfl_frame_reg) (state, op->number, &val1) != 0 || ! push (val1))
	    {
	      free (stack.addrs);
	      return false;
	    }
	  break;
	case DW_OP_breg0 ... DW_OP_breg31:
	  if (INTUSE (dwfl_frame_reg) (state, op->atom - DW_OP_breg0, &val1) != 0)
	    {
	      free (stack.addrs);
	      return false;
	    }
	  val1 += op->number;
	  if (! push (val1))
	    {
	      free (stack.addrs);
	      return false;
	    }
	  break;
	case DW_OP_bregx:
	  if (INTUSE (dwfl_frame_reg) (state, op->number, &val1) != 0)
	    {
	      free (stack.addrs);
	      return false;
	    }
	  val1 += op->number2;
	  if (! push (val1))
	    {
	      free (stack.addrs);
	      return false;
	    }
	  break;
	case DW_OP_dup:
	  if (! pop (&val1) || ! push (val1) || ! push (val1))
	    {
	      free (stack.addrs);
	      return false;
	    }
	  break;
	case DW_OP_drop:
	  if (! pop (&val1))
	    {
	      free (stack.addrs);
	      return false;
	    }
	  break;
	case DW_OP_pick:
	  if (stack.used <= op->number)
	    {
	      free (stack.addrs);
	      __libdwfl_seterrno (DWFL_E_INVALID_DWARF);
	      return false;
	    }
	  if (! push (stack.addrs[stack.used - 1 - op->number]))
	    {
	      free (stack.addrs);
	      return false;
	    }
	  break;
	case DW_OP_over:
	  if (! pop (&val1) || ! pop (&val2)
	      || ! push (val2) || ! push (val1) || ! push (val2))
	    {
	      free (stack.addrs);
	      return false;
	    }
	  break;
	case DW_OP_swap:
	  if (! pop (&val1) || ! pop (&val2) || ! push (val1) || ! push (val2))
	    {
	      free (stack.addrs);
	      return false;
	    }
	  break;
	case DW_OP_rot:
	  {
	    Dwarf_Addr val3;
	    if (! pop (&val1) || ! pop (&val2) || ! pop (&val3)
		|| ! push (val1) || ! push (val3) || ! push (val2))
	      {
		free (stack.addrs);
		return false;
	      }
	  }
	  break;
	case DW_OP_deref:
	case DW_OP_deref_size:
	  if (process->callbacks->memory_read == NULL)
	    {
	      free (stack.addrs);
	      __libdwfl_seterrno (DWFL_E_INVALID_ARGUMENT);
	      return false;
	    }
	  if (! pop (&val1)
	      || ! process->callbacks->memory_read (process->dwfl, val1, &val1,
						    process->callbacks_arg))
	    {
	      free (stack.addrs);
	      return false;
	    }
	  if (op->atom == DW_OP_deref_size)
	    {
	      const int elfclass = frame->cache->e_ident[EI_CLASS];
	      const unsigned addr_bytes = elfclass == ELFCLASS32 ? 4 : 8;
	      if (op->number > addr_bytes)
		{
		  free (stack.addrs);
		  __libdwfl_seterrno (DWFL_E_INVALID_DWARF);
		  return false;
		}
#if BYTE_ORDER == BIG_ENDIAN
	      if (op->number == 0)
		val1 = 0;
	      else
		val1 >>= (addr_bytes - op->number) * 8;
#else
	      if (op->number < 8)
		val1 &= (1ULL << (op->number * 8)) - 1;
#endif
	    }
	  if (! push (val1))
	    {
	      free (stack.addrs);
	      return false;
	    }
	  break;
#define UNOP(atom, expr)						\
	case atom:							\
	  if (! pop (&val1) || ! push (expr))				\
	    {								\
	      free (stack.addrs);					\
	      return false;						\
	    }								\
	  break;
	UNOP (DW_OP_abs, llabs ((int64_t) val1))
	UNOP (DW_OP_neg, -(int64_t) val1)
	UNOP (DW_OP_not, ~val1)
#undef UNOP
	case DW_OP_plus_uconst:
	  if (! pop (&val1) || ! push (val1 + op->number))
	    {
	      free (stack.addrs);
	      return false;
	    }
	  break;
#define BINOP(atom, op)							\
	case atom:							\
	  if (! pop (&val2) || ! pop (&val1) || ! push (val1 op val2))	\
	    {								\
	      free (stack.addrs);					\
	      return false;						\
	    }								\
	  break;
#define BINOP_SIGNED(atom, op)						\
	case atom:							\
	  if (! pop (&val2) || ! pop (&val1)				\
	      || ! push ((int64_t) val1 op (int64_t) val2))		\
	    {								\
	      free (stack.addrs);					\
	      return false;						\
	    }								\
	  break;
	BINOP (DW_OP_and, &)
	case DW_OP_div:
	  if (! pop (&val2) || ! pop (&val1))
	    {
	      free (stack.addrs);
	      return false;
	    }
	  if (val2 == 0)
	    {
	      free (stack.addrs);
	      __libdwfl_seterrno (DWFL_E_INVALID_DWARF);
	      return false;
	    }
	  if (! push ((int64_t) val1 / (int64_t) val2))
	    {
	      free (stack.addrs);
	      return false;
	    }
	  break;
	BINOP (DW_OP_minus, -)
	case DW_OP_mod:
	  if (! pop (&val2) || ! pop (&val1))
	    {
	      free (stack.addrs);
	      return false;
	    }
	  if (val2 == 0)
	    {
	      free (stack.addrs);
	      __libdwfl_seterrno (DWFL_E_INVALID_DWARF);
	      return false;
	    }
	  if (! push (val1 % val2))
	    {
	      free (stack.addrs);
	      return false;
	    }
	  break;
	BINOP (DW_OP_mul, *)
	BINOP (DW_OP_or, |)
	BINOP (DW_OP_plus, +)
	BINOP (DW_OP_shl, <<)
	BINOP (DW_OP_shr, >>)
	BINOP_SIGNED (DW_OP_shra, >>)
	BINOP (DW_OP_xor, ^)
	BINOP_SIGNED (DW_OP_le, <=)
	BINOP_SIGNED (DW_OP_ge, >=)
	BINOP_SIGNED (DW_OP_eq, ==)
	BINOP_SIGNED (DW_OP_lt, <)
	BINOP_SIGNED (DW_OP_gt, >)
	BINOP_SIGNED (DW_OP_ne, !=)
#undef BINOP
#undef BINOP_SIGNED
	case DW_OP_bra:
	  if (! pop (&val1))
	    {
	      free (stack.addrs);
	      return false;
	    }
	  if (val1 == 0)
	    break;
	  FALLTHROUGH;
	case DW_OP_skip:;
	  Dwarf_Word offset = op->offset + 1 + 2 + (int16_t) op->number;
	  const Dwarf_Op *found = bsearch ((void *) (uintptr_t) offset, ops, nops,
					   sizeof (*ops), bra_compar);
	  if (found == NULL)
	    {
	      free (stack.addrs);
	      /* PPC32 vDSO has such invalid operations.  */
	      __libdwfl_seterrno (DWFL_E_INVALID_DWARF);
	      return false;
	    }
	  /* Undo the 'for' statement increment.  */
	  op = found - 1;
	  break;
	case DW_OP_nop:
	  break;
	/* DW_OP_* not listed in libgcc/unwind-dw2.c execute_stack_op:  */
	case DW_OP_call_frame_cfa:;
	  // Not used by CFI itself but it is synthetized by elfutils internation.
	  Dwarf_Op *cfa_ops;
	  size_t cfa_nops;
	  Dwarf_Addr cfa;
	  if (frame == NULL
	      || dwarf_frame_cfa (frame, &cfa_ops, &cfa_nops) != 0
	      || ! expr_eval (state, NULL, cfa_ops, cfa_nops, &cfa, bias)
	      || ! push (cfa))
	    {
	      __libdwfl_seterrno (DWFL_E_LIBDW);
	      free (stack.addrs);
	      return false;
	    }
	  is_location = true;
	  break;
	case DW_OP_stack_value:
	  // Not used by CFI itself but it is synthetized by elfutils internation.
	  is_location = false;
	  break;
	default:
	  __libdwfl_seterrno (DWFL_E_INVALID_DWARF);
	  return false;
      }
    }
  if (! pop (result))
    {
      free (stack.addrs);
      return false;
    }
  free (stack.addrs);
  if (is_location)
    {
      if (process->callbacks->memory_read == NULL)
	{
	  __libdwfl_seterrno (DWFL_E_INVALID_ARGUMENT);
	  return false;
	}
      if (! process->callbacks->memory_read (process->dwfl, *result, result,
					     process->callbacks_arg))
	return false;
    }
  return true;
#undef push
#undef pop
}

static Dwfl_Frame *
new_unwound (Dwfl_Frame *state)
{
  assert (state->unwound == NULL);
  Dwfl_Thread *thread = state->thread;
  Dwfl_Process *process = thread->process;
  Ebl *ebl = process->ebl;
  size_t nregs = ebl_frame_nregs (ebl);
  assert (nregs > 0);
  Dwfl_Frame *unwound;
  unwound = malloc (sizeof (*unwound) + sizeof (*unwound->regs) * nregs);
  if (unlikely (unwound == NULL))
    return NULL;
  state->unwound = unwound;
  unwound->thread = thread;
  unwound->unwound = NULL;
  unwound->signal_frame = false;
  unwound->initial_frame = false;
  unwound->pc_state = DWFL_FRAME_STATE_ERROR;
  memset (unwound->regs_set, 0, sizeof (unwound->regs_set));
  return unwound;
}

/* The logic is to call __libdwfl_seterrno for any CFI bytecode interpretation
   error so one can easily catch the problem with a debugger.  Still there are
   archs with invalid CFI for some registers where the registers are never used
   later.  Therefore we continue unwinding leaving the registers undefined.  */

static void
handle_cfi (Dwfl_Frame *state, Dwarf_Addr pc, Dwarf_CFI *cfi, Dwarf_Addr bias)
{
  Dwarf_Frame *frame;
  if (INTUSE(dwarf_cfi_addrframe) (cfi, pc, &frame) != 0)
    {
      __libdwfl_seterrno (DWFL_E_LIBDW);
      return;
    }

  Dwfl_Frame *unwound = new_unwound (state);
  if (unwound == NULL)
    {
      __libdwfl_seterrno (DWFL_E_NOMEM);
      return;
    }

  unwound->signal_frame = frame->fde->cie->signal_frame;
  Dwfl_Thread *thread = state->thread;
  Dwfl_Process *process = thread->process;
  Ebl *ebl = process->ebl;
  size_t nregs = ebl_frame_nregs (ebl);
  assert (nregs > 0);

  /* The return register is special for setting the unwound->pc_state.  */
  unsigned ra = frame->fde->cie->return_address_register;
  bool ra_set = false;
  if (! ebl_dwarf_to_regno (ebl, &ra))
    {
      __libdwfl_seterrno (DWFL_E_INVALID_REGISTER);
      return;
    }

  for (unsigned regno = 0; regno < nregs; regno++)
    {
      Dwarf_Op reg_ops_mem[3], *reg_ops;
      size_t reg_nops;
      if (dwarf_frame_register (frame, regno, reg_ops_mem, &reg_ops,
				&reg_nops) != 0)
	{
	  __libdwfl_seterrno (DWFL_E_LIBDW);
	  continue;
	}
      Dwarf_Addr regval;
      if (reg_nops == 0)
	{
	  if (reg_ops == reg_ops_mem)
	    {
	      /* REGNO is undefined.  */
	      if (regno == ra)
		unwound->pc_state = DWFL_FRAME_STATE_PC_UNDEFINED;
	      continue;
	    }
	  else if (reg_ops == NULL)
	    {
	      /* REGNO is same-value.  */
	      if (INTUSE (dwfl_frame_reg) (state, regno, &regval) != 0)
		continue;
	    }
	  else
	    {
	      __libdwfl_seterrno (DWFL_E_INVALID_DWARF);
	      continue;
	    }
	}
      else if (! expr_eval (state, frame, reg_ops, reg_nops, &regval, bias))
	{
	  /* PPC32 vDSO has various invalid operations, ignore them.  The
	     register will look as unset causing an error later, if used.
	     But PPC32 does not use such registers.  */
	  continue;
	}

      /* Some architectures encode some extra info in the return address.  */
      if (regno == frame->fde->cie->return_address_register)
	regval &= ebl_func_addr_mask (ebl);

      /* This is another strange PPC[64] case.  There are two
	 registers numbers that can represent the same DWARF return
	 register number.  We only want one to actually set the return
	 register value.  But we always want to override the value if
	 the register is the actual CIE return address register.  */
      if (ra_set && regno != frame->fde->cie->return_address_register)
	{
	  unsigned r = regno;
	  if (ebl_dwarf_to_regno (ebl, &r) && r == ra)
	    continue;
	}

      if (! __libdwfl_frame_reg_set (unwound, regno, regval))
	{
	  __libdwfl_seterrno (DWFL_E_INVALID_REGISTER);
	  continue;
	}
      else if (! ra_set)
	{
	  unsigned r = regno;
          if (ebl_dwarf_to_regno (ebl, &r) && r == ra)
	    ra_set = true;
	}
    }
  if (unwound->pc_state == DWFL_FRAME_STATE_ERROR)
    {
      int res = INTUSE (dwfl_frame_reg) (unwound,
          frame->fde->cie->return_address_register,
          &unwound->pc);
      if (res == 0)
	{
	  /* PPC32 __libc_start_main properly CFI-unwinds PC as zero.
	     Currently none of the archs supported for unwinding have
	     zero as a valid PC.  */
	  if (unwound->pc == 0)
	    unwound->pc_state = DWFL_FRAME_STATE_PC_UNDEFINED;
	  else
	    {
	      unwound->pc_state = DWFL_FRAME_STATE_PC_SET;
	      /* In SPARC the return address register actually contains
		 the address of the call instruction instead of the return
		 address.  Therefore we add here an offset defined by the
		 backend.  Most likely 0.  */
	      unwound->pc += ebl_ra_offset (ebl);
	    }
	}
      else
	{
	  /* We couldn't set the return register, either it was bogus,
	     or the return pc is undefined, maybe end of call stack.  */
	  unsigned pcreg = frame->fde->cie->return_address_register;
	  if (! ebl_dwarf_to_regno (ebl, &pcreg)
	      || pcreg >= ebl_frame_nregs (ebl))
	    __libdwfl_seterrno (DWFL_E_INVALID_REGISTER);
	  else
	    unwound->pc_state = DWFL_FRAME_STATE_PC_UNDEFINED;
	}
    }
  free (frame);
}

static bool
setfunc (int firstreg, unsigned nregs, const Dwarf_Word *regs, void *arg)
{
  Dwfl_Frame *state = arg;
  Dwfl_Frame *unwound = state->unwound;
  if (firstreg < 0)
    {
      assert (firstreg == -1);
      assert (nregs == 1);
      assert (unwound->pc_state == DWFL_FRAME_STATE_PC_UNDEFINED);
      unwound->pc = *regs;
      unwound->pc_state = DWFL_FRAME_STATE_PC_SET;
      return true;
    }
  while (nregs--)
    if (! __libdwfl_frame_reg_set (unwound, firstreg++, *regs++))
      return false;
  return true;
}

static bool
getfunc (int firstreg, unsigned nregs, Dwarf_Word *regs, void *arg)
{
  Dwfl_Frame *state = arg;
  assert (firstreg >= 0);
  while (nregs--)
    if (INTUSE (dwfl_frame_reg) (state, firstreg++, regs++) != 0)
      return false;
  return true;
}

static bool
readfunc (Dwarf_Addr addr, Dwarf_Word *datap, void *arg)
{
  Dwfl_Frame *state = arg;
  Dwfl_Thread *thread = state->thread;
  Dwfl_Process *process = thread->process;
  return process->callbacks->memory_read (process->dwfl, addr, datap,
					  process->callbacks_arg);
}

void
internal_function
__libdwfl_frame_unwind (Dwfl_Frame *state)
{
  if (state->unwound)
    return;
  /* Do not ask dwfl_frame_pc for ISACTIVATION, it would try to unwind STATE
     which would deadlock us.  */
  Dwarf_Addr pc;
  bool ok = INTUSE(dwfl_frame_pc) (state, &pc, NULL);
  if (!ok)
    return;
  /* Check whether this is the initial frame or a signal frame.
     Then we need to unwind from the original, unadjusted PC.  */
  if (! state->initial_frame && ! state->signal_frame)
    pc--;
  Dwfl_Module *mod = INTUSE(dwfl_addrmodule) (state->thread->process->dwfl, pc);
  if (mod == NULL)
    __libdwfl_seterrno (DWFL_E_NO_DWARF);
  else
    {
      Dwarf_Addr bias;
      Dwarf_CFI *cfi_eh = INTUSE(dwfl_module_eh_cfi) (mod, &bias);
      if (cfi_eh)
	{
	  handle_cfi (state, pc - bias, cfi_eh, bias);
	  if (state->unwound)
	    return;
	}
      Dwarf_CFI *cfi_dwarf = INTUSE(dwfl_module_dwarf_cfi) (mod, &bias);
      if (cfi_dwarf)
	{
	  handle_cfi (state, pc - bias, cfi_dwarf, bias);
	  if (state->unwound)
	    return;
	}
    }
  assert (state->unwound == NULL);
  Dwfl_Thread *thread = state->thread;
  Dwfl_Process *process = thread->process;
  Ebl *ebl = process->ebl;
  if (new_unwound (state) == NULL)
    {
      __libdwfl_seterrno (DWFL_E_NOMEM);
      return;
    }
  state->unwound->pc_state = DWFL_FRAME_STATE_PC_UNDEFINED;
  // &Dwfl_Frame.signal_frame cannot be passed as it is a bitfield.
  bool signal_frame = false;
  if (! ebl_unwind (ebl, pc, setfunc, getfunc, readfunc, state, &signal_frame))
    {
      // Discard the unwind attempt.  During next __libdwfl_frame_unwind call
      // we may have for example the appropriate Dwfl_Module already mapped.
      assert (state->unwound->unwound == NULL);
      free (state->unwound);
      state->unwound = NULL;
      // __libdwfl_seterrno has been called above.
      return;
    }
  assert (state->unwound->pc_state == DWFL_FRAME_STATE_PC_SET);
  state->unwound->signal_frame = signal_frame;
}
