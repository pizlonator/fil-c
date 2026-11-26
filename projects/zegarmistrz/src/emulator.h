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

// NOTE: sys/syscall.h must come before <atomic>: libstdc++'s atomic_wait.h
// uses SYS_futex and relies on it being declared already.
#include <sys/syscall.h>
#include <unistd.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

// Process-wide emulator state shared by all guest threads.
struct Emulator {
    std::string prog_path; // guest program path (for core file naming)
    bool trace_insn = false; // -v: disassemble each instruction
    bool trace_syscall = false; // --trace-syscalls (implied by -v)
    // No core dumps: the default, or --no-core, or --passthrough-signals
    // (which implies it), or config `coredump disabled`. Only the core FILE
    // is gated by this -- the fault handler and the diagnostics (banner +
    // guest stack trace) are independent of it.
    bool no_core = false;
    bool stack_trace = true; // --no-stack-trace disables the crash-time guest backtrace
    // --state-trace=FILE: append one line of whole architectural state
    // (rip, all 16 GPRs, raw RFLAGS, ymm0..15 as 512-bit values) after
    // every executed guest instruction. Opened at emulator start; lines
    // are written straight to the fd (the exit syscall path _exits, so
    // stdio buffering would lose the tail). The x86sem differential
    // suite (x86sem-tests/) compares this against the same-format trace
    // a hardware single-stepper produces. The fd lives here rather than
    // in Engine<TaintT> so all three engine instantiations share one
    // trace without templating the format code.
    std::string state_trace_path;
    int state_trace_fd = -1;
    // --history: 0 = off (plain Taint sidecars), 1 = one guest PC per history
    // step (HistoryTaint), N > 1 = N stack frames per step, SIZE_MAX
    // (= --history=full) = whole stacks (DeepHistoryTaint).
    size_t history_frames = 0;

    // CANNOT_VAR_TIME_OP checking level (--var-time-op=):
    //   known      - trap only on instructions the doc lists as known data-dependent (§4)
    //   plausible  - also §5 suspects
    //   everything - trap on anything not in the guaranteed list (§6+§7)
    enum { ZG_VAR_TIME_KNOWN = 1, ZG_VAR_TIME_PLAUSIBLE = 2, ZG_VAR_TIME_EVERYTHING = 3 };
    int var_time_op = ZG_VAR_TIME_KNOWN;

    // Fatal-error exit overrides (valgrind parity; also settable from
    // ~/.zegarmistrz as `error exitcode N` / `error signal N`): the
    // exitcode override has both a CLI form (--error-exitcode=N) and the
    // config form, while the signal override is config-only. 0 = unset.
    // Applied to POISON-class fatal
    // errors only (poison/taint/cannot-/unmapped/bad RIP/string-op
    // violations); unimplemented-instruction errors and other fatal errors
    // keep the natural exit code, so poison and unknown-instruction failures
    // stay distinguishable even with the overrides configured. Exit-code
    // precedence: error_exitcode > error_signo (exit becomes 128+error_signo
    // and NT_PRSTATUS records error_signo) > the natural 128+signo.
    int error_exitcode = 0;
    int error_signo = 0;

    // Guest memory map snapshot for ELF core dumps (filled by the loader;
    // guest VA == host VA so the dumper can memcpy directly).
    struct GuestMap {
        uint64_t start = 0; // inclusive
        uint64_t end = 0; // exclusive
        int prot = 0; // PROT_* bits
        std::string path; // object path (may be empty)
        uint64_t bias = 0; // load bias (guest VA - ELF vaddr); for stack traces
        bool bias_valid = false; // path names a real ELF64 object and bias is its load bias
    };
    std::mutex maps_mu;
    std::vector<GuestMap> guest_maps;

    // Initial guest stack reservation (for the core dumper).
    uint64_t guest_stack_base = 0;
    uint64_t guest_stack_size = 0;

    // Auxv pairs (key,value) placed on the initial stack (for NT_AUXV).
    std::vector<uint64_t> guest_auxv;

    // Virtualized brk.
    std::mutex brk_mu;
    uint64_t guest_brk = 0;
    uint64_t brk_limit = 0; // exclusive upper bound of the brk reservation

    // Exit coordination.
    std::atomic<bool> exiting{false};
    std::atomic<int> exit_code{0};

    // Main thread tid (guest exit from here ends the process).
    int main_tid = 0;
    // Live guest threads (for joining/debug).
    std::mutex thread_mu;
    std::vector<uint64_t> thread_tids;
    // Type-erased CPU pointers (CPU<Taint>, CPU<HistoryTaint>, or
    // CPU<DeepHistoryTaint> depending on history_frames) so every engine
    // instantiation can register here.
    std::vector<void*> thread_cpus;

    // Guest executable info for auxv.
    uint64_t exe_base = 0;
    uint64_t exe_entry = 0;
    uint64_t exe_phdr = 0;
    uint64_t exe_phent = 0;
    uint64_t exe_phnum = 0;
    uint64_t interp_base = 0;
    std::string interp_path;
};
