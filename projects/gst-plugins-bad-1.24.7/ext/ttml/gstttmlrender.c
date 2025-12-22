/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2003> David Schleef <ds@schleef.org>
 * Copyright (C) <2006> Julien Moutte <julien@moutte.net>
 * Copyright (C) <2006> Zeeshan Ali <zeeshan.ali@nokia.com>
 * Copyright (C) <2006-2008> Tim-Philipp Müller <tim centricular net>
 * Copyright (C) <2009> Young-Ho Cha <ganadist@gmail.com>
 * Copyright (C) <2015> British Broadcasting Corporation <dash@rd.bbc.co.uk>
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
 * SECTION:element-ttmlrender
 * @title: ttmlrender
 *
 * Renders timed text on top of a video stream. It receives text in buffers
 * from a ttmlparse element; each text string is in its own #GstMemory within
 * the GstBuffer, and the styling and layout associated with each text string
 * is in metadata attached to the #GstBuffer.
 *
 * ## Example launch lines
 * |[
 * gst-launch-1.0 filesrc location=<media file location> ! video/quicktime ! qtdemux name=q ttmlrender name=r q. ! queue ! h264parse ! avdec_h264 ! autovideoconvert ! r.video_sink filesrc location=<subtitle file location> blocksize=16777216 ! queue ! ttmlparse ! r.text_sink r. ! ximagesink q. ! queue ! aacparse ! avdec_aac ! audioconvert ! alsasink
 * ]| Parse and render TTML subtitles contained in a single XML file over an
 * MP4 stream containing H.264 video and AAC audio:
 *
 */

#include <gst/video/video.h>
#include <gst/video/gstvideometa.h>
#include <gst/video/video-overlay-composition.h>
#include <pango/pangocairo.h>

#include <string.h>
#include <math.h>

#include "gstttmlelements.h"
#include "gstttmlrender.h"
#include "subtitle.h"
#include "subtitlemeta.h"

#define VIDEO_FORMATS GST_VIDEO_OVERLAY_COMPOSITION_BLEND_FORMATS

#define TTML_RENDER_CAPS GST_VIDEO_CAPS_MAKE (VIDEO_FORMATS)

#define TTML_RENDER_ALL_CAPS TTML_RENDER_CAPS ";" \
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("ANY", GST_VIDEO_FORMATS_ANY)

GST_DEBUG_CATEGORY_EXTERN (ttmlrender_debug);
#define GST_CAT_DEFAULT ttmlrender_debug

static GstStaticCaps sw_template_caps = GST_STATIC_CAPS (TTML_RENDER_CAPS);

static GstStaticPadTemplate src_template_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TTML_RENDER_ALL_CAPS)
    );

static GstStaticPadTemplate video_sink_template_factory =
GST_STATIC_PAD_TEMPLATE ("video_sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TTML_RENDER_ALL_CAPS)
    );

static GstStaticPadTemplate text_sink_template_factory =
GST_STATIC_PAD_TEMPLATE ("text_sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("text/x-raw(meta:GstSubtitleMeta)")
    );

#define GST_TTML_RENDER_GET_LOCK(ov) (&GST_TTML_RENDER (ov)->lock)
#define GST_TTML_RENDER_GET_COND(ov) (&GST_TTML_RENDER (ov)->cond)
#define GST_TTML_RENDER_LOCK(ov)     (g_mutex_lock (GST_TTML_RENDER_GET_LOCK (ov)))
#define GST_TTML_RENDER_UNLOCK(ov)   (g_mutex_unlock (GST_TTML_RENDER_GET_LOCK (ov)))
#define GST_TTML_RENDER_WAIT(ov)     (g_cond_wait (GST_TTML_RENDER_GET_COND (ov), GST_TTML_RENDER_GET_LOCK (ov)))
#define GST_TTML_RENDER_SIGNAL(ov)   (g_cond_signal (GST_TTML_RENDER_GET_COND (ov)))
#define GST_TTML_RENDER_BROADCAST(ov)(g_cond_broadcast (GST_TTML_RENDER_GET_COND (ov)))


typedef enum
{
  GST_TTML_DIRECTION_INLINE,
  GST_TTML_DIRECTION_BLOCK
} GstTtmlDirection;


typedef struct
{
  guint line_height;
  guint baseline_offset;
} BlockMetrics;


typedef struct
{
  guint height;
  guint baseline;
} FontMetrics;


typedef struct
{
  guint first_index;
  guint last_index;
} CharRange;


/* @pango_font_size is the font size you would need to tell pango in order that
 * the actual rendered height of @text matches the text height in @element's
 * style set. */
typedef struct
{
  GstSubtitleElement *element;
  guint pango_font_size;
  FontMetrics pango_font_metrics;
  gchar *text;
} UnifiedElement;


typedef struct
{
  GPtrArray *unified_elements;
  GstSubtitleStyleSet *style_set;
  gchar *joined_text;
} UnifiedBlock;


static GstElementClass *parent_class = NULL;
static void gst_ttml_render_base_init (gpointer g_class);
static void gst_ttml_render_class_init (GstTtmlRenderClass * klass);
static void gst_ttml_render_init (GstTtmlRender * render,
    GstTtmlRenderClass * klass);

static GstStateChangeReturn gst_ttml_render_change_state (GstElement *
    element, GstStateChange transition);

static GstCaps *gst_ttml_render_get_videosink_caps (GstPad * pad,
    GstTtmlRender * render, GstCaps * filter);
static GstCaps *gst_ttml_render_get_src_caps (GstPad * pad,
    GstTtmlRender * render, GstCaps * filter);
static gboolean gst_ttml_render_setcaps (GstTtmlRender * render,
    GstCaps * caps);
static gboolean gst_ttml_render_src_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static gboolean gst_ttml_render_src_query (GstPad * pad,
    GstObject * parent, GstQuery * query);

static gboolean gst_ttml_render_video_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static gboolean gst_ttml_render_video_query (GstPad * pad,
    GstObject * parent, GstQuery * query);
static GstFlowReturn gst_ttml_render_video_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buffer);

static gboolean gst_ttml_render_text_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static GstFlowReturn gst_ttml_render_text_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buffer);
static GstPadLinkReturn gst_ttml_render_text_pad_link (GstPad * pad,
    GstObject * parent, GstPad * peer);
static void gst_ttml_render_text_pad_unlink (GstPad * pad, GstObject * parent);
static void gst_ttml_render_pop_text (GstTtmlRender * render);

static void gst_ttml_render_finalize (GObject * object);

static gboolean gst_ttml_render_can_handle_caps (GstCaps * incaps);

static GstTtmlRenderRenderedImage *gst_ttml_render_rendered_image_new
    (GstBuffer * image, gint x, gint y, guint width, guint height);
static GstTtmlRenderRenderedImage *gst_ttml_render_rendered_image_new_empty
    (void);
static GstTtmlRenderRenderedImage *gst_ttml_render_rendered_image_copy
    (GstTtmlRenderRenderedImage * image);
static void gst_ttml_render_rendered_image_free
    (GstTtmlRenderRenderedImage * image);
static GstTtmlRenderRenderedImage *gst_ttml_render_rendered_image_combine
    (GstTtmlRenderRenderedImage * image1, GstTtmlRenderRenderedImage * image2);
static GstTtmlRenderRenderedImage *gst_ttml_render_stitch_images (GPtrArray *
    images, GstTtmlDirection direction);

static gboolean gst_ttml_render_color_is_transparent (GstSubtitleColor * color);
static gboolean gst_element_ttmlrender_init (GstPlugin * plugin);

GType
gst_ttml_render_get_type (void)
{
  static GType type = 0;

  if (g_once_init_enter ((gsize *) & type)) {
    static const GTypeInfo info = {
      sizeof (GstTtmlRenderClass),
      (GBaseInitFunc) gst_ttml_render_base_init,
      NULL,
      (GClassInitFunc) gst_ttml_render_class_init,
      NULL,
      NULL,
      sizeof (GstTtmlRender),
      0,
      (GInstanceInitFunc) gst_ttml_render_init,
    };

    g_once_init_leave ((gsize *) & type,
        g_type_register_static (GST_TYPE_ELEMENT, "GstTtmlRender", &info, 0));
  }

  return type;
}

GST_ELEMENT_REGISTER_DEFINE_CUSTOM (ttmlrender, gst_element_ttmlrender_init);

static void
gst_ttml_render_base_init (gpointer g_class)
{
  GstTtmlRenderClass *klass = GST_TTML_RENDER_CLASS (g_class);
  PangoFontMap *fontmap;

  /* Only lock for the subclasses here, the base class
   * doesn't have this mutex yet and it's not necessary
   * here */
  if (klass->pango_lock)
    g_mutex_lock (klass->pango_lock);
  fontmap = pango_cairo_font_map_get_default ();
  klass->pango_context =
      pango_font_map_create_context (PANGO_FONT_MAP (fontmap));
  if (klass->pango_lock)
    g_mutex_unlock (klass->pango_lock);
}

static void
gst_ttml_render_class_init (GstTtmlRenderClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = gst_ttml_render_finalize;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&video_sink_template_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&text_sink_template_factory));

  gst_element_class_set_static_metadata (gstelement_class,
      "TTML subtitle renderer", "Overlay/Subtitle",
      "Renders timed-text subtitles on top of video buffers",
      "David Schleef <ds@schleef.org>, Zeeshan Ali <zeeshan.ali@nokia.com>, "
      "Chris Bass <dash@rd.bbc.co.uk>");

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_ttml_render_change_state);

  klass->pango_lock = g_new (GMutex, 1);
  g_mutex_init (klass->pango_lock);
}

static void
gst_ttml_render_finalize (GObject * object)
{
  GstTtmlRender *render = GST_TTML_RENDER (object);

  if (render->compositions) {
    g_list_free_full (render->compositions,
        (GDestroyNotify) gst_video_overlay_composition_unref);
    render->compositions = NULL;
  }

  if (render->text_buffer) {
    gst_buffer_unref (render->text_buffer);
    render->text_buffer = NULL;
  }

  if (render->layout) {
    g_object_unref (render->layout);
    render->layout = NULL;
  }

  g_mutex_clear (&render->lock);
  g_cond_clear (&render->cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_ttml_render_init (GstTtmlRender * render, GstTtmlRenderClass * klass)
{
  GstPadTemplate *template;

  /* video sink */
  template = gst_static_pad_template_get (&video_sink_template_factory);
  render->video_sinkpad = gst_pad_new_from_template (template, "video_sink");
  gst_object_unref (template);
  gst_pad_set_event_function (render->video_sinkpad,
      GST_DEBUG_FUNCPTR (gst_ttml_render_video_event));
  gst_pad_set_chain_function (render->video_sinkpad,
      GST_DEBUG_FUNCPTR (gst_ttml_render_video_chain));
  gst_pad_set_query_function (render->video_sinkpad,
      GST_DEBUG_FUNCPTR (gst_ttml_render_video_query));
  GST_PAD_SET_PROXY_ALLOCATION (render->video_sinkpad);
  gst_element_add_pad (GST_ELEMENT (render), render->video_sinkpad);

  template =
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass),
      "text_sink");
  if (template) {
    /* text sink */
    render->text_sinkpad = gst_pad_new_from_template (template, "text_sink");

    gst_pad_set_event_function (render->text_sinkpad,
        GST_DEBUG_FUNCPTR (gst_ttml_render_text_event));
    gst_pad_set_chain_function (render->text_sinkpad,
        GST_DEBUG_FUNCPTR (gst_ttml_render_text_chain));
    gst_pad_set_link_function (render->text_sinkpad,
        GST_DEBUG_FUNCPTR (gst_ttml_render_text_pad_link));
    gst_pad_set_unlink_function (render->text_sinkpad,
        GST_DEBUG_FUNCPTR (gst_ttml_render_text_pad_unlink));
    gst_element_add_pad (GST_ELEMENT (render), render->text_sinkpad);
  }

  /* (video) source */
  template = gst_static_pad_template_get (&src_template_factory);
  render->srcpad = gst_pad_new_from_template (template, "src");
  gst_object_unref (template);
  gst_pad_set_event_function (render->srcpad,
      GST_DEBUG_FUNCPTR (gst_ttml_render_src_event));
  gst_pad_set_query_function (render->srcpad,
      GST_DEBUG_FUNCPTR (gst_ttml_render_src_query));
  gst_element_add_pad (GST_ELEMENT (render), render->srcpad);

  g_mutex_lock (GST_TTML_RENDER_GET_CLASS (render)->pango_lock);

  render->wait_text = TRUE;
  render->need_render = TRUE;
  render->text_buffer = NULL;
  render->text_linked = FALSE;

  render->compositions = NULL;
  render->layout =
      pango_layout_new (GST_TTML_RENDER_GET_CLASS (render)->pango_context);

  g_mutex_init (&render->lock);
  g_cond_init (&render->cond);
  gst_segment_init (&render->segment, GST_FORMAT_TIME);
  g_mutex_unlock (GST_TTML_RENDER_GET_CLASS (render)->pango_lock);
}


