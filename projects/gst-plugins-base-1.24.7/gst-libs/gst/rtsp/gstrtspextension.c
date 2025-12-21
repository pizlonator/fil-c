/* GStreamer RTSP extension
 * Copyright (C) 2007 Wim Taymans <wim.taymans@gmail.com>
 *
 * gstrtspextension.c: RTSP extension mechanism
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

/**
 * SECTION:gstrtspextension
 * @title: GstRTSPExtension
 * @short_description: Interface for extending RTSP protocols
 *
 *  This interface is implemented e.g. by the Windows Media Streaming RTSP
 *  exentension (rtspwms) and the RealMedia RTSP extension (rtspreal).
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstrtsp-enumtypes.h"
#include "gstrtspextension.h"

static void gst_rtsp_extension_iface_init (GstRTSPExtension * iface);

enum
{
  SIGNAL_SEND,
  LAST_SIGNAL
};

static guint gst_rtsp_extension_signals[LAST_SIGNAL] = { 0 };

GType
gst_rtsp_extension_get_type (void)
{
  static gsize gst_rtsp_extension_type = 0;
  static const GTypeInfo gst_rtsp_extension_info = {
    sizeof (GstRTSPExtensionInterface),
    (GBaseInitFunc) gst_rtsp_extension_iface_init,
    NULL,
    NULL,
    NULL,
    NULL,
    0,
    0,
    NULL,
  };

  if (g_once_init_enter (&gst_rtsp_extension_type)) {
    GType tmp = g_type_register_static (G_TYPE_INTERFACE,
        "GstRTSPExtension", &gst_rtsp_extension_info, 0);
    g_once_init_leave (&gst_rtsp_extension_type, tmp);
  }
  return (GType) gst_rtsp_extension_type;
}

static void
gst_rtsp_extension_iface_init (GstRTSPExtension * iface)
{
  static gboolean initialized = FALSE;

  if (!initialized) {
    gst_rtsp_extension_signals[SIGNAL_SEND] =
        g_signal_new ("send", G_TYPE_FROM_CLASS (iface),
        G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstRTSPExtensionInterface,
            send), NULL, NULL, NULL, GST_TYPE_RTSP_RESULT, 2, G_TYPE_POINTER,
        G_TYPE_POINTER);
    initialized = TRUE;
  }
}

gboolean
gst_rtsp_extension_detect_server (GstRTSPExtension * ext, GstRTSPMessage * resp)
{
  GstRTSPExtensionInterface *iface;
  gboolean res = TRUE;

  iface = GST_RTSP_EXTENSION_GET_INTERFACE (ext);
  if (iface->detect_server)
    res = iface->detect_server (ext, resp);

  return res;
}

GstRTSPResult
gst_rtsp_extension_before_send (GstRTSPExtension * ext, GstRTSPMessage * req)
{
  GstRTSPExtensionInterface *iface;
  GstRTSPResult res = GST_RTSP_OK;

  iface = GST_RTSP_EXTENSION_GET_INTERFACE (ext);
  if (iface->before_send)
    res = iface->before_send (ext, req);

  return res;
}

GstRTSPResult
gst_rtsp_extension_after_send (GstRTSPExtension * ext, GstRTSPMessage * req,
    GstRTSPMessage * resp)
{
  GstRTSPExtensionInterface *iface;
  GstRTSPResult res = GST_RTSP_OK;

  iface = GST_RTSP_EXTENSION_GET_INTERFACE (ext);
  if (iface->after_send)
    res = iface->after_send (ext, req, resp);

  return res;
}

GstRTSPResult
gst_rtsp_extension_parse_sdp (GstRTSPExtension * ext, GstSDPMessage * sdp,
    GstStructure * s)
{
  GstRTSPExtensionInterface *iface;
  GstRTSPResult res = GST_RTSP_OK;

  iface = GST_RTSP_EXTENSION_GET_INTERFACE (ext);
  if (iface->parse_sdp)
    res = iface->parse_sdp (ext, sdp, s);

  return res;
}

GstRTSPResult
gst_rtsp_extension_setup_media (GstRTSPExtension * ext, GstSDPMedia * media)
{
  GstRTSPExtensionInterface *iface;
  GstRTSPResult res = GST_RTSP_OK;

  iface = GST_RTSP_EXTENSION_GET_INTERFACE (ext);
  if (iface->setup_media)
    res = iface->setup_media (ext, media);

  return res;
}

gboolean
gst_rtsp_extension_configure_stream (GstRTSPExtension * ext, GstCaps * caps)
{
  GstRTSPExtensionInterface *iface;
  gboolean res = TRUE;

  iface = GST_RTSP_EXTENSION_GET_INTERFACE (ext);
  if (iface->configure_stream)
    res = iface->configure_stream (ext, caps);

  return res;
}

GstRTSPResult
gst_rtsp_extension_get_transports (GstRTSPExtension * ext,
    GstRTSPLowerTrans protocols, gchar ** transport)
{
  GstRTSPExtensionInterface *iface;
  GstRTSPResult res = GST_RTSP_OK;

  iface = GST_RTSP_EXTENSION_GET_INTERFACE (ext);
  if (iface->get_transports)
    res = iface->get_transports (ext, protocols, transport);

  return res;
}

GstRTSPResult
gst_rtsp_extension_stream_select (GstRTSPExtension * ext, GstRTSPUrl * url)
{
  GstRTSPExtensionInterface *iface;
  GstRTSPResult res = GST_RTSP_OK;

  iface = GST_RTSP_EXTENSION_GET_INTERFACE (ext);
  if (iface->stream_select)
    res = iface->stream_select (ext, url);

  return res;
}

GstRTSPResult
gst_rtsp_extension_receive_request (GstRTSPExtension * ext,
    GstRTSPMessage * msg)
{
  GstRTSPExtensionInterface *iface;
  GstRTSPResult res = GST_RTSP_ENOTIMPL;

  iface = GST_RTSP_EXTENSION_GET_INTERFACE (ext);
  if (iface->receive_request)
    res = iface->receive_request (ext, msg);

  return res;
}

GstRTSPResult
gst_rtsp_extension_send (GstRTSPExtension * ext, GstRTSPMessage * req,
    GstRTSPMessage * resp)
{
  GstRTSPResult res = GST_RTSP_OK;

  g_signal_emit (ext, gst_rtsp_extension_signals[SIGNAL_SEND], 0,
      req, resp, &res);

  return res;
}
