#!/usr/bin/env python3
"""Generates pizlonated C shims for cosmopolitan libc's sys_* system call thunks.

Cosmo implements every Linux system call as a tiny assembly thunk (".scall")
in libc/sysv/calls/*.S.  Fil-C compiled code can not call those thunks:
- they are raw machine code (not pizlonated), and
- pizlonated code references extern functions by their `pizlonated_<name>`
  symbol, which the .S thunks do not provide.

So build_usercosmo.sh uses this script to generate libc/sysv/calls shims as C
functions that forward to libpizlo's zsys_* API (which itself funnels into the
yolo cosmo libc below libpizlo).

Return-value convention (verified): cosmo's sys_* thunks return -1 with errno
set on failure (the _sysret helper inside the thunk does the -errno → -1/errno
conversion), and libpizlo's zsys_* functions use the same convention (they
deliver errno through the Fil-C errno handler registered at startup).  So the
happy path for a shim is a plain argument-for-argument forward.

The signatures are extracted from cosmo's own headers by running the Fil-C
clang's AST dump (-ast-dump=json), which guarantees that every generated
definition matches the prototypes that cosmo's C wrappers compile against.
The generated file #includes those same headers so that any mismatch is a
compile error rather than a silent runtime trap.
"""

import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
COSMO = os.path.join(ROOT, "projects", "usercosmo")
FILC_CLANG = os.path.join(ROOT, "build", "bin", "clang")
STDFIL = os.path.join(ROOT, "pizfix", "stdfil-include")
PZ_SYSCALLS = os.path.join(ROOT, "filc", "include", "pizlonated_syscalls.h")

# Headers which are always included when collecting signatures.
BASE_HEADERS = [
    "libc/calls/calls.h",
    "libc/calls/syscall-sysv.internal.h",
    "libc/calls/syscall_support-sysv.internal.h",
    "libc/calls/sched-sysv.internal.h",
    "libc/calls/sigtimedwait.internal.h",
    "libc/calls/struct/timespec.internal.h",
    "libc/calls/struct/iovec.internal.h",
    "libc/calls/struct/timeval.internal.h",
    "libc/calls/struct/utsname.h",
    "libc/calls/struct/utsname-linux.internal.h",
    "libc/calls/struct/sigset.internal.h",
    "libc/calls/struct/statfs.internal.h",
    "libc/calls/struct/rusage.internal.h",
    "libc/calls/struct/rlimit.internal.h",
    "libc/calls/struct/itimerval.internal.h",
    "libc/calls/struct/sysinfo.internal.h",
    "libc/calls/struct/sigval.internal.h",
    "libc/calls/struct/siginfo.internal.h",
    "libc/calls/groups.internal.h",
    "libc/sock/internal.h",
    "libc/sock/struct/pollfd.internal.h",
    "libc/sock/struct/msghdr.internal.h",
    "libc/utime.h",
    "libc/thread/xnu.internal.h",
    "libc/proc/proc.h",
]

