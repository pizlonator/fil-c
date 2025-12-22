/* GStreamer LADSPA source category
 * Copyright (C) 1999 Erik Walthinsen <omega@cse.ogi.edu>
 *               2001 Steve Baker <stevebaker_org@yahoo.co.uk>
 *               2003 Andy Wingo <wingo at pobox.com>
 * Copyright (C) 2005 Stefan Kost <ensonic@users.sf.net> (audiotestsrc)
 * Copyright (C) 2013 Juan Manuel Borges Caño <juanmabcmail@gmail.com>
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

#include "gstladspasource.h"
#include "gstladspa.h"
#include "gstladspautils.h"
#include <gst/base/gstbasetransform.h>

GST_DEBUG_CATEGORY_EXTERN (ladspa_debug);
#define GST_CAT_DEFAULT ladspa_debug

#define GST_LADSPA_SOURCE_CLASS_TAGS                   "Source/Audio/LADSPA"
#define GST_LADSPA_SOURCE_DEFAULT_SAMPLES_PER_BUFFER   1024
#define GST_LADSPA_SOURCE_DEFAULT_IS_LIVE              FALSE
#define GST_LADSPA_SOURCE_DEFAULT_TIMESTAMP_OFFSET     G_GINT64_CONSTANT (0)
#define GST_LADSPA_SOURCE_DEFAULT_CAN_ACTIVATE_PUSH    TRUE
#define GST_LADSPA_SOURCE_DEFAULT_CAN_ACTIVATE_PULL    FALSE

enum
{
  GST_LADSPA_SOURCE_PROP_0,
  GST_LADSPA_SOURCE_PROP_SAMPLES_PER_BUFFER,
  GST_LADSPA_SOURCE_PROP_IS_LIVE,
  GST_LADSPA_SOURCE_PROP_TIMESTAMP_OFFSET,
  GST_LADSPA_SOURCE_PROP_CAN_ACTIVATE_PUSH,
  GST_LADSPA_SOURCE_PROP_CAN_ACTIVATE_PULL,
  GST_LADSPA_SOURCE_PROP_LAST
};

static GstLADSPASourceClass *gst_ladspa_source_type_parent_class = NULL;

/*
 * Boilerplates BaseSrc add pad.
 */
void
gst_my_base_source_class_add_pad_template (GstBaseSrcClass * base_class,
    GstCaps * srccaps)
{
  GstElementClass *elem_class = GST_ELEMENT_CLASS (base_class);
  GstPadTemplate *pad_template;

  g_return_if_fail (GST_IS_CAPS (srccaps));

  pad_template =
      gst_pad_template_new (GST_BASE_TRANSFORM_SRC_NAME, GST_PAD_SRC,
      GST_PAD_ALWAYS, srccaps);
  gst_element_class_add_pad_template (elem_class, pad_template);
}

static GstCaps *
gst_ladspa_source_type_fixate (GstBaseSrc * base, GstCaps * caps)
{
  GstLADSPASource *ladspa = GST_LADSPA_SOURCE (base);
  GstStructure *structure;

  caps = gst_caps_make_writable (caps);

  structure = gst_caps_get_structure (caps, 0);

  GST_DEBUG_OBJECT (ladspa, "fixating samplerate to %d", GST_AUDIO_DEF_RATE);

  gst_structure_fixate_field_nearest_int (structure, "rate",
      GST_AUDIO_DEF_RATE);

  gst_structure_fixate_field_string (structure, "format", GST_AUDIO_NE (F32));

  gst_structure_fixate_field_nearest_int (structure, "channels",
      ladspa->ladspa.klass->count.audio.out);

  caps =
      GST_BASE_SRC_CLASS (gst_ladspa_source_type_parent_class)->fixate
      (base, caps);

  return caps;
}

