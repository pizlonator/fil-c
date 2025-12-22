/* GStreamer
 * Copyright (C) 1999 Erik Walthinsen <omega@cse.ogi.edu>
 *               2001 Steve Baker <stevebaker_org@yahoo.co.uk>
 *               2003 Andy Wingo <wingo at pobox.com>
 *               2016 Stefan Sauer <ensonic@users.sf.net>
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
#include "gstlv2.h"
#include "gstlv2utils.h"

#include <string.h>
#include <math.h>
#include <glib.h>

#include <lilv/lilv.h>

#include <gst/audio/audio.h>
#include <gst/audio/audio-channels.h>
#include <gst/base/gstbasesrc.h>

GST_DEBUG_CATEGORY_EXTERN (lv2_debug);
#define GST_CAT_DEFAULT lv2_debug


typedef struct _GstLV2Source GstLV2Source;
typedef struct _GstLV2SourceClass GstLV2SourceClass;

struct _GstLV2Source
{
  GstBaseSrc parent;

  GstLV2 lv2;

  /* audio parameters */
  GstAudioInfo info;
  gint samples_per_buffer;

  /*< private > */
  gboolean tags_pushed;         /* send tags just once ? */
  GstClockTimeDiff timestamp_offset;    /* base offset */
  GstClockTime next_time;       /* next timestamp */
  gint64 next_sample;           /* next sample to send */
  gint64 next_byte;             /* next byte to send */
  gint64 sample_stop;
  gboolean check_seek_stop;
  gboolean eos_reached;
  gint generate_samples_per_buffer;     /* used to generate a partial buffer */
  gboolean can_activate_pull;
  gboolean reverse;             /* play backwards */
};

struct _GstLV2SourceClass
{
  GstBaseSrcClass parent_class;

  GstLV2Class lv2;
};

enum
{
  GST_LV2_SOURCE_PROP_0,
  GST_LV2_SOURCE_PROP_SAMPLES_PER_BUFFER,
  GST_LV2_SOURCE_PROP_IS_LIVE,
  GST_LV2_SOURCE_PROP_TIMESTAMP_OFFSET,
  GST_LV2_SOURCE_PROP_CAN_ACTIVATE_PUSH,
  GST_LV2_SOURCE_PROP_CAN_ACTIVATE_PULL,
  GST_LV2_SOURCE_PROP_LAST
};

static GstBaseSrc *parent_class = NULL;

/* preset interface */

static gchar **
gst_lv2_source_get_preset_names (GstPreset * preset)
{
  GstLV2Source *self = (GstLV2Source *) preset;

  return gst_lv2_get_preset_names (&self->lv2, (GstObject *) self);
}

static gboolean
gst_lv2_source_load_preset (GstPreset * preset, const gchar * name)
{
  GstLV2Source *self = (GstLV2Source *) preset;

  return gst_lv2_load_preset (&self->lv2, (GstObject *) self, name);
}

static gboolean
gst_lv2_source_save_preset (GstPreset * preset, const gchar * name)
{
  GstLV2Source *self = (GstLV2Source *) preset;

  return gst_lv2_save_preset (&self->lv2, (GstObject *) self, name);
}

static gboolean
gst_lv2_source_rename_preset (GstPreset * preset, const gchar * old_name,
    const gchar * new_name)
{
  return FALSE;
}

static gboolean
gst_lv2_source_delete_preset (GstPreset * preset, const gchar * name)
{
  GstLV2Source *self = (GstLV2Source *) preset;

  return gst_lv2_delete_preset (&self->lv2, (GstObject *) self, name);
}

static gboolean
gst_lv2_source_set_meta (GstPreset * preset, const gchar * name,
    const gchar * tag, const gchar * value)
{
  return FALSE;
}

static gboolean
gst_lv2_source_get_meta (GstPreset * preset, const gchar * name,
    const gchar * tag, gchar ** value)
{
  *value = NULL;
  return FALSE;
}

