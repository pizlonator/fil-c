/* GStreamer
 * Copyright (C) <2017> Carlos Rafael Giani <dv at pseudoterminal dot org>
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
 * SECTION:gstnonstreamaudiodecoder
 * @short_description: Base class for decoding of non-streaming audio
 * @see_also: #GstAudioDecoder
 *
 * This base class is for decoders which do not operate on a streaming model.
 * That is: they load the encoded media at once, as part of an initialization,
 * and afterwards can decode samples (sometimes referred to as "rendering the
 * samples").
 *
 * This sets it apart from GstAudioDecoder, which is a base class for
 * streaming audio decoders.
 *
 * The base class is conceptually a mix between decoder and parser. This is
 * unavoidable, since virtually no format that isn't streaming based has a
 * clear distinction between parsing and decoding. As a result, this class
 * also handles seeking.
 *
 * Non-streaming audio formats tend to have some characteristics unknown to
 * more "regular" bitstreams. These include subsongs and looping.
 *
 * Subsongs are a set of songs-within-a-song. An analogy would be a multitrack
 * recording, where each track is its own song. The first subsong is typically
 * the "main" one. Subsongs were popular for video games to enable context-
 * aware music; for example, subsong `#0` would be the "main" song, `#1` would be
 * an alternate song playing when a fight started, `#2` would be heard during
 * conversations etc. The base class is designed to always have at least one
 * subsong. If the subclass doesn't provide any, the base class creates a
 * "pseudo" subsong, which is actually the whole song.
 * Downstream is informed about the subsong using a table of contents (TOC),
 * but only if there are at least 2 subsongs.
 *
 * Looping refers to jumps within the song, typically backwards to the loop
 * start (although bi-directional looping is possible). The loop is defined
 * by a chronological start and end; once the playback position reaches the
 * loop end, it jumps back to the loop start.
 * Depending on the subclass, looping may not be possible at all, or it
 * may only be possible to enable/disable it (that is, either no looping, or
 * an infinite amount of loops), or it may allow for defining a finite number
 * of times the loop is repeated.
 * Looping can affect output in two ways. Either, the playback position is
 * reset to the start of the loop, similar to what happens after a seek event.
 * Or, it is not reset, so the pipeline sees playback steadily moving forwards,
 * the playback position monotonically increasing. However, seeking must
 * always happen within the confines of the defined subsong duration; for
 * example, if a subsong is 2 minutes long, steady playback is at 5 minutes
 * (because infinite looping is enabled), then seeking will still place the
 * position within the 2 minute period.
 * Loop count 0 means no looping. Loop count -1 means infinite looping.
 * Nonzero positive values indicate how often a loop shall occur.
 *
 * If the initial subsong and loop count are set to values the subclass does
 * not support, the subclass has a chance to correct these values.
 * @get_property then reports the corrected versions.
 *
 * The base class operates as follows:
 * * Unloaded mode
 *   - Initial values are set. If a current subsong has already been
 *     defined (for example over the command line with gst-launch), then
 *     the subsong index is copied over to current_subsong .
 *     Same goes for the num-loops and output-mode properties.
 *     Media is NOT loaded yet.
 *   - Once the sinkpad is activated, the process continues. The sinkpad is
 *     activated in push mode, and the class accumulates the incoming media
 *     data in an adapter inside the sinkpad's chain function until either an
 *     EOS event is received from upstream, or the number of bytes reported
 *     by upstream is reached. Then it loads the media, and starts the decoder
 *     output task.
 *   - If upstream cannot respond to the size query (in bytes) of @load_from_buffer
 *     fails, an error is reported, and the pipeline stops.
 *   - If there are no errors, @load_from_buffer is called to load the media. The
 *     subclass must at least call gst_nonstream_audio_decoder_set_output_format()
 *     there, and is free to make use of the initial subsong, output mode, and
 *     position. If the actual output mode or position differs from the initial
 *     value,it must set the initial value to the actual one (for example, if
 *     the actual starting position is always 0, set *initial_position to 0).
 *     If loading is unsuccessful, an error is reported, and the pipeline
 *     stops. Otherwise, the base class calls @get_current_subsong to retrieve
 *     the actual current subsong, @get_subsong_duration to report the current
 *     subsong's duration in a duration event and message, and @get_subsong_tags
 *     to send tags downstream in an event (these functions are optional; if
 *     set to NULL, the associated operation is skipped). Afterwards, the base
 *     class switches to loaded mode, and starts the decoder output task.
 *
 * * Loaded mode</title>
 *   - Inside the decoder output task, the base class repeatedly calls @decode,
 *     which returns a buffer with decoded, ready-to-play samples. If the
 *     subclass reached the end of playback, @decode returns FALSE, otherwise
 *     TRUE.
 *   - Upon reaching a loop end, subclass either ignores that, or loops back
 *     to the beginning of the loop. In the latter case, if the output mode is set
 *     to LOOPING, the subclass must call gst_nonstream_audio_decoder_handle_loop()
 *     *after* the playback position moved to the start of the loop. In
 *     STEADY mode, the subclass must *not* call this function.
 *     Since many decoders only provide a callback for when the looping occurs,
 *     and that looping occurs inside the decoding operation itself, the following
 *     mechanism for subclass is suggested: set a flag inside such a callback.
 *     Then, in the next @decode call, before doing the decoding, check this flag.
 *     If it is set, gst_nonstream_audio_decoder_handle_loop() is called, and the
 *     flag is cleared.
 *     (This function call is necessary in LOOPING mode because it updates the
 *     current segment and makes sure the next buffer that is sent downstream
 *     has its DISCONT flag set.)
 *   - When the current subsong is switched, @set_current_subsong is called.
 *     If it fails, a warning is reported, and nothing else is done. Otherwise,
 *     it calls @get_subsong_duration to get the new current subsongs's
 *     duration, @get_subsong_tags to get its tags, reports a new duration
 *     (i.e. it sends a duration event downstream and generates a duration
 *     message), updates the current segment, and sends the subsong's tags in
 *     an event downstream. (If @set_current_subsong has been set to NULL by
 *     the subclass, attempts to set a current subsong are ignored; likewise,
 *     if @get_subsong_duration is NULL, no duration is reported, and if
 *     @get_subsong_tags is NULL, no tags are sent downstream.)
 *   - When an attempt is made to switch the output mode, it is checked against
 *     the bitmask returned by @get_supported_output_modes. If the proposed
 *     new output mode is supported, the current segment is updated
 *     (it is open-ended in STEADY mode, and covers the (sub)song length in
 *     LOOPING mode), and the subclass' @set_output_mode function is called
 *     unless it is set to NULL. Subclasses should reset internal loop counters
 *     in this function.
 *
 * The relationship between (sub)song duration, output mode, and number of loops
 * is defined this way (this is all done by the base class automatically):
 *
 * * Segments have their duration and stop values set to GST_CLOCK_TIME_NONE in
 *   STEADY mode, and to the duration of the (sub)song in LOOPING mode.
 *
 * * The duration that is returned to a DURATION query is always the duration
 *   of the (sub)song, regardless of number of loops or output mode. The same
 *   goes for DURATION messages and tags.
 *
 * * If the number of loops is >0 or -1, durations of TOC entries are set to
 *   the duration of the respective subsong in LOOPING mode and to G_MAXINT64 in
 *   STEADY mode. If the number of loops is 0, entry durations are set to the
 *   subsong duration regardless of the output mode.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <gst/gst.h>
#include <gst/audio/audio.h>

#include "gstnonstreamaudiodecoder.h"


GST_DEBUG_CATEGORY (nonstream_audiodecoder_debug);
#define GST_CAT_DEFAULT nonstream_audiodecoder_debug


enum
{
  PROP_0,
  PROP_CURRENT_SUBSONG,
  PROP_SUBSONG_MODE,
  PROP_NUM_LOOPS,
  PROP_OUTPUT_MODE
};

#define DEFAULT_CURRENT_SUBSONG 0
#define DEFAULT_SUBSONG_MODE GST_NONSTREAM_AUDIO_SUBSONG_MODE_DECODER_DEFAULT
#define DEFAULT_NUM_SUBSONGS 0
#define DEFAULT_NUM_LOOPS 0
#define DEFAULT_OUTPUT_MODE GST_NONSTREAM_AUDIO_OUTPUT_MODE_STEADY




static GstElementClass *gst_nonstream_audio_decoder_parent_class = NULL;

static void
gst_nonstream_audio_decoder_class_init (GstNonstreamAudioDecoderClass * klass);
static void gst_nonstream_audio_decoder_init (GstNonstreamAudioDecoder * dec,
    GstNonstreamAudioDecoderClass * klass);

static void gst_nonstream_audio_decoder_finalize (GObject * object);
static void gst_nonstream_audio_decoder_set_property (GObject * object,
    guint prop_id, GValue const *value, GParamSpec * pspec);
static void gst_nonstream_audio_decoder_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static GstStateChangeReturn gst_nonstream_audio_decoder_change_state (GstElement
    * element, GstStateChange transition);

static gboolean gst_nonstream_audio_decoder_sink_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static gboolean gst_nonstream_audio_decoder_sink_query (GstPad * pad,
    GstObject * parent, GstQuery * query);
static GstFlowReturn gst_nonstream_audio_decoder_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buffer);

static gboolean gst_nonstream_audio_decoder_src_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static gboolean gst_nonstream_audio_decoder_src_query (GstPad * pad,
    GstObject * parent, GstQuery * query);

static void
gst_nonstream_audio_decoder_set_initial_state (GstNonstreamAudioDecoder * dec);
static void gst_nonstream_audio_decoder_cleanup_state (GstNonstreamAudioDecoder
    * dec);

static gboolean gst_nonstream_audio_decoder_negotiate (GstNonstreamAudioDecoder
    * dec);

static gboolean
gst_nonstream_audio_decoder_negotiate_default (GstNonstreamAudioDecoder * dec);
static gboolean
gst_nonstream_audio_decoder_decide_allocation_default (GstNonstreamAudioDecoder
    * dec, GstQuery * query);
static gboolean
gst_nonstream_audio_decoder_propose_allocation_default (GstNonstreamAudioDecoder
    * dec, GstQuery * query);

static gboolean
gst_nonstream_audio_decoder_get_upstream_size (GstNonstreamAudioDecoder * dec,
    gint64 * length);
static gboolean
gst_nonstream_audio_decoder_load_from_buffer (GstNonstreamAudioDecoder * dec,
    GstBuffer * buffer);
static gboolean
gst_nonstream_audio_decoder_load_from_custom (GstNonstreamAudioDecoder * dec);
static gboolean
gst_nonstream_audio_decoder_finish_load (GstNonstreamAudioDecoder * dec,
    gboolean load_ok, GstClockTime initial_position,
    gboolean send_stream_start);

static gboolean gst_nonstream_audio_decoder_start_task (GstNonstreamAudioDecoder
    * dec);
static gboolean gst_nonstream_audio_decoder_stop_task (GstNonstreamAudioDecoder
    * dec);

static gboolean
gst_nonstream_audio_decoder_switch_to_subsong (GstNonstreamAudioDecoder * dec,
    guint new_subsong, guint32 const *seqnum);

static void gst_nonstream_audio_decoder_update_toc (GstNonstreamAudioDecoder *
    dec, GstNonstreamAudioDecoderClass * klass);
static void
gst_nonstream_audio_decoder_update_subsong_duration (GstNonstreamAudioDecoder *
    dec, GstClockTime duration);
static void
gst_nonstream_audio_decoder_output_new_segment (GstNonstreamAudioDecoder * dec,
    GstClockTime start_position);
static gboolean gst_nonstream_audio_decoder_do_seek (GstNonstreamAudioDecoder *
    dec, GstEvent * event);

static GstTagList
    * gst_nonstream_audio_decoder_add_main_tags (GstNonstreamAudioDecoder * dec,
    GstTagList * tags);

static void gst_nonstream_audio_decoder_output_task (GstNonstreamAudioDecoder *
    dec);

static char const *get_seek_type_name (GstSeekType seek_type);




static GType gst_nonstream_audio_decoder_output_mode_get_type (void);
#define GST_TYPE_NONSTREAM_AUDIO_DECODER_OUTPUT_MODE (gst_nonstream_audio_decoder_output_mode_get_type())

static GType gst_nonstream_audio_decoder_subsong_mode_get_type (void);
#define GST_TYPE_NONSTREAM_AUDIO_DECODER_SUBSONG_MODE (gst_nonstream_audio_decoder_subsong_mode_get_type())


static GType
gst_nonstream_audio_decoder_output_mode_get_type (void)
{
  static GType gst_nonstream_audio_decoder_output_mode_type = 0;

  if (!gst_nonstream_audio_decoder_output_mode_type) {
    static GEnumValue output_mode_values[] = {
      {GST_NONSTREAM_AUDIO_OUTPUT_MODE_LOOPING, "Looping output", "looping"},
      {GST_NONSTREAM_AUDIO_OUTPUT_MODE_STEADY, "Steady output", "steady"},
      {0, NULL, NULL},
    };

    gst_nonstream_audio_decoder_output_mode_type =
        g_enum_register_static ("GstNonstreamAudioOutputMode",
        output_mode_values);
  }

  return gst_nonstream_audio_decoder_output_mode_type;
}


static GType
gst_nonstream_audio_decoder_subsong_mode_get_type (void)
{
  static GType gst_nonstream_audio_decoder_subsong_mode_type = 0;

  if (!gst_nonstream_audio_decoder_subsong_mode_type) {
    static GEnumValue subsong_mode_values[] = {
      {GST_NONSTREAM_AUDIO_SUBSONG_MODE_SINGLE, "Play single subsong",
          "single"},
      {GST_NONSTREAM_AUDIO_SUBSONG_MODE_ALL, "Play all subsongs", "all"},
      {GST_NONSTREAM_AUDIO_SUBSONG_MODE_DECODER_DEFAULT,
          "Decoder specific default behavior", "default"},
      {0, NULL, NULL},
    };

    gst_nonstream_audio_decoder_subsong_mode_type =
        g_enum_register_static ("GstNonstreamAudioSubsongMode",
        subsong_mode_values);
  }

  return gst_nonstream_audio_decoder_subsong_mode_type;
}



/* Manually defining the GType instead of using G_DEFINE_TYPE_WITH_CODE()
 * because the _init() function needs to be able to access the derived
 * class' sink- and srcpads */


