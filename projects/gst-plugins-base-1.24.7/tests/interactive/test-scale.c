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

static GstElement *
make_pipeline (gint type)
{
  GstElement *result;
  gchar *pstr;

  switch (type) {
    case 0:
      pstr = g_strdup_printf ("videotestsrc ! capsfilter name=filter ! "
          "ximagesink");
      break;
    case 1:
      pstr = g_strdup_printf ("videotestsrc ! queue ! capsfilter name=filter ! "
          "ximagesink");
      break;
    case 2:
      pstr = g_strdup_printf ("videotestsrc ! videoscale ! "
          "capsfilter name=filter ! " "ximagesink");
      break;
    case 3:
      pstr =
          g_strdup_printf ("videotestsrc ! queue ! videoscale ! "
          "capsfilter name=filter ! " "ximagesink");
      break;
    case 4:
      pstr =
          g_strdup_printf ("videotestsrc ! videoscale ! queue ! "
          "capsfilter name=filter ! " "ximagesink");
      break;
    case 5:
      pstr = g_strdup_printf ("v4l2src ! videoconvert ! videoscale ! "
          "capsfilter name=filter ! " "ximagesink");
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
  GstCaps *caps;
  gint width, height;
  gint xdir, ydir;
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

    filter = gst_bin_get_by_name (GST_BIN (pipe), "filter");
    g_assert (filter);

    width = 320;
    height = 240;
    xdir = ydir = -10;

    for (round = 0; round < MAX_ROUND; round++) {
      gchar *capsstr;
      g_print ("resize to %dx%d (%d/%d)   \r", width, height, round, MAX_ROUND);

      /* we prefer our fixed width and height but allow other dimensions to pass
       * as well */
      capsstr =
          g_strdup_printf ("video/x-raw, width=(int)%d, height=(int)%d;"
          "video/x-raw", width, height);

      caps = gst_caps_from_string (capsstr);
      g_free (capsstr);
      g_object_set (filter, "caps", caps, NULL);
      gst_caps_unref (caps);

      if (round == 0)
        gst_element_set_state (pipe, GST_STATE_PLAYING);

      width += xdir;
      if (width >= 320)
        xdir = -10;
      else if (width < 200)
        xdir = 10;

      height += ydir;
      if (height >= 240)
        ydir = -10;
      else if (height < 150)
        ydir = 10;

      message =
          gst_bus_poll (GST_ELEMENT_BUS (pipe), GST_MESSAGE_ERROR,
          50 * GST_MSECOND);
      if (message) {
        g_print ("got error           \n");

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
