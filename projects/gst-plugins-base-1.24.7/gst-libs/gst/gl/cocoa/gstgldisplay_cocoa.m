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

#if !defined(MAC_OS_X_VERSION_MAX_ALLOWED) || MAC_OS_X_VERSION_MAX_ALLOWED >= 1014
# define GL_SILENCE_DEPRECATION
#endif

#include <Cocoa/Cocoa.h>

#include <gst/gl/cocoa/gstgldisplay_cocoa.h>

GST_DEBUG_CATEGORY_STATIC (gst_gl_display_debug);
#define GST_CAT_DEFAULT gst_gl_display_debug

G_DEFINE_TYPE (GstGLDisplayCocoa, gst_gl_display_cocoa, GST_TYPE_GL_DISPLAY);

static guintptr gst_gl_display_cocoa_get_handle (GstGLDisplay * display);

#if MAC_OS_X_VERSION_MAX_ALLOWED < 101200
#define NSEventMaskAny                       NSAnyEventMask
#endif

static void
gst_gl_display_cocoa_class_init (GstGLDisplayCocoaClass * klass)
{
  GST_GL_DISPLAY_CLASS (klass)->get_handle =
      GST_DEBUG_FUNCPTR (gst_gl_display_cocoa_get_handle);
}

static void
gst_gl_display_cocoa_init (GstGLDisplayCocoa * display_cocoa)
{
  GstGLDisplay *display = (GstGLDisplay *) display_cocoa;
  display->type = GST_GL_DISPLAY_TYPE_COCOA;
}

/**
 * gst_gl_display_cocoa_new:
 *
 * Create a new #GstGLDisplayCocoa.
 *
 * Returns: (transfer full): a new #GstGLDisplayCocoa
 */
GstGLDisplayCocoa *
gst_gl_display_cocoa_new (void)
{
  GstGLDisplayCocoa *display;

  GST_DEBUG_CATEGORY_GET (gst_gl_display_debug, "gldisplay");

  if (NSApp == nil)
    g_warning ("An NSApplication needs to be running on the main thread "
        "to ensure correct behaviour on macOS. Use gst_macos_main() or call "
        "[NSApplication sharedApplication] in your code before using this element.");

  display = g_object_new (GST_TYPE_GL_DISPLAY_COCOA, NULL);
  gst_object_ref_sink (display);

  return display;
}

static guintptr
gst_gl_display_cocoa_get_handle (GstGLDisplay * display)
{
  return (guintptr) NSApp;
}
