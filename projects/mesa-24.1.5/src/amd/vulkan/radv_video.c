/*
 * Copyright 2017 Advanced Micro Devices, Inc.
 * Copyright 2021 Red Hat Inc.
 * All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef _WIN32
#include "drm-uapi/amdgpu_drm.h"
#endif

#include "util/vl_zscan_data.h"
#include "vk_video/vulkan_video_codecs_common.h"
#include "ac_uvd_dec.h"
#include "ac_vcn_av1_default.h"
#include "ac_vcn_dec.h"

#include "radv_buffer.h"
#include "radv_cs.h"
#include "radv_debug.h"
#include "radv_device_memory.h"
#include "radv_entrypoints.h"
#include "radv_image.h"
#include "radv_image_view.h"
#include "radv_video.h"

#define NUM_H2645_REFS               16
#define FB_BUFFER_OFFSET             0x1000
#define FB_BUFFER_SIZE               2048
#define FB_BUFFER_SIZE_TONGA         (2048 * 64)
#define IT_SCALING_TABLE_SIZE        992
#define RDECODE_SESSION_CONTEXT_SIZE (128 * 1024)

/* Not 100% sure this isn't too much but works */
#define VID_DEFAULT_ALIGNMENT 256

static bool
radv_enable_tier2(struct radv_physical_device *pdev)
{
   const struct radv_instance *instance = radv_physical_device_instance(pdev);

   if (pdev->info.vcn_ip_version >= VCN_3_0_0 && !(instance->debug_flags & RADV_DEBUG_VIDEO_ARRAY_PATH))
      return true;
   return false;
}

static uint32_t
radv_video_get_db_alignment(struct radv_physical_device *pdev, int width, bool is_h265_main_10_or_av1)
{
   if (pdev->info.vcn_ip_version >= VCN_2_0_0 && width > 32 && is_h265_main_10_or_av1)
      return 64;
   return 32;
}

static bool
radv_vid_buffer_upload_alloc(struct radv_cmd_buffer *cmd_buffer, unsigned size, unsigned *out_offset, void **ptr)
{
   return radv_cmd_buffer_upload_alloc_aligned(cmd_buffer, size, VID_DEFAULT_ALIGNMENT, out_offset, ptr);
}

/* vcn unified queue (sq) ib header */
void
radv_vcn_sq_header(struct radeon_cmdbuf *cs, struct rvcn_sq_var *sq, bool enc)
{
   /* vcn ib signature */
   radeon_emit(cs, RADEON_VCN_SIGNATURE_SIZE);
   radeon_emit(cs, RADEON_VCN_SIGNATURE);
   sq->ib_checksum = &cs->buf[cs->cdw];
   radeon_emit(cs, 0);
   sq->ib_total_size_in_dw = &cs->buf[cs->cdw];
   radeon_emit(cs, 0);

   /* vcn ib engine info */
   radeon_emit(cs, RADEON_VCN_ENGINE_INFO_SIZE);
   radeon_emit(cs, RADEON_VCN_ENGINE_INFO);
   radeon_emit(cs, enc ? RADEON_VCN_ENGINE_TYPE_ENCODE : RADEON_VCN_ENGINE_TYPE_DECODE);
   radeon_emit(cs, 0);
}

void
radv_vcn_sq_tail(struct radeon_cmdbuf *cs, struct rvcn_sq_var *sq)
{
   uint32_t *end;
   uint32_t size_in_dw;
   uint32_t checksum = 0;

   if (sq->ib_checksum == NULL || sq->ib_total_size_in_dw == NULL)
      return;

   end = &cs->buf[cs->cdw];
   size_in_dw = end - sq->ib_total_size_in_dw - 1;
   *sq->ib_total_size_in_dw = size_in_dw;
   *(sq->ib_total_size_in_dw + 4) = size_in_dw * sizeof(uint32_t);

   for (int i = 0; i < size_in_dw; i++)
      checksum += *(sq->ib_checksum + 2 + i);

   *sq->ib_checksum = checksum;
}

static void
radv_vcn_sq_start(struct radv_cmd_buffer *cmd_buffer)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);

   radeon_check_space(device->ws, cmd_buffer->cs, 256);
   radv_vcn_sq_header(cmd_buffer->cs, &cmd_buffer->video.sq, false);
   rvcn_decode_ib_package_t *ib_header = (rvcn_decode_ib_package_t *)&(cmd_buffer->cs->buf[cmd_buffer->cs->cdw]);
   ib_header->package_size = sizeof(struct rvcn_decode_buffer_s) + sizeof(struct rvcn_decode_ib_package_s);
   cmd_buffer->cs->cdw++;
   ib_header->package_type = (RDECODE_IB_PARAM_DECODE_BUFFER);
   cmd_buffer->cs->cdw++;
   cmd_buffer->video.decode_buffer = (rvcn_decode_buffer_t *)&(cmd_buffer->cs->buf[cmd_buffer->cs->cdw]);
   cmd_buffer->cs->cdw += sizeof(struct rvcn_decode_buffer_s) / 4;
   memset(cmd_buffer->video.decode_buffer, 0, sizeof(struct rvcn_decode_buffer_s));
}

/* generate an stream handle */
static unsigned
radv_vid_alloc_stream_handle(struct radv_physical_device *pdev)
{
   unsigned stream_handle = pdev->stream_handle_base;

   stream_handle ^= ++pdev->stream_handle_counter;
   return stream_handle;
}

static void
init_uvd_decoder(struct radv_physical_device *pdev)
{
   if (pdev->info.family >= CHIP_VEGA10) {
      pdev->vid_dec_reg.data0 = RUVD_GPCOM_VCPU_DATA0_SOC15;
      pdev->vid_dec_reg.data1 = RUVD_GPCOM_VCPU_DATA1_SOC15;
      pdev->vid_dec_reg.cmd = RUVD_GPCOM_VCPU_CMD_SOC15;
      pdev->vid_dec_reg.cntl = RUVD_ENGINE_CNTL_SOC15;
   } else {
      pdev->vid_dec_reg.data0 = RUVD_GPCOM_VCPU_DATA0;
      pdev->vid_dec_reg.data1 = RUVD_GPCOM_VCPU_DATA1;
      pdev->vid_dec_reg.cmd = RUVD_GPCOM_VCPU_CMD;
      pdev->vid_dec_reg.cntl = RUVD_ENGINE_CNTL;
   }
}

static void
init_vcn_decoder(struct radv_physical_device *pdev)
{
   switch (pdev->info.vcn_ip_version) {
   case VCN_1_0_0:
   case VCN_1_0_1:
      pdev->vid_dec_reg.data0 = RDECODE_VCN1_GPCOM_VCPU_DATA0;
      pdev->vid_dec_reg.data1 = RDECODE_VCN1_GPCOM_VCPU_DATA1;
      pdev->vid_dec_reg.cmd = RDECODE_VCN1_GPCOM_VCPU_CMD;
      pdev->vid_dec_reg.cntl = RDECODE_VCN1_ENGINE_CNTL;
      break;
   case VCN_2_0_0:
   case VCN_2_0_2:
   case VCN_2_0_3:
   case VCN_2_2_0:
      pdev->vid_dec_reg.data0 = RDECODE_VCN2_GPCOM_VCPU_DATA0;
      pdev->vid_dec_reg.data1 = RDECODE_VCN2_GPCOM_VCPU_DATA1;
      pdev->vid_dec_reg.cmd = RDECODE_VCN2_GPCOM_VCPU_CMD;
      pdev->vid_dec_reg.cntl = RDECODE_VCN2_ENGINE_CNTL;
      break;
   case VCN_2_5_0:
   case VCN_2_6_0:
   case VCN_3_0_0:
   case VCN_3_0_2:
   case VCN_3_0_16:
   case VCN_3_0_33:
   case VCN_3_1_1:
   case VCN_3_1_2:
      pdev->vid_dec_reg.data0 = RDECODE_VCN2_5_GPCOM_VCPU_DATA0;
      pdev->vid_dec_reg.data1 = RDECODE_VCN2_5_GPCOM_VCPU_DATA1;
      pdev->vid_dec_reg.cmd = RDECODE_VCN2_5_GPCOM_VCPU_CMD;
      pdev->vid_dec_reg.cntl = RDECODE_VCN2_5_ENGINE_CNTL;
      break;
   case VCN_4_0_3:
      pdev->vid_addr_gfx_mode = RDECODE_ARRAY_MODE_ADDRLIB_SEL_GFX9;
      pdev->av1_version = RDECODE_AV1_VER_1;
      break;
   case VCN_4_0_0:
   case VCN_4_0_2:
   case VCN_4_0_4:
   case VCN_4_0_5:
   case VCN_4_0_6:
      pdev->vid_addr_gfx_mode = RDECODE_ARRAY_MODE_ADDRLIB_SEL_GFX11;
      pdev->av1_version = RDECODE_AV1_VER_1;
      break;
   default:
      break;
   }
}

void
radv_init_physical_device_decoder(struct radv_physical_device *pdev)
{
   if (pdev->info.vcn_ip_version >= VCN_4_0_0)
      pdev->vid_decode_ip = AMD_IP_VCN_UNIFIED;
   else if (radv_has_uvd(pdev))
      pdev->vid_decode_ip = AMD_IP_UVD;
   else
      pdev->vid_decode_ip = AMD_IP_VCN_DEC;
   pdev->av1_version = RDECODE_AV1_VER_0;

   pdev->stream_handle_counter = 0;
   pdev->stream_handle_base = 0;

   pdev->stream_handle_base = util_bitreverse(getpid());

   pdev->vid_addr_gfx_mode = RDECODE_ARRAY_MODE_LINEAR;

   if (radv_has_uvd(pdev))
      init_uvd_decoder(pdev);
   else
      init_vcn_decoder(pdev);
}

static bool
have_it(struct radv_video_session *vid)
{
   return vid->stream_type == RDECODE_CODEC_H264_PERF || vid->stream_type == RDECODE_CODEC_H265;
}

static bool
have_probs(struct radv_video_session *vid)
{
   return vid->stream_type == RDECODE_CODEC_AV1;
}

static unsigned
calc_ctx_size_h264_perf(struct radv_video_session *vid)
{
   unsigned width_in_mb, height_in_mb, ctx_size;
   unsigned width = align(vid->vk.max_coded.width, VL_MACROBLOCK_WIDTH);
   unsigned height = align(vid->vk.max_coded.height, VL_MACROBLOCK_HEIGHT);

   unsigned max_references = vid->vk.max_dpb_slots + 1;

   /* picture width & height in 16 pixel units */
   width_in_mb = width / VL_MACROBLOCK_WIDTH;
   height_in_mb = align(height / VL_MACROBLOCK_HEIGHT, 2);

   ctx_size = max_references * align(width_in_mb * height_in_mb * 192, 256);

   return ctx_size;
}

static unsigned
calc_ctx_size_h265_main(struct radv_video_session *vid)
{
   unsigned width = align(vid->vk.max_coded.width, VL_MACROBLOCK_WIDTH);
   unsigned height = align(vid->vk.max_coded.height, VL_MACROBLOCK_HEIGHT);

   unsigned max_references = vid->vk.max_dpb_slots + 1;

   if (vid->vk.max_coded.width * vid->vk.max_coded.height >= 4096 * 2000)
      max_references = MAX2(max_references, 8);
   else
      max_references = MAX2(max_references, 17);

   width = align(width, 16);
   height = align(height, 16);
   return ((width + 255) / 16) * ((height + 255) / 16) * 16 * max_references + 52 * 1024;
}

static unsigned
calc_ctx_size_h265_main10(struct radv_video_session *vid)
{
   unsigned log2_ctb_size, width_in_ctb, height_in_ctb, num_16x16_block_per_ctb;
   unsigned context_buffer_size_per_ctb_row, cm_buffer_size, max_mb_address, db_left_tile_pxl_size;
   unsigned db_left_tile_ctx_size = 4096 / 16 * (32 + 16 * 4);

   unsigned width = align(vid->vk.max_coded.width, VL_MACROBLOCK_WIDTH);
   unsigned height = align(vid->vk.max_coded.height, VL_MACROBLOCK_HEIGHT);
   unsigned coeff_10bit = 2;

   unsigned max_references = vid->vk.max_dpb_slots + 1;

   if (vid->vk.max_coded.width * vid->vk.max_coded.height >= 4096 * 2000)
      max_references = MAX2(max_references, 8);
   else
      max_references = MAX2(max_references, 17);

   /* 64x64 is the maximum ctb size. */
   log2_ctb_size = 6;

   width_in_ctb = (width + ((1 << log2_ctb_size) - 1)) >> log2_ctb_size;
   height_in_ctb = (height + ((1 << log2_ctb_size) - 1)) >> log2_ctb_size;

   num_16x16_block_per_ctb = ((1 << log2_ctb_size) >> 4) * ((1 << log2_ctb_size) >> 4);
   context_buffer_size_per_ctb_row = align(width_in_ctb * num_16x16_block_per_ctb * 16, 256);
   max_mb_address = (unsigned)ceil(height * 8 / 2048.0);

   cm_buffer_size = max_references * context_buffer_size_per_ctb_row * height_in_ctb;
   db_left_tile_pxl_size = coeff_10bit * (max_mb_address * 2 * 2048 + 1024);

   return cm_buffer_size + db_left_tile_ctx_size + db_left_tile_pxl_size;
}

static unsigned
calc_ctx_size_av1(struct radv_device *device, struct radv_video_session *vid)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);
   unsigned frame_ctxt_size = pdev->av1_version == RDECODE_AV1_VER_0
                                 ? align(sizeof(rvcn_av1_frame_context_t), 2048)
                                 : align(sizeof(rvcn_av1_vcn4_frame_context_t), 2048);
   unsigned ctx_size = (9 + 4) * frame_ctxt_size + 9 * 64 * 34 * 512 + 9 * 64 * 34 * 256 * 5;

   int num_64x64_CTB_8k = 68;
   int num_128x128_CTB_8k = 34;
   int sdb_pitch_64x64 = align(32 * num_64x64_CTB_8k, 256) * 2;
   int sdb_pitch_128x128 = align(32 * num_128x128_CTB_8k, 256) * 2;
   int sdb_lf_size_ctb_64x64 = sdb_pitch_64x64 * (align(1728, 64) / 64);
   int sdb_lf_size_ctb_128x128 = sdb_pitch_128x128 * (align(3008, 64) / 64);
   int sdb_superres_size_ctb_64x64 = sdb_pitch_64x64 * (align(3232, 64) / 64);
   int sdb_superres_size_ctb_128x128 = sdb_pitch_128x128 * (align(6208, 64) / 64);
   int sdb_output_size_ctb_64x64 = sdb_pitch_64x64 * (align(1312, 64) / 64);
   int sdb_output_size_ctb_128x128 = sdb_pitch_128x128 * (align(2336, 64) / 64);
   int sdb_fg_avg_luma_size_ctb_64x64 = sdb_pitch_64x64 * (align(384, 64) / 64);
   int sdb_fg_avg_luma_size_ctb_128x128 = sdb_pitch_128x128 * (align(640, 64) / 64);

   ctx_size += (MAX2(sdb_lf_size_ctb_64x64, sdb_lf_size_ctb_128x128) +
                MAX2(sdb_superres_size_ctb_64x64, sdb_superres_size_ctb_128x128) +
                MAX2(sdb_output_size_ctb_64x64, sdb_output_size_ctb_128x128) +
                MAX2(sdb_fg_avg_luma_size_ctb_64x64, sdb_fg_avg_luma_size_ctb_128x128)) *
                  2 +
               68 * 512;

   return ctx_size;
}

static void
radv_video_patch_session_parameters(struct vk_video_session_parameters *params)
{
   switch (params->op) {
   case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
   case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
   default:
      return;
   case VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR:
   case VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR:
      radv_video_patch_encode_session_parameters(params);
      break;
   }
}

