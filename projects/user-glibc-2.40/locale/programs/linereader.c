/* Copyright (C) 1996-2024 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published
   by the Free Software Foundation; version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <https://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <libintl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "localedef.h"
#include "charmap.h"
#include "error.h"
#include "linereader.h"
#include "locfile.h"

/* Prototypes for local functions.  */
static struct token *get_toplvl_escape (struct linereader *lr);
static struct token *get_symname (struct linereader *lr);
static struct token *get_ident (struct linereader *lr);
static struct token *get_string (struct linereader *lr,
				 const struct charmap_t *charmap,
				 struct localedef_t *locale,
				 const struct repertoire_t *repertoire,
				 int verbose);
static bool utf8_decode (struct linereader *lr, uint8_t ch1, uint32_t *wch);


struct linereader *
lr_open (const char *fname, kw_hash_fct_t hf)
{
  FILE *fp;

  if (fname == NULL || strcmp (fname, "-") == 0
      || strcmp (fname, "/dev/stdin") == 0)
    return lr_create (stdin, "<stdin>", hf);
  else
    {
      fp = fopen (fname, "rm");
      if (fp == NULL)
	return NULL;
      return lr_create (fp, fname, hf);
    }
}

struct linereader *
lr_create (FILE *fp, const char *fname, kw_hash_fct_t hf)
{
  struct linereader *result;
  int n;

  result = (struct linereader *) xmalloc (sizeof (*result));

  result->fp = fp;
  result->fname = xstrdup (fname);
  result->buf = NULL;
  result->bufsize = 0;
  result->lineno = 1;
  result->idx = 0;
  result->comment_char = '#';
  result->escape_char = '\\';
  result->translate_strings = 1;
  result->return_widestr = 0;

  n = getdelim (&result->buf, &result->bufsize, '\n', result->fp);
  if (n < 0)
    {
      int save = errno;
      fclose (result->fp);
      free ((char *) result->fname);
      free (result);
      errno = save;
      return NULL;
    }

  if (n > 1 && result->buf[n - 2] == '\\' && result->buf[n - 1] == '\n')
    n -= 2;

  result->buf[n] = '\0';
  result->bufact = n;
  result->hash_fct = hf;

  return result;
}


int
lr_eof (struct linereader *lr)
{
  return lr->bufact = 0;
}


void
lr_ignore_rest (struct linereader *lr, int verbose)
{
  if (verbose)
    {
      while (isspace (lr->buf[lr->idx]) && lr->buf[lr->idx] != '\n'
	     && lr->buf[lr->idx] != lr->comment_char)
	if (lr->buf[lr->idx] == '\0')
	  {
	    if (lr_next (lr) < 0)
	      return;
	  }
	else
	  ++lr->idx;

      if (lr->buf[lr->idx] != '\n' && ! feof (lr->fp)
	  && lr->buf[lr->idx] != lr->comment_char)
	lr_error (lr, _("trailing garbage at end of line"));
    }

  /* Ignore continued line.  */
  while (lr->bufact > 0 && lr->buf[lr->bufact - 1] != '\n')
    if (lr_next (lr) < 0)
      break;

  lr->idx = lr->bufact;
}


void
lr_close (struct linereader *lr)
{
  fclose (lr->fp);
  free (lr->buf);
  free (lr);
}


int
lr_next (struct linereader *lr)
{
  int n;

  n = getdelim (&lr->buf, &lr->bufsize, '\n', lr->fp);
  if (n < 0)
    return -1;

  ++lr->lineno;

  if (n > 1 && lr->buf[n - 2] == lr->escape_char && lr->buf[n - 1] == '\n')
    {
#if 0
      /* XXX Is this correct?  */
      /* An escaped newline character is substituted with a single <SP>.  */
      --n;
      lr->buf[n - 1] = ' ';
#else
      n -= 2;
#endif
    }

  lr->buf[n] = '\0';
  lr->bufact = n;
  lr->idx = 0;

  return 0;
}