# Hand-written mappings for thunks whose cosmo signature does not match the
# zsys_* prototype one-to-one, or which have no zsys_* but a workable route.
# Each entry: name -> (code, uses_va) where {args} is substituted with the
# comma-separated argument names.
HAND_MAP = {
    # mmap takes (addr, size, prot, flags, fd, off, off) in cosmo; the two
    # trailing offset slots are for BSDs, on Linux both are the same value.
    "__sys_mmap": ("return zsys_mmap({a0}, {a1}, {a2}, {a3}, {a4}, {a5});", False),
    # sigaction takes two extra legacy args on top of the zsys_sigaction trio.
    "sys_sigaction": ("return zsys_sigaction({a0}, {a1}, {a2});", False),
    # openat_nc is just openat (the _nc suffix means nocancel, xnu-only).
    "__sys_openat_nc": ("return zsys_openat({a0}, {a1}, {a2}, {a3});", False),
    # cosmo's __sys_accept(fd, addr, addrlen, flags): the Linux kernel's
    # accept(2) only reads three arguments (accept4 has its own thunk), so
    # drop the flags.
    "__sys_accept": ("return zsys_accept({a0}, {a1}, {a2});", False),
    # cosmo's __sys_gettid(i64 *tid) writes the tid out as well as returning
    # it (a multi-OS convention); libpizlo only has the returning kind.
    "__sys_gettid": ("*({a0}) = zsys_gettid();\n  return zsys_gettid();", False),
    # getpid returns {ax, dx} in cosmo (dx only matters on XNU).
    "sys_getpid": ("axdx_t r;\n  r.ax = zsys_getpid();\n  r.dx = 0;\n  return r;", False),
    # cosmo's sys_uname() thunk fills the Linux kernel shape (struct
    # utsname_linux, 6x65 bytes), while zsys_uname() fills cosmo's public
    # struct utsname (6xSYS_NMLN).  Go through a full-size temp and copy the
    # kernel-shaped fields out, so the caller's (smaller) buffer is only
    # written within its real bounds.
    "sys_uname": (
        "struct utsname tmp;\n"
        "  int rc = zsys_uname(&tmp);\n"
        "  if (rc) return rc;\n"
        "  struct utsname_linux *out = (struct utsname_linux *){a0};\n"
        "  __builtin_memcpy(out->sysname, tmp.sysname, sizeof(out->sysname));\n"
        "  __builtin_memcpy(out->nodename, tmp.nodename, sizeof(out->nodename));\n"
        "  __builtin_memcpy(out->release, tmp.release, sizeof(out->release));\n"
        "  __builtin_memcpy(out->version, tmp.version, sizeof(out->version));\n"
        "  __builtin_memcpy(out->machine, tmp.machine, sizeof(out->machine));\n"
        "  __builtin_memcpy(out->domainname, tmp.domainname, sizeof(out->domainname));\n"
        "  return rc;", False),
    # rt_sigprocmask's extra trailing argument is the sigset size, which the
    # zsys_sigprocmask wrapper doesn't take.
    "__sys_sigprocmask": ("return zsys_sigprocmask({a0}, {a1}, {a2});", False),
    # pread/pwrite families take (fd, buf, count, off, off); the double offset
    # slot is for BSDs.
    "sys_pread": ("return zsys_pread({a0}, {a1}, {a2}, {a3});", False),
    "sys_preadv": ("return zsys_preadv({a0}, {a1}, {a2}, {a3});", False),
    "sys_pwrite": ("return zsys_pwrite({a0}, {a1}, {a2}, {a3});", False),
    "sys_pwritev": ("return zsys_pwritev({a0}, {a1}, {a2}, {a3});", False),
    # sigsuspend(mask, size): the size is a cosmo-ism, drop it.
    "sys_sigsuspend": ("return zsys_sigsuspend({a0});", False),
    # sync() returns void in libpizlo.
    "sys_sync": ("zsys_sync();\n  return 0;", False),
    # tkill is rejected by libpizlo; cosmo's pthread_kill goes via
    # zthread_kill instead, so trap here.
    "sys_tkill": ('zerrorf("usercosmo: sys_tkill is not supported under Fil-C");\n  return -1;', False),
    # truncate(path, length, length) (double slot for BSDs).
    "sys_truncate": ("return zsys_truncate({a0}, {a1});", False),
    # lseek(fd, offset, whence, 0) on Linux (4th arg is for NetBSD).
    "sys_lseek": ("return zsys_lseek({a0}, {a1}, {a2});", False),
    # dup2(fd, fd2, 0) (3rd arg unused on Linux; dup3 has its own thunk).
    "sys_dup2": ("return zsys_dup2({a0}, {a1});", False),
    # ftruncate(fd, length, length) (double slot for BSDs).
    "sys_ftruncate": ("return zsys_ftruncate({a0}, {a1});", False),
    # getcwd(buf, size) returns the string length on Linux, but zsys_getcwd
    # returns the buffer pointer (or NULL w/ errno).
    "sys_getcwd": ("char *rc = zsys_getcwd({a0}, {a1});\n  if (!rc) return -1;\n  return strlen(rc);", False),
    # kill(pid, sig, 0) (3rd arg unused on Linux).
    "sys_kill": ("return zsys_kill({a0}, {a1});", False),
    # arch_prctl takes the user pointer as an integer in cosmo's prototype,
    # which destroys the Fil-C capability: don't support it (set_tls() is
    # neutralized under Fil-C anyway; nothing should call this).
    "sys_arch_prctl": ('zerrorf("usercosmo: sys_arch_prctl is not supported under Fil-C");\n  return -1;', False),
    # mremap(..., new_address): the trailing integer is only meaningful with
    # MREMAP_FIXED; forge a null pointer otherwise.
    "sys_mremap": ("return zsys_mremap({a0}, {a1}, {a2}, {a3}, ({a4}) ? (void *)(uintptr_t)({a4}) : 0);", False),
    # ppoll(fds, nfds, timeout, mask, sigsetsize): drop the size.
    "sys_ppoll": ("return zsys_ppoll({a0}, {a1}, {a2}, {a3});", False),
    # rt_sigpending(set, sigsetsize): drop the size.
    "sys_sigpending": ("return zsys_sigpending({a0});", False),
    # rt_sigtimedwait(set, info, timeout, sigsetsize): drop the size.
    "sys_sigtimedwait": ("return zsys_sigtimedwait({a0}, {a1}, {a2});", False),
    # sigqueue solaris-style; Linux uses rt_sigqueueinfo which has no zsys.
    # libpizlo does have zsys_sigqueue(pid, sig, void *value) (it funnels into
    # the host's sigqueue(3) with the sigval pointer value), so route both
    # thunks through it.  sigval is a union: the int/pointer variants share
    # storage, so carrying the value through sival_ptr preserves sival_int.
    "sys_sigqueue": ("return zsys_sigqueue({a0}, {a1}, {a2}.sival_ptr);", False),
    # rt_sigqueueinfo: cosmo's sigqueue(3) fills a siginfo_t with SI_QUEUE and
    # calls this.  The kernel rejects si_code >= 0, so we can not simply hand
    # the struct over; deconstruct it into the zsys_sigqueue(pid, sig, value)
    # shape instead.  The kernel fills in si_pid/si_uid/si_code itself for
    # queued signals, so the only fields that matter are the signal number and
    # the payload.
    "sys_sigqueueinfo": (
        "return zsys_sigqueue({a0}, ((const siginfo_t *){a1})->si_signo,\n"
        "                  ((const siginfo_t *){a1})->si_value.sival_ptr);", False),
    # closefrom(from) == close_range(from, ~0, 0)
    "sys_closefrom": ("return zsys_close_range({a0}, -1u, 0);", False),
    # faccessat2 is faccessat with a flags argument.
    "sys_faccessat2": ("return zsys_faccessat({a0}, {a1}, {a2}, {a3});", False),
    # fchmodat2 (chmodxat2) is fchmodat with a flags argument; libpizlo's
    # zsys_fchmodat() takes the flag directly.
    "sys_fchmodat2": ("return zsys_fchmodat({a0}, {a1}, {a2}, {a3});", False),
    # fadvise(fd, off_lo, off_hi, advice) -> posix_fadvise(fd, off, len, advice)
    # cosmo passes the offset in two slots on BSD; on Linux off_hi is 0.
    # (See libc/calls/posix_fadvise.c: it calls sys_fadvise(fd, off, len, adv)?? no:
    #  it uses the netbsd/freebsd aliases; the Linux path uses fadvise64 via
    #  posix_fadvise(); keep this shim for the declared prototype only.)
    "sys_fadvise": ("return zsys_posix_fadvise({a0}, {a1}, {a2}, {a3});", False),
    # fork/pipe return both %ax and %dx (axdx_t); zsys only has the int part.
    "__sys_fork": ("axdx_t r;\n  r.ax = zsys_fork_impl();\n  r.dx = 0;\n  return r;", False),
    "__sys_pipe": ("axdx_t r;\n  r.ax = zsys_pipe({a0});\n  r.dx = 0;\n  return r;", False),
    # exit: zsys_exit_hard is the raw exit_group.
    "sys_exit": ("zsys_exit_hard({a0});", False),
    # sched_yield: zsys returns void.
    "sys_sched_yield": ("zsys_sched_yield();\n  return 0;", False),
}

