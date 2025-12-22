/* GStreamer
 * Copyright (C) 2023 Igalia, S.L.
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

#include "vkh264dec.h"

#include <gst/video/video.h>
#include <gst/vulkan/vulkan.h>

#include "gstvulkanelements.h"

typedef struct _GstVulkanH264Decoder GstVulkanH264Decoder;
typedef struct _GstVulkanH264Picture GstVulkanH264Picture;

struct _GstVulkanH264Picture
{
  GstVulkanDecoderPicture base;

  /* Picture refs. */
  StdVideoDecodeH264ReferenceInfo std_refs[36];
  VkVideoDecodeH264DpbSlotInfoKHR vk_slots[36];

  /* Current picture */
  StdVideoDecodeH264ReferenceInfo std_ref;
  VkVideoDecodeH264DpbSlotInfoKHR vk_slot;

  VkVideoDecodeH264PictureInfoKHR vk_h264pic;
  StdVideoDecodeH264PictureInfo std_h264pic;

  gint32 slot_idx;
};

struct _GstVulkanH264Decoder
{
  GstH264Decoder parent;

  GstVulkanInstance *instance;
  GstVulkanDevice *device;
  GstVulkanQueue *graphic_queue, *decode_queue;

  GstVulkanDecoder *decoder;

  gboolean need_negotiation;
  gboolean need_params_update;

  gint width;
  gint height;
  gint coded_width;
  gint coded_height;
  gint dpb_size;

  VkSamplerYcbcrRange range;
  VkChromaLocation xloc, yloc;

  GstVideoCodecState *output_state;
};

static GstStaticPadTemplate gst_vulkan_h264dec_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-h264, "
        "profile = { (string) high, (string) main, (string) constrained-baseline, (string) baseline } ,"
        "stream-format = { (string) avc, (string) byte-stream }, "
        "alignment = (string) au"));

static GstStaticPadTemplate gst_vulkan_h264dec_src_template =
GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_VULKAN_IMAGE, "NV12")));

GST_DEBUG_CATEGORY (gst_debug_vulkan_h264_decoder);
#define GST_CAT_DEFAULT gst_debug_vulkan_h264_decoder

#define gst_vulkan_h264_decoder_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstVulkanH264Decoder, gst_vulkan_h264_decoder,
    GST_TYPE_H264_DECODER,
    GST_DEBUG_CATEGORY_INIT (gst_debug_vulkan_h264_decoder,
        "vulkanh264dec", 0, "Vulkan H.264 Decoder"));
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (vulkanh264dec, "vulkanh264dec",
    GST_RANK_NONE, GST_TYPE_VULKAN_H264_DECODER, vulkan_element_init (plugin));

