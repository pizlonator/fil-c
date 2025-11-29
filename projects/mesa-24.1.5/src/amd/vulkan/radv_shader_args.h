/*
 * Copyright © 2019 Valve Corporation.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_SHADER_ARGS_H
#define RADV_SHADER_ARGS_H

#include "compiler/shader_enums.h"
#include "util/list.h"
#include "util/macros.h"
#include "ac_shader_args.h"
#include "amd_family.h"
#include "radv_constants.h"

enum radv_ud_index {
   AC_UD_SCRATCH_RING_OFFSETS = 0,
   AC_UD_PUSH_CONSTANTS = 1,
   AC_UD_INLINE_PUSH_CONSTANTS = 2,
   AC_UD_INDIRECT_DESCRIPTOR_SETS = 3,
   AC_UD_VIEW_INDEX = 4,
   AC_UD_STREAMOUT_BUFFERS = 5,
   AC_UD_SHADER_QUERY_STATE = 6,
   AC_UD_NGG_PROVOKING_VTX = 7,
   AC_UD_NGG_CULLING_SETTINGS = 8,
   AC_UD_NGG_VIEWPORT = 9,
   AC_UD_NGG_LDS_LAYOUT = 10,
   AC_UD_VGT_ESGS_RING_ITEMSIZE = 11,
   AC_UD_FORCE_VRS_RATES = 12,
   AC_UD_TASK_RING_ENTRY = 13,
   AC_UD_NUM_VERTS_PER_PRIM = 14,
   AC_UD_NEXT_STAGE_PC = 15,
   AC_UD_EPILOG_PC = 16,
   AC_UD_SHADER_START = 17,
   AC_UD_VS_VERTEX_BUFFERS = AC_UD_SHADER_START,
   AC_UD_VS_BASE_VERTEX_START_INSTANCE,
   AC_UD_VS_PROLOG_INPUTS,
   AC_UD_VS_MAX_UD,
   AC_UD_PS_STATE,
   AC_UD_PS_MAX_UD,
   AC_UD_CS_GRID_SIZE = AC_UD_SHADER_START,
   AC_UD_CS_SBT_DESCRIPTORS,
   AC_UD_CS_RAY_LAUNCH_SIZE_ADDR,
   AC_UD_CS_RAY_DYNAMIC_CALLABLE_STACK_BASE,
   AC_UD_CS_TRAVERSAL_SHADER_ADDR,
   AC_UD_CS_TASK_RING_OFFSETS,
   AC_UD_CS_TASK_DRAW_ID,
   AC_UD_CS_TASK_IB,
   AC_UD_CS_MAX_UD,
   AC_UD_GS_MAX_UD,
   AC_UD_TCS_OFFCHIP_LAYOUT = AC_UD_VS_MAX_UD,
   AC_UD_TCS_MAX_UD,
   /* We might not know the previous stage when compiling a geometry shader, so we just
    * declare both TES and VS user SGPRs.
    */
   AC_UD_TES_MAX_UD = AC_UD_TCS_MAX_UD,
   AC_UD_MAX_UD = AC_UD_CS_MAX_UD,
};

struct radv_userdata_info {
   int8_t sgpr_idx;
   uint8_t num_sgprs;
};

struct radv_userdata_locations {
   struct radv_userdata_info descriptor_sets[MAX_SETS];
   struct radv_userdata_info shader_data[AC_UD_MAX_UD];
   uint32_t descriptor_sets_enabled;
};

struct radv_shader_args {
   struct ac_shader_args ac;

   struct ac_arg descriptor_sets[MAX_SETS];
   /* User data 2/3. same as ring_offsets but for task shaders. */
   struct ac_arg task_ring_offsets;

   /* Streamout */
   struct ac_arg streamout_buffers;

   /* Emulated query */
   struct ac_arg shader_query_state;

   /* NGG */
   struct ac_arg ngg_provoking_vtx;
   struct ac_arg ngg_lds_layout;

   /* NGG GS */
   struct ac_arg ngg_culling_settings;
   struct ac_arg ngg_viewport_scale[2];
   struct ac_arg ngg_viewport_translate[2];

   /* Fragment shaders */
   struct ac_arg ps_state;

   struct ac_arg prolog_inputs;
   struct ac_arg vs_inputs[MAX_VERTEX_ATTRIBS];

   /* PS epilogs */
   struct ac_arg colors[MAX_RTS];
   struct ac_arg depth;
   struct ac_arg stencil;
   struct ac_arg sample_mask;

   /* TCS */
   /* # [0:6] = the number of tessellation patches minus one, max = 127
    * # [7:11] = the number of output patch control points minus one, max = 31
    * # [12:16] = the number of input patch control points minus one, max = 31
    * # [17:22] = the number of LS outputs, up to 32
    * # [23:28] = the number of HS per-vertex outputs, up to 32
    * # [29:30] = tess_primitive_mode
    * # [31] = whether TES reads tess factors
    */
   struct ac_arg tcs_offchip_layout;

   /* GS */
   struct ac_arg vgt_esgs_ring_itemsize;

   /* NGG VS streamout */
   struct ac_arg num_verts_per_prim;

   /* For non-monolithic VS or TES on GFX9+. */
   struct ac_arg next_stage_pc;

   /* PS/TCS epilogs PC. */
   struct ac_arg epilog_pc;

   struct radv_userdata_locations user_sgprs_locs;
   unsigned num_user_sgprs;

   bool explicit_scratch_args;
   bool remap_spi_ps_input;
   bool load_grid_size_from_user_sgpr;
};

static inline struct radv_shader_args *
radv_shader_args_from_ac(struct ac_shader_args *args)
{
   return container_of(args, struct radv_shader_args, ac);
}

struct radv_graphics_state_key;
struct radv_shader_info;
struct radv_ps_epilog_key;
struct radv_device;

void radv_declare_shader_args(const struct radv_device *device, const struct radv_graphics_state_key *gfx_state,
                              const struct radv_shader_info *info, gl_shader_stage stage,
                              gl_shader_stage previous_stage, struct radv_shader_args *args);

void radv_declare_ps_epilog_args(const struct radv_device *device, const struct radv_ps_epilog_key *key,
                                 struct radv_shader_args *args);

void radv_declare_rt_shader_args(enum amd_gfx_level gfx_level, struct radv_shader_args *args);

#endif /* RADV_SHADER_ARGS_H */
