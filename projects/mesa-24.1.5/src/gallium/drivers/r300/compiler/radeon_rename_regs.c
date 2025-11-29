/*
 * Copyright 2010 Tom Stellard <tstellar@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "util/u_math.h"

#include "radeon_rename_regs.h"

#include "radeon_compiler.h"
#include "radeon_list.h"
#include "radeon_program.h"
#include "radeon_variable.h"

/**
 * This function renames registers in an attempt to get the code close to
 * SSA form.  After this function has completed, most of the register are only
 * written to one time, with a few exceptions.
 *
 * This function assumes all the instructions are still of type
 * RC_INSTRUCTION_NORMAL.
 */
void rc_rename_regs(struct radeon_compiler *c, void *user)
{
	struct rc_instruction * inst;
	struct rc_list * variables;
	struct rc_list * var_ptr;

	/* XXX Remove this once the register allocation works with flow control. */
	for(inst = c->Program.Instructions.Next;
					inst != &c->Program.Instructions;
					inst = inst->Next) {
		if (inst->U.I.Opcode == RC_OPCODE_BGNLOOP)
			return;
	}

	variables = rc_get_variables(c);

	for (var_ptr = variables; var_ptr; var_ptr = var_ptr->Next) {
		int new_index;
		unsigned writemask;
		struct rc_variable * var = var_ptr->Item;

		if (var->Inst->U.I.DstReg.File != RC_FILE_TEMPORARY) {
			continue;
		}

		new_index = rc_find_free_temporary(c);
		if (new_index < 0) {
			rc_error(c, "Ran out of temporary registers\n");
			return;
		}

		writemask = rc_variable_writemask_sum(var);
		rc_variable_change_dst(var, new_index, writemask);
	}
}
