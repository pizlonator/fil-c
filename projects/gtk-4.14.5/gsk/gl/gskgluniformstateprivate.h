/* gskgluniformstateprivate.h
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

typedef struct { float v0; } Uniform1f;
typedef struct { float v0; float v1; } Uniform2f;
typedef struct { float v0; float v1; float v2; } Uniform3f;
typedef struct { float v0; float v1; float v2; float v3; } Uniform4f;

typedef struct { int v0; } Uniform1i;
typedef struct { int v0; int v1; } Uniform2i;
typedef struct { int v0; int v1; int v2; } Uniform3i;
typedef struct { int v0; int v1; int v2; int v3; } Uniform4i;

typedef struct { guint v0; } Uniform1ui;

#define GSK_GL_UNIFORM_ARRAY_BITS  5
#define GSK_GL_UNIFORM_FORMAT_BITS 5
#define GSK_GL_UNIFORM_OFFSET_BITS 21

typedef struct _GskGLUniformInfo
{
  guint initial     : 1;
  guint format      : GSK_GL_UNIFORM_FORMAT_BITS;
  guint array_count : GSK_GL_UNIFORM_ARRAY_BITS;
  guint offset      : GSK_GL_UNIFORM_OFFSET_BITS;
} GskGLUniformInfo;

G_STATIC_ASSERT (sizeof (GskGLUniformInfo) == 4);

typedef struct _GskGLUniformMapping
{
  const char *name;
  GskGLUniformInfo info;
  guint stamp;
  int location;
} GskGLUniformMapping;

typedef struct _GskGLUniformProgram
{
  guint program_id;
  guint n_uniforms : 12;
  guint has_attachments : 1;
  guint n_mappings;
  GskGLUniformMapping mappings[32];
} GskGLUniformProgram;

typedef struct _GskGLUniformState
{
  GHashTable *programs;
  guint8 *values_buf;
  guint values_pos;
  guint values_len;
  GskGLUniformInfo apply_hash[512];
} GskGLUniformState;

typedef enum _GskGLUniformKind
{
  GSK_GL_UNIFORM_FORMAT_1F = 1,
  GSK_GL_UNIFORM_FORMAT_2F,
  GSK_GL_UNIFORM_FORMAT_3F,
  GSK_GL_UNIFORM_FORMAT_4F,

  GSK_GL_UNIFORM_FORMAT_1FV,
  GSK_GL_UNIFORM_FORMAT_2FV,
  GSK_GL_UNIFORM_FORMAT_3FV,
  GSK_GL_UNIFORM_FORMAT_4FV,

  GSK_GL_UNIFORM_FORMAT_1I,
  GSK_GL_UNIFORM_FORMAT_2I,
  GSK_GL_UNIFORM_FORMAT_3I,
  GSK_GL_UNIFORM_FORMAT_4I,

  GSK_GL_UNIFORM_FORMAT_1UI,

  GSK_GL_UNIFORM_FORMAT_TEXTURE,

  GSK_GL_UNIFORM_FORMAT_MATRIX,
  GSK_GL_UNIFORM_FORMAT_ROUNDED_RECT,
  GSK_GL_UNIFORM_FORMAT_COLOR,

  GSK_GL_UNIFORM_FORMAT_LAST
} GskGLUniformFormat;

G_STATIC_ASSERT (GSK_GL_UNIFORM_FORMAT_LAST < (1 << GSK_GL_UNIFORM_FORMAT_BITS));

GskGLUniformState   *gsk_gl_uniform_state_new         (void);
GskGLUniformState   *gsk_gl_uniform_state_ref         (GskGLUniformState          *state);
void                 gsk_gl_uniform_state_unref       (GskGLUniformState          *state);
GskGLUniformProgram *gsk_gl_uniform_state_get_program (GskGLUniformState          *state,
                                                       guint                       program,
                                                       const GskGLUniformMapping  *mappings,
                                                       guint                       n_mappings);
void                 gsk_gl_uniform_state_end_frame   (GskGLUniformState          *state);
gsize                gsk_gl_uniform_format_size       (GskGLUniformFormat          format);
gpointer             gsk_gl_uniform_state_init_value  (GskGLUniformState          *state,
                                                       GskGLUniformProgram        *program,
                                                       GskGLUniformFormat          format,
                                                       guint                       array_count,
                                                       guint                       key,
                                                       GskGLUniformMapping       **out_mapping);

#define GSK_GL_UNIFORM_VALUE(base, offset) ((gpointer)((base) + ((offset) * 4)))
#define gsk_gl_uniform_state_get_uniform_data(state,offset) GSK_GL_UNIFORM_VALUE((state)->values_buf, offset)

static inline gpointer
gsk_gl_uniform_state_get_value (GskGLUniformState    *state,
                                GskGLUniformProgram  *program,
                                GskGLUniformFormat    format,
                                guint                 array_count,
                                guint                 key,
                                guint                 stamp,
                                GskGLUniformMapping **out_mapping)
{
  GskGLUniformMapping *mapping;

  g_assert (key < G_N_ELEMENTS (program->mappings));
  g_assert (key < program->n_mappings);

  mapping = &program->mappings[key];

  /* Short-circuit if the program optimized the uniform out */
  if (mapping->location == -1)
    return NULL;

  /* If the stamp is the same, then we can ignore the request
   * and short-circuit as early as possible. This requires the
   * caller to increment their private stamp when they change
   * internal state.
   *
   * This is generally used for the shared uniforms like projection,
   * modelview, clip, etc to avoid so many comparisons which cost
   * considerable CPU.
   */
  if (stamp != 0 && stamp == mapping->stamp)
    return NULL;

  if G_LIKELY (format == mapping->info.format && array_count <= mapping->info.array_count)
    {
      *out_mapping = mapping;
      return GSK_GL_UNIFORM_VALUE (state->values_buf, mapping->info.offset);
    }

  return gsk_gl_uniform_state_init_value (state, program, format, array_count, key, out_mapping);
}

