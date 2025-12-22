/*
 * GStreamer
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GST_VULKAN_FWD_H__
#define __GST_VULKAN_FWD_H__

#include <gst/gst.h>
#include <gst/vulkan/vulkan-prelude.h>

G_BEGIN_DECLS

typedef struct _GstVulkanInstance GstVulkanInstance;
typedef struct _GstVulkanInstanceClass GstVulkanInstanceClass;
typedef struct _GstVulkanInstancePrivate GstVulkanInstancePrivate;

typedef struct _GstVulkanDevice GstVulkanDevice;
typedef struct _GstVulkanDeviceClass GstVulkanDeviceClass;
typedef struct _GstVulkanDevicePrivate GstVulkanDevicePrivate;

typedef struct _GstVulkanPhysicalDevice GstVulkanPhysicalDevice;
typedef struct _GstVulkanPhysicalDeviceClass GstVulkanPhysicalDeviceClass;
typedef struct _GstVulkanPhysicalDevicePrivate GstVulkanPhysicalDevicePrivate;

typedef struct _GstVulkanQueue GstVulkanQueue;
typedef struct _GstVulkanQueueClass GstVulkanQueueClass;
typedef struct _GstVulkanQueuePrivate GstVulkanQueuePrivate;

typedef struct _GstVulkanCommandPool GstVulkanCommandPool;
typedef struct _GstVulkanCommandPoolClass GstVulkanCommandPoolClass;
typedef struct _GstVulkanCommandPoolPrivate GstVulkanCommandPoolPrivate;

typedef struct _GstVulkanCommandBuffer GstVulkanCommandBuffer;

typedef struct _GstVulkanDescriptorSet GstVulkanDescriptorSet;

typedef struct _GstVulkanDescriptorPool GstVulkanDescriptorPool;
typedef struct _GstVulkanDescriptorPoolClass GstVulkanDescriptorPoolClass;
typedef struct _GstVulkanDescriptorPoolPrivate GstVulkanDescriptorPoolPrivate;

typedef struct _GstVulkanDescriptorCache GstVulkanDescriptorCache;
typedef struct _GstVulkanDescriptorCacheClass GstVulkanDescriptorCacheClass;
typedef struct _GstVulkanDescriptorCachePrivate GstVulkanDescriptorCachePrivate;

typedef struct _GstVulkanDisplay GstVulkanDisplay;
typedef struct _GstVulkanDisplayClass GstVulkanDisplayClass;
typedef struct _GstVulkanDisplayPrivate GstVulkanDisplayPrivate;

typedef struct _GstVulkanWindow GstVulkanWindow;
typedef struct _GstVulkanWindowClass GstVulkanWindowClass;
typedef struct _GstVulkanWindowPrivate GstVulkanWindowPrivate;

typedef struct _GstVulkanFence GstVulkanFence;

typedef struct _GstVulkanFenceCache GstVulkanFenceCache;
typedef struct _GstVulkanFenceCacheClass GstVulkanFenceCacheClass;

typedef struct _GstVulkanMemory GstVulkanMemory;
typedef struct _GstVulkanMemoryAllocator GstVulkanMemoryAllocator;
typedef struct _GstVulkanMemoryAllocatorClass GstVulkanMemoryAllocatorClass;

typedef struct _GstVulkanBufferMemory GstVulkanBufferMemory;
typedef struct _GstVulkanBufferMemoryAllocator GstVulkanBufferMemoryAllocator;
typedef struct _GstVulkanBufferMemoryAllocatorClass GstVulkanBufferMemoryAllocatorClass;

typedef struct _GstVulkanBufferPool GstVulkanBufferPool;
typedef struct _GstVulkanBufferPoolClass GstVulkanBufferPoolClass;
typedef struct _GstVulkanBufferPoolPrivate GstVulkanBufferPoolPrivate;

typedef struct _GstVulkanHandle GstVulkanHandle;

typedef struct _GstVulkanHandlePool GstVulkanHandlePool;
typedef struct _GstVulkanHandlePoolClass GstVulkanHandlePoolClass;

typedef struct _GstVulkanImageMemory GstVulkanImageMemory;
typedef struct _GstVulkanImageMemoryAllocator GstVulkanImageMemoryAllocator;
typedef struct _GstVulkanImageMemoryAllocatorClass GstVulkanImageMemoryAllocatorClass;

typedef struct _GstVulkanImageBufferPool GstVulkanImageBufferPool;
typedef struct _GstVulkanImageBufferPoolClass GstVulkanImageBufferPoolClass;
typedef struct _GstVulkanImageBufferPoolPrivate GstVulkanImageBufferPoolPrivate;

typedef struct _GstVulkanImageView GstVulkanImageView;

typedef struct _GstVulkanBarrierMemoryInfo GstVulkanBarrierMemoryInfo;
typedef struct _GstVulkanBarrierBufferInfo GstVulkanBarrierBufferInfo;
typedef struct _GstVulkanBarrierImageInfo GstVulkanBarrierImageInfo;

typedef struct _GstVulkanTrashList GstVulkanTrashList;
typedef struct _GstVulkanTrashListClass GstVulkanTrashListClass;

typedef struct _GstVulkanTrash GstVulkanTrash;

typedef struct _GstVulkanFullScreenQuad GstVulkanFullScreenQuad;
typedef struct _GstVulkanFullScreenQuadClass GstVulkanFullScreenQuadClass;
typedef struct _GstVulkanFullScreenQuadPrivate GstVulkanFullScreenQuadPrivate;

typedef struct _GstVulkanQueueFamilyOps GstVulkanQueueFamilyOps;
typedef struct _GstVulkanVideoProfile GstVulkanVideoProfile;
typedef struct _GstVulkanVideoCapabilities GstVulkanVideoCapabilities;

typedef struct _GstVulkanOperation GstVulkanOperation;
typedef struct _GstVulkanOperationClass GstVulkanOperationClass;
typedef struct _GstVulkanOperationPrivate GstVulkanOperationPrivate;

typedef struct _GstVulkanDecoder GstVulkanDecoder;
typedef struct _GstVulkanDecoderClass GstVulkanDecoderClass;
typedef struct _GstVulkanDecoderPrivate GstVulkanDecoderPrivate;
typedef union _GstVulkanDecoderParameters GstVulkanDecoderParameters;
typedef struct _GstVulkanDecoderPicture GstVulkanDecoderPicture;

G_END_DECLS

#endif /* __GST_VULKAN_FWD_H__ */
