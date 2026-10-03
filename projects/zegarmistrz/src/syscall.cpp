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

#include "syscall.h"

#include <asm/prctl.h>
#include <errno.h>
#include <linux/futex.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <condition_variable>
#include <cstdint>
#include <mutex>

#include "cpu.h"
#include "decode_exec.h"
#include "emulator.h"
#include "mem.h"

#ifndef CLONE_PARENT_SETTID
#define CLONE_PARENT_SETTID 0x00100000
#endif
#ifndef CLONE_CHILD_CLEARTID
#define CLONE_CHILD_CLEARTID 0x00200000
#endif
#ifndef CLONE_SETTLS
#define CLONE_SETTLS 0x00080000
#endif
#ifndef CLONE_THREAD
#define CLONE_THREAD 0x00010000
#endif
#ifndef CLONE_VM
#define CLONE_VM 0x00000100
#endif

// Forward declarations for guest_maps helpers defined below (do_brk needs
// them before their definitions).
struct Emulator;
static void maps_add(Emulator* emu, uint64_t start, uint64_t end, int prot,
                     const char* path);
static void maps_remove_range(Emulator* emu, uint64_t start, uint64_t len);

const char* syscall_name(long num) {
    switch (num) {
    case SYS_read: return "read";
    case SYS_write: return "write";
    case SYS_open: return "open";
    case SYS_close: return "close";
    case SYS_stat: return "stat";
    case SYS_mmap: return "mmap";
    case SYS_mprotect: return "mprotect";
    case SYS_munmap: return "munmap";
    case SYS_brk: return "brk";
    case SYS_rt_sigaction: return "rt_sigaction";
    case SYS_rt_sigprocmask: return "rt_sigprocmask";
    case SYS_ioctl: return "ioctl";
    case SYS_access: return "access";
    case SYS_sched_yield: return "sched_yield";
    case SYS_madvise: return "madvise";
    case SYS_set_tid_address: return "set_tid_address";
    case SYS_clone: return "clone";
    case SYS_futex: return "futex";
    case SYS_arch_prctl: return "arch_prctl";
    case SYS_exit_group: return "exit_group";
    case SYS_exit: return "exit";
    case SYS_openat: return "openat";
    case SYS_newfstatat: return "newfstatat";
    case SYS_set_robust_list: return "set_robust_list";
    case SYS_getrandom: return "getrandom";
    case SYS_prlimit64: return "prlimit64";
    case SYS_clone3: return "clone3";
    case SYS_rseq: return "rseq";
    case SYS_lseek: return "lseek";
    case SYS_gettid: return "gettid";
    case SYS_sched_getaffinity: return "sched_getaffinity";
    case SYS_fstatfs: return "fstatfs";
    case SYS_statfs: return "statfs";
    case SYS_readv: return "readv";
    case SYS_writev: return "writev";
    case SYS_pread64: return "pread64";
    case SYS_pwrite64: return "pwrite64";
    case SYS_readlinkat: return "readlinkat";
    case SYS_getuid: return "getuid";
    case SYS_geteuid: return "geteuid";
    case SYS_getgid: return "getgid";
    case SYS_getegid: return "getegid";
    case SYS_getpid: return "getpid";
    case SYS_getppid: return "getppid";
    case SYS_uname: return "uname";
    case SYS_sysinfo: return "sysinfo";
    case SYS_getcwd: return "getcwd";
    case SYS_lstat: return "lstat";
    default: return "unknown";
    }
}

// --- brk virtualization ----------------------------------------------------

static uint64_t do_brk(CPU* cpu, uint64_t addr) {
    Emulator* emu = cpu->emu;
    std::lock_guard<std::mutex> l(emu->brk_mu);
    if (addr == 0)
        return emu->guest_brk;
    if (emu->brk_limit && addr > emu->brk_limit)
        return emu->guest_brk; // no room: fail like OOM
    if (addr > emu->guest_brk) {
        uint64_t lo = (emu->guest_brk + 0xfffULL) & ~0xfffULL;
        uint64_t hi = (addr + 0xfffULL) & ~0xfffULL;
                if (hi > lo) {
            void* m = mmap((void*)(uintptr_t)lo, hi - lo,
                           PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
                           -1, 0);
            if (m == MAP_FAILED)
                return emu->guest_brk; // allocation failed: keep old brk
            // Track the newly mapped brk pages for core dumps. (Internal
            // MAP_FIXED: exempt from fixed_allowed; always within the brk
            // reservation when brk_limit is set.)
            maps_remove_range(emu, lo, hi - lo);
            maps_add(emu, lo, hi, PROT_READ | PROT_WRITE, "guest-brk");
        }
        emu->guest_brk = addr;
    } else if (addr < emu->guest_brk) {
        // Shrinking: just move the marker (keep pages mapped; harmless).
        emu->guest_brk = addr;
    }
    return emu->guest_brk;
}

