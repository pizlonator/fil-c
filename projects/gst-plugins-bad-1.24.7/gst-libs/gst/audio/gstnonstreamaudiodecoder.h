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


#ifndef __GST_NONSTREAM_AUDIO_DECODER_H__
#define __GST_NONSTREAM_AUDIO_DECODER_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <gst/audio/audio.h>
#include <gst/audio/audio-bad-prelude.h>

G_BEGIN_DECLS


typedef struct _GstNonstreamAudioDecoder GstNonstreamAudioDecoder;
typedef struct _GstNonstreamAudioDecoderClass GstNonstreamAudioDecoderClass;


/**
 * GstNonstreamAudioOutputMode:
 * @GST_NONSTREAM_AUDIO_OUTPUT_MODE_LOOPING: Playback position is moved back to the beginning of the loop
 * @GST_NONSTREAM_AUDIO_OUTPUT_MODE_STEADY: Playback position increases steadily, even when looping
 *
 * The output mode defines how the output behaves with regards to looping. Either the playback position is
 * moved back to the beginning of the loop, acting like a backwards seek, or it increases steadily, as if
 * loop were "unrolled".
 */
typedef enum
{
  GST_NONSTREAM_AUDIO_OUTPUT_MODE_LOOPING,
  GST_NONSTREAM_AUDIO_OUTPUT_MODE_STEADY
} GstNonstreamAudioOutputMode;


/**
 * GstNonstreamAudioSubsongMode:
 * @GST_NONSTREAM_AUDIO_SUBSONG_MODE_SINGLE: Only the current subsong is played
 * @GST_NONSTREAM_AUDIO_SUBSONG_MODE_ALL: All subsongs are played (current subsong index is ignored)
 * @GST_NONSTREAM_AUDIO_SUBSONG_MODE_DECODER_DEFAULT: Use decoder specific default behavior
 *
 * The subsong mode defines how the decoder shall handle subsongs.
 */
typedef enum
{
  GST_NONSTREAM_AUDIO_SUBSONG_MODE_SINGLE,
  GST_NONSTREAM_AUDIO_SUBSONG_MODE_ALL,
  GST_NONSTREAM_AUDIO_SUBSONG_MODE_DECODER_DEFAULT
} GstNonstreamAudioSubsongMode;


#define GST_TYPE_NONSTREAM_AUDIO_DECODER             (gst_nonstream_audio_decoder_get_type())
#define GST_NONSTREAM_AUDIO_DECODER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_NONSTREAM_AUDIO_DECODER, GstNonstreamAudioDecoder))
#define GST_NONSTREAM_AUDIO_DECODER_CAST(obj)        ((GstNonstreamAudioDecoder *)(obj))
#define GST_NONSTREAM_AUDIO_DECODER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_NONSTREAM_AUDIO_DECODER, GstNonstreamAudioDecoderClass))
#define GST_NONSTREAM_AUDIO_DECODER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_NONSTREAM_AUDIO_DECODER, GstNonstreamAudioDecoderClass))
#define GST_IS_NONSTREAM_AUDIO_DECODER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_NONSTREAM_AUDIO_DECODER))
#define GST_IS_NONSTREAM_AUDIO_DECODER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_NONSTREAM_AUDIO_DECODER))

/**
 * GST_NONSTREAM_AUDIO_DECODER_SINK_NAME:
 *
 * The name of the template for the sink pad.
 */
#define GST_NONSTREAM_AUDIO_DECODER_SINK_NAME    "sink"
/**
 * GST_NONSTREAM_AUDIO_DECODER_SRC_NAME:
 *
 * The name of the template for the source pad.
 */
#define GST_NONSTREAM_AUDIO_DECODER_SRC_NAME     "src"

/**
 * GST_NONSTREAM_AUDIO_DECODER_SINK_PAD:
 * @obj: base nonstream audio codec instance
 *
 * Gives the pointer to the sink #GstPad object of the element.
 */
#define GST_NONSTREAM_AUDIO_DECODER_SINK_PAD(obj)        (((GstNonstreamAudioDecoder *) (obj))->sinkpad)
/**
 * GST_NONSTREAM_AUDIO_DECODER_SRC_PAD:
 * @obj: base nonstream audio codec instance
 *
 * Gives the pointer to the source #GstPad object of the element.
 */
#define GST_NONSTREAM_AUDIO_DECODER_SRC_PAD(obj)         (((GstNonstreamAudioDecoder *) (obj))->srcpad)


/**
 * GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX:
 * @obj: base nonstream audio codec instance
 *
 * Locks the decoder mutex.
 *
 * Internally, the mutex is locked before one of the class vfuncs are
 * called, when position and duration queries are handled, and when
 * properties are set/retrieved.
 *
 * Derived classes should call lock during decoder related modifications
 * (for example, setting/clearing filter banks), when at the same time
 * audio might get decoded. An example are configuration changes that
 * happen when properties are set. Properties might be set from another
 * thread, so while the derived decoder is reconfigured, the mutex
 * should be locked.
 */
