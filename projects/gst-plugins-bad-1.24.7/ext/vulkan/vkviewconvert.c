/*
 * GStreamer
 * Copyright (C) 2019 Matthew Waters <matthew@centricular.com>
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
 * SECTION:element-vulkanimageidentity
 * @title: vulkanimgeidentity
 *
 * vulkanimageidentity produces a vulkan image that is a copy of the input image.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "gstvulkanelements.h"
#include "vkviewconvert.h"

#include "shaders/identity.vert.h"
#include "shaders/view_convert.frag.h"
#include "gstvulkan-plugins-enumtypes.h"

GST_DEBUG_CATEGORY (gst_debug_vulkan_view_convert);
#define GST_CAT_DEFAULT gst_debug_vulkan_view_convert

/* *INDENT-OFF* */
/* These match the order and number of DOWNMIX_ANAGLYPH_* modes */
static float downmix_matrices[][2][12] = {
  {                             /* Green-Magenta Dubois */
    {-0.062f, 0.284f, -0.015f, 0.0, -0.158f, 0.668f, -0.027f, 0.0, -0.039f, 0.143f, 0.021f, 0.0},
    {0.529f, -0.016f, 0.009f, 0.0, 0.705f, -0.015f, 0.075f, 0.0, 0.024f, -0.065f, 0.937f, 0.0}
  },
  {                             /* Red-Cyan Dubois */
    /* Source of this matrix: http://www.site.uottawa.ca/~edubois/anaglyph/LeastSquaresHowToPhotoshop.pdf */
    {0.437f, -0.062f, -0.048f, 0.0, 0.449f, -0.062f, -0.050f, 0.0, 0.164f, -0.024f, -0.017f},
    {-0.011f, 0.377f, -0.026f, 0.0, -0.032f, 0.761f, -0.093f, 0.0, -0.007f, 0.009f, 1.234f}
  },
  {                             /* Amber-blue Dubois */
    {1.062f, -0.026f, -0.038f, 0.0, -0.205f, 0.908f, -0.173f, 0.0, 0.299f, 0.068f, 0.022f},
    {-0.016f, 0.006f, 0.094f, 0.0, -0.123f, 0.062f, 0.185f, 0.0, -0.017f, -0.017f, 0.911f}
  }
};
/* *INDENT-ON* */

struct ViewUpdate
{
  int in_reorder_idx[4];
  int out_reorder_idx[4];
  float tex_offset[2][2];
  float tex_scale[2][2];
  int tex_size[2];
  int output_type;
  int _padding;
  float downmix[2][12];
};

static void
get_rgb_format_swizzle_order (GstVideoFormat format,
    gint swizzle[GST_VIDEO_MAX_COMPONENTS])
{
  const GstVideoFormatInfo *finfo = gst_video_format_get_info (format);
  int c_i = 0, i;

  g_return_if_fail (finfo->flags & GST_VIDEO_FORMAT_FLAG_RGB
      || format == GST_VIDEO_FORMAT_AYUV);

  for (i = 0; i < finfo->n_components; i++) {
    swizzle[c_i++] = finfo->poffset[i];
  }

  /* special case spaced RGB formats as the space does not contain a poffset
   * value and we need all four components to be valid in order to swizzle
   * correctly */
  if (format == GST_VIDEO_FORMAT_xRGB || format == GST_VIDEO_FORMAT_xBGR) {
    swizzle[c_i++] = 0;
  } else if (format == GST_VIDEO_FORMAT_RGBx || format == GST_VIDEO_FORMAT_BGRx) {
    swizzle[c_i++] = 3;
  } else {
    for (i = finfo->n_components; i < GST_VIDEO_MAX_COMPONENTS; i++) {
      swizzle[c_i++] = -1;
    }
  }
}

static void
get_vulkan_rgb_format_swizzle_order (VkFormat format, gint * swizzle,
    guint swizzle_count, guint offset)
{
  const GstVulkanFormatInfo *finfo = gst_vulkan_format_get_info (format);
  int i;

  g_return_if_fail (finfo->flags & GST_VULKAN_FORMAT_FLAG_RGB);
  g_return_if_fail (finfo->n_components <= swizzle_count);

  for (i = 0; i < finfo->n_components; i++) {
    swizzle[i] = offset + finfo->poffset[i];
  }
  for (i = finfo->n_components; i < swizzle_count; i++) {
    swizzle[i] = -1;
  }
}

/* given a swizzle index, produce an index such that:
 *
 * swizzle[idx[i]] == identity[i] where:
 * - swizzle is the original swizzle
 * - idx is the result
 * - identity = {0, 1, 2,...}
 * - unset fields are marked by -1
 */
static void
swizzle_identity_order (gint * swizzle, gint * idx)
{
  int i;

  for (i = 0; i < GST_VIDEO_MAX_COMPONENTS; i++) {
    idx[i] = -1;
  }

  for (i = 0; i < GST_VIDEO_MAX_COMPONENTS; i++) {
    if (swizzle[i] >= 0 && swizzle[i] < 4 && idx[swizzle[i]] == -1) {
      idx[swizzle[i]] = i;
    }
  }
}

