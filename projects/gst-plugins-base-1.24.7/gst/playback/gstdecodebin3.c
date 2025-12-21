/* GStreamer
 *
 * Copyright (C) <2015> Centricular Ltd
 *  @author: Edward Hervey <edward@centricular.com>
 *  @author: Jan Schmidt <jan@centricular.com>
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

#include <glib.h>
#include <glib-object.h>
#include <glib/gprintf.h>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include "gstplaybackelements.h"
#include "gstrawcaps.h"

/**
 * SECTION:element-decodebin3
 * @title: decodebin3
 *
 * #GstBin that auto-magically constructs a decoding pipeline using available
 * decoders and demuxers via auto-plugging. The output is raw audio, video
 * or subtitle streams.
 *
 * decodebin3 differs from the previous decodebin (decodebin2) in important ways:
 *
 * * supports publication and selection of stream information via
 * GstStreamCollection messages and #GST_EVENT_SELECT_STREAMS events.
 *
 * * dynamically switches stream connections internally, and
 * reuses decoder elements when stream selections change, so that in
 * the normal case it maintains 1 decoder of each type (video/audio/subtitle)
 * and only creates new elements when streams change and an existing decoder
 * is not capable of handling the new format.
 *
 * * supports multiple input pads for the parallel decoding of auxiliary streams
 * not muxed with the primary stream.
 *
 * * does not handle network stream buffering. decodebin3 expects that network stream
 * buffering is handled upstream, before data is passed to it.
 *
 */

/*
 * Global design
 *
 * 1) From sink pad to elementary streams (GstParseBin or identity)
 *
 * Note : If the incoming streams are push-based-only and are compatible with
 * either the output caps or a potential decoder, the usage of parsebin is
 * replaced by a simple passthrough identity element.
 *
 * The input sink pads are fed to GstParseBin. GstParseBin will feed them
 * through typefind. When the caps are detected (or changed) we recursively
 * figure out which demuxer, parser or depayloader is needed until we get to
 * elementary streams.
 *
 * All elementary streams (whether decoded or not, whether exposed or not) are
 * fed through multiqueue. There is only *one* multiqueue in decodebin3.
 *
 * => MultiQueue is the cornerstone.
 * => No buffering before multiqueue
 *
 * 2) Elementary streams
 *
 * After GstParseBin, there are 3 main components:
 *  1) Input Streams (provided by GstParseBin)
 *  2) Multiqueue slots
 *  3) Output Streams
 *
 * Input Streams correspond to the stream coming from GstParseBin and that gets
 * fed into a multiqueue slot.
 *
 * Output Streams correspond to the combination of a (optional) decoder and an
 * output ghostpad. Output Streams can be moved from one multiqueue slot to
 * another, can reconfigure itself (different decoders), and can be
 * added/removed depending on the configuration (all streams outputted, only one
 * of each type, ...).
 *
 * Multiqueue slots correspond to a pair of sink/src pad from multiqueue. For
 * each 'active' Input Stream there is a corresponding slot.
 * Slots might have different streams on input and output (due to internal
 * buffering).
 *
 * Due to internal queuing/buffering/..., all those components (might) behave
 * asynchronously. Therefore probes will be used on each component source pad to
 * detect various key-points:
 *  * EOS :
 *     the stream is done => Mark that component as done, optionally freeing/removing it
 *  * STREAM_START :
 *     a new stream is starting => link it further if needed
 *
 * 3) Gradual replacement
 *
 * If the caps change at any point in decodebin (input sink pad, demuxer output,
 * multiqueue output, ..), we gradually replace (if needed) the following elements.
 *
 * This is handled by the probes in various locations:
 *  a) typefind output
 *  b) multiqueue input (source pad of Input Streams)
 *  c) multiqueue output (source pad of Multiqueue Slots)
 *  d) final output (target of source ghostpads)
 *
 * When CAPS event arrive at those points, one of three things can happen:
 * a) There is no elements downstream yet, just create/link-to following elements
 * b) There are downstream elements, do a ACCEPT_CAPS query
 *  b.1) The new CAPS are accepted, keep current configuration
 *  b.2) The new CAPS are not accepted, remove following elements then do a)
 *
 *    Components:
 *
 *                                                   MultiQ     Output
 *                     Input(s)                      Slots      Streams
 *  /-------------------------------------------\   /-----\  /------------- \
 *
 * +-------------------------------------------------------------------------+
 * |                                                                         |
 * | +---------------------------------------------+                         |
 * | |   GstParseBin(s)                            |                         |
 * | |                +--------------+             |  +-----+                |
 * | |                |              |---[parser]-[|--| Mul |---[ decoder ]-[|
 * |]--[ typefind ]---|  demuxer(s)  |------------[|  | ti  |                |
 * | |                |  (if needed) |---[parser]-[|--| qu  |                |
 * | |                |              |---[parser]-[|--| eu  |---[ decoder ]-[|
 * | |                +--------------+             |  +------             ^  |
 * | +---------------------------------------------+        ^             |  |
 * |                                               ^        |             |  |
 * +-----------------------------------------------+--------+-------------+--+
 *                                                 |        |             |
 *                                                 |        |             |
 *                                       Probes  --/--------/-------------/
 *
 * 4) ATOMIC SWITCHING
 *
 * We want to ensure we re-use decoders when switching streams. This takes place
 * at the multiqueue output level.
 *
 * MAIN CONCEPTS
 *  1) Activating a stream (i.e. linking a slot to an output) is only done within
 *    the streaming thread in the multiqueue_src_probe() and only if the
 *    stream is in the REQUESTED selection.
 *  2) Deactivating a stream (i.e. unlinking a slot from an output) is also done
 *    within the stream thread, but only in a purposefully called IDLE probe
 *    that calls mq_slot_reassign().
 *
 * Based on those two principles, 2 "selection" of streams (stream-id) are used:
 * 1) requested_selection
 *    All streams within that list should be activated
 * 2) to_activate
 *    List of streams that will be moved to requested_selection in the
 *    mq_slot_reassign() method (i.e. once a stream was deactivated, and the
 *    output was retargetted)
 *
 * 5) Change on collections/selection
 *
 * The collection of streams available can change through time. Either because
 * there is an update in the stream itself (like mpeg-ts/mpeg-ts), or from
 * gapless playback changes, etc ...
 *
 * This therefore means that there are potentially more than one "collection" of
 * streams in decodebin3 (i.e. input streams might be different from what is
 * being outputted). And the incoming stream selection might be happening on any
 * of those.
 *
 * In order to handle that, decodebin3 has a list of `DecodebinCollection` which
 * groups together the GstStreamCollection, the requested streams to activate
 * and their status.
 *
 * * The input and output collection can be different
 * * The output_collection is the one *currently* present on the output of
 *   multiqueue
 * * For each incoming GST_EVENT_SELECT_STREAMS, we figure out the oldest
 *   GstStreamCollection to which this applies and store the list of requested
 *   streams. If it is the current output_collection we can handle the switch
 *   immediately, else it will be handled later.
 * * By detecting the new GST_EVENT_STREAM_START on the output of multiqueue, we
 *   can identify when we are switching to a new DecodebinCollection. If that is
 *   the case we progressively switch over to the new requested streams.
 */


GST_DEBUG_CATEGORY_STATIC (decodebin3_debug);
#define GST_CAT_DEFAULT decodebin3_debug

#define GST_TYPE_DECODEBIN3	 (gst_decodebin3_get_type ())

#define EXTRA_DEBUG 1

#define CUSTOM_FINAL_EOS_QUARK _custom_final_eos_quark_get ()
#define CUSTOM_FINAL_EOS_QUARK_DATA "custom-final-eos"
static GQuark
_custom_final_eos_quark_get (void)
{
  static gsize g_quark;

  if (g_once_init_enter (&g_quark)) {
    gsize quark =
        (gsize) g_quark_from_static_string ("decodebin3-custom-final-eos");
    g_once_init_leave (&g_quark, quark);
  }
  return g_quark;
}

#define CUSTOM_EOS_QUARK _custom_eos_quark_get ()
#define CUSTOM_EOS_QUARK_DATA "custom-eos"
static GQuark
_custom_eos_quark_get (void)
{
  static gsize g_quark;

  if (g_once_init_enter (&g_quark)) {
    gsize quark = (gsize) g_quark_from_static_string ("decodebin3-custom-eos");
    g_once_init_leave (&g_quark, quark);
  }
  return g_quark;
}

typedef struct _GstDecodebin3 GstDecodebin3;
typedef struct _GstDecodebin3Class GstDecodebin3Class;

typedef struct _DecodebinInputStream DecodebinInputStream;
typedef struct _DecodebinInput DecodebinInput;
typedef struct _DecodebinOutputStream DecodebinOutputStream;

/* Store information regarding collections */
typedef struct
{
  GstStreamCollection *collection;
  /* The list of stream-ids requested for this collection.
   *
   * Can be NULL (we need to make a selection ourselves when this collection
   * starts to appear on the output of multiqueue)
   */
  GList *requested_selection;

  /* TEMPORARY : List of streams to activate, LEGACY usage */
  GList *to_activate;

  /* The seqnum of the event that created the list of requested streams
   * (GST_SEQNUM_INVALID if not requested from outside) */
  guint32 seqnum;

  /* TRUE if GST_MESSAGE_STREAMS_SELECTED was posted for the stream_ids. Must be
   * resetted whenever the stream_ids changes */
  gboolean posted_streams_selected_msg;

  /* TRUE if all stream_ids have an associated MultiqueueSlot. i.e. the
   * collection is active */
  gboolean all_streams_present;

  /* TRUE if this collection is an update of the previous one.  i.e. it only
   * *adds* new streams. */
  gboolean is_update;
} DecodebinCollection;

typedef struct
{
  GstElement *element;
  GstMessage *error;            // Last error message seen for that element
  GstMessage *latency;          // Last latency message seen for that element
} CandidateDecoder;

struct _GstDecodebin3
{
  GstBin bin;

  /* input_lock protects the following variables */
  GMutex input_lock;
  /* Main input (static sink pad) */
  DecodebinInput *main_input;
  /* Supplementary input (request sink pads) */
  GList *other_inputs;
  /* counter for input */
  guint32 input_counter;
  /* Current stream group_id (default : GST_GROUP_ID_INVALID) */
  guint32 current_group_id;
  /* End of variables protected by input_lock */

  GstElement *multiqueue;
  GstClockTime default_mq_min_interleave;
  GstClockTime current_mq_min_interleave;

  /* selection_lock protects access to following variables */
  GMutex selection_lock;
  GList *input_streams;         /* List of DecodebinInputStream for active collection */
  GList *output_streams;        /* List of DecodebinOutputStream used for output */
  GList *slots;                 /* List of MultiQueueSlot */
  guint slot_id;

  /* List of DecodebinCollection in existence. ordered by oldest (i.e. first is
   * currently outputted, last is most recent incoming */
  GList *collections;

  /* Current input collection. */
  DecodebinCollection *input_collection;

  /* Current output collection */
  DecodebinCollection *output_collection;
  /* End of variables protected by selection_lock */

  /* Upstream handles stream selection */
  gboolean upstream_handles_selection;

  /* Factories */
  GMutex factories_lock;
  guint32 factories_cookie;
  /* All DECODABLE factories */
  GList *factories;
  /* Only DECODER factories */
  GList *decoder_factories;
  /* DECODABLE but not DECODER factories */
  GList *decodable_factories;

  /* counters for pads */
  guint32 apadcount, vpadcount, tpadcount, opadcount;

  /* Properties */
  GstCaps *caps;

  GList *candidate_decoders;
};

struct _GstDecodebin3Class
{
  GstBinClass class;

    gint (*select_stream) (GstDecodebin3 * dbin,
      GstStreamCollection * collection, GstStream * stream);
};

/* Input of decodebin, controls input pad and parsebin */
struct _DecodebinInput
{
  GstDecodebin3 *dbin;

  gboolean is_main;

  GstPad *ghost_sink;
  GstPad *parsebin_sink;

  GstStreamCollection *collection;      /* Active collection */
  gboolean upstream_selected;

  guint group_id;

  /* Either parsebin or identity is used */
  GstElement *parsebin;
  GstElement *identity;

  gulong pad_added_sigid;
  gulong pad_removed_sigid;
  gulong drained_sigid;

  /* TRUE if the input got drained */
  gboolean drained;

  /* TEMPORARY HACK for knowing if upstream is already parsed and identity can
   * be avoided */
  gboolean input_is_parsed;

  /* List of events that need to be pushed once we get the first
   * GST_EVENT_STREAM_COLLECTION */
  GList *events_waiting_for_collection;

  /* input buffer probe for detecting whether input has caps or not */
  gulong input_probe;
};

/* Streams that come from parsebin or identity */
/* FIXME : All this is hardcoded. Switch to tree of chains */
struct _DecodebinInputStream
{
  GstDecodebin3 *dbin;

  GstStream *active_stream;

  DecodebinInput *input;

  GstPad *srcpad;               /* From parsebin or identity */

  /* id of the pad event probe */
  gulong output_event_probe_id;

  /* id of the buffer blocking probe on the parsebin srcpad pad */
  gulong buffer_probe_id;

  /* Whether we saw an EOS on input. This should be treated accordingly
   * when the stream is no longer used */
  gboolean saw_eos;
};

/* Multiqueue Slots */
typedef struct _MultiQueueSlot
{
  guint id;

  GstDecodebin3 *dbin;
  /* Type of stream handled by this slot */
  GstStreamType type;

  /* Linked input and output */
  DecodebinInputStream *input;

  /* pending => last stream received on sink pad */
  GstStream *pending_stream;
  /* active => last stream outputted on source pad */
  GstStream *active_stream;
  /* Cache of the stream_id of active_stream */
  const gchar *active_stream_id;


  GstPad *sink_pad, *src_pad;

  /* id of the MQ src_pad event probe */
  gulong probe_id;

  /* keyframe dropping probe */
  gulong drop_probe_id;

  /* TRUE if EOS was pushed out by multiqueue */
  gboolean is_drained;

  DecodebinOutputStream *output;
} MultiQueueSlot;

/* Streams that are exposed downstream (i.e. output) */
struct _DecodebinOutputStream
{
  GstDecodebin3 *dbin;
  /* The type of stream handled by this output stream */
  GstStreamType type;

  /* The slot to which this output stream is currently connected to */
  MultiQueueSlot *slot;

  GstElement *decoder;          /* Optional */
  GstPad *decoder_sink, *decoder_src;
  gboolean linked;

  /* ghostpad */
  GstPad *src_pad;
  /* Flag if ghost pad is exposed */
  gboolean src_exposed;

  /* Reported decoder latency */
  GstClockTime decoder_latency;
};

/* properties */
enum
{
  PROP_0,
  PROP_CAPS
};

/* signals */
enum
{
  SIGNAL_SELECT_STREAM,
  SIGNAL_ABOUT_TO_FINISH,
  LAST_SIGNAL
};
static guint gst_decodebin3_signals[LAST_SIGNAL] = { 0 };

#define SELECTION_LOCK(dbin) G_STMT_START {				\
    GST_LOG_OBJECT (dbin,						\
		    "selection locking from thread %p",			\
		    g_thread_self ());					\
    g_mutex_lock (&dbin->selection_lock);				\
    GST_LOG_OBJECT (dbin,						\
		    "selection locked from thread %p",			\
		    g_thread_self ());					\
  } G_STMT_END

#define SELECTION_UNLOCK(dbin) G_STMT_START {				\
    GST_LOG_OBJECT (dbin,						\
		    "selection unlocking from thread %p",		\
		    g_thread_self ());					\
    g_mutex_unlock (&dbin->selection_lock);				\
  } G_STMT_END

#define INPUT_LOCK(dbin) G_STMT_START {				\
    GST_LOG_OBJECT (dbin,						\
		    "input locking from thread %p",			\
		    g_thread_self ());					\
    g_mutex_lock (&dbin->input_lock);				\
    GST_LOG_OBJECT (dbin,						\
		    "input locked from thread %p",			\
		    g_thread_self ());					\
  } G_STMT_END

#define INPUT_UNLOCK(dbin) G_STMT_START {				\
    GST_LOG_OBJECT (dbin,						\
		    "input unlocking from thread %p",		\
		    g_thread_self ());					\
    g_mutex_unlock (&dbin->input_lock);				\
  } G_STMT_END

GType gst_decodebin3_get_type (void);
#define gst_decodebin3_parent_class parent_class
G_DEFINE_TYPE (GstDecodebin3, gst_decodebin3, GST_TYPE_BIN);
#define _do_init \
    GST_DEBUG_CATEGORY_INIT (decodebin3_debug, "decodebin3", 0, "decoder bin");\
    playback_element_init (plugin);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (decodebin3, "decodebin3", GST_RANK_NONE,
    GST_TYPE_DECODEBIN3, _do_init);

