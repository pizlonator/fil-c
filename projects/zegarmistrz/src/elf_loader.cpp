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

#include "elf_loader.h"

#include <elf.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include "emulator.h"

static uint64_t page_align_down(uint64_t v) { return v & ~0xfffULL; }
static uint64_t page_align_up(uint64_t v) { return (v + 0xfffULL) & ~0xfffULL; }

static int prot_of(uint32_t flags) {
    int p = 0;
    if (flags & PF_R)
        p |= PROT_READ;
    if (flags & PF_W)
        p |= PROT_WRITE;
    if (flags & PF_X)
        p |= PROT_EXEC;
    return p;
}

struct ElfFile {
    int fd = -1;
    Elf64_Ehdr eh;
    std::vector<Elf64_Phdr> ph;
    std::string path;
    ~ElfFile() {
        if (fd >= 0)
            close(fd);
    }
};

static ElfFile open_elf(const std::string& path) {
    ElfFile f;
    f.path = path;
    f.fd = open(path.c_str(), O_RDONLY);
    if (f.fd < 0)
        throw std::string("cannot open ELF: ") + path;
    if (read(f.fd, &f.eh, sizeof(f.eh)) != (ssize_t)sizeof(f.eh))
        throw std::string("cannot read EHDR: ") + path;
    if (memcmp(f.eh.e_ident, "\177ELF", 4) != 0)
        throw std::string("not an ELF: ") + path;
    if (f.eh.e_ident[EI_CLASS] != ELFCLASS64)
        throw std::string("not ELF64: ") + path;
    if (f.eh.e_machine != EM_X86_64)
        throw std::string("not x86-64: ") + path;
    f.ph.resize(f.eh.e_phnum);
    if (pread(f.fd, f.ph.data(), f.ph.size() * sizeof(Elf64_Phdr), f.eh.e_phoff) !=
        (ssize_t)(f.ph.size() * sizeof(Elf64_Phdr)))
        throw std::string("cannot read PHDRs: ") + path;
    return f;
}

std::string elf_read_interp(const std::string& path) {
    ElfFile f = open_elf(path);
    for (auto& p : f.ph) {
        if (p.p_type == PT_INTERP) {
            std::vector<char> buf(p.p_filesz + 1, 0);
            if (pread(f.fd, buf.data(), p.p_filesz, p.p_offset) !=
                (ssize_t)p.p_filesz)
                throw std::string("cannot read INTERP: ") + path;
            return std::string(buf.data());
        }
    }
    return "";
}

LoadedObj elf_load_object(Emulator* emu, const std::string& path,
                         uint64_t reserve_extra) {
    ElfFile f = open_elf(path);

    // Compute total span of PT_LOADs.
    uint64_t lo = UINT64_MAX, hi = 0;
    for (auto& p : f.ph) {
        if (p.p_type != PT_LOAD)
            continue;
        uint64_t s = page_align_down(p.p_vaddr);
        uint64_t e = page_align_up(p.p_vaddr + p.p_memsz);
        if (s < lo)
            lo = s;
        if (e > hi)
            hi = e;
    }
    if (hi <= lo)
        throw std::string("no PT_LOAD in ") + path;

    uint64_t bias = 0;
    bool is_dyn = (f.eh.e_type == ET_DYN);
    if (is_dyn) {
        // Reserve a free region to pick a non-conflicting base. For the
        // main exe, reserve_extra leaves contiguous room for brk growth.
        uint64_t total = (hi - lo) + reserve_extra;
        void* r = mmap(nullptr, total, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (r == MAP_FAILED)
            throw std::string("mmap reservation failed for ") + path;
        bias = (uint64_t)(uintptr_t)r - lo;
        munmap(r, total);
    } else if (lo != 0) {
        // ET_EXEC with nonzero base: map where asked (may clobber; hello
        // corpus is PIE so this path is unlikely).
        bias = 0;
    }

    for (auto& p : f.ph) {
        if (p.p_type != PT_LOAD)
            continue;
        uint64_t mstart = page_align_down(bias + p.p_vaddr);
        uint64_t mend = page_align_up(bias + p.p_vaddr + p.p_memsz);
        uint64_t mlen = mend - mstart;
        int prot = prot_of(p.p_flags);
        void* m = mmap((void*)(uintptr_t)mstart, mlen, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        if (m == MAP_FAILED)
            throw std::string("mmap PT_LOAD failed for ") + path;
        // Copy file contents.
        if (p.p_filesz) {
            if (pread(f.fd, (void*)(uintptr_t)(bias + p.p_vaddr), p.p_filesz,
                      p.p_offset) != (ssize_t)p.p_filesz)
                throw std::string("pread segment failed for ") + path;
        }
        // BSS tail is already zero (anonymous). Enforce final permissions.
        if (mprotect((void*)(uintptr_t)mstart, mlen, prot ? prot : PROT_NONE) != 0)
            throw std::string("mprotect segment failed for ") + path;
        // Record for core dumps (guest VA == host VA).
        if (emu) {
            Emulator::GuestMap gm;
            gm.start = mstart;
            gm.end = mend;
            gm.prot = prot;
            gm.path = path;
            std::lock_guard<std::mutex> l(emu->maps_mu);
            emu->guest_maps.push_back(gm);
        }
    }

    // Translate e_phoff (a file offset) to the mapped virtual address by
    // finding the PT_LOAD that contains it. (For PIE with lo==0 this is
    // bias+e_phoff, but for ET_EXEC the file offset and vaddr differ.)
    uint64_t phdr_vaddr = 0;
    bool have_phdr = false;
    for (auto& p : f.ph) {
        if (p.p_type != PT_LOAD)
            continue;
        if (f.eh.e_phoff >= p.p_offset &&
            f.eh.e_phoff < p.p_offset + p.p_filesz) {
            phdr_vaddr = bias + p.p_vaddr + (f.eh.e_phoff - p.p_offset);
            have_phdr = true;
            break;
        }
    }
    if (!have_phdr)
        phdr_vaddr = bias + f.eh.e_phoff; // fallback (usually identical)
    LoadedObj o;
    o.reserve_end = is_dyn ? bias + lo + ((hi - lo) + reserve_extra) : 0;
    o.base = bias;
    o.entry = bias + f.eh.e_entry;
    o.phdr = phdr_vaddr;
    o.phent = f.eh.e_phentsize;
    o.phnum = f.eh.e_phnum;
    o.map_end = page_align_up(bias + hi);
    return o;
}

uint64_t elf_load_program(Emulator* emu, const std::string& path) {
    std::string interp = elf_read_interp(path);
    // Main exe gets 1GB of contiguous brk growth room in the same
    // reservation (see do_brk).
    LoadedObj exe = elf_load_object(emu, path, 1ULL << 30);
    emu->brk_limit = exe.reserve_end;
    emu->exe_base = exe.base;
    emu->exe_entry = exe.entry;
    emu->exe_phdr = exe.phdr;
    emu->exe_phent = exe.phent;
    emu->exe_phnum = exe.phnum;

    uint64_t brk_init = exe.map_end;

    if (!interp.empty()) {
        LoadedObj ld = elf_load_object(emu, interp);
        emu->interp_base = ld.base;
        emu->interp_path = interp;
        // brk starts after the main exe (kernel semantics), not the loader.
        emu->guest_brk = brk_init;
        return ld.entry;
    }
    emu->interp_base = 0;
    emu->guest_brk = brk_init;
    return exe.entry;
}
