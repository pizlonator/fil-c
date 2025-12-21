/* GStreamer
 * Copyright (C) 2008 David Schleef <ds@schleef.org>
 * Copyright (C) 2011 Mark Nauwelaerts <mark.nauwelaerts@collabora.co.uk>.
 * Copyright (C) 2011 Nokia Corporation. All rights reserved.
 *   Contact: Stefan Kost <stefan.kost@nokia.com>
 * Copyright (C) 2012 Collabora Ltd.
 *	Author : Edward Hervey <edward@collabora.com>
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
 * SECTION:gstvideodecoder
 * @title: GstVideoDecoder
 * @short_description: Base class for video decoders
 *
 * This base class is for video decoders turning encoded data into raw video
 * frames.
 *
 * The GstVideoDecoder base class and derived subclasses should cooperate as
 * follows:
 *
 * ## Configuration
 *
 *   * Initially, GstVideoDecoder calls @start when the decoder element
 *     is activated, which allows the subclass to perform any global setup.
 *
 *   * GstVideoDecoder calls @set_format to inform the subclass of caps
 *     describing input video data that it is about to receive, including
 *     possibly configuration data.
 *     While unlikely, it might be called more than once, if changing input
 *     parameters require reconfiguration.
 *
 *   * Incoming data buffers are processed as needed, described in Data
 *     Processing below.
 *
 *   * GstVideoDecoder calls @stop at end of all processing.
 *
 * ## Data processing
 *
 *   * The base class gathers input data, and optionally allows subclass
 *     to parse this into subsequently manageable chunks, typically
 *     corresponding to and referred to as 'frames'.
 *
 *   * Each input frame is provided in turn to the subclass' @handle_frame
 *     callback.
 *   * When the subclass enables the subframe mode with `gst_video_decoder_set_subframe_mode`,
 *     the base class will provide to the subclass the same input frame with
 *     different input buffers to the subclass @handle_frame
 *     callback. During this call, the subclass needs to take
 *     ownership of the input_buffer as @GstVideoCodecFrame.input_buffer
 *     will have been changed before the next subframe buffer is received.
 *     The subclass will call `gst_video_decoder_have_last_subframe`
 *     when a new input frame can be created by the base class.
 *     Every subframe will share the same @GstVideoCodecFrame.output_buffer
 *     to write the decoding result. The subclass is responsible to protect
 *     its access.
 *
 *   * If codec processing results in decoded data, the subclass should call
 *     @gst_video_decoder_finish_frame to have decoded data pushed
 *     downstream. In subframe mode
 *     the subclass should call @gst_video_decoder_finish_subframe until the
 *     last subframe where it should call @gst_video_decoder_finish_frame.
 *     The subclass can detect the last subframe using GST_VIDEO_BUFFER_FLAG_MARKER
 *     on buffers or using its own logic to collect the subframes.
 *     In case of decoding failure, the subclass must call
 *     @gst_video_decoder_drop_frame or @gst_video_decoder_drop_subframe,
 *     to allow the base class to do timestamp and offset tracking, and possibly
 *     to requeue the frame for a later attempt in the case of reverse playback.
 *
 * ## Shutdown phase
 *
 *   * The GstVideoDecoder class calls @stop to inform the subclass that data
 *     parsing will be stopped.
 *
 * ## Additional Notes
 *
 *   * Seeking/Flushing
 *
 *     * When the pipeline is seeked or otherwise flushed, the subclass is
 *       informed via a call to its @reset callback, with the hard parameter
 *       set to true. This indicates the subclass should drop any internal data
 *       queues and timestamps and prepare for a fresh set of buffers to arrive
 *       for parsing and decoding.
 *
 *   * End Of Stream
 *
 *     * At end-of-stream, the subclass @parse function may be called some final
 *       times with the at_eos parameter set to true, indicating that the element
 *       should not expect any more data to be arriving, and it should parse and
 *       remaining frames and call gst_video_decoder_have_frame() if possible.
 *
 * The subclass is responsible for providing pad template caps for
 * source and sink pads. The pads need to be named "sink" and "src". It also
 * needs to provide information about the output caps, when they are known.
 * This may be when the base class calls the subclass' @set_format function,
 * though it might be during decoding, before calling
 * @gst_video_decoder_finish_frame. This is done via
 * @gst_video_decoder_set_output_state
 *
 * The subclass is also responsible for providing (presentation) timestamps
 * (likely based on corresponding input ones).  If that is not applicable
 * or possible, the base class provides limited framerate based interpolation.
 *
 * Similarly, the base class provides some limited (legacy) seeking support
 * if specifically requested by the subclass, as full-fledged support
 * should rather be left to upstream demuxer, parser or alike.  This simple
 * approach caters for seeking and duration reporting using estimated input
 * bitrates. To enable it, a subclass should call
 * @gst_video_decoder_set_estimate_rate to enable handling of incoming
 * byte-streams.
 *
 * The base class provides some support for reverse playback, in particular
 * in case incoming data is not packetized or upstream does not provide
 * fragments on keyframe boundaries.  However, the subclass should then be
 * prepared for the parsing and frame processing stage to occur separately
 * (in normal forward processing, the latter immediately follows the former),
 * The subclass also needs to ensure the parsing stage properly marks
 * keyframes, unless it knows the upstream elements will do so properly for
 * incoming data.
 *
 * The bare minimum that a functional subclass needs to implement is:
 *
 *   * Provide pad templates
 *   * Inform the base class of output caps via
 *      @gst_video_decoder_set_output_state
 *
 *   * Parse input data, if it is not considered packetized from upstream
 *      Data will be provided to @parse which should invoke
 *      @gst_video_decoder_add_to_frame and @gst_video_decoder_have_frame to
 *      separate the data belonging to each video frame.
 *
 *   * Accept data in @handle_frame and provide decoded results to
 *      @gst_video_decoder_finish_frame, or call @gst_video_decoder_drop_frame.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* TODO
 *
 * * Add a flag/boolean for I-frame-only/image decoders so we can do extra
 *   features, like applying QoS on input (as opposed to after the frame is
 *   decoded).
 * * Add a flag/boolean for decoders that require keyframes, so the base
 *   class can automatically discard non-keyframes before one has arrived
 * * Detect reordered frame/timestamps and fix the pts/dts
 * * Support for GstIndex (or shall we not care ?)
 * * Calculate actual latency based on input/output timestamp/frame_number
 *   and if it exceeds the recorded one, save it and emit a GST_MESSAGE_LATENCY
 * * Emit latency message when it changes
 *
 */

/* Implementation notes:
 * The Video Decoder base class operates in 2 primary processing modes, depending
 * on whether forward or reverse playback is requested.
 *
 * Forward playback:
 *   * Incoming buffer -> @parse() -> add_to_frame()/have_frame() ->
 *     handle_frame() -> push downstream
 *
 * Reverse playback is more complicated, since it involves gathering incoming
 * data regions as we loop backwards through the upstream data. The processing
 * concept (using incoming buffers as containing one frame each to simplify
 * things) is:
 *
 * Upstream data we want to play:
 *  Buffer encoded order:  1  2  3  4  5  6  7  8  9  EOS
 *  Keyframe flag:            K        K
 *  Groupings:             AAAAAAA  BBBBBBB  CCCCCCC
 *
 * Input:
 *  Buffer reception order:  7  8  9  4  5  6  1  2  3  EOS
 *  Keyframe flag:                       K        K
 *  Discont flag:            D        D        D
 *
 * - Each Discont marks a discont in the decoding order.
 * - The keyframes mark where we can start decoding.
 *
 * Initially, we prepend incoming buffers to the gather queue. Whenever the
 * discont flag is set on an incoming buffer, the gather queue is flushed out
 * before the new buffer is collected.
 *
 * The above data will be accumulated in the gather queue like this:
 *
 *   gather queue:  9  8  7
 *                        D
 *
 * When buffer 4 is received (with a DISCONT), we flush the gather queue like
 * this:
 *
 *   while (gather)
 *     take head of queue and prepend to parse queue (this reverses the
 *     sequence, so parse queue is 7 -> 8 -> 9)
 *
 *   Next, we process the parse queue, which now contains all un-parsed packets
 *   (including any leftover ones from the previous decode section)
 *
 *   for each buffer now in the parse queue:
 *     Call the subclass parse function, prepending each resulting frame to
 *     the parse_gather queue. Buffers which precede the first one that
 *     produces a parsed frame are retained in the parse queue for
 *     re-processing on the next cycle of parsing.
 *
 *   The parse_gather queue now contains frame objects ready for decoding,
 *   in reverse order.
 *   parse_gather: 9 -> 8 -> 7
 *
 *   while (parse_gather)
 *     Take the head of the queue and prepend it to the decode queue
 *     If the frame was a keyframe, process the decode queue
 *   decode is now 7-8-9
 *
 *  Processing the decode queue results in frames with attached output buffers
 *  stored in the 'output_queue' ready for outputting in reverse order.
 *
 * After we flushed the gather queue and parsed it, we add 4 to the (now empty)
 * gather queue. We get the following situation:
 *
 *  gather queue:    4
 *  decode queue:    7  8  9
 *
 * After we received 5 (Keyframe) and 6:
 *
 *  gather queue:    6  5  4
 *  decode queue:    7  8  9
 *
 * When we receive 1 (DISCONT) which triggers a flush of the gather queue:
 *
 *   Copy head of the gather queue (6) to decode queue:
 *
 *    gather queue:    5  4
 *    decode queue:    6  7  8  9
 *
 *   Copy head of the gather queue (5) to decode queue. This is a keyframe so we
 *   can start decoding.
 *
 *    gather queue:    4
 *    decode queue:    5  6  7  8  9
 *
 *   Decode frames in decode queue, store raw decoded data in output queue, we
 *   can take the head of the decode queue and prepend the decoded result in the
 *   output queue:
 *
 *    gather queue:    4
 *    decode queue:
 *    output queue:    9  8  7  6  5
 *
 *   Now output all the frames in the output queue, picking a frame from the
 *   head of the queue.
 *
 *   Copy head of the gather queue (4) to decode queue, we flushed the gather
 *   queue and can now store input buffer in the gather queue:
 *
 *    gather queue:    1
 *    decode queue:    4
 *
 *  When we receive EOS, the queue looks like:
 *
 *    gather queue:    3  2  1
 *    decode queue:    4
 *
 *  Fill decode queue, first keyframe we copy is 2:
 *
 *    gather queue:    1
 *    decode queue:    2  3  4
 *
 *  Decoded output:
 *
 *    gather queue:    1
 *    decode queue:
 *    output queue:    4  3  2
 *
 *  Leftover buffer 1 cannot be decoded and must be discarded.
 */

#include "gstvideodecoder.h"
#include "gstvideoutils.h"
#include "gstvideoutilsprivate.h"

#include <gst/video/video.h>
#include <gst/video/video-event.h>
#include <gst/video/gstvideopool.h>
#include <gst/video/gstvideometa.h>
#include <string.h>

GST_DEBUG_CATEGORY (videodecoder_debug);
#define GST_CAT_DEFAULT videodecoder_debug

/* properties */
#define DEFAULT_QOS                 TRUE
#define DEFAULT_MAX_ERRORS          GST_VIDEO_DECODER_MAX_ERRORS
#define DEFAULT_MIN_FORCE_KEY_UNIT_INTERVAL 0
#define DEFAULT_DISCARD_CORRUPTED_FRAMES FALSE
#define DEFAULT_AUTOMATIC_REQUEST_SYNC_POINTS FALSE
#define DEFAULT_AUTOMATIC_REQUEST_SYNC_POINT_FLAGS (GST_VIDEO_DECODER_REQUEST_SYNC_POINT_DISCARD_INPUT | GST_VIDEO_DECODER_REQUEST_SYNC_POINT_CORRUPT_OUTPUT)

/* Used for request_sync_point_frame_number. These are out of range for the
 * frame numbers and can be given special meaning */
#define REQUEST_SYNC_POINT_PENDING G_MAXUINT + 1
#define REQUEST_SYNC_POINT_UNSET G_MAXUINT64

enum
{
  PROP_0,
  PROP_QOS,
  PROP_MAX_ERRORS,
  PROP_MIN_FORCE_KEY_UNIT_INTERVAL,
  PROP_DISCARD_CORRUPTED_FRAMES,
  PROP_AUTOMATIC_REQUEST_SYNC_POINTS,
  PROP_AUTOMATIC_REQUEST_SYNC_POINT_FLAGS,
};

struct _GstVideoDecoderPrivate
{
  /* FIXME introduce a context ? */

  GstBufferPool *pool;
  GstAllocator *allocator;
  GstAllocationParams params;

  /* parse tracking */
  /* input data */
  GstAdapter *input_adapter;
  /* assembles current frame */
  GstAdapter *output_adapter;

  /* Whether we attempt to convert newsegment from bytes to
   * time using a bitrate estimation */
  gboolean do_estimate_rate;

  /* Whether input is considered packetized or not */
  gboolean packetized;

  /* whether input is considered as subframes */
  gboolean subframe_mode;

  /* Error handling */
  gint max_errors;
  gint error_count;
  gboolean had_output_data;
  gboolean had_input_data;

  gboolean needs_format;
  /* input_segment are output_segment identical */
  gboolean in_out_segment_sync;

  /* TRUE if we have an active set of instant rate flags */
  gboolean decode_flags_override;
  GstSegmentFlags decode_flags;

  /* ... being tracked here;
   * only available during parsing or when doing subframe decoding */
  GstVideoCodecFrame *current_frame;
  /* events that should apply to the current frame */
  /* FIXME 2.0: Use a GQueue or similar, see GstVideoCodecFrame::events */
  GList *current_frame_events;
  /* events that should be pushed before the next frame */
  /* FIXME 2.0: Use a GQueue or similar, see GstVideoCodecFrame::events */
  GList *pending_events;

  /* relative offset of input data */
  guint64 input_offset;
  /* relative offset of frame */
  guint64 frame_offset;
  /* tracking ts and offsets */
  GQueue timestamps;

  /* last outgoing ts */
  GstClockTime last_timestamp_out;
  /* incoming pts - dts */
  GstClockTime pts_delta;
  gboolean reordered_output;

  /* FIXME: Consider using a GQueue or other better fitting data structure */
  /* reverse playback */
  /* collect input */
  GList *gather;
  /* to-be-parsed */
  GList *parse;
  /* collected parsed frames */
  GList *parse_gather;
  /* frames to be handled == decoded */
  GList *decode;
  /* collected output - of buffer objects, not frames */
  GList *output_queued;

  /* Properties */
  GstClockTime min_force_key_unit_interval;
  gboolean discard_corrupted_frames;

  /* Key unit related state */
  gboolean needs_sync_point;
  GstVideoDecoderRequestSyncPointFlags request_sync_point_flags;
  guint64 request_sync_point_frame_number;
  GstClockTime last_force_key_unit_time;
  /* -1 if we saw no sync point yet */
  guint64 distance_from_sync;

  gboolean automatic_request_sync_points;
  GstVideoDecoderRequestSyncPointFlags automatic_request_sync_point_flags;

  guint32 system_frame_number;
  guint32 decode_frame_number;

  GQueue frames;                /* Protected with OBJECT_LOCK */
  GstVideoCodecState *input_state;
  GstVideoCodecState *output_state;     /* OBJECT_LOCK and STREAM_LOCK */
  gboolean output_state_changed;

  /* QoS properties */
  gboolean do_qos;
  gdouble proportion;           /* OBJECT_LOCK */
  GstClockTime earliest_time;   /* OBJECT_LOCK */
  GstClockTime qos_frame_duration;      /* OBJECT_LOCK */
  gboolean discont;
  /* qos messages: frames dropped/processed */
  guint dropped;
  guint processed;

  /* Outgoing byte size ? */
  gint64 bytes_out;
  gint64 time;

  gint64 min_latency;
  gint64 max_latency;

  /* Tracks whether the latency message was posted at least once */
  gboolean posted_latency_msg;

  /* upstream stream tags (global tags are passed through as-is) */
  GstTagList *upstream_tags;

  /* subclass tags */
  GstTagList *tags;
  GstTagMergeMode tags_merge_mode;

  gboolean tags_changed;

  /* flags */
  gboolean use_default_pad_acceptcaps;

#ifndef GST_DISABLE_DEBUG
  /* Diagnostic time for reporting the time
   * from flush to first output */
  GstClockTime last_reset_time;
#endif
};

static GstElementClass *parent_class = NULL;
static gint private_offset = 0;

/* cached quark to avoid contention on the global quark table lock */
#define META_TAG_VIDEO meta_tag_video_quark
static GQuark meta_tag_video_quark;

static void gst_video_decoder_class_init (GstVideoDecoderClass * klass);
static void gst_video_decoder_init (GstVideoDecoder * dec,
    GstVideoDecoderClass * klass);

static void gst_video_decoder_finalize (GObject * object);
static void gst_video_decoder_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec);
static void gst_video_decoder_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec);

static gboolean gst_video_decoder_setcaps (GstVideoDecoder * dec,
    GstCaps * caps);
static gboolean gst_video_decoder_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_video_decoder_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static GstFlowReturn gst_video_decoder_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buf);
static gboolean gst_video_decoder_sink_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static GstStateChangeReturn gst_video_decoder_change_state (GstElement *
    element, GstStateChange transition);
static gboolean gst_video_decoder_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static void gst_video_decoder_reset (GstVideoDecoder * decoder, gboolean full,
    gboolean flush_hard);

static GstFlowReturn gst_video_decoder_decode_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame);

static void gst_video_decoder_push_event_list (GstVideoDecoder * decoder,
    GList * events);
static GstClockTime gst_video_decoder_get_frame_duration (GstVideoDecoder *
    decoder, GstVideoCodecFrame * frame);
static GstVideoCodecFrame *gst_video_decoder_new_frame (GstVideoDecoder *
    decoder);
static GstFlowReturn gst_video_decoder_clip_and_push_buf (GstVideoDecoder *
    decoder, GstBuffer * buf);
static GstFlowReturn gst_video_decoder_flush_parse (GstVideoDecoder * dec,
    gboolean at_eos);

static void gst_video_decoder_clear_queues (GstVideoDecoder * dec);

static gboolean gst_video_decoder_sink_event_default (GstVideoDecoder * decoder,
    GstEvent * event);
static gboolean gst_video_decoder_src_event_default (GstVideoDecoder * decoder,
    GstEvent * event);
static gboolean gst_video_decoder_decide_allocation_default (GstVideoDecoder *
    decoder, GstQuery * query);
static gboolean gst_video_decoder_propose_allocation_default (GstVideoDecoder *
    decoder, GstQuery * query);
static gboolean gst_video_decoder_negotiate_default (GstVideoDecoder * decoder);
static GstFlowReturn gst_video_decoder_parse_available (GstVideoDecoder * dec,
    gboolean at_eos, gboolean new_buffer);
static gboolean gst_video_decoder_negotiate_unlocked (GstVideoDecoder *
    decoder);
static gboolean gst_video_decoder_sink_query_default (GstVideoDecoder * decoder,
    GstQuery * query);
static gboolean gst_video_decoder_src_query_default (GstVideoDecoder * decoder,
    GstQuery * query);

static gboolean gst_video_decoder_transform_meta_default (GstVideoDecoder *
    decoder, GstVideoCodecFrame * frame, GstMeta * meta);

static gboolean gst_video_decoder_handle_missing_data_default (GstVideoDecoder *
    decoder, GstClockTime timestamp, GstClockTime duration);

static void gst_video_decoder_replace_input_buffer (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame, GstBuffer ** dest_buffer);

static void gst_video_decoder_copy_metas (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame, GstBuffer * src_buffer,
    GstBuffer * dest_buffer);

static void gst_video_decoder_request_sync_point_internal (GstVideoDecoder *
    dec, GstClockTime deadline, GstVideoDecoderRequestSyncPointFlags flags);

/* we can't use G_DEFINE_ABSTRACT_TYPE because we need the klass in the _init
 * method to get to the padtemplates */
GType
gst_video_decoder_get_type (void)
{
  static gsize type = 0;

  if (g_once_init_enter (&type)) {
    GType _type;
    static const GTypeInfo info = {
      sizeof (GstVideoDecoderClass),
      NULL,
      NULL,
      (GClassInitFunc) gst_video_decoder_class_init,
      NULL,
      NULL,
      sizeof (GstVideoDecoder),
      0,
      (GInstanceInitFunc) gst_video_decoder_init,
    };

    _type = g_type_register_static (GST_TYPE_ELEMENT,
        "GstVideoDecoder", &info, G_TYPE_FLAG_ABSTRACT);

    private_offset =
        g_type_add_instance_private (_type, sizeof (GstVideoDecoderPrivate));

    g_once_init_leave (&type, _type);
  }
  return type;
}

static inline GstVideoDecoderPrivate *
gst_video_decoder_get_instance_private (GstVideoDecoder * self)
{
  return (G_STRUCT_MEMBER_P (self, private_offset));
}

