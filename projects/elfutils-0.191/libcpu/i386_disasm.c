/* Disassembler for x86.
   Copyright (C) 2007, 2008, 2009, 2011 Red Hat, Inc.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <config.h>
#include <ctype.h>
#include <errno.h>
#include <gelf.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "libeblP.h"

#define MACHINE_ENCODING LITTLE_ENDIAN
#include "memory-access.h"

#include "i386_mne.h"

#define MNESTRFIELD(line) MNESTRFIELD1 (line)
#define MNESTRFIELD1(line) str##line
static const union mnestr_t
{
  struct
  {
#define MNE(name) char MNESTRFIELD (__LINE__)[sizeof (#name)];
#include MNEFILE
#undef MNE
  };
  char str[0];
} mnestr =
  {
    {
#define MNE(name) #name,
#include MNEFILE
#undef MNE
    }
  };

static const unsigned short int mneidx[] =
  {
#define MNE(name) \
  [MNE_##name] = offsetof (union mnestr_t, MNESTRFIELD (__LINE__)),
#include MNEFILE
#undef MNE
  };


enum
  {
    idx_rex_b = 0,
    idx_rex_x,
    idx_rex_r,
    idx_rex_w,
    idx_rex,
    idx_cs,
    idx_ds,
    idx_es,
    idx_fs,
    idx_gs,
    idx_ss,
    idx_data16,
    idx_addr16,
    idx_rep,
    idx_repne,
    idx_lock
  };

enum
  {
#define prefbit(pref) has_##pref = 1 << idx_##pref
    prefbit (rex_b),
    prefbit (rex_x),
    prefbit (rex_r),
    prefbit (rex_w),
    prefbit (rex),
    prefbit (cs),
    prefbit (ds),
    prefbit (es),
    prefbit (fs),
    prefbit (gs),
    prefbit (ss),
    prefbit (data16),
    prefbit (addr16),
    prefbit (rep),
    prefbit (repne),
    prefbit (lock)
#undef prefbit
  };
#define SEGMENT_PREFIXES \
  (has_cs | has_ds | has_es | has_fs | has_gs | has_ss)

#define prefix_cs	0x2e
#define prefix_ds	0x3e
#define prefix_es	0x26
#define prefix_fs	0x64
#define prefix_gs	0x65
#define prefix_ss	0x36
#define prefix_data16	0x66
#define prefix_addr16	0x67
#define prefix_rep	0xf3
#define prefix_repne	0xf2
#define prefix_lock	0xf0


static const uint8_t known_prefixes[] =
  {
#define newpref(pref) [idx_##pref] = prefix_##pref
    newpref (cs),
    newpref (ds),
    newpref (es),
    newpref (fs),
    newpref (gs),
    newpref (ss),
    newpref (data16),
    newpref (addr16),
    newpref (rep),
    newpref (repne),
    newpref (lock)
#undef newpref
  };
#define nknown_prefixes (sizeof (known_prefixes) / sizeof (known_prefixes[0]))


#if 0
static const char *prefix_str[] =
  {
#define newpref(pref) [idx_##pref] = #pref
    newpref (cs),
    newpref (ds),
    newpref (es),
    newpref (fs),
    newpref (gs),
    newpref (ss),
    newpref (data16),
    newpref (addr16),
    newpref (rep),
    newpref (repne),
    newpref (lock)
#undef newpref
  };
#endif


static const char amd3dnowstr[] =
#define MNE_3DNOW_PAVGUSB 1
  "pavgusb\0"
#define MNE_3DNOW_PFADD (MNE_3DNOW_PAVGUSB + 8)
  "pfadd\0"
#define MNE_3DNOW_PFSUB (MNE_3DNOW_PFADD + 6)
  "pfsub\0"
#define MNE_3DNOW_PFSUBR (MNE_3DNOW_PFSUB + 6)
  "pfsubr\0"
#define MNE_3DNOW_PFACC (MNE_3DNOW_PFSUBR + 7)
  "pfacc\0"
#define MNE_3DNOW_PFCMPGE (MNE_3DNOW_PFACC + 6)
  "pfcmpge\0"
#define MNE_3DNOW_PFCMPGT (MNE_3DNOW_PFCMPGE + 8)
  "pfcmpgt\0"
#define MNE_3DNOW_PFCMPEQ (MNE_3DNOW_PFCMPGT + 8)
  "pfcmpeq\0"
#define MNE_3DNOW_PFMIN (MNE_3DNOW_PFCMPEQ + 8)
  "pfmin\0"
#define MNE_3DNOW_PFMAX (MNE_3DNOW_PFMIN + 6)
  "pfmax\0"
#define MNE_3DNOW_PI2FD (MNE_3DNOW_PFMAX + 6)
  "pi2fd\0"
#define MNE_3DNOW_PF2ID (MNE_3DNOW_PI2FD + 6)
  "pf2id\0"
#define MNE_3DNOW_PFRCP (MNE_3DNOW_PF2ID + 6)
  "pfrcp\0"
#define MNE_3DNOW_PFRSQRT (MNE_3DNOW_PFRCP + 6)
  "pfrsqrt\0"
#define MNE_3DNOW_PFMUL (MNE_3DNOW_PFRSQRT + 8)
  "pfmul\0"
#define MNE_3DNOW_PFRCPIT1 (MNE_3DNOW_PFMUL + 6)
  "pfrcpit1\0"
#define MNE_3DNOW_PFRSQIT1 (MNE_3DNOW_PFRCPIT1 + 9)
  "pfrsqit1\0"
#define MNE_3DNOW_PFRCPIT2 (MNE_3DNOW_PFRSQIT1 + 9)
  "pfrcpit2\0"
#define MNE_3DNOW_PMULHRW (MNE_3DNOW_PFRCPIT2 + 9)
  "pmulhrw";

#define AMD3DNOW_LOW_IDX 0x0d
#define AMD3DNOW_HIGH_IDX (sizeof (amd3dnow) + AMD3DNOW_LOW_IDX - 1)
#define AMD3DNOW_IDX(val) ((val) - AMD3DNOW_LOW_IDX)
static const unsigned char amd3dnow[] =
  {
    [AMD3DNOW_IDX (0xbf)] = MNE_3DNOW_PAVGUSB,
    [AMD3DNOW_IDX (0x9e)] = MNE_3DNOW_PFADD,
    [AMD3DNOW_IDX (0x9a)] = MNE_3DNOW_PFSUB,
    [AMD3DNOW_IDX (0xaa)] = MNE_3DNOW_PFSUBR,
    [AMD3DNOW_IDX (0xae)] = MNE_3DNOW_PFACC,
    [AMD3DNOW_IDX (0x90)] = MNE_3DNOW_PFCMPGE,
    [AMD3DNOW_IDX (0xa0)] = MNE_3DNOW_PFCMPGT,
    [AMD3DNOW_IDX (0xb0)] = MNE_3DNOW_PFCMPEQ,
    [AMD3DNOW_IDX (0x94)] = MNE_3DNOW_PFMIN,
    [AMD3DNOW_IDX (0xa4)] = MNE_3DNOW_PFMAX,
    [AMD3DNOW_IDX (0x0d)] = MNE_3DNOW_PI2FD,
    [AMD3DNOW_IDX (0x1d)] = MNE_3DNOW_PF2ID,
    [AMD3DNOW_IDX (0x96)] = MNE_3DNOW_PFRCP,
    [AMD3DNOW_IDX (0x97)] = MNE_3DNOW_PFRSQRT,
    [AMD3DNOW_IDX (0xb4)] = MNE_3DNOW_PFMUL,
    [AMD3DNOW_IDX (0xa6)] = MNE_3DNOW_PFRCPIT1,
    [AMD3DNOW_IDX (0xa7)] = MNE_3DNOW_PFRSQIT1,
    [AMD3DNOW_IDX (0xb6)] = MNE_3DNOW_PFRCPIT2,
    [AMD3DNOW_IDX (0xb7)] = MNE_3DNOW_PMULHRW
  };


struct output_data
{
  GElf_Addr addr;
  int *prefixes;
  size_t opoff1;
  size_t opoff2;
  size_t opoff3;
  char *bufp;
  size_t *bufcntp;
  size_t bufsize;
  const uint8_t *data;
  const uint8_t **param_start;
  const uint8_t *end;
  char *labelbuf;
  size_t labelbufsize;
  enum
    {
      addr_none = 0,
      addr_abs_symbolic,
      addr_abs_always,
      addr_rel_symbolic,
      addr_rel_always
    } symaddr_use;
  GElf_Addr symaddr;
};


#ifndef DISFILE
# define DISFILE "i386_dis.h"
#endif
#include DISFILE


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


int
i386_disasm (Ebl *ebl __attribute__((unused)),
	     const uint8_t **startp, const uint8_t *end, GElf_Addr addr,
	     const char *fmt, DisasmOutputCB_t outcb, DisasmGetSymCB_t symcb,
	     void *outcbarg, void *symcbarg)
{
  const char *save_fmt = fmt;

#define BUFSIZE 512
  char initbuf[BUFSIZE];
  int prefixes;
  size_t bufcnt;
  size_t bufsize = BUFSIZE;
  char *buf = initbuf;
  const uint8_t *param_start;

  struct output_data output_data =
    {
      .prefixes = &prefixes,
      .bufp = buf,
      .bufsize = bufsize,
      .bufcntp = &bufcnt,
      .param_start = &param_start,
      .end = end
    };

  int retval = 0;
  while (1)
    {
      prefixes = 0;

      const uint8_t *data = *startp;
      const uint8_t *begin = data;

      /* Recognize all prefixes.  */
      int last_prefix_bit = 0;
      while (data < end)
	{
	  unsigned int i;
	  for (i = idx_cs; i < nknown_prefixes; ++i)
	    if (known_prefixes[i] == *data)
	      break;
	  if (i == nknown_prefixes)
	    break;

	  prefixes |= last_prefix_bit = 1 << i;

	  ++data;
	}

