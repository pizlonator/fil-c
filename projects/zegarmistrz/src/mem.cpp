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

#include "mem.h"

#include <csetjmp>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>

#include "cpu.h"
#include "emulator.h"

// Current instruction's guest PC for --history poison-history recording
// (see HistoryTaint/DeepHistoryTaint in mem.h). Set once per instruction by
// the engine's exec_one entry (pre-RIP-advance); only the history taint
// types ever read it (HistoryTaint as the step PC, DeepHistoryTaint as each
// step's frame 0).
thread_local uint64_t zeg_history_pc = 0;

// --history=N/full stack recording inputs (see DeepHistoryTaint in mem.h):
// the exec_one entry of a records_frames instantiation snapshots the guest
// GPRs and the Emulator so zeg_history_record_stack can unwind the current
// stack (dwarf_unwind.cpp).
thread_local uint64_t zeg_history_regs[16];
thread_local Emulator* zeg_history_emu = nullptr;

// --- fault recovery --------------------------------------------------------
// Each emulator thread arms a thread_local recovery point while it touches
// guest memory. A host SIGSEGV/SIGBUS on that thread longjmps back out and
// the access raises a GuestError (guest SIGSEGV semantics).

namespace {
thread_local sigjmp_buf t_recovery_buf;
thread_local bool t_recovery_armed = false;
thread_local uint64_t t_recovery_fault = 0;

// Loop-level recovery: armed by LoopScope around exec_one(). Catches host
// signals raised anywhere in guest execution (including inside JIT-free
// inline asm like RDRAND) and routes them to the guest core-dump path.
thread_local sigjmp_buf t_loop_buf;
thread_local bool t_loop_armed = false;
// Type-erased CPU pointer (CPU<Taint> or CPU<HistoryTaint>): the fault
// handler only null-checks it, never dereferences it.
thread_local const void* t_loop_cpu = nullptr;
thread_local int t_loop_sig = 0;
thread_local uint64_t t_loop_addr = 0;
thread_local bool t_loop_pending = false;
} // namespace

static bool g_passthrough = false;

void mem_set_passthrough(bool v) { g_passthrough = v; }
bool mem_passthrough() { return g_passthrough; }

// Crash-dump policy: while a dump (core writer + BFD guest stack trace) is
// being produced, host faults in dump code must kill the process instead of
// longjmp-ing back into the interpreter loop -- dump_core() runs with the
// LoopScope recovery still armed, so the loop recovery would re-enter the
// dump and livelock. Per-thread: other guest threads keep executing (and
// keep their own loop recovery) while one thread dumps, so only the
// dumping thread's fault handling may change -- and thread_local needs no
// atomics because fault_handler only ever reads the dumping thread's own
// flag. Fault-guarded guest-memory copies still recover locally: their
// recovery point is checked first in fault_handler().
static thread_local bool g_dumping = false;

void mem_set_dumping(bool v) { g_dumping = v; }
bool mem_dumping() { return g_dumping; }

static void fault_handler(int sig, siginfo_t* info, void*) {
    if ((sig == SIGSEGV || sig == SIGBUS) && t_recovery_armed) {
        t_recovery_fault =
            info && info->si_addr ? (uint64_t)(uintptr_t)info->si_addr : 0;
        siglongjmp(t_recovery_buf, 1);
    }
    // While this thread is producing a crash dump, the interpreter-loop recovery
    // must not fire: dump_core() runs with LoopScope still armed, so a
    // fault in dump code (BFD parsing, the demangler, ...) would longjmp
    // back into the loop and re-enter the dump forever. Fall through to
    // the re-raise below and die for real. Fault-guarded guest-memory
    // copies still recover locally (checked above).
    if (!g_dumping && t_loop_armed && t_loop_cpu) {
        t_loop_sig = sig;
        t_loop_addr =
            info && info->si_addr ? (uint64_t)(uintptr_t)info->si_addr : 0;
        siglongjmp(t_loop_buf, 1);
    }
    // Not inside emulator execution: re-raise with default disposition.
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sigaction(sig, &sa, nullptr);
    raise(sig);
}

void mem_install_fault_handler() {
    if (g_passthrough)
        return;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = fault_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_NODEFER;
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);
    sigaction(SIGFPE, &sa, nullptr);
}

LoopScope::LoopScope(const void* c) : cpu(c) {
    prev_armed = t_loop_armed;
    prev_cpu = t_loop_cpu;
    if (sigsetjmp(t_loop_buf, 1))
        t_loop_pending = true;
    t_loop_armed = true;
    t_loop_cpu = c;
}

LoopScope::~LoopScope() {
    t_loop_armed = prev_armed;
    t_loop_cpu = prev_cpu;
}

bool mem_loop_fault(int* signo, uint64_t* addr) {
    if (!t_loop_pending)
        return false;
    t_loop_pending = false;
    if (signo)
        *signo = t_loop_sig;
    if (addr)
        *addr = t_loop_addr;
    t_loop_sig = 0;
    t_loop_addr = 0;
    return true;
}

