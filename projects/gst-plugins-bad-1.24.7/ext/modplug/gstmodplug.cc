/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
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

/* 
   Code based on modplugxmms
   XMMS plugin:
     Kenton Varda <temporal@gauge3d.org>
   Sound Engine:
     Olivier Lapicque <olivierl@jps.net>  
*/

/**
 * SECTION:element-modplug
 * 
 * Modplug uses the [modplug](http://modplug-xmms.sourceforge.net/) library to
 * decode tracked music in the MOD/S3M/XM/IT and related formats.
 * 
 * ## Example pipeline
 *
 * |[
 * gst-launch-1.0 -v filesrc location=1990s-nostalgia.xm ! modplug ! audioconvert ! alsasink
 * ]| Play a FastTracker xm file.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* Required to not get an undefined warning
 * https://bugzilla.gnome.org/show_bug.cgi?id=613795
 */
#ifndef WORDS_BIGENDIAN
#define WORDS_BIGENDIAN 0
#endif

#include <libmodplug/stdafx.h>
#include <libmodplug/sndfile.h>

#include "gstmodplug.h"

#include <gst/gst.h>
#include <stdlib.h>
#include <gst/audio/audio.h>

GST_DEBUG_CATEGORY_STATIC (modplug_debug);
#define GST_CAT_DEFAULT modplug_debug

enum
{
  ARG_0,
  ARG_SONGNAME,
  ARG_REVERB,
  ARG_REVERB_DEPTH,
  ARG_REVERB_DELAY,
  ARG_MEGABASS,
  ARG_MEGABASS_AMOUNT,
  ARG_MEGABASS_RANGE,
  ARG_NOISE_REDUCTION,
  ARG_SURROUND,
  ARG_SURROUND_DEPTH,
  ARG_SURROUND_DELAY,
  ARG_OVERSAMP
};

#define DEFAULT_REVERB           FALSE
#define DEFAULT_REVERB_DEPTH     30
#define DEFAULT_REVERB_DELAY     100
#define DEFAULT_MEGABASS         FALSE
#define DEFAULT_MEGABASS_AMOUNT  40
#define DEFAULT_MEGABASS_RANGE   30
#define DEFAULT_SURROUND         TRUE
#define DEFAULT_SURROUND_DEPTH   20
#define DEFAULT_SURROUND_DELAY   20
#define DEFAULT_OVERSAMP         TRUE
#define DEFAULT_NOISE_REDUCTION  TRUE

#define FORMATS "{ " GST_AUDIO_NE (S32) ", " GST_AUDIO_NE (S16) ", U8 }"

static GstStaticPadTemplate modplug_src_template_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw,"
        " format = (string) " FORMATS ", "
        " layout = (string) interleaved, "
        " rate = (int) { 8000, 11025, 22050, 44100 },"
        " channels = (int) [ 1, 2 ]"));

static GstStaticPadTemplate modplug_sink_template_factory =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-mod; audio/x-xm; audio/x-it; audio/x-s3m; "
        "audio/x-stm"));

static void gst_modplug_dispose (GObject * object);
static void gst_modplug_set_property (GObject * object,
    guint id, const GValue * value, GParamSpec * pspec);
static void gst_modplug_get_property (GObject * object,
    guint id, GValue * value, GParamSpec * pspec);

static gboolean gst_modplug_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_modplug_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static GstStateChangeReturn gst_modplug_change_state (GstElement * element,
    GstStateChange transition);

static gboolean gst_modplug_sinkpad_activate (GstPad * pad, GstObject * parent);
static gboolean gst_modplug_sinkpad_activate_mode (GstPad * pad,
    GstObject * parent, GstPadMode mode, gboolean active);
static void gst_modplug_loop (GstModPlug * element);

