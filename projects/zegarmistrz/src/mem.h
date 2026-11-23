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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

template<typename TaintT>
struct CPU;
struct Emulator;

// Maximum vector width in bytes (ZMM = 64). Centralized so future wider
// vectors only require changing this constant (plus VecVal storage).
constexpr size_t MAX_VEC_BYTES = 64;

// Abstract taint bits for poison tracking. The two enforcement dimensions
// are orthogonal: cannot_branch: value must not influence control flow
// (branch conditions, branch targets, loop/jcxz counters, return addresses).
// cannot_index: value must not be used in any memory address computation
// (nor passed to syscalls) -- and never at control-flow sites.
// cannot_var_time_op: value must not be used as an operand that makes an
// instruction's execution time data-dependent (other than through branching
// or indexing, which have their own bits).
struct Taint {
    // Trait flag: plain taint carries no poison-history trail (--history off).
    static constexpr bool has_history = false;
    // Trait flag: plain taint never records stack frames (--history off).
    static constexpr bool records_frames = false;
    bool cannot_branch = false;
    bool cannot_index = false;
    bool cannot_var_time_op = false;
    Taint() = default;
    Taint(bool cb, bool ci, bool cvt = false)
        : cannot_branch(cb), cannot_index(ci), cannot_var_time_op(cvt) {}
    bool any() const {
        return cannot_branch || cannot_index || cannot_var_time_op;
    }
    bool empty() const { return !any(); }
    void clear() {
        cannot_branch = false;
        cannot_index = false;
        cannot_var_time_op = false;
    }
    void merge(const Taint& o) {
        cannot_branch = cannot_branch || o.cannot_branch;
        cannot_index = cannot_index || o.cannot_index;
        cannot_var_time_op = cannot_var_time_op || o.cannot_var_time_op;
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
        return cannot_branch == o.cannot_branch &&
               cannot_index == o.cannot_index &&
               cannot_var_time_op == o.cannot_var_time_op;
    }
    bool operator!=(const Taint& o) const { return !(*this == o); }
};

// Current instruction's guest PC, for --history poison-history recording.
// Set at exec_one entry (pre-RIP-advance); read by HistoryTaint's recording
// (as the step PC) and by zeg_history_record_stack (as each deep-history
// step's frame 0). Declared before HistoryTaint: its recording reads it.
extern thread_local uint64_t zeg_history_pc;

// --history=N/full: GPR snapshot (ZG_* order, cpu.h) + Emulator pointer +
// instruction PC of the instruction currently being executed, filled at
// exec_one entry by instantiations whose taint records frames, read by
// zeg_history_record_stack().
extern thread_local uint64_t zeg_history_regs[16];
extern thread_local Emulator* zeg_history_emu;

// Record the current instruction's stack for a --history=N/full history
// step: frame 0 is always the instruction's own PC (zeg_history_pc);
// further frames are unwound with zeg_unwind_step (DWARF CFI). The per-step
// frame count is capped by zeg_set_history_frames (0 = full, cap 256).
// Only callable while no fault-recovery window is armed on the calling
// thread (every taint-recording site is between guest-memory accesses).
void zeg_history_record_stack(std::vector<uint64_t>& out);

// Set the per-step frame cap for --history=N (N > 1); 0 = full/unlimited
// (--history=full). Called once from main before the engine starts.
void zeg_set_history_frames(size_t n);

// --history mode taint: the same three enforcement bits plus a bounded
// history of the guest PCs of the operations that produced or consumed the
// poison (loads from poisoned memory, moves/merges into the value's
// sidecar, stores of the value). Recording is driven by the value-assignment
// operations themselves (operator=/operator|=) plus mem_get_taint (taint
// from poisoned memory) and the engine's note_store wrapper (stores),
// against the thread-local current-instruction PC set at exec_one entry.
// Recording spans memory too: the poison map carries a per-byte trail that
// poisoning requests union their call site into and stores of tainted
// values rewrite, and a load seeds the value's trail from the set-unioned
// per-byte trails of the bytes it reads (plus the load PC), so the
// provenance survives memory round trips.
// The history is printed (BFD-symbolicated) when a taint check faults.