FaultScope::FaultScope(const void* c) : cpu(c) {}
FaultScope::~FaultScope() {}

template<typename TaintT>
[[noreturn]] void guest_error(CPU<TaintT>* cpu, const char* reason,
                              uint64_t fault_addr,
                              std::vector<std::vector<uint64_t>> poison_history) {
    GuestError<TaintT> e;
    e.cpu = cpu;
    e.reason = reason;
    e.fault_addr = fault_addr;
    e.poison_history = std::move(poison_history);
    throw e;
}

// --- poison map ------------------------------------------------------------
// Phase 2: per-byte poison bits in a mutex-guarded std::map. Checked on
// every access; client requests via the magic call address populate it.
//
// Flag bitmask (see mem.h POISON_*):
//   bit0 cannot-load, bit1 cannot-store, bit2 cannot-branch,
//   bit3 cannot-index, bit4 clear-branch-on-store, bit5 clear-index-on-store,
//   bit6 cannot-var-time-op, bit7 clear-var-time-op-on-store.

static std::mutex g_poison_mu;
static std::map<uint64_t, uint8_t> g_poison; // key: byte address

// --history: per-byte poison provenance (only populated when a history
// instantiation is running; plain mode never touches it). Each poisoned
// byte carries the trail of the operations that poisoned it: the poisoning
// client request unions the magic CALL's step into it (only for ranges
// poisoned with a value-taint flag; a byte erased by unpoison starts fresh),
// a store of a tainted value replaces it with the stored value's trail plus
// the store PC's step, a clean store leaves it alone (the
// CANNOT_LOAD/CANNOT_STORE bits are sticky, so the original poison
// provenance stays meaningful), and unpoison erases it. mem_get_taint seeds
// the loaded value's trail from the set-unioned per-byte trails over the
// loaded range, plus the load PC's step. For every history mode each byte's
// trail is a list of STEPS -- a step is a single PC for --history=1, or a
// recorded stack for --history=N/full.
static std::map<uint64_t, std::vector<std::vector<uint64_t>>> g_poison_hist; // key: byte address

void mem_poison_range(Emulator*, uint64_t addr, size_t size, unsigned kind) {
    if (!size)
        return;
    kind &= 0xff;
    if (!kind)
        return;
    std::lock_guard<std::mutex> l(g_poison_mu);
    for (size_t i = 0; i < size; i++)
        g_poison[addr + i] |= (uint8_t)kind;
}

void mem_unpoison_range(Emulator*, uint64_t addr, size_t size) {
    if (!size)
        return;
    std::lock_guard<std::mutex> l(g_poison_mu);
    auto it = g_poison.lower_bound(addr);
    // Avoid overflow when addr+size wraps.
    uint64_t end = addr + size;
    if (end < addr)
        end = UINT64_MAX;
    while (it != g_poison.end() && it->first < end)
        it = g_poison.erase(it);
    // --history: erase the bytes' poison provenance too (a no-op on an
    // empty map in plain mode).
    auto hit = g_poison_hist.lower_bound(addr);
    while (hit != g_poison_hist.end() && hit->first < end)
        hit = g_poison_hist.erase(hit);
}

bool mem_poison_any() {
    std::lock_guard<std::mutex> l(g_poison_mu);
    return !g_poison.empty();
}

// See mem.h: mremap moved the bytes [old_addr, old_addr+old_size) to
// [new_addr, new_addr+new_size), so the poison entries move with them.
// One lock over the whole operation so no access slips between the erase
// and the reinsert.
void mem_move_poison(Emulator*, uint64_t old_addr, size_t old_size,
                     uint64_t new_addr, size_t new_size) {
    if (old_addr == new_addr)
        return;
    if (!old_size || !new_size)
        return;
    std::lock_guard<std::mutex> l(g_poison_mu);
    // Overflow-guard the range ends like mem_unpoison_range does.
    uint64_t old_end = old_addr + old_size;
    if (old_end < old_addr)
        old_end = UINT64_MAX;
    uint64_t new_end = new_addr + new_size;
    if (new_end < new_addr)
        new_end = UINT64_MAX;
    // Snapshot the entries (bits + history trails) in the old range.
    std::vector<std::pair<uint64_t, uint8_t>> moved;
    std::vector<std::pair<uint64_t, std::vector<std::vector<uint64_t>>>>
        moved_hist;
    auto it = g_poison.lower_bound(old_addr);
    while (it != g_poison.end() && it->first < old_end) {
        moved.emplace_back(it->first, it->second);
        auto hit = g_poison_hist.find(it->first);
        if (hit != g_poison_hist.end())
            moved_hist.emplace_back(it->first, hit->second);
        ++it;
    }
    if (moved.empty())
        return;
    // Erase the old range, then any stale poison at the destination (the
    // moved data replaces whatever used to live there).
    it = g_poison.lower_bound(old_addr);
    while (it != g_poison.end() && it->first < old_end)
        it = g_poison.erase(it);
    auto hit = g_poison_hist.lower_bound(old_addr);
    while (hit != g_poison_hist.end() && hit->first < old_end)
        hit = g_poison_hist.erase(hit);
    it = g_poison.lower_bound(new_addr);
    while (it != g_poison.end() && it->first < new_end)
        it = g_poison.erase(it);
    hit = g_poison_hist.lower_bound(new_addr);
    while (hit != g_poison_hist.end() && hit->first < new_end)
        hit = g_poison_hist.erase(hit);
    // Reinsert each entry shifted to the new base, bits AND trail intact.
    for (const auto& e : moved)
        g_poison[new_addr + (e.first - old_addr)] = e.second;
    for (const auto& e : moved_hist)
        g_poison_hist[new_addr + (e.first - old_addr)] = e.second;
}