GType
gst_nonstream_audio_decoder_get_type (void)
{
  static gsize nonstream_audio_decoder_type = 0;

  if (g_once_init_enter (&nonstream_audio_decoder_type)) {
    GType type_;
    static const GTypeInfo nonstream_audio_decoder_info = {
      sizeof (GstNonstreamAudioDecoderClass),
      NULL,
      NULL,
      (GClassInitFunc) gst_nonstream_audio_decoder_class_init,
      NULL,
      NULL,
      sizeof (GstNonstreamAudioDecoder),
      0,
      (GInstanceInitFunc) gst_nonstream_audio_decoder_init,
      NULL
    };

    type_ = g_type_register_static (GST_TYPE_ELEMENT,
        "GstNonstreamAudioDecoder",
        &nonstream_audio_decoder_info, G_TYPE_FLAG_ABSTRACT);
    g_once_init_leave (&nonstream_audio_decoder_type, type_);
  }

  return nonstream_audio_decoder_type;
}




static void
gst_nonstream_audio_decoder_class_init (GstNonstreamAudioDecoderClass * klass)
{
  GObjectClass *object_class;
  GstElementClass *element_class;

  object_class = G_OBJECT_CLASS (klass);
  element_class = GST_ELEMENT_CLASS (klass);

  gst_nonstream_audio_decoder_parent_class = g_type_class_peek_parent (klass);

  GST_DEBUG_CATEGORY_INIT (nonstream_audiodecoder_debug,
      "nonstreamaudiodecoder", 0, "nonstream audio decoder base class");

  object_class->finalize =
      GST_DEBUG_FUNCPTR (gst_nonstream_audio_decoder_finalize);
  object_class->set_property =
      GST_DEBUG_FUNCPTR (gst_nonstream_audio_decoder_set_property);
  object_class->get_property =
      GST_DEBUG_FUNCPTR (gst_nonstream_audio_decoder_get_property);
  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_nonstream_audio_decoder_change_state);

  klass->seek = NULL;
  klass->tell = NULL;

  klass->load_from_buffer = NULL;
  klass->load_from_custom = NULL;

  klass->get_main_tags = NULL;

  klass->get_current_subsong = NULL;
  klass->set_current_subsong = NULL;

  klass->get_num_subsongs = NULL;
  klass->get_subsong_duration = NULL;
  klass->get_subsong_tags = NULL;
  klass->set_subsong_mode = NULL;

  klass->set_num_loops = NULL;
  klass->get_num_loops = NULL;

  klass->decode = NULL;

  klass->negotiate =
      GST_DEBUG_FUNCPTR (gst_nonstream_audio_decoder_negotiate_default);

  klass->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_nonstream_audio_decoder_decide_allocation_default);
  klass->propose_allocation =
      GST_DEBUG_FUNCPTR
      (gst_nonstream_audio_decoder_propose_allocation_default);

  klass->loads_from_sinkpad = TRUE;

  g_object_class_install_property (object_class,
      PROP_CURRENT_SUBSONG,
      g_param_spec_uint ("current-subsong",
          "Currently active subsong",
          "Subsong that is currently selected for playback",
          0, G_MAXUINT,
          DEFAULT_CURRENT_SUBSONG, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );

  g_object_class_install_property (object_class,
      PROP_SUBSONG_MODE,
      g_param_spec_enum ("subsong-mode",
          "Subsong mode",
          "Mode which defines how to treat subsongs",
          GST_TYPE_NONSTREAM_AUDIO_DECODER_SUBSONG_MODE,
          DEFAULT_SUBSONG_MODE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );

  g_object_class_install_property (object_class,
      PROP_NUM_LOOPS,
      g_param_spec_int ("num-loops",
          "Number of playback loops",
          "Number of times a playback loop shall be executed (special values: 0 = no looping; -1 = infinite loop)",
          -1, G_MAXINT,
          DEFAULT_NUM_LOOPS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );

  g_object_class_install_property (object_class,
      PROP_OUTPUT_MODE,
      g_param_spec_enum ("output-mode",
          "Output mode",
          "Which mode playback shall use when a loop is encountered; looping = reset position to start of loop, steady = do not reset position",
          GST_TYPE_NONSTREAM_AUDIO_DECODER_OUTPUT_MODE,
          DEFAULT_OUTPUT_MODE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );
}


static void
gst_nonstream_audio_decoder_init (GstNonstreamAudioDecoder * dec,
    GstNonstreamAudioDecoderClass * klass)
{
  GstPadTemplate *pad_template;

  /* These are set here, not in gst_nonstream_audio_decoder_set_initial_state(),
   * because these are values for the properties; they are not supposed to be
   * reset in the READY->NULL state change */
  dec->current_subsong = DEFAULT_CURRENT_SUBSONG;
  dec->subsong_mode = DEFAULT_SUBSONG_MODE;
  dec->output_mode = DEFAULT_OUTPUT_MODE;
  dec->num_loops = DEFAULT_NUM_LOOPS;

  /* Calling this here, not in the NULL->READY state change,
   * to make sure get_property calls return valid values */
  gst_nonstream_audio_decoder_set_initial_state (dec);

  dec->input_data_adapter = gst_adapter_new ();
  g_mutex_init (&(dec->mutex));

  {
    /* set up src pad */

    pad_template =
        gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass), "src");
    g_return_if_fail (pad_template != NULL);    /* derived class is supposed to define a src pad template */

    dec->srcpad = gst_pad_new_from_template (pad_template, "src");
    gst_pad_set_event_function (dec->srcpad,
        GST_DEBUG_FUNCPTR (gst_nonstream_audio_decoder_src_event));
    gst_pad_set_query_function (dec->srcpad,
        GST_DEBUG_FUNCPTR (gst_nonstream_audio_decoder_src_query));
    gst_element_add_pad (GST_ELEMENT (dec), dec->srcpad);
  }

  if (klass->loads_from_sinkpad) {
    /* set up sink pad if this class loads from a sinkpad */

    pad_template =
        gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass), "sink");
    g_return_if_fail (pad_template != NULL);    /* derived class is supposed to define a sink pad template */

    dec->sinkpad = gst_pad_new_from_template (pad_template, "sink");
    gst_pad_set_event_function (dec->sinkpad,
        GST_DEBUG_FUNCPTR (gst_nonstream_audio_decoder_sink_event));
    gst_pad_set_query_function (dec->sinkpad,
        GST_DEBUG_FUNCPTR (gst_nonstream_audio_decoder_sink_query));
    gst_pad_set_chain_function (dec->sinkpad,
        GST_DEBUG_FUNCPTR (gst_nonstream_audio_decoder_chain));
    gst_element_add_pad (GST_ELEMENT (dec), dec->sinkpad);
  }
}




