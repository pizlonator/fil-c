/* gdktexture.c
 *
 * Copyright 2016  Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * GdkTexture:
 *
 * `GdkTexture` is the basic element used to refer to pixel data.
 *
 * It is primarily meant for pixel data that will not change over
 * multiple frames, and will be used for a long time.
 *
 * There are various ways to create `GdkTexture` objects from a
 * [class@GdkPixbuf.Pixbuf], or from bytes stored in memory, a file, or a
 * [struct@Gio.Resource].
 *
 * The ownership of the pixel data is transferred to the `GdkTexture`
 * instance; you can only make a copy of it, via [method@Gdk.Texture.download].
 *
 * `GdkTexture` is an immutable object: That means you cannot change
 * anything about it other than increasing the reference count via
 * [method@GObject.Object.ref], and consequently, it is a thread-safe object.
 */

#include "config.h"

#include "gdktextureprivate.h"

#include <glib/gi18n-lib.h>
#include "gdkmemorytextureprivate.h"
#include "gdkpaintable.h"
#include "gdksnapshot.h"

#include <graphene.h>
#include "loaders/gdkpngprivate.h"
#include "loaders/gdktiffprivate.h"
#include "loaders/gdkjpegprivate.h"

G_DEFINE_QUARK (gdk-texture-error-quark, gdk_texture_error)

/* HACK: So we don't need to include any (not-yet-created) GSK or GTK headers */
void
gtk_snapshot_append_texture (GdkSnapshot            *snapshot,
                             GdkTexture             *texture,
                             const graphene_rect_t  *bounds);

enum {
  PROP_0,
  PROP_WIDTH,
  PROP_HEIGHT,

  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static GdkTextureChain *
gdk_texture_chain_new (void)
{
  GdkTextureChain *chain = g_new0 (GdkTextureChain, 1);

  g_atomic_ref_count_init (&chain->ref_count);
  g_mutex_init (&chain->lock);

  return chain;
}

static void
gdk_texture_chain_ref (GdkTextureChain *chain)
{
  g_atomic_ref_count_inc (&chain->ref_count);
}

static void
gdk_texture_chain_unref (GdkTextureChain *chain)
{
  if (g_atomic_ref_count_dec (&chain->ref_count))
    {
      g_mutex_clear (&chain->lock);
      g_free (chain);
    }
}

static void
gdk_texture_paintable_snapshot (GdkPaintable *paintable,
                                GdkSnapshot  *snapshot,
                                double        width,
                                double        height)
{
  GdkTexture *self = GDK_TEXTURE (paintable);

  gtk_snapshot_append_texture (snapshot,
                               self,
                               &GRAPHENE_RECT_INIT (0, 0, width, height));
}

static GdkPaintableFlags
gdk_texture_paintable_get_flags (GdkPaintable *paintable)
{
  return GDK_PAINTABLE_STATIC_SIZE
       | GDK_PAINTABLE_STATIC_CONTENTS;
}

static int
gdk_texture_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GdkTexture *self = GDK_TEXTURE (paintable);

  return self->width;
}

static int
gdk_texture_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GdkTexture *self = GDK_TEXTURE (paintable);

  return self->height;
}

static void
gdk_texture_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gdk_texture_paintable_snapshot;
  iface->get_flags = gdk_texture_paintable_get_flags;
  iface->get_intrinsic_width = gdk_texture_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = gdk_texture_paintable_get_intrinsic_height;
}

static GVariant *
gdk_texture_icon_serialize (GIcon *icon)
{
  GVariant *result;
  GBytes *bytes;

  bytes = gdk_texture_save_to_png_bytes (GDK_TEXTURE (icon));
  result = g_variant_new_from_bytes (G_VARIANT_TYPE_BYTESTRING, bytes, TRUE);
  g_bytes_unref (bytes);

  return g_variant_new ("(sv)", "bytes", result);
}

static void
gdk_texture_icon_init (GIconIface *iface)
{
  iface->hash = (guint (*) (GIcon *)) g_direct_hash;
  iface->equal = (gboolean (*) (GIcon *, GIcon *)) g_direct_equal;
  iface->serialize = gdk_texture_icon_serialize;
}

