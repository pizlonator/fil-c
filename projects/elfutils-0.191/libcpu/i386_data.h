/* Helper routines for disassembler for x86/x86-64.
   Copyright (C) 2007, 2008 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2007.

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

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <libasm.h>

struct instr_enc
{
  /* The mnemonic.  Especially encoded for the optimized table.  */
  unsigned int mnemonic : MNEMONIC_BITS;

  /* The rep/repe prefixes.  */
  unsigned int rep : 1;
  unsigned int repe : 1;

  /* Mnemonic suffix.  */
  unsigned int suffix : SUFFIX_BITS;

  /* Nonzero if the instruction uses modr/m.  */
  unsigned int modrm : 1;

  /* 1st parameter.  */
  unsigned int fct1 : FCT1_BITS;
#ifdef STR1_BITS
  unsigned int str1 : STR1_BITS;
#endif
  unsigned int off1_1 : OFF1_1_BITS;
  unsigned int off1_2 : OFF1_2_BITS;
  unsigned int off1_3 : OFF1_3_BITS;

  /* 2nd parameter.  */
  unsigned int fct2 : FCT2_BITS;
#ifdef STR2_BITS
  unsigned int str2 : STR2_BITS;
#endif
  unsigned int off2_1 : OFF2_1_BITS;
  unsigned int off2_2 : OFF2_2_BITS;
  unsigned int off2_3 : OFF2_3_BITS;

  /* 3rd parameter.  */
  unsigned int fct3 : FCT3_BITS;
#ifdef STR3_BITS
  unsigned int str3 : STR3_BITS;
#endif
  unsigned int off3_1 : OFF3_1_BITS;
#ifdef OFF3_2_BITS
  unsigned int off3_2 : OFF3_2_BITS;
#endif
#ifdef OFF3_3_BITS
  unsigned int off3_3 : OFF3_3_BITS;
#endif
};


typedef int (*opfct_t) (struct output_data *);


static int
data_prefix (struct output_data *d)
{
  char ch = '\0';
  if (*d->prefixes & has_cs)
    {
      ch = 'c';
      *d->prefixes &= ~has_cs;
    }
  else if (*d->prefixes & has_ds)
    {
      ch = 'd';
      *d->prefixes &= ~has_ds;
    }
  else if (*d->prefixes & has_es)
    {
      ch = 'e';
      *d->prefixes &= ~has_es;
    }
  else if (*d->prefixes & has_fs)
    {
      ch = 'f';
      *d->prefixes &= ~has_fs;
    }
  else if (*d->prefixes & has_gs)
    {
      ch = 'g';
      *d->prefixes &= ~has_gs;
    }
  else if (*d->prefixes & has_ss)
    {
      ch = 's';
      *d->prefixes &= ~has_ss;
    }
  else
    return 0;

  if (*d->bufcntp + 4 > d->bufsize)
    return *d->bufcntp + 4 - d->bufsize;

  d->bufp[(*d->bufcntp)++] = '%';
  d->bufp[(*d->bufcntp)++] = ch;
  d->bufp[(*d->bufcntp)++] = 's';
  d->bufp[(*d->bufcntp)++] = ':';

  return 0;
}

#ifdef X86_64
static const char hiregs[8][4] =
  {
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
  };
static const char aregs[8][4] =
  {
    "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi"
  };
static const char dregs[8][4] =
  {
    "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"
  };
#else
static const char aregs[8][4] =
  {
    "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"
  };
# define dregs aregs
#endif

