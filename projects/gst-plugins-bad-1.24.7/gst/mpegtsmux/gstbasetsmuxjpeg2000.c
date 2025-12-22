/* GStreamer JPEG 2000 Parser
 *
 * Copyright (C) <2016> Milos Seleceni
 *  @author Milos Seleceni <milos.seleceni@comprimato.com>
 *
 * Copyright (C) <2016-2017> Grok Image Compression Inc.
 *  @author Aaron Boxer <boxerab@gmail.com>
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
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include "gstbasetsmuxjpeg2000.h"
#include <string.h>
#include <gst/audio/audio.h>
#include <gst/base/gstbytewriter.h>
#include <gst/gst.h>

#define GST_CAT_DEFAULT gst_base_ts_mux_debug

GstBuffer *
gst_base_ts_mux_prepare_jpeg2000 (GstBuffer * buf, GstBaseTsMuxPad * pad,
    GstBaseTsMux * mux)
{
  j2k_private_data *private_data = pad->prepare_data;
  GstByteWriter wr;
  GstBuffer *out_buf = NULL;
  guint8 *elsm_header = NULL;
  const guint header_size = private_data->interlace ? 48 : 38;
  GstClockTime seconds = buf->pts / GST_SECOND;
  GstClockTime minutes = seconds / 60;
  GstClockTime hours = minutes / 60;

  /* interlaced not supported */
  if (private_data->interlace) {
    GST_ERROR_OBJECT (mux, "Interlaced not supported");
    return NULL;
  }

  seconds = seconds % 60;
  minutes = minutes % 60;
  hours = hours % 24;

  /* ??? Hack for missing frame number index in buffer offset */
  /* guint8 frame_number = private_data->frame_number % 60; */
  gst_byte_writer_init_with_size (&wr, header_size, FALSE);

  /* Elementary stream header box 'elsm' == 0x656c736d */
  gst_byte_writer_put_uint32_be (&wr, 0x656c736d);
  /* Framerate box 'frat' == 0x66726174 */
  gst_byte_writer_put_uint32_be (&wr, 0x66726174);
  /* put framerate denominator */
  gst_byte_writer_put_uint16_be (&wr, private_data->den);
  /* put framerate numerator */
  gst_byte_writer_put_uint16_be (&wr, private_data->num);
  /* Maximum bitrate box 'brat' == 0x62726174 */
  gst_byte_writer_put_uint32_be (&wr, 0x62726174);
  /* put Maximum bitrate */
  gst_byte_writer_put_uint32_be (&wr, private_data->max_bitrate);
  /* put size of first codestream */
  /* private_data->AUF[0] */
  gst_byte_writer_put_uint32_be (&wr, gst_buffer_get_size (buf));

  /* ToDo: the if block below is never called, because we do not support muxing J2K-over-mpeg-TS interlaced data
   * If we ever do, then the code below will need to tested and perhaps modified
   */
  if (private_data->interlace) {
    /* put size of second codestream */
    gst_byte_writer_put_uint32_be (&wr, gst_buffer_get_size (buf));
    /* Time Code Box 'fiel' == 0x6669656c */
    gst_byte_writer_put_uint32_be (&wr, 0x6669656c);
    /* put Fic */
    gst_byte_writer_put_uint8 (&wr, private_data->Fic);
    /* put Fio */
    gst_byte_writer_put_uint8 (&wr, private_data->Fio);
  }

  /* Time Code Box 'tcod' == 0x74636f64 */
  gst_byte_writer_put_uint32_be (&wr, 0x74636f64);

  /* put HHMMSSFF */
  gst_byte_writer_put_uint8 (&wr, (guint8) hours);
  gst_byte_writer_put_uint8 (&wr, (guint8) minutes);
  gst_byte_writer_put_uint8 (&wr, (guint8) seconds);
  gst_byte_writer_put_uint8 (&wr, 0x0);
  /* ??? Hack for missing frame number index in buffer offset */
  /* private_data->frame_number++; */

  /* Broadcast Color Box 'bcol' == 0x62636f6c */
  gst_byte_writer_put_uint32_be (&wr, 0x62636f6c);
  /* put color spec */
  gst_byte_writer_put_uint8 (&wr, private_data->color_spec);
  /* put reserved 8-bit */
  gst_byte_writer_put_uint8 (&wr, 0xff);
  /* Allocate ELSM header size only; gst_buffer_copy_into will add gst_buffer_get_size (buf) bytes to out_buf */
  out_buf = gst_buffer_new_and_alloc (header_size);

  /* Copy ELSM header */
  elsm_header = gst_byte_writer_reset_and_get_data (&wr);
  gst_buffer_fill (out_buf, 0, elsm_header, header_size);
  g_free (elsm_header);
  /* Copy complete frame */
  gst_buffer_copy_into (out_buf, buf,
      GST_BUFFER_COPY_METADATA | GST_BUFFER_COPY_TIMESTAMPS |
      GST_BUFFER_COPY_MEMORY, 0, -1);
  GST_DEBUG_OBJECT (mux, "Prepared J2K PES of size %d",
      (int) gst_buffer_get_size (out_buf));

  return out_buf;
}

void
gst_base_ts_mux_free_jpeg2000 (gpointer prepare_data)
{
  /*  Free prepare data memory object */
  g_free (prepare_data);
}
