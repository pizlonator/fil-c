/* Helper functions to handle tunable strings.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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

#ifndef _DL_TUNABLES_PARSE_H
#define _DL_TUNABLES_PARSE_H 1

#include <assert.h>
#include <string.h>

/* Compare the contents of STRVAL with STR of size LEN.  The STR might not
   be null-terminated.   */
static __always_inline bool
tunable_strcmp (const struct tunable_str_t *strval, const char *str,
		size_t len)
{
  return strval->len == len && memcmp (strval->str, str, len) == 0;
}
#define tunable_strcmp_cte(__tunable, __str) \
 tunable_strcmp (&__tunable->strval, __str, sizeof (__str) - 1)

/*
   Helper functions to iterate over a tunable string composed by multiple
   suboptions separated by commaxi; this is a common pattern for CPU.  Each
   suboptions is return in the form of { address, size } (no null terminated).
   For instance:

     struct tunable_str_comma_t ts;
     tunable_str_comma_init (&ts, valp);

     struct tunable_str_t t;
     while (tunable_str_comma_next (&ts, &t))
      {
	_dl_printf ("[%s] %.*s (%d)\n",
		    __func__,
		    (int) tstr.len,
		    tstr.str,
		    (int) tstr.len);

        if (tunable_str_comma_strcmp (&t, opt, opt1_len))
	  {
	    [...]
	  }
	else if (tunable_str_comma_strcmp_cte (&t, "opt2"))
	  {
	    [...]
	  }
      }

   NB: These function are expected to be called from tunable callback
   functions along with tunable_val_t with string types.
*/

struct tunable_str_comma_state_t
{
  const char *p;
  size_t plen;
  size_t maxplen;
};

struct tunable_str_comma_t
{
  const char *str;
  size_t len;
  bool disable;
};

static inline void
tunable_str_comma_init (struct tunable_str_comma_state_t *state,
			tunable_val_t *valp)
{
  assert (valp->strval.str != NULL);
  state->p = valp->strval.str;
  state->plen = 0;
  state->maxplen = valp->strval.len;
}

static inline bool
tunable_str_comma_next (struct tunable_str_comma_state_t *state,
			struct tunable_str_comma_t *str)
{
  if (*state->p == '\0' || state->plen >= state->maxplen)
    return false;

  const char *c;
  for (c = state->p; *c != ','; c++, state->plen++)
    if (*c == '\0' || state->plen == state->maxplen)
      break;

  str->str = state->p;
  str->len = c - state->p;

  if (str->len > 0)
    {
      str->disable = *str->str == '-';
      if (str->disable)
	{
	  str->str = str->str + 1;
	  str->len = str->len - 1;
	}
    }

  state->p = c + 1;
  state->plen++;

  return true;
}

/* Compare the contents of T with STR of size LEN.  The STR might not be
   null-terminated.   */
static __always_inline bool
tunable_str_comma_strcmp (const struct tunable_str_comma_t *t, const char *str,
			  size_t len)
{
  return t->len == len && memcmp (t->str, str, len) == 0;
}
#define tunable_str_comma_strcmp_cte(__t, __str) \
  tunable_str_comma_strcmp (__t, __str, sizeof (__str) - 1)

#endif
