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

#include "gstbasetsmuxopus.h"
#include <string.h>
#include <gst/audio/audio.h>

#define GST_CAT_DEFAULT gst_base_ts_mux_debug

GstBuffer *
gst_base_ts_mux_prepare_opus (GstBuffer * buf, GstBaseTsMuxPad * pad,
    GstBaseTsMux * mux)
{
  gssize insize = gst_buffer_get_size (buf);
  gsize outsize;
  GstBuffer *outbuf;
  GstMapInfo map;
  guint n;
  GstAudioClippingMeta *cmeta = gst_buffer_get_audio_clipping_meta (buf);

  g_assert (!cmeta || cmeta->format == GST_FORMAT_DEFAULT);

  outsize = 2 + insize / 255 + 1;
  if (cmeta && cmeta->start)
    outsize += 2;
  if (cmeta && cmeta->end)
    outsize += 2;

  outbuf = gst_buffer_new_and_alloc (outsize);
  gst_buffer_copy_into (outbuf, buf,
      GST_BUFFER_COPY_METADATA | GST_BUFFER_COPY_TIMESTAMPS, 0, 0);
  gst_buffer_map (outbuf, &map, GST_MAP_WRITE);
  map.data[0] = 0x7f;
  map.data[1] = 0xe0;

  if (cmeta && cmeta->start)
    map.data[1] |= 0x10;
  if (cmeta && cmeta->end)
    map.data[1] |= 0x08;

  n = 2;
  do {
    g_assert (n < outsize);
    /* FIXME: this should be using insize for writing here but ffmpeg and the
     * only available sample stream from obe.tv are not including the control
     * header size in au_size
     */
    map.data[n] = MIN (insize, 255);
    insize -= 255;
    n++;
  } while (insize >= 0);

  if (cmeta && cmeta->start) {
    GST_WRITE_UINT16_BE (&map.data[n], cmeta->start);
    n += 2;
  }

  if (cmeta && cmeta->end)
    GST_WRITE_UINT16_BE (&map.data[n], cmeta->end);

  gst_buffer_unmap (outbuf, &map);

  outbuf = gst_buffer_append (outbuf, gst_buffer_ref (buf));

  return outbuf;
}