// --- guest memory tracking (guest_maps) --------------------------------------
// Policy (documented choice): guest VA == host VA. Every successful guest
// mmap (and every brk growth) is recorded in emu->guest_maps so core dumps
// include it. MAP_FIXED (or MAP_FIXED_NOREPLACE) is only allowed when the
// target is already owned by the guest:
//   (a) fully covered by existing guest_maps entries (remap, no gaps), or
//   (b) inside the initial 8MB guest stack reservation, or
//   (c) inside the exe reservation [exe_base, brk_limit) (the exe's PT_LOADs
//       plus the 1GB brk-growth gap reserved at load time; for ET_EXEC
//       binaries brk_limit is 0 and this rule is skipped).
// Anything else risks stomping host mappings (the zegarmistrz binary, libc,
// vDSO arenas): reject with -EPERM. do_brk's internal MAP_FIXED extension is
// exempt (it always extends within [guest_brk, brk_limit)).
// mprotect/munmap are passed through and then reflected into guest_maps for
// overlapping ranges. Syscall buffer validation below is poison-only
// (mem_check_*): unmapped addresses still produce the host's EFAULT rather
// than a core dump.

static uint64_t page_align_down(uint64_t v) { return v & ~0xfffULL; }
static uint64_t page_align_up(uint64_t v) { return (v + 0xfffULL) & ~0xfffULL; }
static bool ranges_overlap(uint64_t s1, uint64_t e1, uint64_t s2, uint64_t e2) {
    return s1 < e2 && s2 < e1;
}

// Caller must hold emu->maps_mu. True when [addr,addr+len) has no gaps in
// the current guest_maps.
static bool maps_covers_locked(Emulator* emu, uint64_t addr, uint64_t len) {
    if (!len)
        return false;
    uint64_t end = addr + len;
    if (end < addr)
        return false; // wrap
    uint64_t cur = addr;
    while (cur < end) {
        bool found = false;
        uint64_t adv = cur;
        for (auto& m : emu->guest_maps) {
            if (m.start <= cur && m.end > cur) {
                if (m.end > adv)
                    adv = m.end;
                found = true;
            }
        }
        if (!found)
            return false;
        cur = adv;
    }
    return true;
}

static void maps_add(Emulator* emu, uint64_t start, uint64_t end, int prot,
                     const char* path) {
    if (end <= start)
        return;
    std::lock_guard<std::mutex> l(emu->maps_mu);
    Emulator::GuestMap gm;
    gm.start = start;
    gm.end = end;
    gm.prot = prot;
    if (path)
        gm.path = path;
    emu->guest_maps.push_back(gm);
}

// Erase (splitting entries as needed) the intersection with [start,end).
static void maps_remove_range(Emulator* emu, uint64_t start, uint64_t len) {
    if (!len)
        return;
    uint64_t end = start + len;
    if (end < start)
        return;
    std::lock_guard<std::mutex> l(emu->maps_mu);
    std::vector<Emulator::GuestMap> out;
    out.reserve(emu->guest_maps.size() + 1);
    for (auto& m : emu->guest_maps) {
        if (!ranges_overlap(m.start, m.end, start, end)) {
            out.push_back(m);
            continue;
        }
        if (m.start < start) {
            Emulator::GuestMap left = m;
            left.end = start;
            out.push_back(left);
        }
        if (m.end > end) {
            Emulator::GuestMap right = m;
            right.start = end;
            out.push_back(right);
        }
        // overlapped middle is dropped
    }
    emu->guest_maps.swap(out);
}

// Set prot on the intersection with [start,end), splitting entries.
static void maps_set_prot(Emulator* emu, uint64_t start, uint64_t len,
                          int prot) {
    if (!len)
        return;
    uint64_t end = start + len;
    if (end < start)
        return;
    std::lock_guard<std::mutex> l(emu->maps_mu);
    std::vector<Emulator::GuestMap> out;
    out.reserve(emu->guest_maps.size() + 2);
    for (auto& m : emu->guest_maps) {
        if (!ranges_overlap(m.start, m.end, start, end)) {
            out.push_back(m);
            continue;
        }
        if (m.start < start) {
            Emulator::GuestMap left = m;
            left.end = start;
            out.push_back(left);
        }
        {
            Emulator::GuestMap mid = m;
            if (mid.start < start)
                mid.start = start;
            if (mid.end > end)
                mid.end = end;
            mid.prot = prot;
            out.push_back(mid);
        }
        if (m.end > end) {
            Emulator::GuestMap right = m;
            right.start = end;
            out.push_back(right);
        }
    }
    emu->guest_maps.swap(out);
}

