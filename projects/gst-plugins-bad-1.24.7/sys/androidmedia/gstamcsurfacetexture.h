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

#ifndef __GST_AMC_SURFACE_TEXTURE_H__
#define __GST_AMC_SURFACE_TEXTURE_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GST_TYPE_AMC_SURFACE_TEXTURE gst_amc_surface_texture_get_type ()
G_DECLARE_DERIVABLE_TYPE (GstAmcSurfaceTexture, gst_amc_surface_texture, GST, AMC_SURFACE_TEXTURE, GObject)

typedef void (* GstAmcSurfaceTextureOnFrameAvailableCallback) (GstAmcSurfaceTexture * self, gpointer user_data);

struct _GstAmcSurfaceTextureClass
{
  GObjectClass parent_class;

  gboolean (* update_tex_image)        (GstAmcSurfaceTexture *texture,
                                        GError ** err);

  gboolean (* detach_from_gl_context)  (GstAmcSurfaceTexture *texture,
                                        GError ** err);

  gboolean (* attach_to_gl_context)    (GstAmcSurfaceTexture *texture,
                                        gint index,
                                        GError ** err);

  gboolean (* get_transform_matrix)    (GstAmcSurfaceTexture *texture,
                                        gfloat *matrix,
                                        GError ** err);

  gboolean (* get_timestamp)           (GstAmcSurfaceTexture *texture,
                                        gint64 * result,
                                        GError ** err);

  gboolean (* release)                 (GstAmcSurfaceTexture *texture,
                                        GError ** err);

  gboolean (* set_on_frame_available_callback) (GstAmcSurfaceTexture * self,
                                                GstAmcSurfaceTextureOnFrameAvailableCallback callback,
                                                gpointer user_data,
                                                GError ** err);
};

gboolean gst_amc_surface_texture_update_tex_image        (GstAmcSurfaceTexture *texture,
                                                         GError ** err);

gboolean gst_amc_surface_texture_detach_from_gl_context  (GstAmcSurfaceTexture *texture,
                                                         GError ** err);

gboolean gst_amc_surface_texture_attach_to_gl_context    (GstAmcSurfaceTexture *texture,
                                                         gint index,
                                                         GError ** err);

gboolean gst_amc_surface_texture_get_transform_matrix    (GstAmcSurfaceTexture *texture,
                                                         gfloat *matrix,
                                                         GError ** err);

gboolean gst_amc_surface_texture_get_timestamp           (GstAmcSurfaceTexture *texture,
                                                         gint64 * result,
                                                         GError ** err);

gboolean gst_amc_surface_texture_release                 (GstAmcSurfaceTexture *texture,
                                                         GError ** err);

gboolean gst_amc_surface_texture_set_on_frame_available_callback (GstAmcSurfaceTexture * self,
                                                                  GstAmcSurfaceTextureOnFrameAvailableCallback callback,
                                                                  gpointer user_data,
                                                                  GError ** err);

G_END_DECLS

#endif
