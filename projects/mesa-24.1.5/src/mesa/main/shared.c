/*
 * Mesa 3-D graphics library
 *
 * Copyright (C) 2009  VMware, Inc.  All Rights Reserved.
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
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * \file shared.c
 * Shared-context state
 */


#include "mtypes.h"
#include "hash.h"
#include "atifragshader.h"
#include "bufferobj.h"
#include "shared.h"
#include "program/program.h"
#include "dlist.h"
#include "externalobjects.h"
#include "samplerobj.h"
#include "shaderapi.h"
#include "shaderobj.h"
#include "syncobj.h"
#include "texobj.h"
#include "texturebindless.h"

#include "util/hash_table.h"
#include "util/set.h"
#include "util/u_memory.h"

static void
free_shared_state(struct gl_context *ctx, struct gl_shared_state *shared);

/**
 * Allocate and initialize a shared context state structure.
 * Initializes the display list, texture objects and vertex programs hash
 * tables, allocates the texture objects. If it runs out of memory, frees
 * everything already allocated before returning NULL.
 *
 * \return pointer to a gl_shared_state structure on success, or NULL on
 * failure.
 */
struct gl_shared_state *
_mesa_alloc_shared_state(struct gl_context *ctx)
{
   struct gl_shared_state *shared;
   GLuint i;

   shared = CALLOC_STRUCT(gl_shared_state);
   if (!shared)
      return NULL;

   simple_mtx_init(&shared->Mutex, mtx_plain);

   _mesa_InitHashTable(&shared->DisplayList);
   _mesa_InitHashTable(&shared->TexObjects);
   _mesa_InitHashTable(&shared->Programs);

   shared->DefaultVertexProgram =
      ctx->Driver.NewProgram(ctx, MESA_SHADER_VERTEX, 0, true);
   shared->DefaultFragmentProgram =
      ctx->Driver.NewProgram(ctx, MESA_SHADER_FRAGMENT, 0, true);

   _mesa_InitHashTable(&shared->ATIShaders);
   shared->DefaultFragmentShader = _mesa_new_ati_fragment_shader(ctx, 0);

   _mesa_InitHashTable(&shared->ShaderObjects);

   _mesa_InitHashTable(&shared->BufferObjects);
   shared->ZombieBufferObjects = _mesa_set_create(NULL, _mesa_hash_pointer,
                                                  _mesa_key_pointer_equal);

   /* GL_ARB_sampler_objects */
   _mesa_InitHashTable(&shared->SamplerObjects);

   /* GL_ARB_bindless_texture */
   _mesa_init_shared_handles(shared);

   /* ARB_shading_language_include */
   _mesa_init_shader_includes(shared);
   simple_mtx_init(&shared->ShaderIncludeMutex, mtx_plain);

   /* Create default texture objects */
   for (i = 0; i < NUM_TEXTURE_TARGETS; i++) {
      /* NOTE: the order of these enums matches the TEXTURE_x_INDEX values */
      static const GLenum targets[] = {
         GL_TEXTURE_2D_MULTISAMPLE,
         GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
         GL_TEXTURE_CUBE_MAP_ARRAY,
         GL_TEXTURE_BUFFER,
         GL_TEXTURE_2D_ARRAY_EXT,
         GL_TEXTURE_1D_ARRAY_EXT,
         GL_TEXTURE_EXTERNAL_OES,
         GL_TEXTURE_CUBE_MAP,
         GL_TEXTURE_3D,
         GL_TEXTURE_RECTANGLE_NV,
         GL_TEXTURE_2D,
         GL_TEXTURE_1D
      };
      STATIC_ASSERT(ARRAY_SIZE(targets) == NUM_TEXTURE_TARGETS);
      shared->DefaultTex[i] = _mesa_new_texture_object(ctx, 0, targets[i]);
      /* Need to explicitly set/overwrite the TargetIndex field here since
       * the call to _mesa_tex_target_to_index() in NewTextureObject() may
       * fail if the texture target is not supported.
       */
      shared->DefaultTex[i]->TargetIndex = i;
   }

   /* sanity check */
   assert(shared->DefaultTex[TEXTURE_1D_INDEX]->RefCount == 1);

   /* Mutex and timestamp for texobj state validation */
   simple_mtx_init(&shared->TexMutex, mtx_plain);
   shared->TextureStateStamp = 0;

   _mesa_InitHashTable(&shared->FrameBuffers);
   _mesa_InitHashTable(&shared->RenderBuffers);

   shared->SyncObjects = _mesa_set_create(NULL, _mesa_hash_pointer,
                                          _mesa_key_pointer_equal);

   _mesa_InitHashTable(&shared->MemoryObjects);
   _mesa_InitHashTable(&shared->SemaphoreObjects);

   shared->GLThread.NoLockDuration = ONE_SECOND_IN_NS;

   return shared;
}


/**
 * Callback for deleting a display list.  Called by _mesa_DeleteHashTable().
 */
static void
delete_displaylist_cb(void *data, void *userData)
{
   struct gl_display_list *list = (struct gl_display_list *) data;
   struct gl_context *ctx = (struct gl_context *) userData;
   _mesa_delete_list(ctx, list);
}


