/* GStreamer
 * Copyright (C) 2008 Jan Schmidt <thaytan@noraisin.net>
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
#  include <config.h>
#endif

#include <string.h>

#include <gst/gst.h>
#include <gst/glib-compat-private.h>
#include <gst/pbutils/missing-plugins.h>
#include <gst/video/video.h>
#include <gst/audio/audio.h>
#include <glib/gi18n-lib.h>

#include "resindvdbin.h"
#include "resindvdsrc.h"
#include "rsninputselector.h"
#include "rsndec.h"
#include "rsnparsetter.h"

#include "gstmpegdemux.h"

#define RSN_TYPE_INPUT_SELECTOR GST_TYPE_INPUT_SELECTOR

GST_DEBUG_CATEGORY_EXTERN (resindvd_debug);
#define GST_CAT_DEFAULT resindvd_debug

#define DVDBIN_LOCK(d) g_mutex_lock(&(d)->dvd_lock)
#define DVDBIN_UNLOCK(d) g_mutex_unlock(&(d)->dvd_lock)

#define DVDBIN_PREROLL_LOCK(d) g_mutex_lock(&(d)->preroll_lock)
#define DVDBIN_PREROLL_UNLOCK(d) g_mutex_unlock(&(d)->preroll_lock)

#define DEFAULT_DEVICE "/dev/dvd"
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  ARG_0,
  ARG_DEVICE
};

static GstStaticPadTemplate video_src_template =
GST_STATIC_PAD_TEMPLATE ("video",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (GST_VIDEO_FORMATS_ALL))
    );

static GstStaticPadTemplate audio_src_template =
GST_STATIC_PAD_TEMPLATE ("audio",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS (GST_AUDIO_CAPS_MAKE (GST_AUDIO_FORMATS_ALL))
    );

static GstStaticPadTemplate subpicture_src_template =
GST_STATIC_PAD_TEMPLATE ("subpicture",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS ("subpicture/x-dvd")
    );

static void rsn_dvdbin_finalize (GObject * object);
static void rsn_dvdbin_uri_handler_init (gpointer g_iface, gpointer iface_data);
static gboolean rsndvdbin_element_init (GstPlugin * plugin);

#define rsn_dvdbin_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (RsnDvdBin, rsn_dvdbin, GST_TYPE_BIN,
    G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER, rsn_dvdbin_uri_handler_init));
GST_ELEMENT_REGISTER_DEFINE_CUSTOM (rsndvdbin, rsndvdbin_element_init);

static void demux_pad_added (GstElement * element, GstPad * pad,
    RsnDvdBin * dvdbin);
static void demux_no_more_pads (GstElement * element, RsnDvdBin * dvdbin);
static void rsn_dvdbin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void rsn_dvdbin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static GstStateChangeReturn rsn_dvdbin_change_state (GstElement * element,
    GstStateChange transition);
static void rsn_dvdbin_no_more_pads (RsnDvdBin * dvdbin);


GST_DEBUG_CATEGORY (resindvd_debug);
#define GST_CAT_DEFAULT resindvd_debug

static void
rsn_dvdbin_class_init (RsnDvdBinClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;

  gobject_class = (GObjectClass *) klass;
  element_class = (GstElementClass *) klass;

  gobject_class->finalize = rsn_dvdbin_finalize;
  gobject_class->set_property = rsn_dvdbin_set_property;
  gobject_class->get_property = rsn_dvdbin_get_property;

  g_object_class_install_property (gobject_class, ARG_DEVICE,
      g_param_spec_string ("device", "Device", "DVD device location",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (element_class,
      &video_src_template);
  gst_element_class_add_static_pad_template (element_class,
      &audio_src_template);
  gst_element_class_add_static_pad_template (element_class,
      &subpicture_src_template);

  element_class->change_state = GST_DEBUG_FUNCPTR (rsn_dvdbin_change_state);

  gst_element_class_set_static_metadata (element_class, "rsndvdbin",
      "Generic/Bin/Player",
      "DVD playback element", "Jan Schmidt <thaytan@noraisin.net>");
}

static void
rsn_dvdbin_init (RsnDvdBin * dvdbin)
{
  g_mutex_init (&dvdbin->dvd_lock);
  g_mutex_init (&dvdbin->preroll_lock);
}

static void
rsn_dvdbin_finalize (GObject * object)
{
  RsnDvdBin *dvdbin = RESINDVDBIN (object);

  g_mutex_clear (&dvdbin->dvd_lock);
  g_mutex_clear (&dvdbin->preroll_lock);
  g_free (dvdbin->last_uri);
  g_free (dvdbin->device);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* URI interface */
static GstURIType
rsn_dvdbin_uri_get_type (GType type)
{
  return GST_URI_SRC;
}

static const gchar *const *
rsn_dvdbin_uri_get_protocols (GType type)
{
  static const gchar *protocols[] = { "dvd", NULL };

  return protocols;
}

