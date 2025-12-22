/*
 * GStreamer
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

#include "gstvkdecoder.h"

#include "gstvkoperation.h"
#include "gstvkphysicaldevice-private.h"
#include "gstvkvideo-private.h"

/**
 * SECTION:vkdecoder
 * @title: GstVulkanDecoder
 * @short_description: Abstract Vulkan Video Decoder
 * @see_also: #GstVulkanOperation
 *
 * #GstVulkanOperation abstracts a video decoding operation.
 *
 * Since: 1.24
 */

struct _GstVulkanDecoderPrivate
{
  GstVulkanHandle *empty_params;
  GstVulkanHandle *session_params;
  GstVulkanHandle *sampler;

  GstCaps *profile_caps;
  GstBufferPool *dpb_pool;

  GstVulkanOperation *exec;

  GstVulkanVideoSession session;
  GstVulkanVideoCapabilities caps;
  VkVideoFormatPropertiesKHR format;

  gboolean vk_populated;
  GstVulkanVideoFunctions vk;

  gboolean started;
};

#define GST_CAT_DEFAULT gst_vulkan_decoder_debug
GST_DEBUG_CATEGORY (GST_CAT_DEFAULT);

#define gst_vulkan_decoder_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstVulkanDecoder, gst_vulkan_decoder,
    GST_TYPE_OBJECT, G_ADD_PRIVATE (GstVulkanDecoder)
    GST_DEBUG_CATEGORY_INIT (gst_vulkan_decoder_debug,
        "vulkandecoder", 0, "Vulkan device decoder"));

static GstVulkanHandle *gst_vulkan_decoder_new_video_session_parameters
    (GstVulkanDecoder * self, GstVulkanDecoderParameters * params,
    GError ** error);

static gboolean
_populate_function_table (GstVulkanDecoder * self)
{
  GstVulkanDecoderPrivate *priv =
      gst_vulkan_decoder_get_instance_private (self);
  GstVulkanInstance *instance;

  if (priv->vk_populated)
    return TRUE;

  instance = gst_vulkan_device_get_instance (self->queue->device);
  if (!instance) {
    GST_ERROR_OBJECT (self, "Failed to get instance from the device");
    return FALSE;
  }

  priv->vk_populated = gst_vulkan_video_get_vk_functions (instance, &priv->vk);
  gst_object_unref (instance);
  return priv->vk_populated;
}

