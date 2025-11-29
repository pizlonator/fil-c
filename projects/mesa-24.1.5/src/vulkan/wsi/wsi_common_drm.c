/*
 * Copyright © 2017 Intel Corporation
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

#include "wsi_common_private.h"
#include "wsi_common_drm.h"
#include "util/macros.h"
#include "util/os_file.h"
#include "util/log.h"
#include "util/xmlconfig.h"
#include "vk_device.h"
#include "vk_physical_device.h"
#include "vk_log.h"
#include "vk_util.h"
#include "drm-uapi/drm_fourcc.h"
#include "drm-uapi/dma-buf.h"
#include "util/libsync.h"

#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <xf86drm.h>

static VkResult
wsi_dma_buf_export_sync_file(int dma_buf_fd, int *sync_file_fd)
{
   /* Don't keep trying an IOCTL that doesn't exist. */
   static bool no_dma_buf_sync_file = false;
   if (no_dma_buf_sync_file)
      return VK_ERROR_FEATURE_NOT_PRESENT;

   struct dma_buf_export_sync_file export = {
      .flags = DMA_BUF_SYNC_RW,
      .fd = -1,
   };
   int ret = drmIoctl(dma_buf_fd, DMA_BUF_IOCTL_EXPORT_SYNC_FILE, &export);
   if (ret) {
      if (errno == ENOTTY || errno == EBADF || errno == ENOSYS) {
         no_dma_buf_sync_file = true;
         return VK_ERROR_FEATURE_NOT_PRESENT;
      } else {
         mesa_loge("MESA: failed to export sync file '%s'", strerror(errno));
         return VK_ERROR_OUT_OF_HOST_MEMORY;
      }
   }

   *sync_file_fd = export.fd;

   return VK_SUCCESS;
}

static VkResult
wsi_dma_buf_import_sync_file(int dma_buf_fd, int sync_file_fd)
{
   /* Don't keep trying an IOCTL that doesn't exist. */
   static bool no_dma_buf_sync_file = false;
   if (no_dma_buf_sync_file)
      return VK_ERROR_FEATURE_NOT_PRESENT;

   struct dma_buf_import_sync_file import = {
      .flags = DMA_BUF_SYNC_RW,
      .fd = sync_file_fd,
   };
   int ret = drmIoctl(dma_buf_fd, DMA_BUF_IOCTL_IMPORT_SYNC_FILE, &import);
   if (ret) {
      if (errno == ENOTTY || errno == EBADF || errno == ENOSYS) {
         no_dma_buf_sync_file = true;
         return VK_ERROR_FEATURE_NOT_PRESENT;
      } else {
         mesa_loge("MESA: failed to import sync file '%s'", strerror(errno));
         return VK_ERROR_OUT_OF_HOST_MEMORY;
      }
   }

   return VK_SUCCESS;
}

static VkResult
prepare_signal_dma_buf_from_semaphore(struct wsi_swapchain *chain,
                                      const struct wsi_image *image)
{
   VkResult result;

   if (!(chain->wsi->semaphore_export_handle_types &
         VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT))
      return VK_ERROR_FEATURE_NOT_PRESENT;

   int sync_file_fd = -1;
   result = wsi_dma_buf_export_sync_file(image->dma_buf_fd, &sync_file_fd);
   if (result != VK_SUCCESS)
      return result;

   result = wsi_dma_buf_import_sync_file(image->dma_buf_fd, sync_file_fd);
   close(sync_file_fd);
   if (result != VK_SUCCESS)
      return result;

   /* If we got here, all our checks pass.  Create the actual semaphore */
   const VkExportSemaphoreCreateInfo export_info = {
      .sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
      .handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT,
   };
   const VkSemaphoreCreateInfo semaphore_info = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = &export_info,
   };
   result = chain->wsi->CreateSemaphore(chain->device, &semaphore_info,
                                        &chain->alloc,
                                        &chain->dma_buf_semaphore);
   if (result != VK_SUCCESS)
      return result;

   return VK_SUCCESS;
}

VkResult
wsi_prepare_signal_dma_buf_from_semaphore(struct wsi_swapchain *chain,
                                          const struct wsi_image *image)
{
   VkResult result;

   /* We cache result - 1 in the swapchain */
   if (unlikely(chain->signal_dma_buf_from_semaphore == 0)) {
      result = prepare_signal_dma_buf_from_semaphore(chain, image);
      assert(result <= 0);
      chain->signal_dma_buf_from_semaphore = (int)result - 1;
   } else {
      result = (VkResult)(chain->signal_dma_buf_from_semaphore + 1);
   }

   return result;
}

