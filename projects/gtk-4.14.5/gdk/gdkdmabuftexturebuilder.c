/*
 * Copyright © 2023 Red Hat, Inc.
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

#include "gdkdmabuftexturebuilder.h"

#include "gdkdebugprivate.h"
#include "gdkdisplay.h"
#include "gdkenumtypes.h"
#include "gdkdmabuftextureprivate.h"

#include <cairo-gobject.h>


struct _GdkDmabufTextureBuilder
{
  GObject parent_instance;

  GdkDisplay *display;
  unsigned int width;
  unsigned int height;
  gboolean premultiplied;

  GdkDmabuf dmabuf;

  GdkTexture *update_texture;
  cairo_region_t *update_region;
};

struct _GdkDmabufTextureBuilderClass
{
  GObjectClass parent_class;
};

/**
 * GdkDmabufTextureBuilder:
 *
 * `GdkDmabufTextureBuilder` is a builder used to construct [class@Gdk.Texture]
 * objects from DMA buffers.
 *
 * DMA buffers are commonly called **_dma-bufs_**.
 *
 * DMA buffers are a feature of the Linux kernel to enable efficient buffer and
 * memory sharing between hardware such as codecs, GPUs, displays, cameras and the
 * kernel drivers controlling them. For example, a decoder may want its output to
 * be directly shared with the display server for rendering without a copy.
 *
 * Any device driver which participates in DMA buffer sharing, can do so as either
 * the exporter or importer of buffers (or both).
 *
 * The memory that is shared via DMA buffers is usually stored in non-system memory
 * (maybe in device's local memory or something else not directly accessible by the
 * CPU), and accessing this memory from the CPU may have higher-than-usual overhead.
 *
 * In particular for graphics data, it is not uncommon that data consists of multiple
 * separate blocks of memory, for example one block for each of the red, green and
 * blue channels. These blocks are called **_planes_**. DMA buffers can have up to
 * four planes. Even if the memory is a single block, the data can be organized in
 * multiple planes, by specifying offsets from the beginning of the data.
 *
 * DMA buffers are exposed to user-space as file descriptors allowing to pass them
 * between processes. If a DMA buffer has multiple planes, there is one file
 * descriptor per plane.
 *
 * The format of the data (for graphics data, essentially its colorspace) is described
 * by a 32-bit integer. These format identifiers are defined in the header file `drm_fourcc.h`
 * and commonly referred to as **_fourcc_** values, since they are identified by 4 ASCII
 * characters. Additionally, each DMA buffer has a **_modifier_**, which is a 64-bit integer
 * that describes driver-specific details of the memory layout, such as tiling or compression.
 *
 * For historical reasons, some producers of dma-bufs don't provide an explicit modifier, but
 * instead return `DMA_FORMAT_MOD_INVALID` to indicate that their modifier is **_implicit_**.
 * GTK tries to accommodate this situation by accepting `DMA_FORMAT_MOD_INVALID` as modifier.
 *
 * The operation of `GdkDmabufTextureBuilder` is quite simple: Create a texture builder,
 * set all the necessary properties, and then call [method@Gdk.DmabufTextureBuilder.build]
 * to create the new texture.
 *
 * The required properties for a dma-buf texture are
 *
 *  * The width and height in pixels
 *
 *  * The `fourcc` code and `modifier` which identify the format and memory layout of the dma-buf
 *
 *  * The file descriptor, offset and stride for each of the planes
 *
 * `GdkDmabufTextureBuilder` can be used for quick one-shot construction of
 * textures as well as kept around and reused to construct multiple textures.
 *
 * For further information, see
 *
 * * The Linux kernel [documentation](https://docs.kernel.org/driver-api/dma-buf.html)
 *
 * * The header file [drm_fourcc.h](https://gitlab.freedesktop.org/mesa/drm/-/blob/main/include/drm/drm_fourcc.h)
 *
 * Since: 4.14
 */

enum
{
  PROP_0,
  PROP_DISPLAY,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_FOURCC,
  PROP_MODIFIER,
  PROP_PREMULTIPLIED,
  PROP_N_PLANES,
  PROP_UPDATE_REGION,
  PROP_UPDATE_TEXTURE,

  N_PROPS
};

