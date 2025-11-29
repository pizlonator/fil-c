/*
 * Copyright © 2022 Collabora Ltd. and Red Hat Inc.
 * SPDX-License-Identifier: MIT
 */
#include "nvk_shader.h"

#include "nvk_cmd_buffer.h"
#include "nvk_descriptor_set_layout.h"
#include "nvk_device.h"
#include "nvk_physical_device.h"
#include "nvk_sampler.h"
#include "nvk_shader.h"

#include "vk_nir_convert_ycbcr.h"
#include "vk_pipeline.h"
#include "vk_pipeline_layout.h"
#include "vk_shader_module.h"
#include "vk_ycbcr_conversion.h"

#include "nak.h"
#include "nir.h"
#include "nir_builder.h"
#include "compiler/spirv/nir_spirv.h"

#include "nv50_ir_driver.h"

#include "util/mesa-sha1.h"
#include "util/u_debug.h"

#include "cla097.h"
#include "clb097.h"
#include "clc397.h"
#include "clc597.h"

static void
shared_var_info(const struct glsl_type *type, unsigned *size, unsigned *align)
{
   assert(glsl_type_is_vector_or_scalar(type));

   uint32_t comp_size = glsl_type_is_boolean(type) ? 4 : glsl_get_bit_size(type) / 8;
   unsigned length = glsl_get_vector_elements(type);
   *size = comp_size * length, *align = comp_size;
}

VkShaderStageFlags
nvk_nak_stages(const struct nv_device_info *info)
{
   const VkShaderStageFlags all =
      VK_SHADER_STAGE_VERTEX_BIT |
      VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT |
      VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT |
      VK_SHADER_STAGE_GEOMETRY_BIT |
      VK_SHADER_STAGE_FRAGMENT_BIT |
      VK_SHADER_STAGE_COMPUTE_BIT;

   const struct debug_control flags[] = {
      { "vs", BITFIELD64_BIT(MESA_SHADER_VERTEX) },
      { "tcs", BITFIELD64_BIT(MESA_SHADER_TESS_CTRL) },
      { "tes", BITFIELD64_BIT(MESA_SHADER_TESS_EVAL) },
      { "gs", BITFIELD64_BIT(MESA_SHADER_GEOMETRY) },
      { "fs", BITFIELD64_BIT(MESA_SHADER_FRAGMENT) },
      { "cs", BITFIELD64_BIT(MESA_SHADER_COMPUTE) },
      { "all", all },
      { NULL, 0 },
   };

   const char *env_str = getenv("NVK_USE_NAK");
   if (env_str == NULL)
      return info->cls_eng3d >= VOLTA_A ? all : 0;
   else
      return parse_debug_string(env_str, flags);
}

static bool
use_nak(const struct nvk_physical_device *pdev, gl_shader_stage stage)
{
   return nvk_nak_stages(&pdev->info) & mesa_to_vk_shader_stage(stage);
}

uint64_t
nvk_physical_device_compiler_flags(const struct nvk_physical_device *pdev)
{
   bool no_cbufs = pdev->debug_flags & NVK_DEBUG_NO_CBUF;
   uint64_t prog_debug = nvk_cg_get_prog_debug();
   uint64_t prog_optimize = nvk_cg_get_prog_optimize();
   uint64_t nak_stages = nvk_nak_stages(&pdev->info);
   uint64_t nak_flags = nak_debug_flags(pdev->nak);

   assert(prog_debug <= UINT8_MAX);
   assert(prog_optimize < 16);
   assert(nak_stages <= UINT32_MAX);
   assert(nak_flags <= UINT16_MAX);

   return prog_debug
      | (prog_optimize << 8)
      | ((uint64_t)no_cbufs << 12)
      | (nak_stages << 16)
      | (nak_flags << 48);
}

static const nir_shader_compiler_options *
nvk_get_nir_options(struct vk_physical_device *vk_pdev,
                    gl_shader_stage stage,
                    UNUSED const struct vk_pipeline_robustness_state *rs)
{
   const struct nvk_physical_device *pdev =
      container_of(vk_pdev, struct nvk_physical_device, vk);

   if (use_nak(pdev, stage))
      return nak_nir_options(pdev->nak);
   else
      return nvk_cg_nir_options(pdev, stage);
}

