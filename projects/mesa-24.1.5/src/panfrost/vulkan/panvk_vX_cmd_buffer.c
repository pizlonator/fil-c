/*
 * Copyright © 2021 Collabora Ltd.
 *
 * Derived from tu_cmd_buffer.c which is:
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 * Copyright © 2015 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "genxml/gen_macros.h"

#include "panvk_buffer.h"
#include "panvk_cmd_buffer.h"
#include "panvk_cmd_pool.h"
#include "panvk_device.h"
#include "panvk_entrypoints.h"
#include "panvk_event.h"
#include "panvk_image.h"
#include "panvk_image_view.h"
#include "panvk_instance.h"
#include "panvk_physical_device.h"
#include "panvk_pipeline.h"
#include "panvk_pipeline_layout.h"
#include "panvk_priv_bo.h"

#include "pan_blitter.h"
#include "pan_desc.h"
#include "pan_encoder.h"
#include "pan_props.h"
#include "pan_samples.h"

#include "util/rounding.h"
#include "util/u_pack_color.h"

#include "vk_descriptor_update_template.h"
#include "vk_format.h"

struct panvk_draw_info {
   unsigned first_index;
   unsigned index_count;
   unsigned index_size;
   unsigned first_vertex;
   unsigned vertex_count;
   unsigned vertex_range;
   unsigned padded_vertex_count;
   unsigned first_instance;
   unsigned instance_count;
   int vertex_offset;
   unsigned offset_start;
   struct mali_invocation_packed invocation;
   struct {
      mali_ptr varyings;
      mali_ptr attributes;
      mali_ptr attribute_bufs;
   } stages[MESA_SHADER_STAGES];
   mali_ptr push_uniforms;
   mali_ptr varying_bufs;
   mali_ptr textures;
   mali_ptr samplers;
   mali_ptr ubos;
   mali_ptr position;
   mali_ptr indices;
   union {
      mali_ptr psiz;
      float line_width;
   };
   mali_ptr tls;
   mali_ptr fb;
   const struct pan_tiler_context *tiler_ctx;
   mali_ptr fs_rsd;
   mali_ptr viewport;
   struct {
      struct panfrost_ptr vertex;
      struct panfrost_ptr tiler;
   } jobs;
};

struct panvk_dispatch_info {
   struct pan_compute_dim wg_count;
   mali_ptr attributes;
   mali_ptr attribute_bufs;
   mali_ptr tsd;
   mali_ptr ubos;
   mali_ptr push_uniforms;
   mali_ptr textures;
   mali_ptr samplers;
};

static uint32_t
panvk_debug_adjust_bo_flags(const struct panvk_device *device,
                            uint32_t bo_flags)
{
   struct panvk_instance *instance =
      to_panvk_instance(device->vk.physical->instance);

   if (instance->debug_flags & PANVK_DEBUG_DUMP)
      bo_flags &= ~PAN_KMOD_BO_FLAG_NO_MMAP;

   return bo_flags;
}

static void
panvk_cmd_prepare_fragment_job(struct panvk_cmd_buffer *cmdbuf)
{
   const struct pan_fb_info *fbinfo = &cmdbuf->state.fb.info;
   struct panvk_batch *batch = cmdbuf->state.batch;
   struct panfrost_ptr job_ptr =
      pan_pool_alloc_desc(&cmdbuf->desc_pool.base, FRAGMENT_JOB);

   GENX(pan_emit_fragment_job)
   (fbinfo, batch->fb.desc.gpu, job_ptr.cpu), batch->fragment_job = job_ptr.gpu;
   util_dynarray_append(&batch->jobs, void *, job_ptr.cpu);
}

void
panvk_per_arch(cmd_close_batch)(struct panvk_cmd_buffer *cmdbuf)
{
   struct panvk_batch *batch = cmdbuf->state.batch;

   if (!batch)
      return;

   struct pan_fb_info *fbinfo = &cmdbuf->state.fb.info;

   assert(batch);

   bool clear = fbinfo->zs.clear.z | fbinfo->zs.clear.s;
   for (unsigned i = 0; i < fbinfo->rt_count; i++)
      clear |= fbinfo->rts[i].clear;

   if (!clear && !batch->jc.first_job) {
      if (util_dynarray_num_elements(&batch->event_ops,
                                     struct panvk_cmd_event_op) == 0) {
         /* Content-less batch, let's drop it */
         vk_free(&cmdbuf->vk.pool->alloc, batch);
      } else {
         /* Batch has no jobs but is needed for synchronization, let's add a
          * NULL job so the SUBMIT ioctl doesn't choke on it.
          */
         struct panfrost_ptr ptr =
            pan_pool_alloc_desc(&cmdbuf->desc_pool.base, JOB_HEADER);
         util_dynarray_append(&batch->jobs, void *, ptr.cpu);
         pan_jc_add_job(&cmdbuf->desc_pool.base, &batch->jc, MALI_JOB_TYPE_NULL,
                        false, false, 0, 0, &ptr, false);
         list_addtail(&batch->node, &cmdbuf->batches);
      }
      cmdbuf->state.batch = NULL;
      return;
   }

   struct panvk_device *dev = to_panvk_device(cmdbuf->vk.base.device);
   struct panvk_physical_device *phys_dev =
      to_panvk_physical_device(dev->vk.physical);

   list_addtail(&batch->node, &cmdbuf->batches);

   if (batch->jc.first_tiler) {
      struct panfrost_ptr preload_jobs[2];
      unsigned num_preload_jobs = GENX(pan_preload_fb)(
         &dev->meta.blitter.cache, &cmdbuf->desc_pool.base, &batch->jc,
         &cmdbuf->state.fb.info, batch->tls.gpu, batch->tiler.ctx_desc.gpu,
         preload_jobs);
      for (unsigned i = 0; i < num_preload_jobs; i++)
         util_dynarray_append(&batch->jobs, void *, preload_jobs[i].cpu);
   }

   if (batch->tlsinfo.tls.size) {
      unsigned thread_tls_alloc =
         panfrost_query_thread_tls_alloc(&phys_dev->kmod.props);
      unsigned core_id_range;

      panfrost_query_core_count(&phys_dev->kmod.props, &core_id_range);

      unsigned size = panfrost_get_total_stack_size(
         batch->tlsinfo.tls.size, thread_tls_alloc, core_id_range);
      batch->tlsinfo.tls.ptr =
         pan_pool_alloc_aligned(&cmdbuf->tls_pool.base, size, 4096).gpu;
   }

   if (batch->tlsinfo.wls.size) {
      assert(batch->wls_total_size);
      batch->tlsinfo.wls.ptr =
         pan_pool_alloc_aligned(&cmdbuf->tls_pool.base, batch->wls_total_size,
                                4096)
            .gpu;
   }

   if (batch->tls.cpu)
      GENX(pan_emit_tls)(&batch->tlsinfo, batch->tls.cpu);

   if (batch->fb.desc.cpu) {
      fbinfo->sample_positions = dev->sample_positions->addr.dev +
                                 panfrost_sample_positions_offset(
                                    pan_sample_pattern(fbinfo->nr_samples));

      batch->fb.desc.gpu |=
         GENX(pan_emit_fbd)(&cmdbuf->state.fb.info, &batch->tlsinfo,
                            &batch->tiler.ctx, batch->fb.desc.cpu);

      panvk_cmd_prepare_fragment_job(cmdbuf);
   }

   cmdbuf->state.batch = NULL;
}

void
panvk_per_arch(cmd_alloc_fb_desc)(struct panvk_cmd_buffer *cmdbuf)
{
   struct panvk_batch *batch = cmdbuf->state.batch;

   if (batch->fb.desc.gpu)
      return;

   const struct pan_fb_info *fbinfo = &cmdbuf->state.fb.info;
   bool has_zs_ext = fbinfo->zs.view.zs || fbinfo->zs.view.s;

   batch->fb.bo_count = cmdbuf->state.fb.bo_count;
   memcpy(batch->fb.bos, cmdbuf->state.fb.bos,
          batch->fb.bo_count * sizeof(batch->fb.bos[0]));
   batch->fb.desc = pan_pool_alloc_desc_aggregate(
      &cmdbuf->desc_pool.base, PAN_DESC(FRAMEBUFFER),
      PAN_DESC_ARRAY(has_zs_ext ? 1 : 0, ZS_CRC_EXTENSION),
      PAN_DESC_ARRAY(MAX2(fbinfo->rt_count, 1), RENDER_TARGET));

   memset(&cmdbuf->state.fb.info.bifrost.pre_post.dcds, 0,
          sizeof(cmdbuf->state.fb.info.bifrost.pre_post.dcds));
}

void
panvk_per_arch(cmd_alloc_tls_desc)(struct panvk_cmd_buffer *cmdbuf, bool gfx)
{
   struct panvk_batch *batch = cmdbuf->state.batch;

   assert(batch);
   if (!batch->tls.gpu) {
      batch->tls = pan_pool_alloc_desc(&cmdbuf->desc_pool.base, LOCAL_STORAGE);
   }
}

/*
 * Upload the viewport scale. Defined as (px/2, py/2, pz) at the start of
 * section 24.5 ("Controlling the Viewport") of the Vulkan spec. At the end of
 * the section, the spec defines:
 *
 * px = width
 * py = height
 * pz = maxDepth - minDepth
 */
static void
panvk_sysval_upload_viewport_scale(const VkViewport *viewport,
                                   union panvk_sysval_vec4 *data)
{
   data->f32[0] = 0.5f * viewport->width;
   data->f32[1] = 0.5f * viewport->height;
   data->f32[2] = (viewport->maxDepth - viewport->minDepth);
}

/*
 * Upload the viewport offset. Defined as (ox, oy, oz) at the start of section
 * 24.5 ("Controlling the Viewport") of the Vulkan spec. At the end of the
 * section, the spec defines:
 *
 * ox = x + width/2
 * oy = y + height/2
 * oz = minDepth
 */
static void
panvk_sysval_upload_viewport_offset(const VkViewport *viewport,
                                    union panvk_sysval_vec4 *data)
{
   data->f32[0] = (0.5f * viewport->width) + viewport->x;
   data->f32[1] = (0.5f * viewport->height) + viewport->y;
   data->f32[2] = viewport->minDepth;
}

static void
panvk_cmd_prepare_draw_sysvals(
   struct panvk_cmd_buffer *cmdbuf,
   struct panvk_cmd_bind_point_state *bind_point_state,
   struct panvk_draw_info *draw)
{
   struct panvk_sysvals *sysvals = &bind_point_state->desc_state.sysvals;

   unsigned base_vertex = draw->index_size ? draw->vertex_offset : 0;
   if (sysvals->first_vertex != draw->offset_start ||
       sysvals->base_vertex != base_vertex ||
       sysvals->base_instance != draw->first_instance) {
      sysvals->first_vertex = draw->offset_start;
      sysvals->base_vertex = base_vertex;
      sysvals->base_instance = draw->first_instance;
      bind_point_state->desc_state.sysvals_ptr = 0;
   }

   if (cmdbuf->state.dirty & PANVK_DYNAMIC_BLEND_CONSTANTS) {
      memcpy(&sysvals->blend_constants, cmdbuf->state.blend.constants,
             sizeof(cmdbuf->state.blend.constants));
      bind_point_state->desc_state.sysvals_ptr = 0;
   }

   if (cmdbuf->state.dirty & PANVK_DYNAMIC_VIEWPORT) {
      panvk_sysval_upload_viewport_scale(&cmdbuf->state.viewport,
                                         &sysvals->viewport_scale);
      panvk_sysval_upload_viewport_offset(&cmdbuf->state.viewport,
                                          &sysvals->viewport_offset);
      bind_point_state->desc_state.sysvals_ptr = 0;
   }
}

static void
panvk_cmd_prepare_sysvals(struct panvk_cmd_buffer *cmdbuf,
                          struct panvk_cmd_bind_point_state *bind_point_state)
{
   struct panvk_descriptor_state *desc_state = &bind_point_state->desc_state;

   if (desc_state->sysvals_ptr)
      return;

   struct panfrost_ptr sysvals = pan_pool_alloc_aligned(
      &cmdbuf->desc_pool.base, sizeof(desc_state->sysvals), 16);
   memcpy(sysvals.cpu, &desc_state->sysvals, sizeof(desc_state->sysvals));
   desc_state->sysvals_ptr = sysvals.gpu;
}

