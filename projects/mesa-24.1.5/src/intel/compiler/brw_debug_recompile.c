/*
 * Copyright © 2019 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/**
 * @file brw_debug_recompiles.c
 */

#include <stdio.h>

#include "brw_compiler.h"

static bool
key_debug(const struct brw_compiler *c, void *log,
          const char *name, int a, int b)
{
   if (a != b) {
      brw_shader_perf_log(c, log, "  %s %d->%d\n", name, a, b);
      return true;
   }
   return false;
}

static bool
key_debug_float(const struct brw_compiler *c, void *log,
                const char *name, float a, float b)
{
   if (a != b) {
      brw_shader_perf_log(c, log, "  %s %f->%f\n", name, a, b);
      return true;
   }
   return false;
}

#define check(name, field) \
   key_debug(c, log, name, old_key->field, key->field)
#define check_float(name, field) \
   key_debug_float(c, log, name, old_key->field, key->field)


static bool
debug_base_recompile(const struct brw_compiler *c, void *log,
                     const struct brw_base_prog_key *old_key,
                     const struct brw_base_prog_key *key)
{
   bool found = false;

   found |= check("robust_flags", robust_flags);
   found |= check("limit_trig_input_range", limit_trig_input_range);

   return found;
}

static void
debug_vs_recompile(const struct brw_compiler *c, void *log,
                   const struct brw_vs_prog_key *old_key,
                   const struct brw_vs_prog_key *key)
{
   bool found = debug_base_recompile(c, log, &old_key->base, &key->base);

   if (!found) {
      brw_shader_perf_log(c, log, "  something else\n");
   }
}

static void
debug_tcs_recompile(const struct brw_compiler *c, void *log,
                    const struct brw_tcs_prog_key *old_key,
                    const struct brw_tcs_prog_key *key)
{
   bool found = debug_base_recompile(c, log, &old_key->base, &key->base);

   found |= check("input vertices", input_vertices);
   found |= check("outputs written", outputs_written);
   found |= check("patch outputs written", patch_outputs_written);
   found |= check("tes primitive mode", _tes_primitive_mode);

   if (!found) {
      brw_shader_perf_log(c, log, "  something else\n");
   }
}

static void
debug_tes_recompile(const struct brw_compiler *c, void *log,
                    const struct brw_tes_prog_key *old_key,
                    const struct brw_tes_prog_key *key)
{
   bool found = debug_base_recompile(c, log, &old_key->base, &key->base);

   found |= check("inputs read", inputs_read);
   found |= check("patch inputs read", patch_inputs_read);

   if (!found) {
      brw_shader_perf_log(c, log, "  something else\n");
   }
}

static void
debug_gs_recompile(const struct brw_compiler *c, void *log,
                   const struct brw_gs_prog_key *old_key,
                   const struct brw_gs_prog_key *key)
{
   bool found = debug_base_recompile(c, log, &old_key->base, &key->base);

   if (!found) {
      brw_shader_perf_log(c, log, "  something else\n");
   }
}

static void
debug_fs_recompile(const struct brw_compiler *c, void *log,
                   const struct brw_wm_prog_key *old_key,
                   const struct brw_wm_prog_key *key)
{
   bool found = false;

   found |= check("flat shading", flat_shade);
   found |= check("number of color buffers", nr_color_regions);
   found |= check("MRT alpha test", alpha_test_replicate_alpha);
   found |= check("alpha to coverage", alpha_to_coverage);
   found |= check("fragment color clamping", clamp_fragment_color);
   found |= check("per-sample interpolation", persample_interp);
   found |= check("multisampled FBO", multisample_fbo);
   found |= check("force dual color blending", force_dual_color_blend);
   found |= check("coherent fb fetch", coherent_fb_fetch);
   found |= check("ignore sample mask out", ignore_sample_mask_out);
   found |= check("coarse pixel", coarse_pixel);

   found |= check("input slots valid", input_slots_valid);

   found |= debug_base_recompile(c, log, &old_key->base, &key->base);

   if (!found) {
      brw_shader_perf_log(c, log, "  something else\n");
   }
}

static void
debug_cs_recompile(const struct brw_compiler *c, void *log,
                   const struct brw_cs_prog_key *old_key,
                   const struct brw_cs_prog_key *key)
{
   bool found = debug_base_recompile(c, log, &old_key->base, &key->base);

   if (!found) {
      brw_shader_perf_log(c, log, "  something else\n");
   }
}

void
brw_debug_key_recompile(const struct brw_compiler *c, void *log,
                        gl_shader_stage stage,
                        const struct brw_base_prog_key *old_key,
                        const struct brw_base_prog_key *key)
{
   if (!old_key) {
      brw_shader_perf_log(c, log, "  No previous compile found...\n");
      return;
   }

   switch (stage) {
   case MESA_SHADER_VERTEX:
      debug_vs_recompile(c, log, (const struct brw_vs_prog_key *)old_key,
                                 (const struct brw_vs_prog_key *)key);
      break;
   case MESA_SHADER_TESS_CTRL:
      debug_tcs_recompile(c, log, (const struct brw_tcs_prog_key *)old_key,
                                  (const struct brw_tcs_prog_key *)key);
      break;
   case MESA_SHADER_TESS_EVAL:
      debug_tes_recompile(c, log, (const struct brw_tes_prog_key *)old_key,
                                  (const struct brw_tes_prog_key *)key);
      break;
   case MESA_SHADER_GEOMETRY:
      debug_gs_recompile(c, log, (const struct brw_gs_prog_key *)old_key,
                                 (const struct brw_gs_prog_key *)key);
      break;
   case MESA_SHADER_FRAGMENT:
      debug_fs_recompile(c, log, (const struct brw_wm_prog_key *)old_key,
                                 (const struct brw_wm_prog_key *)key);
      break;
   case MESA_SHADER_COMPUTE:
      debug_cs_recompile(c, log, (const struct brw_cs_prog_key *)old_key,
                                 (const struct brw_cs_prog_key *)key);
      break;
   default:
      break;
   }
}
