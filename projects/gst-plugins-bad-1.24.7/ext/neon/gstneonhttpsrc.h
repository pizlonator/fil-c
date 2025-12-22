/* GStreamer
 * Copyright (C) <2005> Edgard Lima <edgard.lima@gmail.com>
 * Copyright (C) <2006> Rosfran Borges <rosfran.borges@indt.org.br>
 * Copyright (C) <2006> Andre Moreira Magalhaes <andre.magalhaes@indt.org.br>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more 
 */

#ifndef __GST_NEONHTTP_SRC_H__
#define __GST_NEONHTTP_SRC_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <stdio.h>

G_BEGIN_DECLS

#include <ne_session.h>
#include <ne_request.h>
#include <ne_socket.h>

#define GST_TYPE_NEONHTTP_SRC \
  (gst_neonhttp_src_get_type())
#define GST_NEONHTTP_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_NEONHTTP_SRC,GstNeonhttpSrc))
#define GST_NEONHTTP_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_NEONHTTP_SRC,GstNeonhttpSrcClass))
#define GST_IS_NEONHTTP_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_NEONHTTP_SRC))
#define GST_IS_NEONHTTP_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_NEONHTTP_SRC))

typedef struct _GstNeonhttpSrc GstNeonhttpSrc;
typedef struct _GstNeonhttpSrcClass GstNeonhttpSrcClass;

struct _GstNeonhttpSrc {
  GstPushSrc element;

  /* socket */
  ne_session *session;
  ne_request *request;
  ne_uri uri;
  gchar *location;
  gchar *query_string;
  ne_uri proxy;
  gchar *user_agent;
  gchar **cookies;

  guint64 content_size;

  gboolean eos;

  /* whether to request icecast metadata */
  gboolean iradio_mode;

  /* enable Neon HTTP redirects (HTTP 302 status code) */
  gboolean automatic_redirect;

  /* enable Neon HTTP debug messages */
  gboolean neon_http_debug;

  /* accept self-signed certificates */
  gboolean accept_self_signed;

  gint64 read_position;
  gboolean seekable;

  /* seconds before timing out when connecting or reading to/from a socket */
  guint connect_timeout;
  guint read_timeout;
};

struct _GstNeonhttpSrcClass {
  GstPushSrcClass parent_class;
};

GType gst_neonhttp_src_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (neonhttpsrc);

G_END_DECLS

#endif /* __GST_NEONHTTP_SRC_H__ */