template<typename TaintT>
uint64_t mem_client_request(Emulator* emu, CPU<TaintT>* cpu, uint64_t op,
                            uint64_t ptr, uint64_t size, uint64_t flags) {
    (void)cpu;
    if (op == 0)
        return 0; // no-op/query
    if (op == 1) {
        mem_poison_range(emu, ptr, (size_t)size, (unsigned)flags);
        if constexpr (TaintT::has_history) {
            // Seed each byte's provenance with the poisoning call site: the
            // magic CALL's own RIP (exec_one sets zeg_history_pc pre-advance
            // and exec_call intercepts the magic target before advancing).
            // Only ranges poisoned with a value-taint flag ever produce
            // tainted values (CANNOT_LOAD/CANNOT_STORE-only or CLEAR_*-only
            // ranges never set a taint bit), so skip the seeding for those.
            // Union semantics: re-poisoning a byte ORs the new poison site
            // into its provenance instead of replacing it, matching
            // mem_poison_range's bit-OR; a byte erased by op 2 starts fresh
            // with the new call site.
            unsigned kind = (unsigned)flags & 0xff;
            if (size && (kind & (POISON_CANNOT_BRANCH |
                                 POISON_CANNOT_INDEX |
                                 POISON_CANNOT_VAR_TIME_OP))) {
                std::vector<uint64_t> step;
                if constexpr (TaintT::records_frames)
                    zeg_history_record_stack(step);
                else
                    step.push_back(zeg_history_pc);
                std::lock_guard<std::mutex> l(g_poison_mu);
                for (size_t i = 0; i < (size_t)size; i++)
                    zeg_trail_add_step(g_poison_hist[ptr + i], step);
            }
        }
        return 0;
    }
    if (op == 2) {
        mem_unpoison_range(emu, ptr, (size_t)size);
        return 0;
    }
    guest_error(cpu, "bad poisoning client request (unknown op)");
}

static uint64_t g_watch_base = 0;
static uint64_t g_watch_len = 0;
static bool g_watch_init = false;

void mem_watch_hint(uint64_t base, uint64_t len) {
    g_watch_base = base;
    g_watch_len = len;
}

static uint64_t g_watch_vlo = 0, g_watch_vhi = 0;
template<typename TaintT>
static inline void watch_check(CPU<TaintT>* cpu, uint64_t addr, size_t size,
                               uint64_t val) {
    if (g_watch_init && g_watch_vlo == 0) {
        const char* e = getenv("ZEG_WATCH_VAL");
        if (e) {
            unsigned long long a = 0, b = 0;
            if (sscanf(e, "%llx-%llx", &a, &b) == 2) {
                g_watch_vlo = a;
                g_watch_vhi = b;
            }
        }
        // Re-arm once: use a separate flag via vlo==0 check is wrong if
        // range starts at 0; acceptable for debug.
        if (!getenv("ZEG_WATCH_VAL"))
            g_watch_vlo = ~0ULL;
    }
    if (!g_watch_init) {
        g_watch_init = true;
        if (!getenv("ZEG_WATCH_TLS")) {
            g_watch_len = 0;
            return;
        }
    }
    if (g_watch_vlo != ~0ULL && (uint32_t)val >= (uint32_t)g_watch_vlo &&
        (uint32_t)val <= (uint32_t)g_watch_vhi) {
        fprintf(stderr, "[watchval:tid=%d rip=%#lx] store%zu [%#lx] <- %#lx\n",
                cpu ? cpu->tid : -1, cpu ? cpu->rip : 0, size, addr, val);
        fflush(stderr);
    }
    if (!g_watch_len)
        return;
    if (addr + size > g_watch_base && addr < g_watch_base + g_watch_len) {
        fprintf(stderr,
                "[watch:tid=%d rip=%#lx] store%zu [%#lx] <- %#lx\n",
                cpu ? cpu->tid : -1, cpu ? cpu->rip : 0, size, addr, val);
        fflush(stderr);
    }
}

