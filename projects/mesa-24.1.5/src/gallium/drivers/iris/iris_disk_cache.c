/*
 * Copyright © 2018 Intel Corporation
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
 * @file iris_disk_cache.c
 *
 * Functions for interacting with the on-disk shader cache.
 */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

#include "compiler/nir/nir.h"
#include "util/blob.h"
#include "util/build_id.h"
#include "util/disk_cache.h"
#include "util/mesa-sha1.h"
#include "intel/compiler/brw_compiler.h"
#include "intel/compiler/elk/elk_compiler.h"

#include "iris_context.h"

static bool debug = false;

/**
 * Compute a disk cache key for the given uncompiled shader and NOS key.
 */
static void
iris_disk_cache_compute_key(struct disk_cache *cache,
                            const struct iris_uncompiled_shader *ish,
                            const void *orig_prog_key,
                            uint32_t prog_key_size,
                            cache_key cache_key)
{
   /* Create a copy of the program key with program_string_id zeroed out.
    * It's essentially random data which we don't want to include in our
    * hashing and comparisons.  We'll set a proper value on a cache hit.
    */
   union brw_any_prog_key prog_key;
   memcpy(&prog_key, orig_prog_key, prog_key_size);
   prog_key.base.program_string_id = 0;

   uint8_t data[sizeof(prog_key) + sizeof(ish->nir_sha1)];
   uint32_t data_size = prog_key_size + sizeof(ish->nir_sha1);

   memcpy(data, ish->nir_sha1, sizeof(ish->nir_sha1));
   memcpy(data + sizeof(ish->nir_sha1), &prog_key, prog_key_size);

   disk_cache_compute_key(cache, data, data_size, cache_key);
}

/**
 * Store the given compiled shader in the disk cache.
 *
 * This should only be called on newly compiled shaders.  No checking is
 * done to prevent repeated stores of the same shader.
 */
void
iris_disk_cache_store(struct disk_cache *cache,
                      const struct iris_uncompiled_shader *ish,
                      const struct iris_compiled_shader *shader,
                      const void *prog_key,
                      uint32_t prog_key_size)
{
#ifdef ENABLE_SHADER_CACHE
   if (!cache)
      return;

   gl_shader_stage stage = ish->nir->info.stage;
   const struct brw_stage_prog_data *brw = shader->brw_prog_data;
   const struct elk_stage_prog_data *elk = shader->elk_prog_data;
   assert((brw == NULL) != (elk == NULL));

   cache_key cache_key;
   iris_disk_cache_compute_key(cache, ish, prog_key, prog_key_size, cache_key);

   if (debug) {
      char sha1[41];
      _mesa_sha1_format(sha1, cache_key);
      fprintf(stderr, "[mesa disk cache] storing %s\n", sha1);
   }

   struct blob blob;
   blob_init(&blob);

   /* We write the following data to the cache blob:
    *
    * 1. Prog data (must come first because it has the assembly size)
    *   - Zero out pointer values in prog data, so cache entries will be
    *     consistent.
    * 2. Assembly code
    * 3. Number of entries in the system value array
    * 4. System value array
    * 5. Size (in bytes) of kernel inputs
    * 6. Shader relocations
    * 7. Legacy param array (only used for compute workgroup ID)
    * 8. Binding table
    */
   if (brw) {
      size_t prog_data_s = brw_prog_data_size(stage);
      union brw_any_prog_data serializable;
      assert(prog_data_s <= sizeof(serializable));
      memcpy(&serializable, shader->brw_prog_data, prog_data_s);
      serializable.base.param = NULL;
      serializable.base.relocs = NULL;
      blob_write_bytes(&blob, &serializable, prog_data_s);
   } else {
      size_t prog_data_s = elk_prog_data_size(stage);
      union elk_any_prog_data serializable;
      assert(prog_data_s <= sizeof(serializable));
      memcpy(&serializable, shader->elk_prog_data, prog_data_s);
      serializable.base.param = NULL;
      serializable.base.relocs = NULL;
      blob_write_bytes(&blob, &serializable, prog_data_s);
   }

   blob_write_bytes(&blob, shader->map, shader->program_size);
   blob_write_uint32(&blob, shader->num_system_values);
   blob_write_bytes(&blob, shader->system_values,
                    shader->num_system_values * sizeof(uint32_t));
   blob_write_uint32(&blob, shader->kernel_input_size);
   if (brw) {
      blob_write_bytes(&blob, brw->relocs,
                       brw->num_relocs * sizeof(struct brw_shader_reloc));
      blob_write_bytes(&blob, brw->param,
                       brw->nr_params * sizeof(uint32_t));
   } else {
      blob_write_bytes(&blob, elk->relocs,
                       elk->num_relocs * sizeof(struct elk_shader_reloc));
      blob_write_bytes(&blob, elk->param,
                       elk->nr_params * sizeof(uint32_t));
   }
   blob_write_bytes(&blob, &shader->bt, sizeof(shader->bt));

   disk_cache_put(cache, cache_key, blob.data, blob.size, NULL);
   blob_finish(&blob);
#endif
}

