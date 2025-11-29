/*
 * Copyright © 2022 Collabora Ltd. and Red Hat Inc.
 * SPDX-License-Identifier: MIT
 */
#include "nvk_buffer.h"
#include "nvk_entrypoints.h"
#include "nvk_cmd_buffer.h"
#include "nvk_device.h"
#include "nvk_format.h"
#include "nvk_image.h"
#include "nvk_image_view.h"
#include "nvk_mme.h"
#include "nvk_physical_device.h"
#include "nvk_shader.h"

#include "util/bitpack_helpers.h"
#include "vk_format.h"
#include "vk_render_pass.h"
#include "vk_standard_sample_locations.h"

#include "nouveau_context.h"

#include "nvk_cl902d.h"
#include "nvk_cl9097.h"
#include "nvk_cl90b5.h"
#include "nvk_cl90c0.h"
#include "nvk_cla097.h"
#include "nvk_clb097.h"
#include "nvk_clb197.h"
#include "nvk_clc397.h"
#include "nvk_clc597.h"
#include "drf.h"

static inline uint16_t
nvk_cmd_buffer_3d_cls(struct nvk_cmd_buffer *cmd)
{
   struct nvk_device *dev = nvk_cmd_buffer_device(cmd);
   struct nvk_physical_device *pdev = nvk_device_physical(dev);
   return pdev->info.cls_eng3d;
}

void
nvk_mme_set_priv_reg(struct mme_builder *b)
{
   mme_mthd(b, NV9097_WAIT_FOR_IDLE);
   mme_emit(b, mme_zero());

   mme_mthd(b, NV9097_SET_MME_SHADOW_SCRATCH(0));
   mme_emit(b, mme_zero());
   mme_emit(b, mme_load(b));
   mme_emit(b, mme_load(b));

   mme_mthd(b, NV9097_SET_FALCON04);
   mme_emit(b, mme_load(b));

   struct mme_value loop_cond = mme_mov(b, mme_zero());
   mme_while(b, ine, loop_cond, mme_imm(1)) {
      mme_state_to(b, loop_cond, NV9097_SET_MME_SHADOW_SCRATCH(0));
      mme_mthd(b, NV9097_NO_OPERATION);
      mme_emit(b, mme_zero());
   };
}

VkResult
nvk_push_draw_state_init(struct nvk_device *dev, struct nv_push *p)
{
   struct nvk_physical_device *pdev = nvk_device_physical(dev);

   /* 3D state */
   P_MTHD(p, NV9097, SET_OBJECT);
   P_NV9097_SET_OBJECT(p, {
      .class_id = pdev->info.cls_eng3d,
      .engine_id = 0,
   });

   for (uint32_t mme = 0, mme_pos = 0; mme < NVK_MME_COUNT; mme++) {
      size_t size;
      uint32_t *dw = nvk_build_mme(&pdev->info, mme, &size);
      if (dw == NULL)
         return vk_error(dev, VK_ERROR_OUT_OF_HOST_MEMORY);

      assert(size % sizeof(uint32_t) == 0);
      const uint32_t num_dw = size / sizeof(uint32_t);

      P_MTHD(p, NV9097, LOAD_MME_START_ADDRESS_RAM_POINTER);
      P_NV9097_LOAD_MME_START_ADDRESS_RAM_POINTER(p, mme);
      P_NV9097_LOAD_MME_START_ADDRESS_RAM(p, mme_pos);

      P_1INC(p, NV9097, LOAD_MME_INSTRUCTION_RAM_POINTER);
      P_NV9097_LOAD_MME_INSTRUCTION_RAM_POINTER(p, mme_pos);
      P_INLINE_ARRAY(p, dw, num_dw);

      mme_pos += num_dw;

      free(dw);
   }

   if (pdev->info.cls_eng3d >= TURING_A)
      P_IMMD(p, NVC597, SET_MME_DATA_FIFO_CONFIG, FIFO_SIZE_SIZE_4KB);

   /* Enable FP helper invocation memory loads
    *
    * For generations with firmware support for our `SET_PRIV_REG` mme method
    * we simply use that. On older generations we'll let the kernel do it.
    * Starting with GSP we have to do it via the firmware anyway.
    *
    * This clears bit 3 of gr_gpcs_tpcs_sm_disp_ctrl
    * 
    * Without it,
    * dEQP-VK.subgroups.vote.frag_helper.subgroupallequal_bvec2_fragment will
    * occasionally fail.
    */
   if (pdev->info.cls_eng3d >= MAXWELL_B) {
      unsigned reg = pdev->info.cls_eng3d >= VOLTA_A ? 0x419ba4 : 0x419f78;
      P_1INC(p, NV9097, CALL_MME_MACRO(NVK_MME_SET_PRIV_REG));
      P_INLINE_DATA(p, 0);
      P_INLINE_DATA(p, BITFIELD_BIT(3));
      P_INLINE_DATA(p, reg);
   }

   /* Disable Out Of Range Address exceptions
    *
    * From the SPH documentation:
    *
    *    "The SPH fields StoreReqStart and StoreReqEnd set a range of
    *    attributes whose corresponding Odmap values of ST or ST_LAST are
    *    treated as ST_REQ. Normally, for an attribute whose Omap bit is TRUE
    *    and Odmap value is ST, when the shader writes data to this output, it
    *    can not count on being able to read it back, since the next
    *    downstream shader might have its Imap bit FALSE, thereby causing the
    *    Bmap bit to be FALSE. By including a ST type of attribute in the
    *    range of StoreReqStart and StoreReqEnd, the attribute’s Odmap value
    *    is treated as ST_REQ, so an Omap bit being TRUE causes the Bmap bit
    *    to be TRUE. This guarantees the shader program can output the value
    *    and then read it back later. This will save register space."
    *
    * It's unclear exactly what's going on but this seems to imply that the
    * hardware actually ANDs the output mask of one shader stage together with
    * the input mask of the subsequent shader stage to determine which values
    * are actually used.
    *
    * In the case when we have an empty fragment shader, it seems the hardware
    * doesn't allocate any output memory for final geometry stage at all and
    * so any writes to outputs from the final shader stage generates an Out Of
    * Range Address exception.  We could fix this by eliminating unused
    * outputs via cross-stage linking but that won't work in the case of
    * VK_EXT_shader_object and VK_EXT_graphics_pipeline_library fast-link.
    * Instead, the easiest solution is to just disable the exception.
    *
    * NOTE (Faith):
    *
    *    This above analysis is 100% conjecture on my part based on a creative
    *    reading of the SPH docs and what I saw when trying to run certain
    *    OpenGL CTS tests on NVK + Zink.  Without access to NVIDIA HW
    *    engineers, have no way of verifying this analysis.
    *
    *    The CTS test in question is:
    *
    *    KHR-GL46.tessellation_shader.tessellation_control_to_tessellation_evaluation.gl_tessLevel
    *
    * This should also prevent any issues with array overruns on I/O arrays.
    * Before, they would get an exception and kill the context whereas now
    * they should gently get ignored.
    *
    * This clears bit 14 of gr_gpcs_tpcs_sms_hww_warp_esr_report_mask
    */
   if (pdev->info.cls_eng3d >= MAXWELL_B) {
      unsigned reg = pdev->info.cls_eng3d >= VOLTA_A ? 0x419ea8 : 0x419e44;
      P_1INC(p, NV9097, CALL_MME_MACRO(NVK_MME_SET_PRIV_REG));
      P_INLINE_DATA(p, 0);
      P_INLINE_DATA(p, BITFIELD_BIT(14));
      P_INLINE_DATA(p, reg);
   }

   P_IMMD(p, NV9097, SET_RENDER_ENABLE_C, MODE_TRUE);

   P_IMMD(p, NV9097, SET_Z_COMPRESSION, ENABLE_TRUE);
   P_MTHD(p, NV9097, SET_COLOR_COMPRESSION(0));
   for (unsigned i = 0; i < 8; i++)
      P_NV9097_SET_COLOR_COMPRESSION(p, i, ENABLE_TRUE);

   P_IMMD(p, NV9097, SET_CT_SELECT, { .target_count = 1 });

//   P_MTHD(cmd->push, NVC0_3D, CSAA_ENABLE);
//   P_INLINE_DATA(cmd->push, 0);

   P_IMMD(p, NV9097, SET_ALIASED_LINE_WIDTH_ENABLE, V_TRUE);

   P_IMMD(p, NV9097, SET_DA_PRIMITIVE_RESTART_VERTEX_ARRAY, ENABLE_FALSE);

   P_IMMD(p, NV9097, SET_BLEND_SEPARATE_FOR_ALPHA, ENABLE_TRUE);
   P_IMMD(p, NV9097, SET_SINGLE_CT_WRITE_CONTROL, ENABLE_TRUE);
   P_IMMD(p, NV9097, SET_SINGLE_ROP_CONTROL, ENABLE_FALSE);
   P_IMMD(p, NV9097, SET_TWO_SIDED_STENCIL_TEST, ENABLE_TRUE);

   P_IMMD(p, NV9097, SET_SHADE_MODE, V_OGL_SMOOTH);

   P_IMMD(p, NV9097, SET_API_VISIBLE_CALL_LIMIT, V__128);

   P_IMMD(p, NV9097, SET_ZCULL_STATS, ENABLE_TRUE);

   P_IMMD(p, NV9097, SET_L1_CONFIGURATION,
                     DIRECTLY_ADDRESSABLE_MEMORY_SIZE_48KB);

   P_IMMD(p, NV9097, SET_REDUCE_COLOR_THRESHOLDS_ENABLE, V_FALSE);
   P_IMMD(p, NV9097, SET_REDUCE_COLOR_THRESHOLDS_UNORM8, {
      .all_covered_all_hit_once = 0xff,
   });
   P_MTHD(p, NV9097, SET_REDUCE_COLOR_THRESHOLDS_UNORM10);
   P_NV9097_SET_REDUCE_COLOR_THRESHOLDS_UNORM10(p, {
      .all_covered_all_hit_once = 0xff,
   });
   P_NV9097_SET_REDUCE_COLOR_THRESHOLDS_UNORM16(p, {
      .all_covered_all_hit_once = 0xff,
   });
   P_NV9097_SET_REDUCE_COLOR_THRESHOLDS_FP11(p, {
      .all_covered_all_hit_once = 0x3f,
   });
   P_NV9097_SET_REDUCE_COLOR_THRESHOLDS_FP16(p, {
      .all_covered_all_hit_once = 0xff,
   });
   P_NV9097_SET_REDUCE_COLOR_THRESHOLDS_SRGB8(p, {
      .all_covered_all_hit_once = 0xff,
   });

   if (pdev->info.cls_eng3d < VOLTA_A)
      P_IMMD(p, NV9097, SET_ALPHA_FRACTION, 0x3f);

   P_IMMD(p, NV9097, CHECK_SPH_VERSION, {
      .current = 3,
      .oldest_supported = 3,
   });
   P_IMMD(p, NV9097, CHECK_AAM_VERSION, {
      .current = 2,
      .oldest_supported = 2,
   });

   if (pdev->info.cls_eng3d < MAXWELL_A)
      P_IMMD(p, NV9097, SET_SHADER_SCHEDULING, MODE_OLDEST_THREAD_FIRST);

   P_IMMD(p, NV9097, SET_L2_CACHE_CONTROL_FOR_ROP_PREFETCH_READ_REQUESTS,
                     POLICY_EVICT_NORMAL);
   P_IMMD(p, NV9097, SET_L2_CACHE_CONTROL_FOR_ROP_NONINTERLOCKED_READ_REQUESTS,
                     POLICY_EVICT_NORMAL);
   P_IMMD(p, NV9097, SET_L2_CACHE_CONTROL_FOR_ROP_INTERLOCKED_READ_REQUESTS,
                     POLICY_EVICT_NORMAL);
   P_IMMD(p, NV9097, SET_L2_CACHE_CONTROL_FOR_ROP_NONINTERLOCKED_WRITE_REQUESTS,
                     POLICY_EVICT_NORMAL);
   P_IMMD(p, NV9097, SET_L2_CACHE_CONTROL_FOR_ROP_INTERLOCKED_WRITE_REQUESTS,
                     POLICY_EVICT_NORMAL);

   P_IMMD(p, NV9097, SET_BLEND_PER_FORMAT_ENABLE, SNORM8_UNORM16_SNORM16_TRUE);

   P_IMMD(p, NV9097, SET_ATTRIBUTE_DEFAULT, {
      .color_front_diffuse    = COLOR_FRONT_DIFFUSE_VECTOR_0001,
      .color_front_specular   = COLOR_FRONT_SPECULAR_VECTOR_0001,
      .generic_vector         = GENERIC_VECTOR_VECTOR_0001,
      .fixed_fnc_texture      = FIXED_FNC_TEXTURE_VECTOR_0001,
      .dx9_color0             = DX9_COLOR0_VECTOR_0001,
      .dx9_color1_to_color15  = DX9_COLOR1_TO_COLOR15_VECTOR_0000,
   });

   P_IMMD(p, NV9097, SET_DA_OUTPUT, VERTEX_ID_USES_ARRAY_START_TRUE);

   P_IMMD(p, NV9097, SET_RENDER_ENABLE_CONTROL,
                     CONDITIONAL_LOAD_CONSTANT_BUFFER_FALSE);

   P_IMMD(p, NV9097, SET_PS_OUTPUT_SAMPLE_MASK_USAGE, {
      .enable                       = ENABLE_TRUE,
      .qualify_by_anti_alias_enable = QUALIFY_BY_ANTI_ALIAS_ENABLE_ENABLE,
   });

   if (pdev->info.cls_eng3d < VOLTA_A)
      P_IMMD(p, NV9097, SET_PRIM_CIRCULAR_BUFFER_THROTTLE, 0x3fffff);

   P_IMMD(p, NV9097, SET_BLEND_OPT_CONTROL, ALLOW_FLOAT_PIXEL_KILLS_TRUE);
   P_IMMD(p, NV9097, SET_BLEND_FLOAT_OPTION, ZERO_TIMES_ANYTHING_IS_ZERO_TRUE);
   P_IMMD(p, NV9097, SET_BLEND_STATE_PER_TARGET, ENABLE_TRUE);

   if (pdev->info.cls_eng3d < MAXWELL_A)
      P_IMMD(p, NV9097, SET_MAX_TI_WARPS_PER_BATCH, 3);

   if (pdev->info.cls_eng3d >= KEPLER_A &&
       pdev->info.cls_eng3d < MAXWELL_A) {
      P_IMMD(p, NVA097, SET_TEXTURE_INSTRUCTION_OPERAND,
                        ORDERING_KEPLER_ORDER);
   }

   P_IMMD(p, NV9097, SET_ALPHA_TEST, ENABLE_FALSE);
   P_IMMD(p, NV9097, SET_TWO_SIDED_LIGHT, ENABLE_FALSE);
   P_IMMD(p, NV9097, SET_COLOR_CLAMP, ENABLE_TRUE);
   P_IMMD(p, NV9097, SET_PS_SATURATE, {
      .output0 = OUTPUT0_FALSE,
      .output1 = OUTPUT1_FALSE,
      .output2 = OUTPUT2_FALSE,
      .output3 = OUTPUT3_FALSE,
      .output4 = OUTPUT4_FALSE,
      .output5 = OUTPUT5_FALSE,
      .output6 = OUTPUT6_FALSE,
      .output7 = OUTPUT7_FALSE,
   });

   P_IMMD(p, NV9097, SET_POINT_SIZE, fui(1.0));
   P_IMMD(p, NV9097, SET_ATTRIBUTE_POINT_SIZE, { .enable = ENABLE_TRUE });

   /* From vulkan spec's point rasterization:
    * "Point rasterization produces a fragment for each fragment area group of
    * framebuffer pixels with one or more sample points that intersect a region
    * centered at the point’s (xf,yf).
    * This region is a square with side equal to the current point size.
    * ... (xf,yf) is the exact, unrounded framebuffer coordinate of the vertex
    * for the point"
    *
    * So it seems we always need square points with PointCoords like OpenGL
    * point sprites.
    *
    * From OpenGL compatibility spec:
    * Basic point rasterization:
    * "If point sprites are enabled, then point rasterization produces a
    * fragment for each framebuffer pixel whose center lies inside a square
    * centered at the point’s (xw, yw), with side length equal to the current
    * point size.
    * ... and xw and yw are the exact, unrounded window coordinates of the
    * vertex for the point"
    *
    * And Point multisample rasterization:
    * "This region is a circle having diameter equal to the current point width
    * if POINT_SPRITE is disabled, or a square with side equal to the current
    * point width if POINT_SPRITE is enabled."
    */
   P_IMMD(p, NV9097, SET_POINT_SPRITE, ENABLE_TRUE);
   P_IMMD(p, NV9097, SET_POINT_SPRITE_SELECT, {
      .rmode      = RMODE_ZERO,
      .origin     = ORIGIN_TOP,
      .texture0   = TEXTURE0_PASSTHROUGH,
      .texture1   = TEXTURE1_PASSTHROUGH,
      .texture2   = TEXTURE2_PASSTHROUGH,
      .texture3   = TEXTURE3_PASSTHROUGH,
      .texture4   = TEXTURE4_PASSTHROUGH,
      .texture5   = TEXTURE5_PASSTHROUGH,
      .texture6   = TEXTURE6_PASSTHROUGH,
      .texture7   = TEXTURE7_PASSTHROUGH,
      .texture8   = TEXTURE8_PASSTHROUGH,
      .texture9   = TEXTURE9_PASSTHROUGH,
   });

   /* OpenGL's GL_POINT_SMOOTH */
   P_IMMD(p, NV9097, SET_ANTI_ALIASED_POINT, ENABLE_FALSE);

   if (pdev->info.cls_eng3d >= MAXWELL_B)
      P_IMMD(p, NVB197, SET_FILL_VIA_TRIANGLE, MODE_DISABLED);

   P_IMMD(p, NV9097, SET_POLY_SMOOTH, ENABLE_FALSE);

   P_IMMD(p, NV9097, SET_VIEWPORT_PIXEL, CENTER_AT_HALF_INTEGERS);

   P_IMMD(p, NV9097, SET_HYBRID_ANTI_ALIAS_CONTROL, {
      .passes     = 1,
      .centroid   = CENTROID_PER_FRAGMENT,
   });

   /* Enable multisample rasterization even for one sample rasterization,
    * this way we get strict lines and rectangular line support.
    * More info at: DirectX rasterization rules
    */
   P_IMMD(p, NV9097, SET_ANTI_ALIAS_ENABLE, V_TRUE);

   if (pdev->info.cls_eng3d >= MAXWELL_B) {
      P_IMMD(p, NVB197, SET_OFFSET_RENDER_TARGET_INDEX,
                        BY_VIEWPORT_INDEX_FALSE);
   }

   /* TODO: Vertex runout */

   P_IMMD(p, NV9097, SET_WINDOW_ORIGIN, {
      .mode    = MODE_UPPER_LEFT,
      .flip_y  = FLIP_Y_FALSE,
   });

   P_MTHD(p, NV9097, SET_WINDOW_OFFSET_X);
   P_NV9097_SET_WINDOW_OFFSET_X(p, 0);
   P_NV9097_SET_WINDOW_OFFSET_Y(p, 0);

   P_IMMD(p, NV9097, SET_ACTIVE_ZCULL_REGION, 0x3f);
   P_IMMD(p, NV9097, SET_WINDOW_CLIP_ENABLE, V_FALSE);
   P_IMMD(p, NV9097, SET_CLIP_ID_TEST, ENABLE_FALSE);

//   P_IMMD(p, NV9097, X_X_X_SET_CLEAR_CONTROL, {
//      .respect_stencil_mask   = RESPECT_STENCIL_MASK_FALSE,
//      .use_clear_rect         = USE_CLEAR_RECT_FALSE,
//   });

   P_IMMD(p, NV9097, SET_VIEWPORT_SCALE_OFFSET, ENABLE_TRUE);

   P_IMMD(p, NV9097, SET_VIEWPORT_CLIP_CONTROL, {
      .min_z_zero_max_z_one      = MIN_Z_ZERO_MAX_Z_ONE_FALSE,
      .pixel_min_z               = PIXEL_MIN_Z_CLAMP,
      .pixel_max_z               = PIXEL_MAX_Z_CLAMP,
      .geometry_guardband        = GEOMETRY_GUARDBAND_SCALE_256,
      .line_point_cull_guardband = LINE_POINT_CULL_GUARDBAND_SCALE_256,
      .geometry_clip             = GEOMETRY_CLIP_WZERO_CLIP,
      .geometry_guardband_z      = GEOMETRY_GUARDBAND_Z_SAME_AS_XY_GUARDBAND,
   });

   for (unsigned i = 0; i < 16; i++)
      P_IMMD(p, NV9097, SET_SCISSOR_ENABLE(i), V_FALSE);

   P_IMMD(p, NV9097, SET_CT_MRT_ENABLE, V_TRUE);

   if (pdev->info.cls_eng3d < VOLTA_A) {
      uint64_t shader_base_addr =
         nvk_heap_contiguous_base_address(&dev->shader_heap);

      P_MTHD(p, NV9097, SET_PROGRAM_REGION_A);
      P_NV9097_SET_PROGRAM_REGION_A(p, shader_base_addr >> 32);
      P_NV9097_SET_PROGRAM_REGION_B(p, shader_base_addr);
   }

   for (uint32_t group = 0; group < 5; group++) {
      for (uint32_t slot = 0; slot < 16; slot++) {
         P_IMMD(p, NV9097, BIND_GROUP_CONSTANT_BUFFER(group), {
            .valid = VALID_FALSE,
            .shader_slot = slot,
         });
      }
   }

//   P_MTHD(cmd->push, NVC0_3D, MACRO_GP_SELECT);
//   P_INLINE_DATA(cmd->push, 0x40);
   P_IMMD(p, NV9097, SET_RT_LAYER, {
      .v = 0,
      .control = CONTROL_V_SELECTS_LAYER,
   });
//   P_MTHD(cmd->push, NVC0_3D, MACRO_TEP_SELECT;
//   P_INLINE_DATA(cmd->push, 0x30);

   P_IMMD(p, NV9097, SET_POINT_CENTER_MODE, V_OGL);
   P_IMMD(p, NV9097, SET_EDGE_FLAG, V_TRUE);
   P_IMMD(p, NV9097, SET_SAMPLER_BINDING, V_INDEPENDENTLY);

   uint64_t zero_addr = dev->zero_page->offset;
   P_MTHD(p, NV9097, SET_VERTEX_STREAM_SUBSTITUTE_A);
   P_NV9097_SET_VERTEX_STREAM_SUBSTITUTE_A(p, zero_addr >> 32);
   P_NV9097_SET_VERTEX_STREAM_SUBSTITUTE_B(p, zero_addr);

   if (pdev->info.cls_eng3d >= FERMI_A &&
       pdev->info.cls_eng3d < MAXWELL_A) {
      assert(dev->vab_memory);
      uint64_t vab_addr = dev->vab_memory->offset;
      P_MTHD(p, NV9097, SET_VAB_MEMORY_AREA_A);
      P_NV9097_SET_VAB_MEMORY_AREA_A(p, vab_addr >> 32);
      P_NV9097_SET_VAB_MEMORY_AREA_B(p, vab_addr);
      P_NV9097_SET_VAB_MEMORY_AREA_C(p, SIZE_BYTES_256K);
   }

   if (pdev->info.cls_eng3d == MAXWELL_A)
      P_IMMD(p, NVB097, SET_SELECT_MAXWELL_TEXTURE_HEADERS, V_TRUE);

   return VK_SUCCESS;
}

