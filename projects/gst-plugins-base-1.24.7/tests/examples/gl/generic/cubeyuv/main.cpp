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

#include <gst/gst.h>
#include <gst/gl/gl.h>
#include <gst/gl/gstglfuncs.h>

#include <iostream>
#include <sstream>
#include <string>

static gboolean bus_call (GstBus *bus, GstMessage *msg, gpointer data)
{
    GMainLoop *loop = (GMainLoop*)data;

    switch (GST_MESSAGE_TYPE (msg))
    {
        case GST_MESSAGE_EOS:
              g_print ("End-of-stream\n");
              g_main_loop_quit (loop);
              break;
        case GST_MESSAGE_ERROR:
          {
              gchar *debug = NULL;
              GError *err = NULL;

              gst_message_parse_error (msg, &err, &debug);

              g_print ("Error: %s\n", err->message);
              g_error_free (err);

              if (debug)
              {
                  g_print ("Debug deails: %s\n", debug);
                  g_free (debug);
              }

              g_main_loop_quit (loop);
              break;
          }
        default:
          break;
    }

    return TRUE;
}


//display video framerate
static void identityCallback (GstElement *src, GstBuffer  *buffer, GstElement* textoverlay)
{
    static GstClockTime last_timestamp = 0;
    static gint nbFrames = 0 ;

    //display estimated video FPS
    nbFrames++ ;
    if (GST_BUFFER_TIMESTAMP(buffer) - last_timestamp >= 1000000000)
    {
        gchar *s = g_strdup_printf ("video framerate = %d", nbFrames);
        g_object_set(G_OBJECT(textoverlay), "text", s, NULL);
        g_free (s);
        last_timestamp = GST_BUFFER_TIMESTAMP(buffer) ;
        nbFrames = 0 ;
    }
}


//client reshape callback
static gboolean reshapeCallback (void * gl_sink, void *context, GLuint width, GLuint height, gpointer data)
{
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);

    return TRUE;
}


//client draw callback
static gboolean drawCallback (GstElement * gl_sink, GstGLContext *context, GstSample * sample, gpointer data)
{
    static GLfloat	xrot = 0;
    static GLfloat	yrot = 0;
    static GLfloat	zrot = 0;
    static GstClockTime current_time;
    static GstClockTime last_time = gst_util_get_timestamp();
    static gint nbFrames = 0;

    GstVideoFrame v_frame;
    GstVideoInfo v_info;
    guint texture = 0;
    GstBuffer *buf = gst_sample_get_buffer (sample);
    GstCaps *caps = gst_sample_get_caps (sample);

    gst_video_info_from_caps (&v_info, caps);

    if (!gst_video_frame_map (&v_frame, &v_info, buf, (GstMapFlags) (GST_MAP_READ | GST_MAP_GL))) {
      g_warning ("Failed to map the video buffer");
      return TRUE;
    }

    texture = *(guint *) v_frame.data[0];

    current_time = gst_util_get_timestamp ();
    nbFrames++ ;

    if ((current_time - last_time) >= GST_SECOND)
    {
        std::cout << "GRAPHIC FPS = " << nbFrames << std::endl;
        nbFrames = 0;
        last_time = current_time;
    }

    glEnable(GL_DEPTH_TEST);

    glEnable (GL_TEXTURE_2D);
    glBindTexture (GL_TEXTURE_2D, texture);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glRotatef(xrot,1.0f,0.0f,0.0f);
    glRotatef(yrot,0.0f,1.0f,0.0f);
    glRotatef(zrot,0.0f,0.0f,1.0f);

    /* invert the y-axis to get the front face the correct way up */
    glScalef (0.5f, -0.5f, 0.5f);

    glBegin(GL_QUADS);
	      // Front Face
	      glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f, -1.0f,  1.0f);
	      glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f, -1.0f,  1.0f);
	      glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f,  1.0f,  1.0f);
	      glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f,  1.0f,  1.0f);
	      // Back Face
	      glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f, -1.0f, -1.0f);
	      glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f,  1.0f, -1.0f);
	      glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f,  1.0f, -1.0f);
	      glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f, -1.0f, -1.0f);
	      // Top Face
	      glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f,  1.0f, -1.0f);
	      glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f,  1.0f,  1.0f);
	      glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f,  1.0f,  1.0f);
	      glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f,  1.0f, -1.0f);
	      // Bottom Face
	      glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f, -1.0f, -1.0f);
	      glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f, -1.0f, -1.0f);
	      glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f, -1.0f,  1.0f);
	      glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f, -1.0f,  1.0f);
	      // Right face
	      glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f, -1.0f, -1.0f);
	      glTexCoord2f(0.0f, 1.0f); glVertex3f( 1.0f,  1.0f, -1.0f);
	      glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f,  1.0f,  1.0f);
	      glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f, -1.0f,  1.0f);
	      // Left Face
	      glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f, -1.0f, -1.0f);
	      glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f, -1.0f,  1.0f);
	      glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f,  1.0f,  1.0f);
	      glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f,  1.0f, -1.0f);
    glEnd();

    glDisable(GL_DEPTH_TEST);

    gst_video_frame_unmap (&v_frame);

	xrot+=0.03f;
    yrot+=0.02f;
    zrot+=0.04f;

    return TRUE;
}


