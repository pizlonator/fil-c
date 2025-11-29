/*
 * Copyright © 2024 Igalia S.L.
 * SPDX-License-Identifier: MIT
 */

#include "tu_rmv.h"

#include "tu_cmd_buffer.h"
#include "tu_cs.h"
#include "tu_device.h"
#include "tu_image.h"
#include "tu_query.h"

#include <cstdio>

static VkResult
capture_trace(VkQueue _queue)
{
   VK_FROM_HANDLE(tu_queue, queue, _queue);
   struct tu_device *device = queue->device;
   assert(device->vk.memory_trace_data.is_enabled);

   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);

   vk_dump_rmv_capture(&queue->device->vk.memory_trace_data);

   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
   return VK_SUCCESS;
}

static void
tu_rmv_fill_device_info(struct tu_device *device,
                        struct vk_rmv_device_info *info)
{
   struct tu_physical_device *physical_device = device->physical_device;

   /* Turnip backends only set up a single device-local heap. When available,
    * the kernel-provided VA range is used, otherwise we fall back to that
    * heap's calculated size.
    */
   struct vk_rmv_memory_info *device_local_memory_info =
      &info->memory_infos[VK_RMV_MEMORY_LOCATION_DEVICE];
   if (physical_device->has_set_iova) {
      *device_local_memory_info = {
         .size = physical_device->va_size,
         .physical_base_address = physical_device->va_start,
      };
   } else {
      *device_local_memory_info = {
         .size = physical_device->heap.size, .physical_base_address = 0,
      };
   }

   info->memory_infos[VK_RMV_MEMORY_LOCATION_DEVICE_INVISIBLE] = {
      .size = 0, .physical_base_address = 0,
   };
   info->memory_infos[VK_RMV_MEMORY_LOCATION_HOST] = {
      .size = 0, .physical_base_address = 0,
   };

   /* No PCI-e information to provide. Instead, we can include the device's
    * chip ID in the device name string.
    */
   snprintf(info->device_name, sizeof(info->device_name), "%s (0x%" PRIx64 ")",
      physical_device->name, physical_device->dev_id.chip_id);
   info->pcie_family_id = info->pcie_revision_id = info->pcie_device_id = 0;

   /* TODO: provide relevant information here. */
   info->vram_type = VK_RMV_MEMORY_TYPE_LPDDR5;
   info->vram_operations_per_clock = info->vram_bus_width = info->vram_bandwidth = 1;
   info->minimum_shader_clock = info->minimum_memory_clock = 0;
   info->maximum_shader_clock = info->maximum_memory_clock = 1;
}

void
tu_memory_trace_init(struct tu_device *device)
{
   struct vk_rmv_device_info info;
   memset(&info, 0, sizeof(info));
   tu_rmv_fill_device_info(device, &info);

   vk_memory_trace_init(&device->vk, &info);
   if (!device->vk.memory_trace_data.is_enabled)
      return;

   device->vk.capture_trace = capture_trace;
}

void
tu_memory_trace_finish(struct tu_device *device)
{
   vk_memory_trace_finish(&device->vk);
}

static inline uint32_t
tu_rmv_get_resource_id_locked(struct tu_device *device, const void *resource)
{
   return vk_rmv_get_resource_id_locked(&device->vk, (uint64_t) resource);
}

static inline void
tu_rmv_destroy_resource_id_locked(struct tu_device *device,
                                  const void *resource)
{
   vk_rmv_destroy_resource_id_locked(&device->vk, (uint64_t) resource);
}

static inline void
tu_rmv_emit_resource_bind_locked(struct tu_device *device, uint32_t resource_id,
                                 uint64_t address, uint64_t size)
{
   struct vk_rmv_resource_bind_token token = {
      .address = address,
      .size = size,
      .is_system_memory = false,
      .resource_id = resource_id,
   };
   vk_rmv_emit_token(&device->vk.memory_trace_data,
                     VK_RMV_TOKEN_TYPE_RESOURCE_BIND, &token);
}

