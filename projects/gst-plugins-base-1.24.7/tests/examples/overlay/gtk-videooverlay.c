/* GStreamer
 * Copyright (C) <2010> Stefan Kost <ensonic@users.sf.net>
 *
 * gtk-videooverlay: demonstrate overlay handling using gtk
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

#define GDK_VERSION_MIN_REQUIRED (GDK_VERSION_3_0)

#include <glib.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <gst/gst.h>
#include <gst/video/videooverlay.h>

#include <string.h>

static void
window_closed (GtkWidget * widget, GdkEvent * event, gpointer user_data)
{
  GstElement *pipeline = user_data;

  gtk_widget_hide (widget);
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gtk_main_quit ();
}

/* slightly convoluted way to find a working video sink that's not a bin,
 * one could use autovideosink from gst-plugins-good instead
 */
static GstElement *
find_video_sink (void)
{
  GstStateChangeReturn sret;
  GstElement *sink;

  if ((sink = gst_element_factory_make ("xvimagesink", NULL))) {
    sret = gst_element_set_state (sink, GST_STATE_READY);
    if (sret == GST_STATE_CHANGE_SUCCESS)
      return sink;

    gst_element_set_state (sink, GST_STATE_NULL);
    gst_object_unref (sink);
  }

  if ((sink = gst_element_factory_make ("ximagesink", NULL))) {
    sret = gst_element_set_state (sink, GST_STATE_READY);
    if (sret == GST_STATE_CHANGE_SUCCESS)
      return sink;

    gst_element_set_state (sink, GST_STATE_NULL);
    gst_object_unref (sink);
  }

  if (strcmp (DEFAULT_VIDEOSINK, "xvimagesink") == 0 ||
      strcmp (DEFAULT_VIDEOSINK, "ximagesink") == 0)
    return NULL;

  if ((sink = gst_element_factory_make (DEFAULT_VIDEOSINK, NULL))) {
    if (GST_IS_BIN (sink)) {
      gst_object_unref (sink);
      return NULL;
    }

    sret = gst_element_set_state (sink, GST_STATE_READY);
    if (sret == GST_STATE_CHANGE_SUCCESS)
      return sink;

    gst_element_set_state (sink, GST_STATE_NULL);
    gst_object_unref (sink);
  }

  return NULL;
}

int
main (int argc, char **argv)
{
  GdkWindow *video_window_xwindow;
  GtkWidget *window, *video_window;
  GstElement *pipeline, *src, *sink;
  gulong embed_xid;
  GstStateChangeReturn sret;

  gst_init (&argc, &argv);
  gtk_init (&argc, &argv);

  /* prepare the pipeline */

  pipeline = gst_pipeline_new ("xvoverlay");
  src = gst_element_factory_make ("videotestsrc", NULL);
  sink = find_video_sink ();

  if (sink == NULL)
    g_error ("Couldn't find a working video sink.");

  gst_bin_add_many (GST_BIN (pipeline), src, sink, NULL);
  gst_element_link (src, sink);

  /* prepare the ui */

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (G_OBJECT (window), "delete-event",
      G_CALLBACK (window_closed), (gpointer) pipeline);
  gtk_window_set_default_size (GTK_WINDOW (window), 320, 240);
  gtk_window_set_title (GTK_WINDOW (window), "GstVideoOverlay Gtk+ demo");

  video_window = gtk_drawing_area_new ();
  gtk_container_add (GTK_CONTAINER (window), video_window);
  gtk_container_set_border_width (GTK_CONTAINER (window), 16);

  gtk_widget_show_all (window);

  video_window_xwindow = gtk_widget_get_window (video_window);
  embed_xid = GDK_WINDOW_XID (video_window_xwindow);
  gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (sink), embed_xid);

  /* run the pipeline */

  sret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  if (sret == GST_STATE_CHANGE_FAILURE)
    gst_element_set_state (pipeline, GST_STATE_NULL);
  else
    gtk_main ();

  gst_object_unref (pipeline);
  return 0;
}
