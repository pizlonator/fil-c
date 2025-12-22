/* GStreamer libsndfile plugin
 * Copyright (C) 2013 Stefan Sauer <ensonic@users.sf.net>
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
 * License along with self library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <glib/gi18n-lib.h>
#include <gst/audio/audio.h>

#include "gstsfelements.h"
#include "gstsfdec.h"

#define FORMATS \
    "{ "GST_AUDIO_NE (F32)", "GST_AUDIO_NE (S32)", "GST_AUDIO_NE (S16)" }"

static GstStaticPadTemplate sf_dec_src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " FORMATS ", "
        "layout = (string) interleaved, "
        "rate = (int) [ 1, MAX ], " "channels = (int) [ 1, MAX ]"));

GST_DEBUG_CATEGORY_STATIC (gst_sf_dec_debug);
#define GST_CAT_DEFAULT gst_sf_dec_debug

#define DEFAULT_BUFFER_FRAMES	(256)

static gboolean gst_sf_dec_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_sf_dec_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static GstStateChangeReturn gst_sf_dec_change_state (GstElement * element,
    GstStateChange transition);

static gboolean gst_sf_dec_sink_activate (GstPad * pad, GstObject * parent);
static gboolean gst_sf_dec_sink_activate_mode (GstPad * sinkpad,
    GstObject * parent, GstPadMode mode, gboolean active);
static void gst_sf_dec_loop (GstPad * pad);

static gboolean gst_sf_dec_start (GstSFDec * bsrc);
static gboolean gst_sf_dec_stop (GstSFDec * bsrc);

#define _do_init \
    GST_DEBUG_CATEGORY_INIT (gst_sf_dec_debug, "sfdec", 0, "sfdec element");
#define gst_sf_dec_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstSFDec, gst_sf_dec, GST_TYPE_ELEMENT, _do_init);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (sfdec, "sfdec", GST_RANK_MARGINAL,
    GST_TYPE_SF_DEC, sf_element_init (plugin));
/* sf virtual io */

static sf_count_t
gst_sf_vio_get_filelen (void *user_data)
{
  GstSFDec *self = GST_SF_DEC (user_data);
  gint64 dur;

  if (gst_pad_peer_query_duration (self->sinkpad, GST_FORMAT_BYTES, &dur)) {
    return (sf_count_t) dur;
  }
  GST_WARNING_OBJECT (self, "query_duration failed");
  return -1;
}

static sf_count_t
gst_sf_vio_tell (void *user_data)
{
  GstSFDec *self = GST_SF_DEC (user_data);
  return self->pos;
}

static sf_count_t
gst_sf_vio_seek (sf_count_t offset, int whence, void *user_data)
{
  GstSFDec *self = GST_SF_DEC (user_data);

  switch (whence) {
    case SEEK_CUR:
      self->pos += offset;
      break;
    case SEEK_SET:
      self->pos = offset;
      break;
    case SEEK_END:
      self->pos = gst_sf_vio_get_filelen (user_data) - offset;
      break;
  }
  return (sf_count_t) self->pos;
}

static sf_count_t
gst_sf_vio_read (void *ptr, sf_count_t count, void *user_data)
{
  GstSFDec *self = GST_SF_DEC (user_data);
  GstBuffer *buffer = gst_buffer_new_wrapped_full (0, ptr, count, 0, count,
      ptr, NULL);

  if (gst_pad_pull_range (self->sinkpad, self->pos, count, &buffer) ==
      GST_FLOW_OK) {
    GST_DEBUG_OBJECT (self, "read %d bytes @ pos %" G_GUINT64_FORMAT,
        (gint) count, self->pos);
    self->pos += count;
    return count;
  }
  GST_WARNING_OBJECT (self, "read failed");
  return 0;
}

static sf_count_t
gst_sf_vio_write (const void *ptr, sf_count_t count, void *user_data)
{
  GstSFDec *self = GST_SF_DEC (user_data);
  GstBuffer *buffer = gst_buffer_new_memdup (ptr, count);

  if (gst_pad_push (self->srcpad, buffer) == GST_FLOW_OK) {
    return count;
  }
  GST_WARNING_OBJECT (self, "write failed");
  return 0;
}