#ifdef X86_64
      if (data < end && (*data & 0xf0) == 0x40)
	prefixes |= ((*data++) & 0xf) | has_rex;
#endif

      bufcnt = 0;
      size_t cnt = 0;

      const uint8_t *curr = match_data;
      const uint8_t *const match_end = match_data + sizeof (match_data);

      assert (data <= end);
      if (data == end)
	{
	  if (prefixes != 0)
	    goto print_prefix;

	  retval = -1;
	  goto do_ret;
	}

    next_match:
      while (curr < match_end)
	{
	  uint_fast8_t len = *curr++;
	  uint_fast8_t clen = len >> 4;
	  len &= 0xf;
	  const uint8_t *next_curr = curr + clen + (len - clen) * 2;

	  assert (len > 0);
	  assert (curr + clen + 2 * (len - clen) <= match_end);

	  const uint8_t *codep = data;
	  int correct_prefix = 0;
	  int opoff = 0;

	  if (data > begin && codep[-1] == *curr && clen > 0)
	    {
	      /* We match a prefix byte.  This is exactly one byte and
		 is matched exactly, without a mask.  */
	      --len;
	      --clen;
	      opoff = 8;

	      ++curr;

	      if (last_prefix_bit == 0)
		goto invalid_op;
	      correct_prefix = last_prefix_bit;
	    }

	  size_t avail = len;
	  while (clen > 0)
	    {
	      if (*codep++ != *curr++)
		goto not;
	      --avail;
	      --clen;
	      if (codep == end && avail > 0)
		goto do_ret;
	    }

	  while (avail > 0)
	    {
	      uint_fast8_t masked = *codep++ & *curr++;
	      if (masked != *curr++)
		{
		not:
		  curr = next_curr;
		  ++cnt;
		  bufcnt = 0;
		  goto next_match;
		}

	      --avail;
	      if (codep == end && avail > 0)
		goto do_ret;
	    }

	  if (len > end - data)
	    /* There is not enough data for the entire instruction.  The
	       caller can figure this out by looking at the pointer into
	       the input data.  */
	    goto do_ret;

	  if (correct_prefix != 0 && (prefixes & correct_prefix) == 0)
	    goto invalid_op;
	  prefixes ^= correct_prefix;

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

	      output_data.bufp = buf;
	      output_data.bufsize = bufsize;
	      bufcnt = 0;

	      if (data == end)
		{
		  if (prefixes == 0)
		    goto invalid_op;
		  goto print_prefix;
		}

	      /* gcc is not clever enough to see the following variables
		 are not used uninitialized.  */
	      __asm (""
		     : "=mr" (opoff), "=mr" (correct_prefix), "=mr" (codep),
		     "=mr" (next_curr), "=mr" (len));
	    }

	  size_t prefix_size = 0;

	  // XXXonly print as prefix if valid?
	  if ((prefixes & has_lock) != 0)
	    {
	      ADD_STRING ("lock ");
	      prefix_size += 5;
	    }

	  if (instrtab[cnt].rep)
	    {
	      if ((prefixes & has_rep) !=  0)
		{
		  ADD_STRING ("rep ");
		  prefix_size += 4;
		}
	    }
	  else if (instrtab[cnt].repe
		   && (prefixes & (has_rep | has_repne)) != 0)
	    {
	      if ((prefixes & has_repne) != 0)
		{
		  ADD_STRING ("repne ");
		  prefix_size += 6;
		}
	      else if ((prefixes & has_rep) != 0)
		{
		  ADD_STRING ("repe ");
		  prefix_size += 5;
		}
	    }
	  else if ((prefixes & (has_rep | has_repne)) != 0)
	    {
	      uint_fast8_t byte;
	    print_prefix:
	      bufcnt = 0;
	      byte = *begin;
	      /* This is a prefix byte.  Print it.  */
	      switch (byte)
		{
		case prefix_rep:
		  ADD_STRING ("rep");
		  break;
		case prefix_repne:
		  ADD_STRING ("repne");
		  break;
		case prefix_cs:
		  ADD_STRING ("cs");
		  break;
		case prefix_ds:
		  ADD_STRING ("ds");
		  break;
		case prefix_es:
		  ADD_STRING ("es");
		  break;
		case prefix_fs:
		  ADD_STRING ("fs");
		  break;
		case prefix_gs:
		  ADD_STRING ("gs");
		  break;
		case prefix_ss:
		  ADD_STRING ("ss");
		  break;
		case prefix_data16:
		  ADD_STRING ("data16");
		  break;
		case prefix_addr16:
		  ADD_STRING ("addr16");
		  break;
		case prefix_lock:
		  ADD_STRING ("lock");
		  break;
#ifdef X86_64
		case 0x40 ... 0x4f:
		  ADD_STRING ("rex");
		  if (byte != 0x40)
		    {
		      ADD_CHAR ('.');
		      if (byte & 0x8)
			ADD_CHAR ('w');
		      if (byte & 0x4)
			ADD_CHAR ('r');
		      if (byte & 0x3)
			ADD_CHAR ('x');
		      if (byte & 0x1)
			ADD_CHAR ('b');
		    }
		  break;
#endif
		default:
		  /* Cannot happen.  */
		  puts ("unknown prefix");
		  abort ();
		}
	      data = begin + 1;
	      ++addr;

	      goto out;
	    }

	  /* We have a match.  First determine how many bytes are
	     needed for the addressing mode.  */
	  param_start = codep;
	  if (instrtab[cnt].modrm)
	    {
	      uint_fast8_t modrm = codep[-1];

#ifndef X86_64
	      if (likely ((prefixes & has_addr16) != 0))
		{
		  /* Account for displacement.  */
		  if ((modrm & 0xc7) == 6 || (modrm & 0xc0) == 0x80)
		    param_start += 2;
		  else if ((modrm & 0xc0) == 0x40)
		    param_start += 1;
		}
	      else
#endif
		{
		  /* Account for SIB.  */
		  if ((modrm & 0xc0) != 0xc0 && (modrm & 0x7) == 0x4)
		    param_start += 1;

		  /* Account for displacement.  */
		  if ((modrm & 0xc7) == 5 || (modrm & 0xc0) == 0x80
		      || ((modrm & 0xc7) == 0x4
			  && param_start < end
			  && (codep[0] & 0x7) == 0x5))
		    param_start += 4;
		  else if ((modrm & 0xc0) == 0x40)
		    param_start += 1;
		}

	      if (unlikely (param_start > end))
		goto not;
	    }

	  output_data.addr = addr + (data - begin);
	  output_data.data = data;

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
		  char mnebuf[16];
		  const char *str;

		case 'm':
		  /* Mnemonic.  */

		  if (unlikely (instrtab[cnt].mnemonic == MNE_INVALID))
		    {
		      switch (*data)
			{
#ifdef X86_64
			case 0x90:
			  if (prefixes & has_rex_b)
			    goto not;
			  str = "nop";
			  break;
#endif

			case 0x98:
#ifdef X86_64
			  if (prefixes == (has_rex_w | has_rex))
			    {
			      str = "cltq";
			      break;
			    }
#endif
			  if (prefixes & ~has_data16)
			    goto print_prefix;
			  str = prefixes & has_data16 ? "cbtw" : "cwtl";
			  break;

			case 0x99:
#ifdef X86_64
			  if (prefixes == (has_rex_w | has_rex))
			    {
			      str = "cqto";
			      break;
			    }
#endif
			  if (prefixes & ~has_data16)
			    goto print_prefix;
			  str = prefixes & has_data16 ? "cwtd" : "cltd";
			  break;

			case 0xe3:
			  if (prefixes & ~has_addr16)
			    goto print_prefix;
#ifdef X86_64
			  str = prefixes & has_addr16 ? "jecxz" : "jrcxz";
#else
			  str = prefixes & has_addr16 ? "jcxz" : "jecxz";
#endif
			  break;

			case 0x0f:
			  if (data[1] == 0x0f)
			    {
			      /* AMD 3DNOW.  We need one more byte.  */
			      if (param_start >= end)
				goto not;
			      if (*param_start < AMD3DNOW_LOW_IDX
				  || *param_start > AMD3DNOW_HIGH_IDX)
				goto not;
			      unsigned int idx
				= amd3dnow[AMD3DNOW_IDX (*param_start)];
			      if (idx == 0)
				goto not;
			      str = amd3dnowstr + idx - 1;
			      /* Eat the immediate byte indicating the
				 operation.  */
			      ++param_start;
			      break;
			    }
#ifdef X86_64
			  if (data[1] == 0xc7)
			    {
			      str = ((prefixes & has_rex_w)
				     ? "cmpxchg16b" : "cmpxchg8b");
			      break;
			    }
#endif
			  if (data[1] == 0xc2)
			    {
			      if (param_start >= end)
				goto not;
			      if (*param_start > 7)
				goto not;
			      static const char cmpops[][9] =
				{
				  [0] = "cmpeq",
				  [1] = "cmplt",
				  [2] = "cmple",
				  [3] = "cmpunord",
				  [4] = "cmpneq",
				  [5] = "cmpnlt",
				  [6] = "cmpnle",
				  [7] = "cmpord"
				};
			      char *cp = stpcpy (mnebuf, cmpops[*param_start]);
			      if (correct_prefix & (has_rep | has_repne))
				*cp++ = 's';
			      else
				*cp++ = 'p';
			      if (correct_prefix & (has_data16 | has_repne))
				*cp++ = 'd';
			      else
				*cp++ = 's';
			      *cp = '\0';
			      str = mnebuf;
			      /* Eat the immediate byte indicating the
				 operation.  */
			      ++param_start;
			      break;
			    }
			  FALLTHROUGH;
			default:
			  str = "INVALID not handled";
			  break;
			}
		    }
		  else
		    str = mnestr.str + mneidx[instrtab[cnt].mnemonic];

		  if (deferred_start != NULL)
		    {
		      ADD_NSTRING (deferred_start, deferred_len);
		      non_printing += deferred_len;
		    }

		  ADD_STRING (str);

		  switch (instrtab[cnt].suffix)
		    {
		    case suffix_none:
		      break;

		    case suffix_w:
		      if ((codep[-1] & 0xc0) != 0xc0)
			{
			  char ch;

			  if (data[0] & 1)
			    {
			      if (prefixes & has_data16)
				ch = 'w';
#ifdef X86_64
			      else if (prefixes & has_rex_w)
				ch = 'q';
#endif
			      else
				ch = 'l';
			    }
			  else
			    ch = 'b';

			  ADD_CHAR (ch);
			}
		      break;

		    case suffix_w0:
		      if ((codep[-1] & 0xc0) != 0xc0)
			ADD_CHAR ('l');
		      break;

		    case suffix_w1:
		      if ((data[0] & 0x4) == 0)
			ADD_CHAR ('l');
		      break;

		    case suffix_W:
		      if (prefixes & has_data16)
			{
			  ADD_CHAR ('w');
			  prefixes &= ~has_data16;
			}
#ifdef X86_64
		      else
			ADD_CHAR ('q');
#endif
		      break;

		    case suffix_W1:
		      if (prefixes & has_data16)
			{
			  ADD_CHAR ('w');
			  prefixes &= ~has_data16;
			}
#ifdef X86_64
		      else if (prefixes & has_rex_w)
			ADD_CHAR ('q');
#endif
		      break;

		    case suffix_tttn:;
		      static const char tttn[16][3] =
			{
			  "o", "no", "b", "ae", "e", "ne", "be", "a",
			  "s", "ns", "p", "np", "l", "ge", "le", "g"
			};
		      ADD_STRING (tttn[codep[-1 - instrtab[cnt].modrm] & 0x0f]);
		      break;

		    case suffix_D:
		      if ((codep[-1] & 0xc0) != 0xc0)
			ADD_CHAR ((data[0] & 0x04) == 0 ? 's' : 'l');
		      break;

		    default:
		      printf("unknown suffix %d\n", instrtab[cnt].suffix);
		      abort ();
		    }

		  if (deferred_start != NULL)
		    {
		      ADD_STRING (color_off);
		      non_printing += strlen (color_off);
		    }

		  string_end_idx = bufcnt;
		  break;

		case 'o':
		  if (prec == 1 && instrtab[cnt].fct1 != 0)
		    {
		      /* First parameter.  */
		      if (deferred_start != NULL)
			{
			  ADD_NSTRING (deferred_start, deferred_len);
			  non_printing += deferred_len;
			}

		      if (instrtab[cnt].str1 != 0)
			ADD_STRING (op1_str
				    + op1_str_idx[instrtab[cnt].str1 - 1]);

		      output_data.opoff1 = (instrtab[cnt].off1_1
					    + OFF1_1_BIAS - opoff);
		      output_data.opoff2 = (instrtab[cnt].off1_2
					    + OFF1_2_BIAS - opoff);
		      output_data.opoff3 = (instrtab[cnt].off1_3
					    + OFF1_3_BIAS - opoff);
		      int r = op1_fct[instrtab[cnt].fct1] (&output_data);
		      if (r < 0)
			goto not;
		      if (r > 0)
			goto enomem;

		      if (deferred_start != NULL)
			{
			  ADD_STRING (color_off);
			  non_printing += strlen (color_off);
			}

		      string_end_idx = bufcnt;
		    }
		  else if (prec == 2 && instrtab[cnt].fct2 != 0)
		    {
		      /* Second parameter.  */
		      if (deferred_start != NULL)
			{
			  ADD_NSTRING (deferred_start, deferred_len);
			  non_printing += deferred_len;
			}

		      if (instrtab[cnt].str2 != 0)
			ADD_STRING (op2_str
				    + op2_str_idx[instrtab[cnt].str2 - 1]);

		      output_data.opoff1 = (instrtab[cnt].off2_1
					    + OFF2_1_BIAS - opoff);
		      output_data.opoff2 = (instrtab[cnt].off2_2
					    + OFF2_2_BIAS - opoff);
		      output_data.opoff3 = (instrtab[cnt].off2_3
					    + OFF2_3_BIAS - opoff);
		      int r = op2_fct[instrtab[cnt].fct2] (&output_data);
		      if (r < 0)
			goto not;
		      if (r > 0)
			goto enomem;

		      if (deferred_start != NULL)
			{
			  ADD_STRING (color_off);
			  non_printing += strlen (color_off);
			}

		      string_end_idx = bufcnt;
		    }
		  else if (prec == 3 && instrtab[cnt].fct3 != 0)
		    {
		      /* Third parameter.  */
		      if (deferred_start != NULL)
			{
			  ADD_NSTRING (deferred_start, deferred_len);
			  non_printing += deferred_len;
			}

		      if (instrtab[cnt].str3 != 0)
			ADD_STRING (op3_str
				    + op3_str_idx[instrtab[cnt].str3 - 1]);

		      output_data.opoff1 = (instrtab[cnt].off3_1
					    + OFF3_1_BIAS - opoff);
		      output_data.opoff2 = (instrtab[cnt].off3_2
					    + OFF3_2_BIAS - opoff);
#ifdef OFF3_3_BITS
		      output_data.opoff3 = (instrtab[cnt].off3_3
					    + OFF3_3_BIAS - opoff);
#else
		      output_data.opoff3 = 0;
#endif
		      int r = op3_fct[instrtab[cnt].fct3] (&output_data);
		      if (r < 0)
			goto not;
		      if (r > 0)
			goto enomem;

		      if (deferred_start != NULL)
			{
			  ADD_STRING (color_off);
			  non_printing += strlen (color_off);
			}

		      string_end_idx = bufcnt;
		    }
		  else
		    start_idx = bufcnt = string_end_idx;
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
		  if (deferred_start != NULL)
		    {
		      ADD_NSTRING (deferred_start, deferred_len);
		      non_printing += deferred_len;
		    }

		  if (output_data.labelbuf != NULL
		      && output_data.labelbuf[0] != '\0')
		    {
		      ADD_STRING (output_data.labelbuf);
		      output_data.labelbuf[0] = '\0';
		      string_end_idx = bufcnt;
		    }
		  else if (output_data.symaddr_use != addr_none)
		    {
		      GElf_Addr symaddr = output_data.symaddr;
		      if (output_data.symaddr_use >= addr_rel_symbolic)
			symaddr += addr + param_start - begin;

		      // XXX Lookup symbol based on symaddr
		      const char *symstr = NULL;
		      if (symcb != NULL
			  && symcb (0 /* XXX */, 0 /* XXX */, symaddr,
				    &output_data.labelbuf,
				    &output_data.labelbufsize, symcbarg) == 0)
			symstr = output_data.labelbuf;

		      size_t bufavail = bufsize - bufcnt;
		      int r = 0;
		      if (symstr != NULL)
			r = snprintf (&buf[bufcnt], bufavail, "# <%s>",
				      symstr);
		      else if (output_data.symaddr_use == addr_abs_always
			       || output_data.symaddr_use == addr_rel_always)
			r = snprintf (&buf[bufcnt], bufavail, "# %#" PRIx64,
				      (uint64_t) symaddr);

		      assert (r >= 0);
		      if ((size_t) r >= bufavail)
			goto enomem;
		      bufcnt += r;
		      string_end_idx = bufcnt;

		      output_data.symaddr_use = addr_none;
		    }
		  if (deferred_start != NULL)
		    {
		      ADD_STRING (color_off);
		      non_printing += strlen (color_off);
		    }
		  break;

		default:
		  abort ();
		}

	      deferred_start = NULL;

	      /* Pad according to the specified width.  */
	      while (bufcnt + prefix_size - non_printing < start_idx + width)
		ADD_CHAR (' ');
	      prefix_size = 0;
	    }

	  if ((prefixes & SEGMENT_PREFIXES) != 0)
	    goto print_prefix;

	  assert (string_end_idx != ~0ul);
	  bufcnt = string_end_idx;

	  addr += param_start - begin;
	  data = param_start;

	  goto out;
	}

      /* Invalid (or at least unhandled) opcode.  */
    invalid_op:
      if (prefixes != 0)
	goto print_prefix;
      /* Make sure we get past the unrecognized opcode if we haven't yet.  */
      if (*startp == data)
	++data;
      ADD_STRING ("(bad)");
      addr += data - begin;

    out:
      if (bufcnt == bufsize)
	goto enomem;
      buf[bufcnt] = '\0';

      *startp = data;
      retval = outcb (buf, bufcnt, outcbarg);
      if (retval != 0)
	goto do_ret;
    }

 do_ret:
  free (output_data.labelbuf);
  if (buf != initbuf)
    free (buf);

  return retval;
}
