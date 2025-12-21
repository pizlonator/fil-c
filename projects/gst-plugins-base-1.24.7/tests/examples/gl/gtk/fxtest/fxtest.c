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


static GstBusSyncReply
create_window (GstBus * bus, GstMessage * message, GtkWidget * widget)
{
  if (gst_gtk_handle_need_context (bus, message, NULL))
    return GST_BUS_DROP;

  /* ignore anything but 'prepare-window-handle' element messages */
  if (GST_MESSAGE_TYPE (message) != GST_MESSAGE_ELEMENT)
    return GST_BUS_PASS;

  if (!gst_is_video_overlay_prepare_window_handle_message (message))
    return GST_BUS_PASS;

  g_print ("setting window handle\n");

  /* do not call gdk_window_ensure_native for the first time here because
   * we are in a different thread than the main thread
   * (and the main thread the one) */
  gst_video_overlay_set_gtk_window (GST_VIDEO_OVERLAY (GST_MESSAGE_SRC
          (message)), widget);

  gst_message_unref (message);

  return GST_BUS_DROP;
}


static void
end_stream_cb (GstBus * bus, GstMessage * message, GstElement * pipeline)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_EOS:
      g_print ("End of stream\n");

      gst_element_set_state (pipeline, GST_STATE_NULL);
      gst_object_unref (pipeline);
      gtk_main_quit ();
      break;
    case GST_MESSAGE_ERROR:
    {
      gchar *debug = NULL;
      GError *err = NULL;

      gst_message_parse_error (message, &err, &debug);

      g_print ("Error: %s\n", err->message);
      g_clear_error (&err);

      if (debug) {
        g_print ("Debug details: %s\n", debug);
        g_free (debug);
      }

      gst_element_set_state (pipeline, GST_STATE_NULL);
      gst_object_unref (pipeline);
      gtk_main_quit ();
      break;
    }
    default:
      break;
  }
}

static gboolean
resize_cb (GtkWidget * widget, GdkEvent * event, gpointer data)
{
  GtkAllocation allocation;
  GstVideoOverlay *overlay =
      GST_VIDEO_OVERLAY (gst_bin_get_by_interface (GST_BIN (data),
          GST_TYPE_VIDEO_OVERLAY));

  gtk_widget_get_allocation (widget, &allocation);
  gst_video_overlay_set_render_rectangle (overlay, allocation.x, allocation.y,
      allocation.width, allocation.height);
  gst_object_unref (overlay);

  return G_SOURCE_CONTINUE;
}

static gboolean
expose_cb (GtkWidget * widget, gpointer unused, gpointer data)
{
  GstVideoOverlay *overlay =
      GST_VIDEO_OVERLAY (gst_bin_get_by_interface (GST_BIN (data),
          GST_TYPE_VIDEO_OVERLAY));

  gst_video_overlay_expose (overlay);
  gst_object_unref (overlay);

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
apply_fx (GtkWidget * widget, gpointer data)
{
  gchar *fx;
  GEnumClass *p_class;

/* heeeellppppp!! */
  p_class =
      G_PARAM_SPEC_ENUM (g_object_class_find_property (G_OBJECT_GET_CLASS
          (G_OBJECT (data)), "effect")
      )->enum_class;

  fx = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (widget));
  g_print ("setting: %s - %s\n", fx, g_enum_get_value_by_nick (p_class,
          fx)->value_name);
  g_object_set (G_OBJECT (data), "effect", g_enum_get_value_by_nick (p_class,
          fx)->value, NULL);
  return FALSE;
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

gint
main (gint argc, gchar * argv[])
{
  GstStateChangeReturn ret;
  GstElement *pipeline;
  GstElement *upload, *filter, *sink;
  GstElement *sourcebin;
  GstBus *bus;
  GError *error = NULL;

  GtkWidget *window;
  GtkWidget *screen;
  GtkWidget *vbox, *combo;
  GtkWidget *hbox;
  GtkWidget *play, *pause, *null, *ready;

  gchar **source_desc_array = NULL;
  gchar *source_desc = NULL;

  GOptionContext *context;
  GOptionEntry options[] = {
    {"source-bin", 's', 0, G_OPTION_ARG_STRING_ARRAY, &source_desc_array,
        "Use a custom source bin description (gst-launch style)", NULL}
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
        ("videotestsrc ! video/x-raw, width=352, height=288 ! identity");
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

  upload = gst_element_factory_make ("glupload", NULL);
  filter = gst_element_factory_make ("gleffects", "flt");
  sink = gst_element_factory_make ("glimagesink", "glsink");

  gst_bin_add_many (GST_BIN (pipeline), sourcebin, upload, filter, sink, NULL);

  if (!gst_element_link_many (sourcebin, upload, filter, sink, NULL)) {
    g_print ("Failed to link one or more elements!\n");
    return -1;
  }

  g_signal_connect (G_OBJECT (window), "delete-event",
      G_CALLBACK (destroy_cb), pipeline);
  g_signal_connect (G_OBJECT (window), "destroy-event",
      G_CALLBACK (destroy_cb), pipeline);

  screen = gtk_drawing_area_new ();
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message::error", G_CALLBACK (end_stream_cb),
      pipeline);
  g_signal_connect (bus, "message::warning", G_CALLBACK (end_stream_cb),
      pipeline);
  g_signal_connect (bus, "message::eos", G_CALLBACK (end_stream_cb), pipeline);

  gst_bus_set_sync_handler (bus, (GstBusSyncHandler) create_window, screen,
      NULL);
  gst_object_unref (bus);

  gtk_widget_set_size_request (screen, 640, 480);       // 500 x 376

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

  gtk_box_pack_start (GTK_BOX (vbox), screen, TRUE, TRUE, 0);

  combo = gtk_combo_box_text_new ();

  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "identity");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "mirror");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "squeeze");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "stretch");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "fisheye");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "twirl");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "bulge");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "tunnel");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "square");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "heat");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "xpro");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "lumaxpro");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "sepia");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "xray");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "sin");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "glow");

  g_signal_connect (G_OBJECT (combo), "changed", G_CALLBACK (apply_fx), filter);

  gtk_box_pack_start (GTK_BOX (vbox), combo, FALSE, FALSE, 0);

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

  g_signal_connect (screen, "draw", G_CALLBACK (expose_cb), pipeline);
  g_signal_connect (screen, "configure-event", G_CALLBACK (resize_cb),
      pipeline);
  gtk_widget_realize (screen);

  ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_print ("Failed to start up pipeline!\n");
    return -1;
  }

  gtk_widget_show_all (GTK_WIDGET (window));

  gtk_main ();

  return 0;
}