static int
general_mod$r_m (struct output_data *d)
{
  int r = data_prefix (d);
  if (r != 0)
    return r;

  int prefixes = *d->prefixes;
  const uint8_t *data = &d->data[d->opoff1 / 8];
  char *bufp = d->bufp;
  size_t *bufcntp = d->bufcntp;
  size_t bufsize = d->bufsize;

  uint_fast8_t modrm = data[0];
#ifndef X86_64
  if (unlikely ((prefixes & has_addr16) != 0))
    {
      int16_t disp = 0;
      bool nodisp = false;

      if ((modrm & 0xc7) == 6 || (modrm & 0xc0) == 0x80)
	/* 16 bit displacement.  */
	disp = read_2sbyte_unaligned (&data[1]);
      else if ((modrm & 0xc0) == 0x40)
	/* 8 bit displacement.  */
	disp = *(const int8_t *) &data[1];
      else if ((modrm & 0xc0) == 0)
	nodisp = true;

      char tmpbuf[sizeof ("-0x1234(%rr,%rr)")];
      int n;
      if ((modrm & 0xc7) == 6)
	n = snprintf (tmpbuf, sizeof (tmpbuf), "0x%" PRIx16, disp);
      else
	{
	  n = 0;
	  if (!nodisp)
	    n = snprintf (tmpbuf, sizeof (tmpbuf), "%s0x%" PRIx16,
			  disp < 0 ? "-" : "", disp < 0 ? -disp : disp);

	  if ((modrm & 0x4) == 0)
	    n += snprintf (tmpbuf + n, sizeof (tmpbuf) - n, "(%%b%c,%%%ci)",
			   "xp"[(modrm >> 1) & 1], "sd"[modrm & 1]);
	  else
	    n += snprintf (tmpbuf + n, sizeof (tmpbuf) - n, "(%%%s)",
			   ((const char [4][3]) { "si", "di", "bp", "bx" })[modrm & 3]);
	}

      if (*bufcntp + n + 1 > bufsize)
	return *bufcntp + n + 1 - bufsize;

      memcpy (&bufp[*bufcntp], tmpbuf, n + 1);
      *bufcntp += n;
    }
  else
#endif
    {
      if ((modrm & 7) != 4)
	{
	  int32_t disp = 0;
	  bool nodisp = false;

	  if ((modrm & 0xc7) == 5 || (modrm & 0xc0) == 0x80)
	    /* 32 bit displacement.  */
	    disp = read_4sbyte_unaligned (&data[1]);
	  else if ((modrm & 0xc0) == 0x40)
	    /* 8 bit displacement.  */
	    disp = *(const int8_t *) &data[1];
	  else if ((modrm & 0xc0) == 0)
	    nodisp = true;

	  char tmpbuf[sizeof ("-0x12345678(%rrrr)")];
	  int n;
	  if (nodisp)
	    {
	      n = snprintf (tmpbuf, sizeof (tmpbuf), "(%%%s)",
#ifdef X86_64
			    (prefixes & has_rex_b) ? hiregs[modrm & 7] :
#endif
			    aregs[modrm & 7]);
#ifdef X86_64
	      if (prefixes & has_addr16)
		{
		  if (prefixes & has_rex_b)
		    tmpbuf[n++] = 'd';
		  else
		    tmpbuf[2] = 'e';
		}
#endif
	    }
	  else if ((modrm & 0xc7) != 5)
	    {
	      int p;
	      n = snprintf (tmpbuf, sizeof (tmpbuf), "%s0x%" PRIx32 "(%%%n%s)",
			    disp < 0 ? "-" : "", disp < 0 ? -disp : disp, &p,
#ifdef X86_64
			    (prefixes & has_rex_b) ? hiregs[modrm & 7] :
#endif
			    aregs[modrm & 7]);
#ifdef X86_64
	      if (prefixes & has_addr16)
		{
		  if (prefixes & has_rex_b)
		    tmpbuf[n++] = 'd';
		  else
		    tmpbuf[p] = 'e';
		}
#endif
	    }
	  else
	    {
#ifdef X86_64
	      n = snprintf (tmpbuf, sizeof (tmpbuf), "%s0x%" PRIx32 "(%%rip)",
			    disp < 0 ? "-" : "", disp < 0 ? -disp : disp);

	      d->symaddr_use = addr_rel_always;
	      d->symaddr = disp;
#else
	      n = snprintf (tmpbuf, sizeof (tmpbuf), "0x%" PRIx32, disp);
#endif
	    }

	  if (*bufcntp + n + 1 > bufsize)
	    return *bufcntp + n + 1 - bufsize;

	  memcpy (&bufp[*bufcntp], tmpbuf, n + 1);
	  *bufcntp += n;
	}
      else
	{
	  /* SIB */
	  uint_fast8_t sib = data[1];
	  int32_t disp = 0;
	  bool nodisp = false;

	  if ((modrm & 0xc7) == 5 || (modrm & 0xc0) == 0x80
	      || ((modrm & 0xc7) == 0x4 && (sib & 0x7) == 0x5))
	    /* 32 bit displacement.  */
	    disp = read_4sbyte_unaligned (&data[2]);
	  else if ((modrm & 0xc0) == 0x40)
	    /* 8 bit displacement.  */
	    disp = *(const int8_t *) &data[2];
	  else
	    nodisp = true;

	  char tmpbuf[sizeof ("-0x12345678(%rrrr,%rrrr,N)")];
	  char *cp = tmpbuf;
	  int n;
	  if ((modrm & 0xc0) != 0 || (sib & 0x3f) != 0x25
#ifdef X86_64
	      || (prefixes & has_rex_x) != 0
#endif
	      )
	    {
	      if (!nodisp)
		{
		  n = snprintf (cp, sizeof (tmpbuf), "%s0x%" PRIx32,
				disp < 0 ? "-" : "", disp < 0 ? -disp : disp);
		  cp += n;
		}

	      *cp++ = '(';

	      if ((modrm & 0xc7) != 0x4 || (sib & 0x7) != 0x5)
		{
		  *cp++ = '%';
		  cp = stpcpy (cp,
#ifdef X86_64
			       (prefixes & has_rex_b) ? hiregs[sib & 7] :
			       (prefixes & has_addr16) ? dregs[sib & 7] :
#endif
			       aregs[sib & 7]);
#ifdef X86_64
		  if ((prefixes & (has_rex_b | has_addr16))
		      == (has_rex_b | has_addr16))
		    *cp++ = 'd';
#endif
		}

	      if ((sib & 0x38) != 0x20
#ifdef X86_64
		  || (prefixes & has_rex_x) != 0
#endif
		  )
		{
		  *cp++ = ',';
		  *cp++ = '%';
		  cp = stpcpy (cp,
#ifdef X86_64
			       (prefixes & has_rex_x)
			       ? hiregs[(sib >> 3) & 7] :
			       (prefixes & has_addr16)
			       ? dregs[(sib >> 3) & 7] :
#endif
			       aregs[(sib >> 3) & 7]);
#ifdef X86_64
		  if ((prefixes & (has_rex_b | has_addr16))
		      == (has_rex_b | has_addr16))
		    *cp++ = 'd';
#endif

		  *cp++ = ',';
		  *cp++ = '0' + (1 << (sib >> 6));
		}

	      *cp++ = ')';
	    }
	  else
	    {
	      assert (! nodisp);
#ifdef X86_64
	      if ((prefixes & has_addr16) == 0)
		n = snprintf (cp, sizeof (tmpbuf), "0x%" PRIx64,
			      (int64_t) disp);
	      else
#endif
		n = snprintf (cp, sizeof (tmpbuf), "0x%" PRIx32, disp);
	      cp += n;
	    }

	  if (*bufcntp + (cp - tmpbuf) > bufsize)
	    return *bufcntp + (cp - tmpbuf) - bufsize;

	  memcpy (&bufp[*bufcntp], tmpbuf, cp - tmpbuf);
	  *bufcntp += cp - tmpbuf;
	}
    }
  return 0;
}


