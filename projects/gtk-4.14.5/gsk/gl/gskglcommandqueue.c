/* gskglcommandqueue.c
 *
 * Copyright 2017 Timm Bäder <mail@baedert.org>
 * Copyright 2018 Matthias Clasen <mclasen@redhat.com>
 * Copyright 2018 Alexander Larsson <alexl@redhat.com>
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

#include <string.h>

#include <gdk/gdkglcontextprivate.h>
#include <gdk/gdkmemoryformatprivate.h>
#include <gdk/gdkprofilerprivate.h>
#include <gdk/gdktexturedownloaderprivate.h>
#include <gsk/gskdebugprivate.h>
#include <gsk/gskroundedrectprivate.h>

#include "gskglattachmentstateprivate.h"
#include "gskglbufferprivate.h"
#include "gskglcommandqueueprivate.h"
#include "gskgluniformstateprivate.h"

#include "inlinearray.h"

G_DEFINE_TYPE (GskGLCommandQueue, gsk_gl_command_queue, G_TYPE_OBJECT)

#if 0
G_GNUC_UNUSED static inline void
print_uniform (GskGLUniformFormat format,
               guint              array_count,
               gconstpointer      valueptr)
{
  const union {
    graphene_matrix_t matrix[0];
    GskRoundedRect rounded_rect[0];
    float fval[0];
    int ival[0];
    guint uval[0];
  } *data = valueptr;

  switch (format)
    {
    case GSK_GL_UNIFORM_FORMAT_1F:
      g_printerr ("1f<%f>", data->fval[0]);
      break;

    case GSK_GL_UNIFORM_FORMAT_2F:
      g_printerr ("2f<%f,%f>", data->fval[0], data->fval[1]);
      break;

    case GSK_GL_UNIFORM_FORMAT_3F:
      g_printerr ("3f<%f,%f,%f>", data->fval[0], data->fval[1], data->fval[2]);
      break;

    case GSK_GL_UNIFORM_FORMAT_4F:
      g_printerr ("4f<%f,%f,%f,%f>", data->fval[0], data->fval[1], data->fval[2], data->fval[3]);
      break;

    case GSK_GL_UNIFORM_FORMAT_1I:
    case GSK_GL_UNIFORM_FORMAT_TEXTURE:
      g_printerr ("1i<%d>", data->ival[0]);
      break;

    case GSK_GL_UNIFORM_FORMAT_1UI:
      g_printerr ("1ui<%u>", data->uval[0]);
      break;

    case GSK_GL_UNIFORM_FORMAT_COLOR: {
      char *str = gdk_rgba_to_string (valueptr);
      g_printerr ("%s", str);
      g_free (str);
      break;
    }

    case GSK_GL_UNIFORM_FORMAT_ROUNDED_RECT: {
      char *str = gsk_rounded_rect_to_string (valueptr);
      g_printerr ("%s", str);
      g_free (str);
      break;
    }

    case GSK_GL_UNIFORM_FORMAT_MATRIX: {
      float mat[16];
      graphene_matrix_to_float (&data->matrix[0], mat);
      g_printerr ("matrix<");
      for (guint i = 0; i < G_N_ELEMENTS (mat)-1; i++)
        g_printerr ("%f,", mat[i]);
      g_printerr ("%f>", mat[G_N_ELEMENTS (mat)-1]);
      break;
    }

    case GSK_GL_UNIFORM_FORMAT_1FV:
    case GSK_GL_UNIFORM_FORMAT_2FV:
    case GSK_GL_UNIFORM_FORMAT_3FV:
    case GSK_GL_UNIFORM_FORMAT_4FV:
      /* non-V variants are -4 from V variants */
      format -= 4;
      g_printerr ("[");
      for (guint i = 0; i < array_count; i++)
        {
          print_uniform (format, 0, valueptr);
          if (i + 1 != array_count)
            g_printerr (",");
          valueptr = ((guint8*)valueptr + gsk_gl_uniform_format_size (format));
        }
      g_printerr ("]");
      break;

    case GSK_GL_UNIFORM_FORMAT_2I:
      g_printerr ("2i<%d,%d>", data->ival[0], data->ival[1]);
      break;

    case GSK_GL_UNIFORM_FORMAT_3I:
      g_printerr ("3i<%d,%d,%d>", data->ival[0], data->ival[1], data->ival[2]);
      break;

    case GSK_GL_UNIFORM_FORMAT_4I:
      g_printerr ("3i<%d,%d,%d,%d>", data->ival[0], data->ival[1], data->ival[2], data->ival[3]);
      break;

    case GSK_GL_UNIFORM_FORMAT_LAST:
    default:
      g_assert_not_reached ();
    }
}

G_GNUC_UNUSED static inline void
gsk_gl_command_queue_print_batch (GskGLCommandQueue       *self,
                                  const GskGLCommandBatch *batch)
{
  static const char *command_kinds[] = { "Clear", "Draw", };
  guint framebuffer_id;

  g_assert (GSK_IS_GL_COMMAND_QUEUE (self));
  g_assert (batch != NULL);

  if (batch->any.kind == GSK_GL_COMMAND_KIND_CLEAR)
    framebuffer_id = batch->clear.framebuffer;
  else if (batch->any.kind == GSK_GL_COMMAND_KIND_DRAW)
    framebuffer_id = batch->draw.framebuffer;
  else
    return;

  g_printerr ("Batch {\n");
  g_printerr ("         Kind: %s\n", command_kinds[batch->any.kind]);
  g_printerr ("     Viewport: %dx%d\n", batch->any.viewport.width, batch->any.viewport.height);
  g_printerr ("  Framebuffer: %d\n", framebuffer_id);

  if (batch->any.kind == GSK_GL_COMMAND_KIND_DRAW)
    {
      g_printerr ("      Program: %d\n", batch->any.program);
      g_printerr ("     Vertices: %d\n", batch->draw.vbo_count);

      for (guint i = 0; i < batch->draw.bind_count; i++)
        {
          const GskGLCommandBind *bind = &self->batch_binds.items[batch->draw.bind_offset + i];
          g_printerr ("      Bind[%d]: %u\n", bind->texture, bind->id);
        }

      for (guint i = 0; i < batch->draw.uniform_count; i++)
        {
          const GskGLCommandUniform *uniform = &self->batch_uniforms.items[batch->draw.uniform_offset + i];
          g_printerr ("  Uniform[%02d]: ", uniform->location);
          print_uniform (uniform->info.format,
                         uniform->info.array_count,
                         gsk_gl_uniform_state_get_uniform_data (self->uniforms, uniform->info.offset));
          g_printerr ("\n");
        }
    }
  else if (batch->any.kind == GSK_GL_COMMAND_KIND_CLEAR)
    {
      g_printerr ("         Bits: 0x%x\n", batch->clear.bits);
    }

  g_printerr ("}\n");
}

G_GNUC_UNUSED static inline void
gsk_gl_command_queue_capture_png (GskGLCommandQueue *self,
                                  const char        *filename,
                                  guint              width,
                                  guint              height,
                                  gboolean           flip_y)
{
  guint stride;
  guint8 *data;
  GBytes *bytes;
  GdkTexture *texture;

  g_assert (GSK_IS_GL_COMMAND_QUEUE (self));
  g_assert (filename != NULL);

  stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, width);
  data = g_malloc_n (height, stride);

  glReadPixels (0, 0, width, height, GL_BGRA, GL_UNSIGNED_BYTE, data);

  if (flip_y)
    {
      guint8 *flipped = g_malloc_n (height, stride);

      for (guint i = 0; i < height; i++)
        memcpy (flipped + (height * stride) - ((i + 1) * stride),
                data + (stride * i),
                stride);

      g_free (data);
      data = flipped;
    }

  bytes = g_bytes_new_take (data, height * stride);
  texture = gdk_memory_texture_new (width, height, GDK_MEMORY_DEFAULT, bytes, stride);
  g_bytes_unref (bytes);

  gdk_texture_save_to_png (texture, filename);
  g_object_unref (texture);
}
#endif

