/*
 * Copyright © 2021 Collabora Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef PANVK_IMAGE_H
#define PANVK_IMAGE_H

#include "vk_image.h"

#include "pan_texture.h"

struct panvk_image {
   struct vk_image vk;

   /* TODO: See if we can rework the synchronization logic so we don't need to
    * pass BOs around.
    */
   struct pan_kmod_bo *bo;

   struct pan_image pimage;
};

VK_DEFINE_NONDISP_HANDLE_CASTS(panvk_image, vk.base, VkImage,
                               VK_OBJECT_TYPE_IMAGE)

#endif