#define parent_class gst_modplug_parent_class
G_DEFINE_TYPE (GstModPlug, gst_modplug, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE (modplug, "modplug",
    GST_RANK_PRIMARY, GST_TYPE_MODPLUG);

static void
gst_modplug_class_init (GstModPlugClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_modplug_set_property;
  gobject_class->get_property = gst_modplug_get_property;
  gobject_class->dispose = gst_modplug_dispose;

  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_SONGNAME,
      g_param_spec_string ("songname", "Songname", "The song name",
          NULL, (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_REVERB,
      g_param_spec_boolean ("reverb", "reverb", "Reverb",
          DEFAULT_REVERB,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_REVERB_DEPTH,
      g_param_spec_int ("reverb-depth", "reverb depth", "Reverb depth",
          0, 100, DEFAULT_REVERB_DEPTH,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_REVERB_DELAY,
      g_param_spec_int ("reverb-delay", "reverb delay", "Reverb delay",
          0, 200, DEFAULT_REVERB_DELAY,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_MEGABASS,
      g_param_spec_boolean ("megabass", "megabass", "Megabass",
          DEFAULT_MEGABASS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_MEGABASS_AMOUNT,
      g_param_spec_int ("megabass-amount", "megabass amount", "Megabass amount",
          0, 100, DEFAULT_MEGABASS_AMOUNT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_MEGABASS_RANGE,
      g_param_spec_int ("megabass-range", "megabass range", "Megabass range",
          0, 100, DEFAULT_MEGABASS_RANGE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_SURROUND,
      g_param_spec_boolean ("surround", "surround", "Surround",
          DEFAULT_SURROUND,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_SURROUND_DEPTH,
      g_param_spec_int ("surround-depth", "surround depth", "Surround depth",
          0, 100, DEFAULT_SURROUND_DEPTH,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_SURROUND_DELAY,
      g_param_spec_int ("surround-delay", "surround delay", "Surround delay",
          0, 40, DEFAULT_SURROUND_DELAY,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_OVERSAMP,
      g_param_spec_boolean ("oversamp", "oversamp", "oversamp",
          DEFAULT_OVERSAMP,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_NOISE_REDUCTION,
      g_param_spec_boolean ("noise-reduction", "noise reduction",
          "noise reduction", DEFAULT_NOISE_REDUCTION,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  gstelement_class->change_state = gst_modplug_change_state;

  gst_element_class_add_static_pad_template (gstelement_class, &modplug_sink_template_factory);
  gst_element_class_add_static_pad_template (gstelement_class, &modplug_src_template_factory);

  gst_element_class_set_static_metadata (gstelement_class, "ModPlug",
      "Codec/Decoder/Audio", "Module decoder based on modplug engine",
      "Jeremy SIMON <jsimon13@yahoo.fr>");

  GST_DEBUG_CATEGORY_INIT (modplug_debug, "modplug", 0, "ModPlug element");
}

static void
gst_modplug_init (GstModPlug * modplug)
{
  /* create the sink and src pads */
  modplug->sinkpad =
      gst_pad_new_from_static_template (&modplug_sink_template_factory, "sink");
  gst_pad_set_activate_function (modplug->sinkpad,
      GST_DEBUG_FUNCPTR (gst_modplug_sinkpad_activate));
  gst_pad_set_activatemode_function (modplug->sinkpad,
      GST_DEBUG_FUNCPTR (gst_modplug_sinkpad_activate_mode));
  gst_element_add_pad (GST_ELEMENT (modplug), modplug->sinkpad);

  modplug->srcpad =
      gst_pad_new_from_static_template (&modplug_src_template_factory, "src");
  gst_pad_set_event_function (modplug->srcpad,
      GST_DEBUG_FUNCPTR (gst_modplug_src_event));
  gst_pad_set_query_function (modplug->srcpad,
      GST_DEBUG_FUNCPTR (gst_modplug_src_query));
  gst_element_add_pad (GST_ELEMENT (modplug), modplug->srcpad);

  modplug->reverb = DEFAULT_REVERB;
  modplug->reverb_depth = DEFAULT_REVERB_DEPTH;
  modplug->reverb_delay = DEFAULT_REVERB_DELAY;
  modplug->megabass = DEFAULT_MEGABASS;
  modplug->megabass_amount = DEFAULT_MEGABASS_AMOUNT;
  modplug->megabass_range = DEFAULT_MEGABASS_RANGE;
  modplug->surround = DEFAULT_SURROUND;
  modplug->surround_depth = DEFAULT_SURROUND_DEPTH;
  modplug->surround_delay = DEFAULT_SURROUND_DELAY;
  modplug->oversamp = DEFAULT_OVERSAMP;
  modplug->noise_reduction = DEFAULT_NOISE_REDUCTION;

  modplug->bits = 16;
  modplug->channel = 2;
  modplug->frequency = 44100;
}


static void
gst_modplug_dispose (GObject * object)
{
  GstModPlug *modplug = GST_MODPLUG (object);

  G_OBJECT_CLASS (parent_class)->dispose (object);

  if (modplug->buffer) {
    gst_buffer_unref (modplug->buffer);
    modplug->buffer = NULL;
  }
}

static gboolean
gst_modplug_src_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstModPlug *modplug;
  gboolean res = FALSE;

  modplug = GST_MODPLUG (parent);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_DURATION:
    {
      GstFormat format;

      if (!modplug->mSoundFile)
        goto done;

      gst_query_parse_duration (query, &format, NULL);
      if (format == GST_FORMAT_TIME) {
        gst_query_set_duration (query, format, modplug->song_length);
        res = TRUE;
      }
    }
      break;
    case GST_QUERY_POSITION:
    {
      GstFormat format;

      if (!modplug->mSoundFile)
        goto done;

      gst_query_parse_position (query, &format, NULL);
      if (format == GST_FORMAT_TIME) {
        gint64 pos;
        guint32 max;

	max = modplug->mSoundFile->GetMaxPosition();
	if (max > 0) {
          pos = (modplug->song_length * modplug->mSoundFile->GetCurrentPos ()) /
            max;
          gst_query_set_position (query, format, pos);
          res = TRUE;
        }
      }
    }
      break;
    default:
      res = gst_pad_query_default (pad, parent, query);
      break;
  }

done:
  return res;
}

static gboolean
gst_modplug_do_seek (GstModPlug * modplug, GstEvent * event)
{
  gdouble rate;
  GstFormat format;
  GstSeekFlags flags;
  GstSeekType cur_type, stop_type;
  gboolean flush;
  gint64 cur, stop;
  GstSegment seg;

  if (modplug->frequency == 0)
    goto no_song;

  gst_event_parse_seek (event, &rate, &format, &flags, &cur_type, &cur,
      &stop_type, &stop);

  if (format != GST_FORMAT_TIME)
    goto no_time;

  /* FIXME: we should be using GstSegment for all this */
  if (cur_type != GST_SEEK_TYPE_SET || stop_type != GST_SEEK_TYPE_NONE)
    goto not_supported;

  if (stop_type == GST_SEEK_TYPE_NONE)
    stop = GST_CLOCK_TIME_NONE;
  if (!GST_CLOCK_TIME_IS_VALID (stop) && modplug->song_length > 0)
    stop = modplug->song_length;

  cur = CLAMP (cur, -1, modplug->song_length);

  GST_DEBUG_OBJECT (modplug, "seek to %" GST_TIME_FORMAT,
      GST_TIME_ARGS ((guint64) cur));

  modplug->seek_at = cur;

  flush = ((flags & GST_SEEK_FLAG_FLUSH) == GST_SEEK_FLAG_FLUSH);

  if (flush) {
    gst_pad_push_event (modplug->srcpad, gst_event_new_flush_start ());
  } else {
    gst_pad_stop_task (modplug->sinkpad);
  }

  GST_PAD_STREAM_LOCK (modplug->sinkpad);

  if (flags & GST_SEEK_FLAG_SEGMENT) {
    gst_element_post_message (GST_ELEMENT (modplug),
        gst_message_new_segment_start (GST_OBJECT (modplug), format, cur));
  }

  if (flush) {
    gst_pad_push_event (modplug->srcpad, gst_event_new_flush_stop (TRUE));
  }

  GST_LOG_OBJECT (modplug, "sending newsegment from %" GST_TIME_FORMAT "-%"
      GST_TIME_FORMAT ", pos=%" GST_TIME_FORMAT,
      GST_TIME_ARGS ((guint64) cur), GST_TIME_ARGS ((guint64) stop),
      GST_TIME_ARGS ((guint64) cur));

  gst_segment_init (&seg, GST_FORMAT_TIME);
  seg.rate = rate;
  seg.start = cur;
  seg.stop = stop;
  seg.time = cur;
  gst_pad_push_event (modplug->srcpad, gst_event_new_segment (&seg));

  modplug->offset =
      gst_util_uint64_scale_int (cur, modplug->frequency, GST_SECOND);

  gst_pad_start_task (modplug->sinkpad,
      (GstTaskFunction) gst_modplug_loop, modplug, NULL);

  GST_PAD_STREAM_UNLOCK (modplug->sinkpad);

  return TRUE;

  /* ERROR */
no_song:
  {
    GST_DEBUG_OBJECT (modplug, "no song loaded yet");
    return FALSE;
  }
no_time:
  {
    GST_DEBUG_OBJECT (modplug, "seeking is only supported in TIME format");
    return FALSE;
  }
not_supported:
  {
    GST_DEBUG_OBJECT (modplug, "unsupported seek type");
    return FALSE;
  }
}

static gboolean
gst_modplug_src_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstModPlug *modplug;
  gboolean res = FALSE;

  modplug = GST_MODPLUG (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
      res = gst_modplug_do_seek (modplug, event);
      break;
    default:
      res = gst_pad_event_default (pad, parent, event);
      break;
  }
  return res;
}

static gboolean
gst_modplug_load_song (GstModPlug * modplug)
{
  GstCaps *newcaps;
  GstStructure *structure;
  GstMapInfo map;
  const gchar *format;

  GST_DEBUG_OBJECT (modplug, "Setting caps");

  /* negotiate srcpad caps */
  if ((newcaps = gst_pad_get_allowed_caps (modplug->srcpad)) == NULL) {
    newcaps = gst_pad_get_pad_template_caps (modplug->srcpad);
  }
  newcaps = gst_caps_make_writable (newcaps);

  GST_DEBUG_OBJECT (modplug, "allowed caps %" GST_PTR_FORMAT, newcaps);

  structure = gst_caps_get_structure (newcaps, 0);

  if (!gst_structure_fixate_field_string (structure, "format",
          GST_AUDIO_NE (S16)))
    GST_WARNING_OBJECT (modplug, "Failed to fixate format to S16NE");
  if (!gst_structure_fixate_field_nearest_int (structure, "rate", 44100))
    GST_WARNING_OBJECT (modplug, "Failed to fixate rate to 44100");
  if (!gst_structure_fixate_field_nearest_int (structure, "channels", 2))
    GST_WARNING_OBJECT (modplug,
        "Failed to fixate number of channels to stereo");

  GST_DEBUG_OBJECT (modplug, "normalized caps %" GST_PTR_FORMAT, newcaps);

  newcaps = gst_caps_fixate (newcaps);

  GST_DEBUG_OBJECT (modplug, "fixated caps %" GST_PTR_FORMAT, newcaps);

  /* set up modplug to output the negotiated format */
  structure = gst_caps_get_structure (newcaps, 0);
  format = gst_structure_get_string (structure, "format");

  if (g_str_equal (format, GST_AUDIO_NE (S32)))
    modplug->bits = 32;
  else if (g_str_equal (format, GST_AUDIO_NE (S16)))
    modplug->bits = 16;
  else
    modplug->bits = 8;

  gst_structure_get_int (structure, "channels", &modplug->channel);
  gst_structure_get_int (structure, "rate", &modplug->frequency);

  GST_DEBUG_OBJECT (modplug,
      "Audio settings: %d bits, %d channel(s), %d Hz sampling rate",
      modplug->bits, modplug->channel, modplug->frequency);

  gst_pad_set_caps (modplug->srcpad, newcaps);
  gst_caps_unref (newcaps);

  modplug->read_samples = 1152;
  modplug->read_bytes =
      modplug->read_samples * modplug->channel * modplug->bits / 8;

  GST_DEBUG_OBJECT (modplug, "Loading song");

  modplug->mSoundFile = new CSoundFile;

  modplug->mSoundFile->SetWaveConfig (modplug->frequency, modplug->bits,
      modplug->channel);

  modplug->mSoundFile->SetWaveConfigEx (modplug->surround, !modplug->oversamp,
      modplug->reverb, true, modplug->megabass, modplug->noise_reduction, true);
  modplug->mSoundFile->SetResamplingMode (SRCMODE_POLYPHASE);

  if (modplug->surround)
    modplug->mSoundFile->SetSurroundParameters (modplug->surround_depth,
        modplug->surround_delay);

  if (modplug->megabass)
    modplug->mSoundFile->SetXBassParameters (modplug->megabass_amount,
        modplug->megabass_range);

  if (modplug->reverb)
    modplug->mSoundFile->SetReverbParameters (modplug->reverb_depth,
        modplug->reverb_delay);


  gst_buffer_map (modplug->buffer, &map, GST_MAP_READ);
  if (!modplug->mSoundFile->Create (map.data, modplug->song_size))
    goto load_error;
  gst_buffer_unmap (modplug->buffer, &map);

  modplug->song_length = modplug->mSoundFile->GetSongTime () * GST_SECOND;
  modplug->seek_at = -1;

  GST_INFO_OBJECT (modplug, "Song length: %" GST_TIME_FORMAT,
      GST_TIME_ARGS ((guint64) modplug->song_length));

  return TRUE;

  /* ERRORS */
load_error:
  {
    gst_buffer_unmap (modplug->buffer, &map);
    GST_ELEMENT_ERROR (modplug, STREAM, DECODE, (NULL),
        ("Unable to load song"));
    return FALSE;
  }
}

static gboolean
gst_modplug_sinkpad_activate (GstPad * sinkpad, GstObject * parent)
{
  GstQuery *query;
  gboolean pull_mode;

  query = gst_query_new_scheduling ();

  if (!gst_pad_peer_query (sinkpad, query)) {
    gst_query_unref (query);
    goto activate_push;
  }

  pull_mode = gst_query_has_scheduling_mode_with_flags (query,
      GST_PAD_MODE_PULL, GST_SCHEDULING_FLAG_SEEKABLE);
  gst_query_unref (query);

  if (!pull_mode)
    goto activate_push;

  GST_DEBUG_OBJECT (sinkpad, "activating pull");
  return gst_pad_activate_mode (sinkpad, GST_PAD_MODE_PULL, TRUE);

activate_push:
  {
    GST_DEBUG_OBJECT (sinkpad, "activating push");
    return gst_pad_activate_mode (sinkpad, GST_PAD_MODE_PUSH, TRUE);
  }
}

static gboolean
gst_modplug_sinkpad_activate_mode (GstPad * pad, GstObject * parent,
    GstPadMode mode, gboolean active)
{
  GstModPlug *modplug = GST_MODPLUG (parent);
  gboolean res;

  switch (mode) {
    case GST_PAD_MODE_PUSH:
      res = TRUE;
      break;
    case GST_PAD_MODE_PULL:
      if (active) {
        res = gst_pad_start_task (pad, (GstTaskFunction) gst_modplug_loop,
            modplug, NULL);
      } else {
        res = gst_pad_stop_task (pad);
      }
      break;
    default:
      res = FALSE;
      break;
  }
  return res;
}

static gboolean
gst_modplug_get_upstream_size (GstModPlug * modplug, gint64 * length)
{
  gboolean res = FALSE;
  GstPad *peer;

  peer = gst_pad_get_peer (modplug->sinkpad);
  if (peer == NULL)
    return FALSE;

  if (gst_pad_query_duration (peer, GST_FORMAT_BYTES, length) && *length >= 0) {
    res = TRUE;
  }

  gst_object_unref (peer);
  return res;
}

static void
gst_modplug_loop (GstModPlug * modplug)
{
  GstFlowReturn flow;
  GstBuffer *out = NULL;
  GstMapInfo map;

  g_assert (GST_IS_MODPLUG (modplug));

  /* first, get the size of the song */
  if (!modplug->song_size) {
    if (!gst_modplug_get_upstream_size (modplug, &modplug->song_size)) {
      GST_ELEMENT_ERROR (modplug, STREAM, DECODE, (NULL),
          ("Unable to load song"));
      goto pause;
    }

    if (modplug->buffer) {
      gst_buffer_unref (modplug->buffer);
    }
    modplug->buffer = gst_buffer_new_and_alloc (modplug->song_size);
    modplug->offset = 0;
  }

  /* read in the song data */
  if (!modplug->mSoundFile) {
    GstBuffer *buffer = NULL;
    guint64 read_size = modplug->song_size - modplug->offset;

    if (read_size > 4096)
      read_size = 4096;

    flow =
        gst_pad_pull_range (modplug->sinkpad, modplug->offset, read_size,
        &buffer);
    if (flow != GST_FLOW_OK) {
      GST_ELEMENT_ERROR (modplug, STREAM, DECODE, (NULL),
          ("Unable to load song"));
      goto pause;
    }

    /* GST_LOG_OBJECT (modplug, "Read %u bytes", GST_BUFFER_SIZE (buffer)); */
    gst_buffer_map (buffer, &map, GST_MAP_READ);
    gst_buffer_fill (modplug->buffer, modplug->offset, map.data, map.size);
    gst_buffer_unmap (buffer, &map);
    gst_buffer_unref (buffer);

    modplug->offset += read_size;

    /* actually load it */
    if (modplug->offset == modplug->song_size) {
      GstTagList *tags;
      gboolean ok;
#define COMMENT_SIZE 16384
      gchar comment[COMMENT_SIZE];
      GstSegment seg;

      ok = gst_modplug_load_song (modplug);
      gst_buffer_unref (modplug->buffer);
      modplug->buffer = NULL;
      modplug->offset = 0;

      if (!ok) {
        goto pause;
      }

      gst_segment_init (&seg, GST_FORMAT_TIME);
      seg.stop = modplug->song_length;
      gst_pad_push_event (modplug->srcpad, gst_event_new_segment (&seg));

      /* get and send metadata */
      tags = gst_tag_list_new_empty ();
      gst_tag_list_add (tags, GST_TAG_MERGE_APPEND,
          GST_TAG_TITLE, modplug->mSoundFile->GetTitle (),
          GST_TAG_BEATS_PER_MINUTE,
          (gdouble) modplug->mSoundFile->GetMusicTempo (), NULL);

      if (modplug->mSoundFile->GetSongComments ((gchar *) & comment,
              COMMENT_SIZE, 32)) {
        comment[COMMENT_SIZE - 1] = '\0';
        gst_tag_list_add (tags, GST_TAG_MERGE_APPEND,
            GST_TAG_COMMENT, comment, NULL);
      }
      gst_pad_push_event (modplug->srcpad, gst_event_new_tag (tags));
    } else {
      /* not fully loaded yet */
      return;
    }
  }

  /* could move this to gst_modplug_src_event 
   * if libmodplug was definitely thread safe.. */
  if (modplug->seek_at != -1) {
    gint seek_to_pos;
    gfloat temp;

    temp = (gfloat) modplug->song_length / modplug->seek_at;
    seek_to_pos = (gint) (modplug->mSoundFile->GetMaxPosition () / temp);

    GST_DEBUG_OBJECT (modplug, "Seeking to row %d", seek_to_pos);

    modplug->mSoundFile->SetCurrentPos (seek_to_pos);
    modplug->seek_at = -1;
  }

  /* read and output a buffer */
  GST_LOG_OBJECT (modplug, "Read %d bytes", (gint) modplug->read_bytes);
  /* libmodplug 0.8.7 trashes memory */
  out = gst_buffer_new_allocate (NULL, modplug->read_bytes * 2, NULL);

  gst_buffer_map (out, &map, GST_MAP_WRITE);
  if (!modplug->mSoundFile->Read (map.data, modplug->read_bytes)) {
    gst_buffer_unmap (out, &map);
    goto eos;
  }
  gst_buffer_unmap (out, &map);
  gst_buffer_resize (out, 0, modplug->read_bytes);

  GST_BUFFER_DURATION (out) =
      gst_util_uint64_scale_int (modplug->read_samples, GST_SECOND,
      modplug->frequency);
  GST_BUFFER_OFFSET (out) = modplug->offset;
  GST_BUFFER_TIMESTAMP (out) =
      gst_util_uint64_scale_int (modplug->offset, GST_SECOND,
      modplug->frequency);

  modplug->offset += modplug->read_samples;

  flow = gst_pad_push (modplug->srcpad, out);

  if (flow != GST_FLOW_OK) {
    GST_LOG_OBJECT (modplug, "pad push flow: %s", gst_flow_get_name (flow));
    goto pause;
  }

  return;

eos:
  {
    gst_buffer_unref (out);
    GST_INFO_OBJECT (modplug, "EOS");
    gst_pad_push_event (modplug->srcpad, gst_event_new_eos ());
    goto pause;
  }

pause:
  {
    GST_INFO_OBJECT (modplug, "Pausing");
    gst_pad_pause_task (modplug->sinkpad);
  }
}


static GstStateChangeReturn
gst_modplug_change_state (GstElement * element, GstStateChange transition)
{
  GstModPlug *modplug;
  GstStateChangeReturn ret;

  modplug = GST_MODPLUG (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      modplug->buffer = NULL;
      modplug->offset = 0;
      modplug->song_size = 0;
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      if (modplug->buffer) {
        gst_buffer_unref (modplug->buffer);
        modplug->buffer = NULL;
      }
      if (modplug->mSoundFile) {
        modplug->mSoundFile->Destroy ();
        delete modplug->mSoundFile;
        modplug->mSoundFile = NULL;
      }
      break;
    default:
      break;
  }

  return GST_STATE_CHANGE_SUCCESS;
}


static void
gst_modplug_set_property (GObject * object, guint id, const GValue * value,
    GParamSpec * pspec)
{
  GstModPlug *modplug;

  g_return_if_fail (GST_IS_MODPLUG (object));
  modplug = GST_MODPLUG (object);

  switch (id) {
    case ARG_REVERB:
      modplug->reverb = g_value_get_boolean (value);
      break;
    case ARG_REVERB_DEPTH:
      modplug->reverb_depth = g_value_get_int (value);
      break;
    case ARG_REVERB_DELAY:
      modplug->reverb_delay = g_value_get_int (value);
      break;
    case ARG_MEGABASS:
      modplug->megabass = g_value_get_boolean (value);
      break;
    case ARG_MEGABASS_AMOUNT:
      modplug->megabass_amount = g_value_get_int (value);
      break;
    case ARG_MEGABASS_RANGE:
      modplug->megabass_range = g_value_get_int (value);
      break;
    case ARG_NOISE_REDUCTION:
      modplug->noise_reduction = g_value_get_boolean (value);
      break;
    case ARG_SURROUND:
      modplug->surround = g_value_get_boolean (value);
      break;
    case ARG_SURROUND_DEPTH:
      modplug->surround_depth = g_value_get_int (value);
      break;
    case ARG_SURROUND_DELAY:
      modplug->surround_delay = g_value_get_int (value);
      break;
    default:
      break;
  }
}

static void
gst_modplug_get_property (GObject * object, guint id, GValue * value,
    GParamSpec * pspec)
{
  GstModPlug *modplug;

  g_return_if_fail (GST_IS_MODPLUG (object));
  modplug = GST_MODPLUG (object);

  switch (id) {
    case ARG_REVERB:
      g_value_set_boolean (value, modplug->reverb);
      break;
    case ARG_REVERB_DEPTH:
      g_value_set_int (value, modplug->reverb_depth);
      break;
    case ARG_REVERB_DELAY:
      g_value_set_int (value, modplug->reverb_delay);
      break;
    case ARG_MEGABASS:
      g_value_set_boolean (value, modplug->megabass);
      break;
    case ARG_MEGABASS_AMOUNT:
      g_value_set_int (value, modplug->megabass_amount);
      break;
    case ARG_MEGABASS_RANGE:
      g_value_set_int (value, modplug->megabass_range);
      break;
    case ARG_SURROUND:
      g_value_set_boolean (value, modplug->surround);
      break;
    case ARG_SURROUND_DEPTH:
      g_value_set_int (value, modplug->surround_depth);
      break;
    case ARG_SURROUND_DELAY:
      g_value_set_int (value, modplug->surround_delay);
      break;
    case ARG_NOISE_REDUCTION:
      g_value_set_boolean (value, modplug->noise_reduction);
      break;
    default:
      break;
  }
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (modplug, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    modplug,
    ".MOD audio decoding",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
