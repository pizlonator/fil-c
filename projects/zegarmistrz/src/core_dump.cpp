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

// ELF core dumps: on a fatal guest error, write
// zegarmistrz.core.<processname>.<pid> as an ET_CORE file carrying the GUEST
// register state (NT_PRSTATUS), process name (NT_PRPSINFO), auxv (NT_AUXV),
// and guest memory (PT_LOADs), so gdb/lldb show the guest as if it had run
// on bare metal (no interpreter frames).

#include "decode_exec.h"

#include <elf.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/procfs.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "cpu.h"
#include "emulator.h"
#include "mem.h"
#include "stack_trace.h"

#include <atomic>

// Signal chosen for the most recent fatal guest error (for exit-status
// consistency: the process exits with 128+signo). Set by dump_core.
std::atomic<int> g_last_fatal_signo{0};

// Fatal-error reason classification, shared by guest_fatal_signo (the
// NT_PRSTATUS signal choice) and zeg_fatal_exit_code (the process exit
// code). Two exported predicates (declared in decode_exec.h):
//
//   poison-class   - memory-safety/taint violations: poisoned
//     cannot-load/store, tainted cannot-branch/index/var-time values,
//     unmapped accesses, bad RIP, string-op taint. SIGSEGV (exit 139).
//   unimplemented-class - emulator-limitation errors: instructions or
//     operand forms zegarmistrz does not implement. SIGILL (exit 132) --
//     the "different error code" for unknown instructions.
//   other-class    - everything else (bad syscalls, ...). SIGABRT (exit
//     134).
//
// Host-signal reasons ("guest SIGSEGV ...", ...) are matched literally in
// guest_fatal_signo before either predicate and keep the host signal.
//
// NOTE: the poison predicate keeps exactly the substrings guest_fatal_signo
// has always matched for the SIGSEGV bucket, so existing reason strings
// keep their signal. A reason matching BOTH classes (e.g. "bad string op",
// an unsupported string-op form that embeds the string-op taint name) is
// poison-class: guest_fatal_signo checks poison first, and so does
// zeg_fatal_exit_code.

bool zeg_reason_is_poison(const char* reason) {
    if (!reason)
        return false;
    return strstr(reason, "poison") || strstr(reason, "taint") ||
           strstr(reason, "cannot-") || strstr(reason, "unmapped") ||
           strstr(reason, "bad RIP") || strstr(reason, "string op");
}

bool zeg_reason_is_unimplemented(const char* reason) {
    if (!reason)
        return false;
    // "bad RIP" is a memory-safety violation (execution off the rails), not
    // an unsupported form: it stays poison-class.
    if (strstr(reason, "bad RIP"))
        return false;
    if (!strncmp(reason, "unimplemented", 13))
        return true;
    if (strstr(reason, "decode failed"))
        return true;
    if (strstr(reason, "not supported"))
        return true;
    if (strstr(reason, "MVEX"))
        return true;
    if (!strncmp(reason, "bad ", 4))
        return true;
    if (!strncmp(reason, "unsupported ", 12))
        return true;
    if (!strncmp(reason, "cannot store", 12))
        return true;
    if (!strncmp(reason, "cannot determine", 16))
        return true;
    return false;
}

// Map a fatal guest error to the signal recorded in NT_PRSTATUS (si_signo /
// pr_cursig) and used for the process exit status (128+signo):
//   mem violations (host SIGSEGV/SIGBUS-style faults, poisoned
//   cannot-load/store, tainted cannot-branch/index values, unmapped reads/
//   writes, bad RIP) => SIGSEGV (exit 139);
//   unimplemented instructions/forms => SIGILL (exit 132);
//   bad opcodes recognized as traps (INT3, UD2) => SIGTRAP;
//   arithmetic faults => SIGFPE;
//   everything else (bad requests) => SIGABRT (exit 134).
int guest_fatal_signo(const char* reason, int hint) {
    if (hint)
        return hint;
    if (!reason)
        return SIGABRT;
    if (strstr(reason, "SIGSEGV"))
        return SIGSEGV;
    if (strstr(reason, "SIGBUS"))
        return SIGBUS;
    if (strstr(reason, "SIGILL"))
        return SIGILL;
    if (strstr(reason, "SIGFPE"))
        return SIGFPE;
    if (strstr(reason, "SIGTRAP") || strstr(reason, "INT3") ||
        strstr(reason, "UD2"))
        return SIGTRAP;
    if (zeg_reason_is_poison(reason))
        return SIGSEGV;
    if (zeg_reason_is_unimplemented(reason))
        return SIGILL;
    return SIGABRT;
}

