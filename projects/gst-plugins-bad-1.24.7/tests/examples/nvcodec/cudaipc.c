/* GStreamer
 * Copyright (C) 2024 Seungha Yang <seungha@centricular.com>
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
#include <config.h>
#endif

#include <gst/gst.h>

#define CUSTOM_META_NAME "GstCudaIpcTestMeta"

static GstPadProbeReturn
server_probe_cb (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  GstBuffer *buf;
  GstCustomMeta *meta;
  GstStructure *s;
  gchar *str;

  buf = GST_PAD_PROBE_INFO_BUFFER (info);

  meta = gst_buffer_add_custom_meta (buf, CUSTOM_META_NAME);
  s = gst_custom_meta_get_structure (meta);
  gst_structure_set (s, "foo", G_TYPE_STRING, "bar", "timestamp",
      G_TYPE_UINT64, GST_BUFFER_PTS (buf), NULL);
  str = gst_structure_serialize_full (s, GST_SERIALIZE_FLAG_NONE);

  gst_println ("Added custom meta %s", str);

  return GST_PAD_PROBE_OK;
}

static GstElement *
server_process (const gchar * address)
{
  GError *error = NULL;
  GstPad *sinkpad;
  GstElement *sink;
  GstElement *pipeline =
      gst_parse_launch
      ("videotestsrc ! video/x-raw,format=RGBA,framerate=1/1 ! "
      "queue ! cudaupload ! cudaipcsink name=sink", &error);

  if (!pipeline) {
    gst_printerrln ("couldn't create pipeline, err: %s", error->message);
    g_clear_error (&error);
    return NULL;
  }

  sink = gst_bin_get_by_name (GST_BIN (pipeline), "sink");
  g_assert (sink);

  if (address)
    g_object_set (sink, "address", address, NULL);

  sinkpad = gst_element_get_static_pad (sink, "sink");
  gst_pad_add_probe (sinkpad, GST_PAD_PROBE_TYPE_BUFFER,
      (GstPadProbeCallback) server_probe_cb, NULL, NULL);
  gst_object_unref (sinkpad);

  return pipeline;
}

static GstPadProbeReturn
client_probe_cb (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  GstBuffer *buf;
  GstCustomMeta *meta;

  buf = GST_PAD_PROBE_INFO_BUFFER (info);

  meta = gst_buffer_get_custom_meta (buf, CUSTOM_META_NAME);
  if (!meta) {
    gst_printerrln ("Buffer without meta");
  } else {
    GstStructure *s;
    gchar *str;

    s = gst_custom_meta_get_structure (meta);
    str = gst_structure_serialize_full (s, GST_SERIALIZE_FLAG_NONE);
    gst_println ("Found custom meta \"%s\"", str);
    g_free (str);
  }

  return GST_PAD_PROBE_OK;
}

static GstElement *
client_process (const gchar * address)
{
  GError *error = NULL;
  GstPad *srcpad;
  GstElement *src;
  GstElement *pipeline =
      gst_parse_launch ("cudaipcsrc name=src ! fakesink", &error);

  if (!pipeline) {
    gst_printerrln ("couldn't create pipeline, err: %s", error->message);
    g_clear_error (&error);
    return NULL;
  }

  src = gst_bin_get_by_name (GST_BIN (pipeline), "src");
  g_assert (src);

  if (address)
    g_object_set (src, "address", address, NULL);

  srcpad = gst_element_get_static_pad (src, "src");
  gst_pad_add_probe (srcpad, GST_PAD_PROBE_TYPE_BUFFER,
      (GstPadProbeCallback) client_probe_cb, NULL, NULL);
  gst_object_unref (srcpad);

  return pipeline;
}

gint
main (gint argc, gchar ** argv)
{
  GstElement *pipeline = NULL;
  GstStateChangeReturn sret;
  GError *error = NULL;
  GOptionContext *option_ctx;
  gboolean is_server = FALSE;
  gchar *address = NULL;
  gboolean ret;
  static const gchar *tags[] = { NULL };
  GOptionEntry options[] = {
    {"server", 0, 0, G_OPTION_ARG_NONE, &is_server,
        "Launch server process", NULL},
    {"address", 0, 0, G_OPTION_ARG_STRING, &address,
        "IPC communication address"},
    {NULL}
  };
  GMainLoop *loop;

  option_ctx = g_option_context_new ("CUDA IPC example");
  g_option_context_add_main_entries (option_ctx, options, NULL);
  g_option_context_add_group (option_ctx, gst_init_get_option_group ());
  ret = g_option_context_parse (option_ctx, &argc, &argv, &error);
  g_option_context_free (option_ctx);

  if (!ret) {
    g_printerr ("option parsing failed: %s\n", error->message);
    g_clear_error (&error);
    return 1;
  }

  loop = g_main_loop_new (NULL, FALSE);
  gst_meta_register_custom ("GstCudaIpcTestMeta", tags, NULL, NULL, NULL);

  if (is_server)
    pipeline = server_process (address);
  else
    pipeline = client_process (address);

  if (!pipeline)
    return 1;

  sret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  if (sret == GST_STATE_CHANGE_FAILURE) {
    g_printerr ("Pipeline doesn't want to playing\n");
  } else {
    g_main_loop_run (loop);
  }

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
  g_main_loop_unref (loop);
  g_free (address);

  return 0;
}
