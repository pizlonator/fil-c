/* gskgldriver.c
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

#include "gskgldriverprivate.h"

#include <gsk/gskdebugprivate.h>
#include <gsk/gskglshaderprivate.h>
#include <gsk/gskrendererprivate.h>

#include "gskglcommandqueueprivate.h"
#include "gskglcompilerprivate.h"
#include "gskglglyphlibraryprivate.h"
#include "gskgliconlibraryprivate.h"
#include "gskglprogramprivate.h"
#include "gskglshadowlibraryprivate.h"
#include "fp16private.h"

#include <gdk/gdkglcontextprivate.h>
#include <gdk/gdkdisplayprivate.h>
#include <gdk/gdkmemorytextureprivate.h>
#include <gdk/gdkmemoryformatprivate.h>
#include <gdk/gdkprofilerprivate.h>
#include <gdk/gdktextureprivate.h>

#include <gdk/gdkmemoryformatprivate.h>
#include <gdk/gdkdmabuftextureprivate.h>


G_DEFINE_TYPE (GskGLDriver, gsk_gl_driver, G_TYPE_OBJECT)

static guint
texture_key_hash (gconstpointer v)
{
  const GskTextureKey *k = (const GskTextureKey *)v;

  /* Optimize for 0..3 where 0 is the scaled out case. Usually
   * we'll be squarely on 1 or 2 for standard vs HiDPI. When rendering
   * to a texture scaled out like in node-editor, we might be < 1.
   */
  guint scale_x = floorf (k->scale_x);
  guint scale_y = floorf (k->scale_y);

  return GPOINTER_TO_SIZE (k->pointer) ^
    ((scale_x << 8) |
     (scale_y << 4) |
     k->pointer_is_child);
}

static gboolean
texture_key_equal (gconstpointer v1,
                   gconstpointer v2)
{
  const GskTextureKey *k1 = (const GskTextureKey *)v1;
  const GskTextureKey *k2 = (const GskTextureKey *)v2;

  return k1->pointer == k2->pointer &&
         k1->scale_x == k2->scale_x &&
         k1->scale_y == k2->scale_y &&
         k1->pointer_is_child == k2->pointer_is_child &&
         (!k1->pointer_is_child || memcmp (&k1->parent_rect, &k2->parent_rect, sizeof k1->parent_rect) == 0);
}

static void
remove_texture_key_for_id (GskGLDriver *self,
                           guint        texture_id)
{
  GskTextureKey *key;

  g_assert (GSK_IS_GL_DRIVER (self));
  g_assert (texture_id > 0);

  /* g_hash_table_remove() will cause @key to be freed */
  if (g_hash_table_steal_extended (self->texture_id_to_key,
                                   GUINT_TO_POINTER (texture_id),
                                   NULL,
                                   (gpointer *)&key))
    g_hash_table_remove (self->key_to_texture_id, key);
}

static void
gsk_gl_texture_destroyed (gpointer data)
{
  ((GskGLTexture *)data)->user = NULL;
}

static void
gsk_gl_driver_autorelease_texture (GskGLDriver *self,
                                   guint        texture_id)
{
  g_assert (GSK_IS_GL_DRIVER (self));

  g_array_append_val (self->texture_pool, texture_id);
}

static guint
gsk_gl_driver_collect_unused_textures (GskGLDriver *self,
                                       gint64       watermark)
{
  GHashTableIter iter;
  gpointer k, v;
  guint old_size;
  guint collected;

  g_assert (GSK_IS_GL_DRIVER (self));

  old_size = g_hash_table_size (self->textures);

  g_hash_table_iter_init (&iter, self->textures);
  while (g_hash_table_iter_next (&iter, &k, &v))
    {
      GskGLTexture *t = v;

      if (t->user || t->permanent)
        continue;

      if (t->last_used_in_frame <= watermark)
        {
          g_hash_table_iter_steal (&iter);

          g_assert (t->link.prev == NULL);
          g_assert (t->link.next == NULL);
          g_assert (t->link.data == t);

          remove_texture_key_for_id (self, t->texture_id);
          gsk_gl_driver_autorelease_texture (self, t->texture_id);
          t->texture_id = 0;
          gsk_gl_texture_free (t);
        }
    }

  collected = old_size - g_hash_table_size (self->textures);

  return collected;
}

static void
remove_program (gpointer data)
{
  GskGLProgram *program = data;

  g_assert (!program || GSK_IS_GL_PROGRAM (program));

  if (program != NULL)
    {
      gsk_gl_program_delete (program);
      g_object_unref (program);
    }
}

static void
gsk_gl_driver_shader_weak_cb (gpointer  data,
                              GObject  *where_object_was)
{
  GskGLDriver *self = data;

  g_assert (GSK_IS_GL_DRIVER (self));

  if (self->shader_cache != NULL)
    {
      if (self->command_queue != NULL)
        gsk_gl_command_queue_make_current (self->command_queue);

      g_hash_table_remove (self->shader_cache, where_object_was);
    }
}

G_GNUC_UNUSED G_GNUC_NULL_TERMINATED static inline GBytes *
join_sources (GBytes *first_bytes,
              ...)
{
  GByteArray *byte_array = g_byte_array_new ();
  GBytes *bytes = first_bytes;
  va_list args;

  va_start (args, first_bytes);
  while (bytes != NULL)
    {
      gsize len;
      const guint8 *data = g_bytes_get_data (bytes, &len);
      if (len > 0)
        g_byte_array_append (byte_array, data, len);
      g_bytes_unref (bytes);
      bytes = va_arg (args, GBytes *);
    }
  va_end (args);

  return g_byte_array_free_to_bytes (byte_array);
}