static inline void
tu_rmv_emit_cpu_map_locked(struct tu_device *device, uint64_t address,
                           bool unmapped)
{
   struct vk_rmv_cpu_map_token token = {
      .address = address,
      .unmapped = unmapped,
   };
   vk_rmv_emit_token(&device->vk.memory_trace_data,
                     VK_RMV_TOKEN_TYPE_CPU_MAP, &token);
}

static inline void
tu_rmv_emit_page_table_update_locked(struct tu_device *device, struct tu_bo *bo,
                                     bool is_unmap)
{
   /* These tokens are mainly useful for RMV to properly associate buffer
    * allocations and deallocations to a specific memory domain.
    */
   struct vk_rmv_page_table_update_token token = {
      .virtual_address = bo->iova,
      .physical_address = bo->iova,
      .page_count = DIV_ROUND_UP(bo->size, 4096),
      .page_size = 4096,
      .pid = 0,
      .is_unmap = is_unmap,
      .type = VK_RMV_PAGE_TABLE_UPDATE_TYPE_UPDATE,
   };
   vk_rmv_emit_token(&device->vk.memory_trace_data,
                     VK_RMV_TOKEN_TYPE_PAGE_TABLE_UPDATE, &token);
}

void
tu_rmv_log_heap_create(struct tu_device *device,
                       const VkMemoryAllocateInfo *allocate_info,
                       struct tu_device_memory *device_memory)
{
   const VkMemoryAllocateFlagsInfo *flags_info = vk_find_struct_const(
      allocate_info->pNext, MEMORY_ALLOCATE_FLAGS_INFO);

   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);

   struct vk_rmv_resource_create_token token = {
      .resource_id = tu_rmv_get_resource_id_locked(device, device_memory),
      .is_driver_internal = false,
      .type = VK_RMV_RESOURCE_TYPE_HEAP,
      .heap = {
         .alloc_flags = flags_info ? flags_info->flags : 0,
         .size = device_memory->bo->size,
         .alignment = 4096,
         .heap_index = VK_RMV_MEMORY_LOCATION_DEVICE,
      },
   };
   vk_rmv_emit_token(&device->vk.memory_trace_data,
                     VK_RMV_TOKEN_TYPE_RESOURCE_CREATE, &token);

   tu_rmv_emit_resource_bind_locked(device, token.resource_id,
                                    device_memory->bo->iova,
                                    device_memory->bo->size);

   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

void
tu_rmv_log_bo_allocate(struct tu_device *device, struct tu_bo *bo)
{
   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);

   tu_rmv_emit_page_table_update_locked(device, bo, false);

   struct vk_rmv_virtual_allocate_token virtual_allocate_token = {
      .page_count = DIV_ROUND_UP(bo->size, 4096),
      .is_driver_internal = false,
      .is_in_invisible_vram = false,
      .address = bo->iova,
      .preferred_domains = VK_RMV_KERNEL_MEMORY_DOMAIN_VRAM,
   };
   vk_rmv_emit_token(&device->vk.memory_trace_data,
                     VK_RMV_TOKEN_TYPE_VIRTUAL_ALLOCATE,
                     &virtual_allocate_token);

   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

void
tu_rmv_log_bo_destroy(struct tu_device *device, struct tu_bo *bo)
{
   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);

   struct vk_rmv_virtual_free_token virtual_free_token = {
      .address = bo->iova,
   };
   vk_rmv_emit_token(&device->vk.memory_trace_data,
                     VK_RMV_TOKEN_TYPE_VIRTUAL_FREE, &virtual_free_token);

   tu_rmv_emit_page_table_update_locked(device, bo, true);

   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

void
tu_rmv_log_bo_map(struct tu_device *device, struct tu_bo *bo)
{
   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);

   tu_rmv_emit_cpu_map_locked(device, bo->iova, false);

   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

