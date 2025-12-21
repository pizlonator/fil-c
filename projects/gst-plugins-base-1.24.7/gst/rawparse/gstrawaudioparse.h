/* GStreamer
 * Copyright (C) <2016> Carlos Rafael Giani <dv at pseudoterminal dot org>
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

#ifndef __GST_RAW_AUDIO_PARSE_H__
#define __GST_RAW_AUDIO_PARSE_H__

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include "gstrawbaseparse.h"

G_BEGIN_DECLS

#define GST_TYPE_RAW_AUDIO_PARSE (gst_raw_audio_parse_get_type())
#define GST_RAW_AUDIO_PARSE_CAST(obj) ((GstRawAudioParse *)(obj))
G_DECLARE_FINAL_TYPE (GstRawAudioParse, gst_raw_audio_parse,
    GST, RAW_AUDIO_PARSE, GstRawBaseParse)

typedef enum _GstRawAudioParseFormat GstRawAudioParseFormat;

typedef struct _GstRawAudioParseConfig GstRawAudioParseConfig;

enum _GstRawAudioParseFormat
{
  GST_RAW_AUDIO_PARSE_FORMAT_PCM,
  GST_RAW_AUDIO_PARSE_FORMAT_MULAW,
  GST_RAW_AUDIO_PARSE_FORMAT_ALAW
};

/* Contains information about the sample rate, format, and channel count to use. */
struct _GstRawAudioParseConfig
{
  /* If TRUE, then this configuration is ready to use */
  gboolean ready;
  /* Format of the configuration. Can be PCM, a-law, mu-law. */
  GstRawAudioParseFormat format;
  /* If format is set to PCM, this specifies the exact PCM format in use.
   * Meaningless if format is set to anything other than PCM. */
  GstAudioFormat pcm_format;
  /* Bytes per frame. Calculated as: bpf = bytes_per_sample * num_channels
   * Must be nonzero. This is the size of one frame, the value returned
   * by the GstRawBaseParseClass get_config_frame_size() vfunc. */
  guint bpf;
  /* Sample rate in Hz - must be nonzero */
  guint sample_rate;
  /* Number of channels - must be nonzero */
  guint num_channels;
  /* TRUE if the data is interleaved, FALSE otherwise */
  gboolean interleaved;

  /* Array of channel positions, one position per channel; its first
   * num_channels values are valid. They are computed out of the number
   * of channels if no positions are explicitly given. */
  GstAudioChannelPosition channel_positions[64];

  /* If the channel_positions are in a valid GStreamer channel order, then
   * this is not used, and needs_channel_reordering is FALSE. Otherwise,
   * this contains the same positions as in channel_positions, but in the
   * order GStreamer expects. needs_channel_reordering will be TRUE in that
   * case. This is used for reordering samples in outgoing buffers if
   * necessary. */
  GstAudioChannelPosition reordered_channel_positions[64];

  /* TRUE if channel reordering is necessary, FALSE otherwise. See above
   * for details. */
  gboolean needs_channel_reordering;
};

struct _GstRawAudioParse
{
  GstRawBaseParse parent;

  /*< private > */

  /* Configuration controlled by the object properties. Its ready value
   * is set to TRUE from the start, so it can be used right away.
   */
  GstRawAudioParseConfig properties_config;
  /* Configuration controlled by the sink caps. Its ready value is
   * initially set to FALSE until valid sink caps come in. It is set to
   * FALSE again when the stream-start event is observed.
   */
  GstRawAudioParseConfig sink_caps_config;
  /* Currently active configuration. Points either to properties_config
   * or to sink_caps_config. This is never NULL. */
  GstRawAudioParseConfig *current_config;
};

#define GST_TYPE_RAW_AUDIO_PARSE_FORMAT (gst_raw_audio_parse_format_get_type())
GType gst_raw_audio_parse_format_get_type (void);

G_END_DECLS

#endif