static void
gst_vulkan_decoder_finalize (GObject * object)
{
  GstVulkanDecoder *self = GST_VULKAN_DECODER (object);

  gst_clear_object (&self->queue);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_vulkan_decoder_init (GstVulkanDecoder * self)
{
}

static void
gst_vulkan_decoder_class_init (GstVulkanDecoderClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gst_vulkan_decoder_finalize;
}

/**
 * gst_vulkan_decoder_start:
 * @self: a #GstVulkanDecoder
 * @profile: a #GstVulkanVideoProfile
 * @error: a #GError
 *
 * It creates a Vulkan video session for the given @profile. If an error occurs,
 * @error is filled.
 *
 * Returns: whether the video decoder has started correctly.
 */
gboolean
gst_vulkan_decoder_start (GstVulkanDecoder * self,
    GstVulkanVideoProfile * profile, GError ** error)
{
  GstVulkanDecoderPrivate *priv;
  VkPhysicalDevice gpu;
  VkResult res;
  VkVideoDecodeCapabilitiesKHR dec_caps;
  VkVideoFormatPropertiesKHR *fmts = NULL;
  VkVideoProfileListInfoKHR profile_list = {
    .sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR,
    .profileCount = 1,
  };
  VkPhysicalDeviceVideoFormatInfoKHR fmt_info = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_FORMAT_INFO_KHR,
    .pNext = &profile_list,
  };
  VkVideoSessionCreateInfoKHR session_create;
  GstVulkanDecoderParameters empty_params;
  guint i, maxlevel, n_fmts, codec_idx;
  GstVideoFormat format = GST_VIDEO_FORMAT_UNKNOWN;
  VkFormat vk_format = VK_FORMAT_UNDEFINED;
  GstVulkanCommandPool *cmd_pool;
  GError *query_err = NULL;

  g_return_val_if_fail (GST_IS_VULKAN_DECODER (self), FALSE);

  priv = gst_vulkan_decoder_get_instance_private (self);

  if (priv->started)
    return TRUE;

  g_assert (self->codec == profile->profile.videoCodecOperation);

  if (!_populate_function_table (self)) {
    g_set_error (error, GST_VULKAN_ERROR, VK_ERROR_INITIALIZATION_FAILED,
        "Couldn't load Vulkan Video functions");
    return FALSE;
  }

  switch (self->codec) {
    case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
    case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
      if (!gst_vulkan_video_profile_is_valid (profile, self->codec)) {
        g_set_error (error, GST_VULKAN_ERROR, VK_ERROR_INITIALIZATION_FAILED,
            "Invalid profile");
        return FALSE;
      }
      break;
    default:
      g_set_error (error, GST_VULKAN_ERROR, VK_ERROR_INITIALIZATION_FAILED,
          "Invalid codec");
      return FALSE;
  }

  self->profile = *profile;
  self->profile.profile.pNext = &self->profile.usage.decode;
  self->profile.usage.decode.pNext = &self->profile.codec;

  switch (self->codec) {
    case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
      /* *INDENT-OFF* */
      priv->caps.codec.h264dec = (VkVideoDecodeH264CapabilitiesKHR) {
          .sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_CAPABILITIES_KHR,
      };
      /* *INDENT-ON* */
      codec_idx = GST_VK_VIDEO_EXTENSION_DECODE_H264;
      break;
    case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
      /* *INDENT-OFF* */
      priv->caps.codec.h265dec = (VkVideoDecodeH265CapabilitiesKHR) {
          .sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_CAPABILITIES_KHR,
      };
      /* *INDENT-ON* */
      codec_idx = GST_VK_VIDEO_EXTENSION_DECODE_H265;
      break;
    default:
      g_assert_not_reached ();
  }

  /* *INDENT-OFF* */
  dec_caps = (VkVideoDecodeCapabilitiesKHR) {
    .sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR,
    .pNext = &priv->caps.codec,
  };
  priv->caps.caps =  (VkVideoCapabilitiesKHR) {
    .sType = VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR,
    .pNext = &dec_caps,
  };
  /* *INDENT-ON* */

  gpu = gst_vulkan_device_get_physical_device (self->queue->device);
  res = priv->vk.GetPhysicalDeviceVideoCapabilities (gpu,
      &self->profile.profile, &priv->caps.caps);
  if (gst_vulkan_error_to_g_error (res, error,
          "vkGetPhysicalDeviceVideoCapabilitiesKHR") != VK_SUCCESS)
    return FALSE;

  switch (self->codec) {
    case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
      maxlevel = priv->caps.codec.h264dec.maxLevelIdc;
      break;
    case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
      maxlevel = priv->caps.codec.h265dec.maxLevelIdc;
      break;
    default:
      maxlevel = 0;
  }

  priv->profile_caps = gst_vulkan_video_profile_to_caps (&self->profile);
  GST_LOG_OBJECT (self, "Capabilities for %" GST_PTR_FORMAT ":\n"
      "     Maximum level: %d\n"
      "     Width from %i to %i\n"
      "     Height from %i to %i\n"
      "     Width alignment: %i\n"
      "     Height alignment: %i\n"
      "     Buffer offset alignment: %" G_GUINT64_FORMAT "\n"
      "     Buffer size alignment %" G_GUINT64_FORMAT "\n"
      "     Maximum references: %u\n"
      "     Maximum active references: %u\n"
      "     Capabilities flags: %s%s%s\n"
      "     Codec header version: %s [%i.%i.%i] (driver) [%i.%i.%i] (compiled) \n"
      "     Decode modes:%s%s%s",
      priv->profile_caps,
      maxlevel,
      priv->caps.caps.minCodedExtent.width,
      priv->caps.caps.maxCodedExtent.width,
      priv->caps.caps.minCodedExtent.height,
      priv->caps.caps.maxCodedExtent.height,
      priv->caps.caps.pictureAccessGranularity.width,
      priv->caps.caps.pictureAccessGranularity.height,
      priv->caps.caps.minBitstreamBufferOffsetAlignment,
      priv->caps.caps.minBitstreamBufferSizeAlignment,
      priv->caps.caps.maxDpbSlots, priv->caps.caps.maxActiveReferencePictures,
      priv->caps.caps.flags ? "" : " none",
      priv->caps.caps.flags &
      VK_VIDEO_CAPABILITY_PROTECTED_CONTENT_BIT_KHR ?
      " protected" : "",
      priv->caps.caps.flags &
      VK_VIDEO_CAPABILITY_SEPARATE_REFERENCE_IMAGES_BIT_KHR ?
      " separate_references" : "",
      GST_STR_NULL (priv->caps.caps.stdHeaderVersion.extensionName),
      VK_CODEC_VERSION (priv->caps.caps.stdHeaderVersion.specVersion),
      VK_CODEC_VERSION (_vk_codec_extensions[codec_idx].specVersion),
      dec_caps.flags ? "" : " invalid",
      dec_caps.flags &
      VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_COINCIDE_BIT_KHR ?
      " reuse_output_DPB" : "",
      dec_caps.flags &
      VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_DISTINCT_BIT_KHR ?
      " dedicated_DPB" : "");

  /* VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_COINCIDE_BIT_KHR - reports the
   * implementation supports using the same Video Picture Resource for decode
   * DPB and decode output.
   *
   * VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_DISTINCT_BIT_KHR - reports the
   * implementation supports using distinct Video Picture Resources for decode
   * DPB and decode output. */
  self->dedicated_dpb = ((dec_caps.flags &
          VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_COINCIDE_BIT_KHR) == 0);

  /* The DPB or Reconstructed Video Picture Resources for the video session may
   * be created as a separate VkImage for each DPB picture. If not supported,
   * the DPB must be created as single multi-layered image where each layer
   * represents one of the DPB Video Picture Resources. */
  self->layered_dpb = ((priv->caps.caps.flags &
          VK_VIDEO_CAPABILITY_SEPARATE_REFERENCE_IMAGES_BIT_KHR) == 0);

  if (self->layered_dpb && !self->dedicated_dpb) {
    g_set_error (error, GST_VULKAN_ERROR, VK_ERROR_INCOMPATIBLE_DRIVER,
        "Buggy driver: "
        "VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_COINCIDE_BIT_KHR set but "
        "VK_VIDEO_CAPABILITY_SEPARATE_REFERENCE_IMAGES_BIT_KHR is unset!");

    goto failed;
  }

  priv->caps.caps.pNext = NULL;

  /* Get output format */
  profile_list.pProfiles = &self->profile.profile;

  fmt_info.imageUsage = VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR
      | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  if (!self->dedicated_dpb)
    fmt_info.imageUsage |= VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR;

  res = priv->vk.GetPhysicalDeviceVideoFormatProperties (gpu, &fmt_info,
      &n_fmts, NULL);
  if (gst_vulkan_error_to_g_error (res, error,
          "vkGetPhysicalDeviceVideoFormatPropertiesKHR") != VK_SUCCESS)
    goto failed;

  if (n_fmts == 0) {
    g_set_error (error, GST_VULKAN_ERROR, VK_ERROR_INITIALIZATION_FAILED,
        "Profile doesn't have an output format");
    goto failed;
  }

  fmts = g_new0 (VkVideoFormatPropertiesKHR, n_fmts);
  for (i = 0; i < n_fmts; i++)
    fmts[i].sType = VK_STRUCTURE_TYPE_VIDEO_FORMAT_PROPERTIES_KHR;

  res = priv->vk.GetPhysicalDeviceVideoFormatProperties (gpu, &fmt_info,
      &n_fmts, fmts);
  if (gst_vulkan_error_to_g_error (res, error,
          "vkGetPhysicalDeviceVideoFormatPropertiesKHR") != VK_SUCCESS) {
    goto failed;
  }

  if (n_fmts == 0) {
    g_free (fmts);
    g_set_error (error, GST_VULKAN_ERROR, VK_ERROR_INITIALIZATION_FAILED,
        "Profile doesn't have an output format");
    goto failed;
  }

  /* find the best output format */
  for (i = 0; i < n_fmts; i++) {
    format = gst_vulkan_format_to_video_format (fmts[i].format);
    if (format == GST_VIDEO_FORMAT_UNKNOWN) {
      GST_WARNING_OBJECT (self, "Unknown Vulkan format %i", fmts[i].format);
      continue;
    } else {
      vk_format = fmts[i].format;
      priv->format = fmts[i];
      priv->format.pNext = NULL;
      break;
    }
  }
  g_clear_pointer (&fmts, g_free);

  if (vk_format == VK_FORMAT_UNDEFINED) {
    g_set_error (error, GST_VULKAN_ERROR, VK_ERROR_INITIALIZATION_FAILED,
        "No valid output format found");
    goto failed;
  }

  GST_INFO_OBJECT (self, "Using output format %s",
      gst_video_format_to_string (format));

  /* *INDENT-OFF* */
  session_create = (VkVideoSessionCreateInfoKHR) {
    .sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_CREATE_INFO_KHR,
    .queueFamilyIndex = self->queue->family,
    .pVideoProfile = &profile->profile,
    .pictureFormat = vk_format,
    .maxCodedExtent = priv->caps.caps.maxCodedExtent,
    .referencePictureFormat = vk_format,
    .maxDpbSlots = priv->caps.caps.maxDpbSlots,
    .maxActiveReferencePictures = priv->caps.caps.maxActiveReferencePictures,
    .pStdHeaderVersion = &_vk_codec_extensions[codec_idx],
  };
  /* *INDENT-ON* */

  /* create video session */
  if (!gst_vulkan_video_session_create (&priv->session, self->queue->device,
          &priv->vk, &session_create, error))
    goto failed;

  /* create empty codec params */
  switch (self->profile.profile.videoCodecOperation) {
    case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
      /* *INDENT-OFF* */
      empty_params.h264 = (VkVideoDecodeH264SessionParametersCreateInfoKHR) {
        .sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_SESSION_PARAMETERS_CREATE_INFO_KHR,
      };
      /* *INDENT-ON* */
      break;
    case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
      /* *INDENT-OFF* */
      empty_params.h265 = (VkVideoDecodeH265SessionParametersCreateInfoKHR) {
        .sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_SESSION_PARAMETERS_CREATE_INFO_KHR,
      };
      /* *INDENT-ON* */
      break;
    default:
      g_assert_not_reached ();
  }

  priv->empty_params = gst_vulkan_decoder_new_video_session_parameters (self,
      &empty_params, error);
  if (!priv->empty_params)
    goto failed;
  cmd_pool = gst_vulkan_queue_create_command_pool (self->queue, error);
  if (!cmd_pool)
    goto failed;
  priv->exec = gst_vulkan_operation_new (cmd_pool);
  gst_object_unref (cmd_pool);
  if (!gst_vulkan_operation_enable_query (priv->exec,
          VK_QUERY_TYPE_RESULT_STATUS_ONLY_KHR, 1, &profile->profile,
          &query_err)) {
    if (query_err->code != VK_ERROR_FEATURE_NOT_PRESENT) {
      g_propagate_error (error, query_err);
      goto failed;
    }
    g_clear_error (&query_err);
  }

  if (!gst_vulkan_decoder_flush (self, error))
    goto failed;

  priv->started = TRUE;

  return TRUE;

failed:
  {
    g_free (fmts);
    gst_clear_caps (&priv->profile_caps);

    if (priv->session.session)
      gst_vulkan_video_session_destroy (&priv->session);

    gst_clear_vulkan_handle (&priv->empty_params);

    gst_clear_object (&priv->exec);

    return FALSE;
  }
}

