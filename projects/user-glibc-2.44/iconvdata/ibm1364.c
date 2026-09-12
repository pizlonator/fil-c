/* Conversion from and to IBM1364.
   Copyright (C) 2005-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <dlfcn.h>
#include <stdint.h>
#include <wchar.h>
#include <byteswap.h>

#ifndef CHARSET_NAME
/* This is really the IBM1364 converter, not another module sharing
   the code.  */
# define DATA_HEADER	"ibm1364.h"
# define CHARSET_NAME	"IBM1364//"
# define FROM_LOOP	from_ibm1364
# define TO_LOOP	to_ibm1364
# define SB_TO_UCS4	__ibm1364sb_to_ucs4
# define DB_TO_UCS4_IDX	__ibm1364db_to_ucs4_idx
# define DB_TO_UCS4	__ibm1364db_to_ucs4
# define UCS4_TO_SB_IDX	__ucs4_to_ibm1364sb_idx
# define UCS4_TO_SB	__ucs4_to_ibm1364sb
# define UCS4_TO_DB_IDX	__ucs4_to_ibm1364db_idx
# define UCS4_TO_DB	__ucs4_to_ibm1364db
# define UCS_LIMIT	0xffff
#endif


#include DATA_HEADER

/* The shift sequences for this charset (it does not use ESC).  */
#define SI 		0x0F  /* Shift In, host code to turn DBCS off.  */
#define SO 		0x0E  /* Shift Out, host code to turn DBCS on.  */

/* Definitions used in the body of the `gconv' function.  */
#define MIN_NEEDED_FROM	1
#define MAX_NEEDED_FROM	2
#define MIN_NEEDED_TO	4
#ifdef HAS_COMBINED
# define MAX_NEEDED_TO	8
#else
# define MAX_NEEDED_TO	4
#endif
#define ONE_DIRECTION	0
#define PREPARE_LOOP \
  int save_curcs;							      \
  int *curcsp = &data->__statep->__count;
#define EXTRA_LOOP_ARGS		, curcsp

/* Definitions of initialization and destructor function.  */
#define DEFINE_INIT	1
#define DEFINE_FINI	1


/* Since this is a stateful encoding we have to provide code which resets
   the output state to the initial state.  This has to be done during the
   flushing.  For the to-internal direction (FROM_DIRECTION is true),
   there may be a pending character that needs flushing.  */
#define EMIT_SHIFT_TO_INIT \
  if ((data->__statep->__count & ~7) != sb)				      \
    {									      \
      if (FROM_DIRECTION)						      \
	{								      \
	  uint32_t ch = data->__statep->__count >> 7;			      \
	  if (__glibc_unlikely (ch != 0))				      \
	    {								      \
	      if (__glibc_unlikely (outend - outbuf < 4))		      \
		status = __GCONV_FULL_OUTPUT;				      \
	      else							      \
		{							      \
		  put32 (outbuf, ch);					      \
		  outbuf += 4;						      \
		  /* Clear character and db bit.  */			      \
		  data->__statep->__count &= 7;				      \
		}							      \
	    }								      \
	  else								      \
	    data->__statep->__count &= 7;				      \
	}								      \
      else								      \
	{								      \
	  /* We are not in the initial state.  To switch back we have	      \
	     to emit `SI'.  */						      \
	  if (__glibc_unlikely (outbuf >= outend))			      \
	    /* We don't have enough room in the output buffer.  */	      \
	    status = __GCONV_FULL_OUTPUT;				      \
	  else								      \
	    {								      \
	      /* Write out the shift sequence.  */			      \
	      *outbuf++ = SI;						      \
	      data->__statep->__count &= 7;				      \
	    }								      \
	}								      \
    }


/* Since we might have to reset input pointer we must be able to save
   and restore the state.  */
#define SAVE_RESET_STATE(Save) \
  if (Save)								      \
    save_curcs = *curcsp;						      \
  else									      \
    *curcsp = save_curcs


/* Current codeset type.  The bit is stored in the __count variable of
   the conversion state.  If the db bit is set, bit 7 and above store
   a pending UCS-4 code point if non-zero.  */
enum
{
  sb = 0,			/* Single byte mode.  */
  db = 64			/* Double byte mode.  */
};


/* Subroutine to write out converted UCS4 from IBM-13XX.  */
#ifdef HAS_COMBINED
# define SUB_COMBINED_UCS_FROM_IBM13XX \
  {									      \
    if (res != UCS_LIMIT || ch < __TO_UCS4_COMBINED_MIN			      \
	|| ch > __TO_UCS4_COMBINED_MAX)					      \
      {									      \
	put32 (outptr, res);						      \
	outptr += 4;							      \
      }									      \
    else								      \
      {									      \
	const struct divide *cmbp					      \
	  = &DB_TO_UCS4_COMB[ch - __TO_UCS4_COMBINED_MIN];		      \
	assert (cmbp->res1 != 0 && cmbp->res2 != 0);			      \
									      \
	put32 (outptr, cmbp->res1);					      \
	outptr += 4;							      \
									      \
	/* See whether we have room for the second character.  */	      \
	if (outend - outptr >= 4)					      \
	  {								      \
	    put32 (outptr, cmbp->res2);					      \
	    outptr += 4;						      \
	  }								      \
	else								      \
	  {								      \
	    /* Otherwise store only the first character now, and	      \
	       put the second one into the queue.  */			      \
	    curcs |= cmbp->res2 << 7;					      \
	    inptr += 2;							      \
	    /* Tell the caller why we terminate the loop.  */		      \
	    result = __GCONV_FULL_OUTPUT;				      \
	    break;							      \
	  }								      \
      }									      \
  }
