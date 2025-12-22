/* GStreamer chromaprint audio fingerprinting element
 * Copyright (C) 2006 M. Derezynski
 * Copyright (C) 2008 Eric Buehl
 * Copyright (C) 2008 Sebastian Dröge <slomo@circular-chaos.org>
 * Copyright (C) 2011 Lukáš Lalinský <lalinsky@gmail.com>
 * Copyright (C) 2012 Collabora Ltd. <tim.muller@collabora.co.uk>
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
 * SECTION:element-chromaprint
 * @title: chromaprint
 *
 * The chromaprint element calculates an acoustic fingerprint for an
 * audio stream which can be used to identify a song and look up
 * further metadata from the [Acoustid](http://acoustid.org/) and Musicbrainz
 * databases.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -m uridecodebin uri=file:///path/to/song.ogg ! audioconvert ! chromaprint ! fakesink
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstchromaprint.h"

#define DEFAULT_MAX_DURATION 120

#define PAD_CAPS \
	"audio/x-raw, " \
        "format = (string) " GST_AUDIO_NE(S16) ", "\
        "rate = (int) [ 1, MAX ], " \
        "channels = (int) [ 1, 2 ]"

GST_DEBUG_CATEGORY_STATIC (gst_chromaprint_debug);
#define GST_CAT_DEFAULT gst_chromaprint_debug

enum
{
  PROP_0,
  PROP_FINGERPRINT,
  PROP_MAX_DURATION
};

static gboolean chromaprint_element_init (GstPlugin * plugin);

#define parent_class gst_chromaprint_parent_class
G_DEFINE_TYPE (GstChromaprint, gst_chromaprint, GST_TYPE_AUDIO_FILTER);
GST_ELEMENT_REGISTER_DEFINE_CUSTOM (chromaprint, chromaprint_element_init);

static void gst_chromaprint_finalize (GObject * object);
static void gst_chromaprint_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_chromaprint_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static GstFlowReturn gst_chromaprint_transform_ip (GstBaseTransform * trans,
    GstBuffer * buf);
static gboolean gst_chromaprint_sink_event (GstBaseTransform * trans,
    GstEvent * event);

static void
gst_chromaprint_class_init (GstChromaprintClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseTransformClass *gstbasetrans_class;
  GstCaps *caps;

  gobject_class = G_OBJECT_CLASS (klass);
  gstbasetrans_class = GST_BASE_TRANSFORM_CLASS (klass);

  gobject_class->set_property = gst_chromaprint_set_property;
  gobject_class->get_property = gst_chromaprint_get_property;

  g_object_class_install_property (gobject_class, PROP_FINGERPRINT,
      g_param_spec_string ("fingerprint", "Resulting fingerprint",
          "Resulting fingerprint", NULL, G_PARAM_READABLE));

  g_object_class_install_property (gobject_class, PROP_MAX_DURATION,
      g_param_spec_uint ("duration", "Duration limit",
          "Number of seconds of audio to use for fingerprinting",
          0, G_MAXUINT, DEFAULT_MAX_DURATION,
          G_PARAM_READABLE | G_PARAM_WRITABLE));

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_chromaprint_finalize);

  gstbasetrans_class->transform_ip =
      GST_DEBUG_FUNCPTR (gst_chromaprint_transform_ip);
  gstbasetrans_class->sink_event =
      GST_DEBUG_FUNCPTR (gst_chromaprint_sink_event);
  gstbasetrans_class->passthrough_on_same_caps = TRUE;

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "Chromaprint fingerprinting element",
      "Filter/Analyzer/Audio",
      "Find an audio fingerprint using the Chromaprint library",
      "Lukáš Lalinský <lalinsky@gmail.com>");

  caps = gst_caps_from_string (PAD_CAPS);
  gst_audio_filter_class_add_pad_templates (GST_AUDIO_FILTER_CLASS (klass),
      caps);
  gst_caps_unref (caps);
}

static void
gst_chromaprint_reset (GstChromaprint * chromaprint)
{
  if (chromaprint->fingerprint) {
    chromaprint_dealloc (chromaprint->fingerprint);
    chromaprint->fingerprint = NULL;
  }

  chromaprint->nsamples = 0;
  chromaprint->duration = 0;
  chromaprint->record = TRUE;
}

static void
gst_chromaprint_create_fingerprint (GstChromaprint * chromaprint)
{
  GstTagList *tags;

  if (chromaprint->duration <= 3)
    return;

  GST_DEBUG_OBJECT (chromaprint,
      "Generating fingerprint based on %d seconds of audio",
      chromaprint->duration);

  chromaprint_finish (chromaprint->context);
  chromaprint_get_fingerprint (chromaprint->context, &chromaprint->fingerprint);
  chromaprint->record = FALSE;

  g_object_notify ((GObject *) chromaprint, "fingerprint");

  tags = gst_tag_list_new (GST_TAG_CHROMAPRINT_FINGERPRINT,
      chromaprint->fingerprint, NULL);

  gst_pad_push_event (GST_BASE_TRANSFORM_SRC_PAD (chromaprint),
      gst_event_new_tag (tags));
}

static void
gst_chromaprint_init (GstChromaprint * chromaprint)
{
  gst_base_transform_set_passthrough (GST_BASE_TRANSFORM (chromaprint), TRUE);

  chromaprint->context = chromaprint_new (CHROMAPRINT_ALGORITHM_DEFAULT);
  chromaprint->fingerprint = NULL;
  chromaprint->max_duration = DEFAULT_MAX_DURATION;
  gst_chromaprint_reset (chromaprint);
}

static void
gst_chromaprint_finalize (GObject * object)
{
  GstChromaprint *chromaprint = GST_CHROMAPRINT (object);

  chromaprint->record = FALSE;

  if (chromaprint->context) {
    chromaprint_free (chromaprint->context);
    chromaprint->context = NULL;
  }

  if (chromaprint->fingerprint) {
    chromaprint_dealloc (chromaprint->fingerprint);
    chromaprint->fingerprint = NULL;
  }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GstFlowReturn
gst_chromaprint_transform_ip (GstBaseTransform * trans, GstBuffer * buf)
{
  GstChromaprint *chromaprint = GST_CHROMAPRINT (trans);
  GstAudioFilter *filter = GST_AUDIO_FILTER (trans);
  GstMapInfo map_info;
  guint nsamples;
  gint rate, channels;

  rate = GST_AUDIO_INFO_RATE (&filter->info);
  channels = GST_AUDIO_INFO_CHANNELS (&filter->info);

  if (G_UNLIKELY (rate <= 0 || channels <= 0))
    return GST_FLOW_NOT_NEGOTIATED;

  if (!chromaprint->record)
    return GST_FLOW_OK;

  if (!gst_buffer_map (buf, &map_info, GST_MAP_READ))
    return GST_FLOW_ERROR;

  nsamples = map_info.size / (channels * 2);

  if (nsamples == 0)
    goto end;

  if (chromaprint->nsamples == 0) {
    chromaprint_start (chromaprint->context, rate, channels);
  }
  chromaprint->nsamples += nsamples;
  chromaprint->duration = chromaprint->nsamples / rate;

  chromaprint_feed (chromaprint->context, (gint16 *) map_info.data,
      map_info.size / sizeof (guint16));

  if (chromaprint->duration >= chromaprint->max_duration
      && !chromaprint->fingerprint) {
    gst_chromaprint_create_fingerprint (chromaprint);
  }

end:
  gst_buffer_unmap (buf, &map_info);

  return GST_FLOW_OK;
}

static gboolean
gst_chromaprint_sink_event (GstBaseTransform * trans, GstEvent * event)
{
  GstChromaprint *chromaprint = GST_CHROMAPRINT (trans);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_STOP:
    case GST_EVENT_SEGMENT:
      GST_DEBUG_OBJECT (trans, "Got %s event, clearing buffer",
          GST_EVENT_TYPE_NAME (event));
      gst_chromaprint_reset (chromaprint);
      break;
    case GST_EVENT_EOS:
      if (!chromaprint->fingerprint) {
        gst_chromaprint_create_fingerprint (chromaprint);
      }
      break;
    default:
      break;
  }

  return GST_BASE_TRANSFORM_CLASS (parent_class)->sink_event (trans, event);
}

static void
gst_chromaprint_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstChromaprint *chromaprint = GST_CHROMAPRINT (object);

  switch (prop_id) {
    case PROP_MAX_DURATION:
      chromaprint->max_duration = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_chromaprint_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstChromaprint *chromaprint = GST_CHROMAPRINT (object);

  switch (prop_id) {
    case PROP_FINGERPRINT:
      g_value_set_string (value, chromaprint->fingerprint);
      break;
    case PROP_MAX_DURATION:
      g_value_set_uint (value, chromaprint->max_duration);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
chromaprint_element_init (GstPlugin * plugin)
{
  gboolean ret;

  GST_DEBUG_CATEGORY_INIT (gst_chromaprint_debug, "chromaprint",
      0, "chromaprint element");

  GST_INFO ("libchromaprint %s", chromaprint_get_version ());

  ret = gst_element_register (plugin, "chromaprint", GST_RANK_NONE,
      GST_TYPE_CHROMAPRINT);

  if (ret) {
    gst_tag_register (GST_TAG_CHROMAPRINT_FINGERPRINT, GST_TAG_FLAG_META,
        G_TYPE_STRING, "chromaprint fingerprint", "Chromaprint fingerprint",
        NULL);
  }

  return ret;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (chromaprint, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    chromaprint,
    "Calculate Chromaprint fingerprint from audio files",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
