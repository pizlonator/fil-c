/*
 * Copyright 2023 Alyssa Rosenzweig
 * Copyright 2023 Valve Corporation
 * SPDX-License-Identifier: MIT
 */

#include "compiler/shader_enums.h"
#include "libagx.h"

#ifndef __OPENCL_VERSION__
#include "util/bitscan.h"
#define CONST(type_)       uint64_t
#define libagx_popcount(x) util_bitcount64(x)
#else
#define CONST(type_)       constant type_ *
#define libagx_popcount(x) popcount(x)
#endif

#ifndef LIBAGX_GEOMETRY_H
#define LIBAGX_GEOMETRY_H

#define MAX_SO_BUFFERS     4
#define MAX_VERTEX_STREAMS 4

/* Packed geometry state buffer */
struct agx_geometry_state {
   /* Heap to allocate from, in either direction. By convention, the top is used
    * for intra-draw allocations and the bottom is used for full-batch
    * allocations. In the future we could use kernel support to improve this.
    */
   GLOBAL(uchar) heap;
   uint32_t heap_bottom, heap_top, heap_size, padding;
} PACKED;
AGX_STATIC_ASSERT(sizeof(struct agx_geometry_state) == 6 * 4);

struct agx_ia_state {
   /* Heap to allocate from across draws */
   GLOBAL(struct agx_geometry_state) heap;

   /* Input: index buffer if present. */
   CONST(uchar) index_buffer;

   /* Input: draw count */
   CONST(uint) count;

   /* Input: indirect draw descriptor. Raw pointer since it's strided. */
   uint64_t draws;

   /* For the geom/tess path, this is the temporary prefix sum buffer.
    * Caller-allocated. For regular MDI, this is ok since the CPU knows the
    * worst-case draw count.
    */
   GLOBAL(uint) prefix_sums;

   /* When unrolling primitive restart, output draw descriptors */
   GLOBAL(uint) out_draws;

   /* Input: maximum draw count, count is clamped to this */
   uint32_t max_draws;

   /* Primitive restart index, if unrolling */
   uint32_t restart_index;

   /* Input index buffer size in bytes, if unrolling */
   uint32_t index_buffer_size_B;

   /* Stride for the draw descrptor array */
   uint32_t draw_stride;

   /* When unrolling primitive restart, use first vertex as the provoking vertex
    * for flat shading. We could stick this in the key, but meh, you're already
    * hosed for perf on the unroll path.
    */
   uint32_t flatshade_first;

   /* The index size (1, 2, 4) or 0 if drawing without an index buffer. */
   uint32_t index_size_B;
} PACKED;
AGX_STATIC_ASSERT(sizeof(struct agx_ia_state) == 18 * 4);

struct agx_geometry_params {
   /* Persistent (cross-draw) geometry state */
   GLOBAL(struct agx_geometry_state) state;

   /* Address of associated indirect draw buffer */
   GLOBAL(uint) indirect_desc;

   /* Address of count buffer. For an indirect draw, this will be written by the
    * indirect setup kernel.
    */
   GLOBAL(uint) count_buffer;

   /* Address of the primitives generated counters */
   GLOBAL(uint) prims_generated_counter[MAX_VERTEX_STREAMS];
   GLOBAL(uint) xfb_prims_generated_counter[MAX_VERTEX_STREAMS];
   GLOBAL(uint) xfb_overflow[MAX_VERTEX_STREAMS];
   GLOBAL(uint) xfb_any_overflow;

   /* Pointers to transform feedback buffer offsets in bytes */
   GLOBAL(uint) xfb_offs_ptrs[MAX_SO_BUFFERS];

   /* Output index buffer, allocated by pre-GS. */
   GLOBAL(uint) output_index_buffer;

   /* Address of transform feedback buffer in general, supplied by the CPU. */
   GLOBAL(uchar) xfb_base_original[MAX_SO_BUFFERS];

   /* Address of transform feedback for the current primitive. Written by pre-GS
    * program.
    */
   GLOBAL(uchar) xfb_base[MAX_SO_BUFFERS];

   /* Location-indexed mask of flat outputs, used for lowering GL edge flags. */
   uint64_t flat_outputs;

   uint32_t xfb_size[MAX_SO_BUFFERS];

   /* Number of primitives emitted by transform feedback per stream. Written by
    * the pre-GS program.
    */
   uint32_t xfb_prims[MAX_VERTEX_STREAMS];

   /* Within an indirect GS draw, the grids used to dispatch the VS/GS written
    * out by the GS indirect setup kernel. Unused for direct GS draws.
    */
   uint32_t vs_grid[3];
   uint32_t gs_grid[3];

   /* Number of input vertices, part of the stride for the vertex buffer */
   uint32_t input_vertices;

