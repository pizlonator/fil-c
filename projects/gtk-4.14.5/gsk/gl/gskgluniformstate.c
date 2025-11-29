/* gskgluniformstate.c
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

#include <gsk/gskroundedrectprivate.h>
#include <string.h>

#include "gskgluniformstateprivate.h"

static const guint8 uniform_sizes[] = {
  0,

  sizeof (Uniform1f),
  sizeof (Uniform2f),
  sizeof (Uniform3f),
  sizeof (Uniform4f),

  sizeof (Uniform1f),
  sizeof (Uniform2f),
  sizeof (Uniform3f),
  sizeof (Uniform4f),

  sizeof (Uniform1i),
  sizeof (Uniform2i),
  sizeof (Uniform3i),
  sizeof (Uniform4i),

  sizeof (Uniform1ui),

  sizeof (guint),

  sizeof (graphene_matrix_t),
  sizeof (GskRoundedRect),
  sizeof (GdkRGBA),

  0,
};

GskGLUniformState *
gsk_gl_uniform_state_new (void)
{
  GskGLUniformState *state;

  state = g_atomic_rc_box_new0 (GskGLUniformState);
  state->programs = g_hash_table_new_full (NULL, NULL, NULL, g_free);
  state->values_len = 4096;
  state->values_pos = 0;
  state->values_buf = g_malloc (4096);

  memset (state->apply_hash, 0, sizeof state->apply_hash);

  return g_steal_pointer (&state);
}

GskGLUniformState *
gsk_gl_uniform_state_ref (GskGLUniformState *state)
{
  return g_atomic_rc_box_acquire (state);
}

static void
gsk_gl_uniform_state_finalize (gpointer data)
{
  GskGLUniformState *state = data;

  g_clear_pointer (&state->programs, g_hash_table_unref);
  g_clear_pointer (&state->values_buf, g_free);
}

void
gsk_gl_uniform_state_unref (GskGLUniformState *state)
{
  g_atomic_rc_box_release_full (state, gsk_gl_uniform_state_finalize);
}

gpointer
gsk_gl_uniform_state_init_value (GskGLUniformState    *state,
                                 GskGLUniformProgram  *program,
                                 GskGLUniformFormat    format,
                                 guint                 array_count,
                                 guint                 key,
                                 GskGLUniformMapping **infoptr)
{
  GskGLUniformMapping *mapping;
  guint offset;

  g_assert (state != NULL);
  g_assert (array_count < 32);
  g_assert ((int)format >= 0 && format < GSK_GL_UNIFORM_FORMAT_LAST);
  g_assert (format > 0);
  g_assert (program != NULL);
  g_assert (key < program->n_mappings);

  mapping = &program->mappings[key];

  if (mapping->location == -1)
    {
      *infoptr = NULL;
      return NULL;
    }

  if G_LIKELY (format == mapping->info.format)
    {
      if G_LIKELY (array_count <= mapping->info.array_count)
        {
          *infoptr = mapping;
          return GSK_GL_UNIFORM_VALUE (state->values_buf, mapping->info.offset);
        }

      /* We found the uniform, but there is not enough space for the
       * amount that was requested. Instead, allocate new space and
       * set the value to "initial" so that the caller just writes
       * over the previous value.
       *
       * This can happen when using dynamic array lengths like the
       * "n_color_stops" in gradient shaders.
       */
      goto setup_info;
    }
  else if (mapping->info.format == 0)
    {
      goto setup_info;
    }
  else
    {
      g_critical ("Attempt to access uniform with different type of value "
                  "than it was initialized with. Program %u Location %u. "
                  "Was %d now %d (array length %d now %d).",
                  program->program_id, key, mapping->info.format, format,
                  mapping->info.array_count, array_count);
      *infoptr = NULL;
      return NULL;
    }

setup_info:

  gsk_gl_uniform_state_realloc (state,
                                uniform_sizes[format] * MAX (1, array_count),
                                &offset);

  /* we have 21 bits for offset */
  g_assert (offset < (1 << GSK_GL_UNIFORM_OFFSET_BITS));

  mapping->info.format = format;
  mapping->info.offset = offset;
  mapping->info.array_count = array_count;
  mapping->info.initial = TRUE;
  mapping->stamp = 0;

  *infoptr = mapping;

  return GSK_GL_UNIFORM_VALUE (state->values_buf, mapping->info.offset);
}

void
gsk_gl_uniform_state_end_frame (GskGLUniformState *state)
{
  GHashTableIter iter;
  GskGLUniformProgram *program;
  guint allocator = 0;

  g_return_if_fail (state != NULL);

  /* After a frame finishes, we want to remove all our copies of uniform
   * data that isn't needed any longer. Since we treat it as uninitialized
   * after this frame (to reset it on first use next frame) we can just
   * discard it but keep an allocation around to reuse.
   */

  g_hash_table_iter_init (&iter, state->programs);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&program))
    {
      for (guint j = 0; j < program->n_mappings; j++)
        {
          GskGLUniformMapping *mapping = &program->mappings[j];
          guint size;

          /* Skip unused uniform mappings */
          if (mapping->info.format == 0 || mapping->location == -1)
            continue;

          /* Calculate how much size is needed for the uniform, including arrays */
          size = uniform_sizes[mapping->info.format] * MAX (1, mapping->info.array_count);

          /* Adjust alignment for value */
          allocator += gsk_gl_uniform_state_align (allocator, size);

          /* Offset is in slots of 4 bytes */
          mapping->info.offset = allocator / 4;
          mapping->info.initial = TRUE;
          mapping->stamp = 0;

          /* Now advance for this items data */
          allocator += size;
        }
    }

  state->values_pos = allocator;

  /* It can happen that our space requirements grow due to
   * difference in order increasing padding. As a pragmatic
   * solution to this, just increase the allocation to cover
   * the predefined mappins.
   */
  if (allocator > state->values_len)
    {
      while (allocator > state->values_len)
        state->values_len *= 2;
      state->values_buf = g_realloc (state->values_buf, state->values_len);
    }

  memset (state->apply_hash, 0, sizeof state->apply_hash);
}

gsize
gsk_gl_uniform_format_size (GskGLUniformFormat format)
{
  g_assert (format > 0);
  g_assert (format < GSK_GL_UNIFORM_FORMAT_LAST);

  return uniform_sizes[format];
}

GskGLUniformProgram *
gsk_gl_uniform_state_get_program (GskGLUniformState         *state,
                                  guint                      program,
                                  const GskGLUniformMapping *mappings,
                                  guint                      n_mappings)
{
  GskGLUniformProgram *ret;

  g_return_val_if_fail (state != NULL, NULL);
  g_return_val_if_fail (program > 0, NULL);
  g_return_val_if_fail (program < G_MAXUINT, NULL);
  g_return_val_if_fail (n_mappings <= G_N_ELEMENTS (ret->mappings), NULL);

  ret = g_hash_table_lookup (state->programs, GUINT_TO_POINTER (program));

  if (ret == NULL)
    {
      ret = g_new0 (GskGLUniformProgram, 1);
      ret->program_id = program;
      ret->n_mappings = n_mappings;

      memcpy (ret->mappings, mappings, n_mappings * sizeof *mappings);

      g_hash_table_insert (state->programs, GUINT_TO_POINTER (program), ret);
    }

  return ret;
}
