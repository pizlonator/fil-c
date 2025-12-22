/* GStreamer
 * Copyright (C) 2006 Thomas Vander Stichele <thomas at apestaart dot org>
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
 * SECTION:element-gdpdepay
 * @title: gdpdepay
 * @see_also: gdppay
 *
 * This element depayloads GStreamer Data Protocol buffers back to deserialized
 * buffers and events.
 *
 * |[
 * gst-launch-1.0 -v -m filesrc location=test.gdp ! gdpdepay ! xvimagesink
 * ]| This pipeline plays back a serialized video stream as created in the
 * example for gdppay.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "dataprotocol.h"

#include "gstgdpelements.h"
#include "gstgdpdepay.h"

enum
{
  PROP_0,
  PROP_TS_OFFSET
};

static GstStaticPadTemplate gdp_depay_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-gdp"));

static GstStaticPadTemplate gdp_depay_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC (gst_gdp_depay_debug);
#define GST_CAT_DEFAULT gst_gdp_depay_debug

#define _do_init \
    GST_DEBUG_CATEGORY_INIT (gst_gdp_depay_debug, "gdpdepay", 0, \
    "GDP depayloader");
#define gst_gdp_depay_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGDPDepay, gst_gdp_depay,
    GST_TYPE_ELEMENT, _do_init);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (gdpdepay, "gdpdepay", GST_RANK_NONE,
    GST_TYPE_GDP_DEPAY, gdp_element_init (plugin));

static gboolean gst_gdp_depay_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_gdp_depay_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event);

static GstFlowReturn gst_gdp_depay_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer);

static GstStateChangeReturn gst_gdp_depay_change_state (GstElement *
    element, GstStateChange transition);

static void gst_gdp_depay_finalize (GObject * object);
static void gst_gdp_depay_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gdp_depay_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_gdp_depay_decide_allocation (GstGDPDepay * depay);

static void
gst_gdp_depay_class_init (GstGDPDepayClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_gdp_depay_set_property;
  gobject_class->get_property = gst_gdp_depay_get_property;

  g_object_class_install_property (gobject_class, PROP_TS_OFFSET,
      g_param_spec_int64 ("ts-offset", "Timestamp Offset",
          "Timestamp Offset",
          G_MININT64, G_MAXINT64, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (gstelement_class,
      "GDP Depayloader", "GDP/Depayloader",
      "Depayloads GStreamer Data Protocol buffers",
      "Thomas Vander Stichele <thomas at apestaart dot org>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &gdp_depay_sink_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gdp_depay_src_template);

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_gdp_depay_change_state);
  gobject_class->finalize = gst_gdp_depay_finalize;
}

static void
gst_gdp_depay_init (GstGDPDepay * gdpdepay)
{
  gdpdepay->sinkpad =
      gst_pad_new_from_static_template (&gdp_depay_sink_template, "sink");
  gst_pad_set_chain_function (gdpdepay->sinkpad,
      GST_DEBUG_FUNCPTR (gst_gdp_depay_chain));
  gst_pad_set_event_function (gdpdepay->sinkpad,
      GST_DEBUG_FUNCPTR (gst_gdp_depay_sink_event));
  gst_element_add_pad (GST_ELEMENT (gdpdepay), gdpdepay->sinkpad);

  gdpdepay->srcpad =
      gst_pad_new_from_static_template (&gdp_depay_src_template, "src");
  gst_pad_set_event_function (gdpdepay->srcpad,
      GST_DEBUG_FUNCPTR (gst_gdp_depay_src_event));
  /* our caps will always be decided by the incoming GDP caps buffers */
  gst_pad_use_fixed_caps (gdpdepay->srcpad);
  gst_element_add_pad (GST_ELEMENT (gdpdepay), gdpdepay->srcpad);

  gdpdepay->adapter = gst_adapter_new ();

  gdpdepay->allocator = NULL;
  gst_allocation_params_init (&gdpdepay->allocation_params);
}

static void
gst_gdp_depay_finalize (GObject * gobject)
{
  GstGDPDepay *this;

  this = GST_GDP_DEPAY (gobject);
  if (this->caps)
    gst_caps_unref (this->caps);
  g_free (this->header);
  gst_adapter_clear (this->adapter);
  g_object_unref (this->adapter);
  if (this->allocator)
    gst_object_unref (this->allocator);

  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (gobject));
}