static GInputStream *
gdk_texture_loadable_icon_load (GLoadableIcon  *icon,
                                int             size,
                                char          **type,
                                GCancellable   *cancellable,
                                GError        **error)
{
  GInputStream *stream;
  GBytes *bytes;

  bytes = gdk_texture_save_to_png_bytes (GDK_TEXTURE (icon));
  stream = g_memory_input_stream_new_from_bytes (bytes);
  g_bytes_unref (bytes);

  if (type)
    *type = NULL;

  return stream;
}

static void
gdk_texture_loadable_icon_load_in_thread (GTask        *task,
                                          gpointer      source_object,
                                          gpointer      task_data,
                                          GCancellable *cancellable)
{
  GInputStream *stream;
  GBytes *bytes;

  bytes = gdk_texture_save_to_png_bytes (source_object);
  stream = g_memory_input_stream_new_from_bytes (bytes);
  g_bytes_unref (bytes);
  g_task_return_pointer (task, stream, g_object_unref);
}

static void
gdk_texture_loadable_icon_load_async (GLoadableIcon       *icon,
                                      int                  size,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  GTask *task;

  task = g_task_new (icon, cancellable, callback, user_data);
  g_task_run_in_thread (task, gdk_texture_loadable_icon_load_in_thread);
  g_object_unref (task);
}

static GInputStream *
gdk_texture_loadable_icon_load_finish (GLoadableIcon  *icon,
                                       GAsyncResult   *res,
                                       char          **type,
                                       GError        **error)
{
  GInputStream *result;

  g_return_val_if_fail (g_task_is_valid (res, icon), NULL);

  result = g_task_propagate_pointer (G_TASK (res), error);
  if (result == NULL)
    return NULL;

  if (type)
    *type = NULL;

  return result;
}

static void
gdk_texture_loadable_icon_init (GLoadableIconIface *iface)
{
  iface->load = gdk_texture_loadable_icon_load;
  iface->load_async = gdk_texture_loadable_icon_load_async;
  iface->load_finish = gdk_texture_loadable_icon_load_finish;
}

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GdkTexture, gdk_texture, G_TYPE_OBJECT,
                                  G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                         gdk_texture_paintable_init)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_ICON,
                                                         gdk_texture_icon_init)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_LOADABLE_ICON, gdk_texture_loadable_icon_init))

