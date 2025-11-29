/*
 * Copyright © 2015 Intel Corporation
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

#include <assert.h>
#include <stdbool.h>

#include "anv_private.h"
#include "anv_measure.h"
#include "vk_render_pass.h"
#include "vk_util.h"

#include "common/intel_aux_map.h"
#include "genxml/gen_macros.h"
#include "genxml/genX_pack.h"
#include "genxml/genX_rt_pack.h"
#include "common/intel_genX_state_brw.h"

#include "ds/intel_tracepoints.h"

/* We reserve :
 *    - GPR 14 for secondary command buffer returns
 *    - GPR 15 for conditional rendering
 */
#define MI_BUILDER_NUM_ALLOC_GPRS 14
#define __gen_get_batch_dwords anv_batch_emit_dwords
#define __gen_address_offset anv_address_add
#define __gen_get_batch_address(b, a) anv_batch_address(b, a)
#include "common/mi_builder.h"

void
genX(cmd_buffer_ensure_cfe_state)(struct anv_cmd_buffer *cmd_buffer,
                                  uint32_t total_scratch)
{
#if GFX_VERx10 >= 125
   assert(cmd_buffer->state.current_pipeline == GPGPU);

   struct anv_cmd_compute_state *comp_state = &cmd_buffer->state.compute;

   if (total_scratch <= comp_state->scratch_size)
      return;

   const struct intel_device_info *devinfo = cmd_buffer->device->info;
   anv_batch_emit(&cmd_buffer->batch, GENX(CFE_STATE), cfe) {
      cfe.MaximumNumberofThreads =
         devinfo->max_cs_threads * devinfo->subslice_total;

      uint32_t scratch_surf = 0xffffffff;
      if (total_scratch > 0) {
         struct anv_scratch_pool *scratch_pool =
            (cmd_buffer->vk.pool->flags & VK_COMMAND_POOL_CREATE_PROTECTED_BIT) ?
            &cmd_buffer->device->protected_scratch_pool :
            &cmd_buffer->device->scratch_pool;
         struct anv_bo *scratch_bo =
               anv_scratch_pool_alloc(cmd_buffer->device, scratch_pool,
                                      MESA_SHADER_COMPUTE,
                                      total_scratch);
         anv_reloc_list_add_bo(cmd_buffer->batch.relocs,
                               scratch_bo);
         scratch_surf =
            anv_scratch_pool_get_surf(cmd_buffer->device, scratch_pool,
                                      total_scratch);
         cfe.ScratchSpaceBuffer = scratch_surf >> 4;
      }

      cfe.OverDispatchControl = 2; /* 50% overdispatch */
   }

   comp_state->scratch_size = total_scratch;
#else
   unreachable("Invalid call");
#endif
}

static void
genX(cmd_buffer_flush_compute_state)(struct anv_cmd_buffer *cmd_buffer)
{
   struct anv_cmd_compute_state *comp_state = &cmd_buffer->state.compute;
   struct anv_compute_pipeline *pipeline =
      anv_pipeline_to_compute(comp_state->base.pipeline);
   const UNUSED struct intel_device_info *devinfo = cmd_buffer->device->info;

   assert(pipeline->cs);

   genX(cmd_buffer_config_l3)(cmd_buffer, pipeline->base.l3_config);

   genX(flush_descriptor_buffers)(cmd_buffer, &comp_state->base);

   genX(flush_pipeline_select_gpgpu)(cmd_buffer);

   /* Apply any pending pipeline flushes we may have.  We want to apply them
    * now because, if any of those flushes are for things like push constants,
    * the GPU will read the state at weird times.
    */
   genX(cmd_buffer_apply_pipe_flushes)(cmd_buffer);

   if (cmd_buffer->state.compute.pipeline_dirty) {
#if GFX_VERx10 < 125
      /* From the Sky Lake PRM Vol 2a, MEDIA_VFE_STATE:
       *
       *    "A stalling PIPE_CONTROL is required before MEDIA_VFE_STATE unless
       *    the only bits that are changed are scoreboard related: Scoreboard
       *    Enable, Scoreboard Type, Scoreboard Mask, Scoreboard * Delta. For
       *    these scoreboard related states, a MEDIA_STATE_FLUSH is
       *    sufficient."
       */
      anv_add_pending_pipe_bits(cmd_buffer,
                              ANV_PIPE_CS_STALL_BIT,
                              "flush compute state");
      genX(cmd_buffer_apply_pipe_flushes)(cmd_buffer);
#endif

      anv_batch_emit_batch(&cmd_buffer->batch, &pipeline->base.batch);

#if GFX_VERx10 >= 125
      const struct brw_cs_prog_data *prog_data = get_cs_prog_data(pipeline);
      genX(cmd_buffer_ensure_cfe_state)(cmd_buffer, prog_data->base.total_scratch);
#endif

      /* The workgroup size of the pipeline affects our push constant layout
       * so flag push constants as dirty if we change the pipeline.
       */
      cmd_buffer->state.push_constants_dirty |= VK_SHADER_STAGE_COMPUTE_BIT;
      comp_state->base.push_constants_data_dirty = true;
   }

   cmd_buffer->state.descriptors_dirty |=
      genX(cmd_buffer_flush_push_descriptors)(cmd_buffer,
                                              &cmd_buffer->state.compute.base,
                                              &pipeline->base);

   if ((cmd_buffer->state.descriptors_dirty & VK_SHADER_STAGE_COMPUTE_BIT) ||
       cmd_buffer->state.compute.pipeline_dirty) {
      genX(cmd_buffer_flush_descriptor_sets)(cmd_buffer,
                                             &cmd_buffer->state.compute.base,
                                             VK_SHADER_STAGE_COMPUTE_BIT,
                                             &pipeline->cs, 1);
      cmd_buffer->state.descriptors_dirty &= ~VK_SHADER_STAGE_COMPUTE_BIT;

#if GFX_VERx10 < 125
      uint32_t iface_desc_data_dw[GENX(INTERFACE_DESCRIPTOR_DATA_length)];
      struct GENX(INTERFACE_DESCRIPTOR_DATA) desc = {
         .BindingTablePointer =
            cmd_buffer->state.binding_tables[MESA_SHADER_COMPUTE].offset,
         .SamplerStatePointer =
            cmd_buffer->state.samplers[MESA_SHADER_COMPUTE].offset,
      };
      GENX(INTERFACE_DESCRIPTOR_DATA_pack)(NULL, iface_desc_data_dw, &desc);

      struct anv_state state =
         anv_cmd_buffer_merge_dynamic(cmd_buffer, iface_desc_data_dw,
                                      pipeline->interface_descriptor_data,
                                      GENX(INTERFACE_DESCRIPTOR_DATA_length),
                                      64);

      uint32_t size = GENX(INTERFACE_DESCRIPTOR_DATA_length) * sizeof(uint32_t);
      anv_batch_emit(&cmd_buffer->batch,
                     GENX(MEDIA_INTERFACE_DESCRIPTOR_LOAD), mid) {
         mid.InterfaceDescriptorTotalLength        = size;
         mid.InterfaceDescriptorDataStartAddress   = state.offset;
      }
#endif
   }

   if (cmd_buffer->state.push_constants_dirty & VK_SHADER_STAGE_COMPUTE_BIT) {

      if (comp_state->push_data.alloc_size == 0 ||
          comp_state->base.push_constants_data_dirty) {
         comp_state->push_data =
            anv_cmd_buffer_cs_push_constants(cmd_buffer);
         comp_state->base.push_constants_data_dirty = false;
      }

#if GFX_VERx10 < 125
      if (comp_state->push_data.alloc_size) {
         anv_batch_emit(&cmd_buffer->batch, GENX(MEDIA_CURBE_LOAD), curbe) {
            curbe.CURBETotalDataLength    = comp_state->push_data.alloc_size;
            curbe.CURBEDataStartAddress   = comp_state->push_data.offset;
         }
      }
#endif

      cmd_buffer->state.push_constants_dirty &= ~VK_SHADER_STAGE_COMPUTE_BIT;
   }

   cmd_buffer->state.compute.pipeline_dirty = false;

   genX(cmd_buffer_apply_pipe_flushes)(cmd_buffer);
}