/* Defined in error.c.  */
/* This variable is incremented each time `error' is called.  */
extern unsigned int error_message_count;

/* The calling program should define program_name and set it to the
   name of the executing program.  */
extern char *program_name;


struct token *
lr_token (struct linereader *lr, const struct charmap_t *charmap,
	  struct localedef_t *locale, const struct repertoire_t *repertoire,
	  int verbose)
{
  int ch;

  while (1)
    {
      do
	{
	  ch = lr_getc (lr);

	  if (ch == EOF)
	    {
	      lr->token.tok = tok_eof;
	      return &lr->token;
	    };

	  if (ch == '\n')
	    {
	      lr->token.tok = tok_eol;
	      return &lr->token;
	    }
	}
      while (isspace (ch));

      if (ch != lr->comment_char)
	break;

      /* Is there an newline at the end of the buffer?  */
      if (lr->buf[lr->bufact - 1] != '\n')
	{
	  /* No.  Some people want this to mean that only the line in
	     the file not the logical, concatenated line is ignored.
	     Let's try this.  */
	  lr->idx = lr->bufact;
	  continue;
	}

      /* Ignore rest of line.  */
      lr_ignore_rest (lr, 0);
      lr->token.tok = tok_eol;
      return &lr->token;
    }

  /* Match escape sequences.  */
  if (ch == lr->escape_char)
    return get_toplvl_escape (lr);

  /* Match ellipsis.  */
  if (ch == '.')
    {
      if (strncmp (&lr->buf[lr->idx], "...(2)....", 10) == 0)
	{
	  int cnt;
	  for (cnt = 0; cnt < 10; ++cnt)
	    lr_getc (lr);
	  lr->token.tok = tok_ellipsis4_2;
	  return &lr->token;
	}
      if (strncmp (&lr->buf[lr->idx], "...", 3) == 0)
	{
	  lr_getc (lr);
	  lr_getc (lr);
	  lr_getc (lr);
	  lr->token.tok = tok_ellipsis4;
	  return &lr->token;
	}
      if (strncmp (&lr->buf[lr->idx], "..", 2) == 0)
	{
	  lr_getc (lr);
	  lr_getc (lr);
	  lr->token.tok = tok_ellipsis3;
	  return &lr->token;
	}
      if (strncmp (&lr->buf[lr->idx], ".(2)..", 6) == 0)
	{
	  int cnt;
	  for (cnt = 0; cnt < 6; ++cnt)
	    lr_getc (lr);
	  lr->token.tok = tok_ellipsis2_2;
	  return &lr->token;
	}
      if (lr->buf[lr->idx] == '.')
	{
	  lr_getc (lr);
	  lr->token.tok = tok_ellipsis2;
	  return &lr->token;
	}
    }

  switch (ch)
    {
    case '<':
      return get_symname (lr);

    case '0' ... '9':
      lr->token.tok = tok_number;
      lr->token.val.num = ch - '0';

      while (isdigit (ch = lr_getc (lr)))
	{
	  lr->token.val.num *= 10;
	  lr->token.val.num += ch - '0';
	}
      if (isalpha (ch))
	lr_error (lr, _("garbage at end of number"));
      lr_ungetn (lr, 1);

      return &lr->token;

    case ';':
      lr->token.tok = tok_semicolon;
      return &lr->token;

    case ',':
      lr->token.tok = tok_comma;
      return &lr->token;

    case '(':
      lr->token.tok = tok_open_brace;
      return &lr->token;

    case ')':
      lr->token.tok = tok_close_brace;
      return &lr->token;

    case '"':
      return get_string (lr, charmap, locale, repertoire, verbose);

    case '-':
      ch = lr_getc (lr);
      if (ch == '1')
	{
	  lr->token.tok = tok_minus1;
	  return &lr->token;
	}
      lr_ungetn (lr, 2);
      break;

    case 0x80 ... 0xff:		/* UTF-8 sequence.  */
      {
	uint32_t wch;
	if (!utf8_decode (lr, ch, &wch))
	  {
	    lr->token.tok = tok_error;
	    return &lr->token;
	  }
	lr->token.tok = tok_ucs4;
	lr->token.val.ucs4 = wch;
	return &lr->token;
      }
    }

  return get_ident (lr);
}


