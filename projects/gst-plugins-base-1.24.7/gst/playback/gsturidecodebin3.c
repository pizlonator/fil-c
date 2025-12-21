/* GStreamer
 * Copyright (C) <2007> Wim Taymans <wim.taymans@gmail.com>
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
 * SECTION:element-uridecodebin3
 * @title: uridecodebin3
 *
 * Decodes data from a URI into raw media. It selects a source element that can
 * handle the given #GstURIDecodeBin3:uri scheme and connects it to a decodebin3.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>

#include <gst/gst.h>
#include <glib/gi18n-lib.h>
#include <gst/pbutils/missing-plugins.h>

#include "gstplay-enum.h"
#include "gstrawcaps.h"
#include "gstplaybackelements.h"
#include "gstplaybackutils.h"

#define GST_TYPE_URI_DECODE_BIN3 \
  (gst_uri_decode_bin3_get_type())
#define GST_URI_DECODE_BIN3(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_URI_DECODE_BIN3,GstURIDecodeBin3))
#define GST_URI_DECODE_BIN3_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_URI_DECODE_BIN3,GstURIDecodeBin3Class))
#define GST_IS_URI_DECODE_BIN3(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_URI_DECODE_BIN3))
#define GST_IS_URI_DECODE_BIN3_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_URI_DECODE_BIN3))
#define GST_URI_DECODE_BIN3_CAST(obj) ((GstURIDecodeBin3 *) (obj))

typedef struct _GstSourceGroup GstSourceGroup;
typedef struct _GstURIDecodeBin3 GstURIDecodeBin3;
typedef struct _GstURIDecodeBin3Class GstURIDecodeBin3Class;

typedef struct _GstPlayItem GstPlayItem;
typedef struct _GstSourceItem GstSourceItem;
typedef struct _GstSourceHandler GstSourceHandler;
typedef struct _GstSourcePad GstSourcePad;
typedef struct _OutputPad OutputPad;

/* A structure describing a play item, which travels through the elements
 * over time.
 *
 * All source items in this play item will be played together. Corresponds to an
 * end-user "play item" (ex: one item from a playlist, even though it might be
 * using a main content and subtitle content).
 */
struct _GstPlayItem
{
  GstURIDecodeBin3 *uridecodebin;

  /* Main URI */
  GstSourceItem *main_item;

  /* Auxiliary URI */
  /* FIXME : Replace by a list later */
  GstSourceItem *sub_item;

  /* The group_id used to identify this play item via STREAM_START events
   * This is the group_id which will be used externally (i.e. rewritten
   * to outgoing STREAM_START events and in emitted signals).
   * The urisourcebin-specific group_id is located in GstSourceItem */
  guint group_id;

  /* The two following variables are required for gapless, since there could be
   * a play item which is started which is different from the one currently
   * being outputted */

  /* active: TRUE if the backing urisourcebin were created */
  gboolean active;

  /* Whether about-to-finish was already posted for this play item */
  gboolean posted_about_to_finish;

  /* Whether about-to-finish should be posted once this play item becomes the
   * current input item */
  gboolean pending_about_to_finish;
};

/* The actual "source" component of a "play item"
 *
 * This is defined by having a URI, is backed by a `GstSourceHandler`.
 */
struct _GstSourceItem
{
  /* The GstPlayItem to which this GstSourceItem belongs to */
  GstPlayItem *play_item;

  gchar *uri;

  /* The urisourcebin controlling this uri
   * Can be NULL */
  GstSourceHandler *handler;
};

/* Structure wrapping everything related to a urisourcebin */
struct _GstSourceHandler
{
  GstURIDecodeBin3 *uridecodebin;
  GstPlayItem *play_item;

  GstElement *urisourcebin;

  /* Signal handlers */
  gulong pad_added_id;
  gulong pad_removed_id;
  gulong source_setup_id;
  gulong about_to_finish_id;

  /* TRUE if the controlled urisourcebin was added to uridecodebin */
  gboolean active;

  /* TRUE if the urisourcebin handles main item */
  gboolean is_main_source;

  /* buffering message stored for after switching */
  GstMessage *pending_buffering_msg;

  /* TRUE if urisourcebin handles stream-selection */
  gboolean upstream_selected;

  /* Number of expected sourcepads. Default 1, else it's the number of streams
   * specified by GST_MESSAGE_SELECTED_STREAMS from the source */
  guint expected_pads;

  /* List of GstSourcePad */
  GList *sourcepads;

  /* If we saw a redirection message from this source. */
  gboolean saw_redirection;
};

/* Structure wrapping everything related to a urisourcebin pad */
struct _GstSourcePad
{
  GstSourceHandler *handler;

  GstPad *src_pad;

  /* GstStream (if present) */
  GstStream *stream;

  /* Decodebin3 pad to which src_pad is linked to */
  GstPad *db3_sink_pad;

  /* TRUE if db3_sink_pad is a request pad */
  gboolean db3_pad_is_request;

  /* TRUE if EOS went through the source pad. Marked as TRUE if decodebin3
   * notified `about-to-finish` for pull mode */
  gboolean saw_eos;

  /* Downstream blocking probe id. Only set/valid if we need to block this
   * pad */
  gulong block_probe_id;

  /* Downstream event probe id */
  gulong event_probe_id;
};

/* Controls an output source pad */
struct _OutputPad
{
  GstURIDecodeBin3 *uridecodebin;

  GstPad *target_pad;
  GstPad *ghost_pad;

  /* Downstream event probe id */
  gulong probe_id;

  /* The last seen (i.e. current) group_id
   * Can be (guint)-1 if no group_id was seen yet */
  guint current_group_id;
};

#define PLAY_ITEMS_GET_LOCK(d) (&(GST_URI_DECODE_BIN3_CAST(d)->play_items_lock))
#define PLAY_ITEMS_LOCK(d) G_STMT_START { \
    GST_TRACE("Locking play_items from thread %p", g_thread_self()); \
    g_mutex_lock (PLAY_ITEMS_GET_LOCK (d)); \
    GST_TRACE("Locked play_items from thread %p", g_thread_self()); \
 } G_STMT_END

#define PLAY_ITEMS_UNLOCK(d) G_STMT_START { \
    GST_TRACE("Unlocking play_items from thread %p", g_thread_self()); \
    g_mutex_unlock (PLAY_ITEMS_GET_LOCK (d)); \
 } G_STMT_END

/**
 * GstURIDecodeBin3
 *
 * uridecodebin3 element struct
 */
struct _GstURIDecodeBin3
{
  GstBin parent_instance;

  /* Properties */
  GstElement *source;
  guint64 connection_speed;     /* In bits/sec (0 = unknown) */
  GstCaps *caps;
  guint64 buffer_duration;      /* When buffering, buffer duration (ns) */
  guint buffer_size;            /* When buffering, buffer size (bytes) */
  gboolean download;
  gchar *download_dir;
  gboolean use_buffering;
  guint64 ring_buffer_max_size;
  gboolean instant_uri;         /* Whether URI changes should be applied immediately or not */

  /* Mutex to protect play_items/input_item/output_item */
  GMutex play_items_lock;

  /* Notify that the input_item sources have all drained */
  GCond input_source_drained;

  /* List of GstPlayItem ordered by time of creation (first is oldest, new ones
   * are appended) */
  GList *play_items;

  /* Play item currently feeding decodebin3. */
  GstPlayItem *input_item;

  /* Play item currently outputted by decodebin3 */
  GstPlayItem *output_item;

  /* A global decodebin3 that's used to actually do decoding */
  GstElement *decodebin;

  /* db3 signals */
  gulong db_pad_added_id;
  gulong db_pad_removed_id;
  gulong db_select_stream_id;
  gulong db_about_to_finish_id;

  /* 1 if shutting down */
  gint shutdown;

  GList *output_pads;           /* List of OutputPad */
};

static GstStateChangeReturn activate_play_item (GstPlayItem * item);

static gint
gst_uridecodebin3_select_stream (GstURIDecodeBin3 * dbin,
    GstStreamCollection * collection, GstStream * stream)
{
  GST_LOG_OBJECT (dbin, "default select-stream, returning -1");

  return -1;
}

struct _GstURIDecodeBin3Class
{
  GstBinClass parent_class;

    gint (*select_stream) (GstURIDecodeBin3 * dbin,
      GstStreamCollection * collection, GstStream * stream);
};

GST_DEBUG_CATEGORY_STATIC (gst_uri_decode_bin3_debug);
#define GST_CAT_DEFAULT gst_uri_decode_bin3_debug

/* signals */
enum
{
  SIGNAL_SELECT_STREAM,
  SIGNAL_SOURCE_SETUP,
  SIGNAL_ABOUT_TO_FINISH,
  LAST_SIGNAL
};

/* properties */
#define DEFAULT_PROP_URI            NULL
#define DEFAULT_PROP_SUBURI            NULL
#define DEFAULT_CONNECTION_SPEED    0
#define DEFAULT_CAPS                (gst_static_caps_get (&default_raw_caps))
#define DEFAULT_BUFFER_DURATION     -1
#define DEFAULT_BUFFER_SIZE         -1
#define DEFAULT_DOWNLOAD            FALSE
#define DEFAULT_USE_BUFFERING       FALSE
#define DEFAULT_RING_BUFFER_MAX_SIZE 0
#define DEFAULT_INSTANT_URI         FALSE

