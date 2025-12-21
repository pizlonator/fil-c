/*
 * GStreamer
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
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

#include <stdio.h>

#include <gst/gl/gl.h>

#include "gstglelements.h"
#include "gstgluploadelement.h"

GST_DEBUG_CATEGORY_STATIC (gst_gl_upload_element_debug);
#define GST_CAT_DEFAULT gst_gl_upload_element_debug

#define gst_gl_upload_element_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGLUploadElement, gst_gl_upload_element,
    GST_TYPE_GL_BASE_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_gl_upload_element_debug, "gluploadelement", 0,
        "glupload Element"););
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (glupload, "glupload",
    GST_RANK_NONE, GST_TYPE_GL_UPLOAD_ELEMENT, gl_element_init (plugin));

static gboolean gst_gl_upload_element_get_unit_size (GstBaseTransform * trans,
    GstCaps * caps, gsize * size);
static GstCaps *_gst_gl_upload_element_transform_caps (GstBaseTransform * bt,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static gboolean _gst_gl_upload_element_set_caps (GstBaseTransform * bt,
    GstCaps * in_caps, GstCaps * out_caps);
static gboolean gst_gl_upload_element_filter_meta (GstBaseTransform * trans,
    GstQuery * query, GType api, const GstStructure * params);
static gboolean _gst_gl_upload_element_propose_allocation (GstBaseTransform *
    bt, GstQuery * decide_query, GstQuery * query);
static gboolean _gst_gl_upload_element_decide_allocation (GstBaseTransform *
    trans, GstQuery * query);
static GstFlowReturn
gst_gl_upload_element_prepare_output_buffer (GstBaseTransform * bt,
    GstBuffer * buffer, GstBuffer ** outbuf);
static GstFlowReturn gst_gl_upload_element_transform (GstBaseTransform * bt,
    GstBuffer * buffer, GstBuffer * outbuf);
static gboolean gst_gl_upload_element_stop (GstBaseTransform * bt);
static GstCaps *gst_gl_upload_element_fixate_caps (GstBaseTransform * bt,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps);
static GstStateChangeReturn
gst_gl_upload_element_change_state (GstElement * element,
    GstStateChange transition);

static GstStaticPadTemplate gst_gl_upload_element_src_pad_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw(ANY)"));

static void
_gst_gl_upload_element_clear_upload (GstGLUploadElement * upload)
{
  GstGLUpload *ul = NULL;

  GST_OBJECT_LOCK (upload);
  ul = upload->upload;
  upload->upload = NULL;
  GST_OBJECT_UNLOCK (upload);

  if (ul)
    gst_object_unref (ul);
}

static void
gst_gl_upload_element_finalize (GObject * object)
{
  GstGLUploadElement *upload = GST_GL_UPLOAD_ELEMENT (object);

  _gst_gl_upload_element_clear_upload (upload);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_gl_upload_element_class_init (GstGLUploadElementClass * klass)
{
  GstBaseTransformClass *bt_class = GST_BASE_TRANSFORM_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstCaps *upload_caps;

  bt_class->transform_caps = _gst_gl_upload_element_transform_caps;
  bt_class->set_caps = _gst_gl_upload_element_set_caps;
  bt_class->filter_meta = gst_gl_upload_element_filter_meta;
  bt_class->propose_allocation = _gst_gl_upload_element_propose_allocation;
  bt_class->decide_allocation = _gst_gl_upload_element_decide_allocation;
  bt_class->get_unit_size = gst_gl_upload_element_get_unit_size;
  bt_class->prepare_output_buffer = gst_gl_upload_element_prepare_output_buffer;
  bt_class->transform = gst_gl_upload_element_transform;
  bt_class->stop = gst_gl_upload_element_stop;
  bt_class->fixate_caps = gst_gl_upload_element_fixate_caps;

  element_class->change_state = gst_gl_upload_element_change_state;

  gst_element_class_add_static_pad_template (element_class,
      &gst_gl_upload_element_src_pad_template);

  upload_caps = gst_gl_upload_get_input_template_caps ();
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, upload_caps));
  gst_caps_unref (upload_caps);

  gst_element_class_set_metadata (element_class,
      "OpenGL uploader", "Filter/Video",
      "Uploads data into OpenGL", "Matthew Waters <matthew@centricular.com>");

  gobject_class->finalize = gst_gl_upload_element_finalize;
}

static void
gst_gl_upload_element_init (GstGLUploadElement * upload)
{
  gst_base_transform_set_prefer_passthrough (GST_BASE_TRANSFORM (upload), TRUE);
}

static gboolean
gst_gl_upload_element_stop (GstBaseTransform * bt)
{
  GstGLUploadElement *upload = GST_GL_UPLOAD_ELEMENT (bt);

  _gst_gl_upload_element_clear_upload (upload);

  return GST_BASE_TRANSFORM_CLASS (parent_class)->stop (bt);
}

static gboolean
gst_gl_upload_element_get_unit_size (GstBaseTransform * trans, GstCaps * caps,
    gsize * size)
{
  gboolean ret = FALSE;
  GstVideoInfo info;

  ret = gst_video_info_from_caps (&info, caps);
  if (ret)
    *size = GST_VIDEO_INFO_SIZE (&info);

  return TRUE;
}

static GstCaps *
_gst_gl_upload_element_transform_caps (GstBaseTransform * bt,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstGLBaseFilter *base_filter = GST_GL_BASE_FILTER (bt);
  GstGLUploadElement *upload = GST_GL_UPLOAD_ELEMENT (bt);
  GstGLContext *context;
  GstGLUpload *ul = NULL;
  GstCaps *ret_caps;

  if (base_filter->display && !gst_gl_base_filter_find_gl_context (base_filter))
    return NULL;

  context = gst_gl_base_filter_get_gl_context (base_filter);

  GST_OBJECT_LOCK (upload);
  if (upload->upload == NULL) {
    GST_OBJECT_UNLOCK (upload);

    ul = gst_gl_upload_new (context);

    GST_OBJECT_LOCK (upload);
    if (upload->upload) {
      gst_object_unref (ul);
      ul = upload->upload;
    } else {
      upload->upload = ul;
    }
  } else {
    ul = upload->upload;
  }

  gst_object_ref (ul);
  GST_OBJECT_UNLOCK (upload);

  ret_caps =
      gst_gl_upload_transform_caps (ul, context, direction, caps, filter);

  gst_object_unref (ul);
  if (context)
    gst_object_unref (context);

  return ret_caps;
}

static gboolean
gst_gl_upload_element_filter_meta (GstBaseTransform * trans, GstQuery * query,
    GType api, const GstStructure * params)
{
  /* propose all metadata upstream */
  return TRUE;
}

