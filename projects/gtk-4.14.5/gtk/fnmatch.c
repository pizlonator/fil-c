/* Copyright (C) 1991, 1992, 1993 Free Software Foundation, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

/*
 * Stripped down, converted to UTF-8 and test cases added
 *
 *                    Owen Taylor, 13 December 2002;
 */

#include "config.h"

#include "gtkprivate.h"

#include <string.h>

#include <glib.h>

static gunichar
get_char (const char **str,
          gboolean     casefold)
{
  gunichar c = g_utf8_get_char (*str);
  *str = g_utf8_next_char (*str);

  if (casefold)
    c = g_unichar_tolower (c);

  return c;
}

#if defined(G_OS_WIN32) || defined(G_WITH_CYGWIN)
#define DO_ESCAPE 0
#else  
#define DO_ESCAPE 1
#endif  

static gunichar
get_unescaped_char (const char **str,
		    gboolean    *was_escaped,
                    gboolean     casefold)
{
  gunichar c = get_char (str, casefold);

  *was_escaped = DO_ESCAPE && c == '\\';
  if (*was_escaped)
    c = get_char (str, casefold);
  
  return c;
}

/* Match STRING against the filename pattern PATTERN, returning zero if
   it matches, nonzero if not.  */

static gboolean
gtk_fnmatch_intern (const char *pattern,
		    const char *string,
		    gboolean    component_start,
		    gboolean    no_leading_period,
                    gboolean    casefold)
{
  const char *p = pattern, *n = string;
  
  while (*p)
    {
      const char *last_n = n;
      
      gunichar c = get_char (&p, casefold);
      gunichar nc = get_char (&n, casefold);
      
      switch (c)
	{
   	case '?':
	  if (nc == '\0')
	    return FALSE;
	  else if (nc == G_DIR_SEPARATOR)
	    return FALSE;
	  else if (nc == '.' && component_start && no_leading_period)
	    return FALSE;
	  break;
	case '\\':
	  if (DO_ESCAPE)
	    c = get_char (&p, casefold);
	  if (nc != c)
	    return FALSE;
	  break;
	case '*':
	  if (nc == '.' && component_start && no_leading_period)
	    return FALSE;

	  {
	    const char *last_p;

	    for (last_p = p, c = get_char (&p, casefold);
		 c == '?' || c == '*';
		 last_p = p, c = get_char (&p, casefold))
	      {
		if (c == '?')
		  {
		    if (nc == '\0')
		      return FALSE;
		    else if (nc == G_DIR_SEPARATOR)
		      return FALSE;
		    else
		      {
			last_n = n; nc = get_char (&n, casefold);
		      }
		  }
	      }

	    /* If the pattern ends with wildcards, we have a
	     * guaranteed match unless there is a dir separator
	     * in the remainder of the string.
	     */
	    if (c == '\0')
	      {
		if (strchr (last_n, G_DIR_SEPARATOR) != NULL)
		  return FALSE;
		else
		  return TRUE;
	      }

	    if (DO_ESCAPE && c == '\\')
	      c = get_char (&p, casefold);

	    for (p = last_p; nc != '\0';)
	      {
		if ((c == '[' || nc == c) &&
		    gtk_fnmatch_intern (p, last_n, component_start, no_leading_period, casefold))
		  return TRUE;
		
		component_start = (nc == G_DIR_SEPARATOR);
		last_n = n;
		nc = get_char (&n, casefold);
	      }
		  
	    return FALSE;
	  }

	case '[':
	  {
	    /* Nonzero if the sense of the character class is inverted.  */
	    gboolean not;
	    gboolean was_escaped;

	    if (nc == '\0' || nc == G_DIR_SEPARATOR)
	      return FALSE;

	    if (nc == '.' && component_start && no_leading_period)
	      return FALSE;

	    not = (*p == '!' || *p == '^');
	    if (not)
	      ++p;

	    c = get_unescaped_char (&p, &was_escaped, casefold);
	    for (;;)
	      {
		register gunichar cstart = c, cend = c;
		if (c == '\0')
		  /* [ (unterminated) loses.  */
		  return FALSE;

		c = get_unescaped_char (&p, &was_escaped, casefold);
		
		if (!was_escaped && c == '-' && *p != ']')
		  {
		    cend = get_unescaped_char (&p, &was_escaped, casefold);
		    if (cend == '\0')
		      return FALSE;

		    c = get_char (&p, casefold);
		  }

		if (nc >= cstart && nc <= cend)
		  goto matched;

		if (!was_escaped && c == ']')
		  break;
	      }
	    if (!not)
	      return FALSE;
	    break;

	  matched:;
	    /* Skip the rest of the [...] that already matched.  */
	    /* XXX 1003.2d11 is unclear if was_escaped is right.  */
	    while (was_escaped || c != ']')
	      {
		if (c == '\0')
		  /* [... (unterminated) loses.  */
		  return FALSE;

		c = get_unescaped_char (&p, &was_escaped, casefold);
	      }
	    if (not)
	      return FALSE;
	  }
	  break;

	default:
	  if (c != nc)
	    return FALSE;
	}

      component_start = (nc == G_DIR_SEPARATOR);
    }

  if (*n == '\0')
    return TRUE;

  return FALSE;
}

/* Match STRING against the filename pattern PATTERN, returning zero if
 *  it matches, nonzero if not.
 *
 * GTK+ used to use a old version of GNU fnmatch() that was buggy
 * in various ways and didn’t handle UTF-8. The following is
 * converted to UTF-8. To simplify the process of making it
 * correct, this is special-cased to the combinations of flags
 * that gtkfilesel.c uses.
 *
 *   FNM_FILE_NAME   - always set
 *   FNM_LEADING_DIR - never set
 *   FNM_NOESCAPE    - set only on windows
 *   FNM_CASEFOLD    - set only on windows
 */
gboolean
_gtk_fnmatch (const char *pattern,
	      const char *string,
	      gboolean no_leading_period,
              gboolean casefold)
{
  return gtk_fnmatch_intern (pattern, string, TRUE, no_leading_period, casefold);
}

/* Turn a glob pattern into a case-insensitive one, by replacing
 * alphabetic characters by [xX] ranges.
 */
char *
_gtk_make_ci_glob_pattern (const char *pattern)
{
  GString *s;
  gboolean in_range = FALSE;

  s = g_string_new ("");
  for (const char *p = pattern; *p; p = g_utf8_next_char (p))
    {
      gunichar c = g_utf8_get_char (p);
      if (in_range)
        {
          g_string_append_unichar (s, c);
          if (c == ']')
            in_range = FALSE;
          continue;
        }

#if DO_ESCAPE
      if (c == '\\')
        {
          g_string_append (s, "\\");
          p = g_utf8_next_char (p);
          if (*p == '\0')
            break;

          c = g_utf8_get_char (p);
          g_string_append_unichar (s, c);
          continue;
        }
#endif

      if (c == '[')
        {
          g_string_append (s, "[");
          p = g_utf8_next_char (p);
          if (*p == '\0')
            break;

          c = g_utf8_get_char (p);
          g_string_append_unichar (s, c);

          in_range = TRUE;
          continue;
        }
      else if (g_unichar_isalpha (c))
        {
          g_string_append (s, "[");
          g_string_append_unichar (s, g_unichar_tolower (c));
          g_string_append_unichar (s, g_unichar_toupper (c));
          g_string_append (s, "]");
        }
      else
        {
          g_string_append_unichar (s, c);
        }
    }

  return g_string_free (s, FALSE);
}
