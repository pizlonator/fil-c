/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_RRA_H
#define RADV_RRA_H

#include "util/simple_mtx.h"
#include "util/u_dynarray.h"

#include <vulkan/vulkan.h>

#include <assert.h>
#include <stdbool.h>

struct radv_device;

struct radv_rra_accel_struct_data {
   VkEvent build_event;
   uint64_t va;
   uint64_t size;
   VkBuffer buffer;
   VkDeviceMemory memory;
   VkAccelerationStructureTypeKHR type;
   bool is_dead;
};

enum radv_rra_ray_history_metadata_type {
   RADV_RRA_COUNTER_INFO = 1,
   RADV_RRA_DISPATCH_SIZE = 2,
   RADV_RRA_TRAVERSAL_FLAGS = 3,
};

struct radv_rra_ray_history_metadata_info {
   enum radv_rra_ray_history_metadata_type type : 32;
   uint32_t padding;
   uint64_t size;
};

enum radv_rra_pipeline_type {
   RADV_RRA_PIPELINE_RAY_TRACING,
};

struct radv_rra_ray_history_counter {
   uint32_t dispatch_size[3];
   uint32_t hit_shader_count;
   uint32_t miss_shader_count;
   uint32_t shader_count;
   uint64_t pipeline_api_hash;
   uint32_t mode;
   uint32_t mask;
   uint32_t stride;
   uint32_t data_size;
   uint32_t lost_token_size;
   uint32_t ray_id_begin;
   uint32_t ray_id_end;
   enum radv_rra_pipeline_type pipeline_type : 32;
};

struct radv_rra_ray_history_dispatch_size {
   uint32_t size[3];
   uint32_t padding;
};

struct radv_rra_ray_history_traversal_flags {
   uint32_t box_sort_mode : 1;
   uint32_t node_ptr_flags : 1;
   uint32_t reserved : 30;
   uint32_t padding;
};

struct radv_rra_ray_history_metadata {
   struct radv_rra_ray_history_metadata_info counter_info;
   struct radv_rra_ray_history_counter counter;

   struct radv_rra_ray_history_metadata_info dispatch_size_info;
   struct radv_rra_ray_history_dispatch_size dispatch_size;

   struct radv_rra_ray_history_metadata_info traversal_flags_info;
   struct radv_rra_ray_history_traversal_flags traversal_flags;
};
static_assert(sizeof(struct radv_rra_ray_history_metadata) == 136,
              "radv_rra_ray_history_metadata does not match RRA expectations");

struct radv_rra_ray_history_data {
   struct radv_rra_ray_history_metadata metadata;
};

struct radv_rra_trace_data {
   struct hash_table *accel_structs;
   struct hash_table_u64 *accel_struct_vas;
   simple_mtx_t data_mtx;
   bool validate_as;
   bool copy_after_build;
   bool triggered;
   uint32_t copy_memory_index;

   struct util_dynarray ray_history;
   VkBuffer ray_history_buffer;
   VkDeviceMemory ray_history_memory;
   void *ray_history_data;
   uint64_t ray_history_addr;
   uint32_t ray_history_buffer_size;
   uint32_t ray_history_resolution_scale;
};

struct radv_ray_history_header {
   uint32_t offset;
   uint32_t dispatch_index;
   uint32_t submit_base_index;
};

enum radv_packed_token_type {
   radv_packed_token_end_trace,
};

struct radv_packed_token_header {
   uint32_t launch_index : 29;
   uint32_t hit : 1;
   uint32_t token_type : 2;
};

struct radv_packed_end_trace_token {
   struct radv_packed_token_header header;

   uint32_t accel_struct_lo;
   uint32_t accel_struct_hi;

   uint32_t flags : 16;
   uint32_t dispatch_index : 16;

   uint32_t sbt_offset : 4;
   uint32_t sbt_stride : 4;
   uint32_t miss_index : 16;
   uint32_t cull_mask : 8;

   float origin[3];
   float tmin;
   float direction[3];
   float tmax;

   uint32_t iteration_count : 16;
   uint32_t instance_count : 16;

   uint32_t ahit_count : 16;
   uint32_t isec_count : 16;

   uint32_t primitive_id;
   uint32_t geometry_id;

   uint32_t instance_id : 24;
   uint32_t hit_kind : 8;

   float t;
};
static_assert(sizeof(struct radv_packed_end_trace_token) == 76, "Unexpected radv_packed_end_trace_token size");

VkResult radv_rra_trace_init(struct radv_device *device);

void radv_rra_trace_clear_ray_history(VkDevice _device, struct radv_rra_trace_data *data);

void radv_rra_trace_finish(VkDevice vk_device, struct radv_rra_trace_data *data);

void radv_destroy_rra_accel_struct_data(VkDevice device, struct radv_rra_accel_struct_data *data);

VkResult radv_rra_dump_trace(VkQueue vk_queue, char *filename);

#endif /* RADV_RRA_H */
