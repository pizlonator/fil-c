/* gskglshadowlibraryprivate.h
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

#include "gskgltexturelibraryprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_GL_SHADOW_LIBRARY (gsk_gl_shadow_library_get_type())

G_DECLARE_FINAL_TYPE (GskGLShadowLibrary, gsk_gl_shadow_library, GSK, GL_SHADOW_LIBRARY, GObject)

GskGLShadowLibrary * gsk_gl_shadow_library_new         (GskGLDriver          *driver);
void                 gsk_gl_shadow_library_begin_frame (GskGLShadowLibrary   *self);
guint                gsk_gl_shadow_library_lookup      (GskGLShadowLibrary   *self,
                                                        const GskRoundedRect *outline,
                                                        float                 blur_radius);
void                 gsk_gl_shadow_library_insert      (GskGLShadowLibrary   *self,
                                                        const GskRoundedRect *outline,
                                                        float                 blur_radius,
                                                        guint                 texture_id);

G_END_DECLS