static void
gst_gdp_depay_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGDPDepay *this;

  this = GST_GDP_DEPAY (object);

  switch (prop_id) {
    case PROP_TS_OFFSET:
      this->ts_offset = g_value_get_int64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gdp_depay_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGDPDepay *this;

  this = GST_GDP_DEPAY (object);

  switch (prop_id) {
    case PROP_TS_OFFSET:
      g_value_set_int64 (value, this->ts_offset);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_gdp_depay_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstGDPDepay *this;
  gboolean res = TRUE;

  this = GST_GDP_DEPAY (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
      /* forward flush start */
      res = gst_pad_push_event (this->srcpad, event);
      break;
    case GST_EVENT_FLUSH_STOP:
      /* clear adapter on flush */
      gst_adapter_clear (this->adapter);
      /* forward flush stop */
      res = gst_pad_push_event (this->srcpad, event);
      break;
    case GST_EVENT_EOS:
      /* after EOS, we don't expect to output anything anymore */
      res = gst_pad_push_event (this->srcpad, event);
      break;
    case GST_EVENT_SEGMENT:
    case GST_EVENT_TAG:
    case GST_EVENT_BUFFERSIZE:
    default:
      /* we unref most events as we take them from the datastream */
      gst_event_unref (event);
      break;
  }

  return res;
}

static gboolean
gst_gdp_depay_src_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstGDPDepay *this;
  gboolean res = TRUE;

  this = GST_GDP_DEPAY (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
      /* we refuse seek for now. */
      gst_event_unref (event);
      res = FALSE;
      break;
    case GST_EVENT_QOS:
    case GST_EVENT_NAVIGATION:
    default:
      /* everything else is passed */
      res = gst_pad_push_event (this->sinkpad, event);
      break;
  }

  return res;
}