# Variadic thunks.  These must forward the WHOLE argument area (named args +
# varargs) to the zsys_* function: the zsys_* side inspects the argument
# cursor on its own (zsys_fcntl reads the extra argument only for the cmds
# that have one, zsys_ioctl likewise for the ioctls that take an argument).
# Forwarding only the named parameters silently drops F_SETFL's flags /
# ioctl's argument, which breaks every caller that passes one.  zargs()
# snapshots the incoming arguments of the shim and zcall() re-calls the
# zsys_* function with that exact snapshot.
VARIADIC_MAP = {
    "sys_ioctl": "return *(int *)zcall(zsys_ioctl, zargs());",
    "sys_ioctl_cp": "return *(int *)zcall(zsys_ioctl, zargs());",
    "__sys_fcntl": "return *(int *)zcall(zsys_fcntl, zargs());",
    "__sys_fcntl_cp": "return *(int *)zcall(zsys_fcntl, zargs());",
    "sys_openat": "return zsys_openat({a0}, {a1}, {a2}, {a3});",
    "sys_openat_nc": "return zsys_openat({a0}, {a1}, {a2}, {a3});",
    "sys_semctl": "return zsys_semctl({a0}, {a1}, {a2});",
}

# Linux syscall numbers supported by libpizlo's variadic zsys_syscall().
ZSYS_SYSCALL_SUPPORTED = {
    202,  # futex
    217,  # getdents64
    186,  # gettid
    318,  # getrandom
    39,   # getpid
    164,  # settimeofday
    3,    # close
    332,  # statx
    326,  # copy_file_range
    316,  # renameat2
    434,  # pidfd_open
    117,  # setreuid
    118,  # setregid
    319,  # memfd_create
    1,    # write
    437,  # openat2
    203,  # sched_setaffinity
    204,  # sched_getaffinity
}