static void
panvk_cmd_prepare_push_constants(
   struct panvk_cmd_buffer *cmdbuf,
   struct panvk_cmd_bind_point_state *bind_point_state)
{
   struct panvk_descriptor_state *desc_state = &bind_point_state->desc_state;
   const struct panvk_pipeline *pipeline = bind_point_state->pipeline;

   if (!pipeline->layout->push_constants.size || desc_state->push_constants)
      return;

   struct panfrost_ptr push_constants = pan_pool_alloc_aligned(
      &cmdbuf->desc_pool.base,
      ALIGN_POT(pipeline->layout->push_constants.size, 16), 16);

   memcpy(push_constants.cpu, cmdbuf->push_constants,
          pipeline->layout->push_constants.size);
   desc_state->push_constants = push_constants.gpu;
}

static void
panvk_cmd_prepare_push_sets(struct panvk_cmd_buffer *cmdbuf,
                            struct panvk_cmd_bind_point_state *bind_point_state)
{
   struct panvk_descriptor_state *desc_state = &bind_point_state->desc_state;
   const struct panvk_pipeline *pipeline = bind_point_state->pipeline;
   const struct panvk_pipeline_layout *playout = pipeline->layout;

   for (unsigned i = 0; i < playout->vk.set_count; i++) {
      const struct panvk_descriptor_set_layout *slayout =
         vk_to_panvk_descriptor_set_layout(playout->vk.set_layouts[i]);
      bool is_push_set =
         slayout->flags &
         VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;

      if (desc_state->sets[i] || !is_push_set || !desc_state->push_sets[i])
         continue;

      struct panvk_descriptor_set *set = &desc_state->push_sets[i]->set;

      panvk_per_arch(push_descriptor_set_assign_layout)(desc_state->push_sets[i],
                                                        slayout);
      if (slayout->desc_ubo_size) {
         struct panfrost_ptr desc_ubo = pan_pool_alloc_aligned(
            &cmdbuf->desc_pool.base, slayout->desc_ubo_size, 16);
         struct mali_uniform_buffer_packed *ubos = set->ubos;

         memcpy(desc_ubo.cpu, set->desc_ubo.addr.host, slayout->desc_ubo_size);
         set->desc_ubo.addr.dev = desc_ubo.gpu;
         set->desc_ubo.addr.host = desc_ubo.cpu;

         pan_pack(&ubos[slayout->desc_ubo_index], UNIFORM_BUFFER, cfg) {
            cfg.pointer = set->desc_ubo.addr.dev;
            cfg.entries = DIV_ROUND_UP(slayout->desc_ubo_size, 16);
         }
      }

      desc_state->sets[i] = &desc_state->push_sets[i]->set;
   }
}

static void
panvk_cmd_unprepare_push_sets(struct panvk_cmd_buffer *cmdbuf,
                              struct panvk_cmd_bind_point_state *bind_point_state)
{
   struct panvk_descriptor_state *desc_state = &bind_point_state->desc_state;

   for (unsigned i = 0; i < ARRAY_SIZE(desc_state->sets); i++) {
      if (desc_state->push_sets[i] && &desc_state->push_sets[i]->set == desc_state->sets[i])
         desc_state->sets[i] = NULL;
   }
}

static void
panvk_cmd_prepare_ubos(struct panvk_cmd_buffer *cmdbuf,
                       struct panvk_cmd_bind_point_state *bind_point_state)
{
   struct panvk_descriptor_state *desc_state = &bind_point_state->desc_state;
   const struct panvk_pipeline *pipeline = bind_point_state->pipeline;

   if (!pipeline->num_ubos || desc_state->ubos)
      return;

   panvk_cmd_prepare_sysvals(cmdbuf, bind_point_state);

   struct panfrost_ptr ubos = pan_pool_alloc_desc_array(
      &cmdbuf->desc_pool.base, pipeline->num_ubos, UNIFORM_BUFFER);
   struct mali_uniform_buffer_packed *ubo_descs = ubos.cpu;

   pan_pack(&ubo_descs[PANVK_SYSVAL_UBO_INDEX], UNIFORM_BUFFER, cfg) {
      cfg.pointer = desc_state->sysvals_ptr;
      cfg.entries = DIV_ROUND_UP(sizeof(desc_state->sysvals), 16);
   }

   for (unsigned s = 0; s < pipeline->layout->vk.set_count; s++) {
      const struct panvk_descriptor_set_layout *set_layout =
         vk_to_panvk_descriptor_set_layout(pipeline->layout->vk.set_layouts[s]);
      const struct panvk_descriptor_set *set = desc_state->sets[s];

      unsigned ubo_start =
         panvk_per_arch(pipeline_layout_ubo_start)(pipeline->layout, s, false);

      if (!set) {
         unsigned all_ubos = set_layout->num_ubos + set_layout->num_dyn_ubos;
         memset(&ubo_descs[ubo_start], 0, all_ubos * sizeof(*ubo_descs));
      } else {
         memcpy(&ubo_descs[ubo_start], set->ubos,
                set_layout->num_ubos * sizeof(*ubo_descs));

         unsigned dyn_ubo_start = panvk_per_arch(pipeline_layout_ubo_start)(
            pipeline->layout, s, true);

         for (unsigned i = 0; i < set_layout->num_dyn_ubos; i++) {
            const unsigned ubo_idx =
               pipeline->layout->sets[s].dyn_ubo_offset + i;
            const struct panvk_buffer_desc *bdesc =
               &desc_state->dyn.ubos[ubo_idx];

            mali_ptr address =
               panvk_buffer_gpu_ptr(bdesc->buffer, bdesc->offset);
            size_t size =
               panvk_buffer_range(bdesc->buffer, bdesc->offset, bdesc->size);
            if (size) {
               pan_pack(&ubo_descs[dyn_ubo_start + i], UNIFORM_BUFFER, cfg) {
                  cfg.pointer = address;
                  cfg.entries = DIV_ROUND_UP(size, 16);
               }
            } else {
               memset(&ubo_descs[dyn_ubo_start + i], 0, sizeof(*ubo_descs));
            }
         }
      }
   }

   desc_state->ubos = ubos.gpu;
}

static void
panvk_cmd_prepare_textures(struct panvk_cmd_buffer *cmdbuf,
                           struct panvk_cmd_bind_point_state *bind_point_state)
{
   struct panvk_descriptor_state *desc_state = &bind_point_state->desc_state;
   const struct panvk_pipeline *pipeline = bind_point_state->pipeline;
   unsigned num_textures = pipeline->layout->num_textures;

   if (!num_textures || desc_state->textures)
      return;

   struct panfrost_ptr textures = pan_pool_alloc_aligned(
      &cmdbuf->desc_pool.base, num_textures * pan_size(TEXTURE),
      pan_size(TEXTURE));

   void *texture = textures.cpu;

   for (unsigned i = 0; i < ARRAY_SIZE(desc_state->sets); i++) {
      if (!desc_state->sets[i])
         continue;

      memcpy(texture, desc_state->sets[i]->textures,
             desc_state->sets[i]->layout->num_textures * pan_size(TEXTURE));

      texture += desc_state->sets[i]->layout->num_textures * pan_size(TEXTURE);
   }

   desc_state->textures = textures.gpu;
}

static void
panvk_cmd_prepare_samplers(struct panvk_cmd_buffer *cmdbuf,
                           struct panvk_cmd_bind_point_state *bind_point_state)
{
   struct panvk_descriptor_state *desc_state = &bind_point_state->desc_state;
   const struct panvk_pipeline *pipeline = bind_point_state->pipeline;
   unsigned num_samplers = pipeline->layout->num_samplers;

   if (!num_samplers || desc_state->samplers)
      return;

   struct panfrost_ptr samplers =
      pan_pool_alloc_desc_array(&cmdbuf->desc_pool.base, num_samplers, SAMPLER);

   void *sampler = samplers.cpu;

   /* Prepare the dummy sampler */
   pan_pack(sampler, SAMPLER, cfg) {
      cfg.seamless_cube_map = false;
      cfg.magnify_nearest = true;
      cfg.minify_nearest = true;
      cfg.normalized_coordinates = false;
   }

   sampler += pan_size(SAMPLER);

   for (unsigned i = 0; i < ARRAY_SIZE(desc_state->sets); i++) {
      if (!desc_state->sets[i])
         continue;

      memcpy(sampler, desc_state->sets[i]->samplers,
             desc_state->sets[i]->layout->num_samplers * pan_size(SAMPLER));

      sampler += desc_state->sets[i]->layout->num_samplers * pan_size(SAMPLER);
   }

   desc_state->samplers = samplers.gpu;
}

static void
panvk_draw_prepare_fs_rsd(struct panvk_cmd_buffer *cmdbuf,
                          struct panvk_draw_info *draw)
{
   const struct panvk_pipeline *pipeline =
      panvk_cmd_get_pipeline(cmdbuf, GRAPHICS);

   if (!pipeline->fs.dynamic_rsd) {
      draw->fs_rsd = pipeline->rsds[MESA_SHADER_FRAGMENT];
      return;
   }

   if (!cmdbuf->state.fs_rsd) {
      const struct panvk_cmd_state *state = &cmdbuf->state;
      struct panfrost_ptr rsd = pan_pool_alloc_desc_aggregate(
         &cmdbuf->desc_pool.base, PAN_DESC(RENDERER_STATE),
         PAN_DESC_ARRAY(pipeline->blend.state.rt_count, BLEND));

      struct mali_renderer_state_packed rsd_dyn;
      struct mali_renderer_state_packed *rsd_templ =
         (struct mali_renderer_state_packed *)&pipeline->fs.rsd_template;

      STATIC_ASSERT(sizeof(pipeline->fs.rsd_template) >= sizeof(*rsd_templ));

      pan_pack(&rsd_dyn, RENDERER_STATE, cfg) {
         if (pipeline->dynamic_state_mask &
             (1 << VK_DYNAMIC_STATE_DEPTH_BIAS)) {
            cfg.depth_units = state->rast.depth_bias.constant_factor * 2.0f;
            cfg.depth_factor = state->rast.depth_bias.slope_factor;
            cfg.depth_bias_clamp = state->rast.depth_bias.clamp;
         }

         if (pipeline->dynamic_state_mask &
             (1 << VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK)) {
            cfg.stencil_front.mask = state->zs.s_front.compare_mask;
            cfg.stencil_back.mask = state->zs.s_back.compare_mask;
         }

         if (pipeline->dynamic_state_mask &
             (1 << VK_DYNAMIC_STATE_STENCIL_WRITE_MASK)) {
            cfg.stencil_mask_misc.stencil_mask_front =
               state->zs.s_front.write_mask;
            cfg.stencil_mask_misc.stencil_mask_back =
               state->zs.s_back.write_mask;
         }

         if (pipeline->dynamic_state_mask &
             (1 << VK_DYNAMIC_STATE_STENCIL_REFERENCE)) {
            cfg.stencil_front.reference_value = state->zs.s_front.ref;
            cfg.stencil_back.reference_value = state->zs.s_back.ref;
         }
      }

      pan_merge(rsd_dyn, (*rsd_templ), RENDERER_STATE);
      memcpy(rsd.cpu, &rsd_dyn, sizeof(rsd_dyn));

      void *bd = rsd.cpu + pan_size(RENDERER_STATE);
      for (unsigned i = 0; i < pipeline->blend.state.rt_count; i++) {
         if (pipeline->blend.constant[i].index != (uint8_t)~0) {
            struct mali_blend_packed bd_dyn;
            struct mali_blend_packed *bd_templ =
               (struct mali_blend_packed *)&pipeline->blend.bd_template[i];

            float constant =
               cmdbuf->state.blend.constants[pipeline->blend.constant[i].index] *
               pipeline->blend.constant[i].bifrost_factor;

            pan_pack(&bd_dyn, BLEND, cfg) {
               cfg.enable = false;
               cfg.constant = constant;
            }

            pan_merge(bd_dyn, (*bd_templ), BLEND);
            memcpy(bd, &bd_dyn, sizeof(bd_dyn));
         }
         bd += pan_size(BLEND);
      }

      cmdbuf->state.fs_rsd = rsd.gpu;
   }

   draw->fs_rsd = cmdbuf->state.fs_rsd;
}

