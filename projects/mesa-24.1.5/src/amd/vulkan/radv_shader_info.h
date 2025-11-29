/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_SHADER_INFO_H
#define RADV_SHADER_INFO_H

#include <inttypes.h>
#include <stdbool.h>

#include "radv_constants.h"
#include "radv_shader_args.h"

struct radv_device;
struct nir_shader;
struct radv_shader_layout;
struct radv_shader_stage_key;
enum radv_pipeline_type;
struct radv_shader_stage;

enum radv_shader_type {
   RADV_SHADER_TYPE_DEFAULT = 0,
   RADV_SHADER_TYPE_GS_COPY,
   RADV_SHADER_TYPE_TRAP_HANDLER,
};

struct radv_vs_output_info {
   uint8_t vs_output_param_offset[VARYING_SLOT_MAX];
   uint8_t clip_dist_mask;
   uint8_t cull_dist_mask;
   uint8_t param_exports;
   uint8_t prim_param_exports;
   bool writes_pointsize;
   bool writes_layer;
   bool writes_layer_per_primitive;
   bool writes_viewport_index;
   bool writes_viewport_index_per_primitive;
   bool writes_primitive_shading_rate;
   bool writes_primitive_shading_rate_per_primitive;
   bool export_prim_id;
   unsigned pos_exports;
};

struct radv_streamout_info {
   uint16_t num_outputs;
   uint16_t strides[MAX_SO_BUFFERS];
   uint32_t enabled_stream_buffers_mask;
};

struct radv_legacy_gs_info {
   uint32_t vgt_gs_onchip_cntl;
   uint32_t vgt_gs_max_prims_per_subgroup;
   uint32_t vgt_esgs_ring_itemsize;
   uint32_t lds_size;
   uint32_t esgs_ring_size;
   uint32_t gsvs_ring_size;
};

struct gfx10_ngg_info {
   uint16_t ngg_emit_size; /* in dwords */
   uint32_t hw_max_esverts;
   uint32_t max_gsprims;
   uint32_t max_out_verts;
   uint32_t prim_amp_factor;
   uint32_t vgt_esgs_ring_itemsize;
   uint32_t esgs_ring_size;
   uint32_t scratch_lds_base;
   uint32_t lds_size;
   bool max_vert_out_per_gs_instance;
};

struct radv_shader_info {
   uint64_t inline_push_constant_mask;
   bool can_inline_all_push_constants;
   bool loads_push_constants;
   bool loads_dynamic_offsets;
   uint32_t desc_set_used_mask;
   bool uses_view_index;
   bool uses_invocation_id;
   bool uses_prim_id;
   uint8_t wave_size;
   uint8_t ballot_bit_size;
   struct radv_userdata_locations user_sgprs_locs;
   bool is_ngg;
   bool is_ngg_passthrough;
   bool has_ngg_culling;
   bool has_ngg_early_prim_export;
   bool has_prim_query;
   bool has_xfb_query;
   uint32_t num_tess_patches;
   uint32_t esgs_itemsize; /* Only for VS or TES as ES */
   struct radv_vs_output_info outinfo;
   unsigned workgroup_size;
   bool force_vrs_per_vertex;
   gl_shader_stage stage;
   gl_shader_stage next_stage;
   enum radv_shader_type type;
   uint32_t user_data_0;
   bool inputs_linked;
   bool outputs_linked;
   bool has_epilog;                        /* Only for TCS or PS */
   bool merged_shader_compiled_separately; /* GFX9+ */

