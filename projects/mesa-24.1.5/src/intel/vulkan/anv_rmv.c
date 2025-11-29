/*
 * Copyright © 2023 Intel Corporation
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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include "anv_private.h"

static VkResult
capture_trace(VkQueue _queue)
{
   ANV_FROM_HANDLE(anv_queue, queue, _queue);

   simple_mtx_lock(&queue->device->vk.memory_trace_data.token_mtx);
   vk_dump_rmv_capture(&queue->device->vk.memory_trace_data);
   simple_mtx_unlock(&queue->device->vk.memory_trace_data.token_mtx);

   return VK_SUCCESS;
}

void
anv_memory_trace_init(struct anv_device *device)
{
   struct vk_rmv_device_info info;
   memset(&info, 0, sizeof(info));
   anv_rmv_fill_device_info(device->physical, &info);
   vk_memory_trace_init(&device->vk, &info);

   if (!device->vk.memory_trace_data.is_enabled)
      return;

   device->vk.capture_trace = capture_trace;
}

static void
fill_memory_info(const struct anv_physical_device *device,
                 struct vk_rmv_memory_info *out_info,
                 int32_t index)
{
   switch (index) {
   case VK_RMV_MEMORY_LOCATION_DEVICE:
      out_info->physical_base_address = 0;
      out_info->size = device->memory.heaps[0].size;
      break;
   case VK_RMV_MEMORY_LOCATION_DEVICE_INVISIBLE:
      out_info->physical_base_address = device->memory.heaps[0].size;
      out_info->size = device->vram_non_mappable.size;
      break;
   case VK_RMV_MEMORY_LOCATION_HOST:
      out_info->physical_base_address = 0;
      out_info->size = device->memory.heaps[1].size;
      break;
   default:
      unreachable("invalid memory index");
   }
}

void
anv_rmv_fill_device_info(const struct anv_physical_device *device,
                         struct vk_rmv_device_info *info)
{
   for (int32_t i = 0; i < VK_RMV_MEMORY_LOCATION_COUNT; ++i)
      fill_memory_info(device, &info->memory_infos[i], i);

   strncpy(info->device_name, device->info.name, sizeof(info->device_name) - 1);
   info->pcie_revision_id = device->info.pci_revision_id;
   info->pcie_device_id = device->info.pci_device_id;
   /* TODO: */
   info->pcie_family_id = 0;
   info->minimum_shader_clock = 0;
   info->maximum_shader_clock = 1 * 1024 * 1024 * 1024;
   info->vram_type = VK_RMV_MEMORY_TYPE_DDR4;
   info->vram_bus_width = 256;
   info->vram_operations_per_clock = 1;
   info->minimum_memory_clock = 0;
   info->maximum_memory_clock = 1;
   info->vram_bandwidth = 256;
}

void
anv_memory_trace_finish(struct anv_device *device)
{
}

static uint32_t
resource_id_locked(struct anv_device *device, const void *obj)
{
   return vk_rmv_get_resource_id_locked(&device->vk, (uint64_t)(uintptr_t)obj);
}

static void
resource_destroy_locked(struct anv_device *device, const void *obj)
{
   vk_rmv_destroy_resource_id_locked(&device->vk, (uint64_t)(uintptr_t)obj);
}

/* The token lock must be held when entering _locked functions */
static void
log_resource_bind_locked(struct anv_device *device, uint64_t resource_id,
                         struct anv_bo *bo, uint64_t offset,
                         uint64_t size)
{
   struct vk_rmv_resource_bind_token token = {
      .resource_id      = resource_id,
      .is_system_memory = bo ? (bo->alloc_flags & ANV_BO_ALLOC_NO_LOCAL_MEM) : 0,
      .address          = (bo ? bo->offset : 0) + offset,
      .size             = size,
   };

   vk_rmv_emit_token(&device->vk.memory_trace_data, VK_RMV_TOKEN_TYPE_RESOURCE_BIND, &token);
}