void
panvk_per_arch(cmd_get_tiler_context)(struct panvk_cmd_buffer *cmdbuf,
                                      unsigned width, unsigned height)
{
   struct panvk_device *dev = to_panvk_device(cmdbuf->vk.base.device);
   struct pan_fb_info *fbinfo = &cmdbuf->state.fb.info;
   struct panvk_batch *batch = cmdbuf->state.batch;

   if (batch->tiler.ctx_desc.cpu)
      return;

   batch->tiler.heap_desc =
      pan_pool_alloc_desc(&cmdbuf->desc_pool.base, TILER_HEAP);
   batch->tiler.ctx_desc =
      pan_pool_alloc_desc(&cmdbuf->desc_pool.base, TILER_CONTEXT);

   pan_pack(&batch->tiler.heap_templ, TILER_HEAP, cfg) {
      cfg.size = pan_kmod_bo_size(dev->tiler_heap->bo);
      cfg.base = dev->tiler_heap->addr.dev;
      cfg.bottom = dev->tiler_heap->addr.dev;
      cfg.top = cfg.base + cfg.size;
   }

   pan_pack(&batch->tiler.ctx_templ, TILER_CONTEXT, cfg) {
      cfg.hierarchy_mask = 0x28;
      cfg.fb_width = width;
      cfg.fb_height = height;
      cfg.heap = batch->tiler.heap_desc.gpu;
      cfg.sample_pattern = pan_sample_pattern(fbinfo->nr_samples);
   }

   memcpy(batch->tiler.heap_desc.cpu, &batch->tiler.heap_templ,
          sizeof(batch->tiler.heap_templ));
   memcpy(batch->tiler.ctx_desc.cpu, &batch->tiler.ctx_templ,
          sizeof(batch->tiler.ctx_templ));
   batch->tiler.ctx.bifrost = batch->tiler.ctx_desc.gpu;
}

void
panvk_per_arch(cmd_prepare_tiler_context)(struct panvk_cmd_buffer *cmdbuf)
{
   const struct pan_fb_info *fbinfo = &cmdbuf->state.fb.info;

   panvk_per_arch(cmd_get_tiler_context)(cmdbuf, fbinfo->width, fbinfo->height);
}

static void
panvk_draw_prepare_tiler_context(struct panvk_cmd_buffer *cmdbuf,
                                 struct panvk_draw_info *draw)
{
   struct panvk_batch *batch = cmdbuf->state.batch;

   panvk_per_arch(cmd_prepare_tiler_context)(cmdbuf);
   draw->tiler_ctx = &batch->tiler.ctx;
}

static mali_pixel_format
panvk_varying_hw_format(gl_shader_stage stage, gl_varying_slot loc,
                        enum pipe_format pfmt)
{
   switch (loc) {
   case VARYING_SLOT_PNTC:
   case VARYING_SLOT_PSIZ:
#if PAN_ARCH <= 6
      return (MALI_R16F << 12) | panfrost_get_default_swizzle(1);
#else
      return (MALI_R16F << 12) | MALI_RGB_COMPONENT_ORDER_R000;
#endif
   case VARYING_SLOT_POS:
#if PAN_ARCH <= 6
      return (MALI_SNAP_4 << 12) | panfrost_get_default_swizzle(4);
#else
      return (MALI_SNAP_4 << 12) | MALI_RGB_COMPONENT_ORDER_RGBA;
#endif
   default:
      if (pfmt != PIPE_FORMAT_NONE)
         return GENX(panfrost_format_from_pipe_format)(pfmt)->hw;

#if PAN_ARCH >= 7
      return (MALI_CONSTANT << 12) | MALI_RGB_COMPONENT_ORDER_0000;
#else
      return (MALI_CONSTANT << 12) | PAN_V6_SWIZZLE(0, 0, 0, 0);
#endif
   }
}

static void
panvk_draw_prepare_varyings(struct panvk_cmd_buffer *cmdbuf,
                            struct panvk_draw_info *draw)
{
   const struct panvk_pipeline *pipeline =
      panvk_cmd_get_pipeline(cmdbuf, GRAPHICS);
   struct panvk_varyings_info *varyings = &cmdbuf->state.varyings;

   panvk_varyings_alloc(varyings, &cmdbuf->varying_pool.base,
                        draw->padded_vertex_count * draw->instance_count);

   unsigned buf_count = panvk_varyings_buf_count(varyings);
   struct panfrost_ptr bufs = pan_pool_alloc_desc_array(
      &cmdbuf->desc_pool.base, buf_count + 1, ATTRIBUTE_BUFFER);
   struct mali_attribute_buffer_packed *buf_descs = bufs.cpu;

   for (unsigned i = 0, buf_idx = 0; i < PANVK_VARY_BUF_MAX; i++) {
      if (varyings->buf_mask & (1 << i)) {
         pan_pack(&buf_descs[buf_idx], ATTRIBUTE_BUFFER, cfg) {
            unsigned offset = varyings->buf[buf_idx].address & 63;

            cfg.stride = varyings->buf[buf_idx].stride;
            cfg.size = varyings->buf[buf_idx].size + offset;
            cfg.pointer = varyings->buf[buf_idx].address & ~63ULL;
         }

         buf_idx++;
      }
   }

   /* We need an empty entry to stop prefetching on Bifrost */
   memset(bufs.cpu + (pan_size(ATTRIBUTE_BUFFER) * buf_count), 0,
          pan_size(ATTRIBUTE_BUFFER));

   if (BITSET_TEST(varyings->active, VARYING_SLOT_POS)) {
      draw->position =
         varyings->buf[varyings->varying[VARYING_SLOT_POS].buf].address +
         varyings->varying[VARYING_SLOT_POS].offset;
   }

   if (pipeline->ia.writes_point_size) {
      draw->psiz =
         varyings->buf[varyings->varying[VARYING_SLOT_PSIZ].buf].address +
         varyings->varying[VARYING_SLOT_POS].offset;
   } else if (pipeline->ia.topology == MALI_DRAW_MODE_LINES ||
              pipeline->ia.topology == MALI_DRAW_MODE_LINE_STRIP ||
              pipeline->ia.topology == MALI_DRAW_MODE_LINE_LOOP) {
      draw->line_width = pipeline->dynamic_state_mask & PANVK_DYNAMIC_LINE_WIDTH
                            ? cmdbuf->state.rast.line_width
                            : pipeline->rast.line_width;
   } else {
      draw->line_width = 1.0f;
   }
   draw->varying_bufs = bufs.gpu;

   for (unsigned s = 0; s < MESA_SHADER_STAGES; s++) {
      if (!varyings->stage[s].count)
         continue;

      struct panfrost_ptr attribs = pan_pool_alloc_desc_array(
         &cmdbuf->desc_pool.base, varyings->stage[s].count, ATTRIBUTE);
      struct mali_attribute_packed *attrib_descs = attribs.cpu;

      draw->stages[s].varyings = attribs.gpu;
      for (unsigned i = 0; i < varyings->stage[s].count; i++) {
         gl_varying_slot loc = varyings->stage[s].loc[i];

         pan_pack(&attrib_descs[i], ATTRIBUTE, cfg) {
            cfg.buffer_index = varyings->varying[loc].buf;
            cfg.offset = varyings->varying[loc].offset;
            cfg.offset_enable = false;
            cfg.format =
               panvk_varying_hw_format(s, loc, varyings->varying[loc].format);
         }
      }
   }
}

static void
panvk_fill_non_vs_attribs(struct panvk_cmd_buffer *cmdbuf,
                          struct panvk_cmd_bind_point_state *bind_point_state,
                          void *attrib_bufs, void *attribs, unsigned first_buf)
{
   struct panvk_descriptor_state *desc_state = &bind_point_state->desc_state;
   const struct panvk_pipeline *pipeline = bind_point_state->pipeline;

   for (unsigned s = 0; s < pipeline->layout->vk.set_count; s++) {
      const struct panvk_descriptor_set *set = desc_state->sets[s];

      if (!set)
         continue;

      const struct panvk_descriptor_set_layout *layout = set->layout;
      unsigned img_idx = pipeline->layout->sets[s].img_offset;
      unsigned offset = img_idx * pan_size(ATTRIBUTE_BUFFER) * 2;
      unsigned size = layout->num_imgs * pan_size(ATTRIBUTE_BUFFER) * 2;

      memcpy(attrib_bufs + offset, desc_state->sets[s]->img_attrib_bufs, size);

      offset = img_idx * pan_size(ATTRIBUTE);
      for (unsigned i = 0; i < layout->num_imgs; i++) {
         pan_pack(attribs + offset, ATTRIBUTE, cfg) {
            cfg.buffer_index = first_buf + (img_idx + i) * 2;
            cfg.format = desc_state->sets[s]->img_fmts[i];
            cfg.offset_enable = false;
         }
         offset += pan_size(ATTRIBUTE);
      }
   }
}

static void
panvk_prepare_non_vs_attribs(struct panvk_cmd_buffer *cmdbuf,
                             struct panvk_cmd_bind_point_state *bind_point_state)
{
   struct panvk_descriptor_state *desc_state = &bind_point_state->desc_state;
   const struct panvk_pipeline *pipeline = bind_point_state->pipeline;

   if (desc_state->non_vs_attribs || !pipeline->img_access_mask)
      return;

   unsigned attrib_count = pipeline->layout->num_imgs;
   unsigned attrib_buf_count = (pipeline->layout->num_imgs * 2);
   struct panfrost_ptr bufs = pan_pool_alloc_desc_array(
      &cmdbuf->desc_pool.base, attrib_buf_count + 1, ATTRIBUTE_BUFFER);
   struct panfrost_ptr attribs = pan_pool_alloc_desc_array(
      &cmdbuf->desc_pool.base, attrib_count, ATTRIBUTE);

   panvk_fill_non_vs_attribs(cmdbuf, bind_point_state, bufs.cpu, attribs.cpu,
                             0);

   desc_state->non_vs_attrib_bufs = bufs.gpu;
   desc_state->non_vs_attribs = attribs.gpu;
}

static void
panvk_draw_emit_attrib_buf(const struct panvk_draw_info *draw,
                           const struct panvk_attrib_buf_info *buf_info,
                           const struct panvk_attrib_buf *buf, void *desc)
{
   mali_ptr addr = buf->address & ~63ULL;
   unsigned size = buf->size + (buf->address & 63);
   unsigned divisor = draw->padded_vertex_count * buf_info->instance_divisor;

   /* TODO: support instanced arrays */
   if (draw->instance_count <= 1) {
      pan_pack(desc, ATTRIBUTE_BUFFER, cfg) {
         cfg.type = MALI_ATTRIBUTE_TYPE_1D;
         cfg.stride = buf_info->per_instance ? 0 : buf_info->stride;
         cfg.pointer = addr;
         cfg.size = size;
      }
   } else if (!buf_info->per_instance) {
      pan_pack(desc, ATTRIBUTE_BUFFER, cfg) {
         cfg.type = MALI_ATTRIBUTE_TYPE_1D_MODULUS;
         cfg.divisor = draw->padded_vertex_count;
         cfg.stride = buf_info->stride;
         cfg.pointer = addr;
         cfg.size = size;
      }
   } else if (!divisor) {
      /* instance_divisor == 0 means all instances share the same value.
       * Make it a 1D array with a zero stride.
       */
      pan_pack(desc, ATTRIBUTE_BUFFER, cfg) {
         cfg.type = MALI_ATTRIBUTE_TYPE_1D;
         cfg.stride = 0;
         cfg.pointer = addr;
         cfg.size = size;
      }
   } else if (util_is_power_of_two_or_zero(divisor)) {
      pan_pack(desc, ATTRIBUTE_BUFFER, cfg) {
         cfg.type = MALI_ATTRIBUTE_TYPE_1D_POT_DIVISOR;
         cfg.stride = buf_info->stride;
         cfg.pointer = addr;
         cfg.size = size;
         cfg.divisor_r = __builtin_ctz(divisor);
      }
   } else {
      unsigned divisor_r = 0, divisor_e = 0;
      unsigned divisor_num =
         panfrost_compute_magic_divisor(divisor, &divisor_r, &divisor_e);
      pan_pack(desc, ATTRIBUTE_BUFFER, cfg) {
         cfg.type = MALI_ATTRIBUTE_TYPE_1D_NPOT_DIVISOR;
         cfg.stride = buf_info->stride;
         cfg.pointer = addr;
         cfg.size = size;
         cfg.divisor_r = divisor_r;
         cfg.divisor_e = divisor_e;
      }

      desc += pan_size(ATTRIBUTE_BUFFER);
      pan_pack(desc, ATTRIBUTE_BUFFER_CONTINUATION_NPOT, cfg) {
         cfg.divisor_numerator = divisor_num;
         cfg.divisor = buf_info->instance_divisor;
      }
   }
}

