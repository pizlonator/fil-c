/*
 * Copyright © 2023 Benjamin Otte
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gdkgltexturebuilder.h"

#include "gdkenumtypes.h"
#include "gdkglcontext.h"
#include "gdkgltextureprivate.h"

#include <cairo-gobject.h>

struct _GdkGLTextureBuilder
{
  GObject parent_instance;

  GdkGLContext *context;
  guint id;
  int width;
  int height;
  GdkMemoryFormat format;
  gboolean has_mipmap;
  gpointer sync;

  GdkTexture *update_texture;
  cairo_region_t *update_region;
};

struct _GdkGLTextureBuilderClass
{
  GObjectClass parent_class;
};

/**
 * GdkGLTextureBuilder:
 *
 * `GdkGLTextureBuilder` is a builder used to construct [class@Gdk.Texture] objects from
 * GL textures.
 *
 * The operation is quite simple: Create a texture builder, set all the necessary
 * properties - keep in mind that the properties [property@Gdk.GLTextureBuilder:context],
 * [property@Gdk.GLTextureBuilder:id], [property@Gdk.GLTextureBuilder:width], and 
 * [property@Gdk.GLTextureBuilder:height] are mandatory - and then call
 * [method@Gdk.GLTextureBuilder.build] to create the new texture.
 *
 * `GdkGLTextureBuilder` can be used for quick one-shot construction of
 * textures as well as kept around and reused to construct multiple textures.
 *
 * Since: 4.12
 */

enum
{
  PROP_0,
  PROP_CONTEXT,
  PROP_FORMAT,
  PROP_HAS_MIPMAP,
  PROP_HEIGHT,
  PROP_ID,
  PROP_SYNC,
  PROP_UPDATE_REGION,
  PROP_UPDATE_TEXTURE,
  PROP_WIDTH,

  N_PROPS
};

