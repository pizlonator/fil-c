/* gskgldriverprivate.h
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <gdk/gdkgltextureprivate.h>

#include "gskgltypesprivate.h"
#include "gskgltextureprivate.h"

G_BEGIN_DECLS

enum {
  UNIFORM_SHARED_ALPHA,
  UNIFORM_SHARED_SOURCE,
  UNIFORM_SHARED_CLIP_RECT,
  UNIFORM_SHARED_VIEWPORT,
  UNIFORM_SHARED_PROJECTION,
  UNIFORM_SHARED_MODELVIEW,

  UNIFORM_SHARED_LAST
};

enum {
  UNIFORM_CUSTOM_SIZE = UNIFORM_SHARED_LAST,
  UNIFORM_CUSTOM_TEXTURE1,
  UNIFORM_CUSTOM_TEXTURE2,
  UNIFORM_CUSTOM_TEXTURE3,
  UNIFORM_CUSTOM_TEXTURE4,
  UNIFORM_CUSTOM_ARG0,
  UNIFORM_CUSTOM_ARG1,
  UNIFORM_CUSTOM_ARG2,
  UNIFORM_CUSTOM_ARG3,
  UNIFORM_CUSTOM_ARG4,
  UNIFORM_CUSTOM_ARG5,
  UNIFORM_CUSTOM_ARG6,
  UNIFORM_CUSTOM_ARG7,

  UNIFORM_CUSTOM_LAST
};

typedef struct {
  gconstpointer   pointer;
  float           scale_x;
  float           scale_y;
  int             pointer_is_child;
  graphene_rect_t parent_rect; /* Valid when pointer_is_child */
} GskTextureKey;

#define GSK_GL_NO_UNIFORMS CONCAT_EXPANDED(UNIFORM_INVALID_,__COUNTER__)
#define CONCAT_EXPANDED(a,b) CONCAT_EXPANDED2(a,b)
#define CONCAT_EXPANDED2(a,b) a##b
#define GSK_GL_ADD_UNIFORM(pos, KEY, name) UNIFORM_##KEY = UNIFORM_SHARED_LAST + pos,
#define GSK_GL_DEFINE_PROGRAM(name, resource, uniforms) enum { uniforms };
#define GSK_GL_DEFINE_PROGRAM_NO_CLIP(name, resource, uniforms) enum { uniforms };
# include "gskglprograms.defs"
#undef GSK_GL_DEFINE_PROGRAM_NO_CLIP
#undef GSK_GL_DEFINE_PROGRAM
#undef GSK_GL_ADD_UNIFORM
#undef GSK_GL_NO_UNIFORMS
#undef CONCAT_EXPANDED
#undef CONCAT_EXPANDED2

#define GSK_TYPE_GL_DRIVER (gsk_gl_driver_get_type())

G_DECLARE_FINAL_TYPE (GskGLDriver, gsk_gl_driver, GSK, GL_DRIVER, GObject)

struct _GskGLRenderTarget
{
  guint framebuffer_id;
  guint texture_id;
  int format;
  int width;
  int height;
};

struct _GskGLDriver
{
  GObject parent_instance;

  GskGLCommandQueue *shared_command_queue;
  GskGLCommandQueue *command_queue;

  GskGLGlyphLibrary *glyphs_library;
  GskGLIconLibrary *icons_library;
  GskGLShadowLibrary *shadows_library;

  GArray *texture_pool;
  GHashTable *textures;
  GHashTable *key_to_texture_id;
  GHashTable *texture_id_to_key;

  GHashTable *shader_cache;

  GArray *autorelease_framebuffers;
  GPtrArray *render_targets;

#define GSK_GL_NO_UNIFORMS
#define GSK_GL_ADD_UNIFORM(pos, KEY, name)
#define GSK_GL_DEFINE_PROGRAM(name, resource, uniforms) \
  GskGLProgram *name ## _no_clip; \
  GskGLProgram *name ## _rect_clip; \
  GskGLProgram *name;
#define GSK_GL_DEFINE_PROGRAM_NO_CLIP(name, resource, uniforms) \
  GskGLProgram *name;
# include "gskglprograms.defs"
#undef GSK_GL_NO_UNIFORMS
#undef GSK_GL_ADD_UNIFORM
#undef GSK_GL_DEFINE_PROGRAM
#undef GSK_GL_DEFINE_PROGRAM_NO_CLIP

  gint64 current_frame_id;

  /* Used to reduce number of comparisons */
  guint stamps[UNIFORM_SHARED_LAST];

  guint debug : 1;
  guint in_frame : 1;
};

GskGLDriver       * gsk_gl_driver_for_display            (GdkDisplay          *display,
                                                          gboolean             debug_shaders,
                                                          GError             **error);