static gchar *
rsn_dvdbin_uri_get_uri (GstURIHandler * handler)
{
  RsnDvdBin *dvdbin = RESINDVDBIN (handler);

  DVDBIN_LOCK (dvdbin);
  g_free (dvdbin->last_uri);
  if (dvdbin->device)
    dvdbin->last_uri = g_strdup_printf ("dvd://%s", dvdbin->device);
  else
    dvdbin->last_uri = g_strdup ("dvd://");
  DVDBIN_UNLOCK (dvdbin);

  return g_strdup (dvdbin->last_uri);
}

static gboolean
rsn_dvdbin_uri_set_uri (GstURIHandler * handler, const gchar * uri,
    GError ** error)
{
  RsnDvdBin *dvdbin = RESINDVDBIN (handler);
  gboolean ret;
  gchar *protocol, *location;

  protocol = gst_uri_get_protocol (uri);

  ret = (protocol && !strcmp (protocol, "dvd")) ? TRUE : FALSE;

  g_free (protocol);
  protocol = NULL;

  if (!ret)
    return ret;

  location = gst_uri_get_location (uri);
  if (!location)
    return ret;

  /*
   * URI structure: dvd:///path/to/device
   */
  if (g_str_has_prefix (uri, "dvd://")) {
    g_free (dvdbin->device);
    if (strlen (uri) > 6)
      dvdbin->device = g_strdup (uri + 6);
    else
      dvdbin->device = g_strdup (DEFAULT_DEVICE);
  }
#if 0
  /*
   * Parse out the new t/c/a and seek to them
   */
  {
    gchar **strs;
    gchar **strcur;
    gint pos = 0;

    strcur = strs = g_strsplit (location, ",", 0);
    while (strcur && *strcur) {
      gint val;

      if (!sscanf (*strcur, "%d", &val))
        break;

      switch (pos) {
        case 0:
          if (val != dvdbin->uri_title) {
            dvdbin->uri_title = val;
            dvdbin->new_seek = TRUE;
          }
          break;
        case 1:
          if (val != dvdbin->uri_chapter) {
            dvdbin->uri_chapter = val;
            dvdbin->new_seek = TRUE;
          }
          break;
        case 2:
          dvdbin->uri_angle = val;
          break;
      }

      strcur++;
      pos++;
    }

    g_strfreev (strs);
  }
#endif

  g_free (location);

  return ret;
}

static void
rsn_dvdbin_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = rsn_dvdbin_uri_get_type;
  iface->get_protocols = rsn_dvdbin_uri_get_protocols;
  iface->get_uri = rsn_dvdbin_uri_get_uri;
  iface->set_uri = rsn_dvdbin_uri_set_uri;
}

static void
rsn_dvdbin_no_more_pads (RsnDvdBin * dvdbin)
{
  if (dvdbin->did_no_more_pads)
    return;
  dvdbin->did_no_more_pads = TRUE;

  GST_DEBUG_OBJECT (dvdbin, "Firing no more pads");
  /* Shrink subpicture queue to smaller size */
  g_object_set (dvdbin->pieces[DVD_ELEM_SPUQ],
      "max-size-time", G_GUINT64_CONSTANT (0), "max-size-bytes", 0,
      "max-size-buffers", 1, NULL);
  gst_element_no_more_pads (GST_ELEMENT (dvdbin));
}

static gboolean
try_create_piece (RsnDvdBin * dvdbin, gint index,
    const gchar * factory, GType type, const gchar * name, const gchar * descr)
{
  GstElement *e;

  DVDBIN_LOCK (dvdbin);
  if (dvdbin->pieces[index] != NULL) {
    DVDBIN_UNLOCK (dvdbin);
    return TRUE;                /* Already exists */
  }
  DVDBIN_UNLOCK (dvdbin);

  if (factory != NULL) {
    e = gst_element_factory_make (factory, name);
  } else {
    if (name)
      e = g_object_new (type, "name", name, NULL);
    else
      e = g_object_new (type, NULL);
  }
  if (e == NULL)
    goto create_failed;

  if (!gst_bin_add (GST_BIN (dvdbin), e))
    goto add_failed;

  GST_DEBUG_OBJECT (dvdbin, "Added %s element: %" GST_PTR_FORMAT, descr, e);

  DVDBIN_LOCK (dvdbin);
  dvdbin->pieces[index] = e;
  DVDBIN_UNLOCK (dvdbin);

  return TRUE;
create_failed:
  gst_element_post_message (GST_ELEMENT_CAST (dvdbin),
      gst_missing_element_message_new (GST_ELEMENT_CAST (dvdbin), factory));
  GST_ELEMENT_ERROR (dvdbin, CORE, MISSING_PLUGIN, (NULL),
      ("Could not create %s element '%s'", descr, factory));
  return FALSE;
add_failed:
  gst_object_unref (e);
  GST_ELEMENT_ERROR (dvdbin, CORE, FAILED, (NULL),
      ("Could not add %s element to bin", descr));
  return FALSE;
}

