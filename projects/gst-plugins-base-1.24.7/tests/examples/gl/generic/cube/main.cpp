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

//client reshape callback
static gboolean reshapeCallback (void *gl_sink, void *context, GLuint width, GLuint height, gpointer data)
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

    /* invert the y-axis to get the front face the correct way up */
    glScalef (0.5f, -0.5f, 0.5f);

    glRotatef(xrot,1.0f,0.0f,0.0f);
    glRotatef(yrot,0.0f,1.0f,0.0f);
    glRotatef(zrot,0.0f,0.0f,1.0f);

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

    gst_video_frame_unmap (&v_frame);

    xrot+=0.3f;
    yrot+=0.2f;
    zrot+=0.4f;

    glDisable (GL_DEPTH_TEST);
    glDisable (GL_TEXTURE_2D);

    return TRUE;
}


//gst-launch-1.0 videotestsrc num_buffers=400 ! video/x-raw, width=320, height=240 !
//glgraphicmaker ! glfiltercube ! video/x-raw, width=800, height=600 ! glimagesink
gint main (gint argc, gchar *argv[])
{
    GstStateChangeReturn ret;
    GstElement *pipeline, *videosrc, *glimagesink;

    GMainLoop *loop;
    GstBus *bus;

    /* FIXME: remove once the example supports gl3 and/or gles2 */
    g_setenv ("GST_GL_API", "opengl", FALSE);

    /* initialization */
    gst_init (&argc, &argv);
    loop = g_main_loop_new (NULL, FALSE);

    /* create elements */
    pipeline = gst_pipeline_new ("pipeline");

    /* watch for messages on the pipeline's bus (note that this will only
     * work like this when a GLib main loop is running) */
    bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    gst_bus_add_watch (bus, bus_call, loop);
    gst_object_unref (bus);

    /* create elements */
    videosrc = gst_element_factory_make ("videotestsrc", "videotestsrc0");
    glimagesink  = gst_element_factory_make ("glimagesink", "glimagesink0");


    if (!videosrc || !glimagesink)
    {
        g_print ("one element could not be found \n");
        return -1;
    }

    /* change video source caps */
    GstCaps *caps = gst_caps_new_simple("video/x-raw",
                                        "format", G_TYPE_STRING, "RGB",
                                        "width", G_TYPE_INT, 320,
                                        "height", G_TYPE_INT, 240,
                                        "framerate", GST_TYPE_FRACTION, 25, 1,
                                        NULL) ;

    /* configure elements */
    g_object_set(G_OBJECT(videosrc), "num-buffers", 400, NULL);
    g_signal_connect(G_OBJECT(glimagesink), "client-reshape", G_CALLBACK (reshapeCallback), NULL);
    g_signal_connect(G_OBJECT(glimagesink), "client-draw", G_CALLBACK (drawCallback), NULL);

    /* add elements */
    gst_bin_add_many (GST_BIN (pipeline), videosrc, glimagesink, NULL);

    /* link elements */
    gboolean link_ok = gst_element_link_filtered(videosrc, glimagesink, caps) ;
    gst_caps_unref(caps) ;
    if(!link_ok)
    {
        g_warning("Failed to link videosrc to glimagesink!\n") ;
        return -1 ;
    }

    /* run */
    ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
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