static void
log_state_pool_bind_locked(struct anv_device *device, uint64_t resource_id,
                           struct anv_state_pool *pool, struct anv_state *state)
{
   struct vk_rmv_resource_bind_token token = {
      .resource_id      = resource_id,
      .is_system_memory = (pool->block_pool.bo_alloc_flags &
                           ANV_BO_ALLOC_NO_LOCAL_MEM) != 0,
      .address          = anv_address_physical(
         anv_state_pool_state_address(pool, *state)),
      .size             = state->alloc_size,
   };

   vk_rmv_emit_token(&device->vk.memory_trace_data, VK_RMV_TOKEN_TYPE_RESOURCE_BIND, &token);
}

static enum vk_rmv_memory_location
anv_heap_index_to_memory_location(struct anv_device *device,
                                  unsigned heap_index)
{
   if (heap_index == 0)
      return device->physical->vram_non_mappable.size != 0 ?
             VK_RMV_MEMORY_LOCATION_DEVICE_INVISIBLE :
             VK_RMV_MEMORY_LOCATION_DEVICE;
   else if (heap_index == 1)
      return VK_RMV_MEMORY_LOCATION_HOST;
   else
      return VK_RMV_MEMORY_LOCATION_DEVICE;
}

static void
anv_rmv_log_bo_gtt_unmap_locked(struct anv_device *device,
                                struct anv_bo *bo)
{
   if (!bo->gtt_mapped)
      return;

   struct vk_rmv_token token = {
      .type      = VK_RMV_TOKEN_TYPE_PAGE_TABLE_UPDATE,
      .timestamp = (uint64_t)os_time_get_nano(),
      .data      = {
         .page_table_update = {
            .type             = VK_RMV_PAGE_TABLE_UPDATE_TYPE_UPDATE,
            .page_size        = device->info->mem_alignment,
            .page_count       = DIV_ROUND_UP(bo->size,
                                             device->info->mem_alignment),
            .pid              = getpid(),
            .virtual_address  = bo->offset,
            .physical_address = bo->offset,
            .is_unmap         = true,
         },
      },
   };
   util_dynarray_append(&device->vk.memory_trace_data.tokens,
                        struct vk_rmv_token, token);

   bo->gtt_mapped = false;
}

void
anv_rmv_log_bo_gtt_unmap(struct anv_device *device,
                         struct anv_bo *bo)
{
   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);
   anv_rmv_log_bo_gtt_unmap_locked(device, bo);
   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

void
anv_rmv_log_bo_gtt_map(struct anv_device *device,
                       struct anv_bo *bo)
{
   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);
   struct vk_rmv_token token = {
      .type      = VK_RMV_TOKEN_TYPE_PAGE_TABLE_UPDATE,
      .timestamp = (uint64_t)os_time_get_nano(),
      .data      = {
         .page_table_update = {
            .type             = VK_RMV_PAGE_TABLE_UPDATE_TYPE_UPDATE,
            .page_size        = device->info->mem_alignment,
            .page_count       = DIV_ROUND_UP(bo->size,
                                                device->info->mem_alignment),
            .pid              = getpid(),
            .virtual_address  = bo->offset,
            .physical_address = bo->offset,
            .is_unmap         = false,
            },
      },
   };
   util_dynarray_append(&device->vk.memory_trace_data.tokens,
                        struct vk_rmv_token, token);

   bo->gtt_mapped = true;

   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