static const enum iris_program_cache_id cache_id_for_stage[] = {
   [MESA_SHADER_VERTEX]    = IRIS_CACHE_VS,
   [MESA_SHADER_TESS_CTRL] = IRIS_CACHE_TCS,
   [MESA_SHADER_TESS_EVAL] = IRIS_CACHE_TES,
   [MESA_SHADER_GEOMETRY]  = IRIS_CACHE_GS,
   [MESA_SHADER_FRAGMENT]  = IRIS_CACHE_FS,
   [MESA_SHADER_COMPUTE]   = IRIS_CACHE_CS,
};

/**
 * Search for a compiled shader in the disk cache.  If found, upload it
 * to the in-memory program cache so we can use it.
 */
bool
iris_disk_cache_retrieve(struct iris_screen *screen,
                         struct u_upload_mgr *uploader,
                         struct iris_uncompiled_shader *ish,
                         struct iris_compiled_shader *shader,
                         const void *prog_key,
                         uint32_t key_size)
{
#ifdef ENABLE_SHADER_CACHE
   struct disk_cache *cache = screen->disk_cache;
   gl_shader_stage stage = ish->nir->info.stage;

   if (!cache)
      return false;

   cache_key cache_key;
   iris_disk_cache_compute_key(cache, ish, prog_key, key_size, cache_key);

   if (debug) {
      char sha1[41];
      _mesa_sha1_format(sha1, cache_key);
      fprintf(stderr, "[mesa disk cache] retrieving %s: ", sha1);
   }

   size_t size;
   void *buffer = disk_cache_get(screen->disk_cache, cache_key, &size);

   if (debug)
      fprintf(stderr, "%s\n", buffer ? "found" : "missing");

   if (!buffer)
      return false;

   const uint32_t prog_data_size = screen->brw ? brw_prog_data_size(stage)
                                               : elk_prog_data_size(stage);

   void *prog_data = ralloc_size(NULL, prog_data_size);
   const void *assembly;
   uint32_t num_system_values;
   uint32_t kernel_input_size;
   uint32_t *system_values = NULL;
   uint32_t *so_decls = NULL;

   struct brw_stage_prog_data *brw = screen->brw ? prog_data : NULL;
   struct elk_stage_prog_data *elk = screen->elk ? prog_data : NULL;
   assert((brw == NULL) != (elk == NULL));

   struct blob_reader blob;
   blob_reader_init(&blob, buffer, size);
   blob_copy_bytes(&blob, prog_data, prog_data_size);
   assembly = blob_read_bytes(&blob, brw ? brw->program_size : elk->program_size);
   num_system_values = blob_read_uint32(&blob);
   if (num_system_values) {
      system_values =
         ralloc_array(NULL, uint32_t, num_system_values);
      blob_copy_bytes(&blob, system_values,
                      num_system_values * sizeof(uint32_t));
   }

   kernel_input_size = blob_read_uint32(&blob);

   if (brw) {
      brw->relocs = NULL;
      if (brw->num_relocs) {
         struct brw_shader_reloc *relocs =
            ralloc_array(NULL, struct brw_shader_reloc, brw->num_relocs);
         blob_copy_bytes(&blob, relocs,
                         brw->num_relocs * sizeof(struct brw_shader_reloc));
         brw->relocs = relocs;
      }

      brw->param = NULL;
      if (brw->nr_params) {
         brw->param = ralloc_array(NULL, uint32_t, brw->nr_params);
         blob_copy_bytes(&blob, brw->param, brw->nr_params * sizeof(uint32_t));
      }
   } else {
      elk->relocs = NULL;
      if (elk->num_relocs) {
         struct elk_shader_reloc *relocs =
            ralloc_array(NULL, struct elk_shader_reloc, elk->num_relocs);
         blob_copy_bytes(&blob, relocs,
                         elk->num_relocs * sizeof(struct elk_shader_reloc));
         elk->relocs = relocs;
      }

      elk->param = NULL;
      if (elk->nr_params) {
         elk->param = ralloc_array(NULL, uint32_t, elk->nr_params);
         blob_copy_bytes(&blob, elk->param,
                         elk->nr_params * sizeof(uint32_t));
      }
   }

   struct iris_binding_table bt;
   blob_copy_bytes(&blob, &bt, sizeof(bt));

   if (stage == MESA_SHADER_VERTEX ||
       stage == MESA_SHADER_TESS_EVAL ||
       stage == MESA_SHADER_GEOMETRY) {
      struct intel_vue_map *vue_map =
         screen->brw ? &brw_vue_prog_data(prog_data)->vue_map
                     : &elk_vue_prog_data(prog_data)->vue_map;
      so_decls = screen->vtbl.create_so_decl_list(&ish->stream_output, vue_map);
   }

   /* System values and uniforms are stored in constant buffer 0, the
    * user-facing UBOs are indexed by one.  So if any constant buffer is
    * needed, the constant buffer 0 will be needed, so account for it.
    */
   unsigned num_cbufs = ish->nir->info.num_ubos;

   if (num_cbufs || ish->nir->num_uniforms)
      num_cbufs++;

   if (num_system_values || kernel_input_size)
      num_cbufs++;

   if (brw)
      iris_apply_brw_prog_data(shader, brw);
   else
      iris_apply_elk_prog_data(shader, elk);

   iris_finalize_program(shader, so_decls, system_values,
                         num_system_values, kernel_input_size, num_cbufs,
                         &bt);

   assert(stage < ARRAY_SIZE(cache_id_for_stage));
   enum iris_program_cache_id cache_id = cache_id_for_stage[stage];

   /* Upload our newly read shader to the in-memory program cache. */
   iris_upload_shader(screen, ish, shader, NULL, uploader,
                      cache_id, key_size, prog_key, assembly);

   free(buffer);

   return true;
#else
   return false;
#endif
}