VkResult
wsi_signal_dma_buf_from_semaphore(const struct wsi_swapchain *chain,
                                  const struct wsi_image *image)
{
   VkResult result;

   const VkSemaphoreGetFdInfoKHR get_fd_info = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR,
      .semaphore = chain->dma_buf_semaphore,
      .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT,
   };
   int sync_file_fd = -1;
   result = chain->wsi->GetSemaphoreFdKHR(chain->device, &get_fd_info,
                                          &sync_file_fd);
   if (result != VK_SUCCESS)
      return result;

   result = wsi_dma_buf_import_sync_file(image->dma_buf_fd, sync_file_fd);
   close(sync_file_fd);
   return result;
}

static const struct vk_sync_type *
get_sync_file_sync_type(struct vk_device *device,
                        enum vk_sync_features req_features)
{
   for (const struct vk_sync_type *const *t =
        device->physical->supported_sync_types; *t; t++) {
      if (req_features & ~(*t)->features)
         continue;

      if ((*t)->import_sync_file != NULL)
         return *t;
   }

   return NULL;
}

VkResult
wsi_create_sync_for_dma_buf_wait(const struct wsi_swapchain *chain,
                                 const struct wsi_image *image,
                                 enum vk_sync_features req_features,
                                 struct vk_sync **sync_out)
{
   VK_FROM_HANDLE(vk_device, device, chain->device);
   VkResult result;

   const struct vk_sync_type *sync_type =
      get_sync_file_sync_type(device, req_features);
   if (sync_type == NULL)
      return VK_ERROR_FEATURE_NOT_PRESENT;

   int sync_file_fd = -1;
   result = wsi_dma_buf_export_sync_file(image->dma_buf_fd, &sync_file_fd);
   if (result != VK_SUCCESS)
      return result;

   struct vk_sync *sync = NULL;
   result = vk_sync_create(device, sync_type, VK_SYNC_IS_SHAREABLE, 0, &sync);
   if (result != VK_SUCCESS)
      goto fail_close_sync_file;

   result = vk_sync_import_sync_file(device, sync, sync_file_fd);
   if (result != VK_SUCCESS)
      goto fail_destroy_sync;

   close(sync_file_fd);
   *sync_out = sync;

   return VK_SUCCESS;

fail_destroy_sync:
   vk_sync_destroy(device, sync);
fail_close_sync_file:
   close(sync_file_fd);

   return result;
}

VkResult
wsi_create_image_explicit_sync_drm(const struct wsi_swapchain *chain,
                                   struct wsi_image *image)
{
   /* Cleanup of any failures is handled by the caller in wsi_create_image
    * calling wsi_destroy_image -> wsi_destroy_image_explicit_sync_drm. */

   VK_FROM_HANDLE(vk_device, device, chain->device);
   const struct wsi_device *wsi = chain->wsi;
   VkResult result = VK_SUCCESS;
   int ret = 0;

   const VkExportSemaphoreCreateInfo semaphore_export_info = {
      .sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
      /* This is a syncobj fd for any drivers using syncobj. */
      .handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT,
   };

   const VkSemaphoreTypeCreateInfo semaphore_type_info = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
      .pNext = &semaphore_export_info,
      .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
   };

   const VkSemaphoreCreateInfo semaphore_info = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = &semaphore_type_info,
   };

   for (uint32_t i = 0; i < WSI_ES_COUNT; i++) {
      result = wsi->CreateSemaphore(chain->device,
                                    &semaphore_info,
                                    &chain->alloc,
                                    &image->explicit_sync[i].semaphore);
      if (result != VK_SUCCESS)
         return result;

      const VkSemaphoreGetFdInfoKHR semaphore_get_info = {
         .sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR,
         .semaphore = image->explicit_sync[i].semaphore,
         .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT,
      };

      result = wsi->GetSemaphoreFdKHR(chain->device, &semaphore_get_info, &image->explicit_sync[i].fd);
      if (result != VK_SUCCESS)
         return result;
   }

   for (uint32_t i = 0; i < WSI_ES_COUNT; i++) {
      ret = drmSyncobjFDToHandle(device->drm_fd, image->explicit_sync[i].fd, &image->explicit_sync[i].handle);
      if (ret != 0)
         return VK_ERROR_FEATURE_NOT_PRESENT;
   }

   return VK_SUCCESS;
}