static gboolean
gst_ladspa_source_type_set_caps (GstBaseSrc * base, GstCaps * caps)
{
  GstLADSPASource *ladspa = GST_LADSPA_SOURCE (base);
  GstAudioInfo info;

  if (!gst_audio_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (base, "received invalid caps");
    return FALSE;
  }

  GST_DEBUG_OBJECT (ladspa, "negotiated to caps %" GST_PTR_FORMAT, caps);

  ladspa->info = info;

  gst_base_src_set_blocksize (base,
      GST_AUDIO_INFO_BPF (&info) * ladspa->samples_per_buffer);

  return gst_ladspa_setup (&ladspa->ladspa, GST_AUDIO_INFO_RATE (&info));
}

static gboolean
gst_ladspa_source_type_query (GstBaseSrc * base, GstQuery * query)
{
  GstLADSPASource *ladspa = GST_LADSPA_SOURCE (base);
  gboolean res = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONVERT:
    {
      GstFormat src_fmt, dest_fmt;
      gint64 src_val, dest_val;

      gst_query_parse_convert (query, &src_fmt, &src_val, &dest_fmt, &dest_val);

      if (!gst_audio_info_convert (&ladspa->info, src_fmt, src_val, dest_fmt,
              &dest_val)) {
        GST_DEBUG_OBJECT (ladspa, "query failed");
        return FALSE;
      }

      gst_query_set_convert (query, src_fmt, src_val, dest_fmt, dest_val);
      res = TRUE;
      break;
    }
    case GST_QUERY_SCHEDULING:
    {
      /* if we can operate in pull mode */
      gst_query_set_scheduling (query, GST_SCHEDULING_FLAG_SEEKABLE, 1, -1, 0);
      gst_query_add_scheduling_mode (query, GST_PAD_MODE_PUSH);
      if (ladspa->can_activate_pull)
        gst_query_add_scheduling_mode (query, GST_PAD_MODE_PULL);

      res = TRUE;
      break;
    }
    default:
      res =
          GST_BASE_SRC_CLASS (gst_ladspa_source_type_parent_class)->query
          (base, query);
      break;
  }

  return res;
}

static void
gst_ladspa_source_type_get_times (GstBaseSrc * base, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  /* for live sources, sync on the timestamp of the buffer */
  if (gst_base_src_is_live (base)) {
    GstClockTime timestamp = GST_BUFFER_TIMESTAMP (buffer);

    if (GST_CLOCK_TIME_IS_VALID (timestamp)) {
      /* get duration to calculate end time */
      GstClockTime duration = GST_BUFFER_DURATION (buffer);

      if (GST_CLOCK_TIME_IS_VALID (duration)) {
        *end = timestamp + duration;
      }
      *start = timestamp;
    }
  } else {
    *start = -1;
    *end = -1;
  }
}

/* seek to time, will be called when we operate in push mode. In pull mode we
 * get the requested byte offset. */
static gboolean
gst_ladspa_source_type_do_seek (GstBaseSrc * base, GstSegment * segment)
{
  GstLADSPASource *ladspa = GST_LADSPA_SOURCE (base);
  GstClockTime time;
  gint samplerate, bpf;
  gint64 next_sample;

  GST_DEBUG_OBJECT (ladspa, "seeking %" GST_SEGMENT_FORMAT, segment);

  time = segment->position;
  ladspa->reverse = (segment->rate < 0.0);

  samplerate = GST_AUDIO_INFO_RATE (&ladspa->info);
  bpf = GST_AUDIO_INFO_BPF (&ladspa->info);

  /* now move to the time indicated, don't seek to the sample *after* the time */
  next_sample = gst_util_uint64_scale_int (time, samplerate, GST_SECOND);
  ladspa->next_byte = next_sample * bpf;
  if (samplerate == 0)
    ladspa->next_time = 0;
  else
    ladspa->next_time =
        gst_util_uint64_scale_round (next_sample, GST_SECOND, samplerate);

  GST_DEBUG_OBJECT (ladspa, "seeking next_sample=%" G_GINT64_FORMAT
      " next_time=%" GST_TIME_FORMAT, next_sample,
      GST_TIME_ARGS (ladspa->next_time));

  g_assert (ladspa->next_time <= time);

  ladspa->next_sample = next_sample;

  if (!ladspa->reverse) {
    if (GST_CLOCK_TIME_IS_VALID (segment->start)) {
      segment->time = segment->start;
    }
  } else {
    if (GST_CLOCK_TIME_IS_VALID (segment->stop)) {
      segment->time = segment->stop;
    }
  }

  if (GST_CLOCK_TIME_IS_VALID (segment->stop)) {
    time = segment->stop;
    ladspa->sample_stop =
        gst_util_uint64_scale_round (time, samplerate, GST_SECOND);
    ladspa->check_seek_stop = TRUE;
  } else {
    ladspa->check_seek_stop = FALSE;
  }
  ladspa->eos_reached = FALSE;

  return TRUE;
}