static void
gst_nonstream_audio_decoder_finalize (GObject * object)
{
  GstNonstreamAudioDecoder *dec = GST_NONSTREAM_AUDIO_DECODER (object);

  g_mutex_clear (&(dec->mutex));
  g_object_unref (G_OBJECT (dec->input_data_adapter));

  G_OBJECT_CLASS (gst_nonstream_audio_decoder_parent_class)->finalize (object);
}


static void
gst_nonstream_audio_decoder_set_property (GObject * object, guint prop_id,
    GValue const *value, GParamSpec * pspec)
{
  GstNonstreamAudioDecoder *dec = GST_NONSTREAM_AUDIO_DECODER (object);
  GstNonstreamAudioDecoderClass *klass =
      GST_NONSTREAM_AUDIO_DECODER_GET_CLASS (dec);

  switch (prop_id) {
    case PROP_OUTPUT_MODE:
    {
      GstNonstreamAudioOutputMode new_output_mode;
      new_output_mode = g_value_get_enum (value);

      g_assert (klass->get_supported_output_modes);

      if ((klass->get_supported_output_modes (dec) & (1u << new_output_mode)) ==
          0) {
        GST_WARNING_OBJECT (dec,
            "could not set output mode to %s (not supported by subclass)",
            (new_output_mode ==
                GST_NONSTREAM_AUDIO_OUTPUT_MODE_STEADY) ? "steady" : "looping");
        break;
      }

      GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (dec);
      if (new_output_mode != dec->output_mode) {
        gboolean proceed = TRUE;

        if (dec->loaded_mode) {
          GstClockTime cur_position;

          if (klass->set_output_mode != NULL) {
            if (klass->set_output_mode (dec, new_output_mode, &cur_position))
              proceed = TRUE;
            else {
              proceed = FALSE;
              GST_WARNING_OBJECT (dec, "switching to new output mode failed");
            }
          } else {
            GST_DEBUG_OBJECT (dec,
                "cannot call set_output_mode, since it is NULL");
            proceed = FALSE;
          }

          if (proceed) {
            gst_nonstream_audio_decoder_output_new_segment (dec, cur_position);
            dec->output_mode = new_output_mode;
          }
        }

        if (proceed) {
          /* store output mode in case the property is set before the media got loaded */
          dec->output_mode = new_output_mode;
        }
      }
      GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (dec);

      break;
    }

    case PROP_CURRENT_SUBSONG:
    {
      guint new_subsong = g_value_get_uint (value);
      gst_nonstream_audio_decoder_switch_to_subsong (dec, new_subsong, NULL);

      break;
    }

    case PROP_SUBSONG_MODE:
    {
      GstNonstreamAudioSubsongMode new_subsong_mode = g_value_get_enum (value);

      GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (dec);
      if (new_subsong_mode != dec->subsong_mode) {
        gboolean proceed = TRUE;

        if (dec->loaded_mode) {
          GstClockTime cur_position;

          if (klass->set_subsong_mode != NULL) {
            if (klass->set_subsong_mode (dec, new_subsong_mode, &cur_position))
              proceed = TRUE;
            else {
              proceed = FALSE;
              GST_WARNING_OBJECT (dec, "switching to new subsong mode failed");
            }
          } else {
            GST_DEBUG_OBJECT (dec,
                "cannot call set_subsong_mode, since it is NULL");
            proceed = FALSE;
          }

          if (proceed) {
            if (GST_CLOCK_TIME_IS_VALID (cur_position))
              gst_nonstream_audio_decoder_output_new_segment (dec,
                  cur_position);
            dec->subsong_mode = new_subsong_mode;
          }
        }

        if (proceed) {
          /* store subsong mode in case the property is set before the media got loaded */
          dec->subsong_mode = new_subsong_mode;
        }
      }
      GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (dec);

      break;
    }

    case PROP_NUM_LOOPS:
    {
      gint new_num_loops = g_value_get_int (value);

      GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (dec);
      if (new_num_loops != dec->num_loops) {
        if (dec->loaded_mode) {
          if (klass->set_num_loops != NULL) {
            if (!(klass->set_num_loops (dec, new_num_loops)))
              GST_WARNING_OBJECT (dec, "setting number of loops to %u failed",
                  new_num_loops);
          } else
            GST_DEBUG_OBJECT (dec,
                "cannot call set_num_loops, since it is NULL");
        }

        /* store number of loops in case the property is set before the media got loaded */
        dec->num_loops = new_num_loops;
      }
      GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (dec);

      break;
    }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static void
gst_nonstream_audio_decoder_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstNonstreamAudioDecoder *dec = GST_NONSTREAM_AUDIO_DECODER (object);

  switch (prop_id) {
    case PROP_OUTPUT_MODE:
    {
      GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (dec);
      g_value_set_enum (value, dec->output_mode);
      GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (dec);
      break;
    }

    case PROP_CURRENT_SUBSONG:
    {
      GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (dec);
      g_value_set_uint (value, dec->current_subsong);
      GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (dec);
      break;
    }

    case PROP_SUBSONG_MODE:
    {
      GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (dec);
      g_value_set_enum (value, dec->subsong_mode);
      GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (dec);
      break;
    }

    case PROP_NUM_LOOPS:
    {
      GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (dec);
      g_value_set_int (value, dec->num_loops);
      GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (dec);
      break;
    }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}



static GstStateChangeReturn
gst_nonstream_audio_decoder_change_state (GstElement * element,
    GstStateChange transition)
{
  GstStateChangeReturn ret;

  ret =
      GST_ELEMENT_CLASS (gst_nonstream_audio_decoder_parent_class)->change_state
      (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    {

      GstNonstreamAudioDecoder *dec = GST_NONSTREAM_AUDIO_DECODER (element);
      GstNonstreamAudioDecoderClass *klass =
          GST_NONSTREAM_AUDIO_DECODER_GET_CLASS (dec);

      /* For decoders that load with some custom method,
       * this is now the time to load
       *
       * It is done *after* calling the parent class' change_state vfunc,
       * since the pad states need to be set up in order for the loading
       * to succeed, since it will try to push a new_caps event
       * downstream etc. (upwards state changes typically are handled
       * *before* calling the parent class' change_state vfunc ; this is
       * a special case) */
      if (!(klass->loads_from_sinkpad) && !(dec->loaded_mode)) {
        gboolean ret;

        /* load_from_custom is required if loads_from_sinkpad is FALSE */
        g_assert (klass->load_from_custom != NULL);

        ret = gst_nonstream_audio_decoder_load_from_custom (dec);

        if (!ret) {
          GST_ERROR_OBJECT (dec, "loading from custom source failed");
          return GST_STATE_CHANGE_FAILURE;
        }

        if (!gst_nonstream_audio_decoder_start_task (dec))
          return GST_STATE_CHANGE_FAILURE;

      }

      break;
    }

    case GST_STATE_CHANGE_PAUSED_TO_READY:
    {
      GstNonstreamAudioDecoder *dec = GST_NONSTREAM_AUDIO_DECODER (element);
      if (!gst_nonstream_audio_decoder_stop_task (dec))
        return GST_STATE_CHANGE_FAILURE;
      break;
    }

    case GST_STATE_CHANGE_READY_TO_NULL:
    {
      GstNonstreamAudioDecoder *dec = GST_NONSTREAM_AUDIO_DECODER (element);

      /* In the READY->NULL state change, reset the decoder to an
       * initial state ensure it can be used for a fresh new session */
      gst_nonstream_audio_decoder_cleanup_state (dec);
      break;
    }

    default:
      break;
  }

  return ret;
}



static gboolean
gst_nonstream_audio_decoder_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  gboolean res = FALSE;
  GstNonstreamAudioDecoder *dec = GST_NONSTREAM_AUDIO_DECODER (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEGMENT:
    {
      /* Upstream sends in a byte segment, which is uninteresting here,
       * since a custom segment event is generated anyway */
      gst_event_unref (event);
      res = TRUE;
      break;
    }

    case GST_EVENT_EOS:
    {
      gsize avail_size;
      GstBuffer *adapter_buffer;

      if (dec->loaded_mode) {
        /* If media has already been loaded, then the decoder
         * task has been started; the EOS event can be ignored */

        GST_DEBUG_OBJECT (dec,
            "EOS received after media was loaded -> ignoring");
        res = TRUE;
      } else {
        /* take all data in the input data adapter,
         * and try to load the media from it */

        avail_size = gst_adapter_available (dec->input_data_adapter);
        if (avail_size == 0) {
          GST_ELEMENT_ERROR (dec, STREAM, DECODE, (NULL),
              ("EOS event raised, but no data was received - cannot load anything"));
          return FALSE;
        }

        adapter_buffer =
            gst_adapter_take_buffer (dec->input_data_adapter, avail_size);

        if (!gst_nonstream_audio_decoder_load_from_buffer (dec, adapter_buffer)) {
          return FALSE;
        }

        res = gst_nonstream_audio_decoder_start_task (dec);
      }

      break;
    }

    default:
      res = gst_pad_event_default (pad, parent, event);
  }

  return res;
}


static gboolean
gst_nonstream_audio_decoder_sink_query (GstPad * pad, GstObject * parent,
    GstQuery * query)
{
  gboolean res = FALSE;
  GstNonstreamAudioDecoder *dec;
  GstNonstreamAudioDecoderClass *klass;

  dec = GST_NONSTREAM_AUDIO_DECODER (parent);
  klass = GST_NONSTREAM_AUDIO_DECODER_GET_CLASS (dec);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_ALLOCATION:
    {
      if (klass->propose_allocation != NULL)
        res = klass->propose_allocation (dec, query);

      break;
    }

    default:
      res = gst_pad_query_default (pad, parent, query);
  }

  return res;
}


