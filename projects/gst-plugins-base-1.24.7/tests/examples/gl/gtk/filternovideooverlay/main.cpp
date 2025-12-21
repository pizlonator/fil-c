/*
 * GStreamer
 * Copyright (C) 2008-2009 Julien Isorce <julien.isorce@gmail.com>
 * Copyright (C) 2014 Matthew Waters <ystreet00@gmail.com>
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


static void end_stream_cb(GstBus* bus, GstMessage* message, GstElement* pipeline)
{
    g_print("End of stream\n");

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
    gtk_init (&argc, &argv);
    gst_init (&argc, &argv);

    GstElement* pipeline = gst_pipeline_new ("pipeline");

    //window to control the states
    GtkWidget* window_control = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    GdkGeometry geometry;
    geometry.min_width = 1;
    geometry.min_height = 1;
    geometry.max_width = -1;
    geometry.max_height = -1;
    gtk_window_set_geometry_hints (GTK_WINDOW (window_control), window_control, &geometry, GDK_HINT_MIN_SIZE);
    gtk_window_set_resizable (GTK_WINDOW (window_control), FALSE);
    gtk_window_move (GTK_WINDOW (window_control), 10, 10);
    GtkWidget* table = gtk_grid_new ();
    gtk_container_add (GTK_CONTAINER (window_control), table);

    //control state null
    GtkWidget* button_state_null = gtk_button_new_with_label ("GST_STATE_NULL");
    g_signal_connect (G_OBJECT (button_state_null), "clicked",
        G_CALLBACK (button_state_null_cb), pipeline);
    gtk_grid_attach (GTK_GRID (table), button_state_null, 0, 0, 1, 1);
    gtk_widget_show (button_state_null);

    //control state ready
    GtkWidget* button_state_ready = gtk_button_new_with_label ("GST_STATE_READY");
    g_signal_connect (G_OBJECT (button_state_ready), "clicked",
        G_CALLBACK (button_state_ready_cb), pipeline);
    gtk_grid_attach (GTK_GRID (table), button_state_ready, 0, 1, 1, 1);
    gtk_widget_show (button_state_ready);

    //control state paused
    GtkWidget* button_state_paused = gtk_button_new_with_label ("GST_STATE_PAUSED");
    g_signal_connect (G_OBJECT (button_state_paused), "clicked",
        G_CALLBACK (button_state_paused_cb), pipeline);
    gtk_grid_attach (GTK_GRID (table), button_state_paused, 0, 2, 1, 1);
    gtk_widget_show (button_state_paused);

    //control state playing
    GtkWidget* button_state_playing = gtk_button_new_with_label ("GST_STATE_PLAYING");
    g_signal_connect (G_OBJECT (button_state_playing), "clicked",
        G_CALLBACK (button_state_playing_cb), pipeline);
    gtk_grid_attach (GTK_GRID (table), button_state_playing, 0, 3, 1, 1);
    gtk_widget_show (button_state_playing);

    //change framerate
    GtkWidget* slider_fps = gtk_scale_new_with_range (GTK_ORIENTATION_VERTICAL,
        1, 30, 2);
    g_signal_connect (G_OBJECT (slider_fps), "format-value",
        G_CALLBACK (slider_fps_cb), pipeline);
    gtk_grid_attach (GTK_GRID (table), slider_fps, 1, 0, 1, 3);
    gtk_widget_show (slider_fps);

    gtk_widget_show (table);
    gtk_widget_show (window_control);

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

    //set window id on this event
    GstBus* bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    gst_bus_add_signal_watch (bus);
    g_signal_connect(bus, "message::error", G_CALLBACK(end_stream_cb), pipeline);
    g_signal_connect(bus, "message::warning", G_CALLBACK(end_stream_cb), pipeline);
    g_signal_connect(bus, "message::eos", G_CALLBACK(end_stream_cb), pipeline);
    gst_object_unref (bus);

    //start
    GstStateChangeReturn ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_print ("Failed to start up pipeline!\n");
        return -1;
    }

    gtk_main();

    return 0;
}