static struct token *
get_toplvl_escape (struct linereader *lr)
{
  /* This is supposed to be a numeric value.  We return the
     numerical value and the number of bytes.  */
  size_t start_idx = lr->idx - 1;
  unsigned char *bytes = lr->token.val.charcode.bytes;
  size_t nbytes = 0;
  int ch;

  do
    {
      unsigned int byte = 0;
      unsigned int base = 8;

      ch = lr_getc (lr);

      if (ch == 'd')
	{
	  base = 10;
	  ch = lr_getc (lr);
	}
      else if (ch == 'x')
	{
	  base = 16;
	  ch = lr_getc (lr);
	}

      if ((base == 16 && !isxdigit (ch))
	  || (base != 16 && (ch < '0' || ch >= (int) ('0' + base))))
	{
	esc_error:
	  lr->token.val.str.startmb = &lr->buf[start_idx];

	  while (ch != EOF && !isspace (ch))
	    ch = lr_getc (lr);
	  lr->token.val.str.lenmb = lr->idx - start_idx;

	  lr->token.tok = tok_error;
	  return &lr->token;
	}

      if (isdigit (ch))
	byte = ch - '0';
      else
	byte = tolower (ch) - 'a' + 10;

      ch = lr_getc (lr);
      if ((base == 16 && !isxdigit (ch))
	  || (base != 16 && (ch < '0' || ch >= (int) ('0' + base))))
	goto esc_error;

      byte *= base;
      if (isdigit (ch))
	byte += ch - '0';
      else
	byte += tolower (ch) - 'a' + 10;

      ch = lr_getc (lr);
      if (base != 16 && isdigit (ch))
	{
	  byte *= base;
	  byte += ch - '0';

	  ch = lr_getc (lr);
	}

      bytes[nbytes++] = byte;
    }
  while (ch == lr->escape_char
	 && nbytes < (int) sizeof (lr->token.val.charcode.bytes));

  if (!isspace (ch))
    lr_error (lr, _("garbage at end of character code specification"));

  lr_ungetn (lr, 1);

  lr->token.tok = tok_charcode;
  lr->token.val.charcode.nbytes = nbytes;

  return &lr->token;
}

/* Multibyte string buffer.  */
struct lr_buffer
{
  size_t act;
  size_t max;
  char *buf;
};

/* Initialize *LRB with a default-sized buffer.  */
static void
lr_buffer_init (struct lr_buffer *lrb)
{
 lrb->act = 0;
 lrb->max = 56;
 lrb->buf = xmalloc (lrb->max);
}

/* Transfers the buffer string from *LRB to LR->token.mbstr.  */
static void
lr_buffer_to_token (struct lr_buffer *lrb, struct linereader *lr)
{
  lr->token.val.str.startmb = xrealloc (lrb->buf, lrb->act + 1);
  lr->token.val.str.startmb[lrb->act] = '\0';
  lr->token.val.str.lenmb = lrb->act;
}

/* Adds CH to *LRB.  */
static void
addc (struct lr_buffer *lrb, char ch)
{
  if (lrb->act == lrb->max)
    {
      lrb->max *= 2;
      lrb->buf = xrealloc (lrb->buf, lrb->max);
    }
  lrb->buf[lrb->act++] = ch;
}

/* Adds L bytes at S to *LRB.  */
static void
adds (struct lr_buffer *lrb, const unsigned char *s, size_t l)
{
  if (lrb->max - lrb->act < l)
    {
      size_t required_size = lrb->act + l;
      size_t new_max = 2 * lrb->max;
      if (new_max < required_size)
	new_max = required_size;
      lrb->buf = xrealloc (lrb->buf, new_max);
      lrb->max = new_max;
    }
  memcpy (lrb->buf + lrb->act, s, l);
  lrb->act += l;
}