SF_VIRTUAL_IO gst_sf_vio = {
  &gst_sf_vio_get_filelen,
  &gst_sf_vio_seek,
  &gst_sf_vio_read,
  &gst_sf_vio_write,
  &gst_sf_vio_tell,
};


static void
gst_sf_dec_class_init (GstSFDecClass * klass)
{
  GstElementClass *gstelement_class;

  gstelement_class = GST_ELEMENT_CLASS (klass);
  gstelement_class->change_state = GST_DEBUG_FUNCPTR (gst_sf_dec_change_state);

  gst_element_class_set_static_metadata (gstelement_class, "Sndfile decoder",
      "Decoder/Audio",
      "Read audio streams using libsndfile",
      "Stefan Sauer <ensonic@user.sf.net>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &sf_dec_src_factory);

  gst_element_class_add_pad_template (gstelement_class,
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          gst_sf_create_audio_template_caps ()));

}

static void
gst_sf_dec_init (GstSFDec * self)
{
  self->sinkpad = gst_pad_new_from_template (gst_element_class_get_pad_template
      (GST_ELEMENT_GET_CLASS (self), "sink"), "sink");
  gst_pad_set_activate_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_sf_dec_sink_activate));
  gst_pad_set_activatemode_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_sf_dec_sink_activate_mode));
  gst_element_add_pad (GST_ELEMENT (self), self->sinkpad);

  self->srcpad = gst_pad_new_from_static_template (&sf_dec_src_factory, "src");
  gst_pad_set_event_function (self->srcpad,
      GST_DEBUG_FUNCPTR (gst_sf_dec_src_event));
  gst_pad_set_query_function (self->srcpad,
      GST_DEBUG_FUNCPTR (gst_sf_dec_src_query));
  gst_element_add_pad (GST_ELEMENT (self), self->srcpad);
}

static gboolean
gst_sf_dec_do_seek (GstSFDec * self, GstEvent * event)
{
  gdouble rate;
  GstFormat format;
  GstSeekFlags flags;
  GstSeekType cur_type, stop_type;
  gboolean flush;
  gint64 cur, stop, pos;
  GstSegment seg;
  guint64 song_length = gst_util_uint64_scale_int (self->duration, GST_SECOND,
      self->rate);

  gst_event_parse_seek (event, &rate, &format, &flags, &cur_type, &cur,
      &stop_type, &stop);

  if (format != GST_FORMAT_TIME)
    goto unsupported_format;

  /* FIXME: we should be using GstSegment for all this */
  if (cur_type != GST_SEEK_TYPE_SET || stop_type != GST_SEEK_TYPE_NONE)
    goto unsupported_type;

  if (stop_type == GST_SEEK_TYPE_NONE)
    stop = GST_CLOCK_TIME_NONE;
  if (!GST_CLOCK_TIME_IS_VALID (stop) && song_length > 0)
    stop = song_length;

  cur = CLAMP (cur, -1, song_length);

  /* cur -> pos */
  pos = gst_util_uint64_scale_int (cur, self->rate, GST_SECOND);
  if ((pos = sf_seek (self->file, pos, SEEK_SET) == -1))
    goto seek_failed;

  /* pos -> cur */
  cur = gst_util_uint64_scale_int (pos, GST_SECOND, self->rate);

  GST_DEBUG_OBJECT (self, "seek to %" GST_TIME_FORMAT,
      GST_TIME_ARGS ((guint64) cur));

  flush = ((flags & GST_SEEK_FLAG_FLUSH) == GST_SEEK_FLAG_FLUSH);

  if (flush) {
    gst_pad_push_event (self->srcpad, gst_event_new_flush_start ());
  } else {
    gst_pad_stop_task (self->sinkpad);
  }

  GST_PAD_STREAM_LOCK (self->sinkpad);

  if (flags & GST_SEEK_FLAG_SEGMENT) {
    gst_element_post_message (GST_ELEMENT (self),
        gst_message_new_segment_start (GST_OBJECT (self), format, cur));
  }

  if (flush) {
    gst_pad_push_event (self->srcpad, gst_event_new_flush_stop (TRUE));
  }

  GST_LOG_OBJECT (self, "sending newsegment from %" GST_TIME_FORMAT "-%"
      GST_TIME_FORMAT ", pos=%" GST_TIME_FORMAT,
      GST_TIME_ARGS ((guint64) cur), GST_TIME_ARGS ((guint64) stop),
      GST_TIME_ARGS ((guint64) cur));

  gst_segment_init (&seg, GST_FORMAT_TIME);
  seg.rate = rate;
  seg.start = cur;
  seg.stop = stop;
  seg.time = cur;
  gst_pad_push_event (self->srcpad, gst_event_new_segment (&seg));

  gst_pad_start_task (self->sinkpad,
      (GstTaskFunction) gst_sf_dec_loop, self, NULL);

  GST_PAD_STREAM_UNLOCK (self->sinkpad);

  return TRUE;

  /* ERROR */
unsupported_format:
  {
    GST_DEBUG_OBJECT (self, "seeking is only supported in TIME format");
    return FALSE;
  }
unsupported_type:
  {
    GST_DEBUG_OBJECT (self, "unsupported seek type");
    return FALSE;
  }
seek_failed:
  {
    GST_DEBUG_OBJECT (self, "seek failed");
    return FALSE;
  }
}