static inline gboolean
will_ignore_batch (GskGLCommandQueue *self)
{
  if G_LIKELY (self->batches.len < G_MAXINT16)
    return FALSE;

  if (!self->have_truncated)
    {
      self->have_truncated = TRUE;
      g_critical ("GL command queue too large, truncating further batches.");
    }

  return TRUE;
}

static inline GLint
min_filter_from_index (guint index)
{
  GLint filters[3] = { GL_LINEAR, GL_NEAREST, GL_LINEAR_MIPMAP_LINEAR };

  g_assert (index < GSK_GL_N_FILTERS);

  return filters[index];
}

static inline GLint
mag_filter_from_index (guint index)
{
  GLint filters[3] = { GL_LINEAR, GL_NEAREST, GL_LINEAR };

  g_assert (index < GSK_GL_N_FILTERS);

  return filters[index];
}

static inline guint
snapshot_attachments (const GskGLAttachmentState *state,
                      GskGLCommandBinds          *array)
{
  GskGLCommandBind *bind = gsk_gl_command_binds_append_n (array, G_N_ELEMENTS (state->textures));
  guint count = 0;

  for (guint i = 0; i < G_N_ELEMENTS (state->textures); i++)
    {
      if (state->textures[i].id)
        {
          bind[count].id = state->textures[i].id;
          bind[count].texture = state->textures[i].texture;
          if (state->textures[i].target == GL_TEXTURE_EXTERNAL_OES)
            bind[count].sampler = SAMPLER_EXTERNAL;
          else
            bind[count].sampler = state->textures[i].sampler;
          count++;
        }
    }

  if (count != G_N_ELEMENTS (state->textures))
    array->len -= G_N_ELEMENTS (state->textures) - count;

  return count;
}

static inline guint
snapshot_uniforms (GskGLUniformState    *state,
                   GskGLUniformProgram  *program,
                   GskGLCommandUniforms *array)
{
  GskGLCommandUniform *uniform = gsk_gl_command_uniforms_append_n (array, program->n_mappings);
  guint count = 0;

  for (guint i = 0; i < program->n_mappings; i++)
    {
      const GskGLUniformMapping *mapping = &program->mappings[i];

      if (!mapping->info.initial && mapping->info.format && mapping->location > -1)
        {
          uniform[count].location = mapping->location;
          uniform[count].info = mapping->info;
          count++;
        }
    }

  if (count != program->n_mappings)
    array->len -= program->n_mappings - count;

  return count;
}

static inline gboolean
snapshots_equal (GskGLCommandQueue *self,
                 GskGLCommandBatch *first,
                 GskGLCommandBatch *second)
{
  if (first->draw.bind_count != second->draw.bind_count ||
      first->draw.uniform_count != second->draw.uniform_count)
    return FALSE;

  for (guint i = 0; i < first->draw.bind_count; i++)
    {
      const GskGLCommandBind *fb = &self->batch_binds.items[first->draw.bind_offset+i];
      const GskGLCommandBind *sb = &self->batch_binds.items[second->draw.bind_offset+i];

      if (fb->id != sb->id || fb->texture != sb->texture)
        return FALSE;
    }

  for (guint i = 0; i < first->draw.uniform_count; i++)
    {
      const GskGLCommandUniform *fu = &self->batch_uniforms.items[first->draw.uniform_offset+i];
      const GskGLCommandUniform *su = &self->batch_uniforms.items[second->draw.uniform_offset+i];
      gconstpointer fdata;
      gconstpointer sdata;
      gsize len;

      /* Short circuit if we'd end up with the same memory */
      if (fu->info.offset == su->info.offset)
        continue;

      if (fu->info.format != su->info.format ||
          fu->info.array_count != su->info.array_count)
        return FALSE;

      fdata = gsk_gl_uniform_state_get_uniform_data (self->uniforms, fu->info.offset);
      sdata = gsk_gl_uniform_state_get_uniform_data (self->uniforms, su->info.offset);

      switch (fu->info.format)
        {
        case GSK_GL_UNIFORM_FORMAT_1F:
        case GSK_GL_UNIFORM_FORMAT_1FV:
        case GSK_GL_UNIFORM_FORMAT_1I:
        case GSK_GL_UNIFORM_FORMAT_TEXTURE:
        case GSK_GL_UNIFORM_FORMAT_1UI:
          len = 4;
          break;

        case GSK_GL_UNIFORM_FORMAT_2F:
        case GSK_GL_UNIFORM_FORMAT_2FV:
        case GSK_GL_UNIFORM_FORMAT_2I:
          len = 8;
          break;

        case GSK_GL_UNIFORM_FORMAT_3F:
        case GSK_GL_UNIFORM_FORMAT_3FV:
        case GSK_GL_UNIFORM_FORMAT_3I:
          len = 12;
          break;

        case GSK_GL_UNIFORM_FORMAT_4F:
        case GSK_GL_UNIFORM_FORMAT_4FV:
        case GSK_GL_UNIFORM_FORMAT_4I:
          len = 16;
          break;


        case GSK_GL_UNIFORM_FORMAT_MATRIX:
          len = sizeof (float) * 16;
          break;

        case GSK_GL_UNIFORM_FORMAT_ROUNDED_RECT:
          len = sizeof (float) * 12;
          break;

        case GSK_GL_UNIFORM_FORMAT_COLOR:
          len = sizeof (float) * 4;
          break;

        default:
          g_assert_not_reached ();
        }

      len *= fu->info.array_count;

      if (memcmp (fdata, sdata, len) != 0)
        return FALSE;
    }

  return TRUE;
}

static void
gsk_gl_command_queue_dispose (GObject *object)
{
  GskGLCommandQueue *self = (GskGLCommandQueue *)object;

  g_assert (GSK_IS_GL_COMMAND_QUEUE (self));

  g_clear_object (&self->profiler);
  g_clear_object (&self->gl_profiler);
  g_clear_object (&self->context);
  g_clear_pointer (&self->attachments, gsk_gl_attachment_state_unref);
  g_clear_pointer (&self->uniforms, gsk_gl_uniform_state_unref);

  if (self->has_samplers)
    glDeleteSamplers (G_N_ELEMENTS (self->samplers), self->samplers);

  gsk_gl_command_batches_clear (&self->batches);
  gsk_gl_command_binds_clear (&self->batch_binds);
  gsk_gl_command_uniforms_clear (&self->batch_uniforms);
  gsk_gl_syncs_clear (&self->syncs);

  gsk_gl_buffer_destroy (&self->vertices);

  G_OBJECT_CLASS (gsk_gl_command_queue_parent_class)->dispose (object);
}

static void
gsk_gl_command_queue_class_init (GskGLCommandQueueClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gsk_gl_command_queue_dispose;
}

static void
gsk_gl_command_queue_init (GskGLCommandQueue *self)
{
  self->max_texture_size = -1;

  gsk_gl_command_batches_init (&self->batches, 128);
  gsk_gl_command_binds_init (&self->batch_binds, 1024);
  gsk_gl_command_uniforms_init (&self->batch_uniforms, 2048);
  gsk_gl_syncs_init (&self->syncs, 10);

  gsk_gl_buffer_init (&self->vertices, GL_ARRAY_BUFFER, sizeof (GskGLDrawVertex));
}

