/*
 * Copyright (C) 2017 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "gsk/gsk.h"
#include "gsk/gskrendernodeprivate.h"
#include "gskpangoprivate.h"
#include "gtksnapshotprivate.h"
#include "gtktextlayoutprivate.h"
#include "gtktextviewprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkcsscolorvalueprivate.h"

#include <math.h>

#include <pango/pango.h>
#include <cairo.h>

G_DEFINE_TYPE (GskPangoRenderer, gsk_pango_renderer, PANGO_TYPE_RENDERER)

void
gsk_pango_renderer_set_state (GskPangoRenderer      *crenderer,
                              GskPangoRendererState  state)
{
  g_return_if_fail (GSK_IS_PANGO_RENDERER (crenderer));

  crenderer->state = state;
}

void
gsk_pango_renderer_set_shape_handler (GskPangoRenderer    *crenderer,
                                      GskPangoShapeHandler handler)
{
  g_return_if_fail (GSK_IS_PANGO_RENDERER (crenderer));

  crenderer->shape_handler = handler;
}

static void
get_color (GskPangoRenderer *crenderer,
           PangoRenderPart   part,
           GdkRGBA          *rgba)
{
  const PangoColor *color = pango_renderer_get_color ((PangoRenderer *) (crenderer), part);
  const guint16 a = pango_renderer_get_alpha ((PangoRenderer *) (crenderer), part);

  if (color)
    {
      rgba->red = color->red / 65535.;
      rgba->green = color->green / 65535.;
      rgba->blue = color->blue / 65535.;
      rgba->alpha = a ? a  / 65535. : crenderer->fg_color->alpha;
    }
  else
    {
      *rgba = *crenderer->fg_color;
      if (a)
        rgba->alpha = a / 65535.;
    }
}

static void
set_color (GskPangoRenderer *crenderer,
           PangoRenderPart   part,
           cairo_t          *cr)
{
  GdkRGBA rgba = { 0, 0, 0, 1 };

  get_color (crenderer, part, &rgba);
  gdk_cairo_set_source_rgba (cr, &rgba);
}

static void
gsk_pango_renderer_draw_glyph_item (PangoRenderer  *renderer,
                                    const char     *text,
                                    PangoGlyphItem *glyph_item,
                                    int             x,
                                    int             y)
{
  GskPangoRenderer *crenderer = (GskPangoRenderer *) (renderer);
  GdkRGBA color;

  get_color (crenderer, PANGO_RENDER_PART_FOREGROUND, &color);

  gtk_snapshot_append_text (crenderer->snapshot,
                            glyph_item->item->analysis.font,
                            glyph_item->glyphs,
                            &color,
                            (float) x / PANGO_SCALE,
                            (float) y / PANGO_SCALE);
}

static void
gsk_pango_renderer_draw_rectangle (PangoRenderer     *renderer,
                                   PangoRenderPart    part,
                                   int                x,
                                   int                y,
                                   int                width,
                                   int                height)
{
  GskPangoRenderer *crenderer = (GskPangoRenderer *) (renderer);
  GdkRGBA rgba;

  get_color (crenderer, part, &rgba);

  gtk_snapshot_append_color (crenderer->snapshot,
                             &rgba,
                             &GRAPHENE_RECT_INIT ((double)x / PANGO_SCALE,
                                                  (double)y / PANGO_SCALE,
                                                  (double)width / PANGO_SCALE,
                                                  (double)height / PANGO_SCALE));
}

static void
gsk_pango_renderer_draw_trapezoid (PangoRenderer   *renderer,
                                   PangoRenderPart  part,
                                   double           y1_,
                                   double           x11,
                                   double           x21,
                                   double           y2,
                                   double           x12,
                                   double           x22)
{
  GskPangoRenderer *crenderer = (GskPangoRenderer *) (renderer);
  PangoLayout *layout;
  PangoRectangle ink_rect;
  cairo_t *cr;
  double x, y;

  layout = pango_renderer_get_layout (renderer);
  if (!layout)
    return;

  pango_layout_get_pixel_extents (layout, &ink_rect, NULL);
  cr = gtk_snapshot_append_cairo (crenderer->snapshot,
                                  &GRAPHENE_RECT_INIT (ink_rect.x, ink_rect.y,
                                                       ink_rect.width, ink_rect.height));
  set_color (crenderer, part, cr);

  x = y = 0;
  cairo_user_to_device_distance (cr, &x, &y);
  cairo_identity_matrix (cr);
  cairo_translate (cr, x, y);

  cairo_move_to (cr, x11, y1_);
  cairo_line_to (cr, x21, y1_);
  cairo_line_to (cr, x22, y2);
  cairo_line_to (cr, x12, y2);
  cairo_close_path (cr);

  cairo_fill (cr);

  cairo_destroy (cr);
}

static void
gsk_pango_renderer_draw_error_underline (PangoRenderer *renderer,
                                         int            x,
                                         int            y,
                                         int            width,
                                         int            height)
{
  GskPangoRenderer *crenderer = (GskPangoRenderer *) (renderer);
  double xx, yy, ww, hh;
  GdkRGBA rgba;
  GskRoundedRect dot;

  xx = (double)x / PANGO_SCALE;
  yy = (double)y / PANGO_SCALE;
  ww = (double)width / PANGO_SCALE;
  hh = (double)height / PANGO_SCALE;

  get_color (crenderer, PANGO_RENDER_PART_UNDERLINE, &rgba);

  gtk_snapshot_push_repeat (crenderer->snapshot,
                            &GRAPHENE_RECT_INIT (xx, yy, ww, hh),
                            NULL);

  gsk_rounded_rect_init_from_rect (&dot,
                                   &GRAPHENE_RECT_INIT (xx, yy, hh, hh),
                                   hh / 2);

  gtk_snapshot_push_rounded_clip (crenderer->snapshot, &dot);
  gtk_snapshot_append_color (crenderer->snapshot, &rgba, &dot.bounds);
  gtk_snapshot_pop (crenderer->snapshot);
  gtk_snapshot_append_color (crenderer->snapshot,
                             &(GdkRGBA) { 0.f, 0.f, 0.f, 0.f },
                             &GRAPHENE_RECT_INIT (xx, yy, 1.5 * hh, hh));

  gtk_snapshot_pop (crenderer->snapshot);
}

static void
gsk_pango_renderer_draw_shape (PangoRenderer  *renderer,
                               PangoAttrShape *attr,
                               int             x,
                               int             y)
{
  GskPangoRenderer *crenderer = (GskPangoRenderer *) (renderer);
  PangoLayout *layout;
  PangoCairoShapeRendererFunc shape_renderer;
  gpointer shape_renderer_data;
  double base_x = (double)x / PANGO_SCALE;
  double base_y = (double)y / PANGO_SCALE;
  gboolean handled = FALSE;

  if (crenderer->shape_handler)
    {
      double shape_x = base_x;
      double shape_y = (double) (y + attr->logical_rect.y) / PANGO_SCALE;

      if (shape_x != 0 || shape_y != 0)
        {
          gtk_snapshot_save (crenderer->snapshot);
          gtk_snapshot_translate (crenderer->snapshot, &GRAPHENE_POINT_INIT (shape_x, shape_y));
        }

      handled = crenderer->shape_handler (attr,
                                          crenderer->snapshot,
                                          (double)attr->logical_rect.width / PANGO_SCALE,
                                          (double)attr->logical_rect.height / PANGO_SCALE);
      if (shape_x != 0 || shape_y != 0)
        gtk_snapshot_restore (crenderer->snapshot);
    }

  if (!handled)
    {
      cairo_t *cr;
      PangoRectangle ink_rect;

      layout = pango_renderer_get_layout (renderer);
      if (!layout)
        return;

      pango_layout_get_pixel_extents (layout, &ink_rect, NULL);
      cr = gtk_snapshot_append_cairo (crenderer->snapshot,
                                      &GRAPHENE_RECT_INIT (ink_rect.x, ink_rect.y,
                                                           ink_rect.width, ink_rect.height));
      shape_renderer = pango_cairo_context_get_shape_renderer (pango_layout_get_context (layout),
                                                               &shape_renderer_data);

      if (!shape_renderer)
        {
          cairo_destroy (cr);
          return;
        }

      set_color (crenderer, PANGO_RENDER_PART_FOREGROUND, cr);

      cairo_move_to (cr, base_x, base_y);

      shape_renderer (cr, attr, FALSE, shape_renderer_data);

      cairo_destroy (cr);
    }
}

static void
text_renderer_set_rgba (GskPangoRenderer *crenderer,
                        PangoRenderPart   part,
                        const GdkRGBA    *rgba)
{
  PangoRenderer *renderer = PANGO_RENDERER (crenderer);
  PangoColor color = { 0, };
  guint16 alpha;

  if (rgba)
    {
      color.red = (guint16)(rgba->red * 65535);
      color.green = (guint16)(rgba->green * 65535);
      color.blue = (guint16)(rgba->blue * 65535);
      alpha = (guint16)(rgba->alpha * 65535);
      pango_renderer_set_color (renderer, part, &color);
      pango_renderer_set_alpha (renderer, part, alpha);
    }
  else
    {
      pango_renderer_set_color (renderer, part, NULL);
      pango_renderer_set_alpha (renderer, part, 0);
    }
}

static GtkTextAppearance *
get_item_appearance (PangoItem *item)
{
  GSList *tmp_list = item->analysis.extra_attrs;

  while (tmp_list)
    {
      PangoAttribute *attr = tmp_list->data;

      if (attr->klass->type == gtk_text_attr_appearance_type)
        return &((GtkTextAttrAppearance *)attr)->appearance;

      tmp_list = tmp_list->next;
    }

  return NULL;
}

static void
gsk_pango_renderer_prepare_run (PangoRenderer  *renderer,
                                PangoLayoutRun *run)
{
  GskPangoRenderer *crenderer = GSK_PANGO_RENDERER (renderer);
  const GdkRGBA *bg_rgba = NULL;
  const GdkRGBA *fg_rgba = NULL;
  GtkTextAppearance *appearance;

  PANGO_RENDERER_CLASS (gsk_pango_renderer_parent_class)->prepare_run (renderer, run);

  appearance = get_item_appearance (run->item);

  if (appearance == NULL)
    return;

  if (appearance->draw_bg && crenderer->state == GSK_PANGO_RENDERER_NORMAL)
    bg_rgba = appearance->bg_rgba;
  else
    bg_rgba = NULL;

  text_renderer_set_rgba (crenderer, PANGO_RENDER_PART_BACKGROUND, bg_rgba);

  if (crenderer->state == GSK_PANGO_RENDERER_SELECTED &&
      GTK_IS_TEXT_VIEW (crenderer->widget))
    {
      GtkCssNode *node;
      GtkCssValue *value;

      node = gtk_text_view_get_selection_node ((GtkTextView *)crenderer->widget);
      value = gtk_css_node_get_style (node)->core->color;
      fg_rgba = gtk_css_color_value_get_rgba (value);
    }
  else if (crenderer->state == GSK_PANGO_RENDERER_CURSOR && gtk_widget_has_focus (crenderer->widget))
    {
      GtkCssNode *node;
      GtkCssValue *value;

      node = gtk_widget_get_css_node (crenderer->widget);
      value = gtk_css_node_get_style (node)->background->background_color;
      fg_rgba = gtk_css_color_value_get_rgba (value);
    }
  else
    fg_rgba = appearance->fg_rgba;

  text_renderer_set_rgba (crenderer, PANGO_RENDER_PART_FOREGROUND, fg_rgba);

  if (appearance->strikethrough_rgba)
    text_renderer_set_rgba (crenderer, PANGO_RENDER_PART_STRIKETHROUGH, appearance->strikethrough_rgba);
  else
    text_renderer_set_rgba (crenderer, PANGO_RENDER_PART_STRIKETHROUGH, fg_rgba);

  if (appearance->underline_rgba)
    text_renderer_set_rgba (crenderer, PANGO_RENDER_PART_UNDERLINE, appearance->underline_rgba);
  else if (appearance->underline == PANGO_UNDERLINE_ERROR)
    {
      if (!crenderer->error_color)
        {
          static const GdkRGBA red = { 1, 0, 0, 1 };
          crenderer->error_color = gdk_rgba_copy (&red);
        }

      text_renderer_set_rgba (crenderer, PANGO_RENDER_PART_UNDERLINE, crenderer->error_color);
    }
  else
    text_renderer_set_rgba (crenderer, PANGO_RENDER_PART_UNDERLINE, fg_rgba);
}

static void
gsk_pango_renderer_init (GskPangoRenderer *renderer G_GNUC_UNUSED)
{
}

static void
gsk_pango_renderer_class_init (GskPangoRendererClass *klass)
{
  PangoRendererClass *renderer_class = PANGO_RENDERER_CLASS (klass);

  renderer_class->draw_glyph_item = gsk_pango_renderer_draw_glyph_item;
  renderer_class->draw_rectangle = gsk_pango_renderer_draw_rectangle;
  renderer_class->draw_trapezoid = gsk_pango_renderer_draw_trapezoid;
  renderer_class->draw_error_underline = gsk_pango_renderer_draw_error_underline;
  renderer_class->draw_shape = gsk_pango_renderer_draw_shape;
  renderer_class->prepare_run = gsk_pango_renderer_prepare_run;
}

static GskPangoRenderer *cached_renderer = NULL; /* MT-safe */
G_LOCK_DEFINE_STATIC (cached_renderer);

