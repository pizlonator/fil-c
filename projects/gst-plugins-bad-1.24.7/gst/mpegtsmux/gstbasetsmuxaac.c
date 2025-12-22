/*
 * Copyright 2006, 2007, 2008 Fluendo S.A.
 *  Authors: Jan Schmidt <jan@fluendo.com>
 *           Kapil Agrawal <kapil@fluendo.com>
 *           Julien Moutte <julien@fluendo.com>
 *
 * This library is licensed under 3 different licenses and you
 * can choose to use it under the terms of any one of them. The
 * three licenses are the MPL 1.1, the LGPL and the MIT license.
 *
 * MPL:
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/.
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * LGPL:
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
 *
 * MIT:
 *
 * Unless otherwise indicated, Source Code is licensed under MIT license.
 * See further explanation attached in License Statement (distributed in the file
 * LICENSE).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MPL-1.1 OR MIT OR LGPL-2.0-or-later
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/pbutils/pbutils.h>

#include "gstbasetsmuxaac.h"
#include <string.h>

#define GST_CAT_DEFAULT gst_base_ts_mux_debug

static GstBuffer *
gst_base_ts_mux_prepare_aac_adts (GstBuffer * buf,
    GstBaseTsMux * mux, gboolean is_mpeg2, guint8 obj_type_profile,
    guint8 rate_idx, guint8 channels)
{
  guint8 adts_header[7] = { 0, };
  gsize out_size = gst_buffer_get_size (buf) + 7;
  GstBuffer *out_buf = gst_buffer_new_and_alloc (out_size);
  gsize out_offset = 0;
  GstMapInfo buf_map;

  /* Generate ADTS header */
  GST_DEBUG_OBJECT (mux, "Preparing AAC buffer for output");

  gst_buffer_copy_into (out_buf, buf,
      GST_BUFFER_COPY_METADATA | GST_BUFFER_COPY_TIMESTAMPS, 0, 0);

  GST_DEBUG_OBJECT (mux, "Rate index %u, channels %u, object type/profile %u",
      rate_idx, channels, obj_type_profile);

  /* Sync point over a full byte */
  adts_header[0] = 0xFF;
  /* Sync point continued over first 4 bits + static 4 bits
   * (ID, layer, protection)*/
  adts_header[1] = 0xF1 | (is_mpeg2 ? 0x8 : 0x0);
  /* Object type (MPEG4) / Profile (MPEG2) over first 2 bits */
  adts_header[2] = (obj_type_profile - 1) << 6;
  /* rate index over next 4 bits */
  adts_header[2] |= (rate_idx << 2);
  /* channels over last 2 bits */
  adts_header[2] |= (channels & 0x4) >> 2;
  /* channels continued over next 2 bits + 4 bits at zero */
  adts_header[3] = (channels & 0x3) << 6;
  /* frame size over last 2 bits */
  adts_header[3] |= (out_size & 0x1800) >> 11;
  /* frame size continued over full byte */
  adts_header[4] = (out_size & 0x1FF8) >> 3;
  /* frame size continued first 3 bits */
  adts_header[5] = (out_size & 0x7) << 5;
  /* buffer fullness (0x7FF for VBR) over 5 last bits */
  adts_header[5] |= 0x1F;
  /* buffer fullness (0x7FF for VBR) continued over 6 first bits + 2 zeros for
   * number of raw data blocks */
  adts_header[6] = 0xFC;

  /* Insert ADTS header */
  gst_buffer_fill (out_buf, out_offset, adts_header, 7);
  out_offset += 7;

  gst_buffer_map (buf, &buf_map, GST_MAP_READ);

  /* Now copy complete frame */
  gst_buffer_fill (out_buf, out_offset, buf_map.data, buf_map.size);

  gst_buffer_unmap (buf, &buf_map);

  return out_buf;
}

/* Constructs a dummy codec_data buffer for generating ADTS headers
 * from raw MPEG-2 AAC input, where we don't expect codec_data in the caps,
 * and need to get the info from the profile/channels/rate fields */
