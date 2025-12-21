/*
 * GStreamer
 * Copyright (C) 2015 Julien Isorce <julien.isorce@gmail.com>
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

#ifdef WIN32
#include <windows.h>
#endif

#include <GL/gl.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#ifndef WIN32
#include <GL/glx.h>
#include <SDL2/SDL_syswm.h>
#include <gst/gl/x11/gstgldisplay_x11.h>
#endif

#include <gst/gst.h>
#include <gst/gl/gl.h>

static GstGLContext *sdl_context;
static GstGLDisplay *sdl_gl_display;

static SDL_Window *sdl_window;
static SDL_GLContext sdl_gl_context;

/* rotation angle for the triangle. */
static float rtri = 0.0f;

/* rotation angle for the quadrilateral. */
static float rquad = 0.0f;

/* A general OpenGL initialization function.  Sets all of the initial parameters. */
static void
InitGL (int Width, int Height)  // We call this right after our OpenGL window is created.
{
  glViewport (0, 0, Width, Height);
  glClearColor (0.0f, 0.0f, 0.0f, 0.0f);        // This Will Clear The Background Color To Black
  glClearDepth (1.0);           // Enables Clearing Of The Depth Buffer
  glDepthFunc (GL_LESS);        // The Type Of Depth Test To Do
  glEnable (GL_DEPTH_TEST);     // Enables Depth Testing
  glShadeModel (GL_SMOOTH);     // Enables Smooth Color Shading

  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();            // Reset The Projection Matrix

  glMatrixMode (GL_MODELVIEW);
}

/* The main drawing function. */
static void
DrawGLScene (GstVideoFrame * v_frame)
{
  guint texture = 0;

#ifdef WIN32
  if (!wglGetCurrentContext ())
    return;
#else
  if (!glXGetCurrentContext ())
    return;
#endif

  texture = *(guint *) v_frame->data[0];

  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);  // Clear The Screen And The Depth Buffer
  glLoadIdentity ();            // Reset The View

  glTranslatef (-0.4f, 0.0f, 0.0f);     // Move Left 1.5 Units And Into The Screen 6.0

  glRotatef (rtri, 0.0f, 1.0f, 0.0f);   // Rotate The Triangle On The Y axis 
  // draw a triangle (in smooth coloring mode)
  glBegin (GL_POLYGON);         // start drawing a polygon
  glColor3f (1.0f, 0.0f, 0.0f); // Set The Color To Red
  glVertex3f (0.0f, 0.4f, 0.0f);        // Top
  glColor3f (0.0f, 1.0f, 0.0f); // Set The Color To Green
  glVertex3f (0.4f, -0.4f, 0.0f);       // Bottom Right
  glColor3f (0.0f, 0.0f, 1.0f); // Set The Color To Blue
  glVertex3f (-0.4f, -0.4f, 0.0f);      // Bottom Left  
  glEnd ();                     // we're done with the polygon (smooth color interpolation)

  glEnable (GL_TEXTURE_2D);
  glBindTexture (GL_TEXTURE_2D, texture);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

  glLoadIdentity ();            // make sure we're no longer rotated.
  glTranslatef (0.5f, 0.0f, 0.0f);      // Move Right 3 Units, and back into the screen 6.0

  glRotatef (rquad, 1.0f, 0.0f, 0.0f);  // Rotate The Quad On The X axis 
  // draw a square (quadrilateral)
  glColor3f (0.4f, 0.4f, 1.0f); // set color to a blue shade.
  glBegin (GL_QUADS);           // start drawing a polygon (4 sided)
  glTexCoord3f (0.0f, 1.0f, 0.0f);
  glVertex3f (-0.4f, 0.4f, 0.0f);       // Top Left
  glTexCoord3f (1.0f, 1.0f, 0.0f);
  glVertex3f (0.4f, 0.4f, 0.0f);        // Top Right
  glTexCoord3f (1.0f, 0.0f, 0.0f);
  glVertex3f (0.4f, -0.4f, 0.0f);       // Bottom Right
  glTexCoord3f (0.0f, 0.0f, 0.0f);
  glVertex3f (-0.4f, -0.4f, 0.0f);      // Bottom Left  
  glEnd ();                     // done with the polygon

  glBindTexture (GL_TEXTURE_2D, 0);

  rtri += 1.0f;                 // Increase The Rotation Variable For The Triangle
  rquad -= 1.0f;                // Decrease The Rotation Variable For The Quad 

  // swap buffers to display, since we're double buffered.
  SDL_GL_SwapWindow (sdl_window);
}

