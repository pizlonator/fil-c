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

#include "mpegpsmux_aac.h"
#include <string.h>

GST_DEBUG_CATEGORY_EXTERN (mpegpsmux_debug);
#define GST_CAT_DEFAULT mpegpsmux_debug

GstBuffer *
mpegpsmux_prepare_aac (GstBuffer * buf, MpegPsPadData * data, MpegPsMux * mux)
{
  GstBuffer *out_buf;
  GstMemory *mem;
  gsize out_size;
  guint8 *adts_header, codec_data[2];
  guint8 rate_idx = 0, channels = 0, obj_type = 0;

  GST_DEBUG_OBJECT (mux, "Preparing AAC buffer for output");

  adts_header = g_malloc0 (7);

  /* We want the same data and metadata, and then prepend some bytes */
  out_buf = gst_buffer_copy (buf);
  out_size = gst_buffer_get_size (buf) + 7;

  gst_buffer_extract (data->codec_data, 0, codec_data, 2);

  /* Generate ADTS header */
  obj_type = (codec_data[0] & 0xC) >> 2;
  obj_type++;
  rate_idx = (codec_data[0] & 0x3) << 1;
  rate_idx |= (codec_data[1] & 0x80) >> 7;
  channels = (codec_data[1] & 0x78) >> 3;
  GST_DEBUG_OBJECT (mux, "Rate index %u, channels %u, object type %u", rate_idx,
      channels, obj_type);
  /* Sync point over a full byte */
  adts_header[0] = 0xFF;
  /* Sync point continued over first 4 bits + static 4 bits
   * (ID, layer, protection)*/
  adts_header[1] = 0xF1;
  /* Object type over first 2 bits */
  adts_header[2] = obj_type << 6;
  /* rate index over next 4 bits */
  adts_header[2] |= (rate_idx << 2);
  /* channels over last 2 bits */
  adts_header[2] |= (channels & 0x4) >> 2;
  /* channels continued over next 2 bits + 4 bits at zero */
  adts_header[3] = (channels & 0x3) << 6;
  /* frame size over last 2 bits */
  adts_header[3] |= (gst_buffer_get_size (out_buf) & 0x1800) >> 11;
  /* frame size continued over full byte */
  adts_header[4] = (out_size & 0x1FF8) >> 3;
  /* frame size continued first 3 bits */
  adts_header[5] = (out_size & 0x7) << 5;
  /* buffer fullness (0x7FF for VBR) over 5 last bits */
  adts_header[5] |= 0x1F;
  /* buffer fullness (0x7FF for VBR) continued over 6 first bits + 2 zeros for
   * number of raw data blocks */
  adts_header[6] = 0xFC;

  /* Prepend ADTS header */
  mem = gst_memory_new_wrapped (0, adts_header, 7, 0, 7, adts_header, g_free);
  gst_buffer_prepend_memory (out_buf, mem);

  return out_buf;
}
