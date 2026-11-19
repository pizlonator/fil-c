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

#include <vector>

template<typename TaintT>
struct CPU;
struct Emulator;

// Print a symbolized GUEST stack trace to stderr for a fatal guest error.
// The walk starts at the faulting RIP and follows the guest RBP frame chain
// through guest memory (fault-guarded reads; frames validated against the
// guest memory map). If the chain is unusable (guest built with
// -fomit-frame-pointer), falls back to an approximate scan of the guest
// stack for plausible return addresses (marked as approximate). Guest
// addresses are symbolized with BFD against the ELF objects recorded in
// emu->guest_maps (guest VA - load bias == ELF vaddr): the loader's own
// objects plus file-backed guest mmaps -- shared libraries loaded by the
// guest ld.so and dlopen'd objects, whose path + load bias do_mmap records.
// Each physical frame expands its DWARF inline chain via
// bfd_find_inliner_info (innermost inlined frame first, synthetic frames
// marked "(inline)").
//
// Called by dump_core BEFORE the core file is written, so the trace is
// available even if producing the core is interrupted (Ctrl-C).
// Templated on the taint sidecar type (the body only reads non-taint state:
// rip, gpr values, emu, tid); instantiated for both types in stack_trace.cpp.
template<typename TaintT>
void print_guest_stack_trace(CPU<TaintT>* cpu);

// Print a BFD-symbolicated poison-history trail (oldest first) for the
// --history dump path: one numbered step line per recorded PC, with each
// step's inline stack as indented continuation lines that do not consume
// step numbers. Caps output at the most recent 128 steps.
void print_poison_history(Emulator* emu, const std::vector<uint64_t>& pcs);
