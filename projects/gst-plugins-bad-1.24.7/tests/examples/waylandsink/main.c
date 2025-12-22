/*
 * Copyright (C) 2014-2015 Collabora Ltd.
 *   @author George Kiagiadakis <george.kiagiadakis@collabora.com>
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
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#else
#error "Wayland is not supported in GTK+"
#endif

#include <gst/video/videooverlay.h>
#include <gst/wayland/wayland.h>


static gboolean live = FALSE;

static GOptionEntry entries[] = {
  {"live", 'l', 0, G_OPTION_ARG_NONE, &live, "Use a live source", NULL},
  {NULL}
};

typedef struct
{
  GtkWidget *app_widget;
  GtkWidget *video_widget;

  GstElement *pipeline;
  GstVideoOverlay *overlay;

  gchar **argv;
  gint current_uri;             /* index for argv */
} DemoApp;

static void
on_about_to_finish (GstElement * playbin, DemoApp * d)
{
  if (d->argv[++d->current_uri] == NULL)
    d->current_uri = 1;

  g_print ("Now playing %s\n", d->argv[d->current_uri]);
  g_object_set (playbin, "uri", d->argv[d->current_uri], NULL);
}

static void
error_cb (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  DemoApp *d = user_data;
  gchar *debug = NULL;
  GError *err = NULL;

  gst_message_parse_error (msg, &err, &debug);

  g_print ("Error: %s\n", err->message);
  g_error_free (err);

  if (debug) {
    g_print ("Debug details: %s\n", debug);
    g_free (debug);
  }

  gst_element_set_state (d->pipeline, GST_STATE_NULL);
}

static GstBusSyncReply
bus_sync_handler (GstBus * bus, GstMessage * message, gpointer user_data)
{
  DemoApp *d = user_data;

  if (gst_is_wl_display_handle_need_context_message (message)) {
    GstContext *context;
    GdkDisplay *display;
    struct wl_display *display_handle;

    display = gtk_widget_get_display (d->video_widget);
    display_handle = gdk_wayland_display_get_wl_display (display);
    context = gst_wl_display_handle_context_new (display_handle);
    gst_element_set_context (GST_ELEMENT (GST_MESSAGE_SRC (message)), context);
    gst_context_unref (context);

    goto drop;
  } else if (gst_is_video_overlay_prepare_window_handle_message (message)) {
    GtkAllocation allocation;
    GdkWindow *window;
    struct wl_surface *window_handle;

    /* GST_MESSAGE_SRC (message) will be the overlay object that we have to
     * use. This may be waylandsink, but it may also be playbin. In the latter
     * case, we must make sure to use playbin instead of waylandsink, because
     * playbin resets the window handle and render_rectangle after restarting
     * playback and the actual window size is lost */
    d->overlay = GST_VIDEO_OVERLAY (GST_MESSAGE_SRC (message));

    gtk_widget_get_allocation (d->video_widget, &allocation);
    window = gtk_widget_get_window (d->video_widget);
    window_handle = gdk_wayland_window_get_wl_surface (window);

    g_print ("setting window handle and size (%d x %d)\n",
        allocation.width, allocation.height);

    gst_video_overlay_set_window_handle (d->overlay, (guintptr) window_handle);
    gst_video_overlay_set_render_rectangle (d->overlay, allocation.x,
        allocation.y, allocation.width, allocation.height);

    goto drop;
  }

  return GST_BUS_PASS;

drop:
  gst_message_unref (message);
  return GST_BUS_DROP;
}

/* We use the "draw" callback to change the size of the sink
 * because the "configure-event" is only sent to top-level widgets. */
static gboolean
video_widget_draw_cb (GtkWidget * widget, cairo_t * cr, gpointer user_data)
{
  DemoApp *d = user_data;
  GtkAllocation allocation;

  gtk_widget_get_allocation (widget, &allocation);

  g_print ("draw_cb x %d, y %d, w %d, h %d\n",
      allocation.x, allocation.y, allocation.width, allocation.height);

  if (d->overlay) {
    gst_video_overlay_set_render_rectangle (d->overlay, allocation.x,
        allocation.y, allocation.width, allocation.height);
  }

  /* There is no need to call gst_video_overlay_expose().
   * The wayland compositor can always re-draw the window
   * based on its last contents if necessary */

  return FALSE;
}

