/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_DEVICE_GENERATED_COMMANDS_H
#define RADV_DEVICE_GENERATED_COMMANDS_H

#include "vk_object.h"

#include "radv_constants.h"

struct radv_cmd_buffer;
struct radv_pipeline;

struct radv_indirect_command_layout {
   struct vk_object_base base;

   VkIndirectCommandsLayoutUsageFlagsNV flags;
   VkPipelineBindPoint pipeline_bind_point;

   uint32_t input_stride;
   uint32_t token_count;

   bool indexed;
   bool binds_index_buffer;
   bool draw_mesh_tasks;
   uint16_t draw_params_offset;
   uint16_t index_buffer_offset;

   uint16_t dispatch_params_offset;

   bool bind_pipeline;
   uint16_t pipeline_params_offset;

   uint32_t bind_vbo_mask;
   uint32_t vbo_offsets[MAX_VBS];

   uint64_t push_constant_mask;
   uint32_t push_constant_offsets[MAX_PUSH_CONSTANTS_SIZE / 4];
   uint32_t push_constant_size;
   uint32_t dynamic_offset_count;

   uint32_t ibo_type_32;
   uint32_t ibo_type_8;

   VkIndirectCommandsLayoutTokenNV tokens[0];
};

VK_DEFINE_NONDISP_HANDLE_CASTS(radv_indirect_command_layout, base, VkIndirectCommandsLayoutNV,
                               VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NV)

uint32_t radv_get_indirect_cmdbuf_size(const VkGeneratedCommandsInfoNV *cmd_info);

bool radv_use_dgc_predication(struct radv_cmd_buffer *cmd_buffer,
                              const VkGeneratedCommandsInfoNV *pGeneratedCommandsInfo);

bool radv_dgc_can_preprocess(const struct radv_indirect_command_layout *layout, struct radv_pipeline *pipeline);

void radv_prepare_dgc(struct radv_cmd_buffer *cmd_buffer, const VkGeneratedCommandsInfoNV *pGeneratedCommandsInfo,
                      bool cond_render_enabled);

#endif /* RADV_DEVICE_GENERATED_COMMANDS_H */
