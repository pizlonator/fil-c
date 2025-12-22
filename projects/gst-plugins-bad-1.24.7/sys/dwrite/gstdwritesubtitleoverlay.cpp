/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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

#include <gst/d3d11/gstd3d11.h>
#include <gst/d3d11/gstd3d11-private.h>
#include <gst/video/video.h>
#include "gstdwritesubtitleoverlay.h"
#include "gstdwrite-utils.h"
#include "gstdwritetextoverlay.h"
#include <mutex>

GST_DEBUG_CATEGORY_STATIC (dwrite_subtitle_overlay_debug);
#define GST_CAT_DEFAULT dwrite_subtitle_overlay_debug

static GstStaticPadTemplate video_templ = GST_STATIC_PAD_TEMPLATE ("video",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw(ANY)")
    );

static GstStaticPadTemplate text_templ = GST_STATIC_PAD_TEMPLATE ("text",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("text/x-raw, format = { pango-markup, utf8 }"));

static GstStaticPadTemplate src_templ = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw(ANY)")
    );

/* *INDENT-OFF* */
static std::vector<GParamSpec *> _pspec;
/* *INDENT-ON* */

struct GstDWriteSubtitleOverlayPrivate
{
  std::mutex lock;

  GstElement *mux = nullptr;
  GstElement *overlay = nullptr;
  GstPad *text_pad = nullptr;
  GstPad *mux_pad = nullptr;
};

struct _GstDWriteSubtitleOverlay
{
  GstBin parent;

  GstDWriteSubtitleOverlayPrivate *priv;
};

static void gst_dwrite_subtitle_overlay_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_dwrite_subtitle_overlay_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static GstPadLinkReturn gst_dwrite_subtitle_overlay_text_link (GstPad * pad,
    GstObject * parent, GstPad * peer);
static void gst_dwrite_subtitle_overlay_text_unlink (GstPad * pad,
    GstObject * parent);
static gboolean gst_dwrite_subtitle_overlay_src_event (GstPad * pad,
    GstObject * parent, GstEvent * event);

#define gst_dwrite_subtitle_overlay_parent_class parent_class
G_DEFINE_TYPE (GstDWriteSubtitleOverlay, gst_dwrite_subtitle_overlay,
    GST_TYPE_BIN);

static void
gst_dwrite_subtitle_overlay_class_init (GstDWriteSubtitleOverlayClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  object_class->set_property = gst_dwrite_subtitle_overlay_set_property;
  object_class->get_property = gst_dwrite_subtitle_overlay_get_property;

  gst_dwrite_base_overlay_build_param_specs (_pspec);
  gst_dwrite_text_overlay_build_param_specs (_pspec);

  for (guint i = 0; i < _pspec.size (); i++)
    g_object_class_install_property (object_class, i + 1, _pspec[i]);

  gst_element_class_set_static_metadata (element_class,
      "DirectWrite Subtitle Overlay",
      "Filter/Editor/Video/Overlay/Subtitle",
      "Adds subtitle strings on top of a video buffer",
      "Seungha Yang <seungha@centricular.com>");

  gst_element_class_add_static_pad_template (element_class, &video_templ);
  gst_element_class_add_static_pad_template (element_class, &text_templ);
  gst_element_class_add_static_pad_template (element_class, &src_templ);

  GST_DEBUG_CATEGORY_INIT (dwrite_subtitle_overlay_debug,
      "dwritesubtitleoverlay", 0, "dwritesubtitleoverlay");
}

static void
gst_dwrite_subtitle_overlay_init (GstDWriteSubtitleOverlay * self)
{
  GstElement *elem = GST_ELEMENT_CAST (self);
  GstPad *gpad;
  GstPad *pad;
  GstPadTemplate *templ;
  GstDWriteSubtitleOverlayPrivate *priv;

  self->priv = priv = new GstDWriteSubtitleOverlayPrivate ();

  priv->mux = gst_element_factory_make ("dwritesubtitlemux", "subtitle-mux");
  priv->overlay =
      gst_element_factory_make ("dwritetextoverlay", "text-overlay");

  gst_bin_add_many (GST_BIN_CAST (self), priv->mux, priv->overlay, nullptr);
  gst_element_link (priv->mux, priv->overlay);

  pad = gst_element_get_static_pad (priv->mux, "video");
  gpad = gst_ghost_pad_new ("video", pad);
  gst_object_unref (pad);
  gst_element_add_pad (elem, gpad);

  pad = gst_element_get_static_pad (priv->overlay, "src");
  gpad = gst_ghost_pad_new ("src", pad);
  gst_object_unref (pad);
  gst_element_add_pad (elem, gpad);

  pad = GST_PAD_CAST (gst_proxy_pad_get_internal (GST_PROXY_PAD (gpad)));
  gst_pad_set_event_function (pad, gst_dwrite_subtitle_overlay_src_event);
  gst_object_unref (pad);

  templ = gst_static_pad_template_get (&text_templ);
  priv->text_pad = gst_ghost_pad_new_no_target_from_template ("text", templ);
  gst_object_unref (templ);
  gst_element_add_pad (elem, priv->text_pad);

  GST_PAD_SET_ACCEPT_INTERSECT (priv->text_pad);
  GST_PAD_SET_ACCEPT_TEMPLATE (priv->text_pad);

  gst_pad_set_link_function (priv->text_pad,
      gst_dwrite_subtitle_overlay_text_link);
  gst_pad_set_unlink_function (priv->text_pad,
      gst_dwrite_subtitle_overlay_text_unlink);
}

