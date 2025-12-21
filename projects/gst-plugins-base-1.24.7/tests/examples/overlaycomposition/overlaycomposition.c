/* GStreamer
 * Copyright (C) 2014 Tim-Philipp Müller <tim centricular com>
 * Copyright (C) 2018 Sebastian Dröge <sebastian@centricular.com>
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
#include <gst/video/video.h>

#include <math.h>

#include <gio/gio.h>
#include <stdlib.h>

#define VIDEO_WIDTH 720
#define VIDEO_HEIGHT 480
#define VIDEO_FPS 50

/* GdkPixbuf RGBA C-Source image dump from gdk-pixbuf-csource --raw,
 * gzipped and then base64 encoded */
const gchar gzipped_pixdata_base64[] =
    "H4sICPX/Z1QAA2xvZ28ucGl4AO2dsZHrNhCG+ewK2II64ClyrhmnTtSBh4kLUOLQAUuwEhSgFtiA"
    "A7agwA2wBT5AXJ5w4P5LgKLEO97ezDf2SCAIAftjFwuQ7/c///ojy/77+8eP7Jcs+/WfLMv+t/zW"
    "dV32pSmK3HK6sXZbFOXV9PZfWipLbWksHdHQZxVpZP+Ee7u62/d7rt0fivIqimJnOX+w/zhauq68"
    "aWjevfdUR1h3vXq/KMqzufueFN1J1FE+stf8KfCzobZ3q/ePojyT3v8gDSyB09GFND7g/N014rpl"
    "41xF+Wz0/i817nwFjepP+Rb0PnDwOUOehePyQq1eurlrSkXZOkVx7OblbGK43upf+zcqyldhOT22"
    "5GvV9ynKXIriQDpKye242Ldcve2KskX6deaRWVeebnpdu32Koryc4t/iaKksmm9Vvg3W3neWk+Vs"
    "qS2tpSOu9NmZtLG4f7R15paS7jXct1q7XxTl2ZDd157dp1A/6q9I+1Wg+QH1g8pmcb7M0szUHseV"
    "/KTTtJg39XyudH/NASmbhfzOUtqTNHmhew1cglhT9ad8O16kv7m4eFT3/pVNQ77IX8shLgW/RnsW"
    "Li7V5y4UJaDo9wnOT9Sjq1fzn4oSQXHft4tZ00Vpr5jI3yiKwmO1sy/63GZKzNpS+dVyLsaYneVk"
    "Ud+rbIqi32M/kC7DtaXzn6vt9Vm95ZbS0lg64rx2nynKdyHQ3oC+A06ZxdtbkztWundpqS1fKo5j"
    "9OfQ8+jKJNbW987eye47QGO5WE6u/BPakJP2rt4927X7JhanNaBBzQUpEMbmU2hJk66O2ftt7lrS"
    "f8vc4yl+0P7llspyWapOl3th9Hdde4yVz4m17eMD2pP8ZBXjI0l3J7oG1Rf1DlH7dyBOpKuB2+dB"
    "2ZzKXS3ugw9+lnKZB49oH2bL1owGH9K4vX5P7Yg+0+O1/Uj/XSRm8ep9Ws6LclrRfe+N1+xYIxjz"
    "49Jrh6D+m68inSypPeQja7pXRZp3/z1P6M7XM+xX+7eznElHc3E63Hv2xWmoo89H4yLEnpDg+orq"
    "DsnJFq7oWqYtLhd7sbTg3i3dL2VO2Xlt5Oo7DzY1Uc8VtOng9eMZtP0ctpnmJa787Tcm/jbUtmHc"
    "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";

static GstBuffer *logo_buf;

static GMainLoop *main_loop;
static gint count;

