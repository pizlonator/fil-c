/* GStreamer
 * Copyright (C) 2023 Carlos Rafael Giani <crg7475@mailbox.org>
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
#  include "config.h"
#endif

#include <string.h>
#include "gstdsd.h"

#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT ensure_debug_category()
static GstDebugCategory *
ensure_debug_category (void)
{
  static gsize cat_gonce = 0;

  if (g_once_init_enter (&cat_gonce)) {
    gsize cat_done;

    cat_done = (gsize) _gst_debug_category_new ("gst-dsd", 0, "GStreamer DSD");

    g_once_init_leave (&cat_gonce, cat_done);
  }

  return (GstDebugCategory *) cat_gonce;
}
#else
#define ensure_debug_category() /* NOOP */
#endif /* GST_DISABLE_GST_DEBUG */

static const guint8 byte_bit_reversal_table[256] = {
  0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
  0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
  0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
  0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
  0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
  0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
  0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
  0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
  0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
  0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
  0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
  0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
  0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
  0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
  0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
  0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
  0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
  0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
  0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
  0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
  0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
  0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
  0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
  0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
  0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
  0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
  0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
  0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
  0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
  0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
  0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
  0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff,
};

static const char *
layout_to_string (GstAudioLayout layout)
{
  const char *layout_str = NULL;

  switch (layout) {
    case GST_AUDIO_LAYOUT_INTERLEAVED:
      layout_str = "interleaved";
      break;
    case GST_AUDIO_LAYOUT_NON_INTERLEAVED:
      layout_str = "non-interleaved";
      break;
    default:
      g_return_val_if_reached (NULL);
  }

  return layout_str;
}

static gboolean
gst_dsd_plane_offset_meta_init (GstMeta * meta, gpointer params,
    GstBuffer * buffer)
{
  GstDsdPlaneOffsetMeta *ofs_meta = (GstDsdPlaneOffsetMeta *) meta;
  ofs_meta->offsets = NULL;

  return TRUE;
}

static void
gst_dsd_plane_offset_meta_free (GstMeta * meta, GstBuffer * buffer)
{
  GstDsdPlaneOffsetMeta *ofs_meta = (GstDsdPlaneOffsetMeta *) meta;

  if (ofs_meta->offsets && ofs_meta->offsets != ofs_meta->priv_offsets_arr)
    g_free (ofs_meta->offsets);
}

static gboolean
gst_dsd_plane_offset_meta_transform (GstBuffer * dest, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  GstDsdPlaneOffsetMeta *smeta, *dmeta;

  smeta = (GstDsdPlaneOffsetMeta *) meta;

  if (GST_META_TRANSFORM_IS_COPY (type)) {
    dmeta = gst_buffer_add_dsd_plane_offset_meta (dest, smeta->num_channels,
        smeta->num_bytes_per_channel, smeta->offsets);
    if (!dmeta)
      return FALSE;
  } else {
    /* return FALSE, if transform type is not supported */
    return FALSE;
  }

  return TRUE;
}

