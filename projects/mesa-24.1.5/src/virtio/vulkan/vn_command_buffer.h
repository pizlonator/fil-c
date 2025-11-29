/*
 * Copyright 2019 Google LLC
 * SPDX-License-Identifier: MIT
 *
 * based in part on anv and radv which are:
 * Copyright © 2015 Intel Corporation
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 */

#ifndef VN_COMMAND_BUFFER_H
#define VN_COMMAND_BUFFER_H

#include "vn_common.h"

#include "vn_cs.h"

struct vn_command_pool {
   struct vn_object_base base;

   VkAllocationCallbacks allocator;
   struct vn_device *device;
   uint32_t queue_family_index;

   struct list_head command_buffers;

   /* The list contains the recycled query records allocated from the same
    * command pool.
    *
    * For normal cmd pool, no additional locking is needed as below have
    * already been protected by external synchronization:
    * - alloc: record query, reset query and patch-in query records from the
    *          secondary cmds
    * - recycle: explicit and implicit cmd reset, cmd free and pool reset
    * - free: pool purge reset and pool destroy
    *
    * For feedback cmd pool, external locking is needed for now for below:
    * - alloc: queue submission
    * - recycle: queue submission and companion cmd reset/free
    * - free: device destroy
    */
   struct list_head free_query_records;

   /* for scrubbing VK_IMAGE_LAYOUT_PRESENT_SRC_KHR */
   struct vn_cached_storage storage;
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_command_pool,
                               base.base,
                               VkCommandPool,
                               VK_OBJECT_TYPE_COMMAND_POOL)

enum vn_command_buffer_state {
   VN_COMMAND_BUFFER_STATE_INITIAL,
   VN_COMMAND_BUFFER_STATE_RECORDING,
   VN_COMMAND_BUFFER_STATE_EXECUTABLE,
   VN_COMMAND_BUFFER_STATE_INVALID,
};

/* command buffer builder to:
 * - fix wsi image ownership and layout transitions
 * - scrub ignored bits in VkCommandBufferBeginInfo
 * - support asynchronization query optimization (query feedback)
 */
struct vn_command_buffer_builder {
   /* track the active legacy render pass */
   const struct vn_render_pass *render_pass;
   /* track the wsi images requiring layout fixes */
   const struct vn_image **present_src_images;
   /* track if inside a render pass instance */
   bool in_render_pass;
   /* track the active subpass for view mask used in the subpass */
   uint32_t subpass_index;
   /* track the active view mask inside a render pass instance */
   uint32_t view_mask;
   /* track if VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT was set */
   bool is_simultaneous;
   /* track the recorded queries and resets */
   struct list_head query_records;
};

struct vn_query_feedback_cmd;

struct vn_command_buffer {
   struct vn_object_base base;

   struct vn_command_pool *pool;
   VkCommandBufferLevel level;
   enum vn_command_buffer_state state;
   struct vn_cs_encoder cs;

   struct vn_command_buffer_builder builder;

   struct vn_query_feedback_cmd *linked_qfb_cmd;

   struct list_head head;
};
VK_DEFINE_HANDLE_CASTS(vn_command_buffer,
                       base.base,
                       VkCommandBuffer,
                       VK_OBJECT_TYPE_COMMAND_BUFFER)

/* Queries recorded to support qfb.
 * - query_count is the actual queries used with multiview considered
 * - copy is whether the record is for result copy or query reset
 *
 * The query records are tracked at each cmd with the recording order. Those
 * from the secondary cmds are patched into the primary ones at this moment.
 */
struct vn_cmd_query_record {
   struct vn_query_pool *query_pool;
   uint32_t query;
   uint32_t query_count;
   bool copy;

   struct list_head head;
};

struct vn_cmd_query_record *
vn_cmd_pool_alloc_query_record(struct vn_command_pool *cmd_pool,
                               struct vn_query_pool *query_pool,
                               uint32_t query,
                               uint32_t query_count,
                               bool copy);

static inline void
vn_cmd_pool_free_query_records(struct vn_command_pool *cmd_pool,
                               struct list_head *query_records)
{
   list_splicetail(query_records, &cmd_pool->free_query_records);
}

#endif /* VN_COMMAND_BUFFER_H */
