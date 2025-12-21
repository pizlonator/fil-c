/* GStreamer encoding bin
 * Copyright (C) 2009 Edward Hervey <edward.hervey@collabora.co.uk>
 *           (C) 2009 Nokia Corporation
 *           (C) 2016 Jan Schmidt <jan@centricular.com>
 *           (C) 2020 Thibault saunier <tsaunier@igalia.com>
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
#include "config.h"
#endif

#include <string.h>
#include "gstencodebin.h"
#include "gstsmartencoder.h"
#include "gststreamsplitter.h"
#include "gststreamcombiner.h"
#include <glib/gi18n-lib.h>

/**
 * SECTION:element-encodebin
 * @title: encodebin
 *
 * EncodeBin provides a bin for encoding/muxing various streams according to
 * a specified #GstEncodingProfile.
 *
 * Based on the profile that was set (via the #GstEncodeBaseBin:profile property),
 * EncodeBin will internally select and configure the required elements
 * (encoders, muxers, but also audio and video converters) so that you can
 * provide it raw or pre-encoded streams of data in input and have your
 * encoded/muxed/converted stream in output.
 *
 * ## Features
 *
 * * Automatic encoder and muxer selection based on elements available on the
 * system.
 *
 * * Conversion of raw audio/video streams (scaling, framerate conversion,
 * colorspace conversion, samplerate conversion) to conform to the profile
 * output format.
 *
 * * Variable number of streams. If the presence property for a stream encoding
 * profile is 0, you can request any number of sink pads for it via the
 * standard request pad gstreamer API or the #GstEncodeBaseBin::request-pad action
 * signal.
 *
 * * Avoid reencoding (passthrough). If the input stream is already encoded and is
 * compatible with what the #GstEncodingProfile expects, then the stream won't
 * be re-encoded but just passed through downstream to the muxer or the output.
 *
 * * Mix pre-encoded and raw streams as input. In addition to the passthrough
 * feature above, you can feed both raw audio/video *AND* already-encoded data
 * to a pad. #GstEncodeBaseBin will take care of passing through the compatible
 * segments and re-encoding the segments of media that need encoding.
 *
 * * Standard behaviour is to use a #GstEncodingContainerProfile to have both
 * encoding and muxing performed. But you can also provide a single stream
 * profile (like #GstEncodingAudioProfile) to only have the encoding done and
 * handle the encoded output yourself.
 *
 * * Audio imperfection corrections. Incoming audio streams can have non perfect
 * timestamps (jitter), like the streams coming from ASF files. #GstEncodeBaseBin
 * will automatically fix those imperfections for you. See
 * #GstEncodeBaseBin:audio-jitter-tolerance for more details.
 *
 * * Variable or Constant video framerate. If your #GstEncodingVideoProfile has
 * the variableframerate property deactivated (default), then the incoming
 * raw video stream will be retimestampped in order to produce a constant
 * framerate.
 *
 * * Cross-boundary re-encoding. When feeding compatible pre-encoded streams that
 * fall on segment boundaries, and for supported formats (right now only H263),
 * the GOP will be decoded/reencoded when needed to produce an encoded output
 * that fits exactly within the request GstSegment.
 *
 * * Missing plugin support. If a #GstElement is missing to encode/mux to the
 * request profile formats, a missing-plugin #GstMessage will be posted on the
 * #GstBus, allowing systems that support the missing-plugin system to offer the
 * user a way to install the missing element.
 *
 */


/* TODO/FIXME
 *
 * Handling mp3!xing!idv3 and theora!ogg tagsetting scenarios:
 *  Once we have chosen a muxer:
 *   When a new stream is requested:
 *    If muxer isn't 'Formatter' OR doesn't have a TagSetter interface:
 *      Find a Formatter for the given stream (preferably with TagSetter)
 *       Insert that before muxer
 **/

#define fast_pad_link(a,b) gst_pad_link_full((a),(b),GST_PAD_LINK_CHECK_NOTHING)
#define fast_element_link(a,b) gst_element_link_pads_full((a),"src",(b),"sink",GST_PAD_LINK_CHECK_NOTHING)

#define GST_TYPE_ENCODEBIN_FLAGS (gst_encodebin_flags_get_type())
GType gst_encodebin_flags_get_type (void);

