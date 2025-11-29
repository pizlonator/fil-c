/*
 * bluroverlay.c
 * This file is part of gtk
 *
 * Copyright (C) 2011 - Ignacio Casal Quinteiro, Mike Krüger
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

#include "bluroverlay.h"

/*
 * This is a cut-down copy of gtkoverlay.c with a custom snapshot
 * function that support a limited form of blur-under.
 */
typedef struct _BlurOverlayChild BlurOverlayChild;

struct _BlurOverlayChild
{
  double blur;
};

enum {
  GET_CHILD_POSITION,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };
static GQuark child_data_quark = 0;

G_DEFINE_TYPE (BlurOverlay, blur_overlay, GTK_TYPE_WIDGET)

static void
blur_overlay_set_overlay_child (GtkWidget       *widget,
                               BlurOverlayChild *child_data)
{
  g_object_set_qdata_full (G_OBJECT (widget), child_data_quark, child_data, g_free);
}

static BlurOverlayChild *
blur_overlay_get_overlay_child (GtkWidget *widget)
{
  return (BlurOverlayChild *) g_object_get_qdata (G_OBJECT (widget), child_data_quark);
}

static void
blur_overlay_measure (GtkWidget      *widget,
                     GtkOrientation  orientation,
                     int             for_size,
                     int            *minimum,
                     int            *natural,
                     int            *minimum_baseline,
                     int            *natural_baseline)
{
  GtkWidget *child;

  for (child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      int child_min, child_nat, child_min_baseline, child_nat_baseline;

      gtk_widget_measure (child,
                          orientation,
                          for_size,
                          &child_min, &child_nat,
                          &child_min_baseline, &child_nat_baseline);

      *minimum = MAX (*minimum, child_min);
      *natural = MAX (*natural, child_nat);
      if (child_min_baseline > -1)
        *minimum_baseline = MAX (*minimum_baseline, child_min_baseline);
      if (child_nat_baseline > -1)
        *natural_baseline = MAX (*natural_baseline, child_nat_baseline);
    }
}

static void
blur_overlay_compute_child_allocation (BlurOverlay      *overlay,
                                      GtkWidget       *widget,
                                      BlurOverlayChild *child,
                                      GtkAllocation   *widget_allocation)
{
  GtkAllocation allocation;
  gboolean result;

  g_signal_emit (overlay, signals[GET_CHILD_POSITION],
                 0, widget, &allocation, &result);

  widget_allocation->x = allocation.x;
  widget_allocation->y = allocation.y;
  widget_allocation->width = allocation.width;
  widget_allocation->height = allocation.height;
}

static GtkAlign
effective_align (GtkAlign         align,
                 GtkTextDirection direction)
{
  switch (align)
    {
    case GTK_ALIGN_START:
      return direction == GTK_TEXT_DIR_RTL ? GTK_ALIGN_END : GTK_ALIGN_START;
    case GTK_ALIGN_END:
      return direction == GTK_TEXT_DIR_RTL ? GTK_ALIGN_START : GTK_ALIGN_END;
    case GTK_ALIGN_FILL:
    case GTK_ALIGN_CENTER:
    case GTK_ALIGN_BASELINE_FILL:
    case GTK_ALIGN_BASELINE_CENTER:
    default:
      return align;
    }
}

static void
blur_overlay_child_update_style_classes (BlurOverlay *overlay,
                                         GtkWidget *child,
                                         GtkAllocation *child_allocation)
{
  int width, height;
  GtkAlign valign, halign;
  gboolean is_left, is_right, is_top, is_bottom;
  gboolean has_left, has_right, has_top, has_bottom;

  has_left = gtk_widget_has_css_class (child, "left");
  has_right = gtk_widget_has_css_class (child, "right");
  has_top = gtk_widget_has_css_class (child, "top");
  has_bottom = gtk_widget_has_css_class (child, "bottom");

  is_left = is_right = is_top = is_bottom = FALSE;

  width = gtk_widget_get_width (GTK_WIDGET (overlay));
  height = gtk_widget_get_height (GTK_WIDGET (overlay));

  halign = effective_align (gtk_widget_get_halign (child),
                            gtk_widget_get_direction (child));

  if (halign == GTK_ALIGN_START)
    is_left = (child_allocation->x == 0);
  else if (halign == GTK_ALIGN_END)
    is_right = (child_allocation->x + child_allocation->width == width);

  valign = gtk_widget_get_valign (child);

  if (valign == GTK_ALIGN_START)
    is_top = (child_allocation->y == 0);
  else if (valign == GTK_ALIGN_END)
    is_bottom = (child_allocation->y + child_allocation->height == height);

  if (has_left && !is_left)
    gtk_widget_remove_css_class (child, "left");
  else if (!has_left && is_left)
    gtk_widget_add_css_class (child, "left");

  if (has_right && !is_right)
    gtk_widget_remove_css_class (child, "right");
  else if (!has_right && is_right)
    gtk_widget_add_css_class (child, "right");

  if (has_top && !is_top)
    gtk_widget_remove_css_class (child, "top");
  else if (!has_top && is_top)
    gtk_widget_add_css_class (child, "top");

  if (has_bottom && !is_bottom)
    gtk_widget_remove_css_class (child, "bottom");
  else if (!has_bottom && is_bottom)
    gtk_widget_add_css_class (child, "bottom");
}

