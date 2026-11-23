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

// Crash-time guest stack traces. The walk starts at the exact faulting RIP,
// then unwinds primarily with DWARF CFI (.eh_frame/.debug_frame, the same
// mechanism gdb uses on the core: see dwarf_unwind.cpp), which also works
// for guests built with -fomit-frame-pointer. Guest addresses are
// symbolized with BFD against the ELF objects recorded in the guest memory
// map: the loader's own objects plus file-backed guest mmaps (shared
// libraries the guest ld.so mapped, dlopen'd objects). Each physical frame
// expands its DWARF inline chain (innermost inlined function first).
//
// When a module has no usable CFI (e.g. freestanding guests), the legacy
// unwinders still apply, in order: the guest RBP frame chain, then an
// approximate scan of the guest stack for plausible return addresses.
//
// Symbolization follows gdb's line attribution: a return-address frame
// (every frame after 0, however it was unwound) symbolicates at its
// address MINUS ONE -- gdb's "address in block" -- so the reported line
// is the call's own line rather than the first line after the call. The
// printed address stays the raw return address.

#include "stack_trace.h"

#include <cxxabi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <bfd.h>

#include "cpu.h"
#include "dwarf_unwind.h"
#include "emulator.h"
#include "mem.h"

namespace {

// BFD is not thread-safe (and its header/lib pairing is enforced by the
// Makefile probe). Crash-time tracing is rare: one global lock.
std::mutex g_bfd_mu;
bool g_bfd_ready = false;

struct SymTab {
    bfd* abfd = nullptr;
    asymbol** syms = nullptr; // regular symtab, else dynamic symtab
    bool usable = false;
};

SymTab* get_sym_tab(const std::string& path) {
    static std::map<std::string, SymTab> tabs;
    if (!g_bfd_ready) {
        bfd_init();
        g_bfd_ready = true;
    }
    auto it = tabs.find(path);
    if (it != tabs.end())
        return &it->second;
    SymTab t;
    bfd* abfd = bfd_openr(path.c_str(), nullptr);
    if (abfd && bfd_check_format(abfd, bfd_object)) {
        long ub = bfd_get_symtab_upper_bound(abfd);
        if (ub > 0) {
            t.syms = (asymbol**)malloc((size_t)ub);
            // A zero-symbol result (stripped binary) is as useless as an
            // error: fall through to the dynamic symtab below.
            if (t.syms && bfd_canonicalize_symtab(abfd, t.syms) <= 0) {
                free(t.syms);
                t.syms = nullptr;
            }
        }
        if (!t.syms) {
            // Stripped binary: dynamic symbols still give function names.
            long dub = bfd_get_dynamic_symtab_upper_bound(abfd);
            if (dub > 0) {
                t.syms = (asymbol**)malloc((size_t)dub);
                if (t.syms && bfd_canonicalize_dynamic_symtab(abfd, t.syms) < 0) {
                    free(t.syms);
                    t.syms = nullptr;
                }
            }
        }
        t.abfd = abfd;
        t.usable = t.syms != nullptr;
    } else if (abfd) {
        bfd_close(abfd);
    }
    return &tabs.emplace(path, t).first->second;
}

struct SymInfo {
    std::string func; // demangled; empty if unknown
    std::string file;
    unsigned line = 0;
    bool have = false;
};

// Symbolize a file vaddr into a frame chain, innermost first: the function
// whose code the address lands in (which may be an inlined
// DW_TAG_inlined_subroutine), followed by its inline callers outward to the
// real, physically-containing function. bfd_find_nearest_line returns the
// innermost inline frame; repeated bfd_find_inliner_info calls walk the
// DWARF chain outward (it reads per-abfd state seeded by the most recent
// find_nearest_line call, so the loop must run immediately after it, under
// the BFD lock). Empty when the address does not symbolicate.
std::vector<SymInfo> symbolize(SymTab* t, uint64_t file_vaddr) {
    std::vector<SymInfo> chain;
    if (!t || !t->usable)
        return chain;
    for (asection* sec = t->abfd->sections; sec; sec = sec->next) {
        if (!(bfd_section_flags(sec) & SEC_ALLOC))
            continue;
        bfd_vma vma = bfd_section_vma(sec);
        bfd_vma size = bfd_section_size(sec);
        if (file_vaddr < vma || file_vaddr >= vma + size)
            continue;
        const char* file = nullptr;
        const char* func = nullptr;
        unsigned line = 0;
        if (bfd_find_nearest_line(t->abfd, sec, t->syms,
                                  (bfd_vma)(file_vaddr - vma), &file, &func,
                                  &line)) {
            for (int depth = 0; depth < 64; depth++) {
                if (!func || !*func)
                    break;
                SymInfo info;
                int dem_st = 0;
                char* dem = (func[0] == '_')
                                ? abi::__cxa_demangle(func, nullptr, nullptr,
                                                      &dem_st)
                                : nullptr;
                info.func = dem ? dem : func;
                free(dem);
                if (file && *file)
                    info.file = file;
                info.line = line;
                info.have = true;
                chain.push_back(std::move(info));
                if (!bfd_find_inliner_info(t->abfd, &file, &func, &line))
                    break;
            }
        }
        break; // first containing section wins
    }
    return chain;
}

// An ELF object the guest code address belongs to (from the loader's map
// snapshot). Entries without an exact load bias are skipped: tag entries
// like "guest-mmap"/"guest-brk"/"guest-mremap" name no file, and file
// mappings with unknown bias would symbolicate at the wrong offset --
// symbolication needs an exact load bias (guest VA - ELF vaddr).
struct Module {
    bool found = false;
    std::string path;
    uint64_t bias = 0; // guest VA - ELF vaddr
};

Module find_module(const std::vector<Emulator::GuestMap>& maps, uint64_t addr) {
    for (const auto& m : maps) {
        if (!m.bias_valid || m.path.empty() ||
            !(m.start <= addr && addr < m.end))
            continue;
        if (access(m.path.c_str(), R_OK) != 0)
            continue;
        return Module{true, m.path, m.bias};
    }
    return Module{};
}

struct Frame {
    uint64_t addr;
    bool exact; // true: faulting rip or RBP-chain frame; false: scan guess
};

bool in_ranges(const std::vector<std::pair<uint64_t, uint64_t>>& rs,
               uint64_t a) {
    for (auto& r : rs)
        if (r.first <= a && a < r.second)
            return true;
    return false;
}

// True if the bytes ending exactly at `ret` look like a CALL targeting it:
// direct E8 rel32, or any FF /2 indirect-call encoding (optional REX
// prefix, ModRM reg field 2, SIB byte, disp8/disp32 including the
// mod==0 rm==5 RIP-relative GOT-indirect form). The direct-call target is
// checked for executability (cheap false-positive filter; a call whose
// target faults still leaves a valid return slot, so no other validation).
// Best effort: the window may start mid-instruction, so indirect forms are
// accepted only when the trailing bytes decode exactly as a complete call
// of the length their ModRM implies.
bool plausible_call_before(
    uint64_t ret,
    const std::vector<std::pair<uint64_t, uint64_t>>& exec_ranges) {
    uint8_t b[8];
    if (!mem_copy_guarded(ret - 8, b, 8))
        return false;
    // Direct call rel32: opcode at ret-5, exactly 5 bytes (no prefixes).
    if (b[3] == 0xE8) {
        int32_t rel = (int32_t)((uint32_t)b[4] | ((uint32_t)b[5] << 8) |
                                ((uint32_t)b[6] << 16) |
                                ((uint32_t)b[7] << 24));
        if (in_ranges(exec_ranges, ret + (uint64_t)(int64_t)rel))
            return true;
    }
    // Indirect FF /2 (call r/m64): match every trailing length the encoding
    // can have and require the ModRM to imply exactly that length.
    for (size_t len = 2; len <= 7; len++) {
        const uint8_t* w = b + (8 - len); // bytes [ret-len, ret)
        if (w[0] != 0xFF)
            continue;
        uint8_t modrm = w[1];
        if (((modrm >> 3) & 7) != 2)
            continue; // not /2 = CALL
        uint8_t mod = modrm >> 6, rm = modrm & 7;
        if (rm == 4 && mod == 3)
            continue; // FF /2 with SIB in mod==3 is not a real encoding
        if (rm == 4 && len < 3)
            continue; // SIB byte would lie beyond a 2-byte instruction
        size_t need = 2; // opcode + modrm
        if (rm == 4 && mod != 3) {
            need = 3; // + SIB
            if (mod == 1)
                need += 1;
            else if (mod == 2 || (mod == 0 && (w[2] & 7) == 5))
                need += 4;
        } else {
            if (mod == 1)
                need += 1;
            else if (mod == 2 || (mod == 0 && rm == 5))
                need += 4;
        }
        if (need != len)
            continue;
        // An optional REX prefix (0x40-0x4f) before the FF is fine either
        // way; the FF... portion alone already decodes as a full call.
        return true;
    }
    return false;
}

template<typename TaintT>
void collect_frames(CPU<TaintT>* cpu,
                    const std::vector<Emulator::GuestMap>& maps,
                    const std::vector<std::pair<uint64_t, uint64_t>>& exec_ranges,
                    const std::vector<std::pair<uint64_t, uint64_t>>& data_ranges,
                    std::vector<Frame>& out) {
    auto exec_addr = [&](uint64_t a) { return in_ranges(exec_ranges, a); };
    auto data_addr = [&](uint64_t a) { return in_ranges(data_ranges, a); };
    auto read_u64 = [&](uint64_t a, uint64_t* v) {
        return mem_copy_guarded(a, v, 8);
    };

    out.push_back(Frame{cpu->rip, true});

    // Primary: DWARF CFI unwinding (.eh_frame/.debug_frame), like gdb on
    // the same core. zeg_unwind_step refuses to run in passthrough mode
    // (its guarded reads would not be guarded there), but collect_frames is
    // not even called in that mode -- print_guest_stack_trace prints frame
    // 0 only.
    size_t cfi_frames = 0;
    {
        ZegUnwindState st;
        for (int i = 0; i < 16; i++)
            st.gpr[i] = cpu->gpr[i].val;
        st.rip = cpu->rip;
        bool ra_adj = false; // frame 0: rip is the faulting PC, not an RA
        while (out.size() < 256) {
            ZegUnwindState caller;
            if (!zeg_unwind_step(maps, st, caller, ra_adj))
                break;
            ra_adj = true;
            if (!caller.rip || caller.rip == st.rip ||
                !exec_addr(caller.rip))
                break;
            out.push_back(Frame{caller.rip, true});
            st = caller;
            cfi_frames++;
        }
    }

    // Legacy fallbacks, only when CFI produced nothing beyond frame 0 (a
    // guest without usable .eh_frame/.debug_frame).

    // Walk the RBP frame chain. Each frame: [rbp] = caller rbp,
    // [rbp+8] = return address.
    if (cfi_frames == 0) {
        uint64_t rbp = cpu->gpr[ZG_RBP].val;
        for (int i = 0; i < 256 && out.size() < 256; i++) {
            if (!rbp || (rbp & 7) || !data_addr(rbp))
                break;
            uint64_t next = 0, ret = 0;
            if (!read_u64(rbp, &next) || !read_u64(rbp + 8, &ret))
                break;
            if (!ret || !exec_addr(ret))
                break;
            out.push_back(Frame{ret, true});
            if (!next || next <= rbp || (next & 7) || !data_addr(next))
                break;
            rbp = next;
        }

        // Frame-pointer-less guests (-fomit-frame-pointer) break the chain:
        // scan the stack for slots holding plausible return addresses.
        if (out.size() <= 1) {
            uint64_t sp = cpu->gpr[ZG_RSP].val;
            if (sp && !(sp & 7) && data_addr(sp)) {
                size_t approx = 0;
                for (uint64_t a = sp;
                     a + 8 <= sp + (64ULL << 10) && out.size() < 256 &&
                     approx < 64;
                     a += 8) {
                    uint64_t v = 0;
                    if (!read_u64(a, &v))
                        break;
                    if (!exec_addr(v) ||
                        !plausible_call_before(v, exec_ranges))
                        continue;
                    out.push_back(Frame{v, false});
                    approx++;
                }
            }
        }
    }
}

} // namespace