GstBuffer *
gst_base_ts_mux_aac_mpeg2_make_codec_data (GstBaseTsMux * mux,
    const GstCaps * caps)
{
  const GstStructure *s;
  const gchar *profile_str;
  gint channels, rate;
  guint8 profile_idx, channel_idx;
  gint rate_idx;
  GstMapInfo map;
  GstBuffer *ret;

  s = gst_caps_get_structure (caps, 0);
  profile_str = gst_structure_get_string (s, "profile");
  if (G_UNLIKELY (profile_str == NULL)) {
    GST_ERROR_OBJECT (mux, "AAC caps do not contain profile");
    return NULL;
  }

  if (G_UNLIKELY (!gst_structure_get_int (s, "rate", &rate))) {
    GST_ERROR_OBJECT (mux, "AAC caps do not contain a sample rate");
    return NULL;
  }
  if (G_UNLIKELY (!gst_structure_get_int (s, "channels", &channels))) {
    GST_ERROR_OBJECT (mux, "AAC caps do not contain channel count");
    return NULL;
  }

  if (g_strcmp0 (profile_str, "main") == 0) {
    profile_idx = (guint8) 0U;
  } else if (g_strcmp0 (profile_str, "lc") == 0) {
    profile_idx = (guint8) 1U;
  } else if (g_strcmp0 (profile_str, "ssr") == 0) {
    profile_idx = (guint8) 2U;
  } else {
    GST_ERROR_OBJECT (mux, "Invalid profile %s for MPEG-2 AAC caps",
        profile_str);
    return NULL;
  }

  if (channels >= 1 && channels <= 6)   /* Mono up to & including 5.1 */
    channel_idx = (guint8) channels;
  else if (channels == 8)       /* 7.1 */
    channel_idx = (guint8) 7U;
  else {
    GST_ERROR_OBJECT (mux, "Invalid channel count %d for MPEG-2 AAC caps",
        channels);
    return NULL;
  }

  rate_idx = gst_codec_utils_aac_get_index_from_sample_rate (rate);
  if (rate_idx < 0) {
    GST_ERROR_OBJECT (mux, "Invalid samplerate %d for MPEG-2 AAC caps", rate);
    return NULL;
  }

  ret = gst_buffer_new_and_alloc (3);
  gst_buffer_map (ret, &map, GST_MAP_READ);
  map.data[0] = profile_idx;
  map.data[1] = (guint8) rate_idx;
  map.data[2] = channel_idx;
  gst_buffer_unmap (ret, &map);

  return ret;
}

GstBuffer *
gst_base_ts_mux_prepare_aac_mpeg4 (GstBuffer * buf, GstBaseTsMuxPad * pad,
    GstBaseTsMux * mux)
{
  GstMapInfo codec_data_map;
  guint8 rate_idx = 0, channels = 0, obj_type = 0;

  g_return_val_if_fail (pad->codec_data != NULL, NULL);

  gst_buffer_map (pad->codec_data, &codec_data_map, GST_MAP_READ);

  obj_type = GST_READ_UINT8 (codec_data_map.data) >> 3;
  rate_idx = (GST_READ_UINT8 (codec_data_map.data) & 0x7) << 1;
  rate_idx |= (GST_READ_UINT8 (codec_data_map.data + 1) & 0x80) >> 7;
  channels = (GST_READ_UINT8 (codec_data_map.data + 1) & 0x78) >> 3;
  gst_buffer_unmap (pad->codec_data, &codec_data_map);

  return gst_base_ts_mux_prepare_aac_adts (buf, mux, FALSE, obj_type, rate_idx,
      channels);
}

GstBuffer *
gst_base_ts_mux_prepare_aac_mpeg2 (GstBuffer * buf, GstBaseTsMuxPad * pad,
    GstBaseTsMux * mux)
{
  GstMapInfo codec_data_map;
  guint8 rate_idx = 0, channels = 0, profile_obj_type = 0;

  g_return_val_if_fail (pad->codec_data != NULL, NULL);

  /* Dummy codec data with 3 bytes of profile_idx, rate_idx, channel_idx */
  gst_buffer_map (pad->codec_data, &codec_data_map, GST_MAP_READ);

  profile_obj_type = GST_READ_UINT8 (codec_data_map.data);
  rate_idx = GST_READ_UINT8 (codec_data_map.data + 1);
  channels = GST_READ_UINT8 (codec_data_map.data + 2);

  gst_buffer_unmap (pad->codec_data, &codec_data_map);

  return gst_base_ts_mux_prepare_aac_adts (buf, mux, TRUE, profile_obj_type,
      rate_idx, channels);
}