// Trail append with set-union semantics, shared by every site that puts a
// PC into a trail (HistoryTaint::note_op_pc/merge below, and the memory-side
// per-byte trails in mem.cpp). Defined after HistoryTaint (it needs
// kMaxHistory); forward-declared here so the member bodies below can use it.
inline void zeg_trail_add(std::vector<uint64_t>& trail, uint64_t pc);

struct HistoryTaint {
    static constexpr bool has_history = true;
    // Trait flag: mode-1 history records one PC per step, no stack frames.
    static constexpr bool records_frames = false;
    bool cannot_branch = false;
    bool cannot_index = false;
    bool cannot_var_time_op = false;
    std::vector<uint64_t> history; // guest PCs, oldest first
    // Trail capacity: when full, adding a new PC drops the OLDEST entry, so
    // the trail keeps the MOST RECENT operations (the faulting site is never
    // silently frozen out).
    static constexpr size_t kMaxHistory = 4096;
    HistoryTaint() = default;
    bool any() const {
        return cannot_branch || cannot_index || cannot_var_time_op;
    }
    bool empty() const { return !any(); }
    void clear() {
        cannot_branch = false;
        cannot_index = false;
        cannot_var_time_op = false;
        history.clear();
    }
    // Record the current instruction's PC if this taint is poisoned (union
    // semantics via zeg_trail_add: a PC already present anywhere in the
    // trail is not re-added, which collapses repeats from loops; the cap
    // keeps the most recent entries).
    void note_op_pc() {
        if (!any())
            return;
        zeg_trail_add(history, zeg_history_pc);
    }
    void merge(const HistoryTaint& o) {
        cannot_branch = cannot_branch || o.cannot_branch;
        cannot_index = cannot_index || o.cannot_index;
        cannot_var_time_op = cannot_var_time_op || o.cannot_var_time_op;
        // Set-union the other trail in: two operands with the same
        // provenance (or N bytes loaded from identically-provenanced bytes)
        // merge to the distinct set of operations instead of N concatenated
        // copies.
        for (uint64_t pc : o.history)
            zeg_trail_add(history, pc);
    }
    HistoryTaint operator|(const HistoryTaint& o) const {
        HistoryTaint r = *this;
        r.merge(o);
        return r; // temp only: the op PC is noted when this is assigned
    }
    HistoryTaint& operator|=(const HistoryTaint& o) {
        merge(o);
        note_op_pc();
        return *this;
    }
    // Assignment into a value's sidecar: adopt the source's provenance and
    // record this operation's PC (the move/math that produced the value).
    HistoryTaint& operator=(const HistoryTaint& o) {
        cannot_branch = o.cannot_branch;
        cannot_index = o.cannot_index;
        cannot_var_time_op = o.cannot_var_time_op;
        history = o.history;
        note_op_pc();
        return *this;
    }
};

// --history=N (N > 1) / --history=full taint: the same three enforcement
// bits plus a bounded history of STEPS, where each step is the recorded
// guest stack (innermost PC first, via zeg_history_record_stack) of the
// operation that produced or consumed the poison. Same recording rules and
// memory-side per-byte provenance as HistoryTaint (the poison map stores
// steps for every history mode). Recorded PCs are raw and pre-symbolication;
// BFD symbolication and DWARF inline reconstruction happen at print time.
// The per-step frame cap is runtime (--history=N; 0 = full).

// Trail append with set-union semantics for DeepHistoryTaint (a step already
// present anywhere in the trail is not re-added; cap keeps most recent).
inline void zeg_trail_add_step(std::vector<std::vector<uint64_t>>& trail,
                               std::vector<uint64_t> step);

