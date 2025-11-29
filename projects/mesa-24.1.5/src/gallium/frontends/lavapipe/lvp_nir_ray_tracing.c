/*
 * Copyright © 2021 Google
 * Copyright © 2023 Valve Corporation
 * SPDX-License-Identifier: MIT
 */

#include "lvp_nir_ray_tracing.h"
#include "lvp_acceleration_structure.h"
#include "lvp_private.h"

#include "compiler/spirv/spirv.h"

#include <float.h>
#include <math.h>

nir_def *
lvp_mul_vec3_mat(nir_builder *b, nir_def *vec, nir_def *matrix[], bool translation)
{
   nir_def *result_components[3] = {
      nir_channel(b, matrix[0], 3),
      nir_channel(b, matrix[1], 3),
      nir_channel(b, matrix[2], 3),
   };
   for (unsigned i = 0; i < 3; ++i) {
      for (unsigned j = 0; j < 3; ++j) {
         nir_def *v =
            nir_fmul(b, nir_channels(b, vec, 1 << j), nir_channels(b, matrix[i], 1 << j));
         result_components[i] = (translation || j) ? nir_fadd(b, result_components[i], v) : v;
      }
   }
   return nir_vec(b, result_components, 3);
}

void
lvp_load_wto_matrix(nir_builder *b, nir_def *instance_addr, nir_def **out)
{
   unsigned offset = offsetof(struct lvp_bvh_instance_node, wto_matrix);
   for (unsigned i = 0; i < 3; ++i) {
      out[i] = nir_build_load_global(b, 4, 32, nir_iadd_imm(b, instance_addr, offset + i * 16));
   }
}

nir_def *
lvp_load_vertex_position(nir_builder *b, nir_def *instance_addr, nir_def *primitive_id,
                         uint32_t index)
{
   nir_def *bvh_addr = nir_build_load_global(
      b, 1, 64, nir_iadd_imm(b, instance_addr, offsetof(struct lvp_bvh_instance_node, bvh_ptr)));

   nir_def *leaf_nodes_offset = nir_build_load_global(
      b, 1, 32, nir_iadd_imm(b, bvh_addr, offsetof(struct lvp_bvh_header, leaf_nodes_offset)));

   nir_def *offset = nir_imul_imm(b, primitive_id, sizeof(struct lvp_bvh_triangle_node));
   offset = nir_iadd(b, offset, leaf_nodes_offset);
   offset = nir_iadd_imm(b, offset, index * 3 * sizeof(float));

   return nir_build_load_global(b, 3, 32, nir_iadd(b, bvh_addr, nir_u2u64(b, offset)));
}