   struct {
      uint8_t output_usage_mask[VARYING_SLOT_VAR31 + 1];
      bool needs_draw_id;
      bool needs_instance_id;
      bool as_es;
      bool as_ls;
      bool tcs_in_out_eq;
      uint64_t tcs_temp_only_input_mask;
      uint8_t num_linked_outputs;
      bool needs_base_instance;
      bool use_per_attribute_vb_descs;
      uint32_t vb_desc_usage_mask;
      uint32_t input_slot_usage_mask;
      bool has_prolog;
      bool dynamic_inputs;
      bool dynamic_num_verts_per_prim;
      uint32_t num_outputs; /* For NGG streamout only */
   } vs;
   struct {
      uint8_t output_usage_mask[VARYING_SLOT_VAR31 + 1];
      uint8_t num_stream_output_components[4];
      uint8_t output_streams[VARYING_SLOT_VAR31 + 1];
      uint8_t max_stream;
      unsigned gsvs_vertex_size;
      unsigned max_gsvs_emit_size;
      unsigned vertices_in;
      unsigned vertices_out;
      unsigned input_prim;
      unsigned output_prim;
      unsigned invocations;
      unsigned es_type; /* GFX9: VS or TES */
      uint8_t num_linked_inputs;
      bool has_pipeline_stat_query;
   } gs;
   struct {
      uint8_t output_usage_mask[VARYING_SLOT_VAR31 + 1];
      bool as_es;
      enum tess_primitive_mode _primitive_mode;
      enum gl_tess_spacing spacing;
      bool ccw;
      bool point_mode;
      bool reads_tess_factors;
      unsigned tcs_vertices_out;
      uint8_t num_linked_inputs; /* Number of reserved per-vertex input slots in VRAM. */
      uint8_t num_linked_outputs;
      uint32_t num_outputs; /* For NGG streamout only */
   } tes;
   struct {
      bool uses_sample_shading;
      bool needs_sample_positions;
      bool needs_poly_line_smooth;
      bool writes_memory;
      bool writes_z;
      bool writes_stencil;
      bool writes_sample_mask;
      bool writes_mrt0_alpha;
      bool exports_mrtz_via_epilog;
      bool has_pcoord;
      bool prim_id_input;
      bool layer_input;
      bool viewport_index_input;
      uint8_t input_clips_culls_mask;
      uint32_t input_mask;
      uint32_t input_per_primitive_mask;
      uint32_t flat_shaded_mask;
      uint32_t explicit_shaded_mask;
      uint32_t explicit_strict_shaded_mask;
      uint32_t float16_shaded_mask;
      uint32_t float16_hi_shaded_mask;
      uint32_t num_interp;
      uint32_t num_prim_interp;
      bool can_discard;
      bool early_fragment_test;
      bool post_depth_coverage;
      bool reads_sample_mask_in;
      bool reads_front_face;
      bool reads_sample_id;
      bool reads_frag_shading_rate;
      bool reads_barycentric_model;
      bool reads_persp_sample;
      bool reads_persp_center;
      bool reads_persp_centroid;
      bool reads_linear_sample;
      bool reads_linear_center;
      bool reads_linear_centroid;
      bool reads_fully_covered;
      uint8_t reads_frag_coord_mask;
      uint8_t reads_sample_pos_mask;
      uint8_t depth_layout;
      bool allow_flat_shading;
      bool pops; /* Uses Primitive Ordered Pixel Shading (fragment shader interlock) */
      bool pops_is_per_sample;
      bool mrt0_is_dual_src;
      unsigned spi_ps_input;
      unsigned colors_written;
      unsigned spi_shader_col_format;
      uint8_t color0_written;
      bool load_provoking_vtx;
      bool load_rasterization_prim;
      bool force_sample_iter_shading_rate;
      uint32_t db_shader_control; /* DB_SHADER_CONTROL without intrinsic rate overrides */
   } ps;
   struct {
      bool uses_grid_size;
      bool uses_block_id[3];
      bool uses_thread_id[3];
      bool uses_local_invocation_idx;
      unsigned block_size[3];

      bool is_rt_shader;
      bool uses_dynamic_rt_callable_stack;
      bool uses_rt;
      bool uses_full_subgroups;
      bool linear_taskmesh_dispatch;
      bool has_query; /* Task shader only */

      bool regalloc_hang_bug;
   } cs;
   struct {
      uint64_t tes_inputs_read;
      uint64_t tes_patch_inputs_read;
      unsigned tcs_vertices_out;
      uint32_t num_lds_blocks;
      uint8_t num_linked_inputs;          /* Number of reserved per-vertex input slots in LDS. */
      uint8_t num_linked_outputs;         /* Number of reserved per-vertex output slots in VRAM. */
      uint8_t num_linked_patch_outputs;   /* Number of reserved per-patch output slots in VRAM. */
      uint8_t num_lds_per_vertex_outputs; /* Number of reserved per-vertex output slots in LDS. */
      uint8_t num_lds_per_patch_outputs;  /* Number of reserved per-patch output slots in LDS. */
      bool tes_reads_tess_factors : 1;
   } tcs;
   struct {
      enum mesa_prim output_prim;
      bool needs_ms_scratch_ring;
      bool has_task; /* If mesh shader is used together with a task shader. */
      bool has_query;
   } ms;

   struct radv_streamout_info so;

   struct radv_legacy_gs_info gs_ring_info;
   struct gfx10_ngg_info ngg_info;
};

void radv_nir_shader_info_init(gl_shader_stage stage, gl_shader_stage next_stage, struct radv_shader_info *info);

void radv_nir_shader_info_pass(struct radv_device *device, const struct nir_shader *nir,
                               const struct radv_shader_layout *layout, const struct radv_shader_stage_key *stage_key,
                               const struct radv_graphics_state_key *gfx_state,
                               const enum radv_pipeline_type pipeline_type, bool consider_force_vrs,
                               struct radv_shader_info *info);

void gfx10_get_ngg_info(const struct radv_device *device, struct radv_shader_info *es_info,
                        struct radv_shader_info *gs_info, struct gfx10_ngg_info *out);

void radv_nir_shader_info_link(struct radv_device *device, const struct radv_graphics_state_key *gfx_state,
                               struct radv_shader_stage *stages);

enum ac_hw_stage radv_select_hw_stage(const struct radv_shader_info *const info, const enum amd_gfx_level gfx_level);

uint64_t radv_gather_unlinked_io_mask(const uint64_t nir_mask);

uint64_t radv_gather_unlinked_patch_io_mask(const uint64_t nir_io_mask, const uint32_t nir_patch_io_mask);

#endif /* RADV_SHADER_INFO_H */