void
anv_rmv_log_bos_gtt_map(struct anv_device *device,
                        struct anv_bo **bos,
                        uint32_t bo_count)
{
   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);
   for (uint32_t i = 0; i < bo_count; i++) {
      struct anv_bo *bo = bos[i];

      if (bo->gtt_mapped)
         continue;

      struct vk_rmv_token token = {
         .type      = VK_RMV_TOKEN_TYPE_PAGE_TABLE_UPDATE,
         .timestamp = (uint64_t)os_time_get_nano(),
         .data      = {
            .page_table_update = {
               .type             = VK_RMV_PAGE_TABLE_UPDATE_TYPE_UPDATE,
               .page_size        = device->info->mem_alignment,
               .page_count       = DIV_ROUND_UP(bo->size,
                                                device->info->mem_alignment),
               .pid              = getpid(),
               .virtual_address  = bo->offset,
               .physical_address = bo->offset,
               .is_unmap         = false,
            },
         },
      };
      util_dynarray_append(&device->vk.memory_trace_data.tokens,
                           struct vk_rmv_token, token);

      bo->gtt_mapped = true;
   }
   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

void
anv_rmv_log_vm_binds(struct anv_device *device,
                     struct anv_vm_bind *binds,
                     uint32_t bind_count)
{
   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);
   for (uint32_t i = 0; i < bind_count; i++) {

      struct vk_rmv_token token = {
         .type      = VK_RMV_TOKEN_TYPE_PAGE_TABLE_UPDATE,
         .timestamp = (uint64_t)os_time_get_nano(),
         .data      = {
            .page_table_update = {
               .type             = VK_RMV_PAGE_TABLE_UPDATE_TYPE_UPDATE,
               .page_size        = device->info->mem_alignment,
               .page_count       = DIV_ROUND_UP(binds[i].size,
                                                device->info->mem_alignment),
               .pid              = getpid(),
               .virtual_address  = binds[i].address,
               .physical_address = binds[i].bo_offset,
               .is_unmap         = binds[i].op == ANV_VM_UNBIND,
            },
         },
      };
      util_dynarray_append(&device->vk.memory_trace_data.tokens,
                           struct vk_rmv_token, token);
   }
   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

void
anv_rmv_log_heap_create(struct anv_device *device,
                        struct anv_device_memory *memory,
                        bool is_internal,
                        VkMemoryAllocateFlags alloc_flags)
{
   /* Do not log zero-sized device memory objects. */
   if (!memory->vk.size)
      return;

   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);

   struct vk_rmv_resource_create_token token = {
      .type               = VK_RMV_RESOURCE_TYPE_HEAP,
      .resource_id        = resource_id_locked(device, memory),
      .is_driver_internal = is_internal,
      .heap = {
         .alignment   = device->info->mem_alignment,
         .size        = memory->vk.size,
         .heap_index  = anv_heap_index_to_memory_location(device,
                                                          memory->type->heapIndex),
         .alloc_flags = alloc_flags,
      },
   };

   vk_rmv_emit_token(&device->vk.memory_trace_data, VK_RMV_TOKEN_TYPE_RESOURCE_CREATE, &token);
   log_resource_bind_locked(device, token.resource_id, memory->bo, 0, memory->vk.size);
   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

static void
anv_rmv_log_vma_locked(struct anv_device *device, uint64_t address, uint64_t size,
                       bool internal, bool vram, bool in_invisible_vram)
{
   struct vk_rmv_virtual_allocate_token token = {
      .address              = address,
      /* If all VRAM is visible, no bo will be in invisible memory. */
      .is_in_invisible_vram = in_invisible_vram,
      .preferred_domains    = (vram ?
                               VK_RMV_KERNEL_MEMORY_DOMAIN_VRAM :
                               VK_RMV_KERNEL_MEMORY_DOMAIN_GTT),
      .is_driver_internal   = internal,
      .page_count           = DIV_ROUND_UP(size, 4096),
   };


   vk_rmv_emit_token(&device->vk.memory_trace_data, VK_RMV_TOKEN_TYPE_VIRTUAL_ALLOCATE, &token);
}