static GstFlowReturn
gst_nonstream_audio_decoder_chain (G_GNUC_UNUSED GstPad * pad,
    GstObject * parent, GstBuffer * buffer)
{
  GstFlowReturn flow_ret = GST_FLOW_OK;
  GstNonstreamAudioDecoder *dec = GST_NONSTREAM_AUDIO_DECODER (parent);

  /* query upstream size in bytes to know how many bytes to expect
   * this is a safety measure to prevent the case when upstream never
   * reaches EOS (or only after a long time) and we keep loading and
   * loading and eventually run out of memory */
  if (dec->upstream_size < 0) {
    if (!gst_nonstream_audio_decoder_get_upstream_size (dec,
            &(dec->upstream_size))) {
      GST_ELEMENT_ERROR (dec, STREAM, DECODE, (NULL),
          ("Cannot load - upstream size (in bytes) could not be determined"));
      return GST_FLOW_ERROR;
    }
  }

  if (dec->loaded_mode) {
    /* media is already loaded - discard any incoming
     * buffers, since they are not needed */

    GST_DEBUG_OBJECT (dec, "received data after media was loaded - ignoring");

    gst_buffer_unref (buffer);
  } else {
    /* accumulate data until end-of-stream or the upstream
     * size is reached, then load media and commence playback */

    gint64 avail_size;

    gst_adapter_push (dec->input_data_adapter, buffer);
    avail_size = gst_adapter_available (dec->input_data_adapter);
    if (avail_size >= dec->upstream_size) {
      GstBuffer *adapter_buffer =
          gst_adapter_take_buffer (dec->input_data_adapter, avail_size);

      if (gst_nonstream_audio_decoder_load_from_buffer (dec, adapter_buffer))
        flow_ret =
            gst_nonstream_audio_decoder_start_task (dec) ? GST_FLOW_OK :
            GST_FLOW_ERROR;
      else
        flow_ret = GST_FLOW_ERROR;
    }
  }

  return flow_ret;
}



static gboolean
gst_nonstream_audio_decoder_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  gboolean res = FALSE;
  GstNonstreamAudioDecoder *dec = GST_NONSTREAM_AUDIO_DECODER (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
    {
      res = gst_nonstream_audio_decoder_do_seek (dec, event);
      break;
    }

    case GST_EVENT_TOC_SELECT:
    {
      /* NOTE: This event may be received multiple times if it
       * was originally sent to a bin containing multiple sink
       * elements (for example, playbin). This is OK and does
       * not break anything. */

      gchar *uid = NULL;
      guint subsong_idx = 0;
      guint32 seqnum;

      gst_event_parse_toc_select (event, &uid);

      if ((uid != NULL)
          && (sscanf (uid, "nonstream-subsong-%05u", &subsong_idx) == 1)) {
        seqnum = gst_event_get_seqnum (event);

        GST_DEBUG_OBJECT (dec,
            "received TOC select event (sequence number %" G_GUINT32_FORMAT
            "), switching to subsong %u", seqnum, subsong_idx);

        gst_nonstream_audio_decoder_switch_to_subsong (dec, subsong_idx,
            &seqnum);
      }

      g_free (uid);

      res = TRUE;

      break;
    }

    default:
      res = gst_pad_event_default (pad, parent, event);
  }

  return res;
}


static gboolean
gst_nonstream_audio_decoder_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query)
{
  gboolean res = FALSE;
  GstNonstreamAudioDecoder *dec;
  GstNonstreamAudioDecoderClass *klass;

  dec = GST_NONSTREAM_AUDIO_DECODER (parent);
  klass = GST_NONSTREAM_AUDIO_DECODER_GET_CLASS (dec);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_DURATION:
    {
      GstFormat format;
      GST_TRACE_OBJECT (parent, "duration query");

      if (!(dec->loaded_mode)) {
        GST_DEBUG_OBJECT (parent,
            "cannot respond to duration query: nothing is loaded yet");
        break;
      }

      GST_TRACE_OBJECT (parent, "parsing duration query");
      gst_query_parse_duration (query, &format, NULL);

      GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (dec);
      if ((format == GST_FORMAT_TIME)
          && (dec->subsong_duration != GST_CLOCK_TIME_NONE)) {
        GST_DEBUG_OBJECT (parent,
            "responding to query with duration %" GST_TIME_FORMAT,
            GST_TIME_ARGS (dec->subsong_duration));
        gst_query_set_duration (query, format, dec->subsong_duration);
        res = TRUE;
      } else if (format != GST_FORMAT_TIME)
        GST_DEBUG_OBJECT (parent,
            "cannot respond to duration query: format is %s, expected time format",
            gst_format_get_name (format));
      else if (dec->subsong_duration == GST_CLOCK_TIME_NONE)
        GST_DEBUG_OBJECT (parent,
            "cannot respond to duration query: no valid subsong duration available");
      GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (dec);

      break;
    }

    case GST_QUERY_POSITION:
    {
      GstFormat format;
      if (!(dec->loaded_mode)) {
        GST_DEBUG_OBJECT (parent,
            "cannot respond to position query: nothing is loaded yet");
        break;
      }

      if (klass->tell == NULL) {
        GST_DEBUG_OBJECT (parent,
            "cannot respond to position query: subclass does not have tell() function defined");
        break;
      }

      gst_query_parse_position (query, &format, NULL);
      if (format == GST_FORMAT_TIME) {
        GstClockTime pos;

        GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (dec);
        pos = klass->tell (dec);
        GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (dec);

        GST_DEBUG_OBJECT (parent,
            "position query received with format TIME -> reporting position %"
            GST_TIME_FORMAT, GST_TIME_ARGS (pos));
        gst_query_set_position (query, format, pos);
        res = TRUE;
      } else {
        GST_DEBUG_OBJECT (parent,
            "position query received with unsupported format %s -> not reporting anything",
            gst_format_get_name (format));
      }

      break;
    }

    case GST_QUERY_SEEKING:
    {
      GstFormat fmt;
      GstClockTime duration;

      if (!dec->loaded_mode) {
        GST_DEBUG_OBJECT (parent,
            "cannot respond to seeking query: nothing is loaded yet");
        break;
      }

      if (klass->seek == NULL) {
        GST_DEBUG_OBJECT (parent,
            "cannot respond to seeking query: subclass does not have seek() function defined");
        break;
      }

      gst_query_parse_seeking (query, &fmt, NULL, NULL, NULL);

      GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (dec);
      duration = dec->subsong_duration;
      GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (dec);

      if (fmt == GST_FORMAT_TIME) {
        GST_DEBUG_OBJECT (parent,
            "seeking query received with format TIME -> can seek: yes");
        gst_query_set_seeking (query, fmt, TRUE, 0, duration);
        res = TRUE;
      } else {
        GST_DEBUG_OBJECT (parent,
            "seeking query received with unsupported format %s -> can seek: no",
            gst_format_get_name (fmt));
        gst_query_set_seeking (query, fmt, FALSE, 0, -1);
        res = TRUE;
      }

      break;
    }

    default:
      res = gst_pad_query_default (pad, parent, query);
  }

  return res;
}



static void
gst_nonstream_audio_decoder_set_initial_state (GstNonstreamAudioDecoder * dec)
{
  dec->upstream_size = -1;
  dec->loaded_mode = FALSE;

  dec->subsong_duration = GST_CLOCK_TIME_NONE;

  dec->output_format_changed = FALSE;
  gst_audio_info_init (&(dec->output_audio_info));
  dec->num_decoded_samples = 0;
  dec->cur_pos_in_samples = 0;
  gst_segment_init (&(dec->cur_segment), GST_FORMAT_TIME);
  dec->discont = FALSE;

  dec->toc = NULL;

  dec->allocator = NULL;
}


static void
gst_nonstream_audio_decoder_cleanup_state (GstNonstreamAudioDecoder * dec)
{
  gst_adapter_clear (dec->input_data_adapter);

  if (dec->allocator != NULL) {
    gst_object_unref (dec->allocator);
    dec->allocator = NULL;
  }

  if (dec->toc != NULL) {
    gst_toc_unref (dec->toc);
    dec->toc = NULL;
  }

  gst_nonstream_audio_decoder_set_initial_state (dec);
}


static gboolean
gst_nonstream_audio_decoder_negotiate (GstNonstreamAudioDecoder * dec)
{
  /* must be called with lock */

  GstNonstreamAudioDecoderClass *klass;
  gboolean res = TRUE;

  klass = GST_NONSTREAM_AUDIO_DECODER_GET_CLASS (dec);

  /* protected by a mutex, since the allocator might currently be in use */
  if (klass->negotiate != NULL)
    res = klass->negotiate (dec);

  return res;
}


static gboolean
gst_nonstream_audio_decoder_negotiate_default (GstNonstreamAudioDecoder * dec)
{
  /* mutex is locked when this is called */

  GstCaps *caps;
  GstNonstreamAudioDecoderClass *klass;
  gboolean res = TRUE;
  GstQuery *query = NULL;
  GstAllocator *allocator;
  GstAllocationParams allocation_params;

  g_return_val_if_fail (GST_IS_NONSTREAM_AUDIO_DECODER (dec), FALSE);
  g_return_val_if_fail (GST_AUDIO_INFO_IS_VALID (&(dec->output_audio_info)),
      FALSE);

  klass = GST_NONSTREAM_AUDIO_DECODER_CLASS (G_OBJECT_GET_CLASS (dec));

  caps = gst_audio_info_to_caps (&(dec->output_audio_info));

  GST_DEBUG_OBJECT (dec, "setting src caps %" GST_PTR_FORMAT, (gpointer) caps);

  res = gst_pad_push_event (dec->srcpad, gst_event_new_caps (caps));
  /* clear any pending reconfigure flag */
  gst_pad_check_reconfigure (dec->srcpad);

  if (!res) {
    GST_WARNING_OBJECT (dec, "could not push new caps event downstream");
    goto done;
  }

  GST_TRACE_OBJECT (dec, "src caps set");

  dec->output_format_changed = FALSE;

  query = gst_query_new_allocation (caps, TRUE);
  if (!gst_pad_peer_query (dec->srcpad, query)) {
    GST_DEBUG_OBJECT (dec, "didn't get downstream ALLOCATION hints");
  }

  g_assert (klass->decide_allocation != NULL);
  res = klass->decide_allocation (dec, query);

  GST_DEBUG_OBJECT (dec, "ALLOCATION (%d) params: %" GST_PTR_FORMAT, res,
      (gpointer) query);

  if (!res)
    goto no_decide_allocation;

  /* we got configuration from our peer or the decide_allocation method,
   * parse them */
  if (gst_query_get_n_allocation_params (query) > 0) {
    gst_query_parse_nth_allocation_param (query, 0, &allocator,
        &allocation_params);
  } else {
    allocator = NULL;
    gst_allocation_params_init (&allocation_params);
  }

  if (dec->allocator != NULL)
    gst_object_unref (dec->allocator);
  dec->allocator = allocator;
  dec->allocation_params = allocation_params;

done:
  if (query != NULL)
    gst_query_unref (query);
  gst_caps_unref (caps);

  return res;

no_decide_allocation:
  {
    GST_WARNING_OBJECT (dec, "subclass failed to decide allocation");
    goto done;
  }
}


