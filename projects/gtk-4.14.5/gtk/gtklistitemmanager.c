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

#include "gtklistitemmanagerprivate.h"

#include "gtklistitembaseprivate.h"
#include "gtklistitemwidgetprivate.h"
#include "gtksectionmodel.h"
#include "gtkwidgetprivate.h"

typedef struct _GtkListItemChange GtkListItemChange;

struct _GtkListItemManager
{
  GObject parent_instance;

  GtkWidget *widget;
  GtkSelectionModel *model;
  gboolean has_sections;

  GtkRbTree *items;
  GSList *trackers;

  GtkListTile * (* split_func) (GtkWidget *, GtkListTile *, guint);
  GtkListItemBase * (* create_widget) (GtkWidget *);
  void (* prepare_section) (GtkWidget *, GtkListTile *, guint);
  GtkListHeaderBase * (* create_header_widget) (GtkWidget *);
};

struct _GtkListItemManagerClass
{
  GObjectClass parent_class;
};

struct _GtkListItemTracker
{
  guint position;
  GtkListItemBase *widget;
  guint n_before;
  guint n_after;
};

struct _GtkListItemChange
{
  GHashTable *deleted_items;
  GQueue recycled_items;
  GQueue recycled_headers;
};

G_DEFINE_TYPE (GtkListItemManager, gtk_list_item_manager, G_TYPE_OBJECT)

static void
gtk_list_item_change_init (GtkListItemChange *change)
{
  change->deleted_items = NULL;
  g_queue_init (&change->recycled_items);
  g_queue_init (&change->recycled_headers);
}

static void
gtk_list_item_change_finish (GtkListItemChange *change)
{
  GtkWidget *widget;

  g_clear_pointer (&change->deleted_items, g_hash_table_destroy);

  while ((widget = g_queue_pop_head (&change->recycled_items)))
    gtk_widget_unparent (widget);
  while ((widget = g_queue_pop_head (&change->recycled_headers)))
    gtk_widget_unparent (widget);
}

static void
gtk_list_item_change_recycle (GtkListItemChange *change,
                              GtkListItemBase   *widget)
{
  g_queue_push_tail (&change->recycled_items, widget);
}

static void
gtk_list_item_change_clear_header (GtkListItemChange  *change,
                                   GtkWidget         **widget)
{
  if (*widget == NULL)
    return;

  g_assert (GTK_IS_LIST_HEADER_BASE (*widget));
  g_queue_push_tail (&change->recycled_headers, *widget);
  *widget = NULL;
}

static void
gtk_list_item_change_release (GtkListItemChange *change,
                              GtkListItemBase   *widget)
{
  if (change->deleted_items == NULL)
    change->deleted_items = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) gtk_widget_unparent);

  if (!g_hash_table_replace (change->deleted_items, gtk_list_item_base_get_item (GTK_LIST_ITEM_BASE (widget)), widget))
    {
      g_warning ("Duplicate item detected in list. Picking one randomly.");
    }
}

static GtkListItemBase *
gtk_list_item_change_find (GtkListItemChange *change,
                           gpointer           item)
{
  gpointer result;

  if (change->deleted_items && g_hash_table_steal_extended (change->deleted_items, item, NULL, &result))
    return result;

  return NULL;
}

static GtkListItemBase *
gtk_list_item_change_get (GtkListItemChange *change,
                          gpointer           item)
{
  GtkListItemBase *result;

  result = gtk_list_item_change_find (change, item);
  if (result)
    return result;

  result = g_queue_pop_head (&change->recycled_items);
  if (result)
    return result;

  return NULL;
}

static GtkListHeaderBase *
gtk_list_item_change_get_header (GtkListItemChange *change)
{
  return g_queue_pop_head (&change->recycled_headers);
}

static void
potentially_empty_rectangle_union (cairo_rectangle_int_t       *self,
                                   const cairo_rectangle_int_t *area)
{
  if (area->width <= 0 || area->height <= 0)
    return;

  if (self->width <= 0 || self->height <= 0)
    {
      *self = *area;
      return;
    }

  gdk_rectangle_union (self, area, self);
}