void
anv_rmv_log_bo_allocate(struct anv_device *device,
                        struct anv_bo *bo)
{
   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);
   anv_rmv_log_vma_locked(device, bo->offset, bo->size,
                          bo->alloc_flags & ANV_BO_ALLOC_INTERNAL,
                          (bo->alloc_flags & ANV_BO_ALLOC_NO_LOCAL_MEM) == 0,
                          device->physical->vram_non_mappable.size != 0 &&
                          (bo->alloc_flags & (ANV_BO_ALLOC_MAPPED |
                                              ANV_BO_ALLOC_HOST_CACHED_COHERENT |
                                              ANV_BO_ALLOC_LOCAL_MEM_CPU_VISIBLE |
                                              ANV_BO_ALLOC_NO_LOCAL_MEM)) == 0);
   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);

   if (bo->alloc_flags & ANV_BO_ALLOC_MAPPED)
      vk_rmv_log_cpu_map(&device->vk, bo->offset, false);
}

void
anv_rmv_log_bo_destroy(struct anv_device *device, struct anv_bo *bo)
{
   struct vk_rmv_virtual_free_token token = {
      .address = bo->offset,
   };

   if (bo->alloc_flags & ANV_BO_ALLOC_MAPPED)
      vk_rmv_log_cpu_map(&device->vk, bo->offset, true);
   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);
   anv_rmv_log_bo_gtt_unmap_locked(device, bo);
   vk_rmv_emit_token(&device->vk.memory_trace_data, VK_RMV_TOKEN_TYPE_VIRTUAL_FREE, &token);
   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

void
anv_rmv_log_buffer_create(struct anv_device *device,
                          bool is_internal,
                          struct anv_buffer *buffer)
{
   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);
   struct vk_rmv_resource_create_token token = {
      .type               = VK_RMV_RESOURCE_TYPE_BUFFER,
      .is_driver_internal = is_internal,
      .resource_id        = resource_id_locked(device, buffer),
      .buffer             = {
         .create_flags = buffer->vk.create_flags,
         .size         = buffer->vk.size,
         .usage_flags  = buffer->vk.usage,
      },
   };

   vk_rmv_emit_token(&device->vk.memory_trace_data, VK_RMV_TOKEN_TYPE_RESOURCE_CREATE, &token);
   if (buffer->vk.create_flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT) {
      assert(buffer->sparse_data.size != 0);
      anv_rmv_log_vma_locked(device,
                             buffer->sparse_data.address,
                             buffer->sparse_data.size,
                             false /* internal */, true /* TODO: vram */,
                             true /* in_invisible_vram */);
      log_resource_bind_locked(device,
                               resource_id_locked(device, buffer),
                               NULL,
                               buffer->sparse_data.address,
                               buffer->sparse_data.size);
   }
   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);

}

void
anv_rmv_log_buffer_destroy(struct anv_device *device,
                           struct anv_buffer *buffer)
{
   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);
   if (buffer->vk.create_flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT) {
      struct vk_rmv_virtual_free_token token = {
         .address = buffer->sparse_data.address,
      };
      vk_rmv_emit_token(&device->vk.memory_trace_data, VK_RMV_TOKEN_TYPE_VIRTUAL_FREE, &token);
   }
   resource_destroy_locked(device, buffer);
   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);

}

void
anv_rmv_log_buffer_bind(struct anv_device *device, struct anv_buffer *buffer)
{
   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);
   log_resource_bind_locked(device,
                            resource_id_locked(device, buffer),
                            buffer->address.bo,
                            buffer->address.offset, buffer->vk.size);
   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

