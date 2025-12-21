/* GStreamer
 * Copyright (C) 2018 Sebastian Dröge <sebastian@centricular.com>
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
 * SECTION:element-overlaycomposition
 *
 * The overlaycomposition element renders an overlay using an application
 * provided draw function.
 *
 * ## Example code
 *
 * {{ ../../tests/examples/overlaycomposition/overlaycomposition.c[23:341] }}
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "gstoverlaycomposition.h"

GST_DEBUG_CATEGORY_STATIC (gst_overlay_composition_debug);
#define GST_CAT_DEFAULT gst_overlay_composition_debug

#define OVERLAY_COMPOSITION_CAPS GST_VIDEO_CAPS_MAKE (GST_VIDEO_OVERLAY_COMPOSITION_BLEND_FORMATS)

#define ALL_CAPS OVERLAY_COMPOSITION_CAPS ";" \
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("ANY", GST_VIDEO_FORMATS_ANY)

enum
{
  SIGNAL_CAPS_CHANGED,
  SIGNAL_DRAW,
  LAST_SIGNAL
};

static guint overlay_composition_signals[LAST_SIGNAL];

static GstStaticCaps overlay_composition_caps =
GST_STATIC_CAPS (OVERLAY_COMPOSITION_CAPS);

static gboolean
can_blend_caps (GstCaps * incaps)
{
  gboolean ret;
  GstCaps *caps;

  caps = gst_static_caps_get (&overlay_composition_caps);
  ret = gst_caps_is_subset (incaps, caps);
  gst_caps_unref (caps);

  return ret;
}

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (ALL_CAPS)
    );

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (ALL_CAPS)
    );

#define parent_class gst_overlay_composition_parent_class
G_DEFINE_TYPE (GstOverlayComposition, gst_overlay_composition,
    GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE (overlaycomposition, "overlaycomposition",
    GST_RANK_NONE, GST_TYPE_OVERLAY_COMPOSITION);

static GstFlowReturn gst_overlay_composition_sink_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buffer);
static gboolean gst_overlay_composition_sink_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static gboolean gst_overlay_composition_sink_query (GstPad * pad,
    GstObject * parent, GstQuery * query);
static gboolean gst_overlay_composition_src_query (GstPad * pad,
    GstObject * parent, GstQuery * query);

static GstStateChangeReturn gst_overlay_composition_change_state (GstElement *
    element, GstStateChange transition);

static void
gst_overlay_composition_class_init (GstOverlayCompositionClass * klass)
{
  GstElementClass *gstelement_class = (GstElementClass *) klass;

  GST_DEBUG_CATEGORY_INIT (gst_overlay_composition_debug, "overlaycomposition",
      0, "Overlay Composition");

  gst_element_class_set_static_metadata (gstelement_class,
      "Overlay Composition", "Filter/Editor/Video",
      "Overlay Composition", "Sebastian Dröge <sebastian@centricular.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gstelement_class->change_state = gst_overlay_composition_change_state;

  /**
   * GstOverlayComposition::draw:
   * @overlay: Overlay element emitting the signal.
   * @sample: #GstSample containing the current buffer, caps and segment.
   *
   * This signal is emitted when the overlay should be drawn.
   *
   * Returns: #GstVideoOverlayComposition or %NULL
   */
  overlay_composition_signals[SIGNAL_DRAW] =
      g_signal_new ("draw",
      G_TYPE_FROM_CLASS (klass), 0, 0, NULL, NULL, NULL,
      GST_TYPE_VIDEO_OVERLAY_COMPOSITION, 1, GST_TYPE_SAMPLE);

  /**
   * GstOverlayComposition::caps-changed:
   * @overlay: Overlay element emitting the signal.
   * @caps: The #GstCaps of the element.
   * @window_width: The window render width of downstream, or 0.
   * @window_height: The window render height of downstream, or 0.
   *
   * This signal is emitted when the caps of the element has changed.
   *
   * The window width and height define the resolution at which the frame is
   * going to be rendered in the end by e.g. a video sink (i.e. the window
   * size).
   */
  overlay_composition_signals[SIGNAL_CAPS_CHANGED] =
      g_signal_new ("caps-changed",
      G_TYPE_FROM_CLASS (klass), 0, 0, NULL, NULL, NULL, G_TYPE_NONE, 3,
      GST_TYPE_CAPS, G_TYPE_UINT, G_TYPE_UINT);
}

