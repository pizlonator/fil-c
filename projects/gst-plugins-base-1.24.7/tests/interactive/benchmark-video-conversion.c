/* GStreamer video format conversion benchmark
 * Copyright (C) 2014 Wim Taymans <wim.taymans@gmail.com>
 * Copyright (C) 2019 Tim-Philipp Müller <tim centricular com>

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

#define DEFAULT_WIDTH 1920
#define DEFAULT_HEIGHT 1080

#define DEFAULT_DURATION 2.0

static gint
get_num_formats (void)
{
  gint num_formats = 100;

  while (gst_video_format_to_string (num_formats) == NULL)
    --num_formats;
  GST_INFO ("number of known video formats: %d", num_formats);
  return num_formats + 1;
}

static void
do_benchmark_conversions (guint width, guint height, const gchar * in_format,
    const gchar * out_format, gdouble max_duration)
{
  const gchar *infmt_str, *outfmt_str;
  GstVideoFormat infmt, outfmt;
  GTimer *timer;
  gint num_formats;

  timer = g_timer_new ();

  num_formats = get_num_formats ();

  for (infmt = GST_VIDEO_FORMAT_I420; infmt < num_formats; infmt++) {
    GstVideoInfo ininfo;
    GstVideoFrame inframe;
    GstBuffer *inbuffer;

    if (infmt == GST_VIDEO_FORMAT_DMA_DRM)
      continue;

    infmt_str = gst_video_format_to_string (infmt);
    if (in_format != NULL && !g_str_equal (in_format, infmt_str))
      continue;

    gst_video_info_set_format (&ininfo, infmt, width, height);
    inbuffer = gst_buffer_new_and_alloc (ininfo.size);
    gst_buffer_memset (inbuffer, 0, 0, -1);
    gst_video_frame_map (&inframe, &ininfo, inbuffer, GST_MAP_READ);

    for (outfmt = GST_VIDEO_FORMAT_I420; outfmt < num_formats; outfmt++) {
      GstVideoInfo outinfo;
      GstVideoFrame outframe;
      GstBuffer *outbuffer;
      GstVideoConverter *convert;
      gdouble elapsed, convert_sec;
      gint count;

      if (outfmt == GST_VIDEO_FORMAT_DMA_DRM)
        continue;

      outfmt_str = gst_video_format_to_string (outfmt);
      if (out_format != NULL && !g_str_equal (out_format, outfmt_str))
        continue;

      /* Or maybe we should allocate more buffers to minimise cache effects? */
      gst_video_info_set_format (&outinfo, outfmt, width, height);
      outbuffer = gst_buffer_new_and_alloc (outinfo.size);
      gst_video_frame_map (&outframe, &outinfo, outbuffer, GST_MAP_WRITE);

      convert = gst_video_converter_new (&ininfo, &outinfo, NULL);
      /* warmup */
      gst_video_converter_frame (convert, &inframe, &outframe);

      count = 0;
      g_timer_start (timer);
      while (TRUE) {
        gst_video_converter_frame (convert, &inframe, &outframe);

        count++;
        elapsed = g_timer_elapsed (timer, NULL);
        if (elapsed >= max_duration)
          break;
      }

      convert_sec = count / elapsed;

      gst_println ("%8.1f conversions/sec %s -> %s @ %ux%u, %d/%.5f",
          convert_sec, infmt_str, outfmt_str, width, height, count, elapsed);

      gst_video_converter_free (convert);

      gst_video_frame_unmap (&outframe);
      gst_buffer_unref (outbuffer);
    }
    gst_video_frame_unmap (&inframe);
    gst_buffer_unref (inbuffer);
  }

  g_timer_destroy (timer);
}

int
main (int argc, char **argv)
{
  GError *err = NULL;
  gint width = DEFAULT_WIDTH;
  gint height = DEFAULT_HEIGHT;
  gdouble max_dur = DEFAULT_DURATION;
  gchar *from_fmt = NULL;
  gchar *to_fmt = NULL;
  GOptionContext *ctx;
  GOptionEntry options[] = {
    {"width", 'w', 0, G_OPTION_ARG_INT, &width, "Width", NULL},
    {"height", 'h', 0, G_OPTION_ARG_INT, &height, "Height", NULL},
    {"from-format", 'f', 0, G_OPTION_ARG_STRING, &from_fmt, "From Format",
        NULL},
    {"to-format", 't', 0, G_OPTION_ARG_STRING, &to_fmt, "To Format", NULL},
    {"duration", 'd', 0, G_OPTION_ARG_DOUBLE, &max_dur,
        "Benchmark duration for each run (in seconds)", NULL},
    {NULL}
  };

  ctx = g_option_context_new ("");
  g_option_context_add_main_entries (ctx, options, NULL);
  g_option_context_add_group (ctx, gst_init_get_option_group ());
  if (!g_option_context_parse (ctx, &argc, &argv, &err)) {
    g_print ("Error initializing: %s\n", GST_STR_NULL (err->message));
    g_option_context_free (ctx);
    g_clear_error (&err);
    return 1;
  }
  g_option_context_free (ctx);

  do_benchmark_conversions (width, height, from_fmt, to_fmt, max_dur);
  return 0;
}
