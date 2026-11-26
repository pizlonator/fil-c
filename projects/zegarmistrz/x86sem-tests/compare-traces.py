#!/usr/bin/env python3
# Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# compare-traces.py -- the x86sem differential suite's comparator.
#
# Usage: compare-traces.py <native.strace> <emu.strace> [test-binary] [-q]
#
# Compares the whole-state traces that native-trace (hardware) and
# `zegarmistrz --state-trace=` (emulator) produce for the same imported
# corpus program. Both traces have one record per executed instruction,
# in order, so records align 1:1 by index: record k is the architectural
# state after instruction k on both sides. A test PASSES iff every
# comparable slot of every record matches. Exit 0 = pass.
#
# Record line format (both producers):
#   S rip=<16hex> rax=<16hex> rbx=... rcx rdx rsi rdi rbp rsp r8..r15
#     fl=<8hex> ymm0=<128hex> ... ymm15=<128hex>
# GPRs and rip are 16 lowercase hex digits (natural hex, MSB first); fl
# is the raw 32-bit EFLAGS; each ymmN is the 512-bit register as its
# eight qwords q7..q0 (MSW first, each qword big-endian), so the
# register's LOW qword is the last 16 chars and the register image is
# recoverable from the hex string as bytes-reversed-per-qword. The
# 512-bit slot's upper 256 bits are always zero (the kernel zeroes zmm
# state at execve, no corpus instruction writes above 256 bits, and the
# emulator keeps them zero), so comparing the full slot is safe.
#
# Skip rules (a port of the kframework/X86-64-semantics harness
# semantics -- scripts/kutils.pm compareStates/compareInts -- adapted
# to a hardware-vs-emulator oracle):
# - rip: never compared (the loads differ: gdb/native-trace vs the
#   emulator's loader may place the guest differently).
# - rsp, rbp: never compared (stack addresses differ between the two
#   runners; kutils skipped the same slots by register index).
# - rsi, rdi: skipped when stack-like. A value is stack-like if it is
#   within [min(rsp_seen)-65536, max(rsp_seen)+4096] of ITS OWN trace
#   (computed per trace). The corpus only ever points rsi/rdi at the
#   stack (string ops on pushed operands), and the two runners use
#   different stacks; any pointer into their own stack is therefore not
#   comparable. kutils achieved this via the K side's "symloc" marker
#   (pointer-valued register) -- the rsp-window test is the same idea
#   without a symbolic executor.
# - flags: CF/PF/AF/ZF/SF/OF (bits 0,2,4,6,7,11) are extracted and
#   compared under the per-BIT defined/undefined model documented at
#   FLAG_PROFILES below; everything else (DF, IF, TF, AC, and the fixed
#   bit 1) is skipped, matching kutils which parses only the six from
#   gdb's symbolic eflags output and drops DF.
# - ymm0..15: the full 512-bit slot is compared, except that a slot
#   whose hex contains a NaN pattern on BOTH sides counts as equal
#   (ported from kutils compareInts/containsNaN: substring match on the
#   patterns below, both sides must contain one). Float ops producing
#   NaNs carry implementation-defined payloads on hardware, and the K
#   side short-circuits the same way. DEVIATION FROM UPSTREAM: kutils
#   spells the double-quiet-NaN pattern "fff800000000000" -- 15 chars,
#   one hex digit short, so it also matches values whose payload
#   happens to line up at that offset. This port uses the correct
#   16-char "fff8000000000000"; all other patterns were audited for
#   length (the 8-char ones are 32-bit float patterns matched inside
#   the qword hex, the 16-char ones exact qwords) and are as upstream.
# - architecturally undefined flags: handled per BIT, persistently. See
#   the FLAG_PROFILES block below for the model, the classification
#   table, and the empirical method used for the ambiguous families.

import re
import subprocess
import sys
import os


GPR_SLOTS = ["rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp",
             "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"]

# Flag bits compared: CF, PF, AF, ZF, SF, OF (the kutils six). All
# per-bit profile tuples below are aligned with this order.
FLAG_BITS = [(1 << 0, "CF"), (1 << 2, "PF"), (1 << 4, "AF"),
             (1 << 6, "ZF"), (1 << 7, "SF"), (1 << 11, "OF")]

# NaN escape, ported from kutils.pm containsNaN (substring match on the
# hex string, both sides must contain one). See the file comment for the
# corrected 16-char double-NaN pattern and the upstream deviation.
NAN_PATTERNS = ["7fffffff", "7fc00000", "ffc00000", "ffffffff",
                "7fffffffffffffff", "7ff8000000000000",
                "ffffffffffffffff", "fff8000000000000"]


