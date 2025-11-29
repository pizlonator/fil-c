/*
 * Copyright © 2021 Google
 * Copyright © 2023 Valve Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef LVP_NIR_RAY_TRACING_H
#define LVP_NIR_RAY_TRACING_H

#include "nir/nir.h"
#include "nir/nir_builder.h"

nir_def *lvp_mul_vec3_mat(nir_builder *b, nir_def *vec, nir_def *matrix[], bool translation);

void lvp_load_wto_matrix(nir_builder *b, nir_def *instance_addr, nir_def **out);

nir_def *lvp_load_vertex_position(nir_builder *b, nir_def *instance_addr,
                                  nir_def *primitive_id, uint32_t index);

struct lvp_ray_traversal_args;

struct lvp_ray_flags {
   nir_def *force_opaque;
   nir_def *force_not_opaque;
   nir_def *terminate_on_first_hit;
   nir_def *no_cull_front;
   nir_def *no_cull_back;
   nir_def *no_cull_opaque;
   nir_def *no_cull_no_opaque;
   nir_def *no_skip_triangles;
   nir_def *no_skip_aabbs;
};

struct lvp_leaf_intersection {
   nir_def *node_addr;
   nir_def *primitive_id;
   nir_def *geometry_id_and_flags;
   nir_def *opaque;
};

typedef void (*lvp_aabb_intersection_cb)(nir_builder *b, struct lvp_leaf_intersection *intersection,
                                         const struct lvp_ray_traversal_args *args,
                                         const struct lvp_ray_flags *ray_flags);

struct lvp_triangle_intersection {
   struct lvp_leaf_intersection base;

   nir_def *t;
   nir_def *frontface;
   nir_def *barycentrics;
};

typedef void (*lvp_triangle_intersection_cb)(nir_builder *b,
                                             struct lvp_triangle_intersection *intersection,
                                             const struct lvp_ray_traversal_args *args,
                                             const struct lvp_ray_flags *ray_flags);

struct lvp_ray_traversal_vars {
   /* For each accepted hit, tmax will be set to the t value. This allows for automatic intersection
    * culling.
    */
   nir_deref_instr *tmax;

   /* Those variables change when entering and exiting BLASes. */
   nir_deref_instr *origin;
   nir_deref_instr *dir;
   nir_deref_instr *inv_dir;

   /* The base address of the current TLAS/BLAS. */
   nir_deref_instr *bvh_base;

   nir_deref_instr *current_node;

   nir_deref_instr *stack_base;
   nir_deref_instr *stack_ptr;
   nir_deref_instr *stack;

   /* Information about the current instance used for culling. */
   nir_deref_instr *instance_addr;
   nir_deref_instr *sbt_offset_and_flags;
};

struct lvp_ray_traversal_args {
   nir_def *root_bvh_base;
   nir_def *flags;
   nir_def *cull_mask;
   nir_def *origin;
   nir_def *tmin;
   nir_def *dir;

   struct lvp_ray_traversal_vars vars;

   lvp_aabb_intersection_cb aabb_cb;
   lvp_triangle_intersection_cb triangle_cb;

   void *data;
};

/* Builds the ray traversal loop and returns whether traversal is incomplete, similar to
 * rayQueryProceedEXT. Traversal will only be considered incomplete, if one of the specified
 * callbacks breaks out of the traversal loop.
 */
nir_def *lvp_build_ray_traversal(nir_builder *b, const struct lvp_ray_traversal_args *args);

#endif