static gboolean
gst_nonstream_audio_decoder_decide_allocation_default (G_GNUC_UNUSED
    GstNonstreamAudioDecoder * dec, GstQuery * query)
{
  GstAllocator *allocator = NULL;
  GstAllocationParams params;
  gboolean update_allocator;

  /* we got configuration from our peer or the decide_allocation method,
   * parse them */
  if (gst_query_get_n_allocation_params (query) > 0) {
    /* try the allocator */
    gst_query_parse_nth_allocation_param (query, 0, &allocator, &params);
    update_allocator = TRUE;
  } else {
    allocator = NULL;
    gst_allocation_params_init (&params);
    update_allocator = FALSE;
  }

  if (update_allocator)
    gst_query_set_nth_allocation_param (query, 0, allocator, &params);
  else
    gst_query_add_allocation_param (query, allocator, &params);

  if (allocator)
    gst_object_unref (allocator);

  return TRUE;
}


static gboolean
gst_nonstream_audio_decoder_propose_allocation_default (G_GNUC_UNUSED
    GstNonstreamAudioDecoder * dec, G_GNUC_UNUSED GstQuery * query)
{
  return TRUE;
}


static gboolean
gst_nonstream_audio_decoder_get_upstream_size (GstNonstreamAudioDecoder * dec,
    gint64 * length)
{
  return gst_pad_peer_query_duration (dec->sinkpad, GST_FORMAT_BYTES, length)
      && (*length >= 0);
}


static gboolean
gst_nonstream_audio_decoder_load_from_buffer (GstNonstreamAudioDecoder * dec,
    GstBuffer * buffer)
{
  gboolean load_ok;
  GstClockTime initial_position;
  GstNonstreamAudioDecoderClass *klass;
  gboolean ret;

  klass = GST_NONSTREAM_AUDIO_DECODER_CLASS (G_OBJECT_GET_CLASS (dec));
  g_assert (klass->load_from_buffer != NULL);

  GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (dec);

  GST_LOG_OBJECT (dec, "read %" G_GSIZE_FORMAT " bytes from upstream",
      gst_buffer_get_size (buffer));

  initial_position = 0;
  load_ok =
      klass->load_from_buffer (dec, buffer, dec->current_subsong,
      dec->subsong_mode, &initial_position, &(dec->output_mode),
      &(dec->num_loops));
  gst_buffer_unref (buffer);

  ret =
      gst_nonstream_audio_decoder_finish_load (dec, load_ok, initial_position,
      FALSE);

  GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (dec);

  return ret;
}


static gboolean
gst_nonstream_audio_decoder_load_from_custom (GstNonstreamAudioDecoder * dec)
{
  gboolean load_ok;
  GstClockTime initial_position;
  GstNonstreamAudioDecoderClass *klass;
  gboolean ret;

  klass = GST_NONSTREAM_AUDIO_DECODER_CLASS (G_OBJECT_GET_CLASS (dec));
  g_assert (klass->load_from_custom != NULL);

  GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (dec);

  GST_LOG_OBJECT (dec,
      "reading song from custom source defined by derived class");

  initial_position = 0;
  load_ok =
      klass->load_from_custom (dec, dec->current_subsong, dec->subsong_mode,
      &initial_position, &(dec->output_mode), &(dec->num_loops));

  ret =
      gst_nonstream_audio_decoder_finish_load (dec, load_ok, initial_position,
      TRUE);

  GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (dec);

  return ret;
}


static gboolean
gst_nonstream_audio_decoder_finish_load (GstNonstreamAudioDecoder * dec,
    gboolean load_ok, GstClockTime initial_position, gboolean send_stream_start)
{
  /* must be called with lock */

  GstNonstreamAudioDecoderClass *klass =
      GST_NONSTREAM_AUDIO_DECODER_CLASS (G_OBJECT_GET_CLASS (dec));

  GST_TRACE_OBJECT (dec, "enter finish_load");


  /* Prerequisites */

  if (!load_ok) {
    GST_ELEMENT_ERROR (dec, STREAM, DECODE, (NULL), ("Loading failed"));
    return FALSE;
  }

  if (!GST_AUDIO_INFO_IS_VALID (&(dec->output_audio_info))) {
    GST_ELEMENT_ERROR (dec, STREAM, DECODE, (NULL),
        ("Audio info is invalid after loading"));
    return FALSE;
  }


  /* Log the number of available subsongs */
  if (klass->get_num_subsongs != NULL)
    GST_DEBUG_OBJECT (dec, "%u subsong(s) available",
        klass->get_num_subsongs (dec));


  /* Set the current subsong (or use the default value) */
  if (klass->get_current_subsong != NULL) {
    GST_TRACE_OBJECT (dec, "requesting current subsong");
    dec->current_subsong = klass->get_current_subsong (dec);
  }


  /* Handle the subsong duration */
  if (klass->get_subsong_duration != NULL) {
    GstClockTime duration;
    GST_TRACE_OBJECT (dec, "requesting subsong duration");
    duration = klass->get_subsong_duration (dec, dec->current_subsong);
    gst_nonstream_audio_decoder_update_subsong_duration (dec, duration);
  }


  /* Send tags downstream (if some exist) */
  if (klass->get_subsong_tags != NULL) {
    /* Subsong tags available */

    GstTagList *tags;
    GST_TRACE_OBJECT (dec, "requesting subsong tags");
    tags = klass->get_subsong_tags (dec, dec->current_subsong);
    if (tags != NULL)
      tags = gst_nonstream_audio_decoder_add_main_tags (dec, tags);
    if (tags != NULL)
      gst_pad_push_event (dec->srcpad, gst_event_new_tag (tags));
  } else {
    /* No subsong tags - just send main tags out */

    GstTagList *tags = gst_tag_list_new_empty ();
    tags = gst_nonstream_audio_decoder_add_main_tags (dec, tags);
    gst_pad_push_event (dec->srcpad, gst_event_new_tag (tags));
  }


  /* Send stream start downstream if requested */
  if (send_stream_start) {
    gchar *stream_id;
    GstEvent *event;

    stream_id =
        gst_pad_create_stream_id (dec->srcpad, GST_ELEMENT_CAST (dec), NULL);
    GST_DEBUG_OBJECT (dec, "pushing STREAM_START with stream id \"%s\"",
        stream_id);

    event = gst_event_new_stream_start (stream_id);
    gst_event_set_group_id (event, gst_util_group_id_next ());
    gst_pad_push_event (dec->srcpad, event);
    g_free (stream_id);
  }


  /* Update the table of contents */
  gst_nonstream_audio_decoder_update_toc (dec, klass);


  /* Negotiate output caps and an allocator */
  GST_TRACE_OBJECT (dec, "negotiating caps and allocator");
  if (!gst_nonstream_audio_decoder_negotiate (dec)) {
    GST_ERROR_OBJECT (dec, "negotiation failed - aborting load");
    return FALSE;
  }


  /* Send new segment downstream */
  gst_nonstream_audio_decoder_output_new_segment (dec, initial_position);

  dec->loaded_mode = TRUE;

  GST_TRACE_OBJECT (dec, "exit finish_load");

  return TRUE;
}


static gboolean
gst_nonstream_audio_decoder_start_task (GstNonstreamAudioDecoder * dec)
{
  if (!gst_pad_start_task (dec->srcpad,
          (GstTaskFunction) gst_nonstream_audio_decoder_output_task, dec,
          NULL)) {
    GST_ERROR_OBJECT (dec, "could not start decoder output task");
    return FALSE;
  } else
    return TRUE;
}


static gboolean
gst_nonstream_audio_decoder_stop_task (GstNonstreamAudioDecoder * dec)
{
  if (!gst_pad_stop_task (dec->srcpad)) {
    GST_ERROR_OBJECT (dec, "could not stop decoder output task");
    return FALSE;
  } else
    return TRUE;
}


