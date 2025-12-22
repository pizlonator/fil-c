/*
 * GStreamer
 * Copyright (C) 2022 Martin Reboredo <yakoyoku@gmail.com>
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
 * SECTION:element-vulkanshaderspv
 * @title: vulkanshaderspv
 *
 * Vulkan image shader filter.
 *
 * ## Examples
 * ```
 * gst-launch-1.0 videotestsrc ! vulkanupload ! vulkanshaderspv fragment-location="myshader.f.spv" ! vulkansink
 * ```
 * The following is a simple Vulkan passthrough shader with the required inputs.
 * Compile it with `glslc --target-env=vulkan1.0 myshader.frag -o myshader.f.spv`.
 * ``` glsl
 * #version 450
 *
 * layout(location = 0) in vec2 inTexCoord;
 *
 * layout(set = 0, binding = 0) uniform ShaderFilter {
 *   float time;
 *   float width;
 *   float height;
 * };
 * layout(set = 0, binding = 1) uniform sampler2D inTexture;
 *
 * layout(location = 0) out vec4 outColor;
 *
 * void main () {
 *   outColor = texture (inTexture, inTexCoord);
 * }
 * ```
 *
 * Since: 1.22
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <gio/gio.h>

#include "gstvulkanelements.h"
#include "vkshaderspv.h"

#include "shaders/identity.vert.h"
#include "shaders/identity.frag.h"

GST_DEBUG_CATEGORY (gst_debug_vulkan_shader_spv);
#define GST_CAT_DEFAULT gst_debug_vulkan_shader_spv

static void gst_vulkan_shader_spv_finalize (GObject * object);
static void gst_vulkan_shader_spv_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_vulkan_shader_spv_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_vulkan_shader_spv_start (GstBaseTransform * bt);
static gboolean gst_vulkan_shader_spv_stop (GstBaseTransform * bt);

static GstFlowReturn gst_vulkan_shader_spv_transform (GstBaseTransform * bt,
    GstBuffer * inbuf, GstBuffer * outbuf);
static gboolean gst_vulkan_shader_spv_set_caps (GstBaseTransform * bt,
    GstCaps * in_caps, GstCaps * out_caps);

#define IMAGE_FORMATS " { BGRA }"

static GstStaticPadTemplate gst_vulkan_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_VULKAN_IMAGE,
            IMAGE_FORMATS)));

static GstStaticPadTemplate gst_vulkan_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_VULKAN_IMAGE,
            IMAGE_FORMATS)));

enum
{
  PROP_0,
  PROP_VERTEX,
  PROP_FRAGMENT,
  PROP_VERTEX_PATH,
  PROP_FRAGMENT_PATH,
};

enum
{
  SIGNAL_0,
  LAST_SIGNAL
};

/* static guint gst_vulkan_shader_spv_signals[LAST_SIGNAL] = { 0 }; */

#define gst_vulkan_shader_spv_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstVulkanShaderSpv, gst_vulkan_shader_spv,
    GST_TYPE_VULKAN_VIDEO_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_debug_vulkan_shader_spv,
        "vulkanshaderspv", 0, "Vulkan Image identity"));
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (vulkanshaderspv,
    "vulkanshaderspv", GST_RANK_NONE, GST_TYPE_VULKAN_SHADER_SPV,
    vulkan_element_init (plugin));

