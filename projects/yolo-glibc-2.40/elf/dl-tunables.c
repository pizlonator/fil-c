/* The tunable framework.  See the README.tunables to know how to use the
   tunable in a glibc module.

   Copyright (C) 2016-2024 Free Software Foundation, Inc.
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

/* Mark symbols hidden in static PIE for early self relocation to work.  */
#if BUILD_PIE_DEFAULT
# pragma GCC visibility push(hidden)
#endif
#include <startup.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <sysdep.h>
#include <fcntl.h>
#include <ldsodefs.h>
#include <array_length.h>
#include <dl-minimal-malloc.h>
#include <dl-symbol-redir-ifunc.h>

#define TUNABLES_INTERNAL 1
#include "dl-tunables.h"

static char **
get_next_env (char **envp, char **name, char **val, char ***prev_envp)
{
  while (envp != NULL && *envp != NULL)
    {
      char **prev = envp;
      char *envline = *envp++;
      int len = 0;

      while (envline[len] != '\0' && envline[len] != '=')
	len++;

      /* Just the name and no value, go to the next one.  */
      if (envline[len] == '\0')
	continue;

      *name = envline;
      *val = &envline[len + 1];
      *prev_envp = prev;

      return envp;
    }

  return NULL;
}

static void
do_tunable_update_val (tunable_t *cur, const tunable_val_t *valp,
		       const tunable_num_t *minp,
		       const tunable_num_t *maxp)
{
  tunable_num_t val, min, max;

  switch (cur->type.type_code)
    {
    case TUNABLE_TYPE_STRING:
      cur->val.strval = valp->strval;
      cur->initialized = true;
      return;
    case TUNABLE_TYPE_INT_32:
      val = (int32_t) valp->numval;
      break;
    case TUNABLE_TYPE_UINT_64:
      val = (int64_t) valp->numval;
      break;
    case TUNABLE_TYPE_SIZE_T:
      val = (size_t) valp->numval;
      break;
    default:
      __builtin_unreachable ();
    }

  bool unsigned_cmp = unsigned_tunable_type (cur->type.type_code);

  min = minp != NULL ? *minp : cur->type.min;
  max = maxp != NULL ? *maxp : cur->type.max;

  /* We allow only increasingly restrictive bounds.  */
  if (tunable_val_lt (min, cur->type.min, unsigned_cmp))
    min = cur->type.min;

  if (tunable_val_gt (max, cur->type.max, unsigned_cmp))
    max = cur->type.max;

  /* Skip both bounds if they're inconsistent.  */
  if (tunable_val_gt (min, max, unsigned_cmp))
    {
      min = cur->type.min;
      max = cur->type.max;
    }

  /* Bail out if the bounds are not valid.  */
  if (tunable_val_lt (val, min, unsigned_cmp)
      || tunable_val_lt (max, val, unsigned_cmp))
    return;

  cur->val.numval = val;
  cur->type.min = min;
  cur->type.max = max;
  cur->initialized = true;
}

static bool
tunable_parse_num (const char *strval, size_t len, tunable_num_t *val)
{
  char *endptr = NULL;
  uint64_t numval = _dl_strtoul (strval, &endptr);
  if (endptr != strval + len)
    return false;
  *val = (tunable_num_t) numval;
  return true;
}

/* Validate range of the input value and initialize the tunable CUR if it looks
   good.  */
static bool
tunable_initialize (tunable_t *cur, const char *strval, size_t len)
{
  tunable_val_t val = { 0 };

  if (cur->type.type_code != TUNABLE_TYPE_STRING)
    {
      if (!tunable_parse_num (strval, len, &val.numval))
	return false;
    }
  else
    val.strval = (struct tunable_str_t) { strval, len };
  do_tunable_update_val (cur, &val, NULL, NULL);

  return true;
}

bool
__tunable_is_initialized (tunable_id_t id)
{
  return tunable_list[id].initialized;
}
rtld_hidden_def (__tunable_is_initialized)

void
__tunable_set_val (tunable_id_t id, tunable_val_t *valp, tunable_num_t *minp,
		   tunable_num_t *maxp)
{
  tunable_t *cur = &tunable_list[id];

  do_tunable_update_val (cur, valp, minp, maxp);
}

struct tunable_toset_t
{
  tunable_t *t;
  const char *value;
  size_t len;
};