static GMutex app_lock;
static GCond app_cond;
static gboolean app_rendered = FALSE;
static gboolean app_quit = FALSE;

static void
stop_pipeline (GstElement * pipeline)
{
  g_mutex_lock (&app_lock);
  app_quit = TRUE;
  g_cond_signal (&app_cond);
  g_mutex_unlock (&app_lock);
  gst_element_send_event (pipeline, gst_event_new_eos ());
}

static gboolean
update_sdl_scene (gpointer data)
{
  GstElement *pipeline = (GstElement *) data;
  SDL_Event event;

  while (SDL_PollEvent (&event)) {
    if (event.type == SDL_QUIT) {
      stop_pipeline (pipeline);
      return G_SOURCE_REMOVE;
    }
    if (event.type == SDL_KEYDOWN) {
      if (event.key.keysym.sym == SDLK_ESCAPE) {
        stop_pipeline (pipeline);
        return G_SOURCE_REMOVE;
      }
    }
  }

  return G_SOURCE_CONTINUE;
}

static gboolean
executeCallback (gpointer data)
{
  g_mutex_lock (&app_lock);

  if (!app_quit) {
    SDL_GL_MakeCurrent (sdl_window, sdl_gl_context);
    DrawGLScene (data);
    SDL_GL_MakeCurrent (sdl_window, NULL);
  }

  app_rendered = TRUE;
  g_cond_signal (&app_cond);
  g_mutex_unlock (&app_lock);

  return FALSE;
}

static gboolean
on_client_draw (GstElement * glsink, GstGLContext * context, GstSample * sample,
    gpointer data)
{
  GstBuffer *buf = gst_sample_get_buffer (sample);
  GstCaps *caps = gst_sample_get_caps (sample);
  GstVideoFrame v_frame;
  GstVideoInfo v_info;

  /* FIXME don't do that every frame */
  gst_video_info_from_caps (&v_info, caps);

  if (!gst_video_frame_map (&v_frame, &v_info, buf, GST_MAP_READ | GST_MAP_GL)) {
    g_warning ("Failed to map the video buffer");
    return TRUE;
  }

  g_mutex_lock (&app_lock);

  app_rendered = FALSE;
  g_idle_add_full (G_PRIORITY_HIGH, executeCallback, &v_frame, NULL);

  while (!app_rendered && !app_quit)
    g_cond_wait (&app_cond, &app_lock);

  g_mutex_unlock (&app_lock);

  gst_video_frame_unmap (&v_frame);

  return TRUE;
}

/* gst bus signal watch callback */
static void
end_stream_cb (GstBus * bus, GstMessage * msg, GMainLoop * loop)
{
  switch (GST_MESSAGE_TYPE (msg)) {

    case GST_MESSAGE_EOS:
      g_print ("End-of-stream\n");
      break;

    case GST_MESSAGE_ERROR:
    {
      gchar *debug = NULL;
      GError *err = NULL;

      gst_message_parse_error (msg, &err, &debug);

      g_print ("Error: %s\n", err->message);
      g_error_free (err);

      if (debug) {
        g_print ("Debug deails: %s\n", debug);
        g_free (debug);
      }

      break;
    }

    default:
      break;
  }

  g_main_loop_quit (loop);
}

static void
sync_bus_call (GstBus * bus, GstMessage * msg, gpointer data)
{
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_NEED_CONTEXT:
    {
      const gchar *context_type;

      gst_message_parse_context_type (msg, &context_type);
      g_print ("got need context %s\n", context_type);

      if (g_strcmp0 (context_type, GST_GL_DISPLAY_CONTEXT_TYPE) == 0) {
        GstContext *display_context =
            gst_context_new (GST_GL_DISPLAY_CONTEXT_TYPE, TRUE);
        gst_context_set_gl_display (display_context, sdl_gl_display);
        gst_element_set_context (GST_ELEMENT (msg->src), display_context);
        gst_context_unref (display_context);
      } else if (g_strcmp0 (context_type, "gst.gl.app_context") == 0) {
        GstContext *app_context = gst_context_new ("gst.gl.app_context", TRUE);
        GstStructure *s = gst_context_writable_structure (app_context);
        gst_structure_set (s, "context", GST_TYPE_GL_CONTEXT, sdl_context,
            NULL);
        gst_element_set_context (GST_ELEMENT (msg->src), app_context);
        gst_context_unref (app_context);
      }
      break;
    }
    default:
      break;
  }
}