static void
anv_cmd_buffer_push_base_group_id(struct anv_cmd_buffer *cmd_buffer,
                                  uint32_t baseGroupX,
                                  uint32_t baseGroupY,
                                  uint32_t baseGroupZ)
{
   if (anv_batch_has_error(&cmd_buffer->batch))
      return;

   struct anv_push_constants *push =
      &cmd_buffer->state.compute.base.push_constants;
   if (push->cs.base_work_group_id[0] != baseGroupX ||
       push->cs.base_work_group_id[1] != baseGroupY ||
       push->cs.base_work_group_id[2] != baseGroupZ) {
      push->cs.base_work_group_id[0] = baseGroupX;
      push->cs.base_work_group_id[1] = baseGroupY;
      push->cs.base_work_group_id[2] = baseGroupZ;

      cmd_buffer->state.push_constants_dirty |= VK_SHADER_STAGE_COMPUTE_BIT;
      cmd_buffer->state.compute.base.push_constants_data_dirty = true;
   }
}

#define GPGPU_DISPATCHDIMX 0x2500
#define GPGPU_DISPATCHDIMY 0x2504
#define GPGPU_DISPATCHDIMZ 0x2508

static void
compute_load_indirect_params(struct anv_cmd_buffer *cmd_buffer,
                             const struct anv_address indirect_addr)
{
   struct mi_builder b;
   mi_builder_init(&b, cmd_buffer->device->info, &cmd_buffer->batch);

   struct mi_value size_x = mi_mem32(anv_address_add(indirect_addr, 0));
   struct mi_value size_y = mi_mem32(anv_address_add(indirect_addr, 4));
   struct mi_value size_z = mi_mem32(anv_address_add(indirect_addr, 8));

   mi_store(&b, mi_reg32(GPGPU_DISPATCHDIMX), size_x);
   mi_store(&b, mi_reg32(GPGPU_DISPATCHDIMY), size_y);
   mi_store(&b, mi_reg32(GPGPU_DISPATCHDIMZ), size_z);
}

static void
compute_store_indirect_params(struct anv_cmd_buffer *cmd_buffer,
                             const struct anv_address indirect_addr)
{
   struct mi_builder b;
   mi_builder_init(&b, cmd_buffer->device->info, &cmd_buffer->batch);

   struct mi_value size_x = mi_mem32(anv_address_add(indirect_addr, 0));
   struct mi_value size_y = mi_mem32(anv_address_add(indirect_addr, 4));
   struct mi_value size_z = mi_mem32(anv_address_add(indirect_addr, 8));

   mi_store(&b, size_x, mi_reg32(GPGPU_DISPATCHDIMX));
   mi_store(&b, size_y, mi_reg32(GPGPU_DISPATCHDIMY));
   mi_store(&b, size_z, mi_reg32(GPGPU_DISPATCHDIMZ));
}


#if GFX_VERx10 >= 125

static inline struct GENX(INTERFACE_DESCRIPTOR_DATA)
get_interface_descriptor_data(struct anv_cmd_buffer *cmd_buffer,
                              const struct anv_shader_bin *shader,
                              const struct brw_cs_prog_data *prog_data,
                              const struct intel_cs_dispatch_info *dispatch)
{
   const struct intel_device_info *devinfo = cmd_buffer->device->info;

   return (struct GENX(INTERFACE_DESCRIPTOR_DATA)) {
      .KernelStartPointer = shader->kernel.offset,
      .SamplerStatePointer = cmd_buffer->state.samplers[MESA_SHADER_COMPUTE].offset,
      .BindingTablePointer = cmd_buffer->state.binding_tables[MESA_SHADER_COMPUTE].offset,
      /* Typically set to 0 to avoid prefetching on every thread dispatch. */
      .BindingTableEntryCount = devinfo->verx10 == 125 ?
         0 : 1 + MIN2(shader->bind_map.surface_count, 30),
      .NumberofThreadsinGPGPUThreadGroup = dispatch->threads,
      .SharedLocalMemorySize = encode_slm_size(GFX_VER, prog_data->base.total_shared),
      .PreferredSLMAllocationSize = preferred_slm_allocation_size(devinfo),
      .NumberOfBarriers = prog_data->uses_barrier,
   };
}

static inline void
emit_indirect_compute_walker(struct anv_cmd_buffer *cmd_buffer,
                             const struct anv_shader_bin *shader,
                             const struct brw_cs_prog_data *prog_data,
                             struct anv_address indirect_addr)
{
   const struct intel_device_info *devinfo = cmd_buffer->device->info;
   assert(devinfo->has_indirect_unroll);

   struct anv_cmd_compute_state *comp_state = &cmd_buffer->state.compute;
   bool predicate = cmd_buffer->state.conditional_render_enabled;

