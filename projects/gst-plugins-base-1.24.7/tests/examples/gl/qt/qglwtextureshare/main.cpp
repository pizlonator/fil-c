/*
 * GStreamer
 * Copyright (C) 2009 Julien Isorce <julien.isorce@gmail.com>
 * Copyright (C) 2009 Andrey Nechypurenko <andreynech@gmail.com>
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

#include <QApplication>
#include "qglrenderer.h"

#include <gst/gst.h>

int
main(int argc, char *argv[])
{
  /* FIXME: port the example to shaders and remove this */
  g_setenv ("GST_GL_API", "opengl", FALSE);

  gst_init (NULL, NULL);
  QApplication a(argc, argv);
  a.connect(&a, SIGNAL(lastWindowClosed()), &a, SLOT(quit()));
  QGLRenderer w(argc > 1 ? argv[1] : "");
  w.setWindowTitle("Texture sharing example");
  w.show();
  return a.exec();
}