G_GNUC_PURE static inline guint
gsk_gl_uniform_state_align (guint current_pos,
                            guint size)
{
  guint align = size > 8 ? 16 : (size > 4 ? 8 : 4);
  guint masked = current_pos & (align - 1);

  g_assert (size > 0);
  g_assert (align == 4 || align == 8 || align == 16);
  g_assert (masked < align);

  return align - masked;
}

static inline gpointer
gsk_gl_uniform_state_realloc (GskGLUniformState *state,
                              guint              size,
                              guint             *offset)
{
  guint padding = gsk_gl_uniform_state_align (state->values_pos, size);

  if G_UNLIKELY (state->values_len - padding - size < state->values_pos)
    {
      state->values_len *= 2;
      state->values_buf = g_realloc (state->values_buf, state->values_len);
    }

  /* offsets are in slots of 4 to use fewer bits */
  g_assert ((state->values_pos + padding) % 4 == 0);
  *offset = (state->values_pos + padding) / 4;
  state->values_pos += padding + size;

  return GSK_GL_UNIFORM_VALUE (state->values_buf, *offset);
}

#define GSK_GL_UNIFORM_STATE_REPLACE(info, u, type, count)                                \
  G_STMT_START {                                                                           \
    if ((info)->info.initial && count == (info)->info.array_count)                         \
      {                                                                                    \
        u = GSK_GL_UNIFORM_VALUE (state->values_buf, (info)->info.offset);                \
      }                                                                                    \
    else                                                                                   \
      {                                                                                    \
        guint offset;                                                                      \
        u = gsk_gl_uniform_state_realloc (state, sizeof(type) * MAX (1, count), &offset); \
        g_assert (offset < (1 << GSK_GL_UNIFORM_OFFSET_BITS));                            \
        (info)->info.offset = offset;                                                      \
        /* We might have increased array length */                                         \
        (info)->info.array_count = count;                                                  \
      }                                                                                    \
  } G_STMT_END

static inline void
gsk_gl_uniform_info_changed (GskGLUniformMapping *info,
                             guint                stamp)
{
  info->stamp = stamp;
  info->info.initial = FALSE;
}

static inline void
gsk_gl_uniform_state_set1f (GskGLUniformState   *state,
                            GskGLUniformProgram *program,
                            guint                key,
                            guint                stamp,
                            float                value0)
{
  Uniform1f *u;
  GskGLUniformMapping *info;

  g_assert (state != NULL);
  g_assert (program != 0);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_1F, 1, key, stamp, &info)))
    {
      if (info->info.initial || u->v0 != value0)
        {
          GSK_GL_UNIFORM_STATE_REPLACE (info, u, Uniform1f , 1);
          u->v0 = value0;
          gsk_gl_uniform_info_changed (info, stamp);
        }
    }
}

