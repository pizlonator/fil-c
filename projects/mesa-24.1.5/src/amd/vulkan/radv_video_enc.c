/**************************************************************************
 *
 * Copyright 2017 Advanced Micro Devices, Inc.
 * Copyright 2023 Red Hat Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/
#include "radv_buffer.h"
#include "radv_cs.h"
#include "radv_debug.h"
#include "radv_device_memory.h"
#include "radv_entrypoints.h"
#include "radv_image_view.h"
#include "radv_physical_device.h"
#include "radv_video.h"

#include "ac_vcn_enc.h"

#define RENCODE_V4_FW_INTERFACE_MAJOR_VERSION 1
#define RENCODE_V4_FW_INTERFACE_MINOR_VERSION 7

#define RENCODE_V4_IB_PARAM_ENCODE_STATISTICS 0x0000001a

#define RENCODE_V3_FW_INTERFACE_MAJOR_VERSION 1
#define RENCODE_V3_FW_INTERFACE_MINOR_VERSION 27

#define RENCODE_V2_IB_PARAM_SESSION_INFO              0x00000001
#define RENCODE_V2_IB_PARAM_TASK_INFO                 0x00000002
#define RENCODE_V2_IB_PARAM_SESSION_INIT              0x00000003
#define RENCODE_V2_IB_PARAM_LAYER_CONTROL             0x00000004
#define RENCODE_V2_IB_PARAM_LAYER_SELECT              0x00000005
#define RENCODE_V2_IB_PARAM_RATE_CONTROL_SESSION_INIT 0x00000006
#define RENCODE_V2_IB_PARAM_RATE_CONTROL_LAYER_INIT   0x00000007
#define RENCODE_V2_IB_PARAM_RATE_CONTROL_PER_PICTURE  0x00000008
#define RENCODE_V2_IB_PARAM_QUALITY_PARAMS            0x00000009
#define RENCODE_V2_IB_PARAM_DIRECT_OUTPUT_NALU        0x0000000a
#define RENCODE_V2_IB_PARAM_SLICE_HEADER              0x0000000b
#define RENCODE_V2_IB_PARAM_INPUT_FORMAT              0x0000000c
#define RENCODE_V2_IB_PARAM_OUTPUT_FORMAT             0x0000000d
#define RENCODE_V2_IB_PARAM_ENCODE_PARAMS             0x0000000f
#define RENCODE_V2_IB_PARAM_INTRA_REFRESH             0x00000010
#define RENCODE_V2_IB_PARAM_ENCODE_CONTEXT_BUFFER     0x00000011
#define RENCODE_V2_IB_PARAM_VIDEO_BITSTREAM_BUFFER    0x00000012
#define RENCODE_V2_IB_PARAM_FEEDBACK_BUFFER           0x00000015
#define RENCODE_V2_IB_PARAM_ENCODE_STATISTICS         0x00000019
#define RENCODE_V2_IB_PARAM_RATE_CONTROL_PER_PIC_EX   0x0000001d

#define RENCODE_V2_HEVC_IB_PARAM_SLICE_CONTROL 0x00100001
#define RENCODE_V2_HEVC_IB_PARAM_SPEC_MISC     0x00100002
#define RENCODE_V2_HEVC_IB_PARAM_LOOP_FILTER   0x00100003

#define RENCODE_V2_H264_IB_PARAM_SLICE_CONTROL     0x00200001
#define RENCODE_V2_H264_IB_PARAM_SPEC_MISC         0x00200002
#define RENCODE_V2_H264_IB_PARAM_ENCODE_PARAMS     0x00200003
#define RENCODE_V2_H264_IB_PARAM_DEBLOCKING_FILTER 0x00200004

#define RENCODE_V2_FW_INTERFACE_MAJOR_VERSION 1
#define RENCODE_V2_FW_INTERFACE_MINOR_VERSION 18

#define RENCODE_IB_PARAM_SESSION_INFO              0x00000001
#define RENCODE_IB_PARAM_TASK_INFO                 0x00000002
#define RENCODE_IB_PARAM_SESSION_INIT              0x00000003
#define RENCODE_IB_PARAM_LAYER_CONTROL             0x00000004
#define RENCODE_IB_PARAM_LAYER_SELECT              0x00000005
#define RENCODE_IB_PARAM_RATE_CONTROL_SESSION_INIT 0x00000006
#define RENCODE_IB_PARAM_RATE_CONTROL_LAYER_INIT   0x00000007
#define RENCODE_IB_PARAM_RATE_CONTROL_PER_PICTURE  0x00000008
#define RENCODE_IB_PARAM_QUALITY_PARAMS            0x00000009
#define RENCODE_IB_PARAM_SLICE_HEADER              0x0000000a
#define RENCODE_IB_PARAM_ENCODE_PARAMS             0x0000000b
#define RENCODE_IB_PARAM_INTRA_REFRESH             0x0000000c
#define RENCODE_IB_PARAM_ENCODE_CONTEXT_BUFFER     0x0000000d
#define RENCODE_IB_PARAM_VIDEO_BITSTREAM_BUFFER    0x0000000e
#define RENCODE_IB_PARAM_FEEDBACK_BUFFER           0x00000010
#define RENCODE_IB_PARAM_RATE_CONTROL_PER_PIC_EX   0x0000001d
#define RENCODE_IB_PARAM_DIRECT_OUTPUT_NALU        0x00000020
#define RENCODE_IB_PARAM_ENCODE_STATISTICS         0x00000024

#define RENCODE_HEVC_IB_PARAM_SLICE_CONTROL     0x00100001
#define RENCODE_HEVC_IB_PARAM_SPEC_MISC         0x00100002
#define RENCODE_HEVC_IB_PARAM_DEBLOCKING_FILTER 0x00100003

#define RENCODE_H264_IB_PARAM_SLICE_CONTROL     0x00200001
#define RENCODE_H264_IB_PARAM_SPEC_MISC         0x00200002
#define RENCODE_H264_IB_PARAM_ENCODE_PARAMS     0x00200003
#define RENCODE_H264_IB_PARAM_DEBLOCKING_FILTER 0x00200004

#define RENCODE_FW_INTERFACE_MAJOR_VERSION 1
#define RENCODE_FW_INTERFACE_MINOR_VERSION 15

void
radv_probe_video_encode(struct radv_physical_device *pdev)
{
   pdev->video_encode_enabled = false;
   if (pdev->info.vcn_ip_version >= VCN_4_0_0) {
      if (pdev->info.vcn_enc_major_version != RENCODE_V4_FW_INTERFACE_MAJOR_VERSION)
         return;
      if (pdev->info.vcn_enc_minor_version < RENCODE_V4_FW_INTERFACE_MINOR_VERSION)
         return;
   } else if (pdev->info.vcn_ip_version >= VCN_3_0_0) {
      if (pdev->info.vcn_enc_major_version != RENCODE_V3_FW_INTERFACE_MAJOR_VERSION)
         return;
      if (pdev->info.vcn_enc_minor_version < RENCODE_V3_FW_INTERFACE_MINOR_VERSION)
         return;
   } else if (pdev->info.vcn_ip_version >= VCN_2_0_0) {
      if (pdev->info.vcn_enc_major_version != RENCODE_V2_FW_INTERFACE_MAJOR_VERSION)
         return;
      if (pdev->info.vcn_enc_minor_version < RENCODE_V2_FW_INTERFACE_MINOR_VERSION)
         return;
   } else {
      if (pdev->info.vcn_enc_major_version != RENCODE_FW_INTERFACE_MAJOR_VERSION)
         return;
      if (pdev->info.vcn_enc_minor_version < RENCODE_FW_INTERFACE_MINOR_VERSION)
         return;
   }

   struct radv_instance *instance = radv_physical_device_instance(pdev);
   pdev->video_encode_enabled = !!(instance->perftest_flags & RADV_PERFTEST_VIDEO_ENCODE);
}

void
radv_init_physical_device_encoder(struct radv_physical_device *pdev)
{
   if (pdev->info.family >= CHIP_NAVI31) {
      pdev->enc_hw_ver = RADV_VIDEO_ENC_HW_4;
      pdev->encoder_interface_version = ((RENCODE_V4_FW_INTERFACE_MAJOR_VERSION << RENCODE_IF_MAJOR_VERSION_SHIFT) |
                                         (RENCODE_V4_FW_INTERFACE_MINOR_VERSION << RENCODE_IF_MINOR_VERSION_SHIFT));
   } else if (pdev->info.family >= CHIP_NAVI21) {
      pdev->enc_hw_ver = RADV_VIDEO_ENC_HW_3;
      pdev->encoder_interface_version = ((RENCODE_V3_FW_INTERFACE_MAJOR_VERSION << RENCODE_IF_MAJOR_VERSION_SHIFT) |
                                         (RENCODE_V3_FW_INTERFACE_MINOR_VERSION << RENCODE_IF_MINOR_VERSION_SHIFT));
   } else if (pdev->info.family >= CHIP_RENOIR) {
      pdev->enc_hw_ver = RADV_VIDEO_ENC_HW_2;
      pdev->encoder_interface_version = ((RENCODE_V2_FW_INTERFACE_MAJOR_VERSION << RENCODE_IF_MAJOR_VERSION_SHIFT) |
                                         (RENCODE_V2_FW_INTERFACE_MINOR_VERSION << RENCODE_IF_MINOR_VERSION_SHIFT));
   } else {
      pdev->enc_hw_ver = RADV_VIDEO_ENC_HW_1_2;
      pdev->encoder_interface_version = ((RENCODE_FW_INTERFACE_MAJOR_VERSION << RENCODE_IF_MAJOR_VERSION_SHIFT) |
                                         (RENCODE_FW_INTERFACE_MINOR_VERSION << RENCODE_IF_MINOR_VERSION_SHIFT));
   }

   if (pdev->info.family >= CHIP_RENOIR) {
      pdev->vcn_enc_cmds.session_info = RENCODE_V2_IB_PARAM_SESSION_INFO;
      pdev->vcn_enc_cmds.task_info = RENCODE_V2_IB_PARAM_TASK_INFO;
      pdev->vcn_enc_cmds.session_init = RENCODE_V2_IB_PARAM_SESSION_INIT;
      pdev->vcn_enc_cmds.layer_control = RENCODE_V2_IB_PARAM_LAYER_CONTROL;
      pdev->vcn_enc_cmds.layer_select = RENCODE_V2_IB_PARAM_LAYER_SELECT;
      pdev->vcn_enc_cmds.rc_session_init = RENCODE_V2_IB_PARAM_RATE_CONTROL_SESSION_INIT;
      pdev->vcn_enc_cmds.rc_layer_init = RENCODE_V2_IB_PARAM_RATE_CONTROL_LAYER_INIT;
      pdev->vcn_enc_cmds.rc_per_pic = RENCODE_V2_IB_PARAM_RATE_CONTROL_PER_PIC_EX;
      pdev->vcn_enc_cmds.quality_params = RENCODE_V2_IB_PARAM_QUALITY_PARAMS;
      pdev->vcn_enc_cmds.nalu = RENCODE_V2_IB_PARAM_DIRECT_OUTPUT_NALU;
      pdev->vcn_enc_cmds.slice_header = RENCODE_V2_IB_PARAM_SLICE_HEADER;
      pdev->vcn_enc_cmds.input_format = RENCODE_V2_IB_PARAM_INPUT_FORMAT;
      pdev->vcn_enc_cmds.output_format = RENCODE_V2_IB_PARAM_OUTPUT_FORMAT;
      pdev->vcn_enc_cmds.enc_params = RENCODE_V2_IB_PARAM_ENCODE_PARAMS;
      pdev->vcn_enc_cmds.intra_refresh = RENCODE_V2_IB_PARAM_INTRA_REFRESH;
      pdev->vcn_enc_cmds.ctx = RENCODE_V2_IB_PARAM_ENCODE_CONTEXT_BUFFER;
      pdev->vcn_enc_cmds.bitstream = RENCODE_V2_IB_PARAM_VIDEO_BITSTREAM_BUFFER;
      pdev->vcn_enc_cmds.feedback = RENCODE_V2_IB_PARAM_FEEDBACK_BUFFER;
      pdev->vcn_enc_cmds.slice_control_hevc = RENCODE_V2_HEVC_IB_PARAM_SLICE_CONTROL;
      pdev->vcn_enc_cmds.spec_misc_hevc = RENCODE_V2_HEVC_IB_PARAM_SPEC_MISC;
      pdev->vcn_enc_cmds.deblocking_filter_hevc = RENCODE_V2_HEVC_IB_PARAM_LOOP_FILTER;
      pdev->vcn_enc_cmds.slice_control_h264 = RENCODE_V2_H264_IB_PARAM_SLICE_CONTROL;
      pdev->vcn_enc_cmds.spec_misc_h264 = RENCODE_V2_H264_IB_PARAM_SPEC_MISC;
      pdev->vcn_enc_cmds.enc_params_h264 = RENCODE_V2_H264_IB_PARAM_ENCODE_PARAMS;
      pdev->vcn_enc_cmds.deblocking_filter_h264 = RENCODE_V2_H264_IB_PARAM_DEBLOCKING_FILTER;
      if (pdev->enc_hw_ver == RADV_VIDEO_ENC_HW_4) {
         pdev->vcn_enc_cmds.enc_statistics = RENCODE_V4_IB_PARAM_ENCODE_STATISTICS;
      } else
         pdev->vcn_enc_cmds.enc_statistics = RENCODE_V2_IB_PARAM_ENCODE_STATISTICS;
   } else {
      pdev->vcn_enc_cmds.session_info = RENCODE_IB_PARAM_SESSION_INFO;
      pdev->vcn_enc_cmds.task_info = RENCODE_IB_PARAM_TASK_INFO;
      pdev->vcn_enc_cmds.session_init = RENCODE_IB_PARAM_SESSION_INIT;
      pdev->vcn_enc_cmds.layer_control = RENCODE_IB_PARAM_LAYER_CONTROL;
      pdev->vcn_enc_cmds.layer_select = RENCODE_IB_PARAM_LAYER_SELECT;
      pdev->vcn_enc_cmds.rc_session_init = RENCODE_IB_PARAM_RATE_CONTROL_SESSION_INIT;
      pdev->vcn_enc_cmds.rc_layer_init = RENCODE_IB_PARAM_RATE_CONTROL_LAYER_INIT;
      pdev->vcn_enc_cmds.rc_per_pic = RENCODE_IB_PARAM_RATE_CONTROL_PER_PIC_EX;
      pdev->vcn_enc_cmds.quality_params = RENCODE_IB_PARAM_QUALITY_PARAMS;
      pdev->vcn_enc_cmds.nalu = RENCODE_IB_PARAM_DIRECT_OUTPUT_NALU;
      pdev->vcn_enc_cmds.slice_header = RENCODE_IB_PARAM_SLICE_HEADER;
      pdev->vcn_enc_cmds.enc_params = RENCODE_IB_PARAM_ENCODE_PARAMS;
      pdev->vcn_enc_cmds.intra_refresh = RENCODE_IB_PARAM_INTRA_REFRESH;
      pdev->vcn_enc_cmds.ctx = RENCODE_IB_PARAM_ENCODE_CONTEXT_BUFFER;
      pdev->vcn_enc_cmds.bitstream = RENCODE_IB_PARAM_VIDEO_BITSTREAM_BUFFER;
      pdev->vcn_enc_cmds.feedback = RENCODE_IB_PARAM_FEEDBACK_BUFFER;
      pdev->vcn_enc_cmds.slice_control_hevc = RENCODE_HEVC_IB_PARAM_SLICE_CONTROL;
      pdev->vcn_enc_cmds.spec_misc_hevc = RENCODE_HEVC_IB_PARAM_SPEC_MISC;
      pdev->vcn_enc_cmds.deblocking_filter_hevc = RENCODE_HEVC_IB_PARAM_DEBLOCKING_FILTER;
      pdev->vcn_enc_cmds.slice_control_h264 = RENCODE_H264_IB_PARAM_SLICE_CONTROL;
      pdev->vcn_enc_cmds.spec_misc_h264 = RENCODE_H264_IB_PARAM_SPEC_MISC;
      pdev->vcn_enc_cmds.enc_params_h264 = RENCODE_H264_IB_PARAM_ENCODE_PARAMS;
      pdev->vcn_enc_cmds.deblocking_filter_h264 = RENCODE_H264_IB_PARAM_DEBLOCKING_FILTER;
      pdev->vcn_enc_cmds.enc_statistics = RENCODE_IB_PARAM_ENCODE_STATISTICS;
   }
}

/* to process invalid frame rate */
static void
radv_vcn_enc_invalid_frame_rate(uint32_t *den, uint32_t *num)
{
   if (*den == 0 || *num == 0) {
      *den = 1;
      *num = 30;
   }
}

