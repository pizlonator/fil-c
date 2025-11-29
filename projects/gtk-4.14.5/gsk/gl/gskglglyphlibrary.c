/* gskglglyphlibrary.c
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
#include <gdk/gdkmemoryformatprivate.h>
#include <gdk/gdkprofilerprivate.h>

#include "gskglcommandqueueprivate.h"
#include "gskgldriverprivate.h"
#include "gskglglyphlibraryprivate.h"

#include "gskdebugprivate.h"

#define MAX_GLYPH_SIZE 128

G_DEFINE_TYPE (GskGLGlyphLibrary, gsk_gl_glyph_library, GSK_TYPE_GL_TEXTURE_LIBRARY)

GskGLGlyphLibrary *
gsk_gl_glyph_library_new (GskGLDriver *driver)
{
  g_return_val_if_fail (GSK_IS_GL_DRIVER (driver), NULL);

  return g_object_new (GSK_TYPE_GL_GLYPH_LIBRARY,
                       "driver", driver,
                       NULL);
}

static guint
gsk_gl_glyph_key_hash (gconstpointer data)
{
  const GskGLGlyphKey *key = data;

  /* We do not store the hash within the key because GHashTable will already
   * store the hash value for us and so this is called only a single time per
   * cached item. This saves an extra 4 bytes per GskGLGlyphKey which means on
   * 64-bit, we fit nicely within 2 pointers (the smallest allocation size
   * for GSlice).
   */

  return GPOINTER_TO_UINT (key->font) ^
         key->glyph ^
         (key->xshift << 24) ^
         (key->yshift << 26) ^
         key->scale;
}

static gboolean
gsk_gl_glyph_key_equal (gconstpointer v1,
                        gconstpointer v2)
{
  return memcmp (v1, v2, sizeof (GskGLGlyphKey)) == 0;
}

static void
gsk_gl_glyph_key_free (gpointer data)
{
  GskGLGlyphKey *key = data;

  g_clear_object (&key->font);
  g_free (key);
}

static void
gsk_gl_glyph_value_free (gpointer data)
{
  g_free (data);
}

static void
gsk_gl_glyph_library_clear_cache (GskGLTextureLibrary *library)
{
  GskGLGlyphLibrary *self = (GskGLGlyphLibrary *)library;

  g_assert (GSK_IS_GL_GLYPH_LIBRARY (self));

  memset (self->front, 0, sizeof self->front);
}

static void
gsk_gl_glyph_library_init_atlas (GskGLTextureLibrary *self,
                                 GskGLTextureAtlas   *atlas)
{
  gboolean packed G_GNUC_UNUSED;
  int x, y;
  guint gl_format;
  guint gl_type;
  guint8 pixel_data[4 * 3 * 3];

  g_assert (GSK_IS_GL_GLYPH_LIBRARY (self));
  g_assert (atlas != NULL);

  /* Insert a single pixel at 0,0 for use in coloring */

  gdk_gl_context_push_debug_group_printf (gdk_gl_context_get_current (),
                                          "Initializing Atlas");

  packed = gsk_gl_texture_library_allocate (self, atlas, 3, 3, &x, &y);
  g_assert (packed);
  g_assert (x == 0 && y == 0);

  memset (pixel_data, 255, sizeof pixel_data);

  if (gdk_gl_context_get_use_es (gdk_gl_context_get_current ()))
    {
      gl_format = GL_RGBA;
      gl_type = GL_UNSIGNED_BYTE;
    }
  else
    {
      gl_format = GL_BGRA;
      gl_type = GL_UNSIGNED_INT_8_8_8_8_REV;
    }
  glBindTexture (GL_TEXTURE_2D, atlas->texture_id);

  glTexSubImage2D (GL_TEXTURE_2D, 0,
                   0, 0,
                   3, 3,
                   gl_format, gl_type,
                   pixel_data);

  gdk_gl_context_pop_debug_group (gdk_gl_context_get_current ());

  self->driver->command_queue->n_uploads++;
}


static void
gsk_gl_glyph_library_finalize (GObject *object)
{
  GskGLGlyphLibrary *self = (GskGLGlyphLibrary *)object;

  g_clear_pointer (&self->surface_data, g_free);

  G_OBJECT_CLASS (gsk_gl_glyph_library_parent_class)->finalize (object);
}

static void
gsk_gl_glyph_library_class_init (GskGLGlyphLibraryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GskGLTextureLibraryClass *library_class = GSK_GL_TEXTURE_LIBRARY_CLASS (klass);

  object_class->finalize = gsk_gl_glyph_library_finalize;

  library_class->clear_cache = gsk_gl_glyph_library_clear_cache;
  library_class->init_atlas = gsk_gl_glyph_library_init_atlas;
}

