/*
 * Copyright © 2022 Collabora, Ltd.
 * SPDX-License-Identifier: MIT
 */

#include "nak_private.h"
#include "nir_builder.h"

static void
push_block(nir_builder *b, nir_block *block)
{
   assert(nir_cursors_equal(b->cursor, nir_after_impl(b->impl)));
   block->cf_node.parent = &b->impl->cf_node;
   exec_list_push_tail(&b->impl->body, &block->cf_node.node);
   b->cursor = nir_after_block(block);
}

enum scope_type {
   SCOPE_TYPE_SHADER,
   SCOPE_TYPE_IF_MERGE,
   SCOPE_TYPE_LOOP_BREAK,
   SCOPE_TYPE_LOOP_CONT,
};

struct scope {
   enum scope_type type;

   struct scope *parent;
   uint32_t depth;

   nir_block *merge;
   nir_def *bar;

   uint32_t escapes;
};

static struct scope
push_scope(nir_builder *b,
           enum scope_type scope_type,
           struct scope *parent,
           bool needs_sync,
           nir_block *merge_block)
{
   struct scope scope = {
      .parent = parent,
      .type = scope_type,
      .depth = parent != NULL ? parent->depth + 1 : 0,
      .merge = merge_block,
   };

   if (needs_sync)
      scope.bar = nir_bar_set_nv(b);

   return scope;
}

static void
pop_scope(nir_builder *b, nir_def *esc_reg, struct scope scope)
{
   if (scope.bar == NULL)
      return;

   nir_bar_sync_nv(b, scope.bar, scope.bar);

   if (scope.escapes > 0) {
      /* Find the nearest scope with a sync. */
      nir_block *parent_merge = b->impl->end_block;
      for (struct scope *p = scope.parent; p != NULL; p = p->parent) {
         if (p->bar != NULL) {
            parent_merge = p->merge;
            break;
         }
      }

      /* No escape is ~0, halt is 0, and we choose outer scope indices such
       * that outer scopes always have lower indices than inner scopes.
       */
      nir_def *esc = nir_ult_imm(b, nir_load_reg(b, esc_reg), scope.depth);

      /* We have to put the escape in its own block to avoid critical edges.
       * If we just did goto_if, we would end up with multiple successors,
       * including a jump to the parent's merge block which has multiple
       * predecessors.
       */
      nir_block *esc_block = nir_block_create(b->shader);
      nir_block *next_block = nir_block_create(b->shader);
      nir_goto_if(b, esc_block, esc, next_block);
      push_block(b, esc_block);
      nir_goto(b, parent_merge);
      push_block(b, next_block);
   }
}

static enum scope_type
jump_target_scope_type(nir_jump_type jump_type)
{
   switch (jump_type) {
   case nir_jump_break:    return SCOPE_TYPE_LOOP_BREAK;
   case nir_jump_continue: return SCOPE_TYPE_LOOP_CONT;
   default:
      unreachable("Unknown jump type");
   }
}

static void
break_scopes(nir_builder *b, nir_def *esc_reg,
             struct scope *current_scope,
             nir_jump_type jump_type)
{
   nir_block *first_sync = NULL;
   uint32_t target_depth = UINT32_MAX;
   enum scope_type target_scope_type = jump_target_scope_type(jump_type);
   for (struct scope *scope = current_scope; scope; scope = scope->parent) {
      if (first_sync == NULL && scope->bar != NULL)
         first_sync = scope->merge;

      if (scope->type == target_scope_type) {
         if (first_sync == NULL) {
            first_sync = scope->merge;
         } else {
            /* In order for our cascade to work, we need to have the invariant
             * that anything which escapes any scope with a warp sync needs to
             * target a scope with a warp sync.
             */
            assert(scope->bar != NULL);
         }
         target_depth = scope->depth;
         break;
      } else {
         scope->escapes++;
      }
   }
   assert(target_depth < UINT32_MAX);

   nir_store_reg(b, nir_imm_int(b, target_depth), esc_reg);
   nir_goto(b, first_sync);
}

static void
normal_exit(nir_builder *b, nir_def *esc_reg, nir_block *merge_block)
{
   assert(nir_cursors_equal(b->cursor, nir_after_impl(b->impl)));
   nir_block *block = nir_cursor_current_block(b->cursor);

   if (!nir_block_ends_in_jump(block)) {
      nir_store_reg(b, nir_imm_int(b, ~0), esc_reg);
      nir_goto(b, merge_block);
   }
}

