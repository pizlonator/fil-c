/*
 * Copyright © 2017 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *       Christian J. Kellner <christian@kellner.me>
 */

#include "config.h"

#include "bolt-str.h"

#include <string.h>

typedef void (* zero_fn_t) (void  *s,
                            size_t n);
void
bolt_erase_n (void *data, gsize n)
{
#if !HAVE_FN_EXPLICIT_BZERO
  #warning no explicit bzero, using fallback
  static volatile zero_fn_t explicit_bzero = bzero;
#endif

  explicit_bzero (data, n);
}

void
bolt_str_erase (char *str)
{
  if (str == NULL)
    return;

  bolt_erase_n (str, strlen (str));
}

void
bolt_str_erase_clear (char **str)
{
  g_return_if_fail (str != NULL);
  if (*str == NULL)
    return;

  bolt_str_erase (*str);
  g_free (*str);
  *str = NULL;
}

GStrv
bolt_strv_from_ptr_array (GPtrArray **array)
{
  GPtrArray *a;

  if (array == NULL || *array == NULL)
    return NULL;

  a = *array;

  if (a->len == 0 || a->pdata[a->len - 1] != NULL)
    g_ptr_array_add (a, NULL);

  *array = NULL;
  return (GStrv) g_ptr_array_free (a, FALSE);
}

char *
bolt_strdup_validate (const char *string)
{
  g_autofree char *str = NULL;
  gboolean ok;
  gsize l;

  if (string == NULL)
    return NULL;

  str = g_strdup (string);
  str = g_strstrip (str);

  l = strlen (str);
  if (l == 0)
    return NULL;

  ok = g_utf8_validate (str, l, NULL);

  if (!ok)
    return NULL;

  return g_steal_pointer (&str);
}

char *
bolt_strstrip (char *string)
{
  char *str;

  if (string == NULL)
    return NULL;

  str = g_strstrip (string);

  if (strlen (str) == 0)
    g_clear_pointer (&str, g_free);

  return str;
}