/* only negotiate/query video render composition support for now */
static gboolean
gst_ttml_render_negotiate (GstTtmlRender * render, GstCaps * caps)
{
  GstQuery *query;
  gboolean attach = FALSE;
  gboolean caps_has_meta = TRUE;
  gboolean ret;
  GstCapsFeatures *f;
  GstCaps *original_caps;
  gboolean original_has_meta = FALSE;
  gboolean allocation_ret = TRUE;

  GST_DEBUG_OBJECT (render, "performing negotiation");

  gst_pad_check_reconfigure (render->srcpad);

  if (!caps)
    caps = gst_pad_get_current_caps (render->video_sinkpad);
  else
    gst_caps_ref (caps);

  if (!caps || gst_caps_is_empty (caps))
    goto no_format;

  original_caps = caps;

  /* Try to use the render meta if possible */
  f = gst_caps_get_features (caps, 0);

  /* if the caps doesn't have the render meta, we query if downstream
   * accepts it before trying the version without the meta
   * If upstream already is using the meta then we can only use it */
  if (!f
      || !gst_caps_features_contains (f,
          GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION)) {
    GstCaps *overlay_caps;

    /* In this case we added the meta, but we can work without it
     * so preserve the original caps so we can use it as a fallback */
    overlay_caps = gst_caps_copy (caps);

    f = gst_caps_get_features (overlay_caps, 0);
    gst_caps_features_add (f,
        GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION);

    ret = gst_pad_peer_query_accept_caps (render->srcpad, overlay_caps);
    GST_DEBUG_OBJECT (render, "Downstream accepts the render meta: %d", ret);
    if (ret) {
      gst_caps_unref (caps);
      caps = overlay_caps;

    } else {
      /* fallback to the original */
      gst_caps_unref (overlay_caps);
      caps_has_meta = FALSE;
    }
  } else {
    original_has_meta = TRUE;
  }
  GST_DEBUG_OBJECT (render, "Using caps %" GST_PTR_FORMAT, caps);
  ret = gst_pad_set_caps (render->srcpad, caps);

  if (ret) {
    /* find supported meta */
    query = gst_query_new_allocation (caps, FALSE);

    if (!gst_pad_peer_query (render->srcpad, query)) {
      /* no problem, we use the query defaults */
      GST_DEBUG_OBJECT (render, "ALLOCATION query failed");
      allocation_ret = FALSE;
    }

    if (caps_has_meta && gst_query_find_allocation_meta (query,
            GST_VIDEO_OVERLAY_COMPOSITION_META_API_TYPE, NULL))
      attach = TRUE;

    gst_query_unref (query);
  }

  if (!allocation_ret && render->video_flushing) {
    ret = FALSE;
  } else if (original_caps && !original_has_meta && !attach) {
    if (caps_has_meta) {
      /* Some elements (fakesink) claim to accept the meta on caps but won't
         put it in the allocation query result, this leads below
         check to fail. Prevent this by removing the meta from caps */
      gst_caps_unref (caps);
      caps = gst_caps_ref (original_caps);
      ret = gst_pad_set_caps (render->srcpad, caps);
      if (ret && !gst_ttml_render_can_handle_caps (caps))
        ret = FALSE;
    }
  }

  if (!ret) {
    GST_DEBUG_OBJECT (render, "negotiation failed, schedule reconfigure");
    gst_pad_mark_reconfigure (render->srcpad);
  }

  gst_caps_unref (caps);

  if (!ret)
    gst_pad_mark_reconfigure (render->srcpad);

  return ret;

no_format:
  {
    if (caps)
      gst_caps_unref (caps);
    gst_pad_mark_reconfigure (render->srcpad);
    return FALSE;
  }
}

static gboolean
gst_ttml_render_can_handle_caps (GstCaps * incaps)
{
  gboolean ret;
  GstCaps *caps;
  static GstStaticCaps static_caps = GST_STATIC_CAPS (TTML_RENDER_CAPS);

  caps = gst_static_caps_get (&static_caps);
  ret = gst_caps_is_subset (incaps, caps);
  gst_caps_unref (caps);

  return ret;
}

static gboolean
gst_ttml_render_setcaps (GstTtmlRender * render, GstCaps * caps)
{
  GstVideoInfo info;
  gboolean ret = FALSE;

  if (!gst_video_info_from_caps (&info, caps))
    goto invalid_caps;

  render->info = info;
  render->format = GST_VIDEO_INFO_FORMAT (&info);
  render->width = GST_VIDEO_INFO_WIDTH (&info);
  render->height = GST_VIDEO_INFO_HEIGHT (&info);

  ret = gst_ttml_render_negotiate (render, caps);

  GST_TTML_RENDER_LOCK (render);
  g_mutex_lock (GST_TTML_RENDER_GET_CLASS (render)->pango_lock);
  if (!gst_ttml_render_can_handle_caps (caps)) {
    GST_DEBUG_OBJECT (render, "unsupported caps %" GST_PTR_FORMAT, caps);
    ret = FALSE;
  }

  g_mutex_unlock (GST_TTML_RENDER_GET_CLASS (render)->pango_lock);
  GST_TTML_RENDER_UNLOCK (render);

  return ret;

  /* ERRORS */
invalid_caps:
  {
    GST_DEBUG_OBJECT (render, "could not parse caps");
    return FALSE;
  }
}


static gboolean
gst_ttml_render_src_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  gboolean ret = FALSE;
  GstTtmlRender *render;

  render = GST_TTML_RENDER (parent);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstCaps *filter, *caps;

      gst_query_parse_caps (query, &filter);
      caps = gst_ttml_render_get_src_caps (pad, render, filter);
      gst_query_set_caps_result (query, caps);
      gst_caps_unref (caps);
      ret = TRUE;
      break;
    }
    default:
      ret = gst_pad_query_default (pad, parent, query);
      break;
  }

  return ret;
}

static gboolean
gst_ttml_render_src_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstTtmlRender *render;
  gboolean ret;

  render = GST_TTML_RENDER (parent);

  if (render->text_linked) {
    gst_event_ref (event);
    ret = gst_pad_push_event (render->video_sinkpad, event);
    gst_pad_push_event (render->text_sinkpad, event);
  } else {
    ret = gst_pad_push_event (render->video_sinkpad, event);
  }

  return ret;
}

/**
 * gst_ttml_render_add_feature_and_intersect:
 *
 * Creates a new #GstCaps containing the (given caps +
 * given caps feature) + (given caps intersected by the
 * given filter).
 *
 * Returns: the new #GstCaps
 */
static GstCaps *
gst_ttml_render_add_feature_and_intersect (GstCaps * caps,
    const gchar * feature, GstCaps * filter)
{
  int i, caps_size;
  GstCaps *new_caps;

  new_caps = gst_caps_copy (caps);

  caps_size = gst_caps_get_size (new_caps);
  for (i = 0; i < caps_size; i++) {
    GstCapsFeatures *features = gst_caps_get_features (new_caps, i);

    if (!gst_caps_features_is_any (features)) {
      gst_caps_features_add (features, feature);
    }
  }

  gst_caps_append (new_caps, gst_caps_intersect_full (caps,
          filter, GST_CAPS_INTERSECT_FIRST));

  return new_caps;
}

/**
 * gst_ttml_render_intersect_by_feature:
 *
 * Creates a new #GstCaps based on the following filtering rule.
 *
 * For each individual caps contained in given caps, if the
 * caps uses the given caps feature, keep a version of the caps
 * with the feature and an another one without. Otherwise, intersect
 * the caps with the given filter.
 *
 * Returns: the new #GstCaps
 */
static GstCaps *
gst_ttml_render_intersect_by_feature (GstCaps * caps,
    const gchar * feature, GstCaps * filter)
{
  int i, caps_size;
  GstCaps *new_caps;

  new_caps = gst_caps_new_empty ();

  caps_size = gst_caps_get_size (caps);
  for (i = 0; i < caps_size; i++) {
    GstStructure *caps_structure = gst_caps_get_structure (caps, i);
    GstCapsFeatures *caps_features =
        gst_caps_features_copy (gst_caps_get_features (caps, i));
    GstCaps *filtered_caps;
    GstCaps *simple_caps =
        gst_caps_new_full (gst_structure_copy (caps_structure), NULL);
    gst_caps_set_features (simple_caps, 0, caps_features);

    if (gst_caps_features_contains (caps_features, feature)) {
      gst_caps_append (new_caps, gst_caps_copy (simple_caps));

      gst_caps_features_remove (caps_features, feature);
      filtered_caps = gst_caps_ref (simple_caps);
    } else {
      filtered_caps = gst_caps_intersect_full (simple_caps, filter,
          GST_CAPS_INTERSECT_FIRST);
    }

    gst_caps_unref (simple_caps);
    gst_caps_append (new_caps, filtered_caps);
  }

  return new_caps;
}

