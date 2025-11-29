/*
 * Copyright © 2024 Valve Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#include "lvp_private.h"
#include "lvp_acceleration_structure.h"
#include "lvp_nir_ray_tracing.h"

#include "vk_pipeline.h"

#include "nir.h"
#include "nir_builder.h"

#include "spirv/spirv.h"

#include "util/mesa-sha1.h"
#include "util/simple_mtx.h"

static void
lvp_init_ray_tracing_groups(struct lvp_pipeline *pipeline,
                            const VkRayTracingPipelineCreateInfoKHR *create_info)
{
   uint32_t i = 0;
   for (; i < create_info->groupCount; i++) {
      const VkRayTracingShaderGroupCreateInfoKHR *group_info = create_info->pGroups + i;
      struct lvp_ray_tracing_group *dst = pipeline->rt.groups + i;

      dst->recursive_index = VK_SHADER_UNUSED_KHR;
      dst->ahit_index = VK_SHADER_UNUSED_KHR;
      dst->isec_index = VK_SHADER_UNUSED_KHR;

      switch (group_info->type) {
      case VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR:
         if (group_info->generalShader != VK_SHADER_UNUSED_KHR) {
            dst->recursive_index = group_info->generalShader;
         }
         break;
      case VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR:
         if (group_info->closestHitShader != VK_SHADER_UNUSED_KHR) {
            dst->recursive_index = group_info->closestHitShader;
         }
         if (group_info->anyHitShader != VK_SHADER_UNUSED_KHR) {
            dst->ahit_index = group_info->anyHitShader;
         }
         break;
      case VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR:
         if (group_info->closestHitShader != VK_SHADER_UNUSED_KHR) {
            dst->recursive_index = group_info->closestHitShader;
         }
         if (group_info->intersectionShader != VK_SHADER_UNUSED_KHR) {
            dst->isec_index = group_info->intersectionShader;

            if (group_info->anyHitShader != VK_SHADER_UNUSED_KHR)
               dst->ahit_index = group_info->anyHitShader;
         }
         break;
      default:
         unreachable("Unimplemented VkRayTracingShaderGroupTypeKHR");
      }

      dst->handle.index = p_atomic_inc_return(&pipeline->device->group_handle_alloc);
   }

   if (!create_info->pLibraryInfo)
      return;

   uint32_t stage_base_index = create_info->stageCount;
   for (uint32_t library_index = 0; library_index < create_info->pLibraryInfo->libraryCount; library_index++) {
      VK_FROM_HANDLE(lvp_pipeline, library, create_info->pLibraryInfo->pLibraries[library_index]);
      for (uint32_t group_index = 0; group_index < library->rt.group_count; group_index++) {
         const struct lvp_ray_tracing_group *src = library->rt.groups + group_index;
         struct lvp_ray_tracing_group *dst = pipeline->rt.groups + i;

         dst->handle = src->handle;

         if (src->recursive_index != VK_SHADER_UNUSED_KHR)
            dst->recursive_index = stage_base_index + src->recursive_index;
         else
            dst->recursive_index = VK_SHADER_UNUSED_KHR;

         if (src->ahit_index != VK_SHADER_UNUSED_KHR)
            dst->ahit_index = stage_base_index + src->ahit_index;
         else
            dst->ahit_index = VK_SHADER_UNUSED_KHR;

         if (src->isec_index != VK_SHADER_UNUSED_KHR)
            dst->isec_index = stage_base_index + src->isec_index;
         else
            dst->isec_index = VK_SHADER_UNUSED_KHR;

         i++;
      }
      stage_base_index += library->rt.stage_count;
   }
}

static bool
lvp_lower_ray_tracing_derefs(nir_shader *shader)
{
   nir_function_impl *impl = nir_shader_get_entrypoint(shader);

   bool progress = false;

   nir_builder _b = nir_builder_at(nir_before_impl(impl));
   nir_builder *b = &_b;

   nir_def *arg_offset = nir_load_shader_call_data_offset_lvp(b);

   nir_foreach_block (block, impl) {
      nir_foreach_instr_safe (instr, block) {
         if (instr->type != nir_instr_type_deref)
            continue;

         nir_deref_instr *deref = nir_instr_as_deref(instr);
         if (!nir_deref_mode_is_one_of(deref, nir_var_shader_call_data |
                                       nir_var_ray_hit_attrib))
            continue;

         bool is_shader_call_data = nir_deref_mode_is(deref, nir_var_shader_call_data);

         deref->modes = nir_var_function_temp;
         progress = true;

         if (deref->deref_type == nir_deref_type_var) {
            b->cursor = nir_before_instr(&deref->instr);
            nir_def *offset = is_shader_call_data ? arg_offset : nir_imm_int(b, 0);
            nir_deref_instr *replacement =
               nir_build_deref_cast(b, offset, nir_var_function_temp, deref->var->type, 0);
            nir_def_rewrite_uses(&deref->def, &replacement->def);
            nir_instr_remove(&deref->instr);
         }
      }
   }

   if (progress)
      nir_metadata_preserve(impl, nir_metadata_block_index | nir_metadata_dominance);
   else
      nir_metadata_preserve(impl, nir_metadata_all);

   return progress;
}

static bool
lvp_move_ray_tracing_intrinsic(nir_builder *b, nir_intrinsic_instr *instr, void *data)
{
   switch (instr->intrinsic) {
   case nir_intrinsic_load_shader_record_ptr:
   case nir_intrinsic_load_ray_flags:
   case nir_intrinsic_load_ray_object_origin:
   case nir_intrinsic_load_ray_world_origin:
   case nir_intrinsic_load_ray_t_min:
   case nir_intrinsic_load_ray_object_direction:
   case nir_intrinsic_load_ray_world_direction:
   case nir_intrinsic_load_ray_t_max:
      nir_instr_move(nir_before_impl(b->impl), &instr->instr);
      return true;
   default:
      return false;
   }
}

static VkResult
lvp_compile_ray_tracing_stages(struct lvp_pipeline *pipeline,
                               const VkRayTracingPipelineCreateInfoKHR *create_info)
{
   VkResult result = VK_SUCCESS;

   uint32_t i = 0;
   for (; i < create_info->stageCount; i++) {
      nir_shader *nir;
      result = lvp_spirv_to_nir(pipeline, create_info->pStages + i, &nir);
      if (result != VK_SUCCESS)
         return result;

      assert(!nir->scratch_size);
      if (nir->info.stage == MESA_SHADER_ANY_HIT ||
          nir->info.stage == MESA_SHADER_CLOSEST_HIT ||
          nir->info.stage == MESA_SHADER_INTERSECTION)
         nir->scratch_size = LVP_RAY_HIT_ATTRIBS_SIZE;

      NIR_PASS(_, nir, nir_lower_vars_to_explicit_types,
               nir_var_function_temp | nir_var_shader_call_data | nir_var_ray_hit_attrib,
               glsl_get_natural_size_align_bytes);

      NIR_PASS(_, nir, lvp_lower_ray_tracing_derefs);

      NIR_PASS(_, nir, nir_lower_explicit_io, nir_var_function_temp, nir_address_format_32bit_offset);

      NIR_PASS(_, nir, nir_shader_intrinsics_pass, lvp_move_ray_tracing_intrinsic,
               nir_metadata_block_index | nir_metadata_dominance, NULL);

      pipeline->rt.stages[i] = lvp_create_pipeline_nir(nir);
      if (!pipeline->rt.stages[i]) {
         result = VK_ERROR_OUT_OF_HOST_MEMORY;
         ralloc_free(nir);
         return result;
      }
   }

   if (!create_info->pLibraryInfo)
      return result;

   for (uint32_t library_index = 0; library_index < create_info->pLibraryInfo->libraryCount; library_index++) {
      VK_FROM_HANDLE(lvp_pipeline, library, create_info->pLibraryInfo->pLibraries[library_index]);
      for (uint32_t stage_index = 0; stage_index < library->rt.stage_count; stage_index++) {
         lvp_pipeline_nir_ref(pipeline->rt.stages + i, library->rt.stages[stage_index]);
         i++;
      }
   }

   return result;
}

static nir_def *
lvp_load_trace_ray_command_field(nir_builder *b, uint32_t command_offset,
                                 uint32_t num_components, uint32_t bit_size)
{
   return nir_load_ssbo(b, num_components, bit_size, nir_imm_int(b, 0),
                        nir_imm_int(b, command_offset));
}

struct lvp_sbt_entry {
   nir_def *value;
   nir_def *shader_record_ptr;
};

static struct lvp_sbt_entry
lvp_load_sbt_entry(nir_builder *b, nir_def *index,
                   uint32_t command_offset, uint32_t index_offset)
{
   nir_def *addr = lvp_load_trace_ray_command_field(b, command_offset, 1, 64);

   if (index) {
      /* The 32 high bits of stride can be ignored. */
      nir_def *stride = lvp_load_trace_ray_command_field(
         b, command_offset + sizeof(VkDeviceSize) * 2, 1, 32);
      addr = nir_iadd(b, addr, nir_u2u64(b, nir_imul(b, index, stride)));
   }

   return (struct lvp_sbt_entry) {
      .value = nir_build_load_global(b, 1, 32, nir_iadd_imm(b, addr, index_offset)),
      .shader_record_ptr = nir_iadd_imm(b, addr, LVP_RAY_TRACING_GROUP_HANDLE_SIZE),
   };
}