# Thunks whose zsys_* forwarder only works when the yolo libc below libpizlo
# exposes the underlying syscall.  Cosmo (unlike musl and glibc) does not
# expose these system calls at all - no public C API, and the raw .scall
# thunks stay unpizlonated and uncalled - so libpizlo's cosmo build compiles
# these forwarders as filc_internal_panic.  The pizlonated user libc must not
# expose them either, so their shims trap with a clear error instead of
# forwarding into a panicking forwarder.  (These are all things cosmopolitan
# libc would not normally let a program do: epoll/inotify/eventfd/timerfd/
# signalfd, xattr, SysV IPC, swap, port I/O, personality, klog, quota,
# adjtimex, mount umount2, fsuid-independent calls, fallocate/tee/vmsplice/
# remap_file_pages, file handles, memfd_create/setns/unshare, getdents,
# waitid, mremap, mlockall, mkfifo/mknodat, sendmmsg/recvmmsg, acct/vhangup/
# sethostname/setdomainname, futex PI/requeue, and the assorted modern Linux
# adds: capget/keyctl/pidfd/fanotify/fsopen/mempolicy/modules/perf_event_open/
# statx/openat2/renameat2/timer_*.)
COSMO_TRAP = {
    "sys_acct",
    "sys_add_key",
    "sys_adjtime",
    "sys_adjtimex",
    "sys_clock_adjtime",
    "sys_capget",
    "sys_capset",
    "sys_delete_module",
    "sys_epoll_create",
    "sys_epoll_create1",
    "sys_epoll_ctl",
    "sys_epoll_wait",
    "sys_epoll_pwait",
    "sys_epoll_pwait2",
    "sys_eventfd",
    "sys_eventfd2",
    "sys_fallocate",
    "sys_fanotify_init",
    "sys_fanotify_mark",
    "sys_fgetxattr",
    "sys_finit_module",
    "sys_flistxattr",
    "sys_fremovexattr",
    "sys_fsetxattr",
    "sys_fsconfig",
    "sys_fsmount",
    "sys_fsopen",
    "sys_fspick",
    "sys_get_mempolicy",
    "sys_getxattr",
    "sys_inotify_add_watch",
    "sys_inotify_init",
    "sys_inotify_init1",
    "sys_inotify_rm_watch",
    "sys_init_module",
    "sys_ioperm",
    "sys_iopl",
    "sys_keyctl",
    "sys_listxattr",
    "sys_llistxattr",
    "sys_lgetxattr",
    "sys_lremovexattr",
    "sys_lsetxattr",
    "sys_memfd_create",
    "sys_mkfifo",
    "sys_mknodat",
    "sys_mlockall",
    "sys_mremap",
    "sys_msgctl",
    "sys_msgget",
    "sys_msgrcv",
    "sys_msgsnd",
    "sys_munlockall",
    "sys_name_to_handle_at",
    "sys_open_by_handle_at",
    "sys_openat2",
    "sys_open_tree",
    "sys_perf_event_open",
    "sys_personality",
    "sys_pidfd_getfd",
    "sys_pidfd_open",
    "sys_pidfd_send_signal",
    "sys_pkey_alloc",
    "sys_pkey_free",
    "sys_process_madvise",
    "sys_process_mrelease",
    "sys_quotactl",
    "sys_recvmmsg",
    "sys_remap_file_pages",
    "sys_removexattr",
    "sys_renameat2",
    "sys_request_key",
    "sys_sendmmsg",
    "sys_set_mempolicy",
    "sys_setdomainname",
    "sys_sethostname",
    "sys_setns",
    "sys_setxattr",
    "sys_shmat",
    "sys_shmctl",
    "sys_shmdt",
    "sys_shmget",
    "sys_mkfifoat",
    "sys_posix_fallocate",
    "sys_signalfd",
    "sys_signalfd4",
    "sys_statx",
    "sys_swapon",
    "sys_swapoff",
    "sys_syslog",
    "sys_tee",
    "sys_timer_create",
    "sys_timer_delete",
    "sys_timer_gettime",
    "sys_timer_getoverrun",
    "sys_timer_settime",
    "sys_timerfd_create",
    "sys_timerfd_gettime",
    "sys_timerfd_settime",
    "sys_umount2",
    "sys_unshare",
    "sys_vhangup",
    "sys_vmsplice",
    "sys_waitid",
}