void
wsi_destroy_image_explicit_sync_drm(const struct wsi_swapchain *chain,
                                    struct wsi_image *image)
{
   VK_FROM_HANDLE(vk_device, device, chain->device);
   const struct wsi_device *wsi = chain->wsi;

   for (uint32_t i = 0; i < WSI_ES_COUNT; i++) {
      if (image->explicit_sync[i].handle != 0) {
         drmSyncobjDestroy(device->drm_fd, image->explicit_sync[i].handle);
         image->explicit_sync[i].handle = 0;
      }

      if (image->explicit_sync[i].fd >= 0) {
         close(image->explicit_sync[i].fd);
         image->explicit_sync[i].fd = -1;
      }

      if (image->explicit_sync[i].semaphore != VK_NULL_HANDLE) {
         wsi->DestroySemaphore(chain->device, image->explicit_sync[i].semaphore, &chain->alloc);
         image->explicit_sync[i].semaphore = VK_NULL_HANDLE;
      }
   }
}

static VkResult
wsi_create_sync_imm(struct vk_device *device, struct vk_sync **sync_out)
{
   const struct vk_sync_type *sync_type =
      get_sync_file_sync_type(device, VK_SYNC_FEATURE_CPU_WAIT);
   struct vk_sync *sync = NULL;
   VkResult result;

   result = vk_sync_create(device, sync_type, VK_SYNC_IS_SHAREABLE, 0, &sync);
   if (result != VK_SUCCESS)
      goto error;

   result = vk_sync_signal(device, sync, 0);
   if (result != VK_SUCCESS)
      goto error;

   *sync_out = sync;
   goto done;

error:
   vk_sync_destroy(device, sync);
done:
   return result;
}

VkResult
wsi_create_sync_for_image_syncobj(const struct wsi_swapchain *chain,
                                  const struct wsi_image *image,
                                  enum vk_sync_features req_features,
                                  struct vk_sync **sync_out)
{
   VK_FROM_HANDLE(vk_device, device, chain->device);
   const struct vk_sync_type *sync_type =
      get_sync_file_sync_type(device, VK_SYNC_FEATURE_CPU_WAIT);
   VkResult result = VK_SUCCESS;
   struct vk_sync *sync = NULL;
   int sync_file_fds[WSI_ES_COUNT] = { -1, -1 };
   uint32_t tmp_handles[WSI_ES_COUNT] = { 0, 0 };
   int merged_sync_fd = -1;
   if (sync_type == NULL)
      return VK_ERROR_FEATURE_NOT_PRESENT;

   if (image->explicit_sync[WSI_ES_RELEASE].timeline == 0) {
      /* Signal immediately, there is no release to forward. */
      return wsi_create_sync_imm(device, sync_out);
   }

   /* Transfer over to a new sync file with a
    * surrogate handle.
    */
   for (uint32_t i = 0; i < WSI_ES_COUNT; i++) {
      if (drmSyncobjCreate(device->drm_fd, 0, &tmp_handles[i])) {
         result = vk_errorf(NULL, VK_ERROR_OUT_OF_DEVICE_MEMORY, "Failed to create temp syncobj. Errno: %d - %s", errno, strerror(errno));
         goto fail;
      }

      if (drmSyncobjTransfer(device->drm_fd, tmp_handles[i], 0,
                             image->explicit_sync[i].handle, image->explicit_sync[i].timeline, 0)) {
         result = vk_errorf(NULL, VK_ERROR_OUT_OF_DEVICE_MEMORY, "Failed to transfer syncobj. Was the timeline point materialized? Errno: %d - %s", errno, strerror(errno));
         goto fail;
      }
      if (drmSyncobjExportSyncFile(device->drm_fd, tmp_handles[i], &sync_file_fds[i])) {
         result = vk_errorf(NULL, VK_ERROR_OUT_OF_DEVICE_MEMORY, "Failed to export sync file. Errno: %d - %s", errno, strerror(errno));
         goto fail;
      }
   }

   merged_sync_fd = sync_merge("acquire merged sync", sync_file_fds[WSI_ES_ACQUIRE], sync_file_fds[WSI_ES_RELEASE]);
   if (merged_sync_fd < 0) {
      result = vk_errorf(NULL, VK_ERROR_OUT_OF_DEVICE_MEMORY, "Failed to merge acquire + release sync timelines. Errno: %d - %s", errno, strerror(errno));
      goto fail;
   }