G_DEFINE_TYPE (GdkDmabufTextureBuilder, gdk_dmabuf_texture_builder, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gdk_dmabuf_texture_builder_dispose (GObject *object)
{
  GdkDmabufTextureBuilder *self = GDK_DMABUF_TEXTURE_BUILDER (object);

  g_clear_object (&self->update_texture);
  g_clear_pointer (&self->update_region, cairo_region_destroy);

  G_OBJECT_CLASS (gdk_dmabuf_texture_builder_parent_class)->dispose (object);
}

static void
gdk_dmabuf_texture_builder_get_property (GObject    *object,
                                         guint       property_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  GdkDmabufTextureBuilder *self = GDK_DMABUF_TEXTURE_BUILDER (object);

  switch (property_id)
    {
    case PROP_DISPLAY:
      g_value_set_object (value, self->display);
      break;

    case PROP_WIDTH:
      g_value_set_uint (value, self->width);
      break;

    case PROP_HEIGHT:
      g_value_set_uint (value, self->height);
      break;

    case PROP_FOURCC:
      g_value_set_uint (value, self->dmabuf.fourcc);
      break;

    case PROP_MODIFIER:
      g_value_set_uint64 (value, self->dmabuf.modifier);
      break;

    case PROP_PREMULTIPLIED:
      g_value_set_boolean (value, self->premultiplied);
      break;

    case PROP_N_PLANES:
      g_value_set_uint (value, self->dmabuf.n_planes);
      break;

    case PROP_UPDATE_REGION:
      g_value_set_boxed (value, self->update_region);
      break;

    case PROP_UPDATE_TEXTURE:
      g_value_set_object (value, self->update_texture);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gdk_dmabuf_texture_builder_set_property (GObject      *object,
                                         guint         property_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  GdkDmabufTextureBuilder *self = GDK_DMABUF_TEXTURE_BUILDER (object);

  switch (property_id)
    {
    case PROP_DISPLAY:
      gdk_dmabuf_texture_builder_set_display (self, g_value_get_object (value));
      break;

    case PROP_WIDTH:
      gdk_dmabuf_texture_builder_set_width (self, g_value_get_uint (value));
      break;

    case PROP_HEIGHT:
      gdk_dmabuf_texture_builder_set_height (self, g_value_get_uint (value));
      break;

    case PROP_FOURCC:
      gdk_dmabuf_texture_builder_set_fourcc (self, g_value_get_uint (value));
      break;

    case PROP_MODIFIER:
      gdk_dmabuf_texture_builder_set_modifier (self, g_value_get_uint64 (value));
      break;

    case PROP_PREMULTIPLIED:
      gdk_dmabuf_texture_builder_set_premultiplied (self, g_value_get_boolean (value));
      break;

    case PROP_N_PLANES:
      gdk_dmabuf_texture_builder_set_n_planes (self, g_value_get_uint (value));
      break;

    case PROP_UPDATE_REGION:
      gdk_dmabuf_texture_builder_set_update_region (self, g_value_get_boxed (value));
      break;

    case PROP_UPDATE_TEXTURE:
      gdk_dmabuf_texture_builder_set_update_texture (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gdk_dmabuf_texture_builder_class_init (GdkDmabufTextureBuilderClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gdk_dmabuf_texture_builder_dispose;
  gobject_class->get_property = gdk_dmabuf_texture_builder_get_property;
  gobject_class->set_property = gdk_dmabuf_texture_builder_set_property;

  /**
   * GdkDmabufTextureBuilder:display: (attributes org.gtk.Property.get=gdk_dmabuf_texture_builder_get_display org.gtk.Property.set=gdk_dmabuf_texture_builder_set_display)
   *
   * The display that this texture will be used on.
   *
   * Since: 4.14
   */
  properties[PROP_DISPLAY] =
    g_param_spec_object ("display", NULL, NULL,
                         GDK_TYPE_DISPLAY,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDmabufTextureBuilder:width: (attributes org.gtk.Property.get=gdk_dmabuf_texture_builder_get_width org.gtk.Property.set=gdk_dmabuf_texture_builder_set_width)
   *
   * The width of the texture.
   *
   * Since: 4.14
   */
  properties[PROP_WIDTH] =
    g_param_spec_uint ("width", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDmabufTextureBuilder:height: (attributes org.gtk.Property.get=gdk_dmabuf_texture_builder_get_height org.gtk.Property.set=gdk_dmabuf_texture_builder_set_height)
   *
   * The height of the texture.
   *
   * Since: 4.14
   */
  properties[PROP_HEIGHT] =
    g_param_spec_uint ("height", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDmabufTextureBuilder:fourcc: (attributes org.gtk.Property.get=gdk_dmabuf_texture_builder_get_fourcc org.gtk.Property.set=gdk_dmabuf_texture_builder_set_fourcc)
   *
   * The format of the texture, as a fourcc value.
   *
   * Since: 4.14
   */
  properties[PROP_FOURCC] =
    g_param_spec_uint ("fourcc", NULL, NULL,
                       0, 0xffffffff, 0,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDmabufTextureBuilder:modifier:
   *
   * The modifier.
   *
   * Since: 4.14
   */
  properties[PROP_MODIFIER] =
    g_param_spec_uint64 ("modifier", NULL, NULL,
                         0, G_MAXUINT, 0,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDmabufTextureBuilder:premultiplied:
   *
   * Whether the alpha channel is premultiplied into the others.
   *
   * Only relevant if the format has alpha.
   *
   * Since: 4.14
   */
  properties[PROP_PREMULTIPLIED] =
    g_param_spec_boolean ("premultiplied", NULL, NULL,
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDmabufTextureBuilder:n-planes: (attributes org.gtk.Property.get=gdk_dmabuf_texture_builder_get_n_planes org.gtk.Property.set=gdk_dmabuf_texture_builder_set_n_planes)
   *
   * The number of planes of the texture.
   *
   * Note that you can set properties for other planes,
   * but they will be ignored when constructing the texture.
   *
   * Since: 4.14
   */
  properties[PROP_N_PLANES] =
    g_param_spec_uint ("n-planes", NULL, NULL,
                       1, GDK_DMABUF_MAX_PLANES, 1,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDmabufTextureBuilder:update-region: (attributes org.gtk.Property.get=gdk_dmabuf_texture_builder_get_update_region org.gtk.Property.set=gdk_dmabuf_texture_builder_set_update_region)
   *
   * The update region for [property@Gdk.GLTextureBuilder:update-texture].
   *
   * Since: 4.14
   */
  properties[PROP_UPDATE_REGION] =
    g_param_spec_boxed ("update-region", NULL, NULL,
                        CAIRO_GOBJECT_TYPE_REGION,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkDmabufTextureBuilder:update-texture: (attributes org.gtk.Property.get=gdk_dmabuf_texture_builder_get_update_texture org.gtk.Property.set=gdk_dmabuf_texture_builder_set_update_texture)
   *
   * The texture [property@Gdk.DmabufTextureBuilder:update-region] is an update for.
   *
   * Since: 4.14
   */
  properties[PROP_UPDATE_TEXTURE] =
    g_param_spec_object ("update-texture", NULL, NULL,
                         GDK_TYPE_TEXTURE,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gdk_dmabuf_texture_builder_init (GdkDmabufTextureBuilder *self)
{
  self->premultiplied = TRUE;
  self->display = gdk_display_get_default ();
  self->dmabuf.n_planes = 1;

  for (int i = 0; i < GDK_DMABUF_MAX_PLANES; i++)
    self->dmabuf.planes[i].fd = -1;
}

/**
 * gdk_dmabuf_texture_builder_new: (constructor):
 *
 * Creates a new texture builder.
 *
 * Returns: the new `GdkTextureBuilder`
 *
 * Since: 4.14
 **/
GdkDmabufTextureBuilder *
gdk_dmabuf_texture_builder_new (void)
{
  return g_object_new (GDK_TYPE_DMABUF_TEXTURE_BUILDER, NULL);
}

/**
 * gdk_dmabuf_texture_builder_get_display:
 * @self: a `GdkDmabufTextureBuilder
 *
 * Returns the display that this texture builder is
 * associated with.
 *
 * Returns: (transfer none): the display
 *
 * Since: 4.14
 */
GdkDisplay *
gdk_dmabuf_texture_builder_get_display (GdkDmabufTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self), NULL);

  return self->display;
}

/**
 * gdk_dmabuf_texture_builder_set_display:
 * @self: a `GdkDmabufTextureBuilder
 * @display: the display
 *
 * Sets the display that this texture builder is
 * associated with.
 *
 * The display is used to determine the supported
 * dma-buf formats.
 *
 * Since: 4.14
 */
void
gdk_dmabuf_texture_builder_set_display (GdkDmabufTextureBuilder *self,
                                        GdkDisplay              *display)
{
  g_return_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self));
  g_return_if_fail (GDK_IS_DISPLAY (display));

  if (g_set_object (&self->display, display))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DISPLAY]);
}

/**
 * gdk_dmabuf_texture_builder_get_width: (attributes org.gtk.Method.get_property=width)
 * @self: a `GdkDmabufTextureBuilder`
 *
 * Gets the width previously set via gdk_dmabuf_texture_builder_set_width() or
 * 0 if the width wasn't set.
 *
 * Returns: The width
 *
 * Since: 4.14
 */
unsigned int
gdk_dmabuf_texture_builder_get_width (GdkDmabufTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self), 0);

  return self->width;
}

/**
 * gdk_dmabuf_texture_builder_set_width: (attributes org.gtk.Method.set_property=width)
 * @self: a `GdkDmabufTextureBuilder`
 * @width: The texture's width or 0 to unset
 *
 * Sets the width of the texture.
 *
 * The width must be set before calling [method@Gdk.GLTextureBuilder.build].
 *
 * Since: 4.14
 */
void
gdk_dmabuf_texture_builder_set_width (GdkDmabufTextureBuilder *self,
                                      unsigned int             width)
{
  g_return_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self));

  if (self->width == width)
    return;

  self->width = width;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WIDTH]);
}

/**
 * gdk_dmabuf_texture_builder_get_height: (attributes org.gtk.Method.get_property=height)
 * @self: a `GdkDmabufTextureBuilder`
 *
 * Gets the height previously set via gdk_dmabuf_texture_builder_set_height() or
 * 0 if the height wasn't set.
 *
 * Returns: The height
 *
 * Since: 4.14
 */
unsigned int
gdk_dmabuf_texture_builder_get_height (GdkDmabufTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self), 0);

  return self->height;
}

/**
 * gdk_dmabuf_texture_builder_set_height: (attributes org.gtk.Method.set_property=height)
 * @self: a `GdkDmabufTextureBuilder`
 * @height: the texture's height or 0 to unset
 *
 * Sets the height of the texture.
 *
 * The height must be set before calling [method@Gdk.GLTextureBuilder.build].
 *
 * Since: 4.14
 */
void
gdk_dmabuf_texture_builder_set_height (GdkDmabufTextureBuilder *self,
                                       unsigned int             height)
{
  g_return_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self));

  if (self->height == height)
    return;

  self->height = height;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HEIGHT]);
}

