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

// The global execution sequence of the instruction whose PC is in
// zeg_history_pc: set right next to it at exec_one entry (history taints
// only), incremented once per recorded instruction by a global counter so
// sequences stay comparable across threads (trails can cross threads via
// the shared poison map's per-byte trails and still sort correctly). Trails
// record it with every step so the poison-history dump can restore true
// chronological order -- set-union merges assemble trails out of execution
// order (see ZegHistStep/zeg_trail_add_pc).
extern thread_local uint64_t zeg_history_seq;

// Bump and return the global execution-sequence counter (see zeg_history_seq).
// Defined in mem.cpp; only history-mode exec_one entries call it (plain Taint
// never touches the counter -- zero overhead in plain mode).
uint64_t zeg_history_seq_next();

// One recorded poison-history step. `pc` is the operation's instruction PC
// (where the operation physically is); in deep history modes (--history=N/full)
// `callers` holds the rest of the recorded stack, innermost first (empty in
// --history=1). `seq` is the global execution sequence of the instruction that
// recorded the step: trails are assembled by set-union merges that run out of
// execution order, and the seq is what restores true chronological order.
struct ZegHistStep {
    uint64_t pc = 0;
    uint64_t seq = 0;
    std::vector<uint64_t> callers; // empty in --history=1
};

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
// history of the operations (guest PCs, and each step's recording seq) that
// produced or consumed the poison (loads from poisoned memory, moves/merges
// into the value's sidecar, stores of the value). Recording is driven by the
// value-assignment operations themselves (operator=/operator|=) plus
// mem_get_taint (taint from poisoned memory) and the engine's note_store
// wrapper (stores), against the thread-local current-instruction PC and its
// execution seq set at exec_one entry. Recording spans memory too: the
// poison map carries a per-byte trail that poisoning requests union their
// call site into and stores of tainted values rewrite, and a load seeds the
// value's trail from the set-unioned per-byte trails of the bytes it reads
// (plus the load PC), so the provenance survives memory round trips. The
// trail vector's order is merge/insertion order, not execution order; the
// dump sorts by the steps' seq (the recording instruction's global execution
// sequence) to print true chronological order.
// The history is printed (BFD-symbolicated) when a taint check faults.

// Trail append with seq-aware set-union semantics, shared by every site that
// puts a step into a trail (HistoryTaint::note_op_pc/merge below, and the
// memory-side per-byte trails in mem.cpp). Defined after HistoryTaint (it
// needs kMaxHistory); forward-declared here so the member bodies below can
// use it. Mode 1 trails hold single-PC steps: identity is the step's pc.
inline void zeg_trail_add_pc(std::vector<ZegHistStep>& trail, uint64_t pc);
// Same, unioning an already-recorded step (identity: step.pc; the step
// carries its own recording seq).
inline void zeg_trail_add_pc(std::vector<ZegHistStep>& trail,
                             const ZegHistStep& step);