static gboolean
gst_nonstream_audio_decoder_switch_to_subsong (GstNonstreamAudioDecoder * dec,
    guint new_subsong, guint32 const *seqnum)
{
  gboolean ret = TRUE;
  GstNonstreamAudioDecoderClass *klass =
      GST_NONSTREAM_AUDIO_DECODER_GET_CLASS (dec);


  if (klass->set_current_subsong == NULL) {
    /* If set_current_subsong wasn't set by the subclass, then
     * subsongs are not supported. It is not an error if this
     * function is called in that case, since it might happen
     * because the current-subsong property was set (and since
     * this is a base class property, it is always available). */
    GST_DEBUG_OBJECT (dec, "cannot call set_current_subsong, since it is NULL");
    goto finish;
  }

  if (dec->loaded_mode) {
    GstEvent *fevent;
    GstClockTime new_position;
    GstClockTime new_subsong_duration = GST_CLOCK_TIME_NONE;


    /* Check if (a) new_subsong is already the current subsong
     * and (b) if new_subsong exceeds the number of available
     * subsongs. Do this here, when the song is loaded,
     * because prior to loading, the number of subsong is usually
     * not known (and the loading process might choose a specific
     * subsong to be the current one at the start of playback). */

    GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (dec);

    if (new_subsong == dec->current_subsong) {
      GST_DEBUG_OBJECT (dec,
          "subsong %u is already the current subsong - ignoring call",
          new_subsong);
      goto finish_unlock;
    }

    if (klass->get_num_subsongs) {
      guint num_subsongs = klass->get_num_subsongs (dec);

      if (new_subsong >= num_subsongs) {
        GST_WARNING_OBJECT (dec,
            "subsong %u is out of bounds (there are %u subsongs) - not switching",
            new_subsong, num_subsongs);
        goto finish_unlock;
      }
    }

    GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (dec);


    /* Switching subsongs during playback is very similar to a
     * flushing seek. Therefore, the stream lock must be taken,
     * flush-start/flush-stop events have to be sent, and
     * the pad task has to be restarted. */


    fevent = gst_event_new_flush_start ();
    if (seqnum != NULL) {
      gst_event_set_seqnum (fevent, *seqnum);
      GST_DEBUG_OBJECT (dec,
          "sending flush start event with sequence number %" G_GUINT32_FORMAT,
          *seqnum);
    } else
      GST_DEBUG_OBJECT (dec, "sending flush start event (no sequence number)");

    gst_pad_push_event (dec->srcpad, gst_event_ref (fevent));
    /* unlock upstream pull_range */
    if (klass->loads_from_sinkpad)
      gst_pad_push_event (dec->sinkpad, fevent);
    else
      gst_event_unref (fevent);


    GST_PAD_STREAM_LOCK (dec->srcpad);


    GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (dec);


    if (!(klass->set_current_subsong (dec, new_subsong, &new_position))) {
      /* Switch failed. Do _not_ exit early from here - playback must
       * continue from the current subsong, and it cannot do that if
       * we exit here. Try getting the current position and proceed as
       * if the switch succeeded (but set the return value to FALSE.) */

      ret = FALSE;
      if (klass->tell)
        new_position = klass->tell (dec);
      else
        new_position = 0;
      GST_WARNING_OBJECT (dec, "switching to new subsong %u failed",
          new_subsong);
    }

    /* Flushing seek resets the base time, which means num_decoded_samples
     * needs to be set to 0, since it defines the segment.base value */
    dec->num_decoded_samples = 0;


    fevent = gst_event_new_flush_stop (TRUE);
    if (seqnum != NULL) {
      gst_event_set_seqnum (fevent, *seqnum);
      GST_DEBUG_OBJECT (dec,
          "sending flush stop event with sequence number %" G_GUINT32_FORMAT,
          *seqnum);
    } else
      GST_DEBUG_OBJECT (dec, "sending flush stop event (no sequence number)");

    gst_pad_push_event (dec->srcpad, gst_event_ref (fevent));
    /* unlock upstream pull_range */
    if (klass->loads_from_sinkpad)
      gst_pad_push_event (dec->sinkpad, fevent);
    else
      gst_event_unref (fevent);


    /* use the new subsong's duration (if one exists) */
    if (klass->get_subsong_duration != NULL)
      new_subsong_duration = klass->get_subsong_duration (dec, new_subsong);
    gst_nonstream_audio_decoder_update_subsong_duration (dec,
        new_subsong_duration);

    /* create a new segment for the new subsong */
    gst_nonstream_audio_decoder_output_new_segment (dec, new_position);

    /* use the new subsong's tags (if any exist) */
    if (klass->get_subsong_tags != NULL) {
      GstTagList *subsong_tags = klass->get_subsong_tags (dec, new_subsong);
      if (subsong_tags != NULL)
        subsong_tags =
            gst_nonstream_audio_decoder_add_main_tags (dec, subsong_tags);
      if (subsong_tags != NULL)
        gst_pad_push_event (dec->srcpad, gst_event_new_tag (subsong_tags));
    }

    GST_DEBUG_OBJECT (dec, "successfully switched to new subsong %u",
        new_subsong);
    dec->current_subsong = new_subsong;


    GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (dec);


    /* Subsong has been switched, and all necessary events have been
     * pushed downstream. Restart srcpad task. */
    gst_nonstream_audio_decoder_start_task (dec);

    /* Unlock stream, we are done */
    GST_PAD_STREAM_UNLOCK (dec->srcpad);
  } else {
    /* If song hasn't been loaded yet, then playback cannot currently
     * been happening. In this case, a "switch" is simple - just store
     * the current subsong index. When the song is loaded, it will
     * start playing this subsong. */

    GST_DEBUG_OBJECT (dec,
        "playback hasn't started yet - storing subsong index %u as the current subsong",
        new_subsong);

    GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (dec);
    dec->current_subsong = new_subsong;
    GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (dec);
  }


finish:
  return ret;


finish_unlock:
  GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (dec);
  goto finish;
}


static void
gst_nonstream_audio_decoder_update_toc (GstNonstreamAudioDecoder * dec,
    GstNonstreamAudioDecoderClass * klass)
{
  /* must be called with lock */

  guint num_subsongs, i;

  if (dec->toc != NULL) {
    gst_toc_unref (dec->toc);
    dec->toc = NULL;
  }

  if (klass->get_num_subsongs == NULL)
    return;

  num_subsongs = klass->get_num_subsongs (dec);
  if (num_subsongs <= 1) {
    GST_DEBUG_OBJECT (dec, "no need for a TOC since there is only one subsong");
    return;
  }

  dec->toc = gst_toc_new (GST_TOC_SCOPE_GLOBAL);

  if (klass->get_main_tags) {
    GstTagList *main_tags = klass->get_main_tags (dec);
    if (main_tags)
      gst_toc_set_tags (dec->toc, main_tags);
  }

  for (i = 0; i < num_subsongs; ++i) {
    gchar *uid;
    GstTocEntry *entry;
    GstClockTime duration;
    GstTagList *tags;

    duration =
        (klass->get_subsong_duration !=
        NULL) ? klass->get_subsong_duration (dec, i) : GST_CLOCK_TIME_NONE;
    tags =
        (klass->get_subsong_tags != NULL) ? klass->get_subsong_tags (dec,
        i) : NULL;
    if (!tags)
      tags = gst_tag_list_new_empty ();

    uid = g_strdup_printf ("nonstream-subsong-%05u", i);
    entry = gst_toc_entry_new (GST_TOC_ENTRY_TYPE_TRACK, uid);
    /* Set the UID as title tag for TOC entry if no title already present */
    gst_tag_list_add (tags, GST_TAG_MERGE_KEEP, GST_TAG_TITLE, uid, NULL);
    /* Set the subsong duration as duration tag for TOC entry if no duration already present */
    if (duration != GST_CLOCK_TIME_NONE)
      gst_tag_list_add (tags, GST_TAG_MERGE_KEEP, GST_TAG_DURATION, duration,
          NULL);

    /* FIXME: TOC does not allow GST_CLOCK_TIME_NONE as a stop value */
    if (duration == GST_CLOCK_TIME_NONE)
      duration = G_MAXINT64;

    /* Subsongs always start at 00:00 */
    gst_toc_entry_set_start_stop_times (entry, 0, duration);
    gst_toc_entry_set_tags (entry, tags);

    /* NOTE: *not* adding loop count via gst_toc_entry_set_loop(), since
     * in GstNonstreamAudioDecoder, looping is a playback property, not
     * a property of the subsongs themselves */

    GST_DEBUG_OBJECT (dec,
        "new toc entry: uid: \"%s\" duration: %" GST_TIME_FORMAT " tags: %"
        GST_PTR_FORMAT, uid, GST_TIME_ARGS (duration), (gpointer) tags);

    gst_toc_append_entry (dec->toc, entry);

    g_free (uid);
  }

  gst_pad_push_event (dec->srcpad, gst_event_new_toc (dec->toc, FALSE));
}


static void
gst_nonstream_audio_decoder_update_subsong_duration (GstNonstreamAudioDecoder *
    dec, GstClockTime duration)
{
  /* must be called with lock */

  dec->subsong_duration = duration;
  GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (dec);
  gst_element_post_message (GST_ELEMENT (dec),
      gst_message_new_duration_changed (GST_OBJECT (dec)));
  GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (dec);
}


static void
gst_nonstream_audio_decoder_output_new_segment (GstNonstreamAudioDecoder * dec,
    GstClockTime start_position)
{
  /* must be called with lock */

  GstSegment segment;

  gst_segment_init (&segment, GST_FORMAT_TIME);

  segment.base =
      gst_util_uint64_scale_int (dec->num_decoded_samples, GST_SECOND,
      dec->output_audio_info.rate);
  segment.start = 0;
  segment.time = start_position;
  segment.offset = 0;
  segment.position = 0;

  /* note that num_decoded_samples isn't being reset; it is the
   * analogue to the segment base value, and thus is supposed to
   * monotonically increase, except for when a flushing seek happens
   * (since a flushing seek is supposed to be a fresh restart for
   * the whole pipeline) */
  dec->cur_pos_in_samples = 0;

  /* stop/duration members are not set, on purpose - in case of loops,
   * new segments will be generated, which automatically put an implicit
   * end on the current segment (the segment implicitly "ends" when the
   * new one starts), and having a stop value might cause very slight
   * gaps occasionally due to slight jitter in the calculation of
   * base times etc. */

  GST_DEBUG_OBJECT (dec,
      "output new segment with base %" GST_TIME_FORMAT " time %"
      GST_TIME_FORMAT, GST_TIME_ARGS (segment.base),
      GST_TIME_ARGS (segment.time));

  dec->cur_segment = segment;
  dec->discont = TRUE;

  gst_pad_push_event (dec->srcpad, gst_event_new_segment (&segment));
}