static GstStaticCaps default_raw_caps = GST_STATIC_CAPS (DEFAULT_RAW_CAPS);

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate request_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate video_src_template =
GST_STATIC_PAD_TEMPLATE ("video_%u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate audio_src_template =
GST_STATIC_PAD_TEMPLATE ("audio_%u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate text_src_template =
GST_STATIC_PAD_TEMPLATE ("text_%u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src_%u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);


static void gst_decodebin3_dispose (GObject * object);
static void gst_decodebin3_finalize (GObject * object);
static void gst_decodebin3_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_decodebin3_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean parsebin_autoplug_continue_cb (GstElement *
    parsebin, GstPad * pad, GstCaps * caps, GstDecodebin3 * dbin);

static gint
gst_decodebin3_select_stream (GstDecodebin3 * dbin,
    GstStreamCollection * collection, GstStream * stream)
{
  GST_LOG_OBJECT (dbin, "default select-stream, returning -1");

  return -1;
}

static GstPad *gst_decodebin3_request_new_pad (GstElement * element,
    GstPadTemplate * temp, const gchar * name, const GstCaps * caps);
static void gst_decodebin3_release_pad (GstElement * element, GstPad * pad);
static GstMessage *handle_stream_collection_locked (GstDecodebin3 * dbin,
    GstStreamCollection * collection, DecodebinInput * input);
static void gst_decodebin3_handle_message (GstBin * bin, GstMessage * message);
static GstStateChangeReturn gst_decodebin3_change_state (GstElement * element,
    GstStateChange transition);
static gboolean gst_decodebin3_send_event (GstElement * element,
    GstEvent * event);

static void gst_decode_bin_update_factories_list (GstDecodebin3 * dbin);

static DecodebinCollection *db_collection_new (GstStreamCollection *
    collection);
static void db_collection_free (DecodebinCollection * collection);

static void gst_decodebin_input_reset (DecodebinInput * input);
static void gst_decodebin_input_free (DecodebinInput * input);
static DecodebinInput *gst_decodebin_input_new (GstDecodebin3 * dbin,
    gboolean main);
static gboolean gst_decodebin_input_set_group_id (DecodebinInput * input,
    guint32 * group_id);
static void gst_decodebin_input_unblock_streams (DecodebinInput * input,
    gboolean unblock_other_inputs);
static void gst_decodebin_input_link_to_slot (DecodebinInputStream * input);

static gboolean db_output_stream_reconfigure (DecodebinOutputStream * output,
    GstMessage ** msg);
static void db_output_stream_reset (DecodebinOutputStream * output);
static void db_output_stream_free (DecodebinOutputStream * output);
static DecodebinOutputStream *db_output_stream_new (GstDecodebin3 * dbin,
    GstStreamType type);

static GstPadProbeReturn mq_slot_unassign_probe (GstPad * pad,
    GstPadProbeInfo * info, MultiQueueSlot * slot);
static void mq_slot_reassign (MultiQueueSlot * slot);
static MultiQueueSlot
    * gst_decodebin_get_slot_for_input_stream_locked (GstDecodebin3 * dbin,
    DecodebinInputStream * input);
static void mq_slot_free (GstDecodebin3 * dbin, MultiQueueSlot * slot);

static void handle_stream_switch (GstDecodebin3 * dbin);

static GstStreamCollection *get_merged_collection (GstDecodebin3 * dbin);
static void update_requested_selection (GstDecodebin3 * dbin,
    DecodebinCollection * new_collection);

static void parsebin_pad_added_cb (GstElement * demux, GstPad * pad,
    DecodebinInput * input);
static void parsebin_pad_removed_cb (GstElement * demux, GstPad * pad,
    DecodebinInput * input);

static gboolean
_gst_int_accumulator (GSignalInvocationHint * ihint,
    GValue * return_accu, const GValue * handler_return, gpointer dummy)
{
  gint res = g_value_get_int (handler_return);

  g_value_set_int (return_accu, res);

  if (res == -1)
    return TRUE;

  return FALSE;
}

static void
gst_decodebin3_class_init (GstDecodebin3Class * klass)
{
  GObjectClass *gobject_klass = (GObjectClass *) klass;
  GstElementClass *element_class = (GstElementClass *) klass;
  GstBinClass *bin_klass = (GstBinClass *) klass;

  gobject_klass->dispose = gst_decodebin3_dispose;
  gobject_klass->finalize = gst_decodebin3_finalize;
  gobject_klass->set_property = gst_decodebin3_set_property;
  gobject_klass->get_property = gst_decodebin3_get_property;

  g_object_class_install_property (gobject_klass, PROP_CAPS,
      g_param_spec_boxed ("caps", "Caps",
          "The caps on which to stop decoding. (NULL = default)",
          GST_TYPE_CAPS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstDecodebin3::select-stream
   * @decodebin: a #GstDecodebin3
   * @collection: a #GstStreamCollection
   * @stream: a #GstStream
   *
   * This signal is emitted whenever @decodebin needs to decide whether
   * to expose a @stream of a given @collection.
   *
   * Note that the prefered way to select streams is to listen to
   * GST_MESSAGE_STREAM_COLLECTION on the bus and send a
   * GST_EVENT_SELECT_STREAMS with the streams the user wants.
   *
   * Returns: 1 if the stream should be selected, 0 if it shouldn't be selected.
   * A value of -1 (default) lets @decodebin decide what to do with the stream.
   * */
  gst_decodebin3_signals[SIGNAL_SELECT_STREAM] =
      g_signal_new ("select-stream", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstDecodebin3Class, select_stream),
      _gst_int_accumulator, NULL, NULL,
      G_TYPE_INT, 2, GST_TYPE_STREAM_COLLECTION, GST_TYPE_STREAM);

  /**
   * GstDecodebin3::about-to-finish:
   *
   * This signal is emitted when the data for the selected URI is
   * entirely buffered and it is safe to specify another URI.
   */
  gst_decodebin3_signals[SIGNAL_ABOUT_TO_FINISH] =
      g_signal_new ("about-to-finish", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0, G_TYPE_NONE);


  element_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_decodebin3_request_new_pad);
  element_class->change_state = GST_DEBUG_FUNCPTR (gst_decodebin3_change_state);
  element_class->send_event = GST_DEBUG_FUNCPTR (gst_decodebin3_send_event);
  element_class->release_pad = GST_DEBUG_FUNCPTR (gst_decodebin3_release_pad);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&request_sink_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&video_src_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&audio_src_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&text_src_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template));

  gst_element_class_set_static_metadata (element_class,
      "Decoder Bin 3", "Generic/Bin/Decoder",
      "Autoplug and decode to raw media",
      "Edward Hervey <edward@centricular.com>");

  bin_klass->handle_message = gst_decodebin3_handle_message;

  klass->select_stream = gst_decodebin3_select_stream;
}

static void
gst_decodebin3_init (GstDecodebin3 * dbin)
{
  /* Create main input */
  dbin->main_input = gst_decodebin_input_new (dbin, TRUE);

  dbin->multiqueue = gst_element_factory_make ("multiqueue", NULL);
  g_object_get (dbin->multiqueue, "min-interleave-time",
      &dbin->default_mq_min_interleave, NULL);
  dbin->current_mq_min_interleave = dbin->default_mq_min_interleave;
  g_object_set (dbin->multiqueue, "sync-by-running-time", TRUE,
      "max-size-buffers", 0, "use-interleave", TRUE, NULL);
  gst_bin_add ((GstBin *) dbin, dbin->multiqueue);

  dbin->current_group_id = GST_GROUP_ID_INVALID;

  g_mutex_init (&dbin->factories_lock);
  g_mutex_init (&dbin->selection_lock);
  g_mutex_init (&dbin->input_lock);

  dbin->caps = gst_static_caps_get (&default_raw_caps);

  GST_OBJECT_FLAG_SET (dbin, GST_BIN_FLAG_STREAMS_AWARE);
}

static void
gst_decodebin3_reset (GstDecodebin3 * dbin)
{
  GList *tmp;

  GST_DEBUG_OBJECT (dbin, "Resetting");

  /* Free output streams */
  g_list_free_full (dbin->output_streams,
      (GDestroyNotify) db_output_stream_free);
  dbin->output_streams = NULL;

  /* Free multiqueue slots */
  for (tmp = dbin->slots; tmp; tmp = tmp->next) {
    MultiQueueSlot *slot = (MultiQueueSlot *) tmp->data;
    mq_slot_free (dbin, slot);
  }
  g_list_free (dbin->slots);
  dbin->slots = NULL;
  dbin->current_group_id = GST_GROUP_ID_INVALID;

  /* Reset the inputs */
  gst_decodebin_input_reset (dbin->main_input);
  for (tmp = dbin->other_inputs; tmp; tmp = tmp->next) {
    gst_decodebin_input_reset (tmp->data);
  }

  /* Reset multiqueue to default interleave */
  g_object_set (dbin->multiqueue, "min-interleave-time",
      dbin->default_mq_min_interleave, NULL);
  dbin->current_mq_min_interleave = dbin->default_mq_min_interleave;
  dbin->upstream_handles_selection = FALSE;

  g_list_free_full (dbin->collections, (GDestroyNotify) db_collection_free);
  dbin->collections = NULL;
  dbin->input_collection = dbin->output_collection = NULL;
}

static void
gst_decodebin3_dispose (GObject * object)
{
  GstDecodebin3 *dbin = (GstDecodebin3 *) object;

  gst_decodebin3_reset (dbin);

  g_mutex_lock (&dbin->factories_lock);
  if (dbin->factories) {
    gst_plugin_feature_list_free (dbin->factories);
    dbin->factories = NULL;
  }
  if (dbin->decoder_factories) {
    g_list_free (dbin->decoder_factories);
    dbin->decoder_factories = NULL;
  }
  if (dbin->decodable_factories) {
    g_list_free (dbin->decodable_factories);
    dbin->decodable_factories = NULL;
  }
  g_mutex_unlock (&dbin->factories_lock);

  INPUT_LOCK (dbin);
  if (dbin->main_input) {
    gst_decodebin_input_free (dbin->main_input);
    dbin->main_input = NULL;
  }

  g_list_free_full (dbin->other_inputs,
      (GDestroyNotify) gst_decodebin_input_free);
  dbin->other_inputs = NULL;
  INPUT_UNLOCK (dbin);

  gst_clear_caps (&dbin->caps);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_decodebin3_finalize (GObject * object)
{
  GstDecodebin3 *dbin = (GstDecodebin3 *) object;

  g_mutex_clear (&dbin->factories_lock);
  g_mutex_clear (&dbin->selection_lock);
  g_mutex_clear (&dbin->input_lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_decodebin3_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDecodebin3 *dbin = (GstDecodebin3 *) object;

  switch (prop_id) {
    case PROP_CAPS:
      GST_OBJECT_LOCK (dbin);
      if (dbin->caps)
        gst_caps_unref (dbin->caps);
      dbin->caps = g_value_dup_boxed (value);
      GST_OBJECT_UNLOCK (dbin);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_decodebin3_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstDecodebin3 *dbin = (GstDecodebin3 *) object;

  switch (prop_id) {
    case PROP_CAPS:
      GST_OBJECT_LOCK (dbin);
      g_value_set_boxed (value, dbin->caps);
      GST_OBJECT_UNLOCK (dbin);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* WITH SELECTION_LOCK TAKEN! */
static gboolean
all_input_streams_are_eos (GstDecodebin3 * dbin)
{
  GList *tmp;

  /* First check input streams */
  for (tmp = dbin->input_streams; tmp; tmp = tmp->next) {
    DecodebinInputStream *input = (DecodebinInputStream *) tmp->data;
    if (input->saw_eos == FALSE)
      return FALSE;
  }

  GST_DEBUG_OBJECT (dbin, "All input streams are EOS");
  return TRUE;
}

/** check_all_input_streams_for_eos:
 * @dbin: A #GstDecodebin3
 * @eos_event: (transfer none): The GST_EVENT_EOS that triggered this check
 *
 * Check if all inputs streams are EOS. If they are propagates the @eos_event to all
 * #DecodebinInputstream pads.
 *
 * Returns: #TRUE if all pads are EOS and the event was propagated, else #FALSE.
 */
static gboolean
check_all_input_streams_for_eos (GstDecodebin3 * dbin, GstEvent * eos_event)
{
  GList *tmp;
  GList *outputpads = NULL;

  SELECTION_LOCK (dbin);

  if (!all_input_streams_are_eos (dbin)) {
    SELECTION_UNLOCK (dbin);
    return FALSE;
  }

  /* We know all streams are EOS, properly clean up everything */

  /* We grab all peer pads *while* the selection lock is taken and then we will
     push EOS downstream with the selection lock released */
  for (tmp = dbin->input_streams; tmp; tmp = tmp->next) {
    DecodebinInputStream *input = (DecodebinInputStream *) tmp->data;
    GstPad *peer = gst_pad_get_peer (input->srcpad);

    /* Keep a reference to the peer pad */
    if (peer)
      outputpads = g_list_append (outputpads, peer);
  }

  SELECTION_UNLOCK (dbin);

  for (tmp = outputpads; tmp; tmp = tmp->next) {
    GstPad *peer = (GstPad *) tmp->data;

    /* Send EOS and then remove elements */
    gst_pad_send_event (peer, gst_event_ref (eos_event));
    GST_FIXME_OBJECT (peer, "Remove input stream");
    gst_object_unref (peer);
  }

  g_list_free (outputpads);

  return TRUE;
}

/* Get the intersection of parser caps and available (sorted) decoders */
static GstCaps *
get_parser_caps_filter (GstDecodebin3 * dbin, GstCaps * caps)
{
  GList *tmp;
  GstCaps *filter_caps;

  /* If no filter was provided, it can handle anything */
  if (!caps || gst_caps_is_any (caps))
    return gst_caps_new_any ();

  filter_caps = gst_caps_new_empty ();

  g_mutex_lock (&dbin->factories_lock);
  gst_decode_bin_update_factories_list (dbin);
  for (tmp = dbin->decoder_factories; tmp; tmp = tmp->next) {
    GstElementFactory *factory = (GstElementFactory *) tmp->data;
    GstCaps *tcaps, *intersection;
    const GList *tmps;

    GST_LOG ("Trying factory %s",
        gst_plugin_feature_get_name (GST_PLUGIN_FEATURE (factory)));
    for (tmps = gst_element_factory_get_static_pad_templates (factory); tmps;
        tmps = tmps->next) {
      GstStaticPadTemplate *st = (GstStaticPadTemplate *) tmps->data;
      if (st->direction != GST_PAD_SINK || st->presence != GST_PAD_ALWAYS)
        continue;
      tcaps = gst_static_pad_template_get_caps (st);
      intersection =
          gst_caps_intersect_full (tcaps, caps, GST_CAPS_INTERSECT_FIRST);
      filter_caps = gst_caps_merge (filter_caps, intersection);
      gst_caps_unref (tcaps);
    }
  }
  g_mutex_unlock (&dbin->factories_lock);
  GST_DEBUG_OBJECT (dbin, "Got filter caps %" GST_PTR_FORMAT, filter_caps);
  return filter_caps;
}

static gboolean
check_parser_caps_filter (GstDecodebin3 * dbin, GstCaps * caps)
{
  GList *tmp;
  gboolean res = FALSE;
  GstCaps *default_raw = gst_static_caps_get (&default_raw_caps);

  if (gst_caps_can_intersect (caps, default_raw)) {
    GST_INFO_OBJECT (dbin, "Dealing with raw stream from the demuxer, "
        " we can handle them even if we won't expose then");
    gst_caps_unref (default_raw);

    return TRUE;
  }
  gst_caps_unref (default_raw);

  g_mutex_lock (&dbin->factories_lock);
  gst_decode_bin_update_factories_list (dbin);
  for (tmp = dbin->decoder_factories; tmp; tmp = tmp->next) {
    GstElementFactory *factory = (GstElementFactory *) tmp->data;
    GstCaps *tcaps;
    const GList *tmps;

    GST_LOG ("Trying factory %s",
        gst_plugin_feature_get_name (GST_PLUGIN_FEATURE (factory)));
    for (tmps = gst_element_factory_get_static_pad_templates (factory); tmps;
        tmps = tmps->next) {
      GstStaticPadTemplate *st = (GstStaticPadTemplate *) tmps->data;
      if (st->direction != GST_PAD_SINK || st->presence != GST_PAD_ALWAYS)
        continue;
      tcaps = gst_static_pad_template_get_caps (st);
      if (gst_caps_can_intersect (tcaps, caps)) {
        res = TRUE;
        gst_caps_unref (tcaps);
        goto beach;
      }
      gst_caps_unref (tcaps);
    }
  }
beach:
  g_mutex_unlock (&dbin->factories_lock);
  GST_DEBUG_OBJECT (dbin, "Can intersect %" GST_PTR_FORMAT ": %d", caps, res);
  return res;
}

/* Probe on the output of a decodebin input stream (from parsebin or identity) */
static GstPadProbeReturn
gst_decodebin_input_stream_src_probe (GstPad * pad, GstPadProbeInfo * info,
    DecodebinInputStream * input)
{
  GstPadProbeReturn ret = GST_PAD_PROBE_OK;

  if (GST_IS_EVENT (GST_PAD_PROBE_INFO_DATA (info))) {
    GstEvent *ev = GST_PAD_PROBE_INFO_EVENT (info);

    GST_DEBUG_OBJECT (pad, "Got event %s", GST_EVENT_TYPE_NAME (ev));
    switch (GST_EVENT_TYPE (ev)) {
      case GST_EVENT_STREAM_START:
      {
        GstStream *stream = NULL;
        guint group_id = GST_GROUP_ID_INVALID;

        if (!gst_event_parse_group_id (ev, &group_id)) {
          GST_FIXME_OBJECT (pad,
              "Consider implementing group-id handling on stream-start event");
          group_id = gst_util_group_id_next ();
        }

        GST_DEBUG_OBJECT (pad, "Got stream-start, group_id:%d, input %p",
            group_id, input->input);
        if (gst_decodebin_input_set_group_id (input->input, &group_id)) {
          ev = gst_event_make_writable (ev);
          gst_event_set_group_id (ev, group_id);
          GST_PAD_PROBE_INFO_DATA (info) = ev;
        }
        input->saw_eos = FALSE;

        gst_event_parse_stream (ev, &stream);
        /* FIXME : Would we ever end up with a stream already set on the input ?? */
        if (stream) {
          if (input->active_stream != stream) {
            if (input->active_stream)
              gst_object_unref (input->active_stream);
            input->active_stream = stream;
            /* We have the beginning of a stream, get a multiqueue slot and link to it */
            SELECTION_LOCK (input->dbin);
            gst_decodebin_input_link_to_slot (input);
            SELECTION_UNLOCK (input->dbin);
          } else
            gst_object_unref (stream);
        }
      }
        break;
      case GST_EVENT_GAP:
      {
        /* If we are still waiting to be unblocked and we get a gap, unblock */
        if (input->buffer_probe_id) {
          GST_DEBUG_OBJECT (pad, "Got a gap event! Unblocking input(s) !");
          gst_decodebin_input_unblock_streams (input->input, TRUE);
        }
        break;
      }
      case GST_EVENT_CAPS:
      {
        GstCaps *caps = NULL;
        gst_event_parse_caps (ev, &caps);
        GST_DEBUG_OBJECT (pad, "caps %" GST_PTR_FORMAT, caps);
        if (caps && input->active_stream)
          gst_stream_set_caps (input->active_stream, caps);
      }
        break;
      case GST_EVENT_EOS:
      {
        GST_DEBUG_OBJECT (pad, "Marking input as EOS");
        input->saw_eos = TRUE;

        /* If not all pads are EOS yet, we send our custom EOS (which will be
         * handled/dropped downstream of multiqueue) */
        if (!check_all_input_streams_for_eos (input->dbin, ev)) {
          GstPad *peer = gst_pad_get_peer (input->srcpad);
          if (peer) {
            /* Send custom-eos event to multiqueue slot */
            GstEvent *event;

            GST_DEBUG_OBJECT (pad,
                "Got EOS end of input stream, post custom-eos");
            event = gst_event_new_eos ();
            gst_event_set_seqnum (event, gst_event_get_seqnum (ev));
            gst_mini_object_set_qdata (GST_MINI_OBJECT_CAST (event),
                CUSTOM_EOS_QUARK, (gchar *) CUSTOM_EOS_QUARK_DATA, NULL);
            gst_pad_send_event (peer, event);
            gst_object_unref (peer);
          } else {
            GST_FIXME_OBJECT (pad, "No peer, what should we do ?");
          }
        }
        ret = GST_PAD_PROBE_DROP;
      }
        break;
      case GST_EVENT_FLUSH_STOP:
        GST_DEBUG_OBJECT (pad, "Clear saw_eos flag");
        input->saw_eos = FALSE;
      default:
        break;
    }
  } else if (GST_IS_QUERY (GST_PAD_PROBE_INFO_DATA (info))) {
    if (input->input && input->input->identity) {
      GST_DEBUG_OBJECT (pad, "Letting query through");
    } else {
      GstQuery *q = GST_PAD_PROBE_INFO_QUERY (info);
      GST_DEBUG_OBJECT (pad, "Seeing query %" GST_PTR_FORMAT, q);
      /* If we have a parser, we want to reply to the caps query */
      /* FIXME: Set a flag when the input stream is created for
       * streams where we shouldn't reply to these queries */
      if (GST_QUERY_TYPE (q) == GST_QUERY_CAPS
          && (info->type & GST_PAD_PROBE_TYPE_PULL)) {
        GstCaps *filter = NULL;
        GstCaps *allowed;
        gst_query_parse_caps (q, &filter);
        allowed = get_parser_caps_filter (input->dbin, filter);
        GST_DEBUG_OBJECT (pad,
            "Intercepting caps query, setting %" GST_PTR_FORMAT, allowed);
        gst_query_set_caps_result (q, allowed);
        gst_caps_unref (allowed);
        ret = GST_PAD_PROBE_HANDLED;
      } else if (GST_QUERY_TYPE (q) == GST_QUERY_ACCEPT_CAPS) {
        GstCaps *prop = NULL;
        gst_query_parse_accept_caps (q, &prop);
        /* Fast check against target caps */
        if (gst_caps_can_intersect (prop, input->dbin->caps)) {
          gst_query_set_accept_caps_result (q, TRUE);
        } else {
          gboolean accepted = check_parser_caps_filter (input->dbin, prop);
          /* check against caps filter */
          gst_query_set_accept_caps_result (q, accepted);
          GST_DEBUG_OBJECT (pad, "ACCEPT_CAPS query, returning %d", accepted);
        }
        ret = GST_PAD_PROBE_HANDLED;
      }
    }
  }

  return ret;
}

static GstPadProbeReturn
gst_decodebin_input_stream_buffer_probe (GstPad * pad, GstPadProbeInfo * info,
    DecodebinInput * input)
{
  /* We have at least one buffer pending, unblock parsebin/identity pads */
  GST_DEBUG_OBJECT (pad, "Got a buffer ! unblocking");
  gst_decodebin_input_unblock_streams (input, TRUE);

  return GST_PAD_PROBE_OK;
}

/** gst_decodebin_input_add_stream:
 * @input: A #DecodebinInput
 * @pad: (transfer none): The #GstPad that generates this stream
 * @stream: (allow none) (transfer full): The #GstStream if known
 *
 * Creates a new #DecodebinInputstream for the given @pad and @stream, and adds
 * it to the list of #GstDecodebin3 input_streams.
 *
 * Returns: The new #DecodebinInputstream.
 */
/* Call with selection lock */
static DecodebinInputStream *
gst_decodebin_input_add_stream (DecodebinInput * input, GstPad * pad,
    GstStream * stream)
{
  GstDecodebin3 *dbin = input->dbin;
  DecodebinInputStream *res = g_new0 (DecodebinInputStream, 1);

  GST_DEBUG_OBJECT (dbin, "Creating input stream for %" GST_PTR_FORMAT, pad);

  res->dbin = dbin;
  res->input = input;
  res->srcpad = gst_object_ref (pad);
  res->active_stream = stream;

  /* Put probe on output source pad (for detecting EOS/STREAM_START/FLUSH) */
  res->output_event_probe_id =
      gst_pad_add_probe (pad,
      GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM | GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM
      | GST_PAD_PROBE_TYPE_EVENT_FLUSH,
      (GstPadProbeCallback) gst_decodebin_input_stream_src_probe, res, NULL);

  /* Install a blocking buffer probe */
  res->buffer_probe_id =
      gst_pad_add_probe (pad,
      GST_PAD_PROBE_TYPE_BLOCK | GST_PAD_PROBE_TYPE_BUFFER,
      (GstPadProbeCallback) gst_decodebin_input_stream_buffer_probe, input,
      NULL);

  SELECTION_LOCK (dbin);
  /* Add to list of current input streams */
  dbin->input_streams = g_list_append (dbin->input_streams, res);
  SELECTION_UNLOCK (dbin);
  GST_DEBUG_OBJECT (pad, "Done creating input stream");

  return res;
}

/* WITH SELECTION_LOCK TAKEN! */
static void
remove_input_stream (GstDecodebin3 * dbin, DecodebinInputStream * stream)
{
  MultiQueueSlot *slot;

  GST_DEBUG_OBJECT (dbin, "Removing input stream %p %" GST_PTR_FORMAT, stream,
      stream->active_stream);

  gst_object_replace ((GstObject **) & stream->active_stream, NULL);

  /* Unlink from slot */
  if (stream->srcpad) {
    GstPad *peer;
    peer = gst_pad_get_peer (stream->srcpad);
    if (peer) {
      gst_pad_unlink (stream->srcpad, peer);
      gst_object_unref (peer);
    }
    if (stream->buffer_probe_id)
      gst_pad_remove_probe (stream->srcpad, stream->buffer_probe_id);
    gst_object_unref (stream->srcpad);
  }

  slot = gst_decodebin_get_slot_for_input_stream_locked (dbin, stream);
  if (slot) {
    slot->pending_stream = NULL;
    slot->input = NULL;
    GST_DEBUG_OBJECT (dbin, "slot %p cleared", slot);
  }

  dbin->input_streams = g_list_remove (dbin->input_streams, stream);

  g_free (stream);
}

/** gst_decodebin_input_unblock_streams:
 * @input: A #DecodebinInput
 * @unblock_other_inputs: Whether to also unblock other #DecodebinInput targetting the same #GstStreamCollection
 *
 * Unblock all #DecodebinInputstream for the given @input. If
 * @unblock_other_inputs is TRUE, it will also unblock other #DecodebinInput
 * targetting the same #GstStreamCollection.
 */
static void
gst_decodebin_input_unblock_streams (DecodebinInput * input,
    gboolean unblock_other_inputs)
{
  GstDecodebin3 *dbin = input->dbin;
  GList *tmp, *unused_slot_sinkpads = NULL;

  GST_DEBUG_OBJECT (dbin,
      "DecodebinInput for %" GST_PTR_FORMAT " , unblock_other_inputs:%d",
      input->parsebin, unblock_other_inputs);

  /* Re-use existing streams if/when possible */
  GST_FIXME_OBJECT (dbin, "Re-use existing input streams if/when possible");

  /* Unblock all input streams and link to a slot if needed */
  SELECTION_LOCK (dbin);
  tmp = dbin->input_streams;
  while (tmp != NULL) {
    DecodebinInputStream *input_stream = (DecodebinInputStream *) tmp->data;
    GList *next = tmp->next;

    if (input_stream->input != input) {
      tmp = next;
      continue;
    }

    GST_DEBUG_OBJECT (dbin, "Checking input stream %p", input_stream);

    if (!input_stream->active_stream)
      input_stream->active_stream = gst_pad_get_stream (input_stream->srcpad);

    /* Ensure the stream is linked to a slot */
    gst_decodebin_input_link_to_slot (input_stream);

    if (input_stream->buffer_probe_id) {
      GST_DEBUG_OBJECT (dbin,
          "Removing pad block on input %p pad %" GST_PTR_FORMAT, input_stream,
          input_stream->srcpad);
      gst_pad_remove_probe (input_stream->srcpad,
          input_stream->buffer_probe_id);
      input_stream->buffer_probe_id = 0;
    }

    if (input_stream->saw_eos) {
      GST_DEBUG_OBJECT (dbin, "Removing EOS'd stream");
      remove_input_stream (dbin, input_stream);
      tmp = dbin->input_streams;
    } else
      tmp = next;
  }

  /* Weed out unused multiqueue slots */
  for (tmp = dbin->slots; tmp; tmp = tmp->next) {
    MultiQueueSlot *slot = (MultiQueueSlot *) tmp->data;
    GST_LOG_OBJECT (dbin, "Slot %d input:%p", slot->id, slot->input);
    if (slot->input == NULL) {
      unused_slot_sinkpads =
          g_list_append (unused_slot_sinkpads, gst_object_ref (slot->sink_pad));
    }
  }
  SELECTION_UNLOCK (dbin);

  if (unused_slot_sinkpads) {
    for (tmp = unused_slot_sinkpads; tmp; tmp = tmp->next) {
      GstPad *sink_pad = (GstPad *) tmp->data;
      GST_DEBUG_OBJECT (sink_pad, "Sending EOS to unused slot");
      gst_pad_send_event (sink_pad, gst_event_new_eos ());
    }
    g_list_free_full (unused_slot_sinkpads, (GDestroyNotify) gst_object_unref);
  }

  if (unblock_other_inputs) {
    GList *tmp;
    /* If requrested, unblock inputs which are targetting the same collection */
    if (dbin->main_input != input) {
      if (dbin->main_input->collection == input->collection) {
        GST_DEBUG_OBJECT (dbin, "Unblock main input");
        gst_decodebin_input_unblock_streams (dbin->main_input, FALSE);
      }
    }
    for (tmp = dbin->other_inputs; tmp; tmp = tmp->next) {
      DecodebinInput *other = tmp->data;
      if (other->collection == input->collection) {
        GST_DEBUG_OBJECT (dbin, "Unblock other input");
        gst_decodebin_input_unblock_streams (other, FALSE);
      }
    }
  }
}

static void
parsebin_pad_added_cb (GstElement * demux, GstPad * pad, DecodebinInput * input)
{
  GstDecodebin3 *dbin = input->dbin;

  GST_DEBUG_OBJECT (dbin, "New pad %s:%s (input:%p)", GST_DEBUG_PAD_NAME (pad),
      input);

  gst_decodebin_input_add_stream (input, pad, NULL);
}

/* WITH SELECTION_LOCK TAKEN! */
static DecodebinInputStream *
find_input_stream_for_pad (GstDecodebin3 * dbin, GstPad * pad)
{
  GList *tmp;

  for (tmp = dbin->input_streams; tmp; tmp = tmp->next) {
    DecodebinInputStream *cand = (DecodebinInputStream *) tmp->data;
    if (cand->srcpad == pad)
      return cand;
  }
  return NULL;
}

/* Must be called with the selection lock taken */
static void
gst_decodebin3_update_min_interleave (GstDecodebin3 * dbin)
{
  GstClockTime max_latency = GST_CLOCK_TIME_NONE;
  GList *tmp;

  GST_DEBUG_OBJECT (dbin, "Recalculating max latency of decoders");
  for (tmp = dbin->output_streams; tmp; tmp = tmp->next) {
    DecodebinOutputStream *out = (DecodebinOutputStream *) tmp->data;
    if (GST_CLOCK_TIME_IS_VALID (out->decoder_latency)) {
      if (max_latency == GST_CLOCK_TIME_NONE
          || out->decoder_latency > max_latency)
        max_latency = out->decoder_latency;
    }
  }
  GST_DEBUG_OBJECT (dbin, "max latency of all decoders: %" GST_TIME_FORMAT,
      GST_TIME_ARGS (max_latency));

  if (!GST_CLOCK_TIME_IS_VALID (max_latency))
    return;

  /* Make sure we keep an extra overhead */
  max_latency += 100 * GST_MSECOND;
  if (max_latency == dbin->current_mq_min_interleave)
    return;

  dbin->current_mq_min_interleave = max_latency;
  GST_DEBUG_OBJECT (dbin, "Setting mq min-interleave to %" GST_TIME_FORMAT,
      GST_TIME_ARGS (dbin->current_mq_min_interleave));
  g_object_set (dbin->multiqueue, "min-interleave-time",
      dbin->current_mq_min_interleave, NULL);
}

/** remove_slot_from_streaming_thread:
 * @dbin: A #GstDecodebin3
 * @slot: The #MultiQueueslot to remove
 *
 * Remove a #MultiQueueslot and associated output. Call this when done from a
 * multiqueue streaming thread.
 *
 * Must be called with the SELECTION_LOCK taken.
 */
static void
remove_slot_from_streaming_thread (GstDecodebin3 * dbin, MultiQueueSlot * slot)
{
  /* if slot is still there and already drained, remove it in here */
  if (slot->output) {
    DecodebinOutputStream *output = slot->output;
    GST_DEBUG_OBJECT (slot->src_pad,
        "Multiqueue slot is drained, Remove output stream");

    dbin->output_streams = g_list_remove (dbin->output_streams, output);
    db_output_stream_free (output);
  }

  GST_DEBUG_OBJECT (slot->src_pad, "No pending pad, Remove multiqueue slot");
  if (slot->probe_id)
    gst_pad_remove_probe (slot->src_pad, slot->probe_id);
  slot->probe_id = 0;
  dbin->slots = g_list_remove (dbin->slots, slot);

  /* The minimum interleave might have changed, recalculate it */
  gst_decodebin3_update_min_interleave (dbin);

  gst_element_call_async (GST_ELEMENT_CAST (dbin),
      (GstElementCallAsyncFunc) mq_slot_free, slot, NULL);
}

static void
parsebin_pad_removed_cb (GstElement * demux, GstPad * pad, DecodebinInput * inp)
{
  GstDecodebin3 *dbin = inp->dbin;
  DecodebinInputStream *input = NULL;
  MultiQueueSlot *slot;

  if (!GST_PAD_IS_SRC (pad))
    return;

  SELECTION_LOCK (dbin);

  GST_DEBUG_OBJECT (pad, "removed");
  input = find_input_stream_for_pad (dbin, pad);

  if (input == NULL) {
    GST_DEBUG_OBJECT (pad,
        "Input stream not found, it was cleaned-up earlier after receiving EOS");
    SELECTION_UNLOCK (dbin);
    return;
  }

  /* If there are no pending pads, this means we will definitely not need this
   * stream anymore */

  GST_DEBUG_OBJECT (pad, "Remove input stream %p", input);

  slot = gst_decodebin_get_slot_for_input_stream_locked (dbin, input);
  remove_input_stream (dbin, input);

  if (slot && slot->is_drained)
    remove_slot_from_streaming_thread (dbin, slot);

  SELECTION_UNLOCK (dbin);
}

static gboolean
parsebin_autoplug_continue_cb (GstElement * parsebin, GstPad * pad,
    GstCaps * caps, GstDecodebin3 * dbin)
{
  GST_DEBUG_OBJECT (pad, "caps %" GST_PTR_FORMAT, caps);

  /* If it matches our target caps, expose it */
  if (gst_caps_can_intersect (caps, dbin->caps))
    return FALSE;

  return TRUE;
}

/** gst_decodebin_input_set_group_id:
 * @input: A #DecodebinInput
 * @group_id: The new group_id received on that input
 *
 * This method should be called whenever a STREAM_START event comes out of a
 * given input (via parsebin or identity).
 *
 * It will update the input group_id if needed, and also compute and update the
 * current_group_id of #GstDecodebin3.
 *
 * Returns: TRUE if the caller shall replace the group_id
*/
static gboolean
gst_decodebin_input_set_group_id (DecodebinInput * input, guint32 * group_id)
{
  GstDecodebin3 *dbin = input->dbin;

  if (input->group_id != *group_id) {
    if (input->group_id != GST_GROUP_ID_INVALID)
      GST_WARNING_OBJECT (dbin,
          "Group id changed (%" G_GUINT32_FORMAT " -> %" G_GUINT32_FORMAT
          ") on input %p ", input->group_id, *group_id, input);
    input->group_id = *group_id;
  }

  if (*group_id != dbin->current_group_id) {
    /* The input is being re-used with a different incoming stream, we do want
     * to change/unify to this new group-id */
    if (dbin->current_group_id == GST_GROUP_ID_INVALID) {
      GST_DEBUG_OBJECT (dbin,
          "Setting current group id to %" G_GUINT32_FORMAT, *group_id);
      dbin->current_group_id = *group_id;
    } else {
      GST_DEBUG_OBJECT (dbin, "Returning global group id %" G_GUINT32_FORMAT,
          dbin->current_group_id);
    }
    *group_id = dbin->current_group_id;
    return TRUE;
  }

  return FALSE;
}

static void
parsebin_drained_cb (GstElement * parsebin, DecodebinInput * input)
{
  GstDecodebin3 *dbin = input->dbin;
  gboolean all_drained;
  GList *tmp;

  GST_INFO_OBJECT (dbin, "input %p drained", input);
  input->drained = TRUE;

  all_drained = dbin->main_input->drained;
  for (tmp = dbin->other_inputs; tmp; tmp = tmp->next) {
    DecodebinInput *data = (DecodebinInput *) tmp->data;

    all_drained &= data->drained;
  }

  if (all_drained) {
    GST_INFO_OBJECT (dbin, "All inputs drained. Posting about-to-finish");
    g_signal_emit (dbin, gst_decodebin3_signals[SIGNAL_ABOUT_TO_FINISH], 0,
        NULL);
  }
}

/** gst_decodebin_input_ensure_parsebin:
 * @input: A #DecodebinInput
 *
 * Ensure the given @input has a parsebin properly setup for it.
 *
 * Returns: TRUE if parsebin could be properly set, else FALSE.
 */
/* Call with INPUT_LOCK taken */
static gboolean
gst_decodebin_input_ensure_parsebin (DecodebinInput * input)
{
  GstDecodebin3 *dbin = input->dbin;
  gboolean set_state = FALSE;

  if (input->parsebin == NULL) {
    input->parsebin = gst_element_factory_make ("parsebin", NULL);
    if (input->parsebin == NULL)
      goto no_parsebin;
    input->parsebin = gst_object_ref (input->parsebin);
    input->parsebin_sink = gst_element_get_static_pad (input->parsebin, "sink");
    input->pad_added_sigid =
        g_signal_connect (input->parsebin, "pad-added",
        (GCallback) parsebin_pad_added_cb, input);
    input->pad_removed_sigid =
        g_signal_connect (input->parsebin, "pad-removed",
        (GCallback) parsebin_pad_removed_cb, input);
    input->drained_sigid =
        g_signal_connect (input->parsebin, "drained",
        (GCallback) parsebin_drained_cb, input);
    g_signal_connect (input->parsebin, "autoplug-continue",
        (GCallback) parsebin_autoplug_continue_cb, dbin);
  }

  if (GST_OBJECT_PARENT (GST_OBJECT (input->parsebin)) != GST_OBJECT (dbin)) {
    /* The state lock is taken so that we ensure we are the one (de)activating
     * parsebin. We need to do this to ensure any activation taking place in
     * parsebin (including by elements doing upstream activation) are done
     * within the same thread. */
    GST_STATE_LOCK (input->parsebin);
    gst_bin_add (GST_BIN (dbin), input->parsebin);
    set_state = TRUE;
  }

  gst_ghost_pad_set_target (GST_GHOST_PAD (input->ghost_sink),
      input->parsebin_sink);

  if (set_state) {
    gst_element_sync_state_with_parent (input->parsebin);
    GST_STATE_UNLOCK (input->parsebin);
  }

  return TRUE;

  /* ERRORS */
no_parsebin:
  {
    gst_element_post_message ((GstElement *) dbin,
        gst_missing_element_message_new ((GstElement *) dbin, "parsebin"));
    return FALSE;
  }
}

static GstPadProbeReturn
input_pad_buffer_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer user_data)
{
  DecodebinInput *input = user_data;

  INPUT_LOCK (input->dbin);
  if (!input->parsebin && !input->identity) {
    GST_DEBUG_OBJECT (pad, "Push-stream without caps, setting up identity");
    gst_decodebin_input_ensure_parsebin (input);
  }
  input->input_probe = 0;
  INPUT_UNLOCK (input->dbin);

  return GST_PAD_PROBE_REMOVE;
};

static GstPadLinkReturn
gst_decodebin3_input_pad_link (GstPad * pad, GstObject * parent, GstPad * peer)
{
  GstDecodebin3 *dbin = (GstDecodebin3 *) parent;
  GstQuery *query;
  gboolean pull_mode = FALSE;
  GstPadLinkReturn res = GST_PAD_LINK_OK;
  DecodebinInput *input = g_object_get_data (G_OBJECT (pad), "decodebin.input");

  g_return_val_if_fail (input, GST_PAD_LINK_REFUSED);

  GST_LOG_OBJECT (parent, "Got link on input pad %" GST_PTR_FORMAT, pad);

  query = gst_query_new_scheduling ();
  if (gst_pad_query (peer, query)
      && gst_query_has_scheduling_mode_with_flags (query, GST_PAD_MODE_PULL,
          GST_SCHEDULING_FLAG_SEEKABLE))
    pull_mode = TRUE;
  gst_query_unref (query);

  GST_DEBUG_OBJECT (dbin, "Upstream can do pull-based : %d", pull_mode);

  /* If upstream *can* do pull-based we always use a parsebin. If not, we will
   * delay that decision to a later stage (caps/stream/collection event
   * processing) to figure out if one is really needed or whether an identity
   * element will be enough */
  INPUT_LOCK (dbin);
  if (pull_mode) {
    if (!gst_decodebin_input_ensure_parsebin (input))
      res = GST_PAD_LINK_REFUSED;
    else if (input->identity) {
      GST_ERROR_OBJECT (dbin,
          "Can't reconfigure input from push-based to pull-based");
      res = GST_PAD_LINK_REFUSED;
    }
  } else if (input->input_probe == 0) {
    /* We setup a buffer probe to handle the corner case of push-based
     * time-based inputs without CAPS/COLLECTION. If we get a buffer without
     * having figured out if we need identity or parsebin, we will plug in
     * parsebin */
    GST_DEBUG_OBJECT (pad, "Setting up buffer probe");
    input->input_probe =
        gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER,
        input_pad_buffer_probe, input, NULL);
  }

  /* Clear stream-collection corresponding to current INPUT.  We do not
   * recalculate the global one yet, it will be done when at least one
   * collection is received/computed for this input.
   */
  if (input->collection) {
    GST_DEBUG_OBJECT (pad, "Clearing input collection");
    gst_object_unref (input->collection);
    input->collection = NULL;
  }

  INPUT_UNLOCK (dbin);

  return res;
}

/* Drop duration query during _input_pad_unlink */
static GstPadProbeReturn
query_duration_drop_probe (GstPad * pad, GstPadProbeInfo * info,
    DecodebinInput * input)
{
  GstPadProbeReturn ret = GST_PAD_PROBE_OK;

  if (GST_IS_QUERY (GST_PAD_PROBE_INFO_DATA (info))) {
    GstQuery *query = GST_PAD_PROBE_INFO_QUERY (info);
    if (GST_QUERY_TYPE (query) == GST_QUERY_DURATION) {
      GST_LOG_OBJECT (pad, "stop forwarding query duration");
      ret = GST_PAD_PROBE_HANDLED;
    }
  }

  return ret;
}

/* CALL with INPUT LOCK */
static void
recalculate_group_id (GstDecodebin3 * dbin)
{
  guint32 common_group_id;
  GList *iter;

  GST_DEBUG_OBJECT (dbin,
      "recalculating, current global group_id: %" G_GUINT32_FORMAT,
      dbin->current_group_id);

  common_group_id = dbin->main_input->group_id;

  for (iter = dbin->other_inputs; iter; iter = iter->next) {
    DecodebinInput *input = iter->data;

    if (input->group_id != common_group_id) {
      if (common_group_id != GST_GROUP_ID_INVALID)
        return;

      common_group_id = input->group_id;
    }
  }

  if (common_group_id == dbin->current_group_id) {
    GST_DEBUG_OBJECT (dbin, "Global group_id hasn't changed");
  } else {
    GST_DEBUG_OBJECT (dbin, "Updating global group_id to %" G_GUINT32_FORMAT,
        common_group_id);
    dbin->current_group_id = common_group_id;
  }
}

/** gst_decodebin_input_reset_parsebin:
 * @dbin:
 * @input: A #DecodebinInput
 *
 * Reset the parsebin of @input (if any) by resetting all associated variables,
 * inputstreams and elements.
 *
 * Call with INPUT LOCK taken
 */
static void
gst_decodebin_input_reset_parsebin (GstDecodebin3 * dbin,
    DecodebinInput * input)
{
  GList *iter;

  if (input->parsebin == NULL)
    return;

  GST_DEBUG_OBJECT (dbin, "Resetting %" GST_PTR_FORMAT, input->parsebin);

  GST_STATE_LOCK (dbin);
  gst_element_set_state (input->parsebin, GST_STATE_NULL);
  input->drained = FALSE;
  input->group_id = GST_GROUP_ID_INVALID;
  recalculate_group_id (dbin);
  for (iter = dbin->input_streams; iter; iter = iter->next) {
    DecodebinInputStream *istream = iter->data;
    if (istream->input == input)
      istream->saw_eos = TRUE;
  }
  gst_element_sync_state_with_parent (input->parsebin);
  GST_STATE_UNLOCK (dbin);
}


static void
gst_decodebin3_input_pad_unlink (GstPad * pad, GstPad * peer,
    DecodebinInput * input)
{
  GstDecodebin3 *dbin = input->dbin;

  g_return_if_fail (input);

  GST_LOG_OBJECT (dbin, "Got unlink on input pad %" GST_PTR_FORMAT, pad);

  INPUT_LOCK (dbin);

  if (input->parsebin && GST_PAD_MODE (pad) == GST_PAD_MODE_PULL) {
    GST_DEBUG_OBJECT (dbin, "Resetting parsebin since it's pull-based");
    gst_decodebin_input_reset_parsebin (dbin, input);
  }
  /* In all cases we will be receiving new stream-start and data */
  input->group_id = GST_GROUP_ID_INVALID;
  input->drained = FALSE;
  recalculate_group_id (dbin);

  INPUT_UNLOCK (dbin);
}

static void
gst_decodebin3_release_pad (GstElement * element, GstPad * pad)
{
  GstDecodebin3 *dbin = (GstDecodebin3 *) element;
  DecodebinInput *input = g_object_get_data (G_OBJECT (pad), "decodebin.input");
  gulong probe_id = 0;
  GstMessage *msg;

  g_return_if_fail (input);
  GST_LOG_OBJECT (dbin, "Releasing pad %" GST_PTR_FORMAT, pad);

  INPUT_LOCK (dbin);

  /* Clear stream-collection corresponding to current INPUT and post new
   * stream-collection message, if needed */
  if (input->collection) {
    gst_object_unref (input->collection);
    input->collection = NULL;
  }

  msg = handle_stream_collection_locked (dbin, NULL, input);

  if (msg) {
    if (input->parsebin)
      /* Drop duration queries that the application might be doing while this
       * message is posted */
      probe_id =
          gst_pad_add_probe (input->parsebin_sink,
          GST_PAD_PROBE_TYPE_QUERY_UPSTREAM,
          (GstPadProbeCallback) query_duration_drop_probe, input, NULL);

    gst_element_post_message (GST_ELEMENT_CAST (dbin), msg);

    if (input->parsebin) {
      gst_pad_remove_probe (input->parsebin_sink, probe_id);
    }
  }

  if (!input->is_main) {
    dbin->other_inputs = g_list_remove (dbin->other_inputs, input);
    gst_decodebin_input_free (input);
  } else
    gst_decodebin_input_reset (input);

  INPUT_UNLOCK (dbin);
  return;
}

/** gst_decodebin_input_reset:
 * @input: The #DecodebinInput to reset
 *
 * Resets the @input for re-used. Call with the INPUT_LOCK.
 */
static void
gst_decodebin_input_reset (DecodebinInput * input)
{
  GstDecodebin3 *dbin = input->dbin;

  g_return_if_fail (dbin);

  GST_LOG_OBJECT (dbin, "Resetting input %p", input);

  gst_ghost_pad_set_target (GST_GHOST_PAD (input->ghost_sink), NULL);

  if (input->parsebin) {
    g_signal_handler_disconnect (input->parsebin, input->pad_removed_sigid);
    g_signal_handler_disconnect (input->parsebin, input->pad_added_sigid);
    g_signal_handler_disconnect (input->parsebin, input->drained_sigid);
    gst_element_set_state (input->parsebin, GST_STATE_NULL);
    gst_bin_remove (GST_BIN (dbin), input->parsebin);
    gst_clear_object (&input->parsebin);
    gst_clear_object (&input->parsebin_sink);
  }
  if (input->identity) {
    GstPad *idpad = gst_element_get_static_pad (input->identity, "src");
    DecodebinInputStream *stream;

    SELECTION_LOCK (dbin);
    stream = find_input_stream_for_pad (dbin, idpad);
    remove_input_stream (dbin, stream);
    SELECTION_UNLOCK (dbin);

    gst_object_unref (idpad);

    gst_element_set_state (input->identity, GST_STATE_NULL);
    gst_bin_remove (GST_BIN (dbin), input->identity);
    gst_clear_object (&input->identity);
  }
  if (input->collection)
    gst_clear_object (&input->collection);

  if (input->input_probe) {
    gst_pad_remove_probe (input->ghost_sink, input->input_probe);
    input->input_probe = 0;
  }

  g_list_free_full (input->events_waiting_for_collection,
      (GDestroyNotify) gst_event_unref);
  input->events_waiting_for_collection = NULL;

  input->group_id = GST_GROUP_ID_INVALID;
}

/** gst_decodebin_input_free:
 * @input: The #DecodebinInput to free
 *
 * Frees the @input and removes the ghost pad from decodebin
 *
 * Call with INPUT LOCK taken
 */
static void
gst_decodebin_input_free (DecodebinInput * input)
{
  GstDecodebin3 *dbin = input->dbin;

  g_return_if_fail (dbin);

  gst_decodebin_input_reset (input);

  GST_LOG_OBJECT (dbin, "Freeing input %p", input);

  INPUT_UNLOCK (dbin);
  gst_element_remove_pad (GST_ELEMENT (dbin), input->ghost_sink);
  INPUT_LOCK (dbin);

  g_free (input);
}

static gboolean
sink_query_function (GstPad * sinkpad, GstDecodebin3 * dbin, GstQuery * query)
{
  DecodebinInput *input =
      g_object_get_data (G_OBJECT (sinkpad), "decodebin.input");

  g_return_val_if_fail (input, FALSE);

  GST_DEBUG_OBJECT (sinkpad, "query %" GST_PTR_FORMAT, query);

  /* We accept any caps, since we will reconfigure ourself internally if the new
   * stream is incompatible */
  if (GST_QUERY_TYPE (query) == GST_QUERY_ACCEPT_CAPS) {
    GST_DEBUG_OBJECT (dbin, "Accepting ACCEPT_CAPS query");
    gst_query_set_accept_caps_result (query, TRUE);
    return TRUE;
  }
  return gst_pad_query_default (sinkpad, GST_OBJECT (dbin), query);
}

/** gst_decodebin_input_requires_parsebin:
 * @input: A #DecodebinInput
 * @newcaps: The new incoming #GstCaps
 *
 * Returns: #TRUE if @input requires setting up a `parsebin` element for the
 * incoming stream and @newcaps.
 */
static gboolean
gst_decodebin_input_requires_parsebin (DecodebinInput * input,
    GstCaps * newcaps)
{
  GstDecodebin3 *dbin = input->dbin;
  GstPad *sinkpad = input->ghost_sink;
  gboolean parsebin_needed = TRUE;
  GstStream *stream;

  stream = gst_pad_get_stream (sinkpad);

  if (stream == NULL) {
    /* If upstream didn't provide a `GstStream` we will need to create a
     * parsebin to handle that stream */
    GST_DEBUG_OBJECT (sinkpad,
        "Need to create parsebin since upstream doesn't provide GstStream");
  } else if (gst_caps_can_intersect (newcaps, dbin->caps)) {
    /* If the incoming caps match decodebin3 output, no processing is needed */
    GST_FIXME_OBJECT (sinkpad, "parsebin not needed (matches output caps) !");
    parsebin_needed = FALSE;
  } else if (input->input_is_parsed) {
    GST_DEBUG_OBJECT (sinkpad, "input is parsed, no parsebin needed");
    parsebin_needed = FALSE;
  } else {
    GList *decoder_list;
    /* If the incoming caps are compatible with a decoder, we don't need to
     * process it before */
    g_mutex_lock (&dbin->factories_lock);
    gst_decode_bin_update_factories_list (dbin);
    decoder_list =
        gst_element_factory_list_filter (dbin->decoder_factories, newcaps,
        GST_PAD_SINK, TRUE);
    g_mutex_unlock (&dbin->factories_lock);
    if (decoder_list) {
      GST_FIXME_OBJECT (sinkpad, "parsebin not needed (available decoders) !");
      gst_plugin_feature_list_free (decoder_list);
      parsebin_needed = FALSE;
    }
  }
  if (stream)
    gst_object_unref (stream);

  return parsebin_needed;
}

/** gst_decodebin_input_setup_identity:
 * @input: A #DecodebinInput
 *
 * Sets up @input to receive a single elementary stream with `identity`.
 **/
static void
gst_decodebin_input_setup_identity (DecodebinInput * input)
{
  GstDecodebin3 *dbin = input->dbin;
  GstPad *idsrc, *idsink;

  GST_DEBUG_OBJECT (input->ghost_sink, "Adding identity for new input stream");

  input->identity = gst_element_factory_make ("identity", NULL);
  /* We drop allocation queries due to our usage of multiqueue just
   * afterwards. It is just too dangerous.
   *
   * If application users want to have optimal raw source <=> sink allocations
   * they should not use decodebin3
   */
  g_object_set (input->identity, "drop-allocation", TRUE, NULL);
  input->identity = gst_object_ref (input->identity);
  idsink = gst_element_get_static_pad (input->identity, "sink");
  idsrc = gst_element_get_static_pad (input->identity, "src");
  gst_bin_add (GST_BIN (dbin), input->identity);

  /* Forward any existing GstStream directly on the input stream */
  gst_decodebin_input_add_stream (input, idsrc,
      gst_pad_get_stream (input->ghost_sink));

  gst_object_unref (idsrc);
  gst_object_unref (idsink);
  gst_ghost_pad_set_target (GST_GHOST_PAD (input->ghost_sink), idsink);
  gst_element_sync_state_with_parent (input->identity);
}

static gboolean
sink_event_function (GstPad * sinkpad, GstDecodebin3 * dbin, GstEvent * event)
{
  DecodebinInput *input =
      g_object_get_data (G_OBJECT (sinkpad), "decodebin.input");

  g_return_val_if_fail (input, FALSE);

  GST_DEBUG_OBJECT (sinkpad, "event %" GST_PTR_FORMAT, event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_STREAM_START:
    {
      GstQuery *q = gst_query_new_selectable ();
      const GstStructure *s = gst_event_get_structure (event);

      /* Query whether upstream can handle stream selection or not */
      if (gst_pad_peer_query (sinkpad, q)) {
        gst_query_parse_selectable (q, &input->upstream_selected);
        GST_DEBUG_OBJECT (sinkpad, "Upstream is selectable : %d",
            input->upstream_selected);
      } else {
        input->upstream_selected = FALSE;
        GST_DEBUG_OBJECT (sinkpad, "Upstream does not handle SELECTABLE query");
      }
      gst_query_unref (q);

      /* FIXME : We force `decodebin3` to upstream selection mode if *any* of the
         inputs is. This means things might break if there's a mix */
      if (input->upstream_selected)
        dbin->upstream_handles_selection = TRUE;

      input->input_is_parsed = s
          && gst_structure_has_field (s, "urisourcebin-parsed-data");
      if (input->input_is_parsed) {
        /* We remove the custom field from stream-start so as not to pollute
         * downstream */
        event = gst_event_make_writable (event);
        s = gst_event_get_structure (event);
        gst_structure_remove_field ((GstStructure *) s,
            "urisourcebin-parsed-data");
      }

      /* Make sure group ids will be recalculated */
      input->group_id = GST_GROUP_ID_INVALID;
      INPUT_LOCK (dbin);
      recalculate_group_id (dbin);
      INPUT_UNLOCK (dbin);
      break;
    }
    case GST_EVENT_STREAM_COLLECTION:
    {
      GstStreamCollection *collection = NULL;

      gst_event_parse_stream_collection (event, &collection);
      if (collection) {
        GstMessage *collection_msg;
        INPUT_LOCK (dbin);
        collection_msg =
            handle_stream_collection_locked (dbin, collection, input);
        gst_object_unref (collection);
        INPUT_UNLOCK (dbin);
        if (collection_msg)
          gst_element_post_message (GST_ELEMENT_CAST (dbin), collection_msg);
      }

      /* If we are waiting to create an identity passthrough, do it now */
      if (!input->parsebin && !input->identity)
        gst_decodebin_input_setup_identity (input);

      /* Remove buffer probe for caps/collection detection */
      if (input->input_probe) {
        gst_pad_remove_probe (sinkpad, input->input_probe);
        input->input_probe = 0;
      }

      /* Drain all pending events */
      if (input->events_waiting_for_collection) {
        GList *tmp;
        for (tmp = input->events_waiting_for_collection; tmp; tmp = tmp->next)
          gst_pad_event_default (sinkpad, GST_OBJECT (dbin), tmp->data);
        g_list_free (input->events_waiting_for_collection);
        input->events_waiting_for_collection = NULL;
      }
      break;
    }
    case GST_EVENT_CAPS:
    {
      GstCaps *newcaps = NULL;

      gst_event_parse_caps (event, &newcaps);
      if (!newcaps)
        break;
      GST_DEBUG_OBJECT (sinkpad, "new caps %" GST_PTR_FORMAT, newcaps);

      /* Remove buffer probe for caps/collection detection */
      if (input->input_probe) {
        gst_pad_remove_probe (sinkpad, input->input_probe);
        input->input_probe = 0;
      }

      /* No parsebin or identity present, check if we can avoid creating one */
      if (!input->parsebin && !input->identity) {
        if (gst_decodebin_input_requires_parsebin (input, newcaps)) {
          GST_DEBUG_OBJECT (sinkpad, "parsebin is required for input");
          gst_decodebin_input_ensure_parsebin (input);
          break;
        }
        GST_DEBUG_OBJECT (sinkpad,
            "parsebin not required. Will create identity passthrough element once we get the collection");
        break;
      }

      if (input->identity) {
        if (gst_decodebin_input_requires_parsebin (input, newcaps)) {
          GST_ERROR_OBJECT (sinkpad,
              "Switching from passthrough to parsebin on inputs is not supported !");
          gst_event_unref (event);
          return FALSE;
        }
        /* Nothing else to do here */
        break;
      }

      /* Check if the parsebin present can handle the new caps */
      g_assert (input->parsebin);
      GST_DEBUG_OBJECT (sinkpad,
          "New caps, checking if they are compatible with existing parsebin");
      if (!gst_pad_query_accept_caps (input->parsebin_sink, newcaps)) {
        GST_DEBUG_OBJECT (sinkpad,
            "Parsebin doesn't accept the new caps %" GST_PTR_FORMAT, newcaps);
        /* Reset parsebin so that it reconfigures itself for the new stream format */
        INPUT_LOCK (dbin);
        gst_decodebin_input_reset_parsebin (dbin, input);
        INPUT_UNLOCK (dbin);
      } else {
        GST_DEBUG_OBJECT (sinkpad, "Parsebin accepts new caps");
      }
      break;
    }
    case GST_EVENT_SEGMENT:
    {
      const GstSegment *segment = NULL;
      gst_event_parse_segment (event, &segment);

      /* All data reaching multiqueue must be in time format. If it's not, we
       * need to use a parsebin on the incoming stream.
       */
      if (segment && segment->format != GST_FORMAT_TIME && !input->parsebin) {
        GST_DEBUG_OBJECT (sinkpad,
            "Got a non-time segment, forcing parsebin handling");
        gst_decodebin_input_ensure_parsebin (input);
      }
      break;
    }
    default:
      break;
  }

  /* For parsed inputs, if we are waiting for a collection event, store them for
   * now */
  if (!input->collection && input->input_is_parsed) {
    GST_DEBUG_OBJECT (sinkpad,
        "Postponing event until we get a stream collection");
    input->events_waiting_for_collection =
        g_list_append (input->events_waiting_for_collection, event);
    return TRUE;
  }

  /* Chain to parent function */
  return gst_pad_event_default (sinkpad, GST_OBJECT (dbin), event);
}

/** gst_decodebin_input_new:
 * @dbin: The decodebin instance
 * @main: Whether this is the main input (for the static "sink" sinkpad) or not
 *
 * Returns: A new #DecodebinInput
 */
static DecodebinInput *
gst_decodebin_input_new (GstDecodebin3 * dbin, gboolean main)
{
  DecodebinInput *input;

  input = g_new0 (DecodebinInput, 1);
  input->dbin = dbin;
  input->is_main = main;
  input->group_id = GST_GROUP_ID_INVALID;
  if (main)
    input->ghost_sink = gst_ghost_pad_new_no_target ("sink", GST_PAD_SINK);
  else {
    gchar *pad_name = g_strdup_printf ("sink_%u", dbin->input_counter++);
    input->ghost_sink = gst_ghost_pad_new_no_target (pad_name, GST_PAD_SINK);
    g_free (pad_name);
  }
  input->upstream_selected = FALSE;
  g_object_set_data (G_OBJECT (input->ghost_sink), "decodebin.input", input);
  gst_pad_set_event_function (input->ghost_sink,
      (GstPadEventFunction) sink_event_function);
  gst_pad_set_query_function (input->ghost_sink,
      (GstPadQueryFunction) sink_query_function);
  gst_pad_set_link_function (input->ghost_sink, gst_decodebin3_input_pad_link);
  g_signal_connect (input->ghost_sink, "unlinked",
      (GCallback) gst_decodebin3_input_pad_unlink, input);

  gst_pad_set_active (input->ghost_sink, TRUE);
  gst_element_add_pad ((GstElement *) dbin, input->ghost_sink);

  return input;

}

static GstPad *
gst_decodebin3_request_new_pad (GstElement * element, GstPadTemplate * temp,
    const gchar * name, const GstCaps * caps)
{
  GstDecodebin3 *dbin = (GstDecodebin3 *) element;
  DecodebinInput *input;
  GstPad *res = NULL;

  /* We are ignoring names for the time being, not sure it makes any sense
   * within the context of decodebin3 ... */
  input = gst_decodebin_input_new (dbin, FALSE);
  if (input) {
    INPUT_LOCK (dbin);
    dbin->other_inputs = g_list_append (dbin->other_inputs, input);
    res = input->ghost_sink;
    INPUT_UNLOCK (dbin);
  }

  return res;
}

/* Must be called with factories lock! */
static void
gst_decode_bin_update_factories_list (GstDecodebin3 * dbin)
{
  guint cookie;

  cookie = gst_registry_get_feature_list_cookie (gst_registry_get ());
  if (!dbin->factories || dbin->factories_cookie != cookie) {
    GList *tmp;
    if (dbin->factories)
      gst_plugin_feature_list_free (dbin->factories);
    if (dbin->decoder_factories)
      g_list_free (dbin->decoder_factories);
    if (dbin->decodable_factories)
      g_list_free (dbin->decodable_factories);
    dbin->factories =
        gst_element_factory_list_get_elements
        (GST_ELEMENT_FACTORY_TYPE_DECODABLE, GST_RANK_MARGINAL);
    dbin->factories =
        g_list_sort (dbin->factories, gst_plugin_feature_rank_compare_func);
    dbin->factories_cookie = cookie;

    /* Filter decoder and other decodables */
    dbin->decoder_factories = NULL;
    dbin->decodable_factories = NULL;
    for (tmp = dbin->factories; tmp; tmp = tmp->next) {
      GstElementFactory *fact = (GstElementFactory *) tmp->data;
      if (gst_element_factory_list_is_type (fact,
              GST_ELEMENT_FACTORY_TYPE_DECODER))
        dbin->decoder_factories = g_list_append (dbin->decoder_factories, fact);
      else
        dbin->decodable_factories =
            g_list_append (dbin->decodable_factories, fact);
    }
  }
}

/* Must be called with appropriate lock if list is a protected variable */
static const gchar *
stream_in_list (GList * list, const gchar * sid)
{
  GList *tmp;

#if EXTRA_DEBUG
  for (tmp = list; tmp; tmp = tmp->next) {
    gchar *osid = (gchar *) tmp->data;
    GST_DEBUG ("Checking %s against %s", sid, osid);
  }
#endif

  for (tmp = list; tmp; tmp = tmp->next) {
    const gchar *osid = (gchar *) tmp->data;
    if (!g_strcmp0 (sid, osid))
      return osid;
  }

  return NULL;
}

static GList *
remove_from_list (GList * list, const gchar * sid)
{
  GList *tmp;

  for (tmp = list; tmp; tmp = tmp->next) {
    gchar *osid = tmp->data;
    if (!g_strcmp0 (sid, osid)) {
      g_free (osid);
      return g_list_delete_link (list, tmp);
    }
  }
  return list;
}

/* Called with SELECTION_LOCK */
static gboolean
stream_is_active (GstDecodebin3 * dbin, const gchar * stream_id)
{
  GList *tmp;

  for (tmp = dbin->slots; tmp; tmp = tmp->next) {
    MultiQueueSlot *slot = tmp->data;
    if (slot->output && !g_strcmp0 (stream_id, slot->active_stream_id))
      return TRUE;
  }

  return FALSE;
}

/* Called with SELECTION_LOCK */
static gboolean
stream_is_requested (GstDecodebin3 * dbin, const gchar * stream_id)
{
  if (dbin->output_collection == NULL)
    return FALSE;
  return stream_in_list (dbin->output_collection->requested_selection,
      stream_id) != NULL;
}

/** update_requested_selection:
 * @dbin: A #GstDecodebin3
 * @new_collection: The #DecodebinCollection to update
 *
 * Figures out the selection to use for @new_collection. Will figure this out
 * based on signals and current output collection.
 *
 * This function should be called once we start seeing a #DecodebinCollection on
 * the output of multiqueue.
 *
 * Must be called with the SELECTION_LOCK taken
 */
static void
update_requested_selection (GstDecodebin3 * dbin,
    DecodebinCollection * new_collection)
{
  guint i, nb;
  GList *tmp = NULL;
  gboolean all_user_selected = TRUE;
  GstStreamType used_types = 0;

  if (new_collection->requested_selection) {
    GST_DEBUG_OBJECT (dbin, "Collection already has a selection");
    return;
  }

  nb = gst_stream_collection_get_size (new_collection->collection);

  /* 3. If not, check if we already have some of the streams in the
   * existing active/requested selection */
  for (i = 0; i < nb; i++) {
    GstStream *stream =
        gst_stream_collection_get_stream (new_collection->collection, i);
    const gchar *sid = gst_stream_get_stream_id (stream);
    gint request = -1;
    /* Fire select-stream signal to see if outside components want to
     * hint at which streams should be selected */
    g_signal_emit (G_OBJECT (dbin),
        gst_decodebin3_signals[SIGNAL_SELECT_STREAM], 0,
        new_collection->collection, stream, &request);
    GST_DEBUG_OBJECT (dbin, "stream %s , request:%d", sid, request);

    if (request == -1)
      all_user_selected = FALSE;
    if (request == 1 || (request == -1 && (stream_is_requested (dbin, sid)
                || stream_is_active (dbin, sid)))) {
      GstStreamType curtype = gst_stream_get_stream_type (stream);
      if (request == 1)
        GST_DEBUG_OBJECT (dbin,
            "Using stream requested by 'select-stream' signal : %s", sid);
      else
        GST_DEBUG_OBJECT (dbin,
            "Re-using stream already present in requested or active selection : %s",
            sid);
      tmp = g_list_append (tmp, (gchar *) sid);
      used_types |= curtype;
    }
  }

  /* 4. If the user didn't explicitly selected all streams, match one stream of each type */
  if (!all_user_selected && new_collection->seqnum == GST_SEQNUM_INVALID) {
    for (i = 0; i < nb; i++) {
      GstStream *stream =
          gst_stream_collection_get_stream (new_collection->collection, i);
      GstStreamType curtype = gst_stream_get_stream_type (stream);
      if (curtype != GST_STREAM_TYPE_UNKNOWN && !(used_types & curtype)) {
        const gchar *sid = gst_stream_get_stream_id (stream);
        GST_DEBUG_OBJECT (dbin,
            "Automatically selecting stream '%s' of type %s", sid,
            gst_stream_type_get_name (curtype));
        tmp = g_list_append (tmp, (gchar *) sid);
        used_types |= curtype;
      }
    }
  }

  if (tmp) {
    /* Finally set the requested selection */
    new_collection->requested_selection =
        g_list_copy_deep (tmp, (GCopyFunc) g_strdup, NULL);
    new_collection->posted_streams_selected_msg = FALSE;
    g_list_free (tmp);
  }
}

/* sort_streams:
 * GCompareFunc to use with lists of GstStream.
 * Sorts GstStreams by stream type and SELECT flag and stream-id
 * First video, then audio, then others.
 *
 * Return: negative if a<b, 0 if a==b, positive if a>b
 */
static gint
sort_streams (GstStream * sa, GstStream * sb)
{
  GstStreamType typea, typeb;
  GstStreamFlags flaga, flagb;
  const gchar *ida, *idb;
  gint ret = 0;

  typea = gst_stream_get_stream_type (sa);
  typeb = gst_stream_get_stream_type (sb);

  GST_LOG ("sa(%s), sb(%s)", gst_stream_get_stream_id (sa),
      gst_stream_get_stream_id (sb));

  /* Sort by stream type. First video, then audio, then others(text, container, unknown) */
  if (typea != typeb) {
    if (typea & GST_STREAM_TYPE_VIDEO)
      ret = -1;
    else if (typea & GST_STREAM_TYPE_AUDIO)
      ret = (!(typeb & GST_STREAM_TYPE_VIDEO)) ? -1 : 1;
    else if (typea & GST_STREAM_TYPE_TEXT)
      ret = (!(typeb & GST_STREAM_TYPE_VIDEO)
          && !(typeb & GST_STREAM_TYPE_AUDIO)) ? -1 : 1;
    else if (typea & GST_STREAM_TYPE_CONTAINER)
      ret = (typeb & GST_STREAM_TYPE_UNKNOWN) ? -1 : 1;
    else
      ret = 1;

    if (ret != 0) {
      GST_LOG ("Sort by stream-type: %d", ret);
      return ret;
    }
  }

  /* Sort by SELECT flag, if stream type is same. */
  flaga = gst_stream_get_stream_flags (sa);
  flagb = gst_stream_get_stream_flags (sb);

  ret =
      (flaga & GST_STREAM_FLAG_SELECT) ? ((flagb & GST_STREAM_FLAG_SELECT) ? 0 :
      -1) : ((flagb & GST_STREAM_FLAG_SELECT) ? 1 : 0);

  if (ret != 0) {
    GST_LOG ("Sort by SELECT flag: %d", ret);
    return ret;
  }

  /* Sort by stream-id, if otherwise the same. */
  ida = gst_stream_get_stream_id (sa);
  idb = gst_stream_get_stream_id (sb);
  ret = g_strcmp0 (ida, idb);

  GST_LOG ("Sort by stream-id: %d", ret);

  return ret;
}

/* Call with INPUT_LOCK taken */
static GstStreamCollection *
get_merged_collection (GstDecodebin3 * dbin)
{
  gboolean needs_merge = FALSE;
  GstStreamCollection *res = NULL;
  GList *tmp;
  GList *unsorted_streams = NULL;
  guint i, nb_stream;

  /* First check if we need to do a merge or just return the only collection */
  res = dbin->main_input->collection;

  for (tmp = dbin->other_inputs; tmp; tmp = tmp->next) {
    DecodebinInput *input = (DecodebinInput *) tmp->data;
    GST_LOG_OBJECT (dbin, "Comparing res %p input->collection %p", res,
        input->collection);
    if (input->collection && input->collection != res) {
      if (res) {
        needs_merge = TRUE;
        break;
      }
      res = input->collection;
    }
  }

  if (!needs_merge) {
    GST_DEBUG_OBJECT (dbin, "No need to merge, returning %p", res);
    return res ? gst_object_ref (res) : NULL;
  }

  /* We really need to create a new collection */
  /* FIXME : Some numbering scheme maybe ?? */
  res = gst_stream_collection_new ("decodebin3");
  if (dbin->main_input->collection) {
    nb_stream = gst_stream_collection_get_size (dbin->main_input->collection);
    GST_DEBUG_OBJECT (dbin, "main input %p %d", dbin->main_input, nb_stream);
    for (i = 0; i < nb_stream; i++) {
      GstStream *stream =
          gst_stream_collection_get_stream (dbin->main_input->collection, i);
      unsorted_streams = g_list_append (unsorted_streams, stream);
    }
  }

  for (tmp = dbin->other_inputs; tmp; tmp = tmp->next) {
    DecodebinInput *input = (DecodebinInput *) tmp->data;
    GST_DEBUG_OBJECT (dbin, "input %p , collection %p", input,
        input->collection);
    if (input->collection) {
      nb_stream = gst_stream_collection_get_size (input->collection);
      GST_DEBUG_OBJECT (dbin, "nb_stream : %d", nb_stream);
      for (i = 0; i < nb_stream; i++) {
        GstStream *stream =
            gst_stream_collection_get_stream (input->collection, i);
        /* Only add if not already present in the list */
        if (!g_list_find (unsorted_streams, stream))
          unsorted_streams = g_list_append (unsorted_streams, stream);
      }
    }
  }

  /* re-order streams : video, then audio, then others */
  unsorted_streams =
      g_list_sort (unsorted_streams, (GCompareFunc) sort_streams);
  for (tmp = unsorted_streams; tmp; tmp = tmp->next) {
    GstStream *stream = (GstStream *) tmp->data;
    GST_DEBUG_OBJECT (dbin, "Adding #stream(%s) to collection",
        gst_stream_get_stream_id (stream));
    gst_stream_collection_add_stream (res, gst_object_ref (stream));
  }

  if (unsorted_streams)
    g_list_free (unsorted_streams);

  return res;
}

/* Call with INPUT_LOCK taken */
static DecodebinInput *
find_message_parsebin (GstDecodebin3 * dbin, GstElement * child)
{
  DecodebinInput *input = NULL;
  GstElement *parent = gst_object_ref (child);
  GList *tmp;

  do {
    GstElement *next_parent;

    GST_DEBUG_OBJECT (dbin, "parent %s",
        parent ? GST_ELEMENT_NAME (parent) : "<NONE>");

    if (parent == dbin->main_input->parsebin) {
      input = dbin->main_input;
      break;
    }
    for (tmp = dbin->other_inputs; tmp; tmp = tmp->next) {
      DecodebinInput *cur = (DecodebinInput *) tmp->data;
      if (parent == cur->parsebin) {
        input = cur;
        break;
      }
    }
    next_parent = (GstElement *) gst_element_get_parent (parent);
    gst_object_unref (parent);
    parent = next_parent;

  } while (parent && parent != (GstElement *) dbin);

  if (parent)
    gst_object_unref (parent);

  return input;
}

static const gchar *
stream_in_collection (GstStreamCollection * collection, gchar * sid)
{
  guint i, len;

  if (collection == NULL)
    return NULL;
  len = gst_stream_collection_get_size (collection);
  for (i = 0; i < len; i++) {
    GstStream *stream = gst_stream_collection_get_stream (collection, i);
    const gchar *osid = gst_stream_get_stream_id (stream);
    if (!g_strcmp0 (sid, osid))
      return osid;
  }

  return NULL;
}

static DecodebinCollection *
find_collection_for_stream (GstDecodebin3 * dbin, gchar * stream_id)
{
  GList *tmp;

  GST_DEBUG_OBJECT (dbin, "stream_id `%s`", stream_id);

  /* Recursively find the collection to which this stream belongs */
  for (tmp = dbin->collections; tmp; tmp = tmp->next) {
    DecodebinCollection *collection = tmp->data;
    GST_DEBUG_OBJECT (dbin, "Trying on DBCollection %p", collection);
    if (stream_in_collection (collection->collection, stream_id))
      return collection;
  }

  return NULL;
}

static gboolean
are_all_streams_in_collection (GstStreamCollection * collection,
    GList * streams)
{
  GList *tmp;

  for (tmp = streams; tmp; tmp = tmp->next) {
    if (!stream_in_collection (collection, tmp->data))
      return FALSE;
  }
  return TRUE;
}

static void
db_collection_free (DecodebinCollection * collection)
{
  GST_DEBUG ("Freeing collection %p for %" GST_PTR_FORMAT, collection,
      collection->collection);
  gst_object_unref (collection->collection);
  g_list_free_full (collection->requested_selection, g_free);
  g_list_free (collection->to_activate);

  g_free (collection);
}

static DecodebinCollection *
db_collection_new (GstStreamCollection * collection)
{
  DecodebinCollection *db_collection = g_new0 (DecodebinCollection, 1);

  db_collection->collection = collection;
  db_collection->seqnum = GST_SEQNUM_INVALID;

  GST_DEBUG ("Created new collection %p for %" GST_PTR_FORMAT, db_collection,
      collection);

  return db_collection;
}

/** handle_stream_collection_locked:
 * @dbin:
 * @collection: (transfer none): The new collection for @input. Can be %NULL.
 * @input: The #DecodebinInput
 *
 * Called with INPUT_LOCK taken.
 *
 * Handle a new (or updated) @collection for the given @input. If this results
 * in a different collection, the appropriate GST_MESSAGE_STREAM_COLLECTION to
 * be posted will be returned.
 *
 * Returns: A #GstMessage to be posted on the bus if a new collection was
 * generated, else %NULL.
 */
static GstMessage *
handle_stream_collection_locked (GstDecodebin3 * dbin,
    GstStreamCollection * collection, DecodebinInput * input)
{
  GstMessage *message = NULL;
  gboolean is_update = FALSE;
#ifndef GST_DISABLE_GST_DEBUG
  const gchar *upstream_id;
  guint i;
#endif
  if (!input) {
    GST_DEBUG_OBJECT (dbin,
        "Couldn't find corresponding input, most likely shutting down");
    return NULL;
  }

  /* Replace collection in input */
  if (input->collection)
    gst_object_unref (input->collection);
  if (collection)
    input->collection = gst_object_ref (collection);
  GST_DEBUG_OBJECT (dbin, "Setting collection %p on input %p", collection,
      input);

  /* Merge collection if needed */
  collection = get_merged_collection (dbin);
  if (!collection)
    return NULL;

#ifndef GST_DISABLE_GST_DEBUG
  /* Just some debugging */
  upstream_id = gst_stream_collection_get_upstream_id (collection);
  GST_DEBUG ("Received Stream Collection. Upstream_id : %s", upstream_id);
  GST_DEBUG ("From input %p", input);
  GST_DEBUG ("  %d streams", gst_stream_collection_get_size (collection));
  for (i = 0; i < gst_stream_collection_get_size (collection); i++) {
    GstStream *stream = gst_stream_collection_get_stream (collection, i);
    GstTagList *taglist;
    GstCaps *caps;

    GST_DEBUG ("   Stream '%s'", gst_stream_get_stream_id (stream));
    GST_DEBUG ("     type  : %s",
        gst_stream_type_get_name (gst_stream_get_stream_type (stream)));
    GST_DEBUG ("     flags : 0x%x", gst_stream_get_stream_flags (stream));
    taglist = gst_stream_get_tags (stream);
    GST_DEBUG ("     tags  : %" GST_PTR_FORMAT, taglist);
    caps = gst_stream_get_caps (stream);
    GST_DEBUG ("     caps  : %" GST_PTR_FORMAT, caps);
    if (taglist)
      gst_tag_list_unref (taglist);
    if (caps)
      gst_caps_unref (caps);
  }
#endif

  SELECTION_LOCK (dbin);
  /* If collection is same as current input collection, leave */
  if (dbin->input_collection) {
    GstStreamCollection *previous = dbin->input_collection->collection;

    if (collection == previous) {
      GST_DEBUG_OBJECT (dbin, "Collection didn't change");
      gst_object_unref (collection);
      SELECTION_UNLOCK (dbin);
      return NULL;
    }
    /* Check if this collection is an update of the previous one */
    if (gst_stream_collection_get_size (collection) >
        gst_stream_collection_get_size (previous)) {
      guint i;
      is_update = TRUE;
      for (i = 0; i < gst_stream_collection_get_size (previous); i++) {
        GstStream *stream = gst_stream_collection_get_stream (previous, i);
        const gchar *sid = gst_stream_get_stream_id (stream);
        if (!stream_in_collection (collection, (gchar *) sid)) {
          is_update = FALSE;
          break;
        }
      }
    }
  }

  /* We have a new collection, store it */
  GST_DEBUG_OBJECT (dbin, "Switching to new input collection (is_update:%d)",
      is_update);
  dbin->input_collection = db_collection_new (collection);
  dbin->input_collection->is_update = is_update;
  dbin->collections = g_list_append (dbin->collections, dbin->input_collection);
  message = gst_message_new_stream_collection ((GstObject *) dbin, collection);

  SELECTION_UNLOCK (dbin);

  return message;
}

static void
gst_decodebin3_handle_message (GstBin * bin, GstMessage * message)
{
  GstDecodebin3 *dbin = (GstDecodebin3 *) bin;
  GList *l;

  GST_DEBUG_OBJECT (bin, "Got Message %s", GST_MESSAGE_TYPE_NAME (message));

  GST_OBJECT_LOCK (dbin);
  for (l = dbin->candidate_decoders; l; l = l->next) {
    CandidateDecoder *candidate = l->data;
    if (GST_OBJECT_CAST (candidate->element) == GST_MESSAGE_SRC (message)) {
      if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ERROR) {
        if (candidate->error)
          gst_message_unref (candidate->error);
        candidate->error = message;
        GST_OBJECT_UNLOCK (dbin);
        return;
      } else if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_LATENCY) {
        if (candidate->latency)
          gst_message_unref (candidate->latency);
        GST_DEBUG_OBJECT (bin, "store latency message for %" GST_PTR_FORMAT,
            candidate->element);
        candidate->latency = message;
        GST_OBJECT_UNLOCK (dbin);
        return;
      }
      break;
    }
  }
  GST_OBJECT_UNLOCK (dbin);

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_STREAM_COLLECTION:
    {
      GstStreamCollection *collection = NULL;
      DecodebinInput *input;
      GstMessage *collection_message;

      INPUT_LOCK (dbin);
      input =
          find_message_parsebin (dbin,
          (GstElement *) GST_MESSAGE_SRC (message));
      if (input == NULL) {
        GST_DEBUG_OBJECT (dbin,
            "Couldn't find corresponding input, most likely shutting down");
        INPUT_UNLOCK (dbin);
        break;
      }
      if (input->upstream_selected) {
        GST_DEBUG_OBJECT (dbin,
            "Upstream handles selection, not using/forwarding collection");
        INPUT_UNLOCK (dbin);
        goto drop_message;
      }
      gst_message_parse_stream_collection (message, &collection);
      if (!collection) {
        INPUT_UNLOCK (dbin);
        break;
      }

      collection_message =
          handle_stream_collection_locked (dbin, collection, input);
      INPUT_UNLOCK (dbin);

      if (collection_message) {
        gst_message_unref (message);
        message = collection_message;
      }
      gst_object_unref (collection);
      break;
    }
    case GST_MESSAGE_LATENCY:
    {
      GList *tmp;
      /* Check if this is from one of our decoders */
      SELECTION_LOCK (dbin);
      for (tmp = dbin->output_streams; tmp; tmp = tmp->next) {
        DecodebinOutputStream *out = (DecodebinOutputStream *) tmp->data;
        if (out->decoder == (GstElement *) GST_MESSAGE_SRC (message)) {
          GstClockTime min, max;
          if (GST_IS_VIDEO_DECODER (out->decoder)) {
            gst_video_decoder_get_latency (GST_VIDEO_DECODER (out->decoder),
                &min, &max);
            GST_DEBUG_OBJECT (dbin,
                "Got latency update from one of our decoders. min: %"
                GST_TIME_FORMAT " max: %" GST_TIME_FORMAT, GST_TIME_ARGS (min),
                GST_TIME_ARGS (max));
            out->decoder_latency = min;
            /* Trigger recalculation */
            gst_decodebin3_update_min_interleave (dbin);
          }
          break;
        }
      }
      SELECTION_UNLOCK (dbin);
    }
    default:
      break;
  }

  GST_BIN_CLASS (parent_class)->handle_message (bin, message);

  return;

drop_message:
  {
    GST_DEBUG_OBJECT (bin, "dropping message");
    gst_message_unref (message);
  }
}

/* Called with SELECTION_LOCK taken */
static void
handle_stored_latency_message (GstDecodebin3 * dbin,
    DecodebinOutputStream * output, CandidateDecoder * candidate)
{
  GstClockTime min, max;
  if (candidate->latency && GST_IS_VIDEO_DECODER (candidate->element)) {
    gst_video_decoder_get_latency (GST_VIDEO_DECODER (candidate->element),
        &min, &max);
    GST_DEBUG_OBJECT (dbin,
        "Got latency update from %" GST_PTR_FORMAT ". min: %"
        GST_TIME_FORMAT " max: %" GST_TIME_FORMAT, candidate->element,
        GST_TIME_ARGS (min), GST_TIME_ARGS (max));
    output->decoder_latency = min;
    /* Trigger recalculation */
    gst_decodebin3_update_min_interleave (dbin);

    GST_BIN_CLASS (parent_class)->handle_message (GST_BIN_CAST (dbin),
        candidate->latency);
  }
}

static DecodebinOutputStream *
find_free_compatible_output (GstDecodebin3 * dbin, GstStream * stream)
{
  GList *tmp;
  GstStreamType stype = gst_stream_get_stream_type (stream);

  for (tmp = dbin->output_streams; tmp; tmp = tmp->next) {
    DecodebinOutputStream *output = (DecodebinOutputStream *) tmp->data;
    if (output->type == stype && output->slot && output->slot->active_stream) {
      if (!stream_is_requested (dbin, output->slot->active_stream_id)) {
        return output;
      }
    }
  }

  return NULL;
}

/** mq_slot_set_output:
 * @slot: A #MultiQueueSlot
 * @output: (allow none): A #DecodebinOutputStream
 *
 * Sets @output as the @slot output. The slot present previously will be
 * returned.
 *
 * If the output previously associated was linked (via a decoder) to the slot,
 * they will be unlinked.
 *
 * Returns: The output previously used on @slot.
 */
static DecodebinOutputStream *
mq_slot_set_output (MultiQueueSlot * slot, DecodebinOutputStream * output)
{
  DecodebinOutputStream *old_output = slot->output;

  GST_DEBUG_OBJECT (slot->src_pad, "output: %p", output);

  if (old_output == output) {
    GST_LOG_OBJECT (slot->src_pad, "Already targetting that output");
    return output;
  }

  if (old_output) {
    if (!old_output->slot)
      GST_DEBUG_OBJECT (slot->src_pad,
          "Old output %p was not associated to any slot", old_output);
    else
      GST_DEBUG_OBJECT (slot->src_pad,
          "Old output %p was associated to %" GST_PTR_FORMAT, old_output,
          old_output->slot->src_pad);
    /* Check for inconsistencies in assigning */
    g_assert (old_output->slot == slot);
    GST_DEBUG_OBJECT (slot->src_pad, "Unassigning");
    if (old_output->decoder_sink && old_output->decoder)
      gst_pad_unlink (slot->src_pad, old_output->decoder_sink);
    old_output->linked = FALSE;
    old_output->slot = NULL;
  }

  if (output) {
    if (output->slot)
      GST_DEBUG_OBJECT (slot->src_pad,
          "New output was previously associated to slot %s:%s",
          GST_DEBUG_PAD_NAME (output->slot->src_pad));
    output->slot = slot;
  }
  slot->output = output;

  return old_output;
}

/** mq_slot_get_or_create_output:
 * @slot: A #MultiQueueSlot
 *
 * Provides the #DecodebinOutputStream the @slot should use. This function will
 * figure that out based on the current selection. The slot output will be
 * updated accordingly.
 *
 * Call with SELECTION_LOCK taken
 *
 * Returns: The #DecodebinOutputStream to use, or #NULL if none can/should be
 * used.
 */
static DecodebinOutputStream *
mq_slot_get_or_create_output (MultiQueueSlot * slot)
{
  GstDecodebin3 *dbin = slot->dbin;
  DecodebinOutputStream *output = NULL;
  const gchar *stream_id;

  /* If we already have a configured output, just use it */
  if (slot->output != NULL) {
    GST_LOG_OBJECT (slot->src_pad, "Returning current output %s:%s",
        GST_DEBUG_PAD_NAME (slot->output->src_pad));
    return slot->output;
  }

  stream_id = slot->active_stream_id;
  GST_DEBUG_OBJECT (slot->src_pad, "active stream %" GST_PTR_FORMAT,
      slot->active_stream);

  /* If the stream is not requested, bail out */
  if (!stream_is_requested (dbin, stream_id)
      && !dbin->upstream_handles_selection) {
    GST_DEBUG_OBJECT (slot->src_pad, "Not selected, not creating any output");
    return NULL;
  }

  /* Check if we can steal an existing output stream we could re-use.
   * that is:
   * * an output stream whose slot->stream is not in requested
   * * and is of the same type as this stream
   */
  output = find_free_compatible_output (dbin, slot->active_stream);
  if (output) {
    GST_DEBUG_OBJECT (slot->src_pad, "Reassigning to output %s:%s",
        GST_DEBUG_PAD_NAME (output->src_pad));
    /* Move this output from its current slot to this slot */
    SELECTION_UNLOCK (dbin);
    gst_pad_add_probe (output->slot->src_pad, GST_PAD_PROBE_TYPE_IDLE,
        (GstPadProbeCallback) mq_slot_unassign_probe, output->slot, NULL);
    SELECTION_LOCK (dbin);
    return NULL;
  }

  output = db_output_stream_new (dbin, slot->type);
  mq_slot_set_output (slot, output);

  GST_DEBUG_OBJECT (dbin, "Now active : %s", stream_id);

  return output;
}

/* Returns SELECTED_STREAMS message if the active slots are equal to
 * requested_selection, else NULL.
 *
 * Must be called with SELECTION_LOCK taken */
static GstMessage *
is_selection_done (GstDecodebin3 * dbin)
{
  GList *tmp;
  GstMessage *msg;
  DecodebinCollection *collection = dbin->output_collection;

  GST_LOG_OBJECT (dbin, "Checking");

  if (dbin->upstream_handles_selection) {
    GST_DEBUG ("Upstream handles stream selection, returning");
    return NULL;
  }

  if (!collection) {
    GST_DEBUG ("No collection");
    return NULL;
  }

  if (collection->posted_streams_selected_msg) {
    GST_DEBUG ("Already posted message for this selection");
    return NULL;
  }

  if (collection->to_activate != NULL) {
    GST_DEBUG ("Still have streams to activate");
    return NULL;
  }
  for (tmp = collection->requested_selection; tmp; tmp = tmp->next) {
    GST_DEBUG ("Checking requested stream %s", (gchar *) tmp->data);
    if (!stream_is_active (dbin, (gchar *) tmp->data)) {
      GST_DEBUG ("Not in active selection, returning");
      return NULL;
    }
  }

  GST_DEBUG_OBJECT (dbin, "Selection active, creating message");

  /* All requested streams are present */
  msg =
      gst_message_new_streams_selected ((GstObject *) dbin,
      collection->collection);
  if (collection->seqnum != GST_SEQNUM_INVALID) {
    gst_message_set_seqnum (msg, collection->seqnum);
  }
  for (tmp = dbin->slots; tmp; tmp = tmp->next) {
    MultiQueueSlot *slot = tmp->data;
    if (slot->output) {
      GST_DEBUG_OBJECT (dbin, "Adding stream %s", slot->active_stream_id);
      if (!stream_is_requested (dbin, slot->active_stream_id)) {
        /* We *could* still have an old output which isn't fully deactivated
         * yet. Not 100% ready yet */
        GST_DEBUG_OBJECT (dbin,
            "Stream from previous selection still active, bailing out");
        gst_message_unref (msg);
        return NULL;
      }
      gst_message_streams_selected_add (msg, slot->active_stream);
    }
  }
  collection->posted_streams_selected_msg = TRUE;

  return msg;
}

/** check_and_drain_multiqueue_locked:
 * @dbin: A #GstDecodebin3
 * @eos_event: The GST_EVENT_EOS that triggered this check.
 *
 * Check if all #DecodebinInputstream and #MultiqueueSlot are
 * emptied/drained. If that is the case, send the final sequence of final EOS
 * events based on the provided @eos_event.
 */
static void
check_and_drain_multiqueue_locked (GstDecodebin3 * dbin, GstEvent * eos_event)
{
  GList *iter;

  GST_DEBUG_OBJECT (dbin, "checking slots for eos");

  for (iter = dbin->slots; iter; iter = iter->next) {
    MultiQueueSlot *slot = iter->data;

    if (slot->output && !slot->is_drained) {
      GST_LOG_OBJECT (slot->sink_pad, "Not drained, not all slots are done");
      return;
    }
  }

  /* Also check with the inputs, data might be pending */
  if (!all_input_streams_are_eos (dbin))
    return;

  GST_DEBUG_OBJECT (dbin,
      "All active slots are drained, and no pending input, push EOS");

  for (iter = dbin->input_streams; iter; iter = iter->next) {
    GstEvent *stream_start, *eos;
    DecodebinInputStream *input = (DecodebinInputStream *) iter->data;
    GstPad *peer = gst_pad_get_peer (input->srcpad);

    if (!peer) {
      GST_DEBUG_OBJECT (input->srcpad, "Not linked to multiqueue");
      continue;
    }

    /* First forward a custom STREAM_START event to reset the EOS status (if
     * any) */
    stream_start =
        gst_pad_get_sticky_event (input->srcpad, GST_EVENT_STREAM_START, 0);
    if (stream_start) {
      GstStructure *s;
      GstEvent *custom_stream_start = gst_event_copy (stream_start);
      gst_event_unref (stream_start);
      s = (GstStructure *) gst_event_get_structure (custom_stream_start);
      gst_structure_set (s, "decodebin3-flushing-stream-start",
          G_TYPE_BOOLEAN, TRUE, NULL);
      gst_pad_send_event (peer, custom_stream_start);
    }
    /* Send EOS to all slots */
    eos = gst_event_new_eos ();
    gst_event_set_seqnum (eos, gst_event_get_seqnum (eos_event));
    gst_mini_object_set_qdata (GST_MINI_OBJECT_CAST (eos),
        CUSTOM_FINAL_EOS_QUARK, (gchar *) CUSTOM_FINAL_EOS_QUARK_DATA, NULL);
    gst_pad_send_event (peer, eos);
    gst_object_unref (peer);
  }
}

/*
 * Returns TRUE if there are no more streams to output and an ERROR message
 * should be posted
 */
static inline gboolean
no_more_streams_locked (GstDecodebin3 * dbin)
{
  GList *tmp;

  if (!dbin->output_collection)
    return FALSE;

  if (dbin->output_collection->requested_selection)
    return FALSE;

  for (tmp = dbin->slots; tmp; tmp = tmp->next) {
    MultiQueueSlot *slot = tmp->data;
    if (slot->output)
      return FALSE;
  }

  return TRUE;
}

/** mq_slot_check_reconfiguration:
 * @slot: A #MultiQueueSlot
 *
 * Check if the @slot output needs to be (re)configured:
 * * Should an output be created/setup ?
 * * Should the associated output be reconfigured ?
 *
 * Will also handle missing streams message emission
 */
static void
mq_slot_check_reconfiguration (MultiQueueSlot * slot)
{
  GstDecodebin3 *dbin = slot->dbin;
  DecodebinOutputStream *output;
  GstMessage *msg = NULL;
  gboolean no_more_streams;
  DecodebinCollection *collection = dbin->output_collection;

  SELECTION_LOCK (dbin);
  output = mq_slot_get_or_create_output (slot);
  if (!output) {
    /* Slot is not used. */
    no_more_streams = no_more_streams_locked (dbin);
    SELECTION_UNLOCK (dbin);
    if (no_more_streams)
      GST_ELEMENT_ERROR (slot->dbin, STREAM, FAILED, (NULL),
          ("No streams to output"));
    return;
  }

  if (!db_output_stream_reconfigure (output, &msg)) {
    GST_DEBUG_OBJECT (dbin,
        "Removing failing stream from selection: %" GST_PTR_FORMAT,
        slot->active_stream);
    collection->requested_selection =
        remove_from_list (collection->requested_selection,
        slot->active_stream_id);
    collection->posted_streams_selected_msg = FALSE;

    /* Remove output */
    mq_slot_set_output (slot, NULL);
    dbin->output_streams = g_list_remove (dbin->output_streams, output);
    db_output_stream_free (output);

    no_more_streams = no_more_streams_locked (dbin);
    SELECTION_UNLOCK (dbin);
    if (msg)
      gst_element_post_message ((GstElement *) slot->dbin, msg);
    if (no_more_streams)
      GST_ELEMENT_ERROR (slot->dbin, CORE, MISSING_PLUGIN, (NULL),
          ("No suitable plugins found"));
    else
      GST_ELEMENT_WARNING (slot->dbin, CORE, MISSING_PLUGIN, (NULL),
          ("Some plugins were missing"));
  } else {
    GstMessage *selection_msg = is_selection_done (dbin);
    /* All good, we reconfigured the associated output. Check if we're done with
     * the current selection */
    SELECTION_UNLOCK (dbin);
    if (selection_msg)
      gst_element_post_message ((GstElement *) slot->dbin, selection_msg);
  }
}

static void
update_stream_presence (GstDecodebin3 * dbin, DecodebinCollection * collection)
{
  GList *tmp;

  if (dbin->upstream_handles_selection) {
    collection->all_streams_present = TRUE;
    return;
  }

  if (g_list_length (dbin->slots) !=
      gst_stream_collection_get_size (collection->collection)) {
    collection->all_streams_present = FALSE;
    return;
  }

  for (tmp = dbin->slots; tmp; tmp = tmp->next) {
    MultiQueueSlot *slot = tmp->data;
    if (!stream_in_collection (collection->collection,
            (gchar *) slot->active_stream_id)) {
      collection->all_streams_present = FALSE;
      return;
    }
  }

  collection->all_streams_present = TRUE;
}

/** mq_slot_handle_stream_start:
 * @slot: A #MultiQueueSlot
 * @stream_event: A #GST_EVENT_STREAM_START
 *
 * Returns: The #GstPadProbeReturn. If #GST_PAD_PROBE_HANDLED the ownership of
 * @stream_event was taken.
 */
static GstPadProbeReturn
mq_slot_handle_stream_start (MultiQueueSlot * slot, GstEvent * stream_event)
{
  GstDecodebin3 *dbin = slot->dbin;
  GstStream *stream = NULL;
  const GstStructure *s = gst_event_get_structure (stream_event);
  DecodebinCollection *collection;
  GList *tmp, *next;

  /* Drop STREAM_START events used to cleanup multiqueue */
  if (s && gst_structure_has_field (s, "decodebin3-flushing-stream-start")) {
    gst_event_unref (stream_event);
    return GST_PAD_PROBE_HANDLED;
  }

  gst_event_parse_stream (stream_event, &stream);
  if (stream == NULL) {
    GST_ERROR_OBJECT (slot->src_pad,
        "Got a STREAM_START event without a GstStream");
    return GST_PAD_PROBE_OK;
  }

  SELECTION_LOCK (dbin);

  slot->is_drained = FALSE;
  GST_DEBUG_OBJECT (slot->src_pad, "%" GST_PTR_FORMAT, stream);

  /* 1. Store new stream/stream_id */
  if (slot->active_stream == stream) {
    GST_DEBUG_OBJECT (slot->src_pad, "No stream change");
    goto beach;
  }

  gst_object_replace ((GstObject **) & slot->active_stream,
      (GstObject *) stream);
  slot->active_stream_id = gst_stream_get_stream_id (stream);

  /* If the slot is active and the stream type is different, remove it.
   *
   * This will only happen in case no slots of the same type was available for
   * that input (ex: switching from audio-only to video-only upstream of
   * decodebin3).
   */
  if (slot->output && slot->output->type != gst_stream_get_stream_type (stream)) {
    DecodebinOutputStream *previous_output = slot->output;
    GST_DEBUG_OBJECT (slot->src_pad,
        "Slot is changing stream type, removing output");
    mq_slot_set_output (slot, NULL);
    dbin->output_streams =
        g_list_remove (dbin->output_streams, previous_output);
    db_output_stream_free (previous_output);
  }

  collection =
      find_collection_for_stream (dbin, (gchar *) slot->active_stream_id);
  g_assert (collection);

  /* check if all streams are present for that collection. We do it now since we
   * might just have a single stream in the collection */
  update_stream_presence (dbin, collection);

  if (collection->all_streams_present)
    GST_DEBUG_OBJECT (dbin, "All streams are now present for collection");

  /* If the output collection didn't change, we can go and check if it's time to
   * switch */
  if (collection == dbin->output_collection)
    goto check_for_switch;

  /* Collection is different */
  GST_DEBUG_OBJECT (slot->src_pad, "Stream belongs to a new collection");

  /* Make sure the collection has a valid selection at this point. */
  update_requested_selection (dbin, collection);

  /* NOTE: The assumption here would be that the collection for this stream is
   * the "next" collection in the list of collections (i.e. the one following
   * the current outputted one).
   *
   * But this is not always true since the collections might be rapidly
   * expanding ones (like for example when a mpeg-ps or rtsp source dynamically
   * adds the streams to a stream collection as it sees them).
   *
   * When switching output collection we need to drain out those "intermediary"
   * collections and make sure they were valid.
   */

  for (tmp = dbin->collections; tmp; tmp = next) {
    DecodebinCollection *candidate = tmp->data;
    next = tmp->next;

    /* We have reached our target */
    if (candidate == collection)
      break;
    /* This is the current output collection */
    if (candidate == dbin->output_collection)
      continue;
    GST_DEBUG_OBJECT (dbin,
        "Dropping intermediary collection %p is_update:%d %" GST_PTR_FORMAT,
        candidate, candidate->is_update, candidate->collection);

    /* Dropping an intermediate collection is only possible if there wasn't any
     * previous output collection or it was an update of the previous one
     */
    g_assert (candidate->is_update || dbin->output_collection == NULL);
    dbin->collections = g_list_remove (dbin->collections, candidate);
    db_collection_free (candidate);
  }

  if (dbin->output_collection == NULL) {
    /* We can switch immediately to this collection */
    dbin->output_collection = collection;
    goto check_for_switch;
  }

  /* If the new collection is fully present, we can switch */
  if (collection->all_streams_present) {
    GST_DEBUG_OBJECT (dbin, "Switching to new output collection");
    dbin->collections =
        g_list_remove (dbin->collections, dbin->output_collection);
    db_collection_free (dbin->output_collection);
    dbin->output_collection = collection;
  }

check_for_switch:
  if (!dbin->upstream_handles_selection && collection == dbin->output_collection
      && collection->all_streams_present) {
    handle_stream_switch (dbin);
  }

beach:
  gst_object_unref (stream);
  SELECTION_UNLOCK (dbin);

  return GST_PAD_PROBE_OK;
}

static GstPadProbeReturn
multiqueue_src_probe (GstPad * pad, GstPadProbeInfo * info,
    MultiQueueSlot * slot)
{
  GstPadProbeReturn ret = GST_PAD_PROBE_OK;
  GstDecodebin3 *dbin = slot->dbin;

  if (GST_IS_EVENT (GST_PAD_PROBE_INFO_DATA (info))) {
    GstEvent *ev = GST_PAD_PROBE_INFO_EVENT (info);

    GST_DEBUG_OBJECT (pad, "Got event %p %s", ev, GST_EVENT_TYPE_NAME (ev));
    switch (GST_EVENT_TYPE (ev)) {
      case GST_EVENT_STREAM_START:
        ret = mq_slot_handle_stream_start (slot, (GstEvent *) ev);
        break;
      case GST_EVENT_CAPS:
      {
        /* Configure the output slot if needed */
        mq_slot_check_reconfiguration (slot);
      }
        break;
      case GST_EVENT_EOS:
      {
        gboolean was_drained = slot->is_drained;
        slot->is_drained = TRUE;

        /* Custom EOS handling first */
        if (gst_mini_object_get_qdata (GST_MINI_OBJECT_CAST (ev),
                CUSTOM_EOS_QUARK)) {
          /* remove custom-eos */
          ev = gst_event_make_writable (ev);
          GST_PAD_PROBE_INFO_DATA (info) = ev;
          gst_mini_object_set_qdata (GST_MINI_OBJECT_CAST (ev),
              CUSTOM_EOS_QUARK, NULL, NULL);

          GST_LOG_OBJECT (pad, "Received custom EOS");
          ret = GST_PAD_PROBE_HANDLED;
          SELECTION_LOCK (dbin);
          if (slot->input == NULL) {
            GST_DEBUG_OBJECT (pad,
                "Got custom-eos from null input stream, removing slot");
            remove_slot_from_streaming_thread (dbin, slot);
            ret = GST_PAD_PROBE_REMOVE;
          } else if (!was_drained) {
            check_and_drain_multiqueue_locked (dbin, ev);
          }
          if (ret == GST_PAD_PROBE_HANDLED)
            gst_event_unref (ev);
          SELECTION_UNLOCK (dbin);
          break;
        }

        GST_FIXME_OBJECT (pad, "EOS on multiqueue source pad. input:%p",
            slot->input);
        if (slot->input == NULL) {
          GstPad *peer;
          GST_DEBUG_OBJECT (pad,
              "last EOS for input, forwarding and removing slot");
          peer = gst_pad_get_peer (pad);
          if (peer) {
            gst_pad_send_event (peer, gst_event_ref (ev));
            gst_object_unref (peer);
          }

          SELECTION_LOCK (dbin);
          /* FIXME: Removing the slot is async, which means actually
           * unlinking the pad is async. Other things like stream-start
           * might flow through this (now unprobed) link before it actually
           * gets released */
          remove_slot_from_streaming_thread (dbin, slot);
          SELECTION_UNLOCK (dbin);
          ret = GST_PAD_PROBE_REMOVE;
        } else if (gst_mini_object_get_qdata (GST_MINI_OBJECT_CAST (ev),
                CUSTOM_FINAL_EOS_QUARK)) {
          GST_DEBUG_OBJECT (pad, "Got final eos, propagating downstream");
        } else {
          GST_DEBUG_OBJECT (pad, "Got regular eos (all_inputs_are_eos)");
          /* drop current event as eos will be sent in check_all_slot_for_eos
           * when all output streams are also eos */
          ret = GST_PAD_PROBE_DROP;
          SELECTION_LOCK (dbin);
          check_and_drain_multiqueue_locked (dbin, ev);
          SELECTION_UNLOCK (dbin);
        }
      }
        break;
      default:
        break;
    }
  } else if (GST_IS_QUERY (GST_PAD_PROBE_INFO_DATA (info))) {
    GstQuery *query = GST_PAD_PROBE_INFO_QUERY (info);
    switch (GST_QUERY_TYPE (query)) {
      case GST_QUERY_CAPS:
      {
        GST_DEBUG_OBJECT (pad, "Intercepting CAPS query");
        gst_query_set_caps_result (query, GST_CAPS_ANY);
        ret = GST_PAD_PROBE_HANDLED;
      }
        break;

      case GST_QUERY_ACCEPT_CAPS:
      {
        GST_DEBUG_OBJECT (pad, "Intercepting Accept Caps query");
        /* If the current decoder doesn't accept caps, we'll reconfigure
         * on the actual caps event. So accept any caps. */
        gst_query_set_accept_caps_result (query, TRUE);
        ret = GST_PAD_PROBE_HANDLED;
      }
      default:
        break;
    }
  }

  return ret;
}

/* Create a new multiqueue slot for the given type
 *
 * It is up to the caller to know whether that slot is needed or not
 * (and release it when no longer needed) */
static MultiQueueSlot *
create_new_slot (GstDecodebin3 * dbin, GstStreamType type)
{
  MultiQueueSlot *slot;
  GstIterator *it = NULL;
  GValue item = { 0, };

  GST_DEBUG_OBJECT (dbin, "Creating new slot for type %s",
      gst_stream_type_get_name (type));
  slot = g_new0 (MultiQueueSlot, 1);
  slot->dbin = dbin;

  slot->id = dbin->slot_id++;

  slot->type = type;
  slot->sink_pad = gst_element_request_pad_simple (dbin->multiqueue, "sink_%u");
  if (slot->sink_pad == NULL)
    goto fail;

  it = gst_pad_iterate_internal_links (slot->sink_pad);
  if (!it || (gst_iterator_next (it, &item)) != GST_ITERATOR_OK
      || ((slot->src_pad = g_value_dup_object (&item)) == NULL)) {
    GST_ERROR ("Couldn't get srcpad from multiqueue for sink pad %s:%s",
        GST_DEBUG_PAD_NAME (slot->src_pad));
    goto fail;
  }
  gst_iterator_free (it);
  g_value_reset (&item);

  g_object_set (slot->sink_pad, "group-id", (guint) type, NULL);

  /* Add event probe */
  slot->probe_id =
      gst_pad_add_probe (slot->src_pad,
      GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM | GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM,
      (GstPadProbeCallback) multiqueue_src_probe, slot, NULL);

  GST_DEBUG ("Created new slot %u (%p) (%s:%s)", slot->id, slot,
      GST_DEBUG_PAD_NAME (slot->src_pad));

  dbin->slots = g_list_append (dbin->slots, slot);

  return slot;

  /* ERRORS */
fail:
  {
    if (slot->sink_pad)
      gst_element_release_request_pad (dbin->multiqueue, slot->sink_pad);
    g_free (slot);
    return NULL;
  }
}

/** gst_decodebin_get_slot_for_input_stream_locked:
 * @dbin: #GstDecodebin3
 * @input_stream: A #DecodebinInputstream
 *
 * Finds and returns the #MultiQueueslot for the given @input_stream. If needed
 * it will create a new one.
 *
 * Must be called with the SELECTION_LOCK taken
 */
static MultiQueueSlot *
gst_decodebin_get_slot_for_input_stream_locked (GstDecodebin3 * dbin,
    DecodebinInputStream * input_stream)
{
  GList *tmp;
  MultiQueueSlot *empty_slot = NULL;
  GstStreamType input_type = 0;
  gchar *stream_id = NULL;

  GST_DEBUG_OBJECT (dbin, "input %p (stream %p %s)",
      input_stream, input_stream->active_stream,
      input_stream->
      active_stream ? gst_stream_get_stream_id (input_stream->active_stream) :
      "");

  if (input_stream->active_stream) {
    input_type = gst_stream_get_stream_type (input_stream->active_stream);
    stream_id =
        (gchar *) gst_stream_get_stream_id (input_stream->active_stream);
  }

  /* Go over existing slots and check if there is already one for it */
  for (tmp = dbin->slots; tmp; tmp = tmp->next) {
    MultiQueueSlot *slot = (MultiQueueSlot *) tmp->data;
    /* Already used input, return that one */
    if (slot->input == input_stream) {
      GST_DEBUG_OBJECT (dbin, "Returning already specified slot %d", slot->id);
      if (input_type && slot->type != input_type) {
        /* The input stream type has changed. It is the responsibility of the
         * user of decodebin3 to ensure that the inputs are coherent.
         *
         * The only case where the stream type will change is when switching
         * between sources which have non-intersecting stream types (ex:
         * switching from audio-only file to video-only file)
         *
         * NOTE : We need to change the slot type here, since it is notified as
         * soon as the *input* of the slot changes.
         */
        GST_DEBUG_OBJECT (dbin, "Changing multiqueue slot stream type");
        slot->type = input_type;
      }
      return slot;
    }
  }

  /* Go amongst all unused slots of the right type and try to find a candidate */
  for (tmp = dbin->slots; tmp; tmp = tmp->next) {
    MultiQueueSlot *slot = (MultiQueueSlot *) tmp->data;
    if (slot->input == NULL && input_type == slot->type) {
      /* Remember this empty slot for later */
      empty_slot = slot;
      /* Check if available slot is of the same stream_id */
      GST_LOG_OBJECT (dbin, "Checking candidate slot %d (active_stream:%p)",
          slot->id, slot->active_stream);
      if (stream_id && slot->active_stream) {
        GST_DEBUG_OBJECT (dbin, "Checking slot %d %s against %s", slot->id,
            slot->active_stream_id, stream_id);
        if (!g_strcmp0 (stream_id, slot->active_stream_id))
          break;
      }
    }
  }

  if (empty_slot) {
    GST_DEBUG_OBJECT (dbin, "Re-using existing unused slot %d", empty_slot->id);
    return empty_slot;
  }

  if (input_type)
    return create_new_slot (dbin, input_type);

  return NULL;
}

/** gst_decodebin_input_link_to_slot:
 * @input_stream: A #DecodebinInputStream
 *
 * Figures out the appropriate #MultiQueueSlot for @input_stream and links to it
 *
 * Must be called with the SELECTION_LOCK taken
 */
static void
gst_decodebin_input_link_to_slot (DecodebinInputStream * input_stream)
{
  GstDecodebin3 *dbin = input_stream->dbin;
  MultiQueueSlot *slot =
      gst_decodebin_get_slot_for_input_stream_locked (dbin, input_stream);

  if (slot->input != NULL && slot->input != input_stream) {
    GST_ERROR_OBJECT (slot->dbin, "Input stream is already linked to a slot");
    return;
  }
  gst_pad_link_full (input_stream->srcpad, slot->sink_pad,
      GST_PAD_LINK_CHECK_NOTHING);
  slot->pending_stream = input_stream->active_stream;
  slot->input = input_stream;
}

static GList *
create_decoder_factory_list (GstDecodebin3 * dbin, GstCaps * caps)
{
  GList *res;

  g_mutex_lock (&dbin->factories_lock);
  gst_decode_bin_update_factories_list (dbin);
  res = gst_element_factory_list_filter (dbin->decoder_factories,
      caps, GST_PAD_SINK, TRUE);
  g_mutex_unlock (&dbin->factories_lock);
  return res;
}

static GstPadProbeReturn
keyframe_waiter_probe (GstPad * pad, GstPadProbeInfo * info,
    MultiQueueSlot * slot)
{
  GstBuffer *buf = GST_PAD_PROBE_INFO_BUFFER (info);

  /* If we have a keyframe, remove the probe and let all data through */
  if (!GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_DELTA_UNIT) ||
      GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_HEADER)) {
    GST_DEBUG_OBJECT (pad,
        "Buffer is keyframe or header, letting through and removing probe");
    slot->drop_probe_id = 0;
    return GST_PAD_PROBE_REMOVE;
  }
  GST_DEBUG_OBJECT (pad, "Buffer is not a keyframe, dropping");
  return GST_PAD_PROBE_DROP;
}