static void
gtk_list_item_manager_augment_node (GtkRbTree *tree,
                                    gpointer   node_augment,
                                    gpointer   node,
                                    gpointer   left,
                                    gpointer   right)
{
  GtkListTile *tile = node;
  GtkListTileAugment *aug = node_augment;

  aug->n_items = tile->n_items;
  aug->area = tile->area;

  switch (tile->type)
  {
    case GTK_LIST_TILE_HEADER:
    case GTK_LIST_TILE_UNMATCHED_HEADER:
      aug->has_header = TRUE;
      aug->has_footer = FALSE;
      break;
    case GTK_LIST_TILE_FOOTER:
    case GTK_LIST_TILE_UNMATCHED_FOOTER:
      aug->has_header = FALSE;
      aug->has_footer = TRUE;
      break;
    case GTK_LIST_TILE_ITEM:
    case GTK_LIST_TILE_REMOVED:
      aug->has_header = FALSE;
      aug->has_footer = FALSE;
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  if (left)
    {
      GtkListTileAugment *left_aug = gtk_rb_tree_get_augment (tree, left);

      aug->n_items += left_aug->n_items;
      aug->has_header |= left_aug->has_header;
      aug->has_footer |= left_aug->has_footer;
      potentially_empty_rectangle_union (&aug->area, &left_aug->area);
    }

  if (right)
    {
      GtkListTileAugment *right_aug = gtk_rb_tree_get_augment (tree, right);

      aug->n_items += right_aug->n_items;
      aug->has_header |= right_aug->has_header;
      aug->has_footer |= right_aug->has_footer;
      potentially_empty_rectangle_union (&aug->area, &right_aug->area);
    }
}

static void
gtk_list_item_manager_clear_node (gpointer _tile)
{
  GtkListTile *tile G_GNUC_UNUSED = _tile;

  g_assert (tile->widget == NULL);
}

GtkListItemManager *
gtk_list_item_manager_new (GtkWidget          *widget,
                           GtkListTile *       (* split_func) (GtkWidget *, GtkListTile *, guint),
                           GtkListItemBase *   (* create_widget) (GtkWidget *),
                           void                (* prepare_section) (GtkWidget *, GtkListTile *, guint),
                           GtkListHeaderBase * (* create_header_widget) (GtkWidget *))
{
  GtkListItemManager *self;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  self = g_object_new (GTK_TYPE_LIST_ITEM_MANAGER, NULL);

  /* not taking a ref because the widget refs us */
  self->widget = widget;
  self->split_func = split_func;
  self->create_widget = create_widget;
  self->prepare_section = prepare_section;
  self->create_header_widget = create_header_widget;

  self->items = gtk_rb_tree_new_for_size (sizeof (GtkListTile),
                                          sizeof (GtkListTileAugment),
                                          gtk_list_item_manager_augment_node,
                                          gtk_list_item_manager_clear_node,
                                          NULL);

  return self;
}

static gboolean
gtk_list_item_manager_has_sections (GtkListItemManager *self)
{
  if (self->model == NULL || !self->has_sections)
    return FALSE;

  return GTK_IS_SECTION_MODEL (self->model);
}

void
gtk_list_item_manager_get_tile_bounds (GtkListItemManager *self,
                                       GdkRectangle       *out_bounds)
{
  GtkListTile *tile;
  GtkListTileAugment *aug;

  tile = gtk_rb_tree_get_root (self->items);
  if (tile == NULL)
    {
      *out_bounds = (GdkRectangle) { 0, 0, 0, 0 };
      return;
    }

  aug = gtk_rb_tree_get_augment (self->items, tile);
  *out_bounds = aug->area;
}

gpointer
gtk_list_item_manager_get_first (GtkListItemManager *self)
{
  return gtk_rb_tree_get_first (self->items);
}

gpointer
gtk_list_item_manager_get_last (GtkListItemManager *self)
{
  return gtk_rb_tree_get_last (self->items);
}

gpointer
gtk_list_item_manager_get_root (GtkListItemManager *self)
{
  return gtk_rb_tree_get_root (self->items);
}

/*
 * gtk_list_item_manager_get_nth:
 * @self: a `GtkListItemManager`
 * @position: position of the item
 * @offset: (out): offset into the returned tile
 *
 * Looks up the GtkListTile that represents @position.
 *
 * If a the returned tile represents multiple rows, the @offset into
 * the returned tile for @position will be set. If the returned tile
 * represents a row with an existing widget, @offset will always be 0.
 *
 * Returns: (type GtkListTile): the tile for @position or
 *   %NULL if position is out of range
 **/
gpointer
gtk_list_item_manager_get_nth (GtkListItemManager *self,
                               guint               position,
                               guint              *offset)
{
  GtkListTile *tile, *tmp;

  tile = gtk_rb_tree_get_root (self->items);

  while (tile)
    {
      tmp = gtk_rb_tree_node_get_left (tile);
      if (tmp)
        {
          GtkListTileAugment *aug = gtk_rb_tree_get_augment (self->items, tmp);
          if (position < aug->n_items)
            {
              tile = tmp;
              continue;
            }
          position -= aug->n_items;
        }

      if (position < tile->n_items)
        break;
      position -= tile->n_items;

      tile = gtk_rb_tree_node_get_right (tile);
    }

  if (offset)
    *offset = tile ? position : 0;

  return tile;
}

static GtkListTile *
gtk_list_tile_get_header (GtkListItemManager *self,
                          GtkListTile        *tile)
{
  GtkListTileAugment *aug;
  GtkListTile *other;
  gboolean check_right = FALSE;

  while (TRUE)
    {
      if (check_right)
        {
          other = gtk_rb_tree_node_get_right (tile);
          if (other)
            {
              aug = gtk_rb_tree_get_augment (self->items, other);
              if (aug->has_header)
                {
                  check_right = TRUE;
                  tile = other;
                  continue;
                }
            }
        }

      if (tile->type == GTK_LIST_TILE_HEADER ||
          tile->type == GTK_LIST_TILE_UNMATCHED_HEADER)
        return tile;

      other = gtk_rb_tree_node_get_left (tile);
      if (other)
        {
          aug = gtk_rb_tree_get_augment (self->items, other);
          if (aug->has_header)
            {
              check_right = TRUE;
              tile = other;
              continue;
            }
        }

      while ((other = gtk_rb_tree_node_get_parent (tile)))
        {
          if (gtk_rb_tree_node_get_right (other) == tile)
            break;
          tile = other;
        }
      tile = other;
      check_right = FALSE;
    }
}

static GtkListTile *
gtk_list_tile_get_footer (GtkListItemManager *self,
                          GtkListTile        *tile)
{
  GtkListTileAugment *aug;
  GtkListTile *other;
  gboolean check_left = FALSE;

  while (TRUE)
    {
      if (check_left)
        {
          other = gtk_rb_tree_node_get_left (tile);
          if (other)
            {
              aug = gtk_rb_tree_get_augment (self->items, other);
              if (aug->has_footer)
                {
                  check_left = TRUE;
                  tile = other;
                  continue;
                }
            }
        }

      if (tile->type == GTK_LIST_TILE_FOOTER ||
          tile->type == GTK_LIST_TILE_UNMATCHED_FOOTER)
        return tile;

      other = gtk_rb_tree_node_get_right (tile);
      if (other)
        {
          aug = gtk_rb_tree_get_augment (self->items, other);
          if (aug->has_footer)
            {
              check_left = TRUE;
              tile = other;
              continue;
            }
        }

      while ((other = gtk_rb_tree_node_get_parent (tile)))
        {
          if (gtk_rb_tree_node_get_left (other) == tile)
            break;
          tile = other;
        }
      tile = other;
      check_left = FALSE;
    }
}

/* This computes Manhattan distance */
static int
rectangle_distance (const cairo_rectangle_int_t *rect,
                    int                          x,
                    int                          y)
{
  int x_dist, y_dist;

  if (rect->x > x)
    x_dist = rect->x - x;
  else if (rect->x + rect->width < x)
    x_dist = x - (rect->x + rect->width);
  else
    x_dist = 0;

  if (rect->y > y)
    y_dist = rect->y - y;
  else if (rect->y + rect->height < y)
    y_dist = y - (rect->y + rect->height);
  else
    y_dist = 0;

  return x_dist + y_dist;
}

static GtkListTile *
gtk_list_tile_get_tile_at (GtkListItemManager *self,
                           GtkListTile        *tile,
                           int                 x,
                           int                 y,
                           int                *distance)
{
  GtkListTileAugment *aug;
  GtkListTile *left, *right, *result;
  int dist, left_dist, right_dist;

  left = gtk_rb_tree_node_get_left (tile);
  if (left)
    {
      aug = gtk_list_tile_get_augment (self, left);
      left_dist = rectangle_distance (&aug->area, x, y);
    }
  else
    left_dist = *distance;
  right = gtk_rb_tree_node_get_right (tile);
  if (right)
    {
      aug = gtk_list_tile_get_augment (self, right);
      right_dist = rectangle_distance (&aug->area, x, y);
    }
  else
    right_dist = *distance;

  dist = rectangle_distance (&tile->area, x, y);
  result = NULL;

  while (TRUE)
    {
      if (dist < left_dist && dist < right_dist)
        {
          if (dist >= *distance)
            return result;

          *distance = dist;
          return tile;
        }

      if (left_dist < right_dist)
        {
          if (left_dist >= *distance)
            return result;

          left = gtk_list_tile_get_tile_at (self, left, x, y, distance);
          if (left)
            result = left;
          left_dist = G_MAXINT;
        }
      else
        {
          if (right_dist >= *distance)
            return result;

          right = gtk_list_tile_get_tile_at (self, right, x, y, distance);
          if (right)
            result = right;
          right_dist = G_MAXINT;
        }
    }
}

/*
 * gtk_list_item_manager_get_nearest_tile:
 * @self: a GtkListItemManager
 * @x: x coordinate of tile
 * @y: y coordinate of tile
 *
 * Finds the tile closest to the coordinates at (x, y). If no
 * tile occupies the coordinates (for example, if the tile is out of bounds),
 * Manhattan distance is used to find the nearest tile.
 *
 * If multiple tiles have the same distance, the one closest to the start
 * will be returned.
 *
 * Returns: (nullable): The tile nearest to (x, y) or NULL if there are no tiles
 **/
GtkListTile *
gtk_list_item_manager_get_nearest_tile (GtkListItemManager *self,
                                        int                 x,
                                        int                 y)
{
  GtkListTile *root;
  int distance = G_MAXINT;

  root = gtk_list_item_manager_get_root (self);
  if (root == NULL)
    return NULL;

  return gtk_list_tile_get_tile_at (self, root, x, y, &distance);
}

guint
gtk_list_tile_get_position (GtkListItemManager *self,
                            GtkListTile        *tile)
{
  GtkListTile *parent, *left;
  int pos;

  left = gtk_rb_tree_node_get_left (tile);
  if (left)
    {
      GtkListTileAugment *aug = gtk_rb_tree_get_augment (self->items, left);
      pos = aug->n_items;
    }
  else
    {
      pos = 0;
    }

  for (parent = gtk_rb_tree_node_get_parent (tile);
       parent != NULL;
       parent = gtk_rb_tree_node_get_parent (tile))
    {
      left = gtk_rb_tree_node_get_left (parent);

      if (left != tile)
        {
          if (left)
            {
              GtkListTileAugment *aug = gtk_rb_tree_get_augment (self->items, left);
              pos += aug->n_items;
            }
          pos += parent->n_items;
        }

      tile = parent;
    }

  return pos;
}

gpointer
gtk_list_tile_get_augment (GtkListItemManager *self,
                           GtkListTile        *tile)
{
  return gtk_rb_tree_get_augment (self->items, tile);
}

static GtkListTile *
gtk_list_tile_get_next_skip (GtkListTile *tile)
{
  for (tile = gtk_rb_tree_node_get_next (tile);
       tile && tile->type == GTK_LIST_TILE_REMOVED;
       tile = gtk_rb_tree_node_get_next (tile))
    { }

  return tile;
}

static GtkListTile *
gtk_list_tile_get_previous_skip (GtkListTile *tile)
{
  for (tile = gtk_rb_tree_node_get_previous (tile);
       tile && tile->type == GTK_LIST_TILE_REMOVED;
       tile = gtk_rb_tree_node_get_previous (tile))
    { }

  return tile;
}

/*
 * gtk_list_tile_set_area:
 * @self: the list item manager
 * @tile: tile to set area for
 * @area: (nullable): area to set or NULL to clear
 *     the area
 *
 * Updates the area of the tile.
 *
 * The area is given in the internal coordinate system,
 * so the x/y flip due to orientation and the left/right
 * flip for RTL languages will happen later.
 *
 * This function should only be called from inside size_allocate().
 **/
void
gtk_list_tile_set_area (GtkListItemManager          *self,
                        GtkListTile                 *tile,
                        const cairo_rectangle_int_t *area)
{
  cairo_rectangle_int_t empty_area = { 0, 0, 0, 0 };

  if (!area)
    area = &empty_area;

  if (gdk_rectangle_equal (&tile->area, area))
    return;

  tile->area = *area;
  gtk_rb_tree_node_mark_dirty (tile);
}

void
gtk_list_tile_set_area_position (GtkListItemManager *self,
                                 GtkListTile        *tile,
                                 int                 x,
                                 int                 y)
{
  if (tile->area.x == x && tile->area.y == y)
    return;

  tile->area.x = x;
  tile->area.y = y;
  gtk_rb_tree_node_mark_dirty (tile);
}

void
gtk_list_tile_set_area_size (GtkListItemManager *self,
                             GtkListTile        *tile,
                             int                 width,
                             int                 height)
{
  if (tile->area.width == width && tile->area.height == height)
    return;

  tile->area.width = width;
  tile->area.height = height;
  gtk_rb_tree_node_mark_dirty (tile);
}

static void
gtk_list_tile_set_type (GtkListTile     *tile,
                        GtkListTileType  type)
{
  g_assert (tile != NULL);
  if (tile->type == type)
    return;

  g_assert (tile->widget == NULL);
  tile->type = type;
  gtk_rb_tree_node_mark_dirty (tile);
}

static void
gtk_list_item_tracker_unset_position (GtkListItemManager *self,
                                      GtkListItemTracker *tracker)
{
  tracker->widget = NULL;
  tracker->position = GTK_INVALID_LIST_POSITION;
}

static gboolean
gtk_list_item_tracker_query_range (GtkListItemManager *self,
                                   GtkListItemTracker *tracker,
                                   guint               n_items,
                                   guint              *out_start,
                                   guint              *out_n_items)
{
  /* We can't look at tracker->widget here because we might not
   * have set it yet.
   */
  if (tracker->position == GTK_INVALID_LIST_POSITION)
    return FALSE;

  /* This is magic I made up that is meant to be both
   * correct and doesn't overflow when start and/or end are close to 0 or
   * close to max.
   * But beware, I didn't test it.
   */
  *out_n_items = tracker->n_before + tracker->n_after + 1;
  *out_n_items = MIN (*out_n_items, n_items);

  *out_start = MAX (tracker->position, tracker->n_before) - tracker->n_before;
  *out_start = MIN (*out_start, n_items - *out_n_items);

  return TRUE;
}

static void
gtk_list_item_query_tracked_range (GtkListItemManager *self,
                                   guint               n_items,
                                   guint               position,
                                   guint              *out_n_items,
                                   gboolean           *out_tracked)
{
  GSList *l;
  guint tracker_start, tracker_n_items;

  g_assert (position < n_items);

  *out_tracked = FALSE;
  *out_n_items = n_items - position;

  /* step 1: Check if position is tracked */

  for (l = self->trackers; l; l = l->next)
    {
      if (!gtk_list_item_tracker_query_range (self, l->data, n_items, &tracker_start, &tracker_n_items))
        continue;

      if (tracker_start > position)
        {
          *out_n_items = MIN (*out_n_items, tracker_start - position);
        }
      else if (tracker_start + tracker_n_items <= position)
        {
          /* do nothing */
        }
      else
        {
          *out_tracked = TRUE;
          *out_n_items = tracker_start + tracker_n_items - position;
          break;
        }
    }

  /* If nothing's tracked, we're done */
  if (!*out_tracked)
    return;

  /* step 2: make the tracked range as large as possible
   * NB: This is O(N_TRACKERS^2), but the number of trackers should be <5 */
restart:
  for (l = self->trackers; l; l = l->next)
    {
      if (!gtk_list_item_tracker_query_range (self, l->data, n_items, &tracker_start, &tracker_n_items))
        continue;

      if (tracker_start + tracker_n_items <= position + *out_n_items)
        continue;
      if (tracker_start > position + *out_n_items)
        continue;

      if (*out_n_items + position < tracker_start + tracker_n_items)
        {
          *out_n_items = tracker_start + tracker_n_items - position;
          goto restart;
        }
    }
}

static GtkListTile *
gtk_list_item_manager_ensure_split (GtkListItemManager *self,
                                    GtkListTile        *tile,
                                    guint               n_items)
{
  return self->split_func (self->widget, tile, n_items);
}

static void
gtk_list_item_manager_remove_items (GtkListItemManager *self,
                                    GtkListItemChange  *change,
                                    guint               position,
                                    guint               n_items)
{
  GtkListTile *tile, *header;
  guint offset;

  if (n_items == 0)
    return;

  tile = gtk_list_item_manager_get_nth (self, position, &offset);
  if (offset)
    tile = gtk_list_item_manager_ensure_split (self, tile, offset);
  header = gtk_list_tile_get_previous_skip (tile);
  if (header != NULL &&
      (header->type != GTK_LIST_TILE_HEADER && header->type != GTK_LIST_TILE_UNMATCHED_HEADER))
    header = NULL;

  while (n_items > 0)
    {
      switch (tile->type)
        {
        case GTK_LIST_TILE_HEADER:
        case GTK_LIST_TILE_UNMATCHED_HEADER:
          g_assert (header == NULL);
          header = tile;
          break;

        case GTK_LIST_TILE_FOOTER:
        case GTK_LIST_TILE_UNMATCHED_FOOTER:
          if (header)
            {
              gtk_list_item_change_clear_header (change, &header->widget);
              gtk_list_tile_set_type (header, GTK_LIST_TILE_REMOVED);
              gtk_list_tile_set_type (tile, GTK_LIST_TILE_REMOVED);
              header = NULL;
            }
          break;

        case GTK_LIST_TILE_ITEM:
          if (tile->n_items > n_items)
            {
              gtk_list_item_manager_ensure_split (self, tile, n_items);
              g_assert (tile->n_items <= n_items);
            }
          if (tile->widget)
            gtk_list_item_change_release (change, GTK_LIST_ITEM_BASE (tile->widget));
          tile->widget = NULL;
          n_items -= tile->n_items;
          tile->n_items = 0;
          gtk_list_tile_set_type (tile, GTK_LIST_TILE_REMOVED);
          break;

        case GTK_LIST_TILE_REMOVED:
        default:
          g_assert_not_reached ();
          break;
        }

      tile = gtk_list_tile_get_next_skip (tile);
    }

  if (header)
    {
      if (tile->type == GTK_LIST_TILE_FOOTER || tile->type == GTK_LIST_TILE_UNMATCHED_FOOTER)
        {
          gtk_list_item_change_clear_header (change, &header->widget);
          gtk_list_tile_set_type (header, GTK_LIST_TILE_REMOVED);
          gtk_list_tile_set_type (tile, GTK_LIST_TILE_REMOVED);
        }
    }

  gtk_widget_queue_resize (GTK_WIDGET (self->widget));
}

static void
gtk_list_item_manager_add_items (GtkListItemManager *self,
                                 GtkListItemChange  *change,
                                 guint               position,
                                 guint               n_items)
{
  GtkListTile *tile;
  guint offset;
  gboolean has_sections;

  if (n_items == 0)
    return;

  has_sections = gtk_list_item_manager_has_sections (self);

  tile = gtk_list_item_manager_get_nth (self, position, &offset);
  if (tile == NULL)
    {
      /* at end of list, pick the footer */
      for (tile = gtk_rb_tree_get_last (self->items);
           tile && tile->type == GTK_LIST_TILE_REMOVED;
           tile = gtk_rb_tree_node_get_previous (tile))
        { }

      if (tile == NULL)
        {
          /* empty list, there isn't even a footer yet */
          tile = gtk_rb_tree_insert_after (self->items, NULL);
          tile->type = GTK_LIST_TILE_UNMATCHED_HEADER;

          tile = gtk_rb_tree_insert_after (self->items, tile);
          tile->type = GTK_LIST_TILE_UNMATCHED_FOOTER;
        }
      else if (has_sections && tile->type == GTK_LIST_TILE_FOOTER)
        {
          GtkListTile *header;

          gtk_list_tile_set_type (tile, GTK_LIST_TILE_UNMATCHED_FOOTER);

          header = gtk_list_tile_get_header (self, tile);
          gtk_list_item_change_clear_header (change, &header->widget);
          gtk_list_tile_set_type (header, GTK_LIST_TILE_UNMATCHED_HEADER);
        }
    }
  if (offset)
    tile = gtk_list_item_manager_ensure_split (self, tile, offset);

  tile = gtk_rb_tree_insert_before (self->items, tile);
  tile->type = GTK_LIST_TILE_ITEM;
  tile->n_items = n_items;
  gtk_rb_tree_node_mark_dirty (tile);

  if (has_sections)
    {
      GtkListTile *section = gtk_list_tile_get_previous_skip (tile);

      if (section != NULL && section->type == GTK_LIST_TILE_HEADER)
        {
          guint start, end;
          GtkListTile *footer = gtk_list_tile_get_footer (self, section);
          GtkListTile *previous_footer = gtk_list_tile_get_previous_skip (section);

          gtk_section_model_get_section (GTK_SECTION_MODEL (self->model), position, &start, &end);

          if (previous_footer != NULL && previous_footer->type == GTK_LIST_TILE_FOOTER &&
              position > start && position < end)
          {
            gtk_list_item_change_clear_header (change, &section->widget);
            gtk_list_tile_set_type (section, GTK_LIST_TILE_REMOVED);
            gtk_list_tile_set_type (previous_footer, GTK_LIST_TILE_REMOVED);

            section = gtk_list_tile_get_header (self, previous_footer);
          }

          gtk_list_item_change_clear_header (change, &section->widget);
          gtk_list_tile_set_type (section,
                                  GTK_LIST_TILE_UNMATCHED_HEADER);
          gtk_list_tile_set_type (footer,
                                  GTK_LIST_TILE_UNMATCHED_FOOTER);
        }
    }

  gtk_widget_queue_resize (GTK_WIDGET (self->widget));
}

static gboolean
gtk_list_item_manager_merge_list_items (GtkListItemManager *self,
                                        GtkListTile        *first,
                                        GtkListTile        *second)
{
  if (first->widget || second->widget ||
      first->type != GTK_LIST_TILE_ITEM || second->type != GTK_LIST_TILE_ITEM)
    return FALSE;

  first->n_items += second->n_items;
  gtk_rb_tree_node_mark_dirty (first);
  gtk_rb_tree_remove (self->items, second);

  return TRUE;
}

/*
 * gtk_list_tile_split:
 * @self: the listitemmanager
 * @tile: a tile to split into two
 * @n_items: number of items to keep in tile
 *
 * Splits the given tile into two tiles. The original
 * tile will remain with @n_items items, the remaining
 * items will be given to the new tile, which will be
 * inserted after the tile.
 *
 * It is not valid for either tile to have 0 items after
 * the split.
 *
 * This function does not update the tiles' areas.
 *
 * Returns: The new tile
 **/
GtkListTile *
gtk_list_tile_split (GtkListItemManager *self,
                     GtkListTile        *tile,
                     guint               n_items)
{
  GtkListTile *result;

  g_assert (n_items > 0);
  g_assert (n_items < tile->n_items);
  g_assert (tile->type == GTK_LIST_TILE_ITEM);

  result = gtk_rb_tree_insert_after (self->items, tile);
  result->type = GTK_LIST_TILE_ITEM;
  result->n_items = tile->n_items - n_items;
  tile->n_items = n_items;
  gtk_rb_tree_node_mark_dirty (tile);

  return result;
}

/*
 * gtk_list_tile_gc:
 * @self: the listitemmanager
 * @tile: a tile or NULL
 *
 * Tries to get rid of tiles when they aren't needed anymore,
 * either because their referenced listitems were deleted or
 * because they can be merged with the next item(s).
 *
 * Note that this only looks forward, but never backward.
 *
 * Returns: The next tile or NULL if everything was gc'ed
 **/
static GtkListTile *
gtk_list_tile_gc (GtkListItemManager *self,
                  GtkListTile        *tile)
{
  GtkListTile *next;

  if (tile == NULL)
    return NULL;

  while (tile)
    {
      next = gtk_rb_tree_node_get_next (tile);
      while (next && next->type == GTK_LIST_TILE_REMOVED)
        {
          gtk_rb_tree_remove (self->items, next);
          next = gtk_rb_tree_node_get_next (tile);
        }

      switch (tile->type)
        {
        case GTK_LIST_TILE_ITEM:
          g_assert (tile->n_items > 0);
          if (next == NULL)
            break;
          if (gtk_list_item_manager_merge_list_items (self, tile, next))
            continue;
          break;

        case GTK_LIST_TILE_HEADER:
        case GTK_LIST_TILE_FOOTER:
        case GTK_LIST_TILE_UNMATCHED_HEADER:
        case GTK_LIST_TILE_UNMATCHED_FOOTER:
          break;

        case GTK_LIST_TILE_REMOVED:
          gtk_rb_tree_remove (self->items, tile);
          tile = next;
          continue;

        default:
          g_assert_not_reached ();
          break;
        }

      break;
    }

  return tile;
}

/*
 * gtk_list_item_manager_gc_tiles:
 * @self: the listitemmanager
 *
 * Removes all tiles of type GTK_LIST_TILE_REMOVED
 * and merges item tiles as much as possible.
 *
 * This function does not update the tiles' areas.
 */
void
gtk_list_item_manager_gc_tiles (GtkListItemManager *self)
{
  GtkListTile *tile;

  for (tile = gtk_list_tile_gc (self, gtk_list_item_manager_get_first (self));
       tile != NULL;
       tile = gtk_list_tile_gc (self, gtk_rb_tree_node_get_next (tile)))
    {
    }
}

static void
gtk_list_item_manager_release_items (GtkListItemManager *self,
                                     GtkListItemChange  *change)
{
  GtkListTile *tile, *header;
  guint position, i, n_items, query_n_items;
  gboolean tracked, deleted_section;

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->model));
  position = 0;

  while (position < n_items)
    {
      gtk_list_item_query_tracked_range (self, n_items, position, &query_n_items, &tracked);
      if (tracked)
        {
          position += query_n_items;
          continue;
        }

      deleted_section = FALSE;
      tile = gtk_list_item_manager_get_nth (self, position, &i);
      if (i == 0)
        {
          header = gtk_list_tile_get_previous_skip (tile);
          if (header && !gtk_list_tile_is_header (header))
            header = NULL;
        }
      else
        header = NULL;
      i = position - i;
      while (i < position + query_n_items)
        {
          g_assert (tile != NULL);
          switch (tile->type)
            {
            case GTK_LIST_TILE_ITEM:
              if (tile->widget)
                {
                  gtk_list_item_change_recycle (change, GTK_LIST_ITEM_BASE (tile->widget));
                  tile->widget = NULL;
                }
              i += tile->n_items;
              break;

            case GTK_LIST_TILE_HEADER:
            case GTK_LIST_TILE_UNMATCHED_HEADER:
              g_assert (deleted_section);
              gtk_list_item_change_clear_header (change, &tile->widget);
              G_GNUC_FALLTHROUGH;
            case GTK_LIST_TILE_FOOTER:
            case GTK_LIST_TILE_UNMATCHED_FOOTER:
              gtk_list_tile_set_type (tile, GTK_LIST_TILE_REMOVED);
              deleted_section = TRUE;
              header = NULL;
              break;

            case GTK_LIST_TILE_REMOVED:
            default:
              g_assert_not_reached ();
              break;
            }
          tile = gtk_list_tile_get_next_skip (tile);
        }
      if (header && gtk_list_tile_is_footer (tile))
        deleted_section = TRUE;
      if (deleted_section)
        {
          if (header == NULL)
            header = gtk_list_tile_get_header (self, tile);
          gtk_list_item_change_clear_header (change, &header->widget);
          gtk_list_tile_set_type (header, GTK_LIST_TILE_UNMATCHED_HEADER);

          tile = gtk_list_tile_get_footer (self, tile);
          gtk_list_tile_set_type (tile, GTK_LIST_TILE_UNMATCHED_FOOTER);
        }
      position += query_n_items;
    }
}