static void
nvk_cmd_buffer_dirty_render_pass(struct nvk_cmd_buffer *cmd)
{
   struct vk_dynamic_graphics_state *dyn = &cmd->vk.dynamic_graphics_state;

   /* These depend on color attachment count */
   BITSET_SET(dyn->dirty, MESA_VK_DYNAMIC_CB_COLOR_WRITE_ENABLES);
   BITSET_SET(dyn->dirty, MESA_VK_DYNAMIC_CB_BLEND_ENABLES);
   BITSET_SET(dyn->dirty, MESA_VK_DYNAMIC_CB_BLEND_EQUATIONS);
   BITSET_SET(dyn->dirty, MESA_VK_DYNAMIC_CB_WRITE_MASKS);

   /* These depend on the depth/stencil format */
   BITSET_SET(dyn->dirty, MESA_VK_DYNAMIC_DS_DEPTH_TEST_ENABLE);
   BITSET_SET(dyn->dirty, MESA_VK_DYNAMIC_DS_DEPTH_WRITE_ENABLE);
   BITSET_SET(dyn->dirty, MESA_VK_DYNAMIC_DS_DEPTH_BOUNDS_TEST_ENABLE);
   BITSET_SET(dyn->dirty, MESA_VK_DYNAMIC_DS_STENCIL_TEST_ENABLE);

   /* This may depend on render targets for ESO */
   BITSET_SET(dyn->dirty, MESA_VK_DYNAMIC_MS_RASTERIZATION_SAMPLES);
}

void
nvk_cmd_buffer_begin_graphics(struct nvk_cmd_buffer *cmd,
                              const VkCommandBufferBeginInfo *pBeginInfo)
{
   if (cmd->vk.level == VK_COMMAND_BUFFER_LEVEL_PRIMARY) {
      struct nv_push *p = nvk_cmd_buffer_push(cmd, 5);
      P_MTHD(p, NV9097, INVALIDATE_SAMPLER_CACHE_NO_WFI);
      P_NV9097_INVALIDATE_SAMPLER_CACHE_NO_WFI(p, {
         .lines = LINES_ALL,
      });
      P_NV9097_INVALIDATE_TEXTURE_HEADER_CACHE_NO_WFI(p, {
         .lines = LINES_ALL,
      });

      P_IMMD(p, NVA097, INVALIDATE_SHADER_CACHES_NO_WFI, {
         .constant = CONSTANT_TRUE,
      });
   }

   if (cmd->vk.level != VK_COMMAND_BUFFER_LEVEL_PRIMARY &&
       (pBeginInfo->flags & VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT)) {
      char gcbiar_data[VK_GCBIARR_DATA_SIZE(NVK_MAX_RTS)];
      const VkRenderingInfo *resume_info =
         vk_get_command_buffer_inheritance_as_rendering_resume(cmd->vk.level,
                                                               pBeginInfo,
                                                               gcbiar_data);
      if (resume_info) {
         nvk_CmdBeginRendering(nvk_cmd_buffer_to_handle(cmd), resume_info);
      } else {
         const VkCommandBufferInheritanceRenderingInfo *inheritance_info =
            vk_get_command_buffer_inheritance_rendering_info(cmd->vk.level,
                                                             pBeginInfo);
         assert(inheritance_info);

         struct nvk_rendering_state *render = &cmd->state.gfx.render;
         render->flags = inheritance_info->flags;
         render->area = (VkRect2D) { };
         render->layer_count = 0;
         render->view_mask = inheritance_info->viewMask;
         render->samples = inheritance_info->rasterizationSamples;

         render->color_att_count = inheritance_info->colorAttachmentCount;
         for (uint32_t i = 0; i < render->color_att_count; i++) {
            render->color_att[i].vk_format =
               inheritance_info->pColorAttachmentFormats[i];
         }
         render->depth_att.vk_format =
            inheritance_info->depthAttachmentFormat;
         render->stencil_att.vk_format =
            inheritance_info->stencilAttachmentFormat;

         nvk_cmd_buffer_dirty_render_pass(cmd);
      }
   }

   cmd->state.gfx.shaders_dirty = ~0;
}

void
nvk_cmd_invalidate_graphics_state(struct nvk_cmd_buffer *cmd)
{
   vk_dynamic_graphics_state_dirty_all(&cmd->vk.dynamic_graphics_state);

   /* From the Vulkan 1.3.275 spec:
    *
    *    "...There is one exception to this rule - if the primary command
    *    buffer is inside a render pass instance, then the render pass and
    *    subpass state is not disturbed by executing secondary command
    *    buffers."
    *
    * We need to reset everything EXCEPT the render pass state.
    */
   struct nvk_rendering_state render_save = cmd->state.gfx.render;
   memset(&cmd->state.gfx, 0, sizeof(cmd->state.gfx));
   cmd->state.gfx.render = render_save;

   cmd->state.gfx.shaders_dirty = ~0;
}

static void
nvk_attachment_init(struct nvk_attachment *att,
                    const VkRenderingAttachmentInfo *info)
{
   if (info == NULL || info->imageView == VK_NULL_HANDLE) {
      *att = (struct nvk_attachment) { .iview = NULL, };
      return;
   }

   VK_FROM_HANDLE(nvk_image_view, iview, info->imageView);
   *att = (struct nvk_attachment) {
      .vk_format = iview->vk.format,
      .iview = iview,
   };

   if (info->resolveMode != VK_RESOLVE_MODE_NONE) {
      VK_FROM_HANDLE(nvk_image_view, res_iview, info->resolveImageView);
      att->resolve_mode = info->resolveMode;
      att->resolve_iview = res_iview;
   }

   att->store_op = info->storeOp;
}

static uint32_t
nil_to_nv9097_samples_mode(enum nil_sample_layout sample_layout)
{
#define MODE(S) [NIL_SAMPLE_LAYOUT_##S] = NV9097_SET_ANTI_ALIAS_SAMPLES_MODE_##S
   uint16_t nil_to_nv9097[] = {
      MODE(1X1),
      MODE(2X1),
      MODE(2X2),
      MODE(4X2),
      MODE(4X4),
   };
#undef MODE
   assert(sample_layout < ARRAY_SIZE(nil_to_nv9097));

   return nil_to_nv9097[sample_layout];
}

VKAPI_ATTR void VKAPI_CALL
nvk_GetRenderingAreaGranularityKHR(
    VkDevice device,
    const VkRenderingAreaInfoKHR *pRenderingAreaInfo,
    VkExtent2D *pGranularity)
{
   *pGranularity = (VkExtent2D) { .width = 1, .height = 1 };
}

static bool
nvk_rendering_all_linear(const struct nvk_rendering_state *render)
{
   /* Depth and stencil are never linear */
   if (render->depth_att.iview || render->stencil_att.iview)
      return false;

   for (uint32_t i = 0; i < render->color_att_count; i++) {
      const struct nvk_image_view *iview = render->color_att[i].iview;
      if (iview == NULL)
         continue;

      const struct nvk_image *image = (struct nvk_image *)iview->vk.image;
      const uint8_t ip = iview->planes[0].image_plane;
      const struct nil_image_level *level =
         &image->planes[ip].nil.levels[iview->vk.base_mip_level];

      if (level->tiling.is_tiled)
         return false;
   }

   return true;
}

VKAPI_ATTR void VKAPI_CALL
nvk_CmdBeginRendering(VkCommandBuffer commandBuffer,
                      const VkRenderingInfo *pRenderingInfo)
{
   VK_FROM_HANDLE(nvk_cmd_buffer, cmd, commandBuffer);
   struct nvk_rendering_state *render = &cmd->state.gfx.render;

   memset(render, 0, sizeof(*render));

   render->flags = pRenderingInfo->flags;
   render->area = pRenderingInfo->renderArea;
   render->view_mask = pRenderingInfo->viewMask;
   render->layer_count = pRenderingInfo->layerCount;
   render->samples = 0;

   const uint32_t layer_count =
      render->view_mask ? util_last_bit(render->view_mask) :
                          render->layer_count;

   render->color_att_count = pRenderingInfo->colorAttachmentCount;
   for (uint32_t i = 0; i < render->color_att_count; i++) {
      nvk_attachment_init(&render->color_att[i],
                          &pRenderingInfo->pColorAttachments[i]);
   }

   nvk_attachment_init(&render->depth_att,
                       pRenderingInfo->pDepthAttachment);
   nvk_attachment_init(&render->stencil_att,
                       pRenderingInfo->pStencilAttachment);

   nvk_cmd_buffer_dirty_render_pass(cmd);

   /* Always emit at least one color attachment, even if it's just a dummy. */
   uint32_t color_att_count = MAX2(1, render->color_att_count);
   struct nv_push *p = nvk_cmd_buffer_push(cmd, color_att_count * 12 + 29);

   P_IMMD(p, NV9097, SET_MME_SHADOW_SCRATCH(NVK_MME_SCRATCH_VIEW_MASK),
          render->view_mask);

   P_MTHD(p, NV9097, SET_SURFACE_CLIP_HORIZONTAL);
   P_NV9097_SET_SURFACE_CLIP_HORIZONTAL(p, {
      .x       = render->area.offset.x,
      .width   = render->area.extent.width,
   });
   P_NV9097_SET_SURFACE_CLIP_VERTICAL(p, {
      .y       = render->area.offset.y,
      .height  = render->area.extent.height,
   });

   const bool all_linear = nvk_rendering_all_linear(render);

   enum nil_sample_layout sample_layout = NIL_SAMPLE_LAYOUT_INVALID;
   for (uint32_t i = 0; i < color_att_count; i++) {
      if (render->color_att[i].iview) {
         const struct nvk_image_view *iview = render->color_att[i].iview;
         const struct nvk_image *image = (struct nvk_image *)iview->vk.image;
         /* Rendering to multi-planar images is valid for a specific single plane
          * only, so assert that what we have is a single-plane, obtain its index,
          * and begin rendering
          */
         assert(iview->plane_count == 1);
         const uint8_t ip = iview->planes[0].image_plane;
         const struct nvk_image_plane *plane = &image->planes[ip];

         const VkAttachmentLoadOp load_op =
            pRenderingInfo->pColorAttachments[i].loadOp;
         if (!all_linear && !plane->nil.levels[0].tiling.is_tiled) {
            if (load_op == VK_ATTACHMENT_LOAD_OP_LOAD)
               nvk_linear_render_copy(cmd, iview, render->area, true);

            plane = &image->linear_tiled_shadow;
         }

         const struct nil_image *nil_image = &plane->nil;
         const struct nil_image_level *level =
            &nil_image->levels[iview->vk.base_mip_level];
         struct nil_Extent4D_Samples level_extent_sa =
            nil_image_level_extent_sa(nil_image, iview->vk.base_mip_level);

         assert(sample_layout == NIL_SAMPLE_LAYOUT_INVALID ||
                sample_layout == nil_image->sample_layout);
         sample_layout = nil_image->sample_layout;
         render->samples = image->vk.samples;

         uint64_t addr = nvk_image_plane_base_address(plane) + level->offset_B;

         if (nil_image->dim == NIL_IMAGE_DIM_3D) {
            addr += nil_image_level_z_offset_B(nil_image,
                                               iview->vk.base_mip_level,
                                               iview->vk.base_array_layer);
         } else {
            addr += iview->vk.base_array_layer *
                    (uint64_t)nil_image->array_stride_B;
         }

         P_MTHD(p, NV9097, SET_COLOR_TARGET_A(i));
         P_NV9097_SET_COLOR_TARGET_A(p, i, addr >> 32);
         P_NV9097_SET_COLOR_TARGET_B(p, i, addr);

         if (level->tiling.is_tiled) {
            const enum pipe_format p_format =
               vk_format_to_pipe_format(iview->vk.format);

            /* We use the stride for depth/stencil targets because the Z/S
             * hardware has no concept of a tile width.  Instead, we just set
             * the width to the stride divided by bpp.
             */
            const uint32_t row_stride_el =
               level->row_stride_B / util_format_get_blocksize(p_format);
            P_NV9097_SET_COLOR_TARGET_WIDTH(p, i, row_stride_el);
            P_NV9097_SET_COLOR_TARGET_HEIGHT(p, i, level_extent_sa.height);
            const uint8_t ct_format = nil_format_to_color_target(p_format);
            P_NV9097_SET_COLOR_TARGET_FORMAT(p, i, ct_format);

            P_NV9097_SET_COLOR_TARGET_MEMORY(p, i, {
               .block_width   = BLOCK_WIDTH_ONE_GOB,
               .block_height  = level->tiling.y_log2,
               .block_depth   = level->tiling.z_log2,
               .layout        = LAYOUT_BLOCKLINEAR,
               .third_dimension_control = (nil_image->dim == NIL_IMAGE_DIM_3D) ?
                  THIRD_DIMENSION_CONTROL_THIRD_DIMENSION_DEFINES_DEPTH_SIZE :
                  THIRD_DIMENSION_CONTROL_THIRD_DIMENSION_DEFINES_ARRAY_SIZE,
            });

            P_NV9097_SET_COLOR_TARGET_THIRD_DIMENSION(p, i, layer_count);
            P_NV9097_SET_COLOR_TARGET_ARRAY_PITCH(p, i,
               nil_image->array_stride_B >> 2);
            P_NV9097_SET_COLOR_TARGET_LAYER(p, i, 0);
         } else {
            /* NVIDIA can only render to 2D linear images */
            assert(nil_image->dim == NIL_IMAGE_DIM_2D);
            /* NVIDIA can only render to non-multisampled images */
            assert(sample_layout == NIL_SAMPLE_LAYOUT_1X1);
            /* NVIDIA doesn't support linear array images */
            assert(iview->vk.base_array_layer == 0 && layer_count == 1);

            uint32_t pitch = level->row_stride_B;
            const enum pipe_format p_format =
               vk_format_to_pipe_format(iview->vk.format);
            /* When memory layout is set to LAYOUT_PITCH, the WIDTH field
             * takes row pitch
             */
            P_NV9097_SET_COLOR_TARGET_WIDTH(p, i, pitch);
            P_NV9097_SET_COLOR_TARGET_HEIGHT(p, i, level_extent_sa.height);

            const uint8_t ct_format = nil_format_to_color_target(p_format);
            P_NV9097_SET_COLOR_TARGET_FORMAT(p, i, ct_format);

            P_NV9097_SET_COLOR_TARGET_MEMORY(p, i, {
               .layout = LAYOUT_PITCH,
               .third_dimension_control =
                  THIRD_DIMENSION_CONTROL_THIRD_DIMENSION_DEFINES_ARRAY_SIZE,
            });

            P_NV9097_SET_COLOR_TARGET_THIRD_DIMENSION(p, i, 1);
            P_NV9097_SET_COLOR_TARGET_ARRAY_PITCH(p, i, 0);
            P_NV9097_SET_COLOR_TARGET_LAYER(p, i, 0);
         }

         P_IMMD(p, NV9097, SET_COLOR_COMPRESSION(i), nil_image->compressed);
      } else {
         P_MTHD(p, NV9097, SET_COLOR_TARGET_A(i));
         P_NV9097_SET_COLOR_TARGET_A(p, i, 0);
         P_NV9097_SET_COLOR_TARGET_B(p, i, 0);
         P_NV9097_SET_COLOR_TARGET_WIDTH(p, i, 64);
         P_NV9097_SET_COLOR_TARGET_HEIGHT(p, i, 0);
         P_NV9097_SET_COLOR_TARGET_FORMAT(p, i, V_DISABLED);
         P_NV9097_SET_COLOR_TARGET_MEMORY(p, i, {
            .layout        = LAYOUT_BLOCKLINEAR,
         });
         P_NV9097_SET_COLOR_TARGET_THIRD_DIMENSION(p, i, layer_count);
         P_NV9097_SET_COLOR_TARGET_ARRAY_PITCH(p, i, 0);
         P_NV9097_SET_COLOR_TARGET_LAYER(p, i, 0);

         P_IMMD(p, NV9097, SET_COLOR_COMPRESSION(i), ENABLE_TRUE);
      }
   }

   P_IMMD(p, NV9097, SET_CT_SELECT, {
      .target_count = color_att_count,
      .target0 = 0,
      .target1 = 1,
      .target2 = 2,
      .target3 = 3,
      .target4 = 4,
      .target5 = 5,
      .target6 = 6,
      .target7 = 7,
   });

   if (render->depth_att.iview || render->stencil_att.iview) {
      struct nvk_image_view *iview = render->depth_att.iview ?
                                     render->depth_att.iview :
                                     render->stencil_att.iview;
      const struct nvk_image *image = (struct nvk_image *)iview->vk.image;
      /* Depth/stencil are always single-plane */
      assert(iview->plane_count == 1);
      const uint8_t ip = iview->planes[0].image_plane;
      struct nil_image nil_image = image->planes[ip].nil;

      uint64_t addr = nvk_image_base_address(image, ip);
      uint32_t mip_level = iview->vk.base_mip_level;
      uint32_t base_array_layer = iview->vk.base_array_layer;
      uint32_t layer_count = iview->vk.layer_count;

      if (nil_image.dim == NIL_IMAGE_DIM_3D) {
         uint64_t level_offset_B;
         nil_image = nil_image_3d_level_as_2d_array(&nil_image, mip_level,
                                                    &level_offset_B);
         addr += level_offset_B;
         mip_level = 0;
         base_array_layer = 0;
         layer_count = iview->vk.extent.depth;
      }

      const struct nil_image_level *level = &nil_image.levels[mip_level];
      addr += level->offset_B;

      assert(sample_layout == NIL_SAMPLE_LAYOUT_INVALID ||
             sample_layout == nil_image.sample_layout);
      sample_layout = nil_image.sample_layout;
      render->samples = image->vk.samples;

      P_MTHD(p, NV9097, SET_ZT_A);
      P_NV9097_SET_ZT_A(p, addr >> 32);
      P_NV9097_SET_ZT_B(p, addr);
      const enum pipe_format p_format =
         vk_format_to_pipe_format(iview->vk.format);
      const uint8_t zs_format = nil_format_to_depth_stencil(p_format);
      P_NV9097_SET_ZT_FORMAT(p, zs_format);
      assert(level->tiling.is_tiled);
      assert(level->tiling.z_log2 == 0);
      P_NV9097_SET_ZT_BLOCK_SIZE(p, {
         .width = WIDTH_ONE_GOB,
         .height = level->tiling.y_log2,
         .depth = DEPTH_ONE_GOB,
      });
      P_NV9097_SET_ZT_ARRAY_PITCH(p, nil_image.array_stride_B >> 2);

      P_IMMD(p, NV9097, SET_ZT_SELECT, 1 /* target_count */);

      struct nil_Extent4D_Samples level_extent_sa =
         nil_image_level_extent_sa(&nil_image, mip_level);

      /* We use the stride for depth/stencil targets because the Z/S hardware
       * has no concept of a tile width.  Instead, we just set the width to
       * the stride divided by bpp.
       */
      const uint32_t row_stride_el =
         level->row_stride_B / util_format_get_blocksize(p_format);

      P_MTHD(p, NV9097, SET_ZT_SIZE_A);
      P_NV9097_SET_ZT_SIZE_A(p, row_stride_el);
      P_NV9097_SET_ZT_SIZE_B(p, level_extent_sa.height);
      P_NV9097_SET_ZT_SIZE_C(p, {
         .third_dimension  = base_array_layer + layer_count,
         .control          = CONTROL_THIRD_DIMENSION_DEFINES_ARRAY_SIZE,
      });

      P_IMMD(p, NV9097, SET_ZT_LAYER, base_array_layer);

      P_IMMD(p, NV9097, SET_Z_COMPRESSION, nil_image.compressed);

      if (nvk_cmd_buffer_3d_cls(cmd) >= MAXWELL_B) {
         P_IMMD(p, NVC597, SET_ZT_SPARSE, {
            .enable = ENABLE_FALSE,
         });
      }
   } else {
      P_IMMD(p, NV9097, SET_ZT_SELECT, 0 /* target_count */);
   }

   /* From the Vulkan 1.3.275 spec:
    *
    *    "It is legal for a subpass to use no color or depth/stencil
    *    attachments, either because it has no attachment references or
    *    because all of them are VK_ATTACHMENT_UNUSED. This kind of subpass
    *    can use shader side effects such as image stores and atomics to
    *    produce an output. In this case, the subpass continues to use the
    *    width, height, and layers of the framebuffer to define the dimensions
    *    of the rendering area, and the rasterizationSamples from each
    *    pipeline’s VkPipelineMultisampleStateCreateInfo to define the number
    *    of samples used in rasterization;"
    *
    * In the case where we have attachments, we emit SET_ANTI_ALIAS here
    * because SET_COLOR_TARGET_* and SET_ZT_* don't have any other way of
    * specifying the sample layout and we want to ensure it matches.  When
    * we don't have any attachments, we defer SET_ANTI_ALIAS to draw time
    * where we base it on dynamic rasterizationSamples.
    */
   if (sample_layout != NIL_SAMPLE_LAYOUT_INVALID) {
      P_IMMD(p, NV9097, SET_ANTI_ALIAS,
             nil_to_nv9097_samples_mode(sample_layout));
   }

   if (render->flags & VK_RENDERING_RESUMING_BIT)
      return;

   uint32_t clear_count = 0;
   VkClearAttachment clear_att[NVK_MAX_RTS + 1];
   for (uint32_t i = 0; i < pRenderingInfo->colorAttachmentCount; i++) {
      const VkRenderingAttachmentInfo *att_info =
         &pRenderingInfo->pColorAttachments[i];
      if (att_info->imageView == VK_NULL_HANDLE ||
          att_info->loadOp != VK_ATTACHMENT_LOAD_OP_CLEAR)
         continue;

      clear_att[clear_count++] = (VkClearAttachment) {
         .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
         .colorAttachment = i,
         .clearValue = att_info->clearValue,
      };
   }

   clear_att[clear_count] = (VkClearAttachment) { .aspectMask = 0, };
   if (pRenderingInfo->pDepthAttachment != NULL &&
       pRenderingInfo->pDepthAttachment->imageView != VK_NULL_HANDLE &&
       pRenderingInfo->pDepthAttachment->loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) {
      clear_att[clear_count].aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
      clear_att[clear_count].clearValue.depthStencil.depth =
         pRenderingInfo->pDepthAttachment->clearValue.depthStencil.depth;
   }
   if (pRenderingInfo->pStencilAttachment != NULL &&
       pRenderingInfo->pStencilAttachment->imageView != VK_NULL_HANDLE &&
       pRenderingInfo->pStencilAttachment->loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) {
      clear_att[clear_count].aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
      clear_att[clear_count].clearValue.depthStencil.stencil =
         pRenderingInfo->pStencilAttachment->clearValue.depthStencil.stencil;
   }
   if (clear_att[clear_count].aspectMask != 0)
      clear_count++;

   if (clear_count > 0) {
      const VkClearRect clear_rect = {
         .rect = render->area,
         .baseArrayLayer = 0,
         .layerCount = render->view_mask ? 1 : render->layer_count,
      };

      P_MTHD(p, NV9097, SET_RENDER_ENABLE_OVERRIDE);
      P_NV9097_SET_RENDER_ENABLE_OVERRIDE(p, MODE_ALWAYS_RENDER);

      nvk_CmdClearAttachments(nvk_cmd_buffer_to_handle(cmd),
                              clear_count, clear_att, 1, &clear_rect);
      p = nvk_cmd_buffer_push(cmd, 2);
      P_MTHD(p, NV9097, SET_RENDER_ENABLE_OVERRIDE);
      P_NV9097_SET_RENDER_ENABLE_OVERRIDE(p, MODE_USE_RENDER_ENABLE);
   }

   /* TODO: Attachment clears */
}