static void
playing_clicked_cb (GtkButton * button, DemoApp * d)
{
  gst_element_set_state (d->pipeline, GST_STATE_PLAYING);
}

static void
paused_clicked_cb (GtkButton * button, DemoApp * d)
{
  gst_element_set_state (d->pipeline, GST_STATE_PAUSED);
}

static void
ready_clicked_cb (GtkButton * button, DemoApp * d)
{
  gst_element_set_state (d->pipeline, GST_STATE_READY);
}

static void
null_clicked_cb (GtkButton * button, DemoApp * d)
{
  gst_element_set_state (d->pipeline, GST_STATE_NULL);
}

static void
build_window (DemoApp * d)
{
  GtkBuilder *builder;
  GtkWidget *button;
  GError *error = NULL;

  builder = gtk_builder_new ();
  if (!gtk_builder_add_from_file (builder, "window.ui", &error)) {
    g_error ("Failed to load window.ui: %s", error->message);
    g_error_free (error);
    goto exit;
  }

  d->app_widget = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
  g_object_ref (d->app_widget);
  g_signal_connect (d->app_widget, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  d->video_widget = GTK_WIDGET (gtk_builder_get_object (builder, "videoarea"));
  g_signal_connect (d->video_widget, "draw",
      G_CALLBACK (video_widget_draw_cb), d);

  button = GTK_WIDGET (gtk_builder_get_object (builder, "button_playing"));
  g_signal_connect (button, "clicked", G_CALLBACK (playing_clicked_cb), d);

  button = GTK_WIDGET (gtk_builder_get_object (builder, "button_paused"));
  g_signal_connect (button, "clicked", G_CALLBACK (paused_clicked_cb), d);

  button = GTK_WIDGET (gtk_builder_get_object (builder, "button_ready"));
  g_signal_connect (button, "clicked", G_CALLBACK (ready_clicked_cb), d);

  button = GTK_WIDGET (gtk_builder_get_object (builder, "button_null"));
  g_signal_connect (button, "clicked", G_CALLBACK (null_clicked_cb), d);

  gtk_widget_show_all (d->app_widget);

exit:
  g_object_unref (builder);
}

int
main (int argc, char **argv)
{
  DemoApp *d;
  GOptionContext *context;
  GstBus *bus;
  GError *error = NULL;

  gtk_init (&argc, &argv);
  gst_init (&argc, &argv);

  context = g_option_context_new ("- waylandsink gtk demo");
  g_option_context_add_main_entries (context, entries, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error)) {
    g_printerr ("option parsing failed: %s\n", error->message);
    return 1;
  }

  d = g_slice_new0 (DemoApp);
  build_window (d);

  if (argc > 1) {
    d->argv = argv;
    d->current_uri = 1;

    d->pipeline = gst_parse_launch ("playbin video-sink=waylandsink", NULL);
    g_object_set (d->pipeline, "uri", argv[d->current_uri], NULL);

    /* enable looping */
    g_signal_connect (d->pipeline, "about-to-finish",
        G_CALLBACK (on_about_to_finish), d);
  } else {
    if (live) {
      d->pipeline = gst_parse_launch ("videotestsrc pattern=18 "
          "background-color=0xFF0062FF is-live=true ! waylandsink", NULL);
    } else {
      d->pipeline = gst_parse_launch ("videotestsrc pattern=18 "
          "background-color=0xFF0062FF ! waylandsink", NULL);
    }
  }

  bus = gst_pipeline_get_bus (GST_PIPELINE (d->pipeline));
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message::error", G_CALLBACK (error_cb), d);
  gst_bus_set_sync_handler (bus, bus_sync_handler, d, NULL);
  gst_object_unref (bus);

  gst_element_set_state (d->pipeline, GST_STATE_PLAYING);

  gtk_main ();

  gst_element_set_state (d->pipeline, GST_STATE_NULL);
  gst_object_unref (d->pipeline);
  g_object_unref (d->app_widget);
  g_slice_free (DemoApp, d);

  return 0;
}
