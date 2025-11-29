/*
 * Copyright © 2022 Collabora Ltd. and Red Hat Inc.
 * SPDX-License-Identifier: MIT
 */
#ifndef NVK_QUEUE_H
#define NVK_QUEUE_H 1

#include "nvk_private.h"

#include "vk_queue.h"

struct novueau_ws_bo;
struct nouveau_ws_context;
struct novueau_ws_push;
struct nv_push;
struct nvk_device;

struct nvk_queue_state {
   struct {
      struct nouveau_ws_bo *bo;
      uint32_t alloc_count;
   } images;

   struct {
      struct nouveau_ws_bo *bo;
      uint32_t alloc_count;
   } samplers;

   struct {
      struct nouveau_ws_bo *bo;
      uint32_t bytes_per_warp;
      uint32_t bytes_per_tpc;
   } slm;

   struct {
      struct nouveau_ws_bo *bo;
      void *bo_map;
      uint32_t dw_count;
   } push;
};

VkResult nvk_queue_state_update(struct nvk_device *dev,
                                struct nvk_queue_state *qs);

struct nvk_queue {
   struct vk_queue vk;

   struct {
      struct nouveau_ws_context *ws_ctx;
      uint32_t syncobj;
   } drm;

   struct nvk_queue_state state;
};

static inline struct nvk_device *
nvk_queue_device(struct nvk_queue *queue)
{
   return (struct nvk_device *)queue->vk.base.device;
}

VkResult nvk_queue_init(struct nvk_device *dev, struct nvk_queue *queue,
                        const VkDeviceQueueCreateInfo *pCreateInfo,
                        uint32_t index_in_family);

void nvk_queue_finish(struct nvk_device *dev, struct nvk_queue *queue);

VkResult nvk_push_draw_state_init(struct nvk_device *dev,
                                  struct nv_push *p);

VkResult nvk_push_dispatch_state_init(struct nvk_device *dev,
                                      struct nv_push *p);

/* this always syncs, so only use when that doesn't matter */
VkResult nvk_queue_submit_simple(struct nvk_queue *queue,
                                 uint32_t dw_count, const uint32_t *dw,
                                 uint32_t extra_bo_count,
                                 struct nouveau_ws_bo **extra_bos);

VkResult nvk_queue_init_drm_nouveau(struct nvk_device *dev,
                                    struct nvk_queue *queue,
                                    VkQueueFlags queue_flags);

void nvk_queue_finish_drm_nouveau(struct nvk_device *dev,
                                  struct nvk_queue *queue);

VkResult nvk_queue_submit_simple_drm_nouveau(struct nvk_queue *queue,
                                             uint32_t push_dw_count,
                                             struct nouveau_ws_bo *push_bo,
                                             uint32_t extra_bo_count,
                                             struct nouveau_ws_bo **extra_bos);

VkResult nvk_queue_submit_drm_nouveau(struct nvk_queue *queue,
                                      struct vk_queue_submit *submit,
                                      bool sync);

#endif
