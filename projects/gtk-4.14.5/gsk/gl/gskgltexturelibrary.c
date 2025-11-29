/* gskgltexturelibrary.c
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gdk/gdkglcontextprivate.h>
#include <gsk/gskdebugprivate.h>

#include "gskglcommandqueueprivate.h"
#include "gskgldriverprivate.h"
#include "gskgltexturelibraryprivate.h"

#define DEFAULT_MAX_FRAME_AGE 60
#define DEFAULT_ATLAS_WIDTH 512
#define DEFAULT_ATLAS_HEIGHT 512
#define MAX_OLD_RATIO 0.5

G_DEFINE_ABSTRACT_TYPE (GskGLTextureLibrary, gsk_gl_texture_library, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_DRIVER,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
gsk_gl_texture_atlas_free (GskGLTextureAtlas *atlas)
{
  if (atlas->texture_id != 0)
    {
      glDeleteTextures (1, &atlas->texture_id);
      atlas->texture_id = 0;
    }

  g_clear_pointer (&atlas->nodes, g_free);
  g_free (atlas);
}

static gboolean
gsk_gl_texture_library_real_compact (GskGLTextureLibrary *self,
                                     gint64               frame_id)
{
  GPtrArray *removed = NULL;
  gboolean ret = FALSE;
  gboolean periodic_scan;

  g_assert (GSK_IS_GL_TEXTURE_LIBRARY (self));

  periodic_scan = (self->max_frame_age > 0 &&
                   (frame_id % self->max_frame_age) == 0);

  for (guint i = self->atlases->len; i > 0; i--)
    {
      GskGLTextureAtlas *atlas = g_ptr_array_index (self->atlases, i - 1);

      if (gsk_gl_texture_atlas_get_unused_ratio (atlas) > MAX_OLD_RATIO)
        {
          GSK_DEBUG (GLYPH_CACHE,
                     "Dropping atlas %d (%g.2%% old)", i,
                     100.0 * gsk_gl_texture_atlas_get_unused_ratio (atlas));
          if (removed == NULL)
            removed = g_ptr_array_new_with_free_func ((GDestroyNotify)gsk_gl_texture_atlas_free);
          g_ptr_array_add (removed, g_ptr_array_steal_index (self->atlases, i - 1));
        }
    }

  if (periodic_scan || removed != NULL)
    {
      GskGLTextureAtlasEntry *entry;
      GHashTableIter iter;
      guint dropped = 0;
      G_GNUC_UNUSED guint atlased = 0;

      g_hash_table_iter_init (&iter, self->hash_table);
      while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&entry))
        {
          if (entry->is_atlased)
            {
              if (removed && g_ptr_array_find (removed, entry->atlas, NULL))
                {
                  g_hash_table_iter_remove (&iter);
                  dropped++;
                }
              else if (periodic_scan)
                {
                  gsk_gl_texture_atlas_entry_mark_unused (entry);
                  entry->accessed = FALSE;
                  if (entry->is_atlased)
                    atlased++;
                }
            }
          else
            {
              if (!entry->accessed)
                {
                  if (entry->texture)
                    gsk_gl_driver_release_texture (self->driver, entry->texture);
                  g_hash_table_iter_remove (&iter);
                  dropped++;
                }
              else if (periodic_scan)
                entry->accessed = FALSE;
            }
        }

      GSK_DEBUG (GLYPH_CACHE, "%s: Dropped %d individual items",
                              G_OBJECT_TYPE_NAME (self),
                              dropped);
      GSK_DEBUG (GLYPH_CACHE, "%s: %d items cached (%d atlased, %d individually)",
                              G_OBJECT_TYPE_NAME (self),
                              g_hash_table_size (self->hash_table),
                              atlased,
                              g_hash_table_size (self->hash_table) - atlased);

      if (dropped > 0)
        gsk_gl_texture_library_clear_cache (self);

      ret = TRUE;

      g_clear_pointer (&removed, g_ptr_array_unref);
    }

  if (GSK_DEBUG_CHECK (GLYPH_CACHE))
    {
      static gint64 last_message;
      gint64 now = g_get_monotonic_time ();
      if (now - last_message > G_USEC_PER_SEC)
        {
          last_message = now;
          gdk_debug_message ("%s contains %d atlases",
                             G_OBJECT_TYPE_NAME (self),
                             self->atlases->len);
        }
    }

  return ret;
}

static gboolean
gsk_gl_texture_library_real_allocate (GskGLTextureLibrary *self,
                                      GskGLTextureAtlas   *atlas,
                                      int                  width,
                                      int                  height,
                                      int                 *out_x,
                                      int                 *out_y)
{
  stbrp_rect rect;

  g_assert (GSK_IS_GL_TEXTURE_LIBRARY (self));
  g_assert (atlas != NULL);
  g_assert (width > 0);
  g_assert (height > 0);
  g_assert (out_x != NULL);
  g_assert (out_y != NULL);

  rect.w = width;
  rect.h = height;

  stbrp_pack_rects (&atlas->context, &rect, 1);

  if (rect.was_packed)
    {
      *out_x = rect.x;
      *out_y = rect.y;
    }

  return rect.was_packed;
}

static void
gsk_gl_texture_library_constructed (GObject *object)
{
  G_OBJECT_CLASS (gsk_gl_texture_library_parent_class)->constructed (object);

  g_assert (GSK_GL_TEXTURE_LIBRARY (object)->hash_table != NULL);
}

static void
gsk_gl_texture_library_dispose (GObject *object)
{
  GskGLTextureLibrary *self = (GskGLTextureLibrary *)object;

  g_clear_pointer (&self->atlases, g_ptr_array_unref);
  g_clear_object (&self->driver);
  g_clear_pointer (&self->hash_table, g_hash_table_unref);

  G_OBJECT_CLASS (gsk_gl_texture_library_parent_class)->dispose (object);
}

static void
gsk_gl_texture_library_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GskGLTextureLibrary *self = GSK_GL_TEXTURE_LIBRARY (object);

  switch (prop_id)
    {
    case PROP_DRIVER:
      g_value_set_object (value, self->driver);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gsk_gl_texture_library_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GskGLTextureLibrary *self = GSK_GL_TEXTURE_LIBRARY (object);

  switch (prop_id)
    {
    case PROP_DRIVER:
      self->driver = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gsk_gl_texture_library_class_init (GskGLTextureLibraryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gsk_gl_texture_library_constructed;
  object_class->dispose = gsk_gl_texture_library_dispose;
  object_class->get_property = gsk_gl_texture_library_get_property;
  object_class->set_property = gsk_gl_texture_library_set_property;

  klass->compact = gsk_gl_texture_library_real_compact;
  klass->allocate = gsk_gl_texture_library_real_allocate;

  properties [PROP_DRIVER] =
    g_param_spec_object ("driver", NULL, NULL,
                         GSK_TYPE_GL_DRIVER,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gsk_gl_texture_library_init (GskGLTextureLibrary *self)
{
  self->max_frame_age = DEFAULT_MAX_FRAME_AGE;
  self->atlas_width = DEFAULT_ATLAS_WIDTH;
  self->atlas_height = DEFAULT_ATLAS_HEIGHT;
  self->atlases = g_ptr_array_new_with_free_func ((GDestroyNotify)gsk_gl_texture_atlas_free);
}

void
gsk_gl_texture_library_set_funcs (GskGLTextureLibrary *self,
                                  GHashFunc            hash_func,
                                  GEqualFunc           equal_func,
                                  GDestroyNotify       key_destroy,
                                  GDestroyNotify       value_destroy)
{
  g_return_if_fail (GSK_IS_GL_TEXTURE_LIBRARY (self));
  g_return_if_fail (self->hash_table == NULL);

  self->hash_table = g_hash_table_new_full (hash_func, equal_func,
                                            key_destroy, value_destroy);
}

void
gsk_gl_texture_library_begin_frame (GskGLTextureLibrary *self,
                                    gint64               frame_id)
{
  g_return_if_fail (GSK_IS_GL_TEXTURE_LIBRARY (self));

  gsk_gl_texture_library_compact (self, frame_id);

  if (GSK_GL_TEXTURE_LIBRARY_GET_CLASS (self)->begin_frame)
    GSK_GL_TEXTURE_LIBRARY_GET_CLASS (self)->begin_frame (self, frame_id);
}

static GskGLTexture *
gsk_gl_texture_library_pack_one (GskGLTextureLibrary *self,
                                 guint                width,
                                 guint                height)
{
  GskGLTexture *texture;

  g_assert (GSK_IS_GL_TEXTURE_LIBRARY (self));

  if (width > self->driver->command_queue->max_texture_size ||
      height > self->driver->command_queue->max_texture_size)
    {
      g_warning ("Clipping requested texture of size %ux%u to maximum allowable size %u.",
                 width, height, self->driver->command_queue->max_texture_size);
      width = MIN (width, self->driver->command_queue->max_texture_size);
      height = MIN (height, self->driver->command_queue->max_texture_size);
    }

  texture = gsk_gl_driver_create_texture (self->driver, width, height, GL_RGBA8);
  texture->permanent = TRUE;

  return texture;
}

static void
gsk_gl_texture_library_pack_any_atlas (GskGLTextureLibrary  *self,
                                       int                   width,
                                       int                   height,
                                       GskGLTextureAtlas   **out_atlas,
                                       int                  *out_x,
                                       int                  *out_y)
{
  GskGLTextureAtlas *atlas = NULL;
  int x, y;

  g_assert (GSK_IS_GL_TEXTURE_LIBRARY (self));
  g_assert (width > 0);
  g_assert (height > 0);
  g_assert (out_atlas != NULL);
  g_assert (out_x != NULL);
  g_assert (out_y != NULL);

  for (guint i = 0; i < self->atlases->len; i++)
    {
      atlas = g_ptr_array_index (self->atlases, i);

      if (gsk_gl_texture_library_allocate (self, atlas, width, height, &x, &y))
        break;

      atlas = NULL;
    }

  if (atlas == NULL)
    {
      /* No atlas has enough space, so create a new one... */
      atlas = gsk_gl_texture_library_acquire_atlas (self);

      /* Pack it onto that one, which surely has enough space... */
      if (!gsk_gl_texture_library_allocate (self, atlas, width, height, &x, &y))
        g_assert_not_reached ();
    }

  *out_atlas = atlas;
  *out_x = x;
  *out_y = y;
}