static gboolean
clear_sticky_events (GstPad * pad, GstEvent ** event, gpointer user_data)
{
  GST_DEBUG_OBJECT (pad, "clearing sticky event %" GST_PTR_FORMAT, *event);
  gst_event_unref (*event);
  *event = NULL;
  return TRUE;
}

static gboolean
copy_sticky_events (GstPad * pad, GstEvent ** event, gpointer user_data)
{
  GstPad *gpad = GST_PAD_CAST (user_data);

  GST_DEBUG_OBJECT (gpad, "store sticky event %" GST_PTR_FORMAT, *event);
  gst_pad_store_sticky_event (gpad, *event);

  return TRUE;
}

static gboolean
decode_pad_set_target (GstGhostPad * pad, GstPad * target)
{
  gboolean res = gst_ghost_pad_set_target (pad, target);
  if (!res)
    return res;

  if (target == NULL)
    gst_pad_sticky_events_foreach (GST_PAD_CAST (pad), clear_sticky_events,
        NULL);
  else
    gst_pad_sticky_events_foreach (target, copy_sticky_events, pad);

  return res;
}

static void
db_output_stream_expose_src_pad (DecodebinOutputStream * output)
{
  MultiQueueSlot *slot = output->slot;
  GstEvent *stream_start;

  if (output->src_exposed)
    return;

  stream_start =
      gst_pad_get_sticky_event (slot->src_pad, GST_EVENT_STREAM_START, 0);

  /* Ensure GstStream is accesiable from pad-added callback */
  if (stream_start) {
    gst_pad_store_sticky_event (output->src_pad, stream_start);
    gst_event_unref (stream_start);
  } else {
    GST_WARNING_OBJECT (slot->src_pad, "Pad has no stored stream-start event");
  }

  output->src_exposed = TRUE;
  gst_element_add_pad (GST_ELEMENT_CAST (slot->dbin), output->src_pad);
}