static GtkListTile *
gtk_list_item_manager_insert_section (GtkListItemManager *self,
                                      guint               pos,
                                      GtkListTileType     footer_type,
                                      GtkListTileType     header_type)
{
  GtkListTile *tile, *footer, *header;
  guint offset;

  tile = gtk_list_item_manager_get_nth (self, pos, &offset);
  if (tile == NULL)
    {
      if (footer_type == GTK_LIST_TILE_FOOTER)
        {
          footer = gtk_rb_tree_get_last (self->items);
          if (footer->type != GTK_LIST_TILE_FOOTER && footer->type != GTK_LIST_TILE_UNMATCHED_FOOTER)
            footer = gtk_list_tile_get_previous_skip (footer);
          gtk_list_tile_set_type (footer, footer_type);
        }
      return NULL;
    }

  if (offset)
    tile = gtk_list_item_manager_ensure_split (self, tile, offset);

  header = gtk_list_tile_get_previous_skip (tile);
  if (header != NULL &&
      (header->type == GTK_LIST_TILE_HEADER || header->type == GTK_LIST_TILE_UNMATCHED_HEADER))
    {
      if (header_type == GTK_LIST_TILE_HEADER)
        gtk_list_tile_set_type (header, header_type);
      if (footer_type == GTK_LIST_TILE_FOOTER)
        {
          footer = gtk_list_tile_get_previous_skip (header);
          if (footer)
            gtk_list_tile_set_type (footer, footer_type);
        }
    }
  else
    {
      self->prepare_section (self->widget, tile, pos);

      header = gtk_rb_tree_insert_before (self->items, tile);
      gtk_list_tile_set_type (header, header_type);
      footer = gtk_rb_tree_insert_before (self->items, header);
      gtk_list_tile_set_type (footer, footer_type);
    }

  return header;
}