/**
 * gst_vulkan_decoder_stop:
 * @self: a #GstVulkanDecoder
 *
 * Destroys the video session created at gst_vulkan_decoder_start() and clean up
 * the internal objects.
 *
 * Returns: whether the decoder stopped correctly.
 */
gboolean
gst_vulkan_decoder_stop (GstVulkanDecoder * self)
{
  GstVulkanDecoderPrivate *priv;

  g_return_val_if_fail (GST_IS_VULKAN_DECODER (self), FALSE);

  priv = gst_vulkan_decoder_get_instance_private (self);

  if (!priv->started)
    return TRUE;

  gst_vulkan_decoder_wait (self);

  gst_clear_buffer (&self->input_buffer);

  gst_clear_buffer (&self->layered_buffer);
  gst_clear_object (&priv->dpb_pool);

  gst_vulkan_video_session_destroy (&priv->session);

  gst_clear_caps (&priv->profile_caps);

  gst_clear_vulkan_handle (&priv->empty_params);
  gst_clear_vulkan_handle (&priv->session_params);

  gst_clear_vulkan_handle (&priv->sampler);

  gst_clear_object (&priv->exec);

  priv->started = FALSE;

  return TRUE;
}

/**
 * gst_vulkan_decoder_flush:
 * @self: a #GstVulkanDecoder
 * @error: a #GError
 *
 * Initializes the decoder at driver level and set its DPB slots to the inactive
 * state.
 *
 * Returns: whether flush was successful
 */
