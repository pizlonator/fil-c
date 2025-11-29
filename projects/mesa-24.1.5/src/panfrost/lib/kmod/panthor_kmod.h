/*
 * Copyright © 2023 Collabora, Ltd.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct drm_panthor_csif_info;

struct pan_kmod_bo;
struct pan_kmod_dev;
struct pan_kmod_vm;

int panthor_kmod_bo_attach_sync_point(struct pan_kmod_bo *bo,
                                      uint32_t sync_handle,
                                      uint64_t sync_point, bool written);
int panthor_kmod_bo_get_sync_point(struct pan_kmod_bo *bo,
                                   uint32_t *sync_handle, uint64_t *sync_point,
                                   bool read_only);
uint32_t panthor_kmod_vm_sync_handle(struct pan_kmod_vm *vm);
uint64_t panthor_kmod_vm_sync_lock(struct pan_kmod_vm *vm);
void panthor_kmod_vm_sync_unlock(struct pan_kmod_vm *vm,
                                 uint64_t new_sync_point);
uint32_t panthor_kmod_get_flush_id(const struct pan_kmod_dev *dev);

const struct drm_panthor_csif_info *
panthor_kmod_get_csif_props(const struct pan_kmod_dev *dev);

#if defined(__cplusplus)
} // extern "C"
#endif