   result = vk_sync_create(device, sync_type, VK_SYNC_IS_SHAREABLE, 0, &sync);
   if (result != VK_SUCCESS)
      goto fail;

   result = vk_sync_import_sync_file(device, sync, merged_sync_fd);
   if (result != VK_SUCCESS)
      goto fail;

   *sync_out = sync;
   goto done;

fail:
   if (sync)
      vk_sync_destroy(device, sync);
done:
   for (uint32_t i = 0; i < WSI_ES_COUNT; i++) {
      if (tmp_handles[i])
         drmSyncobjDestroy(device->drm_fd, tmp_handles[i]);
   }
   for (uint32_t i = 0; i < WSI_ES_COUNT; i++) {
      if (sync_file_fds[i] >= 0)
         close(sync_file_fds[i]);
   }
   if (merged_sync_fd >= 0)
      close(merged_sync_fd);
   return result;
}


bool
wsi_common_drm_devices_equal(int fd_a, int fd_b)
{
   drmDevicePtr device_a, device_b;
   int ret;

   ret = drmGetDevice2(fd_a, 0, &device_a);
   if (ret)
      return false;

   ret = drmGetDevice2(fd_b, 0, &device_b);
   if (ret) {
      drmFreeDevice(&device_a);
      return false;
   }

   bool result = drmDevicesEqual(device_a, device_b);

   drmFreeDevice(&device_a);
   drmFreeDevice(&device_b);

   return result;
}

bool
wsi_device_matches_drm_fd(const struct wsi_device *wsi, int drm_fd)
{
   if (wsi->can_present_on_device)
      return wsi->can_present_on_device(wsi->pdevice, drm_fd);

   drmDevicePtr fd_device;
   int ret = drmGetDevice2(drm_fd, 0, &fd_device);
   if (ret)
      return false;

   bool match = false;
   switch (fd_device->bustype) {
   case DRM_BUS_PCI:
      match = wsi->pci_bus_info.pciDomain == fd_device->businfo.pci->domain &&
              wsi->pci_bus_info.pciBus == fd_device->businfo.pci->bus &&
              wsi->pci_bus_info.pciDevice == fd_device->businfo.pci->dev &&
              wsi->pci_bus_info.pciFunction == fd_device->businfo.pci->func;
      break;

   default:
      break;
   }

   drmFreeDevice(&fd_device);

   return match;
}

static uint32_t
prime_select_buffer_memory_type(const struct wsi_device *wsi,
                                uint32_t type_bits)
{
   return wsi_select_memory_type(wsi, 0 /* req_props */,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                 type_bits);
}

static const struct VkDrmFormatModifierPropertiesEXT *
get_modifier_props(const struct wsi_image_info *info, uint64_t modifier)
{
   for (uint32_t i = 0; i < info->modifier_prop_count; i++) {
      if (info->modifier_props[i].drmFormatModifier == modifier)
         return &info->modifier_props[i];
   }
   return NULL;
}

static VkResult
wsi_create_native_image_mem(const struct wsi_swapchain *chain,
                            const struct wsi_image_info *info,
                            struct wsi_image *image);

static VkResult
wsi_configure_native_image(const struct wsi_swapchain *chain,
                           const VkSwapchainCreateInfoKHR *pCreateInfo,
                           const struct wsi_drm_image_params *params,
                           struct wsi_image_info *info)
{
   const struct wsi_device *wsi = chain->wsi;

   VkExternalMemoryHandleTypeFlags handle_type =
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

   VkResult result = wsi_configure_image(chain, pCreateInfo, handle_type, info);
   if (result != VK_SUCCESS)
      return result;

   info->explicit_sync = params->explicit_sync;

