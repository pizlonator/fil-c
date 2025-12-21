/*
 * GStreamer
 * Copyright (C) 2008-2009 Julien Isorce <julien.isorce@gmail.com>
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
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "../gstgtk.h"

#ifdef HAVE_X11
#include <X11/Xlib.h>
#endif

static GstBusSyncReply create_window (GstBus* bus, GstMessage* message, GtkWidget* widget)
{
    GtkAllocation allocation;

    if (gst_gtk_handle_need_context (bus, message, NULL))
        return GST_BUS_DROP;

    // ignore anything but 'prepare-window-handle' element messages
    if (GST_MESSAGE_TYPE (message) != GST_MESSAGE_ELEMENT)
        return GST_BUS_PASS;

    if (!gst_is_video_overlay_prepare_window_handle_message (message))
        return GST_BUS_PASS;

    g_print ("setting window handle %p\n", widget);

    gst_video_overlay_set_gtk_window (GST_VIDEO_OVERLAY (GST_MESSAGE_SRC (message)), widget);

    gtk_widget_get_allocation (widget, &allocation);
    gst_video_overlay_set_render_rectangle (GST_VIDEO_OVERLAY (GST_MESSAGE_SRC (message)), allocation.x, allocation.y, allocation.width, allocation.height);

    gst_message_unref (message);

    return GST_BUS_DROP;
}

static gboolean
resize_cb (GtkWidget * widget, GdkEvent * event, gpointer sink)
{
    GtkAllocation allocation;

    gtk_widget_get_allocation (widget, &allocation);
    gst_video_overlay_set_render_rectangle (GST_VIDEO_OVERLAY (sink), allocation.x, allocation.y, allocation.width, allocation.height);

    return G_SOURCE_CONTINUE;
}

static void end_stream_cb(GstBus* bus, GstMessage* message, GstElement* pipeline)
{
    GError *error = NULL;
    gchar *details;

    switch (GST_MESSAGE_TYPE (message)) {
        case GST_MESSAGE_ERROR:
            gst_message_parse_error (message, &error, &details);

            g_print("Error %s\n", error->message);
            g_print("Details %s\n", details);
        /* fallthrough */
        case GST_MESSAGE_EOS:
            g_print("End of stream\n");

            gst_element_set_state (pipeline, GST_STATE_NULL);
            gst_object_unref(pipeline);

            gtk_main_quit();
            break;
        case GST_MESSAGE_WARNING:
            gst_message_parse_warning (message, &error, &details);

            g_print("Warning %s\n", error->message);
            g_print("Details %s\n", details);
            break;
        default:
            break;
    }
}

static gboolean expose_cb(GtkWidget* widget, cairo_t *cr, GstElement* videosink)
{
    gst_video_overlay_expose (GST_VIDEO_OVERLAY (videosink));
    return FALSE;
}


