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

// The engine lives in decode_exec.cpp as Engine<TaintT>, a class template
// over the taint sidecar type (Taint by default, HistoryTaint with
// --history). Only the two entry points below are exported, explicitly
// instantiated for both types in decode_exec.cpp.

// Thread entry: interpreter loop until exit syscall or fatal error.
// Fatal errors dump core and terminate the process.
template<typename TaintT>
void run_cpu_loop(CPU<TaintT>* cpu);

// Write an ELF core dump for a fatal guest error, formatted so gdb/lldb
// show guest state (registers, memory, auxv) rather than the interpreter.
// Filename: zegarmistrz.core.<processname>.<pid> in the current directory.
// signo optionally records the host signal being converted (0 = guess from
// the reason string). poison_history (--history mode) is the offending
// value's recorded poison trail; it is printed BFD-symbolicated after the
// guest stack trace. Returns the dump path. With --no-core, only prints.
template<typename TaintT>
const char* dump_core(CPU<TaintT>* cpu, const char* reason,
                      unsigned long fault_addr, int signo = 0,
                      const std::vector<uint64_t>& poison_history = {});

// Map a fatal guest error to the signal in NT_PRSTATUS and the exit status
// (exit code is 128+signo). hint != 0 wins; otherwise guessed from reason:
// mem/poison/taint violations => SIGSEGV, bad insn => SIGILL, etc.
int guest_fatal_signo(const char* reason, int hint);
// NOTE: sys/syscall.h must come before <atomic>: libstdc++'s atomic_wait.h
// uses SYS_futex and relies on it being declared already.
#include <sys/syscall.h>
#include <unistd.h>
#include <atomic>
// Last fatal signal chosen by dump_core (for exit-status consistency).
extern std::atomic<int> g_last_fatal_signo;
