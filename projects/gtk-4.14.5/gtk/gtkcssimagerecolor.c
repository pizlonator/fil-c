/*
 * Copyright © 2016 Red Hat Inc.
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

#include "gtkcssimagerecolorprivate.h"
#include "gtkcssimageprivate.h"
#include "gtkcsspalettevalueprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gdktextureutilsprivate.h"

#include "gtkstyleproviderprivate.h"

G_DEFINE_TYPE (GtkCssImageRecolor, _gtk_css_image_recolor, GTK_TYPE_CSS_IMAGE)

static void
gtk_css_image_recolor_print (GtkCssImage *image,
                             GString     *string)
{
  GtkCssImageRecolor *recolor = GTK_CSS_IMAGE_RECOLOR (image);
  char *uri;

  g_string_append (string, "-gtk-recolor(url(");
  uri = g_file_get_uri (recolor->file);
  g_string_append (string, uri);
  g_free (uri);
  g_string_append (string, ")");
  if (recolor->palette)
    {
      g_string_append (string, ",");
      _gtk_css_value_print (recolor->palette, string);
    }
  g_string_append (string, ")");
}

static void
gtk_css_image_recolor_dispose (GObject *object)
{
  GtkCssImageRecolor *recolor = GTK_CSS_IMAGE_RECOLOR (object);

  g_clear_pointer (&recolor->palette, _gtk_css_value_unref);
  g_clear_object (&recolor->file);
  g_clear_object (&recolor->texture);

  G_OBJECT_CLASS (_gtk_css_image_recolor_parent_class)->dispose (object);
}

static void
lookup_symbolic_colors (GtkCssStyle *style,
                        GtkCssValue *palette,
                        GdkRGBA     *color_out,
                        GdkRGBA     *success_out,
                        GdkRGBA     *warning_out,
                        GdkRGBA     *error_out)
{
  const GdkRGBA *lookup;

  *color_out = *gtk_css_color_value_get_rgba (style->core->color);

  lookup = gtk_css_palette_value_get_color (palette, "success");
  if (lookup)
    *success_out = *lookup;
  else
    *success_out = *color_out;

  lookup = gtk_css_palette_value_get_color (palette, "warning");
  if (lookup)
    *warning_out = *lookup;
  else
    *warning_out = *color_out;

  lookup = gtk_css_palette_value_get_color (palette, "error");
  if (lookup)
    *error_out = *lookup;
  else
    *error_out = *color_out;
}

static void
gtk_css_image_recolor_load_texture (GtkCssImageRecolor  *recolor,
                                    GError             **error)
{
  char *uri;
  gboolean only_fg;

  if (recolor->texture)
    return;

  uri = g_file_get_uri (recolor->file);

  if (g_file_has_uri_scheme (recolor->file, "resource"))
    {
      char *resource_path = g_uri_unescape_string (uri + strlen ("resource://"), NULL);

      if (g_str_has_suffix (uri, ".symbolic.png"))
        recolor->texture = gtk_load_symbolic_texture_from_resource (resource_path);
      else
        recolor->texture = gdk_texture_new_from_resource_symbolic (resource_path, 0, 0, 1.0, &only_fg, NULL);

      g_free (resource_path);
    }
  else
    {
      if (g_str_has_suffix (uri, ".symbolic.png"))
        recolor->texture = gtk_load_symbolic_texture_from_file (recolor->file);
      else
        recolor->texture = gdk_texture_new_from_file_symbolic (recolor->file, 0, 0, 1.0, &only_fg, NULL);
    }

  g_free (uri);
}

static GtkCssImage *
gtk_css_image_recolor_load (GtkCssImageRecolor  *recolor,
                            GtkCssStyle         *style,
                            GtkCssValue         *palette,
                            int                  scale,
                            GError             **gerror)
{
  GError *local_error = NULL;
  GtkCssImageRecolor *image;

  image = g_object_new (GTK_TYPE_CSS_IMAGE_RECOLOR, NULL);

  lookup_symbolic_colors (style, palette, &image->color, &image->success, &image->warning, &image->error);
  gtk_css_image_recolor_load_texture (recolor, &local_error);

  image->file = g_object_ref (recolor->file);

  if (recolor->texture)
    image->texture = g_object_ref (recolor->texture);
  else
    {
      if (gerror)
        {
          char *uri;

          uri = g_file_get_uri (recolor->file);
          g_set_error (gerror,
                       GTK_CSS_PARSER_ERROR,
                       GTK_CSS_PARSER_ERROR_FAILED,
                       "Error loading image '%s': %s", uri, local_error ? local_error->message : "");
          g_free (uri);
       }
    }

  g_clear_error (&local_error);

  return GTK_CSS_IMAGE (image);
}

static void
gtk_css_image_recolor_snapshot (GtkCssImage *image,
                                GtkSnapshot *snapshot,
                                double       width,
                                double       height)
{
  GtkCssImageRecolor *recolor = GTK_CSS_IMAGE_RECOLOR (image);
  const GdkRGBA *fg = &recolor->color;
  const GdkRGBA *sc = &recolor->success;
  const GdkRGBA *wc = &recolor->warning;
  const GdkRGBA *ec = &recolor->error;
  graphene_matrix_t matrix;
  graphene_vec4_t offset;

  if (recolor->texture == NULL)
    return;

  graphene_matrix_init_from_float (&matrix,
          (float[16]) {
                       sc->red - fg->red, sc->green - fg->green, sc->blue - fg->blue, 0,
                       wc->red - fg->red, wc->green - fg->green, wc->blue - fg->blue, 0,
                       ec->red - fg->red, ec->green - fg->green, ec->blue - fg->blue, 0,
                       0, 0, 0, fg->alpha
                      });

  graphene_vec4_init (&offset, fg->red, fg->green, fg->blue, 0);
  gtk_snapshot_push_color_matrix (snapshot, &matrix, &offset);

  gtk_snapshot_append_texture (snapshot,
                               recolor->texture,
                               &GRAPHENE_RECT_INIT (0, 0, width, height));

  gtk_snapshot_pop (snapshot);
}

static GtkCssImage *
gtk_css_image_recolor_compute (GtkCssImage      *image,
                               guint             property_id,
                               GtkStyleProvider *provider,
                               GtkCssStyle      *style,
                               GtkCssStyle      *parent_style)
{
  GtkCssImageRecolor *recolor = GTK_CSS_IMAGE_RECOLOR (image);
  GtkCssValue *palette;
  GtkCssImage *img;
  int scale;
  GError *error = NULL;

  scale = gtk_style_provider_get_scale (provider);

  if (recolor->palette)
    palette = _gtk_css_value_compute (recolor->palette, property_id, provider, style, parent_style);
  else
    palette = _gtk_css_value_ref (style->core->icon_palette);

  img = gtk_css_image_recolor_load (recolor, style, palette, scale, &error);

  if (error)
    {
      GtkCssSection *section = gtk_css_style_get_section (style, property_id);
      gtk_style_provider_emit_error (provider, section, error);
      g_error_free (error);
    }

  _gtk_css_value_unref (palette);

  return img;
}

static guint
gtk_css_image_recolor_parse_arg (GtkCssParser *parser,
                                 guint         arg,
                                 gpointer      data)
{
  GtkCssImageRecolor *self = data;

  switch (arg)
  {
    case 0:
      {
        char *url = gtk_css_parser_consume_url (parser);
        if (url == NULL)
          return 0;
        self->file = gtk_css_parser_resolve_url (parser, url);
        g_free (url);
        if (self->file == NULL)
          return 0;
        return 1;
      }

    case 1:
      self->palette = gtk_css_palette_value_parse (parser);
      if (self->palette == NULL)
        return 0;
      return 1;

    default:
      g_assert_not_reached ();
      return 0;
  }
}

static gboolean
gtk_css_image_recolor_parse (GtkCssImage  *image,
                                GtkCssParser *parser)
{
  if (!gtk_css_parser_has_function (parser, "-gtk-recolor"))
    {
      gtk_css_parser_error_syntax (parser, "Expected '-gtk-recolor('");
      return FALSE;
    }

  return gtk_css_parser_consume_function (parser, 1, 2, gtk_css_image_recolor_parse_arg, image);
}

static int
gtk_css_image_recolor_get_width (GtkCssImage *image)
{
  GtkCssImageRecolor *recolor = GTK_CSS_IMAGE_RECOLOR (image);

  gtk_css_image_recolor_load_texture (recolor, NULL);

  if (recolor->texture == NULL)
    return 0;

  return gdk_texture_get_width (recolor->texture);
}

static int
gtk_css_image_recolor_get_height (GtkCssImage *image)
{
  GtkCssImageRecolor *recolor = GTK_CSS_IMAGE_RECOLOR (image);

  gtk_css_image_recolor_load_texture (recolor, NULL);

  if (recolor->texture == NULL)
    return 0;

  return gdk_texture_get_height (recolor->texture);
}

static gboolean
gtk_css_image_recolor_is_computed (GtkCssImage *image)
{
  GtkCssImageRecolor *recolor = GTK_CSS_IMAGE_RECOLOR (image);

  return recolor->texture &&
         (!recolor->palette || gtk_css_value_is_computed (recolor->palette));
}

static void
_gtk_css_image_recolor_class_init (GtkCssImageRecolorClass *klass)
{
  GtkCssImageClass *image_class = GTK_CSS_IMAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  image_class->get_width = gtk_css_image_recolor_get_width;
  image_class->get_height = gtk_css_image_recolor_get_height;
  image_class->compute = gtk_css_image_recolor_compute;
  image_class->snapshot = gtk_css_image_recolor_snapshot;
  image_class->parse = gtk_css_image_recolor_parse;
  image_class->print = gtk_css_image_recolor_print;
  image_class->is_computed = gtk_css_image_recolor_is_computed;

  object_class->dispose = gtk_css_image_recolor_dispose;
}

static void
_gtk_css_image_recolor_init (GtkCssImageRecolor *image_recolor)
{
}
