/*
 * Copyright © 2018 Benjamin Otte
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

#include "gtklistviewprivate.h"

#include "gtkbitset.h"
#include "gtklistbaseprivate.h"
#include "gtklistheaderwidgetprivate.h"
#include "gtklistitemmanagerprivate.h"
#include "gtklistitemwidgetprivate.h"
#include "gtkmultiselection.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"

/* Maximum number of list items created by the listview.
 * For debugging, you can set this to G_MAXUINT to ensure
 * there's always a list item for every row.
 */
#define GTK_LIST_VIEW_MAX_LIST_ITEMS 200

/* Extra items to keep above + below every tracker */
#define GTK_LIST_VIEW_EXTRA_ITEMS 2

/**
 * GtkListView:
 *
 * `GtkListView` presents a large dynamic list of items.
 *
 * `GtkListView` uses its factory to generate one row widget for each visible
 * item and shows them in a linear display, either vertically or horizontally.
 *
 * The [property@Gtk.ListView:show-separators] property offers a simple way to
 * display separators between the rows.
 *
 * `GtkListView` allows the user to select items according to the selection
 * characteristics of the model. For models that allow multiple selected items,
 * it is possible to turn on _rubberband selection_, using
 * [property@Gtk.ListView:enable-rubberband].
 *
 * If you need multiple columns with headers, see [class@Gtk.ColumnView].
 *
 * To learn more about the list widget framework, see the
 * [overview](section-list-widget.html).
 *
 * An example of using `GtkListView`:
 * ```c
 * static void
 * setup_listitem_cb (GtkListItemFactory *factory,
 *                    GtkListItem        *list_item)
 * {
 *   GtkWidget *image;
 *
 *   image = gtk_image_new ();
 *   gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);
 *   gtk_list_item_set_child (list_item, image);
 * }
 *
 * static void
 * bind_listitem_cb (GtkListItemFactory *factory,
 *                   GtkListItem        *list_item)
 * {
 *   GtkWidget *image;
 *   GAppInfo *app_info;
 *
 *   image = gtk_list_item_get_child (list_item);
 *   app_info = gtk_list_item_get_item (list_item);
 *   gtk_image_set_from_gicon (GTK_IMAGE (image), g_app_info_get_icon (app_info));
 * }
 *
 * static void
 * activate_cb (GtkListView  *list,
 *              guint         position,
 *              gpointer      unused)
 * {
 *   GAppInfo *app_info;
 *
 *   app_info = g_list_model_get_item (G_LIST_MODEL (gtk_list_view_get_model (list)), position);
 *   g_app_info_launch (app_info, NULL, NULL, NULL);
 *   g_object_unref (app_info);
 * }
 *
 * ...
 *
 *   model = create_application_list ();
 *
 *   factory = gtk_signal_list_item_factory_new ();
 *   g_signal_connect (factory, "setup", G_CALLBACK (setup_listitem_cb), NULL);
 *   g_signal_connect (factory, "bind", G_CALLBACK (bind_listitem_cb), NULL);
 *
 *   list = gtk_list_view_new (GTK_SELECTION_MODEL (gtk_single_selection_new (model)), factory);
 *
 *   g_signal_connect (list, "activate", G_CALLBACK (activate_cb), NULL);
 *
 *   gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), list);
 * ```
 *
 * # CSS nodes
 *
 * ```
 * listview[.separators][.rich-list][.navigation-sidebar][.data-table]
 * ├── row[.activatable]
 * │
 * ├── row[.activatable]
 * │
 * ┊
 * ╰── [rubberband]
 * ```
 *
 * `GtkListView` uses a single CSS node named `listview`. It may carry the
 * `.separators` style class, when [property@Gtk.ListView:show-separators]
 * property is set. Each child widget uses a single CSS node named `row`.
 * If the [property@Gtk.ListItem:activatable] property is set, the
 * corresponding row will have the `.activatable` style class. For
 * rubberband selection, a node with name `rubberband` is used.
 *
 * The main listview node may also carry style classes to select
 * the style of [list presentation](ListContainers.html#list-styles):
 * .rich-list, .navigation-sidebar or .data-table.
 *
 * # Accessibility
 *
 * `GtkListView` uses the %GTK_ACCESSIBLE_ROLE_LIST role, and the list
 * items use the %GTK_ACCESSIBLE_ROLE_LIST_ITEM role.
 */

enum
{
  PROP_0,
  PROP_ENABLE_RUBBERBAND,
  PROP_FACTORY,
  PROP_HEADER_FACTORY,
  PROP_MODEL,
  PROP_SHOW_SEPARATORS,
  PROP_SINGLE_CLICK_ACTIVATE,
  PROP_TAB_BEHAVIOR,

  N_PROPS
};

enum {
  ACTIVATE,
  LAST_SIGNAL
};

G_DEFINE_TYPE (GtkListView, gtk_list_view, GTK_TYPE_LIST_BASE)

static GParamSpec *properties[N_PROPS] = { NULL, };
static guint signals[LAST_SIGNAL] = { 0 };

