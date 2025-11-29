/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_PIPELINE_GRAPHICS_H
#define RADV_PIPELINE_GRAPHICS_H

#include "sid.h"

#include "radv_descriptor_set.h"
#include "radv_pipeline.h"
#include "radv_shader.h"

#include "vk_graphics_state.h"

struct radv_sample_locations_state {
   VkSampleCountFlagBits per_pixel;
   VkExtent2D grid_size;
   uint32_t count;
   VkSampleLocationEXT locations[MAX_SAMPLE_LOCATIONS];
};

struct radv_dynamic_state {
   struct vk_dynamic_graphics_state vk;

   /**
    * Bitmask of (1ull << VK_DYNAMIC_STATE_*).
    * Defines the set of saved dynamic state.
    */
   uint64_t mask;

   struct {
      struct {
         float scale[3];
         float translate[3];
      } xform[MAX_VIEWPORTS];
   } hw_vp;

   struct radv_sample_locations_state sample_location;

   VkImageAspectFlags feedback_loop_aspects;
};

struct radv_multisample_state {
   bool sample_shading_enable;
   float min_sample_shading;
};

struct radv_ia_multi_vgt_param_helpers {
   uint32_t base;
   bool partial_es_wave;
   bool ia_switch_on_eoi;
   bool partial_vs_wave;
};

struct radv_sqtt_shaders_reloc {
   struct radeon_winsys_bo *bo;
   union radv_shader_arena_block *alloc;
   uint64_t va[MESA_VULKAN_SHADER_STAGES];
};

struct radv_graphics_pipeline {
   struct radv_pipeline base;

   struct radv_pipeline_layout layout;

   bool uses_drawid;
   bool uses_baseinstance;

   /* Whether the pipeline forces per-vertex VRS (GFX10.3+). */
   bool force_vrs_per_vertex;

   /* Whether the pipeline uses NGG (GFX10+). */
   bool is_ngg;
   bool has_ngg_culling;

   uint8_t vtx_emit_num;

   uint32_t vtx_base_sgpr;
   uint64_t dynamic_states;
   uint64_t needed_dynamic_state;

   VkShaderStageFlags active_stages;

   /* Used for rbplus */
   uint32_t col_format_non_compacted;

   struct radv_dynamic_state dynamic_state;

   struct radv_vs_input_state vs_input_state;

   struct radv_multisample_state ms;
   struct radv_ia_multi_vgt_param_helpers ia_multi_vgt_param;
   uint32_t binding_stride[MAX_VBS];
   uint8_t attrib_bindings[MAX_VERTEX_ATTRIBS];
   uint32_t attrib_ends[MAX_VERTEX_ATTRIBS];
   uint32_t attrib_index_offset[MAX_VERTEX_ATTRIBS];
   uint32_t db_render_control;

   /* Last pre-PS API stage */
   gl_shader_stage last_vgt_api_stage;

   unsigned rast_prim;

   /* For vk_graphics_pipeline_state */
   void *state_data;

   /* Custom blend mode for internal operations. */
   unsigned custom_blend_mode;

   /* Whether the pipeline uses out-of-order rasterization. */
   bool uses_out_of_order_rast;

   /* Whether the pipeline uses a VRS attachment. */
   bool uses_vrs_attachment;

   /* For graphics pipeline library */
   bool retain_shaders;

   /* For relocation of shaders with RGP. */
   struct radv_sqtt_shaders_reloc *sqtt_shaders_reloc;
};

RADV_DECL_PIPELINE_DOWNCAST(graphics, RADV_PIPELINE_GRAPHICS)

struct radv_retained_shaders {
   struct {
      void *serialized_nir;
      size_t serialized_nir_size;
      unsigned char shader_sha1[SHA1_DIGEST_LENGTH];
      struct radv_shader_stage_key key;
   } stages[MESA_VULKAN_SHADER_STAGES];
};

struct radv_graphics_lib_pipeline {
   struct radv_graphics_pipeline base;