static void
blur_overlay_child_allocate (BlurOverlay      *overlay,
                            GtkWidget       *widget,
                            BlurOverlayChild *child)
{
  GtkAllocation child_allocation;

  if (!gtk_widget_get_visible (widget))
    return;

  blur_overlay_compute_child_allocation (overlay, widget, child, &child_allocation);

  blur_overlay_child_update_style_classes (overlay, widget, &child_allocation);
  gtk_widget_size_allocate (widget, &child_allocation, -1);
}

static void
blur_overlay_size_allocate (GtkWidget *widget,
                           int        width,
                           int        height,
                           int        baseline)
{
  BlurOverlay *overlay = BLUR_OVERLAY (widget);
  GtkWidget *child;
  GtkWidget *main_widget;

  main_widget = overlay->main_widget;
  if (main_widget && gtk_widget_get_visible (main_widget))
    gtk_widget_size_allocate (main_widget,
                              &(GtkAllocation) {
                                0, 0,
                                width, height
                              }, -1);

  for (child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (child != main_widget)
        {
          BlurOverlayChild *child_data = blur_overlay_get_overlay_child (child);
          blur_overlay_child_allocate (overlay, child, child_data);
        }
    }
}

static gboolean
blur_overlay_get_child_position (BlurOverlay    *overlay,
                                 GtkWidget      *widget,
                                 GtkAllocation  *alloc)
{
  GtkRequisition min, req;
  GtkAlign halign;
  GtkTextDirection direction;
  int width, height;

  gtk_widget_get_preferred_size (widget, &min, &req);
  width = gtk_widget_get_width (GTK_WIDGET (overlay));
  height = gtk_widget_get_height (GTK_WIDGET (overlay));

  alloc->x = 0;
  alloc->width = MAX (min.width, MIN (width, req.width));

  direction = gtk_widget_get_direction (widget);

  halign = gtk_widget_get_halign (widget);
  switch (effective_align (halign, direction))
    {
    case GTK_ALIGN_START:
      /* nothing to do */
      break;
    case GTK_ALIGN_FILL:
      alloc->width = MAX (alloc->width, width);
      break;
    case GTK_ALIGN_CENTER:
      alloc->x += width / 2 - alloc->width / 2;
      break;
    case GTK_ALIGN_END:
      alloc->x += width - alloc->width;
      break;
    case GTK_ALIGN_BASELINE_FILL:
    case GTK_ALIGN_BASELINE_CENTER:
    default:
      g_assert_not_reached ();
      break;
    }

  alloc->y = 0;
  alloc->height = MAX  (min.height, MIN (height, req.height));

  switch (gtk_widget_get_valign (widget))
    {
    case GTK_ALIGN_START:
      /* nothing to do */
      break;
    case GTK_ALIGN_FILL:
      alloc->height = MAX (alloc->height, height);
      break;
    case GTK_ALIGN_CENTER:
      alloc->y += height / 2 - alloc->height / 2;
      break;
    case GTK_ALIGN_END:
      alloc->y += height - alloc->height;
      break;
    case GTK_ALIGN_BASELINE_FILL:
    case GTK_ALIGN_BASELINE_CENTER:
    default:
      g_assert_not_reached ();
      break;
    }

  return TRUE;
}