VKAPI_ATTR VkResult VKAPI_CALL
radv_CreateVideoSessionKHR(VkDevice _device, const VkVideoSessionCreateInfoKHR *pCreateInfo,
                           const VkAllocationCallbacks *pAllocator, VkVideoSessionKHR *pVideoSession)
{
   VK_FROM_HANDLE(radv_device, device, _device);
   struct radv_physical_device *pdev = radv_device_physical(device);
   const struct radv_instance *instance = radv_physical_device_instance(pdev);

   struct radv_video_session *vid =
      vk_alloc2(&device->vk.alloc, pAllocator, sizeof(*vid), 8, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!vid)
      return vk_error(instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   memset(vid, 0, sizeof(struct radv_video_session));

   VkResult result = vk_video_session_init(&device->vk, &vid->vk, pCreateInfo);
   if (result != VK_SUCCESS) {
      vk_free2(&device->vk.alloc, pAllocator, vid);
      return result;
   }

   vid->interlaced = false;
   vid->dpb_type = DPB_MAX_RES;

   switch (vid->vk.op) {
   case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
      vid->stream_type = RDECODE_CODEC_H264_PERF;
      if (radv_enable_tier2(pdev))
         vid->dpb_type = DPB_DYNAMIC_TIER_2;
      break;
   case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
      vid->stream_type = RDECODE_CODEC_H265;
      if (radv_enable_tier2(pdev))
         vid->dpb_type = DPB_DYNAMIC_TIER_2;
      break;
   case VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR:
      vid->stream_type = RDECODE_CODEC_AV1;
      vid->dpb_type = DPB_DYNAMIC_TIER_2;
      break;
   case VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR:
      vid->encode = true;
      vid->enc_session.encode_standard = RENCODE_ENCODE_STANDARD_H264;
      vid->enc_session.aligned_picture_width = align(vid->vk.max_coded.width, 16);
      vid->enc_session.aligned_picture_height = align(vid->vk.max_coded.height, 16);
      vid->enc_session.padding_width = vid->enc_session.aligned_picture_width - vid->vk.max_coded.width;
      vid->enc_session.padding_height = vid->enc_session.aligned_picture_height - vid->vk.max_coded.height;
      vid->enc_session.display_remote = 0;
      vid->enc_session.pre_encode_mode = 0;
      vid->enc_session.pre_encode_chroma_enabled = 0;
      switch (vid->vk.enc_usage.tuning_mode) {
      case VK_VIDEO_ENCODE_TUNING_MODE_DEFAULT_KHR:
      default:
         vid->enc_preset_mode = RENCODE_PRESET_MODE_BALANCE;
         break;
      case VK_VIDEO_ENCODE_TUNING_MODE_LOW_LATENCY_KHR:
      case VK_VIDEO_ENCODE_TUNING_MODE_ULTRA_LOW_LATENCY_KHR:
         vid->enc_preset_mode = RENCODE_PRESET_MODE_SPEED;
         break;
      case VK_VIDEO_ENCODE_TUNING_MODE_HIGH_QUALITY_KHR:
      case VK_VIDEO_ENCODE_TUNING_MODE_LOSSLESS_KHR:
         vid->enc_preset_mode = RENCODE_PRESET_MODE_QUALITY;
         break;
      }
      break;
   case VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR:
      vid->encode = true;
      vid->enc_session.encode_standard = RENCODE_ENCODE_STANDARD_HEVC;
      vid->enc_session.aligned_picture_width = align(vid->vk.max_coded.width, 64);
      vid->enc_session.aligned_picture_height = align(vid->vk.max_coded.height, 64);
      vid->enc_session.padding_width = vid->enc_session.aligned_picture_width - vid->vk.max_coded.width;
      vid->enc_session.padding_height = vid->enc_session.aligned_picture_height - vid->vk.max_coded.height;
      vid->enc_session.display_remote = 0;
      vid->enc_session.pre_encode_mode = 0;
      vid->enc_session.pre_encode_chroma_enabled = 0;
      switch (vid->vk.enc_usage.tuning_mode) {
      case VK_VIDEO_ENCODE_TUNING_MODE_DEFAULT_KHR:
      default:
         vid->enc_preset_mode = RENCODE_PRESET_MODE_BALANCE;
         break;
      case VK_VIDEO_ENCODE_TUNING_MODE_LOW_LATENCY_KHR:
      case VK_VIDEO_ENCODE_TUNING_MODE_ULTRA_LOW_LATENCY_KHR:
         vid->enc_preset_mode = RENCODE_PRESET_MODE_SPEED;
         break;
      case VK_VIDEO_ENCODE_TUNING_MODE_HIGH_QUALITY_KHR:
      case VK_VIDEO_ENCODE_TUNING_MODE_LOSSLESS_KHR:
         vid->enc_preset_mode = RENCODE_PRESET_MODE_QUALITY;
         break;
      }
      break;
   default:
      return VK_ERROR_FEATURE_NOT_PRESENT;
   }

   vid->stream_handle = radv_vid_alloc_stream_handle(pdev);
   vid->dbg_frame_cnt = 0;
   vid->db_alignment = radv_video_get_db_alignment(
      pdev, vid->vk.max_coded.width,
      (vid->stream_type == RDECODE_CODEC_AV1 ||
       (vid->stream_type == RDECODE_CODEC_H265 && vid->vk.h265.profile_idc == STD_VIDEO_H265_PROFILE_IDC_MAIN_10)));

   *pVideoSession = radv_video_session_to_handle(vid);
   return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
radv_DestroyVideoSessionKHR(VkDevice _device, VkVideoSessionKHR _session, const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(radv_device, device, _device);
   VK_FROM_HANDLE(radv_video_session, vid, _session);
   if (!_session)
      return;

   vk_object_base_finish(&vid->vk.base);
   vk_free2(&device->vk.alloc, pAllocator, vid);
}

VKAPI_ATTR VkResult VKAPI_CALL
radv_CreateVideoSessionParametersKHR(VkDevice _device, const VkVideoSessionParametersCreateInfoKHR *pCreateInfo,
                                     const VkAllocationCallbacks *pAllocator,
                                     VkVideoSessionParametersKHR *pVideoSessionParameters)
{
   VK_FROM_HANDLE(radv_device, device, _device);
   VK_FROM_HANDLE(radv_video_session, vid, pCreateInfo->videoSession);
   VK_FROM_HANDLE(radv_video_session_params, templ, pCreateInfo->videoSessionParametersTemplate);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   const struct radv_instance *instance = radv_physical_device_instance(pdev);
   struct radv_video_session_params *params =
      vk_alloc2(&device->vk.alloc, pAllocator, sizeof(*params), 8, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!params)
      return vk_error(instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   VkResult result =
      vk_video_session_parameters_init(&device->vk, &params->vk, &vid->vk, templ ? &templ->vk : NULL, pCreateInfo);
   if (result != VK_SUCCESS) {
      vk_free2(&device->vk.alloc, pAllocator, params);
      return result;
   }

   radv_video_patch_session_parameters(&params->vk);

   *pVideoSessionParameters = radv_video_session_params_to_handle(params);
   return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
radv_DestroyVideoSessionParametersKHR(VkDevice _device, VkVideoSessionParametersKHR _params,
                                      const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(radv_device, device, _device);
   VK_FROM_HANDLE(radv_video_session_params, params, _params);

   vk_video_session_parameters_finish(&device->vk, &params->vk);
   vk_free2(&device->vk.alloc, pAllocator, params);
}

VKAPI_ATTR VkResult VKAPI_CALL
radv_GetPhysicalDeviceVideoCapabilitiesKHR(VkPhysicalDevice physicalDevice, const VkVideoProfileInfoKHR *pVideoProfile,
                                           VkVideoCapabilitiesKHR *pCapabilities)
{
   VK_FROM_HANDLE(radv_physical_device, pdev, physicalDevice);
   const struct video_codec_cap *cap = NULL;
   bool is_encode = false;

   switch (pVideoProfile->videoCodecOperation) {
#ifndef _WIN32
   case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
      cap = &pdev->info.dec_caps.codec_info[AMDGPU_INFO_VIDEO_CAPS_CODEC_IDX_MPEG4_AVC];
      break;
   case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
      cap = &pdev->info.dec_caps.codec_info[AMDGPU_INFO_VIDEO_CAPS_CODEC_IDX_HEVC];
      break;
   case VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR:
      cap = &pdev->info.dec_caps.codec_info[AMDGPU_INFO_VIDEO_CAPS_CODEC_IDX_AV1];
      break;
   case VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR:
      cap = &pdev->info.enc_caps.codec_info[AMDGPU_INFO_VIDEO_CAPS_CODEC_IDX_MPEG4_AVC];
      is_encode = true;
      break;
   case VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR:
      cap = &pdev->info.enc_caps.codec_info[AMDGPU_INFO_VIDEO_CAPS_CODEC_IDX_HEVC];
      is_encode = true;
      break;
#endif
   default:
      unreachable("unsupported operation");
   }

   if (cap && !cap->valid)
      cap = NULL;

   pCapabilities->flags = 0;
   pCapabilities->pictureAccessGranularity.width = VL_MACROBLOCK_WIDTH;
   pCapabilities->pictureAccessGranularity.height = VL_MACROBLOCK_HEIGHT;
   pCapabilities->minCodedExtent.width = VL_MACROBLOCK_WIDTH;
   pCapabilities->minCodedExtent.height = VL_MACROBLOCK_HEIGHT;

   struct VkVideoDecodeCapabilitiesKHR *dec_caps = NULL;
   struct VkVideoEncodeCapabilitiesKHR *enc_caps = NULL;
   if (!is_encode) {
      dec_caps =
         (struct VkVideoDecodeCapabilitiesKHR *)vk_find_struct(pCapabilities->pNext, VIDEO_DECODE_CAPABILITIES_KHR);
      if (dec_caps)
         dec_caps->flags = VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_DISTINCT_BIT_KHR;
      pCapabilities->minBitstreamBufferOffsetAlignment = 128;
      pCapabilities->minBitstreamBufferSizeAlignment = 128;
   } else {
      enc_caps =
         (struct VkVideoEncodeCapabilitiesKHR *)vk_find_struct(pCapabilities->pNext, VIDEO_ENCODE_CAPABILITIES_KHR);

      if (enc_caps) {
         enc_caps->flags = 0;
         enc_caps->rateControlModes = VK_VIDEO_ENCODE_RATE_CONTROL_MODE_DISABLED_BIT_KHR |
                                      VK_VIDEO_ENCODE_RATE_CONTROL_MODE_CBR_BIT_KHR |
                                      VK_VIDEO_ENCODE_RATE_CONTROL_MODE_VBR_BIT_KHR;
         enc_caps->maxRateControlLayers = RADV_ENC_MAX_RATE_LAYER;
         enc_caps->maxBitrate = 1000000000;
         enc_caps->maxQualityLevels = 2;
         enc_caps->encodeInputPictureGranularity.width = 1;
         enc_caps->encodeInputPictureGranularity.height = 1;
         enc_caps->supportedEncodeFeedbackFlags = VK_VIDEO_ENCODE_FEEDBACK_BITSTREAM_BUFFER_OFFSET_BIT_KHR |
                                                  VK_VIDEO_ENCODE_FEEDBACK_BITSTREAM_BYTES_WRITTEN_BIT_KHR;
      }
      pCapabilities->minBitstreamBufferOffsetAlignment = 16;
      pCapabilities->minBitstreamBufferSizeAlignment = 16;
   }

   switch (pVideoProfile->videoCodecOperation) {
   case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR: {
      /* H264 allows different luma and chroma bit depths */
      if (pVideoProfile->lumaBitDepth != pVideoProfile->chromaBitDepth)
         return VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR;

      struct VkVideoDecodeH264CapabilitiesKHR *ext = (struct VkVideoDecodeH264CapabilitiesKHR *)vk_find_struct(
         pCapabilities->pNext, VIDEO_DECODE_H264_CAPABILITIES_KHR);

      const struct VkVideoDecodeH264ProfileInfoKHR *h264_profile =
         vk_find_struct_const(pVideoProfile->pNext, VIDEO_DECODE_H264_PROFILE_INFO_KHR);

      if (h264_profile->stdProfileIdc != STD_VIDEO_H264_PROFILE_IDC_BASELINE &&
          h264_profile->stdProfileIdc != STD_VIDEO_H264_PROFILE_IDC_MAIN &&
          h264_profile->stdProfileIdc != STD_VIDEO_H264_PROFILE_IDC_HIGH)
         return VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR;

      if (pVideoProfile->lumaBitDepth != VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR)
         return VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR;

      pCapabilities->maxDpbSlots = NUM_H2645_REFS + 1;
      pCapabilities->maxActiveReferencePictures = NUM_H2645_REFS;

      /* for h264 on navi21+ separate dpb images should work */
      if (radv_enable_tier2(pdev))
         pCapabilities->flags |= VK_VIDEO_CAPABILITY_SEPARATE_REFERENCE_IMAGES_BIT_KHR;
      ext->fieldOffsetGranularity.x = 0;
      ext->fieldOffsetGranularity.y = 0;
      ext->maxLevelIdc = STD_VIDEO_H264_LEVEL_IDC_5_1;
      strcpy(pCapabilities->stdHeaderVersion.extensionName, VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_EXTENSION_NAME);
      pCapabilities->stdHeaderVersion.specVersion = VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_SPEC_VERSION;
      break;
   }
   case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR: {
      /* H265 allows different luma and chroma bit depths */
      if (pVideoProfile->lumaBitDepth != pVideoProfile->chromaBitDepth)
         return VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR;

      struct VkVideoDecodeH265CapabilitiesKHR *ext = (struct VkVideoDecodeH265CapabilitiesKHR *)vk_find_struct(
         pCapabilities->pNext, VIDEO_DECODE_H265_CAPABILITIES_KHR);

      const struct VkVideoDecodeH265ProfileInfoKHR *h265_profile =
         vk_find_struct_const(pVideoProfile->pNext, VIDEO_DECODE_H265_PROFILE_INFO_KHR);

      if (h265_profile->stdProfileIdc != STD_VIDEO_H265_PROFILE_IDC_MAIN &&
          h265_profile->stdProfileIdc != STD_VIDEO_H265_PROFILE_IDC_MAIN_10 &&
          h265_profile->stdProfileIdc != STD_VIDEO_H265_PROFILE_IDC_MAIN_STILL_PICTURE)
         return VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR;

      if (pVideoProfile->lumaBitDepth != VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR &&
          pVideoProfile->lumaBitDepth != VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR)
         return VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR;

      pCapabilities->maxDpbSlots = NUM_H2645_REFS + 1;
      pCapabilities->maxActiveReferencePictures = NUM_H2645_REFS;
      /* for h265 on navi21+ separate dpb images should work */
      if (radv_enable_tier2(pdev))
         pCapabilities->flags |= VK_VIDEO_CAPABILITY_SEPARATE_REFERENCE_IMAGES_BIT_KHR;
      ext->maxLevelIdc = STD_VIDEO_H265_LEVEL_IDC_5_1;
      strcpy(pCapabilities->stdHeaderVersion.extensionName, VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_EXTENSION_NAME);
      pCapabilities->stdHeaderVersion.specVersion = VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_SPEC_VERSION;
      break;
   }
   case VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR: {
      /* Monochrome sampling implies an undefined chroma bit depth, and is supported in profile MAIN for AV1. */
      if (pVideoProfile->chromaSubsampling != VK_VIDEO_CHROMA_SUBSAMPLING_MONOCHROME_BIT_KHR &&
          pVideoProfile->lumaBitDepth != pVideoProfile->chromaBitDepth)
         return VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR;
      struct VkVideoDecodeAV1CapabilitiesKHR *ext =
         vk_find_struct(pCapabilities->pNext, VIDEO_DECODE_AV1_CAPABILITIES_KHR);

      const struct VkVideoDecodeAV1ProfileInfoKHR *av1_profile =
         vk_find_struct_const(pVideoProfile->pNext, VIDEO_DECODE_AV1_PROFILE_INFO_KHR);

      if (av1_profile->stdProfile != STD_VIDEO_AV1_PROFILE_MAIN)
         return VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR;

      if (pVideoProfile->lumaBitDepth != VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR &&
          pVideoProfile->lumaBitDepth != VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR)
         return VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR;

      pCapabilities->maxDpbSlots = 9;
      pCapabilities->maxActiveReferencePictures = STD_VIDEO_AV1_NUM_REF_FRAMES;
      pCapabilities->flags |= VK_VIDEO_CAPABILITY_SEPARATE_REFERENCE_IMAGES_BIT_KHR;
      ext->maxLevel = STD_VIDEO_AV1_LEVEL_6_1; /* For VCN3/4, the only h/w currently with AV1 decode support */
      strcpy(pCapabilities->stdHeaderVersion.extensionName, VK_STD_VULKAN_VIDEO_CODEC_AV1_DECODE_EXTENSION_NAME);
      pCapabilities->stdHeaderVersion.specVersion = VK_STD_VULKAN_VIDEO_CODEC_AV1_DECODE_SPEC_VERSION;
      break;
   }
   case VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR: {
      struct VkVideoEncodeH264CapabilitiesKHR *ext = (struct VkVideoEncodeH264CapabilitiesKHR *)vk_find_struct(
         pCapabilities->pNext, VIDEO_ENCODE_H264_CAPABILITIES_KHR);

      const struct VkVideoEncodeH264ProfileInfoKHR *h264_profile =
         vk_find_struct_const(pVideoProfile->pNext, VIDEO_ENCODE_H264_PROFILE_INFO_KHR);

      if (h264_profile->stdProfileIdc != STD_VIDEO_H264_PROFILE_IDC_BASELINE &&
          h264_profile->stdProfileIdc != STD_VIDEO_H264_PROFILE_IDC_MAIN &&
          h264_profile->stdProfileIdc != STD_VIDEO_H264_PROFILE_IDC_HIGH)
         return VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR;

      if (pVideoProfile->lumaBitDepth != VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR)
         return VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR;

      pCapabilities->maxDpbSlots = NUM_H2645_REFS;
      pCapabilities->maxActiveReferencePictures = NUM_H2645_REFS;
      ext->flags = VK_VIDEO_ENCODE_H264_CAPABILITY_HRD_COMPLIANCE_BIT_KHR |
                   VK_VIDEO_ENCODE_H264_CAPABILITY_PER_PICTURE_TYPE_MIN_MAX_QP_BIT_KHR;
      ext->maxLevelIdc = cap ? cap->max_level : 0;
      ext->maxSliceCount = 128;
      ext->maxPPictureL0ReferenceCount = 1;
      ext->maxBPictureL0ReferenceCount = 0;
      ext->maxL1ReferenceCount = 0;
      ext->maxTemporalLayerCount = 4;
      ext->expectDyadicTemporalLayerPattern = false;
      ext->minQp = 0;
      ext->maxQp = 51;
      ext->prefersGopRemainingFrames = false;
      ext->requiresGopRemainingFrames = false;
      ext->stdSyntaxFlags = VK_VIDEO_ENCODE_H264_STD_CONSTRAINED_INTRA_PRED_FLAG_SET_BIT_KHR |
                            VK_VIDEO_ENCODE_H264_STD_ENTROPY_CODING_MODE_FLAG_UNSET_BIT_KHR |
                            VK_VIDEO_ENCODE_H264_STD_ENTROPY_CODING_MODE_FLAG_SET_BIT_KHR;
      if (pdev->enc_hw_ver >= RADV_VIDEO_ENC_HW_3)
         ext->stdSyntaxFlags |= VK_VIDEO_ENCODE_H264_STD_WEIGHTED_BIPRED_IDC_EXPLICIT_BIT_KHR;

      strcpy(pCapabilities->stdHeaderVersion.extensionName, VK_STD_VULKAN_VIDEO_CODEC_H264_ENCODE_EXTENSION_NAME);
      pCapabilities->stdHeaderVersion.specVersion = VK_STD_VULKAN_VIDEO_CODEC_H264_ENCODE_SPEC_VERSION;
      break;
   }
   case VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR: {
      struct VkVideoEncodeH265CapabilitiesKHR *ext = (struct VkVideoEncodeH265CapabilitiesKHR *)vk_find_struct(
         pCapabilities->pNext, VIDEO_ENCODE_H265_CAPABILITIES_KHR);

      const struct VkVideoEncodeH265ProfileInfoKHR *h265_profile =
         vk_find_struct_const(pVideoProfile->pNext, VIDEO_ENCODE_H265_PROFILE_INFO_KHR);

      if (h265_profile->stdProfileIdc != STD_VIDEO_H265_PROFILE_IDC_MAIN &&
          (pdev->enc_hw_ver < RADV_VIDEO_ENC_HW_2 ||
           h265_profile->stdProfileIdc != STD_VIDEO_H265_PROFILE_IDC_MAIN_10))
         return VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR;

      if (pVideoProfile->lumaBitDepth != VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR &&
          (pdev->enc_hw_ver < RADV_VIDEO_ENC_HW_2 ||
           pVideoProfile->lumaBitDepth != VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR))
         return VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR;

      pCapabilities->maxDpbSlots = NUM_H2645_REFS;
      pCapabilities->maxActiveReferencePictures = NUM_H2645_REFS;
      ext->flags = VK_VIDEO_ENCODE_H265_CAPABILITY_PER_PICTURE_TYPE_MIN_MAX_QP_BIT_KHR;
      ext->maxLevelIdc = cap ? cap->max_level : 0;
      ext->maxSliceSegmentCount = 128;
      ext->maxTiles.width = 1;
      ext->maxTiles.height = 1;
      ext->ctbSizes = VK_VIDEO_ENCODE_H265_CTB_SIZE_64_BIT_KHR;
      ext->transformBlockSizes =
         VK_VIDEO_ENCODE_H265_TRANSFORM_BLOCK_SIZE_4_BIT_KHR | VK_VIDEO_ENCODE_H265_TRANSFORM_BLOCK_SIZE_8_BIT_KHR |
         VK_VIDEO_ENCODE_H265_TRANSFORM_BLOCK_SIZE_16_BIT_KHR | VK_VIDEO_ENCODE_H265_TRANSFORM_BLOCK_SIZE_32_BIT_KHR;
      ext->maxPPictureL0ReferenceCount = 1;
      ext->maxBPictureL0ReferenceCount = 0;
      ext->maxL1ReferenceCount = 0;
      ext->maxSubLayerCount = 4;
      ext->expectDyadicTemporalSubLayerPattern = false;
      ext->minQp = 0;
      ext->maxQp = 51;
      ext->prefersGopRemainingFrames = false;
      ext->requiresGopRemainingFrames = false;
      ext->stdSyntaxFlags = VK_VIDEO_ENCODE_H265_STD_CONSTRAINED_INTRA_PRED_FLAG_SET_BIT_KHR |
                            VK_VIDEO_ENCODE_H265_STD_DEBLOCKING_FILTER_OVERRIDE_ENABLED_FLAG_SET_BIT_KHR |
                            VK_VIDEO_ENCODE_H265_STD_CONSTRAINED_INTRA_PRED_FLAG_SET_BIT_KHR |
                            VK_VIDEO_ENCODE_H265_STD_ENTROPY_CODING_SYNC_ENABLED_FLAG_SET_BIT_KHR;

      if (pdev->enc_hw_ver >= RADV_VIDEO_ENC_HW_2)
         ext->stdSyntaxFlags |= VK_VIDEO_ENCODE_H265_STD_SAMPLE_ADAPTIVE_OFFSET_ENABLED_FLAG_SET_BIT_KHR;

      if (pdev->enc_hw_ver >= RADV_VIDEO_ENC_HW_3)
         ext->stdSyntaxFlags |= VK_VIDEO_ENCODE_H265_STD_TRANSFORM_SKIP_ENABLED_FLAG_SET_BIT_KHR;
      strcpy(pCapabilities->stdHeaderVersion.extensionName, VK_STD_VULKAN_VIDEO_CODEC_H265_ENCODE_EXTENSION_NAME);
      pCapabilities->stdHeaderVersion.specVersion = VK_STD_VULKAN_VIDEO_CODEC_H265_ENCODE_SPEC_VERSION;
      break;
   }
   default:
      break;
   }

   if (cap) {
      pCapabilities->maxCodedExtent.width = cap->max_width;
      pCapabilities->maxCodedExtent.height = cap->max_height;
   } else {
      switch (pVideoProfile->videoCodecOperation) {
      case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
         pCapabilities->maxCodedExtent.width = (pdev->info.family < CHIP_TONGA) ? 2048 : 4096;
         pCapabilities->maxCodedExtent.height = (pdev->info.family < CHIP_TONGA) ? 1152 : 4096;
         break;
      case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
         pCapabilities->maxCodedExtent.width =
            (pdev->info.family < CHIP_RENOIR) ? ((pdev->info.family < CHIP_TONGA) ? 2048 : 4096) : 8192;
         pCapabilities->maxCodedExtent.height =
            (pdev->info.family < CHIP_RENOIR) ? ((pdev->info.family < CHIP_TONGA) ? 1152 : 4096) : 4352;
         break;
      default:
         break;
      }
   }

   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
radv_GetPhysicalDeviceVideoFormatPropertiesKHR(VkPhysicalDevice physicalDevice,
                                               const VkPhysicalDeviceVideoFormatInfoKHR *pVideoFormatInfo,
                                               uint32_t *pVideoFormatPropertyCount,
                                               VkVideoFormatPropertiesKHR *pVideoFormatProperties)
{
   /* radv requires separate allocates for DPB and decode video. */
   if ((pVideoFormatInfo->imageUsage &
        (VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR | VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR)) ==
       (VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR | VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR))
      return VK_ERROR_FORMAT_NOT_SUPPORTED;

   VK_OUTARRAY_MAKE_TYPED(VkVideoFormatPropertiesKHR, out, pVideoFormatProperties, pVideoFormatPropertyCount);

   bool need_8bit = true;
   bool need_10bit = false;
   const struct VkVideoProfileListInfoKHR *prof_list =
      (struct VkVideoProfileListInfoKHR *)vk_find_struct_const(pVideoFormatInfo->pNext, VIDEO_PROFILE_LIST_INFO_KHR);
   if (prof_list) {
      for (unsigned i = 0; i < prof_list->profileCount; i++) {
         const VkVideoProfileInfoKHR *profile = &prof_list->pProfiles[i];
         if (profile->lumaBitDepth & VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR)
            need_10bit = true;
      }
   }

   if (need_10bit) {
      vk_outarray_append_typed(VkVideoFormatPropertiesKHR, &out, p)
      {
         p->format = VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16;
         p->componentMapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
         p->componentMapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
         p->componentMapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
         p->componentMapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;
         p->imageCreateFlags = 0;
         if (pVideoFormatInfo->imageUsage & (VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR | VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR))
            p->imageCreateFlags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;
         p->imageType = VK_IMAGE_TYPE_2D;
         p->imageTiling = VK_IMAGE_TILING_OPTIMAL;
         p->imageUsageFlags = pVideoFormatInfo->imageUsage;
      }

      if (pVideoFormatInfo->imageUsage & (VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR))
         need_8bit = false;
   }

   if (need_8bit) {
      vk_outarray_append_typed(VkVideoFormatPropertiesKHR, &out, p)
      {
         p->format = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
         p->componentMapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
         p->componentMapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
         p->componentMapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
         p->componentMapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;
         p->imageCreateFlags = 0;
         if (pVideoFormatInfo->imageUsage & (VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR | VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR))
            p->imageCreateFlags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;
         p->imageType = VK_IMAGE_TYPE_2D;
         p->imageTiling = VK_IMAGE_TILING_OPTIMAL;
         p->imageUsageFlags = pVideoFormatInfo->imageUsage;
      }
   }

   return vk_outarray_status(&out);
}

#define RADV_BIND_SESSION_CTX 0
#define RADV_BIND_DECODER_CTX 1

VKAPI_ATTR VkResult VKAPI_CALL
radv_GetVideoSessionMemoryRequirementsKHR(VkDevice _device, VkVideoSessionKHR videoSession,
                                          uint32_t *pMemoryRequirementsCount,
                                          VkVideoSessionMemoryRequirementsKHR *pMemoryRequirements)
{
   VK_FROM_HANDLE(radv_device, device, _device);
   VK_FROM_HANDLE(radv_video_session, vid, videoSession);
   const struct radv_physical_device *pdev = radv_device_physical(device);

   uint32_t memory_type_bits = (1u << pdev->memory_properties.memoryTypeCount) - 1;

   if (vid->encode) {
      return radv_video_get_encode_session_memory_requirements(device, vid, pMemoryRequirementsCount,
                                                               pMemoryRequirements);
   }
   VK_OUTARRAY_MAKE_TYPED(VkVideoSessionMemoryRequirementsKHR, out, pMemoryRequirements, pMemoryRequirementsCount);
   /* 1 buffer for session context */
   if (pdev->info.family >= CHIP_POLARIS10) {
      vk_outarray_append_typed(VkVideoSessionMemoryRequirementsKHR, &out, m)
      {
         m->memoryBindIndex = RADV_BIND_SESSION_CTX;
         m->memoryRequirements.size = RDECODE_SESSION_CONTEXT_SIZE;
         m->memoryRequirements.alignment = 0;
         m->memoryRequirements.memoryTypeBits = memory_type_bits;
      }
   }

   if (vid->stream_type == RDECODE_CODEC_H264_PERF && pdev->info.family >= CHIP_POLARIS10) {
      vk_outarray_append_typed(VkVideoSessionMemoryRequirementsKHR, &out, m)
      {
         m->memoryBindIndex = RADV_BIND_DECODER_CTX;
         m->memoryRequirements.size = align(calc_ctx_size_h264_perf(vid), 4096);
         m->memoryRequirements.alignment = 0;
         m->memoryRequirements.memoryTypeBits = memory_type_bits;
      }
   }
   if (vid->stream_type == RDECODE_CODEC_H265) {
      uint32_t ctx_size;

      if (vid->vk.h265.profile_idc == STD_VIDEO_H265_PROFILE_IDC_MAIN_10)
         ctx_size = calc_ctx_size_h265_main10(vid);
      else
         ctx_size = calc_ctx_size_h265_main(vid);
      vk_outarray_append_typed(VkVideoSessionMemoryRequirementsKHR, &out, m)
      {
         m->memoryBindIndex = RADV_BIND_DECODER_CTX;
         m->memoryRequirements.size = align(ctx_size, 4096);
         m->memoryRequirements.alignment = 0;
         m->memoryRequirements.memoryTypeBits = memory_type_bits;
      }
   }
   if (vid->stream_type == RDECODE_CODEC_AV1) {
      vk_outarray_append_typed(VkVideoSessionMemoryRequirementsKHR, &out, m)
      {
         m->memoryBindIndex = RADV_BIND_DECODER_CTX;
         m->memoryRequirements.size = align(calc_ctx_size_av1(device, vid), 4096);
         m->memoryRequirements.alignment = 0;
         m->memoryRequirements.memoryTypeBits = 0;
         for (unsigned i = 0; i < pdev->memory_properties.memoryTypeCount; i++)
            if (pdev->memory_properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
               m->memoryRequirements.memoryTypeBits |= (1 << i);
      }
   }
   return vk_outarray_status(&out);
}

VKAPI_ATTR VkResult VKAPI_CALL
radv_UpdateVideoSessionParametersKHR(VkDevice _device, VkVideoSessionParametersKHR videoSessionParameters,
                                     const VkVideoSessionParametersUpdateInfoKHR *pUpdateInfo)
{
   VK_FROM_HANDLE(radv_video_session_params, params, videoSessionParameters);

   VkResult result = vk_video_session_parameters_update(&params->vk, pUpdateInfo);
   if (result != VK_SUCCESS)
      return result;
   radv_video_patch_session_parameters(&params->vk);
   return result;
}

static void
copy_bind(struct radv_vid_mem *dst, const VkBindVideoSessionMemoryInfoKHR *src)
{
   dst->mem = radv_device_memory_from_handle(src->memory);
   dst->offset = src->memoryOffset;
   dst->size = src->memorySize;
}

VKAPI_ATTR VkResult VKAPI_CALL
radv_BindVideoSessionMemoryKHR(VkDevice _device, VkVideoSessionKHR videoSession, uint32_t videoSessionBindMemoryCount,
                               const VkBindVideoSessionMemoryInfoKHR *pBindSessionMemoryInfos)
{
   VK_FROM_HANDLE(radv_video_session, vid, videoSession);

   for (unsigned i = 0; i < videoSessionBindMemoryCount; i++) {
      switch (pBindSessionMemoryInfos[i].memoryBindIndex) {
      case RADV_BIND_SESSION_CTX:
         copy_bind(&vid->sessionctx, &pBindSessionMemoryInfos[i]);
         break;
      case RADV_BIND_DECODER_CTX:
         copy_bind(&vid->ctx, &pBindSessionMemoryInfos[i]);
         break;
      default:
         assert(0);
         break;
      }
   }
   return VK_SUCCESS;
}

/* add a new set register command to the IB */
static void
set_reg(struct radv_cmd_buffer *cmd_buffer, unsigned reg, uint32_t val)
{
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   radeon_emit(cs, RDECODE_PKT0(reg >> 2, 0));
   radeon_emit(cs, val);
}

static void
send_cmd(struct radv_cmd_buffer *cmd_buffer, unsigned cmd, struct radeon_winsys_bo *bo, uint32_t offset)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   uint64_t addr;

   radv_cs_add_buffer(device->ws, cmd_buffer->cs, bo);
   addr = radv_buffer_get_va(bo);
   addr += offset;

   if (pdev->vid_decode_ip != AMD_IP_VCN_UNIFIED) {
      radeon_check_space(device->ws, cmd_buffer->cs, 6);
      set_reg(cmd_buffer, pdev->vid_dec_reg.data0, addr);
      set_reg(cmd_buffer, pdev->vid_dec_reg.data1, addr >> 32);
      set_reg(cmd_buffer, pdev->vid_dec_reg.cmd, cmd << 1);
      return;
   }
   switch (cmd) {
   case RDECODE_CMD_MSG_BUFFER:
      cmd_buffer->video.decode_buffer->valid_buf_flag |= RDECODE_CMDBUF_FLAGS_MSG_BUFFER;
      cmd_buffer->video.decode_buffer->msg_buffer_address_hi = (addr >> 32);
      cmd_buffer->video.decode_buffer->msg_buffer_address_lo = (addr);
      break;
   case RDECODE_CMD_DPB_BUFFER:
      cmd_buffer->video.decode_buffer->valid_buf_flag |= (RDECODE_CMDBUF_FLAGS_DPB_BUFFER);
      cmd_buffer->video.decode_buffer->dpb_buffer_address_hi = (addr >> 32);
      cmd_buffer->video.decode_buffer->dpb_buffer_address_lo = (addr);
      break;
   case RDECODE_CMD_DECODING_TARGET_BUFFER:
      cmd_buffer->video.decode_buffer->valid_buf_flag |= (RDECODE_CMDBUF_FLAGS_DECODING_TARGET_BUFFER);
      cmd_buffer->video.decode_buffer->target_buffer_address_hi = (addr >> 32);
      cmd_buffer->video.decode_buffer->target_buffer_address_lo = (addr);
      break;
   case RDECODE_CMD_FEEDBACK_BUFFER:
      cmd_buffer->video.decode_buffer->valid_buf_flag |= (RDECODE_CMDBUF_FLAGS_FEEDBACK_BUFFER);
      cmd_buffer->video.decode_buffer->feedback_buffer_address_hi = (addr >> 32);
      cmd_buffer->video.decode_buffer->feedback_buffer_address_lo = (addr);
      break;
   case RDECODE_CMD_PROB_TBL_BUFFER:
      cmd_buffer->video.decode_buffer->valid_buf_flag |= (RDECODE_CMDBUF_FLAGS_PROB_TBL_BUFFER);
      cmd_buffer->video.decode_buffer->prob_tbl_buffer_address_hi = (addr >> 32);
      cmd_buffer->video.decode_buffer->prob_tbl_buffer_address_lo = (addr);
      break;
   case RDECODE_CMD_SESSION_CONTEXT_BUFFER:
      cmd_buffer->video.decode_buffer->valid_buf_flag |= (RDECODE_CMDBUF_FLAGS_SESSION_CONTEXT_BUFFER);
      cmd_buffer->video.decode_buffer->session_contex_buffer_address_hi = (addr >> 32);
      cmd_buffer->video.decode_buffer->session_contex_buffer_address_lo = (addr);
      break;
   case RDECODE_CMD_BITSTREAM_BUFFER:
      cmd_buffer->video.decode_buffer->valid_buf_flag |= (RDECODE_CMDBUF_FLAGS_BITSTREAM_BUFFER);
      cmd_buffer->video.decode_buffer->bitstream_buffer_address_hi = (addr >> 32);
      cmd_buffer->video.decode_buffer->bitstream_buffer_address_lo = (addr);
      break;
   case RDECODE_CMD_IT_SCALING_TABLE_BUFFER:
      cmd_buffer->video.decode_buffer->valid_buf_flag |= (RDECODE_CMDBUF_FLAGS_IT_SCALING_BUFFER);
      cmd_buffer->video.decode_buffer->it_sclr_table_buffer_address_hi = (addr >> 32);
      cmd_buffer->video.decode_buffer->it_sclr_table_buffer_address_lo = (addr);
      break;
   case RDECODE_CMD_CONTEXT_BUFFER:
      cmd_buffer->video.decode_buffer->valid_buf_flag |= (RDECODE_CMDBUF_FLAGS_CONTEXT_BUFFER);
      cmd_buffer->video.decode_buffer->context_buffer_address_hi = (addr >> 32);
      cmd_buffer->video.decode_buffer->context_buffer_address_lo = (addr);
      break;
   default:
      assert(0);
   }
}

static void
rvcn_dec_message_create(struct radv_video_session *vid, void *ptr, uint32_t size)
{
   rvcn_dec_message_header_t *header = ptr;
   rvcn_dec_message_create_t *create = (void *)((char *)ptr + sizeof(rvcn_dec_message_header_t));

   memset(ptr, 0, size);
   header->header_size = sizeof(rvcn_dec_message_header_t);
   header->total_size = size;
   header->num_buffers = 1;
   header->msg_type = RDECODE_MSG_CREATE;
   header->stream_handle = vid->stream_handle;
   header->status_report_feedback_number = 0;

   header->index[0].message_id = RDECODE_MESSAGE_CREATE;
   header->index[0].offset = sizeof(rvcn_dec_message_header_t);
   header->index[0].size = sizeof(rvcn_dec_message_create_t);
   header->index[0].filled = 0;

   create->stream_type = vid->stream_type;
   create->session_flags = 0;
   create->width_in_samples = vid->vk.max_coded.width;
   create->height_in_samples = vid->vk.max_coded.height;
}

static void
rvcn_dec_message_feedback(void *ptr)
{
   rvcn_dec_feedback_header_t *header = (void *)ptr;

   header->header_size = sizeof(rvcn_dec_feedback_header_t);
   header->total_size = sizeof(rvcn_dec_feedback_header_t);
   header->num_buffers = 0;
}

static const uint8_t h264_levels[] = {10, 11, 12, 13, 20, 21, 22, 30, 31, 32, 40, 41, 42, 50, 51, 52, 60, 61, 62};
static uint8_t
get_h264_level(StdVideoH264LevelIdc level)
{
   assert(level <= STD_VIDEO_H264_LEVEL_IDC_6_2);
   return h264_levels[level];
}

static void
update_h264_scaling(unsigned char scaling_list_4x4[6][16], unsigned char scaling_list_8x8[2][64],
                    const StdVideoH264ScalingLists *scaling_lists)
{
   for (int i = 0; i < STD_VIDEO_H264_SCALING_LIST_4X4_NUM_LISTS; i++) {
      for (int j = 0; j < STD_VIDEO_H264_SCALING_LIST_4X4_NUM_ELEMENTS; j++)
         scaling_list_4x4[i][vl_zscan_normal_16[j]] = scaling_lists->ScalingList4x4[i][j];
   }

   for (int i = 0; i < 2; i++) {
      for (int j = 0; j < STD_VIDEO_H264_SCALING_LIST_8X8_NUM_ELEMENTS; j++)
         scaling_list_8x8[i][vl_zscan_normal[j]] = scaling_lists->ScalingList8x8[i][j];
   }
}

static rvcn_dec_message_avc_t
get_h264_msg(struct radv_video_session *vid, struct radv_video_session_params *params,
             const struct VkVideoDecodeInfoKHR *frame_info, uint32_t *slice_offset, uint32_t *width_in_samples,
             uint32_t *height_in_samples, void *it_ptr)
{
   rvcn_dec_message_avc_t result;
   const struct VkVideoDecodeH264PictureInfoKHR *h264_pic_info =
      vk_find_struct_const(frame_info->pNext, VIDEO_DECODE_H264_PICTURE_INFO_KHR);

   *slice_offset = h264_pic_info->pSliceOffsets[0];

   memset(&result, 0, sizeof(result));

   assert(params->vk.h264_dec.h264_sps_count > 0);
   const StdVideoH264SequenceParameterSet *sps =
      vk_video_find_h264_dec_std_sps(&params->vk, h264_pic_info->pStdPictureInfo->seq_parameter_set_id);
   switch (sps->profile_idc) {
   case STD_VIDEO_H264_PROFILE_IDC_BASELINE:
      result.profile = RDECODE_H264_PROFILE_BASELINE;
      break;
   case STD_VIDEO_H264_PROFILE_IDC_MAIN:
      result.profile = RDECODE_H264_PROFILE_MAIN;
      break;
   case STD_VIDEO_H264_PROFILE_IDC_HIGH:
      result.profile = RDECODE_H264_PROFILE_HIGH;
      break;
   default:
      fprintf(stderr, "UNSUPPORTED CODEC %d\n", sps->profile_idc);
      result.profile = RDECODE_H264_PROFILE_MAIN;
      break;
   }

   *width_in_samples = (sps->pic_width_in_mbs_minus1 + 1) * 16;
   *height_in_samples = (sps->pic_height_in_map_units_minus1 + 1) * 16;
   if (!sps->flags.frame_mbs_only_flag)
      *height_in_samples *= 2;
   result.level = get_h264_level(sps->level_idc);

   result.sps_info_flags = 0;

   result.sps_info_flags |= sps->flags.direct_8x8_inference_flag << 0;
   result.sps_info_flags |= sps->flags.mb_adaptive_frame_field_flag << 1;
   result.sps_info_flags |= sps->flags.frame_mbs_only_flag << 2;
   result.sps_info_flags |= sps->flags.delta_pic_order_always_zero_flag << 3;
   if (vid->dpb_type != DPB_DYNAMIC_TIER_2)
      result.sps_info_flags |= 1 << RDECODE_SPS_INFO_H264_EXTENSION_SUPPORT_FLAG_SHIFT;

   result.bit_depth_luma_minus8 = sps->bit_depth_luma_minus8;
   result.bit_depth_chroma_minus8 = sps->bit_depth_chroma_minus8;
   result.log2_max_frame_num_minus4 = sps->log2_max_frame_num_minus4;
   result.pic_order_cnt_type = sps->pic_order_cnt_type;
   result.log2_max_pic_order_cnt_lsb_minus4 = sps->log2_max_pic_order_cnt_lsb_minus4;

   result.chroma_format = sps->chroma_format_idc;

   const StdVideoH264PictureParameterSet *pps =
      vk_video_find_h264_dec_std_pps(&params->vk, h264_pic_info->pStdPictureInfo->pic_parameter_set_id);
   result.pps_info_flags = 0;
   result.pps_info_flags |= pps->flags.transform_8x8_mode_flag << 0;
   result.pps_info_flags |= pps->flags.redundant_pic_cnt_present_flag << 1;
   result.pps_info_flags |= pps->flags.constrained_intra_pred_flag << 2;
   result.pps_info_flags |= pps->flags.deblocking_filter_control_present_flag << 3;
   result.pps_info_flags |= pps->weighted_bipred_idc << 4;
   result.pps_info_flags |= pps->flags.weighted_pred_flag << 6;
   result.pps_info_flags |= pps->flags.bottom_field_pic_order_in_frame_present_flag << 7;
   result.pps_info_flags |= pps->flags.entropy_coding_mode_flag << 8;

   result.pic_init_qp_minus26 = pps->pic_init_qp_minus26;
   result.chroma_qp_index_offset = pps->chroma_qp_index_offset;
   result.second_chroma_qp_index_offset = pps->second_chroma_qp_index_offset;

   StdVideoH264ScalingLists scaling_lists;
   vk_video_derive_h264_scaling_list(sps, pps, &scaling_lists);
   update_h264_scaling(result.scaling_list_4x4, result.scaling_list_8x8, &scaling_lists);

   memset(it_ptr, 0, IT_SCALING_TABLE_SIZE);
   memcpy(it_ptr, result.scaling_list_4x4, 6 * 16);
   memcpy((char *)it_ptr + 96, result.scaling_list_8x8, 2 * 64);

   result.num_ref_idx_l0_active_minus1 = pps->num_ref_idx_l0_default_active_minus1;
   result.num_ref_idx_l1_active_minus1 = pps->num_ref_idx_l1_default_active_minus1;

   result.curr_field_order_cnt_list[0] = h264_pic_info->pStdPictureInfo->PicOrderCnt[0];
   result.curr_field_order_cnt_list[1] = h264_pic_info->pStdPictureInfo->PicOrderCnt[1];

   result.frame_num = h264_pic_info->pStdPictureInfo->frame_num;

   result.num_ref_frames = sps->max_num_ref_frames;
   result.non_existing_frame_flags = 0;
   result.used_for_reference_flags = 0;

   memset(result.ref_frame_list, 0xff, sizeof(unsigned char) * 16);
   memset(result.frame_num_list, 0, sizeof(unsigned int) * 16);
   for (unsigned i = 0; i < frame_info->referenceSlotCount; i++) {
      int idx = frame_info->pReferenceSlots[i].slotIndex;
      const struct VkVideoDecodeH264DpbSlotInfoKHR *dpb_slot =
         vk_find_struct_const(frame_info->pReferenceSlots[i].pNext, VIDEO_DECODE_H264_DPB_SLOT_INFO_KHR);

      result.frame_num_list[i] = dpb_slot->pStdReferenceInfo->FrameNum;
      result.field_order_cnt_list[i][0] = dpb_slot->pStdReferenceInfo->PicOrderCnt[0];
      result.field_order_cnt_list[i][1] = dpb_slot->pStdReferenceInfo->PicOrderCnt[1];

      result.ref_frame_list[i] = idx;

      if (dpb_slot->pStdReferenceInfo->flags.top_field_flag)
         result.used_for_reference_flags |= (1 << (2 * i));
      if (dpb_slot->pStdReferenceInfo->flags.bottom_field_flag)
         result.used_for_reference_flags |= (1 << (2 * i + 1));

      if (!dpb_slot->pStdReferenceInfo->flags.top_field_flag && !dpb_slot->pStdReferenceInfo->flags.bottom_field_flag)
         result.used_for_reference_flags |= (3 << (2 * i));

      if (dpb_slot->pStdReferenceInfo->flags.used_for_long_term_reference)
         result.ref_frame_list[i] |= 0x80;
      if (dpb_slot->pStdReferenceInfo->flags.is_non_existing)
         result.non_existing_frame_flags |= 1 << i;
   }
   result.curr_pic_ref_frame_num = frame_info->referenceSlotCount;
   result.decoded_pic_idx = frame_info->pSetupReferenceSlot->slotIndex;

   return result;
}

static void
update_h265_scaling(void *it_ptr, const StdVideoH265ScalingLists *scaling_lists)
{
   if (scaling_lists) {
      memcpy(it_ptr, scaling_lists->ScalingList4x4,
             STD_VIDEO_H265_SCALING_LIST_4X4_NUM_LISTS * STD_VIDEO_H265_SCALING_LIST_4X4_NUM_ELEMENTS);
      memcpy((char *)it_ptr + 96, scaling_lists->ScalingList8x8,
             STD_VIDEO_H265_SCALING_LIST_8X8_NUM_LISTS * STD_VIDEO_H265_SCALING_LIST_8X8_NUM_ELEMENTS);
      memcpy((char *)it_ptr + 480, scaling_lists->ScalingList16x16,
             STD_VIDEO_H265_SCALING_LIST_16X16_NUM_LISTS * STD_VIDEO_H265_SCALING_LIST_16X16_NUM_ELEMENTS);
      memcpy((char *)it_ptr + 864, scaling_lists->ScalingList32x32,
             STD_VIDEO_H265_SCALING_LIST_32X32_NUM_LISTS * STD_VIDEO_H265_SCALING_LIST_32X32_NUM_ELEMENTS);
   } else {
      memset(it_ptr, 0, STD_VIDEO_H265_SCALING_LIST_4X4_NUM_LISTS * STD_VIDEO_H265_SCALING_LIST_4X4_NUM_ELEMENTS);
      memset((char *)it_ptr + 96, 0,
             STD_VIDEO_H265_SCALING_LIST_8X8_NUM_LISTS * STD_VIDEO_H265_SCALING_LIST_8X8_NUM_ELEMENTS);
      memset((char *)it_ptr + 480, 0,
             STD_VIDEO_H265_SCALING_LIST_16X16_NUM_LISTS * STD_VIDEO_H265_SCALING_LIST_16X16_NUM_ELEMENTS);
      memset((char *)it_ptr + 864, 0,
             STD_VIDEO_H265_SCALING_LIST_32X32_NUM_LISTS * STD_VIDEO_H265_SCALING_LIST_32X32_NUM_ELEMENTS);
   }
}

static rvcn_dec_message_hevc_t
get_h265_msg(struct radv_device *device, struct radv_video_session *vid, struct radv_video_session_params *params,
             const struct VkVideoDecodeInfoKHR *frame_info,
             uint32_t *width_in_samples,
             uint32_t *height_in_samples,
             void *it_ptr)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);
   rvcn_dec_message_hevc_t result;
   int i, j;
   const struct VkVideoDecodeH265PictureInfoKHR *h265_pic_info =
      vk_find_struct_const(frame_info->pNext, VIDEO_DECODE_H265_PICTURE_INFO_KHR);
   memset(&result, 0, sizeof(result));

   const StdVideoH265SequenceParameterSet *sps =
      vk_video_find_h265_dec_std_sps(&params->vk, h265_pic_info->pStdPictureInfo->pps_seq_parameter_set_id);
   const StdVideoH265PictureParameterSet *pps =
      vk_video_find_h265_dec_std_pps(&params->vk, h265_pic_info->pStdPictureInfo->pps_pic_parameter_set_id);

   result.sps_info_flags = 0;
   result.sps_info_flags |= sps->flags.scaling_list_enabled_flag << 0;
   result.sps_info_flags |= sps->flags.amp_enabled_flag << 1;
   result.sps_info_flags |= sps->flags.sample_adaptive_offset_enabled_flag << 2;
   result.sps_info_flags |= sps->flags.pcm_enabled_flag << 3;
   result.sps_info_flags |= sps->flags.pcm_loop_filter_disabled_flag << 4;
   result.sps_info_flags |= sps->flags.long_term_ref_pics_present_flag << 5;
   result.sps_info_flags |= sps->flags.sps_temporal_mvp_enabled_flag << 6;
   result.sps_info_flags |= sps->flags.strong_intra_smoothing_enabled_flag << 7;
   result.sps_info_flags |= sps->flags.separate_colour_plane_flag << 8;

   if (pdev->info.family == CHIP_CARRIZO)
      result.sps_info_flags |= 1 << 9;

   if (!h265_pic_info->pStdPictureInfo->flags.short_term_ref_pic_set_sps_flag) {
      result.sps_info_flags |= 1 << 11;
   }
   result.st_rps_bits = h265_pic_info->pStdPictureInfo->NumBitsForSTRefPicSetInSlice;

   *width_in_samples = sps->pic_width_in_luma_samples;
   *height_in_samples = sps->pic_height_in_luma_samples;
   result.chroma_format = sps->chroma_format_idc;
   result.bit_depth_luma_minus8 = sps->bit_depth_luma_minus8;
   result.bit_depth_chroma_minus8 = sps->bit_depth_chroma_minus8;
   result.log2_max_pic_order_cnt_lsb_minus4 = sps->log2_max_pic_order_cnt_lsb_minus4;
   result.sps_max_dec_pic_buffering_minus1 =
      sps->pDecPicBufMgr->max_dec_pic_buffering_minus1[sps->sps_max_sub_layers_minus1];
   result.log2_min_luma_coding_block_size_minus3 = sps->log2_min_luma_coding_block_size_minus3;
   result.log2_diff_max_min_luma_coding_block_size = sps->log2_diff_max_min_luma_coding_block_size;
   result.log2_min_transform_block_size_minus2 = sps->log2_min_luma_transform_block_size_minus2;
   result.log2_diff_max_min_transform_block_size = sps->log2_diff_max_min_luma_transform_block_size;
   result.max_transform_hierarchy_depth_inter = sps->max_transform_hierarchy_depth_inter;
   result.max_transform_hierarchy_depth_intra = sps->max_transform_hierarchy_depth_intra;
   if (sps->flags.pcm_enabled_flag) {
      result.pcm_sample_bit_depth_luma_minus1 = sps->pcm_sample_bit_depth_luma_minus1;
      result.pcm_sample_bit_depth_chroma_minus1 = sps->pcm_sample_bit_depth_chroma_minus1;
      result.log2_min_pcm_luma_coding_block_size_minus3 = sps->log2_min_pcm_luma_coding_block_size_minus3;
      result.log2_diff_max_min_pcm_luma_coding_block_size = sps->log2_diff_max_min_pcm_luma_coding_block_size;
   }
   result.num_short_term_ref_pic_sets = sps->num_short_term_ref_pic_sets;

   result.pps_info_flags = 0;
   result.pps_info_flags |= pps->flags.dependent_slice_segments_enabled_flag << 0;
   result.pps_info_flags |= pps->flags.output_flag_present_flag << 1;
   result.pps_info_flags |= pps->flags.sign_data_hiding_enabled_flag << 2;
   result.pps_info_flags |= pps->flags.cabac_init_present_flag << 3;
   result.pps_info_flags |= pps->flags.constrained_intra_pred_flag << 4;
   result.pps_info_flags |= pps->flags.transform_skip_enabled_flag << 5;
   result.pps_info_flags |= pps->flags.cu_qp_delta_enabled_flag << 6;
   result.pps_info_flags |= pps->flags.pps_slice_chroma_qp_offsets_present_flag << 7;
   result.pps_info_flags |= pps->flags.weighted_pred_flag << 8;
   result.pps_info_flags |= pps->flags.weighted_bipred_flag << 9;
   result.pps_info_flags |= pps->flags.transquant_bypass_enabled_flag << 10;
   result.pps_info_flags |= pps->flags.tiles_enabled_flag << 11;
   result.pps_info_flags |= pps->flags.entropy_coding_sync_enabled_flag << 12;
   result.pps_info_flags |= pps->flags.uniform_spacing_flag << 13;
   result.pps_info_flags |= pps->flags.loop_filter_across_tiles_enabled_flag << 14;
   result.pps_info_flags |= pps->flags.pps_loop_filter_across_slices_enabled_flag << 15;
   result.pps_info_flags |= pps->flags.deblocking_filter_override_enabled_flag << 16;
   result.pps_info_flags |= pps->flags.pps_deblocking_filter_disabled_flag << 17;
   result.pps_info_flags |= pps->flags.lists_modification_present_flag << 18;
   result.pps_info_flags |= pps->flags.slice_segment_header_extension_present_flag << 19;

   result.num_extra_slice_header_bits = pps->num_extra_slice_header_bits;
   result.num_long_term_ref_pic_sps = sps->num_long_term_ref_pics_sps;
   result.num_ref_idx_l0_default_active_minus1 = pps->num_ref_idx_l0_default_active_minus1;
   result.num_ref_idx_l1_default_active_minus1 = pps->num_ref_idx_l1_default_active_minus1;
   result.pps_cb_qp_offset = pps->pps_cb_qp_offset;
   result.pps_cr_qp_offset = pps->pps_cr_qp_offset;
   result.pps_beta_offset_div2 = pps->pps_beta_offset_div2;
   result.pps_tc_offset_div2 = pps->pps_tc_offset_div2;
   result.diff_cu_qp_delta_depth = pps->diff_cu_qp_delta_depth;
   result.num_tile_columns_minus1 = pps->num_tile_columns_minus1;
   result.num_tile_rows_minus1 = pps->num_tile_rows_minus1;
   result.log2_parallel_merge_level_minus2 = pps->log2_parallel_merge_level_minus2;
   result.init_qp_minus26 = pps->init_qp_minus26;

   for (i = 0; i < 19; ++i)
      result.column_width_minus1[i] = pps->column_width_minus1[i];

   for (i = 0; i < 21; ++i)
      result.row_height_minus1[i] = pps->row_height_minus1[i];

   result.num_delta_pocs_ref_rps_idx = h265_pic_info->pStdPictureInfo->NumDeltaPocsOfRefRpsIdx;
   result.curr_poc = h265_pic_info->pStdPictureInfo->PicOrderCntVal;

   uint8_t idxs[16];
   memset(result.poc_list, 0, 16 * sizeof(int));
   memset(result.ref_pic_list, 0x7f, 16);
   memset(idxs, 0xff, 16);
   for (i = 0; i < frame_info->referenceSlotCount; i++) {
      const struct VkVideoDecodeH265DpbSlotInfoKHR *dpb_slot =
         vk_find_struct_const(frame_info->pReferenceSlots[i].pNext, VIDEO_DECODE_H265_DPB_SLOT_INFO_KHR);
      int idx = frame_info->pReferenceSlots[i].slotIndex;
      result.poc_list[i] = dpb_slot->pStdReferenceInfo->PicOrderCntVal;
      result.ref_pic_list[i] = idx;
      idxs[idx] = i;
   }
   result.curr_idx = frame_info->pSetupReferenceSlot->slotIndex;

#define IDXS(x) ((x) == 0xff ? 0xff : idxs[(x)])
   for (i = 0; i < 8; ++i)
      result.ref_pic_set_st_curr_before[i] = IDXS(h265_pic_info->pStdPictureInfo->RefPicSetStCurrBefore[i]);

   for (i = 0; i < 8; ++i)
      result.ref_pic_set_st_curr_after[i] = IDXS(h265_pic_info->pStdPictureInfo->RefPicSetStCurrAfter[i]);

   for (i = 0; i < 8; ++i)
      result.ref_pic_set_lt_curr[i] = IDXS(h265_pic_info->pStdPictureInfo->RefPicSetLtCurr[i]);

   const StdVideoH265ScalingLists *scaling_lists = NULL;
   if (pps->flags.pps_scaling_list_data_present_flag)
      scaling_lists = pps->pScalingLists;
   else if (sps->flags.sps_scaling_list_data_present_flag)
      scaling_lists = sps->pScalingLists;

   update_h265_scaling(it_ptr, scaling_lists);

   if (scaling_lists) {
      for (i = 0; i < STD_VIDEO_H265_SCALING_LIST_16X16_NUM_LISTS; ++i)
         result.ucScalingListDCCoefSizeID2[i] = scaling_lists->ScalingListDCCoef16x16[i];

      for (i = 0; i < STD_VIDEO_H265_SCALING_LIST_32X32_NUM_LISTS; ++i)
         result.ucScalingListDCCoefSizeID3[i] = scaling_lists->ScalingListDCCoef32x32[i];
   }

   for (i = 0; i < 2; i++) {
      for (j = 0; j < 15; j++)
         result.direct_reflist[i][j] = 0xff;
   }

   if (vid->vk.h265.profile_idc == STD_VIDEO_H265_PROFILE_IDC_MAIN_10) {
      if (vid->vk.picture_format == VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16) {
         result.p010_mode = 1;
         result.msb_mode = 1;
      } else {
         result.p010_mode = 0;
         result.luma_10to8 = 5;
         result.chroma_10to8 = 5;
         result.hevc_reserved[0] = 4; /* sclr_luma10to8 */
         result.hevc_reserved[1] = 4; /* sclr_chroma10to8 */
      }
   }

   return result;
}

enum {
   AV1_RESTORE_NONE = 0,
   AV1_RESTORE_WIENER = 1,
   AV1_RESTORE_SGRPROJ = 2,
   AV1_RESTORE_SWITCHABLE = 3,
};

#define AV1_SUPERRES_NUM       8
#define AV1_SUPERRES_DENOM_MIN 9

#define LUMA_BLOCK_SIZE_Y   73
#define LUMA_BLOCK_SIZE_X   82
#define CHROMA_BLOCK_SIZE_Y 38
#define CHROMA_BLOCK_SIZE_X 44

static int32_t
radv_vcn_av1_film_grain_random_number(unsigned short *seed, int32_t bits)
{
   unsigned short bit;
   unsigned short value = *seed;

   bit = ((value >> 0) ^ (value >> 1) ^ (value >> 3) ^ (value >> 12)) & 1;
   value = (value >> 1) | (bit << 15);
   *seed = value;

   return (value >> (16 - bits)) & ((1 << bits) - 1);
}

static void
radv_vcn_av1_film_grain_init_scaling(uint8_t scaling_points[][2], uint8_t num, short scaling_lut[])
{
   int32_t i, x, delta_x, delta_y;
   int64_t delta;

   if (num == 0)
      return;

   for (i = 0; i < scaling_points[0][0]; i++)
      scaling_lut[i] = scaling_points[0][1];

   for (i = 0; i < num - 1; i++) {
      delta_y = scaling_points[i + 1][1] - scaling_points[i][1];
      delta_x = scaling_points[i + 1][0] - scaling_points[i][0];

      delta = delta_y * ((65536 + (delta_x >> 1)) / delta_x);

      for (x = 0; x < delta_x; x++)
         scaling_lut[scaling_points[i][0] + x] = (short)(scaling_points[i][1] + (int32_t)((x * delta + 32768) >> 16));
   }

   for (i = scaling_points[num - 1][0]; i < 256; i++)
      scaling_lut[i] = scaling_points[num - 1][1];
}

static void
radv_vcn_av1_init_film_grain_buffer(rvcn_dec_film_grain_params_t *fg_params, rvcn_dec_av1_fg_init_buf_t *fg_buf)
{
   const int32_t luma_block_size_y = LUMA_BLOCK_SIZE_Y;
   const int32_t luma_block_size_x = LUMA_BLOCK_SIZE_X;
   const int32_t chroma_block_size_y = CHROMA_BLOCK_SIZE_Y;
   const int32_t chroma_block_size_x = CHROMA_BLOCK_SIZE_X;
   const int32_t gauss_bits = 11;
   int32_t filt_luma_grain_block[LUMA_BLOCK_SIZE_Y][LUMA_BLOCK_SIZE_X];
   int32_t filt_cb_grain_block[CHROMA_BLOCK_SIZE_Y][CHROMA_BLOCK_SIZE_X];
   int32_t filt_cr_grain_block[CHROMA_BLOCK_SIZE_Y][CHROMA_BLOCK_SIZE_X];
   int32_t chroma_subsamp_y = 1;
   int32_t chroma_subsamp_x = 1;
   unsigned short seed = fg_params->random_seed;
   int32_t ar_coeff_lag = fg_params->ar_coeff_lag;
   int32_t bit_depth = fg_params->bit_depth_minus_8 + 8;
   short grain_center = 128 << (bit_depth - 8);
   short grain_min = 0 - grain_center;
   short grain_max = (256 << (bit_depth - 8)) - 1 - grain_center;
   int32_t shift = 12 - bit_depth + fg_params->grain_scale_shift;
   short luma_grain_block_tmp[64][80];
   short cb_grain_block_tmp[32][40];
   short cr_grain_block_tmp[32][40];
   short *align_ptr, *align_ptr0, *align_ptr1;
   int32_t x, y, g, i, j, c, c0, c1, delta_row, delta_col;
   int32_t s, s0, s1, pos, r;

   /* generate luma grain block */
   memset(filt_luma_grain_block, 0, sizeof(filt_luma_grain_block));
   for (y = 0; y < luma_block_size_y; y++) {
      for (x = 0; x < luma_block_size_x; x++) {
         g = 0;
         if (fg_params->num_y_points > 0) {
            r = radv_vcn_av1_film_grain_random_number(&seed, gauss_bits);
            g = gaussian_sequence[CLAMP(r, 0, 2048 - 1)];
         }
         filt_luma_grain_block[y][x] = ROUND_POWER_OF_TWO(g, shift);
      }
   }

   for (y = 3; y < luma_block_size_y; y++) {
      for (x = 3; x < luma_block_size_x - 3; x++) {
         s = 0;
         pos = 0;
         for (delta_row = -ar_coeff_lag; delta_row <= 0; delta_row++) {
            for (delta_col = -ar_coeff_lag; delta_col <= ar_coeff_lag; delta_col++) {
               if (delta_row == 0 && delta_col == 0)
                  break;
               c = fg_params->ar_coeffs_y[pos];
               s += filt_luma_grain_block[y + delta_row][x + delta_col] * c;
               pos++;
            }
         }
         filt_luma_grain_block[y][x] = AV1_CLAMP(
            filt_luma_grain_block[y][x] + ROUND_POWER_OF_TWO(s, fg_params->ar_coeff_shift), grain_min, grain_max);
      }
   }

   /* generate chroma grain block */
   memset(filt_cb_grain_block, 0, sizeof(filt_cb_grain_block));
   shift = 12 - bit_depth + fg_params->grain_scale_shift;
   seed = fg_params->random_seed ^ 0xb524;
   for (y = 0; y < chroma_block_size_y; y++) {
      for (x = 0; x < chroma_block_size_x; x++) {
         g = 0;
         if (fg_params->num_cb_points || fg_params->chroma_scaling_from_luma) {
            r = radv_vcn_av1_film_grain_random_number(&seed, gauss_bits);
            g = gaussian_sequence[CLAMP(r, 0, 2048 - 1)];
         }
         filt_cb_grain_block[y][x] = ROUND_POWER_OF_TWO(g, shift);
      }
   }

   memset(filt_cr_grain_block, 0, sizeof(filt_cr_grain_block));
   seed = fg_params->random_seed ^ 0x49d8;
   for (y = 0; y < chroma_block_size_y; y++) {
      for (x = 0; x < chroma_block_size_x; x++) {
         g = 0;
         if (fg_params->num_cr_points || fg_params->chroma_scaling_from_luma) {
            r = radv_vcn_av1_film_grain_random_number(&seed, gauss_bits);
            g = gaussian_sequence[CLAMP(r, 0, 2048 - 1)];
         }
         filt_cr_grain_block[y][x] = ROUND_POWER_OF_TWO(g, shift);
      }
   }

   for (y = 3; y < chroma_block_size_y; y++) {
      for (x = 3; x < chroma_block_size_x - 3; x++) {
         s0 = 0, s1 = 0, pos = 0;
         for (delta_row = -ar_coeff_lag; delta_row <= 0; delta_row++) {
            for (delta_col = -ar_coeff_lag; delta_col <= ar_coeff_lag; delta_col++) {
               c0 = fg_params->ar_coeffs_cb[pos];
               c1 = fg_params->ar_coeffs_cr[pos];
               if (delta_row == 0 && delta_col == 0) {
                  if (fg_params->num_y_points > 0) {
                     int luma = 0;
                     int luma_x = ((x - 3) << chroma_subsamp_x) + 3;
                     int luma_y = ((y - 3) << chroma_subsamp_y) + 3;
                     for (i = 0; i <= chroma_subsamp_y; i++)
                        for (j = 0; j <= chroma_subsamp_x; j++)
                           luma += filt_luma_grain_block[luma_y + i][luma_x + j];

                     luma = ROUND_POWER_OF_TWO(luma, chroma_subsamp_x + chroma_subsamp_y);
                     s0 += luma * c0;
                     s1 += luma * c1;
                  }
                  break;
               }
               s0 += filt_cb_grain_block[y + delta_row][x + delta_col] * c0;
               s1 += filt_cr_grain_block[y + delta_row][x + delta_col] * c1;
               pos++;
            }
         }
         filt_cb_grain_block[y][x] = AV1_CLAMP(
            filt_cb_grain_block[y][x] + ROUND_POWER_OF_TWO(s0, fg_params->ar_coeff_shift), grain_min, grain_max);
         filt_cr_grain_block[y][x] = AV1_CLAMP(
            filt_cr_grain_block[y][x] + ROUND_POWER_OF_TWO(s1, fg_params->ar_coeff_shift), grain_min, grain_max);
      }
   }

   for (i = 9; i < luma_block_size_y; i++)
      for (j = 9; j < luma_block_size_x; j++)
         luma_grain_block_tmp[i - 9][j - 9] = filt_luma_grain_block[i][j];

   for (i = 6; i < chroma_block_size_y; i++)
      for (j = 6; j < chroma_block_size_x; j++) {
         cb_grain_block_tmp[i - 6][j - 6] = filt_cb_grain_block[i][j];
         cr_grain_block_tmp[i - 6][j - 6] = filt_cr_grain_block[i][j];
      }

   align_ptr = &fg_buf->luma_grain_block[0][0];
   for (i = 0; i < 64; i++) {
      for (j = 0; j < 80; j++)
         *align_ptr++ = luma_grain_block_tmp[i][j];

      if (((i + 1) % 4) == 0)
         align_ptr += 64;
   }

   align_ptr0 = &fg_buf->cb_grain_block[0][0];
   align_ptr1 = &fg_buf->cr_grain_block[0][0];
   for (i = 0; i < 32; i++) {
      for (j = 0; j < 40; j++) {
         *align_ptr0++ = cb_grain_block_tmp[i][j];
         *align_ptr1++ = cr_grain_block_tmp[i][j];
      }
      if (((i + 1) % 8) == 0) {
         align_ptr0 += 64;
         align_ptr1 += 64;
      }
   }

   memset(fg_buf->scaling_lut_y, 0, sizeof(fg_buf->scaling_lut_y));
   radv_vcn_av1_film_grain_init_scaling(fg_params->scaling_points_y, fg_params->num_y_points, fg_buf->scaling_lut_y);
   if (fg_params->chroma_scaling_from_luma) {
      memcpy(fg_buf->scaling_lut_cb, fg_buf->scaling_lut_y, sizeof(fg_buf->scaling_lut_y));
      memcpy(fg_buf->scaling_lut_cr, fg_buf->scaling_lut_y, sizeof(fg_buf->scaling_lut_y));
   } else {
      memset(fg_buf->scaling_lut_cb, 0, sizeof(fg_buf->scaling_lut_cb));
      memset(fg_buf->scaling_lut_cr, 0, sizeof(fg_buf->scaling_lut_cr));
      radv_vcn_av1_film_grain_init_scaling(fg_params->scaling_points_cb, fg_params->num_cb_points,
                                           fg_buf->scaling_lut_cb);
      radv_vcn_av1_film_grain_init_scaling(fg_params->scaling_points_cr, fg_params->num_cr_points,
                                           fg_buf->scaling_lut_cr);
   }
}

static rvcn_dec_message_av1_t
get_av1_msg(struct radv_device *device, struct radv_video_session *vid, struct radv_video_session_params *params,
            const struct VkVideoDecodeInfoKHR *frame_info, void *probs_ptr, int *update_reference_slot)
{
   rvcn_dec_message_av1_t result;
   unsigned i, j;
   const struct VkVideoDecodeAV1PictureInfoKHR *av1_pic_info =
      vk_find_struct_const(frame_info->pNext, VIDEO_DECODE_AV1_PICTURE_INFO_KHR);
   const StdVideoDecodeAV1PictureInfo *pi = av1_pic_info->pStdPictureInfo;
   const StdVideoAV1SequenceHeader *seq_hdr = &params->vk.av1_dec.seq_hdr.base;
   memset(&result, 0, sizeof(result));

   const int intra_only_decoding = vid->vk.max_dpb_slots == 0;
   if (intra_only_decoding)
      assert(frame_info->pSetupReferenceSlot == NULL);

   *update_reference_slot = !(intra_only_decoding || pi->refresh_frame_flags == 0);

   result.frame_header_flags = (1 /*av1_pic_info->frame_header->flags.show_frame*/
                                << RDECODE_FRAME_HDR_INFO_AV1_SHOW_FRAME_SHIFT) &
                               RDECODE_FRAME_HDR_INFO_AV1_SHOW_FRAME_MASK;

   result.frame_header_flags |= (pi->flags.disable_cdf_update << RDECODE_FRAME_HDR_INFO_AV1_DISABLE_CDF_UPDATE_SHIFT) &
                                RDECODE_FRAME_HDR_INFO_AV1_DISABLE_CDF_UPDATE_MASK;

   result.frame_header_flags |=
      ((!pi->flags.disable_frame_end_update_cdf) << RDECODE_FRAME_HDR_INFO_AV1_REFRESH_FRAME_CONTEXT_SHIFT) &
      RDECODE_FRAME_HDR_INFO_AV1_REFRESH_FRAME_CONTEXT_MASK;

   result.frame_header_flags |=
      ((pi->frame_type == STD_VIDEO_AV1_FRAME_TYPE_INTRA_ONLY) << RDECODE_FRAME_HDR_INFO_AV1_INTRA_ONLY_SHIFT) &
      RDECODE_FRAME_HDR_INFO_AV1_INTRA_ONLY_MASK;

   result.frame_header_flags |= (pi->flags.allow_intrabc << RDECODE_FRAME_HDR_INFO_AV1_ALLOW_INTRABC_SHIFT) &
                                RDECODE_FRAME_HDR_INFO_AV1_ALLOW_INTRABC_MASK;

   result.frame_header_flags |=
      (pi->flags.allow_high_precision_mv << RDECODE_FRAME_HDR_INFO_AV1_ALLOW_HIGH_PRECISION_MV_SHIFT) &
      RDECODE_FRAME_HDR_INFO_AV1_ALLOW_HIGH_PRECISION_MV_MASK;

   result.frame_header_flags |=
      (seq_hdr->pColorConfig->flags.mono_chrome << RDECODE_FRAME_HDR_INFO_AV1_MONOCHROME_SHIFT) &
      RDECODE_FRAME_HDR_INFO_AV1_MONOCHROME_MASK;

   result.frame_header_flags |= (pi->flags.skip_mode_present << RDECODE_FRAME_HDR_INFO_AV1_SKIP_MODE_FLAG_SHIFT) &
                                RDECODE_FRAME_HDR_INFO_AV1_SKIP_MODE_FLAG_MASK;

   result.frame_header_flags |=
      (pi->pQuantization->flags.using_qmatrix << RDECODE_FRAME_HDR_INFO_AV1_USING_QMATRIX_SHIFT) &
      RDECODE_FRAME_HDR_INFO_AV1_USING_QMATRIX_MASK;

   result.frame_header_flags |=
      (seq_hdr->flags.enable_filter_intra << RDECODE_FRAME_HDR_INFO_AV1_ENABLE_FILTER_INTRA_SHIFT) &
      RDECODE_FRAME_HDR_INFO_AV1_ENABLE_FILTER_INTRA_MASK;

   result.frame_header_flags |=
      (seq_hdr->flags.enable_intra_edge_filter << RDECODE_FRAME_HDR_INFO_AV1_ENABLE_INTRA_EDGE_FILTER_SHIFT) &
      RDECODE_FRAME_HDR_INFO_AV1_ENABLE_INTRA_EDGE_FILTER_MASK;

   result.frame_header_flags |=
      (seq_hdr->flags.enable_interintra_compound << RDECODE_FRAME_HDR_INFO_AV1_ENABLE_INTERINTRA_COMPOUND_SHIFT) &
      RDECODE_FRAME_HDR_INFO_AV1_ENABLE_INTERINTRA_COMPOUND_MASK;

   result.frame_header_flags |=
      (seq_hdr->flags.enable_masked_compound << RDECODE_FRAME_HDR_INFO_AV1_ENABLE_MASKED_COMPOUND_SHIFT) &
      RDECODE_FRAME_HDR_INFO_AV1_ENABLE_MASKED_COMPOUND_MASK;

   result.frame_header_flags |=
      (pi->flags.allow_warped_motion << RDECODE_FRAME_HDR_INFO_AV1_ALLOW_WARPED_MOTION_SHIFT) &
      RDECODE_FRAME_HDR_INFO_AV1_ALLOW_WARPED_MOTION_MASK;

   result.frame_header_flags |=
      (seq_hdr->flags.enable_dual_filter << RDECODE_FRAME_HDR_INFO_AV1_ENABLE_DUAL_FILTER_SHIFT) &
      RDECODE_FRAME_HDR_INFO_AV1_ENABLE_DUAL_FILTER_MASK;

   result.frame_header_flags |=
      (seq_hdr->flags.enable_order_hint << RDECODE_FRAME_HDR_INFO_AV1_ENABLE_ORDER_HINT_SHIFT) &
      RDECODE_FRAME_HDR_INFO_AV1_ENABLE_ORDER_HINT_MASK;

   result.frame_header_flags |= (seq_hdr->flags.enable_jnt_comp << RDECODE_FRAME_HDR_INFO_AV1_ENABLE_JNT_COMP_SHIFT) &
                                RDECODE_FRAME_HDR_INFO_AV1_ENABLE_JNT_COMP_MASK;

   result.frame_header_flags |= (pi->flags.use_ref_frame_mvs << RDECODE_FRAME_HDR_INFO_AV1_ALLOW_REF_FRAME_MVS_SHIFT) &
                                RDECODE_FRAME_HDR_INFO_AV1_ALLOW_REF_FRAME_MVS_MASK;

   result.frame_header_flags |=
      (pi->flags.allow_screen_content_tools << RDECODE_FRAME_HDR_INFO_AV1_ALLOW_SCREEN_CONTENT_TOOLS_SHIFT) &
      RDECODE_FRAME_HDR_INFO_AV1_ALLOW_SCREEN_CONTENT_TOOLS_MASK;

   result.frame_header_flags |=
      (pi->flags.force_integer_mv << RDECODE_FRAME_HDR_INFO_AV1_CUR_FRAME_FORCE_INTEGER_MV_SHIFT) &
      RDECODE_FRAME_HDR_INFO_AV1_CUR_FRAME_FORCE_INTEGER_MV_MASK;

   result.frame_header_flags |=
      (pi->pLoopFilter->flags.loop_filter_delta_enabled << RDECODE_FRAME_HDR_INFO_AV1_MODE_REF_DELTA_ENABLED_SHIFT) &
      RDECODE_FRAME_HDR_INFO_AV1_MODE_REF_DELTA_ENABLED_MASK;

   result.frame_header_flags |=
      (pi->pLoopFilter->flags.loop_filter_delta_update << RDECODE_FRAME_HDR_INFO_AV1_MODE_REF_DELTA_UPDATE_SHIFT) &
      RDECODE_FRAME_HDR_INFO_AV1_MODE_REF_DELTA_UPDATE_MASK;

   result.frame_header_flags |= (pi->flags.delta_q_present << RDECODE_FRAME_HDR_INFO_AV1_DELTA_Q_PRESENT_FLAG_SHIFT) &
                                RDECODE_FRAME_HDR_INFO_AV1_DELTA_Q_PRESENT_FLAG_MASK;

   result.frame_header_flags |= (pi->flags.delta_lf_present << RDECODE_FRAME_HDR_INFO_AV1_DELTA_LF_PRESENT_FLAG_SHIFT) &
                                RDECODE_FRAME_HDR_INFO_AV1_DELTA_LF_PRESENT_FLAG_MASK;

   result.frame_header_flags |= (pi->flags.reduced_tx_set << RDECODE_FRAME_HDR_INFO_AV1_REDUCED_TX_SET_USED_SHIFT) &
                                RDECODE_FRAME_HDR_INFO_AV1_REDUCED_TX_SET_USED_MASK;

   result.frame_header_flags |=
      (pi->flags.segmentation_enabled << RDECODE_FRAME_HDR_INFO_AV1_SEGMENTATION_ENABLED_SHIFT) &
      RDECODE_FRAME_HDR_INFO_AV1_SEGMENTATION_ENABLED_MASK;

   result.frame_header_flags |=
      (pi->flags.segmentation_update_map << RDECODE_FRAME_HDR_INFO_AV1_SEGMENTATION_UPDATE_MAP_SHIFT) &
      RDECODE_FRAME_HDR_INFO_AV1_SEGMENTATION_UPDATE_MAP_MASK;

   result.frame_header_flags |=
      (pi->flags.segmentation_temporal_update << RDECODE_FRAME_HDR_INFO_AV1_SEGMENTATION_TEMPORAL_UPDATE_SHIFT) &
      RDECODE_FRAME_HDR_INFO_AV1_SEGMENTATION_TEMPORAL_UPDATE_MASK;

   result.frame_header_flags |= (pi->flags.delta_lf_multi << RDECODE_FRAME_HDR_INFO_AV1_DELTA_LF_MULTI_SHIFT) &
                                RDECODE_FRAME_HDR_INFO_AV1_DELTA_LF_MULTI_MASK;

   result.frame_header_flags |=
      (pi->flags.is_motion_mode_switchable << RDECODE_FRAME_HDR_INFO_AV1_SWITCHABLE_SKIP_MODE_SHIFT) &
      RDECODE_FRAME_HDR_INFO_AV1_SWITCHABLE_SKIP_MODE_MASK;

   result.frame_header_flags |= ((!intra_only_decoding ? !(pi->refresh_frame_flags) : 1)
                                 << RDECODE_FRAME_HDR_INFO_AV1_SKIP_REFERENCE_UPDATE_SHIFT) &
                                RDECODE_FRAME_HDR_INFO_AV1_SKIP_REFERENCE_UPDATE_MASK;

   result.frame_header_flags |=
      ((!seq_hdr->flags.enable_ref_frame_mvs) << RDECODE_FRAME_HDR_INFO_AV1_DISABLE_REF_FRAME_MVS_SHIFT) &
      RDECODE_FRAME_HDR_INFO_AV1_DISABLE_REF_FRAME_MVS_MASK;

   result.current_frame_id = pi->current_frame_id;
   result.frame_offset = pi->OrderHint;
   result.profile = seq_hdr->seq_profile;
   result.is_annexb = 0;

   result.frame_type = pi->frame_type;
   result.primary_ref_frame = pi->primary_ref_frame;

   const struct VkVideoDecodeAV1DpbSlotInfoKHR *setup_dpb_slot =
      intra_only_decoding
         ? NULL
         : vk_find_struct_const(frame_info->pSetupReferenceSlot->pNext, VIDEO_DECODE_AV1_DPB_SLOT_INFO_KHR);

   /* The AMD FW interface does not need this information, since it's
    * redundant with the information derivable from the current frame header,
    * which the FW is parsing and tracking.
    */
   (void)setup_dpb_slot;
   result.curr_pic_idx = intra_only_decoding ? 0 : frame_info->pSetupReferenceSlot->slotIndex;

   result.sb_size = seq_hdr->flags.use_128x128_superblock;
   result.interp_filter = pi->interpolation_filter;
   for (i = 0; i < 2; ++i)
      result.filter_level[i] = pi->pLoopFilter->loop_filter_level[i];
   result.filter_level_u = pi->pLoopFilter->loop_filter_level[2];
   result.filter_level_v = pi->pLoopFilter->loop_filter_level[3];
   result.sharpness_level = pi->pLoopFilter->loop_filter_sharpness;
   for (i = 0; i < 8; ++i)
      result.ref_deltas[i] = pi->pLoopFilter->loop_filter_ref_deltas[i];
   for (i = 0; i < 2; ++i)
      result.mode_deltas[i] = pi->pLoopFilter->loop_filter_mode_deltas[i];
   result.base_qindex = pi->pQuantization->base_q_idx;
   result.y_dc_delta_q = pi->pQuantization->DeltaQYDc;
   result.u_dc_delta_q = pi->pQuantization->DeltaQUDc;
   result.v_dc_delta_q = pi->pQuantization->DeltaQVDc;
   result.u_ac_delta_q = pi->pQuantization->DeltaQUAc;
   result.v_ac_delta_q = pi->pQuantization->DeltaQVAc;

   if (pi->pQuantization->flags.using_qmatrix) {
      result.qm_y = pi->pQuantization->qm_y | 0xf0;
      result.qm_u = pi->pQuantization->qm_u | 0xf0;
      result.qm_v = pi->pQuantization->qm_v | 0xf0;
   } else {
      result.qm_y = 0xff;
      result.qm_u = 0xff;
      result.qm_v = 0xff;
   }
   result.delta_q_res = (1 << pi->delta_q_res);
   result.delta_lf_res = (1 << pi->delta_lf_res);
   result.tile_cols = pi->pTileInfo->TileCols;
   result.tile_rows = pi->pTileInfo->TileRows;

   result.tx_mode = pi->TxMode;
   result.reference_mode = (pi->flags.reference_select == 1) ? 2 : 0;
   result.chroma_format = seq_hdr->pColorConfig->flags.mono_chrome ? 0 : 1;
   result.tile_size_bytes = pi->pTileInfo->tile_size_bytes_minus_1;
   result.context_update_tile_id = pi->pTileInfo->context_update_tile_id;

   for (i = 0; i < result.tile_cols; i++)
      result.tile_col_start_sb[i] = pi->pTileInfo->pMiColStarts[i];
   result.tile_col_start_sb[result.tile_cols] =
      result.tile_col_start_sb[result.tile_cols - 1] + pi->pTileInfo->pWidthInSbsMinus1[result.tile_cols - 1] + 1;
   for (i = 0; i < pi->pTileInfo->TileRows; i++)
      result.tile_row_start_sb[i] = pi->pTileInfo->pMiRowStarts[i];
   result.tile_row_start_sb[result.tile_rows] =
      result.tile_row_start_sb[result.tile_rows - 1] + pi->pTileInfo->pHeightInSbsMinus1[result.tile_rows - 1] + 1;

   result.max_width = seq_hdr->max_frame_width_minus_1 + 1;
   result.max_height = seq_hdr->max_frame_height_minus_1 + 1;
   VkExtent2D frameExtent = frame_info->dstPictureResource.codedExtent;
   result.superres_scale_denominator =
      pi->flags.use_superres ? pi->coded_denom + AV1_SUPERRES_DENOM_MIN : AV1_SUPERRES_NUM;
   if (pi->flags.use_superres) {
      result.width =
         (frameExtent.width * 8 + result.superres_scale_denominator / 2) / result.superres_scale_denominator;
   } else {
      result.width = frameExtent.width;
   }
   result.height = frameExtent.height;

   result.superres_upscaled_width = frameExtent.width;

   result.order_hint_bits = seq_hdr->order_hint_bits_minus_1 + 1;

   /* The VCN FW will evict references that aren't specified in
    * ref_frame_map, even if they are still valid. To prevent this we will
    * specify every possible reference in ref_frame_map.
    */
   uint16_t used_slots = (1 << result.curr_pic_idx);
   for (i = 0; i < frame_info->referenceSlotCount; i++) {
      const struct VkVideoDecodeAV1DpbSlotInfoKHR *ref_dpb_slot =
         vk_find_struct_const(frame_info->pReferenceSlots[i].pNext, VIDEO_DECODE_AV1_DPB_SLOT_INFO_KHR);
      (void)ref_dpb_slot; /* Again, the FW is tracking this information for us, so no need for it. */
      (void)ref_dpb_slot; /* the FW is tracking this information for us, so no need for it. */
      int32_t slotIndex = frame_info->pReferenceSlots[i].slotIndex;
      result.ref_frame_map[i] = slotIndex;
      used_slots |= 1 << slotIndex;
   }
   /* Go through all the slots and fill in the ones that haven't been used. */
   for (j = 0; j < STD_VIDEO_AV1_NUM_REF_FRAMES + 1; j++) {
      if ((used_slots & (1 << j)) == 0) {
         result.ref_frame_map[i] = j;
         used_slots |= 1 << j;
         i++;
      }
   }

   assert(used_slots == 0x1ff && i == STD_VIDEO_AV1_NUM_REF_FRAMES);

   for (i = 0; i < STD_VIDEO_AV1_REFS_PER_FRAME; ++i) {
      result.frame_refs[i] =
         av1_pic_info->referenceNameSlotIndices[i] == -1 ? 0x7f : av1_pic_info->referenceNameSlotIndices[i];
   }

   result.bit_depth_luma_minus8 = result.bit_depth_chroma_minus8 = seq_hdr->pColorConfig->BitDepth - 8;

   int16_t *feature_data = (int16_t *)probs_ptr;
   int fd_idx = 0;
   for (i = 0; i < 8; ++i) {
      result.feature_mask[i] = pi->pSegmentation->FeatureEnabled[i];
      for (j = 0; j < 8; ++j) {
         result.feature_data[i][j] = pi->pSegmentation->FeatureData[i][j];
         feature_data[fd_idx++] = result.feature_data[i][j];
      }
   }

   memcpy(((char *)probs_ptr + 128), result.feature_mask, 8);
   result.cdef_damping = pi->pCDEF->cdef_damping_minus_3 + 3;
   result.cdef_bits = pi->pCDEF->cdef_bits;
   for (i = 0; i < 8; ++i) {
      result.cdef_strengths[i] = (pi->pCDEF->cdef_y_pri_strength[i] << 2) + pi->pCDEF->cdef_y_sec_strength[i];
      result.cdef_uv_strengths[i] = (pi->pCDEF->cdef_uv_pri_strength[i] << 2) + pi->pCDEF->cdef_uv_sec_strength[i];
   }

   if (pi->flags.UsesLr) {
      for (int plane = 0; plane < STD_VIDEO_AV1_MAX_NUM_PLANES; plane++) {
         result.frame_restoration_type[plane] = pi->pLoopRestoration->FrameRestorationType[plane];
         result.log2_restoration_unit_size_minus5[plane] = pi->pLoopRestoration->LoopRestorationSize[plane];
      }
   }

   if (seq_hdr->pColorConfig->BitDepth > 8) {
      if (vid->vk.picture_format == VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16 ||
          vid->vk.picture_format == VK_FORMAT_G16_B16R16_2PLANE_420_UNORM) {
         result.p010_mode = 1;
         result.msb_mode = 1;
      } else {
         result.luma_10to8 = 1;
         result.chroma_10to8 = 1;
      }
   }

   result.preskip_segid = 0;
   result.last_active_segid = 0;
   for (i = 0; i < 8; i++) {
      for (j = 0; j < 8; j++) {
         if (result.feature_mask[i] & (1 << j)) {
            result.last_active_segid = i;
            if (j >= 5)
               result.preskip_segid = 1;
         }
      }
   }
   result.seg_lossless_flag = 0;
   for (i = 0; i < 8; ++i) {
      int av1_get_qindex, qindex;
      int segfeature_active = result.feature_mask[i] & (1 << 0);
      if (segfeature_active) {
         int seg_qindex = result.base_qindex + result.feature_data[i][0];
         av1_get_qindex = seg_qindex < 0 ? 0 : (seg_qindex > 255 ? 255 : seg_qindex);
      } else {
         av1_get_qindex = result.base_qindex;
      }
      qindex = pi->flags.segmentation_enabled ? av1_get_qindex : result.base_qindex;
      result.seg_lossless_flag |= (((qindex == 0) && result.y_dc_delta_q == 0 && result.u_dc_delta_q == 0 &&
                                    result.v_dc_delta_q == 0 && result.u_ac_delta_q == 0 && result.v_ac_delta_q == 0)
                                   << i);
   }

   rvcn_dec_film_grain_params_t *fg_params = &result.film_grain;
   fg_params->apply_grain = pi->flags.apply_grain;
   if (fg_params->apply_grain) {
      rvcn_dec_av1_fg_init_buf_t *fg_buf = (rvcn_dec_av1_fg_init_buf_t *)((char *)probs_ptr + 256);
      fg_params->random_seed = pi->pFilmGrain->grain_seed;
      fg_params->grain_scale_shift = pi->pFilmGrain->grain_scale_shift;
      fg_params->scaling_shift = pi->pFilmGrain->grain_scaling_minus_8 + 8;
      fg_params->chroma_scaling_from_luma = pi->pFilmGrain->flags.chroma_scaling_from_luma;
      fg_params->num_y_points = pi->pFilmGrain->num_y_points;
      fg_params->num_cb_points = pi->pFilmGrain->num_cb_points;
      fg_params->num_cr_points = pi->pFilmGrain->num_cr_points;
      fg_params->cb_mult = pi->pFilmGrain->cb_mult;
      fg_params->cb_luma_mult = pi->pFilmGrain->cb_luma_mult;
      fg_params->cb_offset = pi->pFilmGrain->cb_offset;
      fg_params->cr_mult = pi->pFilmGrain->cr_mult;
      fg_params->cr_luma_mult = pi->pFilmGrain->cr_luma_mult;
      fg_params->cr_offset = pi->pFilmGrain->cr_offset;
      fg_params->bit_depth_minus_8 = result.bit_depth_luma_minus8;
      for (i = 0; i < fg_params->num_y_points; ++i) {
         fg_params->scaling_points_y[i][0] = pi->pFilmGrain->point_y_value[i];
         fg_params->scaling_points_y[i][1] = pi->pFilmGrain->point_y_scaling[i];
      }
      for (i = 0; i < fg_params->num_cb_points; ++i) {
         fg_params->scaling_points_cb[i][0] = pi->pFilmGrain->point_cb_value[i];
         fg_params->scaling_points_cb[i][1] = pi->pFilmGrain->point_cb_scaling[i];
      }
      for (i = 0; i < fg_params->num_cr_points; ++i) {
         fg_params->scaling_points_cr[i][0] = pi->pFilmGrain->point_cr_value[i];
         fg_params->scaling_points_cr[i][1] = pi->pFilmGrain->point_cr_scaling[i];
      }

      fg_params->ar_coeff_lag = pi->pFilmGrain->ar_coeff_lag;
      fg_params->ar_coeff_shift = pi->pFilmGrain->ar_coeff_shift_minus_6 + 6;

      for (i = 0; i < 24; ++i)
         fg_params->ar_coeffs_y[i] = pi->pFilmGrain->ar_coeffs_y_plus_128[i] - 128;

      for (i = 0; i < 25; ++i) {
         fg_params->ar_coeffs_cb[i] = pi->pFilmGrain->ar_coeffs_cb_plus_128[i] - 128;
         fg_params->ar_coeffs_cr[i] = pi->pFilmGrain->ar_coeffs_cr_plus_128[i] - 128;
      }

      fg_params->overlap_flag = pi->pFilmGrain->flags.overlap_flag;
      fg_params->clip_to_restricted_range = pi->pFilmGrain->flags.clip_to_restricted_range;
      radv_vcn_av1_init_film_grain_buffer(fg_params, fg_buf);
   }

   result.uncompressed_header_size = 0;
   for (i = 0; i < STD_VIDEO_AV1_NUM_REF_FRAMES; ++i) {
      result.global_motion[i].wmtype = pi->pGlobalMotion->GmType[i];
      for (j = 0; j < STD_VIDEO_AV1_GLOBAL_MOTION_PARAMS; ++j)
         result.global_motion[i].wmmat[j] = pi->pGlobalMotion->gm_params[i][j];
   }
   for (i = 0; i < av1_pic_info->tileCount && i < 256; ++i) {
      result.tile_info[i].offset = av1_pic_info->pTileOffsets[i];
      result.tile_info[i].size = av1_pic_info->pTileSizes[i];
   }

   return result;
}

static void
rvcn_av1_init_mode_probs(void *prob)
{
   rvcn_av1_frame_context_t *fc = (rvcn_av1_frame_context_t *)prob;
   int i;

   memcpy(fc->palette_y_size_cdf, default_palette_y_size_cdf, sizeof(default_palette_y_size_cdf));
   memcpy(fc->palette_uv_size_cdf, default_palette_uv_size_cdf, sizeof(default_palette_uv_size_cdf));
   memcpy(fc->palette_y_color_index_cdf, default_palette_y_color_index_cdf, sizeof(default_palette_y_color_index_cdf));
   memcpy(fc->palette_uv_color_index_cdf, default_palette_uv_color_index_cdf,
          sizeof(default_palette_uv_color_index_cdf));
   memcpy(fc->kf_y_cdf, default_kf_y_mode_cdf, sizeof(default_kf_y_mode_cdf));
   memcpy(fc->angle_delta_cdf, default_angle_delta_cdf, sizeof(default_angle_delta_cdf));
   memcpy(fc->comp_inter_cdf, default_comp_inter_cdf, sizeof(default_comp_inter_cdf));
   memcpy(fc->comp_ref_type_cdf, default_comp_ref_type_cdf, sizeof(default_comp_ref_type_cdf));
   memcpy(fc->uni_comp_ref_cdf, default_uni_comp_ref_cdf, sizeof(default_uni_comp_ref_cdf));
   memcpy(fc->palette_y_mode_cdf, default_palette_y_mode_cdf, sizeof(default_palette_y_mode_cdf));
   memcpy(fc->palette_uv_mode_cdf, default_palette_uv_mode_cdf, sizeof(default_palette_uv_mode_cdf));
   memcpy(fc->comp_ref_cdf, default_comp_ref_cdf, sizeof(default_comp_ref_cdf));
   memcpy(fc->comp_bwdref_cdf, default_comp_bwdref_cdf, sizeof(default_comp_bwdref_cdf));
   memcpy(fc->single_ref_cdf, default_single_ref_cdf, sizeof(default_single_ref_cdf));
   memcpy(fc->txfm_partition_cdf, default_txfm_partition_cdf, sizeof(default_txfm_partition_cdf));
   memcpy(fc->compound_index_cdf, default_compound_idx_cdfs, sizeof(default_compound_idx_cdfs));
   memcpy(fc->comp_group_idx_cdf, default_comp_group_idx_cdfs, sizeof(default_comp_group_idx_cdfs));
   memcpy(fc->newmv_cdf, default_newmv_cdf, sizeof(default_newmv_cdf));
   memcpy(fc->zeromv_cdf, default_zeromv_cdf, sizeof(default_zeromv_cdf));
   memcpy(fc->refmv_cdf, default_refmv_cdf, sizeof(default_refmv_cdf));
   memcpy(fc->drl_cdf, default_drl_cdf, sizeof(default_drl_cdf));
   memcpy(fc->motion_mode_cdf, default_motion_mode_cdf, sizeof(default_motion_mode_cdf));
   memcpy(fc->obmc_cdf, default_obmc_cdf, sizeof(default_obmc_cdf));
   memcpy(fc->inter_compound_mode_cdf, default_inter_compound_mode_cdf, sizeof(default_inter_compound_mode_cdf));
   memcpy(fc->compound_type_cdf, default_compound_type_cdf, sizeof(default_compound_type_cdf));
   memcpy(fc->wedge_idx_cdf, default_wedge_idx_cdf, sizeof(default_wedge_idx_cdf));
   memcpy(fc->interintra_cdf, default_interintra_cdf, sizeof(default_interintra_cdf));
   memcpy(fc->wedge_interintra_cdf, default_wedge_interintra_cdf, sizeof(default_wedge_interintra_cdf));
   memcpy(fc->interintra_mode_cdf, default_interintra_mode_cdf, sizeof(default_interintra_mode_cdf));
   memcpy(fc->pred_cdf, default_segment_pred_cdf, sizeof(default_segment_pred_cdf));
   memcpy(fc->switchable_restore_cdf, default_switchable_restore_cdf, sizeof(default_switchable_restore_cdf));
   memcpy(fc->wiener_restore_cdf, default_wiener_restore_cdf, sizeof(default_wiener_restore_cdf));
   memcpy(fc->sgrproj_restore_cdf, default_sgrproj_restore_cdf, sizeof(default_sgrproj_restore_cdf));
   memcpy(fc->y_mode_cdf, default_if_y_mode_cdf, sizeof(default_if_y_mode_cdf));
   memcpy(fc->uv_mode_cdf, default_uv_mode_cdf, sizeof(default_uv_mode_cdf));
   memcpy(fc->switchable_interp_cdf, default_switchable_interp_cdf, sizeof(default_switchable_interp_cdf));
   memcpy(fc->partition_cdf, default_partition_cdf, sizeof(default_partition_cdf));
   memcpy(fc->intra_ext_tx_cdf, default_intra_ext_tx_cdf, sizeof(default_intra_ext_tx_cdf));
   memcpy(fc->inter_ext_tx_cdf, default_inter_ext_tx_cdf, sizeof(default_inter_ext_tx_cdf));
   memcpy(fc->skip_cdfs, default_skip_cdfs, sizeof(default_skip_cdfs));
   memcpy(fc->intra_inter_cdf, default_intra_inter_cdf, sizeof(default_intra_inter_cdf));
   memcpy(fc->tree_cdf, default_seg_tree_cdf, sizeof(default_seg_tree_cdf));
   for (i = 0; i < SPATIAL_PREDICTION_PROBS; ++i)
      memcpy(fc->spatial_pred_seg_cdf[i], default_spatial_pred_seg_tree_cdf[i],
             sizeof(default_spatial_pred_seg_tree_cdf[i]));
   memcpy(fc->tx_size_cdf, default_tx_size_cdf, sizeof(default_tx_size_cdf));
   memcpy(fc->delta_q_cdf, default_delta_q_cdf, sizeof(default_delta_q_cdf));
   memcpy(fc->skip_mode_cdfs, default_skip_mode_cdfs, sizeof(default_skip_mode_cdfs));
   memcpy(fc->delta_lf_cdf, default_delta_lf_cdf, sizeof(default_delta_lf_cdf));
   memcpy(fc->delta_lf_multi_cdf, default_delta_lf_multi_cdf, sizeof(default_delta_lf_multi_cdf));
   memcpy(fc->cfl_sign_cdf, default_cfl_sign_cdf, sizeof(default_cfl_sign_cdf));
   memcpy(fc->cfl_alpha_cdf, default_cfl_alpha_cdf, sizeof(default_cfl_alpha_cdf));
   memcpy(fc->filter_intra_cdfs, default_filter_intra_cdfs, sizeof(default_filter_intra_cdfs));
   memcpy(fc->filter_intra_mode_cdf, default_filter_intra_mode_cdf, sizeof(default_filter_intra_mode_cdf));
   memcpy(fc->intrabc_cdf, default_intrabc_cdf, sizeof(default_intrabc_cdf));
}

static void
rvcn_av1_init_mv_probs(void *prob)
{
   rvcn_av1_frame_context_t *fc = (rvcn_av1_frame_context_t *)prob;

   memcpy(fc->nmvc_joints_cdf, default_nmv_context.joints_cdf, sizeof(default_nmv_context.joints_cdf));
   memcpy(fc->nmvc_0_bits_cdf, default_nmv_context.comps[0].bits_cdf, sizeof(default_nmv_context.comps[0].bits_cdf));
   memcpy(fc->nmvc_0_class0_cdf, default_nmv_context.comps[0].class0_cdf,
          sizeof(default_nmv_context.comps[0].class0_cdf));
   memcpy(fc->nmvc_0_class0_fp_cdf, default_nmv_context.comps[0].class0_fp_cdf,
          sizeof(default_nmv_context.comps[0].class0_fp_cdf));
   memcpy(fc->nmvc_0_class0_hp_cdf, default_nmv_context.comps[0].class0_hp_cdf,
          sizeof(default_nmv_context.comps[0].class0_hp_cdf));
   memcpy(fc->nmvc_0_classes_cdf, default_nmv_context.comps[0].classes_cdf,
          sizeof(default_nmv_context.comps[0].classes_cdf));
   memcpy(fc->nmvc_0_fp_cdf, default_nmv_context.comps[0].fp_cdf, sizeof(default_nmv_context.comps[0].fp_cdf));
   memcpy(fc->nmvc_0_hp_cdf, default_nmv_context.comps[0].hp_cdf, sizeof(default_nmv_context.comps[0].hp_cdf));
   memcpy(fc->nmvc_0_sign_cdf, default_nmv_context.comps[0].sign_cdf, sizeof(default_nmv_context.comps[0].sign_cdf));
   memcpy(fc->nmvc_1_bits_cdf, default_nmv_context.comps[1].bits_cdf, sizeof(default_nmv_context.comps[1].bits_cdf));
   memcpy(fc->nmvc_1_class0_cdf, default_nmv_context.comps[1].class0_cdf,
          sizeof(default_nmv_context.comps[1].class0_cdf));
   memcpy(fc->nmvc_1_class0_fp_cdf, default_nmv_context.comps[1].class0_fp_cdf,
          sizeof(default_nmv_context.comps[1].class0_fp_cdf));
   memcpy(fc->nmvc_1_class0_hp_cdf, default_nmv_context.comps[1].class0_hp_cdf,
          sizeof(default_nmv_context.comps[1].class0_hp_cdf));
   memcpy(fc->nmvc_1_classes_cdf, default_nmv_context.comps[1].classes_cdf,
          sizeof(default_nmv_context.comps[1].classes_cdf));
   memcpy(fc->nmvc_1_fp_cdf, default_nmv_context.comps[1].fp_cdf, sizeof(default_nmv_context.comps[1].fp_cdf));
   memcpy(fc->nmvc_1_hp_cdf, default_nmv_context.comps[1].hp_cdf, sizeof(default_nmv_context.comps[1].hp_cdf));
   memcpy(fc->nmvc_1_sign_cdf, default_nmv_context.comps[1].sign_cdf, sizeof(default_nmv_context.comps[1].sign_cdf));
   memcpy(fc->ndvc_joints_cdf, default_nmv_context.joints_cdf, sizeof(default_nmv_context.joints_cdf));
   memcpy(fc->ndvc_0_bits_cdf, default_nmv_context.comps[0].bits_cdf, sizeof(default_nmv_context.comps[0].bits_cdf));
   memcpy(fc->ndvc_0_class0_cdf, default_nmv_context.comps[0].class0_cdf,
          sizeof(default_nmv_context.comps[0].class0_cdf));
   memcpy(fc->ndvc_0_class0_fp_cdf, default_nmv_context.comps[0].class0_fp_cdf,
          sizeof(default_nmv_context.comps[0].class0_fp_cdf));
   memcpy(fc->ndvc_0_class0_hp_cdf, default_nmv_context.comps[0].class0_hp_cdf,
          sizeof(default_nmv_context.comps[0].class0_hp_cdf));
   memcpy(fc->ndvc_0_classes_cdf, default_nmv_context.comps[0].classes_cdf,
          sizeof(default_nmv_context.comps[0].classes_cdf));
   memcpy(fc->ndvc_0_fp_cdf, default_nmv_context.comps[0].fp_cdf, sizeof(default_nmv_context.comps[0].fp_cdf));
   memcpy(fc->ndvc_0_hp_cdf, default_nmv_context.comps[0].hp_cdf, sizeof(default_nmv_context.comps[0].hp_cdf));
   memcpy(fc->ndvc_0_sign_cdf, default_nmv_context.comps[0].sign_cdf, sizeof(default_nmv_context.comps[0].sign_cdf));
   memcpy(fc->ndvc_1_bits_cdf, default_nmv_context.comps[1].bits_cdf, sizeof(default_nmv_context.comps[1].bits_cdf));
   memcpy(fc->ndvc_1_class0_cdf, default_nmv_context.comps[1].class0_cdf,
          sizeof(default_nmv_context.comps[1].class0_cdf));
   memcpy(fc->ndvc_1_class0_fp_cdf, default_nmv_context.comps[1].class0_fp_cdf,
          sizeof(default_nmv_context.comps[1].class0_fp_cdf));
   memcpy(fc->ndvc_1_class0_hp_cdf, default_nmv_context.comps[1].class0_hp_cdf,
          sizeof(default_nmv_context.comps[1].class0_hp_cdf));
   memcpy(fc->ndvc_1_classes_cdf, default_nmv_context.comps[1].classes_cdf,
          sizeof(default_nmv_context.comps[1].classes_cdf));
   memcpy(fc->ndvc_1_fp_cdf, default_nmv_context.comps[1].fp_cdf, sizeof(default_nmv_context.comps[1].fp_cdf));
   memcpy(fc->ndvc_1_hp_cdf, default_nmv_context.comps[1].hp_cdf, sizeof(default_nmv_context.comps[1].hp_cdf));
   memcpy(fc->ndvc_1_sign_cdf, default_nmv_context.comps[1].sign_cdf, sizeof(default_nmv_context.comps[1].sign_cdf));
}

static void
rvcn_av1_default_coef_probs(void *prob, int index)
{
   rvcn_av1_frame_context_t *fc = (rvcn_av1_frame_context_t *)prob;

   memcpy(fc->txb_skip_cdf, av1_default_txb_skip_cdfs[index], sizeof(av1_default_txb_skip_cdfs[index]));
   memcpy(fc->eob_extra_cdf, av1_default_eob_extra_cdfs[index], sizeof(av1_default_eob_extra_cdfs[index]));
   memcpy(fc->dc_sign_cdf, av1_default_dc_sign_cdfs[index], sizeof(av1_default_dc_sign_cdfs[index]));
   memcpy(fc->coeff_br_cdf, av1_default_coeff_lps_multi_cdfs[index], sizeof(av1_default_coeff_lps_multi_cdfs[index]));
   memcpy(fc->coeff_base_cdf, av1_default_coeff_base_multi_cdfs[index],
          sizeof(av1_default_coeff_base_multi_cdfs[index]));
   memcpy(fc->coeff_base_eob_cdf, av1_default_coeff_base_eob_multi_cdfs[index],
          sizeof(av1_default_coeff_base_eob_multi_cdfs[index]));
   memcpy(fc->eob_flag_cdf16, av1_default_eob_multi16_cdfs[index], sizeof(av1_default_eob_multi16_cdfs[index]));
   memcpy(fc->eob_flag_cdf32, av1_default_eob_multi32_cdfs[index], sizeof(av1_default_eob_multi32_cdfs[index]));
   memcpy(fc->eob_flag_cdf64, av1_default_eob_multi64_cdfs[index], sizeof(av1_default_eob_multi64_cdfs[index]));
   memcpy(fc->eob_flag_cdf128, av1_default_eob_multi128_cdfs[index], sizeof(av1_default_eob_multi128_cdfs[index]));
   memcpy(fc->eob_flag_cdf256, av1_default_eob_multi256_cdfs[index], sizeof(av1_default_eob_multi256_cdfs[index]));
   memcpy(fc->eob_flag_cdf512, av1_default_eob_multi512_cdfs[index], sizeof(av1_default_eob_multi512_cdfs[index]));
   memcpy(fc->eob_flag_cdf1024, av1_default_eob_multi1024_cdfs[index], sizeof(av1_default_eob_multi1024_cdfs[index]));
}

static void
rvcn_vcn4_init_mode_probs(void *prob)
{
   rvcn_av1_vcn4_frame_context_t *fc = (rvcn_av1_vcn4_frame_context_t *)prob;
   int i;

   memcpy(fc->palette_y_size_cdf, default_palette_y_size_cdf, sizeof(default_palette_y_size_cdf));
   memcpy(fc->palette_uv_size_cdf, default_palette_uv_size_cdf, sizeof(default_palette_uv_size_cdf));
   memcpy(fc->palette_y_color_index_cdf, default_palette_y_color_index_cdf, sizeof(default_palette_y_color_index_cdf));
   memcpy(fc->palette_uv_color_index_cdf, default_palette_uv_color_index_cdf,
          sizeof(default_palette_uv_color_index_cdf));
   memcpy(fc->kf_y_cdf, default_kf_y_mode_cdf, sizeof(default_kf_y_mode_cdf));
   memcpy(fc->angle_delta_cdf, default_angle_delta_cdf, sizeof(default_angle_delta_cdf));
   memcpy(fc->comp_inter_cdf, default_comp_inter_cdf, sizeof(default_comp_inter_cdf));
   memcpy(fc->comp_ref_type_cdf, default_comp_ref_type_cdf, sizeof(default_comp_ref_type_cdf));
   memcpy(fc->uni_comp_ref_cdf, default_uni_comp_ref_cdf, sizeof(default_uni_comp_ref_cdf));
   memcpy(fc->palette_y_mode_cdf, default_palette_y_mode_cdf, sizeof(default_palette_y_mode_cdf));
   memcpy(fc->palette_uv_mode_cdf, default_palette_uv_mode_cdf, sizeof(default_palette_uv_mode_cdf));
   memcpy(fc->comp_ref_cdf, default_comp_ref_cdf, sizeof(default_comp_ref_cdf));
   memcpy(fc->comp_bwdref_cdf, default_comp_bwdref_cdf, sizeof(default_comp_bwdref_cdf));
   memcpy(fc->single_ref_cdf, default_single_ref_cdf, sizeof(default_single_ref_cdf));
   memcpy(fc->txfm_partition_cdf, default_txfm_partition_cdf, sizeof(default_txfm_partition_cdf));
   memcpy(fc->compound_index_cdf, default_compound_idx_cdfs, sizeof(default_compound_idx_cdfs));
   memcpy(fc->comp_group_idx_cdf, default_comp_group_idx_cdfs, sizeof(default_comp_group_idx_cdfs));
   memcpy(fc->newmv_cdf, default_newmv_cdf, sizeof(default_newmv_cdf));
   memcpy(fc->zeromv_cdf, default_zeromv_cdf, sizeof(default_zeromv_cdf));
   memcpy(fc->refmv_cdf, default_refmv_cdf, sizeof(default_refmv_cdf));
   memcpy(fc->drl_cdf, default_drl_cdf, sizeof(default_drl_cdf));
   memcpy(fc->motion_mode_cdf, default_motion_mode_cdf, sizeof(default_motion_mode_cdf));
   memcpy(fc->obmc_cdf, default_obmc_cdf, sizeof(default_obmc_cdf));
   memcpy(fc->inter_compound_mode_cdf, default_inter_compound_mode_cdf, sizeof(default_inter_compound_mode_cdf));
   memcpy(fc->compound_type_cdf, default_compound_type_cdf, sizeof(default_compound_type_cdf));
   memcpy(fc->wedge_idx_cdf, default_wedge_idx_cdf, sizeof(default_wedge_idx_cdf));
   memcpy(fc->interintra_cdf, default_interintra_cdf, sizeof(default_interintra_cdf));
   memcpy(fc->wedge_interintra_cdf, default_wedge_interintra_cdf, sizeof(default_wedge_interintra_cdf));
   memcpy(fc->interintra_mode_cdf, default_interintra_mode_cdf, sizeof(default_interintra_mode_cdf));
   memcpy(fc->pred_cdf, default_segment_pred_cdf, sizeof(default_segment_pred_cdf));
   memcpy(fc->switchable_restore_cdf, default_switchable_restore_cdf, sizeof(default_switchable_restore_cdf));
   memcpy(fc->wiener_restore_cdf, default_wiener_restore_cdf, sizeof(default_wiener_restore_cdf));
   memcpy(fc->sgrproj_restore_cdf, default_sgrproj_restore_cdf, sizeof(default_sgrproj_restore_cdf));
   memcpy(fc->y_mode_cdf, default_if_y_mode_cdf, sizeof(default_if_y_mode_cdf));
   memcpy(fc->uv_mode_cdf, default_uv_mode_cdf, sizeof(default_uv_mode_cdf));
   memcpy(fc->switchable_interp_cdf, default_switchable_interp_cdf, sizeof(default_switchable_interp_cdf));
   memcpy(fc->partition_cdf, default_partition_cdf, sizeof(default_partition_cdf));
   memcpy(fc->intra_ext_tx_cdf, &default_intra_ext_tx_cdf[1], sizeof(default_intra_ext_tx_cdf[1]) * 2);
   memcpy(fc->inter_ext_tx_cdf, &default_inter_ext_tx_cdf[1], sizeof(default_inter_ext_tx_cdf[1]) * 3);
   memcpy(fc->skip_cdfs, default_skip_cdfs, sizeof(default_skip_cdfs));
   memcpy(fc->intra_inter_cdf, default_intra_inter_cdf, sizeof(default_intra_inter_cdf));
   memcpy(fc->tree_cdf, default_seg_tree_cdf, sizeof(default_seg_tree_cdf));
   for (i = 0; i < SPATIAL_PREDICTION_PROBS; ++i)
      memcpy(fc->spatial_pred_seg_cdf[i], default_spatial_pred_seg_tree_cdf[i],
             sizeof(default_spatial_pred_seg_tree_cdf[i]));
   memcpy(fc->tx_size_cdf, default_tx_size_cdf, sizeof(default_tx_size_cdf));
   memcpy(fc->delta_q_cdf, default_delta_q_cdf, sizeof(default_delta_q_cdf));
   memcpy(fc->skip_mode_cdfs, default_skip_mode_cdfs, sizeof(default_skip_mode_cdfs));
   memcpy(fc->delta_lf_cdf, default_delta_lf_cdf, sizeof(default_delta_lf_cdf));
   memcpy(fc->delta_lf_multi_cdf, default_delta_lf_multi_cdf, sizeof(default_delta_lf_multi_cdf));
   memcpy(fc->cfl_sign_cdf, default_cfl_sign_cdf, sizeof(default_cfl_sign_cdf));
   memcpy(fc->cfl_alpha_cdf, default_cfl_alpha_cdf, sizeof(default_cfl_alpha_cdf));
   memcpy(fc->filter_intra_cdfs, default_filter_intra_cdfs, sizeof(default_filter_intra_cdfs));
   memcpy(fc->filter_intra_mode_cdf, default_filter_intra_mode_cdf, sizeof(default_filter_intra_mode_cdf));
   memcpy(fc->intrabc_cdf, default_intrabc_cdf, sizeof(default_intrabc_cdf));
}

static void
rvcn_vcn4_av1_init_mv_probs(void *prob)
{
   rvcn_av1_vcn4_frame_context_t *fc = (rvcn_av1_vcn4_frame_context_t *)prob;

   memcpy(fc->nmvc_joints_cdf, default_nmv_context.joints_cdf, sizeof(default_nmv_context.joints_cdf));
   memcpy(fc->nmvc_0_bits_cdf, default_nmv_context.comps[0].bits_cdf, sizeof(default_nmv_context.comps[0].bits_cdf));
   memcpy(fc->nmvc_0_class0_cdf, default_nmv_context.comps[0].class0_cdf,
          sizeof(default_nmv_context.comps[0].class0_cdf));
   memcpy(fc->nmvc_0_class0_fp_cdf, default_nmv_context.comps[0].class0_fp_cdf,
          sizeof(default_nmv_context.comps[0].class0_fp_cdf));
   memcpy(fc->nmvc_0_class0_hp_cdf, default_nmv_context.comps[0].class0_hp_cdf,
          sizeof(default_nmv_context.comps[0].class0_hp_cdf));
   memcpy(fc->nmvc_0_classes_cdf, default_nmv_context.comps[0].classes_cdf,
          sizeof(default_nmv_context.comps[0].classes_cdf));
   memcpy(fc->nmvc_0_fp_cdf, default_nmv_context.comps[0].fp_cdf, sizeof(default_nmv_context.comps[0].fp_cdf));
   memcpy(fc->nmvc_0_hp_cdf, default_nmv_context.comps[0].hp_cdf, sizeof(default_nmv_context.comps[0].hp_cdf));
   memcpy(fc->nmvc_0_sign_cdf, default_nmv_context.comps[0].sign_cdf, sizeof(default_nmv_context.comps[0].sign_cdf));
   memcpy(fc->nmvc_1_bits_cdf, default_nmv_context.comps[1].bits_cdf, sizeof(default_nmv_context.comps[1].bits_cdf));
   memcpy(fc->nmvc_1_class0_cdf, default_nmv_context.comps[1].class0_cdf,
          sizeof(default_nmv_context.comps[1].class0_cdf));
   memcpy(fc->nmvc_1_class0_fp_cdf, default_nmv_context.comps[1].class0_fp_cdf,
          sizeof(default_nmv_context.comps[1].class0_fp_cdf));
   memcpy(fc->nmvc_1_class0_hp_cdf, default_nmv_context.comps[1].class0_hp_cdf,
          sizeof(default_nmv_context.comps[1].class0_hp_cdf));
   memcpy(fc->nmvc_1_classes_cdf, default_nmv_context.comps[1].classes_cdf,
          sizeof(default_nmv_context.comps[1].classes_cdf));
   memcpy(fc->nmvc_1_fp_cdf, default_nmv_context.comps[1].fp_cdf, sizeof(default_nmv_context.comps[1].fp_cdf));
   memcpy(fc->nmvc_1_hp_cdf, default_nmv_context.comps[1].hp_cdf, sizeof(default_nmv_context.comps[1].hp_cdf));
   memcpy(fc->nmvc_1_sign_cdf, default_nmv_context.comps[1].sign_cdf, sizeof(default_nmv_context.comps[1].sign_cdf));
   memcpy(fc->ndvc_joints_cdf, default_nmv_context.joints_cdf, sizeof(default_nmv_context.joints_cdf));
   memcpy(fc->ndvc_0_bits_cdf, default_nmv_context.comps[0].bits_cdf, sizeof(default_nmv_context.comps[0].bits_cdf));
   memcpy(fc->ndvc_0_class0_cdf, default_nmv_context.comps[0].class0_cdf,
          sizeof(default_nmv_context.comps[0].class0_cdf));
   memcpy(fc->ndvc_0_class0_fp_cdf, default_nmv_context.comps[0].class0_fp_cdf,
          sizeof(default_nmv_context.comps[0].class0_fp_cdf));
   memcpy(fc->ndvc_0_class0_hp_cdf, default_nmv_context.comps[0].class0_hp_cdf,
          sizeof(default_nmv_context.comps[0].class0_hp_cdf));
   memcpy(fc->ndvc_0_classes_cdf, default_nmv_context.comps[0].classes_cdf,
          sizeof(default_nmv_context.comps[0].classes_cdf));
   memcpy(fc->ndvc_0_fp_cdf, default_nmv_context.comps[0].fp_cdf, sizeof(default_nmv_context.comps[0].fp_cdf));
   memcpy(fc->ndvc_0_hp_cdf, default_nmv_context.comps[0].hp_cdf, sizeof(default_nmv_context.comps[0].hp_cdf));
   memcpy(fc->ndvc_0_sign_cdf, default_nmv_context.comps[0].sign_cdf, sizeof(default_nmv_context.comps[0].sign_cdf));
   memcpy(fc->ndvc_1_bits_cdf, default_nmv_context.comps[1].bits_cdf, sizeof(default_nmv_context.comps[1].bits_cdf));
   memcpy(fc->ndvc_1_class0_cdf, default_nmv_context.comps[1].class0_cdf,
          sizeof(default_nmv_context.comps[1].class0_cdf));
   memcpy(fc->ndvc_1_class0_fp_cdf, default_nmv_context.comps[1].class0_fp_cdf,
          sizeof(default_nmv_context.comps[1].class0_fp_cdf));
   memcpy(fc->ndvc_1_class0_hp_cdf, default_nmv_context.comps[1].class0_hp_cdf,
          sizeof(default_nmv_context.comps[1].class0_hp_cdf));
   memcpy(fc->ndvc_1_classes_cdf, default_nmv_context.comps[1].classes_cdf,
          sizeof(default_nmv_context.comps[1].classes_cdf));
   memcpy(fc->ndvc_1_fp_cdf, default_nmv_context.comps[1].fp_cdf, sizeof(default_nmv_context.comps[1].fp_cdf));
   memcpy(fc->ndvc_1_hp_cdf, default_nmv_context.comps[1].hp_cdf, sizeof(default_nmv_context.comps[1].hp_cdf));
   memcpy(fc->ndvc_1_sign_cdf, default_nmv_context.comps[1].sign_cdf, sizeof(default_nmv_context.comps[1].sign_cdf));
}

static void
rvcn_vcn4_av1_default_coef_probs(void *prob, int index)
{
   rvcn_av1_vcn4_frame_context_t *fc = (rvcn_av1_vcn4_frame_context_t *)prob;
   char *p;
   int i, j;
   unsigned size;

   memcpy(fc->txb_skip_cdf, av1_default_txb_skip_cdfs[index], sizeof(av1_default_txb_skip_cdfs[index]));

   p = (char *)fc->eob_extra_cdf;
   size = sizeof(av1_default_eob_extra_cdfs[0][0][0][0]) * EOB_COEF_CONTEXTS_VCN4;
   for (i = 0; i < AV1_TX_SIZES; i++) {
      for (j = 0; j < AV1_PLANE_TYPES; j++) {
         memcpy(p, &av1_default_eob_extra_cdfs[index][i][j][3], size);
         p += size;
      }
   }

   memcpy(fc->dc_sign_cdf, av1_default_dc_sign_cdfs[index], sizeof(av1_default_dc_sign_cdfs[index]));
   memcpy(fc->coeff_br_cdf, av1_default_coeff_lps_multi_cdfs[index], sizeof(av1_default_coeff_lps_multi_cdfs[index]));
   memcpy(fc->coeff_base_cdf, av1_default_coeff_base_multi_cdfs[index],
          sizeof(av1_default_coeff_base_multi_cdfs[index]));
   memcpy(fc->coeff_base_eob_cdf, av1_default_coeff_base_eob_multi_cdfs[index],
          sizeof(av1_default_coeff_base_eob_multi_cdfs[index]));
   memcpy(fc->eob_flag_cdf16, av1_default_eob_multi16_cdfs[index], sizeof(av1_default_eob_multi16_cdfs[index]));
   memcpy(fc->eob_flag_cdf32, av1_default_eob_multi32_cdfs[index], sizeof(av1_default_eob_multi32_cdfs[index]));
   memcpy(fc->eob_flag_cdf64, av1_default_eob_multi64_cdfs[index], sizeof(av1_default_eob_multi64_cdfs[index]));
   memcpy(fc->eob_flag_cdf128, av1_default_eob_multi128_cdfs[index], sizeof(av1_default_eob_multi128_cdfs[index]));
   memcpy(fc->eob_flag_cdf256, av1_default_eob_multi256_cdfs[index], sizeof(av1_default_eob_multi256_cdfs[index]));
   memcpy(fc->eob_flag_cdf512, av1_default_eob_multi512_cdfs[index], sizeof(av1_default_eob_multi512_cdfs[index]));
   memcpy(fc->eob_flag_cdf1024, av1_default_eob_multi1024_cdfs[index], sizeof(av1_default_eob_multi1024_cdfs[index]));
}

static bool
rvcn_dec_message_decode(struct radv_cmd_buffer *cmd_buffer, struct radv_video_session *vid,
                        struct radv_video_session_params *params, void *ptr, void *it_probs_ptr, uint32_t *slice_offset,
                        const struct VkVideoDecodeInfoKHR *frame_info)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   rvcn_dec_message_header_t *header;
   rvcn_dec_message_index_t *index_codec;
   rvcn_dec_message_decode_t *decode;
   rvcn_dec_message_index_t *index_dynamic_dpb = NULL;
   rvcn_dec_message_dynamic_dpb_t2_t *dynamic_dpb_t2 = NULL;
   void *codec;
   unsigned sizes = 0, offset_decode, offset_codec, offset_dynamic_dpb;
   struct radv_image_view *dst_iv = radv_image_view_from_handle(frame_info->dstPictureResource.imageViewBinding);
   struct radv_image *img = dst_iv->image;
   struct radv_image_plane *luma = &img->planes[0];
   struct radv_image_plane *chroma = &img->planes[1];

   header = ptr;
   sizes += sizeof(rvcn_dec_message_header_t);

   index_codec = (void *)((char *)header + sizes);
   sizes += sizeof(rvcn_dec_message_index_t);

   if (vid->dpb_type == DPB_DYNAMIC_TIER_2) {
      index_dynamic_dpb = (void *)((char *)header + sizes);
      sizes += sizeof(rvcn_dec_message_index_t);
   }

   offset_decode = sizes;
   decode = (void *)((char *)header + sizes);
   sizes += sizeof(rvcn_dec_message_decode_t);

   if (vid->dpb_type == DPB_DYNAMIC_TIER_2) {
      offset_dynamic_dpb = sizes;
      dynamic_dpb_t2 = (void *)((char *)header + sizes);
      sizes += sizeof(rvcn_dec_message_dynamic_dpb_t2_t);
   }

   offset_codec = sizes;
   codec = (void *)((char *)header + sizes);

   memset(ptr, 0, sizes);

   header->header_size = sizeof(rvcn_dec_message_header_t);
   header->total_size = sizes;
   header->msg_type = RDECODE_MSG_DECODE;
   header->stream_handle = vid->stream_handle;
   header->status_report_feedback_number = vid->dbg_frame_cnt++;

   header->index[0].message_id = RDECODE_MESSAGE_DECODE;
   header->index[0].offset = offset_decode;
   header->index[0].size = sizeof(rvcn_dec_message_decode_t);
   header->index[0].filled = 0;
   header->num_buffers = 1;

   index_codec->offset = offset_codec;
   index_codec->filled = 0;
   ++header->num_buffers;

   if (vid->dpb_type == DPB_DYNAMIC_TIER_2) {
      index_dynamic_dpb->message_id = RDECODE_MESSAGE_DYNAMIC_DPB;
      index_dynamic_dpb->offset = offset_dynamic_dpb;
      index_dynamic_dpb->filled = 0;
      ++header->num_buffers;
      index_dynamic_dpb->size = sizeof(rvcn_dec_message_dynamic_dpb_t2_t);
   }

   decode->stream_type = vid->stream_type;
   decode->decode_flags = 0;
   decode->width_in_samples = frame_info->dstPictureResource.codedExtent.width;
   decode->height_in_samples = frame_info->dstPictureResource.codedExtent.height;

   decode->bsd_size = frame_info->srcBufferRange;

   decode->dt_size = dst_iv->image->planes[0].surface.total_size + dst_iv->image->planes[1].surface.total_size;
   decode->sct_size = 0;
   decode->sc_coeff_size = 0;

   decode->sw_ctxt_size = RDECODE_SESSION_CONTEXT_SIZE;

   decode->dt_pitch = luma->surface.u.gfx9.surf_pitch * luma->surface.blk_w;
   decode->dt_uv_pitch = chroma->surface.u.gfx9.surf_pitch * chroma->surface.blk_w;

   if (luma->surface.meta_offset) {
      fprintf(stderr, "DCC SURFACES NOT SUPPORTED.\n");
      return false;
   }

   decode->dt_tiling_mode = 0;
   decode->dt_swizzle_mode = luma->surface.u.gfx9.swizzle_mode;
   decode->dt_array_mode = pdev->vid_addr_gfx_mode;
   decode->dt_field_mode = vid->interlaced ? 1 : 0;
   decode->dt_surf_tile_config = 0;
   decode->dt_uv_surf_tile_config = 0;

   decode->dt_luma_top_offset = luma->surface.u.gfx9.surf_offset;
   decode->dt_chroma_top_offset = chroma->surface.u.gfx9.surf_offset;

   if (decode->dt_field_mode) {
      decode->dt_luma_bottom_offset = luma->surface.u.gfx9.surf_offset + luma->surface.u.gfx9.surf_slice_size;
      decode->dt_chroma_bottom_offset = chroma->surface.u.gfx9.surf_offset + chroma->surface.u.gfx9.surf_slice_size;
   } else {
      decode->dt_luma_bottom_offset = decode->dt_luma_top_offset;
      decode->dt_chroma_bottom_offset = decode->dt_chroma_top_offset;
   }
   if (vid->stream_type == RDECODE_CODEC_AV1)
      decode->db_pitch_uv = chroma->surface.u.gfx9.surf_pitch * chroma->surface.blk_w;

   *slice_offset = 0;

   /* Intra-only decoding will only work without a setup slot for AV1
    * currently, other codecs require the application to pass a
    * setup slot for this use-case, since the FW is not able to skip write-out
    * for H26X.  In order to fix that properly, additional scratch space will
    * be needed in the video session just for intra-only DPB targets.
    */
   int dpb_update_required = 1;

   switch (vid->vk.op) {
   case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR: {
      index_codec->size = sizeof(rvcn_dec_message_avc_t);
      rvcn_dec_message_avc_t avc = get_h264_msg(vid, params, frame_info, slice_offset, &decode->width_in_samples,
                                                &decode->height_in_samples, it_probs_ptr);
      memcpy(codec, (void *)&avc, sizeof(rvcn_dec_message_avc_t));
      index_codec->message_id = RDECODE_MESSAGE_AVC;
      break;
   }
   case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR: {
      index_codec->size = sizeof(rvcn_dec_message_hevc_t);
      rvcn_dec_message_hevc_t hevc = get_h265_msg(device, vid, params, frame_info,
                                                  &decode->width_in_samples,
                                                  &decode->height_in_samples,
                                                  it_probs_ptr);
      memcpy(codec, (void *)&hevc, sizeof(rvcn_dec_message_hevc_t));
      index_codec->message_id = RDECODE_MESSAGE_HEVC;
      break;
   }
   case VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR: {
      index_codec->size = sizeof(rvcn_dec_message_av1_t);
      rvcn_dec_message_av1_t av1 = get_av1_msg(device, vid, params, frame_info, it_probs_ptr, &dpb_update_required);
      memcpy(codec, (void *)&av1, sizeof(rvcn_dec_message_av1_t));
      index_codec->message_id = RDECODE_MESSAGE_AV1;
      assert(frame_info->referenceSlotCount < 9);
      break;
   }
   default:
      unreachable("unknown operation");
   }

   if (dpb_update_required)
      assert(frame_info->pSetupReferenceSlot != NULL);

   struct radv_image_view *dpb_iv =
      dpb_update_required
         ? radv_image_view_from_handle(frame_info->pSetupReferenceSlot->pPictureResource->imageViewBinding)
         : NULL;
   struct radv_image *dpb = dpb_update_required ? dpb_iv->image : img;

   int dpb_array_idx = 0;
   if (dpb_update_required) {
      if (dpb_iv->vk.view_type == VK_IMAGE_VIEW_TYPE_2D_ARRAY)
         dpb_array_idx = frame_info->pSetupReferenceSlot->pPictureResource->baseArrayLayer;
   }

   decode->dpb_size = (vid->dpb_type != DPB_DYNAMIC_TIER_2) ? dpb->size : 0;
   decode->db_pitch = dpb->planes[0].surface.u.gfx9.surf_pitch;
   decode->db_aligned_height = dpb->planes[0].surface.u.gfx9.surf_height;
   decode->db_swizzle_mode = dpb->planes[0].surface.u.gfx9.swizzle_mode;
   decode->db_array_mode = pdev->vid_addr_gfx_mode;

   decode->hw_ctxt_size = vid->ctx.size;

   if (vid->dpb_type != DPB_DYNAMIC_TIER_2)
      return true;

   uint64_t addr;
   radv_cs_add_buffer(device->ws, cmd_buffer->cs, dpb->bindings[0].bo);
   addr = radv_buffer_get_va(dpb->bindings[0].bo) + dpb->bindings[0].offset;

   addr += dpb_array_idx * (dpb->planes[0].surface.u.gfx9.surf_slice_size + dpb->planes[1].surface.u.gfx9.surf_slice_size);
   dynamic_dpb_t2->dpbCurrLo = addr;
   dynamic_dpb_t2->dpbCurrHi = addr >> 32;

   if (vid->vk.op == VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR) {
      /* The following loop will fill in the references for the current frame,
       * this ensures all DPB addresses are "valid" (pointing at the current
       * decode target), so that the firmware doesn't evict things it should not.
       * It will not perform any actual writes to these dummy slots.
       */
      for (int i = 0; i < STD_VIDEO_AV1_NUM_REF_FRAMES; i++) {
         dynamic_dpb_t2->dpbAddrHi[i] = addr;
         dynamic_dpb_t2->dpbAddrLo[i] = addr >> 32;
      }
   }

   for (int i = 0; i < frame_info->referenceSlotCount; i++) {
      int32_t slot_idx = frame_info->pReferenceSlots[i].slotIndex;
      assert(slot_idx >= 0 && slot_idx < 16);
      struct radv_image_view *f_dpb_iv =
         radv_image_view_from_handle(frame_info->pReferenceSlots[i].pPictureResource->imageViewBinding);
      assert(f_dpb_iv != NULL);
      struct radv_image *dpb_img = f_dpb_iv->image;
      int f_dpb_array_idx = 0;
      if (f_dpb_iv->vk.view_type == VK_IMAGE_VIEW_TYPE_2D_ARRAY)
         f_dpb_array_idx = frame_info->pReferenceSlots[i].pPictureResource->baseArrayLayer;

      radv_cs_add_buffer(device->ws, cmd_buffer->cs, dpb_img->bindings[0].bo);
      addr = radv_buffer_get_va(dpb_img->bindings[0].bo) + dpb_img->bindings[0].offset;
      addr += f_dpb_array_idx * (dpb_img->planes[0].surface.u.gfx9.surf_slice_size + dpb_img->planes[1].surface.u.gfx9.surf_slice_size);
      dynamic_dpb_t2->dpbAddrLo[i] = addr;
      dynamic_dpb_t2->dpbAddrHi[i] = addr >> 32;

      ++dynamic_dpb_t2->dpbArraySize;
   }

   radv_cs_add_buffer(device->ws, cmd_buffer->cs, dpb->bindings[0].bo);
   addr = radv_buffer_get_va(dpb->bindings[0].bo) + dpb->bindings[0].offset;
   addr += dpb_array_idx * (dpb->planes[0].surface.u.gfx9.surf_slice_size + dpb->planes[1].surface.u.gfx9.surf_slice_size);
   dynamic_dpb_t2->dpbCurrLo = addr;
   dynamic_dpb_t2->dpbCurrHi = addr >> 32;

   decode->decode_flags = 1;
   dynamic_dpb_t2->dpbConfigFlags = 0;
   dynamic_dpb_t2->dpbLumaPitch = luma->surface.u.gfx9.surf_pitch;
   dynamic_dpb_t2->dpbLumaAlignedHeight = luma->surface.u.gfx9.surf_height;
   dynamic_dpb_t2->dpbLumaAlignedSize = luma->surface.u.gfx9.surf_slice_size;

   dynamic_dpb_t2->dpbChromaPitch = chroma->surface.u.gfx9.surf_pitch;
   dynamic_dpb_t2->dpbChromaAlignedHeight = chroma->surface.u.gfx9.surf_height;
   dynamic_dpb_t2->dpbChromaAlignedSize = chroma->surface.u.gfx9.surf_slice_size;

   return true;
}

static struct ruvd_h264
get_uvd_h264_msg(struct radv_video_session *vid, struct radv_video_session_params *params,
                 const struct VkVideoDecodeInfoKHR *frame_info, uint32_t *slice_offset, uint32_t *width_in_samples,
                 uint32_t *height_in_samples, void *it_ptr)
{
   struct ruvd_h264 result;
   const struct VkVideoDecodeH264PictureInfoKHR *h264_pic_info =
      vk_find_struct_const(frame_info->pNext, VIDEO_DECODE_H264_PICTURE_INFO_KHR);

   *slice_offset = h264_pic_info->pSliceOffsets[0];

   memset(&result, 0, sizeof(result));

   const StdVideoH264SequenceParameterSet *sps =
      vk_video_find_h264_dec_std_sps(&params->vk, h264_pic_info->pStdPictureInfo->seq_parameter_set_id);
   switch (sps->profile_idc) {
   case STD_VIDEO_H264_PROFILE_IDC_BASELINE:
      result.profile = RUVD_H264_PROFILE_BASELINE;
      break;
   case STD_VIDEO_H264_PROFILE_IDC_MAIN:
      result.profile = RUVD_H264_PROFILE_MAIN;
      break;
   case STD_VIDEO_H264_PROFILE_IDC_HIGH:
      result.profile = RUVD_H264_PROFILE_HIGH;
      break;
   default:
      fprintf(stderr, "UNSUPPORTED CODEC %d\n", sps->profile_idc);
      result.profile = RUVD_H264_PROFILE_MAIN;
      break;
   }

   *width_in_samples = (sps->pic_width_in_mbs_minus1 + 1) * 16;
   *height_in_samples = (sps->pic_height_in_map_units_minus1 + 1) * 16;
   if (!sps->flags.frame_mbs_only_flag)
      *height_in_samples *= 2;
   result.level = get_h264_level(sps->level_idc);

   result.sps_info_flags = 0;

   result.sps_info_flags |= sps->flags.direct_8x8_inference_flag << 0;
   result.sps_info_flags |= sps->flags.mb_adaptive_frame_field_flag << 1;
   result.sps_info_flags |= sps->flags.frame_mbs_only_flag << 2;
   result.sps_info_flags |= sps->flags.delta_pic_order_always_zero_flag << 3;
   result.sps_info_flags |= 1 << RDECODE_SPS_INFO_H264_EXTENSION_SUPPORT_FLAG_SHIFT;

   result.bit_depth_luma_minus8 = sps->bit_depth_luma_minus8;
   result.bit_depth_chroma_minus8 = sps->bit_depth_chroma_minus8;
   result.log2_max_frame_num_minus4 = sps->log2_max_frame_num_minus4;
   result.pic_order_cnt_type = sps->pic_order_cnt_type;
   result.log2_max_pic_order_cnt_lsb_minus4 = sps->log2_max_pic_order_cnt_lsb_minus4;

   result.chroma_format = sps->chroma_format_idc;

   const StdVideoH264PictureParameterSet *pps =
      vk_video_find_h264_dec_std_pps(&params->vk, h264_pic_info->pStdPictureInfo->pic_parameter_set_id);
   result.pps_info_flags = 0;
   result.pps_info_flags |= pps->flags.transform_8x8_mode_flag << 0;
   result.pps_info_flags |= pps->flags.redundant_pic_cnt_present_flag << 1;
   result.pps_info_flags |= pps->flags.constrained_intra_pred_flag << 2;
   result.pps_info_flags |= pps->flags.deblocking_filter_control_present_flag << 3;
   result.pps_info_flags |= pps->weighted_bipred_idc << 4;
   result.pps_info_flags |= pps->flags.weighted_pred_flag << 6;
   result.pps_info_flags |= pps->flags.bottom_field_pic_order_in_frame_present_flag << 7;
   result.pps_info_flags |= pps->flags.entropy_coding_mode_flag << 8;

   result.pic_init_qp_minus26 = pps->pic_init_qp_minus26;
   result.chroma_qp_index_offset = pps->chroma_qp_index_offset;
   result.second_chroma_qp_index_offset = pps->second_chroma_qp_index_offset;

   StdVideoH264ScalingLists scaling_lists;
   vk_video_derive_h264_scaling_list(sps, pps, &scaling_lists);
   update_h264_scaling(result.scaling_list_4x4, result.scaling_list_8x8, &scaling_lists);

   memset(it_ptr, 0, IT_SCALING_TABLE_SIZE);
   memcpy(it_ptr, result.scaling_list_4x4, 6 * 16);
   memcpy((char *)it_ptr + 96, result.scaling_list_8x8, 2 * 64);

   result.num_ref_idx_l0_active_minus1 = pps->num_ref_idx_l0_default_active_minus1;
   result.num_ref_idx_l1_active_minus1 = pps->num_ref_idx_l1_default_active_minus1;

   result.curr_field_order_cnt_list[0] = h264_pic_info->pStdPictureInfo->PicOrderCnt[0];
   result.curr_field_order_cnt_list[1] = h264_pic_info->pStdPictureInfo->PicOrderCnt[1];

   result.frame_num = h264_pic_info->pStdPictureInfo->frame_num;

   result.num_ref_frames = sps->max_num_ref_frames;
   memset(result.ref_frame_list, 0xff, sizeof(unsigned char) * 16);
   memset(result.frame_num_list, 0, sizeof(unsigned int) * 16);
   for (unsigned i = 0; i < frame_info->referenceSlotCount; i++) {
      int idx = frame_info->pReferenceSlots[i].slotIndex;
      const struct VkVideoDecodeH264DpbSlotInfoKHR *dpb_slot =
         vk_find_struct_const(frame_info->pReferenceSlots[i].pNext, VIDEO_DECODE_H264_DPB_SLOT_INFO_KHR);

      result.frame_num_list[i] = dpb_slot->pStdReferenceInfo->FrameNum;
      result.field_order_cnt_list[i][0] = dpb_slot->pStdReferenceInfo->PicOrderCnt[0];
      result.field_order_cnt_list[i][1] = dpb_slot->pStdReferenceInfo->PicOrderCnt[1];

      result.ref_frame_list[i] = idx;

      if (dpb_slot->pStdReferenceInfo->flags.used_for_long_term_reference)
         result.ref_frame_list[i] |= 0x80;
   }
   result.curr_pic_ref_frame_num = frame_info->referenceSlotCount;
   result.decoded_pic_idx = frame_info->pSetupReferenceSlot->slotIndex;

   return result;
}

static struct ruvd_h265
get_uvd_h265_msg(struct radv_device *device, struct radv_video_session *vid, struct radv_video_session_params *params,
                 const struct VkVideoDecodeInfoKHR *frame_info, uint32_t *width_in_samples,
                 uint32_t *height_in_samples, void *it_ptr)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct ruvd_h265 result;
   int i, j;
   const struct VkVideoDecodeH265PictureInfoKHR *h265_pic_info =
      vk_find_struct_const(frame_info->pNext, VIDEO_DECODE_H265_PICTURE_INFO_KHR);

   memset(&result, 0, sizeof(result));

   const StdVideoH265SequenceParameterSet *sps =
      vk_video_find_h265_dec_std_sps(&params->vk, h265_pic_info->pStdPictureInfo->pps_seq_parameter_set_id);
   const StdVideoH265PictureParameterSet *pps =
      vk_video_find_h265_dec_std_pps(&params->vk, h265_pic_info->pStdPictureInfo->pps_pic_parameter_set_id);

   result.sps_info_flags = 0;
   result.sps_info_flags |= sps->flags.scaling_list_enabled_flag << 0;
   result.sps_info_flags |= sps->flags.amp_enabled_flag << 1;
   result.sps_info_flags |= sps->flags.sample_adaptive_offset_enabled_flag << 2;
   result.sps_info_flags |= sps->flags.pcm_enabled_flag << 3;
   result.sps_info_flags |= sps->flags.pcm_loop_filter_disabled_flag << 4;
   result.sps_info_flags |= sps->flags.long_term_ref_pics_present_flag << 5;
   result.sps_info_flags |= sps->flags.sps_temporal_mvp_enabled_flag << 6;
   result.sps_info_flags |= sps->flags.strong_intra_smoothing_enabled_flag << 7;
   result.sps_info_flags |= sps->flags.separate_colour_plane_flag << 8;

   if (pdev->info.family == CHIP_CARRIZO)
      result.sps_info_flags |= 1 << 9;

   *width_in_samples = sps->pic_width_in_luma_samples;
   *height_in_samples = sps->pic_height_in_luma_samples;
   result.chroma_format = sps->chroma_format_idc;
   result.bit_depth_luma_minus8 = sps->bit_depth_luma_minus8;
   result.bit_depth_chroma_minus8 = sps->bit_depth_chroma_minus8;
   result.log2_max_pic_order_cnt_lsb_minus4 = sps->log2_max_pic_order_cnt_lsb_minus4;
   result.sps_max_dec_pic_buffering_minus1 =
      sps->pDecPicBufMgr->max_dec_pic_buffering_minus1[sps->sps_max_sub_layers_minus1];
   result.log2_min_luma_coding_block_size_minus3 = sps->log2_min_luma_coding_block_size_minus3;
   result.log2_diff_max_min_luma_coding_block_size = sps->log2_diff_max_min_luma_coding_block_size;
   result.log2_min_transform_block_size_minus2 = sps->log2_min_luma_transform_block_size_minus2;
   result.log2_diff_max_min_transform_block_size = sps->log2_diff_max_min_luma_transform_block_size;
   result.max_transform_hierarchy_depth_inter = sps->max_transform_hierarchy_depth_inter;
   result.max_transform_hierarchy_depth_intra = sps->max_transform_hierarchy_depth_intra;
   if (sps->flags.pcm_enabled_flag) {
      result.pcm_sample_bit_depth_luma_minus1 = sps->pcm_sample_bit_depth_luma_minus1;
      result.pcm_sample_bit_depth_chroma_minus1 = sps->pcm_sample_bit_depth_chroma_minus1;
      result.log2_min_pcm_luma_coding_block_size_minus3 = sps->log2_min_pcm_luma_coding_block_size_minus3;
      result.log2_diff_max_min_pcm_luma_coding_block_size = sps->log2_diff_max_min_pcm_luma_coding_block_size;
   }
   result.num_short_term_ref_pic_sets = sps->num_short_term_ref_pic_sets;

   result.pps_info_flags = 0;
   result.pps_info_flags |= pps->flags.dependent_slice_segments_enabled_flag << 0;
   result.pps_info_flags |= pps->flags.output_flag_present_flag << 1;
   result.pps_info_flags |= pps->flags.sign_data_hiding_enabled_flag << 2;
   result.pps_info_flags |= pps->flags.cabac_init_present_flag << 3;
   result.pps_info_flags |= pps->flags.constrained_intra_pred_flag << 4;
   result.pps_info_flags |= pps->flags.transform_skip_enabled_flag << 5;
   result.pps_info_flags |= pps->flags.cu_qp_delta_enabled_flag << 6;
   result.pps_info_flags |= pps->flags.pps_slice_chroma_qp_offsets_present_flag << 7;
   result.pps_info_flags |= pps->flags.weighted_pred_flag << 8;
   result.pps_info_flags |= pps->flags.weighted_bipred_flag << 9;
   result.pps_info_flags |= pps->flags.transquant_bypass_enabled_flag << 10;
   result.pps_info_flags |= pps->flags.tiles_enabled_flag << 11;
   result.pps_info_flags |= pps->flags.entropy_coding_sync_enabled_flag << 12;
   result.pps_info_flags |= pps->flags.uniform_spacing_flag << 13;
   result.pps_info_flags |= pps->flags.loop_filter_across_tiles_enabled_flag << 14;
   result.pps_info_flags |= pps->flags.pps_loop_filter_across_slices_enabled_flag << 15;
   result.pps_info_flags |= pps->flags.deblocking_filter_override_enabled_flag << 16;
   result.pps_info_flags |= pps->flags.pps_deblocking_filter_disabled_flag << 17;
   result.pps_info_flags |= pps->flags.lists_modification_present_flag << 18;
   result.pps_info_flags |= pps->flags.slice_segment_header_extension_present_flag << 19;

   result.num_extra_slice_header_bits = pps->num_extra_slice_header_bits;
   result.num_long_term_ref_pic_sps = sps->num_long_term_ref_pics_sps;
   result.num_ref_idx_l0_default_active_minus1 = pps->num_ref_idx_l0_default_active_minus1;
   result.num_ref_idx_l1_default_active_minus1 = pps->num_ref_idx_l1_default_active_minus1;
   result.pps_cb_qp_offset = pps->pps_cb_qp_offset;
   result.pps_cr_qp_offset = pps->pps_cr_qp_offset;
   result.pps_beta_offset_div2 = pps->pps_beta_offset_div2;
   result.pps_tc_offset_div2 = pps->pps_tc_offset_div2;
   result.diff_cu_qp_delta_depth = pps->diff_cu_qp_delta_depth;
   result.num_tile_columns_minus1 = pps->num_tile_columns_minus1;
   result.num_tile_rows_minus1 = pps->num_tile_rows_minus1;
   result.log2_parallel_merge_level_minus2 = pps->log2_parallel_merge_level_minus2;
   result.init_qp_minus26 = pps->init_qp_minus26;

   for (i = 0; i < 19; ++i)
      result.column_width_minus1[i] = pps->column_width_minus1[i];

   for (i = 0; i < 21; ++i)
      result.row_height_minus1[i] = pps->row_height_minus1[i];

   result.num_delta_pocs_ref_rps_idx = h265_pic_info->pStdPictureInfo->NumDeltaPocsOfRefRpsIdx;
   result.curr_poc = h265_pic_info->pStdPictureInfo->PicOrderCntVal;

   uint8_t idxs[16];
   memset(result.poc_list, 0, 16 * sizeof(int));
   memset(result.ref_pic_list, 0x7f, 16);
   memset(idxs, 0xff, 16);
   for (i = 0; i < frame_info->referenceSlotCount; i++) {
      const struct VkVideoDecodeH265DpbSlotInfoKHR *dpb_slot =
         vk_find_struct_const(frame_info->pReferenceSlots[i].pNext, VIDEO_DECODE_H265_DPB_SLOT_INFO_KHR);
      int idx = frame_info->pReferenceSlots[i].slotIndex;
      result.poc_list[i] = dpb_slot->pStdReferenceInfo->PicOrderCntVal;
      result.ref_pic_list[i] = idx;
      idxs[idx] = i;
   }
   result.curr_idx = frame_info->pSetupReferenceSlot->slotIndex;

#define IDXS(x) ((x) == 0xff ? 0xff : idxs[(x)])
   for (i = 0; i < 8; ++i)
      result.ref_pic_set_st_curr_before[i] = IDXS(h265_pic_info->pStdPictureInfo->RefPicSetStCurrBefore[i]);

   for (i = 0; i < 8; ++i)
      result.ref_pic_set_st_curr_after[i] = IDXS(h265_pic_info->pStdPictureInfo->RefPicSetStCurrAfter[i]);

   for (i = 0; i < 8; ++i)
      result.ref_pic_set_lt_curr[i] = IDXS(h265_pic_info->pStdPictureInfo->RefPicSetLtCurr[i]);

   const StdVideoH265ScalingLists *scaling_lists = NULL;
   if (pps->flags.pps_scaling_list_data_present_flag)
      scaling_lists = pps->pScalingLists;
   else if (sps->flags.sps_scaling_list_data_present_flag)
      scaling_lists = sps->pScalingLists;

   update_h265_scaling(it_ptr, scaling_lists);
   if (scaling_lists) {
      for (i = 0; i < STD_VIDEO_H265_SCALING_LIST_16X16_NUM_LISTS; ++i)
         result.ucScalingListDCCoefSizeID2[i] = scaling_lists->ScalingListDCCoef16x16[i];

      for (i = 0; i < STD_VIDEO_H265_SCALING_LIST_32X32_NUM_LISTS; ++i)
         result.ucScalingListDCCoefSizeID3[i] = scaling_lists->ScalingListDCCoef32x32[i];
   }

   for (i = 0; i < 2; i++) {
      for (j = 0; j < 15; j++)
         result.direct_reflist[i][j] = 0xff;
   }

   if (vid->vk.h265.profile_idc == STD_VIDEO_H265_PROFILE_IDC_MAIN_10) {
      if (vid->vk.picture_format == VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16) {
         result.p010_mode = 1;
         result.msb_mode = 1;
      } else {
         result.p010_mode = 0;
         result.luma_10to8 = 5;
         result.chroma_10to8 = 5;
         result.sclr_luma10to8 = 4;
         result.sclr_chroma10to8 = 4;
      }
   }

   return result;
}

static unsigned
texture_offset_legacy(struct radeon_surf *surface, unsigned layer)
{
   return (uint64_t)surface->u.legacy.level[0].offset_256B * 256 +
          layer * (uint64_t)surface->u.legacy.level[0].slice_size_dw * 4;
}

static bool
ruvd_dec_message_decode(struct radv_device *device, struct radv_video_session *vid,
                        struct radv_video_session_params *params, void *ptr, void *it_ptr, uint32_t *slice_offset,
                        const struct VkVideoDecodeInfoKHR *frame_info)
{
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct ruvd_msg *msg = ptr;
   struct radv_image_view *dst_iv = radv_image_view_from_handle(frame_info->dstPictureResource.imageViewBinding);
   struct radv_image *img = dst_iv->image;
   struct radv_image_plane *luma = &img->planes[0];
   struct radv_image_plane *chroma = &img->planes[1];
   struct radv_image_view *dpb_iv =
      radv_image_view_from_handle(frame_info->pSetupReferenceSlot->pPictureResource->imageViewBinding);
   struct radv_image *dpb = dpb_iv->image;

   memset(msg, 0, sizeof(struct ruvd_msg));
   msg->size = sizeof(*msg);
   msg->msg_type = RUVD_MSG_DECODE;
   msg->stream_handle = vid->stream_handle;
   msg->status_report_feedback_number = vid->dbg_frame_cnt++;

   msg->body.decode.stream_type = vid->stream_type;
   msg->body.decode.decode_flags = 0x1;
   msg->body.decode.width_in_samples = frame_info->dstPictureResource.codedExtent.width;
   msg->body.decode.height_in_samples = frame_info->dstPictureResource.codedExtent.height;

   msg->body.decode.dpb_size = (vid->dpb_type != DPB_DYNAMIC_TIER_2) ? dpb->size : 0;
   msg->body.decode.bsd_size = frame_info->srcBufferRange;
   msg->body.decode.db_pitch = align(frame_info->dstPictureResource.codedExtent.width, vid->db_alignment);

   if (vid->stream_type == RUVD_CODEC_H264_PERF && pdev->info.family >= CHIP_POLARIS10)
      msg->body.decode.dpb_reserved = vid->ctx.size;

   *slice_offset = 0;
   switch (vid->vk.op) {
   case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR: {
      msg->body.decode.codec.h264 =
         get_uvd_h264_msg(vid, params, frame_info, slice_offset, &msg->body.decode.width_in_samples,
                          &msg->body.decode.height_in_samples, it_ptr);
      break;
   }
   case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR: {
      msg->body.decode.codec.h265 = get_uvd_h265_msg(device, vid, params, frame_info,
                                                     &msg->body.decode.width_in_samples,
                                                     &msg->body.decode.height_in_samples,
                                                     it_ptr);

      if (vid->ctx.mem)
         msg->body.decode.dpb_reserved = vid->ctx.size;
      break;
   }
   default:
      return false;
   }

   msg->body.decode.dt_field_mode = false;

   if (pdev->info.gfx_level >= GFX9) {
      msg->body.decode.dt_pitch = luma->surface.u.gfx9.surf_pitch * luma->surface.blk_w;
      msg->body.decode.dt_tiling_mode = RUVD_TILE_LINEAR;
      msg->body.decode.dt_array_mode = RUVD_ARRAY_MODE_LINEAR;
      msg->body.decode.dt_luma_top_offset = luma->surface.u.gfx9.surf_offset;
      msg->body.decode.dt_chroma_top_offset = chroma->surface.u.gfx9.surf_offset;
      if (msg->body.decode.dt_field_mode) {
         msg->body.decode.dt_luma_bottom_offset =
            luma->surface.u.gfx9.surf_offset + luma->surface.u.gfx9.surf_slice_size;
         msg->body.decode.dt_chroma_bottom_offset =
            chroma->surface.u.gfx9.surf_offset + chroma->surface.u.gfx9.surf_slice_size;
      } else {
         msg->body.decode.dt_luma_bottom_offset = msg->body.decode.dt_luma_top_offset;
         msg->body.decode.dt_chroma_bottom_offset = msg->body.decode.dt_chroma_top_offset;
      }
      msg->body.decode.dt_surf_tile_config = 0;
   } else {
      msg->body.decode.dt_pitch = luma->surface.u.legacy.level[0].nblk_x * luma->surface.blk_w;
      switch (luma->surface.u.legacy.level[0].mode) {
      case RADEON_SURF_MODE_LINEAR_ALIGNED:
         msg->body.decode.dt_tiling_mode = RUVD_TILE_LINEAR;
         msg->body.decode.dt_array_mode = RUVD_ARRAY_MODE_LINEAR;
         break;
      case RADEON_SURF_MODE_1D:
         msg->body.decode.dt_tiling_mode = RUVD_TILE_8X8;
         msg->body.decode.dt_array_mode = RUVD_ARRAY_MODE_1D_THIN;
         break;
      case RADEON_SURF_MODE_2D:
         msg->body.decode.dt_tiling_mode = RUVD_TILE_8X8;
         msg->body.decode.dt_array_mode = RUVD_ARRAY_MODE_2D_THIN;
         break;
      default:
         assert(0);
         break;
      }

      msg->body.decode.dt_luma_top_offset = texture_offset_legacy(&luma->surface, 0);
      if (chroma)
         msg->body.decode.dt_chroma_top_offset = texture_offset_legacy(&chroma->surface, 0);
      if (msg->body.decode.dt_field_mode) {
         msg->body.decode.dt_luma_bottom_offset = texture_offset_legacy(&luma->surface, 1);
         if (chroma)
            msg->body.decode.dt_chroma_bottom_offset = texture_offset_legacy(&chroma->surface, 1);
      } else {
         msg->body.decode.dt_luma_bottom_offset = msg->body.decode.dt_luma_top_offset;
         msg->body.decode.dt_chroma_bottom_offset = msg->body.decode.dt_chroma_top_offset;
      }

      if (chroma) {
         assert(luma->surface.u.legacy.bankw == chroma->surface.u.legacy.bankw);
         assert(luma->surface.u.legacy.bankh == chroma->surface.u.legacy.bankh);
         assert(luma->surface.u.legacy.mtilea == chroma->surface.u.legacy.mtilea);
      }

      msg->body.decode.dt_surf_tile_config |= RUVD_BANK_WIDTH(util_logbase2(luma->surface.u.legacy.bankw));
      msg->body.decode.dt_surf_tile_config |= RUVD_BANK_HEIGHT(util_logbase2(luma->surface.u.legacy.bankh));
      msg->body.decode.dt_surf_tile_config |=
         RUVD_MACRO_TILE_ASPECT_RATIO(util_logbase2(luma->surface.u.legacy.mtilea));
   }

   if (pdev->info.family >= CHIP_STONEY)
      msg->body.decode.dt_wa_chroma_top_offset = msg->body.decode.dt_pitch / 2;

   msg->body.decode.db_surf_tile_config = msg->body.decode.dt_surf_tile_config;
   msg->body.decode.extension_support = 0x1;

   return true;
}

static void
ruvd_dec_message_create(struct radv_video_session *vid, void *ptr)
{
   struct ruvd_msg *msg = ptr;

   memset(ptr, 0, sizeof(*msg));
   msg->size = sizeof(*msg);
   msg->msg_type = RUVD_MSG_CREATE;
   msg->stream_handle = vid->stream_handle;
   msg->body.create.stream_type = vid->stream_type;
   msg->body.create.width_in_samples = vid->vk.max_coded.width;
   msg->body.create.height_in_samples = vid->vk.max_coded.height;
}

VKAPI_ATTR void VKAPI_CALL
radv_CmdBeginVideoCodingKHR(VkCommandBuffer commandBuffer, const VkVideoBeginCodingInfoKHR *pBeginInfo)
{
   VK_FROM_HANDLE(radv_cmd_buffer, cmd_buffer, commandBuffer);
   VK_FROM_HANDLE(radv_video_session, vid, pBeginInfo->videoSession);
   VK_FROM_HANDLE(radv_video_session_params, params, pBeginInfo->videoSessionParameters);

   cmd_buffer->video.vid = vid;
   cmd_buffer->video.params = params;

   if (vid->encode)
      radv_video_enc_begin_coding(cmd_buffer);
}

static void
radv_vcn_cmd_reset(struct radv_cmd_buffer *cmd_buffer)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radv_video_session *vid = cmd_buffer->video.vid;
   uint32_t size = sizeof(rvcn_dec_message_header_t) + sizeof(rvcn_dec_message_create_t);

   void *ptr;
   uint32_t out_offset;

   if (vid->stream_type == RDECODE_CODEC_AV1) {
      unsigned frame_ctxt_size = pdev->av1_version == RDECODE_AV1_VER_0
                                    ? align(sizeof(rvcn_av1_frame_context_t), 2048)
                                    : align(sizeof(rvcn_av1_vcn4_frame_context_t), 2048);

      uint8_t *ctxptr = radv_buffer_map(device->ws, vid->ctx.mem->bo);
      ctxptr += vid->ctx.offset;
      if (pdev->av1_version == RDECODE_AV1_VER_0) {
         for (unsigned i = 0; i < 4; ++i) {
            rvcn_av1_init_mode_probs((void *)(ctxptr + i * frame_ctxt_size));
            rvcn_av1_init_mv_probs((void *)(ctxptr + i * frame_ctxt_size));
            rvcn_av1_default_coef_probs((void *)(ctxptr + i * frame_ctxt_size), i);
         }
      } else {
         for (unsigned i = 0; i < 4; ++i) {
            rvcn_vcn4_init_mode_probs((void *)(ctxptr + i * frame_ctxt_size));
            rvcn_vcn4_av1_init_mv_probs((void *)(ctxptr + i * frame_ctxt_size));
            rvcn_vcn4_av1_default_coef_probs((void *)(ctxptr + i * frame_ctxt_size), i);
         }
      }
      device->ws->buffer_unmap(device->ws, vid->ctx.mem->bo, false);
   }
   radv_vid_buffer_upload_alloc(cmd_buffer, size, &out_offset, &ptr);

   if (pdev->vid_decode_ip == AMD_IP_VCN_UNIFIED)
      radv_vcn_sq_start(cmd_buffer);

   rvcn_dec_message_create(vid, ptr, size);
   send_cmd(cmd_buffer, RDECODE_CMD_SESSION_CONTEXT_BUFFER, vid->sessionctx.mem->bo, vid->sessionctx.offset);
   send_cmd(cmd_buffer, RDECODE_CMD_MSG_BUFFER, cmd_buffer->upload.upload_bo, out_offset);
   /* pad out the IB to the 16 dword boundary - otherwise the fw seems to be unhappy */

   if (pdev->vid_decode_ip != AMD_IP_VCN_UNIFIED) {
      radeon_check_space(device->ws, cmd_buffer->cs, 8);
      for (unsigned i = 0; i < 8; i++)
         radeon_emit(cmd_buffer->cs, 0x81ff);
   } else
      radv_vcn_sq_tail(cmd_buffer->cs, &cmd_buffer->video.sq);
}