int
main (int argc, char **argv)
{
#ifdef WIN32
  HGLRC gl_context = 0;
  HDC sdl_dc = 0;
#else
  SDL_SysWMinfo info;
  Display *sdl_display = NULL;
  GLXContext gl_context = NULL;
#endif

  GMainLoop *loop = NULL;
  GstPipeline *pipeline = NULL;
  GstBus *bus = NULL;
  GstElement *glimagesink = NULL;
  const gchar *platform;
  GError *err = NULL;

  /* Initialize SDL for video output */
  if (SDL_Init (SDL_INIT_VIDEO) < 0) {
    fprintf (stderr, "Unable to initialize SDL: %s\n", SDL_GetError ());
    return -1;
  }

  gst_init (&argc, &argv);

  SDL_GL_SetAttribute (SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute (SDL_GL_CONTEXT_MINOR_VERSION, 0);

  /* Create a 640x480 OpenGL screen */
  sdl_window =
      SDL_CreateWindow ("SDL and gst-plugins-gl", SDL_WINDOWPOS_UNDEFINED,
      SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_OPENGL);
  if (sdl_window == NULL) {
    fprintf (stderr, "Unable to create OpenGL screen: %s\n", SDL_GetError ());
    SDL_Quit ();
    return -1;
  }

  sdl_gl_context = SDL_GL_CreateContext (sdl_window);
  if (sdl_gl_context == NULL) {
    fprintf (stderr, "Unable to create OpenGL context: %s\n", SDL_GetError ());
    SDL_Quit ();
    return -1;
  }

  loop = g_main_loop_new (NULL, FALSE);

  SDL_GL_MakeCurrent (sdl_window, sdl_gl_context);

  /* Loop, drawing and checking events */
  InitGL (640, 480);
#ifdef WIN32
  gl_context = wglGetCurrentContext ();
  sdl_dc = wglGetCurrentDC ();
  platform = "wgl";
  sdl_gl_display = gst_gl_display_new ();
#else
  SDL_VERSION (&info.version);
  SDL_GetWindowWMInfo (sdl_window, &info);
  sdl_display = info.info.x11.display;
  gl_context = glXGetCurrentContext ();
  platform = "glx";
  sdl_gl_display =
      (GstGLDisplay *) gst_gl_display_x11_new_with_display (sdl_display);
#endif

  sdl_context =
      gst_gl_context_new_wrapped (sdl_gl_display, (guintptr) gl_context,
      gst_gl_platform_from_string (platform), GST_GL_API_OPENGL);

  gst_gl_context_activate (sdl_context, TRUE);

  if (!gst_gl_context_fill_info (sdl_context, &err)) {
    fprintf (stderr, "Failed to fill in wrapped GStreamer context: %s\n",
        err->message);
    g_clear_error (&err);
    SDL_Quit ();
    return -1;
  }

  pipeline =
      GST_PIPELINE (gst_parse_launch
      ("videotestsrc ! video/x-raw, width=320, height=240, framerate=(fraction)30/1 ! "
          "glimagesink name=glimagesink0", NULL));

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message::error", G_CALLBACK (end_stream_cb), loop);
  g_signal_connect (bus, "message::warning", G_CALLBACK (end_stream_cb), loop);
  g_signal_connect (bus, "message::eos", G_CALLBACK (end_stream_cb), loop);
  gst_bus_enable_sync_message_emission (bus);
  g_signal_connect (bus, "sync-message", G_CALLBACK (sync_bus_call), NULL);

  glimagesink = gst_bin_get_by_name (GST_BIN (pipeline), "glimagesink0");
  g_signal_connect (G_OBJECT (glimagesink), "client-draw",
      G_CALLBACK (on_client_draw), NULL);
  gst_object_unref (glimagesink);

  /* NULL to PAUSED state pipeline to make sure the gst opengl context is created and
   * shared with the sdl one */
  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PAUSED);

  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PLAYING);

  g_timeout_add (100, update_sdl_scene, pipeline);

  g_main_loop_run (loop);

  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_NULL);
  gst_object_unref (pipeline);

  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);

  gst_gl_context_activate (sdl_context, FALSE);
  gst_object_unref (sdl_context);
  gst_object_unref (sdl_gl_display);

  SDL_GL_DeleteContext (gl_context);

  SDL_DestroyWindow (sdl_window);

  SDL_Quit ();

  return 0;
}
