
/*
 * Copyright © 2021 Google
 * Copyright © 2023 Valve Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef LVP_ACCELERATION_STRUCTURE_H
#define LVP_ACCELERATION_STRUCTURE_H

#include "lvp_private.h"

#define LVP_GEOMETRY_OPAQUE (1u << 31)

#define LVP_INSTANCE_FORCE_OPAQUE                 (1u << 31)
#define LVP_INSTANCE_NO_FORCE_NOT_OPAQUE          (1u << 30)
#define LVP_INSTANCE_TRIANGLE_FACING_CULL_DISABLE (1u << 29)
#define LVP_INSTANCE_TRIANGLE_FLIP_FACING         (1u << 28)

#define lvp_bvh_node_triangle 0
#define lvp_bvh_node_internal 1
#define lvp_bvh_node_instance 2
#define lvp_bvh_node_aabb     3

typedef struct {
   float values[3][4];
} lvp_mat3x4;

typedef struct {
   float x;
   float y;
   float z;
} lvp_vec3;

typedef struct lvp_aabb {
   lvp_vec3 min;
   lvp_vec3 max;
} lvp_aabb;

struct lvp_bvh_triangle_node {
   float coords[3][3];

   uint32_t padding;

   uint32_t primitive_id;
   /* flags in upper 4 bits */
   uint32_t geometry_id_and_flags;
};

struct lvp_bvh_aabb_node {
   lvp_aabb bounds;

   uint32_t primitive_id;
   /* flags in upper 4 bits */
   uint32_t geometry_id_and_flags;
};

struct lvp_bvh_instance_node {
   uint64_t bvh_ptr;

   /* lower 24 bits are the custom instance index, upper 8 bits are the visibility mask */
   uint32_t custom_instance_and_mask;
   /* lower 24 bits are the sbt offset, upper 8 bits are VkGeometryInstanceFlagsKHR */
   uint32_t sbt_offset_and_flags;

   lvp_mat3x4 wto_matrix;
   uint32_t padding;

   uint32_t instance_id;

   /* Object to world matrix transposed from the initial transform. */
   lvp_mat3x4 otw_matrix;
};

struct lvp_bvh_box_node {
   lvp_aabb bounds[2];
   uint32_t children[2];
};

struct lvp_bvh_header {
   lvp_aabb bounds;

   uint32_t serialization_size;
   uint32_t instance_count;
   uint32_t leaf_nodes_offset;

   uint32_t padding;
};

struct lvp_accel_struct_serialization_header {
   uint8_t driver_uuid[VK_UUID_SIZE];
   uint8_t accel_struct_compat[VK_UUID_SIZE];
   uint64_t serialization_size;
   uint64_t compacted_size;
   uint64_t instance_count;
   uint64_t instances[];
};

/* The root node is the first node after the header. */
#define LVP_BVH_ROOT_NODE_OFFSET (sizeof(struct lvp_bvh_header))
#define LVP_BVH_ROOT_NODE        (LVP_BVH_ROOT_NODE_OFFSET | lvp_bvh_node_internal)
#define LVP_BVH_INVALID_NODE     0xFFFFFFFF

void
lvp_build_acceleration_structure(VkAccelerationStructureBuildGeometryInfoKHR *info,
                                 const VkAccelerationStructureBuildRangeInfoKHR *ranges);

#endif