   if (params->num_modifier_lists == 0) {
      /* If we don't have modifiers, fall back to the legacy "scanout" flag */
      info->wsi.scanout = true;
   } else {
      /* The winsys can't request modifiers if we don't support them. */
      assert(wsi->supports_modifiers);
      struct VkDrmFormatModifierPropertiesListEXT modifier_props_list = {
         .sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT,
      };
      VkFormatProperties2 format_props = {
         .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
         .pNext = &modifier_props_list,
      };
      wsi->GetPhysicalDeviceFormatProperties2(wsi->pdevice,
                                              pCreateInfo->imageFormat,
                                              &format_props);
      assert(modifier_props_list.drmFormatModifierCount > 0);
      info->modifier_props =
         vk_alloc(&chain->alloc,
                  sizeof(*info->modifier_props) *
                  modifier_props_list.drmFormatModifierCount,
                  8, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
      if (info->modifier_props == NULL)
         goto fail_oom;

      modifier_props_list.pDrmFormatModifierProperties = info->modifier_props;
      wsi->GetPhysicalDeviceFormatProperties2(wsi->pdevice,
                                              pCreateInfo->imageFormat,
                                              &format_props);

      /* Call GetImageFormatProperties with every modifier and filter the list
       * down to those that we know work.
       */
      info->modifier_prop_count = 0;
      for (uint32_t i = 0; i < modifier_props_list.drmFormatModifierCount; i++) {
         VkPhysicalDeviceImageDrmFormatModifierInfoEXT mod_info = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT,
            .drmFormatModifier = info->modifier_props[i].drmFormatModifier,
            .sharingMode = pCreateInfo->imageSharingMode,
            .queueFamilyIndexCount = pCreateInfo->queueFamilyIndexCount,
            .pQueueFamilyIndices = pCreateInfo->pQueueFamilyIndices,
         };
         VkPhysicalDeviceImageFormatInfo2 format_info = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
            .format = pCreateInfo->imageFormat,
            .type = VK_IMAGE_TYPE_2D,
            .tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
            .usage = pCreateInfo->imageUsage,
            .flags = info->create.flags,
         };

         VkImageFormatListCreateInfo format_list;
         if (info->create.flags & VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT) {
            format_list = info->format_list;
            format_list.pNext = NULL;
            __vk_append_struct(&format_info, &format_list);
         }

         struct wsi_image_create_info wsi_info = (struct wsi_image_create_info) {
            .sType = VK_STRUCTURE_TYPE_WSI_IMAGE_CREATE_INFO_MESA,
            .pNext = NULL,
         };
         __vk_append_struct(&format_info, &wsi_info);

         VkImageFormatProperties2 format_props = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
            .pNext = NULL,
         };
         __vk_append_struct(&format_info, &mod_info);
         result = wsi->GetPhysicalDeviceImageFormatProperties2(wsi->pdevice,
                                                               &format_info,
                                                               &format_props);
         if (result == VK_SUCCESS &&
             pCreateInfo->imageExtent.width <= format_props.imageFormatProperties.maxExtent.width &&
             pCreateInfo->imageExtent.height <= format_props.imageFormatProperties.maxExtent.height)
            info->modifier_props[info->modifier_prop_count++] = info->modifier_props[i];
      }

      uint32_t max_modifier_count = 0;
      for (uint32_t l = 0; l < params->num_modifier_lists; l++)
         max_modifier_count = MAX2(max_modifier_count, params->num_modifiers[l]);

      uint64_t *image_modifiers =
         vk_alloc(&chain->alloc, sizeof(*image_modifiers) * max_modifier_count,
                  8, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
      if (!image_modifiers)
         goto fail_oom;

      uint32_t image_modifier_count = 0;
      for (uint32_t l = 0; l < params->num_modifier_lists; l++) {
         /* Walk the modifier lists and construct a list of supported
          * modifiers.
          */
         for (uint32_t i = 0; i < params->num_modifiers[l]; i++) {
            if (get_modifier_props(info, params->modifiers[l][i]))
               image_modifiers[image_modifier_count++] = params->modifiers[l][i];
         }

         /* We only want to take the modifiers from the first list */
         if (image_modifier_count > 0)
            break;
      }

      if (image_modifier_count > 0) {
         info->create.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
         info->drm_mod_list = (VkImageDrmFormatModifierListCreateInfoEXT) {
            .sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT,
            .drmFormatModifierCount = image_modifier_count,
            .pDrmFormatModifiers = image_modifiers,
         };
         image_modifiers = NULL;
         __vk_append_struct(&info->create, &info->drm_mod_list);
      } else {
         vk_free(&chain->alloc, image_modifiers);
         /* TODO: Add a proper error here */
         assert(!"Failed to find a supported modifier!  This should never "
                 "happen because LINEAR should always be available");
         goto fail_oom;
      }
   }

   info->create_mem = wsi_create_native_image_mem;

   return VK_SUCCESS;

fail_oom:
   wsi_destroy_image_info(chain, info);
   return VK_ERROR_OUT_OF_HOST_MEMORY;
}

static VkResult
wsi_init_image_dmabuf_fd(const struct wsi_swapchain *chain,
                          struct wsi_image *image,
                          bool linear)
{
   const struct wsi_device *wsi = chain->wsi;
   const VkMemoryGetFdInfoKHR memory_get_fd_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
      .pNext = NULL,
      .memory = linear ? image->blit.memory : image->memory,
      .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
   };

   return wsi->GetMemoryFdKHR(chain->device, &memory_get_fd_info,
                              &image->dma_buf_fd);
}

