/* GStreamer
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *     @Author: Chengjun Wang <cjun.wang@samsung.com>
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
#include <config.h>
#endif

#include <gst/video/video.h>
#include <gst/video/gstvideometa.h>
#include <gst/base/gstbytereader.h>

#include "gstceaccoverlay.h"
#include <string.h>


#define GST_CAT_DEFAULT gst_cea_cc_overlay_debug
GST_DEBUG_CATEGORY (gst_cea_cc_overlay_debug);


#define DEFAULT_PROP_FONT_DESC	""
#define DEFAULT_PROP_SILENT	FALSE
#define DEFAULT_PROP_SERVICE_NUMBER 1
#define DEFAULT_PROP_WINDOW_H_POS GST_CEA_CC_OVERLAY_WIN_H_CENTER

enum
{
  PROP_0,
  PROP_FONT_DESC,
  PROP_SILENT,
  PROP_SERVICE_NUMBER,
  PROP_WINDOW_H_POS,
  PROP_LAST
};

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
# define CAIRO_ARGB_A 3
# define CAIRO_ARGB_R 2
# define CAIRO_ARGB_G 1
# define CAIRO_ARGB_B 0
#else
# define CAIRO_ARGB_A 0
# define CAIRO_ARGB_R 1
# define CAIRO_ARGB_G 2
# define CAIRO_ARGB_B 3
#endif

#define CAIRO_UNPREMULTIPLY(a,r,g,b) G_STMT_START { \
  b = (a > 0) ? MIN ((b * 255 + a / 2) / a, 255) : 0; \
  g = (a > 0) ? MIN ((g * 255 + a / 2) / a, 255) : 0; \
  r = (a > 0) ? MIN ((r * 255 + a / 2) / a, 255) : 0; \
} G_STMT_END


#define VIDEO_FORMATS GST_VIDEO_OVERLAY_COMPOSITION_BLEND_FORMATS

#define CC_OVERLAY_CAPS GST_VIDEO_CAPS_MAKE (VIDEO_FORMATS)

#define CC_OVERLAY_ALL_CAPS CC_OVERLAY_CAPS ";" \
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("ANY", GST_VIDEO_FORMATS_ALL)

static GstStaticCaps sw_template_caps = GST_STATIC_CAPS (CC_OVERLAY_CAPS);

static GstStaticPadTemplate src_template_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CC_OVERLAY_ALL_CAPS)
    );

static GstStaticPadTemplate video_sink_template_factory =
GST_STATIC_PAD_TEMPLATE ("video_sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CC_OVERLAY_ALL_CAPS)
    );

static GstStaticPadTemplate cc_sink_template_factory =
GST_STATIC_PAD_TEMPLATE ("cc_sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    ("closedcaption/x-cea-708, format={ (string) cdp, (string) cc_data }")
    );


#define GST_TYPE_CC_OVERLAY_WIN_H_POS (gst_cea_cc_overlay_h_pos_get_type())
static GType
gst_cea_cc_overlay_h_pos_get_type (void)
{
  static GType cc_overlay_win_h_pos_type = 0;
  static const GEnumValue cc_overlay_win_h_pos[] = {
    {GST_CEA_CC_OVERLAY_WIN_H_LEFT, "left", "left"},
    {GST_CEA_CC_OVERLAY_WIN_H_CENTER, "center", "center"},
    {GST_CEA_CC_OVERLAY_WIN_H_RIGHT, "right", "right"},
    {GST_CEA_CC_OVERLAY_WIN_H_AUTO, "auto", "auto"},
    {0, NULL, NULL},
  };

  if (!cc_overlay_win_h_pos_type) {
    cc_overlay_win_h_pos_type =
        g_enum_register_static ("GstCeaCcOverlayWinHPos", cc_overlay_win_h_pos);
  }
  return cc_overlay_win_h_pos_type;
}


#define GST_CEA_CC_OVERLAY_GET_LOCK(ov) (&GST_CEA_CC_OVERLAY (ov)->lock)
#define GST_CEA_CC_OVERLAY_GET_COND(ov) (&GST_CEA_CC_OVERLAY (ov)->cond)
#define GST_CEA_CC_OVERLAY_LOCK(ov)     (g_mutex_lock (GST_CEA_CC_OVERLAY_GET_LOCK (ov)))
#define GST_CEA_CC_OVERLAY_UNLOCK(ov)   (g_mutex_unlock (GST_CEA_CC_OVERLAY_GET_LOCK (ov)))
#define GST_CEA_CC_OVERLAY_WAIT(ov)     (g_cond_wait (GST_CEA_CC_OVERLAY_GET_COND (ov), GST_CEA_CC_OVERLAY_GET_LOCK (ov)))
#define GST_CEA_CC_OVERLAY_SIGNAL(ov)   (g_cond_signal (GST_CEA_CC_OVERLAY_GET_COND (ov)))
#define GST_CEA_CC_OVERLAY_BROADCAST(ov)(g_cond_broadcast (GST_CEA_CC_OVERLAY_GET_COND (ov)))

static GstElementClass *parent_class = NULL;
static void gst_base_cea_cc_overlay_base_init (gpointer g_class);
static void gst_base_cea_cc_overlay_class_init (GstCeaCcOverlayClass * klass);
static void gst_base_cea_cc_overlay_init (GstCeaCcOverlay * overlay,
    GstCeaCcOverlayClass * klass);
static GstStateChangeReturn gst_cea_cc_overlay_change_state (GstElement *
    element, GstStateChange transition);
static GstCaps *gst_cea_cc_overlay_get_videosink_caps (GstPad * pad,
    GstCeaCcOverlay * overlay, GstCaps * filter);
static GstCaps *gst_cea_cc_overlay_get_src_caps (GstPad * pad,
    GstCeaCcOverlay * overlay, GstCaps * filter);
static gboolean gst_cea_cc_overlay_setcaps (GstCeaCcOverlay * overlay,
    GstCaps * caps);
static gboolean gst_cea_cc_overlay_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_cea_cc_overlay_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query);

static gboolean gst_cea_cc_overlay_video_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static gboolean gst_cea_cc_overlay_video_query (GstPad * pad,
    GstObject * parent, GstQuery * query);
static GstFlowReturn gst_cea_cc_overlay_video_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buffer);

static gboolean gst_cea_cc_overlay_cc_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static GstFlowReturn gst_cea_cc_overlay_cc_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buffer);
static GstPadLinkReturn gst_cea_cc_overlay_cc_pad_link (GstPad * pad,
    GstObject * parent, GstPad * peer);
static void gst_cea_cc_overlay_cc_pad_unlink (GstPad * pad, GstObject * parent);
static void gst_cea_cc_overlay_pop_text (GstCeaCcOverlay * overlay);
static void gst_cea_cc_overlay_finalize (GObject * object);
static void gst_cea_cc_overlay_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_cea_cc_overlay_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_cea_cc_overlay_can_handle_caps (GstCaps * incaps);

GType
gst_cea_cc_overlay_get_type (void)
{
  static GType type = 0;

  if (g_once_init_enter ((gsize *) & type)) {
    static const GTypeInfo info = {
      sizeof (GstCeaCcOverlayClass),
      (GBaseInitFunc) gst_base_cea_cc_overlay_base_init,
      NULL,
      (GClassInitFunc) gst_base_cea_cc_overlay_class_init,
      NULL,
      NULL,
      sizeof (GstCeaCcOverlay),
      0,
      (GInstanceInitFunc) gst_base_cea_cc_overlay_init,
    };

    g_once_init_leave ((gsize *) & type,
        g_type_register_static (GST_TYPE_ELEMENT, "GstCeaCcOverlay", &info, 0));
  }

  return type;
}

GST_ELEMENT_REGISTER_DEFINE (cc708overlay, "cc708overlay",
    GST_RANK_PRIMARY, GST_TYPE_CEA_CC_OVERLAY);

static void
gst_base_cea_cc_overlay_base_init (gpointer g_class)
{
  GstCeaCcOverlayClass *klass = GST_CEA_CC_OVERLAY_CLASS (g_class);
  PangoFontMap *fontmap;

  /* Only lock for the subclasses here, the base class
   * doesn't have this mutex yet and it's not necessary
   * here */
  fontmap = pango_cairo_font_map_get_default ();
  klass->pango_context =
      pango_font_map_create_context (PANGO_FONT_MAP (fontmap));
}

