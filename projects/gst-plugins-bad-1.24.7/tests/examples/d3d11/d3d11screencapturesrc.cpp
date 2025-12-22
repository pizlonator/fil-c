/* GStreamer
 * Copyright (C) 2021 Seungha Yang <seungha@centricular.com>
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
#include <windows.h>

static GstDevice *
enum_devices (gboolean only_show, gint monitor_idx, guint64 monitor_handle)
{
  GstDeviceMonitor *monitor;
  GstCaps *caps;
  GList *devices, *iter;
  gboolean ret;
  guint i;
  GstDevice *target = nullptr;

  monitor = gst_device_monitor_new ();

  /* Filtering by using d3d11 memory caps with "Source/Monitor" class */
  caps = gst_caps_from_string ("video/x-raw(memory:D3D11Memory)");
  ret = gst_device_monitor_add_filter (monitor, "Source/Monitor", caps);
  gst_caps_unref (caps);

  if (!ret) {
    gst_object_unref (monitor);
    g_warning ("Failed to setup device monitor");
    return nullptr;
  }

  gst_device_monitor_start (monitor);
  devices = gst_device_monitor_get_devices (monitor);

  if (!devices) {
    g_warning ("No detected d3d11 monitor device");
    gst_device_monitor_stop (monitor);
    gst_object_unref (monitor);
    return nullptr;
  }

  gst_println ("Found %d monitor device(s)", g_list_length (devices));

  for (iter = devices, i = 0; iter; iter = g_list_next (iter), i++) {
    GstDevice *dev = GST_DEVICE (iter->data);
    GstStructure *s;
    guint disp_coord_left, disp_coord_top, disp_coord_right, disp_coord_bottom;
    gchar *name = nullptr;
    gchar *adapter_desc = nullptr;
    guint64 hmonitor;
    HMONITOR handle;
    gboolean primary;

    s = gst_device_get_properties (dev);

    gst_structure_get (s,
        "display.coordinates.left", G_TYPE_INT, &disp_coord_left,
        "display.coordinates.top", G_TYPE_INT, &disp_coord_top,
        "display.coordinates.right", G_TYPE_INT, &disp_coord_right,
        "display.coordinates.bottom", G_TYPE_INT, &disp_coord_bottom,
        "device.adapter.description", G_TYPE_STRING, &adapter_desc,
        "device.hmonitor", G_TYPE_UINT64, &hmonitor,
        "device.primary", G_TYPE_BOOLEAN, &primary, nullptr);

    name = gst_device_get_display_name (dev);

    handle = (HMONITOR) hmonitor;

    gst_println ("Monitor %d (%s - %s):", i, name, adapter_desc);
    gst_println ("  HMONITOR: %p (%" G_GUINT64_FORMAT ")", handle, hmonitor);
    gst_println ("  Display Coordinates (left:top:right:bottom): %d:%d:%d:%d\n",
        disp_coord_left, disp_coord_top, disp_coord_right, disp_coord_bottom);

    g_free (adapter_desc);
    g_free (name);

    if (!only_show && !target) {
      if (monitor_handle != 0) {
        if (monitor_handle == hmonitor) {
          target = (GstDevice *) gst_object_ref (dev);
        }
      } else if (monitor_idx < 0) {
        if (primary) {
          target = (GstDevice *) gst_object_ref (dev);
        }
      } else if (monitor_idx == i) {
        target = (GstDevice *) gst_object_ref (dev);
      }

      if (target)
        gst_println ("Found target monitor device");
    }
  }
  g_list_free_full (devices, gst_object_unref);

  return target;
}

static gboolean
bus_msg (GstBus * bus, GstMessage * msg, GMainLoop * loop)
{
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:{
      GError *err;
      gchar *dbg;

      gst_message_parse_error (msg, &err, &dbg);
      g_printerr ("ERROR %s \n", err->message);
      if (dbg != NULL)
        g_printerr ("ERROR debug information: %s\n", dbg);
      g_clear_error (&err);
      g_free (dbg);

      g_main_loop_quit (loop);
      break;
    }
    default:
      break;
  }

  return TRUE;
}