static void
gst_overlay_composition_init (GstOverlayComposition * self)
{
  self->sinkpad = gst_pad_new_from_static_template (&sink_template, "sink");
  gst_pad_set_chain_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_overlay_composition_sink_chain));
  gst_pad_set_event_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_overlay_composition_sink_event));
  gst_pad_set_query_function (self->sinkpad,
      GST_DEBUG_FUNCPTR (gst_overlay_composition_sink_query));
  GST_PAD_SET_PROXY_ALLOCATION (self->sinkpad);
  gst_element_add_pad (GST_ELEMENT (self), self->sinkpad);

  self->srcpad = gst_pad_new_from_static_template (&src_template, "src");
  gst_pad_set_query_function (self->srcpad,
      GST_DEBUG_FUNCPTR (gst_overlay_composition_src_query));
  gst_element_add_pad (GST_ELEMENT (self), self->srcpad);
}

static GstStateChangeReturn
gst_overlay_composition_change_state (GstElement * element,
    GstStateChange transition)
{
  GstOverlayComposition *self = GST_OVERLAY_COMPOSITION (element);
  GstStateChangeReturn state_ret;

  switch (transition) {
    default:
      break;
  }

  state_ret =
      GST_ELEMENT_CLASS (gst_overlay_composition_parent_class)->change_state
      (element, transition);
  if (state_ret == GST_STATE_CHANGE_FAILURE)
    return state_ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_segment_init (&self->segment, GST_FORMAT_UNDEFINED);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      memset (&self->info, 0, sizeof (self->info));
      self->window_width = self->window_height = 0;
      self->attach_compo_to_buffer = FALSE;
      if (self->sample) {
        gst_sample_unref (self->sample);
        self->sample = NULL;
      }
      gst_caps_replace (&self->caps, NULL);
      gst_segment_init (&self->segment, GST_FORMAT_UNDEFINED);
      break;
    default:
      break;
  }

  return state_ret;
}

/* Based on gstbasetextoverlay.c */
static gboolean
gst_overlay_composition_negotiate (GstOverlayComposition * self, GstCaps * caps)
{
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

  GST_DEBUG_OBJECT (self, "performing negotiation");

  /* Clear any pending reconfigure to avoid negotiating twice */
  gst_pad_check_reconfigure (self->srcpad);

  self->window_width = self->window_height = 0;

  if (!caps)
    caps = gst_pad_get_current_caps (self->sinkpad);
  else
    gst_caps_ref (caps);

  if (!caps || gst_caps_is_empty (caps))
    goto no_format;

  /* Check if upstream caps have meta */
  if ((f = gst_caps_get_features (caps, 0))) {
    upstream_has_meta = gst_caps_features_contains (f,
        GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION);
  }

  /* Initialize dimensions */
  width = self->info.width;
  height = self->info.height;

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
    peercaps = gst_pad_peer_query_caps (self->srcpad, overlay_caps);
    caps_has_meta = !gst_caps_is_empty (peercaps);
    gst_caps_unref (peercaps);

    GST_DEBUG_OBJECT (self, "caps have overlay meta %d", caps_has_meta);
  }

  if (upstream_has_meta || caps_has_meta) {
    /* Send caps immediately, it's needed by GstBaseTransform to get a reply
     * from allocation query */
    ret = gst_pad_set_caps (self->srcpad, overlay_caps);

    /* First check if the allocation meta has compositon */
    query = gst_query_new_allocation (overlay_caps, FALSE);

    if (!gst_pad_peer_query (self->srcpad, query)) {
      /* no problem, we use the query defaults */
      GST_DEBUG_OBJECT (self, "ALLOCATION query failed");

      /* In case we were flushing, mark reconfigure and fail this method,
       * will make it retry */
      if (GST_PAD_IS_FLUSHING (self->srcpad))
        ret = FALSE;
    }

    alloc_has_meta = gst_query_find_allocation_meta (query,
        GST_VIDEO_OVERLAY_COMPOSITION_META_API_TYPE, &alloc_index);

    GST_DEBUG_OBJECT (self, "sink alloc has overlay meta %d", alloc_has_meta);

    if (alloc_has_meta) {
      const GstStructure *params;

      gst_query_parse_nth_allocation_meta (query, alloc_index, &params);
      if (params) {
        if (gst_structure_get (params, "width", G_TYPE_UINT, &width,
                "height", G_TYPE_UINT, &height, NULL)) {
          GST_DEBUG_OBJECT (self, "received window size: %dx%d", width, height);
          g_assert (width != 0 && height != 0);
        }
      }
    }

    gst_query_unref (query);
  }

  /* Update render size if needed */
  self->window_width = width;
  self->window_height = height;

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
      attach = !can_blend_caps (caps);
    }
  } else {
    ret = can_blend_caps (caps);
  }

  /* If we attach, then pick the overlay caps */
  if (attach) {
    GST_DEBUG_OBJECT (self, "Using caps %" GST_PTR_FORMAT, overlay_caps);
    /* Caps where already sent */
  } else if (ret) {
    GST_DEBUG_OBJECT (self, "Using caps %" GST_PTR_FORMAT, caps);
    ret = gst_pad_set_caps (self->srcpad, caps);
  }

  self->attach_compo_to_buffer = attach;

  if (!ret) {
    GST_DEBUG_OBJECT (self, "negotiation failed, schedule reconfigure");
    gst_pad_mark_reconfigure (self->srcpad);
  }

  g_signal_emit (self, overlay_composition_signals[SIGNAL_CAPS_CHANGED], 0,
      caps, self->window_width, self->window_height, NULL);

  gst_caps_unref (overlay_caps);
  gst_caps_unref (caps);

  return ret;