static uint32_t
radv_vcn_per_frame_integer(uint32_t bitrate, uint32_t den, uint32_t num)
{
   uint64_t rate_den = (uint64_t)bitrate * (uint64_t)den;

   return (uint32_t)(rate_den / num);
}

static uint32_t
radv_vcn_per_frame_frac(uint32_t bitrate, uint32_t den, uint32_t num)
{
   uint64_t rate_den = (uint64_t)bitrate * (uint64_t)den;
   uint64_t remainder = rate_den % num;

   return (uint32_t)((remainder << 32) / num);
}

static void
radv_enc_set_emulation_prevention(struct radv_cmd_buffer *cmd_buffer, bool set)
{
   struct radv_enc_state *enc = &cmd_buffer->video.enc;
   if (set != enc->emulation_prevention) {
      enc->emulation_prevention = set;
      enc->num_zeros = 0;
   }
}

static uint32_t
radv_enc_value_bits(uint32_t value)
{
   uint32_t i = 1;

   while (value > 1) {
      i++;
      value >>= 1;
   }

   return i;
}

static const unsigned index_to_shifts[4] = {24, 16, 8, 0};

static void
radv_enc_output_one_byte(struct radv_cmd_buffer *cmd_buffer, unsigned char byte)
{
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   struct radv_enc_state *enc = &cmd_buffer->video.enc;
   if (enc->byte_index == 0)
      cs->buf[cs->cdw] = 0;
   cs->buf[cs->cdw] |= ((unsigned int)(byte) << index_to_shifts[enc->byte_index]);
   enc->byte_index++;

   if (enc->byte_index >= 4) {
      enc->byte_index = 0;
      cs->cdw++;
   }
}

static void
radv_enc_emulation_prevention(struct radv_cmd_buffer *cmd_buffer, unsigned char byte)
{
   struct radv_enc_state *enc = &cmd_buffer->video.enc;
   if (enc->emulation_prevention) {
      if ((enc->num_zeros >= 2) && ((byte == 0x00) || (byte == 0x01) || (byte == 0x02) || (byte == 0x03))) {
         radv_enc_output_one_byte(cmd_buffer, 0x03);
         enc->bits_output += 8;
         enc->num_zeros = 0;
      }
      enc->num_zeros = (byte == 0 ? (enc->num_zeros + 1) : 0);
   }
}

static void
radv_enc_code_fixed_bits(struct radv_cmd_buffer *cmd_buffer, unsigned int value, unsigned int num_bits)
{
   struct radv_enc_state *enc = &cmd_buffer->video.enc;
   unsigned int bits_to_pack = 0;
   enc->bits_size += num_bits;

   while (num_bits > 0) {
      unsigned int value_to_pack = value & (0xffffffff >> (32 - num_bits));
      bits_to_pack = num_bits > (32 - enc->bits_in_shifter) ? (32 - enc->bits_in_shifter) : num_bits;

      if (bits_to_pack < num_bits)
         value_to_pack = value_to_pack >> (num_bits - bits_to_pack);

      enc->shifter |= value_to_pack << (32 - enc->bits_in_shifter - bits_to_pack);
      num_bits -= bits_to_pack;
      enc->bits_in_shifter += bits_to_pack;

      while (enc->bits_in_shifter >= 8) {
         unsigned char output_byte = (unsigned char)(enc->shifter >> 24);
         enc->shifter <<= 8;
         radv_enc_emulation_prevention(cmd_buffer, output_byte);
         radv_enc_output_one_byte(cmd_buffer, output_byte);
         enc->bits_in_shifter -= 8;
         enc->bits_output += 8;
      }
   }
}

static void
radv_enc_reset(struct radv_cmd_buffer *cmd_buffer)
{
   struct radv_enc_state *enc = &cmd_buffer->video.enc;
   enc->emulation_prevention = false;
   enc->shifter = 0;
   enc->bits_in_shifter = 0;
   enc->bits_output = 0;
   enc->num_zeros = 0;
   enc->byte_index = 0;
   enc->bits_size = 0;
}

static void
radv_enc_byte_align(struct radv_cmd_buffer *cmd_buffer)
{
   struct radv_enc_state *enc = &cmd_buffer->video.enc;
   unsigned int num_padding_zeros = (32 - enc->bits_in_shifter) % 8;

   if (num_padding_zeros > 0)
      radv_enc_code_fixed_bits(cmd_buffer, 0, num_padding_zeros);
}

static void
radv_enc_flush_headers(struct radv_cmd_buffer *cmd_buffer)
{
   struct radv_enc_state *enc = &cmd_buffer->video.enc;
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   if (enc->bits_in_shifter != 0) {
      unsigned char output_byte = (unsigned char)(enc->shifter >> 24);
      radv_enc_emulation_prevention(cmd_buffer, output_byte);
      radv_enc_output_one_byte(cmd_buffer, output_byte);
      enc->bits_output += enc->bits_in_shifter;
      enc->shifter = 0;
      enc->bits_in_shifter = 0;
      enc->num_zeros = 0;
   }

   if (enc->byte_index > 0) {
      cs->cdw++;
      enc->byte_index = 0;
   }
}

static void
radv_enc_code_ue(struct radv_cmd_buffer *cmd_buffer, unsigned int value)
{
   int x = -1;
   unsigned int ue_code = value + 1;
   value += 1;

   while (value) {
      value = (value >> 1);
      x += 1;
   }

   unsigned int ue_length = (x << 1) + 1;
   radv_enc_code_fixed_bits(cmd_buffer, ue_code, ue_length);
}

static void
radv_enc_code_se(struct radv_cmd_buffer *cmd_buffer, int value)
{
   unsigned int v = 0;

   if (value != 0)
      v = (value < 0 ? ((unsigned int)(0 - value) << 1) : (((unsigned int)(value) << 1) - 1));

   radv_enc_code_ue(cmd_buffer, v);
}

#define ENC_BEGIN                                                                                                      \
   {                                                                                                                   \
      uint32_t begin = cs->cdw++;

#define ENC_END                                                                                                        \
   radeon_emit_direct(cs, begin, (cs->cdw - begin) * 4);                                                               \
   cmd_buffer->video.enc.total_task_size += cs->buf[begin];                                                            \
   }

static void
radv_enc_session_info(struct radv_cmd_buffer *cmd_buffer)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   ENC_BEGIN;
   radeon_emit(cs, pdev->vcn_enc_cmds.session_info);
   radeon_emit(cs, pdev->encoder_interface_version);

   radv_cs_add_buffer(device->ws, cs, cmd_buffer->video.vid->sessionctx.mem->bo);
   uint64_t va = radv_buffer_get_va(cmd_buffer->video.vid->sessionctx.mem->bo);
   va += cmd_buffer->video.vid->sessionctx.offset;
   radeon_emit(cs, va >> 32);
   radeon_emit(cs, va & 0xffffffff);
   radeon_emit(cs, RENCODE_ENGINE_TYPE_ENCODE);
   ENC_END;
}

static void
radv_enc_task_info(struct radv_cmd_buffer *cmd_buffer, bool feedback)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   struct radv_enc_state *enc = &cmd_buffer->video.enc;

   enc->task_id++;
   ENC_BEGIN;
   radeon_emit(cs, pdev->vcn_enc_cmds.task_info);
   enc->task_size_offset = cs->cdw++;
   radeon_emit(cs, enc->task_id);
   radeon_emit(cs, feedback ? 1 : 0);
   ENC_END;
}

static void
radv_enc_session_init(struct radv_cmd_buffer *cmd_buffer, const struct VkVideoEncodeInfoKHR *enc_info)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radv_video_session *vid = cmd_buffer->video.vid;
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   unsigned alignment = 16;
   if (vid->vk.op == VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR) {
      alignment = 64;
   }

   uint32_t w = enc_info->srcPictureResource.codedExtent.width;
   uint32_t h = enc_info->srcPictureResource.codedExtent.height;
   uint32_t aligned_picture_width = align(w, alignment);
   uint32_t aligned_picture_height = align(h, alignment);
   uint32_t padding_width = aligned_picture_width - w;
   uint32_t padding_height = aligned_picture_height - h;

   ENC_BEGIN;
   radeon_emit(cs, pdev->vcn_enc_cmds.session_init);
   radeon_emit(cs, vid->enc_session.encode_standard);
   radeon_emit(cs, aligned_picture_width);
   radeon_emit(cs, aligned_picture_height);
   radeon_emit(cs, padding_width);
   radeon_emit(cs, padding_height);
   radeon_emit(cs, vid->enc_session.pre_encode_mode);
   radeon_emit(cs, vid->enc_session.pre_encode_chroma_enabled);
   if (pdev->enc_hw_ver >= RADV_VIDEO_ENC_HW_3) {
      radeon_emit(cs, 0); // slice output enabled.
   }
   radeon_emit(cs, vid->enc_session.display_remote);
   ENC_END;
}

static void
radv_enc_layer_control(struct radv_cmd_buffer *cmd_buffer, const rvcn_enc_layer_control_t *rc_layer_control)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   ENC_BEGIN;
   radeon_emit(cs, pdev->vcn_enc_cmds.layer_control);
   radeon_emit(cs, rc_layer_control->max_num_temporal_layers); // max num temporal layesr
   radeon_emit(cs, rc_layer_control->num_temporal_layers);     // num temporal layers
   ENC_END;
}

static void
radv_enc_layer_select(struct radv_cmd_buffer *cmd_buffer, int tl_idx)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   ENC_BEGIN;
   radeon_emit(cs, pdev->vcn_enc_cmds.layer_select);
   radeon_emit(cs, tl_idx); // temporal layer index
   ENC_END;
}

