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

#include <cstddef>
#include <cstdint>

struct CPU;
struct Emulator;

// Maximum vector width in bytes (ZMM = 64). Centralized so future wider
// vectors only require changing this constant (plus VecVal storage).
constexpr size_t MAX_VEC_BYTES = 64;

// Abstract taint bits for poison tracking. cannot_branch: value must not
// influence a branch target/condition. cannot_index: value must not be used
// in any memory address computation (nor passed to syscalls).
struct Taint {
    bool cannot_branch = false;
    bool cannot_index = false;
    Taint() = default;
    Taint(bool cb, bool ci) : cannot_branch(cb), cannot_index(ci) {}
    bool any() const { return cannot_branch || cannot_index; }
    bool empty() const { return !any(); }
    void clear() { cannot_branch = false; cannot_index = false; }
    void merge(const Taint& o) {
        cannot_branch = cannot_branch || o.cannot_branch;
        cannot_index = cannot_index || o.cannot_index;
    }
    Taint operator|(const Taint& o) const {
        Taint r = *this;
        r.merge(o);
        return r;
    }
    Taint& operator|=(const Taint& o) {
        merge(o);
        return *this;
    }
    bool operator==(const Taint& o) const {
        return cannot_branch == o.cannot_branch && cannot_index == o.cannot_index;
    }
    bool operator!=(const Taint& o) const { return !(*this == o); }
};

// Common memory pipeline. Guest VA == host VA, so the fast path is memcpy,
// but every access goes through here so poisoning / fault translation can
// hook in one place. All functions take CPU* for future per-thread checks.
//
// Faults (host SIGSEGV/SIGBUS on the access) are translated into a guest
// core dump via the fault-recovery mechanism (see mem.cpp); they never
// propagate as host crashes.

// Scalar loads/stores.
uint8_t mem_load8(CPU* cpu, uint64_t addr);
uint16_t mem_load16(CPU* cpu, uint64_t addr);
uint32_t mem_load32(CPU* cpu, uint64_t addr);
uint64_t mem_load64(CPU* cpu, uint64_t addr);
void mem_store8(CPU* cpu, uint64_t addr, uint8_t v);
void mem_store16(CPU* cpu, uint64_t addr, uint16_t v);
void mem_store32(CPU* cpu, uint64_t addr, uint32_t v);
void mem_store64(CPU* cpu, uint64_t addr, uint64_t v);

// Wide vector loads/stores.
void mem_load_bytes(CPU* cpu, uint64_t addr, uint8_t* out, size_t n);
void mem_store_bytes(CPU* cpu, uint64_t addr, const uint8_t* in, size_t n);

// Fetch up to 15 bytes of guest code at rip into buf. Returns number of
// bytes that could be read (caller needs >= 1; short reads only happen if
// the page tail is unmapped, in which case a fault is raised).
size_t mem_read_code(CPU* cpu, uint64_t rip, uint8_t* buf);

// Raw byte copy helpers used by string ops (rep movs etc.).
void mem_copy(CPU* cpu, uint64_t dst, uint64_t src, size_t n);

// Guarded host read for core dumps: copy [src,src+n) to dst, returning false
// (with the failed tail zeroed) instead of raising on an unmapped page.
// Never throws: stale guest_maps entries cannot crash the dumper.
bool mem_copy_guarded(uint64_t src, void* dst, size_t n);

// Poisoning client requests (magic call 0x1410141014101410).
// kind bitmask:
enum : unsigned {
    POISON_CANNOT_LOAD = 1u << 0,
    POISON_CANNOT_STORE = 1u << 1,
    POISON_CANNOT_BRANCH = 1u << 2,
    POISON_CANNOT_INDEX = 1u << 3,
    POISON_CLEAR_BRANCH_ON_STORE = 1u << 4,
    POISON_CLEAR_INDEX_ON_STORE = 1u << 5,
};
void mem_poison_range(Emulator* emu, uint64_t addr, size_t size, unsigned kind);
// Debug: watch stores to [base, base+len) (enabled by ZEG_WATCH_TLS).
void mem_watch_hint(uint64_t base, uint64_t len);
void mem_unpoison_range(Emulator* emu, uint64_t addr, size_t size);

// Per-access poison checks. Throw (via guest_error) on violation.
void mem_check_load(CPU* cpu, uint64_t addr, size_t size);
void mem_check_store(CPU* cpu, uint64_t addr, size_t size);

// Taint side of poisoning: OR the cannot-branch/index bits of [addr,size)
// into *out (cleared if unpoisoned). Used to taint loaded register values.
void mem_get_taint(uint64_t addr, size_t size, Taint* out);
inline void mem_get_taint(uint64_t addr, size_t size, Taint& out) {
    mem_get_taint(addr, size, &out);
}

// Called after a successful store of a value with taint t to
// [addr,size): the destination bytes gain the value's cannot-branch/index
// bits (plus the matching clear-on-store bit, so a later store clears the
// taint). If the destination already carries a clear-on-store bit for a
// tainted value, that value bit is cleared instead (store sanitizes).
// Load/store poison bits and clear bits are sticky.
void mem_note_store(CPU* cpu, uint64_t addr, size_t size, Taint t);

// Poisoning client request (magic call 0x1410141014101410):
//   RDI=op, RSI=ptr, RDX=size, RCX=flags.
// op 0 = no-op/query (returns 0); op 1 = poison with flags bitmask
// (POISON_* bits 1:1); op 2 = unpoison range. Returns the RAX result.
uint64_t mem_client_request(Emulator* emu, CPU* cpu, uint64_t op,
                            uint64_t ptr, uint64_t size, uint64_t flags);

// Install host SIGSEGV/SIGBUS/SIGILL/SIGFPE handler that redirects
// emulator-thread faults into the guest core-dump path. Called once at
// startup. Does nothing when passthrough mode is set.
void mem_install_fault_handler();

// Passthrough mode (--no-core / --passthrough-signals): do not install
// handlers and do not write core dumps (guest errors just print + exit).
void mem_set_passthrough(bool v);
bool mem_passthrough();

// Fault recovery scope: set around the interpreter loop body per instruction.
struct FaultScope {
    CPU* cpu;
    bool armed = false;
    FaultScope(CPU* c);
    ~FaultScope();
};

// Loop-level recovery scope: arm around exec_one() in run_cpu_loop so host
// SIGSEGV/SIGBUS/SIGILL/SIGFPE raised outside a mem accessor (or inside one
// for ILL/FPE) become guest errors at the current guest RIP.
struct LoopScope {
    CPU* cpu = nullptr;
    bool prev_armed = false;
    CPU* prev_cpu = nullptr;
    LoopScope(CPU* c);
    ~LoopScope();
};

// If the LoopScope captured a host signal, returns true and fills signo
// (e.g. SIGSEGV) and the fault address (si_addr, 0 if none). Clears pending.
bool mem_loop_fault(int* signo, uint64_t* addr);

// Raised on any fatal guest error; caught at the top of the thread loop
// which then dumps core.
struct GuestError {
    CPU* cpu;
    uint64_t fault_addr = 0;
    const char* reason = nullptr;
};
[[noreturn]] void guest_error(CPU* cpu, const char* reason, uint64_t fault_addr = 0);