gboolean
gst_vulkan_decoder_flush (GstVulkanDecoder * self, GError ** error)
{
  GstVulkanDecoderPrivate *priv;
  VkVideoBeginCodingInfoKHR decode_start;
  VkVideoCodingControlInfoKHR decode_ctrl = {
    .sType = VK_STRUCTURE_TYPE_VIDEO_CODING_CONTROL_INFO_KHR,
    .flags = VK_VIDEO_CODING_CONTROL_RESET_BIT_KHR,
  };
  VkVideoEndCodingInfoKHR decode_end = {
    .sType = VK_STRUCTURE_TYPE_VIDEO_END_CODING_INFO_KHR,
  };
  gboolean ret;

  g_return_val_if_fail (GST_IS_VULKAN_DECODER (self), FALSE);

  priv = gst_vulkan_decoder_get_instance_private (self);

  if (!(priv->empty_params && priv->exec))
    return FALSE;

  /* *INDENT-OFF* */
  decode_start = (VkVideoBeginCodingInfoKHR) {
    .sType = VK_STRUCTURE_TYPE_VIDEO_BEGIN_CODING_INFO_KHR,
    .videoSession = priv->session.session->handle,
    .videoSessionParameters = priv->empty_params->handle,
  };
  /* *INDENT-ON* */

  if (!gst_vulkan_operation_begin (priv->exec, error))
    return FALSE;

  priv->vk.CmdBeginVideoCoding (priv->exec->cmd_buf->cmd, &decode_start);
  priv->vk.CmdControlVideoCoding (priv->exec->cmd_buf->cmd, &decode_ctrl);
  priv->vk.CmdEndVideoCoding (priv->exec->cmd_buf->cmd, &decode_end);

  ret = gst_vulkan_operation_end (priv->exec, error);

  return ret;
}

/**
 * gst_vulkan_decoder_create_dpb_pool:
 * @self: a #GstVulkanDecoder
 * @caps: the #GstCaps of the DP
 *
 * Instantiates an internal Vulkan image pool for driver decoders whose output
 * buffers cannot be used as DPB buffers.
 *
 * Returns: whether the pool was created.
 */
gboolean
gst_vulkan_decoder_create_dpb_pool (GstVulkanDecoder * self, GstCaps * caps)
{
  GstVulkanDecoderPrivate *priv;
  VkImageUsageFlags usage = VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR;
  GstStructure *config;
  guint min_buffers, max_buffers;
  GstFlowReturn ret;

  g_return_val_if_fail (GST_IS_VULKAN_DECODER (self), FALSE);
  g_return_val_if_fail (GST_IS_CAPS (caps), FALSE);

  priv = gst_vulkan_decoder_get_instance_private (self);

  if (!priv->started)
    return FALSE;

  if (!self->dedicated_dpb)
    return TRUE;

  if (self->layered_dpb) {
    min_buffers = max_buffers = 1;
  } else {
    min_buffers = priv->caps.caps.maxDpbSlots;
    max_buffers = 0;
  }

  priv->dpb_pool = gst_vulkan_image_buffer_pool_new (self->queue->device);

  config = gst_buffer_pool_get_config (priv->dpb_pool);
  gst_buffer_pool_config_set_params (config, caps, 1024, min_buffers,
      max_buffers);
  gst_vulkan_image_buffer_pool_config_set_allocation_params (config, usage,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR,
      VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);

  if (self->layered_dpb) {
    gst_structure_set (config, "num-layers", G_TYPE_UINT,
        priv->caps.caps.maxDpbSlots, NULL);
  }

  gst_vulkan_image_buffer_pool_config_set_decode_caps (config,
      priv->profile_caps);

  if (!gst_buffer_pool_set_config (priv->dpb_pool, config))
    goto bail;
  if (!gst_buffer_pool_set_active (priv->dpb_pool, TRUE))
    goto bail;

  if (self->layered_dpb) {
    ret = gst_buffer_pool_acquire_buffer (priv->dpb_pool, &self->layered_buffer,
        NULL);
    if (ret != GST_FLOW_OK)
      goto bail;
  }

  return TRUE;

bail:
  g_clear_object (&priv->dpb_pool);
  return FALSE;
}

/**
 * gst_vulkan_decoder_decode:
 * @self: a #GstVulkanDecoder
 * @pic: a #GstVulkanDecoderPicture
 * @error:  a #GError
 *
 * Decodes @pic.
 *
 * Return: whether @pic was decoded correctly. It might fill @error.
 */
