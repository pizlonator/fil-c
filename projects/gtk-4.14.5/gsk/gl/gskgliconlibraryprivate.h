/* gskgliconlibraryprivate.h
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

#define GSK_TYPE_GL_ICON_LIBRARY (gsk_gl_icon_library_get_type())

typedef struct _GskGLIconData
{
  GskGLTextureAtlasEntry entry;
  GdkTexture *source_texture;
} GskGLIconData;

G_DECLARE_FINAL_TYPE (GskGLIconLibrary, gsk_gl_icon_library, GSK, GL_ICON_LIBRARY, GskGLTextureLibrary)

GskGLIconLibrary *gsk_gl_icon_library_new (GskGLDriver          *driver);
void              gsk_gl_icon_library_add (GskGLIconLibrary     *self,
                                           GdkTexture           *key,
                                           const GskGLIconData **out_value);

static inline void
gsk_gl_icon_library_lookup_or_add (GskGLIconLibrary     *self,
                                   GdkTexture           *key,
                                   const GskGLIconData **out_value)
{
  GskGLTextureAtlasEntry *entry;

  if G_LIKELY (gsk_gl_texture_library_lookup ((GskGLTextureLibrary *)self, key, &entry))
    *out_value = (GskGLIconData *)entry;
  else
    gsk_gl_icon_library_add (self, key, out_value);
}

G_END_DECLS