G_DEFINE_TYPE (GdkGLTextureBuilder, gdk_gl_texture_builder, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gdk_gl_texture_builder_dispose (GObject *object)
{
  GdkGLTextureBuilder *self = GDK_GL_TEXTURE_BUILDER (object);

  g_clear_object (&self->context);

  g_clear_object (&self->update_texture);
  g_clear_pointer (&self->update_region, cairo_region_destroy);

  G_OBJECT_CLASS (gdk_gl_texture_builder_parent_class)->dispose (object);
}

static void
gdk_gl_texture_builder_get_property (GObject    *object,
                                     guint       property_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GdkGLTextureBuilder *self = GDK_GL_TEXTURE_BUILDER (object);

  switch (property_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    case PROP_FORMAT:
      g_value_set_enum (value, self->format);
      break;

    case PROP_HAS_MIPMAP:
      g_value_set_boolean (value, self->has_mipmap);
      break;

    case PROP_HEIGHT:
      g_value_set_int (value, self->height);
      break;

    case PROP_ID:
      g_value_set_uint (value, self->id);
      break;

    case PROP_SYNC:
      g_value_set_pointer (value, self->sync);
      break;

    case PROP_UPDATE_REGION:
      g_value_set_boxed (value, self->update_region);
      break;

    case PROP_UPDATE_TEXTURE:
      g_value_set_object (value, self->update_texture);
      break;

    case PROP_WIDTH:
      g_value_set_int (value, self->width);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gdk_gl_texture_builder_set_property (GObject      *object,
                                     guint         property_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GdkGLTextureBuilder *self = GDK_GL_TEXTURE_BUILDER (object);

  switch (property_id)
    {
    case PROP_CONTEXT:
      gdk_gl_texture_builder_set_context (self, g_value_get_object (value));
      break;

    case PROP_FORMAT:
      gdk_gl_texture_builder_set_format (self, g_value_get_enum (value));
      break;

    case PROP_HAS_MIPMAP:
      gdk_gl_texture_builder_set_has_mipmap (self, g_value_get_boolean (value));
      break;

    case PROP_HEIGHT:
      gdk_gl_texture_builder_set_height (self, g_value_get_int (value));
      break;

    case PROP_ID:
      gdk_gl_texture_builder_set_id (self, g_value_get_uint (value));
      break;

    case PROP_SYNC:
      gdk_gl_texture_builder_set_sync (self, g_value_get_pointer (value));
      break;

    case PROP_UPDATE_REGION:
      gdk_gl_texture_builder_set_update_region (self, g_value_get_boxed (value));
      break;

    case PROP_UPDATE_TEXTURE:
      gdk_gl_texture_builder_set_update_texture (self, g_value_get_object (value));
      break;

    case PROP_WIDTH:
      gdk_gl_texture_builder_set_width (self, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gdk_gl_texture_builder_class_init (GdkGLTextureBuilderClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gdk_gl_texture_builder_dispose;
  gobject_class->get_property = gdk_gl_texture_builder_get_property;
  gobject_class->set_property = gdk_gl_texture_builder_set_property;

  /**
   * GdkGLTextureBuilder:context: (attributes org.gtk.Property.get=gdk_gl_texture_builder_get_context org.gtk.Property.set=gdk_gl_texture_builder_set_context)
   *
   * The context owning the texture.
   *
   * Since: 4.12
   */
  properties[PROP_CONTEXT] =
    g_param_spec_object ("context", NULL, NULL,
                         GDK_TYPE_GL_CONTEXT,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkGLTextureBuilder:format: (attributes org.gtk.Property.get=gdk_gl_texture_builder_get_format org.gtk.Property.set=gdk_gl_texture_builder_set_format)
   *
   * The format when downloading the texture.
   *
   * Since: 4.12
   */
  properties[PROP_FORMAT] =
    g_param_spec_enum ("format", NULL, NULL,
                       GDK_TYPE_MEMORY_FORMAT,
                       GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkGLTextureBuilder:has-mipmap: (attributes org.gtk.Property.get=gdk_gl_texture_builder_get_has_mipmap org.gtk.Property.set=gdk_gl_texture_builder_set_has_mipmap)
   *
   * If the texture has a mipmap.
   *
   * Since: 4.12
   */
  properties[PROP_HAS_MIPMAP] =
    g_param_spec_boolean ("has-mipmap", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkGLTextureBuilder:height: (attributes org.gtk.Property.get=gdk_gl_texture_builder_get_height org.gtk.Property.set=gdk_gl_texture_builder_set_height)
   *
   * The height of the texture.
   *
   * Since: 4.12
   */
  properties[PROP_HEIGHT] =
    g_param_spec_int ("height", NULL, NULL,
                      G_MININT, G_MAXINT, 0,
                      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkGLTextureBuilder:id: (attributes org.gtk.Property.get=gdk_gl_texture_builder_get_id org.gtk.Property.set=gdk_gl_texture_builder_set_id)
   *
   * The texture ID to use.
   *
   * Since: 4.12
   */
  properties[PROP_ID] =
    g_param_spec_uint ("id", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkGLTextureBuilder:sync: (attributes org.gtk.Property.get=gdk_gl_texture_builder_get_sync org.gtk.Property.set=gdk_gl_texture_builder_set_sync)
   *
   * An optional `GLSync` object.
   *
   * If this is set, GTK will wait on it before using the texture.
   *
   * Since: 4.12
   */
  properties[PROP_SYNC] =
    g_param_spec_pointer ("sync", NULL, NULL,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkGLTextureBuilder:update-region: (attributes org.gtk.Property.get=gdk_gl_texture_builder_get_update_region org.gtk.Property.set=gdk_gl_texture_builder_set_update_region)
   *
   * The update region for [property@Gdk.GLTextureBuilder:update-texture].
   *
   * Since: 4.12
   */
  properties[PROP_UPDATE_REGION] =
    g_param_spec_boxed ("update-region", NULL, NULL,
                        CAIRO_GOBJECT_TYPE_REGION,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkGLTextureBuilder:update-texture: (attributes org.gtk.Property.get=gdk_gl_texture_builder_get_update_texture org.gtk.Property.set=gdk_gl_texture_builder_set_update_texture)
   *
   * The texture [property@Gdk.GLTextureBuilder:update-region] is an update for.
   *
   * Since: 4.12
   */
  properties[PROP_UPDATE_TEXTURE] =
    g_param_spec_object ("update-texture", NULL, NULL,
                         GDK_TYPE_TEXTURE,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkGLTextureBuilder:width: (attributes org.gtk.Property.get=gdk_gl_texture_builder_get_width org.gtk.Property.set=gdk_gl_texture_builder_set_width)
   *
   * The width of the texture.
   *
   * Since: 4.12
   */
  properties[PROP_WIDTH] =
    g_param_spec_int ("width", NULL, NULL,
                      G_MININT, G_MAXINT, 0,
                      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gdk_gl_texture_builder_init (GdkGLTextureBuilder *self)
{
  self->format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
}

/**
 * gdk_gl_texture_builder_new: (constructor):
 *
 * Creates a new texture builder.
 *
 * Returns: the new `GdkTextureBuilder`
 *
 * Since: 4.12
 **/
GdkGLTextureBuilder *
gdk_gl_texture_builder_new (void)
{
  return g_object_new (GDK_TYPE_GL_TEXTURE_BUILDER, NULL);
}

/**
 * gdk_gl_texture_builder_get_context: (attributes org.gtk.Method.get_property=context)
 * @self: a `GdkGLTextureBuilder`
 *
 * Gets the context previously set via gdk_gl_texture_builder_set_context() or
 * %NULL if none was set.
 *
 * Returns: (transfer none) (nullable): The context
 *
 * Since: 4.12
 */
GdkGLContext *
gdk_gl_texture_builder_get_context (GdkGLTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_GL_TEXTURE_BUILDER (self), NULL);

  return self->context;
}

/**
 * gdk_gl_texture_builder_set_context: (attributes org.gtk.Method.set_property=context)
 * @self: a `GdkGLTextureBuilder`
 * @context: (nullable): The context the texture beongs to or %NULL to unset
 *
 * Sets the context to be used for the texture. This is the context that owns
 * the texture.
 *
 * The context must be set before calling [method@Gdk.GLTextureBuilder.build].
 *
 * Since: 4.12
 */
void
gdk_gl_texture_builder_set_context (GdkGLTextureBuilder *self,
                                    GdkGLContext        *context)
{
  g_return_if_fail (GDK_IS_GL_TEXTURE_BUILDER (self));
  g_return_if_fail (context == NULL || GDK_IS_GL_CONTEXT (context));

  if (!g_set_object (&self->context, context))
    return;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CONTEXT]);
}

/**
 * gdk_gl_texture_builder_get_height: (attributes org.gtk.Method.get_property=height)
 * @self: a `GdkGLTextureBuilder`
 *
 * Gets the height previously set via gdk_gl_texture_builder_set_height() or
 * 0 if the height wasn't set.
 *
 * Returns: The height
 *
 * Since: 4.12
 */
int
gdk_gl_texture_builder_get_height (GdkGLTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_GL_TEXTURE_BUILDER (self), 0);

  return self->height;
}

/**
 * gdk_gl_texture_builder_set_height: (attributes org.gtk.Method.set_property=height)
 * @self: a `GdkGLTextureBuilder`
 * @height: The texture's height or 0 to unset
 *
 * Sets the height of the texture.
 *
 * The height must be set before calling [method@Gdk.GLTextureBuilder.build].
 *
 * Since: 4.12
 */
void
gdk_gl_texture_builder_set_height (GdkGLTextureBuilder *self,
                                   int                  height)
{
  g_return_if_fail (GDK_IS_GL_TEXTURE_BUILDER (self));

  if (self->height == height)
    return;

  self->height = height;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HEIGHT]);
}

/**
 * gdk_gl_texture_builder_get_id: (attributes org.gtk.Method.get_property=id)
 * @self: a `GdkGLTextureBuilder`
 *
 * Gets the texture id previously set via gdk_gl_texture_builder_set_id() or
 * 0 if the id wasn't set.
 *
 * Returns: The id
 *
 * Since: 4.12
 */
guint
gdk_gl_texture_builder_get_id (GdkGLTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_GL_TEXTURE_BUILDER (self), 0);

  return self->id;
}

/**
 * gdk_gl_texture_builder_set_id: (attributes org.gtk.Method.set_property=id)
 * @self: a `GdkGLTextureBuilder`
 * @id: The texture id to be used for creating the texture
 *
 * Sets the texture id of the texture. The texture id must remain unmodified
 * until the texture was finalized. See [method@Gdk.GLTextureBuilder.build]
 * for a longer discussion.
 *
 * The id must be set before calling [method@Gdk.GLTextureBuilder.build].
 *
 * Since: 4.12
 */
void
gdk_gl_texture_builder_set_id (GdkGLTextureBuilder *self,
                               guint                id)
{
  g_return_if_fail (GDK_IS_GL_TEXTURE_BUILDER (self));

  if (self->id == id)
    return;

  self->id = id;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ID]);
}

/**
 * gdk_gl_texture_builder_get_width: (attributes org.gtk.Method.get_property=width)
 * @self: a `GdkGLTextureBuilder`
 *
 * Gets the width previously set via gdk_gl_texture_builder_set_width() or
 * 0 if the width wasn't set.
 *
 * Returns: The width
 *
 * Since: 4.12
 */
int
gdk_gl_texture_builder_get_width (GdkGLTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_GL_TEXTURE_BUILDER (self), 0);

  return self->width;
}

/**
 * gdk_gl_texture_builder_set_width: (attributes org.gtk.Method.set_property=width)
 * @self: a `GdkGLTextureBuilder`
 * @width: The texture's width or 0 to unset
 *
 * Sets the width of the texture.
 *
 * The width must be set before calling [method@Gdk.GLTextureBuilder.build].
 *
 * Since: 4.12
 */
void
gdk_gl_texture_builder_set_width (GdkGLTextureBuilder *self,
                                  int                  width)
{
  g_return_if_fail (GDK_IS_GL_TEXTURE_BUILDER (self));

  if (self->width == width)
    return;

  self->width = width;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WIDTH]);
}

/**
 * gdk_gl_texture_builder_get_has_mipmap: (attributes org.gtk.Method.get_property=has-mipmap)
 * @self: a `GdkGLTextureBuilder`
 *
 * Gets whether the texture has a mipmap.
 *
 * Returns: Whether the texture has a mipmap
 *
 * Since: 4.12
 */
gboolean
gdk_gl_texture_builder_get_has_mipmap (GdkGLTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_GL_TEXTURE_BUILDER (self), FALSE);

  return self->has_mipmap;
}

/**
 * gdk_gl_texture_builder_set_has_mipmap: (attributes org.gtk.Method.set_property=has-mipmap)
 * @self: a `GdkGLTextureBuilder`
 * @has_mipmap: Whether the texture has a mipmap
 *
 * Sets whether the texture has a mipmap. This allows the renderer and other users of the
 * generated texture to use a higher quality downscaling.
 *
 * Typically, the `glGenerateMipmap` function is used to generate a mimap.
 *
 * Since: 4.12
 */
void
gdk_gl_texture_builder_set_has_mipmap (GdkGLTextureBuilder *self,
                                       gboolean             has_mipmap)
{
  g_return_if_fail (GDK_IS_GL_TEXTURE_BUILDER (self));

  if (self->has_mipmap == has_mipmap)
    return;

  self->has_mipmap = has_mipmap;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HAS_MIPMAP]);
}

/**
 * gdk_gl_texture_builder_get_sync: (attributes org.gtk.Method.get_property=sync)
 * @self: a `GdkGLTextureBuilder`
 *
 * Gets the `GLsync` previously set via gdk_gl_texture_builder_set_sync().
 *
 * Returns: (nullable): the `GLSync`
 *
 * Since: 4.12
 */
gpointer
gdk_gl_texture_builder_get_sync (GdkGLTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_GL_TEXTURE_BUILDER (self), NULL);

  return self->sync;
}

