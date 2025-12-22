/* GStreamer
 * Copyright (C) 2017 Make.TV, Inc. <info@make.tv>
 *   Contact: Jan Alexander Steffens (heftig) <jsteffens@make.tv>
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

#ifndef __GST_RTMP_LOCATION_HANDLER_H__
#define __GST_RTMP_LOCATION_HANDLER_H__

#include <gst/gst.h>
#include "rtmp/rtmpclient.h"

G_BEGIN_DECLS
#define GST_TYPE_RTMP_LOCATION_HANDLER                 (gst_rtmp_location_handler_get_type ())
#define GST_RTMP_LOCATION_HANDLER(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_RTMP_LOCATION_HANDLER, GstRtmpLocationHandler))
#define GST_IS_RTMP_LOCATION_HANDLER(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_RTMP_LOCATION_HANDLER))
#define GST_RTMP_LOCATION_HANDLER_GET_INTERFACE(inst)  (G_TYPE_INSTANCE_GET_INTERFACE ((inst), GST_TYPE_RTMP_LOCATION_HANDLER, GstRtmpLocationHandlerInterface))
typedef struct _GstRtmpLocationHandler GstRtmpLocationHandler;  /* dummy object */
typedef struct _GstRtmpLocationHandlerInterface GstRtmpLocationHandlerInterface;

struct _GstRtmpLocationHandlerInterface
{
  GTypeInterface parent_iface;
};

GType gst_rtmp_location_handler_get_type (void);

void gst_rtmp_location_handler_implement_uri_handler (GstURIHandlerInterface *
    iface, GstURIType type);

gboolean gst_rtmp_location_handler_set_uri (GstRtmpLocationHandler * handler,
    const gchar * uri);

G_END_DECLS
#endif
