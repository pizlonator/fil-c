/* Internal header for proving correct grouping in strings of numbers.
   Copyright (C) 1995-2024 Free Software Foundation, Inc.
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

#include <limits.h>
#include <stddef.h>
#include <string.h>

#ifndef MAX
#define MAX(a,b)	({ typeof(a) _a = (a); typeof(b) _b = (b); \
			   _a > _b ? _a : _b; })
#endif

#ifdef USE_WIDE_CHAR
# include <wctype.h>
# define L_(Ch) L##Ch
# define UCHAR_TYPE wint_t
# define STRING_TYPE wchar_t
#else
# define L_(Ch) Ch
# define UCHAR_TYPE unsigned char
# define STRING_TYPE char
#endif

#include "grouping.h"

/* Find the maximum prefix of the string between BEGIN and END which
   satisfies the grouping rules.  It is assumed that at least one digit
   follows BEGIN directly.  */

const STRING_TYPE *
#ifdef USE_WIDE_CHAR
__correctly_grouped_prefixwc (const STRING_TYPE *begin, const STRING_TYPE *end,
			      wchar_t thousands,
#else
__correctly_grouped_prefixmb (const STRING_TYPE *begin, const STRING_TYPE *end,
			      const char *thousands,
#endif
			      const char *grouping)
{
  if (grouping == NULL)
    return end;

#ifdef USE_WIDE_CHAR
  size_t thousands_len = 1;
#else
  size_t thousands_len = strlen (thousands);
#endif

  while (end - begin >= thousands_len)
    {
      const STRING_TYPE *cp = end - thousands_len;
      const char *gp = grouping;

      /* Check first group.  */
      while (cp >= begin)
	{
#ifdef USE_WIDE_CHAR
	  if (*cp == thousands)
	    break;
#else
	  if (memcmp (cp, thousands, thousands_len) == 0)
	    break;
#endif
	  --cp;
	}

      /* We allow the representation to contain no grouping at all even if
	 the locale specifies we can have grouping.  */
      if (cp < begin)
	return end;

      if (end - cp == (int) *gp + thousands_len)
	{
	  /* This group matches the specification.  */

	  const STRING_TYPE *new_end;

	  if (cp < begin)
	    /* There is just one complete group.  We are done.  */
	    return end;

	  /* CP points to a thousands separator character.  The preceding
	     remainder of the string from BEGIN to NEW_END is the part we
	     will consider if there is a grouping error in this trailing
	     portion from CP to END.  */
	  new_end = cp;

	  /* Loop while the grouping is correct.  */
	  while (1)
	    {
	      /* Get the next grouping rule.  */
	      ++gp;
	      if (*gp == 0)
		/* If end is reached use last rule.  */
	        --gp;

	      /* Skip the thousands separator.  */
	      --cp;

	      if (*gp == CHAR_MAX
#if CHAR_MIN < 0
		  || *gp < 0
#endif
		  )
	        {
	          /* No more thousands separators are allowed to follow.  */
	          while (cp >= begin)
		    {
#ifdef USE_WIDE_CHAR
		      if (*cp == thousands)
			break;
#else
		      if (memcmp (cp, thousands, thousands_len) == 0)
			break;
#endif
		      --cp;
		    }

	          if (cp < begin)
		    /* OK, only digits followed.  */
		    return end;
	        }
	      else
	        {
		  /* Check the next group.  */
	          const STRING_TYPE *group_end = cp;

		  while (cp >= begin)
		    {
#ifdef USE_WIDE_CHAR
		      if (*cp == thousands)
			break;
#else
		      if (memcmp (cp, thousands, thousands_len) == 0)
			break;
#endif
		      --cp;
		    }

		  if (cp < begin && group_end - cp <= (int) *gp + thousands_len - 1)
		    /* Final group is correct.  */
		    return end;

		  if (cp < begin || group_end - cp != (int) *gp + thousands_len - 1)
		    /* Incorrect group.  Punt.  */
		    break;
		}
	    }

	  /* The trailing portion of the string starting at NEW_END
	     contains a grouping error.  So we will look for a correctly
	     grouped number in the preceding portion instead.  */
	  end = new_end;
	}
      else
	{
	  /* Even the first group was wrong; determine maximum shift.  */
	  if (end - cp > (int) *gp + thousands_len)
	    end = cp + (int) *gp + thousands_len;
	  else if (cp < begin)
	    /* This number does not fill the first group, but is correct.  */
	    return end;
	  else
	    /* CP points to a thousands separator character.  */
	    end = cp;
	}
    }

  return MAX (begin, end);
}
