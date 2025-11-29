/*
 * Copyright 2023 Alyssa Rosenzweig
 * SPDX-License-Identifier: MIT
 */

#include "geometry.h"

uint
libagx_tcs_patch_vertices_in(constant struct agx_tess_params *p)
{
   return p->input_patch_size;
}

uint
libagx_tes_patch_vertices_in(constant struct agx_tess_params *p)
{
   return p->output_patch_size;
}

ushort
libagx_tcs_in_offset(uint vtx, gl_varying_slot location,
                     uint64_t crosslane_vs_out_mask)
{
   return libagx_tcs_in_offs(vtx, location, crosslane_vs_out_mask);
}

uintptr_t
libagx_tcs_out_address(constant struct agx_tess_params *p, uint patch_id,
                       uint vtx_id, gl_varying_slot location, uint nr_patch_out,
                       uint out_patch_size, uint64_t vtx_out_mask)
{
   uint stride =
      libagx_tcs_out_stride(nr_patch_out, out_patch_size, vtx_out_mask);

   uint offs = libagx_tcs_out_offs(vtx_id, location, nr_patch_out,
                                   out_patch_size, vtx_out_mask);

   return (uintptr_t)(p->tcs_buffer) + (patch_id * stride) + offs;
}

static uint
libagx_tes_unrolled_patch_id(uint raw_id)
{
   return raw_id / LIBAGX_TES_PATCH_ID_STRIDE;
}

uint
libagx_tes_patch_id(constant struct agx_tess_params *p, uint raw_id)
{
   return libagx_tes_unrolled_patch_id(raw_id) % p->patches_per_instance;
}

static uint
tes_vertex_id_in_patch(uint raw_id)
{
   return raw_id % LIBAGX_TES_PATCH_ID_STRIDE;
}

float2
libagx_load_tess_coord(constant struct agx_tess_params *p, uint raw_id)
{
   uint patch = libagx_tes_unrolled_patch_id(raw_id);
   uint vtx = tes_vertex_id_in_patch(raw_id);

   return p->patch_coord_buffer[p->patch_coord_offs[patch] + vtx];
}

uintptr_t
libagx_tes_in_address(constant struct agx_tess_params *p, uint raw_id,
                      uint vtx_id, gl_varying_slot location)
{
   uint patch = libagx_tes_unrolled_patch_id(raw_id);

   return libagx_tcs_out_address(p, patch, vtx_id, location,
                                 p->tcs_patch_constants, p->output_patch_size,
                                 p->tcs_per_vertex_outputs);
}

float4
libagx_tess_level_outer_default(constant struct agx_tess_params *p)
{
   return (
      float4)(p->tess_level_outer_default[0], p->tess_level_outer_default[1],
              p->tess_level_outer_default[2], p->tess_level_outer_default[3]);
}

float2
libagx_tess_level_inner_default(constant struct agx_tess_params *p)
{
   return (float2)(p->tess_level_inner_default[0],
                   p->tess_level_inner_default[1]);
}