enum
{
  PROP_0,
  PROP_URI,
  PROP_CURRENT_URI,
  PROP_SUBURI,
  PROP_CURRENT_SUBURI,
  PROP_SOURCE,
  PROP_CONNECTION_SPEED,
  PROP_BUFFER_SIZE,
  PROP_BUFFER_DURATION,
  PROP_DOWNLOAD,
  PROP_DOWNLOAD_DIR,
  PROP_USE_BUFFERING,
  PROP_RING_BUFFER_MAX_SIZE,
  PROP_CAPS,
  PROP_INSTANT_URI
};

static guint gst_uri_decode_bin3_signals[LAST_SIGNAL] = { 0 };

static GstStaticCaps default_raw_caps = GST_STATIC_CAPS (DEFAULT_RAW_CAPS);

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

GType gst_uri_decode_bin3_get_type (void);
#define gst_uri_decode_bin3_parent_class parent_class
G_DEFINE_TYPE (GstURIDecodeBin3, gst_uri_decode_bin3, GST_TYPE_BIN);

#define _do_init \
    GST_DEBUG_CATEGORY_INIT (gst_uri_decode_bin3_debug, "uridecodebin3", 0, "URI decoder element 3"); \
    playback_element_init (plugin);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (uridecodebin3, "uridecodebin3",
    GST_RANK_NONE, GST_TYPE_URI_DECODE_BIN3, _do_init);

#define REMOVE_SIGNAL(obj,id)            \
if (id) {                                \
  g_signal_handler_disconnect (obj, id); \
  id = 0;                                \
}

static void gst_uri_decode_bin3_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_uri_decode_bin3_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_uri_decode_bin3_dispose (GObject * obj);
static GstSourceHandler *new_source_handler (GstURIDecodeBin3 * uridecodebin,
    GstPlayItem * item, gboolean is_main);
static void free_source_handler (GstURIDecodeBin3 * uridecodebin,
    GstSourceHandler * item, gboolean lock_state);
static void free_source_item (GstURIDecodeBin3 * uridecodebin,
    GstSourceItem * item);

static GstPlayItem *new_play_item (GstURIDecodeBin3 * dec);
static void free_play_item (GstURIDecodeBin3 * dec, GstPlayItem * item);
static gboolean play_item_is_eos (GstPlayItem * item);
static void play_item_set_eos (GstPlayItem * item);
static gboolean play_item_has_all_pads (GstPlayItem * item);

static void gst_uri_decode_bin3_set_uri (GstURIDecodeBin3 * dec,
    const gchar * uri);
static void gst_uri_decode_bin3_set_suburi (GstURIDecodeBin3 * dec,
    const gchar * uri);

static GstStateChangeReturn gst_uri_decode_bin3_change_state (GstElement *
    element, GstStateChange transition);
static gboolean gst_uri_decodebin3_send_event (GstElement * element,
    GstEvent * event);