static gboolean
gst_ladspa_source_type_is_seekable (GstBaseSrc * base)
{
  /* we're seekable... */
  return TRUE;
}

static GstFlowReturn
gst_ladspa_source_type_fill (GstBaseSrc * base, guint64 offset,
    guint length, GstBuffer * buffer)
{
  GstLADSPASource *ladspa = GST_LADSPA_SOURCE (base);
  GstClockTime next_time;
  gint64 next_sample, next_byte;
  gint bytes, samples;
  GstElementClass *eclass;
  GstMapInfo map;
  gint samplerate, bpf;

  /* example for tagging generated data */
  if (!ladspa->tags_pushed) {
    GstTagList *taglist;

    taglist = gst_tag_list_new (GST_TAG_DESCRIPTION, "ladspa wave", NULL);

    eclass = GST_ELEMENT_CLASS (gst_ladspa_source_type_parent_class);
    if (eclass->send_event)
      eclass->send_event (GST_ELEMENT (base), gst_event_new_tag (taglist));
    else
      gst_tag_list_unref (taglist);
    ladspa->tags_pushed = TRUE;
  }

  if (ladspa->eos_reached) {
    GST_INFO_OBJECT (ladspa, "eos");
    return GST_FLOW_EOS;
  }

  samplerate = GST_AUDIO_INFO_RATE (&ladspa->info);
  bpf = GST_AUDIO_INFO_BPF (&ladspa->info);

  /* if no length was given, use our default length in samples otherwise convert
   * the length in bytes to samples. */
  if (length == -1)
    samples = ladspa->samples_per_buffer;
  else
    samples = length / bpf;

  /* if no offset was given, use our next logical byte */
  if (offset == -1)
    offset = ladspa->next_byte;

  /* now see if we are at the byteoffset we think we are */
  if (offset != ladspa->next_byte) {
    GST_DEBUG_OBJECT (ladspa, "seek to new offset %" G_GUINT64_FORMAT, offset);
    /* we have a discont in the expected sample offset, do a 'seek' */
    ladspa->next_sample = offset / bpf;
    ladspa->next_time =
        gst_util_uint64_scale_int (ladspa->next_sample, GST_SECOND, samplerate);
    ladspa->next_byte = offset;
  }

  /* check for eos */
  if (ladspa->check_seek_stop &&
      (ladspa->sample_stop > ladspa->next_sample) &&
      (ladspa->sample_stop < ladspa->next_sample + samples)
      ) {
    /* calculate only partial buffer */
    ladspa->generate_samples_per_buffer =
        ladspa->sample_stop - ladspa->next_sample;
    next_sample = ladspa->sample_stop;
    ladspa->eos_reached = TRUE;
  } else {
    /* calculate full buffer */
    ladspa->generate_samples_per_buffer = samples;
    next_sample =
        ladspa->next_sample + (ladspa->reverse ? (-samples) : samples);
  }

  bytes = ladspa->generate_samples_per_buffer * bpf;

  next_byte = ladspa->next_byte + (ladspa->reverse ? (-bytes) : bytes);
  next_time = gst_util_uint64_scale_int (next_sample, GST_SECOND, samplerate);

  GST_LOG_OBJECT (ladspa, "samplerate %d", samplerate);
  GST_LOG_OBJECT (ladspa,
      "next_sample %" G_GINT64_FORMAT ", ts %" GST_TIME_FORMAT, next_sample,
      GST_TIME_ARGS (next_time));

  gst_buffer_set_size (buffer, bytes);

  GST_BUFFER_OFFSET (buffer) = ladspa->next_sample;
  GST_BUFFER_OFFSET_END (buffer) = next_sample;
  if (!ladspa->reverse) {
    GST_BUFFER_TIMESTAMP (buffer) =
        ladspa->timestamp_offset + ladspa->next_time;
    GST_BUFFER_DURATION (buffer) = next_time - ladspa->next_time;
  } else {
    GST_BUFFER_TIMESTAMP (buffer) = ladspa->timestamp_offset + next_time;
    GST_BUFFER_DURATION (buffer) = ladspa->next_time - next_time;
  }

  gst_object_sync_values (GST_OBJECT (ladspa), GST_BUFFER_TIMESTAMP (buffer));

  ladspa->next_time = next_time;
  ladspa->next_sample = next_sample;
  ladspa->next_byte = next_byte;

  GST_LOG_OBJECT (ladspa, "generating %u samples at ts %" GST_TIME_FORMAT,
      ladspa->generate_samples_per_buffer,
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)));

  gst_buffer_map (buffer, &map, GST_MAP_WRITE);
  gst_ladspa_transform (&ladspa->ladspa, map.data,
      ladspa->generate_samples_per_buffer, NULL);
  gst_buffer_unmap (buffer, &map);

  return GST_FLOW_OK;
}