static VkResult
wsi_create_native_image_mem(const struct wsi_swapchain *chain,
                            const struct wsi_image_info *info,
                            struct wsi_image *image)
{
   const struct wsi_device *wsi = chain->wsi;
   VkResult result;

   VkMemoryRequirements reqs;
   wsi->GetImageMemoryRequirements(chain->device, image->image, &reqs);

   const struct wsi_memory_allocate_info memory_wsi_info = {
      .sType = VK_STRUCTURE_TYPE_WSI_MEMORY_ALLOCATE_INFO_MESA,
      .pNext = NULL,
      .implicit_sync = !info->explicit_sync,
   };
   const VkExportMemoryAllocateInfo memory_export_info = {
      .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
      .pNext = &memory_wsi_info,
      .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
   };
   const VkMemoryDedicatedAllocateInfo memory_dedicated_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
      .pNext = &memory_export_info,
      .image = image->image,
      .buffer = VK_NULL_HANDLE,
   };
   const VkMemoryAllocateInfo memory_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = &memory_dedicated_info,
      .allocationSize = reqs.size,
      .memoryTypeIndex =
         wsi_select_device_memory_type(wsi, reqs.memoryTypeBits),
   };
   result = wsi->AllocateMemory(chain->device, &memory_info,
                                &chain->alloc, &image->memory);
   if (result != VK_SUCCESS)
      return result;

   result = wsi_init_image_dmabuf_fd(chain, image, false);
   if (result != VK_SUCCESS)
      return result;

   if (info->drm_mod_list.drmFormatModifierCount > 0) {
      VkImageDrmFormatModifierPropertiesEXT image_mod_props = {
         .sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT,
      };
      result = wsi->GetImageDrmFormatModifierPropertiesEXT(chain->device,
                                                           image->image,
                                                           &image_mod_props);
      if (result != VK_SUCCESS)
         return result;

      image->drm_modifier = image_mod_props.drmFormatModifier;
      assert(image->drm_modifier != DRM_FORMAT_MOD_INVALID);

      const struct VkDrmFormatModifierPropertiesEXT *mod_props =
         get_modifier_props(info, image->drm_modifier);
      image->num_planes = mod_props->drmFormatModifierPlaneCount;

      for (uint32_t p = 0; p < image->num_planes; p++) {
         const VkImageSubresource image_subresource = {
            .aspectMask = VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT << p,
            .mipLevel = 0,
            .arrayLayer = 0,
         };
         VkSubresourceLayout image_layout;
         wsi->GetImageSubresourceLayout(chain->device, image->image,
                                        &image_subresource, &image_layout);
         image->sizes[p] = image_layout.size;
         image->row_pitches[p] = image_layout.rowPitch;
         image->offsets[p] = image_layout.offset;
      }
   } else {
      const VkImageSubresource image_subresource = {
         .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
         .mipLevel = 0,
         .arrayLayer = 0,
      };
      VkSubresourceLayout image_layout;
      wsi->GetImageSubresourceLayout(chain->device, image->image,
                                     &image_subresource, &image_layout);

      image->drm_modifier = DRM_FORMAT_MOD_INVALID;
      image->num_planes = 1;
      image->sizes[0] = reqs.size;
      image->row_pitches[0] = image_layout.rowPitch;
      image->offsets[0] = 0;
   }

   return VK_SUCCESS;
}

#define WSI_PRIME_LINEAR_STRIDE_ALIGN 256

static VkResult
wsi_create_prime_image_mem(const struct wsi_swapchain *chain,
                           const struct wsi_image_info *info,
                           struct wsi_image *image)
{
   VkResult result =
      wsi_create_buffer_blit_context(chain, info, image,
                                     VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT);
   if (result != VK_SUCCESS)
      return result;

   result = wsi_init_image_dmabuf_fd(chain, image, true);
   if (result != VK_SUCCESS)
      return result;

   image->drm_modifier = info->prime_use_linear_modifier ?
                         DRM_FORMAT_MOD_LINEAR : DRM_FORMAT_MOD_INVALID;

   return VK_SUCCESS;
}