static void
video_format_to_reorder (GstVideoFormat v_format, gint * reorder,
    gboolean input)
{
  switch (v_format) {
    case GST_VIDEO_FORMAT_RGBA:
    case GST_VIDEO_FORMAT_RGBx:
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_BGRx:
    case GST_VIDEO_FORMAT_ARGB:
    case GST_VIDEO_FORMAT_xRGB:
    case GST_VIDEO_FORMAT_ABGR:
    case GST_VIDEO_FORMAT_xBGR:
    case GST_VIDEO_FORMAT_AYUV:
      get_rgb_format_swizzle_order (v_format, reorder);
      break;
    case GST_VIDEO_FORMAT_UYVY:
      reorder[0] = 1;
      reorder[1] = 0;
      reorder[2] = input ? 3 : 2;
      reorder[3] = 0;
      break;
    case GST_VIDEO_FORMAT_YUY2:
      reorder[0] = 0;
      reorder[1] = 1;
      reorder[2] = 0;
      reorder[3] = input ? 3 : 2;
      break;
    case GST_VIDEO_FORMAT_NV12:
      reorder[0] = 0;
      reorder[1] = 1;
      reorder[2] = 2;
      reorder[3] = 0;
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  GST_TRACE ("swizzle: %u, %u, %u, %u", reorder[0], reorder[1], reorder[2],
      reorder[3]);
}

static guint
finfo_get_plane_n_components (const GstVideoFormatInfo * finfo, guint plane)
{
  guint n_components = 0, i;

  switch (finfo->format) {
    case GST_VIDEO_FORMAT_RGBx:
    case GST_VIDEO_FORMAT_xRGB:
    case GST_VIDEO_FORMAT_BGRx:
    case GST_VIDEO_FORMAT_xBGR:
      /* fixup spaced RGB formats as we treat the space as a normal alpha
       * component */
      return plane == 0 ? 4 : 0;
    default:
      break;
  }

  for (i = 0; i < finfo->n_components; i++) {
    if (finfo->plane[i] == plane)
      n_components++;
  }

  return n_components;
}

static void
get_vulkan_format_swizzle_order (GstVideoFormat v_format,
    VkFormat vk_format[GST_VIDEO_MAX_PLANES],
    gint swizzle[GST_VIDEO_MAX_COMPONENTS])
{
  const GstVideoFormatInfo *finfo;
  int i, prev_in_i = 0;

  finfo = gst_video_format_get_info (v_format);
  for (i = 0; i < finfo->n_planes; i++) {
    guint plane_components = finfo_get_plane_n_components (finfo, i);
    get_vulkan_rgb_format_swizzle_order (vk_format[i],
        &swizzle[prev_in_i], plane_components, prev_in_i);
    prev_in_i += plane_components;
  }

  if (v_format == GST_VIDEO_FORMAT_YUY2 || v_format == GST_VIDEO_FORMAT_UYVY) {
    /* Fixup these packed YUV formats as we use a two component format for
     * a 4-component pixel and access two samples in the shader */
    g_assert (swizzle[0] == 0);
    g_assert (swizzle[1] == 1);
    swizzle[2] = 2;
    swizzle[3] = 3;
  }

  GST_TRACE ("%s: %i, %i, %i, %i", finfo->name, swizzle[0], swizzle[1],
      swizzle[2], swizzle[3]);
}

static void
calculate_reorder_indexes (GstVideoFormat in_format,
    GstVulkanImageView * in_views[GST_VIDEO_MAX_COMPONENTS],
    GstVideoFormat out_format,
    GstVulkanImageView * out_views[GST_VIDEO_MAX_COMPONENTS],
    int ret_in[GST_VIDEO_MAX_COMPONENTS], int ret_out[GST_VIDEO_MAX_COMPONENTS])
{
  const GstVideoFormatInfo *in_finfo, *out_finfo;
  VkFormat in_vk_formats[GST_VIDEO_MAX_COMPONENTS];
  VkFormat out_vk_formats[GST_VIDEO_MAX_COMPONENTS];
  int in_vk_order[GST_VIDEO_MAX_COMPONENTS],
      in_reorder[GST_VIDEO_MAX_COMPONENTS];
  int out_vk_order[GST_VIDEO_MAX_COMPONENTS],
      out_reorder[GST_VIDEO_MAX_COMPONENTS];
  int tmp[GST_VIDEO_MAX_PLANES];
  int i;

  in_finfo = gst_video_format_get_info (in_format);
  out_finfo = gst_video_format_get_info (out_format);

  for (i = 0; i < in_finfo->n_planes; i++)
    in_vk_formats[i] = in_views[i]->image->create_info.format;
  for (i = 0; i < out_finfo->n_planes; i++)
    out_vk_formats[i] = out_views[i]->image->create_info.format;

  get_vulkan_format_swizzle_order (in_format, in_vk_formats, in_vk_order);
  video_format_to_reorder (in_format, in_reorder, TRUE);

  video_format_to_reorder (out_format, out_reorder, FALSE);
  get_vulkan_format_swizzle_order (out_format, out_vk_formats, out_vk_order);

  for (i = 0; i < GST_VIDEO_MAX_COMPONENTS; i++)
    tmp[i] = out_vk_order[out_reorder[i]];
  /* find the identity order for RGBA->$format */
  GST_TRACE ("pre-invert: %u, %u, %u, %u", tmp[0], tmp[1], tmp[2], tmp[3]);
  if (out_format == GST_VIDEO_FORMAT_YUY2
      || out_format == GST_VIDEO_FORMAT_UYVY) {
    for (i = 0; i < GST_VIDEO_MAX_COMPONENTS; i++)
      ret_out[i] = tmp[i];
  } else {
    swizzle_identity_order (tmp, ret_out);
  }

  for (i = 0; i < GST_VIDEO_MAX_COMPONENTS; i++)
    ret_in[i] = in_reorder[in_vk_order[i]];
  GST_TRACE ("in reorder: %u, %u, %u, %u", ret_in[0], ret_in[1], ret_in[2],
      ret_in[3]);
  GST_TRACE ("out reorder: %u, %u, %u, %u", ret_out[0], ret_out[1], ret_out[2],
      ret_out[3]);
}

static void
update_descriptor_set (GstVulkanViewConvert * conv,
    GstVulkanImageView ** in_views, guint n_mems)
{
  GstVulkanVideoFilter *vfilter = GST_VULKAN_VIDEO_FILTER (conv);
  VkDescriptorImageInfo image_info[GST_VIDEO_MAX_PLANES];
  VkWriteDescriptorSet writes[GST_VIDEO_MAX_PLANES];
  GstVideoMultiviewMode in_mode;
  GstVideoMultiviewFlags in_flags, out_flags;
  VkImageView views[GST_VIDEO_MAX_PLANES];
  guint i = 0;

  in_mode = conv->input_mode_override;
  in_flags = conv->input_flags_override;
  if (in_mode == GST_VIDEO_MULTIVIEW_MODE_NONE)
    in_flags = GST_VIDEO_INFO_MULTIVIEW_FLAGS (&vfilter->in_info);
  out_flags = GST_VIDEO_INFO_MULTIVIEW_FLAGS (&vfilter->out_info);

  for (i = 0; i < n_mems; i++) {
    if ((in_flags & GST_VIDEO_MULTIVIEW_FLAGS_RIGHT_VIEW_FIRST) ==
        (out_flags & GST_VIDEO_MULTIVIEW_FLAGS_RIGHT_VIEW_FIRST)) {
      views[2 * i] = in_views[i]->view;
      views[2 * i + 1] = in_views[i]->view;
    } else {
      views[2 * i] = in_views[i]->view;
      views[2 * i + 1] = in_views[i]->view;
    }
  }

  for (i = 0; i < n_mems * 2; i++) {
    /* *INDENT-OFF* */
    image_info[i] = (VkDescriptorImageInfo) {
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .imageView = views[i],
        .sampler = (VkSampler) conv->quad->sampler->handle
    };

    g_assert (i < GST_VIDEO_MAX_PLANES);

    writes[i] = (VkWriteDescriptorSet) {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = NULL,
        .dstSet = conv->quad->descriptor_set->set,
        .dstBinding = i+1,
        .dstArrayElement = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .pImageInfo = &image_info[i]
    };
    /* *INDENT-ON* */
  }
  g_assert (i <= G_N_ELEMENTS (writes));

  vkUpdateDescriptorSets (vfilter->device->device, i, writes, 0, NULL);
}

static gboolean
create_uniform_buffer (GstVulkanViewConvert * conv)
{
  GstVulkanVideoFilter *vfilter = GST_VULKAN_VIDEO_FILTER (conv);

  conv->uniform =
      gst_vulkan_buffer_memory_alloc (vfilter->device,
      sizeof (struct ViewUpdate),
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  return TRUE;
}

static gboolean
update_uniform (GstVulkanViewConvert * conv, GstVulkanImageView ** in_views,
    GstVulkanImageView ** out_views)
{
  GstVulkanVideoFilter *vfilter = GST_VULKAN_VIDEO_FILTER (conv);
  GstVideoMultiviewMode in_mode, out_mode;
  GstVideoMultiviewFlags in_flags, out_flags;
  struct ViewUpdate data;
  GstMapInfo map_info;
  guint l_index, r_index;
  gboolean mono_input = FALSE;

  calculate_reorder_indexes (GST_VIDEO_INFO_FORMAT (&vfilter->in_info),
      in_views, GST_VIDEO_INFO_FORMAT (&vfilter->out_info),
      out_views, data.in_reorder_idx, data.out_reorder_idx);

  data.tex_scale[0][0] = data.tex_scale[0][1] = 1.;
  data.tex_scale[1][0] = data.tex_scale[1][1] = 1.;
  data.tex_offset[0][0] = data.tex_offset[0][1] = 0.;
  data.tex_offset[1][0] = data.tex_offset[1][1] = 0.;

  in_mode = conv->input_mode_override;
  in_flags = conv->input_flags_override;
  if (in_mode == GST_VIDEO_MULTIVIEW_MODE_NONE) {
    in_mode = GST_VIDEO_INFO_MULTIVIEW_MODE (&vfilter->in_info);
    in_flags = GST_VIDEO_INFO_MULTIVIEW_FLAGS (&vfilter->in_info);
  }

  /* Configured output mode already takes any override
   * into account */
  out_mode = GST_VIDEO_INFO_MULTIVIEW_MODE (&vfilter->out_info);
  out_flags = GST_VIDEO_INFO_MULTIVIEW_FLAGS (&vfilter->out_info);

  if ((in_flags & GST_VIDEO_MULTIVIEW_FLAGS_RIGHT_VIEW_FIRST) ==
      (out_flags & GST_VIDEO_MULTIVIEW_FLAGS_RIGHT_VIEW_FIRST)) {
    l_index = 0;
    r_index = 1;
  } else {
    GST_LOG_OBJECT (conv, "Switching left/right views");

    l_index = 1;
    r_index = 0;
  }

  if (in_mode < GST_VIDEO_MULTIVIEW_MODE_SIDE_BY_SIDE) {        /* unknown/mono/left/right single image */
  } else if (in_mode == GST_VIDEO_MULTIVIEW_MODE_SIDE_BY_SIDE ||
      in_mode == GST_VIDEO_MULTIVIEW_MODE_SIDE_BY_SIDE_QUINCUNX) {
    /* Side-by-side input */
    data.tex_offset[r_index][0] += 0.5 * data.tex_scale[r_index][0];
    data.tex_scale[0][0] *= 0.5f;       /* Half horizontal scale */
    data.tex_scale[1][0] *= 0.5f;
  } else if (in_mode == GST_VIDEO_MULTIVIEW_MODE_TOP_BOTTOM) {  /* top-bottom */
    data.tex_offset[r_index][1] += 0.5 * data.tex_scale[r_index][1];
    data.tex_scale[0][1] *= 0.5f;       /* Half vertical scale */
    data.tex_scale[1][1] *= 0.5f;
  }

  /* Flipped is vertical, flopped is horizontal.
   * Adjust and offset per-view scaling. This needs to be done
   * after the input scaling already splits the views, before
   * adding any output scaling. */
  if ((in_flags & GST_VIDEO_MULTIVIEW_FLAGS_LEFT_FLIPPED) !=
      (out_flags & GST_VIDEO_MULTIVIEW_FLAGS_LEFT_FLIPPED)) {
    data.tex_offset[l_index][1] += data.tex_scale[l_index][1];
    data.tex_scale[l_index][1] *= -1.0;
  }
  if ((in_flags & GST_VIDEO_MULTIVIEW_FLAGS_LEFT_FLOPPED) !=
      (out_flags & GST_VIDEO_MULTIVIEW_FLAGS_LEFT_FLOPPED)) {
    data.tex_offset[l_index][0] += data.tex_scale[l_index][0];
    data.tex_scale[l_index][0] *= -1.0;
  }
  if ((in_flags & GST_VIDEO_MULTIVIEW_FLAGS_RIGHT_FLIPPED) !=
      (out_flags & GST_VIDEO_MULTIVIEW_FLAGS_RIGHT_FLIPPED)) {
    data.tex_offset[r_index][1] += data.tex_scale[r_index][1];
    data.tex_scale[r_index][1] *= -1.0;
  }
  if ((in_flags & GST_VIDEO_MULTIVIEW_FLAGS_RIGHT_FLOPPED) !=
      (out_flags & GST_VIDEO_MULTIVIEW_FLAGS_RIGHT_FLOPPED)) {
    data.tex_offset[r_index][0] += data.tex_scale[r_index][0];
    data.tex_scale[r_index][0] *= -1.0;
  }

  if (out_mode == GST_VIDEO_MULTIVIEW_MODE_SIDE_BY_SIDE ||
      out_mode == GST_VIDEO_MULTIVIEW_MODE_SIDE_BY_SIDE_QUINCUNX) {
    /* Side-by-Side */
    data.tex_offset[1][0] -= data.tex_scale[1][0];
    data.tex_scale[0][0] *= 2.0f;
    data.tex_scale[1][0] *= 2.0f;
  } else if (out_mode == GST_VIDEO_MULTIVIEW_MODE_TOP_BOTTOM) {
    data.tex_offset[1][1] -= data.tex_scale[1][1];
    data.tex_scale[0][1] *= 2.0f;
    data.tex_scale[1][1] *= 2.0f;
  }

  GST_DEBUG_OBJECT (conv,
      "Scaling matrix [ %f, %f ] [ %f %f]. Offsets [ %f, %f ] [ %f, %f ]",
      data.tex_scale[0][0], data.tex_scale[0][1],
      data.tex_scale[1][0], data.tex_scale[1][1],
      data.tex_offset[0][0], data.tex_offset[0][1], data.tex_offset[1][0],
      data.tex_offset[1][1]);

  if (in_mode == GST_VIDEO_MULTIVIEW_MODE_NONE ||
      in_mode == GST_VIDEO_MULTIVIEW_MODE_MONO ||
      in_mode == GST_VIDEO_MULTIVIEW_MODE_LEFT ||
      in_mode == GST_VIDEO_MULTIVIEW_MODE_RIGHT)
    mono_input = TRUE;

  data.output_type = out_mode;
  if (data.output_type == GST_VIDEO_MULTIVIEW_MODE_NONE ||
      data.output_type == GST_VIDEO_MULTIVIEW_MODE_MONO) {
    if (mono_input)
      data.output_type = GST_VIDEO_MULTIVIEW_MODE_LEFT;
    else
      data.output_type = GST_VIDEO_MULTIVIEW_MODE_MONO;
  } else if (data.output_type == GST_VIDEO_MULTIVIEW_MODE_SIDE_BY_SIDE_QUINCUNX) {
    data.output_type = GST_VIDEO_MULTIVIEW_MODE_SIDE_BY_SIDE;
  }

  data.tex_size[0] = GST_VIDEO_INFO_WIDTH (&vfilter->out_info);
  data.tex_size[1] = GST_VIDEO_INFO_HEIGHT (&vfilter->out_info);
  memcpy (&data.downmix[0], &downmix_matrices[conv->downmix_mode][0],
      sizeof (data.downmix[0]));
  memcpy (&data.downmix[1], &downmix_matrices[conv->downmix_mode][1],
      sizeof (data.downmix[1]));

  if (!gst_memory_map (conv->uniform, &map_info, GST_MAP_WRITE)) {
    return FALSE;
  }
  memcpy (map_info.data, &data, sizeof (data));
  gst_memory_unmap (conv->uniform, &map_info);

  return TRUE;
}

static GstMemory *
get_uniforms (GstVulkanViewConvert * conv,
    GstVulkanImageView ** in_views, GstVulkanImageView ** out_views)
{
  if (!conv->uniform) {
    if (!create_uniform_buffer (conv))
      return NULL;
    if (!update_uniform (conv, in_views, out_views)) {
      gst_memory_unref (conv->uniform);
      conv->uniform = NULL;
      return FALSE;
    }
  }

  return gst_memory_ref (conv->uniform);
}

static void gst_vulkan_view_convert_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_vulkan_view_convert_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static gboolean gst_vulkan_view_convert_start (GstBaseTransform * bt);
static gboolean gst_vulkan_view_convert_stop (GstBaseTransform * bt);

static GstCaps *gst_vulkan_view_convert_transform_caps (GstBaseTransform * bt,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static GstCaps *gst_vulkan_view_convert_fixate_caps (GstBaseTransform * bt,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps);
static GstFlowReturn gst_vulkan_view_convert_transform (GstBaseTransform * bt,
    GstBuffer * inbuf, GstBuffer * outbuf);
static gboolean gst_vulkan_view_convert_set_caps (GstBaseTransform * bt,
    GstCaps * in_caps, GstCaps * out_caps);

static GstStaticPadTemplate gst_vulkan_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_VULKAN_IMAGE,
            "{ BGRA, RGBA }")));

