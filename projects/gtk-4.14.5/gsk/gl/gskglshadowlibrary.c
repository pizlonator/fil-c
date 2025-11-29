/* gskglshadowlibrary.c
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

#include <string.h>

#include "gskgldriverprivate.h"
#include "gskglshadowlibraryprivate.h"

#define MAX_UNUSED_FRAMES (16 * 5)

struct _GskGLShadowLibrary
{
  GObject      parent_instance;
  GskGLDriver *driver;
  GArray      *shadows;
};

typedef struct _Shadow
{
  GskRoundedRect outline;
  float          blur_radius;
  guint          texture_id;
  gint64         last_used_in_frame;
} Shadow;

G_DEFINE_TYPE (GskGLShadowLibrary, gsk_gl_shadow_library, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_DRIVER,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

GskGLShadowLibrary *
gsk_gl_shadow_library_new (GskGLDriver *driver)
{
  g_return_val_if_fail (GSK_IS_GL_DRIVER (driver), NULL);

  return g_object_new (GSK_TYPE_GL_SHADOW_LIBRARY,
                       "driver", driver,
                       NULL);
}

static void
gsk_gl_shadow_library_dispose (GObject *object)
{
  GskGLShadowLibrary *self = (GskGLShadowLibrary *)object;

  for (guint i = 0; i < self->shadows->len; i++)
    {
      const Shadow *shadow = &g_array_index (self->shadows, Shadow, i);
      gsk_gl_driver_release_texture_by_id (self->driver, shadow->texture_id);
    }

  g_clear_pointer (&self->shadows, g_array_unref);
  g_clear_object (&self->driver);

  G_OBJECT_CLASS (gsk_gl_shadow_library_parent_class)->dispose (object);
}

static void
gsk_gl_shadow_library_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GskGLShadowLibrary *self = GSK_GL_SHADOW_LIBRARY (object);

  switch (prop_id)
    {
    case PROP_DRIVER:
      g_value_set_object (value, self->driver);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gsk_gl_shadow_library_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GskGLShadowLibrary *self = GSK_GL_SHADOW_LIBRARY (object);

  switch (prop_id)
    {
    case PROP_DRIVER:
      self->driver = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gsk_gl_shadow_library_class_init (GskGLShadowLibraryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gsk_gl_shadow_library_dispose;
  object_class->get_property = gsk_gl_shadow_library_get_property;
  object_class->set_property = gsk_gl_shadow_library_set_property;

  properties [PROP_DRIVER] =
    g_param_spec_object ("driver", NULL, NULL,
                         GSK_TYPE_GL_DRIVER,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gsk_gl_shadow_library_init (GskGLShadowLibrary *self)
{
  self->shadows = g_array_new (FALSE, FALSE, sizeof (Shadow));
}

void
gsk_gl_shadow_library_insert (GskGLShadowLibrary   *self,
                              const GskRoundedRect *outline,
                              float                 blur_radius,
                              guint                 texture_id)
{
  Shadow *shadow;

  g_assert (GSK_IS_GL_SHADOW_LIBRARY (self));
  g_assert (outline != NULL);
  g_assert (texture_id != 0);

  gsk_gl_driver_mark_texture_permanent (self->driver, texture_id);

  g_array_set_size (self->shadows, self->shadows->len + 1);

  shadow = &g_array_index (self->shadows, Shadow, self->shadows->len - 1);
  shadow->outline = *outline;
  shadow->blur_radius = blur_radius;
  shadow->texture_id = texture_id;
  shadow->last_used_in_frame = self->driver->current_frame_id;
}

guint
gsk_gl_shadow_library_lookup (GskGLShadowLibrary   *self,
                              const GskRoundedRect *outline,
                              float                 blur_radius)
{
  Shadow *ret = NULL;

  g_assert (GSK_IS_GL_SHADOW_LIBRARY (self));
  g_assert (outline != NULL);

  /* Ensure GskRoundedRect  is 12 packed floats without padding
   * so that we can use memcmp instead of float comparisons.
   */
  G_STATIC_ASSERT (sizeof *outline == (sizeof (float) * 12));

  for (guint i = 0; i < self->shadows->len; i++)
    {
      Shadow *shadow = &g_array_index (self->shadows, Shadow, i);

      if (blur_radius == shadow->blur_radius &&
          memcmp (outline, &shadow->outline, sizeof *outline) == 0)
        {
          ret = shadow;
          break;
        }
    }

  if (ret == NULL)
    return 0;

  g_assert (ret->texture_id != 0);

  ret->last_used_in_frame = self->driver->current_frame_id;

  return ret->texture_id;
}

#if 0
static void
write_shadow_to_png (GskGLDriver *driver,
                     const Shadow *shadow)
{
  int width = shadow->outline.bounds.size.width + (shadow->outline.bounds.origin.x * 2);
  int height = shadow->outline.bounds.size.height + (shadow->outline.bounds.origin.y * 2);
  char *filename = g_strdup_printf ("shadow_cache_%d_%d_%d.png",
                                    width, height, shadow->texture_id);
  GdkTexture *texture;

  texture = gdk_gl_texture_new (gsk_gl_driver_get_context (driver),
                                shadow->texture_id,
                                width, height,
                                NULL, NULL);
  gdk_texture_save_to_png (texture, filename);

  g_object_unref (texture);
  g_free (filename);
}
#endif

void
gsk_gl_shadow_library_begin_frame (GskGLShadowLibrary *self)
{
  gint64 watermark;
  int i;
  int p;

  g_return_if_fail (GSK_IS_GL_SHADOW_LIBRARY (self));

#if 0
  for (i = 0, p = self->shadows->len; i < p; i++)
    {
      const Shadow *shadow = &g_array_index (self->shadows, Shadow, i);
      write_shadow_to_png (self->driver, shadow);
    }
#endif

  watermark = self->driver->current_frame_id - MAX_UNUSED_FRAMES;

  for (i = 0, p = self->shadows->len; i < p; i++)
    {
      const Shadow *shadow = &g_array_index (self->shadows, Shadow, i);

      if (shadow->last_used_in_frame < watermark)
        {
          gsk_gl_driver_release_texture_by_id (self->driver, shadow->texture_id);
          g_array_remove_index_fast (self->shadows, i);
          p--;
          i--;
        }
    }
}