   struct vk_graphics_pipeline_state graphics_state;

   VkGraphicsPipelineLibraryFlagsEXT lib_flags;

   struct radv_retained_shaders retained_shaders;

   void *mem_ctx;

   unsigned stage_count;
   VkPipelineShaderStageCreateInfo *stages;
   struct radv_shader_stage_key stage_keys[MESA_VULKAN_SHADER_STAGES];
};

RADV_DECL_PIPELINE_DOWNCAST(graphics_lib, RADV_PIPELINE_GRAPHICS_LIB)

static inline bool
radv_pipeline_has_stage(const struct radv_graphics_pipeline *pipeline, gl_shader_stage stage)
{
   return pipeline->base.shaders[stage];
}

static inline uint32_t
radv_conv_prim_to_gs_out(uint32_t topology, bool is_ngg)
{
   switch (topology) {
   case V_008958_DI_PT_POINTLIST:
   case V_008958_DI_PT_PATCH:
      return V_028A6C_POINTLIST;
   case V_008958_DI_PT_LINELIST:
   case V_008958_DI_PT_LINESTRIP:
   case V_008958_DI_PT_LINELIST_ADJ:
   case V_008958_DI_PT_LINESTRIP_ADJ:
      return V_028A6C_LINESTRIP;
   case V_008958_DI_PT_TRILIST:
   case V_008958_DI_PT_TRISTRIP:
   case V_008958_DI_PT_TRIFAN:
   case V_008958_DI_PT_TRILIST_ADJ:
   case V_008958_DI_PT_TRISTRIP_ADJ:
      return V_028A6C_TRISTRIP;
   case V_008958_DI_PT_RECTLIST:
      return is_ngg ? V_028A6C_RECTLIST : V_028A6C_TRISTRIP;
   default:
      assert(0);
      return 0;
   }
}

static inline uint32_t
radv_translate_prim(unsigned topology)
{
   switch (topology) {
   case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
      return V_008958_DI_PT_POINTLIST;
   case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
      return V_008958_DI_PT_LINELIST;
   case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
      return V_008958_DI_PT_LINESTRIP;
   case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
      return V_008958_DI_PT_TRILIST;
   case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
      return V_008958_DI_PT_TRISTRIP;
   case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
      return V_008958_DI_PT_TRIFAN;
   case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
      return V_008958_DI_PT_LINELIST_ADJ;
   case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
      return V_008958_DI_PT_LINESTRIP_ADJ;
   case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
      return V_008958_DI_PT_TRILIST_ADJ;
   case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
      return V_008958_DI_PT_TRISTRIP_ADJ;
   case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
      return V_008958_DI_PT_PATCH;
   default:
      unreachable("unhandled primitive type");
   }
}

static inline bool
radv_prim_is_points_or_lines(unsigned topology)
{
   switch (topology) {
   case V_008958_DI_PT_POINTLIST:
   case V_008958_DI_PT_LINELIST:
   case V_008958_DI_PT_LINESTRIP:
   case V_008958_DI_PT_LINELIST_ADJ:
   case V_008958_DI_PT_LINESTRIP_ADJ:
      return true;
   default:
      return false;
   }
}

static inline bool
radv_rast_prim_is_point(unsigned rast_prim)
{
   return rast_prim == V_028A6C_POINTLIST;
}

static inline bool
radv_rast_prim_is_line(unsigned rast_prim)
{
   return rast_prim == V_028A6C_LINESTRIP;
}

static inline bool
radv_rast_prim_is_points_or_lines(unsigned rast_prim)
{
   return radv_rast_prim_is_point(rast_prim) || radv_rast_prim_is_line(rast_prim);
}

static inline bool
radv_polygon_mode_is_point(unsigned polygon_mode)
{
   return polygon_mode == V_028814_X_DRAW_POINTS;
}

static inline bool
radv_polygon_mode_is_line(unsigned polygon_mode)
{
   return polygon_mode == V_028814_X_DRAW_LINES;
}