static GtkWidget *
gtk_list_tile_find_widget_before (GtkListTile *tile)
{
  GtkListTile *other;

  for (other = gtk_rb_tree_node_get_previous (tile);
       other;
       other = gtk_rb_tree_node_get_previous (other))
     {
       if (other->widget)
         return other->widget;
     }

  return NULL;
}

static void
gtk_list_item_manager_ensure_items (GtkListItemManager *self,
                                    GtkListItemChange  *change,
                                    guint               update_start,
                                    int                 update_diff)
{
  GtkListTile *tile, *header;
  GtkWidget *insert_after;
  guint position, i, n_items, query_n_items, offset;
  gboolean tracked, has_sections;

  if (self->model == NULL)
    return;

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->model));
  position = 0;
  has_sections = gtk_list_item_manager_has_sections (self);

  gtk_list_item_manager_release_items (self, change);

  while (position < n_items)
    {
      gtk_list_item_query_tracked_range (self, n_items, position, &query_n_items, &tracked);
      if (!tracked)
        {
          position += query_n_items;
          continue;
        }

      tile = gtk_list_item_manager_get_nth (self, position, &offset);
      if (offset > 0)
        tile = gtk_list_item_manager_ensure_split (self, tile, offset);

      if (has_sections)
        {
          header = gtk_list_tile_get_header (self, tile);
          if (header->type == GTK_LIST_TILE_UNMATCHED_HEADER)
            {
              guint start, end;
              gpointer item;

              gtk_section_model_get_section (GTK_SECTION_MODEL (self->model), position, &start, &end);
              header = gtk_list_item_manager_insert_section (self,
                                                             start,
                                                             GTK_LIST_TILE_UNMATCHED_FOOTER,
                                                             GTK_LIST_TILE_HEADER);
              g_assert (header != NULL && header->widget == NULL);
              header->widget = GTK_WIDGET (gtk_list_item_change_get_header (change));
              if (header->widget == NULL)
                header->widget = GTK_WIDGET (self->create_header_widget (self->widget));
              item = g_list_model_get_item (G_LIST_MODEL (self->model), start);
              gtk_list_header_base_update (GTK_LIST_HEADER_BASE (header->widget),
                                           item,
                                           start, end);
              g_object_unref (item);
              gtk_widget_insert_after (header->widget,
                                       self->widget,
                                       gtk_list_tile_find_widget_before (header));

              gtk_list_item_manager_insert_section (self,
                                                    end,
                                                    GTK_LIST_TILE_FOOTER,
                                                    GTK_LIST_TILE_UNMATCHED_HEADER);
            }
          else if (gtk_list_header_base_get_end (GTK_LIST_HEADER_BASE (header->widget)) > update_start)
            {
              GtkListHeaderBase *base = GTK_LIST_HEADER_BASE (header->widget);
              guint start = gtk_list_header_base_get_start (base);
              gtk_list_header_base_update (base,
                                           gtk_list_header_base_get_item (base),
                                           start > update_start ? start + update_diff : start,
                                           gtk_list_header_base_get_end (base) + update_diff);
            }
        }

      insert_after = gtk_list_tile_find_widget_before (tile);

      for (i = 0; i < query_n_items;)
        {
          g_assert (tile != NULL);

          switch (tile->type)
          {
            case GTK_LIST_TILE_ITEM:
              if (tile->n_items > 1)
                gtk_list_item_manager_ensure_split (self, tile, 1);

              if (tile->widget == NULL)
                {
                  gpointer item = g_list_model_get_item (G_LIST_MODEL (self->model), position + i);
                  tile->widget = GTK_WIDGET (gtk_list_item_change_get (change, item));
                  if (tile->widget == NULL)
                    tile->widget = GTK_WIDGET (self->create_widget (self->widget));
                  gtk_list_item_base_update (GTK_LIST_ITEM_BASE (tile->widget),
                                             position + i,
                                             item,
                                             gtk_selection_model_is_selected (self->model, position + i));
                  g_object_unref (item);
                  gtk_widget_insert_after (tile->widget, self->widget, insert_after);
                }
              else
                {
                  if (update_start <= position + i)
                    {
                      gtk_list_item_base_update (GTK_LIST_ITEM_BASE (tile->widget),
                                                 position + i,
                                                 gtk_list_item_base_get_item (GTK_LIST_ITEM_BASE (tile->widget)),
                                                 gtk_selection_model_is_selected (self->model, position + i));
                    }
                }
              insert_after = tile->widget;
              i++;
              break;

            case GTK_LIST_TILE_UNMATCHED_HEADER:
              if (has_sections)
                {
                  guint start, end;
                  gpointer item;

                  gtk_section_model_get_section (GTK_SECTION_MODEL (self->model), position + i, &start, &end);

                  gtk_list_tile_set_type (tile, GTK_LIST_TILE_HEADER);
                  g_assert (tile->widget == NULL);
                  tile->widget = GTK_WIDGET (gtk_list_item_change_get_header (change));
                  if (tile->widget == NULL)
                    tile->widget = GTK_WIDGET (self->create_header_widget (self->widget));
                  item = g_list_model_get_item (G_LIST_MODEL (self->model), start);
                  gtk_list_header_base_update (GTK_LIST_HEADER_BASE (tile->widget),
                                               item,
                                               start, end);
                  g_object_unref (item);
                  gtk_widget_insert_after (tile->widget, self->widget, insert_after);
                  insert_after = tile->widget;

                  gtk_list_item_manager_insert_section (self,
                                                        end,
                                                        GTK_LIST_TILE_FOOTER,
                                                        GTK_LIST_TILE_UNMATCHED_HEADER);
                }
              break;

            case GTK_LIST_TILE_HEADER:
            case GTK_LIST_TILE_FOOTER:
              break;

            case GTK_LIST_TILE_UNMATCHED_FOOTER:
            case GTK_LIST_TILE_REMOVED:
            default:
              g_assert_not_reached ();
              break;
            }
          tile = gtk_list_tile_get_next_skip (tile);
        }

      position += query_n_items;
    }
}