static GtkListTile *
gtk_list_view_split (GtkListBase *base,
                     GtkListTile *tile,
                     guint        n_items)
{
  GtkListView *self = GTK_LIST_VIEW (base);
  GtkListTile *new_tile;
  int spacing, row_height;

  gtk_list_base_get_border_spacing (GTK_LIST_BASE (self), NULL, &spacing);
  row_height = (tile->area.height - (tile->n_items - 1) * spacing) / tile->n_items;

  new_tile = gtk_list_tile_split (self->item_manager, tile, n_items);
  gtk_list_tile_set_area_size (self->item_manager,
                               tile,
                               tile->area.width,
                               row_height * tile->n_items + spacing * (tile->n_items - 1));
  gtk_list_tile_set_area (self->item_manager,
                          new_tile,
                          &(GdkRectangle) {
                            tile->area.x,
                            tile->area.y + tile->area.height + spacing,
                            tile->area.width,
                            row_height * new_tile->n_items + spacing * (new_tile->n_items - 1)
                          });

  return new_tile;
}

static void
gtk_list_view_prepare_section (GtkListBase *base,
                               GtkListTile *tile,
                               guint        position)
{
}

/* We define the listview as **inert** when the factory isn't used. */
static gboolean
gtk_list_view_is_inert (GtkListView *self)
{
  GtkWidget *widget = GTK_WIDGET (self);

  return !gtk_widget_get_visible (widget) ||
         gtk_widget_get_root (widget) == NULL;
}

static void
gtk_list_view_update_factories_with (GtkListView        *self,
                                     GtkListItemFactory *factory,
                                     GtkListItemFactory *header_factory)
{
  GtkListTile *tile;

  for (tile = gtk_list_item_manager_get_first (self->item_manager);
       tile != NULL;
       tile = gtk_rb_tree_node_get_next (tile))
    {
      switch (tile->type)
        {
        case GTK_LIST_TILE_ITEM:
          if (tile->widget)
            gtk_list_factory_widget_set_factory (GTK_LIST_FACTORY_WIDGET (tile->widget), factory);
          break;
        case GTK_LIST_TILE_HEADER:
          if (tile->widget)
            gtk_list_header_widget_set_factory (GTK_LIST_HEADER_WIDGET (tile->widget), header_factory);
          break;
        case GTK_LIST_TILE_UNMATCHED_HEADER:
        case GTK_LIST_TILE_FOOTER:
        case GTK_LIST_TILE_UNMATCHED_FOOTER:
        case GTK_LIST_TILE_REMOVED:
          g_assert (tile->widget == NULL);
          break;
        default:
          g_assert_not_reached();
          break;
        }
    }
}

static void
gtk_list_view_update_factories (GtkListView *self)
{
  gtk_list_view_update_factories_with (self,
                                       gtk_list_view_is_inert (self) ? NULL : self->factory,
                                       gtk_list_view_is_inert (self) ? NULL : self->header_factory);
}

static void
gtk_list_view_clear_factories (GtkListView *self)
{
  gtk_list_view_update_factories_with (self, NULL, NULL);
}

static GtkListItemBase *
gtk_list_view_create_list_widget (GtkListBase *base)
{
  GtkListView *self = GTK_LIST_VIEW (base);
  GtkListItemFactory *factory;
  GtkWidget *result;

  if (gtk_list_view_is_inert (self))
    factory = NULL;
  else
    factory = self->factory;

  result = gtk_list_item_widget_new (factory,
                                     "row",
                                     GTK_ACCESSIBLE_ROLE_LIST_ITEM);

  gtk_list_factory_widget_set_single_click_activate (GTK_LIST_FACTORY_WIDGET (result), self->single_click_activate);

  return GTK_LIST_ITEM_BASE (result);
}

static GtkListHeaderBase *
gtk_list_view_create_header_widget (GtkListBase *base)
{
  GtkListView *self = GTK_LIST_VIEW (base);
  GtkListItemFactory *factory;

  if (gtk_list_view_is_inert (self))
    factory = NULL;
  else
    factory = self->header_factory;

  return GTK_LIST_HEADER_BASE (gtk_list_header_widget_new (factory));
}

static gboolean
gtk_list_view_get_allocation (GtkListBase  *base,
                              guint         pos,
                              GdkRectangle *area)
{
  GtkListView *self = GTK_LIST_VIEW (base);
  GtkListTile *tile;
  guint offset;

  tile = gtk_list_item_manager_get_nth (self->item_manager, pos, &offset);
  if (tile == NULL)
    return FALSE;

  *area = tile->area;
  if (area->width || area->height)
    {
      if (tile->n_items)
        area->height /= tile->n_items;
      if (offset)
        area->y += offset * area->height;
    }
  else
    {
      /* item is not allocated yet */
      GtkListTile *other;
      int spacing;

      gtk_list_base_get_border_spacing (GTK_LIST_BASE (self), NULL, &spacing);

      for (other = gtk_rb_tree_node_get_previous (tile);
           other;
           other = gtk_rb_tree_node_get_previous (other))
        {
          if (other->area.width || other->area.height)
            {
              area->x = other->area.x;
              area->width = other->area.width;
              area->y = other->area.y + other->area.height + spacing;
              break;
            }
        }
      if (other == NULL)
        {
          for (other = gtk_rb_tree_node_get_next (tile);
               other;
               other = gtk_rb_tree_node_get_next (other))
            {
              if (other->area.width || other->area.height)
                {
                  area->x = other->area.x;
                  area->width = other->area.width;
                  area->y = MAX (0, other->area.y - spacing);
                  break;
                }
            }
        }
    }

  return TRUE;
}

