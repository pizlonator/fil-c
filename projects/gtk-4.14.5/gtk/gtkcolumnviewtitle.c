/*
 * Copyright © 2019 Benjamin Otte
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

#include "gtkcolumnviewtitleprivate.h"

#include "gtkcolumnviewprivate.h"
#include "gtkcolumnviewcolumnprivate.h"
#include "gtkcolumnviewsorterprivate.h"
#include "gtkcssboxesprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkprivate.h"
#include "gtklabel.h"
#include "gtkwidgetprivate.h"
#include "gtkbox.h"
#include "gtkbuiltiniconprivate.h"
#include "gtkgestureclick.h"
#include "gtkpopovermenu.h"
#include "gtknative.h"

struct _GtkColumnViewTitle
{
  GtkWidget parent_instance;

  GtkColumnViewColumn *column;

  GtkWidget *box;
  GtkWidget *title;
  GtkWidget *sort;
  GtkWidget *popup_menu;
};

struct _GtkColumnViewTitleClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (GtkColumnViewTitle, gtk_column_view_title, GTK_TYPE_WIDGET)

static int
unadjust_width (GtkWidget *widget,
                int        width)
{
  GtkCssBoxes boxes;

  if (width <= -1)
    return -1;

  gtk_css_boxes_init_border_box (&boxes,
                                 gtk_css_node_get_style (gtk_widget_get_css_node (widget)),
                                 0, 0,
                                 width, 100000);
  return MAX (0, floor (gtk_css_boxes_get_content_rect (&boxes)->size.width));
}

static void
gtk_column_view_title_measure (GtkWidget      *widget,
                               GtkOrientation  orientation,
                               int             for_size,
                               int            *minimum,
                               int            *natural,
                               int            *minimum_baseline,
                               int            *natural_baseline)
{
  GtkColumnViewTitle *self = GTK_COLUMN_VIEW_TITLE (widget);
  GtkWidget *child = gtk_widget_get_first_child (widget);
  int fixed_width, unadj_width;

  fixed_width = gtk_column_view_column_get_fixed_width (self->column);
  unadj_width = unadjust_width (widget, fixed_width);

  if (orientation == GTK_ORIENTATION_VERTICAL)
    {
      if (fixed_width > -1)
        {
          int min;

          if (for_size == -1)
            for_size = unadj_width;
          else
            for_size = MIN (for_size, unadj_width);

          gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL, -1, &min, NULL, NULL, NULL);
          for_size = MAX (for_size, min);
        }
    }

  if (child)
    gtk_widget_measure (child, orientation, for_size, minimum, natural, minimum_baseline, natural_baseline);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (fixed_width > -1)
        {
          *minimum = 0;
          *natural = unadj_width;
        }
    }
}

static void
gtk_column_view_title_size_allocate (GtkWidget *widget,
                                     int        width,
                                     int        height,
                                     int        baseline)
{
  GtkColumnViewTitle *self = GTK_COLUMN_VIEW_TITLE (widget);
  GtkWidget *child = gtk_widget_get_first_child (widget);

  if (child)
    {
      int min;

      gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL, height, &min, NULL, NULL, NULL);

      gtk_widget_allocate (child, MAX (min, width), height, baseline, NULL);
    }

  if (self->popup_menu)
    gtk_popover_present (GTK_POPOVER (self->popup_menu));
}

static void
gtk_column_view_title_dispose (GObject *object)
{
  GtkColumnViewTitle *self = GTK_COLUMN_VIEW_TITLE (object);

  g_clear_pointer (&self->box, gtk_widget_unparent);
  g_clear_pointer (&self->popup_menu, gtk_widget_unparent);

  g_clear_object (&self->column);

  G_OBJECT_CLASS (gtk_column_view_title_parent_class)->dispose (object);
}

static void
gtk_column_view_title_class_init (GtkColumnViewTitleClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  widget_class->measure = gtk_column_view_title_measure;
  widget_class->size_allocate = gtk_column_view_title_size_allocate;

  gobject_class->dispose = gtk_column_view_title_dispose;

  gtk_widget_class_set_css_name (widget_class, I_("button"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_COLUMN_HEADER);
}

static void
gtk_column_view_title_resize_func (GtkWidget *widget)
{
  GtkColumnViewTitle *self = GTK_COLUMN_VIEW_TITLE (widget);

  if (self->column)
    gtk_column_view_column_queue_resize (self->column);
}

static void
activate_sort (GtkColumnViewTitle *self)
{
  GtkSorter *sorter;
  GtkColumnView *view;
  GtkColumnViewSorter *view_sorter;

  sorter = gtk_column_view_column_get_sorter (self->column);
  if (sorter == NULL)
    return;

  view = gtk_column_view_column_get_column_view (self->column);
  view_sorter = GTK_COLUMN_VIEW_SORTER (gtk_column_view_get_sorter (view));
  gtk_column_view_sorter_add_column (view_sorter, self->column);
}

static void
show_menu (GtkColumnViewTitle *self,
           double              x,
           double              y)
{
  if (!self->popup_menu)
    {
      GMenuModel *model;

      model = gtk_column_view_column_get_header_menu (self->column);
      if (!model)
        return;

      self->popup_menu = gtk_popover_menu_new_from_model (model);
      gtk_widget_set_parent (self->popup_menu, GTK_WIDGET (self));
      gtk_popover_set_position (GTK_POPOVER (self->popup_menu), GTK_POS_BOTTOM);

      gtk_popover_set_has_arrow (GTK_POPOVER (self->popup_menu), FALSE);
      gtk_widget_set_halign (self->popup_menu, GTK_ALIGN_START);
    }

  if (x != -1 && y != -1)
    {
      GdkRectangle rect = { x, y, 1, 1 };
      gtk_popover_set_pointing_to (GTK_POPOVER (self->popup_menu), &rect);
    }
  else
    gtk_popover_set_pointing_to (GTK_POPOVER (self->popup_menu), NULL);

  gtk_popover_popup (GTK_POPOVER (self->popup_menu));
}

static void
click_released_cb (GtkGestureClick *gesture,
                   guint            n_press,
                   double           x,
                   double           y,
                   GtkWidget       *widget)
{
  GtkColumnViewTitle *self = GTK_COLUMN_VIEW_TITLE (widget);
  guint button;

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

  if (button == GDK_BUTTON_PRIMARY)
    activate_sort (self);
  else if (button == GDK_BUTTON_SECONDARY)
    show_menu (self, x, y);
}

static void
click_pressed_cb (GtkGestureClick *gesture,
                  int              n_press,
                  double           x,
                  double           y,
                  GtkColumnView   *self)
{
  /* Claim the state here to prevent propagation, the event controllers in
   * GtkColumView have already been handled in the CAPTURE phase */
  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
