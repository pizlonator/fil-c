/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
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
 * SECTION:gstaudiochannels
 * @title: Audio-channels
 * @short_description: Support library for audio channel handling
 *
 * This library contains some helper functions for multichannel audio.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>

#include "audio-channels.h"

#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT ensure_debug_category()
static GstDebugCategory *
ensure_debug_category (void)
{
  static gsize cat_gonce = 0;

  if (g_once_init_enter (&cat_gonce)) {
    gsize cat_done;

    cat_done = (gsize) _gst_debug_category_new ("audio-channels", 0,
        "audio-channels object");

    g_once_init_leave (&cat_gonce, cat_done);
  }

  return (GstDebugCategory *) cat_gonce;
}
#else
#define ensure_debug_category() /* NOOP */
#endif /* GST_DISABLE_GST_DEBUG */


static const GstAudioChannelPosition default_channel_order[64] = {
  GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
  GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
  GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
  GST_AUDIO_CHANNEL_POSITION_LFE1,
  GST_AUDIO_CHANNEL_POSITION_REAR_LEFT,
  GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT,
  GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER,
  GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER,
  GST_AUDIO_CHANNEL_POSITION_REAR_CENTER,
  GST_AUDIO_CHANNEL_POSITION_LFE2,
  GST_AUDIO_CHANNEL_POSITION_SIDE_LEFT,
  GST_AUDIO_CHANNEL_POSITION_SIDE_RIGHT,
  GST_AUDIO_CHANNEL_POSITION_TOP_FRONT_LEFT,
  GST_AUDIO_CHANNEL_POSITION_TOP_FRONT_RIGHT,
  GST_AUDIO_CHANNEL_POSITION_TOP_FRONT_CENTER,
  GST_AUDIO_CHANNEL_POSITION_TOP_CENTER,
  GST_AUDIO_CHANNEL_POSITION_TOP_REAR_LEFT,
  GST_AUDIO_CHANNEL_POSITION_TOP_REAR_RIGHT,
  GST_AUDIO_CHANNEL_POSITION_TOP_SIDE_LEFT,
  GST_AUDIO_CHANNEL_POSITION_TOP_SIDE_RIGHT,
  GST_AUDIO_CHANNEL_POSITION_TOP_REAR_CENTER,
  GST_AUDIO_CHANNEL_POSITION_BOTTOM_FRONT_CENTER,
  GST_AUDIO_CHANNEL_POSITION_BOTTOM_FRONT_LEFT,
  GST_AUDIO_CHANNEL_POSITION_BOTTOM_FRONT_RIGHT,
  GST_AUDIO_CHANNEL_POSITION_WIDE_LEFT,
  GST_AUDIO_CHANNEL_POSITION_WIDE_RIGHT,
  GST_AUDIO_CHANNEL_POSITION_SURROUND_LEFT,
  GST_AUDIO_CHANNEL_POSITION_SURROUND_RIGHT,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID,
  GST_AUDIO_CHANNEL_POSITION_INVALID
};

/*
 * Compares @channels audio channel positions @p1 and @p2 if they are equal.
 * In other words, tells whether channel reordering is needed (unequal) or not (equal).
 *
 * Returns: %TRUE if the channel positions are equal, i.e. no reordering is needed.
 */
static gboolean
gst_audio_channel_positions_equal (const GstAudioChannelPosition * p1,
    const GstAudioChannelPosition * p2, gint channels)
{
  return memcmp (p1, p2, channels * sizeof (p1[0])) == 0;
}