void
anv_rmv_log_image_create(struct anv_device *device,
                         bool is_internal,
                         struct anv_image *image)
{
   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);
   struct vk_rmv_resource_create_token token = {
      .type               = VK_RMV_RESOURCE_TYPE_IMAGE,
      .resource_id        = resource_id_locked(device, image),
      .is_driver_internal = is_internal,
      .image              = {
         .create_flags            = image->vk.create_flags,
         .usage_flags             = image->vk.usage,
         .type                    = image->vk.image_type,
         .extent                  = image->vk.extent,
         .format                  = image->vk.format,
         .num_mips                = image->vk.mip_levels,
         .num_slices              = image->vk.array_layers,
         .tiling                  = image->vk.tiling,
         .alignment_log2          = util_logbase2(
            image->bindings[ANV_IMAGE_MEMORY_BINDING_MAIN].memory_range.alignment),
         .log2_samples            = util_logbase2(image->vk.samples),
         .metadata_alignment_log2 = util_logbase2(
            image->planes[0].aux_surface.isl.alignment_B),
         .image_alignment_log2    = util_logbase2(
            image->planes[0].primary_surface.isl.alignment_B),
         .size                    = image->planes[0].primary_surface.memory_range.size,
         .metadata_size           = image->planes[0].aux_surface.memory_range.size,
         .metadata_header_size    = 0,
         .metadata_offset         = image->planes[0].aux_surface.memory_range.offset,
         .metadata_header_offset  = image->planes[0].aux_surface.memory_range.offset,
         .presentable             = (image->planes[0].primary_surface.isl.usage &
                                     ISL_SURF_USAGE_DISPLAY_BIT) != 0,
      },
   };

   vk_rmv_emit_token(&device->vk.memory_trace_data, VK_RMV_TOKEN_TYPE_RESOURCE_CREATE, &token);
   if (image->vk.create_flags & VK_IMAGE_CREATE_SPARSE_BINDING_BIT) {
      for (uint32_t b = 0; b < ARRAY_SIZE(image->bindings); b++) {
         if (image->bindings[b].sparse_data.size != 0) {
            anv_rmv_log_vma_locked(device,
                                   image->bindings[b].sparse_data.address,
                                   image->bindings[b].sparse_data.size,
                                   false /* internal */, true /* TODO: vram */,
                                   true /* in_invisible_vram */);
            log_resource_bind_locked(device,
                                     resource_id_locked(device, image),
                                     NULL,
                                     image->bindings[b].sparse_data.address,
                                     image->bindings[b].sparse_data.size);
         }
      }
   }
   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

void
anv_rmv_log_image_destroy(struct anv_device *device,
                          struct anv_image *image)
{
   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);
   if (image->vk.create_flags & VK_IMAGE_CREATE_SPARSE_BINDING_BIT) {
      for (uint32_t b = 0; b < ARRAY_SIZE(image->bindings); b++) {
         if (image->bindings[b].sparse_data.size != 0) {
            struct vk_rmv_virtual_free_token token = {
               .address = image->bindings[b].sparse_data.address,
            };

            vk_rmv_emit_token(&device->vk.memory_trace_data, VK_RMV_TOKEN_TYPE_VIRTUAL_FREE, &token);
         }
      }
   }
   resource_destroy_locked(device, image);
   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

void
anv_rmv_log_image_bind(struct anv_device *device,
                       struct anv_image *image,
                       enum anv_image_memory_binding binding)
{
   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);
   log_resource_bind_locked(device,
                            resource_id_locked(device, image),
                            image->bindings[binding].address.bo,
                            image->bindings[binding].address.offset,
                            image->bindings[binding].memory_range.size);
   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

void
anv_rmv_log_query_pool_create(struct anv_device *device,
                              struct anv_query_pool *pool,
                              bool is_internal)
{
   if (pool->vk.query_type != VK_QUERY_TYPE_OCCLUSION &&
       pool->vk.query_type != VK_QUERY_TYPE_PIPELINE_STATISTICS &&
       pool->vk.query_type != VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT)
      return;

   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);
   struct vk_rmv_resource_create_token create_token = {
      .type               = VK_RMV_RESOURCE_TYPE_QUERY_HEAP,
      .resource_id        = resource_id_locked(device, pool),
      .is_driver_internal = is_internal,
      .query_pool = {
         .type           = pool->vk.query_type,
         .has_cpu_access = true,
      },
   };

   vk_rmv_emit_token(&device->vk.memory_trace_data,
                     VK_RMV_TOKEN_TYPE_RESOURCE_CREATE, &create_token);
   log_resource_bind_locked(device, create_token.resource_id,
                            pool->bo, 0, pool->bo->size);
   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