#define ADDWC(ch) \
  do									      \
    {									      \
      if (buf2act == buf2max)						      \
	{								      \
	  buf2max *= 2;							      \
	  buf2 = xrealloc (buf2, buf2max * 4);				      \
	}								      \
      buf2[buf2act++] = (ch);						      \
    }									      \
  while (0)


static struct token *
get_symname (struct linereader *lr)
{
  /* Symbol in brackets.  We must distinguish three kinds:
     1. reserved words
     2. ISO 10646 position values
     3. all other.  */
  const struct keyword_t *kw;
  int ch;
  struct lr_buffer lrb;

  lr_buffer_init (&lrb);

  do
    {
      ch = lr_getc (lr);
      if (ch == lr->escape_char)
	{
	  int c2 = lr_getc (lr);
	  addc (&lrb, c2);

	  if (c2 == '\n')
	    ch = '\n';
	}
      else
	addc (&lrb, ch);
    }
  while (ch != '>' && ch != '\n');

  if (ch == '\n')
    lr_error (lr, _("unterminated symbolic name"));

  /* Test for ISO 10646 position value.  */
  if (lrb.buf[0] == 'U' && (lrb.act == 6 || lrb.act == 10))
    {
      char *cp = lrb.buf + 1;
      while (cp < &lrb.buf[lrb.act - 1] && isxdigit (*cp))
	++cp;

      if (cp == &lrb.buf[lrb.act - 1])
	{
	  /* Yes, it is.  */
	  lr->token.tok = tok_ucs4;
	  lr->token.val.ucs4 = strtoul (lrb.buf + 1, NULL, 16);

	  return &lr->token;
	}
    }

  /* It is a symbolic name.  Test for reserved words.  */
  kw = lr->hash_fct (lrb.buf, lrb.act - 1);

  if (kw != NULL && kw->symname_or_ident == 1)
    {
      lr->token.tok = kw->token;
      free (lrb.buf);
    }
  else
    {
      lr->token.tok = tok_bsymbol;
      lr_buffer_to_token (&lrb, lr);
      --lr->token.val.str.lenmb;  /* Hide the training '>'.  */
    }

  return &lr->token;
}


static struct token *
get_ident (struct linereader *lr)
{
  const struct keyword_t *kw;
  int ch;
  struct lr_buffer lrb;

  lr_buffer_init (&lrb);

  addc (&lrb, lr->buf[lr->idx - 1]);

  while (!isspace ((ch = lr_getc (lr))) && ch != '"' && ch != ';'
	 && ch != '<' && ch != ',' && ch != EOF)
    {
      if (ch == lr->escape_char)
	{
	  ch = lr_getc (lr);
	  if (ch == '\n' || ch == EOF)
	    {
	      lr_error (lr, _("invalid escape sequence"));
	      break;
	    }
	}
      addc (&lrb, ch);
    }

  lr_ungetc (lr, ch);

  kw = lr->hash_fct (lrb.buf, lrb.act);

  if (kw != NULL && kw->symname_or_ident == 0)
    {
      lr->token.tok = kw->token;
      free (lrb.buf);
    }
  else
    {
      lr->token.tok = tok_ident;
      lr_buffer_to_token (&lrb, lr);
    }

  return &lr->token;
}

/* Process a decoded Unicode codepoint WCH in a string, placing the
   multibyte sequence into LRB.  Return false if the character is not
   found in CHARMAP/REPERTOIRE.  */
