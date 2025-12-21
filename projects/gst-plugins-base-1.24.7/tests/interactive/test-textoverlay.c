/* GStreamer interactive textoverlay test
 * Copyright (C) 2007 Tim-Philipp Müller <tim centricular net>
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

#include <gst/gst.h>

static void
set_enum_property_by_name (gpointer object, const gchar * prop,
    const gchar * value)
{
  GParamSpec *pspec;
  GValue val = { 0, };
  GEnumClass *eclass;
  GEnumValue *eval;

  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (object), prop);
  g_return_if_fail (pspec != NULL);

  g_value_init (&val, pspec->value_type);
  g_object_get_property (G_OBJECT (object), prop, &val);
  eclass = G_ENUM_CLASS (g_type_class_peek (G_VALUE_TYPE (&val)));
  g_return_if_fail (eclass != NULL);
  eval = g_enum_get_value_by_name (eclass, value);
  if (eval == NULL)
    eval = g_enum_get_value_by_nick (eclass, value);
  g_return_if_fail (eval != NULL);
  g_value_set_enum (&val, eval->value);
  g_object_set_property (G_OBJECT (object), prop, &val);
  g_value_unset (&val);
}

static void
show_text (GstElement * textoverlay, const gchar * txt, const gchar * valign,
    const gchar * halign, const gchar * line_align)
{
  GstElement *pipe;

  g_object_set (textoverlay, "text", txt, NULL);

  set_enum_property_by_name (textoverlay, "valignment", valign);
  set_enum_property_by_name (textoverlay, "halignment", halign);
  set_enum_property_by_name (textoverlay, "line-alignment", line_align);

  pipe = textoverlay;
  while (GST_ELEMENT_PARENT (pipe))
    pipe = GST_ELEMENT_PARENT (pipe);

  gst_element_set_state (pipe, GST_STATE_PLAYING);
  gst_bus_poll (GST_ELEMENT_BUS (pipe), GST_MESSAGE_ERROR, GST_SECOND);
  gst_element_set_state (pipe, GST_STATE_NULL);
}

static void
test_textoverlay (int width, int height)
{
  const gchar *valigns[] = { /* "baseline", */ "bottom", "top" };
  const gchar *haligns[] = { "left", "center", "right" };
  const gchar *linealigns[] = { "left", "center", "right" };
  GstElement *pipe, *toverlay;
  gchar *pstr;
  gint a, b, c;

  pstr = g_strdup_printf ("videotestsrc pattern=blue ! "
      "video/x-raw,width=%d,height=%d ! t.video_sink "
      "textoverlay name=t font-desc=\"Sans Serif, 20\" ! "
      " videoconvert ! videoscale ! autovideosink", width, height);

  pipe = gst_parse_launch_full (pstr, NULL, GST_PARSE_FLAG_NONE, NULL);
  g_assert (pipe);

  toverlay = gst_bin_get_by_name (GST_BIN (pipe), "t");
  g_assert (toverlay);

  g_object_set (toverlay, "xpad", 3, "ypad", 3, NULL);

  for (a = 0; a < G_N_ELEMENTS (valigns); ++a) {
    for (b = 0; b < G_N_ELEMENTS (haligns); ++b) {
      for (c = 0; c < G_N_ELEMENTS (linealigns); ++c) {
        gchar *s;

        s = g_strdup_printf ("line-alignment = %s\n"
            "&lt;----- halignment = %s -----&gt;\nvalignment = %s",
            linealigns[c], haligns[b], valigns[a]);
        show_text (toverlay, s, valigns[a], haligns[b], linealigns[c]);
        g_free (s);
      }
    }
  }

  gst_object_unref (toverlay);
  g_free (pstr);
}

int
main (int argc, char **argv)
{
  gst_init (&argc, &argv);

  test_textoverlay (640, 480);

  g_print ("Now with odd width/height ...\n");
  test_textoverlay (639, 479);

  /* test_textoverlay (796, 256); */

  return 0;
}
