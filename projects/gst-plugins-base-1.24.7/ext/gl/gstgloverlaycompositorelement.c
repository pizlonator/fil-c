/*
 * gloverlaycompositor element
 * Copyrithg (C) 2018 Matthew Waters <matthew@centricular.com>
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
 * SECTION:element-glcompositoroverlay
 * @title: glcompositoroverlay
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gl/gstglfuncs.h>
#include <gst/video/video.h>

#include "gstglelements.h"
#include "gstgloverlaycompositorelement.h"

enum
{
  PROP_0,
  PROP_LAST,
};

#define GST_CAT_DEFAULT gst_gl_overlay_compositor_element_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_gl_overlay_compositor_element_debug, "gloverlaycompositorelement", 0, "gloverlaycompositor element");
#define gst_gl_overlay_compositor_element_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGLOverlayCompositorElement,
    gst_gl_overlay_compositor_element, GST_TYPE_GL_FILTER, DEBUG_INIT);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (gloverlaycompositor,
    "gloverlaycompositor", GST_RANK_NONE,
    GST_TYPE_GL_OVERLAY_COMPOSITOR_ELEMENT, gl_element_init (plugin));

static GstStaticPadTemplate overlay_sink_pad_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_GL_MEMORY ","
            GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
            "RGBA") ", texture-target=(string) { 2D, rectangle } ; "
        GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_GL_MEMORY,
            "RGBA") ", texture-target=(string) { 2D, rectangle } ; "
        GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("ANY",
            "RGBA") ", texture-target=(string) { 2D, rectangle } "));

static GstStaticPadTemplate overlay_src_pad_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_GL_MEMORY ","
            GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
            "RGBA") ", texture-target=(string) { 2D, rectangle } ; "
        GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_GL_MEMORY,
            "RGBA") ", texture-target=(string) { 2D, rectangle } ; "
        GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("ANY",
            "RGBA") ", texture-target=(string) { 2D, rectangle } "));

static gboolean
gst_gl_overlay_compositor_element_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query);
static GstFlowReturn _oce_prepare_output_buffer (GstBaseTransform * bt,
    GstBuffer * buffer, GstBuffer ** outbuf);

static gboolean gst_gl_overlay_compositor_element_gl_start (GstGLBaseFilter *
    base);
static void gst_gl_overlay_compositor_element_gl_stop (GstGLBaseFilter * base);

static GstCaps *_oce_transform_internal_caps (GstGLFilter *
    filter, GstPadDirection direction, GstCaps * caps, GstCaps * filter_caps);
static gboolean gst_gl_overlay_compositor_element_filter (GstGLFilter * filter,
    GstBuffer * inbuf, GstBuffer * outbuf);
static gboolean gst_gl_overlay_compositor_element_filter_texture (GstGLFilter *
    filter, GstGLMemory * in_tex, GstGLMemory * out_tex);
static gboolean gst_gl_overlay_compositor_element_callback (GstGLFilter *
    filter, GstGLMemory * in_tex, gpointer stuff);

static void
gst_gl_overlay_compositor_element_class_init (GstGLOverlayCompositorElementClass
    * klass)
{
  GstElementClass *element_class;

  element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_set_metadata (element_class,
      "OpenGL overlaying filter", "Filter/Effect",
      "Flatten a stream containing GstVideoOverlayCompositionMeta",
      "<matthew@centricular.com>");

  gst_element_class_add_static_pad_template (element_class,
      &overlay_src_pad_template);
  gst_element_class_add_static_pad_template (element_class,
      &overlay_sink_pad_template);

  GST_BASE_TRANSFORM_CLASS (klass)->passthrough_on_same_caps = TRUE;
  GST_BASE_TRANSFORM_CLASS (klass)->propose_allocation =
      gst_gl_overlay_compositor_element_propose_allocation;
  GST_BASE_TRANSFORM_CLASS (klass)->prepare_output_buffer =
      _oce_prepare_output_buffer;

  GST_GL_FILTER_CLASS (klass)->filter =
      gst_gl_overlay_compositor_element_filter;
  GST_GL_FILTER_CLASS (klass)->filter_texture =
      gst_gl_overlay_compositor_element_filter_texture;
  GST_GL_FILTER_CLASS (klass)->transform_internal_caps =
      _oce_transform_internal_caps;

  GST_GL_BASE_FILTER_CLASS (klass)->gl_start =
      gst_gl_overlay_compositor_element_gl_start;
  GST_GL_BASE_FILTER_CLASS (klass)->gl_stop =
      gst_gl_overlay_compositor_element_gl_stop;
  GST_GL_BASE_FILTER_CLASS (klass)->supported_gl_api =
      GST_GL_API_OPENGL | GST_GL_API_GLES2 | GST_GL_API_OPENGL3;
}

static void
gst_gl_overlay_compositor_element_init (GstGLOverlayCompositorElement *
    overlay_compositor_element)
{
}

static GstCaps *
_oce_transform_internal_caps (GstGLFilter * filter,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter_caps)
{
  GstCaps *ret;

  /* add/remove the composition overlay meta as necessary */
  if (direction == GST_PAD_SRC) {
    ret = gst_gl_overlay_compositor_add_caps (gst_caps_copy (caps));
  } else {
    guint i, n;
    GstCaps *removed;

    ret = gst_caps_copy (caps);
    removed = gst_caps_copy (caps);
    n = gst_caps_get_size (removed);
    for (i = 0; i < n; i++) {
      GstCapsFeatures *feat = gst_caps_get_features (removed, i);

      if (feat && gst_caps_features_contains (feat,
              GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION)) {
        feat = gst_caps_features_copy (feat);
        /* prefer the passthrough case */
        gst_caps_features_remove (feat,
            GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION);
        gst_caps_set_features (removed, i, feat);
      }
    }

    ret = gst_caps_merge (ret, removed);
  }

  GST_DEBUG_OBJECT (filter, "meta modifications returned caps %" GST_PTR_FORMAT,
      ret);
  return ret;
}