   const struct intel_cs_dispatch_info dispatch =
      brw_cs_get_dispatch_info(devinfo, prog_data, NULL);
   const int dispatch_size = dispatch.simd_size / 16;

   struct GENX(COMPUTE_WALKER_BODY) body =  {
      .SIMDSize                 = dispatch_size,
      .MessageSIMD              = dispatch_size,
      .IndirectDataStartAddress = comp_state->push_data.offset,
      .IndirectDataLength       = comp_state->push_data.alloc_size,
      .GenerateLocalID          = prog_data->generate_local_id != 0,
      .EmitLocal                = prog_data->generate_local_id,
      .WalkOrder                = prog_data->walk_order,
      .TileLayout               = prog_data->walk_order == INTEL_WALK_ORDER_YXZ ?
                                  TileY32bpe : Linear,
      .LocalXMaximum            = prog_data->local_size[0] - 1,
      .LocalYMaximum            = prog_data->local_size[1] - 1,
      .LocalZMaximum            = prog_data->local_size[2] - 1,
      .ExecutionMask            = dispatch.right_mask,
      .PostSync.MOCS            = anv_mocs(cmd_buffer->device, NULL, 0),
      .InterfaceDescriptor =
         get_interface_descriptor_data(cmd_buffer, shader, prog_data,
                                       &dispatch),
   };

   cmd_buffer->last_indirect_dispatch =
      anv_batch_emitn(
         &cmd_buffer->batch,
         GENX(EXECUTE_INDIRECT_DISPATCH_length),
         GENX(EXECUTE_INDIRECT_DISPATCH),
         .PredicateEnable            = predicate,
         .MaxCount                   = 1,
         .COMPUTE_WALKER_BODY        = body,
         .ArgumentBufferStartAddress = indirect_addr,
         .MOCS                       = anv_mocs(cmd_buffer->device,
                                                indirect_addr.bo, 0),
      );
}

static inline void
emit_compute_walker(struct anv_cmd_buffer *cmd_buffer,
                    const struct anv_compute_pipeline *pipeline, bool indirect,
                    const struct brw_cs_prog_data *prog_data,
                    uint32_t groupCountX, uint32_t groupCountY,
                    uint32_t groupCountZ)
{
   const struct anv_cmd_compute_state *comp_state = &cmd_buffer->state.compute;
   const bool predicate = cmd_buffer->state.conditional_render_enabled;

   const struct intel_device_info *devinfo = pipeline->base.device->info;
   const struct intel_cs_dispatch_info dispatch =
      brw_cs_get_dispatch_info(devinfo, prog_data, NULL);

   cmd_buffer->last_compute_walker =
      anv_batch_emitn(
         &cmd_buffer->batch,
         GENX(COMPUTE_WALKER_length),
         GENX(COMPUTE_WALKER),
         .IndirectParameterEnable        = indirect,
         .PredicateEnable                = predicate,
         .SIMDSize                       = dispatch.simd_size / 16,
         .MessageSIMD                    = dispatch.simd_size / 16,
         .IndirectDataStartAddress       = comp_state->push_data.offset,
         .IndirectDataLength             = comp_state->push_data.alloc_size,
#if GFX_VERx10 == 125
         .SystolicModeEnable             = prog_data->uses_systolic,
#endif
         .GenerateLocalID                = prog_data->generate_local_id != 0,
         .EmitLocal                      = prog_data->generate_local_id,
         .WalkOrder                      = prog_data->walk_order,
         .TileLayout = prog_data->walk_order == INTEL_WALK_ORDER_YXZ ?
                       TileY32bpe : Linear,
         .LocalXMaximum                  = prog_data->local_size[0] - 1,
         .LocalYMaximum                  = prog_data->local_size[1] - 1,
         .LocalZMaximum                  = prog_data->local_size[2] - 1,
         .ThreadGroupIDXDimension        = groupCountX,
         .ThreadGroupIDYDimension        = groupCountY,
         .ThreadGroupIDZDimension        = groupCountZ,
         .ExecutionMask                  = dispatch.right_mask,
         .PostSync                       = {
            .MOCS                        = anv_mocs(pipeline->base.device, NULL, 0),
         },
         .InterfaceDescriptor =
            get_interface_descriptor_data(cmd_buffer, pipeline->cs,
                                          prog_data, &dispatch),
      );
}

#else /* #if GFX_VERx10 >= 125 */

static inline void
emit_gpgpu_walker(struct anv_cmd_buffer *cmd_buffer,
                  const struct anv_compute_pipeline *pipeline, bool indirect,
                  const struct brw_cs_prog_data *prog_data,
                  uint32_t groupCountX, uint32_t groupCountY,
                  uint32_t groupCountZ)
{
   const bool predicate = cmd_buffer->state.conditional_render_enabled;

   const struct intel_device_info *devinfo = pipeline->base.device->info;
   const struct intel_cs_dispatch_info dispatch =
      brw_cs_get_dispatch_info(devinfo, prog_data, NULL);

   anv_batch_emit(&cmd_buffer->batch, GENX(GPGPU_WALKER), ggw) {
      ggw.IndirectParameterEnable      = indirect;
      ggw.PredicateEnable              = predicate;
      ggw.SIMDSize                     = dispatch.simd_size / 16;
      ggw.ThreadDepthCounterMaximum    = 0;
      ggw.ThreadHeightCounterMaximum   = 0;
      ggw.ThreadWidthCounterMaximum    = dispatch.threads - 1;
      ggw.ThreadGroupIDXDimension      = groupCountX;
      ggw.ThreadGroupIDYDimension      = groupCountY;
      ggw.ThreadGroupIDZDimension      = groupCountZ;
      ggw.RightExecutionMask           = dispatch.right_mask;
      ggw.BottomExecutionMask          = 0xffffffff;
   }

   anv_batch_emit(&cmd_buffer->batch, GENX(MEDIA_STATE_FLUSH), msf);
}

#endif /* #if GFX_VERx10 >= 125 */

static inline void
emit_cs_walker(struct anv_cmd_buffer *cmd_buffer,
               const struct anv_compute_pipeline *pipeline,
               const struct brw_cs_prog_data *prog_data,
               struct anv_address indirect_addr,
               uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
   bool is_indirect = !anv_address_is_null(indirect_addr);

#if GFX_VERx10 >= 125
   if (is_indirect && cmd_buffer->device->info->has_indirect_unroll) {
      emit_indirect_compute_walker(cmd_buffer, pipeline->cs, prog_data,
                                   indirect_addr);
      return;
   }
#endif

   if (is_indirect)
      compute_load_indirect_params(cmd_buffer, indirect_addr);

#if GFX_VERx10 >= 125
   emit_compute_walker(cmd_buffer, pipeline, is_indirect, prog_data,
                       groupCountX, groupCountY, groupCountZ);
#else
   emit_gpgpu_walker(cmd_buffer, pipeline, is_indirect, prog_data,
                     groupCountX, groupCountY, groupCountZ);
#endif
}

