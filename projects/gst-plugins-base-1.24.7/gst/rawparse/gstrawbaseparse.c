/* GStreamer
 * Copyright (C) <2016> Carlos Rafael Giani <dv at pseudoterminal dot org>
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
 * SECTION:gstrawbaseparse
 * @short_description: Base class for raw media data parsers
 *
 * This base class is for parsers which read raw media data and output
 * timestamped buffers with an integer number of frames inside.
 *
 * The format of the raw media data is specified in one of two ways: either,
 * the information from the sink pad's caps is taken, or the information from
 * the properties is used (this is chosen by the use-sink-caps property).
 * These two ways are internally referred to as "configurations". The configuration
 * that receives its information from the sink pad's caps is called the
 * "sink caps configuration", while the one that depends on the information from
 * the properties is the "properties configuration". Configurations have a
 * "readiness". A configuration is "ready" when it contains valid information.
 * For example, with an audio parser, a configuration is not ready unless it
 * contains a valid sample rate, sample format, and channel count.
 *
 * The properties configuration must always be ready, even right from the start.
 * Subclasses must ensure this. The underlying reason is that properties have valid
 * values right from the start, and with the properties configuration, there is
 * nothing that readies it before actual data is sent (unlike with the sink caps
 * configuration, where a sink caps event will ready it before data is pushed
 * downstream).
 *
 * It is possible to switch between the configurations during a stream by
 * setting the use-sink-caps property. Subclasses typically allow for updating the
 * properties configuration during a stream by setting the various properties
 * (like sample-rate for a raw audio parser).
 * In these cases, the parser will produce a new CAPS event and push it downstream
 * to announce the caps for the new configuration. This also happens if the sink
 * caps change.
 *
 * A common mistake when trying to parse raw data with no input caps (for example,
 * a file with raw PCM samples when using rawaudioparse) is to forget to set the
 * use-sink-caps property to FALSE. In this case, the parser will report an error
 * when it tries to access the current configuration (because then the sink caps
 * configuration will be the current one and it will not contain valid values
 * since no sink caps were seen at this point).
 *
 * Subclasses must ensure that the properties configuration is the default one.
 *
 * The sink caps configuration is mostly useful with push-based sources, because these
 * will produce caps events and send them downstream. With pull-based sources, it is
 * possible that this doesn't happen. Since the sink caps configuration requires a caps
 * event to arrive at the sinkpad, this will cause the parser to fail then.
 *
 * The base class identifies the configurations by means of the GstRawAudioParseConfig
 * enum. It instructs the subclass to switch between configurations this way, and
 * also requests information about the current configuration, a configuration's
 * frame size, its readiness, etc. Subclasses are not required to use any particular
 * structure for the configuration implementations.
 *
 * Use the GST_RAW_BASE_PARSE_CONFIG_MUTEX_LOCK and GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK
 * macros to protect configuration modifications.
 *
 * ## Summary of the subclass requirements
 *
 * * Sink caps and properties configurations must both be
 *   implemented and supported. It must also be ensured that there is a
 *   "current" configuration.
 *
 * * Modifications to the configurations must be protected with the
 *   GstRawBaseParse lock. This is typically necessary when the
 *   properties configuration is modified by setting new property values.
 *   (Note that the lock is held during *all* vfunc calls.)
 *
 * * If the properties configuration is updated (typically by
 *   setting new property values), gst_raw_base_parse_invalidate_src_caps()
 *   must be called if the properties config is the current one. This is
 *   necessary to ensure that GstBaseParse pushes a new caps event downstream
 *   which contains caps from the updated configuration.
 *
 * * In case there are bytes in each frame that aren't part of the actual
 *   payload, the `get_overhead_size()` vfunc must be defined, and the
 *   `get_config_frame_size()` vfunc must return a frame size that includes
 *   the number of non-payload bytes (= the overhead). Otherwise, the
 *   timestamps will incorrectly include the overhead bytes.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>
#include "gstrawbaseparse.h"

GST_DEBUG_CATEGORY_STATIC (raw_base_parse_debug);
#define GST_CAT_DEFAULT raw_base_parse_debug

enum
{
  PROP_0,
  PROP_USE_SINK_CAPS
};

#define DEFAULT_USE_SINK_CAPS  FALSE
#define INITIAL_PARSER_CONFIG \
  ((DEFAULT_USE_SINK_CAPS) ? GST_RAW_BASE_PARSE_CONFIG_SINKCAPS : \
   GST_RAW_BASE_PARSE_CONFIG_PROPERTIES)

#define gst_raw_base_parse_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE (GstRawBaseParse, gst_raw_base_parse,
    GST_TYPE_BASE_PARSE);

static void gst_raw_base_parse_finalize (GObject * object);
static void gst_raw_base_parse_set_property (GObject * object, guint prop_id,
    GValue const *value, GParamSpec * pspec);
static void gst_raw_base_parse_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static gboolean gst_raw_base_parse_start (GstBaseParse * parse);
static gboolean gst_raw_base_parse_stop (GstBaseParse * parse);
static gboolean gst_raw_base_parse_set_sink_caps (GstBaseParse * parse,
    GstCaps * caps);
static GstFlowReturn gst_raw_base_parse_handle_frame (GstBaseParse * parse,
    GstBaseParseFrame * frame, gint * skipsize);
static gboolean gst_raw_base_parse_convert (GstBaseParse * parse,
    GstFormat src_format, gint64 src_value, GstFormat dest_format,
    gint64 * dest_value);

static gboolean gst_raw_base_parse_is_using_sink_caps (GstRawBaseParse *
    raw_base_parse);
static gboolean gst_raw_base_parse_is_gstformat_supported (GstRawBaseParse *
    raw_base_parse, GstFormat format);

static void
gst_raw_base_parse_class_init (GstRawBaseParseClass * klass)
{
  GObjectClass *object_class;
  GstBaseParseClass *baseparse_class;

  GST_DEBUG_CATEGORY_INIT (raw_base_parse_debug, "rawbaseparse", 0,
      "raw base parse class");

  object_class = G_OBJECT_CLASS (klass);
  baseparse_class = GST_BASE_PARSE_CLASS (klass);

  object_class->finalize = GST_DEBUG_FUNCPTR (gst_raw_base_parse_finalize);
  object_class->set_property =
      GST_DEBUG_FUNCPTR (gst_raw_base_parse_set_property);
  object_class->get_property =
      GST_DEBUG_FUNCPTR (gst_raw_base_parse_get_property);

  baseparse_class->start = GST_DEBUG_FUNCPTR (gst_raw_base_parse_start);
  baseparse_class->stop = GST_DEBUG_FUNCPTR (gst_raw_base_parse_stop);
  baseparse_class->set_sink_caps =
      GST_DEBUG_FUNCPTR (gst_raw_base_parse_set_sink_caps);
  baseparse_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_raw_base_parse_handle_frame);
  baseparse_class->convert = GST_DEBUG_FUNCPTR (gst_raw_base_parse_convert);

  /**
   * GstRawBaseParse::use-sink-caps:
   *
   * Use sink caps configuration. If set to false, the parser
   * will use the properties configuration instead. It is possible
   * to switch between these during playback.
   */
  g_object_class_install_property (object_class,
      PROP_USE_SINK_CAPS,
      g_param_spec_boolean ("use-sink-caps",
          "Use sink caps",
          "Use the sink caps for defining the output format",
          DEFAULT_USE_SINK_CAPS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );

  gst_type_mark_as_plugin_api (GST_TYPE_RAW_BASE_PARSE, 0);
}

