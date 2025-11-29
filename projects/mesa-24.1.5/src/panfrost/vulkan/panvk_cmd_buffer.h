/*
 * Copyright © 2021 Collabora Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef PANVK_CMD_BUFFER_H
#define PANVK_CMD_BUFFER_H

#ifndef PAN_ARCH
#error "PAN_ARCH must be defined"
#endif

#include <stdint.h>

#include "vulkan/runtime/vk_command_buffer.h"

#include "panvk_descriptor_set.h"
#include "panvk_descriptor_set_layout.h"
#include "panvk_device.h"
#include "panvk_macros.h"
#include "panvk_mempool.h"
#include "panvk_pipeline.h"
#include "panvk_shader.h"

#include "pan_jc.h"

#include "util/list.h"

#include "genxml/gen_macros.h"

#define MAX_BIND_POINTS         2 /* compute + graphics */
#define MAX_VBS                 16
#define MAX_PUSH_CONSTANTS_SIZE 128

struct panvk_batch {
   struct list_head node;
   struct util_dynarray jobs;
   struct util_dynarray event_ops;
   struct pan_jc jc;
   struct {
      struct panfrost_ptr desc;
      uint32_t bo_count;

      /* One slot per color, two more slots for the depth/stencil buffers. */
      struct pan_kmod_bo *bos[MAX_RTS + 2];
   } fb;
   struct {
      struct pan_kmod_bo *src, *dst;
   } blit;
   struct panfrost_ptr tls;
   mali_ptr fragment_job;
   struct {
      struct pan_tiler_context ctx;
      struct panfrost_ptr heap_desc;
      struct panfrost_ptr ctx_desc;
      struct mali_tiler_heap_packed heap_templ;
      struct mali_tiler_context_packed ctx_templ;
   } tiler;
   struct pan_tls_info tlsinfo;
   unsigned wls_total_size;
   bool issued;
};

enum panvk_cmd_event_op_type {
   PANVK_EVENT_OP_SET,
   PANVK_EVENT_OP_RESET,
   PANVK_EVENT_OP_WAIT,
};

struct panvk_cmd_event_op {
   enum panvk_cmd_event_op_type type;
   struct panvk_event *event;
};

enum panvk_dynamic_state_bits {
   PANVK_DYNAMIC_VIEWPORT = 1 << 0,
   PANVK_DYNAMIC_SCISSOR = 1 << 1,
   PANVK_DYNAMIC_LINE_WIDTH = 1 << 2,
   PANVK_DYNAMIC_DEPTH_BIAS = 1 << 3,
   PANVK_DYNAMIC_BLEND_CONSTANTS = 1 << 4,
   PANVK_DYNAMIC_DEPTH_BOUNDS = 1 << 5,
   PANVK_DYNAMIC_STENCIL_COMPARE_MASK = 1 << 6,
   PANVK_DYNAMIC_STENCIL_WRITE_MASK = 1 << 7,
   PANVK_DYNAMIC_STENCIL_REFERENCE = 1 << 8,
   PANVK_DYNAMIC_DISCARD_RECTANGLE = 1 << 9,
   PANVK_DYNAMIC_SSBO = 1 << 10,
   PANVK_DYNAMIC_VERTEX_INSTANCE_OFFSETS = 1 << 11,
   PANVK_DYNAMIC_ALL = (1 << 12) - 1,
};

struct panvk_descriptor_state {
   uint32_t dirty;
   const struct panvk_descriptor_set *sets[MAX_SETS];
   struct panvk_push_descriptor_set *push_sets[MAX_SETS];
   struct panvk_sysvals sysvals;
   struct {
      struct panvk_buffer_desc ubos[MAX_DYNAMIC_UNIFORM_BUFFERS];
      struct panvk_buffer_desc ssbos[MAX_DYNAMIC_STORAGE_BUFFERS];
   } dyn;
   mali_ptr sysvals_ptr;
   mali_ptr ubos;
   mali_ptr textures;
   mali_ptr samplers;
   mali_ptr push_constants;
   mali_ptr vs_attribs;
   mali_ptr vs_attrib_bufs;
   mali_ptr non_vs_attribs;
   mali_ptr non_vs_attrib_bufs;
};