struct HistoryTaint {
    static constexpr bool has_history = true;
    // Trait flag: mode-1 history records one PC per step, no stack frames.
    static constexpr bool records_frames = false;
    bool cannot_branch = false;
    bool cannot_index = false;
    bool cannot_var_time_op = false;
    std::vector<ZegHistStep> history; // one step per recorded op (pc + seq)
    // Trail capacity: when full, adding a new step drops the entry with the
    // SMALLEST seq -- the true oldest, however merges ordered it -- so the
    // trail keeps the MOST RECENT operations (the faulting site is never
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
    // semantics via zeg_trail_add_pc: a PC already present anywhere in the
    // trail is not re-added -- its seq is just refreshed to the most recent
    // execution -- which collapses repeats from loops; the cap keeps the
    // most recent entries).
    void note_op_pc() {
        if (!any())
            return;
        zeg_trail_add_pc(history, zeg_history_pc);
    }
    void merge(const HistoryTaint& o) {
        cannot_branch = cannot_branch || o.cannot_branch;
        cannot_index = cannot_index || o.cannot_index;
        cannot_var_time_op = cannot_var_time_op || o.cannot_var_time_op;
        // Set-union the other trail in: two operands with the same
        // provenance (or N bytes loaded from identically-provenanced bytes)
        // merge to the distinct set of operations instead of N concatenated
        // copies. The union is keyed on the step PCs, so the merged vector's
        // order is merge order -- NOT execution order -- and the dump sorts
        // by the steps' seq to restore chronology.
        for (const ZegHistStep& step : o.history)
            zeg_trail_add_pc(history, step);
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

// Trail append with seq-aware set-union semantics for DeepHistoryTaint (a
// step already present anywhere in the trail is not re-added -- its seq is
// just refreshed; cap keeps the most recent seq). Deep steps are identified
// by the whole recorded stack: pc plus callers.
inline void zeg_trail_add_stack(std::vector<ZegHistStep>& trail,
                                std::vector<uint64_t> pcs);
// Same, unioning an already-recorded step (identity: pc + callers; the step
// carries its own recording seq).
inline void zeg_trail_add_stack(std::vector<ZegHistStep>& trail,
                                const ZegHistStep& step);

struct DeepHistoryTaint {
    static constexpr bool has_history = true;
    static constexpr bool records_frames = true;
    bool cannot_branch = false;
    bool cannot_index = false;
    bool cannot_var_time_op = false;
    std::vector<ZegHistStep> history; // one step per recorded op (pc + callers + seq)
    // Step capacity: when full, adding a new step drops the entry with the
    // SMALLEST seq (the true oldest, however merges ordered it).
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
        std::vector<uint64_t> stack;
        zeg_history_record_stack(stack);
        zeg_trail_add_stack(history, std::move(stack));
    }
    void merge(const DeepHistoryTaint& o) {
        cannot_branch = cannot_branch || o.cannot_branch;
        cannot_index = cannot_index || o.cannot_index;
        cannot_var_time_op = cannot_var_time_op || o.cannot_var_time_op;
        // Set-union the other trail's steps in: two operands with the same
        // provenance (or N bytes loaded from identically-provenanced bytes)
        // merge to the distinct set of operations instead of N concatenated
        // copies. The union is keyed on the whole recorded stacks, so the
        // merged vector's order is merge order -- NOT execution order -- and
        // the dump sorts by the steps' seq to restore chronology.
        for (const ZegHistStep& step : o.history)
            zeg_trail_add_stack(history, step);
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

// Shared seq-aware union core (see the zeg_trail_add_pc/zeg_trail_add_stack
// wrappers below): identity is (pc, callers). A step already present ANYWHERE
// in the trail is not re-added; instead its seq is raised to the max of the
// existing and the new seq -- the most recent execution of that operation in
// this lineage, matching the trail's most-recent philosophy. Otherwise the
// step is appended. This is what keeps merged trails from inflating: N bytes
// with identical provenance, or two operands with the same provenance, merge
// to the distinct set of operations instead of N concatenated copies. When
// the trail is saturated it keeps the MOST RECENT executions: adding a new
// step drops the entry with the SMALLEST seq -- the true oldest, even though
// the vector's insertion order is merge order (merges assemble trails out of
// execution order, which is exactly why the seq exists: the dump sorts by it
// to restore chronology).
inline void zeg_trail_add_seq(std::vector<ZegHistStep>& trail, uint64_t pc,
                              std::vector<uint64_t>&& callers, uint64_t seq) {
    for (ZegHistStep& st : trail) {
        if (st.pc == pc && st.callers == callers) {
            if (seq > st.seq)
                st.seq = seq;
            return;
        }
    }
    if (trail.size() >= HistoryTaint::kMaxHistory) {
        // Evict the smallest seq (the oldest execution); linear scan is fine
        // here (crash-debug path, cap 4096).
        size_t oldest = 0;
        for (size_t i = 1; i < trail.size(); i++)
            if (trail[i].seq < trail[oldest].seq)
                oldest = i;
        trail.erase(trail.begin() + (long)oldest);
    }
    trail.push_back(ZegHistStep{pc, seq, std::move(callers)});
}

// Mode 1 (--history=1): a new step for the CURRENT instruction (pc with the
// current zeg_history_seq, no callers).
inline void zeg_trail_add_pc(std::vector<ZegHistStep>& trail, uint64_t pc) {
    zeg_trail_add_seq(trail, pc, {}, zeg_history_seq);
}

// Mode 1: union an already-recorded single-PC step (identity: its pc; its
// own recorded seq rides along).
inline void zeg_trail_add_pc(std::vector<ZegHistStep>& trail,
                             const ZegHistStep& step) {
    zeg_trail_add_seq(trail, step.pc, {}, step.seq);
}

// Deep modes (--history=N/full): a new step for the CURRENT instruction.
// `pcs` is the recorded stack, innermost PC first (frame 0 =
// zeg_history_pc, as zeg_history_record_stack fills it); the step's pc is
// frame 0 and callers holds the rest.
inline void zeg_trail_add_stack(std::vector<ZegHistStep>& trail,
                                std::vector<uint64_t> pcs) {
    if (pcs.empty())
        return;
    uint64_t pc = pcs.front();
    pcs.erase(pcs.begin());
    zeg_trail_add_seq(trail, pc, std::move(pcs), zeg_history_seq);
}

// Deep modes: union an already-recorded stack step (identity: pc + the whole
// recorded stack; its own recorded seq rides along).
inline void zeg_trail_add_stack(std::vector<ZegHistStep>& trail,
                                const ZegHistStep& step) {
    zeg_trail_add_seq(trail, step.pc,
                      std::vector<uint64_t>(step.callers), step.seq);
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
// [addr,size): for each taint dimension set in t, the destination bytes
// keep/gain that poison bit (and gain the matching clear-on-store bit if
// they were not already poisoned in that dimension, so a later clean store
// can sanitize them). For each dimension clean in t, a clear-on-store bit
// on the destination clears the corresponding poison bit.
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

// Passthrough mode (--passthrough-signals only): do not install host
// signal handlers AND do not translate host signals into guest errors, so
// a fault kills the process like a real host fault would (GuestError
// paths -- poison violations, unimplemented instructions, modeled
// arithmetic faults -- still print their banner and exit normally).
// Implies no core dumps. The core FILE write is gated separately by
// Emulator::no_core (--no-core / config `coredump disabled` / the
// default): the fault handler stays installed there, so the banner +
// symbolized guest stack trace still print and only the file is skipped.
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
    // (--history mode; empty for plain Taint). Steps carry their recording
    // seq; the dump sorts by it to restore chronological order.
    std::vector<ZegHistStep> poison_history;
};
template<typename TaintT>
[[noreturn]] void guest_error(CPU<TaintT>* cpu, const char* reason,
                              uint64_t fault_addr = 0,
                              std::vector<ZegHistStep> poison_history = {});
