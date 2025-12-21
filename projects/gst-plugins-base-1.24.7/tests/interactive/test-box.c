/* GStreamer interactive videoscale test
 * Copyright (C) 2008 Wim Taymans <wim.taymans@gmail.com>
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

#include <stdlib.h>

#include <gst/gst.h>

#define CAPS " capsfilter caps=\"video/x-raw, format=(string)I420, width=(int)640, height=(int)480\" "

static GstElement *
make_pipeline (gint type)
{
  GstElement *result;
  gchar *pstr;

  switch (type) {
    case 0:
      pstr =
          g_strdup_printf ("videotestsrc ! " CAPS
          " ! videobox name=box ! videoscale ! " CAPS
          " ! videoconvert ! ximagesink");
      break;
    default:
      return NULL;
  }

  result = gst_parse_launch_full (pstr, NULL, GST_PARSE_FLAG_NONE, NULL);
  g_print ("created test %d: \"%s\"\n", type, pstr);
  g_free (pstr);

  return result;
}

#define MAX_ROUND 100

int
main (int argc, char **argv)
{
  GstElement *pipe, *filter;
  gint left, right;
  gint top, bottom;
  gint rdir, ldir;
  gint tdir, bdir;
  gint round, type, stop;

  gst_init (&argc, &argv);

  type = 0;
  stop = -1;

  if (argc > 1) {
    type = atoi (argv[1]);
    stop = type + 1;
  }

  while (TRUE) {
    GstMessage *message;

    pipe = make_pipeline (type);
    if (pipe == NULL)
      break;

    filter = gst_bin_get_by_name (GST_BIN (pipe), "box");
    g_assert (filter);

    /* start with no borders or cropping */
    left = right = top = bottom = 0;
    rdir = ldir = tdir = bdir = -10;

    for (round = 0; round < MAX_ROUND; round++) {
      g_print ("box to %dx%d %dx%d (%d/%d)   \r", left, right, top, bottom,
          round, MAX_ROUND);

      g_object_set (filter, "left", left, "right", right, "top", top, "bottom",
          bottom, NULL);

      if (round == 0)
        gst_element_set_state (pipe, GST_STATE_PLAYING);

      left += ldir;
      if (left >= 40)
        ldir = -10;
      else if (left < -30)
        ldir = 10;

      right += rdir;
      if (right >= 30)
        rdir = -10;
      else if (right < -20)
        rdir = 10;

      top += tdir;
      if (top >= 20)
        tdir = -10;
      else if (top < -30)
        tdir = 10;

      bottom += bdir;
      if (bottom >= 60)
        bdir = -10;
      else if (bottom < -40)
        bdir = 10;

      message =
          gst_bus_poll (GST_ELEMENT_BUS (pipe), GST_MESSAGE_ERROR,
          50 * GST_MSECOND);
      if (message) {
        g_print ("got error                                 \n");

        gst_message_unref (message);
      }
    }
    g_print ("test %d done                    \n", type);

    gst_object_unref (filter);
    gst_element_set_state (pipe, GST_STATE_NULL);
    gst_object_unref (pipe);

    type++;
    if (type == stop)
      break;
  }
  return 0;
}
