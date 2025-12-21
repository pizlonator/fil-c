/*
 * GStreamer
 * Copyright (C) 2014 Matthew Waters <ystreet00@gmail.com>
 * Copyright (C) 2015 Freescale Semiconductor <b55597@freescale.com>
 * Copyright (C) 2017 Sebastian Dröge <sebastian@centricular.com>
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

#include "gstgldisplay_viv_fb.h"

GST_DEBUG_CATEGORY_STATIC (gst_gl_display_debug);
#define GST_CAT_DEFAULT gst_gl_display_debug

G_DEFINE_TYPE (GstGLDisplayVivFB, gst_gl_display_viv_fb, GST_TYPE_GL_DISPLAY);

static void gst_gl_display_viv_fb_finalize (GObject * object);
static guintptr gst_gl_display_viv_fb_get_handle (GstGLDisplay * display);

static void
gst_gl_display_viv_fb_class_init (GstGLDisplayVivFBClass * klass)
{
  GST_GL_DISPLAY_CLASS (klass)->get_handle =
      GST_DEBUG_FUNCPTR (gst_gl_display_viv_fb_get_handle);

  G_OBJECT_CLASS (klass)->finalize = gst_gl_display_viv_fb_finalize;
}

static void
gst_gl_display_viv_fb_init (GstGLDisplayVivFB * display_viv_fb)
{
  GstGLDisplay *display = (GstGLDisplay *) display_viv_fb;

  display->type = GST_GL_DISPLAY_TYPE_VIV_FB;

  display_viv_fb->disp_idx = 0;
  display_viv_fb->display = NULL;
}

static void
gst_gl_display_viv_fb_finalize (GObject * object)
{
  // We don't destroy the FB Display - it causes crashes in applications
  // because Vivante doesn't seem to do any refcounting

  G_OBJECT_CLASS (gst_gl_display_viv_fb_parent_class)->finalize (object);
}

/**
 * gst_gl_display_viv_fb_new:
 * @disp_idx: a display index
 *
 * Create a new #GstGLDisplayVivFB from the FB display index.
 *
 * Returns: (transfer full) (nullable): a new #GstGLDisplayVivFB or %NULL
 */
GstGLDisplayVivFB *
gst_gl_display_viv_fb_new (gint disp_idx)
{
  GstGLDisplayVivFB *display;

  GST_DEBUG_CATEGORY_GET (gst_gl_display_debug, "gldisplay");

  GST_DEBUG ("creating Vivante FB EGL display %d", disp_idx);

  display = g_object_new (GST_TYPE_GL_DISPLAY_VIV_FB, NULL);
  gst_object_ref_sink (display);
  display->disp_idx = disp_idx;
  display->display = fbGetDisplayByIndex (display->disp_idx);
  if (!display->display) {
    GST_ERROR ("Failed to open Vivante FB display %d", disp_idx);
    return NULL;
  }

  GST_DEBUG ("Created Vivante FB EGL display %p", (gpointer) display->display);

  return display;
}

static guintptr
gst_gl_display_viv_fb_get_handle (GstGLDisplay * display)
{
  return (guintptr) GST_GL_DISPLAY_VIV_FB (display)->display;
}
