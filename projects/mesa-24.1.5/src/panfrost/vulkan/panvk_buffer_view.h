/*
 * Copyright © 2021 Collabora Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef PANVK_BUFFER_VIEW_H
#define PANVK_BUFFER_VIEW_H

#ifndef PAN_ARCH
#error "PAN_ARCH must be defined"
#endif

#include <stdint.h>

#include "vk_buffer_view.h"

#include "genxml/gen_macros.h"

struct panvk_priv_bo;

struct panvk_buffer_view {
   struct vk_buffer_view vk;
   struct panvk_priv_bo *bo;

   struct {
      struct mali_texture_packed tex;
      struct mali_attribute_buffer_packed img_attrib_buf[2];
   } descs;
};

VK_DEFINE_NONDISP_HANDLE_CASTS(panvk_buffer_view, vk.base, VkBufferView,
                               VK_OBJECT_TYPE_BUFFER_VIEW)

#endif
