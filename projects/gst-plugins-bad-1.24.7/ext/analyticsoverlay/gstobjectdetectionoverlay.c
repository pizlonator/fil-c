/* GStreamer object detection overlay
 * Copyright (C) <2023> Collabora Ltd.
 *  @author: Aaron Boxer <aaron.boxer@collabora.com>
 *  @author: Daniel Morin <daniel.morin@collabora.com>
 *
 * gstobjectdetectionoverlay.c
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
 * SECTION:element-objectdetectionoverlay
 * @title: objectdetectionoverlay
 * @see_also: #GstObjectDetectionOverlay
 *
 * This element create a graphical representation of the analytics object
 * detection metadata attached to video stream and overlay graphics above the
 * video.
 *
 * The object detection overlay element monitor video stream for
 * @GstAnalyticsRelationMeta and query @GstAnalyticsODMtd. Retrieved
 * @GstAnalyticsODMtd are then used to generate an overlay highlighing objects
 * detected.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 multifilesrc location=/onnx-models/images/bus.jpg ! jpegdec ! videoconvert ! onnxinference execution-provider=cpu model-file=/onnx-models/models/ssd_mobilenet_v1_coco.onnx ! ssdobjectdetector label-file=/onnx-models/labels/COCO_classes.txt ! objectdetectionoverlay object-detection-outline-color=0xFF0000FF draw-labels=true ! videoconvertscale ! imagefreeze ! autovideosink
 * ]| This pipeline create an overlay representing results of an object detetion
 * analysis.
 *
 * Since: 1.24
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/analytics/analytics.h>
#include <pango/pangocairo.h>

#include "gstobjectdetectionoverlay.h"

struct _GstObjectDetectionOverlay
{
  GstVideoFilter parent;

  cairo_matrix_t cairo_matrix;
  gsize render_len;

  /* stream metrics */
  GstVideoInfo *in_info;
  GMutex stream_event_mutex;
  gboolean flushing;
  gboolean eos;

  /* properties */
  guint od_outline_color;
  guint od_outline_stroke_width;
  gboolean draw_labels;
  guint labels_color;
  gdouble labels_stroke_width;
  gdouble labels_outline_ofs;

  /* composition */
  gboolean attach_compo_to_buffer;
  GstBuffer *canvas;
  gint canvas_length;
  GstVideoOverlayComposition *composition;
  GstVideoOverlayComposition *upstream_composition;

  /* Graphic Outline */
  PangoContext *pango_context;
  PangoLayout *pango_layout;

};


#define MINIMUM_TEXT_OUTLINE_OFFSET 1.0

GST_DEBUG_CATEGORY_STATIC (objectdetectionoverlay_debug);
#define GST_CAT_DEFAULT objectdetectionoverlay_debug

enum
{
  PROP_OD_OUTLINE_COLOR = 1,
  PROP_DRAW_LABELS,
  PROP_LABELS_COLOR,
  _PROP_COUNT
};

typedef struct _GstObjectDetectionOverlayPangoCairoContext
    GstObjectDetectionOverlayPangoCairoContext;

struct _GstObjectDetectionOverlayPangoCairoContext
{
  cairo_t *cr;
  cairo_surface_t *surface;
  guint8 *data;
  cairo_matrix_t *cairo_matrix;
};

#define VIDEO_FORMATS GST_VIDEO_OVERLAY_COMPOSITION_BLEND_FORMATS
#define OBJECT_DETECTION_OVERLAY_CAPS GST_VIDEO_CAPS_MAKE (VIDEO_FORMATS)

static GstStaticCaps sw_template_caps =
GST_STATIC_CAPS (OBJECT_DETECTION_OVERLAY_CAPS);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (OBJECT_DETECTION_OVERLAY_CAPS)
    );

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (OBJECT_DETECTION_OVERLAY_CAPS)
    );

G_DEFINE_TYPE (GstObjectDetectionOverlay,
    gst_object_detection_overlay, GST_TYPE_VIDEO_FILTER);

#define parent_class gst_object_detection_overlay_parent_class

GST_ELEMENT_REGISTER_DEFINE (objectdetectionoverlay, "objectdetectionoverlay",
    GST_RANK_NONE, GST_TYPE_OBJECT_DETECTION_OVERLAY);

