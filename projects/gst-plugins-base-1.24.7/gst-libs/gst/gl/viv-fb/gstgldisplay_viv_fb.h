/*
 * GStreamer
 * Copyright (C) 2014 Matthew Waters <ystreet00@gmail.com>
 * Copyright (C) 2015 Freescale Semiconductor <b55597@freescale.com>
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

#ifndef __GST_GL_DISPLAY_VIV_FB_H__
#define __GST_GL_DISPLAY_VIV_FB_H__

#include <gst/gst.h>
#include <gst/gl/gstgldisplay.h>
#include <gst/gl/egl/gstegl.h>

G_BEGIN_DECLS

GST_GL_API
GType gst_gl_display_viv_fb_get_type (void);

#define GST_TYPE_GL_DISPLAY_VIV_FB             (gst_gl_display_viv_fb_get_type())
#define GST_GL_DISPLAY_VIV_FB(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_DISPLAY_VIV_FB,GstGLDisplayVivFB))
#define GST_GL_DISPLAY_VIV_FB_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GL_DISPLAY_VIV_FB,GstGLDisplayVivFBClass))
#define GST_IS_GL_DISPLAY_VIV_FB(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_DISPLAY_VIV_FB))
#define GST_IS_GL_DISPLAY_VIV_FB_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GL_DISPLAY_VIV_FB))
#define GST_GL_DISPLAY_VIV_FB_CAST(obj)        ((GstGLDisplayVivFB*)(obj))

typedef struct _GstGLDisplayVivFB GstGLDisplayVivFB;
typedef struct _GstGLDisplayVivFBClass GstGLDisplayVivFBClass;

/**
 * GstGLDisplayVivFB:
 *
 * the contents of a #GstGLDisplayVivFB are private and should only be accessed
 * through the provided API
 */
struct _GstGLDisplayVivFB
{
  GstGLDisplay          parent;

  /*< private >*/
  gint disp_idx;
  EGLNativeDisplayType display;
};

struct _GstGLDisplayVivFBClass
{
  GstGLDisplayClass object_class;
};

GST_GL_API
GstGLDisplayVivFB *gst_gl_display_viv_fb_new (gint disp_idx);

G_END_DECLS

#endif /* __GST_GL_DISPLAY_VIV_FB_H__ */
