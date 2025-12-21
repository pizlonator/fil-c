/* GStreamer
 * Copyright (C) <2006> Edward Hervey <edward@fluendo.com>
 * Copyright (C) <2009> Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) <2011> Hewlett-Packard Development Company, L.P.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>, Collabora Ltd.
 * Copyright (C) <2013> Collabora Ltd.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) <2015-2016> Centricular Ltd
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

/**
 * SECTION:element-parsebin
 * @title: parsebin
 *
 * #GstBin that auto-magically constructs a parsing pipeline
 * using available parsers and demuxers via auto-plugging.
 *
 * parsebin unpacks the contents of the input stream to the
 * level of parsed elementary streams, but unlike decodebin
 * it doesn't connect decoder elements. The output pads
 * produce packetised encoded data with timestamps where possible,
 * or send missing-element messages where not.
 */

/* Implementation notes:
 *
 * The following section describes how ParseBin works internally.
 *
 * The first part of ParseBin is its typefind element, which tries
 * to determine the media type of the input stream. If the type is found
 * autoplugging starts.
 *
 * ParseBin internally organizes the elements it autoplugged into
 * GstParseChains and GstParseGroups. A parse chain is a single chain
 * of parsing, this
 * means that if ParseBin ever autoplugs an element with two+ srcpads
 * (e.g. a demuxer) this will end the chain and everything following this
 * demuxer will be put into parse groups below the chain. Otherwise,
 * if an element has a single srcpad that outputs raw data the parse chain
 * is ended too and a GstParsePad is stored and blocked.
 *
 * A parse group combines a number of chains that are created by a
 * demuxer element.
 *
 * This continues until the top-level parse chain is complete. A parse
 * chain is complete if it either ends with a blocked elementary stream,
 * if autoplugging stopped because no suitable plugins could be found
 * or if the active group is complete. A parse group on the other hand
 * is complete if all child chains are complete.
 *
 * If this happens at some point, all end pads of all active groups are exposed.
 * For this ParseBin adds the end pads, and then unblocks them. Now playback starts.
 *
 * If one of the chains that end on a endpad receives EOS ParseBin checks
 * if all chains and groups are drained. In that case everything goes into EOS.
 * If there is a chain where the active group is drained but there exist next
 * groups, the active group is hidden (endpads are removed) and the next group
 * is exposed. This means that in some cases more pads may be created even
 * after the initial no-more-pads signal. This happens for example with
 * so-called "chained oggs", most commonly found among ogg/vorbis internet
 * radio streams.
 *
 * Note 1: If we're talking about blocked endpads this really means that the
 * *target* pads of the endpads are blocked. Pads that are exposed to the outside
 * should never ever be blocked!
 *
 * Note 2: If a group is complete and the parent's chain demuxer adds new pads
 * but never signaled no-more-pads this additional pads will be ignored!
 *
 */

/* FIXME 0.11: suppress warnings for deprecated API such as GValueArray
 * with newer GLib versions (>= 2.31.0) */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n-lib.h>

#include <string.h>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include "gstplay-enum.h"
#include "gstplaybackelements.h"
#include "gstplaybackutils.h"
#include "gstrawcaps.h"

/* generic templates */
static GstStaticPadTemplate parse_bin_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate parse_bin_src_template =
GST_STATIC_PAD_TEMPLATE ("src_%u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC (gst_parse_bin_debug);
#define GST_CAT_DEFAULT gst_parse_bin_debug

typedef struct _GstPendingPad GstPendingPad;
typedef struct _GstParseElement GstParseElement;
typedef struct _GstParseChain GstParseChain;
typedef struct _GstParseGroup GstParseGroup;
typedef struct _GstParsePad GstParsePad;
typedef GstGhostPadClass GstParsePadClass;
typedef struct _GstParseBin GstParseBin;
typedef struct _GstParseBinClass GstParseBinClass;

#define GST_TYPE_PARSE_BIN             (gst_parse_bin_get_type())
#define GST_PARSE_BIN_CAST(obj)        ((GstParseBin*)(obj))
#define GST_PARSE_BIN(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PARSE_BIN,GstParseBin))
#define GST_PARSE_BIN_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_PARSE_BIN,GstParseBinClass))
#define GST_IS_parse_bin(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PARSE_BIN))
#define GST_IS_parse_bin_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_PARSE_BIN))

/**
 *  GstParseBin:
 *
 *  The opaque #GstParseBin data structure
 */
struct _GstParseBin
{
  GstBin bin;                   /* we extend GstBin */

  /* properties */
  gchar *encoding;              /* encoding of subtitles */
  guint64 connection_speed;

  GstElement *typefind;         /* this holds the typefind object */

  GMutex expose_lock;           /* Protects exposal and removal of groups */
  GstParseChain *parse_chain;   /* Top level parse chain */
  guint nbpads;                 /* unique identifier for source pads */

  GMutex factories_lock;
  guint32 factories_cookie;     /* Cookie from last time when factories was updated */
  GList *factories;             /* factories we can use for selecting elements */

  GMutex subtitle_lock;         /* Protects changes to subtitles and encoding */
  GList *subtitles;             /* List of elements with subtitle-encoding,
                                 * protected by above mutex! */

  gboolean have_type;           /* if we received the have_type signal */
  guint have_type_id;           /* signal id for have-type from typefind */

  GMutex dyn_lock;              /* lock protecting pad blocking */
  gboolean shutdown;            /* if we are shutting down */
  GList *blocked_pads;          /* pads that have set to block */

  gboolean expose_allstreams;   /* Whether to expose unknown type streams or not */

  GList *filtered;              /* elements for which error messages are filtered */
  GList *filtered_errors;       /* filtered error messages */

  GMutex cleanup_lock;          /* Mutex used to protect the cleanup thread */
  GThread *cleanup_thread;      /* thread used to free chains asynchronously.
                                 * We store it to make sure we end up joining it
                                 * before stopping the element.
                                 * Protected by the object lock */

};

struct _GstParseBinClass
{
  GstBinClass parent_class;

  /* signal fired when we found a pad that we cannot parse */
  void (*unknown_type) (GstElement * element, GstPad * pad, GstCaps * caps);

  /* signal fired to know if we continue trying to parse the given caps */
    gboolean (*autoplug_continue) (GstElement * element, GstPad * pad,
      GstCaps * caps);
  /* signal fired to get a list of factories to try to autoplug */
  GValueArray *(*autoplug_factories) (GstElement * element, GstPad * pad,
      GstCaps * caps);
  /* signal fired to sort the factories */
  GValueArray *(*autoplug_sort) (GstElement * element, GstPad * pad,
      GstCaps * caps, GValueArray * factories);
  /* signal fired to select from the proposed list of factories */
    GstAutoplugSelectResult (*autoplug_select) (GstElement * element,
      GstPad * pad, GstCaps * caps, GstElementFactory * factory);
  /* signal fired when a autoplugged element that is not linked downstream
   * or exposed wants to query something */
    gboolean (*autoplug_query) (GstElement * element, GstPad * pad,
      GstQuery * query);

  /* fired when the last group is drained */
  void (*drained) (GstElement * element);
};

/* signals */
enum
{
  SIGNAL_UNKNOWN_TYPE,
  SIGNAL_AUTOPLUG_CONTINUE,
  SIGNAL_AUTOPLUG_FACTORIES,
  SIGNAL_AUTOPLUG_SELECT,
  SIGNAL_AUTOPLUG_SORT,
  SIGNAL_AUTOPLUG_QUERY,
  SIGNAL_DRAINED,
  LAST_SIGNAL
};

#define DEFAULT_SUBTITLE_ENCODING NULL
/* by default we use the automatic values above */
#define DEFAULT_EXPOSE_ALL_STREAMS  TRUE
#define DEFAULT_CONNECTION_SPEED    0

/* Properties */
enum
{
  PROP_0,
  PROP_SUBTITLE_ENCODING,
  PROP_SINK_CAPS,
  PROP_EXPOSE_ALL_STREAMS,
  PROP_CONNECTION_SPEED
};

static GstBinClass *parent_class;
static guint gst_parse_bin_signals[LAST_SIGNAL] = { 0 };

static void type_found (GstElement * typefind, guint probability,
    GstCaps * caps, GstParseBin * parse_bin);

static gboolean gst_parse_bin_autoplug_continue (GstElement * element,
    GstPad * pad, GstCaps * caps);
static GValueArray *gst_parse_bin_autoplug_factories (GstElement *
    element, GstPad * pad, GstCaps * caps);
static GValueArray *gst_parse_bin_autoplug_sort (GstElement * element,
    GstPad * pad, GstCaps * caps, GValueArray * factories);
static GstAutoplugSelectResult gst_parse_bin_autoplug_select (GstElement *
    element, GstPad * pad, GstCaps * caps, GstElementFactory * factory);
static gboolean gst_parse_bin_autoplug_query (GstElement * element,
    GstPad * pad, GstQuery * query);

static void gst_parse_bin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_parse_bin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void caps_notify_cb (GstPad * pad, GParamSpec * unused,
    GstParseChain * chain);

static GstStateChangeReturn gst_parse_bin_change_state (GstElement * element,
    GstStateChange transition);
static void gst_parse_bin_handle_message (GstBin * bin, GstMessage * message);
static void gst_parse_pad_update_caps (GstParsePad * parsepad, GstCaps * caps);
static void gst_parse_pad_update_tags (GstParsePad * parsepad,
    GstTagList * tags);
static GstEvent *gst_parse_pad_stream_start_event (GstParsePad * parsepad,
    GstEvent * event);
static void gst_parse_pad_update_stream_collection (GstParsePad * parsepad,
    GstStreamCollection * collection);

static GstCaps *get_pad_caps (GstPad * pad);
static GstStreamType guess_stream_type_from_caps (GstCaps * caps);

#define EXPOSE_LOCK(parsebin) G_STMT_START {				\
    GST_LOG_OBJECT (parsebin,						\
		    "expose locking from thread %p",			\
		    g_thread_self ());					\
    g_mutex_lock (&GST_PARSE_BIN_CAST(parsebin)->expose_lock);		\
    GST_LOG_OBJECT (parsebin,						\
		    "expose locked from thread %p",			\
		    g_thread_self ());					\
} G_STMT_END

#define EXPOSE_UNLOCK(parsebin) G_STMT_START {				\
    GST_LOG_OBJECT (parsebin,						\
		    "expose unlocking from thread %p",			\
		    g_thread_self ());					\
    g_mutex_unlock (&GST_PARSE_BIN_CAST(parsebin)->expose_lock);		\
} G_STMT_END

#define DYN_LOCK(parsebin) G_STMT_START {			\
    GST_LOG_OBJECT (parsebin,						\
		    "dynlocking from thread %p",			\
		    g_thread_self ());					\
    g_mutex_lock (&GST_PARSE_BIN_CAST(parsebin)->dyn_lock);			\
    GST_LOG_OBJECT (parsebin,						\
		    "dynlocked from thread %p",				\
		    g_thread_self ());					\
} G_STMT_END

#define DYN_UNLOCK(parsebin) G_STMT_START {			\
    GST_LOG_OBJECT (parsebin,						\
		    "dynunlocking from thread %p",			\
		    g_thread_self ());					\
    g_mutex_unlock (&GST_PARSE_BIN_CAST(parsebin)->dyn_lock);		\
} G_STMT_END

#define SUBTITLE_LOCK(parsebin) G_STMT_START {				\
    GST_LOG_OBJECT (parsebin,						\
		    "subtitle locking from thread %p",			\
		    g_thread_self ());					\
    g_mutex_lock (&GST_PARSE_BIN_CAST(parsebin)->subtitle_lock);		\
    GST_LOG_OBJECT (parsebin,						\
		    "subtitle lock from thread %p",			\
		    g_thread_self ());					\
} G_STMT_END

#define SUBTITLE_UNLOCK(parsebin) G_STMT_START {				\
    GST_LOG_OBJECT (parsebin,						\
		    "subtitle unlocking from thread %p",		\
		    g_thread_self ());					\
    g_mutex_unlock (&GST_PARSE_BIN_CAST(parsebin)->subtitle_lock);		\
} G_STMT_END

struct _GstPendingPad
{
  GstPad *pad;
  GstParseChain *chain;
  gulong event_probe_id;
  gulong notify_caps_id;
};

struct _GstParseElement
{
  GstElement *element;
  GstElement *capsfilter;       /* Optional capsfilter for Parser/Convert */
  gulong pad_added_id;
  gulong pad_removed_id;
  gulong no_more_pads_id;
};

/* GstParseGroup
 *
 * Streams belonging to the same group/chain of a media file
 *
 * When changing something here lock the parent chain!
 */
struct _GstParseGroup
{
  GstParseBin *parsebin;
  GstParseChain *parent;

  gboolean no_more_pads;        /* TRUE if the demuxer signaled no-more-pads */
  gboolean drained;             /* TRUE if the all children are drained */

  GList *children;              /* List of GstParseChains in this group */
};

struct _GstParseChain
{
  GstParseGroup *parent;
  GstParseBin *parsebin;

  GMutex lock;                  /* Protects this chain and its groups */

  GstPad *pad;                  /* srcpad that caused creation of this chain */
  GstCaps *start_caps;          /* The initial caps of this chain */

  gboolean drained;             /* TRUE if the all children are drained */
  gboolean demuxer;             /* TRUE if elements->data is a demuxer */
  gboolean parsed;              /* TRUE if any elements are a parser */
  GList *elements;              /* All elements in this group, first
                                   is the latest and most downstream element */

  /* Note: there are only groups if the last element of this chain
   * is a demuxer, otherwise the chain will end with an endpad.
   * The other way around this means, that endpad only exists if this
   * chain doesn't end with a demuxer! */

  GstParseGroup *active_group;  /* Currently active group */
  GList *next_groups;           /* head is newest group, tail is next group.
                                   a new group will be created only if the head
                                   group had no-more-pads. If it's only exposed
                                   all new pads will be ignored! */
  GList *pending_pads;          /* Pads that have no fixed caps yet */

  GstParsePad *current_pad;     /* Current ending pad of the chain that can't
                                 * be exposed yet but would be the same as endpad
                                 * once it can be exposed */
  GstParsePad *endpad;          /* Pad of this chain that could be exposed */
  gboolean deadend;             /* This chain is incomplete and can't be completed,
                                   e.g. no suitable parser could be found
                                   e.g. stream got EOS without buffers
                                 */
  gchar *deadend_details;
  GstCaps *endcaps;             /* Caps that were used when linking to the endpad
                                   or that resulted in the deadend
                                 */

  /* FIXME: This should be done directly via a thread! */
  GList *old_groups;            /* Groups that should be freed later */
};

static gboolean gst_parse_chain_accept_caps (GstParseChain * chain,
    GstCaps * caps);
static void gst_parse_chain_free (GstParseChain * chain);
static GstParseChain *gst_parse_chain_new (GstParseBin * parsebin,
    GstParseGroup * group, GstPad * pad, GstCaps * start_caps);
static void gst_parse_group_hide (GstParseGroup * group);
static void gst_parse_group_free (GstParseGroup * group);
static GstParseGroup *gst_parse_group_new (GstParseBin * parsebin,
    GstParseChain * chain);
static gboolean gst_parse_chain_is_complete (GstParseChain * chain);
static gboolean gst_parse_chain_expose (GstParseChain * chain,
    GList ** endpads, gboolean * missing_plugin,
    GString * missing_plugin_details, gboolean * last_group,
    gboolean * uncollected_streams);
static void build_fallback_collection (GstParseChain * chain,
    GstStreamCollection * collection);
static gboolean gst_parse_chain_is_drained (GstParseChain * chain);
static gboolean gst_parse_group_is_complete (GstParseGroup * group);
static gboolean gst_parse_group_is_drained (GstParseGroup * group);

static gboolean gst_parse_bin_expose (GstParseBin * parsebin);

#define CHAIN_MUTEX_LOCK(chain) G_STMT_START {				\
    GST_LOG_OBJECT (chain->parsebin,					\
		    "locking chain %p from thread %p",			\
		    chain, g_thread_self ());				\
    g_mutex_lock (&chain->lock);						\
    GST_LOG_OBJECT (chain->parsebin,					\
		    "locked chain %p from thread %p",			\
		    chain, g_thread_self ());				\
} G_STMT_END

#define CHAIN_MUTEX_UNLOCK(chain) G_STMT_START {                        \
    GST_LOG_OBJECT (chain->parsebin,					\
		    "unlocking chain %p from thread %p",		\
		    chain, g_thread_self ());				\
    g_mutex_unlock (&chain->lock);					\
} G_STMT_END

/* GstParsePad
 *
 * GstPad private used for source pads of chains
 */
struct _GstParsePad
{
  GstGhostPad parent;
  GstParseBin *parsebin;
  GstParseChain *chain;

  gboolean blocked;             /* the *target* pad is blocked */
  gboolean exposed;             /* the pad is exposed */
  gboolean drained;             /* an EOS has been seen on the pad */

  gulong block_id;

  gboolean in_a_fallback_collection;
  GstStreamCollection *active_collection;
  GstStream *active_stream;
};

GType gst_parse_pad_get_type (void);
G_DEFINE_TYPE (GstParsePad, gst_parse_pad, GST_TYPE_GHOST_PAD);
#define GST_TYPE_PARSE_PAD (gst_parse_pad_get_type ())
#define GST_PARSE_PAD(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PARSE_PAD,GstParsePad))

static GstParsePad *gst_parse_pad_new (GstParseBin * parsebin,
    GstParseChain * chain);
static void gst_parse_pad_activate (GstParsePad * parsepad,
    GstParseChain * chain);
static void gst_parse_pad_unblock (GstParsePad * parsepad);
static void gst_parse_pad_set_blocked (GstParsePad * parsepad,
    gboolean blocked);
static gboolean gst_parse_pad_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static GstPadProbeReturn
gst_parse_pad_event (GstPad * pad, GstPadProbeInfo * info, gpointer user_data);


static void gst_pending_pad_free (GstPendingPad * ppad);
static GstPadProbeReturn pad_event_cb (GstPad * pad, GstPadProbeInfo * info,
    gpointer data);

/********************************
 * Standard GObject boilerplate *
 ********************************/

static void gst_parse_bin_dispose (GObject * object);
static void gst_parse_bin_finalize (GObject * object);
static GType gst_parse_bin_get_type (void);

G_DEFINE_TYPE (GstParseBin, gst_parse_bin, GST_TYPE_BIN);
#define _do_init \
    GST_DEBUG_CATEGORY_INIT (gst_parse_bin_debug, "parsebin", 0, "parser bin");\
    playback_element_init (plugin);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (parsebin, "parsebin", GST_RANK_NONE,
    GST_TYPE_PARSE_BIN, _do_init);

static gboolean
_gst_boolean_accumulator (GSignalInvocationHint * ihint,
    GValue * return_accu, const GValue * handler_return, gpointer dummy)
{
  gboolean myboolean;

  myboolean = g_value_get_boolean (handler_return);
  g_value_set_boolean (return_accu, myboolean);

  /* stop emission if FALSE */
  return myboolean;
}

static gboolean
_gst_boolean_or_accumulator (GSignalInvocationHint * ihint,
    GValue * return_accu, const GValue * handler_return, gpointer dummy)
{
  gboolean myboolean;
  gboolean retboolean;

  myboolean = g_value_get_boolean (handler_return);
  retboolean = g_value_get_boolean (return_accu);

  g_value_set_boolean (return_accu, myboolean || retboolean);

  return TRUE;
}