static void gst_object_detection_overlay_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);

static void gst_object_detection_overlay_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static gboolean gst_object_detection_overlay_sink_event (GstBaseTransform *
    trans, GstEvent * event);

static gboolean gst_object_detection_overlay_start (GstBaseTransform * trans);
static gboolean gst_object_detection_overlay_stop (GstBaseTransform * trans);


static gboolean gst_object_detection_overlay_set_info (GstVideoFilter * filter,
    GstCaps * incaps, GstVideoInfo * in_info, GstCaps * outcaps,
    GstVideoInfo * out_info);

static GstFlowReturn
gst_object_detection_overlay_transform_frame_ip (GstVideoFilter * filter,
    GstVideoFrame * buf);

static void gst_object_detection_overlay_finalize (GObject * object);

static void
gst_object_detection_overlay_render_boundingbox (GstObjectDetectionOverlay
    * overlay, GstObjectDetectionOverlayPangoCairoContext * cairo_ctx,
    GstAnalyticsODMtd * od_mtd);

static void
gst_object_detection_overlay_render_text_annotation (GstObjectDetectionOverlay
    * overlay, GstObjectDetectionOverlayPangoCairoContext * cairo_ctx,
    GstAnalyticsODMtd * od_mtd, const gchar * annotation);

static void
gst_object_detection_overlay_class_init (GstObjectDetectionOverlayClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstBaseTransformClass *basetransform_class;
  GstVideoFilterClass *videofilter_class;

  gobject_class = (GObjectClass *) klass;
  gobject_class->set_property = gst_object_detection_overlay_set_property;
  gobject_class->get_property = gst_object_detection_overlay_get_property;
  gobject_class->finalize = gst_object_detection_overlay_finalize;

  /**
   * GstObjectDetectionOverlay:object-detection-outline-color
   *
   * Object Detetion Overlay outline color
   * ARGB format (ex. 0xFFFF0000 for red)
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_OD_OUTLINE_COLOR,
      g_param_spec_uint ("object-detection-outline-color",
          "Object detection outline color",
          "Color (ARGB) to use for object detection overlay outline",
          0, G_MAXUINT, 0xFFFFFFFF,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstObjectDetectionOverlay:draw-labels
   *
   * Control labels drawing
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_DRAW_LABELS,
      g_param_spec_boolean ("draw-labels",
          "Draw labels",
          "Draw object labels",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstObjectDetectionOverlay:labels-color
   *
   * Control labels color
   * Format ARGB (ex. 0xFFFF0000 for red)
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_LABELS_COLOR,
      g_param_spec_uint ("labels-color",
          "Labels color",
          "Color (ARGB) to use for object labels",
          0, G_MAXUINT, 0xFFFFFF, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  element_class = (GstElementClass *) klass;

  gst_element_class_add_static_pad_template (element_class, &sink_template);
  gst_element_class_add_static_pad_template (element_class, &src_template);
  gst_element_class_set_static_metadata (element_class,
      "Object Detection Overlay",
      "Analyzer/Visualization/Video",
      "Overlay a visual representation of analytics metadata on the video",
      "Daniel Morin");

  basetransform_class = (GstBaseTransformClass *) klass;
  basetransform_class->passthrough_on_same_caps = FALSE;
  basetransform_class->start =
      GST_DEBUG_FUNCPTR (gst_object_detection_overlay_start);

  basetransform_class->stop =
      GST_DEBUG_FUNCPTR (gst_object_detection_overlay_stop);

  basetransform_class->sink_event =
      GST_DEBUG_FUNCPTR (gst_object_detection_overlay_sink_event);

  videofilter_class = (GstVideoFilterClass *) klass;
  videofilter_class->set_info =
      GST_DEBUG_FUNCPTR (gst_object_detection_overlay_set_info);
  videofilter_class->transform_frame_ip =
      GST_DEBUG_FUNCPTR (gst_object_detection_overlay_transform_frame_ip);
}

static void
gst_object_detection_overlay_adj_labels_outline_ofs (GstObjectDetectionOverlay *
    overlay, PangoFontDescription * desc)
{
  gint font_size = pango_font_description_get_size (desc) / PANGO_SCALE;
  overlay->labels_outline_ofs = (double) (font_size) / 15.0;
  if (overlay->labels_outline_ofs < MINIMUM_TEXT_OUTLINE_OFFSET)
    overlay->labels_outline_ofs = MINIMUM_TEXT_OUTLINE_OFFSET;
}

static void
gst_object_detection_overlay_finalize (GObject * object)
{
  gst_object_detection_overlay_stop (GST_BASE_TRANSFORM (object));

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_object_detection_overlay_init (GstObjectDetectionOverlay * overlay)
{
  overlay->pango_context = NULL;
  overlay->pango_layout = NULL;
  overlay->od_outline_color = 0xFFFFFFFF;
  overlay->draw_labels = TRUE;
  overlay->labels_color = 0xFFFFFFFF;
  overlay->in_info = &GST_VIDEO_FILTER (overlay)->in_info;
  overlay->attach_compo_to_buffer = TRUE;
  overlay->canvas = NULL;
  overlay->labels_stroke_width = 1.0;
  overlay->od_outline_stroke_width = 2;
  overlay->composition = NULL;
  overlay->upstream_composition = NULL;
  overlay->flushing = FALSE;
  GST_DEBUG_CATEGORY_INIT (objectdetectionoverlay_debug,
      "analytics_overlay_od", 0, "Object detection overlay");
}

static void
gst_object_detection_overlay_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstObjectDetectionOverlay *overlay;
  overlay = GST_OBJECT_DETECTION_OVERLAY (object);

  switch (prop_id) {
    case PROP_OD_OUTLINE_COLOR:
      overlay->od_outline_color = g_value_get_uint (value);
      break;
    case PROP_DRAW_LABELS:
      overlay->draw_labels = g_value_get_boolean (value);
      break;
    case PROP_LABELS_COLOR:
      overlay->labels_color = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_object_detection_overlay_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstObjectDetectionOverlay *od_overlay = GST_OBJECT_DETECTION_OVERLAY (object);

  switch (prop_id) {
    case PROP_OD_OUTLINE_COLOR:
      g_value_set_uint (value, od_overlay->od_outline_color);
      break;
    case PROP_DRAW_LABELS:
      g_value_set_boolean (value, od_overlay->draw_labels);
      break;
    case PROP_LABELS_COLOR:
      g_value_set_uint (value, od_overlay->labels_color);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_object_detection_overlay_can_handle_caps (GstCaps * incaps)
{
  gboolean ret;
  GstCaps *caps;

  caps = gst_static_caps_get (&sw_template_caps);
  ret = gst_caps_is_subset (incaps, caps);
  gst_caps_unref (caps);

  return ret;
}

static gboolean
gst_object_detection_overlay_negotiate (GstObjectDetectionOverlay * overlay,
    GstCaps * caps)
{
  GstBaseTransform *basetransform = GST_BASE_TRANSFORM (overlay);
  gboolean upstream_has_meta = FALSE;
  gboolean caps_has_meta = FALSE;
  gboolean alloc_has_meta = FALSE;
  gboolean attach = FALSE;
  gboolean ret = TRUE;
  guint width, height;
  GstCapsFeatures *f;
  GstCaps *overlay_caps;
  GstQuery *query;
  guint alloc_index;
  GstPad *srcpad = basetransform->srcpad;
  GstPad *sinkpad = basetransform->sinkpad;

  GST_DEBUG_OBJECT (overlay, "performing negotiation");

  /* Clear any pending reconfigure to avoid negotiating twice */
  gst_pad_check_reconfigure (sinkpad);

  /* Check if upstream caps have meta */
  if ((f = gst_caps_get_features (caps, 0))) {
    GST_DEBUG_OBJECT (overlay, "upstream has caps");
    upstream_has_meta = gst_caps_features_contains (f,
        GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION);
  }

  /* Initialize dimensions */
  width = GST_VIDEO_INFO_WIDTH (overlay->in_info);
  height = GST_VIDEO_INFO_HEIGHT (overlay->in_info);
  GST_DEBUG_OBJECT (overlay, "initial dims: %ux%u", width, height);

  if (upstream_has_meta) {
    overlay_caps = gst_caps_ref (caps);
  } else {
    GstCaps *peercaps;

    /* BaseTransform requires caps for the allocation query to work */
    overlay_caps = gst_caps_copy (caps);
    f = gst_caps_get_features (overlay_caps, 0);
    gst_caps_features_add (f,
        GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION);

    /* Then check if downstream accept overlay composition in caps */
    /* FIXME: We should probably check if downstream *prefers* the
     * overlay meta, and only enforce usage of it if we can't handle
     * the format ourselves and thus would have to drop the overlays.
     * Otherwise we should prefer what downstream wants here.
     */
    peercaps = gst_pad_peer_query_caps (srcpad, overlay_caps);
    caps_has_meta = !gst_caps_is_empty (peercaps);
    gst_caps_unref (peercaps);

    GST_DEBUG_OBJECT (overlay, "caps have overlay meta %d", caps_has_meta);
  }

  if (upstream_has_meta || caps_has_meta) {
    /* Send caps immediately, it's needed by GstBaseTransform to get a reply
     * from allocation query */
    GST_BASE_TRANSFORM_CLASS (parent_class)->set_caps (basetransform, caps,
        overlay_caps);
    ret = gst_pad_set_caps (srcpad, overlay_caps);

    /* First check if the allocation meta has compositon */
    query = gst_query_new_allocation (overlay_caps, FALSE);

    if (!gst_pad_peer_query (srcpad, query)) {
      /* no problem, we use the query defaults */
      GST_DEBUG_OBJECT (overlay, "ALLOCATION query failed");

      /* In case we were flushing, mark reconfigure and fail this method,
       * will make it retry */
      if (overlay->flushing)
        ret = FALSE;
    }

    alloc_has_meta = gst_query_find_allocation_meta (query,
        GST_VIDEO_OVERLAY_COMPOSITION_META_API_TYPE, &alloc_index);

    GST_DEBUG_OBJECT (overlay, "sink alloc has overlay meta %d",
        alloc_has_meta);

    if (alloc_has_meta) {
      const GstStructure *params;

      gst_query_parse_nth_allocation_meta (query, alloc_index, &params);
      if (params) {
        if (gst_structure_get (params, "width", G_TYPE_UINT, &width,
                "height", G_TYPE_UINT, &height, NULL)) {
          GST_DEBUG_OBJECT (overlay, "received window size: %dx%d", width,
              height);
          g_assert (width != 0 && height != 0);
        }
      }
    }

    gst_query_unref (query);
  }

  /* Update render size if needed */
  overlay->canvas_length = width * height;

  /* For backward compatibility, we will prefer blitting if downstream
   * allocation does not support the meta. In other case we will prefer
   * attaching, and will fail the negotiation in the unlikely case we are
   * force to blit, but format isn't supported. */

  if (upstream_has_meta) {
    attach = TRUE;
  } else if (caps_has_meta) {
    if (alloc_has_meta) {
      attach = TRUE;
    } else {
      /* Don't attach unless we cannot handle the format */
      attach = !gst_object_detection_overlay_can_handle_caps (caps);
    }
  } else {
    ret = gst_object_detection_overlay_can_handle_caps (caps);
  }

  /* If we attach, then pick the overlay caps */
  if (attach) {
    GST_DEBUG_OBJECT (overlay, "Using caps %" GST_PTR_FORMAT, overlay_caps);
    /* Caps where already sent */
  } else if (ret) {
    GST_DEBUG_OBJECT (overlay, "Using caps %" GST_PTR_FORMAT, caps);
    GST_BASE_TRANSFORM_CLASS (parent_class)->set_caps (basetransform, caps,
        caps);
    ret = gst_pad_set_caps (srcpad, caps);
  }

  overlay->attach_compo_to_buffer = attach;

  if (attach) {
    GST_BASE_TRANSFORM_CLASS (parent_class)->passthrough_on_same_caps = FALSE;
  }

  if (!ret) {
    GST_DEBUG_OBJECT (overlay, "negotiation failed, schedule reconfigure");
    gst_pad_mark_reconfigure (srcpad);
  }

  gst_caps_unref (overlay_caps);

  return ret;
}

