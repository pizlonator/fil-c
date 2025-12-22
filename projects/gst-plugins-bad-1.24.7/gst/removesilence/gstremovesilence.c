/* GStreamer
 * Copyright (C) 2011 Tiago Katcipis <tiagokatcipis@gmail.com>
 * Copyright (C) 2011 Paulo Pizarro  <paulo.pizarro@gmail.com>
 * Copyright (C) 2012-2016 Nicola Murino  <nicola.murino@gmail.com>
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
 * SECTION:element-removesilence
 * @title: removesilence
 *
 * Removes all silence periods from an audio stream, dropping silence buffers.
 * If the "silent" property is disabled, removesilence will generate
 * bus messages named "removesilence". 
 * The message's structure contains one of these fields:
 * 
 * - #guint64 "silence_detected": the PTS for the first silent buffer after a non silence period.
 *    
 * - #guint64 "silence_finished": the PTS for the first non silent buffer after a silence period.
 *   
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v -m filesrc location="audiofile" ! decodebin ! removesilence remove=true ! wavenc ! filesink location=without_audio.wav
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/audio/audio.h>

#include "gstremovesilence.h"


GST_DEBUG_CATEGORY_STATIC (gst_remove_silence_debug);
#define GST_CAT_DEFAULT gst_remove_silence_debug
#define DEFAULT_VAD_HYSTERESIS  480     /* 60 mseg */
#define MINIMUM_SILENCE_BUFFERS_MIN  0
#define MINIMUM_SILENCE_BUFFERS_MAX  10000
#define MINIMUM_SILENCE_BUFFERS_DEF  0
#define MINIMUM_SILENCE_TIME_MIN  0
#define MINIMUM_SILENCE_TIME_MAX  10000000000
#define MINIMUM_SILENCE_TIME_DEF  0
#define DEFAULT_VAD_THRESHOLD -60

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_REMOVE,
  PROP_HYSTERESIS,
  PROP_THRESHOLD,
  PROP_SQUASH,
  PROP_SILENT,
  PROP_MINIMUM_SILENCE_BUFFERS,
  PROP_MINIMUM_SILENCE_TIME
};


static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16) ", "
        "layout = (string) interleaved, "
        "rate = (int) [ 1, MAX ], " "channels = (int) 1"));

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16) ", "
        "layout = (string) interleaved, "
        "rate = (int) [ 1, MAX ], " "channels = (int) 1"));


#define DEBUG_INIT(bla) \
  GST_DEBUG_CATEGORY_INIT (gst_remove_silence_debug, "removesilence", 0, "removesilence element")

#define gst_remove_silence_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstRemoveSilence, gst_remove_silence,
    GST_TYPE_BASE_TRANSFORM, DEBUG_INIT (0));
GST_ELEMENT_REGISTER_DEFINE (removesilence, "removesilence", GST_RANK_NONE,
    gst_remove_silence_get_type ());

static void gst_remove_silence_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_remove_silence_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_remove_silence_start (GstBaseTransform * trans);
static gboolean gst_remove_silence_sink_event (GstBaseTransform * trans,
    GstEvent * event);
static GstFlowReturn gst_remove_silence_transform_ip (GstBaseTransform * base,
    GstBuffer * buf);
static void gst_remove_silence_finalize (GObject * obj);

/* GObject vmethod implementations */