static void
gst_lv2_source_preset_interface_init (gpointer g_iface, gpointer iface_data)
{
  GstPresetInterface *iface = g_iface;

  iface->get_preset_names = gst_lv2_source_get_preset_names;
  iface->load_preset = gst_lv2_source_load_preset;
  iface->save_preset = gst_lv2_source_save_preset;
  iface->rename_preset = gst_lv2_source_rename_preset;
  iface->delete_preset = gst_lv2_source_delete_preset;
  iface->set_meta = gst_lv2_source_set_meta;
  iface->get_meta = gst_lv2_source_get_meta;
}


/* GstBasesrc vmethods implementation */

static gboolean
gst_lv2_source_set_caps (GstBaseSrc * base, GstCaps * caps)
{
  GstLV2Source *lv2 = (GstLV2Source *) base;
  GstAudioInfo info;

  if (!gst_audio_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (base, "received invalid caps");
    return FALSE;
  }

  GST_DEBUG_OBJECT (lv2, "negotiated to caps %" GST_PTR_FORMAT, caps);

  lv2->info = info;

  gst_base_src_set_blocksize (base,
      GST_AUDIO_INFO_BPF (&info) * lv2->samples_per_buffer);

  if (!gst_lv2_setup (&lv2->lv2, GST_AUDIO_INFO_RATE (&info)))
    goto no_instance;

  return TRUE;

no_instance:
  {
    GST_ERROR_OBJECT (lv2, "could not create instance");
    return FALSE;
  }
}

static GstCaps *
gst_lv2_source_fixate (GstBaseSrc * base, GstCaps * caps)
{
  GstLV2Source *lv2 = (GstLV2Source *) base;
  GstStructure *structure;

  caps = gst_caps_make_writable (caps);

  structure = gst_caps_get_structure (caps, 0);

  GST_DEBUG_OBJECT (lv2, "fixating samplerate to %d", GST_AUDIO_DEF_RATE);

  gst_structure_fixate_field_nearest_int (structure, "rate",
      GST_AUDIO_DEF_RATE);

  gst_structure_fixate_field_string (structure, "format", GST_AUDIO_NE (F32));

  gst_structure_fixate_field_nearest_int (structure, "channels",
      lv2->lv2.klass->out_group.ports->len);

  caps = GST_BASE_SRC_CLASS (parent_class)->fixate (base, caps);

  return caps;
}