/**
 * gdk_dmabuf_texture_builder_get_fourcc: (attributes org.gtk.Method.get_property=fourcc)
 * @self: a `GdkDmabufTextureBuilder`
 *
 * Gets the format previously set via gdk_dmabuf_texture_builder_set_fourcc()
 * or 0 if the format wasn't set.
 *
 * The format is specified as a fourcc code.
 *
 * Returns: The format
 *
 * Since: 4.14
 */
guint32
gdk_dmabuf_texture_builder_get_fourcc (GdkDmabufTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self), 0);

  return self->dmabuf.fourcc;
}

/**
 * gdk_dmabuf_texture_builder_set_fourcc: (attributes org.gtk.Method.set_property=fourcc)
 * @self: a `GdkDmabufTextureBuilder`
 * @fourcc: the texture's format or 0 to unset
 *
 * Sets the format of the texture.
 *
 * The format is specified as a fourcc code.
 *
 * The format must be set before calling [method@Gdk.GLTextureBuilder.build].
 *
 * Since: 4.14
 */
void
gdk_dmabuf_texture_builder_set_fourcc (GdkDmabufTextureBuilder *self,
                                       guint32                  fourcc)
{
  g_return_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self));

  if (self->dmabuf.fourcc == fourcc)
    return;

  self->dmabuf.fourcc = fourcc;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FOURCC]);
}

