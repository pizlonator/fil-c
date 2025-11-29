/* gskglattachmentstate.c
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

#include "config.h"

#include "gskglattachmentstateprivate.h"

GskGLAttachmentState *
gsk_gl_attachment_state_new (void)
{
  GskGLAttachmentState *self;

  self = g_atomic_rc_box_new0 (GskGLAttachmentState);

  self->fbo.changed = FALSE;
  self->fbo.id = 0;
  self->n_changed = 0;

  /* Initialize textures, assume we are 2D by default since it
   * doesn't really matter until we bind something other than
   * GL_TEXTURE0 to it anyway.
   */
  for (guint i = 0; i < G_N_ELEMENTS (self->textures); i++)
    {
      self->textures[i].target = GL_TEXTURE_2D;
      self->textures[i].texture = GL_TEXTURE0;
      self->textures[i].id = 0;
      self->textures[i].changed = FALSE;
      self->textures[i].initial = TRUE;
      self->textures[i].sampler = sampler_index (GL_LINEAR, GL_LINEAR);
    }

  return self;
}

GskGLAttachmentState *
gsk_gl_attachment_state_ref (GskGLAttachmentState *self)
{
  return g_atomic_rc_box_acquire (self);
}

void
gsk_gl_attachment_state_unref (GskGLAttachmentState *self)
{
  g_atomic_rc_box_release (self);
}

void
gsk_gl_attachment_state_bind_texture (GskGLAttachmentState *self,
                                      GLenum                target,
                                      GLenum                texture,
                                      guint                 id,
                                      GLint                 min_filter,
                                      GLint                 mag_filter)
{
  GskGLBindTexture *attach;
  unsigned int sampler;

  g_assert (self != NULL);
  g_assert (target == GL_TEXTURE_1D ||
            target == GL_TEXTURE_2D ||
            target == GL_TEXTURE_3D ||
            target == GL_TEXTURE_EXTERNAL_OES);
  g_assert (texture >= GL_TEXTURE0 && texture <= GL_TEXTURE16);
  g_assert (texture - GL_TEXTURE0 < G_N_ELEMENTS (self->textures));

  attach = &self->textures[texture - GL_TEXTURE0];

  sampler = sampler_index (min_filter, mag_filter);

  if (attach->target != target || attach->texture != texture || attach->id != id ||
      attach->sampler != sampler)
    {
      attach->target = target;
      attach->texture = texture;
      attach->id = id;
      attach->initial = FALSE;
      attach->sampler = sampler;

      if (attach->changed == FALSE)
        {
          attach->changed = TRUE;
          self->n_changed++;
        }
    }
}

void
gsk_gl_attachment_state_bind_framebuffer (GskGLAttachmentState *self,
                                          guint                 id)
{
  g_assert (self != NULL);

  if (self->fbo.id != id)
    {
      self->fbo.id = id;
      self->fbo.changed = TRUE;
    }
}
