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

#include "gtkcssimagefallbackprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtkcsscolorvalueprivate.h"

#include "gtkstyleproviderprivate.h"

G_DEFINE_TYPE (GtkCssImageFallback, _gtk_css_image_fallback, GTK_TYPE_CSS_IMAGE)

static int
gtk_css_image_fallback_get_width (GtkCssImage *image)
{
  GtkCssImageFallback *fallback = GTK_CSS_IMAGE_FALLBACK (image);

  if (fallback->used < 0)
    return 0;

  return _gtk_css_image_get_width (fallback->images[fallback->used]);
}

static int
gtk_css_image_fallback_get_height (GtkCssImage *image)
{
  GtkCssImageFallback *fallback = GTK_CSS_IMAGE_FALLBACK (image);

  if (fallback->used < 0)
    return 0;

  return _gtk_css_image_get_height (fallback->images[fallback->used]);
}

static double
gtk_css_image_fallback_get_aspect_ratio (GtkCssImage *image)
{
  GtkCssImageFallback *fallback = GTK_CSS_IMAGE_FALLBACK (image);

  if (fallback->used < 0)
    return 0;

  return _gtk_css_image_get_aspect_ratio (fallback->images[fallback->used]);
}

static void
gtk_css_image_fallback_snapshot (GtkCssImage *image,
                                 GtkSnapshot *snapshot,
                                 double       width,
                                 double       height)
{
  GtkCssImageFallback *fallback = GTK_CSS_IMAGE_FALLBACK (image);

  if (fallback->used < 0)
    {
      if (fallback->color)
        {
          const GdkRGBA *color = gtk_css_color_value_get_rgba (fallback->color);
          if (!gdk_rgba_is_clear (color))
            gtk_snapshot_append_color (snapshot, color,
                                       &GRAPHENE_RECT_INIT (0, 0, width, height));
        }
      else
        {
          gtk_snapshot_append_color (snapshot, &(GdkRGBA) {1, 0, 0, 1},
                                     &GRAPHENE_RECT_INIT (0, 0, width, height));
        }
    }
  else
    gtk_css_image_snapshot (fallback->images[fallback->used], snapshot, width, height);
}

static void
gtk_css_image_fallback_print (GtkCssImage *image,
                              GString     *string)
{
  GtkCssImageFallback *fallback = GTK_CSS_IMAGE_FALLBACK (image);
  int i;

  g_string_append (string, "image(");
  for (i = 0; i < fallback->n_images; i++)
    {
      if (i > 0)
        g_string_append (string, ",");
      _gtk_css_image_print (fallback->images[i], string);
    }
  if (fallback->color)
    {
      if (fallback->n_images > 0)
        g_string_append (string, ",");
      _gtk_css_value_print (fallback->color, string);
    }

  g_string_append (string, ")");
}

static void
gtk_css_image_fallback_dispose (GObject *object)
{
  GtkCssImageFallback *fallback = GTK_CSS_IMAGE_FALLBACK (object);
  int i;

  for (i = 0; i < fallback->n_images; i++)
    g_object_unref (fallback->images[i]);
  g_free (fallback->images);
  fallback->images = NULL;

  if (fallback->color)
    {
      _gtk_css_value_unref (fallback->color);
      fallback->color = NULL;
    }

  G_OBJECT_CLASS (_gtk_css_image_fallback_parent_class)->dispose (object);
}


static GtkCssImage *
gtk_css_image_fallback_compute (GtkCssImage      *image,
                                guint             property_id,
                                GtkStyleProvider *provider,
                                GtkCssStyle      *style,
                                GtkCssStyle      *parent_style)
{
  GtkCssImageFallback *fallback = GTK_CSS_IMAGE_FALLBACK (image);
  GtkCssImageFallback *copy;
  int i;

  if (fallback->used < 0)
    {
      GtkCssValue *computed_color = NULL;

      if (fallback->color)
        computed_color = _gtk_css_value_compute (fallback->color,
                                                 property_id,
                                                 provider,
                                                 style,
                                                 parent_style);

      /* image($color) that didn't change */
      if (computed_color && !fallback->images &&
          computed_color == fallback->color)
        return g_object_ref (image);

      copy = g_object_new (_gtk_css_image_fallback_get_type (), NULL);
      copy->n_images = fallback->n_images;
      copy->images = g_new (GtkCssImage *, fallback->n_images);
      for (i = 0; i < fallback->n_images; i++)
        {
          copy->images[i] = _gtk_css_image_compute (fallback->images[i],
                                                    property_id,
                                                    provider,
                                                    style,
                                                    parent_style);

          if (gtk_css_image_is_invalid (copy->images[i]))
            continue;

          if (copy->used < 0)
            copy->used = i;
        }

      copy->color = computed_color;

      return GTK_CSS_IMAGE (copy);
    }
  else
    return GTK_CSS_IMAGE (g_object_ref (fallback));
}