/**
 * gdk_dmabuf_texture_builder_get_modifier:
 * @self: a `GdkDmabufTextureBuilder`
 *
 * Gets the modifier value.
 *
 * Returns: the modifier
 *
 * Since: 4.14
 */
guint64
gdk_dmabuf_texture_builder_get_modifier (GdkDmabufTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self), 0);

  return self->dmabuf.modifier;
}

/**
 * gdk_dmabuf_texture_builder_set_modifier:
 * @self: a `GdkDmabufTextureBuilder`
 * @modifier: the modifier value
 *
 * Sets the modifier.
 *
 * Since: 4.14
 */
void
gdk_dmabuf_texture_builder_set_modifier (GdkDmabufTextureBuilder *self,
                                         guint64                  modifier)
{
  g_return_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self));

  if (self->dmabuf.modifier == modifier)
    return;

  self->dmabuf.modifier = modifier;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODIFIER]);
}

/**
 * gdk_dmabuf_texture_builder_get_n_planes: (attributes org.gtk.Method.get_property=n-planes)
 * @self: a `GdkDmabufTextureBuilder`
 *
 * Gets the number of planes.
 *
 * Returns: The number of planes
 *
 * Since: 4.14
 */
unsigned int
gdk_dmabuf_texture_builder_get_n_planes (GdkDmabufTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self), 0);

  return self->dmabuf.n_planes;
}