#define GDK_TEXTURE_WARN_NOT_IMPLEMENTED_METHOD(obj,method) \
  g_critical ("Texture of type '%s' does not implement GdkTexture::" # method, G_OBJECT_TYPE_NAME (obj))

static void
gdk_texture_default_download (GdkTexture      *texture,
                              GdkMemoryFormat  format,
                              guchar          *data,
                              gsize            stride)
{
  GDK_TEXTURE_WARN_NOT_IMPLEMENTED_METHOD (texture, download);
}

static void
gdk_texture_set_property (GObject      *gobject,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GdkTexture *self = GDK_TEXTURE (gobject);

  switch (prop_id)
    {
    case PROP_WIDTH:
      self->width = g_value_get_int (value);
      break;

    case PROP_HEIGHT:
      self->height = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gdk_texture_get_property (GObject    *gobject,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GdkTexture *self = GDK_TEXTURE (gobject);

  switch (prop_id)
    {
    case PROP_WIDTH:
      g_value_set_int (value, self->width);
      break;

    case PROP_HEIGHT:
      g_value_set_int (value, self->height);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gdk_texture_dispose (GObject *object)
{
  GdkTexture *self = GDK_TEXTURE (object);

  if (self->chain)
    {
      g_mutex_lock (&self->chain->lock);

      if (self->next_texture && self->previous_texture)
        {
          cairo_region_union (self->next_texture->diff_to_previous,
                              self->diff_to_previous);
          self->next_texture->previous_texture = self->previous_texture;
          self->previous_texture->next_texture = self->next_texture;
        }
      else if (self->next_texture)
        self->next_texture->previous_texture = NULL;
      else if (self->previous_texture)
        self->previous_texture->next_texture = NULL;

      self->next_texture = NULL;
      self->previous_texture = NULL;
      g_clear_pointer (&self->diff_to_previous, cairo_region_destroy);

      g_mutex_unlock (&self->chain->lock);

      g_clear_pointer (&self->chain, gdk_texture_chain_unref);
    }

  gdk_texture_clear_render_data (self);

  G_OBJECT_CLASS (gdk_texture_parent_class)->dispose (object);
}

static void
gdk_texture_class_init (GdkTextureClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  klass->download = gdk_texture_default_download;

  gobject_class->set_property = gdk_texture_set_property;
  gobject_class->get_property = gdk_texture_get_property;
  gobject_class->dispose = gdk_texture_dispose;

  /**
   * GdkTexture:width: (attributes org.gtk.Property.get=gdk_texture_get_width)
   *
   * The width of the texture, in pixels.
   */
  properties[PROP_WIDTH] =
    g_param_spec_int ("width", NULL, NULL,
                      1,
                      G_MAXINT,
                      1,
                      G_PARAM_READWRITE |
                      G_PARAM_CONSTRUCT_ONLY |
                      G_PARAM_STATIC_STRINGS |
                      G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GdkTexture:height: (attributes org.gtk.Property.get=gdk_texture_get_height)
   *
   * The height of the texture, in pixels.
   */
  properties[PROP_HEIGHT] =
    g_param_spec_int ("height", NULL, NULL,
                      1,
                      G_MAXINT,
                      1,
                      G_PARAM_READWRITE |
                      G_PARAM_CONSTRUCT_ONLY |
                      G_PARAM_STATIC_STRINGS |
                      G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gdk_texture_init (GdkTexture *self)
{
}

/**
 * gdk_texture_new_for_surface:
 * @surface: a cairo image surface
 *
 * Creates a new texture object representing the surface.
 *
 * The @surface must be an image surface with format `CAIRO_FORMAT_ARGB32`.
 *
 * The newly created texture will acquire a reference on the @surface.
 *
 * Returns: a new `GdkTexture`
 */
GdkTexture *
gdk_texture_new_for_surface (cairo_surface_t *surface)
{
  GdkTexture *texture;
  GBytes *bytes;

  g_return_val_if_fail (cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_IMAGE, NULL);
  g_return_val_if_fail (cairo_image_surface_get_width (surface) > 0, NULL);
  g_return_val_if_fail (cairo_image_surface_get_height (surface) > 0, NULL);

  bytes = g_bytes_new_with_free_func (cairo_image_surface_get_data (surface),
                                      cairo_image_surface_get_height (surface)
                                      * cairo_image_surface_get_stride (surface),
                                      (GDestroyNotify) cairo_surface_destroy,
                                      cairo_surface_reference (surface));

  texture = gdk_memory_texture_new (cairo_image_surface_get_width (surface),
                                    cairo_image_surface_get_height (surface),
                                    GDK_MEMORY_DEFAULT,
                                    bytes,
                                    cairo_image_surface_get_stride (surface));

  g_bytes_unref (bytes);

  return texture;
}

/**
 * gdk_texture_new_for_pixbuf:
 * @pixbuf: a `GdkPixbuf`
 *
 * Creates a new texture object representing the `GdkPixbuf`.
 *
 * This function is threadsafe, so that you can e.g. use GTask
 * and [method@Gio.Task.run_in_thread] to avoid blocking the main thread
 * while loading a big image.
 *
 * Returns: a new `GdkTexture`
 */
GdkTexture *
gdk_texture_new_for_pixbuf (GdkPixbuf *pixbuf)
{
  GdkTexture *texture;
  GBytes *bytes;

  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

  bytes = g_bytes_new_with_free_func (gdk_pixbuf_get_pixels (pixbuf),
                                      gdk_pixbuf_get_height (pixbuf)
                                      * (gsize) gdk_pixbuf_get_rowstride (pixbuf),
                                      g_object_unref,
                                      g_object_ref (pixbuf));
  texture = gdk_memory_texture_new (gdk_pixbuf_get_width (pixbuf),
                                    gdk_pixbuf_get_height (pixbuf),
                                    gdk_pixbuf_get_has_alpha (pixbuf)
                                    ? GDK_MEMORY_GDK_PIXBUF_ALPHA
                                    : GDK_MEMORY_GDK_PIXBUF_OPAQUE,
                                    bytes,
                                    gdk_pixbuf_get_rowstride (pixbuf));

  g_bytes_unref (bytes);

  return texture;
}

/**
 * gdk_texture_new_from_resource:
 * @resource_path: the path of the resource file
 *
 * Creates a new texture by loading an image from a resource.
 *
 * The file format is detected automatically. The supported formats
 * are PNG and JPEG, though more formats might be available.
 *
 * It is a fatal error if @resource_path does not specify a valid
 * image resource and the program will abort if that happens.
 * If you are unsure about the validity of a resource, use
 * [ctor@Gdk.Texture.new_from_file] to load it.
 *
 * This function is threadsafe, so that you can e.g. use GTask
 * and [method@Gio.Task.run_in_thread] to avoid blocking the main thread
 * while loading a big image.
 *
 * Return value: A newly-created `GdkTexture`
 */
GdkTexture *
gdk_texture_new_from_resource (const char *resource_path)
{
  GBytes *bytes;
  GdkTexture *texture;
  GError *error = NULL;

  g_return_val_if_fail (resource_path != NULL, NULL);

  bytes = g_resources_lookup_data (resource_path, 0, &error);
  if (bytes != NULL)
    {
      texture = gdk_texture_new_from_bytes (bytes, &error);
      g_bytes_unref (bytes);
    }
  else
    texture = NULL;

  if (texture == NULL)
    g_error ("Resource path %s is not a valid image: %s", resource_path, error->message);

  return texture;
}

/**
 * gdk_texture_new_from_file:
 * @file: `GFile` to load
 * @error: Return location for an error
 *
 * Creates a new texture by loading an image from a file.
 *
 * The file format is detected automatically. The supported formats
 * are PNG, JPEG and TIFF, though more formats might be available.
 *
 * If %NULL is returned, then @error will be set.
 *
 * This function is threadsafe, so that you can e.g. use GTask
 * and [method@Gio.Task.run_in_thread] to avoid blocking the main thread
 * while loading a big image.
 *
 * Return value: A newly-created `GdkTexture`
 */
GdkTexture *
gdk_texture_new_from_file (GFile   *file,
                           GError **error)
{
  GBytes *bytes;
  GdkTexture *texture;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  bytes = g_file_load_bytes (file, NULL, NULL, error);
  if (bytes == NULL)
    return NULL;

  texture = gdk_texture_new_from_bytes (bytes, error);

  g_bytes_unref (bytes);

  return texture;
}

gboolean
gdk_texture_can_load (GBytes *bytes)
{
  return gdk_is_png (bytes) ||
         gdk_is_jpeg (bytes) ||
         gdk_is_tiff (bytes);
}

static GdkTexture *
gdk_texture_new_from_bytes_internal (GBytes  *bytes,
                                     GError **error)
{
  if (gdk_is_png (bytes))
    {
      return gdk_load_png (bytes, NULL, error);
    }
  else if (gdk_is_jpeg (bytes))
    {
      return gdk_load_jpeg (bytes, error);
    }
  else if (gdk_is_tiff (bytes))
    {
      return gdk_load_tiff (bytes, error);
    }
  else
    {
      g_set_error_literal (error,
                           GDK_TEXTURE_ERROR, GDK_TEXTURE_ERROR_UNSUPPORTED_FORMAT,
                           _("Unknown image format."));
      return NULL;
    }
}

static GdkTexture *
gdk_texture_new_from_bytes_pixbuf (GBytes  *bytes,
                                   GError **error)
{
  GInputStream *stream;
  GdkPixbuf *pixbuf;
  GdkTexture *texture;

  stream = g_memory_input_stream_new_from_bytes (bytes);
  pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, error);
  g_object_unref (stream);
  if (pixbuf == NULL)
    return NULL;

  texture = gdk_texture_new_for_pixbuf (pixbuf);
  g_object_unref (pixbuf);

  return texture;
}


/**
 * gdk_texture_new_from_bytes:
 * @bytes: a `GBytes` containing the data to load
 * @error: Return location for an error
 *
 * Creates a new texture by loading an image from memory,
 *
 * The file format is detected automatically. The supported formats
 * are PNG, JPEG and TIFF, though more formats might be available.
 *
 * If %NULL is returned, then @error will be set.
 *
 * This function is threadsafe, so that you can e.g. use GTask
 * and [method@Gio.Task.run_in_thread] to avoid blocking the main thread
 * while loading a big image.
 *
 * Return value: A newly-created `GdkTexture`
 *
 * Since: 4.6
 */
GdkTexture *
gdk_texture_new_from_bytes (GBytes  *bytes,
                            GError **error)
{
  GdkTexture *texture;
  GError *internal_error = NULL;

  g_return_val_if_fail (bytes != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  texture = gdk_texture_new_from_bytes_internal (bytes, &internal_error);
  if (texture)
    return texture;

  if (!g_error_matches (internal_error, GDK_TEXTURE_ERROR, GDK_TEXTURE_ERROR_UNSUPPORTED_CONTENT) &&
      !g_error_matches (internal_error, GDK_TEXTURE_ERROR, GDK_TEXTURE_ERROR_UNSUPPORTED_FORMAT))
    {
      g_propagate_error (error, internal_error);
      return NULL;
    }

  g_clear_error (&internal_error);

  return gdk_texture_new_from_bytes_pixbuf (bytes, error);
}

/**
 * gdk_texture_new_from_filename:
 * @path: (type filename): the filename to load
 * @error: Return location for an error
 *
 * Creates a new texture by loading an image from a file.
 *
 * The file format is detected automatically. The supported formats
 * are PNG, JPEG and TIFF, though more formats might be available.
 *
 * If %NULL is returned, then @error will be set.
 *
 * This function is threadsafe, so that you can e.g. use GTask
 * and [method@Gio.Task.run_in_thread] to avoid blocking the main thread
 * while loading a big image.
 *
 * Return value: A newly-created `GdkTexture`
 *
 * Since: 4.6
 */
GdkTexture *
gdk_texture_new_from_filename (const char  *path,
                               GError     **error)
{
  GdkTexture *texture;
  GFile *file;

  g_return_val_if_fail (path, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  file = g_file_new_for_path (path);

  texture = gdk_texture_new_from_file (file, error);

  g_object_unref (file);

  return texture;
}

/**
 * gdk_texture_get_width: (attributes org.gtk.Method.get_property=width)
 * @texture: a `GdkTexture`
 *
 * Returns the width of @texture, in pixels.
 *
 * Returns: the width of the `GdkTexture`
 */
int
gdk_texture_get_width (GdkTexture *texture)
{
  g_return_val_if_fail (GDK_IS_TEXTURE (texture), 0);

  return texture->width;
}

/**
 * gdk_texture_get_height: (attributes org.gtk.Method.get_property=height)
 * @texture: a `GdkTexture`
 *
 * Returns the height of the @texture, in pixels.
 *
 * Returns: the height of the `GdkTexture`
 */
int
gdk_texture_get_height (GdkTexture *texture)
{
  g_return_val_if_fail (GDK_IS_TEXTURE (texture), 0);

  return texture->height;
}

void
gdk_texture_do_download (GdkTexture      *texture,
                         GdkMemoryFormat  format,
                         guchar          *data,
                         gsize            stride)
{
  GDK_TEXTURE_GET_CLASS (texture)->download (texture, format, data, stride);
}

static gboolean
gdk_texture_has_ancestor (GdkTexture *self,
                          GdkTexture *other)
{
  GdkTexture *texture;

  for (texture = self->previous_texture;
       texture != NULL;
       texture = texture->previous_texture)
    {
      if (texture == other)
        return TRUE;
    }
  return FALSE;
}

static void
gdk_texture_diff_from_known_ancestor (GdkTexture     *self,
                                      GdkTexture     *ancestor,
                                      cairo_region_t *region)
{
  GdkTexture *texture;

  for (texture = self; texture != ancestor; texture = texture->previous_texture)
    cairo_region_union (region, texture->diff_to_previous);
}

void
gdk_texture_diff (GdkTexture     *self,
                  GdkTexture     *other,
                  cairo_region_t *region)
{
  cairo_rectangle_int_t fill = {
    .x = 0,
    .y = 0,
    .width = MAX (self->width, other->width),
    .height = MAX (self->height, other->height),
  };
  GdkTextureChain *chain;

  if (other == self)
    return;

  chain = g_atomic_pointer_get (&self->chain);
  if (chain == NULL || chain != g_atomic_pointer_get (&other->chain))
    {
      cairo_region_union_rectangle (region, &fill);
      return;
    }

  g_mutex_lock (&chain->lock);
  if (gdk_texture_has_ancestor (self, other))
    gdk_texture_diff_from_known_ancestor (self, other, region);
  else if (gdk_texture_has_ancestor (other, self))
    gdk_texture_diff_from_known_ancestor (other, self, region);
  else
    cairo_region_union_rectangle (region, &fill);
  g_mutex_unlock (&chain->lock);
}

void
gdk_texture_set_diff (GdkTexture     *self,
                      GdkTexture     *previous,
                      cairo_region_t *diff)
{
  g_assert (self->diff_to_previous == NULL);
  g_assert (self->chain == NULL);

  self->chain = g_atomic_pointer_get (&previous->chain);
  if (self->chain == NULL)
    {
      self->chain = gdk_texture_chain_new ();
      if (!g_atomic_pointer_compare_and_exchange (&previous->chain,
                                                  NULL,
                                                  self->chain))
        gdk_texture_chain_unref (self->chain);
      self->chain = previous->chain;
    }
  gdk_texture_chain_ref (self->chain);

  g_mutex_lock (&self->chain->lock);
  if (previous->next_texture)
    {
      previous->next_texture->previous_texture = NULL;
      g_clear_pointer (&previous->next_texture->diff_to_previous, cairo_region_destroy);
    }
  previous->next_texture = self;
  self->previous_texture = previous;
  self->diff_to_previous = diff;
  g_mutex_unlock (&self->chain->lock);
}

cairo_surface_t *
gdk_texture_download_surface (GdkTexture *texture)
{
  cairo_surface_t *surface;
  cairo_status_t surface_status;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        texture->width, texture->height);

  surface_status = cairo_surface_status (surface);
  if (surface_status != CAIRO_STATUS_SUCCESS)
    g_warning ("%s: surface error: %s", __FUNCTION__,
               cairo_status_to_string (surface_status));

  gdk_texture_download (texture,
                        cairo_image_surface_get_data (surface),
                        cairo_image_surface_get_stride (surface));
  cairo_surface_mark_dirty (surface);

  return surface;
}

/**
 * gdk_texture_download:
 * @texture: a `GdkTexture`
 * @data: (array): pointer to enough memory to be filled with the
 *   downloaded data of @texture
 * @stride: rowstride in bytes
 *
 * Downloads the @texture into local memory.
 *
 * This may be an expensive operation, as the actual texture data
 * may reside on a GPU or on a remote display server.
 *
 * The data format of the downloaded data is equivalent to
 * %CAIRO_FORMAT_ARGB32, so every downloaded pixel requires
 * 4 bytes of memory.
 *
 * Downloading a texture into a Cairo image surface:
 * ```c
 * surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
 *                                       gdk_texture_get_width (texture),
 *                                       gdk_texture_get_height (texture));
 * gdk_texture_download (texture,
 *                       cairo_image_surface_get_data (surface),
 *                       cairo_image_surface_get_stride (surface));
 * cairo_surface_mark_dirty (surface);
 * ```
 *
 * For more flexible download capabilities, see
 * [struct@Gdk.TextureDownloader].
 */
void
gdk_texture_download (GdkTexture *texture,
                      guchar     *data,
                      gsize       stride)
{
  g_return_if_fail (GDK_IS_TEXTURE (texture));
  g_return_if_fail (data != NULL);
  g_return_if_fail (stride >= gdk_texture_get_width (texture) * 4);

  gdk_texture_do_download (texture,
                           GDK_MEMORY_DEFAULT,
                           data,
                           stride);
}

/**
 * gdk_texture_get_format:
 * @self: a GdkTexture
 *
 * Gets the memory format most closely associated with the data of
 * the texture.
 *
 * Note that it may not be an exact match for texture data
 * stored on the GPU or with compression.
 *
 * The format can give an indication about the bit depth and opacity
 * of the texture and is useful to determine the best format for
 * downloading the texture.
 *
 * Returns: the preferred format for the texture's data
 *
 * Since: 4.10
 **/
GdkMemoryFormat
gdk_texture_get_format (GdkTexture *self)
{
  g_return_val_if_fail (GDK_IS_TEXTURE (self), GDK_MEMORY_DEFAULT);

  return self->format;
}

gboolean
gdk_texture_set_render_data (GdkTexture     *self,
                             gpointer        key,
                             gpointer        data,
                             GDestroyNotify  notify)
{
  g_return_val_if_fail (data != NULL, FALSE);

  if (self->render_key != NULL)
    return FALSE;

  self->render_key = key;
  self->render_data = data;
  self->render_notify = notify;

  return TRUE;
}

void
gdk_texture_clear_render_data (GdkTexture *self)
{
  if (self->render_notify)
    self->render_notify (self->render_data);

  self->render_key = NULL;
  self->render_data = NULL;
  self->render_notify = NULL;
}

gpointer
gdk_texture_get_render_data (GdkTexture  *self,
                             gpointer     key)
{
  if (self->render_key != key)
    return NULL;

  return self->render_data;
}

/**
 * gdk_texture_save_to_png:
 * @texture: a `GdkTexture`
 * @filename: (type filename): the filename to store to
 *
 * Store the given @texture to the @filename as a PNG file.
 *
 * This is a utility function intended for debugging and testing.
 * If you want more control over formats, proper error handling or
 * want to store to a [iface@Gio.File] or other location, you might want to
 * use [method@Gdk.Texture.save_to_png_bytes] or look into the
 * gdk-pixbuf library.
 *
 * Returns: %TRUE if saving succeeded, %FALSE on failure.
 */
gboolean
gdk_texture_save_to_png (GdkTexture *texture,
                         const char *filename)
{
  GBytes *bytes;
  gboolean result;

  g_return_val_if_fail (GDK_IS_TEXTURE (texture), FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);

  bytes = gdk_save_png (texture);
  result = g_file_set_contents (filename,
                                g_bytes_get_data (bytes, NULL),
                                g_bytes_get_size (bytes),
                                NULL);
  g_bytes_unref (bytes);

  return result;
}

/**
 * gdk_texture_save_to_png_bytes:
 * @texture: a `GdkTexture`
 *
 * Store the given @texture in memory as a PNG file.
 *
 * Use [ctor@Gdk.Texture.new_from_bytes] to read it back.
 *
 * If you want to serialize a texture, this is a convenient and
 * portable way to do that.
 *
 * If you need more control over the generated image, such as
 * attaching metadata, you should look into an image handling
 * library such as the gdk-pixbuf library.
 *
 * If you are dealing with high dynamic range float data, you
 * might also want to consider [method@Gdk.Texture.save_to_tiff_bytes]
 * instead.
 *
 * Returns: a newly allocated `GBytes` containing PNG data
 *
 * Since: 4.6
 */
GBytes *
gdk_texture_save_to_png_bytes (GdkTexture *texture)
{
  g_return_val_if_fail (GDK_IS_TEXTURE (texture), NULL);

  return gdk_save_png (texture);
}

/**
 * gdk_texture_save_to_tiff:
 * @texture: a `GdkTexture`
 * @filename: (type filename): the filename to store to
 *
 * Store the given @texture to the @filename as a TIFF file.
 *
 * GTK will attempt to store data without loss.
 * Returns: %TRUE if saving succeeded, %FALSE on failure.
 *
 * Since: 4.6
 */
gboolean
gdk_texture_save_to_tiff (GdkTexture  *texture,
                          const char  *filename)
{
  GBytes *bytes;
  gboolean result;

  g_return_val_if_fail (GDK_IS_TEXTURE (texture), FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);

  bytes = gdk_save_tiff (texture);
  result = g_file_set_contents (filename,
                                g_bytes_get_data (bytes, NULL),
                                g_bytes_get_size (bytes),
                                NULL);
  g_bytes_unref (bytes);

  return result;
}

/**
 * gdk_texture_save_to_tiff_bytes:
 * @texture: a `GdkTexture`
 *
 * Store the given @texture in memory as a TIFF file.
 *
 * Use [ctor@Gdk.Texture.new_from_bytes] to read it back.
 *
 * This function is intended to store a representation of the
 * texture's data that is as accurate as possible. This is
 * particularly relevant when working with high dynamic range
 * images and floating-point texture data.
 *
 * If that is not your concern and you are interested in a
 * smaller size and a more portable format, you might want to
 * use [method@Gdk.Texture.save_to_png_bytes].
 *
 * Returns: a newly allocated `GBytes` containing TIFF data
 *
 * Since: 4.6
 */
GBytes *
gdk_texture_save_to_tiff_bytes (GdkTexture *texture)
{
  g_return_val_if_fail (GDK_IS_TEXTURE (texture), NULL);

  return gdk_save_tiff (texture);
}