static VkResult
wsi_configure_prime_image(UNUSED const struct wsi_swapchain *chain,
                          const VkSwapchainCreateInfoKHR *pCreateInfo,
                          const struct wsi_drm_image_params *params,
                          struct wsi_image_info *info)
{
   bool use_modifier = params->num_modifier_lists > 0;
   wsi_memory_type_select_cb select_buffer_memory_type =
      params->same_gpu ? wsi_select_device_memory_type :
                           prime_select_buffer_memory_type;

   VkResult result = wsi_configure_image(chain, pCreateInfo,
                                         0 /* handle_types */, info);
   if (result != VK_SUCCESS)
      return result;

   info->explicit_sync = params->explicit_sync;

   wsi_configure_buffer_image(chain, pCreateInfo,
                              WSI_PRIME_LINEAR_STRIDE_ALIGN, 4096,
                              info);
   info->prime_use_linear_modifier = use_modifier;

   info->create_mem = wsi_create_prime_image_mem;
   info->select_blit_dst_memory_type = select_buffer_memory_type;
   info->select_image_memory_type = wsi_select_device_memory_type;

   return VK_SUCCESS;
}

bool
wsi_drm_image_needs_buffer_blit(const struct wsi_device *wsi,
                                const struct wsi_drm_image_params *params)
{
   if (!params->same_gpu)
      return true;

   if (params->num_modifier_lists > 0 || wsi->supports_scanout)
      return false;

   return true;
}

VkResult
wsi_drm_configure_image(const struct wsi_swapchain *chain,
                        const VkSwapchainCreateInfoKHR *pCreateInfo,
                        const struct wsi_drm_image_params *params,
                        struct wsi_image_info *info)
{
   assert(params->base.image_type == WSI_IMAGE_TYPE_DRM);

   if (chain->blit.type == WSI_SWAPCHAIN_BUFFER_BLIT) {
      return wsi_configure_prime_image(chain, pCreateInfo,
                                       params,
                                       info);
   } else {
      return wsi_configure_native_image(chain, pCreateInfo,
                                        params,
                                        info);
   }
}

enum wsi_explicit_sync_state_flags
{
   WSI_ES_STATE_RELEASE_MATERIALIZED = (1u << 0),
   WSI_ES_STATE_RELEASE_SIGNALLED    = (1u << 1),
   WSI_ES_STATE_ACQUIRE_SIGNALLED    = (1u << 2),
};

/* Levels of "freeness"
 * 0 -> Acquire Signalled + Release Signalled
 * 1 -> Acquire Signalled + Release Materialized
 * 2 -> Release Signalled
 * 3 -> Release Materialized
 */
static const uint32_t wsi_explicit_sync_free_levels[] = {
   (WSI_ES_STATE_RELEASE_SIGNALLED | WSI_ES_STATE_RELEASE_MATERIALIZED | WSI_ES_STATE_ACQUIRE_SIGNALLED),
   (WSI_ES_STATE_RELEASE_MATERIALIZED | WSI_ES_STATE_ACQUIRE_SIGNALLED),
   (WSI_ES_STATE_RELEASE_MATERIALIZED | WSI_ES_STATE_RELEASE_SIGNALLED),
   (WSI_ES_STATE_RELEASE_MATERIALIZED),
};

static uint32_t
wsi_drm_image_explicit_sync_state(struct vk_device *device, struct wsi_image *image)
{
   if (image->explicit_sync[WSI_ES_RELEASE].timeline == 0) {
      /* This image has never been used in a timeline.
       * It must be free.
       */
      return WSI_ES_STATE_RELEASE_SIGNALLED | WSI_ES_STATE_RELEASE_MATERIALIZED | WSI_ES_STATE_ACQUIRE_SIGNALLED;
   }

   uint64_t points[WSI_ES_COUNT] = { 0 };
   uint32_t handles[WSI_ES_COUNT] = {
      image->explicit_sync[WSI_ES_ACQUIRE].handle,
      image->explicit_sync[WSI_ES_RELEASE].handle
   };
   int ret = drmSyncobjQuery(device->drm_fd, handles, points, WSI_ES_COUNT);
   if (ret)
      return 0;

   uint32_t flags = 0;
   if (points[WSI_ES_ACQUIRE] >= image->explicit_sync[WSI_ES_ACQUIRE].timeline) {
      flags |= WSI_ES_STATE_ACQUIRE_SIGNALLED;
   }

   if (points[WSI_ES_RELEASE] >= image->explicit_sync[WSI_ES_RELEASE].timeline) {
      flags |= WSI_ES_STATE_RELEASE_SIGNALLED | WSI_ES_STATE_RELEASE_MATERIALIZED;
   } else {
      uint32_t first_signalled;
      ret = drmSyncobjTimelineWait(device->drm_fd, &handles[WSI_ES_RELEASE], &image->explicit_sync[WSI_ES_RELEASE].timeline, 1, 0, DRM_SYNCOBJ_WAIT_FLAGS_WAIT_AVAILABLE, &first_signalled);
      if (ret == 0)
         flags |= WSI_ES_STATE_RELEASE_MATERIALIZED;
   }

   return flags;
}