static void
gtk_list_item_manager_model_items_changed_cb (GListModel         *model,
                                              guint               position,
                                              guint               removed,
                                              guint               added,
                                              GtkListItemManager *self)
{
  GtkListItemChange change;
  GSList *l;
  guint n_items;

  gtk_list_item_change_init (&change);
  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->model));

  gtk_list_item_manager_remove_items (self, &change, position, removed);
  gtk_list_item_manager_add_items (self, &change, position, added);

  /* Check if any tracked item was removed */
  for (l = self->trackers; l; l = l->next)
    {
      GtkListItemTracker *tracker = l->data;

      if (tracker->widget == NULL)
        continue;

      if (tracker->position >= position && tracker->position < position + removed)
        break;
    }

  /* At least one tracked item was removed, do a more expensive rebuild
   * trying to find where it moved */
  if (l)
    {
      GtkListTile *tile, *new_tile;
      GtkWidget *insert_after;
      guint i, offset;

      tile = gtk_list_item_manager_get_nth (self, position, &offset);
      for (new_tile = tile ? gtk_rb_tree_node_get_previous (tile) : gtk_rb_tree_get_last (self->items);
           new_tile && new_tile->widget == NULL;
           new_tile = gtk_rb_tree_node_get_previous (new_tile))
        { }
      if (new_tile)
        insert_after = new_tile->widget;
      else
        insert_after = NULL; /* we're at the start */

      for (i = 0; i < added; i++)
        {
          GtkListItemBase *widget;
          gpointer item;

          /* XXX: can we avoid temporarily allocating items on failure? */
          item = g_list_model_get_item (G_LIST_MODEL (self->model), position + i);
          widget = gtk_list_item_change_find (&change, item);
          g_object_unref (item);

          if (widget == NULL)
            {
              offset++;
              continue;
            }

          while (offset >= tile->n_items)
            {
              offset -= tile->n_items;
              tile = gtk_rb_tree_node_get_next (tile);
            }
          if (offset > 0)
            {
              tile = gtk_list_item_manager_ensure_split (self, tile, offset);
              offset = 0;
            }

          new_tile = tile;
          if (tile->n_items == 1)
            tile = gtk_rb_tree_node_get_next (tile);
          else
            tile = gtk_list_item_manager_ensure_split (self, tile, 1);

          new_tile->widget = GTK_WIDGET (widget);
          gtk_list_item_base_update (widget,
                                     position + i,
                                     item,
                                     gtk_selection_model_is_selected (self->model, position + i));
          gtk_widget_insert_after (new_tile->widget, self->widget, insert_after);
          insert_after = new_tile->widget;
        }
    }

  /* Update tracker positions if necessary, they need to have correct
   * positions for gtk_list_item_manager_ensure_items().
   * We don't update the items, they will be updated by ensure_items()
   * and then we can update them. */
  for (l = self->trackers; l; l = l->next)
    {
      GtkListItemTracker *tracker = l->data;

      if (tracker->position == GTK_INVALID_LIST_POSITION)
        {
          /* if the list is no longer empty, set the tracker to a valid position. */
          if (n_items > 0 && n_items == added && removed == 0)
            tracker->position = 0;
        }
      else if (tracker->position >= position + removed)
        {
          tracker->position += added - removed;
        }
      else if (tracker->position >= position)
        {
          GtkListItemBase *widget = gtk_list_item_change_find (&change, gtk_list_item_base_get_item (tracker->widget));
          if (widget)
            {
              /* The item is still in the recycling pool, which means it got deleted.
               * Put the widget back and then guess a good new position */
              gtk_list_item_change_release (&change, widget);

              tracker->position = position + (tracker->position - position) * added / removed;
              if (tracker->position >= n_items)
                {
                  if (n_items == 0)
                    tracker->position = GTK_INVALID_LIST_POSITION;
                  else
                    tracker->position--;
                }
              tracker->widget = NULL;
            }
          else
            {
              /* item was put in its right place in the expensive loop above,
               * and we updated its position while at it. So grab it from there.
               */
              tracker->position = gtk_list_item_base_get_position (tracker->widget);
            }
        }
      else
        {
          /* nothing changed for items before position */
        }
    }

  gtk_list_item_manager_ensure_items (self, &change, position + added, added - removed);

  /* final loop through the trackers: Grab the missing widgets.
   * For items that had been removed and a new position was set, grab
   * their item now that we ensured it exists.
   */
  for (l = self->trackers; l; l = l->next)
    {
      GtkListItemTracker *tracker = l->data;
      GtkListTile *tile;

      if (tracker->widget != NULL ||
          tracker->position == GTK_INVALID_LIST_POSITION)
        continue;

      tile = gtk_list_item_manager_get_nth (self, tracker->position, NULL);
      g_assert (tile != NULL);
      g_assert (tile->widget);
      tracker->widget = GTK_LIST_ITEM_BASE (tile->widget);
    }

  gtk_list_item_change_finish (&change);

  gtk_widget_queue_resize (self->widget);
}