GskGLCommandQueue *
gsk_gl_command_queue_new (GdkGLContext      *context,
                          GskGLUniformState *uniforms)
{
  GskGLCommandQueue *self;
  guint i;

  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), NULL);

  self = g_object_new (GSK_TYPE_GL_COMMAND_QUEUE, NULL);
  self->context = g_object_ref (context);
  self->attachments = gsk_gl_attachment_state_new ();

  /* Use shared uniform state if we're provided one */
  if (uniforms != NULL)
    self->uniforms = gsk_gl_uniform_state_ref (uniforms);
  else
    self->uniforms = gsk_gl_uniform_state_new ();

  /* Determine max texture size immediately and restore context */
  gdk_gl_context_make_current (context);

  glGetIntegerv (GL_MAX_TEXTURE_SIZE, &self->max_texture_size);
  if (g_getenv ("GSK_MAX_TEXTURE_SIZE"))
    {
      int max_texture_size = atoi (g_getenv ("GSK_MAX_TEXTURE_SIZE"));
      if (max_texture_size == 0)
        {
          g_warning ("Failed to parse %s", "GSK_MAX_TEXTURE_SIZE");
        }
      else
        {
          max_texture_size = MAX (max_texture_size, 512);
          GSK_DEBUG (RENDERER,
                     "Limiting texture size in the GL renderer to %d (via %s)",
                     max_texture_size, "GSK_MAX_TEXTURE_SIZE");
          self->max_texture_size = MIN (self->max_texture_size, max_texture_size);
        }
    }

  self->has_samplers = gdk_gl_context_check_version (context, "3.3", "3.0");
  self->can_swizzle = gdk_gl_context_check_version (context, "3.0", "3.0");

  /* create the samplers */
  if (self->has_samplers)
    {
      glGenSamplers (G_N_ELEMENTS (self->samplers), self->samplers);
      for (i = 0; i < G_N_ELEMENTS (self->samplers); i++)
        {
          glSamplerParameteri (self->samplers[i], GL_TEXTURE_MIN_FILTER, min_filter_from_index (i / GSK_GL_N_FILTERS));
          glSamplerParameteri (self->samplers[i], GL_TEXTURE_MAG_FILTER, mag_filter_from_index (i % GSK_GL_N_FILTERS));
          glSamplerParameteri (self->samplers[i], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
          glSamplerParameteri (self->samplers[i], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
    }

  return g_steal_pointer (&self);
}

static inline GskGLCommandBatch *
begin_next_batch (GskGLCommandQueue *self)
{
  GskGLCommandBatch *batch;

  g_assert (GSK_IS_GL_COMMAND_QUEUE (self));

  /* GskGLCommandBatch contains an embedded linked list using integers into the
   * self->batches array. We can't use pointer because the batches could be
   * realloc()'d at runtime.
   *
   * Before we execute the command queue, we sort the batches by framebuffer but
   * leave the batches in place as we can just tweak the links via prev/next.
   *
   * Generally we only traverse forwards, so we could ignore the previous field.
   * But to optimize the reordering of batches by framebuffer we walk backwards
   * so we sort by most-recently-seen framebuffer to ensure draws happen in the
   * proper order.
   */

  batch = gsk_gl_command_batches_append (&self->batches);
  batch->any.next_batch_index = -1;
  batch->any.prev_batch_index = self->tail_batch_index;

  return batch;
}

static void
enqueue_batch (GskGLCommandQueue *self)
{
  guint index;

  g_assert (GSK_IS_GL_COMMAND_QUEUE (self));
  g_assert (self->batches.len > 0);

  /* Batches are linked lists but using indexes into the batches array instead
   * of pointers. This is for two main reasons. First, 16-bit indexes allow us
   * to store the information in 4 bytes, where as two pointers would take 16
   * bytes.  Furthermore, we have an array here so pointers would get
   * invalidated if we realloc()'d (and that can happen from time to time).
   */

  index = self->batches.len - 1;

  if (self->head_batch_index == -1)
    self->head_batch_index = index;

  if (self->tail_batch_index != -1)
    {
      GskGLCommandBatch *prev = &self->batches.items[self->tail_batch_index];

      prev->any.next_batch_index = index;
    }

  self->tail_batch_index = index;
}

static void
discard_batch (GskGLCommandQueue *self)
{
  g_assert (GSK_IS_GL_COMMAND_QUEUE (self));
  g_assert (self->batches.len > 0);

  self->batches.len--;
}

gboolean
gsk_gl_command_queue_begin_draw (GskGLCommandQueue   *self,
                                 GskGLUniformProgram *program,
                                 guint                width,
                                 guint                height)
{
  GskGLCommandBatch *batch;

  g_assert (GSK_IS_GL_COMMAND_QUEUE (self));
  g_assert (self->in_draw == FALSE);
  g_assert (width <= G_MAXUINT16);
  g_assert (height <= G_MAXUINT16);

  /* Our internal links use 16-bits, so that is our max number
   * of batches we can have in one frame.
   */
  if (will_ignore_batch (self))
    return FALSE;

  self->program_info = program;

  batch = begin_next_batch (self);
  batch->any.kind = GSK_GL_COMMAND_KIND_DRAW;
  batch->any.program = program->program_id;
  batch->any.next_batch_index = -1;
  batch->any.viewport.width = width;
  batch->any.viewport.height = height;
  batch->draw.blend = 1;
  batch->draw.framebuffer = 0;
  batch->draw.uniform_count = 0;
  batch->draw.uniform_offset = self->batch_uniforms.len;
  batch->draw.bind_count = 0;
  batch->draw.bind_offset = self->batch_binds.len;
  batch->draw.vbo_count = 0;
  batch->draw.vbo_offset = gsk_gl_buffer_get_offset (&self->vertices);

  self->fbo_max = MAX (self->fbo_max, batch->draw.framebuffer);

  self->in_draw = TRUE;

  return TRUE;
}

void
gsk_gl_command_queue_end_draw (GskGLCommandQueue *self)
{
  GskGLCommandBatch *last_batch;
  GskGLCommandBatch *batch;

  g_assert (GSK_IS_GL_COMMAND_QUEUE (self));
  g_assert (self->batches.len > 0);

  if (will_ignore_batch (self))
    {
      self->in_draw = FALSE;
      return;
    }

  batch = gsk_gl_command_batches_tail (&self->batches);

  g_assert (self->in_draw == TRUE);
  g_assert (batch->any.kind == GSK_GL_COMMAND_KIND_DRAW);

  if G_UNLIKELY (batch->draw.vbo_count == 0)
    {
      discard_batch (self);
      self->in_draw = FALSE;
      return;
    }

  /* Track the destination framebuffer in case it changed */
  batch->draw.framebuffer = self->attachments->fbo.id;
  self->attachments->fbo.changed = FALSE;
  self->fbo_max = MAX (self->fbo_max, self->attachments->fbo.id);

  /* Save our full uniform state for this draw so we can possibly
   * reorder the draw later.
   */
  batch->draw.uniform_offset = self->batch_uniforms.len;
  batch->draw.uniform_count = snapshot_uniforms (self->uniforms, self->program_info, &self->batch_uniforms);

  /* Track the bind attachments that changed */
  if (self->program_info->has_attachments)
    {
      batch->draw.bind_offset = self->batch_binds.len;
      batch->draw.bind_count = snapshot_attachments (self->attachments, &self->batch_binds);
    }
  else
    {
      batch->draw.bind_offset = 0;
      batch->draw.bind_count = 0;
    }

  if (self->batches.len > 1)
    last_batch = &self->batches.items[self->batches.len - 2];
  else
    last_batch = NULL;

  /* Do simple chaining of draw to last batch. */
  if (last_batch != NULL &&
      last_batch->any.kind == GSK_GL_COMMAND_KIND_DRAW &&
      last_batch->any.program == batch->any.program &&
      last_batch->any.viewport.width == batch->any.viewport.width &&
      last_batch->any.viewport.height == batch->any.viewport.height &&
      last_batch->draw.blend == batch->draw.blend &&
      last_batch->draw.framebuffer == batch->draw.framebuffer &&
      last_batch->draw.vbo_offset + last_batch->draw.vbo_count == batch->draw.vbo_offset &&
      last_batch->draw.vbo_count + batch->draw.vbo_count <= 0xffff &&
      snapshots_equal (self, last_batch, batch))
    {
      last_batch->draw.vbo_count += batch->draw.vbo_count;
      discard_batch (self);
    }
  else
    {
      enqueue_batch (self);
    }

  self->in_draw = FALSE;
  self->program_info = NULL;
}

/**
 * gsk_gl_command_queue_split_draw:
 * @self a `GskGLCommandQueue`
 *
 * This function is like calling gsk_gl_command_queue_end_draw() followed by
 * a gsk_gl_command_queue_begin_draw() with the same parameters as a
 * previous begin draw (if shared uniforms where not changed further).
 *
 * This is useful to avoid comparisons inside of loops where we know shared
 * uniforms are not changing.
 *
 * This generally should just be called from gsk_gl_program_split_draw()
 * as that is where the begin/end flow happens from the render job.
 */
void
gsk_gl_command_queue_split_draw (GskGLCommandQueue *self)
{
  GskGLCommandBatch *batch;
  GskGLUniformProgram *program;
  guint width;
  guint height;

  g_assert (GSK_IS_GL_COMMAND_QUEUE (self));
  g_assert (self->batches.len > 0);

  if (will_ignore_batch (self))
    return;

  g_assert (self->in_draw == TRUE);

  program = self->program_info;

  batch = gsk_gl_command_batches_tail (&self->batches);

  g_assert (batch->any.kind == GSK_GL_COMMAND_KIND_DRAW);

  width = batch->any.viewport.width;
  height = batch->any.viewport.height;

  gsk_gl_command_queue_end_draw (self);
  gsk_gl_command_queue_begin_draw (self, program, width, height);
}

void
gsk_gl_command_queue_clear (GskGLCommandQueue     *self,
                            guint                  clear_bits,
                            const graphene_rect_t *viewport)
{
  GskGLCommandBatch *batch;

  g_assert (GSK_IS_GL_COMMAND_QUEUE (self));
  g_assert (self->in_draw == FALSE);

  if (will_ignore_batch (self))
    return;

  if (clear_bits == 0)
    clear_bits = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;

  batch = begin_next_batch (self);
  batch->any.kind = GSK_GL_COMMAND_KIND_CLEAR;
  batch->any.viewport.width = viewport->size.width;
  batch->any.viewport.height = viewport->size.height;
  batch->clear.bits = clear_bits;
  batch->clear.framebuffer = self->attachments->fbo.id;
  batch->any.next_batch_index = -1;
  batch->any.program = 0;

  self->fbo_max = MAX (self->fbo_max, batch->clear.framebuffer);

  enqueue_batch (self);

  self->attachments->fbo.changed = FALSE;
}

GdkGLContext *
gsk_gl_command_queue_get_context (GskGLCommandQueue *self)
{
  g_return_val_if_fail (GSK_IS_GL_COMMAND_QUEUE (self), NULL);

  return self->context;
}

void
gsk_gl_command_queue_make_current (GskGLCommandQueue *self)
{
  g_assert (GSK_IS_GL_COMMAND_QUEUE (self));
  g_assert (GDK_IS_GL_CONTEXT (self->context));

  gdk_gl_context_make_current (self->context);
}

void
gsk_gl_command_queue_delete_program (GskGLCommandQueue *self,
                                     guint              program)
{
  g_assert (GSK_IS_GL_COMMAND_QUEUE (self));

  glDeleteProgram (program);
}

static inline void
apply_viewport (guint *current_width,
                guint *current_height,
                guint  width,
                guint  height)
{
  if G_UNLIKELY (*current_width != width || *current_height != height)
    {
      *current_width = width;
      *current_height = height;
      glViewport (0, 0, width, height);
    }
}

static inline void
apply_scissor (gboolean              *state,
               guint                  framebuffer,
               const graphene_rect_t *scissor,
               gboolean               has_scissor,
               guint                  default_framebuffer)
{
  g_assert (framebuffer != (guint)-1);

  if (framebuffer != default_framebuffer || !has_scissor)
    {
      if (*state != FALSE)
        {
          glDisable (GL_SCISSOR_TEST);
          *state = FALSE;
        }
    }
  else
    {
      if (*state != TRUE)
        {
          glEnable (GL_SCISSOR_TEST);
          glScissor (scissor->origin.x,
                     scissor->origin.y,
                     scissor->size.width,
                     scissor->size.height);
          *state = TRUE;
        }
    }
}

static inline gboolean
apply_framebuffer (int   *framebuffer,
                   guint  new_framebuffer)
{
  if G_UNLIKELY (new_framebuffer != *framebuffer)
    {
      *framebuffer = new_framebuffer;
      glBindFramebuffer (GL_FRAMEBUFFER, new_framebuffer);
      return TRUE;
    }

  return FALSE;
}

static inline void
gsk_gl_command_queue_unlink (GskGLCommandQueue *self,
                             GskGLCommandBatch *batch)
{
  if (batch->any.prev_batch_index == -1)
    self->head_batch_index = batch->any.next_batch_index;
  else
    self->batches.items[batch->any.prev_batch_index].any.next_batch_index = batch->any.next_batch_index;

  if (batch->any.next_batch_index == -1)
    self->tail_batch_index = batch->any.prev_batch_index;
  else
    self->batches.items[batch->any.next_batch_index].any.prev_batch_index = batch->any.prev_batch_index;

  batch->any.prev_batch_index = -1;
  batch->any.next_batch_index = -1;
}

static inline void
gsk_gl_command_queue_insert_before (GskGLCommandQueue *self,
                                    GskGLCommandBatch *batch,
                                    GskGLCommandBatch *sibling)
{
  int sibling_index;
  int index;

  g_assert (batch >= self->batches.items);
  g_assert (batch < &self->batches.items[self->batches.len]);
  g_assert (sibling >= self->batches.items);
  g_assert (sibling < &self->batches.items[self->batches.len]);

  index = gsk_gl_command_batches_index_of (&self->batches, batch);
  sibling_index = gsk_gl_command_batches_index_of (&self->batches, sibling);

  batch->any.next_batch_index = sibling_index;
  batch->any.prev_batch_index = sibling->any.prev_batch_index;

  if (batch->any.prev_batch_index > -1)
    self->batches.items[batch->any.prev_batch_index].any.next_batch_index = index;

  sibling->any.prev_batch_index = index;

  if (batch->any.prev_batch_index == -1)
    self->head_batch_index = index;
}

static void
gsk_gl_command_queue_sort_batches (GskGLCommandQueue *self)
{
  int *seen;
  int *seen_free = NULL;
  int index;

  g_assert (GSK_IS_GL_COMMAND_QUEUE (self));
  g_assert (self->tail_batch_index >= 0);
  g_assert (self->fbo_max >= 0);

  /* Create our seen list with most recent index set to -1,
   * meaning we haven't yet seen that framebuffer.
   */
  if (self->fbo_max < 1024)
    seen = g_alloca (sizeof (int) * (self->fbo_max + 1));
  else
    seen = seen_free = g_new0 (int, (self->fbo_max + 1));
  for (int i = 0; i <= self->fbo_max; i++)
    seen[i] = -1;

  /* Walk in reverse, and if we've seen that framebuffer before, we want to
   * delay this operation until right before the last batch we saw for that
   * framebuffer.
   *
   * We can do this because we don't use a framebuffer's texture until it has
   * been completely drawn.
   */
  index = self->tail_batch_index;

  while (index >= 0)
    {
      GskGLCommandBatch *batch = &self->batches.items[index];
      int cur_index = index;
      int fbo = -1;

      g_assert (index > -1);
      g_assert (index < self->batches.len);

      switch (batch->any.kind)
        {
        case GSK_GL_COMMAND_KIND_DRAW:
          fbo = batch->draw.framebuffer;
          break;

        case GSK_GL_COMMAND_KIND_CLEAR:
          fbo = batch->clear.framebuffer;
          break;

        default:
          g_assert_not_reached ();
        }

      index = batch->any.prev_batch_index;

      g_assert (index >= -1);
      g_assert (index < (int)self->batches.len);
      g_assert (fbo >= -1);

      if (fbo == -1)
        continue;

      g_assert (fbo <= self->fbo_max);
      g_assert (seen[fbo] >= -1);
      g_assert (seen[fbo] < (int)self->batches.len);

      if (seen[fbo] != -1 && seen[fbo] != batch->any.next_batch_index)
        {
          int mru_index = seen[fbo];
          GskGLCommandBatch *mru = &self->batches.items[mru_index];

          g_assert (mru_index > -1);

          gsk_gl_command_queue_unlink (self, batch);

          g_assert (batch->any.prev_batch_index == -1);
          g_assert (batch->any.next_batch_index == -1);

          gsk_gl_command_queue_insert_before (self, batch, mru);

          g_assert (batch->any.prev_batch_index > -1 ||
                    self->head_batch_index == cur_index);
          g_assert (batch->any.next_batch_index == seen[fbo]);
        }

      g_assert (cur_index > -1);
      g_assert (seen[fbo] >= -1);

      seen[fbo] = cur_index;
    }

  g_free (seen_free);
}

/**
 * gsk_gl_command_queue_execute:
 * @self: a `GskGLCommandQueue`
 * @surface_height: the height of the backing surface
 * @scale: the scale of the backing surface
 * @scissor: (nullable): the scissor clip if any
 * @default_framebuffer: the default framebuffer id if not zero
 *
 * Executes all of the batches in the command queue.
 *
 * Typically, the scissor rect is only applied when rendering to the default
 * framebuffer (zero in most cases). However, if @default_framebuffer is not
 * zero, it will be checked to see if the rendering target matches so that
 * the scissor rect is applied. This should be used in cases where rendering
 * to the backbuffer for display is not the default GL framebuffer of zero.
 * Currently, this happens when rendering on macOS using IOSurface.
 */
void
gsk_gl_command_queue_execute (GskGLCommandQueue    *self,
                              guint                 surface_height,
                              float                 scale,
                              const cairo_region_t *scissor,
                              guint                 default_framebuffer)
{
  G_GNUC_UNUSED guint count = 0;
  graphene_rect_t scissor_test = GRAPHENE_RECT_INIT (0, 0, 0, 0);
  gboolean has_scissor = scissor != NULL;
  gboolean scissor_state = -1;
  guint program = 0;
  guint width = 0;
  guint height = 0;
  G_GNUC_UNUSED unsigned int n_binds = 0;
  G_GNUC_UNUSED unsigned int n_fbos = 0;
  G_GNUC_UNUSED unsigned int n_uniforms = 0;
  G_GNUC_UNUSED unsigned int n_programs = 0;
  guint vao_id;
  guint vbo_id;
  int textures[GSK_GL_MAX_TEXTURES_PER_PROGRAM];
  int samplers[GSK_GL_MAX_TEXTURES_PER_PROGRAM];
  int framebuffer = -1;
  int next_batch_index;
  int active = -1;

  g_assert (GSK_IS_GL_COMMAND_QUEUE (self));
  g_assert (self->in_draw == FALSE);

  if (self->batches.len == 0)
    return;

  for (guint i = 0; i < G_N_ELEMENTS (textures); i++)
    textures[i] = -1;
  for (guint i = 0; i < G_N_ELEMENTS (samplers); i++)
    samplers[i] = -1;

  gsk_gl_command_queue_sort_batches (self);

  gsk_gl_command_queue_make_current (self);

  gsk_gl_profiler_begin_gpu_region (self->gl_profiler);
  gsk_profiler_timer_begin (self->profiler, self->metrics.cpu_time);

  glDisable (GL_DEPTH_TEST);

  /* Pre-multiplied alpha */
  glEnable (GL_BLEND);
  glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glBlendEquation (GL_FUNC_ADD);

  if (gdk_gl_context_has_vertex_arrays (self->context))
    {
      glGenVertexArrays (1, &vao_id);
      glBindVertexArray (vao_id);
    }

  vbo_id = gsk_gl_buffer_submit (&self->vertices);

  /* 0 = position location */
  glEnableVertexAttribArray (0);
  glVertexAttribPointer (0, 2, GL_FLOAT, GL_FALSE,
                         sizeof (GskGLDrawVertex),
                         (void *) G_STRUCT_OFFSET (GskGLDrawVertex, position));

  /* 1 = texture coord location */
  glEnableVertexAttribArray (1);
  glVertexAttribPointer (1, 2, GL_FLOAT, GL_FALSE,
                         sizeof (GskGLDrawVertex),
                         (void *) G_STRUCT_OFFSET (GskGLDrawVertex, uv));

  /* 2 = color location */
  glEnableVertexAttribArray (2);
  glVertexAttribPointer (2, 4, GL_HALF_FLOAT, GL_FALSE,
                         sizeof (GskGLDrawVertex),
                         (void *) G_STRUCT_OFFSET (GskGLDrawVertex, color));

  /* 3 = color2 location */
  glEnableVertexAttribArray (3);
  glVertexAttribPointer (3, 4, GL_HALF_FLOAT, GL_FALSE,
                         sizeof (GskGLDrawVertex),
                         (void *) G_STRUCT_OFFSET (GskGLDrawVertex, color2));

  /* Setup initial scissor clip */
  if (scissor != NULL && cairo_region_num_rectangles (scissor) > 0)
    {
      cairo_rectangle_int_t r;

      g_assert (cairo_region_num_rectangles (scissor) == 1);
      cairo_region_get_rectangle (scissor, 0, &r);

      scissor_test.origin.x = (int) floor (r.x * scale);
      scissor_test.origin.y = (int) floor (surface_height - (r.height * scale) - (r.y * scale));
      scissor_test.size.width = (int) ceil (r.width * scale);
      scissor_test.size.height = (int) ceil (r.height * scale);
    }

  next_batch_index = self->head_batch_index;

  while (next_batch_index >= 0)
    {
      const GskGLCommandBatch *batch = &self->batches.items[next_batch_index];

      g_assert (next_batch_index >= 0);
      g_assert (next_batch_index < self->batches.len);
      g_assert (batch->any.next_batch_index != next_batch_index);

      count++;

      switch (batch->any.kind)
        {
        case GSK_GL_COMMAND_KIND_CLEAR:
          if (apply_framebuffer (&framebuffer, batch->clear.framebuffer))
            {
              apply_scissor (&scissor_state, framebuffer, &scissor_test, has_scissor, default_framebuffer);
              n_fbos++;
            }

          apply_viewport (&width,
                          &height,
                          batch->any.viewport.width,
                          batch->any.viewport.height);

          glClearColor (0, 0, 0, 0);
          glClear (batch->clear.bits);
        break;

        case GSK_GL_COMMAND_KIND_DRAW:
          if (batch->any.program != program)
            {
              program = batch->any.program;
              glUseProgram (program);

              n_programs++;
            }

          if (apply_framebuffer (&framebuffer, batch->draw.framebuffer))
            {
              apply_scissor (&scissor_state, framebuffer, &scissor_test, has_scissor, default_framebuffer);
              n_fbos++;
            }

          apply_viewport (&width,
                          &height,
                          batch->any.viewport.width,
                          batch->any.viewport.height);

          if G_UNLIKELY (batch->draw.bind_count > 0)
            {
              const GskGLCommandBind *bind = &self->batch_binds.items[batch->draw.bind_offset];

              g_assert (bind->texture < G_N_ELEMENTS (textures));
              for (guint i = 0; i < batch->draw.bind_count; i++)
                {
                  if (textures[bind->texture] != bind->id)
                    {
                      GskGLSync *s;

                      if (active != bind->texture)
                        {
                          active = bind->texture;
                          glActiveTexture (GL_TEXTURE0 + bind->texture);
                        }

                      s = gsk_gl_syncs_get_sync (&self->syncs, bind->id);
                      if (s && s->sync)
                        {
                          glWaitSync ((GLsync) s->sync, 0, GL_TIMEOUT_IGNORED);
                          s->sync = NULL;
                        }

                      if (bind->sampler == SAMPLER_EXTERNAL)
                        glBindTexture (GL_TEXTURE_EXTERNAL_OES, bind->id);
                      else
                        glBindTexture (GL_TEXTURE_2D, bind->id);
                      textures[bind->texture] = bind->id;
                      if (!self->has_samplers)
                        {
                          if (bind->sampler == SAMPLER_EXTERNAL)
                            {
                              glTexParameteri (GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                              glTexParameteri (GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                            }
                          else
                            {
                              glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter_from_index (bind->sampler / GSK_GL_N_FILTERS));
                              glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter_from_index (bind->sampler % GSK_GL_N_FILTERS));
                            }
                        }
                    }

                  if (samplers[bind->texture] != bind->sampler)
                    {
                      if (self->has_samplers)
                        glBindSampler (bind->texture, self->samplers[bind->sampler]);
                      else
                        {
                          if (bind->sampler == SAMPLER_EXTERNAL)
                            {
                              glTexParameteri (GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                              glTexParameteri (GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                            }
                          else
                            {
                              glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter_from_index (bind->sampler / GSK_GL_N_FILTERS));
                              glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter_from_index (bind->sampler % GSK_GL_N_FILTERS));
                            }
                        }
                      samplers[bind->texture] = bind->sampler;
                    }

                  bind++;
                }

              n_binds += batch->draw.bind_count;
            }

          if (batch->draw.uniform_count > 0)
            {
              const GskGLCommandUniform *u = &self->batch_uniforms.items[batch->draw.uniform_offset];

              for (guint i = 0; i < batch->draw.uniform_count; i++, u++)
                gsk_gl_uniform_state_apply (self->uniforms, program, u->location, u->info);

              n_uniforms += batch->draw.uniform_count;
            }

          if (batch->draw.blend == 0)
            glDisable (GL_BLEND);

          glDrawArrays (GL_TRIANGLES, batch->draw.vbo_offset, batch->draw.vbo_count);

          if (batch->draw.blend == 0)
            glEnable (GL_BLEND);
        break;

        default:
          g_assert_not_reached ();
        }

#if 0
      if (batch->any.kind == GSK_GL_COMMAND_KIND_DRAW ||
          batch->any.kind == GSK_GL_COMMAND_KIND_CLEAR)
        {
          char filename[128];
          g_snprintf (filename, sizeof filename,
                      "capture%03u_batch%03d_kind%u_program%u_u%u_b%u_fb%u_ctx%p.png",
                      count, next_batch_index,
                      batch->any.kind, batch->any.program,
                      batch->any.kind == GSK_GL_COMMAND_KIND_DRAW ? batch->draw.uniform_count : 0,
                      batch->any.kind == GSK_GL_COMMAND_KIND_DRAW ? batch->draw.bind_count : 0,
                      framebuffer,
                      gdk_gl_context_get_current ());
          gsk_gl_command_queue_capture_png (self, filename, width, height, TRUE);
          gsk_gl_command_queue_print_batch (self, batch);
        }
#endif

      next_batch_index = batch->any.next_batch_index;
    }

  glDeleteBuffers (1, &vbo_id);
  if (gdk_gl_context_has_vertex_arrays (self->context))
    glDeleteVertexArrays (1, &vao_id);

  gdk_profiler_set_int_counter (self->metrics.n_binds, n_binds);
  gdk_profiler_set_int_counter (self->metrics.n_uniforms, n_uniforms);
  gdk_profiler_set_int_counter (self->metrics.n_fbos, n_fbos);
  gdk_profiler_set_int_counter (self->metrics.n_programs, n_programs);
  gdk_profiler_set_int_counter (self->metrics.n_uploads, self->n_uploads);
  gdk_profiler_set_int_counter (self->metrics.queue_depth, self->batches.len);

  {
    gint64 start_time G_GNUC_UNUSED = gsk_profiler_timer_get_start (self->profiler, self->metrics.cpu_time);
    gint64 cpu_time = gsk_profiler_timer_end (self->profiler, self->metrics.cpu_time);
    gint64 gpu_time = gsk_gl_profiler_end_gpu_region (self->gl_profiler);

    gsk_profiler_timer_set (self->profiler, self->metrics.gpu_time, gpu_time);
    gsk_profiler_timer_set (self->profiler, self->metrics.cpu_time, cpu_time);
    gsk_profiler_counter_inc (self->profiler, self->metrics.n_frames);

    gsk_profiler_push_samples (self->profiler);
  }
}

void
gsk_gl_command_queue_begin_frame (GskGLCommandQueue *self)
{
  g_assert (GSK_IS_GL_COMMAND_QUEUE (self));
  g_assert (self->batches.len == 0);

  gsk_gl_command_queue_make_current (self);

  self->fbo_max = 0;
  self->tail_batch_index = -1;
  self->head_batch_index = -1;
  self->in_frame = TRUE;
}

/**
 * gsk_gl_command_queue_end_frame:
 * @self: a `GskGLCommandQueue`
 *
 * This function performs cleanup steps that need to be done after
 * a frame has finished. This is not performed as part of the command
 * queue execution to allow for the frame to be submitted as soon
 * as possible.
 *
 * However, it should be executed after the draw contexts end_frame
 * has been called to swap the OpenGL framebuffers.
 */
void
gsk_gl_command_queue_end_frame (GskGLCommandQueue *self)
{
  g_assert (GSK_IS_GL_COMMAND_QUEUE (self));

  gsk_gl_command_queue_make_current (self);
  gsk_gl_uniform_state_end_frame (self->uniforms);

  /* Reset attachments so we don't hold on to any textures
   * that might be released after the frame.
   */
  for (guint i = 0; i < G_N_ELEMENTS (self->attachments->textures); i++)
    {
      if (self->attachments->textures[i].id != 0)
        {
          glActiveTexture (GL_TEXTURE0 + i);
          glBindTexture (self->attachments->textures[i].target, 0);

          self->attachments->textures[i].id = 0;
          self->attachments->textures[i].changed = FALSE;
          self->attachments->textures[i].initial = TRUE;
        }
    }

  self->batches.len = 0;
  self->batch_binds.len = 0;
  self->batch_uniforms.len = 0;
  self->syncs.len = 0;
  self->n_uploads = 0;
  self->tail_batch_index = -1;
  self->in_frame = FALSE;
}

gboolean
gsk_gl_command_queue_create_render_target (GskGLCommandQueue *self,
                                           int                width,
                                           int                height,
                                           int                format,
                                           guint             *out_fbo_id,
                                           guint             *out_texture_id)
{
  GLuint fbo_id = 0;
  GLint texture_id;

  g_assert (GSK_IS_GL_COMMAND_QUEUE (self));
  g_assert (width > 0);
  g_assert (height > 0);
  g_assert (out_fbo_id != NULL);
  g_assert (out_texture_id != NULL);

  texture_id = gsk_gl_command_queue_create_texture (self,
                                                    width, height,
                                                    format);

  if (texture_id == -1)
    {
      *out_fbo_id = 0;
      *out_texture_id = 0;
      return FALSE;
    }

  fbo_id = gsk_gl_command_queue_create_framebuffer (self);

  glBindFramebuffer (GL_FRAMEBUFFER, fbo_id);
  glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0);
  g_assert_cmphex (glCheckFramebufferStatus (GL_FRAMEBUFFER), ==, GL_FRAMEBUFFER_COMPLETE);

  *out_fbo_id = fbo_id;
  *out_texture_id = texture_id;

  return TRUE;
}

int
gsk_gl_command_queue_create_texture (GskGLCommandQueue *self,
                                     int                width,
                                     int                height,
                                     int                format)
{
  GLuint texture_id = 0;

  g_assert (GSK_IS_GL_COMMAND_QUEUE (self));

  if G_UNLIKELY (self->max_texture_size == -1)
    glGetIntegerv (GL_MAX_TEXTURE_SIZE, &self->max_texture_size);

  if (width > self->max_texture_size || height > self->max_texture_size)
    return -1;

  glGenTextures (1, &texture_id);

  glActiveTexture (GL_TEXTURE0);
  glBindTexture (GL_TEXTURE_2D, texture_id);

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  switch (format)
  {
    case 0:
      break;
    case GL_RGBA8:
      glTexImage2D (GL_TEXTURE_2D, 0, format, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
      break;
    case GL_RGBA16F:
      glTexImage2D (GL_TEXTURE_2D, 0, format, width, height, 0, GL_RGBA, GL_HALF_FLOAT, NULL);
      break;
    case GL_RGBA32F:
      glTexImage2D (GL_TEXTURE_2D, 0, format, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
      break;
    default:
      /* If you add new formats, make sure to set the correct format and type here
       * so that GLES doesn't barf invalid operations at you.
       * Because it is very important that these 3 values match when data is set to
       * NULL, do you hear me?
       */
      g_assert_not_reached ();
      break;
  }

  /* Restore the previous texture if it was set */
  if (self->attachments->textures[0].id != 0 &&
      self->attachments->textures[0].target == GL_TEXTURE_2D)
    glBindTexture (self->attachments->textures[0].target, self->attachments->textures[0].id);

  return (int)texture_id;
}

guint
gsk_gl_command_queue_create_framebuffer (GskGLCommandQueue *self)
{
  GLuint fbo_id;

  g_assert (GSK_IS_GL_COMMAND_QUEUE (self));

  glGenFramebuffers (1, &fbo_id);

  return fbo_id;
}

static GdkMemoryFormat
memory_format_gl_format (GskGLCommandQueue *self,
                         GdkMemoryFormat    data_format,
                         gboolean           ensure_mipmap,
                         gboolean          *out_can_mipmap,
                         GLint             *gl_internalformat,
                         GLenum            *gl_format,
                         GLenum            *gl_type,
                         GLint              gl_swizzle[4])
{
  GdkGLMemoryFlags flags, required_flags;
  GdkMemoryFormat alt_format;
  const GdkMemoryFormat *fallbacks;
  gsize i;

  /* No support for straight formats yet */
  if (gdk_memory_format_alpha (data_format) == GDK_MEMORY_ALPHA_STRAIGHT)
    data_format = gdk_memory_format_get_premultiplied (data_format);

  required_flags = GDK_GL_FORMAT_USABLE | GDK_GL_FORMAT_FILTERABLE;
  if (ensure_mipmap)
    required_flags |= GDK_GL_FORMAT_RENDERABLE;

  /* First, try the format itself */
  flags = gdk_gl_context_get_format_flags (self->context, data_format);
  if ((flags & required_flags) == required_flags)
    {
      gdk_memory_format_gl_format (data_format,
                                   gdk_gl_context_get_use_es (self->context),
                                   gl_internalformat,
                                   gl_format,
                                   gl_type,
                                   gl_swizzle);
      *out_can_mipmap = (flags & GDK_GL_FORMAT_RENDERABLE) ? TRUE : FALSE;
      return data_format;
    }

  /* Second, try the potential RGBA format */
  if (gdk_memory_format_gl_rgba_format (data_format,
                                        gdk_gl_context_get_use_es (self->context),
                                        &alt_format,
                                        gl_internalformat,
                                        gl_format,
                                        gl_type,
                                        gl_swizzle))
    {
      flags = gdk_gl_context_get_format_flags (self->context, alt_format);
      if ((flags & required_flags) == required_flags)
        {
          *out_can_mipmap = (flags & GDK_GL_FORMAT_RENDERABLE) ? TRUE : FALSE;

          if (self->can_swizzle)
            return data_format;

          gdk_memory_format_gl_format (alt_format,
                                       gdk_gl_context_get_use_es (self->context),
                                       gl_internalformat,
                                       gl_format,
                                       gl_type,
                                       gl_swizzle);

          return alt_format;
        }
    }

  /* Next, try the fallbacks */
  fallbacks = gdk_memory_format_get_fallbacks (data_format);
  for (i = 0; fallbacks[i] != -1; i++)
    {
      flags = gdk_gl_context_get_format_flags (self->context, fallbacks[i]);
      if (((flags & required_flags) == required_flags))
        {
          gdk_memory_format_gl_format (fallbacks[i],
                                       gdk_gl_context_get_use_es (self->context),
                                       gl_internalformat,
                                       gl_format,
                                       gl_type,
                                       gl_swizzle);

          *out_can_mipmap = (flags & GDK_GL_FORMAT_RENDERABLE) ? TRUE : FALSE;
          return fallbacks[i];
        }
    }

  g_assert_not_reached ();

  return GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
}

static void
gsk_gl_command_queue_do_upload_texture_chunk (GskGLCommandQueue *self,
                                              GdkTexture        *texture,
                                              int                x,
                                              int                y,
                                              GdkMemoryFormat    data_format,
                                              GLenum             gl_format,
                                              GLenum             gl_type,
                                              GLint              gl_swizzle[4])

{
  G_GNUC_UNUSED gint64 start_time = GDK_PROFILER_CURRENT_TIME;
  const guchar *data;
  gsize stride;
  GBytes *bytes;
  GdkTextureDownloader downloader;
  int width, height;
  gsize bpp;

  width = gdk_texture_get_width (texture);
  height = gdk_texture_get_height (texture);

  if (GSK_DEBUG_CHECK (FALLBACK) &&
      data_format != gdk_texture_get_format (texture))
    {
      GEnumClass *enum_class = g_type_class_ref (GDK_TYPE_MEMORY_FORMAT);

      gdk_debug_message ("Unsupported format %s, converting on CPU to %s",
                         g_enum_get_value (enum_class, gdk_texture_get_format (texture))->value_nick,
                         g_enum_get_value (enum_class, data_format)->value_nick);

      g_type_class_unref (enum_class);
    }

  gdk_texture_downloader_init (&downloader, texture);
  gdk_texture_downloader_set_format (&downloader, data_format);
  bytes = gdk_texture_downloader_download_bytes (&downloader, &stride);
  gdk_texture_downloader_finish (&downloader);
  data = g_bytes_get_data (bytes, NULL);
  bpp = gdk_memory_format_bytes_per_pixel (data_format);

  if (gdk_profiler_is_running ())
    {
      gdk_profiler_end_markf (start_time,
                              "Download texture chunk",
                              "Tile %dx%d Size %dx%d", x, y, width, height);
      start_time = GDK_PROFILER_CURRENT_TIME;
    }

  glPixelStorei (GL_UNPACK_ALIGNMENT, gdk_memory_format_alignment (data_format));

  /* GL_UNPACK_ROW_LENGTH is available on desktop GL, OpenGL ES >= 3.0, or if
   * the GL_EXT_unpack_subimage extension for OpenGL ES 2.0 is available
   */
  if (stride == width * bpp)
    {
      glTexSubImage2D (GL_TEXTURE_2D, 0, x, y, width, height, gl_format, gl_type, data);
    }
  else if (stride % bpp == 0 && gdk_gl_context_has_feature (self->context, GDK_GL_FEATURE_UNPACK_SUBIMAGE))
    {
      glPixelStorei (GL_UNPACK_ROW_LENGTH, stride / bpp);

      glTexSubImage2D (GL_TEXTURE_2D, 0, x, y, width, height, gl_format, gl_type, data);

      glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);
    }
  else
    {
      for (int i = 0; i < height; i++)
        glTexSubImage2D (GL_TEXTURE_2D, 0, x, y + i, width, 1, gl_format, gl_type, data + (i * stride));
    }

  glPixelStorei (GL_UNPACK_ALIGNMENT, 4);

  /* Only apply swizzle if really needed, might not even be
   * supported if default values are set
   */
  if (gl_swizzle[0] != GL_RED || gl_swizzle[1] != GL_GREEN || gl_swizzle[2] != GL_BLUE || gl_swizzle[3] != GL_ALPHA)
    {
      /* Set each channel independently since GLES 3.0 doesn't support the iv method */
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, gl_swizzle[0]);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, gl_swizzle[1]);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, gl_swizzle[2]);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, gl_swizzle[3]);
    }

  g_bytes_unref (bytes);

  if (gdk_profiler_is_running ())
    gdk_profiler_end_markf (start_time,
                            "Upload texture chunk",
                            "Tile %dx%d Size %dx%d", x, y, width, height);
}