static guint
gtk_list_view_move_focus_along (GtkListBase *base,
                                guint        pos,
                                int          steps)
{
  if (steps < 0)
    return pos - MIN (pos, -steps);
  else
    {
      pos += MIN (gtk_list_base_get_n_items (base) - pos - 1, steps);
    }

  return pos;
}

static gboolean
gtk_list_view_get_position_from_allocation (GtkListBase           *base,
                                            int                    x,
                                            int                    y,
                                            guint                 *pos,
                                            cairo_rectangle_int_t *area)
{
  GtkListView *self = GTK_LIST_VIEW (base);
  GtkListTile *tile;

  tile = gtk_list_item_manager_get_nearest_tile (self->item_manager, x, y);
  if (tile == NULL)
    return FALSE;

  while (tile && tile->n_items == 0)
    tile = gtk_rb_tree_node_get_previous (tile);
  if (tile == NULL)
    {
      tile = gtk_list_item_manager_get_first (self->item_manager);
      while (tile && tile->n_items == 0)
        tile = gtk_rb_tree_node_get_next (tile);
      if (tile == NULL)
        return FALSE;
    }

  *pos = gtk_list_tile_get_position (self->item_manager, tile);
  if (area)
    *area = tile->area;

  if (tile->n_items > 1)
    {
      int row_height, tile_pos, spacing;

      gtk_list_base_get_border_spacing (GTK_LIST_BASE (self), NULL, &spacing);
      row_height = (tile->area.height - (tile->n_items - 1) * spacing) / tile->n_items;
      if (y >= tile->area.y + tile->area.height)
        tile_pos = tile->n_items - 1;
      else
        tile_pos = (y - tile->area.y) / (row_height + spacing);

      *pos += tile_pos;
      if (area)
        {
          area->y = tile->area.y + tile_pos * (row_height + spacing);
          area->height = row_height;
        }
    }

  return TRUE;
}

static GtkBitset *
gtk_list_view_get_items_in_rect (GtkListBase                 *base,
                                 const cairo_rectangle_int_t *rect)
{
  guint first, last;
  cairo_rectangle_int_t area;
  GtkBitset *result;

  result = gtk_bitset_new_empty ();

  if (!gtk_list_view_get_position_from_allocation (base, rect->x, rect->y, &first, &area))
    return result;
  if (area.y + area.height < rect->y)
    first++;

  if (!gtk_list_view_get_position_from_allocation (base,
                                                   rect->x + rect->width - 1,
                                                   rect->y + rect->height - 1,
                                                   &last, &area))
    return result;
  if (area.y >= rect->y + rect->height)
    last--;

  if (last >= first)
    gtk_bitset_add_range_closed (result, first, last);

  return result;
}

static guint
gtk_list_view_move_focus_across (GtkListBase *base,
                                 guint        pos,
                                 int          steps)
{
  return pos;
}

static int
compare_ints (gconstpointer first,
               gconstpointer second)
{
  return *(int *) first - *(int *) second;
}

static guint
gtk_list_view_get_unknown_row_height (GtkListView *self,
                                      GArray      *heights)
{
  g_return_val_if_fail (heights->len > 0, 0);

  /* return the median and hope rows are generally uniform with few outliers */
  g_array_sort (heights, compare_ints);

  return g_array_index (heights, int, heights->len / 2);
}

static void
gtk_list_view_measure_across (GtkWidget      *widget,
                              GtkOrientation  orientation,
                              int             for_size,
                              int            *minimum,
                              int            *natural)
{
  GtkListView *self = GTK_LIST_VIEW (widget);
  GtkListTile *tile;
  int min, nat, child_min, child_nat;
  /* XXX: Figure out how to split a given height into per-row heights.
   * Good luck! */
  for_size = -1;

  min = 0;
  nat = 0;

  for (tile = gtk_list_item_manager_get_first (self->item_manager);
       tile != NULL;
       tile = gtk_rb_tree_node_get_next (tile))
    {
      /* ignore unavailable rows */
      if (tile->widget == NULL)
        continue;

      gtk_widget_measure (tile->widget,
                          orientation, for_size,
                          &child_min, &child_nat, NULL, NULL);
      min = MAX (min, child_min);
      nat = MAX (nat, child_nat);
    }

  *minimum = min;
  *natural = nat;
}