static GstCaps *
gst_ttml_render_get_videosink_caps (GstPad * pad,
    GstTtmlRender * render, GstCaps * filter)
{
  GstPad *srcpad = render->srcpad;
  GstCaps *peer_caps = NULL, *caps = NULL, *overlay_filter = NULL;

  if (G_UNLIKELY (!render))
    return gst_pad_get_pad_template_caps (pad);

  if (filter) {
    /* filter caps + composition feature + filter caps
     * filtered by the software caps. */
    GstCaps *sw_caps = gst_static_caps_get (&sw_template_caps);
    overlay_filter = gst_ttml_render_add_feature_and_intersect (filter,
        GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION, sw_caps);
    gst_caps_unref (sw_caps);

    GST_DEBUG_OBJECT (render, "render filter %" GST_PTR_FORMAT, overlay_filter);
  }

  peer_caps = gst_pad_peer_query_caps (srcpad, overlay_filter);

  if (overlay_filter)
    gst_caps_unref (overlay_filter);

  if (peer_caps) {

    GST_DEBUG_OBJECT (pad, "peer caps  %" GST_PTR_FORMAT, peer_caps);

    if (gst_caps_is_any (peer_caps)) {
      /* if peer returns ANY caps, return filtered src pad template caps */
      caps = gst_caps_copy (gst_pad_get_pad_template_caps (srcpad));
    } else {

      /* duplicate caps which contains the composition into one version with
       * the meta and one without. Filter the other caps by the software caps */
      GstCaps *sw_caps = gst_static_caps_get (&sw_template_caps);
      caps = gst_ttml_render_intersect_by_feature (peer_caps,
          GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION, sw_caps);
      gst_caps_unref (sw_caps);
    }

    gst_caps_unref (peer_caps);

  } else {
    /* no peer, our padtemplate is enough then */
    caps = gst_pad_get_pad_template_caps (pad);
  }

  if (filter) {
    GstCaps *intersection = gst_caps_intersect_full (filter, caps,
        GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
    caps = intersection;
  }

  GST_DEBUG_OBJECT (render, "returning  %" GST_PTR_FORMAT, caps);

  return caps;
}

static GstCaps *
gst_ttml_render_get_src_caps (GstPad * pad, GstTtmlRender * render,
    GstCaps * filter)
{
  GstPad *sinkpad = render->video_sinkpad;
  GstCaps *peer_caps = NULL, *caps = NULL, *overlay_filter = NULL;

  if (G_UNLIKELY (!render))
    return gst_pad_get_pad_template_caps (pad);

  if (filter) {
    /* duplicate filter caps which contains the composition into one version
     * with the meta and one without. Filter the other caps by the software
     * caps */
    GstCaps *sw_caps = gst_static_caps_get (&sw_template_caps);
    overlay_filter =
        gst_ttml_render_intersect_by_feature (filter,
        GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION, sw_caps);
    gst_caps_unref (sw_caps);
  }

  peer_caps = gst_pad_peer_query_caps (sinkpad, overlay_filter);

  if (overlay_filter)
    gst_caps_unref (overlay_filter);

  if (peer_caps) {

    GST_DEBUG_OBJECT (pad, "peer caps  %" GST_PTR_FORMAT, peer_caps);

    if (gst_caps_is_any (peer_caps)) {

      /* if peer returns ANY caps, return filtered sink pad template caps */
      caps = gst_caps_copy (gst_pad_get_pad_template_caps (sinkpad));

    } else {

      /* return upstream caps + composition feature + upstream caps
       * filtered by the software caps. */
      GstCaps *sw_caps = gst_static_caps_get (&sw_template_caps);
      caps = gst_ttml_render_add_feature_and_intersect (peer_caps,
          GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION, sw_caps);
      gst_caps_unref (sw_caps);
    }

    gst_caps_unref (peer_caps);

  } else {
    /* no peer, our padtemplate is enough then */
    caps = gst_pad_get_pad_template_caps (pad);
  }

  if (filter) {
    GstCaps *intersection;

    intersection =
        gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
    caps = intersection;
  }
  GST_DEBUG_OBJECT (render, "returning  %" GST_PTR_FORMAT, caps);

  return caps;
}


static GstFlowReturn
gst_ttml_render_push_frame (GstTtmlRender * render, GstBuffer * video_frame)
{
  GstVideoFrame frame;
  GList *compositions = render->compositions;

  if (compositions == NULL) {
    GST_CAT_DEBUG (ttmlrender_debug, "No compositions.");
    goto done;
  }

  if (gst_pad_check_reconfigure (render->srcpad)) {
    if (!gst_ttml_render_negotiate (render, NULL)) {
      gst_pad_mark_reconfigure (render->srcpad);
      gst_buffer_unref (video_frame);
      if (GST_PAD_IS_FLUSHING (render->srcpad))
        return GST_FLOW_FLUSHING;
      else
        return GST_FLOW_NOT_NEGOTIATED;
    }
  }

  video_frame = gst_buffer_make_writable (video_frame);

  if (!gst_video_frame_map (&frame, &render->info, video_frame,
          GST_MAP_READWRITE))
    goto invalid_frame;

  while (compositions) {
    GstVideoOverlayComposition *composition = compositions->data;
    gst_video_overlay_composition_blend (composition, &frame);
    compositions = compositions->next;
  }

  gst_video_frame_unmap (&frame);

done:

  return gst_pad_push (render->srcpad, video_frame);

  /* ERRORS */
invalid_frame:
  {
    gst_buffer_unref (video_frame);
    GST_DEBUG_OBJECT (render, "received invalid buffer");
    return GST_FLOW_OK;
  }
}

static GstPadLinkReturn
gst_ttml_render_text_pad_link (GstPad * pad, GstObject * parent, GstPad * peer)
{
  GstTtmlRender *render;

  render = GST_TTML_RENDER (parent);
  if (G_UNLIKELY (!render))
    return GST_PAD_LINK_REFUSED;

  GST_DEBUG_OBJECT (render, "Text pad linked");

  render->text_linked = TRUE;

  return GST_PAD_LINK_OK;
}

static void
gst_ttml_render_text_pad_unlink (GstPad * pad, GstObject * parent)
{
  GstTtmlRender *render;

  /* don't use gst_pad_get_parent() here, will deadlock */
  render = GST_TTML_RENDER (parent);

  GST_DEBUG_OBJECT (render, "Text pad unlinked");

  render->text_linked = FALSE;

  gst_segment_init (&render->text_segment, GST_FORMAT_UNDEFINED);
}

static gboolean
gst_ttml_render_text_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean ret = FALSE;
  GstTtmlRender *render = NULL;

  render = GST_TTML_RENDER (parent);

  GST_LOG_OBJECT (pad, "received event %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEGMENT:
    {
      const GstSegment *segment;

      render->text_eos = FALSE;

      gst_event_parse_segment (event, &segment);

      if (segment->format == GST_FORMAT_TIME) {
        GST_TTML_RENDER_LOCK (render);
        gst_segment_copy_into (segment, &render->text_segment);
        GST_DEBUG_OBJECT (render, "TEXT SEGMENT now: %" GST_SEGMENT_FORMAT,
            &render->text_segment);
        GST_TTML_RENDER_UNLOCK (render);
      } else {
        GST_ELEMENT_WARNING (render, STREAM, MUX, (NULL),
            ("received non-TIME newsegment event on text input"));
      }

      gst_event_unref (event);
      ret = TRUE;

      /* wake up the video chain, it might be waiting for a text buffer or
       * a text segment update */
      GST_TTML_RENDER_LOCK (render);
      GST_TTML_RENDER_BROADCAST (render);
      GST_TTML_RENDER_UNLOCK (render);
      break;
    }
    case GST_EVENT_GAP:
    {
      GstClockTime start, duration;

      gst_event_parse_gap (event, &start, &duration);
      if (GST_CLOCK_TIME_IS_VALID (duration))
        start += duration;
      /* we do not expect another buffer until after gap,
       * so that is our position now */
      render->text_segment.position = start;

      /* wake up the video chain, it might be waiting for a text buffer or
       * a text segment update */
      GST_TTML_RENDER_LOCK (render);
      GST_TTML_RENDER_BROADCAST (render);
      GST_TTML_RENDER_UNLOCK (render);

      gst_event_unref (event);
      ret = TRUE;
      break;
    }
    case GST_EVENT_FLUSH_STOP:
      GST_TTML_RENDER_LOCK (render);
      GST_INFO_OBJECT (render, "text flush stop");
      render->text_flushing = FALSE;
      render->text_eos = FALSE;
      gst_ttml_render_pop_text (render);
      gst_segment_init (&render->text_segment, GST_FORMAT_TIME);
      GST_TTML_RENDER_UNLOCK (render);
      gst_event_unref (event);
      ret = TRUE;
      break;
    case GST_EVENT_FLUSH_START:
      GST_TTML_RENDER_LOCK (render);
      GST_INFO_OBJECT (render, "text flush start");
      render->text_flushing = TRUE;
      GST_TTML_RENDER_BROADCAST (render);
      GST_TTML_RENDER_UNLOCK (render);
      gst_event_unref (event);
      ret = TRUE;
      break;
    case GST_EVENT_EOS:
      GST_TTML_RENDER_LOCK (render);
      render->text_eos = TRUE;
      GST_INFO_OBJECT (render, "text EOS");
      /* wake up the video chain, it might be waiting for a text buffer or
       * a text segment update */
      GST_TTML_RENDER_BROADCAST (render);
      GST_TTML_RENDER_UNLOCK (render);
      gst_event_unref (event);
      ret = TRUE;
      break;
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }

  return ret;
}

static gboolean
gst_ttml_render_video_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean ret = FALSE;
  GstTtmlRender *render = NULL;

  render = GST_TTML_RENDER (parent);

  GST_DEBUG_OBJECT (pad, "received event %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;
      gint prev_width = render->width;
      gint prev_height = render->height;

      gst_event_parse_caps (event, &caps);
      ret = gst_ttml_render_setcaps (render, caps);
      if (render->width != prev_width || render->height != prev_height)
        render->need_render = TRUE;
      gst_event_unref (event);
      break;
    }
    case GST_EVENT_SEGMENT:
    {
      const GstSegment *segment;

      GST_DEBUG_OBJECT (render, "received new segment");

      gst_event_parse_segment (event, &segment);

      if (segment->format == GST_FORMAT_TIME) {
        GST_DEBUG_OBJECT (render, "VIDEO SEGMENT now: %" GST_SEGMENT_FORMAT,
            &render->segment);

        gst_segment_copy_into (segment, &render->segment);
      } else {
        GST_ELEMENT_WARNING (render, STREAM, MUX, (NULL),
            ("received non-TIME newsegment event on video input"));
      }

      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
    case GST_EVENT_EOS:
      GST_TTML_RENDER_LOCK (render);
      GST_INFO_OBJECT (render, "video EOS");
      render->video_eos = TRUE;
      GST_TTML_RENDER_UNLOCK (render);
      ret = gst_pad_event_default (pad, parent, event);
      break;
    case GST_EVENT_FLUSH_START:
      GST_TTML_RENDER_LOCK (render);
      GST_INFO_OBJECT (render, "video flush start");
      render->video_flushing = TRUE;
      GST_TTML_RENDER_BROADCAST (render);
      GST_TTML_RENDER_UNLOCK (render);
      ret = gst_pad_event_default (pad, parent, event);
      break;
    case GST_EVENT_FLUSH_STOP:
      GST_TTML_RENDER_LOCK (render);
      GST_INFO_OBJECT (render, "video flush stop");
      render->video_flushing = FALSE;
      render->video_eos = FALSE;
      gst_segment_init (&render->segment, GST_FORMAT_TIME);
      GST_TTML_RENDER_UNLOCK (render);
      ret = gst_pad_event_default (pad, parent, event);
      break;
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }

  return ret;
}

static gboolean
gst_ttml_render_video_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  gboolean ret = FALSE;
  GstTtmlRender *render;

  render = GST_TTML_RENDER (parent);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstCaps *filter, *caps;

      gst_query_parse_caps (query, &filter);
      caps = gst_ttml_render_get_videosink_caps (pad, render, filter);
      gst_query_set_caps_result (query, caps);
      gst_caps_unref (caps);
      ret = TRUE;
      break;
    }
    default:
      ret = gst_pad_query_default (pad, parent, query);
      break;
  }

  return ret;
}

/* Called with lock held */
static void
gst_ttml_render_pop_text (GstTtmlRender * render)
{
  g_return_if_fail (GST_IS_TTML_RENDER (render));

  if (render->text_buffer) {
    GST_DEBUG_OBJECT (render, "releasing text buffer %p", render->text_buffer);
    gst_buffer_unref (render->text_buffer);
    render->text_buffer = NULL;
  }

  /* Let the text task know we used that buffer */
  GST_TTML_RENDER_BROADCAST (render);
}

/* We receive text buffers here. If they are out of segment we just ignore them.
   If the buffer is in our segment we keep it internally except if another one
   is already waiting here, in that case we wait that it gets kicked out */
static GstFlowReturn
gst_ttml_render_text_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstTtmlRender *render = NULL;
  gboolean in_seg = FALSE;
  guint64 clip_start = 0, clip_stop = 0;

  render = GST_TTML_RENDER (parent);

  GST_TTML_RENDER_LOCK (render);

  if (render->text_flushing) {
    GST_TTML_RENDER_UNLOCK (render);
    ret = GST_FLOW_FLUSHING;
    GST_LOG_OBJECT (render, "text flushing");
    goto beach;
  }

  if (render->text_eos) {
    GST_TTML_RENDER_UNLOCK (render);
    ret = GST_FLOW_EOS;
    GST_LOG_OBJECT (render, "text EOS");
    goto beach;
  }

  GST_LOG_OBJECT (render, "%" GST_SEGMENT_FORMAT "  BUFFER: ts=%"
      GST_TIME_FORMAT ", end=%" GST_TIME_FORMAT, &render->segment,
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)),
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer) +
          GST_BUFFER_DURATION (buffer)));

  if (G_LIKELY (GST_BUFFER_TIMESTAMP_IS_VALID (buffer))) {
    GstClockTime stop;

    if (G_LIKELY (GST_BUFFER_DURATION_IS_VALID (buffer)))
      stop = GST_BUFFER_TIMESTAMP (buffer) + GST_BUFFER_DURATION (buffer);
    else
      stop = GST_CLOCK_TIME_NONE;

    in_seg = gst_segment_clip (&render->text_segment, GST_FORMAT_TIME,
        GST_BUFFER_TIMESTAMP (buffer), stop, &clip_start, &clip_stop);
  } else {
    in_seg = TRUE;
  }

  if (in_seg) {
    if (GST_BUFFER_TIMESTAMP_IS_VALID (buffer))
      GST_BUFFER_TIMESTAMP (buffer) = clip_start;
    else if (GST_BUFFER_DURATION_IS_VALID (buffer))
      GST_BUFFER_DURATION (buffer) = clip_stop - clip_start;

    /* Wait for the previous buffer to go away */
    while (render->text_buffer != NULL) {
      GST_DEBUG ("Pad %s:%s has a buffer queued, waiting",
          GST_DEBUG_PAD_NAME (pad));
      GST_TTML_RENDER_WAIT (render);
      GST_DEBUG ("Pad %s:%s resuming", GST_DEBUG_PAD_NAME (pad));
      if (render->text_flushing) {
        GST_TTML_RENDER_UNLOCK (render);
        ret = GST_FLOW_FLUSHING;
        goto beach;
      }
    }

    if (GST_BUFFER_TIMESTAMP_IS_VALID (buffer))
      render->text_segment.position = clip_start;

    render->text_buffer = buffer;
    /* That's a new text buffer we need to render */
    render->need_render = TRUE;

    /* in case the video chain is waiting for a text buffer, wake it up */
    GST_TTML_RENDER_BROADCAST (render);
  }

  GST_TTML_RENDER_UNLOCK (render);

beach:

  return ret;
}


/* Caller needs to free returned string after use. */
static gchar *
gst_ttml_render_color_to_string (GstSubtitleColor color)
{
#if PANGO_VERSION_CHECK (1,38,0)
  return g_strdup_printf ("#%02x%02x%02x%02x",
      color.r, color.g, color.b, color.a);
#else
  return g_strdup_printf ("#%02x%02x%02x", color.r, color.g, color.b);
#endif
}


static GstBuffer *
gst_ttml_render_draw_rectangle (guint width, guint height,
    GstSubtitleColor color)
{
  GstMapInfo map;
  cairo_surface_t *surface;
  cairo_t *cairo_state;
  GstBuffer *buffer = gst_buffer_new_allocate (NULL, 4 * width * height, NULL);

  gst_buffer_map (buffer, &map, GST_MAP_READWRITE);
  surface = cairo_image_surface_create_for_data (map.data,
      CAIRO_FORMAT_ARGB32, width, height, width * 4);
  cairo_state = cairo_create (surface);

  /* clear surface */
  cairo_set_operator (cairo_state, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cairo_state);
  cairo_set_operator (cairo_state, CAIRO_OPERATOR_OVER);

  cairo_save (cairo_state);
  cairo_set_source_rgba (cairo_state, color.r / 255.0, color.g / 255.0,
      color.b / 255.0, color.a / 255.0);
  cairo_paint (cairo_state);
  cairo_restore (cairo_state);
  cairo_destroy (cairo_state);
  cairo_surface_destroy (surface);
  gst_buffer_unmap (buffer, &map);

  return buffer;
}


static void
gst_ttml_render_char_range_free (CharRange * range)
{
  g_free (range);
}