static gboolean
gst_sf_dec_src_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstSFDec *self = GST_SF_DEC (parent);
  gboolean res = FALSE;

  GST_DEBUG_OBJECT (self, "event %s, %" GST_PTR_FORMAT,
      GST_EVENT_TYPE_NAME (event), event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
      if (!self->file || !self->seekable)
        goto done;
      res = gst_sf_dec_do_seek (self, event);
      break;
    default:
      res = gst_pad_event_default (pad, parent, event);
      break;
  }
done:
  GST_DEBUG_OBJECT (self, "event %s: %d", GST_EVENT_TYPE_NAME (event), res);
  return res;
}

static gboolean
gst_sf_dec_src_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstSFDec *self = GST_SF_DEC (parent);
  GstFormat format;
  gboolean res = FALSE;

  GST_DEBUG_OBJECT (self, "query %s, %" GST_PTR_FORMAT,
      GST_QUERY_TYPE_NAME (query), query);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_DURATION:
      if (!self->file)
        goto done;
      gst_query_parse_duration (query, &format, NULL);
      if (format == GST_FORMAT_TIME) {
        gst_query_set_duration (query, format,
            gst_util_uint64_scale_int (self->duration, GST_SECOND, self->rate));
        res = TRUE;
      }
      break;
    case GST_QUERY_POSITION:
      if (!self->file)
        goto done;
      gst_query_parse_position (query, &format, NULL);
      if (format == GST_FORMAT_TIME) {
        gst_query_set_position (query, format,
            gst_util_uint64_scale_int (self->pos, GST_SECOND, self->rate));
        res = TRUE;
      }
      break;
    default:
      res = gst_pad_query_default (pad, parent, query);
      break;
  }

done:
  GST_DEBUG_OBJECT (self, "query %s: %d", GST_QUERY_TYPE_NAME (query), res);
  return res;
}

static GstStateChangeReturn
gst_sf_dec_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret;
  GstSFDec *self = GST_SF_DEC (element);

  GST_INFO_OBJECT (self, "transition: %s -> %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_sf_dec_start (self);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_sf_dec_stop (self);
      break;
    default:
      break;
  }
  return ret;
}

static gboolean
gst_sf_dec_start (GstSFDec * self)
{
  return TRUE;
}

