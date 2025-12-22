/*
 * GStreamer gstreamer-onnxinference
 * Copyright (C) 2023 Collabora Ltd.
 *
 * gstonnxinference.cpp
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
 * SECTION:element-onnxinference
 * @short_description: Run ONNX inference model on video buffers
 *
 * This element can apply an ONNX model to video buffers. It attaches
 * the tensor output to the buffer as a @ref GstTensorMeta.
 *
 * To install ONNX on your system, follow the instructions in the
 * README.md in with this plugin.
 *
 * ## Example launch command:
 *
 * Test image file, model file (SSD) and label file can be found here :
 * https://gitlab.collabora.com/gstreamer/onnx-models
 *
 * GST_DEBUG=ssdobjectdetector:5 \
 * gst-launch-1.0 filesrc location=onnx-models/images/bus.jpg ! \
 * jpegdec ! videoconvert ! onnxinference execution-provider=cpu model-file=onnx-models/models/ssd_mobilenet_v1_coco.onnx !  \
 * ssdobjectdetector label-file=onnx-models/labels/COCO_classes.txt  ! videoconvert ! imagefreeze ! autovideosink
 *
 *
 * Note: in order for downstream tensor decoders to correctly parse the tensor
 * data in the GstTensorMeta, meta data must be attached to the ONNX model
 * assigning a unique string id to each output layer. These unique string ids
 * and corresponding GQuark ids are currently stored in the tensor decoder's
 * header file, in this case gstssdobjectdetector.h. If the meta data is absent,
 * the pipeline will fail.
 *
 * As a convenience, there is a python script
 * currently stored at
 * https://gitlab.collabora.com/gstreamer/onnx-models/-/blob/master/scripts/modify_onnx_metadata.py
 * to enable users to easily add and remove meta data from json files. It can also dump
 * the names of all output layers, which can then be used to craft the json meta data file.
 *
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include "gstonnxinference.h"
#include "gstonnxclient.h"


/*
 * GstOnnxInference:
 *
 * @model_file model file
 * @optimization_level: ONNX session optimization level
 * @execution_provider: ONNX execution provider
 * @onnx_client opaque pointer to ONNX client
 * @onnx_disabled true if inference is disabled
 * @video_info @ref GstVideoInfo of sink caps
 */
struct _GstOnnxInference
{
  GstBaseTransform basetransform;
  gchar *model_file;
  GstOnnxOptimizationLevel optimization_level;
  GstOnnxExecutionProvider execution_provider;
  gpointer onnx_client;
  gboolean onnx_disabled;
  GstVideoInfo video_info;
};

GST_DEBUG_CATEGORY (onnx_inference_debug);

#define GST_CAT_DEFAULT onnx_inference_debug
#define GST_ONNX_CLIENT_MEMBER( self ) ((GstOnnxNamespace::GstOnnxClient *) (self->onnx_client))
GST_ELEMENT_REGISTER_DEFINE (onnx_inference, "onnxinference",
    GST_RANK_PRIMARY, GST_TYPE_ONNX_INFERENCE);

/* GstOnnxInference properties */
enum
{
  PROP_0,
  PROP_MODEL_FILE,
  PROP_INPUT_IMAGE_FORMAT,
  PROP_OPTIMIZATION_LEVEL,
  PROP_EXECUTION_PROVIDER,
  PROP_INPUT_OFFSET,
  PROP_INPUT_SCALE
};

#define GST_ONNX_INFERENCE_DEFAULT_EXECUTION_PROVIDER    GST_ONNX_EXECUTION_PROVIDER_CPU
#define GST_ONNX_INFERENCE_DEFAULT_OPTIMIZATION_LEVEL    GST_ONNX_OPTIMIZATION_LEVEL_ENABLE_EXTENDED

static GstStaticPadTemplate gst_onnx_inference_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ RGB,RGBA,BGR,BGRA }"))
    );

static GstStaticPadTemplate gst_onnx_inference_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ RGB,RGBA,BGR,BGRA }"))
    );