static void
gst_lv2_source_get_times (GstBaseSrc * base, GstBuffer * buffer,
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
gst_lv2_source_do_seek (GstBaseSrc * base, GstSegment * segment)
{
  GstLV2Source *lv2 = (GstLV2Source *) base;
  GstClockTime time;
  gint samplerate, bpf;
  gint64 next_sample;

  GST_DEBUG_OBJECT (lv2, "seeking %" GST_SEGMENT_FORMAT, segment);

  time = segment->position;
  lv2->reverse = (segment->rate < 0.0);

  samplerate = GST_AUDIO_INFO_RATE (&lv2->info);
  bpf = GST_AUDIO_INFO_BPF (&lv2->info);

  /* now move to the time indicated, don't seek to the sample *after* the time */
  next_sample = gst_util_uint64_scale_int (time, samplerate, GST_SECOND);
  lv2->next_byte = next_sample * bpf;
  if (samplerate == 0)
    lv2->next_time = 0;
  else
    lv2->next_time =
        gst_util_uint64_scale_round (next_sample, GST_SECOND, samplerate);

  GST_DEBUG_OBJECT (lv2, "seeking next_sample=%" G_GINT64_FORMAT
      " next_time=%" GST_TIME_FORMAT, next_sample,
      GST_TIME_ARGS (lv2->next_time));

  g_assert (lv2->next_time <= time);

  lv2->next_sample = next_sample;

  if (!lv2->reverse) {
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
    lv2->sample_stop =
        gst_util_uint64_scale_round (time, samplerate, GST_SECOND);
    lv2->check_seek_stop = TRUE;
  } else {
    lv2->check_seek_stop = FALSE;
  }
  lv2->eos_reached = FALSE;

  return TRUE;
}

static gboolean
gst_lv2_source_is_seekable (GstBaseSrc * base)
{
  /* we're seekable... */
  return TRUE;
}

static gboolean
gst_lv2_source_query (GstBaseSrc * base, GstQuery * query)
{
  GstLV2Source *lv2 = (GstLV2Source *) base;
  gboolean res = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONVERT:
    {
      GstFormat src_fmt, dest_fmt;
      gint64 src_val, dest_val;

      gst_query_parse_convert (query, &src_fmt, &src_val, &dest_fmt, &dest_val);

      if (!gst_audio_info_convert (&lv2->info, src_fmt, src_val, dest_fmt,
              &dest_val)) {
        GST_DEBUG_OBJECT (lv2, "query failed");
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
      if (lv2->can_activate_pull)
        gst_query_add_scheduling_mode (query, GST_PAD_MODE_PULL);

      res = TRUE;
      break;
    }
    default:
      res = GST_BASE_SRC_CLASS (parent_class)->query (base, query);
      break;
  }

  return res;
}

static inline void
gst_lv2_source_interleave_data (guint n_channels, gfloat * outdata,
    guint samples, gfloat * indata)
{
  guint i, j;

  for (i = 0; i < n_channels; i++)
    for (j = 0; j < samples; j++) {
      outdata[j * n_channels + i] = indata[i * samples + j];
    }
}

static GstFlowReturn
gst_lv2_source_fill (GstBaseSrc * base, guint64 offset,
    guint length, GstBuffer * buffer)
{
  GstLV2Source *lv2 = (GstLV2Source *) base;
  GstLV2SourceClass *klass = (GstLV2SourceClass *) GST_BASE_SRC_GET_CLASS (lv2);
  GstLV2Class *lv2_class = &klass->lv2;
  GstLV2Group *lv2_group;
  GstLV2Port *lv2_port;
  GstClockTime next_time;
  gint64 next_sample, next_byte;
  guint bytes, samples;
  GstElementClass *eclass;
  GstMapInfo map;
  gint samplerate, bpf;
  guint j, k, l;
  gfloat *out = NULL, *cv = NULL, *mem;
  gfloat val;

  /* example for tagging generated data */
  if (!lv2->tags_pushed) {
    GstTagList *taglist;

    taglist = gst_tag_list_new (GST_TAG_DESCRIPTION, "lv2 wave", NULL);

    eclass = GST_ELEMENT_CLASS (parent_class);
    if (eclass->send_event)
      eclass->send_event (GST_ELEMENT (base), gst_event_new_tag (taglist));
    else
      gst_tag_list_unref (taglist);
    lv2->tags_pushed = TRUE;
  }

  if (lv2->eos_reached) {
    GST_INFO_OBJECT (lv2, "eos");
    return GST_FLOW_EOS;
  }

  samplerate = GST_AUDIO_INFO_RATE (&lv2->info);
  bpf = GST_AUDIO_INFO_BPF (&lv2->info);

  /* if no length was given, use our default length in samples otherwise convert
   * the length in bytes to samples. */
  if (length == -1)
    samples = lv2->samples_per_buffer;
  else
    samples = length / bpf;

  /* if no offset was given, use our next logical byte */
  if (offset == -1)
    offset = lv2->next_byte;

  /* now see if we are at the byteoffset we think we are */
  if (offset != lv2->next_byte) {
    GST_DEBUG_OBJECT (lv2, "seek to new offset %" G_GUINT64_FORMAT, offset);
    /* we have a discont in the expected sample offset, do a 'seek' */
    lv2->next_sample = offset / bpf;
    lv2->next_time =
        gst_util_uint64_scale_int (lv2->next_sample, GST_SECOND, samplerate);
    lv2->next_byte = offset;
  }

  /* check for eos */
  if (lv2->check_seek_stop &&
      (lv2->sample_stop > lv2->next_sample) &&
      (lv2->sample_stop < lv2->next_sample + samples)
      ) {
    /* calculate only partial buffer */
    lv2->generate_samples_per_buffer = lv2->sample_stop - lv2->next_sample;
    next_sample = lv2->sample_stop;
    lv2->eos_reached = TRUE;

    GST_INFO_OBJECT (lv2, "eos reached");
  } else {
    /* calculate full buffer */
    lv2->generate_samples_per_buffer = samples;
    next_sample = lv2->next_sample + (lv2->reverse ? (-samples) : samples);
  }

  bytes = lv2->generate_samples_per_buffer * bpf;

  next_byte = lv2->next_byte + (lv2->reverse ? (-bytes) : bytes);
  next_time = gst_util_uint64_scale_int (next_sample, GST_SECOND, samplerate);

  GST_LOG_OBJECT (lv2, "samplerate %d", samplerate);
  GST_LOG_OBJECT (lv2,
      "next_sample %" G_GINT64_FORMAT ", ts %" GST_TIME_FORMAT, next_sample,
      GST_TIME_ARGS (next_time));

  gst_buffer_set_size (buffer, bytes);

  GST_BUFFER_OFFSET (buffer) = lv2->next_sample;
  GST_BUFFER_OFFSET_END (buffer) = next_sample;
  if (!lv2->reverse) {
    GST_BUFFER_TIMESTAMP (buffer) = lv2->timestamp_offset + lv2->next_time;
    GST_BUFFER_DURATION (buffer) = next_time - lv2->next_time;
  } else {
    GST_BUFFER_TIMESTAMP (buffer) = lv2->timestamp_offset + next_time;
    GST_BUFFER_DURATION (buffer) = lv2->next_time - next_time;
  }

  gst_object_sync_values (GST_OBJECT (lv2), GST_BUFFER_TIMESTAMP (buffer));

  lv2->next_time = next_time;
  lv2->next_sample = next_sample;
  lv2->next_byte = next_byte;

  GST_LOG_OBJECT (lv2, "generating %u samples at ts %" GST_TIME_FORMAT,
      samples, GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)));

  gst_buffer_map (buffer, &map, GST_MAP_WRITE);

  /* multi channel outputs */
  lv2_group = &lv2_class->out_group;
  if (lv2_group->ports->len > 1) {
    out = g_new0 (gfloat, samples * lv2_group->ports->len);
    for (j = 0; j < lv2_group->ports->len; ++j) {
      lv2_port = &g_array_index (lv2_group->ports, GstLV2Port, j);
      lilv_instance_connect_port (lv2->lv2.instance, lv2_port->index,
          out + (j * samples));
      GST_LOG_OBJECT (lv2, "connected port %d/%d", j, lv2_group->ports->len);
    }
  } else {
    lv2_port = &g_array_index (lv2_group->ports, GstLV2Port, 0);
    lilv_instance_connect_port (lv2->lv2.instance, lv2_port->index,
        (gfloat *) map.data);
    GST_LOG_OBJECT (lv2, "connected port 0");
  }

  /* cv ports */
  cv = g_new (gfloat, samples * lv2_class->num_cv_in);
  for (j = k = 0; j < lv2_class->control_in_ports->len; j++) {
    lv2_port = &g_array_index (lv2_class->control_in_ports, GstLV2Port, j);
    if (lv2_port->type != GST_LV2_PORT_CV)
      continue;

    mem = cv + (k * samples);
    val = lv2->lv2.ports.control.in[j];
    /* FIXME: use gst_control_binding_get_value_array */
    for (l = 0; l < samples; l++)
      mem[l] = val;
    lilv_instance_connect_port (lv2->lv2.instance, lv2_port->index, mem);
    k++;
  }

  lilv_instance_run (lv2->lv2.instance, samples);

  if (lv2_group->ports->len > 1) {
    gst_lv2_source_interleave_data (lv2_group->ports->len,
        (gfloat *) map.data, samples, out);
    g_free (out);
  }

  g_free (cv);

  gst_buffer_unmap (buffer, &map);

  return GST_FLOW_OK;
}