static unsigned poison_lookup(uint64_t addr, size_t size) {
    if (g_poison.empty())
        return 0;
    std::lock_guard<std::mutex> l(g_poison_mu);
    if (g_poison.empty())
        return 0;
    uint64_t end = addr + size;
    if (end < addr)
        end = UINT64_MAX;
    unsigned bits = 0;
    for (auto it = g_poison.lower_bound(addr);
         it != g_poison.end() && it->first < end; ++it) {
        bits |= it->second;
        if (bits == 0xff)
            break;
    }
    return bits;
}

// Bits plus the set-unioned per-byte poison history over [addr, addr+size),
// under one lock. hist_out may be null (plain mode). Unlike poison_lookup
// there is no early break: every byte's trail is needed for the merged
// provenance.
static unsigned poison_lookup_hist(uint64_t addr, size_t size,
                                   std::vector<std::vector<uint64_t>>* hist_out) {
    uint64_t end = addr + size;
    if (end < addr)
        end = UINT64_MAX;
    std::lock_guard<std::mutex> l(g_poison_mu);
    unsigned bits = 0;
    if (hist_out)
        hist_out->clear();
    for (auto it = g_poison.lower_bound(addr);
         it != g_poison.end() && it->first < end; ++it) {
        bits |= it->second;
        if (hist_out) {
            auto hit = g_poison_hist.find(it->first);
            if (hit != g_poison_hist.end()) {
                // Set-union the byte's trail into the merged provenance
                // (same rule as the history taints' merge): N bytes with
                // identical provenance yield the distinct set of
                // operations, not N concatenated copies.
                for (const std::vector<uint64_t>& step : hit->second)
                    zeg_trail_add_step(*hist_out, step);
            }
        }
    }
    return bits;
}

template<typename TaintT>
void mem_get_taint(uint64_t addr, size_t size, TaintT* out) {
    std::vector<std::vector<uint64_t>> mem_hist;
    unsigned bits = poison_lookup_hist(addr, size,
                                       TaintT::has_history ? &mem_hist
                                                           : nullptr);
    if (!out)
        return;
    out->cannot_branch = (bits & POISON_CANNOT_BRANCH) != 0;
    out->cannot_index = (bits & POISON_CANNOT_INDEX) != 0;
    out->cannot_var_time_op = (bits & POISON_CANNOT_VAR_TIME_OP) != 0;
    if constexpr (TaintT::has_history) {
        out->history.clear();
        if (out->any()) {
            if constexpr (TaintT::records_frames) {
                // The value inherits the poison provenance of the memory it
                // was loaded from (merged per-byte trails), plus this load's
                // recorded stack.
                out->history = std::move(mem_hist);
            } else {
                // --history=1: flatten the merged per-byte steps (each is one
                // PC) into the sidecar's flat trail, order preserved.
                for (const std::vector<uint64_t>& step : mem_hist)
                    if (!step.empty())
                        out->history.push_back(step[0]);
            }
            out->note_op_pc();
        }
    }
}