// Exit-code plumbing shared by both run_cpu_loop catch sites. emu carries
// the exitcode override (--error-exitcode=N on the CLI, config `error
// exitcode N`) and the signal override (config-only, `error signal N`;
// there is no CLI form), valgrind-style, applied to POISON-class fatal
// errors only, so poison violations stay distinguishable from
// unimplemented-instruction errors even with the overrides configured:
//
//   precedence: error_exitcode (exit code override) > error_signo (signal
//   override, exit becomes 128+error_signo) > the natural 128+signo.
//
// eff_signo is the caller's effective signal (hint already applied); when
// the error_signo override fires, the adjusted signo is written back
// through eff_out so the caller passes it to dump_core (NT_PRSTATUS
// records it). The writeback is gated on the exitcode override NOT
// firing: when the exitcode override wins, the process exits with the
// configured code instead of 128+signo, so the core's recorded signal
// must not claim the signal override -- NT_PRSTATUS then keeps the
// caller's natural signo, which agrees with the exit status actually
// used. Unimplemented and other classes always keep the natural exit
// code.
int zeg_fatal_exit_code(Emulator* emu, const char* reason, int eff_signo,
                        int* eff_out) {
    // Exitcode override wins outright: the process exits with the
    // configured code, not 128+signo, so leave eff_signo (and the core's
    // NT_PRSTATUS signal) at the caller's natural value.
    if (zeg_reason_is_poison(reason) && emu && emu->error_exitcode)
        return emu->error_exitcode;
    int eff = eff_signo;
    if (zeg_reason_is_poison(reason) && emu && emu->error_signo)
        eff = emu->error_signo;
    if (eff_out)
        *eff_out = eff;
    return 128 + eff;
}

// Guest general registers in elf_gregset_t order (x86-64):
// R15,R14,R13,R12,RBP,RBX,R11,R10,R9,R8,RAX,RCX,RDX,RSI,RDI,ORIG_RAX,
// RIP,CS,EFLAGS,RSP,SS,FS_BASE,GS_BASE,DS,ES,FS,GS.
template<typename TaintT>
static void fill_gregs(const CPU<TaintT>* cpu, elf_gregset_t& regs) {
    memset(regs, 0, sizeof(regs));
    regs[0] = cpu->gpr[ZG_R15].val;
    regs[1] = cpu->gpr[ZG_R14].val;
    regs[2] = cpu->gpr[ZG_R13].val;
    regs[3] = cpu->gpr[ZG_R12].val;
    regs[4] = cpu->gpr[ZG_RBP].val;
    regs[5] = cpu->gpr[ZG_RBX].val;
    regs[6] = cpu->gpr[ZG_R11].val;
    regs[7] = cpu->gpr[ZG_R10].val;
    regs[8] = cpu->gpr[ZG_R9].val;
    regs[9] = cpu->gpr[ZG_R8].val;
    regs[10] = cpu->gpr[ZG_RAX].val;
    regs[11] = cpu->gpr[ZG_RCX].val;
    regs[12] = cpu->gpr[ZG_RDX].val;
    regs[13] = cpu->gpr[ZG_RSI].val;
    regs[14] = cpu->gpr[ZG_RDI].val;
    regs[15] = (elf_greg_t)-1; // ORIG_RAX: unknown
    regs[16] = cpu->rip;
    regs[17] = 0x33; // CS (64-bit user code)
    regs[18] = cpu->rflags;
    regs[19] = cpu->gpr[ZG_RSP].val;
    regs[20] = 0x2b; // SS (64-bit user data)
    regs[21] = cpu->fs_base;
    regs[22] = cpu->gs_base;
    regs[23] = 0; // DS
    regs[24] = 0; // ES
    regs[25] = 0; // FS
    regs[26] = 0; // GS
}

