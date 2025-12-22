/* GStreamer
 * Copyright (C) 2010 David Schleef <ds@schleef.org>
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
 * SECTION:element-chopmydata
 * @title: gstchopmydata
 *
 * The chopmydata element takes an incoming stream and chops it up
 * into randomly sized buffers.  Size of outgoing buffers are determined
 * by the max-size, min-size, and step-size properties.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v audiotestsrc num-buffers=10 ! chopmydata min-size=100
 * max-size=200 step-size=2 ! fakesink -v
 * ]|
 *
 * This pipeline will create 10 buffers that are by default 2048 bytes
 * each (1024 samples each), and chop them up into buffers that range
 * in size from 100 bytes to 200 bytes, with the restriction that sizes
 * are a multiple of 2.  This restriction is important, because the
 * default sample size for audiotestsrc is 2 bytes (one channel, 16-bit
 * audio).
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/gst.h>
#include "gstdebugutilsbadelements.h"
#include "gstchopmydata.h"

/* prototypes */


static void gst_chop_my_data_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_chop_my_data_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);

static GstStateChangeReturn
gst_chop_my_data_change_state (GstElement * element, GstStateChange transition);

static GstFlowReturn gst_chop_my_data_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer);
static gboolean gst_chop_my_data_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_chop_my_data_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event);

#define DEFAULT_MAX_SIZE 4096
#define DEFAULT_MIN_SIZE 1
#define DEFAULT_STEP_SIZE 1

enum
{
  PROP_0,
  PROP_MAX_SIZE,
  PROP_MIN_SIZE,
  PROP_STEP_SIZE
};

/* pad templates */

static GstStaticPadTemplate gst_chop_my_data_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate gst_chop_my_data_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

/* class initialization */

