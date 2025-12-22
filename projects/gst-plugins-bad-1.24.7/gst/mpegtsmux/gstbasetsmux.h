/*
 * Copyright 2006, 2007, 2008, 2009, 2010 Fluendo S.A.
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

#ifndef __BASETSMUX_H__
#define __BASETSMUX_H__

#include <gst/gst.h>
#include <gst/base/gstcollectpads.h>
#include <gst/base/gstadapter.h>
#include <gst/base/gstaggregator.h>

G_BEGIN_DECLS

#include "tsmux/tsmux.h"

#define GST_TYPE_BASE_TS_MUX_PAD (gst_base_ts_mux_pad_get_type())
#define GST_BASE_TS_MUX_PAD(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_BASE_TS_MUX_PAD, GstBaseTsMuxPad))
#define GST_BASE_TS_MUX_PAD_CAST(obj) ((GstBaseTsMuxPad *)(obj))
#define GST_BASE_TS_MUX_PAD_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_BASE_TS_MUX_PAD, GstBaseTsMuxPadClass))
#define GST_IS_BASE_TS_MUX_PAD(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_BASE_TS_MUX_PAD))
#define GST_IS_BASE_TS_MUX_PAD_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_BASE_TS_MUX_PAD))
#define GST_BASE_TS_MUX_PAD_GET_CLASS(obj) \
        (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_BASE_TS_MUX_PAD,GstBaseTsMuxPadClass))

typedef struct _GstBaseTsMuxPad GstBaseTsMuxPad;
typedef struct _GstBaseTsMuxPadClass GstBaseTsMuxPadClass;
typedef struct _GstBaseTsMuxPadPrivate GstBaseTsMuxPadPrivate;
typedef struct GstBaseTsMux GstBaseTsMux;
typedef struct GstBaseTsMuxClass GstBaseTsMuxClass;
typedef struct GstBaseTsPadData GstBaseTsPadData;

typedef GstBuffer * (*GstBaseTsMuxPadPrepareFunction) (GstBuffer * buf,
    GstBaseTsMuxPad * data, GstBaseTsMux * mux);

typedef void (*GstBaseTsMuxPadFreePrepareDataFunction) (gpointer prepare_data);

struct _GstBaseTsMuxPad
{
  GstAggregatorPad              parent;

  gint pid;
  TsMuxStream *stream;

  /* most recent DTS */
  gint64 dts;

  /* optional codec data available in the caps */
  GstBuffer *codec_data;

  /* Opaque data pointer to a structure used by the prepare function */
  gpointer prepare_data;

  /* handler to prepare input data */
  GstBaseTsMuxPadPrepareFunction prepare_func;
  /* handler to free the private data */
  GstBaseTsMuxPadFreePrepareDataFunction free_func;

  /* program id to which it is attached to (not program pid) */
  gint prog_id;
  /* program this stream belongs to */
  TsMuxProgram *prog;

  gchar *language;
  gint bitrate;
  gint max_bitrate;
  gint stream_number;
};

struct _GstBaseTsMuxPadClass
{
  GstAggregatorPadClass parent_class;
};

GType gst_base_ts_mux_pad_get_type   (void);

#define GST_TYPE_BASE_TS_MUX  (gst_base_ts_mux_get_type())
#define GST_BASE_TS_MUX(obj)  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_BASE_TS_MUX, GstBaseTsMux))
#define GST_BASE_TS_MUX_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_BASE_TS_MUX, GstBaseTsMuxClass))
#define GST_BASE_TS_MUX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),GST_TYPE_BASE_TS_MUX,GstBaseTsMuxClass))

#define GST_BASE_TS_MUX_NORMAL_PACKET_LENGTH 188

struct GstBaseTsMux {
  GstAggregator parent;

  TsMux *tsmux;
  GHashTable *programs;

  /* properties */
  GstStructure *prog_map;
  guint pat_interval;
  guint pmt_interval;
  gint alignment;
  guint si_interval;
  guint64 bitrate;
  guint pcr_interval;
  guint scte35_pid;
  guint scte35_null_interval;
  guint32 last_scte35_event_seqnum;

  /* state */
  gboolean first;
  GstClockTime pending_key_unit_ts;
  GstEvent *force_key_unit_event;
  GstMpegtsSection *pending_scte35_section;

  /* write callback handling/state */
  GstFlowReturn last_flow_ret;
  GQueue streamheader;
  gboolean streamheader_sent;
  gboolean is_delta;
  gboolean is_header;
  GstClockTime last_ts;

  gsize packet_size;
  gsize automatic_alignment;

  /* output buffer aggregation */
  GstAdapter *out_adapter;
  GstBuffer *out_buffer;
  GstClockTimeDiff output_ts_offset;

  /* protects the tsmux object, the programs hash table, and pad streams */
  GMutex lock;
};

/**
 * GstBaseTsMuxClass:
 * @create_ts_mux: Optional.
 *                 Called in order to create the #TsMux object.
 * @handle_media_type: Optional.
 *                 Called in order to determine the stream-type for a given
 *                 @media_type (eg. video/x-h264).
 * @allocate_packet: Optional.
 *                 Called when the underlying #TsMux object needs a packet
 *                 to write into.
 * @output_packet: Optional.
 *                 Called when the underlying #TsMux object has a packet
 *                 ready to output.
 * @reset:         Optional.
 *                 Called when the subclass needs to reset.
 * @drain:         Optional.
 *                 Called at EOS, if the subclass has data it needs to drain.
 */
struct GstBaseTsMuxClass {
  GstAggregatorClass parent_class;

  TsMux *   (*create_ts_mux) (GstBaseTsMux *mux);
  guint     (*handle_media_type) (GstBaseTsMux *mux, const gchar *media_type, GstBaseTsMuxPad * pad);
  void      (*allocate_packet) (GstBaseTsMux *mux, GstBuffer **buffer);
  gboolean  (*output_packet) (GstBaseTsMux *mux, GstBuffer *buffer, gint64 new_pcr);
  void      (*reset) (GstBaseTsMux *mux);
  gboolean  (*drain) (GstBaseTsMux *mux);
};

void gst_base_ts_mux_set_packet_size (GstBaseTsMux *mux, gsize size);
void gst_base_ts_mux_set_automatic_alignment (GstBaseTsMux *mux, gsize alignment);

typedef GstBuffer * (*GstBaseTsPadDataPrepareFunction) (GstBuffer * buf,
    GstBaseTsPadData * data, GstBaseTsMux * mux);

typedef void (*GstBaseTsPadDataFreePrepareDataFunction) (gpointer prepare_data);

struct GstBaseTsPadData {
  /* parent */
  GstCollectData collect;

  gint pid;
  TsMuxStream *stream;

  /* most recent DTS */
  gint64 dts;

  /* optional codec data available in the caps */
  GstBuffer *codec_data;

  /* Opaque data pointer to a structure used by the prepare function */
  gpointer prepare_data;

  /* handler to prepare input data */
  GstBaseTsPadDataPrepareFunction prepare_func;
  /* handler to free the private data */
  GstBaseTsPadDataFreePrepareDataFunction free_func;

  /* program id to which it is attached to (not program pid) */
  gint prog_id;
  /* program this stream belongs to */
  TsMuxProgram *prog;

  gchar *language;
};

GType gst_base_ts_mux_get_type (void);

G_END_DECLS

#endif
