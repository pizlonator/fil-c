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
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/futex.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#include <sys/times.h>
#include <sys/timex.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

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

// The kernel's sched_getattr/sched_setattr ABI struct size, measured from
// the kernel header itself (<linux/sched/types.h>; SCHED_ATTR_SIZE_VER1).
// The header cannot be included at file scope here because it defines
// struct sched_param unguarded and glibc's <sched.h> defines it too; pull
// it into its own namespace so only the measurement (not the struct) is
// visible, which keeps the extent table from guessing the size.
namespace zeg_sched_attr_probe {
#undef _LINUX_SCHED_TYPES_H
#include <linux/sched/types.h>
}
// Kernel writes min(size, this many) bytes to sched_getattr's output buffer.
static constexpr size_t ZEG_SCHED_ATTR_KERNEL_SIZE =
    sizeof(zeg_sched_attr_probe::sched_attr); // 56: VER0 48 + util_min/max

// Forward declarations for guest_maps helpers defined below (do_brk needs
// them before their definitions; the defaulted bias params live here so
// every call site can use them).
struct Emulator;
static void maps_add(Emulator* emu, uint64_t start, uint64_t end, int prot,
                     const char* path, uint64_t bias = 0,
                     bool bias_valid = false);
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

template<typename TaintT>
static uint64_t do_brk(CPU<TaintT>* cpu, uint64_t addr) {
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
            // Fresh anonymous pages from the host kernel: clear any stale
            // poison over the grown range (kernel-write modeling for the
            // emulator-side brk growth).
            mem_unpoison_range(emu, lo, hi - lo);
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

// Resolve the file path of fd (via /proc/self/fd) and compute the load bias
// (guest VA - ELF vaddr) for a mapping placed at `base` covering file
// offset `off`. ld.so maps ELF objects so each segment's mapping starts at
// page_align_down(vaddr) with file offset p_offset - (p_vaddr & 0xfff)
// (glibc's single whole-span map satisfies this for the first PT_LOAD too,
// since p_offset ≡ p_vaddr mod page size). When off matches that layout for
// some PT_LOAD, the bias is exact: bias = base - off + p_offset - p_vaddr.
// Returns false for non-files, non-ELF64-x86-64 images, and mappings whose
// offset does not match a segment start (bias unknown).
static bool file_mapping_info(int fd, uint64_t base, uint64_t off,
                              std::string* path, uint64_t* bias) {
    if (fd < 0)
        return false;
    char fdpath[64];
    snprintf(fdpath, sizeof(fdpath), "/proc/self/fd/%d", fd);
    char link[4096];
    ssize_t n = readlink(fdpath, link, sizeof(link) - 1);
    if (n <= 0)
        return false;
    link[n] = '\0';
    Elf64_Ehdr eh;
    if (pread(fd, &eh, sizeof(eh), 0) != (ssize_t)sizeof(eh))
        return false;
    if (memcmp(eh.e_ident, ELFMAG, SELFMAG) != 0 ||
        eh.e_ident[EI_CLASS] != ELFCLASS64 ||
        eh.e_ident[EI_DATA] != ELFDATA2LSB ||
        eh.e_machine != EM_X86_64 ||
        eh.e_phentsize != sizeof(Elf64_Phdr) || eh.e_phnum == 0 ||
        eh.e_phnum > 4096)
        return false;
    std::vector<Elf64_Phdr> ph(eh.e_phnum);
    if (pread(fd, ph.data(), ph.size() * sizeof(Elf64_Phdr), eh.e_phoff) !=
        (ssize_t)(ph.size() * sizeof(Elf64_Phdr)))
        return false;
    for (const auto& p : ph) {
        if (p.p_type != PT_LOAD)
            continue;
        // p_offset ≡ p_vaddr (mod page size), so the underhangs agree.
        if (off == p.p_offset - (p.p_vaddr & 0xfff)) {
            *bias = (uint64_t)((int64_t)base - (int64_t)off +
                               (int64_t)p.p_offset - (int64_t)p.p_vaddr);
            *path = link;
            return true;
        }
    }
    return false;
}

static void maps_add(Emulator* emu, uint64_t start, uint64_t end, int prot,
                     const char* path, uint64_t bias, bool bias_valid) {
    if (end <= start)
        return;
    std::lock_guard<std::mutex> l(emu->maps_mu);
    Emulator::GuestMap gm;
    gm.start = start;
    gm.end = end;
    gm.prot = prot;
    if (path)
        gm.path = path;
    gm.bias = bias;
    gm.bias_valid = bias_valid;
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

// Offending value's poison trail for GuestError ({} for plain Taint): a
// list of ZegHistSteps, one per recorded operation (the operation's PC + its
// recording seq; the deep modes keep the recorded stack in each step's
// callers).
template<typename TaintT>
static std::vector<ZegHistStep> poison_history_of(const TaintT& t) {
    if constexpr (TaintT::has_history)
        return t.history;
    else
        return {};
}

template<typename TaintT>
static void sys_check_mem(CPU<TaintT>* cpu, uint64_t addr, size_t len, bool want_write) {
    if (!len)
        return;
    if (want_write)
        mem_check_store(cpu, addr, len);
    else
        mem_check_load(cpu, addr, len);
}

template<typename TaintT>
static void sys_check_cstr(CPU<TaintT>* cpu, uint64_t addr) {
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
// keep raw passthrough (their layouts are not modeled here -- the
// POST-syscall kernel-write extents are modeled separately, and rejected
// by crashing, in syscall_kernel_writes below).
template<typename TaintT>
static void syscall_check_buffers(CPU<TaintT>* cpu, uint64_t n, uint64_t a1,
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

// --- kernel-write extent modeling (problems.txt #2) --------------------------
// Guest VA == host VA, so a host syscall that fills a guest buffer writes
// guest memory DIRECTLY, bypassing the poison map: stale poison on those
// bytes survives the kernel's overwrite and then false-positives when the
// guest branches on the kernel-written data (the original report: fstat's
// struct stat landing on stack slots that deeper, poisoned crypto frames
// had spilled taint into; the S_ISCHR(st.st_mode) branch inside
// __GI__IO_file_doallocate then faults on the laundered poison). Every
// byte the kernel overwrites is replaced with fresh, non-guest-derived
// data, so the model clears it (mem_unpoison_range) after the syscall.
//
// What the kernel writes to guest memory for a syscall, computed AFTER the
// syscall ran with its return value known. Every syscall the emulator can
// pass through must be classified: extents modeled (out filled), known to
// write no guest memory (out left empty), or unknown -- in which case the
// caller must crash rather than let a kernel overwrite silently launder
// stale poison. Returns false only for the unknown class.
struct ZegKernelWrite {
    uint64_t addr;
    size_t size;
};

// Record an output extent, clamping a wrapping end. A null address is never
// an extent (the kernel cannot write there); callers null-check pointer
// arguments so "argument absent" and "argument null" read the same.
static void zeg_kw_push(std::vector<ZegKernelWrite>& out, uint64_t addr,
                        uint64_t size) {
    if (!addr || !size)
        return;
    if (addr + size < addr)
        size = UINT64_MAX - addr;
    out.push_back(ZegKernelWrite{addr, (size_t)size});
}

// Guarded host read of guest CONTROL data (iovec arrays, socklen words,
// msghdr fields). Deliberately NOT mem_load_bytes: the kernel already
// wrote/validated these bytes, so the model must not fault on the poison
// map while reading them; a failed (unmapped) read instead leaves the
// syscall UNMODELABLE.
static bool zeg_kw_read(uint64_t addr, void* out, size_t n) {
    return mem_copy_guarded(addr, out, n);
}

// Distribute `ret` bytes across the iovec array at iov_addr (the kernel
// fills iov[0] up to iov_len, then iov[1], ...). Count is clamped to 1024
// like syscall_check_buffers. Per-iov lengths are used as-is: the kernel
// wrote exactly `ret` bytes total, so each iov_len only bounds its share
// and clamping it would under-clear large iovs. (recvmsg's control-data
// extent keeps its own 0x8000 budget clamp at its case below.) Any
// guarded read failure makes the syscall UNMODELABLE.
template<typename TaintT>
static bool zeg_kw_distribute_iovs(CPU<TaintT>* cpu, uint64_t iov_addr,
                                   uint64_t iovcnt, uint64_t ret,
                                   std::vector<ZegKernelWrite>& out) {
    (void)cpu;
    if (iovcnt > 1024)
        return false;
    uint64_t left = ret;
    for (uint64_t i = 0; i < iovcnt && left; i++) {
        uint64_t base = 0;
        uint64_t len = 0;
        if (!zeg_kw_read(iov_addr + i * sizeof(struct iovec), &base, 8) ||
            !zeg_kw_read(iov_addr + i * sizeof(struct iovec) + 8, &len, 8))
            return false;
        uint64_t chunk = len < left ? len : left;
        zeg_kw_push(out, base, chunk);
        left -= chunk;
    }
    return true;
}

// Socket output pattern: the kernel writes the address/value bytes into
// the buffer and updates the socklen_t word itself. Only the POST-syscall
// length is recoverable (the kernel overwrote the caller's value in
// place), and the kernel never writes beyond what it reports -- except in
// the truncate-and-report-full-length case, whose over-clear is bounded by
// the sockaddr_storage clamp (128 bytes) below.
static bool zeg_kw_socklen(std::vector<ZegKernelWrite>& out, uint64_t buf,
                           uint64_t lenp, uint64_t clamp) {
    uint32_t len = 0;
    if (!zeg_kw_read(lenp, &len, 4))
        return false;
    if (buf && len)
        zeg_kw_push(out, buf, len < clamp ? len : clamp);
    zeg_kw_push(out, lenp, 4);
    return true;
}

// Kernel struct termios (SYS_ioctl TCGETS output): 4 tcflag_t + cc_t
// c_line + cc_t c_cc[19] = 36 bytes. glibc's <termios.h> struct termios is
// NOT this struct (it has c_cc[32] and is 60 bytes), so the size is fixed
// per the kernel's <asm/termios.h> layout instead of sizeof().
static constexpr size_t ZEG_KERNEL_TERMIOS_SIZE = 36;
// Kernel struct termio (TCGETA): 4 unsigned short + unsigned char c_line +
// unsigned char c_cc[8] = 17, padded to 18. Same reason: not sizeof(struct
// termio).
static constexpr size_t ZEG_KERNEL_TERMIO_SIZE = 18;

template<typename TaintT>
static bool syscall_kernel_writes(CPU<TaintT>* cpu, uint64_t n,
                                  uint64_t a1, uint64_t a2, uint64_t a3,
                                  uint64_t a4, uint64_t a5, uint64_t a6,
                                  int64_t sret,
                                  std::vector<ZegKernelWrite>& out) {
    // Failure: the kernel wrote nothing. The one exception is the sleep
    // calls, which report the remaining time through the out buffer when
    // interrupted (clock_nanosleep with TIMER_ABSTIME never writes its
    // remain buffer, but glibc never passes TIMER_ABSTIME, and treating it
    // as written errs toward clearing fresh data rather than faulting on
    // stale poison).
    if (sret < 0) {
        if (sret == -EINTR) {
            if (n == (uint64_t)SYS_nanosleep && a2)
                zeg_kw_push(out, a2, sizeof(struct timespec));
            if (n == (uint64_t)SYS_clock_nanosleep && a4)
                zeg_kw_push(out, a4, sizeof(struct timespec));
        }
        return true;
    }

    switch (n) {
    // --- reads and read-like fills: ret bytes at the buffer --------------
    case SYS_read:
        if (sret > 0)
            zeg_kw_push(out, a2, (uint64_t)sret);
        return true;
    case SYS_pread64:
        if (sret > 0)
            zeg_kw_push(out, a2, (uint64_t)sret);
        return true;
    case SYS_readv:
        if (sret > 0)
            return zeg_kw_distribute_iovs(cpu, a2, a3, (uint64_t)sret, out);
        return true;
    case SYS_preadv:
    case SYS_preadv2:
        if (sret > 0)
            return zeg_kw_distribute_iovs(cpu, a2, a3, (uint64_t)sret, out);
        return true;
    case SYS_getrandom:
        if (sret > 0)
            zeg_kw_push(out, a1, (uint64_t)sret);
        return true;
    case SYS_getcwd:
        if (sret > 0)
            zeg_kw_push(out, a1, (uint64_t)sret);
        return true;
    case SYS_getdents:
    case SYS_getdents64:
        if (sret > 0)
            zeg_kw_push(out, a2, (uint64_t)sret);
        return true;
    case SYS_readlink:
        if (sret > 0)
            zeg_kw_push(out, a2, (uint64_t)sret);
        return true;
    case SYS_readlinkat:
        if (sret > 0)
            zeg_kw_push(out, a3, (uint64_t)sret);
        return true;
    case SYS_syslog:
        if (sret > 0)
            zeg_kw_push(out, a2, (uint64_t)sret);
        return true;

    // --- stat family: fixed-size output structs --------------------------
    case SYS_fstat:
    case SYS_stat:
    case SYS_lstat:
        // struct stat: 144 on x86-64, identical for the host glibc and the
        // kernel ABI.
        zeg_kw_push(out, a2, sizeof(struct stat));
        return true;
    case SYS_newfstatat:
        // fstat(fd, buf) is newfstatat(fd, "", buf, AT_EMPTY_PATH) on this
        // glibc; the buf is the third argument.
        zeg_kw_push(out, a3, sizeof(struct stat));
        return true;
    case SYS_statx:
        zeg_kw_push(out, a5, sizeof(struct statx)); // 256
        return true;
    case SYS_statfs:
        zeg_kw_push(out, a2, sizeof(struct statfs)); // 120
        return true;
    case SYS_fstatfs:
        zeg_kw_push(out, a2, sizeof(struct statfs));
        return true;

    // --- resource / process info -----------------------------------------
    case SYS_getrlimit:
        zeg_kw_push(out, a2, sizeof(struct rlimit)); // 16
        return true;
    case SYS_prlimit64:
        // prlimit64(pid, resource, new, old): the old limit is written back.
        if (a4)
            zeg_kw_push(out, a4, sizeof(struct rlimit));
        return true;
    case SYS_getrusage:
        zeg_kw_push(out, a2, sizeof(struct rusage)); // 144
        return true;
    case SYS_wait4:
        if (a2)
            zeg_kw_push(out, a2, 4); // int status
        if (a3)
            zeg_kw_push(out, a3, sizeof(struct rusage));
        return true;
    case SYS_waitid:
        // waitid(idtype, id, infop, options, rusage): the kernel fills the
        // siginfo at a3 and, when non-NULL, the rusage at a5 (like wait4's
        // a3 above).
        if (a3)
            zeg_kw_push(out, a3, sizeof(siginfo_t)); // 128
        if (a5)
            zeg_kw_push(out, a5, sizeof(struct rusage));
        return true;
    case SYS_times:
        if (a1)
            zeg_kw_push(out, a1, sizeof(struct tms)); // 32
        return true;

    // --- clocks -----------------------------------------------------------
    case SYS_clock_gettime:
        zeg_kw_push(out, a2, sizeof(struct timespec));
        return true;
    case SYS_clock_getres:
        if (a2)
            zeg_kw_push(out, a2, sizeof(struct timespec));
        return true;
    case SYS_gettimeofday:
        if (a1)
            zeg_kw_push(out, a1, sizeof(struct timeval));
        if (a2)
            zeg_kw_push(out, a2, sizeof(struct timezone)); // 8
        return true;
    case SYS_time:
        if (a1)
            zeg_kw_push(out, a1, 8); // time_t
        return true;
    case SYS_adjtimex:
        zeg_kw_push(out, a1, sizeof(struct timex)); // 208
        return true;
    case SYS_clock_adjtime:
        zeg_kw_push(out, a2, sizeof(struct timex));
        return true;
    case SYS_nanosleep:
    case SYS_clock_nanosleep:
        // Success: the remaining-time buffer is NOT written (only the
        // -EINTR path writes it, handled above).
        return true;

    // --- scheduler / affinity / cpu info ----------------------------------
    case SYS_sched_getaffinity:
        // ret is the number of bytes written into the mask.
        if (sret > 0)
            zeg_kw_push(out, a3, (uint64_t)sret);
        return true;
    case SYS_sched_getparam:
        // struct sched_param on the kernel ABI is a single int.
        zeg_kw_push(out, a2, 4);
        return true;
    case SYS_sched_getattr:
        // The kernel fills min(size, sizeof(struct sched_attr)); the size
        // comes from the kernel header (see the probe at the top of this
        // file).
        zeg_kw_push(out, a2, a3 < ZEG_SCHED_ATTR_KERNEL_SIZE
                                  ? (size_t)a3
                                  : ZEG_SCHED_ATTR_KERNEL_SIZE);
        return true;
    case SYS_sched_rr_get_interval:
        zeg_kw_push(out, a2, sizeof(struct timespec));
        return true;
    case SYS_getcpu:
        if (a1)
            zeg_kw_push(out, a1, 4); // unsigned int cpu
        if (a2)
            zeg_kw_push(out, a2, 4); // unsigned int node
        return true;
    case SYS_getresuid:
    case SYS_getresgid:
        // uid_t/gid_t out pointers, each 4 bytes.
        if (a1)
            zeg_kw_push(out, a1, 4);
        if (a2)
            zeg_kw_push(out, a2, 4);
        if (a3)
            zeg_kw_push(out, a3, 4);
        return true;
    case SYS_getgroups:
        if (sret > 0)
            zeg_kw_push(out, a3, (uint64_t)sret * 4); // gid_t[ret]
        return true;
    case SYS_mincore:
        // One byte per page of [a1, a1+a2).
        zeg_kw_push(out, a3, (a2 + 4095) / 4096);
        return true;

    // --- poll / select / epoll --------------------------------------------
    case SYS_poll:
    case SYS_ppoll: {
        // The kernel writes only the revents short of each pollfd
        // (offset 6 in the 8-byte struct pollfd).
        uint64_t nfds = a3 < 65536 ? a3 : 65536;
        for (uint64_t i = 0; i < nfds; i++)
            zeg_kw_push(out, a1 + i * sizeof(struct pollfd) + 6, 2);
        return true;
    }
    case SYS_select:
    case SYS_pselect6:
        // fd_set writebacks: FD_SETSIZE bits = 128 bytes each.
        if (a2)
            zeg_kw_push(out, a2, 128);
        if (a3)
            zeg_kw_push(out, a3, 128);
        if (a4)
            zeg_kw_push(out, a4, 128);
        return true;
    case SYS_epoll_wait:
    case SYS_epoll_pwait:
    case SYS_epoll_pwait2:
        if (sret > 0)
            zeg_kw_push(out, a2,
                        (uint64_t)sret * sizeof(struct epoll_event)); // 12 packed
        return true;

    // --- descriptors --------------------------------------------------------
    case SYS_pipe:
    case SYS_pipe2:
        zeg_kw_push(out, a1, 2 * 4); // int fds[2]
        return true;
    case SYS_socketpair:
        zeg_kw_push(out, a4, 2 * 4); // int sv[2]
        return true;
    case SYS_get_robust_list:
        // (pid, head_ptr, len_ptr): two written-back pointers.
        zeg_kw_push(out, a2, 8);
        zeg_kw_push(out, a3, 8);
        return true;

    // --- signals ------------------------------------------------------------
    case SYS_rt_sigpending: {
        // (set, sigsetsize): the kernel fills sigsetsize bytes (bounded by
        // the kernel's largest sigset).
        uint64_t sz = a3 < 128 ? a3 : 128;
        zeg_kw_push(out, a2, sz);
        return true;
    }
    case SYS_rt_sigtimedwait:
        if (a2)
            zeg_kw_push(out, a2, sizeof(siginfo_t)); // 128
        return true;
    case SYS_sigaltstack:
        // (uss, oss): the current stack state is written to oss.
        if (a2)
            zeg_kw_push(out, a2, sizeof(stack_t)); // 24
        return true;
    case SYS_timerfd_gettime:
        zeg_kw_push(out, a2, sizeof(struct itimerspec)); // 32
        return true;
    case SYS_timerfd_settime:
        // timerfd_settime(fd, flags, new_value, old_value): new_value (a3)
        // is the caller's INPUT -- the kernel never writes it, and clearing
        // it would launder stale poison -- so only the previous setting,
        // written to old_value (a4), is modeled.
        if (a4)
            zeg_kw_push(out, a4, sizeof(struct itimerspec));
        return true;

    // --- fcntl / futex / madvise ---------------------------------------------
    case SYS_fcntl:
        // Only the lock-query commands fill the caller's struct flock;
        // every other command returns through RAX and writes nothing.
        if (a2 == (uint64_t)F_GETLK || a2 == (uint64_t)F_OFD_GETLK)
            zeg_kw_push(out, a3, sizeof(struct flock)); // 32
        return true;
    case SYS_futex:
        // FUTEX_LOCK_PI: on success the kernel writes the TID word into the
        // futex word (the lock owner). All other ops return through RAX
        // (WAIT/WAKE et al) and write nothing to user memory.
        if ((uint32_t)(a2 & ~(uint64_t)FUTEX_PRIVATE_FLAG) ==
                (uint32_t)FUTEX_LOCK_PI &&
            sret == 0)
            zeg_kw_push(out, a1, 4);
        return true;
    case SYS_madvise:
        // MADV_DONTNEED drops the pages: the range comes back zeroed, so
        // the kernel effectively rewrote every byte of it. Other advice
        // (WILLNEED, PAGEOUT, ...) only reads.
        if (a3 == (uint64_t)MADV_DONTNEED)
            zeg_kw_push(out, a1, a2);
        return true;

    // --- sockets -------------------------------------------------------------
    case SYS_accept:
    case SYS_accept4:
        if (a3)
            return zeg_kw_socklen(out, a2, a3,
                                  sizeof(struct sockaddr_storage));
        return true;
    case SYS_getsockname:
    case SYS_getpeername:
        if (a3)
            return zeg_kw_socklen(out, a2, a3,
                                  sizeof(struct sockaddr_storage));
        return true;
    case SYS_getsockopt:
        // (fd, level, optname, optval, optlen): the value goes to a4, the
        // updated length to a5 (the kernel reports what it wrote; no
        // sockaddr clamp here since optvals are not addresses).
        if (a5)
            return zeg_kw_socklen(out, a4, a5, UINT64_MAX);
        return true;
    case SYS_recvfrom:
        // recvfrom(fd, buf, len, flags, src_addr, addrlen): the data extent
        // is [a2, ret) for ret > 0, and the kernel-written source address
        // lands at a5 with the updated socklen_t word at a6. a4 is the
        // flags VALUE -- never a buffer (modeling it as one pushed a bogus
        // extent at the flags value and left the real sockaddr/addrlen
        // writebacks poisoned).
        if (sret > 0)
            zeg_kw_push(out, a2, (uint64_t)sret);
        if (a6)
            return zeg_kw_socklen(out, a5, a6,
                                  sizeof(struct sockaddr_storage));
        return true;
    case SYS_recvmsg: {
        // The kernel updates the caller's msghdr in place (msg_namelen,
        // msg_controllen, msg_flags); read the post-syscall fields. The
        // pre-syscall values cannot be recovered, but the kernel never
        // writes beyond what it reports (min(pre, post) == post), so the
        // reported lengths bound the true extents. x86-64 struct msghdr:
        // msg_name 8 @0, msg_namelen u32 @8, msg_iov 8 @16, msg_iovlen 8
        // @24, msg_control 8 @32, msg_controllen 8 @40, msg_flags u32 @48.
        uint64_t msg_name = 0;
        uint32_t msg_namelen = 0;
        uint64_t msg_iov = 0;
        uint64_t msg_iovlen = 0;
        uint64_t msg_control = 0;
        uint64_t msg_controllen = 0;
        if (!zeg_kw_read(a2 + 0, &msg_name, 8) ||
            !zeg_kw_read(a2 + 8, &msg_namelen, 4) ||
            !zeg_kw_read(a2 + 16, &msg_iov, 8) ||
            !zeg_kw_read(a2 + 24, &msg_iovlen, 8) ||
            !zeg_kw_read(a2 + 32, &msg_control, 8) ||
            !zeg_kw_read(a2 + 40, &msg_controllen, 8))
            return false; // UNMODELABLE: msghdr unreadable
        zeg_kw_push(out, a2 + 48, 4); // msg_flags writeback
        if (msg_name && msg_namelen)
            zeg_kw_push(out, msg_name,
                        msg_namelen < sizeof(struct sockaddr_storage)
                            ? msg_namelen
                            : sizeof(struct sockaddr_storage));
        if (msg_control && msg_controllen)
            zeg_kw_push(out, msg_control,
                        msg_controllen < 0x8000 ? msg_controllen : 0x8000);
        if (sret > 0)
            return zeg_kw_distribute_iovs(cpu, msg_iov, msg_iovlen,
                                          (uint64_t)sret, out);
        return true;
    }
    case SYS_msgrcv:
        // msgrcv(msqid, msgp, msgsz, msgtyp, msgflg): the kernel writes the
        // long msgtyp back at msgp, then the payload at msgp + 8.
        zeg_kw_push(out, a2, 8);
        if (sret > 0)
            zeg_kw_push(out, a2 + 8, (uint64_t)sret);
        return true;
    case SYS_process_vm_readv:
        // (pid, local_iov, liovcnt, remote_iov, riovcnt, flags): the
        // remote data lands in the LOCAL iovec array.
        if (sret > 0)
            return zeg_kw_distribute_iovs(cpu, a2, a3, (uint64_t)sret, out);
        return true;

    // --- xattr ---------------------------------------------------------------
    case SYS_getxattr:
    case SYS_lgetxattr:
    case SYS_fgetxattr:
        // (path|fd, name, value, size): the value pointer is a3 in every
        // form (checked against <sys/xattr.h>).
        if (sret > 0)
            zeg_kw_push(out, a3, (uint64_t)sret);
        return true;
    case SYS_listxattr:
    case SYS_llistxattr:
    case SYS_flistxattr:
        // (path|fd, list, size): the list pointer is a2.
        if (sret > 0)
            zeg_kw_push(out, a2, (uint64_t)sret);
        return true;

    // --- prctl -----------------------------------------------------------------
    case SYS_prctl:
        // PR_GET_NAME writes the 16-byte name buffer. Six PR_GET_* commands
        // write 4 bytes through the a2 pointer and return 0:
        // PR_GET_PDEATHSIG, PR_GET_UNALIGN, PR_GET_FPEMU, PR_GET_TSC,
        // PR_GET_ENDIAN, and PR_GET_TAGGED_ADDR_CTRL. On x86-64 several of
        // these fail with -EINVAL (a failed syscall writes nothing, handled
        // above), and the kernels that succeed PR_GET_TAGGED_ADDR_CTRL
        // require a null a2, so the 4-byte extent only fires where an ABI
        // actually writes through the pointer. The remaining PR_GET_*
        // commands (PR_GET_DUMPABLE, PR_GET_KEEPCAPS, PR_GET_TIMERSLACK,
        // PR_GET_CHILD_SUBREAPER, PR_GET_SECCOMP, ...) report through RAX
        // and write nothing; everything else reads.
        if (a1 == (uint64_t)PR_GET_NAME)
            zeg_kw_push(out, a2, 16);
        else if (a1 == (uint64_t)PR_GET_PDEATHSIG ||
                 a1 == (uint64_t)PR_GET_UNALIGN ||
                 a1 == (uint64_t)PR_GET_FPEMU ||
                 a1 == (uint64_t)PR_GET_TSC ||
                 a1 == (uint64_t)PR_GET_ENDIAN ||
                 a1 == (uint64_t)PR_GET_TAGGED_ADDR_CTRL)
            zeg_kw_push(out, a2, 4);
        return true;

    // --- ioctl: the heuristic ---------------------------------------------------
    // ioctls are the hardest: the kernel ABI is request-specific and many
    // requests are driver-defined. Priority order:
    //   1. a3 == 0: no argument buffer at all -> nothing written.
    //   2. Classic non-_IOC requests with a fixed output size (the
    //      0x54xx tty / 0x89xx socket style): table lookup; any OTHER
    //      old-style request is UNMODELABLE (their layouts are per-driver
    //      and we refuse to guess).
    //   3. _IOC-decoded requests: x86-64 _IOC_NRBITS=8, _IOC_TYPEBITS=8,
    //      _IOC_SIZEBITS=14, _IOC_DIRBITS=2, so dir = (req >> 30) & 3 and
    //      size = (req >> 16) & 0x3fff. Direction semantics are confusing:
    //      _IOC_WRITE (1) means the kernel READS the user buffer, and
    //      _IOC_READ (2) means the kernel WRITES into it (reads from the
    //      device). A _IOC_READ request with size 0 is malformed ->
    //      UNMODELABLE; a non-reading request writes nothing.
    //   4. Anything else -> UNMODELABLE.
    case SYS_ioctl: {
        uint64_t req = a2;
        if (!a3)
            return true; // no argument buffer at all
        if ((req & ~0xffffULL) == 0) {
            // Old-style (pre-_IOC) request number.
            switch ((int)req) {
            case TCGETS:
                zeg_kw_push(out, a3, ZEG_KERNEL_TERMIOS_SIZE);
                break;
            case TCGETA:
                zeg_kw_push(out, a3, ZEG_KERNEL_TERMIO_SIZE);
                break;
            case TIOCGWINSZ:
                zeg_kw_push(out, a3, sizeof(struct winsize)); // 8
                break;
            case TIOCGPGRP:
            case TIOCGSID:
            case TIOCMGET:
            case TIOCOUTQ:
            case TIOCGETD:
            case TIOCGSOFTCAR:
            case FIONREAD:
            case FIOGETOWN:
            case SIOCGPGRP:
            case SIOCATMARK:
                zeg_kw_push(out, a3, 4); // int out
                break;
            case FIOCLEX:
            case FIONCLEX:
            case FIOASYNC:
            case FIONBIO:
            case FIOSETOWN:
            case SIOCSPGRP:
            case TIOCSPGRP:
            case TIOCSWINSZ:
            case TIOCSCTTY:
            case TIOCNOTTY:
            case TIOCSETD:
            case TIOCSSOFTCAR:
            case TIOCSTI:
            case TIOCEXCL:
            case TIOCNXCL:
            case TIOCSBRK:
            case TIOCCBRK:
            case TCSETS:
            case TCSETSW:
            case TCSETSF:
            case TCSETA:
            case TCSETAW:
            case TCSETAF:
            case TCSBRK:
            case TCXONC:
            case TCFLSH:
            case TIOCMBIS:
            case TIOCMBIC:
            case TIOCMSET:
            case TIOCCONS:
                // Setters: the kernel only READS the argument.
                break;
            default:
                return false; // UNMODELABLE old-style request
            }
            return true;
        }
        // _IOC-decoded request.
        unsigned dir = (unsigned)((req >> 30) & 3);
        unsigned size = (unsigned)((req >> 16) & 0x3fff);
        if (dir & _IOC_READ) {
            if (!size)
                return false; // malformed _IOC_READ with no size
            zeg_kw_push(out, a3, size);
        }
        return true;
    }

    // --- known no-writes --------------------------------------------------------
    // Virtualized syscalls whose emulator-side writes are cleared at the
    // write site (do_mmap/do_brk/mremap/arch_prctl); the kernel itself
    // wrote nothing into guest memory here.
    case SYS_brk:
    case SYS_arch_prctl:
    case SYS_mmap:
    case SYS_mremap:
    case SYS_mprotect:
    case SYS_munmap:
    // Stubbed above: no kernel call happens at all.
    case SYS_rt_sigaction:
    case SYS_rt_sigprocmask:
    // Stubs that return -ENOSYS without a kernel call (their sret < 0
    // already means no write; listed so the intent is explicit).
    case SYS_execve:
    case SYS_execveat:
    case 57: // fork
    case 58: // vfork
    // Early-return paths (clone/exit never reach the epilogue hook).
    case SYS_clone:
    case SYS_clone3:
    case SYS_exit:
    case SYS_exit_group:
    // fd lifecycle / positioning / metadata: the kernel returns a value or
    // a status and never writes the guest's buffers.
    case SYS_open:
    case SYS_openat:
    case SYS_openat2:
    case SYS_close:
    case SYS_close_range:
    case SYS_lseek:
    case SYS_dup:
    case SYS_dup2:
    case SYS_dup3:
    case SYS_fchdir:
    case SYS_chdir:
    case SYS_umask:
    case SYS_chmod:
    case SYS_fchmod:
    case SYS_fchmodat:
    case SYS_chown:
    case SYS_fchown:
    case SYS_lchown:
    case SYS_fchownat:
    case SYS_truncate:
    case SYS_ftruncate:
    case SYS_link:
    case SYS_linkat:
    case SYS_symlink:
    case SYS_symlinkat:
    case SYS_unlink:
    case SYS_unlinkat:
    case SYS_rename:
    case SYS_renameat:
    case SYS_renameat2:
    case SYS_mkdir:
    case SYS_mkdirat:
    case SYS_rmdir:
    case SYS_mknod:
    case SYS_mknodat:
    case SYS_access:
    case SYS_faccessat:
    case SYS_faccessat2:
    case SYS_utimensat:
    case SYS_futimesat:
    // Writes: the kernel READS the guest buffer.
    case SYS_write:
    case SYS_writev:
    case SYS_pwrite64:
    case SYS_pwritev:
    case SYS_pwritev2:
    // Memory management: no guest writes (fresh mappings are cleared by
    // do_mmap; pkey_mprotect only changes protection).
    case SYS_pkey_mprotect:
    case SYS_mlock:
    case SYS_mlock2:
    case SYS_munlock:
    case SYS_mlockall:
    case SYS_munlockall:
    case SYS_msync:
    case SYS_remap_file_pages:
    case SYS_mbind:
    case SYS_set_mempolicy:
    case SYS_migrate_pages:
    // Zero-copy fd-to-fd movers: data never lands in guest memory.
    case SYS_copy_file_range:
    case SYS_splice:
    case SYS_tee:
    case SYS_vmsplice:
    case SYS_sendfile:
    case SYS_readahead:
    case SYS_fadvise64:
    // Sync / storage metadata.
    case SYS_sync:
    case SYS_fsync:
    case SYS_fdatasync:
    case SYS_sync_file_range:
    case SYS_syncfs:
    // xattr setters / removers: the kernel reads the value.
    case SYS_fsetxattr:
    case SYS_setxattr:
    case SYS_lsetxattr:
    case SYS_fremovexattr:
    case SYS_removexattr:
    case SYS_lremovexattr:
    // Admin / mount / identity: status only.
    case SYS_swapon:
    case SYS_swapoff:
    case SYS_reboot:
    case SYS_pivot_root:
    case SYS_chroot:
    case SYS_sethostname:
    case SYS_setdomainname:
    case SYS_vhangup:
    case SYS_iopl:
    case SYS_ioperm:
    case SYS_setfsuid:
    case SYS_setfsgid:
    case SYS_setsid:
    case SYS_setpgid:
    case SYS_getpgid:
    case SYS_getsid:
    case SYS_setresuid:
    case SYS_setresgid:
    case SYS_setuid:
    case SYS_setreuid:
    case SYS_setregid:
    case SYS_setgid:
    case SYS_setgroups:
    case SYS_capset:
    case SYS_personality:
    case SYS_unshare:
    case SYS_setns:
    case SYS_memfd_create:
    case SYS_fsopen:
    case SYS_fsmount:
    case SYS_fspick:
    case SYS_move_mount:
    case SYS_mount_setattr:
    case SYS_open_tree:
    case SYS_mount:
    case SYS_umount2:
    // Signals: kill-style syscalls write nothing.
    case SYS_kill:
    case SYS_tgkill:
    case SYS_tkill:
    case SYS_pidfd_send_signal:
    case SYS_rt_sigreturn:
    // Thread bookkeeping. rseq: the kernel writes the rseq ABI pages
    // LAZILY at preemption points, not at syscall time -- a known
    // approximation (an rseq area overlapping poisoned memory is not
    // modeled).
    case SYS_set_tid_address:
    case SYS_set_robust_list:
    case SYS_rseq:
    case SYS_gettid:
    // Scheduling / priorities: status only.
    case SYS_sched_yield:
    case SYS_sched_setscheduler:
    case SYS_sched_setparam:
    case SYS_sched_setaffinity:
    case SYS_setpriority:
    case SYS_getpriority:
    case SYS_ioprio_get:
    case SYS_ioprio_set:
    case SYS_setrlimit:
    // Sockets: setup and sends write nothing (accept/getsockopt/recv* are
    // modeled above).
    case SYS_socket:
    case SYS_bind:
    case SYS_listen:
    case SYS_connect:
    case SYS_shutdown:
    case SYS_sendto:
    case SYS_sendmsg:
    case SYS_setsockopt:
    // Eventfd / epoll / inotify / signalfd / timerfd creation: fd only.
    case SYS_eventfd:
    case SYS_eventfd2:
    case SYS_epoll_create1:
    case SYS_epoll_ctl:
    case SYS_inotify_init:
    case SYS_inotify_init1:
    case SYS_inotify_add_watch:
    case SYS_inotify_rm_watch:
    case SYS_signalfd4:
    case SYS_timerfd_create:
    case SYS_fanotify_init:
    // SysV primitives that return ids or statuses (semctl/msgctl/shmctl
    // can write user buffers -- IPC_STAT/GETALL -- so they stay UNKNOWN).
    case SYS_semget:
    case SYS_semop:
    case SYS_semtimedop:
    case SYS_msgget:
    case SYS_shmget:
    case SYS_shmat:
    case SYS_shmdt:
    // pid queries: value only.
    case SYS_getpid:
    case SYS_getppid:
    case SYS_getuid:
    case SYS_geteuid:
    case SYS_getgid:
    case SYS_getegid:
        return true;

    // Everything else is UNMODELABLE: refuse rather than let a kernel
    // overwrite silently launder stale poison.
    default:
        return false;
    }
}

// Guest mmap with guest_maps tracking + MAP_FIXED policy (see above).
template<typename TaintT>
static uint64_t do_mmap(CPU<TaintT>* cpu, uint64_t addr, uint64_t len, uint64_t prot,
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
    // File-backed mappings: record the real path + ELF load bias so the
    // crash-time stack trace can symbolicate shared libraries loaded by the
    // guest ld.so (and dlopen'd objects). Anonymous mappings keep the
    // "guest-mmap" tag; non-ELF or oddly-offset file mappings keep it too
    // (their bias would be a guess).
    std::string fpath;
    uint64_t bias = 0;
    if (!(flags & MAP_ANONYMOUS) &&
        file_mapping_info((int)fd, base, off, &fpath, &bias)) {
        maps_add(emu, base, base + aligned, (int)prot, fpath.c_str(), bias,
                 true);
    } else {
        maps_add(emu, base, base + aligned, (int)prot, "guest-mmap");
    }
    // A fresh mapping (zeroed anonymous pages or kernel-loaded file
    // contents) is not guest-derived data: clear any stale poison across
    // the whole page-aligned span (a MAP_FIXED remap replaces old pages).
    mem_unpoison_range(emu, base, aligned);
    return base;
}

// --- clone emulation -------------------------------------------------------

struct CloneArgs {
    uint64_t flags, pidfd, child_tid, parent_tid, exit_signal, stack,
        stack_size, tls, set_tid, set_tid_size, cgroup;
};

// Extended ChildStart with tid addresses. Templated on the taint type (it
// carries a full child CPU).
template<typename TaintT>
struct ChildStart2 {
    CPU<TaintT> child_cpu;
    uint64_t parent_tid_addr = 0;
    uint64_t child_tid_addr = 0;
    std::mutex mu;
    std::condition_variable cv;
    bool ready = false;
    int child_tid = 0;
};

template<typename TaintT>
static void thread_register_tid(CPU<TaintT>* cpu, int tid) {
    cpu->tid = tid;
    Emulator* emu = cpu->emu;
    std::lock_guard<std::mutex> l(emu->thread_mu);
    emu->thread_tids.push_back((uint64_t)tid);
    emu->thread_cpus.push_back((void*)cpu);
}

// Remove a CPU from the live-thread vectors (no dangling pointers). Both
// vectors are indexed in lockstep; the entries are erased, not just nulled.
template<typename TaintT>
static void thread_unregister_cpu(CPU<TaintT>* cpu) {
    Emulator* emu = cpu->emu;
    std::lock_guard<std::mutex> l(emu->thread_mu);
    for (size_t i = 0; i < emu->thread_cpus.size(); i++) {
        if (emu->thread_cpus[i] == (void*)cpu) {
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
template<typename TaintT>
static void thread_clear_ctid(CPU<TaintT>* cpu) {
    if (!cpu->clear_ctid_addr)
        return;
    *(volatile int*)(uintptr_t)cpu->clear_ctid_addr = 0;
    // Emulator-side write (kernel-equivalent: the kernel would clear this
    // word at thread exit): clear stale poison over the tid word.
    mem_unpoison_range(cpu->emu, cpu->clear_ctid_addr, 4);
    ::syscall(SYS_futex, (void*)(uintptr_t)cpu->clear_ctid_addr, FUTEX_WAKE,
              1, nullptr, nullptr, 0);
    ::syscall(SYS_futex, (void*)(uintptr_t)cpu->clear_ctid_addr,
              FUTEX_WAKE | FUTEX_PRIVATE_FLAG, 1, nullptr, nullptr, 0);
}

template<typename TaintT>
static void* child_thread_main2(void* arg) {
    ChildStart2<TaintT>* st = (ChildStart2<TaintT>*)arg;
    CPU<TaintT>* cpu = new CPU<TaintT>(st->child_cpu);
    uint64_t paddr = st->parent_tid_addr;
    uint64_t caddr = st->child_tid_addr;
    (void)caddr;
    int tid = (int)::syscall(SYS_gettid);
    thread_register_tid(cpu, tid);
    if (paddr) {
        // Validate the parent_tid destination before writing (poison-aware).
        mem_check_store(cpu, paddr, 4);
        *(volatile int*)(uintptr_t)paddr = tid;
        // Emulator-side write (kernel-equivalent fresh tid data): clear
        // stale poison over the tid word so the written value cannot
        // inherit laundered taint.
        mem_unpoison_range(cpu->emu, paddr, 4);
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

template<typename TaintT>
static uint64_t do_clone3(CPU<TaintT>* cpu, uint64_t uaddr, uint64_t usize) {
    if (usize < 88) // sizeof(struct clone_args) canonical
        return (uint64_t)(-(long)EINVAL);
    CloneArgs ca;
    memset(&ca, 0, sizeof(ca));
    mem_load_bytes(cpu, uaddr, (uint8_t*)&ca, usize < sizeof(ca) ? usize : sizeof(ca));
    uint64_t flags = ca.flags;

    ChildStart2<TaintT>* st = new ChildStart2<TaintT>();
    st->child_cpu = *cpu; // copies regs incl. already-advanced RIP
    st->child_cpu.gpr[ZG_RAX].val = 0;
    st->child_cpu.gpr[ZG_RAX].taint.clear();
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
    if (pthread_create(&th, &attr, child_thread_main2<TaintT>, st) != 0) {
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

template<typename TaintT>
static uint64_t do_clone(CPU<TaintT>* cpu, uint64_t flags, uint64_t stack,
                         uint64_t parent_tid, uint64_t tls, uint64_t child_tid) {
    if (!(flags & CLONE_THREAD))
        return (uint64_t)(-(long)ENOSYS); // no fork support in phase 1
    ChildStart2<TaintT>* st = new ChildStart2<TaintT>();
    st->child_cpu = *cpu;
    st->child_cpu.gpr[ZG_RAX].val = 0;
    st->child_cpu.gpr[ZG_RAX].taint.clear();
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
    if (pthread_create(&th, &attr, child_thread_main2<TaintT>, st) != 0) {
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

template<typename TaintT>
void emulate_syscall(CPU<TaintT>* cpu, unsigned insn_len) {
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
                guest_error(cpu, "tainted value passed to syscall", 0,
                            poison_history_of(cpu->gpr[regs[i]].taint));
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
            // Emulator-side write (kernel-equivalent fresh data, not
            // guest-derived): clear stale poison at the destination so the
            // written-back fs_base cannot inherit laundered taint.
            mem_unpoison_range(cpu->emu, a2, 8);
            ret = 0;
        } else if (a1 == ARCH_SET_GS) {
            cpu->gs_base = a2;
            ret = 0;
        } else if (a1 == ARCH_GET_GS) {
            mem_store64(cpu, a2, cpu->gs_base);
            mem_unpoison_range(cpu->emu, a2, 8);
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
        cpu->gpr[ZG_RCX].taint.clear();
        cpu->gpr[ZG_R11].val = cpu->rflags;
        cpu->gpr[ZG_R11].taint.clear();
        ret = do_clone3(cpu, a1, a2);
        cpu->gpr[ZG_RAX].val = ret;
        cpu->gpr[ZG_RAX].taint.clear();
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
        cpu->gpr[ZG_RCX].taint.clear();
        cpu->gpr[ZG_R11].val = cpu->rflags;
        cpu->gpr[ZG_R11].taint.clear();
        ret = do_clone(cpu, a1, a2, a3, a4, a5);
        cpu->gpr[ZG_RAX].val = ret;
        cpu->gpr[ZG_RAX].taint.clear();
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
                // Emulator-side write (kernel-equivalent ctid clear): clear
                // stale poison over the tid word (same as thread_clear_ctid).
                mem_unpoison_range(cpu->emu, ctid, 4);
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
            // Kernel-write modeling for mremap: the bytes
            // [a1, a1+a2) MOVED to [base, base+a2), so their poison moves
            // with the data (erase old + clear destination + reinsert
            // shifted, bits and history trails intact); growth beyond the
            // old size [base+a2, base+a3) is fresh zeroed pages. The
            // poison map is byte-granular, so these use the EXACT byte
            // ranges, not the page-rounded guest_maps bookkeeping above.
            mem_move_poison(cpu->emu, a1, (size_t)a2, base, (size_t)a2);
            if (a3 > a2)
                mem_unpoison_range(cpu->emu, base + a2, (size_t)(a3 - a2));
            if (a3 < a2) {
                // Shrink: the tail beyond the new size is unmapped; the
                // move reinserted its entries at the destination, so drop
                // them (nothing lives there anymore).
                mem_unpoison_range(cpu->emu, base + a3, (size_t)(a2 - a3));
            }
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

    // Kernel-write modeling (problems.txt #2): every buffer the kernel
    // overwrites is replaced with fresh, non-guest-derived data, so stale
    // poison there must be cleared -- a poisoned (secret-derived) value that
    // survives a kernel overwrite would false-positive later branches on it
    // (e.g. fstat's struct stat landing on stack slots that older, deeper
    // frames poisoned with spill taint). Syscalls whose write extents cannot
    // be modeled crash instead of silently laundering poison -- but only
    // when poison is active and the syscall succeeded (a failed syscall
    // writes nothing).
    if (mem_poison_any()) {
        std::vector<ZegKernelWrite> kwrites;
        if (!syscall_kernel_writes(cpu, n, a1, a2, a3, a4, a5, a6,
                                   (int64_t)ret, kwrites)) {
            if ((int64_t)ret >= 0) {
                static thread_local char msg[160];
                snprintf(msg, sizeof(msg),
                         "cannot model kernel-write extents of syscall %s "
                         "(%llu): extend syscall_kernel_writes in "
                         "src/syscall.cpp",
                         syscall_name((long)n), (unsigned long long)n);
                guest_error(cpu, msg);
            }
        } else {
            for (const ZegKernelWrite& w : kwrites)
                mem_unpoison_range(cpu->emu, w.addr, w.size);
        }
    }

    cpu->gpr[ZG_RAX].val = ret;
    cpu->gpr[ZG_RAX].taint.clear();
    // syscall ABI: RCX = return RIP, R11 = RFLAGS.
    cpu->rip += insn_len;
    cpu->gpr[ZG_RCX].val = cpu->rip;
    cpu->gpr[ZG_RCX].taint.clear();
    cpu->gpr[ZG_R11].val = cpu->rflags;
    cpu->gpr[ZG_R11].taint.clear();

    if (cpu->emu->trace_syscall) {
        fprintf(stderr, "[zegarmistrz:tid=%d]  -> %#lx\n", cpu->tid, ret);
        fflush(stderr);
    }
}

// Explicit instantiations: the engine (decode_exec.cpp) dispatches syscalls
// for either taint type.
template void emulate_syscall<Taint>(CPU<Taint>* cpu, unsigned insn_len);
template void emulate_syscall<HistoryTaint>(CPU<HistoryTaint>* cpu,
                                            unsigned insn_len);
template void emulate_syscall<DeepHistoryTaint>(CPU<DeepHistoryTaint>* cpu,
                                                unsigned insn_len);