struct DeepHistoryTaint {
    static constexpr bool has_history = true;
    static constexpr bool records_frames = true;
    bool cannot_branch = false;
    bool cannot_index = false;
    bool cannot_var_time_op = false;
    std::vector<std::vector<uint64_t>> history; // steps, oldest first; each step = recorded stack, innermost PC first
    // Step capacity: when full, adding a new step drops the OLDEST step.
    static constexpr size_t kMaxHistory = 4096;
    DeepHistoryTaint() = default;
    bool any() const {
        return cannot_branch || cannot_index || cannot_var_time_op;
    }
    bool empty() const { return !any(); }
    void clear() {
        cannot_branch = false;
        cannot_index = false;
        cannot_var_time_op = false;
        history.clear();
    }
    // Record the current instruction's stack if this taint is poisoned.
    void note_op_pc() {
        if (!any())
            return;
        std::vector<uint64_t> step;
        zeg_history_record_stack(step);
        zeg_trail_add_step(history, std::move(step));
    }
    void merge(const DeepHistoryTaint& o) {
        cannot_branch = cannot_branch || o.cannot_branch;
        cannot_index = cannot_index || o.cannot_index;
        cannot_var_time_op = cannot_var_time_op || o.cannot_var_time_op;
        // Set-union the other trail's steps in: two operands with the same
        // provenance (or N bytes loaded from identically-provenanced bytes)
        // merge to the distinct set of operations instead of N concatenated
        // copies.
        for (const std::vector<uint64_t>& step : o.history)
            zeg_trail_add_step(history, step);
    }
    DeepHistoryTaint operator|(const DeepHistoryTaint& o) const {
        DeepHistoryTaint r = *this;
        r.merge(o);
        return r; // temp only: the op stack is recorded when this is assigned
    }
    DeepHistoryTaint& operator|=(const DeepHistoryTaint& o) {
        merge(o);
        note_op_pc();
        return *this;
    }
    // Assignment into a value's sidecar: adopt the source's provenance and
    // record this operation's stack (the move/math that produced the value).
    DeepHistoryTaint& operator=(const DeepHistoryTaint& o) {
        cannot_branch = o.cannot_branch;
        cannot_index = o.cannot_index;
        cannot_var_time_op = o.cannot_var_time_op;
        history = o.history;
        note_op_pc();
        return *this;
    }
};

inline void zeg_trail_add_step(std::vector<std::vector<uint64_t>>& trail,
                               std::vector<uint64_t> step) {
    if (std::find(trail.begin(), trail.end(), step) != trail.end())
        return;
    if (trail.size() >= DeepHistoryTaint::kMaxHistory)
        trail.erase(trail.begin());
    trail.push_back(std::move(step));
}

// Append pc to a poison trail unless it is already present anywhere in it
// (union semantics; first-occurrence order preserves the chronology of
// first use). This is what keeps merged trails from inflating: N bytes with
// identical provenance, or two operands with the same provenance, merge to
// the distinct set of operations instead of N concatenated copies. When the
// trail is saturated it keeps the MOST RECENT entries: adding a new PC drops
// the oldest one (so the faulting site is never silently frozen out).
inline void zeg_trail_add(std::vector<uint64_t>& trail, uint64_t pc) {
    if (std::find(trail.begin(), trail.end(), pc) != trail.end())
        return;
    if (trail.size() >= HistoryTaint::kMaxHistory)
        trail.erase(trail.begin());
    trail.push_back(pc);
}

// Common memory pipeline. Guest VA == host VA, so the fast path is memcpy,
// but every access goes through here so poisoning / fault translation can
// hook in one place. All functions take a CPU* for future per-thread checks.
// The API is templated on the taint sidecar type (Taint, or HistoryTaint in
// --history mode); both instantiations are explicitly defined in mem.cpp.
//
// Faults (host SIGSEGV/SIGBUS on the access) are translated into a guest
// core dump via the fault-recovery mechanism (see mem.cpp); they never
// propagate as host crashes.

