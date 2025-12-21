/* GStreamer
 * Copyright (C) <2011> Wim Taymans <wim.taymans@gmail.com>
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

/**
 * SECTION:gstaudiometa
 * @title: GstAudio meta
 * @short_description: Buffer metadata for audio downmix matrix handling
 *
 * #GstAudioDownmixMeta defines an audio downmix matrix to be send along with
 * audio buffers. These functions in this module help to create and attach the
 * meta as well as extracting it.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstaudiometa.h"

#include <string.h>
#include <gst/base/base.h>

static gboolean
gst_audio_downmix_meta_init (GstMeta * meta, gpointer params,
    GstBuffer * buffer)
{
  GstAudioDownmixMeta *dmeta = (GstAudioDownmixMeta *) meta;

  dmeta->from_position = dmeta->to_position = NULL;
  dmeta->from_channels = dmeta->to_channels = 0;
  dmeta->matrix = NULL;

  return TRUE;
}

static void
gst_audio_downmix_meta_free (GstMeta * meta, GstBuffer * buffer)
{
  GstAudioDownmixMeta *dmeta = (GstAudioDownmixMeta *) meta;

  g_free (dmeta->from_position);
  if (dmeta->matrix) {
    g_free (*dmeta->matrix);
    g_free (dmeta->matrix);
  }
}

static gboolean
gst_audio_downmix_meta_transform (GstBuffer * dest, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  GstAudioDownmixMeta *smeta, *dmeta;

  smeta = (GstAudioDownmixMeta *) meta;

  if (GST_META_TRANSFORM_IS_COPY (type)) {
    dmeta = gst_buffer_add_audio_downmix_meta (dest, smeta->from_position,
        smeta->from_channels, smeta->to_position, smeta->to_channels,
        (const gfloat **) smeta->matrix);
    if (!dmeta)
      return FALSE;
  } else {
    /* return FALSE, if transform type is not supported */
    return FALSE;
  }

  return TRUE;
}

/**
 * gst_buffer_get_audio_downmix_meta_for_channels:
 * @buffer: a #GstBuffer
 * @to_position: (array length=to_channels): the channel positions of
 *   the destination
 * @to_channels: The number of channels of the destination
 *
 * Find the #GstAudioDownmixMeta on @buffer for the given destination
 * channel positions.
 *
 * Returns: (transfer none): the #GstAudioDownmixMeta on @buffer.
 */
GstAudioDownmixMeta *
gst_buffer_get_audio_downmix_meta_for_channels (GstBuffer * buffer,
    const GstAudioChannelPosition * to_position, gint to_channels)
{
  gpointer state = NULL;
  GstMeta *meta;
  const GstMetaInfo *info = GST_AUDIO_DOWNMIX_META_INFO;

  while ((meta = gst_buffer_iterate_meta (buffer, &state))) {
    if (meta->info->api == info->api) {
      GstAudioDownmixMeta *ameta = (GstAudioDownmixMeta *) meta;
      if (ameta->to_channels == to_channels &&
          memcmp (ameta->to_position, to_position,
              sizeof (GstAudioChannelPosition) * to_channels) == 0)
        return ameta;
    }
  }
  return NULL;
}

/**
 * gst_buffer_add_audio_downmix_meta:
 * @buffer: a #GstBuffer
 * @from_position: (array length=from_channels): the channel positions
 *   of the source
 * @from_channels: The number of channels of the source
 * @to_position: (array length=to_channels): the channel positions of
 *   the destination
 * @to_channels: The number of channels of the destination
 * @matrix: The matrix coefficients.
 *
 * Attaches #GstAudioDownmixMeta metadata to @buffer with the given parameters.
 *
 * @matrix is an two-dimensional array of @to_channels times @from_channels
 * coefficients, i.e. the i-th output channels is constructed by multiplicating
 * the input channels with the coefficients in @matrix[i] and taking the sum
 * of the results.
 *
 * Returns: (transfer none): the #GstAudioDownmixMeta on @buffer.
 */