static CandidateDecoder *
add_candidate_decoder (GstDecodebin3 * dbin, GstElement * element)
{
  GST_OBJECT_LOCK (dbin);
  CandidateDecoder *candidate;
  candidate = g_new0 (CandidateDecoder, 1);
  candidate->element = element;
  dbin->candidate_decoders =
      g_list_prepend (dbin->candidate_decoders, candidate);
  GST_OBJECT_UNLOCK (dbin);
  return candidate;
}

static void
remove_candidate_decoder (GstDecodebin3 * dbin, CandidateDecoder * candidate)
{
  GST_OBJECT_LOCK (dbin);
  dbin->candidate_decoders =
      g_list_remove (dbin->candidate_decoders, candidate);
  if (candidate->error)
    gst_message_unref (candidate->error);
  g_free (candidate);
  GST_OBJECT_UNLOCK (dbin);
}

/** db_output_stream_setup_decoder:
 * @output: A #DecodebinOutputStream
 * @caps: (transfer none): The #GstCaps for which we want a decoder
 * @msg: A pointer to a #GstMessage
 *
 * Finds the appropriate decoder for @caps and sets it up. If the @caps match
 * the decodebin output caps, it will be configured to propagate the stream
 * as-is without any decoder.
 *
 * Returns: #TRUE if a decoder was found and properly setup, else #FALSE. If the
 * failure was due to missing plugins, then @msg will be properly filled up.
 */
