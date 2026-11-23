// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#pragma once

#include <cstdint>
#include <vector>

#include "emulator.h"

// Register state for CFI unwinding: the 16 guest GPRs in the CPU's ZG_*
// index order (0=RAX 1=RCX 2=RDX 3=RBX 4=RSP 5=RBP 6=RSI 7=RDI 8..15=R8..R15,
// see cpu.h) plus the instruction pointer.
struct ZegUnwindState {
    uint64_t gpr[16];
    uint64_t rip = 0;
};

// One DWARF-CFI unwinding step: given the register state of a frame whose
// code lives at st.rip, use the .eh_frame/.debug_frame CFI of the ELF object
// containing it (found via `maps`, which must be a snapshot of
// Emulator::guest_maps; guest VA - load bias == ELF file vaddr) to compute
// the CALLER's register state (caller.gpr[4] = CFA, restored callee-saved
// registers, caller.rip = return address).
//
// `st_rip_is_ra` must be false for the innermost frame (st.rip is the
// faulting PC) and true afterwards (st.rip is a RETURN ADDRESS; CFI is then
// evaluated at st.rip - 1 so the FDE of the caller's function is found even
// when the call instruction is its last one).
//
// Returns false when the unwind cannot continue: no CFI for the module, no
// FDE covering the PC, unsupported CFI/DWARF expression, unreadable guest
// memory, or RA == 0. Never throws; safe to call from the crash-dump path.
bool zeg_unwind_step(const std::vector<Emulator::GuestMap>& maps,
                     const ZegUnwindState& st, ZegUnwindState& caller,
                     bool st_rip_is_ra);