static GstBuffer *
create_overlay_buffer (void)
{
  GZlibDecompressor *decompress;
  GConverterResult decomp_res;
  guchar *gzipped_pixdata, *pixdata;
  gsize gzipped_size, bytes_read, pixdata_size;
  GstBuffer *logo_pixels;
  guint w, h, stride;

  gzipped_pixdata = g_base64_decode (gzipped_pixdata_base64, &gzipped_size);
  g_assert (gzipped_pixdata != NULL);

  pixdata = g_malloc (64 * 1024);

  decompress = g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP);
  decomp_res = g_converter_convert (G_CONVERTER (decompress),
      gzipped_pixdata, gzipped_size, pixdata, 64 * 1024,
      G_CONVERTER_INPUT_AT_END, &bytes_read, &pixdata_size, NULL);
  g_assert (decomp_res == G_CONVERTER_FINISHED);
  g_assert (bytes_read == gzipped_size);
  g_free (gzipped_pixdata);
  g_object_unref (decompress);

  /* 0: Pixbuf magic (0x47646b50) */
  g_assert (GST_READ_UINT32_BE (pixdata) == 0x47646b50);

  /* 4: length incl. header */
  /* 8: pixdata_type */
  /* 12: rowstride (900) */
  stride = GST_READ_UINT32_BE (pixdata + 12);
  /* 16: width (225) */
  w = GST_READ_UINT32_BE (pixdata + 16);
  /* 20: height (57) */
  h = GST_READ_UINT32_BE (pixdata + 20);
  /* 24: pixel_data */
  GST_LOG ("%dx%d @ %d", w, h, stride);
  /* we assume that the last line also has padding at the end */
  g_assert (pixdata_size - 24 >= h * stride);

  logo_pixels = gst_buffer_new_and_alloc (h * stride);
  gst_buffer_fill (logo_pixels, 0, pixdata + 24, h * stride);
  gst_buffer_add_video_meta (logo_pixels, GST_VIDEO_FRAME_FLAG_NONE,
      GST_VIDEO_OVERLAY_COMPOSITION_FORMAT_RGB, w, h);

  g_free (pixdata);

  return logo_pixels;
}

static gboolean
message_cb (GstBus * bus, GstMessage * message, gpointer user_data)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:{
      GError *err = NULL;
      gchar *name, *debug = NULL;

      name = gst_object_get_path_string (message->src);
      gst_message_parse_error (message, &err, &debug);

      g_printerr ("ERROR: from element %s: %s\n", name, err->message);
      if (debug != NULL)
        g_printerr ("Additional debug info:\n%s\n", debug);

      g_error_free (err);
      g_free (debug);
      g_free (name);

      g_main_loop_quit (main_loop);
      break;
    }
    case GST_MESSAGE_WARNING:{
      GError *err = NULL;
      gchar *name, *debug = NULL;

      name = gst_object_get_path_string (message->src);
      gst_message_parse_warning (message, &err, &debug);

      g_printerr ("ERROR: from element %s: %s\n", name, err->message);
      if (debug != NULL)
        g_printerr ("Additional debug info:\n%s\n", debug);

      g_error_free (err);
      g_free (debug);
      g_free (name);
      break;
    }
    case GST_MESSAGE_EOS:
      g_print ("Got EOS\n");
      g_main_loop_quit (main_loop);
      break;
    default:
      break;
  }

  return TRUE;
}

typedef struct
{
  gboolean valid;
  GstVideoInfo info;
} OverlayState;

static void
prepare_overlay (GstElement * overlay, GstCaps * caps, gint window_width,
    gint window_height, gpointer user_data)
{
  OverlayState *s = (OverlayState *) user_data;

  if (gst_video_info_from_caps (&s->info, caps))
    s->valid = TRUE;
  else
    s->valid = FALSE;
}

#define SPEED_SCALE_FACTOR (VIDEO_FPS * 4)

/* nicked from videotestsrc's ball pattern renderer */
static void
calculate_position (gint * x, gint * y, guint logo_w, guint logo_h, guint n)
{
  guint r_x = logo_w / 2;
  guint r_y = logo_h / 2;
  guint w = VIDEO_WIDTH + logo_w;
  guint h = VIDEO_HEIGHT + logo_h;

  *x = r_x + (0.5 + 0.5 * sin (2 * G_PI * n / SPEED_SCALE_FACTOR))
      * (w - 2 * r_x);
  *y = r_y + (0.5 + 0.5 * sin (2 * G_PI * sqrt (2) * n / SPEED_SCALE_FACTOR))
      * (h - 2 * r_y);

  *x -= logo_w;
  *y -= logo_h;
}