static void destroy_cb(GtkWidget* widget, GdkEvent* event, GstElement* pipeline)
{
    g_print("Close\n");

    gst_element_set_state (pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    gtk_main_quit();
}


static void button_state_null_cb(GtkWidget* widget, GstElement* pipeline)
{
    gst_element_set_state (pipeline, GST_STATE_NULL);
    g_print ("GST_STATE_NULL\n");
}


static void button_state_ready_cb(GtkWidget* widget, GstElement* pipeline)
{
    gst_element_set_state (pipeline, GST_STATE_READY);
    g_print ("GST_STATE_READY\n");
}


static void button_state_paused_cb(GtkWidget* widget, GstElement* pipeline)
{
    gst_element_set_state (pipeline, GST_STATE_PAUSED);
    g_print ("GST_STATE_PAUSED\n");
}


static void button_state_playing_cb(GtkWidget* widget, GstElement* pipeline)
{
    gst_element_set_state (pipeline, GST_STATE_PLAYING);
    g_print ("GST_STATE_PLAYING\n");
}


static gchar* slider_fps_cb (GtkScale* scale, gdouble value, GstElement* pipeline)
{
    //change the video frame rate dynamically
    return g_strdup_printf ("video framerate: %0.*g", gtk_scale_get_digits (scale), value);
}



gint main (gint argc, gchar *argv[])
{
#ifdef HAVE_X11
    XInitThreads ();
#endif

    gtk_init (&argc, &argv);
    gst_init (&argc, &argv);

    GstElement* pipeline = gst_pipeline_new ("pipeline");

    //window that contains an area where the video is drawn
    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_size_request (window, 640, 480);
    gtk_window_move (GTK_WINDOW (window), 300, 10);
    gtk_window_set_title (GTK_WINDOW (window), "glimagesink implement the gstvideooverlay interface");
    GdkGeometry geometry;
    geometry.min_width = 1;
    geometry.min_height = 1;
    geometry.max_width = -1;
    geometry.max_height = -1;
    gtk_window_set_geometry_hints (GTK_WINDOW (window), window, &geometry, GDK_HINT_MIN_SIZE);

    //window to control the states
    GtkWidget* window_control = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    geometry.min_width = 1;
    geometry.min_height = 1;
    geometry.max_width = -1;
    geometry.max_height = -1;
    gtk_window_set_geometry_hints (GTK_WINDOW (window_control), window_control, &geometry, GDK_HINT_MIN_SIZE);
    gtk_window_set_resizable (GTK_WINDOW (window_control), FALSE);
    gtk_window_move (GTK_WINDOW (window_control), 10, 10);
    GtkWidget* grid = gtk_grid_new ();
    gtk_container_add (GTK_CONTAINER (window_control), grid);

    //control state null
    GtkWidget* button_state_null = gtk_button_new_with_label ("GST_STATE_NULL");
    g_signal_connect (G_OBJECT (button_state_null), "clicked",
        G_CALLBACK (button_state_null_cb), pipeline);
    gtk_grid_attach (GTK_GRID (grid), button_state_null, 0, 1, 1, 1);
    gtk_widget_show (button_state_null);

    //control state ready
    GtkWidget* button_state_ready = gtk_button_new_with_label ("GST_STATE_READY");
    g_signal_connect (G_OBJECT (button_state_ready), "clicked",
        G_CALLBACK (button_state_ready_cb), pipeline);
    gtk_grid_attach (GTK_GRID (grid), button_state_ready, 0, 2, 1, 1);
    gtk_widget_show (button_state_ready);

    //control state paused
    GtkWidget* button_state_paused = gtk_button_new_with_label ("GST_STATE_PAUSED");
    g_signal_connect (G_OBJECT (button_state_paused), "clicked",
        G_CALLBACK (button_state_paused_cb), pipeline);
    gtk_grid_attach (GTK_GRID (grid), button_state_paused, 0, 3, 1, 1);
    gtk_widget_show (button_state_paused);

    //control state playing
    GtkWidget* button_state_playing = gtk_button_new_with_label ("GST_STATE_PLAYING");
    g_signal_connect (G_OBJECT (button_state_playing), "clicked",
        G_CALLBACK (button_state_playing_cb), pipeline);
    gtk_grid_attach (GTK_GRID (grid), button_state_playing, 0, 4, 1, 1);
    gtk_widget_show (button_state_playing);

    //change framerate
    GtkWidget* slider_fps = gtk_scale_new_with_range (GTK_ORIENTATION_VERTICAL, 1, 30, 2);
    g_signal_connect (G_OBJECT (slider_fps), "format-value",
        G_CALLBACK (slider_fps_cb), pipeline);
    gtk_grid_attach (GTK_GRID (grid), slider_fps, 1, 0, 1, 5);
    gtk_widget_show (slider_fps);

    gtk_widget_show (grid);
    gtk_widget_show (window_control);

    //configure the pipeline
    g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(destroy_cb), pipeline);

    GstElement* videosrc = gst_element_factory_make ("videotestsrc", "videotestsrc");
    GstElement* upload = gst_element_factory_make ("glupload", "glupload");
    GstElement* glfiltercube = gst_element_factory_make ("glfiltercube", "glfiltercube");
    GstElement* videosink = gst_element_factory_make ("glimagesink", "glimagesink");

    GstCaps *caps = gst_caps_new_simple("video/x-raw",
                                        "width", G_TYPE_INT, 640,
                                        "height", G_TYPE_INT, 480,
                                        "framerate", GST_TYPE_FRACTION, 25, 1,
                                        "format", G_TYPE_STRING, "RGBA",
                                        NULL) ;

    gst_bin_add_many (GST_BIN (pipeline), videosrc, upload, glfiltercube, videosink, NULL);

    gboolean link_ok = gst_element_link_filtered(videosrc, upload, caps) ;
    gst_caps_unref(caps) ;
    if(!link_ok)
    {
        g_warning("Failed to link videosrc to glfiltercube!\n") ;
        return -1;
    }

    if(!gst_element_link_many(upload, glfiltercube, videosink, NULL))
    {
        g_warning("Failed to link glfiltercube to videosink!\n") ;
        return -1;
    }

    //area where the video is drawn
    GtkWidget* area = gtk_drawing_area_new();
    gtk_widget_set_redraw_on_allocate (area, TRUE);
    gtk_container_add (GTK_CONTAINER (window), area);

    gtk_widget_realize(area);

    //set window id on this event
    GstBus* bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    gst_bus_set_sync_handler (bus, (GstBusSyncHandler) create_window, area, NULL);
    gst_bus_add_signal_watch (bus);
    g_signal_connect(bus, "message::error", G_CALLBACK(end_stream_cb), pipeline);
    g_signal_connect(bus, "message::warning", G_CALLBACK(end_stream_cb), pipeline);
    g_signal_connect(bus, "message::eos", G_CALLBACK(end_stream_cb), pipeline);
    gst_object_unref (bus);

    //needed when being in GST_STATE_READY, GST_STATE_PAUSED
    //or resizing/obscuring the window
    g_signal_connect(area, "draw", G_CALLBACK(expose_cb), videosink);
    g_signal_connect(area, "configure-event", G_CALLBACK(resize_cb), videosink);

    //start
    GstStateChangeReturn ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_print ("Failed to start up pipeline!\n");
        return -1;
    }

    gtk_widget_show_all (window);

    gtk_main();

    return 0;
}