/**
 * gdk_gl_texture_builder_set_sync: (attributes org.gtk.Method.set_property=sync)
 * @self: a `GdkGLTextureBuilder`
 * @sync: (nullable): the GLSync object
 *
 * Sets the GLSync object to use for the texture.
 *
 * GTK will wait on this object before using the created `GdkTexture`.
 *
 * The `destroy` function that is passed to [method@Gdk.GLTextureBuilder.build]
 * is responsible for freeing the sync object when it is no longer needed.
 * The texture builder does not destroy it and it is the callers
 * responsibility to make sure it doesn't leak.
 *
 * Since: 4.12
 */
void
gdk_gl_texture_builder_set_sync (GdkGLTextureBuilder *self,
                                 gpointer             sync)
{
  g_return_if_fail (GDK_IS_GL_TEXTURE_BUILDER (self));

  if (self->sync == sync)
    return;

  self->sync = sync;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SYNC]);
}

/**
 * gdk_gl_texture_builder_get_format: (attributes org.gtk.Method.get_property=format)
 * @self: a `GdkGLTextureBuilder`
 *
 * Gets the format previously set via gdk_gl_texture_builder_set_format().
 *
 * Returns: The format
 *
 * Since: 4.12
 */
GdkMemoryFormat
gdk_gl_texture_builder_get_format (GdkGLTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_GL_TEXTURE_BUILDER (self), GDK_MEMORY_R8G8B8A8_PREMULTIPLIED);

  return self->format;
}