static gboolean
gst_sf_dec_stop (GstSFDec * self)
{
  int err = 0;

  GST_INFO_OBJECT (self, "Closing sndfile stream");

  if (self->file && (err = sf_close (self->file)))
    goto close_failed;

  self->file = NULL;
  self->offset = 0;
  self->channels = 0;
  self->rate = 0;

  self->pos = 0;
  self->duration = 0;

  return TRUE;

close_failed:
  {
    GST_ELEMENT_ERROR (self, RESOURCE, CLOSE,
        ("Could not close sndfile stream."),
        ("soundfile error: %s", sf_error_number (err)));
    return FALSE;
  }
}

static gboolean
gst_sf_dec_sink_activate (GstPad * sinkpad, GstObject * parent)
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
gst_sf_dec_sink_activate_mode (GstPad * sinkpad, GstObject * parent,
    GstPadMode mode, gboolean active)
{
  gboolean res;

  switch (mode) {
    case GST_PAD_MODE_PUSH:
      res = FALSE;              /* no push support */
      break;
    case GST_PAD_MODE_PULL:
      if (active) {
        /* if we have a scheduler we can start the task */
        GST_DEBUG_OBJECT (sinkpad, "start task");
        res = gst_pad_start_task (sinkpad, (GstTaskFunction) gst_sf_dec_loop,
            sinkpad, NULL);
      } else {
        res = gst_pad_stop_task (sinkpad);
      }
      break;
    default:
      res = FALSE;
      break;
  }
  return res;
}