static nir_def *
lvp_build_intersect_ray_box(nir_builder *b, nir_def *node_addr, nir_def *ray_tmax,
                            nir_def *origin, nir_def *dir, nir_def *inv_dir)
{
   const struct glsl_type *vec2_type = glsl_vector_type(GLSL_TYPE_FLOAT, 2);
   const struct glsl_type *uvec2_type = glsl_vector_type(GLSL_TYPE_UINT, 2);

   nir_variable *distances =
      nir_variable_create(b->shader, nir_var_shader_temp, vec2_type, "distances");
   nir_store_var(b, distances, nir_imm_vec2(b, INFINITY, INFINITY), 0xf);

   nir_variable *child_indices =
      nir_variable_create(b->shader, nir_var_shader_temp, uvec2_type, "child_indices");
   nir_store_var(b, child_indices, nir_imm_ivec2(b, 0xffffffffu, 0xffffffffu), 0xf);

   inv_dir = nir_bcsel(b, nir_feq_imm(b, dir, 0), nir_imm_float(b, FLT_MAX), inv_dir);

   for (int i = 0; i < 2; i++) {
      const uint32_t child_offset = offsetof(struct lvp_bvh_box_node, children[i]);
      const uint32_t coord_offsets[2] = {
         offsetof(struct lvp_bvh_box_node, bounds[i].min.x),
         offsetof(struct lvp_bvh_box_node, bounds[i].max.x),
      };

      nir_def *child_index =
         nir_build_load_global(b, 1, 32, nir_iadd_imm(b, node_addr, child_offset));

      nir_def *node_coords[2] = {
         nir_build_load_global(b, 3, 32, nir_iadd_imm(b, node_addr, coord_offsets[0])),
         nir_build_load_global(b, 3, 32, nir_iadd_imm(b, node_addr, coord_offsets[1])),
      };

      /* If x of the aabb min is NaN, then this is an inactive aabb.
       * We don't need to care about any other components being NaN as that is UB.
       * https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/chap36.html#VkAabbPositionsKHR
       */
      nir_def *min_x = nir_channel(b, node_coords[0], 0);
      nir_def *min_x_is_not_nan =
         nir_inot(b, nir_fneu(b, min_x, min_x)); /* NaN != NaN -> true */

      nir_def *bound0 = nir_fmul(b, nir_fsub(b, node_coords[0], origin), inv_dir);
      nir_def *bound1 = nir_fmul(b, nir_fsub(b, node_coords[1], origin), inv_dir);

      nir_def *tmin =
         nir_fmax(b,
                  nir_fmax(b, nir_fmin(b, nir_channel(b, bound0, 0), nir_channel(b, bound1, 0)),
                           nir_fmin(b, nir_channel(b, bound0, 1), nir_channel(b, bound1, 1))),
                  nir_fmin(b, nir_channel(b, bound0, 2), nir_channel(b, bound1, 2)));

      nir_def *tmax =
         nir_fmin(b,
                  nir_fmin(b, nir_fmax(b, nir_channel(b, bound0, 0), nir_channel(b, bound1, 0)),
                           nir_fmax(b, nir_channel(b, bound0, 1), nir_channel(b, bound1, 1))),
                  nir_fmax(b, nir_channel(b, bound0, 2), nir_channel(b, bound1, 2)));

      nir_push_if(b,
                  nir_iand(b, min_x_is_not_nan,
                           nir_iand(b, nir_fge(b, tmax, nir_fmax(b, nir_imm_float(b, 0.0f), tmin)),
                                    nir_flt(b, tmin, ray_tmax))));
      {
         nir_def *new_child_indices[2] = {child_index, child_index};
         nir_store_var(b, child_indices, nir_vec(b, new_child_indices, 2), 1u << i);

         nir_def *new_distances[2] = {tmin, tmin};
         nir_store_var(b, distances, nir_vec(b, new_distances, 2), 1u << i);
      }
      nir_pop_if(b, NULL);
   }

   nir_def *ssa_distances = nir_load_var(b, distances);
   nir_def *ssa_indices = nir_load_var(b, child_indices);
   nir_push_if(b, nir_flt(b, nir_channel(b, ssa_distances, 1), nir_channel(b, ssa_distances, 0)));
   {
      nir_store_var(b, child_indices,
                    nir_vec2(b, nir_channel(b, ssa_indices, 0), nir_channel(b, ssa_indices, 1)),
                    0b11);
   }
   nir_pop_if(b, NULL);

   return nir_load_var(b, child_indices);
}

