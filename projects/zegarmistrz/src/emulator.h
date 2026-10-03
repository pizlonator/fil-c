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

struct CPU;

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
    bool no_core = false; // --no-core / --passthrough-signals: no core dumps

    // Guest memory map snapshot for ELF core dumps (filled by the loader;
    // guest VA == host VA so the dumper can memcpy directly).
    struct GuestMap {
        uint64_t start = 0; // inclusive
        uint64_t end = 0; // exclusive
        int prot = 0; // PROT_* bits
        std::string path; // object path (may be empty)
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
    std::vector<CPU*> thread_cpus;

    // Guest executable info for auxv.
    uint64_t exe_base = 0;
    uint64_t exe_entry = 0;
    uint64_t exe_phdr = 0;
    uint64_t exe_phent = 0;
    uint64_t exe_phnum = 0;
    uint64_t interp_base = 0;
    std::string interp_path;
};
