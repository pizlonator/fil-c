/*
 * Copyright © 2012 Red Hat Inc.
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

#include <math.h>
#include <string.h>

#include "gtkcssimagecrossfadeprivate.h"

#include "gtkcssnumbervalueprivate.h"
#include "gtkcssimagefallbackprivate.h"
#include "gtkcsscolorvalueprivate.h"


typedef struct _CrossFadeEntry CrossFadeEntry;

struct _CrossFadeEntry
{
  double progress;
  gboolean has_progress;
  GtkCssImage *image;
};

G_DEFINE_TYPE (GtkCssImageCrossFade, gtk_css_image_cross_fade, GTK_TYPE_CSS_IMAGE)

static void
cross_fade_entry_clear (gpointer data)
{
  CrossFadeEntry *entry = data;

  g_clear_object (&entry->image);
}

static void
gtk_css_image_cross_fade_recalculate_progress (GtkCssImageCrossFade *self)
{
  double total_progress;
  guint n_no_progress;
  guint i;

  total_progress = 0.0;
  n_no_progress = 0;

  for (i = 0; i < self->images->len; i++)
    {
      CrossFadeEntry *entry = &g_array_index (self->images, CrossFadeEntry, i);

      if (entry->has_progress)
        total_progress += entry->progress;
      else
        n_no_progress++;
    }

  if (n_no_progress)
    {
      double progress;
      if (total_progress >= 1.0)
        {
          progress = 0.0;
        }
      else
        {
          progress = (1.0 - total_progress) / n_no_progress;
          total_progress = 1.0;
        }
      for (i = 0; i < self->images->len; i++)
        {
          CrossFadeEntry *entry = &g_array_index (self->images, CrossFadeEntry, i);

          if (!entry->has_progress)
            entry->progress = progress;
        }
    }

  self->total_progress = total_progress;
}

static void
gtk_css_image_cross_fade_add (GtkCssImageCrossFade *self,
                              gboolean              has_progress,
                              double                progress,
                              GtkCssImage          *image)
{
  CrossFadeEntry entry;

  entry.has_progress = has_progress;
  entry.progress = progress;
  entry.image = image;
  g_array_append_val (self->images, entry);

  gtk_css_image_cross_fade_recalculate_progress (self);
}

static GtkCssImageCrossFade *
gtk_css_image_cross_fade_new_empty (void)
{
  return g_object_new (GTK_TYPE_CSS_IMAGE_CROSS_FADE, NULL);
}

/* XXX: The following is not correct, it should actually run the
 * CSS sizing algorithm for every child, not just query height and
 * width independently.
 */
static int
gtk_css_image_cross_fade_get_width (GtkCssImage *image)
{
  GtkCssImageCrossFade *self = GTK_CSS_IMAGE_CROSS_FADE (image);
  double sum_width, sum_progress;
  guint i;

  sum_width = 0.0;
  sum_progress = 0.0;

  for (i = 0; i < self->images->len; i++)
    {
      CrossFadeEntry *entry = &g_array_index (self->images, CrossFadeEntry, i);
      int image_width;

      image_width = _gtk_css_image_get_width (entry->image);
      if (image_width == 0)
        continue;
      sum_width += image_width * entry->progress;
      sum_progress += entry->progress;
    }

  if (sum_progress <= 0.0)
    return 0;

  return ceil (sum_width / sum_progress);
}

static int
gtk_css_image_cross_fade_get_height (GtkCssImage *image)
{
  GtkCssImageCrossFade *self = GTK_CSS_IMAGE_CROSS_FADE (image);
  double sum_height, sum_progress;
  guint i;

  sum_height = 0.0;
  sum_progress = 0.0;

  for (i = 0; i < self->images->len; i++)
    {
      CrossFadeEntry *entry = &g_array_index (self->images, CrossFadeEntry, i);
      int image_height;

      image_height = _gtk_css_image_get_height (entry->image);
      if (image_height == 0)
        continue;
      sum_height += image_height * entry->progress;
      sum_progress += entry->progress;
    }

  if (sum_progress <= 0.0)
    return 0;

  return ceil (sum_height / sum_progress);
}

static gboolean
gtk_css_image_cross_fade_equal (GtkCssImage *image1,
                                GtkCssImage *image2)
{
  GtkCssImageCrossFade *cross_fade1 = GTK_CSS_IMAGE_CROSS_FADE (image1);
  GtkCssImageCrossFade *cross_fade2 = GTK_CSS_IMAGE_CROSS_FADE (image2);
  guint i;

  if (cross_fade1->images->len != cross_fade2->images->len)
    return FALSE;

  for (i = 0; i < cross_fade1->images->len; i++)
    {
      CrossFadeEntry *entry1 = &g_array_index (cross_fade1->images, CrossFadeEntry, i);
      CrossFadeEntry *entry2 = &g_array_index (cross_fade2->images, CrossFadeEntry, i);

      if (entry1->progress != entry2->progress ||
          !_gtk_css_image_equal (entry1->image, entry2->image))
        return FALSE;
    }
  
  return TRUE;
}