/**
 * Callback for deleting a texture object.  Called by _mesa_DeleteHashTable().
 */
static void
delete_texture_cb(void *data, void *userData)
{
   struct gl_texture_object *texObj = (struct gl_texture_object *) data;
   struct gl_context *ctx = (struct gl_context *) userData;
   _mesa_delete_texture_object(ctx, texObj);
}


/**
 * Callback for deleting a program object.  Called by _mesa_DeleteHashTable().
 */
static void
delete_program_cb(void *data, void *userData)
{
   struct gl_program *prog = (struct gl_program *) data;
   struct gl_context *ctx = (struct gl_context *) userData;
   if(prog != &_mesa_DummyProgram) {
      assert(prog->RefCount == 1); /* should only be referenced by hash table */
      prog->RefCount = 0;  /* now going away */
      _mesa_delete_program(ctx, prog);
   }
}


/**
 * Callback for deleting an ATI fragment shader object.
 * Called by _mesa_DeleteHashTable().
 */
static void
delete_fragshader_cb(void *data, void *userData)
{
   struct ati_fragment_shader *shader = (struct ati_fragment_shader *) data;
   struct gl_context *ctx = (struct gl_context *) userData;
   _mesa_delete_ati_fragment_shader(ctx, shader);
}


/**
 * Callback for deleting a buffer object.  Called by _mesa_DeleteHashTable().
 */
static void
delete_bufferobj_cb(void *data, void *userData)
{
   struct gl_buffer_object *bufObj = (struct gl_buffer_object *) data;
   struct gl_context *ctx = (struct gl_context *) userData;

   _mesa_buffer_unmap_all_mappings(ctx, bufObj);
   _mesa_reference_buffer_object(ctx, &bufObj, NULL);
}


/**
 * Callback for freeing shader program data. Call it before delete_shader_cb
 * to avoid memory access error.
 */
static void
free_shader_program_data_cb(void *data, void *userData)
{
   struct gl_context *ctx = (struct gl_context *) userData;
   struct gl_shader_program *shProg = (struct gl_shader_program *) data;

   if (shProg->Type == GL_SHADER_PROGRAM_MESA) {
       _mesa_free_shader_program_data(ctx, shProg);
   }
}


/**
 * Callback for deleting shader and shader programs objects.
 * Called by _mesa_DeleteHashTable().
 */
static void
delete_shader_cb(void *data, void *userData)
{
   struct gl_context *ctx = (struct gl_context *) userData;
   struct gl_shader *sh = (struct gl_shader *) data;
   if (_mesa_validate_shader_target(ctx, sh->Type)) {
      _mesa_delete_shader(ctx, sh);
   }
   else {
      struct gl_shader_program *shProg = (struct gl_shader_program *) data;
      assert(shProg->Type == GL_SHADER_PROGRAM_MESA);
      _mesa_delete_shader_program(ctx, shProg);
   }
}


/**
 * Callback for deleting a framebuffer object.  Called by _mesa_DeleteHashTable()
 */
static void
delete_framebuffer_cb(void *data, UNUSED void *userData)
{
   struct gl_framebuffer *fb = (struct gl_framebuffer *) data;
   /* The fact that the framebuffer is in the hashtable means its refcount
    * is one, but we're removing from the hashtable now.  So clear refcount.
    */
   /*assert(fb->RefCount == 1);*/
   fb->RefCount = 0;

   /* NOTE: Delete should always be defined but there are two reports
    * of it being NULL (bugs 13507, 14293).  Work-around for now.
    */
   if (fb->Delete)
      fb->Delete(fb);
}


/**
 * Callback for deleting a renderbuffer object. Called by _mesa_DeleteHashTable()
 */
static void
delete_renderbuffer_cb(void *data, void *userData)
{
   struct gl_context *ctx = (struct gl_context *) userData;
   struct gl_renderbuffer *rb = (struct gl_renderbuffer *) data;
   rb->RefCount = 0;  /* see comment for FBOs above */
   if (rb->Delete)
      rb->Delete(ctx, rb);
}


/**
 * Callback for deleting a sampler object. Called by _mesa_DeleteHashTable()
 */
static void
delete_sampler_object_cb(void *data, void *userData)
{
   struct gl_context *ctx = (struct gl_context *) userData;
   struct gl_sampler_object *sampObj = (struct gl_sampler_object *) data;
   _mesa_reference_sampler_object(ctx, &sampObj, NULL);
}

/**
 * Callback for deleting a memory object.  Called by _mesa_DeleteHashTable().
 */
static void
delete_memory_object_cb(void *data, void *userData)
{
   struct gl_memory_object *memObj = (struct gl_memory_object *) data;
   struct gl_context *ctx = (struct gl_context *) userData;
   _mesa_delete_memory_object(ctx, memObj);
}

/**
 * Callback for deleting a memory object.  Called by _mesa_DeleteHashTable().
 */