static gboolean
check_valid_channel_positions (const GstAudioChannelPosition * position,
    gint channels, gboolean enforce_order, guint64 * channel_mask_out)
{
  gint i, j;
  guint64 channel_mask = 0;

  if (channels == 1 && position[0] == GST_AUDIO_CHANNEL_POSITION_MONO) {
    if (channel_mask_out)
      *channel_mask_out = 0;
    return TRUE;
  }

  if (channels > 0 && position[0] == GST_AUDIO_CHANNEL_POSITION_NONE) {
    if (channel_mask_out)
      *channel_mask_out = 0;
    return TRUE;
  }

  j = 0;
  for (i = 0; i < channels; i++) {
    while (j < G_N_ELEMENTS (default_channel_order)
        && default_channel_order[j] != position[i])
      j++;

    if (position[i] == GST_AUDIO_CHANNEL_POSITION_INVALID ||
        position[i] == GST_AUDIO_CHANNEL_POSITION_MONO ||
        position[i] == GST_AUDIO_CHANNEL_POSITION_NONE)
      return FALSE;

    /* Is this in valid channel order? */
    if (enforce_order && j == G_N_ELEMENTS (default_channel_order))
      return FALSE;
    j++;

    if ((channel_mask & (G_GUINT64_CONSTANT (1) << position[i])))
      return FALSE;

    channel_mask |= (G_GUINT64_CONSTANT (1) << position[i]);
  }

  if (channel_mask_out)
    *channel_mask_out = channel_mask;

  return TRUE;
}

/**
 * gst_audio_reorder_channels:
 * @data: (array length=size) (element-type guint8): The pointer to
 *   the memory.
 * @size: The size of the memory.
 * @format: The %GstAudioFormat of the buffer.
 * @channels: The number of channels.
 * @from: (array length=channels): The channel positions in the buffer.
 * @to: (array length=channels): The channel positions to convert to.
 *
 * Reorders @data from the channel positions @from to the channel
 * positions @to. @from and @to must contain the same number of
 * positions and the same positions, only in a different order.
 *
 * Note: this function assumes the audio data is in interleaved layout
 *
 * Returns: %TRUE if the reordering was possible.
 */
gboolean
gst_audio_reorder_channels (gpointer data, gsize size, GstAudioFormat format,
    gint channels, const GstAudioChannelPosition * from,
    const GstAudioChannelPosition * to)
{
  const GstAudioFormatInfo *info;
  gint i, j, n;
  gint reorder_map[64] = { 0, };
  guint8 *ptr;
  gint bpf, bps;
  guint8 tmp[64 * 8];

  info = gst_audio_format_get_info (format);

  g_return_val_if_fail (data != NULL, FALSE);
  g_return_val_if_fail (from != NULL, FALSE);
  g_return_val_if_fail (to != NULL, FALSE);
  g_return_val_if_fail (info != NULL && info->width > 0, FALSE);
  g_return_val_if_fail (info->width > 0, FALSE);
  g_return_val_if_fail (info->width <= 8 * 64, FALSE);
  g_return_val_if_fail (size % ((info->width * channels) / 8) == 0, FALSE);
  g_return_val_if_fail (channels > 0, FALSE);

  if (size == 0)
    return TRUE;

  if (gst_audio_channel_positions_equal (from, to, channels))
    return TRUE;

  g_return_val_if_fail (channels <= 64, FALSE);

  if (!gst_audio_get_channel_reorder_map (channels, from, to, reorder_map))
    return FALSE;

  bps = info->width / 8;
  bpf = bps * channels;
  ptr = data;

  n = size / bpf;
  for (i = 0; i < n; i++) {

    memcpy (tmp, ptr, bpf);
    for (j = 0; j < channels; j++)
      memcpy (ptr + reorder_map[j] * bps, tmp + j * bps, bps);

    ptr += bpf;
  }

  return TRUE;
}

static gboolean
gst_audio_meta_reorder_channels (GstAudioMeta * meta,
    const GstAudioChannelPosition * from, const GstAudioChannelPosition * to)
{
  gint reorder_map[64] = { 0, };
  gsize tmp_offsets[64] = { 0, };
  gint i;

  g_return_val_if_fail (meta, FALSE);
  g_return_val_if_fail (meta->info.channels > 0, FALSE);
  g_return_val_if_fail (meta->info.channels <= 64, FALSE);
  g_return_val_if_fail (meta->offsets != NULL, FALSE);

  if (!gst_audio_get_channel_reorder_map (meta->info.channels, from, to,
          reorder_map))
    return FALSE;

  memcpy (tmp_offsets, meta->offsets, meta->info.channels * sizeof (gsize));
  for (i = 0; i < meta->info.channels; i++) {
    meta->offsets[reorder_map[i]] = tmp_offsets[i];
  }

  return TRUE;
}