static void
radv_enc_slice_control(struct radv_cmd_buffer *cmd_buffer, const struct VkVideoEncodeInfoKHR *enc_info)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radeon_cmdbuf *cs = cmd_buffer->cs;

   uint32_t num_mbs_in_slice;
   uint32_t width_in_mbs = enc_info->srcPictureResource.codedExtent.width / 16;
   uint32_t height_in_mbs = enc_info->srcPictureResource.codedExtent.height / 16;
   num_mbs_in_slice = width_in_mbs * height_in_mbs;
   ENC_BEGIN;
   radeon_emit(cs, pdev->vcn_enc_cmds.slice_control_h264);
   radeon_emit(cs, RENCODE_H264_SLICE_CONTROL_MODE_FIXED_MBS); // slice control mode
   radeon_emit(cs, num_mbs_in_slice);                          // num mbs per slice
   ENC_END;
}

static void
radv_enc_spec_misc_h264(struct radv_cmd_buffer *cmd_buffer, const struct VkVideoEncodeInfoKHR *enc_info)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   const struct VkVideoEncodeH264PictureInfoKHR *h264_picture_info =
      vk_find_struct_const(enc_info->pNext, VIDEO_ENCODE_H264_PICTURE_INFO_KHR);
   const StdVideoEncodeH264PictureInfo *pic = h264_picture_info->pStdPictureInfo;
   const StdVideoH264SequenceParameterSet *sps =
      vk_video_find_h264_enc_std_sps(&cmd_buffer->video.params->vk, pic->seq_parameter_set_id);
   const StdVideoH264PictureParameterSet *pps =
      vk_video_find_h264_enc_std_pps(&cmd_buffer->video.params->vk, pic->pic_parameter_set_id);
   ENC_BEGIN;
   radeon_emit(cs, pdev->vcn_enc_cmds.spec_misc_h264);
   radeon_emit(cs, pps->flags.constrained_intra_pred_flag);     // constrained_intra_pred_flag
   radeon_emit(cs, pps->flags.entropy_coding_mode_flag);        // cabac enable
   radeon_emit(cs, 0);                                          // cabac init idc
   radeon_emit(cs, 1);                                          // half pel enabled
   radeon_emit(cs, 1);                                          // quarter pel enabled
   radeon_emit(cs, cmd_buffer->video.vid->vk.h264.profile_idc); // profile_idc
   radeon_emit(cs, vk_video_get_h264_level(sps->level_idc));

   if (pdev->enc_hw_ver >= RADV_VIDEO_ENC_HW_3) {
      radeon_emit(cs, 0);                        // v3 b_picture_enabled
      radeon_emit(cs, pps->weighted_bipred_idc); // v3 weighted bipred idc
   }

   ENC_END;
}

static void
radv_enc_spec_misc_hevc(struct radv_cmd_buffer *cmd_buffer, const struct VkVideoEncodeInfoKHR *enc_info)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   const struct VkVideoEncodeH265PictureInfoKHR *h265_picture_info =
      vk_find_struct_const(enc_info->pNext, VIDEO_ENCODE_H265_PICTURE_INFO_KHR);
   const StdVideoEncodeH265PictureInfo *pic = h265_picture_info->pStdPictureInfo;
   const VkVideoEncodeH265NaluSliceSegmentInfoKHR *h265_slice = &h265_picture_info->pNaluSliceSegmentEntries[0];
   const StdVideoEncodeH265SliceSegmentHeader *slice = h265_slice->pStdSliceSegmentHeader;
   const StdVideoH265SequenceParameterSet *sps =
      vk_video_find_h265_enc_std_sps(&cmd_buffer->video.params->vk, pic->pps_seq_parameter_set_id);
   const StdVideoH265PictureParameterSet *pps =
      vk_video_find_h265_enc_std_pps(&cmd_buffer->video.params->vk, pic->pps_pic_parameter_set_id);
   ENC_BEGIN;
   radeon_emit(cs, pdev->vcn_enc_cmds.spec_misc_hevc);
   radeon_emit(cs, sps->log2_min_luma_coding_block_size_minus3);
   radeon_emit(cs, !sps->flags.amp_enabled_flag);
   radeon_emit(cs, sps->flags.strong_intra_smoothing_enabled_flag);
   radeon_emit(cs, pps->flags.constrained_intra_pred_flag);
   radeon_emit(cs, slice->flags.cabac_init_flag);
   radeon_emit(cs, 1); // enc->enc_pic.hevc_spec_misc.half_pel_enabled
   radeon_emit(cs, 1); // enc->enc_pic.hevc_spec_misc.quarter_pel_enabled
   if (pdev->enc_hw_ver >= RADV_VIDEO_ENC_HW_3) {
      radeon_emit(cs, !pps->flags.transform_skip_enabled_flag);
      radeon_emit(cs, pps->flags.cu_qp_delta_enabled_flag);
   }
   ENC_END;
}

static void
radv_enc_slice_control_hevc(struct radv_cmd_buffer *cmd_buffer, const struct VkVideoEncodeInfoKHR *enc_info)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   const struct VkVideoEncodeH265PictureInfoKHR *h265_picture_info =
      vk_find_struct_const(enc_info->pNext, VIDEO_ENCODE_H265_PICTURE_INFO_KHR);
   const StdVideoEncodeH265PictureInfo *pic = h265_picture_info->pStdPictureInfo;
   const StdVideoH265SequenceParameterSet *sps =
      vk_video_find_h265_enc_std_sps(&cmd_buffer->video.params->vk, pic->pps_seq_parameter_set_id);

   uint32_t width_in_ctb, height_in_ctb, num_ctbs_in_slice;

   width_in_ctb = sps->pic_width_in_luma_samples / 64;
   height_in_ctb = sps->pic_height_in_luma_samples / 64;
   num_ctbs_in_slice = width_in_ctb * height_in_ctb;
   ENC_BEGIN;
   radeon_emit(cs, pdev->vcn_enc_cmds.slice_control_hevc);
   radeon_emit(cs, RENCODE_HEVC_SLICE_CONTROL_MODE_FIXED_CTBS);
   radeon_emit(cs, num_ctbs_in_slice); // num_ctbs_in_slice
   radeon_emit(cs, num_ctbs_in_slice); // num_ctbs_in_slice_segment
   ENC_END;
}

static void
radv_enc_rc_session_init(struct radv_cmd_buffer *cmd_buffer)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   struct radv_video_session *vid = cmd_buffer->video.vid;
   ENC_BEGIN;
   radeon_emit(cs, pdev->vcn_enc_cmds.rc_session_init);
   radeon_emit(cs, vid->enc_rate_control_method); // rate_control_method);
   radeon_emit(cs, vid->enc_vbv_buffer_level);    // vbv_buffer_level);
   ENC_END;
}

static void
radv_enc_rc_layer_init(struct radv_cmd_buffer *cmd_buffer, rvcn_enc_rate_ctl_layer_init_t *layer_init)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   ENC_BEGIN;
   radeon_emit(cs, pdev->vcn_enc_cmds.rc_layer_init);
   radeon_emit(cs, layer_init->target_bit_rate);                  // target bit rate
   radeon_emit(cs, layer_init->peak_bit_rate);                    // peak bit rate
   radeon_emit(cs, layer_init->frame_rate_num);                   // frame rate num
   radeon_emit(cs, layer_init->frame_rate_den);                   // frame rate dem
   radeon_emit(cs, layer_init->vbv_buffer_size);                  // vbv buffer size
   radeon_emit(cs, layer_init->avg_target_bits_per_picture);      // avg target bits per picture
   radeon_emit(cs, layer_init->peak_bits_per_picture_integer);    // peak bit per picture int
   radeon_emit(cs, layer_init->peak_bits_per_picture_fractional); // peak bit per picture fract
   ENC_END;
}

static void
radv_enc_deblocking_filter_h264(struct radv_cmd_buffer *cmd_buffer, const VkVideoEncodeInfoKHR *enc_info)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   const struct VkVideoEncodeH264PictureInfoKHR *h264_picture_info =
      vk_find_struct_const(enc_info->pNext, VIDEO_ENCODE_H264_PICTURE_INFO_KHR);
   const VkVideoEncodeH264NaluSliceInfoKHR *h264_slice = &h264_picture_info->pNaluSliceEntries[0];
   const StdVideoEncodeH264SliceHeader *slice = h264_slice->pStdSliceHeader;
   ENC_BEGIN;
   radeon_emit(cs, pdev->vcn_enc_cmds.deblocking_filter_h264);
   radeon_emit(cs, slice->disable_deblocking_filter_idc);
   radeon_emit(cs, slice->slice_alpha_c0_offset_div2);
   radeon_emit(cs, slice->slice_beta_offset_div2);
   radeon_emit(cs, 0); // cb qp offset
   radeon_emit(cs, 0); // cr qp offset
   ENC_END;
}

static void
radv_enc_deblocking_filter_hevc(struct radv_cmd_buffer *cmd_buffer, const VkVideoEncodeInfoKHR *enc_info)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   const struct VkVideoEncodeH265PictureInfoKHR *h265_picture_info =
      vk_find_struct_const(enc_info->pNext, VIDEO_ENCODE_H265_PICTURE_INFO_KHR);
   const StdVideoEncodeH265PictureInfo *pic = h265_picture_info->pStdPictureInfo;
   const VkVideoEncodeH265NaluSliceSegmentInfoKHR *h265_slice = &h265_picture_info->pNaluSliceSegmentEntries[0];
   const StdVideoEncodeH265SliceSegmentHeader *slice = h265_slice->pStdSliceSegmentHeader;
   const StdVideoH265SequenceParameterSet *sps =
      vk_video_find_h265_enc_std_sps(&cmd_buffer->video.params->vk, pic->pps_seq_parameter_set_id);

   ENC_BEGIN;
   radeon_emit(cs, pdev->vcn_enc_cmds.deblocking_filter_hevc);
   radeon_emit(cs, slice->flags.slice_loop_filter_across_slices_enabled_flag);
   radeon_emit(cs, slice->flags.slice_deblocking_filter_disabled_flag);
   radeon_emit(cs, slice->slice_beta_offset_div2);
   radeon_emit(cs, slice->slice_tc_offset_div2);
   radeon_emit(cs, slice->slice_cb_qp_offset);
   radeon_emit(cs, slice->slice_cr_qp_offset);
   if (pdev->enc_hw_ver >= RADV_VIDEO_ENC_HW_2)
      radeon_emit(cs, !sps->flags.sample_adaptive_offset_enabled_flag);
   ENC_END;
}

static void
radv_enc_quality_params(struct radv_cmd_buffer *cmd_buffer)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   ENC_BEGIN;
   radeon_emit(cs, pdev->vcn_enc_cmds.quality_params);
   radeon_emit(cs, 0);
   radeon_emit(cs, 0);
   radeon_emit(cs, 0);
   radeon_emit(cs, 0);
   if (pdev->enc_hw_ver >= RADV_VIDEO_ENC_HW_2)
      radeon_emit(cs, 0);
   ENC_END;
}