static bool
translate_unicode_codepoint (struct localedef_t *locale,
			     const struct charmap_t *charmap,
			     const struct repertoire_t *repertoire,
			     uint32_t wch, struct lr_buffer *lrb)
{
  /* See whether the charmap contains the Uxxxxxxxx names.  */
  char utmp[10];
  snprintf (utmp, sizeof (utmp), "U%08X", wch);
  struct charseq *seq = charmap_find_value (charmap, utmp, 9);

  if (seq == NULL)
    {
      /* No, this isn't the case.  Now determine from
	 the repertoire the name of the character and
	 find it in the charmap.  */
      if (repertoire != NULL)
	{
	  const char *symbol = repertoire_find_symbol (repertoire, wch);
	  if (symbol != NULL)
	    seq = charmap_find_value (charmap, symbol, strlen (symbol));
	}

      if (seq == NULL)
	{
#ifndef NO_TRANSLITERATION
	  /* Transliterate if possible.  */
	  if (locale != NULL)
	    {
	      if ((locale->avail & CTYPE_LOCALE) == 0)
		{
		  /* Load the CTYPE data now.  */
		  int old_needed = locale->needed;

		  locale->needed = 0;
		  locale = load_locale (LC_CTYPE, locale->name,
					locale->repertoire_name,
					charmap, locale);
		  locale->needed = old_needed;
		}

	      uint32_t *translit;
	      if ((locale->avail & CTYPE_LOCALE) != 0
		  && ((translit = find_translit (locale, charmap, wch))
		      != NULL))
		/* The CTYPE data contains a matching
		   transliteration.  */
		{
		  for (int i = 0; translit[i] != 0; ++i)
		    {
		      snprintf (utmp, sizeof (utmp), "U%08X", translit[i]);
		      seq = charmap_find_value (charmap, utmp, 9);
		      assert (seq != NULL);
		      adds (lrb, seq->bytes, seq->nbytes);
		    }
		  return true;
		}
	    }
#endif	/* NO_TRANSLITERATION */

	  /* Not a known name.  */
	  return false;
	}
    }

  if (seq != NULL)
    {
      adds (lrb, seq->bytes, seq->nbytes);
      return true;
    }
  else
    return false;
}

/* Returns true if ch is not EOF (that is, non-negative) and a valid
   UTF-8 trailing byte.  */
static bool
utf8_valid_trailing (int ch)
{
  return ch >= 0 && (ch & 0xc0) == 0x80;
}

/* Reports an error for a broken UTF-8 sequence.  CH2 to CH4 may be
   EOF.  Always returns false.  */
static bool
utf8_sequence_error (struct linereader *lr, uint8_t ch1, int ch2, int ch3,
		     int ch4)
{
  char buf[38];

  if (ch2 < 0)
    snprintf (buf, sizeof (buf), "0x%02x", ch1);
  else if (ch3 < 0)
    snprintf (buf, sizeof (buf), "0x%02x 0x%02x", ch1, ch2);
  else if (ch4 < 0)
    snprintf (buf, sizeof (buf), "0x%02x 0x%02x 0x%02x", ch1, ch2, ch3);
  else
    snprintf (buf, sizeof (buf), "0x%02x 0x%02x 0x%02x 0x%02x",
	      ch1, ch2, ch3, ch4);

  lr_error (lr, _("invalid UTF-8 sequence %s"), buf);
  return false;
}

/* Reads a UTF-8 sequence from LR, with the leading byte CH1, and
   stores the decoded codepoint in *WCH.  Returns false on failure and
   reports an error.  */
static bool
utf8_decode (struct linereader *lr, uint8_t ch1, uint32_t *wch)
{
  /* See RFC 3629 section 4 and __gconv_transform_utf8_internal.  */
  if (ch1 < 0xc2)
    return utf8_sequence_error (lr, ch1, -1, -1, -1);

  int ch2 = lr_getc (lr);
  if (!utf8_valid_trailing (ch2))
    return utf8_sequence_error (lr, ch1, ch2, -1, -1);

  if (ch1 <= 0xdf)
    {
      uint32_t result = ((ch1 & 0x1f)  << 6) | (ch2 & 0x3f);
      if (result < 0x80)
	return utf8_sequence_error (lr, ch1, ch2, -1, -1);
      *wch = result;
      return true;
    }

  int ch3 = lr_getc (lr);
  if (!utf8_valid_trailing (ch3) || ch1 < 0xe0)
    return utf8_sequence_error (lr, ch1, ch2, ch3, -1);

  if (ch1 <= 0xef)
    {
      uint32_t result = (((ch1 & 0x0f)  << 12)
			 | ((ch2 & 0x3f) << 6)
			 | (ch3 & 0x3f));
      if (result < 0x800)
	return utf8_sequence_error (lr, ch1, ch2, ch3, -1);
      *wch = result;
      return true;
    }

  int ch4 = lr_getc (lr);
  if (!utf8_valid_trailing (ch4) || ch1 < 0xf0 || ch1 > 0xf4)
    return utf8_sequence_error (lr, ch1, ch2, ch3, ch4);

  uint32_t result = (((ch1 & 0x07)  << 18)
		     | ((ch2 & 0x3f) << 12)
		     | ((ch3 & 0x3f) << 6)
		     | (ch4 & 0x3f));
  if (result < 0x10000)
    return utf8_sequence_error (lr, ch1, ch2, ch3, ch4);
  *wch = result;
  return true;
}