static void
gst_video_decoder_class_init (GstVideoDecoderClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (videodecoder_debug, "videodecoder", 0,
      "Base Video Decoder");

  parent_class = g_type_class_peek_parent (klass);

  if (private_offset != 0)
    g_type_class_adjust_private_offset (klass, &private_offset);

  gobject_class->finalize = gst_video_decoder_finalize;
  gobject_class->get_property = gst_video_decoder_get_property;
  gobject_class->set_property = gst_video_decoder_set_property;

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_video_decoder_change_state);

  klass->sink_event = gst_video_decoder_sink_event_default;
  klass->src_event = gst_video_decoder_src_event_default;
  klass->decide_allocation = gst_video_decoder_decide_allocation_default;
  klass->propose_allocation = gst_video_decoder_propose_allocation_default;
  klass->negotiate = gst_video_decoder_negotiate_default;
  klass->sink_query = gst_video_decoder_sink_query_default;
  klass->src_query = gst_video_decoder_src_query_default;
  klass->transform_meta = gst_video_decoder_transform_meta_default;
  klass->handle_missing_data = gst_video_decoder_handle_missing_data_default;

  /**
   * GstVideoDecoder:qos:
   *
   * If set to %TRUE the decoder will handle QoS events received
   * from downstream elements.
   * This includes dropping output frames which are detected as late
   * using the metrics reported by those events.
   *
   * Since: 1.18
   */
  g_object_class_install_property (gobject_class, PROP_QOS,
      g_param_spec_boolean ("qos", "Quality of Service",
          "Handle Quality-of-Service events from downstream",
          DEFAULT_QOS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstVideoDecoder:max-errors:
   *
   * Maximum number of tolerated consecutive decode errors. See
   * gst_video_decoder_set_max_errors() for more details.
   *
   * Since: 1.18
   */
  g_object_class_install_property (gobject_class, PROP_MAX_ERRORS,
      g_param_spec_int ("max-errors", "Max errors",
          "Max consecutive decoder errors before returning flow error",
          -1, G_MAXINT, DEFAULT_MAX_ERRORS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstVideoDecoder:min-force-key-unit-interval:
   *
   * Minimum interval between force-key-unit events sent upstream by the
   * decoder. Setting this to 0 will cause every event to be handled, setting
   * this to %GST_CLOCK_TIME_NONE will cause every event to be ignored.
   *
   * See gst_video_event_new_upstream_force_key_unit() for more details about
   * force-key-unit events.
   *
   * Since: 1.20
   */
  g_object_class_install_property (gobject_class,
      PROP_MIN_FORCE_KEY_UNIT_INTERVAL,
      g_param_spec_uint64 ("min-force-key-unit-interval",
          "Minimum Force Keyunit Interval",
          "Minimum interval between force-keyunit requests in nanoseconds", 0,
          G_MAXUINT64, DEFAULT_MIN_FORCE_KEY_UNIT_INTERVAL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstVideoDecoder:discard-corrupted-frames:
   *
   * If set to %TRUE the decoder will discard frames that are marked as
   * corrupted instead of outputting them.
   *
   * Since: 1.20
   */
  g_object_class_install_property (gobject_class, PROP_DISCARD_CORRUPTED_FRAMES,
      g_param_spec_boolean ("discard-corrupted-frames",
          "Discard Corrupted Frames",
          "Discard frames marked as corrupted instead of outputting them",
          DEFAULT_DISCARD_CORRUPTED_FRAMES,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstVideoDecoder:automatic-request-sync-points:
   *
   * If set to %TRUE the decoder will automatically request sync points when
   * it seems like a good idea, e.g. if the first frames are not key frames or
   * if packet loss was reported by upstream.
   *
   * Since: 1.20
   */
  g_object_class_install_property (gobject_class,
      PROP_AUTOMATIC_REQUEST_SYNC_POINTS,
      g_param_spec_boolean ("automatic-request-sync-points",
          "Automatic Request Sync Points",
          "Automatically request sync points when it would be useful",
          DEFAULT_AUTOMATIC_REQUEST_SYNC_POINTS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstVideoDecoder:automatic-request-sync-point-flags:
   *
   * GstVideoDecoderRequestSyncPointFlags to use for the automatically
   * requested sync points if `automatic-request-sync-points` is enabled.
   *
   * Since: 1.20
   */
  g_object_class_install_property (gobject_class,
      PROP_AUTOMATIC_REQUEST_SYNC_POINT_FLAGS,
      g_param_spec_flags ("automatic-request-sync-point-flags",
          "Automatic Request Sync Point Flags",
          "Flags to use when automatically requesting sync points",
          GST_TYPE_VIDEO_DECODER_REQUEST_SYNC_POINT_FLAGS,
          DEFAULT_AUTOMATIC_REQUEST_SYNC_POINT_FLAGS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  meta_tag_video_quark = g_quark_from_static_string (GST_META_TAG_VIDEO_STR);
}

static void
gst_video_decoder_init (GstVideoDecoder * decoder, GstVideoDecoderClass * klass)
{
  GstPadTemplate *pad_template;
  GstPad *pad;

  GST_DEBUG_OBJECT (decoder, "gst_video_decoder_init");

  decoder->priv = gst_video_decoder_get_instance_private (decoder);

  pad_template =
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass), "sink");
  g_return_if_fail (pad_template != NULL);

  decoder->sinkpad = pad = gst_pad_new_from_template (pad_template, "sink");

  gst_pad_set_chain_function (pad, GST_DEBUG_FUNCPTR (gst_video_decoder_chain));
  gst_pad_set_event_function (pad,
      GST_DEBUG_FUNCPTR (gst_video_decoder_sink_event));
  gst_pad_set_query_function (pad,
      GST_DEBUG_FUNCPTR (gst_video_decoder_sink_query));
  gst_element_add_pad (GST_ELEMENT (decoder), decoder->sinkpad);

  pad_template =
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass), "src");
  g_return_if_fail (pad_template != NULL);

  decoder->srcpad = pad = gst_pad_new_from_template (pad_template, "src");

  gst_pad_set_event_function (pad,
      GST_DEBUG_FUNCPTR (gst_video_decoder_src_event));
  gst_pad_set_query_function (pad,
      GST_DEBUG_FUNCPTR (gst_video_decoder_src_query));
  gst_element_add_pad (GST_ELEMENT (decoder), decoder->srcpad);

  gst_segment_init (&decoder->input_segment, GST_FORMAT_TIME);
  gst_segment_init (&decoder->output_segment, GST_FORMAT_TIME);

  g_rec_mutex_init (&decoder->stream_lock);

  decoder->priv->input_adapter = gst_adapter_new ();
  decoder->priv->output_adapter = gst_adapter_new ();
  decoder->priv->packetized = TRUE;
  decoder->priv->needs_format = FALSE;

  g_queue_init (&decoder->priv->frames);
  g_queue_init (&decoder->priv->timestamps);

  /* properties */
  decoder->priv->do_qos = DEFAULT_QOS;
  decoder->priv->max_errors = GST_VIDEO_DECODER_MAX_ERRORS;

  decoder->priv->min_latency = 0;
  decoder->priv->max_latency = 0;

  decoder->priv->automatic_request_sync_points =
      DEFAULT_AUTOMATIC_REQUEST_SYNC_POINTS;
  decoder->priv->automatic_request_sync_point_flags =
      DEFAULT_AUTOMATIC_REQUEST_SYNC_POINT_FLAGS;

  gst_video_decoder_reset (decoder, TRUE, TRUE);
}

static GstVideoCodecState *
_new_input_state (GstCaps * caps)
{
  GstVideoCodecState *state;
  GstStructure *structure;
  const GValue *codec_data;

  state = g_new0 (GstVideoCodecState, 1);
  state->ref_count = 1;
  gst_video_info_init (&state->info);
  if (G_UNLIKELY (!gst_video_info_from_caps (&state->info, caps)))
    goto parse_fail;
  state->caps = gst_caps_ref (caps);

  structure = gst_caps_get_structure (caps, 0);

  codec_data = gst_structure_get_value (structure, "codec_data");
  if (codec_data && G_VALUE_TYPE (codec_data) == GST_TYPE_BUFFER)
    state->codec_data = GST_BUFFER (g_value_dup_boxed (codec_data));

  return state;

parse_fail:
  {
    g_free (state);
    return NULL;
  }
}

static GstVideoCodecState *
_new_output_state (GstVideoFormat fmt, GstVideoInterlaceMode interlace_mode,
    guint width, guint height, GstVideoCodecState * reference,
    gboolean copy_interlace_mode)
{
  GstVideoCodecState *state;

  state = g_new0 (GstVideoCodecState, 1);
  state->ref_count = 1;
  gst_video_info_init (&state->info);
  if (!gst_video_info_set_interlaced_format (&state->info, fmt, interlace_mode,
          width, height)) {
    g_free (state);
    return NULL;
  }

  if (reference) {
    GstVideoInfo *tgt, *ref;

    tgt = &state->info;
    ref = &reference->info;

    /* Copy over extra fields from reference state */
    if (copy_interlace_mode)
      tgt->interlace_mode = ref->interlace_mode;
    tgt->flags = ref->flags;
    tgt->chroma_site = ref->chroma_site;
    tgt->colorimetry = ref->colorimetry;
    GST_DEBUG ("reference par %d/%d fps %d/%d",
        ref->par_n, ref->par_d, ref->fps_n, ref->fps_d);
    tgt->par_n = ref->par_n;
    tgt->par_d = ref->par_d;
    tgt->fps_n = ref->fps_n;
    tgt->fps_d = ref->fps_d;
    tgt->views = ref->views;

    GST_VIDEO_INFO_FIELD_ORDER (tgt) = GST_VIDEO_INFO_FIELD_ORDER (ref);

    if (GST_VIDEO_INFO_MULTIVIEW_MODE (ref) != GST_VIDEO_MULTIVIEW_MODE_NONE) {
      GST_VIDEO_INFO_MULTIVIEW_MODE (tgt) = GST_VIDEO_INFO_MULTIVIEW_MODE (ref);
      GST_VIDEO_INFO_MULTIVIEW_FLAGS (tgt) =
          GST_VIDEO_INFO_MULTIVIEW_FLAGS (ref);
    } else {
      /* Default to MONO, overridden as needed by sub-classes */
      GST_VIDEO_INFO_MULTIVIEW_MODE (tgt) = GST_VIDEO_MULTIVIEW_MODE_MONO;
      GST_VIDEO_INFO_MULTIVIEW_FLAGS (tgt) = GST_VIDEO_MULTIVIEW_FLAGS_NONE;
    }
  }

  GST_DEBUG ("reference par %d/%d fps %d/%d",
      state->info.par_n, state->info.par_d,
      state->info.fps_n, state->info.fps_d);

  return state;
}

static gboolean
gst_video_decoder_setcaps (GstVideoDecoder * decoder, GstCaps * caps)
{
  GstVideoDecoderClass *decoder_class;
  GstVideoCodecState *state;
  gboolean ret = TRUE;

  decoder_class = GST_VIDEO_DECODER_GET_CLASS (decoder);

  GST_DEBUG_OBJECT (decoder, "setcaps %" GST_PTR_FORMAT, caps);

  GST_VIDEO_DECODER_STREAM_LOCK (decoder);

  if (decoder->priv->input_state) {
    GST_DEBUG_OBJECT (decoder,
        "Checking if caps changed old %" GST_PTR_FORMAT " new %" GST_PTR_FORMAT,
        decoder->priv->input_state->caps, caps);
    if (gst_caps_is_equal (decoder->priv->input_state->caps, caps))
      goto caps_not_changed;
  }

  state = _new_input_state (caps);

  if (G_UNLIKELY (state == NULL))
    goto parse_fail;

  if (decoder_class->set_format)
    ret = decoder_class->set_format (decoder, state);

  if (!ret)
    goto refused_format;

  if (decoder->priv->input_state)
    gst_video_codec_state_unref (decoder->priv->input_state);
  decoder->priv->input_state = state;

  GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);

  return ret;

caps_not_changed:
  {
    GST_DEBUG_OBJECT (decoder, "Caps did not change - ignore");
    GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);
    return TRUE;
  }

  /* ERRORS */
parse_fail:
  {
    GST_WARNING_OBJECT (decoder, "Failed to parse caps");
    GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);
    return FALSE;
  }

refused_format:
  {
    GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);
    GST_WARNING_OBJECT (decoder, "Subclass refused caps");
    gst_video_codec_state_unref (state);
    return FALSE;
  }
}

static void
gst_video_decoder_finalize (GObject * object)
{
  GstVideoDecoder *decoder;

  decoder = GST_VIDEO_DECODER (object);

  GST_DEBUG_OBJECT (object, "finalize");

  g_rec_mutex_clear (&decoder->stream_lock);

  if (decoder->priv->input_adapter) {
    g_object_unref (decoder->priv->input_adapter);
    decoder->priv->input_adapter = NULL;
  }
  if (decoder->priv->output_adapter) {
    g_object_unref (decoder->priv->output_adapter);
    decoder->priv->output_adapter = NULL;
  }

  if (decoder->priv->input_state)
    gst_video_codec_state_unref (decoder->priv->input_state);
  if (decoder->priv->output_state)
    gst_video_codec_state_unref (decoder->priv->output_state);

  if (decoder->priv->pool) {
    gst_object_unref (decoder->priv->pool);
    decoder->priv->pool = NULL;
  }

  if (decoder->priv->allocator) {
    gst_object_unref (decoder->priv->allocator);
    decoder->priv->allocator = NULL;
  }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_video_decoder_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstVideoDecoder *dec = GST_VIDEO_DECODER (object);
  GstVideoDecoderPrivate *priv = dec->priv;

  switch (property_id) {
    case PROP_QOS:
      g_value_set_boolean (value, priv->do_qos);
      break;
    case PROP_MAX_ERRORS:
      g_value_set_int (value, gst_video_decoder_get_max_errors (dec));
      break;
    case PROP_MIN_FORCE_KEY_UNIT_INTERVAL:
      g_value_set_uint64 (value, priv->min_force_key_unit_interval);
      break;
    case PROP_DISCARD_CORRUPTED_FRAMES:
      g_value_set_boolean (value, priv->discard_corrupted_frames);
      break;
    case PROP_AUTOMATIC_REQUEST_SYNC_POINTS:
      g_value_set_boolean (value, priv->automatic_request_sync_points);
      break;
    case PROP_AUTOMATIC_REQUEST_SYNC_POINT_FLAGS:
      g_value_set_flags (value, priv->automatic_request_sync_point_flags);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
gst_video_decoder_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVideoDecoder *dec = GST_VIDEO_DECODER (object);
  GstVideoDecoderPrivate *priv = dec->priv;

  switch (property_id) {
    case PROP_QOS:
      priv->do_qos = g_value_get_boolean (value);
      break;
    case PROP_MAX_ERRORS:
      gst_video_decoder_set_max_errors (dec, g_value_get_int (value));
      break;
    case PROP_MIN_FORCE_KEY_UNIT_INTERVAL:
      priv->min_force_key_unit_interval = g_value_get_uint64 (value);
      break;
    case PROP_DISCARD_CORRUPTED_FRAMES:
      priv->discard_corrupted_frames = g_value_get_boolean (value);
      break;
    case PROP_AUTOMATIC_REQUEST_SYNC_POINTS:
      priv->automatic_request_sync_points = g_value_get_boolean (value);
      break;
    case PROP_AUTOMATIC_REQUEST_SYNC_POINT_FLAGS:
      priv->automatic_request_sync_point_flags = g_value_get_flags (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

/* hard == FLUSH, otherwise discont */
static GstFlowReturn
gst_video_decoder_flush (GstVideoDecoder * dec, gboolean hard)
{
  GstVideoDecoderClass *klass = GST_VIDEO_DECODER_GET_CLASS (dec);
  GstFlowReturn ret = GST_FLOW_OK;

  GST_LOG_OBJECT (dec, "flush hard %d", hard);

  /* Inform subclass */
  if (klass->reset) {
    GST_FIXME_OBJECT (dec, "GstVideoDecoder::reset() is deprecated");
    klass->reset (dec, hard);
  }

  if (klass->flush)
    klass->flush (dec);

  /* and get (re)set for the sequel */
  gst_video_decoder_reset (dec, FALSE, hard);

  return ret;
}

static GstEvent *
gst_video_decoder_create_merged_tags_event (GstVideoDecoder * dec)
{
  GstTagList *merged_tags;

  GST_LOG_OBJECT (dec, "upstream : %" GST_PTR_FORMAT, dec->priv->upstream_tags);
  GST_LOG_OBJECT (dec, "decoder  : %" GST_PTR_FORMAT, dec->priv->tags);
  GST_LOG_OBJECT (dec, "mode     : %d", dec->priv->tags_merge_mode);

  merged_tags =
      gst_tag_list_merge (dec->priv->upstream_tags, dec->priv->tags,
      dec->priv->tags_merge_mode);

  GST_DEBUG_OBJECT (dec, "merged   : %" GST_PTR_FORMAT, merged_tags);

  if (merged_tags == NULL)
    return NULL;

  if (gst_tag_list_is_empty (merged_tags)) {
    gst_tag_list_unref (merged_tags);
    return NULL;
  }

  return gst_event_new_tag (merged_tags);
}

static gboolean
gst_video_decoder_push_event (GstVideoDecoder * decoder, GstEvent * event)
{
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEGMENT:
    {
      GstSegment segment;

      gst_event_copy_segment (event, &segment);

      GST_DEBUG_OBJECT (decoder, "segment %" GST_SEGMENT_FORMAT, &segment);

      if (segment.format != GST_FORMAT_TIME) {
        GST_DEBUG_OBJECT (decoder, "received non TIME newsegment");
        break;
      }

      GST_VIDEO_DECODER_STREAM_LOCK (decoder);
      decoder->output_segment = segment;
      decoder->priv->in_out_segment_sync =
          gst_segment_is_equal (&decoder->input_segment, &segment);
      decoder->priv->last_timestamp_out = GST_CLOCK_TIME_NONE;
      decoder->priv->earliest_time = GST_CLOCK_TIME_NONE;
      GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);
      break;
    }
    default:
      break;
  }

  GST_DEBUG_OBJECT (decoder, "pushing event %s",
      gst_event_type_get_name (GST_EVENT_TYPE (event)));

  return gst_pad_push_event (decoder->srcpad, event);
}

static GstFlowReturn
gst_video_decoder_parse_available (GstVideoDecoder * dec, gboolean at_eos,
    gboolean new_buffer)
{
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_GET_CLASS (dec);
  GstVideoDecoderPrivate *priv = dec->priv;
  GstFlowReturn ret = GST_FLOW_OK;
  gsize was_available, available;
  guint inactive = 0;

  available = gst_adapter_available (priv->input_adapter);

  while (available || new_buffer) {
    new_buffer = FALSE;
    /* current frame may have been parsed and handled,
     * so we need to set up a new one when asking subclass to parse */
    if (priv->current_frame == NULL)
      priv->current_frame = gst_video_decoder_new_frame (dec);

    was_available = available;
    ret = decoder_class->parse (dec, priv->current_frame,
        priv->input_adapter, at_eos);
    if (ret != GST_FLOW_OK)
      break;

    /* if the subclass returned success (GST_FLOW_OK), it is expected
     * to have collected and submitted a frame, i.e. it should have
     * called gst_video_decoder_have_frame(), or at least consumed a
     * few bytes through gst_video_decoder_add_to_frame().
     *
     * Otherwise, this is an implementation bug, and we error out
     * after 2 failed attempts */
    available = gst_adapter_available (priv->input_adapter);
    if (!priv->current_frame || available != was_available)
      inactive = 0;
    else if (++inactive == 2)
      goto error_inactive;
  }

  return ret;

  /* ERRORS */
error_inactive:
  {
    GST_ERROR_OBJECT (dec, "Failed to consume data. Error in subclass?");
    return GST_FLOW_ERROR;
  }
}

/* This function has to be called with the stream lock taken. */
static GstFlowReturn
gst_video_decoder_drain_out (GstVideoDecoder * dec, gboolean at_eos)
{
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_GET_CLASS (dec);
  GstVideoDecoderPrivate *priv = dec->priv;
  GstFlowReturn ret = GST_FLOW_OK;

  if (dec->input_segment.rate > 0.0) {
    /* Forward mode, if unpacketized, give the child class
     * a final chance to flush out packets */
    if (!priv->packetized) {
      ret = gst_video_decoder_parse_available (dec, TRUE, FALSE);
    }

    if (at_eos) {
      if (decoder_class->finish)
        ret = decoder_class->finish (dec);
    } else {
      if (decoder_class->drain) {
        ret = decoder_class->drain (dec);
      } else {
        GST_FIXME_OBJECT (dec, "Sub-class should implement drain()");
      }
    }
  } else {
    /* Reverse playback mode */
    ret = gst_video_decoder_flush_parse (dec, TRUE);
  }

  return ret;
}

static GList *
_flush_events (GstPad * pad, GList * events)
{
  GList *tmp;

  for (tmp = events; tmp; tmp = tmp->next) {
    if (GST_EVENT_TYPE (tmp->data) != GST_EVENT_EOS &&
        GST_EVENT_TYPE (tmp->data) != GST_EVENT_SEGMENT &&
        GST_EVENT_IS_STICKY (tmp->data)) {
      gst_pad_store_sticky_event (pad, GST_EVENT_CAST (tmp->data));
    }
    gst_event_unref (tmp->data);
  }
  g_list_free (events);

  return NULL;
}

/* Must be called holding the GST_VIDEO_DECODER_STREAM_LOCK */
static gboolean
gst_video_decoder_negotiate_default_caps (GstVideoDecoder * decoder)
{
  GstCaps *caps, *templcaps;
  GstVideoCodecState *state;
  GstVideoInfo info;
  gint i;
  gint caps_size;
  GstStructure *structure;

  templcaps = gst_pad_get_pad_template_caps (decoder->srcpad);
  caps = gst_pad_peer_query_caps (decoder->srcpad, templcaps);
  if (caps)
    gst_caps_unref (templcaps);
  else
    caps = templcaps;
  templcaps = NULL;

  if (!caps || gst_caps_is_empty (caps) || gst_caps_is_any (caps))
    goto caps_error;

  GST_LOG_OBJECT (decoder, "peer caps %" GST_PTR_FORMAT, caps);

  /* before fixating, try to use whatever upstream provided */
  caps = gst_caps_make_writable (caps);
  caps_size = gst_caps_get_size (caps);
  if (decoder->priv->input_state && decoder->priv->input_state->caps) {
    GstCaps *sinkcaps = decoder->priv->input_state->caps;
    GstStructure *structure = gst_caps_get_structure (sinkcaps, 0);
    gint width, height;

    if (gst_structure_get_int (structure, "width", &width)) {
      for (i = 0; i < caps_size; i++) {
        gst_structure_set (gst_caps_get_structure (caps, i), "width",
            G_TYPE_INT, width, NULL);
      }
    }

    if (gst_structure_get_int (structure, "height", &height)) {
      for (i = 0; i < caps_size; i++) {
        gst_structure_set (gst_caps_get_structure (caps, i), "height",
            G_TYPE_INT, height, NULL);
      }
    }
  }

  for (i = 0; i < caps_size; i++) {
    structure = gst_caps_get_structure (caps, i);
    /* Random I420 1280x720 for fixation */
    if (gst_structure_has_field (structure, "format"))
      gst_structure_fixate_field_string (structure, "format", "I420");
    else
      gst_structure_set (structure, "format", G_TYPE_STRING, "I420", NULL);

    if (gst_structure_has_field (structure, "width"))
      gst_structure_fixate_field_nearest_int (structure, "width", 1280);
    else
      gst_structure_set (structure, "width", G_TYPE_INT, 1280, NULL);

    if (gst_structure_has_field (structure, "height"))
      gst_structure_fixate_field_nearest_int (structure, "height", 720);
    else
      gst_structure_set (structure, "height", G_TYPE_INT, 720, NULL);
  }
  caps = gst_caps_fixate (caps);

  if (!caps || !gst_video_info_from_caps (&info, caps))
    goto caps_error;

  GST_INFO_OBJECT (decoder,
      "Chose default caps %" GST_PTR_FORMAT " for initial gap", caps);
  state =
      gst_video_decoder_set_output_state (decoder, info.finfo->format,
      info.width, info.height, decoder->priv->input_state);
  gst_video_codec_state_unref (state);
  gst_caps_unref (caps);

  return TRUE;

caps_error:
  {
    if (caps)
      gst_caps_unref (caps);
    return FALSE;
  }
}

static gboolean
gst_video_decoder_handle_missing_data_default (GstVideoDecoder *
    decoder, GstClockTime timestamp, GstClockTime duration)
{
  GstVideoDecoderPrivate *priv;

  priv = decoder->priv;

  if (priv->automatic_request_sync_points) {
    GstClockTime deadline =
        gst_segment_to_running_time (&decoder->input_segment, GST_FORMAT_TIME,
        timestamp);

    GST_DEBUG_OBJECT (decoder,
        "Requesting sync point for missing data at running time %"
        GST_TIME_FORMAT " timestamp %" GST_TIME_FORMAT " with duration %"
        GST_TIME_FORMAT, GST_TIME_ARGS (deadline), GST_TIME_ARGS (timestamp),
        GST_TIME_ARGS (duration));

    gst_video_decoder_request_sync_point_internal (decoder, deadline,
        priv->automatic_request_sync_point_flags);
  }

  return TRUE;
}

static gboolean
gst_video_decoder_sink_event_default (GstVideoDecoder * decoder,
    GstEvent * event)
{
  GstVideoDecoderClass *decoder_class;
  GstVideoDecoderPrivate *priv;
  gboolean ret = FALSE;
  gboolean forward_immediate = FALSE;

  decoder_class = GST_VIDEO_DECODER_GET_CLASS (decoder);

  priv = decoder->priv;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_STREAM_START:
    {
      GstFlowReturn flow_ret = GST_FLOW_OK;

      GST_VIDEO_DECODER_STREAM_LOCK (decoder);
      flow_ret = gst_video_decoder_drain_out (decoder, FALSE);
      ret = (flow_ret == GST_FLOW_OK);

      GST_DEBUG_OBJECT (decoder, "received STREAM_START. Clearing taglist");
      /* Flush upstream tags after a STREAM_START */
      if (priv->upstream_tags) {
        gst_tag_list_unref (priv->upstream_tags);
        priv->upstream_tags = NULL;
        priv->tags_changed = TRUE;
      }
      GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);

      /* Forward STREAM_START immediately. Everything is drained after
       * the STREAM_START event and we can forward this event immediately
       * now without having buffers out of order.
       */
      forward_immediate = TRUE;
      break;
    }
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      ret = gst_video_decoder_setcaps (decoder, caps);
      gst_event_unref (event);
      event = NULL;
      break;
    }
    case GST_EVENT_SEGMENT_DONE:
    {
      GstFlowReturn flow_ret = GST_FLOW_OK;

      GST_VIDEO_DECODER_STREAM_LOCK (decoder);
      flow_ret = gst_video_decoder_drain_out (decoder, FALSE);
      GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);
      ret = (flow_ret == GST_FLOW_OK);

      /* Forward SEGMENT_DONE immediately. This is required
       * because no buffer or serialized event might come
       * after SEGMENT_DONE and nothing could trigger another
       * _finish_frame() call.
       *
       * The subclass can override this behaviour by overriding
       * the ::sink_event() vfunc and not chaining up to the
       * parent class' ::sink_event() until a later time.
       */
      forward_immediate = TRUE;
      break;
    }
    case GST_EVENT_EOS:
    {
      GstFlowReturn flow_ret = GST_FLOW_OK;

      GST_VIDEO_DECODER_STREAM_LOCK (decoder);
      flow_ret = gst_video_decoder_drain_out (decoder, TRUE);
      GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);
      ret = (flow_ret == GST_FLOW_OK);

      /* Error out even if EOS was ok when we had input, but no output */
      if (ret && priv->had_input_data && !priv->had_output_data) {
        GST_ELEMENT_ERROR (decoder, STREAM, DECODE,
            ("No valid frames decoded before end of stream"),
            ("no valid frames found"));
      }

      /* Forward EOS immediately. This is required because no
       * buffer or serialized event will come after EOS and
       * nothing could trigger another _finish_frame() call.
       *
       * The subclass can override this behaviour by overriding
       * the ::sink_event() vfunc and not chaining up to the
       * parent class' ::sink_event() until a later time.
       */
      forward_immediate = TRUE;
      break;
    }
    case GST_EVENT_GAP:
    {
      GstClockTime timestamp, duration;
      GstGapFlags gap_flags = 0;
      GstFlowReturn flow_ret = GST_FLOW_OK;
      gboolean needs_reconfigure = FALSE;
      GList *events;
      GList *frame_events;

      gst_event_parse_gap (event, &timestamp, &duration);
      gst_event_parse_gap_flags (event, &gap_flags);

      GST_VIDEO_DECODER_STREAM_LOCK (decoder);
      /* If this is not missing data, or the subclass does not handle it
       * specifically, then drain out the decoder and forward the event
       * directly. */
      if ((gap_flags & GST_GAP_FLAG_MISSING_DATA) == 0
          || !decoder_class->handle_missing_data
          || decoder_class->handle_missing_data (decoder, timestamp,
              duration)) {
        if (decoder->input_segment.flags & GST_SEEK_FLAG_TRICKMODE_KEY_UNITS)
          flow_ret = gst_video_decoder_drain_out (decoder, FALSE);
        ret = (flow_ret == GST_FLOW_OK);

        /* Ensure we have caps before forwarding the event */
        if (!decoder->priv->output_state) {
          if (!gst_video_decoder_negotiate_default_caps (decoder)) {
            GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);
            GST_ELEMENT_ERROR (decoder, STREAM, FORMAT, (NULL),
                ("Decoder output not negotiated before GAP event."));
            forward_immediate = TRUE;
            break;
          }
          needs_reconfigure = TRUE;
        }

        needs_reconfigure = gst_pad_check_reconfigure (decoder->srcpad)
            || needs_reconfigure;
        if (decoder->priv->output_state_changed || needs_reconfigure) {
          if (!gst_video_decoder_negotiate_unlocked (decoder)) {
            GST_WARNING_OBJECT (decoder, "Failed to negotiate with downstream");
            gst_pad_mark_reconfigure (decoder->srcpad);
          }
        }

        GST_DEBUG_OBJECT (decoder, "Pushing all pending serialized events"
            " before the gap");
        events = decoder->priv->pending_events;
        frame_events = decoder->priv->current_frame_events;
        decoder->priv->pending_events = NULL;
        decoder->priv->current_frame_events = NULL;

        GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);

        gst_video_decoder_push_event_list (decoder, events);
        gst_video_decoder_push_event_list (decoder, frame_events);

        /* Forward GAP immediately. Everything is drained after
         * the GAP event and we can forward this event immediately
         * now without having buffers out of order.
         */
        forward_immediate = TRUE;
      } else {
        GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);
        gst_clear_event (&event);
      }
      break;
    }
    case GST_EVENT_CUSTOM_DOWNSTREAM:
    {
      gboolean in_still;
      GstFlowReturn flow_ret = GST_FLOW_OK;

      if (gst_video_event_parse_still_frame (event, &in_still)) {
        if (in_still) {
          GST_DEBUG_OBJECT (decoder, "draining current data for still-frame");
          GST_VIDEO_DECODER_STREAM_LOCK (decoder);
          flow_ret = gst_video_decoder_drain_out (decoder, FALSE);
          GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);
          ret = (flow_ret == GST_FLOW_OK);
        }
        /* Forward STILL_FRAME immediately. Everything is drained after
         * the STILL_FRAME event and we can forward this event immediately
         * now without having buffers out of order.
         */
        forward_immediate = TRUE;
      }
      break;
    }
    case GST_EVENT_SEGMENT:
    {
      GstSegment segment;

      gst_event_copy_segment (event, &segment);

      if (segment.format == GST_FORMAT_TIME) {
        GST_DEBUG_OBJECT (decoder,
            "received TIME SEGMENT %" GST_SEGMENT_FORMAT, &segment);
      } else {
        gint64 start;

        GST_DEBUG_OBJECT (decoder,
            "received SEGMENT %" GST_SEGMENT_FORMAT, &segment);

        /* handle newsegment as a result from our legacy simple seeking */
        /* note that initial 0 should convert to 0 in any case */
        if (priv->do_estimate_rate &&
            gst_pad_query_convert (decoder->sinkpad, GST_FORMAT_BYTES,
                segment.start, GST_FORMAT_TIME, &start)) {
          /* best attempt convert */
          /* as these are only estimates, stop is kept open-ended to avoid
           * premature cutting */
          GST_DEBUG_OBJECT (decoder,
              "converted to TIME start %" GST_TIME_FORMAT,
              GST_TIME_ARGS (start));
          segment.start = start;
          segment.stop = GST_CLOCK_TIME_NONE;
          segment.time = start;
          /* replace event */
          gst_event_unref (event);
          event = gst_event_new_segment (&segment);
        } else {
          goto newseg_wrong_format;
        }
      }

      GST_VIDEO_DECODER_STREAM_LOCK (decoder);

      /* Update the decode flags in the segment if we have an instant-rate
       * override active */
      GST_OBJECT_LOCK (decoder);
      if (!priv->decode_flags_override)
        priv->decode_flags = segment.flags;
      else {
        segment.flags &= ~GST_SEGMENT_INSTANT_FLAGS;
        segment.flags |= priv->decode_flags & GST_SEGMENT_INSTANT_FLAGS;
      }

      decoder->input_segment = segment;
      decoder->priv->in_out_segment_sync = FALSE;

      GST_OBJECT_UNLOCK (decoder);
      GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);

      break;
    }
    case GST_EVENT_INSTANT_RATE_CHANGE:
    {
      GstSegmentFlags flags;
      GstSegment *seg;

      gst_event_parse_instant_rate_change (event, NULL, &flags);

      GST_OBJECT_LOCK (decoder);
      priv->decode_flags_override = TRUE;
      priv->decode_flags = flags;

      /* Update the input segment flags */
      seg = &decoder->input_segment;
      seg->flags &= ~GST_SEGMENT_INSTANT_FLAGS;
      seg->flags |= priv->decode_flags & GST_SEGMENT_INSTANT_FLAGS;
      GST_OBJECT_UNLOCK (decoder);
      break;
    }
    case GST_EVENT_FLUSH_STOP:
    {
      GList *l;

      GST_VIDEO_DECODER_STREAM_LOCK (decoder);
      for (l = priv->frames.head; l; l = l->next) {
        GstVideoCodecFrame *frame = l->data;

        frame->events = _flush_events (decoder->srcpad, frame->events);
      }
      priv->current_frame_events = _flush_events (decoder->srcpad,
          decoder->priv->current_frame_events);

      /* well, this is kind of worse than a DISCONT */
      gst_video_decoder_flush (decoder, TRUE);
      GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);
      /* Forward FLUSH_STOP immediately. This is required because it is
       * expected to be forwarded immediately and no buffers are queued
       * anyway.
       */
      forward_immediate = TRUE;
      break;
    }
    case GST_EVENT_TAG:
    {
      GstTagList *tags;

      gst_event_parse_tag (event, &tags);

      if (gst_tag_list_get_scope (tags) == GST_TAG_SCOPE_STREAM) {
        GST_VIDEO_DECODER_STREAM_LOCK (decoder);
        if (priv->upstream_tags != tags) {
          if (priv->upstream_tags)
            gst_tag_list_unref (priv->upstream_tags);
          priv->upstream_tags = gst_tag_list_ref (tags);
          GST_INFO_OBJECT (decoder, "upstream tags: %" GST_PTR_FORMAT, tags);
        }
        gst_event_unref (event);
        event = gst_video_decoder_create_merged_tags_event (decoder);
        GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);
        if (!event)
          ret = TRUE;
      }
      break;
    }
    default:
      break;
  }

  /* Forward non-serialized events immediately, and all other
   * events which can be forwarded immediately without potentially
   * causing the event to go out of order with other events and
   * buffers as decided above.
   */
  if (event) {
    if (!GST_EVENT_IS_SERIALIZED (event) || forward_immediate) {
      ret = gst_video_decoder_push_event (decoder, event);
    } else {
      GST_VIDEO_DECODER_STREAM_LOCK (decoder);
      decoder->priv->current_frame_events =
          g_list_prepend (decoder->priv->current_frame_events, event);
      GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);
      ret = TRUE;
    }
  }

  return ret;