static gboolean
gst_gl_overlay_compositor_element_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query)
{
  GstStructure *allocation_meta = NULL;
  guint width = 0, height = 0;

  if (!GST_BASE_TRANSFORM_CLASS (parent_class)->propose_allocation (trans,
          decide_query, query))
    return FALSE;

  if (decide_query) {
    GstCaps *decide_caps;
    gst_query_parse_allocation (decide_query, &decide_caps, NULL);

    if (decide_caps) {
      GstVideoInfo vinfo;

      if (gst_video_info_from_caps (&vinfo, decide_caps)) {
        width = GST_VIDEO_INFO_WIDTH (&vinfo);
        height = GST_VIDEO_INFO_HEIGHT (&vinfo);
      }
    }
  }

  if ((width == 0 || height == 0) && query) {
    GstCaps *caps;
    gst_query_parse_allocation (query, &caps, NULL);

    if (caps) {
      GstVideoInfo vinfo;

      if (gst_video_info_from_caps (&vinfo, caps)) {
        width = GST_VIDEO_INFO_WIDTH (&vinfo);
        height = GST_VIDEO_INFO_HEIGHT (&vinfo);
      }
    }
  }

  if (width != 0 && height != 0) {
    allocation_meta =
        gst_structure_new ("GstVideoOverlayCompositionMeta",
        "width", G_TYPE_UINT, width, "height", G_TYPE_UINT, height, NULL);
  }

  GST_DEBUG_OBJECT (trans, "Adding overlay composition meta with size %ux%u",
      width, height);
  if (allocation_meta) {
    if (query)
      gst_query_add_allocation_meta (query,
          GST_VIDEO_OVERLAY_COMPOSITION_META_API_TYPE, allocation_meta);
    gst_structure_free (allocation_meta);
  }
  return TRUE;
}