static inline bool
radv_polygon_mode_is_points_or_lines(unsigned polygon_mode)
{
   return radv_polygon_mode_is_point(polygon_mode) || radv_polygon_mode_is_line(polygon_mode);
}

static inline bool
radv_primitive_topology_is_line_list(unsigned primitive_topology)
{
   return primitive_topology == V_008958_DI_PT_LINELIST || primitive_topology == V_008958_DI_PT_LINELIST_ADJ;
}

static inline unsigned
radv_get_num_vertices_per_prim(const struct radv_graphics_state_key *gfx_state)
{
   if (gfx_state->ia.topology == V_008958_DI_PT_NONE) {
      /* When the topology is unknown (with graphics pipeline library), return the maximum number of
       * vertices per primitives for VS. This is used to lower NGG (the HW will ignore the extra
       * bits for points/lines) and also to enable NGG culling unconditionally (it will be disabled
       * dynamically for points/lines).
       */
      return 3;
   } else {
      /* Need to add 1, because: V_028A6C_POINTLIST=0, V_028A6C_LINESTRIP=1, V_028A6C_TRISTRIP=2, etc. */
      return radv_conv_prim_to_gs_out(gfx_state->ia.topology, false) + 1;
   }
}

static inline uint32_t
radv_translate_fill(VkPolygonMode func)
{
   switch (func) {
   case VK_POLYGON_MODE_FILL:
      return V_028814_X_DRAW_TRIANGLES;
   case VK_POLYGON_MODE_LINE:
      return V_028814_X_DRAW_LINES;
   case VK_POLYGON_MODE_POINT:
      return V_028814_X_DRAW_POINTS;
   default:
      assert(0);
      return V_028814_X_DRAW_POINTS;
   }
}

static inline uint32_t
radv_translate_stencil_op(enum VkStencilOp op)
{
   switch (op) {
   case VK_STENCIL_OP_KEEP:
      return V_02842C_STENCIL_KEEP;
   case VK_STENCIL_OP_ZERO:
      return V_02842C_STENCIL_ZERO;
   case VK_STENCIL_OP_REPLACE:
      return V_02842C_STENCIL_REPLACE_TEST;
   case VK_STENCIL_OP_INCREMENT_AND_CLAMP:
      return V_02842C_STENCIL_ADD_CLAMP;
   case VK_STENCIL_OP_DECREMENT_AND_CLAMP:
      return V_02842C_STENCIL_SUB_CLAMP;
   case VK_STENCIL_OP_INVERT:
      return V_02842C_STENCIL_INVERT;
   case VK_STENCIL_OP_INCREMENT_AND_WRAP:
      return V_02842C_STENCIL_ADD_WRAP;
   case VK_STENCIL_OP_DECREMENT_AND_WRAP:
      return V_02842C_STENCIL_SUB_WRAP;
   default:
      return 0;
   }
}

static inline uint32_t
radv_translate_blend_logic_op(VkLogicOp op)
{
   switch (op) {
   case VK_LOGIC_OP_CLEAR:
      return V_028808_ROP3_CLEAR;
   case VK_LOGIC_OP_AND:
      return V_028808_ROP3_AND;
   case VK_LOGIC_OP_AND_REVERSE:
      return V_028808_ROP3_AND_REVERSE;
   case VK_LOGIC_OP_COPY:
      return V_028808_ROP3_COPY;
   case VK_LOGIC_OP_AND_INVERTED:
      return V_028808_ROP3_AND_INVERTED;
   case VK_LOGIC_OP_NO_OP:
      return V_028808_ROP3_NO_OP;
   case VK_LOGIC_OP_XOR:
      return V_028808_ROP3_XOR;
   case VK_LOGIC_OP_OR:
      return V_028808_ROP3_OR;
   case VK_LOGIC_OP_NOR:
      return V_028808_ROP3_NOR;
   case VK_LOGIC_OP_EQUIVALENT:
      return V_028808_ROP3_EQUIVALENT;
   case VK_LOGIC_OP_INVERT:
      return V_028808_ROP3_INVERT;
   case VK_LOGIC_OP_OR_REVERSE:
      return V_028808_ROP3_OR_REVERSE;
   case VK_LOGIC_OP_COPY_INVERTED:
      return V_028808_ROP3_COPY_INVERTED;
   case VK_LOGIC_OP_OR_INVERTED:
      return V_028808_ROP3_OR_INVERTED;
   case VK_LOGIC_OP_NAND:
      return V_028808_ROP3_NAND;
   case VK_LOGIC_OP_SET:
      return V_028808_ROP3_SET;
   default:
      unreachable("Unhandled logic op");
   }
}