newseg_wrong_format:
  {
    GST_DEBUG_OBJECT (decoder, "received non TIME newsegment");
    gst_event_unref (event);
    /* SWALLOW EVENT */
    return TRUE;
  }
}

static gboolean
gst_video_decoder_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstVideoDecoder *decoder;
  GstVideoDecoderClass *decoder_class;
  gboolean ret = FALSE;

  decoder = GST_VIDEO_DECODER (parent);
  decoder_class = GST_VIDEO_DECODER_GET_CLASS (decoder);

  GST_DEBUG_OBJECT (decoder, "received event %d, %s", GST_EVENT_TYPE (event),
      GST_EVENT_TYPE_NAME (event));

  if (decoder_class->sink_event)
    ret = decoder_class->sink_event (decoder, event);

  return ret;
}

/* perform upstream byte <-> time conversion (duration, seeking)
 * if subclass allows and if enough data for moderately decent conversion */
static inline gboolean
gst_video_decoder_do_byte (GstVideoDecoder * dec)
{
  gboolean ret;

  GST_OBJECT_LOCK (dec);
  ret = dec->priv->do_estimate_rate && (dec->priv->bytes_out > 0)
      && (dec->priv->time > GST_SECOND);
  GST_OBJECT_UNLOCK (dec);

  return ret;
}

static gboolean
gst_video_decoder_do_seek (GstVideoDecoder * dec, GstEvent * event)
{
  GstFormat format;
  GstSeekFlags flags;
  GstSeekType start_type, end_type;
  gdouble rate;
  gint64 start, start_time, end_time;
  GstSegment seek_segment;
  guint32 seqnum;

  gst_event_parse_seek (event, &rate, &format, &flags, &start_type,
      &start_time, &end_type, &end_time);

  /* we'll handle plain open-ended flushing seeks with the simple approach */
  if (rate != 1.0) {
    GST_DEBUG_OBJECT (dec, "unsupported seek: rate");
    return FALSE;
  }

  if (start_type != GST_SEEK_TYPE_SET) {
    GST_DEBUG_OBJECT (dec, "unsupported seek: start time");
    return FALSE;
  }

  if ((end_type != GST_SEEK_TYPE_SET && end_type != GST_SEEK_TYPE_NONE) ||
      (end_type == GST_SEEK_TYPE_SET && end_time != GST_CLOCK_TIME_NONE)) {
    GST_DEBUG_OBJECT (dec, "unsupported seek: end time");
    return FALSE;
  }

  if (!(flags & GST_SEEK_FLAG_FLUSH)) {
    GST_DEBUG_OBJECT (dec, "unsupported seek: not flushing");
    return FALSE;
  }

  memcpy (&seek_segment, &dec->output_segment, sizeof (seek_segment));
  gst_segment_do_seek (&seek_segment, rate, format, flags, start_type,
      start_time, end_type, end_time, NULL);
  start_time = seek_segment.position;

  if (!gst_pad_query_convert (dec->sinkpad, GST_FORMAT_TIME, start_time,
          GST_FORMAT_BYTES, &start)) {
    GST_DEBUG_OBJECT (dec, "conversion failed");
    return FALSE;
  }

  seqnum = gst_event_get_seqnum (event);
  event = gst_event_new_seek (1.0, GST_FORMAT_BYTES, flags,
      GST_SEEK_TYPE_SET, start, GST_SEEK_TYPE_NONE, -1);
  gst_event_set_seqnum (event, seqnum);

  GST_DEBUG_OBJECT (dec, "seeking to %" GST_TIME_FORMAT " at byte offset %"
      G_GINT64_FORMAT, GST_TIME_ARGS (start_time), start);

  return gst_pad_push_event (dec->sinkpad, event);
}

static gboolean
gst_video_decoder_src_event_default (GstVideoDecoder * decoder,
    GstEvent * event)
{
  GstVideoDecoderPrivate *priv;
  gboolean res = FALSE;

  priv = decoder->priv;

  GST_DEBUG_OBJECT (decoder,
      "received event %d, %s", GST_EVENT_TYPE (event),
      GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
    {
      GstFormat format;
      gdouble rate;
      GstSeekFlags flags;
      GstSeekType start_type, stop_type;
      gint64 start, stop;
      gint64 tstart, tstop;
      guint32 seqnum;

      gst_event_parse_seek (event, &rate, &format, &flags, &start_type, &start,
          &stop_type, &stop);
      seqnum = gst_event_get_seqnum (event);

      /* upstream gets a chance first */
      if ((res = gst_pad_push_event (decoder->sinkpad, event)))
        break;

      /* if upstream fails for a time seek, maybe we can help if allowed */
      if (format == GST_FORMAT_TIME) {
        if (gst_video_decoder_do_byte (decoder))
          res = gst_video_decoder_do_seek (decoder, event);
        break;
      }

      /* ... though a non-time seek can be aided as well */
      /* First bring the requested format to time */
      if (!(res =
              gst_pad_query_convert (decoder->srcpad, format, start,
                  GST_FORMAT_TIME, &tstart)))
        goto convert_error;
      if (!(res =
              gst_pad_query_convert (decoder->srcpad, format, stop,
                  GST_FORMAT_TIME, &tstop)))
        goto convert_error;

      /* then seek with time on the peer */
      event = gst_event_new_seek (rate, GST_FORMAT_TIME,
          flags, start_type, tstart, stop_type, tstop);
      gst_event_set_seqnum (event, seqnum);

      res = gst_pad_push_event (decoder->sinkpad, event);
      break;
    }
    case GST_EVENT_QOS:
    {
      GstQOSType type;
      gdouble proportion;
      GstClockTimeDiff diff;
      GstClockTime timestamp;

      gst_event_parse_qos (event, &type, &proportion, &diff, &timestamp);

      GST_OBJECT_LOCK (decoder);
      priv->proportion = proportion;
      if (G_LIKELY (GST_CLOCK_TIME_IS_VALID (timestamp))) {
        if (G_UNLIKELY (diff > 0)) {
          priv->earliest_time = timestamp + 2 * diff + priv->qos_frame_duration;
        } else {
          priv->earliest_time = timestamp + diff;
        }
      } else {
        priv->earliest_time = GST_CLOCK_TIME_NONE;
      }
      GST_OBJECT_UNLOCK (decoder);

      GST_DEBUG_OBJECT (decoder,
          "got QoS %" GST_TIME_FORMAT ", %" GST_STIME_FORMAT ", %g",
          GST_TIME_ARGS (timestamp), GST_STIME_ARGS (diff), proportion);

      res = gst_pad_push_event (decoder->sinkpad, event);
      break;
    }
    default:
      res = gst_pad_push_event (decoder->sinkpad, event);
      break;
  }
done:
  return res;

convert_error:
  GST_DEBUG_OBJECT (decoder, "could not convert format");
  goto done;
}

static gboolean
gst_video_decoder_src_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstVideoDecoder *decoder;
  GstVideoDecoderClass *decoder_class;
  gboolean ret = FALSE;

  decoder = GST_VIDEO_DECODER (parent);
  decoder_class = GST_VIDEO_DECODER_GET_CLASS (decoder);

  GST_DEBUG_OBJECT (decoder, "received event %d, %s", GST_EVENT_TYPE (event),
      GST_EVENT_TYPE_NAME (event));

  if (decoder_class->src_event)
    ret = decoder_class->src_event (decoder, event);

  return ret;
}

static gboolean
gst_video_decoder_src_query_default (GstVideoDecoder * dec, GstQuery * query)
{
  GstPad *pad = GST_VIDEO_DECODER_SRC_PAD (dec);
  gboolean res = TRUE;

  GST_LOG_OBJECT (dec, "handling query: %" GST_PTR_FORMAT, query);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_POSITION:
    {
      GstFormat format;
      gint64 time, value;

      /* upstream gets a chance first */
      if ((res = gst_pad_peer_query (dec->sinkpad, query))) {
        GST_LOG_OBJECT (dec, "returning peer response");
        break;
      }

      /* Refuse BYTES format queries. If it made sense to
       * answer them, upstream would have already */
      gst_query_parse_position (query, &format, NULL);

      if (format == GST_FORMAT_BYTES) {
        GST_LOG_OBJECT (dec, "Ignoring BYTES position query");
        break;
      }

      /* we start from the last seen time */
      time = dec->priv->last_timestamp_out;
      /* correct for the segment values */
      time = gst_segment_to_stream_time (&dec->output_segment,
          GST_FORMAT_TIME, time);

      GST_LOG_OBJECT (dec,
          "query %p: our time: %" GST_TIME_FORMAT, query, GST_TIME_ARGS (time));

      /* and convert to the final format */
      if (!(res = gst_pad_query_convert (pad, GST_FORMAT_TIME, time,
                  format, &value)))
        break;

      gst_query_set_position (query, format, value);

      GST_LOG_OBJECT (dec,
          "query %p: we return %" G_GINT64_FORMAT " (format %u)", query, value,
          format);
      break;
    }
    case GST_QUERY_DURATION:
    {
      GstFormat format;

      /* upstream in any case */
      if ((res = gst_pad_query_default (pad, GST_OBJECT (dec), query)))
        break;

      gst_query_parse_duration (query, &format, NULL);
      /* try answering TIME by converting from BYTE if subclass allows  */
      if (format == GST_FORMAT_TIME && gst_video_decoder_do_byte (dec)) {
        gint64 value;

        if (gst_pad_peer_query_duration (dec->sinkpad, GST_FORMAT_BYTES,
                &value)) {
          GST_LOG_OBJECT (dec, "upstream size %" G_GINT64_FORMAT, value);
          if (gst_pad_query_convert (dec->sinkpad,
                  GST_FORMAT_BYTES, value, GST_FORMAT_TIME, &value)) {
            gst_query_set_duration (query, GST_FORMAT_TIME, value);
            res = TRUE;
          }
        }
      }
      break;
    }
    case GST_QUERY_CONVERT:
    {
      GstFormat src_fmt, dest_fmt;
      gint64 src_val, dest_val;

      GST_DEBUG_OBJECT (dec, "convert query");

      gst_query_parse_convert (query, &src_fmt, &src_val, &dest_fmt, &dest_val);
      GST_OBJECT_LOCK (dec);
      if (dec->priv->output_state != NULL)
        res = __gst_video_rawvideo_convert (dec->priv->output_state,
            src_fmt, src_val, &dest_fmt, &dest_val);
      else
        res = FALSE;
      GST_OBJECT_UNLOCK (dec);
      if (!res)
        goto error;
      gst_query_set_convert (query, src_fmt, src_val, dest_fmt, dest_val);
      break;
    }
    case GST_QUERY_LATENCY:
    {
      gboolean live;
      GstClockTime min_latency, max_latency;

      res = gst_pad_peer_query (dec->sinkpad, query);
      if (res) {
        gst_query_parse_latency (query, &live, &min_latency, &max_latency);
        GST_DEBUG_OBJECT (dec, "Peer qlatency: live %d, min %"
            GST_TIME_FORMAT " max %" GST_TIME_FORMAT, live,
            GST_TIME_ARGS (min_latency), GST_TIME_ARGS (max_latency));

        GST_OBJECT_LOCK (dec);
        min_latency += dec->priv->min_latency;
        if (max_latency == GST_CLOCK_TIME_NONE
            || dec->priv->max_latency == GST_CLOCK_TIME_NONE)
          max_latency = GST_CLOCK_TIME_NONE;
        else
          max_latency += dec->priv->max_latency;
        GST_OBJECT_UNLOCK (dec);

        gst_query_set_latency (query, live, min_latency, max_latency);
      }
    }
      break;
    default:
      res = gst_pad_query_default (pad, GST_OBJECT (dec), query);
  }
  return res;

error:
  GST_ERROR_OBJECT (dec, "query failed");
  return res;
}

static gboolean
gst_video_decoder_src_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstVideoDecoder *decoder;
  GstVideoDecoderClass *decoder_class;
  gboolean ret = FALSE;

  decoder = GST_VIDEO_DECODER (parent);
  decoder_class = GST_VIDEO_DECODER_GET_CLASS (decoder);

  GST_DEBUG_OBJECT (decoder, "received query %d, %s", GST_QUERY_TYPE (query),
      GST_QUERY_TYPE_NAME (query));

  if (decoder_class->src_query)
    ret = decoder_class->src_query (decoder, query);

  return ret;
}

/**
 * gst_video_decoder_proxy_getcaps:
 * @decoder: a #GstVideoDecoder
 * @caps: (nullable): initial caps
 * @filter: (nullable): filter caps
 *
 * Returns caps that express @caps (or sink template caps if @caps == NULL)
 * restricted to resolution/format/... combinations supported by downstream
 * elements.
 *
 * Returns: (transfer full): a #GstCaps owned by caller
 *
 * Since: 1.6
 */
GstCaps *
gst_video_decoder_proxy_getcaps (GstVideoDecoder * decoder, GstCaps * caps,
    GstCaps * filter)
{
  return __gst_video_element_proxy_getcaps (GST_ELEMENT_CAST (decoder),
      GST_VIDEO_DECODER_SINK_PAD (decoder),
      GST_VIDEO_DECODER_SRC_PAD (decoder), caps, filter);
}

static GstCaps *
gst_video_decoder_sink_getcaps (GstVideoDecoder * decoder, GstCaps * filter)
{
  GstVideoDecoderClass *klass;
  GstCaps *caps;

  klass = GST_VIDEO_DECODER_GET_CLASS (decoder);

  if (klass->getcaps)
    caps = klass->getcaps (decoder, filter);
  else
    caps = gst_video_decoder_proxy_getcaps (decoder, NULL, filter);

  GST_LOG_OBJECT (decoder, "Returning caps %" GST_PTR_FORMAT, caps);

  return caps;
}