/**
 * gst_audio_buffer_reorder_channels:
 * @buffer: The buffer to reorder.
 * @format: The %GstAudioFormat of the buffer.
 * @channels: The number of channels.
 * @from: (array length=channels): The channel positions in the buffer.
 * @to: (array length=channels): The channel positions to convert to.
 *
 * Reorders @buffer from the channel positions @from to the channel
 * positions @to. @from and @to must contain the same number of
 * positions and the same positions, only in a different order.
 * @buffer must be writable.
 *
 * Returns: %TRUE if the reordering was possible.
 */
gboolean
gst_audio_buffer_reorder_channels (GstBuffer * buffer,
    GstAudioFormat format, gint channels,
    const GstAudioChannelPosition * from, const GstAudioChannelPosition * to)
{
  GstMapInfo info;
  GstAudioMeta *meta;
  gboolean ret = TRUE;

  g_return_val_if_fail (GST_IS_BUFFER (buffer), FALSE);
  g_return_val_if_fail (gst_buffer_is_writable (buffer), FALSE);

  if (gst_audio_channel_positions_equal (from, to, channels))
    return TRUE;

  meta = gst_buffer_get_audio_meta (buffer);
  if (meta && meta->info.layout == GST_AUDIO_LAYOUT_NON_INTERLEAVED) {
    g_return_val_if_fail (channels == meta->info.channels, FALSE);

    ret = gst_audio_meta_reorder_channels (meta, from, to);
  } else {
    if (!gst_buffer_map (buffer, &info, GST_MAP_READWRITE))
      return FALSE;

    ret = gst_audio_reorder_channels (info.data, info.size, format, channels,
        from, to);

    gst_buffer_unmap (buffer, &info);
  }
  return ret;
}

/**
 * gst_audio_check_valid_channel_positions:
 * @position: (array length=channels): The %GstAudioChannelPositions
 *   to check.
 * @channels: The number of channels.
 * @force_order: Only consider the GStreamer channel order.
 *
 * Checks if @position contains valid channel positions for
 * @channels channels. If @force_order is %TRUE it additionally
 * checks if the channels are in the order required by GStreamer.
 *
 * Returns: %TRUE if the channel positions are valid.
 */
gboolean
gst_audio_check_valid_channel_positions (const GstAudioChannelPosition *
    position, gint channels, gboolean force_order)
{
  return check_valid_channel_positions (position, channels, force_order, NULL);
}

/**
 * gst_audio_channel_positions_to_mask:
 * @position: (array length=channels): The %GstAudioChannelPositions
 * @channels: The number of channels.
 * @force_order: Only consider the GStreamer channel order.
 * @channel_mask: (out): the output channel mask
 *
 * Convert the @position array of @channels channels to a bitmask.
 *
 * If @force_order is %TRUE it additionally checks if the channels are
 * in the order required by GStreamer.
 *
 * Returns: %TRUE if the channel positions are valid and could be converted.
 */
gboolean
gst_audio_channel_positions_to_mask (const GstAudioChannelPosition * position,
    gint channels, gboolean force_order, guint64 * channel_mask)
{
  return check_valid_channel_positions (position, channels, force_order,
      channel_mask);
}