struct lvp_ray_traversal_state {
   nir_variable *origin;
   nir_variable *dir;
   nir_variable *inv_dir;
   nir_variable *bvh_base;
   nir_variable *current_node;
   nir_variable *stack_base;
   nir_variable *stack_ptr;
   nir_variable *stack;
   nir_variable *hit;

   nir_variable *instance_addr;
   nir_variable *sbt_offset_and_flags;
};

struct lvp_ray_tracing_state {
   nir_variable *bvh_base;
   nir_variable *flags;
   nir_variable *cull_mask;
   nir_variable *sbt_offset;
   nir_variable *sbt_stride;
   nir_variable *miss_index;
   nir_variable *origin;
   nir_variable *tmin;
   nir_variable *dir;
   nir_variable *tmax;

   nir_variable *instance_addr;
   nir_variable *primitive_id;
   nir_variable *geometry_id_and_flags;
   nir_variable *hit_kind;
   nir_variable *sbt_index;

   nir_variable *shader_record_ptr;
   nir_variable *stack_ptr;
   nir_variable *shader_call_data_offset;

   nir_variable *accept;
   nir_variable *terminate;
   nir_variable *opaque;

   struct lvp_ray_traversal_state traversal;
};

struct lvp_ray_tracing_pipeline_compiler {
   struct lvp_pipeline *pipeline;
   VkPipelineCreateFlags2KHR flags;

   struct lvp_ray_tracing_state state;

   struct hash_table *functions;

   uint32_t raygen_size;
   uint32_t ahit_size;
   uint32_t chit_size;
   uint32_t miss_size;
   uint32_t isec_size;
   uint32_t callable_size;
};

static uint32_t
lvp_ray_tracing_pipeline_compiler_get_stack_size(
   struct lvp_ray_tracing_pipeline_compiler *compiler, nir_function *function)
{
   hash_table_foreach(compiler->functions, entry) {
      if (entry->data == function) {
         const nir_shader *shader = entry->key;
         return shader->scratch_size;
      }
   }
   return 0;
}

static void
lvp_ray_tracing_state_init(nir_shader *nir, struct lvp_ray_tracing_state *state)
{
   state->bvh_base = nir_variable_create(nir, nir_var_shader_temp, glsl_uint64_t_type(), "bvh_base");
   state->flags = nir_variable_create(nir, nir_var_shader_temp, glsl_uint_type(), "flags");
   state->cull_mask = nir_variable_create(nir, nir_var_shader_temp, glsl_uint_type(), "cull_mask");
   state->sbt_offset = nir_variable_create(nir, nir_var_shader_temp, glsl_uint_type(), "sbt_offset");
   state->sbt_stride = nir_variable_create(nir, nir_var_shader_temp, glsl_uint_type(), "sbt_stride");
   state->miss_index = nir_variable_create(nir, nir_var_shader_temp, glsl_uint_type(), "miss_index");
   state->origin = nir_variable_create(nir, nir_var_shader_temp, glsl_vec_type(3), "origin");
   state->tmin = nir_variable_create(nir, nir_var_shader_temp, glsl_float_type(), "tmin");
   state->dir = nir_variable_create(nir, nir_var_shader_temp, glsl_vec_type(3), "dir");
   state->tmax = nir_variable_create(nir, nir_var_shader_temp, glsl_float_type(), "tmax");

   state->instance_addr = nir_variable_create(nir, nir_var_shader_temp, glsl_uint64_t_type(), "instance_addr");
   state->primitive_id = nir_variable_create(nir, nir_var_shader_temp, glsl_uint_type(), "primitive_id");
   state->geometry_id_and_flags = nir_variable_create(nir, nir_var_shader_temp, glsl_uint_type(), "geometry_id_and_flags");
   state->hit_kind = nir_variable_create(nir, nir_var_shader_temp, glsl_uint_type(), "hit_kind");
   state->sbt_index = nir_variable_create(nir, nir_var_shader_temp, glsl_uint_type(), "sbt_index");

   state->shader_record_ptr = nir_variable_create(nir, nir_var_shader_temp, glsl_uint64_t_type(), "shader_record_ptr");
   state->stack_ptr = nir_variable_create(nir, nir_var_shader_temp, glsl_uint_type(), "stack_ptr");
   state->shader_call_data_offset = nir_variable_create(nir, nir_var_shader_temp, glsl_uint_type(), "shader_call_data_offset");

   state->accept = nir_variable_create(nir, nir_var_shader_temp, glsl_bool_type(), "accept");
   state->terminate = nir_variable_create(nir, nir_var_shader_temp, glsl_bool_type(), "terminate");
   state->opaque = nir_variable_create(nir, nir_var_shader_temp, glsl_bool_type(), "opaque");
}