static bool fixed_allowed(Emulator* emu, uint64_t addr, uint64_t len) {
    if (!len)
        return false;
    uint64_t end = addr + len;
    if (end < addr || end == 0)
        return false;
    // (b) initial guest stack reservation.
    if (emu->guest_stack_size && addr >= emu->guest_stack_base &&
        end <= emu->guest_stack_base + emu->guest_stack_size)
        return true;
    // (c) exe + brk-growth reservation (PIE only; ET_EXEC has brk_limit 0).
    if (emu->brk_limit && addr >= emu->exe_base && end <= emu->brk_limit &&
        addr >= emu->exe_base)
        return true;
    // (a) remap of existing guest mappings (no gaps).
    {
        std::lock_guard<std::mutex> l(emu->maps_mu);
        if (maps_covers_locked(emu, addr, len))
            return true;
    }
    return false;
}

// --- syscall buffer validation (poison-only) ---------------------------------
// Reject poisoned (cannot-load/cannot-store) syscall buffers via GuestError.
// Mapping faults are deliberately left to the host kernel (EFAULT), so only
// poison bits are checked here.

static void sys_check_mem(CPU* cpu, uint64_t addr, size_t len, bool want_write) {
    if (!len)
        return;
    if (want_write)
        mem_check_store(cpu, addr, len);
    else
        mem_check_load(cpu, addr, len);
}

static void sys_check_cstr(CPU* cpu, uint64_t addr) {
    // NUL-terminated path: poison-check page by page until NUL (bounded by
    // PATH_MAX+1). mem_load8 converts unmapped pages to GuestError.
    for (size_t i = 0; i < 4097; i++) {
        mem_check_load(cpu, addr + i, 1);
        if (mem_load8(cpu, addr + i) == 0)
            return;
    }
    guest_error(cpu, "syscall path string too long (no NUL)");
}

// Validate known buffer layouts for passthrough syscalls. Unknown syscalls
// keep raw passthrough (their layouts are not modeled).
static void syscall_check_buffers(CPU* cpu, uint64_t n, uint64_t a1,
                                  uint64_t a2, uint64_t a3, uint64_t a4,
                                  uint64_t a5, uint64_t a6) {
    (void)a4;
    (void)a5;
    (void)a6;
    switch (n) {
    case SYS_read:
        sys_check_mem(cpu, a2, (size_t)a3, true);
        break;
    case SYS_write:
        sys_check_mem(cpu, a2, (size_t)a3, false);
        break;
    case SYS_open:
        sys_check_cstr(cpu, a1);
        break;
    case SYS_openat:
        sys_check_cstr(cpu, a2);
        break;
    case SYS_stat:
    case SYS_lstat:
        sys_check_cstr(cpu, a1);
        sys_check_mem(cpu, a2, 144, true); // struct stat
        break;
    case SYS_fstat:
        sys_check_mem(cpu, a2, 144, true);
        break;
    case SYS_newfstatat:
        sys_check_cstr(cpu, a2);
        sys_check_mem(cpu, a3, 144, true);
        break;
    case SYS_statx:
        sys_check_cstr(cpu, a2);
        sys_check_mem(cpu, a5, 256, true); // struct statx
        break;
    case SYS_readlinkat:
        sys_check_cstr(cpu, a2);
        sys_check_mem(cpu, a3, (size_t)a4, true);
        break;
    case SYS_getcwd:
        sys_check_mem(cpu, a1, (size_t)a2, true);
        break;
    case SYS_pread64:
        sys_check_mem(cpu, a2, (size_t)a3, true);
        break;
    case SYS_pwrite64:
        sys_check_mem(cpu, a2, (size_t)a3, false);
        break;
    case SYS_readv:
    case SYS_writev: {
        bool want_write = (n == (uint64_t)SYS_readv);
        long count = (long)a3;
        if (count < 0 || count > 1024)
            break;
        sys_check_mem(cpu, a2, (size_t)(count * 16), false); // iovec array
        for (long i = 0; i < count; i++) {
            uint64_t base = 0;
            uint64_t blen = 0;
            mem_load_bytes(cpu, a2 + (uint64_t)i * 16, (uint8_t*)&base, 8);
            mem_load_bytes(cpu, a2 + (uint64_t)i * 16 + 8, (uint8_t*)&blen, 8);
            sys_check_mem(cpu, base, (size_t)blen, want_write);
        }
        break;
    }
    case SYS_getdents64:
        sys_check_mem(cpu, a2, (size_t)a3, true);
        break;
    case SYS_uname:
        sys_check_mem(cpu, a1, 390, true); // struct utsname (6x65)
        break;
    case SYS_sysinfo:
        sys_check_mem(cpu, a1, 112, true); // struct sysinfo
        break;
    case SYS_statfs:
        sys_check_cstr(cpu, a1);
        sys_check_mem(cpu, a2, 120, true); // struct statfs
        break;
    case SYS_fstatfs:
        sys_check_mem(cpu, a2, 120, true);
        break;
    case SYS_sched_getaffinity:
        sys_check_mem(cpu, a3, (size_t)a2, true);
        break;
    case SYS_getrandom:
        sys_check_mem(cpu, a1, (size_t)a2, true);
        break;
    case SYS_prlimit64:
        if (a3)
            sys_check_mem(cpu, a3, 16, false); // struct rlimit (new)
        if (a4)
            sys_check_mem(cpu, a4, 16, true); // struct rlimit (old)
        break;
    case SYS_set_robust_list:
        sys_check_mem(cpu, a1, (size_t)a2, false);
        break;
    case SYS_get_robust_list:
        sys_check_mem(cpu, a2, 8, true);
        sys_check_mem(cpu, a3, 8, true);
        break;
    case SYS_futex:
        sys_check_mem(cpu, a1, 4, false);
        break;
    case SYS_nanosleep:
    case SYS_clock_nanosleep:
        break; // timespec layouts vary; host validates
    default:
        break;
    }
}

