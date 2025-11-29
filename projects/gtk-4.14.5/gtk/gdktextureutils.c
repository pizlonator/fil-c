/* Copyright (C) 2016 Red Hat, Inc.
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

#include "config.h"

#include <gdk/gdk.h>
#include "gdktextureutilsprivate.h"
#include "gtkscalerprivate.h"

#include "gdk/gdktextureprivate.h"
#include "gdk/loaders/gdkpngprivate.h"

/* {{{ Pixbuf helpers */

static inline gboolean
pixbuf_is_only_fg (GdkPixbuf *pixbuf)
{
  return gdk_pixbuf_get_option (pixbuf, "tEXt::only-foreground") != NULL;
}

static GdkPixbuf *
load_from_stream (GdkPixbufLoader  *loader,
                  GInputStream     *stream,
                  GCancellable     *cancellable,
                  GError          **error)
{
  GdkPixbuf *pixbuf;
  gssize n_read;
  guchar buffer[65536];
  gboolean res;

  res = TRUE;
  while (1)
    {
      n_read = g_input_stream_read (stream, buffer, sizeof (buffer), cancellable, error);
      if (n_read < 0)
        {
          res = FALSE;
          error = NULL; /* Ignore further errors */
          break;
        }

      if (n_read == 0)
        break;

      if (!gdk_pixbuf_loader_write (loader, buffer, n_read, error))
        {
          res = FALSE;
          error = NULL;
          break;
        }
    }

  if (!gdk_pixbuf_loader_close (loader, error))
    {
      res = FALSE;
      error = NULL;
    }

  pixbuf = NULL;

  if (res)
    {
      pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
      if (pixbuf)
        g_object_ref (pixbuf);
    }

  return pixbuf;
}

static void
size_prepared_cb (GdkPixbufLoader *loader,
                  int              width,
                  int              height,
                  gpointer         data)
{
  double *scale = data;

  width = MAX (*scale * width, 1);
  height = MAX (*scale * height, 1);

  gdk_pixbuf_loader_set_size (loader, width, height);
}

/* Like gdk_pixbuf_new_from_stream_at_scale, but
 * load the image at its original size times the
 * given scale.
 */
static GdkPixbuf *
_gdk_pixbuf_new_from_stream_scaled (GInputStream  *stream,
                                    double         scale,
                                    GCancellable  *cancellable,
                                    GError       **error)
{
  GdkPixbufLoader *loader;
  GdkPixbuf *pixbuf;

  loader = gdk_pixbuf_loader_new ();

  if (scale != 0)
    g_signal_connect (loader, "size-prepared",
                      G_CALLBACK (size_prepared_cb), &scale);

  pixbuf = load_from_stream (loader, stream, cancellable, error);

  g_object_unref (loader);

  return pixbuf;
}

static void
size_prepared_cb2 (GdkPixbufLoader *loader,
                   int              width,
                   int              height,
                   gpointer         data)
{
  int *scales = data;

  if (scales[2]) /* keep same aspect ratio as original, while fitting in given size */
    {
      double aspect = (double) height / width;

      /* First use given width and calculate size */
      width = scales[0];
      height = scales[0] * aspect;

      /* Check if it fits given height, otherwise scale down */
      if (height > scales[1])
        {
          width *= (double) scales[1] / height;
          height = scales[1];
        }
    }
  else
    {
      width = scales[0];
      height = scales[1];
    }

  gdk_pixbuf_loader_set_size (loader, width, height);
}

static GdkPixbuf *
_gdk_pixbuf_new_from_stream_at_scale (GInputStream  *stream,
                                      int            width,
                                      int            height,
                                      gboolean       aspect,
                                      GCancellable  *cancellable,
                                      GError       **error)
{
  GdkPixbufLoader *loader;
  GdkPixbuf *pixbuf;
  int scales[3];

  loader = gdk_pixbuf_loader_new ();

  scales[0] = width;
  scales[1] = height;
  scales[2] = aspect;
  g_signal_connect (loader, "size-prepared",
                    G_CALLBACK (size_prepared_cb2), scales);

  pixbuf = load_from_stream (loader, stream, cancellable, error);

  g_object_unref (loader);

  return pixbuf;
}