static gboolean
gst_object_detection_overlay_setcaps (GstObjectDetectionOverlay * overlay,
    GstCaps * caps)
{
  gboolean ret = FALSE;

  if (!gst_video_info_from_caps (overlay->in_info, caps))
    goto invalid_caps;

  ret = gst_object_detection_overlay_negotiate (overlay, caps);
  GST_VIDEO_FILTER (overlay)->negotiated = ret;

  if (!overlay->attach_compo_to_buffer &&
      !gst_object_detection_overlay_can_handle_caps (caps)) {
    GST_DEBUG_OBJECT (overlay, "unsupported caps %" GST_PTR_FORMAT, caps);
    ret = FALSE;
  }

  return ret;

  /* ERRORS */
invalid_caps:
  {
    GST_DEBUG_OBJECT (overlay, "could not parse caps");
    return FALSE;
  }
}

static gboolean
gst_object_detection_overlay_sink_event (GstBaseTransform * trans,
    GstEvent * event)
{
  gboolean ret = FALSE;
  GST_DEBUG_OBJECT (trans, "received sink event %s",
      GST_EVENT_TYPE_NAME (event));

  GstObjectDetectionOverlay *overlay = GST_OBJECT_DETECTION_OVERLAY (trans);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;
      gst_event_parse_caps (event, &caps);
      ret = gst_object_detection_overlay_setcaps (overlay, caps);
      gst_event_unref (event);
      break;
    }
    case GST_EVENT_EOS:
      g_mutex_lock (&overlay->stream_event_mutex);
      GST_INFO_OBJECT (overlay, "EOS");
      overlay->eos = TRUE;
      g_mutex_unlock (&overlay->stream_event_mutex);
      break;
    case GST_EVENT_FLUSH_START:
      g_mutex_lock (&overlay->stream_event_mutex);
      GST_INFO_OBJECT (overlay, "Flush stop");
      overlay->flushing = TRUE;
      g_mutex_unlock (&overlay->stream_event_mutex);
      break;
    case GST_EVENT_FLUSH_STOP:
      g_mutex_lock (&overlay->stream_event_mutex);
      GST_INFO_OBJECT (overlay, "Flush stop");
      overlay->eos = FALSE;
      overlay->flushing = FALSE;
      g_mutex_unlock (&overlay->stream_event_mutex);
      break;
    default:
      ret = GST_BASE_TRANSFORM_CLASS (parent_class)->sink_event (trans, event);
      break;
  }

  return ret;
}