static void
radv_uvd_cmd_reset(struct radv_cmd_buffer *cmd_buffer)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   struct radv_video_session *vid = cmd_buffer->video.vid;
   uint32_t size = sizeof(struct ruvd_msg);
   void *ptr;
   uint32_t out_offset;
   radv_vid_buffer_upload_alloc(cmd_buffer, size, &out_offset, &ptr);

   ruvd_dec_message_create(vid, ptr);
   if (vid->sessionctx.mem)
      send_cmd(cmd_buffer, RDECODE_CMD_SESSION_CONTEXT_BUFFER, vid->sessionctx.mem->bo, vid->sessionctx.offset);
   send_cmd(cmd_buffer, RDECODE_CMD_MSG_BUFFER, cmd_buffer->upload.upload_bo, out_offset);

   /* pad out the IB to the 16 dword boundary - otherwise the fw seems to be unhappy */
   int padsize = vid->sessionctx.mem ? 4 : 6;
   radeon_check_space(device->ws, cmd_buffer->cs, padsize);
   for (unsigned i = 0; i < padsize; i++)
      radeon_emit(cmd_buffer->cs, PKT2_NOP_PAD);
}

VKAPI_ATTR void VKAPI_CALL
radv_CmdControlVideoCodingKHR(VkCommandBuffer commandBuffer, const VkVideoCodingControlInfoKHR *pCodingControlInfo)
{
   VK_FROM_HANDLE(radv_cmd_buffer, cmd_buffer, commandBuffer);
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   struct radv_physical_device *pdev = radv_device_physical(device);

   if (cmd_buffer->video.vid->encode) {
      radv_video_enc_control_video_coding(cmd_buffer, pCodingControlInfo);
      return;
   }
   if (pCodingControlInfo->flags & VK_VIDEO_CODING_CONTROL_RESET_BIT_KHR) {
      if (radv_has_uvd(pdev))
         radv_uvd_cmd_reset(cmd_buffer);
      else
         radv_vcn_cmd_reset(cmd_buffer);
   }
}