static void
radv_enc_slice_header(struct radv_cmd_buffer *cmd_buffer, const VkVideoEncodeInfoKHR *enc_info)
{
   struct radv_enc_state *enc = &cmd_buffer->video.enc;
   uint32_t instruction[RENCODE_SLICE_HEADER_TEMPLATE_MAX_NUM_INSTRUCTIONS] = {0};
   uint32_t num_bits[RENCODE_SLICE_HEADER_TEMPLATE_MAX_NUM_INSTRUCTIONS] = {0};
   const struct VkVideoEncodeH264PictureInfoKHR *h264_picture_info =
      vk_find_struct_const(enc_info->pNext, VIDEO_ENCODE_H264_PICTURE_INFO_KHR);
   int slice_count = h264_picture_info->naluSliceEntryCount;
   const StdVideoEncodeH264PictureInfo *pic = h264_picture_info->pStdPictureInfo;
   const StdVideoH264SequenceParameterSet *sps =
      vk_video_find_h264_enc_std_sps(&cmd_buffer->video.params->vk, pic->seq_parameter_set_id);
   const StdVideoH264PictureParameterSet *pps =
      vk_video_find_h264_enc_std_pps(&cmd_buffer->video.params->vk, pic->pic_parameter_set_id);
   const VkVideoEncodeH264NaluSliceInfoKHR *slice_info = &h264_picture_info->pNaluSliceEntries[0];

   unsigned int inst_index = 0;
   unsigned int cdw_start = 0;
   unsigned int cdw_filled = 0;
   unsigned int bits_copied = 0;

   assert(slice_count <= 1);

   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   ENC_BEGIN;
   radeon_emit(cs, pdev->vcn_enc_cmds.slice_header);
   radv_enc_reset(cmd_buffer);
   radv_enc_set_emulation_prevention(cmd_buffer, false);

   cdw_start = cs->cdw;

   if (pic->flags.IdrPicFlag)
      radv_enc_code_fixed_bits(cmd_buffer, 0x65, 8);
   else if (!pic->flags.is_reference)
      radv_enc_code_fixed_bits(cmd_buffer, 0x01, 8);
   else
      radv_enc_code_fixed_bits(cmd_buffer, 0x41, 8);

   radv_enc_flush_headers(cmd_buffer);
   instruction[inst_index] = RENCODE_HEADER_INSTRUCTION_COPY;
   num_bits[inst_index] = enc->bits_output - bits_copied;
   bits_copied = enc->bits_output;
   inst_index++;

   instruction[inst_index] = RENCODE_H264_HEADER_INSTRUCTION_FIRST_MB;
   inst_index++;

   switch (pic->primary_pic_type) {
   case STD_VIDEO_H264_PICTURE_TYPE_I:
   case STD_VIDEO_H264_PICTURE_TYPE_IDR:
   default:
      radv_enc_code_ue(cmd_buffer, 7);
      break;
   case STD_VIDEO_H264_PICTURE_TYPE_P:
      radv_enc_code_ue(cmd_buffer, 5);
      break;
   case STD_VIDEO_H264_PICTURE_TYPE_B:
      radv_enc_code_ue(cmd_buffer, 6);
      break;
   }
   radv_enc_code_ue(cmd_buffer, 0x0);
   radv_enc_code_fixed_bits(cmd_buffer, pic->frame_num % 32, sps->log2_max_frame_num_minus4 + 4);
#if 0
   if (enc->enc_pic.h264_enc_params.input_picture_structure !=
       RENCODE_H264_PICTURE_STRUCTURE_FRAME) {
      radv_enc_code_fixed_bits(cmd_buffer, 0x1, 1);
      radv_enc_code_fixed_bits(cmd_buffer,
                                 enc->enc_pic.h264_enc_params.input_picture_structure ==
                                       RENCODE_H264_PICTURE_STRUCTURE_BOTTOM_FIELD
                                    ? 1
                                    : 0,
                                 1);
   }
#endif

   if (pic->flags.IdrPicFlag)
      radv_enc_code_ue(cmd_buffer, pic->idr_pic_id);

   if (sps->pic_order_cnt_type == STD_VIDEO_H264_POC_TYPE_0)
      radv_enc_code_fixed_bits(cmd_buffer, pic->PicOrderCnt % 32, sps->log2_max_pic_order_cnt_lsb_minus4 + 4);

   if (pps->flags.redundant_pic_cnt_present_flag)
      radv_enc_code_ue(cmd_buffer, 0);

   if (pic->primary_pic_type == STD_VIDEO_H264_PICTURE_TYPE_B) {
      radv_enc_code_fixed_bits(cmd_buffer, slice_info->pStdSliceHeader->flags.direct_spatial_mv_pred_flag, 1);
   }
   const StdVideoEncodeH264ReferenceListsInfo *ref_lists = pic->pRefLists;
   /* ref_pic_list_modification() */
   if (pic->primary_pic_type != STD_VIDEO_H264_PICTURE_TYPE_IDR &&
       pic->primary_pic_type != STD_VIDEO_H264_PICTURE_TYPE_I) {

      /* num ref idx active override flag */
      radv_enc_code_fixed_bits(cmd_buffer, slice_info->pStdSliceHeader->flags.num_ref_idx_active_override_flag, 1);
      if (slice_info->pStdSliceHeader->flags.num_ref_idx_active_override_flag) {
         radv_enc_code_ue(cmd_buffer, ref_lists->num_ref_idx_l0_active_minus1);
         if (pic->primary_pic_type == STD_VIDEO_H264_PICTURE_TYPE_B)
            radv_enc_code_ue(cmd_buffer, ref_lists->num_ref_idx_l1_active_minus1);
      }

      radv_enc_code_fixed_bits(cmd_buffer, ref_lists->flags.ref_pic_list_modification_flag_l0, 1);
      if (ref_lists->flags.ref_pic_list_modification_flag_l0) {
         for (unsigned op = 0; op < ref_lists->refList0ModOpCount; op++) {
            const StdVideoEncodeH264RefListModEntry *entry = &ref_lists->pRefList0ModOperations[op];

            radv_enc_code_ue(cmd_buffer, entry->modification_of_pic_nums_idc);
            if (entry->modification_of_pic_nums_idc ==
                   STD_VIDEO_H264_MODIFICATION_OF_PIC_NUMS_IDC_SHORT_TERM_SUBTRACT ||
                entry->modification_of_pic_nums_idc == STD_VIDEO_H264_MODIFICATION_OF_PIC_NUMS_IDC_SHORT_TERM_ADD)
               radv_enc_code_ue(cmd_buffer, entry->abs_diff_pic_num_minus1);
            else if (entry->modification_of_pic_nums_idc == STD_VIDEO_H264_MODIFICATION_OF_PIC_NUMS_IDC_LONG_TERM)
               radv_enc_code_ue(cmd_buffer, entry->long_term_pic_num);
         }
      }

      if (pic->primary_pic_type == STD_VIDEO_H264_PICTURE_TYPE_B) {
         radv_enc_code_fixed_bits(cmd_buffer, ref_lists->flags.ref_pic_list_modification_flag_l1, 1);
         if (ref_lists->flags.ref_pic_list_modification_flag_l1) {
            for (unsigned op = 0; op < ref_lists->refList1ModOpCount; op++) {
               const StdVideoEncodeH264RefListModEntry *entry = &ref_lists->pRefList1ModOperations[op];

               radv_enc_code_ue(cmd_buffer, entry->modification_of_pic_nums_idc);
               if (entry->modification_of_pic_nums_idc ==
                      STD_VIDEO_H264_MODIFICATION_OF_PIC_NUMS_IDC_SHORT_TERM_SUBTRACT ||
                   entry->modification_of_pic_nums_idc == STD_VIDEO_H264_MODIFICATION_OF_PIC_NUMS_IDC_SHORT_TERM_ADD)
                  radv_enc_code_ue(cmd_buffer, entry->abs_diff_pic_num_minus1);
               else if (entry->modification_of_pic_nums_idc == STD_VIDEO_H264_MODIFICATION_OF_PIC_NUMS_IDC_LONG_TERM)
                  radv_enc_code_ue(cmd_buffer, entry->long_term_pic_num);
            }
         }
      }
   }

   if (pic->flags.IdrPicFlag) {
      radv_enc_code_fixed_bits(cmd_buffer, 0x0, 1);
      radv_enc_code_fixed_bits(cmd_buffer, pic->flags.long_term_reference_flag, 1); /* long_term_reference_flag */
   } else if (pic->flags.is_reference) {
      radv_enc_code_fixed_bits(cmd_buffer, ref_lists->refPicMarkingOpCount > 0 ? 1 : 0, 1);
      for (unsigned op = 0; op < ref_lists->refPicMarkingOpCount; op++) {
         const StdVideoEncodeH264RefPicMarkingEntry *entry = &ref_lists->pRefPicMarkingOperations[op];
         radv_enc_code_ue(cmd_buffer, entry->memory_management_control_operation);
         if (entry->memory_management_control_operation == STD_VIDEO_H264_MEM_MGMT_CONTROL_OP_UNMARK_SHORT_TERM ||
             entry->memory_management_control_operation == STD_VIDEO_H264_MEM_MGMT_CONTROL_OP_MARK_LONG_TERM)
            radv_enc_code_ue(cmd_buffer, entry->difference_of_pic_nums_minus1);
         if (entry->memory_management_control_operation == STD_VIDEO_H264_MEM_MGMT_CONTROL_OP_UNMARK_LONG_TERM)
            radv_enc_code_ue(cmd_buffer, entry->long_term_pic_num);
         if (entry->memory_management_control_operation == STD_VIDEO_H264_MEM_MGMT_CONTROL_OP_MARK_LONG_TERM ||
             entry->memory_management_control_operation == STD_VIDEO_H264_MEM_MGMT_CONTROL_OP_MARK_CURRENT_AS_LONG_TERM)
            radv_enc_code_ue(cmd_buffer, entry->long_term_frame_idx);
         if (entry->memory_management_control_operation == STD_VIDEO_H264_MEM_MGMT_CONTROL_OP_SET_MAX_LONG_TERM_INDEX)
            radv_enc_code_ue(cmd_buffer, entry->max_long_term_frame_idx_plus1);
         if (entry->memory_management_control_operation == STD_VIDEO_H264_MEM_MGMT_CONTROL_OP_END)
            break;
      }
   }

   if (pic->primary_pic_type != STD_VIDEO_H264_PICTURE_TYPE_IDR &&
       pic->primary_pic_type != STD_VIDEO_H264_PICTURE_TYPE_I && pps->flags.entropy_coding_mode_flag)
      radv_enc_code_ue(cmd_buffer, slice_info->pStdSliceHeader->cabac_init_idc);

   radv_enc_flush_headers(cmd_buffer);
   instruction[inst_index] = RENCODE_HEADER_INSTRUCTION_COPY;
   num_bits[inst_index] = enc->bits_output - bits_copied;
   bits_copied = enc->bits_output;
   inst_index++;

   instruction[inst_index] = RENCODE_H264_HEADER_INSTRUCTION_SLICE_QP_DELTA;
   inst_index++;

   if (pps->flags.deblocking_filter_control_present_flag) {
      radv_enc_code_ue(cmd_buffer, slice_info->pStdSliceHeader->disable_deblocking_filter_idc);
      if (!slice_info->pStdSliceHeader->disable_deblocking_filter_idc) {
         radv_enc_code_se(cmd_buffer, slice_info->pStdSliceHeader->slice_alpha_c0_offset_div2);
         radv_enc_code_se(cmd_buffer, slice_info->pStdSliceHeader->slice_beta_offset_div2);
      }
   }

   radv_enc_flush_headers(cmd_buffer);
   instruction[inst_index] = RENCODE_HEADER_INSTRUCTION_COPY;
   num_bits[inst_index] = enc->bits_output - bits_copied;
   bits_copied = enc->bits_output;
   inst_index++;

   instruction[inst_index] = RENCODE_HEADER_INSTRUCTION_END;

   cdw_filled = cs->cdw - cdw_start;
   for (int i = 0; i < RENCODE_SLICE_HEADER_TEMPLATE_MAX_TEMPLATE_SIZE_IN_DWORDS - cdw_filled; i++)
      radeon_emit(cs, 0x00000000);
   for (int j = 0; j < RENCODE_SLICE_HEADER_TEMPLATE_MAX_NUM_INSTRUCTIONS; j++) {
      radeon_emit(cs, instruction[j]);
      radeon_emit(cs, num_bits[j]);
   }
   ENC_END;
}

