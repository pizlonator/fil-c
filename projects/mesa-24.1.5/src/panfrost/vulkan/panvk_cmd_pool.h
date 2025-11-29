/*
 * Copyright © 2021 Collabora Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef PANVK_CMD_POOL_H
#define PANVK_CMD_POOL_H

#include "vk_command_pool.h"

#include "panvk_mempool.h"

struct panvk_cmd_pool {
   struct vk_command_pool vk;
   struct panvk_bo_pool desc_bo_pool;
   struct panvk_bo_pool varying_bo_pool;
   struct panvk_bo_pool tls_bo_pool;
};

VK_DEFINE_NONDISP_HANDLE_CASTS(panvk_cmd_pool, vk.base, VkCommandPool,
                               VK_OBJECT_TYPE_COMMAND_POOL)

#endif