GstAudioDownmixMeta *
gst_buffer_add_audio_downmix_meta (GstBuffer * buffer,
    const GstAudioChannelPosition * from_position, gint from_channels,
    const GstAudioChannelPosition * to_position, gint to_channels,
    const gfloat ** matrix)
{
  GstAudioDownmixMeta *meta;
  gint i;

  g_return_val_if_fail (from_position != NULL, NULL);
  g_return_val_if_fail (from_channels > 0, NULL);
  g_return_val_if_fail (to_position != NULL, NULL);
  g_return_val_if_fail (to_channels > 0, NULL);
  g_return_val_if_fail (matrix != NULL, NULL);

  meta =
      (GstAudioDownmixMeta *) gst_buffer_add_meta (buffer,
      GST_AUDIO_DOWNMIX_META_INFO, NULL);

  meta->from_channels = from_channels;
  meta->to_channels = to_channels;

  meta->from_position =
      g_new (GstAudioChannelPosition, meta->from_channels + meta->to_channels);
  meta->to_position = meta->from_position + meta->from_channels;
  memcpy (meta->from_position, from_position,
      sizeof (GstAudioChannelPosition) * meta->from_channels);
  memcpy (meta->to_position, to_position,
      sizeof (GstAudioChannelPosition) * meta->to_channels);

  meta->matrix = g_new (gfloat *, meta->to_channels);
  meta->matrix[0] = g_new (gfloat, meta->from_channels * meta->to_channels);
  memcpy (meta->matrix[0], matrix[0], sizeof (gfloat) * meta->from_channels);
  for (i = 1; i < meta->to_channels; i++) {
    meta->matrix[i] = meta->matrix[0] + i * meta->from_channels;
    memcpy (meta->matrix[i], matrix[i], sizeof (gfloat) * meta->from_channels);
  }

  return meta;
}