static inline void
gsk_gl_uniform_state_set2f (GskGLUniformState   *state,
                            GskGLUniformProgram *program,
                            guint                key,
                            guint                stamp,
                            float                value0,
                            float                value1)
{
  Uniform2f *u;
  GskGLUniformMapping *info;

  g_assert (state != NULL);
  g_assert (program != NULL);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_2F, 1, key, stamp, &info)))
    {
      if (info->info.initial || u->v0 != value0 || u->v1 != value1)
        {
          GSK_GL_UNIFORM_STATE_REPLACE (info, u, Uniform2f, 1);
          u->v0 = value0;
          u->v1 = value1;
          gsk_gl_uniform_info_changed (info, stamp);
        }
    }
}

static inline void
gsk_gl_uniform_state_set3f (GskGLUniformState   *state,
                            GskGLUniformProgram *program,
                            guint                key,
                            guint                stamp,
                            float                value0,
                            float                value1,
                            float                value2)
{
  Uniform3f *u;
  GskGLUniformMapping *info;

  g_assert (state != NULL);
  g_assert (program != NULL);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_3F, 1, key, stamp, &info)))
    {
      if (info->info.initial || u->v0 != value0 || u->v1 != value1 || u->v2 != value2)
        {
          GSK_GL_UNIFORM_STATE_REPLACE (info, u, Uniform3f, 1);
          u->v0 = value0;
          u->v1 = value1;
          u->v2 = value2;
          gsk_gl_uniform_info_changed (info, stamp);
        }
    }
}

static inline void
gsk_gl_uniform_state_set4f (GskGLUniformState   *state,
                            GskGLUniformProgram *program,
                            guint                key,
                            guint                stamp,
                            float                value0,
                            float                value1,
                            float                value2,
                            float                value3)
{
  Uniform4f *u;
  GskGLUniformMapping *info;

  g_assert (state != NULL);
  g_assert (program != NULL);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_4F, 1, key, stamp, &info)))
    {
      if (info->info.initial || u->v0 != value0 || u->v1 != value1 || u->v2 != value2 || u->v3 != value3)
        {
          GSK_GL_UNIFORM_STATE_REPLACE (info, u, Uniform4f, 1);
          u->v0 = value0;
          u->v1 = value1;
          u->v2 = value2;
          u->v3 = value3;
          gsk_gl_uniform_info_changed (info, stamp);
        }
    }
}

static inline void
gsk_gl_uniform_state_set1ui (GskGLUniformState   *state,
                             GskGLUniformProgram *program,
                             guint                key,
                             guint                stamp,
                             guint                value0)
{
  Uniform1ui *u;
  GskGLUniformMapping *info;

  g_assert (state != NULL);
  g_assert (program != NULL);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_1UI, 1, key, stamp, &info)))
    {
      if (info->info.initial || u->v0 != value0)
        {
          GSK_GL_UNIFORM_STATE_REPLACE (info, u, Uniform1ui, 1);
          u->v0 = value0;
          gsk_gl_uniform_info_changed (info, stamp);
        }
    }
}

static inline void
gsk_gl_uniform_state_set1i (GskGLUniformState   *state,
                            GskGLUniformProgram *program,
                            guint                key,
                            guint                stamp,
                            int                  value0)
{
  Uniform1i *u;
  GskGLUniformMapping *info;

  g_assert (state != NULL);
  g_assert (program != NULL);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_1I, 1, key, stamp, &info)))
    {
      if (info->info.initial || u->v0 != value0)
        {
          GSK_GL_UNIFORM_STATE_REPLACE (info, u, Uniform1i, 1);
          u->v0 = value0;
          gsk_gl_uniform_info_changed (info, stamp);
        }
    }
}

static inline void
gsk_gl_uniform_state_set2i (GskGLUniformState   *state,
                            GskGLUniformProgram *program,
                            guint                key,
                            guint                stamp,
                            int                  value0,
                            int                  value1)
{
  Uniform2i *u;
  GskGLUniformMapping *info;

  g_assert (state != NULL);
  g_assert (program != NULL);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_2I, 1, key, stamp, &info)))
    {
      if (info->info.initial || u->v0 != value0 || u->v1 != value1)
        {
          GSK_GL_UNIFORM_STATE_REPLACE (info, u, Uniform2i, 1);
          u->v0 = value0;
          u->v1 = value1;
          gsk_gl_uniform_info_changed (info, stamp);
        }
    }
}