int
gsk_gl_command_queue_upload_texture_chunks (GskGLCommandQueue    *self,
                                            gboolean              ensure_mipmap,
                                            unsigned int          n_chunks,
                                            GskGLTextureChunk    *chunks,
                                            gboolean             *out_can_mipmap)
{
  G_GNUC_UNUSED gint64 start_time = GDK_PROFILER_CURRENT_TIME;
  int width, height;
  GdkMemoryFormat data_format;
  GLint gl_internalformat;
  GLenum gl_format;
  GLenum gl_type;
  GLint gl_swizzle[4];
  int texture_id;

  g_assert (GSK_IS_GL_COMMAND_QUEUE (self));

  width = height = 0;
  for (unsigned int i = 0; i < n_chunks; i++)
    {
      GskGLTextureChunk *c = &chunks[i];

      width = MAX (width, c->x + gdk_texture_get_width (c->texture));
      height = MAX (height, c->y + gdk_texture_get_height (c->texture));
    }

  if (width > self->max_texture_size || height > self->max_texture_size)
    {
      g_warning ("Attempt to create texture of size %ux%u but max size is %d. "
                 "Clipping will occur.",
                 width, height, self->max_texture_size);
      width = MIN (width, self->max_texture_size);
      height = MIN (height, self->max_texture_size);
    }

  texture_id = gsk_gl_command_queue_create_texture (self, width, height, 0);
  if (texture_id == -1)
    return texture_id;

  self->n_uploads++;

  /* Switch to texture0 as 2D. We'll restore it later. */
  glActiveTexture (GL_TEXTURE0);
  glBindTexture (GL_TEXTURE_2D, texture_id);

  /* Initialize the texture */
  data_format = gdk_texture_get_format (chunks[0].texture);
  data_format = memory_format_gl_format (self,
                                         data_format,
                                         ensure_mipmap,
                                         out_can_mipmap,
                                         &gl_internalformat,
                                         &gl_format,
                                         &gl_type,
                                         gl_swizzle);

  glTexImage2D (GL_TEXTURE_2D, 0, gl_internalformat, width, height, 0, gl_format, gl_type, NULL);

  for (unsigned int i = 0; i < n_chunks; i++)
    {
      GskGLTextureChunk *c = &chunks[i];
      gsk_gl_command_queue_do_upload_texture_chunk (self, c->texture, c->x, c->y, data_format, gl_format, gl_type, gl_swizzle);
    }

  /* Restore previous texture state if any */
  if (self->attachments->textures[0].id > 0)
    glBindTexture (self->attachments->textures[0].target,
                   self->attachments->textures[0].id);

  if (gdk_profiler_is_running ())
    gdk_profiler_add_markf (start_time, GDK_PROFILER_CURRENT_TIME-start_time,
                            "Upload Texture",
                            "Size %dx%d", width, height);

  return texture_id;
}