/* Choose fonts for generic fontnames based upon IMSC1 and HbbTV specs. */
static gchar *
gst_ttml_render_resolve_generic_fontname (const gchar * name)
{
  if ((g_strcmp0 (name, "default") == 0)) {
    return
        g_strdup ("TiresiasScreenfont,Liberation Mono,Courier New,monospace");
  } else if ((g_strcmp0 (name, "monospace") == 0)) {
    return g_strdup ("Letter Gothic,Liberation Mono,Courier New,monospace");
  } else if ((g_strcmp0 (name, "sansSerif") == 0)) {
    return g_strdup ("TiresiasScreenfont,sans");
  } else if ((g_strcmp0 (name, "serif") == 0)) {
    return g_strdup ("serif");
  } else if ((g_strcmp0 (name, "monospaceSansSerif") == 0)) {
    return g_strdup ("Letter Gothic,monospace");
  } else if ((g_strcmp0 (name, "monospaceSerif") == 0)) {
    return g_strdup ("Courier New,Liberation Mono,monospace");
  } else if ((g_strcmp0 (name, "proportionalSansSerif") == 0)) {
    return g_strdup ("TiresiasScreenfont,Arial,Helvetica,Liberation Sans,sans");
  } else if ((g_strcmp0 (name, "proportionalSerif") == 0)) {
    return g_strdup ("serif");
  } else {
    return NULL;
  }
}


static gchar *
gst_ttml_render_get_text_from_buffer (GstBuffer * buf, guint index)
{
  GstMapInfo map;
  GstMemory *mem;
  gchar *buf_text = NULL;

  mem = gst_buffer_get_memory (buf, index);
  if (!mem) {
    GST_CAT_ERROR (ttmlrender_debug, "Failed to access memory at index %u.",
        index);
    return NULL;
  }

  if (!gst_memory_map (mem, &map, GST_MAP_READ)) {
    GST_CAT_ERROR (ttmlrender_debug, "Failed to map memory at index %u.",
        index);
    goto map_fail;
  }

  buf_text = g_strndup ((const gchar *) map.data, map.size);
  if (!g_utf8_validate (buf_text, -1, NULL)) {
    GST_CAT_ERROR (ttmlrender_debug, "Text in buffer us not valid UTF-8");
    g_free (buf_text);
    buf_text = NULL;
  }

  gst_memory_unmap (mem, &map);
map_fail:
  gst_memory_unref (mem);
  return buf_text;
}


static void
gst_ttml_render_unified_element_free (UnifiedElement * unified_element)
{
  if (!unified_element)
    return;

  gst_subtitle_element_unref (unified_element->element);
  g_free (unified_element->text);
  g_free (unified_element);
}


static UnifiedElement *
gst_ttml_render_unified_element_copy (const UnifiedElement * unified_element)
{
  UnifiedElement *ret;

  if (!unified_element)
    return NULL;

  ret = g_new0 (UnifiedElement, 1);
  ret->element = gst_subtitle_element_ref (unified_element->element);
  ret->pango_font_size = unified_element->pango_font_size;
  ret->pango_font_metrics.height = unified_element->pango_font_metrics.height;
  ret->pango_font_metrics.baseline =
      unified_element->pango_font_metrics.baseline;
  ret->text = g_strdup (unified_element->text);

  return ret;
}


static void
gst_ttml_render_unified_block_free (UnifiedBlock * unified_block)
{
  if (!unified_block)
    return;

  gst_subtitle_style_set_unref (unified_block->style_set);
  g_ptr_array_unref (unified_block->unified_elements);
  g_free (unified_block->joined_text);
  g_free (unified_block);
}


static UnifiedElement *
gst_ttml_render_unified_block_get_element (const UnifiedBlock * block,
    guint index)
{
  if (index >= block->unified_elements->len)
    return NULL;
  else
    return g_ptr_array_index (block->unified_elements, index);
}


static UnifiedBlock *
gst_ttml_render_unified_block_copy (const UnifiedBlock * block)
{
  UnifiedBlock *ret;
  gint i;

  if (!block)
    return NULL;

  ret = g_new0 (UnifiedBlock, 1);
  ret->joined_text = g_strdup (block->joined_text);
  ret->style_set = gst_subtitle_style_set_ref (block->style_set);
  ret->unified_elements = g_ptr_array_new_with_free_func ((GDestroyNotify)
      gst_ttml_render_unified_element_free);

  for (i = 0; i < block->unified_elements->len; ++i) {
    UnifiedElement *ue = gst_ttml_render_unified_block_get_element (block, i);
    UnifiedElement *ue_copy = gst_ttml_render_unified_element_copy (ue);
    g_ptr_array_add (ret->unified_elements, ue_copy);
  }

  return ret;
}


static guint
gst_ttml_render_unified_block_element_count (const UnifiedBlock * block)
{
  return block->unified_elements->len;
}


/*
 * Generates pango-markup'd version of @text that would make pango render it
 * with the styling specified by @style_set.
 */
static gchar *
gst_ttml_render_generate_pango_markup (GstSubtitleStyleSet * style_set,
    guint font_height, const gchar * text)
{
  gchar *ret, *font_family, *font_size, *fgcolor;
  const gchar *font_style, *font_weight, *underline;
  gchar *escaped_text = g_markup_escape_text (text, -1);

  fgcolor = gst_ttml_render_color_to_string (style_set->color);
  font_size = g_strdup_printf ("%u", font_height);
  font_family =
      gst_ttml_render_resolve_generic_fontname (style_set->font_family);
  if (!font_family)
    font_family = g_strdup (style_set->font_family);
  font_style = (style_set->font_style ==
      GST_SUBTITLE_FONT_STYLE_NORMAL) ? "normal" : "italic";
  font_weight = (style_set->font_weight ==
      GST_SUBTITLE_FONT_WEIGHT_NORMAL) ? "normal" : "bold";
  underline = (style_set->text_decoration ==
      GST_SUBTITLE_TEXT_DECORATION_UNDERLINE) ? "single" : "none";

  ret = g_strconcat ("<span "
      "fgcolor=\"", fgcolor, "\" ",
      "font=\"", font_size, "px\" ",
      "font_family=\"", font_family, "\" ",
      "font_style=\"", font_style, "\" ",
      "font_weight=\"", font_weight, "\" ",
      "underline=\"", underline, "\" ", ">", escaped_text, "</span>", NULL);

  g_free (fgcolor);
  g_free (font_family);
  g_free (font_size);
  g_free (escaped_text);
  return ret;
}


/*
 * Unfortunately, pango does not expose accurate metrics about fonts (their
 * maximum height and baseline position), so we need to calculate this
 * information ourselves by examining the ink rectangle of a string containing
 * characters that extend to the maximum height/depth of the font.
 */
static FontMetrics
gst_ttml_render_get_pango_font_metrics (GstTtmlRender * render,
    GstSubtitleStyleSet * style_set, guint font_size)
{
  PangoRectangle ink_rect;
  gchar *string;
  FontMetrics ret;

  string = gst_ttml_render_generate_pango_markup (style_set, font_size,
      "Áĺľď¿gqy");
  pango_layout_set_markup (render->layout, string, strlen (string));
  pango_layout_get_pixel_extents (render->layout, &ink_rect, NULL);
  g_free (string);

  ret.height = ink_rect.height;
  ret.baseline = PANGO_PIXELS (pango_layout_get_baseline (render->layout))
      - ink_rect.y;
  return ret;
}


/*
 * Return the font size that you would need to pass to pango in order that the
 * font applied to @element would be rendered at the text height applied to
 * @element.
 */
static guint
gst_ttml_render_get_pango_font_size (GstTtmlRender * render,
    const GstSubtitleElement * element)
{
  guint desired_font_size =
      (guint) ceil (element->style_set->font_size * render->height);
  guint font_size = desired_font_size;
  guint rendered_height = G_MAXUINT;
  FontMetrics metrics;

  while (rendered_height > desired_font_size) {
    metrics =
        gst_ttml_render_get_pango_font_metrics (render, element->style_set,
        font_size);
    rendered_height = metrics.height;
    --font_size;
  }

  return font_size + 1;
}


/*
 * Reunites each element in @block with its text, as extracted from @buf. Also
 * stores the concatenated text from all contained elements to facilitate
 * future processing.
 */
static UnifiedBlock *
gst_ttml_render_unify_block (GstTtmlRender * render,
    const GstSubtitleBlock * block, GstBuffer * buf)
{
  UnifiedBlock *ret = g_new0 (UnifiedBlock, 1);
  guint i;

  ret->unified_elements = g_ptr_array_new_with_free_func ((GDestroyNotify)
      gst_ttml_render_unified_element_free);
  ret->style_set = gst_subtitle_style_set_ref (block->style_set);
  ret->joined_text = g_strdup ("");

  for (i = 0; i < gst_subtitle_block_get_element_count (block); ++i) {
    gchar *text;
    UnifiedElement *ue = g_new0 (UnifiedElement, 1);
    ue->element =
        gst_subtitle_element_ref (gst_subtitle_block_get_element (block, i));
    ue->pango_font_size =
        gst_ttml_render_get_pango_font_size (render, ue->element);
    ue->pango_font_metrics =
        gst_ttml_render_get_pango_font_metrics (render, ue->element->style_set,
        ue->pango_font_size);
    ue->text =
        gst_ttml_render_get_text_from_buffer (buf, ue->element->text_index);
    g_ptr_array_add (ret->unified_elements, ue);

    text = g_strjoin (NULL, ret->joined_text, ue->text, NULL);
    g_free (ret->joined_text);
    ret->joined_text = text;
  }

  return ret;
}


/*
 * Returns index of nearest breakpoint before @index in @block's text. If no
 * breakpoints are found, returns -1.
 */
static gint
gst_ttml_render_get_nearest_breakpoint (const UnifiedBlock * block, guint index)
{
  const gchar *end = block->joined_text + index - 1;

  while ((end = g_utf8_find_prev_char (block->joined_text, end))) {
    gchar buf[6] = { 0 };
    gunichar u = g_utf8_get_char (end);
    gint nbytes = g_unichar_to_utf8 (u, buf);

    if (nbytes == 1 && (buf[0] == 0x20 || buf[0] == 0x9 || buf[0] == 0xD))
      return end - block->joined_text;
  }

  return -1;
}


/* Return the pango markup representation of all the elements in @block. */
static gchar *
gst_ttml_render_generate_block_markup (const UnifiedBlock * block)
{
  gchar *joined_text, *old_text;
  guint element_count = gst_ttml_render_unified_block_element_count (block);
  guint i;

  joined_text = g_strdup ("");

  for (i = 0; i < element_count; ++i) {
    UnifiedElement *ue = gst_ttml_render_unified_block_get_element (block, i);
    gchar *element_markup =
        gst_ttml_render_generate_pango_markup (ue->element->style_set,
        ue->pango_font_size, ue->text);

    old_text = joined_text;
    joined_text = g_strconcat (joined_text, element_markup, NULL);
    GST_CAT_DEBUG (ttmlrender_debug, "Joined text is now: %s", joined_text);

    g_free (element_markup);
    g_free (old_text);
  }

  return joined_text;
}


/*
 * Returns a set of character ranges, which correspond to the ranges of
 * characters from @block that should be rendered on each generated line area.
 * Essentially, this function determines line breaking and wrapping.
 */
static GPtrArray *
gst_ttml_render_get_line_char_ranges (GstTtmlRender * render,
    const UnifiedBlock * block, guint width, gboolean wrap)
{
  gint start_index = 0;
  GPtrArray *line_ranges = g_ptr_array_new_with_free_func ((GDestroyNotify)
      gst_ttml_render_char_range_free);
  PangoRectangle ink_rect;
  gchar *markup;
  gint i;

  /* Handle hard breaks in block text. */
  while (start_index < strlen (block->joined_text)) {
    CharRange *range = g_new0 (CharRange, 1);
    gchar *c = block->joined_text + start_index;
    while (*c != '\0' && *c != '\n')
      ++c;
    range->first_index = start_index;
    range->last_index = (c - block->joined_text) - 1;
    g_ptr_array_add (line_ranges, range);
    start_index = range->last_index + 2;
  }

  if (!wrap)
    return line_ranges;

  GST_CAT_LOG (ttmlrender_debug,
      "After handling breaks, we have the following ranges:");
  for (i = 0; i < line_ranges->len; ++i) {
    CharRange *range = g_ptr_array_index (line_ranges, i);
    GST_CAT_LOG (ttmlrender_debug, "ranges[%d] first:%u  last:%u", i,
        range->first_index, range->last_index);
  }

  markup = gst_ttml_render_generate_block_markup (block);
  pango_layout_set_markup (render->layout, markup, strlen (markup));
  pango_layout_set_width (render->layout, -1);

  pango_layout_get_pixel_extents (render->layout, &ink_rect, NULL);
  GST_CAT_LOG (ttmlrender_debug, "Layout extents - x:%d  y:%d  w:%d  h:%d",
      ink_rect.x, ink_rect.y, ink_rect.width, ink_rect.height);

  /* For each range, wrap if it extends beyond allowed width. */
  for (i = 0; i < line_ranges->len; ++i) {
    CharRange *range, *new_range;
    gint max_line_extent;
    gint end_index = 0;
    gint trailing;
    PangoRectangle rect;
    gboolean within_line;

    do {
      range = g_ptr_array_index (line_ranges, i);
      GST_CAT_LOG (ttmlrender_debug,
          "Seeing if we need to wrap range[%d] - start:%u  end:%u", i,
          range->first_index, range->last_index);

      pango_layout_index_to_pos (render->layout, range->first_index, &rect);
      GST_CAT_LOG (ttmlrender_debug, "First char at x:%d  y:%d", rect.x,
          rect.y);

      max_line_extent = rect.x + (PANGO_SCALE * width);
      GST_CAT_LOG (ttmlrender_debug, "max_line_extent: %d",
          PANGO_PIXELS (max_line_extent));

      within_line =
          pango_layout_xy_to_index (render->layout, max_line_extent, rect.y,
          &end_index, &trailing);

      GST_CAT_LOG (ttmlrender_debug, "Index nearest to breakpoint: %d",
          end_index);

      if (within_line) {
        end_index = gst_ttml_render_get_nearest_breakpoint (block, end_index);

        if (end_index > range->first_index) {
          new_range = g_new0 (CharRange, 1);
          new_range->first_index = end_index + 1;
          new_range->last_index = range->last_index;
          GST_CAT_LOG (ttmlrender_debug,
              "Wrapping line %d; added new range - start:%u  end:%u", i,
              new_range->first_index, new_range->last_index);

          range->last_index = end_index;
          GST_CAT_LOG (ttmlrender_debug,
              "Modified last_index of existing range; range is now start:%u  "
              "end:%u", range->first_index, range->last_index);

          g_ptr_array_insert (line_ranges, ++i, new_range);
        } else {
          GST_CAT_DEBUG (ttmlrender_debug,
              "Couldn't find a suitable breakpoint");
          within_line = FALSE;
        }
      }
    } while (within_line);
  }

  g_free (markup);
  return line_ranges;
}