/**
 * gst_audio_channel_positions_from_mask:
 * @channels: The number of channels
 * @channel_mask: The input channel_mask
 * @position: (array length=channels): The
 *   %GstAudioChannelPosition<!-- -->s
 *
 * Convert the @channels present in @channel_mask to a @position array
 * (which should have at least @channels entries ensured by caller).
 * If @channel_mask is set to 0, it is considered as 'not present' for purpose
 * of conversion.
 * A partially valid @channel_mask with less bits set than the number
 * of channels is considered valid.
 *
 * Returns: %TRUE if channel and channel mask are valid and could be converted
 */
gboolean
gst_audio_channel_positions_from_mask (gint channels, guint64 channel_mask,
    GstAudioChannelPosition * position)
{
  g_return_val_if_fail (position != NULL, FALSE);
  g_return_val_if_fail (channels != 0, FALSE);

  GST_DEBUG ("converting %d channels for "
      " channel mask 0x%016" G_GINT64_MODIFIER "x", channels, channel_mask);

  if (!channel_mask) {
    if (channels == 1) {
      position[0] = GST_AUDIO_CHANNEL_POSITION_MONO;
    } else if (channels == 2) {
      position[0] = GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT;
      position[1] = GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT;
    } else {
      goto no_channel_mask;
    }
  } else {
    gint i, j;

    j = 0;
    for (i = 0; i < 64; i++) {
      if ((channel_mask & (G_GUINT64_CONSTANT (1) << i))) {
        if (j < channels)
          position[j] = default_channel_order[i];
        j++;
      }
    }
    if (j != channels)
      GST_WARNING ("Only partially valid channel mask 0x%016" G_GINT64_MODIFIER
          "x for %d channels", channel_mask, channels);
  }

  return TRUE;

  /* ERROR */
no_channel_mask:
  {
    GST_ERROR ("no channel-mask property given");
    return FALSE;
  }
}


/**
 * gst_audio_get_channel_reorder_map:
 * @channels: The number of channels.
 * @from: (array length=channels): The channel positions to reorder from.
 * @to: (array length=channels): The channel positions to reorder to.
 * @reorder_map: (array length=channels): Pointer to the reorder map.
 *
 * Returns a reorder map for @from to @to that can be used in
 * custom channel reordering code, e.g. to convert from or to the
 * GStreamer channel order. @from and @to must contain the same
 * number of positions and the same positions, only in a
 * different order.
 *
 * The resulting @reorder_map can be used for reordering by assigning
 * channel i of the input to channel reorder_map[i] of the output.
 *
 * Returns: %TRUE if the channel positions are valid and reordering
 * is possible.
 */
gboolean
gst_audio_get_channel_reorder_map (gint channels,
    const GstAudioChannelPosition * from, const GstAudioChannelPosition * to,
    gint * reorder_map)
{
  gint i, j;

  g_return_val_if_fail (reorder_map != NULL, FALSE);
  g_return_val_if_fail (channels > 0, FALSE);
  g_return_val_if_fail (channels <= 64, FALSE);
  g_return_val_if_fail (from != NULL, FALSE);
  g_return_val_if_fail (to != NULL, FALSE);
  g_return_val_if_fail (check_valid_channel_positions (from, channels, FALSE,
          NULL), FALSE);
  g_return_val_if_fail (check_valid_channel_positions (to, channels, FALSE,
          NULL), FALSE);

  /* Build reorder map and check compatibility */
  for (i = 0; i < channels; i++) {
    if (from[i] == GST_AUDIO_CHANNEL_POSITION_NONE
        || to[i] == GST_AUDIO_CHANNEL_POSITION_NONE)
      return FALSE;
    if (from[i] == GST_AUDIO_CHANNEL_POSITION_INVALID
        || to[i] == GST_AUDIO_CHANNEL_POSITION_INVALID)
      return FALSE;
    if (from[i] == GST_AUDIO_CHANNEL_POSITION_MONO
        || to[i] == GST_AUDIO_CHANNEL_POSITION_MONO)
      return FALSE;

    for (j = 0; j < channels; j++) {
      if (from[i] == to[j]) {
        reorder_map[i] = j;
        break;
      }
    }

    /* Not all channels present in both */
    if (j == channels)
      return FALSE;
  }

  return TRUE;
}