static struct spirv_to_nir_options
nvk_get_spirv_options(struct vk_physical_device *vk_pdev,
                      UNUSED gl_shader_stage stage,
                      const struct vk_pipeline_robustness_state *rs)
{
   const struct nvk_physical_device *pdev =
      container_of(vk_pdev, struct nvk_physical_device, vk);

   return (struct spirv_to_nir_options) {
      .caps = {
         .demote_to_helper_invocation = true,
         .descriptor_array_dynamic_indexing = true,
         .descriptor_array_non_uniform_indexing = true,
         .descriptor_indexing = true,
         .device_group = true,
         .draw_parameters = true,
         .float_controls = true,
         .float16 = true,
         .float64 = true,
         .fragment_barycentric = true,
         .geometry_streams = true,
         .image_atomic_int64 = true,
         .image_ms_array = true,
         .image_read_without_format = true,
         .image_write_without_format = true,
         .int8 = true,
         .int16 = true,
         .int64 = true,
         .int64_atomics = true,
         .min_lod = true,
         .multiview = true,
         .physical_storage_buffer_address = true,
         .runtime_descriptor_array = true,
         .shader_clock = true,
         .shader_sm_builtins_nv = true,
         .shader_viewport_index_layer = true,
         .sparse_residency = true,
         .storage_8bit = true,
         .storage_16bit = true,
         .storage_image_ms = true,
         .subgroup_arithmetic = true,
         .subgroup_ballot = true,
         .subgroup_basic = true,
         .subgroup_quad = true,
         .subgroup_rotate = true,
         .subgroup_shuffle = true,
         .subgroup_uniform_control_flow = true,
         .subgroup_vote = true,
         .tessellation = true,
         .transform_feedback = true,
         .variable_pointers = true,
         .vk_memory_model_device_scope = true,
         .vk_memory_model = true,
         .workgroup_memory_explicit_layout = true,
      },
      .ssbo_addr_format = nvk_buffer_addr_format(rs->storage_buffers),
      .phys_ssbo_addr_format = nir_address_format_64bit_global,
      .ubo_addr_format = nvk_buffer_addr_format(rs->uniform_buffers),
      .shared_addr_format = nir_address_format_32bit_offset,
      .min_ssbo_alignment = NVK_MIN_SSBO_ALIGNMENT,
      .min_ubo_alignment = nvk_min_cbuf_alignment(&pdev->info),
   };
}

static void
nvk_preprocess_nir(struct vk_physical_device *vk_pdev, nir_shader *nir)
{
   const struct nvk_physical_device *pdev =
      container_of(vk_pdev, struct nvk_physical_device, vk);

   NIR_PASS_V(nir, nir_lower_io_to_temporaries,
              nir_shader_get_entrypoint(nir), true, false);

   if (use_nak(pdev, nir->info.stage))
      nak_preprocess_nir(nir, pdev->nak);
   else
      nvk_cg_preprocess_nir(nir);
}

static void
nvk_populate_fs_key(struct nak_fs_key *key,
                    const struct vk_graphics_pipeline_state *state)
{
   memset(key, 0, sizeof(*key));

   key->sample_locations_cb = 0;
   key->sample_locations_offset = nvk_root_descriptor_offset(draw.sample_locations);

   if (state == NULL)
      return;

   if (state->pipeline_flags &
       VK_PIPELINE_CREATE_2_DEPTH_STENCIL_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT)
      key->zs_self_dep = true;

   /* We force per-sample interpolation whenever sampleShadingEnable is set
    * regardless of minSampleShading or rasterizationSamples.
    *
    * When sampleShadingEnable is set, few guarantees are made about the
    * location of interpolation of the inputs.  The only real guarantees are
    * that the inputs are interpolated within the pixel and that you get at
    * least `rasterizationSamples * minSampleShading` unique positions.
    * Importantly, it does not require that when `rasterizationSamples *
    * minSampleShading <= 1.0` that those positions are at the fragment
    * center.  Therefore, it's valid to just always do per-sample (which maps
    * to CENTROID on NVIDIA hardware) all the time and let the hardware sort
    * it out based on what we set in HYBRID_ANTI_ALIAS_CONTROL::passes.
    *
    * Also, we set HYBRID_ANTI_ALIAS_CONTROL::centroid at draw time based on
    * `rasterizationSamples * minSampleShading` so it should be per-pixel
    * whenever we're running only a single pass.  However, this would still be
    * correct even if it got interpolated at some other sample.
    *
    * The one caveat here is that we have to be careful about gl_SampleMaskIn.
    * When `nak_fs_key::force_sample_shading = true` we also turn any reads of
    * gl_SampleMaskIn into `1 << gl_SampleID` because the hardware sample mask
    * is actually per-fragment, not per-pass.  We handle this by smashing
    * minSampleShading to 1.0 whenever gl_SampleMaskIn is read.
    */
   const struct vk_multisample_state *ms = state->ms;
   if (ms != NULL && ms->sample_shading_enable)
      key->force_sample_shading = true;
}