# ---------------------------------------------------------------------------
# The flag-definition model (per BIT, persistent -- there is no window)
# ---------------------------------------------------------------------------
# For each of the six compared bits the comparator tracks whether the
# bit's CURRENT value is architecturally defined or undefined, exactly
# as hardware behaves:
#
#   D = "defined"    the instruction writes the bit with a defined
#                    value: from this record on, mismatches FAIL.
#   U = "undefined"  the instruction writes the bit with a value the
#                    SDM marks UNDEFINED: mismatches are tolerated and
#                    counted (real silicon produces run-to-run garbage
#                    -- observed: OF after a 16-bit SHLD differs between
#                    runs of the same binary).
#   P = "preserved"  the instruction does NOT write the bit: hardware
#                    leaves the bit exactly as it was (including its
#                    undefined garbage), so the bit KEEPS its previous
#                    defined/undefined state -- compared if it was
#                    defined, suppressed if it was undefined -- until
#                    some instruction redefines it. This preservation
#                    rule is what makes the model persistent per-bit
#                    instead of per-record.
#
# The update happens BEFORE the comparison of each record: the record's
# own instruction defines/undefines bits first, then that record's flag
# slots are compared under the updated state. An undefined-bit mismatch
# is counted as skipped-undefined; a matching undefined bit (or any
# defined bit) counts as compared. Nothing is ever closed
# unconditionally: an undefined bit persists until the first
# instruction that defines it, matching hardware.
#
# Seeded all-DEFINED at trace start: hardware starts from a fixed
# EFLAGS image (the kernel's), and the initial record's flags are
# kernel-defined on both sides, so they are compared.
#
# AMBIGUOUS CASES WERE CLASSIFIED EMPIRICALLY, against the hardware
# oracle: for each candidate family we walked the native traces and
# recorded which of the six bits CHANGE across the instruction
# (before-vs-after within one trace), then re-ran the same guest twice
# and diffed the traces bit-for-bit (the traces are identical across
# runs, so stable unchanged bits are preservation, not run-varying
# garbage; and the ADDR_NO_RANDOMIZE dumper makes the traces
# address-stable too). Findings used below: PEXT/PDEP change no flag
# bit in 530+530 records with varied inputs (SDM: no flags affected);
# SHLX/SHRX/SARX likewise (156 records); ROL/ROR/RCL/RCR leave
# ZF/SF/PF/AF untouched and write CF (OF only for count 1); SHLD with
# count 0 changes nothing (count 0x40 on 64-bit = masked count 0, ten
# records, zero deltas); POPCNT only ever changed ZF on this corpus
# (its CF also cleared stably to 0 on this host, but the SDM marks CF
# undefined after POPCNT, so the model TOLERATES CF instead of
# comparing it -- see the POPCNT entry); BSF/
# BSR write varying PF/SF (undefined garbage) but stable ZF.

# Per-bit states for the six bits, in FLAG_BITS order (CF, PF, AF, ZF,
# SF, OF).
D, U, P = "defined", "undefined", "preserved"

ALLDEF = (D, D, D, D, D, D)   # all six written with defined values
ALLPRES = (P, P, P, P, P, P)  # no flag bit written
ALLUNDEF = (U, U, U, U, U, U) # all six written undefined (div/idiv)
LOGIC = (D, D, U, D, D, D)    # AND/OR/XOR/TEST: CF=OF=0, ZF/SF/PF from
                              # the result, AF undefined
INCDEC = (P, D, D, D, D, D)   # INC/DEC: CF preserved, the rest defined
MUL = (D, U, U, U, U, D)      # MUL/IMUL (all forms): CF/OF defined,
                              # SF/ZF/AF/PF undefined
BSFBSR = (U, U, U, D, U, U)   # BSF/BSR: ZF defined; CF/OF/SF/AF/PF
                              # undefined. (When src==0 the DEST GPR is
                              # also hardware-undefined: hardware leaves
                              # it unmodified and so does the emulator --
                              # the GPR comparison covers that, and the
                              # corpus bsf/bsr dirs pass on it.)
POPCNT = (U, U, U, D, U, U)   # POPCNT: the SDM defines only ZF (set iff
                              # src==0); hardware is deterministic there,
                              # so ZF stays compared. CF is SDM-UNDEFINED
                              # after POPCNT: it happens to clear to 0 on
                              # this host's silicon (matching the
                              # emulator across all 272 corpus popcnt
                              # instructions), but silicon that leaves
                              # SDM-permitted garbage in CF -- including
                              # via the portable ZEG_X86SEM_NATIVE=gdb
                              # oracle -- would spuriously FAIL against
                              # the emulator's cleared CF. CF is
                              # therefore tolerated via the per-bit
                              # model, like OF/SF/AF/PF, which the SDM
                              # also marks undefined.
COMIS = ALLDEF                # COMISS/UCOMISS/COMISD/UCOMISD (+VEX):
                              # ZF/PF/CF from the result; OF/SF/AF are
                              # written 0 (SDM "set to 0"; the emulator
                              # clears them and the corpus agrees across
                              # ~4000 comis-family records, where they
                              # were fully compared before this model).