static GstStaticPadTemplate gst_vulkan_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_VULKAN_IMAGE,
            "{ BGRA, RGBA }")));

enum
{
  PROP_0,
  PROP_INPUT_LAYOUT,
  PROP_INPUT_FLAGS,
  PROP_OUTPUT_LAYOUT,
  PROP_OUTPUT_FLAGS,
  PROP_OUTPUT_DOWNMIX_MODE
};

#define DEFAULT_DOWNMIX GST_VULKAN_STEREO_DOWNMIX_ANAGLYPH_GREEN_MAGENTA_DUBOIS

#define gst_vulkan_view_convert_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstVulkanViewConvert, gst_vulkan_view_convert,
    GST_TYPE_VULKAN_VIDEO_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_debug_vulkan_view_convert,
        "vulkanviewconvert", 0, "Vulkan View Convert"));
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (vulkanviewconvert, "vulkanviewconvert",
    GST_RANK_NONE, GST_TYPE_VULKAN_VIEW_CONVERT, vulkan_element_init (plugin));

static void
gst_vulkan_view_convert_class_init (GstVulkanViewConvertClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseTransformClass *gstbasetransform_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasetransform_class = (GstBaseTransformClass *) klass;

  gobject_class->set_property = gst_vulkan_view_convert_set_property;
  gobject_class->get_property = gst_vulkan_view_convert_get_property;

  g_object_class_install_property (gobject_class, PROP_INPUT_LAYOUT,
      g_param_spec_enum ("input-mode-override",
          "Input Multiview Mode Override",
          "Override any input information about multiview layout",
          GST_TYPE_VIDEO_MULTIVIEW_MODE,
          GST_VIDEO_MULTIVIEW_MODE_NONE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_INPUT_FLAGS,
      g_param_spec_flags ("input-flags-override",
          "Input Multiview Flags Override",
          "Override any input information about multiview layout flags",
          GST_TYPE_VIDEO_MULTIVIEW_FLAGS, GST_VIDEO_MULTIVIEW_FLAGS_NONE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_OUTPUT_LAYOUT,
      g_param_spec_enum ("output-mode-override",
          "Output Multiview Mode Override",
          "Override automatic output mode selection for multiview layout",
          GST_TYPE_VIDEO_MULTIVIEW_MODE, GST_VIDEO_MULTIVIEW_MODE_NONE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_OUTPUT_FLAGS,
      g_param_spec_flags ("output-flags-override",
          "Output Multiview Flags Override",
          "Override automatic negotiation for output multiview layout flags",
          GST_TYPE_VIDEO_MULTIVIEW_FLAGS, GST_VIDEO_MULTIVIEW_FLAGS_NONE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_OUTPUT_DOWNMIX_MODE,
      g_param_spec_enum ("downmix-mode", "Mode for mono downmixed output",
          "Output anaglyph type to generate when downmixing to mono",
          GST_TYPE_VULKAN_STEREO_DOWNMIX, DEFAULT_DOWNMIX,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_metadata (gstelement_class, "Vulkan View Convert",
      "Filter/Video/Convert", "A Vulkan View Convert",
      "Matthew Waters <matthew@centricular.com>");

  gst_type_mark_as_plugin_api (GST_TYPE_VULKAN_STEREO_DOWNMIX, 0);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_vulkan_sink_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_vulkan_src_template);

  gstbasetransform_class->start =
      GST_DEBUG_FUNCPTR (gst_vulkan_view_convert_start);
  gstbasetransform_class->stop =
      GST_DEBUG_FUNCPTR (gst_vulkan_view_convert_stop);
  gstbasetransform_class->transform_caps =
      gst_vulkan_view_convert_transform_caps;
  gstbasetransform_class->fixate_caps = gst_vulkan_view_convert_fixate_caps;
  gstbasetransform_class->set_caps = gst_vulkan_view_convert_set_caps;
  gstbasetransform_class->transform = gst_vulkan_view_convert_transform;
}

static void
gst_vulkan_view_convert_init (GstVulkanViewConvert * conv)
{
  conv->downmix_mode = DEFAULT_DOWNMIX;

  conv->input_mode_override = GST_VIDEO_MULTIVIEW_MODE_NONE;
  conv->input_flags_override = GST_VIDEO_MULTIVIEW_FLAGS_NONE;
  conv->output_mode_override = GST_VIDEO_MULTIVIEW_MODE_NONE;
  conv->output_flags_override = GST_VIDEO_MULTIVIEW_FLAGS_NONE;
}

static void
gst_vulkan_view_convert_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVulkanViewConvert *conv = GST_VULKAN_VIEW_CONVERT (object);

  switch (prop_id) {
    case PROP_INPUT_LAYOUT:
      conv->input_mode_override = g_value_get_enum (value);
      gst_base_transform_reconfigure_src (GST_BASE_TRANSFORM (conv));
      break;
    case PROP_INPUT_FLAGS:
      conv->input_flags_override = g_value_get_flags (value);
      gst_base_transform_reconfigure_src (GST_BASE_TRANSFORM (conv));
      break;
    case PROP_OUTPUT_LAYOUT:
      conv->output_mode_override = g_value_get_enum (value);
      gst_base_transform_reconfigure_src (GST_BASE_TRANSFORM (conv));
      break;
    case PROP_OUTPUT_FLAGS:
      conv->output_flags_override = g_value_get_flags (value);
      gst_base_transform_reconfigure_src (GST_BASE_TRANSFORM (conv));
      break;
    case PROP_OUTPUT_DOWNMIX_MODE:
      conv->downmix_mode = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_vulkan_view_convert_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstVulkanViewConvert *conv = GST_VULKAN_VIEW_CONVERT (object);

  switch (prop_id) {
    case PROP_INPUT_LAYOUT:
      g_value_set_enum (value, conv->input_mode_override);
      break;
    case PROP_INPUT_FLAGS:
      g_value_set_flags (value, conv->input_flags_override);
      break;
    case PROP_OUTPUT_LAYOUT:
      g_value_set_enum (value, conv->output_mode_override);
      break;
    case PROP_OUTPUT_FLAGS:
      g_value_set_flags (value, conv->output_flags_override);
      break;
    case PROP_OUTPUT_DOWNMIX_MODE:
      g_value_set_enum (value, conv->downmix_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* Function that can halve the value
 * of ints, fractions, int/fraction ranges and lists of ints/fractions */
static gboolean
_halve_value (GValue * out, const GValue * in_value)
{
  /* Fundamental fixed types first */
  if (G_VALUE_HOLDS_INT (in_value)) {
    g_value_init (out, G_TYPE_INT);
    g_value_set_int (out, MAX (g_value_get_int (in_value) / 2, 1));
  } else if (GST_VALUE_HOLDS_FRACTION (in_value)) {
    gint num, den;
    num = gst_value_get_fraction_numerator (in_value);
    den = gst_value_get_fraction_denominator (in_value);
    g_value_init (out, GST_TYPE_FRACTION);
    /* Don't adjust 'infinite' fractions */
    if ((num != 1 || den != 2147483647) && (num != 2147483647 || den != 1)) {
      /* FIXME - could do better approximation when den > G_MAXINT/2? */
      den = den > G_MAXINT / 2 ? G_MAXINT : den * 2;
    }
    gst_value_set_fraction (out, num, den);
  } else if (GST_VALUE_HOLDS_INT_RANGE (in_value)) {
    gint range_min = gst_value_get_int_range_min (in_value);
    gint range_max = gst_value_get_int_range_max (in_value);
    gint range_step = gst_value_get_int_range_step (in_value);
    g_value_init (out, GST_TYPE_INT_RANGE);
    if (range_min != 1)
      range_min = MAX (1, range_min / 2);
    if (range_max != G_MAXINT)
      range_max = MAX (1, range_max / 2);
    gst_value_set_int_range_step (out, range_min,
        range_max, MAX (1, range_step / 2));
  } else if (GST_VALUE_HOLDS_FRACTION_RANGE (in_value)) {
    GValue min_out = G_VALUE_INIT;
    GValue max_out = G_VALUE_INIT;
    const GValue *range_min = gst_value_get_fraction_range_min (in_value);
    const GValue *range_max = gst_value_get_fraction_range_max (in_value);
    _halve_value (&min_out, range_min);
    _halve_value (&max_out, range_max);
    g_value_init (out, GST_TYPE_FRACTION_RANGE);
    gst_value_set_fraction_range (out, &min_out, &max_out);
    g_value_unset (&min_out);
    g_value_unset (&max_out);
  } else if (GST_VALUE_HOLDS_LIST (in_value)) {
    gint i;
    g_value_init (out, GST_TYPE_LIST);
    for (i = 0; i < gst_value_list_get_size (in_value); i++) {
      const GValue *entry;
      GValue tmp = G_VALUE_INIT;

      entry = gst_value_list_get_value (in_value, i);
      /* Random list values might not be the right type */
      if (!_halve_value (&tmp, entry))
        goto fail;
      gst_value_list_append_and_take_value (out, &tmp);
    }
  } else {
    return FALSE;
  }

  return TRUE;
fail:
  g_value_unset (out);
  return FALSE;
}

static GstStructure *
_halve_structure_field (const GstStructure * in, const gchar * field_name)
{
  GstStructure *out;
  const GValue *in_value = gst_structure_get_value (in, field_name);
  GValue tmp = G_VALUE_INIT;

  if (G_UNLIKELY (in_value == NULL))
    return gst_structure_copy (in);     /* Field doesn't exist, leave it as is */

  if (!_halve_value (&tmp, in_value))
    return NULL;

  out = gst_structure_copy (in);
  gst_structure_set_value (out, field_name, &tmp);
  g_value_unset (&tmp);

  return out;
}

/* Function that can double the value
 * of ints, fractions, int/fraction ranges and lists of ints/fractions */
static gboolean
_double_value (GValue * out, const GValue * in_value)
{
  /* Fundamental fixed types first */
  if (G_VALUE_HOLDS_INT (in_value)) {
    gint n = g_value_get_int (in_value);
    g_value_init (out, G_TYPE_INT);
    if (n <= G_MAXINT / 2)
      g_value_set_int (out, n * 2);
    else
      g_value_set_int (out, G_MAXINT);
  } else if (GST_VALUE_HOLDS_FRACTION (in_value)) {
    gint num, den;
    num = gst_value_get_fraction_numerator (in_value);
    den = gst_value_get_fraction_denominator (in_value);
    g_value_init (out, GST_TYPE_FRACTION);
    /* Don't adjust 'infinite' fractions */
    if ((num != 1 || den != 2147483647) && (num != 2147483647 || den != 1)) {
      /* FIXME - could do better approximation when num > G_MAXINT/2? */
      num = num > G_MAXINT / 2 ? G_MAXINT : num * 2;
    }
    gst_value_set_fraction (out, num, den);
  } else if (GST_VALUE_HOLDS_INT_RANGE (in_value)) {
    gint range_min = gst_value_get_int_range_min (in_value);
    gint range_max = gst_value_get_int_range_max (in_value);
    gint range_step = gst_value_get_int_range_step (in_value);
    if (range_min != 1) {
      range_min = MIN (G_MAXINT / 2, range_min);
      range_min *= 2;
    }
    if (range_max != G_MAXINT) {
      range_max = MIN (G_MAXINT / 2, range_max);
      range_max *= 2;
    }
    range_step = MIN (G_MAXINT / 2, range_step);
    g_value_init (out, GST_TYPE_INT_RANGE);
    gst_value_set_int_range_step (out, range_min, range_max, range_step);
  } else if (GST_VALUE_HOLDS_FRACTION_RANGE (in_value)) {
    GValue min_out = G_VALUE_INIT;
    GValue max_out = G_VALUE_INIT;
    const GValue *range_min = gst_value_get_fraction_range_min (in_value);
    const GValue *range_max = gst_value_get_fraction_range_max (in_value);
    _double_value (&min_out, range_min);
    _double_value (&max_out, range_max);
    g_value_init (out, GST_TYPE_FRACTION_RANGE);
    gst_value_set_fraction_range (out, &min_out, &max_out);
    g_value_unset (&min_out);
    g_value_unset (&max_out);
  } else if (GST_VALUE_HOLDS_LIST (in_value)) {
    gint i;
    g_value_init (out, GST_TYPE_LIST);
    for (i = 0; i < gst_value_list_get_size (in_value); i++) {
      const GValue *entry;
      GValue tmp = G_VALUE_INIT;

      entry = gst_value_list_get_value (in_value, i);
      /* Random list values might not be the right type */
      if (!_double_value (&tmp, entry))
        goto fail;
      gst_value_list_append_and_take_value (out, &tmp);
    }
  } else {
    return FALSE;
  }

  return TRUE;
fail:
  g_value_unset (out);
  return FALSE;
}

static GstStructure *
_double_structure_field (const GstStructure * in, const gchar * field_name)
{
  GstStructure *out;
  const GValue *in_value = gst_structure_get_value (in, field_name);
  GValue tmp = G_VALUE_INIT;

  if (G_UNLIKELY (in_value == NULL))
    return gst_structure_copy (in);     /* Field doesn't exist, leave it as is */

  if (!_double_value (&tmp, in_value))
    return NULL;

  out = gst_structure_copy (in);
  gst_structure_set_value (out, field_name, &tmp);
  g_value_unset (&tmp);

  return out;
}

/* Return a copy of the caps with the requested field doubled in value/range */
static GstCaps *
_double_caps_field (const GstCaps * in, const gchar * field_name)
{
  gint i;
  GstCaps *out = gst_caps_new_empty ();

  for (i = 0; i < gst_caps_get_size (in); i++) {
    const GstStructure *cur = gst_caps_get_structure (in, i);
    GstCapsFeatures *f = gst_caps_get_features (in, i);

    GstStructure *res = _double_structure_field (cur, field_name);
    out =
        gst_caps_merge_structure_full (out, res,
        f ? gst_caps_features_copy (f) : NULL);
  }

  return out;
}

/* Takes ownership of the input caps  */
static GstCaps *
_expand_par_for_half_aspect (GstCaps * in, gboolean vertical_half_aspect)
{
  guint mview_flags, mview_flags_mask;
  GstCaps *out;
  GstStructure *tmp;

  out = gst_caps_new_empty ();

  while (gst_caps_get_size (in) > 0) {
    GstStructure *s;
    GstCapsFeatures *features;

    features = gst_caps_get_features (in, 0);
    if (features)
      features = gst_caps_features_copy (features);

    s = gst_caps_steal_structure (in, 0);

    if (!gst_structure_get_flagset (s, "multiview-flags", &mview_flags,
            &mview_flags_mask)) {
      gst_caps_append_structure_full (out, s, features);
      continue;
    }
    /* If the input doesn't care about the half-aspect flag, allow current PAR in either variant */
    if ((mview_flags_mask & GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT) == 0) {
      gst_caps_append_structure_full (out, s, features);
      continue;
    }
    if (!gst_structure_has_field (s, "pixel-aspect-ratio")) {
      /* No par field, dont-care the half-aspect flag */
      gst_structure_set (s, "multiview-flags",
          GST_TYPE_VIDEO_MULTIVIEW_FLAGSET,
          mview_flags & ~GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT,
          mview_flags_mask & ~GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT, NULL);
      gst_caps_append_structure_full (out, s, features);
      continue;
    }

    /* Halve or double PAR base on inputs input specified. */

    /* Append a copy with the half-aspect flag as-is */
    tmp = gst_structure_copy (s);
    out = gst_caps_merge_structure_full (out, tmp,
        features ? gst_caps_features_copy (features) : NULL);

    /* and then a copy inverted */
    if (mview_flags & GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT) {
      /* Input is half-aspect. Double/halve the PAR, clear the flag */
      if (vertical_half_aspect)
        tmp = _halve_structure_field (s, "pixel-aspect-ratio");
      else
        tmp = _double_structure_field (s, "pixel-aspect-ratio");
      /* Clear the flag */
      gst_structure_set (tmp, "multiview-flags",
          GST_TYPE_VIDEO_MULTIVIEW_FLAGSET,
          mview_flags & ~GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT,
          mview_flags_mask | GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT, NULL);
    } else {
      if (vertical_half_aspect)
        tmp = _double_structure_field (s, "pixel-aspect-ratio");
      else
        tmp = _halve_structure_field (s, "pixel-aspect-ratio");
      /* Set the flag */
      gst_structure_set (tmp, "multiview-flags",
          GST_TYPE_VIDEO_MULTIVIEW_FLAGSET,
          mview_flags | GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT,
          mview_flags_mask | GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT, NULL);
    }

    out = gst_caps_merge_structure_full (out, tmp,
        features ? gst_caps_features_copy (features) : NULL);

    gst_structure_free (s);
    if (features)
      gst_caps_features_free (features);
  }

  gst_caps_unref (in);

  return out;
}

/* If input supports top-bottom or row-interleaved, we may halve height to mono frames.
 * If input supports left-right, checkerboard, quincunx or column-interleaved,
 * we may halve width to mono frames.
 * For output of top-bottom or row-interleaved, we may double the mono height
 * For output of left-right, checkerboard, quincunx or column-interleaved,
 * we may double the mono width.
 * In all cases, if input has half-aspect and output does not, we may double the PAR
 * And if input does *not* have half-aspect flag and output does not, we may halve the PAR
 */
static GstCaps *
_expand_structure (GstVulkanViewConvert * viewconvert,
    GstCaps * out_caps, GstStructure * structure, GstCapsFeatures * features)
{
  GstCaps *expanded_caps, *tmp;
  GstCaps *mono_caps;
  const gchar *default_mview_mode_str = NULL;
  guint mview_flags, mview_flags_mask;
  const GValue *in_modes;
  gint i;

  /* Empty caps to accumulate into */
  expanded_caps = gst_caps_new_empty ();

  /* First, set defaults if multiview flags are missing */
  default_mview_mode_str =
      gst_video_multiview_mode_to_caps_string (GST_VIDEO_MULTIVIEW_MODE_MONO);

  mview_flags = GST_VIDEO_MULTIVIEW_FLAGS_NONE;
  mview_flags_mask = GST_FLAG_SET_MASK_EXACT;

  if (!gst_structure_has_field (structure, "multiview-mode")) {
    gst_structure_set (structure,
        "multiview-mode", G_TYPE_STRING, default_mview_mode_str, NULL);
  }
  if (!gst_structure_has_field (structure, "multiview-flags")) {
    gst_structure_set (structure,
        "multiview-flags", GST_TYPE_VIDEO_MULTIVIEW_FLAGSET, mview_flags,
        mview_flags_mask, NULL);
  } else {
    gst_structure_get_flagset (structure, "multiview-flags",
        &mview_flags, &mview_flags_mask);
  }

  in_modes = gst_structure_get_value (structure, "multiview-mode");
  mono_caps = gst_caps_new_empty ();
  if (gst_value_intersect (NULL, in_modes,
          gst_video_multiview_get_mono_modes ())) {
    GstStructure *new_struct = gst_structure_copy (structure);
    gst_structure_set_value (new_struct, "multiview-mode",
        gst_video_multiview_get_mono_modes ());
    /* Half-aspect makes no sense for mono or unpacked, get rid of it */
    if (mview_flags & GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT) {
      gst_structure_set (new_struct, "multiview-flags",
          GST_TYPE_VIDEO_MULTIVIEW_FLAGSET,
          mview_flags & ~GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT,
          mview_flags_mask & ~GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT, NULL);
    }
    gst_caps_append_structure_full (mono_caps, new_struct,
        features ? gst_caps_features_copy (features) : NULL);
  }
  if (gst_value_intersect (NULL, in_modes,
          gst_video_multiview_get_unpacked_modes ())) {
    GstStructure *new_struct = gst_structure_copy (structure);

    gst_structure_set_value (new_struct, "multiview-mode",
        gst_video_multiview_get_mono_modes ());

    /* Half-aspect makes no sense for mono or unpacked, get rid of it */
    if (mview_flags & GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT) {
      gst_structure_set (new_struct, "multiview-flags",
          GST_TYPE_VIDEO_MULTIVIEW_FLAGSET,
          mview_flags & ~GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT,
          mview_flags_mask & ~GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT, NULL);
    }
    gst_caps_append_structure_full (mono_caps, new_struct,
        features ? gst_caps_features_copy (features) : NULL);
  }

  if (gst_value_intersect (NULL, in_modes,
          gst_video_multiview_get_doubled_height_modes ())) {
    /* Append mono formats with height halved */
    GstStructure *new_struct = _halve_structure_field (structure, "height");
    gst_structure_set_value (new_struct, "multiview-mode",
        gst_video_multiview_get_mono_modes ());
    /* Normalise the half-aspect flag away */
    if (mview_flags & GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT) {
      GstStructure *s =
          _halve_structure_field (new_struct, "pixel-aspect-ratio");
      gst_structure_set (structure, "multiview-flags",
          GST_TYPE_VIDEO_MULTIVIEW_FLAGSET,
          mview_flags & ~GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT,
          mview_flags_mask | GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT, NULL);
      gst_structure_free (new_struct);
      new_struct = s;
    }
    mono_caps = gst_caps_merge_structure_full (mono_caps, new_struct,
        features ? gst_caps_features_copy (features) : NULL);
  }
  if (gst_value_intersect (NULL, in_modes,
          gst_video_multiview_get_doubled_width_modes ())) {
    /* Append mono formats with width halved */
    GstStructure *new_struct = _halve_structure_field (structure, "width");
    gst_structure_set_value (new_struct, "multiview-mode",
        gst_video_multiview_get_mono_modes ());
    /* Normalise the half-aspect flag away */
    if (mview_flags & GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT) {
      GstStructure *s =
          _double_structure_field (new_struct, "pixel-aspect-ratio");
      gst_structure_set (structure, "multiview-flags",
          GST_TYPE_VIDEO_MULTIVIEW_FLAGSET,
          mview_flags & ~GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT,
          mview_flags_mask | GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT, NULL);
      gst_structure_free (new_struct);
      new_struct = s;
    }
    mono_caps = gst_caps_merge_structure_full (mono_caps, new_struct,
        features ? gst_caps_features_copy (features) : NULL);
  }
  if (gst_value_intersect (NULL, in_modes,
          gst_video_multiview_get_doubled_size_modes ())) {
    /* Append checkerboard/doubled size formats with width & height halved */
    GstStructure *new_struct_w = _halve_structure_field (structure, "width");
    GstStructure *new_struct_wh =
        _halve_structure_field (new_struct_w, "height");
    gst_structure_free (new_struct_w);
    gst_structure_set_value (new_struct_wh, "multiview-mode",
        gst_video_multiview_get_mono_modes ());
    mono_caps = gst_caps_merge_structure_full (mono_caps, new_struct_wh,
        features ? gst_caps_features_copy (features) : NULL);
  }

  /* Everything is normalised now, unset the flags we can change */
  /* Remove the views field, as these are all 'mono' modes
   * Need to do this before we expand caps back out to frame packed modes */
  for (i = 0; i < gst_caps_get_size (mono_caps); i++) {
    GstStructure *s = gst_caps_get_structure (mono_caps, i);
    gst_structure_remove_fields (s, "views", NULL);
    if (gst_structure_get_flagset (s, "multiview-flags", &mview_flags,
            &mview_flags_mask)) {
      /* Preserve only the half-aspect and mixed-mono flags, for now.
       * The rest we can change */
      mview_flags_mask &=
          (GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT |
          GST_VIDEO_MULTIVIEW_FLAGS_MIXED_MONO);
      gst_structure_set (s, "multiview-flags", GST_TYPE_VIDEO_MULTIVIEW_FLAGSET,
          mview_flags, mview_flags_mask, NULL);
    }
  }

  GST_TRACE_OBJECT (viewconvert,
      "Collected single-view caps %" GST_PTR_FORMAT, mono_caps);
  /* Put unpacked and mono modes first. We don't care about flags. Clear them */
  tmp = gst_caps_copy (mono_caps);
  for (i = 0; i < gst_caps_get_size (tmp); i++) {
    GstStructure *s = gst_caps_get_structure (tmp, i);
    gst_structure_remove_fields (s, "views", NULL);
    if (gst_structure_get_flagset (s, "multiview-flags", &mview_flags,
            &mview_flags_mask)) {
      /* We can change any flags for mono modes - half-aspect and mixed-mono have no meaning */
      mview_flags_mask = 0;
      gst_structure_set (s, "multiview-flags", GST_TYPE_VIDEO_MULTIVIEW_FLAGSET,
          mview_flags, mview_flags_mask, NULL);
    }
  }
  expanded_caps = gst_caps_merge (expanded_caps, tmp);

  /* Unpacked output modes have 2 views, for now */
  tmp = gst_caps_copy (mono_caps);
  gst_caps_set_value (tmp, "multiview-mode",
      gst_video_multiview_get_unpacked_modes ());
  for (i = 0; i < gst_caps_get_size (tmp); i++) {
    GstStructure *s = gst_caps_get_structure (tmp, i);
    gst_structure_set (s, "views", G_TYPE_INT, 2, NULL);
    if (gst_structure_get_flagset (s, "multiview-flags", &mview_flags,
            &mview_flags_mask)) {
      /* We can change any flags for unpacked modes - half-aspect and mixed-mono have no meaning */
      mview_flags_mask = 0;
      gst_structure_set (s, "multiview-flags", GST_TYPE_VIDEO_MULTIVIEW_FLAGSET,
          mview_flags, mview_flags_mask, NULL);
    }
  }
  expanded_caps = gst_caps_merge (expanded_caps, tmp);

  /* Double height output modes */
  tmp = _double_caps_field (mono_caps, "height");
  gst_caps_set_value (tmp, "multiview-mode",
      gst_video_multiview_get_doubled_height_modes ());
  tmp = _expand_par_for_half_aspect (tmp, TRUE);

  expanded_caps = gst_caps_merge (expanded_caps, tmp);

  /* Double width output modes */
  tmp = _double_caps_field (mono_caps, "width");
  gst_caps_set_value (tmp, "multiview-mode",
      gst_video_multiview_get_doubled_width_modes ());
  tmp = _expand_par_for_half_aspect (tmp, FALSE);

  expanded_caps = gst_caps_merge (expanded_caps, tmp);

  /* Double size output modes */
  {
    GstCaps *tmp_w = _double_caps_field (mono_caps, "width");
    tmp = _double_caps_field (tmp_w, "height");
    gst_caps_unref (tmp_w);
    gst_caps_set_value (tmp, "multiview-mode",
        gst_video_multiview_get_doubled_size_modes ());
    expanded_caps = gst_caps_merge (expanded_caps, tmp);
  }

  /* We're done with the mono caps now */
  gst_caps_unref (mono_caps);

  GST_TRACE_OBJECT (viewconvert,
      "expanded transform caps now %" GST_PTR_FORMAT, expanded_caps);

  if (gst_caps_is_empty (expanded_caps)) {
    gst_caps_unref (expanded_caps);
    return out_caps;
  }
  /* Really, we can rescale - so at this point we can append full-range
   * height/width/PAR as an unpreferred final option. */
/*  tmp = gst_caps_copy (expanded_caps);
  gst_caps_set_simple (tmp, "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
      "height", GST_TYPE_INT_RANGE, 1, G_MAXINT, NULL);
*/
  out_caps = gst_caps_merge (out_caps, expanded_caps);
/*  out_caps = gst_caps_merge (out_caps, tmp);*/
  return out_caps;
}

static GstCaps *
_intersect_with_mview_mode (GstCaps * caps,
    GstVideoMultiviewMode mode, GstVideoMultiviewFlags flags)
{
  GstCaps *filter, *result;
  const gchar *caps_str;

  caps_str = gst_video_multiview_mode_to_caps_string (mode);

  filter = gst_caps_new_simple ("video/x-raw",
      "multiview-mode", G_TYPE_STRING,
      caps_str, "multiview-flags", GST_TYPE_VIDEO_MULTIVIEW_FLAGSET, flags,
      GST_FLAG_SET_MASK_EXACT, NULL);

  if (mode == GST_VIDEO_MULTIVIEW_MODE_SEPARATED ||
      mode == GST_VIDEO_MULTIVIEW_MODE_FRAME_BY_FRAME)
    gst_caps_set_simple (filter, "views", G_TYPE_INT, 2, NULL);
  gst_caps_set_features (filter, 0, gst_caps_features_new_any ());

  GST_DEBUG ("Intersecting target caps %" GST_PTR_FORMAT
      " with caps %" GST_PTR_FORMAT, caps, filter);

  result = gst_caps_intersect_full (caps, filter, GST_CAPS_INTERSECT_FIRST);
  gst_caps_unref (filter);
  return result;
}

static GstCaps *
_intersect_with_mview_modes (GstCaps * caps, const GValue * modes)
{
  GstCaps *filter, *result;

  filter = gst_caps_new_empty_simple ("video/x-raw");

  gst_caps_set_value (filter, "multiview-mode", modes);
  gst_caps_set_features (filter, 0, gst_caps_features_new_any ());

  GST_DEBUG ("Intersecting target caps %" GST_PTR_FORMAT
      " with caps %" GST_PTR_FORMAT, caps, filter);

  result = gst_caps_intersect_full (caps, filter, GST_CAPS_INTERSECT_FIRST);
  gst_caps_unref (filter);
  return result;
}

static GstCaps *
gst_vulkan_view_convert_transform_caps (GstBaseTransform * bt,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstVulkanViewConvert *viewconvert = GST_VULKAN_VIEW_CONVERT (bt);
  GstCaps *base_caps =
      gst_static_pad_template_get_caps (&gst_vulkan_sink_template);
  GstCaps *out_caps, *tmp_caps;
  gint i;

  GST_DEBUG_OBJECT (viewconvert, "Direction %s "
      "input caps %" GST_PTR_FORMAT " filter %" GST_PTR_FORMAT,
      direction == GST_PAD_SINK ? "sink" : "src", caps, filter);

  /* We can only process VulkanImage caps, start from that */
  caps = gst_caps_intersect (caps, base_caps);
  gst_caps_unref (base_caps);

  /* Change input/output to the formats we can convert to/from,
   * but keep the original caps at the start - we will always prefer
   * passthrough */
  if (direction == GST_PAD_SINK) {
    out_caps = gst_caps_copy (caps);
    if (viewconvert->input_mode_override != GST_VIDEO_MULTIVIEW_MODE_NONE) {
      GstVideoMultiviewMode mode = viewconvert->input_mode_override;
      GstVideoMultiviewFlags flags = viewconvert->input_flags_override;

      const gchar *caps_str = gst_video_multiview_mode_to_caps_string (mode);
      /* Coerce the input caps before transforming, so the sizes come out right */
      gst_caps_set_simple (out_caps, "multiview-mode", G_TYPE_STRING,
          caps_str, "multiview-flags", GST_TYPE_VIDEO_MULTIVIEW_FLAGSET, flags,
          GST_FLAG_SET_MASK_EXACT, NULL);
    }
  } else {
    out_caps = gst_caps_new_empty ();
  }

  for (i = 0; i < gst_caps_get_size (caps); i++) {
    GstStructure *structure = gst_caps_get_structure (caps, i);
    GstCapsFeatures *features = gst_caps_get_features (caps, i);
    out_caps = _expand_structure (viewconvert, out_caps, structure, features);
  }

  if (gst_caps_is_empty (out_caps))
    goto out;

  /* If we have an output mode override, limit things to that */
  if (direction == GST_PAD_SINK &&
      viewconvert->output_mode_override != GST_VIDEO_MULTIVIEW_MODE_NONE) {

    tmp_caps = _intersect_with_mview_mode (out_caps,
        viewconvert->output_mode_override, viewconvert->output_flags_override);

    gst_caps_unref (out_caps);
    out_caps = tmp_caps;
  } else if (viewconvert->input_mode_override != GST_VIDEO_MULTIVIEW_MODE_NONE) {
    /* Prepend a copy of our preferred input caps in case the peer
     * can handle them */
    tmp_caps = _intersect_with_mview_mode (out_caps,
        viewconvert->input_mode_override, viewconvert->input_flags_override);
    out_caps = gst_caps_merge (out_caps, tmp_caps);
  }
  if (direction == GST_PAD_SRC) {
    GstStructure *s;
    /* When generating input caps, we also need a copy of the mono caps
     * without multiview-mode or flags for backwards compat, at the end */
    tmp_caps = _intersect_with_mview_mode (caps,
        GST_VIDEO_MULTIVIEW_MODE_MONO, GST_VIDEO_MULTIVIEW_FLAGS_NONE);
    if (!gst_caps_is_empty (tmp_caps)) {
      s = gst_caps_get_structure (tmp_caps, 0);
      gst_structure_remove_fields (s, "multiview-mode", "multiview-flags",
          NULL);
      out_caps = gst_caps_merge (out_caps, tmp_caps);
    } else
      gst_caps_unref (tmp_caps);
  }
out:
  gst_caps_unref (caps);

  GST_DEBUG_OBJECT (viewconvert, "Have caps %" GST_PTR_FORMAT
      " filtering with caps %" GST_PTR_FORMAT, out_caps, filter);

  if (filter) {
    GstCaps *tmp =
        gst_caps_intersect_full (filter, out_caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (out_caps);
    out_caps = tmp;
  }

  GST_DEBUG_OBJECT (viewconvert, "Returning caps %" GST_PTR_FORMAT, out_caps);
  return out_caps;
}

static GstCaps *
fixate_size (GstVulkanViewConvert * viewconvert,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
  GstStructure *ins, *outs;
  const GValue *from_par, *to_par;
  GValue fpar = { 0, }, tpar = {
    0,
  };

  othercaps = gst_caps_make_writable (othercaps);
  othercaps = gst_caps_truncate (othercaps);

  GST_DEBUG_OBJECT (viewconvert, "trying to fixate othercaps %" GST_PTR_FORMAT
      " based on caps %" GST_PTR_FORMAT, othercaps, caps);

  ins = gst_caps_get_structure (caps, 0);
  outs = gst_caps_get_structure (othercaps, 0);

  from_par = gst_structure_get_value (ins, "pixel-aspect-ratio");
  to_par = gst_structure_get_value (outs, "pixel-aspect-ratio");

  /* If we're fixating from the sinkpad we always set the PAR and
   * assume that missing PAR on the sinkpad means 1/1 and
   * missing PAR on the srcpad means undefined
   */
  if (direction == GST_PAD_SINK) {
    if (!from_par) {
      g_value_init (&fpar, GST_TYPE_FRACTION);
      gst_value_set_fraction (&fpar, 1, 1);
      from_par = &fpar;
    }
    if (!to_par) {
      g_value_init (&tpar, GST_TYPE_FRACTION);
      gst_value_set_fraction (&tpar, 1, 1);
      to_par = &tpar;
    }
  } else {
    if (!to_par) {
      g_value_init (&tpar, GST_TYPE_FRACTION);
      gst_value_set_fraction (&tpar, 1, 1);
      to_par = &tpar;

      gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
          NULL);
    }
    if (!from_par) {
      g_value_init (&fpar, GST_TYPE_FRACTION);
      gst_value_set_fraction (&fpar, 1, 1);
      from_par = &fpar;
    }
  }

  /* we have both PAR but they might not be fixated */
  {
    gint from_w, from_h, from_par_n, from_par_d, to_par_n, to_par_d;
    gint w = 0, h = 0;
    gint from_dar_n, from_dar_d;
    gint num, den;

    /* from_par should be fixed */
    g_return_val_if_fail (gst_value_is_fixed (from_par), othercaps);

    from_par_n = gst_value_get_fraction_numerator (from_par);
    from_par_d = gst_value_get_fraction_denominator (from_par);

    gst_structure_get_int (ins, "width", &from_w);
    gst_structure_get_int (ins, "height", &from_h);

    gst_structure_get_int (outs, "width", &w);
    gst_structure_get_int (outs, "height", &h);

    /* if both width and height are already fixed, we can't do anything
     * about it anymore */
    if (w && h) {
      GST_DEBUG_OBJECT (viewconvert,
          "dimensions already set to %dx%d, not fixating", w, h);
      if (!gst_value_is_fixed (to_par)) {
        GST_DEBUG_OBJECT (viewconvert, "fixating to_par to %dx%d", 1, 1);
        if (gst_structure_has_field (outs, "pixel-aspect-ratio"))
          gst_structure_fixate_field_nearest_fraction (outs,
              "pixel-aspect-ratio", 1, 1);
      }
      goto done;
    }

    /* Calculate input DAR */
    if (!gst_util_fraction_multiply (from_w, from_h, from_par_n, from_par_d,
            &from_dar_n, &from_dar_d)) {
      GST_ELEMENT_ERROR (viewconvert, CORE, NEGOTIATION, (NULL),
          ("Error calculating the output scaled size - integer overflow"));
      goto done;
    }

    GST_DEBUG_OBJECT (viewconvert, "Input DAR is %d/%d", from_dar_n,
        from_dar_d);

    /* If either width or height are fixed there's not much we
     * can do either except choosing a height or width and PAR
     * that matches the DAR as good as possible
     */
    if (h) {
      gint num, den;

      GST_DEBUG_OBJECT (viewconvert, "height is fixed (%d)", h);

      if (!gst_value_is_fixed (to_par)) {
        /* (shortcut) copy-paste (??) of videoscale seems to aim for 1/1,
         * so let's make it so ...
         * especially if following code assumes fixed */
        GST_DEBUG_OBJECT (viewconvert, "fixating to_par to 1x1");
        gst_structure_fixate_field_nearest_fraction (outs,
            "pixel-aspect-ratio", 1, 1);
        to_par = gst_structure_get_value (outs, "pixel-aspect-ratio");
      }

      /* PAR is fixed, choose the height that is nearest to the
       * height with the same DAR */
      to_par_n = gst_value_get_fraction_numerator (to_par);
      to_par_d = gst_value_get_fraction_denominator (to_par);

      GST_DEBUG_OBJECT (viewconvert, "PAR is fixed %d/%d", to_par_n, to_par_d);

      if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, to_par_d,
              to_par_n, &num, &den)) {
        GST_ELEMENT_ERROR (viewconvert, CORE, NEGOTIATION, (NULL),
            ("Error calculating the output scaled size - integer overflow"));
        goto done;
      }

      w = (guint) gst_util_uint64_scale_int (h, num, den);
      gst_structure_fixate_field_nearest_int (outs, "width", w);

      goto done;
    } else if (w) {
      gint num, den;

      GST_DEBUG_OBJECT (viewconvert, "width is fixed (%d)", w);

      if (!gst_value_is_fixed (to_par)) {
        /* (shortcut) copy-paste (??) of videoscale seems to aim for 1/1,
         * so let's make it so ...
         * especially if following code assumes fixed */
        GST_DEBUG_OBJECT (viewconvert, "fixating to_par to 1x1");
        gst_structure_fixate_field_nearest_fraction (outs,
            "pixel-aspect-ratio", 1, 1);
        to_par = gst_structure_get_value (outs, "pixel-aspect-ratio");
      }

      /* PAR is fixed, choose the height that is nearest to the
       * height with the same DAR */
      to_par_n = gst_value_get_fraction_numerator (to_par);
      to_par_d = gst_value_get_fraction_denominator (to_par);

      GST_DEBUG_OBJECT (viewconvert, "PAR is fixed %d/%d", to_par_n, to_par_d);

      if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, to_par_d,
              to_par_n, &num, &den)) {
        GST_ELEMENT_ERROR (viewconvert, CORE, NEGOTIATION, (NULL),
            ("Error calculating the output scaled size - integer overflow"));
        goto done;
      }

      h = (guint) gst_util_uint64_scale_int (w, den, num);
      gst_structure_fixate_field_nearest_int (outs, "height", h);

      goto done;
    } else if (gst_value_is_fixed (to_par)) {
      GstStructure *tmp;
      gint set_h, set_w, f_h, f_w;

      to_par_n = gst_value_get_fraction_numerator (to_par);
      to_par_d = gst_value_get_fraction_denominator (to_par);

      /* Calculate scale factor for the PAR change */
      if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, to_par_n,
              to_par_d, &num, &den)) {
        GST_ELEMENT_ERROR (viewconvert, CORE, NEGOTIATION, (NULL),
            ("Error calculating the output scaled size - integer overflow"));
        goto done;
      }

      /* Try to keep the input height */
      tmp = gst_structure_copy (outs);
      gst_structure_fixate_field_nearest_int (tmp, "height", from_h);
      gst_structure_get_int (tmp, "height", &set_h);

      /* This might have failed but try to scale the width
       * to keep the DAR nonetheless */
      w = (guint) gst_util_uint64_scale_int (set_h, num, den);
      gst_structure_fixate_field_nearest_int (tmp, "width", w);
      gst_structure_get_int (tmp, "width", &set_w);
      gst_structure_free (tmp);

      /* We kept the DAR and the height is nearest to the original height */
      if (set_w == w) {
        gst_structure_set (outs, "width", G_TYPE_INT, set_w, "height",
            G_TYPE_INT, set_h, NULL);
        goto done;
      }

      f_h = set_h;
      f_w = set_w;

      /* If the former failed, try to keep the input width at least */
      tmp = gst_structure_copy (outs);
      gst_structure_fixate_field_nearest_int (tmp, "width", from_w);
      gst_structure_get_int (tmp, "width", &set_w);

      /* This might have failed but try to scale the width
       * to keep the DAR nonetheless */
      h = (guint) gst_util_uint64_scale_int (set_w, den, num);
      gst_structure_fixate_field_nearest_int (tmp, "height", h);
      gst_structure_get_int (tmp, "height", &set_h);
      gst_structure_free (tmp);

      /* We kept the DAR and the width is nearest to the original width */
      if (set_h == h) {
        gst_structure_set (outs, "width", G_TYPE_INT, set_w, "height",
            G_TYPE_INT, set_h, NULL);
        goto done;
      }

      /* If all this failed, keep the height that was nearest to the original
       * height and the nearest possible width. This changes the DAR but
       * there's not much else to do here.
       */
      gst_structure_set (outs, "width", G_TYPE_INT, f_w, "height", G_TYPE_INT,
          f_h, NULL);
      goto done;
    } else {
      GstStructure *tmp;
      gint set_h, set_w, set_par_n, set_par_d, tmp2;

      /* width, height and PAR are not fixed */

      /* First try to keep the height and width as good as possible
       * and scale PAR */
      tmp = gst_structure_copy (outs);
      gst_structure_fixate_field_nearest_int (tmp, "height", from_h);
      gst_structure_get_int (tmp, "height", &set_h);
      gst_structure_fixate_field_nearest_int (tmp, "width", from_w);
      gst_structure_get_int (tmp, "width", &set_w);

      if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, set_h, set_w,
              &to_par_n, &to_par_d)) {
        GST_ELEMENT_ERROR (viewconvert, CORE, NEGOTIATION, (NULL),
            ("Error calculating the output scaled size - integer overflow"));
        gst_structure_free (tmp);
        goto done;
      }

      if (!gst_structure_has_field (tmp, "pixel-aspect-ratio"))
        gst_structure_set_value (tmp, "pixel-aspect-ratio", to_par);
      gst_structure_fixate_field_nearest_fraction (tmp, "pixel-aspect-ratio",
          to_par_n, to_par_d);
      gst_structure_get_fraction (tmp, "pixel-aspect-ratio", &set_par_n,
          &set_par_d);
      gst_structure_free (tmp);

      if (set_par_n == to_par_n && set_par_d == to_par_d) {
        gst_structure_set (outs, "width", G_TYPE_INT, set_w, "height",
            G_TYPE_INT, set_h, NULL);

        if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
            set_par_n != set_par_d)
          gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
              set_par_n, set_par_d, NULL);
        goto done;
      }

      /* Otherwise try to scale width to keep the DAR with the set
       * PAR and height */
      if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, set_par_d,
              set_par_n, &num, &den)) {
        GST_ELEMENT_ERROR (viewconvert, CORE, NEGOTIATION, (NULL),
            ("Error calculating the output scaled size - integer overflow"));
        goto done;
      }

      w = (guint) gst_util_uint64_scale_int (set_h, num, den);
      tmp = gst_structure_copy (outs);
      gst_structure_fixate_field_nearest_int (tmp, "width", w);
      gst_structure_get_int (tmp, "width", &tmp2);
      gst_structure_free (tmp);

      if (tmp2 == w) {
        gst_structure_set (outs, "width", G_TYPE_INT, tmp2, "height",
            G_TYPE_INT, set_h, NULL);
        if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
            set_par_n != set_par_d)
          gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
              set_par_n, set_par_d, NULL);
        goto done;
      }

      /* ... or try the same with the height */
      h = (guint) gst_util_uint64_scale_int (set_w, den, num);
      tmp = gst_structure_copy (outs);
      gst_structure_fixate_field_nearest_int (tmp, "height", h);
      gst_structure_get_int (tmp, "height", &tmp2);
      gst_structure_free (tmp);

      if (tmp2 == h) {
        gst_structure_set (outs, "width", G_TYPE_INT, set_w, "height",
            G_TYPE_INT, tmp2, NULL);
        if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
            set_par_n != set_par_d)
          gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
              set_par_n, set_par_d, NULL);
        goto done;
      }

      /* If all fails we can't keep the DAR and take the nearest values
       * for everything from the first try */
      gst_structure_set (outs, "width", G_TYPE_INT, set_w, "height",
          G_TYPE_INT, set_h, NULL);
      if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
          set_par_n != set_par_d)
        gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
            set_par_n, set_par_d, NULL);
    }
  }