/**
 * gst_audio_channel_positions_to_valid_order:
 * @position: (array length=channels): The channel positions to
 *   reorder to.
 * @channels: The number of channels.
 *
 * Reorders the channel positions in @position from any order to
 * the GStreamer channel order.
 *
 * Returns: %TRUE if the channel positions are valid and reordering
 * was successful.
 */
gboolean
gst_audio_channel_positions_to_valid_order (GstAudioChannelPosition * position,
    gint channels)
{
  GstAudioChannelPosition tmp[64];
  guint64 channel_mask = 0;
  gint i, j;

  g_return_val_if_fail (channels > 0, FALSE);
  g_return_val_if_fail (position != NULL, FALSE);
  g_return_val_if_fail (check_valid_channel_positions (position, channels,
          FALSE, NULL), FALSE);

  if (channels == 1 && position[0] == GST_AUDIO_CHANNEL_POSITION_MONO)
    return TRUE;
  if (position[0] == GST_AUDIO_CHANNEL_POSITION_NONE)
    return TRUE;

  check_valid_channel_positions (position, channels, FALSE, &channel_mask);

  memset (tmp, 0xff, sizeof (tmp));
  j = 0;
  for (i = 0; i < 64; i++) {
    if ((channel_mask & (G_GUINT64_CONSTANT (1) << i))) {
      tmp[j] = i;
      j++;
    }
  }

  memcpy (position, tmp, sizeof (tmp[0]) * channels);

  return TRUE;
}

#define _P(pos) (G_GUINT64_CONSTANT (1) << GST_AUDIO_CHANNEL_POSITION_ ##pos)

static const guint64 default_masks[] = {
  /* 1 channel */
  0,
  /* 2 channels */
  _P (FRONT_LEFT) | _P (FRONT_RIGHT),
  /* 3 channels (2.1) */
  _P (FRONT_LEFT) | _P (FRONT_RIGHT) | _P (LFE1),
  /* 4 channels (4.0) */
  _P (FRONT_LEFT) | _P (FRONT_RIGHT) | _P (REAR_LEFT) | _P (REAR_RIGHT),
  /* 5 channels */
  _P (FRONT_LEFT) | _P (FRONT_RIGHT) | _P (REAR_LEFT) | _P (REAR_RIGHT)
      | _P (FRONT_CENTER),
  /* 6 channels (5.1) */
  _P (FRONT_LEFT) |
      _P (FRONT_RIGHT) |
      _P (REAR_LEFT) | _P (REAR_RIGHT) | _P (FRONT_CENTER) | _P (LFE1),
  /* 7 channels (6.1) */
  _P (FRONT_LEFT) |
      _P (FRONT_RIGHT) |
      _P (REAR_LEFT) |
      _P (REAR_RIGHT) | _P (FRONT_CENTER) | _P (LFE1) | _P (REAR_CENTER),
  /* 8 channels (7.1) */
  _P (FRONT_LEFT) |
      _P (FRONT_RIGHT) |
      _P (REAR_LEFT) |
      _P (REAR_RIGHT) |
      _P (FRONT_CENTER) | _P (LFE1) | _P (SIDE_LEFT) | _P (SIDE_RIGHT),
};

/**
 * gst_audio_channel_get_fallback_mask:
 * @channels: the number of channels
 *
 * Get the fallback channel-mask for the given number of channels.
 *
 * This function returns a reasonable fallback channel-mask and should be
 * called as a last resort when the specific channel map is unknown.
 *
 * Returns: a fallback channel-mask for @channels or 0 when there is no
 * mask and mono.
 *
 * Since: 1.8
 */
guint64
gst_audio_channel_get_fallback_mask (gint channels)
{
  g_return_val_if_fail (channels > 0, 0);

  if (channels > 8)
    return 0;

  return default_masks[channels - 1];
}

