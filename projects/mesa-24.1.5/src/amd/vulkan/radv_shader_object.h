/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_SHADER_OBJECT_H
#define RADV_SHADER_OBJECT_H

#include "radv_shader.h"

struct radv_shader_object {
   struct vk_object_base base;

   gl_shader_stage stage;

   VkShaderCodeTypeEXT code_type;

   /* Main shader */
   struct radv_shader *shader;
   struct radv_shader_binary *binary;

   /* Shader variants */
   /* VS before TCS */
   struct {
      struct radv_shader *shader;
      struct radv_shader_binary *binary;
   } as_ls;

   /* VS/TES before GS */
   struct {
      struct radv_shader *shader;
      struct radv_shader_binary *binary;
   } as_es;

   /* GS copy shader */
   struct {
      struct radv_shader *copy_shader;
      struct radv_shader_binary *copy_binary;
   } gs;

   uint32_t push_constant_size;
   uint32_t dynamic_offset_count;
};

VK_DEFINE_NONDISP_HANDLE_CASTS(radv_shader_object, base, VkShaderEXT, VK_OBJECT_TYPE_SHADER_EXT);

#endif /* RADV_SHADER_OBJECT_H */