static void gst_onnx_inference_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_onnx_inference_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_onnx_inference_finalize (GObject * object);
static GstFlowReturn gst_onnx_inference_transform_ip (GstBaseTransform *
    trans, GstBuffer * buf);
static gboolean gst_onnx_inference_process (GstBaseTransform * trans,
    GstBuffer * buf);
static gboolean gst_onnx_inference_create_session (GstBaseTransform * trans);
static GstCaps *gst_onnx_inference_transform_caps (GstBaseTransform *
    trans, GstPadDirection direction, GstCaps * caps, GstCaps * filter_caps);
static gboolean
gst_onnx_inference_set_caps (GstBaseTransform * trans, GstCaps * incaps,
    GstCaps * outcaps);

G_DEFINE_TYPE (GstOnnxInference, gst_onnx_inference, GST_TYPE_BASE_TRANSFORM);

GType gst_onnx_optimization_level_get_type (void);
#define GST_TYPE_ONNX_OPTIMIZATION_LEVEL (gst_onnx_optimization_level_get_type ())

GType gst_onnx_execution_provider_get_type (void);
#define GST_TYPE_ONNX_EXECUTION_PROVIDER (gst_onnx_execution_provider_get_type ())

GType gst_ml_model_input_image_format_get_type (void);
#define GST_TYPE_ML_MODEL_INPUT_IMAGE_FORMAT (gst_ml_model_input_image_format_get_type ())

GType
gst_onnx_optimization_level_get_type (void)
{
  static GType onnx_optimization_type = 0;

  if (g_once_init_enter (&onnx_optimization_type)) {
    static GEnumValue optimization_level_types[] = {
      {GST_ONNX_OPTIMIZATION_LEVEL_DISABLE_ALL, "Disable all optimization",
          "disable-all"},
      {GST_ONNX_OPTIMIZATION_LEVEL_ENABLE_BASIC,
            "Enable basic optimizations (redundant node removals))",
          "enable-basic"},
      {GST_ONNX_OPTIMIZATION_LEVEL_ENABLE_EXTENDED,
            "Enable extended optimizations (redundant node removals + node fusions)",
          "enable-extended"},
      {GST_ONNX_OPTIMIZATION_LEVEL_ENABLE_ALL,
          "Enable all possible optimizations", "enable-all"},
      {0, NULL, NULL},
    };

    GType temp = g_enum_register_static ("GstOnnxOptimizationLevel",
        optimization_level_types);

    g_once_init_leave (&onnx_optimization_type, temp);
  }

  return onnx_optimization_type;
}

GType
gst_onnx_execution_provider_get_type (void)
{
  static GType onnx_execution_type = 0;

  if (g_once_init_enter (&onnx_execution_type)) {
    static GEnumValue execution_provider_types[] = {
      {GST_ONNX_EXECUTION_PROVIDER_CPU, "CPU execution provider",
          "cpu"},
      {GST_ONNX_EXECUTION_PROVIDER_CUDA,
            "CUDA execution provider",
          "cuda"},
      {0, NULL, NULL},
    };

    GType temp = g_enum_register_static ("GstOnnxExecutionProvider",
        execution_provider_types);

    g_once_init_leave (&onnx_execution_type, temp);
  }

  return onnx_execution_type;
}

GType
gst_ml_model_input_image_format_get_type (void)
{
  static GType ml_model_input_image_format = 0;

  if (g_once_init_enter (&ml_model_input_image_format)) {
    static GEnumValue ml_model_input_image_format_types[] = {
      {GST_ML_INPUT_IMAGE_FORMAT_HWC,
            "Height Width Channel (HWC) a.k.a. interleaved image data format",
          "hwc"},
      {GST_ML_INPUT_IMAGE_FORMAT_CHW,
            "Channel Height Width (CHW) a.k.a. planar image data format",
          "chw"},
      {0, NULL, NULL},
    };

    GType temp = g_enum_register_static ("GstMlInputImageFormat",
        ml_model_input_image_format_types);

    g_once_init_leave (&ml_model_input_image_format, temp);
  }

  return ml_model_input_image_format;
}