GType
gst_dsd_plane_offset_meta_api_get_type (void)
{
  static GType type;
  static const gchar *tags[] = {
    GST_META_TAG_AUDIO_STR,
    GST_META_TAG_DSD_PLANE_OFFSETS_STR,
    NULL
  };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstDsdPlaneOffsetMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

const GstMetaInfo *
gst_dsd_plane_offset_meta_get_info (void)
{
  static const GstMetaInfo *dsd_plane_offset_meta_info = NULL;

  if (g_once_init_enter ((GstMetaInfo **) & dsd_plane_offset_meta_info)) {
    const GstMetaInfo *meta =
        gst_meta_register (GST_DSD_PLANE_OFFSET_META_API_TYPE,
        "GstDsdPlaneOffsetMeta",
        sizeof (GstDsdPlaneOffsetMeta),
        gst_dsd_plane_offset_meta_init,
        gst_dsd_plane_offset_meta_free,
        gst_dsd_plane_offset_meta_transform);
    g_once_init_leave ((GstMetaInfo **) & dsd_plane_offset_meta_info,
        (GstMetaInfo *) meta);
  }
  return dsd_plane_offset_meta_info;
}

/**
 * gst_buffer_add_dsd_plane_offset_meta:
 * @buffer: a #GstBuffer
 * @num_channels: Number of channels in the DSD data
 * @num_bytes_per_channel: Number of bytes per channel
 * @offsets: (nullable): the offsets (in bytes) where each channel plane starts
 *   in the buffer
 *
 * Allocates and attaches a #GstDsdPlaneOffsetMeta on @buffer, which must be
 * writable for that purpose. The fields of the #GstDsdPlaneOffsetMeta are
 * directly populated from the arguments of this function.
 *
 * If @offsets is NULL, then the meta's offsets field is left uninitialized.
 * This is useful if for example offset values are to be calculated in the
 * meta's offsets field in-place. Similarly, @num_bytes_per_channel can be
 * set to 0, but only if @offsets is NULL. This is useful if the number of
 * bytes per channel is known only later.
 *
 * It is not allowed for channels to overlap in memory,
 * i.e. for each i in [0, channels), the range
 * [@offsets[i], @offsets[i] + @num_bytes_per_channel) must not overlap
 * with any other such range. This function will assert if the parameters
 * specified cause this restriction to be violated.
 *
 * It is, obviously, also not allowed to specify parameters that would cause
 * out-of-bounds memory access on @buffer. This is also checked, which means
 * that you must add enough memory on the @buffer before adding this meta.
 *
 * This meta is only needed for non-interleaved (= planar) DSD data.
 *
 * Returns: (transfer none): the #GstDsdPlaneOffsetMeta that was attached
 *   on the @buffer
 *
 * Since: 1.24
 */
GstDsdPlaneOffsetMeta *
gst_buffer_add_dsd_plane_offset_meta (GstBuffer * buffer, gint num_channels,
    gsize num_bytes_per_channel, gsize offsets[])
{
  GstDsdPlaneOffsetMeta *meta;
  gint i;
#ifndef G_DISABLE_CHECKS
  gsize max_offset = 0;
  gint j;
#endif

  g_return_val_if_fail (GST_IS_BUFFER (buffer), NULL);
  g_return_val_if_fail (num_channels >= 1, NULL);
  g_return_val_if_fail (!offsets || (num_bytes_per_channel >= 1), NULL);

  meta = (GstDsdPlaneOffsetMeta *) gst_buffer_add_meta (buffer,
      GST_DSD_PLANE_OFFSET_META_INFO, NULL);

  meta->num_channels = num_channels;
  meta->num_bytes_per_channel = num_bytes_per_channel;

  if (G_UNLIKELY (num_channels > 8))
    meta->offsets = g_new (gsize, num_channels);
  else
    meta->offsets = meta->priv_offsets_arr;

  if (offsets) {
    for (i = 0; i < num_channels; i++) {
      meta->offsets[i] = offsets[i];
#ifndef G_DISABLE_CHECKS
      max_offset = MAX (max_offset, offsets[i]);
      for (j = 0; j < num_channels; j++) {
        if (i != j && !(offsets[j] + num_bytes_per_channel <= offsets[i]
                || offsets[i] + num_bytes_per_channel <= offsets[j])) {
          g_critical ("GstDsdPlaneOffsetMeta properties would cause channel "
              "memory  areas to overlap! offsets: %" G_GSIZE_FORMAT " (%d), %"
              G_GSIZE_FORMAT " (%d) with %" G_GSIZE_FORMAT " bytes per channel",
              offsets[i], i, offsets[j], j, num_bytes_per_channel);
          gst_buffer_remove_meta (buffer, (GstMeta *) meta);
          return NULL;
        }
      }
#endif
    }

#ifndef G_DISABLE_CHECKS
    if (max_offset + num_bytes_per_channel > gst_buffer_get_size (buffer)) {
      g_critical ("GstDsdPlaneOffsetMeta properties would cause "
          "out-of-bounds memory access on the buffer: max_offset %"
          G_GSIZE_FORMAT ", %" G_GSIZE_FORMAT " bytes per channel, "
          "buffer size %" G_GSIZE_FORMAT, max_offset, num_bytes_per_channel,
          gst_buffer_get_size (buffer));
      gst_buffer_remove_meta (buffer, (GstMeta *) meta);
      return NULL;
    }
#endif
  }

  return meta;
}

G_DEFINE_BOXED_TYPE (GstDsdInfo, gst_dsd_info,
    (GBoxedCopyFunc) gst_dsd_info_copy, (GBoxedFreeFunc) gst_dsd_info_free);

/**
 * gst_dsd_info_new:
 *
 * Allocate a new #GstDsdInfo that is also initialized with
 * gst_dsd_info_init().
 *
 * Returns: a new #GstDsdInfo. free with gst_dsd_info_free().
 *
 * Since: 1.24
 */
GstDsdInfo *
gst_dsd_info_new (void)
{
  GstDsdInfo *info;

  info = g_slice_new (GstDsdInfo);
  gst_dsd_info_init (info);

  return info;
}

/**
 * gst_dsd_info_new_from_caps:
 * @caps: a #GstCaps
 *
 * Parse @caps to generate a #GstDsdInfo.
 *
 * Returns: A #GstDsdInfo, or %NULL if @caps couldn't be parsed
 *
 * Since: 1.24
 */
GstDsdInfo *
gst_dsd_info_new_from_caps (const GstCaps * caps)
{
  GstDsdInfo *ret;

  g_return_val_if_fail (caps != NULL, NULL);

  ret = gst_dsd_info_new ();

  if (gst_dsd_info_from_caps (ret, caps)) {
    return ret;
  } else {
    gst_dsd_info_free (ret);
    return NULL;
  }
}

/**
 * gst_dsd_info_init:
 * @info: (out caller-allocates): a #GstDsdInfo
 *
 * Initialize @info with default values.
 *
 * Since: 1.24
 */
void
gst_dsd_info_init (GstDsdInfo * info)
{
  g_return_if_fail (info != NULL);

  memset (info, 0, sizeof (GstDsdInfo));
  info->format = GST_DSD_FORMAT_UNKNOWN;
}

/**
 * gst_dsd_info_set_format:
 * @info: a #GstDsdInfo
 * @format: the format
 * @rate: the DSD rate
 * @channels: the number of channels
 * @positions: (array fixed-size=64) (nullable): the channel positions
 *
 * Set the default info for the DSD info of @format and @rate and @channels.
 *
 * Note: This initializes @info first, no values are preserved.
 *
 * Since: 1.24
 */
void
gst_dsd_info_set_format (GstDsdInfo * info, GstDsdFormat format,
    gint rate, gint channels, const GstAudioChannelPosition * positions)
{
  gint i;

  g_return_if_fail (info != NULL);
  g_return_if_fail (format != GST_DSD_FORMAT_UNKNOWN);
  g_return_if_fail (channels <= 64 || positions == NULL);

  gst_dsd_info_init (info);

  info->format = format;
  info->rate = rate;
  info->channels = channels;
  info->layout = GST_AUDIO_LAYOUT_INTERLEAVED;
  info->flags = GST_AUDIO_FLAG_NONE;

  memset (&info->positions, 0xff, sizeof (info->positions));

  if (!positions && channels == 1) {
    info->positions[0] = GST_AUDIO_CHANNEL_POSITION_MONO;
    return;
  } else if (!positions && channels == 2) {
    info->positions[0] = GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT;
    info->positions[1] = GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT;
    return;
  } else {
    if (!positions
        || !gst_audio_check_valid_channel_positions (positions, channels,
            TRUE)) {
      if (positions)
        g_warning ("Invalid channel positions");
    } else {
      memcpy (&info->positions, positions,
          info->channels * sizeof (info->positions[0]));
      if (info->positions[0] == GST_AUDIO_CHANNEL_POSITION_NONE)
        info->flags |= GST_AUDIO_FLAG_UNPOSITIONED;
      return;
    }
  }

  /* Otherwise a NONE layout */
  info->flags |= GST_AUDIO_FLAG_UNPOSITIONED;
  for (i = 0; i < MIN (64, channels); i++)
    info->positions[i] = GST_AUDIO_CHANNEL_POSITION_NONE;
}

/**
 * gst_dsd_info_copy:
 * @info: a #GstDsdInfo
 *
 * Copy a GstDsdInfo structure.
 *
 * Returns: a new #GstDsdInfo. free with gst_dsd_info_free.
 *
 * Since: 1.24
 */
GstDsdInfo *
gst_dsd_info_copy (const GstDsdInfo * info)
{
  return g_slice_dup (GstDsdInfo, info);
}

/**
 * gst_dsd_info_free:
 * @info: a #GstDsdInfo
 *
 * Free a GstDsdInfo structure previously allocated with gst_dsd_info_new()
 * or gst_dsd_info_copy().
 *
 * Since: 1.24
 */
void
gst_dsd_info_free (GstDsdInfo * info)
{
  g_slice_free (GstDsdInfo, info);
}

/**
 * gst_dsd_info_from_caps:
 * @info: (out caller-allocates): a #GstDsdInfo
 * @caps: a #GstCaps
 *
 * Parse @caps and update @info.
 *
 * Returns: TRUE if @caps could be parsed
 *
 * Since: 1.24
 */
gboolean
gst_dsd_info_from_caps (GstDsdInfo * info, const GstCaps * caps)
{
  GstStructure *fmt_structure;
  const gchar *media_type;
  const gchar *format_str = NULL;
  const gchar *layout_str = NULL;
  gboolean reversed_bytes = FALSE;
  GstAudioFlags flags = GST_AUDIO_FLAG_NONE;

  guint64 channel_mask = 0;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (caps != NULL, FALSE);
  g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);

  fmt_structure = gst_caps_get_structure (caps, 0);
  media_type = gst_structure_get_name (fmt_structure);

  g_return_val_if_fail (g_strcmp0 (media_type, GST_DSD_MEDIA_TYPE) == 0, FALSE);

  /* Parse the format */

  format_str = gst_structure_get_string (fmt_structure, "format");
  if (format_str == NULL) {
    GST_ERROR ("caps have no format field; caps: %" GST_PTR_FORMAT, caps);
    goto error;
  }

  info->format = gst_dsd_format_from_string (format_str);
  if (info->format == GST_DSD_FORMAT_UNKNOWN) {
    GST_ERROR ("caps have unsupported/invalid format field; caps: %"
        GST_PTR_FORMAT, caps);
    goto error;
  }

  /* Parse the rate */

  if (!gst_structure_get_int (fmt_structure, "rate", &(info->rate))) {
    GST_ERROR ("caps have no rate field; caps: %" GST_PTR_FORMAT, caps);
    goto error;
  }

  if (info->rate < 1) {
    GST_ERROR ("caps have invalid rate field; caps: %" GST_PTR_FORMAT, caps);
    goto error;
  }

  /* Parse the channels and the channel mask */

  if (!gst_structure_get_int (fmt_structure, "channels", &(info->channels))) {
    GST_ERROR ("caps have no channels field; caps: %" GST_PTR_FORMAT, caps);
    goto error;
  }

  if (info->channels < 1) {
    GST_ERROR ("caps have invalid channels field; caps: %" GST_PTR_FORMAT,
        caps);
    goto error;
  }

  if (!gst_structure_get (fmt_structure, "channel-mask", GST_TYPE_BITMASK,
          &channel_mask, NULL) || (channel_mask == 0 && info->channels == 1)
      ) {
    switch (info->channels) {
      case 1:
        info->positions[0] = GST_AUDIO_CHANNEL_POSITION_MONO;
        break;

      case 2:
        info->positions[0] = GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT;
        info->positions[1] = GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT;
        break;

      default:
        GST_ERROR
            ("caps indicate multichannel DSD data but they do not contain channel-mask field; caps: %"
            GST_PTR_FORMAT, caps);
        goto error;
    }
  } else if (channel_mask == 0) {
    gint i;
    flags |= GST_AUDIO_FLAG_UNPOSITIONED;
    for (i = 0; i < MIN (64, info->channels); i++)
      info->positions[i] = GST_AUDIO_CHANNEL_POSITION_NONE;
  } else {
    if (!gst_audio_channel_positions_from_mask (info->channels, channel_mask,
            info->positions)) {
      GST_ERROR ("invalid channel mask 0x%016" G_GINT64_MODIFIER
          "x for %d channels", channel_mask, info->channels);
      goto error;
    }
  }

  /* Parse the layout */

  layout_str = gst_structure_get_string (fmt_structure, "layout");
  if (layout_str == NULL || g_strcmp0 (layout_str, "interleaved") == 0)
    info->layout = GST_AUDIO_LAYOUT_INTERLEAVED;
  else if (g_strcmp0 (layout_str, "non-interleaved") == 0)
    info->layout = GST_AUDIO_LAYOUT_NON_INTERLEAVED;
  else {
    GST_ERROR ("caps contain invalid layout field; caps: %" GST_PTR_FORMAT,
        caps);
    goto error;
  }

  gst_structure_get (fmt_structure, "reversed-bytes", G_TYPE_BOOLEAN,
      &reversed_bytes, NULL);

  info->flags = flags;
  info->reversed_bytes = reversed_bytes;

  return TRUE;

error:
  return FALSE;
}