static gboolean
gst_lv2_source_start (GstBaseSrc * base)
{
  GstLV2Source *lv2 = (GstLV2Source *) base;

  lv2->next_sample = 0;
  lv2->next_byte = 0;
  lv2->next_time = 0;
  lv2->check_seek_stop = FALSE;
  lv2->eos_reached = FALSE;
  lv2->tags_pushed = FALSE;

  GST_INFO_OBJECT (base, "starting");

  return TRUE;
}

static gboolean
gst_lv2_source_stop (GstBaseSrc * base)
{
  GstLV2Source *lv2 = (GstLV2Source *) base;

  GST_INFO_OBJECT (base, "stopping");
  return gst_lv2_cleanup (&lv2->lv2, (GstObject *) lv2);
}

/* GObject vmethods implementation */
static void
gst_lv2_source_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstLV2Source *self = (GstLV2Source *) object;

  switch (prop_id) {
    case GST_LV2_SOURCE_PROP_SAMPLES_PER_BUFFER:
      self->samples_per_buffer = g_value_get_int (value);
      gst_base_src_set_blocksize (GST_BASE_SRC (self),
          GST_AUDIO_INFO_BPF (&self->info) * self->samples_per_buffer);
      break;
    case GST_LV2_SOURCE_PROP_IS_LIVE:
      gst_base_src_set_live (GST_BASE_SRC (self), g_value_get_boolean (value));
      break;
    case GST_LV2_SOURCE_PROP_TIMESTAMP_OFFSET:
      self->timestamp_offset = g_value_get_int64 (value);
      break;
    case GST_LV2_SOURCE_PROP_CAN_ACTIVATE_PUSH:
      GST_BASE_SRC (self)->can_activate_push = g_value_get_boolean (value);
      break;
    case GST_LV2_SOURCE_PROP_CAN_ACTIVATE_PULL:
      self->can_activate_pull = g_value_get_boolean (value);
      break;
    default:
      gst_lv2_object_set_property (&self->lv2, object, prop_id, value, pspec);
      break;
  }
}