static void
blur_overlay_snapshot (GtkWidget   *widget,
                      GtkSnapshot *snapshot)
{
  GtkWidget *main_widget;
  GskRenderNode *main_widget_node = NULL;
  GtkWidget *child;
  int width, height;
  cairo_region_t *clip = NULL;
  int i;

  main_widget = BLUR_OVERLAY (widget)->main_widget;
  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  for (child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      BlurOverlayChild *child_info = blur_overlay_get_overlay_child (child);
      double blur = 0;
      if (child_info)
        blur = child_info->blur;

      if (blur > 0)
        {
          cairo_rectangle_int_t rect;
          graphene_rect_t bounds;

          if (main_widget_node == NULL)
            {
              GtkSnapshot *child_snapshot;

              child_snapshot = gtk_snapshot_new ();
              gtk_widget_snapshot_child (widget, main_widget, child_snapshot);
              main_widget_node = gtk_snapshot_free_to_node (child_snapshot);
            }

          if (!gtk_widget_compute_bounds (child, gtk_widget_get_parent (child), &bounds))
            graphene_rect_init (&bounds, 0, 0, 0, 0);
          gtk_snapshot_push_blur (snapshot, blur);
          gtk_snapshot_push_clip (snapshot, &bounds);
          gtk_snapshot_append_node (snapshot, main_widget_node);
          gtk_snapshot_pop (snapshot);
          gtk_snapshot_pop (snapshot);

          if (clip == NULL)
            {
              rect.x = rect.y = 0;
              rect.width = width;
              rect.height = height;
              clip = cairo_region_create_rectangle (&rect);
            }

          rect.x = floor (bounds.origin.x);
          rect.y = floor (bounds.origin.y);
          rect.width = ceil (bounds.origin.x + bounds.size.width - rect.x);
          rect.height = ceil (bounds.origin.y + bounds.size.height - rect.y);
          cairo_region_subtract_rectangle (clip, &rect);
        }
    }

  if (clip == NULL)
    {
      for (child = gtk_widget_get_first_child (widget);
           child != NULL;
           child = gtk_widget_get_next_sibling (child))
        {
          gtk_widget_snapshot_child (widget, child, snapshot);
        }
      return;
    }

  for (i = 0; i < cairo_region_num_rectangles (clip); i++)
    {
      cairo_rectangle_int_t rect;
      graphene_rect_t bounds;

      cairo_region_get_rectangle (clip, i, &rect);
      graphene_rect_init (&bounds, rect.x, rect.y, rect.width, rect.height);
      gtk_snapshot_push_clip (snapshot, &bounds);
      gtk_snapshot_append_node (snapshot, main_widget_node);
      gtk_snapshot_pop (snapshot);
    }

  cairo_region_destroy (clip);

  for (child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (child != main_widget)
        gtk_widget_snapshot_child (widget, child, snapshot);
    }

  gsk_render_node_unref (main_widget_node);
}

static void
blur_overlay_dispose (GObject *object)
{
  BlurOverlay *overlay = BLUR_OVERLAY (object);
  GtkWidget *child;

  g_clear_pointer (&overlay->main_widget, gtk_widget_unparent);

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (overlay))))
    gtk_widget_unparent (child);

  G_OBJECT_CLASS (blur_overlay_parent_class)->dispose (object);
}

static void
blur_overlay_class_init (BlurOverlayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = blur_overlay_dispose;

  widget_class->measure = blur_overlay_measure;
  widget_class->size_allocate = blur_overlay_size_allocate;
  widget_class->snapshot = blur_overlay_snapshot;

  klass->get_child_position = blur_overlay_get_child_position;

  signals[GET_CHILD_POSITION] =
    g_signal_new ("get-child-position",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (BlurOverlayClass, get_child_position),
                  g_signal_accumulator_true_handled, NULL,
                  NULL,
                  G_TYPE_BOOLEAN, 2,
                  GTK_TYPE_WIDGET,
                  GDK_TYPE_RECTANGLE | G_SIGNAL_TYPE_STATIC_SCOPE);

  child_data_quark = g_quark_from_static_string ("gtk-overlay-child-data");

  gtk_widget_class_set_css_name (widget_class, "overlay");
}

static void
blur_overlay_init (BlurOverlay *overlay)
{
}

GtkWidget *
blur_overlay_new (void)
{
  return g_object_new (BLUR_TYPE_OVERLAY, NULL);
}

void
blur_overlay_add_overlay (BlurOverlay *overlay,
                          GtkWidget   *widget,
                          double       blur)
{
  BlurOverlayChild *child = g_new0 (BlurOverlayChild, 1);

  gtk_widget_insert_before (widget, GTK_WIDGET (overlay), NULL);

  child->blur = blur;

  blur_overlay_set_overlay_child (widget, child);
}

void
blur_overlay_set_child (BlurOverlay *overlay,
                        GtkWidget   *widget)
{
  gtk_widget_insert_after (widget, GTK_WIDGET (overlay), NULL);
  overlay->main_widget = widget;
}
