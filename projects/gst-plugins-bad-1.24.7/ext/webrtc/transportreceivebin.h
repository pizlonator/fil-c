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

#ifndef __TRANSPORT_RECEIVE_BIN_H__
#define __TRANSPORT_RECEIVE_BIN_H__

#include <gst/gst.h>
#include "transportstream.h"

G_BEGIN_DECLS

GType transport_receive_bin_get_type(void);
#define GST_TYPE_WEBRTC_TRANSPORT_RECEIVE_BIN (transport_receive_bin_get_type())
#define TRANSPORT_RECEIVE_BIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_WEBRTC_TRANSPORT_RECEIVE_BIN,TransportReceiveBin))
#define TRANSPORT_RECEIVE_BIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_WEBRTC_TRANSPORT_RECEIVE_BIN,TransportReceiveBinClass))
#define TRANSPORT_RECEIVE_BIN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_WEBRTC_TRANSPORT_RECEIVE_BIN,TransportReceiveBinClass))

typedef enum
{
  RECEIVE_STATE_UNSET = 0,
  RECEIVE_STATE_BLOCK = 1,
  RECEIVE_STATE_PASS,
} ReceiveState;

struct _TransportReceiveBin
{
  GstBin                     parent;

  TransportStream           *stream;        /* parent transport stream */

  GstPad                    *rtp_src;
  gulong                     rtp_src_probe_id;
  GstPad                    *rtcp_src;
  gulong                     rtcp_src_probe_id;
  struct pad_block          *rtp_block;
  struct pad_block          *rtcp_block;
  GMutex                     pad_block_lock;
  ReceiveState               receive_state;
  GstElement                *queue;
};

struct _TransportReceiveBinClass
{
  GstBinClass           parent_class;
};

void        transport_receive_bin_set_receive_state         (TransportReceiveBin * receive,
                                                             ReceiveState state);

G_END_DECLS

#endif /* __TRANSPORT_RECEIVE_BIN_H__ */