/*
 * Returns the index of the element in @block containing the character at index
 * @char_index in @block's text. If @offset is not NULL, sets it to the
 * character offset of @char_index within the element where it is found.
 */
static gint
gst_ttml_render_get_element_index (const UnifiedBlock * block,
    const gint char_index, gint * offset)
{
  gint count = 0;
  gint i;

  if ((char_index < 0) || (char_index >= strlen (block->joined_text)))
    return -1;

  for (i = 0; i < gst_ttml_render_unified_block_element_count (block); ++i) {
    UnifiedElement *ue = gst_ttml_render_unified_block_get_element (block, i);
    if ((char_index >= count) && (char_index < (count + strlen (ue->text)))) {
      if (offset)
        *offset = char_index - count;
      break;
    }
    count += strlen (ue->text);
  }

  return i;
}


static guint
gst_ttml_render_strip_leading_spaces (gchar ** string)
{
  gchar *c = *string;

  while (c) {
    gchar buf[6] = { 0 };
    gunichar u = g_utf8_get_char (c);
    gint nbytes = g_unichar_to_utf8 (u, buf);

    if ((nbytes == 1) && (buf[0] == 0x20))
      c = g_utf8_find_next_char (c, c + strlen (*string));
    else
      break;
  }

  if (!c) {
    GST_CAT_DEBUG (ttmlrender_debug,
        "All characters would be removed from string.");
    return 0;
  } else if (c > *string) {
    gchar *tmp = *string;
    *string = g_strdup (c);
    GST_CAT_DEBUG (ttmlrender_debug, "Replacing text \"%s\" with \"%s\"", tmp,
        *string);
    g_free (tmp);
  }

  return strlen (*string);
}


static guint
gst_ttml_render_strip_trailing_spaces (gchar ** string)
{
  gchar *c = *string + strlen (*string) - 1;
  gint nbytes;

  while (c) {
    gchar buf[6] = { 0 };
    gunichar u = g_utf8_get_char (c);
    nbytes = g_unichar_to_utf8 (u, buf);

    if ((nbytes == 1) && (buf[0] == 0x20))
      c = g_utf8_find_prev_char (*string, c);
    else
      break;
  }

  if (!c) {
    GST_CAT_DEBUG (ttmlrender_debug,
        "All characters would be removed from string.");
    return 0;
  } else {
    gchar *tmp = *string;
    *string = g_strndup (*string, (c - *string) + nbytes);
    GST_CAT_DEBUG (ttmlrender_debug, "Replacing text \"%s\" with \"%s\"", tmp,
        *string);
    g_free (tmp);
  }

  return strlen (*string);
}


/*
 * Treating each block in @blocks as a separate line area, conditionally strips
 * space characters from the beginning and end of each line. This function
 * implements the suppress-at-line-break="auto" and
 * white-space-treatment="ignore-if-surrounding-linefeed" behaviours (specified
 * by TTML section 7.2.3) for elements at the start and end of lines that have
 * xml:space="default" applied to them. If stripping whitespace from a block
 * removes all elements of that block, the block will be removed from @blocks.
 * Returns the number of remaining blocks.
 */
static guint
gst_ttml_render_handle_whitespace (GPtrArray * blocks)
{
  gint i;

  for (i = 0; i < blocks->len; ++i) {
    UnifiedBlock *ub = g_ptr_array_index (blocks, i);
    UnifiedElement *ue;
    guint remaining_chars = 0;

    /* Remove leading spaces from line area. */
    while ((gst_ttml_render_unified_block_element_count (ub) > 0)
        && (remaining_chars == 0)) {
      ue = gst_ttml_render_unified_block_get_element (ub, 0);
      if (!ue->element->suppress_whitespace)
        break;
      remaining_chars = gst_ttml_render_strip_leading_spaces (&ue->text);

      if (remaining_chars == 0) {
        g_ptr_array_remove_index (ub->unified_elements, 0);
        GST_CAT_DEBUG (ttmlrender_debug, "Removed first element from block");
      }
    }

    remaining_chars = 0;

    /* Remove trailing spaces from line area. */
    while ((gst_ttml_render_unified_block_element_count (ub) > 0)
        && (remaining_chars == 0)) {
      ue = gst_ttml_render_unified_block_get_element (ub,
          gst_ttml_render_unified_block_element_count (ub) - 1);
      if (!ue->element->suppress_whitespace)
        break;
      remaining_chars = gst_ttml_render_strip_trailing_spaces (&ue->text);

      if (remaining_chars == 0) {
        g_ptr_array_remove_index (ub->unified_elements,
            gst_ttml_render_unified_block_element_count (ub) - 1);
        GST_CAT_DEBUG (ttmlrender_debug, "Removed last element from block");
      }
    }

    if (gst_ttml_render_unified_block_element_count (ub) == 0)
      g_ptr_array_remove_index (blocks, i--);
  }

  return blocks->len;
}


/*
 * Splits a single UnifiedBlock, @block, into an array of separate
 * UnifiedBlocks, according to the character ranges given in @char_ranges.
 * Each resulting UnifiedBlock will contain only the elements to which belong
 * the characters in its corresponding character range; the text of the first
 * and last element in the block will be clipped of any characters before and
 * after, respectively, the first and last characters in the corresponding
 * range.
 */
static GPtrArray *
gst_ttml_render_split_block (UnifiedBlock * block, GPtrArray * char_ranges)
{
  GPtrArray *ret = g_ptr_array_new_with_free_func ((GDestroyNotify)
      gst_ttml_render_unified_block_free);
  gint i;

  for (i = 0; i < char_ranges->len; ++i) {
    gint index;
    gint first_offset = 0;
    gint last_offset = 0;
    CharRange *range = g_ptr_array_index (char_ranges, i);
    UnifiedBlock *clone = gst_ttml_render_unified_block_copy (block);
    UnifiedElement *ue;
    gchar *tmp;

    GST_CAT_LOG (ttmlrender_debug, "range start:%u  end:%u", range->first_index,
        range->last_index);
    index =
        gst_ttml_render_get_element_index (clone, range->last_index,
        &last_offset);
    GST_CAT_LOG (ttmlrender_debug, "Last char in range is in element %d",
        index);

    if (index < 0) {
      GST_CAT_WARNING (ttmlrender_debug, "Range end not found in block text.");
      gst_ttml_render_unified_block_free (clone);
      continue;
    }

    /* Remove elements that are after the one that contains the range end. */
    GST_CAT_LOG (ttmlrender_debug, "There are %d elements in cloned block.",
        gst_ttml_render_unified_block_element_count (clone));
    while (gst_ttml_render_unified_block_element_count (clone) > (index + 1)) {
      GST_CAT_LOG (ttmlrender_debug, "Removing last element in cloned block.");
      g_ptr_array_remove_index (clone->unified_elements, index + 1);
    }

    index =
        gst_ttml_render_get_element_index (clone, range->first_index,
        &first_offset);
    GST_CAT_LOG (ttmlrender_debug, "First char in range is in element %d",
        index);

    if (index < 0) {
      GST_CAT_WARNING (ttmlrender_debug,
          "Range start not found in block text.");
      gst_ttml_render_unified_block_free (clone);
      continue;
    }

    /* Remove elements that are before the one that contains the range start. */
    while (index > 0) {
      GST_CAT_LOG (ttmlrender_debug, "Removing first element in cloned block");
      g_ptr_array_remove_index (clone->unified_elements, 0);
      --index;
    }

    /* Remove characters from first element that are before the range start. */
    ue = gst_ttml_render_unified_block_get_element (clone, 0);
    if (first_offset > 0) {
      tmp = ue->text;
      ue->text = g_strdup (ue->text + first_offset);
      GST_CAT_DEBUG (ttmlrender_debug,
          "First element text has been clipped to \"%s\"", ue->text);
      g_free (tmp);

      if (gst_ttml_render_unified_block_element_count (clone) == 1)
        last_offset -= first_offset;
    }

    /* Remove characters from last element that are after the range end. */
    ue = gst_ttml_render_unified_block_get_element (clone,
        gst_ttml_render_unified_block_element_count (clone) - 1);
    if (last_offset < (strlen (ue->text) - 1)) {
      tmp = ue->text;
      ue->text = g_strndup (ue->text, last_offset + 1);
      GST_CAT_DEBUG (ttmlrender_debug,
          "Last element text has been clipped to \"%s\"", ue->text);
      g_free (tmp);
    }

    if (gst_ttml_render_unified_block_element_count (clone) > 0)
      g_ptr_array_add (ret, clone);
    else
      gst_ttml_render_unified_block_free (clone);
  }

  if (ret->len == 0) {
    GST_CAT_DEBUG (ttmlrender_debug, "No elements remain in clone.");
    g_ptr_array_unref (ret);
    ret = NULL;
  }
  return ret;
}


/* Render the text in a pango-markup string. */
static GstTtmlRenderRenderedImage *
gst_ttml_render_draw_text (GstTtmlRender * render, const gchar * text,
    guint line_height, guint baseline_offset)
{
  GstTtmlRenderRenderedImage *ret;
  cairo_surface_t *surface, *cropped_surface;
  cairo_t *cairo_state, *cropped_state;
  GstMapInfo map;
  PangoRectangle logical_rect, ink_rect;
  guint buf_width, buf_height;
  gint stride;
  gint bounding_box_x1, bounding_box_x2, bounding_box_y1, bounding_box_y2;
  gint baseline;

  ret = gst_ttml_render_rendered_image_new_empty ();

  pango_layout_set_markup (render->layout, text, strlen (text));
  GST_CAT_DEBUG (ttmlrender_debug, "Layout text: \"%s\"",
      pango_layout_get_text (render->layout));
  pango_layout_set_width (render->layout, -1);

  pango_layout_get_pixel_extents (render->layout, &ink_rect, &logical_rect);

  baseline = PANGO_PIXELS (pango_layout_get_baseline (render->layout));

  bounding_box_x1 = MIN (logical_rect.x, ink_rect.x);
  bounding_box_x2 = MAX (logical_rect.x + logical_rect.width,
      ink_rect.x + ink_rect.width);
  bounding_box_y1 = MIN (logical_rect.y, ink_rect.y);
  bounding_box_y2 = MAX (logical_rect.y + logical_rect.height,
      ink_rect.y + ink_rect.height);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
      (bounding_box_x2 - bounding_box_x1), (bounding_box_y2 - bounding_box_y1));
  cairo_state = cairo_create (surface);
  cairo_set_operator (cairo_state, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cairo_state);
  cairo_set_operator (cairo_state, CAIRO_OPERATOR_OVER);

  cairo_save (cairo_state);
  pango_cairo_show_layout (cairo_state, render->layout);
  cairo_restore (cairo_state);

  buf_width = bounding_box_x2 - bounding_box_x1;
  buf_height = ink_rect.height;
  GST_CAT_DEBUG (ttmlrender_debug, "Output buffer width: %u  height: %u",
      buf_width, buf_height);

  ret->image = gst_buffer_new_allocate (NULL, 4 * buf_width * buf_height, NULL);
  gst_buffer_memset (ret->image, 0, 0U, 4 * buf_width * buf_height);
  gst_buffer_map (ret->image, &map, GST_MAP_READWRITE);

  stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, buf_width);
  cropped_surface =
      cairo_image_surface_create_for_data (map.data, CAIRO_FORMAT_ARGB32,
      (bounding_box_x2 - bounding_box_x1), ink_rect.height, stride);
  cropped_state = cairo_create (cropped_surface);
  cairo_set_source_surface (cropped_state, surface, -bounding_box_x1,
      -ink_rect.y);
  cairo_rectangle (cropped_state, 0, 0, buf_width, buf_height);
  cairo_fill (cropped_state);

  cairo_destroy (cairo_state);
  cairo_surface_destroy (surface);
  cairo_destroy (cropped_state);
  cairo_surface_destroy (cropped_surface);
  gst_buffer_unmap (ret->image, &map);

  ret->width = buf_width;
  ret->height = buf_height;
  ret->x = 0;
  ret->y = MAX (0, (gint) baseline_offset - (baseline - ink_rect.y));
  return ret;
}


