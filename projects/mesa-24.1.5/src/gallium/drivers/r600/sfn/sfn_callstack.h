/* -*- mesa-c++  -*-
 * Copyright 2018-2019 Collabora LTD
 * Author: Gert Wollny <gert.wollny@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef SFN_CALLSTACK_HH
#define SFN_CALLSTACK_HH

#include "gallium/drivers/r600/r600_asm.h"

namespace r600 {

class CallStack {
public:
   CallStack(r600_bytecode& bc);
   ~CallStack();
   int push(unsigned type);
   void pop(unsigned type);
   int update_max_depth(unsigned type);

private:
   r600_bytecode& m_bc;
};

} // namespace r600

#endif // SFN_CALLSTACK_HH
