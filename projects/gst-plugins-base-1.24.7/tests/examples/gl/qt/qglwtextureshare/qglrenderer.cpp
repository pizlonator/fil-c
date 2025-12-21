/*
 * GStreamer
 * Copyright (C) 2009 Julien Isorce <julien.isorce@gmail.com>
 * Copyright (C) 2009 Andrey Nechypurenko <andreynech@gmail.com>
 * Copyright (C) 2010 Nuno Santos <nunosantos@imaginando.net>
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

#include <QGLWidget>
#include <QApplication>
#include <QDebug>
#include <QCloseEvent>

#include <gst/video/video.h>
#include <gst/gl/gl.h>
#include <gst/gl/gstglfuncs.h>

#if GST_GL_HAVE_PLATFORM_GLX
#include <GL/glx.h>
#include <QX11Info>
#include <gst/gl/x11/gstgldisplay_x11.h>
#endif

#include "gstthread.h"
#include "qglrenderer.h"
#include "pipeline.h"

#if defined(Q_WS_MAC)
extern void *qt_current_nsopengl_context ();
#endif

QGLRenderer::QGLRenderer (const QString & videoLocation, QWidget * parent)
    :
QGLWidget (parent),
videoLoc (videoLocation),
gst_thread (NULL),
closing (false),
frame (NULL)
{
  move (20, 10);
  resize (640, 480);
}

QGLRenderer::~QGLRenderer ()
{
}

void
QGLRenderer::initializeGL ()
{
  GstGLContext *context;
  GstGLDisplay *display;

#if GST_GL_HAVE_PLATFORM_GLX
  display =
      (GstGLDisplay *) gst_gl_display_x11_new_with_display (QX11Info::
      display ());
#else
  display = gst_gl_display_new ();
#endif

  /* FIXME: Allow the choice at runtime */
#if GST_GL_HAVE_PLATFORM_WGL
  context =
      gst_gl_context_new_wrapped (display, (guintptr) wglGetCurrentContext (),
      GST_GL_PLATFORM_WGL, GST_GL_API_OPENGL);
#elif GST_GL_HAVE_PLATFORM_CGL
  context =
      gst_gl_context_new_wrapped (display,
      (guintptr) qt_current_nsopengl_context (), GST_GL_PLATFORM_CGL,
      GST_GL_API_OPENGL);
#elif GST_GL_HAVE_PLATFORM_GLX
  context =
      gst_gl_context_new_wrapped (display, (guintptr) glXGetCurrentContext (),
      GST_GL_PLATFORM_GLX, GST_GL_API_OPENGL);
#endif
  gst_object_unref (display);

  // We need to unset Qt context before initializing gst-gl plugin.
  // Otherwise the attempt to share gst-gl context with Qt will fail.
  this->doneCurrent ();
  this->gst_thread =
      new GstThread (display, context, this->videoLoc,
      SLOT (newFrame ()), this);
  this->makeCurrent ();

  QObject::connect (this->gst_thread, SIGNAL (finished ()),
      this, SLOT (close ()));
  QObject::connect (this, SIGNAL (closeRequested ()),
      this->gst_thread, SLOT (stop ()), Qt::QueuedConnection);

  qglClearColor (QApplication::palette ().color (QPalette::Active,
          QPalette::Window));
  //glShadeModel(GL_FLAT);
  //glEnable(GL_DEPTH_TEST);
  //glEnable(GL_CULL_FACE);
  glEnable (GL_TEXTURE_2D);     // Enable Texture Mapping

  this->gst_thread->start ();
}

void
QGLRenderer::resizeGL (int width, int height)
{
  // Reset The Current Viewport And Perspective Transformation
  glViewport (0, 0, width, height);

  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();

  glMatrixMode (GL_MODELVIEW);
}

void
QGLRenderer::newFrame ()
{
  Pipeline *pipeline = this->gst_thread->getPipeline ();
  if (!pipeline)
    return;

  /* frame is initialized as null */
  if (this->frame)
    pipeline->queue_output_buf.put (this->frame);

  this->frame = pipeline->queue_input_buf.get ();

  /* direct call to paintGL (no queued) */
  this->updateGL ();
}

static void
flushGstreamerGL (GstGLContext * context, void *data G_GNUC_UNUSED)
{
  context->gl_vtable->Flush ();
}