static void
gtk_list_item_manager_model_sections_changed_cb (GListModel         *model,
                                                 guint               position,
                                                 guint               n_items,
                                                 GtkListItemManager *self)
{
  GtkListItemChange change;
  GtkListTile *tile, *header;
  guint offset;

  if (!gtk_list_item_manager_has_sections (self))
    return;

  gtk_list_item_change_init (&change);

  tile = gtk_list_item_manager_get_nth (self, position, &offset);
  header = gtk_list_tile_get_header (self, tile);
  gtk_list_item_change_clear_header (&change, &header->widget);
  gtk_list_tile_set_type (header, GTK_LIST_TILE_UNMATCHED_HEADER);

  n_items += offset;
  while (n_items > 0)
    {
      switch (tile->type)
        {
        case GTK_LIST_TILE_HEADER:
        case GTK_LIST_TILE_UNMATCHED_HEADER:
          gtk_list_item_change_clear_header (&change, &tile->widget);
          gtk_list_tile_set_type (tile, GTK_LIST_TILE_REMOVED);
          break;

        case GTK_LIST_TILE_FOOTER:
        case GTK_LIST_TILE_UNMATCHED_FOOTER:
          gtk_list_tile_set_type (tile, GTK_LIST_TILE_REMOVED);
          break;

        case GTK_LIST_TILE_ITEM:
          n_items -= MIN (n_items, tile->n_items);
          break;

        case GTK_LIST_TILE_REMOVED:
        default:
          g_assert_not_reached ();
          break;
        }

      tile = gtk_list_tile_get_next_skip (tile);
    }

  if (!gtk_list_tile_is_footer (tile))
    tile = gtk_list_tile_get_footer (self, tile);

  gtk_list_tile_set_type (tile, GTK_LIST_TILE_UNMATCHED_FOOTER);

  gtk_list_item_manager_ensure_items (self, &change, G_MAXUINT, 0);

  gtk_list_item_change_finish (&change);

  gtk_widget_queue_resize (GTK_WIDGET (self->widget));
}