/* we collect the first result */
static gboolean
_gst_array_accumulator (GSignalInvocationHint * ihint,
    GValue * return_accu, const GValue * handler_return, gpointer dummy)
{
  gpointer array;

  array = g_value_get_boxed (handler_return);
  g_value_set_boxed (return_accu, array);

  return FALSE;
}

static gboolean
_gst_select_accumulator (GSignalInvocationHint * ihint,
    GValue * return_accu, const GValue * handler_return, gpointer dummy)
{
  GstAutoplugSelectResult res;

  res = g_value_get_enum (handler_return);
  g_value_set_enum (return_accu, res);

  /* Call the next handler in the chain (if any) when the current callback
   * returns TRY. This makes it possible to register separate autoplug-select
   * handlers that implement different TRY/EXPOSE/SKIP strategies.
   */
  if (res == GST_AUTOPLUG_SELECT_TRY)
    return TRUE;

  return FALSE;
}

static gboolean
_gst_array_hasvalue_accumulator (GSignalInvocationHint * ihint,
    GValue * return_accu, const GValue * handler_return, gpointer dummy)
{
  gpointer array;

  array = g_value_get_boxed (handler_return);
  g_value_set_boxed (return_accu, array);

  if (array != NULL)
    return FALSE;

  return TRUE;
}