static inline void
gsk_gl_uniform_state_set3i (GskGLUniformState   *state,
                            GskGLUniformProgram *program,
                            guint                key,
                            guint                stamp,
                            int                  value0,
                            int                  value1,
                            int                  value2)
{
  Uniform3i *u;
  GskGLUniformMapping *info;

  g_assert (state != NULL);
  g_assert (program != NULL);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_3I, 1, key, stamp, &info)))
    {
      if (info->info.initial || u->v0 != value0 || u->v1 != value1 || u->v2 != value2)
        {
          GSK_GL_UNIFORM_STATE_REPLACE (info, u, Uniform3i, 1);
          u->v0 = value0;
          u->v1 = value1;
          u->v2 = value2;
          gsk_gl_uniform_info_changed (info, stamp);
        }
    }
}

static inline void
gsk_gl_uniform_state_set4i (GskGLUniformState   *state,
                            GskGLUniformProgram *program,
                            guint                key,
                            guint                stamp,
                            int                  value0,
                            int                  value1,
                            int                  value2,
                            int                  value3)
{
  Uniform4i *u;
  GskGLUniformMapping *info;

  g_assert (state != NULL);
  g_assert (program != NULL);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_4I, 1, key, stamp, &info)))
    {
      if (info->info.initial || u->v0 != value0 || u->v1 != value1 || u->v2 != value2 || u->v3 != value3)
        {
          GSK_GL_UNIFORM_STATE_REPLACE (info, u, Uniform4i, 1);
          u->v0 = value0;
          u->v1 = value1;
          u->v2 = value2;
          u->v3 = value3;
          gsk_gl_uniform_info_changed (info, stamp);
        }
    }
}

static inline void
gsk_gl_uniform_state_set_rounded_rect (GskGLUniformState    *state,
                                       GskGLUniformProgram  *program,
                                       guint                 key,
                                       guint                 stamp,
                                       const GskRoundedRect *rounded_rect)
{
  GskRoundedRect *u;
  GskGLUniformMapping *info;

  g_assert (state != NULL);
  g_assert (program != NULL);
  g_assert (rounded_rect != NULL);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_ROUNDED_RECT, 1, key, stamp, &info)))
    {
      if (info->info.initial || memcmp (u, rounded_rect, sizeof *u) != 0)
        {
          GSK_GL_UNIFORM_STATE_REPLACE (info, u, GskRoundedRect, 1);
          memcpy (u, rounded_rect, sizeof *rounded_rect);
          gsk_gl_uniform_info_changed (info, stamp);
        }
    }
}

static inline void
gsk_gl_uniform_state_set_matrix (GskGLUniformState       *state,
                                 GskGLUniformProgram     *program,
                                 guint                    key,
                                 guint                    stamp,
                                 const graphene_matrix_t *matrix)
{
  graphene_matrix_t *u;
  GskGLUniformMapping *info;

  g_assert (state != NULL);
  g_assert (program != NULL);
  g_assert (matrix != NULL);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_MATRIX, 1, key, stamp, &info)))
    {
      if (info->info.initial || memcmp (u, matrix, sizeof *u) != 0)
        {
          GSK_GL_UNIFORM_STATE_REPLACE (info, u, graphene_matrix_t, 1);
          memcpy (u, matrix, sizeof *matrix);
          gsk_gl_uniform_info_changed (info, stamp);
        }
    }
}

/**
 * gsk_gl_uniform_state_set_texture:
 * @state: a `GskGLUniformState`
 * @program: the program id
 * @location: the location of the texture
 * @texture_slot: a texturing slot such as GL_TEXTURE0
 *
 * Sets the uniform expecting a texture to @texture_slot. This API
 * expects a texture slot such as GL_TEXTURE0 to reduce chances of
 * miss-use by the caller.
 *
 * The value stored to the uniform is in the form of 0 for GL_TEXTURE0,
 * 1 for GL_TEXTURE1, and so on.
 */
static inline void
gsk_gl_uniform_state_set_texture (GskGLUniformState   *state,
                                  GskGLUniformProgram *program,
                                  guint                key,
                                  guint                stamp,
                                  guint                texture_slot)
{
  GskGLUniformMapping *info;
  guint *u;

  g_assert (texture_slot >= GL_TEXTURE0);
  g_assert (texture_slot < GL_TEXTURE16);

  texture_slot -= GL_TEXTURE0;

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_TEXTURE, 1, key, stamp, &info)))
    {
      if (info->info.initial || *u != texture_slot)
        {
          GSK_GL_UNIFORM_STATE_REPLACE (info, u, guint, 1);
          *u = texture_slot;
          gsk_gl_uniform_info_changed (info, stamp);
        }
    }
}

