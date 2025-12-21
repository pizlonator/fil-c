/*
 * GStreamer
 * Copyright (C) 2008-2009 Filippo Argiolas <filippo.argiolas@gmail.com>
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

#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include <gst/gst.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "../gstgtk.h"

#include <gst/video/videooverlay.h>

#ifdef HAVE_X11
#include <X11/Xlib.h>
#endif

static gint delay = 0;
static gint saveddelay = 0;
static gint method = 1;

struct _SourceData
{
  gpointer data;
  gpointer nick;
  gpointer value;
};
typedef struct _SourceData SourceData;

static GstBusSyncReply
create_window (GstBus * bus, GstMessage * message, GtkWidget * widget)
{
  // ignore anything but 'prepare-window-handle' element messages
  if (GST_MESSAGE_TYPE (message) != GST_MESSAGE_ELEMENT)
    return GST_BUS_PASS;

  if (!gst_is_video_overlay_prepare_window_handle_message (message))
    return GST_BUS_PASS;

  gst_video_overlay_set_gtk_window (GST_VIDEO_OVERLAY (GST_MESSAGE_SRC
          (message)), widget);

  gst_message_unref (message);

  return GST_BUS_DROP;
}

static void
message_cb (GstBus * bus, GstMessage * message, GstElement * pipeline)
{
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);

  gtk_main_quit ();
}

static gboolean
expose_cb (GtkWidget * widget, cairo_t * cr, GstElement * videosink)
{
  gst_video_overlay_expose (GST_VIDEO_OVERLAY (videosink));
  return FALSE;
}

static void
destroy_cb (GtkWidget * widget, GdkEvent * event, GstElement * pipeline)
{
  g_message ("destroy callback");

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);

  gtk_main_quit ();
}

static gboolean
play_cb (GtkWidget * widget, gpointer data)
{
  g_message ("playing");
  gst_element_set_state (GST_ELEMENT (data), GST_STATE_PLAYING);
  return FALSE;
}

static gboolean
null_cb (GtkWidget * widget, gpointer data)
{
  g_message ("nulling");
  gst_element_set_state (GST_ELEMENT (data), GST_STATE_NULL);
  return FALSE;
}

static gboolean
ready_cb (GtkWidget * widget, gpointer data)
{
  g_message ("readying");
  gst_element_set_state (GST_ELEMENT (data), GST_STATE_READY);
  return FALSE;
}

static gboolean
pause_cb (GtkWidget * widget, gpointer data)
{
  g_message ("pausing");
  gst_element_set_state (GST_ELEMENT (data), GST_STATE_PAUSED);
  return FALSE;
}

static gboolean
set_location_delayed (gpointer data)
{
  SourceData *sdata = (SourceData *) data;
  delay--;
  g_print ("%d\n", delay);
  if (delay > 0) {
    return TRUE;
  }
  g_object_set (G_OBJECT (sdata->data), sdata->nick, sdata->value, NULL);
  delay = saveddelay;
  return FALSE;
}

static void
on_drag_data_received (GtkWidget * widget,
    GdkDragContext * context, int x, int y,
    GtkSelectionData * seldata, guint inf, guint time, gpointer data)
{
  SourceData *userdata = g_new0 (SourceData, 1);
  GdkPixbufFormat *format;
  gchar **uris = gtk_selection_data_get_uris (seldata);
  gchar *filename = NULL;

  g_return_if_fail (uris != NULL);
  filename = g_filename_from_uri (uris[0], NULL, NULL);
  g_return_if_fail (filename != NULL);
  format = gdk_pixbuf_get_file_info (filename, NULL, NULL);
  g_return_if_fail (format);
  g_print ("received %s image: %s\n", filename,
      gdk_pixbuf_format_get_name (format));

  userdata->nick = (gchar *) "location";
  userdata->value = g_strdup (filename);
  userdata->data = data;
  saveddelay = delay;
  if (delay > 0) {
    g_print ("%d\n", delay);
    g_timeout_add_seconds (1, set_location_delayed, userdata);
  } else
    g_object_set (G_OBJECT (userdata->data), userdata->nick, userdata->value,
        NULL);
  g_free (filename);
}


gint
main (gint argc, gchar * argv[])
{
  GstElement *pipeline;
  GstElement *filter, *sink;
  GstElement *sourcebin;
  GstBus *bus;
  GError *error = NULL;

  GtkWidget *window;
  GtkWidget *screen;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *play, *pause, *null, *ready;

  gchar **source_desc_array = NULL;
  gchar *source_desc = NULL;

  GOptionContext *context;
  GOptionEntry options[] = {
    {"source-bin", 's', 0, G_OPTION_ARG_STRING_ARRAY, &source_desc_array,
        "Use a custom source bin description (gst-launch style)", NULL}
    ,
    {"method", 'm', 0, G_OPTION_ARG_INT, &method,
        "1 for gstdifferencematte, 2 for gloverlay", "M"}
    ,
    {"delay", 'd', 0, G_OPTION_ARG_INT, &delay,
          "Wait N seconds before to send the image to gstreamer (useful with differencematte)",
        "N"}
    ,
    {NULL}
  };

#ifdef HAVE_X11
  XInitThreads ();
#endif

  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, options, NULL);
  g_option_context_add_group (context, gst_init_get_option_group ());
  g_option_context_add_group (context, gtk_get_option_group (TRUE));
  if (!g_option_context_parse (context, &argc, &argv, &error)) {
    g_print ("Inizialization error: %s\n", GST_STR_NULL (error->message));
    g_option_context_free (context);
    g_clear_error (&error);
    return -1;
  }
  g_option_context_free (context);

  if (source_desc_array != NULL) {
    source_desc = g_strjoinv (" ", source_desc_array);
    g_strfreev (source_desc_array);
  }
  if (source_desc == NULL) {
    source_desc =
        g_strdup
        ("videotestsrc ! video/x-raw, width=352, height=288 ! identity ! glupload");
  }

  sourcebin =
      gst_parse_bin_from_description (g_strdup (source_desc), TRUE, &error);
  g_free (source_desc);
  if (error) {
    g_print ("Error while parsing source bin description: %s\n",
        GST_STR_NULL (error->message));
    return -1;
  }

  g_set_application_name ("gst-gl-effects test app");

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_container_set_border_width (GTK_CONTAINER (window), 3);

  pipeline = gst_pipeline_new ("pipeline");

  if (method == 2) {
    filter = gst_element_factory_make ("gloverlay", "flt");
  } else {
    filter = gst_element_factory_make ("gldifferencematte", "flt");
  }
  sink = gst_element_factory_make ("glimagesink", "glsink");

  gst_bin_add_many (GST_BIN (pipeline), sourcebin, filter, sink, NULL);

  if (!gst_element_link_many (sourcebin, filter, sink, NULL)) {
    g_print ("Failed to link one or more elements!\n");
    return -1;
  }

  g_signal_connect (G_OBJECT (window), "delete-event",
      G_CALLBACK (destroy_cb), pipeline);
  g_signal_connect (G_OBJECT (window), "destroy-event",
      G_CALLBACK (destroy_cb), pipeline);

  screen = gtk_drawing_area_new ();

  gtk_widget_set_size_request (screen, 640, 480);       // 500 x 376

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

  gtk_box_pack_start (GTK_BOX (vbox), screen, TRUE, TRUE, 0);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  play = gtk_button_new_with_label ("PLAY");

  g_signal_connect (G_OBJECT (play), "clicked", G_CALLBACK (play_cb), pipeline);

  pause = gtk_button_new_with_label ("PAUSE");

  g_signal_connect (G_OBJECT (pause), "clicked",
      G_CALLBACK (pause_cb), pipeline);

  null = gtk_button_new_with_label ("NULL");

  g_signal_connect (G_OBJECT (null), "clicked", G_CALLBACK (null_cb), pipeline);

  ready = gtk_button_new_with_label ("READY");

  g_signal_connect (G_OBJECT (ready), "clicked",
      G_CALLBACK (ready_cb), pipeline);

  gtk_box_pack_start (GTK_BOX (hbox), null, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), ready, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), play, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), pause, TRUE, TRUE, 0);

  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  gtk_container_add (GTK_CONTAINER (window), vbox);

  gtk_widget_realize (screen);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_set_sync_handler (bus, (GstBusSyncHandler) create_window, screen,
      NULL);
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message::error", G_CALLBACK (message_cb), pipeline);
  g_signal_connect (bus, "message::warning", G_CALLBACK (message_cb), pipeline);
  g_signal_connect (bus, "message::eos", G_CALLBACK (message_cb), pipeline);
  gst_object_unref (bus);
  g_signal_connect (screen, "draw", G_CALLBACK (expose_cb), sink);

  gtk_drag_dest_set (screen, GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY);
  gtk_drag_dest_add_uri_targets (screen);

  g_signal_connect (screen, "drag-data-received",
      G_CALLBACK (on_drag_data_received), filter);

  gtk_widget_show_all (GTK_WIDGET (window));

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  gtk_main ();

  return 0;
}
