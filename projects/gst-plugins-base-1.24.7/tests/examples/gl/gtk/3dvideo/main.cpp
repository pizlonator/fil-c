/*
 * GStreamer
 * Copyright (C) 2008-2009 Julien Isorce <julien.isorce@gmail.com>
 * Copyright (C) 2014-2015 Jan Schmidt <jan@centricular.com>
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

#include <string.h>

#include <gdk/gdk.h>
#if defined (GDK_WINDOWING_X11)
#include <X11/Xlib.h>
#endif

#include <gst/gst.h>
#include <gtk/gtk.h>
#include <gst/video/video-info.h>

#include "../gstgtk.h"
#include "mviewwidget.h"

/* Until playbin properties support dynamic changes,
 * use our own glviewconvert */
#define USE_GLCONVERT_FOR_INPUT 1

typedef struct _localstate
{
  GstVideoMultiviewFramePacking in_mode;
  GstVideoMultiviewFlags out_mode;
  GstVideoMultiviewFlags in_flags, out_flags;
} LocalState;

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

  /* do not call gdk_window_ensure_native for the first time here because
   * we are in a different thread than the main thread */
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
      g_error_free (err);

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
draw_cb (GtkWidget * widget, cairo_t * cr, GstElement * videosink)
{
  gst_video_overlay_expose (GST_VIDEO_OVERLAY (videosink));
  return FALSE;
}

static gboolean
resize_cb (GtkWidget * widget, GdkEvent * event, gpointer sink)
{
  GtkAllocation allocation;
  gint scale = 1;

#if GTK_CHECK_VERSION(3, 10, 0)
  scale = gtk_widget_get_scale_factor (widget);
#endif

  gtk_widget_get_allocation (widget, &allocation);
  gst_video_overlay_set_render_rectangle (GST_VIDEO_OVERLAY (sink),
      allocation.x * scale, allocation.y * scale, allocation.width * scale,
      allocation.height * scale);

  return G_SOURCE_CONTINUE;
}

static void
destroy_cb (GtkWidget * widget, GdkEvent * event, GstElement * pipeline)
{
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);

  gtk_main_quit ();
}

static void
button_state_ready_cb (GtkWidget * widget, GstElement * pipeline)
{
  gst_element_set_state (pipeline, GST_STATE_READY);
}

static void
button_state_paused_cb (GtkWidget * widget, GstElement * pipeline)
{
  gst_element_set_state (pipeline, GST_STATE_PAUSED);
}

static void
button_state_playing_cb (GtkWidget * widget, GstElement * pipeline)
{
  gst_element_set_state (pipeline, GST_STATE_PLAYING);
}

static gboolean
set_mview_mode (GtkWidget * combo, GObject * target, const gchar * prop_name)
{
  gchar *mview_mode = NULL;
  GEnumClass *p_class;
  GEnumValue *v;
  GParamSpec *p =
      g_object_class_find_property (G_OBJECT_GET_CLASS (target), prop_name);

  g_return_val_if_fail (p != NULL, FALSE);

  p_class = G_PARAM_SPEC_ENUM (p)->enum_class;
  g_return_val_if_fail (p_class != NULL, FALSE);

  g_object_get (G_OBJECT (combo), "active-id", &mview_mode, NULL);
  g_return_val_if_fail (mview_mode != NULL, FALSE);

  v = g_enum_get_value_by_nick (p_class, mview_mode);
  g_return_val_if_fail (v != NULL, FALSE);

  g_object_set (target, prop_name, v->value, NULL);

  return FALSE;
}

static gboolean
set_mview_input_mode (GtkWidget * widget, gpointer data)
{
#if USE_GLCONVERT_FOR_INPUT
  return set_mview_mode (widget, G_OBJECT (data), "input-mode-override");
#else
  return set_mview_mode (widget, G_OBJECT (data), "video-multiview-mode");
#endif
}

static gboolean
set_mview_output_mode (GtkWidget * widget, gpointer data)
{
  GstElement *sink = gst_bin_get_by_name (GST_BIN (data), "sink");
  set_mview_mode (widget, G_OBJECT (sink), "output-multiview-mode");
  gst_object_unref (GST_OBJECT (sink));
  return FALSE;
}

static void
input_flags_changed (GObject * gobject, GParamSpec * pspec, gpointer user_data)
{
  GObject *target = G_OBJECT (user_data);
  GstVideoMultiviewFlags flags;

  g_object_get (gobject, "flags", &flags, NULL);
#if USE_GLCONVERT_FOR_INPUT
  g_object_set (target, "input-flags-override", flags, NULL);
#else
  g_object_set (target, "video-multiview-flags", flags, NULL);
#endif
}