typedef struct
{
  RsnDvdBin *dvdbin;
  GstPad *pad;
  gulong pad_block_id;
} RsnDvdBinPadBlockCtx;

static GstPadProbeReturn dvdbin_pad_blocked_cb (GstPad * pad,
    GstPadProbeInfo * info, RsnDvdBinPadBlockCtx * ctx);

static void
_pad_block_destroy_notify (RsnDvdBinPadBlockCtx * ctx)
{
  gst_object_unref (ctx->dvdbin);
  gst_object_unref (ctx->pad);
  g_free (ctx);
}

#if DEBUG_TIMING
static GstPadProbeReturn
dvdbin_dump_timing_info (GstPad * opad,
    GstPadProbeInfo * info, gpointer userdata)
{
  if (GST_PAD_PROBE_INFO_TYPE (info) & (GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM |
          GST_PAD_PROBE_TYPE_EVENT_FLUSH)) {
    GstEvent *event = GST_PAD_PROBE_INFO_EVENT (info);
    if (GST_EVENT_TYPE (event) == GST_EVENT_SEGMENT) {
      const GstSegment *seg;

      gst_event_parse_segment (event, &seg);

      g_print ("%s:%s segment: rate %g format %d, start: %"
          GST_TIME_FORMAT ", stop: %" GST_TIME_FORMAT ", time: %"
          GST_TIME_FORMAT " base: %" GST_TIME_FORMAT "\n",
          GST_DEBUG_PAD_NAME (opad),
          seg->rate, seg->format, GST_TIME_ARGS (seg->start),
          GST_TIME_ARGS (seg->stop), GST_TIME_ARGS (seg->time),
          GST_TIME_ARGS (seg->base));
    } else if (GST_EVENT_TYPE (event) == GST_EVENT_GAP) {
      GstClockTime ts, dur, end;
      gst_event_parse_gap (event, &ts, &dur);
      end = ts;
      if (ts != GST_CLOCK_TIME_NONE && dur != GST_CLOCK_TIME_NONE)
        end += dur;
      g_print ("%s:%s Gap TS: %" GST_TIME_FORMAT " dur %" GST_TIME_FORMAT
          " (to %" GST_TIME_FORMAT ")\n", GST_DEBUG_PAD_NAME (opad),
          GST_TIME_ARGS (ts), GST_TIME_ARGS (dur), GST_TIME_ARGS (end));
    } else if (GST_EVENT_TYPE (event) == GST_EVENT_FLUSH_STOP) {
      g_print ("%s:%s FLUSHED\n", GST_DEBUG_PAD_NAME (opad));
    }
  }
  if (GST_PAD_PROBE_INFO_TYPE (info) & GST_PAD_PROBE_TYPE_BUFFER) {
    GstBuffer *buf = GST_PAD_PROBE_INFO_BUFFER (info);
    g_print ("%s:%s Buffer PTS %" GST_TIME_FORMAT " duration %" GST_TIME_FORMAT
        "\n", GST_DEBUG_PAD_NAME (opad), GST_TIME_ARGS (GST_BUFFER_PTS (buf)),
        GST_TIME_ARGS (GST_BUFFER_DURATION (buf)));
  }
  return GST_PAD_PROBE_OK;
}
#endif

static gboolean
try_link_pieces (GstElement * e1, const gchar * pad1, GstElement * e2,
    const gchar * pad2)
{
  GstPad *src = gst_element_get_static_pad (e1, pad1);
  GstPad *sink = gst_element_get_static_pad (e2, pad2);
  gboolean ret = FALSE;

  if (src == NULL || sink == NULL)
    goto done;

  if (GST_PAD_LINK_FAILED (gst_pad_link (src, sink)))
    goto done;

  ret = TRUE;
done:
  if (src)
    gst_object_unref (src);
  if (sink)
    gst_object_unref (sink);
  return ret;
}