void genX(CmdDispatchBase)(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    baseGroupX,
    uint32_t                                    baseGroupY,
    uint32_t                                    baseGroupZ,
    uint32_t                                    groupCountX,
    uint32_t                                    groupCountY,
    uint32_t                                    groupCountZ)
{
   ANV_FROM_HANDLE(anv_cmd_buffer, cmd_buffer, commandBuffer);
   struct anv_compute_pipeline *pipeline =
      anv_pipeline_to_compute(cmd_buffer->state.compute.base.pipeline);
   const struct brw_cs_prog_data *prog_data = get_cs_prog_data(pipeline);

   anv_cmd_buffer_push_base_group_id(cmd_buffer, baseGroupX,
                                     baseGroupY, baseGroupZ);

   if (anv_batch_has_error(&cmd_buffer->batch))
      return;

   anv_measure_snapshot(cmd_buffer,
                        INTEL_SNAPSHOT_COMPUTE,
                        "compute",
                        groupCountX * groupCountY * groupCountZ *
                        prog_data->local_size[0] * prog_data->local_size[1] *
                        prog_data->local_size[2]);

   trace_intel_begin_compute(&cmd_buffer->trace);

   if (prog_data->uses_num_work_groups) {
      struct anv_state state =
         anv_cmd_buffer_alloc_temporary_state(cmd_buffer, 12, 4);
      uint32_t *sizes = state.map;
      sizes[0] = groupCountX;
      sizes[1] = groupCountY;
      sizes[2] = groupCountZ;
      cmd_buffer->state.compute.num_workgroups =
         anv_cmd_buffer_temporary_state_address(cmd_buffer, state);

      /* The num_workgroups buffer goes in the binding table */
      cmd_buffer->state.descriptors_dirty |= VK_SHADER_STAGE_COMPUTE_BIT;
   }

   genX(cmd_buffer_flush_compute_state)(cmd_buffer);

   if (cmd_buffer->state.conditional_render_enabled)
      genX(cmd_emit_conditional_render_predicate)(cmd_buffer);

   emit_cs_walker(cmd_buffer, pipeline, prog_data,
                  ANV_NULL_ADDRESS /* no indirect data */,
                  groupCountX, groupCountY, groupCountZ);

   trace_intel_end_compute(&cmd_buffer->trace,
                           groupCountX, groupCountY, groupCountZ);
}

void genX(CmdDispatchIndirect)(
    VkCommandBuffer                             commandBuffer,
    VkBuffer                                    _buffer,
    VkDeviceSize                                offset)
{
   ANV_FROM_HANDLE(anv_cmd_buffer, cmd_buffer, commandBuffer);
   ANV_FROM_HANDLE(anv_buffer, buffer, _buffer);
   struct anv_compute_pipeline *pipeline =
      anv_pipeline_to_compute(cmd_buffer->state.compute.base.pipeline);
   const struct brw_cs_prog_data *prog_data = get_cs_prog_data(pipeline);
   struct anv_address addr = anv_address_add(buffer->address, offset);
   UNUSED struct anv_batch *batch = &cmd_buffer->batch;

   anv_cmd_buffer_push_base_group_id(cmd_buffer, 0, 0, 0);

   anv_measure_snapshot(cmd_buffer,
                        INTEL_SNAPSHOT_COMPUTE,
                        "compute indirect",
                        0);
   trace_intel_begin_compute(&cmd_buffer->trace);

   if (prog_data->uses_num_work_groups) {
      cmd_buffer->state.compute.num_workgroups = addr;

      /* The num_workgroups buffer goes in the binding table */
      cmd_buffer->state.descriptors_dirty |= VK_SHADER_STAGE_COMPUTE_BIT;
   }

   genX(cmd_buffer_flush_compute_state)(cmd_buffer);

   if (cmd_buffer->state.conditional_render_enabled)
      genX(cmd_emit_conditional_render_predicate)(cmd_buffer);

   emit_cs_walker(cmd_buffer, pipeline, prog_data, addr, 0, 0, 0);

   trace_intel_end_compute(&cmd_buffer->trace, 0, 0, 0);
}

struct anv_address
genX(cmd_buffer_ray_query_globals)(struct anv_cmd_buffer *cmd_buffer)
{
#if GFX_VERx10 >= 125
   struct anv_device *device = cmd_buffer->device;

   struct anv_state state =
      anv_cmd_buffer_alloc_temporary_state(cmd_buffer,
                                           BRW_RT_DISPATCH_GLOBALS_SIZE, 64);
   struct brw_rt_scratch_layout layout;
   uint32_t stack_ids_per_dss = 2048; /* TODO: can we use a lower value in
                                       * some cases?
                                       */
   brw_rt_compute_scratch_layout(&layout, device->info,
                                 stack_ids_per_dss, 1 << 10);

   const struct GENX(RT_DISPATCH_GLOBALS) rtdg = {
      .MemBaseAddress = (struct anv_address) {
         /* The ray query HW computes offsets from the top of the buffer, so
          * let the address at the end of the buffer.
          */
         .bo = device->ray_query_bo,
         .offset = device->ray_query_bo->size
      },
      .AsyncRTStackSize = layout.ray_stack_stride / 64,
      .NumDSSRTStacks = layout.stack_ids_per_dss,
      .MaxBVHLevels = BRW_RT_MAX_BVH_LEVELS,
      .Flags = RT_DEPTH_TEST_LESS_EQUAL,
      .ResumeShaderTable = (struct anv_address) {
         .bo = cmd_buffer->state.ray_query_shadow_bo,
      },
   };
   GENX(RT_DISPATCH_GLOBALS_pack)(NULL, state.map, &rtdg);

   return anv_cmd_buffer_temporary_state_address(cmd_buffer, state);
#else
   unreachable("Not supported");
#endif
}

