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

// zegarmistrz CLI: ./zegarmistrz [-v|--trace|--trace-syscalls] <prog> [args...]

#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "cpu.h"
#include "emulator.h"
#include <mutex>
#include <signal.h>

static Emulator* g_sig_emu = nullptr;

// Per-thread dump for the SIGUSR1 handler. thread_cpus is type-erased
// (CPU<Taint>, CPU<HistoryTaint>, or CPU<DeepHistoryTaint> depending on
// history_frames), so the per-thread print is templated on the taint type
// and dispatched below.
template<typename TaintT>
static void sigusr1_dump_cpus(Emulator* emu,
                              const std::vector<void*>& snapshot) {
    (void)emu;
    char buf[256];
    for (size_t t = 0; t < snapshot.size(); t++) {
        CPU<TaintT>* cpu = (CPU<TaintT>*)snapshot[t];
        if (!cpu)
            continue;
        int n = snprintf(buf, sizeof(buf),
                         "[zegarmistrz] thread %zu tid=%d rip=%#lx rsp=%#lx\n",
                         t, cpu->tid, cpu->rip, cpu->gpr[6 - 2].val);
        (void)n;
        // NOTE: uses fprintf (not strictly async-safe, debug-only).
        fprintf(stderr, "[zegarmistrz] thread %zu tid=%d rip=%#lx rsp=%#lx\n",
                t, cpu->tid, cpu->rip, cpu->gpr[4].val);
        for (int i = 0; i < 40; i++) {
            uint64_t addr = cpu->gpr[4].val + i * 8;
            uint64_t v = 0;
            memcpy(&v, (void*)(uintptr_t)addr, 8);
            fprintf(stderr, "    sp+%d: %#lx\n", i * 8, v);
        }
        fflush(stderr);
    }
}

// SIGUSR1: dump guest backtraces (async-signal-safe-ish, debug only).
static void sigusr1_handler(int) {
    Emulator* emu = g_sig_emu;
    if (!emu)
        return;
    {
        FILE* mf = fopen("/proc/self/maps", "r");
        if (mf) {
            char line[512];
            while (fgets(line, sizeof(line), mf)) {
                if (strstr(line, "pizfix") || strstr(line, "hello"))
                    fprintf(stderr, "[zegarmistrz] map %s", line);
            }
            fclose(mf);
            fflush(stderr);
        }
    }
    // Copy the live-thread list under lock; entries are erased (never left
    // dangling) on thread exit, but the handler still skips nulls.
    std::vector<void*> snapshot;
    {
        std::lock_guard<std::mutex> l(emu->thread_mu);
        snapshot = emu->thread_cpus;
    }
    if (emu->history_frames > 1)
        sigusr1_dump_cpus<DeepHistoryTaint>(emu, snapshot);
    else if (emu->history_frames == 1)
        sigusr1_dump_cpus<HistoryTaint>(emu, snapshot);
    else
        sigusr1_dump_cpus<Taint>(emu, snapshot);
}
#include "decode_exec.h"
#include "elf_loader.h"
#include "emulator.h"
#include "mem.h"

extern char** environ;

