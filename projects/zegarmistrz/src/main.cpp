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
    char buf[256];
    // Copy the live-thread list under lock; entries are erased (never left
    // dangling) on thread exit, but the handler still skips nulls.
    std::vector<CPU*> snapshot;
    {
        std::lock_guard<std::mutex> l(emu->thread_mu);
        snapshot = emu->thread_cpus;
    }
    for (size_t t = 0; t < snapshot.size(); t++) {
        CPU* cpu = snapshot[t];
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
            "  --no-core, --passthrough-signals\n"
            "                    do not install host signal handlers and do not\n"
            "                    write core dumps (guest errors just print)\n"
            "  -h, --help        print this help\n"
            "\n"
            "Poisoning client requests (yolo C, not Fil-C): call the magic\n"
            "address 0x1410141014101410 as\n"
            "  ((uint64_t(*)(uint64_t,void*,size_t,uint64_t))0x1410141014101410)\n"
            "      (op, ptr, size, flags)\n"
            "  op 0 = no-op/query (returns 0)\n"
            "  op 1 = poison [ptr,size) with flags bitmask:\n"
            "         bit0 cannot-load, bit1 cannot-store, bit2 cannot-branch,\n"
            "         bit3 cannot-index, bit4 clear-branch-on-store,\n"
            "         bit5 clear-index-on-store\n"
            "         e.g. (1,ptr,size,1) poisons cannot-load only\n"
            "  op 2 = unpoison [ptr,size)\n"
            "\n"
            "Presence: CPUID.1:ECX bit 31 (hypervisor present) plus hypervisor\n"
            "leaf 0x40000000: EAX=0x40000000 (max hypervisor leaf), EBX:ECX:EDX\n"
            "= \"Zegarmistrz\\0\". Guest client library: include/zegarmistrz.h\n"
            "(is_in_zegarmistrz, zegarmistrz_query, zegarmistrz_poison_range,\n"
            "zegarmistrz_unpoison_range; all no-ops outside the emulator).\n"
            "\n"
            "On a fatal guest error (poison violation, bad memory access,\n"
            "host signal), zegarmistrz writes zegarmistrz.core.<processname>.\n"
            "<pid> in the current directory as an ELF core (ET_CORE) with\n"
            "guest registers, auxv and memory, readable with gdb/lldb:\n"
            "  gdb -batch -ex bt ./prog zegarmistrz.core.prog.<pid>\n",
            argv0);
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

int main(int argc, char** argv) {
    Emulator emu;
    g_sig_emu = &emu;
    signal(SIGUSR1, sigusr1_handler);
    std::vector<std::string> guest_args;
    const char* prog = nullptr;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "-v" || a == "--trace") {
            emu.trace_insn = true;
            emu.trace_syscall = true;
        } else if (a == "--trace-syscalls") {
            emu.trace_syscall = true;
        } else if (a == "--no-core" || a == "--passthrough-signals") {
            emu.no_core = true;
            mem_set_passthrough(true);
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

    mem_install_fault_handler();

    uint64_t entry = 0;
    try {
        entry = elf_load_program(&emu, prog);
    } catch (std::string& e) {
        fprintf(stderr, "zegarmistrz: load failed: %s\n", e.c_str());
        return 1;
    }

    uint64_t rsp = build_stack(&emu, guest_args);

    CPU cpu;
    cpu.emu = &emu;
    {
        std::lock_guard<std::mutex> l(emu.thread_mu);
        emu.thread_cpus.push_back(&cpu);
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
    run_cpu_loop(&cpu);
    return 0; // unreachable (exit via syscall handler)
}