// Templated on the taint sidecar type: the walk only reads non-taint state
// (rip, gpr values, emu, tid), so both instantiations behave identically.
template<typename TaintT>
void print_guest_stack_trace(CPU<TaintT>* cpu) {
    Emulator* emu = cpu->emu;

    fprintf(stderr, "Guest stack trace (tid=%d):\n", cpu->tid);

    // Test-only hook (ZEG_TEST_CRASH_IN_TRACE): inject a host fault inside
    // the dump path to verify the fault policy -- the process must die for
    // real instead of longjmp-ing back into the dump (which livelocks).
    if (getenv("ZEG_TEST_CRASH_IN_TRACE")) {
        volatile uintptr_t bad = 0x10; // volatile: keep -O2 from folding
                                       // the address (avoids -Warray-bounds)
        volatile int* p = (volatile int*)bad;
        *p = 1;
    }

    // Snapshot the guest memory map (frames are validated against it).
    std::vector<Emulator::GuestMap> maps;
    {
        std::lock_guard<std::mutex> l(emu->maps_mu);
        maps = emu->guest_maps;
    }
    std::vector<std::pair<uint64_t, uint64_t>> exec_ranges, data_ranges;
    for (const auto& m : maps) {
        if (m.end <= m.start)
            continue;
        if (m.prot & PROT_EXEC)
            exec_ranges.push_back({m.start, m.end});
        if ((m.prot & (PROT_READ | PROT_WRITE)) ==
            (PROT_READ | PROT_WRITE))
            data_ranges.push_back({m.start, m.end});
    }
    if (emu->guest_stack_size)
        data_ranges.push_back(
            {emu->guest_stack_base,
             emu->guest_stack_base + emu->guest_stack_size});

    std::vector<Frame> frames;
    if (mem_passthrough()) {
        // Passthrough mode installs no fault handler, so guarded guest
        // reads are not actually guarded: only the faulting frame, which
        // needs no guest-memory access.
        frames.push_back(Frame{cpu->rip, true});
    } else {
        collect_frames(cpu, maps, exec_ranges, data_ranges, frames);
    }

    // One gdb-style global frame counter: inline frames consume numbers
    // (each physical frame may print a whole chain, innermost first).
    size_t printed = 0;
    size_t fi = 0;
    for (; fi < frames.size() && printed < 256; fi++) {
        const Frame& f = frames[fi];
        // Frame 0 is the faulting RIP; every later frame is a RETURN
        // ADDRESS (CFI, RBP-chain, and scan frames alike), so it
        // symbolicates at address - 1 -- gdb's "address in block", which
        // lands on the line the call sits on instead of the line after
        // it. The printed address stays the raw f.addr, and the
        // module+offset fallback keeps the raw offset so it matches the
        // shown address.
        uint64_t sym_addr = fi ? f.addr - 1 : f.addr;
        Module mod = find_module(maps, f.addr);
        std::vector<SymInfo> chain;
        std::string modstr;
        if (mod.found) {
            std::lock_guard<std::mutex> l(g_bfd_mu);
            chain = symbolize(get_sym_tab(mod.path), sym_addr - mod.bias);
            if (chain.empty()) {
                const char* slash = strrchr(mod.path.c_str(), '/');
                modstr = (slash ? slash + 1 : mod.path.c_str());
                char off[32];
                snprintf(off, sizeof(off), "+0x%lx",
                         (unsigned long)(f.addr - mod.bias));
                modstr += off;
            }
        }
        auto print_line = [&](const SymInfo* info, bool inline_frame) {
            fprintf(stderr, "  #%-2zu 0x%016lx", printed,
                    (unsigned long)f.addr);
            if (info) {
                fprintf(stderr, " in %s ()", info->func.c_str());
                if (!info->file.empty()) {
                    if (info->line)
                        fprintf(stderr, " at %s:%u", info->file.c_str(),
                                info->line);
                    else
                        fprintf(stderr, " at %s", info->file.c_str());
                }
            } else if (!modstr.empty()) {
                fprintf(stderr, " in %s", modstr.c_str());
            }
            if (inline_frame && !f.exact)
                fprintf(stderr, "  (inline, approximate)");
            else if (inline_frame)
                fprintf(stderr, "  (inline)");
            else if (!f.exact)
                fprintf(stderr, "  (approximate)");
            fprintf(stderr, "\n");
            printed++;
        };
        if (chain.empty()) {
            print_line(nullptr, false);
        } else {
            for (size_t j = 0; j < chain.size() && printed < 256; j++)
                print_line(&chain[j], j + 1 < chain.size());
        }
    }
    if (frames.size() >= 256 || fi < frames.size())
        fprintf(stderr, "  ... (truncated at 256 frames)\n");
    fflush(stderr);
}