static void
nvk_hash_graphics_state(struct vk_physical_device *device,
                        const struct vk_graphics_pipeline_state *state,
                        VkShaderStageFlags stages,
                        blake3_hash blake3_out)
{
   struct mesa_blake3 blake3_ctx;
   _mesa_blake3_init(&blake3_ctx);
   if (stages & VK_SHADER_STAGE_FRAGMENT_BIT) {
      struct nak_fs_key key;
      nvk_populate_fs_key(&key, state);
      _mesa_blake3_update(&blake3_ctx, &key, sizeof(key));

      const bool is_multiview = state->rp->view_mask != 0;
      _mesa_blake3_update(&blake3_ctx, &is_multiview, sizeof(is_multiview));
   }
   _mesa_blake3_final(&blake3_ctx, blake3_out);
}

static bool
lower_load_global_constant_offset_instr(nir_builder *b,
                                        nir_intrinsic_instr *intrin,
                                        UNUSED void *_data)
{
   if (intrin->intrinsic != nir_intrinsic_load_global_constant_offset &&
       intrin->intrinsic != nir_intrinsic_load_global_constant_bounded)
      return false;

   b->cursor = nir_before_instr(&intrin->instr);

   nir_def *base_addr = intrin->src[0].ssa;
   nir_def *offset = intrin->src[1].ssa;

   nir_def *zero = NULL;
   if (intrin->intrinsic == nir_intrinsic_load_global_constant_bounded) {
      nir_def *bound = intrin->src[2].ssa;

      unsigned bit_size = intrin->def.bit_size;
      assert(bit_size >= 8 && bit_size % 8 == 0);
      unsigned byte_size = bit_size / 8;

      zero = nir_imm_zero(b, intrin->num_components, bit_size);

      unsigned load_size = byte_size * intrin->num_components;

      nir_def *sat_offset =
         nir_umin(b, offset, nir_imm_int(b, UINT32_MAX - (load_size - 1)));
      nir_def *in_bounds =
         nir_ilt(b, nir_iadd_imm(b, sat_offset, load_size - 1), bound);

      nir_push_if(b, in_bounds);
   }

   nir_def *val =
      nir_build_load_global_constant(b, intrin->def.num_components,
                                     intrin->def.bit_size,
                                     nir_iadd(b, base_addr, nir_u2u64(b, offset)),
                                     .align_mul = nir_intrinsic_align_mul(intrin),
                                     .align_offset = nir_intrinsic_align_offset(intrin));

   if (intrin->intrinsic == nir_intrinsic_load_global_constant_bounded) {
      nir_pop_if(b, NULL);
      val = nir_if_phi(b, val, zero);
   }

   nir_def_rewrite_uses(&intrin->def, val);

   return true;
}

struct lower_ycbcr_state {
   uint32_t set_layout_count;
   struct vk_descriptor_set_layout * const *set_layouts;
};

static const struct vk_ycbcr_conversion_state *
lookup_ycbcr_conversion(const void *_state, uint32_t set,
                        uint32_t binding, uint32_t array_index)
{
   const struct lower_ycbcr_state *state = _state;
   assert(set < state->set_layout_count);
   assert(state->set_layouts[set] != NULL);
   const struct nvk_descriptor_set_layout *set_layout =
      vk_to_nvk_descriptor_set_layout(state->set_layouts[set]);
   assert(binding < set_layout->binding_count);

   const struct nvk_descriptor_set_binding_layout *bind_layout =
      &set_layout->binding[binding];

   if (bind_layout->immutable_samplers == NULL)
      return NULL;

   array_index = MIN2(array_index, bind_layout->array_size - 1);

   const struct nvk_sampler *sampler =
      bind_layout->immutable_samplers[array_index];

   return sampler && sampler->vk.ycbcr_conversion ?
          &sampler->vk.ycbcr_conversion->state : NULL;
}

static inline bool
nir_has_image_var(nir_shader *nir)
{
   nir_foreach_image_variable(_, nir)
      return true;

   return false;
}

void
nvk_lower_nir(struct nvk_device *dev, nir_shader *nir,
              const struct vk_pipeline_robustness_state *rs,
              bool is_multiview,
              uint32_t set_layout_count,
              struct vk_descriptor_set_layout * const *set_layouts,
              struct nvk_cbuf_map *cbuf_map_out)
{
   struct nvk_physical_device *pdev = nvk_device_physical(dev);

   if (nir->info.stage == MESA_SHADER_FRAGMENT) {
      NIR_PASS(_, nir, nir_lower_input_attachments,
               &(nir_input_attachment_options) {
                  .use_fragcoord_sysval = use_nak(pdev, nir->info.stage),
                  .use_layer_id_sysval = use_nak(pdev, nir->info.stage) ||
                                         is_multiview,
                  .use_view_id_for_layer = is_multiview,
               });
   }

   if (nir->info.stage == MESA_SHADER_TESS_EVAL) {
      NIR_PASS(_, nir, nir_lower_patch_vertices,
               nir->info.tess.tcs_vertices_out, NULL);
   }

