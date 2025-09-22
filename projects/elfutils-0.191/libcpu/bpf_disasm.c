/* Disassembler for BPF.
   Copyright (C) 2016, 2018 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <gelf.h>
#include <inttypes.h>
#include "bpf.h"

#include "common.h"
#include "libeblP.h"

static const char class_string[8][8] = {
  [BPF_LD]    = "ld",
  [BPF_LDX]   = "ldx",
  [BPF_ST]    = "st",
  [BPF_STX]   = "stx",
  [BPF_ALU]   = "alu",
  [BPF_JMP]   = "jmp",
  [BPF_RET]   = "6",		/* completely unused in ebpf */
  [BPF_ALU64] = "alu64",
};


#define REG(N)		"r%" #N "$d"
#define REGU(N)		"(u32)" REG(N)
#define REGS(N)		"(s64)" REG(N)

#define IMMS(N)		"%" #N "$d"
#define IMMX(N)		"%" #N "$#x"

#define OFF(N)		"%" #N "$+d"
#define JMP(N)		"%" #N "$#x"

#define A32(O, S)	REG(1) " = " REGU(1) " " #O " " S
#define A64(O, S)	REG(1) " " #O "= " S
#define J64(D, O, S)	"if " D " " #O " " S " goto " JMP(3)
#define LOAD(T)		REG(1) " = *(" #T " *)(" REG(2) OFF(3) ")"
#define STORE(T, S)	"*(" #T " *)(" REG(1) OFF(3) ") = " S
#define XADD(T, S)	"lock *(" #T " *)(" REG(1) OFF(3) ") += " S
#define LDSKB(T, S)	"r0 = *(" #T " *)skb[" S "]"

static void
bswap_bpf_insn (struct bpf_insn *p)
{
  /* Note that the dst_reg and src_reg fields are 4-bit bitfields.
     That means these two nibbles are (typically) laid out in the
     opposite order between big- and little-endian hosts.  This is
     not required by any standard, but does happen to be true for
     at least ppc, s390, arm and mips as big-endian hosts.  */
  int t = p->dst_reg;
  p->dst_reg = p->src_reg;
  p->src_reg = t;

  /* The other 2 and 4 byte fields are trivially converted.  */
  CONVERT (p->off);
  CONVERT (p->imm);
}