static gboolean
gtk_css_image_cross_fade_is_dynamic (GtkCssImage *image)
{
  GtkCssImageCrossFade *self = GTK_CSS_IMAGE_CROSS_FADE (image);
  guint i;

  for (i = 0; i < self->images->len; i++)
    {
      CrossFadeEntry *entry = &g_array_index (self->images, CrossFadeEntry, i);

      if (gtk_css_image_is_dynamic (entry->image))
        return TRUE;
    }

  return FALSE;
}

static GtkCssImage *
gtk_css_image_cross_fade_get_dynamic_image (GtkCssImage *image,
                                            gint64       monotonic_time)
{
  GtkCssImageCrossFade *self = GTK_CSS_IMAGE_CROSS_FADE (image);
  GtkCssImageCrossFade *result;
  guint i;

  result = gtk_css_image_cross_fade_new_empty ();

  for (i = 0; i < self->images->len; i++)
    {
      CrossFadeEntry *entry = &g_array_index (self->images, CrossFadeEntry, i);

      gtk_css_image_cross_fade_add (result,
                                    entry->has_progress,
                                    entry->progress,
                                    gtk_css_image_get_dynamic_image (entry->image, monotonic_time));
    }

  return GTK_CSS_IMAGE (result);
}

static void
gtk_css_image_cross_fade_snapshot (GtkCssImage *image,
                                   GtkSnapshot *snapshot,
                                   double       width,
                                   double       height)
{
  GtkCssImageCrossFade *self = GTK_CSS_IMAGE_CROSS_FADE (image);
  double remaining;
  guint i, n_cross_fades;

  if (self->total_progress < 1.0)
    {
      n_cross_fades = self->images->len;
      remaining = 1.0;
    }
  else
    {
      n_cross_fades = self->images->len - 1;
      remaining = self->total_progress;
    }

  for (i = 0; i < n_cross_fades; i++)
    {
      CrossFadeEntry *entry = &g_array_index (self->images, CrossFadeEntry, i);

      gtk_snapshot_push_cross_fade (snapshot, 1.0 - entry->progress / remaining);
      remaining -= entry->progress;
      gtk_css_image_snapshot (entry->image, snapshot, width, height);
      gtk_snapshot_pop (snapshot);
    }

  if (n_cross_fades < self->images->len)
    {
      CrossFadeEntry *entry = &g_array_index (self->images, CrossFadeEntry, self->images->len - 1);
      gtk_css_image_snapshot (entry->image, snapshot, width, height);
    }

  for (i = 0; i < n_cross_fades; i++)
    {
      gtk_snapshot_pop (snapshot);
    }
}

static gboolean
parse_progress (GtkCssParser *parser,
                gpointer      option_data,
                gpointer      user_data)
{
  double *progress = option_data;
  GtkCssValue *number;
  
  number = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_PERCENT | GTK_CSS_POSITIVE_ONLY);
  if (number == NULL)
    return FALSE;
  *progress = _gtk_css_number_value_get (number, 1);
  _gtk_css_value_unref (number);

  if (*progress > 1.0)
    {
      gtk_css_parser_error_value (parser, "Percentages over 100%% are not allowed. Given value: %f", *progress);
      return FALSE;
    }

  return TRUE;
}

static gboolean
parse_image (GtkCssParser *parser,
             gpointer      option_data,
             gpointer      user_data)
{
  GtkCssImage **image = option_data;

  if (_gtk_css_image_can_parse (parser))
    {
      *image = _gtk_css_image_new_parse (parser);
      if (*image == NULL)
        return FALSE;

      return TRUE;
    }
  else if (gtk_css_color_value_can_parse (parser))
    {
      GtkCssValue *color;

      color = _gtk_css_color_value_parse (parser);
      if (color == NULL)
        return FALSE;

      *image = _gtk_css_image_fallback_new_for_color (color);

      return TRUE;
    }
  
  return FALSE;
}

static guint
gtk_css_image_cross_fade_parse_arg (GtkCssParser *parser,
                                    guint         arg,
                                    gpointer      data)
{
  GtkCssImageCrossFade *self = data;
  double progress = -1.0;
  GtkCssImage *image = NULL;
  GtkCssParseOption options[] =
    {
      { (void *)gtk_css_number_value_can_parse, parse_progress, &progress },
      { NULL, parse_image, &image },
    };

  if (!gtk_css_parser_consume_any (parser, options, G_N_ELEMENTS (options), self))
    return 0;

  g_assert (image != NULL);

  if (progress < 0.0)
    gtk_css_image_cross_fade_add (self, FALSE, 0.0, image);
  else
    gtk_css_image_cross_fade_add (self, TRUE, progress, image);

  return 1;
}

