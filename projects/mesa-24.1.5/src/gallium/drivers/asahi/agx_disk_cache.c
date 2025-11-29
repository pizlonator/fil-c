/*
 * Copyright 2023 Rose Hudson
 * Copyright 2022 Amazon.com, Inc. or its affiliates.
 * Copyright 2018 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include <assert.h>
#include <stdio.h>

#include "asahi/compiler/agx_debug.h"
#include "compiler/shader_enums.h"
#include "util/blob.h"
#include "util/build_id.h"
#include "util/disk_cache.h"
#include "util/mesa-sha1.h"
#include "agx_bo.h"
#include "agx_device.h"
#include "agx_disk_cache.h"
#include "agx_state.h"

/* Flags that are allowed and do not disable the disk cache */
#define ALLOWED_FLAGS (AGX_DBG_NO16 | AGX_DBG_COMPBLIT)

/**
 * Compute a disk cache key for the given uncompiled shader and shader key.
 */
static void
agx_disk_cache_compute_key(struct disk_cache *cache,
                           const struct agx_uncompiled_shader *uncompiled,
                           const union asahi_shader_key *shader_key,
                           cache_key cache_key)
{
   uint8_t data[sizeof(uncompiled->nir_sha1) + sizeof(*shader_key)];
   int hash_size = sizeof(uncompiled->nir_sha1);
   int key_size;
   if (uncompiled->type == PIPE_SHADER_VERTEX ||
       uncompiled->type == PIPE_SHADER_TESS_EVAL)
      key_size = sizeof(shader_key->vs);
   else if (uncompiled->type == PIPE_SHADER_GEOMETRY)
      key_size = sizeof(shader_key->gs);
   else if (uncompiled->type == PIPE_SHADER_FRAGMENT)
      key_size = sizeof(shader_key->fs);
   else if (uncompiled->type == PIPE_SHADER_COMPUTE ||
            uncompiled->type == PIPE_SHADER_TESS_CTRL)
      key_size = 0;
   else
      unreachable("Unsupported shader stage");

   memcpy(data, uncompiled->nir_sha1, hash_size);

   if (key_size)
      memcpy(data + hash_size, shader_key, key_size);

   disk_cache_compute_key(cache, data, hash_size + key_size, cache_key);
}

static void
write_shader(struct blob *blob, const struct agx_compiled_shader *binary,
             bool is_root_gs)
{
   blob_write_uint32(blob, binary->b.binary_size);

   if (binary->b.binary_size) {
      blob_write_bytes(blob, binary->b.binary, binary->b.binary_size);
   }

   blob_write_bytes(blob, &binary->b.info, sizeof(binary->b.info));
   blob_write_bytes(blob, &binary->uvs, sizeof(binary->uvs));
   blob_write_bytes(blob, &binary->attrib_components_read,
                    sizeof(binary->attrib_components_read));
   blob_write_bytes(blob, &binary->epilog_key, sizeof(binary->epilog_key));
   blob_write_uint32(blob, binary->push_range_count);
   blob_write_bytes(blob, binary->push,
                    sizeof(binary->push[0]) * binary->push_range_count);

   if (is_root_gs) {
      blob_write_uint32(blob, binary->gs_count_words);
      blob_write_uint32(blob, binary->gs_output_mode);
      write_shader(blob, binary->pre_gs, false);

      blob_write_uint8(blob, binary->gs_copy != NULL);
      if (binary->gs_copy)
         write_shader(blob, binary->gs_copy, false);

      blob_write_uint8(blob, binary->gs_count != NULL);
      if (binary->gs_count)
         write_shader(blob, binary->gs_count, false);
   }
}

