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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>

#include "webrtcsctptransport.h"
#include "gstwebrtcbin.h"

#define GST_CAT_DEFAULT webrtc_sctp_transport_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

enum
{
  SIGNAL_0,
  ON_STREAM_RESET_SIGNAL,
  LAST_SIGNAL,
};

enum
{
  PROP_0,
  PROP_TRANSPORT,
  PROP_STATE,
  PROP_MAX_MESSAGE_SIZE,
  PROP_MAX_CHANNELS,
};

static guint webrtc_sctp_transport_signals[LAST_SIGNAL] = { 0 };

#define webrtc_sctp_transport_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (WebRTCSCTPTransport, webrtc_sctp_transport,
    GST_TYPE_WEBRTC_SCTP_TRANSPORT,
    GST_DEBUG_CATEGORY_INIT (webrtc_sctp_transport_debug,
        "webrtcsctptransport", 0, "webrtcsctptransport"););

typedef void (*SCTPTask) (WebRTCSCTPTransport * sctp, gpointer user_data);

struct task
{
  WebRTCSCTPTransport *sctp;
  SCTPTask func;
  gpointer user_data;
  GDestroyNotify notify;
};

static GstStructure *
_execute_task (GstWebRTCBin * webrtc, struct task *task)
{
  if (task->func)
    task->func (task->sctp, task->user_data);
  return NULL;
}

static void
_free_task (struct task *task)
{
  gst_object_unref (task->sctp);

  if (task->notify)
    task->notify (task->user_data);
  g_free (task);
}

static void
_sctp_enqueue_task (WebRTCSCTPTransport * sctp, SCTPTask func,
    gpointer user_data, GDestroyNotify notify)
{
  struct task *task = g_new0 (struct task, 1);

  task->sctp = gst_object_ref (sctp);
  task->func = func;
  task->user_data = user_data;
  task->notify = notify;

  gst_webrtc_bin_enqueue_task (sctp->webrtcbin,
      (GstWebRTCBinFunc) _execute_task, task, (GDestroyNotify) _free_task,
      NULL);
}

static void
_emit_stream_reset (WebRTCSCTPTransport * sctp, gpointer user_data)
{
  guint stream_id = GPOINTER_TO_UINT (user_data);

  g_signal_emit (sctp,
      webrtc_sctp_transport_signals[ON_STREAM_RESET_SIGNAL], 0, stream_id);
}

static void
_on_sctp_dec_pad_removed (GstElement * sctpdec, GstPad * pad,
    WebRTCSCTPTransport * sctp)
{
  guint stream_id;

  if (sscanf (GST_PAD_NAME (pad), "src_%u", &stream_id) != 1)
    return;

  _sctp_enqueue_task (sctp, (SCTPTask) _emit_stream_reset,
      GUINT_TO_POINTER (stream_id), NULL);
}

static void
_on_sctp_association_established (GstElement * sctpenc, gboolean established,
    WebRTCSCTPTransport * sctp)
{
  GST_OBJECT_LOCK (sctp);
  if (established)
    sctp->state = GST_WEBRTC_SCTP_TRANSPORT_STATE_CONNECTED;
  else
    sctp->state = GST_WEBRTC_SCTP_TRANSPORT_STATE_CLOSED;
  sctp->association_established = established;
  GST_OBJECT_UNLOCK (sctp);

  g_object_notify (G_OBJECT (sctp), "state");
}

void
webrtc_sctp_transport_set_priority (WebRTCSCTPTransport * sctp,
    GstWebRTCPriorityType priority)
{
  GstPad *pad;

  pad = gst_element_get_static_pad (sctp->sctpenc, "src");
  gst_pad_push_event (pad,
      gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM_STICKY,
          gst_structure_new ("GstWebRtcBinUpdateTos", "sctp-priority",
              GST_TYPE_WEBRTC_PRIORITY_TYPE, priority, NULL)));
  gst_object_unref (pad);
}

static void
webrtc_sctp_transport_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  WebRTCSCTPTransport *sctp = WEBRTC_SCTP_TRANSPORT (object);

  switch (prop_id) {
    case PROP_TRANSPORT:
      g_value_set_object (value, sctp->transport);
      break;
    case PROP_STATE:
      g_value_set_enum (value, sctp->state);
      break;
    case PROP_MAX_MESSAGE_SIZE:
      g_value_set_uint64 (value, sctp->max_message_size);
      break;
    case PROP_MAX_CHANNELS:
      g_value_set_uint (value, sctp->max_channels);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
webrtc_sctp_transport_finalize (GObject * object)
{
  WebRTCSCTPTransport *sctp = WEBRTC_SCTP_TRANSPORT (object);

  g_signal_handlers_disconnect_by_data (sctp->sctpdec, sctp);
  g_signal_handlers_disconnect_by_data (sctp->sctpenc, sctp);

  gst_object_unref (sctp->sctpdec);
  gst_object_unref (sctp->sctpenc);

  g_clear_object (&sctp->transport);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
webrtc_sctp_transport_constructed (GObject * object)
{
  WebRTCSCTPTransport *sctp = WEBRTC_SCTP_TRANSPORT (object);
  guint association_id;

  association_id = g_random_int_range (0, G_MAXUINT16);

  sctp->sctpdec =
      g_object_ref_sink (gst_element_factory_make ("sctpdec", NULL));
  g_object_set (sctp->sctpdec, "sctp-association-id", association_id, NULL);
  sctp->sctpenc =
      g_object_ref_sink (gst_element_factory_make ("sctpenc", NULL));
  g_object_set (sctp->sctpenc, "sctp-association-id", association_id, NULL);
  g_object_set (sctp->sctpenc, "use-sock-stream", TRUE, NULL);

  g_signal_connect (sctp->sctpdec, "pad-removed",
      G_CALLBACK (_on_sctp_dec_pad_removed), sctp);
  g_signal_connect (sctp->sctpenc, "sctp-association-established",
      G_CALLBACK (_on_sctp_association_established), sctp);

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
webrtc_sctp_transport_class_init (WebRTCSCTPTransportClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->constructed = webrtc_sctp_transport_constructed;
  gobject_class->get_property = webrtc_sctp_transport_get_property;
  gobject_class->finalize = webrtc_sctp_transport_finalize;

  g_object_class_override_property (gobject_class, PROP_TRANSPORT, "transport");
  g_object_class_override_property (gobject_class, PROP_STATE, "state");
  g_object_class_override_property (gobject_class,
      PROP_MAX_MESSAGE_SIZE, "max-message-size");
  g_object_class_override_property (gobject_class,
      PROP_MAX_CHANNELS, "max-channels");

  /**
   * WebRTCSCTPTransport::stream-reset:
   * @object: the #WebRTCSCTPTransport
   * @stream_id: the SCTP stream that was reset
   */
  webrtc_sctp_transport_signals[ON_STREAM_RESET_SIGNAL] =
      g_signal_new ("stream-reset", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_UINT);
}

static void
webrtc_sctp_transport_init (WebRTCSCTPTransport * nice)
{
}

WebRTCSCTPTransport *
webrtc_sctp_transport_new (void)
{
  return g_object_new (TYPE_WEBRTC_SCTP_TRANSPORT, NULL);
}