CMPXCHG8B = (P, P, P, D, P, P)  # CMPXCHG8B/16B: ZF only

# Shifts and rotates are count-dependent; None marks them for the
# count-resolving path (profile_for + shift_profile + resolve_count).
SHIFT_FAMILY = set(["shl", "sal", "shr", "sar", "shld", "shrd",
                    "rol", "ror", "rcl", "rcr"])

# The classification table: objdump mnemonic (+ count for the shift
# family) -> per-bit profile. Keyed by the mnemonic objdump actually
# prints (AT&T: no GAS size suffix on register forms, but suffixes DO
# appear on memory forms -- "divq (%rax)", "shlb %cl,(%rax)" -- so
# base_mnemonic strips a trailing b/w/l/q only when the remainder is a
# known key). Mnemonics not in the table default to ALLPRES (safest:
# preserved flags still compare whenever they are defined) and are
# logged once on stderr so the table grows explicitly rather than
# silently misclassifying.
FLAG_PROFILES = {
    # -- all six defined ------------------------------------------------
    "add": ALLDEF, "adc": ALLDEF, "sub": ALLDEF, "sbb": ALLDEF,
    "cmp": ALLDEF, "neg": ALLDEF, "xadd": ALLDEF, "cmpxchg": ALLDEF,
    "cmpsb": ALLDEF, "cmpsw": ALLDEF, "cmpsq": ALLDEF,
    "scas": ALLDEF, "scasb": ALLDEF, "scasw": ALLDEF, "scasd": ALLDEF,
    "scasq": ALLDEF,                      # string subtract-compare
    "pcmpestri": ALLDEF, "pcmpestrm": ALLDEF,
    "pcmpistri": ALLDEF, "pcmpistrm": ALLDEF,
    "vpcmpestri": ALLDEF, "vpcmpestrm": ALLDEF,
    "vpcmpistri": ALLDEF, "vpcmpistrm": ALLDEF,
    "comiss": COMIS, "ucomiss": COMIS, "comisd": COMIS, "ucomisd": COMIS,
    "vcomiss": COMIS, "vucomiss": COMIS, "vcomisd": COMIS,
    "vucomisd": COMIS,
    # -- defined subsets -------------------------------------------------
    "inc": INCDEC, "dec": INCDEC,
    "and": LOGIC, "or": LOGIC, "xor": LOGIC, "test": LOGIC,
    "mul": MUL, "imul": MUL,
    "div": ALLUNDEF, "idiv": ALLUNDEF,
    "bsf": BSFBSR, "bsr": BSFBSR,
    "popcnt": POPCNT,
    "cmpxchg8b": CMPXCHG8B, "cmpxchg16b": CMPXCHG8B,
    "stc": (D, P, P, P, P, P), "clc": (D, P, P, P, P, P),
    "cmc": (D, P, P, P, P, P),
    # -- count-dependent (resolved per record) ---------------------------
    "shl": None, "sal": None, "shr": None, "sar": None,
    "shld": None, "shrd": None, "rol": None, "ror": None,
    "rcl": None, "rcr": None,
    # -- BMI2 / bit permutes write NO flags (empirically verified; see
    #    the block comment above) ---------------------------------------
    "shlx": ALLPRES, "shrx": ALLPRES, "sarx": ALLPRES,
    "pext": ALLPRES, "pdep": ALLPRES, "mulx": ALLPRES, "rorx": ALLPRES,
    # -- integer data movement / control flow: no flags written ----------
    "mov": ALLPRES, "movabs": ALLPRES, "lea": ALLPRES,
    "push": ALLPRES, "pop": ALLPRES, "xchg": ALLPRES, "nop": ALLPRES,
    "movsx": ALLPRES, "movzx": ALLPRES, "movsxd": ALLPRES,
    "not": ALLPRES, "seto": ALLPRES, "setno": ALLPRES,
    "jmp": ALLPRES, "call": ALLPRES, "leave": ALLPRES,
    "loop": ALLPRES, "loope": ALLPRES, "loopne": ALLPRES,
    "syscall": ALLPRES, "sysenter": ALLPRES,
    # -- string ops that write no flags (CMPS/SCAS are subtract-compares,
    #    listed under all-six above) -------------------------------------
    "movsb": ALLPRES, "movsw": ALLPRES, "movsl": ALLPRES,
    "movsq": ALLPRES,
    "stos": ALLPRES, "stosb": ALLPRES,
    "stosw": ALLPRES, "stosd": ALLPRES, "stosq": ALLPRES,
    "lods": ALLPRES, "lodsb": ALLPRES, "lodsw": ALLPRES,
    "lodsd": ALLPRES, "lodsq": ALLPRES, "lodsl": ALLPRES,
    # MOVSX/MOVZX sign/zero-extends (objdump spellings movsbw/movsbl/
    # movswl/movslq/movsbq/movswq and the movz* family): data movement,
    # no flags.
    "movsbw": ALLPRES, "movsbl": ALLPRES, "movswl": ALLPRES,
    "movsbq": ALLPRES, "movswq": ALLPRES, "movslq": ALLPRES,
    "movzbw": ALLPRES, "movzbl": ALLPRES, "movzwl": ALLPRES,
    "movzbq": ALLPRES, "movzwq": ALLPRES, "movzlq": ALLPRES,
    # -- x87/SSE/AVX float arithmetic and converts: EFLAGS untouched
    #    (MXCSR only, which is never compared) ---------------------------
    "addss": ALLPRES, "addsd": ALLPRES, "addps": ALLPRES, "addpd": ALLPRES,
    "subss": ALLPRES, "subsd": ALLPRES, "subps": ALLPRES, "subpd": ALLPRES,
    "mulss": ALLPRES, "mulsd": ALLPRES, "mulps": ALLPRES, "mulpd": ALLPRES,
    "divss": ALLPRES, "divsd": ALLPRES, "divps": ALLPRES, "divpd": ALLPRES,
    "sqrtss": ALLPRES, "sqrtsd": ALLPRES, "sqrtps": ALLPRES,
    "sqrtpd": ALLPRES,
    "rcpss": ALLPRES, "rcpps": ALLPRES, "rsqrtss": ALLPRES,
    "rsqrtps": ALLPRES,
    "maxss": ALLPRES, "maxsd": ALLPRES, "maxps": ALLPRES, "maxpd": ALLPRES,
    "minss": ALLPRES, "minsd": ALLPRES, "minps": ALLPRES, "minpd": ALLPRES,
    "haddps": ALLPRES, "haddpd": ALLPRES, "hsubps": ALLPRES,
    "hsubpd": ALLPRES,
    "addsubps": ALLPRES, "addsubpd": ALLPRES,
    "mpsadbw": ALLPRES, "vmpsadbw": ALLPRES,
    "pclmulqdq": ALLPRES, "vpclmulqdq": ALLPRES,
    "pclmullqlqdq": ALLPRES, "pclmulhqlqdq": ALLPRES,
    "pclmullqhqdq": ALLPRES, "pclmulhqhqdq": ALLPRES,
    "vpclmullqlqdq": ALLPRES, "vpclmulhqlqdq": ALLPRES,
    "vpclmullqhqdq": ALLPRES, "vpclmulhqhqdq": ALLPRES,
    "cvttsd2si": ALLPRES, "cvttss2si": ALLPRES, "cvtsd2si": ALLPRES,
    "cvtss2si": ALLPRES, "cvtsi2sd": ALLPRES, "cvtsi2ss": ALLPRES,
    "cvtss2sd": ALLPRES, "cvtsd2ss": ALLPRES,
    "vcvttsd2si": ALLPRES, "vcvttss2si": ALLPRES, "vcvtsd2si": ALLPRES,
    "vcvtss2si": ALLPRES,
    "vcvtph2ps": ALLPRES, "vcvtps2ph": ALLPRES,
    "movddup": ALLPRES, "movsldup": ALLPRES, "movshdup": ALLPRES,
    "movmskpd": ALLPRES, "movmskps": ALLPRES, "vmovmskpd": ALLPRES,
    "vmovmskps": ALLPRES,
    "vzeroall": ALLPRES, "vzeroupper": ALLPRES,
    "vbroadcastss": ALLPRES, "vbroadcastsd": ALLPRES,
    "vbroadcasti128": ALLPRES,
    "vxorpd": ALLPRES, "vxorps": ALLPRES,
    "vaddss": ALLPRES, "vaddsd": ALLPRES, "vaddps": ALLPRES,
    "vaddpd": ALLPRES,
    "vsubss": ALLPRES, "vsubsd": ALLPRES, "vsubps": ALLPRES,
    "vsubpd": ALLPRES,
    "vmulss": ALLPRES, "vmulsd": ALLPRES, "vmulps": ALLPRES,
    "vmulpd": ALLPRES,
    "vdivss": ALLPRES, "vdivsd": ALLPRES, "vdivps": ALLPRES,
    "vdivpd": ALLPRES,
    "vsqrtss": ALLPRES, "vsqrtsd": ALLPRES, "vsqrtps": ALLPRES,
    "vsqrtpd": ALLPRES,
    "vmaxss": ALLPRES, "vmaxsd": ALLPRES, "vmaxps": ALLPRES,
    "vmaxpd": ALLPRES,
    "vminss": ALLPRES, "vminsd": ALLPRES, "vminps": ALLPRES,
    "vminpd": ALLPRES,
    "vhaddps": ALLPRES, "vhaddpd": ALLPRES, "vhsubps": ALLPRES,
    "vhsubpd": ALLPRES,
    "vaddsubps": ALLPRES, "vaddsubpd": ALLPRES,
}