// Guest mmap with guest_maps tracking + MAP_FIXED policy (see above).
static uint64_t do_mmap(CPU* cpu, uint64_t addr, uint64_t len, uint64_t prot,
                        uint64_t flags, uint64_t fd, uint64_t off) {
    Emulator* emu = cpu->emu;
    if (!len)
        return (uint64_t)(-(long)EINVAL);
#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif
    if (flags & (MAP_FIXED | MAP_FIXED_NOREPLACE)) {
        if (!fixed_allowed(emu, addr, len))
            return (uint64_t)(-(long)EPERM);
    }
    errno = 0;
    long r = ::syscall(SYS_mmap, (void*)(uintptr_t)addr, (size_t)len,
                       (int)prot, (int)flags, (int)fd, (off_t)off);
    if (r == -1)
        return (uint64_t)(-(long)errno);
    uint64_t base = (uint64_t)(uintptr_t)r;
    uint64_t aligned = page_align_up(len);
    // Replace any overlapped guest range (MAP_FIXED semantics), then record.
    maps_remove_range(emu, base, aligned);
    maps_add(emu, base, base + aligned, (int)prot, "guest-mmap");
    return base;
}

// --- clone emulation -------------------------------------------------------

struct CloneArgs {
    uint64_t flags, pidfd, child_tid, parent_tid, exit_signal, stack,
        stack_size, tls, set_tid, set_tid_size, cgroup;
};

// Extended ChildStart with tid addresses.
struct ChildStart2 {
    CPU child_cpu;
    uint64_t parent_tid_addr = 0;
    uint64_t child_tid_addr = 0;
    std::mutex mu;
    std::condition_variable cv;
    bool ready = false;
    int child_tid = 0;
};

static void thread_register_tid(CPU* cpu, int tid) {
    cpu->tid = tid;
    Emulator* emu = cpu->emu;
    std::lock_guard<std::mutex> l(emu->thread_mu);
    emu->thread_tids.push_back((uint64_t)tid);
    emu->thread_cpus.push_back(cpu);
}

// Remove a CPU from the live-thread vectors (no dangling pointers). Both
// vectors are indexed in lockstep; the entries are erased, not just nulled.
static void thread_unregister_cpu(CPU* cpu) {
    Emulator* emu = cpu->emu;
    std::lock_guard<std::mutex> l(emu->thread_mu);
    for (size_t i = 0; i < emu->thread_cpus.size(); i++) {
        if (emu->thread_cpus[i] == cpu) {
            emu->thread_cpus.erase(emu->thread_cpus.begin() + (ptrdiff_t)i);
            if (i < emu->thread_tids.size())
                emu->thread_tids.erase(emu->thread_tids.begin() +
                                       (ptrdiff_t)i);
            return;
        }
    }
    // Not found (e.g. already unregistered): fall back to clearing any stale
    // null-hole left by older builds; nothing to do.
}