static void
panvk_draw_emit_attrib(const struct panvk_draw_info *draw,
                       const struct panvk_attrib_info *attrib_info,
                       const struct panvk_attrib_buf_info *buf_info,
                       const struct panvk_attrib_buf *buf, void *desc)
{
   enum pipe_format f = attrib_info->format;
   unsigned buf_idx = attrib_info->buf;

   pan_pack(desc, ATTRIBUTE, cfg) {
      cfg.buffer_index = buf_idx * 2;
      cfg.offset = attrib_info->offset + (buf->address & 63);
      cfg.offset_enable = true;

      if (buf_info->per_instance)
         cfg.offset += draw->first_instance * buf_info->stride;

      cfg.format = GENX(panfrost_format_from_pipe_format)(f)->hw;
   }
}

static void
panvk_draw_prepare_vs_attribs(struct panvk_cmd_buffer *cmdbuf,
                              struct panvk_draw_info *draw)
{
   struct panvk_cmd_bind_point_state *bind_point_state =
      panvk_cmd_get_bind_point_state(cmdbuf, GRAPHICS);
   struct panvk_descriptor_state *desc_state = &bind_point_state->desc_state;
   const struct panvk_pipeline *pipeline = bind_point_state->pipeline;
   unsigned num_imgs =
      pipeline->img_access_mask & BITFIELD_BIT(MESA_SHADER_VERTEX)
         ? pipeline->layout->num_imgs
         : 0;
   unsigned attrib_count = pipeline->attribs.attrib_count + num_imgs;

   if (desc_state->vs_attribs || !attrib_count)
      return;

   if (!pipeline->attribs.buf_count) {
      panvk_prepare_non_vs_attribs(cmdbuf, bind_point_state);
      desc_state->vs_attrib_bufs = desc_state->non_vs_attrib_bufs;
      desc_state->vs_attribs = desc_state->non_vs_attribs;
      return;
   }

   unsigned attrib_buf_count = pipeline->attribs.buf_count * 2;
   struct panfrost_ptr bufs = pan_pool_alloc_desc_array(
      &cmdbuf->desc_pool.base, attrib_buf_count + 1, ATTRIBUTE_BUFFER);
   struct mali_attribute_buffer_packed *attrib_buf_descs = bufs.cpu;
   struct panfrost_ptr attribs = pan_pool_alloc_desc_array(
      &cmdbuf->desc_pool.base, attrib_count, ATTRIBUTE);
   struct mali_attribute_packed *attrib_descs = attribs.cpu;

   for (unsigned i = 0; i < pipeline->attribs.buf_count; i++) {
      panvk_draw_emit_attrib_buf(draw, &pipeline->attribs.buf[i],
                                 &cmdbuf->state.vb.bufs[i],
                                 &attrib_buf_descs[i * 2]);
   }

   for (unsigned i = 0; i < pipeline->attribs.attrib_count; i++) {
      unsigned buf_idx = pipeline->attribs.attrib[i].buf;

      panvk_draw_emit_attrib(draw, &pipeline->attribs.attrib[i],
                             &pipeline->attribs.buf[buf_idx],
                             &cmdbuf->state.vb.bufs[buf_idx], &attrib_descs[i]);
   }

   if (attrib_count > pipeline->attribs.attrib_count) {
      unsigned bufs_offset =
         pipeline->attribs.buf_count * pan_size(ATTRIBUTE_BUFFER) * 2;
      unsigned attribs_offset =
         pipeline->attribs.buf_count * pan_size(ATTRIBUTE);

      panvk_fill_non_vs_attribs(
         cmdbuf, bind_point_state, bufs.cpu + bufs_offset,
         attribs.cpu + attribs_offset, pipeline->attribs.buf_count * 2);
   }

   /* A NULL entry is needed to stop prefecting on Bifrost */
   memset(bufs.cpu + (pan_size(ATTRIBUTE_BUFFER) * attrib_buf_count), 0,
          pan_size(ATTRIBUTE_BUFFER));

   desc_state->vs_attrib_bufs = bufs.gpu;
   desc_state->vs_attribs = attribs.gpu;
}

static void
panvk_draw_prepare_attributes(struct panvk_cmd_buffer *cmdbuf,
                              struct panvk_draw_info *draw)
{
   struct panvk_cmd_bind_point_state *bind_point_state =
      panvk_cmd_get_bind_point_state(cmdbuf, GRAPHICS);
   struct panvk_descriptor_state *desc_state = &bind_point_state->desc_state;
   const struct panvk_pipeline *pipeline = bind_point_state->pipeline;

   for (unsigned i = 0; i < ARRAY_SIZE(draw->stages); i++) {
      if (i == MESA_SHADER_VERTEX) {
         panvk_draw_prepare_vs_attribs(cmdbuf, draw);
         draw->stages[i].attributes = desc_state->vs_attribs;
         draw->stages[i].attribute_bufs = desc_state->vs_attrib_bufs;
      } else if (pipeline->img_access_mask & BITFIELD_BIT(i)) {
         panvk_prepare_non_vs_attribs(cmdbuf, bind_point_state);
         draw->stages[i].attributes = desc_state->non_vs_attribs;
         draw->stages[i].attribute_bufs = desc_state->non_vs_attrib_bufs;
      }
   }
}

void
panvk_per_arch(emit_viewport)(const VkViewport *viewport,
                              const VkRect2D *scissor, void *vpd)
{
   /* The spec says "width must be greater than 0.0" */
   assert(viewport->x >= 0);
   int minx = (int)viewport->x;
   int maxx = (int)(viewport->x + viewport->width);

   /* Viewport height can be negative */
   int miny = MIN2((int)viewport->y, (int)(viewport->y + viewport->height));
   int maxy = MAX2((int)viewport->y, (int)(viewport->y + viewport->height));

   assert(scissor->offset.x >= 0 && scissor->offset.y >= 0);
   miny = MAX2(scissor->offset.x, minx);
   miny = MAX2(scissor->offset.y, miny);
   maxx = MIN2(scissor->offset.x + scissor->extent.width, maxx);
   maxy = MIN2(scissor->offset.y + scissor->extent.height, maxy);

   /* Make sure we don't end up with a max < min when width/height is 0 */
   maxx = maxx > minx ? maxx - 1 : maxx;
   maxy = maxy > miny ? maxy - 1 : maxy;

   assert(viewport->minDepth >= 0.0f && viewport->minDepth <= 1.0f);
   assert(viewport->maxDepth >= 0.0f && viewport->maxDepth <= 1.0f);

   pan_pack(vpd, VIEWPORT, cfg) {
      cfg.scissor_minimum_x = minx;
      cfg.scissor_minimum_y = miny;
      cfg.scissor_maximum_x = maxx;
      cfg.scissor_maximum_y = maxy;
      cfg.minimum_z = MIN2(viewport->minDepth, viewport->maxDepth);
      cfg.maximum_z = MAX2(viewport->minDepth, viewport->maxDepth);
   }
}

static void
panvk_draw_prepare_viewport(struct panvk_cmd_buffer *cmdbuf,
                            struct panvk_draw_info *draw)
{
   const struct panvk_pipeline *pipeline =
      panvk_cmd_get_pipeline(cmdbuf, GRAPHICS);

   if (pipeline->vpd) {
      draw->viewport = pipeline->vpd;
   } else if (cmdbuf->state.vpd) {
      draw->viewport = cmdbuf->state.vpd;
   } else {
      struct panfrost_ptr vp =
         pan_pool_alloc_desc(&cmdbuf->desc_pool.base, VIEWPORT);

      const VkViewport *viewport =
         pipeline->dynamic_state_mask & PANVK_DYNAMIC_VIEWPORT
            ? &cmdbuf->state.viewport
            : &pipeline->viewport;
      const VkRect2D *scissor =
         pipeline->dynamic_state_mask & PANVK_DYNAMIC_SCISSOR
            ? &cmdbuf->state.scissor
            : &pipeline->scissor;

      panvk_per_arch(emit_viewport)(viewport, scissor, vp.cpu);
      draw->viewport = cmdbuf->state.vpd = vp.gpu;
   }
}

static void
panvk_draw_prepare_vertex_job(struct panvk_cmd_buffer *cmdbuf,
                              struct panvk_draw_info *draw)
{
   const struct panvk_pipeline *pipeline =
      panvk_cmd_get_pipeline(cmdbuf, GRAPHICS);
   struct panvk_batch *batch = cmdbuf->state.batch;
   struct panfrost_ptr ptr =
      pan_pool_alloc_desc(&cmdbuf->desc_pool.base, COMPUTE_JOB);

   util_dynarray_append(&batch->jobs, void *, ptr.cpu);
   draw->jobs.vertex = ptr;

   memcpy(pan_section_ptr(ptr.cpu, COMPUTE_JOB, INVOCATION), &draw->invocation,
          pan_size(INVOCATION));

   pan_section_pack(ptr.cpu, COMPUTE_JOB, PARAMETERS, cfg) {
      cfg.job_task_split = 5;
   }

   pan_section_pack(ptr.cpu, COMPUTE_JOB, DRAW, cfg) {
      cfg.state = pipeline->rsds[MESA_SHADER_VERTEX];
      cfg.attributes = draw->stages[MESA_SHADER_VERTEX].attributes;
      cfg.attribute_buffers = draw->stages[MESA_SHADER_VERTEX].attribute_bufs;
      cfg.varyings = draw->stages[MESA_SHADER_VERTEX].varyings;
      cfg.varying_buffers = draw->varying_bufs;
      cfg.thread_storage = draw->tls;
      cfg.offset_start = draw->offset_start;
      cfg.instance_size =
         draw->instance_count > 1 ? draw->padded_vertex_count : 1;
      cfg.uniform_buffers = draw->ubos;
      cfg.push_uniforms = draw->push_uniforms;
      cfg.textures = draw->textures;
      cfg.samplers = draw->samplers;
   }
}

static void
panvk_emit_tiler_primitive(const struct panvk_pipeline *pipeline,
                           const struct panvk_draw_info *draw, void *prim)
{
   pan_pack(prim, PRIMITIVE, cfg) {
      cfg.draw_mode = pipeline->ia.topology;
      if (pipeline->ia.writes_point_size)
         cfg.point_size_array_format = MALI_POINT_SIZE_ARRAY_FORMAT_FP16;

      cfg.first_provoking_vertex = true;
      if (pipeline->ia.primitive_restart)
         cfg.primitive_restart = MALI_PRIMITIVE_RESTART_IMPLICIT;
      cfg.job_task_split = 6;

      if (draw->index_size) {
         cfg.index_count = draw->index_count;
         cfg.indices = draw->indices;
         cfg.base_vertex_offset = draw->vertex_offset - draw->offset_start;

         switch (draw->index_size) {
         case 32:
            cfg.index_type = MALI_INDEX_TYPE_UINT32;
            break;
         case 16:
            cfg.index_type = MALI_INDEX_TYPE_UINT16;
            break;
         case 8:
            cfg.index_type = MALI_INDEX_TYPE_UINT8;
            break;
         default:
            unreachable("Invalid index size");
         }
      } else {
         cfg.index_count = draw->vertex_count;
         cfg.index_type = MALI_INDEX_TYPE_NONE;
      }
   }
}

static void
panvk_emit_tiler_primitive_size(const struct panvk_pipeline *pipeline,
                                const struct panvk_draw_info *draw,
                                void *primsz)
{
   pan_pack(primsz, PRIMITIVE_SIZE, cfg) {
      if (pipeline->ia.writes_point_size) {
         cfg.size_array = draw->psiz;
      } else {
         cfg.constant = draw->line_width;
      }
   }
}