/* initialize the removesilence's class */
static void
gst_remove_silence_class_init (GstRemoveSilenceClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseTransformClass *base_transform_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  base_transform_class = GST_BASE_TRANSFORM_CLASS (klass);

  gobject_class->finalize = gst_remove_silence_finalize;
  gobject_class->set_property = gst_remove_silence_set_property;
  gobject_class->get_property = gst_remove_silence_get_property;

  g_object_class_install_property (gobject_class, PROP_REMOVE,
      g_param_spec_boolean ("remove", "Remove",
          "Set to true to remove silence from the stream, false otherwise",
          FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_HYSTERESIS,
      g_param_spec_uint64 ("hysteresis",
          "Hysteresis",
          "Set the hysteresis (on samples) used on the internal VAD",
          1, G_MAXUINT64, DEFAULT_VAD_HYSTERESIS, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_THRESHOLD,
      g_param_spec_int ("threshold",
          "Threshold",
          "Set the silence threshold used on the internal VAD in dB",
          -70, 70, DEFAULT_VAD_THRESHOLD, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_SQUASH,
      g_param_spec_boolean ("squash", "Squash",
          "Set to true to retimestamp buffers when silence is removed and so avoid timestamp gap",
          FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent",
          "Disable/enable bus message notifications for silence detected/finished",
          TRUE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_MINIMUM_SILENCE_BUFFERS,
      g_param_spec_uint ("minimum-silence-buffers", "Minimum silence buffers",
          "Define the minimum number of consecutive silence buffers before "
          "removing silence, 0 means disabled. This will not introduce latency",
          MINIMUM_SILENCE_BUFFERS_MIN, MINIMUM_SILENCE_BUFFERS_MAX,
          MINIMUM_SILENCE_BUFFERS_DEF, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_MINIMUM_SILENCE_TIME,
      g_param_spec_uint64 ("minimum_silence_time",
          "Minimum silence time",
          "Define the minimum silence time in nanoseconds before removing "
          " silence, 0 means disabled. This will not introduce latency",
          MINIMUM_SILENCE_TIME_MIN, MINIMUM_SILENCE_TIME_MAX,
          MINIMUM_SILENCE_TIME_DEF, G_PARAM_READWRITE));

  gst_element_class_set_static_metadata (gstelement_class,
      "RemoveSilence",
      "Filter/Effect/Audio",
      "Removes all the silence periods from the audio stream.",
      "Tiago Katcipis <tiagokatcipis@gmail.com>, "
      "Paulo Pizarro  <paulo.pizarro@gmail.com>, "
      "Nicola Murino  <nicola.murino@gmail.com>");

  gst_element_class_add_static_pad_template (gstelement_class, &src_template);
  gst_element_class_add_static_pad_template (gstelement_class, &sink_template);

  base_transform_class->start = GST_DEBUG_FUNCPTR (gst_remove_silence_start);
  base_transform_class->sink_event =
      GST_DEBUG_FUNCPTR (gst_remove_silence_sink_event);
  base_transform_class->transform_ip =
      GST_DEBUG_FUNCPTR (gst_remove_silence_transform_ip);
}

static void
gst_remove_silence_reset (GstRemoveSilence * filter)
{
  filter->ts_offset = 0;
  filter->silence_detected = FALSE;
  filter->consecutive_silence_buffers = 0;
  filter->consecutive_silence_time = 0;
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad callback functions
 * initialize instance structure
 */
static void
gst_remove_silence_init (GstRemoveSilence * filter)
{
  filter->vad = vad_new (DEFAULT_VAD_HYSTERESIS, DEFAULT_VAD_THRESHOLD);
  filter->remove = FALSE;
  filter->squash = FALSE;
  filter->silent = TRUE;
  filter->minimum_silence_buffers = MINIMUM_SILENCE_BUFFERS_DEF;
  filter->minimum_silence_time = MINIMUM_SILENCE_TIME_DEF;

  gst_remove_silence_reset (filter);

  if (!filter->vad) {
    GST_DEBUG ("Error initializing VAD !!");
    return;
  }
}

static gboolean
gst_remove_silence_start (GstBaseTransform * trans)
{
  GstRemoveSilence *filter = GST_REMOVE_SILENCE (trans);

  GST_INFO ("reset filter on start");
  gst_remove_silence_reset (filter);

  return TRUE;
}

static gboolean
gst_remove_silence_sink_event (GstBaseTransform * trans, GstEvent * event)
{
  GstRemoveSilence *filter = GST_REMOVE_SILENCE (trans);

  if (event->type == GST_EVENT_SEGMENT) {
    GST_INFO ("reset filter on segment event");
    gst_remove_silence_reset (filter);
  }

  return
      GST_BASE_TRANSFORM_CLASS (gst_remove_silence_parent_class)->sink_event
      (trans, event);
}

static void
gst_remove_silence_finalize (GObject * obj)
{
  GstRemoveSilence *filter = GST_REMOVE_SILENCE (obj);
  GST_DEBUG ("Destroying VAD");
  vad_destroy (filter->vad);
  filter->vad = NULL;
  GST_DEBUG ("VAD Destroyed");
  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
gst_remove_silence_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRemoveSilence *filter = GST_REMOVE_SILENCE (object);

  switch (prop_id) {
    case PROP_REMOVE:
      filter->remove = g_value_get_boolean (value);
      break;
    case PROP_HYSTERESIS:
      vad_set_hysteresis (filter->vad, g_value_get_uint64 (value));
      break;
    case PROP_THRESHOLD:
      vad_set_threshold (filter->vad, g_value_get_int (value));
      break;
    case PROP_SQUASH:
      filter->squash = g_value_get_boolean (value);
      break;
    case PROP_SILENT:
      filter->silent = g_value_get_boolean (value);
      break;
    case PROP_MINIMUM_SILENCE_BUFFERS:
      filter->minimum_silence_buffers = g_value_get_uint (value);
      break;
    case PROP_MINIMUM_SILENCE_TIME:
      filter->minimum_silence_time = g_value_get_uint64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_remove_silence_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstRemoveSilence *filter = GST_REMOVE_SILENCE (object);

  switch (prop_id) {
    case PROP_REMOVE:
      g_value_set_boolean (value, filter->remove);
      break;
    case PROP_HYSTERESIS:
      g_value_set_uint64 (value, vad_get_hysteresis (filter->vad));
      break;
    case PROP_THRESHOLD:
      g_value_set_int (value, vad_get_threshold_as_db (filter->vad));
      break;
    case PROP_SQUASH:
      g_value_set_boolean (value, filter->squash);
      break;
    case PROP_SILENT:
      g_value_set_boolean (value, filter->silent);
      break;
    case PROP_MINIMUM_SILENCE_BUFFERS:
      g_value_set_uint (value, filter->minimum_silence_buffers);
      break;
    case PROP_MINIMUM_SILENCE_TIME:
      g_value_set_uint64 (value, filter->minimum_silence_time);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstFlowReturn
gst_remove_silence_transform_ip (GstBaseTransform * trans, GstBuffer * inbuf)
{
  GstRemoveSilence *filter = NULL;
  int frame_type;
  GstMapInfo map;
  gboolean consecutive_silence_reached;

  filter = GST_REMOVE_SILENCE (trans);

  gst_buffer_map (inbuf, &map, GST_MAP_READ);
  frame_type =
      vad_update (filter->vad, (gint16 *) map.data, map.size / sizeof (gint16));
  gst_buffer_unmap (inbuf, &map);

  if (frame_type == VAD_SILENCE) {
    GST_DEBUG ("Silence detected");
    filter->consecutive_silence_buffers++;
    if (GST_BUFFER_DURATION_IS_VALID (inbuf)) {
      filter->consecutive_silence_time += inbuf->duration;
    } else {
      GST_WARNING
          ("Invalid buffer duration, consecutive_silence_time update not possible");
    }
    if (filter->minimum_silence_buffers == 0
        && filter->minimum_silence_time == 0) {
      consecutive_silence_reached = TRUE;
    } else {
      consecutive_silence_reached =
          (filter->minimum_silence_buffers > 0
          && filter->consecutive_silence_buffers >=
          filter->minimum_silence_buffers)
          || (filter->minimum_silence_time > 0
          && filter->consecutive_silence_time >= filter->minimum_silence_time);
    }
    if (!filter->silence_detected && consecutive_silence_reached) {
      if (!filter->silent) {
        if (GST_BUFFER_PTS_IS_VALID (inbuf)) {
          GstStructure *s;
          GstMessage *m;
          s = gst_structure_new ("removesilence", "silence_detected",
              G_TYPE_UINT64, GST_BUFFER_PTS (inbuf) - filter->ts_offset, NULL);
          m = gst_message_new_element (GST_OBJECT (filter), s);
          gst_element_post_message (GST_ELEMENT (filter), m);
        }
      }
      filter->silence_detected = TRUE;
    }

    if (filter->remove && consecutive_silence_reached) {
      GST_DEBUG ("Removing silence");
      if (filter->squash) {
        if (GST_BUFFER_DURATION_IS_VALID (inbuf)) {
          filter->ts_offset += inbuf->duration;
        } else {
          GST_WARNING ("Invalid buffer duration: ts_offset not updated");
        }
      }
      return GST_BASE_TRANSFORM_FLOW_DROPPED;
    }

  } else {
    filter->consecutive_silence_buffers = 0;
    filter->consecutive_silence_time = 0;
    if (filter->silence_detected) {
      if (!filter->silent) {
        if (GST_BUFFER_PTS_IS_VALID (inbuf)) {
          GstStructure *s;
          GstMessage *m;
          s = gst_structure_new ("removesilence", "silence_finished",
              G_TYPE_UINT64, GST_BUFFER_PTS (inbuf) - filter->ts_offset, NULL);
          m = gst_message_new_element (GST_OBJECT (filter), s);
          gst_element_post_message (GST_ELEMENT (filter), m);
        }
      }
      filter->silence_detected = FALSE;
    }
  }

  if (filter->squash && filter->ts_offset > 0) {
    if (GST_BUFFER_PTS_IS_VALID (inbuf)) {
      inbuf = gst_buffer_make_writable (inbuf);
      GST_BUFFER_PTS (inbuf) -= filter->ts_offset;
    } else {
      GST_WARNING ("Invalid buffer pts, update not possible");
    }
  }

  return GST_FLOW_OK;
}

/*Plugin init functions*/
static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (removesilence, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    removesilence,
    "Removes silence from an audio stream",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