// Clear CLONE_CHILD_CLEARTID word + futex wake (both flavors: glibc may wait
// with or without PRIVATE).
static void thread_clear_ctid(CPU* cpu) {
    if (!cpu->clear_ctid_addr)
        return;
    *(volatile int*)(uintptr_t)cpu->clear_ctid_addr = 0;
    ::syscall(SYS_futex, (void*)(uintptr_t)cpu->clear_ctid_addr, FUTEX_WAKE,
              1, nullptr, nullptr, 0);
    ::syscall(SYS_futex, (void*)(uintptr_t)cpu->clear_ctid_addr,
              FUTEX_WAKE | FUTEX_PRIVATE_FLAG, 1, nullptr, nullptr, 0);
}

static void* child_thread_main2(void* arg) {
    ChildStart2* st = (ChildStart2*)arg;
    CPU* cpu = new CPU(st->child_cpu);
    uint64_t paddr = st->parent_tid_addr;
    uint64_t caddr = st->child_tid_addr;
    (void)caddr;
    int tid = (int)::syscall(SYS_gettid);
    thread_register_tid(cpu, tid);
    if (paddr) {
        // Validate the parent_tid destination before writing (poison-aware).
        mem_check_store(cpu, paddr, 4);
        *(volatile int*)(uintptr_t)paddr = tid;
    }
    // NOTE: no child_tid write here. The kernel only writes child_tid at
    // thread start for CLONE_CHILD_SETTID (not set in our flags); for
    // CLONE_CHILD_CLEARTID it only *clears* on thread exit (done in the
    // exit paths below). Writing here would clobber state (e.g. musl's
    // detach_state which shares the word).
    {
        std::lock_guard<std::mutex> l(st->mu);
        st->child_tid = tid;
        st->ready = true;
    }
    st->cv.notify_one();
    // NOTE: st is freed by the parent after wait returns; this thread never
    // touches st after notify_one, so there is no use-after-free.
    run_cpu_loop(cpu);
    // Normal return: whole-process exiting (exit_group). Unregister first so
    // thread_cpus never dangles, then free the heap CPU.
    thread_clear_ctid(cpu);
    thread_unregister_cpu(cpu);
    delete cpu;
    return nullptr;
}

static uint64_t do_clone3(CPU* cpu, uint64_t uaddr, uint64_t usize) {
    if (usize < 88) // sizeof(struct clone_args) canonical
        return (uint64_t)(-(long)EINVAL);
    CloneArgs ca;
    memset(&ca, 0, sizeof(ca));
    mem_load_bytes(cpu, uaddr, (uint8_t*)&ca, usize < sizeof(ca) ? usize : sizeof(ca));
    uint64_t flags = ca.flags;

    ChildStart2* st = new ChildStart2();
    st->child_cpu = *cpu; // copies regs incl. already-advanced RIP
    st->child_cpu.gpr[ZG_RAX].val = 0;
    st->child_cpu.gpr[ZG_RAX].taint.cannot_branch = false;
    st->child_cpu.gpr[ZG_RAX].taint.cannot_index = false;
    st->child_cpu.clear_ctid_addr = 0;
    if (flags & CLONE_SETTLS)
        st->child_cpu.fs_base = ca.tls;
    if (ca.stack && ca.stack_size)
        st->child_cpu.gpr[ZG_RSP].val = ca.stack + ca.stack_size;
    if (flags & CLONE_PARENT_SETTID)
        st->parent_tid_addr = ca.parent_tid;
    if (flags & CLONE_CHILD_CLEARTID) {
        st->child_tid_addr = ca.child_tid;
        st->child_cpu.clear_ctid_addr = ca.child_tid;
    }

    pthread_t th;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    // 8MB host stack for interpreter recursion depth.
    pthread_attr_setstacksize(&attr, 8 << 20);
    if (pthread_create(&th, &attr, child_thread_main2, st) != 0) {
        pthread_attr_destroy(&attr);
        delete st;
        return (uint64_t)(-(long)EAGAIN);
    }
    pthread_attr_destroy(&attr);
    pthread_detach(th);

    int tid = 0;
    {
        std::unique_lock<std::mutex> l(st->mu);
        st->cv.wait(l, [&] { return st->ready; });
        tid = st->child_tid;
    }
    // The child copied everything it needs (child_cpu, tid addrs) before
    // notify_one and never touches st afterwards, so the parent owns st now.
    delete st;
    return (uint64_t)(int64_t)tid;
}