VKAPI_ATTR void VKAPI_CALL
radv_CmdEndVideoCodingKHR(VkCommandBuffer commandBuffer, const VkVideoEndCodingInfoKHR *pEndCodingInfo)
{
   VK_FROM_HANDLE(radv_cmd_buffer, cmd_buffer, commandBuffer);

   if (cmd_buffer->video.vid->encode) {
      radv_video_enc_end_coding(cmd_buffer);
      return;
   }
}

static void
radv_uvd_decode_video(struct radv_cmd_buffer *cmd_buffer, const VkVideoDecodeInfoKHR *frame_info)
{
   VK_FROM_HANDLE(radv_buffer, src_buffer, frame_info->srcBuffer);
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radv_video_session *vid = cmd_buffer->video.vid;
   struct radv_video_session_params *params = cmd_buffer->video.params;
   unsigned size = sizeof(struct ruvd_msg);
   void *ptr, *fb_ptr, *it_probs_ptr = NULL;
   uint32_t out_offset, fb_offset, it_probs_offset = 0;
   struct radeon_winsys_bo *msg_bo, *fb_bo, *it_probs_bo = NULL;
   unsigned fb_size = (pdev->info.family == CHIP_TONGA) ? FB_BUFFER_SIZE_TONGA : FB_BUFFER_SIZE;

   radv_vid_buffer_upload_alloc(cmd_buffer, fb_size, &fb_offset, &fb_ptr);
   fb_bo = cmd_buffer->upload.upload_bo;
   if (have_it(vid)) {
      radv_vid_buffer_upload_alloc(cmd_buffer, IT_SCALING_TABLE_SIZE, &it_probs_offset, &it_probs_ptr);
      it_probs_bo = cmd_buffer->upload.upload_bo;
   }

   radv_vid_buffer_upload_alloc(cmd_buffer, size, &out_offset, &ptr);
   msg_bo = cmd_buffer->upload.upload_bo;

   uint32_t slice_offset;
   ruvd_dec_message_decode(device, vid, params, ptr, it_probs_ptr, &slice_offset, frame_info);
   rvcn_dec_message_feedback(fb_ptr);
   if (vid->sessionctx.mem)
      send_cmd(cmd_buffer, RDECODE_CMD_SESSION_CONTEXT_BUFFER, vid->sessionctx.mem->bo, vid->sessionctx.offset);
   send_cmd(cmd_buffer, RDECODE_CMD_MSG_BUFFER, msg_bo, out_offset);

   if (vid->dpb_type != DPB_DYNAMIC_TIER_2) {
      struct radv_image_view *dpb_iv =
         radv_image_view_from_handle(frame_info->pSetupReferenceSlot->pPictureResource->imageViewBinding);
      struct radv_image *dpb = dpb_iv->image;
      send_cmd(cmd_buffer, RDECODE_CMD_DPB_BUFFER, dpb->bindings[0].bo, dpb->bindings[0].offset);
   }

   if (vid->ctx.mem)
      send_cmd(cmd_buffer, RDECODE_CMD_CONTEXT_BUFFER, vid->ctx.mem->bo, vid->ctx.offset);

   send_cmd(cmd_buffer, RDECODE_CMD_BITSTREAM_BUFFER, src_buffer->bo,
            src_buffer->offset + frame_info->srcBufferOffset + slice_offset);

   struct radv_image_view *dst_iv = radv_image_view_from_handle(frame_info->dstPictureResource.imageViewBinding);
   struct radv_image *img = dst_iv->image;
   send_cmd(cmd_buffer, RDECODE_CMD_DECODING_TARGET_BUFFER, img->bindings[0].bo, img->bindings[0].offset);
   send_cmd(cmd_buffer, RDECODE_CMD_FEEDBACK_BUFFER, fb_bo, fb_offset);
   if (have_it(vid))
      send_cmd(cmd_buffer, RDECODE_CMD_IT_SCALING_TABLE_BUFFER, it_probs_bo, it_probs_offset);

   radeon_check_space(device->ws, cmd_buffer->cs, 2);
   set_reg(cmd_buffer, pdev->vid_dec_reg.cntl, 1);
}

