/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2010 Red Hat, Inc
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <config.h>

#include <glib.h>
#include "info-cleanup.h"

typedef struct
{
  char *regex;
  char *replacement;
} ReplaceStrings;

static char *
prettify_info (const char *info)
{
  g_autofree char *escaped = NULL;
  g_autofree gchar *pretty = NULL;
  int   i;
  static const ReplaceStrings rs[] = {
    { "Mesa DRI ", ""},
    { "Mesa Intel", "Intel"},
    { "[(]R[)]", "\302\256"},
    { "[(](tm|TM)[)]", "\342\204\242"},
    { "(ATI|EPYC|AMD FX|Radeon|Ryzen|Threadripper|GeForce RTX) ", "\\1\342\204\242 "},
    { "Gallium \\d+\\.\\d+ on (.*)", "\\1"},
    { " CPU| Processor| \\S+-Core| @ \\d+\\.\\d+GHz", ""},
    { " x86|/MMX|/SSE2|/PCIe", ""},
    { " [(][^)]*(DRM|MESA|LLVM)[^)]*[)]?", ""},
    { "Graphics Controller", "Graphics"},
    { ".*llvmpipe.*", "Software Rendering"},
  };

  if (*info == '\0')
    return NULL;

  escaped = g_markup_escape_text (info, -1);
  pretty = g_strdup (g_strstrip (escaped));

  for (i = 0; i < G_N_ELEMENTS (rs); i++)
    {
      g_autoptr(GError) error = NULL;
      g_autoptr(GRegex) re = NULL;
      g_autofree gchar *new = NULL;

      re = g_regex_new (rs[i].regex, 0, 0, &error);
      if (re == NULL)
        {
          g_warning ("Error building regex: %s", error->message);
          continue;
        }

      new = g_regex_replace (re,
                             pretty,
                             -1,
                             0,
                             rs[i].replacement,
                             0,
                             &error);

      if (error != NULL)
        {
          g_warning ("Error replacing %s: %s", rs[i].regex, error->message);
          continue;
        }

      g_free (pretty);
      pretty = g_steal_pointer (&new);
    }

  return g_steal_pointer (&pretty);
}

static char *
remove_duplicate_whitespace (const char *old)
{
  g_autofree gchar *new = NULL;
  g_autoptr(GRegex) re = NULL;
  g_autoptr(GError) error = NULL;

  if (old == NULL)
    return NULL;

  re = g_regex_new ("[ \t\n\r]+", G_REGEX_MULTILINE, 0, &error);
  if (re == NULL)
    {
      g_warning ("Error building regex: %s", error->message);
      return g_strdup (old);
    }
  new = g_regex_replace (re,
                         old,
                         -1,
                         0,
                         " ",
                         0,
                         &error);
  if (new == NULL)
    {
      g_warning ("Error replacing string: %s", error->message);
      return g_strdup (old);
    }

  return g_steal_pointer (&new);
}

char *
info_cleanup (const char *input)
{
  g_autofree char *pretty = NULL;

  pretty = prettify_info (input);
  return remove_duplicate_whitespace (pretty);
}