static uint64_t do_clone(CPU* cpu, uint64_t flags, uint64_t stack,
                         uint64_t parent_tid, uint64_t tls, uint64_t child_tid) {
    if (!(flags & CLONE_THREAD))
        return (uint64_t)(-(long)ENOSYS); // no fork support in phase 1
    ChildStart2* st = new ChildStart2();
    st->child_cpu = *cpu;
    st->child_cpu.gpr[ZG_RAX].val = 0;
    st->child_cpu.gpr[ZG_RAX].taint.cannot_branch = false;
    st->child_cpu.gpr[ZG_RAX].taint.cannot_index = false;
    st->child_cpu.clear_ctid_addr = 0;
    if (flags & CLONE_SETTLS)
        st->child_cpu.fs_base = tls;
    if (stack)
        st->child_cpu.gpr[ZG_RSP].val = stack;
    if (flags & CLONE_PARENT_SETTID)
        st->parent_tid_addr = parent_tid;
    if (flags & CLONE_CHILD_CLEARTID) {
        // CLEARTID: kernel clears (never writes) child_tid; see clone3 path.
        st->child_tid_addr = 0;
        st->child_cpu.clear_ctid_addr = child_tid;
    }
    pthread_t th;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 8 << 20);
    if (pthread_create(&th, &attr, child_thread_main2, st) != 0) {
        pthread_attr_destroy(&attr);
        delete st;
        return (uint64_t)(-(long)EAGAIN);
    }
    pthread_attr_destroy(&attr);
    pthread_detach(th);
    int tid = 0;
    {
        std::unique_lock<std::mutex> l(st->mu);
        st->cv.wait(l, [&] { return st->ready; });
        tid = st->child_tid;
    }
    // Same ownership as clone3: the child never touches st after notify.
    delete st;
    return (uint64_t)(int64_t)tid;
}

// --- main entry ------------------------------------------------------------