static gboolean
_find_queues (GstVulkanDevice * device, GstVulkanQueue * queue, gpointer data)
{
  GstVulkanH264Decoder *self = data;
  guint32 flags =
      device->physical_device->queue_family_props[queue->family].queueFlags;
  guint32 codec =
      device->physical_device->queue_family_ops[queue->family].video;

  if (!self->graphic_queue
      && ((flags & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT)) {
    self->graphic_queue = gst_object_ref (queue);
  }

  if (!self->decode_queue
      && ((codec & VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR)
          == VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR)
      && ((flags & VK_QUEUE_VIDEO_DECODE_BIT_KHR)
          == VK_QUEUE_VIDEO_DECODE_BIT_KHR)) {
    self->decode_queue = gst_object_ref (queue);
  }

  return !(self->decode_queue && self->graphic_queue);
}

static gboolean
gst_vulkan_h264_decoder_open (GstVideoDecoder * decoder)
{
  GstVulkanH264Decoder *self = GST_VULKAN_H264_DECODER (decoder);

  if (!gst_vulkan_ensure_element_data (GST_ELEMENT (decoder), NULL,
          &self->instance)) {
    GST_ELEMENT_ERROR (self, RESOURCE, NOT_FOUND,
        ("Failed to retrieve vulkan instance"), (NULL));
    return FALSE;
  }

  if (!gst_vulkan_device_run_context_query (GST_ELEMENT (decoder),
          &self->device)) {
    GError *error = NULL;
    GST_DEBUG_OBJECT (self, "No device retrieved from peer elements");
    self->device = gst_vulkan_instance_create_device (self->instance, &error);
    if (!self->device) {
      GST_ELEMENT_ERROR (self, RESOURCE, NOT_FOUND,
          ("Failed to create vulkan device"),
          ("%s", error ? error->message : ""));
      g_clear_error (&error);
      return FALSE;
    }
  }

  if (!gst_vulkan_queue_run_context_query (GST_ELEMENT (self),
          &self->graphic_queue)) {
    GST_DEBUG_OBJECT (self, "No graphic queue retrieved from peer elements");
  }

  gst_vulkan_device_foreach_queue (self->device, _find_queues, self);

  if (!self->decode_queue) {
    GST_ELEMENT_ERROR (self, RESOURCE, NOT_FOUND,
        ("Failed to create/retrieve vulkan H.264 decoder queue"), (NULL));
    return FALSE;
  }

  self->decoder = gst_vulkan_queue_create_decoder (self->decode_queue,
      VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR);
  if (!self->decoder) {
    GST_ELEMENT_ERROR (self, RESOURCE, NOT_FOUND,
        ("Failed to create vulkan H.264 decoder"), (NULL));
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_vulkan_h264_decoder_close (GstVideoDecoder * decoder)
{
  GstVulkanH264Decoder *self = GST_VULKAN_H264_DECODER (decoder);

  gst_clear_object (&self->decoder);
  gst_clear_object (&self->decode_queue);
  gst_clear_object (&self->graphic_queue);
  gst_clear_object (&self->device);
  gst_clear_object (&self->instance);

  return TRUE;
}

static gboolean
gst_vulkan_h264_decoder_stop (GstVideoDecoder * decoder)
{
  GstVulkanH264Decoder *self = GST_VULKAN_H264_DECODER (decoder);

  if (self->decoder)
    gst_vulkan_decoder_stop (self->decoder);

  if (self->output_state)
    gst_video_codec_state_unref (self->output_state);

  return GST_VIDEO_DECODER_CLASS (parent_class)->stop (decoder);
}

static void
gst_vulkan_h264_decoder_set_context (GstElement * element, GstContext * context)
{
  GstVulkanH264Decoder *self = GST_VULKAN_H264_DECODER (element);

  gst_vulkan_handle_set_context (element, context, NULL, &self->instance);

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

static gboolean
_query_context (GstVulkanH264Decoder * self, GstQuery * query)
{
  if (gst_vulkan_handle_context_query (GST_ELEMENT (self), query, NULL,
          self->instance, self->device))
    return TRUE;

  if (gst_vulkan_queue_handle_context_query (GST_ELEMENT (self), query,
          self->graphic_queue))
    return TRUE;

  return FALSE;
}

static gboolean
gst_vulkan_h264_decoder_src_query (GstVideoDecoder * decoder, GstQuery * query)
{
  gboolean ret = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      ret = _query_context (GST_VULKAN_H264_DECODER (decoder), query);
      break;
    default:
      ret = GST_VIDEO_DECODER_CLASS (parent_class)->src_query (decoder, query);
      break;
  }

  return ret;
}

static gboolean
gst_vulkan_h264_decoder_sink_query (GstVideoDecoder * decoder, GstQuery * query)
{
  gboolean ret = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      ret = _query_context (GST_VULKAN_H264_DECODER (decoder), query);
      break;
    default:
      ret = GST_VIDEO_DECODER_CLASS (parent_class)->sink_query (decoder, query);
      break;
  }

  return ret;
}

static gboolean
gst_vulkan_h264_decoder_negotiate (GstVideoDecoder * decoder)
{
  GstVulkanH264Decoder *self = GST_VULKAN_H264_DECODER (decoder);
  GstH264Decoder *h264dec = GST_H264_DECODER (decoder);
  VkVideoFormatPropertiesKHR format_prop;
  GstVideoInterlaceMode interlace_mode;
  GstVideoFormat format;

  /* Ignore downstream renegotiation request. */
  if (!self->need_negotiation)
    return TRUE;

  if (!gst_vulkan_decoder_out_format (self->decoder, &format_prop))
    return FALSE;

  self->need_negotiation = FALSE;

  if (self->output_state)
    gst_video_codec_state_unref (self->output_state);

  interlace_mode =
      self->decoder->profile.codec.h264dec.pictureLayout ==
      VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_INTERLACED_INTERLEAVED_LINES_BIT_KHR ?
      GST_VIDEO_INTERLACE_MODE_MIXED : GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;

  format = gst_vulkan_format_to_video_format (format_prop.format);
  self->output_state = gst_video_decoder_set_interlaced_output_state (decoder,
      format, interlace_mode, self->width, self->height, h264dec->input_state);

  self->output_state->caps = gst_video_info_to_caps (&self->output_state->info);
  gst_caps_set_features_simple (self->output_state->caps,
      gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_VULKAN_IMAGE, NULL));

  GST_INFO_OBJECT (self, "Negotiated caps %" GST_PTR_FORMAT,
      self->output_state->caps);

  return GST_VIDEO_DECODER_CLASS (parent_class)->negotiate (decoder);
}

static gboolean
gst_vulkan_h264_decoder_decide_allocation (GstVideoDecoder * decoder,
    GstQuery * query)
{
  GstVulkanH264Decoder *self = GST_VULKAN_H264_DECODER (decoder);
  GstCaps *new_caps, *profile_caps, *caps = NULL;
  GstBufferPool *pool = NULL;
  GstStructure *config;
  guint size, min, max;
  gboolean update_pool;
  VkImageUsageFlags usage;
  GstVulkanVideoCapabilities vk_caps;

  gst_query_parse_allocation (query, &caps, NULL);
  if (!caps)
    return FALSE;
  if (!gst_vulkan_decoder_caps (self->decoder, &vk_caps))
    return FALSE;

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);
    update_pool = TRUE;
  } else {
    GstVideoInfo vinfo;

    gst_video_info_from_caps (&vinfo, caps);
    size = GST_VIDEO_INFO_SIZE (&vinfo);
    min = 2;
    max = 0;
    update_pool = FALSE;
  }

  if (!(pool && GST_IS_VULKAN_IMAGE_BUFFER_POOL (pool))) {
    gst_clear_object (&pool);
    pool = gst_vulkan_image_buffer_pool_new (self->device);
  }

  usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
      | VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR;

  if (!self->decoder->dedicated_dpb) {
    min = MAX (min, MIN (self->dpb_size, vk_caps.caps.maxDpbSlots));
    max = 0;
    usage |= VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR;
  }

  new_caps = gst_caps_copy (caps);
  gst_caps_set_simple (new_caps, "width", G_TYPE_INT, self->coded_width,
      "height", G_TYPE_INT, self->coded_height, NULL);
  profile_caps = gst_vulkan_decoder_profile_caps (self->decoder);

  config = gst_buffer_pool_get_config (pool);

  gst_buffer_pool_config_set_params (config, new_caps, size, min, max);

  gst_vulkan_image_buffer_pool_config_set_allocation_params (config, usage,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR,
      VK_ACCESS_TRANSFER_WRITE_BIT);
  gst_vulkan_image_buffer_pool_config_set_decode_caps (config, profile_caps);

  gst_caps_unref (profile_caps);
  gst_caps_unref (new_caps);

  if (!gst_buffer_pool_set_config (pool, config))
    goto bail;

  if (update_pool)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);

  gst_object_unref (pool);

  gst_vulkan_decoder_create_dpb_pool (self->decoder, new_caps);

  return TRUE;

bail:
  {
    gst_clear_caps (&new_caps);
    gst_clear_object (&pool);
    return FALSE;
  }
}

/* set a common pipeline stage valid for any queue to avoid Vulkan Validation
 * errors */
static void
reset_pipeline_stage_mask (GstBuffer * buf)
{
  guint i, n = gst_buffer_n_memory (buf);

  for (i = 0; i < n; i++) {
    GstVulkanImageMemory *vk_mem =
        (GstVulkanImageMemory *) gst_buffer_peek_memory (buf, i);
    vk_mem->barrier.parent.pipeline_stages = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  }
}

static GstVulkanH264Picture *
gst_vulkan_h264_picture_new (GstVulkanH264Decoder * self, GstBuffer * out)
{
  GstVulkanH264Picture *pic;

  pic = g_new0 (GstVulkanH264Picture, 1);
  gst_vulkan_decoder_picture_init (self->decoder, &pic->base, out);
  reset_pipeline_stage_mask (out);

  return pic;
}

static void
gst_vulkan_h264_picture_free (gpointer data)
{
  GstVulkanH264Picture *pic = data;

  gst_vulkan_decoder_picture_release (&pic->base);
  g_free (pic);
}