   const struct lower_ycbcr_state ycbcr_state = {
      .set_layout_count = set_layout_count,
      .set_layouts = set_layouts,
   };
   NIR_PASS(_, nir, nir_vk_lower_ycbcr_tex,
            lookup_ycbcr_conversion, &ycbcr_state);

   nir_lower_compute_system_values_options csv_options = {
      .has_base_workgroup_id = true,
   };
   NIR_PASS(_, nir, nir_lower_compute_system_values, &csv_options);

   /* Lower push constants before lower_descriptors */
   NIR_PASS(_, nir, nir_lower_explicit_io, nir_var_mem_push_const,
            nir_address_format_32bit_offset);

   /* Lower non-uniform access before lower_descriptors */
   enum nir_lower_non_uniform_access_type lower_non_uniform_access_types =
      nir_lower_non_uniform_ubo_access;

   if (pdev->info.cls_eng3d < TURING_A) {
      lower_non_uniform_access_types |= nir_lower_non_uniform_texture_access |
                                        nir_lower_non_uniform_image_access;
   }

   /* In practice, most shaders do not have non-uniform-qualified accesses
    * thus a cheaper and likely to fail check is run first.
    */
   if (nir_has_non_uniform_access(nir, lower_non_uniform_access_types)) {
      struct nir_lower_non_uniform_access_options opts = {
         .types = lower_non_uniform_access_types,
         .callback = NULL,
      };
      NIR_PASS(_, nir, nir_opt_non_uniform_access);
      NIR_PASS(_, nir, nir_lower_non_uniform_access, &opts);
   }

   /* TODO: Kepler image lowering requires image params to be loaded from the
    * descriptor set which we don't currently support.
    */
   assert(pdev->info.cls_eng3d >= MAXWELL_A || !nir_has_image_var(nir));

   struct nvk_cbuf_map *cbuf_map = NULL;
   if (use_nak(pdev, nir->info.stage) &&
       !(pdev->debug_flags & NVK_DEBUG_NO_CBUF)) {
      cbuf_map = cbuf_map_out;

      /* Large constant support assumes cbufs */
      NIR_PASS(_, nir, nir_opt_large_constants, NULL, 32);
   } else {
      /* Codegen sometimes puts stuff in cbuf 1 and adds 1 to our cbuf indices
       * so we can't really rely on it for lowering to cbufs and instead place
       * the root descriptors in both cbuf 0 and cbuf 1.
       */
      *cbuf_map_out = (struct nvk_cbuf_map) {
         .cbuf_count = 2,
         .cbufs = {
            { .type = NVK_CBUF_TYPE_ROOT_DESC },
            { .type = NVK_CBUF_TYPE_ROOT_DESC },
         }
      };
   }

   NIR_PASS(_, nir, nvk_nir_lower_descriptors, rs,
            set_layout_count, set_layouts, cbuf_map);
   NIR_PASS(_, nir, nir_lower_explicit_io, nir_var_mem_global,
            nir_address_format_64bit_global);
   NIR_PASS(_, nir, nir_lower_explicit_io, nir_var_mem_ssbo,
            nvk_buffer_addr_format(rs->storage_buffers));
   NIR_PASS(_, nir, nir_lower_explicit_io, nir_var_mem_ubo,
            nvk_buffer_addr_format(rs->uniform_buffers));
   NIR_PASS(_, nir, nir_shader_intrinsics_pass,
            lower_load_global_constant_offset_instr, nir_metadata_none, NULL);

   if (!nir->info.shared_memory_explicit_layout) {
      NIR_PASS(_, nir, nir_lower_vars_to_explicit_types,
               nir_var_mem_shared, shared_var_info);
   }
   NIR_PASS(_, nir, nir_lower_explicit_io, nir_var_mem_shared,
            nir_address_format_32bit_offset);

   if (nir->info.zero_initialize_shared_memory && nir->info.shared_size > 0) {
      /* QMD::SHARED_MEMORY_SIZE requires an alignment of 256B so it's safe to
       * align everything up to 16B so we can write whole vec4s.
       */
      nir->info.shared_size = align(nir->info.shared_size, 16);
      NIR_PASS(_, nir, nir_zero_initialize_shared_memory,
               nir->info.shared_size, 16);

      /* We need to call lower_compute_system_values again because
       * nir_zero_initialize_shared_memory generates load_invocation_id which
       * has to be lowered to load_invocation_index.
       */
      NIR_PASS(_, nir, nir_lower_compute_system_values, NULL);
   }
}