struct panvk_attrib_buf {
   mali_ptr address;
   unsigned size;
};

struct panvk_cmd_state {
   uint32_t dirty;

   struct panvk_varyings_info varyings;
   mali_ptr fs_rsd;

   struct {
      float constants[4];
   } blend;

   struct {
      struct {
         float constant_factor;
         float clamp;
         float slope_factor;
      } depth_bias;
      float line_width;
   } rast;

   struct {
      struct panvk_attrib_buf bufs[MAX_VBS];
      unsigned count;
   } vb;

   /* Index buffer */
   struct {
      struct panvk_buffer *buffer;
      uint64_t offset;
      uint8_t index_size;
      uint32_t first_vertex, base_vertex, base_instance;
   } ib;

   struct {
      struct {
         uint8_t compare_mask;
         uint8_t write_mask;
         uint8_t ref;
      } s_front, s_back;
   } zs;

   struct {
      struct pan_fb_info info;
      bool crc_valid[MAX_RTS];
      uint32_t bo_count;
      struct pan_kmod_bo *bos[MAX_RTS + 2];
   } fb;

   mali_ptr vpd;
   VkViewport viewport;
   VkRect2D scissor;

   struct panvk_batch *batch;
};

struct panvk_cmd_bind_point_state {
   struct panvk_descriptor_state desc_state;
   const struct panvk_pipeline *pipeline;
};

struct panvk_cmd_buffer {
   struct vk_command_buffer vk;

   struct panvk_pool desc_pool;
   struct panvk_pool varying_pool;
   struct panvk_pool tls_pool;
   struct list_head batches;

   struct panvk_cmd_state state;

   uint8_t push_constants[MAX_PUSH_CONSTANTS_SIZE];

   struct panvk_cmd_bind_point_state bind_points[MAX_BIND_POINTS];
};

VK_DEFINE_HANDLE_CASTS(panvk_cmd_buffer, vk.base, VkCommandBuffer,
                       VK_OBJECT_TYPE_COMMAND_BUFFER)

#define panvk_cmd_get_bind_point_state(cmdbuf, bindpoint)                      \
   &(cmdbuf)->bind_points[VK_PIPELINE_BIND_POINT_##bindpoint]

#define panvk_cmd_get_pipeline(cmdbuf, bindpoint)                              \
   (cmdbuf)->bind_points[VK_PIPELINE_BIND_POINT_##bindpoint].pipeline

#define panvk_cmd_get_desc_state(cmdbuf, bindpoint)                            \
   &(cmdbuf)->bind_points[VK_PIPELINE_BIND_POINT_##bindpoint].desc_state

extern const struct vk_command_buffer_ops panvk_per_arch(cmd_buffer_ops);

struct panvk_batch *
panvk_per_arch(cmd_open_batch)(struct panvk_cmd_buffer *cmdbuf);

void panvk_per_arch(cmd_close_batch)(struct panvk_cmd_buffer *cmdbuf);

void panvk_per_arch(cmd_get_tiler_context)(struct panvk_cmd_buffer *cmdbuf,
                                           unsigned width, unsigned height);

void panvk_per_arch(cmd_alloc_fb_desc)(struct panvk_cmd_buffer *cmdbuf);

void panvk_per_arch(cmd_alloc_tls_desc)(struct panvk_cmd_buffer *cmdbuf,
                                        bool gfx);

void panvk_per_arch(cmd_prepare_tiler_context)(struct panvk_cmd_buffer *cmdbuf);

void panvk_per_arch(emit_viewport)(const VkViewport *viewport,
                                   const VkRect2D *scissor, void *vpd);

#endif