#define GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX(obj)      g_mutex_lock(&(((GstNonstreamAudioDecoder *)(obj))->mutex))
#define GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX(obj)    g_mutex_unlock(&(((GstNonstreamAudioDecoder *)(obj))->mutex))


/**
 * GstNonstreamAudioDecoder:
 *
 * The opaque #GstNonstreamAudioDecoder data structure.
 */
struct _GstNonstreamAudioDecoder
{
  GstElement element;

  /*< protected > */

  /* source and sink pads */
  GstPad *sinkpad, *srcpad;

  /* loading information */
  gint64 upstream_size;
  gboolean loaded_mode;
  GstAdapter *input_data_adapter;

  /* subsong states */
  guint current_subsong;
  GstNonstreamAudioSubsongMode subsong_mode;
  GstClockTime subsong_duration;

  /* output states */
  GstNonstreamAudioOutputMode output_mode;
  gint num_loops;
  gboolean output_format_changed;
  GstAudioInfo output_audio_info;
  /* The difference between these two values is: cur_pos_in_samples is
   * used for the GstBuffer offsets, while num_decoded_samples is used
   * for the segment base time values.
   * cur_pos_in_samples is reset after seeking, looping (when output mode
   * is LOOPING) and switching subsongs, while num_decoded is only reset
   * to 0 after a flushing seek (because flushing seeks alter the
   * pipeline's base_time). */
  guint64 cur_pos_in_samples, num_decoded_samples;
  GstSegment cur_segment;
  gboolean discont;

  /* metadata */
  GstToc *toc;

  /* allocation */
  GstAllocator *allocator;
  GstAllocationParams allocation_params;

  /* thread safety */
  GMutex mutex;
};