static gboolean
gst_ladspa_source_type_start (GstBaseSrc * base)
{
  GstLADSPASource *ladspa = GST_LADSPA_SOURCE (base);

  ladspa->next_sample = 0;
  ladspa->next_byte = 0;
  ladspa->next_time = 0;
  ladspa->check_seek_stop = FALSE;
  ladspa->eos_reached = FALSE;
  ladspa->tags_pushed = FALSE;

  return TRUE;
}

static gboolean
gst_ladspa_source_type_stop (GstBaseSrc * base)
{
  GstLADSPASource *ladspa = GST_LADSPA_SOURCE (base);
  return gst_ladspa_cleanup (&ladspa->ladspa);
}

static void
gst_ladspa_source_type_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstLADSPASource *ladspa = GST_LADSPA_SOURCE (object);

  switch (prop_id) {
    case GST_LADSPA_SOURCE_PROP_SAMPLES_PER_BUFFER:
      ladspa->samples_per_buffer = g_value_get_int (value);
      gst_base_src_set_blocksize (GST_BASE_SRC (ladspa),
          GST_AUDIO_INFO_BPF (&ladspa->info) * ladspa->samples_per_buffer);
      break;
    case GST_LADSPA_SOURCE_PROP_IS_LIVE:
      gst_base_src_set_live (GST_BASE_SRC (ladspa),
          g_value_get_boolean (value));
      break;
    case GST_LADSPA_SOURCE_PROP_TIMESTAMP_OFFSET:
      ladspa->timestamp_offset = g_value_get_int64 (value);
      break;
    case GST_LADSPA_SOURCE_PROP_CAN_ACTIVATE_PUSH:
      GST_BASE_SRC (ladspa)->can_activate_push = g_value_get_boolean (value);
      break;
    case GST_LADSPA_SOURCE_PROP_CAN_ACTIVATE_PULL:
      ladspa->can_activate_pull = g_value_get_boolean (value);
      break;
    default:
      gst_ladspa_object_set_property (&ladspa->ladspa, object, prop_id, value,
          pspec);
      break;
  }
}

static void
gst_ladspa_source_type_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstLADSPASource *ladspa = GST_LADSPA_SOURCE (object);

  switch (prop_id) {
    case GST_LADSPA_SOURCE_PROP_SAMPLES_PER_BUFFER:
      g_value_set_int (value, ladspa->samples_per_buffer);
      break;
    case GST_LADSPA_SOURCE_PROP_IS_LIVE:
      g_value_set_boolean (value, gst_base_src_is_live (GST_BASE_SRC (ladspa)));
      break;
    case GST_LADSPA_SOURCE_PROP_TIMESTAMP_OFFSET:
      g_value_set_int64 (value, ladspa->timestamp_offset);
      break;
    case GST_LADSPA_SOURCE_PROP_CAN_ACTIVATE_PUSH:
      g_value_set_boolean (value, GST_BASE_SRC (ladspa)->can_activate_push);
      break;
    case GST_LADSPA_SOURCE_PROP_CAN_ACTIVATE_PULL:
      g_value_set_boolean (value, ladspa->can_activate_pull);
      break;
    default:
      gst_ladspa_object_get_property (&ladspa->ladspa, object, prop_id, value,
          pspec);
      break;
  }
}