static void
panvk_emit_tiler_dcd(const struct panvk_pipeline *pipeline,
                     const struct panvk_draw_info *draw, void *dcd)
{
   pan_pack(dcd, DRAW, cfg) {
      cfg.front_face_ccw = pipeline->rast.front_ccw;
      cfg.cull_front_face = pipeline->rast.cull_front_face;
      cfg.cull_back_face = pipeline->rast.cull_back_face;
      cfg.position = draw->position;
      cfg.state = draw->fs_rsd;
      cfg.attributes = draw->stages[MESA_SHADER_FRAGMENT].attributes;
      cfg.attribute_buffers = draw->stages[MESA_SHADER_FRAGMENT].attribute_bufs;
      cfg.viewport = draw->viewport;
      cfg.varyings = draw->stages[MESA_SHADER_FRAGMENT].varyings;
      cfg.varying_buffers = cfg.varyings ? draw->varying_bufs : 0;
      cfg.thread_storage = draw->tls;

      /* For all primitives but lines DRAW.flat_shading_vertex must
       * be set to 0 and the provoking vertex is selected with the
       * PRIMITIVE.first_provoking_vertex field.
       */
      if (pipeline->ia.topology == MALI_DRAW_MODE_LINES ||
          pipeline->ia.topology == MALI_DRAW_MODE_LINE_STRIP ||
          pipeline->ia.topology == MALI_DRAW_MODE_LINE_LOOP) {
         cfg.flat_shading_vertex = true;
      }

      cfg.offset_start = draw->offset_start;
      cfg.instance_size =
         draw->instance_count > 1 ? draw->padded_vertex_count : 1;
      cfg.uniform_buffers = draw->ubos;
      cfg.push_uniforms = draw->push_uniforms;
      cfg.textures = draw->textures;
      cfg.samplers = draw->samplers;

      /* TODO: occlusion queries */
   }
}

static void
panvk_draw_prepare_tiler_job(struct panvk_cmd_buffer *cmdbuf,
                             struct panvk_draw_info *draw)
{
   const struct panvk_pipeline *pipeline =
      panvk_cmd_get_pipeline(cmdbuf, GRAPHICS);
   struct panvk_batch *batch = cmdbuf->state.batch;
   struct panfrost_ptr ptr =
      pan_pool_alloc_desc(&cmdbuf->desc_pool.base, TILER_JOB);

   /* If the vertex job doesn't write the position, we don't need a tiler job. */
   if (!draw->position)
      return;

   util_dynarray_append(&batch->jobs, void *, ptr.cpu);
   draw->jobs.tiler = ptr;

   memcpy(pan_section_ptr(ptr.cpu, TILER_JOB, INVOCATION), &draw->invocation,
          pan_size(INVOCATION));

   panvk_emit_tiler_primitive(pipeline, draw,
                              pan_section_ptr(ptr.cpu, TILER_JOB, PRIMITIVE));

   panvk_emit_tiler_primitive_size(
      pipeline, draw, pan_section_ptr(ptr.cpu, TILER_JOB, PRIMITIVE_SIZE));

   panvk_emit_tiler_dcd(pipeline, draw,
                        pan_section_ptr(ptr.cpu, TILER_JOB, DRAW));

   pan_section_pack(ptr.cpu, TILER_JOB, TILER, cfg) {
      cfg.address = draw->tiler_ctx->bifrost;
   }

   pan_section_pack(ptr.cpu, TILER_JOB, PADDING, padding)
      ;
}

static void
panvk_cmd_preload_fb_after_batch_split(struct panvk_cmd_buffer *cmdbuf)
{
   for (unsigned i = 0; i < cmdbuf->state.fb.info.rt_count; i++) {
      if (cmdbuf->state.fb.info.rts[i].view) {
         cmdbuf->state.fb.info.rts[i].clear = false;
         cmdbuf->state.fb.info.rts[i].preload = true;
      }
   }

   if (cmdbuf->state.fb.info.zs.view.zs) {
      cmdbuf->state.fb.info.zs.clear.z = false;
      cmdbuf->state.fb.info.zs.preload.z = true;
   }

   if (cmdbuf->state.fb.info.zs.view.s ||
       (cmdbuf->state.fb.info.zs.view.zs &&
        util_format_is_depth_and_stencil(
           cmdbuf->state.fb.info.zs.view.zs->format))) {
      cmdbuf->state.fb.info.zs.clear.s = false;
      cmdbuf->state.fb.info.zs.preload.s = true;
   }
}

struct panvk_batch *
panvk_per_arch(cmd_open_batch)(struct panvk_cmd_buffer *cmdbuf)
{
   assert(!cmdbuf->state.batch);
   cmdbuf->state.batch =
      vk_zalloc(&cmdbuf->vk.pool->alloc, sizeof(*cmdbuf->state.batch), 8,
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   util_dynarray_init(&cmdbuf->state.batch->jobs, NULL);
   util_dynarray_init(&cmdbuf->state.batch->event_ops, NULL);
   assert(cmdbuf->state.batch);
   return cmdbuf->state.batch;
}

static void
panvk_cmd_draw(struct panvk_cmd_buffer *cmdbuf, struct panvk_draw_info *draw)
{
   struct panvk_batch *batch = cmdbuf->state.batch;
   struct panvk_cmd_bind_point_state *bind_point_state =
      panvk_cmd_get_bind_point_state(cmdbuf, GRAPHICS);
   const struct panvk_pipeline *pipeline =
      panvk_cmd_get_pipeline(cmdbuf, GRAPHICS);

   /* There are only 16 bits in the descriptor for the job ID, make sure all
    * the 3 (2 in Bifrost) jobs in this draw are in the same batch.
    */
   if (batch->jc.job_index >= (UINT16_MAX - 3)) {
      panvk_per_arch(cmd_close_batch)(cmdbuf);
      panvk_cmd_preload_fb_after_batch_split(cmdbuf);
      batch = panvk_per_arch(cmd_open_batch)(cmdbuf);
   }

   if (pipeline->rast.enable)
      panvk_per_arch(cmd_alloc_fb_desc)(cmdbuf);

   panvk_per_arch(cmd_alloc_tls_desc)(cmdbuf, true);

   panvk_cmd_prepare_draw_sysvals(cmdbuf, bind_point_state, draw);
   panvk_cmd_prepare_push_sets(cmdbuf, bind_point_state);
   panvk_cmd_prepare_push_constants(cmdbuf, bind_point_state);
   panvk_cmd_prepare_ubos(cmdbuf, bind_point_state);
   panvk_cmd_prepare_textures(cmdbuf, bind_point_state);
   panvk_cmd_prepare_samplers(cmdbuf, bind_point_state);

   /* TODO: indexed draws */
   struct panvk_descriptor_state *desc_state =
      panvk_cmd_get_desc_state(cmdbuf, GRAPHICS);

   draw->tls = batch->tls.gpu;
   draw->fb = batch->fb.desc.gpu;
   draw->ubos = desc_state->ubos;
   draw->push_uniforms = desc_state->push_constants;
   draw->textures = desc_state->textures;
   draw->samplers = desc_state->samplers;

   panfrost_pack_work_groups_compute(&draw->invocation, 1, draw->vertex_range,
                                     draw->instance_count, 1, 1, 1, true,
                                     false);

   panvk_draw_prepare_fs_rsd(cmdbuf, draw);
   panvk_draw_prepare_varyings(cmdbuf, draw);
   panvk_draw_prepare_attributes(cmdbuf, draw);
   panvk_draw_prepare_viewport(cmdbuf, draw);
   panvk_draw_prepare_tiler_context(cmdbuf, draw);
   panvk_draw_prepare_vertex_job(cmdbuf, draw);
   panvk_draw_prepare_tiler_job(cmdbuf, draw);
   batch->tlsinfo.tls.size = MAX2(pipeline->tls_size, batch->tlsinfo.tls.size);
   assert(!pipeline->wls_size);

   unsigned vjob_id =
      pan_jc_add_job(&cmdbuf->desc_pool.base, &batch->jc, MALI_JOB_TYPE_VERTEX,
                     false, false, 0, 0, &draw->jobs.vertex, false);

   if (pipeline->rast.enable && draw->position) {
      pan_jc_add_job(&cmdbuf->desc_pool.base, &batch->jc, MALI_JOB_TYPE_TILER,
                     false, false, vjob_id, 0, &draw->jobs.tiler, false);
   }

   /* Clear the dirty flags all at once */
   desc_state->dirty = cmdbuf->state.dirty = 0;
   panvk_cmd_unprepare_push_sets(cmdbuf, bind_point_state);
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(CmdDraw)(VkCommandBuffer commandBuffer, uint32_t vertexCount,
                        uint32_t instanceCount, uint32_t firstVertex,
                        uint32_t firstInstance)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);

   if (instanceCount == 0 || vertexCount == 0)
      return;

   struct panvk_draw_info draw = {
      .first_vertex = firstVertex,
      .vertex_count = vertexCount,
      .vertex_range = vertexCount,
      .first_instance = firstInstance,
      .instance_count = instanceCount,
      .padded_vertex_count = instanceCount > 1
                                ? panfrost_padded_vertex_count(vertexCount)
                                : vertexCount,
      .offset_start = firstVertex,
   };

   panvk_cmd_draw(cmdbuf, &draw);
}

static void
panvk_index_minmax_search(struct panvk_cmd_buffer *cmdbuf, uint32_t start,
                          uint32_t count, bool restart, uint32_t *min,
                          uint32_t *max)
{
   struct panvk_device *dev = to_panvk_device(cmdbuf->vk.base.device);
   struct panvk_instance *instance =
      to_panvk_instance(dev->vk.physical->instance);
   void *ptr = cmdbuf->state.ib.buffer->host_ptr + cmdbuf->state.ib.offset;

   assert(cmdbuf->state.ib.buffer);
   assert(cmdbuf->state.ib.buffer->bo);
   assert(cmdbuf->state.ib.buffer->host_ptr);

   if (!(instance->debug_flags & PANVK_DEBUG_NO_KNOWN_WARN)) {
      fprintf(
         stderr,
         "WARNING: Crawling index buffers from the CPU isn't valid in Vulkan\n");
   }

   *max = 0;

   /* TODO: Use panfrost_minmax_cache */
   /* TODO: Read full cacheline of data to mitigate the uncached
    * mapping slowness.
    */
   switch (cmdbuf->state.ib.index_size) {
#define MINMAX_SEARCH_CASE(sz)                                                 \
   case sz: {                                                                  \
      uint##sz##_t *indices = ptr;                                             \
      *min = UINT##sz##_MAX;                                                   \
      for (uint32_t i = 0; i < count; i++) {                                   \
         if (restart && indices[i + start] == UINT##sz##_MAX)                  \
            continue;                                                          \
         *min = MIN2(indices[i + start], *min);                                \
         *max = MAX2(indices[i + start], *max);                                \
      }                                                                        \
      break;                                                                   \
   }
      MINMAX_SEARCH_CASE(32)
      MINMAX_SEARCH_CASE(16)
      MINMAX_SEARCH_CASE(8)
#undef MINMAX_SEARCH_CASE
   default:
      unreachable("Invalid index size");
   }
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(CmdDrawIndexed)(VkCommandBuffer commandBuffer,
                               uint32_t indexCount, uint32_t instanceCount,
                               uint32_t firstIndex, int32_t vertexOffset,
                               uint32_t firstInstance)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   uint32_t min_vertex, max_vertex;

   if (instanceCount == 0 || indexCount == 0)
      return;

   const struct panvk_pipeline *pipeline =
      panvk_cmd_get_pipeline(cmdbuf, GRAPHICS);
   bool primitive_restart = pipeline->ia.primitive_restart;

   panvk_index_minmax_search(cmdbuf, firstIndex, indexCount, primitive_restart,
                             &min_vertex, &max_vertex);

   unsigned vertex_range = max_vertex - min_vertex + 1;
   struct panvk_draw_info draw = {
      .index_size = cmdbuf->state.ib.index_size,
      .first_index = firstIndex,
      .index_count = indexCount,
      .vertex_offset = vertexOffset,
      .first_instance = firstInstance,
      .instance_count = instanceCount,
      .vertex_range = vertex_range,
      .vertex_count = indexCount + abs(vertexOffset),
      .padded_vertex_count = instanceCount > 1
                                ? panfrost_padded_vertex_count(vertex_range)
                                : vertex_range,
      .offset_start = min_vertex + vertexOffset,
      .indices = panvk_buffer_gpu_ptr(cmdbuf->state.ib.buffer,
                                      cmdbuf->state.ib.offset) +
                 (firstIndex * (cmdbuf->state.ib.index_size / 8)),
   };

   panvk_cmd_draw(cmdbuf, &draw);
}

VKAPI_ATTR VkResult VKAPI_CALL
panvk_per_arch(EndCommandBuffer)(VkCommandBuffer commandBuffer)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);

   panvk_per_arch(cmd_close_batch)(cmdbuf);

   return vk_command_buffer_end(&cmdbuf->vk);
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(CmdPipelineBarrier2)(VkCommandBuffer commandBuffer,
                                    const VkDependencyInfo *pDependencyInfo)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);

   /* Caches are flushed/invalidated at batch boundaries for now, nothing to do
    * for memory barriers assuming we implement barriers with the creation of a
    * new batch.
    * FIXME: We can probably do better with a CacheFlush job that has the
    * barrier flag set to true.
    */
   if (cmdbuf->state.batch) {
      panvk_per_arch(cmd_close_batch)(cmdbuf);
      panvk_cmd_preload_fb_after_batch_split(cmdbuf);
      panvk_per_arch(cmd_open_batch)(cmdbuf);
   }
}