   /* Number of input primitives across all instances, calculated by the CPU for
    * a direct draw or the GS indirect setup kernel for an indirect draw.
    */
   uint32_t input_primitives;

   /* Number of input primitives per instance, rounded up to a power-of-two and
    * with the base-2 log taken. This is used to partition the output vertex IDs
    * efficiently.
    */
   uint32_t primitives_log2;

   /* Number of bytes output by the GS count shader per input primitive (may be
    * 0), written by CPU and consumed by indirect draw setup shader for
    * allocating counts.
    */
   uint32_t count_buffer_stride;

   /* Dynamic input topology. Must be compatible with the geometry shader's
    * layout() declared input class.
    */
   uint32_t input_topology;
} PACKED;
AGX_STATIC_ASSERT(sizeof(struct agx_geometry_params) == 79 * 4);

struct agx_tess_params {
   /* Persistent (cross-draw) geometry state */
   GLOBAL(struct agx_geometry_state) state;

   /* Patch coordinate offsets in patch_coord_buffer, indexed by patch ID. */
   GLOBAL(uint) patch_coord_offs;

   /* Patch coordinate buffer, indexed as:
    *
    *    patch_coord_offs[patch_ID] + vertex_in_patch
    *
    * Currently float2s, but we might be able to compact later?
    */
   GLOBAL(float2) patch_coord_buffer;

   /* Tessellation control shader output buffer, indexed by patch ID. */
   GLOBAL(uchar) tcs_buffer;

   /* Bitfield of TCS per-vertex outputs */
   uint64_t tcs_per_vertex_outputs;

   /* Default tess levels used in OpenGL when there is no TCS in the pipeline.
    * Unused in Vulkan and OpenGL ES.
    */
   float tess_level_outer_default[4];
   float tess_level_inner_default[4];

   /* Number of vertices in the input patch */
   uint input_patch_size;

   /* Number of vertices in the TCS output patch */
   uint output_patch_size;

   /* Number of patch constants written by TCS */
   uint tcs_patch_constants;

   /* Number of input patches per instance of the VS/TCS */
   uint patches_per_instance;
} PACKED;
AGX_STATIC_ASSERT(sizeof(struct agx_tess_params) == 22 * 4);

/* TCS shared memory layout:
 *
 *    vec4 vs_outputs[VERTICES_IN_INPUT_PATCH][TOTAL_VERTEX_OUTPUTS];
 *
 * TODO: compact.
 */
static inline uint
libagx_tcs_in_offs(uint vtx, gl_varying_slot location,
                   uint64_t crosslane_vs_out_mask)
{
   uint base = vtx * libagx_popcount(crosslane_vs_out_mask);
   uint offs = libagx_popcount(crosslane_vs_out_mask &
                               (((uint64_t)(1) << location) - 1));

   return (base + offs) * 16;
}

static inline uint
libagx_tcs_in_size(uint32_t vertices_in_patch, uint64_t crosslane_vs_out_mask)
{
   return vertices_in_patch * libagx_popcount(crosslane_vs_out_mask) * 16;
}

/*
 * TCS out buffer layout, per-patch:
 *
 *    float tess_level_outer[4];
 *    float tess_level_inner[2];
 *    vec4 patch_out[MAX_PATCH_OUTPUTS];
 *    vec4 vtx_out[OUT_PATCH_SIZE][TOTAL_VERTEX_OUTPUTS];
 *
 * Vertex out are compacted based on the mask of written out. Patch
 * out are used as-is.
 *
 * Bounding boxes are ignored.
 */
static inline uint
libagx_tcs_out_offs(uint vtx_id, gl_varying_slot location, uint nr_patch_out,
                    uint out_patch_size, uint64_t vtx_out_mask)
{
   uint off = 0;
   if (location == VARYING_SLOT_TESS_LEVEL_OUTER)
      return off;

   off += 4 * sizeof(float);
   if (location == VARYING_SLOT_TESS_LEVEL_INNER)
      return off;

   off += 2 * sizeof(float);
   if (location >= VARYING_SLOT_PATCH0)
      return off + (16 * (location - VARYING_SLOT_PATCH0));

   /* Anything else is a per-vtx output */
   off += 16 * nr_patch_out;
   off += 16 * vtx_id * libagx_popcount(vtx_out_mask);

   uint idx = libagx_popcount(vtx_out_mask & (((uint64_t)(1) << location) - 1));
   return off + (16 * idx);
}

static inline uint
libagx_tcs_out_stride(uint nr_patch_out, uint out_patch_size,
                      uint64_t vtx_out_mask)
{
   return libagx_tcs_out_offs(out_patch_size, VARYING_SLOT_VAR0, nr_patch_out,
                              out_patch_size, vtx_out_mask);
}

/* In a tess eval shader, stride for hw vertex ID */
#define LIBAGX_TES_PATCH_ID_STRIDE 8192

#endif
