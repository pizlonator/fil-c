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

#include "vkh265dec.h"

#include <gst/video/video.h>
#include <gst/vulkan/vulkan.h>

#include "gstvulkanelements.h"

typedef struct _GstVulkanH265Decoder GstVulkanH265Decoder;
typedef struct _GstVulkanH265Picture GstVulkanH265Picture;

struct _GstVulkanH265Picture
{
  GstVulkanDecoderPicture base;

  /* 16 is max DPB size */
  /* Picture refs */
  StdVideoDecodeH265ReferenceInfo std_refs[16];
  VkVideoDecodeH265DpbSlotInfoKHR vk_slots[16];

  /* Current picture */
  StdVideoDecodeH265ReferenceInfo std_ref;
  VkVideoDecodeH265DpbSlotInfoKHR vk_slot;

  VkVideoDecodeH265PictureInfoKHR vk_h265pic;
  StdVideoDecodeH265PictureInfo std_h265pic;

  gint32 slot_idx;
};

struct _GstVulkanH265Decoder
{
  GstH265Decoder parent;

  GstVulkanInstance *instance;
  GstVulkanDevice *device;
  GstVulkanQueue *graphic_queue, *decode_queue;

  GstVulkanDecoder *decoder;

  gboolean need_negotiation;
  gboolean need_params_update;

  gint x, y;
  gint width, height;
  gint coded_width, coded_height;
  gint dpb_size;

  VkSamplerYcbcrRange range;
  VkChromaLocation xloc, yloc;

  GstVideoCodecState *output_state;
};

static GstStaticPadTemplate gst_vulkan_h265dec_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-h265, "
        "profile = (string) main,"
        "stream-format = { (string) hvc1, (string) hev1, (string) byte-stream }, "
        "alignment = (string) au"));

static GstStaticPadTemplate gst_vulkan_h265dec_src_template =
GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_VULKAN_IMAGE, "NV12")));

GST_DEBUG_CATEGORY (gst_debug_vulkan_h265_decoder);
#define GST_CAT_DEFAULT gst_debug_vulkan_h265_decoder

#define gst_vulkan_h265_decoder_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstVulkanH265Decoder, gst_vulkan_h265_decoder,
    GST_TYPE_H265_DECODER,
    GST_DEBUG_CATEGORY_INIT (gst_debug_vulkan_h265_decoder,
        "vulkanh265dec", 0, "Vulkan H.265 Decoder"));
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (vulkanh265dec, "vulkanh265dec",
    GST_RANK_NONE, GST_TYPE_VULKAN_H265_DECODER, vulkan_element_init (plugin));

static void
gst_vulkan_h265_decoder_set_context (GstElement * element, GstContext * context)
{
  GstVulkanH265Decoder *self = GST_VULKAN_H265_DECODER (element);

  gst_vulkan_handle_set_context (element, context, NULL, &self->instance);

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

static gboolean
_query_context (GstVulkanH265Decoder * self, GstQuery * query)
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
gst_vulkan_h265_decoder_src_query (GstVideoDecoder * decoder, GstQuery * query)
{
  gboolean ret = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      ret = _query_context (GST_VULKAN_H265_DECODER (decoder), query);
      break;
    default:
      ret = GST_VIDEO_DECODER_CLASS (parent_class)->src_query (decoder, query);
      break;
  }

  return ret;
}

static gboolean
gst_vulkan_h265_decoder_sink_query (GstVideoDecoder * decoder, GstQuery * query)
{
  gboolean ret = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      ret = _query_context (GST_VULKAN_H265_DECODER (decoder), query);
      break;
    default:
      ret = GST_VIDEO_DECODER_CLASS (parent_class)->sink_query (decoder, query);
      break;
  }

  return ret;
}

static gboolean
_find_queues (GstVulkanDevice * device, GstVulkanQueue * queue, gpointer data)
{
  GstVulkanH265Decoder *self = data;
  guint32 flags =
      device->physical_device->queue_family_props[queue->family].queueFlags;
  guint32 codec =
      device->physical_device->queue_family_ops[queue->family].video;

  if (!self->graphic_queue
      && ((flags & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT)) {
    self->graphic_queue = gst_object_ref (queue);
  }

  if (!self->decode_queue
      && ((codec & VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR)
          == VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR)
      && ((flags & VK_QUEUE_VIDEO_DECODE_BIT_KHR)
          == VK_QUEUE_VIDEO_DECODE_BIT_KHR)) {
    self->decode_queue = gst_object_ref (queue);
  }

  return !(self->decode_queue && self->graphic_queue);
}

static gboolean
gst_vulkan_h265_decoder_open (GstVideoDecoder * decoder)
{
  GstVulkanH265Decoder *self = GST_VULKAN_H265_DECODER (decoder);

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
        ("Failed to create/retrieve vulkan H.265 decoder queue"), (NULL));
    return FALSE;
  }

  self->decoder = gst_vulkan_queue_create_decoder (self->decode_queue,
      VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR);
  if (!self->decoder) {
    GST_ELEMENT_ERROR (self, RESOURCE, NOT_FOUND,
        ("Failed to create vulkan H.265 decoder"), (NULL));
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_vulkan_h265_decoder_close (GstVideoDecoder * decoder)
{
  GstVulkanH265Decoder *self = GST_VULKAN_H265_DECODER (decoder);

  gst_clear_object (&self->decoder);
  gst_clear_object (&self->decode_queue);
  gst_clear_object (&self->graphic_queue);
  gst_clear_object (&self->device);
  gst_clear_object (&self->instance);

  return TRUE;
}

static gboolean
gst_vulkan_h265_decoder_stop (GstVideoDecoder * decoder)
{
  GstVulkanH265Decoder *self = GST_VULKAN_H265_DECODER (decoder);

  if (self->decoder)
    gst_vulkan_decoder_stop (self->decoder);

  if (self->output_state)
    gst_video_codec_state_unref (self->output_state);

  return GST_VIDEO_DECODER_CLASS (parent_class)->stop (decoder);
}

static gboolean
gst_vulkan_h265_decoder_negotiate (GstVideoDecoder * decoder)
{
  GstVulkanH265Decoder *self = GST_VULKAN_H265_DECODER (decoder);
  GstH265Decoder *h265dec = GST_H265_DECODER (decoder);
  VkVideoFormatPropertiesKHR format_prop;
  GstVideoFormat format;

  /* Ignore downstream renegotiation request. */
  if (!self->need_negotiation)
    return TRUE;

  if (!gst_vulkan_decoder_out_format (self->decoder, &format_prop))
    return FALSE;

  self->need_negotiation = FALSE;

  if (self->output_state)
    gst_video_codec_state_unref (self->output_state);

  format = gst_vulkan_format_to_video_format (format_prop.format);
  self->output_state = gst_video_decoder_set_interlaced_output_state (decoder,
      format, GST_VIDEO_INTERLACE_MODE_PROGRESSIVE, self->width, self->height,
      h265dec->input_state);

  self->output_state->caps = gst_video_info_to_caps (&self->output_state->info);
  gst_caps_set_features_simple (self->output_state->caps,
      gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_VULKAN_IMAGE, NULL));

  GST_INFO_OBJECT (self, "Negotiated caps %" GST_PTR_FORMAT,
      self->output_state->caps);

  return GST_VIDEO_DECODER_CLASS (parent_class)->negotiate (decoder);
}

