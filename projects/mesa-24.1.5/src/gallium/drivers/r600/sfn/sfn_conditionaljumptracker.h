/* -*- mesa-c++  -*-
 * Copyright 2018-2019 Collabora LTD
 * Author: Gert Wollny <gert.wollny@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef SFN_CONDITIONALJUMPTRACKER_H
#define SFN_CONDITIONALJUMPTRACKER_H

#include "gallium/drivers/r600/r600_asm.h"

namespace r600 {

enum JumpType {
   jt_loop,
   jt_if
};

/**
  Class to link the jump locations
*/
class ConditionalJumpTracker {
public:
   ConditionalJumpTracker();
   ~ConditionalJumpTracker();

   /* Mark the start of a loop or a if/else */
   void push(r600_bytecode_cf *start, JumpType type);

   /* Mark the end of a loop or a if/else and fixup the jump sites */
   bool pop(r600_bytecode_cf *final, JumpType type);

   /* Add middle sites to the call frame i.e. continue,
    * break inside loops, and else in if-then-else constructs.
    */
   bool add_mid(r600_bytecode_cf *source, JumpType type);

private:
   struct ConditionalJumpTrackerImpl *impl;
};

} // namespace r600

#endif // SFN_CONDITIONALJUMPTRACKER_H