static gboolean
gst_object_detection_overlay_start (GstBaseTransform * trans)
{
  GstObjectDetectionOverlay *overlay = GST_OBJECT_DETECTION_OVERLAY (trans);
  PangoFontDescription *desc;
  PangoFontMap *fontmap;

  fontmap = pango_cairo_font_map_new ();
  overlay->pango_context =
      pango_font_map_create_context (PANGO_FONT_MAP (fontmap));
  g_object_unref (fontmap);
  overlay->pango_layout = pango_layout_new (overlay->pango_context);
  desc = pango_context_get_font_description (overlay->pango_context);
  pango_font_description_set_size (desc, 10000);
  pango_font_description_set_weight (desc, PANGO_WEIGHT_ULTRALIGHT);
  pango_context_set_font_description (overlay->pango_context, desc);
  pango_layout_set_alignment (overlay->pango_layout, PANGO_ALIGN_LEFT);

  gst_object_detection_overlay_adj_labels_outline_ofs (overlay, desc);
  GST_DEBUG_OBJECT (overlay, "labels_outline_offset %f",
      overlay->labels_outline_ofs);

  return TRUE;
}

static gboolean
gst_object_detection_overlay_stop (GstBaseTransform * trans)
{
  GstObjectDetectionOverlay *overlay = GST_OBJECT_DETECTION_OVERLAY (trans);

  g_clear_object (&overlay->pango_layout);
  g_clear_object (&overlay->pango_context);
  gst_clear_buffer (&overlay->canvas);

  return TRUE;
}