static gboolean
create_elements (RsnDvdBin * dvdbin)
{
  GstPadTemplate *src_templ = NULL;
  GstPad *src = NULL;
  GstPad *sink = NULL;
  RsnDvdBinPadBlockCtx *bctx = NULL;

  if (!try_create_piece (dvdbin, DVD_ELEM_SOURCE, NULL,
          RESIN_TYPE_DVDSRC, "dvdsrc", "DVD source")) {
    return FALSE;
  }

  /* FIXME: Locking */
  if (dvdbin->device) {
    g_object_set (G_OBJECT (dvdbin->pieces[DVD_ELEM_SOURCE]),
        "device", dvdbin->device, NULL);
  }

  /* FIXME: Import and use local copy of mpeg PS demuxer */
  if (!try_create_piece (dvdbin, DVD_ELEM_DEMUX,
          NULL, GST_TYPE_FLUPS_DEMUX, "dvddemux", "DVD demuxer"))
    return FALSE;

  if (gst_element_link (dvdbin->pieces[DVD_ELEM_SOURCE],
          dvdbin->pieces[DVD_ELEM_DEMUX]) == FALSE)
    goto failed_connect;

  /* Listen for new pads from the demuxer */
  g_signal_connect (G_OBJECT (dvdbin->pieces[DVD_ELEM_DEMUX]), "pad-added",
      G_CALLBACK (demux_pad_added), dvdbin);

  g_signal_connect (G_OBJECT (dvdbin->pieces[DVD_ELEM_DEMUX]), "no-more-pads",
      G_CALLBACK (demux_no_more_pads), dvdbin);

  if (!try_create_piece (dvdbin, DVD_ELEM_MQUEUE, "multiqueue", 0, "rsnmq",
          "multiqueue"))
    return FALSE;

  g_object_set (dvdbin->pieces[DVD_ELEM_MQUEUE],
      "max-size-time", (7 * GST_SECOND / 10), "max-size-bytes", 0,
      "max-size-buffers", 0, NULL);

  if (!try_create_piece (dvdbin, DVD_ELEM_VIDPARSE, "mpegvideoparse", 0,
          "rsnvidparse", "video parser"))
    return FALSE;

  /* Decodebin will throw a missing element message to find an MPEG decoder */
  if (!try_create_piece (dvdbin, DVD_ELEM_VIDDEC, NULL, RSN_TYPE_VIDEODEC,
          "rsnviddec", "video decoder"))
    return FALSE;

  /* FIXME: Replace identity */
  if (!try_create_piece (dvdbin, DVD_ELEM_PARSET, NULL, RSN_TYPE_RSNPARSETTER,
          "rsnparsetter", "Aspect ratio adjustment"))
    return FALSE;

  if (!try_link_pieces (dvdbin->pieces[DVD_ELEM_VIDPARSE], "src",
          dvdbin->pieces[DVD_ELEM_VIDDEC], "sink"))
    goto failed_vidparse_connect;

  src = gst_element_get_static_pad (dvdbin->pieces[DVD_ELEM_VIDDEC], "src");
  sink = gst_element_get_static_pad (dvdbin->pieces[DVD_ELEM_PARSET], "sink");
  if (src == NULL || sink == NULL)
    goto failed_viddec_connect;
  if (GST_PAD_LINK_FAILED (gst_pad_link (src, sink)))
    goto failed_viddec_connect;
  gst_object_unref (src);
  gst_object_unref (sink);
  src = sink = NULL;

  src = gst_element_get_static_pad (dvdbin->pieces[DVD_ELEM_PARSET], "src");
  if (src == NULL)
    goto failed_video_ghost;
  src_templ = gst_static_pad_template_get (&video_src_template);
  dvdbin->video_pad = gst_ghost_pad_new_from_template ("video", src, src_templ);
  gst_object_unref (src_templ);
  if (dvdbin->video_pad == NULL)
    goto failed_video_ghost;
  gst_pad_set_active (dvdbin->video_pad, TRUE);
  bctx = g_new (RsnDvdBinPadBlockCtx, 1);
  bctx->dvdbin = gst_object_ref (dvdbin);
  bctx->pad = gst_object_ref (dvdbin->video_pad);
  bctx->pad_block_id =
      gst_pad_add_probe (src, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
      (GstPadProbeCallback) dvdbin_pad_blocked_cb, bctx, (GDestroyNotify)
      _pad_block_destroy_notify);
  gst_object_unref (src);
  src = NULL;

#if DEBUG_TIMING
  gst_pad_add_probe (dvdbin->video_pad,
      GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM | GST_PAD_PROBE_TYPE_BUFFER |
      GST_PAD_PROBE_TYPE_EVENT_FLUSH,
      (GstPadProbeCallback) dvdbin_dump_timing_info, NULL, NULL);
#endif

  /* FIXME: Merge stream-selection logic to core and switch back */
  if (!try_create_piece (dvdbin, DVD_ELEM_SPU_SELECT, NULL,
          RSN_TYPE_INPUT_SELECTOR, "subpselect", "Subpicture stream selector"))
    return FALSE;

  g_object_set (G_OBJECT (dvdbin->pieces[DVD_ELEM_SPU_SELECT]),
      "sync-streams", FALSE, NULL);

  /* Add a single standalone queue to hold a single buffer of SPU data */
  if (!try_create_piece (dvdbin, DVD_ELEM_SPUQ, "queue", 0, "spu_q",
          "subpicture decoder buffer"))
    return FALSE;
  /* Allow a lot more while pre-rolling */
  g_object_set (dvdbin->pieces[DVD_ELEM_SPUQ],
      "max-size-time", G_GUINT64_CONSTANT (0), "max-size-bytes", 0,
      "max-size-buffers", 100, NULL);

  src = gst_element_get_static_pad (dvdbin->pieces[DVD_ELEM_SPU_SELECT], "src");
  sink = gst_element_get_static_pad (dvdbin->pieces[DVD_ELEM_SPUQ], "sink");
  if (src == NULL || sink == NULL)
    goto failed_spuq_connect;
  if (GST_PAD_LINK_FAILED (gst_pad_link (src, sink)))
    goto failed_spuq_connect;
  gst_object_unref (src);
  gst_object_unref (sink);
  src = sink = NULL;

  src = gst_element_get_static_pad (dvdbin->pieces[DVD_ELEM_SPUQ], "src");
  if (src == NULL)
    goto failed_spu_ghost;
  src_templ = gst_static_pad_template_get (&subpicture_src_template);
  dvdbin->subpicture_pad =
      gst_ghost_pad_new_from_template ("subpicture", src, src_templ);
  gst_object_unref (src_templ);
  if (dvdbin->subpicture_pad == NULL)
    goto failed_spu_ghost;
  gst_pad_set_active (dvdbin->subpicture_pad, TRUE);
  bctx = g_new (RsnDvdBinPadBlockCtx, 1);
  bctx->dvdbin = gst_object_ref (dvdbin);
  bctx->pad = gst_object_ref (dvdbin->subpicture_pad);
  bctx->pad_block_id =
      gst_pad_add_probe (src, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
      (GstPadProbeCallback) dvdbin_pad_blocked_cb, bctx, (GDestroyNotify)
      _pad_block_destroy_notify);
  gst_object_unref (src);
  src = NULL;

  if (!try_create_piece (dvdbin, DVD_ELEM_AUD_SELECT, NULL,
          RSN_TYPE_INPUT_SELECTOR, "audioselect", "Audio stream selector"))
    return FALSE;
  g_object_set (G_OBJECT (dvdbin->pieces[DVD_ELEM_AUD_SELECT]),
      "sync-streams", FALSE, NULL);

  if (!try_create_piece (dvdbin, DVD_ELEM_AUDDEC, NULL,
          RSN_TYPE_AUDIODEC, "auddec", "audio decoder"))
    return FALSE;

  src = gst_element_get_static_pad (dvdbin->pieces[DVD_ELEM_AUD_SELECT], "src");
  sink = gst_element_get_static_pad (dvdbin->pieces[DVD_ELEM_AUDDEC], "sink");
  if (src == NULL || sink == NULL)
    goto failed_aud_connect;
  if (GST_PAD_LINK_FAILED (gst_pad_link (src, sink)))
    goto failed_aud_connect;
  gst_object_unref (sink);
  gst_object_unref (src);
  src = sink = NULL;

  /* ghost audio munge output pad onto bin */
  src = gst_element_get_static_pad (dvdbin->pieces[DVD_ELEM_AUDDEC], "src");
  if (src == NULL)
    goto failed_aud_ghost;
  src_templ = gst_static_pad_template_get (&audio_src_template);
  dvdbin->audio_pad = gst_ghost_pad_new_from_template ("audio", src, src_templ);
  gst_object_unref (src_templ);
  if (dvdbin->audio_pad == NULL)
    goto failed_aud_ghost;
  gst_pad_set_active (dvdbin->audio_pad, TRUE);
  bctx = g_new (RsnDvdBinPadBlockCtx, 1);
  bctx->dvdbin = gst_object_ref (dvdbin);
  bctx->pad = gst_object_ref (dvdbin->audio_pad);
  bctx->pad_block_id =
      gst_pad_add_probe (src, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
      (GstPadProbeCallback) dvdbin_pad_blocked_cb, bctx, (GDestroyNotify)
      _pad_block_destroy_notify);
  gst_object_unref (src);
  src = NULL;

  if (dvdbin->video_added && (dvdbin->audio_added || dvdbin->audio_broken)
      && dvdbin->subpicture_added) {
    rsn_dvdbin_no_more_pads (dvdbin);
  }

  return TRUE;

failed_connect:
  GST_ELEMENT_ERROR (dvdbin, CORE, FAILED, (NULL),
      ("Could not connect DVD source and demuxer elements"));
  goto error_out;
failed_vidparse_connect:
  GST_ELEMENT_ERROR (dvdbin, CORE, FAILED, (NULL),
      ("Could not connect DVD video parser and video decoder"));
  goto error_out;
failed_viddec_connect:
  GST_ELEMENT_ERROR (dvdbin, CORE, FAILED, (NULL),
      ("Could not connect DVD video decoder and aspect ratio adjuster"));
  goto error_out;
failed_video_ghost:
  GST_ELEMENT_ERROR (dvdbin, CORE, FAILED, (NULL),
      ("Could not ghost video output pad"));
  goto error_out;
failed_spuq_connect:
  GST_ELEMENT_ERROR (dvdbin, CORE, FAILED, (NULL),
      ("Could not connect DVD subpicture selector and buffer elements"));
  goto error_out;
failed_spu_ghost:
  GST_ELEMENT_ERROR (dvdbin, CORE, FAILED, (NULL),
      ("Could not ghost SPU output pad"));
  goto error_out;
failed_aud_connect:
  GST_ELEMENT_ERROR (dvdbin, CORE, FAILED, (NULL),
      ("Could not connect DVD audio decoder"));
  goto error_out;
failed_aud_ghost:
  GST_ELEMENT_ERROR (dvdbin, CORE, FAILED, (NULL),
      ("Could not ghost audio output pad"));
  goto error_out;
error_out:
  if (src != NULL)
    gst_object_unref (src);
  if (sink != NULL)
    gst_object_unref (sink);
  return FALSE;
}

