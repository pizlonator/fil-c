/*
 * Copyright © 2024 Igalia S.L.
 * SPDX-License-Identifier: MIT
 */

#ifndef TU_RMV_H
#define TU_RMV_H

#include "tu_common.h"

#include "rmv/vk_rmv_common.h"

#define TU_RMV(func, device, ...) do { \
      if (unlikely((device)->vk.memory_trace_data.is_enabled)) \
         tu_rmv_log_##func(device, __VA_ARGS__); \
   } while(0)

void
tu_memory_trace_init(struct tu_device *device);

void
tu_memory_trace_finish(struct tu_device *device);

void
tu_rmv_log_heap_create(struct tu_device *device,
                       const VkMemoryAllocateInfo *allocate_info,
                       struct tu_device_memory *device_memory);

void
tu_rmv_log_bo_allocate(struct tu_device *device, struct tu_bo *bo);
void
tu_rmv_log_bo_destroy(struct tu_device *device, struct tu_bo *bo);
void
tu_rmv_log_bo_map(struct tu_device *device, struct tu_bo *bo);
void
tu_rmv_log_bo_unmap(struct tu_device *device, struct tu_bo *bo);

void
tu_rmv_log_buffer_create(struct tu_device *device, struct tu_buffer *buffer);
void
tu_rmv_log_buffer_destroy(struct tu_device *device, struct tu_buffer *buffer);
void
tu_rmv_log_buffer_bind(struct tu_device *device, struct tu_buffer *buffer);

void
tu_rmv_log_image_create(struct tu_device *device, struct tu_image *image);
void
tu_rmv_log_image_destroy(struct tu_device *device, struct tu_image *image);
void
tu_rmv_log_image_bind(struct tu_device *device, struct tu_image *image);

void
tu_rmv_log_cmd_buffer_bo_create(struct tu_device *device,
                                struct tu_bo *bo);
void
tu_rmv_log_cmd_buffer_suballoc_bo_create(struct tu_device *device,
                                         struct tu_suballoc_bo *suballoc_bo);
void
tu_rmv_log_query_pool_create(struct tu_device *device,
                             struct tu_query_pool *query_pool);
void
tu_rmv_log_descriptor_pool_create(struct tu_device *device,
                                  const VkDescriptorPoolCreateInfo *create_info,
                                  struct tu_descriptor_pool *descriptor_pool);
void
tu_rmv_log_graphics_pipeline_create(struct tu_device *device,
                                    struct tu_graphics_pipeline *graphics_pipeline);
void
tu_rmv_log_compute_pipeline_create(struct tu_device *device,
                                   struct tu_compute_pipeline *compute_pipeline);
void
tu_rmv_log_event_create(struct tu_device *device,
                        const VkEventCreateInfo *create_info,
                        struct tu_event *event);

void
tu_rmv_log_internal_resource_create(struct tu_device *device, struct tu_bo *bo);
void
tu_rmv_log_resource_name(struct tu_device *device, const void *resource,
                         const char *resource_name);
void
tu_rmv_log_resource_destroy(struct tu_device *device, const void *resource);

#endif /* TU_RMV_H */
