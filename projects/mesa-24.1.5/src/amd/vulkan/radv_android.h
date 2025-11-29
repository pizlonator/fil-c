/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_ANDROID_H
#define RADV_ANDROID_H

#include <stdbool.h>

#include <vulkan/vulkan.h>

#include <vulkan/vk_android_native_buffer.h>
#include <vulkan/vulkan_android.h>

#include "vk_android.h"

/* Helper to determine if we should compile
 * any of the Android AHB support.
 *
 * To actually enable the ext we also need
 * the necessary kernel support.
 */
#if DETECT_OS_ANDROID && ANDROID_API_LEVEL >= 26
#define RADV_SUPPORT_ANDROID_HARDWARE_BUFFER 1
#include <vndk/hardware_buffer.h>
#else
#define RADV_SUPPORT_ANDROID_HARDWARE_BUFFER 0
#endif

struct radv_device;
struct radv_device_memory;

VkResult radv_image_from_gralloc(VkDevice device_h, const VkImageCreateInfo *base_info,
                                 const VkNativeBufferANDROID *gralloc_info, const VkAllocationCallbacks *alloc,
                                 VkImage *out_image_h);

unsigned radv_ahb_format_for_vk_format(VkFormat vk_format);

VkFormat radv_select_android_external_format(const void *next, VkFormat default_format);

VkResult radv_import_ahb_memory(struct radv_device *device, struct radv_device_memory *mem, unsigned priority,
                                const VkImportAndroidHardwareBufferInfoANDROID *info);

VkResult radv_create_ahb_memory(struct radv_device *device, struct radv_device_memory *mem, unsigned priority,
                                const VkMemoryAllocateInfo *pAllocateInfo);

bool radv_android_gralloc_supports_format(VkFormat format, VkImageUsageFlagBits usage);

#endif /* RADV_ANDROID_H */