#if GFX_VERx10 >= 125
void
genX(cmd_buffer_dispatch_kernel)(struct anv_cmd_buffer *cmd_buffer,
                                 struct anv_kernel *kernel,
                                 const uint32_t *global_size,
                                 uint32_t arg_count,
                                 const struct anv_kernel_arg *args)
{
   const struct intel_device_info *devinfo = cmd_buffer->device->info;
   const struct brw_cs_prog_data *cs_prog_data =
      brw_cs_prog_data_const(kernel->bin->prog_data);

   genX(cmd_buffer_config_l3)(cmd_buffer, kernel->l3_config);

   genX(flush_pipeline_select_gpgpu)(cmd_buffer);

   /* Apply any pending pipeline flushes we may have.  We want to apply them
    * now because, if any of those flushes are for things like push constants,
    * the GPU will read the state at weird times.
    */
   genX(cmd_buffer_apply_pipe_flushes)(cmd_buffer);

   uint32_t indirect_data_size = sizeof(struct brw_kernel_sysvals);
   indirect_data_size += kernel->bin->bind_map.kernel_args_size;
   indirect_data_size = ALIGN(indirect_data_size, 64);
   struct anv_state indirect_data =
      anv_cmd_buffer_alloc_general_state(cmd_buffer,
                                         indirect_data_size, 64);
   memset(indirect_data.map, 0, indirect_data.alloc_size);

   struct brw_kernel_sysvals sysvals = {};
   if (global_size != NULL) {
      for (unsigned i = 0; i < 3; i++)
         sysvals.num_work_groups[i] = global_size[i];
      memcpy(indirect_data.map, &sysvals, sizeof(sysvals));
   } else {
      struct anv_address sysvals_addr = {
         .bo = NULL, /* General state buffer is always 0. */
         .offset = indirect_data.offset,
      };

      compute_store_indirect_params(cmd_buffer, sysvals_addr);
   }

   void *args_map = indirect_data.map + sizeof(sysvals);
   for (unsigned i = 0; i < kernel->bin->bind_map.kernel_arg_count; i++) {
      struct brw_kernel_arg_desc *arg_desc =
         &kernel->bin->bind_map.kernel_args[i];
      assert(i < arg_count);
      const struct anv_kernel_arg *arg = &args[i];
      if (arg->is_ptr) {
         memcpy(args_map + arg_desc->offset, arg->ptr, arg_desc->size);
      } else {
         assert(arg_desc->size <= sizeof(arg->u64));
         memcpy(args_map + arg_desc->offset, &arg->u64, arg_desc->size);
      }
   }

   struct intel_cs_dispatch_info dispatch =
      brw_cs_get_dispatch_info(devinfo, cs_prog_data, NULL);

   anv_batch_emit(&cmd_buffer->batch, GENX(COMPUTE_WALKER), cw) {
      cw.PredicateEnable                = false;
      cw.SIMDSize                       = dispatch.simd_size / 16;
      cw.MessageSIMD                    = dispatch.simd_size / 16;
      cw.IndirectDataStartAddress       = indirect_data.offset;
      cw.IndirectDataLength             = indirect_data.alloc_size;
      cw.LocalXMaximum                  = cs_prog_data->local_size[0] - 1;
      cw.LocalYMaximum                  = cs_prog_data->local_size[1] - 1;
      cw.LocalZMaximum                  = cs_prog_data->local_size[2] - 1;
      cw.ExecutionMask                  = dispatch.right_mask;
      cw.PostSync.MOCS                  = cmd_buffer->device->isl_dev.mocs.internal;

      if (global_size != NULL) {
         cw.ThreadGroupIDXDimension     = global_size[0];
         cw.ThreadGroupIDYDimension     = global_size[1];
         cw.ThreadGroupIDZDimension     = global_size[2];
      } else {
         cw.IndirectParameterEnable     = true;
      }

      cw.InterfaceDescriptor =
         get_interface_descriptor_data(cmd_buffer,
                                       kernel->bin,
                                       cs_prog_data,
                                       &dispatch);
   }

   /* We just blew away the compute pipeline state */
   cmd_buffer->state.compute.pipeline_dirty = true;
}

static void
calc_local_trace_size(uint8_t local_shift[3], const uint32_t global[3])
{
   unsigned total_shift = 0;
   memset(local_shift, 0, 3);

   bool progress;
   do {
      progress = false;
      for (unsigned i = 0; i < 3; i++) {
         assert(global[i] > 0);
         if ((1 << local_shift[i]) < global[i]) {
            progress = true;
            local_shift[i]++;
            total_shift++;
         }

         if (total_shift == 3)
            return;
      }
   } while(progress);

   /* Assign whatever's left to x */
   local_shift[0] += 3 - total_shift;
}

static struct GENX(RT_SHADER_TABLE)
vk_sdar_to_shader_table(const VkStridedDeviceAddressRegionKHR *region)
{
   return (struct GENX(RT_SHADER_TABLE)) {
      .BaseAddress = anv_address_from_u64(region->deviceAddress),
      .Stride = region->stride,
   };
}

struct trace_params {
   /* If is_sbt_indirect, use indirect_sbts_addr to build RT_DISPATCH_GLOBALS
    * with mi_builder.
    */
   bool is_sbt_indirect;
   const VkStridedDeviceAddressRegionKHR *raygen_sbt;
   const VkStridedDeviceAddressRegionKHR *miss_sbt;
   const VkStridedDeviceAddressRegionKHR *hit_sbt;
   const VkStridedDeviceAddressRegionKHR *callable_sbt;

   /* A pointer to a VkTraceRaysIndirectCommand2KHR structure */
   uint64_t indirect_sbts_addr;

   /* If is_indirect, use launch_size_addr to program the dispatch size. */
   bool is_launch_size_indirect;
   uint32_t launch_size[3];

   /* A pointer a uint32_t[3] */
   uint64_t launch_size_addr;
};

static struct anv_state
cmd_buffer_emit_rt_dispatch_globals(struct anv_cmd_buffer *cmd_buffer,
                                    struct trace_params *params)
{
   assert(!params->is_sbt_indirect);
   assert(params->miss_sbt != NULL);
   assert(params->hit_sbt != NULL);
   assert(params->callable_sbt != NULL);

   struct anv_cmd_ray_tracing_state *rt = &cmd_buffer->state.rt;

   struct anv_state rtdg_state =
      anv_cmd_buffer_alloc_temporary_state(cmd_buffer,
                                           BRW_RT_PUSH_CONST_OFFSET +
                                           sizeof(struct anv_push_constants),
                                           64);