# FMA family (all 132/213/231 and addsub/subadd spellings, ss/sd/ps/pd):
# no EFLAGS. Handled by prefix so the combinatorial spellings do not
# have to be enumerated; they are all data-only ops.
NOFLAG_PREFIXES = ("vfm", "vfmsub", "vfnm", "vfmsubadd")

# objdump prints these as separate leading tokens of the instruction
# text (e.g. "rep movsq"); strip them before reading the mnemonic.
PREFIX_TOKENS = set(["rep", "repe", "repz", "repne", "repnz", "lock",
                     "bnd", "notrack", "data16", "addr16",
                     "cs", "ds", "es", "fs", "gs", "ss"])

SUFFIXES = ("b", "w", "l", "q")


def base_mnemonic(m):
    # Strip a trailing GAS size suffix only when the remainder is itself
    # a known key (objdump adds suffixes on memory-operand forms:
    # "divq (%rax)", "shlb %cl,(%rax)"). "movsq" is NOT stripped because
    # bare "movs" is deliberately not a key (see the cmpsd note below).
    m = m.lower()
    if m in FLAG_PROFILES or m in SHIFT_FAMILY:
        return m
    for s in SUFFIXES:
        if m.endswith(s) and m[:-1] in FLAG_PROFILES:
            return m[:-1]
    return m


