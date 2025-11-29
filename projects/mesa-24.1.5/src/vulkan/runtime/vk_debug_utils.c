/*
 * Copyright © 2021 Intel Corporation
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

#include "vk_debug_utils.h"

#include "vk_common_entrypoints.h"
#include "vk_command_buffer.h"
#include "vk_device.h"
#include "vk_queue.h"
#include "vk_object.h"
#include "vk_alloc.h"
#include "vk_util.h"
#include "stdarg.h"
#include "util/u_dynarray.h"

void
vk_debug_message(struct vk_instance *instance,
                 VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                 VkDebugUtilsMessageTypeFlagsEXT types,
                 const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData)
{
   mtx_lock(&instance->debug_utils.callbacks_mutex);

   list_for_each_entry(struct vk_debug_utils_messenger, messenger,
                       &instance->debug_utils.callbacks, link) {
      if ((messenger->severity & severity) &&
          (messenger->type & types))
         messenger->callback(severity, types, pCallbackData, messenger->data);
   }

   mtx_unlock(&instance->debug_utils.callbacks_mutex);
}

/* This function intended to be used by the drivers to report a
 * message to the special messenger, provided in the pNext chain while
 * creating an instance. It's only meant to be used during
 * vkCreateInstance or vkDestroyInstance calls.
 */
void
vk_debug_message_instance(struct vk_instance *instance,
                          VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                          VkDebugUtilsMessageTypeFlagsEXT types,
                          const char *pMessageIdName,
                          int32_t messageIdNumber,
                          const char *pMessage)
{
   if (list_is_empty(&instance->debug_utils.instance_callbacks))
      return;

   const VkDebugUtilsMessengerCallbackDataEXT cbData = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT,
      .pMessageIdName = pMessageIdName,
      .messageIdNumber = messageIdNumber,
      .pMessage = pMessage,
   };

   list_for_each_entry(struct vk_debug_utils_messenger, messenger,
                       &instance->debug_utils.instance_callbacks, link) {
      if ((messenger->severity & severity) &&
          (messenger->type & types))
         messenger->callback(severity, types, &cbData, messenger->data);
   }
}