static void
gst_raw_base_parse_init (GstRawBaseParse * raw_base_parse)
{
  raw_base_parse->src_caps_set = FALSE;
  g_mutex_init (&(raw_base_parse->config_mutex));
}

static void
gst_raw_base_parse_finalize (GObject * object)
{
  GstRawBaseParse *raw_base_parse = GST_RAW_BASE_PARSE (object);

  g_mutex_clear (&(raw_base_parse->config_mutex));

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_raw_base_parse_set_property (GObject * object, guint prop_id,
    GValue const *value, GParamSpec * pspec)
{
  GstBaseParse *base_parse = GST_BASE_PARSE (object);
  GstRawBaseParse *raw_base_parse = GST_RAW_BASE_PARSE (object);
  GstRawBaseParseClass *klass = GST_RAW_BASE_PARSE_GET_CLASS (object);

  g_assert (klass->is_config_ready);
  g_assert (klass->set_current_config);

  switch (prop_id) {
    case PROP_USE_SINK_CAPS:
    {
      gboolean new_state, cur_state;
      GstRawBaseParseConfig new_config;

      GST_RAW_BASE_PARSE_CONFIG_MUTEX_LOCK (object);

      /* Check to ensure nothing is done if the value stays the same */
      new_state = g_value_get_boolean (value);
      cur_state = gst_raw_base_parse_is_using_sink_caps (raw_base_parse);
      if (new_state == cur_state) {
        GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (object);
        break;
      }

      GST_DEBUG_OBJECT (raw_base_parse, "switching to %s config",
          new_state ? "sink caps" : "properties");
      new_config =
          new_state ? GST_RAW_BASE_PARSE_CONFIG_SINKCAPS :
          GST_RAW_BASE_PARSE_CONFIG_PROPERTIES;

      if (!klass->set_current_config (raw_base_parse, new_config)) {
        GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (object);
        GST_ELEMENT_ERROR (raw_base_parse, STREAM, FAILED,
            ("could not set new current config"), ("use-sink-caps property: %d",
                new_state));
        break;
      }

      /* Update the minimum frame size if the config is ready. This ensures that
       * the next buffer that is passed to handle_frame contains complete frames.
       * If the current config is the properties config, then it will always be
       * ready, and its frame size will be valid. Ensure that the baseparse minimum
       * frame size is set properly then.
       * If the current config is the sink caps config, then it will initially not
       * be ready until the sink caps are set, so the minimum frame size cannot be
       * set right here. However, since the caps always come in *before* the actual
       * data, the config will be readied in the set_sink_caps function, and be ready
       * by the time handle_frame is called. There, the minimum frame size is set as
       * well. */
      if (klass->is_config_ready (raw_base_parse,
              GST_RAW_BASE_PARSE_CONFIG_CURRENT)) {
        gsize frame_size = klass->get_config_frame_size (raw_base_parse,
            GST_RAW_BASE_PARSE_CONFIG_CURRENT);
        gst_base_parse_set_min_frame_size (base_parse, frame_size);
      }

      /* Since the current config was switched, the source caps change. Ensure the
       * new caps are pushed downstream by setting src_caps_set to FALSE: This way,
       * the next handle_frame call will take care of that. */
      raw_base_parse->src_caps_set = FALSE;

      GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (object);

      break;
    }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_raw_base_parse_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstRawBaseParse *raw_base_parse = GST_RAW_BASE_PARSE (object);

  switch (prop_id) {
    case PROP_USE_SINK_CAPS:
      GST_RAW_BASE_PARSE_CONFIG_MUTEX_LOCK (object);
      g_value_set_boolean (value,
          gst_raw_base_parse_is_using_sink_caps (raw_base_parse));
      GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (object);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_raw_base_parse_start (GstBaseParse * parse)
{
  GstBaseParse *base_parse = GST_BASE_PARSE (parse);
  GstRawBaseParse *raw_base_parse = GST_RAW_BASE_PARSE (parse);
  GstRawBaseParseClass *klass = GST_RAW_BASE_PARSE_GET_CLASS (parse);

  g_assert (klass->set_current_config);

  GST_RAW_BASE_PARSE_CONFIG_MUTEX_LOCK (raw_base_parse);

  /* If the config is ready from the start, set the min frame size
   * (this will happen with the properties config) */
  if (klass->is_config_ready (raw_base_parse,
          GST_RAW_BASE_PARSE_CONFIG_CURRENT)) {
    gsize frame_size = klass->get_config_frame_size (raw_base_parse,
        GST_RAW_BASE_PARSE_CONFIG_CURRENT);
    gst_base_parse_set_min_frame_size (base_parse, frame_size);
  }

  GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (raw_base_parse);

  return TRUE;
}

static gboolean
gst_raw_base_parse_stop (GstBaseParse * parse)
{
  GstRawBaseParse *raw_base_parse = GST_RAW_BASE_PARSE (parse);

  GST_RAW_BASE_PARSE_CONFIG_MUTEX_LOCK (raw_base_parse);
  raw_base_parse->src_caps_set = FALSE;
  GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (raw_base_parse);

  return TRUE;
}

static gboolean
gst_raw_base_parse_set_sink_caps (GstBaseParse * parse, GstCaps * caps)
{
  gboolean ret = FALSE;
  GstRawBaseParse *raw_base_parse = GST_RAW_BASE_PARSE (parse);
  GstRawBaseParseClass *klass = GST_RAW_BASE_PARSE_GET_CLASS (parse);

  g_assert (klass->set_config_from_caps);
  g_assert (klass->get_caps_from_config);
  g_assert (klass->get_config_frame_size);

  GST_RAW_BASE_PARSE_CONFIG_MUTEX_LOCK (raw_base_parse);

  GST_DEBUG_OBJECT (parse, "getting config from new sink caps");

  /* Convert the new sink caps to sink caps config. This also
   * readies the config. */
  ret =
      klass->set_config_from_caps (raw_base_parse,
      GST_RAW_BASE_PARSE_CONFIG_SINKCAPS, caps);
  if (!ret) {
    GST_ERROR_OBJECT (raw_base_parse, "could not get config from sink caps");
    goto done;
  }

  /* If the sink caps config is currently active, push caps downstream,
   * set the minimum frame size (to guarantee that input buffers hold
   * complete frames), and update the src_caps_set flag. If the sink
   * caps config isn't the currently active config, just exit, since in
   * that case, the caps will always be pushed downstream in handle_frame. */
  if (gst_raw_base_parse_is_using_sink_caps (raw_base_parse)) {
    GstCaps *new_src_caps;
    gsize frame_size;

    GST_DEBUG_OBJECT (parse,
        "sink caps config is the current one; trying to push new caps downstream");

    /* Convert back to caps. The caps may have changed, for example
     * audio/x-unaligned-raw may have been replaced with audio/x-raw.
     * (Also, this keeps the behavior in sync with that of the block
     * in handle_frame that pushes caps downstream if not done already.) */
    if (!klass->get_caps_from_config (raw_base_parse,
            GST_RAW_BASE_PARSE_CONFIG_CURRENT, &new_src_caps)) {
      GST_ERROR_OBJECT (raw_base_parse,
          "could not get src caps from current config");
      goto done;
    }

    GST_DEBUG_OBJECT (raw_base_parse,
        "got new sink caps; updating src caps to %" GST_PTR_FORMAT,
        (gpointer) new_src_caps);

    frame_size =
        klass->get_config_frame_size (raw_base_parse,
        GST_RAW_BASE_PARSE_CONFIG_CURRENT);
    gst_base_parse_set_min_frame_size (parse, frame_size);

    raw_base_parse->src_caps_set = TRUE;

    GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (raw_base_parse);

    /* Push caps outside of the lock */
    gst_pad_push_event (GST_BASE_PARSE_SRC_PAD (raw_base_parse),
        gst_event_new_caps (new_src_caps)
        );

    gst_caps_unref (new_src_caps);
  } else {
    GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (raw_base_parse);
  }

  ret = TRUE;

done:
  return ret;
}

static GstBuffer *
gst_raw_base_parse_align_buffer (GstRawBaseParse * raw_base_parse,
    gsize alignment, GstBuffer * buffer, gsize out_size)
{
  GstMapInfo map;

  gst_buffer_map (buffer, &map, GST_MAP_READ);

  if (map.size < sizeof (guintptr)) {
    gst_buffer_unmap (buffer, &map);
    return NULL;
  }

  if (((guintptr) map.data) & (alignment - 1)) {
    GstBuffer *new_buffer;
    GstAllocationParams params = { 0, alignment - 1, 0, 0, };

    new_buffer = gst_buffer_new_allocate (NULL, out_size, &params);

    /* Copy data "by hand", so ensure alignment is kept: */
    gst_buffer_fill (new_buffer, 0, map.data, out_size);

    gst_buffer_copy_into (new_buffer, buffer, GST_BUFFER_COPY_METADATA, 0,
        out_size);
    GST_DEBUG_OBJECT (raw_base_parse,
        "We want output aligned on %" G_GSIZE_FORMAT ", reallocated",
        alignment);

    gst_buffer_unmap (buffer, &map);

    return new_buffer;
  }

  gst_buffer_unmap (buffer, &map);

  return NULL;
}

static GstFlowReturn
gst_raw_base_parse_handle_frame (GstBaseParse * parse,
    GstBaseParseFrame * frame, gint * skipsize)
{
  gsize in_size, out_size;
  guint frame_size;
  guint num_out_frames;
  gsize units_n, units_d;
  guint64 buffer_duration;
  GstFlowReturn flow_ret = GST_FLOW_OK;
  GstEvent *new_caps_event = NULL;
  gint alignment;
  GstRawBaseParse *raw_base_parse = GST_RAW_BASE_PARSE (parse);
  GstRawBaseParseClass *klass = GST_RAW_BASE_PARSE_GET_CLASS (parse);

  g_assert (klass->is_config_ready);
  g_assert (klass->get_caps_from_config);
  g_assert (klass->get_config_frame_size);
  g_assert (klass->get_units_per_second);

  /* We never skip any bytes this way. Instead, subclass takes care
   * of skipping any overhead (necessary, since the way it needs to
   * be skipped is completely subclass specific). */
  *skipsize = 0;

  /* The operations below access the current config. Protect
   * against race conditions by using the object lock. */
  GST_RAW_BASE_PARSE_CONFIG_MUTEX_LOCK (raw_base_parse);

  /* If the source pad caps haven't been set yet, or need to be
   * set again, do so now, BEFORE any buffers are pushed out */
  if (G_UNLIKELY (!raw_base_parse->src_caps_set)) {
    GstCaps *new_src_caps;

    if (G_UNLIKELY (!klass->is_config_ready (raw_base_parse,
                GST_RAW_BASE_PARSE_CONFIG_CURRENT))) {
      /* The current configuration is not ready. No caps can be
       * generated out of it.
       * The most likely reason for this is that the sink caps config
       * is the current one and no valid sink caps have been pushed
       * by upstream. Report the problem and exit. */

      if (gst_raw_base_parse_is_using_sink_caps (raw_base_parse)) {
        goto config_not_ready;
      } else {
        /* This should not be reached if the property config is active */
        g_assert_not_reached ();
      }
    }

    GST_DEBUG_OBJECT (parse,
        "setting src caps since this has not been done yet");

    /* Convert the current config to a caps structure to
     * inform downstream about the new format */
    if (!klass->get_caps_from_config (raw_base_parse,
            GST_RAW_BASE_PARSE_CONFIG_CURRENT, &new_src_caps)) {
      GST_ERROR_OBJECT (raw_base_parse,
          "could not get src caps from current config");
      flow_ret = GST_FLOW_NOT_NEGOTIATED;
      goto error_locked;
    }

    new_caps_event = gst_event_new_caps (new_src_caps);
    gst_caps_unref (new_src_caps);

    raw_base_parse->src_caps_set = TRUE;
  }

  frame_size =
      klass->get_config_frame_size (raw_base_parse,
      GST_RAW_BASE_PARSE_CONFIG_CURRENT);
  if (frame_size <= 0) {
    GST_ELEMENT_ERROR (parse, STREAM, FORMAT,
        ("Non strictly positive frame size"), (NULL));
    flow_ret = GST_FLOW_ERROR;
    goto error_locked;
  }

  in_size = gst_buffer_get_size (frame->buffer);

  /* drop incomplete frame at the end of the stream
   * https://bugzilla.gnome.org/show_bug.cgi?id=773666
   */
  if (GST_BASE_PARSE_DRAINING (parse) && in_size < frame_size) {
    GST_DEBUG_OBJECT (raw_base_parse,
        "Dropping %" G_GSIZE_FORMAT " bytes at EOS", in_size);
    frame->flags |= GST_BASE_PARSE_FRAME_FLAG_DROP;
    GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (raw_base_parse);

    return gst_base_parse_finish_frame (parse, frame, in_size);
  }

  /* gst_base_parse_set_min_frame_size() is called when the current
   * configuration changes and the change affects the frame size. This
   * means that a buffer must contain at least as many bytes as indicated
   * by the frame size. If there are fewer inside an error occurred;
   * either something in the parser went wrong, or the min frame size
   * wasn't updated properly. */
  g_assert (in_size >= frame_size);

  /* Determine how many complete frames would fit in the input buffer.
   * Then check if this amount exceeds the maximum number of frames
   * as indicated by the subclass. */
  num_out_frames = (in_size / frame_size);
  if (klass->get_max_frames_per_buffer) {
    guint max_num_out_frames = klass->get_max_frames_per_buffer (raw_base_parse,
        GST_RAW_BASE_PARSE_CONFIG_CURRENT);
    num_out_frames = MIN (num_out_frames, max_num_out_frames);
  }

  /* Ensure that the size of the buffers that get pushed downstream
   * is always an integer multiple of the frame size to prevent cases
   * where downstream gets buffers with incomplete frames. */
  out_size = num_out_frames * frame_size;

  /* Set the overhead size to ensure that timestamping excludes these
   * extra overhead bytes. */
  frame->overhead =
      klass->get_overhead_size ? klass->get_overhead_size (raw_base_parse,
      GST_RAW_BASE_PARSE_CONFIG_CURRENT) : 0;

  g_assert (out_size >= (guint) (frame->overhead));
  out_size -= frame->overhead;

  GST_LOG_OBJECT (raw_base_parse,
      "%" G_GSIZE_FORMAT " bytes input  %" G_GSIZE_FORMAT
      " bytes output (%u frame(s))  %d bytes overhead", in_size, out_size,
      num_out_frames, frame->overhead);

  /* Calculate buffer duration */
  klass->get_units_per_second (raw_base_parse, GST_FORMAT_BYTES,
      GST_RAW_BASE_PARSE_CONFIG_CURRENT, &units_n, &units_d);
  if (units_n == 0 || units_d == 0)
    buffer_duration = GST_CLOCK_TIME_NONE;
  else
    buffer_duration =
        gst_util_uint64_scale (out_size, GST_SECOND * units_d, units_n);

  if (klass->process) {
    GstBuffer *processed_data = NULL;

    if (!klass->process (raw_base_parse, GST_RAW_BASE_PARSE_CONFIG_CURRENT,
            frame->buffer, in_size, out_size, &processed_data))
      goto process_error;

    frame->out_buffer = processed_data;
  } else {
    frame->out_buffer = NULL;
  }

  if (klass->get_alignment
      && (alignment =
          klass->get_alignment (raw_base_parse,
              GST_RAW_BASE_PARSE_CONFIG_CURRENT)) != 1) {
    GstBuffer *aligned_buffer;

    aligned_buffer =
        gst_raw_base_parse_align_buffer (raw_base_parse, alignment,
        frame->out_buffer ? frame->out_buffer : frame->buffer, out_size);

    if (aligned_buffer) {
      if (frame->out_buffer)
        gst_buffer_unref (frame->out_buffer);
      frame->out_buffer = aligned_buffer;
    }
  }

  /* Set the duration of the output buffer, or if none exists, of
   * the input buffer. Do this after the process() call, since in
   * case out_buffer is set, the subclass has created a new buffer.
   * Instead of requiring subclasses to set the duration (which
   * anyway must always be buffer_duration), let's do it here. */
  if (frame->out_buffer != NULL)
    GST_BUFFER_DURATION (frame->out_buffer) = buffer_duration;
  else
    GST_BUFFER_DURATION (frame->buffer) = buffer_duration;

  /* Access to the current config is not needed in subsequent
   * operations, so the lock can be released */
  GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (raw_base_parse);

  /* If any new caps have to be pushed downstrean, do so
   * *before* the frame is finished */
  if (G_UNLIKELY (new_caps_event != NULL)) {
    gst_pad_push_event (GST_BASE_PARSE_SRC_PAD (raw_base_parse),
        new_caps_event);
    new_caps_event = NULL;
  }

  flow_ret =
      gst_base_parse_finish_frame (parse, frame, out_size + frame->overhead);

  return flow_ret;

config_not_ready:
  GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (raw_base_parse);
  GST_ELEMENT_ERROR (parse, STREAM, FORMAT,
      ("sink caps config is the current config, and it is not ready -"
          "upstream may not have pushed a caps event yet"), (NULL));
  flow_ret = GST_FLOW_ERROR;
  goto error_end;

process_error:
  GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (raw_base_parse);
  GST_ELEMENT_ERROR (parse, STREAM, DECODE, ("could not process data"), (NULL));
  flow_ret = GST_FLOW_ERROR;
  goto error_end;

error_locked:
  GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (raw_base_parse);
  goto error_end;

error_end:
  frame->flags |= GST_BASE_PARSE_FRAME_FLAG_DROP;
  if (new_caps_event != NULL)
    gst_event_unref (new_caps_event);
  return flow_ret;
}

static gboolean
gst_raw_base_parse_convert (GstBaseParse * parse, GstFormat src_format,
    gint64 src_value, GstFormat dest_format, gint64 * dest_value)
{
  GstRawBaseParse *raw_base_parse = GST_RAW_BASE_PARSE (parse);
  GstRawBaseParseClass *klass = GST_RAW_BASE_PARSE_GET_CLASS (parse);
  gboolean ret = TRUE;
  gsize units_n, units_d;

  g_assert (klass->is_config_ready);
  g_assert (klass->get_units_per_second);

  /* The operations below access the current config. Protect
   * against race conditions by using the object lock. */
  GST_RAW_BASE_PARSE_CONFIG_MUTEX_LOCK (raw_base_parse);

  if (!klass->is_config_ready (raw_base_parse,
          GST_RAW_BASE_PARSE_CONFIG_CURRENT)) {
    if (gst_raw_base_parse_is_using_sink_caps (raw_base_parse)) {
      goto config_not_ready;
    } else {
      /* This should not be reached if the property config is active */
      g_assert_not_reached ();
    }
  }

  if (G_UNLIKELY (src_format == dest_format)) {
    *dest_value = src_value;
  } else if ((src_format == GST_FORMAT_TIME || dest_format == GST_FORMAT_TIME)
      && gst_raw_base_parse_is_gstformat_supported (raw_base_parse, src_format)
      && gst_raw_base_parse_is_gstformat_supported (raw_base_parse,
          dest_format)) {
    /* Perform conversions here if either the src or dest format
     * are GST_FORMAT_TIME and the other format is supported by
     * the subclass. This is because we perform TIME<->non-TIME
     * conversions here. Typically, subclasses only support
     * BYTES and DEFAULT formats. */

    if (src_format == GST_FORMAT_TIME) {
      /* The source format is time, so perform a TIME -> non-TIME conversion */
      klass->get_units_per_second (raw_base_parse, dest_format,
          GST_RAW_BASE_PARSE_CONFIG_CURRENT, &units_n, &units_d);
      *dest_value = (units_n == 0
          || units_d == 0) ? src_value : gst_util_uint64_scale (src_value,
          units_n, GST_SECOND * units_d);
    } else {
      /* The dest format is time, so perform a non-TIME -> TIME conversion */
      klass->get_units_per_second (raw_base_parse, src_format,
          GST_RAW_BASE_PARSE_CONFIG_CURRENT, &units_n, &units_d);
      *dest_value = (units_n == 0
          || units_d == 0) ? src_value : gst_util_uint64_scale (src_value,
          GST_SECOND * units_d, units_n);
    }
  } else {
    /* Fallback for other conversions */
    ret =
        gst_base_parse_convert_default (parse, src_format, src_value,
        dest_format, dest_value);
  }

  GST_DEBUG_OBJECT (parse,
      "converted %s -> %s  %" G_GINT64_FORMAT " -> %" GST_TIME_FORMAT,
      gst_format_get_name (src_format), gst_format_get_name (dest_format),
      src_value, GST_TIME_ARGS (*dest_value));

  GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (raw_base_parse);
  return ret;

config_not_ready:
  GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK (raw_base_parse);
  GST_ELEMENT_ERROR (parse, STREAM, FORMAT,
      ("sink caps config is the current config, and it is not ready - "
          "upstream may not have pushed a caps event yet"), (NULL));
  return FALSE;
}

static gboolean
gst_raw_base_parse_is_using_sink_caps (GstRawBaseParse * raw_base_parse)
{
  /* must be called with lock */
  GstRawBaseParseClass *klass = GST_RAW_BASE_PARSE_GET_CLASS (raw_base_parse);
  g_assert (klass->get_current_config);
  return klass->get_current_config (raw_base_parse) ==
      GST_RAW_BASE_PARSE_CONFIG_SINKCAPS;
}

static gboolean
gst_raw_base_parse_is_gstformat_supported (GstRawBaseParse * raw_base_parse,
    GstFormat format)
{
  /* must be called with lock */
  GstRawBaseParseClass *klass = GST_RAW_BASE_PARSE_GET_CLASS (raw_base_parse);
  g_assert (klass->is_unit_format_supported);
  return klass->is_unit_format_supported (raw_base_parse, format);
}

/**
 * gst_raw_base_parse_invalidate_src_caps:
 * @raw_base_parse: a #GstRawBaseParse instance
 *
 * Flags the current source caps as invalid. Before the next downstream
 * buffer push, @get_caps_from_config is called, and the created caps are
 * pushed downstream in a new caps event, This is used if for example the
 * properties configuration is modified in the subclass.
 *
 * Note that this must be called with the parser lock held. Use the
 * GST_RAW_BASE_PARSE_CONFIG_MUTEX_LOCK() and GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK()
 * macros for this purpose.
 */
void
gst_raw_base_parse_invalidate_src_caps (GstRawBaseParse * raw_base_parse)
{
  /* must be called with lock */
  g_assert (raw_base_parse != NULL);
  raw_base_parse->src_caps_set = FALSE;
}
