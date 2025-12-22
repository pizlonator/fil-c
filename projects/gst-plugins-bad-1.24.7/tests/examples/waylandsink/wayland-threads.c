/*
 * Copyright (C) 2018 LG Electronics
 *   @author Wonchul Lee <w.lee@lge.com>
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

#include <gst/gst.h>

#include <gst/video/videooverlay.h>
#include <gst/wayland/wayland.h>

static gint retry = 100;

typedef struct
{
  struct wl_display *display;
  struct wl_display *display_wrapper;
  struct wl_registry *registry;
  struct wl_compositor *compositor;
  struct wl_event_queue *queue;

  GThread *thread;

  GstElement *pipeline1;
  GstElement *pipeline2;
  GstVideoOverlay *overlay;
  GMainLoop *loop;
} App;

static gboolean
message_cb (GstBus * bus, GstMessage * message, gpointer user_data)
{
  App *app = user_data;
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:{
      GError *err = NULL;
      gchar *debug = NULL;

      gst_message_parse_error (message, &err, &debug);
      gst_printerrln ("Error message received: %s", err->message);
      gst_printerrln ("Debug info: %s", debug);
      g_error_free (err);
      g_free (debug);
    }
    case GST_MESSAGE_EOS:
      if (retry <= 0)
        g_main_loop_quit (app->loop);
      gst_element_set_state (GST_ELEMENT (GST_MESSAGE_SRC (message)),
          GST_STATE_NULL);
      gst_element_set_state (GST_ELEMENT (GST_MESSAGE_SRC (message)),
          GST_STATE_PLAYING);
      retry--;
      break;
    default:
      break;
  }
  return TRUE;
}

static GstBusSyncReply
bus_sync_handler (GstBus * bus, GstMessage * message, gpointer user_data)
{
  App *app = user_data;

  if (gst_is_wl_display_handle_need_context_message (message)) {
    GstContext *context;
    context = gst_wl_display_handle_context_new (app->display);
    gst_element_set_context (GST_ELEMENT (GST_MESSAGE_SRC (message)), context);
    gst_context_unref (context);

    goto drop;
  }
  return GST_BUS_PASS;

drop:
  gst_message_unref (message);
  return GST_BUS_DROP;
}

static void
registry_handle (void *data, struct wl_registry *registry,
    uint32_t id, const char *interface, uint32_t version)
{
  App *app = data;

  if (g_strcmp0 (interface, "wl_compositor") == 0) {
    app->compositor =
        wl_registry_bind (app->registry, id, &wl_compositor_interface,
        MIN (version, 3));
  }
}

static const struct wl_registry_listener registry_listener = {
  registry_handle
};

static gpointer
wl_main_thread_run (gpointer data)
{
  App *app = data;
  while (wl_display_dispatch_queue (app->display, app->queue) != -1)
    return NULL;

  return NULL;
}

static GstElement *
build_pipeline (App * app, gint num_buffers)
{
  GstElement *pipeline;
  GstBus *bus;
  gchar *str;

  str =
      g_strdup_printf ("videotestsrc num-buffers=%d ! waylandsink",
      num_buffers);

  pipeline = gst_parse_launch (str, NULL);
  g_free (str);
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (message_cb), app);
  gst_bus_set_sync_handler (bus, bus_sync_handler, app, NULL);
  gst_object_unref (bus);

  return pipeline;
}

int
main (int argc, char **argv)
{
  App *app;
  GError *error = NULL;

  gst_init (&argc, &argv);

  app = g_slice_new0 (App);

  app->loop = g_main_loop_new (NULL, FALSE);

  app->display = wl_display_connect (NULL);
  if (!app->display)
    goto done;
  app->display_wrapper = wl_proxy_create_wrapper (app->display);
  app->queue = wl_display_create_queue (app->display);
  wl_proxy_set_queue ((struct wl_proxy *) app->display_wrapper, app->queue);
  app->registry = wl_display_get_registry (app->display_wrapper);
  wl_registry_add_listener (app->registry, &registry_listener, app);

  wl_display_roundtrip_queue (app->display, app->queue);
  wl_display_roundtrip_queue (app->display, app->queue);

  if (!app->compositor) {
    g_set_error (&error, g_quark_from_static_string ("waylandMultiThreads"), 0,
        "Could not bind to wl_compositor interface");
    goto done;
  }

  app->thread =
      g_thread_try_new ("WlMainThread", wl_main_thread_run, app, &error);
  if (error) {
    gst_printerrln ("error: %s", error->message);
    g_error_free (error);
    goto done;
  }

  app->pipeline1 = build_pipeline (app, 30);
  app->pipeline2 = build_pipeline (app, 40);

  gst_element_set_state (app->pipeline1, GST_STATE_PLAYING);
  gst_element_set_state (app->pipeline2, GST_STATE_PLAYING);

  g_main_loop_run (app->loop);

  gst_element_set_state (app->pipeline1, GST_STATE_NULL);
  gst_element_set_state (app->pipeline2, GST_STATE_NULL);

  gst_object_unref (app->pipeline1);
  gst_object_unref (app->pipeline2);

done:
  if (app->thread)
    g_thread_join (app->thread);

  if (app->compositor)
    wl_compositor_destroy (app->compositor);
  if (app->registry)
    wl_registry_destroy (app->registry);
  if (app->queue)
    wl_event_queue_destroy (app->queue);
  if (app->display_wrapper)
    wl_proxy_wrapper_destroy (app->display_wrapper);
  if (app->display) {
    wl_display_flush (app->display);
    wl_display_disconnect (app->display);
  }
  g_slice_free (App, app);
  return 0;
}