int
bpf_disasm (Ebl *ebl, const uint8_t **startp, const uint8_t *end,
	    GElf_Addr addr, const char *fmt __attribute__((unused)),
	    DisasmOutputCB_t outcb,
	    DisasmGetSymCB_t symcb __attribute__((unused)),
	    void *outcbarg,
	    void *symcbarg __attribute__((unused)))
{
  const bool need_bswap = MY_ELFDATA != ebl->data;
  const uint8_t *start = *startp;
  char buf[128];
  int len, retval = 0;

  while (start + sizeof(struct bpf_insn) <= end)
    {
      struct bpf_insn i;
      unsigned code, class, jmp;
      const char *code_fmt;

      memcpy(&i, start, sizeof(struct bpf_insn));
      if (need_bswap)
	bswap_bpf_insn (&i);

      start += sizeof(struct bpf_insn);
      addr += sizeof(struct bpf_insn);
      jmp = addr + i.off * sizeof(struct bpf_insn);

      code = i.code;
      switch (code)
	{
	case BPF_LD | BPF_IMM | BPF_DW:
	  {
	    struct bpf_insn i2;
	    uint64_t imm64;

	    if (start + sizeof(struct bpf_insn) > end)
	      {
		start -= sizeof(struct bpf_insn);
		*startp = start;
		goto done;
	      }
	    memcpy(&i2, start, sizeof(struct bpf_insn));
	    if (need_bswap)
	      bswap_bpf_insn (&i2);
	    start += sizeof(struct bpf_insn);
	    addr += sizeof(struct bpf_insn);

	    imm64 = (uint32_t)i.imm | ((uint64_t)i2.imm << 32);
	    switch (i.src_reg)
	      {
	      case 0:
		code_fmt = REG(1) " = %2$#" PRIx64;
		break;
	      case BPF_PSEUDO_MAP_FD:
		code_fmt = REG(1) " = map_fd(%2$#" PRIx64 ")";
		break;
	      default:
		code_fmt = REG(1) " = ld_pseudo(%3$d, %2$#" PRIx64 ")";
		break;
	      }
	    len = snprintf(buf, sizeof(buf), code_fmt,
			   i.dst_reg, imm64, i.src_reg);
	  }
	  break;

	case BPF_JMP | BPF_EXIT:
	  len = snprintf(buf, sizeof(buf), "exit");
	  break;
	case BPF_JMP | BPF_JA:
	  len = snprintf(buf, sizeof(buf), "goto " JMP(1), jmp);
	  break;
	case BPF_JMP | BPF_CALL:
	  code_fmt = "call " IMMS(1);
	  goto do_imm;

	case BPF_ALU | BPF_END | BPF_TO_LE:
	  /* The imm field contains {16,32,64}.  */
	  code_fmt = REG(1) " = le" IMMS(2) "(" REG(1) ")";
	  goto do_dst_imm;
	case BPF_ALU | BPF_END | BPF_TO_BE:
	  code_fmt = REG(1) " = be" IMMS(2) "(" REG(1) ")";
	  goto do_dst_imm;

	case BPF_ALU | BPF_ADD | BPF_K:
	  code_fmt = A32(+, IMMS(2));
	  goto do_dst_imm;
	case BPF_ALU | BPF_SUB | BPF_K:
	  code_fmt = A32(-, IMMS(2));
	  goto do_dst_imm;
	case BPF_ALU | BPF_MUL | BPF_K:
	  code_fmt = A32(*, IMMS(2));
	  goto do_dst_imm;
	case BPF_ALU | BPF_DIV | BPF_K:
	  code_fmt = A32(/, IMMS(2));
	  goto do_dst_imm;
	case BPF_ALU | BPF_OR | BPF_K:
	  code_fmt = A32(|, IMMX(2));
	  goto do_dst_imm;
	case BPF_ALU | BPF_AND | BPF_K:
	  code_fmt = A32(&, IMMX(2));
	  goto do_dst_imm;
	case BPF_ALU | BPF_LSH | BPF_K:
	  code_fmt = A32(<<, IMMS(2));
	  goto do_dst_imm;
	case BPF_ALU | BPF_RSH | BPF_K:
	  code_fmt = A32(>>, IMMS(2));
	  goto do_dst_imm;
	case BPF_ALU | BPF_MOD | BPF_K:
	  code_fmt = A32(%%, IMMS(2));
	  goto do_dst_imm;
	case BPF_ALU | BPF_XOR | BPF_K:
	  code_fmt = A32(^, IMMX(2));
	  goto do_dst_imm;
	case BPF_ALU | BPF_MOV | BPF_K:
	  code_fmt = REG(1) " = " IMMX(2);
	  goto do_dst_imm;
	case BPF_ALU | BPF_ARSH | BPF_K:
	  code_fmt = REG(1) " = (u32)((s32)" REG(1) " >> " IMMS(2) ")";
	  goto do_dst_imm;

	case BPF_ALU | BPF_ADD | BPF_X:
	  code_fmt = A32(+, REGU(2));
	  goto do_dst_src;
	case BPF_ALU | BPF_SUB | BPF_X:
	  code_fmt = A32(-, REGU(2));
	  goto do_dst_src;
	case BPF_ALU | BPF_MUL | BPF_X:
	  code_fmt = A32(*, REGU(2));
	  goto do_dst_src;
	case BPF_ALU | BPF_DIV | BPF_X:
	  code_fmt = A32(/, REGU(2));
	  goto do_dst_src;
	case BPF_ALU | BPF_OR | BPF_X:
	  code_fmt = A32(|, REGU(2));
	  goto do_dst_src;
	case BPF_ALU | BPF_AND | BPF_X:
	  code_fmt = A32(&, REGU(2));
	  goto do_dst_src;
	case BPF_ALU | BPF_LSH | BPF_X:
	  code_fmt = A32(<<, REGU(2));
	  goto do_dst_src;
	case BPF_ALU | BPF_RSH | BPF_X:
	  code_fmt = A32(>>, REGU(2));
	  goto do_dst_src;
	case BPF_ALU | BPF_MOD | BPF_X:
	  code_fmt = A32(%%, REGU(2));
	  goto do_dst_src;
	case BPF_ALU | BPF_XOR | BPF_X:
	  code_fmt = A32(^, REGU(2));
	  goto do_dst_src;
	case BPF_ALU | BPF_MOV | BPF_X:
	  code_fmt = REG(1) " = " REGU(2);
	  goto do_dst_src;
	case BPF_ALU | BPF_ARSH | BPF_X:
	  code_fmt = REG(1) " = (u32)((s32)" REG(1) " >> " REG(2) ")";
	  goto do_dst_src;

	case BPF_ALU64 | BPF_ADD | BPF_K:
	  code_fmt = A64(+, IMMS(2));
	  goto do_dst_imm;
	case BPF_ALU64 | BPF_SUB | BPF_K:
	  code_fmt = A64(-, IMMS(2));
	  goto do_dst_imm;
	case BPF_ALU64 | BPF_MUL | BPF_K:
	  code_fmt = A64(*, IMMS(2));
	  goto do_dst_imm;
	case BPF_ALU64 | BPF_DIV | BPF_K:
	  code_fmt = A64(/, IMMS(2));
	  goto do_dst_imm;
	case BPF_ALU64 | BPF_OR | BPF_K:
	  code_fmt = A64(|, IMMS(2));
	  goto do_dst_imm;
	case BPF_ALU64 | BPF_AND | BPF_K:
	  code_fmt = A64(&, IMMS(2));
	  goto do_dst_imm;
	case BPF_ALU64 | BPF_LSH | BPF_K:
	  code_fmt = A64(<<, IMMS(2));
	  goto do_dst_imm;
	case BPF_ALU64 | BPF_RSH | BPF_K:
	  code_fmt = A64(>>, IMMS(2));
	  goto do_dst_imm;
	case BPF_ALU64 | BPF_MOD | BPF_K:
	  code_fmt = A64(%%, IMMS(2));
	  goto do_dst_imm;
	case BPF_ALU64 | BPF_XOR | BPF_K:
	  code_fmt = A64(^, IMMS(2));
	  goto do_dst_imm;
	case BPF_ALU64 | BPF_MOV | BPF_K:
	  code_fmt = REG(1) " = " IMMS(2);
	  goto do_dst_imm;
	case BPF_ALU64 | BPF_ARSH | BPF_K:
	  code_fmt = REG(1) " = (s64)" REG(1) " >> " IMMS(2);
	  goto do_dst_imm;

	case BPF_ALU64 | BPF_ADD | BPF_X:
	  code_fmt = A64(+, REG(2));
	  goto do_dst_src;
	case BPF_ALU64 | BPF_SUB | BPF_X:
	  code_fmt = A64(-, REG(2));
	  goto do_dst_src;
	case BPF_ALU64 | BPF_MUL | BPF_X:
	  code_fmt = A64(*, REG(2));
	  goto do_dst_src;
	case BPF_ALU64 | BPF_DIV | BPF_X:
	  code_fmt = A64(/, REG(2));
	  goto do_dst_src;
	case BPF_ALU64 | BPF_OR | BPF_X:
	  code_fmt = A64(|, REG(2));
	  goto do_dst_src;
	case BPF_ALU64 | BPF_AND | BPF_X:
	  code_fmt = A64(&, REG(2));
	  goto do_dst_src;
	case BPF_ALU64 | BPF_LSH | BPF_X:
	  code_fmt = A64(<<, REG(2));
	  goto do_dst_src;
	case BPF_ALU64 | BPF_RSH | BPF_X:
	  code_fmt = A64(>>, REG(2));
	  goto do_dst_src;
	case BPF_ALU64 | BPF_MOD | BPF_X:
	  code_fmt = A64(%%, REG(2));
	  goto do_dst_src;
	case BPF_ALU64 | BPF_XOR | BPF_X:
	  code_fmt = A64(^, REG(2));
	  goto do_dst_src;
	case BPF_ALU64 | BPF_MOV | BPF_X:
	  code_fmt = REG(1) " = " REG(2);
	  goto do_dst_src;
	case BPF_ALU64 | BPF_ARSH | BPF_X:
	  code_fmt = REG(1) " = (s64)" REG(1) " >> " REG(2);
	  goto do_dst_src;

	case BPF_ALU | BPF_NEG:
	  code_fmt = REG(1) " = (u32)-" REG(1);
	  goto do_dst_src;
	case BPF_ALU64 | BPF_NEG:
	  code_fmt = REG(1) " = -" REG(1);
	  goto do_dst_src;

	case BPF_JMP | BPF_JEQ | BPF_K:
	  code_fmt = J64(REG(1), ==, IMMS(2));
	  goto do_dst_imm_jmp;
	case BPF_JMP | BPF_JGT | BPF_K:
	  code_fmt = J64(REG(1), >, IMMS(2));
	  goto do_dst_imm_jmp;
	case BPF_JMP | BPF_JGE | BPF_K:
	  code_fmt = J64(REG(1), >=, IMMS(2));
	  goto do_dst_imm_jmp;
	case BPF_JMP | BPF_JSET | BPF_K:
	  code_fmt = J64(REG(1), &, IMMS(2));
	  goto do_dst_imm_jmp;
	case BPF_JMP | BPF_JNE | BPF_K:
	  code_fmt = J64(REG(1), !=, IMMS(2));
	  goto do_dst_imm_jmp;
	case BPF_JMP | BPF_JSGT | BPF_K:
	  code_fmt = J64(REGS(1), >, IMMS(2));
	  goto do_dst_imm_jmp;
	case BPF_JMP | BPF_JSGE | BPF_K:
	  code_fmt = J64(REGS(1), >=, IMMS(2));
	  goto do_dst_imm_jmp;
	case BPF_JMP | BPF_JLT | BPF_K:
	  code_fmt = J64(REG(1), <, IMMS(2));
	  goto do_dst_imm_jmp;
	case BPF_JMP | BPF_JLE | BPF_K:
	  code_fmt = J64(REG(1), <=, IMMS(2));
	  goto do_dst_imm_jmp;
	case BPF_JMP | BPF_JSLT | BPF_K:
	  code_fmt = J64(REGS(1), <, IMMS(2));
	  goto do_dst_imm_jmp;
	case BPF_JMP | BPF_JSLE | BPF_K:
	  code_fmt = J64(REGS(1), <=, IMMS(2));
	  goto do_dst_imm_jmp;

	case BPF_JMP | BPF_JEQ | BPF_X:
	  code_fmt = J64(REG(1), ==, REG(2));
	  goto do_dst_src_jmp;
	case BPF_JMP | BPF_JGT | BPF_X:
	  code_fmt = J64(REG(1), >, REG(2));
	  goto do_dst_src_jmp;
	case BPF_JMP | BPF_JGE | BPF_X:
	  code_fmt = J64(REG(1), >=, REG(2));
	  goto do_dst_src_jmp;
	case BPF_JMP | BPF_JSET | BPF_X:
	  code_fmt = J64(REG(1), &, REG(2));
	  goto do_dst_src_jmp;
	case BPF_JMP | BPF_JNE | BPF_X:
	  code_fmt = J64(REG(1), !=, REG(2));
	  goto do_dst_src_jmp;
	case BPF_JMP | BPF_JSGT | BPF_X:
	  code_fmt = J64(REGS(1), >, REGS(2));
	  goto do_dst_src_jmp;
	case BPF_JMP | BPF_JSGE | BPF_X:
	  code_fmt = J64(REGS(1), >=, REGS(2));
	  goto do_dst_src_jmp;
	case BPF_JMP | BPF_JLT | BPF_X:
	  code_fmt = J64(REG(1), <, REG(2));
	  goto do_dst_src_jmp;
	case BPF_JMP | BPF_JLE | BPF_X:
	  code_fmt = J64(REG(1), <=, REG(2));
	  goto do_dst_src_jmp;
	case BPF_JMP | BPF_JSLT | BPF_X:
	  code_fmt = J64(REGS(1), <, REGS(2));
	  goto do_dst_src_jmp;
	case BPF_JMP | BPF_JSLE | BPF_X:
	  code_fmt = J64(REGS(1), <=, REGS(2));
	  goto do_dst_src_jmp;

	case BPF_LDX | BPF_MEM | BPF_B:
	  code_fmt = LOAD(u8);
	  goto do_dst_src_off;
	case BPF_LDX | BPF_MEM | BPF_H:
	  code_fmt = LOAD(u16);
	  goto do_dst_src_off;
	case BPF_LDX | BPF_MEM | BPF_W:
	  code_fmt = LOAD(u32);
	  goto do_dst_src_off;
	case BPF_LDX | BPF_MEM | BPF_DW:
	  code_fmt = LOAD(u64);
	  goto do_dst_src_off;

	case BPF_STX | BPF_MEM | BPF_B:
	  code_fmt = STORE(u8, REG(2));
	  goto do_dst_src_off;
	case BPF_STX | BPF_MEM | BPF_H:
	  code_fmt = STORE(u16, REG(2));
	  goto do_dst_src_off;
	case BPF_STX | BPF_MEM | BPF_W:
	  code_fmt = STORE(u32, REG(2));
	  goto do_dst_src_off;
	case BPF_STX | BPF_MEM | BPF_DW:
	  code_fmt = STORE(u64, REG(2));
	  goto do_dst_src_off;

	case BPF_STX | BPF_XADD | BPF_W:
	  code_fmt = XADD(u32, REG(2));
	  goto do_dst_src_off;
	case BPF_STX | BPF_XADD | BPF_DW:
	  code_fmt = XADD(u64, REG(2));
	  goto do_dst_src_off;

	case BPF_ST | BPF_MEM | BPF_B:
	  code_fmt = STORE(u8, IMMS(2));
	  goto do_dst_imm_off;
	case BPF_ST | BPF_MEM | BPF_H:
	  code_fmt = STORE(u16, IMMS(2));
	  goto do_dst_imm_off;
	case BPF_ST | BPF_MEM | BPF_W:
	  code_fmt = STORE(u32, IMMS(2));
	  goto do_dst_imm_off;
	case BPF_ST | BPF_MEM | BPF_DW:
	  code_fmt = STORE(u64, IMMS(2));
	  goto do_dst_imm_off;

	case BPF_LD | BPF_ABS | BPF_B:
	  code_fmt = LDSKB(u8, IMMS(1));
	  goto do_imm;
	case BPF_LD | BPF_ABS | BPF_H:
	  code_fmt = LDSKB(u16, IMMS(1));
	  goto do_imm;
	case BPF_LD | BPF_ABS | BPF_W:
	  code_fmt = LDSKB(u32, IMMS(1));
	  goto do_imm;

	case BPF_LD | BPF_IND | BPF_B:
	  code_fmt = LDSKB(u8, REG(1) "+" IMMS(2));
	  goto do_src_imm;
	case BPF_LD | BPF_IND | BPF_H:
	  code_fmt = LDSKB(u16, REG(1) "+" IMMS(2));
	  goto do_src_imm;
	case BPF_LD | BPF_IND | BPF_W:
	  code_fmt = LDSKB(u32, REG(1) "+" IMMS(2));
	  goto do_src_imm;

	do_imm:
	  len = snprintf(buf, sizeof(buf), code_fmt, i.imm);
	  break;
	do_dst_imm:
	  len = snprintf(buf, sizeof(buf), code_fmt, i.dst_reg, i.imm);
	  break;
	do_src_imm:
	  len = snprintf(buf, sizeof(buf), code_fmt, i.src_reg, i.imm);
	  break;
	do_dst_src:
	  len = snprintf(buf, sizeof(buf), code_fmt, i.dst_reg, i.src_reg);
	  break;
	do_dst_imm_jmp:
	  len = snprintf(buf, sizeof(buf), code_fmt, i.dst_reg, i.imm, jmp);
	  break;
	do_dst_src_jmp:
	  len = snprintf(buf, sizeof(buf), code_fmt,
			 i.dst_reg, i.src_reg, jmp);
	  break;
	do_dst_imm_off:
	  len = snprintf(buf, sizeof(buf), code_fmt, i.dst_reg, i.imm, i.off);
	  break;
	do_dst_src_off:
	  len = snprintf(buf, sizeof(buf), code_fmt,
			 i.dst_reg, i.src_reg, i.off);
	  break;

	default:
	  class = BPF_CLASS(code);
	  len = snprintf(buf, sizeof(buf), "invalid class %s",
			 class_string[class]);
	  break;
        }

      *startp = start;
      retval = outcb (buf, len, outcbarg);
      if (retval != 0)
	goto done;
    }

 done:
  return retval;
}