static void
gst_ladspa_source_type_init (GstLADSPASource * ladspa, LADSPA_Descriptor * desc)
{
  GstLADSPASourceClass *ladspa_class = GST_LADSPA_SOURCE_GET_CLASS (ladspa);

  gst_ladspa_init (&ladspa->ladspa, &ladspa_class->ladspa);

  /* we operate in time */
  gst_base_src_set_format (GST_BASE_SRC (ladspa), GST_FORMAT_TIME);
  gst_base_src_set_live (GST_BASE_SRC (ladspa),
      GST_LADSPA_SOURCE_DEFAULT_IS_LIVE);

  ladspa->samples_per_buffer = GST_LADSPA_SOURCE_DEFAULT_SAMPLES_PER_BUFFER;
  ladspa->generate_samples_per_buffer = ladspa->samples_per_buffer;
  ladspa->timestamp_offset = GST_LADSPA_SOURCE_DEFAULT_TIMESTAMP_OFFSET;
  ladspa->can_activate_pull = GST_LADSPA_SOURCE_DEFAULT_CAN_ACTIVATE_PULL;

  gst_base_src_set_blocksize (GST_BASE_SRC (ladspa), -1);
}

static void
gst_ladspa_source_type_dispose (GObject * object)
{
  GstLADSPASource *ladspa = GST_LADSPA_SOURCE (object);

  gst_ladspa_cleanup (&ladspa->ladspa);

  G_OBJECT_CLASS (gst_ladspa_source_type_parent_class)->dispose (object);
}

static void
gst_ladspa_source_type_finalize (GObject * object)
{
  GstLADSPASource *ladspa = GST_LADSPA_SOURCE (object);

  gst_ladspa_finalize (&ladspa->ladspa);

  G_OBJECT_CLASS (gst_ladspa_source_type_parent_class)->finalize (object);
}

/*
 * It is okay for plugins to 'leak' a one-time allocation. This will be freed when
 * the application exits. When the plugins are scanned for the first time, this is
 * done from a separate process to not impose the memory overhead on the calling
 * application (among other reasons). Hence no need for class_finalize.
 */
static void
gst_ladspa_source_type_base_init (GstLADSPASourceClass * ladspa_class)
{
  GstElementClass *elem_class = GST_ELEMENT_CLASS (ladspa_class);
  GstBaseSrcClass *base_class = GST_BASE_SRC_CLASS (ladspa_class);

  gst_ladspa_class_init (&ladspa_class->ladspa,
      G_TYPE_FROM_CLASS (ladspa_class));

  gst_ladspa_element_class_set_metadata (&ladspa_class->ladspa, elem_class,
      GST_LADSPA_SOURCE_CLASS_TAGS);

  gst_ladspa_source_type_class_add_pad_template (&ladspa_class->ladspa,
      base_class);

  gst_type_mark_as_plugin_api (GST_TYPE_LADSPA_SOURCE, 0);
}

static void
gst_ladspa_source_type_base_finalize (GstLADSPASourceClass * ladspa_class)
{
  gst_ladspa_class_finalize (&ladspa_class->ladspa);
}

