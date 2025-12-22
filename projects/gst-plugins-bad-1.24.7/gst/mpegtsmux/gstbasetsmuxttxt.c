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

#include "gstbasetsmuxttxt.h"
#include <string.h>

#define GST_CAT_DEFAULT gst_base_ts_mux_debug

/* from EN 300 472 spec: ITU-R System B Teletext in DVB
 *
 * PES packet = PES header + PES payload data
 *  where PES header must be fixed at 45 bytes (likely by using PES stuffing)
 * PES packet must completely fill an integral number of TS packets
 *  using (184 bytes) payload data only (so no adaptation field stuffing)
 */

GstBuffer *
gst_base_ts_mux_prepare_teletext (GstBuffer * buf, GstBaseTsMuxPad * pad,
    GstBaseTsMux * mux)
{
  GstBuffer *out_buf;
  guint8 *data, *odata;
  gint size, stuff;
  gboolean add_id = FALSE;
  GstMapInfo map, omap;

  gst_buffer_map (buf, &map, GST_MAP_READ);
  size = map.size;
  data = map.data;

  /* check if leading data_identifier byte is already present,
   * if not increase size since it will need to be added */
  if (data[0] < 0x10 || data[0] > 0x1F) {
    size += 1;
    add_id = TRUE;
  }

  if (size <= 184 - 45) {
    stuff = 184 - 45 - size;
  } else {
    stuff = size - (184 - 45);
    stuff = 184 - (stuff % 184);
  }
  if (G_UNLIKELY (stuff == 1))
    stuff += 184;

  GST_DEBUG_OBJECT (mux, "Preparing teletext buffer for output");

  out_buf = gst_buffer_new_and_alloc (size + stuff);
  gst_buffer_copy_into (out_buf, buf,
      GST_BUFFER_COPY_METADATA | GST_BUFFER_COPY_TIMESTAMPS, 0, 0);

  /* copy data */
  gst_buffer_map (out_buf, &omap, GST_MAP_WRITE);
  odata = omap.data;
  /* add data_identifier if needed */
  if (add_id) {
    *odata = 0x10;
    memcpy (odata + 1, data, size - 1);
  } else {
    memcpy (odata, data, size);
  }

  /* add stuffing data_unit */
  odata += size;
  *odata++ = 0xFF;
  *odata = stuff;

  gst_buffer_unmap (buf, &map);
  gst_buffer_unmap (out_buf, &omap);

  return out_buf;
}