GskGLCommandQueue * gsk_gl_driver_create_command_queue   (GskGLDriver         *self,
                                                          GdkGLContext        *context);
GdkGLContext      * gsk_gl_driver_get_context            (GskGLDriver         *self);
gboolean            gsk_gl_driver_create_render_target   (GskGLDriver         *self,
                                                          int                  width,
                                                          int                  height,
                                                          int                  format,
                                                          GskGLRenderTarget  **render_target);
guint               gsk_gl_driver_release_render_target  (GskGLDriver         *self,
                                                          GskGLRenderTarget   *render_target,
                                                          gboolean             release_texture);
void                gsk_gl_driver_begin_frame            (GskGLDriver         *self,
                                                          GskGLCommandQueue   *command_queue);
void                gsk_gl_driver_end_frame              (GskGLDriver         *self);
void                gsk_gl_driver_after_frame            (GskGLDriver         *self);
GdkTexture        * gsk_gl_driver_create_gdk_texture     (GskGLDriver         *self,
                                                          guint                texture_id,
                                                          GdkMemoryFormat      format);
void                gsk_gl_driver_cache_texture          (GskGLDriver         *self,
                                                          const GskTextureKey *key,
                                                          guint                texture_id);
guint               gsk_gl_driver_load_texture           (GskGLDriver         *self,
                                                          GdkTexture          *texture,
                                                          gboolean             ensure_mipmap);
GskGLTexture      * gsk_gl_driver_create_texture         (GskGLDriver         *self,
                                                          float                width,
                                                          float                height,
                                                          int                  format);
void                gsk_gl_driver_release_texture        (GskGLDriver         *self,
                                                          GskGLTexture        *texture);
void                gsk_gl_driver_release_texture_by_id  (GskGLDriver         *self,
                                                          guint                texture_id);
GskGLTexture      * gsk_gl_driver_mark_texture_permanent (GskGLDriver         *self,
                                                          guint                texture_id);
void                gsk_gl_driver_add_texture_slices     (GskGLDriver         *self,
                                                          GdkTexture          *texture,
                                                          gboolean             ensure_mipmap,
                                                          GskGLTextureSlice  **out_slices,
                                                          guint               *out_n_slices);
GskGLProgram      * gsk_gl_driver_lookup_shader          (GskGLDriver         *self,
                                                          GskGLShader         *shader,
                                                          GError             **error);

#if 0
void                gsk_gl_driver_save_texture_to_png    (GskGLDriver         *self,
                                                          int                  texture_id,
                                                          int                  width,
                                                          int                  height,
                                                          const char          *filename);
void                gsk_gl_driver_save_atlases_to_png    (GskGLDriver         *self,
                                                          const char          *filename);
#endif

static inline GskGLTexture *
gsk_gl_driver_get_texture_by_id (GskGLDriver *self,
                                 guint        texture_id)
{
  return g_hash_table_lookup (self->textures, GUINT_TO_POINTER (texture_id));
}

/**
 * gsk_gl_driver_lookup_texture:
 * @self: a `GskGLDriver`
 * @key: the key for the texture
 * @has_mipmap: (out): Return location for whether the texture has a mipmap
 *
 * Looks up a texture in the texture cache by @key.
 *
 * If the texture could not be found, then zero is returned.
 *
 * Returns: a positive integer if the texture was found; otherwise 0.
 */
static inline guint
gsk_gl_driver_lookup_texture (GskGLDriver         *self,
                              const GskTextureKey *key,
                              gboolean            *has_mipmap)
{
  gpointer id;

  if (g_hash_table_lookup_extended (self->key_to_texture_id, key, NULL, &id))
    {
      GskGLTexture *texture = g_hash_table_lookup (self->textures, id);

      if (texture != NULL)
        texture->last_used_in_frame = self->current_frame_id;

       if (has_mipmap)
         *has_mipmap = texture ? texture->has_mipmap : FALSE;

      return GPOINTER_TO_UINT (id);
    }

  return 0;
}

static inline void
gsk_gl_driver_slice_texture (GskGLDriver        *self,
                             GdkTexture         *texture,
                             gboolean            ensure_mipmap,
                             GskGLTextureSlice **out_slices,
                             guint              *out_n_slices)
{
  GskGLTexture *t;

  t = gdk_texture_get_render_data (texture, self);

  if (t && t->slices &&
      (t->has_mipmap || !ensure_mipmap))
    {
      *out_slices = t->slices;
      *out_n_slices = t->n_slices;
      return;
    }

  gsk_gl_driver_add_texture_slices (self, texture, ensure_mipmap, out_slices, out_n_slices);
}

G_END_DECLS

