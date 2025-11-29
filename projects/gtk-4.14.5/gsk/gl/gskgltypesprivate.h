/* gskgltypesprivate.h
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

#include <epoxy/gl.h>
#include <graphene.h>
#include <gdk/gdk.h>
#include <gsk/gsk.h>

G_BEGIN_DECLS

#define GSK_GL_N_VERTICES 6

typedef struct _GskGLAttachmentState GskGLAttachmentState;
typedef struct _GskGLBuffer GskGLBuffer;
typedef struct _GskGLCommandQueue GskGLCommandQueue;
typedef struct _GskGLCompiler GskGLCompiler;
typedef struct _GskGLDrawVertex GskGLDrawVertex;
typedef struct _GskGLRenderTarget GskGLRenderTarget;
typedef struct _GskGLGlyphLibrary GskGLGlyphLibrary;
typedef struct _GskGLIconLibrary GskGLIconLibrary;
typedef struct _GskGLProgram GskGLProgram;
typedef struct _GskGLRenderJob GskGLRenderJob;
typedef struct _GskGLShadowLibrary GskGLShadowLibrary;
typedef struct _GskGLTexture GskGLTexture;
typedef struct _GskGLTextureSlice GskGLTextureSlice;
typedef struct _GskGLTextureAtlas GskGLTextureAtlas;
typedef struct _GskGLTextureLibrary GskGLTextureLibrary;
typedef struct _GskGLTextureNineSlice GskGLTextureNineSlice;
typedef struct _GskGLUniformInfo GskGLUniformInfo;
typedef struct _GskGLUniformProgram GskGLUniformProgram;
typedef struct _GskGLUniformState GskGLUniformState;
typedef struct _GskGLDriver GskGLDriver;

struct _GskGLDrawVertex
{
  float position[2];
  union {
    float uv[2];
    guint16 color2[4];
  };
  guint16 color[4];
};

G_END_DECLS