VKAPI_ATTR void VKAPI_CALL
nvk_CmdEndRendering(VkCommandBuffer commandBuffer)
{
   VK_FROM_HANDLE(nvk_cmd_buffer, cmd, commandBuffer);
   struct nvk_rendering_state *render = &cmd->state.gfx.render;

   const bool all_linear = nvk_rendering_all_linear(render);
   for (uint32_t i = 0; i < render->color_att_count; i++) {
      struct nvk_image_view *iview = render->color_att[i].iview;
      if (iview == NULL)
         continue;

      struct nvk_image *image = (struct nvk_image *)iview->vk.image;
      const uint8_t ip = iview->planes[0].image_plane;
      const struct nvk_image_plane *plane = &image->planes[ip];
      if (!all_linear && !plane->nil.levels[0].tiling.is_tiled &&
          render->color_att[i].store_op == VK_ATTACHMENT_STORE_OP_STORE)
         nvk_linear_render_copy(cmd, iview, render->area, false);
   }

   bool need_resolve = false;

   /* Translate render state back to VK for meta */
   VkRenderingAttachmentInfo vk_color_att[NVK_MAX_RTS];
   for (uint32_t i = 0; i < render->color_att_count; i++) {
      if (render->color_att[i].resolve_mode != VK_RESOLVE_MODE_NONE)
         need_resolve = true;

      vk_color_att[i] = (VkRenderingAttachmentInfo) {
         .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
         .imageView = nvk_image_view_to_handle(render->color_att[i].iview),
         .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
         .resolveMode = render->color_att[i].resolve_mode,
         .resolveImageView =
            nvk_image_view_to_handle(render->color_att[i].resolve_iview),
         .resolveImageLayout = VK_IMAGE_LAYOUT_GENERAL,
      };
   }

   const VkRenderingAttachmentInfo vk_depth_att = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = nvk_image_view_to_handle(render->depth_att.iview),
      .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
      .resolveMode = render->depth_att.resolve_mode,
      .resolveImageView =
         nvk_image_view_to_handle(render->depth_att.resolve_iview),
      .resolveImageLayout = VK_IMAGE_LAYOUT_GENERAL,
   };
   if (render->depth_att.resolve_mode != VK_RESOLVE_MODE_NONE)
      need_resolve = true;

   const VkRenderingAttachmentInfo vk_stencil_att = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = nvk_image_view_to_handle(render->stencil_att.iview),
      .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
      .resolveMode = render->stencil_att.resolve_mode,
      .resolveImageView =
         nvk_image_view_to_handle(render->stencil_att.resolve_iview),
      .resolveImageLayout = VK_IMAGE_LAYOUT_GENERAL,
   };
   if (render->stencil_att.resolve_mode != VK_RESOLVE_MODE_NONE)
      need_resolve = true;

   const VkRenderingInfo vk_render = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea = render->area,
      .layerCount = render->layer_count,
      .viewMask = render->view_mask,
      .colorAttachmentCount = render->color_att_count,
      .pColorAttachments = vk_color_att,
      .pDepthAttachment = &vk_depth_att,
      .pStencilAttachment = &vk_stencil_att,
   };

   if (render->flags & VK_RENDERING_SUSPENDING_BIT)
      need_resolve = false;

   memset(render, 0, sizeof(*render));

   if (need_resolve) {
      struct nv_push *p = nvk_cmd_buffer_push(cmd, 2);
      P_IMMD(p, NVA097, INVALIDATE_TEXTURE_DATA_CACHE, {
         .lines = LINES_ALL,
      });

      nvk_meta_resolve_rendering(cmd, &vk_render);
   }
}

void
nvk_cmd_bind_graphics_shader(struct nvk_cmd_buffer *cmd,
                             const gl_shader_stage stage,
                             struct nvk_shader *shader)
{
   struct vk_dynamic_graphics_state *dyn = &cmd->vk.dynamic_graphics_state;

   assert(stage < ARRAY_SIZE(cmd->state.gfx.shaders));
   if (cmd->state.gfx.shaders[stage] == shader)
      return;

   cmd->state.gfx.shaders[stage] = shader;
   cmd->state.gfx.shaders_dirty |= BITFIELD_BIT(stage);

   /* When a pipeline with tess shaders is bound we need to re-upload the
    * tessellation parameters at flush_ts_state, as the domain origin can be
    * dynamic.
    */
   if (stage == MESA_SHADER_TESS_EVAL)
      BITSET_SET(dyn->dirty, MESA_VK_DYNAMIC_TS_DOMAIN_ORIGIN);

   /* Emitting SET_HYBRID_ANTI_ALIAS_CONTROL requires the fragment shader */
   if (stage == MESA_SHADER_FRAGMENT)
      BITSET_SET(dyn->dirty, MESA_VK_DYNAMIC_MS_RASTERIZATION_SAMPLES);
}

static uint32_t
mesa_to_nv9097_shader_type(gl_shader_stage stage)
{
   static const uint32_t mesa_to_nv9097[] = {
      [MESA_SHADER_VERTEX]    = NV9097_SET_PIPELINE_SHADER_TYPE_VERTEX,
      [MESA_SHADER_TESS_CTRL] = NV9097_SET_PIPELINE_SHADER_TYPE_TESSELLATION_INIT,
      [MESA_SHADER_TESS_EVAL] = NV9097_SET_PIPELINE_SHADER_TYPE_TESSELLATION,
      [MESA_SHADER_GEOMETRY]  = NV9097_SET_PIPELINE_SHADER_TYPE_GEOMETRY,
      [MESA_SHADER_FRAGMENT]  = NV9097_SET_PIPELINE_SHADER_TYPE_PIXEL,
   };
   assert(stage < ARRAY_SIZE(mesa_to_nv9097));
   return mesa_to_nv9097[stage];
}

static uint32_t
nvk_pipeline_bind_group(gl_shader_stage stage)
{
   return stage;
}

static void
nvk_flush_shaders(struct nvk_cmd_buffer *cmd)
{
   if (cmd->state.gfx.shaders_dirty == 0)
      return;

   /* Map shader types to shaders */
   struct nvk_shader *type_shader[6] = { NULL, };
   uint32_t types_dirty = 0;

   const uint32_t gfx_stages = BITFIELD_BIT(MESA_SHADER_VERTEX) |
                               BITFIELD_BIT(MESA_SHADER_TESS_CTRL) |
                               BITFIELD_BIT(MESA_SHADER_TESS_EVAL) |
                               BITFIELD_BIT(MESA_SHADER_GEOMETRY) |
                               BITFIELD_BIT(MESA_SHADER_FRAGMENT);

   u_foreach_bit(stage, cmd->state.gfx.shaders_dirty & gfx_stages) {
      uint32_t type = mesa_to_nv9097_shader_type(stage);
      types_dirty |= BITFIELD_BIT(type);

      /* Only copy non-NULL shaders because mesh/task alias with vertex and
       * tessellation stages.
       */
      if (cmd->state.gfx.shaders[stage] != NULL) {
         assert(type < ARRAY_SIZE(type_shader));
         assert(type_shader[type] == NULL);
         type_shader[type] = cmd->state.gfx.shaders[stage];
      }
   }

   u_foreach_bit(type, types_dirty) {
      struct nvk_shader *shader = type_shader[type];

      /* We always map index == type */
      const uint32_t idx = type;

      struct nv_push *p = nvk_cmd_buffer_push(cmd, 8);
      P_IMMD(p, NV9097, SET_PIPELINE_SHADER(idx), {
         .enable  = shader != NULL,
         .type    = type,
      });

      if (shader == NULL)
         continue;

      uint64_t addr = shader->hdr_addr;
      if (nvk_cmd_buffer_3d_cls(cmd) >= VOLTA_A) {
         P_MTHD(p, NVC397, SET_PIPELINE_PROGRAM_ADDRESS_A(idx));
         P_NVC397_SET_PIPELINE_PROGRAM_ADDRESS_A(p, idx, addr >> 32);
         P_NVC397_SET_PIPELINE_PROGRAM_ADDRESS_B(p, idx, addr);
      } else {
         assert(addr < 0xffffffff);
         P_IMMD(p, NV9097, SET_PIPELINE_PROGRAM(idx), addr);
      }

      P_MTHD(p, NVC397, SET_PIPELINE_REGISTER_COUNT(idx));
      P_NVC397_SET_PIPELINE_REGISTER_COUNT(p, idx, shader->info.num_gprs);
      P_NVC397_SET_PIPELINE_BINDING(p, idx,
         nvk_pipeline_bind_group(shader->info.stage));

      if (shader->info.stage == MESA_SHADER_FRAGMENT) {
         p = nvk_cmd_buffer_push(cmd, 9);

         P_MTHD(p, NVC397, SET_SUBTILING_PERF_KNOB_A);
         P_NV9097_SET_SUBTILING_PERF_KNOB_A(p, {
            .fraction_of_spm_register_file_per_subtile         = 0x10,
            .fraction_of_spm_pixel_output_buffer_per_subtile   = 0x40,
            .fraction_of_spm_triangle_ram_per_subtile          = 0x16,
            .fraction_of_max_quads_per_subtile                 = 0x20,
         });
         P_NV9097_SET_SUBTILING_PERF_KNOB_B(p, 0x20);

         P_IMMD(p, NV9097, SET_API_MANDATED_EARLY_Z,
                shader->info.fs.early_fragment_tests);

         if (nvk_cmd_buffer_3d_cls(cmd) >= MAXWELL_B) {
            P_IMMD(p, NVB197, SET_POST_Z_PS_IMASK,
                   shader->info.fs.post_depth_coverage);
         } else {
            assert(!shader->info.fs.post_depth_coverage);
         }

         P_IMMD(p, NV9097, SET_ZCULL_BOUNDS, {
            .z_min_unbounded_enable = shader->info.fs.writes_depth,
            .z_max_unbounded_enable = shader->info.fs.writes_depth,
         });
      }
   }

   const uint32_t vtg_stages = BITFIELD_BIT(MESA_SHADER_VERTEX) |
                               BITFIELD_BIT(MESA_SHADER_TESS_EVAL) |
                               BITFIELD_BIT(MESA_SHADER_GEOMETRY);
   const uint32_t vtgm_stages = vtg_stages | BITFIELD_BIT(MESA_SHADER_MESH);

   if (cmd->state.gfx.shaders_dirty & vtg_stages) {
      struct nak_xfb_info *xfb = NULL;
      u_foreach_bit(stage, vtg_stages) {
         if (cmd->state.gfx.shaders[stage] != NULL)
            xfb = &cmd->state.gfx.shaders[stage]->info.vtg.xfb;
      }

      if (xfb == NULL) {
         struct nv_push *p = nvk_cmd_buffer_push(cmd, 8);
         for (uint8_t b = 0; b < 4; b++)
            P_IMMD(p, NV9097, SET_STREAM_OUT_CONTROL_COMPONENT_COUNT(b), 0);
      } else {
         for (uint8_t b = 0; b < ARRAY_SIZE(xfb->attr_count); b++) {
            const uint8_t attr_count = xfb->attr_count[b];
            /* upload packed varying indices in multiples of 4 bytes */
            const uint32_t n = DIV_ROUND_UP(attr_count, 4);

            struct nv_push *p = nvk_cmd_buffer_push(cmd, 5 + n);

            P_MTHD(p, NV9097, SET_STREAM_OUT_CONTROL_STREAM(b));
            P_NV9097_SET_STREAM_OUT_CONTROL_STREAM(p, b, xfb->stream[b]);
            P_NV9097_SET_STREAM_OUT_CONTROL_COMPONENT_COUNT(p, b, attr_count);
            P_NV9097_SET_STREAM_OUT_CONTROL_STRIDE(p, b, xfb->stride[b]);

            if (n > 0) {
               P_MTHD(p, NV9097, SET_STREAM_OUT_LAYOUT_SELECT(b, 0));
               P_INLINE_ARRAY(p, (const uint32_t*)xfb->attr_index[b], n);
            }
         }
      }
   }

   if (cmd->state.gfx.shaders_dirty & vtgm_stages) {
      struct nvk_shader *last_vtgm = NULL;
      u_foreach_bit(stage, vtgm_stages) {
         if (cmd->state.gfx.shaders[stage] != NULL)
            last_vtgm = cmd->state.gfx.shaders[stage];
      }

      struct nv_push *p = nvk_cmd_buffer_push(cmd, 6);

      P_IMMD(p, NV9097, SET_RT_LAYER, {
         .v       = 0,
         .control = last_vtgm->info.vtg.writes_layer ?
                    CONTROL_GEOMETRY_SHADER_SELECTS_LAYER :
                    CONTROL_V_SELECTS_LAYER,
      });

      const uint8_t clip_enable = last_vtgm->info.vtg.clip_enable;
      const uint8_t cull_enable = last_vtgm->info.vtg.cull_enable;
      P_IMMD(p, NV9097, SET_USER_CLIP_ENABLE, {
         .plane0 = ((clip_enable | cull_enable) >> 0) & 1,
         .plane1 = ((clip_enable | cull_enable) >> 1) & 1,
         .plane2 = ((clip_enable | cull_enable) >> 2) & 1,
         .plane3 = ((clip_enable | cull_enable) >> 3) & 1,
         .plane4 = ((clip_enable | cull_enable) >> 4) & 1,
         .plane5 = ((clip_enable | cull_enable) >> 5) & 1,
         .plane6 = ((clip_enable | cull_enable) >> 6) & 1,
         .plane7 = ((clip_enable | cull_enable) >> 7) & 1,
      });
      P_IMMD(p, NV9097, SET_USER_CLIP_OP, {
         .plane0 = (cull_enable >> 0) & 1,
         .plane1 = (cull_enable >> 1) & 1,
         .plane2 = (cull_enable >> 2) & 1,
         .plane3 = (cull_enable >> 3) & 1,
         .plane4 = (cull_enable >> 4) & 1,
         .plane5 = (cull_enable >> 5) & 1,
         .plane6 = (cull_enable >> 6) & 1,
         .plane7 = (cull_enable >> 7) & 1,
      });
   }

   cmd->state.gfx.shaders_dirty = 0;
}

static void
nvk_flush_vi_state(struct nvk_cmd_buffer *cmd)
{
   struct nvk_device *dev = nvk_cmd_buffer_device(cmd);
   struct nvk_physical_device *pdev = nvk_device_physical(dev);
   const struct vk_dynamic_graphics_state *dyn =
      &cmd->vk.dynamic_graphics_state;

   struct nv_push *p = nvk_cmd_buffer_push(cmd, 256);

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_VI) ||
       BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_VI_BINDINGS_VALID)) {
      u_foreach_bit(a, dyn->vi->attributes_valid) {
         const struct nvk_va_format *fmt =
            nvk_get_va_format(pdev, dyn->vi->attributes[a].format);

         P_IMMD(p, NV9097, SET_VERTEX_ATTRIBUTE_A(a), {
            .stream                 = dyn->vi->attributes[a].binding,
            .offset                 = dyn->vi->attributes[a].offset,
            .component_bit_widths   = fmt->bit_widths,
            .numerical_type         = fmt->type,
            .swap_r_and_b           = fmt->swap_rb,
         });
      }

      u_foreach_bit(b, dyn->vi->bindings_valid) {
         const bool instanced = dyn->vi->bindings[b].input_rate ==
                                VK_VERTEX_INPUT_RATE_INSTANCE;
         P_IMMD(p, NV9097, SET_VERTEX_STREAM_INSTANCE_A(b), instanced);
         P_IMMD(p, NV9097, SET_VERTEX_STREAM_A_FREQUENCY(b),
            dyn->vi->bindings[b].divisor);
      }
   }

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_VI_BINDINGS_VALID) ||
       BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_VI_BINDING_STRIDES)) {
      for (uint32_t b = 0; b < 32; b++) {
         P_IMMD(p, NV9097, SET_VERTEX_STREAM_A_FORMAT(b), {
            .stride = dyn->vi_binding_strides[b],
            .enable = (dyn->vi->bindings_valid & BITFIELD_BIT(b)) != 0,
         });
      }
   }
}

static void
nvk_flush_ia_state(struct nvk_cmd_buffer *cmd)
{
   const struct vk_dynamic_graphics_state *dyn =
      &cmd->vk.dynamic_graphics_state;

   /** Nothing to do for MESA_VK_DYNAMIC_IA_PRIMITIVE_TOPOLOGY */

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_IA_PRIMITIVE_RESTART_ENABLE)) {
      struct nv_push *p = nvk_cmd_buffer_push(cmd, 2);
      P_IMMD(p, NV9097, SET_DA_PRIMITIVE_RESTART,
             dyn->ia.primitive_restart_enable);
   }
}

