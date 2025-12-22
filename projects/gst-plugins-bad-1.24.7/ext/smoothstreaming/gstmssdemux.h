/* GStreamer
 * Copyright (C) 2012 Smart TV Alliance
 *  Author: Thiago Sousa Santos <thiago.sousa.santos@collabora.com>, Collabora Ltd.
 *
 * gstmssdemux.h:
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GST_MSSDEMUX_H__
#define __GST_MSSDEMUX_H__

#include <gst/gst.h>
#include <gst/adaptivedemux/gstadaptivedemux.h>
#include <gst/base/gstadapter.h>
#include <gst/base/gstdataqueue.h>
#include <gst/gstprotection.h>
#include "gstmssmanifest.h"

G_BEGIN_DECLS

GST_DEBUG_CATEGORY_EXTERN (mssdemux_debug);
#define GST_CAT_DEFAULT mssdemux_debug

#define GST_TYPE_MSS_DEMUX \
  (gst_mss_demux_get_type())
#define GST_MSS_DEMUX(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MSS_DEMUX,GstMssDemux))
#define GST_MSS_DEMUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MSS_DEMUX,GstMssDemuxClass))
#define GST_IS_MSS_DEMUX(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MSS_DEMUX))
#define GST_IS_MSS_DEMUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MSS_DEMUX))

#define GST_MSS_DEMUX_CAST(obj) ((GstMssDemux *)(obj))

typedef struct _GstMssDemuxStream GstMssDemuxStream;
typedef struct _GstMssDemux GstMssDemux;
typedef struct _GstMssDemuxClass GstMssDemuxClass;

struct _GstMssDemuxStream {
  GstAdaptiveDemuxStream parent;

  GstMssStream *manifest_stream;
};

struct _GstMssDemux {
  GstAdaptiveDemux bin;

  /* pads */
  GstPad *sinkpad;

  GstMssManifest *manifest;
  gchar *base_url;

  guint n_videos;
  guint n_audios;

  /* properties */
  guint data_queue_max_size;
};

struct _GstMssDemuxClass {
  GstAdaptiveDemuxClass parent_class;
};

GType gst_mss_demux_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (mssdemux);
G_END_DECLS

#endif /* __GST_MSSDEMUX_H__ */