static void
gst_ladspa_source_type_class_init (GstLADSPASourceClass * ladspa_class,
    LADSPA_Descriptor * desc)
{
  GObjectClass *object_class = (GObjectClass *) ladspa_class;
  GstBaseSrcClass *base_class = (GstBaseSrcClass *) ladspa_class;

  gst_ladspa_source_type_parent_class = g_type_class_peek_parent (ladspa_class);

  object_class->dispose = GST_DEBUG_FUNCPTR (gst_ladspa_source_type_dispose);
  object_class->finalize = GST_DEBUG_FUNCPTR (gst_ladspa_source_type_finalize);
  object_class->set_property =
      GST_DEBUG_FUNCPTR (gst_ladspa_source_type_set_property);
  object_class->get_property =
      GST_DEBUG_FUNCPTR (gst_ladspa_source_type_get_property);

  base_class->set_caps = GST_DEBUG_FUNCPTR (gst_ladspa_source_type_set_caps);
  base_class->fixate = GST_DEBUG_FUNCPTR (gst_ladspa_source_type_fixate);
  base_class->is_seekable =
      GST_DEBUG_FUNCPTR (gst_ladspa_source_type_is_seekable);
  base_class->do_seek = GST_DEBUG_FUNCPTR (gst_ladspa_source_type_do_seek);
  base_class->query = GST_DEBUG_FUNCPTR (gst_ladspa_source_type_query);
  base_class->get_times = GST_DEBUG_FUNCPTR (gst_ladspa_source_type_get_times);
  base_class->start = GST_DEBUG_FUNCPTR (gst_ladspa_source_type_start);
  base_class->stop = GST_DEBUG_FUNCPTR (gst_ladspa_source_type_stop);
  base_class->fill = GST_DEBUG_FUNCPTR (gst_ladspa_source_type_fill);

  g_object_class_install_property (object_class,
      GST_LADSPA_SOURCE_PROP_SAMPLES_PER_BUFFER,
      g_param_spec_int ("samplesperbuffer", "Samples per buffer",
          "Number of samples in each outgoing buffer", 1, G_MAXINT,
          GST_LADSPA_SOURCE_DEFAULT_SAMPLES_PER_BUFFER,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, GST_LADSPA_SOURCE_PROP_IS_LIVE,
      g_param_spec_boolean ("is-live", "Is Live",
          "Whether to act as a live source", GST_LADSPA_SOURCE_DEFAULT_IS_LIVE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
      GST_LADSPA_SOURCE_PROP_TIMESTAMP_OFFSET,
      g_param_spec_int64 ("timestamp-offset", "Timestamp offset",
          "An offset added to timestamps set on buffers (in ns)", G_MININT64,
          G_MAXINT64, GST_LADSPA_SOURCE_DEFAULT_TIMESTAMP_OFFSET,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
      GST_LADSPA_SOURCE_PROP_CAN_ACTIVATE_PUSH,
      g_param_spec_boolean ("can-activate-push", "Can activate push",
          "Can activate in push mode",
          GST_LADSPA_SOURCE_DEFAULT_CAN_ACTIVATE_PUSH,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
      GST_LADSPA_SOURCE_PROP_CAN_ACTIVATE_PULL,
      g_param_spec_boolean ("can-activate-pull", "Can activate pull",
          "Can activate in pull mode",
          GST_LADSPA_SOURCE_DEFAULT_CAN_ACTIVATE_PULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_ladspa_object_class_install_properties (&ladspa_class->ladspa,
      object_class, GST_LADSPA_SOURCE_PROP_LAST);
}

G_DEFINE_ABSTRACT_TYPE (GstLADSPASource, gst_ladspa_source, GST_TYPE_BASE_SRC);

static void
gst_ladspa_source_init (GstLADSPASource * ladspa)
{
}

static void
gst_ladspa_source_class_init (GstLADSPASourceClass * ladspa_class)
{
}

/*
 * Construct the type.
 */
void
ladspa_register_source_element (GstPlugin * plugin, GstStructure * ladspa_meta)
{
  GTypeInfo info = {
    sizeof (GstLADSPASourceClass),
    (GBaseInitFunc) gst_ladspa_source_type_base_init,
    (GBaseFinalizeFunc) gst_ladspa_source_type_base_finalize,
    (GClassInitFunc) gst_ladspa_source_type_class_init,
    NULL,
    NULL,
    sizeof (GstLADSPASource),
    0,
    (GInstanceInitFunc) gst_ladspa_source_type_init,
    NULL
  };
  ladspa_register_element (plugin, GST_TYPE_LADSPA_SOURCE, &info, ladspa_meta);
}
