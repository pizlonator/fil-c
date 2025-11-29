/*
 * Copyright © 2013 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "brw_eu.h"
#include "intel_nir.h"
#include "brw_nir.h"
#include "brw_fs.h"
#include "brw_private.h"
#include "dev/intel_debug.h"

/**
 * Return the number of patches to accumulate before a MULTI_PATCH mode thread is
 * launched.  In cases with a large number of input control points and a large
 * amount of VS outputs, the VS URB space needed to store an entire 8 patches
 * worth of data can be prohibitive, so it can be beneficial to launch threads
 * early.
 *
 * See the 3DSTATE_HS::Patch Count Threshold documentation for the recommended
 * values.  Note that 0 means to "disable" early dispatch, meaning to wait for
 * a full 8 patches as normal.
 */
static int
get_patch_count_threshold(int input_control_points)
{
   if (input_control_points <= 4)
      return 0;
   else if (input_control_points <= 6)
      return 5;
   else if (input_control_points <= 8)
      return 4;
   else if (input_control_points <= 10)
      return 3;
   else if (input_control_points <= 14)
      return 2;

   /* Return patch count 1 for PATCHLIST_15 - PATCHLIST_32 */
   return 1;
}

extern "C" const unsigned *
brw_compile_tcs(const struct brw_compiler *compiler,
                struct brw_compile_tcs_params *params)
{
   const struct intel_device_info *devinfo = compiler->devinfo;
   nir_shader *nir = params->base.nir;
   const struct brw_tcs_prog_key *key = params->key;
   struct brw_tcs_prog_data *prog_data = params->prog_data;
   struct brw_vue_prog_data *vue_prog_data = &prog_data->base;

   const bool debug_enabled = brw_should_print_shader(nir, DEBUG_TCS);

   vue_prog_data->base.stage = MESA_SHADER_TESS_CTRL;
   prog_data->base.base.ray_queries = nir->info.ray_queries;
   prog_data->base.base.total_scratch = 0;

   nir->info.outputs_written = key->outputs_written;
   nir->info.patch_outputs_written = key->patch_outputs_written;

   struct intel_vue_map input_vue_map;
   brw_compute_vue_map(devinfo, &input_vue_map, nir->info.inputs_read,
                       nir->info.separate_shader, 1);
   brw_compute_tess_vue_map(&vue_prog_data->vue_map,
                            nir->info.outputs_written,
                            nir->info.patch_outputs_written);

   brw_nir_apply_key(nir, compiler, &key->base, 8);
   brw_nir_lower_vue_inputs(nir, &input_vue_map);
   brw_nir_lower_tcs_outputs(nir, &vue_prog_data->vue_map,
                             key->_tes_primitive_mode);
   if (key->input_vertices > 0)
      intel_nir_lower_patch_vertices_in(nir, key->input_vertices);

   brw_postprocess_nir(nir, compiler, debug_enabled,
                       key->base.robust_flags);

   bool has_primitive_id =
      BITSET_TEST(nir->info.system_values_read, SYSTEM_VALUE_PRIMITIVE_ID);

   prog_data->patch_count_threshold = get_patch_count_threshold(key->input_vertices);

   if (compiler->use_tcs_multi_patch) {
      vue_prog_data->dispatch_mode = INTEL_DISPATCH_MODE_TCS_MULTI_PATCH;
      prog_data->instances = nir->info.tess.tcs_vertices_out;
      prog_data->include_primitive_id = has_primitive_id;
   } else {
      unsigned verts_per_thread = 8;
      vue_prog_data->dispatch_mode = INTEL_DISPATCH_MODE_TCS_SINGLE_PATCH;
      prog_data->instances =
         DIV_ROUND_UP(nir->info.tess.tcs_vertices_out, verts_per_thread);
   }

   /* Compute URB entry size.  The maximum allowed URB entry size is 32k.
    * That divides up as follows:
    *
    *     32 bytes for the patch header (tessellation factors)
    *    480 bytes for per-patch varyings (a varying component is 4 bytes and
    *              gl_MaxTessPatchComponents = 120)
    *  16384 bytes for per-vertex varyings (a varying component is 4 bytes,
    *              gl_MaxPatchVertices = 32 and
    *              gl_MaxTessControlOutputComponents = 128)
    *
    *  15808 bytes left for varying packing overhead
    */
   const int num_per_patch_slots = vue_prog_data->vue_map.num_per_patch_slots;
   const int num_per_vertex_slots = vue_prog_data->vue_map.num_per_vertex_slots;
   unsigned output_size_bytes = 0;
   /* Note that the patch header is counted in num_per_patch_slots. */
   output_size_bytes += num_per_patch_slots * 16;
   output_size_bytes += nir->info.tess.tcs_vertices_out *
                        num_per_vertex_slots * 16;

   assert(output_size_bytes >= 1);
   if (output_size_bytes > GFX7_MAX_HS_URB_ENTRY_SIZE_BYTES)
      return NULL;

   /* URB entry sizes are stored as a multiple of 64 bytes. */
   vue_prog_data->urb_entry_size = ALIGN(output_size_bytes, 64) / 64;

   /* HS does not use the usual payload pushing from URB to GRFs,
    * because we don't have enough registers for a full-size payload, and
    * the hardware is broken on Haswell anyway.
    */
   vue_prog_data->urb_read_length = 0;

   if (unlikely(debug_enabled)) {
      fprintf(stderr, "TCS Input ");
      brw_print_vue_map(stderr, &input_vue_map, MESA_SHADER_TESS_CTRL);
      fprintf(stderr, "TCS Output ");
      brw_print_vue_map(stderr, &vue_prog_data->vue_map, MESA_SHADER_TESS_CTRL);
   }

   const unsigned dispatch_width = devinfo->ver >= 20 ? 16 : 8;
   fs_visitor v(compiler, &params->base, &key->base,
                &prog_data->base.base, nir, dispatch_width,
                params->base.stats != NULL, debug_enabled);
   if (!v.run_tcs()) {
      params->base.error_str =
         ralloc_strdup(params->base.mem_ctx, v.fail_msg);
      return NULL;
   }

   assert(v.payload().num_regs % reg_unit(devinfo) == 0);
   prog_data->base.base.dispatch_grf_start_reg = v.payload().num_regs / reg_unit(devinfo);

   fs_generator g(compiler, &params->base,
                  &prog_data->base.base, MESA_SHADER_TESS_CTRL);
   if (unlikely(debug_enabled)) {
      g.enable_debug(ralloc_asprintf(params->base.mem_ctx,
                                     "%s tessellation control shader %s",
                                     nir->info.label ? nir->info.label
                                                     : "unnamed",
                                     nir->info.name));
   }

   g.generate_code(v.cfg, dispatch_width, v.shader_stats,
                   v.performance_analysis.require(), params->base.stats);

   g.add_const_data(nir->constant_data, nir->constant_data_size);

   return g.get_assembly();
}