gpointer
gsk_gl_texture_library_pack (GskGLTextureLibrary *self,
                             gpointer             key,
                             gsize                valuelen,
                             guint                width,
                             guint                height,
                             int                  padding,
                             guint               *out_packed_x,
                             guint               *out_packed_y)
{
  GskGLTextureAtlasEntry *entry;
  GskGLTextureAtlas *atlas = NULL;

  g_assert (GSK_IS_GL_TEXTURE_LIBRARY (self));
  g_assert (key != NULL);
  g_assert (valuelen > sizeof (GskGLTextureAtlasEntry));
  g_assert (out_packed_x != NULL);
  g_assert (out_packed_y != NULL);

  entry = g_malloc0 (valuelen);
  entry->n_pixels = width * height;
  entry->accessed = TRUE;
  entry->used = TRUE;

  /* If our size is invisible then we just want an entry in the
   * cache for faster lookups, but do not actually spend any texture
   * allocations on this entry.
   */
  if (width <= 0 && height <= 0)
    {
      entry->is_atlased = FALSE;
      entry->texture = NULL;
      entry->area.x = 0.0f;
      entry->area.y = 0.0f;
      entry->area.x2 = 0.0f;
      entry->area.y2 = 0.0f;

      *out_packed_x = 0;
      *out_packed_y = 0;
    }
  else if (self->max_entry_size == 0 ||
           (width <= self->max_entry_size &&
            height <= self->max_entry_size))
    {
      int packed_x;
      int packed_y;

      gsk_gl_texture_library_pack_any_atlas (self,
                                             padding + width + padding,
                                             padding + height + padding,
                                             &atlas,
                                             &packed_x,
                                             &packed_y);

      entry->atlas = atlas;
      entry->is_atlased = TRUE;
      entry->area.x = (packed_x + padding) / (float)atlas->width;
      entry->area.y = (packed_y + padding) / (float)atlas->height;
      entry->area.x2 = (packed_x + padding + width) / (float)atlas->width;
      entry->area.y2 = (packed_y + padding + height) / (float)atlas->height;

      *out_packed_x = packed_x;
      *out_packed_y = packed_y;
    }
  else
    {
      GskGLTexture *texture = gsk_gl_texture_library_pack_one (self,
                                                               padding + width + padding,
                                                               padding + height + padding);

      entry->texture = texture;
      entry->is_atlased = FALSE;
      entry->area.x = padding / (float) (padding + width + padding);
      entry->area.y = padding / (float) (padding + height + padding);
      entry->area.x2 = (padding + width) / (float) (padding + width + padding);
      entry->area.y2 = (padding + height) / (float) (padding + height + padding);

      *out_packed_x = 0;
      *out_packed_y = 0;
    }

  g_hash_table_insert (self->hash_table, key, entry);

  return entry;
}

