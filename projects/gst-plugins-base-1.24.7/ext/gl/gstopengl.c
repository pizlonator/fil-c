/*
 * GStreamer
 * Copyright (C) 2003 Julien Moutte <julien@moutte.net>
 * Copyright (C) 2005,2006,2007 David A. Schleef <ds@schleef.org>
 * Copyright (C) 2008 Julien Isorce <julien.isorce@gmail.com>
 * Copyright (C) 2008 Filippo Argiolas <filippo.argiolas@gmail.com>
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

/**
 * SECTION:plugin-opengl
 * @title: GstOpengl
 *
 * Cross-platform OpenGL plugin.
 *
 * ## Debugging
 *
 * ## Examples
 * |[
 * gst-launch-1.0 --gst-debug=gldisplay:3 videotestsrc ! glimagesink
 * ]| A debugging pipeline.
  |[
 * GST_DEBUG=gl*:6 gst-launch-1.0 videotestsrc ! glimagesink
 * ]| A debugging pipelines related to shaders.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstglelements.h"

#include "gstglmixerbin.h"
#include "gstglvideomixer.h"
#include "gstglstereomix.h"

/* Register filters that make up the gstgl plugin */
static gboolean
plugin_init (GstPlugin * plugin)
{
  gboolean ret = FALSE;

  ret |= GST_ELEMENT_REGISTER (glimagesink, plugin);
  ret |= GST_ELEMENT_REGISTER (glimagesinkelement, plugin);
  ret |= GST_ELEMENT_REGISTER (glupload, plugin);
  ret |= GST_ELEMENT_REGISTER (gldownload, plugin);
  ret |= GST_ELEMENT_REGISTER (glcolorconvert, plugin);
  ret |= GST_ELEMENT_REGISTER (glcolorbalance, plugin);
  ret |= GST_ELEMENT_REGISTER (glfilterbin, plugin);
  ret |= GST_ELEMENT_REGISTER (glsinkbin, plugin);
  ret |= GST_ELEMENT_REGISTER (glsrcbin, plugin);
  ret |= GST_ELEMENT_REGISTER (glmixerbin, plugin);
  ret |= GST_ELEMENT_REGISTER (glfiltercube, plugin);
#ifdef HAVE_GRAPHENE
  ret |= GST_ELEMENT_REGISTER (gltransformation, plugin);
  ret |= GST_ELEMENT_REGISTER (glvideoflip, plugin);
#endif
  ret |= GST_ELEMENT_REGISTER (gleffects, plugin);
  ret |= GST_ELEMENT_REGISTER (glcolorscale, plugin);
  ret |= GST_ELEMENT_REGISTER (glvideomixer, plugin);
  ret |= GST_ELEMENT_REGISTER (glvideomixerelement, plugin);
  ret |= GST_ELEMENT_REGISTER (glshader, plugin);
  ret |= GST_ELEMENT_REGISTER (glfilterapp, plugin);
  ret |= GST_ELEMENT_REGISTER (glviewconvert, plugin);
  ret |= GST_ELEMENT_REGISTER (glstereosplit, plugin);
  ret |= GST_ELEMENT_REGISTER (glstereomix, plugin);
  ret |= GST_ELEMENT_REGISTER (gltestsrc, plugin);
  ret |= GST_ELEMENT_REGISTER (gldeinterlace, plugin);
  ret |= GST_ELEMENT_REGISTER (glalpha, plugin);
  ret |= GST_ELEMENT_REGISTER (gloverlaycompositor, plugin);
#if defined(HAVE_JPEG) && defined(HAVE_PNG)
  ret |= GST_ELEMENT_REGISTER (gloverlay, plugin);
#endif
#if GST_GL_HAVE_OPENGL
  ret |= GST_ELEMENT_REGISTER (glfilterglass, plugin);
#if 0
  ret |= GST_ELEMENT_REGISTER (glfilterreflectedscreen, plugin);
#endif
  ret |= GST_ELEMENT_REGISTER (glmosaic, plugin);
#ifdef HAVE_PNG
  ret |= GST_ELEMENT_REGISTER (gldifferencematte, plugin);
#if 0
  ret |= GST_ELEMENT_REGISTER (glbumper, plugin);
#endif
#endif /* HAVE_PNG */
#endif /* GST_GL_HAVE_OPENGL */
#if GST_GL_HAVE_WINDOW_COCOA
  ret |= GST_ELEMENT_REGISTER (caopengllayersink, plugin);
#endif

  return ret;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    opengl,
    "OpenGL plugin",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
