/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2004-2010  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>

#include "gstbluezelements.h"
#include "gsta2dpsink.h"

#include <gst/rtp/gstrtpbasepayload.h>

GST_DEBUG_CATEGORY_STATIC (gst_a2dp_sink_debug);
#define GST_CAT_DEFAULT gst_a2dp_sink_debug

#define A2DP_SBC_RTP_PAYLOAD_TYPE 1

#define DEFAULT_AUTOCONNECT TRUE

enum
{
  PROP_0,
  PROP_DEVICE,
  PROP_AUTOCONNECT,
  PROP_TRANSPORT
};

#define parent_class gst_a2dp_sink_parent_class
G_DEFINE_TYPE (GstA2dpSink, gst_a2dp_sink, GST_TYPE_BIN);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (a2dpsink, "a2dpsink", GST_RANK_NONE,
    GST_TYPE_A2DP_SINK, bluez_element_init (plugin));

static GstStaticPadTemplate gst_a2dp_sink_factory =
    GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-sbc, "
        "rate = (int) { 16000, 32000, 44100, 48000 }, "
        "channels = (int) [ 1, 2 ], "
        "channel-mode = (string) { mono, dual, stereo, joint }, "
        "blocks = (int) { 4, 8, 12, 16 }, "
        "subbands = (int) { 4, 8 }, "
        "allocation-method = (string) { snr, loudness }, "
        "bitpool = (int) [ 2, " TEMPLATE_MAX_BITPOOL_STR " ]; "
        "audio/mpeg; " "audio/x-ldac"));

static gboolean gst_a2dp_sink_handle_event (GstPad * pad,
    GstObject * pad_parent, GstEvent * event);
static gboolean gst_a2dp_sink_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static GstCaps *gst_a2dp_sink_get_caps (GstA2dpSink * self);

/*
 * Helper function to create elements, add to the bin and link it
 * to another element.
 */
static GstElement *
gst_a2dp_sink_init_element (GstA2dpSink * self, const gchar * elementname,
    const gchar * name)
{
  GstElement *element;
  GstPad *sinkpad;

  GST_LOG_OBJECT (self, "Initializing %s", elementname);

  element = gst_element_factory_make (elementname, name);
  if (element == NULL) {
    GST_DEBUG_OBJECT (self, "Couldn't create %s", elementname);
    return NULL;
  }

  if (!gst_bin_add (GST_BIN (self), element)) {
    GST_DEBUG_OBJECT (self, "failed to add %s to the bin", elementname);
    goto cleanup_and_fail;
  }

  sinkpad = gst_element_get_static_pad (element, "sink");
  if (!gst_ghost_pad_set_target (GST_GHOST_PAD (self->ghostpad), sinkpad)) {
    GST_ERROR_OBJECT (self, "Failed to set target for ghost pad");
    goto remove_element_and_fail;
  }
  gst_object_unref (sinkpad);

  if (!gst_element_sync_state_with_parent (element)) {
    GST_DEBUG_OBJECT (self, "%s failed to go to playing", elementname);
    goto remove_element_and_fail;
  }

  return element;

remove_element_and_fail:
  gst_object_unref (sinkpad);
  gst_element_set_state (element, GST_STATE_NULL);
  gst_bin_remove (GST_BIN (self), element);
  return NULL;

cleanup_and_fail:
  g_object_unref (G_OBJECT (element));

  return NULL;
}