static GdkPixbuf *
_gdk_pixbuf_new_from_resource_at_scale (const char   *resource_path,
                                        int           width,
                                        int           height,
                                        gboolean      preserve_aspect,
                                        GError      **error)
{
  GInputStream *stream;
  GdkPixbuf *pixbuf;

  stream = g_resources_open_stream (resource_path, 0, error);
  if (stream == NULL)
    return NULL;

  pixbuf = _gdk_pixbuf_new_from_stream_at_scale (stream, width, height, preserve_aspect, NULL, error);
  g_object_unref (stream);

  return pixbuf;
}

/* }}} */
/* {{{ Symbolic processing */

static GdkPixbuf *
load_symbolic_svg (const char     *escaped_file_data,
                   gsize           len,
                   int             width,
                   int             height,
                   const char     *icon_width_str,
                   const char     *icon_height_str,
                   const char     *fg_string,
                   const char     *success_color_string,
                   const char     *warning_color_string,
                   const char     *error_color_string,
                   GError        **error)
{
  GInputStream *stream;
  GdkPixbuf *pixbuf;
  char *data;

  data = g_strconcat ("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n"
                      "<svg version=\"1.1\" "
                           "xmlns=\"http://www.w3.org/2000/svg\" "
                           "xmlns:xi=\"http://www.w3.org/2001/XInclude\" "
                           "width=\"", icon_width_str, "\" "
                           "height=\"", icon_height_str, "\">"
                        "<style type=\"text/css\">"
                          "rect,circle,path {"
                            "fill: ", fg_string," !important;"
                          "}\n"
                          ".warning {"
                             "fill: ", warning_color_string, " !important;"
                          "}\n"
                          ".error {"
                            "fill: ", error_color_string ," !important;"
                          "}\n"
                          ".success {"
                            "fill: ", success_color_string, " !important;"
                          "}"
                        "</style>"
                        "<xi:include href=\"data:text/xml;base64,",
                      NULL);

  stream = g_memory_input_stream_new_from_data (data, -1, g_free);
  g_memory_input_stream_add_data (G_MEMORY_INPUT_STREAM (stream), escaped_file_data, len, NULL);
  g_memory_input_stream_add_data (G_MEMORY_INPUT_STREAM (stream), "\"/></svg>", strlen ("\"/></svg>"), NULL);
  pixbuf = gdk_pixbuf_new_from_stream_at_scale (stream, width, height, TRUE, NULL, error);
  g_object_unref (stream);

  return pixbuf;
}

static gboolean
extract_plane (GdkPixbuf *src,
               GdkPixbuf *dst,
               int        from_plane,
               int        to_plane)
{
  guchar *src_data, *dst_data;
  int width, height;
  gsize src_stride, dst_stride;
  guchar *src_row, *dst_row;
  int x, y;
  gboolean all_clear = TRUE;

  width = gdk_pixbuf_get_width (src);
  height = gdk_pixbuf_get_height (src);

  g_assert (width <= gdk_pixbuf_get_width (dst));
  g_assert (height <= gdk_pixbuf_get_height (dst));

  src_stride = gdk_pixbuf_get_rowstride (src);
  src_data = gdk_pixbuf_get_pixels (src);

  dst_data = gdk_pixbuf_get_pixels (dst);
  dst_stride = gdk_pixbuf_get_rowstride (dst);

  for (y = 0; y < height; y++)
    {
      src_row = src_data + src_stride * y;
      dst_row = dst_data + dst_stride * y;
      for (x = 0; x < width; x++)
        {
          if (src_row[from_plane] != 0)
            all_clear = FALSE;

          dst_row[to_plane] = src_row[from_plane];
          src_row += 4;
          dst_row += 4;
        }
    }

  return all_clear;
}

static void
keep_alpha (GdkPixbuf *src)
{
  guchar *data;
  int width, height;
  gsize stride;

  data = gdk_pixbuf_get_pixels (src);
  width = gdk_pixbuf_get_width (src);
  height = gdk_pixbuf_get_height (src);
  stride = gdk_pixbuf_get_rowstride (src);

  for (int y = 0; y < height; y++)
    {
      guchar *row = data + stride * y;
      for (int x = 0; x < width; x++)
        {
          row[0] = row[1] = row[2] = 0;
          row += 4;
        }
    }
}

