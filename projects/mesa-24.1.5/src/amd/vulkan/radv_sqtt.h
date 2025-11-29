/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_SQTT_H
#define RADV_SQTT_H

#include "radv_device.h"

struct radv_cmd_buffer;
struct radv_dispatch_info;
struct radv_graphics_pipeline;

struct radv_barrier_data {
   union {
      struct {
         uint16_t depth_stencil_expand : 1;
         uint16_t htile_hiz_range_expand : 1;
         uint16_t depth_stencil_resummarize : 1;
         uint16_t dcc_decompress : 1;
         uint16_t fmask_decompress : 1;
         uint16_t fast_clear_eliminate : 1;
         uint16_t fmask_color_expand : 1;
         uint16_t init_mask_ram : 1;
         uint16_t reserved : 8;
      };
      uint16_t all;
   } layout_transitions;
};

/**
 * Value for the reason field of an RGP barrier start marker originating from
 * the Vulkan client (does not include PAL-defined values). (Table 15)
 */
enum rgp_barrier_reason {
   RGP_BARRIER_UNKNOWN_REASON = 0xFFFFFFFF,

   /* External app-generated barrier reasons, i.e. API synchronization
    * commands Range of valid values: [0x00000001 ... 0x7FFFFFFF].
    */
   RGP_BARRIER_EXTERNAL_CMD_PIPELINE_BARRIER = 0x00000001,
   RGP_BARRIER_EXTERNAL_RENDER_PASS_SYNC = 0x00000002,
   RGP_BARRIER_EXTERNAL_CMD_WAIT_EVENTS = 0x00000003,

   /* Internal barrier reasons, i.e. implicit synchronization inserted by
    * the Vulkan driver Range of valid values: [0xC0000000 ... 0xFFFFFFFE].
    */
   RGP_BARRIER_INTERNAL_BASE = 0xC0000000,
   RGP_BARRIER_INTERNAL_PRE_RESET_QUERY_POOL_SYNC = RGP_BARRIER_INTERNAL_BASE + 0,
   RGP_BARRIER_INTERNAL_POST_RESET_QUERY_POOL_SYNC = RGP_BARRIER_INTERNAL_BASE + 1,
   RGP_BARRIER_INTERNAL_GPU_EVENT_RECYCLE_STALL = RGP_BARRIER_INTERNAL_BASE + 2,
   RGP_BARRIER_INTERNAL_PRE_COPY_QUERY_POOL_RESULTS_SYNC = RGP_BARRIER_INTERNAL_BASE + 3
};

bool radv_is_instruction_timing_enabled(void);

bool radv_sqtt_queue_events_enabled(void);

void radv_emit_sqtt_userdata(const struct radv_cmd_buffer *cmd_buffer, const void *data, uint32_t num_dwords);

void radv_emit_spi_config_cntl(const struct radv_device *device, struct radeon_cmdbuf *cs, bool enable);

void radv_emit_inhibit_clockgating(const struct radv_device *device, struct radeon_cmdbuf *cs, bool inhibit);

VkResult radv_sqtt_acquire_gpu_timestamp(struct radv_device *device, struct radeon_winsys_bo **gpu_timestamp_bo,
                                         uint32_t *gpu_timestamp_offset, void **gpu_timestamp_ptr);

bool radv_sqtt_init(struct radv_device *device);

void radv_sqtt_finish(struct radv_device *device);

bool radv_begin_sqtt(struct radv_queue *queue);

bool radv_end_sqtt(struct radv_queue *queue);

bool radv_get_sqtt_trace(struct radv_queue *queue, struct ac_sqtt_trace *sqtt_trace);

void radv_reset_sqtt_trace(struct radv_device *device);

bool radv_sqtt_sample_clocks(struct radv_device *device);

VkResult radv_sqtt_get_timed_cmdbuf(struct radv_queue *queue, struct radeon_winsys_bo *timestamp_bo,
                                    uint32_t timestamp_offset, VkPipelineStageFlags2 timestamp_stage,
                                    VkCommandBuffer *pcmdbuf);

void radv_sqtt_emit_relocated_shaders(struct radv_cmd_buffer *cmd_buffer, struct radv_graphics_pipeline *pipeline);

void radv_write_user_event_marker(struct radv_cmd_buffer *cmd_buffer, enum rgp_sqtt_marker_user_event_type type,
                                  const char *str);

void radv_describe_begin_cmd_buffer(struct radv_cmd_buffer *cmd_buffer);

void radv_describe_end_cmd_buffer(struct radv_cmd_buffer *cmd_buffer);

void radv_describe_draw(struct radv_cmd_buffer *cmd_buffer);

void radv_describe_dispatch(struct radv_cmd_buffer *cmd_buffer, const struct radv_dispatch_info *info);

void radv_describe_begin_render_pass_clear(struct radv_cmd_buffer *cmd_buffer, VkImageAspectFlagBits aspects);

void radv_describe_end_render_pass_clear(struct radv_cmd_buffer *cmd_buffer);

void radv_describe_begin_render_pass_resolve(struct radv_cmd_buffer *cmd_buffer);

void radv_describe_end_render_pass_resolve(struct radv_cmd_buffer *cmd_buffer);

void radv_describe_barrier_end_delayed(struct radv_cmd_buffer *cmd_buffer);

void radv_describe_barrier_start(struct radv_cmd_buffer *cmd_buffer, enum rgp_barrier_reason reason);

void radv_describe_barrier_end(struct radv_cmd_buffer *cmd_buffer);

void radv_describe_layout_transition(struct radv_cmd_buffer *cmd_buffer, const struct radv_barrier_data *barrier);

void radv_describe_begin_accel_struct_build(struct radv_cmd_buffer *cmd_buffer, uint32_t count);

void radv_describe_end_accel_struct_build(struct radv_cmd_buffer *cmd_buffer);

#endif /* RADV_SQTT_H */