/*
 * gsk_gl_texture_library_clear_cache:
 *
 * Clear the front cache if the texture library is using one. For
 * example the glyph cache would drop it's front cache to force
 * next lookups to fall through to the GHashTable key lookup.
 */
void
gsk_gl_texture_library_clear_cache (GskGLTextureLibrary *self)
{
  g_return_if_fail (GSK_IS_GL_TEXTURE_LIBRARY (self));

  if (GSK_GL_TEXTURE_LIBRARY_GET_CLASS (self)->clear_cache)
    GSK_GL_TEXTURE_LIBRARY_GET_CLASS (self)->clear_cache (self);
}

/*
 * gsk_gl_texture_library_compact:
 *
 * Requests that the texture library compact it's altases. That
 * generally means to traverse them to look for unused pixels over
 * a certain threshold and release them if necessary.
 *
 * Returns: %TRUE if any compaction occurred.
 */
gboolean
gsk_gl_texture_library_compact (GskGLTextureLibrary *self,
                                gint64               frame_id)
{
  g_return_val_if_fail (GSK_IS_GL_TEXTURE_LIBRARY (self), FALSE);

  if (GSK_GL_TEXTURE_LIBRARY_GET_CLASS (self)->compact)
    return GSK_GL_TEXTURE_LIBRARY_GET_CLASS (self)->compact (self, frame_id);

  return FALSE;
}

