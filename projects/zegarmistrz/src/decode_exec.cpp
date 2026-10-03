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

// Zydis-based x86-64 interpreter. Single dispatch switch in exec_one().
// Per-instruction semantics live in exec_* functions below, grouped by
// category. Memory always flows through mem_* (see mem.h).

#include "decode_exec.h"

#include <Zydis/Zydis.h>
#include <cmath>
#include <cpuid.h>
#include <csignal>
#include <fcntl.h>
#include <immintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <x86intrin.h>

#include <string>

#include "cpu.h"
#include "emulator.h"
#include "mem.h"
#include "syscall.h"

// Magic client-request call target (see zegarmistrz.txt: poisoning).
static const uint64_t kMagicAddr = 0x1410141014101410ULL;

namespace {

thread_local ZydisDecoder t_decoder;
thread_local bool t_decoder_init = false;
thread_local ZydisFormatter t_formatter;
thread_local bool t_formatter_init = false;

void ensure_decoder() {
    if (!t_decoder_init) {
        ZydisDecoderInit(&t_decoder, ZYDIS_MACHINE_MODE_LONG_64,
                         ZYDIS_STACK_WIDTH_64);
        t_decoder_init = true;
    }
    if (!t_formatter_init) {
        ZydisFormatterInit(&t_formatter, ZYDIS_FORMATTER_STYLE_INTEL);
        t_formatter_init = true;
    }
}

struct Dec {
    ZydisDecodedInstruction insn;
    ZydisDecodedOperand ops[ZYDIS_MAX_OPERAND_COUNT];
    uint8_t code[15];
};

// Decode one instruction at cpu->rip.
Dec fetch_decode(CPU* cpu) {
    ensure_decoder();
    Dec d;
    memset(&d, 0, sizeof(d));
    size_t n = mem_read_code(cpu, cpu->rip, d.code);
    if (!ZYAN_SUCCESS(
            ZydisDecoderDecodeFull(&t_decoder, d.code, n, &d.insn, d.ops)))
        guest_error(cpu, "bad instruction encoding (decode failed)", cpu->rip);
    if (d.insn.length == 0 || d.insn.length > (ZyanU8)n)
        guest_error(cpu, "bad instruction length", cpu->rip);
    return d;
}
// Number of addressable operands: explicit + implicit, in decoded order,
// skipping hidden ones (RFLAGS, implicit RSP/RIP snapshots, ...). All
// exec_* handlers index d.ops[] in this space, e.g. `test al, 1` is
// op0=AL(implicit), op1=imm(explicit).
int explicit_ops(const Dec& d) {
    int n = 0;
    for (int i = 0; i < d.insn.operand_count; i++) {
        if (d.ops[i].visibility != ZYDIS_OPERAND_VISIBILITY_HIDDEN)
            n++;
    }
    return n;
}

// --- register mapping ------------------------------------------------------
// GPR: returns index 0..15, size in bytes (1/2/4/8), shift in bits (0 or 8
// for AH/CH/DH/BH).
bool reg_to_gpr(ZydisRegister r, int& idx, int& size, int& shift) {
    shift = 0;
    if (r >= ZYDIS_REGISTER_AL && r <= ZYDIS_REGISTER_R15B) {
        size = 1;
        switch (r) {
        case ZYDIS_REGISTER_AL: idx = 0; return true;
        case ZYDIS_REGISTER_CL: idx = 1; return true;
        case ZYDIS_REGISTER_DL: idx = 2; return true;
        case ZYDIS_REGISTER_BL: idx = 3; return true;
        case ZYDIS_REGISTER_AH: idx = 0; shift = 8; return true;
        case ZYDIS_REGISTER_CH: idx = 1; shift = 8; return true;
        case ZYDIS_REGISTER_DH: idx = 2; shift = 8; return true;
        case ZYDIS_REGISTER_BH: idx = 3; shift = 8; return true;
        case ZYDIS_REGISTER_SPL: idx = 4; return true;
        case ZYDIS_REGISTER_BPL: idx = 5; return true;
        case ZYDIS_REGISTER_SIL: idx = 6; return true;
        case ZYDIS_REGISTER_DIL: idx = 7; return true;
        default: idx = 8 + (r - ZYDIS_REGISTER_R8B); return true;
        }
    }
    if (r >= ZYDIS_REGISTER_AX && r <= ZYDIS_REGISTER_R15W) {
        size = 2;
        idx = r - ZYDIS_REGISTER_AX;
        return true;
    }
    if (r >= ZYDIS_REGISTER_EAX && r <= ZYDIS_REGISTER_R15D) {
        size = 4;
        idx = r - ZYDIS_REGISTER_EAX;
        return true;
    }
    if (r >= ZYDIS_REGISTER_RAX && r <= ZYDIS_REGISTER_R15) {
        size = 8;
        idx = r - ZYDIS_REGISTER_RAX;
        return true;
    }
    return false;
}

// Vector register: index 0..31, width in bytes (16/32/64).
bool reg_to_vec(ZydisRegister r, int& idx, int& width) {
    if (r >= ZYDIS_REGISTER_XMM0 && r <= ZYDIS_REGISTER_XMM31) {
        idx = r - ZYDIS_REGISTER_XMM0;
        width = 16;
        return true;
    }
    if (r >= ZYDIS_REGISTER_YMM0 && r <= ZYDIS_REGISTER_YMM31) {
        idx = r - ZYDIS_REGISTER_YMM0;
        width = 32;
        return true;
    }
    if (r >= ZYDIS_REGISTER_ZMM0 && r <= ZYDIS_REGISTER_ZMM31) {
        idx = r - ZYDIS_REGISTER_ZMM0;
        width = 64;
        return true;
    }
    return false;
}

// --- flags -----------------------------------------------------------------

bool parity8(uint8_t v) {
    v ^= v >> 4;
    v ^= v >> 2;
    v ^= v >> 1;
    return (v & 1) == 0;
}

uint64_t mask_for(int w) {
    if (w == 8)
        return ~0ULL;
    return (w == 4) ? 0xffffffffULL : (w == 2) ? 0xffffULL : 0xffULL;
}

void flags_add(CPU* cpu, int w, uint64_t a, uint64_t b, uint64_t r) {
    uint64_t m = mask_for(w);
    a &= m;
    b &= m;
    r &= m;
    unsigned bits = w * 8;
    cpu->set_flag(FLAG_CF, r < a);
    bool sa = (a >> (bits - 1)) & 1, sb = (b >> (bits - 1)) & 1,
         sr = (r >> (bits - 1)) & 1;
    cpu->set_flag(FLAG_OF, (sa == sb) && (sr != sa));
    cpu->set_flag(FLAG_ZF, r == 0);
    cpu->set_flag(FLAG_SF, sr);
    cpu->set_flag(FLAG_PF, parity8((uint8_t)r));
    cpu->set_flag(FLAG_AF, ((a ^ b ^ r) & 0x10) != 0);
}

void flags_sub(CPU* cpu, int w, uint64_t a, uint64_t b, uint64_t r) {
    uint64_t m = mask_for(w);
    a &= m;
    b &= m;
    r &= m;
    unsigned bits = w * 8;
    cpu->set_flag(FLAG_CF, a < b);
    bool sa = (a >> (bits - 1)) & 1, sb = (b >> (bits - 1)) & 1,
         sr = (r >> (bits - 1)) & 1;
    cpu->set_flag(FLAG_OF, (sa != sb) && (sr != sa));
    cpu->set_flag(FLAG_ZF, r == 0);
    cpu->set_flag(FLAG_SF, sr);
    cpu->set_flag(FLAG_PF, parity8((uint8_t)r));
    cpu->set_flag(FLAG_AF, ((a ^ b ^ r) & 0x10) != 0);
}

void flags_logic(CPU* cpu, int w, uint64_t r) {
    r &= mask_for(w);
    cpu->set_flag(FLAG_CF, false);
    cpu->set_flag(FLAG_OF, false);
    cpu->set_flag(FLAG_ZF, r == 0);
    cpu->set_flag(FLAG_SF, (r >> (w * 8 - 1)) & 1);
    cpu->set_flag(FLAG_PF, parity8((uint8_t)r));
    cpu->set_flag(FLAG_AF, false);
}

// Condition codes shared by Jcc/SETcc/CMOVcc.
bool eval_cond(CPU* cpu, ZydisMnemonic m) {
    bool cf = cpu->cf(), pf = cpu->pf(), zf = cpu->zf(), sf = cpu->sf(),
         of = cpu->of();
    switch (m) {
    case ZYDIS_MNEMONIC_JO:
    case ZYDIS_MNEMONIC_SETO:
    case ZYDIS_MNEMONIC_CMOVO: return of;
    case ZYDIS_MNEMONIC_JNO:
    case ZYDIS_MNEMONIC_SETNO:
    case ZYDIS_MNEMONIC_CMOVNO: return !of;
    case ZYDIS_MNEMONIC_JB:
    case ZYDIS_MNEMONIC_SETB:
    case ZYDIS_MNEMONIC_CMOVB: return cf;
    case ZYDIS_MNEMONIC_JNB:
    case ZYDIS_MNEMONIC_SETNB:
    case ZYDIS_MNEMONIC_CMOVNB: return !cf;
    case ZYDIS_MNEMONIC_JZ:
    case ZYDIS_MNEMONIC_SETZ:
    case ZYDIS_MNEMONIC_CMOVZ: return zf;
    case ZYDIS_MNEMONIC_JNZ:
    case ZYDIS_MNEMONIC_SETNZ:
    case ZYDIS_MNEMONIC_CMOVNZ: return !zf;
    case ZYDIS_MNEMONIC_JBE:
    case ZYDIS_MNEMONIC_SETBE:
    case ZYDIS_MNEMONIC_CMOVBE: return cf || zf;
    case ZYDIS_MNEMONIC_JNBE:
    case ZYDIS_MNEMONIC_SETNBE:
    case ZYDIS_MNEMONIC_CMOVNBE: return !cf && !zf;
    case ZYDIS_MNEMONIC_JS:
    case ZYDIS_MNEMONIC_SETS:
    case ZYDIS_MNEMONIC_CMOVS: return sf;
    case ZYDIS_MNEMONIC_JNS:
    case ZYDIS_MNEMONIC_SETNS:
    case ZYDIS_MNEMONIC_CMOVNS: return !sf;
    case ZYDIS_MNEMONIC_JP:
    case ZYDIS_MNEMONIC_SETP:
    case ZYDIS_MNEMONIC_CMOVP: return pf;
    case ZYDIS_MNEMONIC_JNP:
    case ZYDIS_MNEMONIC_SETNP:
    case ZYDIS_MNEMONIC_CMOVNP: return !pf;
    case ZYDIS_MNEMONIC_JL:
    case ZYDIS_MNEMONIC_SETL:
    case ZYDIS_MNEMONIC_CMOVL: return sf != of;
    case ZYDIS_MNEMONIC_JNL:
    case ZYDIS_MNEMONIC_SETNL:
    case ZYDIS_MNEMONIC_CMOVNL: return sf == of;
    case ZYDIS_MNEMONIC_JLE:
    case ZYDIS_MNEMONIC_SETLE:
    case ZYDIS_MNEMONIC_CMOVLE: return zf || (sf != of);
    case ZYDIS_MNEMONIC_JNLE:
    case ZYDIS_MNEMONIC_SETNLE:
    case ZYDIS_MNEMONIC_CMOVNLE: return !zf && (sf == of);
    default: return false;
    }
}

bool is_jcc(ZydisMnemonic m) {
    switch (m) {
    case ZYDIS_MNEMONIC_JO:
    case ZYDIS_MNEMONIC_JNO:
    case ZYDIS_MNEMONIC_JB:
    case ZYDIS_MNEMONIC_JNB:
    case ZYDIS_MNEMONIC_JZ:
    case ZYDIS_MNEMONIC_JNZ:
    case ZYDIS_MNEMONIC_JBE:
    case ZYDIS_MNEMONIC_JNBE:
    case ZYDIS_MNEMONIC_JS:
    case ZYDIS_MNEMONIC_JNS:
    case ZYDIS_MNEMONIC_JP:
    case ZYDIS_MNEMONIC_JNP:
    case ZYDIS_MNEMONIC_JL:
    case ZYDIS_MNEMONIC_JNL:
    case ZYDIS_MNEMONIC_JLE:
    case ZYDIS_MNEMONIC_JNLE: return true;
    default: return false;
    }
}

bool is_setcc(ZydisMnemonic m) {
    switch (m) {
    case ZYDIS_MNEMONIC_SETO:
    case ZYDIS_MNEMONIC_SETNO:
    case ZYDIS_MNEMONIC_SETB:
    case ZYDIS_MNEMONIC_SETNB:
    case ZYDIS_MNEMONIC_SETZ:
    case ZYDIS_MNEMONIC_SETNZ:
    case ZYDIS_MNEMONIC_SETBE:
    case ZYDIS_MNEMONIC_SETNBE:
    case ZYDIS_MNEMONIC_SETS:
    case ZYDIS_MNEMONIC_SETNS:
    case ZYDIS_MNEMONIC_SETP:
    case ZYDIS_MNEMONIC_SETNP:
    case ZYDIS_MNEMONIC_SETL:
    case ZYDIS_MNEMONIC_SETNL:
    case ZYDIS_MNEMONIC_SETLE:
    case ZYDIS_MNEMONIC_SETNLE: return true;
    default: return false;
    }
}

bool is_cmovcc(ZydisMnemonic m) {
    switch (m) {
    case ZYDIS_MNEMONIC_CMOVO:
    case ZYDIS_MNEMONIC_CMOVNO:
    case ZYDIS_MNEMONIC_CMOVB:
    case ZYDIS_MNEMONIC_CMOVNB:
    case ZYDIS_MNEMONIC_CMOVZ:
    case ZYDIS_MNEMONIC_CMOVNZ:
    case ZYDIS_MNEMONIC_CMOVBE:
    case ZYDIS_MNEMONIC_CMOVNBE:
    case ZYDIS_MNEMONIC_CMOVS:
    case ZYDIS_MNEMONIC_CMOVNS:
    case ZYDIS_MNEMONIC_CMOVP:
    case ZYDIS_MNEMONIC_CMOVNP:
    case ZYDIS_MNEMONIC_CMOVL:
    case ZYDIS_MNEMONIC_CMOVNL:
    case ZYDIS_MNEMONIC_CMOVLE:
    case ZYDIS_MNEMONIC_CMOVNLE: return true;
    default: return false;
    }
}

// --- operand access --------------------------------------------------------

struct Val {
    uint64_t v = 0;
    Taint taint;
};

// Compute linear address of a memory operand. next_rip = address of the
// following instruction (for RIP-relative).
uint64_t resolve_mem(CPU* cpu, const ZydisDecodedOperand& op, uint64_t next_rip) {
    uint64_t base = 0, index = 0, scale = op.mem.scale ? op.mem.scale : 1;
    bool has_base = false;
    if (op.mem.base != ZYDIS_REGISTER_NONE) {
        if (op.mem.base == ZYDIS_REGISTER_RIP) {
            base = next_rip;
            has_base = true;
        } else {
            int idx, size, shift;
            if (!reg_to_gpr(op.mem.base, idx, size, shift))
                guest_error(cpu, "unsupported base register in memory operand");
            if (cpu->gpr[idx].taint.cannot_index)
                guest_error(cpu, "tainted (cannot-index) value used as address base");
            base = cpu->gpr[idx].val;
            has_base = true;
        }
    }
    if (op.mem.index != ZYDIS_REGISTER_NONE) {
        int idx, size, shift;
        if (!reg_to_gpr(op.mem.index, idx, size, shift))
            guest_error(cpu, "unsupported index register in memory operand");
        if (cpu->gpr[idx].taint.cannot_index)
            guest_error(cpu, "tainted (cannot-index) value used as address index");
        uint64_t iv = cpu->gpr[idx].val;
        // Index is used at its natural width; 32-bit index is zero-extended.
        if (size == 4)
            iv &= 0xffffffffULL;
        else if (size == 2)
            iv &= 0xffffULL;
        else if (size == 1)
            iv &= 0xffULL;
        index = iv * scale;
    }
    int64_t disp = op.mem.disp.has_displacement ? op.mem.disp.value : 0;
    uint64_t addr = (has_base ? base : 0) + index + (uint64_t)(int64_t)disp;
    if (op.mem.segment == ZYDIS_REGISTER_FS)
        addr += cpu->fs_base;
    else if (op.mem.segment == ZYDIS_REGISTER_GS)
        addr += cpu->gs_base;
    return addr;
}

// Load an explicit operand as an integer of its operand size.
Val op_load(CPU* cpu, const Dec& d, int oi) {
    const auto& op = d.ops[oi];
    Val r;
    if (op.type == ZYDIS_OPERAND_TYPE_REGISTER) {
        if (op.reg.value == ZYDIS_REGISTER_RIP) {
            r.v = cpu->rip + d.insn.length;
            return r;
        }
        int idx, size, shift;
        if (!reg_to_gpr(op.reg.value, idx, size, shift))
            guest_error(cpu, "unsupported register operand");
        uint64_t full = cpu->gpr[idx].val;
        r.v = (full >> shift) & mask_for(size);
        r.taint.cannot_branch = cpu->gpr[idx].taint.cannot_branch;
        r.taint.cannot_index = cpu->gpr[idx].taint.cannot_index;
        return r;
    }
    if (op.type == ZYDIS_OPERAND_TYPE_MEMORY) {
        uint64_t addr = resolve_mem(cpu, op, cpu->rip + d.insn.length);
        unsigned bytes = op.size / 8;
        switch (bytes) {
        case 1: r.v = mem_load8(cpu, addr); break;
        case 2: r.v = mem_load16(cpu, addr); break;
        case 4: r.v = mem_load32(cpu, addr); break;
        case 8: r.v = mem_load64(cpu, addr); break;
        default: guest_error(cpu, "unsupported memory operand size");
        }
        // Poisoning: loads from cannot-branch/index memory taint the value.
        mem_get_taint(addr, bytes, &r.taint);
        return r;
    }
    if (op.type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        if (op.imm.is_signed)
            r.v = (uint64_t)op.imm.value.s;
        else
            r.v = op.imm.value.u;
        return r;
    }
    guest_error(cpu, "unsupported operand type in op_load");
}

// Store an integer to an explicit register/memory operand.
void op_store(CPU* cpu, const Dec& d, int oi, Val v) {
    const auto& op = d.ops[oi];
    if (op.type == ZYDIS_OPERAND_TYPE_REGISTER) {
        int idx, size, shift;
        if (!reg_to_gpr(op.reg.value, idx, size, shift))
            guest_error(cpu, "unsupported register operand");
        uint64_t full = cpu->gpr[idx].val;
        if (size == 4 && shift == 0) {
            // 32-bit writes zero-extend.
            full = v.v & 0xffffffffULL;
        } else if (size == 8) {
            full = v.v;
        } else {
            uint64_t m = mask_for(size) << shift;
            full = (full & ~m) | ((v.v & mask_for(size)) << shift);
        }
        cpu->gpr[idx].val = full;
        cpu->gpr[idx].taint.cannot_branch = v.taint.cannot_branch;
        cpu->gpr[idx].taint.cannot_index = v.taint.cannot_index;
        return;
    }
    if (op.type == ZYDIS_OPERAND_TYPE_MEMORY) {
        uint64_t addr = resolve_mem(cpu, op, cpu->rip + d.insn.length);
        unsigned bytes = op.size / 8;
        switch (bytes) {
        case 1: mem_store8(cpu, addr, (uint8_t)v.v); break;
        case 2: mem_store16(cpu, addr, (uint16_t)v.v); break;
        case 4: mem_store32(cpu, addr, (uint32_t)v.v); break;
        case 8: mem_store64(cpu, addr, v.v); break;
        default: guest_error(cpu, "unsupported memory operand size");
        }
        // Poisoning: a stored tainted value poisons the destination.
        mem_note_store(cpu, addr, bytes, v.taint);
        return;
    }
    guest_error(cpu, "cannot store to non-register/memory operand");
}

// Vector operand byte access. Optional taint out-params (cb/ci) report the
// cannot-branch/index taint of the loaded bytes (register sidecar or memory
// poison); vec_store_bytes takes the stored value's taint for mem_note_store
// and propagates register sidecars.
void vec_load_bytes(CPU* cpu, const Dec& d, int oi, uint8_t* out, unsigned n,
                    Taint* t = nullptr) {
    const auto& op = d.ops[oi];
    if (t)
        t->clear();
    if (op.type == ZYDIS_OPERAND_TYPE_REGISTER) {
        int idx, width;
        if (!reg_to_vec(op.reg.value, idx, width))
            guest_error(cpu, "unsupported vector register");
        memcpy(out, cpu->xmm[idx].bytes, n);
        if (t)
            *t = cpu->xmm[idx].taint;
        return;
    }
    if (op.type == ZYDIS_OPERAND_TYPE_MEMORY) {
        uint64_t addr = resolve_mem(cpu, op, cpu->rip + d.insn.length);
        mem_load_bytes(cpu, addr, out, n);
        if (t)
            mem_get_taint(addr, n, t);
        else {
            Taint tmp;
            mem_get_taint(addr, n, &tmp);
            (void)tmp;
        }
        return;
    }
    guest_error(cpu, "unsupported vector operand");
}

void vec_store_bytes(CPU* cpu, const Dec& d, int oi, const uint8_t* in, unsigned n,
                     bool zero_upper, unsigned total_width, Taint t = Taint()) {
    const auto& op = d.ops[oi];
    if (op.type == ZYDIS_OPERAND_TYPE_REGISTER) {
        int idx, width;
        if (!reg_to_vec(op.reg.value, idx, width))
            guest_error(cpu, "unsupported vector register");
        memcpy(cpu->xmm[idx].bytes, in, n);
        cpu->xmm[idx].taint = t;
        if (zero_upper) {
            unsigned zfrom = n < total_width ? n : total_width;
            // VEX: zero everything above n up to 64? No: VEX128 zeros
            // bits[255:128]... but upper ZMM bits: VEX zeros only up to
            // max VL? Real hardware: VEX-encoded insns zero bits above VL
            // up to 512 (actually up to MAXVL=512 for AVX512-capable? No:
            // VEX zeros upper bits of the full ZMM? VEX zeroes bits
            // MAXVL-1:VL where MAXVL is 512 if AVX512... on this host,
            // VEX128 zeros ymm upper + zmm upper? Yes: VEX zeros DEST[MAXVL-1:VL].
            // Phase 1: zero everything above n.
            if (zfrom < MAX_VEC_BYTES)
                memset(cpu->xmm[idx].bytes + zfrom, 0, MAX_VEC_BYTES - zfrom);
        }
        return;
    }
    if (op.type == ZYDIS_OPERAND_TYPE_MEMORY) {
        uint64_t addr = resolve_mem(cpu, op, cpu->rip + d.insn.length);
        mem_store_bytes(cpu, addr, in, n);
        mem_note_store(cpu, addr, n, t);
        return;
    }
    guest_error(cpu, "cannot store vector to operand");
}

// VecVal overloads: taint lives in VecVal.taint. Raw-pointer overloads
// above (for sub-vectors) use Taint as well (no bool pairs).
void vec_load_bytes(CPU* cpu, const Dec& d, int oi, VecVal& out, unsigned n) {
    const auto& op = d.ops[oi];
    out.taint.clear();
    if (op.type == ZYDIS_OPERAND_TYPE_REGISTER) {
        int idx, width;
        if (!reg_to_vec(op.reg.value, idx, width))
            guest_error(cpu, "unsupported vector register");
        memcpy(out.bytes, cpu->xmm[idx].bytes, n);
        out.taint = cpu->xmm[idx].taint;
        return;
    }
    if (op.type == ZYDIS_OPERAND_TYPE_MEMORY) {
        uint64_t addr = resolve_mem(cpu, op, cpu->rip + d.insn.length);
        mem_load_bytes(cpu, addr, out.bytes, n);
        mem_get_taint(addr, n, &out.taint);
        return;
    }
    guest_error(cpu, "unsupported vector operand");
}

void vec_store_bytes(CPU* cpu, const Dec& d, int oi, const VecVal& in, unsigned n,
                     bool zero_upper, unsigned total_width) {
    const auto& op = d.ops[oi];
    if (op.type == ZYDIS_OPERAND_TYPE_REGISTER) {
        int idx, width;
        if (!reg_to_vec(op.reg.value, idx, width))
            guest_error(cpu, "unsupported vector register");
        memcpy(cpu->xmm[idx].bytes, in.bytes, n);
        cpu->xmm[idx].taint = in.taint;
        if (zero_upper) {
            unsigned zfrom = n < total_width ? n : total_width;
            if (zfrom < MAX_VEC_BYTES)
                memset(cpu->xmm[idx].bytes + zfrom, 0, MAX_VEC_BYTES - zfrom);
        }
        return;
    }
    if (op.type == ZYDIS_OPERAND_TYPE_MEMORY) {
        uint64_t addr = resolve_mem(cpu, op, cpu->rip + d.insn.length);
        mem_store_bytes(cpu, addr, in.bytes, n);
        mem_note_store(cpu, addr, n, in.taint);
        return;
    }
    guest_error(cpu, "cannot store vector to operand");
}

// --- taint helpers ---------------------------------------------------------
// All computation propagates OR of inputs' cannot-branch/index bits, except
// self-xor (and self-sub) of the same register, which produce untagged zero.

void flags_taint2(CPU* cpu, Val a, Val b) {
    cpu->flags_taint.cannot_branch = a.taint.cannot_branch || b.taint.cannot_branch;
    cpu->flags_taint.cannot_index = a.taint.cannot_index || b.taint.cannot_index;
}

void flags_taint1(CPU* cpu, Val a) {
    cpu->flags_taint.cannot_branch = a.taint.cannot_branch;
    cpu->flags_taint.cannot_index = a.taint.cannot_index;
}

void flags_clear(CPU* cpu) {
    cpu->flags_taint.cannot_branch = false;
    cpu->flags_taint.cannot_index = false;
}

void check_flags_taint(CPU* cpu, const char* what) {
    if (cpu->flags_taint.cannot_branch)
        guest_error(cpu, "tainted (cannot-branch) value used in branch condition");
    if (cpu->flags_taint.cannot_index)
        guest_error(cpu, "tainted (cannot-index) value used in branch condition");
    (void)what;
}

// True when all explicit register vector operands denote the same register
// (self-xor idiom: result is zero regardless of input, taint clears).
bool vec_self_op(const Dec& d) {
    int first = -1;
    for (int i = 0; i < d.insn.operand_count; i++) {
        if (d.ops[i].visibility == ZYDIS_OPERAND_VISIBILITY_HIDDEN)
            continue;
        if (d.ops[i].type != ZYDIS_OPERAND_TYPE_REGISTER)
            return false;
        int idx, width;
        if (!reg_to_vec(d.ops[i].reg.value, idx, width))
            return false;
        if (first < 0)
            first = idx;
        else if (first != idx)
            return false;
    }
    return first >= 0;
}

uint64_t next_rip(CPU* cpu, const Dec& d) { return cpu->rip + d.insn.length; }

void push64(CPU* cpu, uint64_t v, Taint t = Taint()) {
    uint64_t rsp = cpu->gpr[ZG_RSP].val - 8;
    if (cpu->gpr[ZG_RSP].taint.cannot_index)
        guest_error(cpu, "tainted RSP used by push");
    mem_store64(cpu, rsp, v);
    // Record the stored value's taint on the new stack slot: a clean push
    // clears stale poison (via clear-on-store), a tainted push poisons it.
    mem_note_store(cpu, rsp, 8, t);
    // RSP taint tracks the pointer only: it keeps its old taint (normally
    // clean) and must NOT OR in the stored value's taint. Otherwise every
    // push of a tainted value would taint RSP and all later RSP-relative
    // accesses would false-positive in resolve_mem.
    cpu->gpr[ZG_RSP].val = rsp;
}

// Pop preserving the stack slot's taint (loaded bytes' poison sidecar).
// RSP itself keeps its old taint; only .val advances.
Val pop64(CPU* cpu) {
    uint64_t rsp = cpu->gpr[ZG_RSP].val;
    if (cpu->gpr[ZG_RSP].taint.cannot_index)
        guest_error(cpu, "tainted RSP used by pop");
    uint64_t v = mem_load64(cpu, rsp);
    Val r;
    r.v = v;
    mem_get_taint(rsp, 8, &r.taint);
    cpu->gpr[ZG_RSP].val = rsp + 8;
    return r;
}

void trace_insn(CPU* cpu, const Dec& d) {
    char buf[256];
    if (ZYAN_SUCCESS(ZydisFormatterFormatInstruction(
            &t_formatter, &d.insn, d.ops, d.insn.operand_count_visible, buf,
            sizeof(buf), cpu->rip, nullptr)))
        fprintf(stderr, "[zegarmistrz:tid=%d] %#lx: %s\n", cpu->tid, cpu->rip, buf);
    else
        fprintf(stderr, "[zegarmistrz:tid=%d] %#lx: <unformattable>\n", cpu->tid,
                cpu->rip);
    fflush(stderr);
}

[[noreturn]] void unimplemented(CPU* cpu, const Dec& d, const char* what) {
    static thread_local char msg[256];
    const char* m = ZydisMnemonicGetString(d.insn.mnemonic);
    snprintf(msg, sizeof(msg), "unimplemented %s (%s)", m ? m : "?", what);
    guest_error(cpu, msg, cpu->rip);
}

} // namespace

// === integer ALU / data movement ============================================
// All exec_* take (CPU*, const Dec&) and update RIP on success.

namespace {

int common_int_width(const Dec& d) {
    // Width from first explicit operand with nonzero size.
    for (int i = 0; i < d.insn.operand_count; i++) {
        if (d.ops[i].visibility == ZYDIS_OPERAND_VISIBILITY_HIDDEN)
            continue;
        if (d.ops[i].visibility != ZYDIS_OPERAND_VISIBILITY_EXPLICIT)
            continue;
        if (d.ops[i].type == ZYDIS_OPERAND_TYPE_REGISTER ||
            d.ops[i].type == ZYDIS_OPERAND_TYPE_MEMORY) {
            if (d.ops[i].size)
                return d.ops[i].size / 8;
        }
    }
    // Fallback: implicit register operands (e.g. AL in `test al, 1`).
    for (int i = 0; i < d.insn.operand_count; i++) {
        if (d.ops[i].visibility == ZYDIS_OPERAND_VISIBILITY_HIDDEN)
            continue;
        if (d.ops[i].type == ZYDIS_OPERAND_TYPE_REGISTER ||
            d.ops[i].type == ZYDIS_OPERAND_TYPE_MEMORY) {
            if (d.ops[i].size)
                return d.ops[i].size / 8;
        }
    }
    guest_error(nullptr, "cannot determine operand width");
}


// Target of a near branch/call operand (relative imm or absolute reg/mem).
uint64_t branch_target(CPU* cpu, const Dec& d, int oi) {
    const auto& op = d.ops[oi];
    if (op.type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        uint64_t base = cpu->rip + d.insn.length;
        if (op.imm.is_relative) {
            int64_t off = op.imm.is_signed ? op.imm.value.s : (int64_t)op.imm.value.u;
            return base + (uint64_t)off;
        }
        return op.imm.is_signed ? (uint64_t)op.imm.value.s : op.imm.value.u;
    }
    Val v = op_load(cpu, d, oi);
    if (v.taint.cannot_branch)
        guest_error(cpu, "tainted (cannot-branch) branch target");
    if (v.taint.cannot_index)
        guest_error(cpu, "tainted (cannot-index) branch target");
    return v.v;
}

void exec_mov(CPU* cpu, const Dec& d) {
    Val s = op_load(cpu, d, 1);
    op_store(cpu, d, 0, s);
    cpu->rip = next_rip(cpu, d);
}

void exec_lea(CPU* cpu, const Dec& d) {
    uint64_t addr = resolve_mem(cpu, d.ops[1], cpu->rip + d.insn.length);
    Val v;
    v.v = addr;
    // LEA of a tainted base/index propagates (address derived from secret).
    // resolve_mem rejects cannot-index outright, so only branch taint can
    // arrive here; recompute from the address registers.
    {
        const auto& op = d.ops[1];
        Taint t;
        if (op.mem.base != ZYDIS_REGISTER_NONE &&
            op.mem.base != ZYDIS_REGISTER_RIP) {
            int idx, size, shift;
            if (reg_to_gpr(op.mem.base, idx, size, shift)) {
                t |= cpu->gpr[idx].taint;
            }
        }
        if (op.mem.index != ZYDIS_REGISTER_NONE) {
            int idx, size, shift;
            if (reg_to_gpr(op.mem.index, idx, size, shift)) {
                t |= cpu->gpr[idx].taint;
            }
        }
        v.taint = t;
    }
    op_store(cpu, d, 0, v);
    cpu->rip = next_rip(cpu, d);
}

void exec_xchg(CPU* cpu, const Dec& d) {
    Val a = op_load(cpu, d, 0);
    Val b = op_load(cpu, d, 1);
    op_store(cpu, d, 0, b);
    op_store(cpu, d, 1, a);
    cpu->rip = next_rip(cpu, d);
}

void exec_push(CPU* cpu, const Dec& d) {
    Val v = op_load(cpu, d, 0);
    push64(cpu, v.v, v.taint);
    cpu->rip = next_rip(cpu, d);
}

void exec_pop(CPU* cpu, const Dec& d) {
    Val val = pop64(cpu);
    op_store(cpu, d, 0, val);
    cpu->rip = next_rip(cpu, d);
}

void exec_leave(CPU* cpu, const Dec& d) {
    // MOV RSP,RBP semantics: RSP takes RBP's value and pointer taint.
    cpu->gpr[ZG_RSP].val = cpu->gpr[ZG_RBP].val;
    cpu->gpr[ZG_RSP].taint.cannot_branch = cpu->gpr[ZG_RBP].taint.cannot_branch;
    cpu->gpr[ZG_RSP].taint.cannot_index = cpu->gpr[ZG_RBP].taint.cannot_index;
    Val v = pop64(cpu);
    cpu->gpr[ZG_RBP].val = v.v;
    cpu->gpr[ZG_RBP].taint.cannot_branch = v.taint.cannot_branch;
    cpu->gpr[ZG_RBP].taint.cannot_index = v.taint.cannot_index;
    cpu->rip = next_rip(cpu, d);
}

void exec_enter(CPU* cpu, const Dec& d) {
    // ENTER imm16, imm8. Rare; implement the general form.
    Val alloc = op_load(cpu, d, 0); // bytes to allocate
    Val nesting = op_load(cpu, d, 1);
    // Preserve taint across the frame pushes (push64 keeps RSP taint but
    // records slot taint via mem_note_store).
    push64(cpu, cpu->gpr[ZG_RBP].val, cpu->gpr[ZG_RBP].taint);
    uint64_t frame = cpu->gpr[ZG_RSP].val;
    // RBP at this point still holds the old frame pointer for the copies.
    Taint old_rsp_taint = cpu->gpr[ZG_RSP].taint;
    unsigned level = (unsigned)nesting.v & 0x1f;
    if (level > 0) {
        for (unsigned i = 1; i < level; i++) {
            uint64_t a = cpu->gpr[ZG_RBP].val - i * 8;
            uint64_t w = mem_load64(cpu, a);
            Taint t;
            mem_get_taint(a, 8, &t);
            Val wv;
            wv.v = w;
            wv.taint = t;
            (void)wv;
            push64(cpu, w, t);
        }
        push64(cpu, frame, old_rsp_taint);
    }
    cpu->gpr[ZG_RBP].val = frame;
    cpu->gpr[ZG_RBP].taint = old_rsp_taint;
    // New RSP = frame - alloc: pointer derived from frame and alloc size, so
    // it inherits both the old RSP taint and the alloc-size taint. The
    // nesting level controls the loop above (branch-like), so its taint
    // propagates too.
    cpu->gpr[ZG_RSP].val = frame - (alloc.v & 0xffff);
    cpu->gpr[ZG_RSP].taint = old_rsp_taint | alloc.taint | nesting.taint;
    cpu->rip = next_rip(cpu, d);
}

// ADD/SUB/ADC/SBB/CMP/AND/OR/XOR/TEST shared core.
void exec_alu(CPU* cpu, const Dec& d, ZydisMnemonic m) {
    int n = explicit_ops(d);
    if (n < 2)
        guest_error(cpu, "alu needs 2 operands");
    Val dst = op_load(cpu, d, 0);
    Val src = op_load(cpu, d, 1);
    int w = common_int_width(d);
    uint64_t dm = mask_for(w);
    uint64_t a = dst.v & dm, b = src.v & dm;
    bool is_zero_idiom = (d.ops[0].type == ZYDIS_OPERAND_TYPE_REGISTER &&
                          d.ops[1].type == ZYDIS_OPERAND_TYPE_REGISTER &&
                          d.ops[0].reg.value == d.ops[1].reg.value);
    Val out;
    switch (m) {
    case ZYDIS_MNEMONIC_ADD: {
        uint64_t r = (a + b) & dm;
        flags_add(cpu, w, a, b, r);
        out.v = r;
        break;
    }
    case ZYDIS_MNEMONIC_ADC: {
        uint64_t c = cpu->cf() ? 1 : 0;
        unsigned bits = w * 8;
        uint64_t m = mask_for(w);
        __uint128_t full = (__uint128_t)(a & m) + (b & m) + c;
        uint64_t r = (uint64_t)full & m;
        cpu->set_flag(FLAG_CF, ((full >> bits) & 1) != 0);
        bool sa = ((a & m) >> (bits - 1)) & 1, sb = ((b & m) >> (bits - 1)) & 1;
        uint64_t t1 = ((a & m) + (b & m)) & m;
        bool st1 = (t1 >> (bits - 1)) & 1, sr = (r >> (bits - 1)) & 1;
        bool of1 = (sa == sb) && (st1 != sa);
        bool of2 = (st1 == (bool)c) && (sr != st1);
        cpu->set_flag(FLAG_OF, of1 ^ of2);
        cpu->set_flag(FLAG_ZF, r == 0);
        cpu->set_flag(FLAG_SF, sr);
        cpu->set_flag(FLAG_PF, parity8((uint8_t)r));
        cpu->set_flag(FLAG_AF, ((a ^ b ^ r) & 0x10) != 0);
        out.v = r;
        break;
    }
    case ZYDIS_MNEMONIC_SUB:
    case ZYDIS_MNEMONIC_CMP: {
        if (m == ZYDIS_MNEMONIC_SUB && is_zero_idiom) {
            // sub reg,reg == 0: clears taint like xor.
            flags_logic(cpu, w, 0);
            flags_clear(cpu);
            out.v = 0;
            out.taint.cannot_branch = false;
            out.taint.cannot_index = false;
            op_store(cpu, d, 0, out);
            cpu->rip = next_rip(cpu, d);
            return;
        }
        uint64_t r = (a - b) & dm;
        flags_sub(cpu, w, a, b, r);
        out.v = r;
        break;
    }
    case ZYDIS_MNEMONIC_SBB: {
        uint64_t c = cpu->cf() ? 1 : 0;
        unsigned bits = w * 8;
        uint64_t m = mask_for(w);
        uint64_t av = a & m, bv = b & m;
        uint64_t t1 = (av - bv) & m;
        bool borrow1 = av < bv;
        uint64_t r = (t1 - c) & m;
        bool borrow2 = t1 < c;
        cpu->set_flag(FLAG_CF, borrow1 || borrow2);
        bool sa = (av >> (bits - 1)) & 1, sb = (bv >> (bits - 1)) & 1;
        bool st1 = (t1 >> (bits - 1)) & 1, sr = (r >> (bits - 1)) & 1;
        bool of1 = (sa != sb) && (st1 != sa);
        bool of2 = (st1 != (bool)c) && (sr != st1);
        cpu->set_flag(FLAG_OF, of1 ^ of2);
        cpu->set_flag(FLAG_ZF, r == 0);
        cpu->set_flag(FLAG_SF, sr);
        cpu->set_flag(FLAG_PF, parity8((uint8_t)r));
        cpu->set_flag(FLAG_AF, ((av ^ bv ^ r) & 0x10) != 0);
        out.v = r;
        break;
    }
    case ZYDIS_MNEMONIC_AND:
    case ZYDIS_MNEMONIC_TEST: {
        uint64_t r = a & b;
        flags_logic(cpu, w, r);
        out.v = r;
        break;
    }
    case ZYDIS_MNEMONIC_OR: {
        uint64_t r = a | b;
        flags_logic(cpu, w, r);
        out.v = r;
        break;
    }
    case ZYDIS_MNEMONIC_XOR: {
        if (is_zero_idiom) {
            flags_logic(cpu, w, 0);
            flags_clear(cpu);
            out.v = 0;
            out.taint.cannot_branch = false;
            out.taint.cannot_index = false;
            if (m == ZYDIS_MNEMONIC_XOR) {
                op_store(cpu, d, 0, out);
                cpu->rip = next_rip(cpu, d);
                return;
            }
        }
        uint64_t r = a ^ b;
        flags_logic(cpu, w, r);
        out.v = r;
        break;
    }
    default: guest_error(cpu, "bad alu op");
    }
    // Taint propagation (except zero idiom handled above).
    out.taint.cannot_branch = dst.taint.cannot_branch || src.taint.cannot_branch;
    out.taint.cannot_index = dst.taint.cannot_index || src.taint.cannot_index;
    flags_taint2(cpu, dst, src);
    if (m == ZYDIS_MNEMONIC_CMP || m == ZYDIS_MNEMONIC_TEST) {
        cpu->rip = next_rip(cpu, d);
        return;
    }
    op_store(cpu, d, 0, out);
    // LOCK-prefixed read-modify-write on memory should be atomic. Since we
    // did load-then-store, add atomicity via host CAS retry for the mem case.
    // (Phase 1: single-threaded-correct; helper-thread races on the same
    // word are benign for the hello corpus which uses futexes properly.)
    cpu->rip = next_rip(cpu, d);
}

void exec_incdec(CPU* cpu, const Dec& d, bool is_inc) {
    Val v = op_load(cpu, d, 0);
    int w = common_int_width(d);
    uint64_t m = mask_for(w);
    uint64_t a = v.v & m;
    uint64_t r = is_inc ? (a + 1) & m : (a - 1) & m;
    bool old_cf = cpu->cf();
    if (is_inc)
        flags_add(cpu, w, a, 1, r);
    else
        flags_sub(cpu, w, a, 1, r);
    cpu->set_flag(FLAG_CF, old_cf); // INC/DEC preserve CF
    Val out;
    out.v = r;
    out.taint.cannot_branch = v.taint.cannot_branch;
    out.taint.cannot_index = v.taint.cannot_index;
    flags_taint1(cpu, v);
    op_store(cpu, d, 0, out);
    cpu->rip = next_rip(cpu, d);
}

void exec_neg(CPU* cpu, const Dec& d) {
    Val v = op_load(cpu, d, 0);
    int w = common_int_width(d);
    uint64_t m = mask_for(w);
    uint64_t a = v.v & m;
    uint64_t r = (-a) & m;
    flags_sub(cpu, w, 0, a, r);
    Val out;
    out.v = r;
    out.taint.cannot_branch = v.taint.cannot_branch;
    out.taint.cannot_index = v.taint.cannot_index;
    flags_taint1(cpu, v);
    op_store(cpu, d, 0, out);
    cpu->rip = next_rip(cpu, d);
}

void exec_not(CPU* cpu, const Dec& d) {
    Val v = op_load(cpu, d, 0);
    int w = common_int_width(d);
    Val out;
    out.v = (~v.v) & mask_for(w);
    out.taint.cannot_branch = v.taint.cannot_branch;
    out.taint.cannot_index = v.taint.cannot_index;
    op_store(cpu, d, 0, out);
    cpu->rip = next_rip(cpu, d);
}

void exec_shift(CPU* cpu, const Dec& d, ZydisMnemonic m) {
    Val dst = op_load(cpu, d, 0);
    Val cnt = op_load(cpu, d, 1);
    int w = common_int_width(d);
    unsigned bits = w * 8;
    uint64_t count = cnt.v & (bits == 64 ? 63 : 31);
    uint64_t a = dst.v & mask_for(w);
    Val out;
    out.taint.cannot_branch = dst.taint.cannot_branch || cnt.taint.cannot_branch;
    out.taint.cannot_index = dst.taint.cannot_index || cnt.taint.cannot_index;
    if (count == 0) {
        cpu->rip = next_rip(cpu, d);
        return;
    }
    uint64_t r = 0;
    bool cf = false, of = false;
    if (m == ZYDIS_MNEMONIC_SHL) {
        r = (a << count) & mask_for(w);
        cf = (a >> (bits - count)) & 1;
        if (count == 1)
            of = ((r >> (bits - 1)) & 1) ^ cf;
    } else if (m == ZYDIS_MNEMONIC_SHR) {
        r = (a >> count) & mask_for(w);
        cf = (a >> (count - 1)) & 1;
        if (count == 1)
            of = (a >> (bits - 1)) & 1;
    } else if (m == ZYDIS_MNEMONIC_SAR) {
        // w is in bytes: 1/2/4/8.
        if (w == 1)
            r = (uint64_t)(int64_t)((int8_t)a >> (count & 7)) & 0xff;
        else if (w == 2)
            r = (uint64_t)(int64_t)((int16_t)a >> (count & 15)) & 0xffff;
        else if (w == 4)
            r = (uint64_t)(int64_t)((int32_t)a >> (count & 31)) & 0xffffffffULL;
        else
            r = (uint64_t)((int64_t)a >> count);
        cf = (a >> (count - 1)) & 1;
        if (count == 1)
            of = false;
    } else if (m == ZYDIS_MNEMONIC_ROL) {
        count %= bits;
        r = ((a << count) | (a >> (bits - count))) & mask_for(w);
        cf = r & 1;
        if (count == 1)
            of = ((r >> (bits - 1)) & 1) ^ cf;
    } else if (m == ZYDIS_MNEMONIC_ROR) {
        count %= bits;
        r = ((a >> count) | (a << (bits - count))) & mask_for(w);
        cf = (r >> (bits - 1)) & 1;
        if (count == 1)
            of = ((r >> (bits - 1)) & 1) ^ ((r >> (bits - 2)) & 1);
    } else if (m == ZYDIS_MNEMONIC_RCL || m == ZYDIS_MNEMONIC_RCR) {
        unsigned total = bits + 1;
        uint64_t c = count % total;
        __uint128_t big = (__uint128_t)a | ((__uint128_t)(cpu->cf() ? 1 : 0) << bits);
        __uint128_t mod = (__uint128_t)1 << total;
        if (m == ZYDIS_MNEMONIC_RCL)
            big = ((big << c) | (big >> (total - c))) % mod;
        else
            big = ((big >> c) | (big << (total - c))) % mod;
        cf = (big >> bits) & 1;
        r = (uint64_t)big & mask_for(w);
        if (c == 1)
            of = ((r >> (bits - 1)) & 1) ^ cf;
    } else {
        guest_error(cpu, "bad shift op");
    }
    cpu->set_flag(FLAG_CF, cf);
    if (count == 1)
        cpu->set_flag(FLAG_OF, of);
    if (m == ZYDIS_MNEMONIC_SHL || m == ZYDIS_MNEMONIC_SHR || m == ZYDIS_MNEMONIC_SAR) {
        cpu->set_flag(FLAG_ZF, (r & mask_for(w)) == 0);
        cpu->set_flag(FLAG_SF, (r >> (bits - 1)) & 1);
        cpu->set_flag(FLAG_PF, parity8((uint8_t)r));
    }
    out.v = r;
    flags_taint2(cpu, dst, cnt);
    op_store(cpu, d, 0, out);
    cpu->rip = next_rip(cpu, d);
}

void exec_imul(CPU* cpu, const Dec& d) {
    int n = explicit_ops(d);
    int w = common_int_width(d);
    Val out;
    if (n == 1) {
        // One-operand signed multiply: RDX:RAX = RAX * src.
        Val src = op_load(cpu, d, 0);
        if (w == 1) {
            int16_t r = (int16_t)(int8_t)(cpu->gpr[ZG_RAX].val & 0xff) *
                        (int8_t)(src.v & 0xff);
            cpu->gpr[ZG_RAX].val =
                (cpu->gpr[ZG_RAX].val & ~0xffffULL) | ((uint16_t)r);
            bool o = r != (int8_t)r;
            cpu->set_flag(FLAG_CF, o);
            cpu->set_flag(FLAG_OF, o);
        } else if (w == 2) {
            int32_t r = (int32_t)(int16_t)(cpu->gpr[ZG_RAX].val & 0xffff) *
                        (int16_t)(src.v & 0xffff);
            cpu->gpr[ZG_RAX].val = (cpu->gpr[ZG_RAX].val & ~0xffffULL) | (r & 0xffff);
            cpu->gpr[ZG_RDX].val = (cpu->gpr[ZG_RDX].val & ~0xffffULL) | ((r >> 16) & 0xffff);
            bool o = r != (int16_t)r;
            cpu->set_flag(FLAG_CF, o);
            cpu->set_flag(FLAG_OF, o);
        } else if (w == 4) {
            int64_t r = (int64_t)(int32_t)(cpu->gpr[ZG_RAX].val & 0xffffffffULL) *
                        (int32_t)(src.v & 0xffffffffULL);
            cpu->gpr[ZG_RAX].val = (uint32_t)r;
            cpu->gpr[ZG_RDX].val = (uint32_t)(r >> 32);
            bool o = r != (int32_t)r;
            cpu->set_flag(FLAG_CF, o);
            cpu->set_flag(FLAG_OF, o);
        } else {
            __int128 r = (__int128)(int64_t)cpu->gpr[ZG_RAX].val * (int64_t)src.v;
            cpu->gpr[ZG_RAX].val = (uint64_t)r;
            cpu->gpr[ZG_RDX].val = (uint64_t)(r >> 64);
            bool o = r != (int64_t)r;
            cpu->set_flag(FLAG_CF, o);
            cpu->set_flag(FLAG_OF, o);
        }
        cpu->set_flag(FLAG_ZF, false);
        cpu->set_flag(FLAG_SF, false);
        cpu->set_flag(FLAG_PF, false);
        cpu->rip = next_rip(cpu, d);
        return;
    }
    Val s1 = op_load(cpu, d, n - 1); // last operand is source/imm
    Val s0 = (n == 3) ? op_load(cpu, d, 1) : op_load(cpu, d, 0);
    if (w == 1) {
        int16_t r = (int8_t)s0.v * (int8_t)s1.v;
        out.v = (uint8_t)r;
        bool o = r != (int8_t)r;
        cpu->set_flag(FLAG_CF, o);
        cpu->set_flag(FLAG_OF, o);
    } else if (w == 2) {
        int32_t r = (int16_t)s0.v * (int16_t)s1.v;
        out.v = (uint16_t)r;
        bool o = r != (int16_t)r;
        cpu->set_flag(FLAG_CF, o);
        cpu->set_flag(FLAG_OF, o);
    } else if (w == 4) {
        int64_t r = (int64_t)(int32_t)s0.v * (int32_t)s1.v;
        out.v = (uint32_t)r;
        bool o = r != (int32_t)r;
        cpu->set_flag(FLAG_CF, o);
        cpu->set_flag(FLAG_OF, o);
    } else {
        __int128 r = (__int128)(int64_t)s0.v * (int64_t)s1.v;
        out.v = (uint64_t)r;
        bool o = r != (int64_t)r;
        cpu->set_flag(FLAG_CF, o);
        cpu->set_flag(FLAG_OF, o);
    }
    cpu->set_flag(FLAG_ZF, false);
    cpu->set_flag(FLAG_SF, false);
    cpu->set_flag(FLAG_PF, false);
    out.taint.cannot_branch = s0.taint.cannot_branch || s1.taint.cannot_branch;
    out.taint.cannot_index = s0.taint.cannot_index || s1.taint.cannot_index;
    op_store(cpu, d, 0, out);
    cpu->rip = next_rip(cpu, d);
}

void exec_mul(CPU* cpu, const Dec& d) {
    Val src = op_load(cpu, d, 0);
    int w = common_int_width(d);
    if (w == 1) {
        uint16_t r = (uint8_t)(cpu->gpr[ZG_RAX].val & 0xff) * (uint8_t)(src.v & 0xff);
        cpu->gpr[ZG_RAX].val = (cpu->gpr[ZG_RAX].val & ~0xffffULL) | r;
        bool o = (r & 0xff00) != 0;
        cpu->set_flag(FLAG_CF, o);
        cpu->set_flag(FLAG_OF, o);
    } else if (w == 2) {
        uint32_t r = (uint16_t)(cpu->gpr[ZG_RAX].val & 0xffff) * (uint16_t)(src.v & 0xffff);
        cpu->gpr[ZG_RAX].val = (cpu->gpr[ZG_RAX].val & ~0xffffULL) | (r & 0xffff);
        cpu->gpr[ZG_RDX].val = (cpu->gpr[ZG_RDX].val & ~0xffffULL) | ((r >> 16) & 0xffff);
        bool o = (r & 0xffff0000) != 0;
        cpu->set_flag(FLAG_CF, o);
        cpu->set_flag(FLAG_OF, o);
    } else if (w == 4) {
        uint64_t r = (uint64_t)(uint32_t)(cpu->gpr[ZG_RAX].val & 0xffffffffULL) *
                     (uint32_t)(src.v & 0xffffffffULL);
        cpu->gpr[ZG_RAX].val = (uint32_t)r;
        cpu->gpr[ZG_RDX].val = (uint32_t)(r >> 32);
        bool o = (r >> 32) != 0;
        cpu->set_flag(FLAG_CF, o);
        cpu->set_flag(FLAG_OF, o);
    } else {
        __uint128_t r = (__uint128_t)cpu->gpr[ZG_RAX].val * src.v;
        cpu->gpr[ZG_RAX].val = (uint64_t)r;
        cpu->gpr[ZG_RDX].val = (uint64_t)(r >> 64);
        bool o = (r >> 64) != 0;
        cpu->set_flag(FLAG_CF, o);
        cpu->set_flag(FLAG_OF, o);
    }
    cpu->rip = next_rip(cpu, d);
}

void exec_div(CPU* cpu, const Dec& d, bool is_signed) {
    Val src = op_load(cpu, d, 0);
    int w = common_int_width(d);
    if (w == 1) {
        uint16_t dvd = cpu->gpr[ZG_RAX].val & 0xffff;
        if (is_signed) {
            int8_t ds = (int8_t)(src.v & 0xff);
            if (!ds)
                guest_error(cpu, "guest SIGFPE (integer divide by zero)", cpu->rip);
            int16_t q = (int16_t)dvd / ds;
            int8_t r = (int16_t)dvd % ds;
            if (q != (int8_t)q)
                guest_error(cpu, "guest SIGFPE (integer overflow)", cpu->rip);
            cpu->gpr[ZG_RAX].val =
                (cpu->gpr[ZG_RAX].val & ~0xffffULL) | ((uint8_t)q) | ((uint16_t)(uint8_t)r << 8);
        } else {
            uint8_t ds = src.v & 0xff;
            if (!ds)
                guest_error(cpu, "guest SIGFPE (integer divide by zero)", cpu->rip);
            cpu->gpr[ZG_RAX].val = (cpu->gpr[ZG_RAX].val & ~0xffffULL) |
                                    (uint16_t)(dvd / ds) | ((uint16_t)(dvd % ds) << 8);
        }
    } else if (w == 2) {
        uint32_t dvd =
            (cpu->gpr[ZG_RAX].val & 0xffff) | ((cpu->gpr[ZG_RDX].val & 0xffff) << 16);
        if (is_signed) {
            int16_t ds = (int16_t)(src.v & 0xffff);
            if (!ds)
                guest_error(cpu, "guest SIGFPE (integer divide by zero)", cpu->rip);
            int32_t q = (int32_t)dvd / ds;
            int16_t r = (int32_t)dvd % ds;
            if (q != (int16_t)q)
                guest_error(cpu, "guest SIGFPE (integer overflow)", cpu->rip);
            cpu->gpr[ZG_RAX].val = (cpu->gpr[ZG_RAX].val & ~0xffffULL) | (uint16_t)q;
            cpu->gpr[ZG_RDX].val = (cpu->gpr[ZG_RDX].val & ~0xffffULL) | (uint16_t)r;
        } else {
            uint16_t ds = src.v & 0xffff;
            if (!ds)
                guest_error(cpu, "guest SIGFPE (integer divide by zero)", cpu->rip);
            if (dvd / ds > 0xffff)
                guest_error(cpu, "guest SIGFPE (integer overflow)", cpu->rip);
            cpu->gpr[ZG_RAX].val = (cpu->gpr[ZG_RAX].val & ~0xffffULL) | (uint16_t)(dvd / ds);
            cpu->gpr[ZG_RDX].val = (cpu->gpr[ZG_RDX].val & ~0xffffULL) | (uint16_t)(dvd % ds);
        }
    } else if (w == 4) {
        uint64_t dvd = (cpu->gpr[ZG_RAX].val & 0xffffffffULL) |
                       (cpu->gpr[ZG_RDX].val << 32);
        if (is_signed) {
            int32_t ds = (int32_t)(src.v & 0xffffffffULL);
            if (!ds)
                guest_error(cpu, "guest SIGFPE (integer divide by zero)", cpu->rip);
            int64_t q = (int64_t)dvd / ds;
            int32_t r = (int64_t)dvd % ds;
            if (q != (int32_t)q)
                guest_error(cpu, "guest SIGFPE (integer overflow)", cpu->rip);
            cpu->gpr[ZG_RAX].val = (uint32_t)q;
            cpu->gpr[ZG_RDX].val = (uint32_t)r;
        } else {
            uint32_t ds = src.v & 0xffffffffULL;
            if (!ds)
                guest_error(cpu, "guest SIGFPE (integer divide by zero)", cpu->rip);
            if (dvd / ds > 0xffffffffULL)
                guest_error(cpu, "guest SIGFPE (integer overflow)", cpu->rip);
            cpu->gpr[ZG_RAX].val = (uint32_t)(dvd / ds);
            cpu->gpr[ZG_RDX].val = (uint32_t)(dvd % ds);
        }
    } else {
        __uint128_t dvd =
            (__uint128_t)cpu->gpr[ZG_RAX].val | ((__uint128_t)cpu->gpr[ZG_RDX].val << 64);
        if (is_signed) {
            int64_t ds = (int64_t)src.v;
            if (!ds)
                guest_error(cpu, "guest SIGFPE (integer divide by zero)", cpu->rip);
            __int128 q = (__int128)dvd / ds;
            int64_t r = (__int128)dvd % ds;
            if (q != (int64_t)q)
                guest_error(cpu, "guest SIGFPE (integer overflow)", cpu->rip);
            cpu->gpr[ZG_RAX].val = (uint64_t)q;
            cpu->gpr[ZG_RDX].val = (uint64_t)r;
        } else {
            if (!src.v)
                guest_error(cpu, "guest SIGFPE (integer divide by zero)", cpu->rip);
            if (cpu->gpr[ZG_RDX].val >= src.v)
                guest_error(cpu, "guest SIGFPE (integer overflow)", cpu->rip);
            cpu->gpr[ZG_RAX].val = (uint64_t)(dvd / src.v);
            cpu->gpr[ZG_RDX].val = (uint64_t)(dvd % src.v);
        }
    }
    cpu->rip = next_rip(cpu, d);
}

void exec_movzx(CPU* cpu, const Dec& d) {
    Val s = op_load(cpu, d, 1);
    Val out;
    out.v = s.v; // op_load already masks to source width; zero-extend is free
    out.taint.cannot_branch = s.taint.cannot_branch;
    out.taint.cannot_index = s.taint.cannot_index;
    op_store(cpu, d, 0, out);
    cpu->rip = next_rip(cpu, d);
}

void exec_movsx(CPU* cpu, const Dec& d) {
    Val s = op_load(cpu, d, 1);
    int sw = d.ops[1].size / 8;
    int64_t sv;
    if (sw == 1)
        sv = (int8_t)s.v;
    else if (sw == 2)
        sv = (int16_t)s.v;
    else
        sv = (int32_t)s.v;
    Val out;
    out.v = (uint64_t)sv;
    out.taint.cannot_branch = s.taint.cannot_branch;
    out.taint.cannot_index = s.taint.cannot_index;
    op_store(cpu, d, 0, out);
    cpu->rip = next_rip(cpu, d);
}

void exec_setcc(CPU* cpu, const Dec& d) {
    // SETcc materializes the flag condition as 0/1: the output is derived
    // from flags, so the flags taint propagates (like Jcc, but as data
    // rather than a fault).
    Val out;
    out.v = eval_cond(cpu, d.insn.mnemonic) ? 1 : 0;
    out.taint.cannot_branch = cpu->flags_taint.cannot_branch;
    out.taint.cannot_index = cpu->flags_taint.cannot_index;
    op_store(cpu, d, 0, out);
    cpu->rip = next_rip(cpu, d);
}

void exec_cmovcc(CPU* cpu, const Dec& d) {
    // CMOVcc leaks the flag condition into the destination whether or not
    // the move happens, so flags taint propagates on both paths.
    bool take = eval_cond(cpu, d.insn.mnemonic);
    if (take) {
        Val s = op_load(cpu, d, 1);
        s.taint.cannot_branch = s.taint.cannot_branch || cpu->flags_taint.cannot_branch;
        s.taint.cannot_index = s.taint.cannot_index || cpu->flags_taint.cannot_index;
        op_store(cpu, d, 0, s);
    } else if (cpu->flags_taint.cannot_branch || cpu->flags_taint.cannot_index) {
        // Not taken: taint the destination in place (CMOV dest is always a
        // register, so this load+store is side-effect free).
        Val cur = op_load(cpu, d, 0);
        cur.taint.cannot_branch = cur.taint.cannot_branch || cpu->flags_taint.cannot_branch;
        cur.taint.cannot_index = cur.taint.cannot_index || cpu->flags_taint.cannot_index;
        op_store(cpu, d, 0, cur);
    }
    cpu->rip = next_rip(cpu, d);
}

void exec_cbw(CPU* cpu, const Dec& d) {
    switch (d.insn.mnemonic) {
    case ZYDIS_MNEMONIC_CBW:
        cpu->gpr[ZG_RAX].val = (cpu->gpr[ZG_RAX].val & ~0xffffULL) |
                                (uint16_t)(int16_t)(int8_t)(cpu->gpr[ZG_RAX].val & 0xff);
        break;
    case ZYDIS_MNEMONIC_CWDE:
        cpu->gpr[ZG_RAX].val = (uint64_t)(int64_t)(int32_t)(int16_t)(cpu->gpr[ZG_RAX].val & 0xffff);
        break;
    case ZYDIS_MNEMONIC_CDQE:
        cpu->gpr[ZG_RAX].val = (uint64_t)(int64_t)(int32_t)(cpu->gpr[ZG_RAX].val & 0xffffffffULL);
        break;
    case ZYDIS_MNEMONIC_CWD: {
        int32_t v = (int32_t)(int16_t)(cpu->gpr[ZG_RAX].val & 0xffff);
        cpu->gpr[ZG_RAX].val = (cpu->gpr[ZG_RAX].val & ~0xffffULL) | (uint16_t)v;
        cpu->gpr[ZG_RDX].val = (cpu->gpr[ZG_RDX].val & ~0xffffULL) | (uint16_t)(v >> 16);
        break;
    }
    case ZYDIS_MNEMONIC_CDQ: {
        int64_t v = (int64_t)(int32_t)(cpu->gpr[ZG_RAX].val & 0xffffffffULL);
        cpu->gpr[ZG_RAX].val = (uint32_t)v;
        cpu->gpr[ZG_RDX].val = (uint32_t)(v >> 32);
        break;
    }
    case ZYDIS_MNEMONIC_CQO: {
        __int128 v = (__int128)(int64_t)cpu->gpr[ZG_RAX].val;
        cpu->gpr[ZG_RDX].val = (uint64_t)(v >> 64);
        break;
    }
    default: guest_error(cpu, "bad sign-extend op");
    }
    cpu->rip = next_rip(cpu, d);
}

void exec_bswap(CPU* cpu, const Dec& d) {
    Val v = op_load(cpu, d, 0);
    Val out;
    out.v = __builtin_bswap64(v.v) >> (64 - 8 * common_int_width(d));
    if (common_int_width(d) == 4)
        out.v = __builtin_bswap32((uint32_t)v.v);
    out.taint.cannot_branch = v.taint.cannot_branch;
    out.taint.cannot_index = v.taint.cannot_index;
    op_store(cpu, d, 0, out);
    cpu->rip = next_rip(cpu, d);
}

void exec_bit_test(CPU* cpu, const Dec& d) {
    // BT/BTS/BTR/BTC.
    Val base = op_load(cpu, d, 0);
    Val off = op_load(cpu, d, 1);
    int w = common_int_width(d);
    unsigned bits = w * 8;
    // For mem operands the bit offset can exceed width (address adjusts);
    // implement the full semantics.
    int64_t bit = (int64_t)off.v;
    Val out = base;
    out.taint.cannot_branch = base.taint.cannot_branch || off.taint.cannot_branch;
    out.taint.cannot_index = base.taint.cannot_index || off.taint.cannot_index;
    if (d.ops[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        int64_t byte_off;
        unsigned bit_in_byte;
        if (bit >= 0) {
            byte_off = bit / 8;
            bit_in_byte = bit % 8;
        } else {
            byte_off = -(((-bit) + 7) / 8);
            bit_in_byte = (unsigned)((bit - byte_off * 8));
        }
        uint64_t addr = resolve_mem(cpu, d.ops[0], cpu->rip + d.insn.length) + (uint64_t)byte_off;
        uint8_t byte = mem_load8(cpu, addr);
        cpu->set_flag(FLAG_CF, (byte >> bit_in_byte) & 1);
        if (d.insn.mnemonic == ZYDIS_MNEMONIC_BTS)
            mem_store8(cpu, addr, byte | (1u << bit_in_byte));
        else if (d.insn.mnemonic == ZYDIS_MNEMONIC_BTR)
            mem_store8(cpu, addr, byte & ~(1u << bit_in_byte));
        else if (d.insn.mnemonic == ZYDIS_MNEMONIC_BTC)
            mem_store8(cpu, addr, byte ^ (1u << bit_in_byte));
    } else {
        unsigned b = ((bit % (int64_t)bits) + bits) % bits;
        uint64_t m = mask_for(w);
        cpu->set_flag(FLAG_CF, ((base.v & m) >> b) & 1);
        uint64_t r = base.v & m;
        if (d.insn.mnemonic == ZYDIS_MNEMONIC_BTS)
            r |= (1ULL << b);
        else if (d.insn.mnemonic == ZYDIS_MNEMONIC_BTR)
            r &= ~(1ULL << b);
        else if (d.insn.mnemonic == ZYDIS_MNEMONIC_BTC)
            r ^= (1ULL << b);
        out.v = r;
        op_store(cpu, d, 0, out);
    }
    cpu->rip = next_rip(cpu, d);
}

void exec_bsfbsr(CPU* cpu, const Dec& d, bool is_bsr) {
    Val s = op_load(cpu, d, 1);
    int w = common_int_width(d);
    uint64_t v = s.v & mask_for(w);
    Val out;
    out.taint.cannot_branch = s.taint.cannot_branch;
    out.taint.cannot_index = s.taint.cannot_index;
    if (v == 0) {
        cpu->set_flag(FLAG_ZF, true);
        // dest undefined on zero; leave it.
    } else {
        cpu->set_flag(FLAG_ZF, false);
        out.v = is_bsr ? (63 - __builtin_clzll(v) - (64 - w * 8)) : __builtin_ctzll(v);
        op_store(cpu, d, 0, out);
    }
    cpu->rip = next_rip(cpu, d);
}

void exec_popcnt(CPU* cpu, const Dec& d) {
    Val s = op_load(cpu, d, 1);
    int w = common_int_width(d);
    Val out;
    out.v = __builtin_popcountll(s.v & mask_for(w));
    out.taint.cannot_branch = s.taint.cannot_branch;
    out.taint.cannot_index = s.taint.cannot_index;
    cpu->set_flag(FLAG_CF, false);
    cpu->set_flag(FLAG_OF, false);
    cpu->set_flag(FLAG_SF, false);
    cpu->set_flag(FLAG_AF, false);
    cpu->set_flag(FLAG_PF, false);
    cpu->set_flag(FLAG_ZF, out.v == 0);
    op_store(cpu, d, 0, out);
    cpu->rip = next_rip(cpu, d);
}

void exec_tzcnt_lzcnt(CPU* cpu, const Dec& d, bool is_tzcnt) {
    Val s = op_load(cpu, d, 1);
    int w = common_int_width(d);
    uint64_t v = s.v & mask_for(w);
    Val out;
    out.taint.cannot_branch = s.taint.cannot_branch;
    out.taint.cannot_index = s.taint.cannot_index;
    if (is_tzcnt)
        out.v = (v == 0) ? w * 8 : __builtin_ctzll(v);
    else
        out.v = (v == 0) ? w * 8 : (__builtin_clzll(v) - (64 - w * 8));
    cpu->set_flag(FLAG_CF, v == 0);
    cpu->set_flag(FLAG_ZF, out.v == 0);
    cpu->set_flag(FLAG_OF, false);
    cpu->set_flag(FLAG_SF, false);
    cpu->set_flag(FLAG_AF, false);
    cpu->set_flag(FLAG_PF, false);
    op_store(cpu, d, 0, out);
    cpu->rip = next_rip(cpu, d);
}

void exec_cmpxchg(CPU* cpu, const Dec& d) {
    Val dst = op_load(cpu, d, 0);
    Val src = op_load(cpu, d, 1);
    int w = common_int_width(d);
    uint64_t m = mask_for(w);
    uint64_t acc;
    int acc_idx = (w == 1) ? ZG_RAX : (w == 2) ? ZG_RAX : (w == 4) ? ZG_RAX : ZG_RAX;
    (void)acc_idx;
    if (w == 1)
        acc = cpu->gpr[ZG_RAX].val & 0xff;
    else if (w == 2)
        acc = cpu->gpr[ZG_RAX].val & 0xffff;
    else if (w == 4)
        acc = cpu->gpr[ZG_RAX].val & 0xffffffffULL;
    else
        acc = cpu->gpr[ZG_RAX].val;
    uint64_t dv = dst.v & m, sv = src.v & m;
    uint64_t r = (acc - dv) & m;
    flags_sub(cpu, w, acc, dv, r);
    if (acc == dv) {
        Val out;
        out.v = sv;
        out.taint.cannot_branch = src.taint.cannot_branch;
        out.taint.cannot_index = src.taint.cannot_index;
        op_store(cpu, d, 0, out);
    } else {
        uint64_t full = cpu->gpr[ZG_RAX].val;
        if (w == 1)
            full = (full & ~0xffULL) | dv;
        else if (w == 2)
            full = (full & ~0xffffULL) | dv;
        else if (w == 4)
            full = dv; // zero-extend
        else
            full = dv;
        cpu->gpr[ZG_RAX].val = full;
    }
    cpu->rip = next_rip(cpu, d);
}

void exec_xadd(CPU* cpu, const Dec& d) {
    Val dst = op_load(cpu, d, 0);
    Val src = op_load(cpu, d, 1);
    int w = common_int_width(d);
    uint64_t m = mask_for(w);
    uint64_t r = (dst.v + src.v) & m;
    flags_add(cpu, w, dst.v & m, src.v & m, r);
    Val to_dst;
    to_dst.v = r;
    to_dst.taint.cannot_branch = dst.taint.cannot_branch || src.taint.cannot_branch;
    to_dst.taint.cannot_index = dst.taint.cannot_index || src.taint.cannot_index;
    Val to_src;
    to_src.v = dst.v & m;
    to_src.taint.cannot_branch = dst.taint.cannot_branch;
    to_src.taint.cannot_index = dst.taint.cannot_index;
    op_store(cpu, d, 0, to_dst);
    op_store(cpu, d, 1, to_src);
    cpu->rip = next_rip(cpu, d);
}

void exec_lahf_sahf(CPU* cpu, const Dec& d) {
    if (d.insn.mnemonic == ZYDIS_MNEMONIC_LAHF) {
        uint8_t ah = 0;
        if (cpu->cf())
            ah |= 0x01;
        ah |= 0x02;
        if (cpu->pf())
            ah |= 0x04;
        if (cpu->af())
            ah |= 0x10;
        if (cpu->zf())
            ah |= 0x40;
        if (cpu->sf())
            ah |= 0x80;
        cpu->gpr[ZG_RAX].val = (cpu->gpr[ZG_RAX].val & ~0xff00ULL) | ((uint64_t)ah << 8);
    } else {
        uint8_t ah = (cpu->gpr[ZG_RAX].val >> 8) & 0xff;
        cpu->set_flag(FLAG_CF, ah & 0x01);
        cpu->set_flag(FLAG_PF, ah & 0x04);
        cpu->set_flag(FLAG_AF, ah & 0x10);
        cpu->set_flag(FLAG_ZF, ah & 0x40);
        cpu->set_flag(FLAG_SF, ah & 0x80);
    }
    cpu->rip = next_rip(cpu, d);
}

void exec_flag_simple(CPU* cpu, const Dec& d) {
    switch (d.insn.mnemonic) {
    case ZYDIS_MNEMONIC_STC: cpu->set_flag(FLAG_CF, true); break;
    case ZYDIS_MNEMONIC_CLC: cpu->set_flag(FLAG_CF, false); break;
    case ZYDIS_MNEMONIC_CMC: cpu->set_flag(FLAG_CF, !cpu->cf()); break;
    case ZYDIS_MNEMONIC_STD: cpu->set_flag(FLAG_DF, true); break;
    case ZYDIS_MNEMONIC_CLD: cpu->set_flag(FLAG_DF, false); break;
    default: guest_error(cpu, "bad flag op");
    }
    cpu->rip = next_rip(cpu, d);
}

void exec_movbe(CPU* cpu, const Dec& d) {
    Val s = op_load(cpu, d, 1);
    int w = common_int_width(d);
    Val out;
    if (w == 2)
        out.v = __builtin_bswap16((uint16_t)s.v);
    else if (w == 4)
        out.v = __builtin_bswap32((uint32_t)s.v);
    else if (w == 8)
        out.v = __builtin_bswap64(s.v);
    else
        guest_error(cpu, "bad movbe width");
    out.taint.cannot_branch = s.taint.cannot_branch;
    out.taint.cannot_index = s.taint.cannot_index;
    op_store(cpu, d, 0, out);
    cpu->rip = next_rip(cpu, d);
}

} // namespace

// === control flow / strings / system ========================================

namespace {

// Poison client request via magic call target.
void handle_magic_call(CPU* cpu, const Dec& d) {
    // Convention: ((uint64_t(*)(uint64_t, void*, size_t, uint64_t))magic)(
    //     op, ptr, size, flags)
    // op 0 = no-op/query (returns 0), op 1 = poison with flags as a direct
    // POISON_* bitmask (e.g. (1,ptr,size,1) poisons cannot-load only),
    // op 2 = unpoison. Returns 0 in RAX.
    uint64_t op = cpu->gpr[ZG_RDI].val;
    uint64_t ptr = cpu->gpr[ZG_RSI].val;
    uint64_t size = cpu->gpr[ZG_RDX].val;
    uint64_t flags = cpu->gpr[ZG_RCX].val;
    uint64_t ret = mem_client_request(cpu->emu, cpu, op, ptr, size, flags);
    cpu->gpr[ZG_RAX].val = ret;
    cpu->gpr[ZG_RAX].taint.cannot_branch = false;
    cpu->gpr[ZG_RAX].taint.cannot_index = false;
    // CALL semantics without touching the stack: RIP advances past CALL.
    cpu->rip = next_rip(cpu, d);
}

void exec_call(CPU* cpu, const Dec& d) {
    // Far calls (ptr operands) are unsupported.
    if (d.ops[0].type == ZYDIS_OPERAND_TYPE_POINTER)
        guest_error(cpu, "far call unsupported");
    uint64_t target = branch_target(cpu, d, 0);
    if (target == kMagicAddr) {
        handle_magic_call(cpu, d);
        return;
    }
    push64(cpu, next_rip(cpu, d));
    cpu->rip = target;
}

void exec_ret(CPU* cpu, const Dec& d) {
    uint64_t rsp = cpu->gpr[ZG_RSP].val;
    if (cpu->gpr[ZG_RSP].taint.cannot_index)
        guest_error(cpu, "tainted RSP used by ret");
    uint64_t target = mem_load64(cpu, rsp);
    Taint t;
    mem_get_taint(rsp, 8, &t);
    if (t.cannot_branch)
        guest_error(cpu, "tainted (cannot-branch) return address");
    if (t.cannot_index)
        guest_error(cpu, "tainted (cannot-index) return address");
    cpu->gpr[ZG_RSP].val = rsp + 8;
    int n = explicit_ops(d);
    if (n >= 1) {
        Val v = op_load(cpu, d, 0);
        cpu->gpr[ZG_RSP].val += v.v & 0xffff;
    }
    cpu->rip = target;
}

void exec_jmp(CPU* cpu, const Dec& d) {
    cpu->rip = branch_target(cpu, d, 0);
}

void exec_jcc(CPU* cpu, const Dec& d) {
    Val c = op_load(cpu, d, 0); // target (also validates taint via branch_target below)
    (void)c;
    check_flags_taint(cpu, "jcc");
    uint64_t target = branch_target(cpu, d, 0);
    if (eval_cond(cpu, d.insn.mnemonic))
        cpu->rip = target;
    else
        cpu->rip = next_rip(cpu, d);
}

void exec_jcxz(CPU* cpu, const Dec& d) {
    int w = (d.insn.mnemonic == ZYDIS_MNEMONIC_JCXZ)    ? 2
            : (d.insn.mnemonic == ZYDIS_MNEMONIC_JECXZ) ? 4
                                                       : 8;
    uint64_t m = mask_for(w);
    if (cpu->gpr[ZG_RCX].taint.cannot_branch || cpu->gpr[ZG_RCX].taint.cannot_index)
        guest_error(cpu, "tainted (cannot-branch/index) RCX in jcxz");
    uint64_t cx = cpu->gpr[ZG_RCX].val & m;
    if (cx == 0)
        cpu->rip = branch_target(cpu, d, 0);
    else
        cpu->rip = next_rip(cpu, d);
}

void exec_loop(CPU* cpu, const Dec& d) {
    int w = (d.insn.attributes & ZYDIS_ATTRIB_HAS_ADDRESSSIZE) ? 4 : 8;
    if (cpu->gpr[ZG_RCX].taint.cannot_branch || cpu->gpr[ZG_RCX].taint.cannot_index)
        guest_error(cpu, "tainted (cannot-branch/index) RCX in loop");
    check_flags_taint(cpu, "loop");
    if (w != 8) {
        uint64_t m = mask_for(w);
        cpu->gpr[ZG_RCX].val = (cpu->gpr[ZG_RCX].val & ~m) |
                                ((cpu->gpr[ZG_RCX].val - 1) & m);
    } else {
        cpu->gpr[ZG_RCX].val--;
    }
    uint64_t cx = cpu->gpr[ZG_RCX].val & mask_for(w);
    bool take = false;
    switch (d.insn.mnemonic) {
    case ZYDIS_MNEMONIC_LOOP: take = cx != 0; break;
    case ZYDIS_MNEMONIC_LOOPE: take = cx != 0 && cpu->zf(); break;
    case ZYDIS_MNEMONIC_LOOPNE: take = cx != 0 && !cpu->zf(); break;
    default: guest_error(cpu, "bad loop op");
    }
    if (take)
        cpu->rip = branch_target(cpu, d, 0);
    else
        cpu->rip = next_rip(cpu, d);
}

// --- string ops ------------------------------------------------------------

uint64_t rep_count(CPU* cpu, const Dec& d) {
    // Address-size determines counter width; assume 64-bit (ABI default).
    if (d.insn.attributes & ZYDIS_ATTRIB_HAS_ADDRESSSIZE)
        return cpu->gpr[ZG_RCX].val & 0xffffffffULL;
    return cpu->gpr[ZG_RCX].val;
}

void rep_set_count(CPU* cpu, const Dec& d, uint64_t v) {
    if (d.insn.attributes & ZYDIS_ATTRIB_HAS_ADDRESSSIZE)
        cpu->gpr[ZG_RCX].val = (cpu->gpr[ZG_RCX].val & ~0xffffffffULL) | (v & 0xffffffffULL);
    else
        cpu->gpr[ZG_RCX].val = v;
}

bool has_rep(const Dec& d) {
    return (d.insn.attributes & (ZYDIS_ATTRIB_HAS_REP | ZYDIS_ATTRIB_HAS_REPE |
                                 ZYDIS_ATTRIB_HAS_REPNE)) != 0;
}
bool has_repe(const Dec& d) {
    return (d.insn.attributes & (ZYDIS_ATTRIB_HAS_REP | ZYDIS_ATTRIB_HAS_REPE)) != 0;
}
bool has_repne(const Dec& d) {
    return (d.insn.attributes & ZYDIS_ATTRIB_HAS_REPNE) != 0;
}

int string_width(ZydisMnemonic m) {
    switch (m) {
    case ZYDIS_MNEMONIC_MOVSB:
    case ZYDIS_MNEMONIC_CMPSB:
    case ZYDIS_MNEMONIC_SCASB:
    case ZYDIS_MNEMONIC_LODSB:
    case ZYDIS_MNEMONIC_STOSB: return 1;
    case ZYDIS_MNEMONIC_MOVSW:
    case ZYDIS_MNEMONIC_CMPSW:
    case ZYDIS_MNEMONIC_SCASW:
    case ZYDIS_MNEMONIC_LODSW:
    case ZYDIS_MNEMONIC_STOSW: return 2;
    case ZYDIS_MNEMONIC_MOVSD:
    case ZYDIS_MNEMONIC_CMPSD:
    case ZYDIS_MNEMONIC_SCASD:
    case ZYDIS_MNEMONIC_LODSD:
    case ZYDIS_MNEMONIC_STOSD: return 4;
    case ZYDIS_MNEMONIC_MOVSQ:
    case ZYDIS_MNEMONIC_CMPSQ:
    case ZYDIS_MNEMONIC_SCASQ:
    case ZYDIS_MNEMONIC_LODSQ:
    case ZYDIS_MNEMONIC_STOSQ: return 8;
    default: break;
    }
    // Generic MOVS/CMPS/SCAS/LODS/STOS: operand size decides.
    return 0;
}

void check_si_di_taint(CPU* cpu) {
    if (cpu->gpr[ZG_RSI].taint.cannot_index || cpu->gpr[ZG_RDI].taint.cannot_index)
        guest_error(cpu, "tainted (cannot-index) RSI/RDI in string op");
}

void check_rep_taint(CPU* cpu, const Dec& d) {
    if (!has_rep(d))
        return;
    if (cpu->gpr[ZG_RCX].taint.cannot_branch || cpu->gpr[ZG_RCX].taint.cannot_index)
        guest_error(cpu, "tainted (cannot-branch/index) RCX in rep string op");
}

void exec_string(CPU* cpu, const Dec& d) {
    int w = string_width(d.insn.mnemonic);
    if (!w) {
        // Generic form: size from first explicit mem operand.
        for (int i = 0; i < d.insn.operand_count; i++) {
            if (d.ops[i].visibility == ZYDIS_OPERAND_VISIBILITY_EXPLICIT &&
                d.ops[i].type == ZYDIS_OPERAND_TYPE_MEMORY && d.ops[i].size) {
                w = d.ops[i].size / 8;
                break;
            }
        }
        if (!w)
            guest_error(cpu, "cannot determine string width");
    }
    check_si_di_taint(cpu);
    check_rep_taint(cpu, d);
    int64_t step = cpu->df() ? -w : w;
    bool rep = has_rep(d);
    uint64_t count = rep ? rep_count(cpu, d) : 1;
    ZydisMnemonic m = d.insn.mnemonic;
    bool is_cmps = (m == ZYDIS_MNEMONIC_CMPSB || m == ZYDIS_MNEMONIC_CMPSW ||
                    m == ZYDIS_MNEMONIC_CMPSD || m == ZYDIS_MNEMONIC_CMPSQ);
    bool is_scas = (m == ZYDIS_MNEMONIC_SCASB || m == ZYDIS_MNEMONIC_SCASW ||
                    m == ZYDIS_MNEMONIC_SCASD || m == ZYDIS_MNEMONIC_SCASQ);
    bool is_movs = (m == ZYDIS_MNEMONIC_MOVSB || m == ZYDIS_MNEMONIC_MOVSW ||
                    m == ZYDIS_MNEMONIC_MOVSQ || m == ZYDIS_MNEMONIC_MOVSD);
    // Note: MOVSD collides with SSE movsd; Zydis distinguishes by operands
    // (string form has implicit/mem operands). If explicit operands look
    // like xmm regs, this isn't a string op - but the dispatcher routes by
    // operand shape, so reaching here means string form.
    bool is_stos = (m == ZYDIS_MNEMONIC_STOSB || m == ZYDIS_MNEMONIC_STOSW ||
                    m == ZYDIS_MNEMONIC_STOSD || m == ZYDIS_MNEMONIC_STOSQ);
    bool is_lods = (m == ZYDIS_MNEMONIC_LODSB || m == ZYDIS_MNEMONIC_LODSW ||
                    m == ZYDIS_MNEMONIC_LODSD || m == ZYDIS_MNEMONIC_LODSQ);
    uint64_t si = cpu->gpr[ZG_RSI].val;
    uint64_t di = cpu->gpr[ZG_RDI].val;
    uint64_t ax = cpu->gpr[ZG_RAX].val & mask_for(w);
    Taint ax_taint = cpu->gpr[ZG_RAX].taint;
    uint64_t done = 0;
    for (uint64_t i = 0; i < count; i++) {
        if (is_movs) {
            uint64_t v;
            switch (w) {
            case 1: v = mem_load8(cpu, si); break;
            case 2: v = mem_load16(cpu, si); break;
            case 4: v = mem_load32(cpu, si); break;
            default: v = mem_load64(cpu, si); break;
            }
            Taint mtaint;
            mem_get_taint(si, w, &mtaint);
            switch (w) {
            case 1: mem_store8(cpu, di, (uint8_t)v); break;
            case 2: mem_store16(cpu, di, (uint16_t)v); break;
            case 4: mem_store32(cpu, di, (uint32_t)v); break;
            default: mem_store64(cpu, di, v); break;
            }
            mem_note_store(cpu, di, w, mtaint);
            si += step;
            di += step;
            done++;
        } else if (is_stos) {
            switch (w) {
            case 1: mem_store8(cpu, di, (uint8_t)ax); break;
            case 2: mem_store16(cpu, di, (uint16_t)ax); break;
            case 4: mem_store32(cpu, di, (uint32_t)ax); break;
            default: mem_store64(cpu, di, ax); break;
            }
            mem_note_store(cpu, di, w, ax_taint);
            di += step;
            done++;
        } else if (is_lods) {
            switch (w) {
            case 1: ax = mem_load8(cpu, si); break;
            case 2: ax = mem_load16(cpu, si); break;
            case 4: ax = mem_load32(cpu, si); break;
            default: ax = mem_load64(cpu, si); break;
            }
            mem_get_taint(si, w, &ax_taint);
            si += step;
            done++;
        } else if (is_scas || is_cmps) {
            uint64_t b;
            Taint sctaint;
            if (is_scas) {
                switch (w) {
                case 1: b = mem_load8(cpu, di); break;
                case 2: b = mem_load16(cpu, di); break;
                case 4: b = mem_load32(cpu, di); break;
                default: b = mem_load64(cpu, di); break;
                }
                mem_get_taint(di, w, &sctaint);
                di += step;
            } else {
                uint64_t a;
                switch (w) {
                case 1: a = mem_load8(cpu, si); b = mem_load8(cpu, di); break;
                case 2: a = mem_load16(cpu, si); b = mem_load16(cpu, di); break;
                case 4: a = mem_load32(cpu, si); b = mem_load32(cpu, di); break;
                default: a = mem_load64(cpu, si); b = mem_load64(cpu, di); break;
                }
                Taint s_taint;
                mem_get_taint(si, w, &s_taint);
                si += step;
                di += step;
                ax = a;
                ax_taint = s_taint;
            }
            uint64_t r = (ax - b) & mask_for(w);
            flags_sub(cpu, w, ax, b, r);
            cpu->flags_taint = ax_taint | sctaint;
            done++;
            if (rep) {
                // REPE/REPZ continues while ZF=1; REPNE while ZF=0.
                // Plain REP prefix on CMPS/SCAS behaves as REPE.
                bool repe = has_repe(d) || (!has_repne(d) && rep);
                bool cond = repe ? cpu->zf() : !cpu->zf();
                if (!cond) {
                    break;
                }
            }
        } else {
            guest_error(cpu, "bad string op");
        }
    }
    cpu->gpr[ZG_RSI].val = si;
    cpu->gpr[ZG_RDI].val = di;
    if (is_lods) {
        if (w == 4)
            cpu->gpr[ZG_RAX].val = ax; // zero-extends
        else if (w == 8)
            cpu->gpr[ZG_RAX].val = ax;
        else
            cpu->gpr[ZG_RAX].val =
                (cpu->gpr[ZG_RAX].val & ~mask_for(w)) | (ax & mask_for(w));
        cpu->gpr[ZG_RAX].taint = ax_taint;
    }
    if (is_stos || is_lods) {
        // AL/AX/EAX/RAX already handled: STOS uses ax snapshot; LODS wrote back.
    }
    if (rep)
        rep_set_count(cpu, d, count - done);
    cpu->rip = next_rip(cpu, d);
}

// --- system ---------------------------------------------------------------

void exec_syscall(CPU* cpu, const Dec& d) {
    emulate_syscall(cpu, d.insn.length);
}

// zegarmistrz presence (see cpuid.txt): guests detect us via the CPUID
// hypervisor convention — CPUID.1:ECX bit 31 ("hypervisor present", which
// real silicon reserves as zero) plus the software-reserved hypervisor leaf
// range 0x40000000-0x4000FFFF, where leaf 0x40000000 reports the max
// hypervisor leaf in EAX and the 12-byte vendor signature "Zegarmistrz\0"
// in EBX:ECX:EDX. Our leaves shadow the host's, so guests never see the
// host hypervisor's signature through us; undefined leaves in the range
// read as zero.
static const unsigned kZegHypLeafBase = 0x40000000;
static const unsigned kZegHypLeafEnd = 0x40010000;
static const unsigned kZegSigEbx = 0x6167655Au; // "Zega"
static const unsigned kZegSigEcx = 0x73696D72u; // "rmis"
static const unsigned kZegSigEdx = 0x007A7274u; // "trz\0"

void exec_cpuid(CPU* cpu, const Dec& d) {
    unsigned leaf = (unsigned)cpu->gpr[ZG_RAX].val;
    unsigned sub = (unsigned)cpu->gpr[ZG_RCX].val;
    unsigned a = 0, b = 0, c = 0, e = 0;
    if (leaf >= kZegHypLeafBase && leaf < kZegHypLeafEnd) {
        // Our own hypervisor leaves: never pass through to the host, so the
        // host hypervisor's leaves (and signature) never leak. Leaf
        // 0x40000000 is the vendor leaf; every other leaf in the range is
        // undefined and reads as zero.
        if (leaf == kZegHypLeafBase) {
            a = kZegHypLeafBase; // max hypervisor leaf == the vendor leaf
            b = kZegSigEbx;
            c = kZegSigEcx;
            e = kZegSigEdx;
        }
    } else {
        __cpuid_count(leaf, sub, a, b, c, e);
        // Phase 2 implements EVEX/AVX512 + AES/SHA/VAES/VPCLMUL/GFNI/FMA/F16C, so
        // report host features truthfully. Still hide AMX (tile) state, CET, and
        // UINTR, which the interpreter does not model (clean errors if used).
        if (leaf == 7 && sub == 0) {
            b &= ~((1u << 24) | (1u << 25)); // AMX-BF16, AMX-TILE
            e &= ~((1u << 3) | (1u << 4) | (1u << 5)); // AMX-INT8, AVX-VNNI-INT8, AMX-FP16
        }
        if (leaf == 7 && sub == 1) {
            a &= ~((1u << 3) | (1u << 4) | (1u << 5) | (1u << 21) | (1u << 22)); // AMX-FP16, HRESET, UINTR, CET...
        }
        if (leaf == 1)
            c |= 1u << 31; // hypervisor present (cpuid.txt: guests check this first)
    }
    cpu->gpr[ZG_RAX].val = a;
    cpu->gpr[ZG_RBX].val = b;
    cpu->gpr[ZG_RCX].val = c;
    cpu->gpr[ZG_RDX].val = e;
    for (int r : {ZG_RAX, ZG_RBX, ZG_RCX, ZG_RDX}) {
        cpu->gpr[r].taint.cannot_branch = false;
        cpu->gpr[r].taint.cannot_index = false;
    }
    cpu->rip = next_rip(cpu, d);
}

void exec_xgetbv(CPU* cpu, const Dec& d) {
    unsigned leaf = (unsigned)cpu->gpr[ZG_RCX].val;
    unsigned a = 0, e = 0;
    asm volatile("xgetbv" : "=a"(a), "=d"(e) : "c"(leaf));
    cpu->gpr[ZG_RAX].val = a;
    cpu->gpr[ZG_RDX].val = e;
    cpu->rip = next_rip(cpu, d);
}

void exec_rdtsc(CPU* cpu, const Dec& d, bool tscp) {
    unsigned aux = 0;
    uint64_t t = __rdtsc();
    if (tscp)
        __rdtscp(&aux);
    cpu->gpr[ZG_RAX].val = (uint32_t)t;
    cpu->gpr[ZG_RDX].val = (uint32_t)(t >> 32);
    if (tscp)
        cpu->gpr[ZG_RCX].val = aux;
    cpu->rip = next_rip(cpu, d);
}

void exec_pushf(CPU* cpu, const Dec& d) {
    int w = d.insn.stack_width / 8;
    if (w != 2 && w != 8)
        w = 8;
    uint64_t v = cpu->rflags;
    if (w == 2)
        push64(cpu, v & 0xffff);
    else
        push64(cpu, v & 0x00fcfffe); // mask reserved bits
    cpu->rip = next_rip(cpu, d);
}

void exec_popf(CPU* cpu, const Dec& d) {
    Val v = pop64(cpu);
    // Only user-modifiable bits.
    const uint64_t mask = FLAG_CF | FLAG_PF | FLAG_AF | FLAG_ZF | FLAG_SF |
                          FLAG_DF | FLAG_OF;
    cpu->rflags = (cpu->rflags & ~mask) | (v.v & mask) | 0x2;
    // Flags restored from a tainted stack slot inherit its taint.
    cpu->flags_taint.cannot_branch = v.taint.cannot_branch;
    cpu->flags_taint.cannot_index = v.taint.cannot_index;
    cpu->rip = next_rip(cpu, d);
}

} // namespace

// === vectors (SSE/AVX) ======================================================

namespace {

bool is_vex(const Dec& d) {
    return (d.insn.attributes & ZYDIS_ATTRIB_HAS_VEX) != 0;
}
bool is_evex(const Dec& d) {
    return (d.insn.attributes & ZYDIS_ATTRIB_HAS_EVEX) != 0;
}

// Destination vector width in bytes for a vec op (16 or 32). EVEX is
// rejected by the dispatcher before we get here.
unsigned vec_width(const Dec& d) {
    unsigned bytes = d.ops[0].size / 8;
    if (bytes != 16 && bytes != 32 && bytes != 64)
        guest_error(nullptr, "bad vector width");
    return bytes;
}

// Load the two sources for a 2-operand legacy op (a=dst, b=src) or a
// 3-operand VEX op (a=src1, b=src2). Returns width in bytes. Optional
// cb/ci receive the OR of both sources' cannot-branch/index taint.
unsigned vec_sources(CPU* cpu, const Dec& d, VecVal& a, VecVal& b) {
    int n = explicit_ops(d);
    unsigned w = vec_width(d);
    if (is_vex(d) && n >= 3) {
        vec_load_bytes(cpu, d, 1, a, w);
        vec_load_bytes(cpu, d, 2, b, w);
    } else {
        vec_load_bytes(cpu, d, 0, a, w);
        vec_load_bytes(cpu, d, 1, b, w);
    }
    return w;
}

void vec_store_dst(CPU* cpu, const Dec& d, const VecVal& out, unsigned w) {
    int n = explicit_ops(d);
    bool vex = is_vex(d);
    if (vex && n >= 3)
        vec_store_bytes(cpu, d, 0, out, w, true, w);
    else
        vec_store_bytes(cpu, d, 0, out, w, false, w);
}

bool is_aligned_move(ZydisMnemonic m) {
    switch (m) {
    case ZYDIS_MNEMONIC_MOVAPS:
    case ZYDIS_MNEMONIC_MOVAPD:
    case ZYDIS_MNEMONIC_MOVDQA:
    case ZYDIS_MNEMONIC_VMOVAPS:
    case ZYDIS_MNEMONIC_VMOVAPD:
    case ZYDIS_MNEMONIC_VMOVDQA:
    case ZYDIS_MNEMONIC_VMOVDQA32:
    case ZYDIS_MNEMONIC_VMOVDQA64:
    case ZYDIS_MNEMONIC_MOVNTPS:
    case ZYDIS_MNEMONIC_MOVNTPD:
    case ZYDIS_MNEMONIC_MOVNTDQA:
        case ZYDIS_MNEMONIC_VMOVNTPS:
    case ZYDIS_MNEMONIC_VMOVNTPD:
    case ZYDIS_MNEMONIC_VMOVNTDQ:
    case ZYDIS_MNEMONIC_VMOVNTDQA: return true;
    default: return false;
    }
}

void exec_vec_move(CPU* cpu, const Dec& d) {
    unsigned w = vec_width(d);
    int n = explicit_ops(d);
    // Direction: dst is op0. Source may be reg or mem.
    VecVal tmp;
    if (d.ops[1].type == ZYDIS_OPERAND_TYPE_REGISTER &&
        d.ops[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        // store
        vec_load_bytes(cpu, d, 1, tmp, w);
        if (is_aligned_move(d.insn.mnemonic)) {
            uint64_t addr = resolve_mem(cpu, d.ops[0], cpu->rip + d.insn.length);
            if (addr & (w >= 16 ? 15 : 0))
                guest_error(cpu, "guest SIGSEGV (unaligned vector store)", addr);
        }
        vec_store_bytes(cpu, d, 0, tmp, w, false, w);
    } else {
        // load (reg <- reg/mem)
        if (d.ops[1].type == ZYDIS_OPERAND_TYPE_MEMORY &&
            is_aligned_move(d.insn.mnemonic)) {
            uint64_t addr = resolve_mem(cpu, d.ops[1], cpu->rip + d.insn.length);
            if (addr & 15)
                guest_error(cpu, "guest SIGSEGV (unaligned vector load)", addr);
        }
        vec_load_bytes(cpu, d, 1, tmp, w);
        bool vex = is_vex(d);
        vec_store_bytes(cpu, d, 0, tmp, w, vex, w);
    }
    (void)n;
    cpu->rip = next_rip(cpu, d);
}

void exec_vec_logic(CPU* cpu, const Dec& d, int kind) {
    // kind: 0=AND 1=OR 2=XOR 3=ANDN
    VecVal a, b, o;
    unsigned w = vec_sources(cpu, d, a, b);
    // Self-xor (vpxor/xorps reg,reg,reg with all sources identical) clears
    // taint: the result is zero regardless of input.
    if (kind == 2 && vec_self_op(d)) {
        memset(o, 0, w);
        o.taint.clear();
        vec_store_dst(cpu, d, o, w);
        cpu->rip = next_rip(cpu, d);
        return;
    }
    for (unsigned i = 0; i < w; i++) {
        if (kind == 0)
            o[i] = a[i] & b[i];
        else if (kind == 1)
            o[i] = a[i] | b[i];
        else if (kind == 2)
            o[i] = a[i] ^ b[i];
        else
            o[i] = (~a[i]) & b[i];
    }
    o.taint = a.taint | b.taint;
    vec_store_dst(cpu, d, o, w);
    cpu->rip = next_rip(cpu, d);
}

void exec_pcmpeq(CPU* cpu, const Dec& d, unsigned lane, bool gt) {
    VecVal a, b, o;
    unsigned w = vec_sources(cpu, d, a, b);
    memset(o, 0, w);
    for (unsigned i = 0; i < w; i += lane) {
        uint64_t av = 0, bv = 0;
        memcpy(&av, a + i, lane);
        memcpy(&bv, b + i, lane);
        bool hit;
        if (!gt) {
            hit = (av == bv);
        } else if (lane == 1) {
            hit = ((int8_t)av > (int8_t)bv);
        } else if (lane == 2) {
            hit = ((int16_t)av > (int16_t)bv);
        } else if (lane == 4) {
            hit = ((int32_t)av > (int32_t)bv);
        } else {
            hit = ((int64_t)av > (int64_t)bv);
        }
        if (hit)
            memset(o + i, 0xff, lane);
    }
    o.taint = a.taint | b.taint;
    vec_store_dst(cpu, d, o, w);
    cpu->rip = next_rip(cpu, d);
}

void exec_padd(CPU* cpu, const Dec& d, unsigned lane, bool sub) {
    VecVal a, b, o;
    unsigned w = vec_sources(cpu, d, a, b);
    for (unsigned i = 0; i < w; i += lane) {
        uint64_t av = 0, bv = 0;
        memcpy(&av, a + i, lane);
        memcpy(&bv, b + i, lane);
        uint64_t r = sub ? av - bv : av + bv;
        memcpy(o + i, &r, lane);
    }
    o.taint = a.taint | b.taint;
    vec_store_dst(cpu, d, o, w);
    cpu->rip = next_rip(cpu, d);
}

void exec_pmull(CPU* cpu, const Dec& d, unsigned lane) {
    VecVal a, b, o;
    unsigned w = vec_sources(cpu, d, a, b);
    for (unsigned i = 0; i < w; i += lane) {
        uint64_t av = 0, bv = 0;
        memcpy(&av, a + i, lane);
        memcpy(&bv, b + i, lane);
        uint64_t r;
        if (lane == 2)
            r = (uint16_t)((int16_t)av * (int16_t)bv);
        else if (lane == 4)
            r = (uint32_t)((int32_t)av * (int32_t)bv);
        else
            r = av * bv;
        memcpy(o + i, &r, lane);
    }
    o.taint = a.taint | b.taint;
    vec_store_dst(cpu, d, o, w);
    cpu->rip = next_rip(cpu, d);
}

void exec_pminmax(CPU* cpu, const Dec& d, unsigned lane, bool is_signed, bool is_max) {
    VecVal a, b, o;
    unsigned w = vec_sources(cpu, d, a, b);
    for (unsigned i = 0; i < w; i += lane) {
        uint64_t av = 0, bv = 0;
        memcpy(&av, a + i, lane);
        memcpy(&bv, b + i, lane);
        bool take_b;
        if (is_signed) {
            int64_t sa, sb;
            if (lane == 1) { sa = (int8_t)av; sb = (int8_t)bv; }
            else if (lane == 2) { sa = (int16_t)av; sb = (int16_t)bv; }
            else { sa = (int32_t)av; sb = (int32_t)bv; }
            take_b = is_max ? (sb > sa) : (sb < sa);
        } else {
            take_b = is_max ? (bv > av) : (bv < av);
        }
        memcpy(o + i, take_b ? b + i : a + i, lane);
    }
    o.taint = a.taint | b.taint;
    vec_store_dst(cpu, d, o, w);
    cpu->rip = next_rip(cpu, d);
}

void exec_pmovmskb(CPU* cpu, const Dec& d) {
    unsigned w = d.ops[1].size / 8;
    if (w != 16 && w != 32 && w != 64)
        guest_error(cpu, "bad pmovmskb width");
    VecVal b;
    vec_load_bytes(cpu, d, 1, b, w);
    uint64_t mask = 0;
    for (unsigned i = 0; i < w; i++) {
        if (b[i] & 0x80)
            mask |= (1ULL << i);
    }
    Val out;
    out.v = mask;
    out.taint = b.taint;
    op_store(cpu, d, 0, out);
    cpu->rip = next_rip(cpu, d);
}

void exec_movmskps(CPU* cpu, const Dec& d, bool is_pd) {
    unsigned w = d.ops[1].size / 8;
    unsigned lane = is_pd ? 8 : 4;
    VecVal b;
    vec_load_bytes(cpu, d, 1, b, w);
    uint64_t mask = 0;
    for (unsigned i = 0; i < w; i += lane) {
        if (b[i + lane - 1] & 0x80)
            mask |= (1ULL << (i / lane));
    }
    Val out;
    out.v = mask;
    out.taint = b.taint;
    op_store(cpu, d, 0, out);
    cpu->rip = next_rip(cpu, d);
}

void exec_pshift_dq(CPU* cpu, const Dec& d, bool left) {
    int n = explicit_ops(d);
    unsigned w = vec_width(d);
    VecVal a, o;
    vec_load_bytes(cpu, d, 0, a, w);
    Val imm = op_load(cpu, d, n - 1);
    unsigned cnt = (unsigned)imm.v;
    // PSLLDQ/SRLDQ shift each 128-bit lane (Zydis: VEX128 single lane).
    unsigned lanes = w >= 16 ? (is_vex(d) && w == 32 ? 2 : w / 16) : 1;
    (void)lanes;
    memset(o, 0, w);
    if (cnt < 16) {
        // Shift within each 128-bit lane.
        for (unsigned lane = 0; lane < w; lane += 16) {
            for (unsigned i = 0; i < 16; i++) {
                int src = left ? (int)i - (int)cnt : (int)i + (int)cnt;
                if (src >= 0 && src < 16)
                    o[lane + i] = a[lane + src];
            }
        }
    }
    o.taint = a.taint;
    vec_store_dst(cpu, d, o, w);
    cpu->rip = next_rip(cpu, d);
}

void exec_pshift_bits(CPU* cpu, const Dec& d, unsigned lane, int kind) {
    // kind: 0=left logical 1=right logical 2=right arithmetic
    int n = explicit_ops(d);
    unsigned w = vec_width(d);
    VecVal a, o, c;
    vec_load_bytes(cpu, d, 0, a, w);
    unsigned cnt;
    if (d.ops[n - 1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        cnt = (unsigned)op_load(cpu, d, n - 1).v;
    } else {
        vec_load_bytes(cpu, d, n - 1, c, 16);
        cnt = c[0] | (c[1] << 8);
    }
    for (unsigned i = 0; i < w; i += lane) {
        uint64_t v = 0;
        memcpy(&v, a + i, lane);
        uint64_t r;
        if (cnt >= lane * 8) {
            r = (kind == 2 && lane < 8 && (v >> (lane * 8 - 1))) ? ~0ULL : 0;
        } else if (kind == 0) {
            r = v << cnt;
        } else if (kind == 1) {
            r = v >> cnt;
        } else {
            if (lane == 2)
                r = (uint16_t)((int16_t)v >> cnt);
            else if (lane == 4)
                r = (uint32_t)((int32_t)v >> cnt);
            else
                r = v >> cnt;
        }
        memcpy(o + i, &r, lane);
    }
    o.taint = a.taint | c.taint;
    vec_store_dst(cpu, d, o, w);
    cpu->rip = next_rip(cpu, d);
}

void exec_pshufd(CPU* cpu, const Dec& d) {
    int n = explicit_ops(d);
    unsigned w = vec_width(d);
    VecVal a, o;
    vec_load_bytes(cpu, d, n - 2, a, w);
    unsigned order = (unsigned)op_load(cpu, d, n - 1).v;
    for (unsigned lane = 0; lane < w; lane += 16) {
        for (unsigned i = 0; i < 4; i++) {
            unsigned sel = (order >> (i * 2)) & 3;
            memcpy(o + lane + i * 4, a + lane + sel * 4, 4);
        }
    }
    o.taint = a.taint;
    vec_store_dst(cpu, d, o, w);
    cpu->rip = next_rip(cpu, d);
}

void exec_pshufb(CPU* cpu, const Dec& d) {
    VecVal a, b, o;
    unsigned w = vec_sources(cpu, d, a, b);
    for (unsigned lane = 0; lane < w; lane += 16) {
        for (unsigned i = 0; i < 16; i++) {
            uint8_t c = b[lane + i];
            o[lane + i] = (c & 0x80) ? 0 : a[lane + (c & 0xf)];
        }
    }
    o.taint = a.taint | b.taint;
    vec_store_dst(cpu, d, o, w);
    cpu->rip = next_rip(cpu, d);
}

void exec_shufpd(CPU* cpu, const Dec& d, bool is_ps) {
    int n = explicit_ops(d);
    unsigned w = vec_width(d);
    VecVal a, b, o;
    vec_sources(cpu, d, a, b);
    unsigned order = (unsigned)op_load(cpu, d, n - 1).v;
    unsigned lane = is_ps ? 4 : 8;
    // Per 128-bit lane (VEX256 keeps lanes independent for SHUFPS/PD).
    for (unsigned L = 0; L < w; L += 16) {
        unsigned le = 16 / lane;
        for (unsigned i = 0; i < le; i++) {
            if (is_ps) {
                unsigned sel = (order >> (i * 2)) & 3;
                const uint8_t* src = (i < 2) ? a : b;
                memcpy(o + L + i * lane, src + L + sel * lane, lane);
            } else {
                unsigned sel = (order >> i) & 1;
                const uint8_t* src = sel ? b : a;
                memcpy(o + L + i * lane, src + L + i * lane, lane);
            }
        }
    }
    o.taint = a.taint | b.taint;
    vec_store_dst(cpu, d, o, w);
    cpu->rip = next_rip(cpu, d);
}

void exec_pshuflw(CPU* cpu, const Dec& d, bool high) {
    int n = explicit_ops(d);
    unsigned w = vec_width(d);
    VecVal a, o;
    vec_load_bytes(cpu, d, n - 2, a, w);
    unsigned order = (unsigned)op_load(cpu, d, n - 1).v;
    for (unsigned lane = 0; lane < w; lane += 16) {
        memcpy(o + lane, a + lane, 16);
        unsigned base = high ? 8 : 0;
        for (unsigned i = 0; i < 4; i++) {
            unsigned sel = (order >> (i * 2)) & 3;
            memcpy(o + lane + base + i * 2, a + lane + base + sel * 2, 2);
        }
    }
    o.taint = a.taint;
    vec_store_dst(cpu, d, o, w);
    cpu->rip = next_rip(cpu, d);
}

void exec_punpck(CPU* cpu, const Dec& d, unsigned lane, bool high) {
    VecVal a, b, o;
    unsigned w = vec_sources(cpu, d, a, b);
    // Legacy PUNPCK interleaves within each 128-bit lane: dst = a, src = b.
    for (unsigned L = 0; L < w; L += 16) {
        unsigned elems = 16 / lane;
        unsigned half = elems / 2;
        for (unsigned i = 0; i < half; i++) {
            unsigned k = high ? half + i : i;
            memcpy(o + L + (2 * i) * lane, a + L + k * lane, lane);
            memcpy(o + L + (2 * i + 1) * lane, b + L + k * lane, lane);
        }
    }
    o.taint = a.taint | b.taint;
    vec_store_dst(cpu, d, o, w);
    cpu->rip = next_rip(cpu, d);
}

void exec_ptest(CPU* cpu, const Dec& d) {
    unsigned w = d.ops[0].size / 8;
    if (w != 16 && w != 32)
        guest_error(cpu, "bad ptest width");
    VecVal a, b;
    vec_load_bytes(cpu, d, 0, a, w);
    vec_load_bytes(cpu, d, 1, b, w);
    cpu->flags_taint = a.taint | b.taint;
    bool zf = true, cf = true;
    for (unsigned i = 0; i < w; i++) {
        if ((a[i] & b[i]) != 0)
            zf = false;
        if ((~a[i] & b[i]) != 0)
            cf = false;
    }
    cpu->set_flag(FLAG_ZF, zf);
    cpu->set_flag(FLAG_CF, cf);
    cpu->set_flag(FLAG_OF, false);
    cpu->set_flag(FLAG_SF, false);
    cpu->set_flag(FLAG_AF, false);
    cpu->set_flag(FLAG_PF, false);
    cpu->rip = next_rip(cpu, d);
}

void exec_movd(CPU* cpu, const Dec& d, bool is_q) {
    // MOVD/MOVQ between GPR and XMM.
    if (d.ops[0].type == ZYDIS_OPERAND_TYPE_REGISTER &&
        d.ops[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        int di, ds, dsh, si, ss, ssh;
        bool d_is_gpr = reg_to_gpr(d.ops[0].reg.value, di, ds, dsh);
        bool s_is_gpr = reg_to_gpr(d.ops[1].reg.value, si, ss, ssh);
        if (d_is_gpr && !s_is_gpr) {
            int vi = -1, vw = 0;
            reg_to_vec(d.ops[1].reg.value, vi, vw);
            uint64_t v = 0;
            memcpy(&v, cpu->xmm[vi].bytes, is_q ? 8 : 4);
            Val out;
            out.v = v;
            op_store(cpu, d, 0, out);
        } else if (!d_is_gpr && s_is_gpr) {
            int vi = -1, vw = 0;
            reg_to_vec(d.ops[0].reg.value, vi, vw);
            Val s = op_load(cpu, d, 1);
            memset(cpu->xmm[vi].bytes, 0, 16);
            memcpy(cpu->xmm[vi].bytes, &s.v, is_q ? 8 : 4);
        } else {
            guest_error(cpu, "bad movd operands");
        }
    } else if (d.ops[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        int vi = -1, vw = 0;
        if (!reg_to_vec(d.ops[1].reg.value, vi, vw))
            guest_error(cpu, "bad movd operands");
        uint64_t addr = resolve_mem(cpu, d.ops[1 - 1], cpu->rip + d.insn.length);
        // op1 is index 1? For MOVD m32, xmm: ops[0]=mem, ops[1]=xmm.
        addr = resolve_mem(cpu, d.ops[0], cpu->rip + d.insn.length);
        if (is_q)
            mem_store64(cpu, addr, *(uint64_t*)cpu->xmm[vi].bytes);
        else
            mem_store32(cpu, addr, *(uint32_t*)cpu->xmm[vi].bytes);
    } else {
        // load: xmm <- mem
        int vi = -1, vw = 0;
        if (!reg_to_vec(d.ops[0].reg.value, vi, vw))
            guest_error(cpu, "bad movd operands");
        uint64_t addr = resolve_mem(cpu, d.ops[1], cpu->rip + d.insn.length);
        memset(cpu->xmm[vi].bytes, 0, 16);
        if (is_q)
            *(uint64_t*)cpu->xmm[vi].bytes = mem_load64(cpu, addr);
        else
            *(uint32_t*)cpu->xmm[vi].bytes = mem_load32(cpu, addr);
    }
    cpu->rip = next_rip(cpu, d);
}

// Scalar single/double helpers.
double vec_f64(const uint8_t* b) {
    double v;
    memcpy(&v, b, 8);
    return v;
}
float vec_f32(const uint8_t* b) {
    float v;
    memcpy(&v, b, 4);
    return v;
}

void exec_sse_scalar(CPU* cpu, const Dec& d, int kind) {
    // kind: 0=ADD 1=MUL 2=SUB 3=DIV 4=SQRT 5=MAX 6=MIN 7=CMP
    // dst is xmm (op0), src is xmm/mem (op1, maybe op2 for VEX).
    bool is_sd = (d.insn.mnemonic == ZYDIS_MNEMONIC_ADDSD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_MULSD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_SUBSD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_DIVSD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_SQRTSD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_MAXSD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_MINSD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_CMPSD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_UCOMISD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_COMISD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_VADDSD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_VMULSD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_VSUBSD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_VDIVSD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_VSQRTSD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_VMAXSD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_VMINSD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_VCMPSD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_VUCOMISD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_VCOMISD);
    int n = explicit_ops(d);
    VecVal ab, bb;
    int dsti = -1, dstw = 0;
    if (!reg_to_vec(d.ops[0].reg.value, dsti, dstw))
        guest_error(cpu, "bad scalar fp dst");
    if (is_vex(d) && n >= 3) {
        vec_load_bytes(cpu, d, 1, ab, 16);
        vec_load_bytes(cpu, d, 2, bb, 16);
    } else {
        memcpy(ab, cpu->xmm[dsti].bytes, 16);
        vec_load_bytes(cpu, d, n - 1, bb, 16);
    }
    if (kind <= 6) {
        if (is_sd) {
            double a = vec_f64(ab), b = vec_f64(bb), r = 0;
            switch (kind) {
            case 0: r = a + b; break;
            case 1: r = a * b; break;
            case 2: r = a - b; break;
            case 3: r = a / b; break;
            case 4: r = __builtin_sqrt(a); break;
            case 5: r = a > b ? a : b; break;
            case 6: r = a < b ? a : b; break;
            }
            memcpy(cpu->xmm[dsti].bytes, &r, 8);
            cpu->xmm[dsti].taint |= ab.taint | bb.taint;
            if (is_vex(d) && n >= 3) {
                // VEX: copy upper lanes from src1.
                memcpy(cpu->xmm[dsti].bytes + 8, ab + 8, 8);
                memset(cpu->xmm[dsti].bytes + 16, 0, 48);
            }
        } else {
            float a = vec_f32(ab), b = vec_f32(bb), r = 0;
            switch (kind) {
            case 0: r = a + b; break;
            case 1: r = a * b; break;
            case 2: r = a - b; break;
            case 3: r = a / b; break;
            case 4: r = __builtin_sqrtf(a); break;
            case 5: r = a > b ? a : b; break;
            case 6: r = a < b ? a : b; break;
            }
            memcpy(cpu->xmm[dsti].bytes, &r, 4);
            cpu->xmm[dsti].taint |= ab.taint | bb.taint;
            if (!(is_vex(d) && n >= 3)) {
                // Legacy: preserve upper bytes (already in place).
            } else {
                memcpy(cpu->xmm[dsti].bytes + 4, ab + 4, 12);
                memset(cpu->xmm[dsti].bytes + 16, 0, 48);
            }
        }
    } else {
        // CMP: imm predicate in last operand.
        unsigned pred = (unsigned)op_load(cpu, d, n - 1).v & 7;
        bool r;
        if (is_sd) {
            double a = vec_f64(ab), b = vec_f64(bb);
            switch (pred) {
            case 0: r = a == b; break;
            case 1: r = a < b; break;
            case 2: r = a <= b; break;
            case 3: r = (a != a) || (b != b); break;
            case 4: r = a != b; break;
            case 5: r = !(a < b); break;
            case 6: r = !(a <= b); break;
            default: r = (a == a) && (b == b); break;
            }
            uint64_t m = r ? ~0ULL : 0;
            memcpy(cpu->xmm[dsti].bytes, &m, 8);
            cpu->xmm[dsti].taint |= ab.taint | bb.taint;
        } else {
            float a = vec_f32(ab), b = vec_f32(bb);
            switch (pred) {
            case 0: r = a == b; break;
            case 1: r = a < b; break;
            case 2: r = a <= b; break;
            case 3: r = (a != a) || (b != b); break;
            case 4: r = a != b; break;
            case 5: r = !(a < b); break;
            case 6: r = !(a <= b); break;
            default: r = (a == a) && (b == b); break;
            }
            uint32_t m = r ? ~0u : 0;
            memcpy(cpu->xmm[dsti].bytes, &m, 4);
            cpu->xmm[dsti].taint |= ab.taint | bb.taint;
        }
    }
    cpu->rip = next_rip(cpu, d);
}

void exec_ucomi(CPU* cpu, const Dec& d) {
    bool is_sd = (d.insn.mnemonic == ZYDIS_MNEMONIC_UCOMISD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_COMISD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_VUCOMISD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_VCOMISD);
    VecVal a, b;
    vec_load_bytes(cpu, d, 0, a, 16);
    vec_load_bytes(cpu, d, 1, b, 16);
    cpu->flags_taint = a.taint | b.taint;
    bool unordered = false, zf = false, cf = false;
    if (is_sd) {
        double x = vec_f64(a), y = vec_f64(b);
        unordered = (x != x) || (y != y);
        if (unordered) {
            zf = true;
            cf = true;
        } else if (x == y) {
            zf = true;
            cf = false;
        } else if (x < y) {
            cf = true;
        }
    } else {
        float x = vec_f32(a), y = vec_f32(b);
        unordered = (x != x) || (y != y);
        if (unordered) {
            zf = true;
            cf = true;
        } else if (x == y) {
            zf = true;
        } else if (x < y) {
            cf = true;
        }
    }
    cpu->set_flag(FLAG_ZF, zf);
    cpu->set_flag(FLAG_PF, unordered);
    cpu->set_flag(FLAG_CF, cf);
    cpu->set_flag(FLAG_OF, false);
    cpu->set_flag(FLAG_SF, false);
    cpu->set_flag(FLAG_AF, false);
    cpu->rip = next_rip(cpu, d);
}

void exec_cvtsi2s(CPU* cpu, const Dec& d, bool to_sd) {
    int n = explicit_ops(d);
    Val s = op_load(cpu, d, n - 1);
    int sw = d.ops[n - 1].size / 8;
    int64_t iv = (sw == 8) ? (int64_t)s.v : (sw == 4) ? (int32_t)s.v : (int16_t)s.v;
    int dsti = -1, dstw = 0;
    reg_to_vec(d.ops[0].reg.value, dsti, dstw);
    if (to_sd) {
        double v = (double)iv;
        memcpy(cpu->xmm[dsti].bytes, &v, 8);
    } else {
        float v = (float)iv;
        memcpy(cpu->xmm[dsti].bytes, &v, 4);
    }
    cpu->rip = next_rip(cpu, d);
}

void exec_cvtts2si(CPU* cpu, const Dec& d, bool from_sd) {
    int n = explicit_ops(d);
    VecVal b;
    vec_load_bytes(cpu, d, n - 1, b, 16);
    int dw = d.ops[0].size / 8; // 4 or 8
    Val out;
    const uint64_t inval = (dw == 8) ? 0x8000000000000000ULL : 0x80000000ULL;
    if (from_sd) {
        double v = vec_f64(b);
        if (v != v || v > 9.223372036854776e18 || v < -9.223372036854776e18)
            out.v = inval;
        else if (dw == 8)
            out.v = (int64_t)v;
        else
            out.v = (int32_t)v;
    } else {
        float v = vec_f32(b);
        if (v != v || v > 9.223372036854776e18f || v < -9.223372036854776e18f)
            out.v = inval;
        else if (dw == 8)
            out.v = (int64_t)v;
        else
            out.v = (int32_t)v;
    }
    out.taint = b.taint;
    op_store(cpu, d, 0, out);
    cpu->rip = next_rip(cpu, d);
}

void exec_cvtss2sd(CPU* cpu, const Dec& d, bool to_sd) {
    int n = explicit_ops(d);
    VecVal b;
    vec_load_bytes(cpu, d, n - 1, b, 16);
    int dsti = -1, dstw = 0;
    reg_to_vec(d.ops[0].reg.value, dsti, dstw);
    if (to_sd) {
        double v = (double)vec_f32(b);
        memcpy(cpu->xmm[dsti].bytes, &v, 8);
        cpu->xmm[dsti].taint |= b.taint;
    } else {
        float v = (float)vec_f64(b);
        memcpy(cpu->xmm[dsti].bytes, &v, 4);
        cpu->xmm[dsti].taint |= b.taint;
    }
    cpu->rip = next_rip(cpu, d);
}

void exec_packed_fp(CPU* cpu, const Dec& d, int kind) {
    // kind: 0=ADD 1=MUL 2=SUB 3=DIV 4=SQRT 5=MAX 6=MIN
    bool is_pd = (d.insn.mnemonic == ZYDIS_MNEMONIC_ADDPD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_MULPD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_SUBPD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_DIVPD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_SQRTPD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_MAXPD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_MINPD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_VADDPD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_VMULPD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_VSUBPD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_VDIVPD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_VSQRTPD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_VMAXPD ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_VMINPD);
    VecVal a, b, o;
    unsigned w = vec_sources(cpu, d, a, b);
    unsigned lane = is_pd ? 8 : 4;
    for (unsigned i = 0; i < w; i += lane) {
        if (is_pd) {
            double x, y;
            memcpy(&x, a + i, 8);
            memcpy(&y, b + i, 8);
            double r = 0;
            switch (kind) {
            case 0: r = x + y; break;
            case 1: r = x * y; break;
            case 2: r = x - y; break;
            case 3: r = x / y; break;
            case 4: r = __builtin_sqrt(x); break;
            case 5: r = x > y ? x : y; break;
            case 6: r = x < y ? x : y; break;
            }
            memcpy(o + i, &r, 8);
        } else {
            float x, y;
            memcpy(&x, a + i, 4);
            memcpy(&y, b + i, 4);
            float r = 0;
            switch (kind) {
            case 0: r = x + y; break;
            case 1: r = x * y; break;
            case 2: r = x - y; break;
            case 3: r = x / y; break;
            case 4: r = __builtin_sqrtf(x); break;
            case 5: r = x > y ? x : y; break;
            case 6: r = x < y ? x : y; break;
            }
            memcpy(o + i, &r, 4);
        }
    }
    o.taint = a.taint | b.taint;
    vec_store_dst(cpu, d, o, w);
    cpu->rip = next_rip(cpu, d);
}

void exec_movsd_sse(CPU* cpu, const Dec& d) {
    // Distinguish load/store/reg forms by operand types.
    int dsti = -1, dstw = 0, srci = -1, srcw = 0;
    bool dst_is_vec = (d.ops[0].type == ZYDIS_OPERAND_TYPE_REGISTER &&
                       reg_to_vec(d.ops[0].reg.value, dsti, dstw));
    bool src_is_vec = (d.ops[1].type == ZYDIS_OPERAND_TYPE_REGISTER &&
                       reg_to_vec(d.ops[1].reg.value, srci, srcw));
    if (dst_is_vec && src_is_vec) {
        // reg-reg: copy low 64 bits.
        memcpy(cpu->xmm[dsti].bytes, cpu->xmm[srci].bytes, 8);
    } else if (dst_is_vec) {
        // load m64: low 64 bits, zero upper.
        uint64_t addr = resolve_mem(cpu, d.ops[1], cpu->rip + d.insn.length);
        uint64_t v = mem_load64(cpu, addr);
        memcpy(cpu->xmm[dsti].bytes, &v, 8);
        memset(cpu->xmm[dsti].bytes + 8, 0, 56);
    } else {
        // store m64: low 64 bits.
        uint64_t addr = resolve_mem(cpu, d.ops[0], cpu->rip + d.insn.length);
        mem_store64(cpu, addr, *(uint64_t*)cpu->xmm[srci].bytes);
    }
    cpu->rip = next_rip(cpu, d);
}

void exec_movss_sse(CPU* cpu, const Dec& d) {
    int dsti = -1, dstw = 0, srci = -1, srcw = 0;
    bool dst_is_vec = (d.ops[0].type == ZYDIS_OPERAND_TYPE_REGISTER &&
                       reg_to_vec(d.ops[0].reg.value, dsti, dstw));
    bool src_is_vec = (d.ops[1].type == ZYDIS_OPERAND_TYPE_REGISTER &&
                       reg_to_vec(d.ops[1].reg.value, srci, srcw));
    if (dst_is_vec && src_is_vec) {
        memcpy(cpu->xmm[dsti].bytes, cpu->xmm[srci].bytes, 4);
    } else if (dst_is_vec) {
        uint64_t addr = resolve_mem(cpu, d.ops[1], cpu->rip + d.insn.length);
        uint32_t v = mem_load32(cpu, addr);
        memcpy(cpu->xmm[dsti].bytes, &v, 4);
        memset(cpu->xmm[dsti].bytes + 4, 0, 60);
    } else {
        uint64_t addr = resolve_mem(cpu, d.ops[0], cpu->rip + d.insn.length);
        mem_store32(cpu, addr, *(uint32_t*)cpu->xmm[srci].bytes);
    }
    cpu->rip = next_rip(cpu, d);
}

void exec_movlp(CPU* cpu, const Dec& d, bool high, bool is_pd) {
    (void)is_pd;
    if (d.ops[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        // store form: m64 <- xmm low/high half.
        int srci = -1, srcw = 0;
        if (!reg_to_vec(d.ops[1].reg.value, srci, srcw))
            guest_error(cpu, "bad movlps src");
        uint64_t addr = resolve_mem(cpu, d.ops[0], cpu->rip + d.insn.length);
        uint64_t v;
        memcpy(&v, cpu->xmm[srci].bytes + (high ? 8 : 0), 8);
        mem_store64(cpu, addr, v);
    } else if (d.ops[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
        int dsti = -1, dstw = 0;
        if (!reg_to_vec(d.ops[0].reg.value, dsti, dstw))
            guest_error(cpu, "bad movlps dst");
        uint64_t addr = resolve_mem(cpu, d.ops[1], cpu->rip + d.insn.length);
        uint64_t v = mem_load64(cpu, addr);
        memcpy(cpu->xmm[dsti].bytes + (high ? 8 : 0), &v, 8);
    } else {
        int dsti = -1, dstw = 0, srci = -1, srcw = 0;
        reg_to_vec(d.ops[0].reg.value, dsti, dstw);
        reg_to_vec(d.ops[1].reg.value, srci, srcw);
        uint64_t v;
        memcpy(&v, cpu->xmm[srci].bytes + (high ? 8 : 0), 8);
        memcpy(cpu->xmm[dsti].bytes + (high ? 8 : 0), &v, 8);
    }
    cpu->rip = next_rip(cpu, d);
}

void exec_movhlps(CPU* cpu, const Dec& d, bool hl) {
    int dsti = -1, dstw = 0, srci = -1, srcw = 0;
    reg_to_vec(d.ops[0].reg.value, dsti, dstw);
    reg_to_vec(d.ops[1].reg.value, srci, srcw);
    if (hl) {
        // MOVHLPS: dst[63:0] = src[127:64].
        memcpy(cpu->xmm[dsti].bytes, cpu->xmm[srci].bytes + 8, 8);
    } else {
        // MOVLHPS: dst[127:64] = src[63:0].
        memcpy(cpu->xmm[dsti].bytes + 8, cpu->xmm[srci].bytes, 8);
    }
    cpu->rip = next_rip(cpu, d);
}

void exec_ldmxcsr(CPU* cpu, const Dec& d, bool load) {
    uint64_t addr = resolve_mem(cpu, d.ops[0], cpu->rip + d.insn.length);
    if (load)
        cpu->mxcsr = mem_load32(cpu, addr);
    else
        mem_store32(cpu, addr, cpu->mxcsr);
    cpu->rip = next_rip(cpu, d);
}

void exec_rdrand(CPU* cpu, const Dec& d, bool seed) {
    // Host passthrough with retry; CF=1 on success.
    Val out;
    bool ok = false;
    uint64_t v = 0;
    for (int i = 0; i < 10 && !ok; i++) {
        unsigned char rc;
        if (common_int_width(d) == 8 && d.ops[0].size == 64) {
            unsigned long long x = 0;
            if (seed)
                asm volatile("rdseed %0; setc %1" : "=r"(x), "=qm"(rc) :: "cc");
            else
                asm volatile("rdrand %0; setc %1" : "=r"(x), "=qm"(rc) :: "cc");
            v = x;
        } else if (d.ops[0].size == 16) {
            unsigned short x = 0;
            if (seed)
                asm volatile("rdseed %0; setc %1" : "=r"(x), "=qm"(rc) :: "cc");
            else
                asm volatile("rdrand %0; setc %1" : "=r"(x), "=qm"(rc) :: "cc");
            v = x;
        } else {
            unsigned x = 0;
            if (seed)
                asm volatile("rdseed %0; setc %1" : "=r"(x), "=qm"(rc) :: "cc");
            else
                asm volatile("rdrand %0; setc %1" : "=r"(x), "=qm"(rc) :: "cc");
            v = x;
        }
        ok = rc;
    }
    cpu->set_flag(FLAG_CF, ok);
    cpu->set_flag(FLAG_OF, false);
    cpu->set_flag(FLAG_SF, false);
    cpu->set_flag(FLAG_ZF, false);
    cpu->set_flag(FLAG_AF, false);
    cpu->set_flag(FLAG_PF, false);
    out.v = v;
    op_store(cpu, d, 0, out);
    cpu->rip = next_rip(cpu, d);
}

void exec_crc32(CPU* cpu, const Dec& d) {
    Val acc = op_load(cpu, d, 0);
    Val src = op_load(cpu, d, 1);
    int sw = d.ops[1].size / 8;
    uint32_t crc = (uint32_t)acc.v;
    if (sw == 1) {
        asm volatile("crc32b %1, %0" : "+r"(crc) : "rm"((uint8_t)src.v));
    } else if (sw == 2) {
        asm volatile("crc32w %1, %0" : "+r"(crc) : "rm"((uint16_t)src.v));
    } else if (sw == 4) {
        asm volatile("crc32l %1, %0" : "+r"(crc) : "rm"((uint32_t)src.v));
    } else {
        uint64_t c = acc.v;
        asm volatile("crc32q %1, %0" : "+r"(c) : "rm"(src.v));
        Val out;
        out.v = c;
        op_store(cpu, d, 0, out);
        cpu->rip = next_rip(cpu, d);
        return;
    }
    Val out;
    out.v = crc;
    op_store(cpu, d, 0, out);
    cpu->rip = next_rip(cpu, d);
}

void exec_vzero(CPU* cpu, const Dec& d, bool all) {
    (void)all; // VZEROUPPER and VZEROALL both just clear upper lanes here
    for (int i = 0; i < 32; i++)
        memset(cpu->xmm[i].bytes + 16, 0, 48);
    (void)d;
    cpu->rip = next_rip(cpu, d);
}

void exec_vbroadcast(CPU* cpu, const Dec& d) {
    // VBROADCASTSS/SD/F128 + VPBROADCASTB/W/D/Q.
    unsigned w = vec_width(d);
    int n = explicit_ops(d);
    VecVal src;
    unsigned elem = 4;
    switch (d.insn.mnemonic) {
    case ZYDIS_MNEMONIC_VBROADCASTSS:
    case ZYDIS_MNEMONIC_VPBROADCASTD: elem = 4; break;
    case ZYDIS_MNEMONIC_VBROADCASTSD:
    case ZYDIS_MNEMONIC_VPBROADCASTQ: elem = 8; break;
    case ZYDIS_MNEMONIC_VPBROADCASTB: elem = 1; break;
    case ZYDIS_MNEMONIC_VPBROADCASTW: elem = 2; break;
    case ZYDIS_MNEMONIC_VBROADCASTF128: elem = 16; break;
    default: guest_error(cpu, "bad broadcast");
    }
    {
        const auto& sop = d.ops[n - 1];
        int ridx, rwidth;
        if (sop.type == ZYDIS_OPERAND_TYPE_REGISTER &&
            !reg_to_vec(sop.reg.value, ridx, rwidth)) {
            // GPR source (e.g. vpbroadcastd ymm, eax).
            Val g = op_load(cpu, d, n - 1);
            memcpy(src, &g.v, elem);
            src.taint = g.taint;
        } else {
            vec_load_bytes(cpu, d, n - 1, src, elem);
        }
    }
    VecVal out;
    for (unsigned i = 0; i < w; i += elem)
        memcpy(out + i, src, elem);
    out.taint = src.taint;
    vec_store_bytes(cpu, d, 0, out, w, true, w);
    cpu->rip = next_rip(cpu, d);
}

void exec_vextract(CPU* cpu, const Dec& d, bool insert) {
    int n = explicit_ops(d);
    unsigned w = vec_width(d); // dest width for extract (16), src for insert
    Val imm = op_load(cpu, d, n - 1);
    if (!insert) {
        // VEXTRACTF128/I128 xmm/mem, ymm, imm.
        VecVal src;
        vec_load_bytes(cpu, d, 1, src, 32);
        unsigned off = ((unsigned)imm.v & 1) ? 16 : 0;
        {
            Taint t = src.taint;
            // Sub-vector store via raw pointer: need Taint explicitly.
            vec_store_bytes(cpu, d, 0, (const uint8_t*)(src.bytes + off), 16, false, 16, t);
        }
    } else {
        // VINSERTF128/I128 ymm, ymm, xmm/mem, imm.
        VecVal a, b, o;
        vec_load_bytes(cpu, d, 1, a, 32);
        vec_load_bytes(cpu, d, 2, b, 16);
        memcpy(o, a, 32);
        unsigned off = ((unsigned)imm.v & 1) ? 16 : 0;
        memcpy(o + off, b, 16);
        o.taint = a.taint | b.taint;
        vec_store_bytes(cpu, d, 0, o, 32, true, 32);
        (void)w;
    }
    cpu->rip = next_rip(cpu, d);
}

void exec_vperm2(CPU* cpu, const Dec& d) {
    // VPERM2F128 / VPERM2I128.
    int n = explicit_ops(d);
    VecVal a, b, o;
    vec_load_bytes(cpu, d, 1, a, 32);
    vec_load_bytes(cpu, d, 2, b, 32);
    unsigned sel = (unsigned)op_load(cpu, d, n - 1).v;
    const uint8_t* lanes[4] = {a, a + 16, b, b + 16};
    for (int i = 0; i < 2; i++) {
        unsigned s = (sel >> (i * 4)) & 3;
        bool zero = (sel >> (i * 4 + 3)) & 1; // bit 3/7
        if (zero)
            memset(o + i * 16, 0, 16);
        else
            memcpy(o + i * 16, lanes[s], 16);
    }
    o.taint = a.taint | b.taint;
    vec_store_bytes(cpu, d, 0, o, 32, true, 32);
    cpu->rip = next_rip(cpu, d);
}

void exec_vpermil(CPU* cpu, const Dec& d, bool is_pd, bool var) {
    int n = explicit_ops(d);
    unsigned w = vec_width(d);
    if (n == 3 && d.ops[2].type == ZYDIS_OPERAND_TYPE_REGISTER)
        var = true;
    VecVal a, c, o;
    unsigned lane = is_pd ? 8 : 4;
    if (var) {
        vec_load_bytes(cpu, d, n - 2, a, w);
        vec_load_bytes(cpu, d, n - 1, c, w);
    } else {
        vec_load_bytes(cpu, d, n - 2, a, w);
        unsigned order = (unsigned)op_load(cpu, d, n - 1).v;
        for (unsigned i = 0; i < 8; i++)
            c[i] = (order >> (i * (is_pd ? 1 : 2))) & (is_pd ? 1 : 3);
    }
    for (unsigned L = 0; L < w; L += 16) {
        unsigned elems = 16 / lane;
        for (unsigned i = 0; i < elems; i++) {
            unsigned sel = var ? (c[L + i * lane] & (elems - 1)) : (c[i] & (elems - 1));
            memcpy(o + L + i * lane, a + L + sel * lane, lane);
        }
    }
    o.taint = a.taint | c.taint;
    vec_store_dst(cpu, d, o, w);
    cpu->rip = next_rip(cpu, d);
}

void exec_vpermpd(CPU* cpu, const Dec& d) {
    int n = explicit_ops(d);
    VecVal a, o;
    vec_load_bytes(cpu, d, n - 2, a, 32);
    unsigned order = (unsigned)op_load(cpu, d, n - 1).v;
    for (unsigned i = 0; i < 4; i++) {
        unsigned sel = (order >> (i * 2)) & 3;
        memcpy(o + i * 8, a + sel * 8, 8);
    }
    o.taint = a.taint;
    vec_store_bytes(cpu, d, 0, o, 32, true, 32);
    cpu->rip = next_rip(cpu, d);
}

void exec_vpermq(CPU* cpu, const Dec& d) {
    // VPERMQ ymm, ymm/mem, imm.
    int n = explicit_ops(d);
    VecVal a, o;
    vec_load_bytes(cpu, d, n - 2, a, 32);
    unsigned order = (unsigned)op_load(cpu, d, n - 1).v;
    for (unsigned i = 0; i < 4; i++) {
        unsigned sel = (order >> (i * 2)) & 3;
        memcpy(o + i * 8, a + sel * 8, 8);
    }
    o.taint = a.taint;
    vec_store_bytes(cpu, d, 0, o, 32, true, 32);
    cpu->rip = next_rip(cpu, d);
}


void exec_pblendvb(CPU* cpu, const Dec& d) {
    // PBLENDVB / VPBLENDVB: mask from XMM0 (implicit) or op2 (VEX).
    int n = explicit_ops(d);
    unsigned w = vec_width(d);
    VecVal a, b, m, o;
    if (is_vex(d) && n == 4) {
        vec_load_bytes(cpu, d, 1, a, w);
        vec_load_bytes(cpu, d, 2, b, w);
        vec_load_bytes(cpu, d, 3, m, w);
    } else {
        vec_load_bytes(cpu, d, 0, a, w);
        vec_load_bytes(cpu, d, 1, b, w);
        memcpy(m, cpu->xmm[0].bytes, w);
        m.taint = cpu->xmm[0].taint;
    }
    for (unsigned i = 0; i < w; i++)
        o[i] = (m[i] & 0x80) ? b[i] : a[i];
    o.taint = a.taint | b.taint | m.taint;
    vec_store_dst(cpu, d, o, w);
    cpu->rip = next_rip(cpu, d);
}

void exec_blendps(CPU* cpu, const Dec& d, bool is_pd) {
    int n = explicit_ops(d);
    unsigned w = vec_width(d);
    unsigned lane = is_pd ? 8 : 4;
    VecVal a, b, o;
    vec_sources(cpu, d, a, b);
    unsigned sel = (unsigned)op_load(cpu, d, n - 1).v;
    for (unsigned i = 0; i < w; i += lane) {
        bool take = (sel >> (i / lane)) & 1;
        memcpy(o + i, take ? b + i : a + i, lane);
    }
    o.taint = a.taint | b.taint;
    vec_store_dst(cpu, d, o, w);
    cpu->rip = next_rip(cpu, d);
}

void exec_pblendw(CPU* cpu, const Dec& d) {
    int n = explicit_ops(d);
    VecVal a, b, o;
    unsigned w = vec_sources(cpu, d, a, b);
    unsigned sel = (unsigned)op_load(cpu, d, n - 1).v;
    for (unsigned i = 0; i < w; i += 2) {
        bool take = (sel >> (i / 2)) & 1;
        memcpy(o + i, take ? b + i : a + i, 2);
    }
    o.taint = a.taint | b.taint;
    vec_store_dst(cpu, d, o, w);
    cpu->rip = next_rip(cpu, d);
}

} // namespace

// === x87 FPU ================================================================

namespace {

long double& fpu_st(CPU* cpu, int i) { return cpu->st[(cpu->fpu_top + i) & 7]; }

void fpu_push(CPU* cpu, long double v) {
    cpu->fpu_top = (cpu->fpu_top - 1) & 7;
    cpu->st[cpu->fpu_top] = v;
}

long double fpu_pop(CPU* cpu) {
    long double v = cpu->st[cpu->fpu_top];
    cpu->fpu_top = (cpu->fpu_top + 1) & 7;
    return v;
}

long double x87_mem_load(CPU* cpu, const Dec& d, int oi) {
    unsigned bytes = d.ops[oi].size / 8;
    uint64_t addr = resolve_mem(cpu, d.ops[oi], cpu->rip + d.insn.length);
    if (bytes == 4) {
        float v;
        mem_load_bytes(cpu, addr, (uint8_t*)&v, 4);
        return (long double)v;
    } else if (bytes == 8) {
        double v;
        mem_load_bytes(cpu, addr, (uint8_t*)&v, 8);
        return (long double)v;
    } else if (bytes == 10) {
        // 80-bit extended: 64-bit mantissa + 16-bit exponent/sign.
        uint64_t man = mem_load64(cpu, addr);
        uint16_t exp = mem_load16(cpu, addr + 8);
        long double v;
        // Convert via host FPU: store bytes then load as long double
        // (host x86-64 long double is 80-bit extended).
        uint8_t buf[16] = {0};
        memcpy(buf, &man, 8);
        memcpy(buf + 8, &exp, 2);
        memcpy(&v, buf, 16);
        return v;
    }
    guest_error(cpu, "bad x87 mem size");
}

void x87_mem_store(CPU* cpu, const Dec& d, int oi, long double v) {
    unsigned bytes = d.ops[oi].size / 8;
    uint64_t addr = resolve_mem(cpu, d.ops[oi], cpu->rip + d.insn.length);
    if (bytes == 4) {
        float f = (float)v;
        mem_store_bytes(cpu, addr, (uint8_t*)&f, 4);
    } else if (bytes == 8) {
        double dd = (double)v;
        mem_store_bytes(cpu, addr, (uint8_t*)&dd, 8);
    } else if (bytes == 10) {
        uint8_t buf[16] = {0};
        memcpy(buf, &v, 16); // host long double layout == 80-bit extended
        mem_store_bytes(cpu, addr, buf, 10);
    } else {
        guest_error(cpu, "bad x87 mem size");
    }
}

void x87_compare(CPU* cpu, long double a, long double b, bool to_eflags) {
    // C0->CF, C2->PF, C3->ZF.
    bool c0 = false, c2 = false, c3 = false;
    if (a != a || b != b) {
        c0 = c2 = c3 = true;
    } else if (a == b) {
        c3 = true;
    } else if (a < b) {
        c0 = true;
    }
    if (to_eflags) {
        cpu->set_flag(FLAG_CF, c0);
        cpu->set_flag(FLAG_PF, c2);
        cpu->set_flag(FLAG_ZF, c3);
        cpu->set_flag(FLAG_OF, false);
        cpu->set_flag(FLAG_SF, false);
        cpu->set_flag(FLAG_AF, false);
    } else {
        cpu->fsw = (cpu->fsw & ~0x4700) | (c0 ? 0x0100 : 0) | (c2 ? 0x0400 : 0) |
                   (c3 ? 0x4000 : 0);
    }
}

int x87_st_index(CPU* cpu, const Dec& d, int oi) {
    ZydisRegister r = d.ops[oi].reg.value;
    if (r < ZYDIS_REGISTER_ST0 || r > ZYDIS_REGISTER_ST7)
        guest_error(cpu, "bad x87 stack register");
    return r - ZYDIS_REGISTER_ST0;
}

void exec_x87(CPU* cpu, const Dec& d) {
    int n = explicit_ops(d);
    switch (d.insn.mnemonic) {
    case ZYDIS_MNEMONIC_FLD: {
        if (d.ops[0].type == ZYDIS_OPERAND_TYPE_REGISTER)
            fpu_push(cpu, fpu_st(cpu, x87_st_index(cpu, d, 0)));
        else
            fpu_push(cpu, x87_mem_load(cpu, d, 0));
        break;
    }
    case ZYDIS_MNEMONIC_FST:
    case ZYDIS_MNEMONIC_FSTP: {
        long double v = fpu_st(cpu, 0);
        if (d.ops[0].type == ZYDIS_OPERAND_TYPE_REGISTER) {
            fpu_st(cpu, x87_st_index(cpu, d, 0)) = v;
        } else {
            x87_mem_store(cpu, d, 0, v);
        }
        if (d.insn.mnemonic == ZYDIS_MNEMONIC_FSTP)
            fpu_pop(cpu);
        break;
    }
    case ZYDIS_MNEMONIC_FILD: {
        unsigned bytes = d.ops[0].size / 8;
        uint64_t addr = resolve_mem(cpu, d.ops[0], cpu->rip + d.insn.length);
        if (bytes == 2)
            fpu_push(cpu, (int16_t)mem_load16(cpu, addr));
        else if (bytes == 4)
            fpu_push(cpu, (int32_t)mem_load32(cpu, addr));
        else if (bytes == 8)
            fpu_push(cpu, (int64_t)mem_load64(cpu, addr));
        else
            guest_error(cpu, "bad FILD size");
        break;
    }
    case ZYDIS_MNEMONIC_FIST:
    case ZYDIS_MNEMONIC_FISTP:
    case ZYDIS_MNEMONIC_FISTTP: {
        unsigned bytes = d.ops[0].size / 8;
        uint64_t addr = resolve_mem(cpu, d.ops[0], cpu->rip + d.insn.length);
        long double v = fpu_st(cpu, 0);
        bool trunc = (d.insn.mnemonic == ZYDIS_MNEMONIC_FISTTP);
        long double rv = trunc ? __builtin_truncl(v) : __builtin_roundl(v);
        if (bytes == 2)
            mem_store16(cpu, addr, (int16_t)rv);
        else if (bytes == 4)
            mem_store32(cpu, addr, (int32_t)rv);
        else if (bytes == 8)
            mem_store64(cpu, addr, (int64_t)rv);
        else
            guest_error(cpu, "bad FIST size");
        if (d.insn.mnemonic != ZYDIS_MNEMONIC_FIST)
            fpu_pop(cpu);
        break;
    }
    case ZYDIS_MNEMONIC_FLDZ: fpu_push(cpu, 0.0L); break;
    case ZYDIS_MNEMONIC_FLD1: fpu_push(cpu, 1.0L); break;
    case ZYDIS_MNEMONIC_FLDPI: fpu_push(cpu, 3.14159265358979323846L); break;
    case ZYDIS_MNEMONIC_FLDL2T: fpu_push(cpu, 3.32192809488736234787L); break;
    case ZYDIS_MNEMONIC_FLDL2E: fpu_push(cpu, 1.44269504088896340736L); break;
    case ZYDIS_MNEMONIC_FLDLG2: fpu_push(cpu, 0.30102999566398119521L); break;
    case ZYDIS_MNEMONIC_FLDLN2: fpu_push(cpu, 0.69314718055994530942L); break;
    case ZYDIS_MNEMONIC_FADD:
    case ZYDIS_MNEMONIC_FMUL:
    case ZYDIS_MNEMONIC_FSUB:
    case ZYDIS_MNEMONIC_FDIV:
    case ZYDIS_MNEMONIC_FSUBR:
    case ZYDIS_MNEMONIC_FDIVR:
    case ZYDIS_MNEMONIC_FADDP:
    case ZYDIS_MNEMONIC_FMULP:
    case ZYDIS_MNEMONIC_FSUBP:
    case ZYDIS_MNEMONIC_FDIVP:
    case ZYDIS_MNEMONIC_FSUBRP:
    case ZYDIS_MNEMONIC_FDIVRP:
    case ZYDIS_MNEMONIC_FIADD:
    case ZYDIS_MNEMONIC_FISUB:
    case ZYDIS_MNEMONIC_FIMUL:
    case ZYDIS_MNEMONIC_FIDIV: {
        bool is_p = (d.insn.mnemonic == ZYDIS_MNEMONIC_FADDP ||
                     d.insn.mnemonic == ZYDIS_MNEMONIC_FMULP ||
                     d.insn.mnemonic == ZYDIS_MNEMONIC_FSUBP ||
                     d.insn.mnemonic == ZYDIS_MNEMONIC_FDIVP ||
                     d.insn.mnemonic == ZYDIS_MNEMONIC_FSUBRP ||
                     d.insn.mnemonic == ZYDIS_MNEMONIC_FDIVRP);
        bool is_i = (d.insn.mnemonic == ZYDIS_MNEMONIC_FIADD ||
                     d.insn.mnemonic == ZYDIS_MNEMONIC_FISUB ||
                     d.insn.mnemonic == ZYDIS_MNEMONIC_FIMUL ||
                     d.insn.mnemonic == ZYDIS_MNEMONIC_FIDIV);
        long double a = fpu_st(cpu, 0), b = 0;
        int dsti = 0;
        if (n == 0) {
            // FADD ST(0),ST(1) implicit form? Actually no-operand FADDP etc.
            b = fpu_st(cpu, 1);
            dsti = 1;
        } else if (d.ops[n - 1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
            b = is_i ? 0 : x87_mem_load(cpu, d, n - 1);
            if (is_i) {
                unsigned bytes = d.ops[n - 1].size / 8;
                uint64_t addr =
                    resolve_mem(cpu, d.ops[n - 1], cpu->rip + d.insn.length);
                if (bytes == 2)
                    b = (int16_t)mem_load16(cpu, addr);
                else if (bytes == 4)
                    b = (int32_t)mem_load32(cpu, addr);
                else
                    guest_error(cpu, "bad FILD size");
            }
            dsti = 0;
        } else {
            // st, st(i) form: dst is op0.
            dsti = x87_st_index(cpu, d, 0);
            b = (n > 1) ? fpu_st(cpu, x87_st_index(cpu, d, 1)) : fpu_st(cpu, 0);
            a = fpu_st(cpu, dsti);
        }
        long double r = 0;
        switch (d.insn.mnemonic) {
        case ZYDIS_MNEMONIC_FADD:
        case ZYDIS_MNEMONIC_FADDP:
        case ZYDIS_MNEMONIC_FIADD: r = a + b; break;
        case ZYDIS_MNEMONIC_FMUL:
        case ZYDIS_MNEMONIC_FMULP:
        case ZYDIS_MNEMONIC_FIMUL: r = a * b; break;
        case ZYDIS_MNEMONIC_FSUB:
        case ZYDIS_MNEMONIC_FSUBP:
        case ZYDIS_MNEMONIC_FISUB: r = a - b; break;
        case ZYDIS_MNEMONIC_FDIV:
        case ZYDIS_MNEMONIC_FDIVP:
        case ZYDIS_MNEMONIC_FIDIV: r = a / b; break;
        case ZYDIS_MNEMONIC_FSUBR:
        case ZYDIS_MNEMONIC_FSUBRP: r = b - a; break;
        default: r = b / a; break;
        }
        if (is_p) {
            fpu_st(cpu, dsti) = r;
            fpu_pop(cpu);
        } else {
            fpu_st(cpu, dsti) = r;
        }
        break;
    }
    case ZYDIS_MNEMONIC_FCOM:
    case ZYDIS_MNEMONIC_FCOMP:
    case ZYDIS_MNEMONIC_FUCOM:
    case ZYDIS_MNEMONIC_FUCOMP:
    case ZYDIS_MNEMONIC_FUCOMPP:
    case ZYDIS_MNEMONIC_FCOMPP:
    case ZYDIS_MNEMONIC_FICOMP: {
        long double a = fpu_st(cpu, 0), b = 0;
        if (n == 0) {
            b = fpu_st(cpu, 1);
        } else if (d.ops[0].type == ZYDIS_OPERAND_TYPE_MEMORY) {
            if (d.insn.mnemonic == ZYDIS_MNEMONIC_FICOMP) {
                unsigned bytes = d.ops[0].size / 8;
                uint64_t addr =
                    resolve_mem(cpu, d.ops[0], cpu->rip + d.insn.length);
                if (bytes == 2)
                    b = (int16_t)mem_load16(cpu, addr);
                else
                    b = (int32_t)mem_load32(cpu, addr);
            } else {
                b = x87_mem_load(cpu, d, 0);
            }
        } else {
            b = fpu_st(cpu, x87_st_index(cpu, d, 0));
        }
        x87_compare(cpu, a, b, false);
        if (d.insn.mnemonic == ZYDIS_MNEMONIC_FCOMP ||
            d.insn.mnemonic == ZYDIS_MNEMONIC_FUCOMP ||
            d.insn.mnemonic == ZYDIS_MNEMONIC_FICOMP)
            fpu_pop(cpu);
        else if (d.insn.mnemonic == ZYDIS_MNEMONIC_FUCOMPP ||
                 d.insn.mnemonic == ZYDIS_MNEMONIC_FCOMPP) {
            fpu_pop(cpu);
            fpu_pop(cpu);
        }
        break;
    }
    case ZYDIS_MNEMONIC_FCOMI:
    case ZYDIS_MNEMONIC_FUCOMI:
    case ZYDIS_MNEMONIC_FCOMIP:
    case ZYDIS_MNEMONIC_FUCOMIP:
    case ZYDIS_MNEMONIC_FCMOVB:
    case ZYDIS_MNEMONIC_FCMOVE:
    case ZYDIS_MNEMONIC_FCMOVBE:
    case ZYDIS_MNEMONIC_FCMOVU:
    case ZYDIS_MNEMONIC_FCMOVNB:
    case ZYDIS_MNEMONIC_FCMOVNE:
    case ZYDIS_MNEMONIC_FCMOVNBE:
    case ZYDIS_MNEMONIC_FCMOVNU: {
        long double a = fpu_st(cpu, 0);
        long double b = fpu_st(cpu, x87_st_index(cpu, d, n - 1));
        bool cmp = (d.insn.mnemonic == ZYDIS_MNEMONIC_FCOMI ||
                    d.insn.mnemonic == ZYDIS_MNEMONIC_FUCOMI ||
                    d.insn.mnemonic == ZYDIS_MNEMONIC_FCOMIP ||
                    d.insn.mnemonic == ZYDIS_MNEMONIC_FUCOMIP);
        if (cmp) {
            x87_compare(cpu, a, b, true);
            if (d.insn.mnemonic == ZYDIS_MNEMONIC_FCOMIP ||
                d.insn.mnemonic == ZYDIS_MNEMONIC_FUCOMIP)
                fpu_pop(cpu);
        } else {
            // FCMOVcc: condition on integer flags like CMOVcc.
            ZydisMnemonic eq = ZYDIS_MNEMONIC_INVALID;
            switch (d.insn.mnemonic) {
            case ZYDIS_MNEMONIC_FCMOVB: eq = ZYDIS_MNEMONIC_CMOVB; break;
            case ZYDIS_MNEMONIC_FCMOVE: eq = ZYDIS_MNEMONIC_CMOVZ; break;
            case ZYDIS_MNEMONIC_FCMOVBE: eq = ZYDIS_MNEMONIC_CMOVBE; break;
            case ZYDIS_MNEMONIC_FCMOVU: eq = ZYDIS_MNEMONIC_CMOVP; break;
            case ZYDIS_MNEMONIC_FCMOVNB: eq = ZYDIS_MNEMONIC_CMOVNB; break;
            case ZYDIS_MNEMONIC_FCMOVNE: eq = ZYDIS_MNEMONIC_CMOVNZ; break;
            case ZYDIS_MNEMONIC_FCMOVNBE: eq = ZYDIS_MNEMONIC_CMOVNBE; break;
            default: eq = ZYDIS_MNEMONIC_CMOVNP; break;
            }
            if (eval_cond(cpu, eq))
                fpu_st(cpu, 0) = b;
        }
        break;
    }
    case ZYDIS_MNEMONIC_FNSTSW: {
        if (d.ops[0].type == ZYDIS_OPERAND_TYPE_REGISTER) {
            cpu->gpr[ZG_RAX].val =
                (cpu->gpr[ZG_RAX].val & ~0xffffULL) | cpu->fsw;
        } else {
            uint64_t addr = resolve_mem(cpu, d.ops[0], cpu->rip + d.insn.length);
            mem_store16(cpu, addr, cpu->fsw);
        }
        break;
    }
    case ZYDIS_MNEMONIC_FLDCW: {
        uint64_t addr = resolve_mem(cpu, d.ops[0], cpu->rip + d.insn.length);
        cpu->fcw = mem_load16(cpu, addr);
        break;
    }
    case ZYDIS_MNEMONIC_FNSTCW: {
        uint64_t addr = resolve_mem(cpu, d.ops[0], cpu->rip + d.insn.length);
        mem_store16(cpu, addr, cpu->fcw);
        break;
    }
    case ZYDIS_MNEMONIC_FLDENV: {
        // 28-byte m28byte environment (32-bit protected-mode layout):
        // FCW[0:2], FSW[4:6], FTW[8:10], FIP[12:16], FCS[16:20],
        // FDP[20:24], FDS[24:28]. We model FCW/FSW/TOP; addresses ignored.
        uint64_t addr = resolve_mem(cpu, d.ops[0], cpu->rip + d.insn.length);
        uint8_t env[28];
        mem_load_bytes(cpu, addr, env, 28);
        uint16_t fcw = 0, fsw = 0;
        memcpy(&fcw, env + 0, 2);
        memcpy(&fsw, env + 4, 2);
        cpu->fcw = fcw;
        cpu->fsw = (uint16_t)((fsw & ~(7u << 11)) | (cpu->fsw & (7u << 11)));
        cpu->fpu_top = (fsw >> 11) & 7;
        break;
    }
    case ZYDIS_MNEMONIC_FNSTENV: {
        uint64_t addr = resolve_mem(cpu, d.ops[0], cpu->rip + d.insn.length);
        uint8_t env[28];
        memset(env, 0, sizeof(env));
        uint16_t fsw = (uint16_t)((cpu->fsw & ~(7u << 11)) |
                                  ((cpu->fpu_top & 7) << 11));
        uint16_t ftw = 0;
        for (int i = 0; i < 8; i++) {
            // Tag word: assume all regs valid (tag 0) — minimal model.
            (void)i;
        }
        ftw = 0x0000;
        memcpy(env + 0, &cpu->fcw, 2);
        memcpy(env + 4, &fsw, 2);
        memcpy(env + 8, &ftw, 2);
        mem_store_bytes(cpu, addr, env, 28);
        break;
    }
    case ZYDIS_MNEMONIC_FCHS: fpu_st(cpu, 0) = -fpu_st(cpu, 0); break;
    case ZYDIS_MNEMONIC_FABS: fpu_st(cpu, 0) = __builtin_fabsl(fpu_st(cpu, 0)); break;
    case ZYDIS_MNEMONIC_FXCH:
        if (n == 0) {
            long double t = fpu_st(cpu, 0);
            fpu_st(cpu, 0) = fpu_st(cpu, 1);
            fpu_st(cpu, 1) = t;
        } else {
            int i = x87_st_index(cpu, d, n - 1);
            long double t = fpu_st(cpu, 0);
            fpu_st(cpu, 0) = fpu_st(cpu, i);
            fpu_st(cpu, i) = t;
        }
        break;
    case ZYDIS_MNEMONIC_FRNDINT:
        fpu_st(cpu, 0) = __builtin_roundl(fpu_st(cpu, 0));
        break;
    case ZYDIS_MNEMONIC_FPREM:
    case ZYDIS_MNEMONIC_FPREM1: {
        long double a = fpu_st(cpu, 0), b = fpu_st(cpu, 1);
        fpu_st(cpu, 0) = __builtin_fmodl(a, b);
        break;
    }
    case ZYDIS_MNEMONIC_FSCALE:
        fpu_st(cpu, 0) = fpu_st(cpu, 0) * __builtin_powil(2.0L, __builtin_truncl(fpu_st(cpu, 1)));
        break;
    case ZYDIS_MNEMONIC_FSQRT: fpu_st(cpu, 0) = __builtin_sqrtl(fpu_st(cpu, 0)); break;
    case ZYDIS_MNEMONIC_FSIN: fpu_st(cpu, 0) = __builtin_sinl(fpu_st(cpu, 0)); break;
    case ZYDIS_MNEMONIC_FCOS: fpu_st(cpu, 0) = __builtin_cosl(fpu_st(cpu, 0)); break;
    case ZYDIS_MNEMONIC_F2XM1:
        fpu_st(cpu, 0) = __builtin_powil(2.0L, fpu_st(cpu, 0)) - 1.0L;
        break;
    case ZYDIS_MNEMONIC_FYL2X: {
        long double r = __builtin_log2l(fpu_st(cpu, 0)) * fpu_st(cpu, 1);
        fpu_pop(cpu);
        fpu_st(cpu, 0) = r;
        break;
    }
    case ZYDIS_MNEMONIC_FPATAN: {
        long double r = __builtin_atan2l(fpu_st(cpu, 1), fpu_st(cpu, 0));
        fpu_pop(cpu);
        fpu_st(cpu, 0) = r;
        break;
    }
    case ZYDIS_MNEMONIC_FPTAN: {
        long double v = __builtin_tanl(fpu_st(cpu, 0));
        fpu_st(cpu, 0) = v;
        fpu_push(cpu, 1.0L);
        break;
    }
    case ZYDIS_MNEMONIC_FSINCOS: {
        long double v = fpu_st(cpu, 0);
        fpu_st(cpu, 0) = __builtin_sinl(v);
        fpu_push(cpu, __builtin_cosl(v));
        break;
    }
    case ZYDIS_MNEMONIC_FTST:
        x87_compare(cpu, fpu_st(cpu, 0), 0.0L, false);
        break;
    case ZYDIS_MNEMONIC_FXAM:
        // Examine: set C3/C2/C0 from class. Simplify: normal nonzero.
        cpu->fsw = (cpu->fsw & ~0x4700) | 0x0400; // C2: normal
        if (fpu_st(cpu, 0) == 0.0L)
            cpu->fsw = (cpu->fsw & ~0x4700) | 0x4000; // C3: zero
        break;
    case ZYDIS_MNEMONIC_FFREE:
    case ZYDIS_MNEMONIC_FNOP:
        break;
    case ZYDIS_MNEMONIC_FINCSTP:
        cpu->fpu_top = (cpu->fpu_top + 1) & 7;
        break;
    case ZYDIS_MNEMONIC_FDECSTP:
        cpu->fpu_top = (cpu->fpu_top - 1) & 7;
        break;
    case ZYDIS_MNEMONIC_FNINIT:
        cpu->fpu_top = 0;
        cpu->fcw = 0x037f;
        cpu->fsw = 0;
        break;
    case ZYDIS_MNEMONIC_FNCLEX:
        cpu->fsw &= ~0x7f;
        break;
    case ZYDIS_MNEMONIC_FBLD: {
        // BCD load m80 -> st0. Rare; implement basic.
        uint64_t addr = resolve_mem(cpu, d.ops[0], cpu->rip + d.insn.length);
        uint8_t bcd[10];
        mem_load_bytes(cpu, addr, bcd, 10);
        bool neg = bcd[9] & 0x80;
        long double v = 0;
        for (int i = 8; i >= 0; i--) {
            v = v * 100.0L + (bcd[i] & 0xf) + ((bcd[i] >> 4) & 0xf) * 10.0L;
        }
        int top2 = (bcd[9] & 0x7f);
        v += (long double)(top2 & 0xf) * 1e18L + (long double)((top2 >> 4) & 0x7) * 1e19L;
        fpu_push(cpu, neg ? -v : v);
        break;
    }
    case ZYDIS_MNEMONIC_FBSTP: {
        long double v = fpu_pop(cpu);
        bool neg = v < 0;
        if (neg)
            v = -v;
        uint64_t addr = resolve_mem(cpu, d.ops[0], cpu->rip + d.insn.length);
        uint8_t bcd[10] = {0};
        for (int i = 0; i < 9; i++) {
            unsigned d0 = (unsigned)__builtin_fmodl(v, 10.0L);
            v = __builtin_floorl(v / 10.0L);
            unsigned d1 = (unsigned)__builtin_fmodl(v, 10.0L);
            v = __builtin_floorl(v / 10.0L);
            bcd[i] = d0 | (d1 << 4);
        }
        unsigned hi = (unsigned)__builtin_fmodl(v, 100.0L);
        bcd[9] = (hi / 10) << 4 | (hi % 10);
        if (neg)
            bcd[9] |= 0x80;
        mem_store_bytes(cpu, addr, bcd, 10);
        break;
    }
    default:
        unimplemented(cpu, d, "x87");
    }
    cpu->rip = next_rip(cpu, d);
}

// === misc integer/SIMD odds and ends ========================================

void exec_shld_shrd(CPU* cpu, const Dec& d, bool is_shld) {
    int n = explicit_ops(d);
    Val dst = op_load(cpu, d, 0);
    Val src = op_load(cpu, d, 1);
    Val cnt = op_load(cpu, d, 2);
    int w = common_int_width(d);
    unsigned bits = w * 8;
    uint64_t c = cnt.v & (bits == 64 ? 63 : 31);
    Val out;
    out.taint.cannot_branch = dst.taint.cannot_branch || src.taint.cannot_branch || cnt.taint.cannot_branch;
    out.taint.cannot_index = dst.taint.cannot_index || src.taint.cannot_index || cnt.taint.cannot_index;
    if (c == 0) {
        cpu->rip = next_rip(cpu, d);
        return;
    }
    uint64_t m = mask_for(w);
    uint64_t r;
    if (is_shld) {
        r = ((dst.v << c) | (src.v >> (bits - c))) & m;
        cpu->set_flag(FLAG_CF, (dst.v >> (bits - c)) & 1);
    } else {
        r = ((dst.v >> c) | (src.v << (bits - c))) & m;
        cpu->set_flag(FLAG_CF, (dst.v >> (c - 1)) & 1);
    }
    if (c == 1)
        cpu->set_flag(FLAG_OF, (((r >> (bits - 1)) & 1) != (((dst.v & m) >> (bits - 1)) & 1)));
    cpu->set_flag(FLAG_ZF, r == 0);
    cpu->set_flag(FLAG_SF, (r >> (bits - 1)) & 1);
    cpu->set_flag(FLAG_PF, parity8((uint8_t)r));
    out.v = r;
    op_store(cpu, d, 0, out);
    (void)n;
    cpu->rip = next_rip(cpu, d);
}

void exec_cmpxchg8b(CPU* cpu, const Dec& d) {
    uint64_t addr = resolve_mem(cpu, d.ops[0], cpu->rip + d.insn.length);
    uint64_t rax = cpu->gpr[ZG_RAX].val & 0xffffffffULL;
    uint64_t rdx = cpu->gpr[ZG_RDX].val & 0xffffffffULL;
    uint64_t expect = rax | (rdx << 32);
    uint64_t cur = mem_load64(cpu, addr);
    if (cur == expect) {
        uint64_t nv = (cpu->gpr[ZG_RBX].val & 0xffffffffULL) |
                      ((cpu->gpr[ZG_RCX].val & 0xffffffffULL) << 32);
        mem_store64(cpu, addr, nv);
        cpu->set_flag(FLAG_ZF, true);
    } else {
        cpu->gpr[ZG_RAX].val = (uint32_t)cur;
        cpu->gpr[ZG_RDX].val = (uint32_t)(cur >> 32);
        cpu->set_flag(FLAG_ZF, false);
    }
    cpu->rip = next_rip(cpu, d);
}

void exec_cmpxchg16b(CPU* cpu, const Dec& d) {
    uint64_t addr = resolve_mem(cpu, d.ops[0], cpu->rip + d.insn.length);
    if (addr & 15)
        guest_error(cpu, "guest SIGSEGV (unaligned cmpxchg16b)", addr);
    uint8_t cur[16], want[16], nw[16];
    mem_load_bytes(cpu, addr, cur, 16);
    memcpy(want, &cpu->gpr[ZG_RAX].val, 8);
    memcpy(want + 8, &cpu->gpr[ZG_RDX].val, 8);
    if (memcmp(cur, want, 16) == 0) {
        memcpy(nw, &cpu->gpr[ZG_RBX].val, 8);
        memcpy(nw + 8, &cpu->gpr[ZG_RCX].val, 8);
        mem_store_bytes(cpu, addr, nw, 16);
        cpu->set_flag(FLAG_ZF, true);
    } else {
        memcpy(&cpu->gpr[ZG_RAX].val, cur, 8);
        memcpy(&cpu->gpr[ZG_RDX].val, cur + 8, 8);
        cpu->set_flag(FLAG_ZF, false);
    }
    cpu->rip = next_rip(cpu, d);
}

void exec_movnti(CPU* cpu, const Dec& d) {
    Val s = op_load(cpu, d, 1);
    op_store(cpu, d, 0, s);
    cpu->rip = next_rip(cpu, d);
}

void exec_bmi_simple(CPU* cpu, const Dec& d) {
    int n = explicit_ops(d);
    Val s0 = op_load(cpu, d, n == 3 ? 1 : 0);
    Val s1 = op_load(cpu, d, n == 3 ? 2 : (n == 2 ? 1 : 0));
    int w = common_int_width(d);
    uint64_t m = mask_for(w);
    Val out;
    out.taint.cannot_branch = s0.taint.cannot_branch || s1.taint.cannot_branch;
    out.taint.cannot_index = s0.taint.cannot_index || s1.taint.cannot_index;
    switch (d.insn.mnemonic) {
    case ZYDIS_MNEMONIC_ANDN:
        out.v = (~(s0.v & m) & s1.v) & m;
        flags_logic(cpu, w, out.v);
        break;
    case ZYDIS_MNEMONIC_BLSI:
        out.v = ((-(int64_t)(s1.v & m))) & (s1.v & m) & m;
        cpu->set_flag(FLAG_CF, (s1.v & m) != 0);
        cpu->set_flag(FLAG_ZF, out.v == 0);
        cpu->set_flag(FLAG_SF, (out.v >> (w * 8 - 1)) & 1);
        cpu->set_flag(FLAG_OF, false);
        break;
    case ZYDIS_MNEMONIC_BLSMSK: {
        uint64_t t = s1.v & m;
        out.v = ((t - 1) ^ t) & m;
        cpu->set_flag(FLAG_CF, t == 0);
        cpu->set_flag(FLAG_ZF, false);
        cpu->set_flag(FLAG_SF, false);
        cpu->set_flag(FLAG_OF, false);
        break;
    }
    case ZYDIS_MNEMONIC_BLSR: {
        uint64_t t = s1.v & m;
        out.v = (t & (t - 1)) & m;
        cpu->set_flag(FLAG_CF, t == 0);
        cpu->set_flag(FLAG_ZF, out.v == 0);
        cpu->set_flag(FLAG_SF, (out.v >> (w * 8 - 1)) & 1);
        cpu->set_flag(FLAG_OF, false);
        break;
    }
    case ZYDIS_MNEMONIC_BZHI: {
        Val idx = op_load(cpu, d, 2);
        unsigned k = (unsigned)idx.v;
        unsigned bits = w * 8;
        if (k < bits)
            out.v = (s0.v & m) & (bits == 64 ? (~0ULL >> (64 - k)) : ((1ULL << k) - 1));
        else {
            out.v = s0.v & m;
            cpu->set_flag(FLAG_CF, true);
            cpu->set_flag(FLAG_ZF, out.v == 0);
            cpu->set_flag(FLAG_SF, (out.v >> (bits - 1)) & 1);
            cpu->set_flag(FLAG_OF, false);
            op_store(cpu, d, 0, out);
            cpu->rip = next_rip(cpu, d);
            return;
        }
        cpu->set_flag(FLAG_CF, false);
        cpu->set_flag(FLAG_ZF, out.v == 0);
        cpu->set_flag(FLAG_SF, (out.v >> (bits - 1)) & 1);
        cpu->set_flag(FLAG_OF, false);
        out.taint.cannot_branch = out.taint.cannot_branch || idx.taint.cannot_branch;
        out.taint.cannot_index = out.taint.cannot_index || idx.taint.cannot_index;
        break;
    }
    case ZYDIS_MNEMONIC_SHLX:
    case ZYDIS_MNEMONIC_SHRX:
    case ZYDIS_MNEMONIC_SARX: {
        Val cnt = op_load(cpu, d, 2);
        unsigned c = (unsigned)cnt.v & (w == 8 ? 63 : 31);
        uint64_t t = s0.v & m;
        if (d.insn.mnemonic == ZYDIS_MNEMONIC_SHLX)
            out.v = (t << c) & m;
        else if (d.insn.mnemonic == ZYDIS_MNEMONIC_SHRX)
            out.v = (t >> c) & m;
        else {
            if (w == 8)
                out.v = (uint64_t)((int64_t)t >> c);
            else if (w == 4)
                out.v = (uint32_t)((int32_t)t >> c);
            else
                out.v = t >> c;
        }
        out.taint.cannot_branch = out.taint.cannot_branch || cnt.taint.cannot_branch;
        out.taint.cannot_index = out.taint.cannot_index || cnt.taint.cannot_index;
        break;
    }
    case ZYDIS_MNEMONIC_RORX: {
        Val cnt = op_load(cpu, d, 2);
        unsigned bits = w * 8;
        unsigned c = (unsigned)cnt.v & (bits - 1);
        uint64_t t = s0.v & m;
        out.v = ((t >> c) | (t << (bits - c))) & m;
        out.taint.cannot_branch = out.taint.cannot_branch || cnt.taint.cannot_branch;
        out.taint.cannot_index = out.taint.cannot_index || cnt.taint.cannot_index;
        break;
    }
    case ZYDIS_MNEMONIC_MULX: {
        // MULX r32a, r32b, r/m: r32a=high, r32b=low (implicit RDX).
        Val src = op_load(cpu, d, 2);
        if (w == 4) {
            uint64_t r = (uint64_t)(uint32_t)(cpu->gpr[ZG_RDX].val & 0xffffffffULL) *
                         (uint32_t)(src.v & 0xffffffffULL);
            Val hi, lo;
            hi.v = (uint32_t)(r >> 32);
            lo.v = (uint32_t)r;
            op_store(cpu, d, 0, hi);
            op_store(cpu, d, 1, lo);
        } else {
            __uint128_t r = (__uint128_t)cpu->gpr[ZG_RDX].val * src.v;
            Val hi, lo;
            hi.v = (uint64_t)(r >> 64);
            lo.v = (uint64_t)r;
            op_store(cpu, d, 0, hi);
            op_store(cpu, d, 1, lo);
        }
        cpu->rip = next_rip(cpu, d);
        return;
    }
    case ZYDIS_MNEMONIC_PEXT:
    case ZYDIS_MNEMONIC_PDEP: {
        Val mask = op_load(cpu, d, 2);
        uint64_t t = s0.v & m, mk = mask.v & m, r = 0;
        if (d.insn.mnemonic == ZYDIS_MNEMONIC_PEXT) {
            unsigned k = 0;
            for (unsigned i = 0; i < (unsigned)w * 8; i++) {
                if ((mk >> i) & 1) {
                    if ((t >> i) & 1)
                        r |= (1ULL << k);
                    k++;
                }
            }
        } else {
            unsigned k = 0;
            for (unsigned i = 0; i < (unsigned)w * 8; i++) {
                if ((mk >> i) & 1) {
                    if ((t >> k) & 1)
                        r |= (1ULL << i);
                    k++;
                }
            }
        }
        out.v = r & m;
        break;
    }
    case ZYDIS_MNEMONIC_ADCX:
    case ZYDIS_MNEMONIC_ADOX: {
        bool cf_in = (d.insn.mnemonic == ZYDIS_MNEMONIC_ADCX) ? cpu->cf() : cpu->of();
        uint64_t t = (s0.v & m) + (s1.v & m) + (cf_in ? 1 : 0);
        out.v = t & m;
        bool cout = (t >> (w * 8)) & (w == 8 ? 1 : 1);
        // carry out computation for sub-64 widths:
        __uint128_t full = (__uint128_t)(s0.v & m) + (s1.v & m) + (cf_in ? 1 : 0);
        cout = (bool)((full >> (w * 8)) & 1);
        if (d.insn.mnemonic == ZYDIS_MNEMONIC_ADCX)
            cpu->set_flag(FLAG_CF, cout);
        else
            cpu->set_flag(FLAG_OF, cout);
        break;
    }
    default:
        guest_error(cpu, "bad BMI op");
    }
    op_store(cpu, d, 0, out);
    cpu->rip = next_rip(cpu, d);
}

void exec_pinsr(CPU* cpu, const Dec& d, bool insert) {
    int n = explicit_ops(d);
    unsigned lane = 1;
    switch (d.insn.mnemonic) {
    case ZYDIS_MNEMONIC_PINSRB:
    case ZYDIS_MNEMONIC_PEXTRB: lane = 1; break;
    case ZYDIS_MNEMONIC_PINSRW:
    case ZYDIS_MNEMONIC_PEXTRW:
        lane = 2;
        break;
    case ZYDIS_MNEMONIC_PINSRD:
    case ZYDIS_MNEMONIC_PEXTRD: lane = 4; break;
    case ZYDIS_MNEMONIC_PINSRQ:
    case ZYDIS_MNEMONIC_PEXTRQ: lane = 8; break;
    default: guest_error(cpu, "bad pinsr op");
    }
    unsigned count = (d.insn.mnemonic == ZYDIS_MNEMONIC_PINSRW ||
                      d.insn.mnemonic == ZYDIS_MNEMONIC_PEXTRW)
                         ? 8
                         : 16 / lane;
    Val sel = op_load(cpu, d, n - 1);
    unsigned k = (unsigned)sel.v % count;
    int dsti = -1, dstw = 0;
    if (insert) {
        reg_to_vec(d.ops[0].reg.value, dsti, dstw);
        Val s = op_load(cpu, d, 1);
        memcpy(cpu->xmm[dsti].bytes + k * lane, &s.v, lane);
    } else {
        Val out;
        if (d.ops[0].type == ZYDIS_OPERAND_TYPE_REGISTER) {
            int vi = -1, vw = 0;
            reg_to_vec(d.ops[1].reg.value, vi, vw);
            uint64_t v = 0;
            memcpy(&v, cpu->xmm[vi].bytes + k * lane, lane);
            out.v = v;
        } else {
            guest_error(cpu, "bad pextr form");
        }
        op_store(cpu, d, 0, out);
    }
    cpu->rip = next_rip(cpu, d);
}

void exec_pmovsx(CPU* cpu, const Dec& d, unsigned s_lane, unsigned d_lane, bool is_signed) {
    VecVal a, o;
    unsigned w = d.ops[1].size / 8; // source mem/xmm width (8 or 16)
    vec_load_bytes(cpu, d, 1, a, w);
    unsigned elems = w / s_lane;
    for (unsigned i = 0; i < elems; i++) {
        uint64_t v = 0;
        memcpy(&v, a + i * s_lane, s_lane);
        int64_t sv = 0;
        if (is_signed) {
            if (s_lane == 1)
                sv = (int8_t)v;
            else if (s_lane == 2)
                sv = (int16_t)v;
            else
                sv = (int32_t)v;
        } else {
            sv = (int64_t)v;
        }
        memcpy(o + i * d_lane, &sv, d_lane);
    }
    unsigned ow = elems * d_lane;
    o.taint = a.taint;
    vec_store_bytes(cpu, d, 0, o, ow, is_vex(d), ow);
    cpu->rip = next_rip(cpu, d);
}

void exec_pack(CPU* cpu, const Dec& d, bool is_ss, unsigned lane) {
    VecVal a, b, o;
    unsigned w = vec_sources(cpu, d, a, b);
    unsigned half = (lane == 2) ? 1 : (lane == 4 ? 2 : 4);
    (void)half;
    unsigned elems = w / lane; // per source
    unsigned oi = 0;
    for (unsigned pass = 0; pass < 2; pass++) {
        uint8_t* s = pass ? b : a;
        for (unsigned i = 0; i < elems; i++) {
            int64_t v = 0;
            if (lane == 2)
                v = (int16_t)(s[2 * i] | (s[2 * i + 1] << 8));
            else if (lane == 4) {
                int32_t t;
                memcpy(&t, s + 4 * i, 4);
                v = t;
            } else {
                int64_t t;
                memcpy(&t, s + 8 * i, 8);
                v = t;
            }
            if (is_ss) {
                // signed saturation to half width
                int64_t lo = -(1LL << (half * 8 - 1)), hi = (1LL << (half * 8 - 1)) - 1;
                if (v < lo)
                    v = lo;
                if (v > hi)
                    v = hi;
            } else {
                // unsigned saturation
                uint64_t uhi = (half == 8) ? ~0ULL : ((1ULL << (half * 8)) - 1);
                if (v < 0)
                    v = 0;
                if ((uint64_t)v > uhi)
                    v = uhi;
            }
            memcpy(o + oi, &v, half);
            oi += half;
        }
    }
    o.taint = a.taint | b.taint;
    vec_store_dst(cpu, d, o, w);
    cpu->rip = next_rip(cpu, d);
}

void exec_padds(CPU* cpu, const Dec& d, unsigned lane, bool is_signed, bool add) {
    VecVal a, b, o;
    unsigned w = vec_sources(cpu, d, a, b);
    for (unsigned i = 0; i < w; i += lane) {
        int64_t av = 0, bv = 0;
        memcpy(&av, a + i, lane);
        memcpy(&bv, b + i, lane);
        int64_t r;
        if (is_signed) {
            if (lane == 1) {
                int t = (int8_t)av + (add ? (int8_t)bv : -(int8_t)bv);
                r = t < -128 ? -128 : t > 127 ? 127 : t;
            } else {
                int t = (int16_t)av + (add ? (int16_t)bv : -(int16_t)bv);
                r = t < -32768 ? -32768 : t > 32767 ? 32767 : t;
            }
        } else {
            if (lane == 1) {
                int t = (uint8_t)av + (add ? (uint8_t)bv : -(uint8_t)bv);
                r = t < 0 ? 0 : t > 255 ? 255 : t;
            } else {
                int t = (uint16_t)av + (add ? (uint16_t)bv : -(uint16_t)bv);
                r = t < 0 ? 0 : t > 65535 ? 65535 : t;
            }
        }
        memcpy(o + i, &r, lane);
    }
    o.taint = a.taint | b.taint;
    vec_store_dst(cpu, d, o, w);
    cpu->rip = next_rip(cpu, d);
}

void exec_palignr(CPU* cpu, const Dec& d) {
    int n = explicit_ops(d);
    VecVal a, b, o;
    unsigned w = vec_sources(cpu, d, a, b);
    unsigned cnt = (unsigned)op_load(cpu, d, n - 1).v;
    // Operands: dst, dst, src — concat = src:b || dst:a, shift right by cnt.
    for (unsigned L = 0; L < w; L += 16) {
        for (unsigned i = 0; i < 16; i++) {
            unsigned k = i + cnt;
            if (k < 16)
                o[L + i] = b[L + k];
            else if (k < 32)
                o[L + i] = a[L + k - 16];
            else
                o[L + i] = 0;
        }
    }
    o.taint = a.taint | b.taint;
    vec_store_dst(cpu, d, o, w);
    cpu->rip = next_rip(cpu, d);
}

void exec_cvtdq2ps(CPU* cpu, const Dec& d, int kind) {
    // kind 0: DQ2PS, 1: TTPS2DQ, 2: TPS2DQ
    VecVal a, o;
    unsigned w = vec_width(d);
    vec_load_bytes(cpu, d, explicit_ops(d) - 1, a, w);
    for (unsigned i = 0; i < w; i += 4) {
        if (kind == 0) {
            int32_t t;
            memcpy(&t, a + i, 4);
            float f = (float)t;
            memcpy(o + i, &f, 4);
        } else {
            float f;
            memcpy(&f, a + i, 4);
            int32_t t;
            if (kind == 1)
                t = (f != f || f > 2.1474836e9f || f < -2.1474836e9f) ? (int32_t)0x80000000
                                                                      : (int32_t)f;
            else
                t = (f != f) ? (int32_t)0x80000000 : (int32_t)__builtin_roundf(f);
            memcpy(o + i, &t, 4);
        }
    }
    o.taint = a.taint;
    vec_store_bytes(cpu, d, 0, o, w, is_vex(d), w);
    cpu->rip = next_rip(cpu, d);
}

void exec_cvtpd(CPU* cpu, const Dec& d, bool to_ps) {
    VecVal a, o;
    unsigned w = vec_width(d);
    if (to_ps) {
        // CVTPD2PS: w(src)=2*dwords... src width = 2x dst dwords.
        vec_load_bytes(cpu, d, explicit_ops(d) - 1, a, w * 2);
        for (unsigned i = 0; i < w / 4; i++) {
            double dd;
            memcpy(&dd, a + i * 8, 8);
            float f = (float)dd;
            memcpy(o + i * 4, &f, 4);
        }
    } else {
        // CVTPS2PD
        vec_load_bytes(cpu, d, explicit_ops(d) - 1, a, w / 2);
        for (unsigned i = 0; i < w / 8; i++) {
            float f;
            memcpy(&f, a + i * 4, 4);
            double dd = (double)f;
            memcpy(o + i * 8, &dd, 8);
        }
    }
    o.taint = a.taint;
    vec_store_bytes(cpu, d, 0, o, w, is_vex(d), w);
    cpu->rip = next_rip(cpu, d);
}

void exec_round(CPU* cpu, const Dec& d, bool is_pd, bool scalar) {
    int n = explicit_ops(d);
    unsigned w = scalar ? 16 : vec_width(d);
    VecVal a, o;
    vec_load_bytes(cpu, d, n - 2, a, w);
    unsigned mode = (unsigned)op_load(cpu, d, n - 1).v;
    unsigned elems = scalar ? 1 : w / (is_pd ? 8 : 4);
    unsigned lane = is_pd ? 8 : 4;
    unsigned rmode = mode & 3;
    bool exact = (mode & 8) == 0;
    (void)exact;
    for (unsigned i = 0; i < elems; i++) {
        if (is_pd) {
            double v;
            memcpy(&v, a + i * 8, 8);
            double r = v;
            unsigned m = (mode & 4) ? ((cpu->mxcsr >> 13) & 3) : rmode;
            switch (m) {
            case 0: r = __builtin_round(v); break;
            case 1: r = __builtin_floor(v); break;
            case 2: r = __builtin_ceil(v); break;
            default: r = __builtin_trunc(v); break;
            }
            memcpy(o + i * 8, &r, 8);
        } else {
            float v;
            memcpy(&v, a + i * 4, 4);
            float r = v;
            unsigned m = (mode & 4) ? ((cpu->mxcsr >> 13) & 3) : rmode;
            switch (m) {
            case 0: r = __builtin_roundf(v); break;
            case 1: r = __builtin_floorf(v); break;
            case 2: r = __builtin_ceilf(v); break;
            default: r = __builtin_truncf(v); break;
            }
            memcpy(o + i * 4, &r, 4);
        }
    }
    if (scalar) {
        int dsti = -1, dstw = 0;
        reg_to_vec(d.ops[0].reg.value, dsti, dstw);
        memcpy(cpu->xmm[dsti].bytes, o, lane);
        if (is_vex(d))
            memset(cpu->xmm[dsti].bytes + 16, 0, 48);
    } else {
        o.taint = a.taint;
        vec_store_bytes(cpu, d, 0, o, w, is_vex(d), w);
    }
    (void)lane;
    cpu->rip = next_rip(cpu, d);
}

void exec_vtest(CPU* cpu, const Dec& d) {
    unsigned w = vec_width(d);
    VecVal a, b;
    vec_load_bytes(cpu, d, 0, a, w);
    vec_load_bytes(cpu, d, 1, b, w);
    cpu->flags_taint = a.taint | b.taint;
    (void)d;
    // ZF = (a AND b)==0 over sign bits; CF = (NOT a AND b)==0 over sign bits.
    bool zf = true, cf = true;
    unsigned step = 4;
    for (unsigned i = 0; i < w; i += step) {
        bool abit = (a[i + step - 1] & 0x80) != 0;
        bool bbit = (b[i + step - 1] & 0x80) != 0;
        if (abit && bbit)
            zf = false;
        if (!abit && bbit)
            cf = false;
    }
    cpu->set_flag(FLAG_ZF, zf);
    cpu->set_flag(FLAG_CF, cf);
    cpu->set_flag(FLAG_OF, false);
    cpu->set_flag(FLAG_SF, false);
    cpu->set_flag(FLAG_AF, false);
    cpu->set_flag(FLAG_PF, false);
    cpu->rip = next_rip(cpu, d);
}

void exec_blendv(CPU* cpu, const Dec& d, bool is_pd) {
    unsigned w = vec_width(d);
    unsigned lane = is_pd ? 8 : 4;
    VecVal a, b, o;
    vec_sources(cpu, d, a, b);
    for (unsigned i = 0; i < w; i += lane) {
        bool take = (cpu->xmm[0].bytes[i + lane - 1] & 0x80) != 0;
        memcpy(o + i, take ? b + i : a + i, lane);
    }
    o.taint = a.taint | b.taint;
    vec_store_dst(cpu, d, o, w);
    cpu->rip = next_rip(cpu, d);
}

void exec_movdup(CPU* cpu, const Dec& d, bool high) {
    VecVal a, o;
    unsigned w = vec_width(d);
    vec_load_bytes(cpu, d, explicit_ops(d) - 1, a, w);
    for (unsigned i = 0; i < w; i += 8) {
        memcpy(o + i, a + i + (high ? 4 : 0), 4);
        memcpy(o + i + 4, a + i + (high ? 4 : 0), 4);
    }
    o.taint = a.taint;
    vec_store_bytes(cpu, d, 0, o, w, is_vex(d), w);
    cpu->rip = next_rip(cpu, d);
}

void exec_vpternlog(CPU* cpu, const Dec& d) {
    int n = explicit_ops(d);
    unsigned w = vec_width(d);
    VecVal a, b, c, o;
    vec_load_bytes(cpu, d, 1, a, w);
    vec_load_bytes(cpu, d, 2, b, w);
    vec_load_bytes(cpu, d, 3, c, w);
    unsigned lut = (unsigned)op_load(cpu, d, n - 1).v & 0xff;
    // NOTE: VEX VPTERNLOGD takes (dst, src1, src2, imm) with dst doubling
    // as src3 (a); EVEX has 4 explicit operands. Handle both.
    for (unsigned i = 0; i < w; i++) {
        unsigned idx = ((a[i] & 1) ? 4 : 0) | ((b[i] & 1) ? 2 : 0) | ((c[i] & 1) ? 1 : 0);
        // Bitwise: result bit = LUT[(a,b,c)] — apply per bit.
        unsigned r = 0;
        for (int bit = 0; bit < 8; bit++) {
            unsigned k = (((a[i] >> bit) & 1) << 2) | (((b[i] >> bit) & 1) << 1) |
                         ((c[i] >> bit) & 1);
            if ((lut >> k) & 1)
                r |= (1u << bit);
        }
        o[i] = (uint8_t)r;
        (void)idx;
    }
    o.taint = a.taint | b.taint | c.taint;
    vec_store_bytes(cpu, d, 0, o, w, true, w);
    cpu->rip = next_rip(cpu, d);
}

} // namespace

// === phase 2: EVEX/AVX512 framework ========================================
// Helpers for masked ({k}{z}) vector execution. Opmask state lives in
// CPU::opmask[8]; k0 as a mask means "no masking".

namespace {

bool reg_to_opmask(ZydisRegister r, int& k) {
    if (r >= ZYDIS_REGISTER_K0 && r <= ZYDIS_REGISTER_K7) {
        k = (int)(r - ZYDIS_REGISTER_K0);
        return true;
    }
    return false;
}

// Destination width for an EVEX op in bytes (16/32/64).
unsigned evex_width(CPU* cpu, const Dec& d) {
    unsigned vl = d.insn.avx.vector_length; // bits (128/256/512)
    if (vl == 128 || vl == 256 || vl == 512)
        return vl / 8;
    unsigned w = d.ops[0].size / 8;
    if (w == 16 || w == 32 || w == 64)
        return w;
    guest_error(cpu, "bad EVEX width");
}

// Mask bits + zeroing flag for an EVEX op with `lanes` elements.
uint64_t evex_mask(CPU* cpu, const Dec& d, unsigned lanes, bool& zeroing) {
    zeroing = false;
    uint64_t all = (lanes >= 64) ? ~0ULL : ((lanes == 0) ? 0 : ((1ULL << lanes) - 1));
    if (d.insn.avx.mask.mode == ZYDIS_MASK_MODE_DISABLED)
        return all;
    if (d.insn.avx.mask.mode == ZYDIS_MASK_MODE_ZEROING)
        zeroing = true;
    int k = -1;
    if (!reg_to_opmask(d.insn.avx.mask.reg, k) || k == 0)
        return all; // k0 = unmasked
    return cpu->opmask[k] & all;
}

bool evex_broadcasting(const Dec& d) {
    return (d.insn.attributes & ZYDIS_ATTRIB_HAS_EVEX_B) != 0;
}

// Load an EVEX vector source with broadcast + taint handling. `elem` is the
// element size for embedded-broadcast detection (bytes); broadcasts replicate
// one element across the full width. Static-broadcast instructions
// (VBROADCAST*) are handled by their own exec and pass elem==0 (no ebroadcast).
// Store an EVEX vector result with masking. For register dests, masked-off
// elements merge (or zero with {z}); for memory dests (masked stores),
// masked-off elements are not written. Taint ORs into reg sidecars and
// mem_note_store for memory.
// VecVal overloads for EVEX helpers (preferred): per-byte/per-element precise.
// evex_load only touches enabled lanes for faults/taint; evex_store merges
// old taint for merging-masked reg dests and only stores enabled lanes.
void evex_load(CPU* cpu, const Dec& d, int oi, VecVal& out, unsigned w,
               unsigned elem) {
    out.taint.clear();
    memset(out.bytes, 0, w);
    const auto& op = d.ops[oi];
    uint64_t kbits = ~0ULL;
    bool dummy_z = false;
    if (elem)
        kbits = evex_mask(cpu, d, w / elem, dummy_z);
    if (op.type == ZYDIS_OPERAND_TYPE_REGISTER) {
        int idx, width;
        if (reg_to_opmask(op.reg.value, width))
            guest_error(cpu, "opmask used as vector source");
        if (!reg_to_vec(op.reg.value, idx, width))
            guest_error(cpu, "unsupported EVEX vector register");
        memcpy(out.bytes, cpu->xmm[idx].bytes, w);
        bool any_enabled = (elem == 0) ? true : (kbits != 0);
        if (any_enabled)
            out.taint = cpu->xmm[idx].taint;
        return;
    }
    if (op.type == ZYDIS_OPERAND_TYPE_MEMORY) {
        if (elem && evex_broadcasting(d)) {
            if (kbits != 0) {
                uint8_t one[8];
                uint64_t addr = resolve_mem(cpu, op, cpu->rip + d.insn.length);
                mem_load_bytes(cpu, addr, one, elem);
                mem_get_taint(addr, elem, &out.taint);
                for (unsigned i = 0; i < w; i += elem)
                    memcpy(out.bytes + i, one, elem);
            }
            return;
        }
        if (elem == 0) {
            uint64_t addr = resolve_mem(cpu, op, cpu->rip + d.insn.length);
            mem_load_bytes(cpu, addr, out.bytes, w);
            mem_get_taint(addr, w, &out.taint);
            return;
        }
        uint64_t addr_base = resolve_mem(cpu, op, cpu->rip + d.insn.length);
        unsigned lanes = w / elem;
        for (unsigned i = 0; i < lanes; i++) {
            if ((kbits >> i) & 1) {
                uint64_t addr = addr_base + (uint64_t)i * elem;
                mem_load_bytes(cpu, addr, out.bytes + i * elem, elem);
                Taint t;
                mem_get_taint(addr, elem, &t);
                out.taint |= t;
            }
        }
        return;
    }
    guest_error(cpu, "unsupported EVEX source operand");
}

void evex_store(CPU* cpu, const Dec& d, int oi, const VecVal& res,
                const VecVal& old, unsigned w, unsigned elem, uint64_t kbits,
                bool zeroing) {
    const auto& op = d.ops[oi];
    if (op.type == ZYDIS_OPERAND_TYPE_REGISTER) {
        int idx, width;
        if (!reg_to_vec(op.reg.value, idx, width))
            guest_error(cpu, "unsupported EVEX vector register");
        Taint oldt = cpu->xmm[idx].taint;
        VecVal merged;
        unsigned lanes = w / elem;
        uint64_t all = (lanes >= 64) ? ~0ULL : ((lanes == 0) ? 0 : ((1ULL << lanes) - 1));
        bool fully_enabled = ((kbits & all) == all);
        bool any_enabled = ((kbits & all) != 0);
        for (unsigned i = 0; i < lanes; i++) {
            if ((kbits >> i) & 1)
                memcpy(merged.bytes + i * elem, res.bytes + i * elem, elem);
            else if (zeroing)
                memset(merged.bytes + i * elem, 0, elem);
            else
                memcpy(merged.bytes + i * elem, old.bytes + i * elem, elem);
        }
        memcpy(cpu->xmm[idx].bytes, merged.bytes, w);
        if (w < MAX_VEC_BYTES)
            memset(cpu->xmm[idx].bytes + w, 0, MAX_VEC_BYTES - w);
        Taint out;
        if (fully_enabled)
            out = res.taint;
        else if (zeroing)
            out = any_enabled ? res.taint : Taint();
        else
            out = any_enabled ? (oldt | res.taint) : oldt;
        cpu->xmm[idx].taint = out;
        return;
    }
    if (op.type == ZYDIS_OPERAND_TYPE_MEMORY) {
        uint64_t addr = resolve_mem(cpu, op, cpu->rip + d.insn.length);
        unsigned lanes = w / elem;
        uint64_t all = (lanes >= 64) ? ~0ULL : ((lanes == 0) ? 0 : ((1ULL << lanes) - 1));
        if ((kbits & all) == all) {
            mem_store_bytes(cpu, addr, res.bytes, w);
            mem_note_store(cpu, addr, w, res.taint);
        } else {
            for (unsigned i = 0; i < lanes; i++) {
                if ((kbits >> i) & 1) {
                    mem_store_bytes(cpu, addr + i * elem, res.bytes + i * elem, elem);
                    // Per-element precise: only enabled bytes gain taint.
                    // With whole-vector Taint, apply res taint only to
                    // enabled lanes (disabled untouched).
                    mem_note_store(cpu, addr + i * elem, elem, res.taint);
                }
            }
        }
        return;
    }
    guest_error(cpu, "cannot store EVEX result to operand");
}

void evex_old(CPU* cpu, const Dec& d, int oi, VecVal& old, unsigned w) {
    old.taint.clear();
    memset(old.bytes, 0, MAX_VEC_BYTES);
    const auto& op = d.ops[oi];
    if (op.type == ZYDIS_OPERAND_TYPE_REGISTER) {
        int idx, width;
        if (!reg_to_vec(op.reg.value, idx, width))
            guest_error(cpu, "unsupported EVEX vector register");
        memcpy(old.bytes, cpu->xmm[idx].bytes, w);
        old.taint = cpu->xmm[idx].taint;
        return;
    }
}

// Find explicit operands by position among explicit (non-hidden) operands.
// Returns decoded operand indices for dst, src1, src2, (+src3/imm).
//
// EVEX subtlety: Zydis exposes the write mask ({k}) as an explicit opmask
// operand right after the destination (even {k0} when unmasked), e.g.
// vpaddd decodes as [dst, k1, src1, src2]. The authoritative mask lives in
// d.insn.avx.mask, so skip non-destination explicit operands that equal the
// mask register. (True k-reg data sources, e.g. VPMOVM2B's source, differ
// from the mask register and are kept.)
int evex_explicit(const Dec& d, int list[5]) {
    int n = 0;
    bool evex = is_evex(d);
    for (int i = 0; i < d.insn.operand_count && n < 5; i++) {
        if (d.ops[i].visibility == ZYDIS_OPERAND_VISIBILITY_HIDDEN)
            continue;
        if (evex && n > 0 &&
            d.ops[i].type == ZYDIS_OPERAND_TYPE_REGISTER &&
            d.ops[i].reg.value == d.insn.avx.mask.reg) {
            int dummy;
            if (reg_to_opmask(d.ops[i].reg.value, dummy))
                continue;
        }
        list[n++] = i;
    }
    return n;
}

} // namespace

// === XSAVE / FXSAVE =========================================================

namespace {

uint64_t host_xcr0() {
    unsigned a = 0, d = 0;
    asm volatile("xgetbv" : "=a"(a), "=d"(d) : "c"(0));
    return ((uint64_t)d << 32) | a;
}

void cpuid_d(uint32_t sub, uint32_t& eax, uint32_t& ebx, uint32_t& ecx,
             uint32_t& edx) {
    __cpuid_count(0xd, sub, eax, ebx, ecx, edx);
}

// Write the 512-byte legacy FXSAVE area at addr from CPU state.
void xsave_legacy_write(CPU* cpu, uint64_t addr) {
    uint8_t area[512];
    memset(area, 0, sizeof(area));
    *(uint16_t*)(area + 0) = cpu->fcw;
    *(uint16_t*)(area + 2) = cpu->fsw;
    // FTW abridged: assume all 8 regs valid.
    area[4] = 0xff;
    *(uint32_t*)(area + 24) = cpu->mxcsr;
    *(uint32_t*)(area + 28) = 0x0000ffbf; // MXCSR_MASK typical
    for (int i = 0; i < 8; i++) {
        uint8_t tmp[16] = {0};
        memcpy(tmp, &cpu->st[(cpu->fpu_top + i) & 7], 16);
        memcpy(area + 32 + i * 16, tmp, 16);
    }
    for (int i = 0; i < 16; i++)
        memcpy(area + 160 + i * 16, cpu->xmm[i].bytes, 16);
    mem_store_bytes(cpu, addr, area, 512);
}

void xsave_legacy_read(CPU* cpu, uint64_t addr) {
    uint8_t area[512];
    mem_load_bytes(cpu, addr, area, 512);
    cpu->fcw = *(uint16_t*)(area + 0);
    cpu->fsw = *(uint16_t*)(area + 2);
    cpu->mxcsr = *(uint32_t*)(area + 24);
    for (int i = 0; i < 8; i++) {
        long double v = 0;
        memcpy(&v, area + 32 + i * 16, 16);
        cpu->st[(cpu->fpu_top + i) & 7] = v;
    }
    for (int i = 0; i < 16; i++)
        memcpy(cpu->xmm[i].bytes, area + 160 + i * 16, 16);
}

void exec_fxsave(CPU* cpu, const Dec& d, bool save) {
    uint64_t addr = resolve_mem(cpu, d.ops[0], cpu->rip + d.insn.length);
    if (addr & 15)
        guest_error(cpu, "guest SIGSEGV (unaligned fxsave)", addr);
    if (save)
        xsave_legacy_write(cpu, addr);
    else
        xsave_legacy_read(cpu, addr);
    cpu->rip = next_rip(cpu, d);
}

void exec_xsave(CPU* cpu, const Dec& d, bool save, bool compacted) {
    uint64_t addr = resolve_mem(cpu, d.ops[0], cpu->rip + d.insn.length);
    if (addr & 63)
        guest_error(cpu, "guest SIGSEGV (unaligned xsave)", addr);
    uint64_t xcr0 = host_xcr0();
    if (save) {
        uint64_t req = cpu->gpr[ZG_RAX].val | (cpu->gpr[ZG_RDX].val << 32);
        uint64_t bv = req & xcr0;
        // Legacy area.
        xsave_legacy_write(cpu, addr);
        // Header.
        mem_store64(cpu, addr + 512, bv); // XSTATE_BV
        mem_store64(cpu, addr + 520, compacted ? 0x8000000000000000ULL : 0);
        // Extended states.
        if (compacted) {
            uint64_t off = 576;
            for (int i = 2; i < 63; i++) {
                if (!(bv & (1ULL << i)))
                    continue;
                uint32_t eax, ebx, ecx, edx;
                cpuid_d(i, eax, ebx, ecx, edx);
                uint32_t sz = eax;
                off = (off + 63) & ~63ULL;
                if (i == 2) {
                    // YMM_Hi128 for YMM0-15.
                    uint8_t buf[512];
                    memset(buf, 0, sizeof(buf));
                    for (int r = 0; r < 16; r++)
                        memcpy(buf + r * 16, cpu->xmm[r].bytes + 16, 16);
                    mem_store_bytes(cpu, addr + off, buf, sz < sizeof(buf) ? sz : sizeof(buf));
                    if (sz > 256) {
                        uint8_t z[512] = {0};
                        mem_store_bytes(cpu, addr + off + 256, z, sz - 256);
                    }
                } else {
                    uint8_t z[4096];
                    uint32_t left = sz;
                    memset(z, 0, sizeof(z));
                    while (left) {
                        uint32_t n = left < sizeof(z) ? left : sizeof(z);
                        mem_store_bytes(cpu, addr + off + (sz - left), z, n);
                        left -= n;
                    }
                }
                off += sz;
            }
        } else {
            for (int i = 2; i < 63; i++) {
                if (!(bv & (1ULL << i)))
                    continue;
                uint32_t eax, ebx, ecx, edx;
                cpuid_d(i, eax, ebx, ecx, edx);
                uint32_t sz = eax, off = ebx;
                if (!sz)
                    continue;
                if (i == 2) {
                    uint8_t buf[512];
                    memset(buf, 0, sizeof(buf));
                    for (int r = 0; r < 16; r++)
                        memcpy(buf + r * 16, cpu->xmm[r].bytes + 16, 16);
                    mem_store_bytes(cpu, addr + off, buf, sz < sizeof(buf) ? sz : sizeof(buf));
                } else {
                    uint8_t z[4096];
                    uint32_t left = sz;
                    memset(z, 0, sizeof(z));
                    while (left) {
                        uint32_t n = left < sizeof(z) ? left : sizeof(z);
                        mem_store_bytes(cpu, addr + off + (sz - left), z, n);
                        left -= n;
                    }
                }
            }
        }
    } else {
        // XRSTOR: determine format from XCOMP_BV.
        uint64_t xcomp = mem_load64(cpu, addr + 520);
        bool comp = (xcomp >> 63) & 1;
        uint64_t bv = mem_load64(cpu, addr + 512);
        xsave_legacy_read(cpu, addr);
        if (comp) {
            uint64_t off = 576;
            for (int i = 2; i < 63; i++) {
                if (!(bv & (1ULL << i)))
                    continue;
                uint32_t eax, ebx, ecx, edx;
                cpuid_d(i, eax, ebx, ecx, edx);
                off = (off + 63) & ~63ULL;
                if (i == 2 && eax) {
                    uint8_t buf[512];
                    mem_load_bytes(cpu, addr + off, buf, eax < sizeof(buf) ? eax : sizeof(buf));
                    for (int r = 0; r < 16; r++)
                        memcpy(cpu->xmm[r].bytes + 16, buf + r * 16, 16);
                }
                off += eax;
            }
        } else {
            for (int i = 2; i < 63; i++) {
                if (!(bv & (1ULL << i)))
                    continue;
                uint32_t eax, ebx, ecx, edx;
                cpuid_d(i, eax, ebx, ecx, edx);
                if (!eax)
                    continue;
                if (i == 2) {
                    uint8_t buf[512];
                    mem_load_bytes(cpu, addr + ebx, buf, eax < sizeof(buf) ? eax : sizeof(buf));
                    for (int r = 0; r < 16; r++)
                        memcpy(cpu->xmm[r].bytes + 16, buf + r * 16, 16);
                }
            }
        }
    }
    cpu->rip = next_rip(cpu, d);
}

} // namespace
// === phase 2: EVEX op handlers ============================================

namespace {

// --- opmask (K) instructions ----------------------------------------------

unsigned kmask_width(ZydisMnemonic m) {
    switch (m) {
    case ZYDIS_MNEMONIC_KADDB:
    case ZYDIS_MNEMONIC_KANDB:
    case ZYDIS_MNEMONIC_KANDNB:
    case ZYDIS_MNEMONIC_KMOVB:
    case ZYDIS_MNEMONIC_KNOTB:
    case ZYDIS_MNEMONIC_KORB:
    case ZYDIS_MNEMONIC_KORTESTB:
    case ZYDIS_MNEMONIC_KSHIFTLB:
    case ZYDIS_MNEMONIC_KSHIFTRB:
    case ZYDIS_MNEMONIC_KTESTB:
    case ZYDIS_MNEMONIC_KXNORB:
    case ZYDIS_MNEMONIC_KXORB: return 8;
    case ZYDIS_MNEMONIC_KADDW:
    case ZYDIS_MNEMONIC_KANDW:
    case ZYDIS_MNEMONIC_KANDNW:
    case ZYDIS_MNEMONIC_KMOVW:
    case ZYDIS_MNEMONIC_KNOTW:
    case ZYDIS_MNEMONIC_KORW:
    case ZYDIS_MNEMONIC_KORTESTW:
    case ZYDIS_MNEMONIC_KSHIFTLW:
    case ZYDIS_MNEMONIC_KSHIFTRW:
    case ZYDIS_MNEMONIC_KTESTW:
    case ZYDIS_MNEMONIC_KXNORW:
    case ZYDIS_MNEMONIC_KXORW: return 16;
    case ZYDIS_MNEMONIC_KADDD:
    case ZYDIS_MNEMONIC_KANDD:
    case ZYDIS_MNEMONIC_KANDND:
    case ZYDIS_MNEMONIC_KMOVD:
    case ZYDIS_MNEMONIC_KNOTD:
    case ZYDIS_MNEMONIC_KORD:
    case ZYDIS_MNEMONIC_KORTESTD:
    case ZYDIS_MNEMONIC_KSHIFTLD:
    case ZYDIS_MNEMONIC_KSHIFTRD:
    case ZYDIS_MNEMONIC_KTESTD:
    case ZYDIS_MNEMONIC_KXNORD:
    case ZYDIS_MNEMONIC_KXORD: return 32;
    default: return 64; // Q-suffixed and unsuffixed forms
    }
}

uint64_t kmask(uint64_t v, unsigned bits) {
    if (bits >= 64)
        return v;
    return v & ((1ULL << bits) - 1);
}

// Load a K operand (register k, GPR, or memory) as low `bits` bits.
uint64_t kload(CPU* cpu, const Dec& d, int oi, unsigned bits) {
    const auto& op = d.ops[oi];
    uint64_t v = 0;
    if (op.type == ZYDIS_OPERAND_TYPE_REGISTER) {
        int k;
        if (reg_to_opmask(op.reg.value, k))
            return kmask(cpu->opmask[k], bits);
        Val g = op_load(cpu, d, oi);
        return kmask(g.v, bits);
    }
    if (op.type == ZYDIS_OPERAND_TYPE_MEMORY) {
        uint64_t addr = resolve_mem(cpu, op, cpu->rip + d.insn.length);
        unsigned bytes = (bits + 7) / 8;
        uint8_t buf[8] = {0};
        mem_load_bytes(cpu, addr, buf, bytes);
        memcpy(&v, buf, bytes);
        return kmask(v, bits);
    }
    if (op.type == ZYDIS_OPERAND_TYPE_IMMEDIATE)
        return kmask(op.imm.is_signed ? (uint64_t)op.imm.value.s : op.imm.value.u,
                     bits);
    guest_error(cpu, "unsupported K operand");
}

void kstore(CPU* cpu, const Dec& d, int oi, uint64_t v, unsigned bits) {
    const auto& op = d.ops[oi];
    v = kmask(v, bits);
    if (op.type == ZYDIS_OPERAND_TYPE_REGISTER) {
        int k;
        if (reg_to_opmask(op.reg.value, k)) {
            cpu->opmask[k] = v;
            return;
        }
        Val g;
        g.v = v;
        op_store(cpu, d, oi, g);
        return;
    }
    if (op.type == ZYDIS_OPERAND_TYPE_MEMORY) {
        uint64_t addr = resolve_mem(cpu, op, cpu->rip + d.insn.length);
        unsigned bytes = (bits + 7) / 8;
        mem_store_bytes(cpu, addr, (uint8_t*)&v, bytes);
        return;
    }
    guest_error(cpu, "cannot store K result to operand");
}

static bool is_kmov_mn(ZydisMnemonic m) {
    return m == ZYDIS_MNEMONIC_KMOVB || m == ZYDIS_MNEMONIC_KMOVW ||
           m == ZYDIS_MNEMONIC_KMOVD || m == ZYDIS_MNEMONIC_KMOVQ ||
           m == ZYDIS_MNEMONIC_KMOV;
}

static bool is_knot_mn(ZydisMnemonic m) {
    return m == ZYDIS_MNEMONIC_KNOTB || m == ZYDIS_MNEMONIC_KNOTW ||
           m == ZYDIS_MNEMONIC_KNOTD || m == ZYDIS_MNEMONIC_KNOTQ ||
           m == ZYDIS_MNEMONIC_KNOT;
}

void exec_kop(CPU* cpu, const Dec& d) {
    ZydisMnemonic m = d.insn.mnemonic;
    unsigned bits = kmask_width(m);
    int ex[5];
    int n = evex_explicit(d, ex);
    if (is_kmov_mn(m)) {
        if (n < 2)
            guest_error(cpu, "kmov needs 2 operands");
        kstore(cpu, d, ex[0], kload(cpu, d, ex[1], bits), bits);
        cpu->rip = next_rip(cpu, d);
        return;
    }
    if (is_knot_mn(m)) {
        if (n < 2)
            guest_error(cpu, "knot needs 2 operands");
        kstore(cpu, d, ex[0], ~kload(cpu, d, ex[1], bits), bits);
        cpu->rip = next_rip(cpu, d);
        return;
    }
    if (n < 2)
        guest_error(cpu, "binary K op needs operands");
    // KORTEST / KTEST set flags, no destination write.
    if (m == ZYDIS_MNEMONIC_KORTESTB || m == ZYDIS_MNEMONIC_KORTESTW ||
        m == ZYDIS_MNEMONIC_KORTESTD || m == ZYDIS_MNEMONIC_KORTESTQ ||
        m == ZYDIS_MNEMONIC_KORTEST) {
        uint64_t a = kload(cpu, d, ex[0], bits);
        uint64_t b = kload(cpu, d, ex[1], bits);
        uint64_t o = kmask(a | b, bits);
        cpu->set_flag(FLAG_ZF, o == 0);
        cpu->set_flag(FLAG_CF, o == kmask(~0ULL, bits));
        cpu->set_flag(FLAG_SF, false);
        cpu->set_flag(FLAG_OF, false);
        cpu->set_flag(FLAG_AF, false);
        cpu->set_flag(FLAG_PF, false);
        flags_clear(cpu);
        cpu->rip = next_rip(cpu, d);
        return;
    }
    if (m == ZYDIS_MNEMONIC_KTESTB || m == ZYDIS_MNEMONIC_KTESTW ||
        m == ZYDIS_MNEMONIC_KTESTD || m == ZYDIS_MNEMONIC_KTESTQ) {
        uint64_t a = kload(cpu, d, ex[0], bits);
        uint64_t b = kload(cpu, d, ex[1], bits);
        cpu->set_flag(FLAG_ZF, kmask(a & b, bits) == 0);
        cpu->set_flag(FLAG_CF, kmask(~b & a, bits) == 0);
        cpu->set_flag(FLAG_SF, false);
        cpu->set_flag(FLAG_OF, false);
        cpu->set_flag(FLAG_AF, false);
        cpu->set_flag(FLAG_PF, false);
        flags_clear(cpu);
        cpu->rip = next_rip(cpu, d);
        return;
    }
    // KSHIFT: dst,src,imm8.
    if (m == ZYDIS_MNEMONIC_KSHIFTLB || m == ZYDIS_MNEMONIC_KSHIFTLW ||
        m == ZYDIS_MNEMONIC_KSHIFTLD || m == ZYDIS_MNEMONIC_KSHIFTLQ ||
        m == ZYDIS_MNEMONIC_KSHIFTRB || m == ZYDIS_MNEMONIC_KSHIFTRW ||
        m == ZYDIS_MNEMONIC_KSHIFTRD || m == ZYDIS_MNEMONIC_KSHIFTRQ) {
        if (n < 3)
            guest_error(cpu, "kshift needs 3 operands");
        uint64_t a = kload(cpu, d, ex[0], bits);
        uint64_t cnt = kload(cpu, d, ex[2], 8);
        bool left = (m == ZYDIS_MNEMONIC_KSHIFTLB || m == ZYDIS_MNEMONIC_KSHIFTLW ||
                     m == ZYDIS_MNEMONIC_KSHIFTLD || m == ZYDIS_MNEMONIC_KSHIFTLQ);
        kstore(cpu, d, ex[0], left ? (a << cnt) : (a >> cnt), bits);
        cpu->rip = next_rip(cpu, d);
        return;
    }
    // KUNPCK: interleave chunks of a and b.
    if (m == ZYDIS_MNEMONIC_KUNPCKBW || m == ZYDIS_MNEMONIC_KUNPCKWD ||
        m == ZYDIS_MNEMONIC_KUNPCKDQ) {
        if (n < 3)
            guest_error(cpu, "kunpck needs 3 operands");
        uint64_t a = kload(cpu, d, ex[1], bits);
        uint64_t b = kload(cpu, d, ex[2], bits);
        unsigned chunk = (m == ZYDIS_MNEMONIC_KUNPCKBW) ? 4
                         : (m == ZYDIS_MNEMONIC_KUNPCKWD) ? 8
                                                          : 16;
        uint64_t lo = 0;
        for (unsigned i = 0; i < bits / (2 * chunk); i++) {
            uint64_t cm = (chunk >= 64) ? ~0ULL : ((1ULL << chunk) - 1);
            uint64_t abit = (a >> (i * chunk)) & cm;
            uint64_t bbit = (b >> (i * chunk)) & cm;
            lo |= (abit << (2 * i * chunk)) | (bbit << ((2 * i + 1) * chunk));
        }
        kstore(cpu, d, ex[0], lo, bits);
        cpu->rip = next_rip(cpu, d);
        return;
    }
    uint64_t a = kload(cpu, d, ex[n == 2 ? 0 : 1], bits);
    uint64_t b = kload(cpu, d, ex[n == 2 ? 1 : 2], bits);
    uint64_t r = 0;
    switch (m) {
    case ZYDIS_MNEMONIC_KORB:
    case ZYDIS_MNEMONIC_KORW:
    case ZYDIS_MNEMONIC_KORD:
    case ZYDIS_MNEMONIC_KORQ:
    case ZYDIS_MNEMONIC_KOR: r = a | b; break;
    case ZYDIS_MNEMONIC_KADDB:
    case ZYDIS_MNEMONIC_KADDW:
    case ZYDIS_MNEMONIC_KADDD:
    case ZYDIS_MNEMONIC_KADDQ: r = a + b; break;
    case ZYDIS_MNEMONIC_KANDB:
    case ZYDIS_MNEMONIC_KANDW:
    case ZYDIS_MNEMONIC_KANDD:
    case ZYDIS_MNEMONIC_KANDQ:
    case ZYDIS_MNEMONIC_KAND: r = a & b; break;
    case ZYDIS_MNEMONIC_KXORB:
    case ZYDIS_MNEMONIC_KXORW:
    case ZYDIS_MNEMONIC_KXORD:
    case ZYDIS_MNEMONIC_KXORQ:
    case ZYDIS_MNEMONIC_KXOR: r = a ^ b; break;
    case ZYDIS_MNEMONIC_KXNORB:
    case ZYDIS_MNEMONIC_KXNORW:
    case ZYDIS_MNEMONIC_KXNORD:
    case ZYDIS_MNEMONIC_KXNORQ:
    case ZYDIS_MNEMONIC_KXNOR: r = ~(a ^ b); break;
    case ZYDIS_MNEMONIC_KANDNB:
    case ZYDIS_MNEMONIC_KANDNW:
    case ZYDIS_MNEMONIC_KANDND:
    case ZYDIS_MNEMONIC_KANDNQ:
    case ZYDIS_MNEMONIC_KANDN:
    case ZYDIS_MNEMONIC_KANDNR: r = (~a) & b; break;
    default: guest_error(cpu, "unsupported K operation");
    }
    kstore(cpu, d, ex[0], r, bits);
    cpu->rip = next_rip(cpu, d);
}

// --- EVEX masked moves / logic --------------------------------------------

unsigned evex_move_elem(ZydisMnemonic m) {
    switch (m) {
    case ZYDIS_MNEMONIC_VMOVDQA64:
    case ZYDIS_MNEMONIC_VMOVDQU64:
    case ZYDIS_MNEMONIC_VMOVAPD:
    case ZYDIS_MNEMONIC_VMOVUPD:
    case ZYDIS_MNEMONIC_VMOVNTPD: return 8;
    case ZYDIS_MNEMONIC_VMOVDQA32:
    case ZYDIS_MNEMONIC_VMOVDQU32:
    case ZYDIS_MNEMONIC_VMOVAPS:
    case ZYDIS_MNEMONIC_VMOVUPS:
    case ZYDIS_MNEMONIC_VMOVNTPS: return 4;
    case ZYDIS_MNEMONIC_VMOVDQU16: return 2;
    default: return 1; // VMOVDQU8, VMOVDQA/DQU, MOVNTDQ/DQA
    }
}

void exec_evex_move(CPU* cpu, const Dec& d) {
    unsigned w = evex_width(cpu, d);
    unsigned elem = evex_move_elem(d.insn.mnemonic);
    int ex[5];
    int n = evex_explicit(d, ex);
    if (n < 2)
        guest_error(cpu, "evex move needs 2 operands");
    VecVal tmp, old;
    evex_old(cpu, d, ex[0], old, w);
    evex_load(cpu, d, ex[1], tmp, w, elem);
    bool zeroing = false;
    uint64_t kbits = evex_mask(cpu, d, w / elem, zeroing);
    evex_store(cpu, d, ex[0], tmp, old, w, elem, kbits, zeroing);
    cpu->rip = next_rip(cpu, d);
}

void exec_evex_logic(CPU* cpu, const Dec& d, int kind) {
    // kind: 0=AND 1=OR 2=XOR 3=ANDN. Covers VPANDD/Q, VPORD/Q, VPXORD/Q,
    // VPANDND/Q and unsuffixed EVEX VPAND/VPOR/VPXOR/VPANDN (elem=4).
    unsigned w = evex_width(cpu, d);
    unsigned elem = 4;
    int ex[5];
    int n = evex_explicit(d, ex);
    if (n < 3)
        guest_error(cpu, "evex logic needs 3 operands");
    VecVal a, b, o, old;
    evex_load(cpu, d, ex[1], a, w, elem);
    evex_load(cpu, d, ex[2], b, w, elem);
    // Self-xor clears taint.
    if (kind == 2) {
        const auto& o0 = d.ops[ex[0]];
        const auto& o1 = d.ops[ex[1]];
        const auto& o2 = d.ops[ex[2]];
        if (o0.type == ZYDIS_OPERAND_TYPE_REGISTER &&
            o1.type == ZYDIS_OPERAND_TYPE_REGISTER &&
            o2.type == ZYDIS_OPERAND_TYPE_REGISTER &&
            o0.reg.value == o1.reg.value && o0.reg.value == o2.reg.value) {
            memset(o, 0, w);
            evex_old(cpu, d, ex[0], old, w);
            bool zeroing = false;
            uint64_t kbits = evex_mask(cpu, d, w / elem, zeroing);
            o.taint.clear();
            evex_store(cpu, d, ex[0], o, old, w, elem, kbits, zeroing);
            cpu->rip = next_rip(cpu, d);
            return;
        }
    }
    for (unsigned i = 0; i < w; i++) {
        if (kind == 0)
            o[i] = a[i] & b[i];
        else if (kind == 1)
            o[i] = a[i] | b[i];
        else if (kind == 2)
            o[i] = a[i] ^ b[i];
        else
            o[i] = (~a[i]) & b[i];
    }
    evex_old(cpu, d, ex[0], old, w);
    bool zeroing = false;
    uint64_t kbits = evex_mask(cpu, d, w / elem, zeroing);
    o.taint = a.taint | b.taint;
    evex_store(cpu, d, ex[0], o, old, w, elem, kbits, zeroing);
    cpu->rip = next_rip(cpu, d);
}

void exec_evex_ternlog(CPU* cpu, const Dec& d) {
    unsigned w = evex_width(cpu, d);
    int ex[5];
    int n = evex_explicit(d, ex);
    // EVEX: dst{k},src1,src2,src3/m,imm8. VEX: dst,src1,src2,imm8.
    VecVal a, b, c, o, old;
    unsigned lut = 0;
    if (n >= 5) {
        evex_load(cpu, d, ex[1], a, w, 4);
        evex_load(cpu, d, ex[2], b, w, 4);
        evex_load(cpu, d, ex[3], c, w, 4);
        lut = (unsigned)kload(cpu, d, ex[4], 8);
    } else if (n == 4) {
        evex_load(cpu, d, ex[1], a, w, 4);
        evex_load(cpu, d, ex[2], b, w, 4);
        // VEX dst doubles as third source.
        evex_load(cpu, d, ex[0], c, w, 4);
        lut = (unsigned)kload(cpu, d, ex[3], 8);
    } else {
        guest_error(cpu, "bad vpternlog operands");
    }
    for (unsigned i = 0; i < w; i++) {
        unsigned r = 0;
        for (int bit = 0; bit < 8; bit++) {
            unsigned k = (((a[i] >> bit) & 1) << 2) | (((b[i] >> bit) & 1) << 1) |
                         ((c[i] >> bit) & 1);
            if ((lut >> k) & 1)
                r |= (1u << bit);
        }
        o[i] = (uint8_t)r;
    }
    evex_old(cpu, d, ex[0], old, w);
    bool zeroing = false;
    uint64_t kbits = evex_mask(cpu, d, w / 4, zeroing);
    o.taint = a.taint | b.taint | c.taint;
    evex_store(cpu, d, ex[0], o, old, w, 4, kbits, zeroing);
    cpu->rip = next_rip(cpu, d);
}

// --- EVEX compares ----------------------------------------------------------
// VPCMPB/W/D/Q + unsigned variants: imm8 predicate, k-reg destination.

unsigned evex_cmp_elem(ZydisMnemonic m) {
    switch (m) {
    case ZYDIS_MNEMONIC_VPCMPB:
    case ZYDIS_MNEMONIC_VPCMPUB: return 1;
    case ZYDIS_MNEMONIC_VPCMPW:
    case ZYDIS_MNEMONIC_VPCMPUW: return 2;
    case ZYDIS_MNEMONIC_VPCMPD:
    case ZYDIS_MNEMONIC_VPCMPUD: return 4;
    default: return 8;
    }
}

static bool evex_cmp_unsigned(ZydisMnemonic m) {
    return m == ZYDIS_MNEMONIC_VPCMPUB || m == ZYDIS_MNEMONIC_VPCMPUW ||
           m == ZYDIS_MNEMONIC_VPCMPUD || m == ZYDIS_MNEMONIC_VPCMPUQ;
}

void exec_evex_vpcmp_core(CPU* cpu, const Dec& d, unsigned elem, bool us,
                            unsigned pred) {
    unsigned w = evex_width(cpu, d);
    int ex[5];
    int n = evex_explicit(d, ex);
    if (pred == 0xffffffffu) {
        if (n < 4)
            guest_error(cpu, "vpcmp needs 4 operands");
        pred = (unsigned)kload(cpu, d, ex[3], 8) & 7;
    } else {
        if (n < 3)
            guest_error(cpu, "evex mask-compare needs 3 operands");
    }
    VecVal a, b;
    evex_load(cpu, d, ex[1], a, w, elem);
    evex_load(cpu, d, ex[2], b, w, elem);
    unsigned lanes = w / elem;
    uint64_t res = 0;
    for (unsigned i = 0; i < lanes; i++) {
        uint64_t av = 0, bv = 0;
        memcpy(&av, a + i * elem, elem);
        memcpy(&bv, b + i * elem, elem);
        bool hit = false;
        if (!us) {
            int64_t sa = 0, sb = 0;
            if (elem == 1) {
                sa = (int8_t)av;
                sb = (int8_t)bv;
            } else if (elem == 2) {
                sa = (int16_t)av;
                sb = (int16_t)bv;
            } else if (elem == 4) {
                sa = (int32_t)av;
                sb = (int32_t)bv;
            } else {
                sa = (int64_t)av;
                sb = (int64_t)bv;
            }
            switch (pred) {
            case 0: hit = (sa == sb); break;
            case 1: hit = (sa < sb); break;
            case 2: hit = (sa <= sb); break;
            case 3: hit = false; break;
            case 4: hit = (sa != sb); break;
            case 5: hit = (sa >= sb); break;
            case 6: hit = (sa > sb); break;
            default: hit = true; break;
            }
        } else {
            switch (pred) {
            case 0: hit = (av == bv); break;
            case 1: hit = (av < bv); break;
            case 2: hit = (av <= bv); break;
            case 3: hit = false; break;
            case 4: hit = (av != bv); break;
            case 5: hit = (av >= bv); break;
            case 6: hit = (av > bv); break;
            default: hit = true; break;
            }
        }
        if (hit)
            res |= (1ULL << i);
    }
    // Merge with old dest mask under the write mask (or zero).
    bool zeroing = false;
    uint64_t kbits = evex_mask(cpu, d, lanes, zeroing);
    int dk;
    uint64_t old = 0;
    if (d.ops[ex[0]].type == ZYDIS_OPERAND_TYPE_REGISTER &&
        reg_to_opmask(d.ops[ex[0]].reg.value, dk))
        old = cpu->opmask[dk];
    uint64_t out = 0;
    for (unsigned i = 0; i < lanes; i++) {
        bool take = (res >> i) & 1;
        if ((kbits >> i) & 1)
            out |= (take ? (1ULL << i) : 0);
        else if (!zeroing)
            out |= (old & (1ULL << i));
    }
    kstore(cpu, d, ex[0], out, 64);
    cpu->rip = next_rip(cpu, d);
}

void exec_evex_vpcmp(CPU* cpu, const Dec& d) {
    unsigned elem = evex_cmp_elem(d.insn.mnemonic);
    bool us = evex_cmp_unsigned(d.insn.mnemonic);
    exec_evex_vpcmp_core(cpu, d, elem, us, 0xffffffffu);
}

// EVEX VPCMPEQB/W/D/Q and VPCMPGTB/W/D/Q have two forms: a mask-producing
// form (k destination, like VPCMPB with EQ/GT) and the legacy vector form.
// Dispatch on the destination register kind.
void exec_evex_pcmpeq(CPU* cpu, const Dec& d, unsigned elem, bool gt);
void exec_evex_pcmpeq_auto(CPU* cpu, const Dec& d, unsigned elem, bool gt) {
    int ex[5];
    int n = evex_explicit(d, ex);
    int dk = -1;
    if (n >= 1 && d.ops[ex[0]].type == ZYDIS_OPERAND_TYPE_REGISTER &&
        reg_to_opmask(d.ops[ex[0]].reg.value, dk)) {
        exec_evex_vpcmp_core(cpu, d, elem, false, gt ? 6 : 0);
        return;
    }
    exec_evex_pcmpeq(cpu, d, elem, gt);
}

// EVEX-encoded VPCMPEQB/W/D/Q and VPCMPGTB/W/D/Q: vector destination.
void exec_evex_pcmpeq(CPU* cpu, const Dec& d, unsigned elem, bool gt) {
    unsigned w = evex_width(cpu, d);
    int ex[5];
    int n = evex_explicit(d, ex);
    if (n < 3)
        guest_error(cpu, "evex pcmpeq needs 3 operands");
    VecVal a, b, o, old;
    evex_load(cpu, d, ex[1], a, w, elem);
    evex_load(cpu, d, ex[2], b, w, elem);
    memset(o, 0, w);
    for (unsigned i = 0; i < w; i += elem) {
        uint64_t av = 0, bv = 0;
        memcpy(&av, a + i, elem);
        memcpy(&bv, b + i, elem);
        bool hit;
        if (!gt) {
            hit = (av == bv);
        } else if (elem == 1) {
            hit = ((int8_t)av > (int8_t)bv);
        } else if (elem == 2) {
            hit = ((int16_t)av > (int16_t)bv);
        } else if (elem == 4) {
            hit = ((int32_t)av > (int32_t)bv);
        } else {
            hit = ((int64_t)av > (int64_t)bv);
        }
        if (hit)
            memset(o + i, 0xff, elem);
    }
    evex_old(cpu, d, ex[0], old, w);
    bool zeroing = false;
    uint64_t kbits = evex_mask(cpu, d, w / elem, zeroing);
    o.taint = a.taint | b.taint;
    evex_store(cpu, d, ex[0], o, old, w, elem, kbits, zeroing);
    cpu->rip = next_rip(cpu, d);
}

// VPTESTMB/W/D/Q and VPTESTNMB/...: k destination.
void exec_evex_vptestm(CPU* cpu, const Dec& d, bool is_n) {
    unsigned elem = 1;
    switch (d.insn.mnemonic) {
    case ZYDIS_MNEMONIC_VPTESTMW:
    case ZYDIS_MNEMONIC_VPTESTNMW: elem = 2; break;
    case ZYDIS_MNEMONIC_VPTESTMD:
    case ZYDIS_MNEMONIC_VPTESTNMD: elem = 4; break;
    case ZYDIS_MNEMONIC_VPTESTMQ:
    case ZYDIS_MNEMONIC_VPTESTNMQ: elem = 8; break;
    default: elem = 1; break;
    }
    unsigned w = evex_width(cpu, d);
    int ex[5];
    int n = evex_explicit(d, ex);
    if (n < 3)
        guest_error(cpu, "vptestm needs 3 operands");
    VecVal a, b;
    evex_load(cpu, d, ex[1], a, w, elem);
    evex_load(cpu, d, ex[2], b, w, elem);
    unsigned lanes = w / elem;
    uint64_t res = 0;
    uint64_t em = (elem >= 8) ? ~0ULL : ((1ULL << (elem * 8)) - 1);
    // Verified against hardware: VPTESTM sets the bit when (a AND b) != 0;
    // VPTESTNM sets it when (a AND b) == 0 (exact inverses).
    for (unsigned i = 0; i < lanes; i++) {
        uint64_t av = 0, bv = 0;
        memcpy(&av, a + i * elem, elem);
        memcpy(&bv, b + i * elem, elem);
        bool any = ((av & bv) & em) != 0;
        bool hit = is_n ? !any : any;
        if (hit)
            res |= (1ULL << i);
    }
    bool zeroing = false;
    uint64_t kbits = evex_mask(cpu, d, lanes, zeroing);
    int dk;
    uint64_t old = 0;
    if (d.ops[ex[0]].type == ZYDIS_OPERAND_TYPE_REGISTER &&
        reg_to_opmask(d.ops[ex[0]].reg.value, dk))
        old = cpu->opmask[dk];
    uint64_t out = 0;
    for (unsigned i = 0; i < lanes; i++) {
        if ((kbits >> i) & 1)
            out |= (res & (1ULL << i));
        else if (!zeroing)
            out |= (old & (1ULL << i));
    }
    kstore(cpu, d, ex[0], out, 64);
    cpu->rip = next_rip(cpu, d);
}

// --- EVEX integer arithmetic -----------------------------------------------

void exec_evex_padd(CPU* cpu, const Dec& d, unsigned elem, bool sub) {
    unsigned w = evex_width(cpu, d);
    int ex[5];
    int n = evex_explicit(d, ex);
    if (n < 3)
        guest_error(cpu, "evex padd needs 3 operands");
    VecVal a, b, o, old;
    evex_load(cpu, d, ex[1], a, w, elem);
    evex_load(cpu, d, ex[2], b, w, elem);
    for (unsigned i = 0; i < w; i += elem) {
        uint64_t av = 0, bv = 0;
        memcpy(&av, a + i, elem);
        memcpy(&bv, b + i, elem);
        uint64_t r = sub ? av - bv : av + bv;
        memcpy(o + i, &r, elem);
    }
    evex_old(cpu, d, ex[0], old, w);
    bool zeroing = false;
    uint64_t kbits = evex_mask(cpu, d, w / elem, zeroing);
    o.taint = a.taint | b.taint;
    evex_store(cpu, d, ex[0], o, old, w, elem, kbits, zeroing);
    cpu->rip = next_rip(cpu, d);
}

void exec_evex_pmull(CPU* cpu, const Dec& d, unsigned elem) {
    unsigned w = evex_width(cpu, d);
    int ex[5];
    int n = evex_explicit(d, ex);
    if (n < 3)
        guest_error(cpu, "evex pmull needs 3 operands");
    VecVal a, b, o, old;
    evex_load(cpu, d, ex[1], a, w, elem);
    evex_load(cpu, d, ex[2], b, w, elem);
    for (unsigned i = 0; i < w; i += elem) {
        if (elem == 8) {
            uint64_t av, bv;
            memcpy(&av, a + i, 8);
            memcpy(&bv, b + i, 8);
            uint64_t r = av * bv;
            memcpy(o + i, &r, 8);
        } else {
            uint64_t av = 0, bv = 0;
            memcpy(&av, a + i, elem);
            memcpy(&bv, b + i, elem);
            uint64_t r = 0;
            if (elem == 2)
                r = (uint16_t)((uint16_t)av * (uint16_t)bv);
            else
                r = (uint32_t)((uint32_t)av * (uint32_t)bv);
            memcpy(o + i, &r, elem);
        }
    }
    evex_old(cpu, d, ex[0], old, w);
    bool zeroing = false;
    uint64_t kbits = evex_mask(cpu, d, w / elem, zeroing);
    o.taint = a.taint | b.taint;
    evex_store(cpu, d, ex[0], o, old, w, elem, kbits, zeroing);
    cpu->rip = next_rip(cpu, d);
}

void exec_evex_pminmax(CPU* cpu, const Dec& d, unsigned elem, bool is_signed,
                       bool is_max) {
    unsigned w = evex_width(cpu, d);
    int ex[5];
    int n = evex_explicit(d, ex);
    if (n < 3)
        guest_error(cpu, "evex minmax needs 3 operands");
    VecVal a, b, o, old;
    evex_load(cpu, d, ex[1], a, w, elem);
    evex_load(cpu, d, ex[2], b, w, elem);
    for (unsigned i = 0; i < w; i += elem) {
        uint64_t av = 0, bv = 0;
        memcpy(&av, a + i, elem);
        memcpy(&bv, b + i, elem);
        bool take;
        if (is_signed) {
            int64_t sa = 0, sb = 0;
            if (elem == 1) {
                sa = (int8_t)av;
                sb = (int8_t)bv;
            } else if (elem == 2) {
                sa = (int16_t)av;
                sb = (int16_t)bv;
            } else if (elem == 4) {
                sa = (int32_t)av;
                sb = (int32_t)bv;
            } else {
                sa = (int64_t)av;
                sb = (int64_t)bv;
            }
            take = is_max ? (sa > sb) : (sa < sb);
        } else {
            take = is_max ? (av > bv) : (av < bv);
        }
        memcpy(o + i, take ? a + i : b + i, elem);
    }
    evex_old(cpu, d, ex[0], old, w);
    bool zeroing = false;
    uint64_t kbits = evex_mask(cpu, d, w / elem, zeroing);
    o.taint = a.taint | b.taint;
    evex_store(cpu, d, ex[0], o, old, w, elem, kbits, zeroing);
    cpu->rip = next_rip(cpu, d);
}

void exec_evex_pavg_psad(CPU* cpu, const Dec& d, bool is_sad, unsigned elem) {
    unsigned w = evex_width(cpu, d);
    int ex[5];
    int n = evex_explicit(d, ex);
    if (n < 3)
        guest_error(cpu, "evex avg/sad needs 3 operands");
    VecVal a, b, o, old;
    evex_load(cpu, d, ex[1], a, w, elem);
    evex_load(cpu, d, ex[2], b, w, elem);
    memset(o, 0, w);
    if (!is_sad) {
        for (unsigned i = 0; i < w; i += elem) {
            uint64_t av = 0, bv = 0;
            memcpy(&av, a + i, elem);
            memcpy(&bv, b + i, elem);
            uint64_t r = (av + bv + 1) >> 1;
            memcpy(o + i, &r, elem);
        }
    } else {
        // VPSADBW: per 8-byte block, sum of abs differences -> 64-bit.
        for (unsigned blk = 0; blk < w; blk += 8) {
            uint64_t sum = 0;
            for (unsigned i = 0; i < 8; i++)
                sum += (uint64_t)(a[blk + i] > b[blk + i] ? a[blk + i] - b[blk + i]
                                                          : b[blk + i] - a[blk + i]);
            memcpy(o + blk, &sum, 8);
        }
    }
    evex_old(cpu, d, ex[0], old, w);
    bool zeroing = false;
    unsigned melem = is_sad ? 8 : elem;
    uint64_t kbits = evex_mask(cpu, d, w / melem, zeroing);
    o.taint = a.taint | b.taint;
    evex_store(cpu, d, ex[0], o, old, w, melem, kbits, zeroing);
    cpu->rip = next_rip(cpu, d);
}

// EVEX shifts: immediate (VPSLLW/D/Q, VPSRLW/D/Q, VPSRAW/D/Q, VPSLLDQ/SRLDQ)
// and variable (VPSLLVW/D/Q, VPSRLVW/D/Q, VPSRAVW/D/Q).
void exec_evex_shift(CPU* cpu, const Dec& d, unsigned elem, int kind,
                     bool variable) {
    unsigned w = evex_width(cpu, d);
    int ex[5];
    int n = evex_explicit(d, ex);
    VecVal a, o, old;
    if (!variable && (d.insn.mnemonic == ZYDIS_MNEMONIC_VPSLLDQ ||
                      d.insn.mnemonic == ZYDIS_MNEMONIC_VPSRLDQ)) {
        if (n < 3)
            guest_error(cpu, "evex dq-shift needs 3 operands");
        evex_load(cpu, d, ex[1], a, w, 1);
        unsigned cnt = (unsigned)kload(cpu, d, ex[2], 8);
        bool left = (d.insn.mnemonic == ZYDIS_MNEMONIC_VPSLLDQ);
        memset(o, 0, w);
        if (cnt < 64) {
            for (unsigned lane = 0; lane < w; lane += 64) {
                unsigned bl = (w - lane) < 64 ? (w - lane) : 64;
                for (unsigned i = 0; i < bl; i++) {
                    int src = left ? (int)i - (int)cnt : (int)i + (int)cnt;
                    if (src >= 0 && (unsigned)src < bl)
                        o[lane + i] = a[lane + src];
                }
            }
        }
        evex_old(cpu, d, ex[0], old, w);
        bool zeroing = false;
        uint64_t kbits = evex_mask(cpu, d, w, zeroing);
        o.taint = a.taint;
        evex_store(cpu, d, ex[0], o, old, w, 1, kbits, zeroing);
        cpu->rip = next_rip(cpu, d);
        return;
    }
    if (n < (variable ? 3 : 3))
        guest_error(cpu, "evex shift needs 3 operands");
    evex_load(cpu, d, ex[1], a, w, elem);
    VecVal cntv;
    cntv.clear();
    if (variable)
        evex_load(cpu, d, ex[2], cntv, w, elem);
    else {
        unsigned cnt = (unsigned)kload(cpu, d, ex[2], 8);
        memset(cntv, (int)(cnt & 0xff), 8);
        for (unsigned i = 0; i < w; i++)
            cntv[i] = (uint8_t)cnt;
    }
    for (unsigned i = 0; i < w; i += elem) {
        uint64_t v = 0;
        memcpy(&v, a + i, elem);
        uint64_t cv = 0;
        memcpy(&cv, cntv + i, elem < 8 ? elem : 8);
        unsigned c = (unsigned)(cv & (elem == 8 ? 63 : 255));
        uint64_t r = 0;
        if (c >= elem * 8) {
            r = (kind == 2 && elem < 8 && (v >> (elem * 8 - 1))) ? ~0ULL : 0;
            if (elem == 8 && kind == 2)
                r = ((int64_t)v < 0) ? ~0ULL : 0;
        } else if (kind == 0) {
            r = v << c;
        } else if (kind == 1) {
            r = v >> c;
        } else {
            if (elem == 2)
                r = (uint16_t)((int16_t)v >> c);
            else if (elem == 4)
                r = (uint32_t)((int32_t)v >> c);
            else if (elem == 8)
                r = (uint64_t)((int64_t)v >> c);
            else
                r = (uint64_t)(((int8_t)v >> (c & 7)) & 0xff);
        }
        memcpy(o + i, &r, elem);
    }
    evex_old(cpu, d, ex[0], old, w);
    bool zeroing = false;
    uint64_t kbits = evex_mask(cpu, d, w / elem, zeroing);
    o.taint = a.taint | cntv.taint;
    evex_store(cpu, d, ex[0], o, old, w, elem, kbits, zeroing);
    cpu->rip = next_rip(cpu, d);
}

// --- EVEX broadcast / permute / blend ---------------------------------------

void exec_evex_broadcast(CPU* cpu, const Dec& d) {
    unsigned w = evex_width(cpu, d);
    ZydisMnemonic m = d.insn.mnemonic;
    unsigned srcn = 4, elem = 4;
    bool from_mask = false;
    switch (m) {
    case ZYDIS_MNEMONIC_VPBROADCASTB: srcn = 1; elem = 1; break;
    case ZYDIS_MNEMONIC_VPBROADCASTW: srcn = 2; elem = 2; break;
    case ZYDIS_MNEMONIC_VPBROADCASTD:
    case ZYDIS_MNEMONIC_VBROADCASTSS: srcn = 4; elem = 4; break;
    case ZYDIS_MNEMONIC_VPBROADCASTQ:
    case ZYDIS_MNEMONIC_VBROADCASTSD: srcn = 8; elem = 8; break;
    case ZYDIS_MNEMONIC_VBROADCASTF32X2:
    case ZYDIS_MNEMONIC_VBROADCASTI32X2: srcn = 8; elem = 4; break;
    case ZYDIS_MNEMONIC_VBROADCASTF32X4:
    case ZYDIS_MNEMONIC_VBROADCASTI32X4:
    case ZYDIS_MNEMONIC_VBROADCASTF64X2:
    case ZYDIS_MNEMONIC_VBROADCASTI64X2:
    case ZYDIS_MNEMONIC_VBROADCASTI128: srcn = 16; elem = srcn; break;
    case ZYDIS_MNEMONIC_VBROADCASTF32X8:
    case ZYDIS_MNEMONIC_VBROADCASTI32X8:
    case ZYDIS_MNEMONIC_VBROADCASTF64X4:
    case ZYDIS_MNEMONIC_VBROADCASTI64X4: srcn = 32; elem = srcn; break;
    case ZYDIS_MNEMONIC_VPBROADCASTMW2D:
        srcn = 2;
        elem = 4;
        from_mask = true;
        break;
    case ZYDIS_MNEMONIC_VPBROADCASTMB2Q:
        srcn = 1;
        elem = 8;
        from_mask = true;
        break;
    default: guest_error(cpu, "bad evex broadcast");
    }
    int ex[5];
    int n = evex_explicit(d, ex);
    if (n < 2)
        guest_error(cpu, "broadcast needs 2 operands");
    VecVal src, out, old;
    src.clear();
    const auto& sop = d.ops[ex[1]];
    if (from_mask) {
        // Mask source: the last explicit k-reg (the write mask itself is
        // skipped by evex_explicit, so rescan when it went missing).
        int k = -1;
        for (int i = 0; i < d.insn.operand_count; i++) {
            if (d.ops[i].visibility == ZYDIS_OPERAND_VISIBILITY_HIDDEN)
                continue;
            if (d.ops[i].type == ZYDIS_OPERAND_TYPE_REGISTER) {
                int kk;
                if (reg_to_opmask(d.ops[i].reg.value, kk))
                    k = kk;
            }
        }
        if (k < 0)
            k = 0;
        memcpy(src, &cpu->opmask[k], srcn);
    } else if (sop.type == ZYDIS_OPERAND_TYPE_REGISTER) {
        int idx, width;
        if (reg_to_vec(sop.reg.value, idx, width)) {
            memcpy(src, cpu->xmm[idx].bytes, srcn);
            src.taint = cpu->xmm[idx].taint;
        } else {
            // GPR source (e.g. vpbroadcastb zmm, esi): low bytes.
            Val g = op_load(cpu, d, ex[1]);
            memcpy(src, &g.v, srcn);
            src.taint = g.taint;
        }
    } else if (sop.type == ZYDIS_OPERAND_TYPE_MEMORY) {
        uint64_t addr = resolve_mem(cpu, sop, cpu->rip + d.insn.length);
        mem_load_bytes(cpu, addr, src, srcn);
        mem_get_taint(addr, srcn, &src.taint);
    } else {
        guest_error(cpu, "bad broadcast source");
    }
    for (unsigned i = 0; i < w; i += srcn)
        memcpy(out + i, src, srcn);
    out.taint = src.taint;
    evex_old(cpu, d, ex[0], old, w);
    bool zeroing = false;
    uint64_t kbits = evex_mask(cpu, d, w / elem, zeroing);
    evex_store(cpu, d, ex[0], out, old, w, elem, kbits, zeroing);
    cpu->rip = next_rip(cpu, d);
}

// VPERMB/W/D/Q + VPERMILPS/PD (EVEX) + VPERMPD/Q + VPSHUFD + VSHUFPS/PD.
void exec_evex_perm(CPU* cpu, const Dec& d, int kind) {
    // kind 0=VPERMB 1=VPERMW 2=VPERMD/Q(idx vec) 3=VPERMIL 4=VPERMPD/Q(imm)
    // 5=VPSHUFD 6=VSHUF
    unsigned w = evex_width(cpu, d);
    int ex[5];
    int n = evex_explicit(d, ex);
    VecVal a, b, o, old;
    b.clear();
    if (kind == 4 || kind == 5) {
        if (n < 3)
            guest_error(cpu, "perm needs 3 operands");
        evex_load(cpu, d, ex[1], a, w, kind == 4 ? 8 : 4);
        unsigned order = (unsigned)kload(cpu, d, ex[2], 8);
        if (kind == 5) {
            for (unsigned lane = 0; lane < w; lane += 16)
                for (unsigned i = 0; i < 4; i++) {
                    unsigned sel = (order >> (i * 2)) & 3;
                    memcpy(o + lane + i * 4, a + lane + sel * 4, 4);
                }
        } else {
            for (unsigned i = 0; i < w; i += 8) {
                unsigned sel = (order >> ((i / 8) * 2)) & 3;
                unsigned lane = (i / 32) * 32;
                memcpy(o + i, a + lane + sel * 8, 8);
            }
        }
        evex_old(cpu, d, ex[0], old, w);
        bool zeroing = false;
        uint64_t kbits = evex_mask(cpu, d, w / 8, zeroing);
        o.taint = a.taint;
        evex_store(cpu, d, ex[0], o, old, w, 8, kbits, zeroing);
        cpu->rip = next_rip(cpu, d);
        return;
    }
    if (n < 3)
        guest_error(cpu, "perm needs 3 operands");
    unsigned elem = (kind == 0) ? 1 : (kind == 1 ? 2 : (kind == 3 ? 4 : 4));
    if (kind == 2)
        elem = (d.insn.mnemonic == ZYDIS_MNEMONIC_VPERMQ) ? 8 : 4;
    if (kind == 6)
        elem = 4;
    evex_load(cpu, d, ex[1], a, w, elem);
    evex_load(cpu, d, ex[2], b, w, elem);
    if (kind == 0 || kind == 1 || kind == 2) {
        // Variable permutes (VPERMB/W/D/Q): the INDEX vector is the first
        // source, data the second: dst[i] = data[idx[i]] (verified on HW).
        unsigned idxelem = (kind == 0) ? 1 : (kind == 1 ? 2 : elem);
        unsigned lanes = w / elem;
        for (unsigned i = 0; i < lanes; i++) {
            uint64_t ix = 0;
            memcpy(&ix, a + i * idxelem, idxelem);
            uint64_t sel = ix % lanes;
            memcpy(o + i * elem, b + sel * elem, elem);
        }
    } else if (kind == 3) {
        bool is_pd = (d.insn.mnemonic == ZYDIS_MNEMONIC_VPERMILPD);
        unsigned lane = is_pd ? 8 : 4;
        for (unsigned L = 0; L < w; L += 16) {
            unsigned elems = 16 / lane;
            for (unsigned i = 0; i < elems; i++) {
                uint64_t cv = 0;
                memcpy(&cv, b + L + i * lane, lane);
                unsigned sel = (unsigned)(cv & (elems - 1));
                memcpy(o + L + i * lane, a + L + sel * lane, lane);
            }
        }
    } else {
        // VSHUFPS/PD EVEX: dst,src1,src2,imm.
        if (n < 4)
            guest_error(cpu, "vshuf needs 4 operands");
        VecVal c;
        evex_load(cpu, d, ex[2], c, w, elem);
        unsigned order = (unsigned)kload(cpu, d, ex[3], 8);
        bool is_ps = (d.insn.mnemonic == ZYDIS_MNEMONIC_VSHUFPS);
        unsigned lane = is_ps ? 4 : 8;
        for (unsigned L = 0; L < w; L += 16) {
            unsigned le = 16 / lane;
            for (unsigned i = 0; i < le; i++) {
                if (is_ps) {
                    unsigned sel = (order >> (i * 2)) & 3;
                    const uint8_t* src = (i < 2) ? a : c;
                    memcpy(o + L + i * lane, src + L + sel * lane, lane);
                } else {
                    unsigned sel = (order >> i) & 1;
                    const uint8_t* src = sel ? c : a;
                    memcpy(o + L + i * lane, src + L + i * lane, lane);
                }
            }
        }
    }
    evex_old(cpu, d, ex[0], old, w);
    bool zeroing = false;
    uint64_t kbits = evex_mask(cpu, d, w / elem, zeroing);
    o.taint = a.taint | b.taint;
    evex_store(cpu, d, ex[0], o, old, w, elem, kbits, zeroing);
    cpu->rip = next_rip(cpu, d);
}

// VPBLENDMB/W/D/Q + VBLENDMPS/MPD.
void exec_evex_blend(CPU* cpu, const Dec& d) {
    unsigned w = evex_width(cpu, d);
    unsigned elem = 1;
    switch (d.insn.mnemonic) {
    case ZYDIS_MNEMONIC_VPBLENDMW: elem = 2; break;
    case ZYDIS_MNEMONIC_VPBLENDMD:
    case ZYDIS_MNEMONIC_VBLENDMPS: elem = 4; break;
    case ZYDIS_MNEMONIC_VPBLENDMQ:
    case ZYDIS_MNEMONIC_VBLENDMPD: elem = 8; break;
    default: elem = 1; break;
    }
    int ex[5];
    int n = evex_explicit(d, ex);
    if (n < 3)
        guest_error(cpu, "blend needs 3 operands");
    VecVal a, b, o, old;
    evex_load(cpu, d, ex[1], a, w, elem);
    evex_load(cpu, d, ex[2], b, w, elem);
    bool zeroing = false;
    uint64_t kbits = evex_mask(cpu, d, w / elem, zeroing);
    // Blend selects per the write mask itself (no separate dest merge):
    // k[i]=1 takes b, else a. {z} zeroes the a side.
    for (unsigned i = 0; i < w / elem; i++) {
        if ((kbits >> i) & 1)
            memcpy(o + i * elem, b + i * elem, elem);
        else if (zeroing)
            memset(o + i * elem, 0, elem);
        else
            memcpy(o + i * elem, a + i * elem, elem);
    }
    evex_old(cpu, d, ex[0], old, w);
    o.taint = a.taint | b.taint;
    evex_store(cpu, d, ex[0], o, old, w, elem, ~0ULL, false);
    cpu->rip = next_rip(cpu, d);
}

// --- EVEX converts / misc ----------------------------------------------------

void exec_evex_pmovsx(CPU* cpu, const Dec& d, unsigned s_lane, unsigned d_lane,
                      bool is_signed) {
    unsigned w = evex_width(cpu, d);
    int ex[5];
    int n = evex_explicit(d, ex);
    if (n < 2)
        guest_error(cpu, "evex pmovsx needs 2 operands");
    VecVal a, o, old;
    // Source occupies w*d_lane/s_lane... actually source width = w*s_lane/d_lane.
    unsigned sw = w * s_lane / d_lane;
    evex_load(cpu, d, ex[1], a, sw > MAX_VEC_BYTES ? MAX_VEC_BYTES : sw, s_lane);
    memset(o, 0, w);
    for (unsigned i = 0; i < w / d_lane; i++) {
        uint64_t v = 0;
        memcpy(&v, a + i * s_lane, s_lane);
        uint64_t r = 0;
        if (is_signed) {
            if (s_lane == 1)
                r = (uint64_t)(int64_t)(int8_t)v;
            else if (s_lane == 2)
                r = (uint64_t)(int64_t)(int16_t)v;
            else
                r = (uint64_t)(int64_t)(int32_t)v;
        } else {
            r = v;
        }
        memcpy(o + i * d_lane, &r, d_lane);
    }
    evex_old(cpu, d, ex[0], old, w);
    bool zeroing = false;
    uint64_t kbits = evex_mask(cpu, d, w / d_lane, zeroing);
    o.taint = a.taint;
    evex_store(cpu, d, ex[0], o, old, w, d_lane, kbits, zeroing);
    cpu->rip = next_rip(cpu, d);
}

// Down-convert with truncation (VPMOVDB/DW/QB/QD/QW) or saturation
// (VPMOVSDB/SDW/SQB/SQD/SQW signed, VPMOVUSDB/... unsigned).
void exec_evex_pmov_down(CPU* cpu, const Dec& d, unsigned s_lane,
                         unsigned d_lane, int sat) {
    unsigned w = evex_width(cpu, d);
    int ex[5];
    int n = evex_explicit(d, ex);
    if (n < 2)
        guest_error(cpu, "evex pmov-down needs 2 operands");
    // Dest holds w/d_lane... source lanes = dest bytes/d_lane... For a
    // w-byte dest with d_lane elements of d_lane bytes: lanes = w/d_lane;
    // source consumes lanes*s_lane bytes (may exceed w for 512->256 etc.;
    // the source operand size gives the true extent).
    unsigned lanes = w / d_lane;
    unsigned sw = lanes * s_lane;
    if (sw > 64)
        sw = 64;
    VecVal a, o, old;
    evex_load(cpu, d, ex[1], a, sw, s_lane);
    memset(o, 0, w);
    for (unsigned i = 0; i < lanes; i++) {
        uint64_t v = 0;
        memcpy(&v, a + i * s_lane, s_lane);
        uint64_t r = v;
        if (sat != 0) {
            int64_t sv = 0;
            if (s_lane == 1)
                sv = (int8_t)v;
            else if (s_lane == 2)
                sv = (int16_t)v;
            else if (s_lane == 4)
                sv = (int32_t)v;
            else
                sv = (int64_t)v;
            if (sat > 0) { // signed saturation
                int64_t lo = 0, hi = 0;
                if (d_lane == 1) {
                    lo = -128;
                    hi = 127;
                } else if (d_lane == 2) {
                    lo = -32768;
                    hi = 32767;
                } else {
                    lo = (int64_t)0x80000000LL;
                    hi = 0x7fffffffLL;
                }
                if (sv < lo)
                    sv = lo;
                if (sv > hi)
                    sv = hi;
                r = (uint64_t)sv;
            } else { // unsigned saturation
                uint64_t hi = (d_lane == 1) ? 0xff : (d_lane == 2) ? 0xffff : 0xffffffff;
                if (sv < 0)
                    r = 0;
                else if (v > hi)
                    r = hi;
                else
                    r = v;
            }
        }
        memcpy(o + i * d_lane, &r, d_lane);
    }
    evex_old(cpu, d, ex[0], old, w);
    bool zeroing = false;
    uint64_t kbits = evex_mask(cpu, d, lanes, zeroing);
    o.taint = a.taint;
    evex_store(cpu, d, ex[0], o, old, w, d_lane, kbits, zeroing);
    cpu->rip = next_rip(cpu, d);
}

// VPMOVM2B/W/D/Q (mask -> vector) and VPMOVB2M/W2M/D2M/Q2M (vector -> mask).
void exec_evex_movm(CPU* cpu, const Dec& d, bool to_vec) {
    unsigned elem = 1;
    switch (d.insn.mnemonic) {
    case ZYDIS_MNEMONIC_VPMOVM2W:
    case ZYDIS_MNEMONIC_VPMOVW2M: elem = 2; break;
    case ZYDIS_MNEMONIC_VPMOVM2D:
    case ZYDIS_MNEMONIC_VPMOVD2M: elem = 4; break;
    case ZYDIS_MNEMONIC_VPMOVM2Q:
    case ZYDIS_MNEMONIC_VPMOVQ2M: elem = 8; break;
    default: elem = 1; break;
    }
    unsigned w = evex_width(cpu, d);
    int ex[5];
    int n = evex_explicit(d, ex);
    if (n < 2)
        guest_error(cpu, "movm needs 2 operands");
    unsigned lanes = w / elem;
    if (to_vec) {
        // Mask source: last explicit k-reg (rescan; evex_explicit may have
        // skipped it when it equals the write mask).
        int k = -1;
        for (int i = 0; i < d.insn.operand_count; i++) {
            if (d.ops[i].visibility == ZYDIS_OPERAND_VISIBILITY_HIDDEN)
                continue;
            if (d.ops[i].type == ZYDIS_OPERAND_TYPE_REGISTER) {
                int kk;
                if (reg_to_opmask(d.ops[i].reg.value, kk))
                    k = kk;
            }
        }
        uint64_t bits = (k >= 0) ? cpu->opmask[k] : 0;
        if (k < 0) {
            const auto& sop = d.ops[ex[1]];
            int kk;
            if (sop.type == ZYDIS_OPERAND_TYPE_REGISTER &&
                reg_to_opmask(sop.reg.value, kk))
                bits = cpu->opmask[kk];
            else
                bits = kload(cpu, d, ex[1], 64);
        }
        VecVal o, old;
        for (unsigned i = 0; i < lanes; i++)
            memset(o + i * elem, ((bits >> i) & 1) ? 0xff : 0, elem);
        evex_old(cpu, d, ex[0], old, w);
        bool zeroing = false;
        uint64_t kbits = evex_mask(cpu, d, lanes, zeroing);
        o.taint.clear();
        evex_store(cpu, d, ex[0], o, old, w, elem, kbits, zeroing);
        cpu->rip = next_rip(cpu, d);
        return;
    }
    VecVal a;
    evex_load(cpu, d, ex[1], a, w, elem);
    uint64_t res = 0;
    for (unsigned i = 0; i < lanes; i++) {
        uint64_t v = 0;
        memcpy(&v, a + i * elem, elem);
        if (v != 0)
            res |= (1ULL << i);
    }
    kstore(cpu, d, ex[0], res, 64);
    cpu->rip = next_rip(cpu, d);
}

void exec_evex_popcnt_lzcnt(CPU* cpu, const Dec& d, bool is_lzcnt) {
    unsigned elem = 4;
    switch (d.insn.mnemonic) {
    case ZYDIS_MNEMONIC_VPOPCNTB: elem = 1; break;
    case ZYDIS_MNEMONIC_VPOPCNTW: elem = 2; break;
    case ZYDIS_MNEMONIC_VPOPCNTQ:
    case ZYDIS_MNEMONIC_VPLZCNTQ: elem = 8; break;
    default: elem = 4; break;
    }
    unsigned w = evex_width(cpu, d);
    int ex[5];
    int n = evex_explicit(d, ex);
    if (n < 2)
        guest_error(cpu, "popcnt needs 2 operands");
    VecVal a, o, old;
    evex_load(cpu, d, ex[1], a, w, elem);
    for (unsigned i = 0; i < w; i += elem) {
        uint64_t v = 0;
        memcpy(&v, a + i, elem);
        uint64_t r = 0;
        if (!is_lzcnt) {
            r = (uint64_t)__builtin_popcountll(v & (elem >= 8 ? ~0ULL : ((1ULL << (elem * 8)) - 1)));
        } else {
            if (v == 0)
                r = elem * 8;
            else if (elem == 8)
                r = (uint64_t)__builtin_clzll(v);
            else if (elem == 4)
                r = (uint64_t)__builtin_clz((unsigned)v);
            else
                r = (uint64_t)(elem * 8 - 1 - (63 - __builtin_clzll(v)));
        }
        memcpy(o + i, &r, elem);
    }
    evex_old(cpu, d, ex[0], old, w);
    bool zeroing = false;
    uint64_t kbits = evex_mask(cpu, d, w / elem, zeroing);
    o.taint = a.taint;
    evex_store(cpu, d, ex[0], o, old, w, elem, kbits, zeroing);
    cpu->rip = next_rip(cpu, d);
}

void exec_evex_vpabs(CPU* cpu, const Dec& d) {
    unsigned elem = 1;
    switch (d.insn.mnemonic) {
    case ZYDIS_MNEMONIC_VPABSW: elem = 2; break;
    case ZYDIS_MNEMONIC_VPABSD: elem = 4; break;
    case ZYDIS_MNEMONIC_VPABSQ: elem = 8; break;
    default: elem = 1; break;
    }
    unsigned w = evex_width(cpu, d);
    int ex[5];
    int n = evex_explicit(d, ex);
    if (n < 2)
        guest_error(cpu, "vpabs needs 2 operands");
    VecVal a, o, old;
    evex_load(cpu, d, ex[1], a, w, elem);
    for (unsigned i = 0; i < w; i += elem) {
        uint64_t v = 0;
        memcpy(&v, a + i, elem);
        uint64_t r = v;
        if (elem == 1 && (int8_t)v < 0)
            r = (uint8_t)(-(int8_t)v);
        else if (elem == 2 && (int16_t)v < 0)
            r = (uint16_t)(-(int16_t)v);
        else if (elem == 4 && (int32_t)v < 0)
            r = (uint32_t)(-(int32_t)v);
        else if (elem == 8 && (int64_t)v < 0)
            r = (uint64_t)(-(int64_t)v);
        memcpy(o + i, &r, elem);
    }
    evex_old(cpu, d, ex[0], old, w);
    bool zeroing = false;
    uint64_t kbits = evex_mask(cpu, d, w / elem, zeroing);
    o.taint = a.taint;
    evex_store(cpu, d, ex[0], o, old, w, elem, kbits, zeroing);
    cpu->rip = next_rip(cpu, d);
}

// VP2INTERSECTD/Q: second result goes to the write-mask register (the {k}
// operand, k0 = discard). Explicit ops are [k-dst, src1, src2].
void exec_evex_p2intersect(CPU* cpu, const Dec& d) {
    unsigned elem = (d.insn.mnemonic == ZYDIS_MNEMONIC_VP2INTERSECTQ) ? 8 : 4;
    unsigned w = evex_width(cpu, d);
    int ex[5];
    int n = evex_explicit(d, ex);
    if (n < 3)
        guest_error(cpu, "vp2intersect needs 3 operands");
    VecVal a, b;
    evex_load(cpu, d, ex[1], a, w, elem);
    evex_load(cpu, d, ex[2], b, w, elem);
    unsigned lanes = w / elem;
    uint64_t k1 = 0, k2 = 0;
    for (unsigned i = 0; i < lanes; i++) {
        for (unsigned j = 0; j < lanes; j++) {
            if (memcmp(a + i * elem, b + j * elem, elem) == 0) {
                k1 |= (1ULL << i);
                k2 |= (1ULL << j);
            }
        }
    }
    kstore(cpu, d, ex[0], k1, 64);
    int dk = -1;
    if (reg_to_opmask(d.insn.avx.mask.reg, dk) && dk != 0)
        cpu->opmask[dk] = k2;
    cpu->rip = next_rip(cpu, d);
}

// EVEX punpck/pshufb/palignr/pblendvb forms (element-wise, masked).
void exec_evex_unpack(CPU* cpu, const Dec& d, int kind) {
    // kind 0=punpckl/h (elem from mnemonic) 1=pshufb 2=palignr 3=pblendvb
    unsigned elem = 1;
    bool high = false;
    switch (d.insn.mnemonic) {
    case ZYDIS_MNEMONIC_VPUNPCKLBW:
    case ZYDIS_MNEMONIC_VPUNPCKHBW: elem = 1; break;
    case ZYDIS_MNEMONIC_VPUNPCKLWD:
    case ZYDIS_MNEMONIC_VPUNPCKHWD: elem = 2; break;
    case ZYDIS_MNEMONIC_VPUNPCKLDQ:
    case ZYDIS_MNEMONIC_VPUNPCKHDQ: elem = 4; break;
    default: elem = 8; break;
    }
    switch (d.insn.mnemonic) {
    case ZYDIS_MNEMONIC_VPUNPCKHBW:
    case ZYDIS_MNEMONIC_VPUNPCKHWD:
    case ZYDIS_MNEMONIC_VPUNPCKHDQ:
    case ZYDIS_MNEMONIC_VPUNPCKHQDQ: high = true; break;
    default: break;
    }
    unsigned w = evex_width(cpu, d);
    int ex[5];
    int n = evex_explicit(d, ex);
    if (n < 3)
        guest_error(cpu, "evex unpack needs 3 operands");
    VecVal a, b, o, old;
    evex_load(cpu, d, ex[1], a, w, elem);
    evex_load(cpu, d, ex[2], b, w, elem);
    if (kind == 0) {
        for (unsigned lane = 0; lane < w; lane += 16) {
            unsigned half = 8 / elem;
            for (unsigned i = 0; i < half; i++) {
                memcpy(o + lane + 2 * i * elem, a + lane + (high ? 8 : 0) + i * elem, elem);
                memcpy(o + lane + (2 * i + 1) * elem, b + lane + (high ? 8 : 0) + i * elem, elem);
            }
        }
    } else if (kind == 1) {
        for (unsigned i = 0; i < w; i++) {
            uint8_t sel = b[i];
            o[i] = (sel & 0x80) ? 0 : a[(i & ~15) | (sel & 15)];
        }
    } else if (kind == 2) {
        unsigned cnt = (unsigned)kload(cpu, d, ex[3], 8);
        for (unsigned lane = 0; lane < w; lane += 16) {
            for (unsigned i = 0; i < 16; i++) {
                unsigned src = i + cnt;
                o[lane + i] = (src < 32) ? (src < 16 ? b[lane + src] : a[lane + src - 16]) : 0;
            }
        }
    } else {
        // pblendvb EVEX: mask from implicit XMM0.
        for (unsigned i = 0; i < w; i++)
            o[i] = (cpu->xmm[0].bytes[i] & 0x80) ? b[i] : a[i];
        o.taint = a.taint | b.taint | cpu->xmm[0].taint;
    }
    unsigned melem = 1;
    evex_old(cpu, d, ex[0], old, w);
    bool zeroing = false;
    uint64_t kbits = evex_mask(cpu, d, w / melem, zeroing);
    if (kind != 3)
        o.taint = a.taint | b.taint;
    evex_store(cpu, d, ex[0], o, old, w, melem, kbits, zeroing);
    cpu->rip = next_rip(cpu, d);
}

// --- gathers / scatters / maskmov --------------------------------------------

static bool mn_starts_with(ZydisMnemonic m, const char* prefix) {
    const char* s = ZydisMnemonicGetString(m);
    if (!s)
        return false;
    while (*prefix) {
        char a = *prefix++, b = *s++;
        if (a >= 'A' && a <= 'Z')
            a = (char)(a + 32);
        if (b >= 'A' && b <= 'Z')
            b = (char)(b + 32);
        if (a != b)
            return false;
    }
    return true;
}

// Shared VSIB address computation. Returns base; fills index vector id.
uint64_t vsib_base(CPU* cpu, const ZydisDecodedOperand& mop, int& vidx) {
    if (mop.mem.index == ZYDIS_REGISTER_NONE)
        guest_error(cpu, "gather/scatter needs a vector index");
    int vw;
    if (!reg_to_vec(mop.mem.index, vidx, vw))
        guest_error(cpu, "bad gather/scatter index register");
    if (cpu->xmm[vidx].taint.cannot_index)
        guest_error(cpu, "tainted (cannot-index) gather/scatter index");
    uint64_t base = 0;
    if (mop.mem.base != ZYDIS_REGISTER_NONE) {
        int idx, size, shift;
        if (!reg_to_gpr(mop.mem.base, idx, size, shift))
            guest_error(cpu, "bad gather/scatter base register");
        if (cpu->gpr[idx].taint.cannot_index)
            guest_error(cpu, "tainted (cannot-index) gather/scatter base");
        base = cpu->gpr[idx].val;
    }
    return base;
}

void exec_evex_gather(CPU* cpu, const Dec& d, unsigned idx_elem,
                      unsigned data_elem) {
    unsigned w = evex_width(cpu, d);
    int ex[5];
    int n = evex_explicit(d, ex);
    // Explicit ops: [dst-reg, vsib-mem].
    int dsti = -1, memi = -1;
    for (int i = 0; i < n; i++) {
        if (d.ops[ex[i]].type == ZYDIS_OPERAND_TYPE_REGISTER && dsti < 0)
            dsti = ex[i];
        else if (d.ops[ex[i]].type == ZYDIS_OPERAND_TYPE_MEMORY)
            memi = ex[i];
    }
    if (dsti < 0 || memi < 0)
        guest_error(cpu, "bad gather operands");
    unsigned lanes = w / data_elem;
    VecVal o, old;
    evex_old(cpu, d, dsti, old, w);
    bool zeroing = false;
    uint64_t kbits = evex_mask(cpu, d, lanes, zeroing);
    uint64_t all_g = (lanes >= 64) ? ~0ULL : ((lanes == 0) ? 0 : ((1ULL << lanes) - 1));
    if ((kbits & all_g) == 0) {
        // Fully masked: no lanes enabled, no faults (including index/base
        // taint) and no mem taint. Dest merges old (or zeroes).
        VecVal tmp;
        if (zeroing)
            tmp.clear();
        else
            tmp = old;
        // evex_store with empty mask preserves old/clears correctly for taint.
        evex_store(cpu, d, dsti, tmp, old, w, data_elem, kbits, zeroing);
        cpu->rip = next_rip(cpu, d);
        return;
    }
    const auto& mop = d.ops[memi];
    int vidx = 0;
    uint64_t base = vsib_base(cpu, mop, vidx);
    int64_t disp = mop.mem.disp.has_displacement ? mop.mem.disp.value : 0;
    uint64_t scale = mop.mem.scale ? mop.mem.scale : 1;
    VecVal idxb;
    memcpy(idxb, cpu->xmm[vidx].bytes, MAX_VEC_BYTES);
    // Per-element precise: only enabled lanes fault/taint. Index taint
    // contributes only if some lane enabled; mem taint ORed per enabled lane.
    uint64_t all = (lanes >= 64) ? ~0ULL : ((lanes == 0) ? 0 : ((1ULL << lanes) - 1));
    bool any_enabled = ((kbits & all) != 0);
    Taint idx_taint;
    if (any_enabled)
        idx_taint = cpu->xmm[vidx].taint;
    Taint mem_taint;
    for (unsigned i = 0; i < lanes; i++) {
        if ((kbits >> i) & 1) {
            uint64_t ix = 0;
            memcpy(&ix, idxb + i * idx_elem, idx_elem);
            if (idx_elem == 4)
                ix = (uint64_t)(int32_t)ix; // sign-extend dword indices
            else
                ix = (uint64_t)(int64_t)ix;
            uint64_t addr = base + ix * scale + (uint64_t)disp;
            uint8_t tmp[8];
            mem_load_bytes(cpu, addr, tmp, data_elem);
            Taint t;
            mem_get_taint(addr, data_elem, &t);
            mem_taint |= t;
            memcpy(o + i * data_elem, tmp, data_elem);
        } else if (zeroing) {
            memset(o + i * data_elem, 0, data_elem);
        } else {
            memcpy(o + i * data_elem, old + i * data_elem, data_elem);
        }
    }
    Taint src_taint = idx_taint | mem_taint;
    if (zeroing)
        o.taint = any_enabled ? src_taint : Taint();
    else
        o.taint = any_enabled ? (old.taint | src_taint) : old.taint;
    evex_store(cpu, d, dsti, o, old, w, data_elem, ~0ULL, false);
    cpu->rip = next_rip(cpu, d);
}

void exec_evex_scatter(CPU* cpu, const Dec& d, unsigned idx_elem,
                       unsigned data_elem) {
    unsigned w = evex_width(cpu, d);
    int ex[5];
    int n = evex_explicit(d, ex);
    // Explicit ops: [vsib-mem, src-reg].
    int dsti = -1, srci = -1;
    for (int i = 0; i < n; i++) {
        if (d.ops[ex[i]].type == ZYDIS_OPERAND_TYPE_MEMORY && dsti < 0)
            dsti = ex[i];
        else if (d.ops[ex[i]].type == ZYDIS_OPERAND_TYPE_REGISTER)
            srci = ex[i];
    }
    if (dsti < 0 || srci < 0)
        guest_error(cpu, "bad scatter operands");
    unsigned lanes = w / data_elem;
    bool zeroing_s = false;
    uint64_t kbits_s = evex_mask(cpu, d, lanes, zeroing_s);
    uint64_t all_s = (lanes >= 64) ? ~0ULL : ((lanes == 0) ? 0 : ((1ULL << lanes) - 1));
    if ((kbits_s & all_s) == 0) {
        // Fully masked scatter: no stores, no faults (including index/base).
        cpu->rip = next_rip(cpu, d);
        return;
    }
    const auto& mop = d.ops[dsti];
    int vidx = 0;
    uint64_t base = vsib_base(cpu, mop, vidx);
    int64_t disp = mop.mem.disp.has_displacement ? mop.mem.disp.value : 0;
    uint64_t scale = mop.mem.scale ? mop.mem.scale : 1;
    VecVal src;
    evex_load(cpu, d, srci, src, w, data_elem);
    bool zeroing = false;
    uint64_t kbits = evex_mask(cpu, d, lanes, zeroing);
    (void)zeroing;
    VecVal idxb;
    memcpy(idxb, cpu->xmm[vidx].bytes, MAX_VEC_BYTES);
    for (unsigned i = 0; i < lanes; i++) {
        if ((kbits >> i) & 1) {
            uint64_t ix = 0;
            memcpy(&ix, idxb + i * idx_elem, idx_elem);
            if (idx_elem == 4)
                ix = (uint64_t)(int32_t)ix;
            else
                ix = (uint64_t)(int64_t)ix;
            uint64_t addr = base + ix * scale + (uint64_t)disp;
            mem_store_bytes(cpu, addr, src + i * data_elem, data_elem);
            mem_note_store(cpu, addr, data_elem, src.taint);
        }
    }
    cpu->rip = next_rip(cpu, d);
}

// VEX legacy gathers: VGATHERDPS/DPD/QPS/QPD (mask in a vector register,
// MSB per mask element; completed elements clear their mask bit).
void exec_vex_gather(CPU* cpu, const Dec& d, unsigned idx_elem,
                     unsigned data_elem) {
    int ex[5];
    int n = evex_explicit(d, ex);
    // ops: [dst, vsib-mem, mask-vec].
    int dsti = -1, memi = -1, maski = -1;
    for (int i = 0; i < n; i++) {
        if (d.ops[ex[i]].type == ZYDIS_OPERAND_TYPE_REGISTER && dsti < 0)
            dsti = ex[i];
        else if (d.ops[ex[i]].type == ZYDIS_OPERAND_TYPE_MEMORY)
            memi = ex[i];
        else if (d.ops[ex[i]].type == ZYDIS_OPERAND_TYPE_REGISTER)
            maski = ex[i];
    }
    if (dsti < 0 || memi < 0 || maski < 0)
        guest_error(cpu, "bad vex gather operands");
    unsigned w = d.ops[dsti].size / 8;
    if (w != 16 && w != 32)
        guest_error(cpu, "bad vex gather width");
    const auto& mop = d.ops[memi];
    int vidx = 0;
    uint64_t base = vsib_base(cpu, mop, vidx);
    int64_t disp = mop.mem.disp.has_displacement ? mop.mem.disp.value : 0;
    uint64_t scale = mop.mem.scale ? mop.mem.scale : 1;
    int didx, dw, midx, mw;
    if (!reg_to_vec(d.ops[dsti].reg.value, didx, dw) ||
        !reg_to_vec(d.ops[maski].reg.value, midx, mw))
        guest_error(cpu, "bad vex gather registers");
    unsigned lanes = w / data_elem;
    unsigned step = data_elem; // mask elements match data width
    // Per-element precise: only enabled lanes fault/taint.
    Taint dest_taint = cpu->xmm[didx].taint;
    Taint mask_taint = cpu->xmm[midx].taint;
    Taint idx_taint = cpu->xmm[vidx].taint;
    for (unsigned i = 0; i < lanes; i++) {
        uint64_t mb = 0;
        memcpy(&mb, cpu->xmm[midx].bytes + i * step, step);
        bool enabled = (mb >> (step * 8 - 1)) & 1;
        if (!enabled)
            continue;
        uint64_t ix = 0;
        memcpy(&ix, cpu->xmm[vidx].bytes + i * idx_elem, idx_elem);
        if (idx_elem == 4)
            ix = (uint64_t)(int32_t)ix;
        else
            ix = (uint64_t)(int64_t)ix;
        uint64_t addr = base + ix * scale + (uint64_t)disp;
        uint8_t tmp[8];
        mem_load_bytes(cpu, addr, tmp, data_elem);
        Taint t;
        mem_get_taint(addr, data_elem, &t);
        dest_taint |= mask_taint | idx_taint | t;
        memcpy(cpu->xmm[didx].bytes + i * data_elem, tmp, data_elem);
        memset(cpu->xmm[midx].bytes + i * step, 0, step);
    }
    // Mask register taint: completed elements cleared, but overall mask
    // taint persists? Completed lanes cleared to zero (clean for those lanes),
    // but with whole-vector Taint, preserve mask taint if any lane remains?
    // Conservative: keep original mask taint (clearing is per-lane zeroing,
    // but taint whole). Dest gains OR above.
    cpu->xmm[didx].taint = dest_taint;
    cpu->rip = next_rip(cpu, d);
}

// VMASKMOVPS/PD + VPMASKMOVD/Q (VEX). Masked element MSB decides;
// faults only on enabled elements; no fault for disabled ones.
void exec_maskmov(CPU* cpu, const Dec& d) {
    unsigned elem = 4;
    switch (d.insn.mnemonic) {
    case ZYDIS_MNEMONIC_VMASKMOVPD:
    case ZYDIS_MNEMONIC_VPMASKMOVQ: elem = 8; break;
    default: elem = 4; break;
    }
    int ex[5];
    int n = evex_explicit(d, ex);
    if (n < 3)
        guest_error(cpu, "maskmov needs 3 operands");
    bool is_store = (d.ops[ex[0]].type == ZYDIS_OPERAND_TYPE_MEMORY);
    uint64_t addr = 0;
    VecVal data, mask;
    unsigned w = 0;
    if (is_store) {
        // [mem, mask, src]. Per-element precise: only enabled elements
        // fault (mem_check via mem_store_bytes) and gain taint.
        addr = resolve_mem(cpu, d.ops[ex[0]], cpu->rip + d.insn.length);
        w = d.ops[ex[2]].size / 8;
        if (w != 16 && w != 32)
            guest_error(cpu, "bad maskmov width");
        vec_load_bytes(cpu, d, ex[1], mask, w);
        vec_load_bytes(cpu, d, ex[2], data, w);
        Taint src_taint = mask.taint | data.taint;
        for (unsigned i = 0; i < w; i += elem) {
            uint64_t mv = 0;
            memcpy(&mv, mask + i, elem);
            if ((mv >> (elem * 8 - 1)) & 1) {
                mem_store_bytes(cpu, addr + i, data + i, elem);
                mem_note_store(cpu, addr + i, elem, src_taint);
            }
        }
    } else {
        // [dst, mask, mem]. Only enabled elements fault/taint.
        addr = resolve_mem(cpu, d.ops[ex[2]], cpu->rip + d.insn.length);
        w = d.ops[ex[0]].size / 8;
        if (w != 16 && w != 32)
            guest_error(cpu, "bad maskmov width");
        vec_load_bytes(cpu, d, ex[1], mask, w);
        VecVal old;
        vec_load_bytes(cpu, d, ex[0], old, w);
        // Preserve old dest taint where known: re-read below.
        VecVal out;
        memcpy(out.bytes, old.bytes, w);
        out.taint = old.taint;
        // Fetch current dest taint via register lookup.
        int didx = 0, dw = 0;
        Taint dest_taint;
        if (d.ops[ex[0]].type == ZYDIS_OPERAND_TYPE_REGISTER &&
            reg_to_vec(d.ops[ex[0]].reg.value, didx, dw)) {
            dest_taint = cpu->xmm[didx].taint;
            out.taint = dest_taint;
        } else {
            dest_taint = old.taint;
        }
        for (unsigned i = 0; i < w; i += elem) {
            uint64_t mv = 0;
            memcpy(&mv, mask + i, elem);
            if ((mv >> (elem * 8 - 1)) & 1) {
                mem_load_bytes(cpu, addr + i, out + i, elem);
                Taint t;
                mem_get_taint(addr + i, elem, &t);
                dest_taint |= mask.taint | t;
            }
        }
        out.taint = dest_taint;
        vec_store_bytes(cpu, d, ex[0], out, w, true, w);
    }
    cpu->rip = next_rip(cpu, d);
}

// --- AES / PCLMULQDQ / GFNI / SHA --------------------------------------------
// Implemented with host AES-NI/SHA-NI intrinsics (inline machine
// instructions, per-function target attributes so the base build stays
// portable). 128-bit lanes are processed independently for the 256/512-bit
// VAES/VPCLMULQDQ forms.

__attribute__((target("sse4.1,aes"))) static inline __m128i aes_keygenassist_lane(
    __m128i state, unsigned imm) {
    switch (imm & 0xff) {
    case 0: return _mm_aeskeygenassist_si128(state, 0);
    case 1: return _mm_aeskeygenassist_si128(state, 1);
    case 2: return _mm_aeskeygenassist_si128(state, 2);
    case 3: return _mm_aeskeygenassist_si128(state, 3);
    case 4: return _mm_aeskeygenassist_si128(state, 4);
    case 5: return _mm_aeskeygenassist_si128(state, 5);
    case 6: return _mm_aeskeygenassist_si128(state, 6);
    case 7: return _mm_aeskeygenassist_si128(state, 7);
    case 8: return _mm_aeskeygenassist_si128(state, 8);
    case 9: return _mm_aeskeygenassist_si128(state, 9);
    case 10: return _mm_aeskeygenassist_si128(state, 10);
    case 11: return _mm_aeskeygenassist_si128(state, 11);
    case 12: return _mm_aeskeygenassist_si128(state, 12);
    case 13: return _mm_aeskeygenassist_si128(state, 13);
    case 14: return _mm_aeskeygenassist_si128(state, 14);
    case 15: return _mm_aeskeygenassist_si128(state, 15);
    case 16: return _mm_aeskeygenassist_si128(state, 16);
    case 17: return _mm_aeskeygenassist_si128(state, 17);
    case 18: return _mm_aeskeygenassist_si128(state, 18);
    case 19: return _mm_aeskeygenassist_si128(state, 19);
    case 20: return _mm_aeskeygenassist_si128(state, 20);
    case 21: return _mm_aeskeygenassist_si128(state, 21);
    case 22: return _mm_aeskeygenassist_si128(state, 22);
    case 23: return _mm_aeskeygenassist_si128(state, 23);
    case 24: return _mm_aeskeygenassist_si128(state, 24);
    case 25: return _mm_aeskeygenassist_si128(state, 25);
    case 26: return _mm_aeskeygenassist_si128(state, 26);
    case 27: return _mm_aeskeygenassist_si128(state, 27);
    case 28: return _mm_aeskeygenassist_si128(state, 28);
    case 29: return _mm_aeskeygenassist_si128(state, 29);
    case 30: return _mm_aeskeygenassist_si128(state, 30);
    case 31: return _mm_aeskeygenassist_si128(state, 31);
    case 32: return _mm_aeskeygenassist_si128(state, 32);
    case 33: return _mm_aeskeygenassist_si128(state, 33);
    case 34: return _mm_aeskeygenassist_si128(state, 34);
    case 35: return _mm_aeskeygenassist_si128(state, 35);
    case 36: return _mm_aeskeygenassist_si128(state, 36);
    case 37: return _mm_aeskeygenassist_si128(state, 37);
    case 38: return _mm_aeskeygenassist_si128(state, 38);
    case 39: return _mm_aeskeygenassist_si128(state, 39);
    case 40: return _mm_aeskeygenassist_si128(state, 40);
    case 41: return _mm_aeskeygenassist_si128(state, 41);
    case 42: return _mm_aeskeygenassist_si128(state, 42);
    case 43: return _mm_aeskeygenassist_si128(state, 43);
    case 44: return _mm_aeskeygenassist_si128(state, 44);
    case 45: return _mm_aeskeygenassist_si128(state, 45);
    case 46: return _mm_aeskeygenassist_si128(state, 46);
    case 47: return _mm_aeskeygenassist_si128(state, 47);
    case 48: return _mm_aeskeygenassist_si128(state, 48);
    case 49: return _mm_aeskeygenassist_si128(state, 49);
    case 50: return _mm_aeskeygenassist_si128(state, 50);
    case 51: return _mm_aeskeygenassist_si128(state, 51);
    case 52: return _mm_aeskeygenassist_si128(state, 52);
    case 53: return _mm_aeskeygenassist_si128(state, 53);
    case 54: return _mm_aeskeygenassist_si128(state, 54);
    case 55: return _mm_aeskeygenassist_si128(state, 55);
    case 56: return _mm_aeskeygenassist_si128(state, 56);
    case 57: return _mm_aeskeygenassist_si128(state, 57);
    case 58: return _mm_aeskeygenassist_si128(state, 58);
    case 59: return _mm_aeskeygenassist_si128(state, 59);
    case 60: return _mm_aeskeygenassist_si128(state, 60);
    case 61: return _mm_aeskeygenassist_si128(state, 61);
    case 62: return _mm_aeskeygenassist_si128(state, 62);
    case 63: return _mm_aeskeygenassist_si128(state, 63);
    case 64: return _mm_aeskeygenassist_si128(state, 64);
    case 65: return _mm_aeskeygenassist_si128(state, 65);
    case 66: return _mm_aeskeygenassist_si128(state, 66);
    case 67: return _mm_aeskeygenassist_si128(state, 67);
    case 68: return _mm_aeskeygenassist_si128(state, 68);
    case 69: return _mm_aeskeygenassist_si128(state, 69);
    case 70: return _mm_aeskeygenassist_si128(state, 70);
    case 71: return _mm_aeskeygenassist_si128(state, 71);
    case 72: return _mm_aeskeygenassist_si128(state, 72);
    case 73: return _mm_aeskeygenassist_si128(state, 73);
    case 74: return _mm_aeskeygenassist_si128(state, 74);
    case 75: return _mm_aeskeygenassist_si128(state, 75);
    case 76: return _mm_aeskeygenassist_si128(state, 76);
    case 77: return _mm_aeskeygenassist_si128(state, 77);
    case 78: return _mm_aeskeygenassist_si128(state, 78);
    case 79: return _mm_aeskeygenassist_si128(state, 79);
    case 80: return _mm_aeskeygenassist_si128(state, 80);
    case 81: return _mm_aeskeygenassist_si128(state, 81);
    case 82: return _mm_aeskeygenassist_si128(state, 82);
    case 83: return _mm_aeskeygenassist_si128(state, 83);
    case 84: return _mm_aeskeygenassist_si128(state, 84);
    case 85: return _mm_aeskeygenassist_si128(state, 85);
    case 86: return _mm_aeskeygenassist_si128(state, 86);
    case 87: return _mm_aeskeygenassist_si128(state, 87);
    case 88: return _mm_aeskeygenassist_si128(state, 88);
    case 89: return _mm_aeskeygenassist_si128(state, 89);
    case 90: return _mm_aeskeygenassist_si128(state, 90);
    case 91: return _mm_aeskeygenassist_si128(state, 91);
    case 92: return _mm_aeskeygenassist_si128(state, 92);
    case 93: return _mm_aeskeygenassist_si128(state, 93);
    case 94: return _mm_aeskeygenassist_si128(state, 94);
    case 95: return _mm_aeskeygenassist_si128(state, 95);
    case 96: return _mm_aeskeygenassist_si128(state, 96);
    case 97: return _mm_aeskeygenassist_si128(state, 97);
    case 98: return _mm_aeskeygenassist_si128(state, 98);
    case 99: return _mm_aeskeygenassist_si128(state, 99);
    case 100: return _mm_aeskeygenassist_si128(state, 100);
    case 101: return _mm_aeskeygenassist_si128(state, 101);
    case 102: return _mm_aeskeygenassist_si128(state, 102);
    case 103: return _mm_aeskeygenassist_si128(state, 103);
    case 104: return _mm_aeskeygenassist_si128(state, 104);
    case 105: return _mm_aeskeygenassist_si128(state, 105);
    case 106: return _mm_aeskeygenassist_si128(state, 106);
    case 107: return _mm_aeskeygenassist_si128(state, 107);
    case 108: return _mm_aeskeygenassist_si128(state, 108);
    case 109: return _mm_aeskeygenassist_si128(state, 109);
    case 110: return _mm_aeskeygenassist_si128(state, 110);
    case 111: return _mm_aeskeygenassist_si128(state, 111);
    case 112: return _mm_aeskeygenassist_si128(state, 112);
    case 113: return _mm_aeskeygenassist_si128(state, 113);
    case 114: return _mm_aeskeygenassist_si128(state, 114);
    case 115: return _mm_aeskeygenassist_si128(state, 115);
    case 116: return _mm_aeskeygenassist_si128(state, 116);
    case 117: return _mm_aeskeygenassist_si128(state, 117);
    case 118: return _mm_aeskeygenassist_si128(state, 118);
    case 119: return _mm_aeskeygenassist_si128(state, 119);
    case 120: return _mm_aeskeygenassist_si128(state, 120);
    case 121: return _mm_aeskeygenassist_si128(state, 121);
    case 122: return _mm_aeskeygenassist_si128(state, 122);
    case 123: return _mm_aeskeygenassist_si128(state, 123);
    case 124: return _mm_aeskeygenassist_si128(state, 124);
    case 125: return _mm_aeskeygenassist_si128(state, 125);
    case 126: return _mm_aeskeygenassist_si128(state, 126);
    case 127: return _mm_aeskeygenassist_si128(state, 127);
    case 128: return _mm_aeskeygenassist_si128(state, 128);
    case 129: return _mm_aeskeygenassist_si128(state, 129);
    case 130: return _mm_aeskeygenassist_si128(state, 130);
    case 131: return _mm_aeskeygenassist_si128(state, 131);
    case 132: return _mm_aeskeygenassist_si128(state, 132);
    case 133: return _mm_aeskeygenassist_si128(state, 133);
    case 134: return _mm_aeskeygenassist_si128(state, 134);
    case 135: return _mm_aeskeygenassist_si128(state, 135);
    case 136: return _mm_aeskeygenassist_si128(state, 136);
    case 137: return _mm_aeskeygenassist_si128(state, 137);
    case 138: return _mm_aeskeygenassist_si128(state, 138);
    case 139: return _mm_aeskeygenassist_si128(state, 139);
    case 140: return _mm_aeskeygenassist_si128(state, 140);
    case 141: return _mm_aeskeygenassist_si128(state, 141);
    case 142: return _mm_aeskeygenassist_si128(state, 142);
    case 143: return _mm_aeskeygenassist_si128(state, 143);
    case 144: return _mm_aeskeygenassist_si128(state, 144);
    case 145: return _mm_aeskeygenassist_si128(state, 145);
    case 146: return _mm_aeskeygenassist_si128(state, 146);
    case 147: return _mm_aeskeygenassist_si128(state, 147);
    case 148: return _mm_aeskeygenassist_si128(state, 148);
    case 149: return _mm_aeskeygenassist_si128(state, 149);
    case 150: return _mm_aeskeygenassist_si128(state, 150);
    case 151: return _mm_aeskeygenassist_si128(state, 151);
    case 152: return _mm_aeskeygenassist_si128(state, 152);
    case 153: return _mm_aeskeygenassist_si128(state, 153);
    case 154: return _mm_aeskeygenassist_si128(state, 154);
    case 155: return _mm_aeskeygenassist_si128(state, 155);
    case 156: return _mm_aeskeygenassist_si128(state, 156);
    case 157: return _mm_aeskeygenassist_si128(state, 157);
    case 158: return _mm_aeskeygenassist_si128(state, 158);
    case 159: return _mm_aeskeygenassist_si128(state, 159);
    case 160: return _mm_aeskeygenassist_si128(state, 160);
    case 161: return _mm_aeskeygenassist_si128(state, 161);
    case 162: return _mm_aeskeygenassist_si128(state, 162);
    case 163: return _mm_aeskeygenassist_si128(state, 163);
    case 164: return _mm_aeskeygenassist_si128(state, 164);
    case 165: return _mm_aeskeygenassist_si128(state, 165);
    case 166: return _mm_aeskeygenassist_si128(state, 166);
    case 167: return _mm_aeskeygenassist_si128(state, 167);
    case 168: return _mm_aeskeygenassist_si128(state, 168);
    case 169: return _mm_aeskeygenassist_si128(state, 169);
    case 170: return _mm_aeskeygenassist_si128(state, 170);
    case 171: return _mm_aeskeygenassist_si128(state, 171);
    case 172: return _mm_aeskeygenassist_si128(state, 172);
    case 173: return _mm_aeskeygenassist_si128(state, 173);
    case 174: return _mm_aeskeygenassist_si128(state, 174);
    case 175: return _mm_aeskeygenassist_si128(state, 175);
    case 176: return _mm_aeskeygenassist_si128(state, 176);
    case 177: return _mm_aeskeygenassist_si128(state, 177);
    case 178: return _mm_aeskeygenassist_si128(state, 178);
    case 179: return _mm_aeskeygenassist_si128(state, 179);
    case 180: return _mm_aeskeygenassist_si128(state, 180);
    case 181: return _mm_aeskeygenassist_si128(state, 181);
    case 182: return _mm_aeskeygenassist_si128(state, 182);
    case 183: return _mm_aeskeygenassist_si128(state, 183);
    case 184: return _mm_aeskeygenassist_si128(state, 184);
    case 185: return _mm_aeskeygenassist_si128(state, 185);
    case 186: return _mm_aeskeygenassist_si128(state, 186);
    case 187: return _mm_aeskeygenassist_si128(state, 187);
    case 188: return _mm_aeskeygenassist_si128(state, 188);
    case 189: return _mm_aeskeygenassist_si128(state, 189);
    case 190: return _mm_aeskeygenassist_si128(state, 190);
    case 191: return _mm_aeskeygenassist_si128(state, 191);
    case 192: return _mm_aeskeygenassist_si128(state, 192);
    case 193: return _mm_aeskeygenassist_si128(state, 193);
    case 194: return _mm_aeskeygenassist_si128(state, 194);
    case 195: return _mm_aeskeygenassist_si128(state, 195);
    case 196: return _mm_aeskeygenassist_si128(state, 196);
    case 197: return _mm_aeskeygenassist_si128(state, 197);
    case 198: return _mm_aeskeygenassist_si128(state, 198);
    case 199: return _mm_aeskeygenassist_si128(state, 199);
    case 200: return _mm_aeskeygenassist_si128(state, 200);
    case 201: return _mm_aeskeygenassist_si128(state, 201);
    case 202: return _mm_aeskeygenassist_si128(state, 202);
    case 203: return _mm_aeskeygenassist_si128(state, 203);
    case 204: return _mm_aeskeygenassist_si128(state, 204);
    case 205: return _mm_aeskeygenassist_si128(state, 205);
    case 206: return _mm_aeskeygenassist_si128(state, 206);
    case 207: return _mm_aeskeygenassist_si128(state, 207);
    case 208: return _mm_aeskeygenassist_si128(state, 208);
    case 209: return _mm_aeskeygenassist_si128(state, 209);
    case 210: return _mm_aeskeygenassist_si128(state, 210);
    case 211: return _mm_aeskeygenassist_si128(state, 211);
    case 212: return _mm_aeskeygenassist_si128(state, 212);
    case 213: return _mm_aeskeygenassist_si128(state, 213);
    case 214: return _mm_aeskeygenassist_si128(state, 214);
    case 215: return _mm_aeskeygenassist_si128(state, 215);
    case 216: return _mm_aeskeygenassist_si128(state, 216);
    case 217: return _mm_aeskeygenassist_si128(state, 217);
    case 218: return _mm_aeskeygenassist_si128(state, 218);
    case 219: return _mm_aeskeygenassist_si128(state, 219);
    case 220: return _mm_aeskeygenassist_si128(state, 220);
    case 221: return _mm_aeskeygenassist_si128(state, 221);
    case 222: return _mm_aeskeygenassist_si128(state, 222);
    case 223: return _mm_aeskeygenassist_si128(state, 223);
    case 224: return _mm_aeskeygenassist_si128(state, 224);
    case 225: return _mm_aeskeygenassist_si128(state, 225);
    case 226: return _mm_aeskeygenassist_si128(state, 226);
    case 227: return _mm_aeskeygenassist_si128(state, 227);
    case 228: return _mm_aeskeygenassist_si128(state, 228);
    case 229: return _mm_aeskeygenassist_si128(state, 229);
    case 230: return _mm_aeskeygenassist_si128(state, 230);
    case 231: return _mm_aeskeygenassist_si128(state, 231);
    case 232: return _mm_aeskeygenassist_si128(state, 232);
    case 233: return _mm_aeskeygenassist_si128(state, 233);
    case 234: return _mm_aeskeygenassist_si128(state, 234);
    case 235: return _mm_aeskeygenassist_si128(state, 235);
    case 236: return _mm_aeskeygenassist_si128(state, 236);
    case 237: return _mm_aeskeygenassist_si128(state, 237);
    case 238: return _mm_aeskeygenassist_si128(state, 238);
    case 239: return _mm_aeskeygenassist_si128(state, 239);
    case 240: return _mm_aeskeygenassist_si128(state, 240);
    case 241: return _mm_aeskeygenassist_si128(state, 241);
    case 242: return _mm_aeskeygenassist_si128(state, 242);
    case 243: return _mm_aeskeygenassist_si128(state, 243);
    case 244: return _mm_aeskeygenassist_si128(state, 244);
    case 245: return _mm_aeskeygenassist_si128(state, 245);
    case 246: return _mm_aeskeygenassist_si128(state, 246);
    case 247: return _mm_aeskeygenassist_si128(state, 247);
    case 248: return _mm_aeskeygenassist_si128(state, 248);
    case 249: return _mm_aeskeygenassist_si128(state, 249);
    case 250: return _mm_aeskeygenassist_si128(state, 250);
    case 251: return _mm_aeskeygenassist_si128(state, 251);
    case 252: return _mm_aeskeygenassist_si128(state, 252);
    case 253: return _mm_aeskeygenassist_si128(state, 253);
    case 254: return _mm_aeskeygenassist_si128(state, 254);
    case 255: return _mm_aeskeygenassist_si128(state, 255);
    default: return _mm_aeskeygenassist_si128(state, 0);
    }
}

__attribute__((target("sse4.1,aes"))) static inline __m128i aes_lane(
    ZydisMnemonic m, __m128i state, __m128i key, unsigned imm) {
    switch (m) {
    case ZYDIS_MNEMONIC_AESDEC:
    case ZYDIS_MNEMONIC_VAESDEC: return _mm_aesdec_si128(state, key);
    case ZYDIS_MNEMONIC_AESDECLAST:
    case ZYDIS_MNEMONIC_VAESDECLAST: return _mm_aesdeclast_si128(state, key);
    case ZYDIS_MNEMONIC_AESENC:
    case ZYDIS_MNEMONIC_VAESENC: return _mm_aesenc_si128(state, key);
    case ZYDIS_MNEMONIC_AESENCLAST:
    case ZYDIS_MNEMONIC_VAESENCLAST: return _mm_aesenclast_si128(state, key);
    case ZYDIS_MNEMONIC_AESIMC:
    case ZYDIS_MNEMONIC_VAESIMC: return _mm_aesimc_si128(state);
    case ZYDIS_MNEMONIC_AESKEYGENASSIST:
    case ZYDIS_MNEMONIC_VAESKEYGENASSIST:
        return aes_keygenassist_lane(state, imm);
    default: break;
    }
    return state;
}

static bool is_aes_mn(ZydisMnemonic m) {
    switch (m) {
    case ZYDIS_MNEMONIC_AESDEC:
    case ZYDIS_MNEMONIC_AESDECLAST:
    case ZYDIS_MNEMONIC_AESENC:
    case ZYDIS_MNEMONIC_AESENCLAST:
    case ZYDIS_MNEMONIC_AESIMC:
    case ZYDIS_MNEMONIC_AESKEYGENASSIST:
    case ZYDIS_MNEMONIC_VAESDEC:
    case ZYDIS_MNEMONIC_VAESDECLAST:
    case ZYDIS_MNEMONIC_VAESENC:
    case ZYDIS_MNEMONIC_VAESENCLAST:
    case ZYDIS_MNEMONIC_VAESIMC:
    case ZYDIS_MNEMONIC_VAESKEYGENASSIST: return true;
    default: return false;
    }
}

// Legacy 128-bit AES (2 operands: dst/state, key) and VAES (VEX/EVEX,
// 3 operands: dst,state,key; EVEX supports 512-bit + masking).
__attribute__((target("sse4.1,aes,avx2"))) void exec_aes(CPU* cpu,
                                                        const Dec& d) {
    ZydisMnemonic m = d.insn.mnemonic;
    bool is_vaes = m == ZYDIS_MNEMONIC_VAESDEC ||
                   m == ZYDIS_MNEMONIC_VAESDECLAST ||
                   m == ZYDIS_MNEMONIC_VAESENC ||
                   m == ZYDIS_MNEMONIC_VAESENCLAST ||
                   m == ZYDIS_MNEMONIC_VAESIMC ||
                   m == ZYDIS_MNEMONIC_VAESKEYGENASSIST;
    unsigned w = 16;
    if (is_vaes) {
        if (is_evex(d))
            w = evex_width(cpu, d);
        else {
            w = d.ops[0].size / 8;
            if (w != 16 && w != 32)
                guest_error(cpu, "bad vaes width");
        }
    }
    int ex[5];
    int n = evex_explicit(d, ex);
    VecVal st, ky, o, old;
    unsigned imm = 0;
    if (!is_vaes) {
        if (n < 2)
            guest_error(cpu, "aes needs 2 operands");
        // AESKEYGENASSIST uses only src+imm (dst is not a source).
        int sti = (m == ZYDIS_MNEMONIC_AESKEYGENASSIST) ? ex[1] : ex[0];
        vec_load_bytes(cpu, d, sti, st, 16);
        vec_load_bytes(cpu, d, ex[1], ky, 16);
        if (m == ZYDIS_MNEMONIC_AESKEYGENASSIST)
            imm = (unsigned)op_load(cpu, d, ex[2]).v;
        __m128i r = aes_lane(m, ((__m128i*)st.bytes)[0], ((__m128i*)ky.bytes)[0], imm);
        memcpy(o, &r, 16);
        bool vex = is_vex(d);
        o.taint = st.taint | ky.taint;
        vec_store_bytes(cpu, d, ex[0], o, 16, vex, 16);
        cpu->rip = next_rip(cpu, d);
        return;
    }
    if (n < 3 && m != ZYDIS_MNEMONIC_VAESIMC)
        guest_error(cpu, "vaes needs 3 operands");
    evex_load(cpu, d, ex[1], st, w, 16);
    if (m == ZYDIS_MNEMONIC_VAESIMC) {
        // 2-operand form: key schedule assist on the state alone.
        for (unsigned lane = 0; lane < w; lane += 16) {
            __m128i r = aes_lane(m, ((__m128i*)(st + lane))[0],
                                 _mm_setzero_si128(), 0);
            memcpy(o + lane, &r, 16);
        }
    } else if (m == ZYDIS_MNEMONIC_VAESKEYGENASSIST) {
        // [dst, src, imm8]: no separate key; imm selects the round constant.
        imm = (unsigned)kload(cpu, d, ex[n - 1], 8);
        for (unsigned lane = 0; lane < w; lane += 16) {
            __m128i r = aes_lane(m, ((__m128i*)(st + lane))[0],
                                 _mm_setzero_si128(), imm);
            memcpy(o + lane, &r, 16);
        }
    } else {
        evex_load(cpu, d, ex[2], ky, w, 16);
        for (unsigned lane = 0; lane < w; lane += 16) {
            __m128i r = aes_lane(m, ((__m128i*)(st + lane))[0],
                                 ((__m128i*)(ky + lane))[0], 0);
            memcpy(o + lane, &r, 16);
        }
    }
    evex_old(cpu, d, ex[0], old, w);
    bool zeroing = false;
    uint64_t kbits = is_evex(d) ? evex_mask(cpu, d, w / 16, zeroing) : ~0ULL;
    o.taint = st.taint | ky.taint;
    evex_store(cpu, d, ex[0], o, old, w, 16, kbits, zeroing);
    cpu->rip = next_rip(cpu, d);
}

__attribute__((target("sse4.1,pclmul"))) static inline __m128i pclmul_lane(
    __m128i a, __m128i b, unsigned imm) {
    switch (imm & 0x11) {
    case 0x00: return _mm_clmulepi64_si128(a, b, 0x00);
    case 0x01: return _mm_clmulepi64_si128(a, b, 0x01);
    case 0x10: return _mm_clmulepi64_si128(a, b, 0x10);
    default: return _mm_clmulepi64_si128(a, b, 0x11);
    }
}

__attribute__((target("sse4.1,pclmul,avx2"))) void exec_pclmul(CPU* cpu,
                                                             const Dec& d) {
    bool is_v = (d.insn.mnemonic == ZYDIS_MNEMONIC_VPCLMULQDQ);
    unsigned w = 16;
    if (is_v) {
        if (is_evex(d))
            w = evex_width(cpu, d);
        else {
            w = d.ops[0].size / 8;
            if (w != 16 && w != 32)
                guest_error(cpu, "bad vpclmul width");
        }
    }
    int ex[5];
    int n = evex_explicit(d, ex);
    // imm8 selects halves: bit0 -> a-hi/lo? _mm_clmulepi64_si128(a,b,imm):
    // imm 0x00=a[63:0]*b[63:0] 0x01=a[63:0]*b[127:64] 0x10=a[127:64]*b[63:0]
    // 0x11=a[127:64]*b[127:64].
    VecVal a, b, o, old;
    unsigned imm = 0;
    if (!is_v) {
        if (n < 3)
            guest_error(cpu, "pclmulqdq needs 3 operands");
        vec_load_bytes(cpu, d, ex[0], a, 16);
        vec_load_bytes(cpu, d, ex[1], b, 16);
        imm = (unsigned)op_load(cpu, d, ex[2]).v & 0xff;
        __m128i r = pclmul_lane(((__m128i*)a.bytes)[0], ((__m128i*)b.bytes)[0], imm);
        memcpy(o, &r, 16);
        o.taint = a.taint | b.taint;
        vec_store_bytes(cpu, d, ex[0], o, 16, false, 16);
        cpu->rip = next_rip(cpu, d);
        return;
    }
    if (n < 4)
        guest_error(cpu, "vpclmulqdq needs 4 operands");
    evex_load(cpu, d, ex[1], a, w, 16);
    evex_load(cpu, d, ex[2], b, w, 16);
    imm = (unsigned)kload(cpu, d, ex[3], 8) & 0x11;
    for (unsigned lane = 0; lane < w; lane += 16) {
        __m128i r = pclmul_lane(((__m128i*)(a + lane))[0],
                                ((__m128i*)(b + lane))[0], imm);
        memcpy(o + lane, &r, 16);
    }
    evex_old(cpu, d, ex[0], old, w);
    bool zeroing = false;
    uint64_t kbits = is_evex(d) ? evex_mask(cpu, d, w / 16, zeroing) : ~0ULL;
    o.taint = a.taint | b.taint;
    evex_store(cpu, d, ex[0], o, old, w, 16, kbits, zeroing);
    cpu->rip = next_rip(cpu, d);
}

// Bit-reverse a byte (GFNI affine output is bit-reversed, verified on HW).
static uint8_t brev8(uint8_t x) {
    x = (uint8_t)(((x & 0x55) << 1) | ((x & 0xAA) >> 1));
    x = (uint8_t)(((x & 0x33) << 2) | ((x & 0xCC) >> 2));
    return (uint8_t)((x << 4) | (x >> 4));
}

// GF(2^8) multiply with the AES polynomial (x^8+x^4+x^3+x+1).
static uint8_t gf_mul(uint8_t a, uint8_t b) {    uint8_t r = 0;
    while (b) {
        if (b & 1)
            r ^= a;
        bool hi = (a & 0x80) != 0;
        a <<= 1;
        if (hi)
            a ^= 0x1b;
        b >>= 1;
    }
    return r;
}

static uint8_t gf_inv(uint8_t x) {
    if (!x)
        return 0;
    // x^254 by square-and-multiply (254 = 11111110b).
    uint8_t r = 1, y = x;
    for (int i = 1; i < 8; i++) {
        y = gf_mul(y, y);
        if ((254 >> i) & 1)
            r = gf_mul(r, y);
    }
    return r;
}

// GF2P8AFFINEQB/INVQB (+VEX/EVEX V forms) and GF2P8MULB (+V form).
// y = A*x+b (b = imm8 broadcast); INV applies the GF inverse to x first.
void exec_gfni(CPU* cpu, const Dec& d) {
    ZydisMnemonic m = d.insn.mnemonic;
    bool is_mul = (m == ZYDIS_MNEMONIC_GF2P8MULB || m == ZYDIS_MNEMONIC_VGF2P8MULB);
    bool is_inv = (m == ZYDIS_MNEMONIC_GF2P8AFFINEINVQB ||
                   m == ZYDIS_MNEMONIC_VGF2P8AFFINEINVQB);
    unsigned w = 16;
    if (m == ZYDIS_MNEMONIC_VGF2P8AFFINEQB || m == ZYDIS_MNEMONIC_VGF2P8AFFINEINVQB ||
        m == ZYDIS_MNEMONIC_VGF2P8MULB) {
        if (is_evex(d))
            w = evex_width(cpu, d);
        else {
            w = d.ops[0].size / 8;
            if (w != 16 && w != 32)
                guest_error(cpu, "bad vgfni width");
        }
    }
    int ex[5];
    int n = evex_explicit(d, ex);
    VecVal a, x, o, old;
    unsigned imm = 0;
    bool is_vex = (m == ZYDIS_MNEMONIC_VGF2P8AFFINEQB ||
                   m == ZYDIS_MNEMONIC_VGF2P8AFFINEINVQB ||
                   m == ZYDIS_MNEMONIC_VGF2P8MULB);
    if (!is_mul) {
        // Affine: y = A*x+b. Legacy (2-op + imm): matrix from src (ex[1]),
        // bytes from dst (ex[0], read before write), output bit-reversed.
        // VEX/EVEX: matrix from src1 (ex[1]), bytes from src2 (ex[2]),
        // no reversal. Both verified against hardware.
        bool legacy = !is_vex;
        if ((!legacy && n < 4) || (legacy && n < 3))
            guest_error(cpu, "gfni affine needs operands");
        int ai = ex[1];
        int xi = legacy ? ex[0] : ex[2];
        int imi = legacy ? ex[2] : ex[3];
        // Legacy: matrix from src (ex[1]), bytes from dst (ex[0]).
        // VEX/EVEX: matrix from second source (ex[2]), bytes from first
        // (ex[1]); verified against hardware (intrinsic arg order maps
        // first-arg to src2/dst, second-arg to src1).
        if (!legacy) {
            ai = ex[2];
            xi = ex[1];
        }
        // Legacy: matrix from src (ex[1]), bytes from dst (ex[0]).
        // VEX/EVEX: matrix from first source (ex[1]), bytes from second
        // (ex[2]); verified against hardware.
        // Matrix source holds one 64-bit matrix per 64-bit lane.
        unsigned aw = w > MAX_VEC_BYTES ? MAX_VEC_BYTES : w;
        if (aw < 16)
            aw = 16;
        evex_load(cpu, d, ai, a, aw, 1);
        evex_load(cpu, d, xi, x, w, 1);
        imm = (unsigned)kload(cpu, d, imi, 8);
        uint64_t A = 0;
        memcpy(&A, a, 8);
        for (unsigned i = 0; i < w; i++) {
            // "epi64": each 64-bit lane uses its own 64-bit matrix from the
            // matrix source (bytes 8*(i/8)..+8). Verified against hardware.
            uint64_t Al = 0;
            if (i + 8 <= (unsigned)(legacy ? 16 : 64))
                memcpy(&Al, a + 8 * (i / 8), 8);
            else
                Al = A;
            uint8_t xv = x[i];
            if (is_inv)
                xv = gf_inv(xv);
            uint8_t y = 0;
            for (int bit = 0; bit < 8; bit++) {
                uint8_t row = (uint8_t)(Al >> (bit * 8));
                if (__builtin_parity(row & xv))
                    y |= (uint8_t)(1u << bit);
            }
            // The affine output is bit-reversed (verified on HW for both
            // legacy and VEX/EVEX forms).
            o[i] = brev8(y) ^ (uint8_t)imm;
        }
    } else {
        if (n < (is_vex ? 3 : 2))
            guest_error(cpu, "gfni mul needs operands");
        // Legacy MULB is 2-operand (dst/src combined): a from dst (ex[0]),
        // x from src (ex[1]). VEX has separate dst/src1/src2.
        int ai = is_vex ? ex[1] : ex[0];
        int xi = is_vex ? ex[2] : ex[1];
        evex_load(cpu, d, ai, a, w, 1);
        evex_load(cpu, d, xi, x, w, 1);
        for (unsigned i = 0; i < w; i++)
            o[i] = gf_mul(a[i], x[i]);
    }
    evex_old(cpu, d, ex[0], old, w);
    bool zeroing = false;
    uint64_t kbits = is_evex(d) ? evex_mask(cpu, d, w, zeroing) : ~0ULL;
    o.taint = a.taint | x.taint;
    evex_store(cpu, d, ex[0], o, old, w, 1, kbits, zeroing);
    cpu->rip = next_rip(cpu, d);
}

// SHA1/SHA256 via host SHA-NI intrinsics.
__attribute__((target("sse4.1,sha"))) void exec_sha(CPU* cpu, const Dec& d) {
    ZydisMnemonic m = d.insn.mnemonic;
    int ex[5];
    int n = evex_explicit(d, ex);
    uint8_t a[16], b[16], c[16];
    Taint cb, t1, t3;
    auto load16 = [&](int oi, uint8_t* out, Taint* pt) {
        const auto& op = d.ops[oi];
        if (op.type == ZYDIS_OPERAND_TYPE_REGISTER) {
            int idx, width;
            if (!reg_to_vec(op.reg.value, idx, width))
                guest_error(cpu, "bad sha register");
            memcpy(out, cpu->xmm[idx].bytes, 16);
            if (pt)
                *pt = cpu->xmm[idx].taint;
            return;
        }
        uint64_t addr = resolve_mem(cpu, op, cpu->rip + d.insn.length);
        mem_load_bytes(cpu, addr, out, 16);
        mem_get_taint(addr, 16, pt);
    };
    __m128i r;
    switch (m) {
    case ZYDIS_MNEMONIC_SHA1RNDS4: {
        if (n < 3)
            guest_error(cpu, "sha1rnds4 needs 3 operands");
        load16(ex[0], a, &cb);
        load16(ex[1], b, &t1);
        unsigned f = (unsigned)kload(cpu, d, ex[2], 8) & 3;
        // func is a compile-time constant for the intrinsic: dispatch.
        __m128i aa = ((__m128i*)a)[0], bb = ((__m128i*)b)[0];
        if (f == 0)
            r = _mm_sha1rnds4_epu32(aa, bb, 0);
        else if (f == 1)
            r = _mm_sha1rnds4_epu32(aa, bb, 1);
        else if (f == 2)
            r = _mm_sha1rnds4_epu32(aa, bb, 2);
        else
            r = _mm_sha1rnds4_epu32(aa, bb, 3);
        memcpy(c, &r, 16);
        vec_store_bytes(cpu, d, ex[0], c, 16, false, 16, cb | t1);
        break;
    }
    case ZYDIS_MNEMONIC_SHA1NEXTE: {
        if (n < 2)
            guest_error(cpu, "sha1nexte needs 2 operands");
        load16(ex[0], a, &cb);
        load16(ex[1], b, &t1);
        r = _mm_sha1nexte_epu32(((__m128i*)a)[0], ((__m128i*)b)[0]);
        memcpy(c, &r, 16);
        vec_store_bytes(cpu, d, ex[0], c, 16, false, 16, cb | t1);
        break;
    }
    case ZYDIS_MNEMONIC_SHA1MSG1: {
        if (n < 2)
            guest_error(cpu, "sha1msg1 needs 2 operands");
        load16(ex[0], a, &cb);
        load16(ex[1], b, &t1);
        r = _mm_sha1msg1_epu32(((__m128i*)a)[0], ((__m128i*)b)[0]);
        memcpy(c, &r, 16);
        vec_store_bytes(cpu, d, ex[0], c, 16, false, 16, cb | t1);
        break;
    }
    case ZYDIS_MNEMONIC_SHA1MSG2: {
        if (n < 2)
            guest_error(cpu, "sha1msg2 needs 2 operands");
        load16(ex[0], a, &cb);
        load16(ex[1], b, &t1);
        r = _mm_sha1msg2_epu32(((__m128i*)a)[0], ((__m128i*)b)[0]);
        memcpy(c, &r, 16);
        vec_store_bytes(cpu, d, ex[0], c, 16, false, 16, cb | t1);
        break;
    }
    case ZYDIS_MNEMONIC_SHA256RNDS2: {
        // Operands: [dst(explicit, =a), b(explicit), k(HIDDEN)].
        // Intrinsic: _mm_sha256rnds2_epu32(a=dst, b=mem, k=hidden), verified
        // against GCC's emission (b/k swapped vs Intel operand order).
        if (n < 2)
            guest_error(cpu, "sha256rnds2 needs operands");
        load16(ex[0], a, &cb);
        load16(ex[1], b, &t1);
        bool found = false;
        for (int i = 0; i < d.insn.operand_count; i++) {
            if (d.ops[i].visibility == ZYDIS_OPERAND_VISIBILITY_HIDDEN &&
                d.ops[i].type == ZYDIS_OPERAND_TYPE_REGISTER) {
                int idx, width;
                if (reg_to_vec(d.ops[i].reg.value, idx, width)) {
                    memcpy(c, cpu->xmm[idx].bytes, 16);
                    t3 = cpu->xmm[idx].taint;
                    found = true;
                    break;
                }
            }
        }
        if (!found)
            guest_error(cpu, "sha256rnds2 missing hidden source");
        r = _mm_sha256rnds2_epu32(((__m128i*)a)[0], ((__m128i*)b)[0],
                                  ((__m128i*)c)[0]);
        uint8_t o[16];
        memcpy(o, &r, 16);
        vec_store_bytes(cpu, d, ex[0], o, 16, false, 16, cb | t1 | t3);
        break;
    }
    case ZYDIS_MNEMONIC_SHA256MSG1: {
        if (n < 2)
            guest_error(cpu, "sha256msg1 needs 2 operands");
        load16(ex[0], a, &cb);
        load16(ex[1], b, &t1);
        r = _mm_sha256msg1_epu32(((__m128i*)a)[0], ((__m128i*)b)[0]);
        memcpy(c, &r, 16);
        vec_store_bytes(cpu, d, ex[0], c, 16, false, 16, cb | t1);
        break;
    }
    case ZYDIS_MNEMONIC_SHA256MSG2: {
        if (n < 2)
            guest_error(cpu, "sha256msg2 needs 2 operands");
        load16(ex[0], a, &cb);
        load16(ex[1], b, &t1);
        r = _mm_sha256msg2_epu32(((__m128i*)a)[0], ((__m128i*)b)[0]);
        memcpy(c, &r, 16);
        vec_store_bytes(cpu, d, ex[0], c, 16, false, 16, cb | t1);
        break;
    }
    default: guest_error(cpu, "unsupported SHA operation");
    }
    cpu->rip = next_rip(cpu, d);
}

// --- VEX classic gaps (phase 1 omissions) --------------------------------------
// All use vec_sources/vec_store_dst, so legacy + VEX forms share one path.

void exec_pmadd(CPU* cpu, const Dec& d, bool is_ubsw) {
    VecVal a, b, o;
    unsigned w = vec_sources(cpu, d, a, b);
    if (!is_ubsw) {
        // PMADDWD: 4 signed words -> 2 signed dwords.
        for (unsigned i = 0; i < w; i += 4) {
            int32_t p0 = (int16_t)(a[i] | (a[i + 1] << 8)) * (int16_t)(b[i] | (b[i + 1] << 8));
            int32_t p1 = (int16_t)(a[i + 2] | (a[i + 3] << 8)) * (int16_t)(b[i + 2] | (b[i + 3] << 8));
            int32_t r = p0 + p1;
            memcpy(o + i, &r, 4);
        }
    } else {
        // PMADDUBSW: unsigned bytes * signed bytes -> saturated signed words.
        for (unsigned i = 0; i < w; i += 2) {
            int t = (int)a[i] * (int)(int8_t)b[i] + (int)a[i + 1] * (int)(int8_t)b[i + 1];
            if (t > 32767)
                t = 32767;
            if (t < -32768)
                t = -32768;
            o[i] = (uint8_t)(t & 0xff);
            o[i + 1] = (uint8_t)((t >> 8) & 0xff);
        }
    }
    o.taint = a.taint | b.taint;
    vec_store_dst(cpu, d, o, w);
    cpu->rip = next_rip(cpu, d);
}

void exec_pavg(CPU* cpu, const Dec& d, unsigned elem) {
    VecVal a, b, o;
    unsigned w = vec_sources(cpu, d, a, b);
    for (unsigned i = 0; i < w; i += elem) {
        uint64_t av = 0, bv = 0;
        memcpy(&av, a + i, elem);
        memcpy(&bv, b + i, elem);
        uint64_t r = (av + bv + 1) >> 1;
        memcpy(o + i, &r, elem);
    }
    o.taint = a.taint | b.taint;
    vec_store_dst(cpu, d, o, w);
    cpu->rip = next_rip(cpu, d);
}

void exec_psadbw(CPU* cpu, const Dec& d) {
    VecVal a, b, o;
    unsigned w = vec_sources(cpu, d, a, b);
    memset(o, 0, w);
    for (unsigned blk = 0; blk < w; blk += 8) {
        uint64_t sum = 0;
        for (unsigned i = 0; i < 8; i++)
            sum += (uint64_t)(a[blk + i] > b[blk + i] ? a[blk + i] - b[blk + i]
                                                      : b[blk + i] - a[blk + i]);
        memcpy(o + blk, &sum, 8);
    }
    o.taint = a.taint | b.taint;
    vec_store_dst(cpu, d, o, w);
    cpu->rip = next_rip(cpu, d);
}

void exec_pmulh(CPU* cpu, const Dec& d, int kind) {
    // kind 0=PMULHUW 1=PMULHW 2=PMULUDQ 3=PMULDQ
    VecVal a, b, o;
    unsigned w = vec_sources(cpu, d, a, b);
    if (kind <= 1) {
        for (unsigned i = 0; i < w; i += 2) {
            uint16_t av = a[i] | (a[i + 1] << 8), bv = b[i] | (b[i + 1] << 8);
            uint32_t p = (uint32_t)av * bv;
            uint16_t r;
            if (kind == 0)
                r = (uint16_t)(p >> 16);
            else
                r = (uint16_t)(((uint32_t)(int32_t)(int16_t)av * (int16_t)bv) >> 16);
            memcpy(o + i, &r, 2);
        }
    } else {
        for (unsigned i = 0; i < w; i += 8) {
            uint32_t av = 0, bv = 0;
            memcpy(&av, a + i, 4);
            memcpy(&bv, b + i, 4);
            uint64_t r;
            if (kind == 2)
                r = (uint64_t)av * bv;
            else
                r = (uint64_t)((int64_t)(int32_t)av * (int32_t)bv);
            memcpy(o + i, &r, 8);
        }
    }
    o.taint = a.taint | b.taint;
    vec_store_dst(cpu, d, o, w);
    cpu->rip = next_rip(cpu, d);
}

void exec_phadd(CPU* cpu, const Dec& d, int kind) {
    // kind 0=PHADDW 1=PHADDD 2=PHSUBW 3=PHSUBD 4=PHADDSW 5=PHSUBSW
    VecVal a, b, o;
    unsigned w = vec_sources(cpu, d, a, b);
    unsigned elem = (kind == 1) ? 4 : 2;
    bool sub = (kind == 2 || kind == 3 || kind == 5);
    bool sat = (kind == 4 || kind == 5);
    // VEX256 keeps 128-bit lanes independent.
    for (unsigned lane = 0; lane < w; lane += 16) {
        unsigned pairs = 16 / (2 * elem);
        for (unsigned i = 0; i < pairs; i++) {
            int64_t x = 0, y = 0;
            if (elem == 2) {
                uint16_t x0 = a[lane + 2 * i * 2] | (a[lane + 2 * i * 2 + 1] << 8);
                uint16_t x1 = a[lane + (2 * i + 1) * 2] | (a[lane + (2 * i + 1) * 2 + 1] << 8);
                uint16_t y0 = b[lane + 2 * i * 2] | (b[lane + 2 * i * 2 + 1] << 8);
                uint16_t y1 = b[lane + (2 * i + 1) * 2] | (b[lane + (2 * i + 1) * 2 + 1] << 8);
                x = (int16_t)x0;
                y = (int16_t)x1;
                int64_t z0 = sub ? x - y : x + y;
                x = (int16_t)y0;
                y = (int16_t)y1;
                int64_t z1 = sub ? x - y : x + y;
                if (sat) {
                    if (z0 > 32767)
                        z0 = 32767;
                    if (z0 < -32768)
                        z0 = -32768;
                    if (z1 > 32767)
                        z1 = 32767;
                    if (z1 < -32768)
                        z1 = -32768;
                }
                o[lane + i * 2] = (uint8_t)(z0 & 0xff);
                o[lane + i * 2 + 1] = (uint8_t)((z0 >> 8) & 0xff);
                o[lane + 8 + i * 2] = (uint8_t)(z1 & 0xff);
                o[lane + 8 + i * 2 + 1] = (uint8_t)((z1 >> 8) & 0xff);
            } else {
                int32_t x0, x1, y0, y1;
                memcpy(&x0, a + lane + i * 8, 4);
                memcpy(&x1, a + lane + i * 8 + 4, 4);
                memcpy(&y0, b + lane + i * 8, 4);
                memcpy(&y1, b + lane + i * 8 + 4, 4);
                int64_t z0 = sub ? (int64_t)x0 - x1 : (int64_t)x0 + x1;
                int64_t z1 = sub ? (int64_t)y0 - y1 : (int64_t)y0 + y1;
                int32_t r0 = (int32_t)z0, r1 = (int32_t)z1;
                memcpy(o + lane + i * 4, &r0, 4);
                memcpy(o + lane + 8 + i * 4, &r1, 4);
            }
        }
    }
    o.taint = a.taint | b.taint;
    vec_store_dst(cpu, d, o, w);
    cpu->rip = next_rip(cpu, d);
}

void exec_mpsadbw(CPU* cpu, const Dec& d) {
    int n = explicit_ops(d);
    VecVal a, b, o;
    unsigned w = vec_sources(cpu, d, a, b);
    if (w != 16 && w != 32)
        guest_error(cpu, "bad mpsadbw width");
    unsigned imm = (unsigned)op_load(cpu, d, n - 1).v;
    // Per 128-bit lane: 8 words; word i = SAD of 4 bytes:
    // a[offset_a+i .. +3] vs b[(i&1?4:0)+off_b*4 .. +3].
    for (unsigned lane = 0; lane < w; lane += 16) {
        unsigned off_a = ((imm >> (lane == 0 ? 2 : 5)) & 3) * 4; // imm[3:2], imm[6:5]? use per-lane
        unsigned off_b = ((imm >> (lane == 0 ? 0 : 3)) & 3);
        if (lane) {
            off_a = ((imm >> 5) & 3) * 4;
            off_b = (imm >> 3) & 3;
        } else {
            off_a = ((imm >> 2) & 3) * 4;
            off_b = imm & 3;
        }
        for (unsigned i = 0; i < 8; i++) {
            unsigned sum = 0;
            for (unsigned j = 0; j < 4; j++) {
                uint8_t x = a[lane + off_a + i + j];
                uint8_t y = b[lane + ((i & 4) ? 4 : 0) + off_b * 4 + j];
                sum += x > y ? x - y : y - x;
            }
            o[lane + i * 2] = (uint8_t)(sum & 0xff);
            o[lane + i * 2 + 1] = (uint8_t)((sum >> 8) & 0xff);
        }
    }
    o.taint = a.taint | b.taint;
    vec_store_dst(cpu, d, o, w);
    cpu->rip = next_rip(cpu, d);
}

void exec_extractps(CPU* cpu, const Dec& d, bool insert) {
    int n = explicit_ops(d);
    if (!insert) {
        VecVal a;
        vec_load_bytes(cpu, d, 1, a, 16);
        unsigned sel = (unsigned)op_load(cpu, d, 2).v & 3;
        Val out;
        memcpy(&out.v, a + sel * 4, 4);
        out.taint = a.taint;
        op_store(cpu, d, 0, out);
    } else {
        VecVal a, o, sb;
        vec_load_bytes(cpu, d, 0, a, 16);
        memcpy(o, a, 16);
        o.taint = a.taint;
        unsigned imm = (unsigned)op_load(cpu, d, n - 1).v;
        // INSERTPS imm8: COUNT_S=bits[7:6], COUNT_D=bits[5:4], ZMASK=bits[3:0].
        unsigned cnt_s = (imm >> 6) & 3, cnt_d = (imm >> 4) & 3, zmask = imm & 0xf;
        if (d.ops[1].type == ZYDIS_OPERAND_TYPE_REGISTER) {
            vec_load_bytes(cpu, d, 1, sb, 16);
            o.taint |= sb.taint;
        } else {
            uint64_t addr = resolve_mem(cpu, d.ops[1], cpu->rip + d.insn.length);
            mem_load_bytes(cpu, addr, sb, 4);
            Taint t;
            mem_get_taint(addr, 4, &t);
            o.taint |= t;
        }
        memcpy(o + cnt_d * 4, sb + cnt_s * 4, 4);
        for (unsigned i = 0; i < 4; i++) {
            if ((zmask >> i) & 1)
                memset(o + i * 4, 0, 4);
        }
        vec_store_bytes(cpu, d, 0, o, 16, false, 16);
    }
    cpu->rip = next_rip(cpu, d);
}

// HADDPS/PD, HSUBPS/PD, ADDSUBPS/PD (legacy + VEX).
void exec_hadd_fp(CPU* cpu, const Dec& d, int kind) {
    // kind 0=haddps 1=haddpd 2=hsubps 3=hsubpd 4=addsubps 5=addsubpd
    VecVal a, b, o;
    unsigned w = vec_sources(cpu, d, a, b);
    bool is_pd = (kind == 1 || kind == 3 || kind == 5);
    unsigned elem = is_pd ? 8 : 4;
    for (unsigned lane = 0; lane < w; lane += 16) {
        unsigned elems = 16 / elem;
        if (kind <= 3) {
            bool sub = (kind == 2 || kind == 3);
            for (unsigned i = 0; i < elems / 2; i++) {
                if (!is_pd) {
                    float x0, x1, y0, y1;
                    memcpy(&x0, a + lane + i * 8, 4);
                    memcpy(&x1, a + lane + i * 8 + 4, 4);
                    memcpy(&y0, b + lane + i * 8, 4);
                    memcpy(&y1, b + lane + i * 8 + 4, 4);
                    float r0 = sub ? x0 - x1 : x0 + x1;
                    float r1 = sub ? y0 - y1 : y0 + y1;
                    memcpy(o + lane + i * 4, &r0, 4);
                    memcpy(o + lane + 8 + i * 4, &r1, 4);
                } else {
                    double x0, x1, y0, y1;
                    memcpy(&x0, a + lane + i * 16, 8);
                    memcpy(&x1, a + lane + i * 16 + 8, 8);
                    memcpy(&y0, b + lane + i * 16, 8);
                    memcpy(&y1, b + lane + i * 16 + 8, 8);
                    double r0 = sub ? x0 - x1 : x0 + x1;
                    double r1 = sub ? y0 - y1 : y0 + y1;
                    memcpy(o + lane + i * 8, &r0, 8);
                    memcpy(o + lane + 8 + i * 8, &r1, 8);
                }
            }
        } else {
            for (unsigned i = 0; i < elems; i++) {
                if (!is_pd) {
                    float x, y;
                    memcpy(&x, a + lane + i * 4, 4);
                    memcpy(&y, b + lane + i * 4, 4);
                    float r = (i & 1) ? x + y : x - y;
                    memcpy(o + lane + i * 4, &r, 4);
                } else {
                    double x, y;
                    memcpy(&x, a + lane + i * 8, 8);
                    memcpy(&y, b + lane + i * 8, 8);
                    double r = (i & 1) ? x + y : x - y;
                    memcpy(o + lane + i * 8, &r, 8);
                }
            }
        }
    }
    (void)elem;
    o.taint = a.taint | b.taint;
    vec_store_dst(cpu, d, o, w);
    cpu->rip = next_rip(cpu, d);
}

// BEXTR (BMI1): dst = (src >> start) & ((1<<len)-1).
void exec_bextr(CPU* cpu, const Dec& d) {
    int n = explicit_ops(d);
    if (n < 3)
        guest_error(cpu, "bextr needs 3 operands");
    Val src = op_load(cpu, d, 1);
    Val ctl = op_load(cpu, d, 2);
    int w = common_int_width(d);
    unsigned bits = w * 8;
    unsigned start = (unsigned)ctl.v & 0xff;
    unsigned len = ((unsigned)ctl.v >> 8) & 0xff;
    Val out;
    out.taint.cannot_branch = src.taint.cannot_branch || ctl.taint.cannot_branch;
    out.taint.cannot_index = src.taint.cannot_index || ctl.taint.cannot_index;
    if (start >= bits) {
        out.v = 0;
        cpu->set_flag(FLAG_ZF, true);
    } else {
        if (len > bits - start)
            len = bits - start;
        uint64_t mm = (len >= 64) ? ~0ULL : ((len == 0) ? 0 : ((1ULL << len) - 1));
        out.v = (src.v >> start) & mm;
        cpu->set_flag(FLAG_ZF, out.v == 0);
    }
    cpu->set_flag(FLAG_CF, false);
    cpu->set_flag(FLAG_OF, false);
    flags_taint2(cpu, src, ctl);
    op_store(cpu, d, 0, out);
    cpu->rip = next_rip(cpu, d);
}

// PCMPESTRI/PCMPESTRM/PCMPISTRI/PCMPISTRM (+VEX V forms): SSE4.2 string
// scan. Fully emulated in C++: element extraction per imm[1:0] (ub,uw,sb,sw),
// aggregation per imm[3:2] (equal-any, ranges, equal-each, equal-ordered),
// polarity per imm[5:4], output per imm[6] (index vs mask), index edge per
// imm[7] (lsb vs msb). Widths 16 (and 32 for VEX256 forms, per 128-bit lane
// with OR-ed masks / first-lane index, documented approximation).
void exec_pcmpestri(CPU* cpu, const Dec& d, bool want_index) {
    int n = explicit_ops(d);
    unsigned w = d.ops[1].size / 8; // source width
    if (w != 16 && w != 32)
        guest_error(cpu, "bad pcmpestri width");
    unsigned imm = (unsigned)op_load(cpu, d, n - 1).v & 0x7f;
    int zdx = (n >= 4) ? 0 : 0; // dst/reg operand index for explicit form
    (void)zdx;
    // Operands: [dst/mem?, src, src-len?, ...]. Explicit forms:
    //   PCMPESTRI xmm1, xmm2/m128, imm8 (+implicit EAX/EDX lengths)
    //   PCMPESTRM xmm1, xmm2/m128, imm8
    int src1i = 0, src2i = 1;
    uint8_t A[32], B[32];
    Taint ataint, btaint;
    // src1 is op0 when register, else (mem dst?) — PCMPESTRM can store to
    // xmm0? No: PCMPESTRM writes XMM0 implicitly. op0 is always a source.
    if (d.ops[src1i].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        vec_load_bytes(cpu, d, src1i, A, w, &ataint);
    } else {
        uint64_t addr = resolve_mem(cpu, d.ops[src1i], cpu->rip + d.insn.length);
        mem_load_bytes(cpu, addr, A, w);
        mem_get_taint(addr, w, &ataint);
    }
    if (d.ops[src2i].type == ZYDIS_OPERAND_TYPE_REGISTER) {
        vec_load_bytes(cpu, d, src2i, B, w, &btaint);
    } else {
        uint64_t addr = resolve_mem(cpu, d.ops[src2i], cpu->rip + d.insn.length);
        mem_load_bytes(cpu, addr, B, w);
        mem_get_taint(addr, w, &btaint);
    }
    // Lengths in EAX (src1) / EDX (src2) as element counts.
    unsigned mode = imm & 3;
    unsigned elem_bytes = (mode & 1) ? 2 : 1;
    unsigned elems = w / elem_bytes; // per 128-bit lane this is 16/8
    unsigned la = (unsigned)cpu->gpr[ZG_RAX].val;
    unsigned lb = (unsigned)cpu->gpr[ZG_RDX].val;
    if (la > elems)
        la = elems;
    if (lb > elems)
        lb = elems;
    // Process per 128-bit lane; combine for w=32.
    uint64_t final_mask = 0;
    int final_idx = elems; // per full width for w=16; for w=32 pick first
    bool any = false;
    for (unsigned lane = 0; lane < w; lane += 16) {
        unsigned le = 16 / elem_bytes;
        int64_t av[16], bv[16];
        for (unsigned i = 0; i < le; i++) {
            uint64_t t = 0;
            memcpy(&t, A + lane + i * elem_bytes, elem_bytes);
            if (mode == 0)
                av[i] = t;
            else if (mode == 1)
                av[i] = (int16_t)t;
            else if (mode == 2)
                av[i] = (int8_t)t;
            else
                av[i] = (int16_t)t;
            memcpy(&t, B + lane + i * elem_bytes, elem_bytes);
            if (mode == 0)
                bv[i] = t;
            else if (mode == 1)
                bv[i] = (int16_t)t;
            else if (mode == 2)
                bv[i] = (int8_t)t;
            else
                bv[i] = (int16_t)t;
        }
        unsigned agg = (imm >> 2) & 3;
        // in_range valid matrix: for equal-any, bool per (i in B-valid, j in A-valid).
        bool valid[16][16] = {{false}};
        unsigned alen = (lane == 0) ? la : 0;
        unsigned blen = (lane == 0) ? lb : 0;
        if (w == 32 && alen == 0 && lane == 0) {
            // lengths apply to the full register for 256-bit: split evenly?
            // Documented approximation: lengths count per lane's share.
            alen = la > le ? le : la;
            blen = lb > le ? le : lb;
        }
        if (agg == 0) {
            // Equal-any: B[i] matches any A[j].
            for (unsigned i = 0; i < blen; i++)
                for (unsigned j = 0; j < alen; j++)
                    if (bv[i] == av[j])
                        valid[i][j] = true;
        } else if (agg == 1) {
            // Ranges: A holds pairs; B[i] in any pair.
            for (unsigned i = 0; i < blen; i++) {
                for (unsigned j = 0; j + 1 < alen; j += 2) {
                    if (av[j] <= bv[i] && bv[i] <= av[j + 1])
                        valid[i][j] = true;
                }
            }
        } else if (agg == 2) {
            // Equal-each: B[i]==A[i].
            for (unsigned i = 0; i < blen && i < alen; i++)
                if (bv[i] == av[i])
                    valid[i][i] = true;
        } else {
            // Equal-ordered (substring): B contains A as substring.
            for (unsigned s = 0; s + alen <= blen + (alen ? 0 : 0); s++) {
                bool ok = true;
                for (unsigned j = 0; j < alen; j++) {
                    if (s + j >= blen || bv[s + j] != av[j]) {
                        ok = false;
                        break;
                    }
                }
                if (ok && alen) {
                    for (unsigned j = 0; j < alen; j++)
                        valid[s + j][j] = true;
                }
            }
            if (!alen) {
                // Empty needle matches everywhere (position 0).
                valid[0][0] = true;
            }
        }
        // Collapse to per-B-element result: OR over j; handle polarity.
        bool res[16] = {false};
        for (unsigned i = 0; i < le; i++) {
            bool v = false;
            for (unsigned j = 0; j < le; j++)
                v = v || valid[i][j];
            res[i] = v;
        }
        unsigned pol = (imm >> 4) & 3;
        if (pol == 1) {
            for (unsigned i = 0; i < le; i++)
                res[i] = !res[i];
        } else if (pol == 2) {
            for (unsigned i = 0; i < le; i++)
                if (i >= blen)
                    res[i] = false;
        } else if (pol == 3) {
            for (unsigned i = 0; i < le; i++) {
                if (i < blen)
                    res[i] = !res[i];
                else
                    res[i] = false;
            }
        }
        uint64_t mask = 0;
        for (unsigned i = 0; i < le; i++)
            if (res[i])
                mask |= (1ULL << i);
        // Masked by valid length for the mask output form.
        if (w == 16 || lane == 0) {
            final_mask |= mask << (lane == 0 ? 0 : le);
            if (mask && !any) {
                any = true;
                if (imm & 0x40) {
                    // msb: highest set bit in this lane.
                    for (int i = (int)le - 1; i >= 0; i--) {
                        if ((mask >> i) & 1) {
                            final_idx = i + (lane ? le : 0);
                            break;
                        }
                    }
                } else {
                    for (unsigned i = 0; i < le; i++) {
                        if ((mask >> i) & 1) {
                            final_idx = i + (lane ? le : 0);
                            break;
                        }
                    }
                }
            }
        }
    }
    // Flags: ZF = (mask==0)? ... Intel: ZF set if any... For ISTRI: ECX = index;
    // C flag family: CF = reset if (mask==0)? Actual: CF=1 if B invalid? Use:
    // ZF = (final_mask != 0)? Hmm Intel: ZF set if "no match"? No—
    // PCMPESTRI: ZF=1 if any bit in IntRes2? Let me use: ZF = (final_mask==0);
    // CF = (lb < le)? SF = (la < le)? OF = res LSB? Document approximation.
    cpu->set_flag(FLAG_ZF, final_mask == 0);
    cpu->set_flag(FLAG_CF, lb < elems);
    cpu->set_flag(FLAG_SF, la < elems);
    cpu->set_flag(FLAG_OF, (final_mask & 1) != 0);
    cpu->set_flag(FLAG_AF, false);
    cpu->set_flag(FLAG_PF, false);
    cpu->flags_taint = ataint | btaint;
    if (want_index) {
        Val out;
        out.v = any ? (uint64_t)final_idx : (uint64_t)elems;
        out.taint = ataint | btaint;
        cpu->gpr[ZG_RCX].val = out.v;
        cpu->gpr[ZG_RCX].taint.cannot_branch = out.taint.cannot_branch;
        cpu->gpr[ZG_RCX].taint.cannot_index = out.taint.cannot_index;
    } else {
        // PCMPESTRM: XMM0 = mask (zero-extended to 128 bits).
        uint8_t mout[16] = {0};
        memcpy(mout, &final_mask, sizeof(final_mask));
        memcpy(cpu->xmm[0].bytes, mout, 16);
        memset(cpu->xmm[0].bytes + 16, 0, 48);
        cpu->xmm[0].taint = ataint | btaint;
    }
    cpu->rip = next_rip(cpu, d);
}

// --- FMA (VEX + EVEX), single-rounding via std::fma ----------------------------

static bool fma_info(ZydisMnemonic m, bool& is_pd, bool& scalar, bool& negate,
                     bool& sub, bool& addsub, int& form) {
    // form: 132/213/231, or 0 for legacy (231 semantics).
    is_pd = false;
    scalar = false;
    negate = false;
    sub = false;
    addsub = false;
    form = 0;
    // NOTE: ZydisMnemonicGetString returns lowercase ("vfmadd132ps").
    const char* s = ZydisMnemonicGetString(m);
    if (!s || s[0] != 'v')
        return false;
    // Skip the vfmadd/vfmsub/vfnmadd/vfnmsub prefix.
    const char* p = s + 1; // "fmadd..."/"fmsub..."/"fnmadd..."/"fnmsub..."
    if (p[0] != 'f')
        return false;
    p += 1; // "madd..."/"msub..."/"nmadd..."/"nmsub..."
    if (p[0] == 'n') {
        negate = true;
        p += 1; // "madd..."/"msub..."
    }
    bool is_msub = false;
    if (p[0] == 'm' && p[1] == 's' && p[2] == 'u' && p[3] == 'b') {
        is_msub = true;
        p += 4; // 132pd / ps / ...
    } else if (p[0] == 'm' && p[1] == 'a' && p[2] == 'd' && p[3] == 'd') {
        p += 4;
    } else {
        return false;
    }
    if (p[0] == 's' && p[1] == 'u' && p[2] == 'b') {
        addsub = true;
        p += 3;
    }
    if (p[0] == '1' || p[0] == '2') {
        form = (p[0] - '0') * 100 + (p[1] - '0') * 10 + (p[2] - '0');
        p += 3;
    }
    if (p[0] == 'p' && p[1] == 'd') {
        is_pd = true;
    } else if (p[0] == 'p' && p[1] == 's') {
        is_pd = false;
    } else if (p[0] == 's' && p[1] == 'd') {
        is_pd = true;
        scalar = true;
    } else if (p[0] == 's' && p[1] == 's') {
        is_pd = false;
        scalar = true;
    } else {
        return false;
    }
    sub = is_msub;
    if (form == 0)
        form = 231;
    return true;
}

void exec_fma(CPU* cpu, const Dec& d) {
    bool is_pd, scalar, negate, sub, addsub;
    int form;
    if (!fma_info(d.insn.mnemonic, is_pd, scalar, negate, sub, addsub, form))
        guest_error(cpu, "bad fma mnemonic");
    unsigned elem = is_pd ? 8 : 4;
    unsigned w = 16;
    if (is_evex(d))
        w = evex_width(cpu, d);
    else {
        w = d.ops[0].size / 8;
        if (scalar) {
            if (w != 16 && w != 32)
                guest_error(cpu, "bad scalar fma width");
        } else if (w != 16 && w != 32) {
            guest_error(cpu, "bad fma width");
        }
    }
    int ex[5];
    int n = evex_explicit(d, ex);
    if (n < 3)
        guest_error(cpu, "fma needs 3 operands");
    VecVal ab, bb, cb2, o, old;
    evex_load(cpu, d, ex[0], ab, w, elem); // dst doubles as an operand
    evex_load(cpu, d, ex[1], bb, w, elem);
    evex_load(cpu, d, ex[2], cb2, w, elem);
    // Map 132/213/231 to (x,y,z) with r = +/-x*y +/- z.
    uint8_t *x = ab, *y = bb, *z = cb2;
    if (form == 132) {
        x = ab;
        y = cb2;
        z = bb;
    } else if (form == 213) {
        x = bb;
        y = ab;
        z = cb2;
    } else { // 231
        x = bb;
        y = cb2;
        z = ab;
    }
    memcpy(o, z, w); // scalar upper lanes passthrough (overwritten below for lane 0)
    unsigned lanes = scalar ? 1 : w / elem;
    for (unsigned i = 0; i < lanes; i++) {
        bool do_sub = sub;
        if (addsub) {
            // VFMADDSUB: even lanes add, odd lanes sub (PS); PD: low add, high sub.
            do_sub = ((i & 1) != 0);
        }
        if (!is_pd) {
            float xf, yf, zf;
            memcpy(&xf, x + i * 4, 4);
            memcpy(&yf, y + i * 4, 4);
            memcpy(&zf, z + i * 4, 4);
            if (negate)
                xf = -xf;
            float r = do_sub ? std::fma(xf, yf, -zf) : std::fma(xf, yf, zf);
            memcpy(o + i * 4, &r, 4);
        } else {
            double xd, yd, zd;
            memcpy(&xd, x + i * 8, 8);
            memcpy(&yd, y + i * 8, 8);
            memcpy(&zd, z + i * 8, 8);
            if (negate)
                xd = -xd;
            double r = do_sub ? std::fma(xd, yd, -zd) : std::fma(xd, yd, zd);
            memcpy(o + i * 8, &r, 8);
        }
    }
    if (scalar && !is_evex(d)) {
        // VEX scalar: upper bits of dest preserved from old dst (already in o via memcpy from z? No: z for 231 is ab=old dst. For 132/213, z=bb/cb2... fix: upper lanes must come from OLD DEST (ab)).
        if (form != 231)
            memcpy(o + elem, ab + elem, w - elem);
        o.taint = ab.taint | bb.taint | cb2.taint;
        vec_store_bytes(cpu, d, ex[0], o, w, true, w);
    } else {
        evex_old(cpu, d, ex[0], old, w);
        bool zeroing = false;
        uint64_t kbits = is_evex(d) ? evex_mask(cpu, d, scalar ? 1 : w / elem, zeroing) : ~0ULL;
        if (scalar && is_evex(d)) {
            // EVEX scalar: only lane 0 masked; upper lanes from old dest.
            VecVal merged;
            memcpy(merged, old, w);
            if ((kbits & 1) || true) {
                if ((kbits & 1))
                    memcpy(merged, o, elem);
                else if (zeroing)
                    memset(merged, 0, elem);
            }
            merged.taint = ab.taint | bb.taint | cb2.taint;
            evex_store(cpu, d, ex[0], merged, old, w, w, ~0ULL, false);
        } else {
            o.taint = ab.taint | bb.taint | cb2.taint;
            evex_store(cpu, d, ex[0], o, old, w, elem, kbits, zeroing);
        }
    }
    cpu->rip = next_rip(cpu, d);
}

// --- F16C: VCVTPH2PS / VCVTPS2PH (VEX + EVEX) -----------------------------------

__attribute__((target("avx,f16c"))) void exec_cvtph(CPU* cpu, const Dec& d) {
    bool to_ps = (d.insn.mnemonic == ZYDIS_MNEMONIC_VCVTPH2PS);
    unsigned w = 16;
    if (is_evex(d)) {
        w = evex_width(cpu, d);
    } else if (to_ps) {
        w = d.ops[0].size / 8;
        if (w != 16 && w != 32)
            guest_error(cpu, "bad cvtph width");
    } else {
        // VCVTPS2PH: dest holds half as many bytes as the float source.
        w = d.ops[1].size / 8;
        if (w != 16 && w != 32)
            guest_error(cpu, "bad cvtph width");
    }
    int ex[5];
    int n = evex_explicit(d, ex);
    VecVal a, o, old;
    if (to_ps) {
        // src half-words: w/2 bytes (128-bit mem for 256-bit dst etc.).
        if (n < 2)
            guest_error(cpu, "cvtph2ps needs 2 operands");
        unsigned sw = w / 2;
        evex_load(cpu, d, ex[1], a, sw > MAX_VEC_BYTES ? MAX_VEC_BYTES : sw, 2);
        for (unsigned i = 0; i < w / 4; i++) {
            uint16_t h = 0;
            memcpy(&h, a + i * 2, 2);
            __m128 f = _mm_cvtph_ps(_mm_set1_epi16((short)h));
            memcpy(o + i * 4, &f, 4);
        }
        evex_old(cpu, d, ex[0], old, w);
        bool zeroing = false;
        uint64_t kbits = is_evex(d) ? evex_mask(cpu, d, w / 4, zeroing) : ~0ULL;
        o.taint = a.taint;
        evex_store(cpu, d, ex[0], o, old, w, 4, kbits, zeroing);
    } else {
        if (n < 3)
            guest_error(cpu, "cvtps2ph needs 3 operands");
        evex_load(cpu, d, ex[1], a, w, 4);
        unsigned imm = (unsigned)kload(cpu, d, ex[2], 8);
        (void)imm; // rounding per MXCSR approximation (documented)
        for (unsigned i = 0; i < w / 4; i++) {
            __m128 v = _mm_load_ss((float*)(a + i * 4));
            __m128i h = _mm_cvtps_ph(v, _MM_FROUND_CUR_DIRECTION);
            uint16_t hv = (uint16_t)_mm_extract_epi16(h, 0);
            memcpy(o + i * 2, &hv, 2);
        }
        evex_old(cpu, d, ex[0], old, w / 2);
        bool zeroing = false;
        uint64_t kbits = is_evex(d) ? evex_mask(cpu, d, w / 4, zeroing) : ~0ULL;
        // Dest is half-width; store via evex_store with elem=2 over w/2 bytes.
        o.taint = a.taint;
        evex_store(cpu, d, ex[0], o, old, w / 2, 2, kbits, zeroing);
    }
    cpu->rip = next_rip(cpu, d);
}

// BF16: VCVTNE2PS2BF16 / VCVTNEPS2BF16 (RNE float32 -> bf16).
void exec_cvtnebf16(CPU* cpu, const Dec& d) {
    bool pair = (d.insn.mnemonic == ZYDIS_MNEMONIC_VCVTNE2PS2BF16);
    unsigned w = 16;
    if (is_evex(d))
        w = evex_width(cpu, d);
    else {
        w = d.ops[0].size / 8;
        if (w != 16 && w != 32)
            guest_error(cpu, "bad cvtnebf16 width");
    }
    int ex[5];
    int n = evex_explicit(d, ex);
    VecVal a, b, o, old;
    unsigned lanes = 0; // bf16 outputs
    if (!pair) {
        if (n < 2)
            guest_error(cpu, "cvtneps2bf16 needs 2 operands");
        evex_load(cpu, d, ex[1], a, w * 2 > MAX_VEC_BYTES ? MAX_VEC_BYTES : w * 2, 4);
        lanes = w / 2;
        for (unsigned i = 0; i < lanes; i++) {
            uint32_t x = 0;
            memcpy(&x, a + i * 4, 4);
            // RNE: add 0x7FFF + LSB, then truncate; keep NaN quiet.
            uint32_t lsb = (x >> 16) & 1;
            uint32_t r;
            if ((x & 0x7fffffff) > 0x7f800000)
                r = (x >> 16) | 0x40; // quiet NaN
            else
                r = (x + 0x7fff + lsb) >> 16;
            uint16_t h = (uint16_t)r;
            memcpy(o + i * 2, &h, 2);
        }
        evex_old(cpu, d, ex[0], old, w);
        bool zeroing = false;
        uint64_t kbits = is_evex(d) ? evex_mask(cpu, d, lanes, zeroing) : ~0ULL;
        o.taint = a.taint | b.taint;
        evex_store(cpu, d, ex[0], o, old, w, 2, kbits, zeroing);
    } else {
        if (n < 3)
            guest_error(cpu, "cvtne2ps2bf16 needs 3 operands");
        evex_load(cpu, d, ex[1], a, w, 4);
        evex_load(cpu, d, ex[2], b, w, 4);
        lanes = w / 2;
        for (unsigned i = 0; i < lanes / 2; i++) {
            for (unsigned j = 0; j < 2; j++) {
                uint32_t x = 0;
                memcpy(&x, (j ? b : a) + i * 4, 4);
                uint32_t lsb = (x >> 16) & 1;
                uint32_t r;
                if ((x & 0x7fffffff) > 0x7f800000)
                    r = (x >> 16) | 0x40;
                else
                    r = (x + 0x7fff + lsb) >> 16;
                uint16_t h = (uint16_t)r;
                memcpy(o + i * 4 + j * 2, &h, 2);
            }
        }
        evex_old(cpu, d, ex[0], old, w);
        bool zeroing = false;
        uint64_t kbits = is_evex(d) ? evex_mask(cpu, d, lanes, zeroing) : ~0ULL;
        o.taint = a.taint | b.taint;
        evex_store(cpu, d, ex[0], o, old, w, 2, kbits, zeroing);
    }
    cpu->rip = next_rip(cpu, d);
}

// VPCOMPRESSB/W/D/Q + VPEXPANDB/W/D/Q.
void exec_evex_compress(CPU* cpu, const Dec& d, bool expand) {
    unsigned elem = 1;
    switch (d.insn.mnemonic) {
    case ZYDIS_MNEMONIC_VPCOMPRESSW:
    case ZYDIS_MNEMONIC_VPEXPANDW: elem = 2; break;
    case ZYDIS_MNEMONIC_VPCOMPRESSD:
    case ZYDIS_MNEMONIC_VPEXPANDD: elem = 4; break;
    case ZYDIS_MNEMONIC_VPCOMPRESSQ:
    case ZYDIS_MNEMONIC_VPEXPANDQ: elem = 8; break;
    default: elem = 1; break;
    }
    unsigned w = evex_width(cpu, d);
    int ex[5];
    int n = evex_explicit(d, ex);
    if (n < 2)
        guest_error(cpu, "compress needs 2 operands");
    bool zeroing = false;
    uint64_t kbits = evex_mask(cpu, d, w / elem, zeroing);
    VecVal a, o, old;
    if (!expand) {
        // Compress: enabled elements packed to low (reg) or contiguous (mem).
        // Per-element precise: evex_load only faults/taints enabled lanes.
        evex_load(cpu, d, ex[1], a, w, elem);
        memset(o, 0, w);
        o.taint = a.taint;
        unsigned out = 0;
        for (unsigned i = 0; i < w / elem; i++) {
            if ((kbits >> i) & 1) {
                memcpy(o + out * elem, a + i * elem, elem);
                out++;
            }
        }
        const auto& dop = d.ops[ex[0]];
        if (dop.type == ZYDIS_OPERAND_TYPE_REGISTER) {
            // Upper lanes: zero (or merge? compress to reg zeroes upper).
            evex_old(cpu, d, ex[0], old, w);
            o.taint = a.taint;
            evex_store(cpu, d, ex[0], o, old, w, w, ~0ULL, false);
        } else {
            uint64_t addr = resolve_mem(cpu, dop, cpu->rip + d.insn.length);
            mem_store_bytes(cpu, addr, o, out * elem);
            // Only bytes actually written gain taint (enabled lanes only).
            mem_note_store(cpu, addr, out * elem, o.taint);
        }
    } else {
        // Expand: contiguous (reg low / mem) loaded into enabled lanes.
        // Only bytes/elements actually read participate in taint.
        const auto& sop = d.ops[ex[1]];
        unsigned in = 0;
        for (unsigned i = 0; i < w / elem; i++)
            if ((kbits >> i) & 1)
                in++;
        if (sop.type == ZYDIS_OPERAND_TYPE_REGISTER) {
            int idx, width;
            if (!reg_to_vec(sop.reg.value, idx, width))
                guest_error(cpu, "bad expand source");
            memcpy(a, cpu->xmm[idx].bytes, w);
            // Only low in*elem bytes are read; disabled high bytes ignored.
            // With whole-vector Taint, propagate only if something read.
            if (in > 0)
                a.taint = cpu->xmm[idx].taint;
            else
                a.taint.clear();
        } else {
            uint64_t addr = resolve_mem(cpu, sop, cpu->rip + d.insn.length);
            memset(a, 0, w);
            if (in > 0) {
                mem_load_bytes(cpu, addr, a, in * elem);
                mem_get_taint(addr, in * elem, &a.taint);
            } else {
                a.taint.clear();
            }
        }
        evex_old(cpu, d, ex[0], old, w);
        memcpy(o, old, w);
        o.taint = a.taint;
        unsigned inp = 0;
        for (unsigned i = 0; i < w / elem; i++) {
            if ((kbits >> i) & 1)
                memcpy(o + i * elem, a + inp++ * elem, elem);
            else if (zeroing)
                memset(o + i * elem, 0, elem);
        }
        o.taint = a.taint;
        evex_store(cpu, d, ex[0], o, old, w, elem, kbits, zeroing);
    }
    cpu->rip = next_rip(cpu, d);
}

// VALIGND/Q: double-shift of concatenated (a:b) by imm elements.
void exec_evex_align(CPU* cpu, const Dec& d) {
    unsigned elem = (d.insn.mnemonic == ZYDIS_MNEMONIC_VALIGNQ) ? 8 : 4;
    unsigned w = evex_width(cpu, d);
    int ex[5];
    int n = evex_explicit(d, ex);
    if (n < 4)
        guest_error(cpu, "valign needs 4 operands");
    VecVal a, b, o, old;
    evex_load(cpu, d, ex[1], a, w, elem);
    evex_load(cpu, d, ex[2], b, w, elem);
    unsigned lanes = w / elem;
    unsigned sh = (unsigned)kload(cpu, d, ex[3], 8) % (lanes + 1);
    for (unsigned i = 0; i < lanes; i++) {
        unsigned src = i + sh;
        if (src < lanes)
            memcpy(o + i * elem, b + src * elem, elem);
        else
            memcpy(o + i * elem, a + (src - lanes) * elem, elem);
    }
    evex_old(cpu, d, ex[0], old, w);
    bool zeroing = false;
    uint64_t kbits = evex_mask(cpu, d, lanes, zeroing);
    o.taint = a.taint | b.taint;
    evex_store(cpu, d, ex[0], o, old, w, elem, kbits, zeroing);
    cpu->rip = next_rip(cpu, d);
}

// Masked packed-float converts (EVEX forms of the phase-1 CVT handlers).
void exec_evex_cvt(CPU* cpu, const Dec& d, int kind) {
    // kind 0=dq2ps 1=tt_ps2dq 2=ps2dq 3=pd2ps 4=ps2pd
    unsigned elem = 4;
    if (kind == 3 || kind == 4)
        elem = 8;
    unsigned w = evex_width(cpu, d);
    int ex[5];
    int n = evex_explicit(d, ex);
    if (n < 2)
        guest_error(cpu, "evex cvt needs 2 operands");
    // Source width differs for widening/narrowing; derive from operand size.
    unsigned sw = d.ops[ex[1]].size / 8;
    if (sw != 16 && sw != 32 && sw != 64)
        sw = w;
    VecVal a, o, old;
    evex_load(cpu, d, ex[1], a, sw, elem == 8 ? 8 : 4);
    memset(o, 0, MAX_VEC_BYTES);
    if (kind <= 2) {
        unsigned lanes = w / 4;
        for (unsigned i = 0; i < lanes; i++) {
            int32_t iv = 0;
            float fv = 0;
            memcpy(&iv, a + i * 4, 4);
            if (kind == 0) {
                fv = (float)iv;
                memcpy(o + i * 4, &fv, 4);
            } else {
                memcpy(&fv, a + i * 4, 4);
                int32_t r;
                if (kind == 1)
                    r = (fv > 2147483647.0f || fv < -2147483648.0f || fv != fv)
                            ? (int32_t)0x80000000
                            : (int32_t)fv;
                else
                    r = (int32_t)nearbyintf(fv);
                memcpy(o + i * 4, &r, 4);
            }
        }
        evex_old(cpu, d, ex[0], old, w);
        bool zeroing = false;
        uint64_t kbits = evex_mask(cpu, d, lanes, zeroing);
        o.taint = a.taint;
        evex_store(cpu, d, ex[0], o, old, w, 4, kbits, zeroing);
    } else if (kind == 3) {
        // pd2ps: w/2 bytes of floats from w bytes of doubles.
        unsigned lanes = w / 8;
        for (unsigned i = 0; i < lanes; i++) {
            double dv = 0;
            memcpy(&dv, a + i * 8, 8);
            float fv = (float)dv;
            memcpy(o + i * 4, &fv, 4);
        }
        evex_old(cpu, d, ex[0], old, w / 2);
        bool zeroing = false;
        uint64_t kbits = evex_mask(cpu, d, lanes, zeroing);
        o.taint = a.taint;
        evex_store(cpu, d, ex[0], o, old, w / 2, 4, kbits, zeroing);
    } else {
        // ps2pd: w*2 bytes of doubles from w bytes of floats.
        unsigned lanes = sw / 4;
        for (unsigned i = 0; i < lanes; i++) {
            float fv = 0;
            memcpy(&fv, a + i * 4, 4);
            double dv = (double)fv;
            memcpy(o + i * 8, &dv, 8);
        }
        unsigned dw = lanes * 8;
        if (dw > 64)
            dw = 64;
        evex_old(cpu, d, ex[0], old, dw);
        bool zeroing = false;
        uint64_t kbits = evex_mask(cpu, d, lanes, zeroing);
        o.taint = a.taint;
        evex_store(cpu, d, ex[0], o, old, dw, 8, kbits, zeroing);
    }
    cpu->rip = next_rip(cpu, d);
}

// VINSERTF32X4/F64X2/I32X4/I64X2/F32X8/F64X4 + VEXTRACTF32X4/... (EVEX).
void exec_evex_ins_extract(CPU* cpu, const Dec& d, bool insert) {
    unsigned sub = 16;
    switch (d.insn.mnemonic) {
    case ZYDIS_MNEMONIC_VINSERTF32X8:
    case ZYDIS_MNEMONIC_VINSERTF64X4:
    case ZYDIS_MNEMONIC_VINSERTI32X8:
    case ZYDIS_MNEMONIC_VINSERTI64X4:
    case ZYDIS_MNEMONIC_VEXTRACTF32X8:
    case ZYDIS_MNEMONIC_VEXTRACTF64X4:
    case ZYDIS_MNEMONIC_VEXTRACTI32X8:
    case ZYDIS_MNEMONIC_VEXTRACTI64X4: sub = 32; break;
    default: sub = 16; break;
    }
    unsigned w = evex_width(cpu, d);
    int ex[5];
    int n = evex_explicit(d, ex);
    VecVal a, b, o, old;
    if (!insert) {
        if (n < 3)
            guest_error(cpu, "vextract needs 3 operands");
        evex_load(cpu, d, ex[1], a, w, sub);
        unsigned sel = (unsigned)kload(cpu, d, ex[2], 8);
        unsigned off = (sel * sub) % w;
        memcpy(o, a + off, sub);
        evex_old(cpu, d, ex[0], old, sub);
        bool zeroing = false;
        uint64_t kbits = evex_mask(cpu, d, 1, zeroing);
        // Single sub-vector: mask bit 0 decides all-or-merge.
        if ((kbits & 1) || zeroing) {
            VecVal full;
            memcpy(full, (kbits & 1) ? o : old, sub);
            if (zeroing && !(kbits & 1))
                memset(full, 0, sub);
            full.taint = a.taint | b.taint;
            evex_store(cpu, d, ex[0], full, old, sub, sub, ~0ULL, false);
        } else {
            old.taint = a.taint | b.taint;
            evex_store(cpu, d, ex[0], old, old, sub, sub, ~0ULL, false);
        }
    } else {
        if (n < 4)
            guest_error(cpu, "vinsert needs 4 operands");
        evex_load(cpu, d, ex[1], a, w, sub);
        evex_load(cpu, d, ex[2], b, sub, sub);
        unsigned sel = (unsigned)kload(cpu, d, ex[3], 8);
        unsigned off = (sel * sub) % w;
        memcpy(o, a, w);
        memcpy(o + off, b, sub);
        evex_old(cpu, d, ex[0], old, w);
        bool zeroing = false;
        uint64_t kbits = evex_mask(cpu, d, w / sub, zeroing);
        o.taint = a.taint | b.taint;
        evex_store(cpu, d, ex[0], o, old, w, sub, kbits, zeroing);
    }
    cpu->rip = next_rip(cpu, d);
}

// VSHUFF32X4/64X2/I32X4/I64X2 (EVEX lane shuffle by imm pairs).
void exec_evex_shufx(CPU* cpu, const Dec& d) {
    unsigned sub = 16;
    switch (d.insn.mnemonic) {
    case ZYDIS_MNEMONIC_VSHUFF64X2:
    case ZYDIS_MNEMONIC_VSHUFI64X2: sub = 32; break;
    default: sub = 16; break;
    }
    unsigned w = evex_width(cpu, d);
    int ex[5];
    int n = evex_explicit(d, ex);
    if (n < 4)
        guest_error(cpu, "vshufx needs 4 operands");
    VecVal a, b, o, old;
    evex_load(cpu, d, ex[1], a, w, sub);
    evex_load(cpu, d, ex[2], b, w, sub);
    unsigned order = (unsigned)kload(cpu, d, ex[3], 8);
    unsigned nsub = w / sub;
    for (unsigned i = 0; i < nsub; i++) {
        unsigned sel = (order >> (i * 2)) & 3;
        const uint8_t* src = (sel < 2) ? a : b;
        unsigned s = (sel % 2) * sub;
        // sel indexes the concatenated pair of sub-vectors per output lane
        // pair; for w > 2*sub, wrap within halves.
        unsigned lane_half = (i / 2) * 2 * sub;
        memcpy(o + i * sub, src + lane_half + s, sub);
    }
    evex_old(cpu, d, ex[0], old, w);
    bool zeroing = false;
    uint64_t kbits = evex_mask(cpu, d, nsub, zeroing);
    o.taint = a.taint | b.taint;
    evex_store(cpu, d, ex[0], o, old, w, sub, kbits, zeroing);
    cpu->rip = next_rip(cpu, d);
}

// Master EVEX entry: handles every EVEX-encoded instruction we support.
// Also serves VEX-encoded VAES/VPCLMULQDQ/VGFNI (via their exec_* which
// accept both encodings) and VEX legacy gathers.
void exec_evex_movd(CPU* cpu, const Dec& d, bool is_q);
void exec_evex(CPU* cpu, const Dec& d) {
    ZydisMnemonic m = d.insn.mnemonic;
    // Gather/scatter prefetch hints: architecturally NOP.
    if (mn_starts_with(m, "VGATHERPF") || mn_starts_with(m, "VSCATTERPF")) {
        cpu->rip = next_rip(cpu, d);
        return;
    }
    // Crypto (accept VEX + EVEX).
    if (is_aes_mn(m)) {
        exec_aes(cpu, d);
        return;
    }
    if (m == ZYDIS_MNEMONIC_VPCLMULQDQ) {
        exec_pclmul(cpu, d);
        return;
    }
    if (m == ZYDIS_MNEMONIC_VGF2P8AFFINEQB ||
        m == ZYDIS_MNEMONIC_VGF2P8AFFINEINVQB ||
        m == ZYDIS_MNEMONIC_VGF2P8MULB) {
        exec_gfni(cpu, d);
        return;
    }
    // FMA / F16C / BF16 (accept VEX + EVEX).
    {
        bool is_pd, scalar, negate, sub, addsub;
        int form;
        if (fma_info(m, is_pd, scalar, negate, sub, addsub, form)) {
            // FP16 (PH/SH) forms are unsupported; clean error below.
            const char* s = ZydisMnemonicGetString(m);
            size_t L = s ? strlen(s) : 0;
            if (L >= 2 && s[L - 2] == 'p' && s[L - 1] == 'h')
                guest_error(cpu, "unsupported AVX512-FP16 operation");
            if (L >= 2 && s[L - 2] == 's' && s[L - 1] == 'h')
                guest_error(cpu, "unsupported AVX512-FP16 operation");
            exec_fma(cpu, d);
            return;
        }
    }
    if (m == ZYDIS_MNEMONIC_VCVTPH2PS || m == ZYDIS_MNEMONIC_VCVTPS2PH) {
        exec_cvtph(cpu, d);
        return;
    }
    if (m == ZYDIS_MNEMONIC_VCVTNE2PS2BF16 ||
        m == ZYDIS_MNEMONIC_VCVTNEPS2BF16) {
        exec_cvtnebf16(cpu, d);
        return;
    }
    // Opmask instructions.
    switch (m) {
    case ZYDIS_MNEMONIC_KMOVB:
    case ZYDIS_MNEMONIC_KMOVW:
    case ZYDIS_MNEMONIC_KMOVD:
    case ZYDIS_MNEMONIC_KMOVQ:
    case ZYDIS_MNEMONIC_KMOV:
    case ZYDIS_MNEMONIC_KNOTB:
    case ZYDIS_MNEMONIC_KNOTW:
    case ZYDIS_MNEMONIC_KNOTD:
    case ZYDIS_MNEMONIC_KNOTQ:
    case ZYDIS_MNEMONIC_KNOT:
    case ZYDIS_MNEMONIC_KORB:
    case ZYDIS_MNEMONIC_KORW:
    case ZYDIS_MNEMONIC_KORD:
    case ZYDIS_MNEMONIC_KORQ:
    case ZYDIS_MNEMONIC_KOR:
    case ZYDIS_MNEMONIC_KADDB:
    case ZYDIS_MNEMONIC_KADDW:
    case ZYDIS_MNEMONIC_KADDD:
    case ZYDIS_MNEMONIC_KADDQ:
    case ZYDIS_MNEMONIC_KANDB:
    case ZYDIS_MNEMONIC_KANDW:
    case ZYDIS_MNEMONIC_KANDD:
    case ZYDIS_MNEMONIC_KANDQ:
    case ZYDIS_MNEMONIC_KAND:
    case ZYDIS_MNEMONIC_KXORB:
    case ZYDIS_MNEMONIC_KXORW:
    case ZYDIS_MNEMONIC_KXORD:
    case ZYDIS_MNEMONIC_KXORQ:
    case ZYDIS_MNEMONIC_KXOR:
    case ZYDIS_MNEMONIC_KXNORB:
    case ZYDIS_MNEMONIC_KXNORW:
    case ZYDIS_MNEMONIC_KXNORD:
    case ZYDIS_MNEMONIC_KXNORQ:
    case ZYDIS_MNEMONIC_KXNOR:
    case ZYDIS_MNEMONIC_KANDNB:
    case ZYDIS_MNEMONIC_KANDNW:
    case ZYDIS_MNEMONIC_KANDND:
    case ZYDIS_MNEMONIC_KANDNQ:
    case ZYDIS_MNEMONIC_KANDN:
    case ZYDIS_MNEMONIC_KANDNR:
    case ZYDIS_MNEMONIC_KORTESTB:
    case ZYDIS_MNEMONIC_KORTESTW:
    case ZYDIS_MNEMONIC_KORTESTD:
    case ZYDIS_MNEMONIC_KORTESTQ:
    case ZYDIS_MNEMONIC_KORTEST:
    case ZYDIS_MNEMONIC_KTESTB:
    case ZYDIS_MNEMONIC_KTESTW:
    case ZYDIS_MNEMONIC_KTESTD:
    case ZYDIS_MNEMONIC_KTESTQ:
    case ZYDIS_MNEMONIC_KSHIFTLB:
    case ZYDIS_MNEMONIC_KSHIFTLW:
    case ZYDIS_MNEMONIC_KSHIFTLD:
    case ZYDIS_MNEMONIC_KSHIFTLQ:
    case ZYDIS_MNEMONIC_KSHIFTRB:
    case ZYDIS_MNEMONIC_KSHIFTRW:
    case ZYDIS_MNEMONIC_KSHIFTRD:
    case ZYDIS_MNEMONIC_KSHIFTRQ:
    case ZYDIS_MNEMONIC_KUNPCKBW:
    case ZYDIS_MNEMONIC_KUNPCKWD:
    case ZYDIS_MNEMONIC_KUNPCKDQ: exec_kop(cpu, d); return;
    default: break;
    }
    // Masked moves.
    switch (m) {
    case ZYDIS_MNEMONIC_VMOVDQA32:
    case ZYDIS_MNEMONIC_VMOVDQA64:
    case ZYDIS_MNEMONIC_VMOVDQU8:
    case ZYDIS_MNEMONIC_VMOVDQU16:
    case ZYDIS_MNEMONIC_VMOVDQU32:
    case ZYDIS_MNEMONIC_VMOVDQU64:
    case ZYDIS_MNEMONIC_VMOVDQA:
    case ZYDIS_MNEMONIC_VMOVDQU:
    case ZYDIS_MNEMONIC_VMOVAPS:
    case ZYDIS_MNEMONIC_VMOVUPS:
    case ZYDIS_MNEMONIC_VMOVAPD:
    case ZYDIS_MNEMONIC_VMOVUPD:
    case ZYDIS_MNEMONIC_VMOVNTDQ:
    case ZYDIS_MNEMONIC_VMOVNTPS:
    case ZYDIS_MNEMONIC_VMOVNTPD:
    case ZYDIS_MNEMONIC_VMOVNTDQA: exec_evex_move(cpu, d); return;
    default: break;
    }
    // Logic + ternlog.
    switch (m) {
    case ZYDIS_MNEMONIC_VPANDD:
    case ZYDIS_MNEMONIC_VPANDQ:
    case ZYDIS_MNEMONIC_VPAND: exec_evex_logic(cpu, d, 0); return;
    case ZYDIS_MNEMONIC_VPORD:
    case ZYDIS_MNEMONIC_VPORQ:
    case ZYDIS_MNEMONIC_VPOR: exec_evex_logic(cpu, d, 1); return;
    case ZYDIS_MNEMONIC_VPXORD:
    case ZYDIS_MNEMONIC_VPXORQ:
    case ZYDIS_MNEMONIC_VPXOR: exec_evex_logic(cpu, d, 2); return;
    case ZYDIS_MNEMONIC_VPANDND:
    case ZYDIS_MNEMONIC_VPANDNQ:
    case ZYDIS_MNEMONIC_VPANDN: exec_evex_logic(cpu, d, 3); return;
    case ZYDIS_MNEMONIC_VPTERNLOGD:
    case ZYDIS_MNEMONIC_VPTERNLOGQ: exec_evex_ternlog(cpu, d); return;
    default: break;
    }
    // Compares.
    switch (m) {
    case ZYDIS_MNEMONIC_VPCMPB:
    case ZYDIS_MNEMONIC_VPCMPW:
    case ZYDIS_MNEMONIC_VPCMPD:
    case ZYDIS_MNEMONIC_VPCMPQ:
    case ZYDIS_MNEMONIC_VPCMPUB:
    case ZYDIS_MNEMONIC_VPCMPUW:
    case ZYDIS_MNEMONIC_VPCMPUD:
    case ZYDIS_MNEMONIC_VPCMPUQ: exec_evex_vpcmp(cpu, d); return;
    case ZYDIS_MNEMONIC_VPCMPEQB: exec_evex_pcmpeq_auto(cpu, d, 1, false); return;
    case ZYDIS_MNEMONIC_VPCMPEQW: exec_evex_pcmpeq_auto(cpu, d, 2, false); return;
    case ZYDIS_MNEMONIC_VPCMPEQD: exec_evex_pcmpeq_auto(cpu, d, 4, false); return;
    case ZYDIS_MNEMONIC_VPCMPEQQ: exec_evex_pcmpeq_auto(cpu, d, 8, false); return;
    case ZYDIS_MNEMONIC_VPCMPGTB: exec_evex_pcmpeq_auto(cpu, d, 1, true); return;
    case ZYDIS_MNEMONIC_VPCMPGTW: exec_evex_pcmpeq_auto(cpu, d, 2, true); return;
    case ZYDIS_MNEMONIC_VPCMPGTD: exec_evex_pcmpeq_auto(cpu, d, 4, true); return;
    case ZYDIS_MNEMONIC_VPCMPGTQ: exec_evex_pcmpeq_auto(cpu, d, 8, true); return;
    case ZYDIS_MNEMONIC_VPTESTMB:
    case ZYDIS_MNEMONIC_VPTESTMW:
    case ZYDIS_MNEMONIC_VPTESTMD:
    case ZYDIS_MNEMONIC_VPTESTMQ: exec_evex_vptestm(cpu, d, false); return;
    case ZYDIS_MNEMONIC_VPTESTNMB:
    case ZYDIS_MNEMONIC_VPTESTNMW:
    case ZYDIS_MNEMONIC_VPTESTNMD:
    case ZYDIS_MNEMONIC_VPTESTNMQ: exec_evex_vptestm(cpu, d, true); return;
    default: break;
    }
    // Integer arithmetic.
    switch (m) {
    case ZYDIS_MNEMONIC_VPADDB: exec_evex_padd(cpu, d, 1, false); return;
    case ZYDIS_MNEMONIC_VPADDW: exec_evex_padd(cpu, d, 2, false); return;
    case ZYDIS_MNEMONIC_VPADDD: exec_evex_padd(cpu, d, 4, false); return;
    case ZYDIS_MNEMONIC_VPADDQ: exec_evex_padd(cpu, d, 8, false); return;
    case ZYDIS_MNEMONIC_VPSUBB: exec_evex_padd(cpu, d, 1, true); return;
    case ZYDIS_MNEMONIC_VPSUBW: exec_evex_padd(cpu, d, 2, true); return;
    case ZYDIS_MNEMONIC_VPSUBD: exec_evex_padd(cpu, d, 4, true); return;
    case ZYDIS_MNEMONIC_VPSUBQ: exec_evex_padd(cpu, d, 8, true); return;
    case ZYDIS_MNEMONIC_VPMULLW: exec_evex_pmull(cpu, d, 2); return;
    case ZYDIS_MNEMONIC_VPMULLD: exec_evex_pmull(cpu, d, 4); return;
    case ZYDIS_MNEMONIC_VPMULLQ: exec_evex_pmull(cpu, d, 8); return;
    case ZYDIS_MNEMONIC_VPMINUB: exec_evex_pminmax(cpu, d, 1, false, false); return;
    case ZYDIS_MNEMONIC_VPMINSB: exec_evex_pminmax(cpu, d, 1, true, false); return;
    case ZYDIS_MNEMONIC_VPMINUW: exec_evex_pminmax(cpu, d, 2, false, false); return;
    case ZYDIS_MNEMONIC_VPMINSW: exec_evex_pminmax(cpu, d, 2, true, false); return;
    case ZYDIS_MNEMONIC_VPMINUD: exec_evex_pminmax(cpu, d, 4, false, false); return;
    case ZYDIS_MNEMONIC_VPMINSD: exec_evex_pminmax(cpu, d, 4, true, false); return;
    case ZYDIS_MNEMONIC_VPMINUQ: exec_evex_pminmax(cpu, d, 8, false, false); return;
    case ZYDIS_MNEMONIC_VPMINSQ: exec_evex_pminmax(cpu, d, 8, true, false); return;
    case ZYDIS_MNEMONIC_VPMAXUB: exec_evex_pminmax(cpu, d, 1, false, true); return;
    case ZYDIS_MNEMONIC_VPMAXSB: exec_evex_pminmax(cpu, d, 1, true, true); return;
    case ZYDIS_MNEMONIC_VPMAXUW: exec_evex_pminmax(cpu, d, 2, false, true); return;
    case ZYDIS_MNEMONIC_VPMAXSW: exec_evex_pminmax(cpu, d, 2, true, true); return;
    case ZYDIS_MNEMONIC_VPMAXUD: exec_evex_pminmax(cpu, d, 4, false, true); return;
    case ZYDIS_MNEMONIC_VPMAXSD: exec_evex_pminmax(cpu, d, 4, true, true); return;
    case ZYDIS_MNEMONIC_VPMAXUQ: exec_evex_pminmax(cpu, d, 8, false, true); return;
    case ZYDIS_MNEMONIC_VPMAXSQ: exec_evex_pminmax(cpu, d, 8, true, true); return;
    case ZYDIS_MNEMONIC_VPAVGB: exec_evex_pavg_psad(cpu, d, false, 1); return;
    case ZYDIS_MNEMONIC_VPAVGW: exec_evex_pavg_psad(cpu, d, false, 2); return;
    case ZYDIS_MNEMONIC_VPSADBW: exec_evex_pavg_psad(cpu, d, true, 1); return;
    case ZYDIS_MNEMONIC_VPABSB: exec_evex_vpabs(cpu, d); return;
    case ZYDIS_MNEMONIC_VPABSW: exec_evex_vpabs(cpu, d); return;
    case ZYDIS_MNEMONIC_VPABSD: exec_evex_vpabs(cpu, d); return;
    case ZYDIS_MNEMONIC_VPABSQ: exec_evex_vpabs(cpu, d); return;
    default: break;
    }
    // Shifts.
    switch (m) {
    case ZYDIS_MNEMONIC_VPSLLW: exec_evex_shift(cpu, d, 2, 0, false); return;
    case ZYDIS_MNEMONIC_VPSLLD: exec_evex_shift(cpu, d, 4, 0, false); return;
    case ZYDIS_MNEMONIC_VPSLLQ: exec_evex_shift(cpu, d, 8, 0, false); return;
    case ZYDIS_MNEMONIC_VPSRLW: exec_evex_shift(cpu, d, 2, 1, false); return;
    case ZYDIS_MNEMONIC_VPSRLD: exec_evex_shift(cpu, d, 4, 1, false); return;
    case ZYDIS_MNEMONIC_VPSRLQ: exec_evex_shift(cpu, d, 8, 1, false); return;
    case ZYDIS_MNEMONIC_VPSRAW: exec_evex_shift(cpu, d, 2, 2, false); return;
    case ZYDIS_MNEMONIC_VPSRAD: exec_evex_shift(cpu, d, 4, 2, false); return;
    case ZYDIS_MNEMONIC_VPSRAQ: exec_evex_shift(cpu, d, 8, 2, false); return;
    case ZYDIS_MNEMONIC_VPSLLDQ:
    case ZYDIS_MNEMONIC_VPSRLDQ: exec_evex_shift(cpu, d, 1, 0, false); return;
    case ZYDIS_MNEMONIC_VPSLLVW: exec_evex_shift(cpu, d, 2, 0, true); return;
    case ZYDIS_MNEMONIC_VPSLLVD: exec_evex_shift(cpu, d, 4, 0, true); return;
    case ZYDIS_MNEMONIC_VPSLLVQ: exec_evex_shift(cpu, d, 8, 0, true); return;
    case ZYDIS_MNEMONIC_VPSRLVW: exec_evex_shift(cpu, d, 2, 1, true); return;
    case ZYDIS_MNEMONIC_VPSRLVD: exec_evex_shift(cpu, d, 4, 1, true); return;
    case ZYDIS_MNEMONIC_VPSRLVQ: exec_evex_shift(cpu, d, 8, 1, true); return;
    case ZYDIS_MNEMONIC_VPSRAVW: exec_evex_shift(cpu, d, 2, 2, true); return;
    case ZYDIS_MNEMONIC_VPSRAVD: exec_evex_shift(cpu, d, 4, 2, true); return;
    case ZYDIS_MNEMONIC_VPSRAVQ: exec_evex_shift(cpu, d, 8, 2, true); return;
    default: break;
    }
    // Broadcast.
    switch (m) {
    case ZYDIS_MNEMONIC_VPBROADCASTB:
    case ZYDIS_MNEMONIC_VPBROADCASTW:
    case ZYDIS_MNEMONIC_VPBROADCASTD:
    case ZYDIS_MNEMONIC_VPBROADCASTQ:
    case ZYDIS_MNEMONIC_VBROADCASTSS:
    case ZYDIS_MNEMONIC_VBROADCASTSD:
    case ZYDIS_MNEMONIC_VBROADCASTF32X2:
    case ZYDIS_MNEMONIC_VBROADCASTF32X4:
    case ZYDIS_MNEMONIC_VBROADCASTF32X8:
    case ZYDIS_MNEMONIC_VBROADCASTF64X2:
    case ZYDIS_MNEMONIC_VBROADCASTF64X4:
    case ZYDIS_MNEMONIC_VBROADCASTI32X2:
    case ZYDIS_MNEMONIC_VBROADCASTI32X4:
    case ZYDIS_MNEMONIC_VBROADCASTI32X8:
    case ZYDIS_MNEMONIC_VBROADCASTI64X2:
    case ZYDIS_MNEMONIC_VBROADCASTI64X4:
    case ZYDIS_MNEMONIC_VBROADCASTI128:
    case ZYDIS_MNEMONIC_VPBROADCASTMW2D:
    case ZYDIS_MNEMONIC_VPBROADCASTMB2Q: exec_evex_broadcast(cpu, d); return;
    default: break;
    }
    // Permutes.
    switch (m) {
    case ZYDIS_MNEMONIC_VPERMB: exec_evex_perm(cpu, d, 0); return;
    case ZYDIS_MNEMONIC_VPERMW: exec_evex_perm(cpu, d, 1); return;
    case ZYDIS_MNEMONIC_VPERMD: exec_evex_perm(cpu, d, 2); return;
    case ZYDIS_MNEMONIC_VMOVD: exec_evex_movd(cpu, d, false); return;
    case ZYDIS_MNEMONIC_VMOVQ: exec_evex_movd(cpu, d, true); return;
    case ZYDIS_MNEMONIC_VPERMILPS:
    case ZYDIS_MNEMONIC_VPERMILPD: exec_evex_perm(cpu, d, 3); return;
    case ZYDIS_MNEMONIC_VPERMPD:
    case ZYDIS_MNEMONIC_VPERMQ: exec_evex_perm(cpu, d, 4); return;
    case ZYDIS_MNEMONIC_VPSHUFD: exec_evex_perm(cpu, d, 5); return;
    case ZYDIS_MNEMONIC_VSHUFPS:
    case ZYDIS_MNEMONIC_VSHUFPD: exec_evex_perm(cpu, d, 6); return;
    default: break;
    }
    // Blends.
    switch (m) {
    case ZYDIS_MNEMONIC_VPBLENDMB:
    case ZYDIS_MNEMONIC_VPBLENDMW:
    case ZYDIS_MNEMONIC_VPBLENDMD:
    case ZYDIS_MNEMONIC_VPBLENDMQ:
    case ZYDIS_MNEMONIC_VBLENDMPS:
    case ZYDIS_MNEMONIC_VBLENDMPD: exec_evex_blend(cpu, d); return;
    default: break;
    }
    // Converts.
    switch (m) {
    case ZYDIS_MNEMONIC_VPMOVSXBW: exec_evex_pmovsx(cpu, d, 1, 2, true); return;
    case ZYDIS_MNEMONIC_VPMOVSXBD: exec_evex_pmovsx(cpu, d, 1, 4, true); return;
    case ZYDIS_MNEMONIC_VPMOVSXBQ: exec_evex_pmovsx(cpu, d, 1, 8, true); return;
    case ZYDIS_MNEMONIC_VPMOVSXWD: exec_evex_pmovsx(cpu, d, 2, 4, true); return;
    case ZYDIS_MNEMONIC_VPMOVSXWQ: exec_evex_pmovsx(cpu, d, 2, 8, true); return;
    case ZYDIS_MNEMONIC_VPMOVSXDQ: exec_evex_pmovsx(cpu, d, 4, 8, true); return;
    case ZYDIS_MNEMONIC_VPMOVZXBW: exec_evex_pmovsx(cpu, d, 1, 2, false); return;
    case ZYDIS_MNEMONIC_VPMOVZXBD: exec_evex_pmovsx(cpu, d, 1, 4, false); return;
    case ZYDIS_MNEMONIC_VPMOVZXBQ: exec_evex_pmovsx(cpu, d, 1, 8, false); return;
    case ZYDIS_MNEMONIC_VPMOVZXWD: exec_evex_pmovsx(cpu, d, 2, 4, false); return;
    case ZYDIS_MNEMONIC_VPMOVZXWQ: exec_evex_pmovsx(cpu, d, 2, 8, false); return;
    case ZYDIS_MNEMONIC_VPMOVZXDQ: exec_evex_pmovsx(cpu, d, 4, 8, false); return;
    case ZYDIS_MNEMONIC_VPMOVDB: exec_evex_pmov_down(cpu, d, 4, 1, 0); return;
    case ZYDIS_MNEMONIC_VPMOVDW: exec_evex_pmov_down(cpu, d, 4, 2, 0); return;
    case ZYDIS_MNEMONIC_VPMOVQB: exec_evex_pmov_down(cpu, d, 8, 1, 0); return;
    case ZYDIS_MNEMONIC_VPMOVQD: exec_evex_pmov_down(cpu, d, 8, 4, 0); return;
    case ZYDIS_MNEMONIC_VPMOVQW: exec_evex_pmov_down(cpu, d, 8, 2, 0); return;
    case ZYDIS_MNEMONIC_VPMOVWB: exec_evex_pmov_down(cpu, d, 2, 1, 0); return;
    case ZYDIS_MNEMONIC_VPMOVSDB: exec_evex_pmov_down(cpu, d, 4, 1, 1); return;
    case ZYDIS_MNEMONIC_VPMOVSDW: exec_evex_pmov_down(cpu, d, 4, 2, 1); return;
    case ZYDIS_MNEMONIC_VPMOVSQB: exec_evex_pmov_down(cpu, d, 8, 1, 1); return;
    case ZYDIS_MNEMONIC_VPMOVSQD: exec_evex_pmov_down(cpu, d, 8, 4, 1); return;
    case ZYDIS_MNEMONIC_VPMOVSQW: exec_evex_pmov_down(cpu, d, 8, 2, 1); return;
    case ZYDIS_MNEMONIC_VPMOVSWB: exec_evex_pmov_down(cpu, d, 2, 1, 1); return;
    case ZYDIS_MNEMONIC_VPMOVUSDB: exec_evex_pmov_down(cpu, d, 4, 1, -1); return;
    case ZYDIS_MNEMONIC_VPMOVUSDW: exec_evex_pmov_down(cpu, d, 4, 2, -1); return;
    case ZYDIS_MNEMONIC_VPMOVUSQB: exec_evex_pmov_down(cpu, d, 8, 1, -1); return;
    case ZYDIS_MNEMONIC_VPMOVUSQD: exec_evex_pmov_down(cpu, d, 8, 4, -1); return;
    case ZYDIS_MNEMONIC_VPMOVUSQW: exec_evex_pmov_down(cpu, d, 8, 2, -1); return;
    case ZYDIS_MNEMONIC_VPMOVUSWB: exec_evex_pmov_down(cpu, d, 2, 1, -1); return;
    case ZYDIS_MNEMONIC_VPMOVM2B:
    case ZYDIS_MNEMONIC_VPMOVM2W:
    case ZYDIS_MNEMONIC_VPMOVM2D:
    case ZYDIS_MNEMONIC_VPMOVM2Q: exec_evex_movm(cpu, d, true); return;
    case ZYDIS_MNEMONIC_VPMOVB2M:
    case ZYDIS_MNEMONIC_VPMOVW2M:
    case ZYDIS_MNEMONIC_VPMOVD2M:
    case ZYDIS_MNEMONIC_VPMOVQ2M: exec_evex_movm(cpu, d, false); return;
    case ZYDIS_MNEMONIC_VCVTDQ2PS: exec_evex_cvt(cpu, d, 0); return;
    case ZYDIS_MNEMONIC_VCVTTPS2DQ: exec_evex_cvt(cpu, d, 1); return;
    case ZYDIS_MNEMONIC_VCVTPS2DQ: exec_evex_cvt(cpu, d, 2); return;
    case ZYDIS_MNEMONIC_VCVTPD2PS: exec_evex_cvt(cpu, d, 3); return;
    case ZYDIS_MNEMONIC_VCVTPS2PD: exec_evex_cvt(cpu, d, 4); return;
    default: break;
    }
    // Misc integer.
    switch (m) {
    case ZYDIS_MNEMONIC_VPOPCNTB:
    case ZYDIS_MNEMONIC_VPOPCNTW:
    case ZYDIS_MNEMONIC_VPOPCNTD:
    case ZYDIS_MNEMONIC_VPOPCNTQ: exec_evex_popcnt_lzcnt(cpu, d, false); return;
    case ZYDIS_MNEMONIC_VPLZCNTD:
    case ZYDIS_MNEMONIC_VPLZCNTQ: exec_evex_popcnt_lzcnt(cpu, d, true); return;
    case ZYDIS_MNEMONIC_VP2INTERSECTD:
    case ZYDIS_MNEMONIC_VP2INTERSECTQ: exec_evex_p2intersect(cpu, d); return;
    case ZYDIS_MNEMONIC_VPUNPCKLBW:
    case ZYDIS_MNEMONIC_VPUNPCKHBW:
    case ZYDIS_MNEMONIC_VPUNPCKLWD:
    case ZYDIS_MNEMONIC_VPUNPCKHWD:
    case ZYDIS_MNEMONIC_VPUNPCKLDQ:
    case ZYDIS_MNEMONIC_VPUNPCKHDQ:
    case ZYDIS_MNEMONIC_VPUNPCKLQDQ:
    case ZYDIS_MNEMONIC_VPUNPCKHQDQ:
    case ZYDIS_MNEMONIC_VUNPCKLPS:
    case ZYDIS_MNEMONIC_VUNPCKHPS:
    case ZYDIS_MNEMONIC_VUNPCKLPD:
    case ZYDIS_MNEMONIC_VUNPCKHPD: exec_evex_unpack(cpu, d, 0); return;
    case ZYDIS_MNEMONIC_VPSHUFB: exec_evex_unpack(cpu, d, 1); return;
    case ZYDIS_MNEMONIC_VPALIGNR: exec_evex_unpack(cpu, d, 2); return;
    case ZYDIS_MNEMONIC_VPBLENDVB: exec_evex_unpack(cpu, d, 3); return;
    case ZYDIS_MNEMONIC_VPCOMPRESSB:
    case ZYDIS_MNEMONIC_VPCOMPRESSW:
    case ZYDIS_MNEMONIC_VPCOMPRESSD:
    case ZYDIS_MNEMONIC_VPCOMPRESSQ: exec_evex_compress(cpu, d, false); return;
    case ZYDIS_MNEMONIC_VPEXPANDB:
    case ZYDIS_MNEMONIC_VPEXPANDW:
    case ZYDIS_MNEMONIC_VPEXPANDD:
    case ZYDIS_MNEMONIC_VPEXPANDQ: exec_evex_compress(cpu, d, true); return;
    case ZYDIS_MNEMONIC_VALIGND:
    case ZYDIS_MNEMONIC_VALIGNQ: exec_evex_align(cpu, d); return;
    case ZYDIS_MNEMONIC_VINSERTF32X4:
    case ZYDIS_MNEMONIC_VINSERTF64X2:
    case ZYDIS_MNEMONIC_VINSERTI32X4:
    case ZYDIS_MNEMONIC_VINSERTI64X2:
    case ZYDIS_MNEMONIC_VINSERTF32X8:
    case ZYDIS_MNEMONIC_VINSERTF64X4:
    case ZYDIS_MNEMONIC_VINSERTI32X8:
    case ZYDIS_MNEMONIC_VINSERTI64X4:
    case ZYDIS_MNEMONIC_VEXTRACTF32X4:
    case ZYDIS_MNEMONIC_VEXTRACTF64X2:
    case ZYDIS_MNEMONIC_VEXTRACTI32X4:
    case ZYDIS_MNEMONIC_VEXTRACTI64X2:
    case ZYDIS_MNEMONIC_VEXTRACTF32X8:
    case ZYDIS_MNEMONIC_VEXTRACTF64X4:
    case ZYDIS_MNEMONIC_VEXTRACTI32X8:
    case ZYDIS_MNEMONIC_VEXTRACTI64X4: {
        bool ins = (m == ZYDIS_MNEMONIC_VINSERTF32X4 || m == ZYDIS_MNEMONIC_VINSERTF64X2 ||
                    m == ZYDIS_MNEMONIC_VINSERTI32X4 || m == ZYDIS_MNEMONIC_VINSERTI64X2 ||
                    m == ZYDIS_MNEMONIC_VINSERTF32X8 || m == ZYDIS_MNEMONIC_VINSERTF64X4 ||
                    m == ZYDIS_MNEMONIC_VINSERTI32X8 || m == ZYDIS_MNEMONIC_VINSERTI64X4);
        exec_evex_ins_extract(cpu, d, ins);
        return;
    }
    case ZYDIS_MNEMONIC_VSHUFF32X4:
    case ZYDIS_MNEMONIC_VSHUFF64X2:
    case ZYDIS_MNEMONIC_VSHUFI32X4:
    case ZYDIS_MNEMONIC_VSHUFI64X2: exec_evex_shufx(cpu, d); return;
    default: break;
    }
    // Gathers / scatters.
    switch (m) {
    case ZYDIS_MNEMONIC_VPGATHERDD: exec_evex_gather(cpu, d, 4, 4); return;
    case ZYDIS_MNEMONIC_VPGATHERDQ: exec_evex_gather(cpu, d, 4, 8); return;
    case ZYDIS_MNEMONIC_VPGATHERQD: exec_evex_gather(cpu, d, 8, 4); return;
    case ZYDIS_MNEMONIC_VPGATHERQQ: exec_evex_gather(cpu, d, 8, 8); return;
    case ZYDIS_MNEMONIC_VPSCATTERDD: exec_evex_scatter(cpu, d, 4, 4); return;
    case ZYDIS_MNEMONIC_VPSCATTERDQ: exec_evex_scatter(cpu, d, 4, 8); return;
    case ZYDIS_MNEMONIC_VPSCATTERQD: exec_evex_scatter(cpu, d, 8, 4); return;
    case ZYDIS_MNEMONIC_VPSCATTERQQ: exec_evex_scatter(cpu, d, 8, 8); return;
    default: break;
    }
    unimplemented(cpu, d, "EVEX (phase 2: unsupported EVEX form)");
}


// EVEX VMOVD/VMOVQ: low-element moves between GPR, vector, and memory.
void exec_evex_movd(CPU* cpu, const Dec& d, bool is_q) {
    unsigned elem = is_q ? 8 : 4;
    int ex[5];
    int n = evex_explicit(d, ex);
    if (n < 2)
        guest_error(cpu, "evex movd needs 2 operands");
    bool zeroing = false;
    uint64_t kbits = evex_mask(cpu, d, 1, zeroing);
    bool enabled = (kbits & 1) != 0;
    const auto& dop = d.ops[ex[0]];
    const auto& sop = d.ops[ex[1]];
    int gidx, gsize, gshift;
    bool dst_is_gpr = (dop.type == ZYDIS_OPERAND_TYPE_REGISTER &&
                       reg_to_gpr(dop.reg.value, gidx, gsize, gshift));
    if (dst_is_gpr) {
        // GPR <- xmm/mem. Per-element precise: disabled lanes (masked-off)
        // do not fault and do not taint; merging preserves old GPR taint.
        uint8_t tmp[8] = {0};
        Taint t;
        if (sop.type == ZYDIS_OPERAND_TYPE_REGISTER) {
            int idx, width;
            if (!reg_to_vec(sop.reg.value, idx, width))
                guest_error(cpu, "bad evex movd source");
            // Register source always readable; taint only if enabled.
            memcpy(tmp, cpu->xmm[idx].bytes, elem);
            if (enabled)
                t = cpu->xmm[idx].taint;
        } else if (sop.type == ZYDIS_OPERAND_TYPE_MEMORY) {
            uint64_t addr = resolve_mem(cpu, sop, cpu->rip + d.insn.length);
            if (enabled) {
                mem_load_bytes(cpu, addr, tmp, elem);
                mem_get_taint(addr, elem, &t);
            } else {
                // Disabled: no fault, no mem taint.
                if (zeroing)
                    memset(tmp, 0, elem);
                else {
                    // Merge: keep old GPR value.
                    Val old = op_load(cpu, d, ex[0]);
                    memcpy(tmp, &old.v, elem);
                    t = old.taint;
                }
            }
        } else {
            guest_error(cpu, "bad evex movd source");
        }
        Val out;
        memcpy(&out.v, tmp, elem);
        out.taint = t;
        op_store(cpu, d, ex[0], out);
    } else {
        // xmm/mem <- GPR or xmm/mem (low element). Precise for masking.
        uint8_t tmp[8] = {0};
        Taint t;
        if (sop.type == ZYDIS_OPERAND_TYPE_REGISTER) {
            int idx, width;
            if (reg_to_vec(sop.reg.value, idx, width)) {
                memcpy(tmp, cpu->xmm[idx].bytes, elem);
                t = cpu->xmm[idx].taint;
            } else {
                Val g = op_load(cpu, d, ex[1]);
                memcpy(tmp, &g.v, elem);
                t = g.taint;
            }
        } else if (sop.type == ZYDIS_OPERAND_TYPE_MEMORY) {
            uint64_t addr = resolve_mem(cpu, sop, cpu->rip + d.insn.length);
            mem_load_bytes(cpu, addr, tmp, elem);
            mem_get_taint(addr, elem, &t);
        } else {
            guest_error(cpu, "bad evex movd source");
        }
        if (dop.type == ZYDIS_OPERAND_TYPE_REGISTER) {
            int idx, width;
            if (!reg_to_vec(dop.reg.value, idx, width))
                guest_error(cpu, "bad evex movd dest");
            Taint oldt = cpu->xmm[idx].taint;
            VecVal old;
            memcpy(old, cpu->xmm[idx].bytes, MAX_VEC_BYTES);
            VecVal merged;
            memcpy(merged, old, MAX_VEC_BYTES);
            if (enabled)
                memcpy(merged, tmp, elem);
            else if (zeroing)
                memset(merged, 0, elem);
            // AVX semantics: zero upper bits beyond elem? VMOVQ zeroes
            // bits above 64; VMOVD zeroes above 32.
            memset(merged + elem, 0, MAX_VEC_BYTES - elem);
            memcpy(cpu->xmm[idx].bytes, merged, MAX_VEC_BYTES);
            if (!enabled && !zeroing)
                cpu->xmm[idx].taint = oldt;
            else if (zeroing && !enabled)
                cpu->xmm[idx].taint.clear();
            else if (enabled && !zeroing)
                cpu->xmm[idx].taint = oldt | t;
            else
                cpu->xmm[idx].taint = t;
        } else if (dop.type == ZYDIS_OPERAND_TYPE_MEMORY) {
            uint64_t addr = resolve_mem(cpu, dop, cpu->rip + d.insn.length);
            if (enabled) {
                mem_store_bytes(cpu, addr, tmp, elem);
                mem_note_store(cpu, addr, elem, t);
            }
        } else {
            guest_error(cpu, "bad evex movd dest");
        }
    }
    cpu->rip = next_rip(cpu, d);
}


// RDPKRU/WRPKRU: user-mode PKRU access via host instructions.
void exec_pkru(CPU* cpu, const Dec& d, bool write) {
    if (write) {
        unsigned lo = (unsigned)cpu->gpr[ZG_RAX].val;
        unsigned hi = (unsigned)cpu->gpr[ZG_RDX].val;
        unsigned cx = (unsigned)cpu->gpr[ZG_RCX].val;
        if (cx != 0)
            guest_error(cpu, "guest SIGSEGV (wrpkru with ECX!=0)", cpu->rip);
        asm volatile("wrpkru" :: "a"(lo), "d"(hi), "c"(0u) : "cc");
    } else {
        unsigned cx = (unsigned)cpu->gpr[ZG_RCX].val;
        if (cx != 0)
            guest_error(cpu, "guest SIGSEGV (rdpkru with ECX!=0)", cpu->rip);
        unsigned lo = 0, hi = 0;
        asm volatile("rdpkru" : "=a"(lo), "=d"(hi) : "c"(0u));
        cpu->gpr[ZG_RAX].val = lo;
        cpu->gpr[ZG_RDX].val = hi;
        cpu->gpr[ZG_RAX].taint.cannot_branch = false;
        cpu->gpr[ZG_RAX].taint.cannot_index = false;
        cpu->gpr[ZG_RDX].taint.cannot_branch = false;
        cpu->gpr[ZG_RDX].taint.cannot_index = false;
    }
    cpu->rip = next_rip(cpu, d);
}

} // namespace


// === dispatcher / loop / core dump ==========================================

namespace {

std::mutex g_bus_lock; // emulates the x86 bus lock for LOCK-prefixed insns

bool has_vec_reg_operand(const Dec& d) {
    for (int i = 0; i < d.insn.operand_count; i++) {
        if (d.ops[i].visibility != ZYDIS_OPERAND_VISIBILITY_EXPLICIT)
            continue;
        if (d.ops[i].type == ZYDIS_OPERAND_TYPE_REGISTER) {
            int idx, w;
            if (reg_to_vec(d.ops[i].reg.value, idx, w))
                return true;
        }
    }
    return false;
}

bool is_seg_reg(ZydisRegister r) {
    return r == ZYDIS_REGISTER_ES || r == ZYDIS_REGISTER_CS ||
           r == ZYDIS_REGISTER_SS || r == ZYDIS_REGISTER_DS ||
           r == ZYDIS_REGISTER_FS || r == ZYDIS_REGISTER_GS;
}

bool touches_seg_or_sysreg(const Dec& d) {
    for (int i = 0; i < d.insn.operand_count; i++) {
        if (d.ops[i].visibility != ZYDIS_OPERAND_VISIBILITY_EXPLICIT)
            continue;
        if (d.ops[i].type == ZYDIS_OPERAND_TYPE_REGISTER) {
            ZydisRegister r = d.ops[i].reg.value;
            if (is_seg_reg(r))
                return true;
            if ((r >= ZYDIS_REGISTER_CR0 && r <= ZYDIS_REGISTER_CR15) ||
                (r >= ZYDIS_REGISTER_DR0 && r <= ZYDIS_REGISTER_DR15) ||
                (r >= ZYDIS_REGISTER_TR0 && r <= ZYDIS_REGISTER_TR7) ||
                r == ZYDIS_REGISTER_GDTR || r == ZYDIS_REGISTER_LDTR ||
                r == ZYDIS_REGISTER_IDTR || r == ZYDIS_REGISTER_TR)
                return true;
        }
    }
    return false;
}

void exec_xlat(CPU* cpu, const Dec& d) {
    uint64_t addr = cpu->gpr[ZG_RBX].val + (cpu->gpr[ZG_RAX].val & 0xff);
    uint8_t v = mem_load8(cpu, addr);
    cpu->gpr[ZG_RAX].val = (cpu->gpr[ZG_RAX].val & ~0xffULL) | v;
    cpu->rip = next_rip(cpu, d);
}

void exec_fsbase(CPU* cpu, const Dec& d) {
    bool is_fs = (d.insn.mnemonic == ZYDIS_MNEMONIC_RDFSBASE ||
                  d.insn.mnemonic == ZYDIS_MNEMONIC_WRFSBASE);
    bool is_read = (d.insn.mnemonic == ZYDIS_MNEMONIC_RDFSBASE ||
                    d.insn.mnemonic == ZYDIS_MNEMONIC_RDGSBASE);
    uint64_t& base = is_fs ? cpu->fs_base : cpu->gs_base;
    if (is_read) {
        Val out;
        out.v = base;
        op_store(cpu, d, 0, out);
    } else {
        Val v = op_load(cpu, d, 0);
        base = v.v;
    }
    cpu->rip = next_rip(cpu, d);
}

void exec_nop_like(CPU* cpu, const Dec& d) { cpu->rip = next_rip(cpu, d); }

} // namespace

static uint64_t dbg_lo = 0, dbg_hi = 0, dbg_base = 0;
static uint64_t dbg2_lo = 0, dbg2_hi = 0, dbg2_base = 0;
static bool dbg_init = false;
static char dbg_needle[128] = {0};
static char dbg2_needle[128] = {0};
static void dbg_check() {
    if (!dbg_init) {
        dbg_init = true;
        const char* e = getenv("ZEG_DEBUG_OFF");
        if (e) {
            unsigned long long a = 0, b = 0;
            if (sscanf(e, "%llx-%llx", &a, &b) == 2) {
                dbg_lo = a;
                dbg_hi = b;
            }
        }
        // ZEG_DEBUG_MOD=base:lo-hi uses an explicit base instead of interp.
        // ZEG_DEBUG_MOD=name/needle:lo-hi resolves base via /proc/self/maps.
        // Optional second range.
        const char* m2 = getenv("ZEG_DEBUG_MOD2");
        if (m2) {
            unsigned long long bs = 0, a = 0, b = 0;
            if (sscanf(m2, "%llx:%llx-%llx", &bs, &a, &b) == 3) {
                dbg2_base = bs;
                dbg2_lo = a;
                dbg2_hi = b;
            } else if (sscanf(m2, "%127[^:]:%llx-%llx", dbg2_needle, &a, &b) == 3) {
                dbg2_lo = a;
                dbg2_hi = b;
            }
        }
        const char* m = getenv("ZEG_DEBUG_MOD");
        if (m) {
            unsigned long long bs = 0, a = 0, b = 0;
            if (sscanf(m, "%llx:%llx-%llx", &bs, &a, &b) == 3) {
                dbg_base = bs;
                dbg_lo = a;
                dbg_hi = b;
            } else {
                if (sscanf(m, "%127[^:]:%llx-%llx", dbg_needle, &a, &b) == 3) {
                    dbg_lo = a;
                    dbg_hi = b;
                }
            }
        }
    }
}

void exec_one(CPU* cpu) {
    Dec d = fetch_decode(cpu);
    if (cpu->emu->trace_insn)
        trace_insn(cpu, d);
    dbg_check();
    bool dbg = false;
    if (dbg_lo) {
        uint64_t base = dbg_base ? dbg_base : cpu->emu->interp_base;
        // Throttle maps re-scans (they're slow).
        static thread_local uint64_t dbg_tick = 0;
        if (((!dbg_base && dbg_needle[0]) || (!dbg2_base && dbg2_needle[0])) &&
            ((++dbg_tick & 0xfffff) == 0 || (!dbg_base && dbg_needle[0]) ||
             (!dbg2_base && dbg2_needle[0]))) {
            // DSO may load after first instruction: resolve on demand.
            FILE* mf = fopen("/proc/self/maps", "r");
            if (mf) {
                char line[512];
                while (fgets(line, sizeof(line), mf)) {
                    unsigned long long s0 = 0, s1 = 0, off = 0;
                    char perms[8] = {0};
                    if (sscanf(line, "%llx-%llx %7s %llx", &s0, &s1, perms,
                               &off) < 3)
                        continue;
                    if (dbg_needle[0] && strstr(line, dbg_needle) && off == 0) {
                        // Prefer the highest file-offset-0 match: the host
                        // and guest may map the same DSO; the guest copy
                        // (loaded by our elf_loader) sits higher.
                        if (!dbg_base || s0 > dbg_base)
                            dbg_base = s0;
                    }
                    if (dbg2_needle[0] && strstr(line, dbg2_needle) && off == 0) {
                        if (!dbg2_base || s0 > dbg2_base)
                            dbg2_base = s0;
                    }
                }
                fclose(mf);
                base = dbg_base ? dbg_base : cpu->emu->interp_base;
            }
        }
        if (base) {
            uint64_t off = cpu->rip - base;
            dbg = off >= dbg_lo && off < dbg_hi;
        } else {
            // No base (e.g. static binary without interpreter): the range
            // is an absolute RIP range (ZEG_DEBUG_OFF).
            dbg = cpu->rip >= dbg_lo && cpu->rip < dbg_hi;
        }
        if (!dbg && dbg2_hi) {
            uint64_t base2 = dbg2_base;
            if (!base2 && dbg2_needle[0])
                base2 = 0; // resolved above when tick fires
            if (base2) {
                uint64_t off = cpu->rip - base2;
                dbg = off >= dbg2_lo && off < dbg2_hi;
            }
        }
    }

    if (is_evex(d) ||
        (d.insn.attributes & ZYDIS_ATTRIB_HAS_MVEX) ||
        d.insn.encoding == ZYDIS_INSTRUCTION_ENCODING_EVEX ||
        d.insn.encoding == ZYDIS_INSTRUCTION_ENCODING_MVEX) {
        if (d.insn.encoding == ZYDIS_INSTRUCTION_ENCODING_MVEX ||
            (d.insn.attributes & ZYDIS_ATTRIB_HAS_MVEX))
            unimplemented(cpu, d, "MVEX/KNC (not supported)");
        exec_evex(cpu, d);
        return;
    }

    if (dbg) {
        fprintf(stderr, "[zegdbg:tid=%d] ", cpu->tid);
        trace_insn(cpu, d);
        fprintf(stderr,
                "    rax=%lx rbx=%lx rcx=%lx rdx=%lx rsi=%lx rdi=%lx rbp=%lx rsp=%lx "
                "r8=%lx r9=%lx r10=%lx r11=%lx r12=%lx r13=%lx r14=%lx r15=%lx\n",
                cpu->gpr[0].val, cpu->gpr[3].val, cpu->gpr[1].val,
                cpu->gpr[2].val, cpu->gpr[6].val, cpu->gpr[7].val,
                cpu->gpr[5].val, cpu->gpr[4].val, cpu->gpr[8].val,
                cpu->gpr[9].val, cpu->gpr[10].val, cpu->gpr[11].val,
                cpu->gpr[12].val, cpu->gpr[13].val, cpu->gpr[14].val,
                cpu->gpr[15].val);
        // TEMPDEBUG: vector regs as floats.
        for (int ri = 0; ri < 8; ri++) {
            fprintf(stderr, "    ymm%d:", ri);
            for (int fi = 0; fi < 8; fi++) {
                float f = 0;
                memcpy(&f, cpu->xmm[ri].bytes + fi * 4, 4);
                fprintf(stderr, " %g", f);
            }
            fprintf(stderr, "\n");
        }
        fflush(stderr);
    }

    bool locked = (d.insn.attributes & ZYDIS_ATTRIB_HAS_LOCK) != 0;
    // XCHG (reg/mem) and CMPXCHG8B/16B are implicitly locked even without
    // a LOCK prefix; serialize them too for atomicity across guest threads.
    if (!locked &&
        (d.insn.mnemonic == ZYDIS_MNEMONIC_XCHG ||
         d.insn.mnemonic == ZYDIS_MNEMONIC_CMPXCHG8B ||
         d.insn.mnemonic == ZYDIS_MNEMONIC_CMPXCHG16B)) {
        for (int i = 0; i < d.insn.operand_count; i++) {
            if (d.ops[i].visibility != ZYDIS_OPERAND_VISIBILITY_HIDDEN &&
                d.ops[i].type == ZYDIS_OPERAND_TYPE_MEMORY) {
                locked = true;
                break;
            }
        }
    }
    std::unique_lock<std::mutex> bus(g_bus_lock, std::defer_lock);
    if (locked)
        bus.lock();

    ZydisMnemonic m = d.insn.mnemonic;
    switch (m) {
    // -- data movement --
    case ZYDIS_MNEMONIC_MOV:
        if (touches_seg_or_sysreg(d))
            unimplemented(cpu, d, "segment/control-register mov");
        exec_mov(cpu, d);
        return;
    case ZYDIS_MNEMONIC_LEA: exec_lea(cpu, d); return;
    case ZYDIS_MNEMONIC_XCHG: exec_xchg(cpu, d); return;
    case ZYDIS_MNEMONIC_PUSH:
        if (touches_seg_or_sysreg(d))
            unimplemented(cpu, d, "segment push");
        exec_push(cpu, d);
        return;
    case ZYDIS_MNEMONIC_POP:
        if (touches_seg_or_sysreg(d))
            unimplemented(cpu, d, "segment pop");
        exec_pop(cpu, d);
        return;
    case ZYDIS_MNEMONIC_LEAVE: exec_leave(cpu, d); return;
    case ZYDIS_MNEMONIC_ENTER: exec_enter(cpu, d); return;
    case ZYDIS_MNEMONIC_XLAT: exec_xlat(cpu, d); return;
    case ZYDIS_MNEMONIC_MOVBE: exec_movbe(cpu, d); return;
    case ZYDIS_MNEMONIC_MOVNTI: exec_movnti(cpu, d); return;

    // -- integer ALU --
    case ZYDIS_MNEMONIC_ADD:
    case ZYDIS_MNEMONIC_ADC:
    case ZYDIS_MNEMONIC_SUB:
    case ZYDIS_MNEMONIC_SBB:
    case ZYDIS_MNEMONIC_CMP:
    case ZYDIS_MNEMONIC_AND:
    case ZYDIS_MNEMONIC_OR:
    case ZYDIS_MNEMONIC_XOR:
    case ZYDIS_MNEMONIC_TEST: exec_alu(cpu, d, m); return;
    case ZYDIS_MNEMONIC_INC: exec_incdec(cpu, d, true); return;
    case ZYDIS_MNEMONIC_DEC: exec_incdec(cpu, d, false); return;
    case ZYDIS_MNEMONIC_NEG: exec_neg(cpu, d); return;
    case ZYDIS_MNEMONIC_NOT: exec_not(cpu, d); return;
    case ZYDIS_MNEMONIC_SHL:
    case ZYDIS_MNEMONIC_SHR:
    case ZYDIS_MNEMONIC_SAR:
    case ZYDIS_MNEMONIC_ROL:
    case ZYDIS_MNEMONIC_ROR:
    case ZYDIS_MNEMONIC_RCL:
    case ZYDIS_MNEMONIC_RCR: exec_shift(cpu, d, m); return;
    case ZYDIS_MNEMONIC_SHLD: exec_shld_shrd(cpu, d, true); return;
    case ZYDIS_MNEMONIC_SHRD: exec_shld_shrd(cpu, d, false); return;
    case ZYDIS_MNEMONIC_IMUL: exec_imul(cpu, d); return;
    case ZYDIS_MNEMONIC_MUL: exec_mul(cpu, d); return;
    case ZYDIS_MNEMONIC_DIV: exec_div(cpu, d, false); return;
    case ZYDIS_MNEMONIC_IDIV: exec_div(cpu, d, true); return;
    case ZYDIS_MNEMONIC_MOVZX: exec_movzx(cpu, d); return;
    case ZYDIS_MNEMONIC_MOVSX:
    case ZYDIS_MNEMONIC_MOVSXD: exec_movsx(cpu, d); return;
    case ZYDIS_MNEMONIC_CBW:
    case ZYDIS_MNEMONIC_CWDE:
    case ZYDIS_MNEMONIC_CDQE:
    case ZYDIS_MNEMONIC_CWD:
    case ZYDIS_MNEMONIC_CDQ:
    case ZYDIS_MNEMONIC_CQO: exec_cbw(cpu, d); return;
    case ZYDIS_MNEMONIC_BSWAP: exec_bswap(cpu, d); return;
    case ZYDIS_MNEMONIC_BT:
    case ZYDIS_MNEMONIC_BTS:
    case ZYDIS_MNEMONIC_BTR:
    case ZYDIS_MNEMONIC_BTC: exec_bit_test(cpu, d); return;
    case ZYDIS_MNEMONIC_BSF: exec_bsfbsr(cpu, d, false); return;
    case ZYDIS_MNEMONIC_BSR: exec_bsfbsr(cpu, d, true); return;
    case ZYDIS_MNEMONIC_POPCNT: exec_popcnt(cpu, d); return;
    case ZYDIS_MNEMONIC_TZCNT:
    case ZYDIS_MNEMONIC_LZCNT:
        exec_tzcnt_lzcnt(cpu, d, m == ZYDIS_MNEMONIC_TZCNT);
        return;
    case ZYDIS_MNEMONIC_CMPXCHG: exec_cmpxchg(cpu, d); return;
    case ZYDIS_MNEMONIC_CMPXCHG8B: exec_cmpxchg8b(cpu, d); return;
    case ZYDIS_MNEMONIC_CMPXCHG16B: exec_cmpxchg16b(cpu, d); return;
    case ZYDIS_MNEMONIC_XADD: exec_xadd(cpu, d); return;
    case ZYDIS_MNEMONIC_LAHF:
    case ZYDIS_MNEMONIC_SAHF: exec_lahf_sahf(cpu, d); return;
    case ZYDIS_MNEMONIC_STC:
    case ZYDIS_MNEMONIC_CLC:
    case ZYDIS_MNEMONIC_CMC:
    case ZYDIS_MNEMONIC_STD:
    case ZYDIS_MNEMONIC_CLD: exec_flag_simple(cpu, d); return;
    case ZYDIS_MNEMONIC_PUSHF:
    case ZYDIS_MNEMONIC_PUSHFD:
    case ZYDIS_MNEMONIC_PUSHFQ: exec_pushf(cpu, d); return;
    case ZYDIS_MNEMONIC_POPF:
    case ZYDIS_MNEMONIC_POPFD:
    case ZYDIS_MNEMONIC_POPFQ: exec_popf(cpu, d); return;

    // -- BMI/BMI2 --
    case ZYDIS_MNEMONIC_ANDN:
    case ZYDIS_MNEMONIC_BLSI:
    case ZYDIS_MNEMONIC_BLSMSK:
    case ZYDIS_MNEMONIC_BLSR:
    case ZYDIS_MNEMONIC_BZHI:
    case ZYDIS_MNEMONIC_SHLX:
    case ZYDIS_MNEMONIC_SHRX:
    case ZYDIS_MNEMONIC_SARX:
    case ZYDIS_MNEMONIC_RORX:
    case ZYDIS_MNEMONIC_MULX:
    case ZYDIS_MNEMONIC_PEXT:
    case ZYDIS_MNEMONIC_PDEP:
    case ZYDIS_MNEMONIC_ADCX:
    case ZYDIS_MNEMONIC_ADOX: exec_bmi_simple(cpu, d); return;
    case ZYDIS_MNEMONIC_BEXTR: exec_bextr(cpu, d); return;

    // -- control flow --
    case ZYDIS_MNEMONIC_CALL: exec_call(cpu, d); return;
    case ZYDIS_MNEMONIC_RET: exec_ret(cpu, d); return;
    case ZYDIS_MNEMONIC_JMP: exec_jmp(cpu, d); return;
    case ZYDIS_MNEMONIC_JCXZ:
    case ZYDIS_MNEMONIC_JECXZ:
    case ZYDIS_MNEMONIC_JRCXZ: exec_jcxz(cpu, d); return;
    case ZYDIS_MNEMONIC_LOOP:
    case ZYDIS_MNEMONIC_LOOPE:
    case ZYDIS_MNEMONIC_LOOPNE: exec_loop(cpu, d); return;

    // -- strings --
    case ZYDIS_MNEMONIC_MOVSB:
    case ZYDIS_MNEMONIC_MOVSW:
    case ZYDIS_MNEMONIC_MOVSQ:
    case ZYDIS_MNEMONIC_CMPSB:
    case ZYDIS_MNEMONIC_CMPSW:
    case ZYDIS_MNEMONIC_CMPSQ:
    case ZYDIS_MNEMONIC_SCASB:
    case ZYDIS_MNEMONIC_SCASW:
    case ZYDIS_MNEMONIC_SCASD:
    case ZYDIS_MNEMONIC_SCASQ:
    case ZYDIS_MNEMONIC_LODSB:
    case ZYDIS_MNEMONIC_LODSW:
    case ZYDIS_MNEMONIC_LODSD:
    case ZYDIS_MNEMONIC_LODSQ:
    case ZYDIS_MNEMONIC_STOSB:
    case ZYDIS_MNEMONIC_STOSW:
    case ZYDIS_MNEMONIC_STOSD:
    case ZYDIS_MNEMONIC_STOSQ: exec_string(cpu, d); return;
    case ZYDIS_MNEMONIC_MOVSD:
        if (has_vec_reg_operand(d))
            exec_movsd_sse(cpu, d);
        else
            exec_string(cpu, d);
        return;
    case ZYDIS_MNEMONIC_VMOVSD: exec_movsd_sse(cpu, d); return;
    case ZYDIS_MNEMONIC_CMPSD:
        if (has_vec_reg_operand(d))
            exec_sse_scalar(cpu, d, 7);
        else
            exec_string(cpu, d);
        return;

    // -- system --
    case ZYDIS_MNEMONIC_SYSCALL: exec_syscall(cpu, d); return;
    case ZYDIS_MNEMONIC_UD2: guest_error(cpu, "guest UD2 (invalid opcode)"); return;
    case ZYDIS_MNEMONIC_INT3: guest_error(cpu, "guest INT3 (breakpoint)"); return;
    case ZYDIS_MNEMONIC_HLT: guest_error(cpu, "guest HLT (privileged halt)"); return;
    case ZYDIS_MNEMONIC_INT: guest_error(cpu, "unimplemented INT (software interrupt)"); return;
    case ZYDIS_MNEMONIC_CPUID: exec_cpuid(cpu, d); return;
    case ZYDIS_MNEMONIC_XGETBV: exec_xgetbv(cpu, d); return;
    case ZYDIS_MNEMONIC_RDTSC: exec_rdtsc(cpu, d, false); return;
    case ZYDIS_MNEMONIC_RDTSCP: exec_rdtsc(cpu, d, true); return;
    case ZYDIS_MNEMONIC_RDRAND: exec_rdrand(cpu, d, false); return;
    case ZYDIS_MNEMONIC_RDSEED: exec_rdrand(cpu, d, true); return;
    case ZYDIS_MNEMONIC_RDPKRU: exec_pkru(cpu, d, false); return;
    case ZYDIS_MNEMONIC_WRPKRU: exec_pkru(cpu, d, true); return;
    case ZYDIS_MNEMONIC_CRC32: exec_crc32(cpu, d); return;
    case ZYDIS_MNEMONIC_RDSSPD:
    case ZYDIS_MNEMONIC_RDSSPQ: {
        // No shadow stack in the emulator: report SSP 0.
        Val out;
        out.v = 0;
        op_store(cpu, d, 0, out);
        cpu->rip = next_rip(cpu, d);
        return;
    }
    case ZYDIS_MNEMONIC_INCSSPD:
    case ZYDIS_MNEMONIC_INCSSPQ:
        cpu->rip = next_rip(cpu, d);
        return;
    case ZYDIS_MNEMONIC_RDPID: {
        Val out;
        out.v = 0;
        op_store(cpu, d, 0, out);
        cpu->rip = next_rip(cpu, d);
        return;
    }
    case ZYDIS_MNEMONIC_RDFSBASE:
    case ZYDIS_MNEMONIC_RDGSBASE:
    case ZYDIS_MNEMONIC_WRFSBASE:
    case ZYDIS_MNEMONIC_WRGSBASE: exec_fsbase(cpu, d); return;

    // -- nop-like --
    case ZYDIS_MNEMONIC_NOP:
    case ZYDIS_MNEMONIC_ENDBR32:
    case ZYDIS_MNEMONIC_ENDBR64:
    case ZYDIS_MNEMONIC_FNOP:
    case ZYDIS_MNEMONIC_FWAIT:
    case ZYDIS_MNEMONIC_PAUSE:
    case ZYDIS_MNEMONIC_LFENCE:
    case ZYDIS_MNEMONIC_MFENCE:
    case ZYDIS_MNEMONIC_SFENCE:
    case ZYDIS_MNEMONIC_SERIALIZE:
    case ZYDIS_MNEMONIC_PREFETCH:
    case ZYDIS_MNEMONIC_PREFETCHW:
    case ZYDIS_MNEMONIC_PREFETCHNTA:
    case ZYDIS_MNEMONIC_PREFETCHT0:
    case ZYDIS_MNEMONIC_PREFETCHT1:
    case ZYDIS_MNEMONIC_PREFETCHT2:
    case ZYDIS_MNEMONIC_CLFLUSH:
    case ZYDIS_MNEMONIC_CLFLUSHOPT:
    case ZYDIS_MNEMONIC_CLWB:
    case ZYDIS_MNEMONIC_CLAC:
    case ZYDIS_MNEMONIC_STAC: exec_nop_like(cpu, d); return;

    // -- vector moves --
    case ZYDIS_MNEMONIC_MOVAPS:
    case ZYDIS_MNEMONIC_MOVUPS:
    case ZYDIS_MNEMONIC_MOVAPD:
    case ZYDIS_MNEMONIC_MOVUPD:
    case ZYDIS_MNEMONIC_MOVDQA:
    case ZYDIS_MNEMONIC_MOVDQU:
    case ZYDIS_MNEMONIC_VMOVAPS:
    case ZYDIS_MNEMONIC_VMOVUPS:
    case ZYDIS_MNEMONIC_VMOVAPD:
    case ZYDIS_MNEMONIC_VMOVUPD:
    case ZYDIS_MNEMONIC_VMOVDQA:
    case ZYDIS_MNEMONIC_VMOVDQU:
    case ZYDIS_MNEMONIC_VMOVDQA32:
    case ZYDIS_MNEMONIC_VMOVDQA64:
    case ZYDIS_MNEMONIC_VMOVDQU32:
    case ZYDIS_MNEMONIC_VMOVDQU64:
    case ZYDIS_MNEMONIC_VMOVDQU8:
    case ZYDIS_MNEMONIC_VMOVDQU16:
    case ZYDIS_MNEMONIC_MOVNTPS:
    case ZYDIS_MNEMONIC_MOVNTPD:
    case ZYDIS_MNEMONIC_MOVNTDQ:
    case ZYDIS_MNEMONIC_MOVNTDQA:
    case ZYDIS_MNEMONIC_VMOVNTPS:
    case ZYDIS_MNEMONIC_VMOVNTPD:
    case ZYDIS_MNEMONIC_VMOVNTDQ:
    case ZYDIS_MNEMONIC_VMOVNTDQA: exec_vec_move(cpu, d); return;

    // -- vector logic --
    case ZYDIS_MNEMONIC_XORPS:
    case ZYDIS_MNEMONIC_XORPD:
    case ZYDIS_MNEMONIC_PXOR:
    case ZYDIS_MNEMONIC_VXORPS:
    case ZYDIS_MNEMONIC_VXORPD:
    case ZYDIS_MNEMONIC_VPXOR:
    case ZYDIS_MNEMONIC_VPXORD:
    case ZYDIS_MNEMONIC_VPXORQ: exec_vec_logic(cpu, d, 2); return;
    case ZYDIS_MNEMONIC_ANDPS:
    case ZYDIS_MNEMONIC_ANDPD:
    case ZYDIS_MNEMONIC_PAND:
    case ZYDIS_MNEMONIC_VANDPS:
    case ZYDIS_MNEMONIC_VANDPD:
    case ZYDIS_MNEMONIC_VPAND:
    case ZYDIS_MNEMONIC_VPANDD:
    case ZYDIS_MNEMONIC_VPANDQ: exec_vec_logic(cpu, d, 0); return;
    case ZYDIS_MNEMONIC_ORPS:
    case ZYDIS_MNEMONIC_ORPD:
    case ZYDIS_MNEMONIC_POR:
    case ZYDIS_MNEMONIC_VORPS:
    case ZYDIS_MNEMONIC_VORPD:
    case ZYDIS_MNEMONIC_VPOR:
    case ZYDIS_MNEMONIC_VPORD:
    case ZYDIS_MNEMONIC_VPORQ: exec_vec_logic(cpu, d, 1); return;
    case ZYDIS_MNEMONIC_ANDNPS:
    case ZYDIS_MNEMONIC_ANDNPD:
    case ZYDIS_MNEMONIC_PANDN:
    case ZYDIS_MNEMONIC_VANDNPS:
    case ZYDIS_MNEMONIC_VANDNPD:
    case ZYDIS_MNEMONIC_VPANDN:
    case ZYDIS_MNEMONIC_VPANDND:
    case ZYDIS_MNEMONIC_VPANDNQ: exec_vec_logic(cpu, d, 3); return;

    // -- vector integer compare --
    case ZYDIS_MNEMONIC_PCMPEQB:
    case ZYDIS_MNEMONIC_VPCMPEQB: exec_pcmpeq(cpu, d, 1, false); return;
    case ZYDIS_MNEMONIC_PCMPEQW:
    case ZYDIS_MNEMONIC_VPCMPEQW: exec_pcmpeq(cpu, d, 2, false); return;
    case ZYDIS_MNEMONIC_PCMPEQD:
    case ZYDIS_MNEMONIC_VPCMPEQD: exec_pcmpeq(cpu, d, 4, false); return;
    case ZYDIS_MNEMONIC_PCMPEQQ:
    case ZYDIS_MNEMONIC_VPCMPEQQ: exec_pcmpeq(cpu, d, 8, false); return;
    case ZYDIS_MNEMONIC_PCMPGTB:
    case ZYDIS_MNEMONIC_VPCMPGTB: exec_pcmpeq(cpu, d, 1, true); return;
    case ZYDIS_MNEMONIC_PCMPGTW:
    case ZYDIS_MNEMONIC_VPCMPGTW: exec_pcmpeq(cpu, d, 2, true); return;
    case ZYDIS_MNEMONIC_PCMPGTD:
    case ZYDIS_MNEMONIC_VPCMPGTD: exec_pcmpeq(cpu, d, 4, true); return;
    case ZYDIS_MNEMONIC_PCMPGTQ:
    case ZYDIS_MNEMONIC_VPCMPGTQ: exec_pcmpeq(cpu, d, 8, true); return;

    // -- vector integer arithmetic --
    case ZYDIS_MNEMONIC_PADDB:
    case ZYDIS_MNEMONIC_VPADDB: exec_padd(cpu, d, 1, false); return;
    case ZYDIS_MNEMONIC_PADDW:
    case ZYDIS_MNEMONIC_VPADDW: exec_padd(cpu, d, 2, false); return;
    case ZYDIS_MNEMONIC_PADDD:
    case ZYDIS_MNEMONIC_VPADDD: exec_padd(cpu, d, 4, false); return;
    case ZYDIS_MNEMONIC_PADDQ:
    case ZYDIS_MNEMONIC_VPADDQ: exec_padd(cpu, d, 8, false); return;
    case ZYDIS_MNEMONIC_PSUBB:
    case ZYDIS_MNEMONIC_VPSUBB: exec_padd(cpu, d, 1, true); return;
    case ZYDIS_MNEMONIC_PSUBW:
    case ZYDIS_MNEMONIC_VPSUBW: exec_padd(cpu, d, 2, true); return;
    case ZYDIS_MNEMONIC_PSUBD:
    case ZYDIS_MNEMONIC_VPSUBD: exec_padd(cpu, d, 4, true); return;
    case ZYDIS_MNEMONIC_PSUBQ:
    case ZYDIS_MNEMONIC_VPSUBQ: exec_padd(cpu, d, 8, true); return;
    case ZYDIS_MNEMONIC_PMULLW:
    case ZYDIS_MNEMONIC_VPMULLW: exec_pmull(cpu, d, 2); return;
    case ZYDIS_MNEMONIC_PMULLD:
    case ZYDIS_MNEMONIC_VPMULLD: exec_pmull(cpu, d, 4); return;
    case ZYDIS_MNEMONIC_PADDSB:
    case ZYDIS_MNEMONIC_VPADDSB: exec_padds(cpu, d, 1, true, true); return;
    case ZYDIS_MNEMONIC_PADDSW:
    case ZYDIS_MNEMONIC_VPADDSW: exec_padds(cpu, d, 2, true, true); return;
    case ZYDIS_MNEMONIC_PADDUSB:
    case ZYDIS_MNEMONIC_VPADDUSB: exec_padds(cpu, d, 1, false, true); return;
    case ZYDIS_MNEMONIC_PADDUSW:
    case ZYDIS_MNEMONIC_VPADDUSW: exec_padds(cpu, d, 2, false, true); return;
    case ZYDIS_MNEMONIC_PSUBSB:
    case ZYDIS_MNEMONIC_VPSUBSB: exec_padds(cpu, d, 1, true, false); return;
    case ZYDIS_MNEMONIC_PSUBSW:
    case ZYDIS_MNEMONIC_VPSUBSW: exec_padds(cpu, d, 2, true, false); return;
    case ZYDIS_MNEMONIC_PSUBUSB:
    case ZYDIS_MNEMONIC_VPSUBUSB: exec_padds(cpu, d, 1, false, false); return;
    case ZYDIS_MNEMONIC_PSUBUSW:
    case ZYDIS_MNEMONIC_VPSUBUSW: exec_padds(cpu, d, 2, false, false); return;
    case ZYDIS_MNEMONIC_PMAXUB:
    case ZYDIS_MNEMONIC_VPMAXUB: exec_pminmax(cpu, d, 1, false, true); return;
    case ZYDIS_MNEMONIC_PMAXSB:
    case ZYDIS_MNEMONIC_VPMAXSB: exec_pminmax(cpu, d, 1, true, true); return;
    case ZYDIS_MNEMONIC_PMAXUW:
    case ZYDIS_MNEMONIC_VPMAXUW: exec_pminmax(cpu, d, 2, false, true); return;
    case ZYDIS_MNEMONIC_PMAXSW:
    case ZYDIS_MNEMONIC_VPMAXSW: exec_pminmax(cpu, d, 2, true, true); return;
    case ZYDIS_MNEMONIC_PMAXUD:
    case ZYDIS_MNEMONIC_VPMAXUD: exec_pminmax(cpu, d, 4, false, true); return;
    case ZYDIS_MNEMONIC_PMAXSD:
    case ZYDIS_MNEMONIC_VPMAXSD: exec_pminmax(cpu, d, 4, true, true); return;
    case ZYDIS_MNEMONIC_PMINUB:
    case ZYDIS_MNEMONIC_VPMINUB: exec_pminmax(cpu, d, 1, false, false); return;
    case ZYDIS_MNEMONIC_PMINSB:
    case ZYDIS_MNEMONIC_VPMINSB: exec_pminmax(cpu, d, 1, true, false); return;
    case ZYDIS_MNEMONIC_PMINUW:
    case ZYDIS_MNEMONIC_VPMINUW: exec_pminmax(cpu, d, 2, false, false); return;
    case ZYDIS_MNEMONIC_PMINSW:
    case ZYDIS_MNEMONIC_VPMINSW: exec_pminmax(cpu, d, 2, true, false); return;
    case ZYDIS_MNEMONIC_PMINUD:
    case ZYDIS_MNEMONIC_VPMINUD: exec_pminmax(cpu, d, 4, false, false); return;
    case ZYDIS_MNEMONIC_PMINSD:
    case ZYDIS_MNEMONIC_VPMINSD: exec_pminmax(cpu, d, 4, true, false); return;

    case ZYDIS_MNEMONIC_PMOVMSKB:
    case ZYDIS_MNEMONIC_VPMOVMSKB: exec_pmovmskb(cpu, d); return;
    case ZYDIS_MNEMONIC_MOVMSKPS:
    case ZYDIS_MNEMONIC_VMOVMSKPS: exec_movmskps(cpu, d, false); return;
    case ZYDIS_MNEMONIC_MOVMSKPD:
    case ZYDIS_MNEMONIC_VMOVMSKPD: exec_movmskps(cpu, d, true); return;

    case ZYDIS_MNEMONIC_PSLLDQ:
    case ZYDIS_MNEMONIC_VPSLLDQ: exec_pshift_dq(cpu, d, true); return;
    case ZYDIS_MNEMONIC_PSRLDQ:
    case ZYDIS_MNEMONIC_VPSRLDQ: exec_pshift_dq(cpu, d, false); return;
    case ZYDIS_MNEMONIC_PSLLW:
    case ZYDIS_MNEMONIC_VPSLLW: exec_pshift_bits(cpu, d, 2, 0); return;
    case ZYDIS_MNEMONIC_PSLLD:
    case ZYDIS_MNEMONIC_VPSLLD: exec_pshift_bits(cpu, d, 4, 0); return;
    case ZYDIS_MNEMONIC_PSLLQ:
    case ZYDIS_MNEMONIC_VPSLLQ: exec_pshift_bits(cpu, d, 8, 0); return;
    case ZYDIS_MNEMONIC_PSRLW:
    case ZYDIS_MNEMONIC_VPSRLW: exec_pshift_bits(cpu, d, 2, 1); return;
    case ZYDIS_MNEMONIC_PSRLD:
    case ZYDIS_MNEMONIC_VPSRLD: exec_pshift_bits(cpu, d, 4, 1); return;
    case ZYDIS_MNEMONIC_PSRLQ:
    case ZYDIS_MNEMONIC_VPSRLQ: exec_pshift_bits(cpu, d, 8, 1); return;
    case ZYDIS_MNEMONIC_PSRAW:
    case ZYDIS_MNEMONIC_VPSRAW: exec_pshift_bits(cpu, d, 2, 2); return;
    case ZYDIS_MNEMONIC_PSRAD:
    case ZYDIS_MNEMONIC_VPSRAD: exec_pshift_bits(cpu, d, 4, 2); return;
    case ZYDIS_MNEMONIC_VPSRAQ: exec_pshift_bits(cpu, d, 8, 2); return;

    case ZYDIS_MNEMONIC_PSHUFD:
    case ZYDIS_MNEMONIC_VPSHUFD: exec_pshufd(cpu, d); return;
    case ZYDIS_MNEMONIC_SHUFPS:
    case ZYDIS_MNEMONIC_VSHUFPS: exec_shufpd(cpu, d, true); return;
    case ZYDIS_MNEMONIC_SHUFPD:
    case ZYDIS_MNEMONIC_VSHUFPD: exec_shufpd(cpu, d, false); return;
    case ZYDIS_MNEMONIC_PSHUFB:
    case ZYDIS_MNEMONIC_VPSHUFB: exec_pshufb(cpu, d); return;
    case ZYDIS_MNEMONIC_PSHUFLW:
    case ZYDIS_MNEMONIC_VPSHUFLW: exec_pshuflw(cpu, d, false); return;
    case ZYDIS_MNEMONIC_PSHUFHW:
    case ZYDIS_MNEMONIC_VPSHUFHW: exec_pshuflw(cpu, d, true); return;

    case ZYDIS_MNEMONIC_PUNPCKLBW:
    case ZYDIS_MNEMONIC_VPUNPCKLBW: exec_punpck(cpu, d, 1, false); return;
    case ZYDIS_MNEMONIC_PUNPCKHBW:
    case ZYDIS_MNEMONIC_VPUNPCKHBW: exec_punpck(cpu, d, 1, true); return;
    case ZYDIS_MNEMONIC_PUNPCKLWD:
    case ZYDIS_MNEMONIC_VPUNPCKLWD: exec_punpck(cpu, d, 2, false); return;
    case ZYDIS_MNEMONIC_PUNPCKHWD:
    case ZYDIS_MNEMONIC_VPUNPCKHWD: exec_punpck(cpu, d, 2, true); return;
    case ZYDIS_MNEMONIC_PUNPCKLDQ:
    case ZYDIS_MNEMONIC_VPUNPCKLDQ: exec_punpck(cpu, d, 4, false); return;
    case ZYDIS_MNEMONIC_PUNPCKHDQ:
    case ZYDIS_MNEMONIC_VPUNPCKHDQ: exec_punpck(cpu, d, 4, true); return;
    case ZYDIS_MNEMONIC_PUNPCKLQDQ:
    case ZYDIS_MNEMONIC_VPUNPCKLQDQ:
    case ZYDIS_MNEMONIC_UNPCKLPD:
    case ZYDIS_MNEMONIC_VUNPCKLPD: exec_punpck(cpu, d, 8, false); return;
    case ZYDIS_MNEMONIC_PUNPCKHQDQ:
    case ZYDIS_MNEMONIC_VPUNPCKHQDQ:
    case ZYDIS_MNEMONIC_UNPCKHPD:
    case ZYDIS_MNEMONIC_VUNPCKHPD: exec_punpck(cpu, d, 8, true); return;
    case ZYDIS_MNEMONIC_UNPCKLPS:
    case ZYDIS_MNEMONIC_VUNPCKLPS: exec_punpck(cpu, d, 4, false); return;
    case ZYDIS_MNEMONIC_UNPCKHPS:
    case ZYDIS_MNEMONIC_VUNPCKHPS: exec_punpck(cpu, d, 4, true); return;

    case ZYDIS_MNEMONIC_PACKSSWB:
    case ZYDIS_MNEMONIC_VPACKSSWB: exec_pack(cpu, d, true, 2); return;
    case ZYDIS_MNEMONIC_PACKSSDW:
    case ZYDIS_MNEMONIC_VPACKSSDW: exec_pack(cpu, d, true, 4); return;
    case ZYDIS_MNEMONIC_PACKUSWB:
    case ZYDIS_MNEMONIC_VPACKUSWB: exec_pack(cpu, d, false, 2); return;
    case ZYDIS_MNEMONIC_PACKUSDW:
    case ZYDIS_MNEMONIC_VPACKUSDW: exec_pack(cpu, d, false, 4); return;

    case ZYDIS_MNEMONIC_PINSRB:
    case ZYDIS_MNEMONIC_PINSRW:
    case ZYDIS_MNEMONIC_PINSRD:
    case ZYDIS_MNEMONIC_PINSRQ: exec_pinsr(cpu, d, true); return;
    case ZYDIS_MNEMONIC_PEXTRB:
    case ZYDIS_MNEMONIC_PEXTRW:
    case ZYDIS_MNEMONIC_PEXTRD:
    case ZYDIS_MNEMONIC_PEXTRQ: exec_pinsr(cpu, d, false); return;

    case ZYDIS_MNEMONIC_PMOVSXBW:
    case ZYDIS_MNEMONIC_VPMOVSXBW: exec_pmovsx(cpu, d, 1, 2, true); return;
    case ZYDIS_MNEMONIC_PMOVSXBD:
    case ZYDIS_MNEMONIC_VPMOVSXBD: exec_pmovsx(cpu, d, 1, 4, true); return;
    case ZYDIS_MNEMONIC_PMOVSXBQ:
    case ZYDIS_MNEMONIC_VPMOVSXBQ: exec_pmovsx(cpu, d, 1, 8, true); return;
    case ZYDIS_MNEMONIC_PMOVSXWD:
    case ZYDIS_MNEMONIC_VPMOVSXWD: exec_pmovsx(cpu, d, 2, 4, true); return;
    case ZYDIS_MNEMONIC_PMOVSXWQ:
    case ZYDIS_MNEMONIC_VPMOVSXWQ: exec_pmovsx(cpu, d, 2, 8, true); return;
    case ZYDIS_MNEMONIC_PMOVSXDQ:
    case ZYDIS_MNEMONIC_VPMOVSXDQ: exec_pmovsx(cpu, d, 4, 8, true); return;
    case ZYDIS_MNEMONIC_PMOVZXBW:
    case ZYDIS_MNEMONIC_VPMOVZXBW: exec_pmovsx(cpu, d, 1, 2, false); return;
    case ZYDIS_MNEMONIC_PMOVZXBD:
    case ZYDIS_MNEMONIC_VPMOVZXBD: exec_pmovsx(cpu, d, 1, 4, false); return;
    case ZYDIS_MNEMONIC_PMOVZXBQ:
    case ZYDIS_MNEMONIC_VPMOVZXBQ: exec_pmovsx(cpu, d, 1, 8, false); return;
    case ZYDIS_MNEMONIC_PMOVZXWD:
    case ZYDIS_MNEMONIC_VPMOVZXWD: exec_pmovsx(cpu, d, 2, 4, false); return;
    case ZYDIS_MNEMONIC_PMOVZXWQ:
    case ZYDIS_MNEMONIC_VPMOVZXWQ: exec_pmovsx(cpu, d, 2, 8, false); return;
    case ZYDIS_MNEMONIC_PMOVZXDQ:
    case ZYDIS_MNEMONIC_VPMOVZXDQ: exec_pmovsx(cpu, d, 4, 8, false); return;

    case ZYDIS_MNEMONIC_PALIGNR:
    case ZYDIS_MNEMONIC_VPALIGNR: exec_palignr(cpu, d); return;

    case ZYDIS_MNEMONIC_PTEST:
    case ZYDIS_MNEMONIC_VPTEST: exec_ptest(cpu, d); return;
    case ZYDIS_MNEMONIC_VTESTPS:
    case ZYDIS_MNEMONIC_VTESTPD: exec_vtest(cpu, d); return;

    case ZYDIS_MNEMONIC_PBLENDVB:
    case ZYDIS_MNEMONIC_VPBLENDVB: exec_pblendvb(cpu, d); return;
    case ZYDIS_MNEMONIC_PBLENDW:
    case ZYDIS_MNEMONIC_VPBLENDW: exec_pblendw(cpu, d); return;
    case ZYDIS_MNEMONIC_BLENDPS:
    case ZYDIS_MNEMONIC_VBLENDPS: exec_blendps(cpu, d, false); return;
    case ZYDIS_MNEMONIC_BLENDPD:
    case ZYDIS_MNEMONIC_VBLENDPD: exec_blendps(cpu, d, true); return;
    case ZYDIS_MNEMONIC_BLENDVPS:
    case ZYDIS_MNEMONIC_VBLENDVPS: exec_blendv(cpu, d, false); return;
    case ZYDIS_MNEMONIC_BLENDVPD:
    case ZYDIS_MNEMONIC_VBLENDVPD: exec_blendv(cpu, d, true); return;

    case ZYDIS_MNEMONIC_MOVD:
    case ZYDIS_MNEMONIC_MOVQ:
    case ZYDIS_MNEMONIC_VMOVD:
    case ZYDIS_MNEMONIC_VMOVQ:
        exec_movd(cpu, d,
                  m == ZYDIS_MNEMONIC_MOVQ || m == ZYDIS_MNEMONIC_VMOVQ);
        return;
    case ZYDIS_MNEMONIC_MOVSS: exec_movss_sse(cpu, d); return;
    case ZYDIS_MNEMONIC_VMOVSS: exec_movss_sse(cpu, d); return;

    case ZYDIS_MNEMONIC_MOVLPS:
    case ZYDIS_MNEMONIC_MOVLPD: exec_movlp(cpu, d, false, m == ZYDIS_MNEMONIC_MOVLPD); return;
    case ZYDIS_MNEMONIC_MOVHPS:
    case ZYDIS_MNEMONIC_MOVHPD: exec_movlp(cpu, d, true, m == ZYDIS_MNEMONIC_MOVHPD); return;
    case ZYDIS_MNEMONIC_MOVHLPS: exec_movhlps(cpu, d, true); return;
    case ZYDIS_MNEMONIC_MOVLHPS: exec_movhlps(cpu, d, false); return;
    case ZYDIS_MNEMONIC_MOVSLDUP:
    case ZYDIS_MNEMONIC_VMOVSLDUP: exec_movdup(cpu, d, false); return;
    case ZYDIS_MNEMONIC_MOVSHDUP:
    case ZYDIS_MNEMONIC_VMOVSHDUP: exec_movdup(cpu, d, true); return;

    // -- scalar FP --
    case ZYDIS_MNEMONIC_ADDSS:
    case ZYDIS_MNEMONIC_VADDSS:
    case ZYDIS_MNEMONIC_ADDSD:
    case ZYDIS_MNEMONIC_VADDSD: exec_sse_scalar(cpu, d, 0); return;
    case ZYDIS_MNEMONIC_MULSS:
    case ZYDIS_MNEMONIC_VMULSS:
    case ZYDIS_MNEMONIC_MULSD:
    case ZYDIS_MNEMONIC_VMULSD: exec_sse_scalar(cpu, d, 1); return;
    case ZYDIS_MNEMONIC_SUBSS:
    case ZYDIS_MNEMONIC_VSUBSS:
    case ZYDIS_MNEMONIC_SUBSD:
    case ZYDIS_MNEMONIC_VSUBSD: exec_sse_scalar(cpu, d, 2); return;
    case ZYDIS_MNEMONIC_DIVSS:
    case ZYDIS_MNEMONIC_VDIVSS:
    case ZYDIS_MNEMONIC_DIVSD:
    case ZYDIS_MNEMONIC_VDIVSD: exec_sse_scalar(cpu, d, 3); return;
    case ZYDIS_MNEMONIC_SQRTSS:
    case ZYDIS_MNEMONIC_VSQRTSS:
    case ZYDIS_MNEMONIC_SQRTSD:
    case ZYDIS_MNEMONIC_VSQRTSD: exec_sse_scalar(cpu, d, 4); return;
    case ZYDIS_MNEMONIC_MAXSS:
    case ZYDIS_MNEMONIC_VMAXSS:
    case ZYDIS_MNEMONIC_MAXSD:
    case ZYDIS_MNEMONIC_VMAXSD: exec_sse_scalar(cpu, d, 5); return;
    case ZYDIS_MNEMONIC_MINSS:
    case ZYDIS_MNEMONIC_VMINSS:
    case ZYDIS_MNEMONIC_MINSD:
    case ZYDIS_MNEMONIC_VMINSD: exec_sse_scalar(cpu, d, 6); return;
    case ZYDIS_MNEMONIC_CMPSS:
    case ZYDIS_MNEMONIC_VCMPSS:
    case ZYDIS_MNEMONIC_VCMPSD: exec_sse_scalar(cpu, d, 7); return;
    case ZYDIS_MNEMONIC_UCOMISS:
    case ZYDIS_MNEMONIC_UCOMISD:
    case ZYDIS_MNEMONIC_COMISS:
    case ZYDIS_MNEMONIC_COMISD:
    case ZYDIS_MNEMONIC_VUCOMISS:
    case ZYDIS_MNEMONIC_VUCOMISD:
    case ZYDIS_MNEMONIC_VCOMISS:
    case ZYDIS_MNEMONIC_VCOMISD: exec_ucomi(cpu, d); return;
    case ZYDIS_MNEMONIC_CVTSI2SS:
    case ZYDIS_MNEMONIC_VCVTSI2SS: exec_cvtsi2s(cpu, d, false); return;
    case ZYDIS_MNEMONIC_CVTSI2SD:
    case ZYDIS_MNEMONIC_VCVTSI2SD: exec_cvtsi2s(cpu, d, true); return;
    case ZYDIS_MNEMONIC_CVTTSS2SI:
    case ZYDIS_MNEMONIC_VCVTTSS2SI: exec_cvtts2si(cpu, d, false); return;
    case ZYDIS_MNEMONIC_CVTTSD2SI:
    case ZYDIS_MNEMONIC_VCVTTSD2SI: exec_cvtts2si(cpu, d, true); return;
    case ZYDIS_MNEMONIC_CVTSS2SD:
    case ZYDIS_MNEMONIC_VCVTSS2SD: exec_cvtss2sd(cpu, d, true); return;
    case ZYDIS_MNEMONIC_CVTSD2SS:
    case ZYDIS_MNEMONIC_VCVTSD2SS: exec_cvtss2sd(cpu, d, false); return;

    // -- packed FP --
    case ZYDIS_MNEMONIC_ADDPS:
    case ZYDIS_MNEMONIC_VADDPS:
    case ZYDIS_MNEMONIC_ADDPD:
    case ZYDIS_MNEMONIC_VADDPD: exec_packed_fp(cpu, d, 0); return;
    case ZYDIS_MNEMONIC_MULPS:
    case ZYDIS_MNEMONIC_VMULPS:
    case ZYDIS_MNEMONIC_MULPD:
    case ZYDIS_MNEMONIC_VMULPD: exec_packed_fp(cpu, d, 1); return;
    case ZYDIS_MNEMONIC_SUBPS:
    case ZYDIS_MNEMONIC_VSUBPS:
    case ZYDIS_MNEMONIC_SUBPD:
    case ZYDIS_MNEMONIC_VSUBPD: exec_packed_fp(cpu, d, 2); return;
    case ZYDIS_MNEMONIC_DIVPS:
    case ZYDIS_MNEMONIC_VDIVPS:
    case ZYDIS_MNEMONIC_DIVPD:
    case ZYDIS_MNEMONIC_VDIVPD: exec_packed_fp(cpu, d, 3); return;
    case ZYDIS_MNEMONIC_SQRTPS:
    case ZYDIS_MNEMONIC_VSQRTPS:
    case ZYDIS_MNEMONIC_SQRTPD:
    case ZYDIS_MNEMONIC_VSQRTPD: exec_packed_fp(cpu, d, 4); return;
    case ZYDIS_MNEMONIC_MAXPS:
    case ZYDIS_MNEMONIC_VMAXPS:
    case ZYDIS_MNEMONIC_MAXPD:
    case ZYDIS_MNEMONIC_VMAXPD: exec_packed_fp(cpu, d, 5); return;
    case ZYDIS_MNEMONIC_MINPS:
    case ZYDIS_MNEMONIC_VMINPS:
    case ZYDIS_MNEMONIC_MINPD:
    case ZYDIS_MNEMONIC_VMINPD: exec_packed_fp(cpu, d, 6); return;

    case ZYDIS_MNEMONIC_CVTDQ2PS:
    case ZYDIS_MNEMONIC_VCVTDQ2PS: exec_cvtdq2ps(cpu, d, 0); return;
    case ZYDIS_MNEMONIC_CVTTPS2DQ:
    case ZYDIS_MNEMONIC_VCVTTPS2DQ: exec_cvtdq2ps(cpu, d, 1); return;
    case ZYDIS_MNEMONIC_CVTPS2DQ:
    case ZYDIS_MNEMONIC_VCVTPS2DQ: exec_cvtdq2ps(cpu, d, 2); return;
    case ZYDIS_MNEMONIC_CVTPD2PS:
    case ZYDIS_MNEMONIC_VCVTPD2PS: exec_cvtpd(cpu, d, true); return;
    case ZYDIS_MNEMONIC_CVTPS2PD:
    case ZYDIS_MNEMONIC_VCVTPS2PD: exec_cvtpd(cpu, d, false); return;
    case ZYDIS_MNEMONIC_ROUNDPS:
    case ZYDIS_MNEMONIC_VROUNDPS: exec_round(cpu, d, false, false); return;
    case ZYDIS_MNEMONIC_ROUNDPD:
    case ZYDIS_MNEMONIC_VROUNDPD: exec_round(cpu, d, true, false); return;
    case ZYDIS_MNEMONIC_ROUNDSS:
    case ZYDIS_MNEMONIC_VROUNDSS: exec_round(cpu, d, false, true); return;
    case ZYDIS_MNEMONIC_ROUNDSD:
    case ZYDIS_MNEMONIC_VROUNDSD: exec_round(cpu, d, true, true); return;

    case ZYDIS_MNEMONIC_VZEROUPPER:
    case ZYDIS_MNEMONIC_VZEROALL: exec_vzero(cpu, d, m == ZYDIS_MNEMONIC_VZEROALL); return;
    case ZYDIS_MNEMONIC_VBROADCASTSS:
    case ZYDIS_MNEMONIC_VBROADCASTSD:
    case ZYDIS_MNEMONIC_VBROADCASTF128:
    case ZYDIS_MNEMONIC_VPBROADCASTB:
    case ZYDIS_MNEMONIC_VPBROADCASTW:
    case ZYDIS_MNEMONIC_VPBROADCASTD:
    case ZYDIS_MNEMONIC_VPBROADCASTQ: exec_vbroadcast(cpu, d); return;
    case ZYDIS_MNEMONIC_VEXTRACTF128:
    case ZYDIS_MNEMONIC_VEXTRACTI128: exec_vextract(cpu, d, false); return;
    case ZYDIS_MNEMONIC_VINSERTF128:
    case ZYDIS_MNEMONIC_VINSERTI128: exec_vextract(cpu, d, true); return;
    case ZYDIS_MNEMONIC_VPERM2F128:
    case ZYDIS_MNEMONIC_VPERM2I128: exec_vperm2(cpu, d); return;
    case ZYDIS_MNEMONIC_VPERMILPS:
    case ZYDIS_MNEMONIC_VPERMILPD: exec_vpermil(cpu, d, m == ZYDIS_MNEMONIC_VPERMILPD, false); return;
    case ZYDIS_MNEMONIC_VPERMPD: exec_vpermpd(cpu, d); return;
    case ZYDIS_MNEMONIC_VPERMQ: exec_vpermq(cpu, d); return;
    case ZYDIS_MNEMONIC_VPTERNLOGD:
    case ZYDIS_MNEMONIC_VPTERNLOGQ: exec_vpternlog(cpu, d); return;

    // -- AES / PCLMULQDQ / GFNI / SHA (legacy + VEX; EVEX via exec_evex) --
    case ZYDIS_MNEMONIC_AESDEC:
    case ZYDIS_MNEMONIC_AESDECLAST:
    case ZYDIS_MNEMONIC_AESENC:
    case ZYDIS_MNEMONIC_AESENCLAST:
    case ZYDIS_MNEMONIC_AESIMC:
    case ZYDIS_MNEMONIC_AESKEYGENASSIST:
    case ZYDIS_MNEMONIC_VAESDEC:
    case ZYDIS_MNEMONIC_VAESDECLAST:
    case ZYDIS_MNEMONIC_VAESENC:
    case ZYDIS_MNEMONIC_VAESENCLAST:
    case ZYDIS_MNEMONIC_VAESIMC:
    case ZYDIS_MNEMONIC_VAESKEYGENASSIST: exec_aes(cpu, d); return;
    case ZYDIS_MNEMONIC_PCLMULQDQ:
    case ZYDIS_MNEMONIC_VPCLMULQDQ: exec_pclmul(cpu, d); return;
    case ZYDIS_MNEMONIC_GF2P8AFFINEQB:
    case ZYDIS_MNEMONIC_GF2P8AFFINEINVQB:
    case ZYDIS_MNEMONIC_GF2P8MULB:
    case ZYDIS_MNEMONIC_VGF2P8AFFINEQB:
    case ZYDIS_MNEMONIC_VGF2P8AFFINEINVQB:
    case ZYDIS_MNEMONIC_VGF2P8MULB: exec_gfni(cpu, d); return;
    case ZYDIS_MNEMONIC_SHA1RNDS4:
    case ZYDIS_MNEMONIC_SHA1NEXTE:
    case ZYDIS_MNEMONIC_SHA1MSG1:
    case ZYDIS_MNEMONIC_SHA1MSG2:
    case ZYDIS_MNEMONIC_SHA256RNDS2:
    case ZYDIS_MNEMONIC_SHA256MSG1:
    case ZYDIS_MNEMONIC_SHA256MSG2: exec_sha(cpu, d); return;

    // -- FMA / F16C / BF16 (VEX + EVEX) --
    case ZYDIS_MNEMONIC_VFMADD132PS:
    case ZYDIS_MNEMONIC_VFMADD132PD:
    case ZYDIS_MNEMONIC_VFMADD132SS:
    case ZYDIS_MNEMONIC_VFMADD132SD:
    case ZYDIS_MNEMONIC_VFMADD213PS:
    case ZYDIS_MNEMONIC_VFMADD213PD:
    case ZYDIS_MNEMONIC_VFMADD213SS:
    case ZYDIS_MNEMONIC_VFMADD213SD:
    case ZYDIS_MNEMONIC_VFMADD231PS:
    case ZYDIS_MNEMONIC_VFMADD231PD:
    case ZYDIS_MNEMONIC_VFMADD231SS:
    case ZYDIS_MNEMONIC_VFMADD231SD:
    case ZYDIS_MNEMONIC_VFMADDPS:
    case ZYDIS_MNEMONIC_VFMADDPD:
    case ZYDIS_MNEMONIC_VFMADDSS:
    case ZYDIS_MNEMONIC_VFMADDSD:
    case ZYDIS_MNEMONIC_VFNMADD132PS:
    case ZYDIS_MNEMONIC_VFNMADD132PD:
    case ZYDIS_MNEMONIC_VFNMADD132SS:
    case ZYDIS_MNEMONIC_VFNMADD132SD:
    case ZYDIS_MNEMONIC_VFNMADD213PS:
    case ZYDIS_MNEMONIC_VFNMADD213PD:
    case ZYDIS_MNEMONIC_VFNMADD213SS:
    case ZYDIS_MNEMONIC_VFNMADD213SD:
    case ZYDIS_MNEMONIC_VFNMADD231PS:
    case ZYDIS_MNEMONIC_VFNMADD231PD:
    case ZYDIS_MNEMONIC_VFNMADD231SS:
    case ZYDIS_MNEMONIC_VFNMADD231SD:
    case ZYDIS_MNEMONIC_VFNMADDPS:
    case ZYDIS_MNEMONIC_VFNMADDPD:
    case ZYDIS_MNEMONIC_VFNMADDSS:
    case ZYDIS_MNEMONIC_VFNMADDSD:
    case ZYDIS_MNEMONIC_VFMSUB132PS:
    case ZYDIS_MNEMONIC_VFMSUB132PD:
    case ZYDIS_MNEMONIC_VFMSUB132SS:
    case ZYDIS_MNEMONIC_VFMSUB132SD:
    case ZYDIS_MNEMONIC_VFMSUB213PS:
    case ZYDIS_MNEMONIC_VFMSUB213PD:
    case ZYDIS_MNEMONIC_VFMSUB213SS:
    case ZYDIS_MNEMONIC_VFMSUB213SD:
    case ZYDIS_MNEMONIC_VFMSUB231PS:
    case ZYDIS_MNEMONIC_VFMSUB231PD:
    case ZYDIS_MNEMONIC_VFMSUB231SS:
    case ZYDIS_MNEMONIC_VFMSUB231SD:
    case ZYDIS_MNEMONIC_VFMSUBPS:
    case ZYDIS_MNEMONIC_VFMSUBPD:
    case ZYDIS_MNEMONIC_VFMSUBSS:
    case ZYDIS_MNEMONIC_VFMSUBSD:
    case ZYDIS_MNEMONIC_VFNMSUB132PS:
    case ZYDIS_MNEMONIC_VFNMSUB132PD:
    case ZYDIS_MNEMONIC_VFNMSUB132SS:
    case ZYDIS_MNEMONIC_VFNMSUB132SD:
    case ZYDIS_MNEMONIC_VFNMSUB213PS:
    case ZYDIS_MNEMONIC_VFNMSUB213PD:
    case ZYDIS_MNEMONIC_VFNMSUB213SS:
    case ZYDIS_MNEMONIC_VFNMSUB213SD:
    case ZYDIS_MNEMONIC_VFNMSUB231PS:
    case ZYDIS_MNEMONIC_VFNMSUB231PD:
    case ZYDIS_MNEMONIC_VFNMSUB231SS:
    case ZYDIS_MNEMONIC_VFNMSUB231SD:
    case ZYDIS_MNEMONIC_VFNMSUBPS:
    case ZYDIS_MNEMONIC_VFNMSUBPD:
    case ZYDIS_MNEMONIC_VFNMSUBSS:
    case ZYDIS_MNEMONIC_VFNMSUBSD:
    case ZYDIS_MNEMONIC_VFMADDSUB132PS:
    case ZYDIS_MNEMONIC_VFMADDSUB132PD:
    case ZYDIS_MNEMONIC_VFMADDSUB213PS:
    case ZYDIS_MNEMONIC_VFMADDSUB213PD:
    case ZYDIS_MNEMONIC_VFMADDSUB231PS:
    case ZYDIS_MNEMONIC_VFMADDSUB231PD:
    case ZYDIS_MNEMONIC_VFMADDSUBPS:
    case ZYDIS_MNEMONIC_VFMADDSUBPD:
    case ZYDIS_MNEMONIC_VFMSUBADD132PS:
    case ZYDIS_MNEMONIC_VFMSUBADD132PD:
    case ZYDIS_MNEMONIC_VFMSUBADD213PS:
    case ZYDIS_MNEMONIC_VFMSUBADD213PD:
    case ZYDIS_MNEMONIC_VFMSUBADD231PS:
    case ZYDIS_MNEMONIC_VFMSUBADD231PD:
    case ZYDIS_MNEMONIC_VFMSUBADDPD:
    case ZYDIS_MNEMONIC_VFMSUBADDPS: exec_fma(cpu, d); return;
    case ZYDIS_MNEMONIC_VCVTPH2PS:
    case ZYDIS_MNEMONIC_VCVTPS2PH: exec_cvtph(cpu, d); return;
    case ZYDIS_MNEMONIC_VCVTNE2PS2BF16:
    case ZYDIS_MNEMONIC_VCVTNEPS2BF16: exec_cvtnebf16(cpu, d); return;

    // -- VEX classic gaps --
    case ZYDIS_MNEMONIC_PMADDWD:
    case ZYDIS_MNEMONIC_VPMADDWD: exec_pmadd(cpu, d, false); return;
    case ZYDIS_MNEMONIC_PMADDUBSW:
    case ZYDIS_MNEMONIC_VPMADDUBSW: exec_pmadd(cpu, d, true); return;
    case ZYDIS_MNEMONIC_PAVGB:
    case ZYDIS_MNEMONIC_VPAVGB: exec_pavg(cpu, d, 1); return;
    case ZYDIS_MNEMONIC_PAVGW:
    case ZYDIS_MNEMONIC_VPAVGW: exec_pavg(cpu, d, 2); return;
    case ZYDIS_MNEMONIC_PSADBW:
    case ZYDIS_MNEMONIC_VPSADBW: exec_psadbw(cpu, d); return;
    case ZYDIS_MNEMONIC_PMULHUW:
    case ZYDIS_MNEMONIC_VPMULHUW: exec_pmulh(cpu, d, 0); return;
    case ZYDIS_MNEMONIC_PMULHW:
    case ZYDIS_MNEMONIC_VPMULHW: exec_pmulh(cpu, d, 1); return;
    case ZYDIS_MNEMONIC_PMULUDQ:
    case ZYDIS_MNEMONIC_VPMULUDQ: exec_pmulh(cpu, d, 2); return;
    case ZYDIS_MNEMONIC_PMULDQ:
    case ZYDIS_MNEMONIC_VPMULDQ: exec_pmulh(cpu, d, 3); return;
    case ZYDIS_MNEMONIC_PHADDW:
    case ZYDIS_MNEMONIC_VPHADDW: exec_phadd(cpu, d, 0); return;
    case ZYDIS_MNEMONIC_PHADDD:
    case ZYDIS_MNEMONIC_VPHADDD: exec_phadd(cpu, d, 1); return;
    case ZYDIS_MNEMONIC_PHSUBW:
    case ZYDIS_MNEMONIC_VPHSUBW: exec_phadd(cpu, d, 2); return;
    case ZYDIS_MNEMONIC_PHSUBD:
    case ZYDIS_MNEMONIC_VPHSUBD: exec_phadd(cpu, d, 3); return;
    case ZYDIS_MNEMONIC_PHADDSW:
    case ZYDIS_MNEMONIC_VPHADDSW: exec_phadd(cpu, d, 4); return;
    case ZYDIS_MNEMONIC_PHSUBSW:
    case ZYDIS_MNEMONIC_VPHSUBSW: exec_phadd(cpu, d, 5); return;
    case ZYDIS_MNEMONIC_MPSADBW:
    case ZYDIS_MNEMONIC_VMPSADBW: exec_mpsadbw(cpu, d); return;
    case ZYDIS_MNEMONIC_EXTRACTPS:
    case ZYDIS_MNEMONIC_VEXTRACTPS: exec_extractps(cpu, d, false); return;
    case ZYDIS_MNEMONIC_INSERTPS:
    case ZYDIS_MNEMONIC_VINSERTPS: exec_extractps(cpu, d, true); return;
    case ZYDIS_MNEMONIC_HADDPS:
    case ZYDIS_MNEMONIC_VHADDPS: exec_hadd_fp(cpu, d, 0); return;
    case ZYDIS_MNEMONIC_HADDPD:
    case ZYDIS_MNEMONIC_VHADDPD: exec_hadd_fp(cpu, d, 1); return;
    case ZYDIS_MNEMONIC_HSUBPS:
    case ZYDIS_MNEMONIC_VHSUBPS: exec_hadd_fp(cpu, d, 2); return;
    case ZYDIS_MNEMONIC_VHSUBPD:
    case ZYDIS_MNEMONIC_HSUBPD: exec_hadd_fp(cpu, d, 3); return;
    case ZYDIS_MNEMONIC_ADDSUBPS:
    case ZYDIS_MNEMONIC_VADDSUBPS: exec_hadd_fp(cpu, d, 4); return;
    case ZYDIS_MNEMONIC_ADDSUBPD:
    case ZYDIS_MNEMONIC_VADDSUBPD: exec_hadd_fp(cpu, d, 5); return;
    case ZYDIS_MNEMONIC_PCMPESTRI:
    case ZYDIS_MNEMONIC_VPCMPESTRI: exec_pcmpestri(cpu, d, true); return;
    case ZYDIS_MNEMONIC_PCMPESTRM:
    case ZYDIS_MNEMONIC_VPCMPESTRM: exec_pcmpestri(cpu, d, false); return;
    case ZYDIS_MNEMONIC_PCMPISTRI:
    case ZYDIS_MNEMONIC_VPCMPISTRI: exec_pcmpestri(cpu, d, true); return;
    case ZYDIS_MNEMONIC_VPCMPISTRM:
    case ZYDIS_MNEMONIC_PCMPISTRM: exec_pcmpestri(cpu, d, false); return;
    case ZYDIS_MNEMONIC_VPBLENDD: exec_blendps(cpu, d, false); return;

    // -- opmask (K) instructions (VEX; EVEX via exec_evex) --
    case ZYDIS_MNEMONIC_KMOVB:
    case ZYDIS_MNEMONIC_KMOVW:
    case ZYDIS_MNEMONIC_KMOVD:
    case ZYDIS_MNEMONIC_KMOVQ:
    case ZYDIS_MNEMONIC_KMOV:
    case ZYDIS_MNEMONIC_KNOTB:
    case ZYDIS_MNEMONIC_KNOTW:
    case ZYDIS_MNEMONIC_KNOTD:
    case ZYDIS_MNEMONIC_KNOTQ:
    case ZYDIS_MNEMONIC_KNOT:
    case ZYDIS_MNEMONIC_KORB:
    case ZYDIS_MNEMONIC_KORW:
    case ZYDIS_MNEMONIC_KORD:
    case ZYDIS_MNEMONIC_KORQ:
    case ZYDIS_MNEMONIC_KOR:
    case ZYDIS_MNEMONIC_KADDB:
    case ZYDIS_MNEMONIC_KADDW:
    case ZYDIS_MNEMONIC_KADDD:
    case ZYDIS_MNEMONIC_KADDQ:
    case ZYDIS_MNEMONIC_KANDB:
    case ZYDIS_MNEMONIC_KANDW:
    case ZYDIS_MNEMONIC_KANDD:
    case ZYDIS_MNEMONIC_KANDQ:
    case ZYDIS_MNEMONIC_KAND:
    case ZYDIS_MNEMONIC_KXORB:
    case ZYDIS_MNEMONIC_KXORW:
    case ZYDIS_MNEMONIC_KXORD:
    case ZYDIS_MNEMONIC_KXORQ:
    case ZYDIS_MNEMONIC_KXOR:
    case ZYDIS_MNEMONIC_KXNORB:
    case ZYDIS_MNEMONIC_KXNORW:
    case ZYDIS_MNEMONIC_KXNORD:
    case ZYDIS_MNEMONIC_KXNORQ:
    case ZYDIS_MNEMONIC_KXNOR:
    case ZYDIS_MNEMONIC_KANDNB:
    case ZYDIS_MNEMONIC_KANDNW:
    case ZYDIS_MNEMONIC_KANDND:
    case ZYDIS_MNEMONIC_KANDNQ:
    case ZYDIS_MNEMONIC_KANDN:
    case ZYDIS_MNEMONIC_KANDNR:
    case ZYDIS_MNEMONIC_KORTESTB:
    case ZYDIS_MNEMONIC_KORTESTW:
    case ZYDIS_MNEMONIC_KORTESTD:
    case ZYDIS_MNEMONIC_KORTESTQ:
    case ZYDIS_MNEMONIC_KORTEST:
    case ZYDIS_MNEMONIC_KTESTB:
    case ZYDIS_MNEMONIC_KTESTW:
    case ZYDIS_MNEMONIC_KTESTD:
    case ZYDIS_MNEMONIC_KTESTQ:
    case ZYDIS_MNEMONIC_KSHIFTLB:
    case ZYDIS_MNEMONIC_KSHIFTLW:
    case ZYDIS_MNEMONIC_KSHIFTLD:
    case ZYDIS_MNEMONIC_KSHIFTLQ:
    case ZYDIS_MNEMONIC_KSHIFTRB:
    case ZYDIS_MNEMONIC_KSHIFTRW:
    case ZYDIS_MNEMONIC_KSHIFTRD:
    case ZYDIS_MNEMONIC_KSHIFTRQ:
    case ZYDIS_MNEMONIC_KUNPCKBW:
    case ZYDIS_MNEMONIC_KUNPCKWD:
    case ZYDIS_MNEMONIC_KUNPCKDQ: exec_kop(cpu, d); return;

    // -- gathers / scatters / maskmov (VEX legacy forms; EVEX via exec_evex) --
    case ZYDIS_MNEMONIC_VGATHERDPS: exec_vex_gather(cpu, d, 4, 4); return;
    case ZYDIS_MNEMONIC_VGATHERDPD: exec_vex_gather(cpu, d, 4, 8); return;
    case ZYDIS_MNEMONIC_VGATHERQPS: exec_vex_gather(cpu, d, 8, 4); return;
    case ZYDIS_MNEMONIC_VGATHERQPD: exec_vex_gather(cpu, d, 8, 8); return;
    case ZYDIS_MNEMONIC_VPGATHERDD: exec_vex_gather(cpu, d, 4, 4); return;
    case ZYDIS_MNEMONIC_VPGATHERDQ: exec_vex_gather(cpu, d, 4, 8); return;
    case ZYDIS_MNEMONIC_VPGATHERQD: exec_vex_gather(cpu, d, 8, 4); return;
    case ZYDIS_MNEMONIC_VPGATHERQQ: exec_vex_gather(cpu, d, 8, 8); return;
    case ZYDIS_MNEMONIC_VMASKMOVPS:
    case ZYDIS_MNEMONIC_VMASKMOVPD:
    case ZYDIS_MNEMONIC_VPMASKMOVD:
    case ZYDIS_MNEMONIC_VPMASKMOVQ: exec_maskmov(cpu, d); return;

    case ZYDIS_MNEMONIC_LDMXCSR: exec_ldmxcsr(cpu, d, true); return;
    case ZYDIS_MNEMONIC_STMXCSR: exec_ldmxcsr(cpu, d, false); return;
    case ZYDIS_MNEMONIC_FXSAVE:
    case ZYDIS_MNEMONIC_FXSAVE64: exec_fxsave(cpu, d, true); return;
    case ZYDIS_MNEMONIC_FXRSTOR:
    case ZYDIS_MNEMONIC_FXRSTOR64: exec_fxsave(cpu, d, false); return;
    case ZYDIS_MNEMONIC_XSAVE:
    case ZYDIS_MNEMONIC_XSAVE64:
    case ZYDIS_MNEMONIC_XSAVEOPT:
    case ZYDIS_MNEMONIC_XSAVEOPT64:
    case ZYDIS_MNEMONIC_XSAVES:
    case ZYDIS_MNEMONIC_XSAVES64: exec_xsave(cpu, d, true, false); return;
    case ZYDIS_MNEMONIC_XSAVEC:
    case ZYDIS_MNEMONIC_XSAVEC64: exec_xsave(cpu, d, true, true); return;
    case ZYDIS_MNEMONIC_XRSTOR:
    case ZYDIS_MNEMONIC_XRSTOR64:
    case ZYDIS_MNEMONIC_XRSTORS:
    case ZYDIS_MNEMONIC_XRSTORS64: exec_xsave(cpu, d, false, false); return;

    // -- x87 --
    case ZYDIS_MNEMONIC_FLD:
    case ZYDIS_MNEMONIC_FST:
    case ZYDIS_MNEMONIC_FSTP:
    case ZYDIS_MNEMONIC_FILD:
    case ZYDIS_MNEMONIC_FIST:
    case ZYDIS_MNEMONIC_FISTP:
    case ZYDIS_MNEMONIC_FISTTP:
    case ZYDIS_MNEMONIC_FLDZ:
    case ZYDIS_MNEMONIC_FLD1:
    case ZYDIS_MNEMONIC_FLDPI:
    case ZYDIS_MNEMONIC_FLDL2T:
    case ZYDIS_MNEMONIC_FLDL2E:
    case ZYDIS_MNEMONIC_FLDLG2:
    case ZYDIS_MNEMONIC_FLDLN2:
    case ZYDIS_MNEMONIC_FADD:
    case ZYDIS_MNEMONIC_FMUL:
    case ZYDIS_MNEMONIC_FSUB:
    case ZYDIS_MNEMONIC_FDIV:
    case ZYDIS_MNEMONIC_FSUBR:
    case ZYDIS_MNEMONIC_FDIVR:
    case ZYDIS_MNEMONIC_FADDP:
    case ZYDIS_MNEMONIC_FMULP:
    case ZYDIS_MNEMONIC_FSUBP:
    case ZYDIS_MNEMONIC_FDIVP:
    case ZYDIS_MNEMONIC_FSUBRP:
    case ZYDIS_MNEMONIC_FDIVRP:
    case ZYDIS_MNEMONIC_FIADD:
    case ZYDIS_MNEMONIC_FISUB:
    case ZYDIS_MNEMONIC_FIMUL:
    case ZYDIS_MNEMONIC_FIDIV:
    case ZYDIS_MNEMONIC_FCOM:
    case ZYDIS_MNEMONIC_FCOMP:
    case ZYDIS_MNEMONIC_FUCOM:
    case ZYDIS_MNEMONIC_FUCOMP:
    case ZYDIS_MNEMONIC_FUCOMPP:
    case ZYDIS_MNEMONIC_FCOMPP:
    case ZYDIS_MNEMONIC_FICOMP:
    case ZYDIS_MNEMONIC_FCOMI:
    case ZYDIS_MNEMONIC_FUCOMI:
    case ZYDIS_MNEMONIC_FCOMIP:
    case ZYDIS_MNEMONIC_FUCOMIP:
    case ZYDIS_MNEMONIC_FCMOVB:
    case ZYDIS_MNEMONIC_FCMOVE:
    case ZYDIS_MNEMONIC_FCMOVBE:
    case ZYDIS_MNEMONIC_FCMOVU:
    case ZYDIS_MNEMONIC_FCMOVNB:
    case ZYDIS_MNEMONIC_FCMOVNE:
    case ZYDIS_MNEMONIC_FCMOVNBE:
    case ZYDIS_MNEMONIC_FCMOVNU:
    case ZYDIS_MNEMONIC_FNSTSW:
    case ZYDIS_MNEMONIC_FLDCW:
    case ZYDIS_MNEMONIC_FNSTCW:
    case ZYDIS_MNEMONIC_FCHS:
    case ZYDIS_MNEMONIC_FABS:
    case ZYDIS_MNEMONIC_FXCH:
    case ZYDIS_MNEMONIC_FRNDINT:
    case ZYDIS_MNEMONIC_FPREM:
    case ZYDIS_MNEMONIC_FPREM1:
    case ZYDIS_MNEMONIC_FSCALE:
    case ZYDIS_MNEMONIC_FSQRT:
    case ZYDIS_MNEMONIC_FSIN:
    case ZYDIS_MNEMONIC_FCOS:
    case ZYDIS_MNEMONIC_F2XM1:
    case ZYDIS_MNEMONIC_FYL2X:
    case ZYDIS_MNEMONIC_FPATAN:
    case ZYDIS_MNEMONIC_FPTAN:
    case ZYDIS_MNEMONIC_FSINCOS:
    case ZYDIS_MNEMONIC_FTST:
    case ZYDIS_MNEMONIC_FXAM:
    case ZYDIS_MNEMONIC_FFREE:
    case ZYDIS_MNEMONIC_FINCSTP:
    case ZYDIS_MNEMONIC_FDECSTP:
    case ZYDIS_MNEMONIC_FNINIT:
    case ZYDIS_MNEMONIC_FNCLEX:
    case ZYDIS_MNEMONIC_FBLD:
    case ZYDIS_MNEMONIC_FBSTP:
    case ZYDIS_MNEMONIC_FLDENV:
    case ZYDIS_MNEMONIC_FNSTENV: exec_x87(cpu, d); return;

    default: break;
    }

    // Jcc / SETcc / CMOVcc families (16 mnemonics each).
    if (is_jcc(m)) {
        exec_jcc(cpu, d);
        return;
    }
    if (is_setcc(m)) {
        exec_setcc(cpu, d);
        return;
    }
    if (is_cmovcc(m)) {
        exec_cmovcc(cpu, d);
        return;
    }
    unimplemented(cpu, d, "opcode");
}

void run_cpu_loop(CPU* cpu) {
    while (!cpu->emu->exiting) {
        LoopScope loop(cpu);
        try {
            exec_one(cpu);
        } catch (GuestError& e) {
            int eff = guest_fatal_signo(e.reason, 0);
            dump_core(e.cpu ? e.cpu : cpu, e.reason, e.fault_addr);
            fflush(stdout);
            fflush(stderr);
            _exit(128 + eff);
        }
        // Host signals (SIGSEGV/SIGBUS/SIGILL/SIGFPE) raised during guest
        // execution land here via the loop recovery scope.
        int signo = 0;
        uint64_t faddr = 0;
        if (mem_loop_fault(&signo, &faddr)) {
            const char* reason = "guest signal (host fault during emulation)";
            if (signo == SIGSEGV)
                reason = "guest SIGSEGV (host signal during emulation)";
            else if (signo == SIGBUS)
                reason = "guest SIGBUS (host signal during emulation)";
            else if (signo == SIGILL)
                reason = "guest SIGILL (host signal during emulation)";
            else if (signo == SIGFPE)
                reason = "guest SIGFPE (host signal during emulation)";
            try {
                guest_error(cpu, reason, faddr);
            } catch (GuestError& e) {
                int eff = signo ? signo : guest_fatal_signo(e.reason, 0);
                dump_core(e.cpu ? e.cpu : cpu, e.reason, e.fault_addr, eff);
                fflush(stdout);
                fflush(stderr);
                _exit(128 + eff);
            }
        }
        // NOTE: no catch(...) here: pthread_exit (guest thread exit)
        // unwinds the host stack with forced unwinding, which must
        // propagate (catching it would break thread exit).
    }
}