static void
nvk_flush_ts_state(struct nvk_cmd_buffer *cmd)
{
   const struct vk_dynamic_graphics_state *dyn =
      &cmd->vk.dynamic_graphics_state;
   struct nv_push *p = nvk_cmd_buffer_push(cmd, 4);

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_TS_PATCH_CONTROL_POINTS)) {
      /* The hardware gets grumpy if we set this to 0 so make sure we set it
       * to at least 1 in case it's dirty but uninitialized.
       */
      P_IMMD(p, NV9097, SET_PATCH, MAX2(1, dyn->ts.patch_control_points));
   }

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_TS_DOMAIN_ORIGIN)) {
      const struct nvk_shader *shader =
         cmd->state.gfx.shaders[MESA_SHADER_TESS_EVAL];

      if (shader != NULL) {
         enum nak_ts_prims prims = shader->info.ts.prims;
         /* When the origin is lower-left, we have to flip the winding order */
         if (dyn->ts.domain_origin == VK_TESSELLATION_DOMAIN_ORIGIN_LOWER_LEFT) {
            if (prims == NAK_TS_PRIMS_TRIANGLES_CW)
               prims = NAK_TS_PRIMS_TRIANGLES_CCW;
            else if (prims == NAK_TS_PRIMS_TRIANGLES_CCW)
               prims = NAK_TS_PRIMS_TRIANGLES_CW;
         }
         P_MTHD(p, NV9097, SET_TESSELLATION_PARAMETERS);
         P_NV9097_SET_TESSELLATION_PARAMETERS(p, {
            shader->info.ts.domain,
            shader->info.ts.spacing,
            prims
         });
      }
   }
}

static void
nvk_flush_vp_state(struct nvk_cmd_buffer *cmd)
{
   const struct vk_dynamic_graphics_state *dyn =
      &cmd->vk.dynamic_graphics_state;

   struct nv_push *p =
      nvk_cmd_buffer_push(cmd, 16 * dyn->vp.viewport_count + 4 * NVK_MAX_VIEWPORTS);

   /* Nothing to do for MESA_VK_DYNAMIC_VP_VIEWPORT_COUNT */

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_VP_VIEWPORTS) ||
       BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_VP_DEPTH_CLIP_NEGATIVE_ONE_TO_ONE)) {
      for (uint32_t i = 0; i < dyn->vp.viewport_count; i++) {
         const VkViewport *vp = &dyn->vp.viewports[i];

         /* These exactly match the spec values.  Nvidia hardware oddities
          * are accounted for later.
          */
         const float o_x = vp->x + 0.5f * vp->width;
         const float o_y = vp->y + 0.5f * vp->height;
         const float o_z = !dyn->vp.depth_clip_negative_one_to_one ?
                           vp->minDepth :
                           (vp->maxDepth + vp->minDepth) * 0.5f;

         const float p_x = vp->width;
         const float p_y = vp->height;
         const float p_z = !dyn->vp.depth_clip_negative_one_to_one ?
                           vp->maxDepth - vp->minDepth :
                           (vp->maxDepth - vp->minDepth) * 0.5f;

         P_MTHD(p, NV9097, SET_VIEWPORT_SCALE_X(i));
         P_NV9097_SET_VIEWPORT_SCALE_X(p, i, fui(0.5f * p_x));
         P_NV9097_SET_VIEWPORT_SCALE_Y(p, i, fui(0.5f * p_y));
         P_NV9097_SET_VIEWPORT_SCALE_Z(p, i, fui(p_z));

         P_NV9097_SET_VIEWPORT_OFFSET_X(p, i, fui(o_x));
         P_NV9097_SET_VIEWPORT_OFFSET_Y(p, i, fui(o_y));
         P_NV9097_SET_VIEWPORT_OFFSET_Z(p, i, fui(o_z));

         float xmin = vp->x;
         float xmax = vp->x + vp->width;
         float ymin = MIN2(vp->y, vp->y + vp->height);
         float ymax = MAX2(vp->y, vp->y + vp->height);
         float zmin = MIN2(vp->minDepth, vp->maxDepth);
         float zmax = MAX2(vp->minDepth, vp->maxDepth);
         assert(xmin <= xmax && ymin <= ymax);

         const float max_dim = (float)0xffff;
         xmin = CLAMP(xmin, 0, max_dim);
         xmax = CLAMP(xmax, 0, max_dim);
         ymin = CLAMP(ymin, 0, max_dim);
         ymax = CLAMP(ymax, 0, max_dim);

         P_MTHD(p, NV9097, SET_VIEWPORT_CLIP_HORIZONTAL(i));
         P_NV9097_SET_VIEWPORT_CLIP_HORIZONTAL(p, i, {
            .x0      = xmin,
            .width   = xmax - xmin,
         });
         P_NV9097_SET_VIEWPORT_CLIP_VERTICAL(p, i, {
            .y0      = ymin,
            .height  = ymax - ymin,
         });
         P_NV9097_SET_VIEWPORT_CLIP_MIN_Z(p, i, fui(zmin));
         P_NV9097_SET_VIEWPORT_CLIP_MAX_Z(p, i, fui(zmax));

         if (nvk_cmd_buffer_3d_cls(cmd) >= MAXWELL_B) {
            P_IMMD(p, NVB197, SET_VIEWPORT_COORDINATE_SWIZZLE(i), {
               .x = X_POS_X,
               .y = Y_POS_Y,
               .z = Z_POS_Z,
               .w = W_POS_W,
            });
         }
      }
   }

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_VP_DEPTH_CLIP_NEGATIVE_ONE_TO_ONE)) {
      P_IMMD(p, NV9097, SET_VIEWPORT_Z_CLIP,
             dyn->vp.depth_clip_negative_one_to_one ?
             RANGE_NEGATIVE_W_TO_POSITIVE_W :
             RANGE_ZERO_TO_POSITIVE_W);
   }

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_VP_SCISSOR_COUNT)) {
      for (unsigned i = dyn->vp.scissor_count; i < NVK_MAX_VIEWPORTS; i++)
         P_IMMD(p, NV9097, SET_SCISSOR_ENABLE(i), V_FALSE);
   }

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_VP_SCISSORS)) {
      for (unsigned i = 0; i < dyn->vp.scissor_count; i++) {
         const VkRect2D *s = &dyn->vp.scissors[i];

         const uint32_t xmin = MIN2(16384, s->offset.x);
         const uint32_t xmax = MIN2(16384, s->offset.x + s->extent.width);
         const uint32_t ymin = MIN2(16384, s->offset.y);
         const uint32_t ymax = MIN2(16384, s->offset.y + s->extent.height);

         P_MTHD(p, NV9097, SET_SCISSOR_ENABLE(i));
         P_NV9097_SET_SCISSOR_ENABLE(p, i, V_TRUE);
         P_NV9097_SET_SCISSOR_HORIZONTAL(p, i, {
            .xmin = xmin,
            .xmax = xmax,
         });
         P_NV9097_SET_SCISSOR_VERTICAL(p, i, {
            .ymin = ymin,
            .ymax = ymax,
         });
      }
   }
}

static uint32_t
vk_to_nv9097_polygon_mode(VkPolygonMode vk_mode)
{
   ASSERTED uint16_t vk_to_nv9097[] = {
      [VK_POLYGON_MODE_FILL]  = NV9097_SET_FRONT_POLYGON_MODE_V_FILL,
      [VK_POLYGON_MODE_LINE]  = NV9097_SET_FRONT_POLYGON_MODE_V_LINE,
      [VK_POLYGON_MODE_POINT] = NV9097_SET_FRONT_POLYGON_MODE_V_POINT,
   };
   assert(vk_mode < ARRAY_SIZE(vk_to_nv9097));

   uint32_t nv9097_mode = 0x1b00 | (2 - vk_mode);
   assert(nv9097_mode == vk_to_nv9097[vk_mode]);
   return nv9097_mode;
}

static uint32_t
vk_to_nv9097_cull_mode(VkCullModeFlags vk_cull_mode)
{
   static const uint16_t vk_to_nv9097[] = {
      [VK_CULL_MODE_FRONT_BIT]      = NV9097_OGL_SET_CULL_FACE_V_FRONT,
      [VK_CULL_MODE_BACK_BIT]       = NV9097_OGL_SET_CULL_FACE_V_BACK,
      [VK_CULL_MODE_FRONT_AND_BACK] = NV9097_OGL_SET_CULL_FACE_V_FRONT_AND_BACK,
   };
   assert(vk_cull_mode < ARRAY_SIZE(vk_to_nv9097));
   return vk_to_nv9097[vk_cull_mode];
}

static uint32_t
vk_to_nv9097_front_face(VkFrontFace vk_face)
{
   /* Vulkan and OpenGL are backwards here because Vulkan assumes the D3D
    * convention in which framebuffer coordinates always start in the upper
    * left while OpenGL has framebuffer coordinates starting in the lower
    * left.  Therefore, we want the reverse of the hardware enum name.
    */
   ASSERTED static const uint16_t vk_to_nv9097[] = {
      [VK_FRONT_FACE_COUNTER_CLOCKWISE]   = NV9097_OGL_SET_FRONT_FACE_V_CCW,
      [VK_FRONT_FACE_CLOCKWISE]           = NV9097_OGL_SET_FRONT_FACE_V_CW,
   };
   assert(vk_face < ARRAY_SIZE(vk_to_nv9097));

   uint32_t nv9097_face = 0x900 | (1 - vk_face);
   assert(nv9097_face == vk_to_nv9097[vk_face]);
   return nv9097_face;
}

static uint32_t
vk_to_nv9097_provoking_vertex(VkProvokingVertexModeEXT vk_mode)
{
   STATIC_ASSERT(VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT ==
                 NV9097_SET_PROVOKING_VERTEX_V_FIRST);
   STATIC_ASSERT(VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT ==
                 NV9097_SET_PROVOKING_VERTEX_V_LAST);
   return vk_mode;
}

static void
nvk_flush_rs_state(struct nvk_cmd_buffer *cmd)
{
   struct nv_push *p = nvk_cmd_buffer_push(cmd, 40);

   const struct vk_dynamic_graphics_state *dyn =
      &cmd->vk.dynamic_graphics_state;

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_RS_RASTERIZER_DISCARD_ENABLE))
      P_IMMD(p, NV9097, SET_RASTER_ENABLE, !dyn->rs.rasterizer_discard_enable);

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_RS_DEPTH_CLIP_ENABLE) ||
       BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_RS_DEPTH_CLAMP_ENABLE)) {
      const bool z_clamp = dyn->rs.depth_clamp_enable;
      const bool z_clip = vk_rasterization_state_depth_clip_enable(&dyn->rs);
      P_IMMD(p, NVC397, SET_VIEWPORT_CLIP_CONTROL, {
         /* TODO: Fix pre-Volta
          *
          * This probably involves a few macros, one which stases viewport
          * min/maxDepth in scratch states and one which goes here and
          * emits either min/maxDepth or -/+INF as needed.
          */
         .min_z_zero_max_z_one = MIN_Z_ZERO_MAX_Z_ONE_FALSE,
         .z_clip_range = nvk_cmd_buffer_3d_cls(cmd) >= VOLTA_A
                         ? ((z_clamp || z_clip)
                            ? Z_CLIP_RANGE_MIN_Z_MAX_Z
                            : Z_CLIP_RANGE_MINUS_INF_PLUS_INF)
                         : Z_CLIP_RANGE_USE_FIELD_MIN_Z_ZERO_MAX_Z_ONE,

         .pixel_min_z = PIXEL_MIN_Z_CLAMP,
         .pixel_max_z = PIXEL_MAX_Z_CLAMP,

         .geometry_guardband = GEOMETRY_GUARDBAND_SCALE_256,
         .line_point_cull_guardband = LINE_POINT_CULL_GUARDBAND_SCALE_256,
         .geometry_clip = z_clip ? GEOMETRY_CLIP_FRUSTUM_XYZ_CLIP
                                 : GEOMETRY_CLIP_FRUSTUM_XY_CLIP,

         /* We clip depth with the geometry clipper to ensure that it gets
          * clipped before depth bias is applied.  If we leave it up to the
          * raserizer clipper (pixel_min/max_z = CLIP), it will clip according
          * to the post-bias Z value which is wrong.  In order to always get
          * the geometry clipper, we need to set a tignt guardband
          * (geometry_guardband_z = SCALE_1).
          */
         .geometry_guardband_z = z_clip ? GEOMETRY_GUARDBAND_Z_SCALE_1
                                        : GEOMETRY_GUARDBAND_Z_SCALE_256,
      });
   }

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_RS_POLYGON_MODE)) {
      uint32_t polygon_mode = vk_to_nv9097_polygon_mode(dyn->rs.polygon_mode);
      P_MTHD(p, NV9097, SET_FRONT_POLYGON_MODE);
      P_NV9097_SET_FRONT_POLYGON_MODE(p, polygon_mode);
      P_NV9097_SET_BACK_POLYGON_MODE(p, polygon_mode);
   }

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_RS_CULL_MODE)) {
      P_IMMD(p, NV9097, OGL_SET_CULL, dyn->rs.cull_mode != VK_CULL_MODE_NONE);

      if (dyn->rs.cull_mode != VK_CULL_MODE_NONE) {
         uint32_t face = vk_to_nv9097_cull_mode(dyn->rs.cull_mode);
         P_IMMD(p, NV9097, OGL_SET_CULL_FACE, face);
      }
   }

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_RS_FRONT_FACE)) {
      P_IMMD(p, NV9097, OGL_SET_FRONT_FACE,
         vk_to_nv9097_front_face(dyn->rs.front_face));
   }

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_RS_PROVOKING_VERTEX)) {
      P_IMMD(p, NV9097, SET_PROVOKING_VERTEX,
             vk_to_nv9097_provoking_vertex(dyn->rs.provoking_vertex));
   }

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_RS_DEPTH_BIAS_ENABLE)) {
      P_MTHD(p, NV9097, SET_POLY_OFFSET_POINT);
      P_NV9097_SET_POLY_OFFSET_POINT(p, dyn->rs.depth_bias.enable);
      P_NV9097_SET_POLY_OFFSET_LINE(p, dyn->rs.depth_bias.enable);
      P_NV9097_SET_POLY_OFFSET_FILL(p, dyn->rs.depth_bias.enable);
   }

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_RS_DEPTH_BIAS_FACTORS)) {
      switch (dyn->rs.depth_bias.representation) {
      case VK_DEPTH_BIAS_REPRESENTATION_LEAST_REPRESENTABLE_VALUE_FORMAT_EXT:
         P_IMMD(p, NV9097, SET_DEPTH_BIAS_CONTROL,
                DEPTH_FORMAT_DEPENDENT_TRUE);
         break;
      case VK_DEPTH_BIAS_REPRESENTATION_LEAST_REPRESENTABLE_VALUE_FORCE_UNORM_EXT:
         P_IMMD(p, NV9097, SET_DEPTH_BIAS_CONTROL,
                DEPTH_FORMAT_DEPENDENT_FALSE);
         break;
      case VK_DEPTH_BIAS_REPRESENTATION_FLOAT_EXT:
      default:
         unreachable("Unsupported depth bias representation");
      }
      /* TODO: The blob multiplies by 2 for some reason. We don't. */
      P_IMMD(p, NV9097, SET_DEPTH_BIAS, fui(dyn->rs.depth_bias.constant));
      P_IMMD(p, NV9097, SET_SLOPE_SCALE_DEPTH_BIAS, fui(dyn->rs.depth_bias.slope));
      P_IMMD(p, NV9097, SET_DEPTH_BIAS_CLAMP, fui(dyn->rs.depth_bias.clamp));
   }

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_RS_LINE_WIDTH)) {
      P_MTHD(p, NV9097, SET_LINE_WIDTH_FLOAT);
      P_NV9097_SET_LINE_WIDTH_FLOAT(p, fui(dyn->rs.line.width));
      P_NV9097_SET_ALIASED_LINE_WIDTH_FLOAT(p, fui(dyn->rs.line.width));
   }

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_RS_LINE_MODE)) {
      switch (dyn->rs.line.mode) {
      case VK_LINE_RASTERIZATION_MODE_DEFAULT_KHR:
      case VK_LINE_RASTERIZATION_MODE_RECTANGULAR_KHR:
         P_IMMD(p, NV9097, SET_LINE_MULTISAMPLE_OVERRIDE, ENABLE_FALSE);
         P_IMMD(p, NV9097, SET_ANTI_ALIASED_LINE, ENABLE_FALSE);
         break;

      case VK_LINE_RASTERIZATION_MODE_BRESENHAM_KHR:
         P_IMMD(p, NV9097, SET_LINE_MULTISAMPLE_OVERRIDE, ENABLE_TRUE);
         P_IMMD(p, NV9097, SET_ANTI_ALIASED_LINE, ENABLE_FALSE);
         break;

      case VK_LINE_RASTERIZATION_MODE_RECTANGULAR_SMOOTH_KHR:
         P_IMMD(p, NV9097, SET_LINE_MULTISAMPLE_OVERRIDE, ENABLE_TRUE);
         P_IMMD(p, NV9097, SET_ANTI_ALIASED_LINE, ENABLE_TRUE);
         break;

      default:
         unreachable("Invalid line rasterization mode");
      }
   }

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_RS_LINE_STIPPLE_ENABLE))
      P_IMMD(p, NV9097, SET_LINE_STIPPLE, dyn->rs.line.stipple.enable);

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_RS_LINE_STIPPLE)) {
      /* map factor from [1,256] to [0, 255] */
      uint32_t stipple_factor = CLAMP(dyn->rs.line.stipple.factor, 1, 256) - 1;
      P_IMMD(p, NV9097, SET_LINE_STIPPLE_PARAMETERS, {
         .factor  = stipple_factor,
         .pattern = dyn->rs.line.stipple.pattern,
      });
   }

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_RS_RASTERIZATION_STREAM))
      P_IMMD(p, NV9097, SET_RASTER_INPUT, dyn->rs.rasterization_stream);
}

static VkSampleLocationEXT
vk_sample_location(const struct vk_sample_locations_state *sl,
                   uint32_t x, uint32_t y, uint32_t s)
{
   x = x % sl->grid_size.width;
   y = y % sl->grid_size.height;

   return sl->locations[(x + y * sl->grid_size.width) * sl->per_pixel + s];
}

static struct nvk_sample_location
vk_to_nvk_sample_location(VkSampleLocationEXT loc)
{
   return (struct nvk_sample_location) {
      .x_u4 = util_bitpack_ufixed_clamp(loc.x, 0, 3, 4),
      .y_u4 = util_bitpack_ufixed_clamp(loc.y, 0, 3, 4),
   };
}

static void
nvk_flush_ms_state(struct nvk_cmd_buffer *cmd)
{
   struct nvk_descriptor_state *desc = &cmd->state.gfx.descriptors;
   const struct nvk_rendering_state *render = &cmd->state.gfx.render;
   const struct vk_dynamic_graphics_state *dyn =
      &cmd->vk.dynamic_graphics_state;

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_MS_RASTERIZATION_SAMPLES)) {
      struct nv_push *p = nvk_cmd_buffer_push(cmd, 4);

      /* When we don't have any attachments, we can't know the sample count
       * from the render pass so we need to emit SET_ANTI_ALIAS here.  See the
       * comment in nvk_BeginRendering() for more details.
       */
      if (render->samples == 0) {
         /* Multisample information MAY be missing (rasterizationSamples == 0)
          * if rasterizer discard is enabled.  However, this isn't valid in
          * the hardware so always use at least one sample.
          */
         const uint32_t samples = MAX2(1, dyn->ms.rasterization_samples);
         enum nil_sample_layout layout = nil_choose_sample_layout(samples);
         P_IMMD(p, NV9097, SET_ANTI_ALIAS, nil_to_nv9097_samples_mode(layout));
      } else {
         /* Multisample information MAY be missing (rasterizationSamples == 0)
          * if rasterizer discard is enabled.
          */
         assert(dyn->ms.rasterization_samples == 0 ||
                dyn->ms.rasterization_samples == render->samples);
      }

      struct nvk_shader *fs = cmd->state.gfx.shaders[MESA_SHADER_FRAGMENT];
      const float min_sample_shading = fs != NULL ? fs->min_sample_shading : 0;
      uint32_t min_samples = ceilf(dyn->ms.rasterization_samples *
                                   min_sample_shading);
      min_samples = util_next_power_of_two(MAX2(1, min_samples));

      P_IMMD(p, NV9097, SET_HYBRID_ANTI_ALIAS_CONTROL, {
         .passes = min_samples,
         .centroid = min_samples > 1 ? CENTROID_PER_PASS
                                     : CENTROID_PER_FRAGMENT,
      });
   }

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_MS_ALPHA_TO_COVERAGE_ENABLE) ||
       BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_MS_ALPHA_TO_ONE_ENABLE)) {
      struct nv_push *p = nvk_cmd_buffer_push(cmd, 2);
      P_IMMD(p, NV9097, SET_ANTI_ALIAS_ALPHA_CONTROL, {
         .alpha_to_coverage = dyn->ms.alpha_to_coverage_enable,
         .alpha_to_one      = dyn->ms.alpha_to_one_enable,
      });
   }

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_MS_RASTERIZATION_SAMPLES) ||
       BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_MS_SAMPLE_LOCATIONS) ||
       BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_MS_SAMPLE_LOCATIONS_ENABLE)) {
      const struct vk_sample_locations_state *sl;
      if (dyn->ms.sample_locations_enable) {
         sl = dyn->ms.sample_locations;
      } else {
         const uint32_t samples = MAX2(1, dyn->ms.rasterization_samples);
         sl = vk_standard_sample_locations_state(samples);
      }

      for (uint32_t i = 0; i < sl->per_pixel; i++) {
         desc->root.draw.sample_locations[i] =
            vk_to_nvk_sample_location(sl->locations[i]);
      }

      if (nvk_cmd_buffer_3d_cls(cmd) >= MAXWELL_B) {
         struct nvk_sample_location loc[16];
         for (uint32_t n = 0; n < ARRAY_SIZE(loc); n++) {
            const uint32_t s = n % sl->per_pixel;
            const uint32_t px = n / sl->per_pixel;
            const uint32_t x = px % 2;
            const uint32_t y = px / 2;

            loc[n] = vk_to_nvk_sample_location(vk_sample_location(sl, x, y, s));
         }

         struct nv_push *p = nvk_cmd_buffer_push(cmd, 5);

         P_MTHD(p, NVB197, SET_ANTI_ALIAS_SAMPLE_POSITIONS(0));
         for (uint32_t i = 0; i < 4; i++) {
            P_NVB197_SET_ANTI_ALIAS_SAMPLE_POSITIONS(p, i, {
               .x0 = loc[i * 4 + 0].x_u4,
               .y0 = loc[i * 4 + 0].y_u4,
               .x1 = loc[i * 4 + 1].x_u4,
               .y1 = loc[i * 4 + 1].y_u4,
               .x2 = loc[i * 4 + 2].x_u4,
               .y2 = loc[i * 4 + 2].y_u4,
               .x3 = loc[i * 4 + 3].x_u4,
               .y3 = loc[i * 4 + 3].y_u4,
            });
         }
      }
   }

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_MS_SAMPLE_MASK)) {
      struct nv_push *p = nvk_cmd_buffer_push(cmd, 5);
      P_MTHD(p, NV9097, SET_SAMPLE_MASK_X0_Y0);
      P_NV9097_SET_SAMPLE_MASK_X0_Y0(p, dyn->ms.sample_mask & 0xffff);
      P_NV9097_SET_SAMPLE_MASK_X1_Y0(p, dyn->ms.sample_mask & 0xffff);
      P_NV9097_SET_SAMPLE_MASK_X0_Y1(p, dyn->ms.sample_mask & 0xffff);
      P_NV9097_SET_SAMPLE_MASK_X1_Y1(p, dyn->ms.sample_mask & 0xffff);
   }
}