static void
gst_onnx_inference_class_init (GstOnnxInferenceClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *element_class = (GstElementClass *) klass;
  GstBaseTransformClass *basetransform_class = (GstBaseTransformClass *) klass;

  GST_DEBUG_CATEGORY_INIT (onnx_inference_debug, "onnxinference",
      0, "onnx_inference");
  gobject_class->set_property = gst_onnx_inference_set_property;
  gobject_class->get_property = gst_onnx_inference_get_property;
  gobject_class->finalize = gst_onnx_inference_finalize;

  /**
   * GstOnnxInference:model-file
   *
   * ONNX model file
   *
   * Since: 1.24
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_MODEL_FILE,
      g_param_spec_string ("model-file",
          "ONNX model file", "ONNX model file", NULL, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstOnnxInference:input-image-format
   *
   * Model input image format
   *
   * Since: 1.24
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_INPUT_IMAGE_FORMAT,
      g_param_spec_enum ("input-image-format",
          "Input image format",
          "Input image format",
          GST_TYPE_ML_MODEL_INPUT_IMAGE_FORMAT,
          GST_ML_INPUT_IMAGE_FORMAT_HWC, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

   /**
    * GstOnnxInference:optimization-level
    *
    * ONNX optimization level
    *
    * Since: 1.24
    */
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_OPTIMIZATION_LEVEL,
      g_param_spec_enum ("optimization-level",
          "Optimization level",
          "ONNX optimization level",
          GST_TYPE_ONNX_OPTIMIZATION_LEVEL,
          GST_ONNX_OPTIMIZATION_LEVEL_ENABLE_EXTENDED, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstOnnxInference:execution-provider
   *
   * ONNX execution provider
   *
   * Since: 1.24
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_EXECUTION_PROVIDER,
      g_param_spec_enum ("execution-provider",
          "Execution provider",
          "ONNX execution provider",
          GST_TYPE_ONNX_EXECUTION_PROVIDER,
          GST_ONNX_EXECUTION_PROVIDER_CPU, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_INPUT_OFFSET,
      g_param_spec_float ("input-tensor-offset",
          "Input tensor offset",
          "offset each tensor value by this value",
          -G_MAXFLOAT, G_MAXFLOAT, 0.0,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_INPUT_SCALE,
      g_param_spec_float ("input-tensor-scale",
          "Input tensor scale",
          "Divide each tensor value by this value",
          G_MINFLOAT, G_MAXFLOAT, 1.0,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));


  gst_element_class_set_static_metadata (element_class, "onnxinference",
      "Filter/Effect/Video",
      "Apply neural network to video frames and create tensor output",
      "Aaron Boxer <aaron.boxer@collabora.com>");
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_onnx_inference_sink_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_onnx_inference_src_template));
  basetransform_class->transform_ip =
      GST_DEBUG_FUNCPTR (gst_onnx_inference_transform_ip);
  basetransform_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_onnx_inference_transform_caps);
  basetransform_class->set_caps =
      GST_DEBUG_FUNCPTR (gst_onnx_inference_set_caps);
}

static void
gst_onnx_inference_init (GstOnnxInference * self)
{
  self->onnx_client = new GstOnnxNamespace::GstOnnxClient (GST_ELEMENT(self));
  self->onnx_disabled = TRUE;
}

static void
gst_onnx_inference_finalize (GObject * object)
{
  GstOnnxInference *self = GST_ONNX_INFERENCE (object);

  g_free (self->model_file);
  delete GST_ONNX_CLIENT_MEMBER (self);
  G_OBJECT_CLASS (gst_onnx_inference_parent_class)->finalize (object);
}