// Scalar loads/stores.
template<typename TaintT>
uint8_t mem_load8(CPU<TaintT>* cpu, uint64_t addr);
template<typename TaintT>
uint16_t mem_load16(CPU<TaintT>* cpu, uint64_t addr);
template<typename TaintT>
uint32_t mem_load32(CPU<TaintT>* cpu, uint64_t addr);
template<typename TaintT>
uint64_t mem_load64(CPU<TaintT>* cpu, uint64_t addr);
template<typename TaintT>
void mem_store8(CPU<TaintT>* cpu, uint64_t addr, uint8_t v);
template<typename TaintT>
void mem_store16(CPU<TaintT>* cpu, uint64_t addr, uint16_t v);
template<typename TaintT>
void mem_store32(CPU<TaintT>* cpu, uint64_t addr, uint32_t v);
template<typename TaintT>
void mem_store64(CPU<TaintT>* cpu, uint64_t addr, uint64_t v);

// Wide vector loads/stores.
template<typename TaintT>
void mem_load_bytes(CPU<TaintT>* cpu, uint64_t addr, uint8_t* out, size_t n);
template<typename TaintT>
void mem_store_bytes(CPU<TaintT>* cpu, uint64_t addr, const uint8_t* in,
                     size_t n);

// Fetch up to 15 bytes of guest code at rip into buf. Returns number of
// bytes that could be read (caller needs >= 1; short reads only happen if
// the page tail is unmapped, in which case a fault is raised).
template<typename TaintT>
size_t mem_read_code(CPU<TaintT>* cpu, uint64_t rip, uint8_t* buf);

// Raw byte copy helpers used by string ops (rep movs etc.).
template<typename TaintT>
void mem_copy(CPU<TaintT>* cpu, uint64_t dst, uint64_t src, size_t n);

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
    // Tainting a value with this bit makes it unusable as an operand that
    // makes an instruction's execution time data-dependent (other than
    // through branching or indexing, which have their own bits).
    POISON_CANNOT_VAR_TIME_OP = 1u << 6,
    // A store to memory carrying this bit clears the CANNOT_VAR_TIME_OP bit
    // (store sanitizes), mirroring the branch/index clear bits.
    POISON_CLEAR_VAR_TIME_OP_ON_STORE = 1u << 7,
};
void mem_poison_range(Emulator* emu, uint64_t addr, size_t size, unsigned kind);
// Debug: watch stores to [base, base+len) (enabled by ZEG_WATCH_TLS).
void mem_watch_hint(uint64_t base, uint64_t len);
void mem_unpoison_range(Emulator* emu, uint64_t addr, size_t size);
// True when the poison map has any entries. Used to gate kernel-write
// modeling in syscall.cpp: a kernel overwrite can only launder taint that
// exists, so poison-free runs skip the whole extent-computation step (and
// can never hit its reject-by-crash path) at zero cost.
bool mem_poison_any();
// mremap support: the guest data at [old_addr, old_addr+old_size) MOVED to
// [new_addr, new_addr+new_size) (old_size == new_size for the move part of
// mremap; growth beyond it is fresh and callers clear it separately). The
// poison must move with the data: erase the old range, erase any stale
// poison at the destination, then reinsert each old entry shifted to the
// new base with its bits AND its poison-history trail intact.
void mem_move_poison(Emulator* emu, uint64_t old_addr, size_t old_size,
                     uint64_t new_addr, size_t new_size);

// Per-access poison checks. Throw (via guest_error) on violation.
template<typename TaintT>
void mem_check_load(CPU<TaintT>* cpu, uint64_t addr, size_t size);
template<typename TaintT>
void mem_check_store(CPU<TaintT>* cpu, uint64_t addr, size_t size);

// Taint side of poisoning: OR the taint bits (cannot-branch, cannot-index,
// cannot-var-time-op) of [addr,size) into *out (cleared if unpoisoned). Used
// to taint loaded register values. In --history mode the out taint's trail
// is seeded from the set-unioned per-byte poison history over the loaded
// range (see mem_note_store) plus the current instruction (the load site); a
// clean value (no bits) carries no trail.
template<typename TaintT>
void mem_get_taint(uint64_t addr, size_t size, TaintT* out);
template<typename TaintT>
inline void mem_get_taint(uint64_t addr, size_t size, TaintT& out) {
    mem_get_taint(addr, size, &out);
}

