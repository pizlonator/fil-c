/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2004> Thomas Vander Stichele <thomas at apestaart dot org>
 * Copyright (C) <2014> William Manley <will@williammanley.net>
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


#ifndef __GST_SOCKET_SRC_H__
#define __GST_SOCKET_SRC_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>

#include <gio/gio.h>

G_BEGIN_DECLS

#define GST_TYPE_SOCKET_SRC \
  (gst_socket_src_get_type())
#define GST_SOCKET_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SOCKET_SRC,GstSocketSrc))
#define GST_SOCKET_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SOCKET_SRC,GstSocketSrcClass))
#define GST_IS_SOCKET_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SOCKET_SRC))
#define GST_IS_SOCKET_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SOCKET_SRC))

typedef struct _GstSocketSrc GstSocketSrc;
typedef struct _GstSocketSrcClass GstSocketSrcClass;

struct _GstSocketSrc {
  GstPushSrc element;

 /*< private >*/
  GstCaps *caps;
  GSocket *socket;
  gboolean send_messages;
  GCancellable *cancellable;
};

struct _GstSocketSrcClass {
  GstPushSrcClass parent_class;

  /* signals */
  void  (*connection_closed_by_peer) (GstElement*);
};

GType gst_socket_src_get_type (void);

G_END_DECLS

#endif /* __GST_SOCKET_SRC_H__ */