# Declarations that cosmo puts inside a .c file instead of a header, so the
# ast-dump scan misses them.  Each entry: thunk name -> qualType string.
SYNTHETIC_DECLS = {
    # libc/calls/fchmodat.c declares sys_fchmodat2() itself (the chmodxat2
    # syscall, used whenever fchmodat() gets a flags argument).
    "sys_fchmodat2": "int32_t (int32_t, const char *, uint32_t, int32_t)",
    # libc/calls/landlock_*.c declare their sys_* thunks in the .c files too.
    # Without these, unveil() pulled landlock_*.o whose sys_landlock_*
    # references had no shim, and every program calling unveil() failed to
    # link.  libpizlo has the zsys_landlock_* forwarders (with capability
    # checks on the attr/rule objects), so these are plain identity forwards.
    # The object types are spelled `const void *` because the shim TU does
    # not include landlock.h; the ABI (a single 8-byte user pointer) is
    # identical, and libpizlo checks the pointed-to memory with the size the
    # caller passes.
    "sys_landlock_create_ruleset": "int32_t (const void *, uint64_t, uint32_t)",
    "sys_landlock_add_rule": "int32_t (int32_t, int32_t, const void *, uint32_t)",
    "sys_landlock_restrict_self": "int32_t (int32_t, uint32_t)",
}


def find_thunks():
    """Returns [(name, linux_nr)] for every .scall in libc/sysv/calls/*.S."""
    thunks = []
    directory = os.path.join(COSMO, "libc", "sysv", "calls")
    for fn in sorted(os.listdir(directory)):
        if not fn.endswith(".S"):
            continue
        with open(os.path.join(directory, fn)) as f:
            for line in f:
                m = re.match(r"\.scall\s+([A-Za-z_][A-Za-z0-9_]*),(0x[0-9a-fA-F]+|\d+),", line)
                if m:
                    name = m.group(1)
                    amd = int(m.group(2), 0)
                    nr = amd & 0xFFF
                    if nr != 0xFFF:
                        nr &= ~0x800  # strip the cancellable bit
                    thunks.append((name, nr))
                    break  # one thunk per file
    return thunks


def zsys_names():
    with open(PZ_SYSCALLS) as f:
        return set(re.findall(r"\b(zsys_\w+)\s*\(", f.read()))


def forwarder_conditions():
    """Parses libpas/src/libpas/generate_pizlonated_forwarders.rb, which is
    what decides which zsys_* forwarders the runtime actually implements, and
    returns {name: condition}.  Some forwarders only exist on one architecture
    (e.g. zsys_iopl/zsys_arch_prctl are x86-only, zmath_getfpsr is aarch64
    only), and shims that forward to a forwarder the runtime does not define
    would fail to link with an undefined pizlonated_<name>."""
    conds = {}
    path = os.path.join(ROOT, "libpas", "src", "libpas",
                        "generate_pizlonated_forwarders.rb")
    pat = re.compile(r'^addSig\s+(?:"([^"]*)"|nil)\s*,\s*"[^"]*"\s*,'
                     r'\s*"([A-Za-z_0-9]+)"')
    with open(path) as f:
        for line in f:
            m = pat.match(line.strip())
            if m:
                conds[m.group(2)] = m.group(1)
    return conds