static gboolean
gst_video_decoder_sink_query_default (GstVideoDecoder * decoder,
    GstQuery * query)
{
  GstPad *pad = GST_VIDEO_DECODER_SINK_PAD (decoder);
  GstVideoDecoderPrivate *priv;
  gboolean res = FALSE;

  priv = decoder->priv;

  GST_LOG_OBJECT (decoder, "handling query: %" GST_PTR_FORMAT, query);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONVERT:
    {
      GstFormat src_fmt, dest_fmt;
      gint64 src_val, dest_val;

      gst_query_parse_convert (query, &src_fmt, &src_val, &dest_fmt, &dest_val);
      GST_OBJECT_LOCK (decoder);
      res =
          __gst_video_encoded_video_convert (priv->bytes_out, priv->time,
          src_fmt, src_val, &dest_fmt, &dest_val);
      GST_OBJECT_UNLOCK (decoder);
      if (!res)
        goto error;
      gst_query_set_convert (query, src_fmt, src_val, dest_fmt, dest_val);
      break;
    }
    case GST_QUERY_ALLOCATION:{
      GstVideoDecoderClass *klass = GST_VIDEO_DECODER_GET_CLASS (decoder);

      if (klass->propose_allocation)
        res = klass->propose_allocation (decoder, query);
      break;
    }
    case GST_QUERY_CAPS:{
      GstCaps *filter, *caps;

      gst_query_parse_caps (query, &filter);
      caps = gst_video_decoder_sink_getcaps (decoder, filter);
      gst_query_set_caps_result (query, caps);
      gst_caps_unref (caps);
      res = TRUE;
      break;
    }
    case GST_QUERY_ACCEPT_CAPS:{
      if (decoder->priv->use_default_pad_acceptcaps) {
        res =
            gst_pad_query_default (GST_VIDEO_DECODER_SINK_PAD (decoder),
            GST_OBJECT_CAST (decoder), query);
      } else {
        GstCaps *caps;
        GstCaps *allowed_caps;
        GstCaps *template_caps;
        gboolean accept;

        gst_query_parse_accept_caps (query, &caps);

        template_caps = gst_pad_get_pad_template_caps (pad);
        accept = gst_caps_is_subset (caps, template_caps);
        gst_caps_unref (template_caps);

        if (accept) {
          allowed_caps =
              gst_pad_query_caps (GST_VIDEO_DECODER_SINK_PAD (decoder), caps);

          accept = gst_caps_can_intersect (caps, allowed_caps);

          gst_caps_unref (allowed_caps);
        }

        gst_query_set_accept_caps_result (query, accept);
        res = TRUE;
      }
      break;
    }
    default:
      res = gst_pad_query_default (pad, GST_OBJECT (decoder), query);
      break;
  }
done:

  return res;
error:
  GST_DEBUG_OBJECT (decoder, "query failed");
  goto done;

}

static gboolean
gst_video_decoder_sink_query (GstPad * pad, GstObject * parent,
    GstQuery * query)
{
  GstVideoDecoder *decoder;
  GstVideoDecoderClass *decoder_class;
  gboolean ret = FALSE;

  decoder = GST_VIDEO_DECODER (parent);
  decoder_class = GST_VIDEO_DECODER_GET_CLASS (decoder);

  GST_DEBUG_OBJECT (decoder, "received query %d, %s", GST_QUERY_TYPE (query),
      GST_QUERY_TYPE_NAME (query));

  if (decoder_class->sink_query)
    ret = decoder_class->sink_query (decoder, query);

  return ret;
}

typedef struct _Timestamp Timestamp;
struct _Timestamp
{
  guint64 offset;
  GstClockTime pts;
  GstClockTime dts;
  GstClockTime duration;
  guint flags;
};

static void
timestamp_free (Timestamp * ts)
{
  g_free (ts);
}

static void
gst_video_decoder_add_buffer_info (GstVideoDecoder * decoder,
    GstBuffer * buffer)
{
  GstVideoDecoderPrivate *priv = decoder->priv;
  Timestamp *ts;

  if (!GST_BUFFER_PTS_IS_VALID (buffer) &&
      !GST_BUFFER_DTS_IS_VALID (buffer) &&
      !GST_BUFFER_DURATION_IS_VALID (buffer) &&
      GST_BUFFER_FLAGS (buffer) == 0) {
    /* Save memory - don't bother storing info
     * for buffers with no distinguishing info */
    return;
  }

  ts = g_new (Timestamp, 1);

  GST_LOG_OBJECT (decoder,
      "adding PTS %" GST_TIME_FORMAT " DTS %" GST_TIME_FORMAT
      " (offset:%" G_GUINT64_FORMAT ")",
      GST_TIME_ARGS (GST_BUFFER_PTS (buffer)),
      GST_TIME_ARGS (GST_BUFFER_DTS (buffer)), priv->input_offset);

  ts->offset = priv->input_offset;
  ts->pts = GST_BUFFER_PTS (buffer);
  ts->dts = GST_BUFFER_DTS (buffer);
  ts->duration = GST_BUFFER_DURATION (buffer);
  ts->flags = GST_BUFFER_FLAGS (buffer);

  g_queue_push_tail (&priv->timestamps, ts);

  if (g_queue_get_length (&priv->timestamps) > 40) {
    GST_WARNING_OBJECT (decoder,
        "decoder timestamp list getting long: %d timestamps,"
        "possible internal leaking?", g_queue_get_length (&priv->timestamps));
  }
}

static void
gst_video_decoder_get_buffer_info_at_offset (GstVideoDecoder *
    decoder, guint64 offset, GstClockTime * pts, GstClockTime * dts,
    GstClockTime * duration, guint * flags)
{
#ifndef GST_DISABLE_GST_DEBUG
  guint64 got_offset = 0;
#endif
  Timestamp *ts;
  GList *g;

  *pts = GST_CLOCK_TIME_NONE;
  *dts = GST_CLOCK_TIME_NONE;
  *duration = GST_CLOCK_TIME_NONE;
  *flags = 0;

  g = decoder->priv->timestamps.head;
  while (g) {
    ts = g->data;
    if (ts->offset <= offset) {
      GList *next = g->next;
#ifndef GST_DISABLE_GST_DEBUG
      got_offset = ts->offset;
#endif
      *pts = ts->pts;
      *dts = ts->dts;
      *duration = ts->duration;
      *flags = ts->flags;
      g_queue_delete_link (&decoder->priv->timestamps, g);
      g = next;
      timestamp_free (ts);
    } else {
      break;
    }
  }

  GST_LOG_OBJECT (decoder,
      "got PTS %" GST_TIME_FORMAT " DTS %" GST_TIME_FORMAT " flags %x @ offs %"
      G_GUINT64_FORMAT " (wanted offset:%" G_GUINT64_FORMAT ")",
      GST_TIME_ARGS (*pts), GST_TIME_ARGS (*dts), *flags, got_offset, offset);
}

static void
gst_video_decoder_clear_queues (GstVideoDecoder * dec)
{
  GstVideoDecoderPrivate *priv = dec->priv;

  g_list_free_full (priv->output_queued,
      (GDestroyNotify) gst_mini_object_unref);
  priv->output_queued = NULL;

  g_list_free_full (priv->gather, (GDestroyNotify) gst_mini_object_unref);
  priv->gather = NULL;
  g_list_free_full (priv->decode, (GDestroyNotify) gst_video_codec_frame_unref);
  priv->decode = NULL;
  g_list_free_full (priv->parse, (GDestroyNotify) gst_mini_object_unref);
  priv->parse = NULL;
  g_list_free_full (priv->parse_gather,
      (GDestroyNotify) gst_video_codec_frame_unref);
  priv->parse_gather = NULL;
  g_queue_clear_full (&priv->frames,
      (GDestroyNotify) gst_video_codec_frame_unref);
}

static void
gst_video_decoder_reset (GstVideoDecoder * decoder, gboolean full,
    gboolean flush_hard)
{
  GstVideoDecoderPrivate *priv = decoder->priv;

  GST_DEBUG_OBJECT (decoder, "reset full %d", full);

  GST_VIDEO_DECODER_STREAM_LOCK (decoder);

  if (full || flush_hard) {
    gst_segment_init (&decoder->input_segment, GST_FORMAT_UNDEFINED);
    gst_segment_init (&decoder->output_segment, GST_FORMAT_UNDEFINED);
    gst_video_decoder_clear_queues (decoder);
    decoder->priv->in_out_segment_sync = TRUE;

    if (priv->current_frame) {
      gst_video_codec_frame_unref (priv->current_frame);
      priv->current_frame = NULL;
    }

    g_list_free_full (priv->current_frame_events,
        (GDestroyNotify) gst_event_unref);
    priv->current_frame_events = NULL;
    g_list_free_full (priv->pending_events, (GDestroyNotify) gst_event_unref);
    priv->pending_events = NULL;

    priv->error_count = 0;
    priv->had_output_data = FALSE;
    priv->had_input_data = FALSE;

    GST_OBJECT_LOCK (decoder);
    priv->earliest_time = GST_CLOCK_TIME_NONE;
    priv->proportion = 0.5;
    priv->decode_flags_override = FALSE;

    priv->request_sync_point_flags = 0;
    priv->request_sync_point_frame_number = REQUEST_SYNC_POINT_UNSET;
    priv->last_force_key_unit_time = GST_CLOCK_TIME_NONE;
    GST_OBJECT_UNLOCK (decoder);
    priv->distance_from_sync = -1;
  }

  if (full) {
    if (priv->input_state)
      gst_video_codec_state_unref (priv->input_state);
    priv->input_state = NULL;
    GST_OBJECT_LOCK (decoder);
    if (priv->output_state)
      gst_video_codec_state_unref (priv->output_state);
    priv->output_state = NULL;

    priv->qos_frame_duration = 0;
    GST_OBJECT_UNLOCK (decoder);

    if (priv->tags)
      gst_tag_list_unref (priv->tags);
    priv->tags = NULL;
    priv->tags_merge_mode = GST_TAG_MERGE_APPEND;
    if (priv->upstream_tags) {
      gst_tag_list_unref (priv->upstream_tags);
      priv->upstream_tags = NULL;
    }
    priv->tags_changed = FALSE;
    priv->reordered_output = FALSE;

    priv->dropped = 0;
    priv->processed = 0;

    priv->posted_latency_msg = FALSE;

    priv->decode_frame_number = 0;

    if (priv->pool) {
      GST_DEBUG_OBJECT (decoder, "deactivate pool %" GST_PTR_FORMAT,
          priv->pool);
      gst_buffer_pool_set_active (priv->pool, FALSE);
      gst_object_unref (priv->pool);
      priv->pool = NULL;
    }

    if (priv->allocator) {
      gst_object_unref (priv->allocator);
      priv->allocator = NULL;
    }
  }

  priv->discont = TRUE;

  priv->last_timestamp_out = GST_CLOCK_TIME_NONE;
  priv->pts_delta = GST_CLOCK_TIME_NONE;

  priv->input_offset = 0;
  priv->frame_offset = 0;
  gst_adapter_clear (priv->input_adapter);
  gst_adapter_clear (priv->output_adapter);
  g_queue_clear_full (&priv->timestamps, (GDestroyNotify) timestamp_free);

  GST_OBJECT_LOCK (decoder);
  priv->bytes_out = 0;
  priv->time = 0;
  GST_OBJECT_UNLOCK (decoder);

#ifndef GST_DISABLE_DEBUG
  priv->last_reset_time = gst_util_get_timestamp ();
#endif

  GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);
}

static GstFlowReturn
gst_video_decoder_chain_forward (GstVideoDecoder * decoder,
    GstBuffer * buf, gboolean at_eos)
{
  GstVideoDecoderPrivate *priv;
  GstVideoDecoderClass *klass;
  GstFlowReturn ret = GST_FLOW_OK;

  klass = GST_VIDEO_DECODER_GET_CLASS (decoder);
  priv = decoder->priv;

  g_return_val_if_fail (priv->packetized || klass->parse, GST_FLOW_ERROR);

  /* Draining on DISCONT is handled in chain_reverse() for reverse playback,
   * and this function would only be called to get everything collected GOP
   * by GOP in the parse_gather list */
  if (decoder->input_segment.rate > 0.0 && GST_BUFFER_IS_DISCONT (buf)
      && (decoder->input_segment.flags & GST_SEEK_FLAG_TRICKMODE_KEY_UNITS))
    ret = gst_video_decoder_drain_out (decoder, FALSE);

  if (priv->current_frame == NULL)
    priv->current_frame = gst_video_decoder_new_frame (decoder);

  if (!priv->packetized)
    gst_video_decoder_add_buffer_info (decoder, buf);

  priv->input_offset += gst_buffer_get_size (buf);

  if (priv->packetized) {
    GstVideoCodecFrame *frame;
    gboolean was_keyframe = FALSE;

    frame = priv->current_frame;

    if (gst_video_decoder_get_subframe_mode (decoder)) {
      frame->abidata.ABI.num_subframes++;
      /* End the frame if the marker flag is set */
      if (!GST_BUFFER_FLAG_IS_SET (buf, GST_VIDEO_BUFFER_FLAG_MARKER)
          && (decoder->input_segment.rate > 0.0))
        priv->current_frame = gst_video_codec_frame_ref (frame);
      else
        priv->current_frame = NULL;
    } else {
      priv->current_frame = frame;
    }

    if (!GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_DELTA_UNIT)) {
      was_keyframe = TRUE;
      GST_DEBUG_OBJECT (decoder, "Marking current_frame as sync point");
      GST_VIDEO_CODEC_FRAME_SET_SYNC_POINT (frame);
    }

    gst_video_decoder_replace_input_buffer (decoder, frame, &buf);

    if (decoder->input_segment.rate < 0.0) {
      priv->parse_gather = g_list_prepend (priv->parse_gather, frame);
      priv->current_frame = NULL;
    } else {
      ret = gst_video_decoder_decode_frame (decoder, frame);
      if (!gst_video_decoder_get_subframe_mode (decoder))
        priv->current_frame = NULL;
    }
    /* If in trick mode and it was a keyframe, drain decoder to avoid extra
     * latency. Only do this for forwards playback as reverse playback handles
     * draining on keyframes in flush_parse(), and would otherwise call back
     * from drain_out() to here causing an infinite loop.
     * Also this function is only called for reverse playback to gather frames
     * GOP by GOP, and does not do any actual decoding. That would be done by
     * flush_decode() */
    if (ret == GST_FLOW_OK && was_keyframe && decoder->input_segment.rate > 0.0
        && (decoder->input_segment.flags & GST_SEEK_FLAG_TRICKMODE_KEY_UNITS))
      ret = gst_video_decoder_drain_out (decoder, FALSE);
  } else {
    gst_adapter_push (priv->input_adapter, buf);

    ret = gst_video_decoder_parse_available (decoder, at_eos, TRUE);
  }

  if (ret == GST_VIDEO_DECODER_FLOW_NEED_DATA)
    return GST_FLOW_OK;

  return ret;
}

static GstFlowReturn
gst_video_decoder_flush_decode (GstVideoDecoder * dec)
{
  GstVideoDecoderPrivate *priv = dec->priv;
  GstFlowReturn res = GST_FLOW_OK;
  GList *walk;
  GstVideoCodecFrame *current_frame = NULL;
  gboolean last_subframe;
  GST_DEBUG_OBJECT (dec, "flushing buffers to decode");

  walk = priv->decode;
  while (walk) {
    GList *next;
    GstVideoCodecFrame *frame = (GstVideoCodecFrame *) (walk->data);
    last_subframe = TRUE;
    /* In subframe mode, we need to get rid of intermediary frames
     * created during the buffer gather stage. That's why that we keep a current
     * frame as the main frame and drop all the frame afterwhile until the end
     * of the subframes batch.
     * */
    if (gst_video_decoder_get_subframe_mode (dec)) {
      if (current_frame == NULL) {
        current_frame = gst_video_codec_frame_ref (frame);
      } else {
        if (current_frame->input_buffer) {
          gst_video_decoder_copy_metas (dec, current_frame,
              current_frame->input_buffer, current_frame->output_buffer);
          gst_buffer_unref (current_frame->input_buffer);
        }
        current_frame->input_buffer = gst_buffer_ref (frame->input_buffer);
        gst_video_codec_frame_unref (frame);
      }
      last_subframe = GST_BUFFER_FLAG_IS_SET (current_frame->input_buffer,
          GST_VIDEO_BUFFER_FLAG_MARKER);
    } else {
      current_frame = frame;
    }

    GST_DEBUG_OBJECT (dec, "decoding frame %p buffer %p, PTS %" GST_TIME_FORMAT
        ", DTS %" GST_TIME_FORMAT, frame, frame->input_buffer,
        GST_TIME_ARGS (GST_BUFFER_PTS (frame->input_buffer)),
        GST_TIME_ARGS (GST_BUFFER_DTS (frame->input_buffer)));

    next = walk->next;

    priv->decode = g_list_delete_link (priv->decode, walk);

    /* decode buffer, resulting data prepended to queue */
    res = gst_video_decoder_decode_frame (dec, current_frame);
    if (res != GST_FLOW_OK)
      break;
    if (!gst_video_decoder_get_subframe_mode (dec)
        || last_subframe)
      current_frame = NULL;
    walk = next;
  }

  return res;
}

/* gst_video_decoder_flush_parse is called from the
 * chain_reverse() function when a buffer containing
 * a DISCONT - indicating that reverse playback
 * looped back to the next data block, and therefore
 * all available data should be fed through the
 * decoder and frames gathered for reversed output
 */
static GstFlowReturn
gst_video_decoder_flush_parse (GstVideoDecoder * dec, gboolean at_eos)
{
  GstVideoDecoderPrivate *priv = dec->priv;
  GstFlowReturn res = GST_FLOW_OK;
  GList *walk;
  GstVideoDecoderClass *decoder_class;

  decoder_class = GST_VIDEO_DECODER_GET_CLASS (dec);

  GST_DEBUG_OBJECT (dec, "flushing buffers to parsing");

  /* Reverse the gather list, and prepend it to the parse list,
   * then flush to parse whatever we can */
  priv->gather = g_list_reverse (priv->gather);
  priv->parse = g_list_concat (priv->gather, priv->parse);
  priv->gather = NULL;

  /* clear buffer and decoder state */
  gst_video_decoder_flush (dec, FALSE);

  walk = priv->parse;
  while (walk) {
    GstBuffer *buf = GST_BUFFER_CAST (walk->data);
    GList *next = walk->next;

    GST_DEBUG_OBJECT (dec, "parsing buffer %p, PTS %" GST_TIME_FORMAT
        ", DTS %" GST_TIME_FORMAT " flags %x", buf,
        GST_TIME_ARGS (GST_BUFFER_PTS (buf)),
        GST_TIME_ARGS (GST_BUFFER_DTS (buf)), GST_BUFFER_FLAGS (buf));

    /* parse buffer, resulting frames prepended to parse_gather queue */
    gst_buffer_ref (buf);
    res = gst_video_decoder_chain_forward (dec, buf, at_eos);

    /* if we generated output, we can discard the buffer, else we
     * keep it in the queue */
    if (priv->parse_gather) {
      GST_DEBUG_OBJECT (dec, "parsed buffer to %p", priv->parse_gather->data);
      priv->parse = g_list_delete_link (priv->parse, walk);
      gst_buffer_unref (buf);
    } else {
      GST_DEBUG_OBJECT (dec, "buffer did not decode, keeping");
    }
    walk = next;
  }

  walk = priv->parse_gather;
  while (walk) {
    GstVideoCodecFrame *frame = (GstVideoCodecFrame *) (walk->data);
    GList *walk2;

    /* this is reverse playback, check if we need to apply some segment
     * to the output before decoding, as during decoding the segment.rate
     * must be used to determine if a buffer should be pushed or added to
     * the output list for reverse pushing.
     *
     * The new segment is not immediately pushed here because we must
     * wait for negotiation to happen before it can be pushed to avoid
     * pushing a segment before caps event. Negotiation only happens
     * when finish_frame is called.
     */
    for (walk2 = frame->events; walk2;) {
      GList *cur = walk2;
      GstEvent *event = walk2->data;

      walk2 = g_list_next (walk2);
      if (GST_EVENT_TYPE (event) <= GST_EVENT_SEGMENT) {

        if (GST_EVENT_TYPE (event) == GST_EVENT_SEGMENT) {
          GstSegment segment;

          GST_DEBUG_OBJECT (dec, "Segment at frame %p %" GST_TIME_FORMAT,
              frame, GST_TIME_ARGS (GST_BUFFER_PTS (frame->input_buffer)));
          gst_event_copy_segment (event, &segment);
          if (segment.format == GST_FORMAT_TIME) {
            dec->output_segment = segment;
            dec->priv->in_out_segment_sync =
                gst_segment_is_equal (&dec->input_segment, &segment);
          }
        }
        dec->priv->pending_events =
            g_list_append (dec->priv->pending_events, event);
        frame->events = g_list_delete_link (frame->events, cur);
      }
    }

    walk = walk->next;
  }

  /* now we can process frames. Start by moving each frame from the parse_gather
   * to the decode list, reverse the order as we go, and stopping when/if we
   * copy a keyframe. */
  GST_DEBUG_OBJECT (dec, "checking parsed frames for a keyframe to decode");
  walk = priv->parse_gather;
  while (walk) {
    GstVideoCodecFrame *frame = (GstVideoCodecFrame *) (walk->data);

    /* remove from the gather list */
    priv->parse_gather = g_list_remove_link (priv->parse_gather, walk);

    /* move it to the front of the decode queue */
    priv->decode = g_list_concat (walk, priv->decode);

    /* if we copied a keyframe, flush and decode the decode queue */
    if (GST_VIDEO_CODEC_FRAME_IS_SYNC_POINT (frame)) {
      GST_DEBUG_OBJECT (dec, "found keyframe %p with PTS %" GST_TIME_FORMAT
          ", DTS %" GST_TIME_FORMAT, frame,
          GST_TIME_ARGS (GST_BUFFER_PTS (frame->input_buffer)),
          GST_TIME_ARGS (GST_BUFFER_DTS (frame->input_buffer)));
      res = gst_video_decoder_flush_decode (dec);
      if (res != GST_FLOW_OK)
        goto done;

      /* We need to tell the subclass to drain now.
       * We prefer the drain vfunc, but for backward-compat
       * we use a finish() vfunc if drain isn't implemented */
      if (decoder_class->drain) {
        GST_DEBUG_OBJECT (dec, "Draining");
        res = decoder_class->drain (dec);
      } else if (decoder_class->finish) {
        GST_FIXME_OBJECT (dec, "Sub-class should implement drain(). "
            "Calling finish() for backwards-compat");
        res = decoder_class->finish (dec);
      }

      if (res != GST_FLOW_OK)
        goto done;

      /* now send queued data downstream */
      walk = priv->output_queued;
      while (walk) {
        GstBuffer *buf = GST_BUFFER_CAST (walk->data);

        priv->output_queued =
            g_list_delete_link (priv->output_queued, priv->output_queued);

        if (G_LIKELY (res == GST_FLOW_OK)) {
          /* avoid stray DISCONT from forward processing,
           * which have no meaning in reverse pushing */
          GST_BUFFER_FLAG_UNSET (buf, GST_BUFFER_FLAG_DISCONT);

          /* Last chance to calculate a timestamp as we loop backwards
           * through the list */
          if (GST_BUFFER_TIMESTAMP (buf) != GST_CLOCK_TIME_NONE)
            priv->last_timestamp_out = GST_BUFFER_TIMESTAMP (buf);
          else if (priv->last_timestamp_out != GST_CLOCK_TIME_NONE &&
              GST_BUFFER_DURATION (buf) != GST_CLOCK_TIME_NONE) {
            GST_BUFFER_TIMESTAMP (buf) =
                priv->last_timestamp_out - GST_BUFFER_DURATION (buf);
            priv->last_timestamp_out = GST_BUFFER_TIMESTAMP (buf);
            GST_LOG_OBJECT (dec,
                "Calculated TS %" GST_TIME_FORMAT " working backwards",
                GST_TIME_ARGS (priv->last_timestamp_out));
          }

          res = gst_video_decoder_clip_and_push_buf (dec, buf);
        } else {
          gst_buffer_unref (buf);
        }

        walk = priv->output_queued;
      }

      /* clear buffer and decoder state again
       * before moving to the previous keyframe */
      gst_video_decoder_flush (dec, FALSE);
    }

    walk = priv->parse_gather;
  }

done:
  return res;
}

