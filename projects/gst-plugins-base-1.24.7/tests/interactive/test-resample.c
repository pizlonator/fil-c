/* GStreamer interactive audioresample test
 * Copyright (C) 2016 Wim Taymans <wim.taymans@gmail.com>
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
      pstr = g_strdup_printf ("audiotestsrc ! audio/x-raw,rate=44100 ! "
          " audioresample ! capsfilter name=filter ! capssetter caps="
          "audio/x-raw,rate=44100 ! wavenc ! filesink location=test.wav");
      break;
    default:
      return NULL;
  }

  result = gst_parse_launch_full (pstr, NULL, GST_PARSE_FLAG_NONE, NULL);
  g_print ("created test %d: \"%s\"\n", type, pstr);
  g_free (pstr);

  return result;
}

typedef struct
{
  gint rate;
  GstElement *filter;
} Data;

static GstPadProbeReturn
have_probe (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  Data *data = user_data;
  gchar *capsstr;
  GstCaps *caps;

  g_print ("resample to %d   \r", data->rate);

  capsstr = g_strdup_printf ("audio/x-raw, rate=(int)%d", data->rate);
  caps = gst_caps_from_string (capsstr);
  g_free (capsstr);
  g_object_set (data->filter, "caps", caps, NULL);
  gst_caps_unref (caps);

  data->rate += 100;

  if (data->rate > 128000)
    gst_element_post_message (data->filter,
        gst_message_new_application (GST_OBJECT (data->filter),
            gst_structure_new_empty ("my-message")));

  return GST_PAD_PROBE_OK;
}

int
main (int argc, char **argv)
{
  GstElement *pipe;
  GstMessage *message;
  GstPad *srcpad;
  Data data;

  gst_init (&argc, &argv);

  pipe = make_pipeline (0);
  if (pipe == NULL)
    return -1;

  data.rate = 1000;
  data.filter = gst_bin_get_by_name (GST_BIN (pipe), "filter");
  g_assert (data.filter);

  srcpad = gst_element_get_static_pad (data.filter, "src");
  gst_pad_add_probe (srcpad, GST_PAD_PROBE_TYPE_DATA_DOWNSTREAM, have_probe,
      &data, NULL);
  gst_object_unref (srcpad);

  gst_element_set_state (pipe, GST_STATE_PLAYING);

  while (TRUE) {
    message =
        gst_bus_poll (GST_ELEMENT_BUS (pipe),
        GST_MESSAGE_ERROR | GST_MESSAGE_APPLICATION, 50 * GST_MSECOND);
    if (message) {
      g_print ("got error           \n");

      gst_message_unref (message);
      break;
    }
  }
  gst_object_unref (data.filter);
  gst_element_set_state (pipe, GST_STATE_NULL);
  gst_object_unref (pipe);

  return 0;
}