static void
bind_cmd_buffer_state_stream_locked(struct anv_device *device,
                                    uint64_t resource_id,
                                    struct anv_state_stream *stream)
{
   util_dynarray_foreach(&stream->all_blocks, struct anv_state, block)
      log_state_pool_bind_locked(device, resource_id, stream->state_pool, block);
}

void
anv_rmv_log_cmd_buffer_create(struct anv_device *device,
                              struct anv_cmd_buffer *cmd_buffer)
{
   uint64_t data_size =
      cmd_buffer->surface_state_stream.total_size +
      cmd_buffer->dynamic_state_stream.total_size +
      cmd_buffer->general_state_stream.total_size +
      cmd_buffer->indirect_push_descriptor_stream.total_size;

   uint64_t executable_size = 0;
   list_for_each_entry(struct anv_batch_bo, bbo, &cmd_buffer->batch_bos, link)
      executable_size += bbo->length;

   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);
   struct vk_rmv_resource_create_token create_token = {
      .type               = VK_RMV_RESOURCE_TYPE_COMMAND_ALLOCATOR,
      .resource_id        = resource_id_locked(device, cmd_buffer),
      .is_driver_internal = true,
      .command_buffer     = {
         .preferred_domain                 = VK_RMV_KERNEL_MEMORY_DOMAIN_GTT /* TODO */,
         .executable_size                  = executable_size,
         .app_available_executable_size    = executable_size,
         .embedded_data_size               = data_size,
         .app_available_embedded_data_size = data_size,
         .scratch_size                     = 0,
         .app_available_scratch_size       = 0,
      },
   };

   vk_rmv_emit_token(&device->vk.memory_trace_data,
                     VK_RMV_TOKEN_TYPE_RESOURCE_CREATE,
                     &create_token);
   list_for_each_entry(struct anv_batch_bo, bbo, &cmd_buffer->batch_bos, link) {
      log_resource_bind_locked(device, create_token.resource_id,
                               bbo->bo, 0, bbo->length);
   }
   bind_cmd_buffer_state_stream_locked(device, create_token.resource_id,
                                       &cmd_buffer->surface_state_stream);
   bind_cmd_buffer_state_stream_locked(device, create_token.resource_id,
                                       &cmd_buffer->dynamic_state_stream);
   bind_cmd_buffer_state_stream_locked(device, create_token.resource_id,
                                       &cmd_buffer->general_state_stream);
   bind_cmd_buffer_state_stream_locked(device, create_token.resource_id,
                                       &cmd_buffer->indirect_push_descriptor_stream);
   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

void
anv_rmv_log_cmd_buffer_destroy(struct anv_device *device,
                               struct anv_cmd_buffer *cmd_buffer)
{
   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);
   struct vk_rmv_resource_destroy_token destroy_token = {
      .resource_id = resource_id_locked(device, cmd_buffer),
   };

   vk_rmv_emit_token(&device->vk.memory_trace_data,
                     VK_RMV_TOKEN_TYPE_RESOURCE_DESTROY, &destroy_token);
   resource_destroy_locked(device, cmd_buffer);
   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

void
anv_rmv_log_sparse_add_residency(struct anv_device *device,
                                 struct anv_bo *src_bo,
                                 uint64_t offset)
{
   struct vk_rmv_resource_reference_token token = {
      .virtual_address   = src_bo->offset + offset,
      .residency_removed = false,
   };

   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);
   vk_rmv_emit_token(&device->vk.memory_trace_data,
                     VK_RMV_TOKEN_TYPE_RESOURCE_REFERENCE, &token);
   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