static struct agx_compiled_shader *
read_shader(struct agx_screen *screen, struct blob_reader *blob,
            const struct agx_uncompiled_shader *uncompiled, bool is_root)
{
   struct agx_compiled_shader *binary = CALLOC_STRUCT(agx_compiled_shader);
   binary->stage = uncompiled->type;
   binary->so = uncompiled;

   size_t size = blob_read_uint32(blob);

   if (uncompiled->type == PIPE_SHADER_VERTEX ||
       uncompiled->type == PIPE_SHADER_TESS_EVAL ||
       uncompiled->type == PIPE_SHADER_FRAGMENT) {
      binary->b.binary_size = size;
      binary->b.binary = malloc(binary->b.binary_size);
      blob_copy_bytes(blob, binary->b.binary, binary->b.binary_size);

      if (size) {
         binary->bo = agx_bo_create(&screen->dev, size,
                                    AGX_BO_EXEC | AGX_BO_LOW_VA, "Executable");
         memcpy(binary->bo->ptr.cpu, binary->b.binary, size);
      }
   } else if (size) {
      binary->bo = agx_bo_create(&screen->dev, size,
                                 AGX_BO_EXEC | AGX_BO_LOW_VA, "Executable");
      blob_copy_bytes(blob, binary->bo->ptr.cpu, size);
   }

   blob_copy_bytes(blob, &binary->b.info, sizeof(binary->b.info));
   blob_copy_bytes(blob, &binary->uvs, sizeof(binary->uvs));
   blob_copy_bytes(blob, &binary->attrib_components_read,
                   sizeof(binary->attrib_components_read));
   blob_copy_bytes(blob, &binary->epilog_key, sizeof(binary->epilog_key));
   binary->push_range_count = blob_read_uint32(blob);
   blob_copy_bytes(blob, binary->push,
                   sizeof(binary->push[0]) * binary->push_range_count);

   if (is_root && uncompiled->type == PIPE_SHADER_GEOMETRY) {
      binary->gs_count_words = blob_read_uint32(blob);
      binary->gs_output_mode = blob_read_uint32(blob);
      binary->pre_gs = read_shader(screen, blob, uncompiled, false);

      if (blob_read_uint8(blob))
         binary->gs_copy = read_shader(screen, blob, uncompiled, false);

      if (blob_read_uint8(blob))
         binary->gs_count = read_shader(screen, blob, uncompiled, false);
   }

   return binary;
}

/**
 * Store the given compiled shader in the disk cache.
 *
 * This should only be called on newly compiled shaders.  No checking is
 * done to prevent repeated stores of the same shader.
 */
void
agx_disk_cache_store(struct disk_cache *cache,
                     const struct agx_uncompiled_shader *uncompiled,
                     const union asahi_shader_key *key,
                     const struct agx_compiled_shader *binary)
{
#ifdef ENABLE_SHADER_CACHE
   if (!cache)
      return;

   cache_key cache_key;
   agx_disk_cache_compute_key(cache, uncompiled, key, cache_key);

   struct blob blob;
   blob_init(&blob);

   write_shader(&blob, binary, uncompiled->type == PIPE_SHADER_GEOMETRY);

   disk_cache_put(cache, cache_key, blob.data, blob.size, NULL);
   blob_finish(&blob);
#endif
}

/**
 * Search for a compiled shader in the disk cache.
 */
struct agx_compiled_shader *
agx_disk_cache_retrieve(struct agx_screen *screen,
                        const struct agx_uncompiled_shader *uncompiled,
                        const union asahi_shader_key *key)
{
#ifdef ENABLE_SHADER_CACHE
   struct disk_cache *cache = screen->disk_cache;
   if (!cache)
      return NULL;

   cache_key cache_key;
   agx_disk_cache_compute_key(cache, uncompiled, key, cache_key);

   size_t size;
   void *buffer = disk_cache_get(cache, cache_key, &size);
   if (!buffer)
      return NULL;

   struct blob_reader blob;
   blob_reader_init(&blob, buffer, size);

   struct agx_compiled_shader *binary =
      read_shader(screen, &blob, uncompiled, true);

   free(buffer);
   return binary;
#else
   return NULL;
#endif
}

/**
 * Initialise the on-disk shader cache.
 */
void
agx_disk_cache_init(struct agx_screen *screen)
{
#ifdef ENABLE_SHADER_CACHE
   if (agx_get_compiler_debug() || (screen->dev.debug & ~ALLOWED_FLAGS))
      return;

   const char *renderer = screen->pscreen.get_name(&screen->pscreen);

   const struct build_id_note *note =
      build_id_find_nhdr_for_addr(agx_disk_cache_init);
   assert(note && build_id_length(note) == 20);

   const uint8_t *id_sha1 = build_id_data(note);
   assert(id_sha1);

   char timestamp[41];
   _mesa_sha1_format(timestamp, id_sha1);

   uint64_t driver_flags = screen->dev.debug;
   screen->disk_cache = disk_cache_create(renderer, timestamp, driver_flags);
#endif
}