static void
lvp_ray_traversal_state_init(nir_function_impl *impl, struct lvp_ray_traversal_state *state)
{
   state->origin = nir_local_variable_create(impl, glsl_vec_type(3), "traversal.origin");
   state->dir = nir_local_variable_create(impl, glsl_vec_type(3), "traversal.dir");
   state->inv_dir = nir_local_variable_create(impl, glsl_vec_type(3), "traversal.inv_dir");
   state->bvh_base = nir_local_variable_create(impl, glsl_uint64_t_type(), "traversal.bvh_base");
   state->current_node = nir_local_variable_create(impl, glsl_uint_type(), "traversal.current_node");
   state->stack_base = nir_local_variable_create(impl, glsl_uint_type(), "traversal.stack_base");
   state->stack_ptr = nir_local_variable_create(impl, glsl_uint_type(), "traversal.stack_ptr");
   state->stack = nir_local_variable_create(impl, glsl_array_type(glsl_uint_type(), 24 * 2, 0), "traversal.stack");
   state->hit = nir_local_variable_create(impl, glsl_bool_type(), "traversal.hit");

   state->instance_addr = nir_local_variable_create(impl, glsl_uint64_t_type(), "traversal.instance_addr");
   state->sbt_offset_and_flags = nir_local_variable_create(impl, glsl_uint_type(), "traversal.sbt_offset_and_flags");
}

static void
lvp_call_ray_tracing_stage(nir_builder *b, struct lvp_ray_tracing_pipeline_compiler *compiler, nir_shader *stage)
{
   nir_function *function;

   struct hash_entry *entry = _mesa_hash_table_search(compiler->functions, stage);
   if (entry) {
      function = entry->data;
   } else {
      nir_function_impl *stage_entrypoint = nir_shader_get_entrypoint(stage);
      nir_function_impl *copy = nir_function_impl_clone(b->shader, stage_entrypoint);

      struct hash_table *var_remap = _mesa_pointer_hash_table_create(NULL);

      nir_foreach_block(block, copy) {
         nir_foreach_instr_safe(instr, block) {
            if (instr->type != nir_instr_type_deref)
               continue;

            nir_deref_instr *deref = nir_instr_as_deref(instr);
            if (deref->deref_type != nir_deref_type_var ||
                deref->var->data.mode == nir_var_function_temp)
               continue;

            struct hash_entry *entry =
               _mesa_hash_table_search(var_remap, deref->var);
            if (!entry) {
               nir_variable *new_var = nir_variable_clone(deref->var, b->shader);
               nir_shader_add_variable(b->shader, new_var);
               entry = _mesa_hash_table_insert(var_remap,
                                               deref->var, new_var);
            }
            deref->var = entry->data;
         }
      }

      function = nir_function_create(
         b->shader, _mesa_shader_stage_to_string(stage->info.stage));
      nir_function_set_impl(function, copy);

      ralloc_free(var_remap);

      _mesa_hash_table_insert(compiler->functions, stage, function);
   }

   nir_build_call(b, function, 0, NULL);

   switch(stage->info.stage) {
   case MESA_SHADER_RAYGEN:
      compiler->raygen_size = MAX2(compiler->raygen_size, stage->scratch_size);
      break;
   case MESA_SHADER_ANY_HIT:
      compiler->ahit_size = MAX2(compiler->ahit_size, stage->scratch_size);
      break;
   case MESA_SHADER_CLOSEST_HIT:
      compiler->chit_size = MAX2(compiler->chit_size, stage->scratch_size);
      break;
   case MESA_SHADER_MISS:
      compiler->miss_size = MAX2(compiler->miss_size, stage->scratch_size);
      break;
   case MESA_SHADER_INTERSECTION:
      compiler->isec_size = MAX2(compiler->isec_size, stage->scratch_size);
      break;
   case MESA_SHADER_CALLABLE:
      compiler->callable_size = MAX2(compiler->callable_size, stage->scratch_size);
      break;
   default:
      unreachable("Invalid ray tracing stage");
      break;
   }
}

static void
lvp_execute_callable(nir_builder *b, struct lvp_ray_tracing_pipeline_compiler *compiler,
                     nir_intrinsic_instr *instr)
{
   struct lvp_ray_tracing_state *state = &compiler->state;

   nir_def *sbt_index = instr->src[0].ssa;
   nir_def *payload = instr->src[1].ssa;

   struct lvp_sbt_entry callable_entry = lvp_load_sbt_entry(
      b,
      sbt_index,
      offsetof(VkTraceRaysIndirectCommand2KHR, callableShaderBindingTableAddress),
      offsetof(struct lvp_ray_tracing_group_handle, index));
   nir_store_var(b, compiler->state.shader_record_ptr, callable_entry.shader_record_ptr, 0x1);

   uint32_t stack_size =
      lvp_ray_tracing_pipeline_compiler_get_stack_size(compiler, b->impl->function);
   nir_def *stack_ptr = nir_load_var(b, state->stack_ptr);
   nir_store_var(b, state->stack_ptr, nir_iadd_imm(b, stack_ptr, stack_size), 0x1);

   nir_store_var(b, state->shader_call_data_offset, nir_iadd_imm(b, payload, -stack_size), 0x1);

   for (uint32_t i = 0; i < compiler->pipeline->rt.group_count; i++) {
      struct lvp_ray_tracing_group *group = compiler->pipeline->rt.groups + i;
      if (group->recursive_index == VK_SHADER_UNUSED_KHR)
         continue;

      nir_shader *stage = compiler->pipeline->rt.stages[group->recursive_index]->nir;
      if (stage->info.stage != MESA_SHADER_CALLABLE)
         continue;

      nir_push_if(b, nir_ieq_imm(b, callable_entry.value, group->handle.index));
      lvp_call_ray_tracing_stage(b, compiler, stage);
      nir_pop_if(b, NULL);
   }

   nir_store_var(b, state->stack_ptr, stack_ptr, 0x1);
}

struct lvp_lower_isec_intrinsic_state {
   struct lvp_ray_tracing_pipeline_compiler *compiler;
   nir_shader *ahit;
};

