/*
 * Copyright © 2021 Red Hat
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#ifndef VK_VIDEO_H
#define VK_VIDEO_H

#include "vk_object.h"

#ifdef __cplusplus
extern "C" {
#endif

struct vk_video_h264_sps {
   StdVideoH264SequenceParameterSet base;
   int32_t offsets_for_ref_frame[256];
   StdVideoH264ScalingLists scaling_lists;
   StdVideoH264SequenceParameterSetVui vui;
   StdVideoH264HrdParameters vui_hrd_parameters;
};

struct vk_video_h264_pps {
   StdVideoH264PictureParameterSet base;
   StdVideoH264ScalingLists scaling_lists;
};

struct vk_video_h265_vps {
   StdVideoH265VideoParameterSet base;
   StdVideoH265DecPicBufMgr dec_pic_buf_mgr;
   StdVideoH265SubLayerHrdParameters hrd_parameters_nal;
   StdVideoH265SubLayerHrdParameters hrd_parameters_vcl;
   StdVideoH265HrdParameters hrd_parameters;
   StdVideoH265ProfileTierLevel tier_level;
};

struct vk_video_h265_sps {
   StdVideoH265SequenceParameterSet base;
   StdVideoH265ProfileTierLevel tier_level;
   StdVideoH265DecPicBufMgr dec_pic_buf_mgr;
   StdVideoH265ScalingLists scaling_lists;
   StdVideoH265ShortTermRefPicSet short_term_ref_pic_set;
   StdVideoH265LongTermRefPicsSps long_term_ref_pics_sps;
   StdVideoH265SubLayerHrdParameters hrd_parameters_nal;
   StdVideoH265SubLayerHrdParameters hrd_parameters_vcl;
   StdVideoH265HrdParameters hrd_parameters;
   StdVideoH265SequenceParameterSetVui vui;
   StdVideoH265PredictorPaletteEntries palette_entries;
};

struct vk_video_h265_pps {
   StdVideoH265PictureParameterSet base;
   StdVideoH265ScalingLists scaling_lists;
   StdVideoH265PredictorPaletteEntries palette_entries;
};

struct vk_video_av1_seq_hdr {
   StdVideoAV1SequenceHeader base;
   StdVideoAV1ColorConfig color_config;
   StdVideoAV1TimingInfo timing_info;
};

struct vk_video_session {
   struct vk_object_base base;
   VkVideoSessionCreateFlagsKHR flags;
   VkVideoCodecOperationFlagsKHR op;
   VkExtent2D max_coded;
   VkFormat picture_format;
   VkFormat ref_format;
   uint32_t max_dpb_slots;
   uint32_t max_active_ref_pics;

   struct {
      VkVideoEncodeUsageFlagsKHR video_usage_hints;
      VkVideoEncodeContentFlagsKHR video_content_hints;
      VkVideoEncodeTuningModeKHR tuning_mode;
   } enc_usage;
   union {
      struct {
         StdVideoH264ProfileIdc profile_idc;
      } h264;
      struct {
         StdVideoH265ProfileIdc profile_idc;
      } h265;
      struct {
         StdVideoAV1Profile profile;
         int film_grain_support;
      } av1;
   };
};

struct vk_video_session_parameters {
   struct vk_object_base base;
   VkVideoCodecOperationFlagsKHR op;
   union {
      struct {
         uint32_t max_h264_sps_count;
         uint32_t max_h264_pps_count;

         uint32_t h264_sps_count;
         struct vk_video_h264_sps *h264_sps;
         uint32_t h264_pps_count;
         struct vk_video_h264_pps *h264_pps;
      } h264_dec;

      struct {
         uint32_t max_h265_vps_count;
         uint32_t max_h265_sps_count;
         uint32_t max_h265_pps_count;

         uint32_t h265_vps_count;
         struct vk_video_h265_vps *h265_vps;
         uint32_t h265_sps_count;
         struct vk_video_h265_sps *h265_sps;
         uint32_t h265_pps_count;
         struct vk_video_h265_pps *h265_pps;
      } h265_dec;

      struct {
         struct vk_video_av1_seq_hdr seq_hdr;
      } av1_dec;

      struct {
         uint32_t max_h264_sps_count;
         uint32_t max_h264_pps_count;

         uint32_t h264_sps_count;
         struct vk_video_h264_sps *h264_sps;
         uint32_t h264_pps_count;
         struct vk_video_h264_pps *h264_pps;
         StdVideoH264ProfileIdc profile_idc;
      } h264_enc;

      struct {
         uint32_t max_h265_vps_count;
         uint32_t max_h265_sps_count;
         uint32_t max_h265_pps_count;

         uint32_t h265_vps_count;
         struct vk_video_h265_vps *h265_vps;
         uint32_t h265_sps_count;
         struct vk_video_h265_sps *h265_sps;
         uint32_t h265_pps_count;
         struct vk_video_h265_pps *h265_pps;
      } h265_enc;
   };
};

VkResult vk_video_session_init(struct vk_device *device,
                               struct vk_video_session *vid,
                               const VkVideoSessionCreateInfoKHR *create_info);

VkResult vk_video_session_parameters_init(struct vk_device *device,
                                          struct vk_video_session_parameters *params,
                                          const struct vk_video_session *vid,
                                          const struct vk_video_session_parameters *templ,
                                          const VkVideoSessionParametersCreateInfoKHR *create_info);

VkResult vk_video_session_parameters_update(struct vk_video_session_parameters *params,
                                            const VkVideoSessionParametersUpdateInfoKHR *update);

void vk_video_session_parameters_finish(struct vk_device *device,
                                        struct vk_video_session_parameters *params);

void vk_video_derive_h264_scaling_list(const StdVideoH264SequenceParameterSet *sps,
                                       const StdVideoH264PictureParameterSet *pps,
                                       StdVideoH264ScalingLists *list);

const StdVideoH264SequenceParameterSet *
vk_video_find_h264_dec_std_sps(const struct vk_video_session_parameters *params,
                               uint32_t id);
const StdVideoH264PictureParameterSet *
vk_video_find_h264_dec_std_pps(const struct vk_video_session_parameters *params,
                               uint32_t id);
const StdVideoH265VideoParameterSet *
vk_video_find_h265_dec_std_vps(const struct vk_video_session_parameters *params,
                               uint32_t id);
const StdVideoH265SequenceParameterSet *
vk_video_find_h265_dec_std_sps(const struct vk_video_session_parameters *params,
                               uint32_t id);
const StdVideoH265PictureParameterSet *
vk_video_find_h265_dec_std_pps(const struct vk_video_session_parameters *params,
                               uint32_t id);

struct vk_video_h265_slice_params {
   uint32_t slice_size;

   uint8_t first_slice_segment_in_pic_flag;
   StdVideoH265SliceType slice_type;
   uint8_t dependent_slice_segment;
   uint8_t temporal_mvp_enable;
   uint8_t loop_filter_across_slices_enable;
   int32_t pic_order_cnt_lsb;
   uint8_t sao_luma_flag;
   uint8_t sao_chroma_flag;
   uint8_t collocated_list;
   uint32_t collocated_ref_idx;
   uint8_t mvd_l1_zero_flag;

   uint8_t num_ref_idx_l0_active;
   uint8_t num_ref_idx_l1_active;
   uint8_t rpl_modification_flag[2];
   uint8_t cabac_init_idc;
   int8_t slice_qp_delta;
   int8_t slice_cb_qp_offset;
   int8_t slice_cr_qp_offset;
   int8_t max_num_merge_cand;
   uint32_t slice_data_bytes_offset;
   uint8_t disable_deblocking_filter_idc;
   int8_t tc_offset_div2;
   int8_t beta_offset_div2;
   uint32_t slice_segment_address;

   uint8_t luma_log2_weight_denom;
   uint8_t chroma_log2_weight_denom;
   uint8_t luma_weight_l0_flag[16];
   int16_t luma_weight_l0[16];
   int16_t luma_offset_l0[16];
   uint8_t chroma_weight_l0_flag[16];
   int16_t chroma_weight_l0[16][2];
   int16_t chroma_offset_l0[16][2];
   uint8_t luma_weight_l1_flag[16];
   int16_t luma_weight_l1[16];
   int16_t luma_offset_l1[16];
   uint8_t chroma_weight_l1_flag[16];
   int16_t chroma_weight_l1[16][2];
   int16_t chroma_offset_l1[16][2];

   int8_t delta_luma_weight_l0[16];
   int8_t delta_luma_weight_l1[16];
   int8_t delta_chroma_weight_l0[16][2];
   int8_t delta_chroma_weight_l1[16][2];
   int16_t delta_chroma_offset_l0[16][2];
   int16_t delta_chroma_offset_l1[16][2];
};

void
vk_video_parse_h265_slice_header(const struct VkVideoDecodeInfoKHR *frame_info,
                                 const VkVideoDecodeH265PictureInfoKHR *pic_info,
                                 const StdVideoH265SequenceParameterSet *sps,
                                 const StdVideoH265PictureParameterSet *pps,
                                 void *slice_data,
                                 uint32_t slice_size,
                                 struct vk_video_h265_slice_params *params);


struct vk_video_h265_reference {
   const VkVideoPictureResourceInfoKHR *pPictureResource;
   StdVideoDecodeH265ReferenceInfoFlags flags;
   uint32_t slot_index;
   int32_t pic_order_cnt;
};

int vk_video_h265_poc_by_slot(const struct VkVideoDecodeInfoKHR *frame_info, int slot);

void vk_fill_video_h265_reference_info(const VkVideoDecodeInfoKHR *frame_info,
                                       const struct VkVideoDecodeH265PictureInfoKHR *pic,
                                       const struct vk_video_h265_slice_params *slice_params,
                                       struct vk_video_h265_reference ref_slots[][8]);

#define VK_VIDEO_H264_MACROBLOCK_WIDTH 16
#define VK_VIDEO_H264_MACROBLOCK_HEIGHT 16

#define VK_VIDEO_H265_CTU_MAX_WIDTH 64
#define VK_VIDEO_H265_CTU_MAX_HEIGHT 64

#define VK_VIDEO_AV1_BLOCK_WIDTH 128
#define VK_VIDEO_AV1_BLOCK_HEIGHT 128

void
vk_video_get_profile_alignments(const VkVideoProfileListInfoKHR *profile_list,
                                uint32_t *width_align_out, uint32_t *height_align_out);

uint8_t
vk_video_get_h264_level(StdVideoH264LevelIdc level);

const StdVideoH264SequenceParameterSet *
vk_video_find_h264_enc_std_sps(const struct vk_video_session_parameters *params,
                               uint32_t id);
const StdVideoH264PictureParameterSet *
vk_video_find_h264_enc_std_pps(const struct vk_video_session_parameters *params,
                               uint32_t id);

const StdVideoH265VideoParameterSet *
vk_video_find_h265_enc_std_vps(const struct vk_video_session_parameters *params,
                               uint32_t id);
const StdVideoH265SequenceParameterSet *
vk_video_find_h265_enc_std_sps(const struct vk_video_session_parameters *params,
                               uint32_t id);
const StdVideoH265PictureParameterSet *
vk_video_find_h265_enc_std_pps(const struct vk_video_session_parameters *params,
                               uint32_t id);

void
vk_video_encode_h264_sps(const StdVideoH264SequenceParameterSet *sps,
                         size_t size_limit,
                         size_t *data_size_ptr,
                         void *data_ptr);

void
vk_video_encode_h264_pps(const StdVideoH264PictureParameterSet *pps,
                         bool high_profile,
                         size_t size_limit,
                         size_t *data_size_ptr,
                         void *data_ptr);

unsigned
vk_video_get_h265_nal_unit(const StdVideoEncodeH265PictureInfo *pic_info);

void
vk_video_encode_h265_vps(const StdVideoH265VideoParameterSet *vps,
                         size_t size_limit,
                         size_t *data_size,
                         void *data_ptr);
void
vk_video_encode_h265_sps(const StdVideoH265SequenceParameterSet *sps,
                         size_t size_limit,
                         size_t* pDataSize,
                         void* pData);

void
vk_video_encode_h265_pps(const StdVideoH265PictureParameterSet *pps,
                         size_t size_limit,
                         size_t *data_size,
                         void *data_ptr);

#ifdef __cplusplus
}
#endif

#endif