static GstTtmlRenderRenderedImage *
gst_ttml_render_render_block_elements (GstTtmlRender * render,
    UnifiedBlock * block, BlockMetrics block_metrics)
{
  GPtrArray *inline_images = g_ptr_array_new_with_free_func (
      (GDestroyNotify) gst_ttml_render_rendered_image_free);
  GstTtmlRenderRenderedImage *ret = NULL;
  guint line_padding =
      (guint) ceil (block->style_set->line_padding * render->width);
  gint i;

  for (i = 0; i < gst_ttml_render_unified_block_element_count (block); ++i) {
    UnifiedElement *ue = gst_ttml_render_unified_block_get_element (block, i);
    gchar *markup;
    GstTtmlRenderRenderedImage *text_image, *bg_image, *combined_image;
    guint bg_offset, bg_width, bg_height;
    GstBuffer *background;

    markup = gst_ttml_render_generate_pango_markup (ue->element->style_set,
        ue->pango_font_size, ue->text);
    text_image = gst_ttml_render_draw_text (render, markup,
        block_metrics.line_height, block_metrics.baseline_offset);
    g_free (markup);

    if (!block->style_set->fill_line_gap) {
      bg_offset =
          block_metrics.baseline_offset - ue->pango_font_metrics.baseline;
      bg_height = ue->pango_font_metrics.height;
    } else {
      bg_offset = 0;
      bg_height = block_metrics.line_height;
    }
    bg_width = text_image->width;

    if (line_padding > 0) {
      if (i == 0) {
        text_image->x += line_padding;
        bg_width += line_padding;
      }
      if (i == (gst_ttml_render_unified_block_element_count (block) - 1))
        bg_width += line_padding;
    }

    background = gst_ttml_render_draw_rectangle (bg_width, bg_height,
        ue->element->style_set->background_color);
    bg_image = gst_ttml_render_rendered_image_new (background, 0,
        bg_offset, bg_width, bg_height);
    combined_image = gst_ttml_render_rendered_image_combine (bg_image,
        text_image);
    gst_ttml_render_rendered_image_free (bg_image);
    gst_ttml_render_rendered_image_free (text_image);
    g_ptr_array_add (inline_images, combined_image);
  }

  ret = gst_ttml_render_stitch_images (inline_images,
      GST_TTML_DIRECTION_INLINE);
  GST_CAT_DEBUG (ttmlrender_debug,
      "Stitched line image - x:%d  y:%d  w:%u  h:%u",
      ret->x, ret->y, ret->width, ret->height);
  g_ptr_array_unref (inline_images);
  return ret;
}


/*
 * Align the images in @lines according to the multi_row_align and text_align
 * settings in @style_set.
 */
static void
gst_ttml_render_align_line_areas (GPtrArray * lines,
    const GstSubtitleStyleSet * style_set)
{
  guint longest_line_width = 0;
  gint i;

  for (i = 0; i < lines->len; ++i) {
    GstTtmlRenderRenderedImage *line = g_ptr_array_index (lines, i);
    if (line->width > longest_line_width)
      longest_line_width = line->width;
  }

  for (i = 0; i < lines->len; ++i) {
    GstTtmlRenderRenderedImage *line = g_ptr_array_index (lines, i);

    switch (style_set->multi_row_align) {
      case GST_SUBTITLE_MULTI_ROW_ALIGN_CENTER:
        line->x += (gint) round ((longest_line_width - line->width) / 2.0);
        break;
      case GST_SUBTITLE_MULTI_ROW_ALIGN_END:
        line->x += (longest_line_width - line->width);
        break;
      case GST_SUBTITLE_MULTI_ROW_ALIGN_AUTO:
        switch (style_set->text_align) {
          case GST_SUBTITLE_TEXT_ALIGN_CENTER:
            line->x += (gint) round ((longest_line_width - line->width) / 2.0);
            break;
          case GST_SUBTITLE_TEXT_ALIGN_END:
          case GST_SUBTITLE_TEXT_ALIGN_RIGHT:
            line->x += (longest_line_width - line->width);
            break;
          default:
            break;
        }
        break;
      default:
        break;
    }
  }
}


/*
 * Renders each UnifiedBlock in @blocks, and sets the positions of the
 * resulting images according to the line height in @metrics and the alignment
 * settings in @style_set.
 */
static GPtrArray *
gst_ttml_render_layout_blocks (GstTtmlRender * render, GPtrArray * blocks,
    BlockMetrics metrics, const GstSubtitleStyleSet * style_set)
{
  GPtrArray *ret = g_ptr_array_new_with_free_func ((GDestroyNotify)
      gst_ttml_render_rendered_image_free);
  gint i;

  for (i = 0; i < blocks->len; ++i) {
    UnifiedBlock *block = g_ptr_array_index (blocks, i);

    GstTtmlRenderRenderedImage *line =
        gst_ttml_render_render_block_elements (render, block,
        metrics);
    line->y += (i * metrics.line_height);
    g_ptr_array_add (ret, line);
  }

  gst_ttml_render_align_line_areas (ret, style_set);
  return ret;
}


/* If any of an array of elements has line wrapping enabled, returns TRUE. */
static gboolean
gst_ttml_render_elements_are_wrapped (GPtrArray * elements)
{
  GstSubtitleElement *element;
  guint i;

  for (i = 0; i < elements->len; ++i) {
    element = g_ptr_array_index (elements, i);
    if (element->style_set->wrap_option == GST_SUBTITLE_WRAPPING_ON)
      return TRUE;
  }

  return FALSE;
}


/*
 * Return the descender (in pixels) shared by the greatest number of glyphs in
 * @block.
 */
static guint
gst_ttml_render_get_most_frequent_descender (GstTtmlRender * render,
    UnifiedBlock * block)
{
  GHashTable *count_table = g_hash_table_new (g_direct_hash, g_direct_equal);
  GHashTableIter iter;
  gpointer key, value;
  guint max_count = 0;
  guint ret = 0;
  gint i;

  for (i = 0; i < gst_ttml_render_unified_block_element_count (block); ++i) {
    UnifiedElement *ue = gst_ttml_render_unified_block_get_element (block, i);
    guint descender =
        ue->pango_font_metrics.height - ue->pango_font_metrics.baseline;
    guint count;

    if (g_hash_table_contains (count_table, GUINT_TO_POINTER (descender))) {
      count = GPOINTER_TO_UINT (g_hash_table_lookup (count_table,
              GUINT_TO_POINTER (descender)));
      GST_CAT_LOG (ttmlrender_debug,
          "Table already contains %u glyphs with descender %u; increasing "
          "that count to %ld", count, descender,
          count + g_utf8_strlen (ue->text, -1));
      count += g_utf8_strlen (ue->text, -1);
    } else {
      count = g_utf8_strlen (ue->text, -1);
      GST_CAT_LOG (ttmlrender_debug,
          "No glyphs with descender %u; adding entry to table with count of %u",
          descender, count);
    }

    g_hash_table_insert (count_table,
        GUINT_TO_POINTER (descender), GUINT_TO_POINTER (count));
  }

  g_hash_table_iter_init (&iter, count_table);
  while (g_hash_table_iter_next (&iter, &key, &value)) {
    guint descender = GPOINTER_TO_UINT (key);
    guint count = GPOINTER_TO_UINT (value);

    if (count > max_count) {
      max_count = count;
      ret = descender;
    }
  }

  g_hash_table_unref (count_table);
  return ret;
}


static BlockMetrics
gst_ttml_render_get_block_metrics (GstTtmlRender * render, UnifiedBlock * block)
{
  BlockMetrics ret;

  /*
   * The specified behaviour in TTML when lineHeight is "normal" is different
   * from the behaviour when a percentage is given. In the former case, the
   * line height is a percentage (the TTML spec recommends 125%) of the largest
   * font size that is applied to the spans within the block; in the latter
   * case, the line height is the given percentage of the font size that is
   * applied to the block itself.
   */
  if (block->style_set->line_height < 0) {      /* lineHeight="normal" case */
    guint max_text_height = 0;
    guint descender = 0;
    guint i;

    for (i = 0; i < gst_ttml_render_unified_block_element_count (block); ++i) {
      UnifiedElement *ue = gst_ttml_render_unified_block_get_element (block, i);

      if (ue->pango_font_metrics.height > max_text_height) {
        max_text_height = ue->pango_font_metrics.height;
        descender =
            ue->pango_font_metrics.height - ue->pango_font_metrics.baseline;
      }
    }

    GST_CAT_LOG (ttmlrender_debug, "Max descender: %u   Max text height: %u",
        descender, max_text_height);
    ret.line_height = (guint) ceil (max_text_height * 1.25);
    ret.baseline_offset = (guint) ((max_text_height + ret.line_height) / 2.0)
        - descender;
  } else {
    guint descender;
    guint font_size;

    descender = gst_ttml_render_get_most_frequent_descender (render, block);
    GST_CAT_LOG (ttmlrender_debug,
        "Got most frequent descender value of %u pixels.", descender);
    font_size = (guint) ceil (block->style_set->font_size * render->height);
    ret.line_height = (guint) ceil (font_size * block->style_set->line_height);
    ret.baseline_offset = (guint) ((font_size + ret.line_height) / 2.0)
        - descender;
  }

  return ret;
}


static GstTtmlRenderRenderedImage *
gst_ttml_render_rendered_image_new (GstBuffer * image, gint x, gint y,
    guint width, guint height)
{
  GstTtmlRenderRenderedImage *ret;

  ret = g_new0 (GstTtmlRenderRenderedImage, 1);

  ret->image = image;
  ret->x = x;
  ret->y = y;
  ret->width = width;
  ret->height = height;

  return ret;
}


static GstTtmlRenderRenderedImage *
gst_ttml_render_rendered_image_new_empty (void)
{
  return gst_ttml_render_rendered_image_new (NULL, 0, 0, 0, 0);
}


static inline GstTtmlRenderRenderedImage *
gst_ttml_render_rendered_image_copy (GstTtmlRenderRenderedImage * image)
{
  GstTtmlRenderRenderedImage *ret = g_new0 (GstTtmlRenderRenderedImage, 1);

  ret->image = gst_buffer_ref (image->image);
  ret->x = image->x;
  ret->y = image->y;
  ret->width = image->width;
  ret->height = image->height;

  return ret;
}


static void
gst_ttml_render_rendered_image_free (GstTtmlRenderRenderedImage * image)
{
  if (!image)
    return;
  gst_buffer_unref (image->image);
  g_free (image);
}


/*
 * Combines two rendered image into a single image. The order of arguments is
 * significant: @image2 will be rendered on top of @image1.
 */
static GstTtmlRenderRenderedImage *
gst_ttml_render_rendered_image_combine (GstTtmlRenderRenderedImage * image1,
    GstTtmlRenderRenderedImage * image2)
{
  GstTtmlRenderRenderedImage *ret;
  GstMapInfo map1, map2, map_dest;
  cairo_surface_t *sfc1, *sfc2, *sfc_dest;
  cairo_t *state_dest;

  if (!image1 && !image2)
    return NULL;
  if (image1 && !image2)
    return gst_ttml_render_rendered_image_copy (image1);
  if (image2 && !image1)
    return gst_ttml_render_rendered_image_copy (image2);

  ret = g_new0 (GstTtmlRenderRenderedImage, 1);

  /* Work out dimensions of combined image. */
  ret->x = MIN (image1->x, image2->x);
  ret->y = MIN (image1->y, image2->y);
  ret->width = MAX (image1->x + image1->width, image2->x + image2->width)
      - ret->x;
  ret->height = MAX (image1->y + image1->height, image2->y + image2->height)
      - ret->y;

  GST_CAT_LOG (ttmlrender_debug, "Dimensions of combined image:  x:%u  y:%u  "
      "width:%u  height:%u", ret->x, ret->y, ret->width, ret->height);

  /* Create cairo_surface from src images. */
  gst_buffer_map (image1->image, &map1, GST_MAP_READ);
  sfc1 =
      cairo_image_surface_create_for_data (map1.data, CAIRO_FORMAT_ARGB32,
      image1->width, image1->height,
      cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, image1->width));

  gst_buffer_map (image2->image, &map2, GST_MAP_READ);
  sfc2 =
      cairo_image_surface_create_for_data (map2.data, CAIRO_FORMAT_ARGB32,
      image2->width, image2->height,
      cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, image2->width));

  /* Create cairo_surface for resultant image. */
  ret->image = gst_buffer_new_allocate (NULL, 4 * ret->width * ret->height,
      NULL);
  gst_buffer_memset (ret->image, 0, 0U, 4 * ret->width * ret->height);
  gst_buffer_map (ret->image, &map_dest, GST_MAP_READWRITE);
  sfc_dest =
      cairo_image_surface_create_for_data (map_dest.data, CAIRO_FORMAT_ARGB32,
      ret->width, ret->height,
      cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, ret->width));
  state_dest = cairo_create (sfc_dest);

  /* Blend image1 into destination surface. */
  cairo_set_source_surface (state_dest, sfc1, image1->x - ret->x,
      image1->y - ret->y);
  cairo_rectangle (state_dest, image1->x - ret->x, image1->y - ret->y,
      image1->width, image1->height);
  cairo_fill (state_dest);

  /* Blend image2 into destination surface. */
  cairo_set_source_surface (state_dest, sfc2, image2->x - ret->x,
      image2->y - ret->y);
  cairo_rectangle (state_dest, image2->x - ret->x, image2->y - ret->y,
      image2->width, image2->height);
  cairo_fill (state_dest);

  /* Return destination image. */
  cairo_destroy (state_dest);
  cairo_surface_destroy (sfc1);
  cairo_surface_destroy (sfc2);
  cairo_surface_destroy (sfc_dest);
  gst_buffer_unmap (image1->image, &map1);
  gst_buffer_unmap (image2->image, &map2);
  gst_buffer_unmap (ret->image, &map_dest);

  return ret;
}