no_format:
  {
    if (caps)
      gst_caps_unref (caps);
    gst_pad_mark_reconfigure (self->srcpad);
    return FALSE;
  }
}

static gboolean
gst_overlay_composition_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstOverlayComposition *self = GST_OVERLAY_COMPOSITION (parent);
  gboolean ret = FALSE;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEGMENT:
      gst_event_copy_segment (event, &self->segment);
      ret = gst_pad_event_default (pad, parent, event);
      break;
    case GST_EVENT_CAPS:{
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      if (!gst_video_info_from_caps (&self->info, caps)) {
        gst_event_unref (event);
        ret = FALSE;
        break;
      }

      if (!gst_overlay_composition_negotiate (self, caps)) {
        gst_event_unref (event);
        ret = FALSE;
        break;
      }

      gst_caps_replace (&self->caps, caps);

      ret = TRUE;
      gst_event_unref (event);

      break;
    }
    case GST_EVENT_FLUSH_STOP:
      gst_segment_init (&self->segment, GST_FORMAT_UNDEFINED);
      ret = gst_pad_event_default (pad, parent, event);
      break;
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }

  return ret;
}

/* Based on gstbasetextoverlay.c */
/**
 * add_feature_and_intersect:
 *
 * Creates a new #GstCaps containing the (given caps +
 * given caps feature) + (given caps intersected by the
 * given filter).
 *
 * Returns: the new #GstCaps
 */