static void usage(const char* argv0) {
    fprintf(stderr,
            "usage: %s [options] <program> [args...]\n"
            "  -v, --trace       disassemble each guest instruction to stderr\n"
            "  --trace-syscalls  log guest syscalls to stderr (implied by -v)\n"
            "  --stack-trace / --no-stack-trace\n"
            "                    print a symbolized guest stack trace on a fatal\n"
            "                    guest error, before the core file (default: on)\n"
            "  --no-core         do not write core dumps on fatal guest errors\n"
            "                    (the diagnostics still print: the banner, the\n"
            "                    symbolized guest stack trace and the exit status\n"
            "                    are unaffected; cores are disabled by default)\n"
            "  --core            write core dumps on fatal guest errors (re-enables\n"
            "                    what --no-core turns off, overriding the config\n"
            "                    file default)\n"
            "  --passthrough-signals\n"
            "                    do not install host signal handlers: faults kill\n"
            "                    the process like a real host fault (implies\n"
            "                    --no-core)\n"
            "  --history[=VALUE]  record a per-value poison history trail (the guest\n"
            "                     operations that produced or consumed poison) and print\n"
            "                     it, BFD-symbolicated, when a taint check faults. VALUE:\n"
            "                     yes or 1 (or bare --history) records one guest PC per\n"
            "                     step; no or 0 (the default) records nothing; an integer\n"
            "                     N > 1 records N stack frames per step; full or all\n"
            "                     records the whole stack per step\n"
            "  --error-exitcode=N\n"
            "                    on a fatal POISON-class guest error (poison/taint/\n"
            "                    cannot-/unmapped/bad RIP violations) exit with code N\n"
            "                    (1-255) instead of 128+signal -- valgrind parity;\n"
            "                    unimplemented-instruction errors keep 128+SIGILL\n"
            "                    (132), so poison stays distinguishable from unknown\n"
            "                    instructions even with this override set\n"
            "  --var-time-op=known|plausible|everything\n"
            "                    cannot-var-time-op checking level: known traps\n"
            "                    only on documented data-dependent instructions\n"
            "                    (default), plausible also on §5 suspects, and\n"
            "                    everything on anything not provably\n"
            "                    constant-time\n"
            "  --state-trace=FILE\n"
            "                    append the whole architectural state (rip, all\n"
            "                    16 GPRs, raw RFLAGS, ymm0..15) after every\n"
            "                    executed guest instruction to FILE, one line\n"
            "                    per instruction -- the x86sem differential\n"
            "                    suite's emulator-side oracle\n"
            "  -h, --help        print this help\n"
            "\n"
            "Config file: ~/.zegarmistrz is read before the flags (flags win on\n"
            "conflicts). Missing HOME or a missing/unreadable file silently\n"
            "leaves the defaults; the file is never created. One directive per\n"
            "line, whitespace-separated tokens, '#' starts a comment, blank\n"
            "lines are skipped. Directives:\n"
            "  coredump enabled|disabled\n"
            "                    core dumps on fatal guest errors\n"
            "                    (default: disabled, like --no-core)\n"
            "  history none|no|off|0|yes|1|full|all|N\n"
            "                    poison history trail recording, the same values\n"
            "                    as --history= (default: none)\n"
            "  error exitcode N  like --error-exitcode=N (an integer 1-255)\n"
            "  error signal N    poison-fault signal override (an integer 1-31):\n"
            "                    poison-class fatal errors report and exit with\n"
            "                    128+N (precedence: exitcode > signal > natural)\n"
            "Unknown directives and bad values print a warning and are ignored;\n"
            "a config problem never aborts the run.\n"
            "\n"
            "Poisoning client requests (yolo C, not Fil-C): call the magic\n"
            "address 0x1410141014101410 as\n"
            "  ((uint64_t(*)(uint64_t,void*,size_t,uint64_t))0x1410141014101410)\n"
            "      (op, ptr, size, flags)\n"
            "  op 0 = no-op/query (returns 0)\n"
            "  op 1 = poison [ptr,size) with flags bitmask:\n"
            "         bit0 cannot-load, bit1 cannot-store, bit2 cannot-branch,\n"
            "         bit3 cannot-index, bit4 clear-branch-on-store,\n"
            "         bit5 clear-index-on-store, bit6 cannot-var-time-op,\n"
            "         bit7 clear-var-time-op-on-store\n"
            "         e.g. (1,ptr,size,1) poisons cannot-load only\n"
            "  op 2 = unpoison [ptr,size)\n"
            "\n"
            "cannot-var-time-op taint marks values that must not reach an\n"
            "instruction whose execution time depends on operand values.\n"
            "--var-time-op=known checks only the documented data-dependent\n"
            "instructions, plausible adds the undocumented suspects, and\n"
            "everything traps on any instruction not in the guaranteed\n"
            "constant-time list (branch and address dependencies keep their\n"
            "own cannot-branch/cannot-index checks).\n"
            "Presence: CPUID.1:ECX bit 31 (hypervisor present) plus hypervisor\n"
            "leaf 0x40000000: EAX=0x40000000 (max hypervisor leaf), EBX:ECX:EDX\n"
            "= \"Zegarmistrz\\0\". Guest client library: include/zegarmistrz.h\n"
            "(is_in_zegarmistrz, zegarmistrz_query, zegarmistrz_poison_range,\n"
            "zegarmistrz_unpoison_range; all no-ops outside the emulator).\n"
            "\n"
            "On a fatal guest error (poison violation, bad memory access,\n"
            "host signal), zegarmistrz prints a symbolized guest stack trace\n"
            "(BFD) to stderr, then -- unless core dumps are disabled (the\n"
            "default; see --no-core) -- writes zegarmistrz.core.\n"
            "<processname>.<pid> in the current directory as an ELF core\n"
            "(ET_CORE) with guest registers, auxv and memory, readable with\n"
            "gdb/lldb:\n"
            "  gdb -batch -ex bt ./prog zegarmistrz.core.prog.<pid>\n",
            argv0);
}