static void
svg_find_size_strings (const char  *data,
                       gsize        len,
                       char       **width,
                       char       **height)
{
  gsize i, j, k, l;

  *width = NULL;
  *height = NULL;

  for (i = 0; i < len - 4; i++)
    {
      if (strncmp (data + i, "<svg", 4) == 0)
        {
          for (j = i + strlen ("<svg"); j < len - 9; j++)
            {
              if (strncmp (data + j, "height=\"", strlen ("height=\"")) == 0)
                {
                  k = l = j + strlen ("height=\"");
                  while (l < len && data[l] != '\"')
                    l++;

                  *height = g_strndup (data + k, l - k);

                  if (*width && *height)
                    return;

                  j = l;
                }
              else if (strncmp (data + j, "width=\"", strlen ("width=\"")) == 0)
                {
                  k = l = j + strlen ("width=\"");
                  while (l < len && data[l] != '\"')
                    l++;

                  *width = g_strndup (data + k, l - k);

                  if (*width && *height)
                    return;

                  j = l;
                }
              else if (data[j] == '>')
                {
                  break;
                }
            }

          break;
        }
    }

  *width = g_strdup ("16px");
  *height = g_strdup ("16px");
}

static gboolean
svg_has_symbolic_classes (const char *data,
                          gsize       len)
{
#ifdef HAVE_MEMMEM
  return memmem (data, len, "class=\"error\"", strlen ("class=\"error\"")) != NULL ||
         memmem (data, len, "class=\"warning\"", strlen ("class=\"warning\"")) != NULL ||
         memmem (data, len, "class=\"success\"", strlen ("class=\"success\"")) != NULL;
#else
  return TRUE;
#endif
}

GdkPixbuf *
gtk_make_symbolic_pixbuf_from_data (const char  *file_data,
                                    gsize        file_len,
                                    int          width,
                                    int          height,
                                    double       scale,
                                    const char  *debug_output_basename,
                                    GError     **error)

{
  const char *r_string = "rgb(255,0,0)";
  const char *g_string = "rgb(0,255,0)";
  char *icon_width_str = NULL;
  char *icon_height_str = NULL;
  char *escaped_file_data = NULL;
  gsize len;
  GdkPixbuf *pixbuf = NULL;
  gboolean has_symbolic_classes;
  gboolean only_fg = TRUE;

  has_symbolic_classes = svg_has_symbolic_classes (file_data, file_len);

  /* Fetch size from the original icon */
  if (has_symbolic_classes || width == 0 || height == 0)
    svg_find_size_strings (file_data, file_len, &icon_width_str, &icon_height_str);

  if (width == 0)
    width = (int) (g_ascii_strtoull (icon_width_str, NULL, 0) * scale);
  if (height == 0)
    height = (int) (g_ascii_strtoull (icon_height_str, NULL, 0) * scale);

  if (!has_symbolic_classes)
    {
      GInputStream *stream;

      stream = g_memory_input_stream_new_from_data (file_data, file_len, NULL);
      pixbuf = gdk_pixbuf_new_from_stream_at_scale (stream, width, height, TRUE, NULL, error);
      g_object_unref (stream);

      if (pixbuf)
        keep_alpha (pixbuf);

      goto out;
    }

  escaped_file_data = g_base64_encode ((guchar *) file_data, file_len);
  len = strlen (escaped_file_data);

  for (int plane = 0; plane < 3; plane++)
    {
      GdkPixbuf *loaded;

      /* Here we render the svg with all colors solid, this should
       * always make the alpha channel the same and it should match
       * the final alpha channel for all possible renderings. We
       * Just use it as-is for final alpha.
       *
       * For the 3 non-fg colors, we render once each with that
       * color as red, and every other color as green. The resulting
       * red will describe the amount of that color is in the
       * opaque part of the color. We store these as the rgb
       * channels, with the color of the fg being implicitly
       * the "rest", as all color fractions should add up to 1.
       */
      loaded = load_symbolic_svg (escaped_file_data, len, width, height,
                                  icon_width_str,
                                  icon_height_str,
                                  g_string,
                                  plane == 0 ? r_string : g_string,
                                  plane == 1 ? r_string : g_string,
                                  plane == 2 ? r_string : g_string,
                                  error);
      if (loaded == NULL)
        goto out;

      if (debug_output_basename)
        {
          char *filename;

          filename = g_strdup_printf ("%s.debug%d.png", debug_output_basename, plane);
          g_print ("Writing %s\n", filename);
          gdk_pixbuf_save (loaded, filename, "png", NULL, NULL);
          g_free (filename);
        }

      if (plane == 0)
        {
          pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8,
                                   gdk_pixbuf_get_width (loaded),
                                   gdk_pixbuf_get_height (loaded));
          memset (gdk_pixbuf_get_pixels (pixbuf), 0, gdk_pixbuf_get_byte_length (pixbuf));

          extract_plane (loaded, pixbuf, 3, 3);
        }

      only_fg &= extract_plane (loaded, pixbuf, 0, plane);

      g_object_unref (loaded);
    }