def shift_profile(base, count):
    # Per-SDM profiles for the shift/rotate families at a RESOLVED
    # (masked) count. count == 0 means hardware modifies nothing: every
    # bit preserved (verified: SHLD $0x40 on 64-bit = masked count 0
    # leaves all six bits untouched in the native trace). count is None
    # when the count could NOT be resolved -- the CL form at record 0,
    # where no previous record holds the pre-instruction CL. An unknown
    # count falls back to the count>1 profile: OF undefined (the safe
    # direction -- it can only TOLERATE an OF mismatch that a known
    # count of 1 would have compared, never over-compare), with
    # CF/ZF/SF/PF defined, which matches every count >= 1.
    if base in ("shl", "sal", "shr", "sar", "shld", "shrd"):
        if count == 0:
            return ALLPRES
        if count == 1:
            return (D, D, U, D, D, D)  # CF/ZF/SF/PF/OF defined, AF undef
        # count > 1 (or unresolvable): OF undefined (hardware writes
        # garbage -- observed changing across records at SHLD $0x20),
        # AF undefined.
        return (D, D, U, D, D, U)
    # ROL/ROR/RCL/RCR: CF defined for count >= 1, OF defined only for
    # count == 1 (undefined for count > 1 or unresolvable), ZF/SF/PF/AF
    # untouched by the instruction (verified: zero deltas across the
    # rol corpus).
    if count == 0:
        return ALLPRES
    return (D, P, P, P, P, D if count == 1 else U)


def decode_operand_size(bts):
    # Operand size in bytes (1/2/4/8) from the opcode bytes: 0x66
    # prefix -> word, REX.W -> qword, else the opcode (C0/D0/D2 byte
    # shift opcodes vs C1/D1/D3, two-byte 0F opcodes = 32-bit unless
    # prefixed). Used to mask the shift/rotate count (SDM: the count is
    # masked to 5 bits, or 6 bits iff 64-bit mode AND REX.W).
    i = 0
    rexw = False
    o66 = False
    op = None
    while i < len(bts):
        b = bts[i]
        if 0x40 <= b <= 0x4f:
            rexw = rexw or ((b & 8) != 0)
        elif b == 0x66:
            o66 = True
        elif b in (0x2e, 0x36, 0x3e, 0x26, 0x64, 0x65, 0x67,
                   0xf0, 0xf2, 0xf3):
            pass
        else:
            op = b
            break
        i += 1
    if o66:
        return 2
    if rexw:
        return 8
    if op == 0x0f:
        return 4  # two-byte opcode (SHLD/SHRD 0F A4/A5); 16/64 handled above
    if op in (0xc0, 0xd0, 0xd2):
        return 1
    return 4