int
gsk_gl_command_queue_upload_texture (GskGLCommandQueue *self,
                                     GdkTexture        *texture,
                                     gboolean           ensure_mipmap,
                                     gboolean          *out_can_mipmap)
{
  return gsk_gl_command_queue_upload_texture_chunks (self,
                                                     ensure_mipmap,
                                                     1,
                                                     &(GskGLTextureChunk){ texture, 0, 0},
                                                     out_can_mipmap);
}

void
gsk_gl_command_queue_set_profiler (GskGLCommandQueue *self,
                                   GskProfiler       *profiler)
{
  g_assert (GSK_IS_GL_COMMAND_QUEUE (self));
  g_assert (GSK_IS_PROFILER (profiler));

  if (g_set_object (&self->profiler, profiler))
    {
      self->gl_profiler = gsk_gl_profiler_new (self->context);

      self->metrics.n_frames = gsk_profiler_add_counter (profiler, "frames", "Frames", FALSE);
      self->metrics.cpu_time = gsk_profiler_add_timer (profiler, "cpu-time", "CPU Time", FALSE, TRUE);
      self->metrics.gpu_time = gsk_profiler_add_timer (profiler, "gpu-time", "GPU Time", FALSE, TRUE);

      self->metrics.n_binds = gdk_profiler_define_int_counter ("attachments", "Number of texture attachments");
      self->metrics.n_fbos = gdk_profiler_define_int_counter ("fbos", "Number of framebuffers attached");
      self->metrics.n_uniforms = gdk_profiler_define_int_counter ("uniforms", "Number of uniforms changed");
      self->metrics.n_uploads = gdk_profiler_define_int_counter ("uploads", "Number of texture uploads");
      self->metrics.n_programs = gdk_profiler_define_int_counter ("programs", "Number of program changes");
      self->metrics.queue_depth = gdk_profiler_define_int_counter ("gl-queue-depth", "Depth of GL command batches");
    }
}