out:
  if (only_fg && pixbuf)
    gdk_pixbuf_set_option (pixbuf, "tEXt::only-foreground", "true");

  g_free (escaped_file_data);
  g_free (icon_width_str);
  g_free (icon_height_str);

  return pixbuf;
}

static GdkPixbuf *
make_symbolic_pixbuf_from_resource (const char  *path,
                                    int          width,
                                    int          height,
                                    double       scale,
                                    GError     **error)
{
  GBytes *bytes;
  const char *data;
  gsize size;
  GdkPixbuf *pixbuf;

  bytes = g_resources_lookup_data (path, G_RESOURCE_LOOKUP_FLAGS_NONE, error);
  if (bytes == NULL)
    return NULL;

  data = g_bytes_get_data (bytes, &size);

  pixbuf = gtk_make_symbolic_pixbuf_from_data (data, size, width, height, scale, NULL, error);

  g_bytes_unref (bytes);

  return pixbuf;
}

static GdkPixbuf *
make_symbolic_pixbuf_from_filename (const char  *filename,
                                    int          width,
                                    int          height,
                                    double       scale,
                                    GError     **error)
{
  char *data;
  gsize size;
  GdkPixbuf *pixbuf;

  if (!g_file_get_contents (filename, &data, &size, error))
    return NULL;

  pixbuf = gtk_make_symbolic_pixbuf_from_data (data, size, width, height, scale, NULL, error);

  g_free (data);

  return pixbuf;
}

static GdkPixbuf *
make_symbolic_pixbuf_from_file (GFile       *file,
                                int          width,
                                int          height,
                                double       scale,
                                GError     **error)
{
  char *data;
  gsize size;
  GdkPixbuf *pixbuf;

  if (!g_file_load_contents (file, NULL, &data, &size, NULL, error))
    return NULL;

  pixbuf = gtk_make_symbolic_pixbuf_from_data (data, size, width, height, scale, NULL, error);

  g_free (data);

  return pixbuf;
}

/* }}} */
/* {{{ Texture API */

static GdkTexture *
texture_new_from_bytes (GBytes    *bytes,
                        gboolean  *only_fg,
                        GError   **error)
{
  GHashTable *options;
  GdkTexture *texture;

  if (!gdk_is_png (bytes))
    return gdk_texture_new_from_bytes (bytes, error);

  options = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  texture = gdk_load_png (bytes, options, error);
  *only_fg = g_hash_table_contains (options, "foreground-only");
  g_hash_table_unref (options);

  return texture;
}

GdkTexture *
gdk_texture_new_from_filename_with_fg (const char    *filename,
                                       gboolean      *only_fg,
                                       GError       **error)
{
  GFile *file;
  GBytes *bytes;
  GdkTexture *texture = NULL;

  file = g_file_new_for_path (filename);
  bytes = g_file_load_bytes (file, NULL, NULL, error);
  if (bytes)
    texture = texture_new_from_bytes (bytes, only_fg, error);
  g_bytes_unref (bytes);
  g_object_unref (file);

  return texture;
}