static int
FCT_MOD$R_M (struct output_data *d)
{
  assert (d->opoff1 % 8 == 0);
  uint_fast8_t modrm = d->data[d->opoff1 / 8];
  if ((modrm & 0xc0) == 0xc0)
    {
      assert (d->opoff1 / 8 == d->opoff2 / 8);
      assert (d->opoff2 % 8 == 5);
      //uint_fast8_t byte = d->data[d->opoff2 / 8] & 7;
      uint_fast8_t byte = modrm & 7;

      size_t *bufcntp = d->bufcntp;
      char *buf = d->bufp + *bufcntp;
      size_t avail = d->bufsize - *bufcntp;
      int needed;
      if (*d->prefixes & (has_rep | has_repne))
	needed = snprintf (buf, avail, "%%%s", dregs[byte]);
      else
	needed = snprintf (buf, avail, "%%mm%" PRIxFAST8, byte);
      if ((size_t) needed > avail)
	return needed - avail;
      *bufcntp += needed;
      return 0;
    }

  return general_mod$r_m (d);
}


static int
FCT_Mod$R_m (struct output_data *d)
{
  assert (d->opoff1 % 8 == 0);
  uint_fast8_t modrm = d->data[d->opoff1 / 8];
  if ((modrm & 0xc0) == 0xc0)
    {
      assert (d->opoff1 / 8 == d->opoff2 / 8);
      assert (d->opoff2 % 8 == 5);
      //uint_fast8_t byte = data[opoff2 / 8] & 7;
      uint_fast8_t byte = modrm & 7;

      size_t *bufcntp = d->bufcntp;
      size_t avail = d->bufsize - *bufcntp;
      int needed = snprintf (&d->bufp[*bufcntp], avail, "%%xmm%" PRIxFAST8,
			     byte);
      if ((size_t) needed > avail)
	return needed - avail;
      *d->bufcntp += needed;
      return 0;
    }

  return general_mod$r_m (d);
}

static int
generic_abs (struct output_data *d, const char *absstring
#ifdef X86_64
	     , int abslen
#else
# define abslen 4
#endif
	     )
{
  int r = data_prefix (d);
  if (r != 0)
    return r;

  assert (d->opoff1 % 8 == 0);
  assert (d->opoff1 / 8 == 1);
  if (*d->param_start + abslen > d->end)
    return -1;
  *d->param_start += abslen;
#ifndef X86_64
  uint32_t absval;
# define ABSPRIFMT PRIx32
#else
  uint64_t absval;
# define ABSPRIFMT PRIx64
  if (abslen == 8)
    absval = read_8ubyte_unaligned (&d->data[1]);
  else
#endif
    absval = read_4ubyte_unaligned (&d->data[1]);
  size_t *bufcntp = d->bufcntp;
  size_t avail = d->bufsize - *bufcntp;
  int needed = snprintf (&d->bufp[*bufcntp], avail, "%s0x%" ABSPRIFMT,
			 absstring, absval);
  if ((size_t) needed > avail)
    return needed - avail;
  *bufcntp += needed;
  return 0;
}


static int
FCT_absval (struct output_data *d)
{
  return generic_abs (d, "$"
#ifdef X86_64
		      , 4
#endif
		      );
}

static int
FCT_abs (struct output_data *d)
{
  return generic_abs (d, ""
#ifdef X86_64
		      , 8
#endif
		      );
}

static int
FCT_ax (struct output_data *d)
{
  int is_16bit = (*d->prefixes & has_data16) != 0;

  size_t *bufcntp = d->bufcntp;
  char *bufp = d->bufp;
  size_t bufsize = d->bufsize;

  if (*bufcntp + 4 - is_16bit > bufsize)
    return *bufcntp + 4 - is_16bit - bufsize;

  bufp[(*bufcntp)++] = '%';
  if (! is_16bit)
    bufp[(*bufcntp)++] = (
#ifdef X86_64
			  (*d->prefixes & has_rex_w) ? 'r' :
#endif
			  'e');
  bufp[(*bufcntp)++] = 'a';
  bufp[(*bufcntp)++] = 'x';

  return 0;
}


static int
FCT_ax$w (struct output_data *d)
{
  if ((d->data[d->opoff2 / 8] & (1 << (7 - (d->opoff2 & 7)))) != 0)
    return FCT_ax (d);

  size_t *bufcntp = d->bufcntp;
  char *bufp = d->bufp;
  size_t bufsize = d->bufsize;

  if (*bufcntp + 3 > bufsize)
    return *bufcntp + 3 - bufsize;

  bufp[(*bufcntp)++] = '%';
  bufp[(*bufcntp)++] = 'a';
  bufp[(*bufcntp)++] = 'l';

  return 0;
}