// Store taint semantics (see mem.h): the destination bytes gain the stored
// value's taint bits plus the matching clear-on-store bit, so
// a later store to a clear-on-store location sanitizes the value (and the
// memory). Load/store poison bits and clear bits are sticky.
// --history: a store of a tainted value also records the stored value's
// trail plus the store PC into the destination bytes' poison history (the
// stored value's provenance now owns those bytes), and a store that clears
// a byte's last taint bit (its bits reach 0) erases the byte's history with
// the bits entry; a clean store leaves the history alone (the sticky
// CANNOT_LOAD/CANNOT_STORE bits keep the original poison provenance
// meaningful).
template<typename TaintT>
void mem_note_store(CPU<TaintT>* cpu, uint64_t addr, size_t size, TaintT t) {
    (void)cpu;
    bool cb = t.cannot_branch, ci = t.cannot_index;
    bool cvt = t.cannot_var_time_op;
    if (!cb && !ci && !cvt) {
        // Clean store still honors clear-on-store: it sanitizes locations
        // that carry clear bits.
        std::lock_guard<std::mutex> l(g_poison_mu);
        if (g_poison.empty())
            return;
        uint64_t end = addr + size;
        if (end < addr)
            end = UINT64_MAX;
        auto it = g_poison.lower_bound(addr);
        while (it != g_poison.end() && it->first < end) {
            uint8_t old = it->second;
            uint8_t nu = old;
            if (old & POISON_CLEAR_BRANCH_ON_STORE)
                nu &= ~(uint8_t)POISON_CANNOT_BRANCH;
            if (old & POISON_CLEAR_INDEX_ON_STORE)
                nu &= ~(uint8_t)POISON_CANNOT_INDEX;
            if (old & POISON_CLEAR_VAR_TIME_OP_ON_STORE)
                nu &= ~(uint8_t)POISON_CANNOT_VAR_TIME_OP;
            if (nu != old) {
                if (nu) {
                    it->second = nu;
                } else {
                    uint64_t a = it->first;
                    it = g_poison.erase(it);
                    if constexpr (TaintT::has_history)
                        g_poison_hist.erase(a);
                }
                if (!nu)
                    continue;
            }
            ++it;
        }
        return;
    }
    std::vector<std::vector<uint64_t>> stored_hist;
    if constexpr (TaintT::has_history) {
        if constexpr (TaintT::records_frames) {
            stored_hist = t.history;
            // note_store_op already appended the store step to the sidecar it
            // copied from; be robust to direct callers too (union semantics).
            std::vector<uint64_t> store_step;
            zeg_history_record_stack(store_step);
            zeg_trail_add_step(stored_hist, std::move(store_step));
        } else {
            stored_hist.reserve(t.history.size());
            for (uint64_t pc : t.history)
                stored_hist.push_back({pc});
            zeg_trail_add_step(stored_hist, {zeg_history_pc});
        }
    }
    std::lock_guard<std::mutex> l(g_poison_mu);
    for (size_t i = 0; i < size; i++) {
        uint64_t a = addr + i;
        auto it = g_poison.find(a);
        uint8_t old = (it == g_poison.end()) ? 0 : it->second;
        uint8_t nu = old;
        if (cb) {
            if (old & POISON_CLEAR_BRANCH_ON_STORE)
                nu &= ~(uint8_t)POISON_CANNOT_BRANCH;
            else
                nu |= (uint8_t)(POISON_CANNOT_BRANCH |
                                POISON_CLEAR_BRANCH_ON_STORE);
        }
        if (ci) {
            if (old & POISON_CLEAR_INDEX_ON_STORE)
                nu &= ~(uint8_t)POISON_CANNOT_INDEX;
            else
                nu |= (uint8_t)(POISON_CANNOT_INDEX |
                                POISON_CLEAR_INDEX_ON_STORE);
        }
        if (cvt) {
            if (old & POISON_CLEAR_VAR_TIME_OP_ON_STORE)
                nu &= ~(uint8_t)POISON_CANNOT_VAR_TIME_OP;
            else
                nu |= (uint8_t)(POISON_CANNOT_VAR_TIME_OP |
                                POISON_CLEAR_VAR_TIME_OP_ON_STORE);
        }
        if (nu) {
            g_poison[a] = nu;
            if constexpr (TaintT::has_history)
                g_poison_hist[a] = stored_hist;
        } else if (it != g_poison.end()) {
            g_poison.erase(it);
            if constexpr (TaintT::has_history)
                g_poison_hist.erase(a);
        }
    }
}

template<typename TaintT>
void mem_check_load(CPU<TaintT>* cpu, uint64_t addr, size_t size) {
    unsigned bits = poison_lookup(addr, size);
    if (bits & POISON_CANNOT_LOAD)
        guest_error(cpu, "load from poisoned (cannot-load) memory", addr);
}

template<typename TaintT>
void mem_check_store(CPU<TaintT>* cpu, uint64_t addr, size_t size) {
    unsigned bits = poison_lookup(addr, size);
    if (bits & POISON_CANNOT_STORE)
        guest_error(cpu, "store to poisoned (cannot-store) memory", addr);
}

// --- raw accessors ---------------------------------------------------------

template <typename T> static inline T raw_load(uint64_t addr) {
    T v;
    memcpy(&v, (const void*)(uintptr_t)addr, sizeof(T));
    return v;
}

template <typename T> static inline void raw_store(uint64_t addr, T v) {
    memcpy((void*)(uintptr_t)addr, &v, sizeof(T));
}

#define WITH_RECOVERY(cpu_, addr_, expr_)                                              \
    ([&]() {                                                                           \
        mem_check_load(cpu_, addr_, sizeof(expr_));                                     \
        t_recovery_armed = true;                                                       \
        if (sigsetjmp(t_recovery_buf, 1)) {                                            \
            t_recovery_armed = false;                                                  \
            guest_error(cpu_, "guest SIGSEGV (unmapped read)", t_recovery_fault);     \
        }                                                                              \
        auto _v = (expr_);                                                             \
        t_recovery_armed = false;                                                      \
        return _v;                                                                     \
    })()

template<typename TaintT>
uint8_t mem_load8(CPU<TaintT>* cpu, uint64_t addr) {
    mem_check_load(cpu, addr, 1);
    t_recovery_armed = true;
    uint8_t v = 0;
    if (sigsetjmp(t_recovery_buf, 1)) {
        t_recovery_armed = false;
        guest_error(cpu, "guest SIGSEGV (unmapped read)", t_recovery_fault);
    }
    v = raw_load<uint8_t>(addr);
    t_recovery_armed = false;
    return v;
}

