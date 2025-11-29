/*
 * Copyright © 2021 Collabora Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef PANVK_PHYSICAL_DEVICE_H
#define PANVK_PHYSICAL_DEVICE_H

#include <stdint.h>

#include "panvk_instance.h"

#include "vk_physical_device.h"
#include "vk_sync.h"
#include "vk_util.h"
#include "wsi_common.h"

#include "lib/kmod/pan_kmod.h"

struct panfrost_model;
struct pan_blendable_format;
struct panfrost_format;
struct panvk_instance;

struct panvk_physical_device {
   struct vk_physical_device vk;

   struct {
      struct pan_kmod_dev *dev;
      struct pan_kmod_dev_props props;
   } kmod;

   const struct panfrost_model *model;
   struct {
      const struct pan_blendable_format *blendable;
      const struct panfrost_format *all;
   } formats;

   char name[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];
   uint8_t driver_uuid[VK_UUID_SIZE];
   uint8_t device_uuid[VK_UUID_SIZE];
   uint8_t cache_uuid[VK_UUID_SIZE];

   struct vk_sync_type drm_syncobj_type;
   const struct vk_sync_type *sync_types[2];

   struct wsi_device wsi_device;

   int master_fd;
};

VK_DEFINE_HANDLE_CASTS(panvk_physical_device, vk.base, VkPhysicalDevice,
                       VK_OBJECT_TYPE_PHYSICAL_DEVICE)

static inline struct panvk_physical_device *
to_panvk_physical_device(struct vk_physical_device *phys_dev)
{
   return container_of(phys_dev, struct panvk_physical_device, vk);
}

static inline uint32_t
panvk_get_vk_version()
{
   const uint32_t version_override = vk_get_version_override();
   if (version_override)
      return version_override;

   return VK_MAKE_API_VERSION(0, 1, 0, VK_HEADER_VERSION);
}

VkResult panvk_physical_device_init(struct panvk_physical_device *device,
                                    struct panvk_instance *instance,
                                    drmDevicePtr drm_device);

void panvk_physical_device_finish(struct panvk_physical_device *device);

#endif
