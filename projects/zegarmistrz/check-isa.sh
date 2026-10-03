#!/bin/bash
# Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
# check-isa.sh: extract x86-64 mnemonics from binaries (objdump) and report
# which ones have no handler in src/decode_exec.cpp.
# Usage: ./check-isa.sh [binary...] (defaults: ./hello + test guests + libc)
set -u
cd "$(dirname "$0")"

# Mnemonics known-handled: ZYDIS_MNEMONIC_* referenced in decode_exec.cpp.
grep -o "ZYDIS_MNEMONIC_[A-Z0-9]*" src/decode_exec.cpp \
    | sed 's/ZYDIS_MNEMONIC_//' | sort -u > /tmp/zeg_handled.txt
grep -o "ZYDIS_MNEMONIC_[A-Z0-9]*" /usr/include/Zydis/Generated/EnumMnemonic.h \
    | sed 's/ZYDIS_MNEMONIC_//' | sort -u > /tmp/zydis_all.txt

# Normalize an objdump (AT&T, lowercase) mnemonic to Zydis style (uppercase).
# Handles AT&T size suffixes (b/w/l/q), Jcc/CMOVcc/SETcc aliases (ja->JNBE),
# and x87 load/store size suffixes (fldl->FLD).
norm() {
    local m="$1"
    m=$(echo "$m" | tr 'a-z' 'A-Z')
    case "$m" in
        MOVABS|MOVABSQ) echo MOV; return ;;
        # Jcc aliases -> Zydis canonical (JO/JNO/JB/JNB/JZ/JNZ/...).
        JA|JNA|JNBE) echo JNBE; return ;;
        JAE|JNAE|JNB|JNC) echo JNB; return ;;
        JB|JC|JNAE) echo JB; return ;;
        JBE|JNA) echo JBE; return ;;
        JE|JZ) echo JZ; return ;;
        JNE|JNZ) echo JNZ; return ;;
        JG|JNLE) echo JNLE; return ;;
        JGE|JNL) echo JNL; return ;;
        JL|JNGE) echo JL; return ;;
        JLE|JNG) echo JLE; return ;;
        JNO) echo JNO; return ;;
        JNP|JPO) echo JNP; return ;;
        JNS) echo JNS; return ;;
        JO) echo JO; return ;;
        JP|JPE) echo JP; return ;;
        JS) echo JS; return ;;
        # CMOVcc aliases.
        CMOVA|CMOVNBE) echo CMOVNBE; return ;;
        CMOVAE|CMOVNB|CMOVNC) echo CMOVNB; return ;;
        CMOVB|CMOVC|CMOVNAE) echo CMOVB; return ;;
        CMOVBE|CMOVNA) echo CMOVBE; return ;;
        CMOVE|CMOVZ) echo CMOVZ; return ;;
        CMOVNE|CMOVNZ) echo CMOVNZ; return ;;
        CMOVG|CMOVNLE) echo CMOVNLE; return ;;
        CMOVGE|CMOVNL) echo CMOVNL; return ;;
        CMOVL|CMOVNGE) echo CMOVL; return ;;
        CMOVLE|CMOVNG) echo CMOVLE; return ;;
        CMOVNO) echo CMOVNO; return ;;
        CMOVNP|CMOVPO) echo CMOVNP; return ;;
        CMOVNS) echo CMOVNS; return ;;
        CMOVO) echo CMOVO; return ;;
        CMOVP|CMOVPE) echo CMOVP; return ;;
        CMOVS) echo CMOVS; return ;;
        # x87 size suffixes.
        FILDL|FILDLL|FILDS) echo FILD; return ;;
        FLDL|FLDS|FLDT|FLDL2T|FLDL2E|FLDLG2|FLDLN2) echo "$m" | sed 's/FLD.*/FLD/;s/FLDL2T/FLDL2T/;s/FLDL2E/FLDL2E/;s/FLDLG2/FLDLG2/;s/FLDLN2/FLDLN2/' | head -1; return ;;
        FSTPL|FSTPS) echo FSTP; return ;;
        FSTPT) echo FSTP; return ;;
        FSTL) echo FST; return ;;
        FMULS|FMULL) echo FMUL; return ;;
        FADDS|FADDL) echo FADD; return ;;
        FSUBS|FSUBL|FSUBRS|FSUBRL) echo "$m" | sed 's/S$//;s/L$//' ; return ;;
        FDIVS|FDIVL|FDIVRS|FDIVRL) echo "$m" | sed 's/S$//;s/L$//' ; return ;;
        FCHS|FABS) echo "$m"; return ;;
        CLTD) echo CDQ; return ;;
        CLTQ) echo CDQE; return ;;
        CQTO) echo CQO; return ;;
        CWTL) echo CWDE; return ;;
        CWTD) echo CWD; return ;;
        CBTW) echo CBW; return ;;
        VPCMPEQB|VPCMPNEQB) echo VPCMPB; return ;;
        VPCMPEQW|VPCMPNEQW) echo VPCMPW; return ;;
        VPCMPEQD|VPCMPNEQD) echo VPCMPD; return ;;
        VPCMPEQQ|VPCMPNEQQ) echo VPCMPQ; return ;;
        VPCMPLTUB|VPCMPNEQUB|VPCMPLEUB|VPCMPGTUB|VPCMPGEUB|VPCMPEQUB) echo VPCMPUB; return ;;
        VPCMPLTUW|VPCMPNEQUW|VPCMPLEUW|VPCMPGTUW|VPCMPGEUW|VPCMPEQUW) echo VPCMPUW; return ;;
        VPCMPLTUD|VPCMPNEQUD|VPCMPLEUD|VPCMPGTUD|VPCMPGEUD|VPCMPEQUD) echo VPCMPUD; return ;;
        VPCMPLTUQ|VPCMPNEQUQ|VPCMPLEUQ|VPCMPGTUQ|VPCMPGEUQ|VPCMPEQUQ) echo VPCMPUQ; return ;;
        VPCMPLTB|VPCMPLEB|VPCMPGTB|VPCMPGEB|VPCMPNLEB) echo VPCMPB; return ;;
        VPCMPLTW|VPCMPLEW|VPCMPGTW|VPCMPGEW) echo VPCMPW; return ;;
        VPCMPLTD|VPCMPLED|VPCMPGTD|VPCMPGED) echo VPCMPD; return ;;
        VPCMPLTQ|VPCMPLEQ|VPCMPGTQ|VPCMPGEQ) echo VPCMPQ; return ;;
        # objdump spells VPCLMULQDQ with the operand selection baked into
        # the mnemonic: vpclmullqlqdq/vpclmulhqlqdq/vpclmullqhqdq/
        # vpclmulhqhqdq (imm8 0x00/0x01/0x10/0x11).
        VPCLMULLQLQDQ|VPCLMULHQLQDQ|VPCLMULLQHQDQ|VPCLMULHQHQDQ) echo VPCLMULQDQ; return ;;
        ADDR32) echo ADDR32; return ;;
        MOVSLQ|MOVSL) echo MOVSXD; return ;;
        MOVZB|MOVZBL|MOVZBW|MOVZW|MOVZWL) echo MOVZX; return ;;
        MOVSB|MOVSW) echo "$m"; return ;;
        SETA|SETNBE) echo SETNBE; return ;;
        SETAE|SETNB|SETNC) echo SETNB; return ;;
        SETB|SETC|SETNAE) echo SETB; return ;;
        SETBE|SETNA) echo SETBE; return ;;
        SETE|SETZ) echo SETZ; return ;;
        SETNE|SETNZ) echo SETNZ; return ;;
        SETG|SETNLE) echo SETNLE; return ;;
        SETGE|SETNL) echo SETNL; return ;;
        SETL|SETNGE) echo SETL; return ;;
        SETLE|SETNG) echo SETLE; return ;;
        SETNO) echo SETNO; return ;;
        SETNP|SETPO) echo SETNP; return ;;
        SETNS) echo SETNS; return ;;
        SETO) echo SETO; return ;;
        SETP|SETPE) echo SETP; return ;;
        SETS) echo SETS; return ;;
    esac
    # Strip one AT&T data-size suffix for plain integer ops.
    case "$m" in
        *B|*W|*L|*Q)
            base="${m%?}"
            # Keep FPU/vector names intact (checked against Zydis list later).
            echo "$base $m" ;;
        *) echo "$m" ;;
    esac
}