static void
output_flags_changed (GObject * gobject, GParamSpec * pspec, gpointer user_data)
{
  GObject *target = G_OBJECT (user_data);
  GstVideoMultiviewFlags flags;
  GstElement *sink = gst_bin_get_by_name (GST_BIN (target), "sink");

  g_object_get (gobject, "flags", &flags, NULL);
  g_object_set (G_OBJECT (sink), "output-multiview-flags", flags, NULL);

  gst_object_unref (GST_OBJECT (sink));
}

static void
downmix_method_changed (GObject * gobject, GParamSpec * pspec, gpointer user_data)
{
  GObject *target = G_OBJECT (user_data);
  GstGLStereoDownmix downmix_method;
  GstElement *sink = gst_bin_get_by_name (GST_BIN (target), "sink");

  g_object_get (gobject, "downmix-mode", &downmix_method, NULL);
  g_object_set (sink, "output-multiview-downmix-mode", downmix_method, NULL);
  gst_object_unref (GST_OBJECT (sink));
}

static const gchar *
enum_value_to_nick (GType enum_type, guint value)
{
  GEnumClass *enum_info;
  GEnumValue *v;
  const gchar *nick;

  enum_info = (GEnumClass *) (g_type_class_ref (enum_type));
  g_return_val_if_fail (enum_info != NULL, NULL);

  v = g_enum_get_value (enum_info, value);
  g_return_val_if_fail (v != NULL, NULL);

  nick = v->value_nick;

  g_type_class_unref (enum_info);

  return nick;
}

static void
detect_mode_from_uri (LocalState * state, const gchar * uri)
{
  if (strstr (uri, "HSBS")) {
    state->in_mode = GST_VIDEO_MULTIVIEW_FRAME_PACKING_SIDE_BY_SIDE;
    state->in_flags = GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT;
  } else if (strstr (uri, "SBS")) {
    state->in_mode = GST_VIDEO_MULTIVIEW_FRAME_PACKING_SIDE_BY_SIDE;
    if (g_regex_match_simple ("half", uri, G_REGEX_CASELESS,
            (GRegexMatchFlags) 0)) {
      state->in_flags = GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT;
    }
  }
}

