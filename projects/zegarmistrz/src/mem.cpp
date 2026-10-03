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
thread_local CPU* t_loop_cpu = nullptr;
thread_local int t_loop_sig = 0;
thread_local uint64_t t_loop_addr = 0;
thread_local bool t_loop_pending = false;
} // namespace

static bool g_passthrough = false;

void mem_set_passthrough(bool v) { g_passthrough = v; }
bool mem_passthrough() { return g_passthrough; }

static void fault_handler(int sig, siginfo_t* info, void*) {
    if ((sig == SIGSEGV || sig == SIGBUS) && t_recovery_armed) {
        t_recovery_fault =
            info && info->si_addr ? (uint64_t)(uintptr_t)info->si_addr : 0;
        siglongjmp(t_recovery_buf, 1);
    }
    if (t_loop_armed && t_loop_cpu) {
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

LoopScope::LoopScope(CPU* c) : cpu(c) {
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

FaultScope::FaultScope(CPU* c) : cpu(c) {}
FaultScope::~FaultScope() {}

[[noreturn]] void guest_error(CPU* cpu, const char* reason, uint64_t fault_addr) {
    GuestError e;
    e.cpu = cpu;
    e.reason = reason;
    e.fault_addr = fault_addr;
    throw e;
}

// --- poison map ------------------------------------------------------------
// Phase 2: per-byte poison bits in a mutex-guarded std::map. Checked on
// every access; client requests via the magic call address populate it.
//
// Flag bitmask (see mem.h POISON_*):
//   bit0 cannot-load, bit1 cannot-store, bit2 cannot-branch,
//   bit3 cannot-index, bit4 clear-branch-on-store, bit5 clear-index-on-store.

static std::mutex g_poison_mu;
static std::map<uint64_t, uint8_t> g_poison; // key: byte address

void mem_poison_range(Emulator*, uint64_t addr, size_t size, unsigned kind) {
    if (!size)
        return;
    kind &= 0x3f;
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
}

uint64_t mem_client_request(Emulator* emu, CPU* cpu, uint64_t op,
                            uint64_t ptr, uint64_t size, uint64_t flags) {
    (void)cpu;
    if (op == 0)
        return 0; // no-op/query
    if (op == 1) {
        mem_poison_range(emu, ptr, (size_t)size, (unsigned)flags);
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
static inline void watch_check(CPU* cpu, uint64_t addr, size_t size,
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
        if (bits == 0x3f)
            break;
    }
    return bits;
}

void mem_get_taint(uint64_t addr, size_t size, Taint* out) {
    unsigned bits = poison_lookup(addr, size);
    if (!out)
        return;
    out->cannot_branch = (bits & POISON_CANNOT_BRANCH) != 0;
    out->cannot_index = (bits & POISON_CANNOT_INDEX) != 0;
}

// Store taint semantics (see mem.h): the destination bytes gain the stored
// value's cannot-branch/index bits plus the matching clear-on-store bit, so
// a later store to a clear-on-store location sanitizes the value (and the
// memory). Load/store poison bits and clear bits are sticky.
void mem_note_store(CPU* cpu, uint64_t addr, size_t size, Taint t) {
    (void)cpu;
    bool cb = t.cannot_branch, ci = t.cannot_index;
    if (!cb && !ci) {
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
            if (nu != old) {
                if (nu)
                    it->second = nu;
                else
                    it = g_poison.erase(it);
                if (!nu)
                    continue;
            }
            ++it;
        }
        return;
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
        if (nu)
            g_poison[a] = nu;
        else if (it != g_poison.end())
            g_poison.erase(it);
    }
}

void mem_check_load(CPU* cpu, uint64_t addr, size_t size) {
    unsigned bits = poison_lookup(addr, size);
    if (bits & POISON_CANNOT_LOAD)
        guest_error(cpu, "load from poisoned (cannot-load) memory", addr);
}

void mem_check_store(CPU* cpu, uint64_t addr, size_t size) {
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

uint8_t mem_load8(CPU* cpu, uint64_t addr) {
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

uint16_t mem_load16(CPU* cpu, uint64_t addr) {
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

uint32_t mem_load32(CPU* cpu, uint64_t addr) {
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

uint64_t mem_load64(CPU* cpu, uint64_t addr) {
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

void mem_store8(CPU* cpu, uint64_t addr, uint8_t v) {
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

void mem_store16(CPU* cpu, uint64_t addr, uint16_t v) {
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

void mem_store32(CPU* cpu, uint64_t addr, uint32_t v) {
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

void mem_store64(CPU* cpu, uint64_t addr, uint64_t v) {
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

void mem_load_bytes(CPU* cpu, uint64_t addr, uint8_t* out, size_t n) {
    mem_check_load(cpu, addr, n);
    t_recovery_armed = true;
    if (sigsetjmp(t_recovery_buf, 1)) {
        t_recovery_armed = false;
        guest_error(cpu, "guest SIGSEGV (unmapped read)", t_recovery_fault);
    }
    memcpy(out, (const void*)(uintptr_t)addr, n);
    t_recovery_armed = false;
}

void mem_store_bytes(CPU* cpu, uint64_t addr, const uint8_t* in, size_t n) {
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

void mem_copy(CPU* cpu, uint64_t dst, uint64_t src, size_t n) {
    if (!n)
        return;
    watch_check(cpu, dst, n, 0xccccccccccccccccULL);
    mem_check_load(cpu, src, n);
    mem_check_store(cpu, dst, n);
    Taint t;
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

size_t mem_read_code(CPU* cpu, uint64_t rip, uint8_t* buf) {
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