/**
 * gsk_gl_uniform_state_set_color:
 * @state: a `GskGLUniformState`
 * @program: a program id > 0
 * @location: the uniform location
 * @color: a color to set or %NULL for transparent
 *
 * Sets a uniform to the color described by @color. This is a convenience
 * function to allow callers to avoid having to translate colors to floats
 * in other portions of the renderer.
 */
static inline void
gsk_gl_uniform_state_set_color (GskGLUniformState   *state,
                                GskGLUniformProgram *program,
                                guint                key,
                                guint                stamp,
                                const GdkRGBA       *color)
{
  static const GdkRGBA transparent = {0};
  GskGLUniformMapping *info;
  GdkRGBA *u;

  g_assert (state != NULL);
  g_assert (program != NULL);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_COLOR, 1, key, stamp, &info)))
    {
      if (color == NULL)
        color = &transparent;

      if (info->info.initial || memcmp (color, u, sizeof *u) != 0)
        {
          GSK_GL_UNIFORM_STATE_REPLACE (info, u, GdkRGBA, 1);
          memcpy (u, color, sizeof *color);
          gsk_gl_uniform_info_changed (info, stamp);
        }
    }
}

static inline void
gsk_gl_uniform_state_set1fv (GskGLUniformState   *state,
                             GskGLUniformProgram *program,
                             guint                key,
                             guint                stamp,
                             guint                count,
                             const float         *value)
{
  Uniform1f *u;
  GskGLUniformMapping *info;

  g_assert (state != NULL);
  g_assert (program != NULL);
  g_assert (count > 0);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_1FV, count, key, stamp, &info)))
    {
      if (info->info.initial || count != info->info.array_count || memcmp (u, value, sizeof *u * count) != 0)
        {
          GSK_GL_UNIFORM_STATE_REPLACE (info, u, Uniform1f, count);
          memcpy (u, value, sizeof (Uniform1f) * count);
          gsk_gl_uniform_info_changed (info, stamp);
        }
    }
}

static inline void
gsk_gl_uniform_state_set2fv (GskGLUniformState   *state,
                             GskGLUniformProgram *program,
                             guint                key,
                             guint                stamp,
                             guint                count,
                             const float         *value)
{
  Uniform2f *u;
  GskGLUniformMapping *info;

  g_assert (state != NULL);
  g_assert (program != NULL);
  g_assert (count > 0);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_2FV, count, key, stamp, &info)))
    {
      if (info->info.initial || count != info->info.array_count || memcmp (u, value, sizeof *u * count) != 0)
        {
          GSK_GL_UNIFORM_STATE_REPLACE (info, u, Uniform2f, count);
          memcpy (u, value, sizeof (Uniform2f) * count);
          gsk_gl_uniform_info_changed (info, stamp);
        }
    }
}

static inline void
gsk_gl_uniform_state_set3fv (GskGLUniformState   *state,
                             GskGLUniformProgram *program,
                             guint                key,
                             guint                stamp,
                             guint                count,
                             const float         *value)
{
  Uniform3f *u;
  GskGLUniformMapping *info;

  g_assert (state != NULL);
  g_assert (program != NULL);
  g_assert (count > 0);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_3FV, count, key, stamp, &info)))
    {
      if (info->info.initial || count != info->info.array_count || memcmp (u, value, sizeof *u * count) != 0)
        {
          GSK_GL_UNIFORM_STATE_REPLACE (info, u, Uniform3f, count);
          memcpy (u, value, sizeof (Uniform3f) * count);
          gsk_gl_uniform_info_changed (info, stamp);
        }
    }
}

static inline void
gsk_gl_uniform_state_set4fv (GskGLUniformState   *state,
                             GskGLUniformProgram *program,
                             guint                key,
                             guint                stamp,
                             guint                count,
                             const float         *value)
{
  Uniform4f *u;
  GskGLUniformMapping *info;

  g_assert (state != NULL);
  g_assert (program != NULL);
  g_assert (count > 0);

  if ((u = gsk_gl_uniform_state_get_value (state, program, GSK_GL_UNIFORM_FORMAT_4FV, count, key, stamp, &info)))
    {
      if (info->info.initial || count != info->info.array_count || memcmp (u, value, sizeof *u * count) != 0)
        {
          GSK_GL_UNIFORM_STATE_REPLACE (info, u, Uniform4f, count);
          memcpy (u, value, sizeof (Uniform4f) * count);
          gsk_gl_uniform_info_changed (info, stamp);
        }
    }
}