static void
gtk_list_view_measure_list (GtkWidget      *widget,
                            GtkOrientation  orientation,
                            int             for_size,
                            int            *minimum,
                            int            *natural)
{
  GtkListView *self = GTK_LIST_VIEW (widget);
  GtkListTile *tile;
  int min, nat, child_min, child_nat, spacing;
  GArray *min_heights, *nat_heights;
  guint n_unknown, n_items;

  n_items = gtk_list_base_get_n_items (GTK_LIST_BASE (self));
  if (n_items == 0)
    return;
  gtk_list_base_get_border_spacing (GTK_LIST_BASE (self), NULL, &spacing);

  min_heights = g_array_new (FALSE, FALSE, sizeof (int));
  nat_heights = g_array_new (FALSE, FALSE, sizeof (int));
  n_unknown = 0;
  min = 0;
  nat = 0;

  for (tile = gtk_list_item_manager_get_first (self->item_manager);
       tile != NULL;
       tile = gtk_rb_tree_node_get_next (tile))
    {
      if (tile->widget)
        {
          gtk_widget_measure (tile->widget,
                              orientation, for_size,
                              &child_min, &child_nat, NULL, NULL);
          if (tile->type == GTK_LIST_TILE_ITEM)
            {
              g_array_append_val (min_heights, child_min);
              g_array_append_val (nat_heights, child_nat);
            }
          min += child_min;
          nat += child_nat;
        }
      else
        {
          n_unknown += tile->n_items;
        }
    }

  if (n_unknown)
    {
      min += n_unknown * gtk_list_view_get_unknown_row_height (self, min_heights);
      nat += n_unknown * gtk_list_view_get_unknown_row_height (self, nat_heights);
    }
  g_array_free (min_heights, TRUE);
  g_array_free (nat_heights, TRUE);

  *minimum = min + spacing * (n_items - 1);
  *natural = nat + spacing * (n_items - 1);
}

static void
gtk_list_view_measure (GtkWidget      *widget,
                       GtkOrientation  orientation,
                       int             for_size,
                       int            *minimum,
                       int            *natural,
                       int            *minimum_baseline,
                       int            *natural_baseline)
{
  GtkListView *self = GTK_LIST_VIEW (widget);

  if (orientation == gtk_list_base_get_orientation (GTK_LIST_BASE (self)))
    gtk_list_view_measure_list (widget, orientation, for_size, minimum, natural);
  else
    gtk_list_view_measure_across (widget, orientation, for_size, minimum, natural);
}

static void
gtk_list_view_size_allocate (GtkWidget *widget,
                             int        width,
                             int        height,
                             int        baseline)
{
  GtkListView *self = GTK_LIST_VIEW (widget);
  GtkListTile *tile;
  GArray *heights;
  int min, nat, row_height, y, list_width, spacing;
  GtkOrientation orientation, opposite_orientation;
  GtkScrollablePolicy scroll_policy, opposite_scroll_policy;

  orientation = gtk_list_base_get_orientation (GTK_LIST_BASE (self));
  opposite_orientation = OPPOSITE_ORIENTATION (orientation);
  scroll_policy = gtk_list_base_get_scroll_policy (GTK_LIST_BASE (self), orientation);
  opposite_scroll_policy = gtk_list_base_get_scroll_policy (GTK_LIST_BASE (self), opposite_orientation);
  gtk_list_base_get_border_spacing (GTK_LIST_BASE (self), NULL, &spacing);

  gtk_list_item_manager_gc_tiles (self->item_manager);

  /* step 0: exit early if list is empty */
  tile = gtk_list_item_manager_get_first (self->item_manager);
  if (tile == NULL)
    {
      gtk_list_base_allocate (GTK_LIST_BASE (self));
      return;
    }

  /* step 1: determine width of the list */
  gtk_list_view_measure_across (widget, opposite_orientation,
                                -1,
                                &min, &nat);
  list_width = orientation == GTK_ORIENTATION_VERTICAL ? width : height;
  if (opposite_scroll_policy == GTK_SCROLL_MINIMUM)
    list_width = MAX (min, list_width);
  else
    list_width = MAX (nat, list_width);

  /* step 2: determine height of known list items and gc the list */
  heights = g_array_new (FALSE, FALSE, sizeof (int));

  for (;
       tile != NULL;
       tile = gtk_rb_tree_node_get_next (tile))
    {
      if (tile->widget == NULL)
        continue;

      gtk_widget_measure (tile->widget, orientation,
                          list_width,
                          &min, &nat, NULL, NULL);
      if (scroll_policy == GTK_SCROLL_MINIMUM)
        row_height = min;
      else
        row_height = nat;
      gtk_list_tile_set_area_size (self->item_manager, tile, list_width, row_height);
      if (tile->type == GTK_LIST_TILE_ITEM)
        g_array_append_val (heights, row_height);
    }

  /* step 3: determine height of unknown items and set the positions */
  row_height = gtk_list_view_get_unknown_row_height (self, heights);
  g_array_free (heights, TRUE);

  y = 0;
  for (tile = gtk_list_item_manager_get_first (self->item_manager);
       tile != NULL;
       tile = gtk_rb_tree_node_get_next (tile))
    {
      gtk_list_tile_set_area_position (self->item_manager, tile, 0, y);
      if (tile->widget == NULL)
        {
          gtk_list_tile_set_area_size (self->item_manager,
                                       tile,
                                       list_width,
                                       row_height * tile->n_items
                                       + spacing * (tile->n_items - 1));
        }

      y += tile->area.height + spacing;
    }

  /* step 4: allocate the rest */
  gtk_list_base_allocate (GTK_LIST_BASE (self));
}

static void
gtk_list_view_root (GtkWidget *widget)
{
  GtkListView *self = GTK_LIST_VIEW (widget);

  GTK_WIDGET_CLASS (gtk_list_view_parent_class)->root (widget);

  if (!gtk_list_view_is_inert (self))
    gtk_list_view_update_factories (self);
}

static void
gtk_list_view_unroot (GtkWidget *widget)
{
  GtkListView *self = GTK_LIST_VIEW (widget);

  if (!gtk_list_view_is_inert (self))
    gtk_list_view_clear_factories (self);

  GTK_WIDGET_CLASS (gtk_list_view_parent_class)->unroot (widget);
}

