/*
 * Copyright (C) 2018 Collabora Ltd.
 *   Author: Xavier Claessens <xavier.claessens@collabora.com>
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

#include "gstamc-surfacetexture-ml.h"

#include <gst/gst.h>
#include <ml_media_surface_texture.h>

struct _GstAmcSurfaceTextureML
{
  GObject parent;

  MLHandle handle;
  GstAmcSurfaceTextureOnFrameAvailableCallback callback;
  gpointer user_data;
};

G_DEFINE_TYPE (GstAmcSurfaceTextureML, gst_amc_surface_texture_ml,
    GST_TYPE_AMC_SURFACE_TEXTURE);

static gboolean
gst_amc_surface_texture_ml_update_tex_image (GstAmcSurfaceTexture * base,
    GError ** err)
{
  GstAmcSurfaceTextureML *self = GST_AMC_SURFACE_TEXTURE_ML (base);
  MLResult result;

  result = MLMediaSurfaceTextureUpdateTexImage (self->handle);
  if (result != MLResult_Ok) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to update tex image: %d", result);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_amc_surface_texture_ml_detach_from_gl_context (GstAmcSurfaceTexture * base,
    GError ** err)
{
  GstAmcSurfaceTextureML *self = GST_AMC_SURFACE_TEXTURE_ML (base);
  MLResult result;

  result = MLMediaSurfaceTextureDetachFromGLContext (self->handle);
  if (result != MLResult_Ok) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to detach from gl context: %d", result);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_amc_surface_texture_ml_attach_to_gl_context (GstAmcSurfaceTexture * base,
    gint texture_id, GError ** err)
{
  GstAmcSurfaceTextureML *self = GST_AMC_SURFACE_TEXTURE_ML (base);
  MLResult result;

  result = MLMediaSurfaceTextureAttachToGLContext (self->handle, texture_id);
  if (result != MLResult_Ok) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to attach to gl context: %d", result);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_amc_surface_texture_ml_get_transform_matrix (GstAmcSurfaceTexture * base,
    gfloat * matrix, GError ** err)
{
  GstAmcSurfaceTextureML *self = GST_AMC_SURFACE_TEXTURE_ML (base);
  MLResult result;

  result = MLMediaSurfaceTextureGetTransformationMatrix (self->handle, matrix);
  if (result != MLResult_Ok) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to get transformation matrix: %d", result);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_amc_surface_texture_ml_get_timestamp (GstAmcSurfaceTexture * base,
    gint64 * timestamp, GError ** err)
{
  GstAmcSurfaceTextureML *self = GST_AMC_SURFACE_TEXTURE_ML (base);
  MLResult result;

  result = MLMediaSurfaceTextureGetTimestamp (self->handle, timestamp);
  if (result != MLResult_Ok) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Failed to get timestamp: %d", result);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_amc_surface_texture_ml_release (GstAmcSurfaceTexture * base, GError ** err)
{
  /* Nothing to do here, resources will be released when this object gets
   * destroyed. */
  return TRUE;
}

static gboolean
    gst_amc_surface_texture_ml_set_on_frame_available_callback
    (GstAmcSurfaceTexture * base,
    GstAmcSurfaceTextureOnFrameAvailableCallback callback, gpointer user_data,
    GError ** err)
{
  GstAmcSurfaceTextureML *self = GST_AMC_SURFACE_TEXTURE_ML (base);

  self->callback = callback;
  self->user_data = user_data;
  return TRUE;
}

static void
gst_amc_surface_texture_ml_dispose (GObject * object)
{
  GstAmcSurfaceTextureML *self = GST_AMC_SURFACE_TEXTURE_ML (object);

  MLMediaSurfaceTextureSetOnFrameAvailableCallback (self->handle, NULL, NULL);
  MLMediaSurfaceTextureDestroy (&self->handle);

  G_OBJECT_CLASS (gst_amc_surface_texture_ml_parent_class)->dispose (object);
}

static void
gst_amc_surface_texture_ml_class_init (GstAmcSurfaceTextureMLClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstAmcSurfaceTextureClass *surface_texture_class =
      GST_AMC_SURFACE_TEXTURE_CLASS (klass);

  gobject_class->dispose = gst_amc_surface_texture_ml_dispose;

  surface_texture_class->update_tex_image =
      gst_amc_surface_texture_ml_update_tex_image;
  surface_texture_class->detach_from_gl_context =
      gst_amc_surface_texture_ml_detach_from_gl_context;
  surface_texture_class->attach_to_gl_context =
      gst_amc_surface_texture_ml_attach_to_gl_context;
  surface_texture_class->get_transform_matrix =
      gst_amc_surface_texture_ml_get_transform_matrix;
  surface_texture_class->get_timestamp =
      gst_amc_surface_texture_ml_get_timestamp;
  surface_texture_class->release = gst_amc_surface_texture_ml_release;
  surface_texture_class->set_on_frame_available_callback =
      gst_amc_surface_texture_ml_set_on_frame_available_callback;
}

static void
on_frame_available_cb (MLHandle handle, gpointer user_data)
{
  GstAmcSurfaceTextureML *self = user_data;

  if (self->callback != NULL)
    self->callback (GST_AMC_SURFACE_TEXTURE (self), self->user_data);
}

static void
gst_amc_surface_texture_ml_init (GstAmcSurfaceTextureML * self)
{
  MLResult result;

  result =
      MLMediaSurfaceTextureCreate (MLMediaSurfaceTextureBackend_OpenGL,
      &self->handle);
  if (result != MLResult_Ok) {
    GST_ERROR ("MLMediaSurfaceTextureCreate returned error: %d", result);
    return;
  }

  result =
      MLMediaSurfaceTextureSetOnFrameAvailableCallback (self->handle,
      on_frame_available_cb, self);
  if (result != MLResult_Ok) {
    GST_ERROR
        ("MLMediaSurfaceTextureSetOnFrameAvailableCallback returned error: %d",
        result);
    return;
  }
}

GstAmcSurfaceTextureML *
gst_amc_surface_texture_ml_new (GError ** err)
{
  return g_object_new (GST_TYPE_AMC_SURFACE_TEXTURE_ML, NULL);
}

MLHandle
gst_amc_surface_texture_ml_get_handle (GstAmcSurfaceTextureML * self)
{
  return self->handle;
}