static GstVideoOverlayComposition *
draw_overlay (GstElement * overlay, GstSample * sample, gpointer user_data)
{
  OverlayState *s = (OverlayState *) user_data;
  GstVideoOverlayRectangle *rect;
  GstVideoOverlayComposition *comp;
  GstVideoMeta *vmeta;
  gint x, y;

  if (!s->valid)
    return NULL;

  vmeta = gst_buffer_get_video_meta (logo_buf);
  calculate_position (&x, &y, vmeta->width, vmeta->height, ++count);

  rect = gst_video_overlay_rectangle_new_raw (logo_buf, x, y,
      vmeta->width, vmeta->height, GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);
  comp = gst_video_overlay_composition_new (rect);
  gst_video_overlay_rectangle_unref (rect);

  return comp;
}

int
main (int argc, char **argv)
{
  GstElement *pipeline;
  GstElement *src, *capsfilter, *overlay, *conv, *sink;
  GstBus *bus;
  GstCaps *filter_caps;
  OverlayState overlay_state = { 0, };
  GOptionContext *option_ctx;
  GError *error = NULL;
  gchar *video_sink = NULL;
  gboolean ret;
  GOptionEntry options[] = {
    {"videosink", 0, 0, G_OPTION_ARG_STRING, &video_sink,
        "Video sink element to use (default is autovideosink)", NULL},
    {NULL}
  };

  option_ctx = g_option_context_new ("- test overlaycomposition");
  g_option_context_add_main_entries (option_ctx, options, NULL);
  g_option_context_add_group (option_ctx, gst_init_get_option_group ());
  ret = g_option_context_parse (option_ctx, &argc, &argv, &error);
  g_option_context_free (option_ctx);

  if (!ret) {
    g_printerr ("option parsing failed: %s\n", error->message);
    g_clear_error (&error);
    exit (1);
  }

  pipeline = gst_pipeline_new (NULL);
  src = gst_element_factory_make ("videotestsrc", NULL);
  capsfilter = gst_element_factory_make ("capsfilter", NULL);
  overlay = gst_element_factory_make ("overlaycomposition", NULL);
  conv = gst_element_factory_make ("videoconvert", NULL);

  if (!video_sink)
    video_sink = g_strdup ("autovideosink");

  sink = gst_element_factory_make (video_sink, NULL);
  g_free (video_sink);

  if (!pipeline || !src || !capsfilter || !overlay || !conv || !sink) {
    g_error ("Failed to create elements");
    return -1;
  }

  gst_bin_add_many (GST_BIN (pipeline), src, capsfilter, overlay, conv, sink,
      NULL);
  if (!gst_element_link_many (src, capsfilter, overlay, conv, sink, NULL)) {
    g_error ("Failed to link elements");
    return -2;
  }

  filter_caps = gst_caps_from_string ("video/x-raw, format = "
      GST_VIDEO_OVERLAY_COMPOSITION_BLEND_FORMATS);
  gst_caps_set_simple (filter_caps,
      "width", G_TYPE_INT, VIDEO_WIDTH,
      "height", G_TYPE_INT, VIDEO_HEIGHT,
      "framerate", GST_TYPE_FRACTION, VIDEO_FPS, 1, NULL);
  g_object_set (capsfilter, "caps", filter_caps, NULL);
  gst_caps_unref (filter_caps);

  g_signal_connect (overlay, "draw", G_CALLBACK (draw_overlay), &overlay_state);
  g_signal_connect (overlay, "caps-changed",
      G_CALLBACK (prepare_overlay), &overlay_state);

  count = 0;
  logo_buf = create_overlay_buffer ();

  main_loop = g_main_loop_new (NULL, FALSE);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_signal_watch (bus);
  g_signal_connect (G_OBJECT (bus), "message", G_CALLBACK (message_cb), NULL);
  gst_object_unref (GST_OBJECT (bus));

  if (gst_element_set_state (pipeline,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
    g_error ("Failed to go into PLAYING state");
    return -3;
  }

  g_main_loop_run (main_loop);

  gst_element_set_state (pipeline, GST_STATE_NULL);

  g_main_loop_unref (main_loop);
  gst_object_unref (pipeline);

  return 0;
}