void
tu_rmv_log_bo_unmap(struct tu_device *device, struct tu_bo *bo)
{
   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);

   tu_rmv_emit_cpu_map_locked(device, bo->iova, true);

   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

void
tu_rmv_log_buffer_create(struct tu_device *device, struct tu_buffer *buffer)
{
   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);

   struct vk_rmv_resource_create_token token = {
      .resource_id = tu_rmv_get_resource_id_locked(device, buffer),
      .is_driver_internal = false,
      .type = VK_RMV_RESOURCE_TYPE_BUFFER,
      .buffer = {
         .create_flags = buffer->vk.create_flags,
         .usage_flags = buffer->vk.usage,
         .size = buffer->vk.size,
      },
   };
   vk_rmv_emit_token(&device->vk.memory_trace_data,
                     VK_RMV_TOKEN_TYPE_RESOURCE_CREATE, &token);

   /* Any sparse data would also be reported here, if supported. */

   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

void
tu_rmv_log_buffer_destroy(struct tu_device *device, struct tu_buffer *buffer)
{
   /* Any sparse data would also be reported here, if supported. */
   tu_rmv_log_resource_destroy(device, buffer);
}

void
tu_rmv_log_buffer_bind(struct tu_device *device, struct tu_buffer *buffer)
{
   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);

   tu_rmv_emit_resource_bind_locked(device,
                                    tu_rmv_get_resource_id_locked(device, buffer),
                                    buffer->bo ? buffer->iova : 0,
                                    buffer->vk.size);

   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

void
tu_rmv_log_image_create(struct tu_device *device, struct tu_image *image)
{
   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);

   /* TODO: provide the image metadata information */
   struct vk_rmv_resource_create_token token = {
      .resource_id = tu_rmv_get_resource_id_locked(device, image),
      .is_driver_internal = false,
      .type = VK_RMV_RESOURCE_TYPE_IMAGE,
      .image = {
         .create_flags = image->vk.create_flags,
         .usage_flags = image->vk.usage,
         .type = image->vk.image_type,
         .extent = image->vk.extent,
         .format = image->vk.format,
         .num_mips = image->vk.mip_levels,
         .num_slices = image->vk.array_layers,
         .tiling = image->vk.tiling,
         .log2_samples = util_logbase2(image->vk.samples),
         .log2_storage_samples = util_logbase2(image->vk.samples),
         /* any bound memory should have alignment of 4096 */
         .alignment_log2 = util_logbase2(4096),
         .metadata_alignment_log2 = 0,
         .image_alignment_log2 = util_logbase2(image->layout[0].base_align),
         .size = image->total_size,
         .metadata_size = 0,
         .metadata_header_size = 0,
         .metadata_offset = 0,
         .metadata_header_offset = 0,
         /* TODO: find a better way to determine if an image is presentable */
         .presentable = image->vk.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      },
   };
   vk_rmv_emit_token(&device->vk.memory_trace_data,
                     VK_RMV_TOKEN_TYPE_RESOURCE_CREATE, &token);

   /* Any sparse data would also be reported here, if supported. */

   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

void
tu_rmv_log_image_destroy(struct tu_device *device, struct tu_image *image)
{
   /* Any sparse data would also be reported here, if supported. */
   tu_rmv_log_resource_destroy(device, image);
}

void
tu_rmv_log_image_bind(struct tu_device *device, struct tu_image *image)
{
   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);

   uint64_t address = image->bo ? image->iova : 0;
   uint64_t size = image->bo ? image->total_size : 0;
   tu_rmv_emit_resource_bind_locked(device,
                                    tu_rmv_get_resource_id_locked(device, image),
                                    address, size);

   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

