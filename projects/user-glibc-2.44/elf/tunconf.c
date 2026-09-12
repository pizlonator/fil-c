/* Manage /etc/tunables.*
   Copyright (C) 1999-2023 Free Software Foundation, Inc.
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

#include <alloca.h>
#include <argp.h>
#include <assert.h>
#include <error.h>
#include <inttypes.h>
#include <glob.h>
#include <libgen.h>
#include <libintl.h>
#include <locale.h>
#include <programs/xmalloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#define TUNABLES_INTERNAL
#include <elf/dl-tunables.h>
#include <unistd.h>

#include <ldconfig.h>
#include <dl-cache.h>
#include <version.h>
#include <stringtable.h>
#include <array_length.h>

#include "tunconf.h"

/*----------------------------------------------------------------------*/

#ifndef TUNABLES_CONF
# define TUNABLES_CONF SYSCONFDIR "/tunables.conf"
#endif

#ifndef TUNABLES_CACHE
# define TUNABLES_CACHE SYSCONFDIR "/tunables.cache"
#endif

/* Tunable Override Policies.  */
typedef enum {
  TOP_ALLOW = 0,	/* let the environment variable override */
  TOP_DENY		/* no override allowed */
} TOP;

struct tunable_entry_int {
  struct stringtable_entry *name;
  struct stringtable_entry *value;
  struct stringtable_entry *filter;
  TOP top;
  bool exclude_secure:1;
  bool exclude_nonsecure:1;
  int tunable_id;
  int value_is_negative:1;
  int value_was_parsed:1;
  unsigned long long value_ull;
  signed long long value_sll;
  long filter_flags;

  struct tunable_entry_int *next;
};

struct tunable_entry_int *entry_list;

static int filter_flags = 0;
static char *filter_string = NULL;

/*----------------------------------------------------------------------*/

static void
clear_filter (void)
{
  free (filter_string);
  filter_string = NULL;
  filter_flags = 0;
}

/* Filters are lines the are bracketed, like
   [prog:foo]
*/
static void
parse_filter (char *line, const char *filename, int lineno)
{
  const char *colon = NULL;
  const char *right_bracket = NULL;
  const char *cp;

  for (cp = line; *cp != 0; ++cp)
    {
      if (*cp == ':')
	colon = cp;
      if (*cp == ']')
	{
	  right_bracket = cp;
	  break;
	}
    }
  /* Special case: [] means "no filter" */
  if (right_bracket != NULL && right_bracket == line + 1)
    {
      clear_filter ();
      return;
    }
  if (colon == NULL)
    {
      error_at_line (0, 0, filename, lineno,
		     "syntax error, filter line ignored: `%s' (missing ':')\n",
		     line);
      return;
    }
  if (right_bracket == NULL)
    {
      error_at_line (0, 0, filename, lineno,
		     "syntax error, filter line ignored: `%s' (missing ']')\n",
		     line);
      return;
    }

  if (filter_string != NULL)
    {
      clear_filter ();
    }

  if (colon - line - 1 == 4 && memcmp ("proc", line + 1, 4) == 0)
    {
      /* Consider this example: [proc:foo] ..."  */
      /* We allocate 4 bytes, [0] through [3].  */
      filter_string = (char *) xmalloc (right_bracket - colon);
      /* We copy "foo" for 3 bytes, [0] through [2].  */
      memcpy (filter_string, colon + 1, right_bracket - colon - 1);
      /*  [3] = 0 so now "foo\0".  */
      filter_string [right_bracket - colon - 1] = 0;
      filter_flags = TUNCONF_FILTER_PERPROC;
    }

  else
    error_at_line (0, 0, filename, lineno,
		   "unrecognized filter `%.*s', ignored\n",
		   (int)(colon - line - 1), line + 1);
}