def cond_holds(cond, arch):
    """Evaluates the (very small) condition language used by addSig."""
    if not cond:
        return True
    if "defined(__x86_64__)" in cond:
        return arch == "x86_64"
    if "defined(__aarch64__)" in cond:
        return arch == "aarch64"
    # Unknown condition: keep the shim; the x86_64 build behaved this way
    # before cross builds existed.
    return True


def zname_of(thunk):
    if thunk.startswith("__sys_"):
        rest = thunk[6:]
    elif thunk.startswith("sys_"):
        rest = thunk[4:]
    else:
        rest = thunk
    return "zsys_" + rest


def collect_decl_headers(thunk_names):
    """Finds every header under libc/ that declares one of the thunk names."""
    headers = set(BASE_HEADERS)
    for dirpath, _, files in os.walk(os.path.join(COSMO, "libc")):
        for fn in files:
            if not fn.endswith(".h"):
                continue
            path = os.path.join(dirpath, fn)
            try:
                with open(path) as f:
                    text = f.read()
            except OSError:
                continue
            for name in thunk_names:
                if re.search(r"[^A-Za-z0-9_]%s\s*\(" % re.escape(name), text):
                    rel = os.path.relpath(path, COSMO)
                    headers.add(rel)
                    break
    return sorted(headers)


def run_ast_dump(headers):
    sigs_c = os.path.join("/tmp", "usercosmo_sigs.c")
    with open(sigs_c, "w") as f:
        for h in headers:
            f.write('#include "%s"\n' % h)
        f.write("int __usercosmo_shims_anchor;\n")
    cmd = [
        FILC_CLANG, "-fsyntax-only", "-Xclang", "-ast-dump=json", sigs_c,
        "-nostdinc",
        "-isystem", STDFIL,
        "-isystem", COSMO,
        "-isystem", os.path.join(COSMO, "libc", "isystem"),
        "-include", os.path.join(COSMO, "libc", "integral", "normalize.inc"),
        "-D_COSMO_SOURCE", "-std=gnu23", "-w",
    ]
    # Cross builds (FILCARCH from filc.mk) must parse the headers with the
    # target's predefined macros, since some cosmo declarations are
    # architecture-conditional.
    filcarch = os.environ.get("FILCARCH", "")
    if filcarch and filcarch != os.uname().machine:
        cmd += ["--target=%s-linux-gnu" % filcarch]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        sys.stderr.write(result.stderr[-4000:])
        raise RuntimeError("ast-dump failed")
    return json.loads(result.stdout)


def extract_decls(ast, names):
    found = {}

    def walk(node):
        if isinstance(node, dict):
            nm = node.get("name", "")
            if node.get("kind") == "FunctionDecl" and nm in names and nm not in found:
                found[nm] = node.get("type", {}).get("qualType", None)
            for v in node.values():
                walk(v)
        elif isinstance(node, list):
            for x in node:
                walk(x)

    walk(ast)
    return found


def split_type(qual):
    """'int32_t (int32_t, const char *, ...)' -> ('int32_t', ['int32_t', 'const char *'], True)"""
    depth = 0
    for i, ch in enumerate(qual):
        if ch == "(":
            depth += 1
            if depth == 1:
                open_at = i
        elif ch == ")":
            depth -= 1
            if depth == 0:
                close_at = i
                break
    ret = qual[:open_at].strip()
    params = qual[open_at + 1:close_at].strip()
    variadic = params.endswith("...")
    if variadic:
        params = params[:-3].strip().rstrip(",").strip()
    if params in ("", "void"):
        args = []
    else:
        args = [p.strip() for p in params.split(",")]
    return ret, args, variadic


def defparams(args, named):
    """Renders the parameter list of a definition.  C allows unnamed
    parameters in definitions, which avoids the function-pointer naming
    problem entirely; variadic shims need the last named parameter for
    va_start(), and their params are always simple types."""
    if not args:
        return "void"
    out = []
    for i, a in enumerate(args):
        if named and "(" not in a and a != "void":
            out.append("%s a%d" % (a, i))
        else:
            out.append(a)
    return ", ".join(out)