/* This is a heuristic for what instructions are allowed before we sync.
 * Annoyingly, we've gotten rid of phis so it's not as simple as "is it a
 * phi?".
 */
static bool
instr_is_allowed_before_sync(nir_instr *instr)
{
   switch (instr->type) {
   case nir_instr_type_alu: {
      nir_alu_instr *alu = nir_instr_as_alu(instr);
      /* We could probably allow more ALU as long as it doesn't contain
       * derivatives but let's be conservative and only allow mov for now.
       */
      return alu->op == nir_op_mov;
   }

   case nir_instr_type_intrinsic: {
      nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
      return intrin->intrinsic == nir_intrinsic_load_reg ||
             intrin->intrinsic == nir_intrinsic_store_reg;
   }

   default:
      return false;
   }
}

/** Returns true if our successor will sync for us
 *
 * This is a bit of a heuristic
 */
static bool
parent_scope_will_sync(nir_cf_node *node, struct scope *parent_scope)
{
   /* First search forward to see if there's anything non-trivial after this
    * node within the parent scope.
    */
   nir_block *block = nir_cf_node_as_block(nir_cf_node_next(node));
   nir_foreach_instr(instr, block) {
      if (!instr_is_allowed_before_sync(instr))
         return false;
   }

   /* There's another loop or if following and we didn't find a sync */
   if (nir_cf_node_next(&block->cf_node))
      return false;

   /* See if the parent scope will sync for us. */
   if (parent_scope->bar != NULL)
      return true;

   switch (parent_scope->type) {
   case SCOPE_TYPE_SHADER:
      return true;

   case SCOPE_TYPE_IF_MERGE:
      return parent_scope_will_sync(block->cf_node.parent,
                                    parent_scope->parent);

   case SCOPE_TYPE_LOOP_CONT:
      /* In this case, the loop doesn't have a sync of its own so we're
       * expected to be uniform before we hit the continue.
       */
      return false;

   case SCOPE_TYPE_LOOP_BREAK:
      unreachable("Loops must have a continue scope");

   default:
      unreachable("Unknown scope type");
   }
}

static bool
block_is_merge(const nir_block *block)
{
   /* If it's unreachable, there is no merge */
   if (block->imm_dom == NULL)
      return false;

   unsigned num_preds = 0;
   set_foreach(block->predecessors, entry) {
      const nir_block *pred = entry->key;

      /* We don't care about unreachable blocks */
      if (pred->imm_dom == NULL)
         continue;

      num_preds++;
   }

   return num_preds > 1;
}

