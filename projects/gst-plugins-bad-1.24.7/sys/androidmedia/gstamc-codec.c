/*
 * Copyright (C) 2012,2018 Collabora Ltd.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) 2022 Ratchanan Srirattanamet <peathot@hotmail.com>
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

#include <gst/gst.h>
#include "gstamc-codec.h"

GstAmcCodecVTable *gst_amc_codec_vtable = NULL;

void
gst_amc_buffer_free (GstAmcBuffer * buffer)
{
  g_assert (gst_amc_codec_vtable != NULL);
  return gst_amc_codec_vtable->buffer_free (buffer);
}

gboolean
gst_amc_buffer_set_position_and_limit (GstAmcBuffer * buffer, GError ** err,
    gint position, gint limit)
{
  g_assert (gst_amc_codec_vtable != NULL);
  return gst_amc_codec_vtable->buffer_set_position_and_limit (buffer, err,
      position, limit);
}

GstAmcCodec *
gst_amc_codec_new (const gchar * name, gboolean is_encoder, GError ** err)
{
  g_assert (gst_amc_codec_vtable != NULL);
  return gst_amc_codec_vtable->create (name, is_encoder, err);
}

void
gst_amc_codec_free (GstAmcCodec * codec)
{
  g_assert (gst_amc_codec_vtable != NULL);
  return gst_amc_codec_vtable->free (codec);
}

gboolean
gst_amc_codec_configure (GstAmcCodec * codec, GstAmcFormat * format,
    GstAmcSurfaceTexture * surface_texture, GError ** err)
{
  g_assert (gst_amc_codec_vtable != NULL);
  return gst_amc_codec_vtable->configure (codec, format, surface_texture, err);
}

GstAmcFormat *
gst_amc_codec_get_output_format (GstAmcCodec * codec, GError ** err)
{
  g_assert (gst_amc_codec_vtable != NULL);
  return gst_amc_codec_vtable->get_output_format (codec, err);
}

gboolean
gst_amc_codec_start (GstAmcCodec * codec, GError ** err)
{
  g_assert (gst_amc_codec_vtable != NULL);
  return gst_amc_codec_vtable->start (codec, err);
}

gboolean
gst_amc_codec_stop (GstAmcCodec * codec, GError ** err)
{
  g_assert (gst_amc_codec_vtable != NULL);
  return gst_amc_codec_vtable->stop (codec, err);
}

gboolean
gst_amc_codec_flush (GstAmcCodec * codec, GError ** err)
{
  g_assert (gst_amc_codec_vtable != NULL);
  return gst_amc_codec_vtable->flush (codec, err);
}

gboolean
gst_amc_codec_release (GstAmcCodec * codec, GError ** err)
{
  g_assert (gst_amc_codec_vtable != NULL);
  return gst_amc_codec_vtable->release (codec, err);
}

gboolean
gst_amc_codec_request_key_frame (GstAmcCodec * codec, GError ** err)
{
  g_assert (gst_amc_codec_vtable != NULL);
  return gst_amc_codec_vtable->request_key_frame (codec, err);
}

gboolean
gst_amc_codec_have_dynamic_bitrate (void)
{
  g_assert (gst_amc_codec_vtable != NULL);
  return gst_amc_codec_vtable->have_dynamic_bitrate ();
}

gboolean
gst_amc_codec_set_dynamic_bitrate (GstAmcCodec * codec, GError ** err,
    gint bitrate)
{
  g_assert (gst_amc_codec_vtable != NULL);
  return gst_amc_codec_vtable->set_dynamic_bitrate (codec, err, bitrate);
}

GstAmcBuffer *
gst_amc_codec_get_output_buffer (GstAmcCodec * codec, gint index, GError ** err)
{
  g_assert (gst_amc_codec_vtable != NULL);
  return gst_amc_codec_vtable->get_output_buffer (codec, index, err);
}

GstAmcBuffer *
gst_amc_codec_get_input_buffer (GstAmcCodec * codec, gint index, GError ** err)
{
  g_assert (gst_amc_codec_vtable != NULL);
  return gst_amc_codec_vtable->get_input_buffer (codec, index, err);
}

gint
gst_amc_codec_dequeue_input_buffer (GstAmcCodec * codec, gint64 timeoutUs,
    GError ** err)
{
  g_assert (gst_amc_codec_vtable != NULL);
  return gst_amc_codec_vtable->dequeue_input_buffer (codec, timeoutUs, err);
}

gint
gst_amc_codec_dequeue_output_buffer (GstAmcCodec * codec,
    GstAmcBufferInfo * info, gint64 timeoutUs, GError ** err)
{
  g_assert (gst_amc_codec_vtable != NULL);
  return gst_amc_codec_vtable->dequeue_output_buffer (codec, info, timeoutUs,
      err);
}

gboolean
gst_amc_codec_queue_input_buffer (GstAmcCodec * codec, gint index,
    const GstAmcBufferInfo * info, GError ** err)
{
  g_assert (gst_amc_codec_vtable != NULL);
  return gst_amc_codec_vtable->queue_input_buffer (codec, index, info, err);
}

gboolean
gst_amc_codec_release_output_buffer (GstAmcCodec * codec, gint index,
    gboolean render, GError ** err)
{
  g_assert (gst_amc_codec_vtable != NULL);
  return gst_amc_codec_vtable->release_output_buffer (codec, index, render,
      err);
}

GstAmcSurfaceTexture *
gst_amc_codec_new_surface_texture (GError ** err)
{
  g_assert (gst_amc_codec_vtable != NULL);
  return gst_amc_codec_vtable->new_surface_texture (err);
}
