/*
 * Copyright © 2011 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "brw_fs.h"
#include "brw_eu.h"
#include "brw_nir.h"
#include "brw_private.h"
#include "dev/intel_debug.h"

using namespace brw;

extern "C" const unsigned *
brw_compile_vs(const struct brw_compiler *compiler,
               struct brw_compile_vs_params *params)
{
   struct nir_shader *nir = params->base.nir;
   const struct brw_vs_prog_key *key = params->key;
   struct brw_vs_prog_data *prog_data = params->prog_data;
   const bool debug_enabled =
      brw_should_print_shader(nir, params->base.debug_flag ?
                                   params->base.debug_flag : DEBUG_VS);

   prog_data->base.base.stage = MESA_SHADER_VERTEX;
   prog_data->base.base.ray_queries = nir->info.ray_queries;
   prog_data->base.base.total_scratch = 0;

   brw_nir_apply_key(nir, compiler, &key->base, 8);

   prog_data->inputs_read = nir->info.inputs_read;
   prog_data->double_inputs_read = nir->info.vs.double_inputs;

   brw_nir_lower_vs_inputs(nir);
   brw_nir_lower_vue_outputs(nir);
   brw_postprocess_nir(nir, compiler, debug_enabled,
                       key->base.robust_flags);

   prog_data->base.clip_distance_mask =
      ((1 << nir->info.clip_distance_array_size) - 1);
   prog_data->base.cull_distance_mask =
      ((1 << nir->info.cull_distance_array_size) - 1) <<
      nir->info.clip_distance_array_size;

   unsigned nr_attribute_slots = util_bitcount64(prog_data->inputs_read);

   /* gl_VertexID and gl_InstanceID are system values, but arrive via an
    * incoming vertex attribute.  So, add an extra slot.
    */
   if (BITSET_TEST(nir->info.system_values_read, SYSTEM_VALUE_FIRST_VERTEX) ||
       BITSET_TEST(nir->info.system_values_read, SYSTEM_VALUE_BASE_INSTANCE) ||
       BITSET_TEST(nir->info.system_values_read, SYSTEM_VALUE_VERTEX_ID_ZERO_BASE) ||
       BITSET_TEST(nir->info.system_values_read, SYSTEM_VALUE_INSTANCE_ID)) {
      nr_attribute_slots++;
   }

   /* gl_DrawID and IsIndexedDraw share its very own vec4 */
   if (BITSET_TEST(nir->info.system_values_read, SYSTEM_VALUE_DRAW_ID) ||
       BITSET_TEST(nir->info.system_values_read, SYSTEM_VALUE_IS_INDEXED_DRAW)) {
      nr_attribute_slots++;
   }

   if (BITSET_TEST(nir->info.system_values_read, SYSTEM_VALUE_IS_INDEXED_DRAW))
      prog_data->uses_is_indexed_draw = true;

   if (BITSET_TEST(nir->info.system_values_read, SYSTEM_VALUE_FIRST_VERTEX))
      prog_data->uses_firstvertex = true;

   if (BITSET_TEST(nir->info.system_values_read, SYSTEM_VALUE_BASE_INSTANCE))
      prog_data->uses_baseinstance = true;

   if (BITSET_TEST(nir->info.system_values_read, SYSTEM_VALUE_VERTEX_ID_ZERO_BASE))
      prog_data->uses_vertexid = true;

   if (BITSET_TEST(nir->info.system_values_read, SYSTEM_VALUE_INSTANCE_ID))
      prog_data->uses_instanceid = true;

   if (BITSET_TEST(nir->info.system_values_read, SYSTEM_VALUE_DRAW_ID))
          prog_data->uses_drawid = true;

   prog_data->base.urb_read_length = DIV_ROUND_UP(nr_attribute_slots, 2);
   prog_data->nr_attribute_slots = nr_attribute_slots;

   /* Since vertex shaders reuse the same VUE entry for inputs and outputs
    * (overwriting the original contents), we need to make sure the size is
    * the larger of the two.
    */
   const unsigned vue_entries =
      MAX2(nr_attribute_slots, (unsigned)prog_data->base.vue_map.num_slots);

   prog_data->base.urb_entry_size = DIV_ROUND_UP(vue_entries, 4);

   if (unlikely(debug_enabled)) {
      fprintf(stderr, "VS Output ");
      brw_print_vue_map(stderr, &prog_data->base.vue_map, MESA_SHADER_VERTEX);
   }

   const unsigned dispatch_width = compiler->devinfo->ver >= 20 ? 16 : 8;
   prog_data->base.dispatch_mode = INTEL_DISPATCH_MODE_SIMD8;

   fs_visitor v(compiler, &params->base, &key->base,
                &prog_data->base.base, nir, dispatch_width,
                params->base.stats != NULL, debug_enabled);
   if (!v.run_vs()) {
      params->base.error_str =
         ralloc_strdup(params->base.mem_ctx, v.fail_msg);
      return NULL;
   }

   assert(v.payload().num_regs % reg_unit(compiler->devinfo) == 0);
   prog_data->base.base.dispatch_grf_start_reg =
      v.payload().num_regs / reg_unit(compiler->devinfo);

   fs_generator g(compiler, &params->base,
                  &prog_data->base.base,
                  MESA_SHADER_VERTEX);
   if (unlikely(debug_enabled)) {
      const char *debug_name =
         ralloc_asprintf(params->base.mem_ctx, "%s vertex shader %s",
                         nir->info.label ? nir->info.label :
                            "unnamed",
                         nir->info.name);

      g.enable_debug(debug_name);
   }
   g.generate_code(v.cfg, dispatch_width, v.shader_stats,
                   v.performance_analysis.require(), params->base.stats);
   g.add_const_data(nir->constant_data, nir->constant_data_size);

   return g.get_assembly();
}