static gboolean
gst_object_detection_overlay_set_info (GstVideoFilter * filter,
    GstCaps * incaps, GstVideoInfo * in_info, GstCaps * outcaps,
    GstVideoInfo * out_info)
{
  GstObjectDetectionOverlay *overlay = GST_OBJECT_DETECTION_OVERLAY (filter);
  GST_DEBUG_OBJECT (filter, "set_info incaps:%s", gst_caps_to_string (incaps));
  GST_DEBUG_OBJECT (filter, "set_info outcaps:%s",
      gst_caps_to_string (outcaps));

  filter->in_info = *in_info;
  filter->out_info = *out_info;

  cairo_matrix_init_scale (&overlay->cairo_matrix, 1, 1);
  overlay->render_len = GST_VIDEO_INFO_WIDTH (in_info) *
      GST_VIDEO_INFO_HEIGHT (in_info) * 4;
  return TRUE;
}

static void
gst_object_detection_overlay_create_cairo_context (GstObjectDetectionOverlay *
    overlay, GstObjectDetectionOverlayPangoCairoContext * cairo_ctx,
    guint8 * data)
{
  cairo_ctx->cairo_matrix = &overlay->cairo_matrix;
  cairo_ctx->surface = cairo_image_surface_create_for_data (data,
      CAIRO_FORMAT_ARGB32, GST_VIDEO_INFO_WIDTH (overlay->in_info),
      GST_VIDEO_INFO_HEIGHT (overlay->in_info),
      GST_VIDEO_INFO_WIDTH (overlay->in_info) * 4);
  cairo_ctx->cr = cairo_create (cairo_ctx->surface);

  /* clear surface */
  cairo_set_operator (cairo_ctx->cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cairo_ctx->cr);
  cairo_set_operator (cairo_ctx->cr, CAIRO_OPERATOR_OVER);

  /* apply transformations */
  cairo_set_matrix (cairo_ctx->cr, cairo_ctx->cairo_matrix);
  cairo_save (cairo_ctx->cr);
}