static VkVideoChromaSubsamplingFlagBitsKHR
_get_chroma_subsampling_flag (guint8 chroma_format_idc)
{
  switch (chroma_format_idc) {
    case 1:
      return VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
    case 2:
      return VK_VIDEO_CHROMA_SUBSAMPLING_422_BIT_KHR;
    case 3:
      return VK_VIDEO_CHROMA_SUBSAMPLING_444_BIT_KHR;
    default:
      return VK_VIDEO_CHROMA_SUBSAMPLING_INVALID_KHR;
  }
}

static VkVideoComponentBitDepthFlagBitsKHR
_get_component_bit_depth (guint8 bit_depth)
{
  switch (bit_depth) {
    case 8:
      return VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
    case 10:
      return VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR;
    case 12:
      return VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR;
    default:
      return VK_VIDEO_COMPONENT_BIT_DEPTH_INVALID_KHR;
  }
}

static StdVideoH264ProfileIdc
_get_h264_profile (GstH264Profile profile_idc)
{
  switch (profile_idc) {
    case GST_H264_PROFILE_BASELINE:
      return STD_VIDEO_H264_PROFILE_IDC_BASELINE;
    case GST_H264_PROFILE_MAIN:
      return STD_VIDEO_H264_PROFILE_IDC_MAIN;
    case GST_H264_PROFILE_HIGH:
      return STD_VIDEO_H264_PROFILE_IDC_HIGH;
    case GST_H264_PROFILE_HIGH_444:
      return STD_VIDEO_H264_PROFILE_IDC_HIGH_444_PREDICTIVE;
    default:
      return STD_VIDEO_H264_PROFILE_IDC_INVALID;
  }
}

static StdVideoH264LevelIdc
_get_h264_level_idc (int level_idc)
{
  switch (level_idc) {
    case 10:
      return STD_VIDEO_H264_LEVEL_IDC_1_0;
    case 11:
      return STD_VIDEO_H264_LEVEL_IDC_1_1;
    case 12:
      return STD_VIDEO_H264_LEVEL_IDC_1_2;
    case 13:
      return STD_VIDEO_H264_LEVEL_IDC_1_3;
    case 20:
      return STD_VIDEO_H264_LEVEL_IDC_2_0;
    case 21:
      return STD_VIDEO_H264_LEVEL_IDC_2_1;
    case 22:
      return STD_VIDEO_H264_LEVEL_IDC_2_2;
    case 30:
      return STD_VIDEO_H264_LEVEL_IDC_3_0;
    case 31:
      return STD_VIDEO_H264_LEVEL_IDC_3_1;
    case 32:
      return STD_VIDEO_H264_LEVEL_IDC_3_2;
    case 40:
      return STD_VIDEO_H264_LEVEL_IDC_4_0;
    case 41:
      return STD_VIDEO_H264_LEVEL_IDC_4_1;
    case 42:
      return STD_VIDEO_H264_LEVEL_IDC_4_2;
    case 50:
      return STD_VIDEO_H264_LEVEL_IDC_5_0;
    case 51:
      return STD_VIDEO_H264_LEVEL_IDC_5_1;
    case 52:
      return STD_VIDEO_H264_LEVEL_IDC_5_2;
    case 60:
      return STD_VIDEO_H264_LEVEL_IDC_6_0;
    case 61:
      return STD_VIDEO_H264_LEVEL_IDC_6_1;
    default:
    case 62:
      return STD_VIDEO_H264_LEVEL_IDC_6_2;
  }
}

static void
gst_vulkan_video_profile_from_h264_sps (GstVulkanVideoProfile * profile,
    const GstH264SPS * sps)
{
  /* *INDENT-OFF* */
  *profile = (GstVulkanVideoProfile) {
    .profile = {
      .sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR,
      .pNext = &profile->usage,
      .videoCodecOperation = VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR,
      .chromaSubsampling = _get_chroma_subsampling_flag (sps->chroma_format_idc),
      .lumaBitDepth = _get_component_bit_depth (sps->bit_depth_luma_minus8 + 8),
      .chromaBitDepth = _get_component_bit_depth (sps->bit_depth_chroma_minus8 + 8),
    },
    .usage.decode = {
      .sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_USAGE_INFO_KHR,
      .videoUsageHints = VK_VIDEO_DECODE_USAGE_DEFAULT_KHR,
      .pNext = &profile->codec,
    },
    .codec.h264dec = {
      .sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PROFILE_INFO_KHR,
      .stdProfileIdc = _get_h264_profile (sps->profile_idc),
      .pictureLayout = sps->frame_mbs_only_flag ?
          VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_PROGRESSIVE_KHR :
          VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_INTERLACED_INTERLEAVED_LINES_BIT_KHR,
    },
  };
  /* *INDENT-ON* */
}