/**
 * gdk_gl_texture_builder_set_format: (attributes org.gtk.Method.set_property=format)
 * @self: a `GdkGLTextureBuilder`
 * @format: The texture's format
 *
 * Sets the format of the texture. The default is `GDK_MEMORY_R8G8B8A8_PREMULTIPLIED`.
 *
 * The format is the preferred format the texture data should be downloaded to. The
 * format must be supported by the GL version of [property@Gdk.GLTextureBuilder:context].
 *
 * GDK's texture download code assumes that the format corresponds to the storage
 * parameters of the GL texture in an obvious way. For example, a format of
 * `GDK_MEMORY_R16G16B16A16_PREMULTIPLIED` is expected to be stored as `GL_RGBA16`
 * texture, and `GDK_MEMORY_G8A8` is expected to be stored as `GL_RG8` texture.
 *
 * Setting the right format is particularly useful when using high bit depth textures
 * to preserve the bit depth, to set the correct value for unpremultiplied textures
 * and to make sure opaque textures are treated as such.
 *
 * Non-RGBA textures need to have swizzling parameters set up properly to be usable
 * in GSK's shaders.
 *
 * Since: 4.12
 */
void
gdk_gl_texture_builder_set_format (GdkGLTextureBuilder *self,
                                   GdkMemoryFormat      format)
{
  g_return_if_fail (GDK_IS_GL_TEXTURE_BUILDER (self));

  if (self->format == format)
    return;

  self->format = format;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FORMAT]);
}

