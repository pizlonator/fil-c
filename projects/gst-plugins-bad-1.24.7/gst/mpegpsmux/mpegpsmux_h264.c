/* 
 * Copyright 2006, 2007, 2008 Fluendo S.A. 
 *  Authors: Jan Schmidt <jan@fluendo.com>
 *           Kapil Agrawal <kapil@fluendo.com>
 *           Julien Moutte <julien@fluendo.com>
 *
 * Copyright 2008 Lin YANG <oxcsnicho@gmail.com>
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

#include "mpegpsmux_h264.h"
#include <gst/base/gstbytewriter.h>
#include <string.h>

GST_DEBUG_CATEGORY_EXTERN (mpegpsmux_debug);
#define GST_CAT_DEFAULT mpegpsmux_debug

GstBuffer *
mpegpsmux_prepare_h264 (GstBuffer * buf, MpegPsPadData * data, MpegPsMux * mux)
{
  GstByteWriter bw;
  GstMapInfo codec_data, map;
  guint8 nal_length_size = 0;
  GstBuffer *out_buf;
  guint8 nb_sps = 0, nb_pps = 0;
  gint offset = 4, i = 0;
  gsize in_offset;

  GST_DEBUG_OBJECT (mux, "Preparing H264 buffer for output");

  /* FIXME: are we prepending SPS/PPS in front of every single buffer?
   * (should only be in front of keyframes really, if at all) */
  /* FIXME: create a byte-stream version of SPS/PPS once in set_caps */
  if (!gst_buffer_map (data->codec_data, &codec_data, GST_MAP_READ))
    return NULL;

  gst_byte_writer_init_with_size (&bw, gst_buffer_get_size (buf) * 2, FALSE);

  /* Get NAL length size */
  nal_length_size = (codec_data.data[offset] & 0x03) + 1;
  GST_LOG_OBJECT (mux, "NAL length will be coded on %u bytes", nal_length_size);
  offset++;

  /* Generate SPS */
  nb_sps = codec_data.data[offset] & 0x1f;
  GST_DEBUG_OBJECT (mux, "we have %d Sequence Parameter Set", nb_sps);
  offset++;

  /* For each SPS */
  for (i = 0; i < nb_sps; i++) {
    guint16 sps_size = GST_READ_UINT16_BE (codec_data.data + offset);

    GST_LOG_OBJECT (mux, "Sequence Parameter Set is %d bytes", sps_size);

    /* Jump over SPS size */
    offset += 2;

    /* Fake a start code */
    gst_byte_writer_put_uint32_be (&bw, 0x00000001);

    /* Now push the SPS */
    gst_byte_writer_put_data (&bw, codec_data.data + offset, sps_size);

    offset += sps_size;
  }

  nb_pps = codec_data.data[offset];
  GST_LOG_OBJECT (mux, "we have %d Picture Parameter Set", nb_sps);
  offset++;

  /* For each PPS */
  for (i = 0; i < nb_pps; i++) {
    gint pps_size = GST_READ_UINT16_BE (codec_data.data + offset);

    GST_LOG_OBJECT (mux, "Picture Parameter Set is %d bytes", pps_size);

    /* Jump over PPS size */
    offset += 2;

    /* Fake a start code */
    gst_byte_writer_put_uint32_be (&bw, 0x00000001);

    /* Now push the PPS */
    gst_byte_writer_put_data (&bw, codec_data.data + offset, pps_size);

    offset += pps_size;
  }

  gst_buffer_unmap (data->codec_data, &codec_data);

  if (!gst_buffer_map (buf, &map, GST_MAP_READ))
    return NULL;

  /* now process NALs and change them to byte-stream format */
  in_offset = 0;
  while (in_offset < map.size) {
    guint32 nal_size = 0;

    switch (nal_length_size) {
      case 1:
        nal_size = GST_READ_UINT8 (map.data + in_offset);
        break;
      case 2:
        nal_size = GST_READ_UINT16_BE (map.data + in_offset);
        break;
      case 4:
        nal_size = GST_READ_UINT32_BE (map.data + in_offset);
        break;
      default:
        GST_WARNING_OBJECT (mux, "unsupported NAL length size %u",
            nal_length_size);
        break;
    }
    in_offset += nal_length_size;

    /* Generate an Elementary stream buffer by inserting a startcode */
    gst_byte_writer_put_uint32_be (&bw, 0x00000001);
    gst_byte_writer_put_data (&bw, map.data + in_offset,
        MIN (nal_size, map.size - in_offset));
    in_offset += nal_size;
  }
  gst_buffer_unmap (buf, &map);

  out_buf = gst_byte_writer_reset_and_get_buffer (&bw);

  /* We want the same metadata */
  gst_buffer_copy_into (out_buf, buf, GST_BUFFER_COPY_METADATA, 0, 0);

  return out_buf;
}