// ~/.zegarmistrz config file (djb spec). Parsed BEFORE the CLI flags: CLI
// flags win on conflicts. Path is $HOME/.zegarmistrz; a missing HOME or a
// missing/unreadable file silently leaves the defaults (the config is never
// created). Format: one directive per line, whitespace-separated tokens,
// '#' starts a comment (the rest of the line is ignored), blank lines are
// skipped. Config problems are warnings only -- an unknown directive or a
// bad value prints a warning and the run continues with the default (or the
// earlier value) for that setting; nothing here ever aborts the run.
//
// Directives:
//   coredump enabled|disabled   sets emu.no_core exactly like --core /
//                               --no-core (main() defaults to disabled
//                               before this runs). It never touches the
//                               passthrough mode: the directive controls
//                               the core FILE only, the fault handler is
//                               always installed outside
//                               --passthrough-signals.
//   history none|no|off|0|yes|1|full|all|<integer>
//                               same value mapping as --history= (plus the
//                               none/off spellings); bad value: warning.
//   error exitcode N            emu.error_exitcode, an integer 1..255.
//   error signal N              emu.error_signo, an integer 1..31.
static void read_config(Emulator& emu) {
    const char* home = getenv("HOME");
    if (!home || !*home)
        return;
    char path[4096];
    snprintf(path, sizeof(path), "%s/.zegarmistrz", home);
    FILE* f = fopen(path, "r");
    if (!f)
        return; // missing/unreadable: silently use the defaults
    char line[512];
    int lineno = 0;
    while (fgets(line, sizeof(line), f)) {
        lineno++;
        char* hash = strchr(line, '#');
        if (hash)
            *hash = 0;
        char* save = nullptr;
        char* tok = strtok_r(line, " \t\r\n", &save);
        if (!tok)
            continue; // blank (or comment-only) line
        if (!strcmp(tok, "coredump")) {
            char* v = strtok_r(nullptr, " \t\r\n", &save);
            if (v && !strcmp(v, "enabled")) {
                emu.no_core = false;
            } else if (v && !strcmp(v, "disabled")) {
                emu.no_core = true;
            } else {
                if (v)
                    fprintf(stderr,
                            "zegarmistrz: warning: line %d of %s: coredump "
                            "expects enabled or disabled, got '%s'\n",
                            lineno, path, v);
                else
                    fprintf(stderr,
                            "zegarmistrz: warning: line %d of %s: coredump "
                            "expects enabled or disabled\n",
                            lineno, path);
            }
        } else if (!strcmp(tok, "history")) {
            char* v = strtok_r(nullptr, " \t\r\n", &save);
            if (!v || !*v) {
                fprintf(stderr,
                        "zegarmistrz: warning: line %d of %s: history "
                        "expects a value\n",
                        lineno, path);
                continue;
            }
            // Same mapping as --history=, plus the none/off spellings.
            if (!strcmp(v, "none") || !strcmp(v, "no") || !strcmp(v, "off") ||
                !strcmp(v, "0"))
                emu.history_frames = 0;
            else if (!strcmp(v, "yes") || !strcmp(v, "1"))
                emu.history_frames = 1;
            else if (!strcmp(v, "full") || !strcmp(v, "all"))
                emu.history_frames = SIZE_MAX;
            else if (!*v ||
                     v[strspn(v, "0123456789")]) { // not a pure integer
                fprintf(stderr,
                        "zegarmistrz: warning: line %d of %s: bad history "
                        "value '%s' (expected none, no, off, yes, full, all "
                        "or an integer)\n",
                        lineno, path, v);
            } else {
                errno = 0;
                unsigned long long n = strtoull(v, nullptr, 10);
                if (errno || n > SIZE_MAX) {
                    fprintf(stderr,
                            "zegarmistrz: warning: line %d of %s: bad "
                            "history value '%s' (out of range)\n",
                            lineno, path, v);
                } else {
                    emu.history_frames = (size_t)n;
                }
            }
        } else if (!strcmp(tok, "error")) {
            char* what = strtok_r(nullptr, " \t\r\n", &save);
            char* v = what ? strtok_r(nullptr, " \t\r\n", &save) : nullptr;
            if (!what || (!strcmp(what, "exitcode") && !v) ||
                (!strcmp(what, "signal") && !v)) {
                fprintf(stderr,
                        "zegarmistrz: warning: line %d of %s: error expects "
                        "'exitcode N' or 'signal N'\n",
                        lineno, path);
                continue;
            }
            if (strcmp(what, "exitcode") && strcmp(what, "signal")) {
                fprintf(stderr,
                        "zegarmistrz: warning: unknown error directive "
                        "'%s' in %s (expected exitcode or signal)\n",
                        what, path);
                continue;
            }
            if (!*v || v[strspn(v, "0123456789")]) { // not a pure integer
                fprintf(stderr,
                        "zegarmistrz: warning: line %d of %s: bad error %s "
                        "value '%s' (expected an integer)\n",
                        lineno, path, what, v);
                continue;
            }
            errno = 0;
            unsigned long n = strtoul(v, nullptr, 10);
            if (errno || !n) {
                fprintf(stderr,
                        "zegarmistrz: warning: line %d of %s: bad error %s "
                        "value '%s' (expected a positive integer)\n",
                        lineno, path, what, v);
                continue;
            }
            if (!strcmp(what, "exitcode")) {
                if (n > 255) {
                    fprintf(stderr,
                            "zegarmistrz: warning: line %d of %s: bad error "
                            "exitcode value '%s' (expected 1..255)\n",
                            lineno, path, v);
                    continue;
                }
                emu.error_exitcode = (int)n;
            } else {
                if (n > 31) {
                    fprintf(stderr,
                            "zegarmistrz: warning: line %d of %s: bad error "
                            "signal value '%s' (expected 1..31)\n",
                            lineno, path, v);
                    continue;
                }
                emu.error_signo = (int)n;
            }
        } else {
            fprintf(stderr,
                    "zegarmistrz: warning: unknown directive '%s' in %s\n",
                    tok, path);
        }
    }
    fclose(f);
}

