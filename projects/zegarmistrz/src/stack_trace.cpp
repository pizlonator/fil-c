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

// Crash-time guest stack traces: walk the guest stack (RBP chain, with an
// approximate scan fallback for -fomit-frame-pointer code) and symbolize
// guest addresses with BFD against the ELF objects the emulator loaded.

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

SymInfo symbolize(SymTab* t, uint64_t file_vaddr) {
    SymInfo info;
    if (!t || !t->usable)
        return info;
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
                                  &line) &&
            func && *func) {
            info.have = true;
            int dem_st = 0;
            char* dem = (func[0] == '_')
                            ? abi::__cxa_demangle(func, nullptr, nullptr, &dem_st)
                            : nullptr;
            info.func = dem ? dem : func;
            free(dem);
            if (file && *file)
                info.file = file;
            info.line = line;
        }
        break; // first containing section wins
    }
    return info;
}

// An ELF object the guest code address belongs to (from the loader's map
// snapshot). Paths that are not real files (syscall tag strings like
// "guest-mmap") are skipped.
struct Module {
    bool found = false;
    std::string path;
    uint64_t bias = 0; // guest VA - ELF vaddr
};

Module find_module(const std::vector<Emulator::GuestMap>& maps, uint64_t addr) {
    for (const auto& m : maps) {
        if (m.path.empty() || !(m.start <= addr && addr < m.end))
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

// True if the bytes ending exactly at `ret` look like a CALL targeting it
// (direct E8 rel32, or a 2-byte FF /2 register form). Best effort: the
// window may start mid-instruction.
bool plausible_call_before(uint64_t ret) {
    uint8_t b[5];
    if (!mem_copy_guarded(ret - 5, b, 5))
        return false;
    if (b[0] == 0xE8)
        return true; // call rel32
    if (b[3] == 0xFF) {
        uint8_t m = b[4];
        if ((m & 0x38) == 0x10) { // FF /2 = call r/m64
            uint8_t mod = (m >> 6) & 3, rm = m & 7;
            if (mod != 0 || (rm != 4 && rm != 5)) // exclude SIB/disp forms
                return true;
        }
    }
    return false;
}

void collect_frames(CPU* cpu,
                    const std::vector<std::pair<uint64_t, uint64_t>>& exec_ranges,
                    const std::vector<std::pair<uint64_t, uint64_t>>& data_ranges,
                    std::vector<Frame>& out) {
    auto exec_addr = [&](uint64_t a) { return in_ranges(exec_ranges, a); };
    auto data_addr = [&](uint64_t a) { return in_ranges(data_ranges, a); };
    auto read_u64 = [&](uint64_t a, uint64_t* v) {
        return mem_copy_guarded(a, v, 8);
    };

    out.push_back(Frame{cpu->rip, true});

    // Walk the RBP frame chain. Each frame: [rbp] = caller rbp,
    // [rbp+8] = return address.
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
                if (!exec_addr(v) || !plausible_call_before(v))
                    continue;
                out.push_back(Frame{v, false});
                approx++;
            }
        }
    }
}

} // namespace

void print_guest_stack_trace(CPU* cpu) {
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
        collect_frames(cpu, exec_ranges, data_ranges, frames);
    }

    for (size_t i = 0; i < frames.size(); i++) {
        const Frame& f = frames[i];
        Module mod = find_module(maps, f.addr);
        SymInfo info;
        std::string modstr;
        if (mod.found) {
            std::lock_guard<std::mutex> l(g_bfd_mu);
            info = symbolize(get_sym_tab(mod.path), f.addr - mod.bias);
            if (!info.have) {
                const char* slash = strrchr(mod.path.c_str(), '/');
                modstr = (slash ? slash + 1 : mod.path.c_str());
                char off[32];
                snprintf(off, sizeof(off), "+0x%lx",
                         (unsigned long)(f.addr - mod.bias));
                modstr += off;
            }
        }
        fprintf(stderr, "  #%-2zu 0x%016lx", i, (unsigned long)f.addr);
        if (info.have) {
            fprintf(stderr, " in %s ()", info.func.c_str());
            if (!info.file.empty()) {
                if (info.line)
                    fprintf(stderr, " at %s:%u", info.file.c_str(), info.line);
                else
                    fprintf(stderr, " at %s", info.file.c_str());
            }
        } else if (!modstr.empty()) {
            fprintf(stderr, " in %s", modstr.c_str());
        }
        if (!f.exact)
            fprintf(stderr, "  (approximate)");
        fprintf(stderr, "\n");
    }
    if (frames.size() >= 256)
        fprintf(stderr, "  ... (truncated at 256 frames)\n");
    fflush(stderr);
}