static bool
lvp_lower_isec_intrinsic(nir_builder *b, nir_intrinsic_instr *instr, void *data)
{
   if (instr->intrinsic != nir_intrinsic_report_ray_intersection)
      return false;

   struct lvp_lower_isec_intrinsic_state *isec_state = data;
   struct lvp_ray_tracing_pipeline_compiler *compiler = isec_state->compiler;
   struct lvp_ray_tracing_state *state = &compiler->state;

   b->cursor = nir_after_instr(&instr->instr);

   nir_def *t = instr->src[0].ssa;
   nir_def *hit_kind = instr->src[1].ssa;

   nir_def *prev_accept = nir_load_var(b, state->accept);
   nir_def *prev_tmax = nir_load_var(b, state->tmax);
   nir_def *prev_hit_kind = nir_load_var(b, state->hit_kind);

   nir_variable *commit = nir_local_variable_create(b->impl, glsl_bool_type(), "commit");
   nir_store_var(b, commit, nir_imm_false(b), 0x1);

   nir_push_if(b, nir_iand(b, nir_fge(b, t, nir_load_var(b, state->tmin)), nir_fge(b, nir_load_var(b, state->tmax), t)));
   {
      nir_store_var(b, state->accept, nir_imm_true(b), 0x1);

      nir_store_var(b, state->tmax, t, 1);
      nir_store_var(b, state->hit_kind, hit_kind, 1);

      if (isec_state->ahit) {
         nir_def *prev_terminate = nir_load_var(b, state->terminate);
         nir_store_var(b, state->terminate, nir_imm_false(b), 0x1);

         nir_push_if(b, nir_inot(b, nir_load_var(b, state->opaque)));
         {
            lvp_call_ray_tracing_stage(b, compiler, isec_state->ahit);
         }
         nir_pop_if(b, NULL);

         nir_def *terminate = nir_load_var(b, state->terminate);
         nir_store_var(b, state->terminate, nir_ior(b, terminate, prev_terminate), 0x1);

         nir_push_if(b, terminate);
         nir_jump(b, nir_jump_return);
         nir_pop_if(b, NULL);
      }

      nir_push_if(b, nir_load_var(b, state->accept));
      {
         nir_store_var(b, commit, nir_imm_true(b), 0x1);
      }
      nir_push_else(b, NULL);
      {
         nir_store_var(b, state->accept, prev_accept, 0x1);
         nir_store_var(b, state->tmax, prev_tmax, 1);
         nir_store_var(b, state->hit_kind, prev_hit_kind, 1);
      }
      nir_pop_if(b, NULL);
   }
   nir_pop_if(b, NULL);

   nir_def_rewrite_uses(&instr->def, nir_load_var(b, commit));
   nir_instr_remove(&instr->instr);

   return true;
}

static void
lvp_handle_aabb_intersection(nir_builder *b, struct lvp_leaf_intersection *intersection,
                             const struct lvp_ray_traversal_args *args,
                             const struct lvp_ray_flags *ray_flags)
{
   struct lvp_ray_tracing_pipeline_compiler *compiler = args->data;
   struct lvp_ray_tracing_state *state = &compiler->state;

   nir_store_var(b, state->accept, nir_imm_false(b), 0x1);
   nir_store_var(b, state->terminate, ray_flags->terminate_on_first_hit, 0x1);
   nir_store_var(b, state->opaque, intersection->opaque, 0x1);

   nir_def *prev_instance_addr = nir_load_var(b, state->instance_addr);
   nir_def *prev_primitive_id = nir_load_var(b, state->primitive_id);
   nir_def *prev_geometry_id_and_flags = nir_load_var(b, state->geometry_id_and_flags);

   nir_store_var(b, state->instance_addr, nir_load_var(b, state->traversal.instance_addr), 0x1);
   nir_store_var(b, state->primitive_id, intersection->primitive_id, 0x1);
   nir_store_var(b, state->geometry_id_and_flags, intersection->geometry_id_and_flags, 0x1);

   nir_def *geometry_id = nir_iand_imm(b, intersection->geometry_id_and_flags, 0xfffffff);
   nir_def *sbt_index =
      nir_iadd(b,
               nir_iadd(b, nir_load_var(b, state->sbt_offset),
                        nir_iand_imm(b, nir_load_var(b, state->traversal.sbt_offset_and_flags), 0xffffff)),
               nir_imul(b, nir_load_var(b, state->sbt_stride), geometry_id));

   struct lvp_sbt_entry isec_entry = lvp_load_sbt_entry(
      b,
      sbt_index,
      offsetof(VkTraceRaysIndirectCommand2KHR, hitShaderBindingTableAddress),
      offsetof(struct lvp_ray_tracing_group_handle, index));
   nir_store_var(b, compiler->state.shader_record_ptr, isec_entry.shader_record_ptr, 0x1);

   for (uint32_t i = 0; i < compiler->pipeline->rt.group_count; i++) {
      struct lvp_ray_tracing_group *group = compiler->pipeline->rt.groups + i;
      if (group->isec_index == VK_SHADER_UNUSED_KHR)
         continue;

      nir_shader *stage = compiler->pipeline->rt.stages[group->isec_index]->nir;

      nir_push_if(b, nir_ieq_imm(b, isec_entry.value, group->handle.index));
      lvp_call_ray_tracing_stage(b, compiler, stage);
      nir_pop_if(b, NULL);

      nir_shader *ahit_stage = NULL;
      if (group->ahit_index != VK_SHADER_UNUSED_KHR)
         ahit_stage = compiler->pipeline->rt.stages[group->ahit_index]->nir;

      struct lvp_lower_isec_intrinsic_state isec_state = {
         .compiler = compiler,
         .ahit = ahit_stage,
      };
      nir_shader_intrinsics_pass(b->shader, lvp_lower_isec_intrinsic,
                                 nir_metadata_none, &isec_state);
   }

   nir_push_if(b, nir_load_var(b, state->accept));
   {
      nir_store_var(b, state->sbt_index, sbt_index, 0x1);
      nir_store_var(b, state->traversal.hit, nir_imm_true(b), 0x1);

      nir_push_if(b, nir_load_var(b, state->terminate));
      nir_jump(b, nir_jump_break);
      nir_pop_if(b, NULL);
   }
   nir_push_else(b, NULL);
   {
      nir_store_var(b, state->instance_addr, prev_instance_addr, 0x1);
      nir_store_var(b, state->primitive_id, prev_primitive_id, 0x1);
      nir_store_var(b, state->geometry_id_and_flags, prev_geometry_id_and_flags, 0x1);
   }
   nir_pop_if(b, NULL);
}