static gboolean
gtk_css_image_cross_fade_parse (GtkCssImage  *image,
                                GtkCssParser *parser)
{
  if (!gtk_css_parser_has_function (parser, "cross-fade"))
    {
      gtk_css_parser_error_syntax (parser, "Expected 'cross-fade('");
      return FALSE;
    }

  return gtk_css_parser_consume_function (parser, 1, G_MAXUINT, gtk_css_image_cross_fade_parse_arg, image);
}

static void
gtk_css_image_cross_fade_print (GtkCssImage *image,
                                GString     *string)
{
  GtkCssImageCrossFade *self = GTK_CSS_IMAGE_CROSS_FADE (image);
  guint i;

  g_string_append (string, "cross-fade(");

  for (i = 0; i < self->images->len; i++)
    {
      CrossFadeEntry *entry = &g_array_index (self->images, CrossFadeEntry, i);

      if (i > 0)
        g_string_append_printf (string, ", ");
      if (entry->has_progress)
        g_string_append_printf (string, "%g%% ", entry->progress * 100.0);
      _gtk_css_image_print (entry->image, string);
    }

  g_string_append (string, ")");
}

static GtkCssImage *
gtk_css_image_cross_fade_compute (GtkCssImage      *image,
                                  guint             property_id,
                                  GtkStyleProvider *provider,
                                  GtkCssStyle      *style,
                                  GtkCssStyle      *parent_style)
{
  GtkCssImageCrossFade *self = GTK_CSS_IMAGE_CROSS_FADE (image);
  GtkCssImageCrossFade *result;
  guint i;

  result = gtk_css_image_cross_fade_new_empty ();

  for (i = 0; i < self->images->len; i++)
    {
      CrossFadeEntry *entry = &g_array_index (self->images, CrossFadeEntry, i);

      gtk_css_image_cross_fade_add (result,
                                    entry->has_progress,
                                    entry->progress,
                                    _gtk_css_image_compute (entry->image, property_id, provider, style, parent_style));
    }

  return GTK_CSS_IMAGE (result);
}

static void
gtk_css_image_cross_fade_dispose (GObject *object)
{
  GtkCssImageCrossFade *cross_fade = GTK_CSS_IMAGE_CROSS_FADE (object);

  g_clear_pointer (&cross_fade->images, g_array_unref);

  G_OBJECT_CLASS (gtk_css_image_cross_fade_parent_class)->dispose (object);
}

static gboolean
gtk_css_image_cross_fade_is_computed (GtkCssImage *image)
{
  GtkCssImageCrossFade *cross_fade = GTK_CSS_IMAGE_CROSS_FADE (image);
  guint i;

  for (i = 0; i < cross_fade->images->len; i++)
    {
      const CrossFadeEntry *entry = &g_array_index (cross_fade->images, CrossFadeEntry, i);
      if (!gtk_css_image_is_computed (entry->image))
        return FALSE;
    }

  return TRUE;
}

static void
gtk_css_image_cross_fade_class_init (GtkCssImageCrossFadeClass *klass)
{
  GtkCssImageClass *image_class = GTK_CSS_IMAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  image_class->get_width = gtk_css_image_cross_fade_get_width;
  image_class->get_height = gtk_css_image_cross_fade_get_height;
  image_class->compute = gtk_css_image_cross_fade_compute;
  image_class->equal = gtk_css_image_cross_fade_equal;
  image_class->snapshot = gtk_css_image_cross_fade_snapshot;
  image_class->is_dynamic = gtk_css_image_cross_fade_is_dynamic;
  image_class->get_dynamic_image = gtk_css_image_cross_fade_get_dynamic_image;
  image_class->parse = gtk_css_image_cross_fade_parse;
  image_class->print = gtk_css_image_cross_fade_print;
  image_class->is_computed = gtk_css_image_cross_fade_is_computed;

  object_class->dispose = gtk_css_image_cross_fade_dispose;
}

static void
gtk_css_image_cross_fade_init (GtkCssImageCrossFade *self)
{
  self->images = g_array_new (FALSE, FALSE, sizeof (CrossFadeEntry));
  g_array_set_clear_func (self->images, cross_fade_entry_clear);
}

GtkCssImage *
_gtk_css_image_cross_fade_new (GtkCssImage *start,
                               GtkCssImage *end,
                               double       progress)
{
  GtkCssImageCrossFade *self;

  g_return_val_if_fail (start == NULL || GTK_IS_CSS_IMAGE (start), NULL);
  g_return_val_if_fail (end == NULL || GTK_IS_CSS_IMAGE (end), NULL);

  self = gtk_css_image_cross_fade_new_empty ();

  if (start)
    gtk_css_image_cross_fade_add (self, TRUE, 1.0 - progress, g_object_ref (start));
  if (end)
    gtk_css_image_cross_fade_add (self, TRUE, progress, g_object_ref (end));

  return GTK_CSS_IMAGE (self);
}

