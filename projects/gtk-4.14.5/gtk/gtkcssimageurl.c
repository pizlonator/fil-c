/*
 * Copyright © 2011 Red Hat Inc.
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

#include <string.h>

#include "gtkcssimageurlprivate.h"

#include "gtkcssimageinvalidprivate.h"
#include "gtkcssimagepaintableprivate.h"
#include "gtkstyleproviderprivate.h"
#include "gtk/css/gtkcssdataurlprivate.h"


G_DEFINE_TYPE (GtkCssImageUrl, _gtk_css_image_url, GTK_TYPE_CSS_IMAGE)

static GtkCssImage *
gtk_css_image_url_load_image (GtkCssImageUrl  *url,
                              GError         **error)
{
  GdkTexture *texture;
  GError *local_error = NULL;

  if (url->loaded_image)
    return url->loaded_image;

  if (url->file == NULL)
    {
      url->loaded_image = gtk_css_image_invalid_new ();
      return url->loaded_image;
    }

  texture = gdk_texture_new_from_file (url->file, &local_error);

  if (texture == NULL)
    {
      if (error && local_error)
        {
          char *uri;

          uri = g_file_get_uri (url->file);
          g_set_error (error,
                       GTK_CSS_PARSER_ERROR,
                       GTK_CSS_PARSER_ERROR_FAILED,
                       "Error loading image '%s': %s", uri, local_error->message);
          g_free (uri);
       }

      url->loaded_image = gtk_css_image_invalid_new ();
    }
  else
    {
      url->loaded_image = gtk_css_image_paintable_new (GDK_PAINTABLE (texture), GDK_PAINTABLE (texture));
      g_object_unref (texture);
    }

  g_clear_error (&local_error);

  return url->loaded_image;
}

static int
gtk_css_image_url_get_width (GtkCssImage *image)
{
  GtkCssImageUrl *url = GTK_CSS_IMAGE_URL (image);

  return _gtk_css_image_get_width (gtk_css_image_url_load_image (url, NULL));
}

static int
gtk_css_image_url_get_height (GtkCssImage *image)
{
  GtkCssImageUrl *url = GTK_CSS_IMAGE_URL (image);

  return _gtk_css_image_get_height (gtk_css_image_url_load_image (url, NULL));
}

static double
gtk_css_image_url_get_aspect_ratio (GtkCssImage *image)
{
  GtkCssImageUrl *url = GTK_CSS_IMAGE_URL (image);

  return _gtk_css_image_get_aspect_ratio (gtk_css_image_url_load_image (url, NULL));
}

static void
gtk_css_image_url_snapshot (GtkCssImage *image,
                            GtkSnapshot *snapshot,
                            double       width,
                            double       height)
{
  GtkCssImageUrl *url = GTK_CSS_IMAGE_URL (image);

  gtk_css_image_snapshot (gtk_css_image_url_load_image (url, NULL), snapshot, width, height);
}

static GtkCssImage *
gtk_css_image_url_compute (GtkCssImage      *image,
                           guint             property_id,
                           GtkStyleProvider *provider,
                           GtkCssStyle      *style,
                           GtkCssStyle      *parent_style)
{
  GtkCssImageUrl *url = GTK_CSS_IMAGE_URL (image);
  GtkCssImage *copy;
  GError *error = NULL;

  copy = gtk_css_image_url_load_image (url, &error);
  if (error)
    {
      GtkCssSection *section = gtk_css_style_get_section (style, property_id);
      gtk_style_provider_emit_error (provider, section, error);
      g_error_free (error);
    }

  return g_object_ref (copy);
}

static gboolean
gtk_css_image_url_equal (GtkCssImage *image1,
                         GtkCssImage *image2)
{
  GtkCssImageUrl *url1 = GTK_CSS_IMAGE_URL (image1);
  GtkCssImageUrl *url2 = GTK_CSS_IMAGE_URL (image2);
  
  /* FIXME: We don't save data: urls, so we can't compare them here */
  if (url1->file == NULL || url2->file == NULL)
    return FALSE;

  return g_file_equal (url1->file, url2->file);
}

static gboolean
gtk_css_image_url_is_invalid (GtkCssImage *image)
{
  GtkCssImageUrl *url = GTK_CSS_IMAGE_URL (image);

  return gtk_css_image_is_invalid (gtk_css_image_url_load_image (url, NULL));
}

static gboolean
gtk_css_image_url_is_computed (GtkCssImage *image)
{
  return TRUE;
}

static gboolean
gtk_css_image_url_parse (GtkCssImage  *image,
                         GtkCssParser *parser)
{
  GtkCssImageUrl *self = GTK_CSS_IMAGE_URL (image);
  char *url, *scheme;

  url = gtk_css_parser_consume_url (parser);
  if (url == NULL)
    return FALSE;

  scheme = g_uri_parse_scheme (url);
  if (scheme && g_ascii_strcasecmp (scheme, "data") == 0)
    {
      GBytes *bytes;
      GError *error = NULL;

      bytes = gtk_css_data_url_parse (url, NULL, &error);
      if (bytes)
        {
          GdkTexture *texture;

          texture = gdk_texture_new_from_bytes (bytes, &error);
          g_bytes_unref (bytes);
          if (texture)
            {
              GdkPaintable *paintable = GDK_PAINTABLE (texture);
              self->loaded_image = gtk_css_image_paintable_new (paintable, paintable);
            }
          else
            {
              gtk_css_parser_emit_error (parser,
                                         gtk_css_parser_get_start_location (parser),
                                         gtk_css_parser_get_end_location (parser),
                                         error);
              g_clear_error (&error);
            }
        }
      else
        {
          gtk_css_parser_emit_error (parser,
                                     gtk_css_parser_get_start_location (parser),
                                     gtk_css_parser_get_end_location (parser),
                                     error);
          g_clear_error (&error);
        }
    }
  else
    {
      self->file = gtk_css_parser_resolve_url (parser, url);
    }

  g_free (url);
  g_free (scheme);

  return TRUE;
}

static void
gtk_css_image_url_print (GtkCssImage *image,
                         GString     *string)
{
  GtkCssImageUrl *url = GTK_CSS_IMAGE_URL (image);

  _gtk_css_image_print (gtk_css_image_url_load_image (url, NULL), string);
}

static void
gtk_css_image_url_dispose (GObject *object)
{
  GtkCssImageUrl *url = GTK_CSS_IMAGE_URL (object);

  g_clear_object (&url->file);
  g_clear_object (&url->loaded_image);

  G_OBJECT_CLASS (_gtk_css_image_url_parent_class)->dispose (object);
}

static void
_gtk_css_image_url_class_init (GtkCssImageUrlClass *klass)
{
  GtkCssImageClass *image_class = GTK_CSS_IMAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  image_class->get_width = gtk_css_image_url_get_width;
  image_class->get_height = gtk_css_image_url_get_height;
  image_class->get_aspect_ratio = gtk_css_image_url_get_aspect_ratio;
  image_class->compute = gtk_css_image_url_compute;
  image_class->snapshot = gtk_css_image_url_snapshot;
  image_class->parse = gtk_css_image_url_parse;
  image_class->print = gtk_css_image_url_print;
  image_class->equal = gtk_css_image_url_equal;
  image_class->is_invalid = gtk_css_image_url_is_invalid;
  image_class->is_computed = gtk_css_image_url_is_computed;

  object_class->dispose = gtk_css_image_url_dispose;
}

static void
_gtk_css_image_url_init (GtkCssImageUrl *image_url)
{
}