static inline void
tu_rmv_log_command_allocator_create(struct tu_device *device, void *bo,
                                    uint64_t address, uint64_t size)
{
   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);

   struct vk_rmv_resource_create_token token = {
      .resource_id = tu_rmv_get_resource_id_locked(device, bo),
      .is_driver_internal = true,
      .type = VK_RMV_RESOURCE_TYPE_COMMAND_ALLOCATOR,
      .command_buffer = {
         .preferred_domain = VK_RMV_KERNEL_MEMORY_DOMAIN_VRAM,
         .executable_size = size,
         .app_available_executable_size = size,
         .embedded_data_size = 0,
         .app_available_embedded_data_size = 0,
         .scratch_size = 0,
         .app_available_scratch_size = 0,
      },
   };
   vk_rmv_emit_token(&device->vk.memory_trace_data,
                     VK_RMV_TOKEN_TYPE_RESOURCE_CREATE, &token);

   tu_rmv_emit_resource_bind_locked(device, token.resource_id, address, size);

   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

void
tu_rmv_log_cmd_buffer_bo_create(struct tu_device *device,
                                struct tu_bo *bo)
{
   tu_rmv_log_command_allocator_create(device, bo, bo->iova, bo->size);
}

void
tu_rmv_log_cmd_buffer_suballoc_bo_create(struct tu_device *device,
                                         struct tu_suballoc_bo *suballoc_bo)
{
   tu_rmv_log_command_allocator_create(device, suballoc_bo,
                                       suballoc_bo->iova, suballoc_bo->size);
}

void
tu_rmv_log_query_pool_create(struct tu_device *device,
                             struct tu_query_pool *query_pool)
{
   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);

   struct vk_rmv_resource_create_token token = {
      .resource_id = tu_rmv_get_resource_id_locked(device, query_pool),
      .is_driver_internal = false,
      .type = VK_RMV_RESOURCE_TYPE_QUERY_HEAP,
      .query_pool = {
         .type = query_pool->type,
         .has_cpu_access = true,
      },
   };
   vk_rmv_emit_token(&device->vk.memory_trace_data,
                     VK_RMV_TOKEN_TYPE_RESOURCE_CREATE, &token);

   tu_rmv_emit_resource_bind_locked(device, token.resource_id,
                                    query_pool->bo->iova, query_pool->bo->size);

   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

void
tu_rmv_log_descriptor_pool_create(struct tu_device *device,
                                  const VkDescriptorPoolCreateInfo *create_info,
                                  struct tu_descriptor_pool *descriptor_pool)
{
   size_t pool_sizes_size =
      create_info->poolSizeCount * sizeof(VkDescriptorPoolSize);
   VkDescriptorPoolSize *pool_sizes =
      (VkDescriptorPoolSize *) malloc(pool_sizes_size);
   if (!pool_sizes)
      return;

   memcpy(pool_sizes, create_info->pPoolSizes, pool_sizes_size);

   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);

   struct vk_rmv_resource_create_token token = {
      .resource_id = tu_rmv_get_resource_id_locked(device, descriptor_pool),
      .is_driver_internal = false,
      .type = VK_RMV_RESOURCE_TYPE_DESCRIPTOR_POOL,
      .descriptor_pool = {
         .max_sets = create_info->maxSets,
         .pool_size_count = create_info->poolSizeCount,
         .pool_sizes = pool_sizes,
      },
   };
   vk_rmv_emit_token(&device->vk.memory_trace_data,
                     VK_RMV_TOKEN_TYPE_RESOURCE_CREATE, &token);

   if (descriptor_pool->bo) {
      tu_rmv_emit_resource_bind_locked(device, token.resource_id,
                                       descriptor_pool->bo->iova,
                                       descriptor_pool->bo->size);
   }

   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