static inline guint
gsk_gl_uniform_state_fmix (guint program,
                           guint location)
{
  guint h = (program << 16) | location;

   h ^= h >> 16;
   h *= 0x85ebca6b;
   h ^= h >> 13;
   h *= 0xc2b2ae35;
   h ^= h >> 16;

   return h;
}

/*
 * gsk_gl_uniform_state_apply:
 * @state: the uniform state
 * @program: the program id
 * @location: the location of the uniform
 * @offset: the offset of the data within the buffer
 * @info: the uniform info
 *
 * This function can be used to apply state that was previously recorded
 * by the `GskGLUniformState`.
 *
 * It is specifically useful from the `GskGLCommandQueue` to execute uniform
 * changes but only when they have changed from the current value.
 */
static inline void
gsk_gl_uniform_state_apply (GskGLUniformState *state,
                            guint              program,
                            guint              location,
                            GskGLUniformInfo   info)
{
  guint index = gsk_gl_uniform_state_fmix (program, location) % G_N_ELEMENTS (state->apply_hash);
  gconstpointer dataptr = GSK_GL_UNIFORM_VALUE (state->values_buf, info.offset);

  /* aligned, can treat as unsigned */
  if (*(guint *)&info == *(guint *)&state->apply_hash[index])
    return;

  state->apply_hash[index] = info;

  /* TODO: We could do additional comparisons here to make sure we are
   *       changing state.
   */

  switch (info.format)
    {
    case GSK_GL_UNIFORM_FORMAT_1F:
      glUniform1fv (location, 1, dataptr);
    break;

    case GSK_GL_UNIFORM_FORMAT_2F:
      glUniform2fv (location, 1, dataptr);
    break;

    case GSK_GL_UNIFORM_FORMAT_3F:
      glUniform3fv (location, 1, dataptr);
    break;

    case GSK_GL_UNIFORM_FORMAT_4F:
      glUniform4fv (location, 1, dataptr);
    break;

    case GSK_GL_UNIFORM_FORMAT_1FV:
      glUniform1fv (location, info.array_count, dataptr);
    break;

    case GSK_GL_UNIFORM_FORMAT_2FV:
      glUniform2fv (location, info.array_count, dataptr);
    break;

    case GSK_GL_UNIFORM_FORMAT_3FV:
      glUniform3fv (location, info.array_count, dataptr);
    break;

    case GSK_GL_UNIFORM_FORMAT_4FV:
      glUniform4fv (location, info.array_count, dataptr);
    break;

    case GSK_GL_UNIFORM_FORMAT_1I:
    case GSK_GL_UNIFORM_FORMAT_TEXTURE:
      glUniform1iv (location, 1, dataptr);
    break;

    case GSK_GL_UNIFORM_FORMAT_2I:
      glUniform2iv (location, 1, dataptr);
    break;

    case GSK_GL_UNIFORM_FORMAT_3I:
      glUniform3iv (location, 1, dataptr);
    break;

    case GSK_GL_UNIFORM_FORMAT_4I:
      glUniform4iv (location, 1, dataptr);
    break;

    case GSK_GL_UNIFORM_FORMAT_1UI:
      glUniform1uiv (location, 1, dataptr);
    break;

    case GSK_GL_UNIFORM_FORMAT_MATRIX: {
      float mat[16];
      graphene_matrix_to_float (dataptr, mat);
      glUniformMatrix4fv (location, 1, GL_FALSE, mat);
#if 0
      /* TODO: If Graphene can give us a peek here on platforms
       * where the format is float[16] (most/all x86_64?) then
       * We can avoid the SIMD operation to convert the format.
       */
      G_STATIC_ASSERT (sizeof (graphene_matrix_t) == 16*4);
      glUniformMatrix4fv (location, 1, GL_FALSE, dataptr);
#endif
    }
    break;

    case GSK_GL_UNIFORM_FORMAT_COLOR:
      glUniform4fv (location, 1, dataptr);
    break;

    case GSK_GL_UNIFORM_FORMAT_ROUNDED_RECT:
      glUniform4fv (location, 3, dataptr);
    break;

    default:
      g_assert_not_reached ();
    }
}

G_END_DECLS