enum { tunables_list_size = array_length (tunable_list) };

/* Parse the tunable string VALSTRING and set TUNABLES with the found tunables
   and their respective values.  The VALSTRING is parsed in place, with the
   tunable start and size recorded in TUNABLES.
   Return the number of tunables found (including 0 if the string is empty)
   or -1 if for an ill-formatted definition.  */
static int
parse_tunables_string (const char *valstring, struct tunable_toset_t *tunables)
{
  if (valstring == NULL || *valstring == '\0')
    return 0;

  const char *p = valstring;
  bool done = false;
  int ntunables = 0;

  while (!done)
    {
      const char *name = p;

      /* First, find where the name ends.  */
      while (*p != '=' && *p != ':' && *p != '\0')
	p++;

      /* If we reach the end of the string before getting a valid name-value
	 pair, bail out.  */
      if (*p == '\0')
	return -1;

      /* We did not find a valid name-value pair before encountering the
	 colon.  */
      if (*p == ':')
	{
	  p++;
	  continue;
	}

      /* Skip the '='.  */
      p++;

      const char *value = p;

      while (*p != '=' && *p != ':' && *p != '\0')
	p++;

      if (*p == '=')
	return -1;
      else if (*p == '\0')
	done = true;

      /* Add the tunable if it exists.  */
      for (size_t i = 0; i < tunables_list_size; i++)
	{
	  tunable_t *cur = &tunable_list[i];

	  if (tunable_is_name (cur->name, name))
	    {
	      tunables[i] = (struct tunable_toset_t) { cur, value, p - value };
	      break;
	    }
	}
    }

  return ntunables;
}

static void
parse_tunable_print_error (const struct tunable_toset_t *toset)
{
  _dl_error_printf ("WARNING: ld.so: invalid GLIBC_TUNABLES value `%.*s' "
		    "for option `%s': ignored.\n",
		    (int) toset->len,
		    toset->value,
		    toset->t->name);
}

static void
parse_tunables (const char *valstring)
{
  struct tunable_toset_t tunables[tunables_list_size] = { 0 };
  if (parse_tunables_string (valstring, tunables) == -1)
    {
      _dl_error_printf (
        "WARNING: ld.so: invalid GLIBC_TUNABLES `%s': ignored.\n", valstring);
      return;
    }

  /* Ignore tunables if enable_secure is set */
  struct tunable_toset_t *tsec =
    &tunables[TUNABLE_ENUM_NAME(glibc, rtld, enable_secure)];
  if (tsec->t != NULL)
    {
      tunable_num_t val;
      if (!tunable_parse_num (tsec->value, tsec->len, &val))
        parse_tunable_print_error (tsec);
      else if (val == 1)
	{
	  __libc_enable_secure = 1;
	  return;
	}
    }

  for (int i = 0; i < tunables_list_size; i++)
    {
      if (tunables[i].t == NULL)
	continue;

      if (!tunable_initialize (tunables[i].t, tunables[i].value,
			       tunables[i].len))
	parse_tunable_print_error (&tunables[i]);
    }
}

/* Initialize the tunables list from the environment.  For now we only use the
   ENV_ALIAS to find values.  Later we will also use the tunable names to find
   values.  */
void
__tunables_init (char **envp)
{
  char *envname = NULL;
  char *envval = NULL;
  char **prev_envp = envp;

  /* Ignore tunables for AT_SECURE programs.  */
  if (__libc_enable_secure)
    return;

  enum { tunable_num_env_alias = array_length (tunable_env_alias_list) };
  struct tunable_toset_t tunables_env_alias[tunable_num_env_alias] = { 0 };

  while ((envp = get_next_env (envp, &envname, &envval, &prev_envp)) != NULL)
    {
      /* The environment variable is allocated on the stack by the kernel, so
	 it is safe to keep the references to the suboptions for later parsing
	 of string tunables.  */
      if (tunable_is_name ("GLIBC_TUNABLES", envname))
	{
	  parse_tunables (envval);
	  continue;
	}

      for (int i = 0; i < tunable_num_env_alias; i++)
	{
	  tunable_t *cur = &tunable_list[tunable_env_alias_list[i]];
	  const char *name = cur->env_alias;

	  if (name[0] == '\0')
	    continue;

	  if (tunable_is_name (name, envname))
	    {
	      size_t envvallen = 0;
	      /* The environment variable is always null-terminated.  */
	      for (const char *p = envval; *p != '\0'; p++, envvallen++);

	      tunables_env_alias[i] =
		(struct tunable_toset_t) { cur, envval, envvallen };
	      break;
	    }
	}
    }

  /* Check if glibc.rtld.enable_secure was set and skip over the environment
     variables aliases.  */
  if (__libc_enable_secure)
    return;

  for (int i = 0; i < tunable_num_env_alias; i++)
    {
      /* Skip over tunables that have either been set or already initialized.  */
      if (tunables_env_alias[i].t == NULL
	  || tunables_env_alias[i].t->initialized)
	continue;

      if (!tunable_initialize (tunables_env_alias[i].t,
			       tunables_env_alias[i].value,
			       tunables_env_alias[i].len))
	parse_tunable_print_error (&tunables_env_alias[i]);
    }
}