/**
 * gst_dsd_info_to_caps:
 * @info: a #GstDsdInfo
 *
 * Convert the values of @info into a #GstCaps.
 *
 * Returns: (transfer full): the new #GstCaps containing the
 *          info of @info.
 *
 * Since: 1.24
 */
GstCaps *
gst_dsd_info_to_caps (const GstDsdInfo * info)
{
  GstCaps *caps;
  const gchar *format;
  GstAudioFlags flags;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (info->format > GST_DSD_FORMAT_UNKNOWN
      && info->format < GST_NUM_DSD_FORMATS, NULL);
  g_return_val_if_fail (info->rate >= 1, NULL);
  g_return_val_if_fail (info->channels >= 1, NULL);

  format = gst_dsd_format_to_string (info->format);
  g_return_val_if_fail (format != NULL, NULL);

  flags = info->flags;
  if ((flags & GST_AUDIO_FLAG_UNPOSITIONED) && info->channels > 1
      && info->positions[0] != GST_AUDIO_CHANNEL_POSITION_NONE) {
    flags &= ~GST_AUDIO_FLAG_UNPOSITIONED;
    GST_WARNING ("Unpositioned audio channel position flag set but "
        "channel positions present");
  } else if (!(flags & GST_AUDIO_FLAG_UNPOSITIONED) && info->channels > 1
      && info->positions[0] == GST_AUDIO_CHANNEL_POSITION_NONE) {
    flags |= GST_AUDIO_FLAG_UNPOSITIONED;
    GST_WARNING ("Unpositioned audio channel position flag not set "
        "but no channel positions present");
  }

  caps = gst_caps_new_simple (GST_DSD_MEDIA_TYPE,
      "format", G_TYPE_STRING, format,
      "rate", G_TYPE_INT, info->rate,
      "channels", G_TYPE_INT, info->channels,
      "layout", G_TYPE_STRING, layout_to_string (info->layout),
      "reversed-bytes", G_TYPE_BOOLEAN, info->reversed_bytes, NULL);

  if (info->channels > 1
      || info->positions[0] != GST_AUDIO_CHANNEL_POSITION_MONO) {
    guint64 channel_mask = 0;

    if ((flags & GST_AUDIO_FLAG_UNPOSITIONED)) {
      channel_mask = 0;
    } else {
      if (!gst_audio_channel_positions_to_mask (info->positions, info->channels,
              TRUE, &channel_mask))
        goto invalid_channel_positions;
    }

    if (info->channels == 1
        && info->positions[0] == GST_AUDIO_CHANNEL_POSITION_MONO) {
      /* Default mono special case */
    } else {
      gst_caps_set_simple (caps, "channel-mask", GST_TYPE_BITMASK, channel_mask,
          NULL);
    }
  }

  return caps;

invalid_channel_positions:
  GST_ERROR ("Invalid channel positions");
  gst_caps_unref (caps);
  return NULL;
}

