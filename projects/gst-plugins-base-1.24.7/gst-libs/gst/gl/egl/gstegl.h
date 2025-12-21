/*
 * GStreamer
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
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

#ifndef _GST_EGL_H_
#define _GST_EGL_H_

#include <gst/gl/gstglconfig.h>
#include <gst/gl/gl-prelude.h>

#if GST_GL_HAVE_WINDOW_DISPMANX && defined(__GNUC__)
#ifndef __VCCOREVER__
#define __VCCOREVER__ 0x04000000
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wredundant-decls"
#if !defined(__cplusplus)
#pragma GCC optimize ("gnu89-inline")
#endif
#endif

#ifndef __GI_SCANNER__
#ifndef EGL_EGLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES 1
#endif
#endif
#include <EGL/egl.h>
#include <EGL/eglext.h>

#if GST_GL_HAVE_WINDOW_DISPMANX && defined(__GNUC__)
#pragma GCC reset_options
#pragma GCC diagnostic pop
#endif

/* compatibility definitions... */
#if !GST_GL_HAVE_EGLATTRIB
typedef gintptr EGLAttrib;
#endif

#if !GST_GL_HAVE_EGLUINT64KHR
typedef guint64 EGLuint64KHR;
#endif

GST_GL_API
const gchar *   gst_egl_get_error_string             (EGLint err);

#endif /* _GST_EGL_H_ */