static void
remove_elements (RsnDvdBin * dvdbin)
{
  gint i;
  GList *tmp;

  if (dvdbin->pieces[DVD_ELEM_MQUEUE] != NULL) {
    for (tmp = dvdbin->mq_req_pads; tmp; tmp = g_list_next (tmp)) {
      gst_element_release_request_pad (dvdbin->pieces[DVD_ELEM_MQUEUE],
          GST_PAD (tmp->data));
    }
  }
  g_list_free (dvdbin->mq_req_pads);
  dvdbin->mq_req_pads = NULL;

  for (i = 0; i < DVD_ELEM_LAST; i++) {
    DVDBIN_LOCK (dvdbin);
    if (dvdbin->pieces[i] != NULL) {
      GstElement *piece = dvdbin->pieces[i];

      dvdbin->pieces[i] = NULL;
      DVDBIN_UNLOCK (dvdbin);

      gst_element_set_state (piece, GST_STATE_NULL);
      gst_bin_remove (GST_BIN (dvdbin), piece);
    } else
      DVDBIN_UNLOCK (dvdbin);
  }
  if (dvdbin->video_pad) {
    if (dvdbin->video_added)
      gst_element_remove_pad (GST_ELEMENT (dvdbin), dvdbin->video_pad);
    else
      gst_object_unref (dvdbin->video_pad);
  }
  if (dvdbin->audio_pad) {
    if (dvdbin->audio_added)
      gst_element_remove_pad (GST_ELEMENT (dvdbin), dvdbin->audio_pad);
    else
      gst_object_unref (dvdbin->audio_pad);
  }
  if (dvdbin->subpicture_pad) {
    if (dvdbin->subpicture_added)
      gst_element_remove_pad (GST_ELEMENT (dvdbin), dvdbin->subpicture_pad);
    else
      gst_object_unref (dvdbin->subpicture_pad);
  }

  dvdbin->video_added = dvdbin->audio_added = dvdbin->subpicture_added = FALSE;
  dvdbin->audio_broken = FALSE;
  dvdbin->video_pad = dvdbin->audio_pad = dvdbin->subpicture_pad = NULL;
  dvdbin->did_no_more_pads = FALSE;
}