gint
main (gint argc, gchar * argv[])
{
  LocalState state;
  GtkWidget *area, *combo, *w;
  const gchar *uri;

#if defined (GDK_WINDOWING_X11)
  XInitThreads ();
#endif

  gst_init (&argc, &argv);
  gtk_init (&argc, &argv);

  if (argc < 2) {
    g_print ("Usage: 3dvideo <uri-to-play>\n");
    return 1;
  }

  uri = argv[1];

  GstElement *pipeline = gst_element_factory_make ("playbin", NULL);
  GstBin *sinkbin = (GstBin *) gst_parse_bin_from_description ("glupload ! glcolorconvert ! glviewconvert name=viewconvert ! glimagesink name=sink", TRUE, NULL);
#if USE_GLCONVERT_FOR_INPUT
  GstElement *glconvert = gst_bin_get_by_name (sinkbin, "viewconvert");
#endif
  GstElement *videosink = gst_bin_get_by_name (sinkbin, "sink");

  /* Get defaults */
  g_object_get (pipeline, "video-multiview-mode", &state.in_mode,
      "video-multiview-flags", &state.in_flags, NULL);
  gst_child_proxy_get (GST_CHILD_PROXY (videosink), "sink::output-multiview-mode", &state.out_mode,
      "sink::output-multiview-flags", &state.out_flags, NULL);

  detect_mode_from_uri (&state, uri);

  g_return_val_if_fail (pipeline != NULL, 1);
  g_return_val_if_fail (videosink != NULL, 1);

  g_object_set (G_OBJECT (pipeline), "video-sink", sinkbin, NULL);
  g_object_set (G_OBJECT (pipeline), "uri", uri, NULL);

#if USE_GLCONVERT_FOR_INPUT
  g_object_set (G_OBJECT (glconvert), "input-mode-override", state.in_mode,
      NULL);
  g_object_set (G_OBJECT (glconvert), "input-flags-override", state.in_flags,
      NULL);
#else
  g_object_set (G_OBJECT (pipeline), "video-multiview-mode", state.in_mode,
      NULL);
  g_object_set (G_OBJECT (pipeline), "video-multiview-flags", state.in_flags,
      NULL);
#endif

  /* Connect to bus for signal handling */
  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message::error", G_CALLBACK (end_stream_cb),
      pipeline);
  g_signal_connect (bus, "message::warning", G_CALLBACK (end_stream_cb),
      pipeline);
  g_signal_connect (bus, "message::eos", G_CALLBACK (end_stream_cb), pipeline);

  gst_element_set_state (pipeline, GST_STATE_READY);

  area = gtk_drawing_area_new ();
  gst_bus_set_sync_handler (bus, (GstBusSyncHandler) create_window, area, NULL);
  gst_object_unref (bus);

  /* Toplevel window */
  GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);
  gtk_window_set_title (GTK_WINDOW (window), "Stereoscopic video demo");
  GdkGeometry geometry;
  geometry.min_width = 1;
  geometry.min_height = 1;
  geometry.max_width = -1;
  geometry.max_height = -1;
  gtk_window_set_geometry_hints (GTK_WINDOW (window), window, &geometry,
      GDK_HINT_MIN_SIZE);

  GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  /* area where the video is drawn */
  gtk_box_pack_start (GTK_BOX (vbox), area, TRUE, TRUE, 0);

  /* Buttons to control the pipeline state */
  GtkWidget *table = gtk_grid_new ();
  gtk_container_add (GTK_CONTAINER (vbox), table);

  GtkWidget *button_state_ready = gtk_button_new_with_label ("Stop");
  g_signal_connect (G_OBJECT (button_state_ready), "clicked",
      G_CALLBACK (button_state_ready_cb), pipeline);
  gtk_grid_attach (GTK_GRID (table), button_state_ready, 1, 0, 1, 1);
  gtk_widget_show (button_state_ready);

  //control state paused
  GtkWidget *button_state_paused = gtk_button_new_with_label ("Pause");
  g_signal_connect (G_OBJECT (button_state_paused), "clicked",
      G_CALLBACK (button_state_paused_cb), pipeline);
  gtk_grid_attach (GTK_GRID (table), button_state_paused, 2, 0, 1, 1);
  gtk_widget_show (button_state_paused);

  //control state playing
  GtkWidget *button_state_playing = gtk_button_new_with_label ("Play");
  g_signal_connect (G_OBJECT (button_state_playing), "clicked",
      G_CALLBACK (button_state_playing_cb), pipeline);
  gtk_grid_attach (GTK_GRID (table), button_state_playing, 3, 0, 1, 1);
  //gtk_widget_show (button_state_playing);

  w = gst_mview_widget_new (FALSE);
  combo = GST_MVIEW_WIDGET (w)->mode_selector;
  gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo),
      enum_value_to_nick (GST_TYPE_VIDEO_MULTIVIEW_FRAME_PACKING,
          state.in_mode));
#if USE_GLCONVERT_FOR_INPUT
  g_signal_connect (G_OBJECT (combo), "changed",
      G_CALLBACK (set_mview_input_mode), glconvert);
#else
  g_signal_connect (G_OBJECT (combo), "changed",
      G_CALLBACK (set_mview_input_mode), pipeline);
#endif

  g_object_set (G_OBJECT (w), "flags", state.in_flags, NULL);
#if USE_GLCONVERT_FOR_INPUT
  g_signal_connect (G_OBJECT (w), "notify::flags",
      G_CALLBACK (input_flags_changed), glconvert);
#else
  g_signal_connect (G_OBJECT (w), "notify::flags",
      G_CALLBACK (input_flags_changed), pipeline);
#endif
  gtk_container_add (GTK_CONTAINER (vbox), w);

  w = gst_mview_widget_new (TRUE);
  combo = GST_MVIEW_WIDGET (w)->mode_selector;
  gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo),
      enum_value_to_nick (GST_TYPE_VIDEO_MULTIVIEW_MODE, state.out_mode));
  g_signal_connect (G_OBJECT (combo), "changed",
      G_CALLBACK (set_mview_output_mode), videosink);

  g_object_set (G_OBJECT (w), "flags", state.out_flags, NULL);
  g_signal_connect (G_OBJECT (w), "notify::flags",
      G_CALLBACK (output_flags_changed), videosink);
  g_signal_connect (G_OBJECT (w), "notify::downmix-mode",
      G_CALLBACK (downmix_method_changed), videosink);
  gtk_container_add (GTK_CONTAINER (vbox), w);

  //configure the pipeline
  g_signal_connect (G_OBJECT (window), "delete-event", G_CALLBACK (destroy_cb),
      pipeline);

  gtk_widget_realize (area);

  /* Redraw needed when paused or stopped (PAUSED or READY) */
  g_signal_connect (area, "draw", G_CALLBACK (draw_cb), videosink);
  g_signal_connect(area, "configure-event", G_CALLBACK(resize_cb), videosink);

  gtk_widget_show_all (window);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  gtk_main ();

  return 0;
}