gboolean
gst_vulkan_decoder_decode (GstVulkanDecoder * self,
    GstVulkanDecoderPicture * pic, GError ** error)
{
  GstVulkanDecoderPrivate *priv;
  VkVideoBeginCodingInfoKHR decode_start;
  VkVideoEndCodingInfoKHR decode_end = {
    .sType = VK_STRUCTURE_TYPE_VIDEO_END_CODING_INFO_KHR,
  };
  GArray *barriers;
  VkImageLayout new_layout;
  GstVulkanCommandBuffer *cmd_buf;
  gboolean ret;
  VkVideoReferenceSlotInfoKHR *cur_slot;
  gint32 i;
  GstMemory *mem;
  guint32 slices_size;


  g_return_val_if_fail (GST_IS_VULKAN_DECODER (self), FALSE);
  g_return_val_if_fail (pic, FALSE);

  priv = gst_vulkan_decoder_get_instance_private (self);

  /* *INDENT-OFF* */
  decode_start = (VkVideoBeginCodingInfoKHR) {
    .sType = VK_STRUCTURE_TYPE_VIDEO_BEGIN_CODING_INFO_KHR,
    .videoSession = priv->session.session->handle,
    .videoSessionParameters = priv->session_params->handle,
    .referenceSlotCount = pic->decode_info.referenceSlotCount,
    .pReferenceSlots = pic->decode_info.pReferenceSlots,
  };
  /* *INDENT-ON* */

  if (!(priv->started && priv->session_params)) {
    g_set_error (error, GST_VULKAN_ERROR, VK_ERROR_INITIALIZATION_FAILED,
        "Vulkan Decoder has not started or no session parameters are set");
    return FALSE;
  }

  /* The current decoding reference has to be bound as an inactive reference */
  cur_slot = (VkVideoReferenceSlotInfoKHR *)
      & decode_start.pReferenceSlots[decode_start.referenceSlotCount];
  *cur_slot = pic->slot;
  cur_slot->slotIndex = -1;
  decode_start.referenceSlotCount++;

  /* set the input buffer */
  mem = gst_buffer_peek_memory (self->input_buffer, 0);
  slices_size = g_array_index (pic->slice_offs, guint32,
      pic->slice_offs->len - 1);

  pic->decode_info.srcBuffer = ((GstVulkanBufferMemory *) mem)->buffer;
  pic->decode_info.srcBufferRange = GST_ROUND_UP_N (slices_size,
      priv->caps.caps.minBitstreamBufferSizeAlignment);

  if (!gst_vulkan_operation_begin (priv->exec, error))
    return FALSE;

  cmd_buf = priv->exec->cmd_buf;

  if (!gst_vulkan_operation_add_dependency_frame (priv->exec, pic->out,
          VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR,
          VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR)) {
    return FALSE;
  }

  new_layout = (self->layered_dpb || pic->dpb) ?
      VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR :
      VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR;
  gst_vulkan_operation_add_frame_barrier (priv->exec, pic->out,
      VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR,
      VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR, new_layout, NULL);

  /* Reference for the current image, if existing and not layered */
  if (pic->dpb) {
    if (!gst_vulkan_operation_add_dependency_frame (priv->exec, pic->dpb,
            VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR,
            VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR)) {
      return FALSE;
    }
  }

  if (!self->layered_dpb) {
    /* All references (apart from the current) for non-layered refs */

    for (i = 0; i < pic->decode_info.referenceSlotCount; i++) {
      GstVulkanDecoderPicture *ref_pic = pic->refs[i];
      GstBuffer *ref_buf = ref_pic->dpb ? ref_pic->dpb : ref_pic->out;

      if (!gst_vulkan_operation_add_dependency_frame (priv->exec, ref_buf,
              VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR,
              VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR)) {
        return FALSE;
      }

      if (!ref_pic->dpb) {
        gst_vulkan_operation_add_frame_barrier (priv->exec, ref_buf,
            VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR,
            VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR
            | VK_ACCESS_2_VIDEO_DECODE_READ_BIT_KHR,
            VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR, NULL);
      }
    }
  } else if (pic->decode_info.referenceSlotCount > 1
      || pic->img_view_out != pic->img_view_ref) {
    /* Single barrier for a single layered ref */
    if (!gst_vulkan_operation_add_dependency_frame (priv->exec,
            self->layered_buffer, VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR,
            VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR)) {
      return FALSE;
    }
  }

  /* change image layout */
  barriers = gst_vulkan_operation_retrieve_image_barriers (priv->exec);
  /* *INDENT-OFF* */
  vkCmdPipelineBarrier2 (cmd_buf->cmd, &(VkDependencyInfo) {
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
      .pImageMemoryBarriers = (VkImageMemoryBarrier2 *) barriers->data,
      .imageMemoryBarrierCount = barriers->len,
    });
  /* *INDENT-ON* */
  g_array_unref (barriers);

  priv->vk.CmdBeginVideoCoding (cmd_buf->cmd, &decode_start);
  gst_vulkan_operation_begin_query (priv->exec, 0);
  priv->vk.CmdDecodeVideo (cmd_buf->cmd, &pic->decode_info);
  gst_vulkan_operation_end_query (priv->exec, 0);
  priv->vk.CmdEndVideoCoding (cmd_buf->cmd, &decode_end);

  ret = gst_vulkan_operation_end (priv->exec, error);

  return ret;
}

/**
 * gst_vulkan_decoder_is_started:
 * @self: a #GstVulkanDecoder
 *
 * Returns: whether gst_vulkan_decoder_start() was called correctly previously.
 */
gboolean
gst_vulkan_decoder_is_started (GstVulkanDecoder * self)
{
  GstVulkanDecoderPrivate *priv;

  g_return_val_if_fail (GST_IS_VULKAN_DECODER (self), FALSE);

  priv = gst_vulkan_decoder_get_instance_private (self);
  return priv->started;
}

/**
 * gst_vulkan_decoder_caps:
 * @self: a #GstVulkanDecoder
 * @caps: (out): a #GstVulkanVideoCapabilities
 *
 * Gets the Vulkan decoding capabilities of the current video session.
 *
 * Returns: whether the capabilities were fetched correctly.
 */
gboolean
gst_vulkan_decoder_caps (GstVulkanDecoder * self,
    GstVulkanVideoCapabilities * caps)
{
  GstVulkanDecoderPrivate *priv;

  g_return_val_if_fail (GST_IS_VULKAN_DECODER (self), FALSE);

  priv = gst_vulkan_decoder_get_instance_private (self);

  if (!priv->started)
    return FALSE;

  if (caps) {
    *caps = priv->caps;
    caps->caps.pNext = &caps->codec;
  }

  return TRUE;
}

/**
 * gst_vulkan_decoder_out_format: (skip)
 * @self: a #GstVulkanDecoder
 * @format: the Vulkan output format properties
 *
 * Gets the Vulkan format properties of the output frames.
 *
 * Returns: whether the @format was fetched.
 */