/**
 * gst_dsd_info_is_equal:
 * @info: a #GstDsdInfo
 * @other: a #GstDsdInfo
 *
 * Compares two #GstDsdInfo and returns whether they are equal or not
 *
 * Returns: %TRUE if @info and @other are equal, else %FALSE.
 *
 * Since: 1.24
 */
gboolean
gst_dsd_info_is_equal (const GstDsdInfo * info, const GstDsdInfo * other)
{
  if (info == other)
    return TRUE;

  if (GST_DSD_INFO_FORMAT (info) != GST_DSD_INFO_FORMAT (other))
    return FALSE;
  if (GST_DSD_INFO_RATE (info) != GST_DSD_INFO_RATE (other))
    return FALSE;
  if (GST_DSD_INFO_CHANNELS (info) != GST_DSD_INFO_CHANNELS (other))
    return FALSE;
  if (GST_DSD_INFO_LAYOUT (info) != GST_DSD_INFO_LAYOUT (other))
    return FALSE;
  if (GST_DSD_INFO_REVERSED_BYTES (info) != GST_DSD_INFO_REVERSED_BYTES (other))
    return FALSE;
  if (memcmp (info->positions, other->positions,
          GST_AUDIO_INFO_CHANNELS (info) * sizeof (GstAudioChannelPosition)) !=
      0)
    return FALSE;

  return TRUE;
}