static void
lvp_handle_triangle_intersection(nir_builder *b,
                                 struct lvp_triangle_intersection *intersection,
                                 const struct lvp_ray_traversal_args *args,
                                 const struct lvp_ray_flags *ray_flags)
{
   struct lvp_ray_tracing_pipeline_compiler *compiler = args->data;
   struct lvp_ray_tracing_state *state = &compiler->state;

   nir_store_var(b, state->accept, nir_imm_true(b), 0x1);
   nir_store_var(b, state->terminate, ray_flags->terminate_on_first_hit, 0x1);

   nir_def *barycentrics_offset = nir_load_var(b, state->stack_ptr);

   nir_def *prev_tmax = nir_load_var(b, state->tmax);
   nir_def *prev_instance_addr = nir_load_var(b, state->instance_addr);
   nir_def *prev_primitive_id = nir_load_var(b, state->primitive_id);
   nir_def *prev_geometry_id_and_flags = nir_load_var(b, state->geometry_id_and_flags);
   nir_def *prev_hit_kind = nir_load_var(b, state->hit_kind);
   nir_def *prev_barycentrics = nir_load_scratch(b, 2, 32, barycentrics_offset);

   nir_store_var(b, state->tmax, intersection->t, 0x1);
   nir_store_var(b, state->instance_addr, nir_load_var(b, state->traversal.instance_addr), 0x1);
   nir_store_var(b, state->primitive_id, intersection->base.primitive_id, 0x1);
   nir_store_var(b, state->geometry_id_and_flags, intersection->base.geometry_id_and_flags, 0x1);
   nir_store_var(b, state->hit_kind,
                 nir_bcsel(b, intersection->frontface, nir_imm_int(b, 0xFE), nir_imm_int(b, 0xFF)), 0x1);

   nir_store_scratch(b, intersection->barycentrics, barycentrics_offset);

   nir_def *geometry_id = nir_iand_imm(b, intersection->base.geometry_id_and_flags, 0xfffffff);
   nir_def *sbt_index =
      nir_iadd(b,
               nir_iadd(b, nir_load_var(b, state->sbt_offset),
                        nir_iand_imm(b, nir_load_var(b, state->traversal.sbt_offset_and_flags), 0xffffff)),
               nir_imul(b, nir_load_var(b, state->sbt_stride), geometry_id));

   nir_push_if(b, nir_inot(b, intersection->base.opaque));
   {
      struct lvp_sbt_entry ahit_entry = lvp_load_sbt_entry(
         b,
         sbt_index,
         offsetof(VkTraceRaysIndirectCommand2KHR, hitShaderBindingTableAddress),
         offsetof(struct lvp_ray_tracing_group_handle, index));
      nir_store_var(b, compiler->state.shader_record_ptr, ahit_entry.shader_record_ptr, 0x1);

      for (uint32_t i = 0; i < compiler->pipeline->rt.group_count; i++) {
         struct lvp_ray_tracing_group *group = compiler->pipeline->rt.groups + i;
         if (group->ahit_index == VK_SHADER_UNUSED_KHR)
            continue;

         nir_shader *stage = compiler->pipeline->rt.stages[group->ahit_index]->nir;

         nir_push_if(b, nir_ieq_imm(b, ahit_entry.value, group->handle.index));
         lvp_call_ray_tracing_stage(b, compiler, stage);
         nir_pop_if(b, NULL);
      }
   }
   nir_pop_if(b, NULL);

   nir_push_if(b, nir_load_var(b, state->accept));
   {
      nir_store_var(b, state->sbt_index, sbt_index, 0x1);
      nir_store_var(b, state->traversal.hit, nir_imm_true(b), 0x1);

      nir_push_if(b, nir_load_var(b, state->terminate));
      nir_jump(b, nir_jump_break);
      nir_pop_if(b, NULL);
   }
   nir_push_else(b, NULL);
   {
      nir_store_var(b, state->tmax, prev_tmax, 0x1);
      nir_store_var(b, state->instance_addr, prev_instance_addr, 0x1);
      nir_store_var(b, state->primitive_id, prev_primitive_id, 0x1);
      nir_store_var(b, state->geometry_id_and_flags, prev_geometry_id_and_flags, 0x1);
      nir_store_var(b, state->hit_kind, prev_hit_kind, 0x1);
      nir_store_scratch(b, prev_barycentrics, barycentrics_offset);
   }
   nir_pop_if(b, NULL);
}