static GstCaps *
add_feature_and_intersect (GstCaps * caps,
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

/* Based on gstbasetextoverlay.c */
/* intersect_by_feature:
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
intersect_by_feature (GstCaps * caps, const gchar * feature, GstCaps * filter)
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

/* Based on gstbasetextoverlay.c */
static GstCaps *
gst_overlay_composition_sink_query_caps (GstOverlayComposition * self,
    GstCaps * filter)
{
  GstCaps *peer_caps = NULL, *caps = NULL, *overlay_filter = NULL;

  if (filter) {
    /* filter caps + composition feature + filter caps
     * filtered by the software caps. */
    GstCaps *sw_caps = gst_static_caps_get (&overlay_composition_caps);
    overlay_filter = add_feature_and_intersect (filter,
        GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION, sw_caps);
    gst_caps_unref (sw_caps);

    GST_DEBUG_OBJECT (self->sinkpad, "overlay filter %" GST_PTR_FORMAT,
        overlay_filter);
  }

  peer_caps = gst_pad_peer_query_caps (self->srcpad, overlay_filter);

  if (overlay_filter)
    gst_caps_unref (overlay_filter);

  if (peer_caps) {

    GST_DEBUG_OBJECT (self->sinkpad, "peer caps  %" GST_PTR_FORMAT, peer_caps);

    if (gst_caps_is_any (peer_caps)) {
      /* if peer returns ANY caps, return filtered src pad template caps */
      caps = gst_caps_copy (gst_pad_get_pad_template_caps (self->srcpad));
    } else {

      /* duplicate caps which contains the composition into one version with
       * the meta and one without. Filter the other caps by the software caps */
      GstCaps *sw_caps = gst_static_caps_get (&overlay_composition_caps);
      caps = intersect_by_feature (peer_caps,
          GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION, sw_caps);
      gst_caps_unref (sw_caps);
    }

    gst_caps_unref (peer_caps);

  } else {
    /* no peer, our padtemplate is enough then */
    caps = gst_pad_get_pad_template_caps (self->sinkpad);
  }

  if (filter) {
    GstCaps *intersection = gst_caps_intersect_full (filter, caps,
        GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
    caps = intersection;
  }

  GST_DEBUG_OBJECT (self->sinkpad, "returning  %" GST_PTR_FORMAT, caps);

  return caps;
}

/* Based on gstbasetextoverlay.c */
static GstCaps *
gst_overlay_composition_src_query_caps (GstOverlayComposition * self,
    GstCaps * filter)
{
  GstCaps *peer_caps = NULL, *caps = NULL, *overlay_filter = NULL;

  if (filter) {
    /* duplicate filter caps which contains the composition into one version
     * with the meta and one without. Filter the other caps by the software
     * caps */
    GstCaps *sw_caps = gst_static_caps_get (&overlay_composition_caps);
    overlay_filter =
        intersect_by_feature (filter,
        GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION, sw_caps);
    gst_caps_unref (sw_caps);
  }

  peer_caps = gst_pad_peer_query_caps (self->sinkpad, overlay_filter);

  if (overlay_filter)
    gst_caps_unref (overlay_filter);

  if (peer_caps) {

    GST_DEBUG_OBJECT (self->srcpad, "peer caps  %" GST_PTR_FORMAT, peer_caps);

    if (gst_caps_is_any (peer_caps)) {

      /* if peer returns ANY caps, return filtered sink pad template caps */
      caps = gst_caps_copy (gst_pad_get_pad_template_caps (self->sinkpad));

    } else {

      /* return upstream caps + composition feature + upstream caps
       * filtered by the software caps. */
      GstCaps *sw_caps = gst_static_caps_get (&overlay_composition_caps);
      caps = add_feature_and_intersect (peer_caps,
          GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION, sw_caps);
      gst_caps_unref (sw_caps);
    }

    gst_caps_unref (peer_caps);

  } else {
    /* no peer, our padtemplate is enough then */
    caps = gst_pad_get_pad_template_caps (self->srcpad);
  }

  if (filter) {
    GstCaps *intersection;

    intersection =
        gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
    caps = intersection;
  }
  GST_DEBUG_OBJECT (self->srcpad, "returning  %" GST_PTR_FORMAT, caps);

  return caps;
}

static gboolean
gst_overlay_composition_sink_query (GstPad * pad, GstObject * parent,
    GstQuery * query)
{
  GstOverlayComposition *self = GST_OVERLAY_COMPOSITION (parent);
  gboolean ret = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:{
      GstCaps *filter, *caps;

      gst_query_parse_caps (query, &filter);
      caps = gst_overlay_composition_sink_query_caps (self, filter);
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
gst_overlay_composition_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query)
{
  GstOverlayComposition *self = GST_OVERLAY_COMPOSITION (parent);
  gboolean ret = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:{
      GstCaps *filter, *caps;

      gst_query_parse_caps (query, &filter);
      caps = gst_overlay_composition_src_query_caps (self, filter);
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

static GstFlowReturn
gst_overlay_composition_sink_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer)
{
  GstOverlayComposition *self = GST_OVERLAY_COMPOSITION (parent);
  GstVideoOverlayComposition *compo = NULL;
  GstVideoOverlayCompositionMeta *upstream_compo_meta;

  if (gst_pad_check_reconfigure (self->srcpad)) {
    if (!gst_overlay_composition_negotiate (self, NULL)) {
      gst_pad_mark_reconfigure (self->srcpad);
      gst_buffer_unref (buffer);
      GST_OBJECT_LOCK (self->srcpad);
      if (GST_PAD_IS_FLUSHING (self->srcpad)) {
        GST_OBJECT_UNLOCK (self->srcpad);
        return GST_FLOW_FLUSHING;
      }
      GST_OBJECT_UNLOCK (self->srcpad);
      return GST_FLOW_NOT_NEGOTIATED;
    }
  }

  if (!self->sample) {
    self->sample = gst_sample_new (buffer, self->caps, &self->segment, NULL);
  } else {
    self->sample = gst_sample_make_writable (self->sample);
    gst_sample_set_buffer (self->sample, buffer);
    gst_sample_set_caps (self->sample, self->caps);
    gst_sample_set_segment (self->sample, &self->segment);
  }

  g_signal_emit (self, overlay_composition_signals[SIGNAL_DRAW], 0,
      self->sample, &compo);

  /* Don't store the buffer in the sample any longer, otherwise it will not
   * be writable below as we have one reference in the sample and one in
   * this function.
   *
   * If the sample is not writable itself then the application kept an
   * reference itself.
   */
  if (gst_sample_is_writable (self->sample)) {
    gst_sample_set_buffer (self->sample, NULL);
  }

  if (!compo) {
    GST_DEBUG_OBJECT (self->sinkpad,
        "Application did not provide an overlay composition");
    return gst_pad_push (self->srcpad, buffer);
  }

  /* If upstream attached a meta, we can safely add our own things
   * in it. Upstream must've checked that downstream supports it */
  upstream_compo_meta = gst_buffer_get_video_overlay_composition_meta (buffer);
  if (upstream_compo_meta) {
    GstVideoOverlayComposition *merged_compo =
        gst_video_overlay_composition_copy (upstream_compo_meta->overlay);
    guint i, n;

    GST_DEBUG_OBJECT (self->sinkpad,
        "Appending to upstream overlay composition");

    n = gst_video_overlay_composition_n_rectangles (compo);
    for (i = 0; i < n; i++) {
      GstVideoOverlayRectangle *rect =
          gst_video_overlay_composition_get_rectangle (compo, i);
      gst_video_overlay_composition_add_rectangle (merged_compo, rect);
    }

    gst_video_overlay_composition_unref (compo);
    gst_video_overlay_composition_unref (upstream_compo_meta->overlay);
    upstream_compo_meta->overlay = merged_compo;
  } else if (self->attach_compo_to_buffer) {
    GST_DEBUG_OBJECT (self->sinkpad, "Attaching as meta");

    buffer = gst_buffer_make_writable (buffer);
    gst_buffer_add_video_overlay_composition_meta (buffer, compo);
    gst_video_overlay_composition_unref (compo);
  } else {
    GstVideoFrame frame;

    buffer = gst_buffer_make_writable (buffer);
    if (!gst_video_frame_map (&frame, &self->info, buffer, GST_MAP_READWRITE)) {
      gst_video_overlay_composition_unref (compo);
      goto map_failed;
    }

    gst_video_overlay_composition_blend (compo, &frame);

    gst_video_frame_unmap (&frame);
    gst_video_overlay_composition_unref (compo);
  }

  return gst_pad_push (self->srcpad, buffer);

map_failed:
  {
    GST_ERROR_OBJECT (self->sinkpad, "Failed to map buffer");
    gst_buffer_unref (buffer);
    return GST_FLOW_ERROR;
  }
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (overlaycomposition, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    overlaycomposition,
    "Renders overlays on top of video frames",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