static void
gst_dsd_convert_copy_bytes_same_format (const guint8 * input_data,
    guint8 * output_data, GstDsdFormat format, gsize num_bytes,
    gboolean reverse_byte_bits)
{
  if (reverse_byte_bits) {
    guint index;
    for (index = 0; index < num_bytes; ++index)
      output_data[index] = byte_bit_reversal_table[input_data[index]];
  } else
    memcpy (output_data, input_data, num_bytes);
}

/* The conversion functions work by figuring out the index in the input
 * data that corresponds to the current index in the output data. The DSD
 * bits are grouped into "words" according to the DSD format. For example,
 * if input_format is GST_DSD_FORMAT_U16LE, then the input data is
 * grouped into 16-bit (= 2 byte) words. The in/out_word_index values
 * are the word indices into the input/output data. in/out_word_offset
 * values are the offsets *within* the words that are currently being
 * accessed. in/out_index are the combination of these values.
 * position is the offset in the time axis (= the position value that
 * would be used for seeking). In PCM terms, this is the equivalent of
 * (byte_offset / bytes_per_frame).
 *
 * The calculations first figure out the position and channel_nr out
 * of out_index. Using these two values it is then possible to calculate
 * in_word_index, in_word_width, and ultimately, in_index. The final
 * step is then to copy the DSD byte from in_index in input_data to
 * out_index in output_data (with reversing the byte's bits if requested).
 *
 * Conversions to non-interleaved formats work a little differently:
 * instead of one out_index there is one plane_index, that is, the
 * output is produced per-plane.
 *
 * For example, with interleaved -> interleaved conversion, given stereo
 * data (-> num_channels is 2), U16BE input, and U32BE output, then
 * in_word_width is 2, out_word_width is 4, out_stride is 2*4 = 8. An
 * out_index 15 means (note that indices start at 0, so channel #1 is the
 * second channel):
 *
 * - out_word_index = out_index / out_word_width = 15 / 8 = 1
 *   out_index refers to word #1 in the output array
 * - out_word_offset = out_index - out_word_index * out_word_width = 15 - 1*8 = 7
 *   out_index refers to byte #7 in output word #1
 * - channel_nr = out_word_index % num_channels = 1 % 2 = 1
 *   out_index is referring to a byte that belongs to channel #1
 * - position = (out_index / out_stride) * out_word_width + out_word_offset =
 *   (15/8) * 4 + 7 = 11
 *   out_index refers to time axis offset 11 (in bytes)
 *
 * Then:
 * - in_word_index = (position / in_word_width) * num_channels + channel_nrh =
 *   (11/2) * 2 + 1 = 11
 * - in_word_offset = position % in_word_width = 11 % 2 = 1
 * - in_index = in_word_index * in_word_width + in_word_offset = 11 * 2 + 1 = 23
 *
 * -> We copy the byte #23 in input_data to byte #15 in output_data.
 */