void
vk_address_binding_report(struct vk_instance *instance,
                          struct vk_object_base *object, 
                          uint64_t base_address,
                          uint64_t size,
                          VkDeviceAddressBindingTypeEXT type)
{
   if (list_is_empty(&instance->debug_utils.callbacks))
      return;

   VkDeviceAddressBindingCallbackDataEXT addr_binding = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_ADDRESS_BINDING_CALLBACK_DATA_EXT,
      .flags = object->client_visible ? 0 : VK_DEVICE_ADDRESS_BINDING_INTERNAL_OBJECT_BIT_EXT,
      .baseAddress = base_address,
      .size = size,
      .bindingType = type,
   };

   VkDebugUtilsObjectNameInfoEXT object_name_info = {
         .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
         .pNext = NULL,
         .objectType = object->type,
         .objectHandle = (uint64_t)(uintptr_t)object,
         .pObjectName = object->object_name,
   };

   VkDebugUtilsMessengerCallbackDataEXT cb_data = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT,
      .pNext = &addr_binding,
      .objectCount = 1,
      .pObjects = &object_name_info,
   };

   vk_debug_message(instance, VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
                    VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT,
                    &cb_data);
}

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_CreateDebugUtilsMessengerEXT(
   VkInstance _instance,
   const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
   const VkAllocationCallbacks *pAllocator,
   VkDebugUtilsMessengerEXT *pMessenger)
{
   VK_FROM_HANDLE(vk_instance, instance, _instance);

   struct vk_debug_utils_messenger *messenger =
      vk_alloc2(&instance->alloc, pAllocator,
                sizeof(struct vk_debug_utils_messenger), 8,
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

   if (!messenger)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   if (pAllocator)
      messenger->alloc = *pAllocator;
   else
      messenger->alloc = instance->alloc;

   vk_object_base_init(NULL, &messenger->base,
                       VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT);

   messenger->severity = pCreateInfo->messageSeverity;
   messenger->type = pCreateInfo->messageType;
   messenger->callback = pCreateInfo->pfnUserCallback;
   messenger->data = pCreateInfo->pUserData;

   mtx_lock(&instance->debug_utils.callbacks_mutex);
   list_addtail(&messenger->link, &instance->debug_utils.callbacks);
   mtx_unlock(&instance->debug_utils.callbacks_mutex);

   *pMessenger = vk_debug_utils_messenger_to_handle(messenger);

   return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
vk_common_SubmitDebugUtilsMessageEXT(
   VkInstance _instance,
   VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
   VkDebugUtilsMessageTypeFlagsEXT messageTypes,
   const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData)
{
   VK_FROM_HANDLE(vk_instance, instance, _instance);

   vk_debug_message(instance, messageSeverity, messageTypes, pCallbackData);
}

VKAPI_ATTR void VKAPI_CALL
vk_common_DestroyDebugUtilsMessengerEXT(
   VkInstance _instance,
   VkDebugUtilsMessengerEXT _messenger,
   const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(vk_instance, instance, _instance);
   VK_FROM_HANDLE(vk_debug_utils_messenger, messenger, _messenger);

   if (messenger == NULL)
      return;

   mtx_lock(&instance->debug_utils.callbacks_mutex);
   list_del(&messenger->link);
   mtx_unlock(&instance->debug_utils.callbacks_mutex);

   vk_object_base_finish(&messenger->base);
   vk_free2(&instance->alloc, pAllocator, messenger);
}

static VkResult
vk_common_set_object_name_locked(
   struct vk_device *device,
   const VkDebugUtilsObjectNameInfoEXT *pNameInfo)
{
   if (unlikely(device->swapchain_name == NULL)) {
      /* Even though VkSwapchain/Surface are non-dispatchable objects, we know
       * a priori that these are actually pointers so we can use
       * the pointer hash table for them.
       */
      device->swapchain_name = _mesa_pointer_hash_table_create(NULL);
      if (device->swapchain_name == NULL)
         return VK_ERROR_OUT_OF_HOST_MEMORY;
   }

   char *object_name = vk_strdup(&device->alloc, pNameInfo->pObjectName,
                                 VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (object_name == NULL)
      return VK_ERROR_OUT_OF_HOST_MEMORY;
   struct hash_entry *entry =
      _mesa_hash_table_search(device->swapchain_name,
                              (void *)(uintptr_t)pNameInfo->objectHandle);
   if (unlikely(entry == NULL)) {
      entry = _mesa_hash_table_insert(device->swapchain_name,
                                      (void *)(uintptr_t)pNameInfo->objectHandle,
                                      object_name);
      if (entry == NULL) {
         vk_free(&device->alloc, object_name);
         return VK_ERROR_OUT_OF_HOST_MEMORY;
      }
   } else {
      vk_free(&device->alloc, entry->data);
      entry->data = object_name;
   }
   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_DebugMarkerSetObjectNameEXT(
   VkDevice _device,
   const VkDebugMarkerObjectNameInfoEXT *pNameInfo)
{
   VK_FROM_HANDLE(vk_device, device, _device);

   assert(pNameInfo->sType == VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT);

   VkObjectType object_type;
   switch (pNameInfo->objectType) {
   case VK_DEBUG_REPORT_OBJECT_TYPE_SURFACE_KHR_EXT:
      object_type = VK_OBJECT_TYPE_SURFACE_KHR;
      break;
   case VK_DEBUG_REPORT_OBJECT_TYPE_SWAPCHAIN_KHR_EXT:
      object_type = VK_OBJECT_TYPE_SWAPCHAIN_KHR;
      break;
   case VK_DEBUG_REPORT_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT_EXT:
      object_type = VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT;
      break;
   case VK_DEBUG_REPORT_OBJECT_TYPE_DISPLAY_KHR_EXT:
      object_type = VK_OBJECT_TYPE_DISPLAY_KHR;
      break;
   case VK_DEBUG_REPORT_OBJECT_TYPE_DISPLAY_MODE_KHR_EXT:
      object_type = VK_OBJECT_TYPE_DISPLAY_MODE_KHR;
      break;
   case VK_DEBUG_REPORT_OBJECT_TYPE_VALIDATION_CACHE_EXT_EXT:
      object_type = VK_OBJECT_TYPE_VALIDATION_CACHE_EXT;
      break;
   default:
      object_type = (VkObjectType)pNameInfo->objectType;
      break;
   }

   VkDebugUtilsObjectNameInfoEXT name_info = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
      .objectType = object_type,
      .objectHandle = pNameInfo->object,
      .pObjectName = pNameInfo->pObjectName,
   };

   return device->dispatch_table.SetDebugUtilsObjectNameEXT(_device, &name_info);
}

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_SetDebugUtilsObjectNameEXT(
   VkDevice _device,
   const VkDebugUtilsObjectNameInfoEXT *pNameInfo)
{
   VK_FROM_HANDLE(vk_device, device, _device);

#if DETECT_OS_ANDROID
   if (pNameInfo->objectType == VK_OBJECT_TYPE_SWAPCHAIN_KHR ||
       pNameInfo->objectType == VK_OBJECT_TYPE_SURFACE_KHR) {
#else
   if (pNameInfo->objectType == VK_OBJECT_TYPE_SURFACE_KHR) {
#endif
      mtx_lock(&device->swapchain_name_mtx);
      VkResult res = vk_common_set_object_name_locked(device, pNameInfo);
      mtx_unlock(&device->swapchain_name_mtx);
      return res;
   }

   struct vk_object_base *object =
      vk_object_base_from_u64_handle(pNameInfo->objectHandle,
                                     pNameInfo->objectType);

   assert(object->device != NULL || object->instance != NULL);
   VkAllocationCallbacks *alloc = object->device != NULL ?
      &object->device->alloc : &object->instance->alloc;
   if (object->object_name) {
      vk_free(alloc, object->object_name);
      object->object_name = NULL;
   }
   object->object_name = vk_strdup(alloc, pNameInfo->pObjectName,
                                   VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!object->object_name)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
vk_common_SetDebugUtilsObjectTagEXT(
   VkDevice _device,
   const VkDebugUtilsObjectTagInfoEXT *pTagInfo)
{
   /* no-op */
   return VK_SUCCESS;
}

static void
vk_common_append_debug_label(struct vk_device *device,
                             struct util_dynarray *labels,
                             const VkDebugUtilsLabelEXT *pLabelInfo)
{
   util_dynarray_append(labels, VkDebugUtilsLabelEXT, *pLabelInfo);
   VkDebugUtilsLabelEXT *current_label =
      util_dynarray_top_ptr(labels, VkDebugUtilsLabelEXT);
   current_label->pLabelName =
      vk_strdup(&device->alloc, current_label->pLabelName,
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
}

static void
vk_common_pop_debug_label(struct vk_device *device,
                          struct util_dynarray *labels)
{
   if (labels->size == 0)
      return;

   VkDebugUtilsLabelEXT previous_label =
      util_dynarray_pop(labels, VkDebugUtilsLabelEXT);
   vk_free(&device->alloc, (void *)previous_label.pLabelName);
}

VKAPI_ATTR void VKAPI_CALL
vk_common_CmdBeginDebugUtilsLabelEXT(
   VkCommandBuffer _commandBuffer,
   const VkDebugUtilsLabelEXT *pLabelInfo)
{
   VK_FROM_HANDLE(vk_command_buffer, command_buffer, _commandBuffer);

   /* If the latest label was submitted by CmdInsertDebugUtilsLabelEXT, we
    * should remove it first.
    */
   if (!command_buffer->region_begin) {
      vk_common_pop_debug_label(command_buffer->base.device,
                                &command_buffer->labels);
   }

   vk_common_append_debug_label(command_buffer->base.device,
                                &command_buffer->labels,
                                pLabelInfo);
   command_buffer->region_begin = true;
}

VKAPI_ATTR void VKAPI_CALL
vk_common_CmdEndDebugUtilsLabelEXT(VkCommandBuffer _commandBuffer)
{
   VK_FROM_HANDLE(vk_command_buffer, command_buffer, _commandBuffer);

   /* If the latest label was submitted by CmdInsertDebugUtilsLabelEXT, we
    * should remove it first.
    */
   if (!command_buffer->region_begin) {
      vk_common_pop_debug_label(command_buffer->base.device,
                                &command_buffer->labels);
   }

   vk_common_pop_debug_label(command_buffer->base.device,
                             &command_buffer->labels);
   command_buffer->region_begin = true;
}

VKAPI_ATTR void VKAPI_CALL
vk_common_CmdInsertDebugUtilsLabelEXT(
   VkCommandBuffer _commandBuffer,
   const VkDebugUtilsLabelEXT *pLabelInfo)
{
   VK_FROM_HANDLE(vk_command_buffer, command_buffer, _commandBuffer);

   /* If the latest label was submitted by CmdInsertDebugUtilsLabelEXT, we
    * should remove it first.
    */
   if (!command_buffer->region_begin) {
      vk_common_append_debug_label(command_buffer->base.device,
                                   &command_buffer->labels,
                                   pLabelInfo);
   }

   vk_common_append_debug_label(command_buffer->base.device,
                                &command_buffer->labels,
                                pLabelInfo);
   command_buffer->region_begin = false;
}

VKAPI_ATTR void VKAPI_CALL
vk_common_QueueBeginDebugUtilsLabelEXT(
   VkQueue _queue,
   const VkDebugUtilsLabelEXT *pLabelInfo)
{
   VK_FROM_HANDLE(vk_queue, queue, _queue);

   /* If the latest label was submitted by QueueInsertDebugUtilsLabelEXT, we
    * should remove it first.
    */
   if (!queue->region_begin)
      (void)util_dynarray_pop(&queue->labels, VkDebugUtilsLabelEXT);

   vk_common_append_debug_label(queue->base.device,
                                &queue->labels,
                                pLabelInfo);
   queue->region_begin = true;
}

VKAPI_ATTR void VKAPI_CALL
vk_common_QueueEndDebugUtilsLabelEXT(VkQueue _queue)
{
   VK_FROM_HANDLE(vk_queue, queue, _queue);

   /* If the latest label was submitted by QueueInsertDebugUtilsLabelEXT, we
    * should remove it first.
    */
   if (!queue->region_begin)
      vk_common_pop_debug_label(queue->base.device, &queue->labels);

   vk_common_pop_debug_label(queue->base.device, &queue->labels);
   queue->region_begin = true;
}

VKAPI_ATTR void VKAPI_CALL
vk_common_QueueInsertDebugUtilsLabelEXT(
   VkQueue _queue,
   const VkDebugUtilsLabelEXT *pLabelInfo)
{
   VK_FROM_HANDLE(vk_queue, queue, _queue);

   /* If the latest label was submitted by QueueInsertDebugUtilsLabelEXT, we
    * should remove it first.
    */
   if (!queue->region_begin)
      vk_common_pop_debug_label(queue->base.device, &queue->labels);

   vk_common_append_debug_label(queue->base.device,
                                &queue->labels,
                                pLabelInfo);
   queue->region_begin = false;
}