static int
__attribute__ ((noinline))
FCT_crdb (struct output_data *d, const char *regstr)
{
  if (*d->prefixes & has_data16)
    return -1;

  size_t *bufcntp = d->bufcntp;

  // XXX If this assert is true, use absolute offset below
  assert (d->opoff1 / 8 == 2);
  assert (d->opoff1 % 8 == 2);
  size_t avail = d->bufsize - *bufcntp;
  int needed = snprintf (&d->bufp[*bufcntp], avail, "%%%s%" PRIx32,
			 regstr, (uint32_t) (d->data[d->opoff1 / 8] >> 3) & 7);
  if ((size_t) needed > avail)
    return needed - avail;
  *bufcntp += needed;
  return 0;
}


static int
FCT_ccc (struct output_data *d)
{
  return FCT_crdb (d, "cr");
}


static int
FCT_ddd (struct output_data *d)
{
  return FCT_crdb (d, "db");
}


static int
FCT_disp8 (struct output_data *d)
{
  assert (d->opoff1 % 8 == 0);
  if (*d->param_start >= d->end)
    return -1;
  int32_t offset = *(const int8_t *) (*d->param_start)++;

  size_t *bufcntp = d->bufcntp;
  size_t avail = d->bufsize - *bufcntp;
  int needed = snprintf (&d->bufp[*bufcntp], avail, "0x%" PRIx32,
			 (uint32_t) (d->addr + (*d->param_start - d->data)
				     + offset));
  if ((size_t) needed > avail)
    return needed - avail;
  *bufcntp += needed;
  return 0;
}


static int
__attribute__ ((noinline))
FCT_ds_xx (struct output_data *d, const char *reg)
{
  int prefix = *d->prefixes & SEGMENT_PREFIXES;

  if (prefix == 0)
    *d->prefixes |= prefix = has_ds;
  /* Make sure only one bit is set.  */
  else if ((prefix - 1) & prefix)
    return -1;

  int r = data_prefix (d);

  assert ((*d->prefixes & prefix) == 0);

  if (r != 0)
    return r;

  size_t *bufcntp = d->bufcntp;
  size_t avail = d->bufsize - *bufcntp;
  int needed = snprintf (&d->bufp[*bufcntp], avail, "(%%%s%s)",
#ifdef X86_64
			 *d->prefixes & idx_addr16 ? "e" : "r",
#else
			 *d->prefixes & idx_addr16 ? "" : "e",
#endif
			 reg);
  if ((size_t) needed > avail)
    return (size_t) needed - avail;
  *bufcntp += needed;

  return 0;
}


static int
FCT_ds_bx (struct output_data *d)
{
  return FCT_ds_xx (d, "bx");
}


static int
FCT_ds_si (struct output_data *d)
{
  return FCT_ds_xx (d, "si");
}


static int
FCT_dx (struct output_data *d)
{
  size_t *bufcntp = d->bufcntp;

  if (*bufcntp + 7 > d->bufsize)
    return *bufcntp + 7 - d->bufsize;

  memcpy (&d->bufp[*bufcntp], "(%dx)", 5);
  *bufcntp += 5;

  return 0;
}


static int
FCT_es_di (struct output_data *d)
{
  size_t *bufcntp = d->bufcntp;
  size_t avail = d->bufsize - *bufcntp;
  int needed = snprintf (&d->bufp[*bufcntp], avail, "%%es:(%%%sdi)",
#ifdef X86_64
			 *d->prefixes & idx_addr16 ? "e" : "r"
#else
			 *d->prefixes & idx_addr16 ? "" : "e"
#endif
			 );
  if ((size_t) needed > avail)
    return (size_t) needed - avail;
  *bufcntp += needed;

  return 0;
}


static int
FCT_imm (struct output_data *d)
{
  size_t *bufcntp = d->bufcntp;
  size_t avail = d->bufsize - *bufcntp;
  int needed;
  if (*d->prefixes & has_data16)
    {
      if (*d->param_start + 2 > d->end)
	return -1;
      uint16_t word = read_2ubyte_unaligned_inc (*d->param_start);
      needed = snprintf (&d->bufp[*bufcntp], avail, "$0x%" PRIx16, word);
    }
  else
    {
      if (*d->param_start + 4 > d->end)
	return -1;
      int32_t word = read_4sbyte_unaligned_inc (*d->param_start);
#ifdef X86_64
      if (*d->prefixes & has_rex_w)
	needed = snprintf (&d->bufp[*bufcntp], avail, "$0x%" PRIx64,
			   (int64_t) word);
      else
#endif
	needed = snprintf (&d->bufp[*bufcntp], avail, "$0x%" PRIx32, word);
    }
  if ((size_t) needed > avail)
    return (size_t) needed - avail;
  *bufcntp += needed;
  return 0;
}


static int
FCT_imm$w (struct output_data *d)
{
  if ((d->data[d->opoff2 / 8] & (1 << (7 - (d->opoff2 & 7)))) != 0)
    return FCT_imm (d);

  size_t *bufcntp = d->bufcntp;
  size_t avail = d->bufsize - *bufcntp;
  if (*d->param_start>= d->end)
    return -1;
  uint_fast8_t word = *(*d->param_start)++;
  int needed = snprintf (&d->bufp[*bufcntp], avail, "$0x%" PRIxFAST8, word);
  if ((size_t) needed > avail)
    return (size_t) needed - avail;
  *bufcntp += needed;
  return 0;
}