static uint32_t
vk_to_nv9097_compare_op(VkCompareOp vk_op)
{
   ASSERTED static const uint16_t vk_to_nv9097[] = {
      [VK_COMPARE_OP_NEVER]            = NV9097_SET_DEPTH_FUNC_V_OGL_NEVER,
      [VK_COMPARE_OP_LESS]             = NV9097_SET_DEPTH_FUNC_V_OGL_LESS,
      [VK_COMPARE_OP_EQUAL]            = NV9097_SET_DEPTH_FUNC_V_OGL_EQUAL,
      [VK_COMPARE_OP_LESS_OR_EQUAL]    = NV9097_SET_DEPTH_FUNC_V_OGL_LEQUAL,
      [VK_COMPARE_OP_GREATER]          = NV9097_SET_DEPTH_FUNC_V_OGL_GREATER,
      [VK_COMPARE_OP_NOT_EQUAL]        = NV9097_SET_DEPTH_FUNC_V_OGL_NOTEQUAL,
      [VK_COMPARE_OP_GREATER_OR_EQUAL] = NV9097_SET_DEPTH_FUNC_V_OGL_GEQUAL,
      [VK_COMPARE_OP_ALWAYS]           = NV9097_SET_DEPTH_FUNC_V_OGL_ALWAYS,
   };
   assert(vk_op < ARRAY_SIZE(vk_to_nv9097));

   uint32_t nv9097_op = 0x200 | vk_op;
   assert(nv9097_op == vk_to_nv9097[vk_op]);
   return nv9097_op;
}

static uint32_t
vk_to_nv9097_stencil_op(VkStencilOp vk_op)
{
#define OP(vk, nv) [VK_STENCIL_OP_##vk] = NV9097_SET_STENCIL_OP_FAIL_V_##nv
   ASSERTED static const uint16_t vk_to_nv9097[] = {
      OP(KEEP,                D3D_KEEP),
      OP(ZERO,                D3D_ZERO),
      OP(REPLACE,             D3D_REPLACE),
      OP(INCREMENT_AND_CLAMP, D3D_INCRSAT),
      OP(DECREMENT_AND_CLAMP, D3D_DECRSAT),
      OP(INVERT,              D3D_INVERT),
      OP(INCREMENT_AND_WRAP,  D3D_INCR),
      OP(DECREMENT_AND_WRAP,  D3D_DECR),
   };
   assert(vk_op < ARRAY_SIZE(vk_to_nv9097));
#undef OP

   uint32_t nv9097_op = vk_op + 1;
   assert(nv9097_op == vk_to_nv9097[vk_op]);
   return nv9097_op;
}

static void
nvk_flush_ds_state(struct nvk_cmd_buffer *cmd)
{
   struct nv_push *p = nvk_cmd_buffer_push(cmd, 35);

   const struct nvk_rendering_state *render = &cmd->state.gfx.render;
   const struct vk_dynamic_graphics_state *dyn =
      &cmd->vk.dynamic_graphics_state;

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_DS_DEPTH_TEST_ENABLE)) {
      bool enable = dyn->ds.depth.test_enable &&
                    render->depth_att.vk_format != VK_FORMAT_UNDEFINED;
      P_IMMD(p, NV9097, SET_DEPTH_TEST, enable);
   }

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_DS_DEPTH_WRITE_ENABLE)) {
      bool enable = dyn->ds.depth.write_enable &&
                    render->depth_att.vk_format != VK_FORMAT_UNDEFINED;
      P_IMMD(p, NV9097, SET_DEPTH_WRITE, enable);
   }

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_DS_DEPTH_COMPARE_OP)) {
      const uint32_t func = vk_to_nv9097_compare_op(dyn->ds.depth.compare_op);
      P_IMMD(p, NV9097, SET_DEPTH_FUNC, func);
   }

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_DS_DEPTH_BOUNDS_TEST_ENABLE)) {
      bool enable = dyn->ds.depth.bounds_test.enable &&
                    render->depth_att.vk_format != VK_FORMAT_UNDEFINED;
      P_IMMD(p, NV9097, SET_DEPTH_BOUNDS_TEST, enable);
   }

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_DS_DEPTH_BOUNDS_TEST_BOUNDS)) {
      P_MTHD(p, NV9097, SET_DEPTH_BOUNDS_MIN);
      P_NV9097_SET_DEPTH_BOUNDS_MIN(p, fui(dyn->ds.depth.bounds_test.min));
      P_NV9097_SET_DEPTH_BOUNDS_MAX(p, fui(dyn->ds.depth.bounds_test.max));
   }

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_DS_STENCIL_TEST_ENABLE)) {
      bool enable = dyn->ds.stencil.test_enable &&
                    render->stencil_att.vk_format != VK_FORMAT_UNDEFINED;
      P_IMMD(p, NV9097, SET_STENCIL_TEST, enable);
   }

   const struct vk_stencil_test_face_state *front = &dyn->ds.stencil.front;
   const struct vk_stencil_test_face_state *back = &dyn->ds.stencil.back;
   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_DS_STENCIL_OP)) {
      P_MTHD(p, NV9097, SET_STENCIL_OP_FAIL);
      P_NV9097_SET_STENCIL_OP_FAIL(p, vk_to_nv9097_stencil_op(front->op.fail));
      P_NV9097_SET_STENCIL_OP_ZFAIL(p, vk_to_nv9097_stencil_op(front->op.depth_fail));
      P_NV9097_SET_STENCIL_OP_ZPASS(p, vk_to_nv9097_stencil_op(front->op.pass));
      P_NV9097_SET_STENCIL_FUNC(p, vk_to_nv9097_compare_op(front->op.compare));

      P_MTHD(p, NV9097, SET_BACK_STENCIL_OP_FAIL);
      P_NV9097_SET_BACK_STENCIL_OP_FAIL(p, vk_to_nv9097_stencil_op(back->op.fail));
      P_NV9097_SET_BACK_STENCIL_OP_ZFAIL(p, vk_to_nv9097_stencil_op(back->op.depth_fail));
      P_NV9097_SET_BACK_STENCIL_OP_ZPASS(p, vk_to_nv9097_stencil_op(back->op.pass));
      P_NV9097_SET_BACK_STENCIL_FUNC(p, vk_to_nv9097_compare_op(back->op.compare));
   }

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_DS_STENCIL_COMPARE_MASK)) {
      P_IMMD(p, NV9097, SET_STENCIL_FUNC_MASK, front->compare_mask);
      P_IMMD(p, NV9097, SET_BACK_STENCIL_FUNC_MASK, back->compare_mask);
   }

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_DS_STENCIL_WRITE_MASK)) {
      P_IMMD(p, NV9097, SET_STENCIL_MASK, front->write_mask);
      P_IMMD(p, NV9097, SET_BACK_STENCIL_MASK, back->write_mask);
   }

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_DS_STENCIL_REFERENCE)) {
      P_IMMD(p, NV9097, SET_STENCIL_FUNC_REF, front->reference);
      P_IMMD(p, NV9097, SET_BACK_STENCIL_FUNC_REF, back->reference);
   }
}

static uint32_t
vk_to_nv9097_logic_op(VkLogicOp vk_op)
{
   ASSERTED uint16_t vk_to_nv9097[] = {
      [VK_LOGIC_OP_CLEAR]           = NV9097_SET_LOGIC_OP_FUNC_V_CLEAR,
      [VK_LOGIC_OP_AND]             = NV9097_SET_LOGIC_OP_FUNC_V_AND,
      [VK_LOGIC_OP_AND_REVERSE]     = NV9097_SET_LOGIC_OP_FUNC_V_AND_REVERSE,
      [VK_LOGIC_OP_COPY]            = NV9097_SET_LOGIC_OP_FUNC_V_COPY,
      [VK_LOGIC_OP_AND_INVERTED]    = NV9097_SET_LOGIC_OP_FUNC_V_AND_INVERTED,
      [VK_LOGIC_OP_NO_OP]           = NV9097_SET_LOGIC_OP_FUNC_V_NOOP,
      [VK_LOGIC_OP_XOR]             = NV9097_SET_LOGIC_OP_FUNC_V_XOR,
      [VK_LOGIC_OP_OR]              = NV9097_SET_LOGIC_OP_FUNC_V_OR,
      [VK_LOGIC_OP_NOR]             = NV9097_SET_LOGIC_OP_FUNC_V_NOR,
      [VK_LOGIC_OP_EQUIVALENT]      = NV9097_SET_LOGIC_OP_FUNC_V_EQUIV,
      [VK_LOGIC_OP_INVERT]          = NV9097_SET_LOGIC_OP_FUNC_V_INVERT,
      [VK_LOGIC_OP_OR_REVERSE]      = NV9097_SET_LOGIC_OP_FUNC_V_OR_REVERSE,
      [VK_LOGIC_OP_COPY_INVERTED]   = NV9097_SET_LOGIC_OP_FUNC_V_COPY_INVERTED,
      [VK_LOGIC_OP_OR_INVERTED]     = NV9097_SET_LOGIC_OP_FUNC_V_OR_INVERTED,
      [VK_LOGIC_OP_NAND]            = NV9097_SET_LOGIC_OP_FUNC_V_NAND,
      [VK_LOGIC_OP_SET]             = NV9097_SET_LOGIC_OP_FUNC_V_SET,
   };
   assert(vk_op < ARRAY_SIZE(vk_to_nv9097));

   uint32_t nv9097_op = 0x1500 | vk_op;
   assert(nv9097_op == vk_to_nv9097[vk_op]);
   return nv9097_op;
}

static uint32_t
vk_to_nv9097_blend_op(VkBlendOp vk_op)
{
#define OP(vk, nv) [VK_BLEND_OP_##vk] = NV9097_SET_BLEND_COLOR_OP_V_OGL_##nv
   ASSERTED uint16_t vk_to_nv9097[] = {
      OP(ADD,              FUNC_ADD),
      OP(SUBTRACT,         FUNC_SUBTRACT),
      OP(REVERSE_SUBTRACT, FUNC_REVERSE_SUBTRACT),
      OP(MIN,              MIN),
      OP(MAX,              MAX),
   };
   assert(vk_op < ARRAY_SIZE(vk_to_nv9097));
#undef OP

   return vk_to_nv9097[vk_op];
}

static uint32_t
vk_to_nv9097_blend_factor(VkBlendFactor vk_factor)
{
#define FACTOR(vk, nv) [VK_BLEND_FACTOR_##vk] = \
   NV9097_SET_BLEND_COLOR_SOURCE_COEFF_V_##nv
   ASSERTED uint16_t vk_to_nv9097[] = {
      FACTOR(ZERO,                     OGL_ZERO),
      FACTOR(ONE,                      OGL_ONE),
      FACTOR(SRC_COLOR,                OGL_SRC_COLOR),
      FACTOR(ONE_MINUS_SRC_COLOR,      OGL_ONE_MINUS_SRC_COLOR),
      FACTOR(DST_COLOR,                OGL_DST_COLOR),
      FACTOR(ONE_MINUS_DST_COLOR,      OGL_ONE_MINUS_DST_COLOR),
      FACTOR(SRC_ALPHA,                OGL_SRC_ALPHA),
      FACTOR(ONE_MINUS_SRC_ALPHA,      OGL_ONE_MINUS_SRC_ALPHA),
      FACTOR(DST_ALPHA,                OGL_DST_ALPHA),
      FACTOR(ONE_MINUS_DST_ALPHA,      OGL_ONE_MINUS_DST_ALPHA),
      FACTOR(CONSTANT_COLOR,           OGL_CONSTANT_COLOR),
      FACTOR(ONE_MINUS_CONSTANT_COLOR, OGL_ONE_MINUS_CONSTANT_COLOR),
      FACTOR(CONSTANT_ALPHA,           OGL_CONSTANT_ALPHA),
      FACTOR(ONE_MINUS_CONSTANT_ALPHA, OGL_ONE_MINUS_CONSTANT_ALPHA),
      FACTOR(SRC_ALPHA_SATURATE,       OGL_SRC_ALPHA_SATURATE),
      FACTOR(SRC1_COLOR,               OGL_SRC1COLOR),
      FACTOR(ONE_MINUS_SRC1_COLOR,     OGL_INVSRC1COLOR),
      FACTOR(SRC1_ALPHA,               OGL_SRC1ALPHA),
      FACTOR(ONE_MINUS_SRC1_ALPHA,     OGL_INVSRC1ALPHA),
   };
   assert(vk_factor < ARRAY_SIZE(vk_to_nv9097));
#undef FACTOR

   return vk_to_nv9097[vk_factor];
}

void
nvk_mme_set_write_mask(struct mme_builder *b)
{
   struct mme_value count = mme_load(b);
   struct mme_value mask = mme_load(b);

   /*
    * mask is a bit field
    *
    * attachment index 88887777666655554444333322221111
    * component        abgrabgrabgrabgrabgrabgrabgrabgr
   */

   struct mme_value common_mask = mme_mov(b, mme_imm(1));
   struct mme_value first = mme_and(b, mask, mme_imm(BITFIELD_RANGE(0, 4)));
   struct mme_value i = mme_mov(b, mme_zero());

   mme_while(b, ine, i, count) {
      /*
         We call NV9097_SET_CT_WRITE per attachment. It needs a value as:
         0x0000 0000 0000 0000 000a 000b 000g 000r

         So for i=0 a mask of
         0x0000 0000 0000 0000 0000 0000 0000 1111
         becomes
         0x0000 0000 0000 0000 0001 0001 0001 0001
      */

      struct mme_value val = mme_merge(b, mme_zero(), mask, 0, 1, 0);
      mme_merge_to(b, val, val, mask, 4, 1, 1);
      mme_merge_to(b, val, val, mask, 8, 1, 2);
      mme_merge_to(b, val, val, mask, 12, 1, 3);

      mme_mthd_arr(b, NV9097_SET_CT_WRITE(0), i);
      mme_emit(b, val);
      mme_free_reg(b, val);

      /* Check if all masks are common */
      struct mme_value temp = mme_add(b, mask, mme_imm(BITFIELD_RANGE(0, 4)));
      mme_if(b, ine, first, temp) {
         mme_mov_to(b, common_mask, mme_zero());
      }
      mme_free_reg(b, temp);

      mme_srl_to(b, mask, mask, mme_imm(4));

      mme_add_to(b, i, i, mme_imm(1));
   }

   mme_mthd(b, NV9097_SET_SINGLE_CT_WRITE_CONTROL);
   mme_emit(b, common_mask);
}

static void
nvk_flush_cb_state(struct nvk_cmd_buffer *cmd)
{
   struct nvk_rendering_state *render = &cmd->state.gfx.render;
   const struct vk_dynamic_graphics_state *dyn =
      &cmd->vk.dynamic_graphics_state;

   struct nv_push *p =
      nvk_cmd_buffer_push(cmd, 13 + 10 * render->color_att_count);

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_CB_LOGIC_OP_ENABLE))
      P_IMMD(p, NV9097, SET_LOGIC_OP, dyn->cb.logic_op_enable);

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_CB_LOGIC_OP)) {
      const uint32_t func = vk_to_nv9097_logic_op(dyn->cb.logic_op);
      P_IMMD(p, NV9097, SET_LOGIC_OP_FUNC, func);
   }

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_CB_BLEND_ENABLES)) {
      for (uint8_t a = 0; a < render->color_att_count; a++) {
         P_IMMD(p, NV9097, SET_BLEND(a), dyn->cb.attachments[a].blend_enable);
      }
   }

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_CB_BLEND_EQUATIONS)) {
      for (uint8_t a = 0; a < render->color_att_count; a++) {
         const struct vk_color_blend_attachment_state *att =
            &dyn->cb.attachments[a];
         P_MTHD(p, NV9097, SET_BLEND_PER_TARGET_SEPARATE_FOR_ALPHA(a));
         P_NV9097_SET_BLEND_PER_TARGET_SEPARATE_FOR_ALPHA(p, a, ENABLE_TRUE);
         P_NV9097_SET_BLEND_PER_TARGET_COLOR_OP(p, a,
               vk_to_nv9097_blend_op(att->color_blend_op));
         P_NV9097_SET_BLEND_PER_TARGET_COLOR_SOURCE_COEFF(p, a,
               vk_to_nv9097_blend_factor(att->src_color_blend_factor));
         P_NV9097_SET_BLEND_PER_TARGET_COLOR_DEST_COEFF(p, a,
               vk_to_nv9097_blend_factor(att->dst_color_blend_factor));
         P_NV9097_SET_BLEND_PER_TARGET_ALPHA_OP(p, a,
               vk_to_nv9097_blend_op(att->alpha_blend_op));
         P_NV9097_SET_BLEND_PER_TARGET_ALPHA_SOURCE_COEFF(p, a,
               vk_to_nv9097_blend_factor(att->src_alpha_blend_factor));
         P_NV9097_SET_BLEND_PER_TARGET_ALPHA_DEST_COEFF(p, a,
               vk_to_nv9097_blend_factor(att->dst_alpha_blend_factor));
      }
   }

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_CB_WRITE_MASKS) ||
       BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_CB_COLOR_WRITE_ENABLES) ||
       BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_RP_ATTACHMENTS)) {
      uint32_t color_write_enables = 0x0;
      for (uint8_t a = 0; a < render->color_att_count; a++) {
         if (dyn->cb.color_write_enables & BITFIELD_BIT(a))
            color_write_enables |= 0xf << (4 * a);
      }

      uint32_t cb_att_write_mask = 0x0;
      for (uint8_t a = 0; a < render->color_att_count; a++)
         cb_att_write_mask |= dyn->cb.attachments[a].write_mask << (a * 4);

      uint32_t rp_att_write_mask = 0x0;
      for (uint8_t a = 0; a < MESA_VK_MAX_COLOR_ATTACHMENTS; a++) {
         if (dyn->rp.attachments & (MESA_VK_RP_ATTACHMENT_COLOR_0_BIT << a))
            rp_att_write_mask |= 0xf << (4 * a);
      }

      P_1INC(p, NV9097, CALL_MME_MACRO(NVK_MME_SET_WRITE_MASK));
      P_INLINE_DATA(p, render->color_att_count);
      P_INLINE_DATA(p, color_write_enables &
                       cb_att_write_mask &
                       rp_att_write_mask);
   }

   if (BITSET_TEST(dyn->dirty, MESA_VK_DYNAMIC_CB_BLEND_CONSTANTS)) {
      P_MTHD(p, NV9097, SET_BLEND_CONST_RED);
      P_NV9097_SET_BLEND_CONST_RED(p,     fui(dyn->cb.blend_constants[0]));
      P_NV9097_SET_BLEND_CONST_GREEN(p,   fui(dyn->cb.blend_constants[1]));
      P_NV9097_SET_BLEND_CONST_BLUE(p,    fui(dyn->cb.blend_constants[2]));
      P_NV9097_SET_BLEND_CONST_ALPHA(p,   fui(dyn->cb.blend_constants[3]));
   }
}

static void
nvk_flush_dynamic_state(struct nvk_cmd_buffer *cmd)
{
   struct vk_dynamic_graphics_state *dyn =
      &cmd->vk.dynamic_graphics_state;

   if (!vk_dynamic_graphics_state_any_dirty(dyn))
      return;

   nvk_flush_vi_state(cmd);
   nvk_flush_ia_state(cmd);
   nvk_flush_ts_state(cmd);
   nvk_flush_vp_state(cmd);
   nvk_flush_rs_state(cmd);

   /* MESA_VK_DYNAMIC_FSR */

   nvk_flush_ms_state(cmd);
   nvk_flush_ds_state(cmd);
   nvk_flush_cb_state(cmd);

   vk_dynamic_graphics_state_clear_dirty(dyn);
}

