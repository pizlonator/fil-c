/* gskglprogramprivate.h
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

#include "gskglattachmentstateprivate.h"
#include "gskglcommandqueueprivate.h"
#include "gskgldriverprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_GL_PROGRAM (gsk_gl_program_get_type())
#define GSK_GL_PROGRAM_MAX_CUSTOM_TEXTURES 4
#define GSK_GL_PROGRAM_MAX_CUSTOM_ARGS 8

G_DECLARE_FINAL_TYPE (GskGLProgram, gsk_gl_program, GSK, GL_PROGRAM, GObject)

struct _GskGLProgram
{
  GObject parent_instance;

  int id;
  char *name;
  GskGLDriver *driver;

  /* Cached pointer to avoid lots of pointer chasing/lookups */
  GskGLUniformState *uniforms;
  GskGLUniformProgram *program_info;

  /* Static array for key->location transforms */
  GskGLUniformMapping mappings[32];
  guint n_mappings;
};

GskGLProgram * gsk_gl_program_new            (GskGLDriver  *driver,
                                              const char   *name,
                                              int           program_id);
gboolean       gsk_gl_program_add_uniform    (GskGLProgram *self,
                                              const char   *name,
                                              guint         key);
void           gsk_gl_program_uniforms_added (GskGLProgram *self,
                                              gboolean      has_attachments);
void           gsk_gl_program_delete         (GskGLProgram *self);

static inline void
gsk_gl_program_set_uniform1fv (GskGLProgram *self,
                               guint         key,
                               guint         stamp,
                               guint         count,
                               const float  *values)
{
  gsk_gl_uniform_state_set1fv (self->uniforms, self->program_info,
                               key,
                               stamp,
                               count,
                               values);
}

static inline void
gsk_gl_program_set_uniform2fv (GskGLProgram *self,
                               guint         key,
                               guint         stamp,
                               guint         count,
                               const float  *values)
{
  gsk_gl_uniform_state_set2fv (self->uniforms, self->program_info,
                               key,
                               stamp,
                               count,
                               values);
}

static inline void
gsk_gl_program_set_uniform4fv (GskGLProgram *self,
                               guint         key,
                               guint         stamp,
                               guint         count,
                               const float  *values)
{
  gsk_gl_uniform_state_set4fv (self->uniforms, self->program_info,
                               key,
                               stamp,
                               count,
                               values);
}

static inline void
gsk_gl_program_set_uniform_rounded_rect (GskGLProgram         *self,
                                         guint                 key,
                                         guint                 stamp,
                                         const GskRoundedRect *rounded_rect)
{
  gsk_gl_uniform_state_set_rounded_rect (self->uniforms, self->program_info,
                                         key,
                                         stamp,
                                         rounded_rect);
}

static inline void
gsk_gl_program_set_uniform1i (GskGLProgram *self,
                              guint         key,
                              guint         stamp,
                              int           value0)
{
  gsk_gl_uniform_state_set1i (self->uniforms,
                              self->program_info,
                              key,
                              stamp,
                              value0);
}

static inline void
gsk_gl_program_set_uniform2i (GskGLProgram *self,
                              guint         key,
                              guint         stamp,
                              int           value0,
                              int           value1)
{
  gsk_gl_uniform_state_set2i (self->uniforms,
                              self->program_info,
                              key,
                              stamp,
                              value0, value1);
}

static inline void
gsk_gl_program_set_uniform3i (GskGLProgram *self,
                              guint         key,
                              guint         stamp,
                              int           value0,
                              int           value1,
                              int           value2)
{
  gsk_gl_uniform_state_set3i (self->uniforms,
                              self->program_info,
                              key,
                              stamp,
                              value0, value1, value2);
}

static inline void
gsk_gl_program_set_uniform4i (GskGLProgram *self,
                              guint         key,
                              guint         stamp,
                              int           value0,
                              int           value1,
                              int           value2,
                              int           value3)
{
  gsk_gl_uniform_state_set4i (self->uniforms,
                              self->program_info,
                              key,
                              stamp,
                              value0, value1, value2, value3);
}