static GstFlowReturn
gst_gdp_depay_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstGDPDepay *this;
  GstFlowReturn ret = GST_FLOW_OK;
  GstCaps *caps;
  GstBuffer *buf;
  GstEvent *event;
  guint available;

  this = GST_GDP_DEPAY (parent);

  if (gst_pad_check_reconfigure (this->srcpad)) {
    gst_gdp_depay_decide_allocation (this);
  }

  /* On DISCONT, get rid of accumulated data. We assume a buffer after the
   * DISCONT contains (part of) a new valid header, if not we error because we
   * lost sync */
  if (GST_BUFFER_IS_DISCONT (buffer)) {
    gst_adapter_clear (this->adapter);
    this->state = GST_GDP_DEPAY_STATE_HEADER;
  }
  gst_adapter_push (this->adapter, buffer);

  while (TRUE) {
    switch (this->state) {
      case GST_GDP_DEPAY_STATE_HEADER:
      {
        guint8 *header;

        /* collect a complete header, validate and store the header. Figure out
         * the payload length and switch to the PAYLOAD state */
        available = gst_adapter_available (this->adapter);
        if (available < GST_DP_HEADER_LENGTH)
          goto done;

        GST_LOG_OBJECT (this, "reading GDP header from adapter");
        header = gst_adapter_take (this->adapter, GST_DP_HEADER_LENGTH);
        if (!gst_dp_validate_header (GST_DP_HEADER_LENGTH, header)) {
          g_free (header);
          goto header_validate_error;
        }

        /* store types and payload length. Also store the header, which we need
         * to make the payload. */
        this->payload_length = gst_dp_header_payload_length (header);
        this->payload_type = gst_dp_header_payload_type (header);
        /* free previous header and store new one. */
        g_free (this->header);
        this->header = header;

        GST_LOG_OBJECT (this,
            "read GDP header, payload size %d, payload type %d, switching to state PAYLOAD",
            this->payload_length, this->payload_type);
        this->state = GST_GDP_DEPAY_STATE_PAYLOAD;
        break;
      }
      case GST_GDP_DEPAY_STATE_PAYLOAD:
      {
        /* in this state we wait for all the payload data to be available in the
         * adapter. Then we switch to the state where we actually process the
         * payload. */
        available = gst_adapter_available (this->adapter);
        if (available < this->payload_length)
          goto done;

        /* change state based on type */
        if (this->payload_type == GST_DP_PAYLOAD_BUFFER) {
          GST_LOG_OBJECT (this, "switching to state BUFFER");
          this->state = GST_GDP_DEPAY_STATE_BUFFER;
        } else if (this->payload_type == GST_DP_PAYLOAD_CAPS) {
          GST_LOG_OBJECT (this, "switching to state CAPS");
          this->state = GST_GDP_DEPAY_STATE_CAPS;
        } else if (this->payload_type >= GST_DP_PAYLOAD_EVENT_NONE) {
          GST_LOG_OBJECT (this, "switching to state EVENT");
          this->state = GST_GDP_DEPAY_STATE_EVENT;
        } else {
          goto wrong_type;
        }

        if (this->payload_length) {
          const guint8 *data;
          gboolean res;

          data = gst_adapter_map (this->adapter, this->payload_length);
          res = gst_dp_validate_payload (GST_DP_HEADER_LENGTH, this->header,
              data);
          gst_adapter_unmap (this->adapter);

          if (!res)
            goto payload_validate_error;
        }

        break;
      }
      case GST_GDP_DEPAY_STATE_BUFFER:
      {
        /* if we receive a buffer without caps first, we error out */
        if (!this->caps)
          goto no_caps;

        GST_LOG_OBJECT (this, "reading GDP buffer from adapter");
        buf =
            gst_dp_buffer_from_header (GST_DP_HEADER_LENGTH, this->header,
            this->allocator, &this->allocation_params);
        if (!buf)
          goto buffer_failed;

        /* now take the payload if there is any */
        if (this->payload_length > 0) {
          GstMapInfo map;

          gst_buffer_map (buf, &map, GST_MAP_WRITE);
          gst_adapter_copy (this->adapter, map.data, 0, this->payload_length);
          gst_buffer_unmap (buf, &map);

          gst_adapter_flush (this->adapter, this->payload_length);
        }

        if (GST_BUFFER_TIMESTAMP (buf) > -this->ts_offset)
          GST_BUFFER_TIMESTAMP (buf) += this->ts_offset;
        else
          GST_BUFFER_TIMESTAMP (buf) = 0;

        if (GST_BUFFER_DTS (buf) > -this->ts_offset)
          GST_BUFFER_DTS (buf) += this->ts_offset;
        else
          GST_BUFFER_DTS (buf) = 0;

        /* set caps and push */
        GST_LOG_OBJECT (this, "deserialized buffer %p, pushing, timestamp %"
            GST_TIME_FORMAT ", duration %" GST_TIME_FORMAT
            ", offset %" G_GINT64_FORMAT ", offset_end %" G_GINT64_FORMAT
            ", size %" G_GSIZE_FORMAT ", flags 0x%x",
            buf,
            GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buf)),
            GST_TIME_ARGS (GST_BUFFER_DURATION (buf)),
            GST_BUFFER_OFFSET (buf), GST_BUFFER_OFFSET_END (buf),
            gst_buffer_get_size (buf), GST_BUFFER_FLAGS (buf));
        ret = gst_pad_push (this->srcpad, buf);
        if (ret != GST_FLOW_OK)
          goto push_error;

        GST_LOG_OBJECT (this, "switching to state HEADER");
        this->state = GST_GDP_DEPAY_STATE_HEADER;
        break;
      }
      case GST_GDP_DEPAY_STATE_CAPS:
      {
        guint8 *payload;

        /* take the payload of the caps */
        GST_LOG_OBJECT (this, "reading GDP caps from adapter");
        payload = gst_adapter_take (this->adapter, this->payload_length);
        caps = gst_dp_caps_from_packet (GST_DP_HEADER_LENGTH, this->header,
            payload);
        g_free (payload);
        if (!caps)
          goto caps_failed;

        GST_DEBUG_OBJECT (this, "deserialized caps %" GST_PTR_FORMAT, caps);
        gst_caps_replace (&(this->caps), caps);
        gst_pad_set_caps (this->srcpad, caps);
        gst_gdp_depay_decide_allocation (this);
        /* drop the creation ref we still have */
        gst_caps_unref (caps);

        GST_LOG_OBJECT (this, "switching to state HEADER");
        this->state = GST_GDP_DEPAY_STATE_HEADER;
        break;
      }
      case GST_GDP_DEPAY_STATE_EVENT:
      {
        guint8 *payload;

        GST_LOG_OBJECT (this, "reading GDP event from adapter");

        /* adapter doesn't like 0 length payload */
        if (this->payload_length > 0)
          payload = gst_adapter_take (this->adapter, this->payload_length);
        else
          payload = NULL;
        event = gst_dp_event_from_packet (GST_DP_HEADER_LENGTH, this->header,
            payload);
        g_free (payload);
        if (!event)
          goto event_failed;

        GST_DEBUG_OBJECT (this, "deserialized event %p of type %s, pushing",
            event, gst_event_type_get_name (event->type));
        gst_pad_push_event (this->srcpad, event);

        GST_LOG_OBJECT (this, "switching to state HEADER");
        this->state = GST_GDP_DEPAY_STATE_HEADER;
        break;
      }
    }
  }

