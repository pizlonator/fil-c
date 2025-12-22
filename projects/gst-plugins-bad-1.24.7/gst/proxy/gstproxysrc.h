/*
 * Copyright (C) 2015 Centricular Ltd.
 *   Author: Sebastian Dröge <sebastian@centricular.com>
 *   Author: Nirbheek Chauhan <nirbheek@centricular.com>
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

#ifndef __GST_PROXY_SRC_H__
#define __GST_PROXY_SRC_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_PROXY_SRC            (gst_proxy_src_get_type())
#define GST_PROXY_SRC(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_PROXY_SRC, GstProxySrc))
#define GST_IS_PROXY_SRC(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_PROXY_SRC))
#define GST_PROXY_SRC_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) , GST_TYPE_PROXY_SRC, GstProxySrcClass))
#define GST_IS_PROXY_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) , GST_TYPE_PROXY_SRC))
#define GST_PROXY_SRC_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) , GST_TYPE_PROXY_SRC, GstProxySrcClass))

typedef struct _GstProxySrc GstProxySrc;
typedef struct _GstProxySrcClass GstProxySrcClass;
typedef struct _GstProxySrcPrivate GstProxySrcPrivate;

struct _GstProxySrc {
  GstBin parent;

  /* < private > */

  /* Queue to hold buffers from proxysink */
  GstElement *queue;

  /* Source pad of the above queue and the proxysrc element itself */
  GstPad *srcpad;

  /* Our internal srcpad that proxysink pushes buffers/events/queries into */
  GstPad *internal_srcpad;

  /* An unlinked dummy sinkpad; see gst_proxy_src_init() */
  GstPad *dummy_sinkpad;

  /* The matching proxysink; queries and events are sent to its sinkpad */
  GWeakRef proxysink;
};

struct _GstProxySrcClass {
  GstBinClass parent_class;
};

GType gst_proxy_src_get_type(void);
GST_ELEMENT_REGISTER_DECLARE (proxysrc);

G_END_DECLS

#endif /* __GST_PROXY_SRC_H__ */