static inline void
gsk_gl_program_set_uniform1f (GskGLProgram *self,
                              guint         key,
                              guint         stamp,
                              float         value0)
{
  gsk_gl_uniform_state_set1f (self->uniforms,
                              self->program_info,
                              key,
                              stamp,
                              value0);
}

static inline void
gsk_gl_program_set_uniform2f (GskGLProgram *self,
                              guint         key,
                              guint         stamp,
                              float         value0,
                              float         value1)
{
  gsk_gl_uniform_state_set2f (self->uniforms,
                              self->program_info,
                              key,
                              stamp,
                              value0, value1);
}

static inline void
gsk_gl_program_set_uniform3f (GskGLProgram *self,
                              guint         key,
                              guint         stamp,
                              float         value0,
                              float         value1,
                              float         value2)
{
  gsk_gl_uniform_state_set3f (self->uniforms,
                              self->program_info,
                              key,
                              stamp,
                              value0, value1, value2);
}

static inline void
gsk_gl_program_set_uniform4f (GskGLProgram *self,
                              guint         key,
                              guint         stamp,
                              float         value0,
                              float         value1,
                              float         value2,
                              float         value3)
{
  gsk_gl_uniform_state_set4f (self->uniforms,
                              self->program_info,
                              key,
                              stamp,
                              value0, value1, value2, value3);
}

static inline void
gsk_gl_program_set_uniform_color (GskGLProgram  *self,
                                  guint          key,
                                  guint          stamp,
                                  const GdkRGBA *color)
{
  gsk_gl_uniform_state_set_color (self->uniforms,
                                  self->program_info,
                                  key,
                                  stamp,
                                  color);
}

static inline void
gsk_gl_program_set_uniform_texture_with_filter (GskGLProgram *self,
                                                guint         key,
                                                guint         stamp,
                                                GLenum        texture_target,
                                                GLenum        texture_slot,
                                                guint         texture_id,
                                                GLint         min_filter,
                                                GLint         mag_filter)
{
  gsk_gl_attachment_state_bind_texture (self->driver->command_queue->attachments,
                                        texture_target,
                                        texture_slot,
                                        texture_id,
                                        min_filter,
                                        mag_filter);
  gsk_gl_uniform_state_set_texture (self->uniforms,
                                    self->program_info,
                                    key,
                                    stamp,
                                    texture_slot);
}

static inline void
gsk_gl_program_set_uniform_texture (GskGLProgram *self,
                                    guint         key,
                                    guint         stamp,
                                    GLenum        texture_target,
                                    GLenum        texture_slot,
                                    guint         texture_id)
{
  gsk_gl_program_set_uniform_texture_with_filter (self,
                                                  key,
                                                  stamp,
                                                  texture_target,
                                                  texture_slot,
                                                  texture_id,
                                                  GL_LINEAR,
                                                  GL_LINEAR);
}

static inline void
gsk_gl_program_set_uniform_texture_with_sync (GskGLProgram *self,
                                              guint         key,
                                              guint         stamp,
                                              GLenum        texture_target,
                                              GLenum        texture_slot,
                                              guint         texture_id,
                                              GLint         min_filter,
                                              GLint         max_filter,
                                              gpointer      sync)
{
  gsk_gl_program_set_uniform_texture_with_filter (self, key, stamp, texture_target, texture_slot, texture_id,
                                                  min_filter, max_filter);
  gsk_gl_syncs_add_sync (&self->driver->command_queue->syncs, texture_id, sync);
}

static inline void
gsk_gl_program_set_uniform_matrix (GskGLProgram            *self,
                                   guint                    key,
                                   guint                    stamp,
                                   const graphene_matrix_t *matrix)
{
  gsk_gl_uniform_state_set_matrix (self->uniforms,
                                   self->program_info,
                                   key,
                                   stamp,
                                   matrix);
}

G_END_DECLS