static void
gtk_list_item_manager_model_selection_changed_cb (GListModel         *model,
                                                  guint               position,
                                                  guint               n_items,
                                                  GtkListItemManager *self)
{
  GtkListTile *tile;
  guint offset;

  tile = gtk_list_item_manager_get_nth (self, position, &offset);

  if (offset)
    {
      position += tile->n_items - offset;
      if (tile->n_items - offset > n_items)
        n_items = 0;
      else
        n_items -= tile->n_items - offset;
      tile = gtk_rb_tree_node_get_next (tile);
    }

  while (n_items > 0)
    {
      if (tile->widget && tile->type == GTK_LIST_TILE_ITEM)
        {
          gtk_list_item_base_update (GTK_LIST_ITEM_BASE (tile->widget),
                                     position,
                                     gtk_list_item_base_get_item (GTK_LIST_ITEM_BASE (tile->widget)),
                                     gtk_selection_model_is_selected (self->model, position));
        }
      position += tile->n_items;
      n_items -= MIN (n_items, tile->n_items);
      tile = gtk_list_tile_get_next_skip (tile);
    }
}

static void
gtk_list_item_manager_clear_model (GtkListItemManager *self)
{
  GtkListItemChange change;
  GSList *l;

  if (self->model == NULL)
    return;

  gtk_list_item_change_init (&change);
  gtk_list_item_manager_remove_items (self, &change, 0, g_list_model_get_n_items (G_LIST_MODEL (self->model)));
  gtk_list_item_change_finish (&change);
  for (l = self->trackers; l; l = l->next)
    {
      gtk_list_item_tracker_unset_position (self, l->data);
    }

  g_signal_handlers_disconnect_by_func (self->model,
                                        gtk_list_item_manager_model_selection_changed_cb,
                                        self);
  g_signal_handlers_disconnect_by_func (self->model,
                                        gtk_list_item_manager_model_items_changed_cb,
                                        self);
  g_signal_handlers_disconnect_by_func (self->model,
                                        gtk_list_item_manager_model_sections_changed_cb,
                                        self);
  g_clear_object (&self->model);

  gtk_list_item_manager_gc_tiles (self);

  g_assert (gtk_rb_tree_get_root (self->items) == NULL);
}