static inline uint32_t
radv_translate_blend_function(VkBlendOp op)
{
   switch (op) {
   case VK_BLEND_OP_ADD:
      return V_028780_COMB_DST_PLUS_SRC;
   case VK_BLEND_OP_SUBTRACT:
      return V_028780_COMB_SRC_MINUS_DST;
   case VK_BLEND_OP_REVERSE_SUBTRACT:
      return V_028780_COMB_DST_MINUS_SRC;
   case VK_BLEND_OP_MIN:
      return V_028780_COMB_MIN_DST_SRC;
   case VK_BLEND_OP_MAX:
      return V_028780_COMB_MAX_DST_SRC;
   default:
      return 0;
   }
}

static inline uint32_t
radv_translate_blend_factor(enum amd_gfx_level gfx_level, VkBlendFactor factor)
{
   switch (factor) {
   case VK_BLEND_FACTOR_ZERO:
      return V_028780_BLEND_ZERO;
   case VK_BLEND_FACTOR_ONE:
      return V_028780_BLEND_ONE;
   case VK_BLEND_FACTOR_SRC_COLOR:
      return V_028780_BLEND_SRC_COLOR;
   case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
      return V_028780_BLEND_ONE_MINUS_SRC_COLOR;
   case VK_BLEND_FACTOR_DST_COLOR:
      return V_028780_BLEND_DST_COLOR;
   case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
      return V_028780_BLEND_ONE_MINUS_DST_COLOR;
   case VK_BLEND_FACTOR_SRC_ALPHA:
      return V_028780_BLEND_SRC_ALPHA;
   case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
      return V_028780_BLEND_ONE_MINUS_SRC_ALPHA;
   case VK_BLEND_FACTOR_DST_ALPHA:
      return V_028780_BLEND_DST_ALPHA;
   case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
      return V_028780_BLEND_ONE_MINUS_DST_ALPHA;
   case VK_BLEND_FACTOR_CONSTANT_COLOR:
      return gfx_level >= GFX11 ? V_028780_BLEND_CONSTANT_COLOR_GFX11 : V_028780_BLEND_CONSTANT_COLOR_GFX6;
   case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR:
      return gfx_level >= GFX11 ? V_028780_BLEND_ONE_MINUS_CONSTANT_COLOR_GFX11
                                : V_028780_BLEND_ONE_MINUS_CONSTANT_COLOR_GFX6;
   case VK_BLEND_FACTOR_CONSTANT_ALPHA:
      return gfx_level >= GFX11 ? V_028780_BLEND_CONSTANT_ALPHA_GFX11 : V_028780_BLEND_CONSTANT_ALPHA_GFX6;
   case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:
      return gfx_level >= GFX11 ? V_028780_BLEND_ONE_MINUS_CONSTANT_ALPHA_GFX11
                                : V_028780_BLEND_ONE_MINUS_CONSTANT_ALPHA_GFX6;
   case VK_BLEND_FACTOR_SRC_ALPHA_SATURATE:
      return V_028780_BLEND_SRC_ALPHA_SATURATE;
   case VK_BLEND_FACTOR_SRC1_COLOR:
      return gfx_level >= GFX11 ? V_028780_BLEND_SRC1_COLOR_GFX11 : V_028780_BLEND_SRC1_COLOR_GFX6;
   case VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR:
      return gfx_level >= GFX11 ? V_028780_BLEND_INV_SRC1_COLOR_GFX11 : V_028780_BLEND_INV_SRC1_COLOR_GFX6;
   case VK_BLEND_FACTOR_SRC1_ALPHA:
      return gfx_level >= GFX11 ? V_028780_BLEND_SRC1_ALPHA_GFX11 : V_028780_BLEND_SRC1_ALPHA_GFX6;
   case VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA:
      return gfx_level >= GFX11 ? V_028780_BLEND_INV_SRC1_ALPHA_GFX11 : V_028780_BLEND_INV_SRC1_ALPHA_GFX6;
   default:
      return 0;
   }
}

