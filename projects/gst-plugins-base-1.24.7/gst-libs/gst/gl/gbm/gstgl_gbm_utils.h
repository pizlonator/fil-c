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

#ifndef __GST_GL_GBM_PRIVATE_H__
#define __GST_GL_GBM_PRIVATE_H__

#include <gbm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gst/gst.h>


typedef struct _GstGLDRMFramebuffer GstGLDRMFramebuffer;

struct _GstGLDRMFramebuffer
{
  struct gbm_bo *bo;
  guint32 fb_id;
};

const gchar* gst_gl_gbm_get_name_for_drm_connector (drmModeConnector * connector);
const gchar* gst_gl_gbm_get_name_for_drm_encoder (drmModeEncoder * encoder);
const gchar* gst_gl_gbm_format_to_string (guint32 format);
int gst_gl_gbm_depth_from_format (guint32 format);
int gst_gl_gbm_bpp_from_format (guint32 format);

GstGLDRMFramebuffer* gst_gl_gbm_drm_fb_get_from_bo (struct gbm_bo *bo);

int gst_gl_gbm_find_and_open_drm_node (void);


#endif /* __GST_GL_DISPLAY_GBM_UTILS_H__ */
