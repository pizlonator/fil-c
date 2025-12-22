/*
 * GStreamer - DTMF Detection
 *
 *  Copyright 2009 Nokia Corporation
 *  Copyright 2009 Collabora Ltd,
 *   @author: Olivier Crete <olivier.crete@collabora.co.uk>
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
 *
 */

/**
 * SECTION:element-dtmfdetect
 * @title: dtmfdetect
 * @short_description: Detects DTMF tones
 *
 * This element will detect DTMF tones and emit messages.
 *
 * The message is called `dtmf-event` and has the following fields:
 *
 * * gint `type` (0-1): The application uses this field to specify which of the two methods
 *   specified in RFC 2833 to use. The value should be 0 for tones and 1 for
 *   named events. Tones are specified by their frequencies and events are
 *   specified by their number. This element can only take events as input.
 *   Do not confuse with "method" which specified the output.
 * * gint `number` (0-16): The event number.
 * * gint `method` (2): This field will always been 2 (ie sound) from this element.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstdtmfdetect.h"

#include <string.h>

#include <gst/audio/audio.h>

GST_DEBUG_CATEGORY (dtmf_detect_debug);
#define GST_CAT_DEFAULT (dtmf_detect_debug)

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) \"" GST_AUDIO_NE (S16) "\", "
        "rate = (int) 8000, " "channels = (int) 1")
    );


static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) \"" GST_AUDIO_NE (S16) "\", "
        "rate = (int) 8000, " "channels = (int) 1")
    );

/* signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
};

static void gst_dtmf_detect_finalize (GObject * object);

static gboolean gst_dtmf_detect_set_caps (GstBaseTransform * trans,
    GstCaps * incaps, GstCaps * outcaps);
static GstFlowReturn gst_dtmf_detect_transform_ip (GstBaseTransform * trans,
    GstBuffer * buf);
static gboolean gst_dtmf_detect_sink_event (GstBaseTransform * trans,
    GstEvent * event);

G_DEFINE_TYPE (GstDtmfDetect, gst_dtmf_detect, GST_TYPE_BASE_TRANSFORM);
GST_ELEMENT_REGISTER_DEFINE (dtmfdetect, "dtmfdetect",
    GST_RANK_MARGINAL, GST_TYPE_DTMF_DETECT);

static void
gst_dtmf_detect_class_init (GstDtmfDetectClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseTransformClass *gstbasetransform_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  gstbasetransform_class = (GstBaseTransformClass *) klass;

  GST_DEBUG_CATEGORY_INIT (dtmf_detect_debug, "dtmfdetect", 0, "dtmfdetect");

  gobject_class->finalize = gst_dtmf_detect_finalize;

  gst_element_class_add_static_pad_template (gstelement_class, &srctemplate);
  gst_element_class_add_static_pad_template (gstelement_class, &sinktemplate);

  gst_element_class_set_static_metadata (gstelement_class,
      "DTMF detector element", "Filter/Analyzer/Audio",
      "This element detects DTMF tones",
      "Olivier Crete <olivier.crete@collabora.com>");

  gstbasetransform_class->set_caps =
      GST_DEBUG_FUNCPTR (gst_dtmf_detect_set_caps);
  gstbasetransform_class->transform_ip =
      GST_DEBUG_FUNCPTR (gst_dtmf_detect_transform_ip);
  gstbasetransform_class->sink_event =
      GST_DEBUG_FUNCPTR (gst_dtmf_detect_sink_event);
}

static void
gst_dtmf_detect_init (GstDtmfDetect * dtmfdetect)
{
  gst_base_transform_set_passthrough (GST_BASE_TRANSFORM (dtmfdetect), TRUE);
  gst_base_transform_set_gap_aware (GST_BASE_TRANSFORM (dtmfdetect), TRUE);
}

static void
gst_dtmf_detect_finalize (GObject * object)
{
  GstDtmfDetect *self = GST_DTMF_DETECT (object);

  if (self->dtmf_state)
    dtmf_rx_free (self->dtmf_state);

  G_OBJECT_CLASS (gst_dtmf_detect_parent_class)->finalize (object);
}

static void
gst_dtmf_detect_state_reset (GstDtmfDetect * self)
{
  if (self->dtmf_state)
    dtmf_rx_free (self->dtmf_state);
  self->dtmf_state = dtmf_rx_init (NULL, NULL, NULL);
}

static gboolean
gst_dtmf_detect_set_caps (GstBaseTransform * trans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstDtmfDetect *self = GST_DTMF_DETECT (trans);

  gst_dtmf_detect_state_reset (self);

  return TRUE;
}


static GstFlowReturn
gst_dtmf_detect_transform_ip (GstBaseTransform * trans, GstBuffer * buf)
{
  GstDtmfDetect *self = GST_DTMF_DETECT (trans);
  gint dtmf_count;
  gchar dtmfbuf[MAX_DTMF_DIGITS] = "";
  gint i;
  GstMapInfo map;

  if (GST_BUFFER_IS_DISCONT (buf))
    gst_dtmf_detect_state_reset (self);
  if (GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_GAP))
    return GST_FLOW_OK;

  gst_buffer_map (buf, &map, GST_MAP_READ);

  dtmf_rx (self->dtmf_state, (gint16 *) map.data, map.size / 2);

  dtmf_count = dtmf_rx_get (self->dtmf_state, dtmfbuf, MAX_DTMF_DIGITS);

  if (dtmf_count)
    GST_DEBUG_OBJECT (self, "Got %d DTMF events: %s", dtmf_count, dtmfbuf);
  else
    GST_LOG_OBJECT (self, "Got no DTMF events");

  gst_buffer_unmap (buf, &map);

  for (i = 0; i < dtmf_count; i++) {
    GstMessage *dtmf_message = NULL;
    GstStructure *structure;
    gint dtmf_payload_event;

    GST_DEBUG_OBJECT (self, "Got DTMF event %c", dtmfbuf[i]);

    switch (dtmfbuf[i]) {
      case '0':
        dtmf_payload_event = 0;
        break;
      case '1':
        dtmf_payload_event = 1;
        break;
      case '2':
        dtmf_payload_event = 2;
        break;
      case '3':
        dtmf_payload_event = 3;
        break;
      case '4':
        dtmf_payload_event = 4;
        break;
      case '5':
        dtmf_payload_event = 5;
        break;
      case '6':
        dtmf_payload_event = 6;
        break;
      case '7':
        dtmf_payload_event = 7;
        break;
      case '8':
        dtmf_payload_event = 8;
        break;
      case '9':
        dtmf_payload_event = 9;
        break;
      case '*':
        dtmf_payload_event = 10;
        break;
      case '#':
        dtmf_payload_event = 11;
        break;
      case 'A':
        dtmf_payload_event = 12;
        break;
      case 'B':
        dtmf_payload_event = 13;
        break;
      case 'C':
        dtmf_payload_event = 14;
        break;
      case 'D':
        dtmf_payload_event = 15;
        break;
      default:
        continue;
    }

    structure = gst_structure_new ("dtmf-event",
        "type", G_TYPE_INT, 1,
        "number", G_TYPE_INT, dtmf_payload_event,
        "method", G_TYPE_INT, 2, NULL);
    dtmf_message = gst_message_new_element (GST_OBJECT (self), structure);
    gst_element_post_message (GST_ELEMENT (self), dtmf_message);
  }

  return GST_FLOW_OK;
}


static gboolean
gst_dtmf_detect_sink_event (GstBaseTransform * trans, GstEvent * event)
{
  GstDtmfDetect *self = GST_DTMF_DETECT (trans);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_STOP:
      gst_dtmf_detect_state_reset (self);
      break;
    default:
      break;
  }

  return GST_BASE_TRANSFORM_CLASS (gst_dtmf_detect_parent_class)->sink_event
      (trans, event);
}