#ifdef X86_64
static int
FCT_imm64$w (struct output_data *d)
{
  if ((d->data[d->opoff2 / 8] & (1 << (7 - (d->opoff2 & 7)))) == 0
      || (*d->prefixes & has_data16) != 0)
    return FCT_imm$w (d);

  size_t *bufcntp = d->bufcntp;
  size_t avail = d->bufsize - *bufcntp;
  int needed;
  if (*d->prefixes & has_rex_w)
    {
      if (*d->param_start + 8 > d->end)
	return -1;
      uint64_t word = read_8ubyte_unaligned_inc (*d->param_start);
      needed = snprintf (&d->bufp[*bufcntp], avail, "$0x%" PRIx64, word);
    }
  else
    {
      if (*d->param_start + 4 > d->end)
	return -1;
      int32_t word = read_4sbyte_unaligned_inc (*d->param_start);
      needed = snprintf (&d->bufp[*bufcntp], avail, "$0x%" PRIx32, word);
    }
  if ((size_t) needed > avail)
    return (size_t) needed - avail;
  *bufcntp += needed;
  return 0;
}
#endif


static int
FCT_imms (struct output_data *d)
{
  size_t *bufcntp = d->bufcntp;
  size_t avail = d->bufsize - *bufcntp;
  if (*d->param_start>= d->end)
    return -1;
  int8_t byte = *(*d->param_start)++;
#ifdef X86_64
  int needed = snprintf (&d->bufp[*bufcntp], avail, "$0x%" PRIx64,
			 (int64_t) byte);
#else
  int needed = snprintf (&d->bufp[*bufcntp], avail, "$0x%" PRIx32,
			 (int32_t) byte);
#endif
  if ((size_t) needed > avail)
    return (size_t) needed - avail;
  *bufcntp += needed;
  return 0;
}


static int
FCT_imm$s (struct output_data *d)
{
  uint_fast8_t opcode = d->data[d->opoff2 / 8];
  size_t *bufcntp = d->bufcntp;
  size_t avail = d->bufsize - *bufcntp;
  if ((opcode & 2) != 0)
    return FCT_imms (d);

  if ((*d->prefixes & has_data16) == 0)
    {
      if (*d->param_start + 4 > d->end)
	return -1;
      int32_t word = read_4sbyte_unaligned_inc (*d->param_start);
#ifdef X86_64
      int needed = snprintf (&d->bufp[*bufcntp], avail, "$0x%" PRIx64,
			     (int64_t) word);
#else
      int needed = snprintf (&d->bufp[*bufcntp], avail, "$0x%" PRIx32, word);
#endif
      if ((size_t) needed > avail)
	return (size_t) needed - avail;
      *bufcntp += needed;
    }
  else
    {
      if (*d->param_start + 2 > d->end)
	return -1;
      uint16_t word = read_2ubyte_unaligned_inc (*d->param_start);
      int needed = snprintf (&d->bufp[*bufcntp], avail, "$0x%" PRIx16, word);
      if ((size_t) needed > avail)
	return (size_t) needed - avail;
      *bufcntp += needed;
    }
  return 0;
}


static int
FCT_imm16 (struct output_data *d)
{
  if (*d->param_start + 2 > d->end)
    return -1;
  uint16_t word = read_2ubyte_unaligned_inc (*d->param_start);
  size_t *bufcntp = d->bufcntp;
  size_t avail = d->bufsize - *bufcntp;
  int needed = snprintf (&d->bufp[*bufcntp], avail, "$0x%" PRIx16, word);
  if ((size_t) needed > avail)
    return (size_t) needed - avail;
  *bufcntp += needed;
  return 0;
}


static int
FCT_imms8 (struct output_data *d)
{
  size_t *bufcntp = d->bufcntp;
  size_t avail = d->bufsize - *bufcntp;
  if (*d->param_start >= d->end)
    return -1;
  int_fast8_t byte = *(*d->param_start)++;
  int needed;
#ifdef X86_64
  if (*d->prefixes & has_rex_w)
    needed = snprintf (&d->bufp[*bufcntp], avail, "$0x%" PRIx64,
		       (int64_t) byte);
  else
#endif
    needed = snprintf (&d->bufp[*bufcntp], avail, "$0x%" PRIx32,
		       (int32_t) byte);
  if ((size_t) needed > avail)
    return (size_t) needed - avail;
  *bufcntp += needed;
  return 0;
}


static int
FCT_imm8 (struct output_data *d)
{
  size_t *bufcntp = d->bufcntp;
  size_t avail = d->bufsize - *bufcntp;
  if (*d->param_start >= d->end)
    return -1;
  uint_fast8_t byte = *(*d->param_start)++;
  int needed = snprintf (&d->bufp[*bufcntp], avail, "$0x%" PRIx32,
			 (uint32_t) byte);
  if ((size_t) needed > avail)
    return (size_t) needed - avail;
  *bufcntp += needed;
  return 0;
}


static int
FCT_rel (struct output_data *d)
{
  size_t *bufcntp = d->bufcntp;
  size_t avail = d->bufsize - *bufcntp;
  if (*d->param_start + 4 > d->end)
    return -1;
  int32_t rel = read_4sbyte_unaligned_inc (*d->param_start);
#ifdef X86_64
  int needed = snprintf (&d->bufp[*bufcntp], avail, "0x%" PRIx64,
			 (uint64_t) (d->addr + rel
				     + (*d->param_start - d->data)));
#else
  int needed = snprintf (&d->bufp[*bufcntp], avail, "0x%" PRIx32,
			 (uint32_t) (d->addr + rel
				     + (*d->param_start - d->data)));
#endif
  if ((size_t) needed > avail)
    return (size_t) needed - avail;
  *bufcntp += needed;
  return 0;
}