struct Note {
    uint32_t type;
    std::string name; // "CORE"
    std::vector<uint8_t> desc;
};

static void append_note(std::vector<uint8_t>& out, uint32_t type,
                        const void* desc, size_t len) {
    Elf64_Nhdr nhdr;
    nhdr.n_namesz = 5; // "CORE\0"
    nhdr.n_descsz = (uint32_t)len;
    nhdr.n_type = type;
    size_t base = out.size();
    out.resize(base + sizeof(nhdr) + 8 + ((len + 3) & ~3u));
    memcpy(out.data() + base, &nhdr, sizeof(nhdr));
    memcpy(out.data() + base + sizeof(nhdr), "CORE", 5);
    memset(out.data() + base + sizeof(nhdr) + 8, 0, ((len + 3) & ~3u));
    memcpy(out.data() + base + sizeof(nhdr) + 8, desc, len);
}

// Templated on the taint type so both engine instantiations dump identically
// (the core contents never depend on the taint sidecar; only the optional
// --history poison-history block does).
template<typename TaintT>
const char* dump_core(CPU<TaintT>* cpu, const char* reason,
                      unsigned long fault_addr, int signo,
                      const std::vector<ZegHistStep>& poison_history) {
    // A host fault while producing the dump (e.g. inside BFD parsing)
    // must kill the process, not longjmp back into run_cpu_loop: the
    // LoopScope recovery is still armed in the caller, so the jump would
    // re-enter this dump and livelock. The flag is per-thread: other
    // guest threads keep executing (with their own recovery) while this
    // thread dumps. mem_copy_guarded() still recovers locally (its
    // recovery point is checked first). See fault_handler().
    struct DumpingScope {
        DumpingScope() { mem_set_dumping(true); }
        ~DumpingScope() { mem_set_dumping(false); }
    } dumping;
    static thread_local char path[256];
    const char* prog = cpu->emu->prog_path.c_str();
    const char* base = strrchr(prog, '/');
    base = base ? base + 1 : prog;
    if (!*base)
        base = "guest";
    snprintf(path, sizeof(path), "zegarmistrz.core.%s.%d", base, (int)getpid());

    // Fatal-error banner first, then the guest stack trace (on by
    // default), then the core file itself -- so the trace is visible
    // even if producing the core is interrupted (Ctrl-C).
    fprintf(stderr, "zegarmistrz: fatal guest error (tid=%d): %s at rip=%#lx",
            cpu->tid, reason ? reason : "?", cpu->rip);
    if (fault_addr)
        fprintf(stderr, " fault_addr=%#lx", fault_addr);
    fprintf(stderr, "\n");
    fflush(stderr);

    if (cpu->emu->stack_trace)
        print_guest_stack_trace(cpu);

    // --history: the offending value's recorded poison trail, symbolicated
    // with the same BFD machinery as the stack trace and sorted by each
    // step's recording seq into true chronological order.
    if (!poison_history.empty())
        print_poison_history(cpu->emu, poison_history);

    if (cpu->emu->no_core) {
        // Core dumps disabled (--no-core, config `coredump disabled`, or
        // the default): report without writing anything. The banner and
        // the guest stack trace above still print -- only the file is
        // skipped. True passthrough runs (--passthrough-signals) imply
        // no_core, so they land here too.
        fprintf(stderr, "zegarmistrz: [core dumps disabled]\n");
        fflush(stderr);
        return path;
    }

    if (!signo)
        signo = guest_fatal_signo(reason, 0);
    g_last_fatal_signo.store(signo);

    // --- notes ---
    struct elf_prstatus pr;
    memset(&pr, 0, sizeof(pr));
    pr.pr_info.si_signo = signo;
    pr.pr_info.si_code = 1; // SI_QUEUE-ish; exact code unknown
    pr.pr_info.si_errno = 0;
    pr.pr_cursig = (short)signo;
    pr.pr_pid = getpid();
    pr.pr_ppid = getppid();
    fill_gregs(cpu, pr.pr_reg);
    pr.pr_fpvalid = 0;

    struct elf_prpsinfo ps;
    memset(&ps, 0, sizeof(ps));
    ps.pr_pid = getpid();
    ps.pr_ppid = getppid();
    strncpy(ps.pr_fname, base, sizeof(ps.pr_fname) - 1);
    {
        std::string args = cpu->emu->prog_path + " (emulated)";
        strncpy(ps.pr_psargs, args.c_str(), sizeof(ps.pr_psargs) - 1);
    }

    std::vector<uint8_t> notes;
    append_note(notes, NT_PRSTATUS, &pr, sizeof(pr));
    append_note(notes, NT_PRPSINFO, &ps, sizeof(ps));
    {
        // NT_AUXV from the auxv we placed on the guest stack.
        std::vector<Elf64_auxv_t> av;
        {
            std::lock_guard<std::mutex> l(cpu->emu->maps_mu);
            for (size_t i = 0; i + 1 < cpu->emu->guest_auxv.size(); i += 2) {
                Elf64_auxv_t e;
                e.a_type = cpu->emu->guest_auxv[i];
                e.a_un.a_val = cpu->emu->guest_auxv[i + 1];
                av.push_back(e);
                if (e.a_type == AT_NULL)
                    break;
            }
        }
        if (av.empty() || av.back().a_type != AT_NULL) {
            Elf64_auxv_t e;
            e.a_type = AT_NULL;
            e.a_un.a_val = 0;
            av.push_back(e);
        }
        append_note(notes, NT_AUXV, av.data(), av.size() * sizeof(av[0]));
    }

    // --- load segments: readable guest mappings + initial stack ---
    struct Seg {
        uint64_t start, end;
        uint32_t flags;
    };
    std::vector<Seg> segs;
    {
        std::lock_guard<std::mutex> l(cpu->emu->maps_mu);
        for (auto& m : cpu->emu->guest_maps) {
            if (!(m.prot & (PROT_READ | PROT_WRITE | PROT_EXEC)))
                continue;
            if (m.end <= m.start)
                continue;
            uint32_t fl = 0;
            if (m.prot & PROT_READ)
                fl |= PF_R;
            if (m.prot & PROT_WRITE)
                fl |= PF_W;
            if (m.prot & PROT_EXEC)
                fl |= PF_X;
            segs.push_back(Seg{m.start, m.end, fl});
        }
        if (cpu->emu->guest_stack_size) {
            segs.push_back(Seg{cpu->emu->guest_stack_base,
                               cpu->emu->guest_stack_base +
                                   cpu->emu->guest_stack_size,
                               (uint32_t)(PF_R | PF_W)});
        }
    }

    // Cap total dumped bytes (~256MB) to stay responsive.
    const uint64_t kCap = 256ULL << 20;
    uint64_t total = 0;
    for (auto& s : segs)
        total += s.end - s.start;
    if (total > kCap) {
        // Drop largest segments first (keep code + stack).
        // Simple: truncate the list from the end (loader maps exe first).
        while (segs.size() > 1 && total > kCap) {
            total -= segs.back().end - segs.back().start;
            segs.pop_back();
        }
    }

    size_t phnum = 1 + segs.size();
    size_t hdr_size = sizeof(Elf64_Ehdr) + phnum * sizeof(Elf64_Phdr);
    size_t notes_off = hdr_size;
    size_t notes_size = (notes.size() + 7) & ~7u;
    size_t data_off = notes_off + notes_size;

    FILE* f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "zegarmistrz: cannot write core %s\n", path);
        fflush(stderr);
        return path;
    }

    Elf64_Ehdr eh;
    memset(&eh, 0, sizeof(eh));
    eh.e_ident[0] = 0x7f;
    eh.e_ident[1] = 'E';
    eh.e_ident[2] = 'L';
    eh.e_ident[3] = 'F';
    eh.e_ident[EI_CLASS] = ELFCLASS64;
    eh.e_ident[EI_DATA] = ELFDATA2LSB;
    eh.e_ident[EI_VERSION] = EV_CURRENT;
    eh.e_ident[EI_OSABI] = ELFOSABI_SYSV;
    eh.e_type = ET_CORE;
    eh.e_machine = EM_X86_64;
    eh.e_version = EV_CURRENT;
    eh.e_phoff = sizeof(Elf64_Ehdr);
    eh.e_ehsize = sizeof(Elf64_Ehdr);
    eh.e_phentsize = sizeof(Elf64_Phdr);
    eh.e_phnum = (uint16_t)phnum;
    fwrite(&eh, 1, sizeof(eh), f);

    std::vector<Elf64_Phdr> ph(phnum);
    memset(ph.data(), 0, ph.size() * sizeof(ph[0]));
    ph[0].p_type = PT_NOTE;
    ph[0].p_offset = notes_off;
    ph[0].p_filesz = notes.size();
    ph[0].p_memsz = notes.size();
    ph[0].p_align = 4; // NB: readers align notes by p_align; kernels use 4.
    uint64_t off = data_off;
    for (size_t i = 0; i < segs.size(); i++) {
        uint64_t sz = segs[i].end - segs[i].start;
        ph[1 + i].p_type = PT_LOAD;
        ph[1 + i].p_offset = off;
        ph[1 + i].p_vaddr = segs[i].start;
        ph[1 + i].p_paddr = 0;
        ph[1 + i].p_filesz = sz;
        ph[1 + i].p_memsz = sz;
        ph[1 + i].p_flags = segs[i].flags;
        ph[1 + i].p_align = 0x1000;
        off += sz;
    }
    fwrite(ph.data(), 1, ph.size() * sizeof(ph[0]), f);
    {
        // Pad notes to alignment.
        std::vector<uint8_t> pad(notes_size, 0);
        memcpy(pad.data(), notes.data(), notes.size());
        fwrite(pad.data(), 1, pad.size(), f);
    }
    {
        // Segment contents: direct memcpy (guest VA == host VA). Sizes come
        // from our own mappings, but a stale entry (e.g. a page the guest
        // unmapped without our knowledge) must not crash the dumper, so the
        // copy is fault-guarded and unreadable pages dump as zeros.
        std::vector<uint8_t> buf;
        for (auto& s : segs) {
            uint64_t sz = s.end - s.start;
            buf.resize((size_t)sz);
            mem_copy_guarded(s.start, buf.data(), (size_t)sz);
            fwrite(buf.data(), 1, (size_t)sz, f);
        }
    }
    fclose(f);
    chmod(path, 0600);

    fprintf(stderr, "zegarmistrz: [core: %s]\n", path);
    fflush(stderr);
    return path;
}

// Explicit instantiations: run_cpu_loop (decode_exec.cpp) dumps for either
// taint type.
template const char* dump_core<Taint>(CPU<Taint>*, const char*,
                                      unsigned long, int,
                                      const std::vector<ZegHistStep>&);
template const char* dump_core<HistoryTaint>(CPU<HistoryTaint>*, const char*,
                                             unsigned long, int,
                                             const std::vector<ZegHistStep>&);
template const char* dump_core<DeepHistoryTaint>(
    CPU<DeepHistoryTaint>*, const char*, unsigned long, int,
    const std::vector<ZegHistStep>&);