static gboolean
gst_vulkan_h265_decoder_decide_allocation (GstVideoDecoder * decoder,
    GstQuery * query)
{
  GstVulkanH265Decoder *self = GST_VULKAN_H265_DECODER (decoder);
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

/* FIXME: dup with h264 */
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

/* FIXME: dup with h264 */
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

static StdVideoH265ProfileIdc
_get_h265_profile (GstH265Profile profile_idc)
{
  switch (profile_idc) {
    case GST_H265_PROFILE_MAIN:
      return STD_VIDEO_H265_PROFILE_IDC_MAIN;
    case GST_H265_PROFILE_MAIN_10:
      return STD_VIDEO_H265_PROFILE_IDC_MAIN_10;
    case GST_H265_PROFILE_MAIN_STILL_PICTURE:
      return STD_VIDEO_H265_PROFILE_IDC_MAIN_STILL_PICTURE;
      /* FIXME */
      /* case GST_H265_PROFILE_XXX: */
      /*   return STD_VIDEO_H265_PROFILE_IDC_FORMAT_RANGE_EXTENSIONS; */
    case GST_H265_PROFILE_SCALABLE_MAIN:
      return STD_VIDEO_H265_PROFILE_IDC_SCC_EXTENSIONS;
    default:
      return STD_VIDEO_H265_PROFILE_IDC_INVALID;
  }
}

static void
gst_vulkan_video_profile_from_h265_sps (GstVulkanVideoProfile * profile,
    const GstH265SPS * sps)
{
  /* *INDENT-OFF* */
  *profile = (GstVulkanVideoProfile) {
    .profile = {
      .sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR,
      .pNext = &profile->usage,
      .videoCodecOperation = VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR,
      .chromaSubsampling = _get_chroma_subsampling_flag (sps->chroma_format_idc),
      .lumaBitDepth = _get_component_bit_depth (sps->bit_depth_luma_minus8 + 8),
      .chromaBitDepth = _get_component_bit_depth (sps->bit_depth_chroma_minus8 + 8),
    },
    .usage.decode = {
      .sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_USAGE_INFO_KHR,
      .videoUsageHints = VK_VIDEO_DECODE_USAGE_DEFAULT_KHR,
      .pNext = &profile->codec,
    },
    .codec.h265dec = {
      .sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PROFILE_INFO_KHR,
      .stdProfileIdc =
        _get_h265_profile (gst_h265_get_profile_from_sps ((GstH265SPS *) sps)),
    },
  };
  /* *INDENT-ON* */
}

static GstFlowReturn
gst_vulkan_h265_decoder_new_sequence (GstH265Decoder * decoder,
    const GstH265SPS * sps, gint max_dpb_size)
{
  GstVulkanH265Decoder *self = GST_VULKAN_H265_DECODER (decoder);
  GstVulkanVideoProfile profile;
  GstVulkanVideoCapabilities vk_caps;
  GError *error = NULL;
  gint x, y, width, height;
  VkFormat old_format = VK_FORMAT_UNDEFINED;
  VkVideoFormatPropertiesKHR format_prop;

  gst_vulkan_video_profile_from_h265_sps (&profile, sps);

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

  if (sps->conformance_window_flag) {
    x = sps->crop_rect_x;
    y = sps->crop_rect_y;
    width = sps->crop_rect_width;
    height = sps->crop_rect_height;
  } else {
    width = sps->width;
    height = sps->height;
    x = y = 0;
  }

  gst_vulkan_decoder_caps (self->decoder, &vk_caps);
  self->coded_width =
      GST_ROUND_UP_N (sps->width, vk_caps.caps.pictureAccessGranularity.width);
  self->coded_height = GST_ROUND_UP_N (sps->height,
      vk_caps.caps.pictureAccessGranularity.height);

  self->need_negotiation &= (x != self->x || y != self->y
      || width != self->width || height != self->height);
  self->x = x;
  self->y = y;
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
      const GstH265VUIParams *vui = &sps->vui_params;

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

static GstVulkanH265Picture *
gst_vulkan_h265_picture_new (GstVulkanH265Decoder * self, GstBuffer * out)
{
  GstVulkanH265Picture *pic;

  pic = g_new0 (GstVulkanH265Picture, 1);
  gst_vulkan_decoder_picture_init (self->decoder, &pic->base, out);
  reset_pipeline_stage_mask (out);

  return pic;
}

static void
gst_vulkan_h265_picture_free (gpointer data)
{
  GstVulkanH265Picture *pic = data;

  gst_vulkan_decoder_picture_release (&pic->base);
  g_free (pic);
}

static GstFlowReturn
gst_vulkan_h265_decoder_new_picture (GstH265Decoder * decoder,
    GstVideoCodecFrame * frame, GstH265Picture * picture)
{
  GstVulkanH265Decoder *self = GST_VULKAN_H265_DECODER (decoder);
  GstVideoDecoder *vdec = GST_VIDEO_DECODER (decoder);
  GstFlowReturn ret;
  GstVulkanH265Picture *pic;

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

  pic = gst_vulkan_h265_picture_new (self, frame->output_buffer);
  gst_h265_picture_set_user_data (picture, pic, gst_vulkan_h265_picture_free);

  return GST_FLOW_OK;

allocation_failed:
  {
    GST_WARNING_OBJECT (self, "Failed to allocated input or output buffer: %s",
        gst_flow_get_name (ret));
    return ret;
  }
}

struct SPS
{
  StdVideoH265SequenceParameterSet sps;
  StdVideoH265ScalingLists scaling;
  StdVideoH265HrdParameters vui_header;
  StdVideoH265SequenceParameterSetVui vui;
  StdVideoH265ProfileTierLevel ptl;
  StdVideoH265DecPicBufMgr dpbm;
  StdVideoH265PredictorPaletteEntries pal;
  StdVideoH265SubLayerHrdParameters nal_hrd[GST_H265_MAX_SUB_LAYERS];
  StdVideoH265SubLayerHrdParameters vcl_hrd[GST_H265_MAX_SUB_LAYERS];
  /* 7.4.3.2.1: num_short_term_ref_pic_sets is in [0, 64]. */
  StdVideoH265ShortTermRefPicSet str[64];
  StdVideoH265LongTermRefPicsSps ltr;
};

struct PPS
{
  StdVideoH265PictureParameterSet pps;
  StdVideoH265ScalingLists scaling;
  StdVideoH265PredictorPaletteEntries pal;
};

struct VPS
{
  StdVideoH265VideoParameterSet vps;
  StdVideoH265ProfileTierLevel ptl;
  StdVideoH265DecPicBufMgr dpbm;
  /* FIXME: a VPS can have multiple header params, each with its own nal and vlc
     headers sets. Sadly, that's not currently supported by the GStreamer H265
     parser, which only supports one header params per VPS. */
  StdVideoH265HrdParameters hrd;
  StdVideoH265SubLayerHrdParameters nal_hdr[GST_H265_MAX_SUB_LAYERS];
  StdVideoH265SubLayerHrdParameters vcl_hdr[GST_H265_MAX_SUB_LAYERS];
};

static void
_copy_scaling_list (const GstH265ScalingList * scaling_list,
    StdVideoH265ScalingLists * vk_scaling_lists)
{
  int i;

  for (i = 0; i < STD_VIDEO_H265_SCALING_LIST_4X4_NUM_LISTS; i++) {
    gst_h265_quant_matrix_4x4_get_raster_from_uprightdiagonal
        (vk_scaling_lists->ScalingList4x4[i],
        scaling_list->scaling_lists_4x4[i]);
  }

  for (i = 0; i < STD_VIDEO_H265_SCALING_LIST_8X8_NUM_LISTS; i++) {
    gst_h265_quant_matrix_8x8_get_raster_from_uprightdiagonal
        (vk_scaling_lists->ScalingList8x8[i],
        scaling_list->scaling_lists_8x8[i]);
  }

  for (i = 0; i < STD_VIDEO_H265_SCALING_LIST_16X16_NUM_LISTS; i++) {
    gst_h265_quant_matrix_16x16_get_raster_from_uprightdiagonal
        (vk_scaling_lists->ScalingList16x16[i],
        scaling_list->scaling_lists_16x16[i]);
  }

  for (i = 0; i < STD_VIDEO_H265_SCALING_LIST_32X32_NUM_LISTS; i++) {
    gst_h265_quant_matrix_32x32_get_raster_from_uprightdiagonal
        (vk_scaling_lists->ScalingList32x32[i],
        scaling_list->scaling_lists_32x32[i]);
  }

  for (i = 0; i < STD_VIDEO_H265_SCALING_LIST_16X16_NUM_LISTS; i++) {
    vk_scaling_lists->ScalingListDCCoef16x16[i] =
        scaling_list->scaling_list_dc_coef_minus8_16x16[i] + 8;
  }

  for (i = 0; i < STD_VIDEO_H265_SCALING_LIST_32X32_NUM_LISTS; i++) {
    vk_scaling_lists->ScalingListDCCoef32x32[i] =
        scaling_list->scaling_list_dc_coef_minus8_32x32[i] + 8;
  }
}

static guint32
_array_2_bitmask (const guint8 * array, guint8 size)
{
  guint8 i;
  guint32 bitmask = 0;

  for (i = 0; i < MIN (size, 32); i++)
    bitmask |= !!array[i] << i;
  return bitmask;
}

static StdVideoH265LevelIdc
_get_h265_level_idc (int level_idc)
{
  switch (level_idc) {
    case 10:
      return STD_VIDEO_H265_LEVEL_IDC_1_0;
    case 20:
      return STD_VIDEO_H265_LEVEL_IDC_2_0;
    case 21:
      return STD_VIDEO_H265_LEVEL_IDC_2_1;
    case 30:
      return STD_VIDEO_H265_LEVEL_IDC_3_0;
    case 31:
      return STD_VIDEO_H265_LEVEL_IDC_3_1;
    case 40:
      return STD_VIDEO_H265_LEVEL_IDC_4_0;
    case 41:
      return STD_VIDEO_H265_LEVEL_IDC_4_1;
    case 50:
      return STD_VIDEO_H265_LEVEL_IDC_5_0;
    case 51:
      return STD_VIDEO_H265_LEVEL_IDC_5_1;
    case 52:
      return STD_VIDEO_H265_LEVEL_IDC_5_2;
    case 60:
      return STD_VIDEO_H265_LEVEL_IDC_6_0;
    case 61:
      return STD_VIDEO_H265_LEVEL_IDC_6_1;
    default:
    case 62:
      return STD_VIDEO_H265_LEVEL_IDC_6_2;
  }
}

static void
_copy_sub_layers_hrd_params (const GstH265SubLayerHRDParams * params,
    StdVideoH265SubLayerHrdParameters * vkparams, int num_params)
{
  int i;

  for (i = 0; i < num_params; i++) {
    memcpy (vkparams[i].bit_rate_du_value_minus1,
        params[i].bit_rate_du_value_minus1, STD_VIDEO_H265_CPB_CNT_LIST_SIZE
        * sizeof (*vkparams[i].bit_rate_du_value_minus1));
    memcpy (vkparams[i].bit_rate_value_minus1, params[i].bit_rate_value_minus1,
        STD_VIDEO_H265_CPB_CNT_LIST_SIZE
        * sizeof (*vkparams[i].bit_rate_value_minus1));
    memcpy (vkparams[i].cpb_size_value_minus1, params[i].cpb_size_value_minus1,
        STD_VIDEO_H265_CPB_CNT_LIST_SIZE
        * sizeof (*vkparams[i].cpb_size_value_minus1));
    memcpy (vkparams[i].cpb_size_du_value_minus1,
        params[i].cpb_size_du_value_minus1, STD_VIDEO_H265_CPB_CNT_LIST_SIZE
        * sizeof (*vkparams[i].cpb_size_du_value_minus1));

    vkparams[i].cbr_flag =
        _array_2_bitmask (params[i].cbr_flag, STD_VIDEO_H265_CPB_CNT_LIST_SIZE);
  }
}

static void
_copy_profile_tier_level (const GstH265ProfileTierLevel * ptl,
    StdVideoH265ProfileTierLevel * vk_ptl)
{
  /* *INDENT-OFF* */
  *vk_ptl = (StdVideoH265ProfileTierLevel) {
    .flags = {
      .general_tier_flag = ptl->tier_flag,
      .general_progressive_source_flag = ptl->progressive_source_flag,
      .general_interlaced_source_flag = ptl->interlaced_source_flag,
      .general_non_packed_constraint_flag = ptl->non_packed_constraint_flag,
      .general_frame_only_constraint_flag = ptl->frame_only_constraint_flag,
    },
    .general_profile_idc = _get_h265_profile (ptl->profile_idc),
    .general_level_idc = _get_h265_level_idc (ptl->level_idc),
  };
  /* *INDENT-ON* */
}

static void
_fill_sps (const GstH265SPS * sps, struct SPS *std_sps)
{
  int i;
  const GstH265VUIParams *vui_params = &sps->vui_params;

  _copy_scaling_list (&sps->scaling_list, &std_sps->scaling);

  if (vui_params->hrd_params.nal_hrd_parameters_present_flag) {
    _copy_sub_layers_hrd_params (vui_params->hrd_params.sublayer_hrd_params,
        std_sps->nal_hrd, STD_VIDEO_H265_SUBLAYERS_LIST_SIZE);
  }
  if (vui_params->hrd_params.vcl_hrd_parameters_present_flag) {
    _copy_sub_layers_hrd_params (vui_params->hrd_params.sublayer_hrd_params,
        std_sps->vcl_hrd, STD_VIDEO_H265_SUBLAYERS_LIST_SIZE);
  }
  std_sps->vui_header = (StdVideoH265HrdParameters) {
    /* *INDENT-OFF* */
    .flags = {
      .nal_hrd_parameters_present_flag =
          vui_params->hrd_params.nal_hrd_parameters_present_flag,
      .vcl_hrd_parameters_present_flag =
          vui_params->hrd_params.vcl_hrd_parameters_present_flag,
      .sub_pic_hrd_params_present_flag =
          vui_params->hrd_params.sub_pic_hrd_params_present_flag,
      .sub_pic_cpb_params_in_pic_timing_sei_flag =
          vui_params->hrd_params.sub_pic_cpb_params_in_pic_timing_sei_flag,
      .fixed_pic_rate_general_flag =
          _array_2_bitmask (vui_params->hrd_params.fixed_pic_rate_general_flag,
          sizeof (vui_params->hrd_params.fixed_pic_rate_general_flag)),
      .fixed_pic_rate_within_cvs_flag =
          _array_2_bitmask (vui_params->hrd_params.fixed_pic_rate_within_cvs_flag,
          sizeof (vui_params->hrd_params.fixed_pic_rate_within_cvs_flag)),
      .low_delay_hrd_flag =
          _array_2_bitmask (vui_params->hrd_params.low_delay_hrd_flag,
          sizeof (vui_params->hrd_params.low_delay_hrd_flag)),
    },
    .tick_divisor_minus2 = vui_params->hrd_params.tick_divisor_minus2,
    .du_cpb_removal_delay_increment_length_minus1 =
        vui_params->hrd_params.du_cpb_removal_delay_increment_length_minus1,
    .dpb_output_delay_du_length_minus1 =
        vui_params->hrd_params.dpb_output_delay_du_length_minus1,
    .bit_rate_scale = vui_params->hrd_params.bit_rate_scale,
    .cpb_size_scale = vui_params->hrd_params.cpb_size_scale,
    .cpb_size_du_scale = vui_params->hrd_params.cpb_size_du_scale,
    .initial_cpb_removal_delay_length_minus1 =
        vui_params->hrd_params.initial_cpb_removal_delay_length_minus1,
    .au_cpb_removal_delay_length_minus1 =
        vui_params->hrd_params.au_cpb_removal_delay_length_minus1,
    .dpb_output_delay_length_minus1 =
        vui_params->hrd_params.dpb_output_delay_length_minus1,
    .pSubLayerHrdParametersNal =
        (const StdVideoH265SubLayerHrdParameters *) &std_sps->nal_hrd,
    .pSubLayerHrdParametersVcl =
        (const StdVideoH265SubLayerHrdParameters *) &std_sps->vcl_hrd,
    /* *INDENT-ON* */
  };

  /* *INDENT-OFF* */
  std_sps->vui = (StdVideoH265SequenceParameterSetVui) {
    .flags =  {
      .aspect_ratio_info_present_flag =
          vui_params->aspect_ratio_info_present_flag,
      .overscan_info_present_flag = vui_params->overscan_info_present_flag,
      .overscan_appropriate_flag = vui_params->overscan_appropriate_flag,
      .video_signal_type_present_flag =
          vui_params->video_signal_type_present_flag,
      .video_full_range_flag = vui_params->video_full_range_flag,
      .colour_description_present_flag =
          vui_params->colour_description_present_flag,
      .chroma_loc_info_present_flag = vui_params->chroma_loc_info_present_flag,
      .neutral_chroma_indication_flag =
          vui_params->neutral_chroma_indication_flag,
      .field_seq_flag = vui_params->field_seq_flag,
      .frame_field_info_present_flag =
          vui_params->frame_field_info_present_flag,
      .default_display_window_flag = vui_params->default_display_window_flag,
      .vui_timing_info_present_flag = vui_params->timing_info_present_flag,
      .vui_poc_proportional_to_timing_flag =
          vui_params->poc_proportional_to_timing_flag,
      .vui_hrd_parameters_present_flag =
          vui_params->hrd_parameters_present_flag,
      .bitstream_restriction_flag = vui_params->bitstream_restriction_flag,
      .tiles_fixed_structure_flag = vui_params->tiles_fixed_structure_flag,
      .motion_vectors_over_pic_boundaries_flag =
          vui_params->motion_vectors_over_pic_boundaries_flag,
      .restricted_ref_pic_lists_flag =
          vui_params->restricted_ref_pic_lists_flag,
    },
    .aspect_ratio_idc = vui_params->aspect_ratio_idc,
    .sar_width = vui_params->sar_width,
    .sar_height = vui_params->sar_height,
    .video_format = vui_params->video_format,
    .colour_primaries = vui_params->colour_primaries,
    .transfer_characteristics = vui_params->transfer_characteristics,
    .matrix_coeffs = vui_params->matrix_coefficients,
    .chroma_sample_loc_type_top_field =
        vui_params->chroma_sample_loc_type_top_field,
    .chroma_sample_loc_type_bottom_field =
        vui_params->chroma_sample_loc_type_bottom_field,
    /* Reserved */
    /* Reserved */
    .def_disp_win_left_offset = vui_params->def_disp_win_left_offset,
    .def_disp_win_right_offset = vui_params->def_disp_win_right_offset,
    .def_disp_win_top_offset = vui_params->def_disp_win_top_offset,
    .def_disp_win_bottom_offset = vui_params->def_disp_win_bottom_offset,
    .vui_num_units_in_tick = vui_params->num_units_in_tick,
    .vui_time_scale = vui_params->time_scale,
    .vui_num_ticks_poc_diff_one_minus1 =
       vui_params->num_ticks_poc_diff_one_minus1,
    .min_spatial_segmentation_idc = vui_params->min_spatial_segmentation_idc,
    .max_bytes_per_pic_denom = vui_params->max_bytes_per_pic_denom,
    .max_bits_per_min_cu_denom = vui_params->max_bits_per_min_cu_denom,
    .log2_max_mv_length_horizontal = vui_params->log2_max_mv_length_horizontal,
    .log2_max_mv_length_vertical = vui_params->log2_max_mv_length_vertical,
    .pHrdParameters = &std_sps->vui_header,
  };
  /* *INDENT-ON* */

  _copy_profile_tier_level (&sps->profile_tier_level, &std_sps->ptl);

  memcpy (std_sps->dpbm.max_latency_increase_plus1,
      sps->max_latency_increase_plus1, GST_H265_MAX_SUB_LAYERS
      * sizeof (*std_sps->dpbm.max_latency_increase_plus1));
  memcpy (std_sps->dpbm.max_dec_pic_buffering_minus1,
      sps->max_dec_pic_buffering_minus1, GST_H265_MAX_SUB_LAYERS
      * sizeof (*std_sps->dpbm.max_dec_pic_buffering_minus1));
  memcpy (std_sps->dpbm.max_num_reorder_pics,
      sps->max_num_reorder_pics, GST_H265_MAX_SUB_LAYERS
      * sizeof (*std_sps->dpbm.max_num_reorder_pics));

  memcpy (std_sps->pal.PredictorPaletteEntries,
      sps->sps_scc_extension_params.sps_palette_predictor_initializer,
      STD_VIDEO_H265_PREDICTOR_PALETTE_COMPONENTS_LIST_SIZE
      * sizeof (*std_sps->pal.PredictorPaletteEntries));

  for (i = 0; i < sps->num_short_term_ref_pic_sets; i++) {
    /* *INDENT-OFF* */
    std_sps->str[i] = (StdVideoH265ShortTermRefPicSet) {
      .flags = {
        .inter_ref_pic_set_prediction_flag =
            sps->short_term_ref_pic_set[i].inter_ref_pic_set_prediction_flag,
        .delta_rps_sign = sps->short_term_ref_pic_set[i].delta_rps_sign,
      },
      .delta_idx_minus1 = sps->short_term_ref_pic_set[i].delta_idx_minus1,
      .use_delta_flag = 0,  /* FIXME: not exposed by parser and cannot deduce */
      .abs_delta_rps_minus1 =
          sps->short_term_ref_pic_set[i].abs_delta_rps_minus1,
      .used_by_curr_pic_flag  = 0, /* FIXME: not exposed by parser and cannot deduce */
      .used_by_curr_pic_s0_flag =
          _array_2_bitmask (sps->short_term_ref_pic_set[i].UsedByCurrPicS0,
          sps->short_term_ref_pic_set[i].NumDeltaPocs),
      .used_by_curr_pic_s1_flag =
          _array_2_bitmask (sps->short_term_ref_pic_set[i].UsedByCurrPicS0,
          sps->short_term_ref_pic_set[i].NumDeltaPocs),
      /* Reserved */
      /* Reserved */
      /* Reserved */
      .num_negative_pics = sps->short_term_ref_pic_set[i].NumNegativePics,
      .num_positive_pics = sps->short_term_ref_pic_set[i].NumPositivePics,
    };
    /* *INDENT-ON* */
  }

  std_sps->ltr.used_by_curr_pic_lt_sps_flag =
      _array_2_bitmask (sps->used_by_curr_pic_lt_sps_flag,
      sps->num_long_term_ref_pics_sps);
  memcpy (std_sps->ltr.lt_ref_pic_poc_lsb_sps, sps->lt_ref_pic_poc_lsb_sps,
      32 * sizeof (*std_sps->ltr.lt_ref_pic_poc_lsb_sps));

  /* *INDENT-OFF* */
  std_sps->sps = (StdVideoH265SequenceParameterSet) {
    .flags = {
      .sps_temporal_id_nesting_flag = sps->temporal_id_nesting_flag,
      .separate_colour_plane_flag = sps->separate_colour_plane_flag,
      .conformance_window_flag = sps->conformance_window_flag,
      .sps_sub_layer_ordering_info_present_flag =
          sps->sub_layer_ordering_info_present_flag,
      .scaling_list_enabled_flag = sps->scaling_list_enabled_flag,
      .sps_scaling_list_data_present_flag = sps->scaling_list_enabled_flag,
      .amp_enabled_flag = sps->amp_enabled_flag,
      .sample_adaptive_offset_enabled_flag =
          sps->sample_adaptive_offset_enabled_flag,
      .pcm_enabled_flag = sps->pcm_enabled_flag,
      .pcm_loop_filter_disabled_flag = sps->pcm_loop_filter_disabled_flag,
      .long_term_ref_pics_present_flag = sps->long_term_ref_pics_present_flag,
      .sps_temporal_mvp_enabled_flag = sps->temporal_mvp_enabled_flag,
      .strong_intra_smoothing_enabled_flag =
          sps->strong_intra_smoothing_enabled_flag,
      .vui_parameters_present_flag = sps->vui_parameters_present_flag,
      .sps_extension_present_flag = sps->sps_extension_flag,
      .sps_range_extension_flag = sps->sps_range_extension_flag,
      .transform_skip_rotation_enabled_flag =
          sps->sps_extension_params.transform_skip_rotation_enabled_flag,
      .transform_skip_context_enabled_flag =
          sps->sps_extension_params.transform_skip_context_enabled_flag,
      .implicit_rdpcm_enabled_flag =
          sps->sps_extension_params.implicit_rdpcm_enabled_flag,
      .explicit_rdpcm_enabled_flag =
          sps->sps_extension_params.explicit_rdpcm_enabled_flag,
      .extended_precision_processing_flag =
          sps->sps_extension_params.extended_precision_processing_flag,
      .intra_smoothing_disabled_flag =
          sps->sps_extension_params.intra_smoothing_disabled_flag,
      .high_precision_offsets_enabled_flag =
          sps->sps_extension_params.high_precision_offsets_enabled_flag,
      .persistent_rice_adaptation_enabled_flag =
          sps->sps_extension_params.persistent_rice_adaptation_enabled_flag,
      .cabac_bypass_alignment_enabled_flag =
          sps->sps_extension_params.cabac_bypass_alignment_enabled_flag,
      .sps_scc_extension_flag = sps->sps_scc_extension_flag,
      .sps_curr_pic_ref_enabled_flag =
          sps->sps_scc_extension_params.sps_curr_pic_ref_enabled_flag,
      .palette_mode_enabled_flag =
          sps->sps_scc_extension_params.palette_mode_enabled_flag,
      .sps_palette_predictor_initializers_present_flag =
          sps->sps_scc_extension_params.sps_palette_predictor_initializers_present_flag,
      .intra_boundary_filtering_disabled_flag =
          sps->sps_scc_extension_params.intra_boundary_filtering_disabled_flag,
    },
    .chroma_format_idc = sps->chroma_format_idc,
    .pic_width_in_luma_samples = sps->width,
    .pic_height_in_luma_samples = sps->height,
    .sps_video_parameter_set_id = sps->vps_id,
    .sps_max_sub_layers_minus1 = sps->max_sub_layers_minus1,
    .sps_seq_parameter_set_id = sps->id,
    .bit_depth_luma_minus8 = sps->bit_depth_luma_minus8,
    .bit_depth_chroma_minus8 = sps->bit_depth_chroma_minus8,
    .log2_max_pic_order_cnt_lsb_minus4 = sps->log2_max_pic_order_cnt_lsb_minus4,
    .log2_min_luma_coding_block_size_minus3 =
        sps->log2_min_luma_coding_block_size_minus3,
    .log2_diff_max_min_luma_coding_block_size =
        sps->log2_diff_max_min_luma_coding_block_size,
    .log2_min_luma_transform_block_size_minus2 =
        sps->log2_min_transform_block_size_minus2,
    .log2_diff_max_min_luma_transform_block_size =
        sps->log2_diff_max_min_transform_block_size,
    .max_transform_hierarchy_depth_inter =
        sps->max_transform_hierarchy_depth_inter,
    .max_transform_hierarchy_depth_intra =
        sps->max_transform_hierarchy_depth_intra,
    .num_short_term_ref_pic_sets = sps->num_short_term_ref_pic_sets,
    .num_long_term_ref_pics_sps = sps->num_long_term_ref_pics_sps,
    .pcm_sample_bit_depth_luma_minus1 = sps->pcm_sample_bit_depth_luma_minus1,
    .pcm_sample_bit_depth_chroma_minus1 =
        sps->pcm_sample_bit_depth_chroma_minus1,
    .log2_min_pcm_luma_coding_block_size_minus3 =
        sps->log2_min_pcm_luma_coding_block_size_minus3,
    .log2_diff_max_min_pcm_luma_coding_block_size =
        sps->log2_diff_max_min_pcm_luma_coding_block_size,
    /* Reserved */
    /* Reserved */
    .palette_max_size = sps->sps_scc_extension_params.palette_max_size,
    .delta_palette_max_predictor_size =
        sps->sps_scc_extension_params.delta_palette_max_predictor_size,
    .motion_vector_resolution_control_idc =
        sps->sps_scc_extension_params.motion_vector_resolution_control_idc,
    .sps_num_palette_predictor_initializers_minus1 =
        sps->sps_scc_extension_params.sps_num_palette_predictor_initializer_minus1,
    .conf_win_left_offset = sps->conf_win_left_offset,
    .conf_win_right_offset = sps->conf_win_right_offset,
    .conf_win_top_offset = sps->conf_win_top_offset,
    .conf_win_bottom_offset = sps->conf_win_bottom_offset,
    .pProfileTierLevel = &std_sps->ptl,
    .pDecPicBufMgr = &std_sps->dpbm,
    .pScalingLists = &std_sps->scaling,
    .pShortTermRefPicSet = std_sps->str,
    .pLongTermRefPicsSps = &std_sps->ltr,
    .pSequenceParameterSetVui = &std_sps->vui,
    .pPredictorPaletteEntries = &std_sps->pal,
  };
  /* *INDENT-ON* */
}

static void
_fill_pps (const GstH265PPS * pps, const GstH265SPS * sps, struct PPS *std_pps)
{
  int i, j;

  _copy_scaling_list (&pps->scaling_list, &std_pps->scaling);

  /* *INDENT-OFF* */
  std_pps->pps = (StdVideoH265PictureParameterSet) {
    .flags = {
      .dependent_slice_segments_enabled_flag =
          pps->dependent_slice_segments_enabled_flag,
      .output_flag_present_flag = pps->output_flag_present_flag,
      .sign_data_hiding_enabled_flag = pps->sign_data_hiding_enabled_flag,
      .cabac_init_present_flag = pps->cabac_init_present_flag,
      .constrained_intra_pred_flag = pps->constrained_intra_pred_flag,
      .transform_skip_enabled_flag = pps->transform_skip_enabled_flag,
      .cu_qp_delta_enabled_flag = pps->cu_qp_delta_enabled_flag,
      .pps_slice_chroma_qp_offsets_present_flag =
          pps->slice_chroma_qp_offsets_present_flag,
      .weighted_pred_flag = pps->weighted_pred_flag,
      .weighted_bipred_flag = pps->weighted_bipred_flag,
      .transquant_bypass_enabled_flag = pps->transquant_bypass_enabled_flag,
      .tiles_enabled_flag = pps->tiles_enabled_flag,
      .entropy_coding_sync_enabled_flag = pps->entropy_coding_sync_enabled_flag,
      .uniform_spacing_flag = pps->uniform_spacing_flag,
      .loop_filter_across_tiles_enabled_flag =
          pps->loop_filter_across_tiles_enabled_flag,
      .pps_loop_filter_across_slices_enabled_flag =
          pps->loop_filter_across_slices_enabled_flag,
      .deblocking_filter_control_present_flag =
          pps->deblocking_filter_control_present_flag,
      .deblocking_filter_override_enabled_flag =
          pps->deblocking_filter_override_enabled_flag,
      .pps_deblocking_filter_disabled_flag =
          pps->deblocking_filter_disabled_flag,
      .pps_scaling_list_data_present_flag = pps->scaling_list_data_present_flag,
      .lists_modification_present_flag = pps->lists_modification_present_flag,
      .slice_segment_header_extension_present_flag =
          pps->slice_segment_header_extension_present_flag,
      .pps_extension_present_flag = pps->pps_extension_flag,
      .cross_component_prediction_enabled_flag =
          pps->pps_extension_params.cross_component_prediction_enabled_flag,
      .chroma_qp_offset_list_enabled_flag =
          pps->pps_extension_params.chroma_qp_offset_list_enabled_flag,
      .pps_curr_pic_ref_enabled_flag =
          pps->pps_scc_extension_params.pps_curr_pic_ref_enabled_flag,
      .residual_adaptive_colour_transform_enabled_flag =
          pps->pps_scc_extension_params.residual_adaptive_colour_transform_enabled_flag,
      .pps_slice_act_qp_offsets_present_flag =
          pps->pps_scc_extension_params.pps_slice_act_qp_offsets_present_flag,
      .pps_palette_predictor_initializers_present_flag =
          pps->pps_scc_extension_params.pps_palette_predictor_initializers_present_flag,
      .monochrome_palette_flag =
          pps->pps_scc_extension_params.monochrome_palette_flag,
      .pps_range_extension_flag = pps->pps_range_extension_flag,
    },
    .pps_pic_parameter_set_id = pps->id,
    .pps_seq_parameter_set_id = pps->sps_id,
    .sps_video_parameter_set_id = sps->vps_id,
    .num_extra_slice_header_bits = pps->num_extra_slice_header_bits,
    .num_ref_idx_l0_default_active_minus1 =
        pps->num_ref_idx_l0_default_active_minus1,
    .num_ref_idx_l1_default_active_minus1 =
        pps->num_ref_idx_l1_default_active_minus1,
    .init_qp_minus26 = pps->init_qp_minus26,
    .diff_cu_qp_delta_depth = pps->diff_cu_qp_delta_depth,
    .pps_cb_qp_offset = pps->cb_qp_offset,
    .pps_cr_qp_offset = pps->cr_qp_offset,
    .pps_beta_offset_div2 = pps->beta_offset_div2,
    .pps_tc_offset_div2 = pps->tc_offset_div2,
    .log2_parallel_merge_level_minus2 = pps->log2_parallel_merge_level_minus2,
    .log2_max_transform_skip_block_size_minus2 =
        pps->pps_extension_params.log2_max_transform_skip_block_size_minus2,
    .diff_cu_chroma_qp_offset_depth =
        pps->pps_extension_params.diff_cu_chroma_qp_offset_depth,
    .chroma_qp_offset_list_len_minus1 =
        pps->pps_extension_params.chroma_qp_offset_list_len_minus1,
    .log2_sao_offset_scale_luma =
        pps->pps_extension_params.log2_sao_offset_scale_luma,
    .log2_sao_offset_scale_chroma =
        pps->pps_extension_params.log2_sao_offset_scale_chroma,
    .pps_act_y_qp_offset_plus5 =
        pps->pps_scc_extension_params.pps_act_y_qp_offset_plus5,
    .pps_act_cb_qp_offset_plus5 =
        pps->pps_scc_extension_params.pps_act_cb_qp_offset_plus5,
    .pps_act_cr_qp_offset_plus3 =
        pps->pps_scc_extension_params.pps_act_cr_qp_offset_plus3,
    .pps_num_palette_predictor_initializers =
        pps->pps_scc_extension_params.pps_num_palette_predictor_initializer,
    .luma_bit_depth_entry_minus8 =
        pps->pps_scc_extension_params.luma_bit_depth_entry_minus8,
    .chroma_bit_depth_entry_minus8 =
        pps->pps_scc_extension_params.chroma_bit_depth_entry_minus8,
    .num_tile_columns_minus1 = pps->num_tile_columns_minus1,
    .num_tile_rows_minus1 = pps->num_tile_rows_minus1,
    .pScalingLists = &std_pps->scaling,
    .pPredictorPaletteEntries = &std_pps->pal,
  };
  /* *INDENT-OFF* */

  for (i = 0; i < (pps->pps_scc_extension_params.monochrome_palette_flag ? 1 : 3); i++) {
    for (j = 0; j < pps->pps_scc_extension_params.pps_num_palette_predictor_initializer; j++) {
      std_pps->pal.PredictorPaletteEntries[i][j] =
         pps->pps_scc_extension_params.pps_palette_predictor_initializer[i][j];
    }
  }

  for (i = 0; i < pps->num_tile_columns_minus1 + 1; i++)
    std_pps->pps.column_width_minus1[i] = pps->column_width_minus1[i];

  for (i = 0; i < pps->num_tile_rows_minus1 + 1; i++)
    std_pps->pps.row_height_minus1[i] = pps->row_height_minus1[i];

  for ( i = 0; i <= pps->pps_extension_params.chroma_qp_offset_list_len_minus1 + 1; i++) {
    std_pps->pps.cb_qp_offset_list[i] =
        pps->pps_extension_params.cb_qp_offset_list[i];
    std_pps->pps.cr_qp_offset_list[i] =
        pps->pps_extension_params.cr_qp_offset_list[i];
  }
}

static void
_fill_vps (const GstH265VPS * vps, struct VPS * std_vps)
{
  const GstH265HRDParams *hrd = &vps->hrd_params;

  if (vps->num_hrd_parameters > 1)
    GST_FIXME ("H.265 parser only supports one header parameters per VPS");

  if (hrd->nal_hrd_parameters_present_flag) {
    _copy_sub_layers_hrd_params (hrd->sublayer_hrd_params, std_vps->nal_hdr,
        GST_H265_MAX_SUB_LAYERS);
  }
  if (hrd->vcl_hrd_parameters_present_flag) {
    _copy_sub_layers_hrd_params (hrd->sublayer_hrd_params, std_vps->vcl_hdr,
        GST_H265_MAX_SUB_LAYERS);
  }

  /* for (i = 0; i < vps->num_hrd_parameters; i++) { */
  /* *INDENT-OFF* */
  std_vps->hrd = (StdVideoH265HrdParameters) {
    .flags = {
      .nal_hrd_parameters_present_flag = hrd->nal_hrd_parameters_present_flag,
      .vcl_hrd_parameters_present_flag = hrd->vcl_hrd_parameters_present_flag,
      .sub_pic_hrd_params_present_flag = hrd->sub_pic_hrd_params_present_flag,
      .sub_pic_cpb_params_in_pic_timing_sei_flag =
          hrd->sub_pic_cpb_params_in_pic_timing_sei_flag,
      .fixed_pic_rate_general_flag =
          _array_2_bitmask (hrd->fixed_pic_rate_general_flag, 7),
      .fixed_pic_rate_within_cvs_flag =
          _array_2_bitmask (hrd->fixed_pic_rate_within_cvs_flag, 7),
      .low_delay_hrd_flag = _array_2_bitmask (hrd->low_delay_hrd_flag, 7),
    },
    .tick_divisor_minus2 = hrd->tick_divisor_minus2,
    .du_cpb_removal_delay_increment_length_minus1 = hrd->du_cpb_removal_delay_increment_length_minus1,
    .dpb_output_delay_du_length_minus1 = hrd->dpb_output_delay_du_length_minus1,
    .bit_rate_scale = hrd->bit_rate_scale,
    .cpb_size_scale = hrd->cpb_size_scale,
    .cpb_size_du_scale = hrd->cpb_size_du_scale,
    .initial_cpb_removal_delay_length_minus1 = hrd->initial_cpb_removal_delay_length_minus1,
    .au_cpb_removal_delay_length_minus1 = hrd->au_cpb_removal_delay_length_minus1,
    .dpb_output_delay_length_minus1 = hrd->dpb_output_delay_length_minus1,
    /* Reserved: uint16_t[3] */
    .pSubLayerHrdParametersNal = std_vps->nal_hdr,
    .pSubLayerHrdParametersVcl = std_vps->vcl_hdr,
  };
  /* *INDENT-ON* */

  _copy_profile_tier_level (&vps->profile_tier_level, &std_vps->ptl);

  memcpy (std_vps->dpbm.max_latency_increase_plus1,
      vps->max_latency_increase_plus1, GST_H265_MAX_SUB_LAYERS
      * sizeof (*std_vps->dpbm.max_latency_increase_plus1));
  memcpy (std_vps->dpbm.max_dec_pic_buffering_minus1,
      vps->max_dec_pic_buffering_minus1, GST_H265_MAX_SUB_LAYERS
      * sizeof (*std_vps->dpbm.max_dec_pic_buffering_minus1));
  memcpy (std_vps->dpbm.max_num_reorder_pics,
      vps->max_num_reorder_pics, GST_H265_MAX_SUB_LAYERS
      * sizeof (*std_vps->dpbm.max_num_reorder_pics));

  /* *INDENT-OFF* */
  std_vps->vps = (StdVideoH265VideoParameterSet) {
    .flags = {
      .vps_temporal_id_nesting_flag = vps->temporal_id_nesting_flag,
      .vps_sub_layer_ordering_info_present_flag =
          vps->sub_layer_ordering_info_present_flag,
      .vps_timing_info_present_flag = vps->timing_info_present_flag,
      .vps_poc_proportional_to_timing_flag =
          vps->poc_proportional_to_timing_flag,
    },
    .vps_video_parameter_set_id = vps->id,
    .vps_max_sub_layers_minus1 = vps->max_sub_layers_minus1,
    /* Reserved */
    /* Reserved */
    .vps_num_units_in_tick = vps->num_units_in_tick,
    .vps_time_scale = vps->time_scale,
    .vps_num_ticks_poc_diff_one_minus1 = vps->num_ticks_poc_diff_one_minus1,
    /* Reserved */
    .pDecPicBufMgr = &std_vps->dpbm,
    .pHrdParameters = &std_vps->hrd,
    .pProfileTierLevel = &std_vps->ptl,
  };
  /* *INDENT-ON* */
}

static GstFlowReturn
_update_parameters (GstVulkanH265Decoder * self, const GstH265PPS * pps)
{
  GstH265SPS *sps = pps->sps;
  GstH265VPS *vps = sps->vps;
  struct SPS std_sps;
  struct PPS std_pps;
  struct VPS std_vps;
  VkVideoDecodeH265SessionParametersAddInfoKHR params = {
    .sType =
        VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_SESSION_PARAMETERS_ADD_INFO_KHR,
    .stdSPSCount = 1,
    .pStdSPSs = &std_sps.sps,
    .stdPPSCount = 1,
    .pStdPPSs = &std_pps.pps,
    .stdVPSCount = 1,
    .pStdVPSs = &std_vps.vps,
  };
  /* *INDENT-OFF* */
  GstVulkanDecoderParameters dec_params = {
    .h265 = {
      .sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_SESSION_PARAMETERS_CREATE_INFO_KHR,
      /* .pNext =  */
      .maxStdSPSCount = params.stdSPSCount,
      .maxStdPPSCount = params.stdPPSCount,
      .maxStdVPSCount = params.stdVPSCount,
      .pParametersAddInfo = &params,
    }
  };
  /* *INDENT-ON* */
  GError *error = NULL;

  _fill_sps (sps, &std_sps);
  _fill_pps (pps, sps, &std_pps);
  _fill_vps (vps, &std_vps);

  if (!gst_vulkan_decoder_update_video_session_parameters (self->decoder,
          &dec_params, &error)) {
    if (error) {
      GST_ERROR_OBJECT (self, "Couldn't set codec parameters: %s",
          error->message);
      g_clear_error (&error);
    }
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

static gint32
_find_next_slot_idx (GArray * dpb)
{
  gint32 i;
  guint len;
  GstH265Picture *arr[16] = { NULL, };

  g_assert (dpb->len < 16);

  len = dpb->len;

  for (i = 0; i < len; i++) {
    GstH265Picture *pic = g_array_index (dpb, GstH265Picture *, i);
    GstVulkanH265Picture *h265_pic;

    if (!pic->ref)
      continue;

    h265_pic = gst_h265_picture_get_user_data (pic);
    arr[h265_pic->slot_idx] = pic;
  }

  /* let's return the smallest available / not ref index */
  for (i = 0; i < len; i++) {
    if (!arr[i])
      return i;
  }

  return len;
}

static void
_fill_ref_slot (GstVulkanH265Decoder * self, GstH265Picture * picture,
    VkVideoReferenceSlotInfoKHR * slot, VkVideoPictureResourceInfoKHR * res,
    VkVideoDecodeH265DpbSlotInfoKHR * vkh265_slot,
    StdVideoDecodeH265ReferenceInfo * stdh265_ref,
    GstVulkanDecoderPicture ** ref)
{
  GstVulkanH265Picture *pic = gst_h265_picture_get_user_data (picture);

  /* *INDENT-OFF* */
  *stdh265_ref = (StdVideoDecodeH265ReferenceInfo) {
    .flags = {
      .used_for_long_term_reference = picture->ref && picture->long_term,
      .unused_for_reference = 0,
    },
    .PicOrderCntVal = picture->pic_order_cnt,
  };

  *vkh265_slot = (VkVideoDecodeH265DpbSlotInfoKHR) {
    .sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_DPB_SLOT_INFO_KHR,
    .pStdReferenceInfo = stdh265_ref,
  };

  *res = (VkVideoPictureResourceInfoKHR) {
    .sType = VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR,
    .codedOffset = { self->x, self->y },
    .codedExtent = { self->coded_width, self->coded_height },
    .baseArrayLayer = self->decoder->layered_dpb ? pic->slot_idx : 0,
    .imageViewBinding = pic->base.img_view_ref->view,
  };

  *slot = (VkVideoReferenceSlotInfoKHR) {
    .sType = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR,
    .pNext = vkh265_slot,
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
gst_vulkan_h265_decoder_start_picture (GstH265Decoder * decoder,
    GstH265Picture * picture, GstH265Slice * slice, GstH265Dpb * dpb)
{
  GstVulkanH265Decoder *self = GST_VULKAN_H265_DECODER (decoder);
  GstH265PPS *pps = slice->header.pps;
  GstFlowReturn ret;
  GstVulkanH265Picture *pic;
  GArray *refs;
  guint i, j, num_refs;

  GST_TRACE_OBJECT (self, "Start picture");

  if (self->need_params_update) {
    ret = _update_parameters (self, pps);
    if (ret != GST_FLOW_OK)
      return ret;
    self->need_params_update = FALSE;
  }

  refs = gst_h265_dpb_get_pictures_all (dpb);

  pic = gst_h265_picture_get_user_data (picture);
  g_assert (pic);

  /* *INDENT-OFF* */
  pic->std_h265pic = (StdVideoDecodeH265PictureInfo) {
    .flags = {
      .IrapPicFlag = GST_H265_IS_NAL_TYPE_IRAP (slice->nalu.type),
      .IdrPicFlag = GST_H265_IS_NAL_TYPE_IDR (slice->nalu.type),
      .IsReference = picture->ref,
      .short_term_ref_pic_set_sps_flag =
         slice->header.short_term_ref_pic_set_sps_flag,
    },
    .sps_video_parameter_set_id = pps->sps->vps_id,
    .pps_seq_parameter_set_id = pps->sps_id,
    .pps_pic_parameter_set_id = pps->id,
    .NumDeltaPocsOfRefRpsIdx =
       slice->header.short_term_ref_pic_sets.NumDeltaPocsOfRefRpsIdx,
    .PicOrderCntVal = picture->pic_order_cnt,
    .NumBitsForSTRefPicSetInSlice =
        !slice->header.short_term_ref_pic_set_sps_flag ?
        slice->header.short_term_ref_pic_set_size : 0,
  };

  pic->vk_h265pic = (VkVideoDecodeH265PictureInfoKHR) {
    .sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PICTURE_INFO_KHR,
    .pStdPictureInfo = &pic->std_h265pic,
    .sliceSegmentCount = 0,
  };
  /* *INDENT-ON* */

  pic->slot_idx = _find_next_slot_idx (refs);

  /* fill main slot */
  _fill_ref_slot (self, picture, &pic->base.slot, &pic->base.pic_res,
      &pic->vk_slot, &pic->std_ref, NULL);

  j = 0;

  for (i = 0; i < refs->len; i++) {
    GstH265Picture *ref_pic = g_array_index (refs, GstH265Picture *, i);
    if (!ref_pic->ref)
      continue;
    _fill_ref_slot (self, ref_pic, &pic->base.slots[j], &pic->base.pics_res[j],
        &pic->vk_slots[j], &pic->std_refs[j], &pic->base.refs[j]);
    j++;
  }

  num_refs = j;

  memset (pic->std_h265pic.RefPicSetStCurrBefore, 0xff, 8);
  memset (pic->std_h265pic.RefPicSetStCurrAfter, 0xff, 8);
  memset (pic->std_h265pic.RefPicSetLtCurr, 0xff, 8);

  for (i = 0; i < G_N_ELEMENTS (decoder->RefPicSetStCurrBefore); i++) {
    for (j = 0; j < refs->len; j++) {
      GstH265Picture *ref_pic = g_array_index (refs, GstH265Picture *, j);
      if (ref_pic == decoder->RefPicSetStCurrBefore[i]) {
        GstVulkanH265Picture *h265_ref_pic =
            gst_h265_picture_get_user_data (ref_pic);
        pic->std_h265pic.RefPicSetStCurrBefore[i] = h265_ref_pic->slot_idx;
        break;
      }
    }
  }

  for (i = 0; i < G_N_ELEMENTS (decoder->RefPicSetStCurrAfter); i++) {
    for (j = 0; j < refs->len; j++) {
      GstH265Picture *ref_pic = g_array_index (refs, GstH265Picture *, j);
      if (ref_pic == decoder->RefPicSetStCurrAfter[i]) {
        GstVulkanH265Picture *h265_ref_pic =
            gst_h265_picture_get_user_data (ref_pic);
        pic->std_h265pic.RefPicSetStCurrAfter[i] = h265_ref_pic->slot_idx;
        break;
      }
    }
  }

  for (i = 0; i < G_N_ELEMENTS (decoder->RefPicSetLtCurr); i++) {
    for (j = 0; j < refs->len; j++) {
      GstH265Picture *ref_pic = g_array_index (refs, GstH265Picture *, j);
      if (ref_pic == decoder->RefPicSetLtCurr[i]) {
        GstVulkanH265Picture *h265_ref_pic =
            gst_h265_picture_get_user_data (ref_pic);
        pic->std_h265pic.RefPicSetLtCurr[i] = h265_ref_pic->slot_idx;
        break;
      }
    }
  }

  g_array_unref (refs);


  /* *INDENT-OFF* */
  pic->base.decode_info = (VkVideoDecodeInfoKHR) {
    .sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_INFO_KHR,
    .pNext = &pic->vk_h265pic,
    .flags = 0x0,
    .pSetupReferenceSlot = &pic->base.slot,
    .referenceSlotCount = num_refs,
    .pReferenceSlots = (const VkVideoReferenceSlotInfoKHR *) &pic->base.slots,
    .dstPictureResource = {
      .sType = VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR,
      .codedOffset = { self->x, self->y },
      .codedExtent = { self->coded_width, self->coded_height },
      .baseArrayLayer = 0,
      .imageViewBinding = pic->base.img_view_out->view,
    },
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
gst_vulkan_h265_decoder_decode_slice (GstH265Decoder * decoder,
    GstH265Picture * picture, GstH265Slice * slice, GArray * ref_pic_list0,
    GArray * ref_pic_list1)
{
  GstVulkanH265Decoder *self = GST_VULKAN_H265_DECODER (decoder);
  GstVulkanH265Picture *pic;

  GST_TRACE_OBJECT (self, "Decode slice");

  pic = gst_h265_picture_get_user_data (picture);
  g_assert (pic);

  if (!gst_vulkan_decoder_append_slice (self->decoder, &pic->base,
          slice->nalu.data + slice->nalu.offset, slice->nalu.size, TRUE))
    return GST_FLOW_ERROR;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_vulkan_h265_decoder_end_picture (GstH265Decoder * decoder,
    GstH265Picture * picture)
{
  GstVulkanH265Decoder *self = GST_VULKAN_H265_DECODER (decoder);
  GstVulkanH265Picture *pic;
  GError *error = NULL;

  GST_TRACE_OBJECT (self, "End picture");

  pic = gst_h265_picture_get_user_data (picture);
  g_assert (pic);

  if (pic->base.slice_offs->len == 0)
    return GST_FLOW_OK;

  pic->vk_h265pic.sliceSegmentCount = pic->base.slice_offs->len - 1;
  pic->vk_h265pic.pSliceSegmentOffsets =
      (const guint32 *) pic->base.slice_offs->data;

  GST_LOG_OBJECT (self, "Decoding frame, %d bytes %d slices",
      pic->vk_h265pic.pSliceSegmentOffsets[pic->vk_h265pic.sliceSegmentCount],
      pic->vk_h265pic.sliceSegmentCount);

  if (!gst_vulkan_decoder_decode (self->decoder, &pic->base, &error)) {
    GST_ERROR_OBJECT (self, "Couldn't decode frame: %s",
        error ? error->message : "");
    g_clear_error (&error);
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_vulkan_h265_decoder_output_picture (GstH265Decoder * decoder,
    GstVideoCodecFrame * frame, GstH265Picture * picture)
{
  GstVideoDecoder *vdec = GST_VIDEO_DECODER (decoder);
  GstVulkanH265Decoder *self = GST_VULKAN_H265_DECODER (decoder);

  GST_TRACE_OBJECT (self, "Output picture");

  GST_LOG_OBJECT (self,
      "Outputting picture %p (poc %d)", picture, picture->pic_order_cnt);

  if (GST_CODEC_PICTURE (picture)->discont_state) {
    self->need_negotiation = TRUE;
    if (!gst_video_decoder_negotiate (vdec)) {
      gst_h265_picture_unref (picture);
      GST_ERROR_OBJECT (self, "Could not re-negotiate with updated state");
      return GST_FLOW_ERROR;
    }
  }

  gst_h265_picture_unref (picture);

  reset_pipeline_stage_mask (frame->output_buffer);

  return gst_video_decoder_finish_frame (vdec, frame);
}

static void
gst_vulkan_h265_decoder_init (GstVulkanH265Decoder * self)
{
  gst_vulkan_buffer_memory_init_once ();
}

static void
gst_vulkan_h265_decoder_class_init (GstVulkanH265DecoderClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (klass);
  GstH265DecoderClass *h265decoder_class = GST_H265_DECODER_CLASS (klass);

  gst_element_class_set_metadata (element_class, "Vulkan H.265 decoder",
      "Codec/Decoder/Video/Hardware", "A H.265 video decoder based on Vulkan",
      "Víctor Jáquez <vjaquez@igalia.com>");

  gst_element_class_add_static_pad_template (element_class,
      &gst_vulkan_h265dec_sink_template);

  gst_element_class_add_static_pad_template (element_class,
      &gst_vulkan_h265dec_src_template);

  element_class->set_context =
      GST_DEBUG_FUNCPTR (gst_vulkan_h265_decoder_set_context);

  decoder_class->src_query =
      GST_DEBUG_FUNCPTR (gst_vulkan_h265_decoder_src_query);
  decoder_class->sink_query =
      GST_DEBUG_FUNCPTR (gst_vulkan_h265_decoder_sink_query);
  decoder_class->open = GST_DEBUG_FUNCPTR (gst_vulkan_h265_decoder_open);
  decoder_class->close = GST_DEBUG_FUNCPTR (gst_vulkan_h265_decoder_close);
  decoder_class->stop = GST_DEBUG_FUNCPTR (gst_vulkan_h265_decoder_stop);
  decoder_class->negotiate =
      GST_DEBUG_FUNCPTR (gst_vulkan_h265_decoder_negotiate);
  decoder_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_vulkan_h265_decoder_decide_allocation);

  h265decoder_class->new_sequence =
      GST_DEBUG_FUNCPTR (gst_vulkan_h265_decoder_new_sequence);
  h265decoder_class->new_picture =
      GST_DEBUG_FUNCPTR (gst_vulkan_h265_decoder_new_picture);
  h265decoder_class->start_picture =
      GST_DEBUG_FUNCPTR (gst_vulkan_h265_decoder_start_picture);
  h265decoder_class->decode_slice =
      GST_DEBUG_FUNCPTR (gst_vulkan_h265_decoder_decode_slice);
  h265decoder_class->end_picture =
      GST_DEBUG_FUNCPTR (gst_vulkan_h265_decoder_end_picture);
  h265decoder_class->output_picture =
      GST_DEBUG_FUNCPTR (gst_vulkan_h265_decoder_output_picture);
}