GdkTexture *
gdk_texture_new_from_resource_with_fg (const char *path,
                                       gboolean   *only_fg)
{
  GBytes *bytes;
  GdkTexture *texture = NULL;

  bytes = g_resources_lookup_data (path, 0, NULL);
  if (bytes)
    texture = texture_new_from_bytes (bytes, only_fg, NULL);
  g_bytes_unref (bytes);

  return texture;
}

GdkTexture *
gdk_texture_new_from_stream_with_fg (GInputStream  *stream,
                                     gboolean      *only_fg,
                                     GCancellable  *cancellable,
                                     GError       **error)
{
  GdkPixbuf *pixbuf;
  GdkTexture *texture = NULL;

  pixbuf = _gdk_pixbuf_new_from_stream_scaled (stream, 0, cancellable, error);
  if (pixbuf)
    {
      *only_fg = pixbuf_is_only_fg (pixbuf);
      texture = gdk_texture_new_for_pixbuf (pixbuf);
      g_object_unref (pixbuf);
    }

  return texture;
}

GdkTexture *
gdk_texture_new_from_stream_at_scale (GInputStream  *stream,
                                      int            width,
                                      int            height,
                                      gboolean       aspect,
                                      gboolean      *only_fg,
                                      GCancellable  *cancellable,
                                      GError       **error)
{
  GdkPixbuf *pixbuf;
  GdkTexture *texture = NULL;

  pixbuf = _gdk_pixbuf_new_from_stream_at_scale (stream, width, height, aspect, cancellable, error);
  if (pixbuf)
    {
      *only_fg = pixbuf_is_only_fg (pixbuf);
      texture = gdk_texture_new_for_pixbuf (pixbuf);
      g_object_unref (pixbuf);
    }

  return texture;
}

GdkTexture *
gdk_texture_new_from_resource_at_scale (const char    *path,
                                        int            width,
                                        int            height,
                                        gboolean       preserve_aspect,
                                        gboolean      *only_fg,
                                        GError       **error)
{
  GdkPixbuf *pixbuf;
  GdkTexture *texture = NULL;

  pixbuf = _gdk_pixbuf_new_from_resource_at_scale (path, width, height, preserve_aspect, error);
  if (pixbuf)
    {
      *only_fg = pixbuf_is_only_fg (pixbuf);
      texture = gdk_texture_new_for_pixbuf (pixbuf);
      g_object_unref (pixbuf);
    }

  return texture;
}

/* }}} */
/* {{{ Symbolic texture API */

GdkTexture *
gdk_texture_new_from_filename_symbolic (const char    *filename,
                                        int            width,
                                        int            height,
                                        double         scale,
                                        gboolean      *only_fg,
                                        GError       **error)
{
  GdkPixbuf *pixbuf;
  GdkTexture *texture = NULL;

  pixbuf = make_symbolic_pixbuf_from_filename (filename, width, height, scale, error);
  if (pixbuf)
    {
      *only_fg = pixbuf_is_only_fg (pixbuf);
      texture = gdk_texture_new_for_pixbuf (pixbuf);
      g_object_unref (pixbuf);
    }

  return texture;
}

GdkTexture *
gtk_load_symbolic_texture_from_resource (const char *path)
{
  return gdk_texture_new_from_resource (path);
}

GdkTexture *
gdk_texture_new_from_resource_symbolic (const char  *path,
                                        int          width,
                                        int          height,
                                        double       scale,
                                        gboolean    *only_fg,
                                        GError     **error)
{
  GdkPixbuf *pixbuf;
  GdkTexture *texture = NULL;

  pixbuf = make_symbolic_pixbuf_from_resource (path, width, height, scale, error);
  if (pixbuf)
    {
      *only_fg = pixbuf_is_only_fg (pixbuf);
      texture = gdk_texture_new_for_pixbuf (pixbuf);
      g_object_unref (pixbuf);
    }

  return texture;
}

GdkTexture *
gtk_load_symbolic_texture_from_file (GFile *file)
{
  GdkPixbuf *pixbuf;
  GdkTexture *texture;
  GInputStream *stream;

  stream = G_INPUT_STREAM (g_file_read (file, NULL, NULL));
  if (stream == NULL)
    return NULL;

  pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, NULL);
  g_object_unref (stream);
  if (pixbuf == NULL)
    return NULL;

  texture = gdk_texture_new_for_pixbuf (pixbuf);
  g_object_unref (pixbuf);

  return texture;
}