static void
panvk_add_set_event_operation(struct panvk_cmd_buffer *cmdbuf,
                              struct panvk_event *event,
                              enum panvk_cmd_event_op_type type)
{
   struct panvk_cmd_event_op op = {
      .type = type,
      .event = event,
   };

   if (cmdbuf->state.batch == NULL) {
      /* No open batch, let's create a new one so this operation happens in
       * the right order.
       */
      panvk_per_arch(cmd_open_batch)(cmdbuf);
      util_dynarray_append(&cmdbuf->state.batch->event_ops,
                           struct panvk_cmd_event_op, op);
      panvk_per_arch(cmd_close_batch)(cmdbuf);
   } else {
      /* Let's close the current batch so the operation executes before any
       * future commands.
       */
      util_dynarray_append(&cmdbuf->state.batch->event_ops,
                           struct panvk_cmd_event_op, op);
      panvk_per_arch(cmd_close_batch)(cmdbuf);
      panvk_cmd_preload_fb_after_batch_split(cmdbuf);
      panvk_per_arch(cmd_open_batch)(cmdbuf);
   }
}

static void
panvk_add_wait_event_operation(struct panvk_cmd_buffer *cmdbuf,
                               struct panvk_event *event)
{
   struct panvk_cmd_event_op op = {
      .type = PANVK_EVENT_OP_WAIT,
      .event = event,
   };

   if (cmdbuf->state.batch == NULL) {
      /* No open batch, let's create a new one and have it wait for this event. */
      panvk_per_arch(cmd_open_batch)(cmdbuf);
      util_dynarray_append(&cmdbuf->state.batch->event_ops,
                           struct panvk_cmd_event_op, op);
   } else {
      /* Let's close the current batch so any future commands wait on the
       * event signal operation.
       */
      if (cmdbuf->state.batch->fragment_job ||
          cmdbuf->state.batch->jc.first_job) {
         panvk_per_arch(cmd_close_batch)(cmdbuf);
         panvk_cmd_preload_fb_after_batch_split(cmdbuf);
         panvk_per_arch(cmd_open_batch)(cmdbuf);
      }
      util_dynarray_append(&cmdbuf->state.batch->event_ops,
                           struct panvk_cmd_event_op, op);
   }
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(CmdSetEvent2)(VkCommandBuffer commandBuffer, VkEvent _event,
                             const VkDependencyInfo *pDependencyInfo)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   VK_FROM_HANDLE(panvk_event, event, _event);

   /* vkCmdSetEvent cannot be called inside a render pass */
   assert(cmdbuf->vk.render_pass == NULL);

   panvk_add_set_event_operation(cmdbuf, event, PANVK_EVENT_OP_SET);
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(CmdResetEvent2)(VkCommandBuffer commandBuffer, VkEvent _event,
                               VkPipelineStageFlags2 stageMask)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   VK_FROM_HANDLE(panvk_event, event, _event);

   /* vkCmdResetEvent cannot be called inside a render pass */
   assert(cmdbuf->vk.render_pass == NULL);

   panvk_add_set_event_operation(cmdbuf, event, PANVK_EVENT_OP_RESET);
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(CmdWaitEvents2)(VkCommandBuffer commandBuffer,
                               uint32_t eventCount, const VkEvent *pEvents,
                               const VkDependencyInfo *pDependencyInfos)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);

   assert(eventCount > 0);

   for (uint32_t i = 0; i < eventCount; i++) {
      VK_FROM_HANDLE(panvk_event, event, pEvents[i]);
      panvk_add_wait_event_operation(cmdbuf, event);
   }
}

static void
panvk_reset_cmdbuf(struct vk_command_buffer *vk_cmdbuf,
                   VkCommandBufferResetFlags flags)
{
   struct panvk_cmd_buffer *cmdbuf =
      container_of(vk_cmdbuf, struct panvk_cmd_buffer, vk);

   vk_command_buffer_reset(&cmdbuf->vk);

   list_for_each_entry_safe(struct panvk_batch, batch, &cmdbuf->batches, node) {
      list_del(&batch->node);
      util_dynarray_fini(&batch->jobs);
      util_dynarray_fini(&batch->event_ops);

      vk_free(&cmdbuf->vk.pool->alloc, batch);
   }

   panvk_pool_reset(&cmdbuf->desc_pool);
   panvk_pool_reset(&cmdbuf->tls_pool);
   panvk_pool_reset(&cmdbuf->varying_pool);

   for (unsigned i = 0; i < MAX_BIND_POINTS; i++)
      memset(&cmdbuf->bind_points[i].desc_state.sets, 0,
             sizeof(cmdbuf->bind_points[0].desc_state.sets));
}

static void
panvk_destroy_cmdbuf(struct vk_command_buffer *vk_cmdbuf)
{
   struct panvk_cmd_buffer *cmdbuf =
      container_of(vk_cmdbuf, struct panvk_cmd_buffer, vk);
   struct panvk_device *dev = to_panvk_device(cmdbuf->vk.base.device);

   for (unsigned i = 0; i < MAX_BIND_POINTS; i++) {
      for (unsigned j = 0; j < MAX_SETS; j++) {
         if (cmdbuf->bind_points[i].desc_state.push_sets[j])
            vk_free(&cmdbuf->vk.pool->alloc,
                    cmdbuf->bind_points[i].desc_state.push_sets[j]);
      }
   }

   list_for_each_entry_safe(struct panvk_batch, batch, &cmdbuf->batches, node) {
      list_del(&batch->node);
      util_dynarray_fini(&batch->jobs);
      util_dynarray_fini(&batch->event_ops);

      vk_free(&cmdbuf->vk.pool->alloc, batch);
   }

   panvk_pool_cleanup(&cmdbuf->desc_pool);
   panvk_pool_cleanup(&cmdbuf->tls_pool);
   panvk_pool_cleanup(&cmdbuf->varying_pool);
   vk_command_buffer_finish(&cmdbuf->vk);
   vk_free(&dev->vk.alloc, cmdbuf);
}

static VkResult
panvk_create_cmdbuf(struct vk_command_pool *vk_pool, VkCommandBufferLevel level,
                    struct vk_command_buffer **cmdbuf_out)
{
   struct panvk_device *device =
      container_of(vk_pool->base.device, struct panvk_device, vk);
   struct panvk_cmd_pool *pool =
      container_of(vk_pool, struct panvk_cmd_pool, vk);
   struct panvk_cmd_buffer *cmdbuf;

   cmdbuf = vk_zalloc(&device->vk.alloc, sizeof(*cmdbuf), 8,
                      VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!cmdbuf)
      return vk_error(device, VK_ERROR_OUT_OF_HOST_MEMORY);

   VkResult result = vk_command_buffer_init(
      &pool->vk, &cmdbuf->vk, &panvk_per_arch(cmd_buffer_ops), level);
   if (result != VK_SUCCESS) {
      vk_free(&device->vk.alloc, cmdbuf);
      return result;
   }

   panvk_pool_init(&cmdbuf->desc_pool, device, &pool->desc_bo_pool, 0,
                   64 * 1024, "Command buffer descriptor pool", true);
   panvk_pool_init(
      &cmdbuf->tls_pool, device, &pool->tls_bo_pool,
      panvk_debug_adjust_bo_flags(device, PAN_KMOD_BO_FLAG_NO_MMAP), 64 * 1024,
      "TLS pool", false);
   panvk_pool_init(
      &cmdbuf->varying_pool, device, &pool->varying_bo_pool,
      panvk_debug_adjust_bo_flags(device, PAN_KMOD_BO_FLAG_NO_MMAP), 64 * 1024,
      "Varyings pool", false);
   list_inithead(&cmdbuf->batches);
   *cmdbuf_out = &cmdbuf->vk;
   return VK_SUCCESS;
}

const struct vk_command_buffer_ops panvk_per_arch(cmd_buffer_ops) = {
   .create = panvk_create_cmdbuf,
   .reset = panvk_reset_cmdbuf,
   .destroy = panvk_destroy_cmdbuf,
};

VKAPI_ATTR VkResult VKAPI_CALL
panvk_per_arch(BeginCommandBuffer)(VkCommandBuffer commandBuffer,
                                   const VkCommandBufferBeginInfo *pBeginInfo)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);

   vk_command_buffer_begin(&cmdbuf->vk, pBeginInfo);

   memset(&cmdbuf->state, 0, sizeof(cmdbuf->state));

   return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(CmdDispatch)(VkCommandBuffer commandBuffer, uint32_t x,
                            uint32_t y, uint32_t z)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   struct panvk_device *dev = to_panvk_device(cmdbuf->vk.base.device);
   struct panvk_physical_device *phys_dev =
      to_panvk_physical_device(dev->vk.physical);
   struct panvk_dispatch_info dispatch = {
      .wg_count = {x, y, z},
   };

   panvk_per_arch(cmd_close_batch)(cmdbuf);
   struct panvk_batch *batch = panvk_per_arch(cmd_open_batch)(cmdbuf);

   struct panvk_cmd_bind_point_state *bind_point_state =
      panvk_cmd_get_bind_point_state(cmdbuf, COMPUTE);
   struct panvk_descriptor_state *desc_state = &bind_point_state->desc_state;
   const struct panvk_pipeline *pipeline = bind_point_state->pipeline;
   struct panfrost_ptr job =
      pan_pool_alloc_desc(&cmdbuf->desc_pool.base, COMPUTE_JOB);

   struct panvk_sysvals *sysvals = &desc_state->sysvals;
   sysvals->num_work_groups.u32[0] = x;
   sysvals->num_work_groups.u32[1] = y;
   sysvals->num_work_groups.u32[2] = z;
   sysvals->local_group_size.u32[0] = pipeline->cs.local_size.x;
   sysvals->local_group_size.u32[1] = pipeline->cs.local_size.y;
   sysvals->local_group_size.u32[2] = pipeline->cs.local_size.z;
   desc_state->sysvals_ptr = 0;

   panvk_per_arch(cmd_alloc_tls_desc)(cmdbuf, false);
   dispatch.tsd = batch->tls.gpu;

   panvk_cmd_prepare_push_sets(cmdbuf, bind_point_state);
   panvk_prepare_non_vs_attribs(cmdbuf, bind_point_state);
   dispatch.attributes = desc_state->non_vs_attribs;
   dispatch.attribute_bufs = desc_state->non_vs_attrib_bufs;

   panvk_cmd_prepare_ubos(cmdbuf, bind_point_state);
   dispatch.ubos = desc_state->ubos;

   panvk_cmd_prepare_push_constants(cmdbuf, bind_point_state);
   dispatch.push_uniforms = desc_state->push_constants;

   panvk_cmd_prepare_textures(cmdbuf, bind_point_state);
   dispatch.textures = desc_state->textures;

   panvk_cmd_prepare_samplers(cmdbuf, bind_point_state);
   dispatch.samplers = desc_state->samplers;

   panfrost_pack_work_groups_compute(
      pan_section_ptr(job.cpu, COMPUTE_JOB, INVOCATION), dispatch.wg_count.x,
      dispatch.wg_count.y, dispatch.wg_count.z, pipeline->cs.local_size.x,
      pipeline->cs.local_size.y, pipeline->cs.local_size.z, false, false);

   pan_section_pack(job.cpu, COMPUTE_JOB, PARAMETERS, cfg) {
      cfg.job_task_split = util_logbase2_ceil(pipeline->cs.local_size.x + 1) +
                           util_logbase2_ceil(pipeline->cs.local_size.y + 1) +
                           util_logbase2_ceil(pipeline->cs.local_size.z + 1);
   }

   pan_section_pack(job.cpu, COMPUTE_JOB, DRAW, cfg) {
      cfg.state = pipeline->rsds[MESA_SHADER_COMPUTE];
      cfg.attributes = dispatch.attributes;
      cfg.attribute_buffers = dispatch.attribute_bufs;
      cfg.thread_storage = dispatch.tsd;
      cfg.uniform_buffers = dispatch.ubos;
      cfg.push_uniforms = dispatch.push_uniforms;
      cfg.textures = dispatch.textures;
      cfg.samplers = dispatch.samplers;
   }

   pan_jc_add_job(&cmdbuf->desc_pool.base, &batch->jc, MALI_JOB_TYPE_COMPUTE,
                  false, false, 0, 0, &job, false);

   batch->tlsinfo.tls.size = pipeline->tls_size;
   batch->tlsinfo.wls.size = pipeline->wls_size;
   if (batch->tlsinfo.wls.size) {
      unsigned core_id_range;

      panfrost_query_core_count(&phys_dev->kmod.props, &core_id_range);
      batch->tlsinfo.wls.instances = pan_wls_instances(&dispatch.wg_count);
      batch->wls_total_size = pan_wls_adjust_size(batch->tlsinfo.wls.size) *
                              batch->tlsinfo.wls.instances *
                              core_id_range;
   }

   panvk_per_arch(cmd_close_batch)(cmdbuf);
   desc_state->dirty = 0;
   panvk_cmd_unprepare_push_sets(cmdbuf, bind_point_state);
}