static GstFlowReturn
gst_vulkan_h264_decoder_new_sequence (GstH264Decoder * decoder,
    const GstH264SPS * sps, gint max_dpb_size)
{
  GstVulkanH264Decoder *self = GST_VULKAN_H264_DECODER (decoder);
  GstVulkanVideoProfile profile;
  GstVulkanVideoCapabilities vk_caps;
  GError *error = NULL;
  gint width, height;
  VkFormat old_format = VK_FORMAT_UNDEFINED;
  VkVideoFormatPropertiesKHR format_prop;

  gst_vulkan_video_profile_from_h264_sps (&profile, sps);

  if (gst_vulkan_decoder_is_started (self->decoder)) {
    if (!gst_vulkan_video_profile_is_equal (&self->decoder->profile, &profile)) {
      if (gst_vulkan_decoder_out_format (self->decoder, &format_prop))
        old_format = format_prop.format;
      gst_vulkan_decoder_stop (self->decoder);
    } else {
      self->need_negotiation = FALSE;
    }
  }

  if (!gst_vulkan_decoder_is_started (self->decoder)) {
    self->need_negotiation = TRUE;
    if (!gst_vulkan_decoder_start (self->decoder, &profile, &error)) {
      GST_ERROR_OBJECT (self, "Couldn't start decoder: %s",
          error ? error->message : "");
      g_clear_error (&error);
      return GST_FLOW_ERROR;
    }
  }

  self->dpb_size = MAX (self->dpb_size, max_dpb_size);

  if (sps->frame_cropping_flag) {
    width = sps->crop_rect_width;
    height = sps->crop_rect_height;
  } else {
    width = sps->width;
    height = sps->height;
  }

  gst_vulkan_decoder_caps (self->decoder, &vk_caps);
  self->coded_width =
      GST_ROUND_UP_N (sps->width, vk_caps.caps.pictureAccessGranularity.width);
  self->coded_height = GST_ROUND_UP_N (sps->height,
      vk_caps.caps.pictureAccessGranularity.height);

  self->need_negotiation &= (width != self->width || height != self->height);
  self->width = width;
  self->height = height;

  /* Ycbcr sampler */
  {
    VkSamplerYcbcrRange range;
    VkChromaLocation xloc, yloc;
    gboolean ret;
    int loc;

    ret = gst_vulkan_decoder_out_format (self->decoder, &format_prop);
    g_assert (ret);

    range = VK_SAMPLER_YCBCR_RANGE_ITU_FULL;
    loc = 0;

    if (sps->vui_parameters_present_flag) {
      const GstH264VUIParams *vui = &sps->vui_parameters;

      range = vui->video_full_range_flag > 0 ?
          VK_SAMPLER_YCBCR_RANGE_ITU_FULL : VK_SAMPLER_YCBCR_RANGE_ITU_NARROW;

      if (vui->chroma_loc_info_present_flag)
        loc = vui->chroma_sample_loc_type_top_field;
    }

    xloc = (loc % 2 == 0) ? VK_CHROMA_LOCATION_MIDPOINT
        : VK_CHROMA_LOCATION_COSITED_EVEN;
    yloc = ((loc >> 1) ^ (loc < 4)) ? VK_CHROMA_LOCATION_MIDPOINT
        : VK_CHROMA_LOCATION_COSITED_EVEN;

    if (old_format != format_prop.format || range != self->range
        || xloc != self->xloc || yloc != self->yloc) {
      self->range = range;
      self->xloc = xloc;
      self->yloc = yloc;
      ret = gst_vulkan_decoder_update_ycbcr_sampler (self->decoder, range, xloc,
          yloc, &error);
      if (!ret && error) {
        GST_WARNING_OBJECT (self, "Unable to create Ycbcr sampler: %s",
            error->message);
        g_clear_error (&error);
      }
    }
  }

  self->need_params_update = TRUE;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_vulkan_h264_decoder_new_picture (GstH264Decoder * decoder,
    GstVideoCodecFrame * frame, GstH264Picture * picture)
{
  GstVulkanH264Decoder *self = GST_VULKAN_H264_DECODER (decoder);
  GstVideoDecoder *vdec = GST_VIDEO_DECODER (decoder);
  GstFlowReturn ret;
  GstVulkanH264Picture *pic;

  GST_TRACE_OBJECT (self, "New picture");

  if (self->need_negotiation) {
    if (!gst_video_decoder_negotiate (vdec)) {
      GST_ERROR_OBJECT (self, "Failed downstream negotiation.");
      return GST_FLOW_ERROR;
    }
  }

  ret = gst_video_decoder_allocate_output_frame (vdec, frame);
  if (ret != GST_FLOW_OK)
    goto allocation_failed;

  pic = gst_vulkan_h264_picture_new (self, frame->output_buffer);
  gst_h264_picture_set_user_data (picture, pic, gst_vulkan_h264_picture_free);

  return GST_FLOW_OK;

allocation_failed:
  {
    GST_WARNING_OBJECT (self, "Failed to allocated input or output buffer: %s",
        gst_flow_get_name (ret));
    return ret;
  }
}

static GstFlowReturn
gst_vulkan_h264_decoder_new_field_picture (GstH264Decoder * decoder,
    GstH264Picture * first_field, GstH264Picture * second_field)
{
  GstVulkanH264Decoder *self = GST_VULKAN_H264_DECODER (decoder);
  GstVulkanH264Picture *first_pic, *second_pic;

  GST_TRACE_OBJECT (self, "New field picture");

  first_pic = gst_h264_picture_get_user_data (first_field);
  if (!first_pic)
    return GST_FLOW_ERROR;

  second_pic = gst_vulkan_h264_picture_new (self, first_pic->base.out);
  gst_h264_picture_set_user_data (second_field, second_pic,
      gst_vulkan_h264_picture_free);

  GST_LOG_OBJECT (self, "New vulkan decode picture %p", second_pic);

  return GST_FLOW_OK;
}

static void
_fill_sps (const GstH264SPS * sps, StdVideoH264SequenceParameterSet * std_sps,
    StdVideoH264HrdParameters * vkhrd,
    StdVideoH264SequenceParameterSetVui * vkvui,
    StdVideoH264ScalingLists * vkscaling_lists)
{
  const GstH264VUIParams *vui = &sps->vui_parameters;
  const GstH264HRDParams *hrd;
  int i;

  /* *INDENT-OFF* */
  *vkscaling_lists = (StdVideoH264ScalingLists) {
    .scaling_list_present_mask = sps->scaling_matrix_present_flag,
    .use_default_scaling_matrix_mask = 0, /* We already fill in the default matrix */
  };

  for (i = 0; i < STD_VIDEO_H264_SCALING_LIST_4X4_NUM_LISTS; i++) {
    memcpy (vkscaling_lists->ScalingList4x4[i], sps->scaling_lists_4x4[i],
        STD_VIDEO_H264_SCALING_LIST_4X4_NUM_ELEMENTS
        * sizeof (**sps->scaling_lists_4x4));
  }

  for (i = 0; i < STD_VIDEO_H264_SCALING_LIST_8X8_NUM_LISTS; i++) {
    memcpy (vkscaling_lists->ScalingList8x8[i], sps->scaling_lists_8x8[i],
            STD_VIDEO_H264_SCALING_LIST_8X8_NUM_ELEMENTS
            * sizeof (**sps->scaling_lists_8x8));
  }

  if (sps->vui_parameters_present_flag) {
    if (vui->nal_hrd_parameters_present_flag)
      hrd = &vui->nal_hrd_parameters;
    else if (vui->vcl_hrd_parameters_present_flag)
      hrd = &vui->vcl_hrd_parameters;
    else
      hrd = NULL;

    if (hrd) {
      *vkhrd = (StdVideoH264HrdParameters) {
        .cpb_cnt_minus1 = hrd->cpb_cnt_minus1,
        .bit_rate_scale = hrd->bit_rate_scale,
        .cpb_size_scale = hrd->cpb_size_scale,
        .initial_cpb_removal_delay_length_minus1 =
            hrd->initial_cpb_removal_delay_length_minus1,
        .cpb_removal_delay_length_minus1 = hrd->cpb_removal_delay_length_minus1,
        .dpb_output_delay_length_minus1 = hrd->dpb_output_delay_length_minus1,
        .time_offset_length = hrd->time_offset_length,
      };

      memcpy (vkhrd->bit_rate_value_minus1, hrd->bit_rate_value_minus1,
          STD_VIDEO_H264_CPB_CNT_LIST_SIZE
          * sizeof (*hrd->bit_rate_value_minus1));

      memcpy (vkhrd->cpb_size_value_minus1, hrd->cpb_size_value_minus1,
          STD_VIDEO_H264_CPB_CNT_LIST_SIZE
          * sizeof (*hrd->cpb_size_value_minus1));

      memcpy (vkhrd->cbr_flag, hrd->cbr_flag,
          STD_VIDEO_H264_CPB_CNT_LIST_SIZE * sizeof (*hrd->cbr_flag));
    }

    *vkvui = (StdVideoH264SequenceParameterSetVui) {
      .flags = {
        .aspect_ratio_info_present_flag = vui->aspect_ratio_info_present_flag,
        .overscan_info_present_flag = vui->overscan_info_present_flag,
        .overscan_appropriate_flag = vui->overscan_appropriate_flag,
        .video_signal_type_present_flag = vui->video_signal_type_present_flag,
        .video_full_range_flag = vui->video_full_range_flag,
        .color_description_present_flag = vui->colour_description_present_flag,
        .chroma_loc_info_present_flag = vui->chroma_loc_info_present_flag,
        .timing_info_present_flag = vui->timing_info_present_flag,
        .fixed_frame_rate_flag = vui->fixed_frame_rate_flag,
        .bitstream_restriction_flag = vui->bitstream_restriction_flag,
        .nal_hrd_parameters_present_flag = vui->nal_hrd_parameters_present_flag,
        .vcl_hrd_parameters_present_flag = vui->vcl_hrd_parameters_present_flag,
      },
      .aspect_ratio_idc = vui->aspect_ratio_idc,
      .sar_width = vui->sar_width,
      .sar_height = vui->sar_height,
      .video_format = vui->video_format,
      .colour_primaries = vui->colour_primaries,
      .transfer_characteristics = vui->transfer_characteristics,
      .matrix_coefficients = vui->matrix_coefficients,
      .num_units_in_tick = vui->num_units_in_tick,
      .time_scale = vui->time_scale,
      .max_num_reorder_frames = (uint8_t) vui->num_reorder_frames,
      .max_dec_frame_buffering = (uint8_t) vui->max_dec_frame_buffering,
      .chroma_sample_loc_type_top_field = vui->chroma_sample_loc_type_top_field,
      .chroma_sample_loc_type_bottom_field =
          vui->chroma_sample_loc_type_bottom_field,
      .pHrdParameters = hrd ? vkhrd : NULL,
    };
  }

  *std_sps = (StdVideoH264SequenceParameterSet) {
    .flags = {
      .constraint_set0_flag = sps->constraint_set0_flag,
      .constraint_set1_flag = sps->constraint_set1_flag,
      .constraint_set2_flag = sps->constraint_set2_flag,
      .constraint_set3_flag = sps->constraint_set3_flag,
      .constraint_set4_flag = sps->constraint_set4_flag,
      .constraint_set5_flag = sps->constraint_set5_flag,
      .direct_8x8_inference_flag = sps->direct_8x8_inference_flag,
      .mb_adaptive_frame_field_flag = sps->mb_adaptive_frame_field_flag,
      .frame_mbs_only_flag = sps->frame_mbs_only_flag,
      .delta_pic_order_always_zero_flag = sps->delta_pic_order_always_zero_flag,
      .separate_colour_plane_flag = sps->separate_colour_plane_flag,
      .gaps_in_frame_num_value_allowed_flag =
          sps->gaps_in_frame_num_value_allowed_flag,
      .qpprime_y_zero_transform_bypass_flag =
          sps->qpprime_y_zero_transform_bypass_flag,
      .frame_cropping_flag = sps->frame_cropping_flag,
      .seq_scaling_matrix_present_flag = sps->scaling_matrix_present_flag,
      .vui_parameters_present_flag = sps->vui_parameters_present_flag,
    },
    .profile_idc = sps->profile_idc,
    .level_idc = _get_h264_level_idc (sps->level_idc),
    .chroma_format_idc = sps->chroma_format_idc,
    .seq_parameter_set_id = (uint8_t) sps->id,
    .bit_depth_luma_minus8 = sps->bit_depth_luma_minus8,
    .bit_depth_chroma_minus8 = sps->bit_depth_chroma_minus8,
    .log2_max_frame_num_minus4 = sps->log2_max_frame_num_minus4,
    .pic_order_cnt_type = sps->pic_order_cnt_type,
    .offset_for_non_ref_pic = sps->offset_for_non_ref_pic,
    .offset_for_top_to_bottom_field = sps->offset_for_top_to_bottom_field,
    .log2_max_pic_order_cnt_lsb_minus4 = sps->log2_max_pic_order_cnt_lsb_minus4,
    .num_ref_frames_in_pic_order_cnt_cycle =
        sps->num_ref_frames_in_pic_order_cnt_cycle,
    .max_num_ref_frames = (uint8_t) sps->num_ref_frames,
    .pic_width_in_mbs_minus1 = sps->pic_width_in_mbs_minus1,
    .pic_height_in_map_units_minus1 = sps->pic_height_in_map_units_minus1,
    .frame_crop_left_offset = sps->frame_crop_left_offset,
    .frame_crop_right_offset = sps->frame_crop_right_offset,
    .frame_crop_top_offset = sps->frame_crop_top_offset,
    .frame_crop_bottom_offset = sps->frame_crop_bottom_offset,
    .pOffsetForRefFrame = sps->offset_for_ref_frame,
    .pScalingLists = vkscaling_lists,
    .pSequenceParameterSetVui = sps->vui_parameters_present_flag ? vkvui : NULL,
  };
  /* *INDENT-ON* */

  return;
}

static void
_fill_pps (const GstH264PPS * pps, StdVideoH264PictureParameterSet * std_pps,
    StdVideoH264ScalingLists * vkscaling_lists)
{
  int i;

  /* *INDENT-OFF* */
  *vkscaling_lists = (StdVideoH264ScalingLists) {
    .scaling_list_present_mask = pps->pic_scaling_matrix_present_flag,
    .use_default_scaling_matrix_mask = 0, /* We already fill in the default matrix */
  };

  for (i = 0; i < STD_VIDEO_H264_SCALING_LIST_4X4_NUM_LISTS; i++) {
    memcpy (vkscaling_lists->ScalingList4x4[i], pps->scaling_lists_4x4[i],
        STD_VIDEO_H264_SCALING_LIST_4X4_NUM_ELEMENTS
        * sizeof (**pps->scaling_lists_4x4));
  }

  for (i = 0; i < STD_VIDEO_H264_SCALING_LIST_8X8_NUM_LISTS; i++) {
    memcpy (vkscaling_lists->ScalingList8x8[i], pps->scaling_lists_8x8[i],
        STD_VIDEO_H264_SCALING_LIST_8X8_NUM_ELEMENTS
        * sizeof (**pps->scaling_lists_8x8));
  }

  *std_pps = (StdVideoH264PictureParameterSet) {
    .flags = {
      .transform_8x8_mode_flag = pps->transform_8x8_mode_flag,
      .redundant_pic_cnt_present_flag = pps->redundant_pic_cnt_present_flag,
      .constrained_intra_pred_flag = pps->constrained_intra_pred_flag,
      .deblocking_filter_control_present_flag =
          pps->deblocking_filter_control_present_flag,
      .weighted_pred_flag = pps->weighted_pred_flag,
      .bottom_field_pic_order_in_frame_present_flag =
          pps->pic_order_present_flag,
      .entropy_coding_mode_flag = pps->entropy_coding_mode_flag,
      .pic_scaling_matrix_present_flag = pps->pic_scaling_matrix_present_flag,
    },
    .seq_parameter_set_id = (uint8_t) pps->sequence->id,
    .pic_parameter_set_id = (uint8_t) pps->id,
    .num_ref_idx_l0_default_active_minus1 = pps->num_ref_idx_l0_active_minus1,
    .num_ref_idx_l1_default_active_minus1 = pps->num_ref_idx_l1_active_minus1,
    .weighted_bipred_idc = pps->weighted_bipred_idc,
    .pic_init_qp_minus26 = pps->pic_init_qp_minus26,
    .pic_init_qs_minus26 = pps->pic_init_qs_minus26,
    .chroma_qp_index_offset = pps->chroma_qp_index_offset,
    .second_chroma_qp_index_offset =
        (int8_t) pps->second_chroma_qp_index_offset,
    .pScalingLists = vkscaling_lists,
  };
  /* *INDENT-ON* */

  return;
}

static GstFlowReturn
_update_parameters (GstVulkanH264Decoder * self, const GstH264SPS * sps,
    const GstH264PPS * pps)
{
  /* SPS */
  StdVideoH264SequenceParameterSet std_sps;
  StdVideoH264HrdParameters hrd;
  StdVideoH264SequenceParameterSetVui vui;
  StdVideoH264ScalingLists sps_scaling_lists;

  /* PPS */
  StdVideoH264PictureParameterSet std_pps;
  StdVideoH264ScalingLists pps_scaling_lists;

  VkVideoDecodeH264SessionParametersAddInfoKHR params = {
    .sType =
        VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_SESSION_PARAMETERS_ADD_INFO_KHR,
    /* .pNext =  */
    .stdSPSCount = 1,
    .pStdSPSs = &std_sps,
    .stdPPSCount = 1,
    .pStdPPSs = &std_pps,
  };
  VkVideoDecodeH264SessionParametersCreateInfoKHR info = {
    .sType =
        VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_SESSION_PARAMETERS_CREATE_INFO_KHR,
    /* .pNext =  */
    .maxStdSPSCount = params.stdSPSCount,
    .maxStdPPSCount = params.stdPPSCount,
    .pParametersAddInfo = &params,
  };

  GError *error = NULL;

  _fill_sps (sps, &std_sps, &hrd, &vui, &sps_scaling_lists);
  _fill_pps (pps, &std_pps, &pps_scaling_lists);

  if (!gst_vulkan_decoder_update_video_session_parameters (self->decoder,
          &(GstVulkanDecoderParameters) {
          info}
          , &error)) {
    if (error) {
      GST_ERROR_OBJECT (self, "Couldn't set codec parameters: %s",
          error->message);
      g_clear_error (&error);
    }
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

static void
_fill_h264_pic (const GstH264Picture * picture, const GstH264Slice * slice,
    VkVideoDecodeH264PictureInfoKHR * vk_h264pic,
    StdVideoDecodeH264PictureInfo * std_h264pic)
{
  GstH264PPS *pps = slice->header.pps;
  GstH264SPS *sps = pps->sequence;

  /* *INDENT-OFF* */
  *std_h264pic = (StdVideoDecodeH264PictureInfo) {
    .flags = {
      .field_pic_flag = slice->header.field_pic_flag,
      .is_intra = 1,
      .IdrPicFlag = slice->nalu.idr_pic_flag,
      .bottom_field_flag = slice->header.bottom_field_flag,
      .is_reference = GST_H264_PICTURE_IS_REF (picture),
      .complementary_field_pair = picture->second_field,
    },
    .seq_parameter_set_id = sps->id,
    .pic_parameter_set_id = pps->id,
    /* .reserved1 = */
    /* .reserved2 = */
    .frame_num = picture->frame_num,
    .idr_pic_id = picture->idr_pic_id,
    .PicOrderCnt = { picture->top_field_order_cnt,
        picture->bottom_field_order_cnt },
  };

  *vk_h264pic = (VkVideoDecodeH264PictureInfoKHR) {
    .sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PICTURE_INFO_KHR,
    /* .pNext = */
    .pStdPictureInfo = std_h264pic,
    /* .slicesCount = , *//* filled in end_picture() */
    /* .pSlicesDataOffsets = *//* filled in end_picture() */
  };
  /* *INDENT-ON* */
}

static gint32
_find_next_slot_idx (GArray * dpb)
{
  gint32 i;
  guint len;
  GstH264Picture *arr[36] = { NULL, };

  g_assert (dpb->len < 36);

  len = dpb->len;

  for (i = 0; i < len; i++) {
    GstH264Picture *pic = g_array_index (dpb, GstH264Picture *, i);
    GstVulkanH264Picture *h264_pic = gst_h264_picture_get_user_data (pic);
    arr[h264_pic->slot_idx] = pic;
  }

  /* let's return the smallest available / not ref index */
  for (i = 0; i < len; i++) {
    if (!arr[i])
      return i;
  }

  return len;
}

static inline void
_fill_h264_slot (GstH264Picture * picture,
    VkVideoDecodeH264DpbSlotInfoKHR * vkh264_slot,
    StdVideoDecodeH264ReferenceInfo * stdh264_ref)
{
  /* *INDENT-OFF* */
  *stdh264_ref = (StdVideoDecodeH264ReferenceInfo) {
    .flags = {
      .top_field_flag =
          (picture->field == GST_H264_PICTURE_FIELD_TOP_FIELD),
      .bottom_field_flag =
          (picture->field == GST_H264_PICTURE_FIELD_BOTTOM_FIELD),
      .is_non_existing = picture->nonexisting,
      .used_for_long_term_reference =
          GST_H264_PICTURE_IS_LONG_TERM_REF (picture),
    },
    .FrameNum = GST_H264_PICTURE_IS_LONG_TERM_REF (picture) ?
        picture->long_term_pic_num : picture->pic_num,
    /* .reserved = */
    /* .PicOrderCnt = */
  };
  /* *INDENT-ON* */

  switch (picture->field) {
    case GST_H264_PICTURE_FIELD_FRAME:
      stdh264_ref->PicOrderCnt[0] = picture->top_field_order_cnt;
      stdh264_ref->PicOrderCnt[1] = picture->bottom_field_order_cnt;
      break;
    case GST_H264_PICTURE_FIELD_BOTTOM_FIELD:
      if (picture->other_field)
        stdh264_ref->PicOrderCnt[0] = picture->other_field->top_field_order_cnt;
      else
        stdh264_ref->PicOrderCnt[0] = 0;
      stdh264_ref->PicOrderCnt[1] = picture->bottom_field_order_cnt;
      break;
    case GST_H264_PICTURE_FIELD_TOP_FIELD:
      stdh264_ref->PicOrderCnt[0] = picture->top_field_order_cnt;
      if (picture->other_field) {
        stdh264_ref->PicOrderCnt[1] =
            picture->other_field->bottom_field_order_cnt;
      } else {
        stdh264_ref->PicOrderCnt[1] = 0;
      }
      break;
    default:
      break;
  }

  /* *INDENT-OFF* */
  *vkh264_slot = (VkVideoDecodeH264DpbSlotInfoKHR) {
    .sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_DPB_SLOT_INFO_KHR,
    .pStdReferenceInfo = stdh264_ref,
  };
  /* *INDENT-ON* */

}

static inline void
_fill_ref_slot (GstVulkanH264Decoder * self, GstH264Picture * picture,
    VkVideoReferenceSlotInfoKHR * slot, VkVideoPictureResourceInfoKHR * res,
    VkVideoDecodeH264DpbSlotInfoKHR * vkh264_slot,
    StdVideoDecodeH264ReferenceInfo * stdh264_ref,
    GstVulkanDecoderPicture ** ref)
{
  GstVulkanH264Picture *pic;

  _fill_h264_slot (picture, vkh264_slot, stdh264_ref);

  pic = gst_h264_picture_get_user_data (picture);
  g_assert (pic);

  /* *INDENT-OFF* */
  *res = (VkVideoPictureResourceInfoKHR) {
    .sType = VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR,
    .codedOffset = {
      .x = 0,
      .y = 0,
    },
    .codedExtent = {
      .width = self->coded_width,
      .height = self->coded_height,
    },
    .baseArrayLayer = self->decoder->layered_dpb ? pic->slot_idx : 0,
    .imageViewBinding = pic->base.img_view_ref->view,
  };

  *slot = (VkVideoReferenceSlotInfoKHR) {
    .sType = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR,
    .pNext = vkh264_slot,
    .slotIndex = pic->slot_idx,
    .pPictureResource = res,
  };
  /* *INDENT-ON* */

  if (ref)
    *ref = &pic->base;

  GST_TRACE_OBJECT (self, "0x%lx slotIndex: %d", res->imageViewBinding,
      slot->slotIndex);
}

static GstFlowReturn
gst_vulkan_h264_decoder_start_picture (GstH264Decoder * decoder,
    GstH264Picture * picture, GstH264Slice * slice, GstH264Dpb * dpb)
{
  GstVulkanH264Decoder *self = GST_VULKAN_H264_DECODER (decoder);
  GstH264PPS *pps = slice->header.pps;
  GstH264SPS *sps = pps->sequence;
  GstFlowReturn ret;
  GstVulkanH264Picture *pic;
  GArray *refs;
  guint i, j;

  GST_TRACE_OBJECT (self, "Start picture");

  if (self->need_params_update) {
    ret = _update_parameters (self, sps, pps);
    if (ret != GST_FLOW_OK)
      return ret;
    self->need_params_update = FALSE;
  }

  refs = gst_h264_dpb_get_pictures_all (dpb);

  pic = gst_h264_picture_get_user_data (picture);
  g_assert (pic);

  _fill_h264_pic (picture, slice, &pic->vk_h264pic, &pic->std_h264pic);
  pic->slot_idx = _find_next_slot_idx (refs);

  /* fill main slot */
  _fill_ref_slot (self, picture, &pic->base.slot,
      &pic->base.pic_res, &pic->vk_slot, &pic->std_ref, NULL);

  j = 0;

  /* Fill in short-term references */
  for (i = 0; i < refs->len; i++) {
    GstH264Picture *picture = g_array_index (refs, GstH264Picture *, i);
    /* XXX: shall we add second fields? */
    if (GST_H264_PICTURE_IS_SHORT_TERM_REF (picture)) {
      _fill_ref_slot (self, picture, &pic->base.slots[j],
          &pic->base.pics_res[j], &pic->vk_slots[j], &pic->std_refs[j],
          &pic->base.refs[j]);
      j++;
    }
    /* FIXME: do it in O(n) rather O(2n) */
  }

  /* Fill in long-term refs */
  for (i = 0; i < refs->len; i++) {
    GstH264Picture *picture = g_array_index (refs, GstH264Picture *, i);
    /* XXX: shall we add non existing and second fields? */
    if (GST_H264_PICTURE_IS_LONG_TERM_REF (picture)) {
      _fill_ref_slot (self, picture, &pic->base.slots[j],
          &pic->base.pics_res[j], &pic->vk_slots[j], &pic->std_refs[j],
          &pic->base.refs[j]);
      j++;
    }
  }

  g_array_unref (refs);

  /* *INDENT-OFF* */
  pic->base.decode_info = (VkVideoDecodeInfoKHR) {
    .sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_INFO_KHR,
    .pNext = &pic->vk_h264pic,
    .flags = 0x0,
    /* .srcBuffer = */
    .srcBufferOffset = 0,
    /* .srcBufferRange = */
    .dstPictureResource = {
      .sType = VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR,
      .codedOffset = {
        .x = 0,
        .y = 0,
      },
      .codedExtent = {
        .width = self->coded_width,
        .height = self->coded_height,
      },
      .baseArrayLayer = 0,
      .imageViewBinding = pic->base.img_view_out->view,
    },
    .pSetupReferenceSlot = &pic->base.slot,
    .referenceSlotCount = j,
    .pReferenceSlots = pic->base.slots,
  };
  /* *INDENT-ON* */

  /* only wait if there's a buffer processed */
  if (GST_CODEC_PICTURE_FRAME_NUMBER (picture) > 0) {
    if (!gst_vulkan_decoder_wait (self->decoder)) {
      GST_ERROR_OBJECT (self, "Error at waiting for decoding operation to end");
      return GST_FLOW_ERROR;
    }
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_vulkan_h264_decoder_decode_slice (GstH264Decoder * decoder,
    GstH264Picture * picture, GstH264Slice * slice, GArray * ref_pic_list0,
    GArray * ref_pic_list1)
{
  GstVulkanH264Decoder *self = GST_VULKAN_H264_DECODER (decoder);
  GstVulkanH264Picture *pic;

  GST_TRACE_OBJECT (self, "Decode slice");

  pic = gst_h264_picture_get_user_data (picture);
  g_assert (pic);

  if (!gst_vulkan_decoder_append_slice (self->decoder, &pic->base,
          slice->nalu.data + slice->nalu.offset, slice->nalu.size, TRUE))
    return GST_FLOW_ERROR;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_vulkan_h264_decoder_end_picture (GstH264Decoder * decoder,
    GstH264Picture * picture)
{
  GstVulkanH264Decoder *self = GST_VULKAN_H264_DECODER (decoder);
  GstVulkanH264Picture *pic;
  GError *error = NULL;

  GST_TRACE_OBJECT (self, "End picture");

  pic = gst_h264_picture_get_user_data (picture);
  g_assert (pic);

  pic->vk_h264pic.sliceCount = pic->base.slice_offs->len - 1;
  pic->vk_h264pic.pSliceOffsets = (const guint32 *) pic->base.slice_offs->data;

  GST_LOG_OBJECT (self, "Decoding frame, %d bytes %d slices",
      pic->vk_h264pic.pSliceOffsets[pic->vk_h264pic.sliceCount],
      pic->vk_h264pic.sliceCount);

  if (!gst_vulkan_decoder_decode (self->decoder, &pic->base, &error)) {
    GST_ERROR_OBJECT (self, "Couldn't decode frame: %s",
        error ? error->message : "");
    g_clear_error (&error);
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_vulkan_h264_decoder_output_picture (GstH264Decoder * decoder,
    GstVideoCodecFrame * frame, GstH264Picture * picture)
{
  GstVideoDecoder *vdec = GST_VIDEO_DECODER (decoder);
  GstVulkanH264Decoder *self = GST_VULKAN_H264_DECODER (decoder);

  GST_TRACE_OBJECT (self, "Output picture");

  GST_LOG_OBJECT (self,
      "Outputting picture %p (poc %d)", picture, picture->pic_order_cnt);

  if (GST_CODEC_PICTURE (picture)->discont_state) {
    self->need_negotiation = TRUE;
    if (!gst_video_decoder_negotiate (vdec)) {
      gst_h264_picture_unref (picture);
      GST_ERROR_OBJECT (self, "Could not re-negotiate with updated state");
      return GST_FLOW_ERROR;
    }
  }

  gst_h264_picture_unref (picture);

  reset_pipeline_stage_mask (frame->output_buffer);

  return gst_video_decoder_finish_frame (vdec, frame);
}

static void
gst_vulkan_h264_decoder_init (GstVulkanH264Decoder * self)
{
  gst_vulkan_buffer_memory_init_once ();
}

static void
gst_vulkan_h264_decoder_class_init (GstVulkanH264DecoderClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (klass);
  GstH264DecoderClass *h264decoder_class = GST_H264_DECODER_CLASS (klass);

  gst_element_class_set_metadata (element_class, "Vulkan H.264 decoder",
      "Codec/Decoder/Video/Hardware", "A H.264 video decoder based on Vulkan",
      "Víctor Jáquez <vjaquez@igalia.com>");

  gst_element_class_add_static_pad_template (element_class,
      &gst_vulkan_h264dec_sink_template);

  gst_element_class_add_static_pad_template (element_class,
      &gst_vulkan_h264dec_src_template);

  element_class->set_context =
      GST_DEBUG_FUNCPTR (gst_vulkan_h264_decoder_set_context);

  decoder_class->open = GST_DEBUG_FUNCPTR (gst_vulkan_h264_decoder_open);
  decoder_class->close = GST_DEBUG_FUNCPTR (gst_vulkan_h264_decoder_close);
  decoder_class->stop = GST_DEBUG_FUNCPTR (gst_vulkan_h264_decoder_stop);
  decoder_class->src_query =
      GST_DEBUG_FUNCPTR (gst_vulkan_h264_decoder_src_query);
  decoder_class->sink_query =
      GST_DEBUG_FUNCPTR (gst_vulkan_h264_decoder_sink_query);
  decoder_class->negotiate =
      GST_DEBUG_FUNCPTR (gst_vulkan_h264_decoder_negotiate);
  decoder_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_vulkan_h264_decoder_decide_allocation);

  h264decoder_class->new_sequence =
      GST_DEBUG_FUNCPTR (gst_vulkan_h264_decoder_new_sequence);
  h264decoder_class->new_picture =
      GST_DEBUG_FUNCPTR (gst_vulkan_h264_decoder_new_picture);
  h264decoder_class->new_field_picture =
      GST_DEBUG_FUNCPTR (gst_vulkan_h264_decoder_new_field_picture);
  h264decoder_class->start_picture =
      GST_DEBUG_FUNCPTR (gst_vulkan_h264_decoder_start_picture);
  h264decoder_class->decode_slice =
      GST_DEBUG_FUNCPTR (gst_vulkan_h264_decoder_decode_slice);
  h264decoder_class->end_picture =
      GST_DEBUG_FUNCPTR (gst_vulkan_h264_decoder_end_picture);
  h264decoder_class->output_picture =
      GST_DEBUG_FUNCPTR (gst_vulkan_h264_decoder_output_picture);
}