static nir_def *
lvp_build_intersect_ray_tri(nir_builder *b, nir_def *node_addr, nir_def *ray_tmax,
                            nir_def *origin, nir_def *dir, nir_def *inv_dir)
{
   const struct glsl_type *vec4_type = glsl_vector_type(GLSL_TYPE_FLOAT, 4);

   const uint32_t coord_offsets[3] = {
      offsetof(struct lvp_bvh_triangle_node, coords[0]),
      offsetof(struct lvp_bvh_triangle_node, coords[1]),
      offsetof(struct lvp_bvh_triangle_node, coords[2]),
   };

   nir_def *node_coords[3] = {
      nir_build_load_global(b, 3, 32, nir_iadd_imm(b, node_addr, coord_offsets[0])),
      nir_build_load_global(b, 3, 32, nir_iadd_imm(b, node_addr, coord_offsets[1])),
      nir_build_load_global(b, 3, 32, nir_iadd_imm(b, node_addr, coord_offsets[2])),
   };

   nir_variable *result = nir_variable_create(b->shader, nir_var_shader_temp, vec4_type, "result");
   nir_store_var(b, result, nir_imm_vec4(b, INFINITY, 1.0f, 0.0f, 0.0f), 0xf);

   /* Based on watertight Ray/Triangle intersection from
    * http://jcgt.org/published/0002/01/05/paper.pdf */

   /* Calculate the dimension where the ray direction is largest */
   nir_def *abs_dir = nir_fabs(b, dir);

   nir_def *abs_dirs[3] = {
      nir_channel(b, abs_dir, 0),
      nir_channel(b, abs_dir, 1),
      nir_channel(b, abs_dir, 2),
   };
   /* Find index of greatest value of abs_dir and put that as kz. */
   nir_def *kz = nir_bcsel(
      b, nir_fge(b, abs_dirs[0], abs_dirs[1]),
      nir_bcsel(b, nir_fge(b, abs_dirs[0], abs_dirs[2]), nir_imm_int(b, 0), nir_imm_int(b, 2)),
      nir_bcsel(b, nir_fge(b, abs_dirs[1], abs_dirs[2]), nir_imm_int(b, 1), nir_imm_int(b, 2)));
   nir_def *kx = nir_imod_imm(b, nir_iadd_imm(b, kz, 1), 3);
   nir_def *ky = nir_imod_imm(b, nir_iadd_imm(b, kx, 1), 3);
   nir_def *k_indices[3] = {kx, ky, kz};
   nir_def *k = nir_vec(b, k_indices, 3);

   /* Swap kx and ky dimensions to preserve winding order */
   unsigned swap_xy_swizzle[4] = {1, 0, 2, 3};
   k = nir_bcsel(b, nir_flt_imm(b, nir_vector_extract(b, dir, kz), 0.0f),
                 nir_swizzle(b, k, swap_xy_swizzle, 3), k);

   kx = nir_channel(b, k, 0);
   ky = nir_channel(b, k, 1);
   kz = nir_channel(b, k, 2);

   /* Calculate shear constants */
   nir_def *sz = nir_frcp(b, nir_vector_extract(b, dir, kz));
   nir_def *sx = nir_fmul(b, nir_vector_extract(b, dir, kx), sz);
   nir_def *sy = nir_fmul(b, nir_vector_extract(b, dir, ky), sz);

   /* Calculate vertices relative to ray origin */
   nir_def *v_a = nir_fsub(b, node_coords[0], origin);
   nir_def *v_b = nir_fsub(b, node_coords[1], origin);
   nir_def *v_c = nir_fsub(b, node_coords[2], origin);

   /* Perform shear and scale */
   nir_def *ax =
      nir_fsub(b, nir_vector_extract(b, v_a, kx), nir_fmul(b, sx, nir_vector_extract(b, v_a, kz)));
   nir_def *ay =
      nir_fsub(b, nir_vector_extract(b, v_a, ky), nir_fmul(b, sy, nir_vector_extract(b, v_a, kz)));
   nir_def *bx =
      nir_fsub(b, nir_vector_extract(b, v_b, kx), nir_fmul(b, sx, nir_vector_extract(b, v_b, kz)));
   nir_def *by =
      nir_fsub(b, nir_vector_extract(b, v_b, ky), nir_fmul(b, sy, nir_vector_extract(b, v_b, kz)));
   nir_def *cx =
      nir_fsub(b, nir_vector_extract(b, v_c, kx), nir_fmul(b, sx, nir_vector_extract(b, v_c, kz)));
   nir_def *cy =
      nir_fsub(b, nir_vector_extract(b, v_c, ky), nir_fmul(b, sy, nir_vector_extract(b, v_c, kz)));

   ax = nir_f2f64(b, ax);
   ay = nir_f2f64(b, ay);
   bx = nir_f2f64(b, bx);
   by = nir_f2f64(b, by);
   cx = nir_f2f64(b, cx);
   cy = nir_f2f64(b, cy);

   nir_def *u = nir_fsub(b, nir_fmul(b, cx, by), nir_fmul(b, cy, bx));
   nir_def *v = nir_fsub(b, nir_fmul(b, ax, cy), nir_fmul(b, ay, cx));
   nir_def *w = nir_fsub(b, nir_fmul(b, bx, ay), nir_fmul(b, by, ax));

   /* Perform edge tests. */
   nir_def *cond_back = nir_ior(b, nir_ior(b, nir_flt_imm(b, u, 0.0f), nir_flt_imm(b, v, 0.0f)),
                                    nir_flt_imm(b, w, 0.0f));

   nir_def *cond_front = nir_ior(
      b, nir_ior(b, nir_fgt_imm(b, u, 0.0f), nir_fgt_imm(b, v, 0.0f)), nir_fgt_imm(b, w, 0.0f));

   nir_def *cond = nir_inot(b, nir_iand(b, cond_back, cond_front));

   nir_push_if(b, cond);
   {
      nir_def *det = nir_fadd(b, u, nir_fadd(b, v, w));

      sz = nir_f2f64(b, sz);

      v_a = nir_f2f64(b, v_a);
      v_b = nir_f2f64(b, v_b);
      v_c = nir_f2f64(b, v_c);

      nir_def *az = nir_fmul(b, sz, nir_vector_extract(b, v_a, kz));
      nir_def *bz = nir_fmul(b, sz, nir_vector_extract(b, v_b, kz));
      nir_def *cz = nir_fmul(b, sz, nir_vector_extract(b, v_c, kz));

      nir_def *t =
         nir_fadd(b, nir_fadd(b, nir_fmul(b, u, az), nir_fmul(b, v, bz)), nir_fmul(b, w, cz));

      nir_def *t_signed = nir_fmul(b, nir_fsign(b, det), t);

      nir_def *det_cond_front = nir_inot(b, nir_flt_imm(b, t_signed, 0.0f));

      nir_push_if(b, det_cond_front);
      {
         t = nir_f2f32(b, nir_fdiv(b, t, det));
         det = nir_f2f32(b, det);
         v = nir_fdiv(b, nir_f2f32(b, v), det);
         w = nir_fdiv(b, nir_f2f32(b, w), det);

         nir_def *indices[4] = {t, det, v, w};
         nir_store_var(b, result, nir_vec(b, indices, 4), 0xf);
      }
      nir_pop_if(b, NULL);
   }
   nir_pop_if(b, NULL);

   return nir_load_var(b, result);
}