/**
 * gdk_dmabuf_texture_builder_get_premultiplied:
 * @self: a `GdkDmabufTextureBuilder`
 *
 * Whether the data is premultiplied.
 *
 * Returns: whether the data is premultiplied
 *
 * Since: 4.14
 */
gboolean
gdk_dmabuf_texture_builder_get_premultiplied (GdkDmabufTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self), TRUE);

  return self->premultiplied;
}

/**
 * gdk_dmabuf_texture_builder_set_premultiplied:
 * @self: a `GdkDmabufTextureBuilder`
 * @premultiplied: whether the data is premultiplied
 *
 * Sets whether the data is premultiplied.
 *
 * Unless otherwise specified, all formats including alpha channels are assumed
 * to be premultiplied.
 *
 * Since: 4.14
 */
void
gdk_dmabuf_texture_builder_set_premultiplied (GdkDmabufTextureBuilder *self,
                                              gboolean                 premultiplied)
{
  g_return_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self));

  if (self->premultiplied == premultiplied)
    return;

  self->premultiplied = premultiplied;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PREMULTIPLIED]);
}

/**
 * gdk_dmabuf_texture_builder_set_n_planes: (attributes org.gtk.Method.set_property=n-planes)
 * @self: a `GdkDmabufTextureBuilder`
 * @n_planes: the number of planes
 *
 * Sets the number of planes of the texture.
 *
 * Since: 4.14
 */
void
gdk_dmabuf_texture_builder_set_n_planes (GdkDmabufTextureBuilder *self,
                                         unsigned int             n_planes)
{
  g_return_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self));
  g_return_if_fail (n_planes > 0 && n_planes <= GDK_DMABUF_MAX_PLANES);

  if (self->dmabuf.n_planes == n_planes)
    return;

  self->dmabuf.n_planes = n_planes;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_PLANES]);
}

/**
 * gdk_dmabuf_texture_builder_get_fd:
 * @self: a `GdkDmabufTextureBuilder`
 * @plane: the plane to get the fd for
 *
 * Gets the file descriptor for a plane.
 *
 * Returns: the file descriptor
 *
 * Since: 4.14
 */
int
gdk_dmabuf_texture_builder_get_fd (GdkDmabufTextureBuilder *self,
                                   unsigned int             plane)
{
  g_return_val_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self), 0);
  g_return_val_if_fail (0 <= plane && plane < GDK_DMABUF_MAX_PLANES, 0);

  return self->dmabuf.planes[plane].fd;
}

/**
 * gdk_dmabuf_texture_builder_set_fd:
 * @self: a `GdkDmabufTextureBuilder`
 * @plane: the plane to set the fd for
 * @fd: the file descriptor
 *
 * Sets the file descriptor for a plane.
 *
 * Since: 4.14
 */
void
gdk_dmabuf_texture_builder_set_fd (GdkDmabufTextureBuilder *self,
                                   unsigned int             plane,
                                   int                      fd)
{
  g_return_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self));
  g_return_if_fail (0 <= plane && plane < GDK_DMABUF_MAX_PLANES);

  if (self->dmabuf.planes[plane].fd == fd)
    return;

  self->dmabuf.planes[plane].fd = fd;
}