template<typename TaintT>
uint16_t mem_load16(CPU<TaintT>* cpu, uint64_t addr) {
    mem_check_load(cpu, addr, 2);
    t_recovery_armed = true;
    uint16_t v = 0;
    if (sigsetjmp(t_recovery_buf, 1)) {
        t_recovery_armed = false;
        guest_error(cpu, "guest SIGSEGV (unmapped read)", t_recovery_fault);
    }
    v = raw_load<uint16_t>(addr);
    t_recovery_armed = false;
    return v;
}

template<typename TaintT>
uint32_t mem_load32(CPU<TaintT>* cpu, uint64_t addr) {
    mem_check_load(cpu, addr, 4);
    t_recovery_armed = true;
    uint32_t v = 0;
    if (sigsetjmp(t_recovery_buf, 1)) {
        t_recovery_armed = false;
        guest_error(cpu, "guest SIGSEGV (unmapped read)", t_recovery_fault);
    }
    v = raw_load<uint32_t>(addr);
    t_recovery_armed = false;
    return v;
}

template<typename TaintT>
uint64_t mem_load64(CPU<TaintT>* cpu, uint64_t addr) {
    mem_check_load(cpu, addr, 8);
    t_recovery_armed = true;
    uint64_t v = 0;
    if (sigsetjmp(t_recovery_buf, 1)) {
        t_recovery_armed = false;
        guest_error(cpu, "guest SIGSEGV (unmapped read)", t_recovery_fault);
    }
    v = raw_load<uint64_t>(addr);
    t_recovery_armed = false;
    return v;
}

template<typename TaintT>
void mem_store8(CPU<TaintT>* cpu, uint64_t addr, uint8_t v) {
    watch_check(cpu, addr, 1, v);
    mem_check_store(cpu, addr, 1);
    t_recovery_armed = true;
    if (sigsetjmp(t_recovery_buf, 1)) {
        t_recovery_armed = false;
        guest_error(cpu, "guest SIGSEGV (unmapped write)", t_recovery_fault);
    }
    raw_store<uint8_t>(addr, v);
    t_recovery_armed = false;
}

template<typename TaintT>
void mem_store16(CPU<TaintT>* cpu, uint64_t addr, uint16_t v) {
    watch_check(cpu, addr, 2, v);
    mem_check_store(cpu, addr, 2);
    t_recovery_armed = true;
    if (sigsetjmp(t_recovery_buf, 1)) {
        t_recovery_armed = false;
        guest_error(cpu, "guest SIGSEGV (unmapped write)", t_recovery_fault);
    }
    raw_store<uint16_t>(addr, v);
    t_recovery_armed = false;
}

template<typename TaintT>
void mem_store32(CPU<TaintT>* cpu, uint64_t addr, uint32_t v) {
    watch_check(cpu, addr, 4, v);
    mem_check_store(cpu, addr, 4);
    t_recovery_armed = true;
    if (sigsetjmp(t_recovery_buf, 1)) {
        t_recovery_armed = false;
        guest_error(cpu, "guest SIGSEGV (unmapped write)", t_recovery_fault);
    }
    raw_store<uint32_t>(addr, v);
    t_recovery_armed = false;
}

template<typename TaintT>
void mem_store64(CPU<TaintT>* cpu, uint64_t addr, uint64_t v) {
    watch_check(cpu, addr, 8, v);
    mem_check_store(cpu, addr, 8);
    t_recovery_armed = true;
    if (sigsetjmp(t_recovery_buf, 1)) {
        t_recovery_armed = false;
        guest_error(cpu, "guest SIGSEGV (unmapped write)", t_recovery_fault);
    }
    raw_store<uint64_t>(addr, v);
    t_recovery_armed = false;
}

template<typename TaintT>
void mem_load_bytes(CPU<TaintT>* cpu, uint64_t addr, uint8_t* out, size_t n) {
    mem_check_load(cpu, addr, n);
    t_recovery_armed = true;
    if (sigsetjmp(t_recovery_buf, 1)) {
        t_recovery_armed = false;
        guest_error(cpu, "guest SIGSEGV (unmapped read)", t_recovery_fault);
    }
    memcpy(out, (const void*)(uintptr_t)addr, n);
    t_recovery_armed = false;
}

template<typename TaintT>
void mem_store_bytes(CPU<TaintT>* cpu, uint64_t addr, const uint8_t* in, size_t n) {
    if (n <= 8) {
        uint64_t v = 0;
        memcpy(&v, in, n);
        watch_check(cpu, addr, n, v);
    } else {
        watch_check(cpu, addr, n, 0xeeeeeeeeeeeeeeeeULL);
    }
    mem_check_store(cpu, addr, n);
    t_recovery_armed = true;
    if (sigsetjmp(t_recovery_buf, 1)) {
        t_recovery_armed = false;
        guest_error(cpu, "guest SIGSEGV (unmapped write)", t_recovery_fault);
    }
    memcpy((void*)(uintptr_t)addr, in, n);
    t_recovery_armed = false;
}