static void
    gst_object_detection_overlay_destroy_cairo_context
    (GstObjectDetectionOverlayPangoCairoContext * cairo_ctx)
{
  cairo_restore (cairo_ctx->cr);
  cairo_destroy (cairo_ctx->cr);
  cairo_surface_destroy (cairo_ctx->surface);
}

static GstFlowReturn
gst_object_detection_overlay_transform_frame_ip (GstVideoFilter * filter,
    GstVideoFrame * frame)
{
  GstObjectDetectionOverlay *overlay = GST_OBJECT_DETECTION_OVERLAY (filter);
  GstVideoOverlayCompositionMeta *composition_meta;
  gpointer state = NULL;
  GstVideoOverlayRectangle *rectangle = NULL;
  gchar str_buf[5];
  GstAnalyticsMtd rlt_mtd;
  GstAnalyticsODMtd *od_mtd;
  gint x, y, w, h;
  gfloat loc_confi_lvl;
  gboolean success;

  GST_DEBUG_OBJECT (filter, "buffer writeable=%d",
      gst_buffer_is_writable (frame->buffer));

  g_mutex_lock (&overlay->stream_event_mutex);
  if (overlay->eos || overlay->flushing) {
    g_mutex_unlock (&overlay->stream_event_mutex);
    return GST_FLOW_EOS;
  }
  g_mutex_unlock (&overlay->stream_event_mutex);

  composition_meta =
      gst_buffer_get_video_overlay_composition_meta (frame->buffer);
  if (composition_meta) {
    if (overlay->upstream_composition != composition_meta->overlay) {
      GST_DEBUG_OBJECT (overlay, "GstVideoOverlayCompositionMeta found.");
      overlay->upstream_composition = composition_meta->overlay;
    }
  } else if (overlay->upstream_composition != NULL) {
    overlay->upstream_composition = NULL;
  }

  GstAnalyticsRelationMeta *rmeta = (GstAnalyticsRelationMeta *)
      gst_buffer_get_meta (GST_BUFFER (frame->buffer),
      GST_ANALYTICS_RELATION_META_API_TYPE);

  if (rmeta) {
    GST_DEBUG_OBJECT (filter, "received buffer with analytics relation meta");

    GstBuffer *buffer;
    GstMapInfo map;
    GstObjectDetectionOverlayPangoCairoContext cairo_ctx;

    buffer = gst_buffer_new_and_alloc (overlay->render_len);
    gst_buffer_add_video_meta (buffer,
        GST_VIDEO_FRAME_FLAG_NONE, GST_VIDEO_OVERLAY_COMPOSITION_FORMAT_RGB,
        GST_VIDEO_INFO_WIDTH (overlay->in_info),
        GST_VIDEO_INFO_HEIGHT (overlay->in_info));

    gst_buffer_replace (&overlay->canvas, buffer);
    gst_buffer_unref (buffer);

    gst_buffer_map (buffer, &map, GST_MAP_READWRITE);
    memset (map.data, 0, overlay->render_len);

    gst_object_detection_overlay_create_cairo_context (overlay,
        &cairo_ctx, map.data);

    if (overlay->composition)
      gst_video_overlay_composition_unref (overlay->composition);

    if (overlay->upstream_composition) {
      overlay->composition =
          gst_video_overlay_composition_copy (overlay->upstream_composition);
    } else {
      overlay->composition = gst_video_overlay_composition_new (NULL);
    }

    /* Get quark represent object detection metadata type */
    GstAnalyticsMtdType rlt_type = gst_analytics_od_mtd_get_mtd_type ();
    while (gst_analytics_relation_meta_iterate (rmeta, &state, rlt_type,
            &rlt_mtd)) {
      od_mtd = (GstAnalyticsODMtd *) & rlt_mtd;
      GST_DEBUG_OBJECT (filter, "buffer contain OD mtd");

      /* Quark representing the type of the object detected by OD */
      GQuark od_obj_type = gst_analytics_od_mtd_get_obj_type (od_mtd);

      // Find classification metadata attached to object detection metadata
      GstAnalyticsMtd cls_rlt_mtd;
      success = gst_analytics_relation_meta_get_direct_related (rmeta,
          gst_analytics_mtd_get_id (
              (GstAnalyticsMtd *) od_mtd),
          GST_ANALYTICS_REL_TYPE_RELATE_TO,
          gst_analytics_cls_mtd_get_mtd_type (), NULL, &cls_rlt_mtd);

      gst_object_detection_overlay_render_boundingbox
          (GST_OBJECT_DETECTION_OVERLAY (filter), &cairo_ctx, od_mtd);

      if (overlay->draw_labels) {
        if (success) {
          /* Use associated classification analytics-meta */
          g_snprintf (str_buf, sizeof (str_buf), "%04.2f",
              gst_analytics_cls_mtd_get_level (
                  (GstAnalyticsClsMtd *) & cls_rlt_mtd, 0));

          od_obj_type = gst_analytics_cls_mtd_get_quark (&cls_rlt_mtd, 0);
        } else {
          /* Use basic class type directly on OD.
           * Here we want the confidence level of the bbox but to retrieve
           * we need to also retrieve the bbox location. */
          gst_analytics_od_mtd_get_location (od_mtd, &x, &y, &w, &h,
              &loc_confi_lvl);
          GST_TRACE_OBJECT (filter, "obj {type: %s loc:[(%u,%u)-(%ux%u)] @ %f}",
              g_quark_to_string (od_obj_type), x, y, w, h, loc_confi_lvl);

          g_snprintf (str_buf, sizeof (str_buf), "%04.2f", loc_confi_lvl);
        }
        gchar *text = g_strdup_printf ("%s (c=%s)",
            g_quark_to_string (od_obj_type), str_buf);

        gst_object_detection_overlay_render_text_annotation
            (GST_OBJECT_DETECTION_OVERLAY (filter), &cairo_ctx, od_mtd, text);

        g_free (text);
      }
    }

    rectangle = gst_video_overlay_rectangle_new_raw (overlay->canvas,
        0, 0, GST_VIDEO_INFO_WIDTH (overlay->in_info),
        GST_VIDEO_INFO_HEIGHT (overlay->in_info),
        GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA);
    gst_video_overlay_composition_add_rectangle (overlay->composition,
        rectangle);
    gst_video_overlay_rectangle_unref (rectangle);

    gst_object_detection_overlay_destroy_cairo_context (&cairo_ctx);
    gst_buffer_unmap (buffer, &map);

  }

  if (overlay->composition) {
    GST_DEBUG_OBJECT (filter, "have composition");

    if (overlay->attach_compo_to_buffer) {
      GST_DEBUG_OBJECT (filter, "attach");

      gst_buffer_add_video_overlay_composition_meta (frame->buffer,
          overlay->composition);
    } else {
      gst_video_overlay_composition_blend (overlay->composition, frame);
    }
  }

  return GST_FLOW_OK;
}