static void
delete_semaphore_object_cb(void *data, void *userData)
{
   struct gl_semaphore_object *semObj = (struct gl_semaphore_object *) data;
   struct gl_context *ctx = (struct gl_context *) userData;
   _mesa_delete_semaphore_object(ctx, semObj);
}

/**
 * Deallocate a shared state object and all children structures.
 *
 * \param ctx GL context.
 * \param shared shared state pointer.
 *
 * Frees the display lists, the texture objects (calling the driver texture
 * deletion callback to free its private data) and the vertex programs, as well
 * as their hash tables.
 *
 * \sa alloc_shared_state().
 */
static void
free_shared_state(struct gl_context *ctx, struct gl_shared_state *shared)
{
   GLuint i;

   /* Free the dummy/fallback texture objects */
   for (i = 0; i < NUM_TEXTURE_TARGETS; i++) {
      for (unsigned j = 0; j < ARRAY_SIZE(shared->FallbackTex[0]); j++) {
         if (shared->FallbackTex[i][j])
            _mesa_delete_texture_object(ctx, shared->FallbackTex[i][j]);
      }
   }

   /*
    * Free display lists
    */
   _mesa_DeinitHashTable(&shared->DisplayList, delete_displaylist_cb, ctx);
   free(shared->small_dlist_store.ptr);
   util_idalloc_fini(&shared->small_dlist_store.free_idx);

   _mesa_HashWalk(&shared->ShaderObjects, free_shader_program_data_cb, ctx);
   _mesa_DeinitHashTable(&shared->ShaderObjects, delete_shader_cb, ctx);
   _mesa_DeinitHashTable(&shared->Programs, delete_program_cb, ctx);

   if (shared->DefaultVertexProgram)
      _mesa_reference_program(ctx, &shared->DefaultVertexProgram, NULL);

   if (shared->DefaultFragmentProgram)
      _mesa_reference_program(ctx, &shared->DefaultFragmentProgram, NULL);

   if (shared->DefaultFragmentShader)
      _mesa_delete_ati_fragment_shader(ctx, shared->DefaultFragmentShader);

   _mesa_DeinitHashTable(&shared->ATIShaders, delete_fragshader_cb, ctx);
   _mesa_DeinitHashTable(&shared->BufferObjects, delete_bufferobj_cb, ctx);

   if (shared->ZombieBufferObjects) {
      set_foreach(shared->ZombieBufferObjects, entry) {
         assert(!"ZombieBufferObjects should be empty");
      }
      _mesa_set_destroy(shared->ZombieBufferObjects, NULL);
   }

   _mesa_DeinitHashTable(&shared->FrameBuffers, delete_framebuffer_cb, ctx);
   _mesa_DeinitHashTable(&shared->RenderBuffers, delete_renderbuffer_cb, ctx);

   if (shared->SyncObjects) {
      set_foreach(shared->SyncObjects, entry) {
         _mesa_unref_sync_object(ctx, (struct gl_sync_object *) entry->key, 1);
      }

      _mesa_set_destroy(shared->SyncObjects, NULL);
   }

   _mesa_DeinitHashTable(&shared->SamplerObjects, delete_sampler_object_cb,
                            ctx);

   /*
    * Free texture objects (after FBOs since some textures might have
    * been bound to FBOs).
    */
   /* the default textures */
   for (i = 0; i < NUM_TEXTURE_TARGETS; i++) {
      if (shared->DefaultTex[i])
         _mesa_delete_texture_object(ctx, shared->DefaultTex[i]);
   }

   /* all other textures */
   _mesa_DeinitHashTable(&shared->TexObjects, delete_texture_cb, ctx);

   _mesa_free_shared_handles(shared);

   /* ARB_shading_language_include */
   _mesa_destroy_shader_includes(shared);
   simple_mtx_destroy(&shared->ShaderIncludeMutex);

   _mesa_DeinitHashTable(&shared->MemoryObjects, delete_memory_object_cb,
                         ctx);
   _mesa_DeinitHashTable(&shared->SemaphoreObjects,
                         delete_semaphore_object_cb, ctx);

   simple_mtx_destroy(&shared->Mutex);
   simple_mtx_destroy(&shared->TexMutex);

   FREE(shared);
}


/**
 * gl_shared_state objects are ref counted.
 * If ptr's refcount goes to zero, free the shared state.
 */
void
_mesa_reference_shared_state(struct gl_context *ctx,
                             struct gl_shared_state **ptr,
                             struct gl_shared_state *state)
{
   if (*ptr == state)
      return;

   if (*ptr) {
      /* unref old state */
      struct gl_shared_state *old = *ptr;
      GLboolean delete;

      simple_mtx_lock(&old->Mutex);
      assert(old->RefCount >= 1);
      old->RefCount--;
      delete = (old->RefCount == 0);
      simple_mtx_unlock(&old->Mutex);

      if (delete) {
         free_shared_state(ctx, old);
      }

      *ptr = NULL;
   }

   if (state) {
      /* reference new state */
      simple_mtx_lock(&state->Mutex);
      state->RefCount++;
      *ptr = state;
      simple_mtx_unlock(&state->Mutex);
   }
}