static void
gtk_list_view_show (GtkWidget *widget)
{
  GtkListView *self = GTK_LIST_VIEW (widget);

  GTK_WIDGET_CLASS (gtk_list_view_parent_class)->show (widget);

  if (!gtk_list_view_is_inert (self))
    gtk_list_view_update_factories (self);
}

static void
gtk_list_view_hide (GtkWidget *widget)
{
  GtkListView *self = GTK_LIST_VIEW (widget);

  if (!gtk_list_view_is_inert (self))
    gtk_list_view_clear_factories (self);

  GTK_WIDGET_CLASS (gtk_list_view_parent_class)->hide (widget);
}

static void
gtk_list_view_dispose (GObject *object)
{
  GtkListView *self = GTK_LIST_VIEW (object);

  self->item_manager = NULL;

  g_clear_object (&self->factory);
  g_clear_object (&self->header_factory);

  G_OBJECT_CLASS (gtk_list_view_parent_class)->dispose (object);
}

static void
gtk_list_view_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkListView *self = GTK_LIST_VIEW (object);

  switch (property_id)
    {
    case PROP_ENABLE_RUBBERBAND:
      g_value_set_boolean (value, gtk_list_base_get_enable_rubberband (GTK_LIST_BASE (self)));
      break;

    case PROP_FACTORY:
      g_value_set_object (value, self->factory);
      break;

    case PROP_HEADER_FACTORY:
      g_value_set_object (value, self->header_factory);
      break;

    case PROP_MODEL:
      g_value_set_object (value, gtk_list_base_get_model (GTK_LIST_BASE (self)));
      break;

    case PROP_SHOW_SEPARATORS:
      g_value_set_boolean (value, self->show_separators);
      break;

    case PROP_SINGLE_CLICK_ACTIVATE:
      g_value_set_boolean (value, self->single_click_activate);
      break;

    case PROP_TAB_BEHAVIOR:
      g_value_set_enum (value, gtk_list_base_get_tab_behavior (GTK_LIST_BASE (self)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_list_view_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkListView *self = GTK_LIST_VIEW (object);

  switch (property_id)
    {
    case PROP_ENABLE_RUBBERBAND:
      gtk_list_view_set_enable_rubberband (self, g_value_get_boolean (value));
      break;

    case PROP_FACTORY:
      gtk_list_view_set_factory (self, g_value_get_object (value));
      break;

    case PROP_HEADER_FACTORY:
      gtk_list_view_set_header_factory (self, g_value_get_object (value));
      break;

    case PROP_MODEL:
      gtk_list_view_set_model (self, g_value_get_object (value));
      break;

    case PROP_SHOW_SEPARATORS:
      gtk_list_view_set_show_separators (self, g_value_get_boolean (value));
      break;

    case PROP_SINGLE_CLICK_ACTIVATE:
      gtk_list_view_set_single_click_activate (self, g_value_get_boolean (value));
      break;

    case PROP_TAB_BEHAVIOR:
      gtk_list_view_set_tab_behavior (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_list_view_activate_item (GtkWidget  *widget,
                             const char *action_name,
                             GVariant   *parameter)
{
  GtkListView *self = GTK_LIST_VIEW (widget);
  guint pos;

  if (!g_variant_check_format_string (parameter, "u", FALSE))
    return;

  g_variant_get (parameter, "u", &pos);
  if (pos >= gtk_list_base_get_n_items (GTK_LIST_BASE (self)))
    return;

  g_signal_emit (widget, signals[ACTIVATE], 0, pos);
}

static void
gtk_list_view_class_init (GtkListViewClass *klass)
{
  GtkListBaseClass *list_base_class = GTK_LIST_BASE_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  list_base_class->split = gtk_list_view_split;
  list_base_class->create_list_widget = gtk_list_view_create_list_widget;
  list_base_class->prepare_section = gtk_list_view_prepare_section;
  list_base_class->create_header_widget = gtk_list_view_create_header_widget;
  list_base_class->get_allocation = gtk_list_view_get_allocation;
  list_base_class->get_items_in_rect = gtk_list_view_get_items_in_rect;
  list_base_class->get_position_from_allocation = gtk_list_view_get_position_from_allocation;
  list_base_class->move_focus_along = gtk_list_view_move_focus_along;
  list_base_class->move_focus_across = gtk_list_view_move_focus_across;

  widget_class->measure = gtk_list_view_measure;
  widget_class->size_allocate = gtk_list_view_size_allocate;
  widget_class->root = gtk_list_view_root;
  widget_class->unroot = gtk_list_view_unroot;
  widget_class->show = gtk_list_view_show;
  widget_class->hide = gtk_list_view_hide;

  gobject_class->dispose = gtk_list_view_dispose;
  gobject_class->get_property = gtk_list_view_get_property;
  gobject_class->set_property = gtk_list_view_set_property;

  /**
   * GtkListView:enable-rubberband: (attributes org.gtk.Property.get=gtk_list_view_get_enable_rubberband org.gtk.Property.set=gtk_list_view_set_enable_rubberband)
   *
   * Allow rubberband selection.
   */
  properties[PROP_ENABLE_RUBBERBAND] =
    g_param_spec_boolean ("enable-rubberband", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkListView:factory: (attributes org.gtk.Property.get=gtk_list_view_get_factory org.gtk.Property.set=gtk_list_view_set_factory)
   *
   * Factory for populating list items.
   */
  properties[PROP_FACTORY] =
    g_param_spec_object ("factory", NULL, NULL,
                         GTK_TYPE_LIST_ITEM_FACTORY,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkListView:header-factory: (attributes org.gtk.Property.get=gtk_list_view_get_header_factory org.gtk.Property.set=gtk_list_view_set_header_factory)
   *
   * Factory for creating header widgets.
   *
   * Since: 4.12
   */
  properties[PROP_HEADER_FACTORY] =
    g_param_spec_object ("header-factory", NULL, NULL,
                         GTK_TYPE_LIST_ITEM_FACTORY,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkListView:model: (attributes org.gtk.Property.get=gtk_list_view_get_model org.gtk.Property.set=gtk_list_view_set_model)
   *
   * Model for the items displayed.
   */
  properties[PROP_MODEL] =
    g_param_spec_object ("model", NULL, NULL,
                         GTK_TYPE_SELECTION_MODEL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkListView:show-separators: (attributes org.gtk.Property.get=gtk_list_view_get_show_separators org.gtk.Property.set=gtk_list_view_set_show_separators)
   *
   * Show separators between rows.
   */
  properties[PROP_SHOW_SEPARATORS] =
    g_param_spec_boolean ("show-separators", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkListView:single-click-activate: (attributes org.gtk.Property.get=gtk_list_view_get_single_click_activate org.gtk.Property.set=gtk_list_view_set_single_click_activate)
   *
   * Activate rows on single click and select them on hover.
   */
  properties[PROP_SINGLE_CLICK_ACTIVATE] =
    g_param_spec_boolean ("single-click-activate", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkListView:tab-behavior: (attributes org.gtk.Property.get=gtk_list_view_get_tab_behavior org.gtk.Property.set=gtk_list_view_set_tab_behavior)
   *
   * Behavior of the <kbd>Tab</kbd> key
   *
   * Since: 4.12
   */
  properties[PROP_TAB_BEHAVIOR] =
    g_param_spec_enum ("tab-behavior", NULL, NULL,
                       GTK_TYPE_LIST_TAB_BEHAVIOR,
                       GTK_LIST_TAB_ALL,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);

  /**
   * GtkListView::activate:
   * @self: The `GtkListView`
   * @position: position of item to activate
   *
   * Emitted when a row has been activated by the user,
   * usually via activating the GtkListView|list.activate-item action.
   *
   * This allows for a convenient way to handle activation in a listview.
   * See [method@Gtk.ListItem.set_activatable] for details on how to use
   * this signal.
   */
  signals[ACTIVATE] =
    g_signal_new (I_("activate"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__UINT,
                  G_TYPE_NONE, 1,
                  G_TYPE_UINT);
  g_signal_set_va_marshaller (signals[ACTIVATE],
                              G_TYPE_FROM_CLASS (gobject_class),
                              g_cclosure_marshal_VOID__UINTv);

  /**
   * GtkListView|list.activate-item:
   * @position: position of item to activate
   *
   * Activates the item given in @position by emitting the
   * [signal@Gtk.ListView::activate] signal.
   */
  gtk_widget_class_install_action (widget_class,
                                   "list.activate-item",
                                   "u",
                                   gtk_list_view_activate_item);

  gtk_widget_class_set_css_name (widget_class, I_("listview"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_LIST);
}

static void
gtk_list_view_init (GtkListView *self)
{
  self->item_manager = gtk_list_base_get_manager (GTK_LIST_BASE (self));

  gtk_list_base_set_anchor_max_widgets (GTK_LIST_BASE (self),
                                        GTK_LIST_VIEW_MAX_LIST_ITEMS,
                                        GTK_LIST_VIEW_EXTRA_ITEMS);

  gtk_widget_add_css_class (GTK_WIDGET (self), "view");
}

/**
 * gtk_list_view_new:
 * @model: (nullable) (transfer full): the model to use
 * @factory: (nullable) (transfer full): The factory to populate items with
 *
 * Creates a new `GtkListView` that uses the given @factory for
 * mapping items to widgets.
 *
 * The function takes ownership of the
 * arguments, so you can write code like
 * ```c
 * list_view = gtk_list_view_new (create_model (),
 *   gtk_builder_list_item_factory_new_from_resource ("/resource.ui"));
 * ```
 *
 * Returns: a new `GtkListView` using the given @model and @factory
 */
GtkWidget *
gtk_list_view_new (GtkSelectionModel  *model,
                   GtkListItemFactory *factory)
{
  GtkWidget *result;

  g_return_val_if_fail (model == NULL || GTK_IS_SELECTION_MODEL (model), NULL);
  g_return_val_if_fail (factory == NULL || GTK_IS_LIST_ITEM_FACTORY (factory), NULL);

  result = g_object_new (GTK_TYPE_LIST_VIEW,
                         "model", model,
                         "factory", factory,
                         NULL);

  /* consume the references */
  g_clear_object (&model);
  g_clear_object (&factory);

  return result;
}

/**
 * gtk_list_view_get_model: (attributes org.gtk.Method.get_property=model)
 * @self: a `GtkListView`
 *
 * Gets the model that's currently used to read the items displayed.
 *
 * Returns: (nullable) (transfer none): The model in use
 */
GtkSelectionModel *
gtk_list_view_get_model (GtkListView *self)
{
  g_return_val_if_fail (GTK_IS_LIST_VIEW (self), NULL);

  return gtk_list_base_get_model (GTK_LIST_BASE (self));
}

/**
 * gtk_list_view_set_model: (attributes org.gtk.Method.set_property=model)
 * @self: a `GtkListView`
 * @model: (nullable) (transfer none): the model to use
 *
 * Sets the model to use.
 *
 * This must be a [iface@Gtk.SelectionModel] to use.
 */
void
gtk_list_view_set_model (GtkListView       *self,
                         GtkSelectionModel *model)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (self));
  g_return_if_fail (model == NULL || GTK_IS_SELECTION_MODEL (model));

  if (!gtk_list_base_set_model (GTK_LIST_BASE (self), model))
    return;

  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_MULTI_SELECTABLE, GTK_IS_MULTI_SELECTION (model),
                                  -1);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

/**
 * gtk_list_view_get_factory: (attributes org.gtk.Method.get_property=factory)
 * @self: a `GtkListView`
 *
 * Gets the factory that's currently used to populate list items.
 *
 * Returns: (nullable) (transfer none): The factory in use
 */
GtkListItemFactory *
gtk_list_view_get_factory (GtkListView *self)
{
  g_return_val_if_fail (GTK_IS_LIST_VIEW (self), NULL);

  return self->factory;
}

/**
 * gtk_list_view_set_factory: (attributes org.gtk.Method.set_property=factory)
 * @self: a `GtkListView`
 * @factory: (nullable) (transfer none): the factory to use
 *
 * Sets the `GtkListItemFactory` to use for populating list items.
 */
void
gtk_list_view_set_factory (GtkListView        *self,
                           GtkListItemFactory *factory)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (self));
  g_return_if_fail (factory == NULL || GTK_IS_LIST_ITEM_FACTORY (factory));

  if (!g_set_object (&self->factory, factory))
    return;

  gtk_list_view_update_factories (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FACTORY]);
}

/**
 * gtk_list_view_get_header_factory: (attributes org.gtk.Method.get_property=header-factory)
 * @self: a `GtkListView`
 *
 * Gets the factory that's currently used to populate section headers.
 *
 * Returns: (nullable) (transfer none): The factory in use
 *
 * Since: 4.12
 */
GtkListItemFactory *
gtk_list_view_get_header_factory (GtkListView *self)
{
  g_return_val_if_fail (GTK_IS_LIST_VIEW (self), NULL);

  return self->header_factory;
}

/**
 * gtk_list_view_set_header_factory: (attributes org.gtk.Method.set_property=header-factory)
 * @self: a `GtkListView`
 * @factory: (nullable) (transfer none): the factory to use
 *
 * Sets the `GtkListItemFactory` to use for populating the
 * [class@Gtk.ListHeader] objects used in section headers.
 *
 * If this factory is set to %NULL, the list will not show section headers.
 *
 * Since: 4.12
 */
void
gtk_list_view_set_header_factory (GtkListView        *self,
                                  GtkListItemFactory *factory)
{
  gboolean had_sections;

  g_return_if_fail (GTK_IS_LIST_VIEW (self));
  g_return_if_fail (factory == NULL || GTK_IS_LIST_ITEM_FACTORY (factory));

  had_sections = gtk_list_item_manager_get_has_sections (self->item_manager);

  if (!g_set_object (&self->header_factory, factory))
    return;

  gtk_list_item_manager_set_has_sections (self->item_manager, factory != NULL);

  if (!gtk_list_view_is_inert (self) &&
      had_sections && gtk_list_item_manager_get_has_sections (self->item_manager))
    {
      GtkListTile *tile;

      for (tile = gtk_list_item_manager_get_first (self->item_manager);
           tile != NULL;
           tile = gtk_rb_tree_node_get_next (tile))
        {
          if (tile->widget && tile->type == GTK_LIST_TILE_HEADER)
            gtk_list_header_widget_set_factory (GTK_LIST_HEADER_WIDGET (tile->widget), factory);
        }
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HEADER_FACTORY]);
}

/**
 * gtk_list_view_set_show_separators: (attributes org.gtk.Method.set_property=show-separators)
 * @self: a `GtkListView`
 * @show_separators: %TRUE to show separators
 *
 * Sets whether the list box should show separators
 * between rows.
 */
void
gtk_list_view_set_show_separators (GtkListView *self,
                                   gboolean     show_separators)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (self));

  if (self->show_separators == show_separators)
    return;

  self->show_separators = show_separators;

  if (show_separators)
    gtk_widget_add_css_class (GTK_WIDGET (self), "separators");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), "separators");

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_SEPARATORS]);
}

/**
 * gtk_list_view_get_show_separators: (attributes org.gtk.Method.get_property=show-separators)
 * @self: a `GtkListView`
 *
 * Returns whether the list box should show separators
 * between rows.
 *
 * Returns: %TRUE if the list box shows separators
 */
gboolean
gtk_list_view_get_show_separators (GtkListView *self)
{
  g_return_val_if_fail (GTK_IS_LIST_VIEW (self), FALSE);

  return self->show_separators;
}

/**
 * gtk_list_view_set_single_click_activate: (attributes org.gtk.Method.set_property=single-click-activate)
 * @self: a `GtkListView`
 * @single_click_activate: %TRUE to activate items on single click
 *
 * Sets whether rows should be activated on single click and
 * selected on hover.
 */
void
gtk_list_view_set_single_click_activate (GtkListView *self,
                                         gboolean     single_click_activate)
{
  GtkListTile *tile;

  g_return_if_fail (GTK_IS_LIST_VIEW (self));

  if (single_click_activate == self->single_click_activate)
    return;

  self->single_click_activate = single_click_activate;

  for (tile = gtk_list_item_manager_get_first (self->item_manager);
       tile != NULL;
       tile = gtk_rb_tree_node_get_next (tile))
    {
      if (tile->widget && tile->type == GTK_LIST_TILE_ITEM)
        gtk_list_factory_widget_set_single_click_activate (GTK_LIST_FACTORY_WIDGET (tile->widget), single_click_activate);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SINGLE_CLICK_ACTIVATE]);
}