static void
create_and_send_tags (GstSFDec * self, SF_INFO * info, SF_LOOP_INFO * loop_info,
    SF_INSTRUMENT * instrument)
{
  GstTagList *tags;
  const gchar *tag;
  const gchar *codec_name;

  /* send tags */
  tags = gst_tag_list_new_empty ();
  if ((tag = sf_get_string (self->file, SF_STR_TITLE)) && *tag) {
    gst_tag_list_add (tags, GST_TAG_MERGE_APPEND, GST_TAG_TITLE, tag, NULL);
  }
  if ((tag = sf_get_string (self->file, SF_STR_COMMENT)) && *tag) {
    gst_tag_list_add (tags, GST_TAG_MERGE_APPEND, GST_TAG_COMMENT, tag, NULL);
  }
  if ((tag = sf_get_string (self->file, SF_STR_ARTIST)) && *tag) {
    gst_tag_list_add (tags, GST_TAG_MERGE_APPEND, GST_TAG_ARTIST, tag, NULL);
  }
  if ((tag = sf_get_string (self->file, SF_STR_ALBUM)) && *tag) {
    gst_tag_list_add (tags, GST_TAG_MERGE_APPEND, GST_TAG_ALBUM, tag, NULL);
  }
  if ((tag = sf_get_string (self->file, SF_STR_GENRE)) && *tag) {
    gst_tag_list_add (tags, GST_TAG_MERGE_APPEND, GST_TAG_GENRE, tag, NULL);
  }
  if ((tag = sf_get_string (self->file, SF_STR_COPYRIGHT)) && *tag) {
    gst_tag_list_add (tags, GST_TAG_MERGE_APPEND, GST_TAG_COPYRIGHT, tag, NULL);
  }
  if ((tag = sf_get_string (self->file, SF_STR_LICENSE)) && *tag) {
    gst_tag_list_add (tags, GST_TAG_MERGE_APPEND, GST_TAG_LICENSE, tag, NULL);
  }
  if ((tag = sf_get_string (self->file, SF_STR_SOFTWARE)) && *tag) {
    gst_tag_list_add (tags, GST_TAG_MERGE_APPEND, GST_TAG_APPLICATION_NAME, tag,
        NULL);
  }
  if ((tag = sf_get_string (self->file, SF_STR_TRACKNUMBER)) && *tag) {
    guint track = atoi (tag);
    gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE, GST_TAG_TRACK_NUMBER, track,
        NULL);
  }
  if ((tag = sf_get_string (self->file, SF_STR_DATE)) && *tag) {
    GValue tag_val = { 0, };
    GType tag_type = gst_tag_get_type (GST_TAG_DATE_TIME);

    g_value_init (&tag_val, tag_type);
    if (gst_value_deserialize (&tag_val, tag)) {
      gst_tag_list_add_value (tags, GST_TAG_MERGE_APPEND, GST_TAG_DATE_TIME,
          &tag_val);
    } else {
      GST_WARNING_OBJECT (self, "could not deserialize '%s' into a "
          "tag %s of type %s", tag, GST_TAG_DATE_TIME, g_type_name (tag_type));
    }
    g_value_unset (&tag_val);
  }
  if (loop_info) {
    if (loop_info->bpm != 0.0) {
      gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE, GST_TAG_BEATS_PER_MINUTE,
          (gdouble) loop_info->bpm, NULL);
    }
    if (loop_info->root_key != -1) {
      gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE, GST_TAG_MIDI_BASE_NOTE,
          (guint) loop_info->root_key, NULL);
    }
  }
  if (instrument) {
    gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE, GST_TAG_MIDI_BASE_NOTE,
        (guint) instrument->basenote, NULL);
  }
  /* TODO: calculate bitrate: GST_TAG_BITRATE */
  switch (info->format & SF_FORMAT_SUBMASK) {
    case SF_FORMAT_PCM_S8:
    case SF_FORMAT_PCM_16:
    case SF_FORMAT_PCM_24:
    case SF_FORMAT_PCM_32:
    case SF_FORMAT_PCM_U8:
      codec_name = "Uncompressed PCM audio";
      break;
    case SF_FORMAT_FLOAT:
    case SF_FORMAT_DOUBLE:
      codec_name = "Uncompressed IEEE float audio";
      break;
    case SF_FORMAT_ULAW:
      codec_name = "µ-law audio";
      break;
    case SF_FORMAT_ALAW:
      codec_name = "A-law audio";
      break;
    case SF_FORMAT_IMA_ADPCM:
    case SF_FORMAT_MS_ADPCM:
    case SF_FORMAT_VOX_ADPCM:
    case SF_FORMAT_G721_32:
    case SF_FORMAT_G723_24:
    case SF_FORMAT_G723_40:
      codec_name = "ADPCM audio";
      break;
    case SF_FORMAT_GSM610:
      codec_name = "MS GSM audio";
      break;
    case SF_FORMAT_DWVW_12:
    case SF_FORMAT_DWVW_16:
    case SF_FORMAT_DWVW_24:
    case SF_FORMAT_DWVW_N:
      codec_name = "Delta Width Variable Word encoded audio";
      break;
    case SF_FORMAT_DPCM_8:
    case SF_FORMAT_DPCM_16:
      codec_name = "differential PCM audio";
      break;
    case SF_FORMAT_VORBIS:
      codec_name = "Vorbis";
      break;
    default:
      codec_name = NULL;
      GST_WARNING_OBJECT (self, "unmapped codec_type: %d",
          info->format & SF_FORMAT_SUBMASK);
      break;
  }
  if (codec_name) {
    gst_tag_list_add (tags, GST_TAG_MERGE_APPEND, GST_TAG_AUDIO_CODEC,
        codec_name, NULL);
  }

  if (!gst_tag_list_is_empty (tags)) {
    GST_DEBUG_OBJECT (self, "have tags");
    gst_pad_push_event (self->srcpad, gst_event_new_tag (tags));
  } else {
    gst_tag_list_unref (tags);
  }
}

static gboolean
is_valid_loop (gint mode, guint start, guint end)
{
  if (!end)
    return FALSE;
  if (start >= end)
    return FALSE;
  if (!mode)
    return FALSE;

  return TRUE;
}

