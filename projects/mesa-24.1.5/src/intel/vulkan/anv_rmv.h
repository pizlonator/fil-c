/*
 * Copyright © 2024 Intel Corporation
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

#ifndef ANV_RMV_H
#define ANV_RMV_H

#include <stdbool.h>
#include <stdint.h>

#include "vulkan/vulkan_core.h"

struct anv_device;
struct anv_device_memory;
struct anv_physical_device;
struct anv_descriptor_pool;
struct anv_buffer;
struct anv_image;
struct anv_bo;
struct anv_event;
struct anv_graphics_pipeline;
struct anv_compute_pipeline;
struct anv_ray_tracing_pipeline;

enum anv_image_memory_binding;

#define ANV_RMV(func, device, ...) do { \
      if (unlikely((device)->vk.memory_trace_data.is_enabled)) \
         anv_rmv_log_##func(device, __VA_ARGS__); \
   } while (0)

void anv_memory_trace_init(struct anv_device *device);
void anv_rmv_fill_device_info(const struct anv_physical_device *device,
                              struct vk_rmv_device_info *info);
void anv_memory_trace_finish(struct anv_device *device);

void anv_rmv_log_heap_create(struct anv_device *device,
                             struct anv_device_memory *memory,
                             bool is_internal,
                             VkMemoryAllocateFlags alloc_flags);
void anv_rmv_log_bo_gtt_map(struct anv_device *device,
                            struct anv_bo *bo);
void anv_rmv_log_bo_gtt_unmap(struct anv_device *device,
                              struct anv_bo *bo);
void anv_rmv_log_bos_gtt_map(struct anv_device *device,
                             struct anv_bo **bos,
                             uint32_t bo_count);
void anv_rmv_log_vm_binds(struct anv_device *device,
                          struct anv_vm_bind *binds,
                          uint32_t bind_count);
void anv_rmv_log_bo_allocate(struct anv_device *device,
                             struct anv_bo *bo);
void anv_rmv_log_bo_destroy(struct anv_device *device, struct anv_bo *bo);
void anv_rmv_log_buffer_create(struct anv_device *device,
                               bool is_internal,
                               struct anv_buffer *buffer);
void anv_rmv_log_buffer_destroy(struct anv_device *device,
                                struct anv_buffer *buffer);
void anv_rmv_log_buffer_bind(struct anv_device *device, struct anv_buffer *buffer);
void anv_rmv_log_image_create(struct anv_device *device,
                              bool is_internal,
                              struct anv_image *image);
void anv_rmv_log_image_destroy(struct anv_device *device,
                               struct anv_image *image);
void anv_rmv_log_image_bind(struct anv_device *device,
                            struct anv_image *image,
                            enum anv_image_memory_binding binding);
void anv_rmv_log_query_pool_create(struct anv_device *device,
                                   struct anv_query_pool *pool,
                                   bool is_internal);
void anv_rmv_log_cmd_buffer_create(struct anv_device *device,
                                   struct anv_cmd_buffer *cmd_buffer);
void anv_rmv_log_cmd_buffer_destroy(struct anv_device *device,
                                    struct anv_cmd_buffer *cmd_buffer);
void anv_rmv_log_sparse_add_residency(struct anv_device *device,
                                      struct anv_bo *src_bo,
                                      uint64_t offset);
void anv_rmv_log_sparse_remove_residency(struct anv_device *device,
                                         struct anv_bo *src_bo,
                                         uint64_t offset);
void anv_rmv_log_descriptor_pool_create(struct anv_device *device,
                                        const VkDescriptorPoolCreateInfo *create_info,
                                        struct anv_descriptor_pool *pool,
                                        bool is_internal);
void anv_rmv_log_graphics_pipeline_create(struct anv_device *device,
                                          struct anv_graphics_pipeline *pipeline,
                                          bool is_internal);
void anv_rmv_log_compute_pipeline_create(struct anv_device *device,
                                         struct anv_compute_pipeline *pipeline,
                                         bool is_internal);
void anv_rmv_log_rt_pipeline_create(struct anv_device *device,
                                    struct anv_ray_tracing_pipeline *pipeline,
                                    bool is_internal);
void anv_rmv_log_event_create(struct anv_device *device,
                              struct anv_event *event,
                              VkEventCreateFlags flags, bool is_internal);
void anv_rmv_log_resource_destroy(struct anv_device *device, const void *obj);

#endif /* ANV_RMV_H */