static GstFlowReturn
gst_video_decoder_chain_reverse (GstVideoDecoder * dec, GstBuffer * buf)
{
  GstVideoDecoderPrivate *priv = dec->priv;
  GstFlowReturn result = GST_FLOW_OK;

  /* if we have a discont, move buffers to the decode list */
  if (!buf || GST_BUFFER_IS_DISCONT (buf)) {
    GST_DEBUG_OBJECT (dec, "received discont");

    /* parse and decode stuff in the gather and parse queues */
    result = gst_video_decoder_flush_parse (dec, FALSE);
  }

  if (G_LIKELY (buf)) {
    GST_DEBUG_OBJECT (dec, "gathering buffer %p of size %" G_GSIZE_FORMAT ", "
        "PTS %" GST_TIME_FORMAT ", DTS %" GST_TIME_FORMAT ", dur %"
        GST_TIME_FORMAT, buf, gst_buffer_get_size (buf),
        GST_TIME_ARGS (GST_BUFFER_PTS (buf)),
        GST_TIME_ARGS (GST_BUFFER_DTS (buf)),
        GST_TIME_ARGS (GST_BUFFER_DURATION (buf)));

    /* add buffer to gather queue */
    priv->gather = g_list_prepend (priv->gather, buf);
  }

  return result;
}

static GstFlowReturn
gst_video_decoder_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstVideoDecoder *decoder;
  GstFlowReturn ret = GST_FLOW_OK;

  decoder = GST_VIDEO_DECODER (parent);

  if (G_UNLIKELY (!decoder->priv->input_state && decoder->priv->needs_format))
    goto not_negotiated;

  GST_LOG_OBJECT (decoder,
      "chain PTS %" GST_TIME_FORMAT ", DTS %" GST_TIME_FORMAT " duration %"
      GST_TIME_FORMAT " size %" G_GSIZE_FORMAT " flags %x",
      GST_TIME_ARGS (GST_BUFFER_PTS (buf)),
      GST_TIME_ARGS (GST_BUFFER_DTS (buf)),
      GST_TIME_ARGS (GST_BUFFER_DURATION (buf)),
      gst_buffer_get_size (buf), GST_BUFFER_FLAGS (buf));

  GST_VIDEO_DECODER_STREAM_LOCK (decoder);

  /* NOTE:
   * requiring the pad to be negotiated makes it impossible to use
   * oggdemux or filesrc ! decoder */

  if (decoder->input_segment.format == GST_FORMAT_UNDEFINED) {
    GstEvent *event;
    GstSegment *segment = &decoder->input_segment;

    GST_WARNING_OBJECT (decoder,
        "Received buffer without a new-segment. "
        "Assuming timestamps start from 0.");

    gst_segment_init (segment, GST_FORMAT_TIME);

    event = gst_event_new_segment (segment);

    decoder->priv->current_frame_events =
        g_list_prepend (decoder->priv->current_frame_events, event);
  }

  decoder->priv->had_input_data = TRUE;

  if (decoder->input_segment.rate > 0.0)
    ret = gst_video_decoder_chain_forward (decoder, buf, FALSE);
  else
    ret = gst_video_decoder_chain_reverse (decoder, buf);

  GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);
  return ret;

  /* ERRORS */
not_negotiated:
  {
    GST_ELEMENT_ERROR (decoder, CORE, NEGOTIATION, (NULL),
        ("decoder not initialized"));
    gst_buffer_unref (buf);
    return GST_FLOW_NOT_NEGOTIATED;
  }
}

static GstStateChangeReturn
gst_video_decoder_change_state (GstElement * element, GstStateChange transition)
{
  GstVideoDecoder *decoder;
  GstVideoDecoderClass *decoder_class;
  GstStateChangeReturn ret;

  decoder = GST_VIDEO_DECODER (element);
  decoder_class = GST_VIDEO_DECODER_GET_CLASS (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      /* open device/library if needed */
      if (decoder_class->open && !decoder_class->open (decoder))
        goto open_failed;
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      GST_VIDEO_DECODER_STREAM_LOCK (decoder);
      gst_video_decoder_reset (decoder, TRUE, TRUE);
      GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);

      /* Initialize device/library if needed */
      if (decoder_class->start && !decoder_class->start (decoder))
        goto start_failed;
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:{
      gboolean stopped = TRUE;

      if (decoder_class->stop)
        stopped = decoder_class->stop (decoder);

      GST_VIDEO_DECODER_STREAM_LOCK (decoder);
      gst_video_decoder_reset (decoder, TRUE, TRUE);
      GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);

      if (!stopped)
        goto stop_failed;

      break;
    }
    case GST_STATE_CHANGE_READY_TO_NULL:
      /* close device/library if needed */
      if (decoder_class->close && !decoder_class->close (decoder))
        goto close_failed;
      break;
    default:
      break;
  }

  return ret;

  /* Errors */
open_failed:
  {
    GST_ELEMENT_ERROR (decoder, LIBRARY, INIT, (NULL),
        ("Failed to open decoder"));
    return GST_STATE_CHANGE_FAILURE;
  }

start_failed:
  {
    GST_ELEMENT_ERROR (decoder, LIBRARY, INIT, (NULL),
        ("Failed to start decoder"));
    return GST_STATE_CHANGE_FAILURE;
  }

stop_failed:
  {
    GST_ELEMENT_ERROR (decoder, LIBRARY, INIT, (NULL),
        ("Failed to stop decoder"));
    return GST_STATE_CHANGE_FAILURE;
  }

close_failed:
  {
    GST_ELEMENT_ERROR (decoder, LIBRARY, INIT, (NULL),
        ("Failed to close decoder"));
    return GST_STATE_CHANGE_FAILURE;
  }
}

static GstVideoCodecFrame *
gst_video_decoder_new_frame (GstVideoDecoder * decoder)
{
  GstVideoDecoderPrivate *priv = decoder->priv;
  GstVideoCodecFrame *frame;

  frame = g_new0 (GstVideoCodecFrame, 1);

  frame->ref_count = 1;

  GST_VIDEO_DECODER_STREAM_LOCK (decoder);
  frame->system_frame_number = priv->system_frame_number;
  priv->system_frame_number++;
  frame->decode_frame_number = priv->decode_frame_number;
  priv->decode_frame_number++;

  frame->dts = GST_CLOCK_TIME_NONE;
  frame->pts = GST_CLOCK_TIME_NONE;
  frame->duration = GST_CLOCK_TIME_NONE;
  frame->events = priv->current_frame_events;
  priv->current_frame_events = NULL;

  GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);

  GST_LOG_OBJECT (decoder, "Created new frame %p (sfn:%d)",
      frame, frame->system_frame_number);

  return frame;
}

static void
gst_video_decoder_push_event_list (GstVideoDecoder * decoder, GList * events)
{
  GList *l;

  /* events are stored in reverse order */
  for (l = g_list_last (events); l; l = g_list_previous (l)) {
    GST_LOG_OBJECT (decoder, "pushing %s event", GST_EVENT_TYPE_NAME (l->data));
    gst_video_decoder_push_event (decoder, l->data);
  }
  g_list_free (events);
}

static void
gst_video_decoder_prepare_finish_frame (GstVideoDecoder *
    decoder, GstVideoCodecFrame * frame, gboolean dropping)
{
  GstVideoDecoderPrivate *priv = decoder->priv;
  GList *l, *events = NULL;
  gboolean sync, frames_without_dts, frames_without_pts;
  GstClockTime min_dts, min_pts;
  GstVideoCodecFrame *earliest_dts_frame, *earliest_pts_frame;

#ifndef GST_DISABLE_GST_DEBUG
  GST_LOG_OBJECT (decoder, "n %d in %" G_GSIZE_FORMAT " out %" G_GSIZE_FORMAT,
      priv->frames.length,
      gst_adapter_available (priv->input_adapter),
      gst_adapter_available (priv->output_adapter));
#endif

  sync = GST_VIDEO_CODEC_FRAME_IS_SYNC_POINT (frame);

  GST_LOG_OBJECT (decoder,
      "finish frame %p (#%d)(sub=#%d) sync:%d PTS:%" GST_TIME_FORMAT " DTS:%"
      GST_TIME_FORMAT,
      frame, frame->system_frame_number, frame->abidata.ABI.num_subframes,
      sync, GST_TIME_ARGS (frame->pts), GST_TIME_ARGS (frame->dts));

  /* Push all pending events that arrived before this frame */
  for (l = priv->frames.head; l; l = l->next) {
    GstVideoCodecFrame *tmp = l->data;

    if (tmp->events) {
      events = g_list_concat (tmp->events, events);
      tmp->events = NULL;
    }

    if (tmp == frame)
      break;
  }

  if (dropping || !decoder->priv->output_state) {
    /* Push before the next frame that is not dropped */
    decoder->priv->pending_events =
        g_list_concat (events, decoder->priv->pending_events);
  } else {
    gst_video_decoder_push_event_list (decoder, decoder->priv->pending_events);
    decoder->priv->pending_events = NULL;

    gst_video_decoder_push_event_list (decoder, events);
  }

  /* Check if the data should not be displayed. For example altref/invisible
   * frame in vp8. In this case we should not update the timestamps. */
  if (GST_VIDEO_CODEC_FRAME_IS_DECODE_ONLY (frame))
    return;

  /* If the frame is meant to be output but we don't have an output_buffer
   * we have a problem :) */
  if (G_UNLIKELY ((frame->output_buffer == NULL) && !dropping))
    goto no_output_buffer;

  if (frame->duration == GST_CLOCK_TIME_NONE) {
    frame->duration = gst_video_decoder_get_frame_duration (decoder, frame);
    GST_LOG_OBJECT (decoder,
        "Guessing duration %" GST_TIME_FORMAT " for frame...",
        GST_TIME_ARGS (frame->duration));
  }

  /* The following code is to fix issues with PTS and DTS:
   * * Because the input PTS and/or DTS was mis-used (using DTS as PTS, or PTS
   *   as DTS)
   * * Because the input was missing PTS and/or DTS
   *
   * For that, we will collected 3 important information from the frames in
   * flight:
   * * Whether all frames had a valid PTS or a valid DTS
   * * Which frame has the lowest PTS (and its value)
   * * Which frame has the lowest DTS (And its value)
   */
  frames_without_pts = frames_without_dts = FALSE;
  min_dts = min_pts = GST_CLOCK_TIME_NONE;
  earliest_pts_frame = earliest_dts_frame = NULL;

  /* Check what is the earliest PTS and DTS in our pendings frames */
  for (l = priv->frames.head; l; l = l->next) {
    GstVideoCodecFrame *tmp = l->data;

    /* ABI.ts contains DTS */
    if (!GST_CLOCK_TIME_IS_VALID (tmp->abidata.ABI.ts)) {
      frames_without_dts = TRUE;
    } else if (!GST_CLOCK_TIME_IS_VALID (min_dts)
        || tmp->abidata.ABI.ts < min_dts) {
      min_dts = tmp->abidata.ABI.ts;
      earliest_dts_frame = tmp;
    }

    /* ABI.ts2 contains PTS */
    if (!GST_CLOCK_TIME_IS_VALID (tmp->abidata.ABI.ts2)) {
      frames_without_pts = TRUE;
    } else if (!GST_CLOCK_TIME_IS_VALID (min_pts)
        || tmp->abidata.ABI.ts2 < min_pts) {
      min_pts = tmp->abidata.ABI.ts2;
      earliest_pts_frame = tmp;
    }
  }
  /* save dts if needed */
  if (earliest_dts_frame && earliest_dts_frame != frame) {
    earliest_dts_frame->abidata.ABI.ts = frame->abidata.ABI.ts;
  }
  /* save pts if needed */
  if (earliest_pts_frame && earliest_pts_frame != frame) {
    earliest_pts_frame->abidata.ABI.ts2 = frame->abidata.ABI.ts2;
  }

  /* First attempt at recovering missing PTS:
   * * If we figured out the PTS<->DTS delta (from a keyframe)
   * * AND all frames have a valid DTS (i.e. it is not sparsely timestamped
   *   input)
   * * AND we are not dealing with ordering issues
   *
   * We can figure out the pts from the lowest DTS and the PTS<->DTS delta
   */
  if (!priv->reordered_output &&
      !GST_CLOCK_TIME_IS_VALID (frame->pts) && !frames_without_dts &&
      GST_CLOCK_TIME_IS_VALID (priv->pts_delta)) {
    frame->pts = min_dts + priv->pts_delta;
    GST_DEBUG_OBJECT (decoder,
        "no valid PTS, using oldest DTS %" GST_TIME_FORMAT,
        GST_TIME_ARGS (frame->pts));
  }

  /* if we detected reordered output, then PTS are void, however those were
   * obtained; bogus input, subclass etc */
  if (priv->reordered_output && !frames_without_pts) {
    GST_DEBUG_OBJECT (decoder, "invalidating PTS");
    frame->pts = GST_CLOCK_TIME_NONE;
  }

  /* If the frame doesn't have a PTS we can take the earliest PTS from our
   * pending frame list (Only valid if all pending frames have PTS) */
  if (!GST_CLOCK_TIME_IS_VALID (frame->pts) && !frames_without_pts) {
    frame->pts = min_pts;
    GST_DEBUG_OBJECT (decoder,
        "no valid PTS, using oldest PTS %" GST_TIME_FORMAT,
        GST_TIME_ARGS (frame->pts));
  }

  if (frame->pts == GST_CLOCK_TIME_NONE) {
    /* Last ditch timestamp guess: Just add the duration to the previous
     * frame. If it's the first frame, just use the segment start. */
    if (frame->duration != GST_CLOCK_TIME_NONE) {
      if (GST_CLOCK_TIME_IS_VALID (priv->last_timestamp_out))
        frame->pts = priv->last_timestamp_out + frame->duration;
      else if (frame->dts != GST_CLOCK_TIME_NONE) {
        frame->pts = frame->dts;
        GST_LOG_OBJECT (decoder,
            "Setting DTS as PTS %" GST_TIME_FORMAT " for frame...",
            GST_TIME_ARGS (frame->pts));
      } else if (decoder->output_segment.rate > 0.0)
        frame->pts = decoder->output_segment.start;
      GST_INFO_OBJECT (decoder,
          "Guessing PTS=%" GST_TIME_FORMAT " for frame... DTS=%"
          GST_TIME_FORMAT, GST_TIME_ARGS (frame->pts),
          GST_TIME_ARGS (frame->dts));
    } else if (sync && frame->dts != GST_CLOCK_TIME_NONE) {
      frame->pts = frame->dts;
      GST_LOG_OBJECT (decoder,
          "Setting DTS as PTS %" GST_TIME_FORMAT " for frame...",
          GST_TIME_ARGS (frame->pts));
    }
  }

  if (GST_CLOCK_TIME_IS_VALID (priv->last_timestamp_out) &&
      frame->pts < priv->last_timestamp_out) {
    GST_WARNING_OBJECT (decoder,
        "decreasing timestamp (%" GST_TIME_FORMAT " < %" GST_TIME_FORMAT ")",
        GST_TIME_ARGS (frame->pts), GST_TIME_ARGS (priv->last_timestamp_out));
    priv->reordered_output = TRUE;
    /* make it a bit less weird downstream */
    frame->pts = priv->last_timestamp_out;
  }

  if (GST_CLOCK_TIME_IS_VALID (frame->pts))
    priv->last_timestamp_out = frame->pts;

  return;

  /* ERRORS */
no_output_buffer:
  {
    GST_ERROR_OBJECT (decoder, "No buffer to output !");
  }
}

/**
 * gst_video_decoder_release_frame:
 * @dec: a #GstVideoDecoder
 * @frame: (transfer full): the #GstVideoCodecFrame to release
 *
 * Similar to gst_video_decoder_drop_frame(), but simply releases @frame
 * without any processing other than removing it from list of pending frames,
 * after which it is considered finished and released.
 *
 * Since: 1.2.2
 */
void
gst_video_decoder_release_frame (GstVideoDecoder * dec,
    GstVideoCodecFrame * frame)
{
  GList *link;

  /* unref once from the list */
  GST_VIDEO_DECODER_STREAM_LOCK (dec);
  link = g_queue_find (&dec->priv->frames, frame);
  if (link) {
    gst_video_codec_frame_unref (frame);
    g_queue_delete_link (&dec->priv->frames, link);
  }
  if (frame->events) {
    dec->priv->pending_events =
        g_list_concat (frame->events, dec->priv->pending_events);
    frame->events = NULL;
  }
  GST_VIDEO_DECODER_STREAM_UNLOCK (dec);

  /* unref because this function takes ownership */
  gst_video_codec_frame_unref (frame);
}

/* called with STREAM_LOCK */
static void
gst_video_decoder_post_qos_drop (GstVideoDecoder * dec, GstClockTime timestamp)
{
  GstClockTime stream_time, jitter, earliest_time, qostime;
  GstSegment *segment;
  GstMessage *qos_msg;
  gdouble proportion;
  dec->priv->dropped++;

  /* post QoS message */
  GST_OBJECT_LOCK (dec);
  proportion = dec->priv->proportion;
  earliest_time = dec->priv->earliest_time;
  GST_OBJECT_UNLOCK (dec);

  segment = &dec->output_segment;
  if (G_UNLIKELY (segment->format == GST_FORMAT_UNDEFINED))
    segment = &dec->input_segment;
  stream_time =
      gst_segment_to_stream_time (segment, GST_FORMAT_TIME, timestamp);
  qostime = gst_segment_to_running_time (segment, GST_FORMAT_TIME, timestamp);
  jitter = GST_CLOCK_DIFF (qostime, earliest_time);
  qos_msg =
      gst_message_new_qos (GST_OBJECT_CAST (dec), FALSE, qostime, stream_time,
      timestamp, GST_CLOCK_TIME_NONE);
  gst_message_set_qos_values (qos_msg, jitter, proportion, 1000000);
  gst_message_set_qos_stats (qos_msg, GST_FORMAT_BUFFERS,
      dec->priv->processed, dec->priv->dropped);
  gst_element_post_message (GST_ELEMENT_CAST (dec), qos_msg);
}

/**
 * gst_video_decoder_drop_frame:
 * @dec: a #GstVideoDecoder
 * @frame: (transfer full): the #GstVideoCodecFrame to drop
 *
 * Similar to gst_video_decoder_finish_frame(), but drops @frame in any
 * case and posts a QoS message with the frame's details on the bus.
 * In any case, the frame is considered finished and released.
 *
 * Returns: a #GstFlowReturn, usually GST_FLOW_OK.
 */
GstFlowReturn
gst_video_decoder_drop_frame (GstVideoDecoder * dec, GstVideoCodecFrame * frame)
{
  GST_LOG_OBJECT (dec, "drop frame %p", frame);

  if (gst_video_decoder_get_subframe_mode (dec))
    GST_DEBUG_OBJECT (dec, "Drop subframe %d. Must be the last one.",
        frame->abidata.ABI.num_subframes);

  GST_VIDEO_DECODER_STREAM_LOCK (dec);

  gst_video_decoder_prepare_finish_frame (dec, frame, TRUE);

  GST_DEBUG_OBJECT (dec, "dropping frame %" GST_TIME_FORMAT,
      GST_TIME_ARGS (frame->pts));

  gst_video_decoder_post_qos_drop (dec, frame->pts);

  /* now free the frame */
  gst_video_decoder_release_frame (dec, frame);

  /* store that we have valid decoded data */
  dec->priv->had_output_data = TRUE;

  GST_VIDEO_DECODER_STREAM_UNLOCK (dec);

  return GST_FLOW_OK;
}

/**
 * gst_video_decoder_drop_subframe:
 * @dec: a #GstVideoDecoder
 * @frame: (transfer full): the #GstVideoCodecFrame
 *
 * Drops input data.
 * The frame is not considered finished until the whole frame
 * is finished or dropped by the subclass.
 *
 * Returns: a #GstFlowReturn, usually GST_FLOW_OK.
 *
 * Since: 1.20
 */
GstFlowReturn
gst_video_decoder_drop_subframe (GstVideoDecoder * dec,
    GstVideoCodecFrame * frame)
{
  g_return_val_if_fail (gst_video_decoder_get_subframe_mode (dec),
      GST_FLOW_NOT_SUPPORTED);

  GST_LOG_OBJECT (dec, "drop subframe %p num=%d", frame->input_buffer,
      gst_video_decoder_get_input_subframe_index (dec, frame));

  GST_VIDEO_DECODER_STREAM_LOCK (dec);

  gst_video_codec_frame_unref (frame);

  GST_VIDEO_DECODER_STREAM_UNLOCK (dec);

  return GST_FLOW_OK;
}

static gboolean
gst_video_decoder_transform_meta_default (GstVideoDecoder *
    decoder, GstVideoCodecFrame * frame, GstMeta * meta)
{
  const GstMetaInfo *info = meta->info;
  const gchar *const *tags;
  const gchar *const supported_tags[] = {
    GST_META_TAG_VIDEO_STR,
    GST_META_TAG_VIDEO_ORIENTATION_STR,
    GST_META_TAG_VIDEO_SIZE_STR,
    NULL,
  };

  tags = gst_meta_api_type_get_tags (info->api);

  if (!tags)
    return TRUE;

  while (*tags) {
    if (!g_strv_contains (supported_tags, *tags))
      return FALSE;
    tags++;
  }

  return TRUE;
}

typedef struct
{
  GstVideoDecoder *decoder;
  GstVideoCodecFrame *frame;
  GstBuffer *buffer;
} CopyMetaData;

static gboolean
foreach_metadata (GstBuffer * inbuf, GstMeta ** meta, gpointer user_data)
{
  CopyMetaData *data = user_data;
  GstVideoDecoder *decoder = data->decoder;
  GstVideoDecoderClass *klass = GST_VIDEO_DECODER_GET_CLASS (decoder);
  GstVideoCodecFrame *frame = data->frame;
  GstBuffer *buffer = data->buffer;
  const GstMetaInfo *info = (*meta)->info;
  gboolean do_copy = FALSE;

  if (gst_meta_api_type_has_tag (info->api, _gst_meta_tag_memory)
      || gst_meta_api_type_has_tag (info->api, _gst_meta_tag_memory_reference)) {
    /* never call the transform_meta with memory specific metadata */
    GST_DEBUG_OBJECT (decoder, "not copying memory specific metadata %s",
        g_type_name (info->api));
    do_copy = FALSE;
  } else if (klass->transform_meta) {
    do_copy = klass->transform_meta (decoder, frame, *meta);
    GST_DEBUG_OBJECT (decoder, "transformed metadata %s: copy: %d",
        g_type_name (info->api), do_copy);
  }

  /* we only copy metadata when the subclass implemented a transform_meta
   * function and when it returns %TRUE */
  if (do_copy && info->transform_func) {
    GstMetaTransformCopy copy_data = { FALSE, 0, -1 };
    GST_DEBUG_OBJECT (decoder, "copy metadata %s", g_type_name (info->api));
    /* simply copy then */

    info->transform_func (buffer, *meta, inbuf, _gst_meta_transform_copy,
        &copy_data);
  }
  return TRUE;
}