typedef struct
{
  GtkCssValue *color;
  GPtrArray *images;
} ParseData;

static guint
gtk_css_image_fallback_parse_arg (GtkCssParser *parser,
                                  guint         arg,
                                  gpointer      _data)
{
  ParseData *data = _data;

  if (data->color != NULL)
    {
      gtk_css_parser_error_syntax (parser, "The color must be the last parameter");
      return 0;
    }
  else if (_gtk_css_image_can_parse (parser))
    {
      GtkCssImage *image = _gtk_css_image_new_parse (parser);
      if (image == NULL)
        return 0;

      if (!data->images)
        data->images = g_ptr_array_new_with_free_func (g_object_unref);

      g_ptr_array_add (data->images, image);
      return 1;
    }
  else
    {
      data->color = _gtk_css_color_value_parse (parser);
      if (data->color == NULL)
        return 0;

      return 1;
    }
}

static gboolean
gtk_css_image_fallback_parse (GtkCssImage  *image,
                              GtkCssParser *parser)
{
  GtkCssImageFallback *self = GTK_CSS_IMAGE_FALLBACK (image);
  ParseData data = { NULL, NULL };

  if (!gtk_css_parser_has_function (parser, "image"))
    {
      gtk_css_parser_error_syntax (parser, "Expected 'image('");
      return FALSE;
    }

  if (!gtk_css_parser_consume_function (parser, 1, G_MAXUINT, gtk_css_image_fallback_parse_arg, &data))
    {
      g_clear_pointer (&data.color, _gtk_css_value_unref);
      if (data.images)
        g_ptr_array_free (data.images, TRUE);
      return FALSE;
    }

  self->color = data.color;
  if (data.images)
    {
      self->n_images = data.images->len;
      self->images = (GtkCssImage **) g_ptr_array_free (data.images, FALSE);
    }
  else
    {
      self->n_images = 0;
      self->images = NULL;
    }

  return TRUE;
}

static gboolean
gtk_css_image_fallback_equal (GtkCssImage *image1,
                              GtkCssImage *image2)
{
  GtkCssImageFallback *fallback1 = GTK_CSS_IMAGE_FALLBACK (image1);
  GtkCssImageFallback *fallback2 = GTK_CSS_IMAGE_FALLBACK (image2);

  if (fallback1->used < 0)
    {
      if (fallback2->used >= 0)
        return FALSE;

      return _gtk_css_value_equal (fallback1->color, fallback2->color);
    }

  if (fallback2->used < 0)
    return FALSE;

  return _gtk_css_image_equal (fallback1->images[fallback1->used],
                               fallback2->images[fallback2->used]);
}

static gboolean
gtk_css_image_fallback_is_computed (GtkCssImage *image)
{
  GtkCssImageFallback *fallback = GTK_CSS_IMAGE_FALLBACK (image);

  if (fallback->used < 0)
    {
      guint i;

      if (fallback->color && !fallback->images)
        return gtk_css_value_is_computed (fallback->color);

      for (i = 0; i < fallback->n_images; i++)
        {
          if (!gtk_css_image_is_computed (fallback->images[i]))
            {
              return FALSE;
            }
        }
    }

  return TRUE;
}

static void
_gtk_css_image_fallback_class_init (GtkCssImageFallbackClass *klass)
{
  GtkCssImageClass *image_class = GTK_CSS_IMAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  image_class->get_width = gtk_css_image_fallback_get_width;
  image_class->get_height = gtk_css_image_fallback_get_height;
  image_class->get_aspect_ratio = gtk_css_image_fallback_get_aspect_ratio;
  image_class->snapshot = gtk_css_image_fallback_snapshot;
  image_class->parse = gtk_css_image_fallback_parse;
  image_class->compute = gtk_css_image_fallback_compute;
  image_class->print = gtk_css_image_fallback_print;
  image_class->equal = gtk_css_image_fallback_equal;
  image_class->is_computed = gtk_css_image_fallback_is_computed;

  object_class->dispose = gtk_css_image_fallback_dispose;
}

static void
_gtk_css_image_fallback_init (GtkCssImageFallback *image_fallback)
{
  image_fallback->used = -1;
}

GtkCssImage *
_gtk_css_image_fallback_new_for_color (GtkCssValue *color)
{
  GtkCssImageFallback *image;

  image = g_object_new (GTK_TYPE_CSS_IMAGE_FALLBACK, NULL);
  image->color = gtk_css_value_ref (color);

  return (GtkCssImage *)image;
}