def gen_thunk(name, qual, zsys, out, stats, conds=None, arch="x86_64"):
    ret, args, variadic = split_type(qual)
    names = ["a%d" % i for i in range(len(args))]
    is_void = ret == "void"
    is_ptr = "*" in ret

    def ret_default():
        if is_void:
            return ""
        return "return %s;" % ("0" if is_ptr else "-1")

    if name in COSMO_TRAP:
        # The syscall isn't exposed by the cosmo flavor at all (the yolo libc
        # below libpizlo has no API for it and libpizlo panics for it), so
        # trap instead of forwarding, exactly like the thunks that have no
        # zsys_* equivalent at all.  See the COSMO_TRAP comment above.
        body = '  zerrorf("usercosmo: %s is not supported under Fil-C (not exposed by cosmo)");\n  %s' % (name, ret_default())
        out.append("%s %s(%s) {\n%s\n}\n" % (ret, name, defparams(args, True), body))
        stats["trap"] += 1
        return

    if name in HAND_MAP:
        code, _ = HAND_MAP[name]
        sub = {("a%d" % i): names[i] for i in range(len(names))}
        body = "  " + code.format(**sub).replace("\n", "\n  ")
        out.append("%s %s(%s) {\n%s\n}\n" % (ret, name, defparams(args, True), body))
        stats["hand"] += 1
        return

    if variadic:
        if name in VARIADIC_MAP:
            sub = {("a%d" % i): names[i] for i in range(len(names))}
            body = VARIADIC_MAP[name].format(**sub)
            out.append("%s %s(%s) {\n  %s\n}\n" % (ret, name, defparams(args, True) + ", ...", body))
            stats["variadic"] += 1
            return
        body = '  zerrorf("usercosmo: %s is not supported under Fil-C");\n  %s' % (name, ret_default())
        out.append("%s %s(%s) {\n%s\n}\n" % (ret, name, defparams(args, True) + ", ...", body))
        stats["trap"] += 1
        return

    zname = zname_of(name)
    if zname in zsys and (conds is None or
                          cond_holds(conds.get(zname), arch)):
        sub = {("a%d" % i): names[i] for i in range(len(names))}
        body = "  return %s(%s);" % (zname, ", ".join(names))
        out.append("%s %s(%s) {\n%s\n}\n" % (ret, name, defparams(args, True), body))
        stats["direct"] += 1
        return

    body = '  zerrorf("usercosmo: %s is not supported under Fil-C (no zsys equivalent)");\n  %s' % (name, ret_default())
    out.append("%s %s(%s) {\n%s\n}\n" % (ret, name, defparams(args, False), body))
    stats["trap"] += 1


def find_consts():
    """Returns [(name, linux_value)] for every .syscon in libc/sysv/consts/*.S.

    cosmo's syscon constants are 8-byte objects (.quad) initialized at boot
    with the per-OS value; with SUPPORT_VECTOR=1 the Linux column is the
    whole story.  Defining them as `const long` in C matches both the 8-byte
    object size and the `extern const int` / `extern int` declarations in
    cosmo's const headers (readers only ever read)."""
    consts = []
    directory = os.path.join(COSMO, "libc", "sysv", "consts")
    for fn in sorted(os.listdir(directory)):
        if not fn.endswith(".S"):
            continue
        with open(os.path.join(directory, fn)) as f:
            for line in f:
                line = line.strip()
                if not line.startswith(".syscon"):
                    continue
                parts = [p.strip() for p in line[len(".syscon"):].split(",")]
                if len(parts) < 4:
                    continue
                name = parts[1]
                linux = parts[2]
                consts.append((name, linux))
    return consts