gtk_column_view_title_init (GtkColumnViewTitle *self)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkGesture *gesture;

  widget->priv->resize_func = gtk_column_view_title_resize_func;

  gtk_widget_set_overflow (widget, GTK_OVERFLOW_HIDDEN);

  self->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_parent (self->box, widget);

  self->title = gtk_label_new (NULL);
  gtk_box_append (GTK_BOX (self->box), self->title);

  self->sort = gtk_builtin_icon_new ("sort-indicator");
  gtk_box_append (GTK_BOX (self->box), self->sort);

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  g_signal_connect (gesture, "released", G_CALLBACK (click_released_cb), self);
  g_signal_connect (gesture, "pressed", G_CALLBACK (click_pressed_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (gesture));
}

GtkWidget *
gtk_column_view_title_new (GtkColumnViewColumn *column)
{
  GtkColumnViewTitle *title;

  title = g_object_new (GTK_TYPE_COLUMN_VIEW_TITLE, NULL);

  title->column = g_object_ref (column);
  gtk_column_view_title_update_sort (title);
  gtk_column_view_title_set_title (title, gtk_column_view_column_get_title (column));
  gtk_column_view_title_set_menu (title, gtk_column_view_column_get_header_menu (column));

  return GTK_WIDGET (title);
}

void
gtk_column_view_title_set_title (GtkColumnViewTitle *self,
                                 const char         *title)
{
  gtk_label_set_label (GTK_LABEL (self->title), title);
}

void
gtk_column_view_title_set_menu (GtkColumnViewTitle *self,
                                GMenuModel         *model)
{
  g_clear_pointer (&self->popup_menu, gtk_widget_unparent);
}

void
gtk_column_view_title_update_sort (GtkColumnViewTitle *self)
{
  if (gtk_column_view_column_get_sorter (self->column))
    {
      GtkColumnView *view;
      GtkColumnViewSorter *view_sorter;
      GtkColumnViewColumn *primary;
      GtkSortType sort_order;

      view = gtk_column_view_column_get_column_view (self->column);
      view_sorter = GTK_COLUMN_VIEW_SORTER (gtk_column_view_get_sorter (view));
      primary = gtk_column_view_sorter_get_primary_sort_column (view_sorter);
      sort_order = gtk_column_view_sorter_get_primary_sort_order (view_sorter);

      gtk_widget_set_visible (self->sort, TRUE);
      gtk_widget_remove_css_class (self->sort, "ascending");
      gtk_widget_remove_css_class (self->sort, "descending");
      gtk_widget_remove_css_class (self->sort, "unsorted");

      if (self->column != primary)
        gtk_widget_add_css_class (self->sort, "unsorted");
      else if (sort_order == GTK_SORT_DESCENDING)
        gtk_widget_add_css_class (self->sort, "descending");
      else
        gtk_widget_add_css_class (self->sort, "ascending");
    }
  else
    gtk_widget_set_visible (self->sort, FALSE);
}

GtkColumnViewColumn *
gtk_column_view_title_get_column (GtkColumnViewTitle *self)
{
  return self->column;
}