void
nvk_mme_bind_cbuf_desc(struct mme_builder *b)
{
   /* First 4 bits are group, later bits are slot */
   struct mme_value group_slot = mme_load(b);

   if (b->devinfo->cls_eng3d >= TURING_A) {
      struct mme_value64 addr = mme_load_addr64(b);
      mme_tu104_read_fifoed(b, addr, mme_imm(3));
   }

   /* Load the descriptor */
   struct mme_value addr_lo = mme_load(b);
   struct mme_value addr_hi = mme_load(b);
   struct mme_value size = mme_load(b);

   struct mme_value cb = mme_alloc_reg(b);
   mme_if(b, ieq, size, mme_zero()) {
      /* Bottim bit is the valid bit, 8:4 are shader slot */
      mme_merge_to(b, cb, mme_zero(), group_slot, 4, 5, 4);
   }

   mme_if(b, ine, size, mme_zero()) {
      uint32_t alignment = nvk_min_cbuf_alignment(b->devinfo);
      mme_add_to(b, size, size, mme_imm(alignment - 1));
      mme_and_to(b, size, size, mme_imm(~(alignment - 1)));

      /* size = max(size, NVK_MAX_CBUF_SIZE) */
      assert(util_is_power_of_two_nonzero(NVK_MAX_CBUF_SIZE));
      struct mme_value is_large =
         mme_and(b, size, mme_imm(~(NVK_MAX_CBUF_SIZE - 1)));
      mme_if(b, ine, is_large, mme_zero()) {
         mme_mov_to(b, size, mme_imm(NVK_MAX_CBUF_SIZE));
      }

      mme_mthd(b, NV9097_SET_CONSTANT_BUFFER_SELECTOR_A);
      mme_emit(b, size);
      mme_emit(b, addr_hi);
      mme_emit(b, addr_lo);

      /* Bottim bit is the valid bit, 8:4 are shader slot */
      mme_merge_to(b, cb, mme_imm(1), group_slot, 4, 5, 4);
   }

   mme_free_reg(b, addr_hi);
   mme_free_reg(b, addr_lo);
   mme_free_reg(b, size);

   /* The group comes in the bottom 4 bits in group_slot and we need to
    * combine it with the method.  However, unlike most array methods with a
    * stride if 1 dword, BIND_GROUP_CONSTANT_BUFFER has a stride of 32B or 8
    * dwords.  This means we need to also shift by 3.
    */
   struct mme_value group = mme_merge(b, mme_imm(0), group_slot, 3, 4, 0);
   mme_mthd_arr(b, NV9097_BIND_GROUP_CONSTANT_BUFFER(0), group);
   mme_emit(b, cb);
}

static void
nvk_flush_descriptors(struct nvk_cmd_buffer *cmd)
{
   struct nvk_device *dev = nvk_cmd_buffer_device(cmd);
   struct nvk_physical_device *pdev = nvk_device_physical(dev);
   const uint32_t min_cbuf_alignment = nvk_min_cbuf_alignment(&pdev->info);
   struct nvk_descriptor_state *desc = &cmd->state.gfx.descriptors;
   VkResult result;

   nvk_cmd_buffer_flush_push_descriptors(cmd, desc);

   /* pre Pascal the constant buffer sizes need to be 0x100 aligned. As we
    * simply allocated a buffer and upload data to it, make sure its size is
    * 0x100 aligned.
    */
   STATIC_ASSERT((sizeof(desc->root) & 0xff) == 0);
   assert(sizeof(desc->root) % min_cbuf_alignment == 0);

   void *root_desc_map;
   uint64_t root_desc_addr;
   result = nvk_cmd_buffer_upload_alloc(cmd, sizeof(desc->root),
                                        min_cbuf_alignment,
                                        &root_desc_addr, &root_desc_map);
   if (unlikely(result != VK_SUCCESS)) {
      vk_command_buffer_set_error(&cmd->vk, result);
      return;
   }

   desc->root.root_desc_addr = root_desc_addr;
   memcpy(root_desc_map, &desc->root, sizeof(desc->root));

   /* Find cbuf maps for the 5 cbuf groups */
   const struct nvk_shader *cbuf_shaders[5] = { NULL, };
   for (gl_shader_stage stage = 0; stage < MESA_SHADER_STAGES; stage++) {
      const struct nvk_shader *shader = cmd->state.gfx.shaders[stage];
      if (shader == NULL)
         continue;

      uint32_t group = nvk_cbuf_binding_for_stage(stage);
      assert(group < ARRAY_SIZE(cbuf_shaders));
      cbuf_shaders[group] = shader;
   }

   uint32_t root_cbuf_count = 0;
   for (uint32_t group = 0; group < ARRAY_SIZE(cbuf_shaders); group++) {
      if (cbuf_shaders[group] == NULL)
         continue;

      const struct nvk_shader *shader = cbuf_shaders[group];
      const struct nvk_cbuf_map *cbuf_map = &shader->cbuf_map;

      for (uint32_t c = 0; c < cbuf_map->cbuf_count; c++) {
         const struct nvk_cbuf *cbuf = &cbuf_map->cbufs[c];

         /* We bind these at the very end */
         if (cbuf->type == NVK_CBUF_TYPE_ROOT_DESC) {
            root_cbuf_count++;
            continue;
         }

         struct nvk_buffer_address ba;
         if (nvk_cmd_buffer_get_cbuf_descriptor(cmd, desc, shader, cbuf, &ba)) {
            assert(ba.base_addr % min_cbuf_alignment == 0);
            ba.size = align(ba.size, min_cbuf_alignment);
            ba.size = MIN2(ba.size, NVK_MAX_CBUF_SIZE);

            struct nv_push *p = nvk_cmd_buffer_push(cmd, 6);

            if (ba.size > 0) {
               P_MTHD(p, NV9097, SET_CONSTANT_BUFFER_SELECTOR_A);
               P_NV9097_SET_CONSTANT_BUFFER_SELECTOR_A(p, ba.size);
               P_NV9097_SET_CONSTANT_BUFFER_SELECTOR_B(p, ba.base_addr >> 32);
               P_NV9097_SET_CONSTANT_BUFFER_SELECTOR_C(p, ba.base_addr);
            }

            P_IMMD(p, NV9097, BIND_GROUP_CONSTANT_BUFFER(group), {
               .valid = ba.size > 0,
               .shader_slot = c,
            });
         } else {
            uint64_t desc_addr =
               nvk_cmd_buffer_get_cbuf_descriptor_addr(cmd, desc, cbuf);

            if (nvk_cmd_buffer_3d_cls(cmd) >= TURING_A) {
               struct nv_push *p = nvk_cmd_buffer_push(cmd, 4);

               P_1INC(p, NV9097, CALL_MME_MACRO(NVK_MME_BIND_CBUF_DESC));
               P_INLINE_DATA(p, group | (c << 4));
               P_INLINE_DATA(p, desc_addr >> 32);
               P_INLINE_DATA(p, desc_addr);
            } else {
               struct nv_push *p = nvk_cmd_buffer_push(cmd, 2);

               P_1INC(p, NV9097, CALL_MME_MACRO(NVK_MME_BIND_CBUF_DESC));
               P_INLINE_DATA(p, group | (c << 4));

               nv_push_update_count(p, 3);
               nvk_cmd_buffer_push_indirect(cmd, desc_addr, 12);
            }
         }
      }
   }

   /* We bind all root descriptors last so that CONSTANT_BUFFER_SELECTOR is
    * always left pointing at the root descriptor table.  This way draw
    * parameters and similar MME root table updates always hit the root
    * descriptor table and not some random UBO.
    */
   struct nv_push *p = nvk_cmd_buffer_push(cmd, 4 + 2 * root_cbuf_count);
   P_MTHD(p, NV9097, SET_CONSTANT_BUFFER_SELECTOR_A);
   P_NV9097_SET_CONSTANT_BUFFER_SELECTOR_A(p, sizeof(desc->root));
   P_NV9097_SET_CONSTANT_BUFFER_SELECTOR_B(p, root_desc_addr >> 32);
   P_NV9097_SET_CONSTANT_BUFFER_SELECTOR_C(p, root_desc_addr);

   for (uint32_t group = 0; group < ARRAY_SIZE(cbuf_shaders); group++) {
      if (cbuf_shaders[group] == NULL)
         continue;

      const struct nvk_cbuf_map *cbuf_map = &cbuf_shaders[group]->cbuf_map;

      for (uint32_t c = 0; c < cbuf_map->cbuf_count; c++) {
         const struct nvk_cbuf *cbuf = &cbuf_map->cbufs[c];
         if (cbuf->type == NVK_CBUF_TYPE_ROOT_DESC) {
            P_IMMD(p, NV9097, BIND_GROUP_CONSTANT_BUFFER(group), {
               .valid = VALID_TRUE,
               .shader_slot = c,
            });
         }
      }
   }
}

static void
nvk_flush_gfx_state(struct nvk_cmd_buffer *cmd)
{
   nvk_flush_shaders(cmd);
   nvk_flush_dynamic_state(cmd);
   nvk_flush_descriptors(cmd);
}

static uint32_t
vk_to_nv_index_format(VkIndexType type)
{
   switch (type) {
   case VK_INDEX_TYPE_UINT16:
      return NVC597_SET_INDEX_BUFFER_E_INDEX_SIZE_TWO_BYTES;
   case VK_INDEX_TYPE_UINT32:
      return NVC597_SET_INDEX_BUFFER_E_INDEX_SIZE_FOUR_BYTES;
   case VK_INDEX_TYPE_UINT8_KHR:
      return NVC597_SET_INDEX_BUFFER_E_INDEX_SIZE_ONE_BYTE;
   default:
      unreachable("Invalid index type");
   }
}

VKAPI_ATTR void VKAPI_CALL
nvk_CmdBindIndexBuffer2KHR(VkCommandBuffer commandBuffer,
                           VkBuffer _buffer,
                           VkDeviceSize offset,
                           VkDeviceSize size,
                           VkIndexType indexType)
{
   VK_FROM_HANDLE(nvk_cmd_buffer, cmd, commandBuffer);
   VK_FROM_HANDLE(nvk_buffer, buffer, _buffer);

   struct nv_push *p = nvk_cmd_buffer_push(cmd, 10);

   uint64_t addr, range;
   if (buffer != NULL && size > 0) {
      addr = nvk_buffer_address(buffer, offset);
      range = vk_buffer_range(&buffer->vk, offset, size);
   } else {
      range = addr = 0;
   }

   P_IMMD(p, NV9097, SET_DA_PRIMITIVE_RESTART_INDEX,
          vk_index_to_restart(indexType));

   P_MTHD(p, NV9097, SET_INDEX_BUFFER_A);
   P_NV9097_SET_INDEX_BUFFER_A(p, addr >> 32);
   P_NV9097_SET_INDEX_BUFFER_B(p, addr);

   if (nvk_cmd_buffer_3d_cls(cmd) >= TURING_A) {
      P_MTHD(p, NVC597, SET_INDEX_BUFFER_SIZE_A);
      P_NVC597_SET_INDEX_BUFFER_SIZE_A(p, range >> 32);
      P_NVC597_SET_INDEX_BUFFER_SIZE_B(p, range);
   } else {
      /* TODO: What about robust zero-size buffers? */
      const uint64_t limit = range > 0 ? addr + range - 1 : 0;
      P_MTHD(p, NV9097, SET_INDEX_BUFFER_C);
      P_NV9097_SET_INDEX_BUFFER_C(p, limit >> 32);
      P_NV9097_SET_INDEX_BUFFER_D(p, limit);
   }

   P_IMMD(p, NV9097, SET_INDEX_BUFFER_E, vk_to_nv_index_format(indexType));
}

void
nvk_cmd_bind_vertex_buffer(struct nvk_cmd_buffer *cmd, uint32_t vb_idx,
                           struct nvk_addr_range addr_range)
{
   /* Used for meta save/restore */
   if (vb_idx == 0)
      cmd->state.gfx.vb0 = addr_range;

   struct nv_push *p = nvk_cmd_buffer_push(cmd, 6);

   P_MTHD(p, NV9097, SET_VERTEX_STREAM_A_LOCATION_A(vb_idx));
   P_NV9097_SET_VERTEX_STREAM_A_LOCATION_A(p, vb_idx, addr_range.addr >> 32);
   P_NV9097_SET_VERTEX_STREAM_A_LOCATION_B(p, vb_idx, addr_range.addr);

   if (nvk_cmd_buffer_3d_cls(cmd) >= TURING_A) {
      P_MTHD(p, NVC597, SET_VERTEX_STREAM_SIZE_A(vb_idx));
      P_NVC597_SET_VERTEX_STREAM_SIZE_A(p, vb_idx, addr_range.range >> 32);
      P_NVC597_SET_VERTEX_STREAM_SIZE_B(p, vb_idx, addr_range.range);
   } else {
      /* TODO: What about robust zero-size buffers? */
      const uint64_t limit = addr_range.range > 0 ?
         addr_range.addr + addr_range.range - 1 : 0;
      P_MTHD(p, NV9097, SET_VERTEX_STREAM_LIMIT_A_A(vb_idx));
      P_NV9097_SET_VERTEX_STREAM_LIMIT_A_A(p, vb_idx, limit >> 32);
      P_NV9097_SET_VERTEX_STREAM_LIMIT_A_B(p, vb_idx, limit);
   }
}

VKAPI_ATTR void VKAPI_CALL
nvk_CmdBindVertexBuffers2(VkCommandBuffer commandBuffer,
                          uint32_t firstBinding,
                          uint32_t bindingCount,
                          const VkBuffer *pBuffers,
                          const VkDeviceSize *pOffsets,
                          const VkDeviceSize *pSizes,
                          const VkDeviceSize *pStrides)
{
   VK_FROM_HANDLE(nvk_cmd_buffer, cmd, commandBuffer);

   if (pStrides) {
      vk_cmd_set_vertex_binding_strides(&cmd->vk, firstBinding,
                                        bindingCount, pStrides);
   }

   for (uint32_t i = 0; i < bindingCount; i++) {
      VK_FROM_HANDLE(nvk_buffer, buffer, pBuffers[i]);
      uint32_t idx = firstBinding + i;

      uint64_t size = pSizes ? pSizes[i] : VK_WHOLE_SIZE;
      const struct nvk_addr_range addr_range =
         nvk_buffer_addr_range(buffer, pOffsets[i], size);

      nvk_cmd_bind_vertex_buffer(cmd, idx, addr_range);
   }
}

static uint32_t
vk_to_nv9097_primitive_topology(VkPrimitiveTopology prim)
{
   switch (prim) {
   case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
      return NV9097_BEGIN_OP_POINTS;
   case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
      return NV9097_BEGIN_OP_LINES;
   case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
      return NV9097_BEGIN_OP_LINE_STRIP;
   case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"
   case VK_PRIMITIVE_TOPOLOGY_META_RECT_LIST_MESA:
#pragma GCC diagnostic pop
      return NV9097_BEGIN_OP_TRIANGLES;
   case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
      return NV9097_BEGIN_OP_TRIANGLE_STRIP;
   case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
      return NV9097_BEGIN_OP_TRIANGLE_FAN;
   case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
      return NV9097_BEGIN_OP_LINELIST_ADJCY;
   case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
      return NV9097_BEGIN_OP_LINESTRIP_ADJCY;
   case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
      return NV9097_BEGIN_OP_TRIANGLELIST_ADJCY;
   case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
      return NV9097_BEGIN_OP_TRIANGLESTRIP_ADJCY;
   case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
      return NV9097_BEGIN_OP_PATCH;
   default:
      unreachable("Invalid primitive topology");
   }
}

struct mme_draw_params {
   struct mme_value base_vertex;
   struct mme_value first_vertex;
   struct mme_value first_instance;
   struct mme_value draw_idx;
};

static void
nvk_mme_build_set_draw_params(struct mme_builder *b,
                              const struct mme_draw_params *p)
{
   const uint32_t draw_params_offset = nvk_root_descriptor_offset(draw);
   mme_mthd(b, NV9097_LOAD_CONSTANT_BUFFER_OFFSET);
   mme_emit(b, mme_imm(draw_params_offset));
   mme_mthd(b, NV9097_LOAD_CONSTANT_BUFFER(0));
   mme_emit(b, p->first_vertex);
   mme_emit(b, p->first_instance);
   mme_emit(b, p->draw_idx);
   mme_emit(b, mme_zero() /* view_index */);

   mme_mthd(b, NV9097_SET_GLOBAL_BASE_VERTEX_INDEX);
   mme_emit(b, p->base_vertex);
   mme_mthd(b, NV9097_SET_VERTEX_ID_BASE);
   mme_emit(b, p->base_vertex);

   mme_mthd(b, NV9097_SET_GLOBAL_BASE_INSTANCE_INDEX);
   mme_emit(b, p->first_instance);
}

static void
nvk_mme_emit_view_index(struct mme_builder *b, struct mme_value view_index)
{
   /* Set the push constant */
   mme_mthd(b, NV9097_LOAD_CONSTANT_BUFFER_OFFSET);
   mme_emit(b, mme_imm(nvk_root_descriptor_offset(draw.view_index)));
   mme_mthd(b, NV9097_LOAD_CONSTANT_BUFFER(0));
   mme_emit(b, view_index);

   /* Set the layer to the view index */
   STATIC_ASSERT(DRF_LO(NV9097_SET_RT_LAYER_V) == 0);
   STATIC_ASSERT(NV9097_SET_RT_LAYER_CONTROL_V_SELECTS_LAYER == 0);
   mme_mthd(b, NV9097_SET_RT_LAYER);
   mme_emit(b, view_index);
}

static void
nvk_mme_build_draw_loop(struct mme_builder *b,
                        struct mme_value instance_count,
                        struct mme_value first_vertex,
                        struct mme_value vertex_count)
{
   struct mme_value begin = nvk_mme_load_scratch(b, DRAW_BEGIN);

   mme_loop(b, instance_count) {
      mme_mthd(b, NV9097_BEGIN);
      mme_emit(b, begin);

      mme_mthd(b, NV9097_SET_VERTEX_ARRAY_START);
      mme_emit(b, first_vertex);
      mme_emit(b, vertex_count);

      mme_mthd(b, NV9097_END);
      mme_emit(b, mme_zero());

      mme_set_field_enum(b, begin, NV9097_BEGIN_INSTANCE_ID, SUBSEQUENT);
   }

   mme_free_reg(b, begin);
}

static void
nvk_mme_build_draw(struct mme_builder *b,
                   struct mme_value draw_idx)
{
   /* These are in VkDrawIndirectCommand order */
   struct mme_value vertex_count = mme_load(b);
   struct mme_value instance_count = mme_load(b);
   struct mme_value first_vertex = mme_load(b);
   struct mme_value first_instance = mme_load(b);

   struct mme_draw_params params = {
      .first_vertex = first_vertex,
      .first_instance = first_instance,
      .draw_idx = draw_idx,
   };
   nvk_mme_build_set_draw_params(b, &params);

   mme_free_reg(b, first_instance);

   if (b->devinfo->cls_eng3d < TURING_A)
      nvk_mme_spill(b, DRAW_IDX, draw_idx);

   struct mme_value view_mask = nvk_mme_load_scratch(b, VIEW_MASK);
   mme_if(b, ieq, view_mask, mme_zero()) {
      mme_free_reg(b, view_mask);

      nvk_mme_build_draw_loop(b, instance_count,
                              first_vertex, vertex_count);
   }

   view_mask = nvk_mme_load_scratch(b, VIEW_MASK);
   mme_if(b, ine, view_mask, mme_zero()) {
      mme_free_reg(b, view_mask);

      struct mme_value view = mme_mov(b, mme_zero());
      mme_while(b, ine, view, mme_imm(32)) {
         view_mask = nvk_mme_load_scratch(b, VIEW_MASK);
         struct mme_value has_view = mme_bfe(b, view_mask, view, 1);
         mme_free_reg(b, view_mask);
         mme_if(b, ine, has_view, mme_zero()) {
            mme_free_reg(b, has_view);
            nvk_mme_emit_view_index(b, view);
            nvk_mme_build_draw_loop(b, instance_count,
                                    first_vertex, vertex_count);
         }

         mme_add_to(b, view, view, mme_imm(1));
      }
      mme_free_reg(b, view);
   }

   mme_free_reg(b, instance_count);
   mme_free_reg(b, first_vertex);
   mme_free_reg(b, vertex_count);

   if (b->devinfo->cls_eng3d < TURING_A)
      nvk_mme_unspill(b, DRAW_IDX, draw_idx);
}

void
nvk_mme_draw(struct mme_builder *b)
{
   nvk_mme_load_to_scratch(b, DRAW_BEGIN);
   struct mme_value draw_idx = mme_load(b);

   nvk_mme_build_draw(b, draw_idx);
}

