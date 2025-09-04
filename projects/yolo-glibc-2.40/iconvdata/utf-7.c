/* Conversion module for UTF-7.
   Copyright (C) 2000-2024 Free Software Foundation, Inc.
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

/* UTF-7 is a legacy encoding used for transmitting Unicode within the
   ASCII character set, used primarily by mail agents.  New programs
   are encouraged to use UTF-8 instead.

   UTF-7 is specified in RFC 2152 (and old RFC 1641, RFC 1642).  The
   original Base64 encoding is defined in RFC 2045.  */

#include <dlfcn.h>
#include <gconv.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


enum variant
{
  UTF7,
  UTF_7_IMAP
};

/* Must be in the same order as enum variant above.  */
static const char names[] =
  "UTF-7//\0"
  "UTF-7-IMAP//\0"
  "\0";

static uint32_t
shift_character (enum variant const var)
{
  if (var == UTF7)
    return '+';
  else if (var == UTF_7_IMAP)
    return '&';
  else
    abort ();
}

static bool
between (uint32_t const ch,
	 uint32_t const lower_bound, uint32_t const upper_bound)
{
  return (ch >= lower_bound && ch <= upper_bound);
}

/* The set of "direct characters":
   A-Z a-z 0-9 ' ( ) , - . / : ? space tab lf cr
   FOR UTF-7-IMAP
   A-Z a-z 0-9 ' ( ) , - . / : ? space
   ! " # $ % + * ; < = > @ [ \ ] ^ _ ` { | } ~
*/