static void
gsk_gl_glyph_library_init (GskGLGlyphLibrary *self)
{
  GskGLTextureLibrary *tl = (GskGLTextureLibrary *)self;

  tl->max_entry_size = MAX_GLYPH_SIZE;
  gsk_gl_texture_library_set_funcs (tl,
                                    gsk_gl_glyph_key_hash,
                                    gsk_gl_glyph_key_equal,
                                    gsk_gl_glyph_key_free,
                                    gsk_gl_glyph_value_free);
}

static cairo_surface_t *
gsk_gl_glyph_library_create_surface (GskGLGlyphLibrary *self,
                                     int                stride,
                                     int                width,
                                     int                height,
                                     int                uwidth,
                                     int                uheight)
{
  cairo_surface_t *surface;
  gsize n_bytes;

  g_assert (GSK_IS_GL_GLYPH_LIBRARY (self));
  g_assert (width > 0);
  g_assert (height > 0);

  n_bytes = stride * height;

  if G_LIKELY (n_bytes > self->surface_data_len)
    {
      self->surface_data = g_realloc (self->surface_data, n_bytes);
      self->surface_data_len = n_bytes;
    }

  memset (self->surface_data, 0, n_bytes);
  surface = cairo_image_surface_create_for_data (self->surface_data,
                                                 CAIRO_FORMAT_ARGB32,
                                                 width, height, stride);
  cairo_surface_set_device_scale (surface, width / (double)uwidth, height / (double)uheight);

  return surface;
}

static void
render_glyph (cairo_surface_t           *surface,
              const GskGLGlyphKey       *key,
              const GskGLGlyphValue     *value)
{
  cairo_t *cr;
  PangoGlyphString glyph_string;
  PangoGlyphInfo glyph_info;

  g_assert (surface != NULL);

  cr = cairo_create (surface);
  cairo_set_source_rgba (cr, 1, 1, 1, 1);

  glyph_info.glyph = key->glyph;
  glyph_info.geometry.width = value->ink_rect.width * 1024;
  glyph_info.geometry.x_offset = (0.25 * key->xshift - value->ink_rect.x) * 1024;
  glyph_info.geometry.y_offset = (0.25 * key->yshift - value->ink_rect.y) * 1024;

  glyph_string.num_glyphs = 1;
  glyph_string.glyphs = &glyph_info;

  pango_cairo_show_glyph_string (cr, key->font, &glyph_string);
  cairo_destroy (cr);

  cairo_surface_flush (surface);
}