/**
 * GstNonstreamAudioDecoderClass:
 * @element_class:              The parent class structure
 * @seek:                       Optional.
 *                              Called when a seek event is received by the parent class.
 *                              new_position is a pointer to a GstClockTime integer which
 *                              contains a position relative to the current subsong.
 *                              Minimum is 0, maximum is the subsong length.
 *                              After this function finishes, new_position is set to the
 *                              actual new position (which may differ from the request
 *                              position, depending on the decoder).
 * @tell:                       Optional.
 *                              Called when a position query is received by the parent class.
 *                              The position that this function returns must be relative to
 *                              the current subsong. Thus, the minimum is 0, and the maximum
 *                              is the subsong length.
 * @load_from_buffer:           Required if loads_from_sinkpad is set to TRUE (the default value).
 *                              Loads the media from the given buffer. The entire media is supplied at once,
 *                              so after this call, loading should be finished. This function
 *                              can also make use of a suggested initial subsong & subsong mode and initial
 *                              playback position (but isn't required to). In case it chooses a different starting
 *                              position, the function must pass this position to *initial_position.
 *                              The subclass does not have to unref the input buffer; the base class does that
 *                              already.
 * @load_from_custom:           Required if loads_from_sinkpad is set to FALSE.
 *                              Loads the media in a way defined by the custom sink. Data is not supplied;
 *                              the derived class has to handle this on its own. Otherwise, this function is
 *                              identical to @load_from_buffer.
 * @get_main_tags:              Optional.
 *                              Returns a tag list containing the main song tags, or NULL if there are
 *                              no such tags. Returned tags will be unref'd. Use this vfunc instead of
 *                              manually pushing a tag event downstream to avoid edge cases where not yet
 *                              pushed sticky tag events get overwritten before they are pushed (can for
 *                              example happen with decodebin if tags are pushed downstream before the
 *                              decodebin pads are linked).
 * @set_current_subsong:        Optional.
 *                              Sets the current subsong. This function is allowed to switch to a different
 *                              subsong than the required one, and can optionally make use of the suggested initial
 *                              position. In case it chooses a different starting position, the function must pass
 *                              this position to *initial_position.
 *                              This function switches the subsong mode to GST_NONSTREAM_AUDIO_SUBSONG_MODE_SINGLE
 *                              automatically.
 *                              If this function is implemented by the subclass, @get_current_subsong and
 *                              @get_num_subsongs should be implemented as well.
 * @get_current_subsong:        Optional.
 *                              Returns the current subsong.
 *                              If the current subsong mode is not GST_NONSTREAM_AUDIO_SUBSONG_MODE_SINGLE, this
 *                              function's return value is undefined.
 *                              If this function is implemented by the subclass,
 *                              @get_num_subsongs should be implemented as well.
 * @get_num_subsongs:           Optional.
 *                              Returns the number of subsongs available.
 *                              The return values 0 and 1 have a similar, but distinct, meaning.
 *                              If this function returns 0, then this decoder does not support subsongs at all.
 *                              @get_current_subsong must then also always return 0. In other words, this function
 *                              either never returns 0, or never returns anything else than 0.
 *                              A return value of 1 means that the media contains either only one or no subsongs
 *                              (the entire song is then considered to be one single subsong). 1 also means that only
 *                              this very media has no or just one subsong, and the decoder itself can
 *                              support multiple subsongs.
 * @get_subsong_duration:       Optional.
 *                              Returns the duration of a subsong. Returns GST_CLOCK_TIME_NONE if duration is unknown.
 * @get_subsong_tags:           Optional.
 *                              Returns tags for a subsong, or NULL if there are no tags.
 *                              Returned tags will be unref'd.
 * @set_subsong_mode:           Optional.
 *                              Sets the current subsong mode. Since this might influence the current playback position,
 *                              this function must set the initial_position integer argument to a defined value.
 *                              If the playback position is not affected at all, it must be set to GST_CLOCK_TIME_NONE.
 *                              If the subsong is restarted after the mode switch, it is recommended to set the value
 *                              to the position in the playback right after the switch (or 0 if the subsongs are always
 *                              reset back to the beginning).
 * @set_num_loops:              Optional.
 *                              Sets the number of loops for playback. If this is called during playback,
 *                              the subclass must set any internal loop counters to zero. A loop value of -1
 *                              means infinite looping; 0 means no looping; and when the num_loops is greater than 0,
 *                              playback should loop exactly num_loops times. If this function is implemented,
 *                              @get_num_loops should be implemented as well. The function can ignore the given values
 *                              and choose another; however, @get_num_loops should return this other value afterwards.
 *                              It is up to the subclass to define where the loop starts and ends. It can mean that only
 *                              a subset at the end or in the middle of a song is repeated, for example.
 *                              If the current subsong mode is GST_NONSTREAM_AUDIO_SUBSONG_MODE_SINGLE, then the subsong
 *                              is repeated this many times. If it is GST_NONSTREAM_AUDIO_SUBSONG_MODE_ALL, then all
 *                              subsongs are repeated this many times. With GST_NONSTREAM_AUDIO_SUBSONG_MODE_DECODER_DEFAULT,
 *                              the behavior is decoder specific.
 * @get_num_loops:              Optional.
 *                              Returns the number of loops for playback.
 * @get_supported_output_modes: Always required.
 *                              Returns a bitmask containing the output modes the subclass supports.
 *                              The mask is formed by a bitwise OR combination of integers, which can be calculated
 *                              this way:  1 << GST_NONSTREAM_AUDIO_OUTPUT_MODE_<mode> , where mode is either STEADY or LOOPING
 * @set_output_mode:            Optional.
 *                              Sets the output mode the subclass has to use. Unlike with most other functions, the subclass
 *                              cannot choose a different mode; it must use the requested one.
 *                              If the output mode is set to LOOPING, @gst_nonstream_audio_decoder_handle_loop
 *                              must be called after playback moved back to the start of a loop.
 * @decode:                     Always required.
 *                              Allocates an output buffer, fills it with decoded audio samples, and must be passed on to
 *                              *buffer . The number of decoded samples must be passed on to *num_samples.
 *                              If decoding finishes or the decoding is no longer possible (for example, due to an
 *                              unrecoverable error), this function returns FALSE, otherwise TRUE.
 * @decide_allocation:          Optional.
 *                              Sets up the allocation parameters for allocating output
 *                              buffers. The passed in query contains the result of the
 *                              downstream allocation query.
 *                              Subclasses should chain up to the parent implementation to
 *                              invoke the default handler.
 * @propose_allocation:         Optional.
 *                              Proposes buffer allocation parameters for upstream elements.
 *                              Subclasses should chain up to the parent implementation to
 *                              invoke the default handler.
 *
 * Subclasses can override any of the available optional virtual methods or not, as
 * needed. At minimum, @load_from_buffer (or @load_from_custom), @get_supported_output_modes,
 * and @decode need to be overridden.
 *
 * All functions are called with a locked decoder mutex.
 *
 * > If GST_ELEMENT_ERROR, GST_ELEMENT_WARNING, or GST_ELEMENT_INFO are called from
 * > inside one of these functions, it is strongly recommended to unlock the decoder mutex
 * > before and re-lock it after these macros to prevent potential deadlocks in case the
 * > application does something with the element when it receives an ERROR/WARNING/INFO
 * > message. Same goes for gst_element_post_message() calls and non-serialized events.
 *
 * By default, this class works by reading media data from the sinkpad, and then commencing
 * playback. Some decoders cannot be given data from a memory block, so the usual way of
 * reading all upstream data and passing it to @load_from_buffer doesn't work then. In this case,
 * set the value of loads_from_sinkpad to FALSE. This changes the way this class operates;
 * it does not require a sinkpad to exist anymore, and will call @load_from_custom instead.
 * One example of a decoder where this makes sense is UADE (Unix Amiga Delitracker Emulator).
 * For some formats (such as TFMX), it needs to do the file loading by itself.
 * Since most decoders can read input data from a memory block, the default value of
 * loads_from_sinkpad is TRUE.
 */