done:
  othercaps = gst_caps_fixate (othercaps);

  GST_DEBUG_OBJECT (viewconvert, "fixated othercaps to %" GST_PTR_FORMAT,
      othercaps);

  if (from_par == &fpar)
    g_value_unset (&fpar);
  if (to_par == &tpar)
    g_value_unset (&tpar);

  return othercaps;
}

static GstCaps *
gst_vulkan_view_convert_fixate_caps (GstBaseTransform * bt,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
  GstVulkanViewConvert *viewconvert = GST_VULKAN_VIEW_CONVERT (bt);
  GstVideoMultiviewMode mode = viewconvert->output_mode_override;
  GstVideoMultiviewFlags flags = viewconvert->output_flags_override;
  GstCaps *tmp;

  othercaps = gst_caps_make_writable (othercaps);
  GST_LOG_OBJECT (viewconvert, "dir %s fixating %" GST_PTR_FORMAT
      " against caps %" GST_PTR_FORMAT,
      direction == GST_PAD_SINK ? "sink" : "src", othercaps, caps);

  if (direction == GST_PAD_SINK) {
    if (mode != GST_VIDEO_MULTIVIEW_MODE_NONE) {
      /* We have a requested output mode and are fixating source caps, try and enforce it */
      GST_DEBUG_OBJECT (bt, "fixating multiview mode using the configured "
          "output override mode 0x%x and flags 0x%x", mode, flags);
      tmp = _intersect_with_mview_mode (othercaps, mode, flags);
      gst_caps_unref (othercaps);
      othercaps = tmp;
    } else {
      /* See if we can do passthrough */
      GstVideoInfo info;

      if (gst_video_info_from_caps (&info, caps)) {
        GstVideoMultiviewMode mode = GST_VIDEO_INFO_MULTIVIEW_MODE (&info);
        GstVideoMultiviewFlags flags = GST_VIDEO_INFO_MULTIVIEW_FLAGS (&info);

        if (viewconvert->input_mode_override != GST_VIDEO_MULTIVIEW_MODE_NONE) {
          mode = viewconvert->input_mode_override;
          flags = viewconvert->input_flags_override;
        }

        tmp = _intersect_with_mview_mode (othercaps, mode, flags);
        if (gst_caps_is_empty (tmp)) {
          /* Nope, we can't pass our input caps downstream */
          gst_caps_unref (tmp);
        } else {
          GST_DEBUG_OBJECT (bt, "can configure a passthrough multiview mode "
              "using the input override mode 0x%x and flags 0x%x", mode, flags);
          gst_caps_unref (othercaps);
          othercaps = tmp;
          goto done;
        }
      }

      /* Prefer an unpacked mode for output */
      tmp =
          _intersect_with_mview_modes (othercaps,
          gst_video_multiview_get_unpacked_modes ());
      if (!gst_caps_is_empty (tmp)) {
        GST_DEBUG_OBJECT (bt, "preferring an unpacked multiview mode");
        gst_caps_unref (othercaps);
        othercaps = tmp;
      } else {
        gst_caps_unref (tmp);
      }
    }
  } else if (viewconvert->input_mode_override != GST_VIDEO_MULTIVIEW_MODE_NONE) {
    /* See if we can coerce the caps into matching input mode/flags,
     * in case it doesn't care at all, but allow it not to too */
    mode = viewconvert->input_mode_override;
    flags = viewconvert->input_flags_override;
    tmp = _intersect_with_mview_mode (othercaps, mode, flags);
    if (gst_caps_is_empty (tmp)) {
      /* Nope, we can pass our input caps downstream */
      gst_caps_unref (tmp);
    } else {
      GST_DEBUG_OBJECT (bt, "can configure a passthrough multiview mode "
          "using the input override mode 0x%x and flags 0x%x", mode, flags);
      gst_caps_unref (othercaps);
      othercaps = tmp;
    }
  }

done:
  othercaps = fixate_size (viewconvert, direction, caps, othercaps);
  othercaps = gst_caps_fixate (othercaps);

  GST_DEBUG_OBJECT (viewconvert, "dir %s fixated to %" GST_PTR_FORMAT
      " against caps %" GST_PTR_FORMAT,
      direction == GST_PAD_SINK ? "sink" : "src", othercaps, caps);
  return othercaps;
}