static int
FCT_mmxreg (struct output_data *d)
{
  uint_fast8_t byte = d->data[d->opoff1 / 8];
  assert (d->opoff1 % 8 == 2 || d->opoff1 % 8 == 5);
  byte = (byte >> (5 - d->opoff1 % 8)) & 7;
  size_t *bufcntp =  d->bufcntp;
  size_t avail = d->bufsize - *bufcntp;
  int needed = snprintf (&d->bufp[*bufcntp], avail, "%%mm%" PRIxFAST8, byte);
  if ((size_t) needed > avail)
    return needed - avail;
  *bufcntp += needed;
  return 0;
}


static int
FCT_mod$r_m (struct output_data *d)
{
  assert (d->opoff1 % 8 == 0);
  uint_fast8_t modrm = d->data[d->opoff1 / 8];
  if ((modrm & 0xc0) == 0xc0)
    {
      int prefixes = *d->prefixes;
      if (prefixes & has_addr16)
	return -1;

      int is_16bit = (prefixes & has_data16) != 0;

      size_t *bufcntp = d->bufcntp;
      char *bufp = d->bufp;
      if (*bufcntp + 5 - is_16bit > d->bufsize)
	return *bufcntp + 5 - is_16bit - d->bufsize;
      bufp[(*bufcntp)++] = '%';

      char *cp;
#ifdef X86_64
      if ((prefixes & has_rex_b) != 0 && !is_16bit)
	{
	  cp = stpcpy (&bufp[*bufcntp], hiregs[modrm & 7]);
	  if ((prefixes & has_rex_w) == 0)
	    *cp++ = 'd';
	}
      else
#endif
	{
	  cp = stpcpy (&bufp[*bufcntp], dregs[modrm & 7] + is_16bit);
#ifdef X86_64
	  if ((prefixes & has_rex_w) != 0)
	    bufp[*bufcntp] = 'r';
#endif
	}
      *bufcntp = cp - bufp;
      return 0;
    }

  return general_mod$r_m (d);
}


#ifndef X86_64
static int
FCT_moda$r_m (struct output_data *d)
{
  assert (d->opoff1 % 8 == 0);
  uint_fast8_t modrm = d->data[d->opoff1 / 8];
  if ((modrm & 0xc0) == 0xc0)
    {
      if (*d->prefixes & has_addr16)
	return -1;

      size_t *bufcntp = d->bufcntp;
      if (*bufcntp + 3 > d->bufsize)
	return *bufcntp + 3 - d->bufsize;

      memcpy (&d->bufp[*bufcntp], "???", 3);
      *bufcntp += 3;

      return 0;
    }

  return general_mod$r_m (d);
}
#endif


#ifdef X86_64
static const char rex_8bit[8][3] =
  {
    [0] = "a", [1] = "c", [2] = "d", [3] = "b",
    [4] = "sp", [5] = "bp", [6] = "si", [7] = "di"
  };
#endif


static int
FCT_mod$r_m$w (struct output_data *d)
{
  assert (d->opoff1 % 8 == 0);
  const uint8_t *data = d->data;
  uint_fast8_t modrm = data[d->opoff1 / 8];
  if ((modrm & 0xc0) == 0xc0)
    {
      int prefixes = *d->prefixes;

      if (prefixes & has_addr16)
	return -1;

      size_t *bufcntp = d->bufcntp;
      char *bufp = d->bufp;
      if (*bufcntp + 5 > d->bufsize)
	return *bufcntp + 5 - d->bufsize;

      if ((data[d->opoff3 / 8] & (1 << (7 - (d->opoff3 & 7)))) == 0)
	{
	  bufp[(*bufcntp)++] = '%';

#ifdef X86_64
	  if (prefixes & has_rex)
	    {
	      if (prefixes & has_rex_r)
		*bufcntp += snprintf (bufp + *bufcntp, d->bufsize - *bufcntp,
				      "r%db", 8 + (modrm & 7));
	      else
		{
		  char *cp = stpcpy (bufp + *bufcntp, hiregs[modrm & 7]);
		  *cp++ = 'l';
		  *bufcntp = cp - bufp;
		}
	    }
	  else
#endif
	    {
	      bufp[(*bufcntp)++] = "acdb"[modrm & 3];
	      bufp[(*bufcntp)++] = "lh"[(modrm & 4) >> 2];
	    }
	}
      else
	{
	  int is_16bit = (prefixes & has_data16) != 0;

	  bufp[(*bufcntp)++] = '%';

	  char *cp;
#ifdef X86_64
	  if ((prefixes & has_rex_b) != 0 && !is_16bit)
	    {
	      cp = stpcpy (&bufp[*bufcntp], hiregs[modrm & 7]);
	      if ((prefixes & has_rex_w) == 0)
		*cp++ = 'd';
	    }
	  else
#endif
	    {
	      cp = stpcpy (&bufp[*bufcntp], dregs[modrm & 7] + is_16bit);
#ifdef X86_64
	      if ((prefixes & has_rex_w) != 0)
		bufp[*bufcntp] = 'r';
#endif
	    }
	  *bufcntp = cp - bufp;
	}
      return 0;
    }

  return general_mod$r_m (d);
}