VKAPI_ATTR void VKAPI_CALL
nvk_CmdDraw(VkCommandBuffer commandBuffer,
            uint32_t vertexCount,
            uint32_t instanceCount,
            uint32_t firstVertex,
            uint32_t firstInstance)
{
   VK_FROM_HANDLE(nvk_cmd_buffer, cmd, commandBuffer);
   const struct vk_dynamic_graphics_state *dyn =
      &cmd->vk.dynamic_graphics_state;

   nvk_flush_gfx_state(cmd);

   uint32_t begin;
   V_NV9097_BEGIN(begin, {
      .op = vk_to_nv9097_primitive_topology(dyn->ia.primitive_topology),
      .primitive_id = NV9097_BEGIN_PRIMITIVE_ID_FIRST,
      .instance_id = NV9097_BEGIN_INSTANCE_ID_FIRST,
      .split_mode = SPLIT_MODE_NORMAL_BEGIN_NORMAL_END,
   });

   struct nv_push *p = nvk_cmd_buffer_push(cmd, 7);
   P_1INC(p, NV9097, CALL_MME_MACRO(NVK_MME_DRAW));
   P_INLINE_DATA(p, begin);
   P_INLINE_DATA(p, 0 /* draw_idx */);
   P_INLINE_DATA(p, vertexCount);
   P_INLINE_DATA(p, instanceCount);
   P_INLINE_DATA(p, firstVertex);
   P_INLINE_DATA(p, firstInstance);
}

VKAPI_ATTR void VKAPI_CALL
nvk_CmdDrawMultiEXT(VkCommandBuffer commandBuffer,
                    uint32_t drawCount,
                    const VkMultiDrawInfoEXT *pVertexInfo,
                    uint32_t instanceCount,
                    uint32_t firstInstance,
                    uint32_t stride)
{
   VK_FROM_HANDLE(nvk_cmd_buffer, cmd, commandBuffer);
   const struct vk_dynamic_graphics_state *dyn =
      &cmd->vk.dynamic_graphics_state;

   nvk_flush_gfx_state(cmd);

   uint32_t begin;
   V_NV9097_BEGIN(begin, {
      .op = vk_to_nv9097_primitive_topology(dyn->ia.primitive_topology),
      .primitive_id = NV9097_BEGIN_PRIMITIVE_ID_FIRST,
      .instance_id = NV9097_BEGIN_INSTANCE_ID_FIRST,
      .split_mode = SPLIT_MODE_NORMAL_BEGIN_NORMAL_END,
   });

   for (uint32_t draw_idx = 0; draw_idx < drawCount; draw_idx++) {
      struct nv_push *p = nvk_cmd_buffer_push(cmd, 7);
      P_1INC(p, NV9097, CALL_MME_MACRO(NVK_MME_DRAW));
      P_INLINE_DATA(p, begin);
      P_INLINE_DATA(p, draw_idx);
      P_INLINE_DATA(p, pVertexInfo->vertexCount);
      P_INLINE_DATA(p, instanceCount);
      P_INLINE_DATA(p, pVertexInfo->firstVertex);
      P_INLINE_DATA(p, firstInstance);

      pVertexInfo = ((void *)pVertexInfo) + stride;
   }
}

static void
nvk_mme_build_draw_indexed_loop(struct mme_builder *b,
                                struct mme_value instance_count,
                                struct mme_value first_index,
                                struct mme_value index_count)
{
   struct mme_value begin = nvk_mme_load_scratch(b, DRAW_BEGIN);

   mme_loop(b, instance_count) {
      mme_mthd(b, NV9097_BEGIN);
      mme_emit(b, begin);

      mme_mthd(b, NV9097_SET_INDEX_BUFFER_F);
      mme_emit(b, first_index);
      mme_emit(b, index_count);

      mme_mthd(b, NV9097_END);
      mme_emit(b, mme_zero());

      mme_set_field_enum(b, begin, NV9097_BEGIN_INSTANCE_ID, SUBSEQUENT);
   }

   mme_free_reg(b, begin);
}

static void
nvk_mme_build_draw_indexed(struct mme_builder *b,
                           struct mme_value draw_idx)
{
   /* These are in VkDrawIndexedIndirectCommand order */
   struct mme_value index_count = mme_load(b);
   struct mme_value instance_count = mme_load(b);
   struct mme_value first_index = mme_load(b);
   struct mme_value vertex_offset = mme_load(b);
   struct mme_value first_instance = mme_load(b);

   struct mme_draw_params params = {
      .base_vertex = vertex_offset,
      .first_vertex = vertex_offset,
      .first_instance = first_instance,
      .draw_idx = draw_idx,
   };
   nvk_mme_build_set_draw_params(b, &params);

   mme_free_reg(b, vertex_offset);
   mme_free_reg(b, first_instance);

   if (b->devinfo->cls_eng3d < TURING_A)
      nvk_mme_spill(b, DRAW_IDX, draw_idx);

   struct mme_value view_mask = nvk_mme_load_scratch(b, VIEW_MASK);
   mme_if(b, ieq, view_mask, mme_zero()) {
      mme_free_reg(b, view_mask);

      nvk_mme_build_draw_indexed_loop(b, instance_count,
                                      first_index, index_count);
   }

   view_mask = nvk_mme_load_scratch(b, VIEW_MASK);
   mme_if(b, ine, view_mask, mme_zero()) {
      mme_free_reg(b, view_mask);

      struct mme_value view = mme_mov(b, mme_zero());
      mme_while(b, ine, view, mme_imm(32)) {
         view_mask = nvk_mme_load_scratch(b, VIEW_MASK);
         struct mme_value has_view = mme_bfe(b, view_mask, view, 1);
         mme_free_reg(b, view_mask);
         mme_if(b, ine, has_view, mme_zero()) {
            mme_free_reg(b, has_view);
            nvk_mme_emit_view_index(b, view);
            nvk_mme_build_draw_indexed_loop(b, instance_count,
                                            first_index, index_count);
         }

         mme_add_to(b, view, view, mme_imm(1));
      }
      mme_free_reg(b, view);
   }

   mme_free_reg(b, instance_count);
   mme_free_reg(b, first_index);
   mme_free_reg(b, index_count);

   if (b->devinfo->cls_eng3d < TURING_A)
      nvk_mme_unspill(b, DRAW_IDX, draw_idx);
}

void
nvk_mme_draw_indexed(struct mme_builder *b)
{
   nvk_mme_load_to_scratch(b, DRAW_BEGIN);
   struct mme_value draw_idx = mme_load(b);

   nvk_mme_build_draw_indexed(b, draw_idx);
}

VKAPI_ATTR void VKAPI_CALL
nvk_CmdDrawIndexed(VkCommandBuffer commandBuffer,
                   uint32_t indexCount,
                   uint32_t instanceCount,
                   uint32_t firstIndex,
                   int32_t vertexOffset,
                   uint32_t firstInstance)
{
   VK_FROM_HANDLE(nvk_cmd_buffer, cmd, commandBuffer);
   const struct vk_dynamic_graphics_state *dyn =
      &cmd->vk.dynamic_graphics_state;

   nvk_flush_gfx_state(cmd);

   uint32_t begin;
   V_NV9097_BEGIN(begin, {
      .op = vk_to_nv9097_primitive_topology(dyn->ia.primitive_topology),
      .primitive_id = NV9097_BEGIN_PRIMITIVE_ID_FIRST,
      .instance_id = NV9097_BEGIN_INSTANCE_ID_FIRST,
      .split_mode = SPLIT_MODE_NORMAL_BEGIN_NORMAL_END,
   });

   struct nv_push *p = nvk_cmd_buffer_push(cmd, 8);
   P_1INC(p, NV9097, CALL_MME_MACRO(NVK_MME_DRAW_INDEXED));
   P_INLINE_DATA(p, begin);
   P_INLINE_DATA(p, 0 /* draw_idx */);
   P_INLINE_DATA(p, indexCount);
   P_INLINE_DATA(p, instanceCount);
   P_INLINE_DATA(p, firstIndex);
   P_INLINE_DATA(p, vertexOffset);
   P_INLINE_DATA(p, firstInstance);
}

VKAPI_ATTR void VKAPI_CALL
nvk_CmdDrawMultiIndexedEXT(VkCommandBuffer commandBuffer,
                           uint32_t drawCount,
                           const VkMultiDrawIndexedInfoEXT *pIndexInfo,
                           uint32_t instanceCount,
                           uint32_t firstInstance,
                           uint32_t stride,
                           const int32_t *pVertexOffset)
{
   VK_FROM_HANDLE(nvk_cmd_buffer, cmd, commandBuffer);
   const struct vk_dynamic_graphics_state *dyn =
      &cmd->vk.dynamic_graphics_state;

   nvk_flush_gfx_state(cmd);

   uint32_t begin;
   V_NV9097_BEGIN(begin, {
      .op = vk_to_nv9097_primitive_topology(dyn->ia.primitive_topology),
      .primitive_id = NV9097_BEGIN_PRIMITIVE_ID_FIRST,
      .instance_id = NV9097_BEGIN_INSTANCE_ID_FIRST,
      .split_mode = SPLIT_MODE_NORMAL_BEGIN_NORMAL_END,
   });

   for (uint32_t draw_idx = 0; draw_idx < drawCount; draw_idx++) {
      const uint32_t vertex_offset =
         pVertexOffset != NULL ? *pVertexOffset : pIndexInfo->vertexOffset;

      struct nv_push *p = nvk_cmd_buffer_push(cmd, 8);
      P_1INC(p, NV9097, CALL_MME_MACRO(NVK_MME_DRAW_INDEXED));
      P_INLINE_DATA(p, begin);
      P_INLINE_DATA(p, draw_idx);
      P_INLINE_DATA(p, pIndexInfo->indexCount);
      P_INLINE_DATA(p, instanceCount);
      P_INLINE_DATA(p, pIndexInfo->firstIndex);
      P_INLINE_DATA(p, vertex_offset);
      P_INLINE_DATA(p, firstInstance);

      pIndexInfo = ((void *)pIndexInfo) + stride;
   }
}

void
nvk_mme_draw_indirect(struct mme_builder *b)
{
   nvk_mme_load_to_scratch(b, DRAW_BEGIN);

   if (b->devinfo->cls_eng3d >= TURING_A) {
      struct mme_value64 draw_addr = mme_load_addr64(b);
      struct mme_value draw_count = mme_load(b);
      struct mme_value stride = mme_load(b);

      struct mme_value draw = mme_mov(b, mme_zero());
      mme_while(b, ult, draw, draw_count) {
         mme_tu104_read_fifoed(b, draw_addr, mme_imm(4));

         nvk_mme_build_draw(b, draw);

         mme_add_to(b, draw, draw, mme_imm(1));
         mme_add64_to(b, draw_addr, draw_addr, mme_value64(stride, mme_zero()));
      }
   } else {
      struct mme_value draw_count = mme_load(b);
      nvk_mme_load_to_scratch(b, DRAW_PAD_DW);

      struct mme_value draw = mme_mov(b, mme_zero());
      mme_while(b, ine, draw, draw_count) {
         nvk_mme_spill(b, DRAW_COUNT, draw_count);

         nvk_mme_build_draw(b, draw);
         mme_add_to(b, draw, draw, mme_imm(1));

         struct mme_value pad_dw = nvk_mme_load_scratch(b, DRAW_PAD_DW);
         mme_loop(b, pad_dw) {
            mme_free_reg(b, mme_load(b));
         }
         mme_free_reg(b, pad_dw);

         nvk_mme_unspill(b, DRAW_COUNT, draw_count);
      }
   }
}

VKAPI_ATTR void VKAPI_CALL
nvk_CmdDrawIndirect(VkCommandBuffer commandBuffer,
                    VkBuffer _buffer,
                    VkDeviceSize offset,
                    uint32_t drawCount,
                    uint32_t stride)
{
   VK_FROM_HANDLE(nvk_cmd_buffer, cmd, commandBuffer);
   VK_FROM_HANDLE(nvk_buffer, buffer, _buffer);
   const struct vk_dynamic_graphics_state *dyn =
      &cmd->vk.dynamic_graphics_state;

   /* From the Vulkan 1.3.238 spec:
    *
    *    VUID-vkCmdDrawIndirect-drawCount-00476
    *
    *    "If drawCount is greater than 1, stride must be a multiple of 4 and
    *    must be greater than or equal to sizeof(VkDrawIndirectCommand)"
    *
    * and
    *
    *    "If drawCount is less than or equal to one, stride is ignored."
    */
   if (drawCount > 1) {
      assert(stride % 4 == 0);
      assert(stride >= sizeof(VkDrawIndirectCommand));
   } else {
      stride = sizeof(VkDrawIndirectCommand);
   }

   nvk_flush_gfx_state(cmd);

   uint32_t begin;
   V_NV9097_BEGIN(begin, {
      .op = vk_to_nv9097_primitive_topology(dyn->ia.primitive_topology),
      .primitive_id = NV9097_BEGIN_PRIMITIVE_ID_FIRST,
      .instance_id = NV9097_BEGIN_INSTANCE_ID_FIRST,
      .split_mode = SPLIT_MODE_NORMAL_BEGIN_NORMAL_END,
   });

   if (nvk_cmd_buffer_3d_cls(cmd) >= TURING_A) {
      struct nv_push *p = nvk_cmd_buffer_push(cmd, 6);
      P_1INC(p, NV9097, CALL_MME_MACRO(NVK_MME_DRAW_INDIRECT));
      P_INLINE_DATA(p, begin);
      uint64_t draw_addr = nvk_buffer_address(buffer, offset);
      P_INLINE_DATA(p, draw_addr >> 32);
      P_INLINE_DATA(p, draw_addr);
      P_INLINE_DATA(p, drawCount);
      P_INLINE_DATA(p, stride);
   } else {
      const uint32_t max_draws_per_push =
         ((NV_PUSH_MAX_COUNT - 3) * 4) / stride;

      uint64_t draw_addr = nvk_buffer_address(buffer, offset);
      while (drawCount) {
         const uint32_t count = MIN2(drawCount, max_draws_per_push);

         struct nv_push *p = nvk_cmd_buffer_push(cmd, 4);
         P_1INC(p, NV9097, CALL_MME_MACRO(NVK_MME_DRAW_INDIRECT));
         P_INLINE_DATA(p, begin);
         P_INLINE_DATA(p, count);
         P_INLINE_DATA(p, (stride - sizeof(VkDrawIndirectCommand)) / 4);

         uint64_t range = count * (uint64_t)stride;
         nv_push_update_count(p, range / 4);
         nvk_cmd_buffer_push_indirect(cmd, draw_addr, range);

         draw_addr += range;
         drawCount -= count;
      }
   }
}

void
nvk_mme_draw_indexed_indirect(struct mme_builder *b)
{
   nvk_mme_load_to_scratch(b, DRAW_BEGIN);

   if (b->devinfo->cls_eng3d >= TURING_A) {
      struct mme_value64 draw_addr = mme_load_addr64(b);
      struct mme_value draw_count = mme_load(b);
      struct mme_value stride = mme_load(b);

      struct mme_value draw = mme_mov(b, mme_zero());
      mme_while(b, ult, draw, draw_count) {
         mme_tu104_read_fifoed(b, draw_addr, mme_imm(5));

         nvk_mme_build_draw_indexed(b, draw);

         mme_add_to(b, draw, draw, mme_imm(1));
         mme_add64_to(b, draw_addr, draw_addr, mme_value64(stride, mme_zero()));
      }
   } else {
      struct mme_value draw_count = mme_load(b);
      nvk_mme_load_to_scratch(b, DRAW_PAD_DW);

      struct mme_value draw = mme_mov(b, mme_zero());
      mme_while(b, ine, draw, draw_count) {
         nvk_mme_spill(b, DRAW_COUNT, draw_count);

         nvk_mme_build_draw_indexed(b, draw);
         mme_add_to(b, draw, draw, mme_imm(1));

         struct mme_value pad_dw = nvk_mme_load_scratch(b, DRAW_PAD_DW);
         mme_loop(b, pad_dw) {
            mme_free_reg(b, mme_load(b));
         }
         mme_free_reg(b, pad_dw);

         nvk_mme_unspill(b, DRAW_COUNT, draw_count);
      }
   }
}

VKAPI_ATTR void VKAPI_CALL
nvk_CmdDrawIndexedIndirect(VkCommandBuffer commandBuffer,
                           VkBuffer _buffer,
                           VkDeviceSize offset,
                           uint32_t drawCount,
                           uint32_t stride)
{
   VK_FROM_HANDLE(nvk_cmd_buffer, cmd, commandBuffer);
   VK_FROM_HANDLE(nvk_buffer, buffer, _buffer);
   const struct vk_dynamic_graphics_state *dyn =
      &cmd->vk.dynamic_graphics_state;

   /* From the Vulkan 1.3.238 spec:
    *
    *    VUID-vkCmdDrawIndexedIndirect-drawCount-00528
    *
    *    "If drawCount is greater than 1, stride must be a multiple of 4 and
    *    must be greater than or equal to sizeof(VkDrawIndexedIndirectCommand)"
    *
    * and
    *
    *    "If drawCount is less than or equal to one, stride is ignored."
    */
   if (drawCount > 1) {
      assert(stride % 4 == 0);
      assert(stride >= sizeof(VkDrawIndexedIndirectCommand));
   } else {
      stride = sizeof(VkDrawIndexedIndirectCommand);
   }

   nvk_flush_gfx_state(cmd);

   uint32_t begin;
   V_NV9097_BEGIN(begin, {
      .op = vk_to_nv9097_primitive_topology(dyn->ia.primitive_topology),
      .primitive_id = NV9097_BEGIN_PRIMITIVE_ID_FIRST,
      .instance_id = NV9097_BEGIN_INSTANCE_ID_FIRST,
      .split_mode = SPLIT_MODE_NORMAL_BEGIN_NORMAL_END,
   });

   if (nvk_cmd_buffer_3d_cls(cmd) >= TURING_A) {
      struct nv_push *p = nvk_cmd_buffer_push(cmd, 6);
      P_1INC(p, NV9097, CALL_MME_MACRO(NVK_MME_DRAW_INDEXED_INDIRECT));
      P_INLINE_DATA(p, begin);
      uint64_t draw_addr = nvk_buffer_address(buffer, offset);
      P_INLINE_DATA(p, draw_addr >> 32);
      P_INLINE_DATA(p, draw_addr);
      P_INLINE_DATA(p, drawCount);
      P_INLINE_DATA(p, stride);
   } else {
      const uint32_t max_draws_per_push =
         ((NV_PUSH_MAX_COUNT - 3) * 4) / stride;

      uint64_t draw_addr = nvk_buffer_address(buffer, offset);
      while (drawCount) {
         const uint32_t count = MIN2(drawCount, max_draws_per_push);

         struct nv_push *p = nvk_cmd_buffer_push(cmd, 4);
         P_1INC(p, NV9097, CALL_MME_MACRO(NVK_MME_DRAW_INDEXED_INDIRECT));
         P_INLINE_DATA(p, begin);
         P_INLINE_DATA(p, count);
         P_INLINE_DATA(p, (stride - sizeof(VkDrawIndexedIndirectCommand)) / 4);

         uint64_t range = count * (uint64_t)stride;
         nv_push_update_count(p, range / 4);
         nvk_cmd_buffer_push_indirect(cmd, draw_addr, range);

         draw_addr += range;
         drawCount -= count;
      }
   }
}

void
nvk_mme_draw_indirect_count(struct mme_builder *b)
{
   if (b->devinfo->cls_eng3d < TURING_A)
      return;

   nvk_mme_load_to_scratch(b, DRAW_BEGIN);

   struct mme_value64 draw_addr = mme_load_addr64(b);
   struct mme_value64 draw_count_addr = mme_load_addr64(b);
   struct mme_value draw_max = mme_load(b);
   struct mme_value stride = mme_load(b);

   mme_tu104_read_fifoed(b, draw_count_addr, mme_imm(1));
   mme_free_reg64(b, draw_count_addr);
   struct mme_value draw_count_buf = mme_load(b);

   mme_if(b, ule, draw_count_buf, draw_max) {
      mme_mov_to(b, draw_max, draw_count_buf);
   }
   mme_free_reg(b, draw_count_buf);

   struct mme_value draw = mme_mov(b, mme_zero());
   mme_while(b, ult, draw, draw_max) {
      mme_tu104_read_fifoed(b, draw_addr, mme_imm(4));

      nvk_mme_build_draw(b, draw);

      mme_add_to(b, draw, draw, mme_imm(1));
      mme_add64_to(b, draw_addr, draw_addr, mme_value64(stride, mme_zero()));
   }
}

VKAPI_ATTR void VKAPI_CALL
nvk_CmdDrawIndirectCount(VkCommandBuffer commandBuffer,
                         VkBuffer _buffer,
                         VkDeviceSize offset,
                         VkBuffer countBuffer,
                         VkDeviceSize countBufferOffset,
                         uint32_t maxDrawCount,
                         uint32_t stride)
{
   VK_FROM_HANDLE(nvk_cmd_buffer, cmd, commandBuffer);
   VK_FROM_HANDLE(nvk_buffer, buffer, _buffer);
   VK_FROM_HANDLE(nvk_buffer, count_buffer, countBuffer);

   const struct vk_dynamic_graphics_state *dyn =
      &cmd->vk.dynamic_graphics_state;

   /* TODO: Indirect count draw pre-Turing */
   assert(nvk_cmd_buffer_3d_cls(cmd) >= TURING_A);

   nvk_flush_gfx_state(cmd);

   uint32_t begin;
   V_NV9097_BEGIN(begin, {
      .op = vk_to_nv9097_primitive_topology(dyn->ia.primitive_topology),
      .primitive_id = NV9097_BEGIN_PRIMITIVE_ID_FIRST,
      .instance_id = NV9097_BEGIN_INSTANCE_ID_FIRST,
      .split_mode = SPLIT_MODE_NORMAL_BEGIN_NORMAL_END,
   });

   struct nv_push *p = nvk_cmd_buffer_push(cmd, 8);
   P_1INC(p, NV9097, CALL_MME_MACRO(NVK_MME_DRAW_INDIRECT_COUNT));
   P_INLINE_DATA(p, begin);
   uint64_t draw_addr = nvk_buffer_address(buffer, offset);
   P_INLINE_DATA(p, draw_addr >> 32);
   P_INLINE_DATA(p, draw_addr);
   uint64_t draw_count_addr = nvk_buffer_address(count_buffer,
                                                 countBufferOffset);
   P_INLINE_DATA(p, draw_count_addr >> 32);
   P_INLINE_DATA(p, draw_count_addr);
   P_INLINE_DATA(p, maxDrawCount);
   P_INLINE_DATA(p, stride);
}