static nir_def *
lvp_build_hit_is_opaque(nir_builder *b, nir_def *sbt_offset_and_flags,
                        const struct lvp_ray_flags *ray_flags, nir_def *geometry_id_and_flags)
{
   nir_def *opaque = nir_uge_imm(b, nir_ior(b, geometry_id_and_flags, sbt_offset_and_flags),
                                     LVP_INSTANCE_FORCE_OPAQUE | LVP_INSTANCE_NO_FORCE_NOT_OPAQUE);
   opaque = nir_bcsel(b, ray_flags->force_opaque, nir_imm_true(b), opaque);
   opaque = nir_bcsel(b, ray_flags->force_not_opaque, nir_imm_false(b), opaque);
   return opaque;
}

static void
lvp_build_triangle_case(nir_builder *b, const struct lvp_ray_traversal_args *args,
                        const struct lvp_ray_flags *ray_flags, nir_def *result,
                        nir_def *node_addr)
{
   if (!args->triangle_cb)
      return;

   struct lvp_triangle_intersection intersection;
   intersection.t = nir_channel(b, result, 0);
   intersection.barycentrics = nir_channels(b, result, 0xc);

   nir_push_if(b, nir_flt(b, intersection.t, nir_load_deref(b, args->vars.tmax)));
   {
      intersection.frontface = nir_fgt_imm(b, nir_channel(b, result, 1), 0);
      nir_def *switch_ccw = nir_test_mask(b, nir_load_deref(b, args->vars.sbt_offset_and_flags),
                                              LVP_INSTANCE_TRIANGLE_FLIP_FACING);
      intersection.frontface = nir_ixor(b, intersection.frontface, switch_ccw);

      nir_def *not_cull = ray_flags->no_skip_triangles;
      nir_def *not_facing_cull =
         nir_bcsel(b, intersection.frontface, ray_flags->no_cull_front, ray_flags->no_cull_back);

      not_cull =
         nir_iand(b, not_cull,
                  nir_ior(b, not_facing_cull,
                          nir_test_mask(b, nir_load_deref(b, args->vars.sbt_offset_and_flags),
                                        LVP_INSTANCE_TRIANGLE_FACING_CULL_DISABLE)));

      nir_push_if(b, nir_iand(b, nir_flt(b, args->tmin, intersection.t), not_cull));
      {
         intersection.base.node_addr = node_addr;
         nir_def *triangle_info = nir_build_load_global(
            b, 2, 32,
            nir_iadd_imm(b, intersection.base.node_addr,
                         offsetof(struct lvp_bvh_triangle_node, primitive_id)));
         intersection.base.primitive_id = nir_channel(b, triangle_info, 0);
         intersection.base.geometry_id_and_flags = nir_channel(b, triangle_info, 1);
         intersection.base.opaque =
            lvp_build_hit_is_opaque(b, nir_load_deref(b, args->vars.sbt_offset_and_flags), ray_flags,
                                    intersection.base.geometry_id_and_flags);

         not_cull = nir_bcsel(b, intersection.base.opaque, ray_flags->no_cull_opaque,
                              ray_flags->no_cull_no_opaque);
         nir_push_if(b, not_cull);
         {
            args->triangle_cb(b, &intersection, args, ray_flags);
         }
         nir_pop_if(b, NULL);
      }
      nir_pop_if(b, NULL);
   }
   nir_pop_if(b, NULL);
}

