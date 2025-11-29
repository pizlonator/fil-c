/*
 * Copyright © 2021 Collabora Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef PANVK_EVENT_H
#define PANVK_EVENT_H

#include <stdint.h>

#include "vk_object.h"

struct panvk_event {
   struct vk_object_base base;
   uint32_t syncobj;
};

VK_DEFINE_NONDISP_HANDLE_CASTS(panvk_event, base, VkEvent, VK_OBJECT_TYPE_EVENT)

#endif