void emulate_syscall(CPU* cpu, unsigned insn_len) {
    uint64_t n = cpu->gpr[ZG_RAX].val;
    uint64_t a1 = cpu->gpr[ZG_RDI].val;
    uint64_t a2 = cpu->gpr[ZG_RSI].val;
    uint64_t a3 = cpu->gpr[ZG_RDX].val;
    uint64_t a4 = cpu->gpr[ZG_R10].val;
    uint64_t a5 = cpu->gpr[ZG_R8].val;
    uint64_t a6 = cpu->gpr[ZG_R9].val;

    // Taint: tainted values must not reach syscalls (including the syscall
    // number in RAX: a poisoned number would dispatch to a secret-dependent
    // syscall).
    {
        const int regs[7] = {ZG_RAX, ZG_RDI, ZG_RSI, ZG_RDX,
                             ZG_R10, ZG_R8, ZG_R9};
        for (int i = 0; i < 7; i++) {
            if (cpu->gpr[regs[i]].taint.cannot_branch || cpu->gpr[regs[i]].taint.cannot_index)
                guest_error(cpu, "tainted value passed to syscall");
        }
    }

    // Poison validation for known syscall buffer layouts (unmapped faults
    // are still the host's EFAULT; see syscall_check_buffers).
    syscall_check_buffers(cpu, n, a1, a2, a3, a4, a5, a6);

    if (cpu->emu->trace_syscall) {
        fprintf(stderr, "[zegarmistrz:tid=%d rip=%#lx] syscall %s(%lu) args=%lx,%lx,%lx,%lx,%lx,%lx\n",
                cpu->tid, cpu->rip, syscall_name((long)n), n, a1, a2, a3, a4, a5, a6);
        fflush(stderr);
    }

    uint64_t ret = 0;

    switch (n) {
    case SYS_brk:
        ret = do_brk(cpu, a1);
        break;
    case SYS_arch_prctl: {
        // Virtualize: never touch the host thread's FS/GS.
        if (a1 == ARCH_SET_FS) {
            cpu->fs_base = a2;
            ret = 0;
        } else if (a1 == ARCH_GET_FS) {
            mem_store64(cpu, a2, cpu->fs_base);
            ret = 0;
        } else if (a1 == ARCH_SET_GS) {
            cpu->gs_base = a2;
            ret = 0;
        } else if (a1 == ARCH_GET_GS) {
            mem_store64(cpu, a2, cpu->gs_base);
            ret = 0;
        } else {
            ret = (uint64_t)(-(long)EINVAL);
        }
        break;
    }
    case SYS_clone3: {
        // Advance past the syscall first: the child copy must resume AFTER
        // it (with RAX=0), not re-execute it.
        cpu->rip += insn_len;
        cpu->gpr[ZG_RCX].val = cpu->rip;
        cpu->gpr[ZG_RCX].taint.cannot_branch = false;
        cpu->gpr[ZG_RCX].taint.cannot_index = false;
        cpu->gpr[ZG_R11].val = cpu->rflags;
        cpu->gpr[ZG_R11].taint.cannot_branch = false;
        cpu->gpr[ZG_R11].taint.cannot_index = false;
        ret = do_clone3(cpu, a1, a2);
        cpu->gpr[ZG_RAX].val = ret;
        cpu->gpr[ZG_RAX].taint.cannot_branch = false;
        cpu->gpr[ZG_RAX].taint.cannot_index = false;
        if (cpu->emu->trace_syscall) {
            fprintf(stderr, "[zegarmistrz:tid=%d]  -> %#lx\n", cpu->tid, ret);
            fflush(stderr);
        }
        return;
    }
    case SYS_clone: {
        // clone(flags, stack, parent_tid, tls, child_tid)
        cpu->rip += insn_len;
        cpu->gpr[ZG_RCX].val = cpu->rip;
        cpu->gpr[ZG_RCX].taint.cannot_branch = false;
        cpu->gpr[ZG_RCX].taint.cannot_index = false;
        cpu->gpr[ZG_R11].val = cpu->rflags;
        cpu->gpr[ZG_R11].taint.cannot_branch = false;
        cpu->gpr[ZG_R11].taint.cannot_index = false;
        ret = do_clone(cpu, a1, a2, a3, a4, a5);
        cpu->gpr[ZG_RAX].val = ret;
        cpu->gpr[ZG_RAX].taint.cannot_branch = false;
        cpu->gpr[ZG_RAX].taint.cannot_index = false;
        if (cpu->emu->trace_syscall) {
            fprintf(stderr, "[zegarmistrz:tid=%d]  -> %#lx\n", cpu->tid, ret);
            fflush(stderr);
        }
        return;
    }
    case SYS_exit: {
        // Thread exit: end only this thread (glibc pthread_exit path).
        // The main thread exiting ends the process.
        int code = (int)a1;
        if (cpu->emu->trace_syscall) {
            fprintf(stderr, "[zegarmistrz:tid=%d] exit(%d)\n", cpu->tid, code);
            fflush(stderr);
        }
        if (cpu->tid == cpu->emu->main_tid) {
            cpu->emu->exit_code = code;
            cpu->emu->exiting = true;
            fflush(stdout);
            fflush(stderr);
            _exit(code);
        }
        // Worker exit: unregister before the thread dies so thread_cpus never
        // holds a dangling pointer, clear CLONE_CHILD_CLEARTID, free the heap
        // CPU, then die. (The run_cpu_loop return path in child_thread_main2
        // handles the exit_group case; pthread_exit unwinds past it, so the
        // cleanup must happen here and must not touch cpu afterwards.)
        {
            uint64_t ctid = cpu->clear_ctid_addr;
            Emulator* emu = cpu->emu;
            (void)emu;
            thread_unregister_cpu(cpu);
            if (ctid) {
                *(volatile int*)(uintptr_t)ctid = 0;
                ::syscall(SYS_futex, (void*)(uintptr_t)ctid, FUTEX_WAKE, 1,
                          nullptr, nullptr, 0);
                ::syscall(SYS_futex, (void*)(uintptr_t)ctid,
                          FUTEX_WAKE | FUTEX_PRIVATE_FLAG, 1, nullptr, nullptr,
                          0);
            }
            delete cpu;
        }
        pthread_exit(NULL);
        break;
    }
    case SYS_exit_group: {
        int code = (int)a1;
        cpu->emu->exit_code = code;
        cpu->emu->exiting = true;
        if (cpu->emu->trace_syscall) {
            fprintf(stderr, "[zegarmistrz:tid=%d] exit_group(%d)\n", cpu->tid,
                    code);
            fflush(stderr);
        }
        // End the whole process immediately; helper threads die with it.
        fflush(stdout);
        fflush(stderr);
        _exit(code);
        break;
    }
    case SYS_execve:
    case SYS_execveat:
        ret = (uint64_t)(-(long)ENOSYS);
        break;
    case 57: // fork
    case 58: // vfork
        ret = (uint64_t)(-(long)ENOSYS);
        break;
    case SYS_rt_sigaction:
    case SYS_rt_sigprocmask:
        // Phase 1: signals are process-hostile under emulation; stub as
        // no-op success.
        ret = 0;
        break;
    case SYS_set_robust_list:
        cpu->robust_list = a1;
        // fallthrough to host so the kernel can do its job on our thread.
    case SYS_rseq:
    case SYS_set_tid_address:
    case SYS_gettid:
    case SYS_futex:
    case SYS_madvise:
        goto passthrough;
    case SYS_mmap:
        ret = do_mmap(cpu, a1, a2, a3, a4, a5, a6);
        break;
    case SYS_mprotect: {
        errno = 0;
        long r = ::syscall(SYS_mprotect, (void*)(uintptr_t)a1, (size_t)a2,
                           (int)a3);
        if (r == -1)
            ret = (uint64_t)(-(long)errno);
        else {
            ret = 0;
            // The kernel operates on whole pages; align so guest_maps never
            // keeps an unmapped tail (a stale tail would crash core dumps).
            uint64_t s = page_align_down(a1);
            uint64_t e = page_align_up(a1 + a2);
            maps_set_prot(cpu->emu, s, e - s, (int)a3);
        }
        break;
    }
    case SYS_munmap: {
        errno = 0;
        long r =
            ::syscall(SYS_munmap, (void*)(uintptr_t)a1, (size_t)a2);
        if (r == -1)
            ret = (uint64_t)(-(long)errno);
        else {
            ret = 0;
            uint64_t s = page_align_down(a1);
            uint64_t e = page_align_up(a1 + a2);
            maps_remove_range(cpu->emu, s, e - s);
        }
        break;
    }
    case SYS_mremap: {
        // mremap(old, old_size, new_size, flags, new_addr).
        int prot = PROT_READ | PROT_WRITE; // best effort if unknown
        {
            std::lock_guard<std::mutex> l(cpu->emu->maps_mu);
            for (auto& m : cpu->emu->guest_maps) {
                if (m.start <= a1 && m.end >= a1 + page_align_up(a2)) {
                    prot = m.prot;
                    break;
                }
            }
        }
        errno = 0;
        long r = ::syscall(SYS_mremap, (void*)(uintptr_t)a1, (size_t)a2,
                           (size_t)a3, (int)a4, (void*)(uintptr_t)a5);
        if (r == -1)
            ret = (uint64_t)(-(long)errno);
        else {
            uint64_t base = (uint64_t)(uintptr_t)r;
            uint64_t nlen = page_align_up(a3);
            uint64_t os = page_align_down(a1);
            uint64_t oe = page_align_up(a1 + a2);
            maps_remove_range(cpu->emu, os, oe - os);
            maps_remove_range(cpu->emu, base, nlen);
            maps_add(cpu->emu, base, base + nlen, prot, "guest-mremap");
            ret = base;
        }
        break;
    }
    passthrough:
    default: {
        errno = 0;
        long r = ::syscall((long)n, (void*)(uintptr_t)a1, (void*)(uintptr_t)a2,
                           (void*)(uintptr_t)a3, (void*)(uintptr_t)a4,
                           (void*)(uintptr_t)a5, (void*)(uintptr_t)a6);
        if (r == -1)
            ret = (uint64_t)(-(long)errno);
        else
            ret = (uint64_t)r;
        // Record rseq/robust addresses for thread bookkeeping.
        if (n == (uint64_t)SYS_rseq && ret == 0)
            cpu->rseq_addr = a1;
        break;
    }
    }

    cpu->gpr[ZG_RAX].val = ret;
    cpu->gpr[ZG_RAX].taint.cannot_branch = false;
    cpu->gpr[ZG_RAX].taint.cannot_index = false;
    // syscall ABI: RCX = return RIP, R11 = RFLAGS.
    cpu->rip += insn_len;
    cpu->gpr[ZG_RCX].val = cpu->rip;
    cpu->gpr[ZG_RCX].taint.cannot_branch = false;
    cpu->gpr[ZG_RCX].taint.cannot_index = false;
    cpu->gpr[ZG_R11].val = cpu->rflags;
    cpu->gpr[ZG_R11].taint.cannot_branch = false;
    cpu->gpr[ZG_R11].taint.cannot_index = false;

    if (cpu->emu->trace_syscall) {
        fprintf(stderr, "[zegarmistrz:tid=%d]  -> %#lx\n", cpu->tid, ret);
        fflush(stderr);
    }
}