static void
gst_dsd_convert_interleaved_to_interleaved (const guint8 * input_data,
    guint8 * output_data, GstDsdFormat input_format, GstDsdFormat output_format,
    gsize num_dsd_bytes, gint num_channels, gboolean reverse_byte_bits)
{
  if (input_format != output_format) {
    guint out_index;
    guint in_word_width, out_word_width;
    guint out_stride;
    gboolean input_is_le = gst_dsd_format_is_le (input_format);
    gboolean output_is_le = gst_dsd_format_is_le (output_format);

    in_word_width = gst_dsd_format_get_width (input_format);
    out_word_width = gst_dsd_format_get_width (output_format);
    out_stride = out_word_width * num_channels;

    for (out_index = 0; out_index < num_dsd_bytes; ++out_index) {
      guint in_word_index, in_word_offset;
      guint out_word_index, out_word_offset;
      guint in_index;
      guint channel_nr;
      guint position;
      guint8 input_byte;

      out_word_index = out_index / out_word_width;
      out_word_offset = out_index % out_word_width;
      if (output_is_le)
        out_word_offset = out_word_width - 1 - out_word_offset;

      channel_nr = out_word_index % num_channels;
      position = (out_index / out_stride) * out_word_width + out_word_offset;

      in_word_index = (position / in_word_width) * num_channels + channel_nr;
      in_word_offset = position % in_word_width;
      if (input_is_le)
        in_word_offset = in_word_width - 1 - in_word_offset;

      in_index = in_word_index * in_word_width + in_word_offset;

      input_byte = input_data[in_index];
      output_data[out_index] =
          reverse_byte_bits ? byte_bit_reversal_table[input_byte] : input_byte;
    }
  } else
    gst_dsd_convert_copy_bytes_same_format (input_data, output_data,
        input_format, num_dsd_bytes, reverse_byte_bits);
}

static void
gst_dsd_convert_interleaved_to_non_interleaved (const guint8 * input_data,
    guint8 * output_data, GstDsdFormat input_format, GstDsdFormat output_format,
    const gsize * output_plane_offsets, gsize num_dsd_bytes, gint num_channels,
    gboolean reverse_byte_bits)
{
  guint plane_index;
  guint in_word_width, out_word_width;
  guint channel_nr;
  gsize num_bytes_per_plane = num_dsd_bytes / num_channels;
  gboolean input_is_le = gst_dsd_format_is_le (input_format);
  gboolean output_is_le = gst_dsd_format_is_le (output_format);

  in_word_width = gst_dsd_format_get_width (input_format);
  out_word_width = gst_dsd_format_get_width (output_format);

  for (channel_nr = 0; channel_nr < num_channels; ++channel_nr) {
    for (plane_index = 0; plane_index < num_bytes_per_plane; ++plane_index) {
      guint in_word_index, in_word_offset;
      guint out_word_index, out_word_offset;
      guint in_index;
      guint out_index;
      guint position;
      guint8 input_byte;

      out_word_index = plane_index / out_word_width;
      out_word_offset = plane_index % out_word_width;
      if (output_is_le)
        out_word_offset = out_word_width - 1 - out_word_offset;

      position = plane_index;

      in_word_index = (position / in_word_width) * num_channels + channel_nr;
      in_word_offset = position % in_word_width;
      if (input_is_le)
        in_word_offset = in_word_width - 1 - in_word_offset;

      in_index = in_word_index * in_word_width + in_word_offset;
      out_index =
          output_plane_offsets[channel_nr] + out_word_index * out_word_width +
          out_word_offset;

      input_byte = input_data[in_index];
      output_data[out_index] =
          reverse_byte_bits ? byte_bit_reversal_table[input_byte] : input_byte;
    }
  }
}