static void
gst_object_detection_overlay_render_boundingbox (GstObjectDetectionOverlay
    * overlay, GstObjectDetectionOverlayPangoCairoContext * ctx,
    GstAnalyticsODMtd * od_mtd)
{
  gint x, y, w, h;
  gfloat _dummy;
  cairo_save (ctx->cr);
  gst_analytics_od_mtd_get_location (od_mtd, &x, &y, &w, &h, &_dummy);
  gint maxw = GST_VIDEO_INFO_WIDTH (overlay->in_info) - 1;
  gint maxh = GST_VIDEO_INFO_HEIGHT (overlay->in_info) - 1;

  x = CLAMP (x, 0, maxw);
  y = CLAMP (y, 0, maxh);
  w = CLAMP (w, 0, maxw - x);
  h = CLAMP (h, 0, maxh - y);

  /* Set bounding box stroke color and width */
  cairo_set_source_rgba (ctx->cr,
      ((overlay->od_outline_color >> 16) & 0xFF) / 255.0,
      ((overlay->od_outline_color >> 8) & 0xFF) / 255.0,
      ((overlay->od_outline_color) & 0xFF) / 255.0,
      ((overlay->od_outline_color >> 24) & 0xFF) / 255.0);
  cairo_set_line_width (ctx->cr, overlay->od_outline_stroke_width);

  /* draw bounding box */
  cairo_rectangle (ctx->cr, x, y, w, h);
  cairo_stroke (ctx->cr);
  cairo_restore (ctx->cr);
}