static const gchar *
position_to_string (GstAudioChannelPosition pos)
{
  switch (pos) {
    case GST_AUDIO_CHANNEL_POSITION_NONE:
      return "NONE";
    case GST_AUDIO_CHANNEL_POSITION_MONO:
      return "MONO";
    case GST_AUDIO_CHANNEL_POSITION_INVALID:
      return "INVALID";
    case GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT:
      return "FL";
    case GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT:
      return "FR";
    case GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER:
      return "FC";
    case GST_AUDIO_CHANNEL_POSITION_LFE1:
      return "LFE1";
    case GST_AUDIO_CHANNEL_POSITION_REAR_LEFT:
      return "RL";
    case GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT:
      return "RR";
    case GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER:
      return "FLoC";
    case GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER:
      return "FRoC";
    case GST_AUDIO_CHANNEL_POSITION_REAR_CENTER:
      return "RC";
    case GST_AUDIO_CHANNEL_POSITION_LFE2:
      return "LF2";
    case GST_AUDIO_CHANNEL_POSITION_SIDE_LEFT:
      return "SL";
    case GST_AUDIO_CHANNEL_POSITION_SIDE_RIGHT:
      return "SR";
    case GST_AUDIO_CHANNEL_POSITION_TOP_FRONT_LEFT:
      return "TFL";
    case GST_AUDIO_CHANNEL_POSITION_TOP_FRONT_RIGHT:
      return "TFR";
    case GST_AUDIO_CHANNEL_POSITION_TOP_FRONT_CENTER:
      return "TFC";
    case GST_AUDIO_CHANNEL_POSITION_TOP_CENTER:
      return "TFC";
    case GST_AUDIO_CHANNEL_POSITION_TOP_REAR_LEFT:
      return "TRL";
    case GST_AUDIO_CHANNEL_POSITION_TOP_REAR_RIGHT:
      return "TRR";
    case GST_AUDIO_CHANNEL_POSITION_TOP_SIDE_LEFT:
      return "TSL";
    case GST_AUDIO_CHANNEL_POSITION_TOP_SIDE_RIGHT:
      return "TSR";
    case GST_AUDIO_CHANNEL_POSITION_TOP_REAR_CENTER:
      return "TRC";
    case GST_AUDIO_CHANNEL_POSITION_BOTTOM_FRONT_CENTER:
      return "BFC";
    case GST_AUDIO_CHANNEL_POSITION_BOTTOM_FRONT_LEFT:
      return "BFL";
    case GST_AUDIO_CHANNEL_POSITION_BOTTOM_FRONT_RIGHT:
      return "BFR";
    case GST_AUDIO_CHANNEL_POSITION_WIDE_LEFT:
      return "WL";
    case GST_AUDIO_CHANNEL_POSITION_WIDE_RIGHT:
      return "WR";
    case GST_AUDIO_CHANNEL_POSITION_SURROUND_LEFT:
      return "SL";
    case GST_AUDIO_CHANNEL_POSITION_SURROUND_RIGHT:
      return "SR";
    default:
      break;
  }

  return "UNKNOWN";
}

/**
 * gst_audio_channel_positions_to_string:
 * @position: (array length=channels): The %GstAudioChannelPositions
 *   to convert.
 * @channels: The number of channels.
 *
 * Converts @position to a human-readable string representation for
 * debugging purposes.
 *
 * Returns: (transfer full): a newly allocated string representing
 * @position
 *
 * Since: 1.10
 */
gchar *
gst_audio_channel_positions_to_string (const GstAudioChannelPosition * position,
    gint channels)
{
  guint i;
  GString *tmp;

  g_return_val_if_fail (channels > 0, FALSE);
  g_return_val_if_fail (position != NULL, FALSE);

  tmp = g_string_new ("[");
  for (i = 0; i < channels; i++)
    g_string_append_printf (tmp, " %s", position_to_string (position[i]));
  g_string_append (tmp, " ]");

  return g_string_free (tmp, FALSE);
}