/**
 * gtk_list_view_get_single_click_activate: (attributes org.gtk.Method.set_property=single-click-activate)
 * @self: a `GtkListView`
 *
 * Returns whether rows will be activated on single click and
 * selected on hover.
 *
 * Returns: %TRUE if rows are activated on single click
 */
gboolean
gtk_list_view_get_single_click_activate (GtkListView *self)
{
  g_return_val_if_fail (GTK_IS_LIST_VIEW (self), FALSE);

  return self->single_click_activate;
}

/**
 * gtk_list_view_set_enable_rubberband: (attributes org.gtk.Method.set_property=enable-rubberband)
 * @self: a `GtkListView`
 * @enable_rubberband: %TRUE to enable rubberband selection
 *
 * Sets whether selections can be changed by dragging with the mouse.
 */
void
gtk_list_view_set_enable_rubberband (GtkListView *self,
                                     gboolean     enable_rubberband)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (self));

  if (enable_rubberband == gtk_list_base_get_enable_rubberband (GTK_LIST_BASE (self)))
    return;

  gtk_list_base_set_enable_rubberband (GTK_LIST_BASE (self), enable_rubberband);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ENABLE_RUBBERBAND]);
}

/**
 * gtk_list_view_get_enable_rubberband: (attributes org.gtk.Method.get_property=enable-rubberband)
 * @self: a `GtkListView`
 *
 * Returns whether rows can be selected by dragging with the mouse.
 *
 * Returns: %TRUE if rubberband selection is enabled
 */