static void cb_new_pad (GstElement* decodebin, GstPad* pad, GstElement* identity)
{
    GstPad* identity_pad = gst_element_get_static_pad (identity, "sink");

    //only link once
    if (GST_PAD_IS_LINKED (identity_pad))
    {
        gst_object_unref (identity_pad);
        return;
    }

    GstCaps* caps = gst_pad_get_current_caps (pad);
    GstStructure* str = gst_caps_get_structure (caps, 0);
    if (!g_strrstr (gst_structure_get_name (str), "video"))
    {
        gst_caps_unref (caps);
        gst_object_unref (identity_pad);
        return;
    }
    gst_caps_unref (caps);

    GstPadLinkReturn ret = gst_pad_link (pad, identity_pad);
    if (ret != GST_PAD_LINK_OK)
        g_warning ("Failed to link with decodebin!\n");
}


gint main (gint argc, gchar *argv[])
{
    if (argc != 2)
    {
        g_warning ("usage: cubeyuv.exe videolocation\n");
        return -1;
    }

    /* FIXME: remove once the example supports gl3 and/or gles2 */
    g_setenv ("GST_GL_API", "opengl", FALSE);

    std::string video_location(argv[1]);

    /* initialization */
    gst_init (&argc, &argv);
    GMainLoop* loop = g_main_loop_new (NULL, FALSE);

    /* create elements */
    GstElement* pipeline = gst_pipeline_new ("pipeline");

    /* watch for messages on the pipeline's bus (note that this will only
     * work like this when a GLib main loop is running) */
    GstBus* bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    gst_bus_add_watch (bus, bus_call, loop);
    gst_object_unref (bus);

    /* create elements */
    GstElement* videosrc = gst_element_factory_make ("filesrc", "filesrc0");
    GstElement* decodebin = gst_element_factory_make ("decodebin", "decodebin");
    GstElement* identity  = gst_element_factory_make ("identity", "identity0");
    GstElement* textoverlay = gst_element_factory_make ("textoverlay", "textoverlay0");
    GstElement* glimagesink  = gst_element_factory_make ("glimagesink", "glimagesink0");


    if (!videosrc || !decodebin || !identity || !textoverlay || !glimagesink)
    {
        g_print ("one element could not be found \n");
        return -1;
    }

    /* configure elements */
    g_object_set(G_OBJECT(videosrc), "num-buffers", 800, NULL);
    g_object_set(G_OBJECT(videosrc), "location", video_location.c_str(), NULL);
    g_signal_connect(identity, "handoff", G_CALLBACK(identityCallback), textoverlay) ;
    g_object_set(G_OBJECT(textoverlay), "font_desc", "Ahafoni CLM Bold 30", NULL);
    g_signal_connect(G_OBJECT(glimagesink), "client-reshape", G_CALLBACK (reshapeCallback), NULL);
    g_signal_connect(G_OBJECT(glimagesink), "client-draw", G_CALLBACK (drawCallback), NULL);

    /* add elements */
    gst_bin_add_many (GST_BIN (pipeline), videosrc, decodebin, identity,
        textoverlay, glimagesink, NULL);

    /* link elements */
	gst_element_link_pads (videosrc, "src", decodebin, "sink");

    g_signal_connect (decodebin, "pad-added", G_CALLBACK (cb_new_pad), identity);

    if (!gst_element_link_pads(identity, "src", textoverlay, "video_sink"))
    {
        g_print ("Failed to link identity to textoverlay!\n");
        return -1;
    }

    gboolean link_ok = gst_element_link (textoverlay, glimagesink);
    if(!link_ok)
    {
        g_warning("Failed to link textoverlay to glimagesink!\n") ;
        return -1 ;
    }

    /* run */
    GstStateChangeReturn ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_print ("Failed to start up pipeline!\n");

        /* check if there is an error message with details on the bus */
        GstMessage* msg = gst_bus_poll (bus, GST_MESSAGE_ERROR, 0);
        if (msg)
        {
          GError *err = NULL;

          gst_message_parse_error (msg, &err, NULL);
          g_print ("ERROR: %s\n", err->message);
          g_error_free (err);
          gst_message_unref (msg);
        }
        return -1;
    }

    g_main_loop_run (loop);

    /* clean up */
    gst_element_set_state (pipeline, GST_STATE_NULL);
    gst_object_unref (pipeline);

    return 0;
}