static GstTtmlRenderRenderedImage *
gst_ttml_render_rendered_image_crop (GstTtmlRenderRenderedImage * image,
    gint x, gint y, guint width, guint height)
{
  GstTtmlRenderRenderedImage *ret;
  GstMapInfo map_src, map_dest;
  cairo_surface_t *sfc_src, *sfc_dest;
  cairo_t *state_dest;

  if ((x <= image->x) && (y <= image->y) && (width >= image->width)
      && (height >= image->height))
    return gst_ttml_render_rendered_image_copy (image);

  if (image->x >= (x + (gint) width)
      || (image->x + (gint) image->width) <= x
      || image->y >= (y + (gint) height)
      || (image->y + (gint) image->height) <= y) {
    GST_CAT_WARNING (ttmlrender_debug,
        "Crop rectangle doesn't intersect image.");
    return NULL;
  }

  ret = g_new0 (GstTtmlRenderRenderedImage, 1);

  ret->x = MAX (image->x, x);
  ret->y = MAX (image->y, y);
  ret->width = MIN ((image->x + image->width) - ret->x, (x + width) - ret->x);
  ret->height = MIN ((image->y + image->height) - ret->y,
      (y + height) - ret->y);

  GST_CAT_LOG (ttmlrender_debug, "Dimensions of cropped image:  x:%u  y:%u  "
      "width:%u  height:%u", ret->x, ret->y, ret->width, ret->height);

  /* Create cairo_surface from src image. */
  gst_buffer_map (image->image, &map_src, GST_MAP_READ);
  sfc_src =
      cairo_image_surface_create_for_data (map_src.data, CAIRO_FORMAT_ARGB32,
      image->width, image->height,
      cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, image->width));

  /* Create cairo_surface for cropped image. */
  ret->image = gst_buffer_new_allocate (NULL, 4 * ret->width * ret->height,
      NULL);
  gst_buffer_memset (ret->image, 0, 0U, 4 * ret->width * ret->height);
  gst_buffer_map (ret->image, &map_dest, GST_MAP_READWRITE);
  sfc_dest =
      cairo_image_surface_create_for_data (map_dest.data, CAIRO_FORMAT_ARGB32,
      ret->width, ret->height,
      cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, ret->width));
  state_dest = cairo_create (sfc_dest);

  /* Copy section of image1 into destination surface. */
  cairo_set_source_surface (state_dest, sfc_src, (image->x - ret->x),
      (image->y - ret->y));
  cairo_rectangle (state_dest, 0, 0, ret->width, ret->height);
  cairo_fill (state_dest);

  cairo_destroy (state_dest);
  cairo_surface_destroy (sfc_src);
  cairo_surface_destroy (sfc_dest);
  gst_buffer_unmap (image->image, &map_src);
  gst_buffer_unmap (ret->image, &map_dest);

  return ret;
}


static gboolean
gst_ttml_render_color_is_transparent (GstSubtitleColor * color)
{
  return (color->a == 0);
}


/*
 * Overlays a set of rendered images to return a single image. Order is
 * significant: later entries in @images are rendered on top of earlier
 * entries.
 */
static GstTtmlRenderRenderedImage *
gst_ttml_render_overlay_images (GPtrArray * images)
{
  GstTtmlRenderRenderedImage *ret = NULL;
  gint i;

  for (i = 0; i < images->len; ++i) {
    GstTtmlRenderRenderedImage *tmp = ret;
    ret = gst_ttml_render_rendered_image_combine (ret,
        g_ptr_array_index (images, i));
    gst_ttml_render_rendered_image_free (tmp);
  }

  return ret;
}


/*
 * Takes a set of images and renders them as a single image, where all the
 * images are arranged contiguously in the direction given by @direction. Note
 * that the positions of the images in @images will be altered.
 */
static GstTtmlRenderRenderedImage *
gst_ttml_render_stitch_images (GPtrArray * images, GstTtmlDirection direction)
{
  guint cur_offset = 0;
  GstTtmlRenderRenderedImage *ret = NULL;
  gint i;

  for (i = 0; i < images->len; ++i) {
    GstTtmlRenderRenderedImage *block;
    block = g_ptr_array_index (images, i);

    if (direction == GST_TTML_DIRECTION_BLOCK) {
      block->y += cur_offset;
      cur_offset = block->y + block->height;
    } else {
      block->x += cur_offset;
      cur_offset = block->x + block->width;
    }
  }

  ret = gst_ttml_render_overlay_images (images);

  if (ret) {
    if (direction == GST_TTML_DIRECTION_BLOCK)
      GST_CAT_LOG (ttmlrender_debug, "Height of stitched image: %u",
          ret->height);
    else
      GST_CAT_LOG (ttmlrender_debug, "Width of stitched image: %u", ret->width);
    ret->image = gst_buffer_make_writable (ret->image);
  }
  return ret;
}


static GstTtmlRenderRenderedImage *
gst_ttml_render_render_text_block (GstTtmlRender * render,
    const GstSubtitleBlock * block, GstBuffer * text_buf, guint width,
    gboolean overflow)
{
  UnifiedBlock *unified_block;
  BlockMetrics metrics;
  gboolean wrap;
  guint line_padding;
  GPtrArray *ranges;
  GPtrArray *split_blocks;
  GPtrArray *images;
  GstTtmlRenderRenderedImage *rendered_block = NULL;
  gint i;

  unified_block = gst_ttml_render_unify_block (render, block, text_buf);
  metrics = gst_ttml_render_get_block_metrics (render, unified_block);
  wrap = gst_ttml_render_elements_are_wrapped (block->elements);

  line_padding = (guint) ceil (block->style_set->line_padding * render->width);
  ranges = gst_ttml_render_get_line_char_ranges (render, unified_block, width -
      (2 * line_padding), wrap);

  for (i = 0; i < ranges->len; ++i) {
    CharRange *range = g_ptr_array_index (ranges, i);
    GST_CAT_LOG (ttmlrender_debug, "ranges[%d] first:%u  last:%u", i,
        range->first_index, range->last_index);
  }

  split_blocks = gst_ttml_render_split_block (unified_block, ranges);
  if (split_blocks) {
    guint blocks_remining = gst_ttml_render_handle_whitespace (split_blocks);
    GST_CAT_DEBUG (ttmlrender_debug,
        "There are %u blocks remaining after whitespace handling.",
        blocks_remining);

    if (blocks_remining > 0) {
      images = gst_ttml_render_layout_blocks (render, split_blocks, metrics,
          unified_block->style_set);
      rendered_block = gst_ttml_render_overlay_images (images);
      g_ptr_array_unref (images);
    }
    g_ptr_array_unref (split_blocks);
  }

  g_ptr_array_unref (ranges);
  gst_ttml_render_unified_block_free (unified_block);
  return rendered_block;
}


static GstVideoOverlayComposition *
gst_ttml_render_compose_overlay (GstTtmlRenderRenderedImage * image)
{
  GstVideoOverlayRectangle *rectangle;
  GstVideoOverlayComposition *ret = NULL;

  gst_buffer_add_video_meta (image->image, GST_VIDEO_FRAME_FLAG_NONE,
      GST_VIDEO_OVERLAY_COMPOSITION_FORMAT_RGB, image->width, image->height);

  rectangle = gst_video_overlay_rectangle_new_raw (image->image, image->x,
      image->y, image->width, image->height,
      GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA);

  ret = gst_video_overlay_composition_new (rectangle);
  gst_video_overlay_rectangle_unref (rectangle);
  return ret;
}


static GstVideoOverlayComposition *
gst_ttml_render_render_text_region (GstTtmlRender * render,
    GstSubtitleRegion * region, GstBuffer * text_buf)
{
  guint region_x, region_y, region_width, region_height;
  guint window_x, window_y, window_width, window_height;
  guint padding_start, padding_end, padding_before, padding_after;
  GPtrArray *rendered_blocks =
      g_ptr_array_new_with_free_func (
      (GDestroyNotify) gst_ttml_render_rendered_image_free);
  GstTtmlRenderRenderedImage *region_image = NULL;
  GstVideoOverlayComposition *ret = NULL;
  guint i;

  region_width = (guint) (round (region->style_set->extent_w * render->width));
  region_height =
      (guint) (round (region->style_set->extent_h * render->height));
  region_x = (guint) (round (region->style_set->origin_x * render->width));
  region_y = (guint) (round (region->style_set->origin_y * render->height));

  padding_start =
      (guint) (round (region->style_set->padding_start * render->width));
  padding_end =
      (guint) (round (region->style_set->padding_end * render->width));
  padding_before =
      (guint) (round (region->style_set->padding_before * render->height));
  padding_after =
      (guint) (round (region->style_set->padding_after * render->height));

  /* "window" here refers to the section of the region that we're allowed to
   * render into, i.e., the region minus padding. */
  window_x = region_x + padding_start;
  window_y = region_y + padding_before;
  window_width = region_width - (padding_start + padding_end);
  window_height = region_height - (padding_before + padding_after);

  GST_CAT_DEBUG (ttmlrender_debug,
      "Padding: start: %u  end: %u  before: %u  after: %u",
      padding_start, padding_end, padding_before, padding_after);

  /* Render region background, if non-transparent. */
  if (!gst_ttml_render_color_is_transparent (&region->style_set->
          background_color)) {
    GstBuffer *bg_rect;

    bg_rect = gst_ttml_render_draw_rectangle (region_width, region_height,
        region->style_set->background_color);
    region_image = gst_ttml_render_rendered_image_new (bg_rect, region_x,
        region_y, region_width, region_height);
  }

  /* Render each block and append to list. */
  for (i = 0; i < gst_subtitle_region_get_block_count (region); ++i) {
    const GstSubtitleBlock *block;
    GstTtmlRenderRenderedImage *rendered_block, *block_bg_image, *tmp;
    GstBuffer *block_bg_buf;
    gint block_height;

    block = gst_subtitle_region_get_block (region, i);
    rendered_block = gst_ttml_render_render_text_block (render, block, text_buf,
        window_width, TRUE);

    if (!rendered_block)
      continue;

    GST_CAT_LOG (ttmlrender_debug, "rendered_block - x:%d  y:%d  w:%u  h:%u",
        rendered_block->x, rendered_block->y, rendered_block->width,
        rendered_block->height);

    switch (block->style_set->text_align) {
      case GST_SUBTITLE_TEXT_ALIGN_CENTER:
        rendered_block->x
            += (gint) round ((window_width - rendered_block->width) / 2.0);
        break;

      case GST_SUBTITLE_TEXT_ALIGN_RIGHT:
      case GST_SUBTITLE_TEXT_ALIGN_END:
        rendered_block->x += (window_width - rendered_block->width);
        break;

      default:
        break;
    }

    tmp = rendered_block;

    block_height = rendered_block->height + (2 * rendered_block->y);
    block_bg_buf = gst_ttml_render_draw_rectangle (window_width,
        block_height, block->style_set->background_color);
    block_bg_image = gst_ttml_render_rendered_image_new (block_bg_buf, 0, 0,
        window_width, block_height);
    rendered_block = gst_ttml_render_rendered_image_combine (block_bg_image,
        rendered_block);
    gst_ttml_render_rendered_image_free (tmp);
    gst_ttml_render_rendered_image_free (block_bg_image);

    rendered_block->y = 0;
    g_ptr_array_add (rendered_blocks, rendered_block);
  }

  if (rendered_blocks->len > 0) {
    GstTtmlRenderRenderedImage *blocks_image, *tmp;

    blocks_image = gst_ttml_render_stitch_images (rendered_blocks,
        GST_TTML_DIRECTION_BLOCK);
    blocks_image->x += window_x;

    switch (region->style_set->display_align) {
      case GST_SUBTITLE_DISPLAY_ALIGN_BEFORE:
        blocks_image->y = window_y;
        break;
      case GST_SUBTITLE_DISPLAY_ALIGN_CENTER:
        blocks_image->y = region_y + ((gint) ((region_height + padding_before)
                - (padding_after + blocks_image->height))) / 2;
        break;
      case GST_SUBTITLE_DISPLAY_ALIGN_AFTER:
        blocks_image->y = (region_y + region_height)
            - (padding_after + blocks_image->height);
        break;
    }

    if ((region->style_set->overflow == GST_SUBTITLE_OVERFLOW_MODE_HIDDEN)
        && ((blocks_image->height > window_height)
            || (blocks_image->width > window_width))) {
      GstTtmlRenderRenderedImage *tmp = blocks_image;
      blocks_image = gst_ttml_render_rendered_image_crop (blocks_image,
          window_x, window_y, window_width, window_height);
      gst_ttml_render_rendered_image_free (tmp);
    }

    tmp = region_image;
    region_image =
        gst_ttml_render_rendered_image_combine (region_image, blocks_image);
    gst_ttml_render_rendered_image_free (tmp);
    gst_ttml_render_rendered_image_free (blocks_image);
  }

  if (region_image) {
    ret = gst_ttml_render_compose_overlay (region_image);
    gst_ttml_render_rendered_image_free (region_image);
  }

  g_ptr_array_unref (rendered_blocks);
  return ret;
}