static void
lvp_build_aabb_case(nir_builder *b, const struct lvp_ray_traversal_args *args,
                           const struct lvp_ray_flags *ray_flags, nir_def *node_addr)
{
   if (!args->aabb_cb)
      return;

   struct lvp_leaf_intersection intersection;
   intersection.node_addr = node_addr;
   nir_def *triangle_info = nir_build_load_global(
      b, 2, 32,
      nir_iadd_imm(b, intersection.node_addr, offsetof(struct lvp_bvh_aabb_node, primitive_id)));
   intersection.primitive_id = nir_channel(b, triangle_info, 0);
   intersection.geometry_id_and_flags = nir_channel(b, triangle_info, 1);
   intersection.opaque = lvp_build_hit_is_opaque(b, nir_load_deref(b, args->vars.sbt_offset_and_flags),
                                                 ray_flags, intersection.geometry_id_and_flags);

   nir_def *not_cull =
      nir_bcsel(b, intersection.opaque, ray_flags->no_cull_opaque, ray_flags->no_cull_no_opaque);
   not_cull = nir_iand(b, not_cull, ray_flags->no_skip_aabbs);
   nir_push_if(b, not_cull);
   {
      args->aabb_cb(b, &intersection, args, ray_flags);
   }
   nir_pop_if(b, NULL);
}

static void
lvp_build_push_stack(nir_builder *b, const struct lvp_ray_traversal_args *args, nir_def *node)
{
   nir_def *stack_ptr = nir_load_deref(b, args->vars.stack_ptr);
   nir_store_deref(b, nir_build_deref_array(b, args->vars.stack, stack_ptr), node, 0x1);
   nir_store_deref(b, args->vars.stack_ptr, nir_iadd_imm(b, nir_load_deref(b, args->vars.stack_ptr), 1), 0x1);
}

