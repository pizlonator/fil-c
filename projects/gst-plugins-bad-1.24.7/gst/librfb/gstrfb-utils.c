/* GStreamer
 * Copyright (C) <2022> Marc Leeman <marc.leeman@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * See: https://gitlab.freedesktop.org/gstreamer/gstreamer/-/issues/226
 */

#include "gstrfb-utils.h"

static void
gst_rfb_utils_uri_query_foreach (const gchar * key, const gchar * value,
    GObject * src)
{
  if (key == NULL) {
    GST_WARNING_OBJECT (src, "Refusing to use empty key.");
    return;
  }

  if (value == NULL) {
    GST_WARNING_OBJECT (src, "Refusing to use NULL for key %s.", key);
    return;
  }

  GST_DEBUG_OBJECT (src, "Setting property '%s' to '%s'", key, value);
  gst_util_set_object_arg (src, key, value);
}

void
gst_rfb_utils_set_properties_from_uri_query (GObject * obj, const GstUri * uri)
{
  GHashTable *hash_table;

  g_return_if_fail (uri != NULL);
  hash_table = gst_uri_get_query_table (uri);

  if (hash_table) {
    g_hash_table_foreach (hash_table,
        (GHFunc) gst_rfb_utils_uri_query_foreach, obj);

    g_hash_table_unref (hash_table);
  }
}
