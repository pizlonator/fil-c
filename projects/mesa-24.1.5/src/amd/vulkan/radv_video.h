/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_VIDEO_H
#define RADV_VIDEO_H

#include "vk_video.h"

#include "ac_vcn.h"

#define VL_MACROBLOCK_WIDTH  16
#define VL_MACROBLOCK_HEIGHT 16

struct radv_physical_device;
struct rvcn_sq_var;
struct radv_cmd_buffer;

#define RADV_ENC_MAX_RATE_LAYER 4

struct radv_vid_mem {
   struct radv_device_memory *mem;
   VkDeviceSize offset;
   VkDeviceSize size;
};

struct radv_video_session {
   struct vk_video_session vk;

   uint32_t stream_handle;
   unsigned stream_type;
   bool interlaced;
   bool encode;
   enum { DPB_MAX_RES = 0, DPB_DYNAMIC_TIER_1, DPB_DYNAMIC_TIER_2 } dpb_type;
   unsigned db_alignment;

   struct radv_vid_mem sessionctx;
   struct radv_vid_mem ctx;

   unsigned dbg_frame_cnt;
   rvcn_enc_session_init_t enc_session;
   rvcn_enc_layer_control_t rc_layer_control;
   rvcn_enc_rate_ctl_layer_init_t rc_layer_init[RADV_ENC_MAX_RATE_LAYER];
   rvcn_enc_rate_ctl_per_picture_t rc_per_pic[RADV_ENC_MAX_RATE_LAYER];
   uint32_t enc_preset_mode;
   uint32_t enc_rate_control_method;
   uint32_t enc_vbv_buffer_level;
   bool enc_rate_control_default;
   bool enc_need_begin;
   bool enc_need_rate_control;
};

VK_DEFINE_NONDISP_HANDLE_CASTS(radv_video_session, vk.base, VkVideoSessionKHR, VK_OBJECT_TYPE_VIDEO_SESSION_KHR)

struct radv_video_session_params {
   struct vk_video_session_parameters vk;
};

VK_DEFINE_NONDISP_HANDLE_CASTS(radv_video_session_params, vk.base, VkVideoSessionParametersKHR,
                               VK_OBJECT_TYPE_VIDEO_SESSION_PARAMETERS_KHR)

void radv_init_physical_device_decoder(struct radv_physical_device *pdev);

void radv_video_get_profile_alignments(struct radv_physical_device *pdev, const VkVideoProfileListInfoKHR *profile_list,
                                       uint32_t *width_align_out, uint32_t *height_align_out);

void radv_vcn_sq_header(struct radeon_cmdbuf *cs, struct rvcn_sq_var *sq, bool enc);

void radv_vcn_sq_tail(struct radeon_cmdbuf *cs, struct rvcn_sq_var *sq);

void radv_init_physical_device_encoder(struct radv_physical_device *pdevice);
void radv_probe_video_encode(struct radv_physical_device *pdev);
void radv_video_enc_begin_coding(struct radv_cmd_buffer *cmd_buffer);
void radv_video_enc_end_coding(struct radv_cmd_buffer *cmd_buffer);
void radv_video_enc_control_video_coding(struct radv_cmd_buffer *cmd_buffer,
                                         const VkVideoCodingControlInfoKHR *pCodingControlInfo);
VkResult radv_video_get_encode_session_memory_requirements(struct radv_device *device, struct radv_video_session *vid,
                                                           uint32_t *pMemoryRequirementsCount,
                                                           VkVideoSessionMemoryRequirementsKHR *pMemoryRequirements);
void radv_video_patch_encode_session_parameters(struct vk_video_session_parameters *params);

#endif /* RADV_VIDEO_H */