static gboolean
gst_nonstream_audio_decoder_do_seek (GstNonstreamAudioDecoder * dec,
    GstEvent * event)
{
  gboolean res;
  gdouble rate;
  GstFormat format;
  GstSeekFlags flags;
  GstSeekType start_type, stop_type;
  GstClockTime new_position;
  gint64 start, stop;
  GstSegment segment;
  guint32 seqnum;
  gboolean flush;
  GstNonstreamAudioDecoderClass *klass =
      GST_NONSTREAM_AUDIO_DECODER_GET_CLASS (dec);

  if (klass->seek == NULL) {
    GST_DEBUG_OBJECT (dec,
        "cannot seek: subclass does not have seek() function defined");
    return FALSE;
  }

  if (!dec->loaded_mode) {
    GST_DEBUG_OBJECT (dec, "nothing loaded yet - cannot seek");
    return FALSE;
  }

  GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (dec);
  if (!GST_AUDIO_INFO_IS_VALID (&(dec->output_audio_info))) {
    GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (dec);
    GST_DEBUG_OBJECT (dec, "no valid output audioinfo present - cannot seek");
    return FALSE;
  }
  GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (dec);


  GST_DEBUG_OBJECT (dec, "starting seek");

  gst_event_parse_seek (event, &rate, &format, &flags, &start_type, &start,
      &stop_type, &stop);
  seqnum = gst_event_get_seqnum (event);

  GST_DEBUG_OBJECT (dec,
      "seek event data:  "
      "rate %f  format %s  "
      "start type %s  start %" GST_TIME_FORMAT "  "
      "stop type %s  stop %" GST_TIME_FORMAT,
      rate, gst_format_get_name (format),
      get_seek_type_name (start_type), GST_TIME_ARGS (start),
      get_seek_type_name (stop_type), GST_TIME_ARGS (stop)
      );

  if (format != GST_FORMAT_TIME) {
    GST_DEBUG_OBJECT (dec, "seeking is only supported in TIME format");
    return FALSE;
  }

  if (rate < 0) {
    GST_DEBUG_OBJECT (dec, "only positive seek rates are supported");
    return FALSE;
  }

  flush = ((flags & GST_SEEK_FLAG_FLUSH) == GST_SEEK_FLAG_FLUSH);

  if (flush) {
    GstEvent *fevent = gst_event_new_flush_start ();
    gst_event_set_seqnum (fevent, seqnum);

    GST_DEBUG_OBJECT (dec,
        "sending flush start event with sequence number %" G_GUINT32_FORMAT,
        seqnum);

    gst_pad_push_event (dec->srcpad, gst_event_ref (fevent));
    /* unlock upstream pull_range */
    if (klass->loads_from_sinkpad)
      gst_pad_push_event (dec->sinkpad, fevent);
    else
      gst_event_unref (fevent);
  } else
    gst_pad_pause_task (dec->srcpad);

  GST_PAD_STREAM_LOCK (dec->srcpad);

  segment = dec->cur_segment;

  if (!gst_segment_do_seek (&segment,
          rate, format, flags, start_type, start, stop_type, stop, NULL)) {
    GST_DEBUG_OBJECT (dec, "could not seek in segment");
    GST_PAD_STREAM_UNLOCK (dec->srcpad);
    return FALSE;
  }

  GST_DEBUG_OBJECT (dec,
      "segment data: "
      "seek event data:  "
      "rate %f  applied rate %f  "
      "format %s  "
      "base %" GST_TIME_FORMAT "  "
      "offset %" GST_TIME_FORMAT "  "
      "start %" GST_TIME_FORMAT "  "
      "stop %" GST_TIME_FORMAT "  "
      "time %" GST_TIME_FORMAT "  "
      "position %" GST_TIME_FORMAT "  "
      "duration %" GST_TIME_FORMAT,
      segment.rate, segment.applied_rate,
      gst_format_get_name (segment.format),
      GST_TIME_ARGS (segment.base),
      GST_TIME_ARGS (segment.offset),
      GST_TIME_ARGS (segment.start),
      GST_TIME_ARGS (segment.stop),
      GST_TIME_ARGS (segment.time),
      GST_TIME_ARGS (segment.position), GST_TIME_ARGS (segment.duration)
      );

  GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (dec);

  new_position = segment.position;
  res = klass->seek (dec, &new_position);
  segment.position = new_position;

  dec->cur_segment = segment;
  dec->cur_pos_in_samples =
      gst_util_uint64_scale_int (dec->cur_segment.position,
      dec->output_audio_info.rate, GST_SECOND);
  dec->num_decoded_samples = 0;

  GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (dec);

  if (flush) {
    GstEvent *fevent = gst_event_new_flush_stop (TRUE);
    gst_event_set_seqnum (fevent, seqnum);

    GST_DEBUG_OBJECT (dec,
        "sending flush stop event with sequence number %" G_GUINT32_FORMAT,
        seqnum);

    gst_pad_push_event (dec->srcpad, gst_event_ref (fevent));
    if (klass->loads_from_sinkpad)
      gst_pad_push_event (dec->sinkpad, fevent);
    else
      gst_event_unref (fevent);
  }

  if (res) {
    if (flags & GST_SEEK_FLAG_SEGMENT) {
      GST_DEBUG_OBJECT (dec, "posting SEGMENT_START message");

      gst_element_post_message (GST_ELEMENT (dec),
          gst_message_new_segment_start (GST_OBJECT (dec),
              GST_FORMAT_TIME, segment.start)
          );
    }

    gst_pad_push_event (dec->srcpad, gst_event_new_segment (&segment));

    GST_INFO_OBJECT (dec, "seek succeeded");

    gst_nonstream_audio_decoder_start_task (dec);
  } else {
    GST_WARNING_OBJECT (dec, "seek failed");
  }

  GST_PAD_STREAM_UNLOCK (dec->srcpad);

  gst_event_unref (event);

  return res;
}


static GstTagList *
gst_nonstream_audio_decoder_add_main_tags (GstNonstreamAudioDecoder * dec,
    GstTagList * tags)
{
  GstNonstreamAudioDecoderClass *klass =
      GST_NONSTREAM_AUDIO_DECODER_GET_CLASS (dec);

  if (!klass->get_main_tags)
    return tags;

  tags = gst_tag_list_make_writable (tags);
  if (tags) {
    GstClockTime duration;
    GstTagList *main_tags;

    /* Get main tags. If some exist, merge them with the given tags,
     * and return the merged result. Otherwise, just return the given tags. */
    main_tags = klass->get_main_tags (dec);
    if (main_tags) {
      tags = gst_tag_list_merge (main_tags, tags, GST_TAG_MERGE_REPLACE);
      gst_tag_list_unref (main_tags);
    }

    /* Add subsong duration if available */
    duration = dec->subsong_duration;
    if (GST_CLOCK_TIME_IS_VALID (duration))
      gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE, GST_TAG_DURATION, duration,
          NULL);

    return tags;
  } else {
    GST_ERROR_OBJECT (dec, "could not make subsong tags writable");
    return NULL;
  }
}


static void
gst_nonstream_audio_decoder_output_task (GstNonstreamAudioDecoder * dec)
{
  GstFlowReturn flow;
  GstBuffer *outbuf;
  guint num_samples;

  GstNonstreamAudioDecoderClass *klass;
  klass = GST_NONSTREAM_AUDIO_DECODER_CLASS (G_OBJECT_GET_CLASS (dec));
  g_assert (klass->decode != NULL);

  GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (dec);

  /* perform the actual decoding */
  if (!(klass->decode (dec, &outbuf, &num_samples))) {
    /* EOS case */
    GST_INFO_OBJECT (dec, "decode() reports end -> sending EOS event");
    gst_pad_push_event (dec->srcpad, gst_event_new_eos ());
    goto pause_unlock;
  }

  if (outbuf == NULL) {
    GST_ERROR_OBJECT (outbuf, "decode() produced NULL buffer");
    goto pause_unlock;
  }

  /* set the buffer's metadata */
  GST_BUFFER_DURATION (outbuf) =
      gst_util_uint64_scale_int (num_samples, GST_SECOND,
      dec->output_audio_info.rate);
  GST_BUFFER_OFFSET (outbuf) = dec->cur_pos_in_samples;
  GST_BUFFER_OFFSET_END (outbuf) = dec->cur_pos_in_samples + num_samples;
  GST_BUFFER_PTS (outbuf) =
      gst_util_uint64_scale_int (dec->cur_pos_in_samples, GST_SECOND,
      dec->output_audio_info.rate);
  GST_BUFFER_DTS (outbuf) = GST_BUFFER_PTS (outbuf);

  if (G_UNLIKELY (dec->discont)) {
    GST_BUFFER_FLAG_SET (outbuf, GST_BUFFER_FLAG_DISCONT);
    dec->discont = FALSE;
  }

  GST_LOG_OBJECT (dec,
      "output buffer stats: num_samples = %u  duration = %" GST_TIME_FORMAT
      "  cur_pos_in_samples = %" G_GUINT64_FORMAT "  timestamp = %"
      GST_TIME_FORMAT, num_samples,
      GST_TIME_ARGS (GST_BUFFER_DURATION (outbuf)), dec->cur_pos_in_samples,
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (outbuf))
      );

  /* increment sample counters */
  dec->cur_pos_in_samples += num_samples;
  dec->num_decoded_samples += num_samples;

  /* the decode() call might have set a new output format -> renegotiate
   * before sending the new buffer downstream */
  if (G_UNLIKELY (dec->output_format_changed ||
          (GST_AUDIO_INFO_IS_VALID (&(dec->output_audio_info))
              && gst_pad_check_reconfigure (dec->srcpad))
      )) {
    if (!gst_nonstream_audio_decoder_negotiate (dec)) {
      gst_buffer_unref (outbuf);
      GST_LOG_OBJECT (dec, "could not push output buffer: negotiation failed");
      goto pause_unlock;
    }
  }

  GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (dec);

  /* push new samples downstream
   * no need to unref buffer - gst_pad_push() does it in
   * all cases (success and failure) */
  flow = gst_pad_push (dec->srcpad, outbuf);
  switch (flow) {
    case GST_FLOW_OK:
      break;

    case GST_FLOW_FLUSHING:
      GST_LOG_OBJECT (dec, "pipeline is being flushed - pausing task");
      goto pause;

    case GST_FLOW_NOT_NEGOTIATED:
      if (gst_pad_needs_reconfigure (dec->srcpad)) {
        GST_DEBUG_OBJECT (dec, "trying to renegotiate");
        break;
      }
      /* fallthrough to default */

    default:
      GST_ELEMENT_ERROR (dec, STREAM, FAILED, ("Internal data flow error."),
          ("streaming task paused, reason %s (%d)", gst_flow_get_name (flow),
              flow));
  }

  return;

pause:
  GST_INFO_OBJECT (dec, "pausing task");
  /* NOT using stop_task here, since that would cause a deadlock.
   * See the gst_pad_stop_task() documentation for details. */
  gst_pad_pause_task (dec->srcpad);
  return;
pause_unlock:
  GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (dec);
  goto pause;
}


static char const *
get_seek_type_name (GstSeekType seek_type)
{
  switch (seek_type) {
    case GST_SEEK_TYPE_NONE:
      return "none";
    case GST_SEEK_TYPE_SET:
      return "set";
    case GST_SEEK_TYPE_END:
      return "end";
    default:
      return "<unknown>";
  }
}




/**
 * gst_nonstream_audio_decoder_handle_loop:
 * @dec: a #GstNonstreamAudioDecoder
 * @new_position New position the next loop starts with
 *
 * Reports that a loop has been completed and creates a new appropriate
 * segment for the next loop.
 *
 * @new_position exists because a loop may not start at the beginning.
 *
 * This function is only useful for subclasses which can be in the
 * GST_NONSTREAM_AUDIO_OUTPUT_MODE_LOOPING output mode, since in the
 * GST_NONSTREAM_AUDIO_OUTPUT_MODE_STEADY output mode, this function
 * does nothing. See #GstNonstreamAudioOutputMode for more details.
 *
 * The subclass calls this during playback when it loops. It produces
 * a new segment with updated base time and internal time values, to allow
 * for seamless looping. It does *not* check the number of elapsed loops;
 * this is up the subclass.
 *
 * Note that if this function is called, then it must be done after the
 * last samples of the loop have been decoded and pushed downstream.
 *
 * This function must be called with the decoder mutex lock held, since it
 * is typically called from within @decode (which in turn are called with
 * the lock already held).
 */
void
gst_nonstream_audio_decoder_handle_loop (GstNonstreamAudioDecoder * dec,
    GstClockTime new_position)
{
  if (dec->output_mode == GST_NONSTREAM_AUDIO_OUTPUT_MODE_STEADY) {
    /* handle_loop makes no sense with open-ended decoders */
    GST_WARNING_OBJECT (dec,
        "ignoring handle_loop() call, since the decoder output mode is \"steady\"");
    return;
  }

  GST_DEBUG_OBJECT (dec,
      "handle_loop() invoked with new_position = %" GST_TIME_FORMAT,
      GST_TIME_ARGS (new_position));

  dec->discont = TRUE;

  gst_nonstream_audio_decoder_output_new_segment (dec, new_position);
}


