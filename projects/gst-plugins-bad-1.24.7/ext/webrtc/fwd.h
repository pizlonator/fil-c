/* GStreamer
 * Copyright (C) 2017 Matthew Waters <matthew@centricular.com>
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

#ifndef __WEBRTC_FWD_H__
#define __WEBRTC_FWD_H__

#include <gst/gst.h>
#include <gst/webrtc/webrtc.h>

G_BEGIN_DECLS

typedef struct _GstWebRTCBin GstWebRTCBin;
typedef struct _GstWebRTCBinClass GstWebRTCBinClass;
typedef struct _GstWebRTCBinPrivate GstWebRTCBinPrivate;

typedef struct _GstWebRTCSCTPTransport GstWebRTCSCTPTransport;
typedef struct _GstWebRTCSCTPTransportClass GstWebRTCSCTPTransportClass;
typedef struct _GstWebRTCSCTPTransportPrivate GstWebRTCSCTPTransportPrivate;

typedef struct _TransportStream TransportStream;
typedef struct _TransportStreamClass TransportStreamClass;

typedef struct _TransportSendBin TransportSendBin;
typedef struct _TransportSendBinClass TransportSendBinClass;

typedef struct _TransportReceiveBin TransportReceiveBin;
typedef struct _TransportReceiveBinClass TransportReceiveBinClass;

typedef struct _WebRTCTransceiver WebRTCTransceiver;
typedef struct _WebRTCTransceiverClass WebRTCTransceiverClass;

G_END_DECLS

#endif /* __WEBRTC_FWD_H__ */
