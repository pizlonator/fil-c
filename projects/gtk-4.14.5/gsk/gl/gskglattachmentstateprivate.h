/* gskglattachmentstateprivate.h
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

G_BEGIN_DECLS

typedef struct _GskGLAttachmentState GskGLAttachmentState;
typedef struct _GskGLBindFramebuffer GskGLBindFramebuffer;
typedef struct _GskGLBindTexture     GskGLBindTexture;

#define GSK_GL_N_FILTERS 3
#define SAMPLER_EXTERNAL 9

G_STATIC_ASSERT (SAMPLER_EXTERNAL >= GSK_GL_N_FILTERS * GSK_GL_N_FILTERS);

static inline guint
filter_index (GLint filter)
{
  switch (filter)
  {
    case GL_LINEAR:
      return 0;
    case GL_NEAREST:
      return 1;
    case GL_LINEAR_MIPMAP_LINEAR:
      return 2;
    default:
      g_assert_not_reached ();
  }
}

static inline guint
sampler_index (GLint min_filter,
               GLint mag_filter)
{
  return filter_index (min_filter) * GSK_GL_N_FILTERS + filter_index (mag_filter);
}

struct _GskGLBindTexture
{
  guint changed : 1;
  guint initial : 1;
  GLenum target : 26;
  guint sampler : 4;
  GLenum texture;
  guint id;
};

G_STATIC_ASSERT (sizeof (GskGLBindTexture) == 12);

struct _GskGLBindFramebuffer
{
  guint changed : 1;
  guint id : 31;
};

G_STATIC_ASSERT (sizeof (GskGLBindFramebuffer) == 4);

/* Increase if shaders add more textures */
#define GSK_GL_MAX_TEXTURES_PER_PROGRAM 4

struct _GskGLAttachmentState
{
  GskGLBindFramebuffer fbo;
  GskGLBindTexture textures[GSK_GL_MAX_TEXTURES_PER_PROGRAM];
  guint n_changed;
};

GskGLAttachmentState *gsk_gl_attachment_state_new              (void);
GskGLAttachmentState *gsk_gl_attachment_state_ref              (GskGLAttachmentState *self);
void                  gsk_gl_attachment_state_unref            (GskGLAttachmentState *self);
void                  gsk_gl_attachment_state_bind_texture     (GskGLAttachmentState *self,
                                                                GLenum                 target,
                                                                GLenum                 texture,
                                                                guint                  id,
                                                                GLint                  min_filter,
                                                                GLint                  mag_filter);
void                  gsk_gl_attachment_state_bind_framebuffer (GskGLAttachmentState *self,
                                                                guint                  id);

G_END_DECLS