/**
 * gdk_dmabuf_texture_builder_get_stride:
 * @self: a `GdkDmabufTextureBuilder`
 * @plane: the plane to get the stride for
 *
 * Gets the stride value for a plane.
 *
 * Returns: the stride
 *
 * Since: 4.14
 */
unsigned int
gdk_dmabuf_texture_builder_get_stride (GdkDmabufTextureBuilder *self,
                                       unsigned int             plane)
{
  g_return_val_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self), 0);
  g_return_val_if_fail (0 <= plane && plane < GDK_DMABUF_MAX_PLANES, 0);

  return self->dmabuf.planes[plane].stride;
}

/**
 * gdk_dmabuf_texture_builder_set_stride:
 * @self: a `GdkDmabufTextureBuilder`
 * @plane: the plane to set the stride for
 * @stride: the stride value
 *
 * Sets the stride for a plane.
 *
 * The stride must be set for all planes before calling [method@Gdk.GLTextureBuilder.build].
 *
 * Since: 4.14
 */
void
gdk_dmabuf_texture_builder_set_stride (GdkDmabufTextureBuilder *self,
                                       unsigned int             plane,
                                       unsigned int             stride)
{
  g_return_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self));
  g_return_if_fail (0 <= plane && plane < GDK_DMABUF_MAX_PLANES);

  if (self->dmabuf.planes[plane].stride == stride)
    return;

  self->dmabuf.planes[plane].stride = stride;
}

/**
 * gdk_dmabuf_texture_builder_get_offset:
 * @self: a `GdkDmabufTextureBuilder`
 * @plane: the plane to get the offset for
 *
 * Gets the offset value for a plane.
 *
 * Returns: the offset
 *
 * Since: 4.14
 */
unsigned int
gdk_dmabuf_texture_builder_get_offset (GdkDmabufTextureBuilder *self,
                                       unsigned int             plane)
{
  g_return_val_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self), 0);
  g_return_val_if_fail (0 <= plane && plane < GDK_DMABUF_MAX_PLANES, 0);

  return self->dmabuf.planes[plane].offset;
}

/**
 * gdk_dmabuf_texture_builder_set_offset:
 * @self: a `GdkDmabufTextureBuilder`
 * @plane: the plane to set the offset for
 * @offset: the offset value
 *
 * Sets the offset for a plane.
 *
 * Since: 4.14
 */
void
gdk_dmabuf_texture_builder_set_offset (GdkDmabufTextureBuilder *self,
                                       unsigned int             plane,
                                       unsigned int             offset)
{
  g_return_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self));
  g_return_if_fail (0 <= plane && plane < GDK_DMABUF_MAX_PLANES);

  if (self->dmabuf.planes[plane].offset == offset)
    return;

  self->dmabuf.planes[plane].offset = offset;
}

/**
 * gdk_dmabuf_texture_builder_get_update_texture: (attributes org.gtk.Method.get_property=update-texture)
 * @self: a `GdkDmabufTextureBuilder`
 *
 * Gets the texture previously set via gdk_dmabuf_texture_builder_set_update_texture() or
 * %NULL if none was set.
 *
 * Returns: (transfer none) (nullable): The texture
 *
 * Since: 4.14
 */
GdkTexture *
gdk_dmabuf_texture_builder_get_update_texture (GdkDmabufTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self), NULL);

  return self->update_texture;
}

/**
 * gdk_dmabuf_texture_builder_set_update_texture: (attributes org.gtk.Method.set_property=update-texture)
 * @self: a `GdkDmabufTextureBuilder`
 * @texture: (nullable): the texture to update
 *
 * Sets the texture to be updated by this texture. See
 * [method@Gdk.DmabufTextureBuilder.set_update_region] for an explanation.
 *
 * Since: 4.14
 */
void
gdk_dmabuf_texture_builder_set_update_texture (GdkDmabufTextureBuilder *self,
                                               GdkTexture          *texture)
{
  g_return_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self));
  g_return_if_fail (texture == NULL || GDK_IS_TEXTURE (texture));

  if (!g_set_object (&self->update_texture, texture))
    return;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_UPDATE_TEXTURE]);
}

