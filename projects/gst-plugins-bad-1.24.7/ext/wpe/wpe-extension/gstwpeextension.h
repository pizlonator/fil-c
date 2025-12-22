/* Copyright (C) <2021> Thibault Saunier <tsaunier@igalia.com>
 *
 * This library is free software; you can redistribute it and/or modify it under the terms of the
 * GNU Library General Public License as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License along with this
 * library; if not, write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if USE_WPE2
#include <wpe/webkit-web-process-extension.h>
#else
#include <wpe/webkit-web-extension.h>
#endif
#include <gio/gunixfdlist.h>
#include <gst/gst.h>
#include <gst/base/gstbasesink.h>

void gst_wpe_extension_send_message (WebKitUserMessage *msg, GCancellable *cancellable, GAsyncReadyCallback cb, gpointer udata);

G_DECLARE_FINAL_TYPE (GstWpeAudioSink, gst_wpe_audio_sink, GST, WPE_AUDIO_SINK, GstBaseSink);
G_DECLARE_FINAL_TYPE (GstWpeBusMsgForwarder, gst_wpe_bus_msg_forwarder, GST, WPE_BUS_MSG_FORWARDER, GstTracer);