static void
radv_enc_slice_header_hevc(struct radv_cmd_buffer *cmd_buffer, const VkVideoEncodeInfoKHR *enc_info)
{
   struct radv_enc_state *enc = &cmd_buffer->video.enc;
   uint32_t instruction[RENCODE_SLICE_HEADER_TEMPLATE_MAX_NUM_INSTRUCTIONS] = {0};
   uint32_t num_bits[RENCODE_SLICE_HEADER_TEMPLATE_MAX_NUM_INSTRUCTIONS] = {0};
   const struct VkVideoEncodeH265PictureInfoKHR *h265_picture_info =
      vk_find_struct_const(enc_info->pNext, VIDEO_ENCODE_H265_PICTURE_INFO_KHR);
   const StdVideoEncodeH265PictureInfo *pic = h265_picture_info->pStdPictureInfo;
   const VkVideoEncodeH265NaluSliceSegmentInfoKHR *h265_slice = &h265_picture_info->pNaluSliceSegmentEntries[0];
   const StdVideoEncodeH265SliceSegmentHeader *slice = h265_slice->pStdSliceSegmentHeader;
   const StdVideoH265SequenceParameterSet *sps =
      vk_video_find_h265_enc_std_sps(&cmd_buffer->video.params->vk, pic->pps_seq_parameter_set_id);
   const StdVideoH265PictureParameterSet *pps =
      vk_video_find_h265_enc_std_pps(&cmd_buffer->video.params->vk, pic->pps_pic_parameter_set_id);
   unsigned int inst_index = 0;
   unsigned int cdw_start = 0;
   unsigned int cdw_filled = 0;
   unsigned int bits_copied = 0;
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   unsigned nal_unit_type = vk_video_get_h265_nal_unit(pic);

   ENC_BEGIN;
   radeon_emit(cs, pdev->vcn_enc_cmds.slice_header);
   radv_enc_reset(cmd_buffer);
   radv_enc_set_emulation_prevention(cmd_buffer, false);

   cdw_start = cs->cdw;
   radv_enc_code_fixed_bits(cmd_buffer, 0x0, 1);
   radv_enc_code_fixed_bits(cmd_buffer, nal_unit_type, 6);
   radv_enc_code_fixed_bits(cmd_buffer, 0x0, 6);
   radv_enc_code_fixed_bits(cmd_buffer, 0x1, 3);

   radv_enc_flush_headers(cmd_buffer);
   instruction[inst_index] = RENCODE_HEADER_INSTRUCTION_COPY;
   num_bits[inst_index] = enc->bits_output - bits_copied;
   bits_copied = enc->bits_output;
   inst_index++;

   instruction[inst_index] = RENCODE_HEVC_HEADER_INSTRUCTION_FIRST_SLICE;
   inst_index++;

   if ((nal_unit_type >= 16) && (nal_unit_type <= 23))
      radv_enc_code_fixed_bits(cmd_buffer, 0x0, 1);

   radv_enc_code_ue(cmd_buffer, pic->pps_pic_parameter_set_id);

   radv_enc_flush_headers(cmd_buffer);
   instruction[inst_index] = RENCODE_HEADER_INSTRUCTION_COPY;
   num_bits[inst_index] = enc->bits_output - bits_copied;
   bits_copied = enc->bits_output;
   inst_index++;

   instruction[inst_index] = RENCODE_HEVC_HEADER_INSTRUCTION_SLICE_SEGMENT;
   inst_index++;

   instruction[inst_index] = RENCODE_HEVC_HEADER_INSTRUCTION_DEPENDENT_SLICE_END;
   inst_index++;

   /* slice_type */
   switch (pic->pic_type) {
   case STD_VIDEO_H265_PICTURE_TYPE_I:
   case STD_VIDEO_H265_PICTURE_TYPE_IDR:
      radv_enc_code_ue(cmd_buffer, 0x2);
      break;
   case STD_VIDEO_H265_PICTURE_TYPE_P:
      radv_enc_code_ue(cmd_buffer, 0x1);
      break;
   case STD_VIDEO_H265_PICTURE_TYPE_B:
      radv_enc_code_ue(cmd_buffer, 0x0);
      break;
   default:
      radv_enc_code_ue(cmd_buffer, 0x1);
   }

   if ((nal_unit_type != 19) && nal_unit_type != 20) {
      /* slice_pic_order_cnt_lsb */
      radv_enc_code_fixed_bits(cmd_buffer, pic->PicOrderCntVal, sps->log2_max_pic_order_cnt_lsb_minus4 + 4);
      radv_enc_code_fixed_bits(cmd_buffer, pic->flags.short_term_ref_pic_set_sps_flag, 0x1);
      if (!pic->flags.short_term_ref_pic_set_sps_flag) {
         int st_rps_idx = sps->num_short_term_ref_pic_sets;
         const StdVideoH265ShortTermRefPicSet *rps = &pic->pShortTermRefPicSet[st_rps_idx];

         if (st_rps_idx != 0)
            radv_enc_code_fixed_bits(cmd_buffer, rps->flags.inter_ref_pic_set_prediction_flag, 0x1);

         if (rps->flags.inter_ref_pic_set_prediction_flag) {
            int ref_rps_idx = st_rps_idx - (rps->delta_idx_minus1 + 1);
            if (st_rps_idx == sps->num_short_term_ref_pic_sets)
               radv_enc_code_ue(cmd_buffer, rps->delta_idx_minus1);
            radv_enc_code_fixed_bits(cmd_buffer, rps->flags.delta_rps_sign, 0x1);
            radv_enc_code_ue(cmd_buffer, rps->abs_delta_rps_minus1);

            const StdVideoH265ShortTermRefPicSet *rps_ref = &sps->pShortTermRefPicSet[ref_rps_idx];
            int num_delta_pocs = rps_ref->num_negative_pics + rps_ref->num_positive_pics;
            for (int j = 0; j < num_delta_pocs; j++) {
               radv_enc_code_fixed_bits(cmd_buffer, !!(rps->used_by_curr_pic_flag & (1 << j)), 0x1);
               if (!(rps->used_by_curr_pic_flag & (1 << j))) {
                  radv_enc_code_fixed_bits(cmd_buffer, !!(rps->use_delta_flag & (1 << j)), 0x1);
               }
            }
         } else {
            radv_enc_code_ue(cmd_buffer, rps->num_negative_pics);
            radv_enc_code_ue(cmd_buffer, rps->num_positive_pics);

            for (int i = 0; i < rps->num_negative_pics; i++) {
               radv_enc_code_ue(cmd_buffer, rps->delta_poc_s0_minus1[i]);
               radv_enc_code_fixed_bits(cmd_buffer, !!(rps->used_by_curr_pic_s0_flag & (1 << i)), 0x1);
            }
            for (int i = 0; i < rps->num_positive_pics; i++) {
               radv_enc_code_ue(cmd_buffer, rps->delta_poc_s1_minus1[i]);
               radv_enc_code_fixed_bits(cmd_buffer, !!(rps->used_by_curr_pic_s1_flag & (1 << i)), 0x1);
            }
         }
      } else if (sps->num_short_term_ref_pic_sets > 1)
         radv_enc_code_ue(cmd_buffer, pic->short_term_ref_pic_set_idx);

      if (sps->flags.sps_temporal_mvp_enabled_flag)
         radv_enc_code_fixed_bits(cmd_buffer, pic->flags.slice_temporal_mvp_enabled_flag, 1);
   }

   if (sps->flags.sample_adaptive_offset_enabled_flag) {
      radv_enc_flush_headers(cmd_buffer);
      instruction[inst_index] = RENCODE_HEADER_INSTRUCTION_COPY;
      num_bits[inst_index] = enc->bits_output - bits_copied;
      bits_copied = enc->bits_output;
      inst_index++;

      instruction[inst_index] = RENCODE_HEVC_HEADER_INSTRUCTION_SAO_ENABLE;
      inst_index++;
   }

   if ((pic->pic_type == STD_VIDEO_H265_PICTURE_TYPE_P) || (pic->pic_type == STD_VIDEO_H265_PICTURE_TYPE_B)) {
      radv_enc_code_fixed_bits(cmd_buffer, slice->flags.num_ref_idx_active_override_flag, 1);
      if (slice->flags.num_ref_idx_active_override_flag) {
         radv_enc_code_ue(cmd_buffer, pic->pRefLists->num_ref_idx_l0_active_minus1);
         if (pic->pic_type == STD_VIDEO_H265_PICTURE_TYPE_B)
            radv_enc_code_ue(cmd_buffer, pic->pRefLists->num_ref_idx_l1_active_minus1);
      }
      if (pic->pic_type == STD_VIDEO_H265_PICTURE_TYPE_B)
         radv_enc_code_fixed_bits(cmd_buffer, slice->flags.mvd_l1_zero_flag, 1);
      if (pps->flags.cabac_init_present_flag)
         radv_enc_code_fixed_bits(cmd_buffer, slice->flags.cabac_init_flag, 1);
      if (pic->flags.slice_temporal_mvp_enabled_flag) {
         if (pic->pic_type == STD_VIDEO_H265_PICTURE_TYPE_B)
            radv_enc_code_fixed_bits(cmd_buffer, slice->flags.collocated_from_l0_flag, 1);
      }
      radv_enc_code_ue(cmd_buffer, 5 - slice->MaxNumMergeCand);
   }

   radv_enc_flush_headers(cmd_buffer);
   instruction[inst_index] = RENCODE_HEADER_INSTRUCTION_COPY;
   num_bits[inst_index] = enc->bits_output - bits_copied;
   bits_copied = enc->bits_output;
   inst_index++;

   instruction[inst_index] = RENCODE_HEVC_HEADER_INSTRUCTION_SLICE_QP_DELTA;
   inst_index++;

   if (pps->flags.pps_slice_chroma_qp_offsets_present_flag) {
      radv_enc_code_se(cmd_buffer, slice->slice_cb_qp_offset);
      radv_enc_code_se(cmd_buffer, slice->slice_cr_qp_offset);
   }

   if (pps->flags.pps_slice_act_qp_offsets_present_flag) {
      radv_enc_code_se(cmd_buffer, slice->slice_act_y_qp_offset);
      radv_enc_code_se(cmd_buffer, slice->slice_act_cb_qp_offset);
      radv_enc_code_se(cmd_buffer, slice->slice_act_cr_qp_offset);
   }

   if (pps->flags.chroma_qp_offset_list_enabled_flag) {
      radv_enc_code_fixed_bits(cmd_buffer, slice->flags.cu_chroma_qp_offset_enabled_flag, 1);
   }

   if (pps->flags.deblocking_filter_override_enabled_flag) {
      radv_enc_code_fixed_bits(cmd_buffer, slice->flags.deblocking_filter_override_flag, 1);
      if (slice->flags.deblocking_filter_override_flag) {
         radv_enc_code_fixed_bits(cmd_buffer, slice->flags.slice_deblocking_filter_disabled_flag, 1);
         if (!slice->flags.slice_deblocking_filter_disabled_flag) {
            radv_enc_code_se(cmd_buffer, slice->slice_beta_offset_div2);
            radv_enc_code_se(cmd_buffer, slice->slice_tc_offset_div2);
         }
      }
   }
   if ((pps->flags.pps_loop_filter_across_slices_enabled_flag) &&
       (!slice->flags.slice_deblocking_filter_disabled_flag || slice->flags.slice_sao_luma_flag ||
        slice->flags.slice_sao_chroma_flag)) {

      if (slice->flags.slice_sao_luma_flag || slice->flags.slice_sao_chroma_flag) {
         instruction[inst_index] = RENCODE_HEVC_HEADER_INSTRUCTION_LOOP_FILTER_ACROSS_SLICES_ENABLE;
         inst_index++;
      } else {
         radv_enc_code_fixed_bits(cmd_buffer, slice->flags.slice_loop_filter_across_slices_enabled_flag, 1);
         radv_enc_flush_headers(cmd_buffer);
         instruction[inst_index] = RENCODE_HEADER_INSTRUCTION_COPY;
         num_bits[inst_index] = enc->bits_output - bits_copied;
         bits_copied = enc->bits_output;
         inst_index++;
      }
   }

   instruction[inst_index] = RENCODE_HEADER_INSTRUCTION_END;

   cdw_filled = cs->cdw - cdw_start;
   for (int i = 0; i < RENCODE_SLICE_HEADER_TEMPLATE_MAX_TEMPLATE_SIZE_IN_DWORDS - cdw_filled; i++)
      radeon_emit(cs, 0x00000000);
   for (int j = 0; j < RENCODE_SLICE_HEADER_TEMPLATE_MAX_NUM_INSTRUCTIONS; j++) {
      radeon_emit(cs, instruction[j]);
      radeon_emit(cs, num_bits[j]);
   }
   ENC_END;
}

static void
radv_enc_ctx(struct radv_cmd_buffer *cmd_buffer, const VkVideoEncodeInfoKHR *info)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   struct radv_image_view *dpb_iv = NULL;
   struct radv_image *dpb = NULL;
   struct radv_image_plane *dpb_luma = NULL;
   struct radv_image_plane *dpb_chroma = NULL;
   uint64_t va = 0;
   uint32_t luma_pitch = 0;

   if (info->pSetupReferenceSlot)
      dpb_iv = radv_image_view_from_handle(info->pSetupReferenceSlot->pPictureResource->imageViewBinding);
   else if (info->referenceSlotCount > 0)
      dpb_iv = radv_image_view_from_handle(info->pReferenceSlots[0].pPictureResource->imageViewBinding);

   if (dpb_iv) {
      dpb = dpb_iv->image;
      dpb_luma = &dpb->planes[0];
      dpb_chroma = &dpb->planes[1];
      radv_cs_add_buffer(device->ws, cs, dpb->bindings[0].bo);
      va = radv_buffer_get_va(dpb->bindings[0].bo) + dpb->bindings[0].offset;     // TODO DPB resource
      luma_pitch = dpb_luma->surface.u.gfx9.surf_pitch * dpb_luma->surface.blk_w; // rec_luma_pitch
   }

   uint32_t swizzle_mode = 0;

   if (pdev->enc_hw_ver >= RADV_VIDEO_ENC_HW_4)
      swizzle_mode = RENCODE_REC_SWIZZLE_MODE_256B_D;
   else if (pdev->enc_hw_ver >= RADV_VIDEO_ENC_HW_2)
      swizzle_mode = RENCODE_REC_SWIZZLE_MODE_256B_S;
   ENC_BEGIN;
   radeon_emit(cs, pdev->vcn_enc_cmds.ctx);
   radeon_emit(cs, va >> 32);
   radeon_emit(cs, va & 0xffffffff);
   radeon_emit(cs, swizzle_mode);
   radeon_emit(cs, luma_pitch);                   // rec_luma_pitch
   radeon_emit(cs, luma_pitch);                   // rec_luma_pitch0); //rec_chromma_pitch
   radeon_emit(cs, info->referenceSlotCount + 1); // num_reconstructed_pictures

   int i;
   for (i = 0; i < info->referenceSlotCount + 1; i++) {
      radeon_emit(cs, dpb_luma ? dpb_luma->surface.u.gfx9.surf_offset + i * dpb_luma->surface.u.gfx9.surf_slice_size
                               : 0); // luma offset
      radeon_emit(cs, dpb_chroma
                         ? dpb_chroma->surface.u.gfx9.surf_offset + i * dpb_chroma->surface.u.gfx9.surf_slice_size
                         : 0); // chroma offset

      if (pdev->enc_hw_ver >= RADV_VIDEO_ENC_HW_4) {
         radeon_emit(cs, 0); /* unused offset 1 */
         radeon_emit(cs, 0); /* unused offset 2 */
      }
   }

   for (; i < RENCODE_MAX_NUM_RECONSTRUCTED_PICTURES; i++) {
      radeon_emit(cs, 0);
      radeon_emit(cs, 0);
      if (pdev->enc_hw_ver >= RADV_VIDEO_ENC_HW_4) {
         radeon_emit(cs, 0); /* unused offset 1 */
         radeon_emit(cs, 0); /* unused offset 2 */
      }
   }

   if (pdev->enc_hw_ver == RADV_VIDEO_ENC_HW_3) {
      radeon_emit(cs, 0); // colloc buffer offset
   }
   radeon_emit(cs, 0); // enc pic pre encode luma pitch
   radeon_emit(cs, 0); // enc pic pre encode chroma pitch

   for (i = 0; i < RENCODE_MAX_NUM_RECONSTRUCTED_PICTURES; i++) {
      radeon_emit(cs, 0); // pre encode luma offset
      radeon_emit(cs, 0); // pre encode chroma offset
      if (pdev->enc_hw_ver >= RADV_VIDEO_ENC_HW_4) {
         radeon_emit(cs, 0); /* unused offset 1 */
         radeon_emit(cs, 0); /* unused offset 2 */
      }
   }

   if (pdev->enc_hw_ver == RADV_VIDEO_ENC_HW_2) {
      radeon_emit(cs, 0); // enc pic yuv luma offset
      radeon_emit(cs, 0); // enc pic yuv chroma offset

      radeon_emit(cs, 0); // two pass search center map offset

      // rgboffsets
      radeon_emit(cs, 0); // red
      radeon_emit(cs, 0); // green
      radeon_emit(cs, 0); // blue
   } else if (pdev->enc_hw_ver >= RADV_VIDEO_ENC_HW_3) {
      radeon_emit(cs, 0); // red
      radeon_emit(cs, 0); // green
      radeon_emit(cs, 0); // blue
      radeon_emit(cs, 0); // v3 two pass search center map offset
      radeon_emit(cs, 0);
      if (pdev->enc_hw_ver == RADV_VIDEO_ENC_HW_3) {
         radeon_emit(cs, 0);
      }
   }
   ENC_END;
}

