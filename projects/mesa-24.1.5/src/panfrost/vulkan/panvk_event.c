/*
 * Copyright © 2021 Collabora Ltd.
 * SPDX-License-Identifier: MIT
 */

#include "panvk_event.h"
#include "panvk_device.h"
#include "panvk_entrypoints.h"

#include "vk_log.h"

VKAPI_ATTR VkResult VKAPI_CALL
panvk_CreateEvent(VkDevice _device, const VkEventCreateInfo *pCreateInfo,
                  const VkAllocationCallbacks *pAllocator, VkEvent *pEvent)
{
   VK_FROM_HANDLE(panvk_device, device, _device);
   struct panvk_event *event = vk_object_zalloc(
      &device->vk, pAllocator, sizeof(*event), VK_OBJECT_TYPE_EVENT);
   if (!event)
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   struct drm_syncobj_create create = {
      .flags = 0,
   };

   int ret = drmIoctl(device->vk.drm_fd, DRM_IOCTL_SYNCOBJ_CREATE, &create);
   if (ret)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   event->syncobj = create.handle;
   *pEvent = panvk_event_to_handle(event);

   return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
panvk_DestroyEvent(VkDevice _device, VkEvent _event,
                   const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(panvk_device, device, _device);
   VK_FROM_HANDLE(panvk_event, event, _event);

   if (!event)
      return;

   struct drm_syncobj_destroy destroy = {.handle = event->syncobj};
   drmIoctl(device->vk.drm_fd, DRM_IOCTL_SYNCOBJ_DESTROY, &destroy);

   vk_object_free(&device->vk, pAllocator, event);
}

VKAPI_ATTR VkResult VKAPI_CALL
panvk_GetEventStatus(VkDevice _device, VkEvent _event)
{
   VK_FROM_HANDLE(panvk_device, device, _device);
   VK_FROM_HANDLE(panvk_event, event, _event);
   bool signaled;

   struct drm_syncobj_wait wait = {
      .handles = (uintptr_t)&event->syncobj,
      .count_handles = 1,
      .timeout_nsec = 0,
      .flags = DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT,
   };

   int ret = drmIoctl(device->vk.drm_fd, DRM_IOCTL_SYNCOBJ_WAIT, &wait);
   if (ret) {
      if (errno == ETIME)
         signaled = false;
      else {
         assert(0);
         return VK_ERROR_DEVICE_LOST; /* TODO */
      }
   } else
      signaled = true;

   return signaled ? VK_EVENT_SET : VK_EVENT_RESET;
}

VKAPI_ATTR VkResult VKAPI_CALL
panvk_SetEvent(VkDevice _device, VkEvent _event)
{
   VK_FROM_HANDLE(panvk_device, device, _device);
   VK_FROM_HANDLE(panvk_event, event, _event);

   struct drm_syncobj_array objs = {
      .handles = (uint64_t)(uintptr_t)&event->syncobj,
      .count_handles = 1};

   /* This is going to just replace the fence for this syncobj with one that
    * is already in signaled state. This won't be a problem because the spec
    * mandates that the event will have been set before the vkCmdWaitEvents
    * command executes.
    * https://www.khronos.org/registry/vulkan/specs/1.2/html/chap6.html#commandbuffers-submission-progress
    */
   if (drmIoctl(device->vk.drm_fd, DRM_IOCTL_SYNCOBJ_SIGNAL, &objs))
      return VK_ERROR_DEVICE_LOST;

   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
panvk_ResetEvent(VkDevice _device, VkEvent _event)
{
   VK_FROM_HANDLE(panvk_device, device, _device);
   VK_FROM_HANDLE(panvk_event, event, _event);

   struct drm_syncobj_array objs = {
      .handles = (uint64_t)(uintptr_t)&event->syncobj,
      .count_handles = 1};

   if (drmIoctl(device->vk.drm_fd, DRM_IOCTL_SYNCOBJ_RESET, &objs))
      return VK_ERROR_DEVICE_LOST;

   return VK_SUCCESS;
}