template<typename TaintT>
void mem_copy(CPU<TaintT>* cpu, uint64_t dst, uint64_t src, size_t n) {
    if (!n)
        return;
    watch_check(cpu, dst, n, 0xccccccccccccccccULL);
    mem_check_load(cpu, src, n);
    mem_check_store(cpu, dst, n);
    TaintT t;
    mem_get_taint(src, n, &t);
    t_recovery_armed = true;
    if (sigsetjmp(t_recovery_buf, 1)) {
        t_recovery_armed = false;
        guest_error(cpu, "guest SIGSEGV (string op)", t_recovery_fault);
    }
    memmove((void*)(uintptr_t)dst, (const void*)(uintptr_t)src, n);
    t_recovery_armed = false;
    mem_note_store(cpu, dst, n, t);
}

bool mem_copy_guarded(uint64_t src, void* dst, size_t n) {
    if (!n)
        return true;
    // Page-granular so one stale page only zeroes its own chunk.
    uint8_t* d = (uint8_t*)dst;
    uint64_t s = src;
    size_t left = n;
    bool ok = true;
    while (left) {
        size_t chunk = left;
        size_t page_off = (size_t)(s & 0xfffULL);
        if (page_off + chunk > 4096)
            chunk = 4096 - page_off;
        t_recovery_armed = true;
        if (sigsetjmp(t_recovery_buf, 1)) {
            t_recovery_armed = false;
            memset(d, 0, chunk);
            ok = false;
        } else {
            memcpy(d, (const void*)(uintptr_t)s, chunk);
            t_recovery_armed = false;
        }
        d += chunk;
        s += chunk;
        left -= chunk;
    }
    return ok;
}

template<typename TaintT>
size_t mem_read_code(CPU<TaintT>* cpu, uint64_t rip, uint8_t* buf) {
    // Code fetch: try to copy 15 bytes; on fault, copy byte-by-byte to find
    // how much is readable, then raise if nothing is.
    t_recovery_armed = true;
    if (sigsetjmp(t_recovery_buf, 1)) {
        t_recovery_armed = false;
        // Slow path: probe byte by byte.
        size_t n = 0;
        for (; n < 15; n++) {
            t_recovery_armed = true;
            if (sigsetjmp(t_recovery_buf, 1)) {
                t_recovery_armed = false;
                break;
            }
            buf[n] = *(volatile uint8_t*)(uintptr_t)(rip + n);
            t_recovery_armed = false;
        }
        if (!n)
            guest_error(cpu, "guest SIGSEGV (bad RIP)", rip);
        return n;
    }
    memcpy(buf, (const void*)(uintptr_t)rip, 15);
    t_recovery_armed = false;
    return 15;
}

// --- explicit instantiations -----------------------------------------------
// The whole mem API is templated on the taint sidecar type so the engine can
// run with plain Taint (default), --history's HistoryTaint, or
// --history=N/full's DeepHistoryTaint. All instantiations are defined here
// once; no other TU instantiates them.

template uint8_t mem_load8<Taint>(CPU<Taint>*, uint64_t);
template uint8_t mem_load8<HistoryTaint>(CPU<HistoryTaint>*, uint64_t);
template uint8_t mem_load8<DeepHistoryTaint>(CPU<DeepHistoryTaint>*, uint64_t);
template uint16_t mem_load16<Taint>(CPU<Taint>*, uint64_t);
template uint16_t mem_load16<HistoryTaint>(CPU<HistoryTaint>*, uint64_t);
template uint16_t mem_load16<DeepHistoryTaint>(CPU<DeepHistoryTaint>*,
                                               uint64_t);
template uint32_t mem_load32<Taint>(CPU<Taint>*, uint64_t);
template uint32_t mem_load32<HistoryTaint>(CPU<HistoryTaint>*, uint64_t);
template uint32_t mem_load32<DeepHistoryTaint>(CPU<DeepHistoryTaint>*,
                                               uint64_t);
template uint64_t mem_load64<Taint>(CPU<Taint>*, uint64_t);
template uint64_t mem_load64<HistoryTaint>(CPU<HistoryTaint>*, uint64_t);
template uint64_t mem_load64<DeepHistoryTaint>(CPU<DeepHistoryTaint>*,
                                               uint64_t);
template void mem_store8<Taint>(CPU<Taint>*, uint64_t, uint8_t);
template void mem_store8<HistoryTaint>(CPU<HistoryTaint>*, uint64_t, uint8_t);
template void mem_store8<DeepHistoryTaint>(CPU<DeepHistoryTaint>*, uint64_t,
                                           uint8_t);
template void mem_store16<Taint>(CPU<Taint>*, uint64_t, uint16_t);
template void mem_store16<HistoryTaint>(CPU<HistoryTaint>*, uint64_t, uint16_t);
template void mem_store16<DeepHistoryTaint>(CPU<DeepHistoryTaint>*, uint64_t,
                                            uint16_t);