static void
panvk_cmd_begin_rendering_init_fbinfo(struct panvk_cmd_buffer *cmdbuf,
                                      const VkRenderingInfo *pRenderingInfo)
{
   struct panvk_device *dev = to_panvk_device(cmdbuf->vk.base.device);
   struct panvk_physical_device *phys_dev =
      to_panvk_physical_device(dev->vk.physical);
   struct pan_fb_info *fbinfo = &cmdbuf->state.fb.info;
   uint32_t att_width = 0, att_height = 0;
   bool has_attachments = false;

   cmdbuf->state.fb.bo_count = 0;
   memset(cmdbuf->state.fb.bos, 0, sizeof(cmdbuf->state.fb.bos));
   memset(cmdbuf->state.fb.crc_valid, 0, sizeof(cmdbuf->state.fb.crc_valid));

   *fbinfo = (struct pan_fb_info){
      .tile_buf_budget = panfrost_query_optimal_tib_size(phys_dev->model),
      .nr_samples = 1,
      .rt_count = pRenderingInfo->colorAttachmentCount,
   };

   assert(pRenderingInfo->colorAttachmentCount <= ARRAY_SIZE(fbinfo->rts));

   for (uint32_t i = 0; i < pRenderingInfo->colorAttachmentCount; i++) {
      const VkRenderingAttachmentInfo *att =
         &pRenderingInfo->pColorAttachments[i];
      VK_FROM_HANDLE(panvk_image_view, iview, att->imageView);

      if (!iview)
         continue;

      struct panvk_image *img =
         container_of(iview->vk.image, struct panvk_image, vk);
      const VkExtent3D iview_size =
         vk_image_mip_level_extent(&img->vk, iview->vk.base_mip_level);

      has_attachments = true;
      att_width = MAX2(iview_size.width, att_width);
      att_height = MAX2(iview_size.height, att_height);

      assert(att->resolveMode == VK_RESOLVE_MODE_NONE);

      cmdbuf->state.fb.bos[cmdbuf->state.fb.bo_count++] = img->bo;
      fbinfo->rts[i].view = &iview->pview;
      fbinfo->rts[i].crc_valid = &cmdbuf->state.fb.crc_valid[i];
      fbinfo->nr_samples =
         MAX2(fbinfo->nr_samples, pan_image_view_get_nr_samples(&iview->pview));

      if (att->loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) {
         enum pipe_format fmt = vk_format_to_pipe_format(iview->vk.format);
         union pipe_color_union *col =
            (union pipe_color_union *)&att->clearValue.color;

         fbinfo->rts[i].clear = true;
         pan_pack_color(phys_dev->formats.blendable, fbinfo->rts[i].clear_value,
                        col, fmt, false);
      } else if (att->loadOp == VK_ATTACHMENT_LOAD_OP_LOAD) {
         fbinfo->rts[i].preload = true;
      }
   }

   if (pRenderingInfo->pDepthAttachment &&
       pRenderingInfo->pDepthAttachment->imageView != VK_NULL_HANDLE) {
      const VkRenderingAttachmentInfo *att = pRenderingInfo->pDepthAttachment;
      VK_FROM_HANDLE(panvk_image_view, iview, att->imageView);
      struct panvk_image *img =
         container_of(iview->vk.image, struct panvk_image, vk);
      const VkExtent3D iview_size =
         vk_image_mip_level_extent(&img->vk, iview->vk.base_mip_level);

      has_attachments = true;
      att_width = MAX2(iview_size.width, att_width);
      att_height = MAX2(iview_size.height, att_height);

      assert(att->resolveMode == VK_RESOLVE_MODE_NONE);

      cmdbuf->state.fb.bos[cmdbuf->state.fb.bo_count++] = img->bo;
      fbinfo->zs.view.zs = &iview->pview;

      if (att->loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) {
         fbinfo->zs.clear.z = true;
         fbinfo->zs.clear_value.depth = att->clearValue.depthStencil.depth;
      } else if (att->loadOp == VK_ATTACHMENT_LOAD_OP_LOAD) {
         fbinfo->zs.preload.z = true;
      }
   }

   if (pRenderingInfo->pStencilAttachment &&
       pRenderingInfo->pStencilAttachment->imageView != VK_NULL_HANDLE) {
      const VkRenderingAttachmentInfo *att = pRenderingInfo->pStencilAttachment;
      VK_FROM_HANDLE(panvk_image_view, iview, att->imageView);
      struct panvk_image *img =
         container_of(iview->vk.image, struct panvk_image, vk);
      const VkExtent3D iview_size =
         vk_image_mip_level_extent(&img->vk, iview->vk.base_mip_level);

      has_attachments = true;
      att_width = MAX2(iview_size.width, att_width);
      att_height = MAX2(iview_size.height, att_height);

      assert(att->resolveMode == VK_RESOLVE_MODE_NONE);

      cmdbuf->state.fb.bos[cmdbuf->state.fb.bo_count++] = img->bo;
      fbinfo->zs.view.s =
         iview && &iview->pview != fbinfo->zs.view.zs ? &iview->pview : NULL;

      if (iview && att->loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) {
         fbinfo->zs.clear.s = true;
         fbinfo->zs.clear_value.stencil = att->clearValue.depthStencil.stencil;
      } else if (att->loadOp == VK_ATTACHMENT_LOAD_OP_LOAD) {
         fbinfo->zs.preload.s = true;
      }
   }

   fbinfo->width = pRenderingInfo->renderArea.offset.x +
                   pRenderingInfo->renderArea.extent.width;
   fbinfo->height = pRenderingInfo->renderArea.offset.y +
                    pRenderingInfo->renderArea.extent.height;

   if (has_attachments) {
      /* We need the rendering area to be aligned on a 32x32 section for tile
       * buffer preloading to work correctly.
       */
      fbinfo->width = MIN2(att_width, ALIGN_POT(fbinfo->width, 32));
      fbinfo->height = MIN2(att_height, ALIGN_POT(fbinfo->height, 32));
   }

   assert(fbinfo->width && fbinfo->height);

   fbinfo->extent.maxx = fbinfo->width - 1;
   fbinfo->extent.maxy = fbinfo->height - 1;
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(CmdBeginRendering)(VkCommandBuffer commandBuffer,
                                  const VkRenderingInfo *pRenderingInfo)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);

   panvk_cmd_begin_rendering_init_fbinfo(cmdbuf, pRenderingInfo);
   panvk_per_arch(cmd_open_batch)(cmdbuf);
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(CmdEndRendering)(VkCommandBuffer commandBuffer)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);

   panvk_per_arch(cmd_close_batch)(cmdbuf);
   cmdbuf->state.batch = NULL;
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(CmdBindVertexBuffers)(VkCommandBuffer commandBuffer,
                                     uint32_t firstBinding,
                                     uint32_t bindingCount,
                                     const VkBuffer *pBuffers,
                                     const VkDeviceSize *pOffsets)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   struct panvk_descriptor_state *desc_state =
      panvk_cmd_get_desc_state(cmdbuf, GRAPHICS);

   assert(firstBinding + bindingCount <= MAX_VBS);

   for (uint32_t i = 0; i < bindingCount; i++) {
      VK_FROM_HANDLE(panvk_buffer, buffer, pBuffers[i]);

      cmdbuf->state.vb.bufs[firstBinding + i].address =
         panvk_buffer_gpu_ptr(buffer, pOffsets[i]);
      cmdbuf->state.vb.bufs[firstBinding + i].size =
         panvk_buffer_range(buffer, pOffsets[i], VK_WHOLE_SIZE);
   }

   cmdbuf->state.vb.count =
      MAX2(cmdbuf->state.vb.count, firstBinding + bindingCount);
   desc_state->vs_attrib_bufs = desc_state->vs_attribs = 0;
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(CmdBindIndexBuffer)(VkCommandBuffer commandBuffer,
                                   VkBuffer buffer, VkDeviceSize offset,
                                   VkIndexType indexType)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   VK_FROM_HANDLE(panvk_buffer, buf, buffer);

   cmdbuf->state.ib.buffer = buf;
   cmdbuf->state.ib.offset = offset;
   switch (indexType) {
   case VK_INDEX_TYPE_UINT16:
      cmdbuf->state.ib.index_size = 16;
      break;
   case VK_INDEX_TYPE_UINT32:
      cmdbuf->state.ib.index_size = 32;
      break;
   case VK_INDEX_TYPE_NONE_KHR:
      cmdbuf->state.ib.index_size = 0;
      break;
   case VK_INDEX_TYPE_UINT8_EXT:
      cmdbuf->state.ib.index_size = 8;
      break;
   default:
      unreachable("Invalid index type\n");
   }
}

static void
panvk_set_dyn_ssbo_pointers(struct panvk_descriptor_state *desc_state,
                            unsigned dyn_ssbo_offset,
                            struct panvk_descriptor_set *set)
{
   struct panvk_sysvals *sysvals = &desc_state->sysvals;

   for (unsigned i = 0; i < set->layout->num_dyn_ssbos; i++) {
      const struct panvk_buffer_desc *ssbo =
         &desc_state->dyn.ssbos[dyn_ssbo_offset + i];

      sysvals->dyn_ssbos[dyn_ssbo_offset + i] = (struct panvk_ssbo_addr){
         .base_addr = panvk_buffer_gpu_ptr(ssbo->buffer, ssbo->offset),
         .size = panvk_buffer_range(ssbo->buffer, ssbo->offset, ssbo->size),
      };
   }

   desc_state->sysvals_ptr = 0;
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(CmdBindDescriptorSets)(
   VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint,
   VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount,
   const VkDescriptorSet *pDescriptorSets, uint32_t dynamicOffsetCount,
   const uint32_t *pDynamicOffsets)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   VK_FROM_HANDLE(panvk_pipeline_layout, playout, layout);

   struct panvk_descriptor_state *descriptors_state =
      &cmdbuf->bind_points[pipelineBindPoint].desc_state;