   struct GENX(RT_DISPATCH_GLOBALS) rtdg = {
      .MemBaseAddress     = (struct anv_address) {
         .bo = rt->scratch.bo,
         .offset = rt->scratch.layout.ray_stack_start,
      },
      .CallStackHandler   = anv_shader_bin_get_bsr(
         cmd_buffer->device->rt_trivial_return, 0),
      .AsyncRTStackSize   = rt->scratch.layout.ray_stack_stride / 64,
      .NumDSSRTStacks     = rt->scratch.layout.stack_ids_per_dss,
      .MaxBVHLevels       = BRW_RT_MAX_BVH_LEVELS,
      .Flags              = RT_DEPTH_TEST_LESS_EQUAL,
      .HitGroupTable      = vk_sdar_to_shader_table(params->hit_sbt),
      .MissGroupTable     = vk_sdar_to_shader_table(params->miss_sbt),
      .SWStackSize        = rt->scratch.layout.sw_stack_size / 64,
      .LaunchWidth        = params->launch_size[0],
      .LaunchHeight       = params->launch_size[1],
      .LaunchDepth        = params->launch_size[2],
      .CallableGroupTable = vk_sdar_to_shader_table(params->callable_sbt),
   };
   GENX(RT_DISPATCH_GLOBALS_pack)(NULL, rtdg_state.map, &rtdg);

   return rtdg_state;
}

static struct mi_value
mi_build_sbt_entry(struct mi_builder *b,
                   uint64_t addr_field_addr,
                   uint64_t stride_field_addr)
{
   return mi_ior(b,
                 mi_iand(b, mi_mem64(anv_address_from_u64(addr_field_addr)),
                            mi_imm(BITFIELD64_BIT(49) - 1)),
                 mi_ishl_imm(b, mi_mem32(anv_address_from_u64(stride_field_addr)),
                                48));
}

static struct anv_state
cmd_buffer_emit_rt_dispatch_globals_indirect(struct anv_cmd_buffer *cmd_buffer,
                                             struct trace_params *params)
{
   struct anv_cmd_ray_tracing_state *rt = &cmd_buffer->state.rt;

   struct anv_state rtdg_state =
      anv_cmd_buffer_alloc_temporary_state(cmd_buffer,
                                           BRW_RT_PUSH_CONST_OFFSET +
                                           sizeof(struct anv_push_constants),
                                           64);

   struct GENX(RT_DISPATCH_GLOBALS) rtdg = {
      .MemBaseAddress     = (struct anv_address) {
         .bo = rt->scratch.bo,
         .offset = rt->scratch.layout.ray_stack_start,
      },
      .CallStackHandler   = anv_shader_bin_get_bsr(
         cmd_buffer->device->rt_trivial_return, 0),
      .AsyncRTStackSize   = rt->scratch.layout.ray_stack_stride / 64,
      .NumDSSRTStacks     = rt->scratch.layout.stack_ids_per_dss,
      .MaxBVHLevels       = BRW_RT_MAX_BVH_LEVELS,
      .Flags              = RT_DEPTH_TEST_LESS_EQUAL,
      .SWStackSize        = rt->scratch.layout.sw_stack_size / 64,
   };
   GENX(RT_DISPATCH_GLOBALS_pack)(NULL, rtdg_state.map, &rtdg);

   struct anv_address rtdg_addr =
      anv_cmd_buffer_temporary_state_address(cmd_buffer, rtdg_state);

   struct mi_builder b;
   mi_builder_init(&b, cmd_buffer->device->info, &cmd_buffer->batch);
   const uint32_t mocs = anv_mocs_for_address(cmd_buffer->device, &rtdg_addr);
   mi_builder_set_mocs(&b, mocs);

   /* Fill the MissGroupTable, HitGroupTable & CallableGroupTable fields of
    * RT_DISPATCH_GLOBALS using the mi_builder.
    */
   mi_store(&b,
            mi_mem64(
               anv_address_add(
                  rtdg_addr,
                  GENX(RT_DISPATCH_GLOBALS_MissGroupTable_start) / 8)),
            mi_build_sbt_entry(&b,
                               params->indirect_sbts_addr +
                               offsetof(VkTraceRaysIndirectCommand2KHR,
                                        missShaderBindingTableAddress),
                               params->indirect_sbts_addr +
                               offsetof(VkTraceRaysIndirectCommand2KHR,
                                        missShaderBindingTableStride)));
   mi_store(&b,
            mi_mem64(
               anv_address_add(
                  rtdg_addr,
                  GENX(RT_DISPATCH_GLOBALS_HitGroupTable_start) / 8)),
            mi_build_sbt_entry(&b,
                               params->indirect_sbts_addr +
                               offsetof(VkTraceRaysIndirectCommand2KHR,
                                        hitShaderBindingTableAddress),
                               params->indirect_sbts_addr +
                               offsetof(VkTraceRaysIndirectCommand2KHR,
                                        hitShaderBindingTableStride)));
   mi_store(&b,
            mi_mem64(
               anv_address_add(
                  rtdg_addr,
                  GENX(RT_DISPATCH_GLOBALS_CallableGroupTable_start) / 8)),
            mi_build_sbt_entry(&b,
                               params->indirect_sbts_addr +
                               offsetof(VkTraceRaysIndirectCommand2KHR,
                                        callableShaderBindingTableAddress),
                               params->indirect_sbts_addr +
                               offsetof(VkTraceRaysIndirectCommand2KHR,
                                        callableShaderBindingTableStride)));

   return rtdg_state;
}

static void
cmd_buffer_trace_rays(struct anv_cmd_buffer *cmd_buffer,
                      struct trace_params *params)
{
   struct anv_device *device = cmd_buffer->device;
   struct anv_cmd_ray_tracing_state *rt = &cmd_buffer->state.rt;
   struct anv_ray_tracing_pipeline *pipeline =
      anv_pipeline_to_ray_tracing(rt->base.pipeline);

   if (anv_batch_has_error(&cmd_buffer->batch))
      return;

   /* If we have a known degenerate launch size, just bail */
   if (!params->is_launch_size_indirect &&
       (params->launch_size[0] == 0 ||
        params->launch_size[1] == 0 ||
        params->launch_size[2] == 0))
      return;

   trace_intel_begin_rays(&cmd_buffer->trace);

   genX(cmd_buffer_config_l3)(cmd_buffer, pipeline->base.l3_config);

   genX(flush_descriptor_buffers)(cmd_buffer, &rt->base);

   genX(flush_pipeline_select_gpgpu)(cmd_buffer);

   cmd_buffer->state.rt.pipeline_dirty = false;

   genX(cmd_buffer_apply_pipe_flushes)(cmd_buffer);

   genX(cmd_buffer_flush_push_descriptors)(cmd_buffer,
                                           &cmd_buffer->state.rt.base,
                                           &pipeline->base);