/**
 * gdk_dmabuf_texture_builder_get_update_region: (attributes org.gtk.Method.get_property=update-region)
 * @self: a `GdkDmabufTextureBuilder`
 *
 * Gets the region previously set via gdk_dmabuf_texture_builder_set_update_region() or
 * %NULL if none was set.
 *
 * Returns: (transfer none) (nullable): The region
 *
 * Since: 4.14
 */
cairo_region_t *
gdk_dmabuf_texture_builder_get_update_region (GdkDmabufTextureBuilder *self)
{
  g_return_val_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self), NULL);

  return self->update_region;
}

/**
 * gdk_dmabuf_texture_builder_set_update_region: (attributes org.gtk.Method.set_property=update-region)
 * @self: a `GdkDmabufTextureBuilder`
 * @region: (nullable): the region to update
 *
 * Sets the region to be updated by this texture. Together with
 * [property@Gdk.DmabufTextureBuilder:update-texture] this describes an
 * update of a previous texture.
 *
 * When rendering animations of large textures, it is possible that
 * consecutive textures are only updating contents in parts of the texture.
 * It is then possible to describe this update via these two properties,
 * so that GTK can avoid rerendering parts that did not change.
 *
 * An example would be a screen recording where only the mouse pointer moves.
 *
 * Since: 4.14
 */
void
gdk_dmabuf_texture_builder_set_update_region (GdkDmabufTextureBuilder *self,
                                              cairo_region_t      *region)
{
  g_return_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self));

  if (self->update_region == region)
    return;

  g_clear_pointer (&self->update_region, cairo_region_destroy);

  if (region)
    self->update_region = cairo_region_reference (region);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_UPDATE_REGION]);
}

/**
 * gdk_dmabuf_texture_builder_build:
 * @self: a `GdkDmabufTextureBuilder`
 * @destroy: (nullable): destroy function to be called when the texture is
 *   released
 * @data: user data to pass to the destroy function
 * @error: Return location for an error
 *
 * Builds a new `GdkTexture` with the values set up in the builder.
 *
 * It is a programming error to call this function if any mandatory
 * property has not been set.
 *
 * If the dmabuf is not supported by GTK, %NULL will be returned and @error will be set.
 *
 * The `destroy` function gets called when the returned texture gets released.
 *
 * It is possible to call this function multiple times to create multiple textures,
 * possibly with changing properties in between.
 *
 * It is the responsibility of the caller to keep the file descriptors for the planes
 * open until the created texture is no longer used, and close them afterwards (possibly
 * using the @destroy notify).
 *
 * Not all formats defined in the `drm_fourcc.h` header are supported. You can use
 * [method@Gdk.Display.get_dmabuf_formats] to get a list of supported formats.
 *
 * Returns: (transfer full) (nullable): a newly built `GdkTexture` or `NULL`
 *   if the format is not supported
 *
 * Since: 4.14
 */
GdkTexture *
gdk_dmabuf_texture_builder_build (GdkDmabufTextureBuilder *self,
                                  GDestroyNotify           destroy,
                                  gpointer                 data,
                                  GError                 **error)
{
  unsigned i;

  g_return_val_if_fail (GDK_IS_DMABUF_TEXTURE_BUILDER (self), NULL);
  g_return_val_if_fail (destroy == NULL || data != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  g_return_val_if_fail (self->width > 0, NULL);
  g_return_val_if_fail (self->height > 0, NULL);
  g_return_val_if_fail (self->dmabuf.fourcc != 0, NULL);

  for (i = 0; i < self->dmabuf.n_planes; i++)
    g_return_val_if_fail (self->dmabuf.planes[i].fd != -1, NULL);

  if (GDK_DISPLAY_DEBUG_CHECK (self->display, DMABUF_DISABLE))
    {
      g_set_error_literal (error,
                           GDK_DMABUF_ERROR, GDK_DMABUF_ERROR_NOT_AVAILABLE,
                           "dmabuf support disabled via GDK_DEBUG environment variable");
      return NULL;
    }

  return gdk_dmabuf_texture_new_from_builder (self, destroy, data, error);
}

const GdkDmabuf *
gdk_dmabuf_texture_builder_get_dmabuf (GdkDmabufTextureBuilder *self)
{
  return &self->dmabuf;
}