static void
gst_object_detection_overlay_render_text_annotation (GstObjectDetectionOverlay
    * overlay, GstObjectDetectionOverlayPangoCairoContext * ctx,
    GstAnalyticsODMtd * od_mtd, const gchar * annotation)
{
  PangoRectangle ink_rect, logical_rect;
  gint x, y, w, h;
  gfloat _dummy;
  gint maxw = GST_VIDEO_INFO_WIDTH (overlay->in_info) - 1;
  gint maxh = GST_VIDEO_INFO_HEIGHT (overlay->in_info) - 1;

  cairo_save (ctx->cr);
  gst_analytics_od_mtd_get_location (od_mtd, &x, &y, &w, &h, &_dummy);

  x = CLAMP (x, 0, maxw);
  y = CLAMP (y, 0, maxh);
  w = CLAMP (w, 0, maxw - x);
  h = CLAMP (h, 0, maxh - y);

  /* Set label strokes color and width */
  cairo_set_source_rgba (ctx->cr,
      ((overlay->labels_color >> 16) & 0xFF) / 255.0,
      ((overlay->labels_color >> 8) & 0xFF) / 255.0,
      ((overlay->labels_color) & 0xFF) / 255.0,
      ((overlay->labels_color >> 24) & 0xFF) / 255.0);
  cairo_set_line_width (ctx->cr, overlay->labels_stroke_width);

  pango_layout_set_markup (overlay->pango_layout, annotation,
      strlen (annotation));
  pango_layout_get_pixel_extents (overlay->pango_layout, &ink_rect,
      &logical_rect);
  GST_DEBUG_OBJECT (overlay, "logical_rect:(%d,%d),%dx%d", logical_rect.x,
      logical_rect.y, logical_rect.width, logical_rect.height);
  GST_DEBUG_OBJECT (overlay, "ink_rect:(%d,%d),%dx%d", ink_rect.x, ink_rect.y,
      ink_rect.width, ink_rect.height);
  cairo_move_to (ctx->cr, x + overlay->labels_outline_ofs,
      y - logical_rect.height - overlay->labels_outline_ofs);

  pango_cairo_layout_path (ctx->cr, overlay->pango_layout);
  cairo_stroke (ctx->cr);
  cairo_restore (ctx->cr);
}
