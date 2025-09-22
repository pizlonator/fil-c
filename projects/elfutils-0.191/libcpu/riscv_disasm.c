/* Disassembler for RISC-V.
   Copyright (C) 2019 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2019.

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
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libeblP.h"

#define MACHINE_ENCODING LITTLE_ENDIAN
#include "memory-access.h"


#define ADD_CHAR(ch) \
  do {									      \
    if (unlikely (bufcnt == bufsize))					      \
      goto enomem;							      \
    buf[bufcnt++] = (ch);						      \
  } while (0)

#define ADD_STRING(str) \
  do {									      \
    const char *_str0 = (str);						      \
    size_t _len0 = strlen (_str0);					      \
    ADD_NSTRING (_str0, _len0);						      \
  } while (0)

#define ADD_NSTRING(str, len) \
  do {									      \
    const char *_str = (str);						      \
    size_t _len = (len);						      \
    if (unlikely (bufcnt + _len > bufsize))				      \
      goto enomem;							      \
    memcpy (buf + bufcnt, _str, _len);					      \
    bufcnt += _len;							      \
  } while (0)


static const char *regnames[32] =
  {
    "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
    "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
    "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
  };
#define REG(nr) ((char *) regnames[nr])
#define REGP(nr) REG (8 + (nr))


static const char *fregnames[32] =
  {
    "ft0", "ft1", "ft2", "ft3", "ft4", "ft5", "ft6", "ft7",
    "fs0", "fs1", "fa0", "fa1", "fa2", "fa3", "fa4", "fa5",
    "fa6", "fa7", "fs2", "fs3", "fs4", "fs5", "fs6", "fs7",
    "fs8", "fs9", "fs10", "fs11", "ft8", "ft9", "ft10", "ft11"
  };
#define FREG(nr) ((char *) fregnames[nr])
#define FREGP(nr) FREG (8 + (nr))


struct known_csrs
  {
    uint16_t nr;
    const char *name;
  };

static int compare_csr (const void *a, const void *b)
{
  const struct known_csrs *ka = (const struct known_csrs *) a;
  const struct known_csrs *kb = (const struct known_csrs *) b;
  if (ka->nr < kb->nr)
    return -1;
  return ka->nr == kb->nr ? 0 : 1;
}


int
riscv_disasm (Ebl *ebl,
	      const uint8_t **startp, const uint8_t *end, GElf_Addr addr,
	      const char *fmt, DisasmOutputCB_t outcb,
	      DisasmGetSymCB_t symcb __attribute__((unused)),
	      void *outcbarg, void *symcbarg __attribute__((unused)))
{
  const char *const save_fmt = fmt;

#define BUFSIZE 512
  char initbuf[BUFSIZE];
  size_t bufcnt;
  size_t bufsize = BUFSIZE;
  char *buf = initbuf;

  int retval = 0;
  while (1)
    {
      const uint8_t *data = *startp;
      assert (data <= end);
      if (data + 2 > end)
	{
	  if (data != end)
	    retval = -1;
	  break;
	}
      uint16_t first = read_2ubyte_unaligned (data);

      // Determine length.
      size_t length;
      if ((first & 0x3) != 0x3)
	length = 2;
      else if ((first & 0x1f) != 0x1f)
	length = 4;
      else if ((first & 0x3f) != 0x3f)
	length = 6;
      else if ((first & 0x7f) != 0x7f)
	length = 8;
      else
	{
	  uint16_t nnn = (first >> 12) & 0x7;
	  if (nnn != 0x7)
	    length = 10 + 2 * nnn;
	  else
	    // This is invalid as of the RISC-V spec on 2019-06-21.
	    // The instruction is at least 192 bits in size so use
	    // this minimum size.
	    length = 24;
	}
      if (data + length > end)
	{
	  retval = -1;
	  break;
	}

      char *mne = NULL;
      char mnebuf[32];
      char *op[5] = { NULL, NULL, NULL, NULL, NULL };
      char immbuf[32];
      size_t len;
      char *strp = NULL;
      char addrbuf[32];
      bufcnt = 0;
      int64_t opaddr;
      if (length == 2)
	{
	  size_t idx = (first >> 13) * 3 + (first & 0x3);
	  switch (idx)
	    {
	    uint16_t rd;
	    uint16_t rs1;
	    uint16_t rs2;

	    case 0:
	      if ((first & 0x1fe0) != 0)
		{
		  mne = "addi";
		  op[0] = REGP ((first & 0x1c) >> 2);
		  op[1] = REG (2);
		  opaddr = (((first >> 1) & 0x3c0)
			    | ((first >> 7) & 0x30)
			    | ((first >> 2) & 0x8)
			    | ((first >> 4) & 0x4));
		  snprintf (addrbuf, sizeof (addrbuf), "%" PRIu64, opaddr);
		  op[2] = addrbuf;
		}
	      else if (first == 0)
		mne = "unimp";
	      break;
	    case 1:
	      rs1 = (first >> 7) & 0x1f;
	      int16_t nzimm = ((0 - ((first >> 7) & 0x20))
			       | ((first >> 2) & 0x1f));
	      if (rs1 == 0)
	        mne = nzimm == 0 ? "nop" : "c.nop";
	      else
		{
		  mne = nzimm == 0 ? "c.addi" : "addi";
		  op[0] = op[1] = REG (rs1);
		  snprintf (addrbuf, sizeof (addrbuf), "%" PRId16, nzimm);
		  op[2] = addrbuf;
		}
	      break;
	    case 2:
	      rs1 = (first >> 7) & 0x1f;
	      op[0] = op[1] = REG (rs1);
	      opaddr = ((first >> 7) & 0x20) | ((first >> 2) & 0x1f);
	      snprintf (addrbuf, sizeof (addrbuf), "0x%" PRIx64, opaddr);
	      op[2] = addrbuf;
	      mne = rs1 == 0 ? "c.slli" : "slli";
	      break;
	    case 3:
	      op[0] = FREGP ((first >> 2) & 0x7);
	      opaddr = ((first << 1) & 0xc0) | ((first >> 7) & 0x38);
	      snprintf (addrbuf, sizeof (addrbuf), "%" PRIu64 "(%s)",
			opaddr, REGP ((first >> 7) & 0x7));
	      op[1] = addrbuf;
	      mne = "fld";
	      break;
	    case 4:
	      if (ebl->class == ELFCLASS32)
		{
		  mne = "jal";
		  opaddr = (((first << 3) & 0x20) | ((first >> 2) & 0xe)
			    | ((first << 1) & 0x80) | ((first >> 1) | 0x40)
			    | ((first << 2) & 0x400) | (first & 0xb00)
			    | ((first >> 6) & 0x10));
		  snprintf (addrbuf, sizeof (addrbuf), "0x%" PRIx64, opaddr);
		  op[0] = addrbuf;
		}
	      else
		{
		  int32_t imm = (((UINT32_C (0) - ((first >> 12) & 0x1)) << 5)
				 | ((first >> 2) & 0x1f));
		  uint16_t reg = (first >> 7) & 0x1f;
		  if (reg == 0)
		    {
		      // Reserved
		      len = snprintf (addrbuf, sizeof (addrbuf), "0x%" PRIx16, first);
		      strp = addrbuf;
		    }
		  else
		    {
		      if (imm == 0)
			mne = "sext.w";
		      else
			{
			  mne = "addiw";
			  snprintf (addrbuf, sizeof (addrbuf), "%" PRId32, imm);
			  op[2] = addrbuf;
			}
		      op[0] = op[1] = REG (reg);
		    }
		}
	      break;
	    case 5:
	      op[0] = FREG ((first >> 7) & 0x1f);
	      opaddr = ((first << 4) & 0x1c0) | ((first >> 7) & 0x20) | ((first >> 2) & 0x18);
	      snprintf (addrbuf, sizeof (addrbuf), "%" PRIu64 "(%s)", opaddr, REG (2));
	      op[1] = addrbuf;
	      mne = "fld";
	      break;
	    case 6:
	    case 18:
	      mne = idx == 6 ? "lw" : "sw";
	      op[0] = REGP ((first >> 2) & 0x7);
	      opaddr = (((first >> 7) & 0x38) | ((first << 1) & 0x40)
			| ((first >> 4) & 0x4));
	      snprintf (addrbuf, sizeof (addrbuf), "%" PRId64 "(%s)",
			opaddr, REGP ((first >> 7) & 0x7));
	      op[1] = addrbuf;
	      break;
	    case 7:
	      mne = (first & 0xf80) == 0 ? "c.li" : "li";
	      op[0] = REG((first >> 7) & 0x1f);
	      snprintf (addrbuf, sizeof (addrbuf), "%" PRId16,
			(UINT16_C (0) - ((first >> 7) & 0x20)) | ((first >> 2) & 0x1f));
	      op[1] = addrbuf;
	      break;
	    case 8:
	      rd = ((first >> 7) & 0x1f);
	      if (rd == 0)
		{
		  len = snprintf (addrbuf, sizeof (addrbuf), "0x%" PRIx16, first);
		  strp = addrbuf;
		}
	      else
		{
		  uint16_t uimm = (((first << 4) & 0xc0)
				   | ((first >> 7) & 0x20)
				   | ((first >> 2) & 0x1c));
		  mne = "lw";
		  op[0] = REG (rd);
		  snprintf (addrbuf, sizeof (addrbuf), "%" PRIu16 "(%s)", uimm, REG (2));
		  op[1] = addrbuf;
		}
	      break;
	    case 9:
	      if (ebl->class == ELFCLASS32)
		{
		  mne = "flw";
		  op[0] = FREGP ((first >> 2) & 0x7);
		  opaddr = (((first << 1) & 0x40)
		            | ((first >> 7) & 0x38)
			    | ((first >> 4) & 0x4));
		}
	      else
		{
		  mne = "ld";
		  op[0] = REGP ((first >> 2) & 0x7);
		  opaddr = ((first >> 7) & 0x38) | ((first << 1) & 0xc0);
		}
	      snprintf (addrbuf, sizeof (addrbuf), "%" PRId64 "(%s)",
			opaddr, REGP ((first >> 7) & 0x7));
	      op[1] = addrbuf;
	      break;
	    case 10:
	      if ((first & 0xf80) == (2 << 7))
		{
		  mne = "addi";
		  op[0] = op[1] = REG (2);
		  opaddr = (((first >> 2) & 0x10) | ((first << 3) & 0x20)
			    | ((first << 1) & 0x40) | ((first << 4) & 0x180)
			    | ((UINT64_C (0) - ((first >> 12) & 0x1)) << 9));
		  snprintf (addrbuf, sizeof (addrbuf), "%" PRId64, opaddr);
		  op[2] = addrbuf;
		}
	      else
		{
		  mne = "lui";
		  op[0] = REG((first & 0xf80) >> 7);
		  opaddr = (((UINT64_C (0) - ((first >> 12) & 0x1)) & ~0x1f)
			    | ((first >> 2) & 0x1f));
		  snprintf (addrbuf, sizeof (addrbuf), "0x%" PRIx64, opaddr & 0xfffff);
		  op[1] = addrbuf;
		}
	      break;
	    case 11:
	      if (ebl->class == ELFCLASS32)
		{
		  mne = "flw";
		  op[0] = FREG ((first >> 7) & 0x1f);
		  opaddr = (((first << 4) & 0xc0)
			    | ((first >> 7) & 0x20)
			    | ((first >> 2) & 0x1c));
		}
	      else
		{
		  mne = "ld";
		  op[0] = REG ((first >> 7) & 0x1f);
		  opaddr = (((first << 4) & 0x1c0)
			    | ((first >> 7) & 0x20)
			    | ((first >> 2) & 0x18));
		}
	      snprintf (addrbuf, sizeof (addrbuf), "%" PRId64 "(%s)", opaddr, REG (2));
	      op[1] = addrbuf;
	      break;
	    case 13:
	      if ((first & 0xc00) != 0xc00)
		{
		  int16_t imm = ((first >> 7) & 0x20) | ((first >> 2) & 0x1f);
		  if ((first & 0xc00) == 0x800)
		    {
		      imm |= 0 - (imm & 0x20);
		      mne = "andi";
		      snprintf (addrbuf, sizeof (addrbuf), "%" PRId16, imm);
		    }
		  else
		    {
		      if (ebl->class != ELFCLASS32 || imm < 32)
			{
			  mne = (first & 0x400) ? "srai" : "srli";
			  if (imm == 0)
			    {
			      strcpy (stpcpy (mnebuf, "c."), mne);
			      mne = mnebuf;
			    }
			}
		      snprintf (addrbuf, sizeof (addrbuf), "0x%" PRIx16, imm);
		    }
		  op[2] = addrbuf;
		}
	      else
		{
		  op[2] = REGP ((first >> 2) & 0x7);
		  static const char *const arithmne[8] =
		    {
		      "sub", "xor", "or", "and", "subw", "addw", NULL, NULL
		    };
		  mne = (char *) arithmne[((first >> 10) & 0x4) | ((first >> 5) & 0x3)];
		}
		op[0] = op[1] = REGP ((first >> 7) & 0x7);
	      break;
	    case 14:
	      rs1 = (first >> 7) & 0x1f;
	      rs2 = (first >> 2) & 0x1f;
	      op[0] = REG (rs1);
	      if ((first & 0x1000) == 0)
		{
		  if (rs2 == 0)
		    {
		      op[1] = NULL;
		      if (rs1 == 1)
			{
			  mne = "ret";
			  op[0] = NULL;
			}
		      else
			mne = "jr";
		    }
		  else
		    {
		      mne = rs1 != 0 ? "mv" : "c.mv";
		      op[1] = REG (rs2);
		    }
		}
	      else
		{
		  if (rs2 == 0)
		    {
		      if (rs1 == 0)
			{
			  mne = "ebreak";
			  op[0] = op[1] = NULL;
			}
		      else
			mne = "jalr";
		    }
		  else
		    {
		      mne = rs1 != 0 ? "add" : "c.add";
		      op[2] = REG (rs2);
		      op[1] = op[0];
		    }
		}
	      break;
	    case 15:
	      op[0] = FREGP ((first >> 2) & 0x7);
	      opaddr = ((first << 1) & 0xc0) | ((first >> 7) & 0x38);
	      snprintf (addrbuf, sizeof (addrbuf), "%" PRIu64 "(%s)",
			opaddr, REGP ((first >> 7) & 0x7));
	      op[1] = addrbuf;
	      mne = "fsd";
	      break;
	    case 16:
	      opaddr = (((UINT64_C (0) - ((first >> 12) & 0x1)) << 11)
			| ((first << 2) & 0x400)
			| ((first >> 1) & 0x300)
			| ((first << 1) & 0x80)
			| ((first >> 1) & 0x40)
			| ((first << 3) & 0x20)
			| ((first >> 7) & 0x10)
			| ((first >> 2) & 0xe));
	      mne = "j";
	      // TODO translate address
	      snprintf (addrbuf, sizeof (addrbuf), "0x%" PRIx64, addr + opaddr);
	      op[0] = addrbuf;
	      break;
	    case 17:
	      op[0] = FREG ((first >> 2) & 0x1f);
	      opaddr = ((first >> 1) & 0x1c0) | ((first >> 7) & 0x38);
	      snprintf (addrbuf, sizeof (addrbuf), "%" PRIu64 "(%s)", opaddr, REG (2));
	      op[1] = addrbuf;
	      mne = "fsd";
	      break;
	    case 19:
	    case 22:
	      mne = idx == 19 ? "beqz" : "bnez";
	      op[0] = REG (8 + ((first >> 7) & 0x7));
	      opaddr = addr + (((UINT64_C (0) - ((first >> 12) & 0x1)) & ~0xff)
			       | ((first << 1) & 0xc0) | ((first << 3) & 0x20)
			       | ((first >> 7) & 0x18) |  ((first >> 2) & 0x6));
	      // TODO translate address
	      snprintf (addrbuf, sizeof (addrbuf), "0x%" PRIx64, opaddr);
	      op[1] = addrbuf;
	      break;
	    case 20:
	      op[0] = REG ((first >> 2) & 0x1f);
	      opaddr = ((first >> 1) & 0xc0) | ((first >> 7) & 0x3c);
	      snprintf (addrbuf, sizeof (addrbuf), "%" PRId64 "(%s)", opaddr, REG (2));
	      op[1] = addrbuf;
	      mne = "sw";
	      break;
	    case 21:
	      if (idx == 18 || ebl->class == ELFCLASS32)
		{
		  mne = "fsw";
		  op[0] = FREGP ((first >> 2) & 0x7);
		  opaddr = (((first >> 7) & 0x38) | ((first << 1) & 0x40)
			    | ((first >> 4) & 0x4));
		}
	      else
		{
		  mne = "sd";
		  op[0] = REGP ((first >> 2) & 0x7);
		  opaddr = ((first >> 7) & 0x38) | ((first << 1) & 0xc0);
		}
	      snprintf (addrbuf, sizeof (addrbuf), "%" PRId64 "(%s)",
			opaddr, REGP ((first >> 7) & 0x7));
	      op[1] = addrbuf;
	      break;
	    case 23:
	      if (idx == 18 || ebl->class == ELFCLASS32)
		{
		  mne = "fsw";
		  op[0] = FREG ((first & 0x7c) >> 2);
		  opaddr = ((first & 0x1e00) >> 7) | ((first & 0x180) >> 1);
		}
	      else
		{
		  mne = "sd";
		  op[0] = REG ((first & 0x7c) >> 2);
		  opaddr = ((first & 0x1c00) >> 7) | ((first & 0x380) >> 1);
		}
	      snprintf (addrbuf, sizeof (addrbuf), "%" PRId64 "(%s)", opaddr, REG (2));
	      op[1] = addrbuf;
	      break;
	    default:
	      break;
	    }

	  if (strp == NULL && mne == NULL)
	    {
	      len = snprintf (immbuf, sizeof (immbuf), "0x%04" PRIx16, first);
	      strp = immbuf;
	    }
	}
      else if (length == 4)
	{
	  uint32_t word = read_4ubyte_unaligned (data);
	  size_t idx = (word >> 2) & 0x1f;

	  switch (idx)
	    {
	    static const char widthchar[4] = { 's', 'd', '\0', 'q' };
	    static const char intwidthchar[4] = { 'w', 'd', '\0', 'q' };
	    static const char *const rndmode[8] = { "rne", "rtz", "rdn", "rup", "rmm", "???", "???", "dyn" };
	    uint32_t rd;
	    uint32_t rs1;
	    uint32_t rs2;
	    uint32_t rs3;
	    uint32_t func;

	    case 0x00:
	    case 0x01:
	      // LOAD and LOAD-FP
	      rd = (word >> 7) & 0x1f;
	      op[0] = idx == 0x00 ? REG (rd) : FREG (rd);
	      opaddr = ((int32_t) word) >> 20;
	      snprintf (addrbuf, sizeof (addrbuf), "%" PRId64 "(%s)",
			opaddr, REG ((word >> 15) & 0x1f));
	      op[1] = addrbuf;
	      func = (word >> 12) & 0x7;
	      static const char *const loadmne[8] =
	        {
	          "lb", "lh", "lw", "ld", "lbu", "lhu", "lwu", NULL
	        };
	      static const char *const floadmne[8] =
		{
		  NULL, NULL, "flw", "fld", "flq", NULL, NULL, NULL
		};
	      mne = (char *) (idx == 0x00 ? loadmne[func] : floadmne[func]);
	      break;
	    case 0x03:
	      // MISC-MEM
	      rd = (word >> 7) & 0x1f;
	      rs1 = (word >> 15) & 0x1f;
	      func = (word >> 12) & 0x7;

	      if (word == 0x8330000f)
		mne = "fence.tso";
	      else if (word == 0x0000100f)
		mne = "fence.i";
	      else if (func == 0 && rd == 0 && rs1 == 0 && (word & 0xf0000000) == 0)
		{
		  static const char *const order[16] =
		    {
		      "unknown", "w", "r", "rw", "o", "ow", "or", "orw",
		      "i", "iw", "ir", "irw", "io", "iow", "ior", "iorw"
		    };
		  uint32_t pred = (word >> 20) & 0xf;
		  uint32_t succ = (word >> 24) & 0xf;
		  if (pred != 0xf || succ != 0xf)
		    {
		      op[0] = (char *) order[succ];
		      op[1] = (char *) order[pred];
		     }
		   mne = "fence";
		}
	      break;
	    case 0x04:
	    case 0x06:
	      // OP-IMM and OP-IMM32
	      rd = (word >> 7) & 0x1f;
	      op[0] = REG (rd);
	      rs1 = (word >> 15) & 0x1f;
	      op[1] = REG (rs1);
	      opaddr = ((int32_t) word) >> 20;
	      static const char *const opimmmne[8] =
		{
		  "addi", NULL, "slti", "sltiu", "xori", NULL, "ori", "andi"
		};
	      func = (word >> 12) & 0x7;
	      mne = (char *) opimmmne[func];
	      if (mne == NULL)
		{
		  const uint64_t shiftmask = ebl->class == ELFCLASS32 ? 0x1f : 0x3f;
		  if (func == 0x1 && (opaddr & ~shiftmask) == 0)
		    mne = "slli";
		  else if (func == 0x5 && (opaddr & ~shiftmask) == 0)
		    mne = "srli";
		  else if (func == 0x5 && (opaddr & ~shiftmask) == 0x400)
		    mne = "srai";
		  snprintf (addrbuf, sizeof (addrbuf), "0x%" PRIx64, opaddr & shiftmask);
		  op[2] = addrbuf;
		}
	      else if (func == 0x0 && (rd != 0 || idx == 0x06) && rs1 == 0 && rd != 0)
		{
		  mne = "li";
		  snprintf (addrbuf, sizeof (addrbuf), "%" PRId64, opaddr);
		  op[1] = addrbuf;
		}
	      else if (func == 0x00 && opaddr == 0)
		{
		  if (idx == 0x06)
		    mne ="sext.";
		  else if (rd == 0)
		    {
		      mne = "nop";
		      op[0] = op[1] = NULL;
		    }
		  else
		    mne = "mv";
		}
	      else if (func == 0x3 && opaddr == 1)
		mne = "seqz";
	      else if (func == 0x4 && opaddr == -1)
		{
		  mne = "not";
		  op[2] = NULL;
		}
	      else
		{
		  snprintf (addrbuf, sizeof (addrbuf), "%" PRId64, opaddr);
		  op[2] = addrbuf;

		  if (func == 0x0 && rs1 == 0 && rd != 0)
		    {
		      op[1] = op[2];
		      op[2] = NULL;
		      mne = "li";
		    }
		}
	      if (mne != NULL && idx == 0x06)
		{
		  mne = strcpy (mnebuf, mne);
		  strcat (mnebuf, "w");
		}
	      break;
	    case 0x05:
	    case 0x0d:
	      // LUI and AUIPC
	      mne = idx == 0x05 ? "auipc" : "lui";
	      op[0] = REG ((word >> 7) & 0x1f);
	      opaddr = word >> 12;
	      snprintf (addrbuf, sizeof (addrbuf), "0x%" PRIx64, opaddr);
	      op[1] = addrbuf;
	      break;
	    case 0x08:
	    case 0x09:
	      // STORE and STORE-FP
	      rs2 = (word >> 20) & 0x1f;
	      op[0] = idx == 0x08 ? REG (rs2) : FREG (rs2);
	      opaddr = ((((int64_t) ((int32_t) word) >> 20)) & ~0x1f) | ((word >> 7) & 0x1f);
	      snprintf (addrbuf, sizeof (addrbuf), "%" PRId64 "(%s)",
			opaddr, REG ((word >> 15) & 0x1f));
	      op[1] = addrbuf;
	      func = (word >> 12) & 0x7;
	      static const char *const storemne[8] =
		{
		  "sb", "sh", "sw", "sd", NULL, NULL, NULL, NULL
		};
	      static const char *const fstoremne[8] =
		{
		  NULL, NULL, "fsw", "fsd", "fsq", NULL, NULL, NULL
		};
	      mne = (char *) (idx == 0x08 ? storemne[func] : fstoremne[func]);
	      break;
	    case 0x0b:
	      // AMO
	      op[0] = REG ((word >> 7) & 0x1f);
	      rs1 = (word >> 15) & 0x1f;
	      rs2 = (word >> 20) & 0x1f;
	      snprintf (addrbuf, sizeof (addrbuf), "(%s)", REG (rs1));
	      op[2] = addrbuf;
	      size_t width = (word >> 12) & 0x7;
	      func = word >> 27;
	      static const char *const amomne[32] =
		{
		  "amoadd", "amoswap", "lr", "sc", "amoxor", NULL, NULL, NULL,
		  "amoor", NULL, NULL, NULL, "amoand", NULL, NULL, NULL,
		  "amomin", NULL, NULL, NULL, "amomax", NULL, NULL, NULL,
		  "amominu", NULL, NULL, NULL, "amomaxu", NULL, NULL, NULL
		};
	      if (amomne[func] != NULL && width >= 2 && width <= 3
		  && (func != 0x02 || rs2 == 0))
		{
		  if (func == 0x02)
		    {
		      op[1] = op[2];
		      op[2] = NULL;
		    }
		  else
		    op[1] = REG (rs2);

		  char *cp = stpcpy (mnebuf, amomne[func]);
		  *cp++ = '.';
		  *cp++ = "  wd    "[width];
		  assert (cp[-1] != ' ');
		  static const char *const aqrlstr[4] =
		    {
		      "", ".rl", ".aq", ".aqrl"
		    };
		  strcpy (cp, aqrlstr[(word >> 25) & 0x3]);
		  mne = mnebuf;
		}
	      break;
	    case 0x0c:
	    case 0x0e:
	      // OP and OP-32
	      if ((word & 0xbc000000) == 0)
		{
		  rs1 = (word >> 15) & 0x1f;
		  rs2 = (word >> 20) & 0x1f;
		  op[0] = REG ((word >> 7) & 0x1f);
		  func = ((word >> 21) & 0x10) | ((word >> 27) & 0x8) | ((word >> 12) & 0x7);
		  static const char *const arithmne2[32] =
		    {
		      "add", "sll", "slt", "sltu", "xor", "srl", "or", "and",
		      "sub", NULL, NULL, NULL, NULL, "sra", NULL, NULL,
		      "mul", "mulh", "mulhsu", "mulhu", "div", "divu", "rem", "remu",
		      NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
		    };
		  static const char *const arithmne3[32] =
		    {
		      "addw", "sllw", NULL, NULL, NULL, "srlw", NULL, NULL,
		      "subw", NULL, NULL, NULL, NULL, "sraw", NULL, NULL,
		      "mulw", NULL, NULL, NULL, "divw", "divuw", "remw", "remuw",
		      NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
		    };
		  if (func == 8 && rs1 == 0)
		    {
		      mne = idx == 0x0c ? "neg" : "negw";
		      op[1] = REG (rs2);
		    }
		  else if (idx == 0x0c && rs2 == 0 && func == 2)
		    {
		      op[1] = REG (rs1);
		      mne = "sltz";
		    }
		  else if (idx == 0x0c && rs1 == 0 && (func == 2 || func == 3))
		    {
		      op[1] = REG (rs2);
		      mne = func == 2 ? "sgtz" : "snez";
		    }
		  else
		    {
		      mne = (char *) (idx == 0x0c ? arithmne2[func] : arithmne3[func]);
		      op[1] = REG (rs1);
		      op[2] = REG (rs2);
		    }
		}
	      break;
	    case 0x10:
	    case 0x11:
	    case 0x12:
	    case 0x13:
	      // MADD, MSUB, NMSUB, NMADD
	      if ((word & 0x06000000) != 0x04000000)
		{
		  rd = (word >> 7) & 0x1f;
		  rs1 = (word >> 15) & 0x1f;
		  rs2 = (word >> 20) & 0x1f;
		  rs3 = (word >> 27) & 0x1f;
		  uint32_t rm = (word >> 12) & 0x7;
		  width = (word >> 25) & 0x3;

		  static const char *const fmamne[4] =
		    {
		      "fmadd.", "fmsub.", "fnmsub.", "fnmadd."
		    };
		  char *cp = stpcpy (mnebuf, fmamne[idx & 0x3]);
		  *cp++ = widthchar[width];
		  *cp = '\0';
		  mne = mnebuf;
		  op[0] = FREG (rd);
		  op[1] = FREG (rs1);
		  op[2] = FREG (rs2);
		  op[3] = FREG (rs3);
		  if (rm != 0x7)
		    op[4] = (char *) rndmode[rm];
		}
	      break;
	    case 0x14:
	      // OP-FP
	      if ((word & 0x06000000) != 0x04000000)
		{
		  width = (word >> 25) & 0x3;
		  rd = (word >> 7) & 0x1f;
		  rs1 = (word >> 15) & 0x1f;
		  rs2 = (word >> 20) & 0x1f;
		  func = word >> 27;
		  uint32_t rm = (word >> 12) & 0x7;
		  if (func < 4)
		    {
		      static const char *const fpop[4] =
			{
			  "fadd", "fsub", "fmul", "fdiv"
			};
		      char *cp = stpcpy (mnebuf, fpop[func]);
		      *cp++ = '.';
		      *cp++ = widthchar[width];
		      *cp = '\0';
		      mne = mnebuf;
		      op[0] = FREG (rd);
		      op[1] = FREG (rs1);
		      op[2] = FREG (rs2);
		      if (rm != 0x7)
			op[3] = (char *) rndmode[rm];
		    }
		  else if (func == 0x1c && width != 2 && rs2 == 0 && rm <= 1)
		    {
		      char *cp;
		      if (rm == 0)
			{
			  cp = stpcpy (mnebuf, "fmv.x.");
			  *cp++ = intwidthchar[width];
			}
		      else
			{
			  cp = stpcpy (mnebuf, "fclass.");
			  *cp++ = widthchar[width];
			}
		      *cp = '\0';
		      mne = mnebuf;
		      op[0] = REG (rd);
		      op[1] = FREG (rs1);
		    }
		  else if (func == 0x1e && width != 2 && rs2 == 0 && rm == 0)
		    {
		      char *cp = stpcpy (mnebuf, "fmv.");
		      *cp++ = intwidthchar[width];
		      strcpy (cp, ".x");
		      mne = mnebuf;
		      op[0] = FREG (rd);
		      op[1] = REG (rs1);
		    }
		  else if (func == 0x14)
		    {
		      uint32_t cmpop = (word >> 12) & 0x7;
		      if (cmpop < 3)
			{
			  static const char *const mnefpcmp[3] =
			    {
			      "fle", "flt", "feq"
			    };
			  char *cp = stpcpy (mnebuf, mnefpcmp[cmpop]);
			  *cp++ = '.';
			  *cp++ = widthchar[width];
			  *cp = '\0';
			  mne = mnebuf;
			  op[0] = REG (rd);
			  op[1] = FREG (rs1);
			  op[2] = FREG (rs2);
			}
		    }
		  else if (func == 0x04)
		    {
		      uint32_t cmpop = (word >> 12) & 0x7;
		      if (cmpop < 3)
			{
			  op[0] = FREG (rd);
			  op[1] = FREG (rs1);

			  static const char *const mnefpcmp[3] =
			    {
			      "fsgnj.", "fsgnjn.", "fsgnjx."
			    };
			  static const char *const altsignmne[3] =
			    {
			      "fmv.", "fneg.", "fabs."
			    };
			  char *cp = stpcpy (mnebuf, rs1 == rs2 ? altsignmne[cmpop] : mnefpcmp[cmpop]);
			  *cp++ = widthchar[width];
			  *cp = '\0';
			  mne = mnebuf;

			  if (rs1 != rs2)
			    op[2] = FREG (rs2);
			}
		    }
		  else if (func == 0x08 && width != 2 && rs2 <= 3 && rs2 != 2 && rs2 != width)
		    {
		      op[0] = FREG (rd);
		      op[1] = FREG (rs1);
		      char *cp = stpcpy (mnebuf, "fcvt.");
		      *cp++ = widthchar[width];
		      *cp++ = '.';
		      *cp++ = widthchar[rs2];
		      *cp = '\0';
		      mne = mnebuf;
		    }
		  else if ((func & 0x1d) == 0x18 && width != 2 && rs2 < 4)
		    {
		      char *cp = stpcpy (mnebuf, "fcvt.");
		      if (func == 0x18)
			{
			  *cp++ = rs2 >= 2 ? 'l' : 'w';
			  if ((rs2 & 1) == 1)
			    *cp++ = 'u';
			  *cp++ = '.';
			  *cp++ = widthchar[width];
			  *cp = '\0';
			  op[0] = REG (rd);
			  op[1] = FREG (rs1);
			}
		      else
			{
			  *cp++ = widthchar[width];
			  *cp++ = '.';
			  *cp++ = rs2 >= 2 ? 'l' : 'w';
			  if ((rs2 & 1) == 1)
			    *cp++ = 'u';
			  *cp = '\0';
			  op[0] = FREG (rd);
			  op[1] = REG (rs1);
			}
		      mne = mnebuf;
		      if (rm != 0x7 && (func == 0x18 || width == 0 || rs2 >= 2))
			op[2] = (char *) rndmode[rm];
		    }
		  else if (func == 0x0b && rs2 == 0)
		    {
		      op[0] = FREG (rd);
		      op[1] = FREG (rs1);
		      char *cp = stpcpy (mnebuf, "fsqrt.");
		      *cp++ = widthchar[width];
		      *cp = '\0';
		      mne = mnebuf;
		      if (rm != 0x7)
			op[2] = (char *) rndmode[rm];
		    }
		  else if (func == 0x05 && rm < 2)
		    {
		      op[0] = FREG (rd);
		      op[1] = FREG (rs1);
		      op[2] = FREG (rs2);
		      char *cp = stpcpy (mnebuf, rm == 0 ? "fmin." : "fmax.");
		      *cp++ = widthchar[width];
		      *cp = '\0';
		      mne = mnebuf;
		    }
		  else if (func == 0x14 && rm <= 0x2)
		    {
		      op[0] = REG (rd);
		      op[1] = FREG (rs1);
		      op[2] = FREG (rs2);
		      static const char *const fltcmpmne[3] =
			{
			  "fle.", "flt.", "feq."
			};
		      char *cp = stpcpy (mnebuf, fltcmpmne[rm]);
		      *cp++ = widthchar[width];
		      *cp = '\0';
		      mne = mnebuf;
		    }
		}
	      break;
	    case 0x18:
	      // BRANCH
	      rs1 = (word >> 15) & 0x1f;
	      op[0] = REG (rs1);
	      rs2 = (word >> 20) & 0x1f;
	      op[1] = REG (rs2);
	      opaddr = addr + (((UINT64_C (0) - (word >> 31)) << 12)
			       + ((word << 4) & 0x800)
			       + ((word >> 20) & 0x7e0)
			       + ((word >> 7) & 0x1e));
	      // TODO translate address
	      snprintf (addrbuf, sizeof (addrbuf), "0x%" PRIx64, opaddr);
	      op[2] = addrbuf;
	      static const char *const branchmne[8] =
		{
		  "beq", "bne", NULL, NULL, "blt", "bge", "bltu", "bgeu"
		};
	      func = (word >> 12) & 0x7;
	      mne = (char *) branchmne[func];
	      if (rs1 == 0 && func == 5)
		{
		  op[0] = op[1];
		  op[1] = op[2];
		  op[2] = NULL;
		  mne = "blez";
		}
	      else if (rs1 == 0 && func == 4)
		{
		  op[0] = op[1];
		  op[1] = op[2];
		  op[2] = NULL;
		  mne = "bgtz";
		}
	      else if (rs2 == 0)
		{
		  if (func == 0 || func == 1 || func == 4 || func == 5)
		    {
		      op[1] = op[2];
		      op[2] = NULL;
		      strcpy (stpcpy (mnebuf, mne), "z");
		      mne = mnebuf;
		    }
		}
	      else if (func == 5 || func == 7)
		{
		  // binutils use these opcodes and the reverse parameter order
		  char *tmp = op[0];
		  op[0] = op[1];
		  op[1] = tmp;
		  mne = func == 5 ? "ble" : "bleu";
		}
	      break;
	    case 0x19:
	      // JALR
	      if ((word & 0x7000) == 0)
		{
		  rd = (word >> 7) & 0x1f;
		  rs1 = (word >> 15) & 0x1f;
		  opaddr = (int32_t) word >> 20;
		  size_t next = 0;
		  if (rd > 1)
		    op[next++] = REG (rd);
		  if (opaddr == 0)
		    {
		      if (rs1 != 0 || next == 0)
			op[next] = REG (rs1);
		    }
		  else
		    {
		      snprintf (addrbuf, sizeof (addrbuf), "%" PRId64 "(%s)", opaddr, REG (rs1));
		      op[next] = addrbuf;
		    }
		  mne = rd == 0 ? "jr" : "jalr";
		}
	      break;
	    case 0x1b:
	      // JAL
	      rd = (word >> 7) & 0x1f;
	      if (rd != 0)
		op[0] = REG (rd);
	      opaddr = addr + ((UINT64_C (0) - ((word >> 11) & 0x100000))
			       | (word & 0xff000)
			       | ((word >> 9) & 0x800)
			       | ((word >> 20) & 0x7fe));
	      // TODO translate address
	      snprintf (addrbuf, sizeof (addrbuf), "0x%" PRIx64, opaddr);
	      op[rd != 0] = addrbuf;
	      mne = rd == 0 ? "j" : "jal";
	      break;
	    case 0x1c:
	      // SYSTEM
	      rd = (word >> 7) & 0x1f;
	      rs1 = (word >> 15) & 0x1f;
	      if (word == 0x00000073)
		mne = "ecall";
	      else if (word == 0x00100073)
		mne = "ebreak";
	      else if (word == 0x00200073)
		mne = "uret";
	      else if (word == 0x10200073)
		mne = "sret";
	      else if (word == 0x30200073)
		mne = "mret";
	      else if (word == 0x10500073)
		mne = "wfi";
	      else if ((word & 0x3000) == 0x2000 && rs1 == 0)
		{
		  uint32_t csr = word >> 20;
		  if (/* csr >= 0x000 && */ csr <= 0x007)
		    {
		      static const char *const unprivrw[4] =
			{
			  NULL, "frflags", "frrm", "frsr",
			};
		      mne = (char *) unprivrw[csr - 0x000];
		    }
		  else if (csr >= 0xc00 && csr <= 0xc03)
		    {
		      static const char *const unprivrolow[3] =
			{
			  "rdcycle", "rdtime", "rdinstret"
			};
		      mne = (char *) unprivrolow[csr - 0xc00];
		    }
		  op[0] = REG ((word >> 7) & 0x1f);
		}
	      else if ((word & 0x3000) == 0x1000 && rd == 0)
		{
		  uint32_t csr = word >> 20;
		  if (/* csr >= 0x000 && */ csr <= 0x003)
		    {
		      static const char *const unprivrs[4] =
			{
			  NULL, "fsflags", "fsrm", "fssr",
			};
		      static const char *const unprivrsi[4] =
			{
			  NULL, "fsflagsi", "fsrmi", NULL
			};
		      mne = (char *) ((word & 0x4000) == 0 ? unprivrs : unprivrsi)[csr - 0x000];

		      if ((word & 0x4000) == 0)
			op[0] = REG ((word >> 15) & 0x1f);
		      else
			{
			  snprintf (immbuf, sizeof (immbuf), "%" PRIu32, (word >> 15) & 0x1f);
			  op[0] = immbuf;
			}
		    }
		}
	      if (mne == NULL && (word & 0x3000) != 0)
		{
		  static const char *const mnecsr[8] =
		    {
		      NULL, "csrrw", "csrrs", "csrrc",
		      NULL, "csrrwi", "csrrsi", "csrrci"
		    };
		  static const struct known_csrs known[] =
		    {
		      // This list must remain sorted by NR.
		      { 0x000, "ustatus" },
		      { 0x001, "fflags" },
		      { 0x002, "fram" },
		      { 0x003, "fcsr" },
		      { 0x004, "uie" },
		      { 0x005, "utvec" },
		      { 0x040, "uscratch" },
		      { 0x041, "uepc" },
		      { 0x042, "ucause" },
		      { 0x043, "utval" },
		      { 0x044, "uip" },
		      { 0x100, "sstatus" },
		      { 0x102, "sedeleg" },
		      { 0x103, "sideleg" },
		      { 0x104, "sie" },
		      { 0x105, "stvec" },
		      { 0x106, "scounteren" },
		      { 0x140, "sscratch" },
		      { 0x141, "sepc" },
		      { 0x142, "scause" },
		      { 0x143, "stval" },
		      { 0x144, "sip" },
		      { 0x180, "satp" },
		      { 0x200, "vsstatus" },
		      { 0x204, "vsie" },
		      { 0x205, "vstvec" },
		      { 0x240, "vsscratch" },
		      { 0x241, "vsepc" },
		      { 0x242, "vscause" },
		      { 0x243, "vstval" },
		      { 0x244, "vsip" },
		      { 0x280, "vsatp" },
		      { 0x600, "hstatus" },
		      { 0x602, "hedeleg" },
		      { 0x603, "hideleg" },
		      { 0x605, "htimedelta" },
		      { 0x606, "hcounteren" },
		      { 0x615, "htimedeltah" },
		      { 0x680, "hgatp" },
		      { 0xc00, "cycle" },
		      { 0xc01, "time" },
		      { 0xc02, "instret" },
		      { 0xc03, "hpmcounter3" },
		      { 0xc04, "hpmcounter4" },
		      { 0xc05, "hpmcounter5" },
		      { 0xc06, "hpmcounter6" },
		      { 0xc07, "hpmcounter7" },
		      { 0xc08, "hpmcounter8" },
		      { 0xc09, "hpmcounter9" },
		      { 0xc0a, "hpmcounter10" },
		      { 0xc0b, "hpmcounter11" },
		      { 0xc0c, "hpmcounter12" },
		      { 0xc0d, "hpmcounter13" },
		      { 0xc0e, "hpmcounter14" },
		      { 0xc0f, "hpmcounter15" },
		      { 0xc10, "hpmcounter16" },
		      { 0xc11, "hpmcounter17" },
		      { 0xc12, "hpmcounter18" },
		      { 0xc13, "hpmcounter19" },
		      { 0xc14, "hpmcounter20" },
		      { 0xc15, "hpmcounter21" },
		      { 0xc16, "hpmcounter22" },
		      { 0xc17, "hpmcounter23" },
		      { 0xc18, "hpmcounter24" },
		      { 0xc19, "hpmcounter25" },
		      { 0xc1a, "hpmcounter26" },
		      { 0xc1b, "hpmcounter27" },
		      { 0xc1c, "hpmcounter28" },
		      { 0xc1d, "hpmcounter29" },
		      { 0xc1e, "hpmcounter30" },
		      { 0xc1f, "hpmcounter31" },
		      { 0xc80, "cycleh" },
		      { 0xc81, "timeh" },
		      { 0xc82, "instreth" },
		      { 0xc83, "hpmcounter3h" },
		      { 0xc84, "hpmcounter4h" },
		      { 0xc85, "hpmcounter5h" },
		      { 0xc86, "hpmcounter6h" },
		      { 0xc87, "hpmcounter7h" },
		      { 0xc88, "hpmcounter8h" },
		      { 0xc89, "hpmcounter9h" },
		      { 0xc8a, "hpmcounter10h" },
		      { 0xc8b, "hpmcounter11h" },
		      { 0xc8c, "hpmcounter12h" },
		      { 0xc8d, "hpmcounter13h" },
		      { 0xc8e, "hpmcounter14h" },
		      { 0xc8f, "hpmcounter15h" },
		      { 0xc90, "hpmcounter16h" },
		      { 0xc91, "hpmcounter17h" },
		      { 0xc92, "hpmcounter18h" },
		      { 0xc93, "hpmcounter19h" },
		      { 0xc94, "hpmcounter20h" },
		      { 0xc95, "hpmcounter21h" },
		      { 0xc96, "hpmcounter22h" },
		      { 0xc97, "hpmcounter23h" },
		      { 0xc98, "hpmcounter24h" },
		      { 0xc99, "hpmcounter25h" },
		      { 0xc9a, "hpmcounter26h" },
		      { 0xc9b, "hpmcounter27h" },
		      { 0xc9c, "hpmcounter28h" },
		      { 0xc9d, "hpmcounter29h" },
		      { 0xc9e, "hpmcounter30h" },
		      { 0xc9f, "hpmcounter31h" },
		    };
		  uint32_t csr = word >> 20;
		  uint32_t instr = (word >> 12) & 0x7;
		  size_t last = 0;
		  if (rd != 0)
		    op[last++] = REG (rd);
		  struct known_csrs key = { csr, NULL };
		  struct known_csrs *found = bsearch (&key, known,
						      sizeof (known) / sizeof (known[0]),
						      sizeof (known[0]),
						      compare_csr);
		  if (found)
		    op[last] = (char *) found->name;
		  else
		    {
		      snprintf (addrbuf, sizeof (addrbuf), "0x%" PRIx32, csr);
		      op[last] = addrbuf;
		    }
		  ++last;
		  if ((word & 0x4000) == 0)
		    op[last] = REG ((word >> 15) & 0x1f);
		  else
		    {
		      snprintf (immbuf, sizeof (immbuf), "%" PRIu32, (word >> 15) & UINT32_C(0x1f));
		      op[last] = immbuf;
		    }
		  if (instr == 1 && rd == 0)
		    mne = "csrw";
		  else if (instr == 2 && rd == 0)
		    mne = "csrs";
		  else if (instr == 6 && rd == 0)
		    mne = "csrsi";
		  else if (instr == 2 && rs1 == 0)
		    mne = "csrr";
		  else if (instr == 3 && rd == 0)
		    mne = "csrc";
		  else
		    mne = (char *) mnecsr[instr];
		}
	      break;
	    default:
	      break;
	    }

	  if (strp == NULL && mne == NULL)
	    {
	      len = snprintf (addrbuf, sizeof (addrbuf), "0x%08" PRIx32, word);
	      strp = addrbuf;
	    }
	}
      else
	{
	  // No instruction encodings defined for these sizes yet.
	  char *cp = stpcpy (mnebuf, "0x");
	  assert (length % 2 == 0);
	  for (size_t i = 0; i < length; i += 2)
	    cp += snprintf (cp, mnebuf + sizeof (mnebuf) - cp, "%04" PRIx16,
			    read_2ubyte_unaligned (data + i));
	  strp = mnebuf;
	  len = cp - mnebuf;
	}

      if (strp == NULL)
	{

	  if (0)
	    {
	      /* Resize the buffer.  */
	      char *oldbuf;
	    enomem:
	      oldbuf = buf;
	      if (buf == initbuf)
		buf = malloc (2 * bufsize);
	      else
		buf = realloc (buf, 2 * bufsize);
	      if (buf == NULL)
		{
		  buf = oldbuf;
		  retval = ENOMEM;
		  goto do_ret;
		}
	      bufsize *= 2;

	      bufcnt = 0;
	    }

	  unsigned long string_end_idx = 0;
	  fmt = save_fmt;
	  const char *deferred_start = NULL;
	  size_t deferred_len = 0;
	  // XXX Can we get this from color.c?
	  static const char color_off[] = "\e[0m";
	  while (*fmt != '\0')
	    {
	      if (*fmt != '%')
		{
		  char ch = *fmt++;
		  if (ch == '\\')
		    {
		      switch ((ch = *fmt++))
			{
			case '0' ... '7':
			  {
			    int val = ch - '0';
			    ch = *fmt;
			    if (ch >= '0' && ch <= '7')
			      {
				val *= 8;
				val += ch - '0';
				ch = *++fmt;
				if (ch >= '0' && ch <= '7' && val < 32)
				  {
				    val *= 8;
				    val += ch - '0';
				    ++fmt;
				  }
			      }
			    ch = val;
			  }
			  break;

			case 'n':
			  ch = '\n';
			  break;

			case 't':
			  ch = '\t';
			  break;

			default:
			  retval = EINVAL;
			  goto do_ret;
			}
		    }
		  else if (ch == '\e' && *fmt == '[')
		    {
		      deferred_start = fmt - 1;
		      do
			++fmt;
		      while (*fmt != 'm' && *fmt != '\0');

		      if (*fmt == 'm')
			{
			  deferred_len = ++fmt - deferred_start;
			  continue;
			}

		      fmt = deferred_start + 1;
		      deferred_start = NULL;
		    }
		  ADD_CHAR (ch);
		  continue;
		}
	      ++fmt;

	      int width = 0;
	      while (isdigit (*fmt))
		width = width * 10 + (*fmt++ - '0');

	      int prec = 0;
	      if (*fmt == '.')
		while (isdigit (*++fmt))
		  prec = prec * 10 + (*fmt - '0');

	      size_t start_idx = bufcnt;
	      size_t non_printing = 0;
	      switch (*fmt++)
		{
		case 'm':
		  if (deferred_start != NULL)
		    {
		      ADD_NSTRING (deferred_start, deferred_len);
		      non_printing += deferred_len;
		    }

		  ADD_STRING (mne);

		  if (deferred_start != NULL)
		    {
		      ADD_STRING (color_off);
		      non_printing += strlen (color_off);
		    }

		  string_end_idx = bufcnt;
		  break;

		case 'o':
		  if (op[prec - 1] != NULL)
		    {
		      if (deferred_start != NULL)
			{
			  ADD_NSTRING (deferred_start, deferred_len);
			  non_printing += deferred_len;
			}

		      ADD_STRING (op[prec - 1]);

		      if (deferred_start != NULL)
			{
			  ADD_STRING (color_off);
			  non_printing += strlen (color_off);
			}

		      string_end_idx = bufcnt;
		    }
		  else
		    bufcnt = string_end_idx;
		  break;

		case 'e':
		  string_end_idx = bufcnt;
		  break;

		case 'a':
		  /* Pad to requested column.  */
		  while (bufcnt - non_printing < (size_t) width)
		    ADD_CHAR (' ');
		  width = 0;
		  break;

		case 'l':
		  // TODO
		  break;

		default:
		  abort();
		}

	      /* Pad according to the specified width.  */
	      while (bufcnt - non_printing < start_idx + width)
		ADD_CHAR (' ');
	    }

	  strp = buf;
	  len = bufcnt;
	}

      addr += length;
      *startp = data + length;
      retval = outcb (strp, len, outcbarg);
      if (retval != 0)
	break;
    }

 do_ret:
  if (buf != initbuf)
    free (buf);

  return retval;
}