static gboolean
create_descriptor_set_layout (GstVulkanViewConvert * conv, guint n_mems,
    GError ** error)
{
  GstVulkanVideoFilter *vfilter = GST_VULKAN_VIDEO_FILTER (conv);
  VkDescriptorSetLayoutBinding bindings[GST_VIDEO_MAX_PLANES * 2 + 1] =
      { {0,}, };
  VkDescriptorSetLayoutCreateInfo layout_info;
  VkDescriptorSetLayout descriptor_set_layout;
  int descriptor_n = 0;
  VkResult err;
  int i;

  /* *INDENT-OFF* */
  bindings[descriptor_n++] = (VkDescriptorSetLayoutBinding) {
      .binding = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .pImmutableSamplers = NULL,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
  };
  for (i = 0; i < n_mems * 2; i++) {
    bindings[descriptor_n++] = (VkDescriptorSetLayoutBinding) {
      .binding = i + 1,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .pImmutableSamplers = NULL,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
    };
  };

  layout_info = (VkDescriptorSetLayoutCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .pNext = NULL,
      .bindingCount = descriptor_n,
      .pBindings = bindings
  };
  /* *INDENT-ON* */

  err =
      vkCreateDescriptorSetLayout (vfilter->device->device, &layout_info,
      NULL, &descriptor_set_layout);
  if (gst_vulkan_error_to_g_error (err, error,
          "vkCreateDescriptorSetLayout") < 0) {
    return FALSE;
  }

  conv->quad->descriptor_set_layout =
      gst_vulkan_handle_new_wrapped (vfilter->device,
      GST_VULKAN_HANDLE_TYPE_DESCRIPTOR_SET_LAYOUT,
      (GstVulkanHandleTypedef) descriptor_set_layout,
      gst_vulkan_handle_free_descriptor_set_layout, NULL);

  return TRUE;
}