void
nvk_mme_draw_indexed_indirect_count(struct mme_builder *b)
{
   if (b->devinfo->cls_eng3d < TURING_A)
      return;

   nvk_mme_load_to_scratch(b, DRAW_BEGIN);

   struct mme_value64 draw_addr = mme_load_addr64(b);
   struct mme_value64 draw_count_addr = mme_load_addr64(b);
   struct mme_value draw_max = mme_load(b);
   struct mme_value stride = mme_load(b);

   mme_tu104_read_fifoed(b, draw_count_addr, mme_imm(1));
   mme_free_reg64(b, draw_count_addr);
   struct mme_value draw_count_buf = mme_load(b);

   mme_if(b, ule, draw_count_buf, draw_max) {
      mme_mov_to(b, draw_max, draw_count_buf);
   }
   mme_free_reg(b, draw_count_buf);

   struct mme_value draw = mme_mov(b, mme_zero());
   mme_while(b, ult, draw, draw_max) {
      mme_tu104_read_fifoed(b, draw_addr, mme_imm(5));

      nvk_mme_build_draw_indexed(b, draw);

      mme_add_to(b, draw, draw, mme_imm(1));
      mme_add64_to(b, draw_addr, draw_addr, mme_value64(stride, mme_zero()));
   }
}

VKAPI_ATTR void VKAPI_CALL
nvk_CmdDrawIndexedIndirectCount(VkCommandBuffer commandBuffer,
                                VkBuffer _buffer,
                                VkDeviceSize offset,
                                VkBuffer countBuffer,
                                VkDeviceSize countBufferOffset,
                                uint32_t maxDrawCount,
                                uint32_t stride)
{
   VK_FROM_HANDLE(nvk_cmd_buffer, cmd, commandBuffer);
   VK_FROM_HANDLE(nvk_buffer, buffer, _buffer);
   VK_FROM_HANDLE(nvk_buffer, count_buffer, countBuffer);

   const struct vk_dynamic_graphics_state *dyn =
      &cmd->vk.dynamic_graphics_state;

   /* TODO: Indexed indirect count draw pre-Turing */
   assert(nvk_cmd_buffer_3d_cls(cmd) >= TURING_A);

   nvk_flush_gfx_state(cmd);

   uint32_t begin;
   V_NV9097_BEGIN(begin, {
      .op = vk_to_nv9097_primitive_topology(dyn->ia.primitive_topology),
      .primitive_id = NV9097_BEGIN_PRIMITIVE_ID_FIRST,
      .instance_id = NV9097_BEGIN_INSTANCE_ID_FIRST,
      .split_mode = SPLIT_MODE_NORMAL_BEGIN_NORMAL_END,
   });

   struct nv_push *p = nvk_cmd_buffer_push(cmd, 8);
   P_1INC(p, NV9097, CALL_MME_MACRO(NVK_MME_DRAW_INDEXED_INDIRECT_COUNT));
   P_INLINE_DATA(p, begin);
   uint64_t draw_addr = nvk_buffer_address(buffer, offset);
   P_INLINE_DATA(p, draw_addr >> 32);
   P_INLINE_DATA(p, draw_addr);
   uint64_t draw_count_addr = nvk_buffer_address(count_buffer,
                                                 countBufferOffset);
   P_INLINE_DATA(p, draw_count_addr >> 32);
   P_INLINE_DATA(p, draw_count_addr);
   P_INLINE_DATA(p, maxDrawCount);
   P_INLINE_DATA(p, stride);
}

static void
nvk_mme_xfb_draw_indirect_loop(struct mme_builder *b,
                               struct mme_value instance_count,
                               struct mme_value counter)
{
   struct mme_value begin = nvk_mme_load_scratch(b, DRAW_BEGIN);

   mme_loop(b, instance_count) {
      mme_mthd(b, NV9097_BEGIN);
      mme_emit(b, begin);

      mme_mthd(b, NV9097_DRAW_AUTO);
      mme_emit(b, counter);

      mme_mthd(b, NV9097_END);
      mme_emit(b, mme_zero());

      mme_set_field_enum(b, begin, NV9097_BEGIN_INSTANCE_ID, SUBSEQUENT);
   }

   mme_free_reg(b, begin);
}

void
nvk_mme_xfb_draw_indirect(struct mme_builder *b)
{
   nvk_mme_load_to_scratch(b, DRAW_BEGIN);

   struct mme_value instance_count = mme_load(b);
   struct mme_value first_instance = mme_load(b);

   if (b->devinfo->cls_eng3d >= TURING_A) {
      struct mme_value64 counter_addr = mme_load_addr64(b);
      mme_tu104_read_fifoed(b, counter_addr, mme_imm(1));
      mme_free_reg(b, counter_addr.lo);
      mme_free_reg(b, counter_addr.hi);
   }
   struct mme_value counter = mme_load(b);

   struct mme_draw_params params = {
      .first_instance = first_instance,
   };
   nvk_mme_build_set_draw_params(b, &params);

   mme_free_reg(b, first_instance);

   struct mme_value view_mask = nvk_mme_load_scratch(b, VIEW_MASK);
   mme_if(b, ieq, view_mask, mme_zero()) {
      mme_free_reg(b, view_mask);

      nvk_mme_xfb_draw_indirect_loop(b, instance_count, counter);
   }

   view_mask = nvk_mme_load_scratch(b, VIEW_MASK);
   mme_if(b, ine, view_mask, mme_zero()) {
      mme_free_reg(b, view_mask);

      struct mme_value view = mme_mov(b, mme_zero());
      mme_while(b, ine, view, mme_imm(32)) {
         view_mask = nvk_mme_load_scratch(b, VIEW_MASK);
         struct mme_value has_view = mme_bfe(b, view_mask, view, 1);
         mme_free_reg(b, view_mask);
         mme_if(b, ine, has_view, mme_zero()) {
            mme_free_reg(b, has_view);
            nvk_mme_emit_view_index(b, view);
            nvk_mme_xfb_draw_indirect_loop(b, instance_count, counter);
         }

         mme_add_to(b, view, view, mme_imm(1));
      }
   }

   mme_free_reg(b, instance_count);
   mme_free_reg(b, counter);
}

VKAPI_ATTR void VKAPI_CALL
nvk_CmdDrawIndirectByteCountEXT(VkCommandBuffer commandBuffer,
                                uint32_t instanceCount,
                                uint32_t firstInstance,
                                VkBuffer counterBuffer,
                                VkDeviceSize counterBufferOffset,
                                uint32_t counterOffset,
                                uint32_t vertexStride)
{
   VK_FROM_HANDLE(nvk_cmd_buffer, cmd, commandBuffer);
   VK_FROM_HANDLE(nvk_buffer, counter_buffer, counterBuffer);
   const struct vk_dynamic_graphics_state *dyn =
      &cmd->vk.dynamic_graphics_state;

   nvk_flush_gfx_state(cmd);

   uint32_t begin;
   V_NV9097_BEGIN(begin, {
      .op = vk_to_nv9097_primitive_topology(dyn->ia.primitive_topology),
      .primitive_id = NV9097_BEGIN_PRIMITIVE_ID_FIRST,
      .instance_id = NV9097_BEGIN_INSTANCE_ID_FIRST,
      .split_mode = SPLIT_MODE_NORMAL_BEGIN_NORMAL_END,
   });

   uint64_t counter_addr = nvk_buffer_address(counter_buffer,
                                              counterBufferOffset);

   if (nvk_cmd_buffer_3d_cls(cmd) >= TURING_A) {
      struct nv_push *p = nvk_cmd_buffer_push(cmd, 10);
      P_IMMD(p, NV9097, SET_DRAW_AUTO_START, counterOffset);
      P_IMMD(p, NV9097, SET_DRAW_AUTO_STRIDE, vertexStride);

      P_1INC(p, NV9097, CALL_MME_MACRO(NVK_MME_XFB_DRAW_INDIRECT));
      P_INLINE_DATA(p, begin);
      P_INLINE_DATA(p, instanceCount);
      P_INLINE_DATA(p, firstInstance);
      P_INLINE_DATA(p, counter_addr >> 32);
      P_INLINE_DATA(p, counter_addr);
   } else {
      struct nv_push *p = nvk_cmd_buffer_push(cmd, 9);
      P_IMMD(p, NV9097, SET_DRAW_AUTO_START, counterOffset);
      P_IMMD(p, NV9097, SET_DRAW_AUTO_STRIDE, vertexStride);

      P_1INC(p, NV9097, CALL_MME_MACRO(NVK_MME_XFB_DRAW_INDIRECT));
      P_INLINE_DATA(p, begin);
      P_INLINE_DATA(p, instanceCount);
      P_INLINE_DATA(p, firstInstance);
      nv_push_update_count(p, 1);
      nvk_cmd_buffer_push_indirect(cmd, counter_addr, 4);
   }
}

VKAPI_ATTR void VKAPI_CALL
nvk_CmdBindTransformFeedbackBuffersEXT(VkCommandBuffer commandBuffer,
                                       uint32_t firstBinding,
                                       uint32_t bindingCount,
                                       const VkBuffer *pBuffers,
                                       const VkDeviceSize *pOffsets,
                                       const VkDeviceSize *pSizes)
{
   VK_FROM_HANDLE(nvk_cmd_buffer, cmd, commandBuffer);

   for (uint32_t i = 0; i < bindingCount; i++) {
      VK_FROM_HANDLE(nvk_buffer, buffer, pBuffers[i]);
      uint32_t idx = firstBinding + i;
      uint64_t size = pSizes ? pSizes[i] : VK_WHOLE_SIZE;
      struct nvk_addr_range addr_range =
         nvk_buffer_addr_range(buffer, pOffsets[i], size);
      assert(addr_range.range <= UINT32_MAX);

      struct nv_push *p = nvk_cmd_buffer_push(cmd, 5);

      P_MTHD(p, NV9097, SET_STREAM_OUT_BUFFER_ENABLE(idx));
      P_NV9097_SET_STREAM_OUT_BUFFER_ENABLE(p, idx, V_TRUE);
      P_NV9097_SET_STREAM_OUT_BUFFER_ADDRESS_A(p, idx, addr_range.addr >> 32);
      P_NV9097_SET_STREAM_OUT_BUFFER_ADDRESS_B(p, idx, addr_range.addr);
      P_NV9097_SET_STREAM_OUT_BUFFER_SIZE(p, idx, (uint32_t)addr_range.range);
   }

   // TODO: do we need to SET_STREAM_OUT_BUFFER_ENABLE V_FALSE ?
}

void
nvk_mme_xfb_counter_load(struct mme_builder *b)
{
   struct mme_value buffer = mme_load(b);

   struct mme_value counter;
   if (b->devinfo->cls_eng3d >= TURING_A) {
      struct mme_value64 counter_addr = mme_load_addr64(b);

      mme_tu104_read_fifoed(b, counter_addr, mme_imm(1));
      mme_free_reg(b, counter_addr.lo);
      mme_free_reg(b, counter_addr.hi);

      counter = mme_load(b);
   } else {
      counter = mme_load(b);
   }

   mme_mthd_arr(b, NV9097_SET_STREAM_OUT_BUFFER_LOAD_WRITE_POINTER(0), buffer);
   mme_emit(b, counter);

   mme_free_reg(b, counter);
   mme_free_reg(b, buffer);
}

VKAPI_ATTR void VKAPI_CALL
nvk_CmdBeginTransformFeedbackEXT(VkCommandBuffer commandBuffer,
                                 uint32_t firstCounterBuffer,
                                 uint32_t counterBufferCount,
                                 const VkBuffer *pCounterBuffers,
                                 const VkDeviceSize *pCounterBufferOffsets)
{
   VK_FROM_HANDLE(nvk_cmd_buffer, cmd, commandBuffer);
   const uint32_t max_buffers = 4;

   struct nv_push *p = nvk_cmd_buffer_push(cmd, 2 + 2 * max_buffers);

   P_IMMD(p, NV9097, SET_STREAM_OUTPUT, ENABLE_TRUE);
   for (uint32_t i = 0; i < max_buffers; ++i) {
      P_IMMD(p, NV9097, SET_STREAM_OUT_BUFFER_LOAD_WRITE_POINTER(i), 0);
   }

   for (uint32_t i = 0; i < counterBufferCount; ++i) {
      if (pCounterBuffers[i] == VK_NULL_HANDLE)
         continue;

      VK_FROM_HANDLE(nvk_buffer, buffer, pCounterBuffers[i]);
      // index of counter buffer corresponts to index of transform buffer
      uint32_t cb_idx = firstCounterBuffer + i;
      uint64_t offset = pCounterBufferOffsets ? pCounterBufferOffsets[i] : 0;
      uint64_t cb_addr = nvk_buffer_address(buffer, offset);

      if (nvk_cmd_buffer_3d_cls(cmd) >= TURING_A) {
         struct nv_push *p = nvk_cmd_buffer_push(cmd, 4);
         P_1INC(p, NV9097, CALL_MME_MACRO(NVK_MME_XFB_COUNTER_LOAD));
         /* The STREAM_OUT_BUFFER_LOAD_WRITE_POINTER registers are 8 dword stride */
         P_INLINE_DATA(p, cb_idx * 8);
         P_INLINE_DATA(p, cb_addr >> 32);
         P_INLINE_DATA(p, cb_addr);
      } else {
         struct nv_push *p = nvk_cmd_buffer_push(cmd, 2);
         P_1INC(p, NV9097, CALL_MME_MACRO(NVK_MME_XFB_COUNTER_LOAD));
         P_INLINE_DATA(p, cb_idx);
         nv_push_update_count(p, 1);
         nvk_cmd_buffer_push_indirect(cmd, cb_addr, 4);
      }
   }
}

VKAPI_ATTR void VKAPI_CALL
nvk_CmdEndTransformFeedbackEXT(VkCommandBuffer commandBuffer,
                               uint32_t firstCounterBuffer,
                               uint32_t counterBufferCount,
                               const VkBuffer *pCounterBuffers,
                               const VkDeviceSize *pCounterBufferOffsets)
{
   VK_FROM_HANDLE(nvk_cmd_buffer, cmd, commandBuffer);

   struct nv_push *p = nvk_cmd_buffer_push(cmd, 5 * counterBufferCount + 2);

   P_IMMD(p, NV9097, SET_STREAM_OUTPUT, ENABLE_FALSE);

   for (uint32_t i = 0; i < counterBufferCount; ++i) {
      if (pCounterBuffers[i] == VK_NULL_HANDLE)
         continue;

      VK_FROM_HANDLE(nvk_buffer, buffer, pCounterBuffers[i]);
      // index of counter buffer corresponts to index of transform buffer
      uint32_t cb_idx = firstCounterBuffer + i;
      uint64_t offset = pCounterBufferOffsets ? pCounterBufferOffsets[i] : 0;
      uint64_t cb_addr = nvk_buffer_address(buffer, offset);

      P_MTHD(p, NV9097, SET_REPORT_SEMAPHORE_A);
      P_NV9097_SET_REPORT_SEMAPHORE_A(p, cb_addr >> 32);
      P_NV9097_SET_REPORT_SEMAPHORE_B(p, cb_addr);
      P_NV9097_SET_REPORT_SEMAPHORE_C(p, 0);
      P_NV9097_SET_REPORT_SEMAPHORE_D(p, {
         .operation = OPERATION_REPORT_ONLY,
         .pipeline_location = PIPELINE_LOCATION_STREAMING_OUTPUT,
         .report = REPORT_STREAMING_BYTE_COUNT,
         .sub_report = cb_idx,
         .structure_size = STRUCTURE_SIZE_ONE_WORD,
      });
   }
}

VKAPI_ATTR void VKAPI_CALL
nvk_CmdBeginConditionalRenderingEXT(VkCommandBuffer commandBuffer,
                                    const VkConditionalRenderingBeginInfoEXT *pConditionalRenderingBegin)
{
   VK_FROM_HANDLE(nvk_cmd_buffer, cmd, commandBuffer);
   VK_FROM_HANDLE(nvk_buffer, buffer, pConditionalRenderingBegin->buffer);

   uint64_t addr = nvk_buffer_address(buffer, pConditionalRenderingBegin->offset);
   bool inverted = pConditionalRenderingBegin->flags &
      VK_CONDITIONAL_RENDERING_INVERTED_BIT_EXT;

   /* From the Vulkan 1.3.280 spec:
    *
    *    "If the 32-bit value at offset in buffer memory is zero,
    *     then the rendering commands are discarded,
    *     otherwise they are executed as normal."
    *
    * The hardware compare a 64-bit value, as such we are required to copy it.
    */
   uint64_t tmp_addr;
   VkResult result = nvk_cmd_buffer_cond_render_alloc(cmd, &tmp_addr);
   if (result != VK_SUCCESS) {
      vk_command_buffer_set_error(&cmd->vk, result);
      return;
   }

   struct nv_push *p = nvk_cmd_buffer_push(cmd, 26);

   P_MTHD(p, NV90B5, OFFSET_IN_UPPER);
   P_NV90B5_OFFSET_IN_UPPER(p, addr >> 32);
   P_NV90B5_OFFSET_IN_LOWER(p, addr & 0xffffffff);
   P_NV90B5_OFFSET_OUT_UPPER(p, tmp_addr >> 32);
   P_NV90B5_OFFSET_OUT_LOWER(p, tmp_addr & 0xffffffff);
   P_NV90B5_PITCH_IN(p, 4);
   P_NV90B5_PITCH_OUT(p, 4);
   P_NV90B5_LINE_LENGTH_IN(p, 4);
   P_NV90B5_LINE_COUNT(p, 1);

   P_IMMD(p, NV90B5, SET_REMAP_COMPONENTS, {
      .dst_x = DST_X_SRC_X,
      .dst_y = DST_Y_SRC_X,
      .dst_z = DST_Z_NO_WRITE,
      .dst_w = DST_W_NO_WRITE,
      .component_size = COMPONENT_SIZE_ONE,
      .num_src_components = NUM_SRC_COMPONENTS_ONE,
      .num_dst_components = NUM_DST_COMPONENTS_TWO,
   });

   P_IMMD(p, NV90B5, LAUNCH_DMA, {
      .data_transfer_type = DATA_TRANSFER_TYPE_PIPELINED,
      .multi_line_enable = MULTI_LINE_ENABLE_TRUE,
      .flush_enable = FLUSH_ENABLE_TRUE,
      .src_memory_layout = SRC_MEMORY_LAYOUT_PITCH,
      .dst_memory_layout = DST_MEMORY_LAYOUT_PITCH,
      .remap_enable = REMAP_ENABLE_TRUE,
   });

   P_MTHD(p, NV9097, SET_RENDER_ENABLE_A);
   P_NV9097_SET_RENDER_ENABLE_A(p, tmp_addr >> 32);
   P_NV9097_SET_RENDER_ENABLE_B(p, tmp_addr & 0xfffffff0);
   P_NV9097_SET_RENDER_ENABLE_C(p, inverted ? MODE_RENDER_IF_EQUAL : MODE_RENDER_IF_NOT_EQUAL);

   P_MTHD(p, NV90C0, SET_RENDER_ENABLE_A);
   P_NV90C0_SET_RENDER_ENABLE_A(p, tmp_addr >> 32);
   P_NV90C0_SET_RENDER_ENABLE_B(p, tmp_addr & 0xfffffff0);
   P_NV90C0_SET_RENDER_ENABLE_C(p, inverted ? MODE_RENDER_IF_EQUAL : MODE_RENDER_IF_NOT_EQUAL);
}

VKAPI_ATTR void VKAPI_CALL
nvk_CmdEndConditionalRenderingEXT(VkCommandBuffer commandBuffer)
{
   VK_FROM_HANDLE(nvk_cmd_buffer, cmd, commandBuffer);

   struct nv_push *p = nvk_cmd_buffer_push(cmd, 12);
   P_MTHD(p, NV9097, SET_RENDER_ENABLE_A);
   P_NV9097_SET_RENDER_ENABLE_A(p, 0);
   P_NV9097_SET_RENDER_ENABLE_B(p, 0);
   P_NV9097_SET_RENDER_ENABLE_C(p, MODE_TRUE);

   P_MTHD(p, NV90C0, SET_RENDER_ENABLE_A);
   P_NV90C0_SET_RENDER_ENABLE_A(p, 0);
   P_NV90C0_SET_RENDER_ENABLE_B(p, 0);
   P_NV90C0_SET_RENDER_ENABLE_C(p, MODE_TRUE);
}
