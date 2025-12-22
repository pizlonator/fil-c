/* MPEG-PS muxer plugin for GStreamer
 * Copyright 2008 Lin YANG <oxcsnicho@gmail.com>
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
/*
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
 */



#ifndef __PSMUX_H__
#define __PSMUX_H__

#include <glib.h>

#include "psmuxcommon.h"
#include "psmuxstream.h"

G_BEGIN_DECLS

#define PSMUX_MAX_ES_INFO_LENGTH ((1 << 12) - 1)

typedef gboolean (*PsMuxWriteFunc) (guint8 *data, guint len, void *user_data);

struct PsMux {
  GList *streams;    /* PsMuxStream* array of all streams */
  guint nb_streams;
  guint nb_private_streams;
  PsMuxStreamIdInfo id_info; /* carrying the info which ids are used */

  /* timestamps: pts */ 
  GstClockTime pts;

  guint32 pes_cnt; /* # of pes that has been created */
  guint16 pes_max_payload; /* maximum payload size in pes packets */

  guint64 bit_size;  /* accumulated bit size of processed data */
  guint bit_rate;  /* bit rate */ 
  GstClockTime bit_pts; /* last time the bit_rate is updated */

  guint pack_hdr_freq; /* PS pack header frequency */
  GstClockTime pack_hdr_pts; /* last time a pack header is written */

  guint sys_hdr_freq; /* system header frequency */ 
  GstClockTime sys_hdr_pts; /* last time a system header is written */

  guint psm_freq; /* program stream map frequency */ 
  GstClockTime psm_pts; /* last time a psm is written */

  guint8 packet_buf[PSMUX_MAX_PACKET_LEN];
  guint packet_bytes_written; /* # of bytes written in the buf */
  PsMuxWriteFunc write_func;
  void *write_func_data;

  /* Scratch space for writing ES_info descriptors */
  guint8 es_info_buf[PSMUX_MAX_ES_INFO_LENGTH];

  /* bounds in system header */ 
  guint8 audio_bound;
  guint8 video_bound;
  guint32 rate_bound;

  /* stream headers */
  GstBuffer *sys_header;
  GstBuffer *psm;
};

/* create/free new muxer session */
PsMux *		psmux_new 			(void);
void 		psmux_free 			(PsMux *mux);

/* Setting muxing session properties */
void 		psmux_set_write_func 		(PsMux *mux, PsMuxWriteFunc func, void *user_data);

/* stream management */
PsMuxStream *	psmux_create_stream 		(PsMux *mux, PsMuxStreamType stream_type);

/* writing stuff */
gboolean 	psmux_write_stream_packet 	(PsMux *mux, PsMuxStream *stream); 
gboolean	psmux_write_end_code		(PsMux *mux);

GList *		psmux_get_stream_headers	(PsMux *mux);

G_END_DECLS

#endif