// Symbolize one recorded poison-history PC with the same BFD machinery as
// the stack trace: chain receives the PC's inline chain (innermost first,
// empty when nothing symbolicated) and modstr the "module+0xoff" fallback
// string (empty when the PC is in no known module at all). A step's frame
// 0 is the recorded instruction's own PC (return_addr=false); a step's
// further recorded frames are RETURN ADDRESSES and symbolicate at address
// - 1 like the stack trace's caller frames (return_addr=true) -- the
// module+offset fallback keeps the raw pc so the offset matches the shown
// address. No guest memory is read, so this is safe in every mode.
static void symbolize_hist_pc(const std::vector<Emulator::GuestMap>& maps,
                              uint64_t pc, bool return_addr,
                              std::vector<SymInfo>& chain,
                              std::string& modstr) {
    chain.clear();
    modstr.clear();
    Module mod = find_module(maps, pc);
    if (!mod.found)
        return;
    uint64_t sym_pc = return_addr ? pc - 1 : pc;
    std::lock_guard<std::mutex> l(g_bfd_mu);
    chain = symbolize(get_sym_tab(mod.path), sym_pc - mod.bias);
    if (chain.empty()) {
        const char* slash = strrchr(mod.path.c_str(), '/');
        modstr = (slash ? slash + 1 : mod.path.c_str());
        char off[32];
        snprintf(off, sizeof(off), "+0x%lx", (unsigned long)(pc - mod.bias));
        modstr += off;
    }
}