static void
gst_dsd_convert_non_interleaved_to_interleaved (const guint8 * input_data,
    guint8 * output_data, GstDsdFormat input_format, GstDsdFormat output_format,
    const gsize * input_plane_offsets, gsize num_dsd_bytes, gint num_channels,
    gboolean reverse_byte_bits)
{
  guint out_index;
  guint in_word_width, out_word_width;
  guint out_stride;
  gboolean input_is_le = gst_dsd_format_is_le (input_format);
  gboolean output_is_le = gst_dsd_format_is_le (output_format);

  in_word_width = gst_dsd_format_get_width (input_format);
  out_word_width = gst_dsd_format_get_width (output_format);
  out_stride = out_word_width * num_channels;

  for (out_index = 0; out_index < num_dsd_bytes; ++out_index) {
    guint in_word_index, in_word_offset;
    guint out_word_index, out_word_offset;
    guint in_index;
    guint channel_nr;
    guint position;
    guint8 input_byte;

    out_word_index = out_index / out_word_width;
    out_word_offset = out_index % out_word_width;
    if (output_is_le)
      out_word_offset = out_word_width - 1 - out_word_offset;

    channel_nr = out_word_index % num_channels;
    position = (out_index / out_stride) * out_word_width + out_word_offset;

    in_word_index = position / in_word_width;
    in_word_offset = position % in_word_width;
    if (input_is_le)
      in_word_offset = in_word_width - 1 - in_word_offset;

    in_index =
        input_plane_offsets[channel_nr] + in_word_index * in_word_width +
        in_word_offset;

    input_byte = input_data[in_index];
    output_data[out_index] =
        reverse_byte_bits ? byte_bit_reversal_table[input_byte] : input_byte;
  }
}

static void
gst_dsd_convert_non_interleaved_to_non_interleaved (const guint8 * input_data,
    guint8 * output_data, GstDsdFormat input_format, GstDsdFormat output_format,
    const gsize * input_plane_offsets, const gsize * output_plane_offsets,
    gsize num_dsd_bytes, gint num_channels, gboolean reverse_byte_bits)
{
  gboolean same_format = input_format == output_format;
  gboolean same_plane_offsets = memcmp (input_plane_offsets,
      output_plane_offsets, num_channels * sizeof (gsize)) == 0;

  if (same_format && same_plane_offsets) {
    gst_dsd_convert_copy_bytes_same_format (input_data, output_data,
        input_format, num_dsd_bytes, reverse_byte_bits);
  } else if (same_format) {
    gint channel_nr;
    gsize num_bytes_per_plane = num_dsd_bytes / num_channels;

    if (reverse_byte_bits) {
      guint plane_index;
      guint8 input_byte;

      for (channel_nr = 0; channel_nr < num_channels; ++channel_nr) {
        for (plane_index = 0; plane_index < num_bytes_per_plane; ++plane_index) {
          guint in_index = input_plane_offsets[channel_nr] + plane_index;
          guint out_index = output_plane_offsets[channel_nr] + plane_index;
          input_byte = input_data[in_index];
          output_data[out_index] = byte_bit_reversal_table[input_byte];
        }
      }
    } else {
      for (channel_nr = 0; channel_nr < num_channels; ++channel_nr) {
        memcpy (output_data + output_plane_offsets[channel_nr],
            input_data + input_plane_offsets[channel_nr], num_bytes_per_plane);
      }
    }
  } else {
    guint channel_nr;
    guint plane_index;
    gsize num_bytes_per_plane = num_dsd_bytes / num_channels;
    guint in_word_width, out_word_width;
    gboolean input_is_le = gst_dsd_format_is_le (input_format);
    gboolean output_is_le = gst_dsd_format_is_le (output_format);

    in_word_width = gst_dsd_format_get_width (input_format);
    out_word_width = gst_dsd_format_get_width (output_format);

    for (channel_nr = 0; channel_nr < num_channels; ++channel_nr) {
      for (plane_index = 0; plane_index < num_bytes_per_plane; ++plane_index) {
        guint in_word_index, in_word_offset;
        guint out_word_index, out_word_offset;
        guint in_index;
        guint out_index;
        guint position;
        guint8 input_byte;

        out_word_index = plane_index / out_word_width;
        out_word_offset = plane_index % out_word_width;
        if (output_is_le)
          out_word_offset = out_word_width - 1 - out_word_offset;

        position = plane_index;

        in_word_index = position / in_word_width;
        in_word_offset = position % in_word_width;
        if (input_is_le)
          in_word_offset = in_word_width - 1 - in_word_offset;

        in_index =
            input_plane_offsets[channel_nr] + in_word_index * in_word_width +
            in_word_offset;
        out_index =
            output_plane_offsets[channel_nr] + out_word_index * out_word_width +
            out_word_offset;

        input_byte = input_data[in_index];
        output_data[out_index] =
            reverse_byte_bits ? byte_bit_reversal_table[input_byte] :
            input_byte;
      }
    }
  }
}