static gboolean
db_output_stream_setup_decoder (DecodebinOutputStream * output,
    GstCaps * new_caps, GstMessage ** msg)
{
  gboolean ret = TRUE;
  GstDecodebin3 *dbin = output->dbin;
  MultiQueueSlot *slot = output->slot;
  GList *factories, *next_factory;

  GST_DEBUG_OBJECT (dbin, "output %s:%s caps %" GST_PTR_FORMAT,
      GST_DEBUG_PAD_NAME (output->src_pad), new_caps);

  /* If no decoder is required, use the slot source pad and we're done */
  if (gst_caps_can_intersect (new_caps, dbin->caps)) {
    output->decoder_src = gst_object_ref (slot->src_pad);
    goto done;
  }

  factories = next_factory = create_decoder_factory_list (dbin, new_caps);
  if (!next_factory) {
    GST_DEBUG ("Could not find an element for caps %" GST_PTR_FORMAT, new_caps);
    g_assert (output->decoder == NULL);
    ret = FALSE;
    goto missing_decoder;
  }

  while (next_factory) {
    CandidateDecoder *candidate = NULL;

    /* If we don't have a decoder yet, instantiate one */
    output->decoder = gst_element_factory_create (
        (GstElementFactory *) next_factory->data, NULL);
    GST_DEBUG ("Trying decoder %" GST_PTR_FORMAT, output->decoder);

    if (output->decoder == NULL)
      goto try_next;

    if (!gst_bin_add ((GstBin *) dbin, output->decoder)) {
      GST_WARNING_OBJECT (dbin, "could not add decoder '%s' to pipeline",
          GST_ELEMENT_NAME (output->decoder));
      goto try_next;
    }
    output->decoder_sink = gst_element_get_static_pad (output->decoder, "sink");
    output->decoder_src = gst_element_get_static_pad (output->decoder, "src");

    candidate = add_candidate_decoder (dbin, output->decoder);
    if (gst_pad_link_full (slot->src_pad, output->decoder_sink,
            GST_PAD_LINK_CHECK_NOTHING) != GST_PAD_LINK_OK) {
      GST_WARNING_OBJECT (dbin, "could not link to %s:%s",
          GST_DEBUG_PAD_NAME (output->decoder_sink));
      goto try_next;
    }
    output->linked = TRUE;

    if (gst_element_set_state (output->decoder, GST_STATE_READY) ==
        GST_STATE_CHANGE_FAILURE) {
      GST_WARNING_OBJECT (dbin, "Decoder '%s' failed to reach READY state",
          GST_ELEMENT_NAME (output->decoder));
      goto try_next;
    }

    if (!gst_pad_query_accept_caps (output->decoder_sink, new_caps)) {
      GST_DEBUG_OBJECT (dbin,
          "Decoder '%s' did not accept the caps, trying the next type",
          GST_ELEMENT_NAME (output->decoder));
      goto try_next;
    }

    if (gst_element_set_state (output->decoder, GST_STATE_PAUSED) ==
        GST_STATE_CHANGE_FAILURE) {
      GST_WARNING_OBJECT (dbin, "Decoder '%s' failed to reach PAUSED state",
          GST_ELEMENT_NAME (output->decoder));
      goto try_next;
    }

    /* Everything went well, we have a decoder */
    GST_DEBUG ("created decoder %" GST_PTR_FORMAT, output->decoder);

    handle_stored_latency_message (dbin, output, candidate);
    remove_candidate_decoder (dbin, candidate);
    break;

  try_next:{
      db_output_stream_reset (output);
      if (candidate)
        remove_candidate_decoder (dbin, candidate);

      if (!next_factory->next) {
        ret = FALSE;
        if (output->decoder == NULL)
          goto missing_decoder;
        goto cleanup;
      }
      next_factory = next_factory->next;
    }
  }
  gst_plugin_feature_list_free (factories);

done:
  if (output->type & GST_STREAM_TYPE_VIDEO && slot->drop_probe_id == 0) {
    GST_DEBUG_OBJECT (dbin, "Adding keyframe-waiter probe");
    slot->drop_probe_id =
        gst_pad_add_probe (slot->src_pad, GST_PAD_PROBE_TYPE_BUFFER,
        (GstPadProbeCallback) keyframe_waiter_probe, slot, NULL);
  }

  /* Set the decode pad target */
  decode_pad_set_target ((GstGhostPad *) output->src_pad, output->decoder_src);

  /* Expose the source pad if needed */
  db_output_stream_expose_src_pad (output);

  if (output->decoder)
    gst_element_sync_state_with_parent (output->decoder);

  return ret;

missing_decoder:
  {
    GstCaps *caps;

    caps = gst_stream_get_caps (slot->active_stream);
    GST_DEBUG_OBJECT (slot->src_pad,
        "We are missing a decoder for %" GST_PTR_FORMAT, caps);
    *msg = gst_missing_decoder_message_new (GST_ELEMENT_CAST (dbin), caps);
    gst_caps_unref (caps);

    /* FALLTHROUGH */
  }

cleanup:
  {
    GST_DEBUG_OBJECT (dbin, "Cleanup");
    if (output->decoder_sink) {
      gst_object_unref (output->decoder_sink);
      output->decoder_sink = NULL;
    }
    if (output->decoder_src) {
      gst_object_unref (output->decoder_src);
      output->decoder_src = NULL;
    }
    if (output->decoder) {
      gst_element_set_state (output->decoder, GST_STATE_NULL);
      gst_bin_remove ((GstBin *) dbin, output->decoder);
      output->decoder = NULL;
    }
    return ret;
  }
}

