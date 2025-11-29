/*
 * Copyright 2023 Valve Corporation
 * SPDX-License-Identifier: MIT
 */

#include "asahi/compiler/agx_compile.h"
#include "compiler/nir/nir_builder.h"
#include "shaders/geometry.h"
#include "util/compiler.h"
#include "agx_nir_lower_gs.h"
#include "libagx_shaders.h"
#include "nir.h"
#include "nir_builder_opcodes.h"
#include "nir_intrinsics.h"
#include "shader_enums.h"

/*
 * This file implements input assembly in software for geometry/tessellation
 * shaders. load_vertex_id is lowered based on the topology. Most of the logic
 * lives in CL library routines.
 */

nir_def *
agx_vertex_id_for_topology_class(nir_builder *b, nir_def *vert,
                                 enum mesa_prim cls)
{
   nir_def *prim = nir_load_primitive_id(b);
   nir_def *flatshade_first = nir_ieq_imm(b, nir_load_provoking_last(b), 0);
   nir_def *nr = nir_load_num_vertices(b);
   nir_def *topology = nir_load_input_topology_agx(b);

   switch (cls) {
   case MESA_PRIM_POINTS:
      return prim;

   case MESA_PRIM_LINES:
      return libagx_vertex_id_for_line_class(b, topology, prim, vert, nr);

   case MESA_PRIM_TRIANGLES:
      return libagx_vertex_id_for_tri_class(b, topology, prim, vert,
                                            flatshade_first);

   case MESA_PRIM_LINES_ADJACENCY:
      return libagx_vertex_id_for_line_adj_class(b, topology, prim, vert);

   case MESA_PRIM_TRIANGLES_ADJACENCY:
      return libagx_vertex_id_for_tri_adj_class(b, topology, prim, vert, nr,
                                                flatshade_first);

   default:
      unreachable("invalid topology class");
   }
}

struct state {
   unsigned index_size;
   bool patches;
};

static nir_def *
load_vertex_id(nir_builder *b, struct state *state)
{
   nir_def *id = nir_load_primitive_id(b);

   if (state->patches) {
      id = nir_iadd(b, nir_imul(b, id, nir_load_patch_vertices_in(b)),
                    nir_load_invocation_id(b));
   }

   /* If drawing with an index buffer, pull the vertex ID. Otherwise, the
    * vertex ID is just the index as-is.
    */
   if (state->index_size) {
      nir_def *ia = nir_load_input_assembly_buffer_agx(b);

      nir_def *address =
         libagx_index_buffer(b, ia, id, nir_imm_int(b, state->index_size));

      nir_def *index = nir_load_global_constant(b, address, state->index_size,
                                                1, state->index_size * 8);

      id = nir_u2uN(b, index, id->bit_size);
   }

   /* Add the "start", either an index bias or a base vertex. This must happen
    * after indexing for proper index bias behaviour.
    */
   return nir_iadd(b, id, nir_load_first_vertex(b));
}

static bool
lower_vertex_id(nir_builder *b, nir_intrinsic_instr *intr, void *data)
{
   if (intr->intrinsic != nir_intrinsic_load_vertex_id)
      return false;

   b->cursor = nir_instr_remove(&intr->instr);
   assert(intr->def.bit_size == 32);
   nir_def_rewrite_uses(&intr->def, load_vertex_id(b, data));
   return true;
}

bool
agx_nir_lower_index_buffer(nir_shader *s, unsigned index_size_B, bool patches)
{
   return nir_shader_intrinsics_pass(
      s, lower_vertex_id, nir_metadata_block_index | nir_metadata_dominance,
      &(struct state){index_size_B, patches});
}