static void
radv_enc_bitstream(struct radv_cmd_buffer *cmd_buffer, struct radv_buffer *buffer, VkDeviceSize offset)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   uint64_t va = radv_buffer_get_va(buffer->bo) + buffer->offset;
   radv_cs_add_buffer(device->ws, cs, buffer->bo);

   ENC_BEGIN;
   radeon_emit(cs, pdev->vcn_enc_cmds.bitstream);
   radeon_emit(cs, RENCODE_REC_SWIZZLE_MODE_LINEAR);
   radeon_emit(cs, va >> 32);
   radeon_emit(cs, va & 0xffffffff);
   radeon_emit(cs, buffer->vk.size);
   radeon_emit(cs, offset);
   ENC_END;
}

static void
radv_enc_feedback(struct radv_cmd_buffer *cmd_buffer)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   ENC_BEGIN;

   radeon_emit(cs, pdev->vcn_enc_cmds.feedback);
   radeon_emit(cs, RENCODE_FEEDBACK_BUFFER_MODE_LINEAR);
   radeon_emit(cs, cmd_buffer->video.feedback_query_va >> 32);
   radeon_emit(cs, cmd_buffer->video.feedback_query_va & 0xffffffff);
   radeon_emit(cs, 16); // buffer_size
   radeon_emit(cs, 40); // data_size
   ENC_END;
}

static void
radv_enc_intra_refresh(struct radv_cmd_buffer *cmd_buffer)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   ENC_BEGIN;
   radeon_emit(cs, pdev->vcn_enc_cmds.intra_refresh);
   radeon_emit(cs, 0); // intra refresh mode
   radeon_emit(cs, 0); // intra ref offset
   radeon_emit(cs, 0); // intra region size
   ENC_END;
}

static void
radv_enc_rc_per_pic(struct radv_cmd_buffer *cmd_buffer, const VkVideoEncodeInfoKHR *enc_info,
                    rvcn_enc_rate_ctl_per_picture_t *per_pic)
{
   struct radv_video_session *vid = cmd_buffer->video.vid;
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radeon_cmdbuf *cs = cmd_buffer->cs;

   unsigned qp = per_pic->qp_i;

   if (vid->enc_rate_control_method == RENCODE_RATE_CONTROL_METHOD_NONE && !vid->enc_rate_control_default) {
      switch (vid->vk.op) {
      case VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR: {
         const struct VkVideoEncodeH264PictureInfoKHR *h264_picture_info =
            vk_find_struct_const(enc_info->pNext, VIDEO_ENCODE_H264_PICTURE_INFO_KHR);
         const VkVideoEncodeH264NaluSliceInfoKHR *h264_slice = &h264_picture_info->pNaluSliceEntries[0];
         qp = h264_slice->constantQp;
         break;
      }
      case VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR: {
         const struct VkVideoEncodeH265PictureInfoKHR *h265_picture_info =
            vk_find_struct_const(enc_info->pNext, VIDEO_ENCODE_H265_PICTURE_INFO_KHR);
         const VkVideoEncodeH265NaluSliceSegmentInfoKHR *h265_slice = &h265_picture_info->pNaluSliceSegmentEntries[0];
         qp = h265_slice->constantQp;
         break;
      }
      default:
         break;
      }
   }
   ENC_BEGIN;
   radeon_emit(cs, pdev->vcn_enc_cmds.rc_per_pic);
   radeon_emit(cs, qp);                           // qp_i
   radeon_emit(cs, qp);                           // qp_p
   radeon_emit(cs, qp);                           // qp_b
   radeon_emit(cs, per_pic->min_qp_i);
   radeon_emit(cs, per_pic->max_qp_i);
   radeon_emit(cs, per_pic->min_qp_p);
   radeon_emit(cs, per_pic->max_qp_p);
   radeon_emit(cs, per_pic->min_qp_b);
   radeon_emit(cs, per_pic->max_qp_b);
   radeon_emit(cs, per_pic->max_au_size_i);
   radeon_emit(cs, per_pic->max_au_size_p);
   radeon_emit(cs, per_pic->max_au_size_b);
   radeon_emit(cs, per_pic->enabled_filler_data);
   radeon_emit(cs, per_pic->skip_frame_enable);
   radeon_emit(cs, per_pic->enforce_hrd);
   radeon_emit(cs, 0xFFFFFFFF);                   // reserved_0xff
   ENC_END;
}

static void
radv_enc_params(struct radv_cmd_buffer *cmd_buffer, const VkVideoEncodeInfoKHR *enc_info)
{
   const struct VkVideoEncodeH264PictureInfoKHR *h264_picture_info =
      vk_find_struct_const(enc_info->pNext, VIDEO_ENCODE_H264_PICTURE_INFO_KHR);
   const struct VkVideoEncodeH265PictureInfoKHR *h265_picture_info =
      vk_find_struct_const(enc_info->pNext, VIDEO_ENCODE_H265_PICTURE_INFO_KHR);
   const StdVideoEncodeH264PictureInfo *h264_pic = h264_picture_info ? h264_picture_info->pStdPictureInfo : NULL;
   const StdVideoEncodeH265PictureInfo *h265_pic = h265_picture_info ? h265_picture_info->pStdPictureInfo : NULL;
   struct radv_image_view *src_iv = radv_image_view_from_handle(enc_info->srcPictureResource.imageViewBinding);
   struct radv_image *src_img = src_iv->image;
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   uint64_t va = radv_buffer_get_va(src_img->bindings[0].bo) + src_img->bindings[0].offset;
   uint64_t luma_va = va + src_img->planes[0].surface.u.gfx9.surf_offset;
   uint64_t chroma_va = va + src_img->planes[1].surface.u.gfx9.surf_offset;
   uint32_t pic_type;
   unsigned int slot_idx = 0xffffffff;

   radv_cs_add_buffer(device->ws, cs, src_img->bindings[0].bo);
   if (h264_pic) {
      switch (h264_pic->primary_pic_type) {
      case STD_VIDEO_H264_PICTURE_TYPE_P:
         slot_idx = enc_info->pReferenceSlots[0].slotIndex;
         pic_type = RENCODE_PICTURE_TYPE_P;
         break;
      case STD_VIDEO_H264_PICTURE_TYPE_B:
         slot_idx = enc_info->pReferenceSlots[0].slotIndex;
         pic_type = RENCODE_PICTURE_TYPE_B;
         break;
      case STD_VIDEO_H264_PICTURE_TYPE_I:
      case STD_VIDEO_H264_PICTURE_TYPE_IDR:
      default:
         pic_type = RENCODE_PICTURE_TYPE_I;
         break;
      }
      radv_enc_layer_select(cmd_buffer, h264_pic->temporal_id);
   } else if (h265_pic) {
      switch (h265_pic->pic_type) {
      case STD_VIDEO_H265_PICTURE_TYPE_P:
         slot_idx = enc_info->pReferenceSlots[0].slotIndex;
         pic_type = RENCODE_PICTURE_TYPE_P;
         break;
      case STD_VIDEO_H265_PICTURE_TYPE_B:
         slot_idx = enc_info->pReferenceSlots[0].slotIndex;
         pic_type = RENCODE_PICTURE_TYPE_B;
         break;
      case STD_VIDEO_H265_PICTURE_TYPE_I:
      case STD_VIDEO_H265_PICTURE_TYPE_IDR:
      default:
         pic_type = RENCODE_PICTURE_TYPE_I;
         break;
      }
      radv_enc_layer_select(cmd_buffer, h265_pic->TemporalId);
   } else {
      assert(0);
      return;
   }

   ENC_BEGIN;
   radeon_emit(cs, pdev->vcn_enc_cmds.enc_params);
   radeon_emit(cs, pic_type);                 // pic type
   radeon_emit(cs, enc_info->dstBufferRange); // allowed max bitstream size
   radeon_emit(cs, luma_va >> 32);
   radeon_emit(cs, luma_va & 0xffffffff);
   radeon_emit(cs, chroma_va >> 32);
   radeon_emit(cs, chroma_va & 0xffffffff);
   radeon_emit(cs, src_img->planes[0].surface.u.gfx9.surf_pitch);   // luma pitch
   radeon_emit(cs, src_img->planes[1].surface.u.gfx9.surf_pitch);   // chroma pitch
   radeon_emit(cs, src_img->planes[0].surface.u.gfx9.swizzle_mode); // swizzle mode
   radeon_emit(cs, slot_idx);                                       // ref0_idx

   if (enc_info->pSetupReferenceSlot)
      radeon_emit(cs, enc_info->pSetupReferenceSlot->slotIndex); // reconstructed picture index
   else
      radeon_emit(cs, 0);
   ENC_END;
}

static void
radv_enc_params_h264(struct radv_cmd_buffer *cmd_buffer)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   ENC_BEGIN;
   radeon_emit(cs, pdev->vcn_enc_cmds.enc_params_h264);

   if (pdev->enc_hw_ver < RADV_VIDEO_ENC_HW_3) {
      radeon_emit(cs, RENCODE_H264_PICTURE_STRUCTURE_FRAME);
      radeon_emit(cs, RENCODE_H264_INTERLACING_MODE_PROGRESSIVE);
      radeon_emit(cs, RENCODE_H264_PICTURE_STRUCTURE_FRAME);
      radeon_emit(cs, 0xffffffff); // reference_picture1_index
   } else {
      // V3
      radeon_emit(cs, RENCODE_H264_PICTURE_STRUCTURE_FRAME);
      radeon_emit(cs, 0); // input pic order cnt
      radeon_emit(cs, RENCODE_H264_INTERLACING_MODE_PROGRESSIVE);
      radeon_emit(cs, 0);          // l0 ref pic0 pic_type
      radeon_emit(cs, 0);          // l0 ref pic0 is long term
      radeon_emit(cs, 0);          // l0 ref pic0 picture structure
      radeon_emit(cs, 0);          // l0 ref pic0 pic order cnt
      radeon_emit(cs, 0xffffffff); // l0 ref pic1 index
      radeon_emit(cs, 0);          // l0 ref pic1 pic_type
      radeon_emit(cs, 0);          // l0 ref pic1 is long term
      radeon_emit(cs, 0);          // l0 ref pic1 picture structure
      radeon_emit(cs, 0);          // l0 ref pic1 pic order cnt
      radeon_emit(cs, 0xffffffff); // l1 ref pic0 index
      radeon_emit(cs, 0);          // l1 ref pic0 pic_type
      radeon_emit(cs, 0);          // l1 ref pic0 is long term
      radeon_emit(cs, 0);          // l1 ref pic0 picture structure
      radeon_emit(cs, 0);          // l1 ref pic0 pic order cnt
   }
   ENC_END;
}

static void
radv_enc_op_init(struct radv_cmd_buffer *cmd_buffer)
{
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   ENC_BEGIN;
   radeon_emit(cs, RENCODE_IB_OP_INITIALIZE);
   ENC_END;
}

static void
radv_enc_op_close(struct radv_cmd_buffer *cmd_buffer)
{
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   ENC_BEGIN;
   radeon_emit(cs, RENCODE_IB_OP_CLOSE_SESSION);
   ENC_END;
}

static void
radv_enc_op_enc(struct radv_cmd_buffer *cmd_buffer)
{
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   ENC_BEGIN;
   radeon_emit(cs, RENCODE_IB_OP_ENCODE);
   ENC_END;
}

static void
radv_enc_op_init_rc(struct radv_cmd_buffer *cmd_buffer)
{
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   ENC_BEGIN;
   radeon_emit(cs, RENCODE_IB_OP_INIT_RC);
   ENC_END;
}

static void
radv_enc_op_init_rc_vbv(struct radv_cmd_buffer *cmd_buffer)
{
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   ENC_BEGIN;
   radeon_emit(cs, RENCODE_IB_OP_INIT_RC_VBV_BUFFER_LEVEL);
   ENC_END;
}

