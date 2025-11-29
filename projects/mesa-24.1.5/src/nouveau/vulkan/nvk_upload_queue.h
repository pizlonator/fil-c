/*
 * Copyright © 2024 Collabora Ltd. and Red Hat Inc.
 * SPDX-License-Identifier: MIT
 */
#ifndef NVK_DMA_QUEUE_H
#define NVK_DMA_QUEUE_H 1

#include "nvk_private.h"

#include "util/list.h"
#include "util/simple_mtx.h"

struct nouveau_ws_context;
struct nvk_device;
struct nvk_upload_bo;

struct nvk_upload_queue {
   simple_mtx_t mutex;

   struct {
      struct nouveau_ws_context *ws_ctx;
      uint32_t syncobj;
   } drm;

   uint64_t last_time_point;

   struct nvk_upload_bo *bo;

   /* We grow the buffer from both ends.  Pushbuf data goes at the start of
    * the buffer and upload data at the tail.
    */
   uint32_t bo_push_start;
   uint32_t bo_push_end;
   uint32_t bo_data_start;

   /* BO recycle pool */
   struct list_head bos;
};

VkResult nvk_upload_queue_init(struct nvk_device *dev,
                               struct nvk_upload_queue *queue);
void nvk_upload_queue_finish(struct nvk_device *dev,
                             struct nvk_upload_queue *queue);

VkResult nvk_upload_queue_flush(struct nvk_device *dev,
                                struct nvk_upload_queue *queue,
                                uint64_t *time_point_out);

VkResult nvk_upload_queue_sync(struct nvk_device *dev,
                               struct nvk_upload_queue *queue);

VkResult nvk_upload_queue_upload(struct nvk_device *dev,
                                 struct nvk_upload_queue *queue,
                                 uint64_t dst_addr,
                                 const void *src, size_t size);

#endif /* NVK_DMA_QUEUE_H */
