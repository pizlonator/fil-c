/* GStreamer
 * Copyright (C) 2009 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

#include <stdio.h>
#include <string.h>
#include "gstopuscommon.h"

/* http://www.xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-800004.3.9 */
/* copy of the same structure in the vorbis plugin */
const GstAudioChannelPosition gst_opus_channel_positions[][8] = {
  {                             /* Mono */
      GST_AUDIO_CHANNEL_POSITION_MONO},
  {                             /* Stereo */
        GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
      GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT},
  {                             /* Stereo + Centre */
        GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
        GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
      GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT},
  {                             /* Quadraphonic */
        GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
        GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
        GST_AUDIO_CHANNEL_POSITION_REAR_LEFT,
        GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT,
      },
  {                             /* Stereo + Centre + rear stereo */
        GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
        GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
        GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
        GST_AUDIO_CHANNEL_POSITION_REAR_LEFT,
        GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT,
      },
  {                             /* Full 5.1 Surround */
        GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
        GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
        GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
        GST_AUDIO_CHANNEL_POSITION_REAR_LEFT,
        GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT,
        GST_AUDIO_CHANNEL_POSITION_LFE1,
      },
  {                             /* 6.1 Surround, in Vorbis spec since 2010-01-13 */
        GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
        GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
        GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
        GST_AUDIO_CHANNEL_POSITION_SIDE_LEFT,
        GST_AUDIO_CHANNEL_POSITION_SIDE_RIGHT,
        GST_AUDIO_CHANNEL_POSITION_REAR_CENTER,
      GST_AUDIO_CHANNEL_POSITION_LFE1},
  {                             /* 7.1 Surround, in Vorbis spec since 2010-01-13 */
        GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
        GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
        GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
        GST_AUDIO_CHANNEL_POSITION_SIDE_LEFT,
        GST_AUDIO_CHANNEL_POSITION_SIDE_RIGHT,
        GST_AUDIO_CHANNEL_POSITION_REAR_LEFT,
        GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT,
      GST_AUDIO_CHANNEL_POSITION_LFE1},
};

const char *gst_opus_channel_names[] = {
  "mono",
  "front left",
  "front right",
  "rear center",
  "rear left",
  "rear right",
  "lfe",
  "front center",
  "front left of center",
  "front right of center",
  "side left",
  "side right",
  "none"
};

void
gst_opus_common_log_channel_mapping_table (GstElement * element,
    GstDebugCategory * category, const char *msg, int n_channels,
    const guint8 * table)
{
  int n;
  GString *s;

  if (gst_debug_category_get_threshold (category) < GST_LEVEL_INFO)
    return;

  s = g_string_new ("[ ");
  for (n = 0; n < n_channels; ++n) {
    g_string_append_printf (s, "%d ", table[n]);
  }
  g_string_append (s, "]");

  GST_CAT_LEVEL_LOG (category, GST_LEVEL_INFO, element, "%s: %s", msg, s->str);
  g_string_free (s, TRUE);
}
