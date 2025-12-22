/*
 * Copyright (C) 2014-2021 Collabora Ltd.
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

static gboolean live = FALSE;
static gboolean scrollable = FALSE;

static GOptionEntry entries[] = {
  {"live", 'l', 0, G_OPTION_ARG_NONE, &live, "Use a live source", NULL},
  {"scrollable", 's', 0, G_OPTION_ARG_NONE, &scrollable,
      "Put the GtkWaylandSink into a GtkScrolledWindow ", NULL},
  {NULL}
};

typedef struct
{
  GtkWidget *app_widget;

  GstElement *pipeline;

  gchar **argv;
  gint current_uri;             /* index for argv */

  gboolean is_fullscreen;
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

static gboolean
video_widget_button_pressed_cb (GtkWidget * widget,
    GdkEventButton * eventButton, DemoApp * d)
{
  if (eventButton->type == GDK_2BUTTON_PRESS) {
    if (d->is_fullscreen) {
      gtk_window_unfullscreen (GTK_WINDOW (d->app_widget));
      d->is_fullscreen = FALSE;
    } else {
      gtk_window_fullscreen (GTK_WINDOW (d->app_widget));
      d->is_fullscreen = TRUE;
    }
  }

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
  GstElement *sink;
  GtkWidget *box;
  GtkWidget *widget;
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

  box = GTK_WIDGET (gtk_builder_get_object (builder, "box"));
  sink = gst_bin_get_by_name (GST_BIN (d->pipeline), "vsink");
  if (!sink && !g_strcmp0 (G_OBJECT_TYPE_NAME (d->pipeline), "GstPlayBin")) {
    g_object_get (d->pipeline, "video-sink", &sink, NULL);
    if (sink && g_strcmp0 (G_OBJECT_TYPE_NAME (sink), "GstGtkWaylandSink") != 0
        && GST_IS_BIN (sink)) {
      GstBin *sinkbin = GST_BIN (sink);
      sink = gst_bin_get_by_name (sinkbin, "vsink");
      gst_object_unref (sinkbin);
    }
  }
  g_assert (sink);

  g_object_get (sink, "widget", &widget, NULL);
  if (scrollable) {
    GtkWidget *scrollable;
    scrollable = gtk_scrolled_window_new (NULL, NULL);

    gtk_container_add (GTK_CONTAINER (scrollable), widget);
    g_object_unref (widget);
    widget = scrollable;
  }

  gtk_box_pack_start (GTK_BOX (box), widget, TRUE, TRUE, 0);
  gtk_box_reorder_child (GTK_BOX (box), widget, 0);

  g_signal_connect (widget, "button-press-event",
      G_CALLBACK (video_widget_button_pressed_cb), d);
  if (!scrollable)
    g_object_unref (widget);
  g_object_unref (sink);

  widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_playing"));
  g_signal_connect (widget, "clicked", G_CALLBACK (playing_clicked_cb), d);

  widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_paused"));
  g_signal_connect (widget, "clicked", G_CALLBACK (paused_clicked_cb), d);

  widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_ready"));
  g_signal_connect (widget, "clicked", G_CALLBACK (ready_clicked_cb), d);

  widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_null"));
  g_signal_connect (widget, "clicked", G_CALLBACK (null_clicked_cb), d);

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

  context = g_option_context_new ("- gtkwaylandsink demo");
  g_option_context_add_main_entries (context, entries, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error)) {
    g_printerr ("option parsing failed: %s\n", error->message);
    return 1;
  }

  d = g_slice_new0 (DemoApp);

  if (argc > 1) {
    d->argv = argv;
    d->current_uri = 1;

    d->pipeline =
        gst_parse_launch ("playbin video-sink=\"gtkwaylandsink name=vsink\"",
        NULL);
    g_object_set (d->pipeline, "uri", argv[d->current_uri], NULL);

    /* enable looping */
    g_signal_connect (d->pipeline, "about-to-finish",
        G_CALLBACK (on_about_to_finish), d);
  } else {
    if (live) {
      d->pipeline = gst_parse_launch ("videotestsrc pattern=18 "
          "background-color=0xFF0062FF is-live=true ! navigationtest ! "
          "videoconvert ! gtkwaylandsink name=vsink", NULL);
    } else {
      d->pipeline = gst_parse_launch ("videotestsrc pattern=18 "
          "background-color=0xFF0062FF ! navigationtest ! videoconvert ! "
          "gtkwaylandsink name=vsink", NULL);
    }
  }

  build_window (d);

  bus = gst_pipeline_get_bus (GST_PIPELINE (d->pipeline));
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message::error", G_CALLBACK (error_cb), d);
  gst_object_unref (bus);

  gst_element_set_state (d->pipeline, GST_STATE_PLAYING);

  gtk_main ();

  gst_element_set_state (d->pipeline, GST_STATE_NULL);
  gst_object_unref (d->pipeline);
  g_object_unref (d->app_widget);
  g_slice_free (DemoApp, d);

  return 0;
}