static void
lvp_trace_ray(nir_builder *b, struct lvp_ray_tracing_pipeline_compiler *compiler,
              nir_intrinsic_instr *instr)
{
   struct lvp_ray_tracing_state *state = &compiler->state;

   nir_def *accel_struct = instr->src[0].ssa;
   nir_def *flags = instr->src[1].ssa;
   nir_def *cull_mask = instr->src[2].ssa;
   nir_def *sbt_offset = nir_iand_imm(b, instr->src[3].ssa, 0xF);
   nir_def *sbt_stride = nir_iand_imm(b, instr->src[4].ssa, 0xF);
   nir_def *miss_index = nir_iand_imm(b, instr->src[5].ssa, 0xFFFF);
   nir_def *origin = instr->src[6].ssa;
   nir_def *tmin = instr->src[7].ssa;
   nir_def *dir = instr->src[8].ssa;
   nir_def *tmax = instr->src[9].ssa;
   nir_def *payload = instr->src[10].ssa;

   uint32_t stack_size =
      lvp_ray_tracing_pipeline_compiler_get_stack_size(compiler, b->impl->function);
   nir_def *stack_ptr = nir_load_var(b, state->stack_ptr);
   nir_store_var(b, state->stack_ptr, nir_iadd_imm(b, stack_ptr, stack_size), 0x1);

   nir_store_var(b, state->shader_call_data_offset, nir_iadd_imm(b, payload, -stack_size), 0x1);

   nir_def *bvh_base = accel_struct;
   if (bvh_base->bit_size != 64) {
      assert(bvh_base->num_components >= 2);
      bvh_base = nir_load_ubo(
         b, 1, 64, nir_channel(b, accel_struct, 0),
         nir_imul_imm(b, nir_channel(b, accel_struct, 1), sizeof(struct lp_descriptor)), .range = ~0);
   }

   lvp_ray_traversal_state_init(b->impl, &state->traversal);

   nir_store_var(b, state->bvh_base, bvh_base, 0x1);
   nir_store_var(b, state->flags, flags, 0x1);
   nir_store_var(b, state->cull_mask, cull_mask, 0x1);
   nir_store_var(b, state->sbt_offset, sbt_offset, 0x1);
   nir_store_var(b, state->sbt_stride, sbt_stride, 0x1);
   nir_store_var(b, state->miss_index, miss_index, 0x1);
   nir_store_var(b, state->origin, origin, 0x7);
   nir_store_var(b, state->tmin, tmin, 0x1);
   nir_store_var(b, state->dir, dir, 0x7);
   nir_store_var(b, state->tmax, tmax, 0x1);

   nir_store_var(b, state->traversal.bvh_base, bvh_base, 0x1);
   nir_store_var(b, state->traversal.origin, origin, 0x7);
   nir_store_var(b, state->traversal.dir, dir, 0x7);
   nir_store_var(b, state->traversal.inv_dir, nir_frcp(b, dir), 0x7);
   nir_store_var(b, state->traversal.current_node, nir_imm_int(b, LVP_BVH_ROOT_NODE), 0x1);
   nir_store_var(b, state->traversal.stack_base, nir_imm_int(b, -1), 0x1);
   nir_store_var(b, state->traversal.stack_ptr, nir_imm_int(b, 0), 0x1);

   nir_store_var(b, state->traversal.hit, nir_imm_false(b), 0x1);

   struct lvp_ray_traversal_vars vars = {
      .tmax = nir_build_deref_var(b, state->tmax),
      .origin = nir_build_deref_var(b, state->traversal.origin),
      .dir = nir_build_deref_var(b, state->traversal.dir),
      .inv_dir = nir_build_deref_var(b, state->traversal.inv_dir),
      .bvh_base = nir_build_deref_var(b, state->traversal.bvh_base),
      .current_node = nir_build_deref_var(b, state->traversal.current_node),
      .stack_base = nir_build_deref_var(b, state->traversal.stack_base),
      .stack_ptr = nir_build_deref_var(b, state->traversal.stack_ptr),
      .stack = nir_build_deref_var(b, state->traversal.stack),
      .instance_addr = nir_build_deref_var(b, state->traversal.instance_addr),
      .sbt_offset_and_flags = nir_build_deref_var(b, state->traversal.sbt_offset_and_flags),
   };

   struct lvp_ray_traversal_args args = {
      .root_bvh_base = bvh_base,
      .flags = flags,
      .cull_mask = nir_ishl_imm(b, cull_mask, 24),
      .origin = origin,
      .tmin = tmin,
      .dir = dir,
      .vars = vars,
      .aabb_cb = (compiler->flags & VK_PIPELINE_CREATE_2_RAY_TRACING_SKIP_AABBS_BIT_KHR) ?
                 NULL : lvp_handle_aabb_intersection,
      .triangle_cb = (compiler->flags & VK_PIPELINE_CREATE_2_RAY_TRACING_SKIP_TRIANGLES_BIT_KHR) ?
                     NULL : lvp_handle_triangle_intersection,
      .data = compiler,
   };

   nir_push_if(b, nir_ine_imm(b, bvh_base, 0));
   lvp_build_ray_traversal(b, &args);
   nir_pop_if(b, NULL);

   nir_push_if(b, nir_load_var(b, state->traversal.hit));
   {
      nir_def *skip_chit = nir_test_mask(b, flags, SpvRayFlagsSkipClosestHitShaderKHRMask);
      nir_push_if(b, nir_inot(b, skip_chit));

      struct lvp_sbt_entry chit_entry = lvp_load_sbt_entry(
         b,
         nir_load_var(b, state->sbt_index),
         offsetof(VkTraceRaysIndirectCommand2KHR, hitShaderBindingTableAddress),
         offsetof(struct lvp_ray_tracing_group_handle, index));
      nir_store_var(b, compiler->state.shader_record_ptr, chit_entry.shader_record_ptr, 0x1);

      for (uint32_t i = 0; i < compiler->pipeline->rt.group_count; i++) {
         struct lvp_ray_tracing_group *group = compiler->pipeline->rt.groups + i;
         if (group->recursive_index == VK_SHADER_UNUSED_KHR)
            continue;

         nir_shader *stage = compiler->pipeline->rt.stages[group->recursive_index]->nir;
         if (stage->info.stage != MESA_SHADER_CLOSEST_HIT)
            continue;

         nir_push_if(b, nir_ieq_imm(b, chit_entry.value, group->handle.index));
         lvp_call_ray_tracing_stage(b, compiler, stage);
         nir_pop_if(b, NULL);
      }

      nir_pop_if(b, NULL);
   }
   nir_push_else(b, NULL);
   {
      struct lvp_sbt_entry miss_entry = lvp_load_sbt_entry(
         b,
         miss_index,
         offsetof(VkTraceRaysIndirectCommand2KHR, missShaderBindingTableAddress),
         offsetof(struct lvp_ray_tracing_group_handle, index));
      nir_store_var(b, compiler->state.shader_record_ptr, miss_entry.shader_record_ptr, 0x1);

      for (uint32_t i = 0; i < compiler->pipeline->rt.group_count; i++) {
         struct lvp_ray_tracing_group *group = compiler->pipeline->rt.groups + i;
         if (group->recursive_index == VK_SHADER_UNUSED_KHR)
            continue;

         nir_shader *stage = compiler->pipeline->rt.stages[group->recursive_index]->nir;
         if (stage->info.stage != MESA_SHADER_MISS)
            continue;

         nir_push_if(b, nir_ieq_imm(b, miss_entry.value, group->handle.index));
         lvp_call_ray_tracing_stage(b, compiler, stage);
         nir_pop_if(b, NULL);
      }
   }
   nir_pop_if(b, NULL);

   nir_store_var(b, state->stack_ptr, stack_ptr, 0x1);
}

