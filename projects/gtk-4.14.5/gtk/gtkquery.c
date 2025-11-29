/*
 * Copyright (C) 2005 Novell, Inc.
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
 *
 * Author: Anders Carlsson <andersca@imendio.com>
 *
 * Based on nautilus-query.c
 */

#include "config.h"
#include <string.h>

#include "gtkquery.h"

struct _GtkQueryPrivate
{
  char *text;
  GFile *location;
  GList *mime_types;
  char **words;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkQuery, gtk_query, G_TYPE_OBJECT)

static void
finalize (GObject *object)
{
  GtkQuery *query = GTK_QUERY (object);
  GtkQueryPrivate *priv = gtk_query_get_instance_private (query);

  g_clear_object (&priv->location);
  g_free (priv->text);
  g_strfreev (priv->words);

  G_OBJECT_CLASS (gtk_query_parent_class)->finalize (object);
}

static void
gtk_query_class_init (GtkQueryClass *class)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (class);
  gobject_class->finalize = finalize;
}

static void
gtk_query_init (GtkQuery *query)
{
}

GtkQuery *
gtk_query_new (void)
{
  return g_object_new (GTK_TYPE_QUERY,  NULL);
}


const char *
gtk_query_get_text (GtkQuery *query)
{
  GtkQueryPrivate *priv = gtk_query_get_instance_private (query);

  return priv->text;
}

void
gtk_query_set_text (GtkQuery    *query,
                    const char *text)
{
  GtkQueryPrivate *priv = gtk_query_get_instance_private (query);

  g_free (priv->text);
  priv->text = g_strdup (text);

  g_strfreev (priv->words);
  priv->words = NULL;
}

GFile *
gtk_query_get_location (GtkQuery *query)
{
  GtkQueryPrivate *priv = gtk_query_get_instance_private (query);

  return priv->location;
}

void
gtk_query_set_location (GtkQuery *query,
                        GFile    *file)
{
  GtkQueryPrivate *priv = gtk_query_get_instance_private (query);

  g_set_object (&priv->location, file);
}

static char *
prepare_string_for_compare (const char *string)
{
  char *normalized, *res;

  normalized = g_utf8_normalize (string, -1, G_NORMALIZE_NFD);
  res = g_utf8_strdown (normalized, -1);
  g_free (normalized);

  return res;
}

gboolean
gtk_query_matches_string (GtkQuery    *query,
                          const char *string)
{
  GtkQueryPrivate *priv = gtk_query_get_instance_private (query);
  char *prepared;
  gboolean found;
  int i;

  if (!priv->text)
    return FALSE;

  if (!priv->words)
    {
      prepared = prepare_string_for_compare (priv->text);
      priv->words = g_strsplit (prepared, " ", -1);
      g_free (prepared);
    }

  prepared = prepare_string_for_compare (string);

  found = TRUE;
  for (i = 0; priv->words[i]; i++)
    {
      if (strstr (prepared, priv->words[i]) == NULL)
        {
          found = FALSE;
          break;
        }
    }

  g_free (prepared);

  return found;
}
