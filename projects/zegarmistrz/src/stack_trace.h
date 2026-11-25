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

template<typename TaintT>
struct CPU;
struct Emulator;
struct ZegHistStep;

// Print a symbolized GUEST stack trace to stderr for a fatal guest error.
// The walk starts at the exact faulting RIP and unwinds primarily with
// DWARF CFI read from the guest ELF objects' .eh_frame/.debug_frame
// sections (dwarf_unwind.cpp -- the same mechanism gdb uses on the core),
// which works regardless of frame-pointer omission; the CFI evaluation runs
// at RIP - 1 for return-address frames so the call site's row is used.
// Guests without usable CFI fall back to the legacy unwinders, in order:
// the guest RBP frame chain through guest memory (fault-guarded reads;
// frames validated against the guest memory map), then an approximate scan
// of the guest stack for plausible return addresses (marked as approximate).
// Guest addresses are symbolized with BFD against the ELF objects recorded
// in emu->guest_maps (guest VA - load bias == ELF vaddr): the loader's own
// objects plus file-backed guest mmaps -- shared libraries loaded by the
// guest ld.so and dlopen'd objects, whose path + load bias do_mmap records.
// Like gdb, every frame after 0 is a return address and symbolicates at
// its address minus one ("address in block") so the reported line is the
// call's own line; the printed address stays the raw return address. Each
// physical frame expands its DWARF inline chain via
// bfd_find_inliner_info (innermost inlined frame first, synthetic frames
// marked "(inline)").
//
// Called by dump_core BEFORE the core file is written, so the trace is
// available even if producing the core is interrupted (Ctrl-C).
// Templated on the taint sidecar type (the body only reads non-taint state:
// rip, gpr values, emu, tid); instantiated for all three types in
// stack_trace.cpp.
template<typename TaintT>
void print_guest_stack_trace(CPU<TaintT>* cpu);

// Print a BFD-symbolicated poison-history trail for the --history dump path.
// The steps are first sorted by their recording-instruction sequence
// (ZegHistStep::seq) ascending -- trails are assembled by set-union merges
// that run out of execution order, and the seq is what restores true
// chronological order, so the printed trail reads in true chronological
// execution order (every step consumes values produced at earlier steps;
// consecutive steps may assign the same register when the program chains
// back-to-back operations on it, the later step then consuming that
// register's prior value -- an rmw chain (the regression guest's own
// trail contains four such pairs); what the chronological ordering
// eliminates is the scramble symptom from the original complaint: a trail
// that placed an independent overwrite of a register between its producer
// and the step that actually consumed that value; strict per-step
// chaining -- step I+1 using exactly step I's value -- is NOT guaranteed
// when provenance forks, since each merged trail is the union of its
// lineages) even though the stored vector's
// order is merge order. Steps are labeled "step N:" (1-based over
// the whole SORTED trail, oldest first) and EVERY displayed line of a step
// is numbered "#0", "#1", ... gdb-style -- a continuous per-step counter
// over ALL displayed lines, restarting at #0 under the step label -- exactly
// like the guest stack trace numbers its displayed lines, except the trace's
// one counter runs over the whole trace instead of restarting per step. A
// frame's symbolized chain prints one numbered line per entry, all
// repeating the frame's own recorded PC: the frame line is the innermost
// location and the rest of the chain (its inline expansion) follows, so
// inline frames consume numbers exactly like the trace's inline frames do
// (which repeat the physical address instead). Every line prints
// "#%-2zu 0x%016lx " plus its location ("in <func> () at <file>:<line>",
// marked "  (inline)" on all but the chain's last -- physically-containing
// -- entry). A step holds its own PC (--history=1) or its PC plus the rest
// of its recorded stack (--history=N/full): the step's PC is the recorded
// instruction's own PC and the further frames in callers are return
// addresses, which symbolicate at address minus one like the stack trace's
// caller frames. A frame that does not symbolicate falls back to the
// module+offset string; a frame in no known module prints just its bare
// numbered PC prefix. Caps output at the most recent 128 steps (the last
// 128 of the sorted range; the step numbers keep each printed step's
// position in the whole trail).
void print_poison_history(Emulator* emu,
                          const std::vector<ZegHistStep>& steps);
