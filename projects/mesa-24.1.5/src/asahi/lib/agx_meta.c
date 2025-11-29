/*
 * Copyright 2022 Alyssa Rosenzweig
 * SPDX-License-Identifier: MIT
 */

#include "agx_meta.h"
#include "agx_compile.h"
#include "agx_device.h" /* for AGX_MEMORY_TYPE_SHADER */
#include "agx_nir_passes.h"
#include "agx_tilebuffer.h"
#include "nir.h"
#include "nir_builder.h"
#include "nir_intrinsics.h"

static bool
lower_tex_handle_to_u0(nir_builder *b, nir_intrinsic_instr *intr, void *data)
{
   if (intr->intrinsic != nir_intrinsic_load_texture_handle_agx)
      return false;

   b->cursor = nir_instr_remove(&intr->instr);
   nir_def_rewrite_uses(
      &intr->def,
      nir_vec2(b, nir_imm_int(b, 0), nir_imul_imm(b, intr->src[0].ssa, 24)));

   return true;
}

static struct agx_meta_shader *
agx_compile_meta_shader(struct agx_meta_cache *cache, nir_shader *shader,
                        struct agx_shader_key *key,
                        struct agx_tilebuffer_layout *tib)
{
   agx_nir_lower_texture(shader);
   agx_preprocess_nir(shader, cache->dev->libagx);
   if (tib) {
      unsigned bindless_base = 0;
      agx_nir_lower_tilebuffer(shader, tib, NULL, &bindless_base, NULL);
      agx_nir_lower_monolithic_msaa(shader, tib->nr_samples);
      agx_nir_lower_multisampled_image_store(shader);

      nir_shader_intrinsics_pass(
         shader, lower_tex_handle_to_u0,
         nir_metadata_dominance | nir_metadata_block_index, NULL);
   }

   key->libagx = cache->dev->libagx;

   struct agx_meta_shader *res = rzalloc(cache->ht, struct agx_meta_shader);
   struct agx_shader_part bin;
   agx_compile_shader_nir(shader, key, NULL, &bin);

   res->info = bin.info;
   res->ptr = agx_pool_upload_aligned_with_bo(&cache->pool, bin.binary,
                                              bin.binary_size, 128, &res->bo);
   free(bin.binary);
   ralloc_free(shader);

   return res;
}

static nir_def *
build_background_op(nir_builder *b, enum agx_meta_op op, unsigned rt,
                    unsigned nr, bool msaa, bool layered)
{
   if (op == AGX_META_OP_LOAD) {
      nir_def *coord = nir_u2u32(b, nir_load_pixel_coord(b));

      if (layered) {
         coord = nir_vec3(b, nir_channel(b, coord, 0), nir_channel(b, coord, 1),
                          nir_load_layer_id(b));
      }

      nir_tex_instr *tex = nir_tex_instr_create(b->shader, 2);
      /* The type doesn't matter as long as it matches the store */
      tex->dest_type = nir_type_uint32;
      tex->sampler_dim = msaa ? GLSL_SAMPLER_DIM_MS : GLSL_SAMPLER_DIM_2D;
      tex->is_array = layered;
      tex->op = msaa ? nir_texop_txf_ms : nir_texop_txf;
      tex->src[0] = nir_tex_src_for_ssa(nir_tex_src_coord, coord);

      /* Layer is necessarily already in-bounds so we do not want the compiler
       * to clamp it, which would require reading the descriptor
       */
      tex->backend_flags = AGX_TEXTURE_FLAG_NO_CLAMP;

      if (msaa) {
         tex->src[1] =
            nir_tex_src_for_ssa(nir_tex_src_ms_index, nir_load_sample_id(b));
         b->shader->info.fs.uses_sample_shading = true;
      } else {
         tex->src[1] = nir_tex_src_for_ssa(nir_tex_src_lod, nir_imm_int(b, 0));
      }

      tex->coord_components = layered ? 3 : 2;
      tex->texture_index = rt * 2;
      nir_def_init(&tex->instr, &tex->def, 4, 32);
      nir_builder_instr_insert(b, &tex->instr);

      return nir_trim_vector(b, &tex->def, nr);
   } else {
      assert(op == AGX_META_OP_CLEAR);

      return nir_load_preamble(b, nr, 32, 4 + (rt * 8));
   }
}