   /* Add these to the reloc list as they're internal buffers that don't
    * actually have relocs to pick them up manually.
    *
    * TODO(RT): This is a bit of a hack
    */
   anv_reloc_list_add_bo(cmd_buffer->batch.relocs,
                         rt->scratch.bo);
   anv_reloc_list_add_bo(cmd_buffer->batch.relocs,
                         cmd_buffer->device->btd_fifo_bo);

   /* Allocate and set up our RT_DISPATCH_GLOBALS */
   struct anv_state rtdg_state =
      params->is_sbt_indirect ?
      cmd_buffer_emit_rt_dispatch_globals_indirect(cmd_buffer, params) :
      cmd_buffer_emit_rt_dispatch_globals(cmd_buffer, params);

   assert(rtdg_state.alloc_size >= (BRW_RT_PUSH_CONST_OFFSET +
                                    sizeof(struct anv_push_constants)));
   assert(GENX(RT_DISPATCH_GLOBALS_length) * 4 <= BRW_RT_PUSH_CONST_OFFSET);
   /* Push constants go after the RT_DISPATCH_GLOBALS */
   memcpy(rtdg_state.map + BRW_RT_PUSH_CONST_OFFSET,
          &cmd_buffer->state.rt.base.push_constants,
          sizeof(struct anv_push_constants));

   struct anv_address rtdg_addr =
      anv_cmd_buffer_temporary_state_address(cmd_buffer, rtdg_state);

   uint8_t local_size_log2[3];
   uint32_t global_size[3] = {};
   if (params->is_launch_size_indirect) {
      /* Pick a local size that's probably ok.  We assume most TraceRays calls
       * will use a two-dimensional dispatch size.  Worst case, our initial
       * dispatch will be a little slower than it has to be.
       */
      local_size_log2[0] = 2;
      local_size_log2[1] = 1;
      local_size_log2[2] = 0;

      struct mi_builder b;
      mi_builder_init(&b, cmd_buffer->device->info, &cmd_buffer->batch);
      const uint32_t mocs = anv_mocs_for_address(cmd_buffer->device, &rtdg_addr);
      mi_builder_set_mocs(&b, mocs);

      struct mi_value launch_size[3] = {
         mi_mem32(anv_address_from_u64(params->launch_size_addr + 0)),
         mi_mem32(anv_address_from_u64(params->launch_size_addr + 4)),
         mi_mem32(anv_address_from_u64(params->launch_size_addr + 8)),
      };

      /* Store the original launch size into RT_DISPATCH_GLOBALS */
      mi_store(&b, mi_mem32(anv_address_add(rtdg_addr,
                                            GENX(RT_DISPATCH_GLOBALS_LaunchWidth_start) / 8)),
               mi_value_ref(&b, launch_size[0]));
      mi_store(&b, mi_mem32(anv_address_add(rtdg_addr,
                                            GENX(RT_DISPATCH_GLOBALS_LaunchHeight_start) / 8)),
               mi_value_ref(&b, launch_size[1]));
      mi_store(&b, mi_mem32(anv_address_add(rtdg_addr,
                                            GENX(RT_DISPATCH_GLOBALS_LaunchDepth_start) / 8)),
               mi_value_ref(&b, launch_size[2]));

      /* Compute the global dispatch size */
      for (unsigned i = 0; i < 3; i++) {
         if (local_size_log2[i] == 0)
            continue;

         /* global_size = DIV_ROUND_UP(launch_size, local_size)
          *
          * Fortunately for us MI_ALU math is 64-bit and , mi_ushr32_imm
          * has the semantics of shifting the enture 64-bit value and taking
          * the bottom 32 so we don't have to worry about roll-over.
          */
         uint32_t local_size = 1 << local_size_log2[i];
         launch_size[i] = mi_iadd(&b, launch_size[i],
                                      mi_imm(local_size - 1));
         launch_size[i] = mi_ushr32_imm(&b, launch_size[i],
                                            local_size_log2[i]);
      }

      mi_store(&b, mi_reg32(GPGPU_DISPATCHDIMX), launch_size[0]);
      mi_store(&b, mi_reg32(GPGPU_DISPATCHDIMY), launch_size[1]);
      mi_store(&b, mi_reg32(GPGPU_DISPATCHDIMZ), launch_size[2]);

   } else {
      calc_local_trace_size(local_size_log2, params->launch_size);

      for (unsigned i = 0; i < 3; i++) {
         /* We have to be a bit careful here because DIV_ROUND_UP adds to the
          * numerator value may overflow.  Cast to uint64_t to avoid this.
          */
         uint32_t local_size = 1 << local_size_log2[i];
         global_size[i] = DIV_ROUND_UP((uint64_t)params->launch_size[i], local_size);
      }
   }

#if GFX_VERx10 == 125
   /* Wa_14014427904 - We need additional invalidate/flush when
    * emitting NP state commands with ATS-M in compute mode.
    */
   if (intel_device_info_is_atsm(device->info) &&
      cmd_buffer->queue_family->engine_class == INTEL_ENGINE_CLASS_COMPUTE) {
      genx_batch_emit_pipe_control(&cmd_buffer->batch,
                                   cmd_buffer->device->info,
                                   cmd_buffer->state.current_pipeline,
                                   ANV_PIPE_CS_STALL_BIT |
                                   ANV_PIPE_STATE_CACHE_INVALIDATE_BIT |
                                   ANV_PIPE_CONSTANT_CACHE_INVALIDATE_BIT |
                                   ANV_PIPE_UNTYPED_DATAPORT_CACHE_FLUSH_BIT |
                                   ANV_PIPE_TEXTURE_CACHE_INVALIDATE_BIT |
                                   ANV_PIPE_INSTRUCTION_CACHE_INVALIDATE_BIT |
                                   ANV_PIPE_HDC_PIPELINE_FLUSH_BIT);
   }
#endif