static void
lower_cf_list(nir_builder *b, nir_def *esc_reg, struct scope *parent_scope,
              struct exec_list *cf_list)
{
   foreach_list_typed_safe(nir_cf_node, node, node, cf_list) {
      switch (node->type) {
      case nir_cf_node_block: {
         nir_block *block = nir_cf_node_as_block(node);
         if (exec_list_is_empty(&block->instr_list))
            break;

         nir_cursor start = nir_before_block(block);
         nir_cursor end = nir_after_block(block);

         nir_jump_instr *jump = NULL;
         nir_instr *last_instr = nir_block_last_instr(block);
         if (last_instr->type == nir_instr_type_jump) {
            jump = nir_instr_as_jump(last_instr);
            end = nir_before_instr(&jump->instr);
         }

         nir_cf_list instrs;
         nir_cf_extract(&instrs, start, end);
         b->cursor = nir_cf_reinsert(&instrs, b->cursor);

         if (jump != NULL) {
            if (jump->type == nir_jump_halt) {
               /* Halt instructions map to OpExit on NVIDIA hardware and
                * exited lanes never block a bsync.
                */
               nir_instr_remove(&jump->instr);
               nir_builder_instr_insert(b, &jump->instr);
            } else {
               /* Everything else needs a break cascade */
               break_scopes(b, esc_reg, parent_scope, jump->type);
            }
         }
         break;
      }

      case nir_cf_node_if: {
         nir_if *nif = nir_cf_node_as_if(node);

         nir_def *cond = nif->condition.ssa;
         nir_instr_clear_src(NULL, &nif->condition);

         nir_block *then_block = nir_block_create(b->shader);
         nir_block *else_block = nir_block_create(b->shader);
         nir_block *merge_block = nir_block_create(b->shader);

         const bool needs_sync = cond->divergent &&
            block_is_merge(nir_cf_node_as_block(nir_cf_node_next(node))) &&
            !parent_scope_will_sync(&nif->cf_node, parent_scope);

         struct scope scope = push_scope(b, SCOPE_TYPE_IF_MERGE,
                                         parent_scope, needs_sync,
                                         merge_block);

         nir_goto_if(b, then_block, cond, else_block);

         push_block(b, then_block);
         lower_cf_list(b, esc_reg, &scope, &nif->then_list);
         normal_exit(b, esc_reg, merge_block);

         push_block(b, else_block);
         lower_cf_list(b, esc_reg, &scope, &nif->else_list);
         normal_exit(b, esc_reg, merge_block);

         push_block(b, merge_block);
         pop_scope(b, esc_reg, scope);

         break;
      }

      case nir_cf_node_loop: {
         nir_loop *loop = nir_cf_node_as_loop(node);

         nir_block *head_block = nir_block_create(b->shader);
         nir_block *break_block = nir_block_create(b->shader);
         nir_block *cont_block = nir_block_create(b->shader);

         /* TODO: We can potentially avoid the break sync for loops when the
          * parent scope syncs for us.  However, we still need to handle the
          * continue clause cascading to the break.  If there is a
          * nir_jump_halt involved, then we have a real cascade where it needs
          * to then jump to the next scope.  Getting all these cases right
          * while avoiding an extra sync for the loop break is tricky at best.
          */
         struct scope break_scope = push_scope(b, SCOPE_TYPE_LOOP_BREAK,
                                               parent_scope, loop->divergent,
                                               break_block);

         nir_goto(b, head_block);
         push_block(b, head_block);

         struct scope cont_scope = push_scope(b, SCOPE_TYPE_LOOP_CONT,
                                              &break_scope, loop->divergent,
                                              cont_block);

         lower_cf_list(b, esc_reg, &cont_scope, &loop->body);
         normal_exit(b, esc_reg, cont_block);

         push_block(b, cont_block);

         pop_scope(b, esc_reg, cont_scope);

         lower_cf_list(b, esc_reg, &break_scope, &loop->continue_list);

         nir_goto(b, head_block);
         push_block(b, break_block);

         pop_scope(b, esc_reg, break_scope);

         break;
      }

      default:
         unreachable("Unknown CF node type");
      }
   }
}

static bool
lower_cf_func(nir_function *func)
{
   if (func->impl == NULL)
      return false;

   if (exec_list_is_singular(&func->impl->body)) {
      nir_metadata_preserve(func->impl, nir_metadata_all);
      return false;
   }

   nir_function_impl *old_impl = func->impl;

   /* We use this in block_is_merge() */
   nir_metadata_require(old_impl, nir_metadata_dominance);

   /* First, we temporarily get rid of SSA.  This will make all our block
    * motion way easier.
    */
   nir_foreach_block(block, old_impl)
      nir_lower_phis_to_regs_block(block);

   /* We create a whole new nir_function_impl and copy the contents over */
   func->impl = NULL;
   nir_function_impl *new_impl = nir_function_impl_create(func);
   new_impl->structured = false;

   /* We copy defs from the old impl */
   new_impl->ssa_alloc = old_impl->ssa_alloc;

   nir_builder b = nir_builder_at(nir_before_impl(new_impl));
   nir_def *esc_reg = nir_decl_reg(&b, 1, 32, 0);

   /* Having a function scope makes everything easier */
   struct scope scope = {
      .type = SCOPE_TYPE_SHADER,
      .merge = new_impl->end_block,
   };
   lower_cf_list(&b, esc_reg, &scope, &old_impl->body);
   normal_exit(&b, esc_reg, new_impl->end_block);

   /* Now sort by reverse PDFS and restore SSA
    *
    * Note: Since we created a new nir_function_impl, there is no metadata,
    * dirty or otherwise, so we have no need to call nir_metadata_preserve().
    */
   nir_sort_unstructured_blocks(new_impl);
   nir_repair_ssa_impl(new_impl);
   nir_lower_reg_intrinsics_to_ssa_impl(new_impl);

   return true;
}

bool
nak_nir_lower_cf(nir_shader *nir)
{
   bool progress = false;

   nir_foreach_function(func, nir) {
      if (lower_cf_func(func))
         progress = true;
   }

   return progress;
}
