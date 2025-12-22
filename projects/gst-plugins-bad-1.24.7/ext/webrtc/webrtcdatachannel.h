/* GStreamer
 * Copyright (C) 2018 Matthew Waters <matthew@centricular.com>
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

#ifndef __WEBRTC_DATA_CHANNEL_H__
#define __WEBRTC_DATA_CHANNEL_H__

#include <gst/gst.h>
#include <gst/webrtc/webrtc_fwd.h>
#include <gst/webrtc/dtlstransport.h>
#include <gst/webrtc/datachannel.h>
#include "webrtcsctptransport.h"

#include "gst/webrtc/webrtc-priv.h"

G_BEGIN_DECLS

GType webrtc_data_channel_get_type(void);
#define WEBRTC_TYPE_DATA_CHANNEL            (webrtc_data_channel_get_type())
#define WEBRTC_DATA_CHANNEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),WEBRTC_TYPE_DATA_CHANNEL,WebRTCDataChannel))
#define WEBRTC_IS_DATA_CHANNEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),WEBRTC_TYPE_DATA_CHANNEL))
#define WEBRTC_DATA_CHANNEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,WEBRTC_TYPE_DATA_CHANNEL,WebRTCDataChannelClass))
#define WEBRTC_IS_DATA_CHANNEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,WEBRTC_TYPE_DATA_CHANNEL))
#define WEBRTC_DATA_CHANNEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,WEBRTC_TYPE_DATA_CHANNEL,WebRTCDataChannelClass))

typedef struct _WebRTCDataChannel WebRTCDataChannel;
typedef struct _WebRTCDataChannelClass WebRTCDataChannelClass;

struct _WebRTCDataChannel
{
  GstWebRTCDataChannel              parent;

  WebRTCSCTPTransport              *sctp_transport;
  GstElement                       *src_bin;
  GstElement                       *appsrc;
  GstElement                       *sink_bin;
  GstElement                       *appsink;

  GWeakRef                          webrtcbin_weak;
  gboolean                          opened;
  gulong                            src_probe;
  GError                           *stored_error;
  gboolean                          peer_closed;

  gpointer                          _padding[GST_PADDING];
};

struct _WebRTCDataChannelClass
{
  GstWebRTCDataChannelClass  parent_class;

  gpointer                  _padding[GST_PADDING];
};

void    webrtc_data_channel_start_negotiation   (WebRTCDataChannel       *channel);
G_GNUC_INTERNAL
void    webrtc_data_channel_link_to_sctp (WebRTCDataChannel                 *channel,
                                          WebRTCSCTPTransport               *sctp_transport);

G_GNUC_INTERNAL
void    webrtc_data_channel_set_webrtcbin (WebRTCDataChannel                *channel,
                                           GstWebRTCBin                     *webrtcbin);

G_DECLARE_FINAL_TYPE (WebRTCErrorIgnoreBin, webrtc_error_ignore_bin, WEBRTC, ERROR_IGNORE_BIN, GstBin);

G_END_DECLS

#endif /* __WEBRTC_DATA_CHANNEL_H__ */