static inline uint32_t
radv_translate_blend_opt_factor(VkBlendFactor factor, bool is_alpha)
{
   switch (factor) {
   case VK_BLEND_FACTOR_ZERO:
      return V_028760_BLEND_OPT_PRESERVE_NONE_IGNORE_ALL;
   case VK_BLEND_FACTOR_ONE:
      return V_028760_BLEND_OPT_PRESERVE_ALL_IGNORE_NONE;
   case VK_BLEND_FACTOR_SRC_COLOR:
      return is_alpha ? V_028760_BLEND_OPT_PRESERVE_A1_IGNORE_A0 : V_028760_BLEND_OPT_PRESERVE_C1_IGNORE_C0;
   case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
      return is_alpha ? V_028760_BLEND_OPT_PRESERVE_A0_IGNORE_A1 : V_028760_BLEND_OPT_PRESERVE_C0_IGNORE_C1;
   case VK_BLEND_FACTOR_SRC_ALPHA:
      return V_028760_BLEND_OPT_PRESERVE_A1_IGNORE_A0;
   case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
      return V_028760_BLEND_OPT_PRESERVE_A0_IGNORE_A1;
   case VK_BLEND_FACTOR_SRC_ALPHA_SATURATE:
      return is_alpha ? V_028760_BLEND_OPT_PRESERVE_ALL_IGNORE_NONE : V_028760_BLEND_OPT_PRESERVE_NONE_IGNORE_A0;
   default:
      return V_028760_BLEND_OPT_PRESERVE_NONE_IGNORE_NONE;
   }
}

static inline uint32_t
radv_translate_blend_opt_function(VkBlendOp op)
{
   switch (op) {
   case VK_BLEND_OP_ADD:
      return V_028760_OPT_COMB_ADD;
   case VK_BLEND_OP_SUBTRACT:
      return V_028760_OPT_COMB_SUBTRACT;
   case VK_BLEND_OP_REVERSE_SUBTRACT:
      return V_028760_OPT_COMB_REVSUBTRACT;
   case VK_BLEND_OP_MIN:
      return V_028760_OPT_COMB_MIN;
   case VK_BLEND_OP_MAX:
      return V_028760_OPT_COMB_MAX;
   default:
      return V_028760_OPT_COMB_BLEND_DISABLED;
   }
}

static inline bool
radv_blend_factor_uses_dst(VkBlendFactor factor)
{
   return factor == VK_BLEND_FACTOR_DST_COLOR || factor == VK_BLEND_FACTOR_DST_ALPHA ||
          factor == VK_BLEND_FACTOR_SRC_ALPHA_SATURATE || factor == VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA ||
          factor == VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
}

static inline bool
radv_is_dual_src(VkBlendFactor factor)
{
   switch (factor) {
   case VK_BLEND_FACTOR_SRC1_COLOR:
   case VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR:
   case VK_BLEND_FACTOR_SRC1_ALPHA:
   case VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA:
      return true;
   default:
      return false;
   }
}

