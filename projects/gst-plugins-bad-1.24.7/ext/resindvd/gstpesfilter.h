/*
 * This library is licensed under 2 different licenses and you
 * can choose to use it under the terms of either one of them. The
 * two licenses are the MPL 1.1 and the LGPL.
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
 * The Original Code is Fluendo MPEG Demuxer plugin.
 *
 * The Initial Developer of the Original Code is Fluendo, S.L.
 * Portions created by Fluendo, S.L. are Copyright (C) 2005
 * Fluendo, S.L. All Rights Reserved.
 *
 * Contributor(s): Wim Taymans <wim@fluendo.com>
 */

#ifndef __GST_PES_FILTER_H__
#define __GST_PES_FILTER_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>

G_BEGIN_DECLS

typedef struct _GstPESFilter GstPESFilter;

typedef GstFlowReturn (*GstPESFilterData) (GstPESFilter * filter, gboolean first, GstBuffer * buffer, 
                         gpointer user_data);
typedef void (*GstPESFilterResync) (GstPESFilter * filter, gpointer user_data);
typedef void (*GstPESFilterIndex) (GstPESFilter * filter, gpointer user_data);

typedef enum {
  STATE_HEADER_PARSE,
  STATE_DATA_PUSH, 
  STATE_DATA_SKIP 
} GstPESFilterState;

struct _GstPESFilter {
  GstAdapter         * adapter;
  guint64            * adapter_offset;

  GstPESFilterState  state;
  /* Whether to collect entire PES packets before
   * outputting */
  gboolean           gather_pes;
  /* Whether unbounded packets are allowed in this
   * stream */
  gboolean           allow_unbounded;

  gboolean           first;
  GstPESFilterData   data_cb;
  GstPESFilterResync resync_cb;
  gpointer           user_data;

  guint32            start_code;
  guint8             id;
  gboolean           unbounded_packet;
  guint16            length;

  gint64             pts;
  gint64             dts;
};

void gst_pes_filter_init (GstPESFilter * filter, GstAdapter * adapter, guint64 * adapter_offset);

void gst_pes_filter_uninit (GstPESFilter * filter);

void gst_pes_filter_set_callbacks (GstPESFilter * filter, 
                         GstPESFilterData data_cb,
                         GstPESFilterResync resync_cb,
                         gpointer user_data);

GstFlowReturn gst_pes_filter_push (GstPESFilter * filter, GstBuffer * buffer);
GstFlowReturn gst_pes_filter_process (GstPESFilter * filter);

void gst_pes_filter_flush (GstPESFilter * filter);
GstFlowReturn gst_pes_filter_drain (GstPESFilter * filter);

G_END_DECLS

#endif /* __GST_PES_FILTER_H__ */
