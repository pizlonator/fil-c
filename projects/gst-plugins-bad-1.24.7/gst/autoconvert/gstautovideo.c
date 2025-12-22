/* GStreamer
 * Copyright 2023 Igalia S.L.
 *  @author: Thibault Saunier <tsaunier@igalia.com>
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
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "gstautovideo.h"

GST_DEBUG_CATEGORY (autovideo_debug);
#define GST_CAT_DEFAULT (autovideo_debug)

static gboolean
append_elements (GString * name, GString * bindesc,
    const gchar * const *elements)
{
  if (!elements) {
    /* Nothing to do. */
    return TRUE;
  }

  for (gint i = 0; elements[i]; i++) {
    gchar **e = g_strsplit (elements[i], " ", 2);
    GstElementFactory *factory = gst_element_factory_find (g_strchomp (e[0]));

    if (!factory) {
      GST_DEBUG_OBJECT (NULL, "Factory %s not found: %s", e[0], elements[i]);

      g_strfreev (e);
      return FALSE;
    }

    if (name) {
      g_string_append (name, e[0]);
    }
    g_strfreev (e);

    if (bindesc->len > 0) {
      g_string_append (bindesc, " ! ");
    }
    g_string_append (bindesc, elements[i]);
  }

  return TRUE;
}

static void
register_known_bin (GstBaseAutoConvert * self,
    const gchar * const *first_elements,
    const gchar * const *colorspace_converters,
    const gchar * const *last_elements,
    const gchar * const *filters, GstRank rank)
{
  GString *name = g_string_new ("autovideoconvert-");
  GString *bindesc = g_string_new ("");

  if (!append_elements (name, bindesc, first_elements))
    goto abort;
  if (!append_elements (name, bindesc, colorspace_converters))
    goto abort;

  for (gint i = 0; filters && filters[i]; i++) {
    const gchar *tmp[2] = { filters[i], NULL };
    if (!append_elements (name, bindesc, tmp))
      goto abort;
    if (!append_elements (NULL, bindesc, colorspace_converters))
      goto abort;
  }
  if (!append_elements (name, bindesc, last_elements))
    goto abort;

  gst_base_auto_convert_register_filter (GST_BASE_AUTO_CONVERT (self),
      g_string_free (name, FALSE), g_string_free (bindesc, FALSE), rank);
  return;

abort:
  g_string_free (name, TRUE);
  g_string_free (bindesc, TRUE);
}


void
gst_auto_video_register_well_known_bins (GstBaseAutoConvert * self,
    const GstAutoVideoFilterGenerator * gen)
{
  static gsize gst_auto_video_register_well_known_bins_once = 0;

  if (g_once_init_enter (&gst_auto_video_register_well_known_bins_once)) {
    GST_DEBUG_CATEGORY_INIT (autovideo_debug, "autovideo", 0, "Auto video");
    g_once_init_leave (&gst_auto_video_register_well_known_bins_once, TRUE);
  }

  for (gint i = 0; gen[i].colorspace_converters[0] || gen[i].first_elements[0]
      || gen[i].last_elements[0] || gen[i].filters[0]; i++) {
    register_known_bin (self, gen[i].first_elements,
        gen[i].colorspace_converters, gen[i].last_elements, gen[i].filters,
        gen[i].rank);
  }
}