gboolean
gst_vulkan_decoder_out_format (GstVulkanDecoder * self,
    VkVideoFormatPropertiesKHR * format)
{
  GstVulkanDecoderPrivate *priv;

  g_return_val_if_fail (GST_IS_VULKAN_DECODER (self), FALSE);

  priv = gst_vulkan_decoder_get_instance_private (self);

  if (!priv->started)
    return FALSE;

  if (format)
    *format = priv->format;

  return TRUE;
}

/**
 * gst_vulkan_decoder_profile_caps:
 * @self: a #GstVulkanDecoder
 *
 * Returns: (transfer full): the #GstCaps of the profile defined at
 *     gst_vulkan_decoder_start().
 */
GstCaps *
gst_vulkan_decoder_profile_caps (GstVulkanDecoder * self)
{
  GstVulkanDecoderPrivate *priv;

  g_return_val_if_fail (GST_IS_VULKAN_DECODER (self), NULL);

  priv = gst_vulkan_decoder_get_instance_private (self);

  if (!priv->started)
    return NULL;

  return gst_caps_ref (priv->profile_caps);
}

static void
gst_vulkan_handle_free_video_session_parameters (GstVulkanHandle * handle,
    gpointer data)
{
  PFN_vkDestroyVideoSessionParametersKHR vkDestroyVideoSessionParameters;

  g_return_if_fail (handle != NULL);
  g_return_if_fail (handle->handle != VK_NULL_HANDLE);
  g_return_if_fail (handle->type ==
      GST_VULKAN_HANDLE_TYPE_VIDEO_SESSION_PARAMETERS);
  g_return_if_fail (handle->user_data);

  vkDestroyVideoSessionParameters = handle->user_data;
  vkDestroyVideoSessionParameters (handle->device->device,
      (VkVideoSessionKHR) handle->handle, NULL);
}

static GstVulkanHandle *
gst_vulkan_decoder_new_video_session_parameters (GstVulkanDecoder * self,
    GstVulkanDecoderParameters * params, GError ** error)
{
  GstVulkanDecoderPrivate *priv;
  VkVideoSessionParametersCreateInfoKHR session_params_info;
  VkResult res;
  VkVideoSessionParametersKHR session_params;

  priv = gst_vulkan_decoder_get_instance_private (self);

  if (!priv->session.session)
    return NULL;

  /* *INDENT-OFF* */
  session_params_info = (VkVideoSessionParametersCreateInfoKHR) {
    .sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_CREATE_INFO_KHR,
    .pNext = params,
    .videoSession = priv->session.session->handle,
  };
  /* *INDENT-ON* */

  res = priv->vk.CreateVideoSessionParameters (self->queue->device->device,
      &session_params_info, NULL, &session_params);
  if (gst_vulkan_error_to_g_error (res, error,
          "vkCreateVideoSessionParametersKHR") != VK_SUCCESS)
    return NULL;

  return gst_vulkan_handle_new_wrapped (self->queue->device,
      GST_VULKAN_HANDLE_TYPE_VIDEO_SESSION_PARAMETERS,
      (GstVulkanHandleTypedef) session_params,
      gst_vulkan_handle_free_video_session_parameters,
      priv->vk.DestroyVideoSessionParameters);
}

/**
 * gst_vulkan_decoder_update_video_session_parameters:
 * @self: a #GstVulkanDecoder
 * @params: a GstVulkanDecoderParameters union
 * @error: a #GError
 *
 * Update the internal codec parameters for the current video session.
 *
 * Returns: whether the @params were updated internally. It might fill @error.
 */
gboolean
gst_vulkan_decoder_update_video_session_parameters (GstVulkanDecoder * self,
    GstVulkanDecoderParameters * params, GError ** error)
{
  GstVulkanDecoderPrivate *priv;
  GstVulkanHandle *handle;

  g_return_val_if_fail (GST_IS_VULKAN_DECODER (self), FALSE);
  g_return_val_if_fail (params, FALSE);

  handle =
      gst_vulkan_decoder_new_video_session_parameters (self, params, error);
  if (!handle)
    return FALSE;

  priv = gst_vulkan_decoder_get_instance_private (self);

  gst_clear_vulkan_handle (&priv->session_params);
  priv->session_params = handle;

  return TRUE;
}

static void
gst_vulkan_handle_free_sampler_ycbcr_conversion (GstVulkanHandle * handle,
    gpointer data)
{
  g_return_if_fail (handle != NULL);
  g_return_if_fail (handle->handle != VK_NULL_HANDLE);
  g_return_if_fail (handle->type ==
      GST_VULKAN_HANDLE_TYPE_SAMPLER_YCBCR_CONVERSION);

  vkDestroySamplerYcbcrConversion (handle->device->device,
      (VkSamplerYcbcrConversion) handle->handle, NULL);
}

/**
 * gst_vulkan_decoder_update_ycbcr_sampler:
 * @self: a #GstVulkanDecoder
 * @range: whether color components are encoded using the full range of
 *     numerical values or whether values are reserved for headroom and foot
 *     room.
 * @xloc: x location of downsampled chroma component samples relative to the luma
 *     samples.
 * @yloc: y location of downsampled chroma component samples relative to the luma
 *     samples.
 *
 * Update the internal Ycbcr sampler for the output images.
 *
 * Returns: whether the sampler was updated.
 */