static void
gst_a2dp_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstA2dpSink *self = GST_A2DP_SINK (object);

  switch (prop_id) {
    case PROP_DEVICE:
      if (self->sink != NULL)
        gst_avdtp_sink_set_device (self->sink, g_value_get_string (value));

      g_free (self->device);
      self->device = g_value_dup_string (value);
      break;

    case PROP_TRANSPORT:
      if (self->sink != NULL)
        gst_avdtp_sink_set_transport (self->sink, g_value_get_string (value));

      g_free (self->transport);
      self->transport = g_value_dup_string (value);
      break;

    case PROP_AUTOCONNECT:
      self->autoconnect = g_value_get_boolean (value);

      if (self->sink != NULL)
        g_object_set (G_OBJECT (self->sink), "auto-connect",
            self->autoconnect, NULL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_a2dp_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstA2dpSink *self = GST_A2DP_SINK (object);
  gchar *device, *transport;

  switch (prop_id) {
    case PROP_DEVICE:
      if (self->sink != NULL) {
        device = gst_avdtp_sink_get_device (self->sink);
        if (device != NULL)
          g_value_take_string (value, device);
      }
      break;
    case PROP_AUTOCONNECT:
      if (self->sink != NULL)
        g_object_get (G_OBJECT (self->sink), "auto-connect",
            &self->autoconnect, NULL);

      g_value_set_boolean (value, self->autoconnect);
      break;
    case PROP_TRANSPORT:
      if (self->sink != NULL) {
        transport = gst_avdtp_sink_get_transport (self->sink);
        if (transport != NULL)
          g_value_take_string (value, transport);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_a2dp_sink_init_ghost_pad (GstA2dpSink * self)
{
  GstPadTemplate *templ;

  /* now we add a ghostpad */
  templ = gst_static_pad_template_get (&gst_a2dp_sink_factory);
  self->ghostpad = gst_ghost_pad_new_no_target_from_template ("sink", templ);
  g_object_unref (templ);

  /* the getcaps of our ghostpad must reflect the device caps */
  gst_pad_set_query_function (self->ghostpad, gst_a2dp_sink_query);

  gst_pad_set_event_function (self->ghostpad, gst_a2dp_sink_handle_event);

  if (!gst_element_add_pad (GST_ELEMENT (self), self->ghostpad))
    GST_ERROR_OBJECT (self, "failed to add ghostpad");

  return TRUE;
}

static void
gst_a2dp_sink_remove_dynamic_elements (GstA2dpSink * self)
{
  if (self->rtp) {
    GST_LOG_OBJECT (self, "removing rtp element from the bin");
    if (!gst_bin_remove (GST_BIN (self), GST_ELEMENT (self->rtp)))
      GST_WARNING_OBJECT (self, "failed to remove rtp " "element from bin");
    else
      self->rtp = NULL;
  }
}

static GstStateChangeReturn
gst_a2dp_sink_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret;
  GstA2dpSink *self = GST_A2DP_SINK (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      self->taglist = gst_tag_list_new_empty ();
      break;

    case GST_STATE_CHANGE_NULL_TO_READY:
      if (self->device != NULL)
        gst_avdtp_sink_set_device (self->sink, self->device);

      if (self->transport != NULL)
        gst_avdtp_sink_set_transport (self->sink, self->transport);

      g_object_set (G_OBJECT (self->sink), "auto-connect",
          self->autoconnect, NULL);

      break;

    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      if (self->taglist) {
        gst_tag_list_unref (self->taglist);
        self->taglist = NULL;
      }
      break;

    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_a2dp_sink_remove_dynamic_elements (self);

      break;
    default:
      break;
  }

  return ret;
}

static void
gst_a2dp_sink_class_init (GstA2dpSinkClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->set_property = GST_DEBUG_FUNCPTR (gst_a2dp_sink_set_property);
  object_class->get_property = GST_DEBUG_FUNCPTR (gst_a2dp_sink_get_property);

  element_class->change_state = GST_DEBUG_FUNCPTR (gst_a2dp_sink_change_state);

  g_object_class_install_property (object_class, PROP_DEVICE,
      g_param_spec_string ("device", "Device",
          "Bluetooth remote device address", NULL, G_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_AUTOCONNECT,
      g_param_spec_boolean ("auto-connect", "Auto-connect",
          "Automatically attempt to connect to device",
          DEFAULT_AUTOCONNECT, G_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_TRANSPORT,
      g_param_spec_string ("transport", "Transport",
          "Use configured transport", NULL, G_PARAM_READWRITE));

  gst_element_class_set_static_metadata (element_class, "Bluetooth A2DP sink",
      "Sink/Audio", "Plays audio to an A2DP device",
      "Marcel Holtmann <marcel@holtmann.org>");

  GST_DEBUG_CATEGORY_INIT (gst_a2dp_sink_debug, "a2dpsink", 0,
      "A2DP sink element");

  gst_element_class_add_static_pad_template (element_class,
      &gst_a2dp_sink_factory);
}

GstCaps *
gst_a2dp_sink_get_device_caps (GstA2dpSink * self)
{
  return gst_avdtp_sink_get_device_caps (self->sink);
}

static GstCaps *
gst_a2dp_sink_get_caps (GstA2dpSink * self)
{
  GstCaps *caps = NULL;

  if (self->sink != NULL) {
    caps = gst_a2dp_sink_get_device_caps (self);
    GST_LOG_OBJECT (self, "Got device caps %" GST_PTR_FORMAT, caps);
  }

  if (!caps)
    caps = gst_static_pad_template_get_caps (&gst_a2dp_sink_factory);

  return caps;
}

static gboolean
gst_a2dp_sink_init_avdtp_sink (GstA2dpSink * self)
{
  GstElement *sink;

  if (self->sink == NULL)
    sink = gst_element_factory_make ("avdtpsink", "avdtpsink");
  else
    sink = GST_ELEMENT (self->sink);

  if (sink == NULL) {
    GST_ERROR_OBJECT (self, "Couldn't create avdtpsink");
    return FALSE;
  }

  if (!gst_bin_add (GST_BIN (self), sink)) {
    GST_ERROR_OBJECT (self, "failed to add avdtpsink " "to the bin");
    goto cleanup_and_fail;
  }

  self->sink = GST_AVDTP_SINK (sink);
  g_object_set (G_OBJECT (self->sink), "device", self->device, NULL);
  g_object_set (G_OBJECT (self->sink), "transport", self->transport, NULL);

  gst_element_sync_state_with_parent (sink);

  return TRUE;

cleanup_and_fail:
  if (sink != NULL)
    g_object_unref (G_OBJECT (sink));

  return FALSE;
}

static gboolean
gst_a2dp_sink_init_rtp_sbc_element (GstA2dpSink * self)
{
  GstElement *rtppay;

  /* if we already have a rtp, we don't need a new one */
  if (self->rtp != NULL)
    return TRUE;

  rtppay = gst_a2dp_sink_init_element (self, "rtpsbcpay", "rtp");
  if (rtppay == NULL)
    return FALSE;

  self->rtp = rtppay;
  g_object_set (self->rtp, "min-frames", -1, NULL);

  gst_element_set_state (rtppay, GST_STATE_PAUSED);

  return TRUE;
}

static gboolean
gst_a2dp_sink_init_rtp_mpeg_element (GstA2dpSink * self)
{
  GstElement *rtppay;

  /* check if we don't need a new rtp */
  if (self->rtp)
    return TRUE;

  GST_LOG_OBJECT (self, "Initializing rtp mpeg element");

  rtppay = gst_a2dp_sink_init_element (self, "rtpmpapay", "rtp");
  if (rtppay == NULL)
    return FALSE;

  self->rtp = rtppay;

  gst_element_set_state (rtppay, GST_STATE_PAUSED);

  return TRUE;
}

static gboolean
gst_a2dp_sink_init_rtp_ldac_element (GstA2dpSink * self)
{
  GstElement *rtppay;

  /* check if we don't need a new rtp */
  if (self->rtp)
    return TRUE;

  GST_LOG_OBJECT (self, "Initializing rtp ldac element");

  rtppay = gst_a2dp_sink_init_element (self, "rtpldacpay", "rtp");
  if (rtppay == NULL)
    return FALSE;

  self->rtp = rtppay;

  gst_element_set_state (rtppay, GST_STATE_PAUSED);

  return TRUE;
}

static gboolean
gst_a2dp_sink_init_dynamic_elements (GstA2dpSink * self, GstCaps * caps)
{
  GstStructure *structure;
  GstEvent *event;
  gboolean crc;
  gchar *mode = NULL;
  guint mtu;

  structure = gst_caps_get_structure (caps, 0);

  /* first, we need to create our rtp payloader */
  if (gst_structure_has_name (structure, "audio/x-sbc")) {
    GST_LOG_OBJECT (self, "sbc media received");
    if (!gst_a2dp_sink_init_rtp_sbc_element (self))
      return FALSE;
  } else if (gst_structure_has_name (structure, "audio/mpeg")) {
    GST_LOG_OBJECT (self, "mp3 media received");
    if (!gst_a2dp_sink_init_rtp_mpeg_element (self))
      return FALSE;
  } else if (gst_structure_has_name (structure, "audio/x-ldac")) {
    GST_LOG_OBJECT (self, "ldac media received");
    if (!gst_a2dp_sink_init_rtp_ldac_element (self))
      return FALSE;
  } else {
    GST_ERROR_OBJECT (self, "Unexpected media type");
    return FALSE;
  }

  if (!gst_element_link (GST_ELEMENT (self->rtp), GST_ELEMENT (self->sink))) {
    GST_ERROR_OBJECT (self, "couldn't link rtp payloader to avdtpsink");
    return FALSE;
  }

  /* check if we should push the taglist FIXME should we push this?
   * we can send the tags directly if needed */
  if (self->taglist != NULL && gst_structure_has_name (structure, "audio/mpeg")) {

    event = gst_event_new_tag (self->taglist);

    /* send directly the crc */
    if (gst_tag_list_get_boolean (self->taglist, "has-crc", &crc))
      gst_avdtp_sink_set_crc (self->sink, crc);

    if (gst_tag_list_get_string (self->taglist, "channel-mode", &mode))
      gst_avdtp_sink_set_channel_mode (self->sink, mode);

    gst_pad_send_event (self->ghostpad, event);

    self->taglist = NULL;
    g_free (mode);
  }

  mtu = gst_avdtp_sink_get_link_mtu (self->sink);
  GST_INFO_OBJECT (self, "Setting MTU to %u", mtu);
  g_object_set (self->rtp, "mtu", mtu, NULL);

  return TRUE;
}

static gboolean
gst_a2dp_sink_handle_event (GstPad * pad, GstObject * pad_parent,
    GstEvent * event)
{
  GstA2dpSink *self;
  GstTagList *taglist = NULL;

  self = GST_A2DP_SINK (pad_parent);

  if (GST_EVENT_TYPE (event) == GST_EVENT_TAG) {
    if (self->taglist == NULL)
      gst_event_parse_tag (event, &self->taglist);
    else {
      gst_event_parse_tag (event, &taglist);
      gst_tag_list_insert (self->taglist, taglist, GST_TAG_MERGE_REPLACE);
    }
  } else if (GST_EVENT_TYPE (event) == GST_EVENT_CAPS) {
    GstCaps *caps = NULL;

    gst_event_parse_caps (event, &caps);
    gst_a2dp_sink_init_dynamic_elements (self, caps);
  }

  return gst_pad_event_default (pad, pad_parent, event);
}

static gboolean
gst_a2dp_sink_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstA2dpSink *sink = GST_A2DP_SINK (parent);
  gboolean ret;

  if (GST_QUERY_TYPE (query) == GST_QUERY_CAPS) {
    GstCaps *caps;

    caps = gst_a2dp_sink_get_caps (sink);
    gst_query_set_caps_result (query, caps);
    gst_caps_unref (caps);
    ret = TRUE;
  } else {
    ret = gst_pad_query_default (pad, parent, query);
  }

  return ret;
}

static void
gst_a2dp_sink_init (GstA2dpSink * self)
{
  self->sink = NULL;
  self->rtp = NULL;
  self->device = NULL;
  self->transport = NULL;
  self->autoconnect = DEFAULT_AUTOCONNECT;
  self->taglist = NULL;
  self->ghostpad = NULL;

  gst_a2dp_sink_init_ghost_pad (self);

  gst_a2dp_sink_init_avdtp_sink (self);
}