static inline void
tu_rmv_log_pipeline_create(struct tu_device *device,
                           struct tu_pipeline *pipeline)
{
   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);

   struct vk_rmv_resource_create_token token = {
      .resource_id = tu_rmv_get_resource_id_locked(device, pipeline),
      .is_driver_internal = false,
      .type = VK_RMV_RESOURCE_TYPE_PIPELINE,
      .pipeline = {
         .is_internal = false,
         /* TODO: provide pipeline hash data when available. */
         .hash_lo = 0, .hash_hi = 0,
         .shader_stages = pipeline->active_stages,
         .is_ngg = false,
      },
   };
   vk_rmv_emit_token(&device->vk.memory_trace_data,
                     VK_RMV_TOKEN_TYPE_RESOURCE_CREATE, &token);

   if (pipeline->bo.bo) {
      tu_rmv_emit_resource_bind_locked(device, token.resource_id,
                                       pipeline->bo.iova, pipeline->bo.size);
   }

   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

void
tu_rmv_log_graphics_pipeline_create(struct tu_device *device,
                                    struct tu_graphics_pipeline *graphics_pipeline)
{
   tu_rmv_log_pipeline_create(device, &graphics_pipeline->base);
}

void
tu_rmv_log_compute_pipeline_create(struct tu_device *device,
                                   struct tu_compute_pipeline *compute_pipeline)
{
   tu_rmv_log_pipeline_create(device, &compute_pipeline->base);
}

void
tu_rmv_log_event_create(struct tu_device *device,
                        const VkEventCreateInfo *create_info,
                        struct tu_event *event)
{
   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);

   struct vk_rmv_resource_create_token token = {
      .resource_id = tu_rmv_get_resource_id_locked(device, event),
      .is_driver_internal = false,
      .type = VK_RMV_RESOURCE_TYPE_GPU_EVENT,
      .event = {
         .flags = create_info->flags,
      },
   };
   vk_rmv_emit_token(&device->vk.memory_trace_data,
                     VK_RMV_TOKEN_TYPE_RESOURCE_CREATE, &token);

   if (event->bo) {
      tu_rmv_emit_resource_bind_locked(device, token.resource_id,
                                       event->bo->iova, event->bo->size);
   }

   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

void
tu_rmv_log_internal_resource_create(struct tu_device *device, struct tu_bo *bo)
{
   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);

   struct vk_rmv_resource_create_token token = {
      .resource_id = tu_rmv_get_resource_id_locked(device, bo),
      .is_driver_internal = true,
      .type = VK_RMV_RESOURCE_TYPE_MISC_INTERNAL,
      .misc_internal = {
         .type = VK_RMV_MISC_INTERNAL_TYPE_PADDING,
      },
   };
   vk_rmv_emit_token(&device->vk.memory_trace_data,
                     VK_RMV_TOKEN_TYPE_RESOURCE_CREATE, &token);

   tu_rmv_emit_resource_bind_locked(device, token.resource_id,
                                    bo->iova, bo->size);

   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

void
tu_rmv_log_resource_name(struct tu_device *device, const void *resource,
                         const char *resource_name)
{
   size_t name_len = MIN2(strlen(resource_name) + 1, 128);
   char *name_buf = (char *) malloc(name_len);
   if (!name_buf)
      return;

   strncpy(name_buf, resource_name, name_len);
   name_buf[name_len - 1] = '\0';

   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);

   struct vk_rmv_userdata_token token = {
      .name = name_buf,
      .resource_id = tu_rmv_get_resource_id_locked(device, resource)
   };
   vk_rmv_emit_token(&device->vk.memory_trace_data,
                     VK_RMV_TOKEN_TYPE_USERDATA, &token);

   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

void
tu_rmv_log_resource_destroy(struct tu_device *device, const void *resource)
{
   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);

   struct vk_rmv_resource_destroy_token token = {
      .resource_id = tu_rmv_get_resource_id_locked(device, resource),
   };
   vk_rmv_emit_token(&device->vk.memory_trace_data,
                     VK_RMV_TOKEN_TYPE_RESOURCE_DESTROY, &token);

   tu_rmv_destroy_resource_id_locked(device, resource);
   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

