/* gskglglyphlibraryprivate.h
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

#pragma once

#include <pango/pango.h>

#include "gskgltexturelibraryprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_GL_GLYPH_LIBRARY (gsk_gl_glyph_library_get_type())

typedef struct _GskGLGlyphKey
{
  PangoFont *font;
  PangoGlyph glyph;
  guint xshift : 2;
  guint yshift : 2;
  guint scale  : 28; /* times 1024 */
} GskGLGlyphKey;

typedef struct _GskGLGlyphValue
{
  GskGLTextureAtlasEntry entry;
  PangoRectangle ink_rect;
} GskGLGlyphValue;

#if GLIB_SIZEOF_VOID_P == 8
G_STATIC_ASSERT (sizeof (GskGLGlyphKey) == 16);
#elif GLIB_SIZEOF_VOID_P == 4
G_STATIC_ASSERT (sizeof (GskGLGlyphKey) == 12);
#endif

G_DECLARE_FINAL_TYPE (GskGLGlyphLibrary, gsk_gl_glyph_library, GSK, GL_GLYPH_LIBRARY, GskGLTextureLibrary)

struct _GskGLGlyphLibrary
{
  GskGLTextureLibrary parent_instance;
  guint8 *surface_data;
  gsize surface_data_len;
  struct {
    GskGLGlyphKey key;
    const GskGLGlyphValue *value;
  } front[256];
};

GskGLGlyphLibrary *gsk_gl_glyph_library_new (GskGLDriver            *driver);
gboolean           gsk_gl_glyph_library_add (GskGLGlyphLibrary      *self,
                                             GskGLGlyphKey          *key,
                                             const GskGLGlyphValue **out_value);

static inline guint
gsk_gl_glyph_library_lookup_or_add (GskGLGlyphLibrary      *self,
                                    const GskGLGlyphKey    *key,
                                    const GskGLGlyphValue **out_value)
{
  GskGLTextureAtlasEntry *entry;
  guint front_index = ((key->glyph << 2) | key->xshift) & 0xFF;

  if (memcmp (key, &self->front[front_index], sizeof *key) == 0)
    {
      *out_value = self->front[front_index].value;
    }
  else if (gsk_gl_texture_library_lookup ((GskGLTextureLibrary *)self, key, &entry))
    {
      *out_value = (GskGLGlyphValue *)entry;
      self->front[front_index].key = *key;
      self->front[front_index].value = *out_value;
    }
  else
    {
      GskGLGlyphKey *k;
      k = g_new (GskGLGlyphKey, 1);
      memcpy (k, key, sizeof (GskGLGlyphKey));
      g_object_ref (k->font);
      gsk_gl_glyph_library_add (self, k, out_value);
      self->front[front_index].key = *key;
      self->front[front_index].value = *out_value;
    }

  return GSK_GL_TEXTURE_ATLAS_ENTRY_TEXTURE (*out_value);
}

G_END_DECLS