static void gst_uri_decode_bin3_handle_message (GstBin * bin, GstMessage * msg);

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
gst_uri_decode_bin3_class_init (GstURIDecodeBin3Class * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBinClass *gstbin_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  gstbin_class = GST_BIN_CLASS (klass);

  gobject_class->set_property = gst_uri_decode_bin3_set_property;
  gobject_class->get_property = gst_uri_decode_bin3_get_property;
  gobject_class->dispose = gst_uri_decode_bin3_dispose;

  g_object_class_install_property (gobject_class, PROP_URI,
      g_param_spec_string ("uri", "URI", "URI to decode",
          DEFAULT_PROP_URI, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CURRENT_URI,
      g_param_spec_string ("current-uri", "Current URI",
          "The currently playing URI", NULL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SUBURI,
      g_param_spec_string ("suburi", ".sub-URI", "Optional URI of a subtitle",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CURRENT_SUBURI,
      g_param_spec_string ("current-suburi", "Current .sub-URI",
          "The currently playing URI of a subtitle",
          NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SOURCE,
      g_param_spec_object ("source", "Source", "Source object used",
          GST_TYPE_ELEMENT, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CONNECTION_SPEED,
      g_param_spec_uint64 ("connection-speed", "Connection Speed",
          "Network connection speed in kbps (0 = unknown)",
          0, G_MAXUINT64 / 1000, DEFAULT_CONNECTION_SPEED,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BUFFER_SIZE,
      g_param_spec_int ("buffer-size", "Buffer size (bytes)",
          "Buffer size when buffering streams (-1 default value)",
          -1, G_MAXINT, DEFAULT_BUFFER_SIZE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BUFFER_DURATION,
      g_param_spec_int64 ("buffer-duration", "Buffer duration (ns)",
          "Buffer duration when buffering streams (-1 default value)",
          -1, G_MAXINT64, DEFAULT_BUFFER_DURATION,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstURIDecodeBin3::download:
   *
   * For certain media type, enable download buffering.
   */
  g_object_class_install_property (gobject_class, PROP_DOWNLOAD,
      g_param_spec_boolean ("download", "Download",
          "Attempt download buffering when buffering network streams",
          DEFAULT_DOWNLOAD, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstURIDecodeBin3:download-dir:
   *
   * The directory where buffers are downloaded to, if 'download' is enabled.
   * If not set (default), the XDG cache directory is used.
   * Will be applied to the next 'uri' played or until the element go back to
   * the PAUSED state.
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_DOWNLOAD_DIR,
      g_param_spec_string ("download-dir", "Download Directory",
          "The directory where buffers are downloaded to, if 'download' is enabled. "
          "If not set (default), the XDG cache directory is used.",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstURIDecodeBin3::use-buffering:
   *
   * Emit BUFFERING messages based on low-/high-percent thresholds of the
   * demuxed or parsed data.
   * When download buffering is activated and used for the current media
   * type, this property does nothing. Otherwise perform buffering on the
   * demuxed or parsed media.
   */
  g_object_class_install_property (gobject_class, PROP_USE_BUFFERING,
      g_param_spec_boolean ("use-buffering", "Use Buffering",
          "Perform buffering on demuxed/parsed media",
          DEFAULT_USE_BUFFERING, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstURIDecodeBin3::ring-buffer-max-size
   *
   * The maximum size of the ring buffer in kilobytes. If set to 0, the ring
   * buffer is disabled. Default is 0.
   */
  g_object_class_install_property (gobject_class, PROP_RING_BUFFER_MAX_SIZE,
      g_param_spec_uint64 ("ring-buffer-max-size",
          "Max. ring buffer size (bytes)",
          "Max. amount of data in the ring buffer (bytes, 0 = ring buffer disabled)",
          0, G_MAXUINT, DEFAULT_RING_BUFFER_MAX_SIZE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CAPS,
      g_param_spec_boxed ("caps", "Caps",
          "The caps on which to stop decoding. (NULL = default)",
          GST_TYPE_CAPS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstURIDecodeBin3:instant-uri:
   *
   * Changes to uri are applied immediately (instead of on EOS or when the
   * element is set back to PLAYING).
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_INSTANT_URI,
      g_param_spec_boolean ("instant-uri", "Instantaneous URI change",
          "When enabled, URI changes are applied immediately",
          DEFAULT_INSTANT_URI, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstURIDecodebin3::select-stream
   * @decodebin: a #GstURIDecodebin3
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
  gst_uri_decode_bin3_signals[SIGNAL_SELECT_STREAM] =
      g_signal_new ("select-stream", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstURIDecodeBin3Class, select_stream),
      _gst_int_accumulator, NULL, NULL, G_TYPE_INT, 2,
      GST_TYPE_STREAM_COLLECTION, GST_TYPE_STREAM);

  /**
   * GstURIDecodeBin3::source-setup:
   * @bin: the uridecodebin.
   * @source: source element
   *
   * This signal is emitted after a source element has been created, so
   * it can be configured by setting additional properties (e.g. set a
   * proxy server for an http source, or set the device and read speed for
   * an audio cd source).
   */
  gst_uri_decode_bin3_signals[SIGNAL_SOURCE_SETUP] =
      g_signal_new ("source-setup", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1, GST_TYPE_ELEMENT);
  /**
   * GstURIDecodeBin3::about-to-finish:
   *
   * This signal is emitted when the data for the selected URI is
   * entirely buffered and it is safe to specify another URI.
   */
  gst_uri_decode_bin3_signals[SIGNAL_ABOUT_TO_FINISH] =
      g_signal_new ("about-to-finish", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0, G_TYPE_NONE);


  gst_element_class_add_static_pad_template (gstelement_class,
      &video_src_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &audio_src_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &text_src_template);
  gst_element_class_add_static_pad_template (gstelement_class, &src_template);
  gst_element_class_set_static_metadata (gstelement_class,
      "URI Decoder", "Generic/Bin/Decoder",
      "Autoplug and decode an URI to raw media",
      "Edward Hervey <edward@centricular.com>, Jan Schmidt <jan@centricular.com>");

  gstelement_class->change_state = gst_uri_decode_bin3_change_state;
  gstelement_class->send_event =
      GST_DEBUG_FUNCPTR (gst_uri_decodebin3_send_event);

  gstbin_class->handle_message = gst_uri_decode_bin3_handle_message;

  klass->select_stream = gst_uridecodebin3_select_stream;
}

static void
current_updated (GstURIDecodeBin3 * dec)
{
  GObject *object = G_OBJECT (dec);

  g_object_notify (object, "current-uri");
  g_object_notify (object, "current-suburi");
}

static void
check_output_group_id (GstURIDecodeBin3 * dec)
{
  GList *iter;
  guint common_group_id = GST_GROUP_ID_INVALID;
  gboolean notify_current = FALSE;

  PLAY_ITEMS_LOCK (dec);

  for (iter = dec->output_pads; iter; iter = iter->next) {
    OutputPad *pad = iter->data;

    if (common_group_id == GST_GROUP_ID_INVALID)
      common_group_id = pad->current_group_id;
    else if (common_group_id != pad->current_group_id) {
      GST_DEBUG_OBJECT (dec, "transitioning output play item");
      PLAY_ITEMS_UNLOCK (dec);
      return;
    }
  }

  if (dec->output_item->group_id == common_group_id) {
    GST_DEBUG_OBJECT (dec, "Output play item %d fully active", common_group_id);
  } else if (dec->output_item->group_id == GST_GROUP_ID_INVALID) {
    /* This can happen for pull-based situations */
    GST_DEBUG_OBJECT (dec, "Assigning group id %u to current output play item",
        common_group_id);
    dec->output_item->group_id = common_group_id;
  } else if (common_group_id != GST_GROUP_ID_INVALID &&
      dec->output_item->group_id != common_group_id) {
    GstPlayItem *previous_item = dec->output_item;
    GST_DEBUG_OBJECT (dec, "Output play item %d fully active", common_group_id);
    if (g_list_length (dec->play_items) > 1) {
      dec->play_items = g_list_remove (dec->play_items, previous_item);
      dec->output_item = dec->play_items->data;
      dec->output_item->group_id = common_group_id;
      free_play_item (dec, previous_item);
    }
    notify_current = TRUE;
  }

  PLAY_ITEMS_UNLOCK (dec);

  if (notify_current) {
    /* don't hold the object lock as application could fetch some properties whose getters require this lock as well */
    current_updated (dec);
  }
}

static GstPadProbeReturn
db_src_probe (GstPad * pad, GstPadProbeInfo * info, OutputPad * output)
{
  GstEvent *event = GST_PAD_PROBE_INFO_EVENT (info);
  GstURIDecodeBin3 *uridecodebin = output->uridecodebin;

  GST_DEBUG_OBJECT (pad, "event %" GST_PTR_FORMAT, event);

  /* EOS : Mark pad as EOS */

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_EOS:
    {
      /* If there is a next input, drop the EOS event */
      if (uridecodebin->input_item != uridecodebin->output_item ||
          uridecodebin->input_item !=
          g_list_last (uridecodebin->play_items)->data) {
        GST_DEBUG_OBJECT (uridecodebin,
            "Dropping EOS event because in gapless mode");
        return GST_PAD_PROBE_DROP;
      }
      break;
    }
    case GST_EVENT_STREAM_START:
    {
      /* STREAM_START : Store group_id and check if currently active
       *  PlayEntry changed */
      if (gst_event_parse_group_id (event, &output->current_group_id)) {
        GST_DEBUG_OBJECT (pad, "current group id %" G_GUINT32_FORMAT,
            output->current_group_id);
        /* Check if we switched over to a new output */
        check_output_group_id (uridecodebin);
      }

      break;
    }
    default:
      break;
  }


  return GST_PAD_PROBE_OK;
}

static OutputPad *
add_output_pad (GstURIDecodeBin3 * dec, GstPad * target_pad)
{
  OutputPad *output;
  gchar *pad_name;
  GstEvent *stream_start;

  output = g_new0 (OutputPad, 1);

  GST_LOG_OBJECT (dec, "Created output %p", output);

  output->uridecodebin = dec;
  output->target_pad = target_pad;
  output->current_group_id = GST_GROUP_ID_INVALID;
  pad_name = gst_pad_get_name (target_pad);
  output->ghost_pad = gst_ghost_pad_new (pad_name, target_pad);
  g_free (pad_name);

  gst_pad_set_active (output->ghost_pad, TRUE);

  stream_start = gst_pad_get_sticky_event (target_pad,
      GST_EVENT_STREAM_START, 0);
  if (stream_start) {
    gst_pad_store_sticky_event (output->ghost_pad, stream_start);
    gst_event_unref (stream_start);
  } else {
    GST_WARNING_OBJECT (target_pad,
        "Exposing pad without stored stream-start event");
  }

  gst_element_add_pad (GST_ELEMENT (dec), output->ghost_pad);

  output->probe_id =
      gst_pad_add_probe (output->target_pad,
      GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, (GstPadProbeCallback) db_src_probe,
      output, NULL);

  /* FIXME: LOCK TO PROTECT PAD LIST */
  dec->output_pads = g_list_append (dec->output_pads, output);

  return output;
}

static void
db_pad_added_cb (GstElement * element, GstPad * pad, GstURIDecodeBin3 * dec)
{
  GST_DEBUG_OBJECT (dec, "Wrapping new pad %s:%s", GST_DEBUG_PAD_NAME (pad));

  if (GST_PAD_IS_SRC (pad))
    add_output_pad (dec, pad);
}

static void
db_pad_removed_cb (GstElement * element, GstPad * pad, GstURIDecodeBin3 * dec)
{
  GList *tmp;
  OutputPad *output = NULL;

  if (!GST_PAD_IS_SRC (pad))
    return;

  GST_DEBUG_OBJECT (dec, "pad %s:%s", GST_DEBUG_PAD_NAME (pad));
  /* FIXME: LOCK for list access */

  for (tmp = dec->output_pads; tmp; tmp = tmp->next) {
    OutputPad *cand = (OutputPad *) tmp->data;

    if (cand->target_pad == pad) {
      output = cand;
      dec->output_pads = g_list_delete_link (dec->output_pads, tmp);
      break;
    }
  }

  if (output) {
    GST_LOG_OBJECT (element, "Removing output %p", output);
    /* Remove source ghost pad */
    gst_ghost_pad_set_target ((GstGhostPad *) output->ghost_pad, NULL);
    gst_element_remove_pad ((GstElement *) dec, output->ghost_pad);

    /* Remove event probe */
    gst_pad_remove_probe (output->target_pad, output->probe_id);

    g_free (output);

    check_output_group_id (dec);
  }
}

static gint
db_select_stream_cb (GstElement * decodebin,
    GstStreamCollection * collection, GstStream * stream,
    GstURIDecodeBin3 * uridecodebin)
{
  gint response = -1;

  g_signal_emit (uridecodebin,
      gst_uri_decode_bin3_signals[SIGNAL_SELECT_STREAM], 0, collection, stream,
      &response);
  return response;
}

static gboolean
check_pad_mode (GstElement * src, GstPad * pad, gpointer udata)
{
  GstPadMode curmode = GST_PAD_MODE (pad);
  GstPadMode *retmode = (GstPadMode *) udata;

  /* We don't care if pads aren't activated */
  if (curmode == GST_PAD_MODE_NONE)
    return TRUE;

  if (*retmode == GST_PAD_MODE_NONE) {
    *retmode = curmode;
  } else if (*retmode != curmode) {
    GST_ERROR_OBJECT (src, "source has different scheduling mode ?");
  }

  return TRUE;
}

static gboolean
play_item_is_pull_based (GstPlayItem * item)
{
  GstElement *src;
  GstPadMode mode = GST_PAD_MODE_NONE;

  g_assert (item->main_item && item->main_item->handler
      && item->main_item->handler->urisourcebin);

  src = item->main_item->handler->urisourcebin;
  gst_element_foreach_src_pad (src, check_pad_mode, &mode);

  return (mode == GST_PAD_MODE_PULL);
}

static void
emit_and_handle_about_to_finish (GstURIDecodeBin3 * uridecodebin,
    GstPlayItem * item)
{
  GST_DEBUG_OBJECT (uridecodebin, "output %d , posted_about_to_finish:%d",
      item->group_id, item->posted_about_to_finish);

  if (item->posted_about_to_finish) {
    GST_DEBUG_OBJECT (uridecodebin,
        "already handling about-to-finish for this play item");
    return;
  }

  if (item != uridecodebin->input_item) {
    GST_DEBUG_OBJECT (uridecodebin, "Postponing about-to-finish propagation");
    item->pending_about_to_finish = TRUE;
    return;
  }

  /* If the input entry is pull-based, mark all the source pads as EOS */
  if (play_item_is_pull_based (item)) {
    GST_DEBUG_OBJECT (uridecodebin, "Marking play item as EOS");
    play_item_set_eos (item);
  }

  item->posted_about_to_finish = TRUE;
  GST_DEBUG_OBJECT (uridecodebin, "Posting about-to-finish");
  g_signal_emit (uridecodebin,
      gst_uri_decode_bin3_signals[SIGNAL_ABOUT_TO_FINISH], 0, NULL);

  /* Note : Activation of the (potential) next entry is handled in
   * gst_uri_decode_bin3_set_uri */
}

static void
db_about_to_finish_cb (GstElement * decodebin, GstURIDecodeBin3 * uridecodebin)
{
  GST_LOG_OBJECT (uridecodebin, "about to finish from %s",
      GST_OBJECT_NAME (decodebin));

  emit_and_handle_about_to_finish (uridecodebin, uridecodebin->output_item);
}

static void
gst_uri_decode_bin3_init (GstURIDecodeBin3 * dec)
{
  GstPlayItem *item;

  dec->connection_speed = DEFAULT_CONNECTION_SPEED;
  dec->caps = DEFAULT_CAPS;
  dec->buffer_duration = DEFAULT_BUFFER_DURATION;
  dec->buffer_size = DEFAULT_BUFFER_SIZE;
  dec->download = DEFAULT_DOWNLOAD;
  dec->use_buffering = DEFAULT_USE_BUFFERING;
  dec->ring_buffer_max_size = DEFAULT_RING_BUFFER_MAX_SIZE;

  g_mutex_init (&dec->play_items_lock);
  g_cond_init (&dec->input_source_drained);

  dec->decodebin = gst_element_factory_make ("decodebin3", NULL);
  gst_bin_add (GST_BIN_CAST (dec), dec->decodebin);
  dec->db_pad_added_id =
      g_signal_connect (dec->decodebin, "pad-added",
      G_CALLBACK (db_pad_added_cb), dec);
  dec->db_pad_removed_id =
      g_signal_connect (dec->decodebin, "pad-removed",
      G_CALLBACK (db_pad_removed_cb), dec);
  dec->db_select_stream_id =
      g_signal_connect (dec->decodebin, "select-stream",
      G_CALLBACK (db_select_stream_cb), dec);
  dec->db_about_to_finish_id =
      g_signal_connect (dec->decodebin, "about-to-finish",
      G_CALLBACK (db_about_to_finish_cb), dec);

  GST_OBJECT_FLAG_SET (dec, GST_ELEMENT_FLAG_SOURCE);
  gst_bin_set_suppressed_flags (GST_BIN (dec),
      GST_ELEMENT_FLAG_SOURCE | GST_ELEMENT_FLAG_SINK);

  item = new_play_item (dec);
  dec->play_items = g_list_append (dec->play_items, item);
  /* The initial play item is automatically the input and output one */
  dec->input_item = dec->output_item = item;
}

static void
gst_uri_decode_bin3_dispose (GObject * obj)
{
  GstURIDecodeBin3 *dec = GST_URI_DECODE_BIN3 (obj);
  GList *iter;

  GST_DEBUG_OBJECT (obj, "Disposing");

  /* Free all play items */
  for (iter = dec->play_items; iter; iter = iter->next) {
    GstPlayItem *item = iter->data;
    free_play_item (dec, item);
  }
  g_list_free (dec->play_items);
  dec->play_items = NULL;
  g_clear_pointer (&dec->download_dir, g_free);

  g_mutex_clear (&dec->play_items_lock);

  gst_clear_caps (&dec->caps);

  G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static GstStateChangeReturn
activate_source_item (GstSourceItem * item)
{
  GstSourceHandler *handler = item->handler;

  if (handler == NULL) {
    GST_WARNING ("Can't activate item without a handler");
    return GST_STATE_CHANGE_FAILURE;
  }

  g_object_set (handler->urisourcebin, "uri", item->uri, NULL);
  if (!handler->active) {
    gst_bin_add ((GstBin *) handler->uridecodebin, handler->urisourcebin);
    handler->active = TRUE;
  }

  if (!gst_element_sync_state_with_parent (handler->urisourcebin))
    return GST_STATE_CHANGE_FAILURE;

  return GST_STATE_CHANGE_SUCCESS;
}

static void
link_src_pad_to_db3 (GstURIDecodeBin3 * uridecodebin, GstSourcePad * spad)
{
  GstSourceHandler *handler = spad->handler;
  GstPad *sinkpad = NULL;

  /* Try to link to main sink pad only if it's from a main handler */
  if (handler->is_main_source) {
    sinkpad = gst_element_get_static_pad (uridecodebin->decodebin, "sink");
    if (gst_pad_is_linked (sinkpad)) {
      gst_object_unref (sinkpad);
      sinkpad = NULL;
    }
  }

  if (sinkpad == NULL) {
    sinkpad =
        gst_element_request_pad_simple (uridecodebin->decodebin, "sink_%u");
    spad->db3_pad_is_request = TRUE;
  }

  if (sinkpad) {
    GstPadLinkReturn res;
    GST_DEBUG_OBJECT (uridecodebin,
        "Linking %" GST_PTR_FORMAT " to %" GST_PTR_FORMAT, spad->src_pad,
        sinkpad);
    res = gst_pad_link (spad->src_pad, sinkpad);
    gst_object_unref (sinkpad);
    if (GST_PAD_LINK_FAILED (res)) {
      GST_ERROR_OBJECT (uridecodebin,
          "failed to link pad %s:%s to decodebin, reason %s (%d)",
          GST_DEBUG_PAD_NAME (spad->src_pad), gst_pad_link_get_name (res), res);
      return;
    }
  } else {
    GST_ERROR_OBJECT (uridecodebin, "Could not get a sinkpad from decodebin3");
    return;
  }

  spad->db3_sink_pad = sinkpad;

  /* Activate sub_item after the main source activation was finished */
  if (handler->is_main_source && handler->play_item->sub_item
      && !handler->play_item->sub_item->handler) {
    GstStateChangeReturn ret;

    /* The state lock is taken to ensure we can atomically change the
     * urisourcebin back to NULL in case of failures */
    GST_STATE_LOCK (uridecodebin);
    handler->play_item->sub_item->handler =
        new_source_handler (uridecodebin, handler->play_item, FALSE);
    ret = activate_source_item (handler->play_item->sub_item);
    if (ret == GST_STATE_CHANGE_FAILURE) {
      free_source_handler (uridecodebin, handler->play_item->sub_item->handler,
          FALSE);
      handler->play_item->sub_item->handler = NULL;
      GST_STATE_UNLOCK (uridecodebin);
      goto sub_item_activation_failed;
    }
    GST_STATE_UNLOCK (uridecodebin);
  }

  return;

sub_item_activation_failed:
  {
    GST_ERROR_OBJECT (uridecodebin,
        "failed to activate subtitle playback item");
    return;
  }
}

static GList *
get_all_play_item_source_pads (GstPlayItem * item)
{
  GList *ret = NULL;

  if (item->main_item && item->main_item->handler) {
    ret = g_list_copy (item->main_item->handler->sourcepads);
  }

  if (item->sub_item && item->sub_item->handler) {
    ret =
        g_list_concat (ret, g_list_copy (item->sub_item->handler->sourcepads));
  }

  return ret;
}

static GstSourcePad *
find_matching_source_pad (GList * candidates, GstSourcePad * target)
{
  GList *iter;
  GstStream *stream = target->stream;

  GST_DEBUG_OBJECT (target->src_pad, "Find match for stream %" GST_PTR_FORMAT,
      stream);

  for (iter = candidates; iter; iter = iter->next) {
    GstSourcePad *cand = iter->data;

    if (!cand->db3_sink_pad)
      continue;

    /* Target doesn't have a specific GstStream, return the first result */
    if (!stream)
      return cand;

    if (gst_stream_get_stream_type (cand->stream) ==
        gst_stream_get_stream_type (stream))
      return cand;
  }

  return NULL;
}

/* PLAY_ITEMS_LOCK held
 *
 * Switch the input play item to the next one
 */
static void
switch_and_activate_input_locked (GstURIDecodeBin3 * uridecodebin,
    GstPlayItem * new_item)
{
  GList *new_pads = get_all_play_item_source_pads (new_item);
  GList *old_pads = get_all_play_item_source_pads (uridecodebin->input_item);
  GList *to_activate = NULL;
  GList *iternew, *iterold;
  gboolean inactive_previous_item = old_pads == NULL;
  GstMessage *pending_buffering_msg = NULL;
  gboolean pending_about_to_finish = FALSE;

  /* Deactivate old urisourcebins first ? Problem is they might remove the pads */

  /* Go over new item source pads and figure out a candidate replacement in  */
  /* Figure out source pad matches */
  for (iternew = new_pads; iternew; iternew = iternew->next) {
    GstSourcePad *new_spad = iternew->data;
    GstSourcePad *old_spad = find_matching_source_pad (old_pads, new_spad);

    if (old_spad) {
      GST_DEBUG_OBJECT (uridecodebin, "Relinking %s:%s from %s:%s to %s:%s",
          GST_DEBUG_PAD_NAME (old_spad->db3_sink_pad),
          GST_DEBUG_PAD_NAME (old_spad->src_pad),
          GST_DEBUG_PAD_NAME (new_spad->src_pad));
      gst_pad_unlink (old_spad->src_pad, old_spad->db3_sink_pad);
      new_spad->db3_sink_pad = old_spad->db3_sink_pad;
      new_spad->db3_pad_is_request = old_spad->db3_pad_is_request;
      old_spad->db3_sink_pad = NULL;

      /* NOTE : Pad will be linked further down */
      old_pads = g_list_remove (old_pads, old_spad);
    } else {
      GST_DEBUG_OBJECT (new_spad->src_pad, "Needs a new pad");
      to_activate = g_list_append (to_activate, new_spad);
    }
  }

  /* If the old pads contains the static decodebin3 sinkpad *and* we have a new
   *  pad to activate, we re-use it */
  if (to_activate) {
    /* Remove unmatched old source pads */
    for (iterold = old_pads; iterold; iterold = iterold->next) {
      GstSourcePad *old_spad = iterold->data;
      if (old_spad->db3_sink_pad && !old_spad->db3_pad_is_request) {
        GstSourcePad *new_spad = to_activate->data;

        GST_DEBUG_OBJECT (uridecodebin, "Static sinkpad can be re-used");
        GST_DEBUG_OBJECT (uridecodebin, "Relinking %s:%s from %s:%s to %s:%s",
            GST_DEBUG_PAD_NAME (old_spad->db3_sink_pad),
            GST_DEBUG_PAD_NAME (old_spad->src_pad),
            GST_DEBUG_PAD_NAME (new_spad->src_pad));
        gst_pad_unlink (old_spad->src_pad, old_spad->db3_sink_pad);
        new_spad->db3_sink_pad = old_spad->db3_sink_pad;
        new_spad->db3_pad_is_request = old_spad->db3_pad_is_request;
        old_spad->db3_sink_pad = NULL;

        /* NOTE : Pad will be linked further down */
        old_pads = g_list_remove (old_pads, old_spad);
        to_activate = g_list_remove (to_activate, new_spad);
        break;
      }
    }
  }

  /* Remove unmatched old source pads */
  for (iterold = old_pads; iterold; iterold = iterold->next) {
    GstSourcePad *old_spad = iterold->data;
    if (old_spad->db3_sink_pad && old_spad->db3_pad_is_request) {
      GST_DEBUG_OBJECT (uridecodebin, "Releasing no longer used db3 pad");
      gst_element_release_request_pad (uridecodebin->decodebin,
          old_spad->db3_sink_pad);
      old_spad->db3_sink_pad = NULL;
    }
  }

  /* Link existing pads */
  for (iternew = new_pads; iternew; iternew = iternew->next) {
    GstSourcePad *new_spad = iternew->data;
    if (new_spad->db3_sink_pad)
      gst_pad_link (new_spad->src_pad, new_spad->db3_sink_pad);
  }

  /* Request (and link) new pads */
  for (iternew = to_activate; iternew; iternew = iternew->next) {
    GstSourcePad *new_spad = iternew->data;
    link_src_pad_to_db3 (uridecodebin, new_spad);
  }

  /* Unblock all new item source pads */
  for (iternew = new_pads; iternew; iternew = iternew->next) {
    GstSourcePad *new_spad = iternew->data;
    if (new_spad->block_probe_id) {
      gst_pad_remove_probe (new_spad->src_pad, new_spad->block_probe_id);
      new_spad->block_probe_id = 0;
    }
  }
  g_list_free (new_pads);
  g_list_free (old_pads);

  /* Deactivate old input item (by removing the source components). The final
   * removal of this play item will be done once decodebin3 starts output the
   * content of the new play item. */
  if (uridecodebin->input_item->main_item) {
    free_source_item (uridecodebin, uridecodebin->input_item->main_item);
    uridecodebin->input_item->main_item = NULL;
  }
  if (uridecodebin->input_item->sub_item) {
    free_source_item (uridecodebin, uridecodebin->input_item->sub_item);
    uridecodebin->input_item->sub_item = NULL;
  }

  /* If the previous play item was not active at all (i.e. was never linked to
   * decodebin3), this one *also* becomes the output one */
  if (inactive_previous_item) {
    GST_DEBUG_OBJECT (uridecodebin,
        "Previous play item was never activated, discarding");
    uridecodebin->play_items =
        g_list_remove (uridecodebin->play_items, uridecodebin->input_item);
    free_play_item (uridecodebin, uridecodebin->input_item);
    uridecodebin->output_item = new_item;
  }

  /* and set new one as input item */
  uridecodebin->input_item = new_item;

  pending_about_to_finish = new_item->pending_about_to_finish;
  if (new_item->main_item->handler->pending_buffering_msg) {
    pending_buffering_msg = new_item->main_item->handler->pending_buffering_msg;
    new_item->main_item->handler->pending_buffering_msg = NULL;
  }

  /* If we have to post message or emit signals, it might trigger some
   * re-entring actions (like setting the next URI). Make sure we release the
   * lock when posting/emitting */
  if (pending_buffering_msg || pending_about_to_finish) {
    PLAY_ITEMS_UNLOCK (uridecodebin);
    /* If the new source is already drained, propagate about-to-finish */
    if (pending_about_to_finish) {
      emit_and_handle_about_to_finish (uridecodebin, new_item);
    }

    /* Finally propagate pending buffering message */
    if (pending_buffering_msg) {
      GST_DEBUG_OBJECT (uridecodebin,
          "Posting pending buffering message %" GST_PTR_FORMAT,
          pending_buffering_msg);
      GST_BIN_CLASS (parent_class)->handle_message ((GstBin *) uridecodebin,
          pending_buffering_msg);
    }
    PLAY_ITEMS_LOCK (uridecodebin);
  }
}

static GstPadProbeReturn
uri_src_probe (GstPad * pad, GstPadProbeInfo * info, GstSourcePad * srcpad)
{
  GstEvent *event = GST_PAD_PROBE_INFO_EVENT (info);
  GstSourceHandler *handler = srcpad->handler;
  GstPadProbeReturn ret = GST_PAD_PROBE_OK;

  GST_DEBUG_OBJECT (pad, "event %" GST_PTR_FORMAT, event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_EOS:
    {
      GstPad *peer;
      /* Propagate the EOS *before* triggering any potential switch */
      peer = gst_pad_get_peer (pad);
      if (peer) {
        gst_pad_send_event (peer, event);
        gst_object_unref (peer);
      } else {
        /* No peer, just drop it (since we're returning HANDLED below) */
        gst_event_unref (event);
      }

      PLAY_ITEMS_LOCK (handler->uridecodebin);
      /* EOS : Mark pad as EOS */
      srcpad->saw_eos = TRUE;
      /* Check if the input play item is fully EOS. If yes and there is a
       * pending play item, switch to it */
      if (handler->play_item == handler->uridecodebin->input_item &&
          play_item_is_eos (handler->play_item)) {
        g_cond_broadcast (&handler->uridecodebin->input_source_drained);
      }
      PLAY_ITEMS_UNLOCK (handler->uridecodebin);
      ret = GST_PAD_PROBE_HANDLED;
      break;
    }
    case GST_EVENT_STREAM_START:
    {
      GstStream *stream = NULL;
      GstQuery *q = gst_query_new_selectable ();
      guint group_id = GST_GROUP_ID_INVALID;

      srcpad->saw_eos = FALSE;
      gst_event_parse_group_id (event, &group_id);

      /* Unify group id */
      if (handler->play_item->group_id == GST_GROUP_ID_INVALID) {
        GST_DEBUG_OBJECT (pad,
            "Setting play item to group_id %" G_GUINT32_FORMAT, group_id);
        handler->play_item->group_id = group_id;
      } else if (handler->play_item->group_id != group_id) {
        GST_DEBUG_OBJECT (pad, "Updating event group-id to %" G_GUINT32_FORMAT,
            handler->play_item->group_id);
        event = gst_event_make_writable (event);
        GST_PAD_PROBE_INFO_DATA (info) = event;
        gst_event_set_group_id (event, handler->play_item->group_id);
      }

      gst_event_parse_stream (event, &stream);
      if (stream) {
        GST_DEBUG_OBJECT (srcpad->src_pad, "Got GstStream %" GST_PTR_FORMAT,
            stream);
        gst_object_replace ((GstObject **) & srcpad->stream,
            (GstObject *) stream);
        gst_object_unref (stream);
      }

      /* Remember whether upstream is selectable or not */
      if (gst_pad_query (pad, q)) {
        PLAY_ITEMS_LOCK (handler->uridecodebin);
        gst_query_parse_selectable (q, &handler->upstream_selected);
        GST_DEBUG_OBJECT (srcpad->src_pad, "Upstream is selectable : %d",
            handler->upstream_selected);
        PLAY_ITEMS_UNLOCK (handler->uridecodebin);
      }
      gst_query_unref (q);

      break;
    }
    case GST_EVENT_STREAM_COLLECTION:
    {
      GstStreamCollection *collection = NULL;
      PLAY_ITEMS_LOCK (handler->uridecodebin);
      if (!handler->upstream_selected) {
        gst_event_parse_stream_collection (event, &collection);
        if (collection) {
          GST_DEBUG_OBJECT (srcpad->src_pad, "Seen collection with %d streams",
              gst_stream_collection_get_size (collection));
          if (handler->expected_pads == 1) {
            handler->expected_pads =
                gst_stream_collection_get_size (collection);
          }
          gst_object_unref (collection);
        }
      }
      PLAY_ITEMS_UNLOCK (handler->uridecodebin);

      break;
    }
    case GST_EVENT_SEGMENT:
    {
      srcpad->saw_eos = FALSE;
      break;
    }
    default:
      break;
  }

  return ret;
}

static GstPadProbeReturn
uri_src_block_probe (GstPad * pad, GstPadProbeInfo * info,
    GstSourcePad * srcpad)
{
  GstPadProbeReturn ret = GST_PAD_PROBE_OK;
  GstSourceHandler *handler = srcpad->handler;

  /* We only block on buffers, buffer list and gap events. Everything else is
   * passed to the regular (non-blocking) probe */
  if (GST_IS_EVENT (GST_PAD_PROBE_INFO_DATA (info)) &&
      GST_EVENT_TYPE (GST_PAD_PROBE_INFO_EVENT (info)) != GST_EVENT_GAP) {
    return GST_PAD_PROBE_PASS;
  }

  GST_DEBUG_OBJECT (pad, "blocking on buffer or gap");

  PLAY_ITEMS_LOCK (handler->uridecodebin);
  if (srcpad->block_probe_id == 0) {
    GST_DEBUG_OBJECT (pad, "pad has already been unblocked");
    ret = GST_PAD_PROBE_REMOVE;
  } else if (play_item_is_eos (handler->uridecodebin->input_item)) {
    GST_DEBUG_OBJECT (handler->uridecodebin,
        "We can switch over to the next input item");
    switch_and_activate_input_locked (handler->uridecodebin,
        handler->play_item);
    ret = GST_PAD_PROBE_REMOVE;
  } else if (play_item_has_all_pads (handler->play_item)) {
    /* We have all expected pads for this play item but the current input
     * play item isn't done yet, wait for it */
    GST_DEBUG_OBJECT (pad, "Waiting for input source to be drained");
    g_cond_wait (&handler->uridecodebin->input_source_drained,
        &handler->uridecodebin->play_items_lock);
    if (g_atomic_int_get (&handler->uridecodebin->shutdown))
      goto shutdown;
    if (play_item_is_eos (handler->uridecodebin->input_item)) {
      GST_DEBUG_OBJECT (handler->uridecodebin,
          "We can switch over to the next input item");
      switch_and_activate_input_locked (handler->uridecodebin,
          handler->play_item);
    }
    ret = GST_PAD_PROBE_REMOVE;
  }

  PLAY_ITEMS_UNLOCK (handler->uridecodebin);

  return ret;

  /* ERRORS */
shutdown:
  {
    GST_LOG_OBJECT (pad, "Shutting down");
    /* We are shutting down, we both want to remove this probe and propagate a
     * GST_FLOW_FLUSHING upstream (to cause tasks to stop) */
    if (srcpad->block_probe_id)
      gst_pad_remove_probe (pad, srcpad->block_probe_id);
    srcpad->block_probe_id = 0;
    PLAY_ITEMS_UNLOCK (handler->uridecodebin);
    GST_PAD_PROBE_INFO_FLOW_RETURN (info) = GST_FLOW_FLUSHING;
    gst_mini_object_unref (GST_PAD_PROBE_INFO_DATA (info));
    return GST_PAD_PROBE_HANDLED;
  }
}

static void
src_pad_added_cb (GstElement * element, GstPad * pad,
    GstSourceHandler * handler)
{
  GstSourcePad *spad = g_new0 (GstSourcePad, 1);
  GstURIDecodeBin3 *uridecodebin;

  uridecodebin = handler->uridecodebin;

  PLAY_ITEMS_LOCK (uridecodebin);

  GST_DEBUG_OBJECT (uridecodebin,
      "New pad %" GST_PTR_FORMAT " from source %" GST_PTR_FORMAT, pad, element);

  /* Register the new pad information with the source handler */
  spad->handler = handler;
  spad->src_pad = pad;
  spad->event_probe_id =
      gst_pad_add_probe (pad,
      GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, (GstPadProbeCallback) uri_src_probe,
      spad, NULL);

  handler->sourcepads = g_list_append (handler->sourcepads, spad);

  /* Can the pad be linked straight away to db3 ?
   * This can happen if:
   * * It is the initial play item
   * * It is part of the current input item
   */
  if (handler->play_item == uridecodebin->input_item) {
    GST_DEBUG_OBJECT (uridecodebin,
        "Pad is part of current input item, linking");

    link_src_pad_to_db3 (uridecodebin, spad);
    PLAY_ITEMS_UNLOCK (uridecodebin);
    return;
  }

  /* This pad is not from the current input item. We add a blocking probe to
   * wait until we block on the new urisourcebin streaming thread and can
   * switch */
  GST_DEBUG_OBJECT (uridecodebin, "Blocking input pad");
  spad->block_probe_id =
      gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
      (GstPadProbeCallback) uri_src_block_probe, spad, NULL);
  PLAY_ITEMS_UNLOCK (uridecodebin);
}

static GstSourcePad *
handler_get_source_pad (GstSourceHandler * handler, GstPad * srcpad)
{
  GList *iter;

  for (iter = handler->sourcepads; iter; iter = iter->next) {
    GstSourcePad *spad = iter->data;
    if (spad->src_pad == srcpad)
      return spad;
  }

  return NULL;
}

static void
src_pad_removed_cb (GstElement * element, GstPad * pad,
    GstSourceHandler * handler)
{
  GstURIDecodeBin3 *uridecodebin = handler->uridecodebin;
  GstSourcePad *spad = handler_get_source_pad (handler, pad);

  if (!spad)
    return;

  GST_DEBUG_OBJECT (uridecodebin,
      "Source %" GST_PTR_FORMAT " removed pad %" GST_PTR_FORMAT " peer %"
      GST_PTR_FORMAT, element, pad, spad->db3_sink_pad);

  if (spad->db3_sink_pad && spad->db3_pad_is_request)
    gst_element_release_request_pad (uridecodebin->decodebin,
        spad->db3_sink_pad);

  if (spad->stream)
    gst_object_unref (spad->stream);

  handler->sourcepads = g_list_remove (handler->sourcepads, spad);
  g_free (spad);
}

static void
src_source_setup_cb (GstElement * element, GstElement * source,
    GstSourceHandler * handler)
{
  g_signal_emit (handler->uridecodebin,
      gst_uri_decode_bin3_signals[SIGNAL_SOURCE_SETUP], 0, source, NULL);
}

static void
src_about_to_finish_cb (GstElement * element, GstSourceHandler * handler)
{
  GST_LOG_OBJECT (handler->uridecodebin, "about to finish from %s",
      GST_OBJECT_NAME (element));

  emit_and_handle_about_to_finish (handler->uridecodebin, handler->play_item);
}

static GstSourceHandler *
new_source_handler (GstURIDecodeBin3 * uridecodebin, GstPlayItem * item,
    gboolean is_main)
{
  GstSourceHandler *handler;

  handler = g_new0 (GstSourceHandler, 1);

  handler->uridecodebin = uridecodebin;
  handler->play_item = item;
  handler->is_main_source = is_main;
  handler->urisourcebin = gst_element_factory_make ("urisourcebin", NULL);
  /* Set pending properties */
  g_object_set (handler->urisourcebin,
      "connection-speed", uridecodebin->connection_speed / 1000,
      "download", uridecodebin->download,
      "download-dir", uridecodebin->download_dir,
      "use-buffering", uridecodebin->use_buffering,
      "buffer-duration", uridecodebin->buffer_duration,
      "buffer-size", uridecodebin->buffer_size,
      "ring-buffer-max-size", uridecodebin->ring_buffer_max_size,
      "parse-streams", TRUE, NULL);

  handler->pad_added_id =
      g_signal_connect (handler->urisourcebin, "pad-added",
      (GCallback) src_pad_added_cb, handler);
  handler->pad_removed_id =
      g_signal_connect (handler->urisourcebin, "pad-removed",
      (GCallback) src_pad_removed_cb, handler);
  handler->source_setup_id =
      g_signal_connect (handler->urisourcebin, "source-setup",
      (GCallback) src_source_setup_cb, handler);
  handler->about_to_finish_id =
      g_signal_connect (handler->urisourcebin, "about-to-finish",
      (GCallback) src_about_to_finish_cb, handler);

  handler->expected_pads = 1;

  return handler;
}

static gboolean
source_handler_is_eos (GstSourceHandler * handler)
{
  GList *iter;

  for (iter = handler->sourcepads; iter; iter = iter->next) {
    GstSourcePad *spad = iter->data;

    if (!spad->saw_eos)
      return FALSE;
  }

  return TRUE;
}

static void
source_handler_set_eos (GstSourceHandler * handler)
{
  GList *iter;

  for (iter = handler->sourcepads; iter; iter = iter->next) {
    GstSourcePad *spad = iter->data;

    spad->saw_eos = TRUE;
  }
}

static void
gst_uri_decode_bin3_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstURIDecodeBin3 *dec = GST_URI_DECODE_BIN3 (object);

  switch (prop_id) {
    case PROP_URI:
      gst_uri_decode_bin3_set_uri (dec, g_value_get_string (value));
      break;
    case PROP_SUBURI:
      gst_uri_decode_bin3_set_suburi (dec, g_value_get_string (value));
      break;
    case PROP_CONNECTION_SPEED:
      GST_OBJECT_LOCK (dec);
      dec->connection_speed = g_value_get_uint64 (value) * 1000;
      GST_OBJECT_UNLOCK (dec);
      break;
    case PROP_BUFFER_SIZE:
      dec->buffer_size = g_value_get_int (value);
      break;
    case PROP_BUFFER_DURATION:
      dec->buffer_duration = g_value_get_int64 (value);
      break;
    case PROP_DOWNLOAD:
      dec->download = g_value_get_boolean (value);
      break;
    case PROP_DOWNLOAD_DIR:
      dec->download_dir = g_value_dup_string (value);
      break;
    case PROP_USE_BUFFERING:
      dec->use_buffering = g_value_get_boolean (value);
      break;
    case PROP_RING_BUFFER_MAX_SIZE:
      dec->ring_buffer_max_size = g_value_get_uint64 (value);
      break;
    case PROP_CAPS:
      GST_OBJECT_LOCK (dec);
      if (dec->caps)
        gst_caps_unref (dec->caps);
      dec->caps = g_value_dup_boxed (value);
      GST_OBJECT_UNLOCK (dec);
      break;
    case PROP_INSTANT_URI:
      GST_OBJECT_LOCK (dec);
      dec->instant_uri = g_value_get_boolean (value);
      GST_OBJECT_UNLOCK (dec);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_uri_decode_bin3_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstURIDecodeBin3 *dec = GST_URI_DECODE_BIN3 (object);

  switch (prop_id) {
    case PROP_URI:
    {
      GstPlayItem *item = dec->play_items->data;
      /* Return from the head */
      if (item->main_item)
        g_value_set_string (value, item->main_item->uri);
      else
        g_value_set_string (value, NULL);
      break;
    }
    case PROP_CURRENT_URI:
    {
      if (dec->output_item && dec->output_item->main_item) {
        g_value_set_string (value, dec->output_item->main_item->uri);
      } else {
        g_value_set_string (value, NULL);
      }
      break;
    }
    case PROP_SUBURI:
    {
      GstPlayItem *item = dec->play_items->data;
      /* Return from the head */
      if (item->sub_item)
        g_value_set_string (value, item->sub_item->uri);
      else
        g_value_set_string (value, NULL);
      break;
    }
    case PROP_CURRENT_SUBURI:
    {
      if (dec->output_item && dec->output_item->sub_item) {
        g_value_set_string (value, dec->output_item->sub_item->uri);
      } else {
        g_value_set_string (value, NULL);
      }
      break;
    }
    case PROP_SOURCE:
    {
      GST_OBJECT_LOCK (dec);
      g_value_set_object (value, dec->source);
      GST_OBJECT_UNLOCK (dec);
      break;
    }
    case PROP_CONNECTION_SPEED:
      GST_OBJECT_LOCK (dec);
      g_value_set_uint64 (value, dec->connection_speed / 1000);
      GST_OBJECT_UNLOCK (dec);
      break;
    case PROP_BUFFER_SIZE:
      GST_OBJECT_LOCK (dec);
      g_value_set_int (value, dec->buffer_size);
      GST_OBJECT_UNLOCK (dec);
      break;
    case PROP_BUFFER_DURATION:
      GST_OBJECT_LOCK (dec);
      g_value_set_int64 (value, dec->buffer_duration);
      GST_OBJECT_UNLOCK (dec);
      break;
    case PROP_DOWNLOAD:
      g_value_set_boolean (value, dec->download);
      break;
    case PROP_DOWNLOAD_DIR:
      g_value_set_string (value, dec->download_dir);
      break;
    case PROP_USE_BUFFERING:
      g_value_set_boolean (value, dec->use_buffering);
      break;
    case PROP_RING_BUFFER_MAX_SIZE:
      g_value_set_uint64 (value, dec->ring_buffer_max_size);
      break;
    case PROP_CAPS:
      GST_OBJECT_LOCK (dec);
      g_value_set_boxed (value, dec->caps);
      GST_OBJECT_UNLOCK (dec);
      break;
    case PROP_INSTANT_URI:
      GST_OBJECT_LOCK (dec);
      g_value_set_boolean (value, dec->instant_uri);
      GST_OBJECT_UNLOCK (dec);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* lock_state: TRUE if the STATE LOCK should be taken. Set to FALSE if the
 * caller already has taken it */
static void
free_source_handler (GstURIDecodeBin3 * uridecodebin,
    GstSourceHandler * handler, gboolean lock_state)
{
  GST_LOG_OBJECT (uridecodebin, "source handler %p", handler);
  if (handler->active) {
    GList *iter;
    if (lock_state)
      GST_STATE_LOCK (uridecodebin);
    GST_LOG_OBJECT (uridecodebin, "Removing %" GST_PTR_FORMAT,
        handler->urisourcebin);
    for (iter = handler->sourcepads; iter; iter = iter->next) {
      GstSourcePad *spad = iter->data;
      if (spad->block_probe_id)
        gst_pad_remove_probe (spad->src_pad, spad->block_probe_id);
    }
    gst_element_set_state (handler->urisourcebin, GST_STATE_NULL);
    gst_bin_remove ((GstBin *) uridecodebin, handler->urisourcebin);
    if (lock_state)
      GST_STATE_UNLOCK (uridecodebin);
    g_list_free (handler->sourcepads);
  }
  if (handler->pending_buffering_msg)
    gst_message_unref (handler->pending_buffering_msg);
  g_free (handler);
}

static GstSourceItem *
new_source_item (GstURIDecodeBin3 * dec, GstPlayItem * item, gchar * uri)
{
  GstSourceItem *sourceitem = g_new0 (GstSourceItem, 1);

  sourceitem->play_item = item;
  sourceitem->uri = uri;

  return sourceitem;
}

static void
free_source_item (GstURIDecodeBin3 * uridecodebin, GstSourceItem * item)
{
  GST_LOG_OBJECT (uridecodebin, "source item %p", item);
  if (item->handler)
    free_source_handler (uridecodebin, item->handler, TRUE);
  g_free (item->uri);
  g_free (item);
}

static void
source_item_set_uri (GstSourceItem * item, const gchar * uri)
{
  if (item->uri)
    g_free (item->uri);
  item->uri = g_strdup (uri);
  if (item->handler) {
    g_object_set (item->handler->urisourcebin, "uri", uri, NULL);
  }
}

static GstPlayItem *
new_play_item (GstURIDecodeBin3 * dec)
{
  GstPlayItem *item = g_new0 (GstPlayItem, 1);

  item->uridecodebin = dec;
  item->group_id = GST_GROUP_ID_INVALID;

  return item;
}

static void
free_play_item (GstURIDecodeBin3 * dec, GstPlayItem * item)
{
  GST_LOG_OBJECT (dec, "play item %p", item);
  if (item->main_item)
    free_source_item (dec, item->main_item);
  if (item->sub_item)
    free_source_item (dec, item->sub_item);

  g_free (item);
}

static void
play_item_set_uri (GstPlayItem * item, const gchar * uri)
{
  if (!uri)
    return;

  if (!item->main_item) {
    item->main_item =
        new_source_item (item->uridecodebin, item, g_strdup (uri));
  } else {
    source_item_set_uri (item->main_item, uri);
  }
}

static void
play_item_set_suburi (GstPlayItem * item, const gchar * uri)
{
  if (!uri) {
    if (item->sub_item) {
      free_source_item (item->uridecodebin, item->sub_item);
      item->sub_item = NULL;
    }
    return;
  }

  if (!item->sub_item) {
    item->sub_item = new_source_item (item->uridecodebin, item, g_strdup (uri));
  } else {
    source_item_set_uri (item->sub_item, uri);
  }
}

static gboolean
play_item_is_eos (GstPlayItem * item)
{
  if (item->main_item && item->main_item->handler) {
    if (!source_handler_is_eos (item->main_item->handler))
      return FALSE;
  }
  if (item->sub_item && item->sub_item->handler) {
    if (!source_handler_is_eos (item->sub_item->handler))
      return FALSE;
  }

  return TRUE;
}

/* Mark all sourcepads of a play item as EOS. Used in pull-mode */
static void
play_item_set_eos (GstPlayItem * item)
{
  if (item->main_item && item->main_item->handler)
    source_handler_set_eos (item->main_item->handler);

  if (item->sub_item && item->sub_item->handler)
    source_handler_set_eos (item->sub_item->handler);
}

static gboolean
play_item_has_all_pads (GstPlayItem * item)
{
  GstSourceHandler *handler;

  if (item->main_item && item->main_item->handler) {
    handler = item->main_item->handler;
    if (handler->expected_pads != g_list_length (handler->sourcepads))
      return FALSE;
  }

  if (item->sub_item && item->sub_item->handler) {
    handler = item->sub_item->handler;
    if (handler->expected_pads != g_list_length (handler->sourcepads))
      return FALSE;
  }

  return TRUE;
}

/* Returns the next inactive play item. If none available, it will create one
 * and add it to the list of play items */
static GstPlayItem *
next_inactive_play_item (GstURIDecodeBin3 * dec)
{
  GstPlayItem *res;
  GList *iter;

  for (iter = dec->play_items; iter; iter = iter->next) {
    res = iter->data;
    if (!res->active)
      return res;
  }

  GST_DEBUG_OBJECT (dec, "No inactive play items, creating a new one");
  res = new_play_item (dec);
  dec->play_items = g_list_append (dec->play_items, res);

  return res;
}

static GstPadProbeReturn
uri_src_ignore_block_probe (GstPad * pad, GstPadProbeInfo * info,
    GstSourcePad * srcpad)
{
  GST_DEBUG_OBJECT (pad, "blocked");
  return GST_PAD_PROBE_OK;
}

static void
gst_uri_decode_bin3_set_uri (GstURIDecodeBin3 * dec, const gchar * uri)
{
  GstPlayItem *item;
  gboolean start_item = FALSE;

  GST_DEBUG_OBJECT (dec, "uri: %s", uri);

  PLAY_ITEMS_LOCK (dec);
  item = next_inactive_play_item (dec);
  play_item_set_uri (item, uri);

  if (dec->instant_uri && item != dec->input_item) {
    GList *old_pads = get_all_play_item_source_pads (dec->input_item);
    GList *iter;

    /* Switch immediately if not the current input item */
    GST_DEBUG_OBJECT (dec, "Switching immediately");

    /* FLUSH START all input pads */
    for (iter = old_pads; iter; iter = iter->next) {
      GstSourcePad *spad = iter->data;
      if (spad->db3_sink_pad) {
        /* Mark all input pads as EOS */
        gst_pad_send_event (spad->db3_sink_pad, gst_event_new_flush_start ());
      }
      /* Block all input source pads */
      spad->block_probe_id =
          gst_pad_add_probe (spad->src_pad, GST_PAD_PROBE_TYPE_IDLE,
          (GstPadProbeCallback) uri_src_ignore_block_probe, spad, NULL);
      spad->saw_eos = TRUE;
    }
    for (iter = old_pads; iter; iter = iter->next) {
      /* FLUSH_STOP all current input pads */
      GstSourcePad *spad = iter->data;
      if (spad->db3_sink_pad) {
        gst_pad_send_event (spad->db3_sink_pad,
            gst_event_new_flush_stop (TRUE));
      }
    }
    start_item = TRUE;
  } else if (dec->input_item->posted_about_to_finish) {
    GList *iter = g_list_find (dec->play_items, dec->input_item);
    /* If the current item is finishing and the new item is the one just after,
     *  we need to activate it */
    if (iter && iter->next && iter->next->data == item) {
      GST_DEBUG_OBJECT (dec, "Starting new entry (gapless mode)");
      start_item = TRUE;
    }
  }

  PLAY_ITEMS_UNLOCK (dec);
  if (start_item) {
    /* Start new item */
    activate_play_item (item);
  }
}

static void
gst_uri_decode_bin3_set_suburi (GstURIDecodeBin3 * dec, const gchar * uri)
{
  GstPlayItem *item;
  GST_DEBUG_OBJECT (dec, "suburi: %s", uri);

  /* FIXME : Handle instant-uri-change. Should we just apply it automatically to
   * the current input item ? */

  if (dec->input_item->posted_about_to_finish) {
    /* WARNING : Setting sub-uri in gapless mode is unreliable */
    GST_ELEMENT_WARNING (dec, CORE, NOT_IMPLEMENTED,
        ("Setting sub-uri in gapless mode is not handled"),
        ("Setting sub-uri in gapless mode is not implemented"));
  } else {
    item = next_inactive_play_item (dec);
    play_item_set_suburi (item, uri);
  }
}

/* Sync source handlers for the given play item. Might require creating/removing some
 * and/or configure the handlers accordingly */
static GstStateChangeReturn
assign_handlers_to_item (GstURIDecodeBin3 * dec, GstPlayItem * item)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  if (item->main_item == NULL)
    return GST_STATE_CHANGE_FAILURE;

  /* Create missing handlers */
  if (item->main_item->handler == NULL) {
    /* The state lock is taken to ensure we can atomically change the
     * urisourcebin back to NULL in case of failures */
    GST_STATE_LOCK (dec);
    item->main_item->handler = new_source_handler (dec, item, TRUE);
    ret = activate_source_item (item->main_item);
    if (ret == GST_STATE_CHANGE_FAILURE) {
      free_source_handler (dec, item->main_item->handler, FALSE);
      item->main_item->handler = NULL;
    }
    GST_STATE_UNLOCK (dec);
  }

  return ret;
}

/* Called to activate the next play item */
static GstStateChangeReturn
activate_play_item (GstPlayItem * item)
{
  GstStateChangeReturn ret;

  GST_DEBUG_OBJECT (item->uridecodebin, "Activating play item");

  ret = assign_handlers_to_item (item->uridecodebin, item);
  if (ret != GST_STATE_CHANGE_FAILURE) {
    item->active = TRUE;
  }

  return ret;
}

/* Remove all but the last play item */
static void
purge_play_items (GstURIDecodeBin3 * dec)
{
  GST_DEBUG_OBJECT (dec, "Purging play items");

  PLAY_ITEMS_LOCK (dec);
  g_cond_broadcast (&dec->input_source_drained);
  while (dec->play_items && dec->play_items->next) {
    GstPlayItem *item = dec->play_items->data;
    dec->play_items = g_list_remove (dec->play_items, item);
    free_play_item (dec, item);
  }

  dec->output_item = dec->input_item = dec->play_items->data;
  dec->output_item->posted_about_to_finish = FALSE;
  PLAY_ITEMS_UNLOCK (dec);
}

static GstStateChangeReturn
gst_uri_decode_bin3_change_state (GstElement * element,
    GstStateChange transition)
{
  GstStateChangeReturn ret;
  GstURIDecodeBin3 *uridecodebin = (GstURIDecodeBin3 *) element;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      g_object_set (uridecodebin->decodebin, "caps", uridecodebin->caps, NULL);
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      g_atomic_int_set (&uridecodebin->shutdown, 0);
      ret = activate_play_item (uridecodebin->input_item);
      current_updated (uridecodebin);
      if (ret == GST_STATE_CHANGE_FAILURE)
        goto failure;
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      PLAY_ITEMS_LOCK (uridecodebin);
      g_atomic_int_set (&uridecodebin->shutdown, 1);
      g_cond_broadcast (&uridecodebin->input_source_drained);
      PLAY_ITEMS_UNLOCK (uridecodebin);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    goto failure;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      /* Remove all play items *but* the last one, which becomes the current entry */
      purge_play_items (uridecodebin);
      uridecodebin->input_item->active = FALSE;
      break;
    default:
      break;
  }

  return ret;

  /* ERRORS */
failure:
  {
    if (transition == GST_STATE_CHANGE_READY_TO_PAUSED)
      purge_play_items (uridecodebin);
    return ret;
  }
}

static GstSourceHandler *
find_source_handler_for_element (GstURIDecodeBin3 * uridecodebin,
    GstObject * element)
{
  GList *iter;

  for (iter = uridecodebin->play_items; iter; iter = iter->next) {
    GstPlayItem *item = iter->data;

    if (item->main_item && item->main_item->handler) {
      GstSourceHandler *handler = item->main_item->handler;

      if (gst_object_has_as_ancestor (element,
              (GstObject *) handler->urisourcebin))
        return handler;
    }
    if (item->sub_item && item->sub_item->handler) {
      GstSourceHandler *handler = item->sub_item->handler;

      if (gst_object_has_as_ancestor (element,
              (GstObject *) handler->urisourcebin))
        return handler;
    }
  }

  return NULL;
}

static GstMessage *
gst_uri_decode_bin3_handle_redirection (GstURIDecodeBin3 * uridecodebin,
    GstMessage * message, const GstStructure * details)
{
  gchar *uri = NULL;
  GstSourceHandler *handler;
  const gchar *location;
  gchar *current_uri;

  PLAY_ITEMS_LOCK (uridecodebin);
  /* Find the matching handler (if any) */
  handler = find_source_handler_for_element (uridecodebin, message->src);
  if (!handler || !handler->play_item || !handler->play_item->main_item)
    goto beach;

  current_uri = handler->play_item->main_item->uri;

  location = gst_structure_get_string ((GstStructure *) details,
      "redirect-location");
  GST_DEBUG_OBJECT (uridecodebin, "Handle redirection message from '%s' to '%s",
      current_uri, location);

  if (gst_uri_is_valid (location)) {
    uri = g_strdup (location);
  } else if (current_uri) {
    uri = gst_uri_join_strings (current_uri, location);
  }
  if (!uri)
    goto beach;

  if (g_strcmp0 (current_uri, uri)) {
    gst_message_unref (message);
    message = NULL;

    /* We only want to handle the redirection once */
    if (!handler->saw_redirection) {
      gboolean was_instant = uridecodebin->instant_uri;
      handler->saw_redirection = TRUE;
      GST_DEBUG_OBJECT (uridecodebin, "Doing instant switch to '%s'", uri);
      uridecodebin->instant_uri = TRUE;
      /* Force instant switch */
      PLAY_ITEMS_UNLOCK (uridecodebin);
      gst_uri_decode_bin3_set_uri (uridecodebin, uri);
      PLAY_ITEMS_LOCK (uridecodebin);
      uridecodebin->instant_uri = was_instant;
    }
  }
  g_free (uri);

beach:
  PLAY_ITEMS_UNLOCK (uridecodebin);
  return message;
}

static void
gst_uri_decode_bin3_handle_message (GstBin * bin, GstMessage * msg)
{
  GstURIDecodeBin3 *uridecodebin = (GstURIDecodeBin3 *) bin;

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_STREAMS_SELECTED:
    {
      GstSourceHandler *handler;
      GST_DEBUG_OBJECT (uridecodebin, "Handle streams selected");
      PLAY_ITEMS_LOCK (uridecodebin);
      /* Find the matching handler (if any) */
      if ((handler = find_source_handler_for_element (uridecodebin, msg->src))) {
        handler->expected_pads = gst_message_streams_selected_get_size (msg);
        GST_DEBUG_OBJECT (uridecodebin,
            "Got streams-selected for %s with %d streams selected",
            GST_ELEMENT_NAME (handler->urisourcebin), handler->expected_pads);
      }
      PLAY_ITEMS_UNLOCK (uridecodebin);
      break;
    }
    case GST_MESSAGE_BUFFERING:
    {
      GstSourceHandler *handler;
      GST_DEBUG_OBJECT (uridecodebin, "Handle buffering message");
      PLAY_ITEMS_LOCK (uridecodebin);
      /* Find the matching handler (if any) */
      handler = find_source_handler_for_element (uridecodebin, msg->src);
      if (!handler) {
        GST_LOG_OBJECT (uridecodebin, "No handler for message, dropping it");
        gst_message_unref (msg);
        msg = NULL;
      } else if (!uridecodebin->input_item->main_item
          || handler != uridecodebin->input_item->main_item->handler) {
        GST_LOG_OBJECT (uridecodebin,
            "Handler isn't active input item, storing message");
        /* Store the message for a later time */
        if (handler->pending_buffering_msg)
          gst_message_unref (handler->pending_buffering_msg);
        handler->pending_buffering_msg = msg;
        msg = NULL;
      } else {
        /* This is the active main input item, we can forward directly */
        GST_DEBUG_OBJECT (uridecodebin,
            "Forwarding message for active input item");
      }
      PLAY_ITEMS_UNLOCK (uridecodebin);
      break;

    }
    case GST_MESSAGE_ERROR:
    {
      const GstStructure *details = NULL;

      gst_message_parse_error_details (msg, &details);
      if (details && gst_structure_has_field (details, "redirect-location"))
        msg =
            gst_uri_decode_bin3_handle_redirection (uridecodebin, msg, details);
      break;
    }
    default:
      break;
  }

  if (msg)
    GST_BIN_CLASS (parent_class)->handle_message (bin, msg);
}

static gboolean
gst_uri_decodebin3_send_event (GstElement * element, GstEvent * event)
{
  GstURIDecodeBin3 *self = GST_URI_DECODE_BIN3 (element);

  if (GST_EVENT_IS_UPSTREAM (event) && self->decodebin)
    return gst_element_send_event (self->decodebin, event);

  return GST_ELEMENT_CLASS (parent_class)->send_event (element, event);
}