#ifndef NDEBUG
static void
nvk_shader_dump(struct nvk_shader *shader)
{
   unsigned pos;

   if (shader->info.stage != MESA_SHADER_COMPUTE) {
      _debug_printf("dumping HDR for %s shader\n",
                    _mesa_shader_stage_to_string(shader->info.stage));
      for (pos = 0; pos < ARRAY_SIZE(shader->info.hdr); ++pos)
         _debug_printf("HDR[%02"PRIxPTR"] = 0x%08x\n",
                      pos * sizeof(shader->info.hdr[0]), shader->info.hdr[pos]);
   }
   _debug_printf("shader binary code (0x%x bytes):", shader->code_size);
   for (pos = 0; pos < shader->code_size / 4; ++pos) {
      if ((pos % 8) == 0)
         _debug_printf("\n");
      _debug_printf("%08x ", ((const uint32_t *)shader->code_ptr)[pos]);
   }
   _debug_printf("\n");
}
#endif

static VkResult
nvk_compile_nir_with_nak(struct nvk_physical_device *pdev,
                         nir_shader *nir,
                         VkShaderCreateFlagsEXT shader_flags,
                         const struct vk_pipeline_robustness_state *rs,
                         const struct nak_fs_key *fs_key,
                         struct nvk_shader *shader)
{
   const bool dump_asm =
      shader_flags & VK_SHADER_CREATE_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_MESA;

   nir_variable_mode robust2_modes = 0;
   if (rs->uniform_buffers == VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2_EXT)
      robust2_modes |= nir_var_mem_ubo;
   if (rs->storage_buffers == VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2_EXT)
      robust2_modes |= nir_var_mem_ssbo;

   shader->nak = nak_compile_shader(nir, dump_asm, pdev->nak, robust2_modes, fs_key);
   shader->info = shader->nak->info;
   shader->code_ptr = shader->nak->code;
   shader->code_size = shader->nak->code_size;

   return VK_SUCCESS;
}

static VkResult
nvk_compile_nir(struct nvk_device *dev, nir_shader *nir,
                VkShaderCreateFlagsEXT shader_flags,
                const struct vk_pipeline_robustness_state *rs,
                const struct nak_fs_key *fs_key,
                struct nvk_shader *shader)
{
   struct nvk_physical_device *pdev = nvk_device_physical(dev);
   VkResult result;

   if (use_nak(pdev, nir->info.stage)) {
      result = nvk_compile_nir_with_nak(pdev, nir, shader_flags, rs,
                                       fs_key, shader);
   } else {
      result = nvk_cg_compile_nir(pdev, nir, fs_key, shader);
   }
   if (result != VK_SUCCESS)
      return result;

   if (nir->constant_data_size > 0) {
      uint32_t data_align = nvk_min_cbuf_alignment(&pdev->info);
      uint32_t data_size = align(nir->constant_data_size, data_align);

      void *data = malloc(data_size);
      if (data == NULL)
         return vk_error(dev, VK_ERROR_OUT_OF_HOST_MEMORY);

      memcpy(data, nir->constant_data, nir->constant_data_size);

      assert(nir->constant_data_size <= data_size);
      memset(data + nir->constant_data_size, 0,
             data_size - nir->constant_data_size);

      shader->data_ptr = data;
      shader->data_size = data_size;
   }

   return VK_SUCCESS;
}

VkResult
nvk_shader_upload(struct nvk_device *dev, struct nvk_shader *shader)
{
   struct nvk_physical_device *pdev = nvk_device_physical(dev);

   uint32_t hdr_size = 0;
   if (shader->info.stage != MESA_SHADER_COMPUTE) {
      if (pdev->info.cls_eng3d >= TURING_A)
         hdr_size = TU102_SHADER_HEADER_SIZE;
      else
         hdr_size = GF100_SHADER_HEADER_SIZE;
   }

   /* Fermi   needs 0x40 alignment
    * Kepler+ needs the first instruction to be 0x80 aligned, so we waste 0x30 bytes
    */
   int alignment = pdev->info.cls_eng3d >= KEPLER_A ? 0x80 : 0x40;

   uint32_t total_size = 0;
   if (pdev->info.cls_eng3d >= KEPLER_A &&
       pdev->info.cls_eng3d < TURING_A &&
       hdr_size > 0) {
      /* The instructions are what has to be aligned so we need to start at a
       * small offset (0x30 B) into the upload area.
       */
      total_size = alignment - hdr_size;
   }

   const uint32_t hdr_offset = total_size;
   total_size += hdr_size;

   const uint32_t code_offset = total_size;
   assert(code_offset % alignment == 0);
   total_size += shader->code_size;

   uint32_t data_offset = 0;
   if (shader->data_size > 0) {
      total_size = align(total_size, nvk_min_cbuf_alignment(&pdev->info));
      data_offset = total_size;
      total_size += shader->data_size;
   }

   char *data = malloc(total_size);
   if (data == NULL)
      return vk_error(dev, VK_ERROR_OUT_OF_HOST_MEMORY);

   assert(hdr_size <= sizeof(shader->info.hdr));
   memcpy(data + hdr_offset, shader->info.hdr, hdr_size);
   memcpy(data + code_offset, shader->code_ptr, shader->code_size);
   if (shader->data_size > 0)
      memcpy(data + data_offset, shader->data_ptr, shader->data_size);

#ifndef NDEBUG
   if (debug_get_bool_option("NV50_PROG_DEBUG", false))
      nvk_shader_dump(shader);
#endif

   VkResult result = nvk_heap_upload(dev, &dev->shader_heap, data,
                                     total_size, alignment,
                                     &shader->upload_addr);
   if (result == VK_SUCCESS) {
      shader->upload_size = total_size;

      shader->hdr_addr = shader->upload_addr + hdr_offset;
      if (pdev->info.cls_eng3d < VOLTA_A) {
         const uint64_t heap_base_addr =
            nvk_heap_contiguous_base_address(&dev->shader_heap);
         assert(shader->upload_addr - heap_base_addr < UINT32_MAX);
         shader->hdr_addr -= heap_base_addr;
      }
      shader->data_addr = shader->upload_addr + data_offset;
   }
   free(data);

   return result;
}

