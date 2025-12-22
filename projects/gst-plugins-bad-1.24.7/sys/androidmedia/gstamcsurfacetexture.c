/*
 * Copyright (C) 2013, Fluendo S.A.
 *   Author: Andoni Morales <amorales@fluendo.com>
 *
 * Copyright (C) 2014,2018 Collabora Ltd.
 *   Author: Matthieu Bouron <matthieu.bouron@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstamcsurfacetexture.h"

G_DEFINE_ABSTRACT_TYPE (GstAmcSurfaceTexture, gst_amc_surface_texture,
    G_TYPE_OBJECT);

static void
gst_amc_surface_texture_init (GstAmcSurfaceTexture * self)
{
}

static void
gst_amc_surface_texture_class_init (GstAmcSurfaceTextureClass * klass)
{
}

gboolean
gst_amc_surface_texture_update_tex_image (GstAmcSurfaceTexture * self,
    GError ** err)
{
  GstAmcSurfaceTextureClass *klass;
  klass = GST_AMC_SURFACE_TEXTURE_GET_CLASS (self);
  return klass->update_tex_image (self, err);
}

gboolean
gst_amc_surface_texture_detach_from_gl_context (GstAmcSurfaceTexture * self,
    GError ** err)
{
  GstAmcSurfaceTextureClass *klass;
  klass = GST_AMC_SURFACE_TEXTURE_GET_CLASS (self);
  return klass->detach_from_gl_context (self, err);
}

gboolean
gst_amc_surface_texture_attach_to_gl_context (GstAmcSurfaceTexture * self,
    gint texture_id, GError ** err)
{
  GstAmcSurfaceTextureClass *klass;
  klass = GST_AMC_SURFACE_TEXTURE_GET_CLASS (self);
  return klass->attach_to_gl_context (self, texture_id, err);
}

gboolean
gst_amc_surface_texture_get_transform_matrix (GstAmcSurfaceTexture * self,
    gfloat * matrix, GError ** err)
{
  GstAmcSurfaceTextureClass *klass;
  klass = GST_AMC_SURFACE_TEXTURE_GET_CLASS (self);
  return klass->get_transform_matrix (self, matrix, err);
}

gboolean
gst_amc_surface_texture_get_timestamp (GstAmcSurfaceTexture * self,
    gint64 * result, GError ** err)
{
  GstAmcSurfaceTextureClass *klass;
  klass = GST_AMC_SURFACE_TEXTURE_GET_CLASS (self);
  return klass->get_timestamp (self, result, err);
}

gboolean
gst_amc_surface_texture_release (GstAmcSurfaceTexture * self, GError ** err)
{
  GstAmcSurfaceTextureClass *klass;
  klass = GST_AMC_SURFACE_TEXTURE_GET_CLASS (self);
  return klass->release (self, err);
}

gboolean
gst_amc_surface_texture_set_on_frame_available_callback (GstAmcSurfaceTexture *
    self, GstAmcSurfaceTextureOnFrameAvailableCallback callback,
    gpointer user_data, GError ** err)
{
  GstAmcSurfaceTextureClass *klass;
  klass = GST_AMC_SURFACE_TEXTURE_GET_CLASS (self);
  return klass->set_on_frame_available_callback (self, callback, user_data,
      err);
}
