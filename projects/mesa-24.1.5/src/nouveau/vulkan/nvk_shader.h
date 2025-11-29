/*
 * Copyright © 2022 Collabora Ltd. and Red Hat Inc.
 * SPDX-License-Identifier: MIT
 */
#ifndef NVK_SHADER_H
#define NVK_SHADER_H 1

#include "nvk_private.h"
#include "nvk_device_memory.h"

#include "vk_pipeline_cache.h"

#include "nak.h"
#include "nir.h"
#include "nouveau_bo.h"

#include "vk_shader.h"

struct nak_shader_bin;
struct nvk_device;
struct nvk_physical_device;
struct nvk_pipeline_compilation_ctx;
struct vk_descriptor_set_layout;
struct vk_graphics_pipeline_state;
struct vk_pipeline_cache;
struct vk_pipeline_layout;
struct vk_pipeline_robustness_state;
struct vk_shader_module;

#define GF100_SHADER_HEADER_SIZE (20 * 4)
#define TU102_SHADER_HEADER_SIZE (32 * 4)
#define NVC0_MAX_SHADER_HEADER_SIZE TU102_SHADER_HEADER_SIZE

static inline uint32_t
nvk_cbuf_binding_for_stage(gl_shader_stage stage)
{
   return stage;
}

enum ENUM_PACKED nvk_cbuf_type {
   NVK_CBUF_TYPE_INVALID = 0,
   NVK_CBUF_TYPE_ROOT_DESC,
   NVK_CBUF_TYPE_SHADER_DATA,
   NVK_CBUF_TYPE_DESC_SET,
   NVK_CBUF_TYPE_DYNAMIC_UBO,
   NVK_CBUF_TYPE_UBO_DESC,
};

PRAGMA_DIAGNOSTIC_PUSH
PRAGMA_DIAGNOSTIC_ERROR(-Wpadded)
struct nvk_cbuf {
   enum nvk_cbuf_type type;
   uint8_t desc_set;
   uint8_t dynamic_idx;
   uint8_t _pad;
   uint32_t desc_offset;
};
PRAGMA_DIAGNOSTIC_POP
static_assert(sizeof(struct nvk_cbuf) == 8, "This struct has no holes");

struct nvk_cbuf_map {
   uint32_t cbuf_count;
   struct nvk_cbuf cbufs[16];
};

struct nvk_shader {
   struct vk_shader vk;

   struct nak_shader_info info;
   struct nvk_cbuf_map cbuf_map;

   /* Only relevant for fragment shaders */
   float min_sample_shading;

   struct nak_shader_bin *nak;
   const void *code_ptr;
   uint32_t code_size;

   const void *data_ptr;
   uint32_t data_size;

   uint32_t upload_size;
   uint64_t upload_addr;

   /* Address of the shader header (or start of the shader code) for compute
    * shaders.
    *
    * Prior to Volta, this is relative to the start of the shader heap. On
    * Volta and later, it's an absolute address.
    */
   uint64_t hdr_addr;

   /* Address of the start of the shader data section */
   uint64_t data_addr;
};

extern const struct vk_device_shader_ops nvk_device_shader_ops;

VkShaderStageFlags nvk_nak_stages(const struct nv_device_info *info);

uint64_t
nvk_physical_device_compiler_flags(const struct nvk_physical_device *pdev);

static inline nir_address_format
nvk_buffer_addr_format(VkPipelineRobustnessBufferBehaviorEXT robustness)
{
   switch (robustness) {
   case VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DISABLED_EXT:
      return nir_address_format_64bit_global_32bit_offset;
   case VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_EXT:
   case VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2_EXT:
      return nir_address_format_64bit_bounded_global;
   default:
      unreachable("Invalid robust buffer access behavior");
   }
}

bool
nvk_nir_lower_descriptors(nir_shader *nir,
                          const struct vk_pipeline_robustness_state *rs,
                          uint32_t set_layout_count,
                          struct vk_descriptor_set_layout * const *set_layouts,
                          struct nvk_cbuf_map *cbuf_map_out);
void
nvk_lower_nir(struct nvk_device *dev, nir_shader *nir,
              const struct vk_pipeline_robustness_state *rs,
              bool is_multiview,
              uint32_t set_layout_count,
              struct vk_descriptor_set_layout * const *set_layouts,
              struct nvk_cbuf_map *cbuf_map_out);

VkResult
nvk_shader_upload(struct nvk_device *dev, struct nvk_shader *shader);

/* Codegen wrappers.
 *
 * TODO: Delete these once NAK supports everything.
 */
uint64_t nvk_cg_get_prog_debug(void);
uint64_t nvk_cg_get_prog_optimize(void);

const nir_shader_compiler_options *
nvk_cg_nir_options(const struct nvk_physical_device *pdev,
                   gl_shader_stage stage);

void nvk_cg_preprocess_nir(nir_shader *nir);
void nvk_cg_optimize_nir(nir_shader *nir);

VkResult nvk_cg_compile_nir(struct nvk_physical_device *pdev, nir_shader *nir,
                            const struct nak_fs_key *fs_key,
                            struct nvk_shader *shader);

#endif