static void
radv_enc_op_preset(struct radv_cmd_buffer *cmd_buffer, const VkVideoEncodeInfoKHR *enc_info)
{
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   struct radv_video_session *vid = cmd_buffer->video.vid;
   uint32_t preset_mode;

   switch (vid->vk.op) {
   case VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR: {
      const struct VkVideoEncodeH265PictureInfoKHR *h265_picture_info =
         vk_find_struct_const(enc_info->pNext, VIDEO_ENCODE_H265_PICTURE_INFO_KHR);
      const StdVideoEncodeH265PictureInfo *pic = h265_picture_info->pStdPictureInfo;
      const StdVideoH265SequenceParameterSet *sps =
         vk_video_find_h265_enc_std_sps(&cmd_buffer->video.params->vk, pic->pps_seq_parameter_set_id);
      if (sps->flags.sample_adaptive_offset_enabled_flag && vid->enc_preset_mode == RENCODE_PRESET_MODE_SPEED) {
         preset_mode = RENCODE_IB_OP_SET_BALANCE_ENCODING_MODE;
         return;
      }
      break;
   }
   default:
      break;
   }

   if (vid->enc_preset_mode == RENCODE_PRESET_MODE_QUALITY)
      preset_mode = RENCODE_IB_OP_SET_QUALITY_ENCODING_MODE;
   else if (vid->enc_preset_mode == RENCODE_PRESET_MODE_BALANCE)
      preset_mode = RENCODE_IB_OP_SET_BALANCE_ENCODING_MODE;
   else
      preset_mode = RENCODE_IB_OP_SET_SPEED_ENCODING_MODE;
   ENC_BEGIN;
   radeon_emit(cs, preset_mode);
   ENC_END;
}

static void
radv_enc_input_format(struct radv_cmd_buffer *cmd_buffer)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   struct radv_video_session *vid = cmd_buffer->video.vid;
   uint32_t color_bit_depth;
   uint32_t color_packing_format;

   switch (vid->vk.picture_format) {
   case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
      color_bit_depth = RENCODE_COLOR_BIT_DEPTH_8_BIT;
      color_packing_format = RENCODE_COLOR_PACKING_FORMAT_NV12;
      break;
   case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
      color_bit_depth = RENCODE_COLOR_BIT_DEPTH_10_BIT;
      color_packing_format = RENCODE_COLOR_PACKING_FORMAT_P010;
      break;
   default:
      assert(0);
      return;
   }

   ENC_BEGIN;
   radeon_emit(cs, pdev->vcn_enc_cmds.input_format);
   radeon_emit(cs, 0);                          // input color volume
   radeon_emit(cs, 0);                          // input color space
   radeon_emit(cs, RENCODE_COLOR_RANGE_STUDIO); // input color range
   radeon_emit(cs, 0);                          // input chroma subsampling
   radeon_emit(cs, 0);                          // input chroma location
   radeon_emit(cs, color_bit_depth);            // input color bit depth
   radeon_emit(cs, color_packing_format);       // input color packing format
   ENC_END;
}

static void
radv_enc_output_format(struct radv_cmd_buffer *cmd_buffer)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radeon_cmdbuf *cs = cmd_buffer->cs;
   struct radv_video_session *vid = cmd_buffer->video.vid;
   uint32_t color_bit_depth;

   switch (vid->vk.op) {
   case VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR:
      color_bit_depth = RENCODE_COLOR_BIT_DEPTH_8_BIT;
      break;
   case VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR:
      if (vid->vk.h265.profile_idc == STD_VIDEO_H265_PROFILE_IDC_MAIN_10)
         color_bit_depth = RENCODE_COLOR_BIT_DEPTH_10_BIT;
      else
         color_bit_depth = RENCODE_COLOR_BIT_DEPTH_8_BIT;
      break;
   default:
      assert(0);
      return;
   }

   ENC_BEGIN;
   radeon_emit(cs, pdev->vcn_enc_cmds.output_format);
   radeon_emit(cs, 0);                          // output color volume
   radeon_emit(cs, RENCODE_COLOR_RANGE_STUDIO); // output color range
   radeon_emit(cs, 0);                          // output chroma location
   radeon_emit(cs, color_bit_depth);            // output color bit depth
   ENC_END;
}

static void
radv_enc_headers_h264(struct radv_cmd_buffer *cmd_buffer, const VkVideoEncodeInfoKHR *enc_info)
{
   radv_enc_slice_header(cmd_buffer, enc_info);
   radv_enc_params(cmd_buffer, enc_info);
   radv_enc_params_h264(cmd_buffer);
}

static void
radv_enc_headers_hevc(struct radv_cmd_buffer *cmd_buffer, const VkVideoEncodeInfoKHR *enc_info)
{
   radv_enc_slice_header_hevc(cmd_buffer, enc_info);
   radv_enc_params(cmd_buffer, enc_info);
}

static void
begin(struct radv_cmd_buffer *cmd_buffer, const VkVideoEncodeInfoKHR *enc_info)
{
   struct radv_enc_state *enc = &cmd_buffer->video.enc;
   struct radv_video_session *vid = cmd_buffer->video.vid;

   radv_enc_session_info(cmd_buffer);
   cmd_buffer->video.enc.total_task_size = 0;
   radv_enc_task_info(cmd_buffer, false);
   radv_enc_op_init(cmd_buffer);
   radv_enc_session_init(cmd_buffer, enc_info);
   if (vid->vk.op == VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR) {
      radv_enc_slice_control(cmd_buffer, enc_info);
      radv_enc_spec_misc_h264(cmd_buffer, enc_info);
      radv_enc_deblocking_filter_h264(cmd_buffer, enc_info);
   } else if (vid->vk.op == VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR) {
      radv_enc_slice_control_hevc(cmd_buffer, enc_info);
      radv_enc_spec_misc_hevc(cmd_buffer, enc_info);
      radv_enc_deblocking_filter_hevc(cmd_buffer, enc_info);
   }
   radv_enc_layer_control(cmd_buffer, &vid->rc_layer_control);
   radv_enc_rc_session_init(cmd_buffer);
   radv_enc_quality_params(cmd_buffer);
   // temporal layers init
   unsigned i = 0;
   do {
      radv_enc_layer_select(cmd_buffer, i);
      radv_enc_rc_layer_init(cmd_buffer, &vid->rc_layer_init[i]);
      radv_enc_layer_select(cmd_buffer, i);
      radv_enc_rc_per_pic(cmd_buffer, enc_info, &vid->rc_per_pic[i]);
   } while (++i < vid->rc_layer_control.num_temporal_layers);
   radv_enc_op_init_rc(cmd_buffer);
   radv_enc_op_init_rc_vbv(cmd_buffer);
   radeon_emit_direct(cmd_buffer->cs, enc->task_size_offset, enc->total_task_size);
}

static void
destroy(struct radv_cmd_buffer *cmd_buffer)
{
   struct radv_enc_state *enc = &cmd_buffer->video.enc;
   radv_enc_session_info(cmd_buffer);
   cmd_buffer->video.enc.total_task_size = 0;
   radv_enc_task_info(cmd_buffer, false);
   radv_enc_op_close(cmd_buffer);
   radeon_emit_direct(cmd_buffer->cs, enc->task_size_offset, enc->total_task_size);
}

static void
radv_vcn_encode_video(struct radv_cmd_buffer *cmd_buffer, const VkVideoEncodeInfoKHR *enc_info)
{
   VK_FROM_HANDLE(radv_buffer, dst_buffer, enc_info->dstBuffer);
   struct radv_video_session *vid = cmd_buffer->video.vid;
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   struct radv_enc_state *enc = &cmd_buffer->video.enc;

   switch (vid->vk.op) {
   case VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR:
   case VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR:
      break;
   default:
      assert(0);
      return;
   }

   if (vid->enc_need_begin) {
      begin(cmd_buffer, enc_info);
      vid->enc_need_begin = false;
   }
   // before encode
   // session info
   radv_enc_session_info(cmd_buffer);

   cmd_buffer->video.enc.total_task_size = 0;
   // task info
   radv_enc_task_info(cmd_buffer, true);

   // temporal layers init
   unsigned i = 0;
   do {
      if (vid->enc_need_rate_control) {
         radv_enc_layer_select(cmd_buffer, i);
         radv_enc_rc_layer_init(cmd_buffer, &vid->rc_layer_init[i]);
         vid->enc_need_rate_control = false;
      }
      radv_enc_layer_select(cmd_buffer, i);
      radv_enc_rc_per_pic(cmd_buffer, enc_info, &vid->rc_per_pic[i]);
   } while (++i < vid->rc_layer_control.num_temporal_layers);

   // encode headers
   // ctx
   if (vid->vk.op == VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR) {
      radv_enc_headers_h264(cmd_buffer, enc_info);
      radv_enc_ctx(cmd_buffer, enc_info);
   } else if (vid->vk.op == VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR) {
      radv_enc_headers_hevc(cmd_buffer, enc_info);
      radv_enc_ctx(cmd_buffer, enc_info);
   }
   // bitstream
   radv_enc_bitstream(cmd_buffer, dst_buffer, enc_info->dstBufferOffset);

   // feedback
   radv_enc_feedback(cmd_buffer);

   // v2 encode statistics
   if (pdev->enc_hw_ver >= RADV_VIDEO_ENC_HW_2) {
   }
   // intra_refresh
   radv_enc_intra_refresh(cmd_buffer);
   // v2 input format
   if (pdev->enc_hw_ver >= RADV_VIDEO_ENC_HW_2) {
      radv_enc_input_format(cmd_buffer);
      radv_enc_output_format(cmd_buffer);
   }
   // v2 output format

   // op_preset
   radv_enc_op_preset(cmd_buffer, enc_info);
   // op_enc
   radv_enc_op_enc(cmd_buffer);

   radeon_emit_direct(cmd_buffer->cs, enc->task_size_offset, enc->total_task_size);

   destroy(cmd_buffer);
}

static void
set_rate_control_defaults(struct radv_video_session *vid)
{
   uint32_t frame_rate_den = 1, frame_rate_num = 30;
   vid->enc_rate_control_method = RENCODE_RATE_CONTROL_METHOD_NONE;
   vid->enc_vbv_buffer_level = 64;
   vid->rc_layer_control.num_temporal_layers = 1;
   vid->rc_layer_control.max_num_temporal_layers = 1;
   vid->rc_per_pic[0].qp_i = 26;
   vid->rc_per_pic[0].qp_p = 26;
   vid->rc_per_pic[0].qp_b = 26;
   vid->rc_per_pic[0].min_qp_i = 0;
   vid->rc_per_pic[0].max_qp_i = 51;
   vid->rc_per_pic[0].min_qp_p = 0;
   vid->rc_per_pic[0].max_qp_p = 51;
   vid->rc_per_pic[0].min_qp_b = 0;
   vid->rc_per_pic[0].max_qp_b = 51;
   vid->rc_per_pic[0].max_au_size_i = 0;
   vid->rc_per_pic[0].max_au_size_p = 0;
   vid->rc_per_pic[0].max_au_size_b = 0;
   vid->rc_per_pic[0].enabled_filler_data = 1;
   vid->rc_per_pic[0].skip_frame_enable = 0;
   vid->rc_per_pic[0].enforce_hrd = 1;
   vid->rc_layer_init[0].frame_rate_den = frame_rate_den;
   vid->rc_layer_init[0].frame_rate_num = frame_rate_num;
   vid->rc_layer_init[0].vbv_buffer_size = 20000000; // rate_control->virtualBufferSizeInMs;
   vid->rc_layer_init[0].target_bit_rate = 16000;
   vid->rc_layer_init[0].peak_bit_rate = 32000;
   vid->rc_layer_init[0].avg_target_bits_per_picture =
      radv_vcn_per_frame_integer(16000, frame_rate_den, frame_rate_num);
   vid->rc_layer_init[0].peak_bits_per_picture_integer =
      radv_vcn_per_frame_integer(32000, frame_rate_den, frame_rate_num);
   vid->rc_layer_init[0].peak_bits_per_picture_fractional =
      radv_vcn_per_frame_frac(32000, frame_rate_den, frame_rate_num);
   return;
}

