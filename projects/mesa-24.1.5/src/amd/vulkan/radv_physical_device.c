/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#include <fcntl.h>

#ifdef MAJOR_IN_SYSMACROS
#include <sys/sysmacros.h>
#endif

#include "vk_log.h"
#include "vk_shader_module.h"

#include "util/disk_cache.h"
#include "util/hex.h"
#include "util/u_debug.h"
#include "radv_android.h"
#include "radv_debug.h"
#include "radv_entrypoints.h"
#include "radv_instance.h"
#include "radv_physical_device.h"
#include "radv_pipeline_rt.h"
#include "radv_video.h"
#include "radv_wsi.h"

#ifdef _WIN32
typedef void *drmDevicePtr;
#include <io.h>
#else
#include <amdgpu.h>
#include <xf86drm.h>
#include "drm-uapi/amdgpu_drm.h"
#include "winsys/amdgpu/radv_amdgpu_winsys_public.h"
#endif
#include "winsys/null/radv_null_winsys_public.h"
#include "git_sha1.h"

#if LLVM_AVAILABLE
#include "ac_llvm_util.h"
#endif

#ifdef _WIN32
#define RADV_SUPPORT_CALIBRATED_TIMESTAMPS 0
#else
#define RADV_SUPPORT_CALIBRATED_TIMESTAMPS 1
#endif

static bool
radv_perf_query_supported(const struct radv_physical_device *pdev)
{
   const struct radv_instance *instance = radv_physical_device_instance(pdev);

   /* SQTT / SPM interfere with the register states for perf counters, and
    * the code has only been tested on GFX10.3 */
   return pdev->info.gfx_level == GFX10_3 && !(instance->vk.trace_mode & RADV_TRACE_MODE_RGP);
}

static bool
radv_taskmesh_enabled(const struct radv_physical_device *pdev)
{
   const struct radv_instance *instance = radv_physical_device_instance(pdev);

   if (instance->debug_flags & RADV_DEBUG_NO_MESH_SHADER)
      return false;

   return pdev->use_ngg && !pdev->use_llvm && pdev->info.gfx_level >= GFX10_3 &&
          !(instance->debug_flags & RADV_DEBUG_NO_COMPUTE_QUEUE) && pdev->info.has_gang_submit;
}

static bool
radv_transfer_queue_enabled(const struct radv_physical_device *pdev)
{
   const struct radv_instance *instance = radv_physical_device_instance(pdev);

   /* Check if the GPU has SDMA support and transfer queues are allowed. */
   if (pdev->info.sdma_ip_version == SDMA_UNKNOWN || !pdev->info.ip[AMD_IP_SDMA].num_queues ||
       !(instance->perftest_flags & RADV_PERFTEST_TRANSFER_QUEUE))
      return false;

   return pdev->info.gfx_level >= GFX9;
}

static bool
radv_vrs_attachment_enabled(const struct radv_physical_device *pdev)
{
   const struct radv_instance *instance = radv_physical_device_instance(pdev);
   return pdev->info.gfx_level >= GFX11 || !(instance->debug_flags & RADV_DEBUG_NO_HIZ);
}

static bool
radv_calibrated_timestamps_enabled(const struct radv_physical_device *pdev)
{
   return RADV_SUPPORT_CALIBRATED_TIMESTAMPS && !(pdev->info.family == CHIP_RAVEN || pdev->info.family == CHIP_RAVEN2);
}

static bool
radv_filter_minmax_enabled(const struct radv_physical_device *pdev)
{
   /* Tahiti and Verde only: reduction mode is unsupported due to a bug
    * (it might work sometimes, but that's not enough)
    */
   return !(pdev->info.family == CHIP_TAHITI || pdev->info.family == CHIP_VERDE);
}

bool
radv_enable_rt(const struct radv_physical_device *pdev, bool rt_pipelines)
{
   if (pdev->info.gfx_level < GFX10_3 && !radv_emulate_rt(pdev))
      return false;

   if (rt_pipelines && pdev->use_llvm)
      return false;

   return true;
}

bool
radv_emulate_rt(const struct radv_physical_device *pdev)
{
   const struct radv_instance *instance = radv_physical_device_instance(pdev);
   return instance->perftest_flags & RADV_PERFTEST_EMULATE_RT;
}

static bool
radv_is_conformant(const struct radv_physical_device *pdev)
{
   return pdev->info.gfx_level >= GFX8;
}

bool
radv_device_supports_etc(const struct radv_physical_device *pdev)
{
   return pdev->info.family == CHIP_VEGA10 || pdev->info.family == CHIP_RAVEN || pdev->info.family == CHIP_RAVEN2 ||
          pdev->info.family == CHIP_STONEY;
}

static void
parse_hex(char *out, const char *in, unsigned length)
{
   for (unsigned i = 0; i < length; ++i)
      out[i] = 0;

   for (unsigned i = 0; i < 2 * length; ++i) {
      unsigned v = in[i] <= '9' ? in[i] - '0' : (in[i] >= 'a' ? (in[i] - 'a' + 10) : (in[i] - 'A' + 10));
      out[i / 2] |= v << (4 * (1 - i % 2));
   }
}

static void
radv_physical_device_init_cache_key(struct radv_physical_device *pdev)
{
   const struct radv_instance *instance = radv_physical_device_instance(pdev);
   struct radv_physical_device_cache_key *key = &pdev->cache_key;

   key->family = pdev->info.family;
   key->ptr_size = sizeof(void *);
   key->conformant_trunc_coord = pdev->info.conformant_trunc_coord;

   key->clear_lds = instance->drirc.clear_lds;
   key->cs_wave32 = pdev->cs_wave_size == 32;
   key->disable_aniso_single_level = instance->drirc.disable_aniso_single_level && pdev->info.gfx_level < GFX8;
   key->disable_shrink_image_store = instance->drirc.disable_shrink_image_store;
   key->disable_sinking_load_input_fs = instance->drirc.disable_sinking_load_input_fs;
   key->dual_color_blend_by_location = instance->drirc.dual_color_blend_by_location;
   key->emulate_rt = !!(instance->perftest_flags & RADV_PERFTEST_EMULATE_RT);
   key->ge_wave32 = pdev->ge_wave_size == 32;
   key->invariant_geom = !!(instance->debug_flags & RADV_DEBUG_INVARIANT_GEOM);
   key->lower_discard_to_demote = !!(instance->debug_flags & RADV_DEBUG_DISCARD_TO_DEMOTE);
   key->mesh_fast_launch_2 = pdev->mesh_fast_launch_2;
   key->no_fmask = !!(instance->debug_flags & RADV_DEBUG_NO_FMASK);
   key->no_ngg_gs = !!(instance->debug_flags & RADV_DEBUG_NO_NGG_GS);
   key->no_rt = !!(instance->debug_flags & RADV_DEBUG_NO_RT);
   key->ps_wave32 = pdev->ps_wave_size == 32;
   key->rt_wave64 = pdev->rt_wave_size == 64;
   key->split_fma = !!(instance->debug_flags & RADV_DEBUG_SPLIT_FMA);
   key->ssbo_non_uniform = instance->drirc.ssbo_non_uniform;
   key->tex_non_uniform = instance->drirc.tex_non_uniform;
   key->use_llvm = pdev->use_llvm;
   key->use_ngg = pdev->use_ngg;
   key->use_ngg_culling = pdev->use_ngg_culling;
}

static int
radv_device_get_cache_uuid(struct radv_physical_device *pdev, void *uuid)
{
   struct mesa_sha1 ctx;
   unsigned char sha1[20];

   memset(uuid, 0, VK_UUID_SIZE);
   _mesa_sha1_init(&ctx);

#ifdef RADV_BUILD_ID_OVERRIDE
   {
      unsigned size = strlen(RADV_BUILD_ID_OVERRIDE) / 2;
      char *data = alloca(size);
      parse_hex(data, RADV_BUILD_ID_OVERRIDE, size);
      _mesa_sha1_update(&ctx, data, size);
   }
#else
   if (!disk_cache_get_function_identifier(radv_device_get_cache_uuid, &ctx))
      return -1;
#endif

#if LLVM_AVAILABLE
   if (pdev->use_llvm && !disk_cache_get_function_identifier(LLVMInitializeAMDGPUTargetInfo, &ctx))
      return -1;
#endif

   _mesa_sha1_update(&ctx, &pdev->cache_key, sizeof(pdev->cache_key));
   _mesa_sha1_final(&ctx, sha1);

   memcpy(uuid, sha1, VK_UUID_SIZE);
   return 0;
}

static void
radv_get_driver_uuid(void *uuid)
{
   ac_compute_driver_uuid(uuid, VK_UUID_SIZE);
}

static void
radv_get_device_uuid(const struct radeon_info *gpu_info, void *uuid)
{
   ac_compute_device_uuid(gpu_info, uuid, VK_UUID_SIZE);
}

static void
radv_physical_device_init_queue_table(struct radv_physical_device *pdev)
{
   const struct radv_instance *instance = radv_physical_device_instance(pdev);
   int idx = 0;
   pdev->vk_queue_to_radv[idx] = RADV_QUEUE_GENERAL;
   idx++;

   for (unsigned i = 1; i < RADV_MAX_QUEUE_FAMILIES; i++)
      pdev->vk_queue_to_radv[i] = RADV_MAX_QUEUE_FAMILIES + 1;

   if (pdev->info.ip[AMD_IP_COMPUTE].num_queues > 0 && !(instance->debug_flags & RADV_DEBUG_NO_COMPUTE_QUEUE)) {
      pdev->vk_queue_to_radv[idx] = RADV_QUEUE_COMPUTE;
      idx++;
   }

   if (instance->perftest_flags & RADV_PERFTEST_VIDEO_DECODE) {
      if (pdev->info.ip[pdev->vid_decode_ip].num_queues > 0) {
         pdev->vk_queue_to_radv[idx] = RADV_QUEUE_VIDEO_DEC;
         idx++;
      }
   }

   if (radv_transfer_queue_enabled(pdev)) {
      pdev->vk_queue_to_radv[idx] = RADV_QUEUE_TRANSFER;
      idx++;
   }

   if (pdev->video_encode_enabled) {
      if (pdev->info.ip[AMD_IP_VCN_ENC].num_queues > 0) {
         pdev->vk_queue_to_radv[idx] = RADV_QUEUE_VIDEO_ENC;
         idx++;
      }
   }

   if (radv_sparse_queue_enabled(pdev)) {
      pdev->vk_queue_to_radv[idx] = RADV_QUEUE_SPARSE;
      idx++;
   }

   pdev->num_queues = idx;
}

enum radv_heap {
   RADV_HEAP_VRAM = 1 << 0,
   RADV_HEAP_GTT = 1 << 1,
   RADV_HEAP_VRAM_VIS = 1 << 2,
   RADV_HEAP_MAX = 1 << 3,
};

static uint64_t
radv_get_adjusted_vram_size(struct radv_physical_device *pdev)
{
   const struct radv_instance *instance = radv_physical_device_instance(pdev);
   int ov = instance->drirc.override_vram_size;
   if (ov >= 0)
      return MIN2((uint64_t)pdev->info.vram_size_kb * 1024, (uint64_t)ov << 20);
   return (uint64_t)pdev->info.vram_size_kb * 1024;
}

static uint64_t
radv_get_visible_vram_size(struct radv_physical_device *pdev)
{
   return MIN2(radv_get_adjusted_vram_size(pdev), (uint64_t)pdev->info.vram_vis_size_kb * 1024);
}

static uint64_t
radv_get_vram_size(struct radv_physical_device *pdev)
{
   uint64_t total_size = radv_get_adjusted_vram_size(pdev);
   return total_size - MIN2(total_size, (uint64_t)pdev->info.vram_vis_size_kb * 1024);
}