#define gst_chop_my_data_parent_class parent_class
G_DEFINE_TYPE (GstChopMyData, gst_chop_my_data, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE (chopmydata, "chopmydata",
    GST_RANK_NONE, gst_chop_my_data_get_type ());

static void
gst_chop_my_data_class_init (GstChopMyDataClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gobject_class->set_property = gst_chop_my_data_set_property;
  gobject_class->get_property = gst_chop_my_data_get_property;
  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_chop_my_data_change_state);

  g_object_class_install_property (gobject_class, PROP_MAX_SIZE,
      g_param_spec_int ("max-size", "max-size",
          "Maximum size of outgoing buffers", 1, G_MAXINT,
          DEFAULT_MAX_SIZE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MIN_SIZE,
      g_param_spec_int ("min-size", "max-size",
          "Minimum size of outgoing buffers", 1, G_MAXINT,
          DEFAULT_MIN_SIZE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_STEP_SIZE,
      g_param_spec_int ("step-size", "step-size",
          "Step increment for random buffer sizes", 1, G_MAXINT,
          DEFAULT_MAX_SIZE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (element_class,
      &gst_chop_my_data_src_template);
  gst_element_class_add_static_pad_template (element_class,
      &gst_chop_my_data_sink_template);

  gst_element_class_set_static_metadata (element_class, "Chop my data",
      "Generic", "Split up a stream into randomly-sized buffers",
      "David Schleef <ds@schleef.org>");
}

static void
gst_chop_my_data_init (GstChopMyData * chopmydata)
{

  chopmydata->sinkpad =
      gst_pad_new_from_static_template (&gst_chop_my_data_sink_template,
      "sink");
  gst_pad_set_event_function (chopmydata->sinkpad,
      GST_DEBUG_FUNCPTR (gst_chop_my_data_sink_event));
  gst_pad_set_chain_function (chopmydata->sinkpad,
      GST_DEBUG_FUNCPTR (gst_chop_my_data_chain));
  GST_PAD_SET_PROXY_CAPS (chopmydata->sinkpad);
  gst_element_add_pad (GST_ELEMENT (chopmydata), chopmydata->sinkpad);

  chopmydata->srcpad =
      gst_pad_new_from_static_template (&gst_chop_my_data_src_template, "src");
  gst_pad_set_event_function (chopmydata->srcpad,
      GST_DEBUG_FUNCPTR (gst_chop_my_data_src_event));
  GST_PAD_SET_PROXY_CAPS (chopmydata->srcpad);
  gst_element_add_pad (GST_ELEMENT (chopmydata), chopmydata->srcpad);

  chopmydata->step_size = DEFAULT_STEP_SIZE;
  chopmydata->min_size = DEFAULT_MIN_SIZE;
  chopmydata->max_size = DEFAULT_MAX_SIZE;
}

void
gst_chop_my_data_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstChopMyData *chopmydata;

  g_return_if_fail (GST_IS_CHOP_MY_DATA (object));
  chopmydata = GST_CHOP_MY_DATA (object);

  switch (property_id) {
    case PROP_MAX_SIZE:
      chopmydata->max_size = g_value_get_int (value);
      break;
    case PROP_MIN_SIZE:
      chopmydata->min_size = g_value_get_int (value);
      break;
    case PROP_STEP_SIZE:
      chopmydata->step_size = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_chop_my_data_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstChopMyData *chopmydata;

  g_return_if_fail (GST_IS_CHOP_MY_DATA (object));
  chopmydata = GST_CHOP_MY_DATA (object);

  switch (property_id) {
    case PROP_MAX_SIZE:
      g_value_set_int (value, chopmydata->max_size);
      break;
    case PROP_MIN_SIZE:
      g_value_set_int (value, chopmydata->min_size);
      break;
    case PROP_STEP_SIZE:
      g_value_set_int (value, chopmydata->step_size);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static GstStateChangeReturn
gst_chop_my_data_change_state (GstElement * element, GstStateChange transition)
{
  GstChopMyData *chopmydata;
  GstStateChangeReturn ret;

  chopmydata = GST_CHOP_MY_DATA (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      GST_OBJECT_LOCK (chopmydata);
      chopmydata->adapter = gst_adapter_new ();
      chopmydata->rand = g_rand_new ();
      chopmydata->next_size = 0;
      GST_OBJECT_UNLOCK (chopmydata);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_OBJECT_LOCK (chopmydata);
      g_object_unref (chopmydata->adapter);
      chopmydata->adapter = NULL;
      g_rand_free (chopmydata->rand);
      GST_OBJECT_UNLOCK (chopmydata);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }

  return ret;
}

static void
get_next_size (GstChopMyData * chopmydata)
{
  int begin;
  int end;

  begin = (chopmydata->min_size + chopmydata->step_size - 1) /
      chopmydata->step_size;
  end = (chopmydata->max_size + chopmydata->step_size) / chopmydata->step_size;

  if (begin >= end) {
    chopmydata->next_size = begin * chopmydata->step_size;
    return;
  }

  chopmydata->next_size =
      g_rand_int_range (chopmydata->rand, begin, end) * chopmydata->step_size;
}

static GstFlowReturn
gst_chop_my_data_process (GstChopMyData * chopmydata, gboolean flush)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *buffer;

  if (chopmydata->next_size == 0) {
    get_next_size (chopmydata);
  }

  while (gst_adapter_available (chopmydata->adapter) >= chopmydata->next_size) {
    buffer =
        gst_adapter_take_buffer (chopmydata->adapter, chopmydata->next_size);

    GST_BUFFER_PTS (buffer) = gst_adapter_prev_pts (chopmydata->adapter, NULL);
    GST_BUFFER_DTS (buffer) = gst_adapter_prev_dts (chopmydata->adapter, NULL);

    chopmydata->next_size = 0;

    ret = gst_pad_push (chopmydata->srcpad, buffer);
    if (ret != GST_FLOW_OK) {
      return ret;
    }

    get_next_size (chopmydata);
  }

  if (flush) {
    guint min_size = chopmydata->min_size;

    while (gst_adapter_available (chopmydata->adapter) >= min_size) {
      buffer = gst_adapter_take_buffer (chopmydata->adapter, min_size);
      ret = gst_pad_push (chopmydata->srcpad, buffer);
      if (ret != GST_FLOW_OK)
        break;
    }
    gst_adapter_clear (chopmydata->adapter);
  }

  return ret;
}

static GstFlowReturn
gst_chop_my_data_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstChopMyData *chopmydata;
  GstFlowReturn ret;

  chopmydata = GST_CHOP_MY_DATA (parent);

  GST_DEBUG_OBJECT (chopmydata, "chain");

  gst_adapter_push (chopmydata->adapter, buffer);
  ret = gst_chop_my_data_process (chopmydata, FALSE);

  return ret;
}

static gboolean
gst_chop_my_data_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean res;
  GstChopMyData *chopmydata;

  chopmydata = GST_CHOP_MY_DATA (parent);

  GST_DEBUG_OBJECT (chopmydata, "event");

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
      res = gst_pad_push_event (chopmydata->srcpad, event);
      break;
    case GST_EVENT_FLUSH_STOP:
      gst_adapter_clear (chopmydata->adapter);
      res = gst_pad_push_event (chopmydata->srcpad, event);
      break;
    case GST_EVENT_SEGMENT:
      res = gst_pad_push_event (chopmydata->srcpad, event);
      break;
    case GST_EVENT_EOS:
      gst_chop_my_data_process (chopmydata, TRUE);
      res = gst_pad_push_event (chopmydata->srcpad, event);
      break;
    default:
      res = gst_pad_push_event (chopmydata->srcpad, event);
      break;
  }

  return res;
}

static gboolean
gst_chop_my_data_src_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean res;
  GstChopMyData *chopmydata;

  chopmydata = GST_CHOP_MY_DATA (parent);

  GST_DEBUG_OBJECT (chopmydata, "event");

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
      res = gst_pad_push_event (chopmydata->sinkpad, event);
      break;
    default:
      res = gst_pad_push_event (chopmydata->sinkpad, event);
      break;
  }

  return res;
}