gboolean
gst_vulkan_decoder_update_ycbcr_sampler (GstVulkanDecoder * self,
    VkSamplerYcbcrRange range, VkChromaLocation xloc,
    VkChromaLocation yloc, GError ** error)
{
  const VkPhysicalDeviceFeatures2 *features;
  const VkBaseOutStructure *iter;
  GstVulkanDevice *device;
  GstVulkanDecoderPrivate *priv;
  GstVulkanHandle *handle;
  VkSamplerYcbcrConversionCreateInfo create_info;
  VkSamplerYcbcrConversion ycbr_conversion;
  VkResult res;
  gboolean found = FALSE;

  g_return_val_if_fail (GST_IS_VULKAN_DECODER (self), FALSE);

  device = self->queue->device;

  if (!gst_vulkan_instance_check_version (device->instance, 1, 2, 0)) {
    g_set_error (error, GST_VULKAN_ERROR, VK_ERROR_INITIALIZATION_FAILED,
        "Sampler Ycbcr conversion not available in API");
    return FALSE;
  }

  features = gst_vulkan_physical_device_get_features (device->physical_device);
  for (iter = (const VkBaseOutStructure *) features; iter; iter = iter->pNext) {
    if (iter->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES) {
      const VkPhysicalDeviceVulkan11Features *features11 =
          (const VkPhysicalDeviceVulkan11Features *) iter;

      if (!features11->samplerYcbcrConversion)
        return FALSE;
      found = TRUE;
      break;
    }
  }

  if (!found) {
    g_set_error (error, GST_VULKAN_ERROR, VK_ERROR_INITIALIZATION_FAILED,
        "Sampler Ycbcr conversion not available in driver");
    return FALSE;
  }

  priv = gst_vulkan_decoder_get_instance_private (self);

  /* *INDENT-OFF* */
  create_info = (VkSamplerYcbcrConversionCreateInfo) {
    .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,
    .components = _vk_identity_component_map,
    .ycbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY,
    .ycbcrRange = range,
    .xChromaOffset = xloc,
    .yChromaOffset = yloc,
    .format = priv->format.format,
  };
  /* *INDENT-ON* */

  res = vkCreateSamplerYcbcrConversion (device->device, &create_info, NULL,
      &ycbr_conversion);
  if (gst_vulkan_error_to_g_error (res, error,
          "vkCreateSamplerYcbcrConversion") != VK_SUCCESS)
    return FALSE;

  handle = gst_vulkan_handle_new_wrapped (device,
      GST_VULKAN_HANDLE_TYPE_SAMPLER_YCBCR_CONVERSION,
      (GstVulkanHandleTypedef) ycbr_conversion,
      gst_vulkan_handle_free_sampler_ycbcr_conversion, NULL);

  gst_clear_vulkan_handle (&priv->sampler);
  priv->sampler = handle;

  return TRUE;
}

/**
 * gst_vulkan_decoder_picture_create_view:
 * @self: a #GstVulkanDecoder
 * @buf: a #GstBuffer
 * @is_out: if @buf is for output or for DPB
 *
 * Creates a #GstVulkanImageView for @buf for decoding, with the internal Ycbcr
 * sampler, if available.
 *
 * Returns: (transfer full) (nullable): the #GstVulkanImageView.
 */
GstVulkanImageView *
gst_vulkan_decoder_picture_create_view (GstVulkanDecoder * self,
    GstBuffer * buf, gboolean is_out)
{
  GstVulkanDecoderPrivate *priv;
  VkSamplerYcbcrConversionInfo yuv_sampler_info;
  VkImageViewCreateInfo view_create_info;
  GstVulkanImageMemory *vkmem;
  GstMemory *mem;
  gpointer pnext;
  guint n_mems;

  g_return_val_if_fail (GST_IS_VULKAN_DECODER (self) && GST_IS_BUFFER (buf),
      NULL);

  n_mems = gst_buffer_n_memory (buf);
  if (n_mems != 1)
    return NULL;

  mem = gst_buffer_peek_memory (buf, 0);
  if (!gst_is_vulkan_image_memory (mem))
    return NULL;

  priv = gst_vulkan_decoder_get_instance_private (self);

  pnext = NULL;
  if (priv->sampler) {
    /* *INDENT-OFF* */
    yuv_sampler_info = (VkSamplerYcbcrConversionInfo) {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
      .conversion = priv->sampler->handle,
    };
    /* *INDENT-ON* */

    pnext = &yuv_sampler_info;
  }

  vkmem = (GstVulkanImageMemory *) mem;

  /* *INDENT-OFF* */
  view_create_info = (VkImageViewCreateInfo) {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .pNext = pnext,
    .viewType = self->layered_dpb && !is_out ?
        VK_IMAGE_VIEW_TYPE_2D_ARRAY: VK_IMAGE_VIEW_TYPE_2D,
    .format = vkmem->create_info.format,
    .image = vkmem->image,
    .components = _vk_identity_component_map,
    .subresourceRange = (VkImageSubresourceRange) {
      .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseArrayLayer = 0,
      .layerCount     = self->layered_dpb && !is_out ?
          VK_REMAINING_ARRAY_LAYERS : 1,
      .levelCount     = 1,
    },
  };
  /* *INDENT-ON* */

  return gst_vulkan_get_or_create_image_view_with_info (vkmem,
      &view_create_info);
}

/**
 * gst_vulkan_decoder_picture_init:
 * @self: a #GstVulkanDecoder
 * @pic: a #GstVulkanDecoderPicture
 * @out: the #GstBuffer to use as output
 *
 * Initializes @pic with @out as output buffer.
 *
 * Returns: whether @pic was initialized.
 */