#else
# define SUB_COMBINED_UCS_FROM_IBM13XX \
  {									      \
    put32 (outptr, res);						      \
    outptr += 4;							      \
  }
#endif /* HAS_COMBINED */


/* First, define the conversion function from IBM-13XX to UCS4.  */
#define MIN_NEEDED_INPUT  	MIN_NEEDED_FROM
#define MAX_NEEDED_INPUT  	MAX_NEEDED_FROM
#define MIN_NEEDED_OUTPUT 	MIN_NEEDED_TO
#define MAX_NEEDED_OUTPUT 	MAX_NEEDED_TO
#define LOOPFCT 		FROM_LOOP
#define BODY \
  {									      \
    uint32_t ch;							      \
									      \
    ch = curcs >> 7;							      \
    if (__glibc_unlikely (ch != 0))					      \
      {									      \
	put32 (outptr, ch);						      \
	outptr += 4;							      \
	/* Remove the pending character, but preserve state bits.  */	      \
	curcs &= (1 << 7) - 1;						      \
	continue;							      \
      }									      \
									      \
    /* Otherwise read the next input byte.  */				      \
    ch = *inptr;							      \
									      \
    if (__builtin_expect (ch, 0) == SO)					      \
      {									      \
	/* Shift OUT, change to DBCS converter (redundant escape okay).  */   \
	curcs = db;							      \
	++inptr;							      \
	continue;							      \
      }									      \
    if (__builtin_expect (ch, 0) == SI)					      \
      {									      \
	/* Shift IN, change to SBCS converter (redundant escape okay).  */    \
	curcs = sb;							      \
	++inptr;							      \
	continue;							      \
      }									      \
									      \
    if (curcs == sb)							      \
      {									      \
	/* Use the IBM13XX table for single byte.  */			      \
	uint32_t res = SB_TO_UCS4[ch];				      \
	if (__builtin_expect (res, L'\1') == L'\0' && ch != '\0')	      \
	  {								      \
	    /* This is an illegal character.  */			      \
	    if (! ignore_errors_p ())					      \
	      {								      \
		result = __gconv_mark_illegal_input (step_data);	      \
		break;							      \
	      }								      \
	    ++*irreversible;						      \
	  }								      \
	else								      \
	  {								      \
	    put32 (outptr, res);					      \
	    outptr += 4;						      \
	  }								      \
	++inptr;							      \
      }									      \
    else								      \
      {									      \
	assert (curcs == db);						      \
									      \
	if (__glibc_unlikely (inptr + 1 >= inend))			      \
	  {								      \
	    /* The second character is not available.  Store the	      \
	       intermediate result.  */					      \
	    result = __GCONV_INCOMPLETE_INPUT;				      \
	    break;							      \
	  }								      \
									      \
	ch = (ch * 0x100) + inptr[1];					      \
									      \
	/* Use the IBM1364 table for double byte.  */			      \
	const struct gap *rp2 = DB_TO_UCS4_IDX;				      \
	while (ch > rp2->end)						      \
	  ++rp2;							      \
									      \
	uint32_t res;							      \
	if (__builtin_expect (rp2->start == 0xffff, 0)			      \
	    || __builtin_expect (ch < rp2->start, 0)			      \
	    || (res = DB_TO_UCS4[ch + rp2->idx],			      \
		__builtin_expect (res, L'\1') == L'\0' && ch != '\0'))	      \
	  {								      \
	    /* This is an illegal character.  */			      \
	    if (! ignore_errors_p ())					      \
	      {								      \
		result = __gconv_mark_illegal_input (step_data);	      \
		break;							      \
	      }								      \
	    ++*irreversible;						      \
	  }								      \
	else								      \
	  {								      \
	    SUB_COMBINED_UCS_FROM_IBM13XX;				      \
	  }								      \
	inptr += 2;							      \
      }									      \
  }
#define LOOP_NEED_FLAGS
#define EXTRA_LOOP_DECLS	, int *curcsp
#define INIT_PARAMS		int curcs = *curcsp & ~7
#define UPDATE_PARAMS		*curcsp = curcs
#include <iconv/loop.c>


