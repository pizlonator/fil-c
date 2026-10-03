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
#pragma once

#include <cstdint>
#include <string>

struct Emulator;

// Loaded object info.
struct LoadedObj {
    uint64_t base = 0; // load bias (0 for ET_EXEC)
    uint64_t entry = 0; // absolute entry address
    uint64_t phdr = 0; // absolute address of program headers in guest memory
    uint64_t phent = 0;
    uint64_t phnum = 0;
    uint64_t map_end = 0; // end of last PT_LOAD (for brk init)
    uint64_t reserve_end = 0; // end of the address reservation (brk limit)
};

// Map one ELF file's PT_LOAD segments at guest addresses (== host addresses
// via mmap MAP_FIXED). For ET_DYN a free base is chosen by reservation.
// Returns load info. Throws std::string on error.
LoadedObj elf_load_object(Emulator* emu, const std::string& path,
                         uint64_t reserve_extra = 0);

// Read the PT_INTERP string of an ELF file; empty if none.
std::string elf_read_interp(const std::string& path);

// Load program + interpreter (if any). Fills emu exe/interp fields.
// Returns the entry RIP to start execution at.
uint64_t elf_load_program(Emulator* emu, const std::string& path);