static bool
lvp_lower_ray_tracing_instr(nir_builder *b, nir_instr *instr, void *data)
{
   struct lvp_ray_tracing_pipeline_compiler *compiler = data;
   struct lvp_ray_tracing_state *state = &compiler->state;

   if (instr->type == nir_instr_type_jump) {
      nir_jump_instr *jump = nir_instr_as_jump(instr);
      if (jump->type == nir_jump_halt) {
         jump->type = nir_jump_return;
         return true;
      }
      return false;
   } else if (instr->type != nir_instr_type_intrinsic) {
      return false;
   }

   nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);

   nir_def *def = NULL;

   b->cursor = nir_before_instr(instr);

   switch (intr->intrinsic) {
   /* Ray tracing instructions */
   case nir_intrinsic_execute_callable:
      lvp_execute_callable(b, compiler, intr);
      break;
   case nir_intrinsic_trace_ray:
      lvp_trace_ray(b, compiler, intr);
      break;
   case nir_intrinsic_ignore_ray_intersection: {
      nir_store_var(b, state->accept, nir_imm_false(b), 0x1);

      nir_push_if(b, nir_imm_true(b));
      nir_jump(b, nir_jump_return);
      nir_pop_if(b, NULL);
      break;
   }
   case nir_intrinsic_terminate_ray: {
      nir_store_var(b, state->accept, nir_imm_true(b), 0x1);
      nir_store_var(b, state->terminate, nir_imm_true(b), 0x1);

      nir_push_if(b, nir_imm_true(b));
      nir_jump(b, nir_jump_return);
      nir_pop_if(b, NULL);
      break;
   }
   /* Ray tracing system values */
   case nir_intrinsic_load_ray_launch_id:
      def = nir_load_global_invocation_id(b, 32);
      break;
   case nir_intrinsic_load_ray_launch_size:
      def = lvp_load_trace_ray_command_field(
         b, offsetof(VkTraceRaysIndirectCommand2KHR, width), 3, 32);
      break;
   case nir_intrinsic_load_shader_record_ptr:
      def = nir_load_var(b, state->shader_record_ptr);
      break;
   case nir_intrinsic_load_ray_t_min:
      def = nir_load_var(b, state->tmin);
      break;
   case nir_intrinsic_load_ray_t_max:
      def = nir_load_var(b, state->tmax);
      break;
   case nir_intrinsic_load_ray_world_origin:
      def = nir_load_var(b, state->origin);
      break;
   case nir_intrinsic_load_ray_world_direction:
      def = nir_load_var(b, state->dir);
      break;
   case nir_intrinsic_load_ray_instance_custom_index: {
      nir_def *instance_node_addr = nir_load_var(b, state->instance_addr);
      nir_def *custom_instance_and_mask = nir_build_load_global(
         b, 1, 32,
         nir_iadd_imm(b, instance_node_addr, offsetof(struct lvp_bvh_instance_node, custom_instance_and_mask)));
      def = nir_iand_imm(b, custom_instance_and_mask, 0xFFFFFF);
      break;
   }
   case nir_intrinsic_load_primitive_id:
      def = nir_load_var(b, state->primitive_id);
      break;
   case nir_intrinsic_load_ray_geometry_index:
      def = nir_load_var(b, state->geometry_id_and_flags);
      def = nir_iand_imm(b, def, 0xFFFFFFF);
      break;
   case nir_intrinsic_load_instance_id: {
      nir_def *instance_node_addr = nir_load_var(b, state->instance_addr);
      def = nir_build_load_global(
         b, 1, 32, nir_iadd_imm(b, instance_node_addr, offsetof(struct lvp_bvh_instance_node, instance_id)));
      break;
   }
   case nir_intrinsic_load_ray_flags:
      def = nir_load_var(b, state->flags);
      break;
   case nir_intrinsic_load_ray_hit_kind:
      def = nir_load_var(b, state->hit_kind);
      break;
   case nir_intrinsic_load_ray_world_to_object: {
      unsigned c = nir_intrinsic_column(intr);
      nir_def *instance_node_addr = nir_load_var(b, state->instance_addr);
      nir_def *wto_matrix[3];
      lvp_load_wto_matrix(b, instance_node_addr, wto_matrix);

      nir_def *vals[3];
      for (unsigned i = 0; i < 3; ++i)
         vals[i] = nir_channel(b, wto_matrix[i], c);

      def = nir_vec(b, vals, 3);
      break;
   }
   case nir_intrinsic_load_ray_object_to_world: {
      unsigned c = nir_intrinsic_column(intr);
      nir_def *instance_node_addr = nir_load_var(b, state->instance_addr);
      nir_def *rows[3];
      for (unsigned r = 0; r < 3; ++r)
         rows[r] = nir_build_load_global(
            b, 4, 32,
            nir_iadd_imm(b, instance_node_addr, offsetof(struct lvp_bvh_instance_node, otw_matrix) + r * 16));
      def = nir_vec3(b, nir_channel(b, rows[0], c), nir_channel(b, rows[1], c), nir_channel(b, rows[2], c));
      break;
   }
   case nir_intrinsic_load_ray_object_origin: {
      nir_def *instance_node_addr = nir_load_var(b, state->instance_addr);
      nir_def *wto_matrix[3];
      lvp_load_wto_matrix(b, instance_node_addr, wto_matrix);
      def = lvp_mul_vec3_mat(b, nir_load_var(b, state->origin), wto_matrix, true);
      break;
   }
   case nir_intrinsic_load_ray_object_direction: {
      nir_def *instance_node_addr = nir_load_var(b, state->instance_addr);
      nir_def *wto_matrix[3];
      lvp_load_wto_matrix(b, instance_node_addr, wto_matrix);
      def = lvp_mul_vec3_mat(b, nir_load_var(b, state->dir), wto_matrix, false);
      break;
   }
   case nir_intrinsic_load_cull_mask:
      def = nir_iand_imm(b, nir_load_var(b, state->cull_mask), 0xFF);
      break;
   /* Ray tracing stack lowering */
   case nir_intrinsic_load_scratch: {
      nir_src_rewrite(&intr->src[0], nir_iadd(b, nir_load_var(b, state->stack_ptr), intr->src[0].ssa));
      return true;
   }
   case nir_intrinsic_store_scratch: {
      nir_src_rewrite(&intr->src[1], nir_iadd(b, nir_load_var(b, state->stack_ptr), intr->src[1].ssa));
      return true;
   }
   case nir_intrinsic_load_ray_triangle_vertex_positions: {
      def = lvp_load_vertex_position(
         b, nir_load_var(b, state->instance_addr), nir_load_var(b, state->primitive_id),
         nir_intrinsic_column(intr));
      break;
   }
   /* Internal system values */
   case nir_intrinsic_load_shader_call_data_offset_lvp:
      def = nir_load_var(b, state->shader_call_data_offset);
      break;
   default:
      return false;
   }

   if (def)
      nir_def_rewrite_uses(&intr->def, def);
   nir_instr_remove(instr);

   return true;
}

static bool
lvp_lower_ray_tracing_stack_base(nir_builder *b, nir_intrinsic_instr *instr, void *data)
{
   if (instr->intrinsic != nir_intrinsic_load_ray_tracing_stack_base_lvp)
      return false;

   b->cursor = nir_after_instr(&instr->instr);

   nir_def_rewrite_uses(&instr->def, nir_imm_int(b, b->shader->scratch_size));
   nir_instr_remove(&instr->instr);

   return true;
}

static void
lvp_compile_ray_tracing_pipeline(struct lvp_pipeline *pipeline,
                                 const VkRayTracingPipelineCreateInfoKHR *create_info)
{
   nir_builder _b = nir_builder_init_simple_shader(
      MESA_SHADER_COMPUTE,
      pipeline->device->pscreen->get_compiler_options(pipeline->device->pscreen, PIPE_SHADER_IR_NIR, MESA_SHADER_COMPUTE),
      "ray tracing pipeline");
   nir_builder *b = &_b;

   b->shader->info.workgroup_size[0] = 8;

   struct lvp_ray_tracing_pipeline_compiler compiler = {
      .pipeline = pipeline,
      .flags = vk_rt_pipeline_create_flags(create_info),
   };
   lvp_ray_tracing_state_init(b->shader, &compiler.state);
   compiler.functions = _mesa_pointer_hash_table_create(NULL);

   nir_def *launch_id = nir_load_ray_launch_id(b);
   nir_def *launch_size = nir_load_ray_launch_size(b);
   nir_def *oob = nir_ige(b, nir_channel(b, launch_id, 0), nir_channel(b, launch_size, 0));
   oob = nir_ior(b, oob, nir_ige(b, nir_channel(b, launch_id, 1), nir_channel(b, launch_size, 1)));
   oob = nir_ior(b, oob, nir_ige(b, nir_channel(b, launch_id, 2), nir_channel(b, launch_size, 2)));

   nir_push_if(b, oob);
   nir_jump(b, nir_jump_return);
   nir_pop_if(b, NULL);

   nir_store_var(b, compiler.state.stack_ptr, nir_load_ray_tracing_stack_base_lvp(b), 0x1);

   struct lvp_sbt_entry raygen_entry = lvp_load_sbt_entry(
      b,
      NULL,
      offsetof(VkTraceRaysIndirectCommand2KHR, raygenShaderRecordAddress),
      offsetof(struct lvp_ray_tracing_group_handle, index));
   nir_store_var(b, compiler.state.shader_record_ptr, raygen_entry.shader_record_ptr, 0x1);

   for (uint32_t i = 0; i < pipeline->rt.group_count; i++) {
      struct lvp_ray_tracing_group *group = pipeline->rt.groups + i;
      if (group->recursive_index == VK_SHADER_UNUSED_KHR)
         continue;

      nir_shader *stage = pipeline->rt.stages[group->recursive_index]->nir;

      if (stage->info.stage != MESA_SHADER_RAYGEN)
         continue;

      nir_push_if(b, nir_ieq_imm(b, raygen_entry.value, group->handle.index));
      lvp_call_ray_tracing_stage(b, &compiler, stage);
      nir_pop_if(b, NULL);
   }

   nir_shader_instructions_pass(b->shader, lvp_lower_ray_tracing_instr, nir_metadata_none, &compiler);

   NIR_PASS(_, b->shader, nir_lower_returns);
   NIR_PASS(_, b->shader, nir_lower_compute_system_values, NULL);
   NIR_PASS(_, b->shader, nir_lower_global_vars_to_local);
   NIR_PASS(_, b->shader, nir_lower_vars_to_ssa);

   NIR_PASS(_, b->shader, nir_lower_vars_to_explicit_types,
            nir_var_shader_temp,
            glsl_get_natural_size_align_bytes);

   NIR_PASS(_, b->shader, nir_lower_explicit_io, nir_var_shader_temp,
            nir_address_format_32bit_offset);

   NIR_PASS(_, b->shader, nir_shader_intrinsics_pass, lvp_lower_ray_tracing_stack_base,
            nir_metadata_block_index | nir_metadata_dominance, NULL);

   /* We can not support dynamic stack sizes, assume the worst. */
   b->shader->scratch_size +=
      compiler.raygen_size +
      MIN2(create_info->maxPipelineRayRecursionDepth, 1) * MAX3(compiler.chit_size, compiler.miss_size, compiler.isec_size + compiler.ahit_size) +
      MAX2(0, (int)create_info->maxPipelineRayRecursionDepth - 1) * MAX2(compiler.chit_size, compiler.miss_size) + 31 * compiler.callable_size;

   struct lvp_shader *shader = &pipeline->shaders[MESA_SHADER_RAYGEN];
   lvp_shader_init(shader, b->shader);
   shader->shader_cso = lvp_shader_compile(pipeline->device, shader, nir_shader_clone(NULL, shader->pipeline_nir->nir), false);

   _mesa_hash_table_destroy(compiler.functions, NULL);
}