static struct agx_meta_shader *
agx_build_background_shader(struct agx_meta_cache *cache,
                            struct agx_meta_key *key)
{
   nir_builder b = nir_builder_init_simple_shader(
      MESA_SHADER_FRAGMENT, &agx_nir_options, "agx_background");
   b.shader->info.fs.untyped_color_outputs = true;

   struct agx_shader_key compiler_key = {
      .fs.ignore_tib_dependencies = true,
      .reserved_preamble = key->reserved_preamble,
   };

   for (unsigned rt = 0; rt < ARRAY_SIZE(key->op); ++rt) {
      if (key->op[rt] == AGX_META_OP_NONE)
         continue;

      unsigned nr = util_format_get_nr_components(key->tib.logical_format[rt]);
      bool msaa = key->tib.nr_samples > 1;
      bool layered = key->tib.layered;
      assert(nr > 0);

      nir_store_output(
         &b, build_background_op(&b, key->op[rt], rt, nr, msaa, layered),
         nir_imm_int(&b, 0), .write_mask = BITFIELD_MASK(nr),
         .src_type = nir_type_uint32,
         .io_semantics.location = FRAG_RESULT_DATA0 + rt,
         .io_semantics.num_slots = 1);

      b.shader->info.outputs_written |= BITFIELD64_BIT(FRAG_RESULT_DATA0 + rt);
   }

   return agx_compile_meta_shader(cache, b.shader, &compiler_key, &key->tib);
}

static struct agx_meta_shader *
agx_build_end_of_tile_shader(struct agx_meta_cache *cache,
                             struct agx_meta_key *key)
{
   nir_builder b = nir_builder_init_simple_shader(MESA_SHADER_COMPUTE,
                                                  &agx_nir_options, "agx_eot");

   enum glsl_sampler_dim dim =
      (key->tib.nr_samples > 1) ? GLSL_SAMPLER_DIM_MS : GLSL_SAMPLER_DIM_2D;

   for (unsigned rt = 0; rt < ARRAY_SIZE(key->op); ++rt) {
      if (key->op[rt] == AGX_META_OP_NONE)
         continue;

      /* The end-of-tile shader is unsuitable to handle spilled render targets.
       * Skip them. If blits are needed with spilled render targets, other parts
       * of the driver need to implement them.
       */
      if (key->tib.spilled[rt])
         continue;

      assert(key->op[rt] == AGX_META_OP_STORE);
      unsigned offset_B = agx_tilebuffer_offset_B(&key->tib, rt);

      nir_def *layer = nir_undef(&b, 1, 16);
      if (key->tib.layered)
         layer = nir_u2u16(&b, nir_load_layer_id(&b));

      nir_block_image_store_agx(
         &b, nir_imm_int(&b, rt), nir_imm_intN_t(&b, offset_B, 16), layer,
         .format = agx_tilebuffer_physical_format(&key->tib, rt),
         .image_dim = dim, .image_array = key->tib.layered);
   }

   struct agx_shader_key compiler_key = {
      .reserved_preamble = key->reserved_preamble,
   };

   return agx_compile_meta_shader(cache, b.shader, &compiler_key, NULL);
}

struct agx_meta_shader *
agx_get_meta_shader(struct agx_meta_cache *cache, struct agx_meta_key *key)
{
   struct hash_entry *ent = _mesa_hash_table_search(cache->ht, key);
   if (ent)
      return ent->data;

   struct agx_meta_shader *ret = NULL;

   for (unsigned rt = 0; rt < ARRAY_SIZE(key->op); ++rt) {
      if (key->op[rt] == AGX_META_OP_STORE) {
         ret = agx_build_end_of_tile_shader(cache, key);
         break;
      }
   }

   if (!ret)
      ret = agx_build_background_shader(cache, key);

   ret->key = *key;
   _mesa_hash_table_insert(cache->ht, &ret->key, ret);
   return ret;
}

DERIVE_HASH_TABLE(agx_meta_key);

void
agx_meta_init(struct agx_meta_cache *cache, struct agx_device *dev)
{
   agx_pool_init(&cache->pool, dev, AGX_BO_EXEC | AGX_BO_LOW_VA, true);
   cache->ht = agx_meta_key_table_create(NULL);
   cache->dev = dev;
}

void
agx_meta_cleanup(struct agx_meta_cache *cache)
{
   agx_pool_cleanup(&cache->pool);
   _mesa_hash_table_destroy(cache->ht, NULL);
   cache->ht = NULL;
   cache->dev = NULL;
}