static gboolean
gst_vulkan_view_convert_start (GstBaseTransform * bt)
{
  GstVulkanVideoFilter *vfilter = GST_VULKAN_VIDEO_FILTER (bt);
  GstVulkanViewConvert *conv = GST_VULKAN_VIEW_CONVERT (bt);
  GstVulkanHandle *vert, *frag;

  if (!GST_BASE_TRANSFORM_CLASS (parent_class)->start (bt))
    return FALSE;

  conv->quad = gst_vulkan_full_screen_quad_new (vfilter->queue);

  if (!(vert =
          gst_vulkan_create_shader (vfilter->device, identity_vert,
              identity_vert_size, NULL))) {
    return FALSE;
  }
  if (!(frag =
          gst_vulkan_create_shader (vfilter->device, view_convert_frag,
              view_convert_frag_size, NULL))) {
    gst_vulkan_handle_unref (vert);
    return FALSE;
  }

  if (!gst_vulkan_full_screen_quad_set_shaders (conv->quad, vert, frag)) {
    gst_vulkan_handle_unref (vert);
    gst_vulkan_handle_unref (frag);
    return FALSE;
  }
  gst_vulkan_handle_unref (vert);
  gst_vulkan_handle_unref (frag);

  return TRUE;
}

static gboolean
gst_vulkan_view_convert_set_caps (GstBaseTransform * bt, GstCaps * in_caps,
    GstCaps * out_caps)
{
  GstVulkanVideoFilter *vfilter = GST_VULKAN_VIDEO_FILTER (bt);
  GstVulkanViewConvert *conv = GST_VULKAN_VIEW_CONVERT (bt);

  if (!GST_BASE_TRANSFORM_CLASS (parent_class)->set_caps (bt, in_caps,
          out_caps))
    return FALSE;

  if (!gst_vulkan_full_screen_quad_set_info (conv->quad, &vfilter->in_info,
          &vfilter->out_info))
    return FALSE;

  if (conv->uniform)
    gst_memory_unref (conv->uniform);
  conv->uniform = NULL;

  return TRUE;
}