gboolean
gst_vulkan_decoder_picture_init (GstVulkanDecoder * self,
    GstVulkanDecoderPicture * pic, GstBuffer * out)
{
  GstVulkanDecoderPrivate *priv;
  GstFlowReturn ret;

  g_return_val_if_fail (GST_IS_VULKAN_DECODER (self), FALSE);
  g_return_val_if_fail (pic, FALSE);
  g_return_val_if_fail (GST_IS_BUFFER (out), FALSE);

  priv = gst_vulkan_decoder_get_instance_private (self);

  if (self->layered_dpb)
    g_return_val_if_fail (GST_IS_BUFFER (self->layered_buffer), FALSE);
  else if (self->dedicated_dpb)
    g_return_val_if_fail (GST_IS_BUFFER_POOL (priv->dpb_pool), FALSE);

  pic->out = gst_buffer_ref (out);
  pic->img_view_out =
      gst_vulkan_decoder_picture_create_view (self, pic->out, TRUE);
  g_assert (pic->img_view_out);

  pic->dpb = NULL;
  pic->img_view_ref = NULL;

  if (self->layered_dpb) {
    pic->img_view_ref =
        gst_vulkan_decoder_picture_create_view (self, self->layered_buffer,
        FALSE);
  } else if (self->dedicated_dpb) {
    ret = gst_buffer_pool_acquire_buffer (priv->dpb_pool, &pic->dpb, NULL);
    if (ret != GST_FLOW_OK)
      return FALSE;
    pic->img_view_ref =
        gst_vulkan_decoder_picture_create_view (self, pic->dpb, FALSE);
  } else {
    pic->img_view_ref = gst_vulkan_image_view_ref (pic->img_view_out);
  }

  pic->slice_offs = NULL;

  return TRUE;
}

/**
 * gst_vulkan_decoder_picture_release:
 * @pic: a #GstVulkanDecoderPicture
 *
 * Releases the internal resource of @pic.
 */
void
gst_vulkan_decoder_picture_release (GstVulkanDecoderPicture * pic)
{
  gst_clear_vulkan_image_view (&pic->img_view_ref);
  gst_clear_vulkan_image_view (&pic->img_view_out);

  gst_clear_buffer (&pic->out);
  gst_clear_buffer (&pic->dpb);

  g_clear_pointer (&pic->slice_offs, g_array_unref);
}

/**
 * gst_vulkan_decoder_append_slice:
 * @self: a #GstVulkanDecoder
 * @pic: a #GstVulkanDecoderPicture
 * @data: slice's bitstream data
 * @size: the size of @data
 * @add_startcode: whether add start code
 *
 * Appends slices's @data bitstream into @pic internal input buffer.
 *
 * Returns: whether the slice @data were added.
 */
gboolean
gst_vulkan_decoder_append_slice (GstVulkanDecoder * self,
    GstVulkanDecoderPicture * pic, const guint8 * data, size_t size,
    gboolean add_startcode)
{
  GstVulkanDecoderPrivate *priv;
  static const guint8 startcode[3] = { 0x0, 0x0, 0x1 };
  size_t new_size, cur_size, buf_size, startcode_len;
  GstBuffer *new_buf = NULL;

  g_return_val_if_fail (GST_IS_VULKAN_DECODER (self), FALSE);

  priv = gst_vulkan_decoder_get_instance_private (self);

  startcode_len = add_startcode ? sizeof (startcode) : 0;
  buf_size = self->input_buffer ? gst_buffer_get_size (self->input_buffer) : 0;
  cur_size = pic->slice_offs ?
      g_array_index (pic->slice_offs, guint32, pic->slice_offs->len - 1) : 0;
  new_size = cur_size + startcode_len + size;
  new_size = GST_ROUND_UP_N (new_size,
      priv->caps.caps.minBitstreamBufferSizeAlignment);

  if (new_size > buf_size) {
    new_buf = gst_vulkan_video_codec_buffer_new (self->queue->device,
        &self->profile, VK_BUFFER_USAGE_VIDEO_DECODE_SRC_BIT_KHR, new_size);
    if (!new_buf)
      goto error;

    if (self->input_buffer) {
      if (!gst_buffer_copy_into (new_buf, self->input_buffer,
              GST_BUFFER_COPY_MEMORY | GST_BUFFER_COPY_DEEP, 0, -1))
        goto error;
    }

    gst_clear_buffer (&self->input_buffer);
    self->input_buffer = new_buf;
  }

  /* append data */
  {
    GstBufferMapInfo mapinfo;
    guint32 offset;

    if (!gst_buffer_map (self->input_buffer, &mapinfo, GST_MAP_WRITE))
      goto error;
    memcpy (mapinfo.data + cur_size, startcode, startcode_len);
    memcpy (mapinfo.data + cur_size + startcode_len, data, size);
    gst_buffer_unmap (self->input_buffer, &mapinfo);

    if (!pic->slice_offs) {
      offset = 0;
      pic->slice_offs = g_array_new (FALSE, FALSE, sizeof (guint32));
      g_array_append_val (pic->slice_offs, offset);
    }

    offset = cur_size + startcode_len + size;
    g_array_append_val (pic->slice_offs, offset);
  }

  return TRUE;

error:
  gst_clear_buffer (&new_buf);
  return FALSE;
}

/**
 * gst_vulkan_decoder_wait:
 * @self: a #GstVulkanDecoder
 *
 * Waits indefinitely for decoding fences to signal, and queries the operation
 * result if available.
 *
 * Returns: whether the wait succeeded in waiting for all the fences to be
 *     freed.
 */
gboolean
gst_vulkan_decoder_wait (GstVulkanDecoder * self)
{
  GstVulkanDecoderPrivate *priv;
  gint32 *query = NULL;
  GError *error = NULL;

  g_return_val_if_fail (GST_IS_VULKAN_DECODER (self), FALSE);

  priv = gst_vulkan_decoder_get_instance_private (self);

  if (!gst_vulkan_operation_wait (priv->exec))
    return FALSE;

  if (!gst_vulkan_operation_get_query (priv->exec, (gpointer *) & query,
          &error)) {
    GST_WARNING_OBJECT (self, "Operation query error: %s", error->message);
    g_clear_error (&error);
  } else if (query && query[0] != 1) {
    GST_WARNING_OBJECT (self, "query result: %d", query[0]);
  }

  return TRUE;
}
