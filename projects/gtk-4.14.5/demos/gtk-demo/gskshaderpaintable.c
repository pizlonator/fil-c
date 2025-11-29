/*
 * Copyright © 2020 Red Hat, Inc
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include <gtk/gtk.h>
#include "gskshaderpaintable.h"

/**
 * GskShaderPaintable:
 *
 * `GskShaderPaintable` is an implementation of the `GdkPaintable` interface
 * that uses a `GskGLShader` to create pixels.
 *
 * You can set the uniform data that the shader needs for rendering
 * using gsk_shader_paintable_set_args(). This function can
 * be called repeatedly to change the uniform data for the next
 * snapshot.
 *
 * Commonly, time is passed to shaders as a float uniform containing
 * the elapsed time in seconds. The convenience API
 * gsk_shader_paintable_update_time() can be called from a `GtkTickCallback`
 * to update the time based on the frame time of the frame clock.
 */


struct _GskShaderPaintable
{
  GObject parent_instance;

  GskGLShader *shader;
  GBytes *args;

  gint64 start_time;
};

struct _GskShaderPaintableClass
{
  GObjectClass parent_class;
};

enum {
  PROP_0,
  PROP_SHADER,
  PROP_ARGS,

  N_PROPS,
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gsk_shader_paintable_paintable_snapshot (GdkPaintable *paintable,
                                         GdkSnapshot  *snapshot,
                                         double        width,
                                         double        height)
{
  GskShaderPaintable *self = GSK_SHADER_PAINTABLE (paintable);

  gtk_snapshot_push_gl_shader (snapshot, self->shader, &GRAPHENE_RECT_INIT(0, 0, width, height), g_bytes_ref (self->args));
  gtk_snapshot_pop (snapshot);
}

static void
gsk_shader_paintable_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gsk_shader_paintable_paintable_snapshot;
}

G_DEFINE_TYPE_EXTENDED (GskShaderPaintable, gsk_shader_paintable, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                               gsk_shader_paintable_paintable_init))

static void
gsk_shader_paintable_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)

{
  GskShaderPaintable *self = GSK_SHADER_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_SHADER:
      gsk_shader_paintable_set_shader (self, g_value_get_object (value));
      break;

    case PROP_ARGS:
      gsk_shader_paintable_set_args (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gsk_shader_paintable_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GskShaderPaintable *self = GSK_SHADER_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_SHADER:
      g_value_set_object (value, self->shader);
      break;

    case PROP_ARGS:
      g_value_set_boxed (value, self->args);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gsk_shader_paintable_finalize (GObject *object)
{
  GskShaderPaintable *self = GSK_SHADER_PAINTABLE (object);

  g_clear_pointer (&self->args, g_bytes_unref);
  g_clear_object (&self->shader);

  G_OBJECT_CLASS (gsk_shader_paintable_parent_class)->finalize (object);
}

static void
gsk_shader_paintable_class_init (GskShaderPaintableClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = gsk_shader_paintable_get_property;
  gobject_class->set_property = gsk_shader_paintable_set_property;
  gobject_class->finalize = gsk_shader_paintable_finalize;

  properties[PROP_SHADER] =
    g_param_spec_object ("shader", "Shader", "The shader",
                         GSK_TYPE_GL_SHADER,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_ARGS] =
    g_param_spec_boxed ("args", "Arguments", "The uniform arguments",
                        G_TYPE_BYTES,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gsk_shader_paintable_init (GskShaderPaintable *self)
{
}

/**
 * gsk_shader_paintable_new:
 * @shader: (transfer full) (nullable): the shader to use
 * @data: (transfer full) (nullable): uniform data
 *
 * Creates a paintable that uses the @shader to create
 * pixels. The shader must not require input textures.
 * If @data is %NULL, all uniform values are set to zero.
 *
 * Returns: (transfer full): a new `GskShaderPaintable`
 */
GdkPaintable *
gsk_shader_paintable_new (GskGLShader *shader,
                          GBytes      *data)
{
  GdkPaintable *ret;

  g_return_val_if_fail (shader == NULL || GSK_IS_GL_SHADER (shader), NULL);

  if (shader && !data)
    {
      int size = gsk_gl_shader_get_args_size (shader);
      data = g_bytes_new_take (g_new0 (guchar, size), size);
    }

  ret = g_object_new (GSK_TYPE_SHADER_PAINTABLE,
                      "shader", shader,
                      "args", data,
                      NULL);

  g_clear_object (&shader);
  g_clear_pointer (&data, g_bytes_unref);

  return ret;
}

/**
 * gsk_shader_paintable_set_shader:
 * @self: a `GskShaderPaintable`
 * @shader: the `GskGLShader` to use
 *
 * Sets the shader that the paintable will use
 * to create pixels. The shader must not require
 * input textures.
 */
void
gsk_shader_paintable_set_shader (GskShaderPaintable *self,
                                 GskGLShader        *shader)
{
  g_return_if_fail (GSK_IS_SHADER_PAINTABLE (self));
  g_return_if_fail (shader == NULL || GSK_IS_GL_SHADER (shader));
  g_return_if_fail (shader == NULL || gsk_gl_shader_get_n_textures (shader) == 0);

  if (!g_set_object (&self->shader, shader))
    return;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHADER]);
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));

  g_clear_pointer (&self->args, g_bytes_unref);
}