static struct token *
get_string (struct linereader *lr, const struct charmap_t *charmap,
	    struct localedef_t *locale, const struct repertoire_t *repertoire,
	    int verbose)
{
  int return_widestr = lr->return_widestr;
  struct lr_buffer lrb;
  wchar_t *buf2 = NULL;

  lr_buffer_init (&lrb);

  /* We know it'll be a string.  */
  lr->token.tok = tok_string;

  /* If we need not translate the strings (i.e., expand <...> parts)
     we can run a simple loop.  */
  if (!lr->translate_strings)
    {
      int ch;

      buf2 = NULL;
      while ((ch = lr_getc (lr)) != '"' && ch != '\n' && ch != EOF)
	{
	  if (ch >= 0x80)
	    lr_error (lr, _("illegal 8-bit character in untranslated string"));
	  addc (&lrb, ch);
	}

      /* Catch errors with trailing escape character.  */
      if (lrb.act > 0 && lrb.buf[lrb.act - 1] == lr->escape_char
	  && (lrb.act == 1 || lrb.buf[lrb.act - 2] != lr->escape_char))
	{
	  lr_error (lr, _("illegal escape sequence at end of string"));
	  --lrb.act;
	}
      else if (ch == '\n' || ch == EOF)
	lr_error (lr, _("unterminated string"));

      addc (&lrb, '\0');
    }
  else
    {
      bool illegal_string = false;
      size_t buf2act = 0;
      size_t buf2max = 56 * sizeof (uint32_t);
      int ch;

      /* We have to provide the wide character result as well.  */
      if (return_widestr)
	buf2 = xmalloc (buf2max);

      /* Read until the end of the string (or end of the line or file).  */
      while ((ch = lr_getc (lr)) != '"' && ch != '\n' && ch != EOF)
	{
	  size_t startidx;
	  uint32_t wch;
	  struct charseq *seq;

	  if (ch != '<')
	    {
	      /* The standards leave it up to the implementation to
		 decide what to do with characters which stand for
		 themselves.  This implementation treats the input
		 file as encoded in UTF-8.  */
	      if (ch == lr->escape_char)
		{
		  ch = lr_getc (lr);
		  if (ch >= 0x80)
		    {
		      lr_error (lr, _("illegal 8-bit escape sequence"));
		      illegal_string = true;
		      break;
		    }
		  if (ch == '\n' || ch == EOF)
		    break;
		  addc (&lrb, ch);
		  wch = ch;
		}
	      else if (ch < 0x80)
		{
		  wch = ch;
		  addc (&lrb, ch);
		}
	      else 		/* UTF-8 sequence.  */
		{
		  if (!utf8_decode (lr, ch, &wch))
		    {
		      illegal_string = true;
		      break;
		    }
		  if (!translate_unicode_codepoint (locale, charmap,
						    repertoire, wch, &lrb))
		    {
		      /* Ignore the rest of the string.  Callers may
			 skip this string because it cannot be encoded
			 in the output character set.  */
		      illegal_string = true;
		      continue;
		    }
		}

	      if (return_widestr)
		ADDWC (wch);

	      continue;
	    }

	  /* Now we have to search for the end of the symbolic name, i.e.,
	     the closing '>'.  */
	  startidx = lrb.act;
	  while ((ch = lr_getc (lr)) != '>' && ch != '\n' && ch != EOF)
	    {
	      if (ch == lr->escape_char)
		{
		  ch = lr_getc (lr);
		  if (ch == '\n' || ch == EOF)
		    break;
		}
	      addc (&lrb, ch);
	    }
	  if (ch == '\n' || ch == EOF)
	    /* Not a correct string.  */
	    break;
	  if (lrb.act == startidx)
	    {
	      /* <> is no correct name.  Ignore it and also signal an
		 error.  */
	      illegal_string = true;
	      continue;
	    }

	  /* It might be a Uxxxx symbol.  */
	  if (lrb.buf[startidx] == 'U'
	      && (lrb.act - startidx == 5 || lrb.act - startidx == 9))
	    {
	      char *cp = lrb.buf + startidx + 1;
	      while (cp < &lrb.buf[lrb.act] && isxdigit (*cp))
		++cp;

	      if (cp == &lrb.buf[lrb.act])
		{
		  /* Yes, it is.  */
		  addc (&lrb, '\0');
		  wch = strtoul (lrb.buf + startidx + 1, NULL, 16);

		  /* Now forget about the name we just added.  */
		  lrb.act = startidx;

		  if (return_widestr)
		    ADDWC (wch);

		  if (!translate_unicode_codepoint (locale, charmap,
						    repertoire, wch, &lrb))
		    illegal_string = true;
		  continue;
		}
	    }

	  /* We now have the symbolic name in lrb.buf[startidx] to
	     lrb.buf[lrb.act-1].  Now find out the value for this character
	     in the charmap as well as in the repertoire map (in this
	     order).  */
	  seq = charmap_find_value (charmap, &lrb.buf[startidx],
				    lrb.act - startidx);

	  if (seq == NULL)
	    {
	      /* This name is not in the charmap.  */
	      lr_error (lr, _("symbol `%.*s' not in charmap"),
			(int) (lrb.act - startidx), &lrb.buf[startidx]);
	      illegal_string = true;
	    }

	  if (return_widestr)
	    {
	      /* Now the same for the multibyte representation.  */
	      if (seq != NULL && seq->ucs4 != UNINITIALIZED_CHAR_VALUE)
		wch = seq->ucs4;
	      else
		{
		  wch = repertoire_find_value (repertoire, &lrb.buf[startidx],
					       lrb.act - startidx);
		  if (seq != NULL)
		    seq->ucs4 = wch;
		}

	      if (wch == ILLEGAL_CHAR_VALUE)
		{
		  /* This name is not in the repertoire map.  */
		  lr_error (lr, _("symbol `%.*s' not in repertoire map"),
			    (int) (lrb.act - startidx), &lrb.buf[startidx]);
		  illegal_string = true;
		}
	      else
		ADDWC (wch);
	    }

	  /* Now forget about the name we just added.  */
	  lrb.act = startidx;

	  /* And copy the bytes.  */
	  if (seq != NULL)
	    adds (&lrb, seq->bytes, seq->nbytes);
	}

      if (ch == '\n' || ch == EOF)
	{
	  lr_error (lr, _("unterminated string"));
	  illegal_string = true;
	}

      if (illegal_string)
	{
	  free (lrb.buf);
	  free (buf2);
	  lr->token.val.str.startmb = NULL;
	  lr->token.val.str.lenmb = 0;
	  lr->token.val.str.startwc = NULL;
	  lr->token.val.str.lenwc = 0;

	  return &lr->token;
	}

      addc (&lrb, '\0');

      if (return_widestr)
	{
	  ADDWC (0);
	  lr->token.val.str.startwc = xrealloc (buf2,
						buf2act * sizeof (uint32_t));
	  lr->token.val.str.lenwc = buf2act;
	}
    }

  lr_buffer_to_token (&lrb, lr);

  return &lr->token;
}