static nir_def *
lvp_build_pop_stack(nir_builder *b, const struct lvp_ray_traversal_args *args)
{
   nir_def *stack_ptr = nir_iadd_imm(b, nir_load_deref(b, args->vars.stack_ptr), -1);
   nir_store_deref(b, args->vars.stack_ptr, stack_ptr, 0x1);
   return nir_load_deref(b, nir_build_deref_array(b, args->vars.stack, stack_ptr));
}

nir_def *
lvp_build_ray_traversal(nir_builder *b, const struct lvp_ray_traversal_args *args)
{
   nir_variable *incomplete = nir_local_variable_create(b->impl, glsl_bool_type(), "incomplete");
   nir_store_var(b, incomplete, nir_imm_true(b), 0x1);

   nir_def *vec3ones = nir_imm_vec3(b, 1.0, 1.0, 1.0);

   struct lvp_ray_flags ray_flags = {
      .force_opaque = nir_test_mask(b, args->flags, SpvRayFlagsOpaqueKHRMask),
      .force_not_opaque = nir_test_mask(b, args->flags, SpvRayFlagsNoOpaqueKHRMask),
      .terminate_on_first_hit =
         nir_test_mask(b, args->flags, SpvRayFlagsTerminateOnFirstHitKHRMask),
      .no_cull_front = nir_ieq_imm(
         b, nir_iand_imm(b, args->flags, SpvRayFlagsCullFrontFacingTrianglesKHRMask), 0),
      .no_cull_back =
         nir_ieq_imm(b, nir_iand_imm(b, args->flags, SpvRayFlagsCullBackFacingTrianglesKHRMask), 0),
      .no_cull_opaque =
         nir_ieq_imm(b, nir_iand_imm(b, args->flags, SpvRayFlagsCullOpaqueKHRMask), 0),
      .no_cull_no_opaque =
         nir_ieq_imm(b, nir_iand_imm(b, args->flags, SpvRayFlagsCullNoOpaqueKHRMask), 0),
      .no_skip_triangles =
         nir_ieq_imm(b, nir_iand_imm(b, args->flags, SpvRayFlagsSkipTrianglesKHRMask), 0),
      .no_skip_aabbs = nir_ieq_imm(b, nir_iand_imm(b, args->flags, SpvRayFlagsSkipAABBsKHRMask), 0),
   };

   nir_push_loop(b);
   {
      nir_push_if(b, nir_ieq_imm(b, nir_load_deref(b, args->vars.current_node), LVP_BVH_INVALID_NODE));
      {
         nir_push_if(b, nir_ieq_imm(b, nir_load_deref(b, args->vars.stack_ptr), 0));
         {
            nir_store_var(b, incomplete, nir_imm_false(b), 0x1);
            nir_jump(b, nir_jump_break);
         }
         nir_pop_if(b, NULL);

         nir_push_if(b, nir_ige(b, nir_load_deref(b, args->vars.stack_base), nir_load_deref(b, args->vars.stack_ptr)));
         {
            nir_store_deref(b, args->vars.stack_base, nir_imm_int(b, -1), 1);

            nir_store_deref(b, args->vars.bvh_base, args->root_bvh_base, 1);
            nir_store_deref(b, args->vars.origin, args->origin, 7);
            nir_store_deref(b, args->vars.dir, args->dir, 7);
            nir_store_deref(b, args->vars.inv_dir, nir_fdiv(b, vec3ones, args->dir), 7);
         }
         nir_pop_if(b, NULL);

         nir_store_deref(b, args->vars.current_node, lvp_build_pop_stack(b, args), 0x1);
      }
      nir_pop_if(b, NULL);

      nir_def *bvh_node = nir_load_deref(b, args->vars.current_node);
      nir_store_deref(b, args->vars.current_node, nir_imm_int(b, LVP_BVH_INVALID_NODE), 0x1);

      nir_def *node_addr = nir_iadd(b, nir_load_deref(b, args->vars.bvh_base), nir_u2u64(b, nir_iand_imm(b, bvh_node, ~3u)));

      nir_def *node_type = nir_iand_imm(b, bvh_node, 3);
      nir_push_if(b, nir_uge_imm(b, node_type, lvp_bvh_node_internal));
      {
         nir_push_if(b, nir_uge_imm(b, node_type, lvp_bvh_node_instance));
         {
            nir_push_if(b, nir_ieq_imm(b, node_type, lvp_bvh_node_aabb));
            {
               lvp_build_aabb_case(b, args, &ray_flags, node_addr);
            }
            nir_push_else(b, NULL);
            {
               /* instance */
               nir_store_deref(b, args->vars.instance_addr, node_addr, 1);

               nir_def *instance_data = nir_build_load_global(
                  b, 4, 32,
                  nir_iadd_imm(b, node_addr, offsetof(struct lvp_bvh_instance_node, bvh_ptr)));

               nir_def *wto_matrix[3];
               lvp_load_wto_matrix(b, node_addr, wto_matrix);

               nir_store_deref(b, args->vars.sbt_offset_and_flags, nir_channel(b, instance_data, 3),
                               1);

               nir_def *instance_and_mask = nir_channel(b, instance_data, 2);
               nir_push_if(b, nir_ult(b, nir_iand(b, instance_and_mask, args->cull_mask),
                                      nir_imm_int(b, 1 << 24)));
               {
                  nir_jump(b, nir_jump_continue);
               }
               nir_pop_if(b, NULL);

               nir_store_deref(b, args->vars.bvh_base,
                               nir_pack_64_2x32(b, nir_trim_vector(b, instance_data, 2)), 1);

               nir_store_deref(b, args->vars.stack_base, nir_load_deref(b, args->vars.stack_ptr), 0x1);

               /* Push the instance root node onto the stack */
               nir_store_deref(b, args->vars.current_node, nir_imm_int(b, LVP_BVH_ROOT_NODE), 0x1);

               /* Transform the ray into object space */
               nir_store_deref(b, args->vars.origin,
                               lvp_mul_vec3_mat(b, args->origin, wto_matrix, true), 7);
               nir_store_deref(b, args->vars.dir,
                               lvp_mul_vec3_mat(b, args->dir, wto_matrix, false), 7);
               nir_store_deref(b, args->vars.inv_dir,
                               nir_fdiv(b, vec3ones, nir_load_deref(b, args->vars.dir)), 7);
            }
            nir_pop_if(b, NULL);
         }
         nir_push_else(b, NULL);
         {
            nir_def *result = lvp_build_intersect_ray_box(
               b, node_addr, nir_load_deref(b, args->vars.tmax),
               nir_load_deref(b, args->vars.origin), nir_load_deref(b, args->vars.dir),
               nir_load_deref(b, args->vars.inv_dir));

            nir_store_deref(b, args->vars.current_node, nir_channel(b, result, 0), 0x1);

            nir_push_if(b, nir_ine_imm(b, nir_channel(b, result, 1), LVP_BVH_INVALID_NODE));
            {
               lvp_build_push_stack(b, args, nir_channel(b, result, 1));
            }
            nir_pop_if(b, NULL);
         }
         nir_pop_if(b, NULL);
      }
      nir_push_else(b, NULL);
      {
         nir_def *result = lvp_build_intersect_ray_tri(
            b, node_addr, nir_load_deref(b, args->vars.tmax), nir_load_deref(b, args->vars.origin),
            nir_load_deref(b, args->vars.dir), nir_load_deref(b, args->vars.inv_dir));

         lvp_build_triangle_case(b, args, &ray_flags, result, node_addr);
      }
      nir_pop_if(b, NULL);
   }
   nir_pop_loop(b, NULL);

   return nir_load_var(b, incomplete);
}