/**
 * Initialize the on-disk shader cache.
 */
void
iris_disk_cache_init(struct iris_screen *screen)
{
#ifdef ENABLE_SHADER_CACHE
   if (INTEL_DEBUG(DEBUG_DISK_CACHE_DISABLE_MASK))
      return;

   /* array length = strlen("iris_") + sha + nul char */
   char renderer[5 + 40 + 1] = {0};

   if (screen->brw) {
      char device_info_sha[41];
      brw_device_sha1(device_info_sha, screen->devinfo);
      memcpy(renderer, "iris_", 5);
      memcpy(renderer + 5, device_info_sha, 40);
   } else {
      /* For Gfx8, just use PCI ID. */
      ASSERTED int len = snprintf(renderer, sizeof(renderer),
                                  "iris_%04x", screen->devinfo->pci_device_id);
      assert(len < ARRAY_SIZE(renderer) - 1);
   }

   const struct build_id_note *note =
      build_id_find_nhdr_for_addr(iris_disk_cache_init);
   assert(note && build_id_length(note) == 20); /* sha1 */

   const uint8_t *id_sha1 = build_id_data(note);
   assert(id_sha1);

   char timestamp[41];
   _mesa_sha1_format(timestamp, id_sha1);

   const uint64_t driver_flags = screen->brw ?
      brw_get_compiler_config_value(screen->brw) :
      elk_get_compiler_config_value(screen->elk);
   screen->disk_cache = disk_cache_create(renderer, timestamp, driver_flags);
#endif
}
