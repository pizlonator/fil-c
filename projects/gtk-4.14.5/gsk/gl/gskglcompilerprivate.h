/* gskglcompilerprivate.h
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

#include "gskgltypesprivate.h"

G_BEGIN_DECLS

typedef enum _GskGLCompilerKind
{
  GSK_GL_COMPILER_ALL,
  GSK_GL_COMPILER_FRAGMENT,
  GSK_GL_COMPILER_VERTEX,
} GskGLCompilerKind;

#define GSK_TYPE_GL_COMPILER (gsk_gl_compiler_get_type())

G_DECLARE_FINAL_TYPE (GskGLCompiler, gsk_gl_compiler, GSK, GL_COMPILER, GObject)

GskGLCompiler * gsk_gl_compiler_new                        (GskGLDriver        *driver,
                                                            gboolean            debug);
void            gsk_gl_compiler_set_preamble               (GskGLCompiler      *self,
                                                            GskGLCompilerKind   kind,
                                                            GBytes             *preamble_bytes);
void            gsk_gl_compiler_set_preamble_from_resource (GskGLCompiler      *self,
                                                            GskGLCompilerKind   kind,
                                                            const char         *resource_path);
void            gsk_gl_compiler_set_source                 (GskGLCompiler      *self,
                                                            GskGLCompilerKind   kind,
                                                            GBytes             *source_bytes);
void            gsk_gl_compiler_set_source_from_resource   (GskGLCompiler      *self,
                                                            GskGLCompilerKind   kind,
                                                            const char         *resource_path);
void            gsk_gl_compiler_set_suffix                 (GskGLCompiler      *self,
                                                            GskGLCompilerKind   kind,
                                                            GBytes             *suffix_bytes);
void            gsk_gl_compiler_set_suffix_from_resource   (GskGLCompiler      *self,
                                                            GskGLCompilerKind   kind,
                                                            const char         *resource_path);
void            gsk_gl_compiler_bind_attribute             (GskGLCompiler      *self,
                                                            const char         *name,
                                                            guint               location);
void            gsk_gl_compiler_clear_attributes           (GskGLCompiler      *self);
GskGLProgram  * gsk_gl_compiler_compile                    (GskGLCompiler      *self,
                                                            const char         *name,
                                                            const char         *clip,
                                                            GError            **error);

G_END_DECLS