static uint64_t
wsi_drm_rel_timeout_to_abs(uint64_t rel_timeout_ns)
{
   uint64_t cur_time_ns = os_time_get_nano();

   /* Syncobj timeouts are signed */
   return rel_timeout_ns > INT64_MAX - cur_time_ns
      ? INT64_MAX
      : cur_time_ns + rel_timeout_ns;
}

VkResult
wsi_drm_wait_for_explicit_sync_release(struct wsi_swapchain *chain,
                                       uint32_t image_count,
                                       struct wsi_image **images,
                                       uint64_t rel_timeout_ns,
                                       uint32_t *image_index)
{
   STACK_ARRAY(uint32_t, handles, image_count);
   STACK_ARRAY(uint64_t, points, image_count);
   STACK_ARRAY(uint32_t, indices, image_count);
   STACK_ARRAY(uint32_t, flags, image_count);
   VK_FROM_HANDLE(vk_device, device, chain->device);
   int ret = 0;

   /* We don't need to wait for the merged timeline on the CPU,
    * only on the GPU side of things.
    *
    * We already know that the CPU side for the acquire has materialized,
    * for all images in this array.
    * That's what "busy"/"free" essentially represents.
    */
   uint32_t unacquired_image_count = 0;
   for (uint32_t i = 0; i < image_count; i++) {
      if (images[i]->acquired)
         continue;

      flags[unacquired_image_count] = wsi_drm_image_explicit_sync_state(device, images[i]);
      handles[unacquired_image_count] = images[i]->explicit_sync[WSI_ES_RELEASE].handle;
      points[unacquired_image_count] = images[i]->explicit_sync[WSI_ES_RELEASE].timeline;
      indices[unacquired_image_count] = i;
      unacquired_image_count++;
   }

   /* Handle the case where there are no images to possible acquire. */
   if (!unacquired_image_count) {
      ret = -ETIME;
      goto done;
   }

   /* Find the most optimal image using the free levels above. */
   for (uint32_t free_level_idx = 0; free_level_idx < ARRAY_SIZE(wsi_explicit_sync_free_levels); free_level_idx++) {
      uint32_t free_level = wsi_explicit_sync_free_levels[free_level_idx];

      uint64_t present_serial = UINT64_MAX;
      for (uint32_t i = 0; i < unacquired_image_count; i++) {
         /* Pick the image that was presented longest ago inside
          * of this free level, so it has the highest chance of
          * being totally free the soonest.
          */
         if ((flags[i] & free_level) == free_level &&
             images[indices[i]]->present_serial < present_serial) {
            *image_index = indices[i];
            present_serial = images[indices[i]]->present_serial;
         }
      }
      if (present_serial != UINT64_MAX)
         goto done;
   }

   /* Use DRM_SYNCOBJ_WAIT_FLAGS_WAIT_AVAILABLE so we do not need to wait for the
    * compositor's GPU work to be finished to acquire on the CPU side.
    *
    * We will forward the GPU signal to the VkSemaphore/VkFence of the acquire.
    */
   uint32_t first_signalled;
   ret = drmSyncobjTimelineWait(device->drm_fd, handles, points, unacquired_image_count,
                                wsi_drm_rel_timeout_to_abs(rel_timeout_ns),
                                DRM_SYNCOBJ_WAIT_FLAGS_WAIT_AVAILABLE,
                                &first_signalled);

   /* Return the first image that materialized. */
   if (ret != 0)
      goto done;

   *image_index = indices[first_signalled];
done:
   STACK_ARRAY_FINISH(flags);
   STACK_ARRAY_FINISH(indices);
   STACK_ARRAY_FINISH(points);
   STACK_ARRAY_FINISH(handles);

   if (ret == 0)
      return VK_SUCCESS;
   else if (ret == -ETIME)
      return rel_timeout_ns ? VK_TIMEOUT : VK_NOT_READY;
   else
      return VK_ERROR_OUT_OF_DATE_KHR;
}