done:
  return ret;

  /* ERRORS */
header_validate_error:
  {
    GST_ELEMENT_ERROR (this, STREAM, DECODE, (NULL),
        ("GDP packet header does not validate"));
    ret = GST_FLOW_ERROR;
    goto done;
  }
payload_validate_error:
  {
    GST_ELEMENT_ERROR (this, STREAM, DECODE, (NULL),
        ("GDP packet payload does not validate"));
    ret = GST_FLOW_ERROR;
    goto done;
  }
wrong_type:
  {
    GST_ELEMENT_ERROR (this, STREAM, DECODE, (NULL),
        ("GDP packet header is of wrong type"));
    ret = GST_FLOW_ERROR;
    goto done;
  }
no_caps:
  {
    GST_ELEMENT_ERROR (this, STREAM, DECODE, (NULL),
        ("Received a buffer without first receiving caps"));
    ret = GST_FLOW_NOT_NEGOTIATED;
    goto done;
  }
buffer_failed:
  {
    GST_ELEMENT_ERROR (this, STREAM, DECODE, (NULL),
        ("could not create buffer from GDP packet"));
    ret = GST_FLOW_ERROR;
    goto done;
  }
push_error:
  {
    GST_WARNING_OBJECT (this, "pushing depayloaded buffer returned %d", ret);
    goto done;
  }
caps_failed:
  {
    GST_ELEMENT_ERROR (this, STREAM, DECODE, (NULL),
        ("could not create caps from GDP packet"));
    ret = GST_FLOW_ERROR;
    goto done;
  }
event_failed:
  {
    GST_ELEMENT_ERROR (this, STREAM, DECODE, (NULL),
        ("could not create event from GDP packet"));
    ret = GST_FLOW_ERROR;
    goto done;
  }
}

static GstStateChangeReturn
gst_gdp_depay_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret;
  GstGDPDepay *this = GST_GDP_DEPAY (element);

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      if (this->caps) {
        gst_caps_unref (this->caps);
        this->caps = NULL;
      }
      gst_adapter_clear (this->adapter);
      if (this->allocator)
        gst_object_unref (this->allocator);
      this->allocator = NULL;
      gst_allocation_params_init (&this->allocation_params);
      break;
    default:
      break;
  }
  return ret;
}

static void
gst_gdp_depay_decide_allocation (GstGDPDepay * gdpdepay)
{
  GstAllocator *allocator;
  GstAllocationParams params;
  GstQuery *query = NULL;
  GstCaps *caps;

  caps = gst_pad_query_caps (gdpdepay->srcpad, NULL);
  if (!caps) {
    GST_LOG_OBJECT (gdpdepay,
        "No peer pad caps found. Using default allocator.");
    return;
  }

  if (!gst_caps_is_fixed (caps)) {
    GST_LOG_OBJECT (gdpdepay, "Caps on src pad are not fixed. Not querying.");
    return;
  }

  query = gst_query_new_allocation (caps, TRUE);
  if (!gst_pad_peer_query (gdpdepay->srcpad, query)) {
    GST_WARNING_OBJECT (gdpdepay, "Peer allocation query failed.");
  }

  if (gst_query_get_n_allocation_params (query) > 0) {
    gst_query_parse_nth_allocation_param (query, 0, &allocator, &params);
  } else {
    allocator = NULL;
    gst_allocation_params_init (&params);
  }

  if (gdpdepay->allocator)
    gst_object_unref (gdpdepay->allocator);

  gdpdepay->allocator = allocator;
  gdpdepay->allocation_params = params;

  gst_caps_unref (caps);
  gst_query_unref (query);
}