/** db_output_stream_reconfigure:
 * @output: A #DecodebinOutputStream
 * @msg: A pointer to a #GstMessage
 *
 * (Re)Configure the @output for the associated slot active stream.
 * 
 * Returns: #TRUE if the output was properly (re)configured. #FALSE if it
 * failed, in which case the stream shouldn't be used and the @msg might contain
 * a message to be posted on the bus.
 */
static gboolean
db_output_stream_reconfigure (DecodebinOutputStream * output, GstMessage ** msg)
{
  MultiQueueSlot *slot = output->slot;
  GstDecodebin3 *dbin = output->dbin;
  GstCaps *new_caps = (GstCaps *) gst_stream_get_caps (slot->active_stream);
  gboolean needs_decoder;
  gboolean ret = TRUE;

  needs_decoder = gst_caps_can_intersect (new_caps, dbin->caps) != TRUE;

  GST_DEBUG_OBJECT (dbin,
      "Reconfiguring output %s:%s to slot %s:%s, needs_decoder:%d",
      GST_DEBUG_PAD_NAME (output->src_pad), GST_DEBUG_PAD_NAME (slot->src_pad),
      needs_decoder);

  /* First check if we can re-use the output as-is for the new caps:
   * * Either we have a decoder and it can accept the new caps
   * * Or we don't have one and don't need one
   */

  /* If we need a decoder and the existing one can accept the new caps, re-use it */
  if (needs_decoder && output->decoder &&
      gst_pad_query_accept_caps (output->decoder_sink, new_caps)) {
    GST_DEBUG_OBJECT (dbin,
        "Reusing existing decoder '%" GST_PTR_FORMAT "' for slot %p",
        output->decoder, slot);
    /* Re-add the keyframe-waiter probe */
    if (output->type & GST_STREAM_TYPE_VIDEO && slot->drop_probe_id == 0) {
      GST_DEBUG_OBJECT (dbin, "Adding keyframe-waiter probe");
      slot->drop_probe_id =
          gst_pad_add_probe (slot->src_pad, GST_PAD_PROBE_TYPE_BUFFER,
          (GstPadProbeCallback) keyframe_waiter_probe, slot, NULL);
    }
    if (output->linked == FALSE) {
      gst_pad_link_full (slot->src_pad, output->decoder_sink,
          GST_PAD_LINK_CHECK_NOTHING);
      output->linked = TRUE;
    }
  } else {
    /* We need to reset the output and set it up again */
    db_output_stream_reset (output);

    /* Setup the decoder */
    ret = db_output_stream_setup_decoder (output, new_caps, msg);
  }

  gst_caps_unref (new_caps);

  return ret;
}