void
QGLRenderer::paintGL ()
{
  static GLfloat xrot = 0;
  static GLfloat yrot = 0;
  static GLfloat zrot = 0;

  if (this->frame) {
    guint tex_id;
    GstMemory *mem;
    GstVideoInfo v_info;
    GstVideoFrame v_frame;
    GstVideoMeta *v_meta;

    mem = gst_buffer_peek_memory (this->frame, 0);
    v_meta = gst_buffer_get_video_meta (this->frame);

    Q_ASSERT (gst_is_gl_memory (mem));

    GstGLMemory *gl_memory = (GstGLMemory *) mem;

    gst_gl_context_thread_add (gl_memory->mem.context, flushGstreamerGL, NULL);

    gst_video_info_set_format (&v_info, v_meta->format, v_meta->width,
        v_meta->height);

    gst_video_frame_map (&v_frame, &v_info, this->frame,
        (GstMapFlags) (GST_MAP_READ | GST_MAP_GL));

    tex_id = *(guint *) v_frame.data[0];

    glEnable (GL_DEPTH_TEST);

    glEnable (GL_TEXTURE_2D);
    glBindTexture (GL_TEXTURE_2D, tex_id);
    if (glGetError () != GL_NO_ERROR) {
      qDebug ("failed to bind texture that comes from gst-gl");
      emit closeRequested ();
      return;
    }

    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity ();

    glScalef (0.5f, 0.5f, 0.5f);

    glRotatef (xrot, 1.0f, 0.0f, 0.0f);
    glRotatef (yrot, 0.0f, 1.0f, 0.0f);
    glRotatef (zrot, 0.0f, 0.0f, 1.0f);

    glBegin (GL_QUADS);
    // Front Face
    glTexCoord2f (1.0f, 0.0f);
    glVertex3f (-1.0f, -1.0f, 1.0f);
    glTexCoord2f (0.0f, 0.0f);
    glVertex3f (1.0f, -1.0f, 1.0f);
    glTexCoord2f (0.0f, 1.0f);
    glVertex3f (1.0f, 1.0f, 1.0f);
    glTexCoord2f (1.0f, 1.0f);
    glVertex3f (-1.0f, 1.0f, 1.0f);
    // Back Face
    glTexCoord2f (0.0f, 0.0f);
    glVertex3f (-1.0f, -1.0f, -1.0f);
    glTexCoord2f (0.0f, 1.0f);
    glVertex3f (-1.0f, 1.0f, -1.0f);
    glTexCoord2f (1.0f, 1.0f);
    glVertex3f (1.0f, 1.0f, -1.0f);
    glTexCoord2f (1.0f, 0.0f);
    glVertex3f (1.0f, -1.0f, -1.0f);
    // Top Face
    glTexCoord2f (1.0f, 1.0f);
    glVertex3f (-1.0f, 1.0f, -1.0f);
    glTexCoord2f (1.0f, 0.0f);
    glVertex3f (-1.0f, 1.0f, 1.0f);
    glTexCoord2f (0.0f, 0.0f);
    glVertex3f (1.0f, 1.0f, 1.0f);
    glTexCoord2f (0.0f, 1.0f);
    glVertex3f (1.0f, 1.0f, -1.0f);
    // Bottom Face
    glTexCoord2f (1.0f, 0.0f);
    glVertex3f (-1.0f, -1.0f, -1.0f);
    glTexCoord2f (0.0f, 0.0f);
    glVertex3f (1.0f, -1.0f, -1.0f);
    glTexCoord2f (0.0f, 1.0f);
    glVertex3f (1.0f, -1.0f, 1.0f);
    glTexCoord2f (1.0f, 1.0f);
    glVertex3f (-1.0f, -1.0f, 1.0f);
    // Right face
    glTexCoord2f (0.0f, 0.0f);
    glVertex3f (1.0f, -1.0f, -1.0f);
    glTexCoord2f (0.0f, 1.0f);
    glVertex3f (1.0f, 1.0f, -1.0f);
    glTexCoord2f (1.0f, 1.0f);
    glVertex3f (1.0f, 1.0f, 1.0f);
    glTexCoord2f (1.0f, 0.0f);
    glVertex3f (1.0f, -1.0f, 1.0f);
    // Left Face
    glTexCoord2f (1.0f, 0.0f);
    glVertex3f (-1.0f, -1.0f, -1.0f);
    glTexCoord2f (0.0f, 0.0f);
    glVertex3f (-1.0f, -1.0f, 1.0f);
    glTexCoord2f (0.0f, 1.0f);
    glVertex3f (-1.0f, 1.0f, 1.0f);
    glTexCoord2f (1.0f, 1.0f);
    glVertex3f (-1.0f, 1.0f, -1.0f);
    glEnd ();

    xrot += 0.3f;
    yrot += 0.2f;
    zrot += 0.4f;

    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glBindTexture (GL_TEXTURE_2D, 0);

    gst_video_frame_unmap (&v_frame);
  }
}

void
QGLRenderer::closeEvent (QCloseEvent * event)
{
  if (this->closing == false) {
    this->closing = true;
    emit closeRequested ();
    event->ignore ();
  }
}