def resolve_count(bts, text, rcx):
    # The shift/rotate count, resolved and masked. rcx is the
    # PRE-instruction RCX -- the PREVIOUS record's, since record k holds
    # the state AFTER instruction k, so record k-1 is the state
    # instruction k sees -- or None at record 0 (no previous record).
    # Encodings:
    #   C0/C1 (imm8 form)  -- objdump text shows "$0x..": count = imm
    #   D2/D3 (CL form)    -- objdump text shows "%cl":  count = cl
    #   D0/D1 (count-1)    -- no immediate, no %cl:      count = 1
    # The CL form MUST read the pre-instruction CL: the current record's
    # RCX is the POST-instruction value, which for a CL-form shift/
    # rotate/SHLD/SHRD whose destination is RCX is the shifted RESULT,
    # not the source count -- e.g. a true count of 5 misread as the 1
    # the shift left in CL would flip the profile from undefined-OF to
    # defined-OF. Record 0 has no previous record to read CL from, so
    # the count is unresolvable there: return None (resolved below into
    # the undefined-OF profile). (An even older comparator tried to
    # read the immediate from the objdump text but only ever captured
    # the MNEMONIC token, so the D0/D1 one-operand form -- which
    # assembles `shl $1,%reg` -- never resolved and OF was treated as
    # undefined for every shift. The count is now resolved for all
    # three encodings, wired into both the shift/rotate and the
    # SHLD/SHRD profiles.)
    size = decode_operand_size(bts)
    mask = 63 if size == 8 else 31
    m = re.search(r"\$0x([0-9a-fA-F]+)", text)
    if m:
        return int(m.group(1), 16) & mask
    if "%cl" in text:
        if rcx is None:
            return None
        return rcx & mask
    return 1 & mask


def profile_for(text, bts, rcx, unknown):
    # Per-bit profile of the instruction at a record. text is the full
    # objdump text (None if unresolvable); bts the opcode bytes; rcx
    # the PRE-instruction RCX (the previous record's, None at record 0)
    # -- the CL-form shift/rotate count source, see resolve_count.
    if not text:
        return ALLPRES
    toks = text.split()
    i = 0
    while i < len(toks) and (toks[i].lower() in PREFIX_TOKENS or
                             toks[i].lower().startswith("rex")):
        i += 1
    if i >= len(toks):
        return ALLPRES
    mn = toks[i].rstrip(",;:").lower()
    base = base_mnemonic(mn)
    if base in SHIFT_FAMILY:
        return shift_profile(base, resolve_count(bts, text, rcx))
    prof = FLAG_PROFILES.get(base)
    if prof is None:
        for pref in NOFLAG_PREFIXES:
            if mn.startswith(pref):
                return ALLPRES
        # Mnemonic prefixes for the flag-reading families (cmovcc,
        # setcc, jcc): they never WRITE flags. Anything else unknown is
        # logged (once per mnemonic, at the end) and treated as
        # preserved -- preserved bits still compare whenever they are
        # defined, so a misclassification cannot hide a defined-flag
        # bug; it can only over-compare a bit an unknown instruction
        # writes undefined.
        if mn.startswith(("cmov", "set")) or mn.startswith("j"):
            return ALLPRES
        unknown.add(mn)
        return ALLPRES
    return prof


def parse_trace(path):
    recs = []
    with open(path) as f:
        for line in f:
            if not line.startswith("S "):
                continue
            rec = {}
            for field in line.split()[1:]:
                k, _, v = field.partition("=")
                rec[k] = v
            if len(rec) != 34:
                sys.stderr.write(
                    "compare-traces: %s: malformed record (%d fields)\n"
                    % (path, len(rec)))
                sys.exit(2)
            recs.append(rec)
    return recs


class InsnMap:
    # rip -> (full objdump text, opcode bytes), resolved from objdump -d
    # once on demand. The record's rip is the POST-advance rip; the
    # instruction is the one whose [addr, addr+len) contains it.
    #
    # objdump splits instructions longer than the 7-byte bytes column
    # across continuation lines (e.g. movabs prints 7 bytes + the
    # mnemonic on the first line and the remaining bytes alone on the
    # next). The continuation carries no mnemonic: it EXTENDS the
    # previous instruction's range (without it, every record whose rip
    # lands after a movabs resolved to no instruction at all).
    def __init__(self, binary):
        self.ranges = []  # (start, end, full text, [opcode bytes])
        self.ok = False
        if not binary or not os.path.isfile(binary):
            return
        try:
            out = subprocess.run(["objdump", "-d", binary],
                                 capture_output=True, text=True,
                                 timeout=60).stdout
        except Exception as e:
            sys.stderr.write("compare-traces: objdump failed: %s\n" % e)
            return
        head = re.compile(r"^\s*([0-9a-f]+):\t([0-9a-f ]+)\t(\S.*)$")
        cont = re.compile(r"^\s*([0-9a-f]+):\t([0-9a-f ]+)\s*$")
        for line in out.splitlines():
            m = head.match(line)
            if m:
                start = int(m.group(1), 16)
                bts = [int(b, 16) for b in m.group(2).split()]
                self.ranges.append((start, start + len(bts),
                                    m.group(3).strip(), bts))
                continue
            m = cont.match(line)
            if m and self.ranges and \
                    self.ranges[-1][1] == int(m.group(1), 16):
                s, _, text, bts = self.ranges[-1]
                more = [int(b, 16) for b in m.group(2).split()]
                self.ranges[-1] = (s, int(m.group(1), 16) + len(more),
                                   text, bts + more)
        self.ranges.sort()
        self.starts = [r[0] for r in self.ranges]
        self.ok = True

    def insn_at(self, rip):
        # (text, bytes) of the instruction whose [addr, addr+len)
        # contains the post-advance rip. The record's rip is the NEXT
        # instruction's start whenever the code is contiguous, so
        # bisect_LEFT: the candidate range is the last instruction whose
        # start <= rip, and it contains rip iff start < rip <= end.
        if not self.ok or not self.ranges:
            return None, None
        import bisect
        i = bisect.bisect_left(self.starts, rip) - 1
        if i < 0:
            return None, None
        start, end, text, bts = self.ranges[i]
        if start < rip <= end:
            return text, bts
        return None, None