static const struct vk_shader_ops nvk_shader_ops;

static void
nvk_shader_destroy(struct vk_device *vk_dev,
                   struct vk_shader *vk_shader,
                   const VkAllocationCallbacks* pAllocator)
{
   struct nvk_device *dev = container_of(vk_dev, struct nvk_device, vk);
   struct nvk_shader *shader = container_of(vk_shader, struct nvk_shader, vk);

   if (shader->upload_size > 0) {
      nvk_heap_free(dev, &dev->shader_heap,
                    shader->upload_addr,
                    shader->upload_size);
   }

   if (shader->nak) {
      nak_shader_bin_destroy(shader->nak);
   } else {
      /* This came from codegen or deserialize, just free it */
      free((void *)shader->code_ptr);
   }

   free((void *)shader->data_ptr);

   vk_shader_free(&dev->vk, pAllocator, &shader->vk);
}

static VkResult
nvk_compile_shader(struct nvk_device *dev,
                   struct vk_shader_compile_info *info,
                   const struct vk_graphics_pipeline_state *state,
                   const VkAllocationCallbacks* pAllocator,
                   struct vk_shader **shader_out)
{
   struct nvk_shader *shader;
   VkResult result;

   /* We consume the NIR, regardless of success or failure */
   nir_shader *nir = info->nir;

   shader = vk_shader_zalloc(&dev->vk, &nvk_shader_ops, info->stage,
                             pAllocator, sizeof(*shader));
   if (shader == NULL) {
      ralloc_free(nir);
      return vk_error(dev, VK_ERROR_OUT_OF_HOST_MEMORY);
   }

   /* TODO: Multiview with ESO */
   const bool is_multiview = state && state->rp->view_mask != 0;

   nvk_lower_nir(dev, nir, info->robustness, is_multiview,
                 info->set_layout_count, info->set_layouts,
                 &shader->cbuf_map);

   struct nak_fs_key fs_key_tmp, *fs_key = NULL;
   if (nir->info.stage == MESA_SHADER_FRAGMENT) {
      nvk_populate_fs_key(&fs_key_tmp, state);
      fs_key = &fs_key_tmp;
   }

   result = nvk_compile_nir(dev, nir, info->flags, info->robustness,
                            fs_key, shader);
   ralloc_free(nir);
   if (result != VK_SUCCESS) {
      nvk_shader_destroy(&dev->vk, &shader->vk, pAllocator);
      return result;
   }

   result = nvk_shader_upload(dev, shader);
   if (result != VK_SUCCESS) {
      nvk_shader_destroy(&dev->vk, &shader->vk, pAllocator);
      return result;
   }

   if (info->stage == MESA_SHADER_FRAGMENT) {
      if (shader->info.fs.reads_sample_mask ||
          shader->info.fs.uses_sample_shading) {
         shader->min_sample_shading = 1;
      } else if (state != NULL && state->ms != NULL &&
                 state->ms->sample_shading_enable) {
         shader->min_sample_shading =
            CLAMP(state->ms->min_sample_shading, 0, 1);
      } else {
         shader->min_sample_shading = 0;
      }
   }

   *shader_out = &shader->vk;

   return VK_SUCCESS;
}

static VkResult
nvk_compile_shaders(struct vk_device *vk_dev,
                    uint32_t shader_count,
                    struct vk_shader_compile_info *infos,
                    const struct vk_graphics_pipeline_state *state,
                    const VkAllocationCallbacks* pAllocator,
                    struct vk_shader **shaders_out)
{
   struct nvk_device *dev = container_of(vk_dev, struct nvk_device, vk);