static GstPad *
connect_thru_mq (RsnDvdBin * dvdbin, GstPad * pad)
{
  GstPad *mq_sink;
  GstPad *mq_src;
  gchar *tmp, *sinkname, *srcname;

  /* Request a pad from multiqueue, then connect this one, then
   * discover the corresponding output pad and return it */
  mq_sink = gst_element_request_pad_simple (dvdbin->pieces[DVD_ELEM_MQUEUE],
      "sink_%u");
  if (mq_sink == NULL)
    return FALSE;
  dvdbin->mq_req_pads = g_list_prepend (dvdbin->mq_req_pads, mq_sink);

  if (gst_pad_link (pad, mq_sink) != GST_PAD_LINK_OK)
    return FALSE;

  sinkname = gst_pad_get_name (mq_sink);
  tmp = sinkname + 5;
  srcname = g_strdup_printf ("src_%s", tmp);

  mq_src = gst_element_get_static_pad (dvdbin->pieces[DVD_ELEM_MQUEUE],
      srcname);

  g_free (sinkname);
  g_free (srcname);

  return mq_src;
}

static gboolean
can_sink_caps (GstElement * e, GstCaps * caps)
{
  gboolean res = FALSE;
  GstPad *sink = gst_element_get_static_pad (e, "sink");

  if (sink) {
    GstCaps *sink_caps = gst_pad_query_caps (sink, caps);
    if (sink_caps) {
      res = !gst_caps_is_empty (sink_caps);
      gst_caps_unref (sink_caps);
    }
    gst_object_unref (sink);
  }

  return res;
}