static ALWAYS_INLINE bool
radv_can_enable_dual_src(const struct vk_color_blend_attachment_state *att)
{
   VkBlendOp eqRGB = att->color_blend_op;
   VkBlendFactor srcRGB = att->src_color_blend_factor;
   VkBlendFactor dstRGB = att->dst_color_blend_factor;
   VkBlendOp eqA = att->alpha_blend_op;
   VkBlendFactor srcA = att->src_alpha_blend_factor;
   VkBlendFactor dstA = att->dst_alpha_blend_factor;
   bool eqRGB_minmax = eqRGB == VK_BLEND_OP_MIN || eqRGB == VK_BLEND_OP_MAX;
   bool eqA_minmax = eqA == VK_BLEND_OP_MIN || eqA == VK_BLEND_OP_MAX;

   if (!eqRGB_minmax && (radv_is_dual_src(srcRGB) || radv_is_dual_src(dstRGB)))
      return true;
   if (!eqA_minmax && (radv_is_dual_src(srcA) || radv_is_dual_src(dstA)))
      return true;
   return false;
}

static inline void
radv_normalize_blend_factor(VkBlendOp op, VkBlendFactor *src_factor, VkBlendFactor *dst_factor)
{
   if (op == VK_BLEND_OP_MIN || op == VK_BLEND_OP_MAX) {
      *src_factor = VK_BLEND_FACTOR_ONE;
      *dst_factor = VK_BLEND_FACTOR_ONE;
   }
}

bool radv_pipeline_has_ngg(const struct radv_graphics_pipeline *pipeline);

bool radv_pipeline_has_gs_copy_shader(const struct radv_pipeline *pipeline);

void radv_blend_remove_dst(VkBlendOp *func, VkBlendFactor *src_factor, VkBlendFactor *dst_factor,
                           VkBlendFactor expected_dst, VkBlendFactor replacement_src);

unsigned radv_compact_spi_shader_col_format(uint32_t spi_shader_col_format);

unsigned radv_format_meta_fs_key(struct radv_device *device, VkFormat format);

struct radv_ia_multi_vgt_param_helpers radv_compute_ia_multi_vgt_param(const struct radv_device *device,
                                                                       struct radv_shader *const *shaders);

void radv_get_viewport_xform(const VkViewport *viewport, float scale[3], float translate[3]);

struct radv_shader *radv_get_shader(struct radv_shader *const *shaders, gl_shader_stage stage);

struct radv_ps_epilog_state {
   uint8_t color_attachment_count;
   VkFormat color_attachment_formats[MAX_RTS];

   uint32_t color_write_mask;
   uint32_t color_blend_enable;

   uint32_t colors_written;
   bool mrt0_is_dual_src;
   bool export_depth;
   bool export_stencil;
   bool export_sample_mask;
   bool alpha_to_coverage_via_mrtz;
   bool alpha_to_one;
   uint8_t need_src_alpha;
};

struct radv_ps_epilog_key radv_generate_ps_epilog_key(const struct radv_device *device,
                                                      const struct radv_ps_epilog_state *state);

void radv_graphics_shaders_compile(struct radv_device *device, struct vk_pipeline_cache *cache,
                                   struct radv_shader_stage *stages, const struct radv_graphics_state_key *gfx_state,
                                   bool keep_executable_info, bool keep_statistic_info, bool is_internal,
                                   struct radv_retained_shaders *retained_shaders, bool noop_fs,
                                   struct radv_shader **shaders, struct radv_shader_binary **binaries,
                                   struct radv_shader **gs_copy_shader, struct radv_shader_binary **gs_copy_binary);

void radv_emit_blend_state(struct radeon_cmdbuf *ctx_cs, const struct radv_shader *ps, uint32_t spi_shader_col_format,
                           uint32_t cb_shader_mask);

void radv_emit_vgt_gs_mode(const struct radv_device *device, struct radeon_cmdbuf *ctx_cs,
                           const struct radv_shader *last_vgt_api_shader);

void radv_emit_vertex_shader(const struct radv_device *device, struct radeon_cmdbuf *ctx_cs, struct radeon_cmdbuf *cs,
                             const struct radv_shader *vs, const struct radv_shader *next_stage);