static gboolean
_gst_gl_upload_element_propose_allocation (GstBaseTransform * bt,
    GstQuery * decide_query, GstQuery * query)
{
  GstGLUploadElement *upload = GST_GL_UPLOAD_ELEMENT (bt);
  GstGLUpload *ul;
  GstGLContext *context;
  gboolean ret;

  GST_OBJECT_LOCK (upload);
  if (!upload->upload) {
    GST_OBJECT_UNLOCK (upload);
    return FALSE;
  }
  ul = gst_object_ref (upload->upload);
  GST_OBJECT_UNLOCK (upload);

  context = gst_gl_base_filter_get_gl_context (GST_GL_BASE_FILTER (bt));
  if (!context) {
    gst_object_unref (ul);
    return FALSE;
  }

  gst_gl_upload_set_context (ul, context);

  ret = GST_BASE_TRANSFORM_CLASS (parent_class)->propose_allocation (bt,
      decide_query, query);
  gst_gl_upload_propose_allocation (ul, decide_query, query);

  gst_object_unref (ul);
  gst_object_unref (context);

  return ret;
}

static gboolean
_gst_gl_upload_element_decide_allocation (GstBaseTransform * trans,
    GstQuery * query)
{
  GstGLUploadElement *upload = GST_GL_UPLOAD_ELEMENT (trans);
  GstGLContext *context = GST_GL_BASE_FILTER (trans)->context;

  if (upload->upload && context)
    gst_gl_upload_set_context (upload->upload, context);

  return
      GST_BASE_TRANSFORM_CLASS
      (gst_gl_upload_element_parent_class)->decide_allocation (trans, query);
}