static GstPadProbeReturn
idle_reconfigure (GstPad * pad, GstPadProbeInfo * info, MultiQueueSlot * slot)
{
  mq_slot_check_reconfiguration (slot);

  return GST_PAD_PROBE_REMOVE;
}

static MultiQueueSlot *
find_slot_for_stream_id (GstDecodebin3 * dbin, const gchar * sid)
{
  GList *tmp;

  for (tmp = dbin->slots; tmp; tmp = tmp->next) {
    MultiQueueSlot *slot = (MultiQueueSlot *) tmp->data;
    if (!g_strcmp0 (sid, slot->active_stream_id))
      return slot;
    if (slot->pending_stream && slot->pending_stream != slot->active_stream) {
      const gchar *stream_id = gst_stream_get_stream_id (slot->pending_stream);
      if (!g_strcmp0 (sid, stream_id))
        return slot;
    }
  }

  return NULL;
}

/* This function handles the reassignment of a slot. Call this from
 * the streaming thread of a slot. */
static void
mq_slot_reassign (MultiQueueSlot * slot)
{
  GstDecodebin3 *dbin = slot->dbin;
  DecodebinOutputStream *output;
  MultiQueueSlot *target_slot = NULL;
  GList *tmp;
  DecodebinCollection *collection = dbin->output_collection;

  SELECTION_LOCK (dbin);
  output = slot->output;

  if (G_UNLIKELY (slot->active_stream == NULL || output == NULL)) {
    GST_DEBUG_OBJECT (slot->src_pad, "Called on slot not active or requested");
    SELECTION_UNLOCK (dbin);
    return;
  }

  GST_DEBUG_OBJECT (slot->src_pad, "stream: %s", slot->active_stream_id);

  /* Recheck whether this stream is still in the list of streams to deactivate */
  if (stream_is_requested (dbin, slot->active_stream_id)) {
    /* Stream is in the list of requested streams, don't remove */
    SELECTION_UNLOCK (dbin);
    GST_DEBUG_OBJECT (slot->src_pad,
        "Stream '%s' doesn't need to be deactivated", slot->active_stream_id);
    return;
  }

  /* Unlink slot from output */
  GST_DEBUG_OBJECT (slot->src_pad, "Unlinking from previous output");
  mq_slot_set_output (slot, NULL);

  /* Can we re-assign this output to a requested stream ? */
  GST_DEBUG_OBJECT (slot->src_pad, "Attempting to re-assing output stream");
  for (tmp = collection->to_activate; tmp; tmp = tmp->next) {
    MultiQueueSlot *tslot = find_slot_for_stream_id (dbin, tmp->data);
    GST_LOG_OBJECT (slot->src_pad,
        "Checking slot %s:%s (output:%p , stream:%s)",
        GST_DEBUG_PAD_NAME (tslot->src_pad), tslot->output,
        tslot->active_stream_id);
    if (tslot && tslot->type == output->type && tslot->output == NULL) {
      GST_DEBUG_OBJECT (slot->src_pad, "Using %s:%s as reassigned slot",
          GST_DEBUG_PAD_NAME (tslot->src_pad));
      target_slot = tslot;
      collection->to_activate =
          g_list_delete_link (collection->to_activate, tmp);
      break;
    }
  }

  if (target_slot) {
    GST_DEBUG_OBJECT (slot->src_pad, "Assigning output to slot %s:%s '%s'",
        GST_DEBUG_PAD_NAME (target_slot->src_pad),
        target_slot->active_stream_id);
    mq_slot_set_output (target_slot, output);
    SELECTION_UNLOCK (dbin);

    /* Wakeup the target slot so that it retries to send events/buffers
     * thereby triggering the output reconfiguration codepath */
    gst_pad_add_probe (target_slot->src_pad, GST_PAD_PROBE_TYPE_IDLE,
        (GstPadProbeCallback) idle_reconfigure, target_slot, NULL);
  } else {
    GstMessage *msg;

    GST_DEBUG_OBJECT (slot->src_pad, "No target slot, removing output");
    dbin->output_streams = g_list_remove (dbin->output_streams, output);
    db_output_stream_free (output);
    msg = is_selection_done (slot->dbin);
    SELECTION_UNLOCK (dbin);

    if (msg)
      gst_element_post_message ((GstElement *) slot->dbin, msg);
  }
}