// Poison-history trail (--history mode): symbolize each recorded step with
// the same BFD machinery as the stack trace and print it oldest first. A
// step is one PC (--history=1) or a recorded guest stack, innermost PC
// first (--history=N/full). Steps are labeled "step N:" (1-based over the
// whole trail, oldest first) and the frames RECORDED in each step are
// numbered "#0", "#1", ... restarting at #0 within the step: the step's
// first frame line opens with the step label and each further recorded
// frame prints under it, its "#K" aligned under the step line's "#0".
// EVERY line of the block carries a PC. A frame line prints
// "#%-2zu 0x%016lx " followed by the location ("in <func> () at
// <file>:<line>", marked "  (inline)" when the frame's symbolized chain
// has more than one entry, i.e. the innermost symbol is an inlined
// instance); the rest of that chain prints as continuation lines below the
// frame line, each repeating the frame's own recorded PC, indented so its
// "0x" aligns under the frame line's "0x" (the fixed-width PC field also
// aligns the "in" columns), with "  (inline)" on all but the chain's last
// -- physically-containing -- entry. step[0] is the recorded instruction's
// own PC and symbolicates at it unchanged; a step's further recorded
// frames are return addresses and symbolicate at address - 1 like the
// stack trace's caller frames (see symbolize_hist_pc). A frame that does
// not symbolicate falls back to the module+offset string ("in
// <module+0xoff>", the recorded PC's own offset, on a frame line that
// still carries the PC); a frame in no known module prints just its bare
// PC prefix. Unlike the guest stack trace -- whose gdb-style numbering
// consumes one number per displayed (possibly inline) frame across the
// whole trace -- the history numbers the frames WITHIN each step under
// its "step N:" header, because its numbering unit is the recorded
// operation. Output is capped at the most recent 128 steps (kMaxPrint)
// with the omitted older count noted up front; the step numbers still
// reflect each printed step's position in the whole trail.
void print_poison_history(Emulator* emu,
                          const std::vector<std::vector<uint64_t>>& steps) {
    constexpr size_t kMaxPrint = 128;
    size_t omit = steps.size() > kMaxPrint ? steps.size() - kMaxPrint : 0;
    fprintf(stderr,
            "zegarmistrz: poison history of the offending value "
            "(%zu steps, oldest first):\n",
            steps.size());
    if (omit)
        fprintf(stderr, "  ... (%zu older entries omitted)\n", omit);

    // Snapshot the guest memory map once (symbolication needs each module's
    // path + load bias).
    std::vector<Emulator::GuestMap> maps;
    {
        std::lock_guard<std::mutex> l(emu->maps_mu);
        maps = emu->guest_maps;
    }

    // Shared "in <func> () at <file>:<line>" body for every frame line and
    // inline continuation line.
    auto print_loc = [](const SymInfo& info) {
        fprintf(stderr, "in %s ()", info.func.c_str());
        if (!info.file.empty()) {
            if (info.line)
                fprintf(stderr, " at %s:%u", info.file.c_str(), info.line);
            else
                fprintf(stderr, " at %s", info.file.c_str());
        }
    };

    // One recorded frame of the current step. lead prefixes the frame
    // number: "step N:  " for the step's first frame (k == 0), or the
    // blank indent that aligns the further frames' "#K" under the step
    // line's "#0". The frame line is lead + "#%-2zu 0x%016lx " + the
    // innermost location (or the module+offset fallback, or nothing at all
    // when the PC is in no known module -- like the old bare line, the PC
    // is all there is). The rest of the frame's inline chain prints as
    // continuation lines that repeat the frame's own recorded PC, indented
    // so their "0x" aligns under the frame line's "0x": snprintf computes
    // the frame prefix's width (plen) so the alignment is exact for any
    // lead and 1-3 digit frame numbers.
    auto print_frame = [&](const std::vector<uint64_t>& step, size_t k,
                           const char* lead) {
        uint64_t pc = step[k];
        char prefix[128];
        // "#%-2zu 0x%016lx ": the %-2zu pad space plus the literal one put
        // two spaces between a 1-digit "#K" and the PC and one between a
        // 2-digit "#K" and it, so the PC column is fixed-width either way
        // (same visual as the guest stack trace's frame lines).
        int plen = snprintf(prefix, sizeof(prefix),
                            "%s#%-2zu 0x%016lx ", lead, k,
                            (unsigned long)pc);
        std::vector<SymInfo> chain;
        std::string modstr;
        // k == 0 is the recorded instruction's own PC -- symbolize at it
        // unchanged; the further recorded frames are return addresses and
        // symbolicate at address - 1 (see symbolize_hist_pc).
        symbolize_hist_pc(maps, pc, k > 0, chain, modstr);
        if (chain.empty()) {
            if (!modstr.empty())
                fprintf(stderr, "%sin %s\n", prefix, modstr.c_str());
            else
                fprintf(stderr, "%s\n", prefix);
            return;
        }
        fprintf(stderr, "%s", prefix);
        print_loc(chain[0]);
        if (chain.size() > 1)
            fprintf(stderr, "  (inline)");
        fprintf(stderr, "\n");
        // Continuations carry the frame's own PC, indented so their "0x"
        // lands in the frame line's PC column (the prefix ends with the PC
        // field's separator space, hence the - 18 - 1).
        for (size_t j = 1; j < chain.size(); j++) {
            fprintf(stderr, "%*s0x%016lx ", plen - 18 - 1, "",
                    (unsigned long)pc);
            print_loc(chain[j]);
            if (j + 1 < chain.size())
                fprintf(stderr, "  (inline)");
            fprintf(stderr, "\n");
        }
    };

    for (size_t i = omit; i < steps.size(); i++) {
        const std::vector<uint64_t>& step = steps[i];
        // Defensive: an empty step (no recorder produces one) carries no
        // PC -- skip it rather than print a bare address.
        if (step.empty())
            continue;
        // Step label; the same width, as blanks, is the indent that puts
        // each further frame's "#K" under the step line's "#0".
        char lead[32];
        int llen = snprintf(lead, sizeof(lead), "step %zu:  ", i + 1);
        std::string findent((size_t)llen, ' ');
        for (size_t k = 0; k < step.size(); k++)
            print_frame(step, k, k ? findent.c_str() : lead);
    }
    fflush(stderr);
}

// Explicit instantiations: dump_core (core_dump.cpp) traces for any taint
// type.
template void print_guest_stack_trace<Taint>(CPU<Taint>*);
template void print_guest_stack_trace<HistoryTaint>(CPU<HistoryTaint>*);
template void print_guest_stack_trace<DeepHistoryTaint>(CPU<DeepHistoryTaint>*);