static void
demux_pad_added (GstElement * element, GstPad * pad, RsnDvdBin * dvdbin)
{
  gboolean skip_mq = FALSE;
  GstPad *mq_pad = NULL;
  GstPad *dest_pad = NULL;
  GstCaps *caps;
  GstStructure *s;

  GST_DEBUG_OBJECT (dvdbin, "New pad: %" GST_PTR_FORMAT, pad);

  caps = gst_pad_query_caps (pad, NULL);
  if (caps == NULL) {
    GST_WARNING_OBJECT (dvdbin, "NULL caps from pad %" GST_PTR_FORMAT, pad);
    return;
  }
  if (!gst_caps_is_fixed (caps)) {
    GST_WARNING_OBJECT (dvdbin, "Unfixed caps %" GST_PTR_FORMAT
        " on pad %" GST_PTR_FORMAT, caps, pad);
    gst_caps_unref (caps);
    return;
  }

  GST_DEBUG_OBJECT (dvdbin,
      "Pad %" GST_PTR_FORMAT " has caps: %" GST_PTR_FORMAT, pad, caps);

  s = gst_caps_get_structure (caps, 0);
  g_return_if_fail (s != NULL);

  if (can_sink_caps (dvdbin->pieces[DVD_ELEM_VIDPARSE], caps)) {
    GST_LOG_OBJECT (dvdbin, "Found video pad w/ caps %" GST_PTR_FORMAT, caps);
    dest_pad =
        gst_element_get_static_pad (dvdbin->pieces[DVD_ELEM_VIDPARSE], "sink");
  } else if (g_str_equal (gst_structure_get_name (s), "subpicture/x-dvd")) {
    GST_LOG_OBJECT (dvdbin, "Found subpicture pad w/ caps %" GST_PTR_FORMAT,
        caps);
    dest_pad =
        gst_element_request_pad_simple (dvdbin->pieces[DVD_ELEM_SPU_SELECT],
        "sink_%u");
    skip_mq = TRUE;
  } else if (can_sink_caps (dvdbin->pieces[DVD_ELEM_AUDDEC], caps)) {
    GST_LOG_OBJECT (dvdbin, "Found audio pad w/ caps %" GST_PTR_FORMAT, caps);
    dest_pad =
        gst_element_request_pad_simple (dvdbin->pieces[DVD_ELEM_AUD_SELECT],
        "sink_%u");
  } else {
    GstStructure *s;

    GST_DEBUG_OBJECT (dvdbin, "Ignoring unusable pad w/ caps %" GST_PTR_FORMAT,
        caps);
    gst_element_post_message (GST_ELEMENT_CAST (dvdbin),
        gst_missing_decoder_message_new (GST_ELEMENT_CAST (dvdbin), caps));

    s = gst_caps_get_structure (caps, 0);
    if (g_str_has_prefix ("video/", gst_structure_get_name (s))) {
      GST_ELEMENT_ERROR (dvdbin, STREAM, CODEC_NOT_FOUND, (NULL),
          ("No MPEG video decoder found"));
    } else {
      GST_ELEMENT_WARNING (dvdbin, STREAM, CODEC_NOT_FOUND, (NULL),
          ("No audio decoder found"));
    }
  }

  gst_caps_unref (caps);

  if (dest_pad == NULL) {
    GST_DEBUG_OBJECT (dvdbin, "Don't know how to handle pad. Ignoring");
    return;
  }

  if (skip_mq) {
    mq_pad = gst_object_ref (pad);
  } else {
    mq_pad = connect_thru_mq (dvdbin, pad);
    if (mq_pad == NULL)
      goto failed;
    GST_DEBUG_OBJECT (dvdbin, "Linking new pad %" GST_PTR_FORMAT
        " through multiqueue to %" GST_PTR_FORMAT, pad, dest_pad);
  }

  gst_pad_link (mq_pad, dest_pad);

  gst_object_unref (mq_pad);
  gst_object_unref (dest_pad);

  return;
failed:
  GST_ELEMENT_ERROR (dvdbin, CORE, FAILED, (NULL),
      ("Failed to handle new demuxer pad %s", GST_PAD_NAME (pad)));
  if (mq_pad)
    gst_object_unref (mq_pad);
  if (dest_pad)
    gst_object_unref (dest_pad);
  return;
}

static void
demux_no_more_pads (GstElement * element, RsnDvdBin * dvdbin)
{
  gboolean no_more_pads = FALSE;
  guint n_audio_pads = 0;

  GST_DEBUG_OBJECT (dvdbin, "Received no more pads from demuxer");
  DVDBIN_PREROLL_LOCK (dvdbin);

  g_object_get (dvdbin->pieces[DVD_ELEM_AUD_SELECT], "n-pads", &n_audio_pads,
      NULL);
  if (n_audio_pads == 0) {
    no_more_pads = dvdbin->video_added && dvdbin->subpicture_added;
    dvdbin->audio_broken = TRUE;
  }

  DVDBIN_PREROLL_UNLOCK (dvdbin);

  if (no_more_pads) {
    GST_DEBUG_OBJECT (dvdbin,
        "Firing no more pads from demuxer no-more-pads cb");
    rsn_dvdbin_no_more_pads (dvdbin);
  }
}