static bool
isdirect (uint32_t ch, enum variant var)
{
  if (var == UTF7)
    return (between (ch, 'A', 'Z')
	    || between (ch, 'a', 'z')
	    || between (ch, '0', '9')
	    || ch == '\'' || ch == '(' || ch == ')'
	    || between (ch, ',', '/')
	    || ch == ':' || ch == '?'
	    || ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r');
  else if (var == UTF_7_IMAP)
    return (ch != '&' && between (ch, ' ', '~'));
  abort ();
}


/* The set of "direct and optional direct characters":
   A-Z a-z 0-9 ' ( ) , - . / : ? space tab lf cr
   (UTF-7 only)
   ! " # $ % & * ; < = > @ [ ] ^ _ ` { | }
*/

static bool
isxdirect (uint32_t ch, enum variant var)
{
  if (isdirect (ch, var))
    return true;
  if (var != UTF7)
    return false;
  return between (ch, '!', '&')
    || ch == '*'
    || between (ch, ';', '@')
    || (between (ch, '[', '`') && ch != '\\')
    || between (ch, '{', '}');
}


/* Characters which needs to trigger an explicit shift back to US-ASCII (UTF-7
   only): Modified base64 + '-' (shift back character)
   A-Z a-z 0-9 + / -
*/

static bool
needs_explicit_shift (uint32_t ch)
{
  return (between (ch, 'A', 'Z')
	  || between (ch, 'a', 'z')
	  || between (ch, '/', '9') || ch == '+' || ch == '-');
}


/* Converts a value in the range 0..63 to a base64 encoded char.  */
static unsigned char
base64 (unsigned int i, enum variant var)
{
  if (i < 26)
    return i + 'A';
  else if (i < 52)
    return i - 26 + 'a';
  else if (i < 62)
    return i - 52 + '0';
  else if (i == 62)
    return '+';
  else if (i == 63 && var == UTF7)
    return '/';
  else if (i == 63 && var == UTF_7_IMAP)
    return ',';
  else
    abort ();
}


/* Definitions used in the body of the `gconv' function.  */
#define DEFINE_INIT		0
#define DEFINE_FINI		0
#define FROM_LOOP		from_utf7_loop
#define TO_LOOP			to_utf7_loop
#define MIN_NEEDED_FROM		1
#define MAX_NEEDED_FROM		6
#define MIN_NEEDED_TO		4
#define MAX_NEEDED_TO		4
#define ONE_DIRECTION		0
#define FROM_DIRECTION      (dir == from_utf7)
#define PREPARE_LOOP \
  mbstate_t saved_state;						      \
  mbstate_t *statep = data->__statep;					      \
  enum direction dir = ((struct utf7_data *) step->__data)->dir;	      \
  enum variant var = ((struct utf7_data *) step->__data)->var;
#define EXTRA_LOOP_ARGS		, statep, var


enum direction
{
  illegal_dir,
  from_utf7,
  to_utf7
};

struct utf7_data
{
  enum direction dir;
  enum variant var;
};

/* Since we might have to reset input pointer we must be able to save
   and restore the state.  */
#define SAVE_RESET_STATE(Save) \
  if (Save)								      \
    saved_state = *statep;						      \
  else									      \
    *statep = saved_state

int
gconv_init (struct __gconv_step *step)
{
  /* Determine which direction.  */
  struct utf7_data *new_data;
  enum direction dir = illegal_dir;

  enum variant var = 0;
  for (const char *name = names; *name != '\0';
       name = strchr (name, '\0') + 1)
    {
      if (__strcasecmp (step->__from_name, name) == 0)
	{
	  dir = from_utf7;
	  break;
	}
      else if (__strcasecmp (step->__to_name, name) == 0)
	{
	  dir = to_utf7;
	  break;
	}
      ++var;
    }

  if (__glibc_likely (dir != illegal_dir))
    {
      new_data = malloc (sizeof (*new_data));
      if (new_data == NULL)
	return __GCONV_NOMEM;

      new_data->dir = dir;
      new_data->var = var;
      step->__data = new_data;

      if (dir == from_utf7)
	{
	  step->__min_needed_from = MIN_NEEDED_FROM;
	  step->__max_needed_from = MAX_NEEDED_FROM;
	  step->__min_needed_to = MIN_NEEDED_TO;
	  step->__max_needed_to = MAX_NEEDED_TO;
	}
      else
	{
	  step->__min_needed_from = MIN_NEEDED_TO;
	  step->__max_needed_from = MAX_NEEDED_TO;
	  step->__min_needed_to = MIN_NEEDED_FROM;
	  step->__max_needed_to = MAX_NEEDED_FROM;
	}
    }
  else
    return __GCONV_NOCONV;

  step->__stateful = 1;

  return __GCONV_OK;
}

void
gconv_end (struct __gconv_step *data)
{
  free (data->__data);
}



/* First define the conversion function from UTF-7 to UCS4.
   The state is structured as follows:
     __count bit 2..0: zero
     __count bit 8..3: shift
     __wch: data
   Precise meaning:
     shift      data
       0         --          not inside base64 encoding
     1..32  XX..XX00..00     inside base64, (32 - shift) bits pending
   This state layout is simpler than relying on STORE_REST/UNPACK_BYTES.

   When shift = 0, __wch needs to store at most one lookahead byte (see
   __GCONV_INCOMPLETE_INPUT below).
*/
#define MIN_NEEDED_INPUT	MIN_NEEDED_FROM
#define MAX_NEEDED_INPUT	MAX_NEEDED_FROM
#define MIN_NEEDED_OUTPUT	MIN_NEEDED_TO
#define MAX_NEEDED_OUTPUT	MAX_NEEDED_TO
#define LOOPFCT			FROM_LOOP
#define BODY \
  {									      \
    uint_fast8_t ch = *inptr;						      \
									      \
    if ((statep->__count >> 3) == 0)					      \
      {									      \
	/* base64 encoding inactive.  */				      \
	if (isxdirect (ch, var))					      \
	  {								      \
	    inptr++;							      \
	    put32 (outptr, ch);						      \
	    outptr += 4;						      \
	  }								      \
	else if (__glibc_likely (ch == shift_character (var)))		      \
	  {								      \
	    if (__glibc_unlikely (inptr + 2 > inend))			      \
	      {								      \
		/* Not enough input available.  */			      \
		result = __GCONV_INCOMPLETE_INPUT;			      \
		break;							      \
	      }								      \
	    if (inptr[1] == '-')					      \
	      {								      \
		inptr += 2;						      \
		put32 (outptr, ch);					      \
		outptr += 4;						      \
	      }								      \
	    else							      \
	      {								      \
		/* Switch into base64 mode.  */				      \
		inptr++;						      \
		statep->__count = (32 << 3);				      \
		statep->__value.__wch = 0;				      \
	      }								      \
	  }								      \
	else								      \
	  {								      \
	    /* The input is invalid.  */				      \
	    STANDARD_FROM_LOOP_ERR_HANDLER (1);				      \
	  }								      \
      }									      \
    else								      \
      {									      \
	/* base64 encoding active.  */					      \
	uint32_t i;							      \
	int shift;							      \
									      \
	if (ch >= 'A' && ch <= 'Z')					      \
	  i = ch - 'A';							      \
	else if (ch >= 'a' && ch <= 'z')				      \
	  i = ch - 'a' + 26;						      \
	else if (ch >= '0' && ch <= '9')				      \
	  i = ch - '0' + 52;						      \
	else if (ch == '+')						      \
	  i = 62;							      \
	else if ((var == UTF7 && ch == '/')                                   \
		  || (var == UTF_7_IMAP && ch == ','))			      \
	  i = 63;							      \
	else								      \
	  {								      \
	    /* Terminate base64 encoding.  */				      \
									      \
	    /* If accumulated data is nonzero, the input is invalid.  */      \
	    /* Also, partial UTF-16 characters are invalid.  */		      \
	    /* In IMAP variant, must be terminated by '-'.  */		      \
	    if (__glibc_unlikely (statep->__value.__wch != 0)		      \
		|| __glibc_unlikely ((statep->__count >> 3) <= 26)	      \
		|| __glibc_unlikely (var == UTF_7_IMAP && ch != '-'))	      \
	      {								      \
		STANDARD_FROM_LOOP_ERR_HANDLER ((statep->__count = 0, 1));    \
	      }								      \
									      \
	    if (ch == '-')						      \
	      inptr++;							      \
									      \
	    statep->__count = 0;					      \
	    continue;							      \
	  }								      \
									      \
	/* Concatenate the base64 integer i to the accumulator.  */	      \
	shift = (statep->__count >> 3);					      \
	if (shift > 6)							      \
	  {								      \
	    uint32_t wch;						      \
									      \
	    shift -= 6;							      \
	    wch = statep->__value.__wch | (i << shift);			      \
									      \
	    if (shift <= 16 && shift > 10)				      \
	      {								      \
		/* An UTF-16 character has just been completed.  */	      \
		uint32_t wc1 = wch >> 16;				      \
									      \
		/* UTF-16: When we see a High Surrogate, we must also decode  \
		   the following Low Surrogate. */			      \
		if (!(wc1 >= 0xd800 && wc1 < 0xdc00))			      \
		  {							      \
		    wch = wch << 16;					      \
		    shift += 16;					      \
		    put32 (outptr, wc1);				      \
		    outptr += 4;					      \
		  }							      \
	      }								      \
	    else if (shift <= 10 && shift > 4)				      \
	      {								      \
		/* After a High Surrogate, verify that the next 16 bit	      \
		   indeed form a Low Surrogate.  */			      \
		uint32_t wc2 = wch & 0xffff;				      \
									      \
		if (! __glibc_likely (wc2 >= 0xdc00 && wc2 < 0xe000))	      \
		  {							      \
		    STANDARD_FROM_LOOP_ERR_HANDLER ((statep->__count = 0, 1));\
		  }							      \
	      }								      \
									      \
	    statep->__value.__wch = wch;				      \
	  }								      \
	else								      \
	  {								      \
	    /* An UTF-16 surrogate pair has just been completed.  */	      \
	    uint32_t wc1 = (uint32_t) statep->__value.__wch >> 16;	      \
	    uint32_t wc2 = ((uint32_t) statep->__value.__wch & 0xffff)	      \
			   | (i >> (6 - shift));			      \
									      \
	    statep->__value.__wch = (i << shift) << 26;			      \
	    shift += 26;						      \
									      \
	    assert (wc1 >= 0xd800 && wc1 < 0xdc00);			      \
	    assert (wc2 >= 0xdc00 && wc2 < 0xe000);			      \
	    put32 (outptr,						      \
		   0x10000 + ((wc1 - 0xd800) << 10) + (wc2 - 0xdc00));	      \
	    outptr += 4;						      \
	  }								      \
									      \
	statep->__count = shift << 3;					      \
									      \
	/* Now that we digested the input increment the input pointer.  */    \
	inptr++;							      \
      }									      \
  }
#define LOOP_NEED_FLAGS
#define EXTRA_LOOP_DECLS	, mbstate_t *statep, enum variant var
#include <iconv/loop.c>


/* Next, define the conversion from UCS4 to UTF-7.
   The state is structured as follows:
     __count bit 2..0: zero
     __count bit 4..3: shift
     __count bit 8..5: data
   Precise meaning:
     shift      data
       0         0           not inside base64 encoding
       1         0           inside base64, no pending bits
       2       XX00          inside base64, 2 bits known for next byte
       3       XXXX          inside base64, 4 bits known for next byte

   __count bit 2..0 and __wch are always zero, because this direction
   never returns __GCONV_INCOMPLETE_INPUT.
*/
#define MIN_NEEDED_INPUT	MIN_NEEDED_TO
#define MAX_NEEDED_INPUT	MAX_NEEDED_TO
#define MIN_NEEDED_OUTPUT	MIN_NEEDED_FROM
#define MAX_NEEDED_OUTPUT	MAX_NEEDED_FROM
#define LOOPFCT			TO_LOOP
#define BODY \
  {									      \
    uint32_t ch = get32 (inptr);					      \
									      \
    if ((statep->__count & 0x18) == 0)					      \
      {									      \
	/* base64 encoding inactive */					      \
	if (isdirect (ch, var))						      \
	  {								      \
	    *outptr++ = (unsigned char) ch;				      \
	  }								      \
	else								      \
	  {								      \
	    size_t count;						      \
									      \
	    if (ch == shift_character (var))				      \
	      count = 2;						      \
	    else if (ch < 0x10000)					      \
	      count = 3;						      \
	    else if (ch < 0x110000)					      \
	      count = 6;						      \
	    else							      \
	      STANDARD_TO_LOOP_ERR_HANDLER (4);				      \
									      \
	    if (__glibc_unlikely (outptr + count > outend))		      \
	      {								      \
		result = __GCONV_FULL_OUTPUT;				      \
		break;							      \
	      }								      \
									      \
	    *outptr++ = shift_character (var);				      \
	    if (ch == shift_character (var))				      \
	      *outptr++ = '-';						      \
	    else if (ch < 0x10000)					      \
	      {								      \
		*outptr++ = base64 (ch >> 10, var);			      \
		*outptr++ = base64 ((ch >> 4) & 0x3f, var);		      \
		statep->__count = ((ch & 15) << 5) | (3 << 3);		      \
	      }								      \
	    else if (ch < 0x110000)					      \
	      {								      \
		uint32_t ch1 = 0xd800 + ((ch - 0x10000) >> 10);		      \
		uint32_t ch2 = 0xdc00 + ((ch - 0x10000) & 0x3ff);	      \
									      \
		ch = (ch1 << 16) | ch2;					      \
		*outptr++ = base64 (ch >> 26, var);			      \
		*outptr++ = base64 ((ch >> 20) & 0x3f, var);		      \
		*outptr++ = base64 ((ch >> 14) & 0x3f, var);		      \
		*outptr++ = base64 ((ch >> 8) & 0x3f, var);		      \
		*outptr++ = base64 ((ch >> 2) & 0x3f, var);		      \
		statep->__count = ((ch & 3) << 7) | (2 << 3);		      \
	      }								      \
	    else							      \
	      abort ();							      \
	  }								      \
      }									      \
    else								      \
      {									      \
	/* base64 encoding active */					      \
	if ((var == UTF_7_IMAP && ch == '&') || isdirect (ch, var))	      \
	  {								      \
	    /* deactivate base64 encoding */				      \
	    size_t count;						      \
									      \
	    count = ((statep->__count & 0x18) >= 0x10)			      \
	      + (var == UTF_7_IMAP || needs_explicit_shift (ch))	      \
	      + (var == UTF_7_IMAP && ch == '&')			      \
	      + 1;							      \
	    if (__glibc_unlikely (outptr + count > outend))		      \
	      {								      \
		result = __GCONV_FULL_OUTPUT;				      \
		break;							      \
	      }								      \
									      \
	    if ((statep->__count & 0x18) >= 0x10)			      \
	      *outptr++ = base64 ((statep->__count >> 3) & ~3, var);	      \
	    if (var == UTF_7_IMAP || needs_explicit_shift (ch))		      \
	      *outptr++ = '-';						      \
	    *outptr++ = (unsigned char) ch;				      \
	    if (var == UTF_7_IMAP && ch == '&')				      \
	      *outptr++ = '-';						      \
	    statep->__count = 0;					      \
	  }								      \
	else								      \
	  {								      \
	    size_t count;						      \
									      \
	    if (ch < 0x10000)						      \
	      count = ((statep->__count & 0x18) >= 0x10 ? 3 : 2);	      \
	    else if (ch < 0x110000)					      \
	      count = ((statep->__count & 0x18) >= 0x18 ? 6 : 5);	      \
	    else							      \
	      STANDARD_TO_LOOP_ERR_HANDLER (4);				      \
									      \
	    if (__glibc_unlikely (outptr + count > outend))		      \
	      {								      \
		result = __GCONV_FULL_OUTPUT;				      \
		break;							      \
	      }								      \
									      \
	    if (ch < 0x10000)						      \
	      {								      \
		switch ((statep->__count >> 3) & 3)			      \
		  {							      \
		  case 1:						      \
		    *outptr++ = base64 (ch >> 10, var);			      \
		    *outptr++ = base64 ((ch >> 4) & 0x3f, var);		      \
		    statep->__count = ((ch & 15) << 5) | (3 << 3);	      \
		    break;						      \
		  case 2:						      \
		    *outptr++ =						      \
		      base64 (((statep->__count >> 3) & ~3) | (ch >> 12),     \
			      var);					      \
		    *outptr++ = base64 ((ch >> 6) & 0x3f, var);		      \
		    *outptr++ = base64 (ch & 0x3f, var);		      \
		    statep->__count = (1 << 3);				      \
		    break;						      \
		  case 3:						      \
		    *outptr++ =						      \
		      base64 (((statep->__count >> 3) & ~3) | (ch >> 14),     \
			      var);					      \
		    *outptr++ = base64 ((ch >> 8) & 0x3f, var);		      \
		    *outptr++ = base64 ((ch >> 2) & 0x3f, var);		      \
		    statep->__count = ((ch & 3) << 7) | (2 << 3);	      \
		    break;						      \
		  default:						      \
		    abort ();						      \
		  }							      \
	      }								      \
	    else if (ch < 0x110000)					      \
	      {								      \
		uint32_t ch1 = 0xd800 + ((ch - 0x10000) >> 10);		      \
		uint32_t ch2 = 0xdc00 + ((ch - 0x10000) & 0x3ff);	      \
									      \
		ch = (ch1 << 16) | ch2;					      \
		switch ((statep->__count >> 3) & 3)			      \
		  {							      \
		  case 1:						      \
		    *outptr++ = base64 (ch >> 26, var);			      \
		    *outptr++ = base64 ((ch >> 20) & 0x3f, var);	      \
		    *outptr++ = base64 ((ch >> 14) & 0x3f, var);	      \
		    *outptr++ = base64 ((ch >> 8) & 0x3f, var);		      \
		    *outptr++ = base64 ((ch >> 2) & 0x3f, var);		      \
		    statep->__count = ((ch & 3) << 7) | (2 << 3);	      \
		    break;						      \
		  case 2:						      \
		    *outptr++ =						      \
		      base64 (((statep->__count >> 3) & ~3) | (ch >> 28),     \
			      var);					      \
		    *outptr++ = base64 ((ch >> 22) & 0x3f, var);	      \
		    *outptr++ = base64 ((ch >> 16) & 0x3f, var);	      \
		    *outptr++ = base64 ((ch >> 10) & 0x3f, var);	      \
		    *outptr++ = base64 ((ch >> 4) & 0x3f, var);		      \
		    statep->__count = ((ch & 15) << 5) | (3 << 3);	      \
		    break;						      \
		  case 3:						      \
		    *outptr++ =						      \
		      base64 (((statep->__count >> 3) & ~3) | (ch >> 30),     \
			      var);					      \
		    *outptr++ = base64 ((ch >> 24) & 0x3f, var);	      \
		    *outptr++ = base64 ((ch >> 18) & 0x3f, var);	      \
		    *outptr++ = base64 ((ch >> 12) & 0x3f, var);	      \
		    *outptr++ = base64 ((ch >> 6) & 0x3f, var);		      \
		    *outptr++ = base64 (ch & 0x3f, var);		      \
		    statep->__count = (1 << 3);				      \
		    break;						      \
		  default:						      \
		    abort ();						      \
		  }							      \
	      }								      \
	    else							      \
	      abort ();							      \
	  }								      \
      }									      \
									      \
    /* Now that we wrote the output increment the input pointer.  */	      \
    inptr += 4;								      \
  }
#define LOOP_NEED_FLAGS
#define EXTRA_LOOP_DECLS	, mbstate_t *statep, enum variant var
#include <iconv/loop.c>


/* Since this is a stateful encoding we have to provide code which resets
   the output state to the initial state.  This has to be done during the
   flushing.  */
#define EMIT_SHIFT_TO_INIT \
  if (FROM_DIRECTION)							      \
    /* Nothing to emit.  */						      \
    memset (data->__statep, '\0', sizeof (mbstate_t));			      \
  else									      \
    {									      \
      /* The "to UTF-7" direction.  Flush the remaining bits and terminate    \
	 with a '-' byte.  This will guarantee correct decoding if more	      \
	 UTF-7 encoded text is added afterwards.  */			      \
      int state = data->__statep->__count;				      \
									      \
      if (state & 0x18)							      \
	{								      \
	  /* Deactivate base64 encoding.  */				      \
	  size_t count = ((state & 0x18) >= 0x10) + 1;			      \
									      \
	  if (__glibc_unlikely (outbuf + count > outend))		      \
	    /* We don't have enough room in the output buffer.  */	      \
	    status = __GCONV_FULL_OUTPUT;				      \
	  else								      \
	    {								      \
	      /* Write out the shift sequence.  */			      \
	      if ((state & 0x18) >= 0x10)				      \
		*outbuf++ = base64 ((state >> 3) & ~3, var);		      \
	      *outbuf++ = '-';						      \
									      \
	      data->__statep->__count = 0;				      \
	    }								      \
	}								      \
      else								      \
	data->__statep->__count = 0;					      \
    }


/* Now define the toplevel functions.  */
#include <iconv/skeleton.c>