void
anv_rmv_log_sparse_remove_residency(struct anv_device *device,
                                    struct anv_bo *src_bo,
                                    uint64_t offset)
{
   struct vk_rmv_resource_reference_token token = {
      .virtual_address   = src_bo->offset + offset,
      .residency_removed = true,
   };

   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);
   vk_rmv_emit_token(&device->vk.memory_trace_data,
                     VK_RMV_TOKEN_TYPE_RESOURCE_REFERENCE, &token);
   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

void
anv_rmv_log_descriptor_pool_create(struct anv_device *device,
                                   const VkDescriptorPoolCreateInfo *create_info,
                                   struct anv_descriptor_pool *pool,
                                   bool is_internal)
{
   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);
   struct vk_rmv_resource_create_token create_token = {
      .type               = VK_RMV_RESOURCE_TYPE_DESCRIPTOR_POOL,
      .resource_id        = resource_id_locked(device, pool),
      .is_driver_internal = false,
      .descriptor_pool    = {
         .max_sets        = create_info->maxSets,
         .pool_size_count = create_info->poolSizeCount,
         /* Using vk_rmv_token_pool_alloc frees the allocation automatically
          * when the trace is done. */
         .pool_sizes      = malloc(create_info->poolSizeCount *
                                   sizeof(VkDescriptorPoolSize)),
      },
   };

   if (!create_token.descriptor_pool.pool_sizes) {
      simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
      return;
   }

   memcpy(create_token.descriptor_pool.pool_sizes, create_info->pPoolSizes,
          create_info->poolSizeCount * sizeof(VkDescriptorPoolSize));

   vk_rmv_emit_token(&device->vk.memory_trace_data,
                     VK_RMV_TOKEN_TYPE_RESOURCE_CREATE, &create_token);
   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);

   if (pool->surfaces.bo) {
      struct vk_rmv_resource_bind_token bind_token = {
         .resource_id      = create_token.resource_id,
         .is_system_memory = false,
         .address          = pool->surfaces.bo->offset,
         .size             = pool->surfaces.bo->size,
      };

      simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);
      vk_rmv_emit_token(&device->vk.memory_trace_data, VK_RMV_TOKEN_TYPE_RESOURCE_BIND, &bind_token);
      simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
   }
   if (pool->samplers.bo) {
      struct vk_rmv_resource_bind_token bind_token = {
         .resource_id      = create_token.resource_id,
         .is_system_memory = false,
         .address          = pool->samplers.bo->offset,
         .size             = pool->samplers.bo->size,
      };

      simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);
      vk_rmv_emit_token(&device->vk.memory_trace_data, VK_RMV_TOKEN_TYPE_RESOURCE_BIND, &bind_token);
      simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
   }
}

void
anv_rmv_log_graphics_pipeline_create(struct anv_device *device,
                                     struct anv_graphics_pipeline *pipeline,
                                     bool is_internal)
{
   struct vk_rmv_resource_create_token create_token = {
      .type               = VK_RMV_RESOURCE_TYPE_PIPELINE,
      .resource_id        = resource_id_locked(device, pipeline),
      .is_driver_internal = is_internal,
      .pipeline           = {
         .is_internal   = is_internal,
         .hash_lo       = 0,/* TODO pipeline->pipeline_hash; */
         .shader_stages = pipeline->base.base.active_stages,
      },
   };

   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);
   vk_rmv_emit_token(&device->vk.memory_trace_data, VK_RMV_TOKEN_TYPE_RESOURCE_CREATE, &create_token);
   for (unsigned s = 0; s < ARRAY_SIZE(pipeline->base.shaders); s++) {
      struct anv_shader_bin *shader = pipeline->base.shaders[s];

      if (!shader)
         continue;

      log_state_pool_bind_locked(device, create_token.resource_id,
                                 &device->instruction_state_pool,
                                 &shader->kernel);
   }
   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