GskPangoRenderer *
gsk_pango_renderer_acquire (void)
{
  GskPangoRenderer *renderer;

  if (G_LIKELY (G_TRYLOCK (cached_renderer)))
    {
      if (G_UNLIKELY (!cached_renderer))
        {
          cached_renderer = g_object_new (GSK_TYPE_PANGO_RENDERER, NULL);
          cached_renderer->is_cached_renderer = TRUE;
        }

      renderer = cached_renderer;

      /* Reset to standard state */
      renderer->state = GSK_PANGO_RENDERER_NORMAL;
      renderer->shape_handler = NULL;
    }
  else
    {
      renderer = g_object_new (GSK_TYPE_PANGO_RENDERER, NULL);
    }

  return renderer;
}

void
gsk_pango_renderer_release (GskPangoRenderer *renderer)
{
  if (G_LIKELY (renderer->is_cached_renderer))
    {
      renderer->widget = NULL;
      renderer->snapshot = NULL;

      if (renderer->error_color)
        {
          gdk_rgba_free (renderer->error_color);
          renderer->error_color = NULL;
        }

      G_UNLOCK (cached_renderer);
    }
  else
    g_object_unref (renderer);
}

/**
 * gtk_snapshot_append_layout:
 * @snapshot: a `GtkSnapshot`
 * @layout: the `PangoLayout` to render
 * @color: the foreground color to render the layout in
 *
 * Creates render nodes for rendering @layout in the given foregound @color
 * and appends them to the current node of @snapshot without changing the
 * current node. The current theme's foreground color for a widget can be
 * obtained with [method@Gtk.Widget.get_color].
 *
 * Note that if the layout does not produce any visible output, then nodes
 * may not be added to the @snapshot.
 **/
void
gtk_snapshot_append_layout (GtkSnapshot   *snapshot,
                            PangoLayout   *layout,
                            const GdkRGBA *color)
{
  GskPangoRenderer *crenderer;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (PANGO_IS_LAYOUT (layout));

  crenderer = gsk_pango_renderer_acquire ();

  crenderer->snapshot = snapshot;
  crenderer->fg_color = color;

  pango_renderer_draw_layout (PANGO_RENDERER (crenderer), layout, 0, 0);

  gsk_pango_renderer_release (crenderer);
}