static GstFlowReturn
gst_ttml_render_video_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer)
{
  GstTtmlRender *render;
  GstFlowReturn ret = GST_FLOW_OK;
  gboolean in_seg = FALSE;
  guint64 start, stop, clip_start = 0, clip_stop = 0;
  gchar *text = NULL;

  render = GST_TTML_RENDER (parent);

  if (!GST_BUFFER_TIMESTAMP_IS_VALID (buffer))
    goto missing_timestamp;

  /* ignore buffers that are outside of the current segment */
  start = GST_BUFFER_TIMESTAMP (buffer);

  if (!GST_BUFFER_DURATION_IS_VALID (buffer)) {
    stop = GST_CLOCK_TIME_NONE;
  } else {
    stop = start + GST_BUFFER_DURATION (buffer);
  }

  GST_LOG_OBJECT (render, "%" GST_SEGMENT_FORMAT "  BUFFER: ts=%"
      GST_TIME_FORMAT ", end=%" GST_TIME_FORMAT, &render->segment,
      GST_TIME_ARGS (start), GST_TIME_ARGS (stop));

  /* segment_clip() will adjust start unconditionally to segment_start if
   * no stop time is provided, so handle this ourselves */
  if (stop == GST_CLOCK_TIME_NONE && start < render->segment.start)
    goto out_of_segment;

  in_seg = gst_segment_clip (&render->segment, GST_FORMAT_TIME, start, stop,
      &clip_start, &clip_stop);

  if (!in_seg)
    goto out_of_segment;

  /* if the buffer is only partially in the segment, fix up stamps */
  if (clip_start != start || (stop != -1 && clip_stop != stop)) {
    GST_DEBUG_OBJECT (render, "clipping buffer timestamp/duration to segment");
    buffer = gst_buffer_make_writable (buffer);
    GST_BUFFER_TIMESTAMP (buffer) = clip_start;
    if (stop != -1)
      GST_BUFFER_DURATION (buffer) = clip_stop - clip_start;
  }

  /* now, after we've done the clipping, fix up end time if there's no
   * duration (we only use those estimated values internally though, we
   * don't want to set bogus values on the buffer itself) */
  if (stop == -1) {
    if (render->info.fps_n && render->info.fps_d) {
      GST_DEBUG_OBJECT (render, "estimating duration based on framerate");
      stop = start + gst_util_uint64_scale_int (GST_SECOND,
          render->info.fps_d, render->info.fps_n);
    } else {
      GST_LOG_OBJECT (render, "no duration, assuming minimal duration");
      stop = start + 1;         /* we need to assume some interval */
    }
  }

  gst_object_sync_values (GST_OBJECT (render), GST_BUFFER_TIMESTAMP (buffer));

wait_for_text_buf:

  GST_TTML_RENDER_LOCK (render);

  if (render->video_flushing)
    goto flushing;

  if (render->video_eos)
    goto have_eos;

  /* Text pad not linked; push input video frame */
  if (!render->text_linked) {
    GST_LOG_OBJECT (render, "Text pad not linked");
    GST_TTML_RENDER_UNLOCK (render);
    ret = gst_pad_push (render->srcpad, buffer);
    goto not_linked;
  }

  /* Text pad linked, check if we have a text buffer queued */
  if (render->text_buffer) {
    gboolean pop_text = FALSE, valid_text_time = TRUE;
    GstClockTime text_start = GST_CLOCK_TIME_NONE;
    GstClockTime text_end = GST_CLOCK_TIME_NONE;
    GstClockTime text_running_time = GST_CLOCK_TIME_NONE;
    GstClockTime text_running_time_end = GST_CLOCK_TIME_NONE;
    GstClockTime vid_running_time, vid_running_time_end;

    /* if the text buffer isn't stamped right, pop it off the
     * queue and display it for the current video frame only */
    if (!GST_BUFFER_TIMESTAMP_IS_VALID (render->text_buffer) ||
        !GST_BUFFER_DURATION_IS_VALID (render->text_buffer)) {
      GST_WARNING_OBJECT (render,
          "Got text buffer with invalid timestamp or duration");
      pop_text = TRUE;
      valid_text_time = FALSE;
    } else {
      text_start = GST_BUFFER_TIMESTAMP (render->text_buffer);
      text_end = text_start + GST_BUFFER_DURATION (render->text_buffer);
    }

    vid_running_time =
        gst_segment_to_running_time (&render->segment, GST_FORMAT_TIME, start);
    vid_running_time_end =
        gst_segment_to_running_time (&render->segment, GST_FORMAT_TIME, stop);

    /* If timestamp and duration are valid */
    if (valid_text_time) {
      text_running_time =
          gst_segment_to_running_time (&render->text_segment,
          GST_FORMAT_TIME, text_start);
      text_running_time_end =
          gst_segment_to_running_time (&render->text_segment,
          GST_FORMAT_TIME, text_end);
    }

    GST_LOG_OBJECT (render, "T: %" GST_TIME_FORMAT " - %" GST_TIME_FORMAT,
        GST_TIME_ARGS (text_running_time),
        GST_TIME_ARGS (text_running_time_end));
    GST_LOG_OBJECT (render, "V: %" GST_TIME_FORMAT " - %" GST_TIME_FORMAT,
        GST_TIME_ARGS (vid_running_time), GST_TIME_ARGS (vid_running_time_end));

    /* Text too old or in the future */
    if (valid_text_time && text_running_time_end <= vid_running_time) {
      /* text buffer too old, get rid of it and do nothing  */
      GST_LOG_OBJECT (render, "text buffer too old, popping");
      pop_text = FALSE;
      gst_ttml_render_pop_text (render);
      GST_TTML_RENDER_UNLOCK (render);
      goto wait_for_text_buf;
    } else if (valid_text_time && vid_running_time_end <= text_running_time) {
      GST_LOG_OBJECT (render, "text in future, pushing video buf");
      GST_TTML_RENDER_UNLOCK (render);
      /* Push the video frame */
      ret = gst_pad_push (render->srcpad, buffer);
    } else {
      if (render->need_render) {
        GstSubtitleRegion *region = NULL;
        GstSubtitleMeta *subtitle_meta = NULL;
        guint i;

        if (render->compositions) {
          g_list_free_full (render->compositions,
              (GDestroyNotify) gst_video_overlay_composition_unref);
          render->compositions = NULL;
        }

        subtitle_meta = gst_buffer_get_subtitle_meta (render->text_buffer);
        if (!subtitle_meta) {
          GST_CAT_WARNING (ttmlrender_debug, "Failed to get subtitle meta.");
        } else {
          for (i = 0; i < subtitle_meta->regions->len; ++i) {
            GstVideoOverlayComposition *composition;
            region = g_ptr_array_index (subtitle_meta->regions, i);
            composition = gst_ttml_render_render_text_region (render, region,
                render->text_buffer);
            if (composition) {
              render->compositions = g_list_append (render->compositions,
                  composition);
            }
          }
        }
        render->need_render = FALSE;
      }

      GST_TTML_RENDER_UNLOCK (render);
      ret = gst_ttml_render_push_frame (render, buffer);

      if (valid_text_time && text_running_time_end <= vid_running_time_end) {
        GST_LOG_OBJECT (render, "text buffer not needed any longer");
        pop_text = TRUE;
      }
    }
    if (pop_text) {
      GST_TTML_RENDER_LOCK (render);
      gst_ttml_render_pop_text (render);
      GST_TTML_RENDER_UNLOCK (render);
    }
  } else {
    gboolean wait_for_text_buf = TRUE;

    if (render->text_eos)
      wait_for_text_buf = FALSE;

    if (!render->wait_text)
      wait_for_text_buf = FALSE;

    /* Text pad linked, but no text buffer available - what now? */
    if (render->text_segment.format == GST_FORMAT_TIME) {
      GstClockTime text_start_running_time, text_position_running_time;
      GstClockTime vid_running_time;

      vid_running_time =
          gst_segment_to_running_time (&render->segment, GST_FORMAT_TIME,
          GST_BUFFER_TIMESTAMP (buffer));
      text_start_running_time =
          gst_segment_to_running_time (&render->text_segment,
          GST_FORMAT_TIME, render->text_segment.start);
      text_position_running_time =
          gst_segment_to_running_time (&render->text_segment,
          GST_FORMAT_TIME, render->text_segment.position);

      if ((GST_CLOCK_TIME_IS_VALID (text_start_running_time) &&
              vid_running_time < text_start_running_time) ||
          (GST_CLOCK_TIME_IS_VALID (text_position_running_time) &&
              vid_running_time < text_position_running_time)) {
        wait_for_text_buf = FALSE;
      }
    }

    if (wait_for_text_buf) {
      GST_DEBUG_OBJECT (render, "no text buffer, need to wait for one");
      GST_TTML_RENDER_WAIT (render);
      GST_DEBUG_OBJECT (render, "resuming");
      GST_TTML_RENDER_UNLOCK (render);
      goto wait_for_text_buf;
    } else {
      GST_TTML_RENDER_UNLOCK (render);
      GST_LOG_OBJECT (render, "no need to wait for a text buffer");
      ret = gst_pad_push (render->srcpad, buffer);
    }
  }

not_linked:
  g_free (text);

  /* Update position */
  render->segment.position = clip_start;

  return ret;

missing_timestamp:
  {
    GST_WARNING_OBJECT (render, "buffer without timestamp, discarding");
    gst_buffer_unref (buffer);
    return GST_FLOW_OK;
  }

flushing:
  {
    GST_TTML_RENDER_UNLOCK (render);
    GST_DEBUG_OBJECT (render, "flushing, discarding buffer");
    gst_buffer_unref (buffer);
    return GST_FLOW_FLUSHING;
  }
have_eos:
  {
    GST_TTML_RENDER_UNLOCK (render);
    GST_DEBUG_OBJECT (render, "eos, discarding buffer");
    gst_buffer_unref (buffer);
    return GST_FLOW_EOS;
  }
out_of_segment:
  {
    GST_DEBUG_OBJECT (render, "buffer out of segment, discarding");
    gst_buffer_unref (buffer);
    return GST_FLOW_OK;
  }
}

static GstStateChangeReturn
gst_ttml_render_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstTtmlRender *render = GST_TTML_RENDER (element);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_TTML_RENDER_LOCK (render);
      render->text_flushing = TRUE;
      render->video_flushing = TRUE;
      /* pop_text will broadcast on the GCond and thus also make the video
       * chain exit if it's waiting for a text buffer */
      gst_ttml_render_pop_text (render);
      GST_TTML_RENDER_UNLOCK (render);
      break;
    default:
      break;
  }

  ret = parent_class->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      GST_TTML_RENDER_LOCK (render);
      render->text_flushing = FALSE;
      render->video_flushing = FALSE;
      render->video_eos = FALSE;
      render->text_eos = FALSE;
      gst_segment_init (&render->segment, GST_FORMAT_TIME);
      gst_segment_init (&render->text_segment, GST_FORMAT_TIME);
      GST_TTML_RENDER_UNLOCK (render);
      break;
    default:
      break;
  }

  return ret;
}

static gboolean
gst_element_ttmlrender_init (GstPlugin * plugin)
{
  guint rank = GST_RANK_NONE;

  ttml_element_init (plugin);

  GST_DEBUG_CATEGORY_INIT (ttmlrender_debug, "ttmlrender", 0, "TTML renderer");

  /* We don't want this autoplugged by default yet for now */
  if (g_getenv ("GST_TTML_AUTOPLUG")) {
    GST_INFO_OBJECT (plugin, "Registering ttml elements with primary rank.");
    rank = GST_RANK_PRIMARY;
  }

  return gst_element_register (plugin, "ttmlrender", rank,
      GST_TYPE_TTML_RENDER);
}