static void
gst_base_cea_cc_overlay_class_init (GstCeaCcOverlayClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  GST_DEBUG_CATEGORY_INIT (gst_cea_cc_overlay_debug, "cc708overlay", 0,
      "cc708overlay");

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = gst_cea_cc_overlay_finalize;
  gobject_class->set_property = gst_cea_cc_overlay_set_property;
  gobject_class->get_property = gst_cea_cc_overlay_get_property;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&video_sink_template_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&cc_sink_template_factory));

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_cea_cc_overlay_change_state);

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_SERVICE_NUMBER,
      g_param_spec_int ("service-number", "service-number",
          "Service number. Service 1 is designated as the Primary Caption Service,"
          " Service 2 is the Secondary Language Service.",
          -1, 63, DEFAULT_PROP_SERVICE_NUMBER,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_WINDOW_H_POS,
      g_param_spec_enum ("window-h-pos", "window-h-pos",
          "Window's Horizontal position", GST_TYPE_CC_OVERLAY_WIN_H_POS,
          DEFAULT_PROP_WINDOW_H_POS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_FONT_DESC,
      g_param_spec_string ("font-desc", "font description",
          "Pango font description of font to be used for rendering.\n"
          "See documentation of pango_font_description_from_string for syntax.\n"
          "this will override closed caption stream specified font style/pen size.",
          DEFAULT_PROP_FONT_DESC, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstCeaCcOverlay:silent:
   *
   * If set, no text is rendered. Useful to switch off text rendering
   * temporarily without removing the textoverlay element from the pipeline.
   */
  /* FIXME 0.11: rename to "visible" or "text-visible" or "render-text" */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_SILENT,
      g_param_spec_boolean ("silent", "silent",
          "Whether to render the text string",
          DEFAULT_PROP_SILENT,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (gstelement_class,
      "Closed Caption overlay", "Mixer/Video/Overlay/Subtitle",
      "Decode cea608/cea708 data and overlay on proper position of a video buffer",
      "Chengjun Wang <cjun.wang@samsung.com>");
  gst_cea708_decoder_init_debug ();

  gst_type_mark_as_plugin_api (GST_TYPE_CC_OVERLAY_WIN_H_POS, 0);

}

static void
gst_cea_cc_overlay_finalize (GObject * object)
{
  GstCeaCcOverlay *overlay = GST_CEA_CC_OVERLAY (object);

  if (overlay->current_composition) {
    gst_video_overlay_composition_unref (overlay->current_composition);
    overlay->current_composition = NULL;
  }
  if (overlay->next_composition) {
    gst_video_overlay_composition_unref (overlay->next_composition);
    overlay->next_composition = NULL;
  }

  gst_cea708dec_free (overlay->decoder);
  overlay->decoder = NULL;

  g_mutex_clear (&overlay->lock);
  g_cond_clear (&overlay->cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_base_cea_cc_overlay_init (GstCeaCcOverlay * overlay,
    GstCeaCcOverlayClass * klass)
{
  GstPadTemplate *template;
  overlay->decoder = gst_cea708dec_create (GST_CEA_CC_OVERLAY_GET_CLASS
      (overlay)->pango_context);

  /* video sink */
  template = gst_static_pad_template_get (&video_sink_template_factory);
  overlay->video_sinkpad = gst_pad_new_from_template (template, "video_sink");
  gst_object_unref (template);
  gst_pad_set_event_function (overlay->video_sinkpad,
      GST_DEBUG_FUNCPTR (gst_cea_cc_overlay_video_event));
  gst_pad_set_chain_function (overlay->video_sinkpad,
      GST_DEBUG_FUNCPTR (gst_cea_cc_overlay_video_chain));
  gst_pad_set_query_function (overlay->video_sinkpad,
      GST_DEBUG_FUNCPTR (gst_cea_cc_overlay_video_query));
  GST_PAD_SET_PROXY_ALLOCATION (overlay->video_sinkpad);
  gst_element_add_pad (GST_ELEMENT (overlay), overlay->video_sinkpad);

  template =
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass), "cc_sink");
  if (template) {
    /* text sink */
    overlay->cc_sinkpad = gst_pad_new_from_template (template, "cc_sink");

    gst_pad_set_event_function (overlay->cc_sinkpad,
        GST_DEBUG_FUNCPTR (gst_cea_cc_overlay_cc_event));
    gst_pad_set_chain_function (overlay->cc_sinkpad,
        GST_DEBUG_FUNCPTR (gst_cea_cc_overlay_cc_chain));
    gst_pad_set_link_function (overlay->cc_sinkpad,
        GST_DEBUG_FUNCPTR (gst_cea_cc_overlay_cc_pad_link));
    gst_pad_set_unlink_function (overlay->cc_sinkpad,
        GST_DEBUG_FUNCPTR (gst_cea_cc_overlay_cc_pad_unlink));
    gst_element_add_pad (GST_ELEMENT (overlay), overlay->cc_sinkpad);
  }

  /* (video) source */
  template = gst_static_pad_template_get (&src_template_factory);
  overlay->srcpad = gst_pad_new_from_template (template, "src");
  gst_object_unref (template);
  gst_pad_set_event_function (overlay->srcpad,
      GST_DEBUG_FUNCPTR (gst_cea_cc_overlay_src_event));
  gst_pad_set_query_function (overlay->srcpad,
      GST_DEBUG_FUNCPTR (gst_cea_cc_overlay_src_query));
  gst_element_add_pad (GST_ELEMENT (overlay), overlay->srcpad);


  overlay->silent = DEFAULT_PROP_SILENT;
  overlay->need_update = TRUE;
  overlay->current_composition = NULL;
  overlay->next_composition = NULL;
  overlay->cc_pad_linked = FALSE;
  overlay->current_comp_start_time = GST_CLOCK_TIME_NONE;
  overlay->next_comp_start_time = GST_CLOCK_TIME_NONE;
  overlay->cea608_index[0] = 0;
  overlay->cea608_index[1] = 0;
  overlay->cea708_index = 0;
  overlay->default_window_h_pos = DEFAULT_PROP_WINDOW_H_POS;

  g_mutex_init (&overlay->lock);
  g_cond_init (&overlay->cond);
  gst_segment_init (&overlay->segment, GST_FORMAT_TIME);
}

/* only negotiate/query video overlay composition support for now */
static gboolean
gst_cea_cc_overlay_negotiate (GstCeaCcOverlay * overlay, GstCaps * caps)
{
  GstQuery *query;
  gboolean attach = FALSE;
  gboolean caps_has_meta = TRUE;
  gboolean ret;
  GstCapsFeatures *f;
  GstCaps *original_caps;
  gboolean original_has_meta = FALSE;
  gboolean allocation_ret = TRUE;

  GST_DEBUG_OBJECT (overlay, "performing negotiation");

  if (!caps)
    caps = gst_pad_get_current_caps (overlay->video_sinkpad);
  else
    gst_caps_ref (caps);

  if (!caps || gst_caps_is_empty (caps))
    goto no_format;

  original_caps = caps;

  /* Try to use the overlay meta if possible */
  f = gst_caps_get_features (caps, 0);

  /* if the caps doesn't have the overlay meta, we query if downstream
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

    ret = gst_pad_peer_query_accept_caps (overlay->srcpad, overlay_caps);
    GST_DEBUG_OBJECT (overlay, "Downstream accepts the overlay meta: %d", ret);
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
  GST_DEBUG_OBJECT (overlay, "Using caps %" GST_PTR_FORMAT, caps);
  ret = gst_pad_set_caps (overlay->srcpad, caps);

  if (ret) {
    /* find supported meta */
    query = gst_query_new_allocation (caps, FALSE);

    if (!gst_pad_peer_query (overlay->srcpad, query)) {
      /* no problem, we use the query defaults */
      GST_DEBUG_OBJECT (overlay, "ALLOCATION query failed");
      allocation_ret = FALSE;
    }

    if (caps_has_meta && gst_query_find_allocation_meta (query,
            GST_VIDEO_OVERLAY_COMPOSITION_META_API_TYPE, NULL))
      attach = TRUE;
    gst_query_unref (query);
  }

  overlay->attach_compo_to_buffer = attach;

  if (!allocation_ret && overlay->video_flushing) {
    ret = FALSE;
  } else if (original_caps && !original_has_meta && !attach) {
    if (caps_has_meta) {
      /* Some elements (fakesink) claim to accept the meta on caps but won't
         put it in the allocation query result, this leads below
         check to fail. Prevent this by removing the meta from caps */
      gst_caps_unref (caps);
      caps = gst_caps_ref (original_caps);
      ret = gst_pad_set_caps (overlay->srcpad, caps);
      if (ret && !gst_cea_cc_overlay_can_handle_caps (caps))
        ret = FALSE;
    }
  }

  if (!ret) {
    GST_DEBUG_OBJECT (overlay, "negotiation failed, schedule reconfigure");
    gst_pad_mark_reconfigure (overlay->srcpad);
  }
  gst_caps_unref (caps);
  GST_DEBUG_OBJECT (overlay, "ret=%d", ret);

  return ret;

no_format:
  {
    if (caps)
      gst_caps_unref (caps);
    return FALSE;
  }
}

static gboolean
gst_cea_cc_overlay_can_handle_caps (GstCaps * incaps)
{
  gboolean ret;
  GstCaps *caps;
  static GstStaticCaps static_caps = GST_STATIC_CAPS (CC_OVERLAY_CAPS);

  caps = gst_static_caps_get (&static_caps);
  ret = gst_caps_is_subset (incaps, caps);
  gst_caps_unref (caps);

  return ret;
}

static gboolean
gst_cea_cc_overlay_setcaps (GstCeaCcOverlay * overlay, GstCaps * caps)
{
  GstVideoInfo info;
  gboolean ret = FALSE;

  if (!gst_video_info_from_caps (&info, caps))
    goto invalid_caps;

  overlay->info = info;
  overlay->format = GST_VIDEO_INFO_FORMAT (&info);
  overlay->width = GST_VIDEO_INFO_WIDTH (&info);
  overlay->height = GST_VIDEO_INFO_HEIGHT (&info);
  gst_cea708dec_set_video_width_height (overlay->decoder, overlay->width,
      overlay->height);
  ret = gst_cea_cc_overlay_negotiate (overlay, caps);

  GST_CEA_CC_OVERLAY_LOCK (overlay);
  if (!overlay->attach_compo_to_buffer &&
      !gst_cea_cc_overlay_can_handle_caps (caps)) {
    GST_DEBUG_OBJECT (overlay, "unsupported caps %" GST_PTR_FORMAT, caps);
    ret = FALSE;
  }
  GST_CEA_CC_OVERLAY_UNLOCK (overlay);

  return ret;

  /* ERRORS */
invalid_caps:
  {
    GST_DEBUG_OBJECT (overlay, "could not parse caps");
    return FALSE;
  }
}

static void
gst_cea_cc_overlay_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCeaCcOverlay *overlay = GST_CEA_CC_OVERLAY (object);
  Cea708Dec *decoder = overlay->decoder;

  GST_CEA_CC_OVERLAY_LOCK (overlay);
  switch (prop_id) {
    case PROP_SERVICE_NUMBER:
    {
      int desired_service = g_value_get_int (value);
      gst_cea708dec_set_service_number (decoder, desired_service);
      break;
    }
    case PROP_FONT_DESC:
    {
      PangoFontDescription *desc = NULL;
      const gchar *fontdesc_str;
      fontdesc_str = g_value_get_string (value);

      GST_LOG_OBJECT (overlay, "Got font description '%s'", fontdesc_str);
      if (fontdesc_str)
        desc = pango_font_description_from_string (fontdesc_str);
      /* Only set if NULL or valid description */
      if (desc || !fontdesc_str) {
        if (desc) {
          GST_INFO_OBJECT (overlay, "Setting font description: '%s'",
              fontdesc_str);
          pango_font_description_free (desc);
        } else
          GST_INFO_OBJECT (overlay, "Resetting default font description");
        g_free (decoder->default_font_desc);
        decoder->default_font_desc = g_strdup (fontdesc_str);
      }
      break;
    }
    case PROP_SILENT:
      overlay->silent = g_value_get_boolean (value);
      break;
    case PROP_WINDOW_H_POS:
      overlay->default_window_h_pos = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  overlay->need_update = TRUE;
  GST_CEA_CC_OVERLAY_UNLOCK (overlay);
}

static void
gst_cea_cc_overlay_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstCeaCcOverlay *overlay = GST_CEA_CC_OVERLAY (object);
  Cea708Dec *decoder = overlay->decoder;

  GST_CEA_CC_OVERLAY_LOCK (overlay);
  switch (prop_id) {
    case PROP_SERVICE_NUMBER:
      g_value_set_int (value, decoder->desired_service);
      break;
    case PROP_SILENT:
      g_value_set_boolean (value, overlay->silent);
      break;
    case PROP_FONT_DESC:
      g_value_set_string (value, decoder->default_font_desc);
      break;
    case PROP_WINDOW_H_POS:
      g_value_set_enum (value, overlay->default_window_h_pos);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_CEA_CC_OVERLAY_UNLOCK (overlay);
}

static gboolean
gst_cea_cc_overlay_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query)
{
  gboolean ret = FALSE;
  GstCeaCcOverlay *overlay;

  overlay = GST_CEA_CC_OVERLAY (parent);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstCaps *filter, *caps;

      gst_query_parse_caps (query, &filter);
      caps = gst_cea_cc_overlay_get_src_caps (pad, overlay, filter);
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
gst_cea_cc_overlay_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstCeaCcOverlay *overlay;
  gboolean ret;

  overlay = GST_CEA_CC_OVERLAY (parent);

  if (overlay->cc_pad_linked) {
    gst_event_ref (event);
    ret = gst_pad_push_event (overlay->video_sinkpad, event);
    gst_pad_push_event (overlay->cc_sinkpad, event);
  } else {
    ret = gst_pad_push_event (overlay->video_sinkpad, event);
  }

  return ret;
}

/**
 * gst_cea_cc_overlay_add_feature_and_intersect:
 *
 * Creates a new #GstCaps containing the (given caps +
 * given caps feature) + (given caps intersected by the
 * given filter).
 *
 * Returns: the new #GstCaps
 */
static GstCaps *
gst_cea_cc_overlay_add_feature_and_intersect (GstCaps * caps,
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
 * gst_cea_cc_overlay_intersect_by_feature:
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
gst_cea_cc_overlay_intersect_by_feature (GstCaps * caps,
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
gst_cea_cc_overlay_get_videosink_caps (GstPad * pad,
    GstCeaCcOverlay * overlay, GstCaps * filter)
{
  GstPad *srcpad = overlay->srcpad;
  GstCaps *peer_caps = NULL, *caps = NULL, *overlay_filter = NULL;

  if (G_UNLIKELY (!overlay))
    return gst_pad_get_pad_template_caps (pad);

  if (filter) {
    /* filter caps + composition feature + filter caps
     * filtered by the software caps. */
    GstCaps *sw_caps = gst_static_caps_get (&sw_template_caps);
    overlay_filter = gst_cea_cc_overlay_add_feature_and_intersect (filter,
        GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION, sw_caps);
    gst_caps_unref (sw_caps);

    GST_DEBUG_OBJECT (overlay, "overlay filter %" GST_PTR_FORMAT,
        overlay_filter);
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
      caps = gst_cea_cc_overlay_intersect_by_feature (peer_caps,
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

  GST_DEBUG_OBJECT (overlay, "returning  %" GST_PTR_FORMAT, caps);

  return caps;
}

static GstCaps *
gst_cea_cc_overlay_get_src_caps (GstPad * pad, GstCeaCcOverlay * overlay,
    GstCaps * filter)
{
  GstPad *sinkpad = overlay->video_sinkpad;
  GstCaps *peer_caps = NULL, *caps = NULL, *overlay_filter = NULL;

  if (G_UNLIKELY (!overlay))
    return gst_pad_get_pad_template_caps (pad);

  if (filter) {
    /* duplicate filter caps which contains the composition into one version
     * with the meta and one without. Filter the other caps by the software
     * caps */
    GstCaps *sw_caps = gst_static_caps_get (&sw_template_caps);
    overlay_filter =
        gst_cea_cc_overlay_intersect_by_feature (filter,
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
      caps = gst_cea_cc_overlay_add_feature_and_intersect (peer_caps,
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
  GST_DEBUG_OBJECT (overlay, "returning  %" GST_PTR_FORMAT, caps);

  return caps;
}

/* FIXME: should probably be relative to width/height (adjusted for PAR) */
#define BOX_XPAD  6
#define BOX_YPAD  6

static GstFlowReturn
gst_cea_cc_overlay_push_frame (GstCeaCcOverlay * overlay,
    GstBuffer * video_frame)
{
  GstVideoFrame frame;

  if (overlay->current_composition == NULL)
    goto done;
  GST_LOG_OBJECT (overlay, "gst_cea_cc_overlay_push_frame");

  if (gst_pad_check_reconfigure (overlay->srcpad))
    gst_cea_cc_overlay_negotiate (overlay, NULL);

  video_frame = gst_buffer_make_writable (video_frame);

  if (overlay->attach_compo_to_buffer) {
    GST_DEBUG_OBJECT (overlay, "Attaching text overlay image to video buffer");
    gst_buffer_add_video_overlay_composition_meta (video_frame,
        overlay->current_composition);
    goto done;
  }

  if (!gst_video_frame_map (&frame, &overlay->info, video_frame,
          GST_MAP_READWRITE))
    goto invalid_frame;

  gst_video_overlay_composition_blend (overlay->current_composition, &frame);

  gst_video_frame_unmap (&frame);

done:

  return gst_pad_push (overlay->srcpad, video_frame);

  /* ERRORS */
invalid_frame:
  {
    gst_buffer_unref (video_frame);
    return GST_FLOW_OK;
  }
}

static GstPadLinkReturn
gst_cea_cc_overlay_cc_pad_link (GstPad * pad, GstObject * parent, GstPad * peer)
{
  GstCeaCcOverlay *overlay;

  overlay = GST_CEA_CC_OVERLAY (parent);
  if (G_UNLIKELY (!overlay))
    return GST_PAD_LINK_REFUSED;

  GST_DEBUG_OBJECT (overlay, "Closed Caption pad linked");

  overlay->cc_pad_linked = TRUE;

  return GST_PAD_LINK_OK;
}

static void
gst_cea_cc_overlay_cc_pad_unlink (GstPad * pad, GstObject * parent)
{
  GstCeaCcOverlay *overlay;

  /* don't use gst_pad_get_parent() here, will deadlock */
  overlay = GST_CEA_CC_OVERLAY (parent);

  GST_DEBUG_OBJECT (overlay, "Closed Caption pad unlinked");

  overlay->cc_pad_linked = FALSE;

  gst_segment_init (&overlay->cc_segment, GST_FORMAT_UNDEFINED);
}

static gboolean
gst_cea_cc_overlay_cc_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean ret = FALSE;
  GstCeaCcOverlay *overlay = NULL;

  overlay = GST_CEA_CC_OVERLAY (parent);

  GST_LOG_OBJECT (overlay, "received event %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;
      GstStructure *st;
      const gchar *cctype;

      gst_event_parse_caps (event, &caps);
      st = gst_caps_get_structure (caps, 0);
      cctype = gst_structure_get_string (st, "format");
      overlay->is_cdp = !g_strcmp0 (cctype, "cdp");
      ret = TRUE;
      break;
    }
    case GST_EVENT_SEGMENT:
    {
      const GstSegment *segment;

      overlay->cc_eos = FALSE;

      gst_event_parse_segment (event, &segment);

      if (segment->format == GST_FORMAT_TIME) {
        GST_CEA_CC_OVERLAY_LOCK (overlay);
        gst_segment_copy_into (segment, &overlay->cc_segment);
        GST_DEBUG_OBJECT (overlay, "TEXT SEGMENT now: %" GST_SEGMENT_FORMAT,
            &overlay->cc_segment);
        GST_CEA_CC_OVERLAY_UNLOCK (overlay);
      } else {
        GST_ELEMENT_WARNING (overlay, STREAM, MUX, (NULL),
            ("received non-TIME newsegment event on text input"));
      }

      ret = TRUE;

      /* wake up the video chain, it might be waiting for a text buffer or
       * a text segment update */
      GST_CEA_CC_OVERLAY_LOCK (overlay);
      GST_CEA_CC_OVERLAY_BROADCAST (overlay);
      GST_CEA_CC_OVERLAY_UNLOCK (overlay);
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
      overlay->cc_segment.position = start;

      /* wake up the video chain, it might be waiting for a text buffer or
       * a text segment update */
      GST_CEA_CC_OVERLAY_LOCK (overlay);
      GST_CEA_CC_OVERLAY_BROADCAST (overlay);
      GST_CEA_CC_OVERLAY_UNLOCK (overlay);

      ret = TRUE;
      break;
    }
    case GST_EVENT_FLUSH_STOP:
      GST_CEA_CC_OVERLAY_LOCK (overlay);
      GST_INFO_OBJECT (overlay, "text flush stop");
      overlay->cc_flushing = FALSE;
      overlay->cc_eos = FALSE;
      gst_cea_cc_overlay_pop_text (overlay);
      gst_segment_init (&overlay->cc_segment, GST_FORMAT_TIME);
      GST_CEA_CC_OVERLAY_UNLOCK (overlay);
      ret = TRUE;
      break;
    case GST_EVENT_FLUSH_START:
      GST_CEA_CC_OVERLAY_LOCK (overlay);
      GST_INFO_OBJECT (overlay, "text flush start");
      overlay->cc_flushing = TRUE;
      GST_CEA_CC_OVERLAY_BROADCAST (overlay);
      GST_CEA_CC_OVERLAY_UNLOCK (overlay);
      ret = TRUE;
      break;
    case GST_EVENT_EOS:
      GST_CEA_CC_OVERLAY_LOCK (overlay);
      overlay->cc_eos = TRUE;
      GST_INFO_OBJECT (overlay, "closed caption EOS");
      /* wake up the video chain, it might be waiting for a text buffer or
       * a text segment update */
      GST_CEA_CC_OVERLAY_BROADCAST (overlay);
      GST_CEA_CC_OVERLAY_UNLOCK (overlay);
      ret = TRUE;
      break;
    default:
      break;
  }

  if (ret) {
    gst_event_unref (event);
  } else {
    ret = gst_pad_event_default (pad, parent, event);
  }

  return ret;
}

static gboolean
gst_cea_cc_overlay_video_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  gboolean ret = FALSE;
  GstCeaCcOverlay *overlay = NULL;

  overlay = GST_CEA_CC_OVERLAY (parent);

  GST_DEBUG_OBJECT (pad, "received event %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      ret = gst_cea_cc_overlay_setcaps (overlay, caps);
      gst_event_unref (event);
      break;
    }
    case GST_EVENT_SEGMENT:
    {
      const GstSegment *segment;

      GST_DEBUG_OBJECT (overlay, "received new segment");

      gst_event_parse_segment (event, &segment);

      if (segment->format == GST_FORMAT_TIME) {
        GST_DEBUG_OBJECT (overlay, "VIDEO SEGMENT now: %" GST_SEGMENT_FORMAT,
            &overlay->segment);

        gst_segment_copy_into (segment, &overlay->segment);
      } else {
        GST_ELEMENT_WARNING (overlay, STREAM, MUX, (NULL),
            ("received non-TIME newsegment event on video input"));
      }

      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
    case GST_EVENT_EOS:
      GST_CEA_CC_OVERLAY_LOCK (overlay);
      GST_INFO_OBJECT (overlay, "video EOS");
      overlay->video_eos = TRUE;
      GST_CEA_CC_OVERLAY_UNLOCK (overlay);
      ret = gst_pad_event_default (pad, parent, event);
      break;
    case GST_EVENT_FLUSH_START:
      GST_CEA_CC_OVERLAY_LOCK (overlay);
      GST_INFO_OBJECT (overlay, "video flush start");
      overlay->video_flushing = TRUE;
      GST_CEA_CC_OVERLAY_BROADCAST (overlay);
      GST_CEA_CC_OVERLAY_UNLOCK (overlay);
      ret = gst_pad_event_default (pad, parent, event);
      break;
    case GST_EVENT_FLUSH_STOP:
      GST_CEA_CC_OVERLAY_LOCK (overlay);
      GST_INFO_OBJECT (overlay, "video flush stop");
      overlay->video_flushing = FALSE;
      overlay->video_eos = FALSE;
      gst_segment_init (&overlay->segment, GST_FORMAT_TIME);
      GST_CEA_CC_OVERLAY_UNLOCK (overlay);
      ret = gst_pad_event_default (pad, parent, event);
      break;
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }

  return ret;
}

static gboolean
gst_cea_cc_overlay_video_query (GstPad * pad, GstObject * parent,
    GstQuery * query)
{
  gboolean ret = FALSE;
  GstCeaCcOverlay *overlay;

  overlay = GST_CEA_CC_OVERLAY (parent);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstCaps *filter, *caps;

      gst_query_parse_caps (query, &filter);
      caps = gst_cea_cc_overlay_get_videosink_caps (pad, overlay, filter);
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
gst_cea_cc_overlay_pop_text (GstCeaCcOverlay * overlay)
{
  g_return_if_fail (GST_IS_CEA_CC_OVERLAY (overlay));

  if (GST_CLOCK_TIME_IS_VALID (overlay->current_comp_start_time)
      && overlay->current_composition) {
    GST_DEBUG_OBJECT (overlay, "releasing composition %p",
        overlay->current_composition);
    gst_video_overlay_composition_unref (overlay->current_composition);
    overlay->current_composition = NULL;
    overlay->current_comp_start_time = GST_CLOCK_TIME_NONE;
  }

  /* Let the text task know we used that buffer */
  GST_CEA_CC_OVERLAY_BROADCAST (overlay);
}

static void
gst_cea_cc_overlay_image_to_argb (guchar * pixbuf,
    cea708Window * window, int stride)
{
  int i, j;
  guchar *p, *bitp;
  int width, height;

  width = window->image_width;
  height = window->image_height;

  for (i = 0; i < height; i++) {
    p = pixbuf + i * stride;
    bitp = window->text_image + i * width * 4;

    for (j = 0; j < width; j++) {
      p[0] = bitp[CAIRO_ARGB_A];
      p[1] = bitp[CAIRO_ARGB_R];
      p[2] = bitp[CAIRO_ARGB_G];
      p[3] = bitp[CAIRO_ARGB_B];

      /* Cairo uses pre-multiplied ARGB, unpremultiply it */
      CAIRO_UNPREMULTIPLY (p[0], p[1], p[2], p[3]);

      bitp += 4;
      p += 4;
    }
  }
}

static void
gst_cea_cc_overlay_image_to_ayuv (guchar * pixbuf,
    cea708Window * window, int stride)
{
  int y;                        /* text bitmap coordinates */
  guchar *p, *bitp;
  guchar a, r, g, b;
  int width, height;

  width = window->image_width;
  height = window->image_height;

  for (y = 0; y < height; y++) {
    int n;
    p = pixbuf + y * stride;
    bitp = window->text_image + y * width * 4;

    for (n = 0; n < width; n++) {
      b = bitp[CAIRO_ARGB_B];
      g = bitp[CAIRO_ARGB_G];
      r = bitp[CAIRO_ARGB_R];
      a = bitp[CAIRO_ARGB_A];
      bitp += 4;

      /* Cairo uses pre-multiplied ARGB, unpremultiply it */
      CAIRO_UNPREMULTIPLY (a, r, g, b);

      *p++ = a;
      *p++ = CLAMP ((int) (((19595 * r) >> 16) + ((38470 * g) >> 16) +
              ((7471 * b) >> 16)), 0, 255);
      *p++ = CLAMP ((int) (-((11059 * r) >> 16) - ((21709 * g) >> 16) +
              ((32768 * b) >> 16) + 128), 0, 255);
      *p++ = CLAMP ((int) (((32768 * r) >> 16) - ((27439 * g) >> 16) -
              ((5329 * b) >> 16) + 128), 0, 255);
    }
  }
}

static void
gst_cea_cc_overlay_create_and_push_buffer (GstCeaCcOverlay * overlay)
{
  Cea708Dec *decoder = overlay->decoder;
  GstBuffer *outbuf;
  GstMapInfo map;
  guint8 *window_image;
  gint n;
  guint window_id;
  cea708Window *window;
  guint v_anchor = 0;
  guint h_anchor = 0;
  GstVideoOverlayComposition *comp = NULL;
  GstVideoOverlayRectangle *rect = NULL;
  GST_CEA_CC_OVERLAY_LOCK (overlay);

  for (window_id = 0; window_id < 8; window_id++) {
    window = decoder->cc_windows[window_id];

    if (!window->updated) {
      continue;
    }
    if (!window->deleted && window->visible && window->text_image != NULL) {
      GST_DEBUG_OBJECT (overlay, "Allocating buffer");
      outbuf =
          gst_buffer_new_and_alloc (window->image_width *
          window->image_height * 4);
      gst_buffer_map (outbuf, &map, GST_MAP_WRITE);
      window_image = map.data;
      if (decoder->use_ARGB) {
        memset (window_image, 0,
            window->image_width * window->image_height * 4);
        gst_buffer_add_video_meta (outbuf, GST_VIDEO_FRAME_FLAG_NONE,
            GST_VIDEO_OVERLAY_COMPOSITION_FORMAT_RGB, window->image_width,
            window->image_height);
      } else {
        for (n = 0; n < window->image_width * window->image_height; n++) {
          window_image[n * 4] = window_image[n * 4 + 1] = 0;
          window_image[n * 4 + 2] = window_image[n * 4 + 3] = 128;
        }
        gst_buffer_add_video_meta (outbuf, GST_VIDEO_FRAME_FLAG_NONE,
            GST_VIDEO_OVERLAY_COMPOSITION_FORMAT_YUV, window->image_width,
            window->image_height);
      }

      v_anchor = window->screen_vertical * overlay->height / 100;
      switch (overlay->default_window_h_pos) {
        case GST_CEA_CC_OVERLAY_WIN_H_LEFT:
          window->h_offset = 0;
          break;
        case GST_CEA_CC_OVERLAY_WIN_H_CENTER:
          window->h_offset = (overlay->width - window->image_width) / 2;
          break;
        case GST_CEA_CC_OVERLAY_WIN_H_RIGHT:
          window->h_offset = overlay->width - window->image_width;
          break;
        case GST_CEA_CC_OVERLAY_WIN_H_AUTO:
        default:
          switch (window->anchor_point) {
            case ANCHOR_PT_TOP_LEFT:
            case ANCHOR_PT_MIDDLE_LEFT:
            case ANCHOR_PT_BOTTOM_LEFT:
              window->h_offset = h_anchor;
              break;

            case ANCHOR_PT_TOP_CENTER:
            case ANCHOR_PT_CENTER:
            case ANCHOR_PT_BOTTOM_CENTER:
              window->h_offset = h_anchor - window->image_width / 2;
              break;

            case ANCHOR_PT_TOP_RIGHT:
            case ANCHOR_PT_MIDDLE_RIGHT:
            case ANCHOR_PT_BOTTOM_RIGHT:
              window->h_offset = h_anchor - window->image_width;
              break;
            default:
              break;
          }
          break;
      }

      switch (window->anchor_point) {
        case ANCHOR_PT_TOP_LEFT:
        case ANCHOR_PT_TOP_CENTER:
        case ANCHOR_PT_TOP_RIGHT:
          window->v_offset = v_anchor;
          break;

        case ANCHOR_PT_MIDDLE_LEFT:
        case ANCHOR_PT_CENTER:
        case ANCHOR_PT_MIDDLE_RIGHT:
          window->v_offset = v_anchor - window->image_height / 2;
          break;

        case ANCHOR_PT_BOTTOM_LEFT:
        case ANCHOR_PT_BOTTOM_CENTER:
        case ANCHOR_PT_BOTTOM_RIGHT:
          window->v_offset = v_anchor - window->image_height;
          break;
        default:
          break;
      }
      if (decoder->use_ARGB) {
        gst_cea_cc_overlay_image_to_argb (window_image, window,
            window->image_width * 4);
      } else {
        gst_cea_cc_overlay_image_to_ayuv (window_image, window,
            window->image_width * 4);
      }
      gst_buffer_unmap (outbuf, &map);
      GST_INFO_OBJECT (overlay,
          "window->anchor_point=%d,v_anchor=%d,h_anchor=%d,window->image_height=%d,window->image_width=%d, window->v_offset=%d, window->h_offset=%d,window->justify_mode=%d",
          window->anchor_point, v_anchor, h_anchor, window->image_height,
          window->image_width, window->v_offset, window->h_offset,
          window->justify_mode);
      rect =
          gst_video_overlay_rectangle_new_raw (outbuf, window->h_offset,
          window->v_offset, window->image_width, window->image_height, 0);
      if (comp == NULL) {
        comp = gst_video_overlay_composition_new (rect);
      } else {
        gst_video_overlay_composition_add_rectangle (comp, rect);
      }
      gst_video_overlay_rectangle_unref (rect);
      gst_buffer_unref (outbuf);
    }
  }

  /* Wait for the previous buffer to go away */
  if (GST_CLOCK_TIME_IS_VALID (overlay->current_comp_start_time)) {
    overlay->next_composition = comp;
    overlay->next_comp_start_time = decoder->current_time;
    GST_DEBUG_OBJECT (overlay,
        "wait for render next %p, current is %p BUFFER: next ts=%"
        GST_TIME_FORMAT ",current ts=%" GST_TIME_FORMAT,
        overlay->next_composition, overlay->current_composition,
        GST_TIME_ARGS (overlay->next_comp_start_time),
        GST_TIME_ARGS (overlay->current_comp_start_time));

    GST_DEBUG_OBJECT (overlay, "has a closed caption buffer queued, waiting");
    GST_CEA_CC_OVERLAY_WAIT (overlay);
    GST_DEBUG_OBJECT (overlay, "resuming");
    if (overlay->cc_flushing) {
      GST_CEA_CC_OVERLAY_UNLOCK (overlay);
      return;
    }
  }

  overlay->next_composition = NULL;
  overlay->next_comp_start_time = GST_CLOCK_TIME_NONE;
  overlay->current_composition = comp;
  overlay->current_comp_start_time = decoder->current_time;
  GST_DEBUG_OBJECT (overlay, "T: %" GST_TIME_FORMAT,
      GST_TIME_ARGS (overlay->current_comp_start_time));
  overlay->need_update = FALSE;

  /* in case the video chain is waiting for a text buffer, wake it up */
  GST_CEA_CC_OVERLAY_BROADCAST (overlay);
  GST_CEA_CC_OVERLAY_UNLOCK (overlay);
}

static void
gst_cea_cc_overlay_process_packet (GstCeaCcOverlay * overlay, guint8 cc_type)
{
  gint16 *index = NULL;
  guint8 *buffer = NULL;
  guint8 *dtvcc_buffer = NULL;
  gboolean need_render = FALSE;

  switch (cc_type) {
    case CCTYPE_608_CC1:
    case CCTYPE_608_CC2:
      index = &overlay->cea608_index[cc_type];
      buffer = overlay->cea608_buffer[cc_type];
      break;

    case CCTYPE_708_ADD:
    case CCTYPE_708_START:
      index = &overlay->cea708_index;
      buffer = overlay->cea708_buffer;
      break;
    default:
      GST_ERROR_OBJECT (overlay,
          "attempted to process packet for unknown cc_type %d", cc_type);
      return;
  }

  if (*index > 0) {
    /*TODO: in future need add 608 decoder, currently only deal with 708 */
    if (cc_type == CCTYPE_708_ADD || cc_type == CCTYPE_708_START) {
      GST_LOG_OBJECT (overlay,
          "called - buf[%" G_GINT16_FORMAT "] = %02X:%02X:%02X:%02X", *index,
          buffer[0], buffer[1], buffer[2], buffer[3]);
      dtvcc_buffer = g_malloc0 (*index + 1);
      memcpy (dtvcc_buffer, buffer, *index);
      need_render =
          gst_cea708dec_process_dtvcc_packet (overlay->decoder, dtvcc_buffer,
          *index);
      g_free (dtvcc_buffer);
      if (need_render)
        gst_cea_cc_overlay_create_and_push_buffer (overlay);
    }
  }
  *index = 0;
}


/**
 * gst_cea_cc_overlay_user_data_decode:
 * @overlay: The #GstCeaCcOverlay 
 * @user_data: The #GstMpegVideoCCData to decode
 *
 * decode closed caption data and render when necessary
 * in struct GstMpegVideoCCData type's user_data's data field, 3 byte's data construct 1 cc_data_pkt
 *
 * A cc_data_pkt is 3 bytes as follows:
 * -------------------------------------------
 *   5 bits (b7-b3)  marker_bits (should be all 1's)
 *   1 bit (b2)      cc_valid
 *   2 bits (b1-b0)  cc_type (bslbf)
 *   8 bits          cc_data_1 (bslbf)
 *   8 bits          cc_data_2 (bslbf)
 *
 *   If cc_valid != 1, then ignore this packet
 *
 *   cc_type has these values:
 *   0   NTSC_CC_FIELD_1     - CEA-608
 *   1   NTSC_CC_FIELD_2     - CEA-608
 *   2   DTVCC_PACKET_DATA   - CEA-708
 *   3   DTVCC_PACKET_START  - CEA-708
 *
 *   DTVCC packet (aka. caption channel packet)
 *   This is formed by accumulating cc_data_1/cc_data_2 from each cc_data_pkt
 *   starting with a packet where cc_type = 3, and ending with a packet
 *   where again cc_type = 3 (start of next buffer), or cc_valid=0 && cc_type=2
 *   DTVCC packet's structure is:
 *   --------------------------------------------------------------------------
 *   2 bits (b6-b7)  sequence_number
 *   6 bits (b0-b5)  packet_size
 *   ((packet_size*2-1)&0xFF) * 8 bits packet_data (Service Block)
 */
static void
gst_cea_cc_overlay_user_data_decode (GstCeaCcOverlay * overlay,
    const guint8 * ccdata, gsize ccsize)
{
  guint8 temp;
  guint8 cc_count;
  guint i;
  guint8 cc_type;
  guint8 cc_valid;
  guint8 cc_data[2];

  cc_count = ccsize / 3;

  for (i = 0; i < cc_count; i++) {
    temp = *ccdata++;
    cc_data[0] = *ccdata++;
    cc_data[1] = *ccdata++;
    cc_valid = (temp & CCTYPE_VALID_MASK) ? TRUE : FALSE;
    cc_type = (temp & CCTYPE_TYPE_MASK);

    GST_LOG_OBJECT (overlay, "cc_data_pkt(%d): cc_valid=%d cc_type=%d "
        "cc_data[0]=0x%02X cc_data[1]=0x%02X",
        i, cc_valid, cc_type, cc_data[0], cc_data[1]);

    /* accumulate dvtcc packet */
    switch (cc_type) {
      case CCTYPE_608_CC1:
      case CCTYPE_608_CC2:
        if (cc_valid) {
          if (overlay->cea608_index[cc_type] <= DTVCC_LENGTH - 2) {
            size_t j;
            for (j = 0; j < 2; ++j) {
              if ((cc_data[j] < ' ') || (cc_data[j] > '~')) {
                gst_cea_cc_overlay_process_packet (overlay, cc_type);
              }
              overlay->cea608_buffer[cc_type][overlay->
                  cea608_index[cc_type]++] = cc_data[j];
            }
          } else {
            GST_ERROR_OBJECT (overlay, "cea608_buffer[%d] overflow!", cc_type);
          }
        }
        break;

      case CCTYPE_708_ADD:
      case CCTYPE_708_START:
        if (cc_valid) {
          if (cc_type == CCTYPE_708_START) {
            /* The previous packet is complete */
            gst_cea_cc_overlay_process_packet (overlay, cc_type);
          }
          /* Add on to the current DTVCC packet */
          if (overlay->cea708_index <= DTVCC_LENGTH - 2) {
            overlay->cea708_buffer[overlay->cea708_index++] = cc_data[0];
            overlay->cea708_buffer[overlay->cea708_index++] = cc_data[1];
          } else {
            GST_ERROR_OBJECT (overlay, "cea708_buffer overflow!");
          }
        } else if (cc_type == CCTYPE_708_ADD) {
          /* This packet should be ignored, but if there is a current */
          /* DTVCC packet then this is the end. */
          gst_cea_cc_overlay_process_packet (overlay, cc_type);
        }
        break;
    }
  }
}

/* FIXME : Move to GstVideo ANC/CC helper library */
static gboolean
extract_ccdata_from_cdp (const guint8 * indata, gsize insize,
    const guint8 ** ccdata, gsize * ccsize)
{
  GstByteReader br;
  guint8 cdp_length;
  guint8 flags;
#ifndef GST_DISABLE_GST_DEBUG
  guint8 framerate_code;
  guint16 seqhdr;
#endif

  GST_MEMDUMP ("CDP", indata, insize);

  gst_byte_reader_init (&br, indata, insize);

  /* The smallest valid CDP we are interested in is 7 (header) + 2 (cc
   * section) + 4 (footer) bytes long */
  if (gst_byte_reader_get_remaining (&br) < 13)
    return FALSE;

  /* Check header */
  if (gst_byte_reader_get_uint16_be_unchecked (&br) != 0x9669) {
    GST_WARNING ("Invalid CDP header");
    return FALSE;
  }
  cdp_length = gst_byte_reader_get_uint8_unchecked (&br);
  if (cdp_length > insize) {
    GST_WARNING ("CDP too small (need %d bytes, have %" G_GSIZE_FORMAT ")",
        cdp_length, insize);
    return FALSE;
  }
#ifndef GST_DISABLE_GST_DEBUG
  framerate_code = gst_byte_reader_get_uint8_unchecked (&br) >> 4;
#else
  gst_byte_reader_skip (&br, 1);
#endif
  flags = gst_byte_reader_get_uint8_unchecked (&br);
#ifndef GST_DISABLE_GST_DEBUG
  seqhdr = gst_byte_reader_get_uint16_be_unchecked (&br);
#else
  gst_byte_reader_skip (&br, 2);
#endif

  GST_DEBUG
      ("framerate_code : 0x%02x , flags : 0x%02x , sequencer_counter : %u",
      framerate_code, flags, seqhdr);

  /* Skip timecode if present */
  if (flags & 0x80) {
    GST_LOG ("Skipping timecode section");
    gst_byte_reader_skip (&br, 5);
  }

  /* cc data */
  if (flags & 0x40) {
    guint8 ccid, cc_count;
    if (!gst_byte_reader_get_uint8 (&br, &ccid) ||
        !gst_byte_reader_get_uint8 (&br, &cc_count))
      return FALSE;
    if (ccid != 0x72) {
      GST_WARNING ("Invalid ccdata_id (expected 0x72, got 0x%02x)", ccid);
      return FALSE;
    }
    cc_count &= 0x1f;
    if (!gst_byte_reader_get_data (&br, cc_count * 3, ccdata)) {
      GST_WARNING ("Not enough ccdata");
      *ccdata = NULL;
      *ccsize = 0;
      return FALSE;
    }
    *ccsize = cc_count * 3;
  }

  /* FIXME : Parse/validate the rest of the CDP ! */

  return TRUE;
}

/* We receive text buffers here. If they are out of segment we just ignore them.
   If the buffer is in our segment we keep it internally except if another one
   is already waiting here, in that case we wait that it gets kicked out */
static GstFlowReturn
gst_cea_cc_overlay_cc_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstCeaCcOverlay *overlay = (GstCeaCcOverlay *) parent;
  gboolean in_seg = FALSE;
  guint64 clip_start = 0, clip_stop = 0;

  GST_CEA_CC_OVERLAY_LOCK (overlay);

  if (overlay->cc_flushing) {
    GST_CEA_CC_OVERLAY_UNLOCK (overlay);
    ret = GST_FLOW_FLUSHING;
    GST_LOG_OBJECT (overlay, "closed caption flushing");
    goto beach;
  }

  if (overlay->cc_eos) {
    GST_CEA_CC_OVERLAY_UNLOCK (overlay);
    ret = GST_FLOW_EOS;
    GST_LOG_OBJECT (overlay, "closed caption EOS");
    goto beach;
  }

  GST_LOG_OBJECT (overlay, "%" GST_SEGMENT_FORMAT "  BUFFER: ts=%"
      GST_TIME_FORMAT ", end=%" GST_TIME_FORMAT, &overlay->segment,
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)),
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer) +
          GST_BUFFER_DURATION (buffer)));

  if (G_LIKELY (GST_BUFFER_TIMESTAMP_IS_VALID (buffer))) {
    GstClockTime stop;

    if (G_LIKELY (GST_BUFFER_DURATION_IS_VALID (buffer)))
      stop = GST_BUFFER_TIMESTAMP (buffer) + GST_BUFFER_DURATION (buffer);
    else
      stop = GST_CLOCK_TIME_NONE;

    in_seg = gst_segment_clip (&overlay->cc_segment, GST_FORMAT_TIME,
        GST_BUFFER_TIMESTAMP (buffer), stop, &clip_start, &clip_stop);
    GST_LOG_OBJECT (overlay, "stop:%" GST_TIME_FORMAT ", in_seg: %d",
        GST_TIME_ARGS (stop), in_seg);
  } else {
    in_seg = TRUE;
  }


  if (in_seg) {
    GstMapInfo buf_map = { 0 };
    const guint8 *ccdata = NULL;
    gsize ccsize = 0;

    overlay->cc_segment.position = clip_start;
    GST_CEA_CC_OVERLAY_UNLOCK (overlay);

    gst_buffer_map (buffer, &buf_map, GST_MAP_READ);
    if (overlay->is_cdp) {
      extract_ccdata_from_cdp (buf_map.data, buf_map.size, &ccdata, &ccsize);
    } else {
      ccdata = buf_map.data;
      ccsize = buf_map.size;
    }
    if (ccsize) {
      gst_cea_cc_overlay_user_data_decode (overlay, ccdata, ccsize);
      overlay->decoder->current_time = GST_BUFFER_PTS (buffer);
    }
    gst_buffer_unmap (buffer, &buf_map);
  } else {
    GST_CEA_CC_OVERLAY_UNLOCK (overlay);
  }

beach:
  gst_buffer_unref (buffer);
  return ret;
}

static GstFlowReturn
gst_cea_cc_overlay_video_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer)
{
  GstCeaCcOverlay *overlay;
  GstFlowReturn ret = GST_FLOW_OK;
  gboolean in_seg = FALSE;
  guint64 start, stop, clip_start = 0, clip_stop = 0;

  overlay = GST_CEA_CC_OVERLAY (parent);

  if (!GST_BUFFER_TIMESTAMP_IS_VALID (buffer))
    goto missing_timestamp;

  /* ignore buffers that are outside of the current segment */
  start = GST_BUFFER_TIMESTAMP (buffer);

  if (!GST_BUFFER_DURATION_IS_VALID (buffer)) {
    stop = GST_CLOCK_TIME_NONE;
  } else {
    stop = start + GST_BUFFER_DURATION (buffer);
  }

  GST_LOG_OBJECT (overlay, "%" GST_SEGMENT_FORMAT "  BUFFER: ts=%"
      GST_TIME_FORMAT ", end=%" GST_TIME_FORMAT, &overlay->segment,
      GST_TIME_ARGS (start), GST_TIME_ARGS (stop));

  /* segment_clip() will adjust start unconditionally to segment_start if
   * no stop time is provided, so handle this ourselves */
  if (stop == GST_CLOCK_TIME_NONE && start < overlay->segment.start)
    goto out_of_segment;

  in_seg = gst_segment_clip (&overlay->segment, GST_FORMAT_TIME, start, stop,
      &clip_start, &clip_stop);

  if (!in_seg)
    goto out_of_segment;

  /* if the buffer is only partially in the segment, fix up stamps */
  if (clip_start != start || (stop != -1 && clip_stop != stop)) {
    GST_DEBUG_OBJECT (overlay, "clipping buffer timestamp/duration to segment");
    buffer = gst_buffer_make_writable (buffer);
    GST_BUFFER_TIMESTAMP (buffer) = clip_start;
    if (stop != -1)
      GST_BUFFER_DURATION (buffer) = clip_stop - clip_start;
  }

  /* now, after we've done the clipping, fix up end time if there's no
   * duration (we only use those estimated values internally though, we
   * don't want to set bogus values on the buffer itself) */
  if (stop == -1) {
    if (overlay->info.fps_n && overlay->info.fps_d) {
      GST_DEBUG_OBJECT (overlay, "estimating duration based on framerate");
      stop = start + gst_util_uint64_scale_int (GST_SECOND,
          overlay->info.fps_d, overlay->info.fps_n);
    } else {
      GST_LOG_OBJECT (overlay, "no duration, assuming minimal duration");
      stop = start + 1;         /* we need to assume some interval */
    }
  }

  gst_object_sync_values (GST_OBJECT (overlay), GST_BUFFER_TIMESTAMP (buffer));

wait_for_text_buf:

  GST_CEA_CC_OVERLAY_LOCK (overlay);

  if (overlay->video_flushing)
    goto flushing;

  if (overlay->video_eos)
    goto have_eos;

  if (overlay->silent) {
    GST_CEA_CC_OVERLAY_UNLOCK (overlay);
    ret = gst_pad_push (overlay->srcpad, buffer);

    /* Update position */
    overlay->segment.position = clip_start;

    return ret;
  }

  /* Closed Caption pad not linked, rendering video only */
  if (!overlay->cc_pad_linked) {
    GST_CEA_CC_OVERLAY_UNLOCK (overlay);
    ret = gst_pad_push (overlay->srcpad, buffer);
  } else {
    /* Closed Caption pad linked, check if we have a text buffer queued */
    if (GST_CLOCK_TIME_IS_VALID (overlay->current_comp_start_time)) {
      gboolean pop_text = FALSE, valid_text_time = TRUE;

      GstClockTime text_running_time = GST_CLOCK_TIME_NONE;
      GstClockTime next_buffer_text_running_time = GST_CLOCK_TIME_NONE;
#ifndef GST_DISABLE_GST_DEBUG
      GstClockTime vid_running_time;
#endif
      GstClockTime vid_running_time_end;

#ifndef GST_DISABLE_GST_DEBUG
      vid_running_time =
          gst_segment_to_running_time (&overlay->segment, GST_FORMAT_TIME,
          start);
#endif
      vid_running_time_end =
          gst_segment_to_running_time (&overlay->segment, GST_FORMAT_TIME,
          stop);
      if (GST_CLOCK_TIME_IS_VALID (overlay->next_comp_start_time)) {
        next_buffer_text_running_time =
            gst_segment_to_running_time (&overlay->cc_segment, GST_FORMAT_TIME,
            overlay->next_comp_start_time);

        if (next_buffer_text_running_time < vid_running_time_end) {
          /* text buffer should be force updated, popping  */
          GST_DEBUG_OBJECT (overlay,
              "T: next_buffer_text_running_time: %" GST_TIME_FORMAT
              " - overlay->next_comp_start_time: %" GST_TIME_FORMAT,
              GST_TIME_ARGS (next_buffer_text_running_time),
              GST_TIME_ARGS (overlay->next_comp_start_time));
          GST_DEBUG_OBJECT (overlay,
              "V: %" GST_TIME_FORMAT " - %" GST_TIME_FORMAT,
              GST_TIME_ARGS (vid_running_time),
              GST_TIME_ARGS (vid_running_time_end));
          GST_LOG_OBJECT (overlay,
              "text buffer should be force updated, popping");
          pop_text = FALSE;
          gst_cea_cc_overlay_pop_text (overlay);
          GST_CEA_CC_OVERLAY_WAIT (overlay);
          GST_DEBUG_OBJECT (overlay, "resuming");
          GST_CEA_CC_OVERLAY_UNLOCK (overlay);
          goto wait_for_text_buf;
        }

      }

      /* if the text buffer isn't stamped right, pop it off the
       * queue and display it for the current video frame only */
      if (!GST_CLOCK_TIME_IS_VALID (overlay->current_comp_start_time)) {
        GST_WARNING_OBJECT (overlay, "Got text buffer with invalid timestamp");
        pop_text = TRUE;
        valid_text_time = FALSE;
      }

      /* If timestamp and duration are valid */
      if (valid_text_time) {
        text_running_time =
            gst_segment_to_running_time (&overlay->cc_segment,
            GST_FORMAT_TIME, overlay->current_comp_start_time);
      }

      GST_DEBUG_OBJECT (overlay, "T: %" GST_TIME_FORMAT,
          GST_TIME_ARGS (text_running_time));
      GST_DEBUG_OBJECT (overlay, "V: %" GST_TIME_FORMAT " - %" GST_TIME_FORMAT,
          GST_TIME_ARGS (vid_running_time),
          GST_TIME_ARGS (vid_running_time_end));

      if (valid_text_time && vid_running_time_end <= text_running_time) {
        GST_LOG_OBJECT (overlay, "text in future, pushing video buf");
        GST_CEA_CC_OVERLAY_UNLOCK (overlay);
        /* Push the video frame */
        ret = gst_pad_push (overlay->srcpad, buffer);
      } else {
        GST_CEA_CC_OVERLAY_UNLOCK (overlay);
        ret = gst_cea_cc_overlay_push_frame (overlay, buffer);
      }
      if (pop_text) {
        GST_CEA_CC_OVERLAY_LOCK (overlay);
        gst_cea_cc_overlay_pop_text (overlay);
        GST_CEA_CC_OVERLAY_UNLOCK (overlay);
      }
    } else {
      GST_CEA_CC_OVERLAY_UNLOCK (overlay);
      GST_LOG_OBJECT (overlay, "no need to wait for a text buffer");
      ret = gst_pad_push (overlay->srcpad, buffer);
    }
  }

  /* Update position */
  overlay->segment.position = clip_start;
  GST_DEBUG_OBJECT (overlay, "ret=%d", ret);

  return ret;

missing_timestamp:
  {
    GST_WARNING_OBJECT (overlay, "buffer without timestamp, discarding");
    gst_buffer_unref (buffer);
    return GST_FLOW_OK;
  }

flushing:
  {
    GST_CEA_CC_OVERLAY_UNLOCK (overlay);
    GST_DEBUG_OBJECT (overlay, "flushing, discarding buffer");
    gst_buffer_unref (buffer);
    return GST_FLOW_FLUSHING;
  }
have_eos:
  {
    GST_CEA_CC_OVERLAY_UNLOCK (overlay);
    GST_DEBUG_OBJECT (overlay, "eos, discarding buffer");
    gst_buffer_unref (buffer);
    return GST_FLOW_EOS;
  }
out_of_segment:
  {
    GST_DEBUG_OBJECT (overlay, "buffer out of segment, discarding");
    gst_buffer_unref (buffer);
    return GST_FLOW_OK;
  }
}

static GstStateChangeReturn
gst_cea_cc_overlay_change_state (GstElement * element,
    GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstCeaCcOverlay *overlay = GST_CEA_CC_OVERLAY (element);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_CEA_CC_OVERLAY_LOCK (overlay);
      overlay->cc_flushing = TRUE;
      overlay->video_flushing = TRUE;
      /* pop_text will broadcast on the GCond and thus also make the video
       * chain exit if it's waiting for a text buffer */
      gst_cea_cc_overlay_pop_text (overlay);
      GST_CEA_CC_OVERLAY_UNLOCK (overlay);
      break;
    default:
      break;
  }

  ret = parent_class->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      GST_CEA_CC_OVERLAY_LOCK (overlay);
      overlay->cc_flushing = FALSE;
      overlay->video_flushing = FALSE;
      overlay->video_eos = FALSE;
      overlay->cc_eos = FALSE;
      gst_segment_init (&overlay->segment, GST_FORMAT_TIME);
      gst_segment_init (&overlay->cc_segment, GST_FORMAT_TIME);
      GST_CEA_CC_OVERLAY_UNLOCK (overlay);
      break;
    default:
      break;
  }

  return ret;
}