static void
gtk_list_item_manager_dispose (GObject *object)
{
  GtkListItemManager *self = GTK_LIST_ITEM_MANAGER (object);

  gtk_list_item_manager_clear_model (self);

  g_clear_pointer (&self->items, gtk_rb_tree_unref);

  G_OBJECT_CLASS (gtk_list_item_manager_parent_class)->dispose (object);
}

static void
gtk_list_item_manager_class_init (GtkListItemManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_list_item_manager_dispose;
}

static void
gtk_list_item_manager_init (GtkListItemManager *self)
{
}

void
gtk_list_item_manager_set_model (GtkListItemManager *self,
                                 GtkSelectionModel  *model)
{
  g_return_if_fail (GTK_IS_LIST_ITEM_MANAGER (self));
  g_return_if_fail (model == NULL || GTK_IS_SELECTION_MODEL (model));

  if (self->model == model)
    return;

  gtk_list_item_manager_clear_model (self);

  if (model)
    {
      GtkListItemChange change;

      self->model = g_object_ref (model);

      g_signal_connect (model,
                        "items-changed",
                        G_CALLBACK (gtk_list_item_manager_model_items_changed_cb),
                        self);
      g_signal_connect (model,
                        "selection-changed",
                        G_CALLBACK (gtk_list_item_manager_model_selection_changed_cb),
                        self);
      if (GTK_IS_SECTION_MODEL (model))
        g_signal_connect (model,
                          "sections-changed",
                          G_CALLBACK (gtk_list_item_manager_model_sections_changed_cb),
                          self);

      gtk_list_item_change_init (&change);
      gtk_list_item_manager_add_items (self, &change, 0, g_list_model_get_n_items (G_LIST_MODEL (model)));
      gtk_list_item_manager_ensure_items (self, &change, G_MAXUINT, 0);
      gtk_list_item_change_finish (&change);
    }
}

GtkSelectionModel *
gtk_list_item_manager_get_model (GtkListItemManager *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM_MANAGER (self), NULL);

  return self->model;
}

void
gtk_list_item_manager_set_has_sections (GtkListItemManager *self,
                                        gboolean            has_sections)
{
  GtkListItemChange change;
  GtkListTile *tile;
  gboolean had_sections;

  if (self->has_sections == has_sections)
    return;

  had_sections = gtk_list_item_manager_has_sections (self);

  self->has_sections = has_sections;

  gtk_list_item_change_init (&change);

  if (had_sections && !gtk_list_item_manager_has_sections (self))
    {
      GtkListTile *header = NULL, *footer = NULL;

      for (tile = gtk_rb_tree_get_first (self->items);
           tile;
           tile = gtk_list_tile_get_next_skip (tile))
        {
          switch (tile->type)
            {
            case GTK_LIST_TILE_HEADER:
            case GTK_LIST_TILE_UNMATCHED_HEADER:
              gtk_list_item_change_clear_header (&change, &tile->widget);
              if (!header)
                header = tile;
              else
                gtk_list_tile_set_type (tile, GTK_LIST_TILE_REMOVED);
              break;
            case GTK_LIST_TILE_FOOTER:
            case GTK_LIST_TILE_UNMATCHED_FOOTER:
              if (footer)
                gtk_list_tile_set_type (footer, GTK_LIST_TILE_REMOVED);
              footer = tile;
              break;
            case GTK_LIST_TILE_ITEM:
            case GTK_LIST_TILE_REMOVED:
              break;
            default:
              g_assert_not_reached ();
              break;
            }
        }
      if (header)
        {
          gtk_list_tile_set_type (header, GTK_LIST_TILE_UNMATCHED_HEADER);
          gtk_list_tile_set_type (footer, GTK_LIST_TILE_UNMATCHED_FOOTER);
        }
    }

  gtk_list_item_manager_ensure_items (self, &change, G_MAXUINT, 0);
  gtk_list_item_change_finish (&change);

  gtk_widget_queue_resize (self->widget);
}

gboolean
gtk_list_item_manager_get_has_sections (GtkListItemManager *self)
{
  return self->has_sections;
}

GtkListItemTracker *
gtk_list_item_tracker_new (GtkListItemManager *self)
{
  GtkListItemTracker *tracker;

  g_return_val_if_fail (GTK_IS_LIST_ITEM_MANAGER (self), NULL);

  tracker = g_new0 (GtkListItemTracker, 1);

  tracker->position = GTK_INVALID_LIST_POSITION;

  self->trackers = g_slist_prepend (self->trackers, tracker);

  return tracker;
}

void
gtk_list_item_tracker_free (GtkListItemManager *self,
                            GtkListItemTracker *tracker)
{
  GtkListItemChange change;

  gtk_list_item_tracker_unset_position (self, tracker);

  self->trackers = g_slist_remove (self->trackers, tracker);

  g_free (tracker);

  gtk_list_item_change_init (&change);
  gtk_list_item_manager_ensure_items (self, &change, G_MAXUINT, 0);
  gtk_list_item_change_finish (&change);

  gtk_widget_queue_resize (self->widget);
}

void
gtk_list_item_tracker_set_position (GtkListItemManager *self,
                                    GtkListItemTracker *tracker,
                                    guint               position,
                                    guint               n_before,
                                    guint               n_after)
{
  GtkListItemChange change;
  GtkListTile *tile;
  guint n_items;

  gtk_list_item_tracker_unset_position (self, tracker);

  if (self->model == NULL)
    return;

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->model));
  if (position >= n_items)
    position = n_items - 1; /* for n_items == 0 this underflows to GTK_INVALID_LIST_POSITION */

  tracker->position = position;
  tracker->n_before = n_before;
  tracker->n_after = n_after;

  gtk_list_item_change_init (&change);
  gtk_list_item_manager_ensure_items (self, &change, G_MAXUINT, 0);
  gtk_list_item_change_finish (&change);

  tile = gtk_list_item_manager_get_nth (self, position, NULL);
  if (tile)
    tracker->widget = GTK_LIST_ITEM_BASE (tile->widget);

  gtk_widget_queue_resize (self->widget);
}

guint
gtk_list_item_tracker_get_position (GtkListItemManager *self,
                                    GtkListItemTracker *tracker)
{
  return tracker->position;
}