static void
create_and_send_toc (GstSFDec * self, SF_INFO * info, SF_LOOP_INFO * loop_info,
    SF_INSTRUMENT * instrument)
{
  GstToc *toc;
  GstTocEntry *entry = NULL, *subentry = NULL;
  gint64 start, stop;
  gchar *id;
  gint i;
  gboolean have_loops = FALSE;

  if (!instrument)
    return;

  for (i = 0; i < 16; i++) {
    if (is_valid_loop (instrument->loops[i].mode, instrument->loops[i].start,
            instrument->loops[i].end)) {
      have_loops = TRUE;
      break;
    }
  }
  if (!have_loops) {
    GST_INFO_OBJECT (self, "Have no loops");
    return;
  }


  toc = gst_toc_new (GST_TOC_SCOPE_GLOBAL);
  GST_DEBUG_OBJECT (self, "have toc");

  /* add cue edition */
  entry = gst_toc_entry_new (GST_TOC_ENTRY_TYPE_EDITION, "loops");
  stop = gst_util_uint64_scale_int (self->duration, GST_SECOND, self->rate);
  gst_toc_entry_set_start_stop_times (entry, 0, stop);
  gst_toc_append_entry (toc, entry);

  for (i = 0; i < 16; i++) {
    GST_DEBUG_OBJECT (self,
        "loop[%2d]: mode=%d, start=%u, end=%u, count=%u", i,
        instrument->loops[i].mode, instrument->loops[i].start,
        instrument->loops[i].end, instrument->loops[i].count);
    if (is_valid_loop (instrument->loops[i].mode, instrument->loops[i].start,
            instrument->loops[i].end)) {
      id = g_strdup_printf ("%08x", i);
      subentry = gst_toc_entry_new (GST_TOC_ENTRY_TYPE_CHAPTER, id);
      g_free (id);
      start = gst_util_uint64_scale_int (instrument->loops[i].start,
          GST_SECOND, self->rate);
      stop = gst_util_uint64_scale_int (instrument->loops[i].end,
          GST_SECOND, self->rate);
      gst_toc_entry_set_start_stop_times (subentry, start, stop);
      gst_toc_entry_append_sub_entry (entry, subentry);
    }
  }

  gst_pad_push_event (self->srcpad, gst_event_new_toc (toc, FALSE));
}

static gboolean
gst_sf_dec_open_file (GstSFDec * self)
{
  SF_INFO info = { 0, };
  SF_LOOP_INFO loop_info = { 0, };
  SF_INSTRUMENT instrument = { 0, };
  GstCaps *caps;
  GstStructure *s;
  GstSegment seg;
  gint width;
  const gchar *format;
  gchar *stream_id;
  gboolean have_loop_info = FALSE;
  gboolean have_instrument = FALSE;

  GST_DEBUG_OBJECT (self, "opening the stream");
  if (!(self->file = sf_open_virtual (&gst_sf_vio, SFM_READ, &info, self)))
    goto open_failed;

  stream_id =
      gst_pad_create_stream_id (self->srcpad, GST_ELEMENT_CAST (self), NULL);
  gst_pad_push_event (self->srcpad, gst_event_new_stream_start (stream_id));
  g_free (stream_id);

  self->channels = info.channels;
  self->rate = info.samplerate;
  self->duration = info.frames;
  self->seekable = info.seekable;
  GST_DEBUG_OBJECT (self, "stream openend: channels=%d, rate=%d, seekable=%d",
      info.channels, info.samplerate, info.seekable);

  /* negotiate srcpad caps */
  if ((caps = gst_pad_get_allowed_caps (self->srcpad)) == NULL) {
    caps = gst_pad_get_pad_template_caps (self->srcpad);
  }
  caps = gst_caps_make_writable (caps);
  GST_DEBUG_OBJECT (self, "allowed caps %" GST_PTR_FORMAT, caps);

  s = gst_caps_get_structure (caps, 0);
  gst_structure_set (s,
      "channels", G_TYPE_INT, self->channels,
      "rate", G_TYPE_INT, self->rate, NULL);

  if (!gst_structure_fixate_field_string (s, "format", GST_AUDIO_NE (S16)))
    GST_WARNING_OBJECT (self, "Failed to fixate format to S16NE");

  caps = gst_caps_fixate (caps);

  GST_DEBUG_OBJECT (self, "fixated caps %" GST_PTR_FORMAT, caps);

  /* configure to output the negotiated format */
  s = gst_caps_get_structure (caps, 0);
  format = gst_structure_get_string (s, "format");
  if (g_str_equal (format, GST_AUDIO_NE (S32))) {
    self->reader = (GstSFReader) sf_readf_int;
    width = 32;
  } else if (g_str_equal (format, GST_AUDIO_NE (S16))) {
    self->reader = (GstSFReader) sf_readf_short;
    width = 16;
  } else {
    self->reader = (GstSFReader) sf_readf_float;
    width = 32;
  }
  self->bytes_per_frame = width * self->channels / 8;

  gst_pad_set_caps (self->srcpad, caps);
  gst_caps_unref (caps);

  /* push initial segment */
  gst_segment_init (&seg, GST_FORMAT_TIME);
  seg.stop = gst_util_uint64_scale_int (self->duration, GST_SECOND, self->rate);
  gst_pad_push_event (self->srcpad, gst_event_new_segment (&seg));

  /* get extra details */
  if (sf_command (self->file, SFC_GET_LOOP_INFO, &loop_info,
          sizeof (loop_info))) {
    GST_DEBUG_OBJECT (self, "have loop info");
    have_loop_info = TRUE;
  }
  if (sf_command (self->file, SFC_GET_INSTRUMENT, &instrument,
          sizeof (instrument))) {
    GST_DEBUG_OBJECT (self, "have instrument");
    have_instrument = TRUE;
  }

  create_and_send_tags (self, &info, (have_loop_info ? &loop_info : NULL),
      (have_instrument ? &instrument : NULL));

  create_and_send_toc (self, &info, (have_loop_info ? &loop_info : NULL),
      (have_instrument ? &instrument : NULL));

  return TRUE;

open_failed:
  {
    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ,
        (_("Could not open sndfile stream for reading.")),
        ("soundfile error: %s", sf_strerror (NULL)));
    return FALSE;
  }
}