GdkTexture *
gdk_texture_new_from_file_symbolic (GFile       *file,
                                    int          width,
                                    int          height,
                                    double       scale,
                                    gboolean    *only_fg,
                                    GError     **error)
{
  GdkPixbuf *pixbuf;
  GdkTexture *texture = NULL;

  pixbuf = make_symbolic_pixbuf_from_file (file, width, height, scale, error);
  if (pixbuf)
    {
      *only_fg = pixbuf_is_only_fg (pixbuf);
      texture = gdk_texture_new_for_pixbuf (pixbuf);
      g_object_unref (pixbuf);
    }

  return texture;
}

/* }}} */
/* {{{ Scaled paintable API */

typedef struct {
  double scale;
} LoaderData;

static void
on_loader_size_prepared (GdkPixbufLoader *loader,
                         int              width,
                         int              height,
                         gpointer         user_data)
{
  LoaderData *loader_data = user_data;
  GdkPixbufFormat *format;

  /* Let the regular icon helper code path handle non-scalable images */
  format = gdk_pixbuf_loader_get_format (loader);
  if (!gdk_pixbuf_format_is_scalable (format))
    {
      loader_data->scale = 1.0;
      return;
    }

  gdk_pixbuf_loader_set_size (loader,
                              width * loader_data->scale,
                              height * loader_data->scale);
}

static GdkPaintable *
gdk_paintable_new_from_bytes_scaled (GBytes *bytes,
                                     double  scale)
{
  LoaderData loader_data;
  GdkTexture *texture;
  GdkPaintable *paintable;

  loader_data.scale = scale;

  if (gdk_texture_can_load (bytes))
    {
      texture = gdk_texture_new_from_bytes (bytes, NULL);
      if (texture == NULL)
        return NULL;

      /* We know these formats can't be scaled */
      paintable = GDK_PAINTABLE (texture);
    }
  else
    {
      GdkPixbufLoader *loader;
      gboolean success;

      loader = gdk_pixbuf_loader_new ();
      g_signal_connect (loader, "size-prepared",
                        G_CALLBACK (on_loader_size_prepared), &loader_data);

      success = gdk_pixbuf_loader_write_bytes (loader, bytes, NULL);
      /* close even when writing failed */
      success &= gdk_pixbuf_loader_close (loader, NULL);

      if (!success)
        return NULL;

      texture = gdk_texture_new_for_pixbuf (gdk_pixbuf_loader_get_pixbuf (loader));
      g_object_unref (loader);

      if (loader_data.scale != 1.0)
        paintable = gtk_scaler_new (GDK_PAINTABLE (texture), loader_data.scale);
      else
        paintable = g_object_ref (GDK_PAINTABLE (texture));

      g_object_unref (texture);
    }

  return paintable;
}

GdkPaintable *
gdk_paintable_new_from_filename_scaled (const char *filename,
                                        double      scale)
{
  char *contents;
  gsize length;
  GBytes *bytes;
  GdkPaintable *paintable;

  if (!g_file_get_contents (filename, &contents, &length, NULL))
    return NULL;

  bytes = g_bytes_new_take (contents, length);

  paintable = gdk_paintable_new_from_bytes_scaled (bytes, scale);

  g_bytes_unref (bytes);

  return paintable;
}

GdkPaintable *
gdk_paintable_new_from_resource_scaled (const char *path,
                                        double      scale)
{
  GBytes *bytes;
  GdkPaintable *paintable;

  bytes = g_resources_lookup_data (path, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
  if (!bytes)
    return NULL;

  paintable = gdk_paintable_new_from_bytes_scaled (bytes, scale);

  g_bytes_unref (bytes);

  return paintable;
}

GdkPaintable *
gdk_paintable_new_from_file_scaled (GFile  *file,
                                    double  scale)
{
  GBytes *bytes;
  GdkPaintable *paintable;

  bytes = g_file_load_bytes (file, NULL, NULL, NULL);
  if (!bytes)
    return NULL;

  paintable = gdk_paintable_new_from_bytes_scaled (bytes, scale);

  g_bytes_unref (bytes);

  return paintable;
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
