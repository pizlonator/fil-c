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
#include <cstring>

#include "mem.h"

// GPR index convention: 0=RAX 1=RCX 2=RDX 3=RBX 4=RSP 5=RBP 6=RSI 7=RDI
//                       8..15 = R8..R15
enum : int {
    ZG_RAX = 0, ZG_RCX = 1, ZG_RDX = 2, ZG_RBX = 3,
    ZG_RSP = 4, ZG_RBP = 5, ZG_RSI = 6, ZG_RDI = 7,
    ZG_R8 = 8, ZG_R9 = 9, ZG_R10 = 10, ZG_R11 = 11,
    ZG_R12 = 12, ZG_R13 = 13, ZG_R14 = 14, ZG_R15 = 15,
};

// RFLAGS bits we model.
enum : uint64_t {
    FLAG_CF = 1ULL << 0,
    FLAG_PF = 1ULL << 2,
    FLAG_AF = 1ULL << 4,
    FLAG_ZF = 1ULL << 6,
    FLAG_SF = 1ULL << 7,
    FLAG_DF = 1ULL << 10,
    FLAG_OF = 1ULL << 11,
};

// A general-purpose register value with Fil-C-style sidecar taint.
struct GPRValue {
    uint64_t val = 0;
    Taint taint;
};

// A vector register value: MAX_VEC_BYTES bytes of storage (ZMM width) plus taint.
struct XMMValue {
    uint8_t bytes[MAX_VEC_BYTES];
    Taint taint;
    XMMValue() { memset(bytes, 0, sizeof(bytes)); }
};

// General vector temporary: bytes + taint. Sized by MAX_VEC_BYTES so wider
// vectors only require bumping the constant. Provides byte access plus
// helpers to keep decode_exec free of raw [MAX_VEC_BYTES] arrays.
struct VecVal {
    uint8_t bytes[MAX_VEC_BYTES];
    Taint taint;
    VecVal() { memset(bytes, 0, sizeof(bytes)); }
    uint8_t& operator[](size_t i) { return bytes[i]; }
    const uint8_t& operator[](size_t i) const { return bytes[i]; }
    uint8_t* data() { return bytes; }
    const uint8_t* data() const { return bytes; }
    // Implicit decay to byte pointer so legacy memcpy/memset/pointer-arith
    // call sites keep working after uint8_t[N] -> VecVal migration. The
    // taint sidecar must still be handled explicitly via .taint.
    operator uint8_t*() { return bytes; }
    operator const uint8_t*() const { return bytes; }
    void clear() {
        memset(bytes, 0, sizeof(bytes));
        taint.clear();
    }
};

struct Emulator; // forward

// Per-thread guest CPU state.
struct CPU {
    GPRValue gpr[16];
    uint64_t rip = 0;
    uint64_t rflags = 0x2; // bit 1 always set
    XMMValue xmm[32];
    uint32_t mxcsr = 0x1f80;

    uint64_t fs_base = 0;
    uint64_t gs_base = 0;

    // x87 state (minimal): 8x 80-bit regs emulated as long double.
    long double st[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int fpu_top = 0;
    uint16_t fcw = 0x037f;
    uint16_t fsw = 0;

    // AVX512 opmask registers k0..k7. Only the low N bits are used where N

    // is the lane count of the masked operation. k0 is special: when used

    // as a write mask it means "no masking".

    uint64_t opmask[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    // Taint of the RFLAGS condition state: set when an ALU op that writes

    // flags consumes a tainted input. Checked on Jcc/JCXZ/LOOP.

    Taint flags_taint;

    // Thread bookkeeping for clone emulation.
    int tid = 0;                 // guest tid (== host gettid())
    uint64_t clear_ctid_addr = 0; // CLONE_CHILD_CLEARTID address to clear+wake on exit
    uint64_t robust_list = 0;
    uint64_t rseq_addr = 0;

    bool exited = false;

    Emulator* emu = nullptr;

    uint64_t rsp() const { return gpr[ZG_RSP].val; }
    void set_rsp(uint64_t v, Taint t = Taint()) {
        gpr[ZG_RSP].val = v;
        gpr[ZG_RSP].taint = t;
    }
    bool df() const { return (rflags & FLAG_DF) != 0; }
    bool cf() const { return (rflags & FLAG_CF) != 0; }
    bool pf() const { return (rflags & FLAG_PF) != 0; }
    bool af() const { return (rflags & FLAG_AF) != 0; }
    bool zf() const { return (rflags & FLAG_ZF) != 0; }
    bool sf() const { return (rflags & FLAG_SF) != 0; }
    bool of() const { return (rflags & FLAG_OF) != 0; }
    void set_flag(uint64_t bit, bool v) {
        if (v) rflags |= bit;
        else rflags &= ~bit;
    }
};