/* Idle probe called when a slot should be unassigned from its output stream.
 * This is needed to ensure nothing is flowing when unlinking the slot.
 *
 * Also, this method will search for a pending stream which could re-use
 * the output stream. */
static GstPadProbeReturn
mq_slot_unassign_probe (GstPad * pad, GstPadProbeInfo * info,
    MultiQueueSlot * slot)
{
  mq_slot_reassign (slot);

  return GST_PAD_PROBE_REMOVE;
}

/** handle_stream_switch:
 * @dbin: A #GstDecodebin3
 *
 * Figures out which slots to (de)activate for the given output_collection.
 *
 * Must be called with SELECTION_LOCK taken.
 */
static void
handle_stream_switch (GstDecodebin3 * dbin)
{
  DecodebinCollection *collection = dbin->output_collection;
  GList *tmp;
  /* List of slots to (de)activate. */
  GList *slots_to_deactivate = NULL;
  GList *slots_to_activate = NULL;

  GList *streams_to_reassign = NULL;
  GList *future_request_streams = NULL;
  GList *pending_streams = NULL;
  GList *slots_to_reassign = NULL;

  g_return_if_fail (collection);

  /* COMPARE the requested streams to the active and requested streams
   * on multiqueue. */

  /* First check the slots to activate and which ones are unknown */
  for (tmp = collection->requested_selection; tmp; tmp = tmp->next) {
    const gchar *sid = (const gchar *) tmp->data;
    MultiQueueSlot *slot;
    GST_DEBUG_OBJECT (dbin, "Checking for requested stream '%s'", sid);
    slot = find_slot_for_stream_id (dbin, sid);

    /* Find the multiqueue slot which is outputting, or will output, this stream
     * id */
    if (slot == NULL || slot->active_stream == NULL) {
      /* There is no slot on which this stream is active */
      GST_DEBUG_OBJECT (dbin, "Adding to pending streams '%s'", sid);
      pending_streams = g_list_append (pending_streams, (gchar *) sid);
    } else if (slot->output == NULL) {
      /* There is a slot on which this stream is active or pending */
      GST_DEBUG_OBJECT (dbin,
          "We need to activate slot %s:%s for stream '%s')",
          GST_DEBUG_PAD_NAME (slot->src_pad), sid);
      slots_to_activate = g_list_append (slots_to_activate, slot);
    } else {
      GST_DEBUG_OBJECT (dbin,
          "Stream '%s' from slot %s:%s is already active on output %p", sid,
          GST_DEBUG_PAD_NAME (slot->src_pad), slot->output);
      future_request_streams =
          g_list_append (future_request_streams, (gchar *) sid);
    }
  }

  for (tmp = dbin->slots; tmp; tmp = tmp->next) {
    MultiQueueSlot *slot = (MultiQueueSlot *) tmp->data;
    /* For slots that have an output, check if it's part of the streams to
     * be active */
    if (slot->output) {
      gboolean slot_to_deactivate = TRUE;

      if (slot->active_stream) {
        if (stream_in_list (collection->requested_selection,
                slot->active_stream_id))
          slot_to_deactivate = FALSE;
      }
      if (slot_to_deactivate && slot->pending_stream
          && slot->pending_stream != slot->active_stream) {
        if (stream_in_list (collection->requested_selection,
                gst_stream_get_stream_id (slot->pending_stream)))
          slot_to_deactivate = FALSE;
      }
      if (slot_to_deactivate) {
        GST_DEBUG_OBJECT (dbin,
            "Slot %s:%s (%s) should be deactivated, no longer used",
            GST_DEBUG_PAD_NAME (slot->src_pad), slot->active_stream_id);
        slots_to_deactivate = g_list_append (slots_to_deactivate, slot);
      }
    }
  }

  if (slots_to_deactivate != NULL) {
    GST_DEBUG_OBJECT (dbin, "Check if we can reassign slots");
    /* We need to compare what needs to be activated and deactivated in order
     * to determine whether there are outputs that can be transferred */
    /* Take the stream-id of the slots that are to be activated, for which there
     * is a slot of the same type that needs to be deactivated */
    tmp = slots_to_deactivate;
    while (tmp) {
      MultiQueueSlot *slot_to_deactivate = (MultiQueueSlot *) tmp->data;
      gboolean removeit = FALSE;
      GList *tmp2, *next;
      GST_DEBUG_OBJECT (dbin,
          "Checking if slot to deactivate (%s:%s) has a candidate slot to activate",
          GST_DEBUG_PAD_NAME (slot_to_deactivate->src_pad));
      for (tmp2 = slots_to_activate; tmp2; tmp2 = tmp2->next) {
        MultiQueueSlot *slot_to_activate = (MultiQueueSlot *) tmp2->data;
        GST_DEBUG_OBJECT (dbin, "Comparing to slot %s:%s (%s)",
            GST_DEBUG_PAD_NAME (slot_to_activate->src_pad),
            slot_to_activate->active_stream_id);
        if (slot_to_activate->type == slot_to_deactivate->type) {
          GST_DEBUG_OBJECT (dbin, "Re-using");
          streams_to_reassign = g_list_append (streams_to_reassign, (gchar *)
              slot_to_activate->active_stream_id);
          slots_to_reassign =
              g_list_append (slots_to_reassign, slot_to_deactivate);
          slots_to_activate =
              g_list_remove (slots_to_activate, slot_to_activate);
          removeit = TRUE;
          break;
        }
      }
      next = tmp->next;
      if (removeit)
        slots_to_deactivate = g_list_delete_link (slots_to_deactivate, tmp);
      tmp = next;
    }
  }

  for (tmp = slots_to_deactivate; tmp; tmp = tmp->next) {
    MultiQueueSlot *slot = (MultiQueueSlot *) tmp->data;
    GST_DEBUG_OBJECT (dbin,
        "Really need to deactivate slot %s:%s (%s), but no available alternative",
        GST_DEBUG_PAD_NAME (slot->src_pad), slot->active_stream_id);

    slots_to_reassign = g_list_append (slots_to_reassign, slot);
  }

  /* The only slots left to activate are the ones that won't be reassigned and
   * therefore really need to have a new output created */
  for (tmp = slots_to_activate; tmp; tmp = tmp->next) {
    MultiQueueSlot *slot = (MultiQueueSlot *) tmp->data;
    if (slot->active_stream)
      future_request_streams =
          g_list_append (future_request_streams,
          (gchar *) slot->active_stream_id);
    else if (slot->pending_stream)
      future_request_streams =
          g_list_append (future_request_streams,
          (gchar *) gst_stream_get_stream_id (slot->pending_stream));
    else
      GST_ERROR_OBJECT (dbin, "No stream for slot %s:%s !!",
          GST_DEBUG_PAD_NAME (slot->src_pad));
  }

  if (slots_to_activate == NULL && pending_streams != NULL) {
    GST_ERROR_OBJECT (dbin, "Stream switch requested for future collection");
    g_list_free (slots_to_deactivate);
    g_list_free (pending_streams);
    slots_to_deactivate = NULL;
    pending_streams = NULL;
    /* This should never happen, this function is only called for streams present */
    g_assert (FALSE);
  } else {
    if (collection->to_activate)
      g_list_free (collection->to_activate);
    collection->to_activate = g_list_copy (streams_to_reassign);
  }

  SELECTION_UNLOCK (dbin);

  if (slots_to_activate && !slots_to_reassign) {
    for (tmp = slots_to_activate; tmp; tmp = tmp->next) {
      MultiQueueSlot *slot = (MultiQueueSlot *) tmp->data;
      gst_pad_add_probe (slot->src_pad, GST_PAD_PROBE_TYPE_IDLE,
          (GstPadProbeCallback) idle_reconfigure, slot, NULL);
    }
  }

  /* For all streams to deactivate, add an idle probe where we will do
   * the unassignment and switch over */
  for (tmp = slots_to_reassign; tmp; tmp = tmp->next) {
    MultiQueueSlot *slot = (MultiQueueSlot *) tmp->data;
    gst_pad_add_probe (slot->src_pad, GST_PAD_PROBE_TYPE_IDLE,
        (GstPadProbeCallback) mq_slot_unassign_probe, slot, NULL);
  }

  if (slots_to_deactivate)
    g_list_free (slots_to_deactivate);
  if (slots_to_activate)
    g_list_free (slots_to_activate);
  if (streams_to_reassign)
    g_list_free (streams_to_reassign);
  if (future_request_streams)
    g_list_free (future_request_streams);
  if (pending_streams)
    g_list_free (pending_streams);
  if (slots_to_reassign)
    g_list_free (slots_to_reassign);

  SELECTION_LOCK (dbin);
}

/*
 * * event : (transfer full): The select streams event
 *
 * Handles a GST_EVENT_SELECT_STREAMS (from application or downstream)
 *
 * Returns: TRUE if the event was handled, or FALSE if it should be forwarded to
 * the default handler.
 */
static gboolean
handle_select_streams (GstDecodebin3 * dbin, GstEvent * event)
{
  GList *streams = NULL;
  guint32 seqnum = gst_event_get_seqnum (event);
  GList *tmp;
  DecodebinCollection *collection = NULL;

  if (dbin->upstream_handles_selection) {
    GST_DEBUG_OBJECT (dbin, "Letting select-streams event flow upstream");
    return FALSE;
  }

  gst_event_parse_select_streams (event, &streams);
  if (streams == NULL) {
    GST_DEBUG_OBJECT (dbin, "No streams in select streams");
    gst_event_unref (event);
    return TRUE;
  }

  SELECTION_LOCK (dbin);
  /* Find the collection to which these list of streams apply */
  for (tmp = dbin->collections; tmp; tmp = tmp->next) {
    DecodebinCollection *cand = tmp->data;
    if (are_all_streams_in_collection (cand->collection, streams)) {
      collection = cand;
      break;
    }
  }

  if (collection == NULL) {
    GST_WARNING_OBJECT (dbin, "Requested streams from no known collection");
    goto beach;
  }

  if (seqnum == collection->seqnum) {
    GST_DEBUG_OBJECT (dbin,
        "Already handled/handling that SELECT_STREAMS event");
    goto beach;
  }

  /* Update the requested list of streams */
  if (collection->requested_selection) {
    g_list_free_full (collection->requested_selection, g_free);
  }
  /* We give ownership of the streams to the DecodebinCollection */
  collection->requested_selection = streams;
  collection->seqnum = seqnum;
  collection->posted_streams_selected_msg = FALSE;

  /* If the collection is the current output one, handle the switch */
  if (collection == dbin->output_collection)
    handle_stream_switch (dbin);

beach:
  SELECTION_UNLOCK (dbin);
  gst_event_unref (event);
  return TRUE;
}

static GstPadProbeReturn
ghost_pad_event_probe (GstPad * pad, GstPadProbeInfo * info,
    DecodebinOutputStream * output)
{
  GstPadProbeReturn ret = GST_PAD_PROBE_OK;
  GstDecodebin3 *dbin = output->dbin;
  GstEvent *event = GST_PAD_PROBE_INFO_EVENT (info);

  GST_DEBUG_OBJECT (pad, "Got event %p %s", event, GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SELECT_STREAMS:
    {
      if (handle_select_streams (dbin, event))
        ret = GST_PAD_PROBE_HANDLED;
    }
      break;
    default:
      break;
  }

  return ret;
}

static gboolean
gst_decodebin3_send_event (GstElement * element, GstEvent * event)
{
  GstDecodebin3 *dbin = (GstDecodebin3 *) element;

  GST_DEBUG_OBJECT (element, "event %s", GST_EVENT_TYPE_NAME (event));

  if (GST_EVENT_TYPE (event) == GST_EVENT_SELECT_STREAMS
      && handle_select_streams (dbin, event)) {
    return TRUE;
  }

  return GST_ELEMENT_CLASS (parent_class)->send_event (element, event);
}

static void
mq_slot_free (GstDecodebin3 * dbin, MultiQueueSlot * slot)
{
  if (slot->probe_id)
    gst_pad_remove_probe (slot->src_pad, slot->probe_id);
  if (slot->drop_probe_id)
    gst_pad_remove_probe (slot->src_pad, slot->drop_probe_id);
  if (slot->input) {
    if (slot->input->srcpad)
      gst_pad_unlink (slot->input->srcpad, slot->sink_pad);
  }

  gst_element_release_request_pad (dbin->multiqueue, slot->sink_pad);
  gst_object_replace ((GstObject **) & slot->sink_pad, NULL);
  gst_object_replace ((GstObject **) & slot->src_pad, NULL);
  gst_object_replace ((GstObject **) & slot->active_stream, NULL);
  g_free (slot);
}

/** db_output_stream_new:
 * @dbin: A #GstDecodebin3
 * @type: The #GstStreamType
 *
 * Creates a #DecodebinOutputStream for the given type and adds it to the list
 * of available outputs.
 *
 * Returns: a #DecodebinOutputStream for the given @type.
 */
static DecodebinOutputStream *
db_output_stream_new (GstDecodebin3 * dbin, GstStreamType type)
{
  DecodebinOutputStream *res = g_new0 (DecodebinOutputStream, 1);
  gchar *pad_name;
  const gchar *prefix;
  GstStaticPadTemplate *templ;
  GstPadTemplate *ptmpl;
  guint32 *counter;
  GstPad *internal_pad;

  GST_DEBUG_OBJECT (dbin, "Created new output stream %p for type %s",
      res, gst_stream_type_get_name (type));

  res->type = type;
  res->dbin = dbin;
  res->decoder_latency = GST_CLOCK_TIME_NONE;

  if (type & GST_STREAM_TYPE_VIDEO) {
    templ = &video_src_template;
    counter = &dbin->vpadcount;
    prefix = "video";
  } else if (type & GST_STREAM_TYPE_AUDIO) {
    templ = &audio_src_template;
    counter = &dbin->apadcount;
    prefix = "audio";
  } else if (type & GST_STREAM_TYPE_TEXT) {
    templ = &text_src_template;
    counter = &dbin->tpadcount;
    prefix = "text";
  } else {
    templ = &src_template;
    counter = &dbin->opadcount;
    prefix = "src";
  }

  pad_name = g_strdup_printf ("%s_%u", prefix, *counter);
  *counter += 1;
  ptmpl = gst_static_pad_template_get (templ);
  res->src_pad = gst_ghost_pad_new_no_target_from_template (pad_name, ptmpl);
  gst_object_unref (ptmpl);
  g_free (pad_name);
  gst_pad_set_active (res->src_pad, TRUE);
  /* Put an event probe on the internal proxy pad to detect upstream
   * events */
  internal_pad =
      (GstPad *) gst_proxy_pad_get_internal ((GstProxyPad *) res->src_pad);
  gst_pad_add_probe (internal_pad, GST_PAD_PROBE_TYPE_EVENT_UPSTREAM,
      (GstPadProbeCallback) ghost_pad_event_probe, res, NULL);
  gst_object_unref (internal_pad);

  dbin->output_streams = g_list_append (dbin->output_streams, res);

  GST_DEBUG_OBJECT (dbin, "Created output stream %p (%s:%s)", res,
      GST_DEBUG_PAD_NAME (res->src_pad));

  return res;
}

/** db_output_stream_reset:
 * @output: A #DecodebinOutputStream
 *
 * Resets the @output to be able to be re-used by another slot/format. If a
 * decoder is present it will be disabled and removed
 */
static void
db_output_stream_reset (DecodebinOutputStream * output)
{
  MultiQueueSlot *slot = output->slot;

  GST_DEBUG_OBJECT (output->dbin, "Resetting %s:%s",
      GST_DEBUG_PAD_NAME (output->src_pad));

  /* Unlink decoder if needed */
  if (output->linked && slot && output->decoder_sink) {
    gst_pad_unlink (slot->src_pad, output->decoder_sink);
  }
  output->linked = FALSE;

  if (slot && slot->drop_probe_id) {
    gst_pad_remove_probe (slot->src_pad, slot->drop_probe_id);
    slot->drop_probe_id = 0;
  }

  /* Remove/Reset pads */
  gst_object_replace ((GstObject **) & output->decoder_sink, NULL);
  decode_pad_set_target ((GstGhostPad *) output->src_pad, NULL);
  gst_object_replace ((GstObject **) & output->decoder_src, NULL);

  /* Remove decoder */
  if (output->decoder) {
    gst_element_set_locked_state (output->decoder, TRUE);
    gst_element_set_state (output->decoder, GST_STATE_NULL);
    gst_bin_remove ((GstBin *) output->dbin, output->decoder);

    output->decoder = NULL;
    output->decoder_latency = GST_CLOCK_TIME_NONE;
  }

}

/** db_output_stream_free:
 * @output: A #DecodebinOutputstream
 *
 * Releases the @output from the associated slot, removes the associated source
 * ghost pad and frees any decoder
 */
static void
db_output_stream_free (DecodebinOutputStream * output)
{
  GstDecodebin3 *dbin = output->dbin;

  GST_DEBUG_OBJECT (output->src_pad, "Freeing");

  db_output_stream_reset (output);

  if (output->slot)
    mq_slot_set_output (output->slot, NULL);

  if (output->src_exposed) {
    gst_element_remove_pad ((GstElement *) dbin, output->src_pad);
  }
  g_free (output);
}

static GstStateChangeReturn
gst_decodebin3_change_state (GstElement * element, GstStateChange transition)
{
  GstDecodebin3 *dbin = (GstDecodebin3 *) element;
  GstStateChangeReturn ret;

  /* Upwards */
  switch (transition) {
    default:
      break;
  }
  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    goto beach;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_decodebin3_reset (dbin);
      break;
    default:
      break;
  }
beach:
  return ret;
}