static void
add_tunable (char *line, const char *filename, int lineno)
{
  TOP top = TOP_ALLOW;
  char *name;
  char *value;
  char *eq;
  char *orig_line;
  struct tunable_entry_int *entry;
  int i, id;
  static struct tunable_entry_int **entry_list_next = &entry_list;
  bool exclude_secure = 1, exclude_nonsecure = 0;

  /* Denotes file boundaries.  */
  if (line == NULL)
    {
      clear_filter();
      return;
    }

  orig_line = line;

  /* Leading whitespace has already been stripped.  */

  /* Canonicalize the line.  */
  for (i=0; line[i]; i++)
    {
      if (line[i] == '\t')
	line[i] = ' ';
      if (line[i] == '\n' || line[i] == '\r')
	{
	  line[i] = '\0';
	  break;
	}
    }

  /* Parse modifiers.  */
  while (*line)
    {
      int prefix_len;

#define TUN_PREFIX(s)				\
      prefix_len = sizeof(s) - 1,			\
	strncmp (line, s " ", prefix_len + 1) == 0

      if (TUN_PREFIX("overridable"))
	{
	  top = TOP_ALLOW;
	  /* The line++ below skips the space.  */
	  line += prefix_len;
	}
      else if (TUN_PREFIX ("nonoverridable"))
	{
	  top = TOP_DENY;
	  line += prefix_len;
	}
      else if (TUN_PREFIX ("onlysecure"))
	{
	  exclude_nonsecure = 1;
	  exclude_secure = 0;
	  line += prefix_len;
	}
      else if (TUN_PREFIX ("nonsecure"))
	{
	  exclude_secure = 1;
	  exclude_nonsecure = 0;
	  line += prefix_len;
	}
      else if (TUN_PREFIX ("anysecure"))
	{
	  exclude_secure = 0;
	  exclude_nonsecure = 0;
	  line += prefix_len;
	}
      else switch (*line)
	{
	case '+':
	  top = TOP_ALLOW;
	  break;
	case '-':
	  top = TOP_DENY;
	  break;
	case '@':
	  exclude_nonsecure = 1;
	  exclude_secure = 0;
	  break;
	case '$':
	  exclude_nonsecure = 0;
	  exclude_secure = 1;
	  break;
	case '*':
	  exclude_nonsecure = 0;
	  exclude_secure = 0;
	  break;
	case '[':
	  parse_filter (line, filename, lineno);
	  return;
	case ' ':
	  break;

	default:
	  goto done;
	}
      line ++;
    }
 done:

  /* NAME now points to the start of the tunable name.  */
  name = line;

  /* Look for the '=' separator.  */
  eq = strchr (line, '=');
  if (eq == NULL)
    {
      error_at_line (0, 0, filename, lineno,
		     "syntax error, line ignored: `%s' (missing '=')",
		     orig_line);
      return;
    }

  if (eq == name)
    {
      error_at_line (0, 0, filename, lineno,
		     "syntax error, line ignored: `%s' (missing tunable name)",
		     orig_line);
      return;
    }

  /* At this point, EQ actually points to '='.  */
  value = eq + 1;

  while (*value && isspace(*value))
    value ++;

  if (*value == 0)
    {
      error_at_line (0, 0, filename, lineno,
		     "syntax error, line ignored: `%s' (missing value)",
		     orig_line);
      return;
    }

  /* VALUE now points to the start of the value.  */

  /* Split the string into name and value c-strings.  */
  *eq = 0;
  /* Trim trailing whitespace off NAME.  */
  while (*name && isspace (name[strlen(name)-1]))
    name[strlen(name)-1] = 0;
  /* Trim trailing whitespace off VALUE.  */
  while (*value && isspace (value[strlen(value)-1]))
    value[strlen(value)-1] = 0;

  id = -1;
  for (i = 0; i < array_length (tunable_list); i ++)
    if (strcmp (tunable_list[i].name, name) == 0)
      {
	id = i;
	break;
      }
  if (id == -1)
    printf ("%s:%d: Warning: tunable %s not recognized.\n",
	    filename, lineno, name);

  entry = (struct tunable_entry_int *) xcalloc (sizeof (struct tunable_entry_int), 1);
  entry->name = cache_store_string (name);
  entry->value = cache_store_string (value);
  entry->tunable_id = id;
  entry->top = top;
  entry->exclude_secure = exclude_secure;
  entry->exclude_nonsecure = exclude_nonsecure;

  if (filter_flags)
    {
      entry->filter_flags = filter_flags;
      entry->filter = cache_store_string (filter_string);
    }

  if (value[0] == '-')
    {
      entry->value_is_negative = 1;
      if (sscanf (value, "%lld", &entry->value_sll) == 1)
	entry->value_was_parsed = 1;
    }
  else
    {
      entry->value_is_negative = 0;
      if (sscanf (value, "%llu", &entry->value_ull) == 1)
	entry->value_was_parsed = 1;
    }

  *entry_list_next = entry;
  entry_list_next = & (entry->next);
}

void
parse_tunconf (const char *filename, char *opt_chroot)
{
  ldconfig_parse_config (filename, opt_chroot, add_tunable);
}

struct tunable_header_cached *
get_tunconf_ext (uint32_t string_table_offset)
{
  struct tunable_entry_int *tei;
  struct tunable_header_cached *thc;
  size_t count;
  size_t size;

  /* First, count the number of entries we have.  */
  tei = entry_list;
  count = 0;
  while (tei != NULL)
    {
      ++ count;
      tei = tei->next;
    }
  if (count == 0)
    return NULL;

  /* Allocate enough space for the whole cached block.  */
  size = sizeof (struct tunable_header_cached)
       + sizeof (struct tunable_entry_cached) * count;
  thc = (struct tunable_header_cached *) xmalloc (size);

  /* Now, fill in the structures.  */

  thc->signature = TUNCONF_SIGNATURE;
  thc->version = TUNCONF_VERSION;
  thc->num_tunables = count;
  thc->unused_1 = 0;

  tei = entry_list;
  count = 0;
  while (tei != NULL)
    {
      struct tunable_entry_cached *tec;

      tec = & ( thc->tunables[count] );

      tec->flags = 0;
      if (tei->value_was_parsed)
	tec->flags |= TUNCONF_FLAG_PARSED;
      if (tei->value_is_negative)
	tec->flags |= TUNCONF_FLAG_NEGATIVE;
      switch (tei->top)
	{
	case TOP_ALLOW:
	  tec->flags |= TUNCONF_OVERRIDE_ALLOW;
	  break;
	case TOP_DENY:
	  tec->flags |= TUNCONF_OVERRIDE_DENY;
	  break;
	}
      if (tei->exclude_secure)
	tec->flags |= TUNCONF_EXCLUDE_SECURE;
      if (tei->exclude_nonsecure)
	tec->flags |= TUNCONF_EXCLUDE_UNSECURE;

      tec->tunable_id = tei->tunable_id;
      tec->name_offset = tei->name->offset + string_table_offset;
      tec->value_offset = tei->value->offset + string_table_offset;

      if (tei->filter_flags != 0)
	{
	  tec->flag_offset = tei->filter->offset + string_table_offset;
	  tec->flags |= tei->filter_flags;
	}
      else
	tec->flag_offset = 0;

      tec->unused_1 = 0;
      if (tei->value_is_negative)
	tec->parsed_value = (uint64_t) tei->value_sll;
      else
	tec->parsed_value = (uint64_t) tei->value_ull;

      ++ count;
      tei = tei->next;
    }

  return thc;
}