static void
gst_video_decoder_copy_metas (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame, GstBuffer * src_buffer, GstBuffer * dest_buffer)
{
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_GET_CLASS (decoder);

  if (decoder_class->transform_meta) {
    if (G_LIKELY (frame)) {
      CopyMetaData data;

      data.decoder = decoder;
      data.frame = frame;
      data.buffer = dest_buffer;
      gst_buffer_foreach_meta (src_buffer, foreach_metadata, &data);
    } else {
      GST_WARNING_OBJECT (decoder,
          "Can't copy metadata because input frame disappeared");
    }
  }
}

static void
gst_video_decoder_replace_input_buffer (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame, GstBuffer ** dest_buffer)
{
  if (frame->input_buffer) {
    *dest_buffer = gst_buffer_make_writable (*dest_buffer);
    gst_video_decoder_copy_metas (decoder, frame, frame->input_buffer,
        *dest_buffer);
    gst_buffer_unref (frame->input_buffer);
  }

  frame->input_buffer = *dest_buffer;
}

/**
 * gst_video_decoder_finish_frame:
 * @decoder: a #GstVideoDecoder
 * @frame: (transfer full): a decoded #GstVideoCodecFrame
 *
 * @frame should have a valid decoded data buffer, whose metadata fields
 * are then appropriately set according to frame data and pushed downstream.
 * If no output data is provided, @frame is considered skipped.
 * In any case, the frame is considered finished and released.
 *
 * After calling this function the output buffer of the frame is to be
 * considered read-only. This function will also change the metadata
 * of the buffer.
 *
 * Returns: a #GstFlowReturn resulting from sending data downstream
 */
GstFlowReturn
gst_video_decoder_finish_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstVideoDecoderPrivate *priv = decoder->priv;
  GstBuffer *output_buffer;
  gboolean needs_reconfigure = FALSE;

  GST_LOG_OBJECT (decoder, "finish frame %p", frame);

  GST_VIDEO_DECODER_STREAM_LOCK (decoder);

  needs_reconfigure = gst_pad_check_reconfigure (decoder->srcpad);
  if (G_UNLIKELY (priv->output_state_changed || (priv->output_state
              && needs_reconfigure))) {
    if (!gst_video_decoder_negotiate_unlocked (decoder)) {
      gst_pad_mark_reconfigure (decoder->srcpad);
      if (GST_PAD_IS_FLUSHING (decoder->srcpad))
        ret = GST_FLOW_FLUSHING;
      else
        ret = GST_FLOW_NOT_NEGOTIATED;
      goto done;
    }
  }

  gst_video_decoder_prepare_finish_frame (decoder, frame, FALSE);
  priv->processed++;

  if (priv->tags_changed) {
    GstEvent *tags_event;

    tags_event = gst_video_decoder_create_merged_tags_event (decoder);

    if (tags_event != NULL)
      gst_video_decoder_push_event (decoder, tags_event);

    priv->tags_changed = FALSE;
  }

  /* no buffer data means this frame is skipped */
  if (!frame->output_buffer || GST_VIDEO_CODEC_FRAME_IS_DECODE_ONLY (frame)) {
    GST_DEBUG_OBJECT (decoder,
        "skipping frame %" GST_TIME_FORMAT " because not output was produced",
        GST_TIME_ARGS (frame->pts));
    goto done;
  }

  /* Mark output as corrupted if the subclass requested so and we're either
   * still before the sync point after the request, or we don't even know the
   * frame number of the sync point yet (it is 0) */
  GST_OBJECT_LOCK (decoder);
  if (frame->system_frame_number <= priv->request_sync_point_frame_number
      && priv->request_sync_point_frame_number != REQUEST_SYNC_POINT_UNSET) {
    if (priv->request_sync_point_flags &
        GST_VIDEO_DECODER_REQUEST_SYNC_POINT_CORRUPT_OUTPUT) {
      GST_DEBUG_OBJECT (decoder,
          "marking frame %" GST_TIME_FORMAT
          " as corrupted because it is still before the sync point",
          GST_TIME_ARGS (frame->pts));
      GST_VIDEO_CODEC_FRAME_FLAG_SET (frame,
          GST_VIDEO_CODEC_FRAME_FLAG_CORRUPTED);
    }
  } else {
    /* Reset to -1 to mark it as unset now that we've reached the frame */
    priv->request_sync_point_frame_number = REQUEST_SYNC_POINT_UNSET;
  }
  GST_OBJECT_UNLOCK (decoder);

  if (priv->discard_corrupted_frames
      && (GST_VIDEO_CODEC_FRAME_FLAG_IS_SET (frame,
              GST_VIDEO_CODEC_FRAME_FLAG_CORRUPTED)
          || GST_BUFFER_FLAG_IS_SET (frame->output_buffer,
              GST_BUFFER_FLAG_CORRUPTED))) {
    GST_DEBUG_OBJECT (decoder,
        "skipping frame %" GST_TIME_FORMAT " because it is corrupted",
        GST_TIME_ARGS (frame->pts));
    goto done;
  }

  /* We need a writable buffer for the metadata changes below */
  output_buffer = frame->output_buffer =
      gst_buffer_make_writable (frame->output_buffer);

  GST_BUFFER_FLAG_UNSET (output_buffer, GST_BUFFER_FLAG_DELTA_UNIT);

  GST_BUFFER_PTS (output_buffer) = frame->pts;
  GST_BUFFER_DTS (output_buffer) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_DURATION (output_buffer) = frame->duration;

  GST_BUFFER_OFFSET (output_buffer) = GST_BUFFER_OFFSET_NONE;
  GST_BUFFER_OFFSET_END (output_buffer) = GST_BUFFER_OFFSET_NONE;

  if (priv->discont) {
    GST_BUFFER_FLAG_SET (output_buffer, GST_BUFFER_FLAG_DISCONT);
  }

  if (GST_VIDEO_CODEC_FRAME_FLAG_IS_SET (frame,
          GST_VIDEO_CODEC_FRAME_FLAG_CORRUPTED)) {
    GST_DEBUG_OBJECT (decoder,
        "marking frame %" GST_TIME_FORMAT " as corrupted",
        GST_TIME_ARGS (frame->pts));
    GST_BUFFER_FLAG_SET (output_buffer, GST_BUFFER_FLAG_CORRUPTED);
  }

  gst_video_decoder_copy_metas (decoder, frame, frame->input_buffer,
      frame->output_buffer);

  /* Get an additional ref to the buffer, which is going to be pushed
   * downstream, the original ref is owned by the frame
   */
  output_buffer = gst_buffer_ref (output_buffer);

  /* Release frame so the buffer is writable when we push it downstream
   * if possible, i.e. if the subclass does not hold additional references
   * to the frame
   */
  gst_video_decoder_release_frame (decoder, frame);
  frame = NULL;

  if (decoder->output_segment.rate < 0.0
      && !(decoder->output_segment.flags & GST_SEEK_FLAG_TRICKMODE_KEY_UNITS)) {
    GST_LOG_OBJECT (decoder, "queued frame");
    priv->output_queued = g_list_prepend (priv->output_queued, output_buffer);
  } else {
    ret = gst_video_decoder_clip_and_push_buf (decoder, output_buffer);
  }

done:
  if (frame)
    gst_video_decoder_release_frame (decoder, frame);
  GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);
  return ret;
}

/**
 * gst_video_decoder_finish_subframe:
 * @decoder: a #GstVideoDecoder
 * @frame: (transfer full): the #GstVideoCodecFrame
 *
 * Indicate that a subframe has been finished to be decoded
 * by the subclass. This method should be called for all subframes
 * except the last subframe where @gst_video_decoder_finish_frame
 * should be called instead.
 *
 * Returns: a #GstFlowReturn, usually GST_FLOW_OK.
 *
 * Since: 1.20
 */
GstFlowReturn
gst_video_decoder_finish_subframe (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame)
{
  g_return_val_if_fail (gst_video_decoder_get_subframe_mode (decoder),
      GST_FLOW_NOT_SUPPORTED);

  GST_LOG_OBJECT (decoder, "finish subframe %p num=%d", frame->input_buffer,
      gst_video_decoder_get_input_subframe_index (decoder, frame));

  GST_VIDEO_DECODER_STREAM_LOCK (decoder);
  frame->abidata.ABI.subframes_processed++;
  gst_video_codec_frame_unref (frame);

  GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);

  return GST_FLOW_OK;
}

/* With stream lock, takes the frame reference */
static GstFlowReturn
gst_video_decoder_clip_and_push_buf (GstVideoDecoder * decoder, GstBuffer * buf)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstVideoDecoderPrivate *priv = decoder->priv;
  guint64 start, stop;
  guint64 cstart, cstop;
  GstSegment *segment;
  GstClockTime duration;

  /* Check for clipping */
  start = GST_BUFFER_PTS (buf);
  duration = GST_BUFFER_DURATION (buf);

  /* store that we have valid decoded data */
  priv->had_output_data = TRUE;

  stop = GST_CLOCK_TIME_NONE;

  if (GST_CLOCK_TIME_IS_VALID (start) && GST_CLOCK_TIME_IS_VALID (duration)) {
    stop = start + duration;
  } else if (GST_CLOCK_TIME_IS_VALID (start)
      && !GST_CLOCK_TIME_IS_VALID (duration)) {
    /* If we don't clip away buffers that far before the segment we
     * can cause the pipeline to lockup. This can happen if audio is
     * properly clipped, and thus the audio sink does not preroll yet
     * but the video sink prerolls because we already outputted a
     * buffer here... and then queues run full.
     *
     * In the worst case we will clip one buffer too many here now if no
     * framerate is given, no buffer duration is given and the actual
     * framerate is lower than 25fps */
    stop = start + 40 * GST_MSECOND;
  }

  segment = &decoder->output_segment;
  if (gst_segment_clip (segment, GST_FORMAT_TIME, start, stop, &cstart, &cstop)) {
    GST_BUFFER_PTS (buf) = cstart;

    if (stop != GST_CLOCK_TIME_NONE && GST_CLOCK_TIME_IS_VALID (duration))
      GST_BUFFER_DURATION (buf) = cstop - cstart;

    GST_LOG_OBJECT (decoder,
        "accepting buffer inside segment: %" GST_TIME_FORMAT " %"
        GST_TIME_FORMAT " seg %" GST_TIME_FORMAT " to %" GST_TIME_FORMAT
        " time %" GST_TIME_FORMAT,
        GST_TIME_ARGS (cstart),
        GST_TIME_ARGS (cstop),
        GST_TIME_ARGS (segment->start), GST_TIME_ARGS (segment->stop),
        GST_TIME_ARGS (segment->time));
  } else {
    GST_LOG_OBJECT (decoder,
        "dropping buffer outside segment: %" GST_TIME_FORMAT
        " %" GST_TIME_FORMAT
        " seg %" GST_TIME_FORMAT " to %" GST_TIME_FORMAT
        " time %" GST_TIME_FORMAT,
        GST_TIME_ARGS (start), GST_TIME_ARGS (stop),
        GST_TIME_ARGS (segment->start),
        GST_TIME_ARGS (segment->stop), GST_TIME_ARGS (segment->time));
    /* only check and return EOS if upstream still
     * in the same segment and interested as such */
    if (decoder->priv->in_out_segment_sync) {
      if (segment->rate >= 0) {
        if (GST_BUFFER_PTS (buf) >= segment->stop)
          ret = GST_FLOW_EOS;
      } else if (GST_BUFFER_PTS (buf) < segment->start) {
        ret = GST_FLOW_EOS;
      }
    }
    gst_buffer_unref (buf);
    goto done;
  }

  /* Is buffer too late (QoS) ? */
  if (priv->do_qos && GST_CLOCK_TIME_IS_VALID (priv->earliest_time)
      && GST_CLOCK_TIME_IS_VALID (cstart)) {
    GstClockTime deadline =
        gst_segment_to_running_time (segment, GST_FORMAT_TIME, cstart);
    if (GST_CLOCK_TIME_IS_VALID (deadline) && deadline < priv->earliest_time) {
      GST_WARNING_OBJECT (decoder,
          "Dropping frame due to QoS. start:%" GST_TIME_FORMAT " deadline:%"
          GST_TIME_FORMAT " earliest_time:%" GST_TIME_FORMAT,
          GST_TIME_ARGS (start), GST_TIME_ARGS (deadline),
          GST_TIME_ARGS (priv->earliest_time));
      gst_video_decoder_post_qos_drop (decoder, cstart);
      gst_buffer_unref (buf);
      priv->discont = TRUE;
      goto done;
    }
  }

  /* Set DISCONT flag here ! */

  if (priv->discont) {
    GST_DEBUG_OBJECT (decoder, "Setting discont on output buffer");
    GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_DISCONT);
    priv->discont = FALSE;
  }

  /* update rate estimate */
  GST_OBJECT_LOCK (decoder);
  priv->bytes_out += gst_buffer_get_size (buf);
  if (GST_CLOCK_TIME_IS_VALID (duration)) {
    priv->time += duration;
  } else {
    /* FIXME : Use difference between current and previous outgoing
     * timestamp, and relate to difference between current and previous
     * bytes */
    /* better none than nothing valid */
    priv->time = GST_CLOCK_TIME_NONE;
  }
  GST_OBJECT_UNLOCK (decoder);

  GST_DEBUG_OBJECT (decoder, "pushing buffer %p of size %" G_GSIZE_FORMAT ", "
      "PTS %" GST_TIME_FORMAT ", dur %" GST_TIME_FORMAT, buf,
      gst_buffer_get_size (buf),
      GST_TIME_ARGS (GST_BUFFER_PTS (buf)),
      GST_TIME_ARGS (GST_BUFFER_DURATION (buf)));

  /* we got data, so note things are looking up again, reduce
   * the error count, if there is one */
  if (G_UNLIKELY (priv->error_count))
    priv->error_count = 0;

#ifndef GST_DISABLE_DEBUG
  if (G_UNLIKELY (priv->last_reset_time != GST_CLOCK_TIME_NONE)) {
    GstClockTime elapsed = gst_util_get_timestamp () - priv->last_reset_time;

    /* First buffer since reset, report how long we took */
    GST_INFO_OBJECT (decoder, "First buffer since flush took %" GST_TIME_FORMAT
        " to produce", GST_TIME_ARGS (elapsed));
    priv->last_reset_time = GST_CLOCK_TIME_NONE;
  }
#endif

  /* release STREAM_LOCK not to block upstream
   * while pushing buffer downstream */
  GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);
  ret = gst_pad_push (decoder->srcpad, buf);
  GST_VIDEO_DECODER_STREAM_LOCK (decoder);

done:
  return ret;
}

/**
 * gst_video_decoder_add_to_frame:
 * @decoder: a #GstVideoDecoder
 * @n_bytes: the number of bytes to add
 *
 * Removes next @n_bytes of input data and adds it to currently parsed frame.
 */
void
gst_video_decoder_add_to_frame (GstVideoDecoder * decoder, int n_bytes)
{
  GstVideoDecoderPrivate *priv = decoder->priv;
  GstBuffer *buf;

  GST_LOG_OBJECT (decoder, "add %d bytes to frame", n_bytes);

  if (n_bytes == 0)
    return;

  GST_VIDEO_DECODER_STREAM_LOCK (decoder);
  if (gst_adapter_available (priv->output_adapter) == 0) {
    priv->frame_offset =
        priv->input_offset - gst_adapter_available (priv->input_adapter);
  }
  buf = gst_adapter_take_buffer (priv->input_adapter, n_bytes);

  gst_adapter_push (priv->output_adapter, buf);
  GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);
}

/**
 * gst_video_decoder_get_pending_frame_size:
 * @decoder: a #GstVideoDecoder
 *
 * Returns the number of bytes previously added to the current frame
 * by calling gst_video_decoder_add_to_frame().
 *
 * Returns: The number of bytes pending for the current frame
 *
 * Since: 1.4
 */
gsize
gst_video_decoder_get_pending_frame_size (GstVideoDecoder * decoder)
{
  GstVideoDecoderPrivate *priv = decoder->priv;
  gsize ret;

  GST_VIDEO_DECODER_STREAM_LOCK (decoder);
  ret = gst_adapter_available (priv->output_adapter);
  GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);

  GST_LOG_OBJECT (decoder, "Current pending frame has %" G_GSIZE_FORMAT "bytes",
      ret);

  return ret;
}

static guint64
gst_video_decoder_get_frame_duration (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame)
{
  GstVideoCodecState *state = decoder->priv->output_state;

  /* it's possible that we don't have a state yet when we are dropping the
   * initial buffers */
  if (state == NULL)
    return GST_CLOCK_TIME_NONE;

  if (state->info.fps_d == 0 || state->info.fps_n == 0) {
    return GST_CLOCK_TIME_NONE;
  }

  /* FIXME: For interlaced frames this needs to take into account
   * the number of valid fields in the frame
   */

  return gst_util_uint64_scale (GST_SECOND, state->info.fps_d,
      state->info.fps_n);
}

/**
 * gst_video_decoder_have_frame:
 * @decoder: a #GstVideoDecoder
 *
 * Gathers all data collected for currently parsed frame, gathers corresponding
 * metadata and passes it along for further processing, i.e. @handle_frame.
 *
 * Returns: a #GstFlowReturn
 */
GstFlowReturn
gst_video_decoder_have_frame (GstVideoDecoder * decoder)
{
  GstVideoDecoderPrivate *priv = decoder->priv;
  GstBuffer *buffer;
  int n_available;
  GstClockTime pts, dts, duration;
  guint flags;
  GstFlowReturn ret = GST_FLOW_OK;

  GST_LOG_OBJECT (decoder, "have_frame at offset %" G_GUINT64_FORMAT,
      priv->frame_offset);

  GST_VIDEO_DECODER_STREAM_LOCK (decoder);

  n_available = gst_adapter_available (priv->output_adapter);
  if (n_available) {
    buffer = gst_adapter_take_buffer (priv->output_adapter, n_available);
  } else {
    buffer = gst_buffer_new_and_alloc (0);
  }

  gst_video_decoder_replace_input_buffer (decoder, priv->current_frame,
      &buffer);

  gst_video_decoder_get_buffer_info_at_offset (decoder,
      priv->frame_offset, &pts, &dts, &duration, &flags);

  GST_BUFFER_PTS (buffer) = pts;
  GST_BUFFER_DTS (buffer) = dts;
  GST_BUFFER_DURATION (buffer) = duration;
  GST_BUFFER_FLAGS (buffer) = flags;

  GST_LOG_OBJECT (decoder, "collected frame size %d, "
      "PTS %" GST_TIME_FORMAT ", DTS %" GST_TIME_FORMAT ", dur %"
      GST_TIME_FORMAT, n_available, GST_TIME_ARGS (pts), GST_TIME_ARGS (dts),
      GST_TIME_ARGS (duration));

  if (!GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT)) {
    GST_DEBUG_OBJECT (decoder, "Marking as sync point");
    GST_VIDEO_CODEC_FRAME_SET_SYNC_POINT (priv->current_frame);
  }

  if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_CORRUPTED)) {
    GST_DEBUG_OBJECT (decoder, "Marking as corrupted");
    GST_VIDEO_CODEC_FRAME_FLAG_SET (priv->current_frame,
        GST_VIDEO_CODEC_FRAME_FLAG_CORRUPTED);
  }

  /* In reverse playback, just capture and queue frames for later processing */
  if (decoder->input_segment.rate < 0.0) {
    priv->parse_gather =
        g_list_prepend (priv->parse_gather, priv->current_frame);
    priv->current_frame = NULL;
  } else {
    GstVideoCodecFrame *frame = priv->current_frame;

    /* In subframe mode, we keep a ref for ourselves
     * as this frame will be kept during the data collection
     * in parsed mode. The frame reference will be released by
     * finish_(sub)frame or drop_(sub)frame.*/
    if (gst_video_decoder_get_subframe_mode (decoder)) {
      frame->abidata.ABI.num_subframes++;
      gst_video_codec_frame_ref (priv->current_frame);
    } else {
      priv->current_frame = NULL;
    }

    /* Decode the frame, which gives away our ref */
    ret = gst_video_decoder_decode_frame (decoder, frame);
  }

  GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);

  return ret;
}

/* Pass the frame in priv->current_frame through the
 * handle_frame() callback for decoding and passing to gvd_finish_frame(),
 * or dropping by passing to gvd_drop_frame() */
static GstFlowReturn
gst_video_decoder_decode_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame)
{
  GstVideoDecoderPrivate *priv = decoder->priv;
  GstVideoDecoderClass *decoder_class;
  GstFlowReturn ret = GST_FLOW_OK;

  decoder_class = GST_VIDEO_DECODER_GET_CLASS (decoder);

  /* FIXME : This should only have to be checked once (either the subclass has an
   * implementation, or it doesn't) */
  g_return_val_if_fail (decoder_class->handle_frame != NULL, GST_FLOW_ERROR);
  g_return_val_if_fail (frame != NULL, GST_FLOW_ERROR);

  frame->pts = GST_BUFFER_PTS (frame->input_buffer);
  frame->dts = GST_BUFFER_DTS (frame->input_buffer);
  frame->duration = GST_BUFFER_DURATION (frame->input_buffer);
  frame->deadline =
      gst_segment_to_running_time (&decoder->input_segment, GST_FORMAT_TIME,
      frame->pts);

  /* For keyframes, PTS = DTS + constant_offset, usually 0 to 3 frame
   * durations. */
  /* FIXME upstream can be quite wrong about the keyframe aspect,
   * so we could be going off here as well,
   * maybe let subclass decide if it really is/was a keyframe */
  if (GST_VIDEO_CODEC_FRAME_IS_SYNC_POINT (frame)) {
    priv->distance_from_sync = 0;

    GST_OBJECT_LOCK (decoder);
    priv->request_sync_point_flags &=
        ~GST_VIDEO_DECODER_REQUEST_SYNC_POINT_DISCARD_INPUT;
    if (priv->request_sync_point_frame_number == REQUEST_SYNC_POINT_PENDING)
      priv->request_sync_point_frame_number = frame->system_frame_number;
    GST_OBJECT_UNLOCK (decoder);

    if (GST_CLOCK_TIME_IS_VALID (frame->pts)
        && GST_CLOCK_TIME_IS_VALID (frame->dts)) {
      /* just in case they are not equal as might ideally be,
       * e.g. quicktime has a (positive) delta approach */
      priv->pts_delta = frame->pts - frame->dts;
      GST_DEBUG_OBJECT (decoder, "PTS delta %d ms",
          (gint) (priv->pts_delta / GST_MSECOND));
    }
  } else {
    if (priv->distance_from_sync == -1 && priv->automatic_request_sync_points) {
      GST_DEBUG_OBJECT (decoder,
          "Didn't receive a keyframe yet, requesting sync point");
      gst_video_decoder_request_sync_point (decoder, frame,
          priv->automatic_request_sync_point_flags);
    }

    GST_OBJECT_LOCK (decoder);
    if ((priv->needs_sync_point && priv->distance_from_sync == -1)
        || (priv->request_sync_point_flags &
            GST_VIDEO_DECODER_REQUEST_SYNC_POINT_DISCARD_INPUT)) {
      GST_WARNING_OBJECT (decoder,
          "Subclass requires a sync point but we didn't receive one yet, discarding input");
      GST_OBJECT_UNLOCK (decoder);
      if (priv->automatic_request_sync_points) {
        gst_video_decoder_request_sync_point (decoder, frame,
            priv->automatic_request_sync_point_flags);
      }
      gst_video_decoder_release_frame (decoder, frame);
      return GST_FLOW_OK;
    }
    GST_OBJECT_UNLOCK (decoder);

    priv->distance_from_sync++;
  }

  frame->distance_from_sync = priv->distance_from_sync;

  if (!gst_video_decoder_get_subframe_mode (decoder)
      || frame->abidata.ABI.num_subframes == 1) {
    frame->abidata.ABI.ts = frame->dts;
    frame->abidata.ABI.ts2 = frame->pts;
  }

  GST_LOG_OBJECT (decoder,
      "frame %p PTS %" GST_TIME_FORMAT ", DTS %" GST_TIME_FORMAT ", dist %d",
      frame, GST_TIME_ARGS (frame->pts), GST_TIME_ARGS (frame->dts),
      frame->distance_from_sync);
  /* FIXME: suboptimal way to add a unique frame to the list, in case of subframe mode. */
  if (!g_queue_find (&priv->frames, frame)) {
    g_queue_push_tail (&priv->frames, gst_video_codec_frame_ref (frame));
  } else {
    GST_LOG_OBJECT (decoder,
        "Do not add an existing frame used to decode subframes");
  }

  if (priv->frames.length > 10) {
    GST_DEBUG_OBJECT (decoder, "decoder frame list getting long: %d frames,"
        "possible internal leaking?", priv->frames.length);
  }

  /* do something with frame */
  ret = decoder_class->handle_frame (decoder, frame);
  if (ret != GST_FLOW_OK)
    GST_DEBUG_OBJECT (decoder, "flow error %s", gst_flow_get_name (ret));

  /* the frame has either been added to parse_gather or sent to
     handle frame so there is no need to unref it */
  return ret;
}