template void mem_store32<Taint>(CPU<Taint>*, uint64_t, uint32_t);
template void mem_store32<HistoryTaint>(CPU<HistoryTaint>*, uint64_t, uint32_t);
template void mem_store32<DeepHistoryTaint>(CPU<DeepHistoryTaint>*, uint64_t,
                                            uint32_t);
template void mem_store64<Taint>(CPU<Taint>*, uint64_t, uint64_t);
template void mem_store64<HistoryTaint>(CPU<HistoryTaint>*, uint64_t, uint64_t);
template void mem_store64<DeepHistoryTaint>(CPU<DeepHistoryTaint>*, uint64_t,
                                            uint64_t);
template void mem_load_bytes<Taint>(CPU<Taint>*, uint64_t, uint8_t*, size_t);
template void mem_load_bytes<HistoryTaint>(CPU<HistoryTaint>*, uint64_t,
                                           uint8_t*, size_t);
template void mem_load_bytes<DeepHistoryTaint>(CPU<DeepHistoryTaint>*, uint64_t,
                                               uint8_t*, size_t);
template void mem_store_bytes<Taint>(CPU<Taint>*, uint64_t, const uint8_t*,
                                     size_t);
template void mem_store_bytes<HistoryTaint>(CPU<HistoryTaint>*, uint64_t,
                                            const uint8_t*, size_t);
template void mem_store_bytes<DeepHistoryTaint>(CPU<DeepHistoryTaint>*,
                                                uint64_t, const uint8_t*,
                                                size_t);
template size_t mem_read_code<Taint>(CPU<Taint>*, uint64_t, uint8_t*);
template size_t mem_read_code<HistoryTaint>(CPU<HistoryTaint>*, uint64_t,
                                            uint8_t*);
template size_t mem_read_code<DeepHistoryTaint>(CPU<DeepHistoryTaint>*,
                                                uint64_t, uint8_t*);
template void mem_copy<Taint>(CPU<Taint>*, uint64_t, uint64_t, size_t);
template void mem_copy<HistoryTaint>(CPU<HistoryTaint>*, uint64_t, uint64_t,
                                     size_t);
template void mem_copy<DeepHistoryTaint>(CPU<DeepHistoryTaint>*, uint64_t,
                                         uint64_t, size_t);
template void mem_check_load<Taint>(CPU<Taint>*, uint64_t, size_t);
template void mem_check_load<HistoryTaint>(CPU<HistoryTaint>*, uint64_t,
                                           size_t);
template void mem_check_load<DeepHistoryTaint>(CPU<DeepHistoryTaint>*,
                                               uint64_t, size_t);
template void mem_check_store<Taint>(CPU<Taint>*, uint64_t, size_t);
template void mem_check_store<HistoryTaint>(CPU<HistoryTaint>*, uint64_t,
                                            size_t);
template void mem_check_store<DeepHistoryTaint>(CPU<DeepHistoryTaint>*,
                                                uint64_t, size_t);
template void mem_get_taint<Taint>(uint64_t, size_t, Taint*);
template void mem_get_taint<HistoryTaint>(uint64_t, size_t, HistoryTaint*);
template void mem_get_taint<DeepHistoryTaint>(uint64_t, size_t,
                                              DeepHistoryTaint*);
template void mem_note_store<Taint>(CPU<Taint>*, uint64_t, size_t, Taint);
template void mem_note_store<HistoryTaint>(CPU<HistoryTaint>*, uint64_t,
                                           size_t, HistoryTaint);
template void mem_note_store<DeepHistoryTaint>(CPU<DeepHistoryTaint>*, uint64_t,
                                               size_t, DeepHistoryTaint);
template uint64_t mem_client_request<Taint>(Emulator*, CPU<Taint>*, uint64_t,
                                            uint64_t, uint64_t, uint64_t);
template uint64_t mem_client_request<HistoryTaint>(Emulator*,
                                                   CPU<HistoryTaint>*, uint64_t,
                                                   uint64_t, uint64_t,
                                                   uint64_t);
template uint64_t mem_client_request<DeepHistoryTaint>(
    Emulator*, CPU<DeepHistoryTaint>*, uint64_t, uint64_t, uint64_t, uint64_t);
template [[noreturn]] void guest_error<Taint>(CPU<Taint>*, const char*,
                                              uint64_t,
                                              std::vector<std::vector<uint64_t>>);
template [[noreturn]] void guest_error<HistoryTaint>(CPU<HistoryTaint>*,
                                                     const char*, uint64_t,
                                                     std::vector<std::vector<uint64_t>>);
template [[noreturn]] void guest_error<DeepHistoryTaint>(
    CPU<DeepHistoryTaint>*, const char*, uint64_t,
    std::vector<std::vector<uint64_t>>);