void
__tunables_print (void)
{
  for (int i = 0; i < array_length (tunable_list); i++)
    {
      const tunable_t *cur = &tunable_list[i];
      if (cur->type.type_code == TUNABLE_TYPE_STRING
	  && cur->val.strval.str == NULL)
	_dl_printf ("%s:\n", cur->name);
      else
	{
	  _dl_printf ("%s: ", cur->name);
	  switch (cur->type.type_code)
	    {
	    case TUNABLE_TYPE_INT_32:
	      _dl_printf ("%d (min: %d, max: %d)\n",
			  (int) cur->val.numval,
			  (int) cur->type.min,
			  (int) cur->type.max);
	      break;
	    case TUNABLE_TYPE_UINT_64:
	      _dl_printf ("0x%lx (min: 0x%lx, max: 0x%lx)\n",
			  (long int) cur->val.numval,
			  (long int) cur->type.min,
			  (long int) cur->type.max);
	      break;
	    case TUNABLE_TYPE_SIZE_T:
	      _dl_printf ("0x%zx (min: 0x%zx, max: 0x%zx)\n",
			  (size_t) cur->val.numval,
			  (size_t) cur->type.min,
			  (size_t) cur->type.max);
	      break;
	    case TUNABLE_TYPE_STRING:
	      _dl_printf ("%.*s\n",
			  (int) cur->val.strval.len,
			  cur->val.strval.str);
	      break;
	    default:
	      __builtin_unreachable ();
	    }
	}
    }
}

void
__tunable_get_default (tunable_id_t id, void *valp)
{
  tunable_t *cur = &tunable_list[id];

  switch (cur->type.type_code)
    {
    case TUNABLE_TYPE_UINT_64:
	{
	  *((uint64_t *) valp) = (uint64_t) cur->def.numval;
	  break;
	}
    case TUNABLE_TYPE_INT_32:
	{
	  *((int32_t *) valp) = (int32_t) cur->def.numval;
	  break;
	}
    case TUNABLE_TYPE_SIZE_T:
	{
	  *((size_t *) valp) = (size_t) cur->def.numval;
	  break;
	}
    case TUNABLE_TYPE_STRING:
	{
	  *((const struct tunable_str_t **)valp) = &cur->def.strval;
	  break;
	}
    default:
      __builtin_unreachable ();
    }
}
rtld_hidden_def (__tunable_get_default)

/* Set the tunable value.  This is called by the module that the tunable exists
   in. */
void
__tunable_get_val (tunable_id_t id, void *valp, tunable_callback_t callback)
{
  tunable_t *cur = &tunable_list[id];

  switch (cur->type.type_code)
    {
    case TUNABLE_TYPE_UINT_64:
	{
	  *((uint64_t *) valp) = (uint64_t) cur->val.numval;
	  break;
	}
    case TUNABLE_TYPE_INT_32:
	{
	  *((int32_t *) valp) = (int32_t) cur->val.numval;
	  break;
	}
    case TUNABLE_TYPE_SIZE_T:
	{
	  *((size_t *) valp) = (size_t) cur->val.numval;
	  break;
	}
    case TUNABLE_TYPE_STRING:
	{
	  *((const struct tunable_str_t **) valp) = &cur->val.strval;
	  break;
	}
    default:
      __builtin_unreachable ();
    }

  if (cur->initialized && callback != NULL)
    callback (&cur->val);
}

rtld_hidden_def (__tunable_get_val)