static int
FCT_mod$8r_m (struct output_data *d)
{
  assert (d->opoff1 % 8 == 0);
  uint_fast8_t modrm = d->data[d->opoff1 / 8];
  if ((modrm & 0xc0) == 0xc0)
    {
      size_t *bufcntp = d->bufcntp;
      char *bufp = d->bufp;
      if (*bufcntp + 3 > d->bufsize)
	return *bufcntp + 3 - d->bufsize;
      bufp[(*bufcntp)++] = '%';
      bufp[(*bufcntp)++] = "acdb"[modrm & 3];
      bufp[(*bufcntp)++] = "lh"[(modrm & 4) >> 2];
      return 0;
    }

  return general_mod$r_m (d);
}


static int
FCT_mod$16r_m (struct output_data *d)
{
  assert (d->opoff1 % 8 == 0);
  uint_fast8_t modrm = d->data[d->opoff1 / 8];
  if ((modrm & 0xc0) == 0xc0)
    {
      assert (d->opoff1 / 8 == d->opoff2 / 8);
      //uint_fast8_t byte = data[opoff2 / 8] & 7;
      uint_fast8_t byte = modrm & 7;

      size_t *bufcntp = d->bufcntp;
      if (*bufcntp + 3 > d->bufsize)
	return *bufcntp + 3 - d->bufsize;
      d->bufp[(*bufcntp)++] = '%';
      memcpy (&d->bufp[*bufcntp], dregs[byte] + 1, sizeof (dregs[0]) - 1);
      *bufcntp += 2;
      return 0;
    }

  return general_mod$r_m (d);
}


#ifdef X86_64
static int
FCT_mod$64r_m (struct output_data *d)
{
  assert (d->opoff1 % 8 == 0);
  uint_fast8_t modrm = d->data[d->opoff1 / 8];
  if ((modrm & 0xc0) == 0xc0)
    {
      assert (d->opoff1 / 8 == d->opoff2 / 8);
      //uint_fast8_t byte = data[opoff2 / 8] & 7;
      uint_fast8_t byte = modrm & 7;

      size_t *bufcntp = d->bufcntp;
      if (*bufcntp + 4 > d->bufsize)
	return *bufcntp + 4 - d->bufsize;
      char *cp = &d->bufp[*bufcntp];
      *cp++ = '%';
      cp = stpcpy (cp,
		   (*d->prefixes & has_rex_b) ? hiregs[byte] : aregs[byte]);
      *bufcntp = cp - d->bufp;
      return 0;
    }

  return general_mod$r_m (d);
}
#else
#define FCT_mod$64r_m FCT_mod$r_m
#endif


static int
FCT_reg (struct output_data *d)
{
  uint_fast8_t byte = d->data[d->opoff1 / 8];
  assert (d->opoff1 % 8 + 3 <= 8);
  byte >>= 8 - (d->opoff1 % 8 + 3);
  byte &= 7;
  int is_16bit = (*d->prefixes & has_data16) != 0;
  size_t *bufcntp = d->bufcntp;
  if (*bufcntp + 5 > d->bufsize)
    return *bufcntp + 5 - d->bufsize;
  d->bufp[(*bufcntp)++] = '%';
#ifdef X86_64
  if ((*d->prefixes & has_rex_r) != 0 && !is_16bit)
    {
      *bufcntp += snprintf (&d->bufp[*bufcntp], d->bufsize - *bufcntp, "r%d",
			    8 + byte);
      if ((*d->prefixes & has_rex_w) == 0)
	d->bufp[(*bufcntp)++] = 'd';
    }
  else
#endif
    {
      memcpy (&d->bufp[*bufcntp], dregs[byte] + is_16bit, 3 - is_16bit);
#ifdef X86_64
      if ((*d->prefixes & has_rex_w) != 0 && !is_16bit)
	d->bufp[*bufcntp] = 'r';
#endif
      *bufcntp += 3 - is_16bit;
    }
  return 0;
}


#ifdef X86_64
static int
FCT_oreg (struct output_data *d)
{
  /* Special form where register comes from opcode.  The rex.B bit is used,
     rex.R and rex.X are ignored.  */
  int save_prefixes = *d->prefixes;

  *d->prefixes = ((save_prefixes & ~has_rex_r)
		  | ((save_prefixes & has_rex_b) << (idx_rex_r - idx_rex_b)));

  int r = FCT_reg (d);

  *d->prefixes = save_prefixes;

  return r;
}
#endif


static int
FCT_reg64 (struct output_data *d)
{
  uint_fast8_t byte = d->data[d->opoff1 / 8];
  assert (d->opoff1 % 8 + 3 <= 8);
  byte >>= 8 - (d->opoff1 % 8 + 3);
  byte &= 7;
  if ((*d->prefixes & has_data16) != 0)
    return -1;
  size_t *bufcntp = d->bufcntp;
  if (*bufcntp + 5 > d->bufsize)
    return *bufcntp + 5 - d->bufsize;
  d->bufp[(*bufcntp)++] = '%';
#ifdef X86_64
  if ((*d->prefixes & has_rex_r) != 0)
    {
      *bufcntp += snprintf (&d->bufp[*bufcntp], d->bufsize - *bufcntp, "r%d",
			    8 + byte);
      if ((*d->prefixes & has_rex_w) == 0)
	d->bufp[(*bufcntp)++] = 'd';
    }
  else
#endif
    {
      memcpy (&d->bufp[*bufcntp], aregs[byte], 3);
      *bufcntp += 3;
    }
  return 0;
}


