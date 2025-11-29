/* gskgltexturelibraryprivate.h
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

#include "gskgltypesprivate.h"
#include "gskgltextureprivate.h"

#include "stb_rect_pack.h"

G_BEGIN_DECLS

#define GSK_TYPE_GL_TEXTURE_LIBRARY            (gsk_gl_texture_library_get_type ())
#define GSK_GL_TEXTURE_LIBRARY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GSK_TYPE_GL_TEXTURE_LIBRARY, GskGLTextureLibrary))
#define GSK_IS_GL_TEXTURE_LIBRARY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GSK_TYPE_GL_TEXTURE_LIBRARY))
#define GSK_GL_TEXTURE_LIBRARY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GSK_TYPE_GL_TEXTURE_LIBRARY, GskGLTextureLibraryClass))
#define GSK_IS_GL_TEXTURE_LIBRARY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GSK_TYPE_GL_TEXTURE_LIBRARY))
#define GSK_GL_TEXTURE_LIBRARY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GSK_TYPE_GL_TEXTURE_LIBRARY, GskGLTextureLibraryClass))

typedef struct _GskGLTextureAtlas
{
  struct stbrp_context context;
  struct stbrp_node *nodes;

  int width;
  int height;

  guint texture_id;

  /* Pixels of rects that have been used at some point,
   * But are now unused.
   */
  int unused_pixels;

} GskGLTextureAtlas;

typedef struct _GskGLTextureAtlasEntry
{
  /* A backreference to either the atlas or texture containing
   * the contents of the atlas entry. For larger items, no atlas
   * is used and instead a direct texture.
   */
  union {
    GskGLTextureAtlas *atlas;
    GskGLTexture *texture;
  };

  /* The area within the atlas translated to 0..1 bounds */
  struct {
    float x;
    float y;
    float x2;
    float y2;
  } area;

  /* Number of pixels in the entry, used to calculate usage
   * of an atlas while processing.
   */
  guint n_pixels : 29;

  /* If entry has marked pixels as used in the atlas this frame */
  guint used : 1;

  /* If entry was accessed this frame */
  guint accessed : 1;

  /* When true, backref is an atlas, otherwise texture */
  guint is_atlased : 1;
} GskGLTextureAtlasEntry;

typedef struct _GskGLTextureLibrary
{
  GObject      parent_instance;
  GskGLDriver *driver;
  GPtrArray   *atlases;
  GHashTable  *hash_table;
  guint        max_entry_size;
  guint        max_frame_age;
  guint        atlas_width;
  guint        atlas_height;
} GskGLTextureLibrary;

typedef struct _GskGLTextureLibraryClass
{
  GObjectClass parent_class;

  void     (*begin_frame) (GskGLTextureLibrary *library,
                           gint64               frame_id);
  gboolean (*compact)     (GskGLTextureLibrary *library,
                           gint64               frame_id);
  void     (*clear_cache) (GskGLTextureLibrary *library);
  void     (*init_atlas)  (GskGLTextureLibrary *library,
                           GskGLTextureAtlas   *atlas);
  gboolean (*allocate)    (GskGLTextureLibrary *library,
                           GskGLTextureAtlas   *atlas,
                           int                  width,
                           int                  height,
                           int                 *out_x,
                           int                 *out_y);
} GskGLTextureLibraryClass;

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GskGLTextureLibrary, g_object_unref)

GType              gsk_gl_texture_library_get_type       (void) G_GNUC_CONST;
gboolean           gsk_gl_texture_library_compact        (GskGLTextureLibrary *self,
                                                          gint64               frame_id);
void               gsk_gl_texture_library_clear_cache    (GskGLTextureLibrary *self);
void               gsk_gl_texture_library_reset          (GskGLTextureLibrary *self);
void               gsk_gl_texture_library_set_atlas_size (GskGLTextureLibrary *self,
                                                          int                  width,
                                                          int                  height);
GskGLTextureAtlas *gsk_gl_texture_library_acquire_atlas  (GskGLTextureLibrary *self);
void               gsk_gl_texture_library_set_funcs      (GskGLTextureLibrary *self,
                                                          GHashFunc            hash_func,
                                                          GEqualFunc           equal_func,
                                                          GDestroyNotify       key_destroy,
                                                          GDestroyNotify       value_destroy);
void               gsk_gl_texture_library_begin_frame    (GskGLTextureLibrary *self,
                                                          gint64               frame_id);
gboolean           gsk_gl_texture_library_allocate       (GskGLTextureLibrary *self,
                                                          GskGLTextureAtlas   *atlas,
                                                          int                  width,
                                                          int                  height,
                                                          int                 *out_x,
                                                          int                 *out_y);
gpointer           gsk_gl_texture_library_pack           (GskGLTextureLibrary *self,
                                                          gpointer             key,
                                                          gsize                valuelen,
                                                          guint                width,
                                                          guint                height,
                                                          int                  padding,
                                                          guint               *out_packed_x,
                                                          guint               *out_packed_y);

static inline void
gsk_gl_texture_atlas_mark_unused (GskGLTextureAtlas *self,
                                  int                n_pixels)
{
  g_assert (n_pixels >= 0);

  self->unused_pixels += n_pixels;
}

static inline void
gsk_gl_texture_atlas_entry_mark_used (GskGLTextureAtlasEntry *entry)
{
  if (entry->used == TRUE || entry->is_atlased == FALSE)
    return;

  entry->atlas->unused_pixels -= entry->n_pixels;
  entry->used = TRUE;
}

static inline void
gsk_gl_texture_atlas_entry_mark_unused (GskGLTextureAtlasEntry *entry)
{
  if (entry->used == FALSE || entry->is_atlased == FALSE)
    return;

  entry->atlas->unused_pixels += entry->n_pixels;
  entry->used = FALSE;
}

static inline gboolean
gsk_gl_texture_library_lookup (GskGLTextureLibrary     *self,
                               gconstpointer            key,
                               GskGLTextureAtlasEntry **out_entry)
{
  GskGLTextureAtlasEntry *entry = g_hash_table_lookup (self->hash_table, key);

  if G_LIKELY (entry != NULL && entry->accessed && entry->used)
    {
      *out_entry = entry;
      return TRUE;
    }

  if (entry != NULL)
    {
      gsk_gl_texture_atlas_entry_mark_used (entry);
      entry->accessed = TRUE;
      *out_entry = entry;
      return TRUE;
    }

  return FALSE;
}

static inline guint
GSK_GL_TEXTURE_ATLAS_ENTRY_TEXTURE (gconstpointer d)
{
  const GskGLTextureAtlasEntry *e = d;

  return e->is_atlased ? e->atlas->texture_id
                       : e->texture ? e->texture->texture_id : 0;
}

static inline double
gsk_gl_texture_atlas_get_unused_ratio (const GskGLTextureAtlas *self)
{
  if (self->unused_pixels > 0)
    return (double)(self->unused_pixels) / (double)(self->width * self->height);
  return 0.0;
}

static inline gboolean
gsk_gl_texture_library_can_cache (GskGLTextureLibrary *self,
                                  int                  width,
                                  int                  height)
{
  g_assert (self->max_entry_size > 0);
  return width <= self->max_entry_size && height <= self->max_entry_size;
}

G_END_DECLS