   anv_batch_emit(&cmd_buffer->batch, GENX(3DSTATE_BTD), btd) {
      /* TODO: This is the timeout after which the bucketed thread dispatcher
       *       will kick off a wave of threads. We go with the lowest value
       *       for now. It could be tweaked on a per application basis
       *       (drirc).
       */
      btd.DispatchTimeoutCounter = _64clocks;
      /* BSpec 43851: "This field must be programmed to 6h i.e. memory backed
       *               buffer must be 128KB."
       */
      btd.PerDSSMemoryBackedBufferSize = 6;
      btd.MemoryBackedBufferBasePointer = (struct anv_address) { .bo = device->btd_fifo_bo };
      if (pipeline->base.scratch_size > 0) {
         struct anv_bo *scratch_bo =
            anv_scratch_pool_alloc(device,
                                   &device->scratch_pool,
                                   MESA_SHADER_COMPUTE,
                                   pipeline->base.scratch_size);
         anv_reloc_list_add_bo(cmd_buffer->batch.relocs,
                               scratch_bo);
         uint32_t scratch_surf =
            anv_scratch_pool_get_surf(cmd_buffer->device,
                                      &device->scratch_pool,
                                      pipeline->base.scratch_size);
         btd.ScratchSpaceBuffer = scratch_surf >> 4;
      }
   }

   genX(cmd_buffer_ensure_cfe_state)(cmd_buffer, pipeline->base.scratch_size);

   const struct brw_cs_prog_data *cs_prog_data =
      brw_cs_prog_data_const(device->rt_trampoline->prog_data);
   struct intel_cs_dispatch_info dispatch =
      brw_cs_get_dispatch_info(device->info, cs_prog_data, NULL);

   anv_batch_emit(&cmd_buffer->batch, GENX(COMPUTE_WALKER), cw) {
      cw.IndirectParameterEnable        = params->is_launch_size_indirect;
      cw.PredicateEnable                = cmd_buffer->state.conditional_render_enabled;
      cw.SIMDSize                       = dispatch.simd_size / 16;
      cw.MessageSIMD                    = dispatch.simd_size / 16;
      cw.LocalXMaximum                  = (1 << local_size_log2[0]) - 1;
      cw.LocalYMaximum                  = (1 << local_size_log2[1]) - 1;
      cw.LocalZMaximum                  = (1 << local_size_log2[2]) - 1;
      cw.ThreadGroupIDXDimension        = global_size[0];
      cw.ThreadGroupIDYDimension        = global_size[1];
      cw.ThreadGroupIDZDimension        = global_size[2];
      cw.ExecutionMask                  = 0xff;
      cw.EmitInlineParameter            = true;
      cw.PostSync.MOCS                  = anv_mocs(pipeline->base.device, NULL, 0);

      const gl_shader_stage s = MESA_SHADER_RAYGEN;
      struct anv_device *device = cmd_buffer->device;
      struct anv_state *surfaces = &cmd_buffer->state.binding_tables[s];
      struct anv_state *samplers = &cmd_buffer->state.samplers[s];
      cw.InterfaceDescriptor = (struct GENX(INTERFACE_DESCRIPTOR_DATA)) {
         .KernelStartPointer = device->rt_trampoline->kernel.offset,
         .SamplerStatePointer = samplers->offset,
         /* i965: DIV_ROUND_UP(CLAMP(stage_state->sampler_count, 0, 16), 4), */
         .SamplerCount = 0,
         .BindingTablePointer = surfaces->offset,
         .NumberofThreadsinGPGPUThreadGroup = 1,
         .BTDMode = true,
      };

      struct brw_rt_raygen_trampoline_params trampoline_params = {
         .rt_disp_globals_addr = anv_address_physical(rtdg_addr),
         .raygen_bsr_addr =
            params->is_sbt_indirect ?
            (params->indirect_sbts_addr +
             offsetof(VkTraceRaysIndirectCommand2KHR,
                      raygenShaderRecordAddress)) :
            params->raygen_sbt->deviceAddress,
         .is_indirect = params->is_sbt_indirect,
         .local_group_size_log2 = {
            local_size_log2[0],
            local_size_log2[1],
            local_size_log2[2],
         },
      };
      STATIC_ASSERT(sizeof(trampoline_params) == 32);
      memcpy(cw.InlineData, &trampoline_params, sizeof(trampoline_params));
   }

   trace_intel_end_rays(&cmd_buffer->trace,
                        params->launch_size[0],
                        params->launch_size[1],
                        params->launch_size[2]);
}

void
genX(CmdTraceRaysKHR)(
    VkCommandBuffer                             commandBuffer,
    const VkStridedDeviceAddressRegionKHR*      pRaygenShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR*      pMissShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR*      pHitShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR*      pCallableShaderBindingTable,
    uint32_t                                    width,
    uint32_t                                    height,
    uint32_t                                    depth)
{
   ANV_FROM_HANDLE(anv_cmd_buffer, cmd_buffer, commandBuffer);
   struct trace_params params = {
      .is_sbt_indirect         = false,
      .raygen_sbt              = pRaygenShaderBindingTable,
      .miss_sbt                = pMissShaderBindingTable,
      .hit_sbt                 = pHitShaderBindingTable,
      .callable_sbt            = pCallableShaderBindingTable,
      .is_launch_size_indirect = false,
      .launch_size             = {
         width,
         height,
         depth,
      },
   };

   cmd_buffer_trace_rays(cmd_buffer, &params);
}

void
genX(CmdTraceRaysIndirectKHR)(
    VkCommandBuffer                             commandBuffer,
    const VkStridedDeviceAddressRegionKHR*      pRaygenShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR*      pMissShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR*      pHitShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR*      pCallableShaderBindingTable,
    VkDeviceAddress                             indirectDeviceAddress)
{
   ANV_FROM_HANDLE(anv_cmd_buffer, cmd_buffer, commandBuffer);
   struct trace_params params = {
      .is_sbt_indirect         = false,
      .raygen_sbt              = pRaygenShaderBindingTable,
      .miss_sbt                = pMissShaderBindingTable,
      .hit_sbt                 = pHitShaderBindingTable,
      .callable_sbt            = pCallableShaderBindingTable,
      .is_launch_size_indirect = true,
      .launch_size_addr        = indirectDeviceAddress,
   };

   cmd_buffer_trace_rays(cmd_buffer, &params);
}

void
genX(CmdTraceRaysIndirect2KHR)(
    VkCommandBuffer                             commandBuffer,
    VkDeviceAddress                             indirectDeviceAddress)
{
   ANV_FROM_HANDLE(anv_cmd_buffer, cmd_buffer, commandBuffer);
   struct trace_params params = {
      .is_sbt_indirect         = true,
      .indirect_sbts_addr      = indirectDeviceAddress,
      .is_launch_size_indirect = true,
      .launch_size_addr        = indirectDeviceAddress +
                                 offsetof(VkTraceRaysIndirectCommand2KHR, width),
   };

   cmd_buffer_trace_rays(cmd_buffer, &params);
}

#endif /* GFX_VERx10 >= 125 */