static void
gst_gl_overlay_compositor_element_gl_stop (GstGLBaseFilter * base)
{
  GstGLOverlayCompositorElement *self =
      GST_GL_OVERLAY_COMPOSITOR_ELEMENT (base);

  if (self->shader)
    gst_object_unref (self->shader);
  self->shader = NULL;

  if (self->overlay_compositor) {
    gst_gl_overlay_compositor_free_overlays (self->overlay_compositor);
    gst_object_unref (self->overlay_compositor);
  }
  self->overlay_compositor = NULL;

  GST_GL_BASE_FILTER_CLASS (parent_class)->gl_stop (base);
}

static gboolean
gst_gl_overlay_compositor_element_gl_start (GstGLBaseFilter * base)
{
  GstGLOverlayCompositorElement *self =
      GST_GL_OVERLAY_COMPOSITOR_ELEMENT (base);
  GError *error = NULL;

  self->overlay_compositor = gst_gl_overlay_compositor_new (base->context);
  g_object_set (self->overlay_compositor, "yinvert", TRUE, NULL);

  if (!(self->shader = gst_gl_shader_new_default (base->context, &error))) {
    GST_ELEMENT_ERROR (base, RESOURCE, NOT_FOUND, ("%s",
            "Failed to compile identity shader"), ("%s", error->message));
    return FALSE;
  }

  return GST_GL_BASE_FILTER_CLASS (parent_class)->gl_start (base);
}

static GstFlowReturn
_oce_prepare_output_buffer (GstBaseTransform * bt,
    GstBuffer * buffer, GstBuffer ** outbuf)
{
  GstGLOverlayCompositorElement *self = GST_GL_OVERLAY_COMPOSITOR_ELEMENT (bt);
  GstVideoOverlayCompositionMeta *comp_meta;

  if (gst_base_transform_is_passthrough (bt))
    goto passthrough;

  if (!self->overlay_compositor)
    return GST_FLOW_NOT_NEGOTIATED;

  comp_meta = gst_buffer_get_video_overlay_composition_meta (buffer);
  if (!comp_meta)
    goto passthrough;

  if (gst_video_overlay_composition_n_rectangles (comp_meta->overlay) == 0)
    goto passthrough;

  return GST_BASE_TRANSFORM_CLASS (parent_class)->prepare_output_buffer (bt,
      buffer, outbuf);

passthrough:
  GST_LOG_OBJECT (bt, "passthrough detected, forwarding input buffer");
  *outbuf = buffer;
  return GST_FLOW_OK;
}

static gboolean
gst_gl_overlay_compositor_element_filter (GstGLFilter * filter,
    GstBuffer * inbuf, GstBuffer * outbuf)
{
  GstGLOverlayCompositorElement *self =
      GST_GL_OVERLAY_COMPOSITOR_ELEMENT (filter);

  if (inbuf == outbuf)
    return TRUE;

  gst_gl_overlay_compositor_upload_overlays (self->overlay_compositor, inbuf);

  return gst_gl_filter_filter_texture (filter, inbuf, outbuf);
}

static gboolean
gst_gl_overlay_compositor_element_filter_texture (GstGLFilter * filter,
    GstGLMemory * in_tex, GstGLMemory * out_tex)
{
  GstGLOverlayCompositorElement *self =
      GST_GL_OVERLAY_COMPOSITOR_ELEMENT (filter);

  gst_gl_filter_render_to_target_with_shader (filter, in_tex, out_tex,
      self->shader);

  gst_gl_filter_render_to_target (filter, NULL, out_tex,
      gst_gl_overlay_compositor_element_callback, NULL);

  return TRUE;
}

static gboolean
gst_gl_overlay_compositor_element_callback (GstGLFilter * filter,
    GstGLMemory * in_tex, gpointer stuff)
{
  GstGLOverlayCompositorElement *self =
      GST_GL_OVERLAY_COMPOSITOR_ELEMENT (filter);

  GST_LOG_OBJECT (self, "drawing overlays");

  gst_gl_overlay_compositor_draw_overlays (self->overlay_compositor);

  return TRUE;
}
