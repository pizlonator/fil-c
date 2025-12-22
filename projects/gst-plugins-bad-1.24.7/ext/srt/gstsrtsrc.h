/* GStreamer
 * Copyright (C) 2018, Collabora Ltd.
 * Copyright (C) 2018, SK Telecom, Co., Ltd.
 *   Author: Jeongseok Kim <jeongseok.kim@sk.com>
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

#ifndef __GST_SRT_SRC_H__
#define __GST_SRT_SRC_H__

#include <gio/gio.h>
#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>

#include "gstsrtobject.h"

G_BEGIN_DECLS

#define GST_TYPE_SRT_SRC              (gst_srt_src_get_type ())
#define GST_IS_SRT_SRC(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_SRT_SRC))
#define GST_IS_SRT_SRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_SRT_SRC))
#define GST_SRT_SRC_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_SRT_SRC, GstSRTSrcClass))
#define GST_SRT_SRC(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_SRT_SRC, GstSRTSrc))
#define GST_SRT_SRC_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_SRT_SRC, GstSRTSrcClass))
#define GST_SRT_SRC_CAST(obj)         ((GstSRTSrc*)(obj))
#define GST_SRT_SRC_CLASS_CAST(klass) ((GstSRTSrcClass*)(klass))

typedef struct _GstSRTSrc GstSRTSrc;
typedef struct _GstSRTSrcClass GstSRTSrcClass;

struct _GstSRTSrc {
  GstPushSrc parent;

  GstCaps      *caps;

  GstSRTObject *srtobject;

  guint32       next_pktseq;
  gboolean      keep_listening;
};

struct _GstSRTSrcClass {
  GstPushSrcClass parent_class;

  void (*caller_added)      (GstSRTSrc *self, int sock, GSocketAddress * addr);
  void (*caller_removed)    (GstSRTSrc *self, int sock, GSocketAddress * addr);
  void (*caller_rejected)   (GstSRTSrc *self, GSocketAddress * peer_address,
    const gchar * stream_id, gpointer data);
  gboolean (*caller_connecting) (GstSRTSrc *self, GSocketAddress * peer_address,
    const gchar * stream_id, gpointer data);
};

GType   gst_srt_src_get_type (void);

G_END_DECLS

#endif // __GST_SRT_SRC_H__