static void
gsk_gl_glyph_library_upload_glyph (GskGLGlyphLibrary     *self,
                                   const GskGLGlyphKey   *key,
                                   const GskGLGlyphValue *value,
                                   int                    packed_x,
                                   int                    packed_y,
                                   int                    width,
                                   int                    height,
                                   int                    uwidth,
                                   int                    uheight)
{
  GskGLTextureLibrary *tl = (GskGLTextureLibrary *)self;
  G_GNUC_UNUSED gint64 start_time = GDK_PROFILER_CURRENT_TIME;
  cairo_surface_t *surface;
  guchar *pixel_data;
  guchar *free_data = NULL;
  guint gl_format;
  guint gl_type;
  guint texture_id;
  gsize stride;

  g_assert (GSK_IS_GL_GLYPH_LIBRARY (self));
  g_assert (key != NULL);
  g_assert (value != NULL);

  stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, width);

  gdk_gl_context_push_debug_group_printf (gdk_gl_context_get_current (),
                                          "Uploading glyph %d",
                                          key->glyph);

  surface = gsk_gl_glyph_library_create_surface (self, stride, width, height, uwidth, uheight);
  render_glyph (surface, key, value);

  texture_id = GSK_GL_TEXTURE_ATLAS_ENTRY_TEXTURE (value);

  g_assert (texture_id > 0);

  if (gdk_gl_context_get_use_es (gdk_gl_context_get_current ()))
    {
      pixel_data = free_data = g_malloc (width * height * 4);
      gdk_memory_convert (pixel_data, width * 4,
                          GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
                          cairo_image_surface_get_data (surface),
                          stride,
                          GDK_MEMORY_DEFAULT,
                          width, height);
      stride = width * 4;
      gl_format = GL_RGBA;
      gl_type = GL_UNSIGNED_BYTE;
    }
  else
    {
      pixel_data = cairo_image_surface_get_data (surface);
      gl_format = GL_BGRA;
      gl_type = GL_UNSIGNED_INT_8_8_8_8_REV;
    }

  glPixelStorei (GL_UNPACK_ROW_LENGTH, stride / 4);
  glBindTexture (GL_TEXTURE_2D, texture_id);

  glTexSubImage2D (GL_TEXTURE_2D, 0,
                   packed_x + 1, packed_y + 1,
                   width, height,
                   gl_format, gl_type,
                   pixel_data);

  /* Padding top */
  glTexSubImage2D (GL_TEXTURE_2D, 0,
                   packed_x + 1, packed_y,
                   width, 1,
                   gl_format, gl_type,
                   pixel_data);
  /* Padding left */
  glTexSubImage2D (GL_TEXTURE_2D, 0,
                   packed_x, packed_y + 1,
                   1, height,
                   gl_format, gl_type,
                   pixel_data);
  /* Padding top left */
  glTexSubImage2D (GL_TEXTURE_2D, 0,
                   packed_x, packed_y,
                   1, 1,
                   gl_format, gl_type,
                   pixel_data);
  /* Padding right */
  glPixelStorei (GL_UNPACK_ROW_LENGTH, width);
  glPixelStorei (GL_UNPACK_SKIP_PIXELS, width - 1);
  glTexSubImage2D (GL_TEXTURE_2D, 0,
                   packed_x + width + 1, packed_y + 1,
                   1, height,
                   gl_format, gl_type,
                   pixel_data);
  /* Padding top right */
  glTexSubImage2D (GL_TEXTURE_2D, 0,
                   packed_x + width + 1, packed_y,
                   1, 1,
                   gl_format, gl_type,
                   pixel_data);
  /* Padding bottom */
  glPixelStorei (GL_UNPACK_SKIP_PIXELS, 0);
  glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);
  glPixelStorei (GL_UNPACK_SKIP_ROWS, height - 1);
  glTexSubImage2D (GL_TEXTURE_2D, 0,
                   packed_x + 1, packed_y + 1 + height,
                   width, 1,
                   gl_format, gl_type,
                   pixel_data);
  /* Padding bottom left */
  glTexSubImage2D (GL_TEXTURE_2D, 0,
                   packed_x, packed_y + 1 + height,
                   1, 1,
                   gl_format, gl_type,
                   pixel_data);
  /* Padding bottom right */
  glPixelStorei (GL_UNPACK_ROW_LENGTH, width);
  glPixelStorei (GL_UNPACK_SKIP_PIXELS, width - 1);
  glTexSubImage2D (GL_TEXTURE_2D, 0,
                   packed_x + 1 + width, packed_y + 1 + height,
                   1, 1,
                   gl_format, gl_type,
                   pixel_data);
  /* Reset this */
  glPixelStorei (GL_UNPACK_SKIP_PIXELS, 0);
  glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);
  glPixelStorei (GL_UNPACK_SKIP_ROWS, 0);

  cairo_surface_destroy (surface);
  g_free (free_data);

  gdk_gl_context_pop_debug_group (gdk_gl_context_get_current ());

  tl->driver->command_queue->n_uploads++;

  if (gdk_profiler_is_running ())
    {
      char message[64];
      g_snprintf (message, sizeof message, "Size %dx%d", width, height);
      gdk_profiler_add_mark (start_time, GDK_PROFILER_CURRENT_TIME-start_time, "Upload glyph", message);
    }
}

gboolean
gsk_gl_glyph_library_add (GskGLGlyphLibrary      *self,
                          GskGLGlyphKey          *key,
                          const GskGLGlyphValue **out_value)
{
  GskGLTextureLibrary *tl = (GskGLTextureLibrary *)self;
  PangoRectangle ink_rect;
  GskGLGlyphValue *value;
  int width;
  int height;
  guint packed_x;
  guint packed_y;

  g_assert (GSK_IS_GL_GLYPH_LIBRARY (self));
  g_assert (key != NULL);
  g_assert (out_value != NULL);

  pango_font_get_glyph_extents (key->font, key->glyph, &ink_rect, NULL);
  pango_extents_to_pixels (&ink_rect, NULL);

  ink_rect.x -= 1;
  ink_rect.width += 2;
  ink_rect.y -= 1;
  ink_rect.height += 2;

  width = (int) ceil (ink_rect.width * key->scale / 1024.0);
  height = (int) ceil (ink_rect.height * key->scale / 1024.0);

  GSK_DEBUG (GLYPH_CACHE, "font %p glyph %u: %u x %u pixels", key->font, key->glyph, width, height);

  value = gsk_gl_texture_library_pack (tl,
                                       key,
                                       sizeof *value,
                                       width,
                                       height,
                                       1,
                                       &packed_x, &packed_y);

  memcpy (&value->ink_rect, &ink_rect, sizeof ink_rect);

  if (key->scale > 0 && width > 0 && height > 0)
    gsk_gl_glyph_library_upload_glyph (self,
                                       key,
                                       value,
                                       packed_x,
                                       packed_y,
                                       width,
                                       height,
                                       ink_rect.width,
                                       ink_rect.height);

  *out_value = value;

  return GSK_GL_TEXTURE_ATLAS_ENTRY_TEXTURE (value) != 0;
}