/**
 * gst_video_decoder_get_output_state:
 * @decoder: a #GstVideoDecoder
 *
 * Get the #GstVideoCodecState currently describing the output stream.
 *
 * Returns: (transfer full) (nullable): #GstVideoCodecState describing format of video data.
 */
GstVideoCodecState *
gst_video_decoder_get_output_state (GstVideoDecoder * decoder)
{
  GstVideoCodecState *state = NULL;

  GST_OBJECT_LOCK (decoder);
  if (decoder->priv->output_state)
    state = gst_video_codec_state_ref (decoder->priv->output_state);
  GST_OBJECT_UNLOCK (decoder);

  return state;
}

static GstVideoCodecState *
_set_interlaced_output_state (GstVideoDecoder * decoder,
    GstVideoFormat fmt, GstVideoInterlaceMode interlace_mode, guint width,
    guint height, GstVideoCodecState * reference, gboolean copy_interlace_mode)
{
  GstVideoDecoderPrivate *priv = decoder->priv;
  GstVideoCodecState *state;

  g_assert ((copy_interlace_mode
          && interlace_mode == GST_VIDEO_INTERLACE_MODE_PROGRESSIVE)
      || !copy_interlace_mode);

  GST_DEBUG_OBJECT (decoder,
      "fmt:%d, width:%d, height:%d, interlace-mode: %s, reference:%p", fmt,
      width, height, gst_video_interlace_mode_to_string (interlace_mode),
      reference);

  /* Create the new output state */
  state =
      _new_output_state (fmt, interlace_mode, width, height, reference,
      copy_interlace_mode);
  if (!state)
    return NULL;

  GST_VIDEO_DECODER_STREAM_LOCK (decoder);

  GST_OBJECT_LOCK (decoder);
  /* Replace existing output state by new one */
  if (priv->output_state)
    gst_video_codec_state_unref (priv->output_state);
  priv->output_state = gst_video_codec_state_ref (state);

  if (priv->output_state != NULL && priv->output_state->info.fps_n > 0) {
    priv->qos_frame_duration =
        gst_util_uint64_scale (GST_SECOND, priv->output_state->info.fps_d,
        priv->output_state->info.fps_n);
  } else {
    priv->qos_frame_duration = 0;
  }
  priv->output_state_changed = TRUE;
  GST_OBJECT_UNLOCK (decoder);

  GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);

  return state;
}

/**
 * gst_video_decoder_set_output_state:
 * @decoder: a #GstVideoDecoder
 * @fmt: a #GstVideoFormat
 * @width: The width in pixels
 * @height: The height in pixels
 * @reference: (nullable) (transfer none): An optional reference #GstVideoCodecState
 *
 * Creates a new #GstVideoCodecState with the specified @fmt, @width and @height
 * as the output state for the decoder.
 * Any previously set output state on @decoder will be replaced by the newly
 * created one.
 *
 * If the subclass wishes to copy over existing fields (like pixel aspec ratio,
 * or framerate) from an existing #GstVideoCodecState, it can be provided as a
 * @reference.
 *
 * If the subclass wishes to override some fields from the output state (like
 * pixel-aspect-ratio or framerate) it can do so on the returned #GstVideoCodecState.
 *
 * The new output state will only take effect (set on pads and buffers) starting
 * from the next call to #gst_video_decoder_finish_frame().
 *
 * Returns: (transfer full) (nullable): the newly configured output state.
 */
GstVideoCodecState *
gst_video_decoder_set_output_state (GstVideoDecoder * decoder,
    GstVideoFormat fmt, guint width, guint height,
    GstVideoCodecState * reference)
{
  return _set_interlaced_output_state (decoder, fmt,
      GST_VIDEO_INTERLACE_MODE_PROGRESSIVE, width, height, reference, TRUE);
}

/**
 * gst_video_decoder_set_interlaced_output_state:
 * @decoder: a #GstVideoDecoder
 * @fmt: a #GstVideoFormat
 * @width: The width in pixels
 * @height: The height in pixels
 * @interlace_mode: A #GstVideoInterlaceMode
 * @reference: (nullable) (transfer none): An optional reference #GstVideoCodecState
 *
 * Same as #gst_video_decoder_set_output_state() but also allows you to also set
 * the interlacing mode.
 *
 * Returns: (transfer full) (nullable): the newly configured output state.
 *
 * Since: 1.16.
 */
GstVideoCodecState *
gst_video_decoder_set_interlaced_output_state (GstVideoDecoder * decoder,
    GstVideoFormat fmt, GstVideoInterlaceMode interlace_mode, guint width,
    guint height, GstVideoCodecState * reference)
{
  return _set_interlaced_output_state (decoder, fmt, interlace_mode, width,
      height, reference, FALSE);
}


/**
 * gst_video_decoder_get_oldest_frame:
 * @decoder: a #GstVideoDecoder
 *
 * Get the oldest pending unfinished #GstVideoCodecFrame
 *
 * Returns: (transfer full) (nullable): oldest pending unfinished #GstVideoCodecFrame.
 */
GstVideoCodecFrame *
gst_video_decoder_get_oldest_frame (GstVideoDecoder * decoder)
{
  GstVideoCodecFrame *frame = NULL;

  GST_VIDEO_DECODER_STREAM_LOCK (decoder);
  if (decoder->priv->frames.head)
    frame = gst_video_codec_frame_ref (decoder->priv->frames.head->data);
  GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);

  return (GstVideoCodecFrame *) frame;
}

/**
 * gst_video_decoder_get_frame:
 * @decoder: a #GstVideoDecoder
 * @frame_number: system_frame_number of a frame
 *
 * Get a pending unfinished #GstVideoCodecFrame
 *
 * Returns: (transfer full) (nullable): pending unfinished #GstVideoCodecFrame identified by @frame_number.
 */
GstVideoCodecFrame *
gst_video_decoder_get_frame (GstVideoDecoder * decoder, int frame_number)
{
  GList *g;
  GstVideoCodecFrame *frame = NULL;

  GST_DEBUG_OBJECT (decoder, "frame_number : %d", frame_number);

  GST_VIDEO_DECODER_STREAM_LOCK (decoder);
  for (g = decoder->priv->frames.head; g; g = g->next) {
    GstVideoCodecFrame *tmp = g->data;

    if (tmp->system_frame_number == frame_number) {
      frame = gst_video_codec_frame_ref (tmp);
      break;
    }
  }
  GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);

  return frame;
}

/**
 * gst_video_decoder_get_frames:
 * @decoder: a #GstVideoDecoder
 *
 * Get all pending unfinished #GstVideoCodecFrame
 *
 * Returns: (transfer full) (element-type GstVideoCodecFrame): pending unfinished #GstVideoCodecFrame.
 */
GList *
gst_video_decoder_get_frames (GstVideoDecoder * decoder)
{
  GList *frames;

  GST_VIDEO_DECODER_STREAM_LOCK (decoder);
  frames =
      g_list_copy_deep (decoder->priv->frames.head,
      (GCopyFunc) gst_video_codec_frame_ref, NULL);
  GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);

  return frames;
}

static gboolean
gst_video_decoder_decide_allocation_default (GstVideoDecoder * decoder,
    GstQuery * query)
{
  GstCaps *outcaps = NULL;
  GstBufferPool *pool = NULL;
  guint size, min, max;
  GstAllocator *allocator = NULL;
  GstAllocationParams params;
  GstStructure *config;
  gboolean update_pool, update_allocator;
  GstVideoInfo vinfo;

  gst_query_parse_allocation (query, &outcaps, NULL);
  gst_video_info_init (&vinfo);
  if (outcaps)
    gst_video_info_from_caps (&vinfo, outcaps);

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

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);
    size = MAX (size, vinfo.size);
    update_pool = TRUE;
  } else {
    pool = NULL;
    size = vinfo.size;
    min = max = 0;

    update_pool = FALSE;
  }

  if (pool == NULL) {
    /* no pool, we can make our own */
    GST_DEBUG_OBJECT (decoder, "no pool, making new pool");
    pool = gst_video_buffer_pool_new ();
  }

  /* now configure */
  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config, outcaps, size, min, max);
  gst_buffer_pool_config_set_allocator (config, allocator, &params);

  GST_DEBUG_OBJECT (decoder,
      "setting config %" GST_PTR_FORMAT " in pool %" GST_PTR_FORMAT, config,
      pool);
  if (!gst_buffer_pool_set_config (pool, config)) {
    config = gst_buffer_pool_get_config (pool);

    /* If change are not acceptable, fallback to generic pool */
    if (!gst_buffer_pool_config_validate_params (config, outcaps, size, min,
            max)) {
      GST_DEBUG_OBJECT (decoder, "unsupported pool, making new pool");

      gst_object_unref (pool);
      pool = gst_video_buffer_pool_new ();
      gst_buffer_pool_config_set_params (config, outcaps, size, min, max);
      gst_buffer_pool_config_set_allocator (config, allocator, &params);
    }

    if (!gst_buffer_pool_set_config (pool, config))
      goto config_failed;
  }

  if (update_allocator)
    gst_query_set_nth_allocation_param (query, 0, allocator, &params);
  else
    gst_query_add_allocation_param (query, allocator, &params);
  if (allocator)
    gst_object_unref (allocator);

  if (update_pool)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);

  if (pool)
    gst_object_unref (pool);

  return TRUE;

config_failed:
  if (allocator)
    gst_object_unref (allocator);
  if (pool)
    gst_object_unref (pool);
  GST_ELEMENT_ERROR (decoder, RESOURCE, SETTINGS,
      ("Failed to configure the buffer pool"),
      ("Configuration is most likely invalid, please report this issue."));
  return FALSE;
}

static gboolean
gst_video_decoder_propose_allocation_default (GstVideoDecoder * decoder,
    GstQuery * query)
{
  return TRUE;
}

static gboolean
gst_video_decoder_negotiate_pool (GstVideoDecoder * decoder, GstCaps * caps)
{
  GstVideoDecoderClass *klass;
  GstQuery *query = NULL;
  GstBufferPool *pool = NULL;
  GstAllocator *allocator;
  GstAllocationParams params;
  gboolean ret = TRUE;

  klass = GST_VIDEO_DECODER_GET_CLASS (decoder);

  query = gst_query_new_allocation (caps, TRUE);

  GST_DEBUG_OBJECT (decoder, "do query ALLOCATION");

  if (!gst_pad_peer_query (decoder->srcpad, query)) {
    GST_DEBUG_OBJECT (decoder, "didn't get downstream ALLOCATION hints");
  }

  g_assert (klass->decide_allocation != NULL);
  ret = klass->decide_allocation (decoder, query);

  GST_DEBUG_OBJECT (decoder, "ALLOCATION (%d) params: %" GST_PTR_FORMAT, ret,
      query);

  if (!ret)
    goto no_decide_allocation;

  /* we got configuration from our peer or the decide_allocation method,
   * parse them */
  if (gst_query_get_n_allocation_params (query) > 0) {
    gst_query_parse_nth_allocation_param (query, 0, &allocator, &params);
  } else {
    allocator = NULL;
    gst_allocation_params_init (&params);
  }

  if (gst_query_get_n_allocation_pools (query) > 0)
    gst_query_parse_nth_allocation_pool (query, 0, &pool, NULL, NULL, NULL);
  if (!pool) {
    if (allocator)
      gst_object_unref (allocator);
    ret = FALSE;
    goto no_decide_allocation;
  }

  if (decoder->priv->allocator)
    gst_object_unref (decoder->priv->allocator);
  decoder->priv->allocator = allocator;
  decoder->priv->params = params;

  if (decoder->priv->pool) {
    /* do not set the bufferpool to inactive here, it will be done
     * on its finalize function. As videodecoder do late renegotiation
     * it might happen that some element downstream is already using this
     * same bufferpool and deactivating it will make it fail.
     * Happens when a downstream element changes from passthrough to
     * non-passthrough and gets this same bufferpool to use */
    GST_DEBUG_OBJECT (decoder, "unref pool %" GST_PTR_FORMAT,
        decoder->priv->pool);
    gst_object_unref (decoder->priv->pool);
  }
  decoder->priv->pool = pool;

  /* and activate */
  GST_DEBUG_OBJECT (decoder, "activate pool %" GST_PTR_FORMAT, pool);
  gst_buffer_pool_set_active (pool, TRUE);

done:
  if (query)
    gst_query_unref (query);

  return ret;

  /* Errors */
no_decide_allocation:
  {
    GST_WARNING_OBJECT (decoder, "Subclass failed to decide allocation");
    goto done;
  }
}

static gboolean
gst_video_decoder_negotiate_default (GstVideoDecoder * decoder)
{
  GstVideoCodecState *state = decoder->priv->output_state;
  gboolean ret = TRUE;
  GstVideoCodecFrame *frame;
  GstCaps *prevcaps;
  GstCaps *incaps;

  if (!state) {
    GST_DEBUG_OBJECT (decoder,
        "Trying to negotiate the pool with out setting the o/p format");
    ret = gst_video_decoder_negotiate_pool (decoder, NULL);
    goto done;
  }

  g_return_val_if_fail (GST_VIDEO_INFO_WIDTH (&state->info) != 0, FALSE);
  g_return_val_if_fail (GST_VIDEO_INFO_HEIGHT (&state->info) != 0, FALSE);

  /* If the base class didn't set any multiview params, assume mono
   * now */
  if (GST_VIDEO_INFO_MULTIVIEW_MODE (&state->info) ==
      GST_VIDEO_MULTIVIEW_MODE_NONE) {
    GST_VIDEO_INFO_MULTIVIEW_MODE (&state->info) =
        GST_VIDEO_MULTIVIEW_MODE_MONO;
    GST_VIDEO_INFO_MULTIVIEW_FLAGS (&state->info) =
        GST_VIDEO_MULTIVIEW_FLAGS_NONE;
  }

  GST_DEBUG_OBJECT (decoder, "output_state par %d/%d fps %d/%d",
      state->info.par_n, state->info.par_d,
      state->info.fps_n, state->info.fps_d);

  if (state->caps == NULL)
    state->caps = gst_video_info_to_caps (&state->info);

  incaps = gst_pad_get_current_caps (GST_VIDEO_DECODER_SINK_PAD (decoder));
  if (incaps) {
    GstStructure *in_struct;

    in_struct = gst_caps_get_structure (incaps, 0);
    if (gst_structure_has_field (in_struct, "mastering-display-info") ||
        gst_structure_has_field (in_struct, "content-light-level")) {
      const gchar *s;

      /* prefer upstream information */
      state->caps = gst_caps_make_writable (state->caps);
      if ((s = gst_structure_get_string (in_struct, "mastering-display-info"))) {
        gst_caps_set_simple (state->caps,
            "mastering-display-info", G_TYPE_STRING, s, NULL);
      }

      if ((s = gst_structure_get_string (in_struct, "content-light-level"))) {
        gst_caps_set_simple (state->caps,
            "content-light-level", G_TYPE_STRING, s, NULL);
      }
    }

    gst_caps_unref (incaps);
  }

  if (state->allocation_caps == NULL)
    state->allocation_caps = gst_caps_ref (state->caps);

  GST_DEBUG_OBJECT (decoder, "setting caps %" GST_PTR_FORMAT, state->caps);

  /* Push all pending pre-caps events of the oldest frame before
   * setting caps */
  frame = decoder->priv->frames.head ? decoder->priv->frames.head->data : NULL;
  if (frame || decoder->priv->current_frame_events) {
    GList **events, *l;

    if (frame) {
      events = &frame->events;
    } else {
      events = &decoder->priv->current_frame_events;
    }

    for (l = g_list_last (*events); l;) {
      GstEvent *event = GST_EVENT (l->data);
      GList *tmp;

      if (GST_EVENT_TYPE (event) < GST_EVENT_CAPS) {
        gst_video_decoder_push_event (decoder, event);
        tmp = l;
        l = l->prev;
        *events = g_list_delete_link (*events, tmp);
      } else {
        l = l->prev;
      }
    }
  }

  prevcaps = gst_pad_get_current_caps (decoder->srcpad);
  if (!prevcaps || !gst_caps_is_equal (prevcaps, state->caps)) {
    if (!prevcaps) {
      GST_DEBUG_OBJECT (decoder, "decoder src pad has currently NULL caps");
    }
    ret = gst_pad_set_caps (decoder->srcpad, state->caps);
  } else {
    ret = TRUE;
    GST_DEBUG_OBJECT (decoder,
        "current src pad and output state caps are the same");
  }
  if (prevcaps)
    gst_caps_unref (prevcaps);

  if (!ret)
    goto done;
  decoder->priv->output_state_changed = FALSE;
  /* Negotiate pool */
  ret = gst_video_decoder_negotiate_pool (decoder, state->allocation_caps);

done:
  return ret;
}

static gboolean
gst_video_decoder_negotiate_unlocked (GstVideoDecoder * decoder)
{
  GstVideoDecoderClass *klass = GST_VIDEO_DECODER_GET_CLASS (decoder);
  gboolean ret = TRUE;

  if (G_LIKELY (klass->negotiate))
    ret = klass->negotiate (decoder);

  return ret;
}

/**
 * gst_video_decoder_negotiate:
 * @decoder: a #GstVideoDecoder
 *
 * Negotiate with downstream elements to currently configured #GstVideoCodecState.
 * Unmark GST_PAD_FLAG_NEED_RECONFIGURE in any case. But mark it again if
 * negotiate fails.
 *
 * Returns: %TRUE if the negotiation succeeded, else %FALSE.
 */
gboolean
gst_video_decoder_negotiate (GstVideoDecoder * decoder)
{
  GstVideoDecoderClass *klass;
  gboolean ret = TRUE;

  g_return_val_if_fail (GST_IS_VIDEO_DECODER (decoder), FALSE);

  klass = GST_VIDEO_DECODER_GET_CLASS (decoder);

  GST_VIDEO_DECODER_STREAM_LOCK (decoder);
  gst_pad_check_reconfigure (decoder->srcpad);
  if (klass->negotiate) {
    ret = klass->negotiate (decoder);
    if (!ret)
      gst_pad_mark_reconfigure (decoder->srcpad);
  }
  GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);

  return ret;
}

/**
 * gst_video_decoder_allocate_output_buffer:
 * @decoder: a #GstVideoDecoder
 *
 * Helper function that allocates a buffer to hold a video frame for @decoder's
 * current #GstVideoCodecState.
 *
 * You should use gst_video_decoder_allocate_output_frame() instead of this
 * function, if possible at all.
 *
 * Returns: (transfer full) (nullable): allocated buffer, or NULL if no buffer could be
 *     allocated (e.g. when downstream is flushing or shutting down)
 */
GstBuffer *
gst_video_decoder_allocate_output_buffer (GstVideoDecoder * decoder)
{
  GstFlowReturn flow;
  GstBuffer *buffer = NULL;
  gboolean needs_reconfigure = FALSE;

  GST_DEBUG ("alloc src buffer");

  GST_VIDEO_DECODER_STREAM_LOCK (decoder);
  needs_reconfigure = gst_pad_check_reconfigure (decoder->srcpad);
  if (G_UNLIKELY (!decoder->priv->output_state
          || decoder->priv->output_state_changed || needs_reconfigure)) {
    if (!gst_video_decoder_negotiate_unlocked (decoder)) {
      if (decoder->priv->output_state) {
        GST_DEBUG_OBJECT (decoder, "Failed to negotiate, fallback allocation");
        gst_pad_mark_reconfigure (decoder->srcpad);
        goto fallback;
      } else {
        GST_DEBUG_OBJECT (decoder, "Failed to negotiate, output_buffer=NULL");
        goto failed_allocation;
      }
    }
  }

  flow = gst_buffer_pool_acquire_buffer (decoder->priv->pool, &buffer, NULL);

  if (flow != GST_FLOW_OK) {
    GST_INFO_OBJECT (decoder, "couldn't allocate output buffer, flow %s",
        gst_flow_get_name (flow));
    if (decoder->priv->output_state && decoder->priv->output_state->info.size)
      goto fallback;
    else
      goto failed_allocation;
  }
  GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);

  return buffer;

fallback:
  GST_INFO_OBJECT (decoder,
      "Fallback allocation, creating new buffer which doesn't belongs to any buffer pool");
  buffer =
      gst_buffer_new_allocate (NULL, decoder->priv->output_state->info.size,
      NULL);

failed_allocation:
  GST_ERROR_OBJECT (decoder, "Failed to allocate the buffer..");
  GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);

  return buffer;
}

/**
 * gst_video_decoder_allocate_output_frame:
 * @decoder: a #GstVideoDecoder
 * @frame: a #GstVideoCodecFrame
 *
 * Helper function that allocates a buffer to hold a video frame for @decoder's
 * current #GstVideoCodecState.  Subclass should already have configured video
 * state and set src pad caps.
 *
 * The buffer allocated here is owned by the frame and you should only
 * keep references to the frame, not the buffer.
 *
 * Returns: %GST_FLOW_OK if an output buffer could be allocated
 */
GstFlowReturn
gst_video_decoder_allocate_output_frame (GstVideoDecoder *
    decoder, GstVideoCodecFrame * frame)
{
  return gst_video_decoder_allocate_output_frame_with_params (decoder, frame,
      NULL);
}

/**
 * gst_video_decoder_allocate_output_frame_with_params:
 * @decoder: a #GstVideoDecoder
 * @frame: a #GstVideoCodecFrame
 * @params: a #GstBufferPoolAcquireParams
 *
 * Same as #gst_video_decoder_allocate_output_frame except it allows passing
 * #GstBufferPoolAcquireParams to the sub call gst_buffer_pool_acquire_buffer.
 *
 * Returns: %GST_FLOW_OK if an output buffer could be allocated
 *
 * Since: 1.12
 */