/**
 * gdk_gl_texture_builder_get_update_texture: (attributes org.gtk.Method.get_property=update-texture)
 * @self: a `GdkGLTextureBuilder`
 *
 * Gets the texture previously set via gdk_gl_texture_builder_set_update_texture() or
 * %NULL if none was set.
 *
 * Returns: (transfer none) (nullable): The texture 
 *
 * Since: 4.12
 */
GdkTexture *
gdk_gl_texture_builder_get_update_texture (GdkGLTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_GL_TEXTURE_BUILDER (self), NULL);

  return self->update_texture;
}

/**
 * gdk_gl_texture_builder_set_update_texture: (attributes org.gtk.Method.set_property=update-texture)
 * @self: a `GdkGLTextureBuilder`
 * @texture: (nullable): the texture to update
 *
 * Sets the texture to be updated by this texture. See
 * [method@Gdk.GLTextureBuilder.set_update_region] for an explanation.
 *
 * Since: 4.12
 */
void
gdk_gl_texture_builder_set_update_texture (GdkGLTextureBuilder *self,
                                           GdkTexture          *texture)
{
  g_return_if_fail (GDK_IS_GL_TEXTURE_BUILDER (self));
  g_return_if_fail (texture == NULL || GDK_IS_TEXTURE (texture));

  if (!g_set_object (&self->update_texture, texture))
    return;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_UPDATE_TEXTURE]);
}