def contains_nan(hexstr):
    return any(p in hexstr for p in NAN_PATTERNS)


def rsp_window(recs):
    vals = [int(r["rsp"], 16) for r in recs]
    return min(vals) - 65536, max(vals) + 4096


def compare(native_path, emu_path, binary, quiet):
    native = parse_trace(native_path)
    emu = parse_trace(emu_path)
    if not native and not emu:
        sys.stderr.write("compare-traces: both traces are empty\n")
        return 1
    n_recs, e_recs = len(native), len(emu)
    if n_recs != e_recs:
        if n_recs < e_recs:
            which = "native"
        else:
            which = "emu"
        if not quiet:
            print("FAIL: record count mismatch: native=%d emu=%d" %
                  (n_recs, e_recs))
            print("  the %s trace ended early: instruction %d (1-based) "
                  "of %d produced no record" %
                  (which, min(n_recs, e_recs) + 1, max(n_recs, e_recs)))
            print("  for the emu side check emu.stderr for an "
                  "unimplemented-instruction / fault banner")
            print("  for the native side check native.stderr for a fault")
        return 1
    insns = InsnMap(binary)
    n_win = rsp_window(native)
    e_win = rsp_window(emu)

    def stack_like(rec, win, slot):
        v = int(rec[slot], 16)
        return win[0] <= v <= win[1]

    # The persistent per-bit defined/undefined state (see the
    # FLAG_PROFILES block). Seeded all-defined: the initial record's
    # flags are kernel-defined on both sides.
    state = [True] * 6  # aligned with FLAG_BITS: True = defined
    unknown = set()
    undefined_skips = 0  # tolerated mismatches on UNDEFINED bits
    compared = 0
    # Per-bit entry counts for the stderr summary: for each of the six
    # bits, how many records held it DEFINED (a mismatch there FAILs)
    # vs UNDEFINED (a mismatch is tolerated). This is what makes the
    # per-bit model observable per dir -- e.g. POPCNT's SDM-undefined
    # CF shows up as tolerated, not compared, while its ZF stays
    # compared (see the popcnt dir's cmp.err).
    bit_cmp = [0] * 6
    bit_und = [0] * 6
    for i in range(n_recs):
        n_rec, e_rec = native[i], emu[i]

        # rip + rsp + rbp: never compared. rsi/rdi: skipped when
        # stack-like in their own trace.
        skip = {"rip", "rsp", "rbp"}
        if stack_like(n_rec, n_win, "rsi") and stack_like(e_rec, e_win,
                                                          "rsi"):
            skip.add("rsi")
        if stack_like(n_rec, n_win, "rdi") and stack_like(e_rec, e_win,
                                                          "rdi"):
            skip.add("rdi")

        first_diff = None
        value_diffs = []
        for slot in GPR_SLOTS:
            if slot in skip:
                continue
            if n_rec[slot] != e_rec[slot]:
                value_diffs.append(slot)
                if first_diff is None:
                    first_diff = slot
        for r in range(16):
            slot = "ymm%d" % r
            nh, eh = n_rec[slot], e_rec[slot]
            if nh == eh:
                continue
            if contains_nan(nh) and contains_nan(eh):
                continue
            value_diffs.append(slot)
            if first_diff is None:
                first_diff = slot

        # Resolve the executed instruction and update the per-bit state
        # with its flag-definition profile BEFORE comparing this
        # record's flags (the record's own instruction defines or
        # undefines bits, then they are compared per the updated
        # state). The CL-form shift/rotate/SHLD/SHRD count comes from
        # the PREVIOUS record's RCX: record i-1 is the state BEFORE
        # instruction i, while this record's own RCX is
        # POST-instruction (for a CL-form shift whose destination is
        # RCX that is the shifted result, not the count). Record 0 has
        # no previous record, so prev_rcx is None and the CL-form count
        # is treated as unknown (the undefined-OF profile).
        prev_rcx = int(native[i - 1]["rcx"], 16) if i else None
        text, bts = insns.insn_at(int(n_rec["rip"], 16))
        prof = profile_for(text, bts, prev_rcx, unknown)
        for j, st in enumerate(prof):
            if st == D:
                state[j] = True
            elif st == U:
                state[j] = False
            # P: preserved -- keep the previous state (hardware does).
        for j in range(6):
            if state[j]:
                bit_cmp[j] += 1
            else:
                bit_und[j] += 1

        # Compare the six flag bits under the updated state: a defined
        # bit that mismatches is a real divergence; an undefined bit
        # that mismatches is tolerated run-to-run garbage (counted);
        # matching bits count as compared.
        flag_diffs = []
        n_fl, e_fl = int(n_rec["fl"], 16), int(e_rec["fl"], 16)
        undef_here = 0
        for j, (bit, name) in enumerate(FLAG_BITS):
            if (n_fl & bit) != (e_fl & bit):
                if state[j]:
                    flag_diffs.append(name)
                    if first_diff is None:
                        first_diff = "fl:" + name
                else:
                    undef_here += 1
        undefined_skips += undef_here

        if not value_diffs and not flag_diffs:
            compared += 1
            continue

        # Real divergence: report it. (An undefined-bit-only mismatch is
        # not a divergence -- it never reaches this point.)
        if not quiet:
            print("FAIL: first divergence at record %d (instruction #%d, "
                  "rip %s)" % (i, i + 1, n_rec["rip"]))
            if text:
                print("  instruction: %s" % text)
            if first_diff and first_diff.startswith("fl:"):
                print("  slot: %s" % first_diff)
                print("  native flags: %s (raw %s)" %
                      (" ".join("%s=%d" % (nm, 1 if n_fl & b else 0)
                                for b, nm in FLAG_BITS), n_rec["fl"]))
                print("  emu flags:    %s (raw %s)" %
                      (" ".join("%s=%d" % (nm, 1 if e_fl & b else 0)
                                for b, nm in FLAG_BITS), e_rec["fl"]))
            else:
                print("  slot: %s" % first_diff)
                print("  native: %s" % n_rec[first_diff])
                print("  emu:    %s" % e_rec[first_diff])
            print("  compare-skipped slots this record: %s"
                  % (", ".join(sorted(skip)) or "none"))
            # ONE-LINE machine-greppable summary of the divergence (the
            # run-x86sem-tests FAIL line greps this first, so a remote
            # report like "FAIL x86/vadd: record 993 slot ymm0
            # native=... emu=... rip=..." is actionable without opening
            # the traces; triage.py keeps parsing the multi-line detail
            # above, which is unchanged).
            if first_diff and first_diff.startswith("fl:"):
                print("ONE-LINE: record %d slot %s native=%s emu=%s "
                      "rip=%s instruction=%s"
                      % (i, first_diff, n_rec["fl"], e_rec["fl"],
                         n_rec["rip"],
                         "'%s'" % text if text else "?"))
            else:
                print("ONE-LINE: record %d slot %s native=%s emu=%s "
                      "rip=%s instruction=%s"
                      % (i, first_diff, n_rec[first_diff],
                         e_rec[first_diff], n_rec["rip"],
                         "'%s'" % text if text else "?"))
        if unknown:
            sys.stderr.write("compare-traces: unclassified mnemonic(s) "
                             "treated as flag-preserving: %s\n"
                             % ", ".join(sorted(unknown)))
        return 1
    if not quiet:
        skip_note = ""
        if undefined_skips:
            skip_note = (", %d flag-bit mismatch(es) skipped on "
                         "architecturally-undefined bits" % undefined_skips)
        print("COMPARE OK %d records, %d compared%s"
              % (n_recs, compared, skip_note))
        # One-line stderr summary of the per-bit model over the whole
        # trace (lands in <dir>/cmp.err): per bit, the entries held
        # defined (compared) / undefined (tolerated).
        sys.stderr.write(
            "compare-traces: flag-bit entries compared/undefined: %s\n"
            % " ".join("%s %d/%d" % (name, bit_cmp[j], bit_und[j])
                       for j, (bit, name) in enumerate(FLAG_BITS)))
    if unknown:
        sys.stderr.write("compare-traces: unclassified mnemonic(s) "
                         "treated as flag-preserving: %s\n"
                         % ", ".join(sorted(unknown)))
    return 0


def main(argv):
    args = [a for a in argv[1:] if a != "-q"]
    quiet = "-q" in argv[1:]
    if len(args) < 2:
        sys.stderr.write(
            "usage: compare-traces.py <native.strace> <emu.strace> "
            "[test-binary] [-q]\n")
        return 2
    binary = args[2] if len(args) > 2 else None
    return compare(args[0], args[1], binary, quiet)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
