/*
 * GStreamer
 * Copyright (C) 2018 Carlos Rafael Giani <dv@pseudoterminal.org>
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

#ifndef __GST_GL_DISPLAY_GBM_H__
#define __GST_GL_DISPLAY_GBM_H__

#include <gbm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gst/gst.h>
#include <gst/gl/gstgldisplay.h>
#include <gst/gl/egl/gstegl.h>

G_BEGIN_DECLS

GType gst_gl_display_gbm_get_type (void);

#define GST_TYPE_GL_DISPLAY_GBM             (gst_gl_display_gbm_get_type())
#define GST_GL_DISPLAY_GBM(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_DISPLAY_GBM,GstGLDisplayGBM))
#define GST_GL_DISPLAY_GBM_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GL_DISPLAY_GBM,GstGLDisplayGBMClass))
#define GST_IS_GL_DISPLAY_GBM(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_DISPLAY_GBM))
#define GST_IS_GL_DISPLAY_GBM_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GL_DISPLAY_GBM))
#define GST_GL_DISPLAY_GBM_CAST(obj)        ((GstGLDisplayGBM*)(obj))

#define GST_GL_DISPLAY_GBM_PRIVATE(obj)     (((GstGLDisplayGBM*)(obj))->priv)

typedef struct _GstGLDisplayGBM GstGLDisplayGBM;
typedef struct _GstGLDisplayGBMClass GstGLDisplayGBMClass;

struct _GstGLDisplayGBM
{
  GstGLDisplay parent;

  /*< private >*/

  int drm_fd;
  drmModeRes *drm_mode_resources;
  drmModeConnector *drm_mode_connector;
  drmModeModeInfo *drm_mode_info;
  int crtc_index;
  guint32 crtc_id;

  struct gbm_device *gbm_dev;

  gpointer _reserved[GST_PADDING];
};

struct _GstGLDisplayGBMClass
{
  GstGLDisplayClass object_class;

  /*< private >*/
  gpointer _reserved[GST_PADDING_LARGE];
};

GstGLDisplayGBM *gst_gl_display_gbm_new (void);

G_END_DECLS

#endif /* __GST_GL_DISPLAY_GBM_H__ */