void
gsk_gl_texture_library_reset (GskGLTextureLibrary *self)
{
  g_return_if_fail (GSK_IS_GL_TEXTURE_LIBRARY (self));

  gsk_gl_texture_library_clear_cache (self);

  g_hash_table_remove_all (self->hash_table);

  if (self->atlases->len)
    g_ptr_array_remove_range (self->atlases, 0, self->atlases->len);
}

void
gsk_gl_texture_library_set_atlas_size (GskGLTextureLibrary *self,
                                       int                  width,
                                       int                  height)
{
  g_return_if_fail (GSK_IS_GL_TEXTURE_LIBRARY (self));

  if (width <= 0)
    width = DEFAULT_ATLAS_WIDTH;

  if (height <= 0)
    height = DEFAULT_ATLAS_HEIGHT;

  self->atlas_height = height;
  self->atlas_width = width;

  gsk_gl_texture_library_reset (self);
}

/*
 * gsk_gl_texture_library_acquire_atlas:
 *
 * Allocates a new texture atlas based on the current size
 * and format requirements.
 */
GskGLTextureAtlas *
gsk_gl_texture_library_acquire_atlas (GskGLTextureLibrary *self)
{
  GskGLTextureAtlas *atlas;

  g_return_val_if_fail (GSK_IS_GL_TEXTURE_LIBRARY (self), NULL);
  g_return_val_if_fail (GSK_IS_GL_DRIVER (self->driver), NULL);
  g_return_val_if_fail (GSK_IS_GL_COMMAND_QUEUE (self->driver->command_queue), NULL);
  g_return_val_if_fail (self->atlas_width > 0, NULL);
  g_return_val_if_fail (self->atlas_height > 0, NULL);

  atlas = g_new0 (GskGLTextureAtlas, 1);
  atlas->width = self->atlas_width;
  atlas->height = self->atlas_height;
  /* TODO: We might want to change the strategy about the amount of
   *       nodes here? stb_rect_pack.h says width is optimal. */
  atlas->nodes = g_malloc0_n (atlas->width, sizeof (struct stbrp_node));
  stbrp_init_target (&atlas->context, atlas->width, atlas->height, atlas->nodes, atlas->width);
  atlas->texture_id = gsk_gl_command_queue_create_texture (self->driver->command_queue,
                                                           atlas->width,
                                                           atlas->height,
                                                           GL_RGBA8);

  gdk_gl_context_label_object_printf (gdk_gl_context_get_current (),
                                      GL_TEXTURE, atlas->texture_id,
                                      "Texture atlas %d",
                                      atlas->texture_id);

  g_ptr_array_add (self->atlases, atlas);

  if (GSK_GL_TEXTURE_LIBRARY_GET_CLASS (self)->init_atlas)
    GSK_GL_TEXTURE_LIBRARY_GET_CLASS (self)->init_atlas (self, atlas);

  return atlas;
}

gboolean
gsk_gl_texture_library_allocate (GskGLTextureLibrary *self,
                                 GskGLTextureAtlas   *atlas,
                                 int                  width,
                                 int                  height,
                                 int                 *out_x,
                                 int                 *out_y)
{
  g_assert (GSK_IS_GL_TEXTURE_LIBRARY (self));
  g_assert (atlas != NULL);
  g_assert (width > 0);
  g_assert (height > 0);
  g_assert (out_x != NULL);
  g_assert (out_y != NULL);

  return GSK_GL_TEXTURE_LIBRARY_GET_CLASS (self)->allocate (self, atlas, width, height, out_x, out_y);
}