static VkResult
lvp_create_ray_tracing_pipeline(VkDevice _device, const VkAllocationCallbacks *allocator,
                                const VkRayTracingPipelineCreateInfoKHR *create_info,
                                VkPipeline *out_pipeline)
{
   VK_FROM_HANDLE(lvp_device, device, _device);
   VK_FROM_HANDLE(lvp_pipeline_layout, layout, create_info->layout);

   VkResult result = VK_SUCCESS;

   struct lvp_pipeline *pipeline = vk_zalloc2(&device->vk.alloc, allocator, sizeof(struct lvp_pipeline), 8,
                                              VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!pipeline)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   vk_object_base_init(&device->vk, &pipeline->base,
                       VK_OBJECT_TYPE_PIPELINE);

   vk_pipeline_layout_ref(&layout->vk);

   pipeline->device = device;
   pipeline->layout = layout;
   pipeline->type = LVP_PIPELINE_RAY_TRACING;

   pipeline->rt.stage_count = create_info->stageCount;
   pipeline->rt.group_count = create_info->groupCount;
   if (create_info->pLibraryInfo) {
      for (uint32_t i = 0; i < create_info->pLibraryInfo->libraryCount; i++) {
         VK_FROM_HANDLE(lvp_pipeline, library, create_info->pLibraryInfo->pLibraries[i]);
         pipeline->rt.stage_count += library->rt.stage_count;
         pipeline->rt.group_count += library->rt.group_count;
      }
   }

   pipeline->rt.stages = calloc(pipeline->rt.stage_count, sizeof(struct lvp_pipeline_nir *));
   pipeline->rt.groups = calloc(pipeline->rt.group_count, sizeof(struct lvp_ray_tracing_group));
   if (!pipeline->rt.stages || !pipeline->rt.groups) {
      result = VK_ERROR_OUT_OF_HOST_MEMORY;
      goto fail;
   }

   result = lvp_compile_ray_tracing_stages(pipeline, create_info);
   if (result != VK_SUCCESS)
      goto fail;

   lvp_init_ray_tracing_groups(pipeline, create_info);

   VkPipelineCreateFlags2KHR create_flags = vk_rt_pipeline_create_flags(create_info);
   if (!(create_flags & VK_PIPELINE_CREATE_2_LIBRARY_BIT_KHR)) {
      lvp_compile_ray_tracing_pipeline(pipeline, create_info);
   }

   *out_pipeline = lvp_pipeline_to_handle(pipeline);

   return VK_SUCCESS;

fail:
   lvp_pipeline_destroy(device, pipeline, false);
   return result;
}

VKAPI_ATTR VkResult VKAPI_CALL
lvp_CreateRayTracingPipelinesKHR(
   VkDevice device,
   VkDeferredOperationKHR deferredOperation,
   VkPipelineCache pipelineCache,
   uint32_t createInfoCount,
   const VkRayTracingPipelineCreateInfoKHR *pCreateInfos,
   const VkAllocationCallbacks *pAllocator,
   VkPipeline *pPipelines)
{
   VkResult result = VK_SUCCESS;

   uint32_t i = 0;
   for (; i < createInfoCount; i++) {
      VkResult tmp_result = lvp_create_ray_tracing_pipeline(
         device, pAllocator, pCreateInfos + i, pPipelines + i);

      if (tmp_result != VK_SUCCESS) {
         result = tmp_result;
         pPipelines[i] = VK_NULL_HANDLE;

         if (vk_rt_pipeline_create_flags(&pCreateInfos[i]) &
             VK_PIPELINE_CREATE_2_EARLY_RETURN_ON_FAILURE_BIT_KHR)
            break;
      }
   }

   for (; i < createInfoCount; i++)
      pPipelines[i] = VK_NULL_HANDLE;

   return result;
}


VKAPI_ATTR VkResult VKAPI_CALL
lvp_GetRayTracingShaderGroupHandlesKHR(
    VkDevice _device,
    VkPipeline _pipeline,
    uint32_t firstGroup,
    uint32_t groupCount,
    size_t dataSize,
    void *pData)
{
   VK_FROM_HANDLE(lvp_pipeline, pipeline, _pipeline);

   uint8_t *data = pData;
   memset(data, 0, dataSize);

   for (uint32_t i = 0; i < groupCount; i++) {
      memcpy(data + i * LVP_RAY_TRACING_GROUP_HANDLE_SIZE,
             pipeline->rt.groups + firstGroup + i,
             sizeof(struct lvp_ray_tracing_group_handle));
   }

   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
lvp_GetRayTracingCaptureReplayShaderGroupHandlesKHR(
   VkDevice device,
   VkPipeline pipeline,
   uint32_t firstGroup,
   uint32_t groupCount,
   size_t dataSize,
   void *pData)
{
   return VK_SUCCESS;
}

VKAPI_ATTR VkDeviceSize VKAPI_CALL
lvp_GetRayTracingShaderGroupStackSizeKHR(
   VkDevice device,
   VkPipeline pipeline,
   uint32_t group,
   VkShaderGroupShaderKHR groupShader)
{
   return 4;
}