static void
gst_sf_dec_loop (GstPad * pad)
{
  GstSFDec *self = GST_SF_DEC (GST_PAD_PARENT (pad));
  GstBuffer *buf;
  GstMapInfo map;
  GstFlowReturn flow;
  sf_count_t frames_read;
  guint num_frames = 1024;      /* arbitrary */

  if (G_UNLIKELY (!self->file)) {
    /* not started yet */
    if (!gst_sf_dec_open_file (self))
      goto pause;
  }

  buf = gst_buffer_new_and_alloc (self->bytes_per_frame * num_frames);
  gst_buffer_map (buf, &map, GST_MAP_WRITE);
  frames_read = self->reader (self->file, map.data, num_frames);
  GST_LOG_OBJECT (self, "read %d / %d bytes = %d frames of audio",
      (gint) frames_read, (gint) map.size, num_frames);
  gst_buffer_unmap (buf, &map);

  if (G_UNLIKELY (frames_read < 0))
    goto could_not_read;

  if (G_UNLIKELY (frames_read == 0))
    goto eos;

  GST_BUFFER_OFFSET (buf) = self->offset;
  GST_BUFFER_TIMESTAMP (buf) = gst_util_uint64_scale_int (self->offset,
      GST_SECOND, self->rate);
  self->offset += frames_read;
  GST_BUFFER_DURATION (buf) = gst_util_uint64_scale_int (self->offset,
      GST_SECOND, self->rate) - GST_BUFFER_TIMESTAMP (buf);

  flow = gst_pad_push (self->srcpad, buf);
  if (flow != GST_FLOW_OK) {
    GST_LOG_OBJECT (self, "pad push flow: %s", gst_flow_get_name (flow));
    goto pause;
  }

  return;

  /* ERROR */
could_not_read:
  {
    GST_ELEMENT_ERROR (self, RESOURCE, READ, (NULL), GST_ERROR_SYSTEM);
    gst_buffer_unref (buf);
    goto pause;
  }
eos:
  {
    GST_DEBUG_OBJECT (self, "EOS");
    gst_buffer_unref (buf);
    gst_pad_push_event (self->srcpad, gst_event_new_eos ());
    goto pause;
  }
pause:
  {
    GST_INFO_OBJECT (self, "Pausing");
    gst_pad_pause_task (self->sinkpad);
  }
}