gboolean
gtk_list_view_get_enable_rubberband (GtkListView *self)
{
  g_return_val_if_fail (GTK_IS_LIST_VIEW (self), FALSE);

  return gtk_list_base_get_enable_rubberband (GTK_LIST_BASE (self));
}

/**
 * gtk_list_view_set_tab_behavior: (attributes org.gtk.Method.set_property=tab-behavior)
 * @self: a `GtkListView`
 * @tab_behavior: The desired tab behavior
 *
 * Sets the behavior of the <kbd>Tab</kbd> and <kbd>Shift</kbd>+<kbd>Tab</kbd> keys.
 *
 * Since: 4.12
 */
void
gtk_list_view_set_tab_behavior (GtkListView        *self,
                                GtkListTabBehavior  tab_behavior)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (self));

  if (tab_behavior == gtk_list_base_get_tab_behavior (GTK_LIST_BASE (self)))
    return;

  gtk_list_base_set_tab_behavior (GTK_LIST_BASE (self), tab_behavior);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TAB_BEHAVIOR]);
}

/**
 * gtk_list_view_get_tab_behavior: (attributes org.gtk.Method.get_property=tab-behavior)
 * @self: a `GtkListView`
 *
 * Gets the behavior set for the <kbd>Tab</kbd> key.
 *
 * Returns: The behavior of the <kbd>Tab</kbd> key
 *
 * Since: 4.12
 */
GtkListTabBehavior
gtk_list_view_get_tab_behavior (GtkListView *self)
{
  g_return_val_if_fail (GTK_IS_LIST_VIEW (self), FALSE);

  return gtk_list_base_get_tab_behavior (GTK_LIST_BASE (self));
}

/**
 * gtk_list_view_scroll_to:
 * @self: The listview to scroll in
 * @pos: position of the item. Must be less than the number of
 *   items in the view.
 * @flags: actions to perform
 * @scroll: (nullable) (transfer full): details of how to perform
 *   the scroll operation or %NULL to scroll into view 
 *
 * Scrolls to the item at the given position and performs the actions
 * specified in @flags.
 *
 * This function works no matter if the listview is shown or focused.
 * If it isn't, then the changes will take effect once that happens.
 *
 * Since: 4.12
 */
void
gtk_list_view_scroll_to (GtkListView        *self,
                         guint               pos,
                         GtkListScrollFlags  flags,
                         GtkScrollInfo      *scroll)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (self));
  g_return_if_fail (pos < gtk_list_base_get_n_items (GTK_LIST_BASE (self)));

  gtk_list_base_scroll_to (GTK_LIST_BASE (self), pos, flags, scroll);
}