static void
gsk_gl_driver_dispose (GObject *object)
{
  GskGLDriver *self = (GskGLDriver *)object;

  g_assert (GSK_IS_GL_DRIVER (self));
  g_assert (self->in_frame == FALSE);

#define GSK_GL_NO_UNIFORMS
#define GSK_GL_SHADER_RESOURCE(name)
#define GSK_GL_SHADER_STRING(str)
#define GSK_GL_SHADER_SINGLE(name)
#define GSK_GL_SHADER_JOINED(kind, ...)
#define GSK_GL_ADD_UNIFORM(pos, KEY, name)
#define GSK_GL_DEFINE_PROGRAM(name, resource, uniforms) \
  GSK_GL_DELETE_PROGRAM(name);                          \
  GSK_GL_DELETE_PROGRAM(name ## _no_clip);              \
  GSK_GL_DELETE_PROGRAM(name ## _rect_clip);
#define GSK_GL_DEFINE_PROGRAM_NO_CLIP(name, resource, uniforms) \
  GSK_GL_DELETE_PROGRAM(name);
#define GSK_GL_DELETE_PROGRAM(name)                     \
  G_STMT_START {                                        \
    if (self->name)                                     \
      gsk_gl_program_delete (self->name);               \
    g_clear_object (&self->name);                       \
  } G_STMT_END;
# include "gskglprograms.defs"
#undef GSK_GL_NO_UNIFORMS
#undef GSK_GL_SHADER_RESOURCE
#undef GSK_GL_SHADER_STRING
#undef GSK_GL_SHADER_SINGLE
#undef GSK_GL_SHADER_JOINED
#undef GSK_GL_ADD_UNIFORM
#undef GSK_GL_DEFINE_PROGRAM
#undef GSK_GL_DEFINE_PROGRAM_NO_CLIP

  if (self->shader_cache != NULL)
    {
      GHashTableIter iter;
      gpointer k, v;

      g_hash_table_iter_init (&iter, self->shader_cache);
      while (g_hash_table_iter_next (&iter, &k, &v))
        {
          GskGLShader *shader = k;
          g_object_weak_unref (G_OBJECT (shader),
                               gsk_gl_driver_shader_weak_cb,
                               self);
          g_hash_table_iter_remove (&iter);
        }

      g_clear_pointer (&self->shader_cache, g_hash_table_unref);
    }

  if (self->command_queue != NULL)
    {
      gsk_gl_command_queue_make_current (self->command_queue);
      gsk_gl_driver_collect_unused_textures (self, 0);
      g_clear_object (&self->command_queue);
    }

  if (self->autorelease_framebuffers != NULL &&
      self->autorelease_framebuffers->len > 0)
    {
      glDeleteFramebuffers (self->autorelease_framebuffers->len,
                            (GLuint *)(gpointer)self->autorelease_framebuffers->data);
      self->autorelease_framebuffers->len = 0;
    }

  g_clear_object (&self->glyphs_library);
  g_clear_object (&self->icons_library);
  g_clear_object (&self->shadows_library);

  g_clear_pointer (&self->texture_pool, g_array_unref);

  g_clear_pointer (&self->autorelease_framebuffers, g_array_unref);
  g_clear_pointer (&self->key_to_texture_id, g_hash_table_unref);
  g_clear_pointer (&self->textures, g_hash_table_unref);
  g_clear_pointer (&self->key_to_texture_id, g_hash_table_unref);
  g_clear_pointer (&self->texture_id_to_key, g_hash_table_unref);
  g_clear_pointer (&self->render_targets, g_ptr_array_unref);
  g_clear_pointer (&self->shader_cache, g_hash_table_unref);

  g_clear_object (&self->command_queue);
  g_clear_object (&self->shared_command_queue);

  G_OBJECT_CLASS (gsk_gl_driver_parent_class)->dispose (object);
}

static void
gsk_gl_driver_class_init (GskGLDriverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gsk_gl_driver_dispose;
}

static void
gsk_gl_driver_init (GskGLDriver *self)
{
  self->autorelease_framebuffers = g_array_new (FALSE, FALSE, sizeof (guint));
  self->textures = g_hash_table_new_full (NULL, NULL, NULL,
                                          (GDestroyNotify)gsk_gl_texture_free);
  self->texture_id_to_key = g_hash_table_new (NULL, NULL);
  self->key_to_texture_id = g_hash_table_new_full (texture_key_hash,
                                                   texture_key_equal,
                                                   g_free,
                                                   NULL);
  self->shader_cache = g_hash_table_new_full (NULL, NULL, NULL, remove_program);
  self->texture_pool = g_array_new (FALSE, FALSE, sizeof (guint));
  self->render_targets = g_ptr_array_new ();
}

static gboolean
gsk_gl_driver_load_programs (GskGLDriver  *self,
                             GError      **error)
{
  GskGLCompiler *compiler;
  gboolean ret = FALSE;
  G_GNUC_UNUSED gint64 start_time = GDK_PROFILER_CURRENT_TIME;

  g_assert (GSK_IS_GL_DRIVER (self));
  g_assert (GSK_IS_GL_COMMAND_QUEUE (self->command_queue));

  compiler = gsk_gl_compiler_new (self, self->debug);

  /* Setup preambles that are shared by all shaders */
  gsk_gl_compiler_set_preamble_from_resource (compiler,
                                              GSK_GL_COMPILER_ALL,
                                              "/org/gtk/libgsk/gl/preamble.glsl");
  gsk_gl_compiler_set_preamble_from_resource (compiler,
                                              GSK_GL_COMPILER_VERTEX,
                                              "/org/gtk/libgsk/gl/preamble.vs.glsl");
  gsk_gl_compiler_set_preamble_from_resource (compiler,
                                              GSK_GL_COMPILER_FRAGMENT,
                                              "/org/gtk/libgsk/gl/preamble.fs.glsl");

  /* Setup attributes that are provided via VBO */
  gsk_gl_compiler_bind_attribute (compiler, "aPosition", 0);
  gsk_gl_compiler_bind_attribute (compiler, "aUv", 1);
  gsk_gl_compiler_bind_attribute (compiler, "aColor", 2);
  gsk_gl_compiler_bind_attribute (compiler, "aColor2", 3);

  /* Use XMacros to register all of our programs and their uniforms */
#define GSK_GL_NO_UNIFORMS
#define GSK_GL_SHADER_RESOURCE(name)                                                            \
  g_resources_lookup_data("/org/gtk/libgsk/gl/" name, 0, NULL)
#define GSK_GL_SHADER_STRING(str)                                                               \
  g_bytes_new_static(str, strlen(str))
#define GSK_GL_SHADER_SINGLE(bytes)                                                             \
  G_STMT_START {                                                                                \
    GBytes *b = bytes;                                                                          \
    gsk_gl_compiler_set_source (compiler, GSK_GL_COMPILER_ALL, b);                              \
    g_bytes_unref (b);                                                                          \
  } G_STMT_END;
#define GSK_GL_SHADER_JOINED(kind, ...)                                                         \
  G_STMT_START {                                                                                \
    GBytes *bytes = join_sources(__VA_ARGS__);                                                  \
    gsk_gl_compiler_set_source (compiler, GSK_GL_COMPILER_##kind, bytes);                       \
    g_bytes_unref (bytes);                                                                      \
  } G_STMT_END;
#define GSK_GL_ADD_UNIFORM(pos, KEY, name)                                                      \
  gsk_gl_program_add_uniform (program, #name, UNIFORM_##KEY);
#define GSK_GL_DEFINE_PROGRAM(name, sources, uniforms)                                          \
  gsk_gl_compiler_set_source (compiler, GSK_GL_COMPILER_VERTEX, NULL);                          \
  gsk_gl_compiler_set_source (compiler, GSK_GL_COMPILER_FRAGMENT, NULL);                        \
  sources                                                                                       \
  GSK_GL_COMPILE_PROGRAM(name ## _no_clip, uniforms, "#define NO_CLIP 1\n");                    \
  GSK_GL_COMPILE_PROGRAM(name ## _rect_clip, uniforms, "#define RECT_CLIP 1\n");                \
  GSK_GL_COMPILE_PROGRAM(name, uniforms, "");
#define GSK_GL_DEFINE_PROGRAM_NO_CLIP(name, sources, uniforms)                                  \
  gsk_gl_compiler_set_source (compiler, GSK_GL_COMPILER_VERTEX, NULL);                          \
  gsk_gl_compiler_set_source (compiler, GSK_GL_COMPILER_FRAGMENT, NULL);                        \
  sources                                                                                       \
  GSK_GL_COMPILE_PROGRAM(name, uniforms, "#define NO_CLIP 1\n");
#define GSK_GL_COMPILE_PROGRAM(name, uniforms, clip)                                            \
  G_STMT_START {                                                                                \
    GskGLProgram *program;                                                                      \
    gboolean have_alpha;                                                                        \
    gboolean have_source;                                                                       \
                                                                                                \
    if (!(program = gsk_gl_compiler_compile (compiler, #name, clip, error)))                    \
      goto failure;                                                                             \
                                                                                                \
    have_alpha = gsk_gl_program_add_uniform (program, "u_alpha", UNIFORM_SHARED_ALPHA);         \
    have_source = gsk_gl_program_add_uniform (program, "u_source", UNIFORM_SHARED_SOURCE);      \
    gsk_gl_program_add_uniform (program, "u_clip_rect", UNIFORM_SHARED_CLIP_RECT);              \
    gsk_gl_program_add_uniform (program, "u_viewport", UNIFORM_SHARED_VIEWPORT);                \
    gsk_gl_program_add_uniform (program, "u_projection", UNIFORM_SHARED_PROJECTION);            \
    gsk_gl_program_add_uniform (program, "u_modelview", UNIFORM_SHARED_MODELVIEW);              \
                                                                                                \
    uniforms                                                                                    \
                                                                                                \
    gsk_gl_program_uniforms_added (program, have_source);                                       \
    if (have_alpha)                                                                             \
      gsk_gl_program_set_uniform1f (program, UNIFORM_SHARED_ALPHA, 0, 1.0f);                    \
                                                                                                \
    *(GskGLProgram **)(((guint8 *)self) + G_STRUCT_OFFSET (GskGLDriver, name)) =                \
         g_steal_pointer (&program);                                                            \
  } G_STMT_END;
# include "gskglprograms.defs"
#undef GSK_GL_DEFINE_PROGRAM
#undef GSK_GL_DEFINE_PROGRAM_NO_CLIP
#undef GSK_GL_ADD_UNIFORM
#undef GSK_GL_SHADER_SINGLE
#undef GSK_GL_SHADER_JOINED
#undef GSK_GL_SHADER_RESOURCE
#undef GSK_GL_SHADER_STRING
#undef GSK_GL_NO_UNIFORMS

  ret = TRUE;

failure:
  g_clear_object (&compiler);

  gdk_profiler_end_mark (start_time, "Load GL programs", NULL);

  return ret;
}

/**
 * gsk_gl_driver_autorelease_framebuffer:
 * @self: a `GskGLDriver`
 * @framebuffer_id: the id of the OpenGL framebuffer
 *
 * Marks @framebuffer_id to be deleted when the current frame has cmopleted.
 */
static void
gsk_gl_driver_autorelease_framebuffer (GskGLDriver *self,
                                       guint        framebuffer_id)
{
  g_assert (GSK_IS_GL_DRIVER (self));

  g_array_append_val (self->autorelease_framebuffers, framebuffer_id);
}

static GskGLDriver *
gsk_gl_driver_new (GskGLCommandQueue  *command_queue,
                   gboolean            debug_shaders,
                   GError            **error)
{
  GskGLDriver *self;
  GdkGLContext *context;
  gint64 before G_GNUC_UNUSED;

  g_return_val_if_fail (GSK_IS_GL_COMMAND_QUEUE (command_queue), NULL);

  before = GDK_PROFILER_CURRENT_TIME;

  context = gsk_gl_command_queue_get_context (command_queue);

  gdk_gl_context_make_current (context);

  self = g_object_new (GSK_TYPE_GL_DRIVER, NULL);
  self->command_queue = g_object_ref (command_queue);
  self->shared_command_queue = g_object_ref (command_queue);
  self->debug = !!debug_shaders;

  if (!gsk_gl_driver_load_programs (self, error))
    {
      g_object_unref (self);
      return NULL;
    }

  self->glyphs_library = gsk_gl_glyph_library_new (self);
  self->icons_library = gsk_gl_icon_library_new (self);
  self->shadows_library = gsk_gl_shadow_library_new (self);

  gdk_profiler_end_mark (before, "Create GL driver", NULL);

  return g_steal_pointer (&self);
}

static void
free_driver (GskGLDriver *driver)
{
  g_object_run_dispose (G_OBJECT (driver));
  g_object_unref (driver);
}

static void
display_closed (GdkDisplay *display)
{
  g_object_set_data (G_OBJECT (display), "GSK_GL_DRIVER", NULL);
}

/**
 * gsk_gl_driver_for_display:
 * @display: A #GdkDisplay that is known to support GL
 * @debug_shaders: if debug information for shaders should be displayed
 * @error: location for error information
 *
 * Retrieves a driver for a shared display. Generally this is shared across all GL
 * contexts for a display so that fewer programs are necessary for driving output.
 *
 * Returns: (transfer full): a `GskGLDriver` if successful; otherwise %NULL and
 *   @error is set.
 */
GskGLDriver *
gsk_gl_driver_for_display (GdkDisplay  *display,
                           gboolean     debug_shaders,
                           GError     **error)
{
  GdkGLContext *context;
  GskGLCommandQueue *command_queue = NULL;
  GskGLDriver *driver;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  if ((driver = g_object_get_data (G_OBJECT (display), "GSK_GL_DRIVER")))
    return g_object_ref (driver);

  context = gdk_display_get_gl_context (display);
  g_assert (context);

  gdk_gl_context_make_current (context);

  /* Initially we create a command queue using the shared context. However,
   * as frames are processed this will be replaced with the command queue
   * for a given renderer. But since the programs are compiled into the
   * shared context, all other contexts sharing with it will have access
   * to those programs.
   */
  command_queue = gsk_gl_command_queue_new (context, NULL);

  if (!(driver = gsk_gl_driver_new (command_queue, debug_shaders, error)))
    goto failure;

  g_object_set_data_full (G_OBJECT (display),
                          "GSK_GL_DRIVER",
                          g_object_ref (driver),
                          (GDestroyNotify) free_driver);
  g_signal_connect (display, "closed", G_CALLBACK (display_closed), NULL);

failure:
  g_clear_object (&command_queue);

  return g_steal_pointer (&driver);
}

/**
 * gsk_gl_driver_begin_frame:
 * @self: a `GskGLDriver`
 * @command_queue: A `GskGLCommandQueue` from the renderer
 *
 * Begin a new frame.
 *
 * Texture atlases, pools, and other resources will be prepared to draw the
 * next frame. The command queue should be one that was created for the
 * target context to be drawn into (the context of the renderer's surface).
 */
void
gsk_gl_driver_begin_frame (GskGLDriver       *self,
                           GskGLCommandQueue *command_queue)
{
  gint64 last_frame_id;

  g_return_if_fail (GSK_IS_GL_DRIVER (self));
  g_return_if_fail (GSK_IS_GL_COMMAND_QUEUE (command_queue));
  g_return_if_fail (self->in_frame == FALSE);

  last_frame_id = self->current_frame_id;

  self->in_frame = TRUE;
  self->current_frame_id++;

  g_set_object (&self->command_queue, command_queue);

  gsk_gl_command_queue_begin_frame (self->command_queue);

  /* Mark unused pixel regions of the atlases */
  gsk_gl_texture_library_begin_frame (GSK_GL_TEXTURE_LIBRARY (self->icons_library),
                                      self->current_frame_id);
  gsk_gl_texture_library_begin_frame (GSK_GL_TEXTURE_LIBRARY (self->glyphs_library),
                                      self->current_frame_id);

  /* Cleanup old shadows */
  gsk_gl_shadow_library_begin_frame (self->shadows_library);

  /* Remove all textures that are from a previous frame or are no
   * longer used by linked GdkTexture. We do this at the beginning
   * of the following frame instead of the end so that we reduce chances
   * we block on any resources while delivering our frames.
   */
  gsk_gl_driver_collect_unused_textures (self, last_frame_id - 1);
}

/**
 * gsk_gl_driver_end_frame:
 * @self: a `GskGLDriver`
 *
 * Clean up resources from drawing the current frame.
 *
 * Temporary resources used while drawing will be released.
 */
void
gsk_gl_driver_end_frame (GskGLDriver *self)
{
  g_return_if_fail (GSK_IS_GL_DRIVER (self));
  g_return_if_fail (self->in_frame == TRUE);

  gsk_gl_command_queue_make_current (self->command_queue);
  gsk_gl_command_queue_end_frame (self->command_queue);

  self->in_frame = FALSE;
}

/**
 * gsk_gl_driver_after_frame:
 * @self: a `GskGLDriver`
 *
 * This function does post-frame cleanup operations.
 *
 * To reduce the chances of blocking on the driver it is performed
 * after the frame has swapped buffers.
 */
void
gsk_gl_driver_after_frame (GskGLDriver *self)
{
  g_return_if_fail (GSK_IS_GL_DRIVER (self));
  g_return_if_fail (self->in_frame == FALSE);

  /* Release any render targets (possibly adding them to
   * self->autorelease_framebuffers) so we can release the FBOs immediately
   * afterwards.
   */
  while (self->render_targets->len > 0)
    {
      GskGLRenderTarget *render_target = g_ptr_array_index (self->render_targets, self->render_targets->len - 1);

      gsk_gl_driver_autorelease_framebuffer (self, render_target->framebuffer_id);
      gsk_gl_driver_autorelease_texture (self, render_target->texture_id);
      g_free (render_target);

      self->render_targets->len--;
    }

  /* Now that we have collected render targets, release all the FBOs */
  if (self->autorelease_framebuffers->len > 0)
    {
      glDeleteFramebuffers (self->autorelease_framebuffers->len,
                            (GLuint *)(gpointer)self->autorelease_framebuffers->data);
      self->autorelease_framebuffers->len = 0;
    }

  /* Release any cached textures we used during the frame */
  if (self->texture_pool->len > 0)
    {
      glDeleteTextures (self->texture_pool->len,
                        (GLuint *)(gpointer)self->texture_pool->data);
      self->texture_pool->len = 0;
    }

  /* Reset command queue to our shared queue in case we have operations
   * that need to be processed outside of a frame (such as callbacks
   * from external systems such as GDK).
   */
  g_set_object (&self->command_queue, self->shared_command_queue);
}

GdkGLContext *
gsk_gl_driver_get_context (GskGLDriver *self)
{
  g_return_val_if_fail (GSK_IS_GL_DRIVER (self), NULL);
  g_return_val_if_fail (GSK_IS_GL_COMMAND_QUEUE (self->command_queue), NULL);

  return gsk_gl_command_queue_get_context (self->command_queue);
}

/**
 * gsk_gl_driver_cache_texture:
 * @self: a `GskGLDriver`
 * @key: the key for the texture
 * @texture_id: the id of the texture to be cached
 *
 * Inserts @texture_id into the texture cache using @key.
 *
 * Textures can be looked up by @key after calling this function using
 * gsk_gl_driver_lookup_texture().
 *
 * Textures that have not been used within a number of frames will be
 * purged from the texture cache automatically.
 */
void
gsk_gl_driver_cache_texture (GskGLDriver         *self,
                             const GskTextureKey *key,
                             guint                texture_id)
{
  g_assert (GSK_IS_GL_DRIVER (self));
  g_assert (key != NULL);
  g_assert (texture_id > 0);
  g_assert (g_hash_table_contains (self->textures, GUINT_TO_POINTER (texture_id)));

  if (!g_hash_table_contains (self->key_to_texture_id, key))
    {
      GskTextureKey *k;

      k = g_memdup (key, sizeof *key);

      g_assert (!g_hash_table_contains (self->texture_id_to_key, GUINT_TO_POINTER (texture_id)));
      g_hash_table_insert (self->key_to_texture_id, k, GUINT_TO_POINTER (texture_id));
      g_hash_table_insert (self->texture_id_to_key, GUINT_TO_POINTER (texture_id), k);
    }
}

#if defined(HAVE_DMABUF) && defined (HAVE_EGL)
static void
set_viewport_for_size (GskGLDriver  *self,
                       GskGLProgram *program,
                       float         width,
                       float         height)
{
  float viewport[4] = { 0, 0, width, height };

  gsk_gl_uniform_state_set4fv (program->uniforms,
                               program->program_info,
                               UNIFORM_SHARED_VIEWPORT, 0,
                               1,
                               (const float *)&viewport);
  self->stamps[UNIFORM_SHARED_VIEWPORT]++;
}

#define ORTHO_NEAR_PLANE   -10000
#define ORTHO_FAR_PLANE     10000

static void
set_projection_for_size (GskGLDriver  *self,
                         GskGLProgram *program,
                         float         width,
                         float         height)
{
  graphene_matrix_t projection;

  graphene_matrix_init_ortho (&projection, 0, width, 0, height, ORTHO_NEAR_PLANE, ORTHO_FAR_PLANE);
  graphene_matrix_scale (&projection, 1, -1, 1);

  gsk_gl_uniform_state_set_matrix (program->uniforms,
                                   program->program_info,
                                   UNIFORM_SHARED_PROJECTION, 0,
                                   &projection);
  self->stamps[UNIFORM_SHARED_PROJECTION]++;
}

static void
reset_modelview (GskGLDriver  *self,
                 GskGLProgram *program)
{
  graphene_matrix_t modelview;

  graphene_matrix_init_identity (&modelview);

  gsk_gl_uniform_state_set_matrix (program->uniforms,
                                   program->program_info,
                                   UNIFORM_SHARED_MODELVIEW, 0,
                                   &modelview);
  self->stamps[UNIFORM_SHARED_MODELVIEW]++;
}

static void
draw_rect (GskGLCommandQueue *command_queue,
           float              min_x,
           float              min_y,
           float              max_x,
           float              max_y)
{
  GskGLDrawVertex *vertices = gsk_gl_command_queue_add_vertices (command_queue);
  float min_u = 0;
  float max_u = 1;
  float min_v = 1;
  float max_v = 0;
  guint16 c = FP16_ZERO;

  vertices[0] = (GskGLDrawVertex) { .position = { min_x, min_y }, .uv = { min_u, min_v }, .color = { c, c, c, c } };
  vertices[1] = (GskGLDrawVertex) { .position = { min_x, max_y }, .uv = { min_u, max_v }, .color = { c, c, c, c } };
  vertices[2] = (GskGLDrawVertex) { .position = { max_x, min_y }, .uv = { max_u, min_v }, .color = { c, c, c, c } };
  vertices[3] = (GskGLDrawVertex) { .position = { max_x, max_y }, .uv = { max_u, max_v }, .color = { c, c, c, c } };
  vertices[4] = (GskGLDrawVertex) { .position = { min_x, max_y }, .uv = { min_u, max_v }, .color = { c, c, c, c } };
  vertices[5] = (GskGLDrawVertex) { .position = { max_x, min_y }, .uv = { max_u, min_v }, .color = { c, c, c, c } };
}

static unsigned int release_render_target (GskGLDriver       *self,
                                           GskGLRenderTarget *render_target,
                                           gboolean           release_texture,
                                           gboolean           cache_texture);

static guint
gsk_gl_driver_import_dmabuf_texture (GskGLDriver      *self,
                                     GdkDmabufTexture *texture)
{
  GdkGLContext *context = self->command_queue->context;
  int max_texture_size = self->command_queue->max_texture_size;
  const GdkDmabuf *dmabuf;
  guint texture_id;
  int width, height;
  GskGLProgram *program;
  GskGLRenderTarget *render_target;
  guint prev_fbo;
  gboolean external;
  GdkMemoryFormat format;
  gboolean premultiply;

  gdk_gl_context_make_current (context);

  width = gdk_texture_get_width (GDK_TEXTURE (texture));
  height = gdk_texture_get_height (GDK_TEXTURE (texture));

  if (width > max_texture_size || height > max_texture_size)
    {
      GDK_DISPLAY_DEBUG (gdk_gl_context_get_display (context), DMABUF,
                         "Can't import dmabuf bigger than MAX_TEXTURE_SIZE (%d)",
                         max_texture_size);
      return 0;
    }

  dmabuf = gdk_dmabuf_texture_get_dmabuf (texture);
  format = gdk_texture_get_format (GDK_TEXTURE (texture));
  premultiply = gdk_memory_format_alpha (format) == GDK_MEMORY_ALPHA_STRAIGHT;

  texture_id = gdk_gl_context_import_dmabuf (context,
                                             width, height,
                                             dmabuf,
                                             &external);
  if (texture_id == 0)
    return 0;

  if (!external && !premultiply)
    return texture_id;

  gsk_gl_driver_autorelease_texture (self, texture_id);

  if (external)
    program = self->external;
  else
    program = self->premultiply;

  if (!gsk_gl_driver_create_render_target (self, width, height, GL_RGBA8, &render_target))
    return texture_id;

  prev_fbo = gsk_gl_command_queue_bind_framebuffer (self->command_queue, render_target->framebuffer_id);
  gsk_gl_command_queue_clear (self->command_queue, 0, &GRAPHENE_RECT_INIT (0, 0, width, height));

  if (gsk_gl_command_queue_begin_draw (self->command_queue, program->program_info, width, height))
    {
      set_projection_for_size (self, program, width, height);
      set_viewport_for_size (self, program, width, height);
      reset_modelview (self, program);

      if (external)
        {
          gsk_gl_program_set_uniform_texture (program,
                                              UNIFORM_EXTERNAL_SOURCE, 0,
                                              GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE0, texture_id);

          gsk_gl_program_set_uniform1i (program, UNIFORM_PREMULTIPLY, 0, premultiply);
        }
      else
        {
          gsk_gl_program_set_uniform_texture (program,
                                              UNIFORM_SHARED_SOURCE, 0,
                                              GL_TEXTURE_2D, GL_TEXTURE0, texture_id);
        }

      draw_rect (self->command_queue, 0, 0, width, height);

      gsk_gl_command_queue_end_draw (self->command_queue);
    }

  gsk_gl_command_queue_bind_framebuffer (self->command_queue, prev_fbo);

  return release_render_target (self, render_target, FALSE, FALSE);
}

#else

static guint
gsk_gl_driver_import_dmabuf_texture (GskGLDriver      *self,
                                     GdkDmabufTexture *texture)
{
  return 0;
}

#endif /* HAVE_DMABUF && HAVE_EGL */

/**
 * gsk_gl_driver_load_texture:
 * @self: a `GdkTexture`
 * @texture: a `GdkTexture`
 * @ensure_mipmap: Mipmaps for this texture must exist for downscaling
 *
 * Loads a `GdkTexture` by uploading the contents to the GPU when
 * necessary. If @texture is a `GdkGLTexture`, it can be used without
 * uploading contents to the GPU.
 *
 * If the texture has already been uploaded and not yet released
 * from cache, this function returns that texture id without further
 * work.
 *
 * If the texture has not been used for a number of frames, it will
 * be removed from cache.
 *
 * There is no need to release the resulting texture identifier after
 * using it. It will be released automatically.
 *
 * Returns: a texture identifier
 */
guint
gsk_gl_driver_load_texture (GskGLDriver *self,
                            GdkTexture  *texture,
                            gboolean     ensure_mipmap)
{
  GdkGLContext *context;
  GdkMemoryTexture *downloaded_texture;
  GskGLTexture *t;
  guint texture_id;
  int height;
  int width;
  gboolean can_mipmap = FALSE;

  g_return_val_if_fail (GSK_IS_GL_DRIVER (self), 0);
  g_return_val_if_fail (GDK_IS_TEXTURE (texture), 0);
  g_return_val_if_fail (GSK_IS_GL_COMMAND_QUEUE (self->command_queue), 0);

  context = self->command_queue->context;

  texture_id = 0;
  downloaded_texture = NULL;

  t = gdk_texture_get_render_data (texture, self);
  if (t && t->texture_id)
    {
      if (ensure_mipmap && t->can_mipmap && !t->has_mipmap)
        {
          glBindTexture (GL_TEXTURE_2D, t->texture_id);
          glGenerateMipmap (GL_TEXTURE_2D);
          t->has_mipmap = TRUE;
        }

      if (!ensure_mipmap || t->has_mipmap)
        return t->texture_id;

      gdk_texture_clear_render_data (texture);
    }

  if (GDK_IS_DMABUF_TEXTURE (texture) && !ensure_mipmap)
    {
      texture_id = gsk_gl_driver_import_dmabuf_texture (self, GDK_DMABUF_TEXTURE (texture));
    }
  else if (GDK_IS_GL_TEXTURE (texture))
    {
      GdkGLTexture *gl_texture = (GdkGLTexture *) texture;
      GdkGLContext *texture_context = gdk_gl_texture_get_context (gl_texture);

      if (gdk_gl_context_is_shared (context, texture_context) &&
          (!ensure_mipmap || gdk_gl_texture_has_mipmap (gl_texture)) &&
          gdk_memory_format_alpha (gdk_texture_get_format (texture)) != GDK_MEMORY_ALPHA_STRAIGHT)
        {
          /* A GL texture from the same GL context is a simple task... */
          return gdk_gl_texture_get_id (gl_texture);
        }
    }

  if (texture_id == 0)
    {
      downloaded_texture = gdk_memory_texture_from_texture (texture, gdk_texture_get_format (texture));

      /* The download_texture() call may have switched the GL context. Make sure
       * the right context is at work again.
       */
      gdk_gl_context_make_current (context);

      texture_id = gsk_gl_command_queue_upload_texture (self->command_queue, GDK_TEXTURE (downloaded_texture), ensure_mipmap, &can_mipmap);
    }

  width = gdk_texture_get_width (texture);
  height = gdk_texture_get_height (texture);

  t = gsk_gl_texture_new (texture_id,
                          width, height,
                          self->current_frame_id);
  t->can_mipmap = can_mipmap;
  if (ensure_mipmap)
    {
      g_assert (can_mipmap);
      glBindTexture (GL_TEXTURE_2D, t->texture_id);
      glGenerateMipmap (GL_TEXTURE_2D);
      t->has_mipmap = TRUE;
    }

  g_hash_table_insert (self->textures, GUINT_TO_POINTER (texture_id), t);

  if (gdk_texture_set_render_data (texture, self, t, gsk_gl_texture_destroyed))
    t->user = texture;

  gdk_gl_context_label_object_printf (context, GL_TEXTURE, t->texture_id,
                                      "GdkTexture<%p> %d", texture, t->texture_id);

  g_clear_object (&downloaded_texture);

  return t->texture_id;
}

/**
 * gsk_gl_driver_create_texture:
 * @self: a `GskGLDriver`
 * @width: the width of the texture
 * @height: the height of the texture
 * @format: format for the texture
 *
 * Creates a new texture immediately that can be used by the caller
 * to upload data, map to a framebuffer, or other uses which may
 * modify the texture immediately.
 *
 * Typical examples for @format are GL_RGBA8, GL_RGBA16F or GL_RGBA32F.
 *
 * Use gsk_gl_driver_release_texture() to release this texture back into
 * the pool so it may be reused later in the pipeline.
 *
 * Returns: a `GskGLTexture` which can be returned to the pool with
 *   gsk_gl_driver_release_texture().
 */
GskGLTexture *
gsk_gl_driver_create_texture (GskGLDriver *self,
                              float        width,
                              float        height,
                              int          format)
{
  GskGLTexture *texture;
  guint texture_id;

  g_return_val_if_fail (GSK_IS_GL_DRIVER (self), NULL);

  texture_id = gsk_gl_command_queue_create_texture (self->command_queue,
                                                    width, height,
                                                    format);
  texture = gsk_gl_texture_new (texture_id,
                                width, height,
                                self->current_frame_id);
  g_hash_table_insert (self->textures,
                       GUINT_TO_POINTER (texture->texture_id),
                       texture);

  return texture;
}

/**
 * gsk_gl_driver_release_texture:
 * @self: a `GskGLDriver`
 * @texture: a `GskGLTexture`
 *
 * Releases @texture back into the pool so that it can be used later
 * in the command stream by future batches. This helps reduce VRAM
 * usage on the GPU.
 *
 * When the frame has completed, pooled textures will be released
 * to free additional VRAM back to the system.
 */
void
gsk_gl_driver_release_texture (GskGLDriver  *self,
                               GskGLTexture *texture)
{
  guint texture_id;

  g_assert (GSK_IS_GL_DRIVER (self));
  g_assert (texture != NULL);

  texture_id = texture->texture_id;
  texture->texture_id = 0;
  gsk_gl_texture_free (texture);

  if (texture_id > 0)
    remove_texture_key_for_id (self, texture_id);

  g_hash_table_steal (self->textures, GUINT_TO_POINTER (texture_id));
  gsk_gl_driver_autorelease_texture (self, texture_id);
}

/**
 * gsk_gl_driver_create_render_target:
 * @self: a `GskGLDriver`
 * @width: the width for the render target
 * @height: the height for the render target
 * @format: the format to use
 * @out_render_target: (out): a location for the render target
 *
 * Creates a new render target which contains a framebuffer and a texture
 * bound to that framebuffer of the size @width x @height and using the
 * appropriate filters.
 *
 * Typical examples for @format are GK_RGBA8, GL_RGBA16F or GL_RGBA32F.
 *
 * Use gsk_gl_driver_release_render_target() when you are finished with
 * the render target to release it. You may steal the texture from the
 * render target when releasing it.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @out_fbo_id and
 *   @out_texture_id are undefined.
 */
gboolean
gsk_gl_driver_create_render_target (GskGLDriver        *self,
                                    int                 width,
                                    int                 height,
                                    int                 format,
                                    GskGLRenderTarget **out_render_target)
{
  guint framebuffer_id;
  guint texture_id;

  g_return_val_if_fail (GSK_IS_GL_DRIVER (self), FALSE);
  g_return_val_if_fail (GSK_IS_GL_COMMAND_QUEUE (self->command_queue), FALSE);
  g_return_val_if_fail (out_render_target != NULL, FALSE);

#if 0
  if (self->render_targets->len > 0)
    {
      for (guint i = self->render_targets->len; i > 0; i--)
        {
          GskGLRenderTarget *render_target = g_ptr_array_index (self->render_targets, i-1);

          if (render_target->width == width &&
              render_target->height == height)
            {
              *out_render_target = g_ptr_array_steal_index_fast (self->render_targets, i-1);
              return TRUE;
            }
        }
    }
#endif

  if (gsk_gl_command_queue_create_render_target (self->command_queue,
                                                  width, height,
                                                  format,
                                                  &framebuffer_id, &texture_id))
    {
      GskGLRenderTarget *render_target;

      render_target = g_new0 (GskGLRenderTarget, 1);
      render_target->format = format;
      render_target->width = width;
      render_target->height = height;
      render_target->framebuffer_id = framebuffer_id;
      render_target->texture_id = texture_id;

      *out_render_target = render_target;

      return TRUE;
    }

  *out_render_target = NULL;

  return FALSE;
}

static unsigned int
release_render_target (GskGLDriver       *self,
                       GskGLRenderTarget *render_target,
                       gboolean           release_texture,
                       gboolean           cache_texture)
{
  guint texture_id;

  g_return_val_if_fail (GSK_IS_GL_DRIVER (self), 0);
  g_return_val_if_fail (render_target != NULL, 0);

  if (release_texture)
    {
      texture_id = 0;
      g_ptr_array_add (self->render_targets, render_target);
    }
  else
    {
      texture_id = render_target->texture_id;

      if (cache_texture)
        {
          GskGLTexture *texture;

          texture = gsk_gl_texture_new (render_target->texture_id,
                                        render_target->width,
                                        render_target->height,
                                        self->current_frame_id);
          g_hash_table_insert (self->textures,
                               GUINT_TO_POINTER (texture_id),
                               g_steal_pointer (&texture));
        }

      gsk_gl_driver_autorelease_framebuffer (self, render_target->framebuffer_id);
      g_free (render_target);

    }

  return texture_id;
}

/**
 * gsk_gl_driver_release_render_target:
 * @self: a `GskGLDriver`
 * @render_target: a `GskGLRenderTarget` created with
 *   gsk_gl_driver_create_render_target().
 * @release_texture: if the texture should also be released
 *
 * Releases a render target that was previously created. An attempt may
 * be made to cache the render target so that future creations of render
 * targets are performed faster.
 *
 * If @release_texture is %FALSE, the backing texture id is returned and
 * the framebuffer is released. Otherwise, both the texture and framebuffer
 * are released or cached until the end of the frame.
 *
 * This may be called when building the render job as the texture or
 * framebuffer will not be removed immediately.
 *
 * Returns: a texture id if @release_texture is %FALSE, otherwise zero.
 */
guint
gsk_gl_driver_release_render_target (GskGLDriver       *self,
                                     GskGLRenderTarget *render_target,
                                     gboolean           release_texture)
{
  return release_render_target (self, render_target, release_texture, TRUE);
}

/**
 * gsk_gl_driver_lookup_shader:
 * @self: a `GskGLDriver`
 * @shader: the shader to lookup or load
 * @error: a location for a `GError`
 *
 * Attempts to load @shader from the shader cache.
 *
 * If it has not been loaded, then it will compile the shader on demand.
 *
 * Returns: (nullable) (transfer none): a `GskGLShader`
 */
GskGLProgram *
gsk_gl_driver_lookup_shader (GskGLDriver  *self,
                             GskGLShader  *shader,
                             GError      **error)
{
  GskGLProgram *program;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (shader != NULL, NULL);

  program = g_hash_table_lookup (self->shader_cache, shader);

  if (program == NULL)
    {
      const GskGLUniform *uniforms;
      GskGLCompiler *compiler;
      GBytes *suffix;
      int n_required_textures;
      int n_uniforms;

      uniforms = gsk_gl_shader_get_uniforms (shader, &n_uniforms);
      if (n_uniforms > GSK_GL_PROGRAM_MAX_CUSTOM_ARGS)
        {
          g_set_error (error,
                       GDK_GL_ERROR,
                       GDK_GL_ERROR_UNSUPPORTED_FORMAT,
                       "Tried to use %d uniforms, while only %d is supported",
                       n_uniforms,
                       GSK_GL_PROGRAM_MAX_CUSTOM_ARGS);
          return NULL;
        }

      n_required_textures = gsk_gl_shader_get_n_textures (shader);
      if (n_required_textures > GSK_GL_PROGRAM_MAX_CUSTOM_TEXTURES)
        {
          g_set_error (error,
                       GDK_GL_ERROR,
                       GDK_GL_ERROR_UNSUPPORTED_FORMAT,
                       "Tried to use %d textures, while only %d is supported",
                       n_required_textures,
                       GSK_GL_PROGRAM_MAX_CUSTOM_TEXTURES);
          return NULL;
        }

      compiler = gsk_gl_compiler_new (self, FALSE);
      suffix = gsk_gl_shader_get_source (shader);

      gsk_gl_compiler_set_preamble_from_resource (compiler,
                                                  GSK_GL_COMPILER_ALL,
                                                  "/org/gtk/libgsk/gl/preamble.glsl");
      gsk_gl_compiler_set_preamble_from_resource (compiler,
                                                  GSK_GL_COMPILER_VERTEX,
                                                  "/org/gtk/libgsk/gl/preamble.vs.glsl");
      gsk_gl_compiler_set_preamble_from_resource (compiler,
                                                  GSK_GL_COMPILER_FRAGMENT,
                                                  "/org/gtk/libgsk/gl/preamble.fs.glsl");
      gsk_gl_compiler_set_source_from_resource (compiler,
                                                GSK_GL_COMPILER_ALL,
                                                "/org/gtk/libgsk/gl/custom.glsl");
      gsk_gl_compiler_set_suffix (compiler, GSK_GL_COMPILER_FRAGMENT, suffix);

      /* Setup attributes that are provided via VBO */
      gsk_gl_compiler_bind_attribute (compiler, "aPosition", 0);
      gsk_gl_compiler_bind_attribute (compiler, "aUv", 1);
      gsk_gl_compiler_bind_attribute (compiler, "aColor", 2);
      gsk_gl_compiler_bind_attribute (compiler, "aColor2", 3);

      if ((program = gsk_gl_compiler_compile (compiler, NULL, "", error)))
        {
          gboolean have_alpha;

          gsk_gl_program_add_uniform (program, "u_source", UNIFORM_SHARED_SOURCE);
          gsk_gl_program_add_uniform (program, "u_clip_rect", UNIFORM_SHARED_CLIP_RECT);
          gsk_gl_program_add_uniform (program, "u_viewport", UNIFORM_SHARED_VIEWPORT);
          gsk_gl_program_add_uniform (program, "u_projection", UNIFORM_SHARED_PROJECTION);
          gsk_gl_program_add_uniform (program, "u_modelview", UNIFORM_SHARED_MODELVIEW);
          have_alpha = gsk_gl_program_add_uniform (program, "u_alpha", UNIFORM_SHARED_ALPHA);

          gsk_gl_program_add_uniform (program, "u_size", UNIFORM_CUSTOM_SIZE);
          gsk_gl_program_add_uniform (program, "u_texture1", UNIFORM_CUSTOM_TEXTURE1);
          gsk_gl_program_add_uniform (program, "u_texture2", UNIFORM_CUSTOM_TEXTURE2);
          gsk_gl_program_add_uniform (program, "u_texture3", UNIFORM_CUSTOM_TEXTURE3);
          gsk_gl_program_add_uniform (program, "u_texture4", UNIFORM_CUSTOM_TEXTURE4);

          /* Custom arguments (max is 8) */
          for (guint i = 0; i < n_uniforms; i++)
            gsk_gl_program_add_uniform (program, uniforms[i].name, UNIFORM_CUSTOM_ARG0+i);

          gsk_gl_program_uniforms_added (program, TRUE);

          if (have_alpha)
            gsk_gl_program_set_uniform1f (program, UNIFORM_SHARED_ALPHA, 0, 1.0f);

          g_hash_table_insert (self->shader_cache, shader, program);
          g_object_weak_ref (G_OBJECT (shader),
                             gsk_gl_driver_shader_weak_cb,
                             self);
        }

      g_object_unref (compiler);
    }

  return program;
}

#if 0
void
gsk_gl_driver_save_texture_to_png (GskGLDriver       *driver,
                                   int                texture_id,
                                   int                width,
                                   int                height,
                                   const char        *filename)
{
  GdkGLTextureBuilder *builder;
  GdkTexture *texture;

  builder = gdk_gl_texture_builder_new ();
  gdk_gl_texture_builder_set_context (builder, gsk_gl_driver_get_context (driver));
  gdk_gl_texture_builder_set_id (builder, texture_id);
  gdk_gl_texture_builder_set_width (builder, width);
  gdk_gl_texture_builder_set_height (builder, height);

  texture = gdk_gl_texture_builder_build (builder, NULL, NULL);
  gdk_texture_save_to_png (texture, filename);

  g_object_unref (texture);
  g_object_unref (builder);
}

void
gsk_gl_driver_save_atlases_to_png (GskGLDriver *self,
                                   const char  *directory)
{
  GPtrArray *atlases;

  g_return_if_fail (GSK_IS_GL_DRIVER (self));

  if (directory == NULL)
    directory = ".";

#define copy_atlases(dst, library) \
  g_ptr_array_extend(dst, GSK_GL_TEXTURE_LIBRARY(library)->atlases, NULL, NULL)
  atlases = g_ptr_array_new ();
  copy_atlases (atlases, self->glyphs_library);
  copy_atlases (atlases, self->icons_library);
#undef copy_atlases

  for (guint i = 0; i < atlases->len; i++)
    {
      GskGLTextureAtlas *atlas = g_ptr_array_index (atlases, i);
      char *filename = g_strdup_printf ("%s%sframe-%d-atlas-%d.png",
                                        directory,
                                        G_DIR_SEPARATOR_S,
                                        (int)self->current_frame_id,
                                        atlas->texture_id);
      gsk_gl_driver_save_texture_to_png (self, atlas->texture_id, atlas->width, atlas->height, filename);
      g_free (filename);
    }

  g_ptr_array_unref (atlases);
}
#endif

GskGLCommandQueue *
gsk_gl_driver_create_command_queue (GskGLDriver *self,
                                     GdkGLContext *context)
{
  g_return_val_if_fail (GSK_IS_GL_DRIVER (self), NULL);
  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), NULL);

  return gsk_gl_command_queue_new (context, self->shared_command_queue->uniforms);
}

void
gsk_gl_driver_add_texture_slices (GskGLDriver        *self,
                                  GdkTexture         *texture,
                                  gboolean            ensure_mipmap,
                                  GskGLTextureSlice **out_slices,
                                  guint              *out_n_slices)
{
  int max_slice_size;
  GskGLTextureSlice *slices;
  GskGLTexture *t;
  guint n_slices;
  guint cols;
  guint rows;
  int tex_width;
  int tex_height;
  int x = 0, y = 0;
  GdkMemoryTexture *memtex;
  GdkMemoryFormat format;
  gsize bpp;
  int extra_pixels;
  GdkMemoryTexture *memtex1 = NULL;
  GdkMemoryTexture *memtex2 = NULL;
  GdkMemoryTexture *memtex3 = NULL;
  GdkMemoryTexture *memtex4 = NULL;
  gboolean can_mipmap = TRUE, slice_can_mipmap;

  g_assert (GSK_IS_GL_DRIVER (self));
  g_assert (GDK_IS_TEXTURE (texture));
  g_assert (out_slices != NULL);
  g_assert (out_n_slices != NULL);

  max_slice_size = self->command_queue->max_texture_size / 2;
  tex_width = texture->width;
  tex_height = texture->height;

  cols = (texture->width / max_slice_size) + 1;
  rows = (texture->height / max_slice_size) + 1;

  n_slices = cols * rows;

  t = gdk_texture_get_render_data (texture, self);

  if (t)
    {
      if (t->n_slices == n_slices &&
          (t->has_mipmap || !ensure_mipmap))
        {
          *out_slices = t->slices;
          *out_n_slices = t->n_slices;
          return;
        }

      gdk_texture_clear_render_data (texture);
    }

  slices = g_new0 (GskGLTextureSlice, n_slices);
  memtex = gdk_memory_texture_from_texture (texture,
                                            gdk_texture_get_format (texture));

  if (ensure_mipmap)
    {
      guchar *data1, *data2, *data3, *data4;
      guchar *top_row, *bot_row, *left_row, *right_row;
      GdkTexture *tmp;
      int w;
      GBytes *bytes;

      /* We need some extra pixels around our tiles, in order for
       * GL to properly determine the right level of detail to use.
       * This number should probably depend on the scale, but for
       * now we just hardcode it.
       *
       * We create some auxiliary textures to hold the extra pixels:
       *
       *    +---------------------+
       *    | memtex1             |
       *    *---+-------------+---+
       *    |   |             |   |
       * memtex2|   memtex    |memtex3
       *    |   |             |   |
       *    *---+-------------+---+
       *    | memtex4         |   |
       *    +---------------------+
       */

      extra_pixels = 15;

      top_row = g_malloc (4 * tex_width);
      tmp = gdk_memory_texture_new_subtexture (memtex, 0, 0, tex_width, 1);
      gdk_texture_download (tmp, top_row, 4 * tex_width);
      g_object_unref (tmp);

      bot_row = g_malloc (4 * tex_width);
      tmp = gdk_memory_texture_new_subtexture (memtex, 0, tex_height - 1, tex_width, 1);
      gdk_texture_download (tmp, bot_row, 4 * tex_width);
      g_object_unref (tmp);

      left_row = g_malloc (4 * tex_height);
      tmp = gdk_memory_texture_new_subtexture (memtex, 0, 0, 1, tex_height);
      gdk_texture_download (tmp, left_row, 4);
      g_object_unref (tmp);

      right_row = g_malloc (4 * tex_height);
      tmp = gdk_memory_texture_new_subtexture (memtex, tex_width - 1, 0, 1, tex_height);
      gdk_texture_download (tmp, right_row, 4);
      g_object_unref (tmp);

      w = tex_width + 2 * extra_pixels;

      data1 = g_malloc (4 * w * extra_pixels);
      data2 = g_malloc (4 * extra_pixels * tex_height);
      data3 = g_malloc (4 * extra_pixels * tex_height);
      data4 = g_malloc (4 * w * extra_pixels);

      format = gdk_texture_get_format (GDK_TEXTURE (memtex));
      bpp = gdk_memory_format_bytes_per_pixel (format);

      for (int i = 0; i < w; i++)
        {
          int ii = CLAMP (i, extra_pixels, (tex_width - 1) + extra_pixels) - extra_pixels;

          for (int j = 0; j < extra_pixels; j++)
            {
              for (int k = 0; k < bpp; k++)
                {
                  data1[(j * w + i) * 4 + k] = top_row[ii * 4 + k];
                  data4[(j * w + i) * 4 + k] = bot_row[ii * 4 + k];
                }
            }
        }

      for (int i = 0; i < extra_pixels; i++)
        {
          for (int j = 0; j < tex_height; j++)
            {
              for (int k = 0; k < bpp; k++)
                {
                  data2[(j * extra_pixels + i) * 4 + k] = left_row[j * 4 + k];
                  data3[(j * extra_pixels + i) * 4 + k] = right_row[j * 4 + k];
                }
            }
        }

      g_free (top_row);
      g_free (bot_row);
      g_free (left_row);
      g_free (right_row);

      bytes = g_bytes_new_take (data1, 4 * w * extra_pixels);
      memtex1 = GDK_MEMORY_TEXTURE (gdk_memory_texture_new (w, extra_pixels, format, bytes, 4 * w));
      g_bytes_unref (bytes);

      bytes = g_bytes_new_take (data2, 4 * extra_pixels * tex_height);
      memtex2 = GDK_MEMORY_TEXTURE (gdk_memory_texture_new (extra_pixels, tex_height, format, bytes, 4 * extra_pixels));
      g_bytes_unref (bytes);

      bytes = g_bytes_new_take (data3, 4 * extra_pixels * tex_height);
      memtex3 = GDK_MEMORY_TEXTURE (gdk_memory_texture_new (extra_pixels, tex_height, format, bytes, 4 * extra_pixels));
      g_bytes_unref (bytes);

      bytes = g_bytes_new_take (data4, 4 * w * extra_pixels);
      memtex4 = GDK_MEMORY_TEXTURE (gdk_memory_texture_new (w, extra_pixels, format, bytes, 4 * w));
      g_bytes_unref (bytes);
    }
  else
    extra_pixels = 0;

  x = 0;
  for (guint col = 0; col < cols; col++)
    {
      int slice_width = col + 1 < cols ? tex_width / cols : tex_width - x;

      y = 0;
      for (guint row = 0; row < rows; row++)
        {
          int slice_height = row + 1 < rows ? tex_height / rows : tex_height - y;
          int slice_index = (col * rows) + row;
          guint texture_id;

          if (ensure_mipmap)
            {
              GskGLTextureChunk chunks[5];
              unsigned int n_chunks = 0;

              if (row == 0)
                {
                  chunks[n_chunks].texture = gdk_memory_texture_new_subtexture (memtex1,
                                                                                x, 0,
                                                                                slice_width + 2 * extra_pixels, extra_pixels);
                  chunks[n_chunks].x = 0;
                  chunks[n_chunks].y = 0;
                  n_chunks++;
                }

              if (row == rows - 1)
                {
                  chunks[n_chunks].texture = gdk_memory_texture_new_subtexture (memtex4,
                                                                                x, 0,
                                                                                slice_width + 2 * extra_pixels, extra_pixels);
                  chunks[n_chunks].x = 0;
                  chunks[n_chunks].y = slice_height + extra_pixels;
                  n_chunks++;
                }

              if (col == 0)
                {
                  int yy = y - extra_pixels;
                  int hh = slice_height + 2 * extra_pixels;
                  int y0 = 0;

                  if (row == 0)
                    {
                      yy = 0;
                      y0 = extra_pixels;
                      hh -= extra_pixels;
                    }
                  if (row == rows - 1)
                    {
                      hh -= extra_pixels;
                    }

                  chunks[n_chunks].texture = gdk_memory_texture_new_subtexture (memtex2,
                                                                                0, yy, extra_pixels, hh);
                  chunks[n_chunks].x = 0;
                  chunks[n_chunks].y = y0;
                  n_chunks++;
                }

              if (col == cols - 1)
                {
                  int yy = y - extra_pixels;
                  int hh = slice_height + 2 * extra_pixels;
                  int y0 = 0;

                  if (row == 0)
                    {
                      yy = 0;
                      y0 = extra_pixels;
                      hh -= extra_pixels;
                    }
                  if (row == rows - 1)
                    {
                      hh -= extra_pixels;
                    }

                  chunks[n_chunks].texture = gdk_memory_texture_new_subtexture (memtex3,
                                                                                0, yy, extra_pixels, hh);
                  chunks[n_chunks].x = slice_width + extra_pixels;
                  chunks[n_chunks].y = y0;
                  n_chunks++;
                }

              {
                int xx = x - extra_pixels;
                int yy = y - extra_pixels;
                int ww = slice_width + 2 * extra_pixels;
                int hh = slice_height + 2 * extra_pixels;
                int x0 = 0;
                int y0 = 0;
                if (col == 0)
                  {
                    xx = 0;
                    ww -= extra_pixels;
                    x0 = extra_pixels;
                  }
                if (col == cols - 1)
                  {
                    ww -= extra_pixels;
                  }
                if (row == 0)
                  {
                    yy = 0;
                    hh -= extra_pixels;
                    y0 = extra_pixels;
                  }
                if (row == rows - 1)
                  {
                    hh -= extra_pixels;
                  }

                chunks[n_chunks].texture = gdk_memory_texture_new_subtexture (memtex, xx, yy, ww, hh);
                chunks[n_chunks].x = x0;
                chunks[n_chunks].y = y0;
                n_chunks++;
              }

              texture_id = gsk_gl_command_queue_upload_texture_chunks (self->command_queue, TRUE, n_chunks, chunks, &slice_can_mipmap);

              glBindTexture (GL_TEXTURE_2D, texture_id);
              glGenerateMipmap (GL_TEXTURE_2D);

              for (unsigned int i = 0; i < n_chunks; i++)
                g_object_unref (chunks[i].texture);
            }
          else
            {
              GdkTexture *subtex;

              subtex = gdk_memory_texture_new_subtexture (memtex, x, y, slice_width, slice_height);
              texture_id = gsk_gl_command_queue_upload_texture (self->command_queue, subtex, FALSE, &slice_can_mipmap);
              g_object_unref (subtex);
            }

          can_mipmap &= slice_can_mipmap;

          slices[slice_index].rect.x = x;
          slices[slice_index].rect.y = y;
          slices[slice_index].rect.width = slice_width;
          slices[slice_index].rect.height = slice_height;
          slices[slice_index].texture_id = texture_id;
          slices[slice_index].area.x = extra_pixels / (float) (slice_width + 2 * extra_pixels);
          slices[slice_index].area.y = extra_pixels / (float) (slice_height + 2 * extra_pixels);
          slices[slice_index].area.x2 = (extra_pixels + slice_width) / (float) (slice_width + 2 * extra_pixels);
          slices[slice_index].area.y2 = (extra_pixels + slice_height) / (float) (slice_height + 2 * extra_pixels);

          y += slice_height;
        }

      x += slice_width;
    }

  g_object_unref (memtex);
  g_clear_object (&memtex1);
  g_clear_object (&memtex2);
  g_clear_object (&memtex3);
  g_clear_object (&memtex4);

  /* Allocate one Texture for the entire thing. */
  t = gsk_gl_texture_new (0,
                          tex_width, tex_height,
                          self->current_frame_id);
  t->can_mipmap = can_mipmap;
  t->has_mipmap = ensure_mipmap;

  /* Use gsk_gl_texture_free() as destroy notify here since we are
   * not inserting this GskGLTexture into self->textures!
   */
  gdk_texture_set_render_data (texture, self, t,
                               (GDestroyNotify)gsk_gl_texture_free);

  t->slices = *out_slices = slices;
  t->n_slices = *out_n_slices = n_slices;
}

GskGLTexture *
gsk_gl_driver_mark_texture_permanent (GskGLDriver *self,
                                      guint        texture_id)
{
  GskGLTexture *t;

  g_return_val_if_fail (GSK_IS_GL_DRIVER (self), NULL);
  g_return_val_if_fail (texture_id > 0, NULL);

  if ((t = g_hash_table_lookup (self->textures, GUINT_TO_POINTER (texture_id))))
    t->permanent = TRUE;

  return t;
}

void
gsk_gl_driver_release_texture_by_id (GskGLDriver *self,
                                     guint        texture_id)
{
  GskGLTexture *texture;

  g_return_if_fail (GSK_IS_GL_DRIVER (self));
  g_return_if_fail (texture_id > 0);

  remove_texture_key_for_id (self, texture_id);

  if ((texture = g_hash_table_lookup (self->textures, GUINT_TO_POINTER (texture_id))))
    gsk_gl_driver_release_texture (self, texture);
}

typedef struct _GskGLTextureState
{
  GdkGLContext *context;
  GLuint        texture_id;
  GLsync        sync;
} GskGLTextureState;

static void
create_texture_from_texture_destroy (gpointer data)
{
  GskGLTextureState *state = data;

  g_assert (state != NULL);
  g_assert (GDK_IS_GL_CONTEXT (state->context));

  gdk_gl_context_make_current (state->context);
  glDeleteTextures (1, &state->texture_id);
  if (state->sync)
    glDeleteSync (state->sync);
  g_clear_object (&state->context);
  g_free (state);
}

GdkTexture *
gsk_gl_driver_create_gdk_texture (GskGLDriver     *self,
                                  guint            texture_id,
                                  GdkMemoryFormat  format)
{
  GskGLTextureState *state;
  GdkGLTextureBuilder *builder;
  GskGLTexture *texture;
  GdkTexture *result;

  g_return_val_if_fail (GSK_IS_GL_DRIVER (self), NULL);
  g_return_val_if_fail (self->command_queue != NULL, NULL);
  g_return_val_if_fail (GDK_IS_GL_CONTEXT (self->command_queue->context), NULL);
  g_return_val_if_fail (texture_id > 0, NULL);
  g_return_val_if_fail (!g_hash_table_contains (self->texture_id_to_key, GUINT_TO_POINTER (texture_id)), NULL);

  /* We must be tracking this texture_id already to use it */
  if (!(texture = g_hash_table_lookup (self->textures, GUINT_TO_POINTER (texture_id))))
    g_return_val_if_reached (NULL);

  state = g_new0 (GskGLTextureState, 1);
  state->texture_id = texture_id;
  state->context = g_object_ref (self->command_queue->context);
  if (gdk_gl_context_has_feature (self->command_queue->context, GDK_GL_FEATURE_SYNC))
    state->sync = glFenceSync (GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

  g_hash_table_steal (self->textures, GUINT_TO_POINTER (texture_id));

  builder = gdk_gl_texture_builder_new ();
  gdk_gl_texture_builder_set_context (builder, self->command_queue->context);
  gdk_gl_texture_builder_set_id (builder, texture_id);
  gdk_gl_texture_builder_set_format (builder, format);
  gdk_gl_texture_builder_set_width (builder, texture->width);
  gdk_gl_texture_builder_set_height (builder, texture->height);
  gdk_gl_texture_builder_set_sync (builder, state->sync);

  result = gdk_gl_texture_builder_build (builder,
                                         create_texture_from_texture_destroy,
                                         state);

  texture->texture_id = 0;
  gsk_gl_texture_free (texture);
  g_object_unref (builder);

  return result;
}