gint
main (gint argc, gchar ** argv)
{
  GstElement *pipeline, *src, *queue, *sink;
  GstElement *pipeline_1 = nullptr, *src_1, *queue_1, *sink_1;
  GMainLoop *loop;
  gboolean ret;
  gboolean show_devices = FALSE;
  gboolean multi_pipelines = FALSE;
  gboolean show_cursor = FALSE;
  gint64 hmonitor = 0;
  gint monitor_index = -1;
  GError *err = nullptr;
  GstDevice *device;
  GOptionContext *option_ctx;
  GOptionEntry options[] = {
    {"show-devices", 0, 0, G_OPTION_ARG_NONE, &show_devices,
        "Display available monitor devices", nullptr},
    {"hmonitor", 0, 0, G_OPTION_ARG_INT64, &hmonitor,
        "Address of HMONITOR handle", nullptr},
    {"index", 0, 0, G_OPTION_ARG_INT, &monitor_index,
        "Monitor index to capture (-1 for primary monitor)", nullptr},
    {"multi-pipelines", 0, 0, G_OPTION_ARG_NONE, &multi_pipelines,
        "Run two separate pipelines for capturing a single monitor", nullptr},
    {"show-cursor", 0, 0, G_OPTION_ARG_NONE, &show_cursor,
        "Draw mouse cursor", nullptr},
    {nullptr}
  };

  option_ctx = g_option_context_new ("D3D11 screen capture example");
  g_option_context_add_main_entries (option_ctx, options, NULL);
  g_option_context_add_group (option_ctx, gst_init_get_option_group ());
  ret = g_option_context_parse (option_ctx, &argc, &argv, &err);
  g_option_context_free (option_ctx);

  if (!ret) {
    g_printerr ("option parsing failed: %s\n", err->message);
    g_clear_error (&err);
    return 1;
  }

  device = enum_devices (show_devices, monitor_index, (guint64) hmonitor);
  if (show_devices) {
    gst_clear_object (&device);
    return 0;
  }

  if (!device) {
    gst_println ("Failed to find monitor device");
    return 1;
  }

  src = gst_device_create_element (device, nullptr);
  if (!src) {
    g_warning ("Failed to create d3d11screencapture element");
    return 1;
  }

  g_object_set (src, "show-cursor", show_cursor, nullptr);

  if (multi_pipelines) {
    src_1 = gst_device_create_element (device, nullptr);
    if (!src_1) {
      g_warning ("Failed to create second d3d11screencapture element");
      return 1;
    }

    g_object_set (src_1, "show-cursor", show_cursor, nullptr);
  }

  gst_object_unref (device);

  loop = g_main_loop_new (nullptr, FALSE);
  pipeline = gst_pipeline_new (nullptr);

  queue = gst_element_factory_make ("queue", nullptr);
  sink = gst_element_factory_make ("d3d11videosink", nullptr);

  gst_bin_add_many (GST_BIN (pipeline), src, queue, sink, nullptr);
  gst_element_link_many (src, queue, sink, nullptr);

  gst_bus_add_watch (GST_ELEMENT_BUS (pipeline), (GstBusFunc) bus_msg, loop);
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  if (multi_pipelines) {
    pipeline_1 = gst_pipeline_new (nullptr);

    queue_1 = gst_element_factory_make ("queue", nullptr);
    sink_1 = gst_element_factory_make ("d3d11videosink", nullptr);

    gst_bin_add_many (GST_BIN (pipeline_1), src_1, queue_1, sink_1, nullptr);
    gst_element_link_many (src_1, queue_1, sink_1, nullptr);

    gst_bus_add_watch (GST_ELEMENT_BUS (pipeline_1), (GstBusFunc) bus_msg, loop);
    gst_element_set_state (pipeline_1, GST_STATE_PLAYING);
  }

  g_main_loop_run (loop);

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_bus_remove_watch (GST_ELEMENT_BUS (pipeline));
  gst_object_unref (pipeline);

  if (multi_pipelines) {
    gst_element_set_state (pipeline_1, GST_STATE_NULL);
    gst_bus_remove_watch (GST_ELEMENT_BUS (pipeline));
    gst_object_unref (pipeline_1);
  }

  g_main_loop_unref (loop);

  return 0;
}