void radv_emit_tess_ctrl_shader(const struct radv_device *device, struct radeon_cmdbuf *cs,
                                const struct radv_shader *tcs);

void radv_emit_tess_eval_shader(const struct radv_device *device, struct radeon_cmdbuf *ctx_cs,
                                struct radeon_cmdbuf *cs, const struct radv_shader *tes, const struct radv_shader *gs);

void radv_emit_geometry_shader(const struct radv_device *device, struct radeon_cmdbuf *ctx_cs, struct radeon_cmdbuf *cs,
                               const struct radv_shader *gs, const struct radv_shader *es,
                               const struct radv_shader *gs_copy_shader);

void radv_emit_mesh_shader(const struct radv_device *device, struct radeon_cmdbuf *ctx_cs, struct radeon_cmdbuf *cs,
                           const struct radv_shader *ms);

void radv_emit_ps_inputs(const struct radv_device *device, struct radeon_cmdbuf *cs,
                         const struct radv_shader *last_vgt_shader, const struct radv_shader *ps);

void radv_emit_fragment_shader(const struct radv_device *device, struct radeon_cmdbuf *ctx_cs, struct radeon_cmdbuf *cs,
                               const struct radv_shader *ps);

struct radv_vgt_shader_key {
   uint8_t tess : 1;
   uint8_t gs : 1;
   uint8_t mesh_scratch_ring : 1;
   uint8_t mesh : 1;
   uint8_t ngg_passthrough : 1;
   uint8_t ngg : 1; /* gfx10+ */
   uint8_t ngg_streamout : 1;
   uint8_t hs_wave32 : 1;
   uint8_t gs_wave32 : 1;
   uint8_t vs_wave32 : 1;
};

void radv_emit_vgt_reuse(const struct radv_device *device, struct radeon_cmdbuf *ctx_cs, const struct radv_shader *tes,
                         const struct radv_vgt_shader_key *key);

struct radv_vgt_shader_key radv_get_vgt_shader_key(const struct radv_device *device, struct radv_shader **shaders,
                                                   const struct radv_shader *gs_copy_shader);

void radv_emit_vgt_shader_config(const struct radv_device *device, struct radeon_cmdbuf *ctx_cs,
                                 const struct radv_vgt_shader_key *key);

void radv_emit_vgt_gs_out(const struct radv_device *device, struct radeon_cmdbuf *ctx_cs,
                          uint32_t vgt_gs_out_prim_type);

void gfx103_emit_vgt_draw_payload_cntl(struct radeon_cmdbuf *ctx_cs, const struct radv_shader *mesh_shader,
                                       bool enable_vrs);

void gfx103_emit_vrs_state(const struct radv_device *device, struct radeon_cmdbuf *ctx_cs, const struct radv_shader *ps,
                           bool enable_vrs, bool enable_vrs_coarse_shading, bool force_vrs_per_vertex);

uint32_t radv_get_vgt_gs_out(struct radv_shader **shaders, uint32_t primitive_topology);

bool radv_needs_null_export_workaround(const struct radv_device *device, const struct radv_shader *ps,
                                       unsigned custom_blend_mode);

struct radv_graphics_pipeline_create_info {
   bool use_rectlist;
   bool db_depth_clear;
   bool db_stencil_clear;
   bool depth_compress_disable;
   bool stencil_compress_disable;
   bool resummarize_enable;
   uint32_t custom_blend_mode;
};

VkResult radv_graphics_pipeline_create(VkDevice device, VkPipelineCache cache,
                                       const VkGraphicsPipelineCreateInfo *pCreateInfo,
                                       const struct radv_graphics_pipeline_create_info *extra,
                                       const VkAllocationCallbacks *alloc, VkPipeline *pPipeline);

void radv_destroy_graphics_pipeline(struct radv_device *device, struct radv_graphics_pipeline *pipeline);

void radv_destroy_graphics_lib_pipeline(struct radv_device *device, struct radv_graphics_lib_pipeline *pipeline);

#endif /* RADV_PIPELINE_GRAPHICS_H */
