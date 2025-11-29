/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_CP_DMA_H
#define RADV_CP_DMA_H

#include <inttypes.h>
#include <stdbool.h>

struct radv_device;
struct radeon_cmdbuf;
struct radv_cmd_buffer;

void radv_cs_cp_dma_prefetch(const struct radv_device *device, struct radeon_cmdbuf *cs, uint64_t va, unsigned size,
                             bool predicating);

void radv_cp_dma_prefetch(struct radv_cmd_buffer *cmd_buffer, uint64_t va, unsigned size);

void radv_cp_dma_buffer_copy(struct radv_cmd_buffer *cmd_buffer, uint64_t src_va, uint64_t dest_va, uint64_t size);

void radv_cp_dma_clear_buffer(struct radv_cmd_buffer *cmd_buffer, uint64_t va, uint64_t size, unsigned value);

void radv_cp_dma_wait_for_idle(struct radv_cmd_buffer *cmd_buffer);

#endif /* RADV_CP_DMA_H */