/**
 * gst_nonstream_audio_decoder_set_output_format:
 * @dec: a #GstNonstreamAudioDecoder
 * @audio_info: Valid audio info structure containing the output format
 *
 * Sets the output caps by means of a GstAudioInfo structure.
 *
 * This must be called latest in the first @decode call, to ensure src caps are
 * set before decoded samples are sent downstream. Typically, this is called
 * from inside @load_from_buffer or @load_from_custom.
 *
 * This function must be called with the decoder mutex lock held, since it
 * is typically called from within the aforementioned vfuncs (which in turn
 * are called with the lock already held).
 *
 * Returns: TRUE if setting the output format succeeded, FALSE otherwise
 */
gboolean
gst_nonstream_audio_decoder_set_output_format (GstNonstreamAudioDecoder * dec,
    GstAudioInfo const *audio_info)
{
  GstCaps *caps;
  GstCaps *templ_caps;
  gboolean caps_ok;
  gboolean res = TRUE;

  g_return_val_if_fail (GST_IS_NONSTREAM_AUDIO_DECODER (dec), FALSE);

  caps = gst_audio_info_to_caps (audio_info);
  if (caps == NULL) {
    GST_WARNING_OBJECT (dec, "Could not create caps out of audio info");
    return FALSE;
  }

  templ_caps = gst_pad_get_pad_template_caps (dec->srcpad);
  caps_ok = gst_caps_is_subset (caps, templ_caps);

  if (caps_ok) {
    dec->output_audio_info = *audio_info;
    dec->output_format_changed = TRUE;

    GST_INFO_OBJECT (dec, "setting output format to %" GST_PTR_FORMAT,
        (gpointer) caps);
  } else {
    GST_WARNING_OBJECT (dec,
        "requested output format %" GST_PTR_FORMAT " does not match template %"
        GST_PTR_FORMAT, (gpointer) caps, (gpointer) templ_caps);

    res = FALSE;
  }

  gst_caps_unref (caps);
  gst_caps_unref (templ_caps);

  return res;
}


/**
 * gst_nonstream_audio_decoder_set_output_format_simple:
 * @dec: a #GstNonstreamAudioDecoder
 * @sample_rate: Output sample rate to use, in Hz
 * @sample_format: Output sample format to use
 * @num_channels: Number of output channels to use
 *
 * Convenience function; sets the output caps by means of common parameters.
 *
 * Internally, this fills a GstAudioInfo structure and calls
 * gst_nonstream_audio_decoder_set_output_format().
 *
 * Returns: TRUE if setting the output format succeeded, FALSE otherwise
 */
gboolean
gst_nonstream_audio_decoder_set_output_format_simple (GstNonstreamAudioDecoder *
    dec, guint sample_rate, GstAudioFormat sample_format, guint num_channels)
{
  GstAudioInfo output_audio_info;

  gst_audio_info_init (&output_audio_info);

  gst_audio_info_set_format (&output_audio_info,
      sample_format, sample_rate, num_channels, NULL);

  return gst_nonstream_audio_decoder_set_output_format (dec,
      &output_audio_info);
}


/**
 * gst_nonstream_audio_decoder_get_downstream_info:
 * @dec: a #GstNonstreamAudioDecoder
 * @format: #GstAudioFormat value to fill with a sample format
 * @sample_rate: Integer to fill with a sample rate
 * @num_channels: Integer to fill with a channel count
 *
 * Gets sample format, sample rate, channel count from the allowed srcpad caps.
 *
 * This is useful for when the subclass wishes to adjust one or more output
 * parameters to whatever downstream is supporting. For example, the output
 * sample rate is often a freely adjustable value in module players.
 *
 * This function tries to find a value inside the srcpad peer's caps for
 * @format, @sample_rate, @num_chnanels . Any of these can be NULL; they
 * (and the corresponding downstream caps) are then skipped while retrieving
 * information. Non-fixated caps are fixated first; the value closest to
 * their present value is then chosen. For example, if the variables pointed
 * to by the arguments are GST_AUDIO_FORMAT_16, 48000 Hz, and 2 channels,
 * and the downstream caps are:
 *
 * "audio/x-raw, format={S16LE,S32LE}, rate=[1,32000], channels=[1,MAX]"
 *
 * Then @format and @channels stay the same, while @sample_rate is set to 32000 Hz.
 * This way, the initial values the the variables pointed to by the arguments
 * are set to can be used as default output values. Note that if no downstream
 * caps can be retrieved, then this function does nothing, therefore it is
 * necessary to ensure that @format, @sample_rate, and @channels have valid
 * initial values.
 *
 * Decoder lock is not held by this function, so it can be called from within
 * any of the class vfuncs.
 */
void
gst_nonstream_audio_decoder_get_downstream_info (GstNonstreamAudioDecoder * dec,
    GstAudioFormat * format, gint * sample_rate, gint * num_channels)
{
  GstCaps *allowed_srccaps;
  guint structure_nr, num_structures;
  gboolean ds_format_found = FALSE, ds_rate_found = FALSE, ds_channels_found =
      FALSE;

  g_return_if_fail (GST_IS_NONSTREAM_AUDIO_DECODER (dec));

  allowed_srccaps = gst_pad_get_allowed_caps (dec->srcpad);
  if (allowed_srccaps == NULL) {
    GST_INFO_OBJECT (dec,
        "no downstream caps available - not modifying arguments");
    return;
  }

  num_structures = gst_caps_get_size (allowed_srccaps);
  GST_DEBUG_OBJECT (dec, "%u structure(s) in downstream caps", num_structures);
  for (structure_nr = 0; structure_nr < num_structures; ++structure_nr) {
    GstStructure *structure;

    ds_format_found = FALSE;
    ds_rate_found = FALSE;
    ds_channels_found = FALSE;

    structure = gst_caps_get_structure (allowed_srccaps, structure_nr);

    /* If all formats which need to be queried are present in the structure,
     * check its contents */
    if (((format == NULL) || gst_structure_has_field (structure, "format")) &&
        ((sample_rate == NULL) || gst_structure_has_field (structure, "rate"))
        && ((num_channels == NULL)
            || gst_structure_has_field (structure, "channels"))) {
      gint fixated_sample_rate;
      gint fixated_num_channels;
      GstAudioFormat fixated_format = 0;
      GstStructure *fixated_str;
      gboolean passed = TRUE;

      /* Make a copy of the structure, since we need to modify
       * (fixate) values inside */
      fixated_str = gst_structure_copy (structure);

      /* Try to fixate and retrieve the sample format */
      if (passed && (format != NULL)) {
        passed = FALSE;

        if ((gst_structure_get_field_type (fixated_str,
                    "format") == G_TYPE_STRING)
            || gst_structure_fixate_field_string (fixated_str, "format",
                gst_audio_format_to_string (*format))) {
          gchar const *fmt_str =
              gst_structure_get_string (fixated_str, "format");
          if (fmt_str
              && ((fixated_format =
                      gst_audio_format_from_string (fmt_str)) !=
                  GST_AUDIO_FORMAT_UNKNOWN)) {
            GST_DEBUG_OBJECT (dec, "found fixated format: %s", fmt_str);
            ds_format_found = TRUE;
            passed = TRUE;
          }
        }
      }

      /* Try to fixate and retrieve the sample rate */
      if (passed && (sample_rate != NULL)) {
        passed = FALSE;

        if ((gst_structure_get_field_type (fixated_str, "rate") == G_TYPE_INT)
            || gst_structure_fixate_field_nearest_int (fixated_str, "rate",
                *sample_rate)) {
          if (gst_structure_get_int (fixated_str, "rate", &fixated_sample_rate)) {
            GST_DEBUG_OBJECT (dec, "found fixated sample rate: %d",
                fixated_sample_rate);
            ds_rate_found = TRUE;
            passed = TRUE;
          }
        }
      }

      /* Try to fixate and retrieve the channel count */
      if (passed && (num_channels != NULL)) {
        passed = FALSE;

        if ((gst_structure_get_field_type (fixated_str,
                    "channels") == G_TYPE_INT)
            || gst_structure_fixate_field_nearest_int (fixated_str, "channels",
                *num_channels)) {
          if (gst_structure_get_int (fixated_str, "channels",
                  &fixated_num_channels)) {
            GST_DEBUG_OBJECT (dec, "found fixated channel count: %d",
                fixated_num_channels);
            ds_channels_found = TRUE;
            passed = TRUE;
          }
        }
      }

      gst_structure_free (fixated_str);

      if (ds_format_found && ds_rate_found && ds_channels_found) {
        *format = fixated_format;
        *sample_rate = fixated_sample_rate;
        *num_channels = fixated_num_channels;
        break;
      }
    }
  }

  gst_caps_unref (allowed_srccaps);

  if ((format != NULL) && !ds_format_found)
    GST_INFO_OBJECT (dec,
        "downstream did not specify format - using default (%s)",
        gst_audio_format_to_string (*format));
  if ((sample_rate != NULL) && !ds_rate_found)
    GST_INFO_OBJECT (dec,
        "downstream did not specify sample rate - using default (%d Hz)",
        *sample_rate);
  if ((num_channels != NULL) && !ds_channels_found)
    GST_INFO_OBJECT (dec,
        "downstream did not specify number of channels - using default (%d channels)",
        *num_channels);
}


/**
 * gst_nonstream_audio_decoder_allocate_output_buffer:
 * @dec: Decoder instance
 * @size: Size of the output buffer, in bytes
 *
 * Allocates an output buffer with the internally configured buffer pool.
 *
 * This function may only be called from within @load_from_buffer,
 * @load_from_custom, and @decode.
 *
 * Returns: (transfer full) (nullable): Newly allocated output buffer, or NULL
 * if allocation failed
 */
GstBuffer *
gst_nonstream_audio_decoder_allocate_output_buffer (GstNonstreamAudioDecoder *
    dec, gsize size)
{
  if (G_UNLIKELY (dec->output_format_changed ||
          (GST_AUDIO_INFO_IS_VALID (&(dec->output_audio_info))
              && gst_pad_check_reconfigure (dec->srcpad))
      )) {
    /* renegotiate if necessary, before allocating,
     * to make sure the right allocator and the right allocation
     * params are used */
    if (!gst_nonstream_audio_decoder_negotiate (dec)) {
      GST_ERROR_OBJECT (dec,
          "could not allocate output buffer because negotiation failed");
      return NULL;
    }
  }

  return gst_buffer_new_allocate (dec->allocator, size,
      &(dec->allocation_params));
}