static void
radv_vcn_decode_video(struct radv_cmd_buffer *cmd_buffer, const VkVideoDecodeInfoKHR *frame_info)
{
   VK_FROM_HANDLE(radv_buffer, src_buffer, frame_info->srcBuffer);
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radv_video_session *vid = cmd_buffer->video.vid;
   struct radv_video_session_params *params = cmd_buffer->video.params;
   unsigned size = 0;
   void *ptr, *fb_ptr, *it_probs_ptr = NULL;
   uint32_t out_offset, fb_offset, it_probs_offset = 0;
   struct radeon_winsys_bo *msg_bo, *fb_bo, *it_probs_bo = NULL;

   size += sizeof(rvcn_dec_message_header_t); /* header */
   size += sizeof(rvcn_dec_message_index_t);  /* codec */
   if (vid->dpb_type == DPB_DYNAMIC_TIER_2) {
      size += sizeof(rvcn_dec_message_index_t);
      size += sizeof(rvcn_dec_message_dynamic_dpb_t2_t);
   }
   size += sizeof(rvcn_dec_message_decode_t); /* decode */
   switch (vid->vk.op) {
   case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
      size += sizeof(rvcn_dec_message_avc_t);
      break;
   case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
      size += sizeof(rvcn_dec_message_hevc_t);
      break;
   case VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR:
      size += sizeof(rvcn_dec_message_av1_t);
      break;
   default:
      unreachable("unsupported codec.");
   }

   radv_vid_buffer_upload_alloc(cmd_buffer, FB_BUFFER_SIZE, &fb_offset, &fb_ptr);
   fb_bo = cmd_buffer->upload.upload_bo;
   if (have_it(vid)) {
      radv_vid_buffer_upload_alloc(cmd_buffer, IT_SCALING_TABLE_SIZE, &it_probs_offset, &it_probs_ptr);
      it_probs_bo = cmd_buffer->upload.upload_bo;
   } else if (have_probs(vid)) {
      radv_vid_buffer_upload_alloc(cmd_buffer, sizeof(rvcn_dec_av1_segment_fg_t), &it_probs_offset, &it_probs_ptr);
      it_probs_bo = cmd_buffer->upload.upload_bo;
   }

   radv_vid_buffer_upload_alloc(cmd_buffer, size, &out_offset, &ptr);
   msg_bo = cmd_buffer->upload.upload_bo;

   if (pdev->vid_decode_ip == AMD_IP_VCN_UNIFIED)
      radv_vcn_sq_start(cmd_buffer);

   uint32_t slice_offset;
   rvcn_dec_message_decode(cmd_buffer, vid, params, ptr, it_probs_ptr, &slice_offset, frame_info);
   rvcn_dec_message_feedback(fb_ptr);
   send_cmd(cmd_buffer, RDECODE_CMD_SESSION_CONTEXT_BUFFER, vid->sessionctx.mem->bo, vid->sessionctx.offset);
   send_cmd(cmd_buffer, RDECODE_CMD_MSG_BUFFER, msg_bo, out_offset);

   if (vid->dpb_type != DPB_DYNAMIC_TIER_2) {
      struct radv_image_view *dpb_iv =
         radv_image_view_from_handle(frame_info->pSetupReferenceSlot->pPictureResource->imageViewBinding);
      struct radv_image *dpb = dpb_iv->image;
      send_cmd(cmd_buffer, RDECODE_CMD_DPB_BUFFER, dpb->bindings[0].bo, dpb->bindings[0].offset);
   }

   if (vid->ctx.mem)
      send_cmd(cmd_buffer, RDECODE_CMD_CONTEXT_BUFFER, vid->ctx.mem->bo, vid->ctx.offset);

   send_cmd(cmd_buffer, RDECODE_CMD_BITSTREAM_BUFFER, src_buffer->bo,
            src_buffer->offset + frame_info->srcBufferOffset + slice_offset);

   struct radv_image_view *dst_iv = radv_image_view_from_handle(frame_info->dstPictureResource.imageViewBinding);
   struct radv_image *img = dst_iv->image;
   send_cmd(cmd_buffer, RDECODE_CMD_DECODING_TARGET_BUFFER, img->bindings[0].bo, img->bindings[0].offset);
   send_cmd(cmd_buffer, RDECODE_CMD_FEEDBACK_BUFFER, fb_bo, fb_offset);
   if (have_it(vid))
      send_cmd(cmd_buffer, RDECODE_CMD_IT_SCALING_TABLE_BUFFER, it_probs_bo, it_probs_offset);
   else if (have_probs(vid))
      send_cmd(cmd_buffer, RDECODE_CMD_PROB_TBL_BUFFER, it_probs_bo, it_probs_offset);

   if (pdev->vid_decode_ip != AMD_IP_VCN_UNIFIED) {
      radeon_check_space(device->ws, cmd_buffer->cs, 2);
      set_reg(cmd_buffer, pdev->vid_dec_reg.cntl, 1);
   } else
      radv_vcn_sq_tail(cmd_buffer->cs, &cmd_buffer->video.sq);
}