static void
gst_parse_bin_class_init (GstParseBinClass * klass)
{
  GObjectClass *gobject_klass;
  GstElementClass *gstelement_klass;
  GstBinClass *gstbin_klass;

  gobject_klass = (GObjectClass *) klass;
  gstelement_klass = (GstElementClass *) klass;
  gstbin_klass = (GstBinClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_klass->dispose = gst_parse_bin_dispose;
  gobject_klass->finalize = gst_parse_bin_finalize;
  gobject_klass->set_property = gst_parse_bin_set_property;
  gobject_klass->get_property = gst_parse_bin_get_property;

  /**
   * GstParseBin::unknown-type:
   * @bin: The ParseBin.
   * @pad: The new pad containing caps that cannot be resolved to a 'final'
   *       stream type.
   * @caps: The #GstCaps of the pad that cannot be resolved.
   *
   * This signal is emitted when a pad for which there is no further possible
   * parsing is added to the ParseBin.
   */
  gst_parse_bin_signals[SIGNAL_UNKNOWN_TYPE] =
      g_signal_new ("unknown-type", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstParseBinClass, unknown_type),
      NULL, NULL, NULL, G_TYPE_NONE, 2, GST_TYPE_PAD, GST_TYPE_CAPS);

  /**
   * GstParseBin::autoplug-continue:
   * @bin: The ParseBin.
   * @pad: The #GstPad.
   * @caps: The #GstCaps found.
   *
   * This signal is emitted whenever ParseBin finds a new stream. It is
   * emitted before looking for any elements that can handle that stream.
   *
   * >   Invocation of signal handlers stops after the first signal handler
   * >   returns %FALSE. Signal handlers are invoked in the order they were
   * >   connected in.
   *
   * Returns: %TRUE if you wish ParseBin to look for elements that can
   * handle the given @caps. If %FALSE, those caps will be considered as
   * final and the pad will be exposed as such (see 'pad-added' signal of
   * #GstElement).
   */
  gst_parse_bin_signals[SIGNAL_AUTOPLUG_CONTINUE] =
      g_signal_new ("autoplug-continue", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstParseBinClass, autoplug_continue),
      _gst_boolean_accumulator, NULL, NULL, G_TYPE_BOOLEAN, 2, GST_TYPE_PAD,
      GST_TYPE_CAPS);

  /**
   * GstParseBin::autoplug-factories:
   * @bin: The ParseBin.
   * @pad: The #GstPad.
   * @caps: The #GstCaps found.
   *
   * This function is emitted when an array of possible factories for @caps on
   * @pad is needed. ParseBin will by default return an array with all
   * compatible factories, sorted by rank.
   *
   * If this function returns NULL, @pad will be exposed as a final caps.
   *
   * If this function returns an empty array, the pad will be considered as
   * having an unhandled type media type.
   *
   * >   Only the signal handler that is connected first will ever by invoked.
   * >   Don't connect signal handlers with the #G_CONNECT_AFTER flag to this
   * >   signal, they will never be invoked!
   *
   * Returns: a #GValueArray* with a list of factories to try. The factories are
   * by default tried in the returned order or based on the index returned by
   * "autoplug-select".
   */
  gst_parse_bin_signals[SIGNAL_AUTOPLUG_FACTORIES] =
      g_signal_new ("autoplug-factories", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstParseBinClass,
          autoplug_factories), _gst_array_accumulator, NULL,
      NULL, G_TYPE_VALUE_ARRAY, 2, GST_TYPE_PAD, GST_TYPE_CAPS);

  /**
   * GstParseBin::autoplug-sort:
   * @bin: The ParseBin.
   * @pad: The #GstPad.
   * @caps: The #GstCaps.
   * @factories: A #GValueArray of possible #GstElementFactory to use.
   *
   * Once ParseBin has found the possible #GstElementFactory objects to try
   * for @caps on @pad, this signal is emitted. The purpose of the signal is for
   * the application to perform additional sorting or filtering on the element
   * factory array.
   *
   * The callee should copy and modify @factories or return %NULL if the
   * order should not change.
   *
   * >   Invocation of signal handlers stops after one signal handler has
   * >   returned something else than %NULL. Signal handlers are invoked in
   * >   the order they were connected in.
   * >   Don't connect signal handlers with the #G_CONNECT_AFTER flag to this
   * >   signal, they will never be invoked!
   *
   * Returns: A new sorted array of #GstElementFactory objects.
   */
  gst_parse_bin_signals[SIGNAL_AUTOPLUG_SORT] =
      g_signal_new ("autoplug-sort", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstParseBinClass, autoplug_sort),
      _gst_array_hasvalue_accumulator, NULL,
      NULL, G_TYPE_VALUE_ARRAY, 3, GST_TYPE_PAD, GST_TYPE_CAPS,
      G_TYPE_VALUE_ARRAY | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GstParseBin::autoplug-select:
   * @bin: The ParseBin.
   * @pad: The #GstPad.
   * @caps: The #GstCaps.
   * @factory: A #GstElementFactory to use.
   *
   * This signal is emitted once ParseBin has found all the possible
   * #GstElementFactory that can be used to handle the given @caps. For each of
   * those factories, this signal is emitted.
   *
   * The signal handler should return a #GstAutoplugSelectResult enum
   * value indicating what ParseBin should do next.
   *
   * A value of #GstAutoplugSelectResult::try will try to autoplug an element from
   * @factory.
   *
   * A value of #GstAutoplugSelectResult::expose will expose @pad without plugging
   * any element to it.
   *
   * A value of #GstAutoplugSelectResult::skip will skip @factory and move to the
   * next factory.
   *
   * >   The signal handler will not be invoked if any of the previously
   * >   registered signal handlers (if any) return a value other than
   * >   GST_AUTOPLUG_SELECT_TRY. Which also means that if you return
   * >   GST_AUTOPLUG_SELECT_TRY from one signal handler, handlers that get
   * >   registered next (again, if any) can override that decision.
   *
   * Returns: a #GstAutoplugSelectResult that indicates the required
   * operation. the default handler will always return
   * #GstAutoplugSelectResult::try.
   */
  gst_parse_bin_signals[SIGNAL_AUTOPLUG_SELECT] =
      g_signal_new ("autoplug-select", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstParseBinClass, autoplug_select),
      _gst_select_accumulator, NULL, NULL, GST_TYPE_AUTOPLUG_SELECT_RESULT, 3,
      GST_TYPE_PAD, GST_TYPE_CAPS, GST_TYPE_ELEMENT_FACTORY);

  /**
   * GstParseBin::autoplug-query:
   * @bin: The ParseBin.
   * @child: The child element doing the query
   * @pad: The #GstPad.
   * @element: The #GstElement.
   * @query: The #GstQuery.
   *
   * This signal is emitted whenever an autoplugged element that is
   * not linked downstream yet and not exposed does a query. It can
   * be used to tell the element about the downstream supported caps
   * for example.
   *
   * Returns: %TRUE if the query was handled, %FALSE otherwise.
   */
  gst_parse_bin_signals[SIGNAL_AUTOPLUG_QUERY] =
      g_signal_new ("autoplug-query", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstParseBinClass, autoplug_query),
      _gst_boolean_or_accumulator, NULL, NULL, G_TYPE_BOOLEAN, 3, GST_TYPE_PAD,
      GST_TYPE_ELEMENT, GST_TYPE_QUERY | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GstParseBin::drained:
   * @bin: The ParseBin
   *
   * This signal is emitted once ParseBin has finished parsing all the data.
   */
  gst_parse_bin_signals[SIGNAL_DRAINED] =
      g_signal_new ("drained", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstParseBinClass, drained),
      NULL, NULL, NULL, G_TYPE_NONE, 0, G_TYPE_NONE);

  g_object_class_install_property (gobject_klass, PROP_SUBTITLE_ENCODING,
      g_param_spec_string ("subtitle-encoding", "subtitle encoding",
          "Encoding to assume if input subtitles are not in UTF-8 encoding. "
          "If not set, the GST_SUBTITLE_ENCODING environment variable will "
          "be checked for an encoding to use. If that is not set either, "
          "ISO-8859-15 will be assumed.", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_klass, PROP_SINK_CAPS,
      g_param_spec_boxed ("sink-caps", "Sink Caps",
          "The caps of the input data. (NULL = use typefind element)",
          GST_TYPE_CAPS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstParseBin::expose-all-streams:
   *
   * Expose streams of unknown type.
   *
   * If set to %FALSE, then only the streams that can be parsed to the final
   * caps (see 'caps' property) will have a pad exposed. Streams that do not
   * match those caps but could have been parsed will not have parser plugged
   * in internally and will not have a pad exposed.
   */
  g_object_class_install_property (gobject_klass, PROP_EXPOSE_ALL_STREAMS,
      g_param_spec_boolean ("expose-all-streams", "Expose All Streams",
          "Expose all streams, including those of unknown type or that don't match the 'caps' property",
          DEFAULT_EXPOSE_ALL_STREAMS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstParseBin2::connection-speed:
   *
   * Network connection speed in kbps (0 = unknownw)
   */
  g_object_class_install_property (gobject_klass, PROP_CONNECTION_SPEED,
      g_param_spec_uint64 ("connection-speed", "Connection Speed",
          "Network connection speed in kbps (0 = unknown)",
          0, G_MAXUINT64 / 1000, DEFAULT_CONNECTION_SPEED,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  klass->autoplug_continue =
      GST_DEBUG_FUNCPTR (gst_parse_bin_autoplug_continue);
  klass->autoplug_factories =
      GST_DEBUG_FUNCPTR (gst_parse_bin_autoplug_factories);
  klass->autoplug_sort = GST_DEBUG_FUNCPTR (gst_parse_bin_autoplug_sort);
  klass->autoplug_select = GST_DEBUG_FUNCPTR (gst_parse_bin_autoplug_select);
  klass->autoplug_query = GST_DEBUG_FUNCPTR (gst_parse_bin_autoplug_query);

  gst_element_class_add_pad_template (gstelement_klass,
      gst_static_pad_template_get (&parse_bin_sink_template));
  gst_element_class_add_pad_template (gstelement_klass,
      gst_static_pad_template_get (&parse_bin_src_template));

  gst_element_class_set_static_metadata (gstelement_klass,
      "Parse Bin", "Generic/Bin/Parser",
      "Parse and de-multiplex to elementary stream",
      "Jan Schmidt <jan@centricular.com>, "
      "Edward Hervey <edward@centricular.com>");

  gstelement_klass->change_state =
      GST_DEBUG_FUNCPTR (gst_parse_bin_change_state);

  gstbin_klass->handle_message =
      GST_DEBUG_FUNCPTR (gst_parse_bin_handle_message);

  g_type_class_ref (GST_TYPE_PARSE_PAD);
}

static void
gst_parse_bin_update_factories_list (GstParseBin * parsebin)
{
  guint cookie;

  cookie = gst_registry_get_feature_list_cookie (gst_registry_get ());
  if (!parsebin->factories || parsebin->factories_cookie != cookie) {
    if (parsebin->factories)
      gst_plugin_feature_list_free (parsebin->factories);
    parsebin->factories =
        gst_element_factory_list_get_elements
        (GST_ELEMENT_FACTORY_TYPE_DECODABLE, GST_RANK_MARGINAL);
    parsebin->factories =
        g_list_sort (parsebin->factories,
        gst_playback_utils_compare_factories_func);
    parsebin->factories_cookie = cookie;
  }
}

static gboolean
sink_query_function (GstPad * sinkpad, GstParseBin * parsebin, GstQuery * query)
{
  GST_DEBUG_OBJECT (parsebin, "query %" GST_PTR_FORMAT, query);

  if (parsebin->parse_chain && GST_QUERY_TYPE (query) == GST_QUERY_ACCEPT_CAPS) {
    GstCaps *querycaps = NULL;
    gst_query_parse_accept_caps (query, &querycaps);
    if (querycaps) {
      gboolean ret =
          gst_parse_chain_accept_caps (parsebin->parse_chain, querycaps);
      gst_query_set_accept_caps_result (query, ret);
    }
    return TRUE;
  }
  return gst_pad_query_default (sinkpad, GST_OBJECT (parsebin), query);
}

static void
gst_parse_bin_init (GstParseBin * parse_bin)
{
  /* first filter out the interesting element factories */
  g_mutex_init (&parse_bin->factories_lock);

  /* we create the typefind element only once */
  parse_bin->typefind = gst_element_factory_make ("typefind", "typefind");
  if (!parse_bin->typefind) {
    g_warning ("can't find typefind element, ParseBin will not work");
  } else {
    GstPad *pad;
    GstPad *gpad;
    GstPadTemplate *pad_tmpl;

    /* add the typefind element */
    if (!gst_bin_add (GST_BIN (parse_bin), parse_bin->typefind)) {
      g_warning ("Could not add typefind element, ParseBin will not work");
      gst_object_unref (parse_bin->typefind);
      parse_bin->typefind = NULL;
    }

    /* get the sinkpad */
    pad = gst_element_get_static_pad (parse_bin->typefind, "sink");

    /* get the pad template */
    pad_tmpl = gst_static_pad_template_get (&parse_bin_sink_template);

    /* ghost the sink pad to ourself */
    gpad = gst_ghost_pad_new_from_template ("sink", pad, pad_tmpl);
    gst_pad_set_query_function (gpad,
        (GstPadQueryFunction) sink_query_function);
    gst_pad_set_active (gpad, TRUE);
    gst_element_add_pad (GST_ELEMENT (parse_bin), gpad);

    gst_object_unref (pad_tmpl);
    gst_object_unref (pad);
  }

  g_mutex_init (&parse_bin->expose_lock);
  parse_bin->parse_chain = NULL;

  g_mutex_init (&parse_bin->dyn_lock);
  parse_bin->shutdown = FALSE;
  parse_bin->blocked_pads = NULL;

  g_mutex_init (&parse_bin->subtitle_lock);

  parse_bin->encoding = g_strdup (DEFAULT_SUBTITLE_ENCODING);

  parse_bin->expose_allstreams = DEFAULT_EXPOSE_ALL_STREAMS;
  parse_bin->connection_speed = DEFAULT_CONNECTION_SPEED;

  g_mutex_init (&parse_bin->cleanup_lock);
  parse_bin->cleanup_thread = NULL;

  GST_OBJECT_FLAG_SET (parse_bin, GST_BIN_FLAG_STREAMS_AWARE);
}

static void
gst_parse_bin_dispose (GObject * object)
{
  GstParseBin *parse_bin;

  parse_bin = GST_PARSE_BIN (object);

  if (parse_bin->factories)
    gst_plugin_feature_list_free (parse_bin->factories);
  parse_bin->factories = NULL;

  if (parse_bin->parse_chain)
    gst_parse_chain_free (parse_bin->parse_chain);
  parse_bin->parse_chain = NULL;

  g_free (parse_bin->encoding);
  parse_bin->encoding = NULL;

  g_list_free (parse_bin->subtitles);
  parse_bin->subtitles = NULL;

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_parse_bin_finalize (GObject * object)
{
  GstParseBin *parse_bin;

  parse_bin = GST_PARSE_BIN (object);

  g_mutex_clear (&parse_bin->expose_lock);
  g_mutex_clear (&parse_bin->dyn_lock);
  g_mutex_clear (&parse_bin->subtitle_lock);
  g_mutex_clear (&parse_bin->factories_lock);
  g_mutex_clear (&parse_bin->cleanup_lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_parse_bin_set_sink_caps (GstParseBin * parsebin, GstCaps * caps)
{
  GST_DEBUG_OBJECT (parsebin, "Setting new caps: %" GST_PTR_FORMAT, caps);

  g_object_set (parsebin->typefind, "force-caps", caps, NULL);
}

static GstCaps *
gst_parse_bin_get_sink_caps (GstParseBin * parsebin)
{
  GstCaps *caps;

  GST_DEBUG_OBJECT (parsebin, "Getting currently set caps");

  g_object_get (parsebin->typefind, "force-caps", &caps, NULL);

  return caps;
}

static void
gst_parse_bin_set_subs_encoding (GstParseBin * parsebin, const gchar * encoding)
{
  GList *walk;

  GST_DEBUG_OBJECT (parsebin, "Setting new encoding: %s",
      GST_STR_NULL (encoding));

  SUBTITLE_LOCK (parsebin);
  g_free (parsebin->encoding);
  parsebin->encoding = g_strdup (encoding);

  /* set the subtitle encoding on all added elements */
  for (walk = parsebin->subtitles; walk; walk = g_list_next (walk)) {
    g_object_set (G_OBJECT (walk->data), "subtitle-encoding",
        parsebin->encoding, NULL);
  }
  SUBTITLE_UNLOCK (parsebin);
}

static gchar *
gst_parse_bin_get_subs_encoding (GstParseBin * parsebin)
{
  gchar *encoding;

  GST_DEBUG_OBJECT (parsebin, "Getting currently set encoding");

  SUBTITLE_LOCK (parsebin);
  encoding = g_strdup (parsebin->encoding);
  SUBTITLE_UNLOCK (parsebin);

  return encoding;
}

static void
gst_parse_bin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstParseBin *parsebin;

  parsebin = GST_PARSE_BIN (object);

  switch (prop_id) {
    case PROP_SUBTITLE_ENCODING:
      gst_parse_bin_set_subs_encoding (parsebin, g_value_get_string (value));
      break;
    case PROP_SINK_CAPS:
      gst_parse_bin_set_sink_caps (parsebin, g_value_get_boxed (value));
      break;
    case PROP_EXPOSE_ALL_STREAMS:
      parsebin->expose_allstreams = g_value_get_boolean (value);
      break;
    case PROP_CONNECTION_SPEED:
      GST_OBJECT_LOCK (parsebin);
      parsebin->connection_speed = g_value_get_uint64 (value) * 1000;
      GST_OBJECT_UNLOCK (parsebin);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_parse_bin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstParseBin *parsebin;

  parsebin = GST_PARSE_BIN (object);
  switch (prop_id) {
    case PROP_SUBTITLE_ENCODING:
      g_value_take_string (value, gst_parse_bin_get_subs_encoding (parsebin));
      break;
    case PROP_SINK_CAPS:
      g_value_take_boxed (value, gst_parse_bin_get_sink_caps (parsebin));
      break;
    case PROP_EXPOSE_ALL_STREAMS:
      g_value_set_boolean (value, parsebin->expose_allstreams);
      break;
    case PROP_CONNECTION_SPEED:
      GST_OBJECT_LOCK (parsebin);
      g_value_set_uint64 (value, parsebin->connection_speed / 1000);
      GST_OBJECT_UNLOCK (parsebin);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


/*****
 * Default autoplug signal handlers
 *****/
static gboolean
gst_parse_bin_autoplug_continue (GstElement * element, GstPad * pad,
    GstCaps * caps)
{
  static GstStaticCaps raw_caps = GST_STATIC_CAPS (DEFAULT_RAW_CAPS);

  GST_DEBUG_OBJECT (element, "caps %" GST_PTR_FORMAT, caps);

  /* If it matches our target caps, expose it */
  if (gst_caps_can_intersect (caps, gst_static_caps_get (&raw_caps))) {
    GST_DEBUG_OBJECT (element, "autoplug-continue returns FALSE");
    return FALSE;
  }

  GST_DEBUG_OBJECT (element, "autoplug-continue returns TRUE");

  return TRUE;
}

static GValueArray *
gst_parse_bin_autoplug_factories (GstElement * element, GstPad * pad,
    GstCaps * caps)
{
  GList *list, *tmp;
  GValueArray *result;
  GstParseBin *parsebin = GST_PARSE_BIN_CAST (element);

  GST_DEBUG_OBJECT (element, "finding factories");

  /* return all compatible factories for caps */
  g_mutex_lock (&parsebin->factories_lock);
  gst_parse_bin_update_factories_list (parsebin);
  list =
      gst_element_factory_list_filter (parsebin->factories, caps, GST_PAD_SINK,
      gst_caps_is_fixed (caps));
  g_mutex_unlock (&parsebin->factories_lock);

  result = g_value_array_new (g_list_length (list));
  for (tmp = list; tmp; tmp = tmp->next) {
    GstElementFactory *factory = GST_ELEMENT_FACTORY_CAST (tmp->data);
    GValue val = { 0, };

    g_value_init (&val, G_TYPE_OBJECT);
    g_value_set_object (&val, factory);
    g_value_array_append (result, &val);
    g_value_unset (&val);
  }
  gst_plugin_feature_list_free (list);

  GST_DEBUG_OBJECT (element, "autoplug-factories returns %p", result);

  return result;
}

static GValueArray *
gst_parse_bin_autoplug_sort (GstElement * element, GstPad * pad,
    GstCaps * caps, GValueArray * factories)
{
  return NULL;
}

static GstAutoplugSelectResult
gst_parse_bin_autoplug_select (GstElement * element, GstPad * pad,
    GstCaps * caps, GstElementFactory * factory)
{
  /* Try factory. */
  return GST_AUTOPLUG_SELECT_TRY;
}

static gboolean
gst_parse_bin_autoplug_query (GstElement * element, GstPad * pad,
    GstQuery * query)
{
  /* No query handled here */
  return FALSE;
}

/********
 * Discovery methods
 *****/

static gboolean is_demuxer_element (GstElement * srcelement);

static gboolean connect_pad (GstParseBin * parsebin, GstElement * src,
    GstParsePad * parsepad, GstPad * pad, GstCaps * caps,
    GValueArray * factories, GstParseChain * chain, gchar ** deadend_details);
static GList *connect_element (GstParseBin * parsebin, GstParseElement * pelem,
    GstParseChain * chain);
static void expose_pad (GstParseBin * parsebin, GstElement * src,
    GstParsePad * parsepad, GstPad * pad, GstCaps * caps,
    GstParseChain * chain);

static void pad_added_cb (GstElement * element, GstPad * pad,
    GstParseChain * chain);
static void pad_removed_cb (GstElement * element, GstPad * pad,
    GstParseChain * chain);
static void no_more_pads_cb (GstElement * element, GstParseChain * chain);

static GstParseGroup *gst_parse_chain_get_current_group (GstParseChain * chain);

static gboolean
clear_sticky_events (GstPad * pad, GstEvent ** event, gpointer user_data)
{
  GST_DEBUG_OBJECT (pad, "clearing sticky event %" GST_PTR_FORMAT, *event);
  gst_event_unref (*event);
  *event = NULL;
  return TRUE;
}

static gboolean
copy_sticky_events (GstPad * pad, GstEvent ** eventptr, gpointer user_data)
{
  GstParsePad *ppad = GST_PARSE_PAD (user_data);
  GstEvent *event = gst_event_ref (*eventptr);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:{
      GstCaps *caps = NULL;
      gst_event_parse_caps (event, &caps);
      gst_parse_pad_update_caps (ppad, caps);
      break;
    }
    case GST_EVENT_STREAM_START:{
      event = gst_parse_pad_stream_start_event (ppad, event);
      break;
    }
    case GST_EVENT_STREAM_COLLECTION:{
      GstStreamCollection *collection = NULL;
      gst_event_parse_stream_collection (event, &collection);
      gst_parse_pad_update_stream_collection (ppad, collection);
      gst_object_unref (collection);
      break;
    }
    default:
      break;
  }

  GST_DEBUG_OBJECT (ppad, "store sticky event %" GST_PTR_FORMAT, event);
  gst_pad_store_sticky_event (GST_PAD_CAST (ppad), event);
  gst_event_unref (event);

  return TRUE;
}

static void
parse_pad_set_target (GstParsePad * parsepad, GstPad * target)
{
  GstPad *old_target = gst_ghost_pad_get_target (GST_GHOST_PAD_CAST (parsepad));
  if (old_target)
    gst_object_unref (old_target);

  if (old_target == target)
    return;

  gst_pad_sticky_events_foreach (GST_PAD_CAST (parsepad),
      clear_sticky_events, NULL);
  gst_ghost_pad_set_target (GST_GHOST_PAD_CAST (parsepad), target);

  if (target == NULL) {
    GST_LOG_OBJECT (parsepad->parsebin, "Setting pad %" GST_PTR_FORMAT
        " target to NULL", parsepad);
  } else {
    GST_LOG_OBJECT (parsepad->parsebin, "Setting pad %" GST_PTR_FORMAT
        " target to %" GST_PTR_FORMAT, parsepad, target);
    gst_pad_sticky_events_foreach (target, copy_sticky_events, parsepad);
  }
}

/* called when a new pad is discovered. It will perform some basic actions
 * before trying to link something to it.
 *
 *  - Check the caps, don't do anything when there are no caps or when they have
 *    no good type.
 *  - signal AUTOPLUG_CONTINUE to check if we need to continue autoplugging this
 *    pad.
 *  - if the caps are non-fixed, setup a handler to continue autoplugging when
 *    the caps become fixed (connect to notify::caps).
 *  - get list of factories to autoplug.
 *  - continue autoplugging to one of the factories.
 */
static void
analyze_new_pad (GstParseBin * parsebin, GstElement * src, GstPad * pad,
    GstCaps * caps, GstParseChain * chain)
{
  gboolean apcontinue = TRUE;
  GValueArray *factories = NULL, *result = NULL;
  GstParsePad *parsepad = NULL;
  GstElementFactory *factory;
  const gchar *classification;
  gboolean is_parser_converter = FALSE;
  gboolean res;
  gchar *deadend_details = NULL;

  GST_DEBUG_OBJECT (parsebin, "Pad %s:%s caps:%" GST_PTR_FORMAT,
      GST_DEBUG_PAD_NAME (pad), caps);

  if (chain->elements
      && src != ((GstParseElement *) chain->elements->data)->element
      && src != ((GstParseElement *) chain->elements->data)->capsfilter) {
    GST_ERROR_OBJECT (parsebin,
        "New pad from not the last element in this chain");
    return;
  }

  if (chain->demuxer) {
    GstParseGroup *group;
    GstParseChain *oldchain = chain;
    GstParseElement *demux = (chain->elements ? chain->elements->data : NULL);

    if (chain->current_pad)
      gst_object_unref (chain->current_pad);
    chain->current_pad = NULL;

    /* we are adding a new pad for a demuxer (see is_demuxer_element(),
     * start a new chain for it */
    CHAIN_MUTEX_LOCK (oldchain);
    group = gst_parse_chain_get_current_group (chain);
    if (group && !g_list_find (group->children, chain)) {
      chain = gst_parse_chain_new (parsebin, group, pad, caps);
      group->children = g_list_prepend (group->children, chain);
    }
    CHAIN_MUTEX_UNLOCK (oldchain);
    if (!group) {
      GST_WARNING_OBJECT (parsebin, "No current group");
      return;
    }

    /* If this is not a dynamic pad demuxer, we're no-more-pads
     * already before anything else happens
     */
    if (demux == NULL || !demux->no_more_pads_id)
      group->no_more_pads = TRUE;
  }

  /* From here on we own a reference to the caps as
   * we might create new caps below and would need
   * to unref them later */
  if (caps)
    gst_caps_ref (caps);

  if ((caps == NULL) || gst_caps_is_empty (caps))
    goto unknown_type;

  if (gst_caps_is_any (caps))
    goto any_caps;

  if (!chain->current_pad)
    chain->current_pad = gst_parse_pad_new (parsebin, chain);

  parsepad = gst_object_ref (chain->current_pad);
  gst_pad_set_active (GST_PAD_CAST (parsepad), TRUE);
  parse_pad_set_target (parsepad, pad);

  /* 1. Emit 'autoplug-continue' the result will tell us if this pads needs
   * further autoplugging. Only do this for fixed caps, for unfixed caps
   * we will later come here again from the notify::caps handler. The
   * problem with unfixed caps is that, we can't reliably tell if the output
   * is e.g. accepted by a sink because only parts of the possible final
   * caps might be accepted by the sink. */
  if (gst_caps_is_fixed (caps))
    g_signal_emit (G_OBJECT (parsebin),
        gst_parse_bin_signals[SIGNAL_AUTOPLUG_CONTINUE], 0, parsepad, caps,
        &apcontinue);
  else
    apcontinue = TRUE;

  /* 1.a if autoplug-continue is FALSE or caps is a raw format, goto pad_is_final */
  if (!apcontinue)
    goto expose_pad;

  /* 1.b For Parser/Converter that can output different stream formats
   * we insert a capsfilter with the sorted caps of all possible next
   * elements and continue with the capsfilter srcpad */
  factory = gst_element_get_factory (src);
  classification =
      gst_element_factory_get_metadata (factory, GST_ELEMENT_METADATA_KLASS);
  is_parser_converter = (strstr (classification, "Parser")
      && strstr (classification, "Converter"));

  /* FIXME: We just need to be sure that the next element is not a parser */
  /* 1.c when the caps are not fixed yet, we can't be sure what element to
   * connect. We delay autoplugging until the caps are fixed */
  if (!is_parser_converter && !gst_caps_is_fixed (caps)) {
    goto non_fixed;
  } else if (!is_parser_converter) {
    gst_caps_unref (caps);
    caps = gst_pad_get_current_caps (pad);
    if (!caps) {
      GST_DEBUG_OBJECT (parsebin,
          "No final caps set yet, delaying autoplugging");
      gst_object_unref (parsepad);
      goto setup_caps_delay;
    }
  }

  /* 1.d else get the factories and if there's no compatible factory goto
   * unknown_type */
  g_signal_emit (G_OBJECT (parsebin),
      gst_parse_bin_signals[SIGNAL_AUTOPLUG_FACTORIES], 0, parsepad, caps,
      &factories);

  /* NULL means that we can expose the pad */
  if (factories == NULL)
    goto expose_pad;

  /* if the array is empty, we have a type for which we have no handler */
  if (factories->n_values == 0) {
    g_value_array_free (factories);

    if (parsebin->expose_allstreams) {
      /* If we expose all streams, we only need to inform the application about
       * a missing handler but still expose it. We also make sure the stream
       * type is unknown. */
      if (parsepad->in_a_fallback_collection) {
        GstStream *newstream;
        GST_LOG_OBJECT (parsepad, "Existing GstStream %" GST_PTR_FORMAT,
            parsepad->active_stream);
        g_assert (parsepad->active_stream);
        newstream =
            gst_stream_new (gst_stream_get_stream_id (parsepad->active_stream),
            caps, GST_STREAM_TYPE_UNKNOWN,
            gst_stream_get_stream_flags (parsepad->active_stream));
        gst_object_replace ((GstObject **) & parsepad->active_stream,
            (GstObject *) newstream);
        GST_LOG_OBJECT (parsepad, "New GstStream %" GST_PTR_FORMAT,
            parsepad->active_stream);
      }
      gst_element_post_message (GST_ELEMENT_CAST (parsebin),
          gst_missing_decoder_message_new (GST_ELEMENT_CAST (parsebin), caps));
      goto expose_pad;
    }
    /* Else we will bail out */
    goto unknown_type;
  }

  /* 1.e sort some more. */
  g_signal_emit (G_OBJECT (parsebin),
      gst_parse_bin_signals[SIGNAL_AUTOPLUG_SORT], 0, parsepad, caps, factories,
      &result);
  if (result) {
    g_value_array_free (factories);
    factories = result;
  }

  /* 1.g now get the factory template caps and insert the capsfilter if this
   * is a parser/converter
   */
  if (is_parser_converter) {
    GstCaps *filter_caps;
    gint i;
    GstPad *p;
    GstParseElement *pelem;

    g_assert (chain->elements != NULL);
    pelem = (GstParseElement *) chain->elements->data;

    filter_caps = gst_caps_new_empty ();
    for (i = 0; i < factories->n_values; i++) {
      GstElementFactory *factory =
          g_value_get_object (g_value_array_get_nth (factories, i));
      GstCaps *tcaps, *intersection;
      const GList *tmps;

      GST_DEBUG ("Trying factory %s",
          gst_plugin_feature_get_name (GST_PLUGIN_FEATURE (factory)));

      if (gst_element_get_factory (src) == factory ||
          gst_element_factory_list_is_type (factory,
              GST_ELEMENT_FACTORY_TYPE_PARSER)) {
        GST_DEBUG ("Skipping factory");
        continue;
      }

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

    /* Append the parser caps to prevent any not-negotiated errors */
    filter_caps = gst_caps_merge (filter_caps, gst_caps_ref (caps));

    pelem->capsfilter = gst_element_factory_make ("capsfilter", NULL);
    g_object_set (G_OBJECT (pelem->capsfilter), "caps", filter_caps, NULL);
    gst_caps_unref (filter_caps);
    gst_element_set_state (pelem->capsfilter, GST_STATE_PAUSED);
    gst_bin_add (GST_BIN_CAST (parsebin), gst_object_ref (pelem->capsfilter));

    parse_pad_set_target (parsepad, NULL);
    p = gst_element_get_static_pad (pelem->capsfilter, "sink");
    gst_pad_link_full (pad, p, GST_PAD_LINK_CHECK_NOTHING);
    gst_object_unref (p);
    p = gst_element_get_static_pad (pelem->capsfilter, "src");
    parse_pad_set_target (parsepad, p);
    pad = p;

    gst_caps_unref (caps);

    caps = gst_pad_get_current_caps (pad);
    if (!caps) {
      GST_DEBUG_OBJECT (parsebin,
          "No final caps set yet, delaying autoplugging");
      gst_object_unref (parsepad);
      g_value_array_free (factories);
      goto setup_caps_delay;
    }
  }

  /* 1.h else continue autoplugging something from the list. */
  GST_LOG_OBJECT (pad, "Let's continue discovery on this pad");
  res =
      connect_pad (parsebin, src, parsepad, pad, caps, factories, chain,
      &deadend_details);

  /* Need to unref the capsfilter srcpad here if
   * we inserted a capsfilter */
  if (is_parser_converter)
    gst_object_unref (pad);

  g_value_array_free (factories);

  if (!res) {
    if (deadend_details == NULL) {
      /* connect_pad() only failed because no element was compatible
       * (i.e. deadend_details is NULL). If this stream is an elementary stream,
       * we can expose it since this is non-fatal */
      GstPbUtilsCapsDescriptionFlags caps_flags =
          gst_pb_utils_get_caps_description_flags (caps);
      if (caps_flags
          && !(caps_flags & GST_PBUTILS_CAPS_DESCRIPTION_FLAG_CONTAINER)) {
        goto expose_pad;
      }
    }
    goto unknown_type;
  }

  gst_object_unref (parsepad);
  gst_caps_unref (caps);

  return;

expose_pad:
  {
    GST_LOG_OBJECT (parsebin, "Pad is final. autoplug-continue:%d", apcontinue);
    expose_pad (parsebin, src, parsepad, pad, caps, chain);
    gst_object_unref (parsepad);
    gst_caps_unref (caps);
    return;
  }

unknown_type:
  {
    GST_LOG_OBJECT (pad, "Unknown type, posting message and firing signal");
    if (parsepad)
      gst_object_unref (parsepad);

    chain->deadend_details = deadend_details;
    chain->deadend = TRUE;
    chain->drained = TRUE;
    chain->endcaps = caps;
    gst_object_replace ((GstObject **) & chain->current_pad, NULL);

    gst_element_post_message (GST_ELEMENT_CAST (parsebin),
        gst_missing_decoder_message_new (GST_ELEMENT_CAST (parsebin), caps));

    g_signal_emit (G_OBJECT (parsebin),
        gst_parse_bin_signals[SIGNAL_UNKNOWN_TYPE], 0, pad, caps);

    /* Try to expose anything */
    EXPOSE_LOCK (parsebin);
    if (parsebin->parse_chain) {
      if (gst_parse_chain_is_complete (parsebin->parse_chain)) {
        gst_parse_bin_expose (parsebin);
      }
    }
    EXPOSE_UNLOCK (parsebin);

    if (src == parsebin->typefind) {
      if (!caps || gst_caps_is_empty (caps)) {
        GST_ELEMENT_ERROR (parsebin, STREAM, TYPE_NOT_FOUND,
            (_("Could not determine type of stream")), (NULL));
      }
    }
    return;
  }
#if 1
non_fixed:
  {
    GST_DEBUG_OBJECT (pad, "pad has non-fixed caps delay autoplugging");
    gst_object_unref (parsepad);
    goto setup_caps_delay;
  }
#endif
any_caps:
  {
    GST_DEBUG_OBJECT (pad, "pad has ANY caps, delaying auto-plugging");
    goto setup_caps_delay;
  }
setup_caps_delay:
  {
    GstPendingPad *ppad;

    /* connect to caps notification */
    CHAIN_MUTEX_LOCK (chain);
    GST_LOG_OBJECT (parsebin, "Chain %p has now %d dynamic pads", chain,
        g_list_length (chain->pending_pads));
    ppad = g_new0 (GstPendingPad, 1);
    ppad->pad = gst_object_ref (pad);
    ppad->chain = chain;
    ppad->event_probe_id =
        gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
        pad_event_cb, ppad, NULL);
    chain->pending_pads = g_list_prepend (chain->pending_pads, ppad);
    ppad->notify_caps_id = g_signal_connect (pad, "notify::caps",
        G_CALLBACK (caps_notify_cb), chain);
    CHAIN_MUTEX_UNLOCK (chain);

    /* If we're here because we have a Parser/Converter
     * we have to unref the pad */
    if (is_parser_converter)
      gst_object_unref (pad);
    if (caps)
      gst_caps_unref (caps);

    return;
  }
}

static void
add_error_filter (GstParseBin * parsebin, GstElement * element)
{
  GST_OBJECT_LOCK (parsebin);
  parsebin->filtered = g_list_prepend (parsebin->filtered, element);
  GST_OBJECT_UNLOCK (parsebin);
}

static void
remove_error_filter (GstParseBin * parsebin, GstElement * element,
    GstMessage ** error)
{
  GList *l;

  GST_OBJECT_LOCK (parsebin);
  parsebin->filtered = g_list_remove (parsebin->filtered, element);

  if (error)
    *error = NULL;

  l = parsebin->filtered_errors;
  while (l) {
    GstMessage *msg = l->data;

    if (GST_MESSAGE_SRC (msg) == GST_OBJECT_CAST (element)) {
      /* Get the last error of this element, i.e. the earliest */
      if (error)
        gst_message_replace (error, msg);
      gst_message_unref (msg);
      l = parsebin->filtered_errors =
          g_list_delete_link (parsebin->filtered_errors, l);
    } else {
      l = l->next;
    }
  }
  GST_OBJECT_UNLOCK (parsebin);
}

typedef struct
{
  gboolean ret;
  GstPad *peer;
} SendStickyEventsData;

static gboolean
send_sticky_event (GstPad * pad, GstEvent ** event, gpointer user_data)
{
  SendStickyEventsData *data = user_data;
  gboolean ret;

  ret = gst_pad_send_event (data->peer, gst_event_ref (*event));
  if (!ret)
    data->ret = FALSE;

  return data->ret;
}

static gboolean
send_sticky_events (GstParseBin * parsebin, GstPad * pad)
{
  SendStickyEventsData data;

  data.ret = TRUE;
  data.peer = gst_pad_get_peer (pad);

  gst_pad_sticky_events_foreach (pad, send_sticky_event, &data);

  gst_object_unref (data.peer);

  return data.ret;
}

static gchar *
error_message_to_string (GstMessage * msg)
{
  GError *err;
  gchar *debug, *message, *full_message;

  gst_message_parse_error (msg, &err, &debug);

  message = gst_error_get_message (err->domain, err->code);

  if (debug)
    full_message = g_strdup_printf ("%s\n%s\n%s", message, err->message, debug);
  else
    full_message = g_strdup_printf ("%s\n%s", message, err->message);

  g_free (message);
  g_free (debug);
  g_clear_error (&err);

  return full_message;
}

/* We consider elements as "simple demuxer" when they are a demuxer
 * with one and only one ALWAYS source pad.
 */
static gboolean
is_simple_demuxer_factory (GstElementFactory * factory)
{
  if (strstr (gst_element_factory_get_metadata (factory,
              GST_ELEMENT_METADATA_KLASS), "Demuxer")) {
    const GList *tmp;
    gint num_alway_srcpads = 0;

    for (tmp = gst_element_factory_get_static_pad_templates (factory);
        tmp; tmp = tmp->next) {
      GstStaticPadTemplate *template = tmp->data;

      if (template->direction == GST_PAD_SRC) {
        if (template->presence == GST_PAD_ALWAYS) {
          if (num_alway_srcpads >= 0)
            num_alway_srcpads++;
        } else {
          num_alway_srcpads = -1;
        }
      }

    }

    if (num_alway_srcpads == 1)
      return TRUE;
  }

  return FALSE;
}

/* connect_pad:
 *
 * Try to connect the given pad to an element created from one of the factories,
 * and recursively.
 *
 * Note that parsepad is ghosting pad, and so pad is linked; be sure to unset parsepad's
 * target before trying to link pad.
 *
 * Returns TRUE if an element was properly created and linked
 */
static gboolean
connect_pad (GstParseBin * parsebin, GstElement * src, GstParsePad * parsepad,
    GstPad * pad, GstCaps * caps, GValueArray * factories,
    GstParseChain * chain, gchar ** deadend_details)
{
  gboolean res = FALSE;
  GString *error_details = NULL;
  GstFormat segment_format = GST_FORMAT_TIME;
  GstPbUtilsCapsDescriptionFlags caps_flags =
      gst_pb_utils_get_caps_description_flags (caps);

  g_return_val_if_fail (factories != NULL, FALSE);
  g_return_val_if_fail (factories->n_values > 0, FALSE);

  /* For subtitles, which can come from standalone files, we need to ensure we
   * output a timed/parsed stream. But not all formats have a parser, so we also
   * want to try plugging in subtitle "decoders" like `subparse`.
   *
   * In order to ensure that, if the caps are subtitles, we query the stream
   * format to check if it's in time or not. If it's not in time format, we will
   * attempt to plugin in a "decoder" (if present). */
  if (caps_flags == GST_PBUTILS_CAPS_DESCRIPTION_FLAG_SUBTITLE) {
    GstEvent *segment_event =
        gst_pad_get_sticky_event (pad, GST_EVENT_SEGMENT, 0);
    const GstSegment *segment = NULL;

    segment_format = GST_FORMAT_UNDEFINED;
    if (segment_event) {
      gst_event_parse_segment (segment_event, &segment);
      if (segment)
        segment_format = segment->format;
    }
    if (segment == GST_FORMAT_UNDEFINED) {
      GstQuery *segment_query = gst_query_new_segment (GST_FORMAT_TIME);
      if (gst_pad_query (pad, segment_query)) {
        gst_query_parse_segment (segment_query, NULL, &segment_format, NULL,
            NULL);
      }
      gst_query_unref (segment_query);
    }
  }

  GST_DEBUG_OBJECT (parsebin,
      "pad %s:%s , chain:%p, %d factories, caps %" GST_PTR_FORMAT,
      GST_DEBUG_PAD_NAME (pad), chain, factories->n_values, caps);

  error_details = g_string_new ("");

  /* 2. Try to create an element and link to it */
  while (factories->n_values > 0) {
    GstAutoplugSelectResult ret;
    GstElementFactory *factory;
    GstParseElement *pelem;
    GstElement *element;
    GstPad *sinkpad;
    GParamSpec *pspec;
    gboolean subtitle;
    GList *to_connect = NULL;
    gboolean is_parser_converter = FALSE, is_simple_demuxer = FALSE;

    /* Set parsepad target to pad again, it might've been unset
     * below but we came back here because something failed
     */
    parse_pad_set_target (parsepad, pad);

    /* take first factory */
    factory = g_value_get_object (g_value_array_get_nth (factories, 0));
    /* Remove selected factory from the list. */
    g_value_array_remove (factories, 0);

    GST_LOG_OBJECT (src, "trying factory %" GST_PTR_FORMAT, factory);

    /* Check if the caps are really supported by the factory. The
     * factory list is non-empty-subset filtered while caps
     * are only accepted by a pad if they are a subset of the
     * pad caps.
     *
     * FIXME: Only do this for fixed caps here. Non-fixed caps
     * can happen if a Parser/Converter was autoplugged before
     * this. We then assume that it will be able to convert to
     * everything that the parser would want.
     *
     * A subset check will fail here because the parser caps
     * will be generic and while the parser will only
     * support a subset of the parser caps.
     */
    if (gst_caps_is_fixed (caps)) {
      const GList *templs;
      gboolean skip = FALSE;

      templs = gst_element_factory_get_static_pad_templates (factory);

      while (templs) {
        GstStaticPadTemplate *templ = (GstStaticPadTemplate *) templs->data;

        if (templ->direction == GST_PAD_SINK) {
          GstCaps *templcaps = gst_static_caps_get (&templ->static_caps);

          if (!gst_caps_is_subset (caps, templcaps)) {
            GST_DEBUG_OBJECT (src,
                "caps %" GST_PTR_FORMAT " not subset of %" GST_PTR_FORMAT, caps,
                templcaps);
            gst_caps_unref (templcaps);
            skip = TRUE;
            break;
          }

          gst_caps_unref (templcaps);
        }
        templs = g_list_next (templs);
      }
      if (skip)
        continue;
    }

    /* If the factory is for a parser we first check if the factory
     * was already used for the current chain. If it was used already
     * we would otherwise create an infinite loop here because the
     * parser apparently accepts its own output as input.
     * This is only done for parsers because it's perfectly valid
     * to have other element classes after each other because a
     * parser is the only one that does not change the data. A
     * valid example for this would be multiple id3demux in a row.
     */
    is_parser_converter = strstr (gst_element_factory_get_metadata (factory,
            GST_ELEMENT_METADATA_KLASS), "Parser") != NULL;
    is_simple_demuxer = is_simple_demuxer_factory (factory);

    if (is_parser_converter) {
      gboolean skip = FALSE;
      GList *l;

      CHAIN_MUTEX_LOCK (chain);
      for (l = chain->elements; l; l = l->next) {
        GstParseElement *pelem = (GstParseElement *) l->data;
        GstElement *otherelement = pelem->element;

        if (gst_element_get_factory (otherelement) == factory) {
          skip = TRUE;
          break;
        }
      }

      if (!skip && chain->parent && chain->parent->parent) {
        GstParseChain *parent_chain = chain->parent->parent;
        GstParseElement *pelem =
            parent_chain->elements ? parent_chain->elements->data : NULL;

        if (pelem && gst_element_get_factory (pelem->element) == factory)
          skip = TRUE;
      }
      CHAIN_MUTEX_UNLOCK (chain);
      if (skip) {
        GST_DEBUG_OBJECT (parsebin,
            "Skipping factory '%s' because it was already used in this chain",
            gst_plugin_feature_get_name (GST_PLUGIN_FEATURE_CAST (factory)));
        continue;
      }

    }

    /* Expose pads if the next factory is a decoder. segment_format might be not
     * time for subtitle streams */
    if (gst_element_factory_list_is_type (factory,
            GST_ELEMENT_FACTORY_TYPE_DECODER)
        && segment_format == GST_FORMAT_TIME) {
      ret = GST_AUTOPLUG_SELECT_EXPOSE;
    } else {
      /* emit autoplug-select to see what we should do with it. */
      g_signal_emit (G_OBJECT (parsebin),
          gst_parse_bin_signals[SIGNAL_AUTOPLUG_SELECT],
          0, parsepad, caps, factory, &ret);
    }

    switch (ret) {
      case GST_AUTOPLUG_SELECT_TRY:
        GST_DEBUG_OBJECT (parsebin, "autoplug select requested try");
        break;
      case GST_AUTOPLUG_SELECT_EXPOSE:
        GST_DEBUG_OBJECT (parsebin, "autoplug select requested expose");
        /* expose the pad, we don't have the source element */
        expose_pad (parsebin, src, parsepad, pad, caps, chain);
        res = TRUE;
        goto beach;
      case GST_AUTOPLUG_SELECT_SKIP:
        GST_DEBUG_OBJECT (parsebin, "autoplug select requested skip");
        continue;
      default:
        GST_WARNING_OBJECT (parsebin, "autoplug select returned unhandled %d",
            ret);
        break;
    }

    /* 2.0. Unlink pad */
    parse_pad_set_target (parsepad, NULL);

    /* 2.1. Try to create an element */
    if ((element = gst_element_factory_create (factory, NULL)) == NULL) {
      GST_WARNING_OBJECT (parsebin, "Could not create an element from %s",
          gst_plugin_feature_get_name (GST_PLUGIN_FEATURE (factory)));
      g_string_append_printf (error_details,
          "Could not create an element from %s\n",
          gst_plugin_feature_get_name (GST_PLUGIN_FEATURE (factory)));
      continue;
    }

    /* Filter errors, this will prevent the element from causing the pipeline
     * to error while we test it using READY state. */
    add_error_filter (parsebin, element);

    /* We don't yet want the bin to control the element's state */
    gst_element_set_locked_state (element, TRUE);

    /* ... add it ... */
    if (!(gst_bin_add (GST_BIN_CAST (parsebin), element))) {
      GST_WARNING_OBJECT (parsebin, "Couldn't add %s to the bin",
          GST_ELEMENT_NAME (element));
      remove_error_filter (parsebin, element, NULL);
      g_string_append_printf (error_details, "Couldn't add %s to the bin\n",
          GST_ELEMENT_NAME (element));
      gst_object_unref (element);
      continue;
    }

    /* Find its sink pad. */
    sinkpad = NULL;
    GST_OBJECT_LOCK (element);
    if (element->sinkpads != NULL)
      sinkpad = gst_object_ref (element->sinkpads->data);
    GST_OBJECT_UNLOCK (element);

    if (sinkpad == NULL) {
      GST_WARNING_OBJECT (parsebin, "Element %s doesn't have a sink pad",
          GST_ELEMENT_NAME (element));
      remove_error_filter (parsebin, element, NULL);
      g_string_append_printf (error_details,
          "Element %s doesn't have a sink pad", GST_ELEMENT_NAME (element));
      gst_bin_remove (GST_BIN (parsebin), element);
      continue;
    }

    /* ... and try to link */
    if ((gst_pad_link_full (pad, sinkpad,
                GST_PAD_LINK_CHECK_NOTHING)) != GST_PAD_LINK_OK) {
      GST_WARNING_OBJECT (parsebin, "Link failed on pad %s:%s",
          GST_DEBUG_PAD_NAME (sinkpad));
      remove_error_filter (parsebin, element, NULL);
      g_string_append_printf (error_details, "Link failed on pad %s:%s",
          GST_DEBUG_PAD_NAME (sinkpad));
      gst_object_unref (sinkpad);
      gst_bin_remove (GST_BIN (parsebin), element);
      continue;
    }

    /* ... activate it ... */
    if ((gst_element_set_state (element,
                GST_STATE_READY)) == GST_STATE_CHANGE_FAILURE) {
      GstMessage *error_msg;

      GST_WARNING_OBJECT (parsebin, "Couldn't set %s to READY",
          GST_ELEMENT_NAME (element));
      remove_error_filter (parsebin, element, &error_msg);

      if (error_msg) {
        gchar *error_string = error_message_to_string (error_msg);
        g_string_append_printf (error_details, "Couldn't set %s to READY:\n%s",
            GST_ELEMENT_NAME (element), error_string);
        gst_message_unref (error_msg);
        g_free (error_string);
      } else {
        g_string_append_printf (error_details, "Couldn't set %s to READY",
            GST_ELEMENT_NAME (element));
      }
      gst_object_unref (sinkpad);
      gst_bin_remove (GST_BIN (parsebin), element);
      continue;
    }

    /* check if we still accept the caps on the pad after setting
     * the element to READY */
    if (!gst_pad_query_accept_caps (sinkpad, caps)) {
      GstMessage *error_msg;

      GST_WARNING_OBJECT (parsebin, "Element %s does not accept caps",
          GST_ELEMENT_NAME (element));

      remove_error_filter (parsebin, element, &error_msg);

      if (error_msg) {
        gchar *error_string = error_message_to_string (error_msg);
        g_string_append_printf (error_details,
            "Element %s does not accept caps:\n%s", GST_ELEMENT_NAME (element),
            error_string);
        gst_message_unref (error_msg);
        g_free (error_string);
      } else {
        g_string_append_printf (error_details,
            "Element %s does not accept caps", GST_ELEMENT_NAME (element));
      }

      gst_element_set_state (element, GST_STATE_NULL);
      gst_object_unref (sinkpad);
      gst_bin_remove (GST_BIN (parsebin), element);
      continue;
    }

    gst_object_unref (sinkpad);
    GST_LOG_OBJECT (parsebin, "linked on pad %s:%s", GST_DEBUG_PAD_NAME (pad));

    CHAIN_MUTEX_LOCK (chain);
    pelem = g_new0 (GstParseElement, 1);
    pelem->element = gst_object_ref (element);
    pelem->capsfilter = NULL;
    chain->elements = g_list_prepend (chain->elements, pelem);
    chain->demuxer = is_demuxer_element (element);

    /* If we plugging a parser, mark the chain as parsed */
    chain->parsed |= is_parser_converter;

    CHAIN_MUTEX_UNLOCK (chain);

    /* Set connection-speed property if needed */
    if (chain->demuxer) {
      GParamSpec *pspec;

      if ((pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (element),
                  "connection-speed"))) {
        guint64 speed = parsebin->connection_speed / 1000;
        gboolean wrong_type = FALSE;

        if (G_PARAM_SPEC_TYPE (pspec) == G_TYPE_PARAM_UINT) {
          GParamSpecUInt *pspecuint = G_PARAM_SPEC_UINT (pspec);

          speed = CLAMP (speed, pspecuint->minimum, pspecuint->maximum);
        } else if (G_PARAM_SPEC_TYPE (pspec) == G_TYPE_PARAM_INT) {
          GParamSpecInt *pspecint = G_PARAM_SPEC_INT (pspec);

          speed = CLAMP (speed, pspecint->minimum, pspecint->maximum);
        } else if (G_PARAM_SPEC_TYPE (pspec) == G_TYPE_PARAM_UINT64) {
          GParamSpecUInt64 *pspecuint = G_PARAM_SPEC_UINT64 (pspec);

          speed = CLAMP (speed, pspecuint->minimum, pspecuint->maximum);
        } else if (G_PARAM_SPEC_TYPE (pspec) == G_TYPE_PARAM_INT64) {
          GParamSpecInt64 *pspecint = G_PARAM_SPEC_INT64 (pspec);

          speed = CLAMP (speed, pspecint->minimum, pspecint->maximum);
        } else {
          GST_WARNING_OBJECT (parsebin,
              "The connection speed property %" G_GUINT64_FORMAT " of type %s"
              " is not useful not setting it", speed,
              g_type_name (G_PARAM_SPEC_TYPE (pspec)));
          wrong_type = TRUE;
        }

        if (!wrong_type) {
          GST_DEBUG_OBJECT (parsebin,
              "setting connection-speed=%" G_GUINT64_FORMAT
              " to demuxer element", speed);

          g_object_set (element, "connection-speed", speed, NULL);
        }
      }
    }

    /* try to configure the subtitle encoding property when we can */
    pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (element),
        "subtitle-encoding");
    if (pspec && G_PARAM_SPEC_VALUE_TYPE (pspec) == G_TYPE_STRING) {
      SUBTITLE_LOCK (parsebin);
      GST_DEBUG_OBJECT (parsebin,
          "setting subtitle-encoding=%s to element", parsebin->encoding);
      g_object_set (G_OBJECT (element), "subtitle-encoding", parsebin->encoding,
          NULL);
      SUBTITLE_UNLOCK (parsebin);
      subtitle = TRUE;
    } else {
      subtitle = FALSE;
    }

    /* link this element further */
    to_connect = connect_element (parsebin, pelem, chain);

    if ((is_simple_demuxer || is_parser_converter) && to_connect) {
      GList *l;
      for (l = to_connect; l; l = g_list_next (l)) {
        GstPad *opad = GST_PAD_CAST (l->data);
        GstCaps *ocaps;

        ocaps = get_pad_caps (opad);
        analyze_new_pad (parsebin, pelem->element, opad, ocaps, chain);
        if (ocaps)
          gst_caps_unref (ocaps);

        gst_object_unref (opad);
      }
      g_list_free (to_connect);
      to_connect = NULL;
    }

    /* Bring the element to the state of the parent */

    /* First lock element's sinkpad stream lock so no data reaches
     * the possible new element added when caps are sent by element
     * while we're still sending sticky events */
    GST_PAD_STREAM_LOCK (sinkpad);

    if ((gst_element_set_state (element,
                GST_STATE_PAUSED)) == GST_STATE_CHANGE_FAILURE) {
      GstParseElement *dtmp = NULL;
      GstElement *tmp = NULL;
      GstMessage *error_msg;

      GST_PAD_STREAM_UNLOCK (sinkpad);

      GST_WARNING_OBJECT (parsebin, "Couldn't set %s to PAUSED",
          GST_ELEMENT_NAME (element));

      g_list_free_full (to_connect, (GDestroyNotify) gst_object_unref);
      to_connect = NULL;

      remove_error_filter (parsebin, element, &error_msg);

      if (error_msg) {
        gchar *error_string = error_message_to_string (error_msg);
        g_string_append_printf (error_details, "Couldn't set %s to PAUSED:\n%s",
            GST_ELEMENT_NAME (element), error_string);
        gst_message_unref (error_msg);
        g_free (error_string);
      } else {
        g_string_append_printf (error_details, "Couldn't set %s to PAUSED",
            GST_ELEMENT_NAME (element));
      }

      /* Remove all elements in this chain that were just added. No
       * other thread could've added elements in the meantime */
      CHAIN_MUTEX_LOCK (chain);
      do {
        GList *l;

        dtmp = chain->elements->data;
        tmp = dtmp->element;

        /* Disconnect any signal handlers that might be connected
         * in connect_element() or analyze_pad() */
        if (dtmp->pad_added_id)
          g_signal_handler_disconnect (tmp, dtmp->pad_added_id);
        if (dtmp->pad_removed_id)
          g_signal_handler_disconnect (tmp, dtmp->pad_removed_id);
        if (dtmp->no_more_pads_id)
          g_signal_handler_disconnect (tmp, dtmp->no_more_pads_id);

        for (l = chain->pending_pads; l;) {
          GstPendingPad *pp = l->data;
          GList *n;

          if (GST_PAD_PARENT (pp->pad) != tmp) {
            l = l->next;
            continue;
          }

          gst_pending_pad_free (pp);

          /* Remove element from the list, update list head and go to the
           * next element in the list */
          n = l->next;
          chain->pending_pads = g_list_delete_link (chain->pending_pads, l);
          l = n;
        }

        if (dtmp->capsfilter) {
          gst_bin_remove (GST_BIN (parsebin), dtmp->capsfilter);
          gst_element_set_state (dtmp->capsfilter, GST_STATE_NULL);
          gst_object_unref (dtmp->capsfilter);
        }

        gst_bin_remove (GST_BIN (parsebin), tmp);
        gst_element_set_state (tmp, GST_STATE_NULL);

        gst_object_unref (tmp);
        g_free (dtmp);

        chain->elements = g_list_delete_link (chain->elements, chain->elements);
      } while (tmp != element);
      CHAIN_MUTEX_UNLOCK (chain);

      continue;
    } else {
      send_sticky_events (parsebin, pad);
      /* Everything went well, the spice must flow now */
      GST_PAD_STREAM_UNLOCK (sinkpad);
    }

    /* Remove error filter now, from now on we can't gracefully
     * handle errors of the element anymore */
    remove_error_filter (parsebin, element, NULL);

    /* Now let the bin handle the state */
    gst_element_set_locked_state (element, FALSE);

    if (subtitle) {
      SUBTITLE_LOCK (parsebin);
      /* we added the element now, add it to the list of subtitle-encoding
       * elements when we can set the property */
      parsebin->subtitles = g_list_prepend (parsebin->subtitles, element);
      SUBTITLE_UNLOCK (parsebin);
    }

    if (to_connect) {
      GList *l;
      for (l = to_connect; l; l = g_list_next (l)) {
        GstPad *opad = GST_PAD_CAST (l->data);
        GstCaps *ocaps;

        ocaps = get_pad_caps (opad);
        analyze_new_pad (parsebin, pelem->element, opad, ocaps, chain);
        if (ocaps)
          gst_caps_unref (ocaps);

        gst_object_unref (opad);
      }
      g_list_free (to_connect);
      to_connect = NULL;
    }

    res = TRUE;
    break;
  }

beach:
  if (error_details)
    *deadend_details = g_string_free (error_details, (error_details->len == 0
            || res));
  else
    *deadend_details = NULL;

  return res;
}

static GstCaps *
get_pad_caps (GstPad * pad)
{
  GstCaps *caps;

  /* first check the pad caps, if this is set, we are positively sure it is
   * fixed and exactly what the element will produce. */
  caps = gst_pad_get_current_caps (pad);

  /* then use the getcaps function if we don't have caps. These caps might not
   * be fixed in some cases, in which case analyze_new_pad will set up a
   * notify::caps signal to continue autoplugging. */
  if (caps == NULL)
    caps = gst_pad_query_caps (pad, NULL);

  return caps;
}

/* Returns a list of pads that can be connected to already and
 * connects to pad-added and related signals */
static GList *
connect_element (GstParseBin * parsebin, GstParseElement * pelem,
    GstParseChain * chain)
{
  GstElement *element = pelem->element;
  GList *pads;
  gboolean dynamic = FALSE;
  GList *to_connect = NULL;

  GST_DEBUG_OBJECT (parsebin,
      "Attempting to connect element %s [chain:%p] further",
      GST_ELEMENT_NAME (element), chain);

  /* 1. Loop over pad templates, grabbing existing pads along the way */
  for (pads = GST_ELEMENT_GET_CLASS (element)->padtemplates; pads;
      pads = g_list_next (pads)) {
    GstPadTemplate *templ = GST_PAD_TEMPLATE (pads->data);
    const gchar *templ_name;

    /* we are only interested in source pads */
    if (GST_PAD_TEMPLATE_DIRECTION (templ) != GST_PAD_SRC)
      continue;

    templ_name = GST_PAD_TEMPLATE_NAME_TEMPLATE (templ);
    GST_DEBUG_OBJECT (parsebin, "got a source pad template %s", templ_name);

    /* figure out what kind of pad this is */
    switch (GST_PAD_TEMPLATE_PRESENCE (templ)) {
      case GST_PAD_ALWAYS:
      {
        /* get the pad that we need to autoplug */
        GstPad *pad = gst_element_get_static_pad (element, templ_name);

        if (pad) {
          GST_DEBUG_OBJECT (parsebin, "got the pad for always template %s",
              templ_name);
          /* here is the pad, we need to autoplug it */
          to_connect = g_list_prepend (to_connect, pad);
        } else {
          /* strange, pad is marked as always but it's not
           * there. Fix the element */
          GST_WARNING_OBJECT (parsebin,
              "could not get the pad for always template %s", templ_name);
        }
        break;
      }
      case GST_PAD_SOMETIMES:
      {
        /* try to get the pad to see if it is already created or
         * not */
        GstPad *pad = gst_element_get_static_pad (element, templ_name);

        if (pad) {
          GST_DEBUG_OBJECT (parsebin, "got the pad for sometimes template %s",
              templ_name);
          /* the pad is created, we need to autoplug it */
          to_connect = g_list_prepend (to_connect, pad);
        } else {
          GST_DEBUG_OBJECT (parsebin,
              "did not get the sometimes pad of template %s", templ_name);
          /* we have an element that will create dynamic pads */
          dynamic = TRUE;
        }
        break;
      }
      case GST_PAD_REQUEST:
        /* ignore request pads */
        GST_DEBUG_OBJECT (parsebin, "ignoring request padtemplate %s",
            templ_name);
        break;
    }
  }

  /* 2. if there are more potential pads, connect to relevant signals */
  if (dynamic) {
    GST_LOG_OBJECT (parsebin, "Adding signals to element %s in chain %p",
        GST_ELEMENT_NAME (element), chain);
    pelem->pad_added_id = g_signal_connect (element, "pad-added",
        G_CALLBACK (pad_added_cb), chain);
    pelem->pad_removed_id = g_signal_connect (element, "pad-removed",
        G_CALLBACK (pad_removed_cb), chain);
    pelem->no_more_pads_id = g_signal_connect (element, "no-more-pads",
        G_CALLBACK (no_more_pads_cb), chain);
  }

  /* 3. return all pads that can be connected to already */

  return to_connect;
}

/* expose_pad:
 *
 * Expose the given pad on the chain as a parsed pad.
 */
static void
expose_pad (GstParseBin * parsebin, GstElement * src, GstParsePad * parsepad,
    GstPad * pad, GstCaps * caps, GstParseChain * chain)
{
  GST_DEBUG_OBJECT (parsebin, "pad %s:%s, chain:%p",
      GST_DEBUG_PAD_NAME (pad), chain);

  gst_parse_pad_activate (parsepad, chain);
  chain->endpad = gst_object_ref (parsepad);
  if (caps)
    chain->endcaps = gst_caps_ref (caps);
  else
    chain->endcaps = NULL;
}

static void
type_found (GstElement * typefind, guint probability,
    GstCaps * caps, GstParseBin * parse_bin)
{
  GstPad *pad, *sink_pad;

  GST_DEBUG_OBJECT (parse_bin, "typefind found caps %" GST_PTR_FORMAT, caps);

  /* If the typefinder (but not something else) finds text/plain - i.e. that's
   * the top-level type of the file - then error out.
   */
  if (gst_structure_has_name (gst_caps_get_structure (caps, 0), "text/plain")) {
    GST_ELEMENT_ERROR (parse_bin, STREAM, WRONG_TYPE,
        (_("This appears to be a text file")),
        ("ParseBin cannot parse plain text files"));
    goto exit;
  }

  /* FIXME: we can only deal with one type, we don't yet support dynamically changing
   * caps from the typefind element */
  if (parse_bin->have_type || parse_bin->parse_chain)
    goto exit;

  parse_bin->have_type = TRUE;

  pad = gst_element_get_static_pad (typefind, "src");
  sink_pad = gst_element_get_static_pad (typefind, "sink");

  /* need some lock here to prevent race with shutdown state change
   * which might yank away e.g. parse_chain while building stuff here.
   * In typical cases, STREAM_LOCK is held and handles that, it need not
   * be held (if called from a proxied setcaps), so grab it anyway */
  GST_PAD_STREAM_LOCK (sink_pad);
  parse_bin->parse_chain = gst_parse_chain_new (parse_bin, NULL, pad, caps);
  analyze_new_pad (parse_bin, typefind, pad, caps, parse_bin->parse_chain);
  GST_PAD_STREAM_UNLOCK (sink_pad);

  gst_object_unref (sink_pad);
  gst_object_unref (pad);

exit:
  return;
}

static GstPadProbeReturn
pad_event_cb (GstPad * pad, GstPadProbeInfo * info, gpointer data)
{
  GstEvent *event = GST_PAD_PROBE_INFO_EVENT (info);
  GstPendingPad *ppad = (GstPendingPad *) data;
  GstParseChain *chain = ppad->chain;
  GstParseBin *parsebin = chain->parsebin;

  g_assert (ppad);
  g_assert (chain);
  g_assert (parsebin);
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_EOS:
      GST_DEBUG_OBJECT (pad, "Received EOS on a non final pad, this stream "
          "ended too early");
      chain->deadend = TRUE;
      chain->drained = TRUE;
      gst_object_replace ((GstObject **) & chain->current_pad, NULL);
      /* we don't set the endcaps because NULL endcaps means early EOS */

      EXPOSE_LOCK (parsebin);
      if (parsebin->parse_chain)
        if (gst_parse_chain_is_complete (parsebin->parse_chain))
          gst_parse_bin_expose (parsebin);
      EXPOSE_UNLOCK (parsebin);
      break;
    default:
      break;
  }
  return GST_PAD_PROBE_OK;
}

static void
pad_added_cb (GstElement * element, GstPad * pad, GstParseChain * chain)
{
  GstCaps *caps;
  GstParseBin *parsebin;

  parsebin = chain->parsebin;

  GST_DEBUG_OBJECT (pad, "pad added, chain:%p", chain);

  caps = get_pad_caps (pad);
  analyze_new_pad (parsebin, element, pad, caps, chain);
  if (caps)
    gst_caps_unref (caps);

  EXPOSE_LOCK (parsebin);
  if (parsebin->parse_chain) {
    if (gst_parse_chain_is_complete (parsebin->parse_chain)) {
      GST_LOG_OBJECT (parsebin,
          "That was the last dynamic object, now attempting to expose the group");
      if (!gst_parse_bin_expose (parsebin))
        GST_WARNING_OBJECT (parsebin, "Couldn't expose group");
    }
  } else {
    GST_DEBUG_OBJECT (parsebin, "No parse chain, new pad ignored");
  }
  EXPOSE_UNLOCK (parsebin);
}

static void
pad_removed_cb (GstElement * element, GstPad * pad, GstParseChain * chain)
{
  GList *l;

  GST_LOG_OBJECT (pad, "pad removed, chain:%p", chain);

  /* In fact, we don't have to do anything here, the active group will be
   * removed when the group's multiqueue is drained */
  CHAIN_MUTEX_LOCK (chain);
  for (l = chain->pending_pads; l; l = l->next) {
    GstPendingPad *ppad = l->data;
    GstPad *opad = ppad->pad;

    if (pad == opad) {
      gst_pending_pad_free (ppad);
      chain->pending_pads = g_list_delete_link (chain->pending_pads, l);
      break;
    }
  }
  CHAIN_MUTEX_UNLOCK (chain);
}

static void
no_more_pads_cb (GstElement * element, GstParseChain * chain)
{
  GstParseGroup *group = NULL;

  GST_LOG_OBJECT (element, "got no more pads");

  CHAIN_MUTEX_LOCK (chain);
  if (!chain->elements
      || ((GstParseElement *) chain->elements->data)->element != element) {
    GST_LOG_OBJECT (chain->parsebin, "no-more-pads from old chain element '%s'",
        GST_OBJECT_NAME (element));
    CHAIN_MUTEX_UNLOCK (chain);
    return;
  } else if (!chain->demuxer) {
    GST_LOG_OBJECT (chain->parsebin,
        "no-more-pads from a non-demuxer element '%s'",
        GST_OBJECT_NAME (element));
    CHAIN_MUTEX_UNLOCK (chain);
    return;
  }

  /* when we received no_more_pads, we can complete the pads of the chain */
  if (!chain->next_groups && chain->active_group) {
    group = chain->active_group;
  } else if (chain->next_groups) {
    GList *iter;
    for (iter = chain->next_groups; iter; iter = g_list_next (iter)) {
      group = iter->data;
      if (!group->no_more_pads)
        break;
    }
  }
  if (!group) {
    GST_ERROR_OBJECT (chain->parsebin, "can't find group for element");
    CHAIN_MUTEX_UNLOCK (chain);
    return;
  }

  GST_DEBUG_OBJECT (element, "Setting group %p to complete", group);

  group->no_more_pads = TRUE;
  CHAIN_MUTEX_UNLOCK (chain);

  EXPOSE_LOCK (chain->parsebin);
  if (chain->parsebin->parse_chain) {
    if (gst_parse_chain_is_complete (chain->parsebin->parse_chain)) {
      gst_parse_bin_expose (chain->parsebin);
    }
  }
  EXPOSE_UNLOCK (chain->parsebin);
}

static void
caps_notify_cb (GstPad * pad, GParamSpec * unused, GstParseChain * chain)
{
  GstElement *element;
  GList *l;

  GST_LOG_OBJECT (pad, "Notified caps for pad %s:%s", GST_DEBUG_PAD_NAME (pad));

  /* Disconnect this; if we still need it, we'll reconnect to this in
   * analyze_new_pad */
  element = GST_ELEMENT_CAST (gst_pad_get_parent (pad));

  CHAIN_MUTEX_LOCK (chain);
  for (l = chain->pending_pads; l; l = l->next) {
    GstPendingPad *ppad = l->data;
    if (ppad->pad == pad) {
      gst_pending_pad_free (ppad);
      chain->pending_pads = g_list_delete_link (chain->pending_pads, l);
      break;
    }
  }
  CHAIN_MUTEX_UNLOCK (chain);

  pad_added_cb (element, pad, chain);

  gst_object_unref (element);
}

/* Decide whether an element is a demuxer based on the
 * klass and number/type of src pad templates it has */
static gboolean
is_demuxer_element (GstElement * srcelement)
{
  GstElementFactory *srcfactory;
  GstElementClass *elemclass;
  GList *walk;
  const gchar *klass;
  gint potential_src_pads = 0;

  srcfactory = gst_element_get_factory (srcelement);
  klass =
      gst_element_factory_get_metadata (srcfactory, GST_ELEMENT_METADATA_KLASS);

  /* Can't be a demuxer unless it has Demux in the klass name */
  if (!strstr (klass, "Demux"))
    return FALSE;

  /* Walk the src pad templates and count how many the element
   * might produce */
  elemclass = GST_ELEMENT_GET_CLASS (srcelement);

  walk = gst_element_class_get_pad_template_list (elemclass);
  while (walk != NULL) {
    GstPadTemplate *templ;

    templ = (GstPadTemplate *) walk->data;
    if (GST_PAD_TEMPLATE_DIRECTION (templ) == GST_PAD_SRC) {
      switch (GST_PAD_TEMPLATE_PRESENCE (templ)) {
        case GST_PAD_ALWAYS:
        case GST_PAD_SOMETIMES:
          if (strstr (GST_PAD_TEMPLATE_NAME_TEMPLATE (templ), "%"))
            potential_src_pads += 2;    /* Might make multiple pads */
          else
            potential_src_pads += 1;
          break;
        case GST_PAD_REQUEST:
          potential_src_pads += 2;
          break;
      }
    }
    walk = g_list_next (walk);
  }

  if (potential_src_pads < 2)
    return FALSE;

  return TRUE;
}

/* gst_parse_chain_get_current_group:
 *
 * Returns the current group of this chain, to which
 * new chains should be attached or NULL if the last
 * group didn't have no-more-pads.
 *
 * Not MT-safe: Call with parent chain lock!
 */
static GstParseGroup *
gst_parse_chain_get_current_group (GstParseChain * chain)
{
  GstParseGroup *group;

  /* Now we know that we can really return something useful */
  if (!chain->active_group) {
    chain->active_group = group = gst_parse_group_new (chain->parsebin, chain);
  } else if (!chain->active_group->no_more_pads) {
    group = chain->active_group;
  } else {
    GList *iter;
    group = NULL;
    for (iter = chain->next_groups; iter; iter = g_list_next (iter)) {
      GstParseGroup *next_group = iter->data;

      if (!next_group->no_more_pads) {
        group = next_group;
        break;
      }
    }
  }
  if (!group) {
    group = gst_parse_group_new (chain->parsebin, chain);
    chain->next_groups = g_list_append (chain->next_groups, group);
  }

  return group;
}

static void gst_parse_group_free_internal (GstParseGroup * group,
    gboolean hide);

static void
gst_parse_chain_free_internal (GstParseChain * chain, gboolean hide)
{
  GList *l, *set_to_null = NULL;

  CHAIN_MUTEX_LOCK (chain);

  GST_DEBUG_OBJECT (chain->parsebin, "%s chain %p",
      (hide ? "Hiding" : "Freeing"), chain);

  if (chain->active_group) {
    gst_parse_group_free_internal (chain->active_group, hide);
    if (!hide)
      chain->active_group = NULL;
  }

  for (l = chain->next_groups; l; l = l->next) {
    gst_parse_group_free_internal ((GstParseGroup *) l->data, hide);
    if (!hide)
      l->data = NULL;
  }
  if (!hide) {
    g_list_free (chain->next_groups);
    chain->next_groups = NULL;
  }

  if (!hide) {
    for (l = chain->old_groups; l; l = l->next) {
      GstParseGroup *group = l->data;

      gst_parse_group_free (group);
    }
    g_list_free (chain->old_groups);
    chain->old_groups = NULL;
  }

  gst_object_replace ((GstObject **) & chain->current_pad, NULL);

  for (l = chain->pending_pads; l; l = l->next) {
    GstPendingPad *ppad = l->data;
    gst_pending_pad_free (ppad);
    l->data = NULL;
  }
  g_list_free (chain->pending_pads);
  chain->pending_pads = NULL;

  for (l = chain->elements; l; l = l->next) {
    GstParseElement *pelem = l->data;
    GstElement *element = pelem->element;

    if (pelem->pad_added_id)
      g_signal_handler_disconnect (element, pelem->pad_added_id);
    pelem->pad_added_id = 0;
    if (pelem->pad_removed_id)
      g_signal_handler_disconnect (element, pelem->pad_removed_id);
    pelem->pad_removed_id = 0;
    if (pelem->no_more_pads_id)
      g_signal_handler_disconnect (element, pelem->no_more_pads_id);
    pelem->no_more_pads_id = 0;

    if (pelem->capsfilter) {
      if (GST_OBJECT_PARENT (pelem->capsfilter) ==
          GST_OBJECT_CAST (chain->parsebin))
        gst_bin_remove (GST_BIN_CAST (chain->parsebin), pelem->capsfilter);
      if (!hide) {
        set_to_null =
            g_list_append (set_to_null, gst_object_ref (pelem->capsfilter));
      }
    }

    if (GST_OBJECT_PARENT (element) == GST_OBJECT_CAST (chain->parsebin))
      gst_bin_remove (GST_BIN_CAST (chain->parsebin), element);
    if (!hide) {
      set_to_null = g_list_append (set_to_null, gst_object_ref (element));
    }

    SUBTITLE_LOCK (chain->parsebin);
    /* remove possible subtitle element */
    chain->parsebin->subtitles =
        g_list_remove (chain->parsebin->subtitles, element);
    SUBTITLE_UNLOCK (chain->parsebin);

    if (!hide) {
      if (pelem->capsfilter) {
        gst_object_unref (pelem->capsfilter);
        pelem->capsfilter = NULL;
      }

      gst_object_unref (element);
      l->data = NULL;

      g_free (pelem);
    }
  }
  if (!hide) {
    g_list_free (chain->elements);
    chain->elements = NULL;
  }

  if (chain->endpad) {
    if (chain->endpad->exposed) {
      GstPad *endpad = GST_PAD_CAST (chain->endpad);
      GST_DEBUG_OBJECT (chain->parsebin, "Removing pad %s:%s",
          GST_DEBUG_PAD_NAME (endpad));
      gst_pad_push_event (endpad, gst_event_new_eos ());
      gst_element_remove_pad (GST_ELEMENT_CAST (chain->parsebin), endpad);
    }

    parse_pad_set_target (chain->endpad, NULL);
    chain->endpad->exposed = FALSE;
    if (!hide) {
      gst_object_unref (chain->endpad);
      chain->endpad = NULL;
    }
  }

  if (!hide && chain->current_pad) {
    gst_object_unref (chain->current_pad);
    chain->current_pad = NULL;
  }

  if (chain->pad) {
    gst_object_unref (chain->pad);
    chain->pad = NULL;
  }
  if (chain->start_caps) {
    gst_caps_unref (chain->start_caps);
    chain->start_caps = NULL;
  }

  if (chain->endcaps) {
    gst_caps_unref (chain->endcaps);
    chain->endcaps = NULL;
  }
  g_free (chain->deadend_details);
  chain->deadend_details = NULL;

  GST_DEBUG_OBJECT (chain->parsebin, "%s chain %p", (hide ? "Hidden" : "Freed"),
      chain);
  CHAIN_MUTEX_UNLOCK (chain);

  while (set_to_null) {
    GstElement *element = set_to_null->data;
    set_to_null = g_list_delete_link (set_to_null, set_to_null);
    gst_element_set_state (element, GST_STATE_NULL);
    gst_object_unref (element);
  }

  if (!hide) {
    g_mutex_clear (&chain->lock);
    g_free (chain);
  }
}

/* gst_parse_chain_free:
 *
 * Completely frees and removes the chain and all
 * child groups from ParseBin.
 *
 * MT-safe, don't hold the chain lock or any child chain's lock
 * when calling this!
 */
static void
gst_parse_chain_free (GstParseChain * chain)
{
  gst_parse_chain_free_internal (chain, FALSE);
}

/* gst_parse_chain_new:
 *
 * Creates a new parse chain and initializes it.
 *
 * It's up to the caller to add it to the list of child chains of
 * a group!
 */
static GstParseChain *
gst_parse_chain_new (GstParseBin * parsebin, GstParseGroup * parent,
    GstPad * pad, GstCaps * start_caps)
{
  GstParseChain *chain = g_new0 (GstParseChain, 1);

  GST_DEBUG_OBJECT (parsebin, "Creating new chain %p with parent group %p",
      chain, parent);

  chain->parsebin = parsebin;
  chain->parent = parent;
  g_mutex_init (&chain->lock);
  chain->pad = gst_object_ref (pad);
  if (start_caps)
    chain->start_caps = gst_caps_ref (start_caps);

  return chain;
}

/****
 * GstParseGroup functions
 ****/

static void
gst_parse_group_free_internal (GstParseGroup * group, gboolean hide)
{
  GList *l;

  GST_DEBUG_OBJECT (group->parsebin, "%s group %p",
      (hide ? "Hiding" : "Freeing"), group);
  for (l = group->children; l; l = l->next) {
    GstParseChain *chain = (GstParseChain *) l->data;

    gst_parse_chain_free_internal (chain, hide);
    if (!hide)
      l->data = NULL;
  }
  if (!hide) {
    g_list_free (group->children);
    group->children = NULL;
  }

  GST_DEBUG_OBJECT (group->parsebin, "%s group %p", (hide ? "Hid" : "Freed"),
      group);
  if (!hide)
    g_free (group);
}

/* gst_parse_group_free:
 *
 * Completely frees and removes the parse group and all
 * it's children.
 *
 * Never call this from any streaming thread!
 *
 * Not MT-safe, call with parent's chain lock!
 */
static void
gst_parse_group_free (GstParseGroup * group)
{
  gst_parse_group_free_internal (group, FALSE);
}

/* gst_parse_group_hide:
 *
 * Hide the parse group only, this means that
 * all child endpads are removed from ParseBin
 * and all signals are unconnected.
 *
 * No element is set to NULL state and completely
 * unrefed here.
 *
 * Can be called from streaming threads.
 *
 * Not MT-safe, call with parent's chain lock!
 */
static void
gst_parse_group_hide (GstParseGroup * group)
{
  gst_parse_group_free_internal (group, TRUE);
}

/* gst_parse_chain_free_hidden_groups:
 *
 * Frees any parse groups that were hidden previously.
 * This allows keeping memory use from ballooning when
 * switching chains repeatedly.
 *
 * A new throwaway thread will be created to free the
 * groups, so any delay does not block the setup of a
 * new group.
 *
 * Not MT-safe, call with parent's chain lock!
 */
static void
gst_parse_chain_free_hidden_groups (GList * old_groups)
{
  GList *l;

  for (l = old_groups; l; l = l->next) {
    GstParseGroup *group = l->data;

    gst_parse_group_free (group);
  }
  g_list_free (old_groups);
}

static void
gst_parse_chain_start_free_hidden_groups_thread (GstParseChain * chain)
{
  GThread *thread;
  GError *error = NULL;
  GList *old_groups;
  GstParseBin *parsebin = chain->parsebin;

  old_groups = chain->old_groups;
  if (!old_groups)
    return;

  /* If we already have a thread running, wait for it to finish */
  g_mutex_lock (&parsebin->cleanup_lock);
  if (parsebin->cleanup_thread) {
    g_thread_join (parsebin->cleanup_thread);
    parsebin->cleanup_thread = NULL;
  }

  chain->old_groups = NULL;
  thread = g_thread_try_new ("free-hidden-groups",
      (GThreadFunc) gst_parse_chain_free_hidden_groups, old_groups, &error);
  if (!thread || error) {
    GST_ERROR ("Failed to start free-hidden-groups thread: %s",
        error ? error->message : "unknown reason");
    g_clear_error (&error);
    chain->old_groups = old_groups;
    g_mutex_unlock (&parsebin->cleanup_lock);
    return;
  }

  parsebin->cleanup_thread = thread;
  g_mutex_unlock (&parsebin->cleanup_lock);

  GST_DEBUG_OBJECT (chain->parsebin, "Started free-hidden-groups thread");
}

/* gst_parse_group_new:
 * @parsebin: Parent ParseBin
 * @parent: Parent chain or %NULL
 *
 * Creates a new GstParseGroup. It is up to the caller to add it to the list
 * of groups.
 */
static GstParseGroup *
gst_parse_group_new (GstParseBin * parsebin, GstParseChain * parent)
{
  GstParseGroup *group = g_new0 (GstParseGroup, 1);

  GST_DEBUG_OBJECT (parsebin, "Creating new group %p with parent chain %p",
      group, parent);

  group->parsebin = parsebin;
  group->parent = parent;

  return group;
}

/* gst_parse_group_is_complete:
 *
 * Checks if the group is complete, this means that
 * a) no-more-pads happened
 * b) all child chains are complete
 *
 * Not MT-safe, always call with ParseBin expose lock
 */
static gboolean
gst_parse_group_is_complete (GstParseGroup * group)
{
  GList *l;
  gboolean complete = TRUE;

  if (!group->no_more_pads) {
    complete = FALSE;
    goto out;
  }

  for (l = group->children; l; l = l->next) {
    GstParseChain *chain = l->data;

    /* Any blocked chain requires we complete this group
     * since everything is synchronous, we can't proceed otherwise */
    if (chain->endpad && chain->endpad->blocked)
      goto out;

    if (!gst_parse_chain_is_complete (chain)) {
      complete = FALSE;
      goto out;
    }
  }

out:
  GST_DEBUG_OBJECT (group->parsebin, "Group %p is complete: %d", group,
      complete);
  return complete;
}

/* gst_parse_chain_is_complete:
 *
 * Returns TRUE if the chain is complete, this means either
 * a) This chain is a dead end, i.e. we have no suitable plugins
 * b) This chain ends in an endpad and this is blocked or exposed
 * c) The chain has gotten far enough to have plugged 1 parser at least.
 *
 * Not MT-safe, always call with ParseBin expose lock
 */
static gboolean
gst_parse_chain_is_complete (GstParseChain * chain)
{
  gboolean complete = FALSE;

  CHAIN_MUTEX_LOCK (chain);
  if (chain->parsebin->shutdown)
    goto out;

  if (chain->deadend) {
    complete = TRUE;
    goto out;
  }

  if (chain->endpad && (chain->endpad->blocked || chain->endpad->exposed)) {
    complete = TRUE;
    goto out;
  }

  if (chain->demuxer) {
    if (chain->active_group
        && gst_parse_group_is_complete (chain->active_group)) {
      complete = TRUE;
      goto out;
    }
  }

  if (chain->parsed) {
    complete = TRUE;
    goto out;
  }

out:
  CHAIN_MUTEX_UNLOCK (chain);
  GST_DEBUG_OBJECT (chain->parsebin, "Chain %p is complete: %d", chain,
      complete);
  return complete;
}

static void
chain_remove_old_groups (GstParseChain * chain)
{
  GList *tmp;

  /* First go in child */
  if (chain->active_group) {
    for (tmp = chain->active_group->children; tmp; tmp = tmp->next) {
      GstParseChain *child = (GstParseChain *) tmp->data;
      chain_remove_old_groups (child);
    }
  }

  if (chain->old_groups) {
    gst_parse_group_hide (chain->old_groups->data);
    gst_parse_chain_start_free_hidden_groups_thread (chain);
  }
}

static gboolean
gst_parse_chain_accept_caps (GstParseChain * chain, GstCaps * caps)
{
  GstParseElement *initial_element;
  GList *lastlist;
  GstPad *sink;
  gboolean ret;

  if (!chain->elements)
    return TRUE;

  lastlist = g_list_last (chain->elements);
  initial_element = lastlist->data;

  GST_DEBUG_OBJECT (chain->parsebin, "element %s caps %" GST_PTR_FORMAT,
      GST_ELEMENT_NAME (initial_element->element), caps);

  sink = gst_element_get_static_pad (initial_element->element, "sink");
  if (sink) {
    ret = gst_pad_query_accept_caps (sink, caps);
    gst_object_unref (sink);
  } else {
    GST_OBJECT_LOCK (initial_element->element);
    if (G_UNLIKELY (!initial_element->element->numsinkpads)) {
      GST_ERROR_OBJECT (chain->parsebin,
          "element %" GST_PTR_FORMAT " has no sink pad",
          initial_element->element);
      GST_OBJECT_UNLOCK (initial_element->element);
      return FALSE;
    }
    GstPad *pad =
        GST_PAD_CAST (gst_object_ref (initial_element->element->sinkpads));
    GST_OBJECT_UNLOCK (initial_element->element);
    GST_DEBUG_OBJECT (chain->parsebin,
        "element %" GST_PTR_FORMAT
        " doesn't have a 'sink' pad, sending accept-caps query to "
        "%" GST_PTR_FORMAT, initial_element->element, pad);
    ret = gst_pad_query_accept_caps (pad, caps);
    gst_object_unref (pad);
  }
  GST_DEBUG_OBJECT (chain->parsebin, "Chain can%s handle caps",
      ret ? "" : " NOT");

  return ret;
}

static gboolean
drain_and_switch_chains (GstParseChain * chain, GstParsePad * drainpad,
    gboolean * last_group, gboolean * drained, gboolean * switched);
/* drain_and_switch_chains/groups:
 *
 * CALL WITH CHAIN LOCK (or group parent) TAKEN !
 *
 * Goes down the chains/groups until it finds the chain
 * to which the drainpad belongs.
 *
 * It marks that pad/chain as drained and then will figure
 * out which group to switch to or not.
 *
 * last_chain will be set to TRUE if the group to which the
 * pad belongs is the last one.
 *
 * drained will be set to TRUE if the chain/group is drained.
 *
 * Returns: TRUE if the chain contained the target pad */
static gboolean
drain_and_switch_group (GstParseGroup * group, GstParsePad * drainpad,
    gboolean * last_group, gboolean * drained, gboolean * switched)
{
  gboolean handled = FALSE;
  GList *tmp;

  GST_DEBUG ("Checking group %p (target pad %s:%s)",
      group, GST_DEBUG_PAD_NAME (drainpad));

  /* Definitely can't be in drained groups */
  if (G_UNLIKELY (group->drained)) {
    goto beach;
  }

  /* Figure out if all our chains are drained with the
   * new information */
  group->drained = TRUE;
  for (tmp = group->children; tmp; tmp = tmp->next) {
    GstParseChain *chain = (GstParseChain *) tmp->data;
    gboolean subdrained = FALSE;

    handled |=
        drain_and_switch_chains (chain, drainpad, last_group, &subdrained,
        switched);
    if (!subdrained)
      group->drained = FALSE;
  }

beach:
  GST_DEBUG ("group %p (last_group:%d, drained:%d, switched:%d, handled:%d)",
      group, *last_group, group->drained, *switched, handled);
  *drained = group->drained;
  return handled;
}

static gboolean
drain_and_switch_chains (GstParseChain * chain, GstParsePad * drainpad,
    gboolean * last_group, gboolean * drained, gboolean * switched)
{
  gboolean handled = FALSE;
  GstParseBin *parsebin = chain->parsebin;

  GST_DEBUG ("Checking chain %p %s:%s (target pad %s:%s)",
      chain, GST_DEBUG_PAD_NAME (chain->pad), GST_DEBUG_PAD_NAME (drainpad));

  CHAIN_MUTEX_LOCK (chain);

  /* Definitely can't be in drained chains */
  if (G_UNLIKELY (chain->drained)) {
    goto beach;
  }

  if (chain->endpad) {
    /* Check if we're reached the target endchain */
    if (drainpad != NULL && chain == drainpad->chain) {
      GST_DEBUG ("Found the target chain");
      drainpad->drained = TRUE;
      handled = TRUE;
    }

    chain->drained = chain->endpad->drained;
    goto beach;
  }

  /* We known there are groups to switch to */
  if (chain->next_groups)
    *last_group = FALSE;

  /* Check the active group */
  if (chain->active_group) {
    gboolean subdrained = FALSE;
    handled = drain_and_switch_group (chain->active_group, drainpad,
        last_group, &subdrained, switched);

    /* The group is drained, see if we can switch to another */
    if ((handled || drainpad == NULL) && subdrained && !*switched) {
      if (chain->next_groups) {
        /* Switch to next group, the actual removal of the current group will
         * be done when the next one is activated */
        GST_DEBUG_OBJECT (parsebin, "Moving current group %p to old groups",
            chain->active_group);
        chain->old_groups =
            g_list_prepend (chain->old_groups, chain->active_group);
        GST_DEBUG_OBJECT (parsebin, "Switching to next group %p",
            chain->next_groups->data);
        chain->active_group = chain->next_groups->data;
        chain->next_groups =
            g_list_delete_link (chain->next_groups, chain->next_groups);
        *switched = TRUE;
        chain->drained = FALSE;
      } else {
        GST_DEBUG ("Group %p was the last in chain %p", chain->active_group,
            chain);
        chain->drained = TRUE;
        /* We're drained ! */
      }
    } else {
      if (subdrained && !chain->next_groups)
        *drained = TRUE;
    }
  }

beach:
  CHAIN_MUTEX_UNLOCK (chain);

  GST_DEBUG
      ("Chain %p (%s:%s handled:%d, last_group:%d, drained:%d, switched:%d, deadend:%d)",
      chain, GST_DEBUG_PAD_NAME (chain->pad), handled, *last_group,
      chain->drained, *switched, chain->deadend);

  *drained = chain->drained;

  return handled;
}

/* check if the group is drained, meaning all pads have seen an EOS
 * event.  */
static gboolean
gst_parse_pad_handle_eos (GstParsePad * pad)
{
  gboolean last_group = TRUE;
  gboolean switched = FALSE;
  gboolean drained = FALSE;
  GstParseChain *chain = pad->chain;
  GstParseBin *parsebin = chain->parsebin;

  GST_LOG_OBJECT (parsebin, "pad %p", pad);
  EXPOSE_LOCK (parsebin);
  if (parsebin->parse_chain) {
    drain_and_switch_chains (parsebin->parse_chain, pad, &last_group, &drained,
        &switched);

    GST_LOG_OBJECT (parsebin, "drained:%d switched:%d", drained, switched);
    if (switched) {
      /* If we resulted in a group switch, expose what's needed */
      if (gst_parse_chain_is_complete (parsebin->parse_chain))
        gst_parse_bin_expose (parsebin);
    }

    if (drained) {
      GST_DEBUG_OBJECT (parsebin, "We are fully drained, emitting signal");
      g_signal_emit (parsebin, gst_parse_bin_signals[SIGNAL_DRAINED], 0, NULL);
    }

  }
  EXPOSE_UNLOCK (parsebin);

  return last_group;
}

/* gst_parse_group_is_drained:
 *
 * Check is this group is drained and cache this result.
 * The group is drained if all child chains are drained.
 *
 * Not MT-safe, call with group->parent's lock */
static gboolean
gst_parse_group_is_drained (GstParseGroup * group)
{
  GList *l;
  gboolean drained = TRUE;

  if (group->drained) {
    drained = TRUE;
    goto out;
  }

  for (l = group->children; l; l = l->next) {
    GstParseChain *chain = l->data;

    CHAIN_MUTEX_LOCK (chain);
    if (!gst_parse_chain_is_drained (chain))
      drained = FALSE;
    CHAIN_MUTEX_UNLOCK (chain);
    if (!drained)
      goto out;
  }
  group->drained = drained;

out:
  GST_DEBUG_OBJECT (group->parsebin, "Group %p is drained: %d", group, drained);
  return drained;
}

/* gst_parse_chain_is_drained:
 *
 * Check is the chain is drained, which means that
 * either
 *
 * a) it's endpad is drained
 * b) there are no pending pads, the active group is drained
 *    and there are no next groups
 *
 * Not MT-safe, call with chain lock
 */
static gboolean
gst_parse_chain_is_drained (GstParseChain * chain)
{
  gboolean drained = FALSE;

  if (chain->endpad) {
    drained = chain->endpad->drained;
    goto out;
  }

  if (chain->pending_pads) {
    drained = FALSE;
    goto out;
  }

  if (chain->active_group && gst_parse_group_is_drained (chain->active_group)
      && !chain->next_groups) {
    drained = TRUE;
    goto out;
  }

out:
  GST_DEBUG_OBJECT (chain->parsebin, "Chain %p is drained: %d", chain, drained);
  return drained;
}

/* sort_end_pads:
 * GCompareFunc to use with lists of GstPad.
 * Sorts pads by mime type.
 * First video (raw, then non-raw), then audio (raw, then non-raw),
 * then others.
 *
 * Return: negative if a<b, 0 if a==b, positive if a>b
 */
static gint
sort_end_pads (GstParsePad * da, GstParsePad * db)
{
  gint va, vb;
  const gchar *namea, *nameb;
  gchar *ida, *idb;
  gint ret;
  GstCaps *capsa, *capsb;

  capsa = get_pad_caps (GST_PAD_CAST (da));
  capsb = get_pad_caps (GST_PAD_CAST (db));

  if (gst_caps_get_size (capsa) == 0 || gst_caps_get_size (capsb) == 0) {
    if (gst_caps_is_any (capsa))
      va = 6;
    if (gst_caps_is_empty (capsa))
      va = 7;
    else
      va = 0;

    if (gst_caps_is_any (capsb))
      vb = 6;
    if (gst_caps_is_empty (capsb))
      vb = 7;
    else
      vb = 0;
  } else {
    GstStructure *sa, *sb;

    sa = gst_caps_get_structure ((const GstCaps *) capsa, 0);
    sb = gst_caps_get_structure ((const GstCaps *) capsb, 0);

    namea = gst_structure_get_name (sa);
    nameb = gst_structure_get_name (sb);

    if (g_strrstr (namea, "video/x-raw"))
      va = 0;
    else if (g_strrstr (namea, "video/"))
      va = 1;
    else if (g_strrstr (namea, "image/"))
      va = 2;
    else if (g_strrstr (namea, "audio/x-raw"))
      va = 3;
    else if (g_strrstr (namea, "audio/"))
      va = 4;
    else
      va = 5;

    if (g_strrstr (nameb, "video/x-raw"))
      vb = 0;
    else if (g_strrstr (nameb, "video/"))
      vb = 1;
    else if (g_strrstr (nameb, "image/"))
      vb = 2;
    else if (g_strrstr (nameb, "audio/x-raw"))
      vb = 3;
    else if (g_strrstr (nameb, "audio/"))
      vb = 4;
    else
      vb = 5;
  }

  gst_caps_unref (capsa);
  gst_caps_unref (capsb);

  if (va != vb)
    return va - vb;

  /* if otherwise the same, sort by stream-id */
  ida = gst_pad_get_stream_id (GST_PAD_CAST (da));
  idb = gst_pad_get_stream_id (GST_PAD_CAST (db));
  ret = (ida) ? ((idb) ? strcmp (ida, idb) : -1) : 1;
  g_free (ida);
  g_free (idb);

  return ret;
}

static gboolean
debug_sticky_event (GstPad * pad, GstEvent ** event, gpointer user_data)
{
  GST_DEBUG_OBJECT (pad, "sticky event %s (%p)", GST_EVENT_TYPE_NAME (*event),
      *event);
  return TRUE;
}

/* Must only be called if the toplevel chain is complete and blocked! */
/* Not MT-safe, call with ParseBin expose lock! */
static gboolean
gst_parse_bin_expose (GstParseBin * parsebin)
{
  GList *tmp, *endpads;
  gboolean missing_plugin;
  GString *missing_plugin_details;
  gboolean already_exposed;
  gboolean last_group;
  gboolean uncollected_streams;
  GstStreamCollection *fallback_collection = NULL;

retry:
  endpads = NULL;
  missing_plugin = FALSE;
  already_exposed = TRUE;
  last_group = TRUE;

  missing_plugin_details = g_string_new ("");

  GST_DEBUG_OBJECT (parsebin, "Exposing currently active chains/groups");

  /* Don't expose if we're currently shutting down */
  DYN_LOCK (parsebin);
  if (G_UNLIKELY (parsebin->shutdown)) {
    GST_WARNING_OBJECT (parsebin,
        "Currently, shutting down, aborting exposing");
    DYN_UNLOCK (parsebin);
    return FALSE;
  }
  DYN_UNLOCK (parsebin);

  /* Get the pads that we're going to expose and mark things as exposed */
  uncollected_streams = FALSE;
  CHAIN_MUTEX_LOCK (parsebin->parse_chain);
  if (!gst_parse_chain_expose (parsebin->parse_chain, &endpads, &missing_plugin,
          missing_plugin_details, &last_group, &uncollected_streams)) {
    g_list_free_full (endpads, (GDestroyNotify) gst_object_unref);
    g_string_free (missing_plugin_details, TRUE);
    GST_ERROR_OBJECT (parsebin, "Broken chain/group tree");
    CHAIN_MUTEX_UNLOCK (parsebin->parse_chain);
    return FALSE;
  }
  CHAIN_MUTEX_UNLOCK (parsebin->parse_chain);
  if (endpads == NULL) {
    if (missing_plugin) {
      if (missing_plugin_details->len > 0) {
        gchar *details = g_string_free (missing_plugin_details, FALSE);
        GST_ELEMENT_ERROR (parsebin, CORE, MISSING_PLUGIN, (NULL),
            ("no suitable plugins found:\n%s", details));
        g_free (details);
      } else {
        g_string_free (missing_plugin_details, TRUE);
        GST_ELEMENT_ERROR (parsebin, CORE, MISSING_PLUGIN, (NULL),
            ("no suitable plugins found"));
      }
    } else {
      /* in this case, the stream ended without buffers,
       * just post a warning */
      g_string_free (missing_plugin_details, TRUE);

      GST_WARNING_OBJECT (parsebin, "All streams finished without buffers. "
          "Last group: %d", last_group);
      if (last_group) {
        GST_ELEMENT_ERROR (parsebin, STREAM, FAILED, (NULL),
            ("all streams without buffers"));
      } else {
        gboolean switched = FALSE;
        gboolean drained = FALSE;

        drain_and_switch_chains (parsebin->parse_chain, NULL, &last_group,
            &drained, &switched);
        GST_ELEMENT_WARNING (parsebin, STREAM, FAILED, (NULL),
            ("all streams without buffers"));
        if (switched) {
          if (gst_parse_chain_is_complete (parsebin->parse_chain))
            goto retry;
          else
            return FALSE;
        }
      }
    }

    return FALSE;
  }

  if (uncollected_streams) {
    /* FIXME: Collect and use a stream id from the top chain as
     * upstream ID? */
    fallback_collection = gst_stream_collection_new (NULL);

    build_fallback_collection (parsebin->parse_chain, fallback_collection);

    gst_element_post_message (GST_ELEMENT (parsebin),
        gst_message_new_stream_collection (GST_OBJECT (parsebin),
            fallback_collection));
  }

  g_string_free (missing_plugin_details, TRUE);

  /* Check if this was called when everything was exposed already,
   * and see if we need to post a new fallback collection */
  for (tmp = endpads; tmp && already_exposed; tmp = tmp->next) {
    GstParsePad *parsepad = tmp->data;

    already_exposed &= parsepad->exposed;
  }
  if (already_exposed) {
    GST_DEBUG_OBJECT (parsebin, "Everything was exposed already!");
    if (fallback_collection)
      gst_object_unref (fallback_collection);
    g_list_free_full (endpads, (GDestroyNotify) gst_object_unref);
    return TRUE;
  }

  /* Set all already exposed pads to blocked */
  for (tmp = endpads; tmp; tmp = tmp->next) {
    GstParsePad *parsepad = tmp->data;

    if (parsepad->exposed) {
      GST_DEBUG_OBJECT (parsepad, "blocking exposed pad");
      gst_parse_pad_set_blocked (parsepad, TRUE);
    }
  }

  /* re-order pads : video, then audio, then others */
  endpads = g_list_sort (endpads, (GCompareFunc) sort_end_pads);

  /* Don't expose if we're currently shutting down */
  DYN_LOCK (parsebin);
  if (G_UNLIKELY (parsebin->shutdown)) {
    g_list_free_full (endpads, (GDestroyNotify) gst_object_unref);
    GST_WARNING_OBJECT (parsebin,
        "Currently, shutting down, aborting exposing");
    DYN_UNLOCK (parsebin);
    if (fallback_collection)
      gst_object_unref (fallback_collection);
    return FALSE;
  }

  /* Expose pads */
  for (tmp = endpads; tmp; tmp = tmp->next) {
    GstParsePad *parsepad = (GstParsePad *) tmp->data;
    gchar *padname;

    if (parsepad->exposed) {
      GST_DEBUG_OBJECT (parsepad, "Pad is already exposed, not doing anything");
      continue;
    }

    /* 1. rewrite name */
    padname = g_strdup_printf ("src_%u", parsebin->nbpads);
    parsebin->nbpads++;
    GST_DEBUG_OBJECT (parsebin, "About to expose parsepad %s as %s",
        GST_OBJECT_NAME (parsepad), padname);
    gst_object_set_name (GST_OBJECT (parsepad), padname);
    g_free (padname);

    gst_pad_sticky_events_foreach (GST_PAD_CAST (parsepad), debug_sticky_event,
        parsepad);

    /* 2. activate and add */
    parsepad->exposed = TRUE;
    if (!gst_element_add_pad (GST_ELEMENT (parsebin), GST_PAD_CAST (parsepad))) {
      /* not really fatal, we can try to add the other pads */
      g_warning ("error adding pad to ParseBin");
      parsepad->exposed = FALSE;
      continue;
    }

    GST_INFO_OBJECT (parsepad, "added new parsed pad");
  }

  DYN_UNLOCK (parsebin);

  /* Unblock internal pads. The application should have connected stuff now
   * so that streaming can continue. */
  for (tmp = endpads; tmp; tmp = tmp->next) {
    GstParsePad *parsepad = (GstParsePad *) tmp->data;

    if (parsepad->exposed) {
      GST_DEBUG_OBJECT (parsepad, "unblocking");
      gst_parse_pad_unblock (parsepad);
      GST_DEBUG_OBJECT (parsepad, "unblocked");
    }

    /* Send stream-collection events for any pads that don't have them,
     * and post a stream-collection onto the bus */
    if (parsepad->active_collection == NULL && fallback_collection) {
      gst_pad_push_event (GST_PAD (parsepad),
          gst_event_new_stream_collection (fallback_collection));
    }
    gst_object_unref (parsepad);
  }
  g_list_free (endpads);

  if (fallback_collection)
    gst_object_unref (fallback_collection);

  /* Remove old groups */
  chain_remove_old_groups (parsebin->parse_chain);

  GST_DEBUG_OBJECT (parsebin, "Exposed everything");
  return TRUE;
}

/* gst_parse_chain_expose:
 *
 * Check if the chain can be exposed and add all endpads
 * to the endpads list.
 *
 * Not MT-safe, call with ParseBin expose lock! *
 */
static gboolean
gst_parse_chain_expose (GstParseChain * chain, GList ** endpads,
    gboolean * missing_plugin, GString * missing_plugin_details,
    gboolean * last_group, gboolean * uncollected_streams)
{
  GstParseGroup *group;
  GList *l;
  gboolean ret = FALSE;

  if (chain->deadend) {
    if (chain->endcaps) {
      if (chain->deadend_details) {
        g_string_append (missing_plugin_details, chain->deadend_details);
        g_string_append_c (missing_plugin_details, '\n');
      } else {
        gchar *desc = gst_pb_utils_get_codec_description (chain->endcaps);
        gchar *caps_str = gst_caps_to_string (chain->endcaps);
        g_string_append_printf (missing_plugin_details,
            "Missing parser: %s (%s)\n", desc, caps_str);
        g_free (caps_str);
        g_free (desc);
      }
      *missing_plugin = TRUE;
    }
    return TRUE;
  }

  if (chain->endpad == NULL && chain->parsed && chain->pending_pads) {
    /* The chain has a pending pad from a parser, let's just
     * expose that now as the endpad */
    GList *cur = chain->pending_pads;
    GstPendingPad *ppad = (GstPendingPad *) (cur->data);
    GstPad *endpad = gst_object_ref (ppad->pad);
    GstElement *elem =
        GST_ELEMENT (gst_object_get_parent (GST_OBJECT (endpad)));

    chain->pending_pads = g_list_remove (chain->pending_pads, ppad);

    gst_pending_pad_free (ppad);

    GST_DEBUG_OBJECT (chain->parsebin,
        "Exposing pad %" GST_PTR_FORMAT " with incomplete caps "
        "because it's parsed", endpad);

    expose_pad (chain->parsebin, elem, chain->current_pad, endpad, NULL, chain);
    gst_object_unref (endpad);
    gst_object_unref (elem);
  }

  if (chain->endpad) {
    GstParsePad *p = chain->endpad;

    if (p->active_stream && p->active_collection == NULL
        && !p->in_a_fallback_collection)
      *uncollected_streams = TRUE;

    *endpads = g_list_prepend (*endpads, gst_object_ref (p));
    return TRUE;
  }

  if (chain->next_groups)
    *last_group = FALSE;

  group = chain->active_group;
  if (!group) {
    GstParsePad *p = chain->current_pad;

    if (!p)
      return FALSE;

    if (p->active_stream && p->active_collection == NULL
        && !p->in_a_fallback_collection)
      *uncollected_streams = TRUE;

    return FALSE;
  }

  for (l = group->children; l; l = l->next) {
    GstParseChain *childchain = l->data;

    CHAIN_MUTEX_LOCK (childchain);
    ret |= gst_parse_chain_expose (childchain, endpads, missing_plugin,
        missing_plugin_details, last_group, uncollected_streams);
    CHAIN_MUTEX_UNLOCK (childchain);
  }

  return ret;
}

static void
build_fallback_collection (GstParseChain * chain,
    GstStreamCollection * collection)
{
  GstParseGroup *group = chain->active_group;
  GList *l;

  /* If it's an end pad, or a not-finished chain that's
   * not a group, put it in the collection */
  if (chain->endpad || (chain->current_pad && group == NULL)) {
    GstParsePad *p = chain->current_pad;

    if (p->active_stream != NULL && p->active_collection == NULL) {
      GST_DEBUG_OBJECT (p, "Adding stream to fallback collection");
      if (G_UNLIKELY (gst_stream_get_stream_type (p->active_stream) ==
              GST_STREAM_TYPE_UNKNOWN)) {
        GstCaps *caps;
        caps = get_pad_caps (GST_PAD_CAST (p));

        if (caps) {
          GstStreamType type = guess_stream_type_from_caps (caps);
          if (type != GST_STREAM_TYPE_UNKNOWN) {
            gst_stream_set_stream_type (p->active_stream, type);
            gst_stream_set_caps (p->active_stream, caps);
          }
          gst_caps_unref (caps);
        }
      }
      gst_stream_collection_add_stream (collection,
          gst_object_ref (p->active_stream));
      p->in_a_fallback_collection = TRUE;
    }
    return;
  }

  if (!group)
    return;

  /* we used g_list_prepend when adding children, so iterate from last
   * to first to maintain the original order they were added in */
  for (l = g_list_last (group->children); l != NULL; l = l->prev) {
    GstParseChain *childchain = l->data;

    build_fallback_collection (childchain, collection);
  }
}

/*************************
 * GstParsePad functions
 *************************/

static void gst_parse_pad_dispose (GObject * object);

static void
gst_parse_pad_class_init (GstParsePadClass * klass)
{
  GObjectClass *gobject_klass;

  gobject_klass = (GObjectClass *) klass;

  gobject_klass->dispose = gst_parse_pad_dispose;
}

static void
gst_parse_pad_init (GstParsePad * pad)
{
  pad->chain = NULL;
  pad->blocked = FALSE;
  pad->exposed = FALSE;
  pad->drained = FALSE;
  gst_object_ref_sink (pad);
}

static void
gst_parse_pad_dispose (GObject * object)
{
  GstParsePad *parsepad = (GstParsePad *) (object);
  parse_pad_set_target (parsepad, NULL);

  gst_object_replace ((GstObject **) & parsepad->active_collection, NULL);
  gst_object_replace ((GstObject **) & parsepad->active_stream, NULL);

  G_OBJECT_CLASS (gst_parse_pad_parent_class)->dispose (object);
}

static GstPadProbeReturn
source_pad_blocked_cb (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  GstParsePad *parsepad = user_data;
  GstParseChain *chain;
  GstParseBin *parsebin;
  GstPadProbeReturn ret = GST_PAD_PROBE_OK;

  if (GST_PAD_PROBE_INFO_TYPE (info) & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM) {
    GstEvent *event = GST_PAD_PROBE_INFO_EVENT (info);

    GST_LOG_OBJECT (pad, "Seeing event '%s'", GST_EVENT_TYPE_NAME (event));

    if (!GST_EVENT_IS_SERIALIZED (event)) {
      /* do not block on sticky or out of band events otherwise the allocation query
         from demuxer might block the loop thread */
      GST_LOG_OBJECT (pad, "Letting OOB event through");
      return GST_PAD_PROBE_PASS;
    }

    if (GST_EVENT_IS_STICKY (event) && GST_EVENT_TYPE (event) != GST_EVENT_EOS) {
      GstPad *peer;

      /* manually push sticky events to ghost pad to avoid exposing pads
       * that don't have the sticky events. Handle EOS separately as we
       * want to block the pad on it if we didn't get any buffers before
       * EOS and expose the pad then. */
      peer = gst_pad_get_peer (pad);
      gst_pad_send_event (peer, event);
      gst_object_unref (peer);
      GST_LOG_OBJECT (pad, "Manually pushed sticky event through");
      ret = GST_PAD_PROBE_HANDLED;
      goto done;
    }
  } else if (GST_PAD_PROBE_INFO_TYPE (info) &
      GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM) {
    GstQuery *query = GST_PAD_PROBE_INFO_QUERY (info);

    if (!GST_QUERY_IS_SERIALIZED (query)) {
      /* do not block on non-serialized queries */
      GST_LOG_OBJECT (pad, "Letting non-serialized query through");
      return GST_PAD_PROBE_PASS;
    }
    if (!gst_pad_has_current_caps (pad)) {
      /* do not block on allocation queries before we have caps,
       * this would deadlock because we are doing no autoplugging
       * without caps.
       * TODO: Try to do autoplugging based on the query caps
       */
      GST_LOG_OBJECT (pad, "Letting serialized query before caps through");
      return GST_PAD_PROBE_PASS;
    }
  }
  chain = parsepad->chain;
  parsebin = chain->parsebin;

  GST_LOG_OBJECT (parsepad, "blocked: parsepad->chain:%p", chain);

  parsepad->blocked = TRUE;

  EXPOSE_LOCK (parsebin);
  if (parsebin->parse_chain) {
    if (!gst_parse_bin_expose (parsebin))
      GST_WARNING_OBJECT (parsebin, "Couldn't expose group");
  }
  EXPOSE_UNLOCK (parsebin);

done:
  return ret;
}

/* FIXME: We can probably do some cleverer things, and maybe move this into
 * pbutils. Ideas:
 *    if there are tags look if it's got an AUDIO_CODEC VIDEO_CODEC CONTAINER_FORMAT tag
 *    Look at the factory klass designation of parsers in the chain
 *    Consider demuxer pad names as well, sometimes they give the type away
 */
static GstStreamType
guess_stream_type_from_caps (GstCaps * caps)
{
  GstStructure *s;
  const gchar *name;
  GstPbUtilsCapsDescriptionFlags desc;

  if (gst_caps_get_size (caps) < 1)
    return GST_STREAM_TYPE_UNKNOWN;

  s = gst_caps_get_structure (caps, 0);
  name = gst_structure_get_name (s);

  if (gst_structure_has_field (s, "original-media-type")) {
    /* Caps describe an encrypted payload, use original-media-type to determine stream type. */
    name = gst_structure_get_string (s, "original-media-type");
  }

  if (g_str_has_prefix (name, "video/") || g_str_has_prefix (name, "image/"))
    return GST_STREAM_TYPE_VIDEO;
  if (g_str_has_prefix (name, "audio/"))
    return GST_STREAM_TYPE_AUDIO;
  if (g_str_has_prefix (name, "text/") ||
      g_str_has_prefix (name, "subpicture/") ||
      g_str_has_prefix (name, "subtitle/") ||
      g_str_has_prefix (name, "closedcaption/"))
    return GST_STREAM_TYPE_TEXT;

  /* Use information from pbutils. Note that we only care about elementary
   * streams which is why we check flag equality */
  desc = gst_pb_utils_get_caps_description_flags (caps);
  switch (desc) {
    case GST_PBUTILS_CAPS_DESCRIPTION_FLAG_AUDIO:
      return GST_STREAM_TYPE_AUDIO;
    case GST_PBUTILS_CAPS_DESCRIPTION_FLAG_VIDEO:
      return GST_STREAM_TYPE_VIDEO;
    case GST_PBUTILS_CAPS_DESCRIPTION_FLAG_SUBTITLE:
      return GST_STREAM_TYPE_TEXT;
    default:
      return GST_STREAM_TYPE_UNKNOWN;
  }
}

static void
gst_parse_pad_update_caps (GstParsePad * parsepad, GstCaps * caps)
{
  if (caps && parsepad->active_stream) {
    GST_DEBUG_OBJECT (parsepad, "Storing caps %" GST_PTR_FORMAT
        " on stream %" GST_PTR_FORMAT, caps, parsepad->active_stream);

    if (gst_caps_is_fixed (caps))
      gst_stream_set_caps (parsepad->active_stream, caps);
    /* intuit a type */
    if (gst_stream_get_stream_type (parsepad->active_stream) ==
        GST_STREAM_TYPE_UNKNOWN) {
      GstStreamType new_type = guess_stream_type_from_caps (caps);
      if (new_type != GST_STREAM_TYPE_UNKNOWN)
        gst_stream_set_stream_type (parsepad->active_stream, new_type);
    }
  }
}

static void
gst_parse_pad_update_tags (GstParsePad * parsepad, GstTagList * tags)
{
  if (tags && gst_tag_list_get_scope (tags) == GST_TAG_SCOPE_STREAM
      && parsepad->active_stream) {
    GST_DEBUG_OBJECT (parsepad,
        "Storing new tags %" GST_PTR_FORMAT " on stream %" GST_PTR_FORMAT, tags,
        parsepad->active_stream);
    gst_stream_set_tags (parsepad->active_stream, tags);
  }
}

static GstEvent *
gst_parse_pad_stream_start_event (GstParsePad * parsepad, GstEvent * event)
{
  GstStream *stream = NULL;
  const gchar *stream_id = NULL;
  gboolean repeat_event = FALSE;
  GstStreamFlags streamflags;

  gst_event_parse_stream_start (event, &stream_id);
  gst_event_parse_stream_flags (event, &streamflags);

  if (parsepad->active_stream != NULL &&
      g_str_equal (parsepad->active_stream->stream_id, stream_id))
    repeat_event = TRUE;
  else {
    /* A new stream requires a new collection event, or else
     * we'll place it in a fallback collection later */
    gst_object_replace ((GstObject **) & parsepad->active_collection, NULL);
    parsepad->in_a_fallback_collection = FALSE;
  }

  gst_event_parse_stream (event, &stream);
  if (stream == NULL) {
    GstCaps *caps = gst_pad_get_current_caps (GST_PAD_CAST (parsepad));
    if (caps == NULL) {
      /* Try and get caps from the parsepad peer */
      GstPad *peer = gst_ghost_pad_get_target (GST_GHOST_PAD (parsepad));
      caps = gst_pad_get_current_caps (peer);
      gst_object_unref (peer);
    }
    if (caps == NULL && parsepad->chain && parsepad->chain->start_caps) {
      /* Still no caps, use the chain start caps */
      caps = gst_caps_ref (parsepad->chain->start_caps);
    }

    GST_DEBUG_OBJECT (parsepad,
        "Saw stream_start with no GstStream. Adding one. Caps %"
        GST_PTR_FORMAT, caps);

    if (repeat_event) {
      GST_LOG_OBJECT (parsepad, "Using previously created GstStream");
      stream = gst_object_ref (parsepad->active_stream);
    } else {
      GST_LOG_OBJECT (parsepad, "Creating unknown GstStream");
      stream =
          gst_stream_new (stream_id, NULL, GST_STREAM_TYPE_UNKNOWN,
          streamflags);
      gst_object_replace ((GstObject **) & parsepad->active_stream,
          (GstObject *) stream);
    }
    if (caps) {
      gst_parse_pad_update_caps (parsepad, caps);
      gst_caps_unref (caps);
    }

    event = gst_event_make_writable (event);
    gst_event_set_stream (event, stream);
  }
  gst_object_unref (stream);
  GST_LOG_OBJECT (parsepad, "Saw stream %s (GstStream %p)",
      stream->stream_id, stream);

  return event;
}

static void
gst_parse_pad_update_stream_collection (GstParsePad * parsepad,
    GstStreamCollection * collection)
{
  GST_LOG_OBJECT (parsepad, "Got new stream collection %p", collection);
  gst_object_replace ((GstObject **) & parsepad->active_collection,
      (GstObject *) collection);
  parsepad->in_a_fallback_collection = FALSE;
}

static GstPadProbeReturn
gst_parse_pad_event (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  GstEvent *event = GST_PAD_PROBE_INFO_EVENT (info);
  GstObject *parent = gst_pad_get_parent (pad);
  GstParsePad *parsepad = GST_PARSE_PAD (parent);
  gboolean forwardit = TRUE;

  GST_LOG_OBJECT (pad, "%s parsepad:%p", GST_EVENT_TYPE_NAME (event), parsepad);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:{
      GstCaps *caps = NULL;
      gst_event_parse_caps (event, &caps);
      gst_parse_pad_update_caps (parsepad, caps);
      break;
    }
    case GST_EVENT_TAG:{
      GstTagList *tags;
      gst_event_parse_tag (event, &tags);
      gst_parse_pad_update_tags (parsepad, tags);
      break;
    }
    case GST_EVENT_STREAM_START:{
      GST_PAD_PROBE_INFO_DATA (info) =
          gst_parse_pad_stream_start_event (parsepad, event);
      break;
    }
    case GST_EVENT_STREAM_COLLECTION:{
      GstStreamCollection *collection = NULL;
      gst_event_parse_stream_collection (event, &collection);
      gst_parse_pad_update_stream_collection (parsepad, collection);
      gst_element_post_message (GST_ELEMENT (parsepad->parsebin),
          gst_message_new_stream_collection (GST_OBJECT (parsepad->parsebin),
              collection));
      gst_object_unref (collection);
      break;
    }
    case GST_EVENT_EOS:{
      GST_DEBUG_OBJECT (pad, "we received EOS");

      /* Check if all pads are drained.
       * * If there is no next group, we will let the EOS go through.
       * * If there is a next group but the current group isn't completely
       *   drained, we will drop the EOS event.
       * * If there is a next group to expose and this was the last non-drained
       *   pad for that group, we will remove the ghostpad of the current group
       *   first, which unlinks the peer and so drops the EOS. */
      forwardit = gst_parse_pad_handle_eos (parsepad);
    }
    default:
      break;
  }
  gst_object_unref (parent);
  if (forwardit)
    return GST_PAD_PROBE_OK;
  else
    return GST_PAD_PROBE_DROP;
}

static void
gst_parse_pad_set_blocked (GstParsePad * parsepad, gboolean blocked)
{
  GstParseBin *parsebin = parsepad->parsebin;
  GstPad *opad;

  DYN_LOCK (parsebin);

  GST_DEBUG_OBJECT (parsepad, "blocking pad: %d", blocked);

  opad = gst_ghost_pad_get_target (GST_GHOST_PAD_CAST (parsepad));
  if (!opad)
    goto out;

  /* do not block if shutting down.
   * we do not consider/expect it blocked further below, but use other trick */
  if (!blocked || !parsebin->shutdown) {
    if (blocked) {
      if (parsepad->block_id == 0)
        parsepad->block_id =
            gst_pad_add_probe (opad,
            GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM |
            GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM, source_pad_blocked_cb,
            gst_object_ref (parsepad), (GDestroyNotify) gst_object_unref);
    } else {
      if (parsepad->block_id != 0) {
        gst_pad_remove_probe (opad, parsepad->block_id);
        parsepad->block_id = 0;
      }
      parsepad->blocked = FALSE;
    }
  }

  if (blocked) {
    if (parsebin->shutdown) {
      /* deactivate to force flushing state to prevent NOT_LINKED errors */
      gst_pad_set_active (GST_PAD_CAST (parsepad), FALSE);
      /* note that deactivating the target pad would have no effect here,
       * since elements are typically connected first (and pads exposed),
       * and only then brought to PAUSED state (so pads activated) */
    } else {
      gst_object_ref (parsepad);
      parsebin->blocked_pads =
          g_list_prepend (parsebin->blocked_pads, parsepad);
    }
  } else {
    GList *l;

    if ((l = g_list_find (parsebin->blocked_pads, parsepad))) {
      gst_object_unref (parsepad);
      parsebin->blocked_pads = g_list_delete_link (parsebin->blocked_pads, l);
    }
  }
  gst_object_unref (opad);
out:
  DYN_UNLOCK (parsebin);
}

static void
gst_parse_pad_activate (GstParsePad * parsepad, GstParseChain * chain)
{
  g_return_if_fail (chain != NULL);

  parsepad->chain = chain;
  gst_pad_set_active (GST_PAD_CAST (parsepad), TRUE);
  gst_parse_pad_set_blocked (parsepad, TRUE);
}

static void
gst_parse_pad_unblock (GstParsePad * parsepad)
{
  gst_parse_pad_set_blocked (parsepad, FALSE);
}

static gboolean
gst_parse_pad_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstParsePad *parsepad = GST_PARSE_PAD (parent);
  gboolean ret = FALSE;

  CHAIN_MUTEX_LOCK (parsepad->chain);
  if (!parsepad->exposed && !parsepad->parsebin->shutdown
      && !parsepad->chain->deadend && parsepad->chain->elements) {
    GstParseElement *pelem = parsepad->chain->elements->data;

    ret = FALSE;
    GST_DEBUG_OBJECT (parsepad->parsebin,
        "calling autoplug-query for %s (element %s): %" GST_PTR_FORMAT,
        GST_PAD_NAME (parsepad), GST_ELEMENT_NAME (pelem->element), query);
    g_signal_emit (G_OBJECT (parsepad->parsebin),
        gst_parse_bin_signals[SIGNAL_AUTOPLUG_QUERY], 0, parsepad,
        pelem->element, query, &ret);

    if (ret)
      GST_DEBUG_OBJECT (parsepad->parsebin,
          "autoplug-query returned %d: %" GST_PTR_FORMAT, ret, query);
    else
      GST_DEBUG_OBJECT (parsepad->parsebin, "autoplug-query returned %d", ret);
  }
  CHAIN_MUTEX_UNLOCK (parsepad->chain);

  /* If exposed or nothing handled the query use the default handler */
  if (!ret)
    ret = gst_pad_query_default (pad, parent, query);

  return ret;
}

/*gst_parse_pad_new:
 *
 * Creates a new GstParsePad for the given pad.
 */
static GstParsePad *
gst_parse_pad_new (GstParseBin * parsebin, GstParseChain * chain)
{
  GstParsePad *parsepad;
  GstProxyPad *ppad;
  GstPadTemplate *pad_tmpl;

  GST_DEBUG_OBJECT (parsebin, "making new parsepad");
  pad_tmpl = gst_static_pad_template_get (&parse_bin_src_template);
  parsepad =
      g_object_new (GST_TYPE_PARSE_PAD, "direction", GST_PAD_SRC,
      "template", pad_tmpl, NULL);
  parsepad->chain = chain;
  parsepad->parsebin = parsebin;
  gst_object_unref (pad_tmpl);

  ppad = gst_proxy_pad_get_internal (GST_PROXY_PAD (parsepad));
  gst_pad_set_query_function (GST_PAD_CAST (ppad), gst_parse_pad_query);

  /* Add downstream event probe */
  GST_LOG_OBJECT (parsepad, "Adding event probe on internal pad %"
      GST_PTR_FORMAT, ppad);
  gst_pad_add_probe (GST_PAD_CAST (ppad),
      GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, gst_parse_pad_event, parsepad, NULL);
  gst_object_unref (ppad);

  return parsepad;
}

static void
gst_pending_pad_free (GstPendingPad * ppad)
{
  g_assert (ppad);
  g_assert (ppad->pad);

  if (ppad->event_probe_id != 0)
    gst_pad_remove_probe (ppad->pad, ppad->event_probe_id);
  if (ppad->notify_caps_id)
    g_signal_handler_disconnect (ppad->pad, ppad->notify_caps_id);
  gst_object_unref (ppad->pad);
  g_free (ppad);
}

/*****
 * Element add/remove
 *****/

/* call with dyn_lock held */
static void
unblock_pads (GstParseBin * parsebin)
{
  GList *tmp;

  GST_LOG_OBJECT (parsebin, "unblocking pads");

  for (tmp = parsebin->blocked_pads; tmp; tmp = tmp->next) {
    GstParsePad *parsepad = (GstParsePad *) tmp->data;
    GstPad *opad;

    opad = gst_ghost_pad_get_target (GST_GHOST_PAD_CAST (parsepad));
    if (!opad)
      continue;

    GST_DEBUG_OBJECT (parsepad, "unblocking");
    if (parsepad->block_id != 0) {
      gst_pad_remove_probe (opad, parsepad->block_id);
      parsepad->block_id = 0;
    }
    parsepad->blocked = FALSE;
    /* make flushing, prevent NOT_LINKED */
    gst_pad_set_active (GST_PAD_CAST (parsepad), FALSE);
    gst_object_unref (parsepad);
    gst_object_unref (opad);
    GST_DEBUG_OBJECT (parsepad, "unblocked");
  }

  /* clear, no more blocked pads */
  g_list_free (parsebin->blocked_pads);
  parsebin->blocked_pads = NULL;
}

static GstStateChangeReturn
gst_parse_bin_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstParseBin *parsebin = GST_PARSE_BIN (element);
  GstParseChain *chain_to_free = NULL;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (parsebin->typefind == NULL)
        goto missing_typefind;
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      /* Make sure we've cleared all existing chains */
      EXPOSE_LOCK (parsebin);
      if (parsebin->parse_chain) {
        gst_parse_chain_free (parsebin->parse_chain);
        parsebin->parse_chain = NULL;
      }
      EXPOSE_UNLOCK (parsebin);
      DYN_LOCK (parsebin);
      GST_LOG_OBJECT (parsebin, "clearing shutdown flag");
      parsebin->shutdown = FALSE;
      DYN_UNLOCK (parsebin);
      parsebin->have_type = FALSE;


      /* connect a signal to find out when the typefind element found
       * a type */
      parsebin->have_type_id =
          g_signal_connect (parsebin->typefind, "have-type",
          G_CALLBACK (type_found), parsebin);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      if (parsebin->have_type_id)
        g_signal_handler_disconnect (parsebin->typefind,
            parsebin->have_type_id);
      parsebin->have_type_id = 0;
      DYN_LOCK (parsebin);
      GST_LOG_OBJECT (parsebin, "setting shutdown flag");
      parsebin->shutdown = TRUE;
      unblock_pads (parsebin);
      DYN_UNLOCK (parsebin);
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (G_UNLIKELY (ret == GST_STATE_CHANGE_FAILURE))
    goto activate_failed;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      EXPOSE_LOCK (parsebin);
      if (parsebin->parse_chain) {
        chain_to_free = parsebin->parse_chain;
        gst_parse_chain_free_internal (parsebin->parse_chain, TRUE);
        parsebin->parse_chain = NULL;
      }
      EXPOSE_UNLOCK (parsebin);
      if (chain_to_free)
        gst_parse_chain_free (chain_to_free);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      g_mutex_lock (&parsebin->cleanup_lock);
      if (parsebin->cleanup_thread) {
        g_thread_join (parsebin->cleanup_thread);
        parsebin->cleanup_thread = NULL;
      }
      g_mutex_unlock (&parsebin->cleanup_lock);
    default:
      break;
  }

  return ret;

/* ERRORS */
missing_typefind:
  {
    gst_element_post_message (element,
        gst_missing_element_message_new (element, "typefind"));
    GST_ELEMENT_ERROR (parsebin, CORE, MISSING_PLUGIN, (NULL),
        ("no typefind!"));
    return GST_STATE_CHANGE_FAILURE;
  }
activate_failed:
  {
    GST_DEBUG_OBJECT (element,
        "element failed to change states -- activation problem?");
    return GST_STATE_CHANGE_FAILURE;
  }
}

static void
gst_parse_bin_handle_message (GstBin * bin, GstMessage * msg)
{
  GstParseBin *parsebin = GST_PARSE_BIN (bin);
  gboolean drop = FALSE;

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:{
      /* Don't pass errors when shutting down. Sometimes,
       * elements can generate spurious errors because we set the
       * output pads to flushing, and they can't detect that if they
       * send an event at exactly the wrong moment */
      DYN_LOCK (parsebin);
      drop = parsebin->shutdown;
      DYN_UNLOCK (parsebin);

      if (!drop) {
        GST_OBJECT_LOCK (parsebin);
        drop =
            (g_list_find (parsebin->filtered, GST_MESSAGE_SRC (msg)) != NULL);
        if (drop)
          parsebin->filtered_errors =
              g_list_prepend (parsebin->filtered_errors, gst_message_ref (msg));
        GST_OBJECT_UNLOCK (parsebin);
      }
      break;
    }
    default:
      break;
  }

  if (drop)
    gst_message_unref (msg);
  else
    GST_BIN_CLASS (parent_class)->handle_message (bin, msg);
}
