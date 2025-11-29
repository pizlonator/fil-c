/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_CP_REG_SHADOWING_H
#define RADV_CP_REG_SHADOWING_H

#include "radv_radeon_winsys.h"

struct radv_device;
struct radv_queue_state;
struct radv_queue;

VkResult radv_create_shadow_regs_preamble(struct radv_device *device, struct radv_queue_state *queue_state);

void radv_destroy_shadow_regs_preamble(struct radv_device *device, struct radv_queue_state *queue_state,
                                       struct radeon_winsys *ws);

void radv_emit_shadow_regs_preamble(struct radeon_cmdbuf *cs, const struct radv_device *device,
                                    struct radv_queue_state *queue_state);

VkResult radv_init_shadowed_regs_buffer_state(const struct radv_device *device, struct radv_queue *queue);

#endif /* RADV_CP_REG_SHADOWING_H */