struct _GstNonstreamAudioDecoderClass
{
  GstElementClass element_class;

  gboolean loads_from_sinkpad;

  /*< public > */
  /* virtual methods for subclasses */

  gboolean     (*seek)                       (GstNonstreamAudioDecoder * dec,
                                              GstClockTime * new_position);
  GstClockTime (*tell)                       (GstNonstreamAudioDecoder * dec);

  gboolean     (*load_from_buffer)           (GstNonstreamAudioDecoder * dec,
                                              GstBuffer * source_data,
                                              guint initial_subsong,
                                              GstNonstreamAudioSubsongMode initial_subsong_mode,
                                              GstClockTime * initial_position,
                                              GstNonstreamAudioOutputMode * initial_output_mode,
                                              gint * initial_num_loops);
  gboolean     (*load_from_custom)           (GstNonstreamAudioDecoder * dec,
                                              guint initial_subsong,
                                              GstNonstreamAudioSubsongMode initial_subsong_mode,
                                              GstClockTime * initial_position,
                                              GstNonstreamAudioOutputMode * initial_output_mode,
                                              gint * initial_num_loops);

  GstTagList * (*get_main_tags)              (GstNonstreamAudioDecoder * dec);

  gboolean     (*set_current_subsong)        (GstNonstreamAudioDecoder * dec,
                                              guint subsong,
                                              GstClockTime * initial_position);
  guint        (*get_current_subsong)        (GstNonstreamAudioDecoder * dec);

  guint        (*get_num_subsongs)           (GstNonstreamAudioDecoder * dec);
  GstClockTime (*get_subsong_duration)       (GstNonstreamAudioDecoder * dec,
                                              guint subsong);
  GstTagList * (*get_subsong_tags)           (GstNonstreamAudioDecoder * dec,
                                              guint subsong);
  gboolean (*set_subsong_mode)               (GstNonstreamAudioDecoder * dec,
                                              GstNonstreamAudioSubsongMode mode,
                                              GstClockTime * initial_position);

  gboolean     (*set_num_loops)              (GstNonstreamAudioDecoder * dec,
                                              gint num_loops);
  gint         (*get_num_loops)              (GstNonstreamAudioDecoder * dec);

  guint        (*get_supported_output_modes) (GstNonstreamAudioDecoder * dec);
  gboolean     (*set_output_mode)            (GstNonstreamAudioDecoder * dec,
                                              GstNonstreamAudioOutputMode mode,
                                              GstClockTime * current_position);

  gboolean     (*decode)                     (GstNonstreamAudioDecoder * dec,
                                              GstBuffer ** buffer,
                                              guint * num_samples);

  gboolean     (*negotiate)                  (GstNonstreamAudioDecoder * dec);

  gboolean     (*decide_allocation)          (GstNonstreamAudioDecoder * dec,
                                              GstQuery * query);
  gboolean     (*propose_allocation)         (GstNonstreamAudioDecoder * dec,
                                              GstQuery * query);

  /*< private > */
  gpointer _gst_reserved[GST_PADDING_LARGE];
};


GST_AUDIO_BAD_API
GType gst_nonstream_audio_decoder_get_type (void);


GST_AUDIO_BAD_API
void gst_nonstream_audio_decoder_handle_loop (GstNonstreamAudioDecoder * dec,
                                              GstClockTime new_position);

GST_AUDIO_BAD_API
gboolean gst_nonstream_audio_decoder_set_output_format (GstNonstreamAudioDecoder * dec,
                                                        GstAudioInfo const *audio_info);

GST_AUDIO_BAD_API
gboolean gst_nonstream_audio_decoder_set_output_format_simple (GstNonstreamAudioDecoder * dec,
                                                               guint sample_rate,
                                                               GstAudioFormat sample_format,
                                                               guint num_channels);

GST_AUDIO_BAD_API
void gst_nonstream_audio_decoder_get_downstream_info (GstNonstreamAudioDecoder * dec,
                                                      GstAudioFormat * format,
                                                      gint * sample_rate,
                                                      gint * num_channels);

GST_AUDIO_BAD_API
GstBuffer *gst_nonstream_audio_decoder_allocate_output_buffer (GstNonstreamAudioDecoder * dec,
                                                               gsize size);


G_END_DECLS


#endif /* __GST_NONSTREAM_AUDIO_DECODER_H__ */