static gboolean
gst_vulkan_view_convert_stop (GstBaseTransform * bt)
{
  GstVulkanViewConvert *conv = GST_VULKAN_VIEW_CONVERT (bt);

  gst_clear_object (&conv->quad);
  if (conv->uniform)
    gst_memory_unref (conv->uniform);
  conv->uniform = NULL;

  return GST_BASE_TRANSFORM_CLASS (parent_class)->stop (bt);
}

static GstFlowReturn
gst_vulkan_view_convert_transform (GstBaseTransform * bt, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstVulkanVideoFilter *vfilter = GST_VULKAN_VIDEO_FILTER (bt);
  GstVulkanViewConvert *conv = GST_VULKAN_VIEW_CONVERT (bt);
  GstVulkanImageView *in_img_views[GST_VIDEO_MAX_PLANES] = { NULL, };
  GstVulkanImageView *out_img_views[GST_VIDEO_MAX_PLANES] = { NULL, };
  GstVulkanCommandBuffer *cmd_buf = NULL;
  GstVulkanFence *fence = NULL;
  GError *error = NULL;
  VkResult err;
  guint in_n_mems, out_n_mems;
  int i;

  if (!gst_vulkan_full_screen_quad_set_input_buffer (conv->quad, inbuf, &error))
    goto error;
  if (!gst_vulkan_full_screen_quad_set_output_buffer (conv->quad, outbuf,
          &error))
    goto error;

  fence = gst_vulkan_device_create_fence (vfilter->device, &error);
  if (!fence)
    goto error;

  in_n_mems = gst_buffer_n_memory (inbuf);
  for (i = 0; i < in_n_mems; i++) {
    GstMemory *img_mem = gst_buffer_peek_memory (inbuf, i);
    if (!gst_is_vulkan_image_memory (img_mem)) {
      g_set_error_literal (&error, GST_VULKAN_ERROR, GST_VULKAN_FAILED,
          "Input memory must be a GstVulkanImageMemory");
      goto error;
    }
    in_img_views[i] =
        gst_vulkan_get_or_create_image_view ((GstVulkanImageMemory *) img_mem);
    gst_vulkan_trash_list_add (conv->quad->trash_list,
        gst_vulkan_trash_list_acquire (conv->quad->trash_list, fence,
            gst_vulkan_trash_mini_object_unref,
            (GstMiniObject *) in_img_views[i]));
  }
  out_n_mems = gst_buffer_n_memory (outbuf);
  for (i = 0; i < out_n_mems; i++) {
    GstMemory *mem = gst_buffer_peek_memory (outbuf, i);
    if (!gst_is_vulkan_image_memory (mem)) {
      g_set_error_literal (&error, GST_VULKAN_ERROR, GST_VULKAN_FAILED,
          "Output memory must be a GstVulkanImageMemory");
      goto error;
    }
    out_img_views[i] =
        gst_vulkan_get_or_create_image_view ((GstVulkanImageMemory *) mem);
    gst_vulkan_trash_list_add (conv->quad->trash_list,
        gst_vulkan_trash_list_acquire (conv->quad->trash_list, fence,
            gst_vulkan_trash_mini_object_unref,
            (GstMiniObject *) out_img_views[i]));
  }

  {
    GstMemory *uniforms = get_uniforms (conv, in_img_views, out_img_views);
    if (!gst_vulkan_full_screen_quad_set_uniform_buffer (conv->quad, uniforms,
            &error))
      goto error;
    gst_memory_unref (uniforms);
  }

  if (!conv->quad->descriptor_set_layout)
    if (!create_descriptor_set_layout (conv, in_n_mems, &error))
      goto error;

  if (!gst_vulkan_full_screen_quad_prepare_draw (conv->quad, fence, &error))
    goto error;

  if (!(cmd_buf =
          gst_vulkan_command_pool_create (conv->quad->cmd_pool, &error)))
    goto error;

  {
    VkCommandBufferBeginInfo cmd_buf_info = { 0, };

    /* *INDENT-OFF* */
    cmd_buf_info = (VkCommandBufferBeginInfo) {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = NULL
    };
    /* *INDENT-ON* */

    gst_vulkan_command_buffer_lock (cmd_buf);
    err = vkBeginCommandBuffer (cmd_buf->cmd, &cmd_buf_info);
    if (gst_vulkan_error_to_g_error (err, &error, "vkBeginCommandBuffer") < 0)
      goto error;
  }

  update_descriptor_set (conv, in_img_views, in_n_mems);
  if (!gst_vulkan_full_screen_quad_fill_command_buffer (conv->quad, cmd_buf,
          fence, &error))
    goto unlock_error;

  err = vkEndCommandBuffer (cmd_buf->cmd);
  gst_vulkan_command_buffer_unlock (cmd_buf);
  if (gst_vulkan_error_to_g_error (err, &error, "vkEndCommandBuffer") < 0)
    goto error;

  if (!gst_vulkan_full_screen_quad_submit (conv->quad, cmd_buf, fence, &error))
    goto error;

  gst_vulkan_fence_unref (fence);

  return GST_FLOW_OK;

unlock_error:
  if (cmd_buf) {
    gst_vulkan_command_buffer_unlock (cmd_buf);
    gst_vulkan_command_buffer_unref (cmd_buf);
  }

error:
  gst_clear_mini_object ((GstMiniObject **) & fence);

  GST_ELEMENT_ERROR (bt, LIBRARY, FAILED, ("%s", error->message), (NULL));
  g_clear_error (&error);
  return GST_FLOW_ERROR;
}