static int
FCT_reg$w (struct output_data *d)
{
  if (d->data[d->opoff2 / 8] & (1 << (7 - (d->opoff2 & 7))))
    return FCT_reg (d);

  uint_fast8_t byte = d->data[d->opoff1 / 8];
  assert (d->opoff1 % 8 + 3 <= 8);
  byte >>= 8 - (d->opoff1 % 8 + 3);
  byte &= 7;

  size_t *bufcntp = d->bufcntp;
  if (*bufcntp + 4 > d->bufsize)
    return *bufcntp + 4 - d->bufsize;

  d->bufp[(*bufcntp)++] = '%';

#ifdef X86_64
  if (*d->prefixes & has_rex)
    {
      if (*d->prefixes & has_rex_r)
	*bufcntp += snprintf (d->bufp + *bufcntp, d->bufsize - *bufcntp,
			      "r%db", 8 + byte);
      else
	{
	  char* cp = stpcpy (d->bufp + *bufcntp, rex_8bit[byte]);
	  *cp++ = 'l';
	  *bufcntp = cp - d->bufp;
	}
    }
  else
#endif
    {
      d->bufp[(*bufcntp)++] = "acdb"[byte & 3];
      d->bufp[(*bufcntp)++] = "lh"[byte >> 2];
    }
  return 0;
}


#ifdef X86_64
static int
FCT_oreg$w (struct output_data *d)
{
  /* Special form where register comes from opcode.  The rex.B bit is used,
     rex.R and rex.X are ignored.  */
  int save_prefixes = *d->prefixes;

  *d->prefixes = ((save_prefixes & ~has_rex_r)
		  | ((save_prefixes & has_rex_b) << (idx_rex_r - idx_rex_b)));

  int r = FCT_reg$w (d);

  *d->prefixes = save_prefixes;

  return r;
}
#endif


static int
FCT_freg (struct output_data *d)
{
  assert (d->opoff1 / 8 == 1);
  assert (d->opoff1 % 8 == 5);
  size_t *bufcntp = d->bufcntp;
  size_t avail = d->bufsize - *bufcntp;
  int needed = snprintf (&d->bufp[*bufcntp], avail, "%%st(%" PRIx32 ")",
			 (uint32_t) (d->data[1] & 7));
  if ((size_t) needed > avail)
    return (size_t) needed - avail;
  *bufcntp += needed;
  return 0;
}


#ifndef X86_64
static int
FCT_reg16 (struct output_data *d)
{
  if (*d->prefixes & has_data16)
    return -1;

  *d->prefixes |= has_data16;
  return FCT_reg (d);
}
#endif


static int
FCT_sel (struct output_data *d)
{
  assert (d->opoff1 % 8 == 0);
  assert (d->opoff1 / 8 == 5);
  if (*d->param_start + 2 >= d->end)
    return -1;
  *d->param_start += 2;
  uint16_t absval = read_2ubyte_unaligned (&d->data[5]);

  size_t *bufcntp = d->bufcntp;
  size_t avail = d->bufsize - *bufcntp;
  int needed = snprintf (&d->bufp[*bufcntp], avail, "$0x%" PRIx16, absval);
  if ((size_t) needed > avail)
    return needed - avail;
  *bufcntp += needed;
  return 0;
}


static int
FCT_sreg2 (struct output_data *d)
{
  uint_fast8_t byte = d->data[d->opoff1 / 8];
  assert (d->opoff1 % 8 + 3 <= 8);
  byte >>= 8 - (d->opoff1 % 8 + 2);

  size_t *bufcntp = d->bufcntp;
  char *bufp = d->bufp;
  if (*bufcntp + 3 > d->bufsize)
    return *bufcntp + 3 - d->bufsize;

  bufp[(*bufcntp)++] = '%';
  bufp[(*bufcntp)++] = "ecsd"[byte & 3];
  bufp[(*bufcntp)++] = 's';

  return 0;
}


static int
FCT_sreg3 (struct output_data *d)
{
  uint_fast8_t byte = d->data[d->opoff1 / 8];
  assert (d->opoff1 % 8 + 4 <= 8);
  byte >>= 8 - (d->opoff1 % 8 + 3);

  if ((byte & 7) >= 6)
    return -1;

  size_t *bufcntp = d->bufcntp;
  char *bufp = d->bufp;
  if (*bufcntp + 3 > d->bufsize)
    return *bufcntp + 3 - d->bufsize;

  bufp[(*bufcntp)++] = '%';
  bufp[(*bufcntp)++] = "ecsdfg"[byte & 7];
  bufp[(*bufcntp)++] = 's';

  return 0;
}


static int
FCT_string (struct output_data *d __attribute__ ((unused)))
{
  return 0;
}


static int
FCT_xmmreg (struct output_data *d)
{
  uint_fast8_t byte = d->data[d->opoff1 / 8];
  assert (d->opoff1 % 8 == 2 || d->opoff1 % 8 == 5);
  byte = (byte >> (5 - d->opoff1 % 8)) & 7;

  size_t *bufcntp = d->bufcntp;
  size_t avail = d->bufsize - *bufcntp;
  int needed = snprintf (&d->bufp[*bufcntp], avail, "%%xmm%" PRIxFAST8, byte);
  if ((size_t) needed > avail)
    return needed - avail;
  *bufcntp += needed;
  return 0;
}