/**
 * gdk_gl_texture_builder_get_update_region: (attributes org.gtk.Method.get_property=update-region)
 * @self: a `GdkGLTextureBuilder`
 *
 * Gets the region previously set via gdk_gl_texture_builder_set_update_region() or
 * %NULL if none was set.
 *
 * Returns: (transfer none) (nullable): The region 
 *
 * Since: 4.12
 */
cairo_region_t *
gdk_gl_texture_builder_get_update_region (GdkGLTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_GL_TEXTURE_BUILDER (self), NULL);

  return self->update_region;
}

/**
 * gdk_gl_texture_builder_set_update_region: (attributes org.gtk.Method.set_property=update-region)
 * @self: a `GdkGLTextureBuilder`
 * @region: (nullable): the region to update
 *
 * Sets the region to be updated by this texture. Together with
 * [property@Gdk.GLTextureBuilder:update-texture] this describes an
 * update of a previous texture.
 *
 * When rendering animations of large textures, it is possible that
 * consecutive textures are only updating contents in parts of the texture.
 * It is then possible to describe this update via these two properties,
 * so that GTK can avoid rerendering parts that did not change.
 *
 * An example would be a screen recording where only the mouse pointer moves.
 *
 * Since: 4.12
 */
void
gdk_gl_texture_builder_set_update_region (GdkGLTextureBuilder *self,
                                         cairo_region_t      *region)
{
  g_return_if_fail (GDK_IS_GL_TEXTURE_BUILDER (self));

  if (self->update_region == region)
    return;

  g_clear_pointer (&self->update_region, cairo_region_destroy);

  if (region)
    self->update_region = cairo_region_reference (region);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_UPDATE_REGION]);
}

/**
 * gdk_gl_texture_builder_build:
 * @self: a `GdkGLTextureBuilder`
 * @destroy: (nullable): destroy function to be called when the texture is
 *   released
 * @data: user data to pass to the destroy function
 *
 * Builds a new `GdkTexture` with the values set up in the builder.
 *
 * The `destroy` function gets called when the returned texture gets released;
 * either when the texture is finalized or by an explicit call to
 * [method@Gdk.GLTexture.release]. It should release all GL resources associated
 * with the texture, such as the [property@Gdk.GLTextureBuilder:id] and the
 * [property@Gdk.GLTextureBuilder:sync].
 *
 * Note that it is a programming error to call this function if any mandatory
 * property has not been set.
 *
 * It is possible to call this function multiple times to create multiple textures,
 * possibly with changing properties in between.
 *
 * Returns: (transfer full): a newly built `GdkTexture`
 *
 * Since: 4.12
 */
GdkTexture *
gdk_gl_texture_builder_build (GdkGLTextureBuilder *self,
                              GDestroyNotify       destroy,
                              gpointer             data)
{
  g_return_val_if_fail (GDK_IS_GL_TEXTURE_BUILDER (self), NULL);
  g_return_val_if_fail (destroy == NULL || data != NULL, NULL);
  g_return_val_if_fail (self->context != NULL, NULL);
  g_return_val_if_fail (self->id != 0, NULL);
  g_return_val_if_fail (self->width > 0, NULL);
  g_return_val_if_fail (self->height > 0, NULL);

  return gdk_gl_texture_new_from_builder (self, destroy, data);
}