   unsigned dynoffset_idx = 0;
   for (unsigned i = 0; i < descriptorSetCount; ++i) {
      unsigned idx = i + firstSet;
      VK_FROM_HANDLE(panvk_descriptor_set, set, pDescriptorSets[i]);

      descriptors_state->sets[idx] = set;

      if (set->layout->num_dyn_ssbos || set->layout->num_dyn_ubos) {
         unsigned dyn_ubo_offset = playout->sets[idx].dyn_ubo_offset;
         unsigned dyn_ssbo_offset = playout->sets[idx].dyn_ssbo_offset;

         for (unsigned b = 0; b < set->layout->binding_count; b++) {
            for (unsigned e = 0; e < set->layout->bindings[b].array_size; e++) {
               struct panvk_buffer_desc *bdesc = NULL;

               if (set->layout->bindings[b].type ==
                   VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) {
                  bdesc = &descriptors_state->dyn.ubos[dyn_ubo_offset++];
                  *bdesc =
                     set->dyn_ubos[set->layout->bindings[b].dyn_ubo_idx + e];
               } else if (set->layout->bindings[b].type ==
                          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC) {
                  bdesc = &descriptors_state->dyn.ssbos[dyn_ssbo_offset++];
                  *bdesc =
                     set->dyn_ssbos[set->layout->bindings[b].dyn_ssbo_idx + e];
               }

               if (bdesc) {
                  bdesc->offset += pDynamicOffsets[dynoffset_idx++];
               }
            }
         }
      }

      if (set->layout->num_dyn_ssbos) {
         panvk_set_dyn_ssbo_pointers(descriptors_state,
                                     playout->sets[idx].dyn_ssbo_offset, set);
      }

      if (set->layout->num_dyn_ssbos)
         descriptors_state->dirty |= PANVK_DYNAMIC_SSBO;

      if (set->layout->num_ubos || set->layout->num_dyn_ubos ||
          set->layout->num_dyn_ssbos || set->layout->desc_ubo_size)
         descriptors_state->ubos = 0;

      if (set->layout->num_textures)
         descriptors_state->textures = 0;

      if (set->layout->num_samplers)
         descriptors_state->samplers = 0;

      if (set->layout->num_imgs) {
         descriptors_state->vs_attrib_bufs =
            descriptors_state->non_vs_attrib_bufs = 0;
         descriptors_state->vs_attribs = descriptors_state->non_vs_attribs = 0;
      }
   }

   assert(dynoffset_idx == dynamicOffsetCount);
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(CmdPushConstants)(VkCommandBuffer commandBuffer,
                                 VkPipelineLayout layout,
                                 VkShaderStageFlags stageFlags, uint32_t offset,
                                 uint32_t size, const void *pValues)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);

   memcpy(cmdbuf->push_constants + offset, pValues, size);

   if (stageFlags & VK_SHADER_STAGE_ALL_GRAPHICS) {
      struct panvk_descriptor_state *desc_state =
         panvk_cmd_get_desc_state(cmdbuf, GRAPHICS);

      desc_state->push_constants = 0;
   }

   if (stageFlags & VK_SHADER_STAGE_COMPUTE_BIT) {
      struct panvk_descriptor_state *desc_state =
         panvk_cmd_get_desc_state(cmdbuf, COMPUTE);

      desc_state->push_constants = 0;
   }
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(CmdBindPipeline)(VkCommandBuffer commandBuffer,
                                VkPipelineBindPoint pipelineBindPoint,
                                VkPipeline _pipeline)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   VK_FROM_HANDLE(panvk_pipeline, pipeline, _pipeline);

   cmdbuf->bind_points[pipelineBindPoint].pipeline = pipeline;
   cmdbuf->state.fs_rsd = 0;

   if (pipelineBindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
      cmdbuf->state.varyings = pipeline->varyings;

      if (!(pipeline->dynamic_state_mask &
            BITFIELD_BIT(VK_DYNAMIC_STATE_VIEWPORT))) {
         cmdbuf->state.viewport = pipeline->viewport;
         cmdbuf->state.dirty |= PANVK_DYNAMIC_VIEWPORT;
      }
      if (!(pipeline->dynamic_state_mask &
            BITFIELD_BIT(VK_DYNAMIC_STATE_SCISSOR))) {
         cmdbuf->state.scissor = pipeline->scissor;
         cmdbuf->state.dirty |= PANVK_DYNAMIC_SCISSOR;
      }
   }

   /* Sysvals are passed through UBOs, we need dirty the UBO array if the
    * pipeline contain shaders using sysvals.
    */
   cmdbuf->bind_points[pipelineBindPoint].desc_state.ubos = 0;
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(CmdSetViewport)(VkCommandBuffer commandBuffer,
                               uint32_t firstViewport, uint32_t viewportCount,
                               const VkViewport *pViewports)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   assert(viewportCount == 1);
   assert(!firstViewport);

   cmdbuf->state.viewport = pViewports[0];
   cmdbuf->state.vpd = 0;
   cmdbuf->state.dirty |= PANVK_DYNAMIC_VIEWPORT;
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(CmdSetScissor)(VkCommandBuffer commandBuffer,
                              uint32_t firstScissor, uint32_t scissorCount,
                              const VkRect2D *pScissors)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   assert(scissorCount == 1);
   assert(!firstScissor);

   cmdbuf->state.scissor = pScissors[0];
   cmdbuf->state.vpd = 0;
   cmdbuf->state.dirty |= PANVK_DYNAMIC_SCISSOR;
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(CmdSetLineWidth)(VkCommandBuffer commandBuffer, float lineWidth)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);

   cmdbuf->state.rast.line_width = lineWidth;
   cmdbuf->state.dirty |= PANVK_DYNAMIC_LINE_WIDTH;
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(CmdSetDepthBias)(VkCommandBuffer commandBuffer,
                                float depthBiasConstantFactor,
                                float depthBiasClamp,
                                float depthBiasSlopeFactor)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);

   cmdbuf->state.rast.depth_bias.constant_factor = depthBiasConstantFactor;
   cmdbuf->state.rast.depth_bias.clamp = depthBiasClamp;
   cmdbuf->state.rast.depth_bias.slope_factor = depthBiasSlopeFactor;
   cmdbuf->state.dirty |= PANVK_DYNAMIC_DEPTH_BIAS;
   cmdbuf->state.fs_rsd = 0;
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(CmdSetBlendConstants)(VkCommandBuffer commandBuffer,
                                     const float blendConstants[4])
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);

   for (unsigned i = 0; i < 4; i++)
      cmdbuf->state.blend.constants[i] = CLAMP(blendConstants[i], 0.0f, 1.0f);

   cmdbuf->state.dirty |= PANVK_DYNAMIC_BLEND_CONSTANTS;
   cmdbuf->state.fs_rsd = 0;
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(CmdSetDepthBounds)(VkCommandBuffer commandBuffer,
                                  float minDepthBounds, float maxDepthBounds)
{
   panvk_stub();
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(CmdSetStencilCompareMask)(VkCommandBuffer commandBuffer,
                                         VkStencilFaceFlags faceMask,
                                         uint32_t compareMask)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);

   if (faceMask & VK_STENCIL_FACE_FRONT_BIT)
      cmdbuf->state.zs.s_front.compare_mask = compareMask;

   if (faceMask & VK_STENCIL_FACE_BACK_BIT)
      cmdbuf->state.zs.s_back.compare_mask = compareMask;

   cmdbuf->state.dirty |= PANVK_DYNAMIC_STENCIL_COMPARE_MASK;
   cmdbuf->state.fs_rsd = 0;
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(CmdSetStencilWriteMask)(VkCommandBuffer commandBuffer,
                                       VkStencilFaceFlags faceMask,
                                       uint32_t writeMask)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);

   if (faceMask & VK_STENCIL_FACE_FRONT_BIT)
      cmdbuf->state.zs.s_front.write_mask = writeMask;

   if (faceMask & VK_STENCIL_FACE_BACK_BIT)
      cmdbuf->state.zs.s_back.write_mask = writeMask;

   cmdbuf->state.dirty |= PANVK_DYNAMIC_STENCIL_WRITE_MASK;
   cmdbuf->state.fs_rsd = 0;
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(CmdSetStencilReference)(VkCommandBuffer commandBuffer,
                                       VkStencilFaceFlags faceMask,
                                       uint32_t reference)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);

   if (faceMask & VK_STENCIL_FACE_FRONT_BIT)
      cmdbuf->state.zs.s_front.ref = reference;

   if (faceMask & VK_STENCIL_FACE_BACK_BIT)
      cmdbuf->state.zs.s_back.ref = reference;

   cmdbuf->state.dirty |= PANVK_DYNAMIC_STENCIL_REFERENCE;
   cmdbuf->state.fs_rsd = 0;
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(CmdDrawIndirect)(VkCommandBuffer commandBuffer, VkBuffer _buffer,
                                VkDeviceSize offset, uint32_t drawCount,
                                uint32_t stride)
{
   panvk_stub();
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(CmdDrawIndexedIndirect)(VkCommandBuffer commandBuffer,
                                       VkBuffer _buffer, VkDeviceSize offset,
                                       uint32_t drawCount, uint32_t stride)
{
   panvk_stub();
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(CmdDispatchBase)(VkCommandBuffer commandBuffer, uint32_t base_x,
                                uint32_t base_y, uint32_t base_z, uint32_t x,
                                uint32_t y, uint32_t z)
{
   panvk_stub();
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(CmdDispatchIndirect)(VkCommandBuffer commandBuffer,
                                    VkBuffer _buffer, VkDeviceSize offset)
{
   panvk_stub();
}

static struct panvk_push_descriptor_set *
panvk_cmd_push_descriptors(struct panvk_cmd_buffer *cmdbuf,
                           VkPipelineBindPoint bind_point,
                           uint32_t set)
{
   struct panvk_cmd_bind_point_state *bind_point_state =
      &cmdbuf->bind_points[bind_point];
   struct panvk_descriptor_state *desc_state = &bind_point_state->desc_state;

   assert(set < MAX_SETS);
   if (unlikely(desc_state->push_sets[set] == NULL)) {
      desc_state->push_sets[set] =
         vk_zalloc(&cmdbuf->vk.pool->alloc, sizeof(*desc_state->push_sets[0]),
                   8, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
      if (unlikely(desc_state->push_sets[set] == NULL)) {
         vk_command_buffer_set_error(&cmdbuf->vk, VK_ERROR_OUT_OF_HOST_MEMORY);
         return NULL;
      }
   }

   /* Pushing descriptors replaces whatever sets are bound */
   desc_state->sets[set] = NULL;

   /* Reset all descs to force emission of new tables on the next draw/dispatch.
    * TODO: Be smarter and only reset those when required.
    */
   desc_state->ubos = 0;
   desc_state->textures = 0;
   desc_state->samplers = 0;
   desc_state->vs_attrib_bufs = desc_state->non_vs_attrib_bufs = 0;
   desc_state->vs_attribs = desc_state->non_vs_attribs = 0;
   return desc_state->push_sets[set];
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(CmdPushDescriptorSetKHR)(
   VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint,
   VkPipelineLayout layout, uint32_t set, uint32_t descriptorWriteCount,
   const VkWriteDescriptorSet *pDescriptorWrites)
{
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   VK_FROM_HANDLE(panvk_pipeline_layout, playout, layout);
   const struct panvk_descriptor_set_layout *set_layout =
      vk_to_panvk_descriptor_set_layout(playout->vk.set_layouts[set]);
   struct panvk_push_descriptor_set *push_set =
      panvk_cmd_push_descriptors(cmdbuf, pipelineBindPoint, set);
   if (!push_set)
      return;

   panvk_per_arch(push_descriptor_set)(push_set, set_layout,
                                       descriptorWriteCount, pDescriptorWrites);
}

VKAPI_ATTR void VKAPI_CALL
panvk_per_arch(CmdPushDescriptorSetWithTemplateKHR)(
   VkCommandBuffer commandBuffer,
   VkDescriptorUpdateTemplate descriptorUpdateTemplate, VkPipelineLayout layout,
   uint32_t set, const void *pData)
{
   VK_FROM_HANDLE(vk_descriptor_update_template, template, descriptorUpdateTemplate);
   VK_FROM_HANDLE(panvk_cmd_buffer, cmdbuf, commandBuffer);
   VK_FROM_HANDLE(panvk_pipeline_layout, playout, layout);
   const struct panvk_descriptor_set_layout *set_layout =
      vk_to_panvk_descriptor_set_layout(playout->vk.set_layouts[set]);
   struct panvk_push_descriptor_set *push_set =
      panvk_cmd_push_descriptors(cmdbuf, template->bind_point, set);
   if (!push_set)
      return;

   panvk_per_arch(push_descriptor_set_with_template)(
      push_set, set_layout, descriptorUpdateTemplate, pData);
}