   for (uint32_t i = 0; i < shader_count; i++) {
      VkResult result = nvk_compile_shader(dev, &infos[i], state,
                                           pAllocator, &shaders_out[i]);
      if (result != VK_SUCCESS) {
         /* Clean up all the shaders before this point */
         for (uint32_t j = 0; j < i; j++)
            nvk_shader_destroy(&dev->vk, shaders_out[j], pAllocator);

         /* Clean up all the NIR after this point */
         for (uint32_t j = i + 1; j < shader_count; j++)
            ralloc_free(infos[j].nir);

         /* Memset the output array */
         memset(shaders_out, 0, shader_count * sizeof(*shaders_out));

         return result;
      }
   }

   return VK_SUCCESS;
}

static VkResult
nvk_deserialize_shader(struct vk_device *vk_dev,
                       struct blob_reader *blob,
                       uint32_t binary_version,
                       const VkAllocationCallbacks* pAllocator,
                       struct vk_shader **shader_out)
{
   struct nvk_device *dev = container_of(vk_dev, struct nvk_device, vk);
   struct nvk_shader *shader;
   VkResult result;

   struct nak_shader_info info;
   blob_copy_bytes(blob, &info, sizeof(info));

   struct nvk_cbuf_map cbuf_map;
   blob_copy_bytes(blob, &cbuf_map, sizeof(cbuf_map));

   float min_sample_shading;
   blob_copy_bytes(blob, &min_sample_shading, sizeof(min_sample_shading));

   const uint32_t code_size = blob_read_uint32(blob);
   const uint32_t data_size = blob_read_uint32(blob);
   if (blob->overrun)
      return vk_error(dev, VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT);

   shader = vk_shader_zalloc(&dev->vk, &nvk_shader_ops, info.stage,
                             pAllocator, sizeof(*shader));
   if (shader == NULL)
      return vk_error(dev, VK_ERROR_OUT_OF_HOST_MEMORY);

   shader->info = info;
   shader->cbuf_map = cbuf_map;
   shader->min_sample_shading = min_sample_shading;
   shader->code_size = code_size;
   shader->data_size = data_size;

   shader->code_ptr = malloc(code_size);
   if (shader->code_ptr == NULL) {
      nvk_shader_destroy(&dev->vk, &shader->vk, pAllocator);
      return vk_error(dev, VK_ERROR_OUT_OF_HOST_MEMORY);
   }

   shader->data_ptr = malloc(data_size);
   if (shader->data_ptr == NULL) {
      nvk_shader_destroy(&dev->vk, &shader->vk, pAllocator);
      return vk_error(dev, VK_ERROR_OUT_OF_HOST_MEMORY);
   }

   blob_copy_bytes(blob, (void *)shader->code_ptr, shader->code_size);
   blob_copy_bytes(blob, (void *)shader->data_ptr, shader->data_size);
   if (blob->overrun) {
      nvk_shader_destroy(&dev->vk, &shader->vk, pAllocator);
      return vk_error(dev, VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT);
   }

   result = nvk_shader_upload(dev, shader);
   if (result != VK_SUCCESS) {
      nvk_shader_destroy(&dev->vk, &shader->vk, pAllocator);
      return result;
   }

   *shader_out = &shader->vk;

   return VK_SUCCESS;
}

static bool
nvk_shader_serialize(struct vk_device *vk_dev,
                     const struct vk_shader *vk_shader,
                     struct blob *blob)
{
   struct nvk_shader *shader = container_of(vk_shader, struct nvk_shader, vk);

   /* We can't currently cache assmbly */
   if (shader->nak != NULL && shader->nak->asm_str != NULL)
      return false;

   blob_write_bytes(blob, &shader->info, sizeof(shader->info));
   blob_write_bytes(blob, &shader->cbuf_map, sizeof(shader->cbuf_map));
   blob_write_bytes(blob, &shader->min_sample_shading,
                    sizeof(shader->min_sample_shading));

   blob_write_uint32(blob, shader->code_size);
   blob_write_uint32(blob, shader->data_size);
   blob_write_bytes(blob, shader->code_ptr, shader->code_size);
   blob_write_bytes(blob, shader->data_ptr, shader->data_size);

   return !blob->out_of_memory;
}

#define WRITE_STR(field, ...) ({                               \
   memset(field, 0, sizeof(field));                            \
   UNUSED int i = snprintf(field, sizeof(field), __VA_ARGS__); \
   assert(i > 0 && i < sizeof(field));                         \
})