/* generic templates */
static GstStaticPadTemplate video_sink_template =
GST_STATIC_PAD_TEMPLATE ("video_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS_ANY);
static GstStaticPadTemplate audio_sink_template =
GST_STATIC_PAD_TEMPLATE ("audio_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS_ANY);
/* static GstStaticPadTemplate text_sink_template = */
/* GST_STATIC_PAD_TEMPLATE ("text_%u", */
/*     GST_PAD_SINK, */
/*     GST_PAD_REQUEST, */
/*     GST_STATIC_CAPS_ANY); */
static GstStaticPadTemplate private_sink_template =
GST_STATIC_PAD_TEMPLATE ("private_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS_ANY);

typedef struct _StreamGroup StreamGroup;

struct _StreamGroup
{
  GstEncodeBaseBin *ebin;
  GstEncodingProfile *profile;
  GstPad *ghostpad;             /* Sink ghostpad */
  GstElement *identity;         /* Identity just after the ghostpad */
  GstElement *inqueue;          /* Queue just after the identity */
  GstElement *splitter;
  GList *converters;            /* List of conversion GstElement */
  GstElement *capsfilter;       /* profile->restriction (if non-NULL/ANY) */
  gulong inputfilter_caps_sid;
  GstElement *encoder;          /* Encoder (can be NULL) */
  GstElement *fakesink;         /* Fakesink (can be NULL) */
  GstElement *combiner;
  GstElement *parser;
  GstElement *timestamper;
  GstElement *smartencoder;
  GstElement *smart_capsfilter;
  gulong smart_capsfilter_sid;
  GstElement *outfilter;        /* Output capsfilter (streamprofile.format) */
  gulong outputfilter_caps_sid;
  GstElement *formatter;
  GstElement *outqueue;         /* Queue just before the muxer */
  gulong restriction_sid;
};

/* Default for queues (same defaults as queue element) */
#define DEFAULT_QUEUE_BUFFERS_MAX  200
#define DEFAULT_QUEUE_BYTES_MAX    10 * 1024 * 1024
#define DEFAULT_QUEUE_TIME_MAX     GST_SECOND
#define DEFAULT_AUDIO_JITTER_TOLERANCE 20 * GST_MSECOND
#define DEFAULT_AVOID_REENCODING   FALSE
#define DEFAULT_FLAGS              0

#define DEFAULT_RAW_CAPS			\
  "video/x-raw; "				\
  "audio/x-raw; "				\
  "text/x-raw; "				\
  "subpicture/x-dvd; "			\
  "subpicture/x-pgs"

/* Properties */
enum
{
  PROP_0,
  PROP_PROFILE,
  PROP_QUEUE_BUFFERS_MAX,
  PROP_QUEUE_BYTES_MAX,
  PROP_QUEUE_TIME_MAX,
  PROP_AUDIO_JITTER_TOLERANCE,
  PROP_AVOID_REENCODING,
  PROP_FLAGS
};

/* Signals */
enum
{
  SIGNAL_REQUEST_PAD,
  SIGNAL_REQUEST_PROFILE_PAD,
  LAST_SIGNAL
};

#define C_FLAGS(v) ((guint) v)

GType
gst_encodebin_flags_get_type (void)
{
  static const GFlagsValue values[] = {
    {C_FLAGS (GST_ENCODEBIN_FLAG_NO_AUDIO_CONVERSION), "Do not use audio "
          "conversion elements", "no-audio-conversion"},
    {C_FLAGS (GST_ENCODEBIN_FLAG_NO_VIDEO_CONVERSION), "Do not use video "
          "conversion elements", "no-video-conversion"},
    {0, NULL, NULL}
  };
  static GType id = 0;

  if (g_once_init_enter ((gsize *) & id)) {
    GType _id;

    _id = g_flags_register_static ("GstEncodeBinFlags", values);

    g_once_init_leave ((gsize *) & id, _id);
  }

  return id;
}

static guint gst_encode_base_bin_signals[LAST_SIGNAL] = { 0 };

static GstStaticCaps default_raw_caps = GST_STATIC_CAPS (DEFAULT_RAW_CAPS);

GST_DEBUG_CATEGORY_STATIC (gst_encode_base_bin_debug);
#define GST_CAT_DEFAULT gst_encode_base_bin_debug

G_DEFINE_TYPE (GstEncodeBaseBin, gst_encode_base_bin, GST_TYPE_BIN);

static void gst_encode_base_bin_dispose (GObject * object);
static void gst_encode_base_bin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_encode_base_bin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static GstStateChangeReturn gst_encode_base_bin_change_state (GstElement *
    element, GstStateChange transition);

static GstPad *gst_encode_base_bin_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps);
static void gst_encode_base_bin_release_pad (GstElement * element,
    GstPad * pad);

static gboolean
gst_encode_base_bin_set_profile (GstEncodeBaseBin * ebin,
    GstEncodingProfile * profile);
static void gst_encode_base_bin_tear_down_profile (GstEncodeBaseBin * ebin);
static gboolean gst_encode_base_bin_setup_profile (GstEncodeBaseBin * ebin,
    GstEncodingProfile * profile);

static StreamGroup *_create_stream_group (GstEncodeBaseBin * ebin,
    GstEncodingProfile * sprof, const gchar * sinkpadname, GstCaps * sinkcaps,
    gboolean * encoder_not_found);
static void stream_group_remove (GstEncodeBaseBin * ebin, StreamGroup * sgroup);
static void stream_group_free (GstEncodeBaseBin * ebin, StreamGroup * sgroup);
static GstPad *gst_encode_base_bin_request_pad_signal (GstEncodeBaseBin *
    encodebin, GstCaps * caps);
static GstPad *gst_encode_base_bin_request_profile_pad_signal (GstEncodeBaseBin
    * encodebin, const gchar * profilename);

static inline GstElement *_get_formatter (GstEncodeBaseBin * ebin,
    GstEncodingProfile * sprof);
static void _post_missing_plugin_message (GstEncodeBaseBin * ebin,
    GstEncodingProfile * prof);

static void
gst_encode_base_bin_class_init (GstEncodeBaseBinClass * klass)
{
  GObjectClass *gobject_klass;
  GstElementClass *gstelement_klass;

  gobject_klass = (GObjectClass *) klass;
  gstelement_klass = (GstElementClass *) klass;

  GST_DEBUG_CATEGORY_INIT (gst_encode_base_bin_debug, "encodebasebin", 0,
      "base encodebin");
  gobject_klass->dispose = gst_encode_base_bin_dispose;
  gobject_klass->set_property = gst_encode_base_bin_set_property;
  gobject_klass->get_property = gst_encode_base_bin_get_property;

  /* Properties */

  /**
   * GstEncodeBaseBin:profile:
   *
   * The #GstEncodingProfile to use. This property must be set before going
   * to %GST_STATE_PAUSED or higher.
   */
  g_object_class_install_property (gobject_klass, PROP_PROFILE,
      g_param_spec_object ("profile", "Profile",
          "The GstEncodingProfile to use", GST_TYPE_ENCODING_PROFILE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_klass, PROP_QUEUE_BYTES_MAX,
      g_param_spec_uint ("queue-bytes-max", "Max. size (kB)",
          "Max. amount of data in the queue (bytes, 0=disable)",
          0, G_MAXUINT, DEFAULT_QUEUE_BYTES_MAX,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_klass, PROP_QUEUE_BUFFERS_MAX,
      g_param_spec_uint ("queue-buffers-max", "Max. size (buffers)",
          "Max. number of buffers in the queue (0=disable)", 0, G_MAXUINT,
          DEFAULT_QUEUE_BUFFERS_MAX,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_klass, PROP_QUEUE_TIME_MAX,
      g_param_spec_uint64 ("queue-time-max", "Max. size (ns)",
          "Max. amount of data in the queue (in ns, 0=disable)", 0, G_MAXUINT64,
          DEFAULT_QUEUE_TIME_MAX, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_klass, PROP_AUDIO_JITTER_TOLERANCE,
      g_param_spec_uint64 ("audio-jitter-tolerance", "Audio jitter tolerance",
          "Amount of timestamp jitter/imperfection to allow on audio streams before inserting/dropping samples (ns)",
          0, G_MAXUINT64, DEFAULT_AUDIO_JITTER_TOLERANCE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_klass, PROP_AVOID_REENCODING,
      g_param_spec_boolean ("avoid-reencoding", "Avoid re-encoding",
          "Whether to re-encode portions of compatible video streams that lay on segment boundaries",
          DEFAULT_AVOID_REENCODING,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstEncodeBaseBin:flags
   *
   * Control the behaviour of encodebin.
   */
  g_object_class_install_property (gobject_klass, PROP_FLAGS,
      g_param_spec_flags ("flags", "Flags", "Flags to control behaviour",
          GST_TYPE_ENCODEBIN_FLAGS, DEFAULT_FLAGS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* Signals */
  /**
   * GstEncodeBaseBin::request-pad
   * @encodebin: a #GstEncodeBaseBin instance
   * @caps: a #GstCaps
   *
   * Use this method to request an unused sink request #GstPad that can take the
   * provided @caps as input. You must release the pad with
   * gst_element_release_request_pad() when you are done with it.
   *
   * Returns: A compatible #GstPad, or %NULL if no compatible #GstPad could be
   * created or is available.
   */
  gst_encode_base_bin_signals[SIGNAL_REQUEST_PAD] =
      g_signal_new ("request-pad", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (GstEncodeBaseBinClass, request_pad), NULL, NULL, NULL,
      GST_TYPE_PAD, 1, GST_TYPE_CAPS);

  /**
   * GstEncodeBaseBin::request-profile-pad
   * @encodebin: a #GstEncodeBaseBin instance
   * @profilename: the name of a #GstEncodingProfile
   *
   * Use this method to request an unused sink request #GstPad from the profile
   * @profilename. You must release the pad with
   * gst_element_release_request_pad() when you are done with it.
   *
   * Returns: A compatible #GstPad, or %NULL if no compatible #GstPad could be
   * created or is available.
   */
  gst_encode_base_bin_signals[SIGNAL_REQUEST_PROFILE_PAD] =
      g_signal_new ("request-profile-pad", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (GstEncodeBaseBinClass, request_profile_pad), NULL, NULL,
      NULL, GST_TYPE_PAD, 1, G_TYPE_STRING);

  klass->request_pad = gst_encode_base_bin_request_pad_signal;
  klass->request_profile_pad = gst_encode_base_bin_request_profile_pad_signal;

  gst_element_class_add_static_pad_template (gstelement_klass,
      &video_sink_template);
  gst_element_class_add_static_pad_template (gstelement_klass,
      &audio_sink_template);
  /* gst_element_class_add_static_pad_template (gstelement_klass, &text_sink_template); */
  gst_element_class_add_static_pad_template (gstelement_klass,
      &private_sink_template);

  gstelement_klass->change_state =
      GST_DEBUG_FUNCPTR (gst_encode_base_bin_change_state);
  gstelement_klass->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_encode_base_bin_request_new_pad);
  gstelement_klass->release_pad =
      GST_DEBUG_FUNCPTR (gst_encode_base_bin_release_pad);

  gst_element_class_set_static_metadata (gstelement_klass,
      "Encoder Bin",
      "Generic/Bin/Encoder",
      "Convenience encoding/muxing element",
      "Edward Hervey <edward.hervey@collabora.co.uk>");

  gst_type_mark_as_plugin_api (GST_TYPE_ENCODEBIN_FLAGS, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_ENCODE_BASE_BIN, 0);
}

static void
gst_encode_base_bin_dispose (GObject * object)
{
  GstEncodeBaseBin *ebin = (GstEncodeBaseBin *) object;

  if (ebin->muxers)
    gst_plugin_feature_list_free (ebin->muxers);
  ebin->muxers = NULL;

  if (ebin->formatters)
    gst_plugin_feature_list_free (ebin->formatters);
  ebin->formatters = NULL;

  if (ebin->encoders)
    gst_plugin_feature_list_free (ebin->encoders);
  ebin->encoders = NULL;

  if (ebin->parsers)
    gst_plugin_feature_list_free (ebin->parsers);
  ebin->parsers = NULL;

  if (ebin->timestampers)
    gst_plugin_feature_list_free (ebin->timestampers);
  ebin->timestampers = NULL;

  gst_encode_base_bin_tear_down_profile (ebin);

  if (ebin->raw_video_caps)
    gst_caps_unref (ebin->raw_video_caps);
  ebin->raw_video_caps = NULL;
  if (ebin->raw_audio_caps)
    gst_caps_unref (ebin->raw_audio_caps);
  ebin->raw_audio_caps = NULL;
  /* if (ebin->raw_text_caps) */
  /*   gst_caps_unref (ebin->raw_text_caps); */

  G_OBJECT_CLASS (gst_encode_base_bin_parent_class)->dispose (object);
}

static void
gst_encode_base_bin_init (GstEncodeBaseBin * encode_bin)
{
  encode_bin->muxers =
      gst_element_factory_list_get_elements (GST_ELEMENT_FACTORY_TYPE_MUXER,
      GST_RANK_MARGINAL);

  encode_bin->formatters =
      gst_element_factory_list_get_elements (GST_ELEMENT_FACTORY_TYPE_FORMATTER,
      GST_RANK_SECONDARY);

  encode_bin->encoders =
      gst_element_factory_list_get_elements (GST_ELEMENT_FACTORY_TYPE_ENCODER,
      GST_RANK_MARGINAL);

  encode_bin->parsers =
      gst_element_factory_list_get_elements (GST_ELEMENT_FACTORY_TYPE_PARSER,
      GST_RANK_MARGINAL);

  encode_bin->timestampers =
      gst_element_factory_list_get_elements
      (GST_ELEMENT_FACTORY_TYPE_TIMESTAMPER, GST_RANK_MARGINAL);

  encode_bin->raw_video_caps = gst_caps_from_string ("video/x-raw");
  encode_bin->raw_audio_caps = gst_caps_from_string ("audio/x-raw");
  /* encode_bin->raw_text_caps = */
  /*     gst_caps_from_string ("text/x-raw"); */

  encode_bin->queue_buffers_max = DEFAULT_QUEUE_BUFFERS_MAX;
  encode_bin->queue_bytes_max = DEFAULT_QUEUE_BYTES_MAX;
  encode_bin->queue_time_max = DEFAULT_QUEUE_TIME_MAX;
  encode_bin->tolerance = DEFAULT_AUDIO_JITTER_TOLERANCE;
  encode_bin->avoid_reencoding = DEFAULT_AVOID_REENCODING;
  encode_bin->flags = DEFAULT_FLAGS;
}

static void
gst_encode_base_bin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstEncodeBaseBin *ebin = (GstEncodeBaseBin *) object;

  switch (prop_id) {
    case PROP_PROFILE:
      gst_encode_base_bin_set_profile (ebin,
          (GstEncodingProfile *) g_value_get_object (value));
      break;
    case PROP_QUEUE_BUFFERS_MAX:
      ebin->queue_buffers_max = g_value_get_uint (value);
      break;
    case PROP_QUEUE_BYTES_MAX:
      ebin->queue_bytes_max = g_value_get_uint (value);
      break;
    case PROP_QUEUE_TIME_MAX:
      ebin->queue_time_max = g_value_get_uint64 (value);
      break;
    case PROP_AUDIO_JITTER_TOLERANCE:
      ebin->tolerance = g_value_get_uint64 (value);
      break;
    case PROP_AVOID_REENCODING:
    {
      gboolean avoided_reencoding = ebin->avoid_reencoding;
      ebin->avoid_reencoding = g_value_get_boolean (value);
      if (ebin->avoid_reencoding != avoided_reencoding && ebin->profile)
        gst_encode_base_bin_set_profile (ebin, gst_object_ref (ebin->profile));

      break;
    }
    case PROP_FLAGS:
      ebin->flags = g_value_get_flags (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_encode_base_bin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstEncodeBaseBin *ebin = (GstEncodeBaseBin *) object;

  switch (prop_id) {
    case PROP_PROFILE:
      g_value_set_object (value, (GObject *) ebin->profile);
      break;
    case PROP_QUEUE_BUFFERS_MAX:
      g_value_set_uint (value, ebin->queue_buffers_max);
      break;
    case PROP_QUEUE_BYTES_MAX:
      g_value_set_uint (value, ebin->queue_bytes_max);
      break;
    case PROP_QUEUE_TIME_MAX:
      g_value_set_uint64 (value, ebin->queue_time_max);
      break;
    case PROP_AUDIO_JITTER_TOLERANCE:
      g_value_set_uint64 (value, ebin->tolerance);
      break;
    case PROP_AVOID_REENCODING:
      g_value_set_boolean (value, ebin->avoid_reencoding);
      break;
    case PROP_FLAGS:
      g_value_set_flags (value, ebin->flags);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static inline gboolean
are_raw_caps (const GstCaps * caps)
{
  GstCaps *raw = gst_static_caps_get (&default_raw_caps);
  gboolean res = gst_caps_can_intersect (caps, raw);

  gst_caps_unref (raw);
  return res;
}

/* Returns the number of time a given stream profile is currently used
 * in encodebin */
static inline guint
stream_profile_used_count (GstEncodeBaseBin * ebin, GstEncodingProfile * sprof)
{
  guint nbprofused = 0;
  GList *tmp;

  for (tmp = ebin->streams; tmp; tmp = tmp->next) {
    StreamGroup *sgroup = (StreamGroup *) tmp->data;

    if (sgroup->profile == sprof)
      nbprofused++;
  }

  return nbprofused;
}

static inline GstEncodingProfile *
next_unused_stream_profile (GstEncodeBaseBin * ebin, GType ptype,
    const gchar * name, GstCaps * caps, GstEncodingProfile * previous_profile)
{
  GST_DEBUG_OBJECT (ebin, "ptype:%s, caps:%" GST_PTR_FORMAT,
      g_type_name (ptype), caps);

  if (G_UNLIKELY (ptype == G_TYPE_NONE && caps != NULL)) {
    /* Identify the profile type based on raw caps */
    if (gst_caps_can_intersect (ebin->raw_video_caps, caps))
      ptype = GST_TYPE_ENCODING_VIDEO_PROFILE;
    else if (gst_caps_can_intersect (ebin->raw_audio_caps, caps))
      ptype = GST_TYPE_ENCODING_AUDIO_PROFILE;
    /* else if (gst_caps_can_intersect (ebin->raw_text_caps, caps)) */
    /*   ptype = GST_TYPE_ENCODING_TEXT_PROFILE; */
    GST_DEBUG_OBJECT (ebin, "Detected profile type as being %s",
        g_type_name (ptype));
  }

  if (GST_IS_ENCODING_CONTAINER_PROFILE (ebin->profile)) {
    const GList *tmp;

    if (name) {
      /* If we have a name, try to find a profile with the same name */
      tmp =
          gst_encoding_container_profile_get_profiles
          (GST_ENCODING_CONTAINER_PROFILE (ebin->profile));

      for (; tmp; tmp = tmp->next) {
        GstEncodingProfile *sprof = (GstEncodingProfile *) tmp->data;
        const gchar *profilename = gst_encoding_profile_get_name (sprof);

        if (profilename && !strcmp (name, profilename)) {
          guint presence = gst_encoding_profile_get_presence (sprof);

          GST_DEBUG ("Found profile matching the requested name");

          if (!gst_encoding_profile_is_enabled (sprof)) {
            GST_INFO_OBJECT (ebin, "%p is disabled, not using it", sprof);

            return NULL;
          }

          if (presence == 0
              || presence > stream_profile_used_count (ebin, sprof))
            return sprof;

          GST_WARNING ("Matching stream already used");
          return NULL;
        }
      }
      GST_DEBUG
          ("No profiles matching requested pad name, carrying on with normal stream matching");
    }

    for (tmp =
        gst_encoding_container_profile_get_profiles
        (GST_ENCODING_CONTAINER_PROFILE (ebin->profile)); tmp;
        tmp = tmp->next) {
      GstEncodingProfile *sprof = (GstEncodingProfile *) tmp->data;

      /* Pick an available Stream profile for which:
       * * either it is of the compatible raw type,
       * * OR we can pass it through directly without encoding
       */
      if (G_TYPE_FROM_INSTANCE (sprof) == ptype) {
        guint presence = gst_encoding_profile_get_presence (sprof);
        GST_DEBUG ("Found a stream profile with the same type");
        if (!gst_encoding_profile_is_enabled (sprof)) {
          GST_INFO_OBJECT (ebin, "%p is disabled, not using it", sprof);
        } else if (presence == 0
            || (presence > stream_profile_used_count (ebin, sprof))) {

          if (sprof != previous_profile)
            return sprof;
        }
      } else if (caps && ptype == G_TYPE_NONE) {
        GstCaps *outcaps;
        gboolean res;

        outcaps = gst_encoding_profile_get_input_caps (sprof);
        GST_DEBUG ("Unknown stream, seeing if it's compatible with %"
            GST_PTR_FORMAT, outcaps);
        res = gst_caps_can_intersect (outcaps, caps);
        gst_caps_unref (outcaps);

        if (res && sprof != previous_profile)
          return sprof;
      }
    }
  }

  return NULL;
}

static GstPad *
request_pad_for_stream (GstEncodeBaseBin * encodebin, GType ptype,
    const gchar * name, GstCaps * caps)
{
  StreamGroup *sgroup = NULL;
  GList *not_found_encoder_profs = NULL, *tmp;
  GstEncodingProfile *sprof = NULL;

  GST_DEBUG_OBJECT (encodebin, "name:%s caps:%" GST_PTR_FORMAT, name, caps);

  while (sgroup == NULL) {
    gboolean encoder_not_found = FALSE;
    /* Figure out if we have a unused GstEncodingProfile we can use for
     * these caps */
    sprof = next_unused_stream_profile (encodebin, ptype, name, caps, sprof);

    if (G_UNLIKELY (sprof == NULL))
      goto no_stream_profile;

    sgroup = _create_stream_group (encodebin, sprof, name, caps,
        &encoder_not_found);

    if (G_UNLIKELY (sgroup))
      break;

    if (encoder_not_found) {
      not_found_encoder_profs = g_list_prepend (not_found_encoder_profs, sprof);
      if (name) {
        GST_DEBUG ("Could not create an encoder for %s", name);
        goto no_stream_group;
      }
    } else {
      break;
    }
  }

  if (!sgroup)
    goto no_stream_group;

  g_list_free (not_found_encoder_profs);
  return sgroup->ghostpad;

no_stream_profile:
  {
    GST_WARNING_OBJECT (encodebin, "Couldn't find a compatible stream profile");
    return NULL;
  }

no_stream_group:
  {
    for (tmp = not_found_encoder_profs; tmp; tmp = tmp->next)
      _post_missing_plugin_message (encodebin, tmp->data);
    g_list_free (not_found_encoder_profs);

    GST_WARNING_OBJECT (encodebin, "Couldn't create a StreamGroup");
    return NULL;
  }
}

static GstPad *
gst_encode_base_bin_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{
  GstEncodeBaseBin *ebin = (GstEncodeBaseBin *) element;
  GstPad *res = NULL;

  GST_DEBUG_OBJECT (element, "templ:%s, name:%s", templ->name_template, name);

  /* Identify the stream group (if name or caps have been provided) */
  if (caps != NULL || name != NULL) {
    res = request_pad_for_stream (ebin, G_TYPE_NONE, name, (GstCaps *) caps);
  }

  if (res == NULL) {
    GType ptype = G_TYPE_NONE;

    if (!strcmp (templ->name_template, "video_%u"))
      ptype = GST_TYPE_ENCODING_VIDEO_PROFILE;
    else if (!strcmp (templ->name_template, "audio_%u"))
      ptype = GST_TYPE_ENCODING_AUDIO_PROFILE;
    /* else if (!strcmp (templ->name_template, "text_%u")) */
    /*   ptype = GST_TYPE_ENCODING_TEXT_PROFILE; */

    /* FIXME : Check uniqueness of pad */
    /* FIXME : Check that the requested number is the last one, and if not,
     * update the last_pad_id variable so that we don't create a pad with
     * the same name/number in the future */

    res = request_pad_for_stream (ebin, ptype, name, NULL);
  }

  return res;
}

static GstPad *
gst_encode_base_bin_request_pad_signal (GstEncodeBaseBin * encodebin,
    GstCaps * caps)
{
  GstPad *pad = request_pad_for_stream (encodebin, G_TYPE_NONE, NULL, caps);

  return pad ? GST_PAD_CAST (gst_object_ref (pad)) : NULL;
}

static GstPad *
gst_encode_base_bin_request_profile_pad_signal (GstEncodeBaseBin * encodebin,
    const gchar * profilename)
{
  GstPad *pad =
      request_pad_for_stream (encodebin, G_TYPE_NONE, profilename, NULL);

  return pad ? GST_PAD_CAST (gst_object_ref (pad)) : NULL;
}

static inline StreamGroup *
find_stream_group_from_pad (GstEncodeBaseBin * ebin, GstPad * pad)
{
  GList *tmp;

  for (tmp = ebin->streams; tmp; tmp = tmp->next) {
    StreamGroup *sgroup = (StreamGroup *) tmp->data;
    if (G_UNLIKELY (sgroup->ghostpad == pad))
      return sgroup;
  }

  return NULL;
}

static void
gst_encode_base_bin_release_pad (GstElement * element, GstPad * pad)
{
  GstEncodeBaseBin *ebin = (GstEncodeBaseBin *) element;
  StreamGroup *sgroup;

  /* Find the associated StreamGroup */

  sgroup = find_stream_group_from_pad (ebin, pad);
  if (G_UNLIKELY (sgroup == NULL))
    goto no_stream_group;

  /* Release objects/data associated with the StreamGroup */
  stream_group_remove (ebin, sgroup);

  return;

no_stream_group:
  {
    GST_WARNING_OBJECT (ebin, "Couldn't find corresponding StreamGroup");
    return;
  }
}

/* Filters processing elements (can be a parser or a timestamper) from
 * @all_processors and returns the best fit (or %NULL if none matches)
 */
static inline GstElement *
_create_compatible_processor (GList * all_processors,
    GstEncodingProfile * sprof, GstElement * encoder)
{
  GList *processors1, *processors, *tmp;
  GstElement *processor = NULL;
  GstElementFactory *factory = NULL;
  GstCaps *format = NULL;

  if (encoder) {
    GstPadTemplate *template = gst_element_get_pad_template (encoder, "src");

    if (template)
      format = gst_pad_template_get_caps (template);
  }

  if (!format || gst_caps_is_any (format)) {
    gst_clear_caps (&format);
    format = gst_encoding_profile_get_format (sprof);
  }

  GST_DEBUG ("Getting list of processors for format %" GST_PTR_FORMAT, format);

  /* FIXME : requesting twice the processing element twice is a bit ugly, we
   * should have a method to request on more than one condition */
  processors1 =
      gst_element_factory_list_filter (all_processors, format,
      GST_PAD_SRC, FALSE);
  processors =
      gst_element_factory_list_filter (processors1, format, GST_PAD_SINK,
      FALSE);
  gst_plugin_feature_list_free (processors1);

  if (G_UNLIKELY (processors == NULL)) {
    GST_DEBUG ("Couldn't find any compatible processing element");
    goto beach;
  }

  for (tmp = processors; tmp; tmp = tmp->next) {
    /* FIXME : We're only picking the first one so far */
    /* FIXME : signal the user if he wants this */
    factory = (GstElementFactory *) tmp->data;
    break;
  }

  if (factory)
    processor = gst_element_factory_create (factory, NULL);

  gst_plugin_feature_list_free (processors);

beach:
  if (format)
    gst_caps_unref (format);

  return processor;
}

static gboolean
_set_properties (GQuark property_id, const GValue * value, GObject * element)
{
  GST_DEBUG_OBJECT (element, "Setting %s", g_quark_to_string (property_id));
  g_object_set_property (element, g_quark_to_string (property_id), value);

  return TRUE;
}

static void
set_element_properties_from_encoding_profile (GstEncodingProfile * profile,
    GParamSpec * arg G_GNUC_UNUSED, GstElement * element)
{
  gint i;
  const GValue *v;
  GstElementFactory *factory;
  GstStructure *properties =
      gst_encoding_profile_get_element_properties (profile);

  if (!properties)
    return;

  if (!gst_structure_has_name (properties, "element-properties-map")) {
    gst_structure_foreach (properties,
        (GstStructureForeachFunc) _set_properties, element);
    goto done;
  }

  factory = gst_element_get_factory (element);
  if (!factory) {
    GST_INFO_OBJECT (profile, "No factory for underlying element, "
        "not setting properties");
    return;
  }

  v = gst_structure_get_value (properties, "map");
  for (i = 0; i < gst_value_list_get_size (v); i++) {
    const GValue *map_value = gst_value_list_get_value (v, i);
    const GstStructure *tmp_properties;

    if (!GST_VALUE_HOLDS_STRUCTURE (map_value)) {
      g_warning ("Invalid value type %s in the property map "
          "(expected GstStructure)", G_VALUE_TYPE_NAME (map_value));
      continue;
    }

    tmp_properties = gst_value_get_structure (map_value);
    if (!gst_structure_has_name (tmp_properties, GST_OBJECT_NAME (factory))) {
      GST_INFO_OBJECT (GST_OBJECT_PARENT (element),
          "Ignoring values for %" GST_PTR_FORMAT, tmp_properties);
      continue;
    }

    GST_DEBUG_OBJECT (GST_OBJECT_PARENT (element),
        "Setting %" GST_PTR_FORMAT " on %" GST_PTR_FORMAT, tmp_properties,
        element);
    gst_structure_foreach (tmp_properties,
        (GstStructureForeachFunc) _set_properties, element);
    goto done;
  }

  GST_ERROR_OBJECT (GST_OBJECT_PARENT (element), "Unknown factory: %s",
      GST_OBJECT_NAME (factory));

done:
  gst_structure_free (properties);
}

static GstElement *
_create_element_and_set_preset (GstElementFactory * factory,
    GstEncodingProfile * profile, const gchar * name)
{
  GstElement *res = NULL;
  const gchar *preset;
  const gchar *preset_name;

  preset_name = gst_encoding_profile_get_preset_name (profile);
  preset = gst_encoding_profile_get_preset (profile);
  GST_DEBUG ("Creating element from factory %s (preset factory name: %s"
      " preset name: %s)", GST_OBJECT_NAME (factory), preset_name, preset);

  if (preset_name && g_strcmp0 (GST_OBJECT_NAME (factory), preset_name)) {
    GST_DEBUG ("Got to use %s, not %s", preset_name, GST_OBJECT_NAME (factory));
    return NULL;
  }

  res = gst_element_factory_create (factory, name);

  if (preset && GST_IS_PRESET (res)) {
    if (preset_name == NULL ||
        g_strcmp0 (GST_OBJECT_NAME (factory), preset_name) == 0) {

      if (!gst_preset_load_preset (GST_PRESET (res), preset)) {
        GST_WARNING ("Couldn't set preset [%s] on element [%s]",
            preset, GST_OBJECT_NAME (factory));
        gst_object_unref (res);
        res = NULL;
      }
    } else {
      GST_DEBUG ("Using a preset with no preset name, making use of the"
          " proper element without setting any property");
    }
  }
  /* Else we keep it */
  if (res) {
    set_element_properties_from_encoding_profile (profile, NULL, res);

    g_signal_connect (profile, "notify::element-properties",
        G_CALLBACK (set_element_properties_from_encoding_profile), res);
  }

  return res;
}

/* Create the encoder for the given stream profile */
static inline GstElement *
_get_encoder (GstEncodeBaseBin * ebin, GstEncodingProfile * sprof)
{
  GList *encoders, *tmp;
  GstElement *encoder = NULL;
  GstElementFactory *encoderfact = NULL;
  GstCaps *format;

  format = gst_encoding_profile_get_format (sprof);

  GST_DEBUG ("Getting list of encoders for format %" GST_PTR_FORMAT, format);

  /* If stream caps are raw, return identity */
  if (G_UNLIKELY (are_raw_caps (format))) {
    GST_DEBUG ("Stream format is raw, returning identity as the encoder");
    encoder = gst_element_factory_make ("identity", NULL);
    goto beach;
  }

  encoders =
      gst_element_factory_list_filter (ebin->encoders, format,
      GST_PAD_SRC, FALSE);

  if (G_UNLIKELY (encoders == NULL) && sprof == ebin->profile) {
    /* Special case: if the top-level profile is an encoder,
     * it could be listed in our muxers (for example wavenc)
     */
    encoders = gst_element_factory_list_filter (ebin->muxers, format,
        GST_PAD_SRC, FALSE);
  }

  if (G_UNLIKELY (encoders == NULL)) {
    GST_DEBUG ("Couldn't find any compatible encoders");
    goto beach;
  }

  for (tmp = encoders; tmp; tmp = tmp->next) {
    encoderfact = (GstElementFactory *) tmp->data;
    if ((encoder = _create_element_and_set_preset (encoderfact, sprof, NULL)))
      break;
  }

  gst_plugin_feature_list_free (encoders);

beach:
  if (format)
    gst_caps_unref (format);

  return encoder;
}

static GstPad *
local_element_request_pad (GstElement * element, GstPadTemplate * templ,
    const gchar * name, const GstCaps * caps)
{
  GstPad *newpad = NULL;
  GstElementClass *oclass;

  oclass = GST_ELEMENT_GET_CLASS (element);

  if (oclass->request_new_pad)
    newpad = (oclass->request_new_pad) (element, templ, name, caps);

  if (newpad)
    gst_object_ref (newpad);

  return newpad;
}

static GstPad *
gst_element_get_pad_from_template (GstElement * element, GstPadTemplate * templ)
{
  GstPad *ret = NULL;
  GstPadPresence presence;

  /* If this function is ever exported, we need check the validity of `element'
   * and `templ', and to make sure the template actually belongs to the
   * element. */

  presence = GST_PAD_TEMPLATE_PRESENCE (templ);

  switch (presence) {
    case GST_PAD_ALWAYS:
    case GST_PAD_SOMETIMES:
      ret = gst_element_get_static_pad (element, templ->name_template);
      if (!ret && presence == GST_PAD_ALWAYS)
        g_warning
            ("Element %s has an ALWAYS template %s, but no pad of the same name",
            GST_OBJECT_NAME (element), templ->name_template);
      break;

    case GST_PAD_REQUEST:
      ret = gst_element_request_pad (element, templ, NULL, NULL);
      break;
  }

  return ret;
}

static inline GstPad *
get_compatible_muxer_sink_pad (GstEncodeBaseBin * ebin,
    GstEncodingProfile * sprof, GstCaps * sinkcaps)
{
  GstPad *sinkpad;
  GList *padl, *compatible_templates = NULL;

  GST_DEBUG_OBJECT (ebin, "Finding muxer pad for caps: %" GST_PTR_FORMAT,
      sinkcaps);
  padl = gst_element_get_pad_template_list (ebin->muxer);
  for (; padl; padl = padl->next) {
    const gchar *type_name, *other_type_name;
    GstPadTemplate *padtempl = padl->data;

    if (padtempl->direction == GST_PAD_SRC)
      continue;

    if (!gst_caps_can_intersect (GST_PAD_TEMPLATE_CAPS (padtempl), sinkcaps))
      continue;

    if (!gst_caps_is_any (GST_PAD_TEMPLATE_CAPS (padtempl))) {
      compatible_templates = g_list_append (compatible_templates, padtempl);
      continue;
    }

    if (GST_IS_ENCODING_VIDEO_PROFILE (sprof)) {
      type_name = "video";
      other_type_name = "audio";
    } else if (GST_IS_ENCODING_AUDIO_PROFILE (sprof)) {
      type_name = "audio";
      other_type_name = "video";
    } else {
      compatible_templates = g_list_prepend (compatible_templates, padtempl);
      continue;
    }

    if (strstr (padtempl->name_template, type_name) == padtempl->name_template)
      compatible_templates = g_list_prepend (compatible_templates, padtempl);
    else if (!strstr (padtempl->name_template, other_type_name))
      compatible_templates = g_list_append (compatible_templates, padtempl);
    else
      GST_LOG_OBJECT (padtempl, "not compatible with %" GST_PTR_FORMAT, sprof);
  }

  if (G_UNLIKELY (compatible_templates == NULL))
    goto no_template;

  for (padl = compatible_templates; padl; padl = padl->next) {
    sinkpad = gst_element_get_pad_from_template (ebin->muxer, padl->data);
    if (sinkpad)
      break;
  }

  g_list_free (compatible_templates);

  GST_DEBUG_OBJECT (ebin, "Returning pad: %" GST_PTR_FORMAT, sinkpad);

  return sinkpad;

no_template:
  {
    GST_WARNING_OBJECT (ebin, "No compatible pad available on muxer");
    return NULL;
  }
}

static gboolean
_has_class (GstElement * element, const gchar * classname)
{
  GstElementClass *klass;
  const gchar *value;

  klass = GST_ELEMENT_GET_CLASS (element);
  value = gst_element_class_get_metadata (klass, GST_ELEMENT_METADATA_KLASS);
  if (!value)
    return FALSE;

  return strstr (value, classname) != NULL;
}

static void
_profile_restriction_caps_cb (GstEncodingProfile * profile,
    GParamSpec * arg G_GNUC_UNUSED, StreamGroup * group)
{
  GstCaps *restriction = gst_encoding_profile_get_restriction (profile);

  g_object_set (group->capsfilter, "caps", restriction, NULL);
}

static void
_capsfilter_force_format (GstPad * pad,
    GParamSpec * arg G_GNUC_UNUSED, StreamGroup * sgroup)
{
  GstCaps *caps;
  GstElement *parent =
      GST_ELEMENT_CAST (gst_object_get_parent (GST_OBJECT (pad)));

  if (!parent) {
    GST_DEBUG_OBJECT (pad, "Doesn't have a parent anymore");
    return;
  }

  g_object_get (pad, "caps", &caps, NULL);
  caps = gst_caps_copy (caps);

  GST_INFO_OBJECT (pad, "Forcing caps to %" GST_PTR_FORMAT, caps);
  if (parent == sgroup->outfilter || parent == sgroup->smart_capsfilter) {
    /* outfilter and the smart encoder internal capsfilter need to always be
     * in sync so the caps match between the two */
    if (sgroup->smart_capsfilter) {
      GstStructure *structure = gst_caps_get_structure (caps, 0);

      /* Pick a stream format that allows for in-band SPS updates if none
       * specified by the user, and remove restrictions on fields that can be
       * updated by codec_data or in-band SPS
       */
      if (gst_structure_has_name (structure, "video/x-h264") &&
          !gst_structure_has_field (structure, "stream_format")) {
        gst_structure_set (structure, "stream-format",
            G_TYPE_STRING, "avc3", NULL);

        gst_structure_remove_fields (structure, "codec_data", "profile",
            "level", NULL);
      } else if (gst_structure_has_name (structure, "video/x-h265") &&
          !gst_structure_has_field (structure, "stream_format")) {
        gst_structure_set (structure, "stream-format",
            G_TYPE_STRING, "hev1", NULL);

        gst_structure_remove_fields (structure, "codec_data", "tier", "profile",
            "level", NULL);
      }

      /* For VP8 / VP9, streamheader in the caps is informative, and
       * not actually used by muxers, we can allow it to change */
      if (gst_structure_has_name (structure, "video/x-vp8") ||
          gst_structure_has_name (structure, "video/x-vp9")) {
        gst_structure_remove_field (structure, "streamheader");
      }

      g_object_set (sgroup->smart_capsfilter, "caps", caps, NULL);

      g_signal_handler_disconnect (sgroup->smart_capsfilter->sinkpads->data,
          sgroup->smart_capsfilter_sid);
      sgroup->smart_capsfilter_sid = 0;
    }

    if (sgroup->outfilter) {
      GstCaps *tmpcaps = gst_caps_copy (caps);
      g_object_set (sgroup->outfilter, "caps", tmpcaps, NULL);
      gst_caps_unref (tmpcaps);
      g_signal_handler_disconnect (sgroup->outfilter->sinkpads->data,
          sgroup->outputfilter_caps_sid);
      sgroup->outputfilter_caps_sid = 0;
    }
  } else if (parent == sgroup->capsfilter) {
    g_object_set (parent, "caps", caps, NULL);
    g_signal_handler_disconnect (pad, sgroup->inputfilter_caps_sid);
  } else {
    g_assert_not_reached ();
  }

  gst_caps_unref (caps);
  gst_object_unref (parent);
}

static void
_set_group_caps_format (StreamGroup * sgroup, GstEncodingProfile * prof,
    GstCaps * format)
{
  g_object_set (sgroup->outfilter, "caps", format, NULL);

  if (!gst_encoding_profile_get_allow_dynamic_output (prof)) {
    if (!sgroup->outputfilter_caps_sid) {
      sgroup->outputfilter_caps_sid =
          g_signal_connect (sgroup->outfilter->sinkpads->data,
          "notify::caps", G_CALLBACK (_capsfilter_force_format), sgroup);
    }
  }
}

static void
_post_missing_plugin_message (GstEncodeBaseBin * ebin,
    GstEncodingProfile * prof)
{
  GstCaps *format;
  format = gst_encoding_profile_get_format (prof);

  GST_ERROR_OBJECT (ebin,
      "Couldn't create encoder with preset %s and preset name %s"
      " for format %" GST_PTR_FORMAT,
      GST_STR_NULL (gst_encoding_profile_get_preset (prof)),
      GST_STR_NULL (gst_encoding_profile_get_preset_name (prof)), format);

  /* missing plugin support */
  gst_element_post_message (GST_ELEMENT_CAST (ebin),
      gst_missing_encoder_message_new (GST_ELEMENT_CAST (ebin), format));
  GST_ELEMENT_ERROR (ebin, CORE, MISSING_PLUGIN,
      ("Couldn't create encoder for format %" GST_PTR_FORMAT, format), (NULL));

  gst_caps_unref (format);
}

static GstPadProbeReturn
_missing_plugin_probe (GstPad * pad, GstPadProbeInfo * info, gpointer udata)
{
  StreamGroup *sgroup = udata;
  GstEncodeBaseBin *ebin = sgroup->ebin;

  _post_missing_plugin_message (ebin, sgroup->profile);

  return GST_PAD_PROBE_OK;
}

static void
_set_up_fake_encoder_pad_probe (GstEncodeBaseBin * ebin, StreamGroup * sgroup)
{
  GstPad *pad = gst_element_get_static_pad (sgroup->fakesink, "sink");

  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER, _missing_plugin_probe,
      sgroup, NULL);

  gst_object_unref (pad);
}

static GstElement *
setup_smart_encoder (GstEncodeBaseBin * ebin, GstEncodingProfile * sprof,
    StreamGroup * sgroup)
{
  GstElement *encoder = NULL, *parser = NULL;
  GstElement *reencoder_bin = NULL;
  GstElement *sinkelement, *convert = NULL;
  GstElement *smartencoder = g_object_new (GST_TYPE_SMART_ENCODER, NULL);
  GstPad *srcpad = gst_element_get_static_pad (smartencoder, "src");
  GstCaps *format =
      gst_caps_make_writable (gst_encoding_profile_get_format (sprof));
  GstCaps *tmpcaps = gst_pad_query_caps (srcpad, NULL);
  const gboolean native_video =
      !!(ebin->flags & GST_ENCODEBIN_FLAG_NO_VIDEO_CONVERSION);
  GstStructure *structure = gst_caps_get_structure (format, 0);

  /* Check if stream format is compatible */
  if (!gst_caps_can_intersect (tmpcaps, format)) {
    GST_DEBUG_OBJECT (ebin,
        "We don't have a smart encoder for the stream format: %" GST_PTR_FORMAT,
        format);
    goto err;
  }

  sinkelement = encoder = _get_encoder (ebin, sprof);
  if (!encoder) {
    GST_INFO_OBJECT (ebin, "No encoder found... not using smart rendering");
    goto err;
  }

  parser = _create_compatible_processor (ebin->parsers, sprof, encoder);
  sgroup->smart_capsfilter = gst_element_factory_make ("capsfilter", NULL);
  reencoder_bin = gst_bin_new (NULL);

  /* Pick a stream format that allows for in-band SPS updates, and remove
   * restrictions on fields that can be updated by codec_data or in-band SPS
   */
  if (gst_structure_has_name (structure, "video/x-h264")) {
    gst_structure_set (structure, "stream-format", G_TYPE_STRING, "avc3", NULL);

    gst_structure_remove_fields (structure, "codec_data", "profile",
        "level", NULL);
  } else if (gst_structure_has_name (structure, "video/x-h265")) {
    gst_structure_set (structure, "stream-format", G_TYPE_STRING, "hev1", NULL);

    gst_structure_remove_fields (structure, "codec_data", "tier", "profile",
        "level", NULL);
  }

  /* For VP8 / VP9, streamheader in the caps is informative, and
   * not actually used by muxers, we can allow it to change */
  if (gst_structure_has_name (structure, "video/x-vp8") ||
      gst_structure_has_name (structure, "video/x-vp9")) {
    gst_structure_remove_field (structure, "streamheader");
  }

  g_object_set (sgroup->smart_capsfilter, "caps", format, NULL);

  gst_bin_add_many (GST_BIN (reencoder_bin),
      gst_object_ref (encoder),
      parser ? gst_object_ref (parser) :
      gst_object_ref (sgroup->smart_capsfilter),
      parser ? gst_object_ref (sgroup->smart_capsfilter) : NULL, NULL);
  if (!native_video) {
    convert = gst_element_factory_make ("videoconvert", NULL);
    if (!convert) {
      GST_ERROR_OBJECT (ebin, "`videoconvert` element missing");
      goto err;
    }

    gst_bin_add (GST_BIN (reencoder_bin), gst_object_ref (convert));
    if (!gst_element_link (convert, sinkelement)) {
      GST_ERROR_OBJECT (ebin, "Can not link `videoconvert` to %" GST_PTR_FORMAT,
          sinkelement);
      goto err;
    }
    sinkelement = convert;
  }

  if (!gst_element_link_many (encoder,
          parser ? parser : sgroup->smart_capsfilter,
          parser ? sgroup->smart_capsfilter : NULL, NULL)) {
    GST_ERROR_OBJECT (ebin, "Can not link smart encoding elements");
    goto err;
  }

  if (!gst_element_add_pad (reencoder_bin,
          gst_ghost_pad_new ("sink", sinkelement->sinkpads->data))) {
    GST_ERROR_OBJECT (ebin, "Can add smart encoding bin `srcpad`");
    goto err;
  }

  if (!gst_element_add_pad (reencoder_bin,
          gst_ghost_pad_new ("src", sgroup->smart_capsfilter->srcpads->data))) {
    GST_ERROR_OBJECT (ebin, "Could not ghost smart encoder bin"
        " srcpad, not being smart.");
    goto err;
  }

  if (!gst_encoding_profile_get_allow_dynamic_output (sprof)) {
    /* Enforce no dynamic output in the smart encoder */
    if (!sgroup->smart_capsfilter_sid) {
      sgroup->smart_capsfilter_sid =
          g_signal_connect (sgroup->smart_capsfilter->sinkpads->data,
          "notify::caps", G_CALLBACK (_capsfilter_force_format), sgroup);
    }
  }

  if (!gst_smart_encoder_set_encoder (GST_SMART_ENCODER (smartencoder),
          format, reencoder_bin)) {
    reencoder_bin = NULL;       /* We do not own the ref anymore */
    GST_ERROR_OBJECT (ebin, "Could not set encoder to the smart encoder,"
        " disabling smartness");
    goto err;
  }

done:
  gst_caps_unref (tmpcaps);
  gst_caps_unref (format);
  gst_object_unref (srcpad);
  gst_clear_object (&encoder);
  gst_clear_object (&parser);
  gst_clear_object (&convert);

  return smartencoder;

err:
  gst_clear_object (&smartencoder);
  gst_clear_object (&reencoder_bin);
  goto done;
}

static gboolean
gst_encode_base_bin_create_src_pad (GstEncodeBaseBin * ebin, GstPad * target)
{
  GstPadTemplate *template =
      gst_element_get_pad_template (GST_ELEMENT (ebin), "src_%u");
  gchar *name;
  GstPad *pad;

  GST_OBJECT_LOCK (ebin);
  name = g_strdup_printf ("src_%u", GST_ELEMENT (ebin)->numsrcpads);
  GST_OBJECT_UNLOCK (ebin);

  pad = gst_ghost_pad_new_from_template (name, target, template);
  g_free (name);
  if (!pad)
    return FALSE;

  gst_element_add_pad (GST_ELEMENT (ebin), pad);

  return TRUE;
}


/* FIXME : Add handling of streams that don't require conversion elements */
/*
 * Create the elements, StreamGroup, add the sink pad, link it to the muxer
 *
 * sinkpadname: If non-NULL, that name will be assigned to the sink ghost pad
 * sinkcaps: If non-NULL will be used to figure out how to setup the group
 * encoder_not_found: If non NULL, set to TRUE if failure happened because
 * the encoder could not be found
 */
static StreamGroup *
_create_stream_group (GstEncodeBaseBin * ebin, GstEncodingProfile * sprof,
    const gchar * sinkpadname, GstCaps * sinkcaps, gboolean * encoder_not_found)
{
  StreamGroup *sgroup = NULL;
  GstPad *sinkpad, *srcpad = NULL, *muxerpad = NULL;
  /* Element we will link to the encoder */
  GstElement *last = NULL;
  GstElement *encoder = NULL;
  GList *tmp, *tosync = NULL;
  GstCaps *format, *restriction;
  const gchar *missing_element_name;

  format = gst_encoding_profile_get_format (sprof);
  restriction = gst_encoding_profile_get_restriction (sprof);

  GST_DEBUG ("Creating group. format %" GST_PTR_FORMAT ", for caps %"
      GST_PTR_FORMAT, format, sinkcaps);
  GST_DEBUG ("avoid_reencoding:%d", ebin->avoid_reencoding);

  sgroup = g_new0 (StreamGroup, 1);
  sgroup->ebin = ebin;
  sgroup->profile = sprof;

  /* NOTE for people reading this code:
   *
   * We construct the group starting by the furthest downstream element
   * and making our way up adding/syncing/linking as we go.
   *
   * There are two parallel paths:
   * * One for raw data which goes through converters and encoders
   * * One for already encoded data
   */

  /* Put _get_encoder() before request pad from muxer as _get_encoder() may fail and
   * MOV/MP4 muxer don't support addition/removal of tracks at random times */
  sgroup->encoder = _get_encoder (ebin, sprof);
  if (!sgroup->encoder && (gst_encoding_profile_get_preset (sgroup->profile)
          || gst_encoding_profile_get_preset_name (sgroup->profile))) {

    if (!encoder_not_found)
      _post_missing_plugin_message (ebin, sprof);
    else
      *encoder_not_found = TRUE;
    goto cleanup;
  } else {
    /* passthrough can still work, if we discover that *
     * encoding is required we post a missing plugin message */
  }

  /* Muxer.
   * If we are handling a container profile, figure out if the muxer has a
   * sinkpad compatible with the selected profile */
  if (ebin->muxer) {
    muxerpad = get_compatible_muxer_sink_pad (ebin, sprof, format);
    if (G_UNLIKELY (muxerpad == NULL))
      goto no_muxer_pad;

  }

  /* Output Queue.
   * The actual queueing will be done in the input queue, but some queuing
   * after the encoder can be beneficial for encoding performance. */
  last = sgroup->outqueue = gst_element_factory_make ("queue", NULL);
  g_object_set (sgroup->outqueue, "max-size-buffers", (guint) 0,
      "max-size-bytes", (guint) 0, "max-size-time", (guint64) 3 * GST_SECOND,
      "silent", TRUE, NULL);

  gst_bin_add (GST_BIN (ebin), sgroup->outqueue);
  tosync = g_list_append (tosync, sgroup->outqueue);
  srcpad = gst_element_get_static_pad (sgroup->outqueue, "src");
  if (muxerpad) {
    if (G_UNLIKELY (fast_pad_link (srcpad, muxerpad) != GST_PAD_LINK_OK)) {
      goto muxer_link_failure;
    }
    gst_object_unref (muxerpad);
  } else {
    if (ebin->srcpad) {
      gst_ghost_pad_set_target (GST_GHOST_PAD (ebin->srcpad), srcpad);
    } else {
      if (!gst_encode_base_bin_create_src_pad (ebin, srcpad)) {
        gst_object_unref (srcpad);

        goto cant_add_src_pad;
      }
    }
  }
  gst_object_unref (srcpad);
  srcpad = NULL;

  /* Check if we need a formatter
   * If we have no muxer or
   * if the muxer isn't a formatter and doesn't implement the tagsetter interface
   */
  if (!ebin->muxer || (!GST_IS_TAG_SETTER (ebin->muxer)
          && !_has_class (ebin->muxer, "Formatter"))) {
    sgroup->formatter = _get_formatter (ebin, sprof);
    if (sgroup->formatter) {
      GST_DEBUG ("Adding formatter for %" GST_PTR_FORMAT, format);

      gst_bin_add (GST_BIN (ebin), sgroup->formatter);
      tosync = g_list_append (tosync, sgroup->formatter);
      if (G_UNLIKELY (!fast_element_link (sgroup->formatter, last)))
        goto formatter_link_failure;
      last = sgroup->formatter;
    }
  }


  /* Output capsfilter
   * This will receive the format caps from the streamprofile */
  GST_DEBUG ("Adding output capsfilter for %" GST_PTR_FORMAT, format);
  sgroup->outfilter = gst_element_factory_make ("capsfilter", NULL);
  _set_group_caps_format (sgroup, sprof, format);

  gst_bin_add (GST_BIN (ebin), sgroup->outfilter);
  tosync = g_list_append (tosync, sgroup->outfilter);
  if (G_UNLIKELY (!fast_element_link (sgroup->outfilter, last)))
    goto outfilter_link_failure;
  last = sgroup->outfilter;

  sgroup->parser =
      _create_compatible_processor (ebin->parsers, sgroup->profile,
      sgroup->encoder);
  if (sgroup->parser != NULL) {
    GST_DEBUG ("Got a parser %s", GST_ELEMENT_NAME (sgroup->parser));
    gst_bin_add (GST_BIN (ebin), sgroup->parser);
    tosync = g_list_append (tosync, sgroup->parser);
    if (G_UNLIKELY (!gst_element_link (sgroup->parser, last)))
      goto parser_link_failure;
    last = sgroup->parser;
  }

  sgroup->timestamper =
      _create_compatible_processor (ebin->timestampers, sprof, encoder);
  if (sgroup->timestamper != NULL) {
    GST_DEBUG ("Got a timestamper %s", GST_ELEMENT_NAME (sgroup->timestamper));
    gst_bin_add (GST_BIN (ebin), sgroup->timestamper);
    tosync = g_list_append (tosync, sgroup->timestamper);
    if (G_UNLIKELY (!gst_element_link (sgroup->timestamper, last)))
      goto parser_link_failure;

    last = sgroup->timestamper;
    if (sgroup->parser) {
      GstElement *p1 =
          gst_element_factory_make (GST_OBJECT_NAME (gst_element_get_factory
              (sgroup->parser)), NULL);

      gst_bin_add (GST_BIN (ebin), p1);
      tosync = g_list_append (tosync, p1);
      if (G_UNLIKELY (!gst_element_link (p1, last)))
        goto parser_link_failure;

      last = p1;
    }
  }

  /* Stream combiner */
  sgroup->combiner = g_object_new (GST_TYPE_STREAM_COMBINER, NULL);

  gst_bin_add (GST_BIN (ebin), sgroup->combiner);
  tosync = g_list_append (tosync, sgroup->combiner);
  if (G_UNLIKELY (!fast_element_link (sgroup->combiner, last)))
    goto combiner_link_failure;


  /* Stream splitter */
  sgroup->splitter = g_object_new (GST_TYPE_STREAM_SPLITTER, NULL);

  gst_bin_add (GST_BIN (ebin), sgroup->splitter);
  tosync = g_list_append (tosync, sgroup->splitter);

  if (gst_encoding_profile_get_single_segment (sprof)) {

    if (!ebin->avoid_reencoding) {
      sgroup->identity = gst_element_factory_make ("identity", NULL);
      g_object_set (sgroup->identity, "single-segment", TRUE, NULL);
      gst_bin_add (GST_BIN (ebin), sgroup->identity);
      tosync = g_list_append (tosync, sgroup->identity);
    } else {
      GST_INFO_OBJECT (ebin, "Single segment is not supported when avoiding"
          " to re-encode!");
    }
  }

  /* Input queue
   * FIXME : figure out what max-size to use for the input queue */
  sgroup->inqueue = gst_element_factory_make ("queue", NULL);
  g_object_set (sgroup->inqueue, "max-size-buffers",
      (guint) ebin->queue_buffers_max, "max-size-bytes",
      (guint) ebin->queue_bytes_max, "max-size-time",
      (guint64) ebin->queue_time_max, "silent", TRUE, NULL);

  gst_bin_add (GST_BIN (ebin), sgroup->inqueue);
  tosync = g_list_append (tosync, sgroup->inqueue);

  /* Expose input queue or identity sink pad as ghostpad */
  sinkpad =
      gst_element_get_static_pad (sgroup->identity ? sgroup->identity : sgroup->
      inqueue, "sink");
  if (sinkpadname == NULL) {
    gchar *pname =
        g_strdup_printf ("%s_%u", gst_encoding_profile_get_type_nick (sprof),
        ebin->last_pad_id++);
    GST_DEBUG ("Adding ghost pad %s", pname);
    sgroup->ghostpad = gst_ghost_pad_new (pname, sinkpad);
    g_free (pname);
  } else
    sgroup->ghostpad = gst_ghost_pad_new (sinkpadname, sinkpad);
  gst_object_unref (sinkpad);

  if (sgroup->identity
      && G_UNLIKELY (!fast_element_link (sgroup->identity, sgroup->inqueue)))
    goto queue_link_failure;

  if (G_UNLIKELY (!fast_element_link (sgroup->inqueue, sgroup->splitter)))
    goto splitter_link_failure;


  /* Path 1 : Already-encoded data */
  sinkpad =
      local_element_request_pad (sgroup->combiner, NULL, "passthroughsink",
      NULL);
  if (G_UNLIKELY (sinkpad == NULL))
    goto no_combiner_sinkpad;

  if (ebin->avoid_reencoding) {
    GST_DEBUG ("Asked to use Smart Encoder");
    sgroup->smartencoder = setup_smart_encoder (ebin, sprof, sgroup);
    if (sgroup->smartencoder) {
      gst_bin_add ((GstBin *) ebin, sgroup->smartencoder);
      srcpad = gst_element_get_static_pad (sgroup->smartencoder, "src");
      fast_pad_link (srcpad, sinkpad);
      gst_object_unref (srcpad);
      tosync = g_list_append (tosync, sgroup->smartencoder);
      sinkpad = gst_element_get_static_pad (sgroup->smartencoder, "sink");
    }
  }

  srcpad =
      local_element_request_pad (sgroup->splitter, NULL, "passthroughsrc",
      NULL);
  if (G_UNLIKELY (srcpad == NULL))
    goto no_splitter_srcpad;

  /* Go straight to splitter */
  if (G_UNLIKELY (fast_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK))
    goto passthrough_link_failure;
  gst_object_unref (sinkpad);
  gst_object_unref (srcpad);
  srcpad = NULL;

  /* Path 2 : Conversion / Encoding */

  /* 1. Create the encoder */
  GST_LOG ("Adding encoder");
  if (sgroup->encoder) {
    gst_bin_add ((GstBin *) ebin, sgroup->encoder);
    tosync = g_list_append (tosync, sgroup->encoder);

    sinkpad =
        local_element_request_pad (sgroup->combiner, NULL, "encodingsink",
        NULL);
    if (G_UNLIKELY (sinkpad == NULL))
      goto no_combiner_sinkpad;
    srcpad = gst_element_get_static_pad (sgroup->encoder, "src");
    if (G_UNLIKELY (fast_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK))
      goto encoder_link_failure;
    gst_object_unref (sinkpad);
    gst_object_unref (srcpad);
    srcpad = NULL;
  }


  /* 3. Create the conversion/restriction elements */
  /* 3.1. capsfilter */
  GST_LOG ("Adding capsfilter for restriction caps : %" GST_PTR_FORMAT,
      restriction);

  last = sgroup->capsfilter = gst_element_factory_make ("capsfilter", NULL);
  if (restriction && !gst_caps_is_any (restriction))
    g_object_set (sgroup->capsfilter, "caps", restriction, NULL);

  if (!gst_encoding_profile_get_allow_dynamic_output (sprof)) {
    if (!sgroup->inputfilter_caps_sid) {
      sgroup->inputfilter_caps_sid =
          g_signal_connect (sgroup->capsfilter->sinkpads->data,
          "notify::caps", G_CALLBACK (_capsfilter_force_format), sgroup);
    }
  }

  gst_bin_add ((GstBin *) ebin, sgroup->capsfilter);
  tosync = g_list_append (tosync, sgroup->capsfilter);
  if (sgroup->encoder == NULL) {
    /* no encoder available but it might be possible to just do passthrough, so
     * let's just set up a fake pad to detect that encoding was attempted and
     * if so it posts the missing plugin message */
    sgroup->fakesink = gst_element_factory_make ("fakesink", NULL);
    g_object_set (sgroup->fakesink, "async", FALSE, NULL);
    gst_bin_add (GST_BIN_CAST (ebin), sgroup->fakesink);
    tosync = g_list_append (tosync, sgroup->fakesink);
    encoder = sgroup->fakesink;

    _set_up_fake_encoder_pad_probe (ebin, sgroup);
  } else {
    encoder = sgroup->encoder;
  }
  fast_element_link (sgroup->capsfilter, encoder);
  sgroup->restriction_sid = g_signal_connect (sprof, "notify::restriction-caps",
      G_CALLBACK (_profile_restriction_caps_cb), sgroup);

  /* 3.2. restriction elements */
  /* FIXME : Once we have properties for specific converters, use those */
  if (GST_IS_ENCODING_VIDEO_PROFILE (sprof)) {
    const gboolean native_video =
        !!(ebin->flags & GST_ENCODEBIN_FLAG_NO_VIDEO_CONVERSION);
    GstElement *cspace = NULL, *scale, *vrate, *cspace2 = NULL;

    GST_LOG ("Adding conversion elements for video stream");

    if (!native_video) {
      cspace = gst_element_factory_make ("videoconvert", NULL);
      scale = gst_element_factory_make ("videoscale", NULL);
      if (!scale) {
        missing_element_name = "videoscale";
        goto missing_element;
      }
      /* 4-tap scaling and black borders */
      g_object_set (scale, "method", 2, "add-borders", TRUE, NULL);
      cspace2 = gst_element_factory_make ("videoconvert", NULL);

      if (!cspace || !cspace2) {
        missing_element_name = "videoconvert";
        goto missing_element;
      }

      gst_bin_add_many ((GstBin *) ebin, cspace, scale, cspace2, NULL);
      tosync = g_list_append (tosync, cspace);
      tosync = g_list_append (tosync, scale);
      tosync = g_list_append (tosync, cspace2);

      sgroup->converters = g_list_prepend (sgroup->converters, cspace);
      sgroup->converters = g_list_prepend (sgroup->converters, scale);
      sgroup->converters = g_list_prepend (sgroup->converters, cspace2);

      if (!fast_element_link (cspace, scale) ||
          !fast_element_link (scale, cspace2))
        goto converter_link_failure;
    }

    if (!gst_encoding_video_profile_get_variableframerate
        (GST_ENCODING_VIDEO_PROFILE (sprof))) {
      vrate = gst_element_factory_make ("videorate", NULL);
      if (!vrate) {
        missing_element_name = "videorate";
        goto missing_element;
      }
      g_object_set (vrate, "skip-to-first", TRUE, NULL);

      gst_bin_add ((GstBin *) ebin, vrate);
      tosync = g_list_prepend (tosync, vrate);
      sgroup->converters = g_list_prepend (sgroup->converters, vrate);

      if ((!native_video && !fast_element_link (cspace2, vrate))
          || !fast_element_link (vrate, last))
        goto converter_link_failure;

      if (!native_video)
        last = cspace;
      else
        last = vrate;
    } else if (!native_video) {
      if (!fast_element_link (cspace2, last))
        goto converter_link_failure;
      last = cspace;
    }

  } else if (GST_IS_ENCODING_AUDIO_PROFILE (sprof)
      && !(ebin->flags & GST_ENCODEBIN_FLAG_NO_AUDIO_CONVERSION)) {
    GstElement *aconv, *ares, *arate, *aconv2;

    GST_LOG ("Adding conversion elements for audio stream");

    arate = gst_element_factory_make ("audiorate", NULL);
    if (!arate) {
      missing_element_name = "audiorate";
      goto missing_element;
    }
    g_object_set (arate, "tolerance", (guint64) ebin->tolerance, NULL);
    g_object_set (arate, "skip-to-first", TRUE, NULL);

    aconv = gst_element_factory_make ("audioconvert", NULL);
    aconv2 = gst_element_factory_make ("audioconvert", NULL);
    ares = gst_element_factory_make ("audioresample", NULL);
    if (!aconv || !aconv2) {
      missing_element_name = "audioconvert";
      goto missing_element;
    }
    if (!ares) {
      missing_element_name = "audioresample";
      goto missing_element;
    }

    gst_bin_add_many ((GstBin *) ebin, arate, aconv, ares, aconv2, NULL);
    tosync = g_list_append (tosync, arate);
    tosync = g_list_append (tosync, aconv);
    tosync = g_list_append (tosync, ares);
    tosync = g_list_append (tosync, aconv2);
    if (!fast_element_link (arate, aconv) ||
        !fast_element_link (aconv, ares) ||
        !fast_element_link (ares, aconv2) || !fast_element_link (aconv2, last))
      goto converter_link_failure;

    sgroup->converters = g_list_prepend (sgroup->converters, arate);
    sgroup->converters = g_list_prepend (sgroup->converters, aconv);
    sgroup->converters = g_list_prepend (sgroup->converters, ares);
    sgroup->converters = g_list_prepend (sgroup->converters, aconv2);

    last = arate;
  }

  /* Link to stream splitter */
  sinkpad = gst_element_get_static_pad (last, "sink");
  srcpad =
      local_element_request_pad (sgroup->splitter, NULL, "encodingsrc", NULL);
  if (G_UNLIKELY (srcpad == NULL))
    goto no_splitter_srcpad;
  if (G_UNLIKELY (fast_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK))
    goto splitter_encoding_failure;
  gst_object_unref (sinkpad);
  gst_object_unref (srcpad);
  srcpad = NULL;

  /* End of Stream 2 setup */

  /* Sync all elements to parent state */
  for (tmp = tosync; tmp; tmp = tmp->next)
    gst_element_sync_state_with_parent ((GstElement *) tmp->data);
  g_list_free (tosync);

  /* Add ghostpad */
  GST_DEBUG ("Adding ghostpad %s:%s", GST_DEBUG_PAD_NAME (sgroup->ghostpad));
  gst_pad_set_active (sgroup->ghostpad, TRUE);
  gst_element_add_pad ((GstElement *) ebin, sgroup->ghostpad);

  /* Add StreamGroup to our list of streams */

  GST_DEBUG
      ("Done creating elements, adding StreamGroup to our controlled stream list");

  ebin->streams = g_list_prepend (ebin->streams, sgroup);

  if (format)
    gst_caps_unref (format);
  if (restriction)
    gst_caps_unref (restriction);

  return sgroup;

splitter_encoding_failure:
  GST_ERROR_OBJECT (ebin, "Error linking splitter to encoding stream");
  goto cleanup;

cant_add_src_pad:
  GST_ERROR_OBJECT (ebin, "Couldn't add srcpad to encodebin");
  goto cleanup;

no_muxer_pad:
  GST_ERROR_OBJECT (ebin,
      "Couldn't find a compatible muxer pad to link encoder to");
  goto cleanup;

missing_element:
  gst_element_post_message (GST_ELEMENT_CAST (ebin),
      gst_missing_element_message_new (GST_ELEMENT_CAST (ebin),
          missing_element_name));
  GST_ELEMENT_ERROR (ebin, CORE, MISSING_PLUGIN,
      (_("Missing element '%s' - check your GStreamer installation."),
          missing_element_name), (NULL));
  goto cleanup;

encoder_link_failure:
  GST_ERROR_OBJECT (ebin, "Failed to link the encoder");
  goto cleanup;

muxer_link_failure:
  GST_ERROR_OBJECT (ebin, "Couldn't link encoder to muxer");
  goto cleanup;

formatter_link_failure:
  GST_ERROR_OBJECT (ebin, "Couldn't link output filter to output queue");
  goto cleanup;

outfilter_link_failure:
  GST_ERROR_OBJECT (ebin,
      "Couldn't link output filter to output queue/formatter");
  goto cleanup;

passthrough_link_failure:
  GST_ERROR_OBJECT (ebin, "Failed linking splitter in passthrough mode");
  goto cleanup;

no_splitter_srcpad:
  GST_ERROR_OBJECT (ebin, "Couldn't get a source pad from the splitter");
  goto cleanup;

no_combiner_sinkpad:
  GST_ERROR_OBJECT (ebin, "Couldn't get a sink pad from the combiner");
  goto cleanup;

splitter_link_failure:
  GST_ERROR_OBJECT (ebin, "Failure linking to the splitter");
  goto cleanup;

queue_link_failure:
  GST_ERROR_OBJECT (ebin, "Failure linking to the inqueue");
  goto cleanup;

combiner_link_failure:
  GST_ERROR_OBJECT (ebin, "Failure linking to the combiner");
  goto cleanup;

parser_link_failure:
  GST_ERROR_OBJECT (ebin, "Failure linking the parser");
  goto cleanup;

converter_link_failure:
  GST_ERROR_OBJECT (ebin, "Failure linking the video converters");
  goto cleanup;

cleanup:
  /* FIXME : Actually properly cleanup everything */
  if (format)
    gst_caps_unref (format);
  if (restriction)
    gst_caps_unref (restriction);
  if (srcpad)
    gst_object_unref (srcpad);
  stream_group_free (ebin, sgroup);
  g_list_free (tosync);
  return NULL;
}

static gboolean
_gst_caps_match_foreach (GQuark field_id, const GValue * value, gpointer data)
{
  GstStructure *structure = data;
  const GValue *other_value = gst_structure_id_get_value (structure, field_id);

  if (G_UNLIKELY (other_value == NULL))
    return FALSE;
  if (gst_value_compare (value, other_value) == GST_VALUE_EQUAL) {
    return TRUE;
  }

  return FALSE;
}

/*
 * checks that there is at least one structure on caps_a that has
 * all its fields exactly the same as one structure on caps_b
 */
static gboolean
_gst_caps_match (const GstCaps * caps_a, const GstCaps * caps_b)
{
  gint i, j;
  gboolean res = FALSE;

  for (i = 0; i < gst_caps_get_size (caps_a); i++) {
    GstStructure *structure_a = gst_caps_get_structure (caps_a, i);
    for (j = 0; j < gst_caps_get_size (caps_b); j++) {
      GstStructure *structure_b = gst_caps_get_structure (caps_b, j);

      res = gst_structure_foreach (structure_a, _gst_caps_match_foreach,
          structure_b);
      if (res)
        goto end;
    }
  }
end:
  return res;
}

static gboolean
_factory_can_handle_caps (GstElementFactory * factory, const GstCaps * caps,
    GstPadDirection dir, gboolean exact)
{
  const GList *templates;

  templates = gst_element_factory_get_static_pad_templates (factory);
  while (templates) {
    GstStaticPadTemplate *template = (GstStaticPadTemplate *) templates->data;

    if (template->direction == dir) {
      GstCaps *tmp = gst_static_caps_get (&template->static_caps);

      if ((exact && _gst_caps_match (caps, tmp)) ||
          (!exact && gst_caps_can_intersect (tmp, caps))) {
        gst_caps_unref (tmp);
        return TRUE;
      }
      gst_caps_unref (tmp);
    }
    templates = g_list_next (templates);
  }

  return FALSE;
}

static inline GstElement *
_get_formatter (GstEncodeBaseBin * ebin, GstEncodingProfile * sprof)
{
  GList *formatters, *tmpfmtr;
  GstElement *formatter = NULL;
  GstElementFactory *formatterfact = NULL;
  GstCaps *format;
  format = gst_encoding_profile_get_format (sprof);

  GST_DEBUG ("Getting list of formatters for format %" GST_PTR_FORMAT, format);

  formatters =
      gst_element_factory_list_filter (ebin->formatters, format, GST_PAD_SRC,
      FALSE);

  if (formatters == NULL)
    goto beach;

  /* FIXME : signal the user if he wants this */
  for (tmpfmtr = formatters; tmpfmtr; tmpfmtr = tmpfmtr->next) {
    formatterfact = (GstElementFactory *) tmpfmtr->data;

    GST_DEBUG_OBJECT (ebin, "Trying formatter %s",
        GST_OBJECT_NAME (formatterfact));

    if ((formatter =
            _create_element_and_set_preset (formatterfact, sprof, NULL)))
      break;
  }

  gst_plugin_feature_list_free (formatters);

beach:
  if (format)
    gst_caps_unref (format);
  return formatter;
}

static gint
compare_elements (gconstpointer a, gconstpointer b, gpointer udata)
{
  GstCaps *caps = udata;
  GstElementFactory *fac_a = (GstElementFactory *) a;
  GstElementFactory *fac_b = (GstElementFactory *) b;

  /* FIXME not quite sure this is the best algorithm to order the elements
   * Some caps similarity comparison algorithm would fit better than going
   * boolean (equals/not equals).
   */
  gboolean equals_a = _factory_can_handle_caps (fac_a, caps, GST_PAD_SRC, TRUE);
  gboolean equals_b = _factory_can_handle_caps (fac_b, caps, GST_PAD_SRC, TRUE);

  if (equals_a == equals_b) {
    return gst_plugin_feature_get_rank ((GstPluginFeature *) fac_b) -
        gst_plugin_feature_get_rank ((GstPluginFeature *) fac_a);
  } else if (equals_a) {
    return -1;
  } else if (equals_b) {
    return 1;
  }
  return 0;
}

static inline GstElement *
_get_muxer (GstEncodeBaseBin * ebin)
{
  GList *muxers = NULL, *formatters, *tmpmux;
  GstElement *muxer = NULL;
  GstElementFactory *muxerfact = NULL;
  const GList *tmp;
  GstCaps *format;
  const gchar *preset_name;

  format = gst_encoding_profile_get_format (ebin->profile);
  preset_name = gst_encoding_profile_get_preset_name (ebin->profile);

  GST_DEBUG_OBJECT (ebin, "Getting list of muxers for format %" GST_PTR_FORMAT,
      format);

  if (preset_name) {
    GstElementFactory *f =
        (GstElementFactory *) gst_registry_find_feature (gst_registry_get (),
        preset_name,
        GST_TYPE_ELEMENT_FACTORY);

    if (f)
      muxers = g_list_append (muxers, f);
  } else {
    muxers =
        gst_element_factory_list_filter (ebin->muxers, format, GST_PAD_SRC,
        !preset_name);

  }

  formatters =
      gst_element_factory_list_filter (ebin->formatters, format, GST_PAD_SRC,
      TRUE);

  muxers = g_list_sort_with_data (muxers, compare_elements, (gpointer) format);
  formatters =
      g_list_sort_with_data (formatters, compare_elements, (gpointer) format);

  muxers = g_list_concat (muxers, formatters);

  if (muxers == NULL)
    goto beach;

  /* FIXME : signal the user if he wants this */
  for (tmpmux = muxers; tmpmux; tmpmux = tmpmux->next) {
    gboolean cansinkstreams = TRUE;
    const GList *profiles =
        gst_encoding_container_profile_get_profiles
        (GST_ENCODING_CONTAINER_PROFILE (ebin->profile));

    muxerfact = (GstElementFactory *) tmpmux->data;

    GST_DEBUG_OBJECT (ebin, "Trying muxer %s", GST_OBJECT_NAME (muxerfact));

    /* See if the muxer can sink all of our stream profile caps */
    for (tmp = profiles; tmp; tmp = tmp->next) {
      GstEncodingProfile *sprof = (GstEncodingProfile *) tmp->data;
      GstCaps *sformat = gst_encoding_profile_get_format (sprof);

      if (!_factory_can_handle_caps (muxerfact, sformat, GST_PAD_SINK, FALSE)) {
        GST_DEBUG ("Skipping muxer because it can't sink caps %"
            GST_PTR_FORMAT, sformat);
        cansinkstreams = FALSE;
        if (sformat)
          gst_caps_unref (sformat);
        break;
      }
      if (sformat)
        gst_caps_unref (sformat);
    }

    /* Only use a muxer than can use all streams and than can accept the
     * preset (which may be present or not) */
    if (cansinkstreams && (muxer =
            _create_element_and_set_preset (muxerfact, ebin->profile, "muxer")))
      break;
  }

  gst_plugin_feature_list_free (muxers);

beach:
  if (format)
    gst_caps_unref (format);
  return muxer;
}

static gboolean
create_elements_and_pads (GstEncodeBaseBin * ebin)
{
  gboolean ret = TRUE;
  GstElement *muxer = NULL;
  GstPad *muxerpad;
  const GList *tmp, *profiles;
  GstEncodingProfile *sprof;

  GST_DEBUG ("Current profile : %s",
      gst_encoding_profile_get_name (ebin->profile));

  if (GST_IS_ENCODING_CONTAINER_PROFILE (ebin->profile)) {
    /* Get the compatible muxer */
    muxer = _get_muxer (ebin);
    if (G_UNLIKELY (muxer == NULL))
      goto no_muxer;

    /* Record the muxer */
    ebin->muxer = muxer;
    gst_bin_add ((GstBin *) ebin, muxer);

    /* If the subclass exposes a static sourcepad, ghost the muxer
     * output, otherwise expose the muxer srcpad if it has one,
     * do not expose any srcpad if we are dealing with a muxing sink. */
    /* FIXME : We should figure out if it's a static/request/dynamic pad,
     * but for the time being let's assume it's a static pad :) */
    muxerpad = gst_element_get_static_pad (muxer, "src");
    if (ebin->srcpad) {
      if (G_UNLIKELY (muxerpad == NULL))
        goto no_muxer_pad;
      if (!gst_ghost_pad_set_target (GST_GHOST_PAD (ebin->srcpad), muxerpad))
        goto no_muxer_ghost_pad;

      gst_object_unref (muxerpad);
    } else if (muxerpad) {
      if (!gst_encode_base_bin_create_src_pad (ebin, muxerpad)) {
        goto no_muxer_ghost_pad;
      }
      gst_object_unref (muxerpad);
    }

    /* Activate fixed presence streams */
    profiles =
        gst_encoding_container_profile_get_profiles
        (GST_ENCODING_CONTAINER_PROFILE (ebin->profile));
    for (tmp = profiles; tmp; tmp = tmp->next) {
      sprof = (GstEncodingProfile *) tmp->data;

      GST_DEBUG ("Trying stream profile with presence %d",
          gst_encoding_profile_get_presence (sprof));

      if (gst_encoding_profile_get_presence (sprof) != 0 &&
          gst_encoding_profile_is_enabled (sprof)) {
        if (G_UNLIKELY (_create_stream_group (ebin, sprof, NULL, NULL,
                    NULL) == NULL))
          goto stream_error;
      }
    }
    gst_element_sync_state_with_parent (muxer);
  } else {
    if (G_UNLIKELY (_create_stream_group (ebin, ebin->profile, NULL,
                NULL, NULL) == NULL))
      goto stream_error;
  }

  return ret;

no_muxer:
  {
    GstCaps *format = gst_encoding_profile_get_format (ebin->profile);

    GST_WARNING ("No available muxer for %" GST_PTR_FORMAT, format);
    /* missing plugin support */
    gst_element_post_message (GST_ELEMENT_CAST (ebin),
        gst_missing_encoder_message_new (GST_ELEMENT_CAST (ebin), format));
    GST_ELEMENT_ERROR (ebin, CORE, MISSING_PLUGIN,
        ("No available muxer for format %" GST_PTR_FORMAT, format), (NULL));
    if (format)
      gst_caps_unref (format);
    return FALSE;
  }

no_muxer_pad:
  {
    GST_WARNING ("Can't get source pad from muxer (%s)",
        GST_ELEMENT_NAME (muxer));
    gst_bin_remove (GST_BIN (ebin), muxer);
    return FALSE;
  }

no_muxer_ghost_pad:
  {
    GST_WARNING ("Couldn't set %s:%s as source ghostpad target",
        GST_DEBUG_PAD_NAME (muxerpad));
    gst_bin_remove (GST_BIN (ebin), muxer);
    gst_object_unref (muxerpad);
    return FALSE;
  }

stream_error:
  {
    GST_WARNING ("Could not create Streams");
    if (muxer)
      gst_bin_remove (GST_BIN (ebin), muxer);
    ebin->muxer = NULL;
    return FALSE;
  }
}

static void
release_pads (const GValue * item, GstElement * elt)
{
  GstPad *pad = g_value_get_object (item);
  GstPad *peer = NULL;

  GST_DEBUG_OBJECT (elt, "Releasing pad %s:%s", GST_DEBUG_PAD_NAME (pad));

  /* Unlink from its peer pad */
  if ((peer = gst_pad_get_peer (pad))) {
    if (GST_PAD_DIRECTION (peer) == GST_PAD_SRC)
      gst_pad_unlink (peer, pad);
    else
      gst_pad_unlink (pad, peer);
    gst_object_unref (peer);
  }

  /* Release it from the object */
  gst_element_release_request_pad (elt, pad);
}

static void
stream_group_free (GstEncodeBaseBin * ebin, StreamGroup * sgroup)
{
  GList *tmp;
  GstPad *tmppad;
  GstPad *pad;

  GST_DEBUG_OBJECT (ebin, "Freeing StreamGroup %p", sgroup);

  if (sgroup->restriction_sid != 0)
    g_signal_handler_disconnect (sgroup->profile, sgroup->restriction_sid);

  if (sgroup->outqueue) {
    if (ebin->muxer) {
      /* outqueue - Muxer */
      tmppad = gst_element_get_static_pad (sgroup->outqueue, "src");
      pad = gst_pad_get_peer (tmppad);

      if (pad) {
        /* Remove muxer request sink pad */
        gst_pad_unlink (tmppad, pad);
        if (GST_PAD_TEMPLATE_PRESENCE (GST_PAD_PAD_TEMPLATE (pad)) ==
            GST_PAD_REQUEST)
          gst_element_release_request_pad (ebin->muxer, pad);
        gst_object_unref (pad);
      }
      gst_object_unref (tmppad);
    }
    gst_element_set_state (sgroup->outqueue, GST_STATE_NULL);
  }

  if (sgroup->formatter) {
    /* capsfilter - formatter - outqueue */
    gst_element_set_state (sgroup->formatter, GST_STATE_NULL);
    gst_element_set_state (sgroup->outfilter, GST_STATE_NULL);
    gst_element_unlink (sgroup->formatter, sgroup->outqueue);
    gst_element_unlink (sgroup->outfilter, sgroup->formatter);
  } else if (sgroup->outfilter) {
    /* Capsfilter - outqueue */
    gst_element_set_state (sgroup->outfilter, GST_STATE_NULL);
    gst_element_unlink (sgroup->outfilter, sgroup->outqueue);
  }

  if (sgroup->outqueue) {
    gst_element_set_state (sgroup->outqueue, GST_STATE_NULL);
    gst_bin_remove (GST_BIN (ebin), sgroup->outqueue);
  }

  /* streamcombiner - parser - capsfilter */
  if (sgroup->parser) {
    gst_element_set_state (sgroup->parser, GST_STATE_NULL);
    gst_element_unlink (sgroup->parser, sgroup->outfilter);
    gst_element_unlink (sgroup->combiner, sgroup->parser);
    gst_bin_remove ((GstBin *) ebin, sgroup->parser);
  }

  /* Sink Ghostpad */
  if (sgroup->ghostpad) {
    if (GST_PAD_PARENT (sgroup->ghostpad) != NULL)
      gst_element_remove_pad (GST_ELEMENT_CAST (ebin), sgroup->ghostpad);
    else
      gst_object_unref (sgroup->ghostpad);
  }

  if (sgroup->inqueue)
    gst_element_set_state (sgroup->inqueue, GST_STATE_NULL);

  if (sgroup->encoder) {
    gst_element_set_state (sgroup->encoder, GST_STATE_NULL);
    g_signal_handlers_disconnect_by_func (sgroup->profile,
        set_element_properties_from_encoding_profile, sgroup->encoder);
  }
  if (sgroup->fakesink)
    gst_element_set_state (sgroup->fakesink, GST_STATE_NULL);
  if (sgroup->outfilter) {
    gst_element_set_state (sgroup->outfilter, GST_STATE_NULL);

    if (sgroup->outputfilter_caps_sid) {
      g_signal_handler_disconnect (sgroup->outfilter->sinkpads->data,
          sgroup->outputfilter_caps_sid);
      sgroup->outputfilter_caps_sid = 0;
    }
  }
  if (sgroup->smartencoder)
    gst_element_set_state (sgroup->smartencoder, GST_STATE_NULL);
  gst_clear_object (&sgroup->smart_capsfilter);

  if (sgroup->capsfilter) {
    gst_element_set_state (sgroup->capsfilter, GST_STATE_NULL);
    if (sgroup->encoder)
      gst_element_unlink (sgroup->capsfilter, sgroup->encoder);
    else
      gst_element_unlink (sgroup->capsfilter, sgroup->fakesink);

    gst_bin_remove ((GstBin *) ebin, sgroup->capsfilter);
  }

  for (tmp = sgroup->converters; tmp; tmp = tmp->next) {
    GstElement *elt = (GstElement *) tmp->data;

    gst_element_set_state (elt, GST_STATE_NULL);
    gst_bin_remove ((GstBin *) ebin, elt);
  }
  if (sgroup->converters)
    g_list_free (sgroup->converters);

  if (sgroup->combiner) {
    GstIterator *it = gst_element_iterate_sink_pads (sgroup->combiner);
    GstIteratorResult itret = GST_ITERATOR_OK;

    while (itret == GST_ITERATOR_OK || itret == GST_ITERATOR_RESYNC) {
      itret =
          gst_iterator_foreach (it, (GstIteratorForeachFunction) release_pads,
          sgroup->combiner);
      gst_iterator_resync (it);
    }
    gst_iterator_free (it);
    gst_element_set_state (sgroup->combiner, GST_STATE_NULL);
    gst_bin_remove ((GstBin *) ebin, sgroup->combiner);
  }

  if (sgroup->splitter) {
    GstIterator *it = gst_element_iterate_src_pads (sgroup->splitter);
    GstIteratorResult itret = GST_ITERATOR_OK;
    while (itret == GST_ITERATOR_OK || itret == GST_ITERATOR_RESYNC) {
      itret =
          gst_iterator_foreach (it, (GstIteratorForeachFunction) release_pads,
          sgroup->splitter);
      gst_iterator_resync (it);
    }
    gst_iterator_free (it);

    gst_element_set_state (sgroup->splitter, GST_STATE_NULL);
    gst_bin_remove ((GstBin *) ebin, sgroup->splitter);
  }

  if (sgroup->inqueue)
    gst_bin_remove ((GstBin *) ebin, sgroup->inqueue);

  if (sgroup->encoder)
    gst_bin_remove ((GstBin *) ebin, sgroup->encoder);

  if (sgroup->fakesink)
    gst_bin_remove ((GstBin *) ebin, sgroup->fakesink);

  if (sgroup->smartencoder)
    gst_bin_remove ((GstBin *) ebin, sgroup->smartencoder);

  if (sgroup->outfilter)
    gst_bin_remove ((GstBin *) ebin, sgroup->outfilter);

  g_free (sgroup);
}

static void
stream_group_remove (GstEncodeBaseBin * ebin, StreamGroup * sgroup)
{
  ebin->streams = g_list_remove (ebin->streams, sgroup);

  stream_group_free (ebin, sgroup);
}

static void
gst_encode_base_bin_tear_down_profile (GstEncodeBaseBin * ebin)
{
  GstElement *element = GST_ELEMENT (ebin);

  if (G_UNLIKELY (ebin->profile == NULL))
    return;

  GST_DEBUG ("Tearing down profile %s",
      gst_encoding_profile_get_name (ebin->profile));

  while (ebin->streams)
    stream_group_remove (ebin, (StreamGroup *) ebin->streams->data);

  if (ebin->srcpad) {
    /* Set ghostpad target to NULL */
    gst_ghost_pad_set_target (GST_GHOST_PAD (ebin->srcpad), NULL);
  }

  /* Remove muxer if present */
  if (ebin->muxer) {
    g_signal_handlers_disconnect_by_func (ebin->profile,
        set_element_properties_from_encoding_profile, ebin->muxer);
    gst_element_set_state (ebin->muxer, GST_STATE_NULL);
    gst_bin_remove (GST_BIN (ebin), ebin->muxer);
    ebin->muxer = NULL;
  }

  if (!element->srcpads) {
    while (element->srcpads)
      gst_element_remove_pad (element, element->srcpads->data);
  }

  /* free/clear profile */
  gst_encoding_profile_unref (ebin->profile);
  ebin->profile = NULL;
}

static gboolean
gst_encode_base_bin_setup_profile (GstEncodeBaseBin * ebin,
    GstEncodingProfile * profile)
{
  gboolean res;

  g_return_val_if_fail (ebin->profile == NULL, FALSE);

  GST_DEBUG ("Setting up profile %p:%s (type:%s)", profile,
      gst_encoding_profile_get_name (profile),
      gst_encoding_profile_get_type_nick (profile));

  ebin->profile = profile;
  gst_object_ref (ebin->profile);

  /* Create elements */
  res = create_elements_and_pads (ebin);
  if (!res)
    gst_encode_base_bin_tear_down_profile (ebin);

  return res;
}

static gboolean
gst_encode_base_bin_set_profile (GstEncodeBaseBin * ebin,
    GstEncodingProfile * profile)
{
  g_return_val_if_fail (GST_IS_ENCODING_PROFILE (profile), FALSE);

  GST_DEBUG_OBJECT (ebin, "profile (%p) : %s", profile,
      gst_encoding_profile_get_name (profile));

  if (G_UNLIKELY (ebin->active)) {
    GST_WARNING_OBJECT (ebin, "Element already active, can't change profile");
    return FALSE;
  }

  /* If we're not active, we can deactivate the previous profile */
  if (ebin->profile) {
    gst_encode_base_bin_tear_down_profile (ebin);
  }

  return gst_encode_base_bin_setup_profile (ebin, profile);
}

static inline gboolean
gst_encode_base_bin_activate (GstEncodeBaseBin * ebin)
{
  ebin->active = ebin->profile != NULL;
  return ebin->active;
}

static void
gst_encode_base_bin_deactivate (GstEncodeBaseBin * ebin)
{
  GList *tmp;

  for (tmp = ebin->streams; tmp; tmp = tmp->next) {
    StreamGroup *sgroup = tmp->data;
    GstCaps *format = gst_encoding_profile_get_format (sgroup->profile);

    _set_group_caps_format (sgroup, sgroup->profile, format);

    if (format)
      gst_caps_unref (format);
  }

  ebin->active = FALSE;
}

static GstStateChangeReturn
gst_encode_base_bin_change_state (GstElement * element,
    GstStateChange transition)
{
  GstStateChangeReturn ret;
  GstEncodeBaseBin *ebin = (GstEncodeBaseBin *) element;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      if (!gst_encode_base_bin_activate (ebin)) {
        ret = GST_STATE_CHANGE_FAILURE;
        goto beach;
      }
      break;
    default:
      break;
  }

  ret =
      GST_ELEMENT_CLASS (gst_encode_base_bin_parent_class)->change_state
      (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    goto beach;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_encode_base_bin_deactivate (ebin);
      break;
    default:
      break;
  }

beach:
  return ret;
}
