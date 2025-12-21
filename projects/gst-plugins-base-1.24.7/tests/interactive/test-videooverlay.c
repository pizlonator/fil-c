/* GStreamer
 * Copyright (C) <2008> Stefan Kost <ensonic@users.sf.net>
 *
 * test-videooverlay: test videooverlay custom event handling and subregions
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
/* Disable deprecation warnings because we need to use
 * gtk_widget_set_double_buffered () or display will flicker */
#define GDK_DISABLE_DEPRECATION_WARNINGS

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <glib.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <gst/video/gstvideosink.h>

static struct
{
  gint w, h;
  GstVideoOverlay *overlay;
  GtkWidget *widget;
  gdouble a, p;
  GstVideoRectangle rect;
  gboolean running;
} anim_state;

static gboolean verbose = FALSE;

static gboolean
animate_render_rect (gpointer user_data)
{
  if (anim_state.running) {
    GstVideoRectangle *r = &anim_state.rect;
    gdouble s = sin (3.0 * anim_state.a);
    gdouble c = cos (2.0 * anim_state.a);

    anim_state.a += anim_state.p;
    if (anim_state.a > (G_PI + G_PI))
      anim_state.a -= (G_PI + G_PI);

    r->w = anim_state.w / 2;
    r->x = (r->w - (r->w / 2)) + c * (r->w / 2);
    r->h = anim_state.h / 2;
    r->y = (r->h - (r->h / 2)) + s * (r->h / 2);

    gst_video_overlay_set_render_rectangle (anim_state.overlay, r->x, r->y,
        r->w, r->h);
    gtk_widget_queue_draw (anim_state.widget);
  }
  return TRUE;
}

static gboolean
handle_resize_cb (GtkWidget * widget, GdkEventConfigure * event,
    gpointer user_data)
{
  GtkAllocation allocation;

  gtk_widget_get_allocation (widget, &allocation);

  if (verbose) {
    g_print ("resize(%p): %dx%d\n", widget, allocation.width,
        allocation.height);
  }
  anim_state.w = allocation.width;
  anim_state.h = allocation.height;
  animate_render_rect (NULL);

  return FALSE;
}

static gboolean
handle_draw_cb (GtkWidget * widget, cairo_t * cr, gpointer user_data)
{
  GstVideoRectangle *r = &anim_state.rect;
  GtkStyleContext *style;
  GdkRGBA color;
  int width, height;

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  style = gtk_widget_get_style_context (widget);

  gtk_style_context_get_color (style, 0, &color);
  gdk_cairo_set_source_rgba (cr, &color);

  /* we should only redraw outside of the video rect! */
  cairo_rectangle (cr, 0, 0, r->x, height);
  cairo_rectangle (cr, r->x + r->w, 0, width - (r->x + r->w), height);

  cairo_rectangle (cr, 0, 0, width, r->y);
  cairo_rectangle (cr, 0, r->y + r->h, width, height - (r->y + r->h));

  cairo_fill (cr);

  if (verbose) {
    g_print ("draw(%p)\n", widget);
  }
  gst_video_overlay_expose (anim_state.overlay);
  return FALSE;
}

static void
window_closed (GtkWidget * widget, GdkEvent * event, gpointer user_data)
{
  GstElement *pipeline = user_data;

  if (verbose) {
    g_print ("stopping\n");
  }
  anim_state.running = FALSE;
  gtk_widget_hide (widget);
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gtk_main_quit ();
}

gint
main (gint argc, gchar ** argv)
{
  GdkWindow *video_window_xwindow;
  GtkWidget *window, *video_window;
  GstElement *pipeline, *src, *sink;
  GstStateChangeReturn sret;
  gulong embed_xid = 0;
  gboolean force_aspect = FALSE, draw_borders = FALSE;

  gst_init (&argc, &argv);
  gtk_init (&argc, &argv);

  if (argc) {
    gint arg;
    for (arg = 0; arg < argc; arg++) {
      if (!strcmp (argv[arg], "-a"))
        force_aspect = TRUE;
      else if (!strcmp (argv[arg], "-b"))
        draw_borders = TRUE;
      else if (!strcmp (argv[arg], "-v"))
        verbose = TRUE;
    }
  }

  /* prepare the pipeline */

  pipeline = gst_pipeline_new ("xvoverlay");
  src = gst_element_factory_make ("videotestsrc", NULL);
  sink = gst_element_factory_make ("xvimagesink", NULL);
  gst_bin_add_many (GST_BIN (pipeline), src, sink, NULL);
  gst_element_link (src, sink);

  g_object_set (G_OBJECT (sink), "handle-events", FALSE,
      "force-aspect-ratio", force_aspect, "draw-borders", draw_borders, NULL);

  /* prepare the ui */

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (G_OBJECT (window), "delete-event",
      G_CALLBACK (window_closed), (gpointer) pipeline);
  gtk_window_set_default_size (GTK_WINDOW (window), 320, 240);

  video_window = gtk_drawing_area_new ();
  gtk_widget_set_double_buffered (video_window, FALSE);
  gtk_container_add (GTK_CONTAINER (window), video_window);

  /* show the gui and play */
  gtk_widget_show_all (window);

  /* realize window now so that the video window gets created and we can
   * obtain its XID before the pipeline is started up and the videosink
   * asks for the XID of the window to render onto */
  gtk_widget_realize (window);

  video_window_xwindow = gtk_widget_get_window (video_window);
  embed_xid = GDK_WINDOW_XID (video_window_xwindow);
  if (verbose) {
    g_print ("Window realize: got XID %lu\n", embed_xid);
  }

  /* we know what the video sink is in this case (xvimagesink), so we can
   * just set it directly here now (instead of waiting for a
   * prepare-window-handle element message in a sync bus handler and setting
   * it there) */
  gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (sink), embed_xid);

  anim_state.overlay = GST_VIDEO_OVERLAY (sink);
  anim_state.widget = video_window;
  anim_state.w = 320;
  anim_state.h = 240;
  anim_state.a = 0.0;
  anim_state.p = (G_PI + G_PI) / 200.0;

  handle_resize_cb (video_window, NULL, sink);
  g_signal_connect (video_window, "configure-event",
      G_CALLBACK (handle_resize_cb), NULL);
  g_signal_connect (video_window, "draw", G_CALLBACK (handle_draw_cb), NULL);

  g_timeout_add (50, (GSourceFunc) animate_render_rect, NULL);

  /* run the pipeline */
  sret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  if (sret == GST_STATE_CHANGE_FAILURE)
    gst_element_set_state (pipeline, GST_STATE_NULL);
  else {
    anim_state.running = TRUE;
    gtk_main ();
  }

  gst_object_unref (pipeline);
  return 0;
}