static void
radv_physical_device_init_mem_types(struct radv_physical_device *pdev)
{
   const struct radv_instance *instance = radv_physical_device_instance(pdev);
   uint64_t visible_vram_size = radv_get_visible_vram_size(pdev);
   uint64_t vram_size = radv_get_vram_size(pdev);
   uint64_t gtt_size = (uint64_t)pdev->info.gart_size_kb * 1024;
   int vram_index = -1, visible_vram_index = -1, gart_index = -1;

   pdev->memory_properties.memoryHeapCount = 0;
   pdev->heaps = 0;

   if (!pdev->info.has_dedicated_vram) {
      const uint64_t total_size = gtt_size + visible_vram_size;

      if (instance->drirc.enable_unified_heap_on_apu) {
         /* Some applications seem better when the driver exposes only one heap of VRAM on APUs. */
         visible_vram_size = total_size;
         gtt_size = 0;
      } else {
         /* On APUs, the carveout is usually too small for games that request a minimum VRAM size
          * greater than it. To workaround this, we compute the total available memory size (GTT +
          * visible VRAM size) and report 2/3 as VRAM and 1/3 as GTT.
          */
         visible_vram_size = align64((total_size * 2) / 3, pdev->info.gart_page_size);
         gtt_size = total_size - visible_vram_size;
      }

      vram_size = 0;
   }

   /* Only get a VRAM heap if it is significant, not if it is a 16 MiB
    * remainder above visible VRAM. */
   if (vram_size > 0 && vram_size * 9 >= visible_vram_size) {
      vram_index = pdev->memory_properties.memoryHeapCount++;
      pdev->heaps |= RADV_HEAP_VRAM;
      pdev->memory_properties.memoryHeaps[vram_index] = (VkMemoryHeap){
         .size = vram_size,
         .flags = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT,
      };
   }

   if (gtt_size > 0) {
      gart_index = pdev->memory_properties.memoryHeapCount++;
      pdev->heaps |= RADV_HEAP_GTT;
      pdev->memory_properties.memoryHeaps[gart_index] = (VkMemoryHeap){
         .size = gtt_size,
         .flags = 0,
      };
   }

   if (visible_vram_size) {
      visible_vram_index = pdev->memory_properties.memoryHeapCount++;
      pdev->heaps |= RADV_HEAP_VRAM_VIS;
      pdev->memory_properties.memoryHeaps[visible_vram_index] = (VkMemoryHeap){
         .size = visible_vram_size,
         .flags = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT,
      };
   }

   unsigned type_count = 0;

   if (vram_index >= 0 || visible_vram_index >= 0) {
      pdev->memory_domains[type_count] = RADEON_DOMAIN_VRAM;
      pdev->memory_flags[type_count] = RADEON_FLAG_NO_CPU_ACCESS;
      pdev->memory_properties.memoryTypes[type_count++] = (VkMemoryType){
         .propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
         .heapIndex = vram_index >= 0 ? vram_index : visible_vram_index,
      };

      pdev->memory_domains[type_count] = RADEON_DOMAIN_VRAM;
      pdev->memory_flags[type_count] = RADEON_FLAG_NO_CPU_ACCESS | RADEON_FLAG_32BIT;
      pdev->memory_properties.memoryTypes[type_count++] = (VkMemoryType){
         .propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
         .heapIndex = vram_index >= 0 ? vram_index : visible_vram_index,
      };
   }

   if (gart_index >= 0) {
      pdev->memory_domains[type_count] = RADEON_DOMAIN_GTT;
      pdev->memory_flags[type_count] = RADEON_FLAG_GTT_WC | RADEON_FLAG_CPU_ACCESS;
      pdev->memory_properties.memoryTypes[type_count++] = (VkMemoryType){
         .propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
         .heapIndex = gart_index,
      };
   }
   if (visible_vram_index >= 0) {
      pdev->memory_domains[type_count] = RADEON_DOMAIN_VRAM;
      pdev->memory_flags[type_count] = RADEON_FLAG_CPU_ACCESS;
      pdev->memory_properties.memoryTypes[type_count++] = (VkMemoryType){
         .propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
         .heapIndex = visible_vram_index,
      };

      pdev->memory_domains[type_count] = RADEON_DOMAIN_VRAM;
      pdev->memory_flags[type_count] = RADEON_FLAG_CPU_ACCESS | RADEON_FLAG_32BIT;
      pdev->memory_properties.memoryTypes[type_count++] = (VkMemoryType){
         .propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
         .heapIndex = visible_vram_index,
      };
   }

   if (gart_index >= 0) {
      pdev->memory_domains[type_count] = RADEON_DOMAIN_GTT;
      pdev->memory_flags[type_count] = RADEON_FLAG_CPU_ACCESS;
      pdev->memory_properties.memoryTypes[type_count++] = (VkMemoryType){
         .propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                          VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
         .heapIndex = gart_index,
      };

      pdev->memory_domains[type_count] = RADEON_DOMAIN_GTT;
      pdev->memory_flags[type_count] = RADEON_FLAG_CPU_ACCESS | RADEON_FLAG_32BIT;
      pdev->memory_properties.memoryTypes[type_count++] = (VkMemoryType){
         .propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                          VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
         .heapIndex = gart_index,
      };
   }
   pdev->memory_properties.memoryTypeCount = type_count;

   if (pdev->info.has_l2_uncached) {
      for (int i = 0; i < pdev->memory_properties.memoryTypeCount; i++) {
         VkMemoryType mem_type = pdev->memory_properties.memoryTypes[i];

         if (((mem_type.propertyFlags & (VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) ||
              mem_type.propertyFlags == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) &&
             !(pdev->memory_flags[i] & RADEON_FLAG_32BIT)) {

            VkMemoryPropertyFlags property_flags = mem_type.propertyFlags | VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD |
                                                   VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD;

            pdev->memory_domains[type_count] = pdev->memory_domains[i];
            pdev->memory_flags[type_count] = pdev->memory_flags[i] | RADEON_FLAG_VA_UNCACHED;
            pdev->memory_properties.memoryTypes[type_count++] = (VkMemoryType){
               .propertyFlags = property_flags,
               .heapIndex = mem_type.heapIndex,
            };
         }
      }
      pdev->memory_properties.memoryTypeCount = type_count;
   }

   for (unsigned i = 0; i < type_count; ++i) {
      if (pdev->memory_flags[i] & RADEON_FLAG_32BIT)
         pdev->memory_types_32bit |= BITFIELD_BIT(i);
   }
}

uint32_t
radv_find_memory_index(const struct radv_physical_device *pdev, VkMemoryPropertyFlags flags)
{
   const VkPhysicalDeviceMemoryProperties *mem_properties = &pdev->memory_properties;
   for (uint32_t i = 0; i < mem_properties->memoryTypeCount; ++i) {
      if (mem_properties->memoryTypes[i].propertyFlags == flags) {
         return i;
      }
   }
   unreachable("invalid memory properties");
}

static void
radv_get_binning_settings(const struct radv_physical_device *pdev, struct radv_binning_settings *settings)
{
   if ((pdev->info.has_dedicated_vram && pdev->info.max_render_backends > 4) || pdev->info.gfx_level >= GFX10) {
      /* Using higher settings on GFX10+ can cause random GPU hangs. */
      settings->context_states_per_bin = 1;
      settings->persistent_states_per_bin = 1;
   } else {
      settings->context_states_per_bin = pdev->info.has_gfx9_scissor_bug ? 1 : 3;
      settings->persistent_states_per_bin = 1;
   }

   settings->fpovs_per_batch = 63;
}

static void
radv_physical_device_get_supported_extensions(const struct radv_physical_device *pdev,
                                              struct vk_device_extension_table *out_ext)
{
   const struct radv_instance *instance = radv_physical_device_instance(pdev);
   const struct vk_device_extension_table ext = {
      .KHR_8bit_storage = true,
      .KHR_16bit_storage = true,
      .KHR_acceleration_structure = radv_enable_rt(pdev, false),
      .KHR_calibrated_timestamps = radv_calibrated_timestamps_enabled(pdev),
      .KHR_cooperative_matrix = pdev->info.gfx_level >= GFX11 && !pdev->use_llvm,
      .KHR_bind_memory2 = true,
      .KHR_buffer_device_address = true,
      .KHR_copy_commands2 = true,
      .KHR_create_renderpass2 = true,
      .KHR_dedicated_allocation = true,
      .KHR_deferred_host_operations = true,
      .KHR_depth_stencil_resolve = true,
      .KHR_descriptor_update_template = true,
      .KHR_device_group = true,
      .KHR_draw_indirect_count = true,
      .KHR_driver_properties = true,
      .KHR_dynamic_rendering = true,
      .KHR_external_fence = true,
      .KHR_external_fence_fd = true,
      .KHR_external_memory = true,
      .KHR_external_memory_fd = true,
      .KHR_external_semaphore = true,
      .KHR_external_semaphore_fd = true,
      .KHR_format_feature_flags2 = true,
      .KHR_fragment_shader_barycentric = pdev->info.gfx_level >= GFX10_3,
      .KHR_fragment_shading_rate = pdev->info.gfx_level >= GFX10_3,
      .KHR_get_memory_requirements2 = true,
      .KHR_global_priority = true,
      .KHR_image_format_list = true,
      .KHR_imageless_framebuffer = true,
#ifdef RADV_USE_WSI_PLATFORM
      .KHR_incremental_present = true,
#endif
      .KHR_index_type_uint8 = pdev->info.gfx_level >= GFX8,
      .KHR_line_rasterization = true,
      .KHR_load_store_op_none = true,
      .KHR_maintenance1 = true,
      .KHR_maintenance2 = true,
      .KHR_maintenance3 = true,
      .KHR_maintenance4 = true,
      .KHR_maintenance5 = true,
      .KHR_maintenance6 = true,
      .KHR_map_memory2 = true,
      .KHR_multiview = true,
      .KHR_performance_query = radv_perf_query_supported(pdev),
      .KHR_pipeline_executable_properties = true,
      .KHR_pipeline_library = !pdev->use_llvm,
      /* Hide these behind dri configs for now since we cannot implement it reliably on
       * all surfaces yet. There is no surface capability query for present wait/id,
       * but the feature is useful enough to hide behind an opt-in mechanism for now.
       * If the instance only enables surface extensions that unconditionally support present wait,
       * we can also expose the extension that way. */
      .KHR_present_id =
         instance->drirc.enable_khr_present_wait || wsi_common_vk_instance_supports_present_wait(&instance->vk),
      .KHR_present_wait =
         instance->drirc.enable_khr_present_wait || wsi_common_vk_instance_supports_present_wait(&instance->vk),
      .KHR_push_descriptor = true,
      .KHR_ray_query = radv_enable_rt(pdev, false),
      .KHR_ray_tracing_maintenance1 = radv_enable_rt(pdev, false),
      .KHR_ray_tracing_pipeline = radv_enable_rt(pdev, true),
      .KHR_ray_tracing_position_fetch = radv_enable_rt(pdev, false),
      .KHR_relaxed_block_layout = true,
      .KHR_sampler_mirror_clamp_to_edge = true,
      .KHR_sampler_ycbcr_conversion = true,
      .KHR_separate_depth_stencil_layouts = true,
      .KHR_shader_atomic_int64 = true,
      .KHR_shader_clock = true,
      .KHR_shader_draw_parameters = true,
      .KHR_shader_expect_assume = true,
      .KHR_shader_float16_int8 = true,
      .KHR_shader_float_controls = true,
      .KHR_shader_integer_dot_product = true,
      .KHR_shader_maximal_reconvergence = true,
      .KHR_shader_non_semantic_info = true,
      .KHR_shader_quad_control = true,
      .KHR_shader_subgroup_extended_types = true,
      .KHR_shader_subgroup_rotate = true,
      .KHR_shader_subgroup_uniform_control_flow = true,
      .KHR_shader_terminate_invocation = true,
      .KHR_spirv_1_4 = true,
      .KHR_storage_buffer_storage_class = true,
#ifdef RADV_USE_WSI_PLATFORM
      .KHR_swapchain = true,
      .KHR_swapchain_mutable_format = true,
#endif
      .KHR_synchronization2 = true,
      .KHR_timeline_semaphore = true,
      .KHR_uniform_buffer_standard_layout = true,
      .KHR_variable_pointers = true,
      .KHR_vertex_attribute_divisor = true,
      .KHR_video_queue = !!(instance->perftest_flags & RADV_PERFTEST_VIDEO_DECODE) || pdev->video_encode_enabled,
      .KHR_video_decode_av1 = (pdev->info.vcn_ip_version >= VCN_3_0_0 && pdev->info.vcn_ip_version != VCN_3_0_33 &&
                               VIDEO_CODEC_AV1DEC && !!(instance->perftest_flags & RADV_PERFTEST_VIDEO_DECODE)),
      .KHR_video_decode_queue = !!(instance->perftest_flags & RADV_PERFTEST_VIDEO_DECODE),
      .KHR_video_decode_h264 = VIDEO_CODEC_H264DEC && !!(instance->perftest_flags & RADV_PERFTEST_VIDEO_DECODE),
      .KHR_video_decode_h265 = VIDEO_CODEC_H265DEC && !!(instance->perftest_flags & RADV_PERFTEST_VIDEO_DECODE),
      .KHR_video_encode_h264 = VIDEO_CODEC_H264ENC && pdev->video_encode_enabled,
      .KHR_video_encode_h265 = VIDEO_CODEC_H265ENC && pdev->video_encode_enabled,
      .KHR_video_encode_queue = pdev->video_encode_enabled,
      .KHR_vulkan_memory_model = true,
      .KHR_workgroup_memory_explicit_layout = true,
      .KHR_zero_initialize_workgroup_memory = true,
      .EXT_4444_formats = true,
      .EXT_attachment_feedback_loop_dynamic_state = true,
      .EXT_attachment_feedback_loop_layout = true,
      .EXT_border_color_swizzle = pdev->info.gfx_level >= GFX10,
      .EXT_buffer_device_address = true,
      .EXT_calibrated_timestamps = radv_calibrated_timestamps_enabled(pdev),
      .EXT_color_write_enable = true,
      .EXT_conditional_rendering = true,
      .EXT_conservative_rasterization = pdev->info.gfx_level >= GFX9,
      .EXT_custom_border_color = true,
      .EXT_debug_marker = instance->vk.trace_mode & RADV_TRACE_MODE_RGP,
      .EXT_depth_bias_control = true,
      .EXT_depth_clamp_zero_one = true,
      .EXT_depth_clip_control = true,
      .EXT_depth_clip_enable = true,
      .EXT_depth_range_unrestricted = true,
      .EXT_descriptor_buffer = true,
      .EXT_descriptor_indexing = true,
      .EXT_device_address_binding_report = true,
      .EXT_device_fault = pdev->info.has_gpuvm_fault_query,
      .EXT_discard_rectangles = true,
#ifdef VK_USE_PLATFORM_DISPLAY_KHR
      .EXT_display_control = true,
#endif
      .EXT_dynamic_rendering_unused_attachments = true,
      .EXT_extended_dynamic_state = true,
      .EXT_extended_dynamic_state2 = true,
      .EXT_extended_dynamic_state3 = true,
      .EXT_external_memory_acquire_unmodified = true,
      .EXT_external_memory_dma_buf = true,
      .EXT_external_memory_host = pdev->info.has_userptr,
      .EXT_fragment_shader_interlock = radv_has_pops(pdev),
      .EXT_global_priority = true,
      .EXT_global_priority_query = true,
      .EXT_graphics_pipeline_library = !pdev->use_llvm && !(instance->debug_flags & RADV_DEBUG_NO_GPL),
      .EXT_host_query_reset = true,
      .EXT_image_2d_view_of_3d = true,
      .EXT_image_compression_control = true,
      .EXT_image_drm_format_modifier = pdev->info.gfx_level >= GFX9,
      .EXT_image_robustness = true,
      .EXT_image_sliced_view_of_3d = pdev->info.gfx_level >= GFX10,
      .EXT_image_view_min_lod = true,
      .EXT_index_type_uint8 = pdev->info.gfx_level >= GFX8,
      .EXT_inline_uniform_block = true,
      .EXT_line_rasterization = true,
      .EXT_load_store_op_none = true,
      .EXT_map_memory_placed = true,
      .EXT_memory_budget = true,
      .EXT_memory_priority = true,
      .EXT_mesh_shader = radv_taskmesh_enabled(pdev),
      .EXT_multi_draw = true,
      .EXT_mutable_descriptor_type = true, /* Trivial promotion from VALVE. */
      .EXT_nested_command_buffer = true,
      .EXT_non_seamless_cube_map = true,
      .EXT_pci_bus_info = true,
#ifndef _WIN32
      .EXT_physical_device_drm = true,
#endif
      .EXT_pipeline_creation_cache_control = true,
      .EXT_pipeline_creation_feedback = true,
      .EXT_pipeline_library_group_handles = radv_enable_rt(pdev, true),
      .EXT_pipeline_robustness = !pdev->use_llvm,
      .EXT_post_depth_coverage = pdev->info.gfx_level >= GFX10,
      .EXT_primitive_topology_list_restart = true,
      .EXT_primitives_generated_query = true,
      .EXT_private_data = true,
      .EXT_provoking_vertex = true,
      .EXT_queue_family_foreign = true,
      .EXT_robustness2 = true,
      .EXT_sample_locations = pdev->info.gfx_level < GFX10,
      .EXT_sampler_filter_minmax = radv_filter_minmax_enabled(pdev),
      .EXT_scalar_block_layout = pdev->info.gfx_level >= GFX7,
      .EXT_separate_stencil_usage = true,
      .EXT_shader_atomic_float = true,
      .EXT_shader_atomic_float2 = true,
      .EXT_shader_demote_to_helper_invocation = true,
      .EXT_shader_image_atomic_int64 = true,
      .EXT_shader_module_identifier = true,
      .EXT_shader_object = !pdev->use_llvm && !(instance->debug_flags & RADV_DEBUG_NO_ESO),
      .EXT_shader_stencil_export = true,
      .EXT_shader_subgroup_ballot = true,
      .EXT_shader_subgroup_vote = true,
      .EXT_shader_viewport_index_layer = true,
      .EXT_subgroup_size_control = true,
#ifdef RADV_USE_WSI_PLATFORM
      .EXT_swapchain_maintenance1 = true,
#endif
      .EXT_texel_buffer_alignment = true,
      .EXT_tooling_info = true,
      .EXT_transform_feedback = true,
      .EXT_vertex_attribute_divisor = true,
      .EXT_vertex_input_dynamic_state = !pdev->use_llvm && !instance->drirc.enable_dgc,
      .EXT_ycbcr_image_arrays = true,
      .AMD_buffer_marker = true,
      .AMD_device_coherent_memory = true,
      .AMD_draw_indirect_count = true,
      .AMD_gcn_shader = true,
      .AMD_gpu_shader_half_float = pdev->info.has_packed_math_16bit,
      .AMD_gpu_shader_int16 = pdev->info.has_packed_math_16bit,
      .AMD_memory_overallocation_behavior = true,
      .AMD_mixed_attachment_samples = true,
      .AMD_rasterization_order = pdev->info.has_out_of_order_rast,
      .AMD_shader_ballot = true,
      .AMD_shader_core_properties = true,
      .AMD_shader_core_properties2 = true,
      .AMD_shader_early_and_late_fragment_tests = true,
      .AMD_shader_explicit_vertex_parameter = true,
      .AMD_shader_fragment_mask = pdev->use_fmask,
      .AMD_shader_image_load_store_lod = true,
      .AMD_shader_trinary_minmax = true,
      .AMD_texture_gather_bias_lod = pdev->info.gfx_level < GFX11,
#if DETECT_OS_ANDROID
      .ANDROID_external_memory_android_hardware_buffer = RADV_SUPPORT_ANDROID_HARDWARE_BUFFER,
      .ANDROID_native_buffer = true,
#endif
      .GOOGLE_decorate_string = true,
      .GOOGLE_hlsl_functionality1 = true,
      .GOOGLE_user_type = true,
      .INTEL_shader_integer_functions2 = true,
      .NV_compute_shader_derivatives = true,
      .NV_device_generated_commands = !pdev->use_llvm && instance->drirc.enable_dgc,
      .NV_device_generated_commands_compute = !pdev->use_llvm && instance->drirc.enable_dgc,
      /* Undocumented extension purely for vkd3d-proton. This check is to prevent anyone else from
       * using it.
       */
      .VALVE_descriptor_set_host_mapping =
         pdev->vk.instance->app_info.engine_name && strcmp(pdev->vk.instance->app_info.engine_name, "vkd3d") == 0,
      .VALVE_mutable_descriptor_type = true,
   };
   *out_ext = ext;
}

static void
radv_physical_device_get_features(const struct radv_physical_device *pdev, struct vk_features *features)
{
   const struct radv_instance *instance = radv_physical_device_instance(pdev);
   bool taskmesh_en = radv_taskmesh_enabled(pdev);
   bool has_perf_query = radv_perf_query_supported(pdev);
   bool has_shader_image_float_minmax = pdev->info.gfx_level != GFX8 && pdev->info.gfx_level != GFX9 &&
                                        pdev->info.gfx_level != GFX11 && pdev->info.gfx_level != GFX11_5;
   bool has_fragment_shader_interlock = radv_has_pops(pdev);

   *features = (struct vk_features){
      /* Vulkan 1.0 */
      .robustBufferAccess = true,
      .fullDrawIndexUint32 = true,
      .imageCubeArray = true,
      .independentBlend = true,
      .geometryShader = true,
      .tessellationShader = true,
      .sampleRateShading = true,
      .dualSrcBlend = true,
      .logicOp = true,
      .multiDrawIndirect = true,
      .drawIndirectFirstInstance = true,
      .depthClamp = true,
      .depthBiasClamp = true,
      .fillModeNonSolid = true,
      .depthBounds = true,
      .wideLines = true,
      .largePoints = true,
      .alphaToOne = true,
      .multiViewport = true,
      .samplerAnisotropy = true,
      .textureCompressionETC2 = radv_device_supports_etc(pdev) || pdev->emulate_etc2,
      .textureCompressionASTC_LDR = pdev->emulate_astc,
      .textureCompressionBC = true,
      .occlusionQueryPrecise = true,
      .pipelineStatisticsQuery = true,
      .vertexPipelineStoresAndAtomics = true,
      .fragmentStoresAndAtomics = true,
      .shaderTessellationAndGeometryPointSize = true,
      .shaderImageGatherExtended = true,
      .shaderStorageImageExtendedFormats = true,
      .shaderStorageImageMultisample = true,
      .shaderUniformBufferArrayDynamicIndexing = true,
      .shaderSampledImageArrayDynamicIndexing = true,
      .shaderStorageBufferArrayDynamicIndexing = true,
      .shaderStorageImageArrayDynamicIndexing = true,
      .shaderStorageImageReadWithoutFormat = true,
      .shaderStorageImageWriteWithoutFormat = true,
      .shaderClipDistance = true,
      .shaderCullDistance = true,
      .shaderFloat64 = true,
      .shaderInt64 = true,
      .shaderInt16 = true,
      .sparseBinding = true,
      .sparseResidencyBuffer = pdev->info.family >= CHIP_POLARIS10,
      .sparseResidencyImage2D = pdev->info.family >= CHIP_POLARIS10,
      .sparseResidencyImage3D = pdev->info.family >= CHIP_POLARIS10,
      .sparseResidencyAliased = pdev->info.family >= CHIP_POLARIS10,
      .variableMultisampleRate = true,
      .shaderResourceMinLod = true,
      .shaderResourceResidency = true,
      .inheritedQueries = true,

      /* Vulkan 1.1 */
      .storageBuffer16BitAccess = true,
      .uniformAndStorageBuffer16BitAccess = true,
      .storagePushConstant16 = true,
      .storageInputOutput16 = pdev->info.has_packed_math_16bit,
      .multiview = true,
      .multiviewGeometryShader = true,
      .multiviewTessellationShader = true,
      .variablePointersStorageBuffer = true,
      .variablePointers = true,
      .protectedMemory = false,
      .samplerYcbcrConversion = true,
      .shaderDrawParameters = true,

      /* Vulkan 1.2 */
      .samplerMirrorClampToEdge = true,
      .drawIndirectCount = true,
      .storageBuffer8BitAccess = true,
      .uniformAndStorageBuffer8BitAccess = true,
      .storagePushConstant8 = true,
      .shaderBufferInt64Atomics = true,
      .shaderSharedInt64Atomics = true,
      .shaderFloat16 = pdev->info.has_packed_math_16bit,
      .shaderInt8 = true,

      .descriptorIndexing = true,
      .shaderInputAttachmentArrayDynamicIndexing = true,
      .shaderUniformTexelBufferArrayDynamicIndexing = true,
      .shaderStorageTexelBufferArrayDynamicIndexing = true,
      .shaderUniformBufferArrayNonUniformIndexing = true,
      .shaderSampledImageArrayNonUniformIndexing = true,
      .shaderStorageBufferArrayNonUniformIndexing = true,
      .shaderStorageImageArrayNonUniformIndexing = true,
      .shaderInputAttachmentArrayNonUniformIndexing = true,
      .shaderUniformTexelBufferArrayNonUniformIndexing = true,
      .shaderStorageTexelBufferArrayNonUniformIndexing = true,
      .descriptorBindingUniformBufferUpdateAfterBind = true,
      .descriptorBindingSampledImageUpdateAfterBind = true,
      .descriptorBindingStorageImageUpdateAfterBind = true,
      .descriptorBindingStorageBufferUpdateAfterBind = true,
      .descriptorBindingUniformTexelBufferUpdateAfterBind = true,
      .descriptorBindingStorageTexelBufferUpdateAfterBind = true,
      .descriptorBindingUpdateUnusedWhilePending = true,
      .descriptorBindingPartiallyBound = true,
      .descriptorBindingVariableDescriptorCount = true,
      .runtimeDescriptorArray = true,

      .samplerFilterMinmax = true,
      .scalarBlockLayout = pdev->info.gfx_level >= GFX7,
      .imagelessFramebuffer = true,
      .uniformBufferStandardLayout = true,
      .shaderSubgroupExtendedTypes = true,
      .separateDepthStencilLayouts = true,
      .hostQueryReset = true,
      .timelineSemaphore = true,
      .bufferDeviceAddress = true,
      .bufferDeviceAddressCaptureReplay = true,
      .bufferDeviceAddressMultiDevice = false,
      .vulkanMemoryModel = true,
      .vulkanMemoryModelDeviceScope = true,
      .vulkanMemoryModelAvailabilityVisibilityChains = false,
      .shaderOutputViewportIndex = true,
      .shaderOutputLayer = true,
      .subgroupBroadcastDynamicId = true,

      /* Vulkan 1.3 */
      .robustImageAccess = true,
      .inlineUniformBlock = true,
      .descriptorBindingInlineUniformBlockUpdateAfterBind = true,
      .pipelineCreationCacheControl = true,
      .privateData = true,
      .shaderDemoteToHelperInvocation = true,
      .shaderTerminateInvocation = true,
      .subgroupSizeControl = true,
      .computeFullSubgroups = true,
      .synchronization2 = true,
      .textureCompressionASTC_HDR = false,
      .shaderZeroInitializeWorkgroupMemory = true,
      .dynamicRendering = true,
      .shaderIntegerDotProduct = true,
      .maintenance4 = true,

      /* VK_EXT_conditional_rendering */
      .conditionalRendering = true,
      .inheritedConditionalRendering = false,

      /* VK_KHR_vertex_attribute_divisor */
      .vertexAttributeInstanceRateDivisor = true,
      .vertexAttributeInstanceRateZeroDivisor = true,

      /* VK_EXT_transform_feedback */
      .transformFeedback = true,
      .geometryStreams = true,

      /* VK_EXT_memory_priority */
      .memoryPriority = true,

      /* VK_EXT_depth_clip_enable */
      .depthClipEnable = true,

      /* VK_NV_compute_shader_derivatives */
      .computeDerivativeGroupQuads = false,
      .computeDerivativeGroupLinear = true,

      /* VK_EXT_ycbcr_image_arrays */
      .ycbcrImageArrays = true,

      /* VK_KHR_index_type_uint8 */
      .indexTypeUint8 = pdev->info.gfx_level >= GFX8,

      /* VK_KHR_pipeline_executable_properties */
      .pipelineExecutableInfo = true,

      /* VK_KHR_shader_clock */
      .shaderSubgroupClock = true,
      .shaderDeviceClock = pdev->info.gfx_level >= GFX8,

      /* VK_EXT_texel_buffer_alignment */
      .texelBufferAlignment = true,

      /* VK_AMD_device_coherent_memory */
      .deviceCoherentMemory = pdev->info.has_l2_uncached,

      /* VK_KHR_line_rasterization */
      .rectangularLines = true,
      .bresenhamLines = true,
      .smoothLines = true,
      .stippledRectangularLines = false,
      .stippledBresenhamLines = true,
      .stippledSmoothLines = false,

      /* VK_EXT_robustness2 */
      .robustBufferAccess2 = true,
      .robustImageAccess2 = true,
      .nullDescriptor = true,

      /* VK_EXT_custom_border_color */
      .customBorderColors = true,
      .customBorderColorWithoutFormat = true,

      /* VK_EXT_extended_dynamic_state */
      .extendedDynamicState = true,

      /* VK_EXT_shader_atomic_float */
      .shaderBufferFloat32Atomics = true,
      .shaderBufferFloat32AtomicAdd = pdev->info.gfx_level >= GFX11,
      .shaderBufferFloat64Atomics = true,
      .shaderBufferFloat64AtomicAdd = false,
      .shaderSharedFloat32Atomics = true,
      .shaderSharedFloat32AtomicAdd = pdev->info.gfx_level >= GFX8,
      .shaderSharedFloat64Atomics = true,
      .shaderSharedFloat64AtomicAdd = false,
      .shaderImageFloat32Atomics = true,
      .shaderImageFloat32AtomicAdd = false,
      .sparseImageFloat32Atomics = true,
      .sparseImageFloat32AtomicAdd = false,

      /* VK_EXT_4444_formats */
      .formatA4R4G4B4 = true,
      .formatA4B4G4R4 = true,

      /* VK_EXT_shader_image_atomic_int64 */
      .shaderImageInt64Atomics = true,
      .sparseImageInt64Atomics = true,

      /* VK_EXT_mutable_descriptor_type */
      .mutableDescriptorType = true,

      /* VK_KHR_fragment_shading_rate */
      .pipelineFragmentShadingRate = true,
      .primitiveFragmentShadingRate = true,
      .attachmentFragmentShadingRate = radv_vrs_attachment_enabled(pdev),

      /* VK_KHR_workgroup_memory_explicit_layout */
      .workgroupMemoryExplicitLayout = true,
      .workgroupMemoryExplicitLayoutScalarBlockLayout = true,
      .workgroupMemoryExplicitLayout8BitAccess = true,
      .workgroupMemoryExplicitLayout16BitAccess = true,

      /* VK_EXT_provoking_vertex */
      .provokingVertexLast = true,
      .transformFeedbackPreservesProvokingVertex = true,

      /* VK_EXT_extended_dynamic_state2 */
      .extendedDynamicState2 = true,
      .extendedDynamicState2LogicOp = true,
      .extendedDynamicState2PatchControlPoints = true,

      /* VK_EXT_global_priority_query */
      .globalPriorityQuery = true,

      /* VK_KHR_acceleration_structure */
      .accelerationStructure = true,
      .accelerationStructureCaptureReplay = true,
      .accelerationStructureIndirectBuild = false,
      .accelerationStructureHostCommands = false,
      .descriptorBindingAccelerationStructureUpdateAfterBind = true,

      /* VK_EXT_buffer_device_address */
      .bufferDeviceAddressCaptureReplayEXT = true,

      /* VK_KHR_shader_subgroup_uniform_control_flow */
      .shaderSubgroupUniformControlFlow = true,

      /* VK_EXT_map_memory_placed */
      .memoryMapPlaced = true,
      .memoryMapRangePlaced = false,
      .memoryUnmapReserve = true,

      /* VK_EXT_multi_draw */
      .multiDraw = true,

      /* VK_EXT_color_write_enable */
      .colorWriteEnable = true,

      /* VK_EXT_shader_atomic_float2 */
      .shaderBufferFloat16Atomics = false,
      .shaderBufferFloat16AtomicAdd = false,
      .shaderBufferFloat16AtomicMinMax = false,
      .shaderBufferFloat32AtomicMinMax = radv_has_shader_buffer_float_minmax(pdev, 32),
      .shaderBufferFloat64AtomicMinMax = radv_has_shader_buffer_float_minmax(pdev, 64),
      .shaderSharedFloat16Atomics = false,
      .shaderSharedFloat16AtomicAdd = false,
      .shaderSharedFloat16AtomicMinMax = false,
      .shaderSharedFloat32AtomicMinMax = true,
      .shaderSharedFloat64AtomicMinMax = true,
      .shaderImageFloat32AtomicMinMax = has_shader_image_float_minmax,
      .sparseImageFloat32AtomicMinMax = has_shader_image_float_minmax,

      /* VK_KHR_present_id */
      .presentId = pdev->vk.supported_extensions.KHR_present_id,

      /* VK_KHR_present_wait */
      .presentWait = pdev->vk.supported_extensions.KHR_present_wait,

      /* VK_EXT_primitive_topology_list_restart */
      .primitiveTopologyListRestart = true,
      .primitiveTopologyPatchListRestart = false,

      /* VK_KHR_ray_query */
      .rayQuery = true,

      /* VK_EXT_pipeline_library_group_handles */
      .pipelineLibraryGroupHandles = true,

      /* VK_KHR_ray_tracing_pipeline */
      .rayTracingPipeline = true,
      .rayTracingPipelineShaderGroupHandleCaptureReplay = true,
      .rayTracingPipelineShaderGroupHandleCaptureReplayMixed = false,
      .rayTracingPipelineTraceRaysIndirect = true,
      .rayTraversalPrimitiveCulling = true,

      /* VK_KHR_ray_tracing_maintenance1 */
      .rayTracingMaintenance1 = true,
      .rayTracingPipelineTraceRaysIndirect2 = radv_enable_rt(pdev, true),

      /* VK_KHR_ray_tracing_position_fetch */
      .rayTracingPositionFetch = true,

      /* VK_EXT_vertex_input_dynamic_state */
      .vertexInputDynamicState = true,

      /* VK_EXT_image_view_min_lod */
      .minLod = true,

      /* VK_EXT_mesh_shader */
      .meshShader = taskmesh_en,
      .taskShader = taskmesh_en,
      .multiviewMeshShader = taskmesh_en,
      .primitiveFragmentShadingRateMeshShader = taskmesh_en,
      .meshShaderQueries = false,

      /* VK_VALVE_descriptor_set_host_mapping */
      .descriptorSetHostMapping = true,

      /* VK_EXT_depth_clip_control */
      .depthClipControl = true,

      /* VK_EXT_image_2d_view_of_3d  */
      .image2DViewOf3D = true,
      .sampler2DViewOf3D = false,

      /* VK_INTEL_shader_integer_functions2 */
      .shaderIntegerFunctions2 = true,

      /* VK_EXT_primitives_generated_query */
      .primitivesGeneratedQuery = true,
      .primitivesGeneratedQueryWithRasterizerDiscard = true,
      .primitivesGeneratedQueryWithNonZeroStreams = true,

      /* VK_EXT_non_seamless_cube_map */
      .nonSeamlessCubeMap = true,

      /* VK_EXT_border_color_swizzle */
      .borderColorSwizzle = true,
      .borderColorSwizzleFromImage = true,

      /* VK_EXT_shader_module_identifier */
      .shaderModuleIdentifier = true,

      /* VK_KHR_performance_query */
      .performanceCounterQueryPools = has_perf_query,
      .performanceCounterMultipleQueryPools = has_perf_query,

      /* VK_NV_device_generated_commands */
      .deviceGeneratedCommands = true,

      /* VK_EXT_attachment_feedback_loop_layout */
      .attachmentFeedbackLoopLayout = true,

      /* VK_EXT_graphics_pipeline_library */
      .graphicsPipelineLibrary = true,

      /* VK_EXT_extended_dynamic_state3 */
      .extendedDynamicState3TessellationDomainOrigin = true,
      .extendedDynamicState3PolygonMode = true,
      .extendedDynamicState3SampleMask = true,
      .extendedDynamicState3AlphaToCoverageEnable = !pdev->use_llvm,
      .extendedDynamicState3LogicOpEnable = true,
      .extendedDynamicState3LineStippleEnable = true,
      .extendedDynamicState3ColorBlendEnable = !pdev->use_llvm,
      .extendedDynamicState3DepthClipEnable = true,
      .extendedDynamicState3ConservativeRasterizationMode = pdev->info.gfx_level >= GFX9,
      .extendedDynamicState3DepthClipNegativeOneToOne = true,
      .extendedDynamicState3ProvokingVertexMode = true,
      .extendedDynamicState3DepthClampEnable = true,
      .extendedDynamicState3ColorWriteMask = !pdev->use_llvm,
      .extendedDynamicState3RasterizationSamples = true,
      .extendedDynamicState3ColorBlendEquation = !pdev->use_llvm,
      .extendedDynamicState3SampleLocationsEnable = pdev->info.gfx_level < GFX10,
      .extendedDynamicState3LineRasterizationMode = true,
      .extendedDynamicState3ExtraPrimitiveOverestimationSize = false,
      .extendedDynamicState3AlphaToOneEnable = !pdev->use_llvm,
      .extendedDynamicState3RasterizationStream = false,
      .extendedDynamicState3ColorBlendAdvanced = false,
      .extendedDynamicState3ViewportWScalingEnable = false,
      .extendedDynamicState3ViewportSwizzle = false,
      .extendedDynamicState3CoverageToColorEnable = false,
      .extendedDynamicState3CoverageToColorLocation = false,
      .extendedDynamicState3CoverageModulationMode = false,
      .extendedDynamicState3CoverageModulationTableEnable = false,
      .extendedDynamicState3CoverageModulationTable = false,
      .extendedDynamicState3CoverageReductionMode = false,
      .extendedDynamicState3RepresentativeFragmentTestEnable = false,
      .extendedDynamicState3ShadingRateImageEnable = false,

      /* VK_EXT_descriptor_buffer */
      .descriptorBuffer = true,
      .descriptorBufferCaptureReplay = false,
      .descriptorBufferImageLayoutIgnored = true,
      .descriptorBufferPushDescriptors = true,

      /* VK_AMD_shader_early_and_late_fragment_tests */
      .shaderEarlyAndLateFragmentTests = true,

      /* VK_EXT_image_sliced_view_of_3d */
      .imageSlicedViewOf3D = true,

#ifdef RADV_USE_WSI_PLATFORM
      /* VK_EXT_swapchain_maintenance1 */
      .swapchainMaintenance1 = true,
#endif

      /* VK_EXT_attachment_feedback_loop_dynamic_state */
      .attachmentFeedbackLoopDynamicState = true,

      /* VK_EXT_dynamic_rendering_unused_attachments */
      .dynamicRenderingUnusedAttachments = true,

      /* VK_KHR_fragment_shader_barycentric */
      .fragmentShaderBarycentric = true,

      /* VK_EXT_depth_bias_control */
      .depthBiasControl = true,
      .leastRepresentableValueForceUnormRepresentation = true,
      .floatRepresentation = true,
      .depthBiasExact = true,

      /* VK_EXT_fragment_shader_interlock */
      .fragmentShaderSampleInterlock = has_fragment_shader_interlock,
      .fragmentShaderPixelInterlock = has_fragment_shader_interlock,
      .fragmentShaderShadingRateInterlock = false,

      /* VK_EXT_pipeline_robustness */
      .pipelineRobustness = true,

      /* VK_KHR_maintenance5 */
      .maintenance5 = true,

      /* VK_NV_device_generated_commands_compute */
      .deviceGeneratedCompute = true,
      .deviceGeneratedComputePipelines = true,
      .deviceGeneratedComputeCaptureReplay = false,

      /* VK_KHR_cooperative_matrix */
      .cooperativeMatrix = pdev->info.gfx_level >= GFX11 && !pdev->use_llvm,
      .cooperativeMatrixRobustBufferAccess = pdev->info.gfx_level >= GFX11 && !pdev->use_llvm,

      /* VK_EXT_image_compression_control */
      .imageCompressionControl = true,

      /* VK_EXT_device_fault */
      .deviceFault = true,
      .deviceFaultVendorBinary = instance->debug_flags & RADV_DEBUG_HANG,

      /* VK_EXT_depth_clamp_zero_one */
      .depthClampZeroOne = true,

      /* VK_KHR_maintenance6 */
      .maintenance6 = true,

      /* VK_KHR_shader_subgroup_rotate */
      .shaderSubgroupRotate = true,
      .shaderSubgroupRotateClustered = true,

      /* VK_EXT_shader_object */
      .shaderObject = true,

      /* VK_KHR_shader_expect_assume */
      .shaderExpectAssume = true,

      /* VK_KHR_shader_maximal_reconvergence */
      .shaderMaximalReconvergence = true,

      /* VK_KHR_shader_quad_control */
      .shaderQuadControl = true,

      /* VK_EXT_address_binding_report */
      .reportAddressBinding = true,

      /* VK_EXT_nested_command_buffer */
      .nestedCommandBuffer = true,
      .nestedCommandBufferRendering = true,
      .nestedCommandBufferSimultaneousUse = true,
   };
}

static size_t
radv_max_descriptor_set_size()
{
   /* make sure that the entire descriptor set is addressable with a signed
    * 32-bit int. So the sum of all limits scaled by descriptor size has to
    * be at most 2 GiB. the combined image & samples object count as one of
    * both. This limit is for the pipeline layout, not for the set layout, but
    * there is no set limit, so we just set a pipeline limit. I don't think
    * any app is going to hit this soon. */
   return ((1ull << 31) - 16 * MAX_DYNAMIC_BUFFERS - MAX_INLINE_UNIFORM_BLOCK_SIZE * MAX_INLINE_UNIFORM_BLOCK_COUNT) /
          (32 /* uniform buffer, 32 due to potential space wasted on alignment */ +
           32 /* storage buffer, 32 due to potential space wasted on alignment */ +
           32 /* sampler, largest when combined with image */ + 64 /* sampled image */ + 64 /* storage image */);
}

static uint32_t
radv_uniform_buffer_offset_alignment(const struct radv_physical_device *pdev)
{
   const struct radv_instance *instance = radv_physical_device_instance(pdev);
   uint32_t uniform_offset_alignment = instance->drirc.override_uniform_offset_alignment;
   if (!util_is_power_of_two_or_zero(uniform_offset_alignment)) {
      fprintf(stderr,
              "ERROR: invalid radv_override_uniform_offset_alignment setting %d:"
              "not a power of two\n",
              uniform_offset_alignment);
      uniform_offset_alignment = 0;
   }

   /* Take at least the hardware limit. */
   return MAX2(uniform_offset_alignment, 4);
}

static const char *
radv_get_compiler_string(struct radv_physical_device *pdev)
{
   const struct radv_instance *instance = radv_physical_device_instance(pdev);

   if (!pdev->use_llvm) {
      /* Some games like SotTR apply shader workarounds if the LLVM
       * version is too old or if the LLVM version string is
       * missing. This gives 2-5% performance with SotTR and ACO.
       */
      if (instance->drirc.report_llvm9_version_string) {
         return " (LLVM 9.0.1)";
      }

      return "";
   }

#if LLVM_AVAILABLE
   return " (LLVM " MESA_LLVM_VERSION_STRING ")";
#else
   unreachable("LLVM is not available");
#endif
}

static void
radv_get_physical_device_properties(struct radv_physical_device *pdev)
{
   VkSampleCountFlags sample_counts = 0xf;

   size_t max_descriptor_set_size = radv_max_descriptor_set_size();

   VkPhysicalDeviceType device_type;
   if (pdev->info.has_dedicated_vram) {
      device_type = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
   } else {
      device_type = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
   }

   pdev->vk.properties = (struct vk_properties){
      .apiVersion = RADV_API_VERSION,
      .driverVersion = vk_get_driver_version(),
      .vendorID = ATI_VENDOR_ID,
      .deviceID = pdev->info.pci_id,
      .deviceType = device_type,
      .maxImageDimension1D = (1 << 14),
      .maxImageDimension2D = (1 << 14),
      .maxImageDimension3D = (1 << 11),
      .maxImageDimensionCube = (1 << 14),
      .maxImageArrayLayers = (1 << 11),
      .maxTexelBufferElements = UINT32_MAX,
      .maxUniformBufferRange = UINT32_MAX,
      .maxStorageBufferRange = UINT32_MAX,
      .maxPushConstantsSize = MAX_PUSH_CONSTANTS_SIZE,
      .maxMemoryAllocationCount = UINT32_MAX,
      .maxSamplerAllocationCount = 64 * 1024,
      .bufferImageGranularity = 1,
      .sparseAddressSpaceSize = RADV_MAX_MEMORY_ALLOCATION_SIZE, /* buffer max size */
      .maxBoundDescriptorSets = MAX_SETS,
      .maxPerStageDescriptorSamplers = max_descriptor_set_size,
      .maxPerStageDescriptorUniformBuffers = max_descriptor_set_size,
      .maxPerStageDescriptorStorageBuffers = max_descriptor_set_size,
      .maxPerStageDescriptorSampledImages = max_descriptor_set_size,
      .maxPerStageDescriptorStorageImages = max_descriptor_set_size,
      .maxPerStageDescriptorInputAttachments = max_descriptor_set_size,
      .maxPerStageResources = max_descriptor_set_size,
      .maxDescriptorSetSamplers = max_descriptor_set_size,
      .maxDescriptorSetUniformBuffers = max_descriptor_set_size,
      .maxDescriptorSetUniformBuffersDynamic = MAX_DYNAMIC_UNIFORM_BUFFERS,
      .maxDescriptorSetStorageBuffers = max_descriptor_set_size,
      .maxDescriptorSetStorageBuffersDynamic = MAX_DYNAMIC_STORAGE_BUFFERS,
      .maxDescriptorSetSampledImages = max_descriptor_set_size,
      .maxDescriptorSetStorageImages = max_descriptor_set_size,
      .maxDescriptorSetInputAttachments = max_descriptor_set_size,
      .maxVertexInputAttributes = MAX_VERTEX_ATTRIBS,
      .maxVertexInputBindings = MAX_VBS,
      .maxVertexInputAttributeOffset = UINT32_MAX,
      .maxVertexInputBindingStride = 2048,
      .maxVertexOutputComponents = 128,
      .maxTessellationGenerationLevel = 64,
      .maxTessellationPatchSize = 32,
      .maxTessellationControlPerVertexInputComponents = 128,
      .maxTessellationControlPerVertexOutputComponents = 128,
      .maxTessellationControlPerPatchOutputComponents = 120,
      .maxTessellationControlTotalOutputComponents = 4096,
      .maxTessellationEvaluationInputComponents = 128,
      .maxTessellationEvaluationOutputComponents = 128,
      .maxGeometryShaderInvocations = 127,
      .maxGeometryInputComponents = 64,
      .maxGeometryOutputComponents = 128,
      .maxGeometryOutputVertices = 256,
      .maxGeometryTotalOutputComponents = 1024,
      .maxFragmentInputComponents = 128,
      .maxFragmentOutputAttachments = 8,
      .maxFragmentDualSrcAttachments = 1,
      .maxFragmentCombinedOutputResources = max_descriptor_set_size,
      .maxComputeSharedMemorySize = pdev->max_shared_size,
      .maxComputeWorkGroupCount = {65535, 65535, 65535},
      .maxComputeWorkGroupInvocations = 1024,
      .maxComputeWorkGroupSize = {1024, 1024, 1024},
      .subPixelPrecisionBits = 8,
      .subTexelPrecisionBits = 8,
      .mipmapPrecisionBits = 8,
      .maxDrawIndexedIndexValue = UINT32_MAX,
      .maxDrawIndirectCount = UINT32_MAX,
      .maxSamplerLodBias = 16,
      .maxSamplerAnisotropy = 16,
      .maxViewports = MAX_VIEWPORTS,
      .maxViewportDimensions = {(1 << 14), (1 << 14)},
      .viewportBoundsRange = {INT16_MIN, INT16_MAX},
      .viewportSubPixelBits = 8,
      .minMemoryMapAlignment = 4096, /* A page */
      .minTexelBufferOffsetAlignment = 4,
      .minUniformBufferOffsetAlignment = radv_uniform_buffer_offset_alignment(pdev),
      .minStorageBufferOffsetAlignment = 4,
      .minTexelOffset = -32,
      .maxTexelOffset = 31,
      .minTexelGatherOffset = -32,
      .maxTexelGatherOffset = 31,
      .minInterpolationOffset = -2,
      .maxInterpolationOffset = 2,
      .subPixelInterpolationOffsetBits = 8,
      .maxFramebufferWidth = MAX_FRAMEBUFFER_WIDTH,
      .maxFramebufferHeight = MAX_FRAMEBUFFER_HEIGHT,
      .maxFramebufferLayers = (1 << 10),
      .framebufferColorSampleCounts = sample_counts,
      .framebufferDepthSampleCounts = sample_counts,
      .framebufferStencilSampleCounts = sample_counts,
      .framebufferNoAttachmentsSampleCounts = sample_counts,
      .maxColorAttachments = MAX_RTS,
      .sampledImageColorSampleCounts = sample_counts,
      .sampledImageIntegerSampleCounts = sample_counts,
      .sampledImageDepthSampleCounts = sample_counts,
      .sampledImageStencilSampleCounts = sample_counts,
      .storageImageSampleCounts = sample_counts,
      .maxSampleMaskWords = 1,
      .timestampComputeAndGraphics = true,
      .timestampPeriod = 1000000.0 / pdev->info.clock_crystal_freq,
      .maxClipDistances = 8,
      .maxCullDistances = 8,
      .maxCombinedClipAndCullDistances = 8,
      .discreteQueuePriorities = 2,
      .pointSizeRange = {0.0, 8191.875},
      .lineWidthRange = {0.0, 8.0},
      .pointSizeGranularity = (1.0 / 8.0),
      .lineWidthGranularity = (1.0 / 8.0),
      .strictLines = false, /* FINISHME */
      .standardSampleLocations = true,
      .optimalBufferCopyOffsetAlignment = 1,
      .optimalBufferCopyRowPitchAlignment = 1,
      .nonCoherentAtomSize = 64,
      .sparseResidencyNonResidentStrict = pdev->info.family >= CHIP_POLARIS10,
      .sparseResidencyStandard2DBlockShape = pdev->info.family >= CHIP_POLARIS10,
      .sparseResidencyStandard3DBlockShape = pdev->info.gfx_level >= GFX9,
   };

   struct vk_properties *p = &pdev->vk.properties;

   /* Vulkan 1.1 */
   strcpy(p->deviceName, pdev->marketing_name);
   memcpy(p->pipelineCacheUUID, pdev->cache_uuid, VK_UUID_SIZE);

   memcpy(p->deviceUUID, pdev->device_uuid, VK_UUID_SIZE);
   memcpy(p->driverUUID, pdev->driver_uuid, VK_UUID_SIZE);
   memset(p->deviceLUID, 0, VK_LUID_SIZE);
   /* The LUID is for Windows. */
   p->deviceLUIDValid = false;
   p->deviceNodeMask = 0;

   p->subgroupSize = RADV_SUBGROUP_SIZE;
   p->subgroupSupportedStages = VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT;
   if (radv_taskmesh_enabled(pdev))
      p->subgroupSupportedStages |= VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT;

   if (radv_enable_rt(pdev, true))
      p->subgroupSupportedStages |= RADV_RT_STAGE_BITS;
   p->subgroupSupportedOperations = VK_SUBGROUP_FEATURE_BASIC_BIT | VK_SUBGROUP_FEATURE_VOTE_BIT |
                                    VK_SUBGROUP_FEATURE_ARITHMETIC_BIT | VK_SUBGROUP_FEATURE_BALLOT_BIT |
                                    VK_SUBGROUP_FEATURE_CLUSTERED_BIT | VK_SUBGROUP_FEATURE_QUAD_BIT |
                                    VK_SUBGROUP_FEATURE_SHUFFLE_BIT | VK_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT |
                                    VK_SUBGROUP_FEATURE_ROTATE_BIT_KHR | VK_SUBGROUP_FEATURE_ROTATE_CLUSTERED_BIT_KHR;
   p->subgroupQuadOperationsInAllStages = true;

   p->pointClippingBehavior = VK_POINT_CLIPPING_BEHAVIOR_ALL_CLIP_PLANES;
   p->maxMultiviewViewCount = MAX_VIEWS;
   p->maxMultiviewInstanceIndex = INT_MAX;
   p->protectedNoFault = false;
   p->maxPerSetDescriptors = RADV_MAX_PER_SET_DESCRIPTORS;
   p->maxMemoryAllocationSize = RADV_MAX_MEMORY_ALLOCATION_SIZE;

   /* Vulkan 1.2 */
   p->driverID = VK_DRIVER_ID_MESA_RADV;
   snprintf(p->driverName, VK_MAX_DRIVER_NAME_SIZE, "radv");
   snprintf(p->driverInfo, VK_MAX_DRIVER_INFO_SIZE, "Mesa " PACKAGE_VERSION MESA_GIT_SHA1 "%s",
            radv_get_compiler_string(pdev));

   if (radv_is_conformant(pdev)) {
      if (pdev->info.gfx_level >= GFX10_3) {
         p->conformanceVersion = (VkConformanceVersion){
            .major = 1,
            .minor = 3,
            .subminor = 0,
            .patch = 0,
         };
      } else {
         p->conformanceVersion = (VkConformanceVersion){
            .major = 1,
            .minor = 2,
            .subminor = 7,
            .patch = 1,
         };
      }
   } else {
      p->conformanceVersion = (VkConformanceVersion){
         .major = 0,
         .minor = 0,
         .subminor = 0,
         .patch = 0,
      };
   }

   /* On AMD hardware, denormals and rounding modes for fp16/fp64 are
    * controlled by the same config register.
    */
   if (pdev->info.has_packed_math_16bit) {
      p->denormBehaviorIndependence = VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_32_BIT_ONLY;
      p->roundingModeIndependence = VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_32_BIT_ONLY;
   } else {
      p->denormBehaviorIndependence = VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_ALL;
      p->roundingModeIndependence = VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_ALL;
   }

   /* With LLVM, do not allow both preserving and flushing denorms because
    * different shaders in the same pipeline can have different settings and
    * this won't work for merged shaders. To make it work, this requires LLVM
    * support for changing the register. The same logic applies for the
    * rounding modes because they are configured with the same config
    * register.
    */
   p->shaderDenormFlushToZeroFloat32 = true;
   p->shaderDenormPreserveFloat32 = !pdev->use_llvm;
   p->shaderRoundingModeRTEFloat32 = true;
   p->shaderRoundingModeRTZFloat32 = !pdev->use_llvm;
   p->shaderSignedZeroInfNanPreserveFloat32 = true;

   p->shaderDenormFlushToZeroFloat16 = pdev->info.has_packed_math_16bit && !pdev->use_llvm;
   p->shaderDenormPreserveFloat16 = pdev->info.has_packed_math_16bit;
   p->shaderRoundingModeRTEFloat16 = pdev->info.has_packed_math_16bit;
   p->shaderRoundingModeRTZFloat16 = pdev->info.has_packed_math_16bit && !pdev->use_llvm;
   p->shaderSignedZeroInfNanPreserveFloat16 = pdev->info.has_packed_math_16bit;

   p->shaderDenormFlushToZeroFloat64 = pdev->info.gfx_level >= GFX8 && !pdev->use_llvm;
   p->shaderDenormPreserveFloat64 = pdev->info.gfx_level >= GFX8;
   p->shaderRoundingModeRTEFloat64 = pdev->info.gfx_level >= GFX8;
   p->shaderRoundingModeRTZFloat64 = pdev->info.gfx_level >= GFX8 && !pdev->use_llvm;
   p->shaderSignedZeroInfNanPreserveFloat64 = pdev->info.gfx_level >= GFX8;

   p->maxUpdateAfterBindDescriptorsInAllPools = UINT32_MAX / 64;
   p->shaderUniformBufferArrayNonUniformIndexingNative = false;
   p->shaderSampledImageArrayNonUniformIndexingNative = false;
   p->shaderStorageBufferArrayNonUniformIndexingNative = false;
   p->shaderStorageImageArrayNonUniformIndexingNative = false;
   p->shaderInputAttachmentArrayNonUniformIndexingNative = false;
   p->robustBufferAccessUpdateAfterBind = true;
   p->quadDivergentImplicitLod = false;

   p->maxPerStageDescriptorUpdateAfterBindSamplers = max_descriptor_set_size;
   p->maxPerStageDescriptorUpdateAfterBindUniformBuffers = max_descriptor_set_size;
   p->maxPerStageDescriptorUpdateAfterBindStorageBuffers = max_descriptor_set_size;
   p->maxPerStageDescriptorUpdateAfterBindSampledImages = max_descriptor_set_size;
   p->maxPerStageDescriptorUpdateAfterBindStorageImages = max_descriptor_set_size;
   p->maxPerStageDescriptorUpdateAfterBindInputAttachments = max_descriptor_set_size;
   p->maxPerStageUpdateAfterBindResources = max_descriptor_set_size;
   p->maxDescriptorSetUpdateAfterBindSamplers = max_descriptor_set_size;
   p->maxDescriptorSetUpdateAfterBindUniformBuffers = max_descriptor_set_size;
   p->maxDescriptorSetUpdateAfterBindUniformBuffersDynamic = MAX_DYNAMIC_UNIFORM_BUFFERS;
   p->maxDescriptorSetUpdateAfterBindStorageBuffers = max_descriptor_set_size;
   p->maxDescriptorSetUpdateAfterBindStorageBuffersDynamic = MAX_DYNAMIC_STORAGE_BUFFERS;
   p->maxDescriptorSetUpdateAfterBindSampledImages = max_descriptor_set_size;
   p->maxDescriptorSetUpdateAfterBindStorageImages = max_descriptor_set_size;
   p->maxDescriptorSetUpdateAfterBindInputAttachments = max_descriptor_set_size;

   /* We support all of the depth resolve modes */
   p->supportedDepthResolveModes =
      VK_RESOLVE_MODE_SAMPLE_ZERO_BIT | VK_RESOLVE_MODE_AVERAGE_BIT | VK_RESOLVE_MODE_MIN_BIT | VK_RESOLVE_MODE_MAX_BIT;

   /* Average doesn't make sense for stencil so we don't support that */
   p->supportedStencilResolveModes =
      VK_RESOLVE_MODE_SAMPLE_ZERO_BIT | VK_RESOLVE_MODE_MIN_BIT | VK_RESOLVE_MODE_MAX_BIT;

   p->independentResolveNone = true;
   p->independentResolve = true;

   /* GFX6-8 only support single channel min/max filter. */
   p->filterMinmaxImageComponentMapping = pdev->info.gfx_level >= GFX9;
   p->filterMinmaxSingleComponentFormats = true;

   p->maxTimelineSemaphoreValueDifference = UINT64_MAX;

   p->framebufferIntegerColorSampleCounts = VK_SAMPLE_COUNT_1_BIT;

   /* Vulkan 1.3 */
   p->minSubgroupSize = 64;
   p->maxSubgroupSize = 64;
   p->maxComputeWorkgroupSubgroups = UINT32_MAX;
   p->requiredSubgroupSizeStages = 0;
   if (pdev->info.gfx_level >= GFX10) {
      /* Only GFX10+ supports wave32. */
      p->minSubgroupSize = 32;
      p->requiredSubgroupSizeStages = VK_SHADER_STAGE_COMPUTE_BIT;

      if (radv_taskmesh_enabled(pdev)) {
         p->requiredSubgroupSizeStages |= VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT;
      }
   }

   p->maxInlineUniformBlockSize = MAX_INLINE_UNIFORM_BLOCK_SIZE;
   p->maxPerStageDescriptorInlineUniformBlocks = MAX_INLINE_UNIFORM_BLOCK_SIZE * MAX_SETS;
   p->maxPerStageDescriptorUpdateAfterBindInlineUniformBlocks = MAX_INLINE_UNIFORM_BLOCK_SIZE * MAX_SETS;
   p->maxDescriptorSetInlineUniformBlocks = MAX_INLINE_UNIFORM_BLOCK_COUNT;
   p->maxDescriptorSetUpdateAfterBindInlineUniformBlocks = MAX_INLINE_UNIFORM_BLOCK_COUNT;
   p->maxInlineUniformTotalSize = UINT16_MAX;

   bool accel_dot = pdev->info.has_accelerated_dot_product;
   bool gfx11plus = pdev->info.gfx_level >= GFX11;
   p->integerDotProduct8BitUnsignedAccelerated = accel_dot;
   p->integerDotProduct8BitSignedAccelerated = accel_dot;
   p->integerDotProduct8BitMixedSignednessAccelerated = accel_dot && gfx11plus;
   p->integerDotProduct4x8BitPackedUnsignedAccelerated = accel_dot;
   p->integerDotProduct4x8BitPackedSignedAccelerated = accel_dot;
   p->integerDotProduct4x8BitPackedMixedSignednessAccelerated = accel_dot && gfx11plus;
   p->integerDotProduct16BitUnsignedAccelerated = accel_dot && !gfx11plus;
   p->integerDotProduct16BitSignedAccelerated = accel_dot && !gfx11plus;
   p->integerDotProduct16BitMixedSignednessAccelerated = false;
   p->integerDotProduct32BitUnsignedAccelerated = false;
   p->integerDotProduct32BitSignedAccelerated = false;
   p->integerDotProduct32BitMixedSignednessAccelerated = false;
   p->integerDotProduct64BitUnsignedAccelerated = false;
   p->integerDotProduct64BitSignedAccelerated = false;
   p->integerDotProduct64BitMixedSignednessAccelerated = false;
   p->integerDotProductAccumulatingSaturating8BitUnsignedAccelerated = accel_dot;
   p->integerDotProductAccumulatingSaturating8BitSignedAccelerated = accel_dot;
   p->integerDotProductAccumulatingSaturating8BitMixedSignednessAccelerated = accel_dot && gfx11plus;
   p->integerDotProductAccumulatingSaturating4x8BitPackedUnsignedAccelerated = accel_dot;
   p->integerDotProductAccumulatingSaturating4x8BitPackedSignedAccelerated = accel_dot;
   p->integerDotProductAccumulatingSaturating4x8BitPackedMixedSignednessAccelerated = accel_dot && gfx11plus;
   p->integerDotProductAccumulatingSaturating16BitUnsignedAccelerated = accel_dot && !gfx11plus;
   p->integerDotProductAccumulatingSaturating16BitSignedAccelerated = accel_dot && !gfx11plus;
   p->integerDotProductAccumulatingSaturating16BitMixedSignednessAccelerated = false;
   p->integerDotProductAccumulatingSaturating32BitUnsignedAccelerated = false;
   p->integerDotProductAccumulatingSaturating32BitSignedAccelerated = false;
   p->integerDotProductAccumulatingSaturating32BitMixedSignednessAccelerated = false;
   p->integerDotProductAccumulatingSaturating64BitUnsignedAccelerated = false;
   p->integerDotProductAccumulatingSaturating64BitSignedAccelerated = false;
   p->integerDotProductAccumulatingSaturating64BitMixedSignednessAccelerated = false;

   p->storageTexelBufferOffsetAlignmentBytes = 4;
   p->storageTexelBufferOffsetSingleTexelAlignment = true;
   p->uniformTexelBufferOffsetAlignmentBytes = 4;
   p->uniformTexelBufferOffsetSingleTexelAlignment = true;

   p->maxBufferSize = RADV_MAX_MEMORY_ALLOCATION_SIZE;

   /* VK_KHR_push_descriptor */
   p->maxPushDescriptors = MAX_PUSH_DESCRIPTORS;

   /* VK_EXT_discard_rectangles */
   p->maxDiscardRectangles = MAX_DISCARD_RECTANGLES;

   /* VK_EXT_external_memory_host */
   p->minImportedHostPointerAlignment = 4096;

   /* VK_AMD_shader_core_properties */
   /* Shader engines. */
   p->shaderEngineCount = pdev->info.max_se;
   p->shaderArraysPerEngineCount = pdev->info.max_sa_per_se;
   p->computeUnitsPerShaderArray = pdev->info.min_good_cu_per_sa;
   p->simdPerComputeUnit = pdev->info.num_simd_per_compute_unit;
   p->wavefrontsPerSimd = pdev->info.max_waves_per_simd;
   p->wavefrontSize = 64;

   /* SGPR. */
   p->sgprsPerSimd = pdev->info.num_physical_sgprs_per_simd;
   p->minSgprAllocation = pdev->info.min_sgpr_alloc;
   p->maxSgprAllocation = pdev->info.max_sgpr_alloc;
   p->sgprAllocationGranularity = pdev->info.sgpr_alloc_granularity;

   /* VGPR. */
   p->vgprsPerSimd = pdev->info.num_physical_wave64_vgprs_per_simd;
   p->minVgprAllocation = pdev->info.min_wave64_vgpr_alloc;
   p->maxVgprAllocation = pdev->info.max_vgpr_alloc;
   p->vgprAllocationGranularity = pdev->info.wave64_vgpr_alloc_granularity;

   /* VK_AMD_shader_core_properties2 */
   p->shaderCoreFeatures = 0;
   p->activeComputeUnitCount = pdev->info.num_cu;

   /* VK_KHR_vertex_attribute_divisor */
   p->maxVertexAttribDivisor = UINT32_MAX;
   p->supportsNonZeroFirstInstance = true;

   /* VK_EXT_conservative_rasterization */
   p->primitiveOverestimationSize = 0;
   p->maxExtraPrimitiveOverestimationSize = 0;
   p->extraPrimitiveOverestimationSizeGranularity = 0;
   p->primitiveUnderestimation = true;
   p->conservativePointAndLineRasterization = false;
   p->degenerateTrianglesRasterized = true;
   p->degenerateLinesRasterized = false;
   p->fullyCoveredFragmentShaderInputVariable = true;
   p->conservativeRasterizationPostDepthCoverage = false;

   /* VK_EXT_pci_bus_info */
#ifndef _WIN32
   p->pciDomain = pdev->bus_info.domain;
   p->pciBus = pdev->bus_info.bus;
   p->pciDevice = pdev->bus_info.dev;
   p->pciFunction = pdev->bus_info.func;
#endif

   /* VK_EXT_transform_feedback */
   p->maxTransformFeedbackStreams = MAX_SO_STREAMS;
   p->maxTransformFeedbackBuffers = MAX_SO_BUFFERS;
   p->maxTransformFeedbackBufferSize = UINT32_MAX;
   p->maxTransformFeedbackStreamDataSize = 512;
   p->maxTransformFeedbackBufferDataSize = 512;
   p->maxTransformFeedbackBufferDataStride = 512;
   p->transformFeedbackQueries = true;
   p->transformFeedbackStreamsLinesTriangles = true;
   p->transformFeedbackRasterizationStreamSelect = false;
   p->transformFeedbackDraw = true;

   /* VK_EXT_sample_locations */
   p->sampleLocationSampleCounts = VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_4_BIT | VK_SAMPLE_COUNT_8_BIT;
   p->maxSampleLocationGridSize = (VkExtent2D){2, 2};
   p->sampleLocationCoordinateRange[0] = 0.0f;
   p->sampleLocationCoordinateRange[1] = 0.9375f;
   p->sampleLocationSubPixelBits = 4;
   p->variableSampleLocations = false;

   /* VK_KHR_line_rasterization */
   p->lineSubPixelPrecisionBits = 4;

   /* VK_EXT_robustness2 */
   p->robustStorageBufferAccessSizeAlignment = 4;
   p->robustUniformBufferAccessSizeAlignment = 4;

   /* VK_EXT_custom_border_color */
   p->maxCustomBorderColorSamplers = RADV_BORDER_COLOR_COUNT;

   /* VK_KHR_fragment_shading_rate */
   if (radv_vrs_attachment_enabled(pdev)) {
      p->minFragmentShadingRateAttachmentTexelSize = (VkExtent2D){8, 8};
      p->maxFragmentShadingRateAttachmentTexelSize = (VkExtent2D){8, 8};
   } else {
      p->minFragmentShadingRateAttachmentTexelSize = (VkExtent2D){0, 0};
      p->maxFragmentShadingRateAttachmentTexelSize = (VkExtent2D){0, 0};
   }
   p->maxFragmentShadingRateAttachmentTexelSizeAspectRatio = 1;
   p->primitiveFragmentShadingRateWithMultipleViewports = true;
   p->layeredShadingRateAttachments = false; /* TODO */
   p->fragmentShadingRateNonTrivialCombinerOps = true;
   p->maxFragmentSize = (VkExtent2D){2, 2};
   p->maxFragmentSizeAspectRatio = 2;
   p->maxFragmentShadingRateCoverageSamples = 32;
   p->maxFragmentShadingRateRasterizationSamples = VK_SAMPLE_COUNT_8_BIT;
   p->fragmentShadingRateWithShaderDepthStencilWrites = !pdev->info.has_vrs_ds_export_bug;
   p->fragmentShadingRateWithSampleMask = true;
   p->fragmentShadingRateWithShaderSampleMask = false;
   p->fragmentShadingRateWithConservativeRasterization = true;
   p->fragmentShadingRateWithFragmentShaderInterlock = pdev->info.gfx_level >= GFX11 && radv_has_pops(pdev);
   p->fragmentShadingRateWithCustomSampleLocations = false;
   p->fragmentShadingRateStrictMultiplyCombiner = true;

   /* VK_EXT_provoking_vertex */
   p->provokingVertexModePerPipeline = true;
   p->transformFeedbackPreservesTriangleFanProvokingVertex = true;

   /* VK_KHR_acceleration_structure */
   p->maxGeometryCount = (1 << 24) - 1;
   p->maxInstanceCount = (1 << 24) - 1;
   p->maxPrimitiveCount = (1 << 29) - 1;
   p->maxPerStageDescriptorAccelerationStructures = p->maxPerStageDescriptorStorageBuffers;
   p->maxPerStageDescriptorUpdateAfterBindAccelerationStructures = p->maxPerStageDescriptorStorageBuffers;
   p->maxDescriptorSetAccelerationStructures = p->maxDescriptorSetStorageBuffers;
   p->maxDescriptorSetUpdateAfterBindAccelerationStructures = p->maxDescriptorSetStorageBuffers;
   p->minAccelerationStructureScratchOffsetAlignment = 128;

   /* VK_EXT_physical_device_drm */
#ifndef _WIN32
   if (pdev->available_nodes & (1 << DRM_NODE_PRIMARY)) {
      p->drmHasPrimary = true;
      p->drmPrimaryMajor = (int64_t)major(pdev->primary_devid);
      p->drmPrimaryMinor = (int64_t)minor(pdev->primary_devid);
   } else {
      p->drmHasPrimary = false;
   }
   if (pdev->available_nodes & (1 << DRM_NODE_RENDER)) {
      p->drmHasRender = true;
      p->drmRenderMajor = (int64_t)major(pdev->render_devid);
      p->drmRenderMinor = (int64_t)minor(pdev->render_devid);
   } else {
      p->drmHasRender = false;
   }
#endif

   /* VK_EXT_multi_draw */
   p->maxMultiDrawCount = 2048;

   /* VK_KHR_ray_tracing_pipeline */

   p->shaderGroupHandleSize = RADV_RT_HANDLE_SIZE;
   p->maxRayRecursionDepth = 31;    /* Minimum allowed for DXR. */
   p->maxShaderGroupStride = 16384; /* dummy */
   /* This isn't strictly necessary, but Doom Eternal breaks if the
    * alignment is any lower. */
   p->shaderGroupBaseAlignment = RADV_RT_HANDLE_SIZE;
   p->shaderGroupHandleCaptureReplaySize = sizeof(struct radv_rt_capture_replay_handle);
   p->maxRayDispatchInvocationCount = 1024 * 1024 * 64;
   p->shaderGroupHandleAlignment = 16;
   p->maxRayHitAttributeSize = RADV_MAX_HIT_ATTRIB_SIZE;

   /* VK_EXT_shader_module_identifier */
   STATIC_ASSERT(sizeof(vk_shaderModuleIdentifierAlgorithmUUID) == sizeof(p->shaderModuleIdentifierAlgorithmUUID));
   memcpy(p->shaderModuleIdentifierAlgorithmUUID, vk_shaderModuleIdentifierAlgorithmUUID,
          sizeof(p->shaderModuleIdentifierAlgorithmUUID));

   /* VK_KHR_performance_query */
   p->allowCommandBufferQueryCopies = false;

   /* VK_NV_device_generated_commands */
   p->maxIndirectCommandsStreamCount = 1;
   p->maxIndirectCommandsStreamStride = UINT32_MAX;
   p->maxIndirectCommandsTokenCount = 512;
   p->maxIndirectCommandsTokenOffset = UINT16_MAX;
   p->minIndirectCommandsBufferOffsetAlignment = 4;
   p->minSequencesCountBufferOffsetAlignment = 4;
   p->minSequencesIndexBufferOffsetAlignment = 4;
   /* Don't support even a shader group count = 1 until we support shader
    * overrides during pipeline creation. */
   p->maxGraphicsShaderGroupCount = 0;
   /* MSB reserved for signalling indirect count enablement. */
   p->maxIndirectSequenceCount = UINT32_MAX >> 1;

   /* VK_EXT_graphics_pipeline_library */
   p->graphicsPipelineLibraryFastLinking = true;
   p->graphicsPipelineLibraryIndependentInterpolationDecoration = true;

   /* VK_EXT_mesh_shader */
   p->maxTaskWorkGroupTotalCount = 4194304; /* 2^22 min required */
   p->maxTaskWorkGroupCount[0] = 65535;
   p->maxTaskWorkGroupCount[1] = 65535;
   p->maxTaskWorkGroupCount[2] = 65535;
   p->maxTaskWorkGroupInvocations = 1024;
   p->maxTaskWorkGroupSize[0] = 1024;
   p->maxTaskWorkGroupSize[1] = 1024;
   p->maxTaskWorkGroupSize[2] = 1024;
   p->maxTaskPayloadSize = 16384; /* 16K min required */
   p->maxTaskSharedMemorySize = 65536;
   p->maxTaskPayloadAndSharedMemorySize = 65536;

   p->maxMeshWorkGroupTotalCount = 4194304; /* 2^22 min required */
   p->maxMeshWorkGroupCount[0] = 65535;
   p->maxMeshWorkGroupCount[1] = 65535;
   p->maxMeshWorkGroupCount[2] = 65535;
   p->maxMeshWorkGroupInvocations = 256; /* Max NGG HW limit */
   p->maxMeshWorkGroupSize[0] = 256;
   p->maxMeshWorkGroupSize[1] = 256;
   p->maxMeshWorkGroupSize[2] = 256;
   p->maxMeshOutputMemorySize = 32 * 1024;                                                    /* 32K min required */
   p->maxMeshSharedMemorySize = 28672;                                                        /* 28K min required */
   p->maxMeshPayloadAndSharedMemorySize = p->maxTaskPayloadSize + p->maxMeshSharedMemorySize; /* 28K min required */
   p->maxMeshPayloadAndOutputMemorySize = p->maxTaskPayloadSize + p->maxMeshOutputMemorySize; /* 47K min required */
   p->maxMeshOutputComponents = 128; /* 32x vec4 min required */
   p->maxMeshOutputVertices = 256;
   p->maxMeshOutputPrimitives = 256;
   p->maxMeshOutputLayers = 8;
   p->maxMeshMultiviewViewCount = MAX_VIEWS;
   p->meshOutputPerVertexGranularity = 1;
   p->meshOutputPerPrimitiveGranularity = 1;

   p->maxPreferredTaskWorkGroupInvocations = 64;
   p->maxPreferredMeshWorkGroupInvocations = 128;
   p->prefersLocalInvocationVertexOutput = true;
   p->prefersLocalInvocationPrimitiveOutput = true;
   p->prefersCompactVertexOutput = true;
   p->prefersCompactPrimitiveOutput = false;

   /* VK_EXT_extended_dynamic_state3 */
   p->dynamicPrimitiveTopologyUnrestricted = false;

   /* VK_EXT_descriptor_buffer */
   p->combinedImageSamplerDescriptorSingleArray = true;
   p->bufferlessPushDescriptors = true;
   p->allowSamplerImageViewPostSubmitCreation = false;
   p->descriptorBufferOffsetAlignment = 4;
   p->maxDescriptorBufferBindings = MAX_SETS;
   p->maxResourceDescriptorBufferBindings = MAX_SETS;
   p->maxSamplerDescriptorBufferBindings = MAX_SETS;
   p->maxEmbeddedImmutableSamplerBindings = MAX_SETS;
   p->maxEmbeddedImmutableSamplers = radv_max_descriptor_set_size();
   p->bufferCaptureReplayDescriptorDataSize = 0;
   p->imageCaptureReplayDescriptorDataSize = 0;
   p->imageViewCaptureReplayDescriptorDataSize = 0;
   p->samplerCaptureReplayDescriptorDataSize = 0;
   p->accelerationStructureCaptureReplayDescriptorDataSize = 0;
   p->samplerDescriptorSize = 16;
   p->combinedImageSamplerDescriptorSize = 96;
   p->sampledImageDescriptorSize = 64;
   p->storageImageDescriptorSize = 32;
   p->uniformTexelBufferDescriptorSize = 16;
   p->robustUniformTexelBufferDescriptorSize = 16;
   p->storageTexelBufferDescriptorSize = 16;
   p->robustStorageTexelBufferDescriptorSize = 16;
   p->uniformBufferDescriptorSize = 16;
   p->robustUniformBufferDescriptorSize = 16;
   p->storageBufferDescriptorSize = 16;
   p->robustStorageBufferDescriptorSize = 16;
   p->inputAttachmentDescriptorSize = 64;
   p->accelerationStructureDescriptorSize = 16;
   p->maxSamplerDescriptorBufferRange = UINT32_MAX;
   p->maxResourceDescriptorBufferRange = UINT32_MAX;
   p->samplerDescriptorBufferAddressSpaceSize = RADV_MAX_MEMORY_ALLOCATION_SIZE;
   p->resourceDescriptorBufferAddressSpaceSize = RADV_MAX_MEMORY_ALLOCATION_SIZE;
   p->descriptorBufferAddressSpaceSize = RADV_MAX_MEMORY_ALLOCATION_SIZE;

   /* VK_KHR_fragment_shader_barycentric */
   p->triStripVertexOrderIndependentOfProvokingVertex = false;

   /* VK_EXT_pipeline_robustness */
   p->defaultRobustnessStorageBuffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_EXT;
   p->defaultRobustnessUniformBuffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_EXT;
   p->defaultRobustnessVertexInputs = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DISABLED_EXT;
   p->defaultRobustnessImages = VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_ROBUST_IMAGE_ACCESS_2_EXT;

   /* VK_KHR_maintenance5 */
   p->earlyFragmentMultisampleCoverageAfterSampleCounting = false;
   p->earlyFragmentSampleMaskTestBeforeSampleCounting = false;
   p->depthStencilSwizzleOneSupport = false;
   p->polygonModePointSize = true;
   p->nonStrictSinglePixelWideLinesUseParallelogram = false;
   p->nonStrictWideLinesUseParallelogram = false;

   /* VK_KHR_cooperative_matrix */
   p->cooperativeMatrixSupportedStages = VK_SHADER_STAGE_COMPUTE_BIT;

   /* VK_KHR_maintenance6 */
   p->blockTexelViewCompatibleMultipleLayers = true;
   p->maxCombinedImageSamplerDescriptorCount = 1;
   p->fragmentShadingRateClampCombinerInputs = true;

   /* VK_EXT_shader_object */
   radv_device_get_cache_uuid(pdev, p->shaderBinaryUUID);
   p->shaderBinaryVersion = 1;

   /* VK_EXT_map_memory_placed */
   uint64_t os_page_size = 4096;
   os_get_page_size(&os_page_size);
   p->minPlacedMemoryMapAlignment = os_page_size;

   /* VK_EXT_nested_command_buffer */
   p->maxCommandBufferNestingLevel = UINT32_MAX;
}

static VkResult
radv_physical_device_try_create(struct radv_instance *instance, drmDevicePtr drm_device,
                                struct radv_physical_device **pdev_out)
{
   VkResult result;
   int fd = -1;
   int master_fd = -1;

#ifdef _WIN32
   assert(drm_device == NULL);
#else
   if (drm_device) {
      const char *path = drm_device->nodes[DRM_NODE_RENDER];
      drmVersionPtr version;

      fd = open(path, O_RDWR | O_CLOEXEC);
      if (fd < 0) {
         return vk_errorf(instance, VK_ERROR_INCOMPATIBLE_DRIVER, "Could not open device %s: %m", path);
      }

      version = drmGetVersion(fd);
      if (!version) {
         close(fd);

         return vk_errorf(instance, VK_ERROR_INCOMPATIBLE_DRIVER,
                          "Could not get the kernel driver version for device %s: %m", path);
      }

      if (strcmp(version->name, "amdgpu")) {
         drmFreeVersion(version);
         close(fd);

         return vk_errorf(instance, VK_ERROR_INCOMPATIBLE_DRIVER,
                          "Device '%s' is not using the AMDGPU kernel driver: %m", path);
      }
      drmFreeVersion(version);

      if (instance->debug_flags & RADV_DEBUG_STARTUP)
         fprintf(stderr, "radv: info: Found compatible device '%s'.\n", path);
   }
#endif

   struct radv_physical_device *pdev =
      vk_zalloc2(&instance->vk.alloc, NULL, sizeof(*pdev), 8, VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
   if (!pdev) {
      result = vk_error(instance, VK_ERROR_OUT_OF_HOST_MEMORY);
      goto fail_fd;
   }

   struct vk_physical_device_dispatch_table dispatch_table;
   vk_physical_device_dispatch_table_from_entrypoints(&dispatch_table, &radv_physical_device_entrypoints, true);
   vk_physical_device_dispatch_table_from_entrypoints(&dispatch_table, &wsi_physical_device_entrypoints, false);

   result = vk_physical_device_init(&pdev->vk, &instance->vk, NULL, NULL, NULL, &dispatch_table);
   if (result != VK_SUCCESS) {
      goto fail_alloc;
   }

#ifdef _WIN32
   pdev->ws = radv_null_winsys_create();
#else
   if (drm_device) {
      bool reserve_vmid = instance->vk.trace_mode & RADV_TRACE_MODE_RGP;

      pdev->ws = radv_amdgpu_winsys_create(fd, instance->debug_flags, instance->perftest_flags, reserve_vmid);
   } else {
      pdev->ws = radv_null_winsys_create();
   }
#endif

   if (!pdev->ws) {
      result = vk_errorf(instance, VK_ERROR_INITIALIZATION_FAILED, "failed to initialize winsys");
      goto fail_base;
   }

   pdev->vk.supported_sync_types = pdev->ws->get_sync_types(pdev->ws);

#ifndef _WIN32
   if (drm_device && instance->vk.enabled_extensions.KHR_display) {
      master_fd = open(drm_device->nodes[DRM_NODE_PRIMARY], O_RDWR | O_CLOEXEC);
      if (master_fd >= 0) {
         uint32_t accel_working = 0;
         struct drm_amdgpu_info request = {.return_pointer = (uintptr_t)&accel_working,
                                           .return_size = sizeof(accel_working),
                                           .query = AMDGPU_INFO_ACCEL_WORKING};

         if (drmCommandWrite(master_fd, DRM_AMDGPU_INFO, &request, sizeof(struct drm_amdgpu_info)) < 0 ||
             !accel_working) {
            close(master_fd);
            master_fd = -1;
         }
      }
   }
#endif

   pdev->master_fd = master_fd;
   pdev->local_fd = fd;
   pdev->ws->query_info(pdev->ws, &pdev->info);

   pdev->use_llvm = instance->debug_flags & RADV_DEBUG_LLVM;
#if !LLVM_AVAILABLE
   if (pdev->use_llvm) {
      fprintf(stderr, "ERROR: LLVM compiler backend selected for radv, but LLVM support was not "
                      "enabled at build time.\n");
      abort();
   }
#endif

#if DETECT_OS_ANDROID
   pdev->emulate_etc2 = !radv_device_supports_etc(pdev);
   pdev->emulate_astc = true;
#else
   pdev->emulate_etc2 = !radv_device_supports_etc(pdev) && instance->drirc.vk_require_etc2;
   pdev->emulate_astc = instance->drirc.vk_require_astc;
#endif

   snprintf(pdev->name, sizeof(pdev->name), "AMD RADV %s%s", pdev->info.name, radv_get_compiler_string(pdev));

   const char *marketing_name = pdev->ws->get_chip_name(pdev->ws);
   snprintf(pdev->marketing_name, sizeof(pdev->name), "%s (RADV %s%s)", marketing_name ? marketing_name : "AMD Unknown",
            pdev->info.name, radv_get_compiler_string(pdev));

   if (!radv_is_conformant(pdev))
      vk_warn_non_conformant_implementation("radv");

   radv_get_driver_uuid(&pdev->driver_uuid);
   radv_get_device_uuid(&pdev->info, &pdev->device_uuid);

   pdev->dcc_msaa_allowed = (instance->perftest_flags & RADV_PERFTEST_DCC_MSAA);

   pdev->use_fmask = pdev->info.gfx_level < GFX11 && !(instance->debug_flags & RADV_DEBUG_NO_FMASK);

   pdev->use_ngg = (pdev->info.gfx_level >= GFX10 && pdev->info.family != CHIP_NAVI14 &&
                    !(instance->debug_flags & RADV_DEBUG_NO_NGG)) ||
                   pdev->info.gfx_level >= GFX11;

   /* TODO: Investigate if NGG culling helps on GFX11. */
   pdev->use_ngg_culling = pdev->use_ngg && pdev->info.max_render_backends > 1 &&
                           (pdev->info.gfx_level == GFX10_3 || (instance->perftest_flags & RADV_PERFTEST_NGGC)) &&
                           !(instance->debug_flags & RADV_DEBUG_NO_NGGC);

   pdev->use_ngg_streamout = pdev->info.gfx_level >= GFX11;

   pdev->emulate_ngg_gs_query_pipeline_stat = pdev->use_ngg && pdev->info.gfx_level < GFX11;

   pdev->mesh_fast_launch_2 =
      pdev->info.gfx_level >= GFX11 && !(instance->debug_flags & RADV_DEBUG_NO_GS_FAST_LAUNCH_2);

   pdev->emulate_mesh_shader_queries = pdev->info.gfx_level == GFX10_3;

   /* Determine the number of threads per wave for all stages. */
   pdev->cs_wave_size = 64;
   pdev->ps_wave_size = 64;
   pdev->ge_wave_size = 64;
   pdev->rt_wave_size = 64;

   if (pdev->info.gfx_level >= GFX10) {
      if (instance->perftest_flags & RADV_PERFTEST_CS_WAVE_32)
         pdev->cs_wave_size = 32;

      /* For pixel shaders, wave64 is recommended. */
      if (instance->perftest_flags & RADV_PERFTEST_PS_WAVE_32)
         pdev->ps_wave_size = 32;

      if (instance->perftest_flags & RADV_PERFTEST_GE_WAVE_32)
         pdev->ge_wave_size = 32;

      /* Default to 32 on RDNA1-2 as that gives better perf due to less issues with divergence.
       * However, on RDNA3+ default to wave64 as implicit dual issuing is likely better than
       * wave32 VOPD for VALU dependent code.
       * (as well as the SALU count becoming more problematic with wave32)
       */
      if (instance->perftest_flags & RADV_PERFTEST_RT_WAVE_32 || pdev->info.gfx_level < GFX11)
         pdev->rt_wave_size = 32;

      if (instance->perftest_flags & RADV_PERFTEST_RT_WAVE_64 || instance->drirc.force_rt_wave64)
         pdev->rt_wave_size = 64;
   }

   radv_probe_video_encode(pdev);

   pdev->max_shared_size = pdev->info.gfx_level >= GFX7 ? 65536 : 32768;

   radv_physical_device_init_mem_types(pdev);

   radv_physical_device_get_supported_extensions(pdev, &pdev->vk.supported_extensions);
   radv_physical_device_get_features(pdev, &pdev->vk.supported_features);

   radv_get_nir_options(pdev);

#ifndef _WIN32
   if (drm_device) {
      struct stat primary_stat = {0}, render_stat = {0};

      pdev->available_nodes = drm_device->available_nodes;
      pdev->bus_info = *drm_device->businfo.pci;

      if ((drm_device->available_nodes & (1 << DRM_NODE_PRIMARY)) &&
          stat(drm_device->nodes[DRM_NODE_PRIMARY], &primary_stat) != 0) {
         result = vk_errorf(instance, VK_ERROR_INITIALIZATION_FAILED, "failed to stat DRM primary node %s",
                            drm_device->nodes[DRM_NODE_PRIMARY]);
         goto fail_perfcounters;
      }
      pdev->primary_devid = primary_stat.st_rdev;

      if ((drm_device->available_nodes & (1 << DRM_NODE_RENDER)) &&
          stat(drm_device->nodes[DRM_NODE_RENDER], &render_stat) != 0) {
         result = vk_errorf(instance, VK_ERROR_INITIALIZATION_FAILED, "failed to stat DRM render node %s",
                            drm_device->nodes[DRM_NODE_RENDER]);
         goto fail_perfcounters;
      }
      pdev->render_devid = render_stat.st_rdev;
   }
#endif

   radv_physical_device_init_cache_key(pdev);

   if (radv_device_get_cache_uuid(pdev, pdev->cache_uuid)) {
      result = vk_errorf(instance, VK_ERROR_INITIALIZATION_FAILED, "cannot generate UUID");
      goto fail_wsi;
   }

   /* The gpu id is already embedded in the uuid so we just pass "radv"
    * when creating the cache.
    */
   char buf[VK_UUID_SIZE * 2 + 1];
   mesa_bytes_to_hex(buf, pdev->cache_uuid, VK_UUID_SIZE);
   pdev->vk.disk_cache = disk_cache_create(pdev->name, buf, 0);

   radv_get_physical_device_properties(pdev);

   if ((instance->debug_flags & RADV_DEBUG_INFO))
      ac_print_gpu_info(&pdev->info, stdout);

   radv_init_physical_device_decoder(pdev);
   radv_init_physical_device_encoder(pdev);

   radv_physical_device_init_queue_table(pdev);

   /* We don't check the error code, but later check if it is initialized. */
   ac_init_perfcounters(&pdev->info, false, false, &pdev->ac_perfcounters);

   /* The WSI is structured as a layer on top of the driver, so this has
    * to be the last part of initialization (at least until we get other
    * semi-layers).
    */
   result = radv_init_wsi(pdev);
   if (result != VK_SUCCESS) {
      vk_error(instance, result);
      goto fail_perfcounters;
   }

   pdev->gs_table_depth = ac_get_gs_table_depth(pdev->info.gfx_level, pdev->info.family);

   ac_get_hs_info(&pdev->info, &pdev->hs);
   ac_get_task_info(&pdev->info, &pdev->task_info);
   radv_get_binning_settings(pdev, &pdev->binning_settings);

   *pdev_out = pdev;

   return VK_SUCCESS;

fail_perfcounters:
   ac_destroy_perfcounters(&pdev->ac_perfcounters);
   disk_cache_destroy(pdev->vk.disk_cache);
fail_wsi:
   pdev->ws->destroy(pdev->ws);
fail_base:
   vk_physical_device_finish(&pdev->vk);
fail_alloc:
   vk_free(&instance->vk.alloc, pdev);
fail_fd:
   if (fd != -1)
      close(fd);
   if (master_fd != -1)
      close(master_fd);
   return result;
}

VkResult
create_null_physical_device(struct vk_instance *vk_instance)
{
   struct radv_instance *instance = container_of(vk_instance, struct radv_instance, vk);
   struct radv_physical_device *pdev;

   VkResult result = radv_physical_device_try_create(instance, NULL, &pdev);
   if (result != VK_SUCCESS)
      return result;

   list_addtail(&pdev->vk.link, &instance->vk.physical_devices.list);
   return VK_SUCCESS;
}

VkResult
create_drm_physical_device(struct vk_instance *vk_instance, struct _drmDevice *device, struct vk_physical_device **out)
{
#ifndef _WIN32
   if (!(device->available_nodes & (1 << DRM_NODE_RENDER)) || device->bustype != DRM_BUS_PCI ||
       device->deviceinfo.pci->vendor_id != ATI_VENDOR_ID)
      return VK_ERROR_INCOMPATIBLE_DRIVER;

   return radv_physical_device_try_create((struct radv_instance *)vk_instance, device,
                                          (struct radv_physical_device **)out);
#else
   return VK_SUCCESS;
#endif
}

void
radv_physical_device_destroy(struct vk_physical_device *vk_device)
{
   struct radv_physical_device *pdev = container_of(vk_device, struct radv_physical_device, vk);
   const struct radv_instance *instance = radv_physical_device_instance(pdev);

   radv_finish_wsi(pdev);
   ac_destroy_perfcounters(&pdev->ac_perfcounters);
   pdev->ws->destroy(pdev->ws);
   disk_cache_destroy(pdev->vk.disk_cache);
   if (pdev->local_fd != -1)
      close(pdev->local_fd);
   if (pdev->master_fd != -1)
      close(pdev->master_fd);
   vk_physical_device_finish(&pdev->vk);
   vk_free(&instance->vk.alloc, pdev);
}

static void
radv_get_physical_device_queue_family_properties(struct radv_physical_device *pdev, uint32_t *pCount,
                                                 VkQueueFamilyProperties **pQueueFamilyProperties)
{
   const struct radv_instance *instance = radv_physical_device_instance(pdev);
   int num_queue_families = 1;
   int idx;
   if (pdev->info.ip[AMD_IP_COMPUTE].num_queues > 0 && !(instance->debug_flags & RADV_DEBUG_NO_COMPUTE_QUEUE))
      num_queue_families++;

   if (instance->perftest_flags & RADV_PERFTEST_VIDEO_DECODE) {
      if (pdev->info.ip[pdev->vid_decode_ip].num_queues > 0)
         num_queue_families++;
   }

   if (radv_transfer_queue_enabled(pdev)) {
      num_queue_families++;
   }

   if (pdev->video_encode_enabled) {
     if (pdev->info.ip[AMD_IP_VCN_ENC].num_queues > 0)
       num_queue_families++;
   }

   if (radv_sparse_queue_enabled(pdev)) {
      num_queue_families++;
   }

   if (pQueueFamilyProperties == NULL) {
      *pCount = num_queue_families;
      return;
   }

   if (!*pCount)
      return;

   idx = 0;
   if (*pCount >= 1) {
      VkQueueFlags gfx_flags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
      if (!radv_sparse_queue_enabled(pdev))
         gfx_flags |= VK_QUEUE_SPARSE_BINDING_BIT;
      *pQueueFamilyProperties[idx] = (VkQueueFamilyProperties){
         .queueFlags = gfx_flags,
         .queueCount = 1,
         .timestampValidBits = 64,
         .minImageTransferGranularity = (VkExtent3D){1, 1, 1},
      };
      idx++;
   }

   if (pdev->info.ip[AMD_IP_COMPUTE].num_queues > 0 && !(instance->debug_flags & RADV_DEBUG_NO_COMPUTE_QUEUE)) {
      VkQueueFlags compute_flags = VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
      if (!radv_sparse_queue_enabled(pdev))
         compute_flags |= VK_QUEUE_SPARSE_BINDING_BIT;
      if (*pCount > idx) {
         *pQueueFamilyProperties[idx] = (VkQueueFamilyProperties){
            .queueFlags = compute_flags,
            .queueCount = pdev->info.ip[AMD_IP_COMPUTE].num_queues,
            .timestampValidBits = 64,
            .minImageTransferGranularity = (VkExtent3D){1, 1, 1},
         };
         idx++;
      }
   }

   if (instance->perftest_flags & RADV_PERFTEST_VIDEO_DECODE) {
      if (pdev->info.ip[pdev->vid_decode_ip].num_queues > 0) {
         if (*pCount > idx) {
            *pQueueFamilyProperties[idx] = (VkQueueFamilyProperties){
               .queueFlags = VK_QUEUE_VIDEO_DECODE_BIT_KHR,
               .queueCount = pdev->info.ip[pdev->vid_decode_ip].num_queues,
               .timestampValidBits = 0,
               .minImageTransferGranularity = (VkExtent3D){1, 1, 1},
            };
            idx++;
         }
      }
   }

   if (radv_transfer_queue_enabled(pdev)) {
      if (*pCount > idx) {
         *pQueueFamilyProperties[idx] = (VkQueueFamilyProperties){
            .queueFlags = VK_QUEUE_TRANSFER_BIT,
            .queueCount = pdev->info.ip[AMD_IP_SDMA].num_queues,
            .timestampValidBits = 64,
            .minImageTransferGranularity = (VkExtent3D){16, 16, 8},
         };
         idx++;
      }
   }

   if (pdev->video_encode_enabled) {
      if (pdev->info.ip[AMD_IP_VCN_ENC].num_queues > 0) {
         if (*pCount > idx) {
            *pQueueFamilyProperties[idx] = (VkQueueFamilyProperties){
               .queueFlags = VK_QUEUE_VIDEO_ENCODE_BIT_KHR,
               .queueCount = pdev->info.ip[AMD_IP_VCN_ENC].num_queues,
               .timestampValidBits = 0,
               .minImageTransferGranularity = (VkExtent3D){1, 1, 1},
            };
            idx++;
         }
      }
   }

   if (radv_sparse_queue_enabled(pdev)) {
      if (*pCount > idx) {
         *pQueueFamilyProperties[idx] = (VkQueueFamilyProperties){
            .queueFlags = VK_QUEUE_SPARSE_BINDING_BIT,
            .queueCount = 1,
            .timestampValidBits = 64,
            .minImageTransferGranularity = (VkExtent3D){1, 1, 1},
         };
         idx++;
      }
   }

   *pCount = idx;
}

static const VkQueueGlobalPriorityKHR radv_global_queue_priorities[] = {
   VK_QUEUE_GLOBAL_PRIORITY_LOW_KHR,
   VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_KHR,
   VK_QUEUE_GLOBAL_PRIORITY_HIGH_KHR,
   VK_QUEUE_GLOBAL_PRIORITY_REALTIME_KHR,
};

VKAPI_ATTR void VKAPI_CALL
radv_GetPhysicalDeviceQueueFamilyProperties2(VkPhysicalDevice physicalDevice, uint32_t *pCount,
                                             VkQueueFamilyProperties2 *pQueueFamilyProperties)
{
   VK_FROM_HANDLE(radv_physical_device, pdev, physicalDevice);
   if (!pQueueFamilyProperties) {
      radv_get_physical_device_queue_family_properties(pdev, pCount, NULL);
      return;
   }
   VkQueueFamilyProperties *properties[] = {
      &pQueueFamilyProperties[0].queueFamilyProperties, &pQueueFamilyProperties[1].queueFamilyProperties,
      &pQueueFamilyProperties[2].queueFamilyProperties, &pQueueFamilyProperties[3].queueFamilyProperties,
      &pQueueFamilyProperties[4].queueFamilyProperties, &pQueueFamilyProperties[5].queueFamilyProperties,
   };
   radv_get_physical_device_queue_family_properties(pdev, pCount, properties);
   assert(*pCount <= 6);

   for (uint32_t i = 0; i < *pCount; i++) {
      vk_foreach_struct (ext, pQueueFamilyProperties[i].pNext) {
         switch (ext->sType) {
         case VK_STRUCTURE_TYPE_QUEUE_FAMILY_GLOBAL_PRIORITY_PROPERTIES_KHR: {
            VkQueueFamilyGlobalPriorityPropertiesKHR *prop = (VkQueueFamilyGlobalPriorityPropertiesKHR *)ext;
            STATIC_ASSERT(ARRAY_SIZE(radv_global_queue_priorities) <= VK_MAX_GLOBAL_PRIORITY_SIZE_KHR);
            prop->priorityCount = ARRAY_SIZE(radv_global_queue_priorities);
            memcpy(&prop->priorities, radv_global_queue_priorities, sizeof(radv_global_queue_priorities));
            break;
         }
         case VK_STRUCTURE_TYPE_QUEUE_FAMILY_QUERY_RESULT_STATUS_PROPERTIES_KHR: {
            VkQueueFamilyQueryResultStatusPropertiesKHR *prop = (VkQueueFamilyQueryResultStatusPropertiesKHR *)ext;
            prop->queryResultStatusSupport = VK_FALSE;
            break;
         }
         case VK_STRUCTURE_TYPE_QUEUE_FAMILY_VIDEO_PROPERTIES_KHR: {
            VkQueueFamilyVideoPropertiesKHR *prop = (VkQueueFamilyVideoPropertiesKHR *)ext;
            prop->videoCodecOperations = 0;
            if (pQueueFamilyProperties[i].queueFamilyProperties.queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR) {
               if (VIDEO_CODEC_H264DEC)
                  prop->videoCodecOperations |= VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR;
               if (VIDEO_CODEC_H265DEC)
                  prop->videoCodecOperations |= VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR;
               if (VIDEO_CODEC_AV1DEC && pdev->info.vcn_ip_version >= VCN_3_0_0 &&
                   pdev->info.vcn_ip_version != VCN_3_0_33)
                  prop->videoCodecOperations |= VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR;
            }
            if (pQueueFamilyProperties[i].queueFamilyProperties.queueFlags & VK_QUEUE_VIDEO_ENCODE_BIT_KHR) {
               if (VIDEO_CODEC_H264ENC)
                  prop->videoCodecOperations |= VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR;
               if (VIDEO_CODEC_H265ENC)
                  prop->videoCodecOperations |= VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR;
            }
            break;
         }
         default:
            break;
         }
      }
   }
}

static void
radv_get_memory_budget_properties(VkPhysicalDevice physicalDevice,
                                  VkPhysicalDeviceMemoryBudgetPropertiesEXT *memoryBudget)
{
   VK_FROM_HANDLE(radv_physical_device, pdev, physicalDevice);
   const struct radv_instance *instance = radv_physical_device_instance(pdev);
   VkPhysicalDeviceMemoryProperties *memory_properties = &pdev->memory_properties;

   /* For all memory heaps, the computation of budget is as follow:
    *	heap_budget = heap_size - global_heap_usage + app_heap_usage
    *
    * The Vulkan spec 1.1.97 says that the budget should include any
    * currently allocated device memory.
    *
    * Note that the application heap usages are not really accurate (eg.
    * in presence of shared buffers).
    */
   if (!pdev->info.has_dedicated_vram) {
      if (instance->drirc.enable_unified_heap_on_apu) {
         /* When the heaps are unified, only the visible VRAM heap is exposed on APUs. */
         assert(pdev->heaps == RADV_HEAP_VRAM_VIS);
         assert(pdev->memory_properties.memoryHeaps[0].flags == VK_MEMORY_HEAP_DEVICE_LOCAL_BIT);
         const uint8_t vram_vis_heap_idx = 0;

         /* Get the total heap size which is the visible VRAM heap size. */
         uint64_t total_heap_size = pdev->memory_properties.memoryHeaps[vram_vis_heap_idx].size;

         /* Get the different memory usages. */
         uint64_t vram_vis_internal_usage = pdev->ws->query_value(pdev->ws, RADEON_ALLOCATED_VRAM_VIS) +
                                            pdev->ws->query_value(pdev->ws, RADEON_ALLOCATED_VRAM);
         uint64_t gtt_internal_usage = pdev->ws->query_value(pdev->ws, RADEON_ALLOCATED_GTT);
         uint64_t total_internal_usage = vram_vis_internal_usage + gtt_internal_usage;
         uint64_t total_system_usage =
            pdev->ws->query_value(pdev->ws, RADEON_VRAM_VIS_USAGE) + pdev->ws->query_value(pdev->ws, RADEON_GTT_USAGE);
         uint64_t total_usage = MAX2(total_internal_usage, total_system_usage);

         /* Compute the total free space that can be allocated for this process across all heaps. */
         uint64_t total_free_space = total_heap_size - MIN2(total_heap_size, total_usage);

         memoryBudget->heapBudget[vram_vis_heap_idx] = total_free_space + total_internal_usage;
         memoryBudget->heapUsage[vram_vis_heap_idx] = total_internal_usage;
      } else {
         /* On APUs, the driver exposes fake heaps to the application because usually the carveout
          * is too small for games but the budgets need to be redistributed accordingly.
          */
         assert(pdev->heaps == (RADV_HEAP_GTT | RADV_HEAP_VRAM_VIS));
         assert(pdev->memory_properties.memoryHeaps[0].flags == 0); /* GTT */
         assert(pdev->memory_properties.memoryHeaps[1].flags == VK_MEMORY_HEAP_DEVICE_LOCAL_BIT);
         const uint8_t gtt_heap_idx = 0, vram_vis_heap_idx = 1;

         /* Get the visible VRAM/GTT heap sizes and internal usages. */
         uint64_t gtt_heap_size = pdev->memory_properties.memoryHeaps[gtt_heap_idx].size;
         uint64_t vram_vis_heap_size = pdev->memory_properties.memoryHeaps[vram_vis_heap_idx].size;

         uint64_t vram_vis_internal_usage = pdev->ws->query_value(pdev->ws, RADEON_ALLOCATED_VRAM_VIS) +
                                            pdev->ws->query_value(pdev->ws, RADEON_ALLOCATED_VRAM);
         uint64_t gtt_internal_usage = pdev->ws->query_value(pdev->ws, RADEON_ALLOCATED_GTT);

         /* Compute the total heap size, internal and system usage. */
         uint64_t total_heap_size = vram_vis_heap_size + gtt_heap_size;
         uint64_t total_internal_usage = vram_vis_internal_usage + gtt_internal_usage;
         uint64_t total_system_usage =
            pdev->ws->query_value(pdev->ws, RADEON_VRAM_VIS_USAGE) + pdev->ws->query_value(pdev->ws, RADEON_GTT_USAGE);

         uint64_t total_usage = MAX2(total_internal_usage, total_system_usage);

         /* Compute the total free space that can be allocated for this process across all heaps. */
         uint64_t total_free_space = total_heap_size - MIN2(total_heap_size, total_usage);

         /* Compute the remaining visible VRAM size for this process. */
         uint64_t vram_vis_free_space = vram_vis_heap_size - MIN2(vram_vis_heap_size, vram_vis_internal_usage);

         /* Distribute the total free space (2/3rd as VRAM and 1/3rd as GTT) to match the heap
          * sizes, and align down to the page size to be conservative.
          */
         vram_vis_free_space =
            ROUND_DOWN_TO(MIN2((total_free_space * 2) / 3, vram_vis_free_space), pdev->info.gart_page_size);
         uint64_t gtt_free_space = total_free_space - vram_vis_free_space;

         memoryBudget->heapBudget[vram_vis_heap_idx] = vram_vis_free_space + vram_vis_internal_usage;
         memoryBudget->heapUsage[vram_vis_heap_idx] = vram_vis_internal_usage;
         memoryBudget->heapBudget[gtt_heap_idx] = gtt_free_space + gtt_internal_usage;
         memoryBudget->heapUsage[gtt_heap_idx] = gtt_internal_usage;
      }
   } else {
      unsigned mask = pdev->heaps;
      unsigned heap = 0;
      while (mask) {
         uint64_t internal_usage = 0, system_usage = 0;
         unsigned type = 1u << u_bit_scan(&mask);

         switch (type) {
         case RADV_HEAP_VRAM:
            internal_usage = pdev->ws->query_value(pdev->ws, RADEON_ALLOCATED_VRAM);
            system_usage = pdev->ws->query_value(pdev->ws, RADEON_VRAM_USAGE);
            break;
         case RADV_HEAP_VRAM_VIS:
            internal_usage = pdev->ws->query_value(pdev->ws, RADEON_ALLOCATED_VRAM_VIS);
            if (!(pdev->heaps & RADV_HEAP_VRAM))
               internal_usage += pdev->ws->query_value(pdev->ws, RADEON_ALLOCATED_VRAM);
            system_usage = pdev->ws->query_value(pdev->ws, RADEON_VRAM_VIS_USAGE);
            break;
         case RADV_HEAP_GTT:
            internal_usage = pdev->ws->query_value(pdev->ws, RADEON_ALLOCATED_GTT);
            system_usage = pdev->ws->query_value(pdev->ws, RADEON_GTT_USAGE);
            break;
         }

         uint64_t total_usage = MAX2(internal_usage, system_usage);

         uint64_t free_space = pdev->memory_properties.memoryHeaps[heap].size -
                               MIN2(pdev->memory_properties.memoryHeaps[heap].size, total_usage);
         memoryBudget->heapBudget[heap] = free_space + internal_usage;
         memoryBudget->heapUsage[heap] = internal_usage;
         ++heap;
      }

      assert(heap == memory_properties->memoryHeapCount);
   }

   /* The heapBudget value must be less than or equal to VkMemoryHeap::size for each heap. */
   for (uint32_t i = 0; i < memory_properties->memoryHeapCount; i++) {
      memoryBudget->heapBudget[i] = MIN2(memory_properties->memoryHeaps[i].size, memoryBudget->heapBudget[i]);
   }

   /* The heapBudget and heapUsage values must be zero for array elements
    * greater than or equal to
    * VkPhysicalDeviceMemoryProperties::memoryHeapCount.
    */
   for (uint32_t i = memory_properties->memoryHeapCount; i < VK_MAX_MEMORY_HEAPS; i++) {
      memoryBudget->heapBudget[i] = 0;
      memoryBudget->heapUsage[i] = 0;
   }
}

VKAPI_ATTR void VKAPI_CALL
radv_GetPhysicalDeviceMemoryProperties2(VkPhysicalDevice physicalDevice,
                                        VkPhysicalDeviceMemoryProperties2 *pMemoryProperties)
{
   VK_FROM_HANDLE(radv_physical_device, pdev, physicalDevice);

   pMemoryProperties->memoryProperties = pdev->memory_properties;

   VkPhysicalDeviceMemoryBudgetPropertiesEXT *memory_budget =
      vk_find_struct(pMemoryProperties->pNext, PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT);
   if (memory_budget)
      radv_get_memory_budget_properties(physicalDevice, memory_budget);
}

static const VkTimeDomainKHR radv_time_domains[] = {
   VK_TIME_DOMAIN_DEVICE_KHR,
   VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR,
#ifdef CLOCK_MONOTONIC_RAW
   VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_KHR,
#endif
};

VKAPI_ATTR VkResult VKAPI_CALL
radv_GetPhysicalDeviceCalibrateableTimeDomainsKHR(VkPhysicalDevice physicalDevice, uint32_t *pTimeDomainCount,
                                                  VkTimeDomainKHR *pTimeDomains)
{
   int d;
   VK_OUTARRAY_MAKE_TYPED(VkTimeDomainKHR, out, pTimeDomains, pTimeDomainCount);

   for (d = 0; d < ARRAY_SIZE(radv_time_domains); d++) {
      vk_outarray_append_typed(VkTimeDomainKHR, &out, i)
      {
         *i = radv_time_domains[d];
      }
   }

   return vk_outarray_status(&out);
}

VKAPI_ATTR void VKAPI_CALL
radv_GetPhysicalDeviceMultisamplePropertiesEXT(VkPhysicalDevice physicalDevice, VkSampleCountFlagBits samples,
                                               VkMultisamplePropertiesEXT *pMultisampleProperties)
{
   VkSampleCountFlagBits supported_samples = VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_4_BIT | VK_SAMPLE_COUNT_8_BIT;

   if (samples & supported_samples) {
      pMultisampleProperties->maxSampleLocationGridSize = (VkExtent2D){2, 2};
   } else {
      pMultisampleProperties->maxSampleLocationGridSize = (VkExtent2D){0, 0};
   }
}

VKAPI_ATTR VkResult VKAPI_CALL
radv_GetPhysicalDeviceFragmentShadingRatesKHR(VkPhysicalDevice physicalDevice, uint32_t *pFragmentShadingRateCount,
                                              VkPhysicalDeviceFragmentShadingRateKHR *pFragmentShadingRates)
{
   VK_OUTARRAY_MAKE_TYPED(VkPhysicalDeviceFragmentShadingRateKHR, out, pFragmentShadingRates,
                          pFragmentShadingRateCount);

#define append_rate(w, h, s)                                                                                           \
   {                                                                                                                   \
      VkPhysicalDeviceFragmentShadingRateKHR rate = {                                                                  \
         .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR,                              \
         .sampleCounts = s,                                                                                            \
         .fragmentSize = {.width = w, .height = h},                                                                    \
      };                                                                                                               \
      vk_outarray_append_typed(VkPhysicalDeviceFragmentShadingRateKHR, &out, r) *r = rate;                             \
   }

   for (uint32_t x = 2; x >= 1; x--) {
      for (uint32_t y = 2; y >= 1; y--) {
         VkSampleCountFlagBits samples;

         if (x == 1 && y == 1) {
            samples = ~0;
         } else {
            samples = VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_4_BIT | VK_SAMPLE_COUNT_8_BIT;
         }

         append_rate(x, y, samples);
      }
   }
#undef append_rate

   return vk_outarray_status(&out);
}

/* VK_EXT_tooling_info */
VKAPI_ATTR VkResult VKAPI_CALL
radv_GetPhysicalDeviceToolProperties(VkPhysicalDevice physicalDevice, uint32_t *pToolCount,
                                     VkPhysicalDeviceToolProperties *pToolProperties)
{
   VK_FROM_HANDLE(radv_physical_device, pdev, physicalDevice);
   const struct radv_instance *instance = radv_physical_device_instance(pdev);
   VK_OUTARRAY_MAKE_TYPED(VkPhysicalDeviceToolProperties, out, pToolProperties, pToolCount);
   bool rgp_enabled, rmv_enabled, rra_enabled;
   uint32_t tool_count = 0;

   /* RGP */
   rgp_enabled = instance->vk.trace_mode & RADV_TRACE_MODE_RGP;
   if (rgp_enabled)
      tool_count++;

   /* RMV */
   rmv_enabled = instance->vk.trace_mode & VK_TRACE_MODE_RMV;
   if (rmv_enabled)
      tool_count++;

   /* RRA */
   rra_enabled = instance->vk.trace_mode & RADV_TRACE_MODE_RRA;
   if (rra_enabled)
      tool_count++;

   if (!pToolProperties) {
      *pToolCount = tool_count;
      return VK_SUCCESS;
   }

   if (rgp_enabled) {
      VkPhysicalDeviceToolProperties tool = {
         .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TOOL_PROPERTIES,
         .name = "Radeon GPU Profiler",
         .version = "1.15",
         .description = "A ground-breaking low-level optimization tool that provides detailed "
                        "information on Radeon GPUs.",
         .purposes = VK_TOOL_PURPOSE_PROFILING_BIT | VK_TOOL_PURPOSE_TRACING_BIT |
                     /* VK_EXT_debug_marker is only exposed if SQTT is enabled. */
                     VK_TOOL_PURPOSE_ADDITIONAL_FEATURES_BIT | VK_TOOL_PURPOSE_DEBUG_MARKERS_BIT_EXT,
      };
      vk_outarray_append_typed(VkPhysicalDeviceToolProperties, &out, t) *t = tool;
   }

   if (rmv_enabled) {
      VkPhysicalDeviceToolProperties tool = {
         .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TOOL_PROPERTIES,
         .name = "Radeon Memory Visualizer",
         .version = "1.6",
         .description = "A tool to allow you to gain a deep understanding of how your application "
                        "uses memory for graphics resources.",
         .purposes = VK_TOOL_PURPOSE_PROFILING_BIT | VK_TOOL_PURPOSE_TRACING_BIT,
      };
      vk_outarray_append_typed(VkPhysicalDeviceToolProperties, &out, t) *t = tool;
   }

   if (rra_enabled) {
      VkPhysicalDeviceToolProperties tool = {
         .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TOOL_PROPERTIES,
         .name = "Radeon Raytracing Analyzer",
         .version = "1.2",
         .description = "A tool to investigate the performance of your ray tracing applications and "
                        "highlight potential bottlenecks.",
         .purposes = VK_TOOL_PURPOSE_PROFILING_BIT | VK_TOOL_PURPOSE_TRACING_BIT,
      };
      vk_outarray_append_typed(VkPhysicalDeviceToolProperties, &out, t) *t = tool;
   }

   return vk_outarray_status(&out);
}

VKAPI_ATTR VkResult VKAPI_CALL
radv_GetPhysicalDeviceCooperativeMatrixPropertiesKHR(VkPhysicalDevice physicalDevice, uint32_t *pPropertyCount,
                                                     VkCooperativeMatrixPropertiesKHR *pProperties)
{
   VK_OUTARRAY_MAKE_TYPED(VkCooperativeMatrixPropertiesKHR, out, pProperties, pPropertyCount);

   vk_outarray_append_typed(VkCooperativeMatrixPropertiesKHR, &out, p)
   {
      *p = (struct VkCooperativeMatrixPropertiesKHR){.sType = VK_STRUCTURE_TYPE_COOPERATIVE_MATRIX_PROPERTIES_KHR,
                                                     .MSize = 16,
                                                     .NSize = 16,
                                                     .KSize = 16,
                                                     .AType = VK_COMPONENT_TYPE_FLOAT16_KHR,
                                                     .BType = VK_COMPONENT_TYPE_FLOAT16_KHR,
                                                     .CType = VK_COMPONENT_TYPE_FLOAT16_KHR,
                                                     .ResultType = VK_COMPONENT_TYPE_FLOAT16_KHR,
                                                     .saturatingAccumulation = false,
                                                     .scope = VK_SCOPE_SUBGROUP_KHR};
   }

   vk_outarray_append_typed(VkCooperativeMatrixPropertiesKHR, &out, p)
   {
      *p = (struct VkCooperativeMatrixPropertiesKHR){.sType = VK_STRUCTURE_TYPE_COOPERATIVE_MATRIX_PROPERTIES_KHR,
                                                     .MSize = 16,
                                                     .NSize = 16,
                                                     .KSize = 16,
                                                     .AType = VK_COMPONENT_TYPE_FLOAT16_KHR,
                                                     .BType = VK_COMPONENT_TYPE_FLOAT16_KHR,
                                                     .CType = VK_COMPONENT_TYPE_FLOAT32_KHR,
                                                     .ResultType = VK_COMPONENT_TYPE_FLOAT32_KHR,
                                                     .saturatingAccumulation = false,
                                                     .scope = VK_SCOPE_SUBGROUP_KHR};
   }

   for (unsigned asigned = 0; asigned < 2; asigned++) {
      for (unsigned bsigned = 0; bsigned < 2; bsigned++) {
         for (unsigned csigned = 0; csigned < 2; csigned++) {
            for (unsigned saturate = 0; saturate < 2; saturate++) {
               if (!csigned && saturate)
                  continue; /* The HW only supports signed acc. */
               vk_outarray_append_typed(VkCooperativeMatrixPropertiesKHR, &out, p)
               {
                  *p = (struct VkCooperativeMatrixPropertiesKHR){
                     .sType = VK_STRUCTURE_TYPE_COOPERATIVE_MATRIX_PROPERTIES_KHR,
                     .MSize = 16,
                     .NSize = 16,
                     .KSize = 16,
                     .AType = asigned ? VK_COMPONENT_TYPE_SINT8_KHR : VK_COMPONENT_TYPE_UINT8_KHR,
                     .BType = bsigned ? VK_COMPONENT_TYPE_SINT8_KHR : VK_COMPONENT_TYPE_UINT8_KHR,
                     .CType = csigned ? VK_COMPONENT_TYPE_SINT32_KHR : VK_COMPONENT_TYPE_UINT32_KHR,
                     .ResultType = csigned ? VK_COMPONENT_TYPE_SINT32_KHR : VK_COMPONENT_TYPE_UINT32_KHR,
                     .saturatingAccumulation = saturate,
                     .scope = VK_SCOPE_SUBGROUP_KHR};
               }
            }
         }
      }
   }

   return vk_outarray_status(&out);
}