/**
 * gsk_shader_paintable_get_shader:
 * @self: a `GskShaderPaintable`
 *
 * Returns the shader that the paintable is using.
 *
 * Returns: (transfer none): the `GskGLShader` that is used
 */
GskGLShader *
gsk_shader_paintable_get_shader (GskShaderPaintable *self)
{
  g_return_val_if_fail (GSK_IS_SHADER_PAINTABLE (self), NULL);

  return self->shader;
}

/**
 * gsk_shader_paintable_set_args:
 * @self: a `GskShaderPaintable`
 * @data: Data block with uniform data for the shader
 *
 * Sets the uniform data that will be passed to the
 * shader when rendering. The @data will typically
 * be produced by a `GskUniformDataBuilder`.
 *
 * Note that the @data should be considered immutable
 * after it has been passed to this function.
 */
void
gsk_shader_paintable_set_args (GskShaderPaintable *self,
                                       GBytes             *data)
{
  g_return_if_fail (GSK_IS_SHADER_PAINTABLE (self));
  g_return_if_fail (data == NULL || g_bytes_get_size (data) == gsk_gl_shader_get_args_size (self->shader));

  g_clear_pointer (&self->args, g_bytes_unref);
  if (data)
    self->args = g_bytes_ref (data);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ARGS]);
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}

/**
 * gsk_shader_paintable_get_args:
 * @self: a `GskShaderPaintable`
 *
 * Returns the uniform data set with
 * gsk_shader_paintable_get_args().
 *
 * Returns: (transfer none): the uniform data
 */
GBytes *
gsk_shader_paintable_get_args (GskShaderPaintable *self)
{
  g_return_val_if_fail (GSK_IS_SHADER_PAINTABLE (self), NULL);

  return self->args;
}

/**
 * gsk_shader_paintable_update_time:
 * @self: a `GskShaderPaintable`
 * @time_idx: the index of the uniform for time in seconds as float
 * @frame_time: the current frame time, as returned by `GdkFrameClock`
 *
 * This function is a convenience wrapper for
 * gsk_shader_paintable_set_args() that leaves all
 * uniform values unchanged, except for the uniform with
 * index @time_idx, which will be set to the elapsed time
 * in seconds, since the first call to this function.
 *
 * This function is usually called from a `GtkTickCallback`.
 */
void
gsk_shader_paintable_update_time (GskShaderPaintable *self,
                                  int                 time_idx,
                                  gint64              frame_time)
{
  GskShaderArgsBuilder *builder;
  GBytes *args;
  float time;

  if (self->start_time == 0)
    self->start_time = frame_time;

  time = (frame_time - self->start_time) / (float)G_TIME_SPAN_SECOND;

  builder = gsk_shader_args_builder_new (self->shader, self->args);
  gsk_shader_args_builder_set_float (builder, time_idx, time);
  args = gsk_shader_args_builder_free_to_args (builder);

  gsk_shader_paintable_set_args (self, args);

  g_bytes_unref (args);
}