static void
gst_dwrite_subtitle_overlay_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDWriteSubtitleOverlay *self = GST_DWRITE_SUBTITLE_OVERLAY (object);
  GstDWriteSubtitleOverlayPrivate *priv = self->priv;

  g_object_set_property (G_OBJECT (priv->overlay), pspec->name, value);
}

static void
gst_dwrite_subtitle_overlay_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDWriteSubtitleOverlay *self = GST_DWRITE_SUBTITLE_OVERLAY (object);
  GstDWriteSubtitleOverlayPrivate *priv = self->priv;

  g_object_get_property (G_OBJECT (priv->overlay), pspec->name, value);
}

static GstPadLinkReturn
gst_dwrite_subtitle_overlay_text_link (GstPad * pad, GstObject * parent,
    GstPad * peer)
{
  GstDWriteSubtitleOverlay *self = GST_DWRITE_SUBTITLE_OVERLAY (parent);
  GstDWriteSubtitleOverlayPrivate *priv = self->priv;
  GstPad *mux_pad;

  std::lock_guard < std::mutex > lk (priv->lock);

  mux_pad = gst_element_request_pad_simple (priv->mux, "text_%u");
  if (!mux_pad) {
    GST_ERROR_OBJECT (self, "Couldn't get mux pad");
    return GST_PAD_LINK_REFUSED;
  }

  gst_ghost_pad_set_target (GST_GHOST_PAD_CAST (priv->text_pad), mux_pad);
  gst_clear_object (&priv->mux_pad);
  priv->mux_pad = mux_pad;

  GST_DEBUG_OBJECT (self, "Text pad linked");

  return GST_PAD_LINK_OK;
}

static void
gst_dwrite_subtitle_overlay_text_unlink (GstPad * pad, GstObject * parent)
{
  GstDWriteSubtitleOverlay *self = GST_DWRITE_SUBTITLE_OVERLAY (parent);
  GstDWriteSubtitleOverlayPrivate *priv = self->priv;
  std::lock_guard < std::mutex > lk (priv->lock);

  /* We cannot clear target on unlink function, since unlink function is
   * called with GST_OBJECT_LOCK and get/set target will take the lock
   * as well. Let ghostpad hold old target but it's fine */
  if (!priv->mux_pad) {
    GST_WARNING_OBJECT (self, "No linked mux pad");
  } else {
    GST_DEBUG_OBJECT (self, "Unlinking text pad");
    gst_element_release_request_pad (priv->mux, priv->mux_pad);
    gst_clear_object (&priv->mux_pad);
  }
}

static gboolean
gst_dwrite_subtitle_overlay_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  guint32 seqnum;

  /* subtitleoverlay elements will drop flush event if it was passed to text pad
   * based on the pango element's behavior, it should be dropped since
   * aggregator will forward the same flush event to text pad as well.
   * Replace flush event with ours */
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
      seqnum = gst_event_get_seqnum (event);
      gst_event_unref (event);
      event = gst_event_new_flush_start ();
      gst_event_set_seqnum (event, seqnum);
      break;
    case GST_EVENT_FLUSH_STOP:
    {
      gboolean reset;
      gst_event_parse_flush_stop (event, &reset);
      seqnum = gst_event_get_seqnum (event);
      gst_event_unref (event);
      event = gst_event_new_flush_stop (reset);
      gst_event_set_seqnum (event, seqnum);
      break;
    }
    default:
      break;
  }

  return gst_pad_event_default (pad, parent, event);
}