VKAPI_ATTR void VKAPI_CALL
radv_CmdDecodeVideoKHR(VkCommandBuffer commandBuffer, const VkVideoDecodeInfoKHR *frame_info)
{
   VK_FROM_HANDLE(radv_cmd_buffer, cmd_buffer, commandBuffer);
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   struct radv_physical_device *pdev = radv_device_physical(device);

   if (radv_has_uvd(pdev))
      radv_uvd_decode_video(cmd_buffer, frame_info);
   else
      radv_vcn_decode_video(cmd_buffer, frame_info);
}

void
radv_video_get_profile_alignments(struct radv_physical_device *pdev, const VkVideoProfileListInfoKHR *profile_list,
                                  uint32_t *width_align_out, uint32_t *height_align_out)
{
   vk_video_get_profile_alignments(profile_list, width_align_out, height_align_out);
   bool is_h265_main_10 = false;
   for (unsigned i = 0; i < profile_list->profileCount; i++) {
      if (profile_list->pProfiles[i].videoCodecOperation == VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR) {
         const struct VkVideoDecodeH265ProfileInfoKHR *h265_profile =
            vk_find_struct_const(profile_list->pProfiles[i].pNext, VIDEO_DECODE_H265_PROFILE_INFO_KHR);
         if (h265_profile->stdProfileIdc == STD_VIDEO_H265_PROFILE_IDC_MAIN_10)
            is_h265_main_10 = true;
      }
   }

   uint32_t db_alignment = radv_video_get_db_alignment(pdev, 64, is_h265_main_10);
   *width_align_out = MAX2(*width_align_out, db_alignment);
   *height_align_out = MAX2(*height_align_out, db_alignment);
}
