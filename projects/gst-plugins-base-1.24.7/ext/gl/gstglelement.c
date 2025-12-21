/*
 * GStreamer
 * Copyright (C) 2003 Julien Moutte <julien@moutte.net>
 * Copyright (C) 2005,2006,2007 David A. Schleef <ds@schleef.org>
 * Copyright (C) 2008 Julien Isorce <julien.isorce@gmail.com>
 * Copyright (C) 2008 Filippo Argiolas <filippo.argiolas@gmail.com>
 * Copyright (C) 2020 Huawei Technologies Co., Ltd.
 *   @Author: Stéphane Cerveau <scerveau@collabora.com>
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

#include "gstglelements.h"

#include "gstglmixerbin.h"
#include "gstglvideomixer.h"
#include "gstglstereomix.h"

#if GST_GL_HAVE_WINDOW_COCOA
/* avoid including Cocoa/CoreFoundation from a C file... */
extern GType gst_ca_opengl_layer_sink_bin_get_type (void);
#endif

#if GST_GL_HAVE_WINDOW_DISPMANX
extern void bcm_host_init (void);
#endif

#if GST_GL_HAVE_WINDOW_X11
#include <X11/Xlib.h>
#endif

#define GST_CAT_DEFAULT gst_gl_gstgl_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

void
gl_element_init (GstPlugin * plugin)
{
  static gsize res = FALSE;
  if (g_once_init_enter (&res)) {
    GST_DEBUG_CATEGORY_INIT (gst_gl_gstgl_debug, "gstopengl", 0, "gstopengl");

#if GST_GL_HAVE_WINDOW_DISPMANX
    GST_DEBUG ("Initialize BCM host");
    bcm_host_init ();
#endif

#if GST_GL_HAVE_WINDOW_X11
    if (g_getenv ("GST_GL_XINITTHREADS") || g_getenv ("GST_XINITTHREADS"))
      XInitThreads ();
#endif
    g_once_init_leave (&res, TRUE);
  }
}