# Resolve a normalized name against the Zydis list. norm() may emit
# "base full" for suffix-stripped names; try full first, then base.
resolve() {
    local m="$1"
    if grep -qx "$m" /tmp/zydis_all.txt; then echo "$m"; return; fi
    set -- $m
    if [ $# = 2 ]; then
        if grep -qx "$2" /tmp/zydis_all.txt; then echo "$2"; return; fi
        if grep -qx "$1" /tmp/zydis_all.txt; then echo "$1"; return; fi
    fi
    echo "$m"
}
BINS=("$@")
if [ ${#BINS[@]} -eq 0 ]; then
    BINS=(./hello)
    for g in test-output/*; do
        [ -x "$g" ] && [ -f "$g" ] && BINS+=("$g")
    done
    for s in /lib/x86_64-linux-gnu/libc.so.6 /lib64/ld-linux-x86-64.so.2; do
        [ -f "$s" ] && BINS+=("$s")
    done
fi

# Regression guard: always scan a tiny object with VPCLMULQDQ using all
# four imm8 operand selectors (0x00/0x01/0x10/0x11). objdump spells each
# with a different alias mnemonic, so a missed alias in norm() shows up
# as MISSING here instead of only biting when some binary happens to use
# that immediate.
if command -v gcc >/dev/null 2>&1 \
   && gcc -O2 -mavx -mvpclmulqdq -mpclmul -c -o /dev/null -x c /dev/null 2>/dev/null; then
    mkdir -p test-output
    cat > test-output/vpclmul_aliases.c <<'EOF'
#include <wmmintrin.h>
__m128i f00(__m128i a, __m128i b) { return _mm_clmulepi64_si128(a, b, 0x00); }
__m128i f01(__m128i a, __m128i b) { return _mm_clmulepi64_si128(a, b, 0x01); }
__m128i f10(__m128i a, __m128i b) { return _mm_clmulepi64_si128(a, b, 0x10); }
__m128i f11(__m128i a, __m128i b) { return _mm_clmulepi64_si128(a, b, 0x11); }
EOF
    # -mavx -mpclmul is required on GCC 12: -mvpclmulqdq alone does not
    # define __PCLMUL__, and -mavx is what makes gcc emit the VEX-encoded
    # (vp-prefixed) forms that this guard is about.
    if gcc -O2 -mavx -mvpclmulqdq -mpclmul -c -o test-output/vpclmul_aliases.o \
           test-output/vpclmul_aliases.c 2>/dev/null; then
        BINS+=(test-output/vpclmul_aliases.o)
    fi
fi

# shellcheck disable=SC2068
objdump -d --no-show-raw-insn ${BINS[@]} 2>/dev/null \
    | grep -oE ":[[:space:]]+[a-z][a-z0-9]*" \
    | awk '{print $2}' | sort -u > /tmp/zeg_found_raw.txt

> /tmp/zeg_found.txt
while read -r m; do
    case "$m" in
        file|data16|data32|addr32|rex*|lock|rep|repe|repne|repz|notrack|bnd|hint|cs|ds|es|fs|gs|ss) continue ;;
        # Unsupported-by-design (clean guest error; documented in README):
        # CET shadow-stack (would #UD natively without CET), TSX (host has
        # no RTM/HLE, so guests never use it).
        rstorssp|saveprevssp|xabort|xbegin|xend|xtest) continue ;;
    esac
    resolve "$(norm "$m")" >> /tmp/zeg_found.txt
done < /tmp/zeg_found_raw.txt
sort -u -o /tmp/zeg_found.txt /tmp/zeg_found.txt

MISS=0
while read -r m; do
    if ! grep -qx "$m" /tmp/zeg_handled.txt; then
        echo "MISSING: $m"
        MISS=1
    fi
done < /tmp/zeg_found.txt
[ "$MISS" = 0 ] && echo "check-isa: all mnemonics handled"
exit "$MISS"