static VkResult
nvk_shader_get_executable_properties(
   UNUSED struct vk_device *device,
   const struct vk_shader *vk_shader,
   uint32_t *executable_count,
   VkPipelineExecutablePropertiesKHR *properties)
{
   struct nvk_shader *shader = container_of(vk_shader, struct nvk_shader, vk);
   VK_OUTARRAY_MAKE_TYPED(VkPipelineExecutablePropertiesKHR, out,
                          properties, executable_count);

   vk_outarray_append_typed(VkPipelineExecutablePropertiesKHR, &out, props) {
      props->stages = mesa_to_vk_shader_stage(shader->info.stage);
      props->subgroupSize = 32;
      WRITE_STR(props->name, "%s",
                _mesa_shader_stage_to_string(shader->info.stage));
      WRITE_STR(props->description, "%s shader",
                _mesa_shader_stage_to_string(shader->info.stage));
   }

   return vk_outarray_status(&out);
}

static VkResult
nvk_shader_get_executable_statistics(
   UNUSED struct vk_device *device,
   const struct vk_shader *vk_shader,
   uint32_t executable_index,
   uint32_t *statistic_count,
   VkPipelineExecutableStatisticKHR *statistics)
{
   struct nvk_shader *shader = container_of(vk_shader, struct nvk_shader, vk);
   VK_OUTARRAY_MAKE_TYPED(VkPipelineExecutableStatisticKHR, out,
                          statistics, statistic_count);

   assert(executable_index == 0);

   vk_outarray_append_typed(VkPipelineExecutableStatisticKHR, &out, stat) {
      WRITE_STR(stat->name, "Code Size");
      WRITE_STR(stat->description,
                "Size of the compiled shader binary, in bytes");
      stat->format = VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR;
      stat->value.u64 = shader->code_size;
   }

   vk_outarray_append_typed(VkPipelineExecutableStatisticKHR, &out, stat) {
      WRITE_STR(stat->name, "Number of GPRs");
      WRITE_STR(stat->description, "Number of GPRs used by this pipeline");
      stat->format = VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR;
      stat->value.u64 = shader->info.num_gprs;
   }

   vk_outarray_append_typed(VkPipelineExecutableStatisticKHR, &out, stat) {
      WRITE_STR(stat->name, "SLM Size");
      WRITE_STR(stat->description,
                "Size of shader local (scratch) memory, in bytes");
      stat->format = VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR;
      stat->value.u64 = shader->info.slm_size;
   }

   return vk_outarray_status(&out);
}

static bool
write_ir_text(VkPipelineExecutableInternalRepresentationKHR* ir,
              const char *data)
{
   ir->isText = VK_TRUE;

   size_t data_len = strlen(data) + 1;

   if (ir->pData == NULL) {
      ir->dataSize = data_len;
      return true;
   }

   strncpy(ir->pData, data, ir->dataSize);
   if (ir->dataSize < data_len)
      return false;

   ir->dataSize = data_len;
   return true;
}

static VkResult
nvk_shader_get_executable_internal_representations(
   UNUSED struct vk_device *device,
   const struct vk_shader *vk_shader,
   uint32_t executable_index,
   uint32_t *internal_representation_count,
   VkPipelineExecutableInternalRepresentationKHR *internal_representations)
{
   struct nvk_shader *shader = container_of(vk_shader, struct nvk_shader, vk);
   VK_OUTARRAY_MAKE_TYPED(VkPipelineExecutableInternalRepresentationKHR, out,
                          internal_representations,
                          internal_representation_count);
   bool incomplete_text = false;

   assert(executable_index == 0);

   if (shader->nak != NULL && shader->nak->asm_str != NULL) {
      vk_outarray_append_typed(VkPipelineExecutableInternalRepresentationKHR, &out, ir) {
         WRITE_STR(ir->name, "NAK assembly");
         WRITE_STR(ir->description, "NAK assembly");
         if (!write_ir_text(ir, shader->nak->asm_str))
            incomplete_text = true;
      }
   }

   return incomplete_text ? VK_INCOMPLETE : vk_outarray_status(&out);
}

static const struct vk_shader_ops nvk_shader_ops = {
   .destroy = nvk_shader_destroy,
   .serialize = nvk_shader_serialize,
   .get_executable_properties = nvk_shader_get_executable_properties,
   .get_executable_statistics = nvk_shader_get_executable_statistics,
   .get_executable_internal_representations =
      nvk_shader_get_executable_internal_representations,
};

const struct vk_device_shader_ops nvk_device_shader_ops = {
   .get_nir_options = nvk_get_nir_options,
   .get_spirv_options = nvk_get_spirv_options,
   .preprocess_nir = nvk_preprocess_nir,
   .hash_graphics_state = nvk_hash_graphics_state,
   .compile = nvk_compile_shaders,
   .deserialize = nvk_deserialize_shader,
   .cmd_set_dynamic_graphics_state = vk_cmd_set_dynamic_graphics_state,
   .cmd_bind_shaders = nvk_cmd_bind_shaders,
};
