/* -*- mesa-c++  -*-
 * Copyright 2019 Collabora LTD
 * Author: Gert Wollny <gert.wollny@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include "sfn_callstack.h"

namespace r600 {

CallStack::CallStack(r600_bytecode& bc):
    m_bc(bc)
{
}

CallStack::~CallStack() {}

int
CallStack::push(unsigned type)
{
   switch (type) {
   case FC_PUSH_VPM:
      ++m_bc.stack.push;
      break;
   case FC_PUSH_WQM:
      ++m_bc.stack.push_wqm;
      break;
   case FC_LOOP:
      ++m_bc.stack.loop;
      break;
   default:
      assert(0);
   }

   return update_max_depth(type);
}

void
CallStack::pop(unsigned type)
{
   switch (type) {
   case FC_PUSH_VPM:
      --m_bc.stack.push;
      assert(m_bc.stack.push >= 0);
      break;
   case FC_PUSH_WQM:
      --m_bc.stack.push_wqm;
      assert(m_bc.stack.push_wqm >= 0);
      break;
   case FC_LOOP:
      --m_bc.stack.loop;
      assert(m_bc.stack.loop >= 0);
      break;
   default:
      assert(0);
      break;
   }
}

int
CallStack::update_max_depth(unsigned type)
{

   r600_stack_info& stack = m_bc.stack;
   int elements;
   int entries;

   int entry_size = stack.entry_size;

   elements = (stack.loop + stack.push_wqm) * entry_size;
   elements += stack.push;

   switch (m_bc.gfx_level) {
   case R600:
   case R700:
      /* pre-r8xx: if any non-WQM PUSH instruction is invoked, 2 elements on
       * the stack must be reserved to hold the current active/continue
       * masks */
      if (type == FC_PUSH_VPM || stack.push > 0) {
         elements += 2;
      }
      break;
   case CAYMAN:
      /* r9xx: any stack operation on empty stack consumes 2 additional
       * elements */
      elements += 2;
      break;
   case EVERGREEN:
      /* r8xx+: 2 extra elements are not always required, but one extra
       * element must be added for each of the following cases:
       * 1. There is an ALU_ELSE_AFTER instruction at the point of greatest
       *    stack usage.
       *    (Currently we don't use ALU_ELSE_AFTER.)
       * 2. There are LOOP/WQM frames on the stack when any flavor of non-WQM
       *    PUSH instruction executed.
       *
       *    NOTE: it seems we also need to reserve additional element in some
       *    other cases, e.g. when we have 4 levels of PUSH_VPM in the shader,
       *    then STACK_SIZE should be 2 instead of 1 */
      if (type == FC_PUSH_VPM || stack.push > 0) {
         elements += 1;
      }
      break;
   default:
      assert(0);
      break;
   }

   entry_size = 4;

   entries = (elements + (entry_size - 1)) / entry_size;

   if (entries > stack.max_entries)
      stack.max_entries = entries;

   return elements;
}

} // namespace r600