void
radv_video_enc_control_video_coding(struct radv_cmd_buffer *cmd_buffer, const VkVideoCodingControlInfoKHR *control_info)
{
   struct radv_video_session *vid = cmd_buffer->video.vid;

   switch (vid->vk.op) {
   case VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR:
   case VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR:
      break;
   default:
      unreachable("Unsupported\n");
   }

   if (control_info->flags & VK_VIDEO_CODING_CONTROL_RESET_BIT_KHR) {
      set_rate_control_defaults(vid);
      vid->enc_need_begin = true;
   }

   if (control_info->flags & VK_VIDEO_CODING_CONTROL_ENCODE_RATE_CONTROL_BIT_KHR) {
      const VkVideoEncodeRateControlInfoKHR *rate_control = (VkVideoEncodeRateControlInfoKHR *)vk_find_struct_const(
         control_info->pNext, VIDEO_ENCODE_RATE_CONTROL_INFO_KHR);

      assert(rate_control);
      const VkVideoEncodeH264RateControlInfoKHR *h264_rate_control =
         (VkVideoEncodeH264RateControlInfoKHR *)vk_find_struct_const(rate_control->pNext,
                                                                     VIDEO_ENCODE_H264_RATE_CONTROL_INFO_KHR);
      const VkVideoEncodeH265RateControlInfoKHR *h265_rate_control =
         (VkVideoEncodeH265RateControlInfoKHR *)vk_find_struct_const(rate_control->pNext,
                                                                     VIDEO_ENCODE_H265_RATE_CONTROL_INFO_KHR);

      uint32_t rate_control_method = RENCODE_RATE_CONTROL_METHOD_NONE;

      vid->enc_rate_control_default = false;

      if (rate_control->rateControlMode == VK_VIDEO_ENCODE_RATE_CONTROL_MODE_DEFAULT_KHR) {
         vid->enc_rate_control_default = true;
         set_rate_control_defaults(vid);
      } else if (rate_control->rateControlMode == VK_VIDEO_ENCODE_RATE_CONTROL_MODE_CBR_BIT_KHR)
         rate_control_method = RENCODE_RATE_CONTROL_METHOD_CBR;
      else if (rate_control->rateControlMode == VK_VIDEO_ENCODE_RATE_CONTROL_MODE_VBR_BIT_KHR)
         rate_control_method = RENCODE_RATE_CONTROL_METHOD_PEAK_CONSTRAINED_VBR;

      vid->enc_need_rate_control = true;
      if (vid->enc_rate_control_method != rate_control_method)
         vid->enc_need_begin = true;

      vid->enc_rate_control_method = rate_control_method;

      if (rate_control->rateControlMode == VK_VIDEO_ENCODE_RATE_CONTROL_MODE_DEFAULT_KHR)
         return;

      if (h264_rate_control) {
         vid->rc_layer_control.max_num_temporal_layers = h264_rate_control->temporalLayerCount;
         vid->rc_layer_control.num_temporal_layers = h264_rate_control->temporalLayerCount;
      } else if (h265_rate_control) {
         vid->rc_layer_control.max_num_temporal_layers = h265_rate_control->subLayerCount;
         vid->rc_layer_control.num_temporal_layers = h265_rate_control->subLayerCount;
      }

      for (unsigned l = 0; l < rate_control->layerCount; l++) {
         const VkVideoEncodeRateControlLayerInfoKHR *layer = &rate_control->pLayers[l];
         const VkVideoEncodeH264RateControlLayerInfoKHR *h264_layer =
            (VkVideoEncodeH264RateControlLayerInfoKHR *)vk_find_struct_const(
               layer->pNext, VIDEO_ENCODE_H264_RATE_CONTROL_LAYER_INFO_KHR);
         const VkVideoEncodeH265RateControlLayerInfoKHR *h265_layer =
            (VkVideoEncodeH265RateControlLayerInfoKHR *)vk_find_struct_const(
               layer->pNext, VIDEO_ENCODE_H265_RATE_CONTROL_LAYER_INFO_KHR);
         uint32_t frame_rate_den, frame_rate_num;
         vid->rc_layer_init[l].target_bit_rate = layer->averageBitrate;
         vid->rc_layer_init[l].peak_bit_rate = layer->maxBitrate;
         frame_rate_den = layer->frameRateDenominator;
         frame_rate_num = layer->frameRateNumerator;
         radv_vcn_enc_invalid_frame_rate(&frame_rate_den, &frame_rate_num);
         vid->rc_layer_init[l].frame_rate_den = frame_rate_den;
         vid->rc_layer_init[l].frame_rate_num = frame_rate_num;
         vid->rc_layer_init[l].vbv_buffer_size =
            (rate_control->virtualBufferSizeInMs / 1000.) * layer->averageBitrate;
         vid->rc_layer_init[l].avg_target_bits_per_picture =
            radv_vcn_per_frame_integer(layer->averageBitrate, frame_rate_den, frame_rate_num);
         vid->rc_layer_init[l].peak_bits_per_picture_integer =
            radv_vcn_per_frame_integer(layer->maxBitrate, frame_rate_den, frame_rate_num);
         vid->rc_layer_init[l].peak_bits_per_picture_fractional =
            radv_vcn_per_frame_frac(layer->maxBitrate, frame_rate_den, frame_rate_num);

         if (h264_layer) {
            vid->rc_per_pic[l].min_qp_i = h264_layer->useMinQp ? h264_layer->minQp.qpI : 0;
            vid->rc_per_pic[l].min_qp_p = h264_layer->useMinQp ? h264_layer->minQp.qpP : 0;
            vid->rc_per_pic[l].min_qp_b = h264_layer->useMinQp ? h264_layer->minQp.qpB : 0;
            vid->rc_per_pic[l].max_qp_i = h264_layer->useMaxQp ? h264_layer->maxQp.qpI : 51;
            vid->rc_per_pic[l].max_qp_p = h264_layer->useMaxQp ? h264_layer->maxQp.qpP : 51;
            vid->rc_per_pic[l].max_qp_b = h264_layer->useMaxQp ? h264_layer->maxQp.qpB : 51;
            vid->rc_per_pic[l].max_au_size_i = h264_layer->useMaxFrameSize ? h264_layer->maxFrameSize.frameISize : 0;
            vid->rc_per_pic[l].max_au_size_p = h264_layer->useMaxFrameSize ? h264_layer->maxFrameSize.framePSize : 0;
            vid->rc_per_pic[l].max_au_size_b = h264_layer->useMaxFrameSize ? h264_layer->maxFrameSize.frameBSize : 0;
         } else if (h265_layer) {
            vid->rc_per_pic[l].min_qp_i = h265_layer->useMinQp ? h265_layer->minQp.qpI : 0;
            vid->rc_per_pic[l].min_qp_p = h265_layer->useMinQp ? h265_layer->minQp.qpP : 0;
            vid->rc_per_pic[l].min_qp_b = h265_layer->useMinQp ? h265_layer->minQp.qpB : 0;
            vid->rc_per_pic[l].max_qp_i = h265_layer->useMaxQp ? h265_layer->maxQp.qpI : 51;
            vid->rc_per_pic[l].max_qp_p = h265_layer->useMaxQp ? h265_layer->maxQp.qpP : 51;
            vid->rc_per_pic[l].max_qp_b = h265_layer->useMaxQp ? h265_layer->maxQp.qpB : 51;
            vid->rc_per_pic[l].max_au_size_i = h265_layer->useMaxFrameSize ? h265_layer->maxFrameSize.frameISize : 0;
            vid->rc_per_pic[l].max_au_size_p = h265_layer->useMaxFrameSize ? h265_layer->maxFrameSize.framePSize : 0;
            vid->rc_per_pic[l].max_au_size_b = h265_layer->useMaxFrameSize ? h265_layer->maxFrameSize.frameBSize : 0;
         }

         vid->rc_per_pic[l].enabled_filler_data = 1;
         vid->rc_per_pic[l].skip_frame_enable = 0;
         vid->rc_per_pic[l].enforce_hrd = 1;
      }

      if (rate_control->virtualBufferSizeInMs > 0)
         vid->enc_vbv_buffer_level =
            lroundf((float)rate_control->initialVirtualBufferSizeInMs / rate_control->virtualBufferSizeInMs * 64);
   }
}

VKAPI_ATTR void VKAPI_CALL
radv_CmdEncodeVideoKHR(VkCommandBuffer commandBuffer, const VkVideoEncodeInfoKHR *pEncodeInfo)
{
   VK_FROM_HANDLE(radv_cmd_buffer, cmd_buffer, commandBuffer);
   radv_vcn_encode_video(cmd_buffer, pEncodeInfo);
}

VKAPI_ATTR VkResult VKAPI_CALL
radv_GetPhysicalDeviceVideoEncodeQualityLevelPropertiesKHR(
   VkPhysicalDevice physicalDevice, const VkPhysicalDeviceVideoEncodeQualityLevelInfoKHR *pQualityLevelInfo,
   VkVideoEncodeQualityLevelPropertiesKHR *pQualityLevelProperties)
{
   return VK_SUCCESS;
}

void
radv_video_patch_encode_session_parameters(struct vk_video_session_parameters *params)
{
   switch (params->op) {
   case VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR:
      break;
   case VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR: {
      /*
       * AMD firmware requires these flags to be set in h265 with RC modes,
       * VCN 3 need 1.27 and VCN 4 needs 1.7 or newer to pass the CTS tests,
       * dEQP-VK.video.encode.h265_rc_*.
       */
      for (unsigned i = 0; i < params->h265_enc.h265_pps_count; i++) {
         params->h265_enc.h265_pps[i].base.flags.cu_qp_delta_enabled_flag = 1;
         params->h265_enc.h265_pps[i].base.diff_cu_qp_delta_depth = 0;
      }
      break;
   }
   default:
      break;
   }
}

VKAPI_ATTR VkResult VKAPI_CALL
radv_GetEncodedVideoSessionParametersKHR(VkDevice device,
                                         const VkVideoEncodeSessionParametersGetInfoKHR *pVideoSessionParametersInfo,
                                         VkVideoEncodeSessionParametersFeedbackInfoKHR *pFeedbackInfo,
                                         size_t *pDataSize, void *pData)
{
   VK_FROM_HANDLE(radv_video_session_params, templ, pVideoSessionParametersInfo->videoSessionParameters);
   size_t total_size = 0;
   size_t size_limit = 0;

   if (pData)
      size_limit = *pDataSize;

   switch (templ->vk.op) {
   case VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR: {
      const struct VkVideoEncodeH264SessionParametersGetInfoKHR *h264_get_info =
         vk_find_struct_const(pVideoSessionParametersInfo->pNext, VIDEO_ENCODE_H264_SESSION_PARAMETERS_GET_INFO_KHR);
      if (h264_get_info->writeStdSPS) {
         const StdVideoH264SequenceParameterSet *sps =
            vk_video_find_h264_enc_std_sps(&templ->vk, h264_get_info->stdSPSId);
         assert(sps);
         vk_video_encode_h264_sps(sps, size_limit, &total_size, pData);
      }
      if (h264_get_info->writeStdPPS) {
         const StdVideoH264PictureParameterSet *pps =
            vk_video_find_h264_enc_std_pps(&templ->vk, h264_get_info->stdPPSId);
         assert(pps);
         vk_video_encode_h264_pps(pps, templ->vk.h264_enc.profile_idc == STD_VIDEO_H264_PROFILE_IDC_HIGH, size_limit,
                                  &total_size, pData);
      }
      break;
   }
   case VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR: {
      const struct VkVideoEncodeH265SessionParametersGetInfoKHR *h265_get_info =
         vk_find_struct_const(pVideoSessionParametersInfo->pNext, VIDEO_ENCODE_H265_SESSION_PARAMETERS_GET_INFO_KHR);
      if (h265_get_info->writeStdVPS) {
         const StdVideoH265VideoParameterSet *vps = vk_video_find_h265_enc_std_vps(&templ->vk, h265_get_info->stdVPSId);
         assert(vps);
         vk_video_encode_h265_vps(vps, size_limit, &total_size, pData);
      }
      if (h265_get_info->writeStdSPS) {
         const StdVideoH265SequenceParameterSet *sps =
            vk_video_find_h265_enc_std_sps(&templ->vk, h265_get_info->stdSPSId);
         assert(sps);
         vk_video_encode_h265_sps(sps, size_limit, &total_size, pData);
      }
      if (h265_get_info->writeStdPPS) {
         const StdVideoH265PictureParameterSet *pps =
            vk_video_find_h265_enc_std_pps(&templ->vk, h265_get_info->stdPPSId);
         assert(pps);
         vk_video_encode_h265_pps(pps, size_limit, &total_size, pData);
      }
      break;
   }
   default:
      break;
   }

   *pDataSize = total_size;
   return VK_SUCCESS;
}

void
radv_video_enc_begin_coding(struct radv_cmd_buffer *cmd_buffer)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   radeon_check_space(device->ws, cmd_buffer->cs, 1024);

   if (pdev->enc_hw_ver >= RADV_VIDEO_ENC_HW_4)
      radv_vcn_sq_header(cmd_buffer->cs, &cmd_buffer->video.sq, true);
}

void
radv_video_enc_end_coding(struct radv_cmd_buffer *cmd_buffer)
{
   struct radv_device *device = radv_cmd_buffer_device(cmd_buffer);
   const struct radv_physical_device *pdev = radv_device_physical(device);
   if (pdev->enc_hw_ver >= RADV_VIDEO_ENC_HW_4)
      radv_vcn_sq_tail(cmd_buffer->cs, &cmd_buffer->video.sq);
}

#define VCN_ENC_SESSION_SIZE 128 * 1024

VkResult
radv_video_get_encode_session_memory_requirements(struct radv_device *device, struct radv_video_session *vid,
                                                  uint32_t *pMemoryRequirementsCount,
                                                  VkVideoSessionMemoryRequirementsKHR *pMemoryRequirements)
{
   struct radv_physical_device *pdev = radv_device_physical(device);
   uint32_t memory_type_bits = (1u << pdev->memory_properties.memoryTypeCount) - 1;

   VK_OUTARRAY_MAKE_TYPED(VkVideoSessionMemoryRequirementsKHR, out, pMemoryRequirements, pMemoryRequirementsCount);

   vk_outarray_append_typed(VkVideoSessionMemoryRequirementsKHR, &out, m)
   {
      m->memoryBindIndex = 0;
      m->memoryRequirements.size = VCN_ENC_SESSION_SIZE;
      m->memoryRequirements.alignment = 0;
      m->memoryRequirements.memoryTypeBits = memory_type_bits;
   }

   return vk_outarray_status(&out);
}