// Called after a successful store of a value with taint t to
// [addr,size): the destination bytes gain the value's taint bits
// (plus the matching clear-on-store bit, so a later store clears the
// taint). If the destination already carries a clear-on-store bit for a
// tainted value, that value bit is cleared instead (store sanitizes).
// Load/store poison bits and clear bits are sticky.
// In --history mode the destination bytes' poison history is rewritten too:
// a store of a tainted value records the stored value's trail plus the
// store PC into each destination byte (the stored value's provenance now
// owns those bytes), a store that zeroes a byte's last taint bit erases the
// byte's history with its bits entry, and a clean store leaves the history
// alone (the sticky CANNOT_LOAD/CANNOT_STORE bits keep the original poison
// provenance meaningful).
template<typename TaintT>
void mem_note_store(CPU<TaintT>* cpu, uint64_t addr, size_t size, TaintT t);

// Poisoning client request (magic call 0x1410141014101410):
//   RDI=op, RSI=ptr, RDX=size, RCX=flags.
// op 0 = no-op/query (returns 0); op 1 = poison with flags bitmask
// (POISON_* bits 1:1); op 2 = unpoison range. Returns the RAX result.
template<typename TaintT>
uint64_t mem_client_request(Emulator* emu, CPU<TaintT>* cpu, uint64_t op,
                            uint64_t ptr, uint64_t size, uint64_t flags);

// Install host SIGSEGV/SIGBUS/SIGILL/SIGFPE handler that redirects
// emulator-thread faults into the guest core-dump path. Called once at
// startup. Does nothing when passthrough mode is set.
void mem_install_fault_handler();

// Passthrough mode (--no-core / --passthrough-signals): do not install
// handlers and do not write core dumps (guest errors just print + exit).
void mem_set_passthrough(bool v);
bool mem_passthrough();

// While the calling thread is producing a crash dump, host faults in dump
// code kill the process instead of jumping back into the interpreter loop
// (which would re-enter the dump and livelock); fault-guarded
// guest-memory copies keep recovering locally. Per-thread state: other
// guest threads keep their own loop recovery while one thread dumps.
// Set for the duration of dump_core().
void mem_set_dumping(bool v);
bool mem_dumping();

// Fault recovery scope: set around the interpreter loop body per instruction.
// (The stored pointer is only null-checked by the fault handler; it is
// type-erased so the scope works for either CPU instantiation.)
struct FaultScope {
    const void* cpu;
    bool armed = false;
    FaultScope(const void* c);
    ~FaultScope();
};

// Loop-level recovery scope: arm around exec_one() in run_cpu_loop so host
// SIGSEGV/SIGBUS/SIGILL/SIGFPE raised outside a mem accessor (or inside one
// for ILL/FPE) become guest errors at the current guest RIP.
struct LoopScope {
    const void* cpu = nullptr;
    bool prev_armed = false;
    const void* prev_cpu = nullptr;
    LoopScope(const void* c);
    ~LoopScope();
};

// If the LoopScope captured a host signal, returns true and fills signo
// (e.g. SIGSEGV) and the fault address (si_addr, 0 if none). Clears pending.
bool mem_loop_fault(int* signo, uint64_t* addr);

// Raised on any fatal guest error; caught at the top of the thread loop
// which then dumps core. Templated on the taint type; carries the offending
// value's poison history (--history mode; empty for plain Taint).
template<typename TaintT>
struct GuestError {
    CPU<TaintT>* cpu;
    uint64_t fault_addr = 0;
    const char* reason = nullptr;
    // The offending value's recorded trail, one step per recorded operation
    // (--history mode; empty for plain Taint).
    std::vector<std::vector<uint64_t>> poison_history;
};
template<typename TaintT>
[[noreturn]] void guest_error(CPU<TaintT>* cpu, const char* reason,
                              uint64_t fault_addr = 0,
                              std::vector<std::vector<uint64_t>> poison_history = {});