static void
gst_lv2_source_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstLV2Source *self = (GstLV2Source *) object;

  switch (prop_id) {
    case GST_LV2_SOURCE_PROP_SAMPLES_PER_BUFFER:
      g_value_set_int (value, self->samples_per_buffer);
      break;
    case GST_LV2_SOURCE_PROP_IS_LIVE:
      g_value_set_boolean (value, gst_base_src_is_live (GST_BASE_SRC (self)));
      break;
    case GST_LV2_SOURCE_PROP_TIMESTAMP_OFFSET:
      g_value_set_int64 (value, self->timestamp_offset);
      break;
    case GST_LV2_SOURCE_PROP_CAN_ACTIVATE_PUSH:
      g_value_set_boolean (value, GST_BASE_SRC (self)->can_activate_push);
      break;
    case GST_LV2_SOURCE_PROP_CAN_ACTIVATE_PULL:
      g_value_set_boolean (value, self->can_activate_pull);
      break;
    default:
      gst_lv2_object_get_property (&self->lv2, object, prop_id, value, pspec);
      break;
  }
}

static void
gst_lv2_source_finalize (GObject * object)
{
  GstLV2Source *self = (GstLV2Source *) object;

  gst_lv2_finalize (&self->lv2);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
gst_lv2_source_base_init (gpointer g_class)
{
  GstLV2SourceClass *klass = (GstLV2SourceClass *) g_class;
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  GstPadTemplate *pad_template;
  GstCaps *srccaps;

  gst_lv2_class_init (&klass->lv2, G_TYPE_FROM_CLASS (klass));

  gst_lv2_element_class_set_metadata (&klass->lv2, element_class,
      "Source/Audio/LV2");

  srccaps = gst_caps_new_simple ("audio/x-raw",
      "format", G_TYPE_STRING, GST_AUDIO_NE (F32),
      "channels", G_TYPE_INT, klass->lv2.out_group.ports->len,
      "rate", GST_TYPE_INT_RANGE, 1, G_MAXINT,
      "layout", G_TYPE_STRING, "interleaved", NULL);

  pad_template =
      gst_pad_template_new (GST_BASE_TRANSFORM_SRC_NAME, GST_PAD_SRC,
      GST_PAD_ALWAYS, srccaps);
  gst_element_class_add_pad_template (element_class, pad_template);

  gst_caps_unref (srccaps);
}

static void
gst_lv2_source_base_finalize (GstLV2SourceClass * lv2_class)
{
  gst_lv2_class_finalize (&lv2_class->lv2);
}

static void
gst_lv2_source_class_init (GstLV2SourceClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBaseSrcClass *src_class = (GstBaseSrcClass *) klass;

  GST_DEBUG ("class_init %p", klass);

  gobject_class->set_property = gst_lv2_source_set_property;
  gobject_class->get_property = gst_lv2_source_get_property;
  gobject_class->finalize = gst_lv2_source_finalize;

  src_class->set_caps = gst_lv2_source_set_caps;
  src_class->fixate = gst_lv2_source_fixate;
  src_class->is_seekable = gst_lv2_source_is_seekable;
  src_class->do_seek = gst_lv2_source_do_seek;
  src_class->query = gst_lv2_source_query;
  src_class->get_times = gst_lv2_source_get_times;
  src_class->start = gst_lv2_source_start;
  src_class->stop = gst_lv2_source_stop;
  src_class->fill = gst_lv2_source_fill;

  g_object_class_install_property (gobject_class,
      GST_LV2_SOURCE_PROP_SAMPLES_PER_BUFFER,
      g_param_spec_int ("samplesperbuffer", "Samples per buffer",
          "Number of samples in each outgoing buffer", 1, G_MAXINT, 1024,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, GST_LV2_SOURCE_PROP_IS_LIVE,
      g_param_spec_boolean ("is-live", "Is Live",
          "Whether to act as a live source", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      GST_LV2_SOURCE_PROP_TIMESTAMP_OFFSET,
      g_param_spec_int64 ("timestamp-offset", "Timestamp offset",
          "An offset added to timestamps set on buffers (in ns)", G_MININT64,
          G_MAXINT64, G_GINT64_CONSTANT (0),
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      GST_LV2_SOURCE_PROP_CAN_ACTIVATE_PUSH,
      g_param_spec_boolean ("can-activate-push", "Can activate push",
          "Can activate in push mode", TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      GST_LV2_SOURCE_PROP_CAN_ACTIVATE_PULL,
      g_param_spec_boolean ("can-activate-pull", "Can activate pull",
          "Can activate in pull mode", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_lv2_class_install_properties (&klass->lv2, gobject_class,
      GST_LV2_SOURCE_PROP_LAST);
}

static void
gst_lv2_source_init (GstLV2Source * self, GstLV2SourceClass * klass)
{
  gst_lv2_init (&self->lv2, &klass->lv2);

  gst_base_src_set_format (GST_BASE_SRC (self), GST_FORMAT_TIME);
  gst_base_src_set_blocksize (GST_BASE_SRC (self), -1);

  self->samples_per_buffer = 1024;
  self->generate_samples_per_buffer = self->samples_per_buffer;
}

void
gst_lv2_source_register_element (GstPlugin * plugin, GstStructure * lv2_meta)
{
  GTypeInfo info = {
    sizeof (GstLV2SourceClass),
    (GBaseInitFunc) gst_lv2_source_base_init,
    (GBaseFinalizeFunc) gst_lv2_source_base_finalize,
    (GClassInitFunc) gst_lv2_source_class_init,
    NULL,
    NULL,
    sizeof (GstLV2Source),
    0,
    (GInstanceInitFunc) gst_lv2_source_init,
  };
  const gchar *type_name =
      gst_structure_get_string (lv2_meta, "element-type-name");
  GType element_type =
      g_type_register_static (GST_TYPE_BASE_SRC, type_name, &info, 0);
  gboolean can_do_presets;

  /* register interfaces */
  gst_structure_get_boolean (lv2_meta, "can-do-presets", &can_do_presets);
  if (can_do_presets) {
    const GInterfaceInfo preset_interface_info = {
      (GInterfaceInitFunc) gst_lv2_source_preset_interface_init,
      NULL,
      NULL
    };

    g_type_add_interface_static (element_type, GST_TYPE_PRESET,
        &preset_interface_info);
  }

  gst_element_register (plugin, type_name, GST_RANK_NONE, element_type);

  if (!parent_class)
    parent_class = g_type_class_ref (GST_TYPE_BASE_SRC);
}