void
anv_rmv_log_compute_pipeline_create(struct anv_device *device,
                                    struct anv_compute_pipeline *pipeline,
                                    bool is_internal)
{
   VkShaderStageFlagBits active_stages =
      pipeline->base.type == ANV_PIPELINE_COMPUTE ?
      VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_RAYGEN_BIT_KHR;

   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);
   struct vk_rmv_resource_create_token create_token = {
      .type               = VK_RMV_RESOURCE_TYPE_PIPELINE,
      .resource_id        = resource_id_locked(device, pipeline),
      .is_driver_internal = is_internal,
      .pipeline           = {
         .is_internal   = is_internal,
         .hash_lo       = 0,/* TODO pipeline->pipeline_hash; */
         .shader_stages = active_stages,
      },
   };

   vk_rmv_emit_token(&device->vk.memory_trace_data, VK_RMV_TOKEN_TYPE_RESOURCE_CREATE, &create_token);
   struct anv_shader_bin *shader = pipeline->cs;
   log_state_pool_bind_locked(device, create_token.resource_id,
                              &device->instruction_state_pool,
                              &shader->kernel);
   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

void
anv_rmv_log_rt_pipeline_create(struct anv_device *device,
                               struct anv_ray_tracing_pipeline *pipeline,
                               bool is_internal)
{
   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);

   struct vk_rmv_resource_create_token create_token = {
      .resource_id        = resource_id_locked(device, pipeline),
      .type               = VK_RMV_RESOURCE_TYPE_PIPELINE,
      .is_driver_internal = is_internal,
      .pipeline           = {
         .is_internal   = is_internal,
         .hash_lo       = 0, /* TODO */
         .shader_stages = pipeline->base.active_stages,
      },
   };
   vk_rmv_emit_token(&device->vk.memory_trace_data, VK_RMV_TOKEN_TYPE_RESOURCE_CREATE, &create_token);

   struct anv_state_pool *state_pool = &device->instruction_state_pool;
   for (uint32_t i = 0; i < pipeline->group_count; i++) {
      struct anv_rt_shader_group *group = &pipeline->groups[i];

      if (group->imported)
         continue;

      if (group->general) {
         log_state_pool_bind_locked(device, create_token.resource_id, state_pool,
                                    &group->general->kernel);
      }
      if (group->closest_hit) {
         log_state_pool_bind_locked(device, create_token.resource_id, state_pool,
                                    &group->closest_hit->kernel);
      }
      if (group->any_hit) {
         log_state_pool_bind_locked(device, create_token.resource_id, state_pool,
                                    &group->any_hit->kernel);
      }
      if (group->intersection) {
         log_state_pool_bind_locked(device, create_token.resource_id, state_pool,
                                    &group->intersection->kernel);
      }
   }

   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

void
anv_rmv_log_event_create(struct anv_device *device,
                         struct anv_event *event,
                         VkEventCreateFlags flags,
                         bool is_internal)
{
   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);
   struct vk_rmv_resource_create_token create_token = {
      .type               = VK_RMV_RESOURCE_TYPE_GPU_EVENT,
      .resource_id        = resource_id_locked(device, event),
      .is_driver_internal = is_internal,
      .event              = {
         .flags = flags,
      },
   };

   vk_rmv_emit_token(&device->vk.memory_trace_data, VK_RMV_TOKEN_TYPE_RESOURCE_CREATE, &create_token);
   log_state_pool_bind_locked(device, create_token.resource_id,
                              &device->dynamic_state_pool,
                              &event->state);
   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}

void
anv_rmv_log_resource_destroy(struct anv_device *device, const void *obj)
{
   simple_mtx_lock(&device->vk.memory_trace_data.token_mtx);
   struct vk_rmv_resource_destroy_token token = {
      .resource_id = resource_id_locked(device, obj),
   };

   vk_rmv_emit_token(&device->vk.memory_trace_data, VK_RMV_TOKEN_TYPE_RESOURCE_DESTROY, &token);
   resource_destroy_locked(device, obj);
   simple_mtx_unlock(&device->vk.memory_trace_data.token_mtx);
}