static gboolean
_gst_gl_upload_element_set_caps (GstBaseTransform * bt, GstCaps * in_caps,
    GstCaps * out_caps)
{
  GstGLUploadElement *upload = GST_GL_UPLOAD_ELEMENT (bt);

  return gst_gl_upload_set_caps (upload->upload, in_caps, out_caps);
}

GstFlowReturn
gst_gl_upload_element_prepare_output_buffer (GstBaseTransform * bt,
    GstBuffer * buffer, GstBuffer ** outbuf)
{
  GstGLUploadElement *upload = GST_GL_UPLOAD_ELEMENT (bt);
  GstGLUploadReturn ret;
  GstBaseTransformClass *bclass;

  bclass = GST_BASE_TRANSFORM_GET_CLASS (bt);

  if (gst_base_transform_is_passthrough (bt)) {
    *outbuf = buffer;
    return GST_FLOW_OK;
  }

  if (!upload->upload)
    return GST_FLOW_NOT_NEGOTIATED;

again:
  ret = gst_gl_upload_perform_with_buffer (upload->upload, buffer, outbuf);
  if (ret == GST_GL_UPLOAD_RECONFIGURE) {
    GstPad *sinkpad = GST_BASE_TRANSFORM_SINK_PAD (bt);
    GstCaps *incaps = gst_pad_get_current_caps (sinkpad);
    GST_DEBUG_OBJECT (bt,
        "Failed to upload with curren caps -- reconfiguring.");
    /* Note: gst_base_transform_reconfigure_src() cannot be used here.
     * Reconfiguring must be synchronous to avoid dropping the current
     * buffer */
    gst_pad_send_event (sinkpad, gst_event_new_caps (incaps));
    gst_caps_unref (incaps);
    if (!gst_pad_needs_reconfigure (GST_BASE_TRANSFORM_SRC_PAD (bt))) {
      GST_DEBUG_OBJECT (bt, "Retry uploading with new caps");
      goto again;
    }
    return GST_FLOW_OK;
  }

  if (ret != GST_GL_UPLOAD_DONE || *outbuf == NULL) {
    GST_ELEMENT_ERROR (bt, RESOURCE, NOT_FOUND, ("%s",
            "Failed to upload buffer"), (NULL));
    if (*outbuf)
      gst_buffer_unref (*outbuf);
    return GST_FLOW_ERROR;
  }

  /* basetransform doesn't unref if they're the same */
  if (buffer == *outbuf)
    gst_buffer_unref (*outbuf);
  else
    bclass->copy_metadata (bt, buffer, *outbuf);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_gl_upload_element_transform (GstBaseTransform * bt, GstBuffer * buffer,
    GstBuffer * outbuf)
{
  return GST_FLOW_OK;
}

static GstCaps *
gst_gl_upload_element_fixate_caps (GstBaseTransform * bt,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
  GstGLUploadElement *upload = GST_GL_UPLOAD_ELEMENT (bt);

  return gst_gl_upload_fixate_caps (upload->upload, direction, caps, othercaps);
}

static GstStateChangeReturn
gst_gl_upload_element_change_state (GstElement * element,
    GstStateChange transition)
{
  GstGLUploadElement *upload = GST_GL_UPLOAD_ELEMENT (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  GST_DEBUG_OBJECT (upload, "changing state: %s => %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:
      _gst_gl_upload_element_clear_upload (upload);
      break;
    default:
      break;
  }

  return ret;
}