def gen_errfuns(path):
    """Generates C implementations of cosmo's tiny errno-returning helpers
    (ebadf(), einval(), ...), which are normally push/jmp assembly thunks
    around __errfun() (libc/sysv/errfuns/*.S)."""
    directory = os.path.join(COSMO, "libc", "sysv", "errfuns")
    lines = []
    count = 0
    # gather names that already have a C definition somewhere in libc
    have_c = set()
    for dirpath, _, files in os.walk(os.path.join(COSMO, "libc")):
        for fn in files:
            if not fn.endswith(".c"):
                continue
            try:
                text = open(os.path.join(dirpath, fn)).read()
            except OSError:
                continue
            for m in re.finditer(r"^\w[\w ]*?\**\b(e[a-z0-9]+)\s*\(\s*void\s*\)", text, re.M):
                have_c.add(m.group(1))
    for fn in sorted(os.listdir(directory)):
        if not fn.endswith(".S"):
            continue
        name = fn[:-2]
        if name in have_c:
            continue
        val = None
        with open(os.path.join(directory, fn)) as f:
            for line in f:
                m = re.search(r"push\s+\$([A-Za-z0-9_]+)", line)
                if m:
                    val = m.group(1)
                    break
        if val is None:
            continue
        lines.append("intptr_t %s(void) { return __errfun(%s); }\n" % (name, val))
        count += 1
    with open(path, "w") as f:
        f.write("/* Generated by projects/usercosmo/filc/gen_shims.py -- do not edit.\n"
                " *\n"
                " * C replacements for cosmo's errno-returning helper thunks\n"
                " * (libc/sysv/errfuns/*.S: \"push $E; pop %rdi; jmp __errfun\").\n"
                " */\n")
        f.write('#include "libc/errno.h"\n'
                '#include "libc/sysv/errfuns.h"\n'
                'long __errfun(int);\n\n')
        for l in lines:
            f.write(l)
    return count


def gen_consts(path):
    consts = find_consts()
    seen = set()
    with open(path, "w") as f:
        f.write("/* Generated by projects/usercosmo/filc/gen_shims.py -- do not edit.\n"
                " *\n"
                " * C definitions for cosmo's .syscon constants (FUTEX_*, MAP_*,\n"
                " * CLOCK_*, __NR_*, ...), which are normally 8-byte objects defined\n"
                " * and initialized by assembly (libc/sysv/consts/*.S).  Fil-C compiled\n"
                " * code references them as pizlonated_<name> symbols, so provide real\n"
                " * C objects initialized with the Linux values (SUPPORT_VECTOR=1).\n"
                " */\n")
        for name, value in consts:
            if name in seen:
                continue
            seen.add(name)
            f.write("const long %s = %s;\n" % (name, value))
    return len(seen)


def main():
    out_path = sys.argv[1]
    consts_path = sys.argv[2] if len(sys.argv) > 2 else None
    thunks = find_thunks()
    thunk_names = set(t[0] for t in thunks)
    zsys = zsys_names()
    conds = forwarder_conditions()
    filcarch = os.environ.get("FILCARCH", "") or os.uname().machine
    headers = collect_decl_headers(thunk_names)
    ast = run_ast_dump(headers)
    decls = extract_decls(ast, thunk_names)

    stats = {"direct": 0, "hand": 0, "variadic": 0, "trap": 0, "skipped": 0}
    chunks = []
    for name, nr in thunks:
        if name not in decls and name in SYNTHETIC_DECLS:
            decls[name] = SYNTHETIC_DECLS[name]
        if name not in decls:
            # No declaration anywhere in cosmo's headers: cosmo builds with
            # -Wall -Werror so nothing can be calling this thunk implicitly.
            # Leave it out; the linker will complain if that was wrong.
            stats["skipped"] += 1
            continue
        gen_thunk(name, decls[name], zsys, chunks, stats, conds, filcarch)

    with open(out_path, "w") as f:
        f.write("/* Generated by projects/usercosmo/filc/gen_shims.py -- do not edit.\n"
                " *\n"
                " * C shims replacing cosmo's .S system call thunks for the Fil-C build.\n"
                " * Every sys_* thunk returns -1 with errno set on failure (cosmo\n"
                " * convention, see libc/sysv/sysret.c) which is exactly the zsys_*\n"
                " * convention, so the common case is a plain forward.\n"
                " */\n")
        f.write("#include <stdfil.h>\n")
        f.write("#include <pizlonated_syscalls.h>\n")
        f.write("#include <pizlonated_runtime.h>\n")
        for h in headers:
            f.write('#include "%s"\n' % h)
        f.write("\n")
        for chunk in chunks:
            f.write(chunk)
            f.write("\n")

    nconsts = gen_consts(consts_path) if consts_path else 0
    nerr = gen_errfuns(os.path.join(os.path.dirname(consts_path), "errfuns.c")) if consts_path else 0
    print("gen_shims: %d thunks; declared=%d skipped(no decl)=%d direct=%d hand=%d variadic=%d trap=%d; %d consts; %d errfuns"
          % (len(thunks), len(decls), stats["skipped"], stats["direct"], stats["hand"],
             stats["variadic"], stats["trap"], nconsts, nerr))


if __name__ == "__main__":
    main()
