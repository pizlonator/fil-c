/*
 * Copyright (C) 2024 Collabora, Ltd.
 * SPDX-License-Identifier: MIT
 */

#include "compiler/nir/nir_builder.h"
#include "pan_ir.h"

/* Vertex shader gets passed image attribute descriptors through the
 * vertex attribute descriptor array. This forces us to apply an offset
 * to all image access to get the actual attribute offset.
 *
 * The gallium driver emits the vertex attributes on each draw, and puts
 * image attributes right after the vertex attributes, which implies passing
 * vs_img_attrib_offset = util_bitcount64(nir->info.inputs_read).
 *
 * The Vulkan driver, on the other hand, uses
 * VkVertexInputAttributeDescription to build a table of attributes passed
 * to the shader. While there's no reason for the app to define more
 * attributes than it actually uses in the vertex shader, it doesn't seem
 * to be disallowed either. Not to mention that vkCmdSetVertexInputEXT()
 * allows one to dynamically change the vertex input configuration, and
 * possibly pass more attributes than referenced by the vertex shader bound to
 * the command buffer at draw time. Of course, we could carry this information
 * at the pipeline level, and re-emit the attribute array, but emitting only
 * when the vertex input configuration is flagged dirty is simpler.
 * In order for this to work, we use a fixed image attribute offset.
 */
static bool
lower_image_intr(struct nir_builder *b, nir_intrinsic_instr *intr, void *data)
{
   if (intr->intrinsic != nir_intrinsic_image_load &&
       intr->intrinsic != nir_intrinsic_image_store)
      return false;

   unsigned img_attr_offset = *(unsigned *)data;
   nir_def *index = intr->src[0].ssa;

   b->cursor = nir_before_instr(&intr->instr);

   index = nir_iadd_imm(b, index, img_attr_offset);
   nir_src_rewrite(&intr->src[0], index);
   return true;
}

bool
pan_lower_image_index(nir_shader *shader, unsigned vs_img_attrib_offset)
{
   if (shader->info.stage != MESA_SHADER_VERTEX)
      return false;

   return nir_shader_intrinsics_pass(
      shader, lower_image_intr,
      nir_metadata_block_index | nir_metadata_dominance, &vs_img_attrib_offset);
}