static void
gst_onnx_inference_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstOnnxInference *self = GST_ONNX_INFERENCE (object);
  const gchar *filename;
  auto onnxClient = GST_ONNX_CLIENT_MEMBER (self);

  switch (prop_id) {
    case PROP_MODEL_FILE:
      filename = g_value_get_string (value);
      if (filename
          && g_file_test (filename,
              (GFileTest) (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR))) {
        if (self->model_file)
          g_free (self->model_file);
        self->model_file = g_strdup (filename);
        self->onnx_disabled = FALSE;
      } else {
        GST_WARNING_OBJECT (self, "Model file '%s' not found!", filename);
      }
      break;
    case PROP_OPTIMIZATION_LEVEL:
      self->optimization_level =
          (GstOnnxOptimizationLevel) g_value_get_enum (value);
      break;
    case PROP_EXECUTION_PROVIDER:
      self->execution_provider =
          (GstOnnxExecutionProvider) g_value_get_enum (value);
      break;
    case PROP_INPUT_IMAGE_FORMAT:
      onnxClient->setInputImageFormat ((GstMlInputImageFormat)
          g_value_get_enum (value));
      break;
    case PROP_INPUT_OFFSET:
      onnxClient->setInputImageOffset (g_value_get_float (value));
      break;
    case PROP_INPUT_SCALE:
      onnxClient->setInputImageScale (g_value_get_float (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_onnx_inference_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstOnnxInference *self = GST_ONNX_INFERENCE (object);
  auto onnxClient = GST_ONNX_CLIENT_MEMBER (self);

  switch (prop_id) {
    case PROP_MODEL_FILE:
      g_value_set_string (value, self->model_file);
      break;
    case PROP_OPTIMIZATION_LEVEL:
      g_value_set_enum (value, self->optimization_level);
      break;
    case PROP_EXECUTION_PROVIDER:
      g_value_set_enum (value, self->execution_provider);
      break;
    case PROP_INPUT_IMAGE_FORMAT:
      g_value_set_enum (value, onnxClient->getInputImageFormat ());
      break;
    case PROP_INPUT_OFFSET:
      g_value_set_float (value, onnxClient->getInputImageOffset ());
      break;
    case PROP_INPUT_SCALE:
      g_value_set_float (value, onnxClient->getInputImageScale ());
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_onnx_inference_create_session (GstBaseTransform * trans)
{
  GstOnnxInference *self = GST_ONNX_INFERENCE (trans);
  auto onnxClient = GST_ONNX_CLIENT_MEMBER (self);

  GST_OBJECT_LOCK (self);
  if (self->onnx_disabled) {
    GST_OBJECT_UNLOCK (self);

    return FALSE;
  }
  if (onnxClient->hasSession ()) {
    GST_OBJECT_UNLOCK (self);

    return TRUE;
  }
  if (self->model_file) {
    gboolean ret =
        GST_ONNX_CLIENT_MEMBER (self)->createSession (self->model_file,
        self->optimization_level,
        self->execution_provider);
    if (!ret) {
      GST_ERROR_OBJECT (self,
          "Unable to create ONNX session. Model is disabled.");
      self->onnx_disabled = TRUE;
    }
  } else {
    self->onnx_disabled = TRUE;
    GST_ELEMENT_ERROR (self, STREAM, FAILED, (NULL), ("Model file not found"));
  }
  GST_OBJECT_UNLOCK (self);
  if (self->onnx_disabled) {
    gst_base_transform_set_passthrough (GST_BASE_TRANSFORM (self), TRUE);
  }

  return TRUE;
}

static GstCaps *
gst_onnx_inference_transform_caps (GstBaseTransform *
    trans, GstPadDirection direction, GstCaps * caps, GstCaps * filter_caps)
{
  GstOnnxInference *self = GST_ONNX_INFERENCE (trans);
  auto onnxClient = GST_ONNX_CLIENT_MEMBER (self);
  GstCaps *other_caps;
  GstCaps *restrictions;

  if (!gst_onnx_inference_create_session (trans))
    return NULL;
  GST_LOG_OBJECT (self, "transforming caps %" GST_PTR_FORMAT, caps);

  if (gst_base_transform_is_passthrough (trans))
    return gst_caps_ref (caps);

  restrictions = gst_caps_new_empty_simple ("video/x-raw");
  if (onnxClient->isFixedInputImageSize ())
    gst_caps_set_simple (restrictions, "width", G_TYPE_INT,
      onnxClient->getWidth (), "height", G_TYPE_INT,
      onnxClient->getHeight (), NULL);

  if (onnxClient->getInputImageDatatype() == GST_TENSOR_TYPE_UINT8 &&
      onnxClient->getInputImageScale() == 1.0 &&
      onnxClient->getInputImageOffset() == 0.0) {
    switch (onnxClient->getChannels()) {
      case 1:
        gst_caps_set_simple (restrictions, "format", G_TYPE_STRING, "GRAY8",
          NULL);
        break;
      case 3:
        switch (onnxClient->getInputImageFormat ()) {
        case GST_ML_INPUT_IMAGE_FORMAT_HWC:
          gst_caps_set_simple (restrictions, "format", G_TYPE_STRING, "RGB",
                               NULL);
          break;
        case GST_ML_INPUT_IMAGE_FORMAT_CHW:
          gst_caps_set_simple (restrictions, "format", G_TYPE_STRING, "RGBP",
                               NULL);
          break;
        }
        break;
      case 4:
        switch (onnxClient->getInputImageFormat ()) {
        case GST_ML_INPUT_IMAGE_FORMAT_HWC:
          gst_caps_set_simple (restrictions, "format", G_TYPE_STRING, "RGBA",
                               NULL);
          break;
        case GST_ML_INPUT_IMAGE_FORMAT_CHW:
          gst_caps_set_simple (restrictions, "format", G_TYPE_STRING, "RGBAP",
                               NULL);
          break;
        }
        break;
      default:
        GST_ERROR_OBJECT (self, "Invalid number of channels %d",
          onnxClient->getChannels());
        return NULL;
    }
  }

  GST_DEBUG_OBJECT(self, "Applying caps restrictions: %" GST_PTR_FORMAT,
    restrictions);

  other_caps = gst_caps_intersect_full (caps, restrictions,
                                        GST_CAPS_INTERSECT_FIRST);
  gst_caps_unref (restrictions);

  if (filter_caps) {
    GstCaps *tmp = gst_caps_intersect_full (
        other_caps, filter_caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_replace (&other_caps, tmp);
    gst_caps_unref (tmp);
  }

  return other_caps;
}

static gboolean
gst_onnx_inference_set_caps (GstBaseTransform * trans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstOnnxInference *self = GST_ONNX_INFERENCE (trans);
  auto onnxClient = GST_ONNX_CLIENT_MEMBER (self);

  if (!gst_video_info_from_caps (&self->video_info, incaps)) {
    GST_ERROR_OBJECT (self, "Failed to parse caps");
    return FALSE;
  }

  onnxClient->parseDimensions (self->video_info);
  return TRUE;
}

static GstFlowReturn
gst_onnx_inference_transform_ip (GstBaseTransform * trans, GstBuffer * buf)
{
  if (!gst_base_transform_is_passthrough (trans)
      && !gst_onnx_inference_process (trans, buf)) {
    GST_ELEMENT_ERROR (trans, STREAM, FAILED,
        (NULL), ("ONNX inference failed"));
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

static gboolean
gst_onnx_inference_process (GstBaseTransform * trans, GstBuffer * buf)
{
  GstMapInfo info;
  if (gst_buffer_map (buf, &info, GST_MAP_READ)) {
    GstOnnxInference *self = GST_ONNX_INFERENCE (trans);
    try {
      auto client = GST_ONNX_CLIENT_MEMBER (self);
      auto outputs = client->run (info.data, self->video_info);
      auto meta = client->copy_tensors_to_meta (outputs, buf);
      if (!meta)
        return FALSE;
      GST_TRACE_OBJECT (trans, "Num tensors:%d", meta->num_tensors);
      meta->batch_size = 1;
    }
    catch (Ort::Exception & ortex) {
      GST_ERROR_OBJECT (self, "%s", ortex.what ());
      gst_buffer_unmap (buf, &info);
      return FALSE;
    }

    gst_buffer_unmap (buf, &info);
  }

  return TRUE;
}