static GstPadProbeReturn
dvdbin_pad_blocked_cb (GstPad * opad,
    GstPadProbeInfo * info, RsnDvdBinPadBlockCtx * ctx)
{
  RsnDvdBin *dvdbin;
  GstPad *pad;
  gboolean added_last_pad = FALSE;
  gboolean added = FALSE;
  guint pad_block_id = 0;

  dvdbin = ctx->dvdbin;
  pad = ctx->pad;

  if (pad == dvdbin->subpicture_pad) {
    GST_DEBUG_OBJECT (opad, "Pad block -> subpicture pad");
    DVDBIN_PREROLL_LOCK (dvdbin);
    added = dvdbin->subpicture_added;
    dvdbin->subpicture_added = TRUE;

    if (!added) {
      gst_element_add_pad (GST_ELEMENT (dvdbin), dvdbin->subpicture_pad);
      added_last_pad = ((dvdbin->audio_broken || dvdbin->audio_added)
          && dvdbin->video_added);
    }
    pad_block_id = ctx->pad_block_id;
    ctx->pad_block_id = 0;
    DVDBIN_PREROLL_UNLOCK (dvdbin);

    if (pad_block_id)
      gst_pad_remove_probe (opad, pad_block_id);
  } else if (pad == dvdbin->audio_pad) {
    GST_DEBUG_OBJECT (opad, "Pad block -> audio pad");
    DVDBIN_PREROLL_LOCK (dvdbin);
    added = dvdbin->audio_added;
    dvdbin->audio_added = TRUE;

    if (!added) {
      gst_element_add_pad (GST_ELEMENT (dvdbin), dvdbin->audio_pad);
      added_last_pad = (dvdbin->subpicture_added && dvdbin->video_added);
    }
    pad_block_id = ctx->pad_block_id;
    ctx->pad_block_id = 0;
    DVDBIN_PREROLL_UNLOCK (dvdbin);

    if (pad_block_id)
      gst_pad_remove_probe (opad, pad_block_id);
  } else if (pad == dvdbin->video_pad) {
    GST_DEBUG_OBJECT (opad, "Pad block -> video pad");

    DVDBIN_PREROLL_LOCK (dvdbin);
    added = dvdbin->video_added;
    dvdbin->video_added = TRUE;

    if (!added) {
      gst_element_add_pad (GST_ELEMENT (dvdbin), dvdbin->video_pad);
      added_last_pad = (dvdbin->subpicture_added && (dvdbin->audio_added
              || dvdbin->audio_broken));
    }
    pad_block_id = ctx->pad_block_id;
    ctx->pad_block_id = 0;
    DVDBIN_PREROLL_UNLOCK (dvdbin);

    if (pad_block_id)
      gst_pad_remove_probe (opad, pad_block_id);
  }

  if (added_last_pad) {
    GST_DEBUG_OBJECT (dvdbin, "Firing no more pads from pad-blocked cb");
    rsn_dvdbin_no_more_pads (dvdbin);
  }

  return GST_PAD_PROBE_OK;
}

static void
rsn_dvdbin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  RsnDvdBin *dvdbin = RESINDVDBIN (object);

  switch (prop_id) {
    case ARG_DEVICE:
      DVDBIN_LOCK (dvdbin);
      g_free (dvdbin->device);
      if (g_value_get_string (value) == NULL)
        dvdbin->device = g_strdup (DEFAULT_DEVICE);
      else
        dvdbin->device = g_value_dup_string (value);

      if (dvdbin->pieces[DVD_ELEM_SOURCE]) {
        g_object_set_property (G_OBJECT (dvdbin->pieces[DVD_ELEM_SOURCE]),
            "device", value);
      }
      DVDBIN_UNLOCK (dvdbin);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
rsn_dvdbin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  RsnDvdBin *dvdbin = RESINDVDBIN (object);

  switch (prop_id) {
    case ARG_DEVICE:
      DVDBIN_LOCK (dvdbin);
      if (dvdbin->device)
        g_value_set_string (value, dvdbin->device);
      else if (dvdbin->pieces[DVD_ELEM_SOURCE])
        g_object_get_property (G_OBJECT (dvdbin->pieces[DVD_ELEM_SOURCE]),
            "device", value);
      else
        g_value_set_string (value, DEFAULT_DEVICE);
      DVDBIN_UNLOCK (dvdbin);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstStateChangeReturn
rsn_dvdbin_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret;
  RsnDvdBin *dvdbin = RESINDVDBIN (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!create_elements (dvdbin)) {
        remove_elements (dvdbin);
        return GST_STATE_CHANGE_FAILURE;
      }
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
    case GST_STATE_CHANGE_READY_TO_NULL:
      remove_elements (dvdbin);
      break;
    default:
      break;
  }

  return ret;
}

static gboolean
rsndvdbin_element_init (GstPlugin * plugin)
{
  gboolean result = TRUE;

  GST_DEBUG_CATEGORY_INIT (resindvd_debug, "resindvd",
      0, "DVD playback elements from resindvd");

#ifdef ENABLE_NLS
  GST_DEBUG ("binding text domain %s to locale dir %s", GETTEXT_PACKAGE,
      LOCALEDIR);
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

  result &= gst_element_register (plugin, "rsndvdbin",
      GST_RANK_PRIMARY, RESIN_TYPE_DVDBIN);

  result &= gst_flups_demux_plugin_init (plugin);

  return result;
}