static void
gst_vulkan_shader_spv_class_init (GstVulkanShaderSpvClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseTransformClass *gstbasetransform_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  gstbasetransform_class = GST_BASE_TRANSFORM_CLASS (klass);

  gobject_class->finalize = gst_vulkan_shader_spv_finalize;
  gobject_class->set_property = gst_vulkan_shader_spv_set_property;
  gobject_class->get_property = gst_vulkan_shader_spv_get_property;

  g_object_class_install_property (gobject_class, PROP_VERTEX,
      g_param_spec_boxed ("vertex", "Vertex Binary",
          "SPIRV vertex binary", G_TYPE_BYTES,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_FRAGMENT,
      g_param_spec_boxed ("fragment", "Fragment Binary",
          "SPIRV fragment binary", G_TYPE_BYTES,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_VERTEX_PATH,
      g_param_spec_string ("vertex-location", "Vertex Source",
          "SPIRV vertex source", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_FRAGMENT_PATH,
      g_param_spec_string ("fragment-location", "Fragment Source",
          "SPIRV fragment source", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_metadata (gstelement_class, "Vulkan Shader SPV",
      "Filter/Video", "Performs operations with SPIRV shaders in Vulkan",
      "Martin Reboredo <yakoyoku@gmail.com>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_vulkan_sink_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_vulkan_src_template);

  gstbasetransform_class->start =
      GST_DEBUG_FUNCPTR (gst_vulkan_shader_spv_start);
  gstbasetransform_class->stop = GST_DEBUG_FUNCPTR (gst_vulkan_shader_spv_stop);
  gstbasetransform_class->set_caps = gst_vulkan_shader_spv_set_caps;
  gstbasetransform_class->transform = gst_vulkan_shader_spv_transform;
}

static void
gst_vulkan_shader_spv_init (GstVulkanShaderSpv * vk_shader)
{
  vk_shader->vert = g_bytes_new (NULL, 0);
  vk_shader->frag = g_bytes_new (NULL, 0);
}

static void
gst_vulkan_shader_spv_finalize (GObject * object)
{
  GstVulkanShaderSpv *filter = GST_VULKAN_SHADER_SPV (object);

  g_bytes_unref (filter->vert);
  filter->vert = NULL;

  g_bytes_unref (filter->frag);
  filter->frag = NULL;

  g_free (filter->vert_path);
  filter->vert_path = NULL;

  g_free (filter->frag_path);
  filter->frag_path = NULL;

  if (filter->uniforms)
    gst_memory_unref (filter->uniforms);
  filter->uniforms = NULL;

  G_OBJECT_CLASS (gst_vulkan_shader_spv_parent_class)->finalize (object);
}

#define SPIRV_MAGIC_NUMBER_NE 0x07230203
#define SPIRV_MAGIC_NUMBER_OE 0x03022307

static GBytes *
gst_vulkan_shader_spv_check_shader_binary (const GValue * value)
{
  GBytes *bytes = NULL;
  gsize len;
  const gchar *data;
  gint32 first_word;

  bytes = g_value_dup_boxed (value);
  if (!bytes)
    return NULL;
  data = g_bytes_get_data (bytes, &len);
  if (len == 0 || len & 0x03) {
    g_bytes_unref (bytes);
    return NULL;
  }
  first_word = data[0] | data[1] << 8 | data[2] << 16 | data[3] << 24;
  if (first_word != SPIRV_MAGIC_NUMBER_NE &&
      first_word != SPIRV_MAGIC_NUMBER_OE) {
    g_bytes_unref (bytes);
    return NULL;
  }
  return bytes;
}

static void
gst_vulkan_shader_spv_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVulkanShaderSpv *filter = GST_VULKAN_SHADER_SPV (object);
  GBytes *bytes = NULL;

  switch (prop_id) {
    case PROP_VERTEX:
      GST_OBJECT_LOCK (filter);
      if (!(bytes = gst_vulkan_shader_spv_check_shader_binary (value)))
        goto wrong_format;
      g_bytes_unref (filter->vert);
      filter->vert = bytes;
      GST_OBJECT_UNLOCK (filter);
      break;
    case PROP_FRAGMENT:
      GST_OBJECT_LOCK (filter);
      if (!(bytes = gst_vulkan_shader_spv_check_shader_binary (value)))
        goto wrong_format;
      g_bytes_unref (filter->frag);
      filter->frag = bytes;
      GST_OBJECT_UNLOCK (filter);
      break;
    case PROP_VERTEX_PATH:
      GST_OBJECT_LOCK (filter);
      g_free (filter->vert_path);
      filter->vert_path = g_value_dup_string (value);
      GST_OBJECT_UNLOCK (filter);
      break;
    case PROP_FRAGMENT_PATH:
      GST_OBJECT_LOCK (filter);
      g_free (filter->frag_path);
      filter->frag_path = g_value_dup_string (value);
      GST_OBJECT_UNLOCK (filter);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  return;

wrong_format:
  {
    g_critical ("Badly formatted byte sequence, must have a nonzero length"
        " that is a multiple of four and start with the SPIRV magic number");
    GST_OBJECT_UNLOCK (filter);
    return;
  }
}

static void
gst_vulkan_shader_spv_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstVulkanShaderSpv *filter = GST_VULKAN_SHADER_SPV (object);

  switch (prop_id) {
    case PROP_VERTEX:
      GST_OBJECT_LOCK (filter);
      g_value_set_boxed (value, filter->vert);
      GST_OBJECT_UNLOCK (filter);
      break;
    case PROP_FRAGMENT:
      GST_OBJECT_LOCK (filter);
      g_value_set_boxed (value, filter->frag);
      GST_OBJECT_UNLOCK (filter);
      break;
    case PROP_VERTEX_PATH:
      GST_OBJECT_LOCK (filter);
      g_value_set_string (value, filter->vert_path);
      GST_OBJECT_UNLOCK (filter);
      break;
    case PROP_FRAGMENT_PATH:
      GST_OBJECT_LOCK (filter);
      g_value_set_string (value, filter->frag_path);
      GST_OBJECT_UNLOCK (filter);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_vulkan_shader_spv_set_caps (GstBaseTransform * bt, GstCaps * in_caps,
    GstCaps * out_caps)
{
  GstVulkanVideoFilter *vfilter = GST_VULKAN_VIDEO_FILTER (bt);
  GstVulkanShaderSpv *vk_identity = GST_VULKAN_SHADER_SPV (bt);

  if (!GST_BASE_TRANSFORM_CLASS (parent_class)->set_caps (bt, in_caps,
          out_caps))
    return FALSE;

  if (!gst_vulkan_full_screen_quad_set_info (vk_identity->quad,
          &vfilter->in_info, &vfilter->out_info))
    return FALSE;

  return TRUE;
}

static GstVulkanHandle *
gst_vulkan_shader_spv_create_shader (GstVulkanShaderSpv * shader,
    GBytes * binary, const char *path, const gchar * identity,
    gsize identity_size, GError ** error)
{
  GstVulkanVideoFilter *vfilter = GST_VULKAN_VIDEO_FILTER (shader);
  const gchar *data;
  gsize len;
  GstVulkanHandle *handle;

  data = g_bytes_get_data (binary, &len);
  if (data) {
    if (!(handle =
            gst_vulkan_create_shader (vfilter->device, data, len, error)))
      return NULL;
  } else if (path) {
    GFile *file;
    GFileInfo *info;
    GFileInputStream *istream;
    GBytes *res;
    const gchar *data;
    gsize len = 35648;

    file = g_file_new_for_path (path);
    if (!(istream = g_file_read (file, NULL, error))) {
      g_object_unref (file);
      return NULL;
    }
    if ((info =
            g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_SIZE,
                G_FILE_QUERY_INFO_NONE, NULL, NULL))) {
      len = g_file_info_get_size (info);
      g_object_unref (info);
    }
    if (!(res =
            g_input_stream_read_bytes (G_INPUT_STREAM (istream), len, NULL,
                error))) {
      g_input_stream_close (G_INPUT_STREAM (istream), NULL, NULL);
      g_object_unref (file);
      return NULL;
    }
    data = g_bytes_get_data (res, &len);
    if (!(handle =
            gst_vulkan_create_shader (vfilter->device, data, len, error))) {
      g_bytes_unref (res);
      g_input_stream_close (G_INPUT_STREAM (istream), NULL, NULL);
      g_object_unref (file);
      return NULL;
    }
    g_bytes_unref (res);
    g_input_stream_close (G_INPUT_STREAM (istream), NULL, NULL);
    g_object_unref (file);
  } else {
    if (!(handle = gst_vulkan_create_shader (vfilter->device, identity,
                identity_size, error)))
      return NULL;
  }

  return handle;
}

static gboolean
gst_vulkan_shader_spv_start (GstBaseTransform * bt)
{
  GstVulkanShaderSpv *vk_shader = GST_VULKAN_SHADER_SPV (bt);
  GstVulkanVideoFilter *vfilter = GST_VULKAN_VIDEO_FILTER (vk_shader);
  GstVulkanHandle *vert, *frag;
  GError *error = NULL;

  if (!GST_BASE_TRANSFORM_CLASS (parent_class)->start (bt))
    return FALSE;

  GST_OBJECT_LOCK (vfilter);

  vk_shader->quad = gst_vulkan_full_screen_quad_new (vfilter->queue);

  if (!(vert = gst_vulkan_shader_spv_create_shader (vk_shader, vk_shader->vert,
              vk_shader->vert_path, identity_vert, identity_vert_size,
              &error))) {
    goto error;
  }

  if (!(frag = gst_vulkan_shader_spv_create_shader (vk_shader, vk_shader->frag,
              vk_shader->frag_path, identity_frag, identity_frag_size,
              &error))) {
    gst_vulkan_handle_unref (vert);
    goto error;
  }

  if (!gst_vulkan_full_screen_quad_set_shaders (vk_shader->quad, vert, frag)) {
    gst_vulkan_handle_unref (vert);
    gst_vulkan_handle_unref (frag);
    g_set_error (&error, GST_VULKAN_WINDOW_ERROR, FALSE,
        "Failed to set shaders in full screen quad");
    goto error;
  }

  gst_vulkan_handle_unref (vert);
  gst_vulkan_handle_unref (frag);

  GST_OBJECT_UNLOCK (vfilter);

  return TRUE;

error:
  GST_OBJECT_UNLOCK (vfilter);
  if (error->domain == GST_VULKAN_ERROR) {
    GST_ELEMENT_ERROR (bt, RESOURCE, NOT_FOUND, ("Failed to create shader: %s",
            gst_vulkan_result_to_string (error->code)), (NULL));
    GST_DEBUG ("%s", error->message);
  } else {
    GST_ELEMENT_ERROR (bt, RESOURCE, NOT_FOUND, ("Failed to create shader: %s",
            error->message), (NULL));
  }
  return FALSE;
}

static gboolean
gst_vulkan_shader_spv_stop (GstBaseTransform * bt)
{
  GstVulkanShaderSpv *vk_shader = GST_VULKAN_SHADER_SPV (bt);

  gst_clear_object (&vk_shader->quad);

  return GST_BASE_TRANSFORM_CLASS (parent_class)->stop (bt);
}

struct ShaderUpdateData
{
  float time;
  float width;
  float height;
};

static inline gboolean
_gst_clock_time_to_double (GstClockTime time, gint64 * result)
{
  if (!GST_CLOCK_TIME_IS_VALID (time))
    return FALSE;

  *result = time;

  return TRUE;
}

static inline gboolean
_gint64_time_val_to_double (gint64 time, gint64 * result)
{
  if (time == -1)
    return FALSE;

  *result = time / GST_SECOND;

  return TRUE;
}

static gboolean
shader_spv_update_time (GstVulkanShaderSpv * shader_spv, GstBuffer * inbuf)
{
  GstMapInfo map_info;
  gint64 time = 0;

  if (!_gst_clock_time_to_double (GST_BUFFER_PTS (inbuf), &time)) {
    if (!_gst_clock_time_to_double (GST_BUFFER_DTS (inbuf), &time))
      _gint64_time_val_to_double (g_get_monotonic_time (), &time);
  }

  if (!gst_memory_map (shader_spv->uniforms, &map_info, GST_MAP_WRITE))
    return FALSE;

  ((struct ShaderUpdateData *) map_info.data)->time = (float) time / GST_SECOND;
  gst_memory_unmap (shader_spv->uniforms, &map_info);

  return TRUE;
}

static GstMemory *
shader_spv_create_uniform (GstVulkanShaderSpv * shader_spv)
{
  GstVulkanVideoFilter *vfilter = GST_VULKAN_VIDEO_FILTER (shader_spv);

  if (shader_spv->uniforms) {
    return shader_spv->uniforms;
  } else {
    struct ShaderUpdateData data = { 0.0f,
      GST_VIDEO_INFO_WIDTH (&shader_spv->quad->in_info),
      GST_VIDEO_INFO_HEIGHT (&shader_spv->quad->in_info),
    };
    GstMapInfo map_info;
    GstMemory *uniforms;

    uniforms =
        gst_vulkan_buffer_memory_alloc (vfilter->device,
        sizeof (struct ShaderUpdateData),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (!gst_memory_map (uniforms, &map_info, GST_MAP_WRITE))
      return NULL;

    memcpy (map_info.data, &data, sizeof (data));
    gst_memory_unmap (uniforms, &map_info);

    shader_spv->uniforms = uniforms;
    return uniforms;
  }
}

static GstFlowReturn
gst_vulkan_shader_spv_transform (GstBaseTransform * bt, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstVulkanShaderSpv *vk_shader = GST_VULKAN_SHADER_SPV (bt);
  GError *error = NULL;
  GstMemory *uniforms;

  if (!gst_vulkan_full_screen_quad_set_input_buffer (vk_shader->quad, inbuf,
          &error))
    goto error;
  if (!gst_vulkan_full_screen_quad_set_output_buffer (vk_shader->quad, outbuf,
          &error))
    goto error;

  if (!(uniforms = shader_spv_create_uniform (vk_shader)))
    goto error;

  shader_spv_update_time (vk_shader, inbuf);
  if (!gst_vulkan_full_screen_quad_set_uniform_buffer (vk_shader->quad,
          uniforms, &error))
    goto error;

  if (!gst_vulkan_full_screen_quad_draw (vk_shader->quad, &error))
    goto error;

  return GST_FLOW_OK;

error:
  if (error->domain == GST_VULKAN_ERROR) {
    GST_ELEMENT_ERROR (bt, LIBRARY, FAILED, ("Failed to apply shader: %s",
            gst_vulkan_result_to_string (error->code)), (NULL));
    GST_DEBUG ("%s", error->message);
  } else {
    GST_ELEMENT_ERROR (bt, LIBRARY, FAILED, ("Failed to apply shader: %s",
            error->message), (NULL));
  }
  g_clear_error (&error);
  return GST_FLOW_ERROR;
}