GstFlowReturn
gst_video_decoder_allocate_output_frame_with_params (GstVideoDecoder *
    decoder, GstVideoCodecFrame * frame, GstBufferPoolAcquireParams * params)
{
  GstFlowReturn flow_ret;
  GstVideoCodecState *state;
  int num_bytes;
  gboolean needs_reconfigure = FALSE;

  g_return_val_if_fail (decoder->priv->output_state, GST_FLOW_NOT_NEGOTIATED);
  g_return_val_if_fail (frame->output_buffer == NULL, GST_FLOW_ERROR);

  GST_VIDEO_DECODER_STREAM_LOCK (decoder);

  state = decoder->priv->output_state;
  if (state == NULL) {
    g_warning ("Output state should be set before allocating frame");
    goto error;
  }
  num_bytes = GST_VIDEO_INFO_SIZE (&state->info);
  if (num_bytes == 0) {
    g_warning ("Frame size should not be 0");
    goto error;
  }

  needs_reconfigure = gst_pad_check_reconfigure (decoder->srcpad);
  if (G_UNLIKELY (decoder->priv->output_state_changed || needs_reconfigure)) {
    if (!gst_video_decoder_negotiate_unlocked (decoder)) {
      gst_pad_mark_reconfigure (decoder->srcpad);
      if (GST_PAD_IS_FLUSHING (decoder->srcpad)) {
        GST_DEBUG_OBJECT (decoder,
            "Failed to negotiate a pool: pad is flushing");
        goto flushing;
      } else if (!decoder->priv->pool || decoder->priv->output_state_changed) {
        GST_DEBUG_OBJECT (decoder,
            "Failed to negotiate a pool and no previous pool to reuse");
        goto error;
      } else {
        GST_DEBUG_OBJECT (decoder,
            "Failed to negotiate a pool, falling back to the previous pool");
      }
    }
  }

  GST_LOG_OBJECT (decoder, "alloc buffer size %d", num_bytes);

  flow_ret = gst_buffer_pool_acquire_buffer (decoder->priv->pool,
      &frame->output_buffer, params);

  GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);

  return flow_ret;

flushing:
  GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);
  return GST_FLOW_FLUSHING;

error:
  GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);
  return GST_FLOW_ERROR;
}

/**
 * gst_video_decoder_get_max_decode_time:
 * @decoder: a #GstVideoDecoder
 * @frame: a #GstVideoCodecFrame
 *
 * Determines maximum possible decoding time for @frame that will
 * allow it to decode and arrive in time (as determined by QoS events).
 * In particular, a negative result means decoding in time is no longer possible
 * and should therefore occur as soon/skippy as possible.
 *
 * Returns: max decoding time.
 */
GstClockTimeDiff
gst_video_decoder_get_max_decode_time (GstVideoDecoder *
    decoder, GstVideoCodecFrame * frame)
{
  GstClockTimeDiff deadline;
  GstClockTime earliest_time;

  GST_OBJECT_LOCK (decoder);
  earliest_time = decoder->priv->earliest_time;
  if (GST_CLOCK_TIME_IS_VALID (earliest_time)
      && GST_CLOCK_TIME_IS_VALID (frame->deadline))
    deadline = GST_CLOCK_DIFF (earliest_time, frame->deadline);
  else
    deadline = G_MAXINT64;

  GST_LOG_OBJECT (decoder, "earliest %" GST_TIME_FORMAT
      ", frame deadline %" GST_TIME_FORMAT ", deadline %" GST_STIME_FORMAT,
      GST_TIME_ARGS (earliest_time), GST_TIME_ARGS (frame->deadline),
      GST_STIME_ARGS (deadline));

  GST_OBJECT_UNLOCK (decoder);

  return deadline;
}

/**
 * gst_video_decoder_get_qos_proportion:
 * @decoder: a #GstVideoDecoder
 *     current QoS proportion, or %NULL
 *
 * Returns: The current QoS proportion.
 *
 * Since: 1.0.3
 */
gdouble
gst_video_decoder_get_qos_proportion (GstVideoDecoder * decoder)
{
  gdouble proportion;

  g_return_val_if_fail (GST_IS_VIDEO_DECODER (decoder), 1.0);

  GST_OBJECT_LOCK (decoder);
  proportion = decoder->priv->proportion;
  GST_OBJECT_UNLOCK (decoder);

  return proportion;
}

GstFlowReturn
_gst_video_decoder_error (GstVideoDecoder * dec, gint weight,
    GQuark domain, gint code, gchar * txt, gchar * dbg, const gchar * file,
    const gchar * function, gint line)
{
  if (txt)
    GST_WARNING_OBJECT (dec, "error: %s", txt);
  if (dbg)
    GST_WARNING_OBJECT (dec, "error: %s", dbg);
  dec->priv->error_count += weight;
  dec->priv->discont = TRUE;
  if (dec->priv->max_errors >= 0 &&
      dec->priv->error_count > dec->priv->max_errors) {
    gst_element_message_full (GST_ELEMENT (dec), GST_MESSAGE_ERROR,
        domain, code, txt, dbg, file, function, line);
    return GST_FLOW_ERROR;
  } else {
    g_free (txt);
    g_free (dbg);
    return GST_FLOW_OK;
  }
}

/**
 * gst_video_decoder_set_max_errors:
 * @dec: a #GstVideoDecoder
 * @num: max tolerated errors
 *
 * Sets numbers of tolerated decoder errors, where a tolerated one is then only
 * warned about, but more than tolerated will lead to fatal error.  You can set
 * -1 for never returning fatal errors. Default is set to
 * GST_VIDEO_DECODER_MAX_ERRORS.
 *
 * The '-1' option was added in 1.4
 */
void
gst_video_decoder_set_max_errors (GstVideoDecoder * dec, gint num)
{
  g_return_if_fail (GST_IS_VIDEO_DECODER (dec));

  dec->priv->max_errors = num;
}

/**
 * gst_video_decoder_get_max_errors:
 * @dec: a #GstVideoDecoder
 *
 * Returns: currently configured decoder tolerated error count.
 */
gint
gst_video_decoder_get_max_errors (GstVideoDecoder * dec)
{
  g_return_val_if_fail (GST_IS_VIDEO_DECODER (dec), 0);

  return dec->priv->max_errors;
}

/**
 * gst_video_decoder_set_needs_format:
 * @dec: a #GstVideoDecoder
 * @enabled: new state
 *
 * Configures decoder format needs.  If enabled, subclass needs to be
 * negotiated with format caps before it can process any data.  It will then
 * never be handed any data before it has been configured.
 * Otherwise, it might be handed data without having been configured and
 * is then expected being able to do so either by default
 * or based on the input data.
 *
 * Since: 1.4
 */
void
gst_video_decoder_set_needs_format (GstVideoDecoder * dec, gboolean enabled)
{
  g_return_if_fail (GST_IS_VIDEO_DECODER (dec));

  dec->priv->needs_format = enabled;
}

/**
 * gst_video_decoder_get_needs_format:
 * @dec: a #GstVideoDecoder
 *
 * Queries decoder required format handling.
 *
 * Returns: %TRUE if required format handling is enabled.
 *
 * Since: 1.4
 */
gboolean
gst_video_decoder_get_needs_format (GstVideoDecoder * dec)
{
  gboolean result;

  g_return_val_if_fail (GST_IS_VIDEO_DECODER (dec), FALSE);

  result = dec->priv->needs_format;

  return result;
}

/**
 * gst_video_decoder_set_packetized:
 * @decoder: a #GstVideoDecoder
 * @packetized: whether the input data should be considered as packetized.
 *
 * Allows baseclass to consider input data as packetized or not. If the
 * input is packetized, then the @parse method will not be called.
 */
void
gst_video_decoder_set_packetized (GstVideoDecoder * decoder,
    gboolean packetized)
{
  decoder->priv->packetized = packetized;
}

/**
 * gst_video_decoder_get_packetized:
 * @decoder: a #GstVideoDecoder
 *
 * Queries whether input data is considered packetized or not by the
 * base class.
 *
 * Returns: TRUE if input data is considered packetized.
 */
gboolean
gst_video_decoder_get_packetized (GstVideoDecoder * decoder)
{
  return decoder->priv->packetized;
}

/**
 * gst_video_decoder_have_last_subframe:
 * @decoder: a #GstVideoDecoder
 * @frame: (transfer none): the #GstVideoCodecFrame to update
 *
 * Indicates that the last subframe has been processed by the decoder
 * in @frame. This will release the current frame in video decoder
 * allowing to receive new frames from upstream elements. This method
 * must be called in the subclass @handle_frame callback.
 *
 * Returns: a #GstFlowReturn, usually GST_FLOW_OK.
 *
 * Since: 1.20
 */
GstFlowReturn
gst_video_decoder_have_last_subframe (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame)
{
  g_return_val_if_fail (gst_video_decoder_get_subframe_mode (decoder),
      GST_FLOW_OK);
  /* unref once from the list */
  GST_VIDEO_DECODER_STREAM_LOCK (decoder);
  if (decoder->priv->current_frame == frame) {
    gst_video_codec_frame_unref (decoder->priv->current_frame);
    decoder->priv->current_frame = NULL;
  }
  GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);

  return GST_FLOW_OK;
}

/**
 * gst_video_decoder_set_subframe_mode:
 * @decoder: a #GstVideoDecoder
 * @subframe_mode: whether the input data should be considered as subframes.
 *
 * If this is set to TRUE, it informs the base class that the subclass
 * can receive the data at a granularity lower than one frame.
 *
 * Note that in this mode, the subclass has two options. It can either
 * require the presence of a GST_VIDEO_BUFFER_FLAG_MARKER to mark the
 * end of a frame. Or it can operate in such a way that it will decode
 * a single frame at a time. In this second case, every buffer that
 * arrives to the element is considered part of the same frame until
 * gst_video_decoder_finish_frame() is called.
 *
 * In either case, the same #GstVideoCodecFrame will be passed to the
 * GstVideoDecoderClass:handle_frame vmethod repeatedly with a
 * different GstVideoCodecFrame:input_buffer every time until the end of the
 * frame has been signaled using either method.
 * This method must be called during the decoder subclass @set_format call.
 *
 * Since: 1.20
 */
void
gst_video_decoder_set_subframe_mode (GstVideoDecoder * decoder,
    gboolean subframe_mode)
{
  decoder->priv->subframe_mode = subframe_mode;
}

/**
 * gst_video_decoder_get_subframe_mode:
 * @decoder: a #GstVideoDecoder
 *
 * Queries whether input data is considered as subframes or not by the
 * base class. If FALSE, each input buffer will be considered as a full
 * frame.
 *
 * Returns: TRUE if input data is considered as sub frames.
 *
 * Since: 1.20
 */
gboolean
gst_video_decoder_get_subframe_mode (GstVideoDecoder * decoder)
{
  return decoder->priv->subframe_mode;
}

/**
 * gst_video_decoder_get_input_subframe_index:
 * @decoder: a #GstVideoDecoder
 * @frame: (transfer none): the #GstVideoCodecFrame to update
 *
 * Queries the number of the last subframe received by
 * the decoder baseclass in the @frame.
 *
 * Returns: the current subframe index received in subframe mode, 1 otherwise.
 *
 * Since: 1.20
 */
guint
gst_video_decoder_get_input_subframe_index (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame)
{
  if (gst_video_decoder_get_subframe_mode (decoder))
    return frame->abidata.ABI.num_subframes;
  return 1;
}

/**
 * gst_video_decoder_get_processed_subframe_index:
 * @decoder: a #GstVideoDecoder
 * @frame: (transfer none): the #GstVideoCodecFrame to update
 *
 * Queries the number of subframes in the frame processed by
 * the decoder baseclass.
 *
 * Returns: the current subframe processed received in subframe mode.
 *
 * Since: 1.20
 */
guint
gst_video_decoder_get_processed_subframe_index (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame)
{
  return frame->abidata.ABI.subframes_processed;
}

/**
 * gst_video_decoder_set_estimate_rate:
 * @dec: a #GstVideoDecoder
 * @enabled: whether to enable byte to time conversion
 *
 * Allows baseclass to perform byte to time estimated conversion.
 */
void
gst_video_decoder_set_estimate_rate (GstVideoDecoder * dec, gboolean enabled)
{
  g_return_if_fail (GST_IS_VIDEO_DECODER (dec));

  dec->priv->do_estimate_rate = enabled;
}

/**
 * gst_video_decoder_get_estimate_rate:
 * @dec: a #GstVideoDecoder
 *
 * Returns: currently configured byte to time conversion setting
 */
gboolean
gst_video_decoder_get_estimate_rate (GstVideoDecoder * dec)
{
  g_return_val_if_fail (GST_IS_VIDEO_DECODER (dec), 0);

  return dec->priv->do_estimate_rate;
}

/**
 * gst_video_decoder_set_latency:
 * @decoder: a #GstVideoDecoder
 * @min_latency: minimum latency
 * @max_latency: maximum latency
 *
 * Lets #GstVideoDecoder sub-classes tell the baseclass what the decoder latency
 * is. If the provided values changed from previously provided ones, this will
 * also post a LATENCY message on the bus so the pipeline can reconfigure its
 * global latency.
 */
void
gst_video_decoder_set_latency (GstVideoDecoder * decoder,
    GstClockTime min_latency, GstClockTime max_latency)
{
  gboolean post_message = FALSE;
  g_return_if_fail (GST_CLOCK_TIME_IS_VALID (min_latency));
  g_return_if_fail (max_latency >= min_latency);

  GST_DEBUG_OBJECT (decoder,
      "min_latency:%" GST_TIME_FORMAT " max_latency:%" GST_TIME_FORMAT,
      GST_TIME_ARGS (min_latency), GST_TIME_ARGS (max_latency));

  GST_OBJECT_LOCK (decoder);
  if (decoder->priv->min_latency != min_latency) {
    decoder->priv->min_latency = min_latency;
    post_message = TRUE;
  }
  if (decoder->priv->max_latency != max_latency) {
    decoder->priv->max_latency = max_latency;
    post_message = TRUE;
  }
  if (!decoder->priv->posted_latency_msg) {
    decoder->priv->posted_latency_msg = TRUE;
    post_message = TRUE;
  }
  GST_OBJECT_UNLOCK (decoder);

  if (post_message)
    gst_element_post_message (GST_ELEMENT_CAST (decoder),
        gst_message_new_latency (GST_OBJECT_CAST (decoder)));
}

/**
 * gst_video_decoder_get_latency:
 * @decoder: a #GstVideoDecoder
 * @min_latency: (out) (optional): address of variable in which to store the
 *     configured minimum latency, or %NULL
 * @max_latency: (out) (optional): address of variable in which to store the
 *     configured mximum latency, or %NULL
 *
 * Query the configured decoder latency. Results will be returned via
 * @min_latency and @max_latency.
 */
void
gst_video_decoder_get_latency (GstVideoDecoder * decoder,
    GstClockTime * min_latency, GstClockTime * max_latency)
{
  GST_OBJECT_LOCK (decoder);
  if (min_latency)
    *min_latency = decoder->priv->min_latency;
  if (max_latency)
    *max_latency = decoder->priv->max_latency;
  GST_OBJECT_UNLOCK (decoder);
}

/**
 * gst_video_decoder_merge_tags:
 * @decoder: a #GstVideoDecoder
 * @tags: (nullable): a #GstTagList to merge, or NULL to unset
 *     previously-set tags
 * @mode: the #GstTagMergeMode to use, usually #GST_TAG_MERGE_REPLACE
 *
 * Sets the audio decoder tags and how they should be merged with any
 * upstream stream tags. This will override any tags previously-set
 * with gst_audio_decoder_merge_tags().
 *
 * Note that this is provided for convenience, and the subclass is
 * not required to use this and can still do tag handling on its own.
 *
 * MT safe.
 */
void
gst_video_decoder_merge_tags (GstVideoDecoder * decoder,
    const GstTagList * tags, GstTagMergeMode mode)
{
  g_return_if_fail (GST_IS_VIDEO_DECODER (decoder));
  g_return_if_fail (tags == NULL || GST_IS_TAG_LIST (tags));
  g_return_if_fail (tags == NULL || mode != GST_TAG_MERGE_UNDEFINED);

  GST_VIDEO_DECODER_STREAM_LOCK (decoder);
  if (decoder->priv->tags != tags) {
    if (decoder->priv->tags) {
      gst_tag_list_unref (decoder->priv->tags);
      decoder->priv->tags = NULL;
      decoder->priv->tags_merge_mode = GST_TAG_MERGE_APPEND;
    }
    if (tags) {
      decoder->priv->tags = gst_tag_list_ref ((GstTagList *) tags);
      decoder->priv->tags_merge_mode = mode;
    }

    GST_DEBUG_OBJECT (decoder, "set decoder tags to %" GST_PTR_FORMAT, tags);
    decoder->priv->tags_changed = TRUE;
  }
  GST_VIDEO_DECODER_STREAM_UNLOCK (decoder);
}

/**
 * gst_video_decoder_get_buffer_pool:
 * @decoder: a #GstVideoDecoder
 *
 * Returns: (transfer full) (nullable): the instance of the #GstBufferPool used
 * by the decoder; free it after use it
 */
GstBufferPool *
gst_video_decoder_get_buffer_pool (GstVideoDecoder * decoder)
{
  g_return_val_if_fail (GST_IS_VIDEO_DECODER (decoder), NULL);

  if (decoder->priv->pool)
    return gst_object_ref (decoder->priv->pool);

  return NULL;
}

/**
 * gst_video_decoder_get_allocator:
 * @decoder: a #GstVideoDecoder
 * @allocator: (out) (optional) (nullable) (transfer full): the #GstAllocator
 * used
 * @params: (out) (optional) (transfer full): the
 * #GstAllocationParams of @allocator
 *
 * Lets #GstVideoDecoder sub-classes to know the memory @allocator
 * used by the base class and its @params.
 *
 * Unref the @allocator after use it.
 */
void
gst_video_decoder_get_allocator (GstVideoDecoder * decoder,
    GstAllocator ** allocator, GstAllocationParams * params)
{
  g_return_if_fail (GST_IS_VIDEO_DECODER (decoder));

  if (allocator)
    *allocator = decoder->priv->allocator ?
        gst_object_ref (decoder->priv->allocator) : NULL;

  if (params)
    *params = decoder->priv->params;
}

/**
 * gst_video_decoder_set_use_default_pad_acceptcaps:
 * @decoder: a #GstVideoDecoder
 * @use: if the default pad accept-caps query handling should be used
 *
 * Lets #GstVideoDecoder sub-classes decide if they want the sink pad
 * to use the default pad query handler to reply to accept-caps queries.
 *
 * By setting this to true it is possible to further customize the default
 * handler with %GST_PAD_SET_ACCEPT_INTERSECT and
 * %GST_PAD_SET_ACCEPT_TEMPLATE
 *
 * Since: 1.6
 */
void
gst_video_decoder_set_use_default_pad_acceptcaps (GstVideoDecoder * decoder,
    gboolean use)
{
  decoder->priv->use_default_pad_acceptcaps = use;
}

static void
gst_video_decoder_request_sync_point_internal (GstVideoDecoder * dec,
    GstClockTime deadline, GstVideoDecoderRequestSyncPointFlags flags)
{
  GstEvent *fku = NULL;
  GstVideoDecoderPrivate *priv;

  g_return_if_fail (GST_IS_VIDEO_DECODER (dec));

  priv = dec->priv;

  GST_OBJECT_LOCK (dec);

  /* Check if we're allowed to send a new force-keyunit event.
   * frame->deadline is set to the running time of the PTS. */
  if (priv->min_force_key_unit_interval == 0 ||
      deadline == GST_CLOCK_TIME_NONE ||
      (priv->min_force_key_unit_interval != GST_CLOCK_TIME_NONE &&
          (priv->last_force_key_unit_time == GST_CLOCK_TIME_NONE
              || (priv->last_force_key_unit_time +
                  priv->min_force_key_unit_interval <= deadline)))) {
    GST_DEBUG_OBJECT (dec,
        "Requesting a new key-unit for frame with deadline %" GST_TIME_FORMAT,
        GST_TIME_ARGS (deadline));
    fku =
        gst_video_event_new_upstream_force_key_unit (GST_CLOCK_TIME_NONE, FALSE,
        0);
    priv->last_force_key_unit_time = deadline;
  } else {
    GST_DEBUG_OBJECT (dec,
        "Can't request a new key-unit for frame with deadline %"
        GST_TIME_FORMAT, GST_TIME_ARGS (deadline));
  }
  priv->request_sync_point_flags |= flags;
  /* We don't know yet the frame number of the sync point so set it to a
   * frame number higher than any allowed frame number */
  priv->request_sync_point_frame_number = REQUEST_SYNC_POINT_PENDING;
  GST_OBJECT_UNLOCK (dec);

  if (fku)
    gst_pad_push_event (dec->sinkpad, fku);
}

/**
 * gst_video_decoder_request_sync_point:
 * @dec: a #GstVideoDecoder
 * @frame: a #GstVideoCodecFrame
 * @flags: #GstVideoDecoderRequestSyncPointFlags
 *
 * Allows the #GstVideoDecoder subclass to request from the base class that
 * a new sync should be requested from upstream, and that @frame was the frame
 * when the subclass noticed that a new sync point is required. A reason for
 * the subclass to do this could be missing reference frames, for example.
 *
 * The base class will then request a new sync point from upstream as long as
 * the time that passed since the last one is exceeding
 * #GstVideoDecoder:min-force-key-unit-interval.
 *
 * The subclass can signal via @flags how the frames until the next sync point
 * should be handled:
 *
 *   * If %GST_VIDEO_DECODER_REQUEST_SYNC_POINT_DISCARD_INPUT is selected then
 *     all following input frames until the next sync point are discarded.
 *     This can be useful if the lack of a sync point will prevent all further
 *     decoding and the decoder implementation is not very robust in handling
 *     missing references frames.
 *   * If %GST_VIDEO_DECODER_REQUEST_SYNC_POINT_CORRUPT_OUTPUT is selected
 *     then all output frames following @frame are marked as corrupted via
 *     %GST_BUFFER_FLAG_CORRUPTED. Corrupted frames can be automatically
 *     dropped by the base class, see #GstVideoDecoder:discard-corrupted-frames.
 *     Subclasses can manually mark frames as corrupted via %GST_VIDEO_CODEC_FRAME_FLAG_CORRUPTED
 *     before calling gst_video_decoder_finish_frame().
 *
 * Since: 1.20
 */
void
gst_video_decoder_request_sync_point (GstVideoDecoder * dec,
    GstVideoCodecFrame * frame, GstVideoDecoderRequestSyncPointFlags flags)
{
  g_return_if_fail (GST_IS_VIDEO_DECODER (dec));
  g_return_if_fail (frame != NULL);

  gst_video_decoder_request_sync_point_internal (dec, frame->deadline, flags);
}

/**
 * gst_video_decoder_set_needs_sync_point:
 * @dec: a #GstVideoDecoder
 * @enabled: new state
 *
 * Configures whether the decoder requires a sync point before it starts
 * outputting data in the beginning. If enabled, the base class will discard
 * all non-sync point frames in the beginning and after a flush and does not
 * pass it to the subclass.
 *
 * If the first frame is not a sync point, the base class will request a sync
 * point via the force-key-unit event.
 *
 * Since: 1.20
 */
void
gst_video_decoder_set_needs_sync_point (GstVideoDecoder * dec, gboolean enabled)
{
  g_return_if_fail (GST_IS_VIDEO_DECODER (dec));

  dec->priv->needs_sync_point = enabled;
}

/**
 * gst_video_decoder_get_needs_sync_point:
 * @dec: a #GstVideoDecoder
 *
 * Queries if the decoder requires a sync point before it starts outputting
 * data in the beginning.
 *
 * Returns: %TRUE if a sync point is required in the beginning.
 *
 * Since: 1.20
 */
gboolean
gst_video_decoder_get_needs_sync_point (GstVideoDecoder * dec)
{
  gboolean result;

  g_return_val_if_fail (GST_IS_VIDEO_DECODER (dec), FALSE);

  result = dec->priv->needs_sync_point;

  return result;
}