/* Subroutine to convert two UCS4 codes to IBM-13XX.  */
#ifdef HAS_COMBINED
# define SUB_COMBINED_UCS_TO_IBM13XX \
  {									      \
    const struct combine *cmbp = UCS4_COMB_TO_DB;			      \
    while (cmbp->res1 < ch)						      \
      ++cmbp;								      \
    /* XXX if last char is beginning of combining store in state */	      \
    if (cmbp->res1 == ch && inptr + 4 < inend)				      \
      {									      \
	/* See if input is part of a combined character.  */		      \
	uint32_t ch_next = get32 (inptr + 4);				      \
	while (cmbp->res2 != ch_next)					      \
	  {								      \
	    ++cmbp;							      \
	    if (cmbp->res1 != ch)					      \
	      goto not_combined;					      \
	  }								      \
									      \
	/* It is a combined character.  First make sure we are in	      \
	   double byte mode.  */					      \
	if (curcs == sb)						      \
	  {								      \
	    /* We know there is room for at least one byte.  */		      \
	    *outptr++ = SO;						      \
	    curcs = db;							      \
	  }								      \
									      \
	if (__glibc_unlikely (outptr + 2 > outend))			      \
	  {								      \
	    result = __GCONV_FULL_OUTPUT;				      \
	    break;							      \
	  }								      \
	*outptr++ = cmbp->ch[0];					      \
	*outptr++ = cmbp->ch[1];					      \
	inptr += 8;							      \
	continue;							      \
									      \
      not_combined:;							      \
      }									      \
  }
#else
# define SUB_COMBINED_UCS_TO_IBM13XX
#endif /* HAS_COMBINED */


/* Next, define the other direction.  */
#define MIN_NEEDED_INPUT	MIN_NEEDED_TO
#define MAX_NEEDED_INPUT  	MAX_NEEDED_TO
#define MIN_NEEDED_OUTPUT	MIN_NEEDED_FROM
#define MAX_NEEDED_OUTPUT	MAX_NEEDED_FROM
#define LOOPFCT			TO_LOOP
#define BODY \
  {									      \
    uint32_t ch = get32 (inptr);					      \
									      \
    if (__glibc_unlikely (ch >= UCS_LIMIT))				      \
      {									      \
	UNICODE_TAG_HANDLER (ch, 4);					      \
									      \
	if (! ignore_errors_p ())					      \
	  {								      \
	    result = __gconv_mark_illegal_input (step_data);		      \
	    break;							      \
	  }								      \
	++*irreversible;						      \
	inptr += 4;							      \
	continue;							      \
      }									      \
									      \
    SUB_COMBINED_UCS_TO_IBM13XX;					      \
									      \
    const struct gap *rp1 = UCS4_TO_SB_IDX;				      \
    while (ch > rp1->end)						      \
      ++rp1;								      \
									      \
    /* Use the UCS4 table for single byte.  */				      \
    const char *cp;							      \
    if (__builtin_expect (ch < rp1->start, 0)				      \
	|| (cp = UCS4_TO_SB[ch + rp1->idx],				      \
	    __builtin_expect (cp[0], L'\1') == L'\0' && ch != '\0'))	      \
      {									      \
	/* Use the UCS4 table for double byte.  */			      \
	const struct gap *rp2 = UCS4_TO_DB_IDX;				      \
	while (ch > rp2->end)						      \
	  ++rp2;							      \
									      \
	if (__builtin_expect (ch < rp2->start, 0)			      \
	    || (cp = UCS4_TO_DB[ch + rp2->idx],				      \
		__builtin_expect (cp[0], L'\1') == L'\0' && ch != '\0'))      \
	  {								      \
	    /* This is an illegal character.  */			      \
	    if (! ignore_errors_p ())					      \
	      {								      \
		result = __gconv_mark_illegal_input (step_data);	      \
		break;							      \
	      }								      \
	    ++*irreversible;						      \
	  }								      \
	else								      \
	  {								      \
	    if (curcs == sb)						      \
	      {								      \
		/* We know there is room for at least one byte.  */	      \
		*outptr++ = SO;						      \
		curcs = db;						      \
	      }								      \
									      \
	    if (__glibc_unlikely (outptr + 2 > outend))			      \
	      {								      \
		result = __GCONV_FULL_OUTPUT;				      \
		break;							      \
	      }								      \
	    *outptr++ = cp[0];						      \
	    *outptr++ = cp[1];						      \
	  }								      \
      }									      \
    else								      \
      {									      \
	if (__glibc_unlikely (curcs == db))				      \
	  {								      \
	    /* We know there is room for at least one byte.  */		      \
	    *outptr++ = SI;						      \
	    curcs = sb;							      \
									      \
	    if (__glibc_unlikely (outptr >= outend))			      \
	      {								      \
		result = __GCONV_FULL_OUTPUT;				      \
		break;							      \
	      }								      \
	  }								      \
									      \
	*outptr++ = cp[0];						      \
      }									      \
									      \
    /* Now that we wrote the output increment the input pointer.  */	      \
    inptr += 4;								      \
  }
#define LOOP_NEED_FLAGS
#define EXTRA_LOOP_DECLS	, int *curcsp
#define INIT_PARAMS		int curcs = *curcsp & ~7
#define REINIT_PARAMS		curcs = *curcsp & ~7
#define UPDATE_PARAMS		*curcsp = curcs
#include <iconv/loop.c>

/* Now define the toplevel functions.  */
#include <iconv/skeleton.c>