/**
 * gst_dsd_convert:
 * @input_data: the DSD format conversion's input source
 * @output_data: the DSD format conversion's output destination
 * @input_format: DSD format of the input data to convert from
 * @output_format: DSD format of the output data to convert to
 * @input_layout: Input data layout
 * @output_layout: Output data layout
 * @input_plane_offsets: Plane offsets for non-interleaved input data
 * @output_plane_offsets: Plane offsets for non-interleaved output data
 * @num_dsd_bytes: How many bytes with DSD data to convert
 * @num_channels: Number of channels (must be at least 1)
 * @reverse_byte_bits: If TRUE, reverse the bits in each DSD byte
 *
 * Converts DSD data from one layout and grouping format to another.
 * @num_bytes must be an integer multiple of the width of both input
 * and output format. For example, if the input format is GST_DSD_FORMAT_U32LE,
 * and the output format is GST_DSD_FORMAT_U16BE, then @num_bytes must
 * be an integer multiple of both 4 (U32LE width) and 2 (U16BE width).
 *
 * @reverse_byte_bits is necessary if the bit order within the DSD bytes
 * needs to be reversed. This is rarely necessary, and is not to be
 * confused with the endianness of formats (which determines the ordering
 * of *bytes*).
 *
 * @input_plane_offsets must not be NULL if @input_layout is set to
 * #GST_AUDIO_LAYOUT_NON_INTERLEAVED. The same applies to @output_plane_offsets.
 * These plane offsets define the starting offset of the planes (there is
 * exactly one plane per channel) within @input_data and @output_data
 * respectively. If GST_AUDIO_LAYOUT_INTERLEAVED is used, the plane offsets
 * are ignored.
 *
 * Since: 1.24
 */
void
gst_dsd_convert (const guint8 * input_data, guint8 * output_data,
    GstDsdFormat input_format, GstDsdFormat output_format,
    GstAudioLayout input_layout, GstAudioLayout output_layout,
    const gsize * input_plane_offsets, const gsize * output_plane_offsets,
    gsize num_dsd_bytes, gint num_channels, gboolean reverse_byte_bits)
{
  g_return_if_fail (input_data != NULL);
  g_return_if_fail (output_data != NULL);
  g_return_if_fail (input_format > GST_DSD_FORMAT_UNKNOWN
      && input_format < GST_NUM_DSD_FORMATS);
  g_return_if_fail (output_format > GST_DSD_FORMAT_UNKNOWN
      && output_format < GST_NUM_DSD_FORMATS);
  g_return_if_fail (input_layout == GST_AUDIO_LAYOUT_INTERLEAVED
      || input_plane_offsets != NULL);
  g_return_if_fail (output_layout == GST_AUDIO_LAYOUT_INTERLEAVED
      || output_plane_offsets != NULL);
  g_return_if_fail (num_dsd_bytes > 0);
  g_return_if_fail (
      (num_dsd_bytes % gst_dsd_format_get_width (input_format)) == 0);
  g_return_if_fail (
      (num_dsd_bytes % gst_dsd_format_get_width (output_format)) == 0);
  g_return_if_fail (num_channels > 0);

  GST_LOG ("converting DSD:  input: format %s layout %s  output: format %s "
      "layout %s  num channels: %d  num DSD bytes: %" G_GSIZE_FORMAT "  "
      "reverse byte bits: %d", gst_dsd_format_to_string (input_format),
      layout_to_string (input_layout), gst_dsd_format_to_string (output_format),
      layout_to_string (output_layout), num_channels, num_dsd_bytes,
      reverse_byte_bits);

  switch (input_layout) {
    case GST_AUDIO_LAYOUT_INTERLEAVED:
      switch (output_layout) {
        case GST_AUDIO_LAYOUT_INTERLEAVED:
          gst_dsd_convert_interleaved_to_interleaved (input_data, output_data,
              input_format, output_format, num_dsd_bytes, num_channels,
              reverse_byte_bits);
          break;

        case GST_AUDIO_LAYOUT_NON_INTERLEAVED:
          gst_dsd_convert_interleaved_to_non_interleaved (input_data,
              output_data, input_format, output_format, output_plane_offsets,
              num_dsd_bytes, num_channels, reverse_byte_bits);
          break;

        default:
          g_assert_not_reached ();
      }
      break;

    case GST_AUDIO_LAYOUT_NON_INTERLEAVED:
      switch (output_layout) {
        case GST_AUDIO_LAYOUT_INTERLEAVED:
          gst_dsd_convert_non_interleaved_to_interleaved (input_data,
              output_data, input_format, output_format, input_plane_offsets,
              num_dsd_bytes, num_channels, reverse_byte_bits);
          break;

        case GST_AUDIO_LAYOUT_NON_INTERLEAVED:
          gst_dsd_convert_non_interleaved_to_non_interleaved (input_data,
              output_data, input_format, output_format, input_plane_offsets,
              output_plane_offsets, num_dsd_bytes, num_channels,
              reverse_byte_bits);
          break;

        default:
          g_assert_not_reached ();
      }
      break;

    default:
      g_assert_not_reached ();
  }
}