GType
gst_audio_downmix_meta_api_get_type (void)
{
  static GType type;
  static const gchar *tags[] =
      { GST_META_TAG_AUDIO_STR, GST_META_TAG_AUDIO_CHANNELS_STR, NULL };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstAudioDownmixMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

const GstMetaInfo *
gst_audio_downmix_meta_get_info (void)
{
  static const GstMetaInfo *audio_downmix_meta_info = NULL;

  if (g_once_init_enter ((GstMetaInfo **) & audio_downmix_meta_info)) {
    const GstMetaInfo *meta =
        gst_meta_register (GST_AUDIO_DOWNMIX_META_API_TYPE,
        "GstAudioDownmixMeta", sizeof (GstAudioDownmixMeta),
        gst_audio_downmix_meta_init, gst_audio_downmix_meta_free,
        gst_audio_downmix_meta_transform);
    g_once_init_leave ((GstMetaInfo **) & audio_downmix_meta_info,
        (GstMetaInfo *) meta);
  }
  return audio_downmix_meta_info;
}

static gboolean
gst_audio_clipping_meta_init (GstMeta * meta, gpointer params,
    GstBuffer * buffer)
{
  GstAudioClippingMeta *cmeta = (GstAudioClippingMeta *) meta;

  cmeta->format = GST_FORMAT_UNDEFINED;
  cmeta->start = cmeta->end = 0;

  return TRUE;
}

static gboolean
gst_audio_clipping_meta_transform (GstBuffer * dest, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  GstAudioClippingMeta *smeta, *dmeta;

  smeta = (GstAudioClippingMeta *) meta;

  if (GST_META_TRANSFORM_IS_COPY (type)) {
    GstMetaTransformCopy *copy = data;

    if (copy->region)
      return FALSE;

    dmeta =
        gst_buffer_add_audio_clipping_meta (dest, smeta->format, smeta->start,
        smeta->end);
    if (!dmeta)
      return FALSE;
  } else {
    /* TODO: Could implement an automatic transform for resampling */
    /* return FALSE, if transform type is not supported */
    return FALSE;
  }

  return TRUE;
}

/**
 * gst_buffer_add_audio_clipping_meta:
 * @buffer: a #GstBuffer
 * @format: GstFormat of @start and @stop, GST_FORMAT_DEFAULT is samples
 * @start: Amount of audio to clip from start of buffer
 * @end: Amount of  to clip from end of buffer
 *
 * Attaches #GstAudioClippingMeta metadata to @buffer with the given parameters.
 *
 * Returns: (transfer none): the #GstAudioClippingMeta on @buffer.
 *
 * Since: 1.8
 */
GstAudioClippingMeta *
gst_buffer_add_audio_clipping_meta (GstBuffer * buffer,
    GstFormat format, guint64 start, guint64 end)
{
  GstAudioClippingMeta *meta;

  g_return_val_if_fail (format != GST_FORMAT_UNDEFINED, NULL);

  meta =
      (GstAudioClippingMeta *) gst_buffer_add_meta (buffer,
      GST_AUDIO_CLIPPING_META_INFO, NULL);

  meta->format = format;
  meta->start = start;
  meta->end = end;

  return meta;
}

GType
gst_audio_clipping_meta_api_get_type (void)
{
  static GType type;
  static const gchar *tags[] =
      { GST_META_TAG_AUDIO_STR, GST_META_TAG_AUDIO_RATE_STR, NULL };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstAudioClippingMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

const GstMetaInfo *
gst_audio_clipping_meta_get_info (void)
{
  static const GstMetaInfo *audio_clipping_meta_info = NULL;

  if (g_once_init_enter ((GstMetaInfo **) & audio_clipping_meta_info)) {
    const GstMetaInfo *meta =
        gst_meta_register (GST_AUDIO_CLIPPING_META_API_TYPE,
        "GstAudioClippingMeta", sizeof (GstAudioClippingMeta),
        gst_audio_clipping_meta_init, NULL,
        gst_audio_clipping_meta_transform);
    g_once_init_leave ((GstMetaInfo **) & audio_clipping_meta_info,
        (GstMetaInfo *) meta);
  }
  return audio_clipping_meta_info;
}


static gboolean
gst_audio_meta_init (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  GstAudioMeta *ameta = (GstAudioMeta *) meta;

  gst_audio_info_init (&ameta->info);
  ameta->samples = 0;
  ameta->offsets = NULL;

  return TRUE;
}

static void
gst_audio_meta_free (GstMeta * meta, GstBuffer * buffer)
{
  GstAudioMeta *ameta = (GstAudioMeta *) meta;

  if (ameta->offsets && ameta->offsets != ameta->priv_offsets_arr)
    g_free (ameta->offsets);
}

static gboolean
gst_audio_meta_transform (GstBuffer * dest, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  GstAudioMeta *smeta, *dmeta;

  smeta = (GstAudioMeta *) meta;

  if (GST_META_TRANSFORM_IS_COPY (type)) {
    dmeta = gst_buffer_add_audio_meta (dest, &smeta->info, smeta->samples,
        smeta->offsets);
    if (!dmeta)
      return FALSE;
  } else {
    /* return FALSE, if transform type is not supported */
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_audio_meta_serialize (const GstMeta * meta, GstByteArrayInterface * data,
    guint8 * version)
{
  GstAudioMeta *ameta = (GstAudioMeta *) meta;

  /* Position is limited to 64 */
  gint n_position = ameta->info.channels > 64 ? 0 : ameta->info.channels;

  gsize size = 28 + n_position * 4 + ameta->info.channels * 8;
  guint8 *ptr = gst_byte_array_interface_append (data, size);
  if (ptr == NULL)
    return FALSE;

  GstByteWriter bw;
  gboolean success = TRUE;
  gst_byte_writer_init_with_data (&bw, ptr, size, FALSE);
  success &= gst_byte_writer_put_int32_le (&bw, ameta->info.finfo->format);
  success &= gst_byte_writer_put_int32_le (&bw, ameta->info.flags);
  success &= gst_byte_writer_put_int32_le (&bw, ameta->info.layout);
  success &= gst_byte_writer_put_int32_le (&bw, ameta->info.rate);
  success &= gst_byte_writer_put_int32_le (&bw, ameta->info.channels);
  for (int i = 0; i < n_position; i++)
    success &= gst_byte_writer_put_int32_le (&bw, ameta->info.position[i]);
  success &= gst_byte_writer_put_uint64_le (&bw, ameta->samples);
  for (int i = 0; i < ameta->info.channels; i++)
    success &= gst_byte_writer_put_uint64_le (&bw, ameta->offsets[i]);
  g_assert (success);

  return TRUE;
}

static GstMeta *
gst_audio_meta_deserialize (const GstMetaInfo * info, GstBuffer * buffer,
    const guint8 * data, gsize size, guint8 version)
{
  GstAudioMeta *ameta = NULL;
  gint32 format;
  gint32 flags;
  gint32 layout;
  gint32 rate;
  gint32 channels;

  if (version != 0)
    return NULL;

  GstByteReader br;
  gboolean success = TRUE;
  gst_byte_reader_init (&br, data, size);
  success &= gst_byte_reader_get_int32_le (&br, &format);
  success &= gst_byte_reader_get_int32_le (&br, &flags);
  success &= gst_byte_reader_get_int32_le (&br, &layout);
  success &= gst_byte_reader_get_int32_le (&br, &rate);
  success &= gst_byte_reader_get_int32_le (&br, &channels);

  if (!success)
    return NULL;

  /* Position is limited to 64 */
  gint n_position = channels > 64 ? 0 : channels;
  gint32 *position = g_new (gint32, n_position);
  guint64 *offsets64 = g_new (guint64, channels);
  guint64 samples = 0;

  for (int i = 0; i < n_position; i++)
    success &= gst_byte_reader_get_int32_le (&br, &position[i]);
  success &= gst_byte_reader_get_uint64_le (&br, &samples);
  for (int i = 0; i < channels; i++)
    success &= gst_byte_reader_get_uint64_le (&br, &offsets64[i]);

  if (!success) {
    g_free (position);
    g_free (offsets64);
    return NULL;
  }
#if GLIB_SIZEOF_SIZE_T != 8
  gsize *offsets = g_new (gsize, channels);
  for (int i = 0; i < channels; i++) {
    if (offsets64[i] > G_MAXSIZE) {
      g_free (offsets64);
      g_free (offsets);
      g_free (position);
      return NULL;
    }
    offsets[i] = offsets64[i];
  }
  g_free (offsets64);
#else
  gsize *offsets = (gsize *) offsets64;
#endif

  GstAudioInfo audio_info;
  gst_audio_info_set_format (&audio_info, format, rate, channels,
      (channels > 64) ? NULL : position);
  audio_info.flags = flags;
  audio_info.layout = layout;

  ameta = gst_buffer_add_audio_meta (buffer, &audio_info, samples, offsets);

  g_free (offsets);
  g_free (position);

  return (GstMeta *) ameta;
}

/**
 * gst_buffer_add_audio_meta:
 * @buffer: a #GstBuffer
 * @info: the audio properties of the buffer
 * @samples: the number of valid samples in the buffer
 * @offsets: (nullable): the offsets (in bytes) where each channel plane starts
 *   in the buffer or %NULL to calculate it (see below); must be %NULL also
 *   when @info->layout is %GST_AUDIO_LAYOUT_INTERLEAVED
 *
 * Allocates and attaches a #GstAudioMeta on @buffer, which must be writable
 * for that purpose. The fields of the #GstAudioMeta are directly populated
 * from the arguments of this function.
 *
 * When @info->layout is %GST_AUDIO_LAYOUT_NON_INTERLEAVED and @offsets is
 * %NULL, the offsets are calculated with a formula that assumes the planes are
 * tightly packed and in sequence:
 * offsets[channel] = channel * @samples * sample_stride
 *
 * It is not allowed for channels to overlap in memory,
 * i.e. for each i in [0, channels), the range
 * [@offsets[i], @offsets[i] + @samples * sample_stride) must not overlap
 * with any other such range. This function will assert if the parameters
 * specified cause this restriction to be violated.
 *
 * It is, obviously, also not allowed to specify parameters that would cause
 * out-of-bounds memory access on @buffer. This is also checked, which means
 * that you must add enough memory on the @buffer before adding this meta.
 *
 * Returns: (transfer none): the #GstAudioMeta that was attached on the @buffer
 *
 * Since: 1.16
 */
GstAudioMeta *
gst_buffer_add_audio_meta (GstBuffer * buffer, const GstAudioInfo * info,
    gsize samples, gsize offsets[])
{
  GstAudioMeta *meta;
  gint i;
  gsize plane_size;

  g_return_val_if_fail (GST_IS_BUFFER (buffer), FALSE);
  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GST_AUDIO_INFO_IS_VALID (info), NULL);
  g_return_val_if_fail (GST_AUDIO_INFO_FORMAT (info) !=
      GST_AUDIO_FORMAT_UNKNOWN, NULL);
  g_return_val_if_fail (info->layout == GST_AUDIO_LAYOUT_NON_INTERLEAVED
      || !offsets, NULL);

  meta =
      (GstAudioMeta *) gst_buffer_add_meta (buffer, GST_AUDIO_META_INFO, NULL);

  meta->info = *info;
  meta->samples = samples;
  plane_size = samples * info->finfo->width / 8;

  if (info->layout == GST_AUDIO_LAYOUT_NON_INTERLEAVED) {
#ifndef G_DISABLE_CHECKS
    gsize max_offset = 0;
    gint j;
#endif

    if (G_UNLIKELY (info->channels > 8))
      meta->offsets = g_new (gsize, info->channels);
    else
      meta->offsets = meta->priv_offsets_arr;

    if (offsets) {
      for (i = 0; i < info->channels; i++) {
        meta->offsets[i] = offsets[i];
#ifndef G_DISABLE_CHECKS
        max_offset = MAX (max_offset, offsets[i]);
        for (j = 0; j < info->channels; j++) {
          if (i != j && !(offsets[j] + plane_size <= offsets[i]
                  || offsets[i] + plane_size <= offsets[j])) {
            g_critical ("GstAudioMeta properties would cause channel memory "
                "areas to overlap! offsets: %" G_GSIZE_FORMAT " (%d), %"
                G_GSIZE_FORMAT " (%d) with plane size %" G_GSIZE_FORMAT,
                offsets[i], i, offsets[j], j, plane_size);
            gst_buffer_remove_meta (buffer, (GstMeta *) meta);
            return NULL;
          }
        }
#endif
      }
    } else {
      /* default offsets assume channels are laid out sequentially in memory */
      for (i = 0; i < info->channels; i++)
        meta->offsets[i] = i * plane_size;
#ifndef G_DISABLE_CHECKS
      max_offset = meta->offsets[info->channels - 1];
#endif
    }

#ifndef G_DISABLE_CHECKS
    if (max_offset + plane_size > gst_buffer_get_size (buffer)) {
      g_critical ("GstAudioMeta properties would cause "
          "out-of-bounds memory access on the buffer: max_offset %"
          G_GSIZE_FORMAT ", samples %" G_GSIZE_FORMAT ", bps %u, buffer size %"
          G_GSIZE_FORMAT, max_offset, samples, info->finfo->width / 8,
          gst_buffer_get_size (buffer));
      gst_buffer_remove_meta (buffer, (GstMeta *) meta);
      return NULL;
    }
#endif
  }

  return meta;
}

GType
gst_audio_meta_api_get_type (void)
{
  static GType type;
  static const gchar *tags[] = {
    GST_META_TAG_AUDIO_STR, GST_META_TAG_AUDIO_CHANNELS_STR,
    GST_META_TAG_AUDIO_RATE_STR, NULL
  };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstAudioMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

const GstMetaInfo *
gst_audio_meta_get_info (void)
{
  static const GstMetaInfo *audio_meta_info = NULL;

  if (g_once_init_enter ((GstMetaInfo **) & audio_meta_info)) {
    GstMetaInfo *info = gst_meta_info_new (GST_AUDIO_META_API_TYPE,
        "GstAudioMeta", sizeof (GstAudioMeta));

    info->init_func = gst_audio_meta_init;
    info->free_func = gst_audio_meta_free;
    info->transform_func = gst_audio_meta_transform;
    info->serialize_func = gst_audio_meta_serialize;
    info->deserialize_func = gst_audio_meta_deserialize;
    const GstMetaInfo *meta = gst_meta_info_register (info);

    g_once_init_leave ((GstMetaInfo **) & audio_meta_info,
        (GstMetaInfo *) meta);
  }
  return audio_meta_info;
}

/**
 * gst_audio_level_meta_api_get_type:
 *
 * Return the #GType associated with #GstAudioLevelMeta.
 *
 * Returns: a #GType
 *
 * Since: 1.20
 */
GType
gst_audio_level_meta_api_get_type (void)
{
  static GType type = 0;
  static const gchar *tags[] = { NULL };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstAudioLevelMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

static gboolean
gst_audio_level_meta_init (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  GstAudioLevelMeta *dmeta = (GstAudioLevelMeta *) meta;

  dmeta->level = 127;
  dmeta->voice_activity = FALSE;

  return TRUE;
}

static gboolean
gst_audio_level_meta_transform (GstBuffer * dst, GstMeta * meta,
    GstBuffer * src, GQuark type, gpointer data)
{
  if (GST_META_TRANSFORM_IS_COPY (type)) {
    GstAudioLevelMeta *smeta = (GstAudioLevelMeta *) meta;
    GstAudioLevelMeta *dmeta;

    dmeta = gst_buffer_add_audio_level_meta (dst, smeta->level,
        smeta->voice_activity);
    if (dmeta == NULL)
      return FALSE;
  } else {
    /* return FALSE, if transform type is not supported */
    return FALSE;
  }

  return TRUE;
}

/**
 * gst_audio_level_meta_get_info:
 *
 * Return the #GstMetaInfo associated with #GstAudioLevelMeta.
 *
 * Returns: (transfer none): a #GstMetaInfo
 *
 * Since: 1.20
 */
const GstMetaInfo *
gst_audio_level_meta_get_info (void)
{
  static const GstMetaInfo *audio_level_meta_info = NULL;

  if (g_once_init_enter (&audio_level_meta_info)) {
    const GstMetaInfo *meta = gst_meta_register (GST_AUDIO_LEVEL_META_API_TYPE,
        "GstAudioLevelMeta",
        sizeof (GstAudioLevelMeta),
        gst_audio_level_meta_init,
        (GstMetaFreeFunction) NULL,
        gst_audio_level_meta_transform);
    g_once_init_leave (&audio_level_meta_info, meta);
  }
  return audio_level_meta_info;
}

/**
 * gst_buffer_add_audio_level_meta:
 * @buffer: a #GstBuffer
 * @level: the -dBov from 0-127 (127 is silence).
 * @voice_activity: whether the buffer contains voice activity.
 *
 * Attaches audio level information to @buffer. (RFC 6464)
 *
 * Returns: (transfer none) (nullable): the #GstAudioLevelMeta on @buffer.
 *
 * Since: 1.20
 */
GstAudioLevelMeta *
gst_buffer_add_audio_level_meta (GstBuffer * buffer, guint8 level,
    gboolean voice_activity)
{
  GstAudioLevelMeta *meta;

  g_return_val_if_fail (buffer != NULL, NULL);

  meta = (GstAudioLevelMeta *) gst_buffer_add_meta (buffer,
      GST_AUDIO_LEVEL_META_INFO, NULL);
  if (!meta)
    return NULL;

  meta->level = level;
  meta->voice_activity = voice_activity;

  return meta;
}

/**
 * gst_buffer_get_audio_level_meta:
 * @buffer: a #GstBuffer
 *
 * Find the #GstAudioLevelMeta on @buffer.
 *
 * Returns: (transfer none) (nullable): the #GstAudioLevelMeta or %NULL when
 * there is no such metadata on @buffer.
 *
 * Since: 1.20
 */
GstAudioLevelMeta *
gst_buffer_get_audio_level_meta (GstBuffer * buffer)
{
  return (GstAudioLevelMeta *) gst_buffer_get_meta (buffer,
      gst_audio_level_meta_api_get_type ());
}