// Build the initial guest stack per the Linux x86-64 ABI and return the
// new RSP. Maps an 8MB stack with mmap.
static uint64_t build_stack(Emulator* emu, const std::vector<std::string>& args) {
    const size_t kStackSize = 8 << 20;
    uint8_t* stack = (uint8_t*)mmap(nullptr, kStackSize, PROT_READ | PROT_WRITE,
                                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (stack == MAP_FAILED) {
        perror("mmap guest stack");
        exit(1);
    }
    emu->guest_stack_base = (uint64_t)(uintptr_t)stack;
    emu->guest_stack_size = kStackSize;
    uint8_t* top = stack + kStackSize;

    // Collect env strings.
    std::vector<std::string> envs;
    for (char** e = environ; *e; e++)
        envs.push_back(*e);

    // Auxv pairs (key,value) built identically below; keep count in sync.
    const size_t kAuxPairs = 18; // 17 real + AT_NULL
    size_t strings_bytes = 16;   // AT_RANDOM blob
    for (auto& x : envs)
        strings_bytes += x.size() + 1;
    for (auto& x : args)
        strings_bytes += x.size() + 1;
    strings_bytes += emu->prog_path.size() + 1;
    size_t words =
        1 + (args.size() + 1) + (envs.size() + 1) + 2 * kAuxPairs;
    size_t total = strings_bytes + 8 * words;
    size_t pad = (16 - (total % 16)) % 16;
    top -= pad; // keep final RSP 16-aligned with contiguous data

    auto push_bytes = [&](const void* data, size_t n) -> uint64_t {
        top -= n;
        memcpy(top, data, n);
        return (uint64_t)(uintptr_t)top;
    };
    auto push_str = [&](const std::string& str) -> uint64_t {
        top -= str.size() + 1;
        memcpy(top, str.c_str(), str.size() + 1);
        return (uint64_t)(uintptr_t)top;
    };
    auto push_u64 = [&](uint64_t v) -> uint64_t {
        top -= 8;
        memcpy(top, &v, 8);
        return (uint64_t)(uintptr_t)top;
    };

    // 16 random bytes for AT_RANDOM.
    uint8_t randbytes[16];
    {
        int fd = open("/dev/urandom", O_RDONLY);
        if (fd >= 0) {
            if (read(fd, randbytes, sizeof(randbytes)) != (ssize_t)sizeof(randbytes)) {
                for (int i = 0; i < 16; i++)
                    randbytes[i] = (uint8_t)(getpid() + i * 31);
            }
            close(fd);
        } else {
            for (int i = 0; i < 16; i++)
                randbytes[i] = (uint8_t)(getpid() + i * 31);
        }
    }

    std::vector<uint64_t> env_addrs;
    for (auto& e : envs)
        env_addrs.push_back(push_str(e));
    std::vector<uint64_t> arg_addrs;
    for (auto& a : args)
        arg_addrs.push_back(push_str(a));
    uint64_t execfn_addr = push_str(emu->prog_path);
    uint64_t rand_addr = push_bytes(randbytes, 16);

    unsigned long hwcap = getauxval(AT_HWCAP);
    unsigned long hwcap2 = getauxval(AT_HWCAP2);

    // Auxv pairs.
    std::vector<uint64_t> auxv;
    auto aux = [&](uint64_t k, uint64_t v) {
        auxv.push_back(k);
        auxv.push_back(v);
    };
    aux(AT_PHDR, emu->exe_phdr);
    aux(AT_PHENT, emu->exe_phent);
    aux(AT_PHNUM, emu->exe_phnum);
    aux(AT_PAGESZ, 4096);
    aux(AT_BASE, emu->interp_base);
    aux(AT_FLAGS, 0);
    aux(AT_ENTRY, emu->exe_entry);
    aux(AT_UID, getuid());
    aux(AT_EUID, geteuid());
    aux(AT_GID, getgid());
    aux(AT_EGID, getegid());
    aux(AT_CLKTCK, 100);
    aux(AT_HWCAP, hwcap);
    aux(AT_HWCAP2, hwcap2);
    aux(AT_RANDOM, rand_addr);
    aux(AT_EXECFN, execfn_addr);
    aux(AT_SECURE, 0);
    aux(AT_NULL, 0);

    {
        // Snapshot for NT_AUXV in core dumps.
        std::lock_guard<std::mutex> l(emu->maps_mu);
        emu->guest_auxv = auxv;
    }

    // Pad to align: total below-strings area must leave RSP 16-aligned.
    // Layout (growing down): auxv, envp(NULL-term), argv(NULL-term), argc.
    size_t nwords = 1 + (arg_addrs.size() + 1) + (env_addrs.size() + 1) + auxv.size();
    // Compute current top offset and pad so final RSP % 16 == 0.
    // We'll push in reverse: auxv values, then envp, argv, argc.
    for (size_t i = auxv.size(); i > 0; i--)
        push_u64(auxv[i - 1]);
    push_u64(0); // envp NULL
    for (size_t i = env_addrs.size(); i > 0; i--)
        push_u64(env_addrs[i - 1]);
    push_u64(0); // argv NULL
    for (size_t i = arg_addrs.size(); i > 0; i--)
        push_u64(arg_addrs[i - 1]);
    push_u64(arg_addrs.size()); // argc

    uint64_t rsp = (uint64_t)(uintptr_t)top;
    (void)nwords;
    return rsp;
}

// Set up the initial CPU for one taint type and run the interpreter loop
// on it. run_cpu_loop never returns: the process exits via the exit/exit_group
// syscall handler or through a fatal guest error's dump + _exit path.
template<typename TaintT>
static int run_guest(Emulator& emu, uint64_t entry, uint64_t rsp) {
    CPU<TaintT> cpu;
    cpu.emu = &emu;
    {
        std::lock_guard<std::mutex> l(emu.thread_mu);
        emu.thread_cpus.push_back((void*)&cpu);
    }
    cpu.rip = entry;
    cpu.gpr[ZG_RSP].val = rsp;
    cpu.gpr[ZG_RDX].val = 0; // no DT_FINI atexit function at startup
    cpu.rflags = 0x2;
    cpu.tid = (int)::syscall(SYS_gettid);
    emu.main_tid = cpu.tid;
    {
        std::lock_guard<std::mutex> l(emu.thread_mu);
        emu.thread_tids.push_back((uint64_t)cpu.tid);
    }

    if (emu.trace_syscall) {
        fprintf(stderr, "[zegarmistrz] entry=%#lx rsp=%#lx exe_base=%#lx interp_base=%#lx\n",
                entry, rsp, emu.exe_base, emu.interp_base);
        fflush(stderr);
    }
    run_cpu_loop<TaintT>(&cpu);
    return 0; // unreachable (exit via syscall handler)
}

int main(int argc, char** argv) {
    Emulator emu;
    g_sig_emu = &emu;
    signal(SIGUSR1, sigusr1_handler);
    std::vector<std::string> guest_args;
    const char* prog = nullptr;

    // djb default: core dumps DISABLED unless the config file (`coredump
    // enabled`) or a flag (--core) turns them back on. The config is read
    // first, then the flags; both simply set the policy, and the flags win
    // on conflicts because they run later.
    //
    // Only the core FILE write is disabled by default: the host-fault
    // handler is still installed (mem_install_fault_handler below skips it
    // only in passthrough mode), so an unmapped access or another host
    // fault still prints the banner + symbolized guest stack trace (and
    // --error-exitcode still applies) instead of killing the process with
    // a bare host SIGSEGV. Suppressing the handlers entirely is the
    // separate --passthrough-signals policy, set only by that flag.
    emu.no_core = true;
    read_config(emu);

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "-v" || a == "--trace") {
            emu.trace_insn = true;
            emu.trace_syscall = true;
        } else if (a == "--trace-syscalls") {
            emu.trace_syscall = true;
        } else if (a == "--no-core") {
            // Do not write core dumps; the fault handler stays installed,
            // so the banner + guest stack trace still print.
            emu.no_core = true;
        } else if (a == "--passthrough-signals") {
            // Do not install host signal handlers at all: faults kill the
            // process like a real host fault. Implies no core dumps.
            emu.no_core = true;
            mem_set_passthrough(true);
        } else if (a == "--core") {
            // Re-enable core dumps (overrides the config / the disabled
            // default). Diagnostics print either way; this flag only
            // turns the core file on.
            emu.no_core = false;
        } else if (a == "--stack-trace") {
            emu.stack_trace = true;
        } else if (a == "--no-stack-trace") {
            emu.stack_trace = false;
        } else if (a == "--history") {
            emu.history_frames = 1;
        } else if (a.rfind("--history=", 0) == 0) {
            std::string v = a.substr(sizeof("--history=") - 1);
            if (v == "yes")
                emu.history_frames = 1;
            else if (v == "no")
                emu.history_frames = 0;
            else if (v == "full" || v == "all")
                emu.history_frames = SIZE_MAX;
            else if (!v.empty() &&
                     v.find_first_not_of("0123456789") == std::string::npos) {
                errno = 0;
                unsigned long long n = strtoull(v.c_str(), nullptr, 10);
                if (errno || n > SIZE_MAX) {
                    fprintf(stderr, "zegarmistrz: bad --history value '%s' (expected "
                                    "yes, no, full, all, or an integer)\n",
                            v.c_str());
                    return 1;
                }
                emu.history_frames = (size_t)n;
            } else {
                fprintf(stderr, "zegarmistrz: bad --history value '%s' (expected "
                                "yes, no, full, all, or an integer)\n",
                        v.c_str());
                return 1;
            }
        } else if (a.rfind("--error-exitcode=", 0) == 0) {
            // Fatal poison-class guest errors exit with N instead of
            // 128+signal (valgrind parity). Attached value only, like
            // --var-time-op=; must be an integer 1..255.
            std::string v = a.substr(sizeof("--error-exitcode=") - 1);
            unsigned long n = 0;
            bool ok = !v.empty() && !v[strspn(v.c_str(), "0123456789")];
            if (ok) {
                errno = 0;
                n = strtoul(v.c_str(), nullptr, 10);
                ok = !errno && n >= 1 && n <= 255;
            }
            if (!ok) {
                fprintf(stderr,
                        "zegarmistrz: bad --error-exitcode value '%s' "
                        "(expected an integer 1..255)\n",
                        v.c_str());
                return 1;
            }
            emu.error_exitcode = (int)n;
        } else if (a.rfind("--state-trace=", 0) == 0) {
            // Whole-state trace for the x86sem differential suite
            // (x86sem-tests/): one line per executed guest instruction,
            // written to FILE. Attached value required, like
            // --history=; a missing/empty value is a usage error. The
            // file is opened at emulator start (after argument parsing
            // -- not lazily by the run loop), so an unopenable path
            // surfaces as a clean CLI error instead of a mid-run
            // surprise.
            std::string v = a.substr(sizeof("--state-trace=") - 1);
            if (v.empty()) {
                fprintf(stderr,
                        "zegarmistrz: --state-trace requires a file path "
                        "(--state-trace=FILE)\n");
                return 1;
            }
            emu.state_trace_path = v;
        } else if (a.rfind("--var-time-op=", 0) == 0) {
            std::string v = a.substr(sizeof("--var-time-op=") - 1);
            if (v == "known")
                emu.var_time_op = Emulator::ZG_VAR_TIME_KNOWN;
            else if (v == "plausible")
                emu.var_time_op = Emulator::ZG_VAR_TIME_PLAUSIBLE;
            else if (v == "everything")
                emu.var_time_op = Emulator::ZG_VAR_TIME_EVERYTHING;
            else {
                fprintf(stderr,
                        "zegarmistrz: bad --var-time-op value '%s' (expected "
                        "known, plausible or everything)\n",
                        v.c_str());
                return 1;
            }
        } else if (a == "-h" || a == "--help") {
            usage(argv[0]);
            return 0;
        } else if (!prog) {
            prog = argv[i];
        } else {
            guest_args.push_back(argv[i]);
        }
    }
    if (!prog) {
        usage(argv[0]);
        return 1;
    }
    emu.prog_path = prog;
    guest_args.insert(guest_args.begin(), prog);

    if (!emu.state_trace_path.empty()) {
        // Opened at emulator start (see Emulator::state_trace_fd): an
        // unopenable path is a clean CLI error, not a mid-run surprise.
        emu.state_trace_fd = open(emu.state_trace_path.c_str(),
                                  O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (emu.state_trace_fd < 0) {
            fprintf(stderr, "zegarmistrz: cannot open --state-trace file %s: "
                            "%s\n",
                    emu.state_trace_path.c_str(), strerror(errno));
            return 1;
        }
    }

    mem_install_fault_handler();

    uint64_t entry = 0;
    try {
        entry = elf_load_program(&emu, prog);
    } catch (std::string& e) {
        fprintf(stderr, "zegarmistrz: load failed: %s\n", e.c_str());
        return 1;
    }

    uint64_t rsp = build_stack(&emu, guest_args);

    // --history selects the engine's taint sidecar: plain Taint (off/--history=
    // no/0), HistoryTaint (--history/=yes/=1: one PC per step), or
    // DeepHistoryTaint (--history=N for N > 1 and --history=full: recorded
    // stacks per step, capped at N frames).
    if (!emu.history_frames)
        return run_guest<Taint>(emu, entry, rsp);
    if (emu.history_frames == 1)
        return run_guest<HistoryTaint>(emu, entry, rsp);
    zeg_set_history_frames(emu.history_frames == SIZE_MAX ? 0
                                                          : emu.history_frames);
    return run_guest<DeepHistoryTaint>(emu, entry, rsp);
}
