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


#pragma once

#include "gtk/gtktypes.h"
#include "gtk/gtkenums.h"

#include "gtk/gtklistitembaseprivate.h"
#include "gtk/gtklistheaderbaseprivate.h"
#include "gtk/gtklistitemfactory.h"
#include "gtk/gtkrbtreeprivate.h"
#include "gtk/gtkselectionmodel.h"

G_BEGIN_DECLS

#define GTK_TYPE_LIST_ITEM_MANAGER         (gtk_list_item_manager_get_type ())
#define GTK_LIST_ITEM_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_LIST_ITEM_MANAGER, GtkListItemManager))
#define GTK_LIST_ITEM_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GTK_TYPE_LIST_ITEM_MANAGER, GtkListItemManagerClass))
#define GTK_IS_LIST_ITEM_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_LIST_ITEM_MANAGER))
#define GTK_IS_LIST_ITEM_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GTK_TYPE_LIST_ITEM_MANAGER))
#define GTK_LIST_ITEM_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_LIST_ITEM_MANAGER, GtkListItemManagerClass))

typedef struct _GtkListItemManager GtkListItemManager;
typedef struct _GtkListItemManagerClass GtkListItemManagerClass;
typedef struct _GtkListTile GtkListTile;
typedef struct _GtkListTileAugment GtkListTileAugment;
typedef struct _GtkListItemTracker GtkListItemTracker;

typedef enum
{
  GTK_LIST_TILE_ITEM,
  GTK_LIST_TILE_HEADER,
  GTK_LIST_TILE_FOOTER,
  GTK_LIST_TILE_UNMATCHED_HEADER,
  GTK_LIST_TILE_UNMATCHED_FOOTER,
  GTK_LIST_TILE_REMOVED,
} GtkListTileType;

struct _GtkListTile
{
  GtkListTileType type;
  GtkWidget *widget;
  guint n_items;
  /* area occupied by tile. May be empty if tile has no allocation */
  cairo_rectangle_int_t area;
};

struct _GtkListTileAugment
{
  guint n_items;

  guint has_header :1;
  guint has_footer :1;

  /* union of all areas of tile and children */
  cairo_rectangle_int_t area;
};


GType                   gtk_list_item_manager_get_type          (void) G_GNUC_CONST;

GtkListItemManager *    gtk_list_item_manager_new               (GtkWidget              *widget,
                                                                 GtkListTile *           (* split_func) (GtkWidget *, GtkListTile *, guint),
                                                                 GtkListItemBase *       (* create_widget) (GtkWidget *),
                                                                 void                    (* prepare_section) (GtkWidget *, GtkListTile *, guint),
                                                                 GtkListHeaderBase *     (* create_header_widget) (GtkWidget *));

void                    gtk_list_item_manager_get_tile_bounds   (GtkListItemManager     *self,
                                                                 GdkRectangle           *out_bounds);
gpointer                gtk_list_item_manager_get_root          (GtkListItemManager     *self);
gpointer                gtk_list_item_manager_get_first         (GtkListItemManager     *self);
gpointer                gtk_list_item_manager_get_last          (GtkListItemManager     *self);
gpointer                gtk_list_item_manager_get_nth           (GtkListItemManager     *self,
                                                                 guint                   position,
                                                                 guint                  *offset);
GtkListTile *           gtk_list_item_manager_get_nearest_tile  (GtkListItemManager     *self,
                                                                 int                     x,
                                                                 int                     y);
void                    gtk_list_item_manager_gc_tiles          (GtkListItemManager     *self);

static inline gboolean
gtk_list_tile_is_header (GtkListTile *tile)
{
  return tile->type == GTK_LIST_TILE_HEADER || tile->type == GTK_LIST_TILE_UNMATCHED_HEADER;
}

static inline gboolean
gtk_list_tile_is_footer (GtkListTile *tile)
{
  return tile->type == GTK_LIST_TILE_FOOTER || tile->type == GTK_LIST_TILE_UNMATCHED_FOOTER;
}

guint                   gtk_list_tile_get_position              (GtkListItemManager     *self,
                                                                 GtkListTile            *tile);
gpointer                gtk_list_tile_get_augment               (GtkListItemManager     *self,
                                                                 GtkListTile            *tile);
void                    gtk_list_tile_set_area                  (GtkListItemManager     *self,
                                                                 GtkListTile            *tile,
                                                                 const cairo_rectangle_int_t *area);
void                    gtk_list_tile_set_area_position         (GtkListItemManager     *self,
                                                                 GtkListTile            *tile,
                                                                 int                     x,
                                                                 int                     y);
void                    gtk_list_tile_set_area_size             (GtkListItemManager     *self,
                                                                 GtkListTile            *tile,
                                                                 int                     width,
                                                                 int                     height);

GtkListTile *           gtk_list_tile_split                     (GtkListItemManager     *self,
                                                                 GtkListTile            *tile,
                                                                 guint                   n_items);

void                    gtk_list_item_manager_set_model         (GtkListItemManager     *self,
                                                                 GtkSelectionModel      *model);
GtkSelectionModel *     gtk_list_item_manager_get_model         (GtkListItemManager     *self);
void                    gtk_list_item_manager_set_has_sections  (GtkListItemManager     *self,
                                                                 gboolean                has_sections);
gboolean                gtk_list_item_manager_get_has_sections  (GtkListItemManager     *self);

GtkListItemTracker *    gtk_list_item_tracker_new               (GtkListItemManager     *self);
void                    gtk_list_item_tracker_free              (GtkListItemManager     *self,
                                                                 GtkListItemTracker     *tracker);
void                    gtk_list_item_tracker_set_position      (GtkListItemManager     *self,
                                                                 GtkListItemTracker     *tracker,
                                                                 guint                   position,
                                                                 guint                   n_before,
                                                                 guint                   n_after);
guint                   gtk_list_item_tracker_get_position      (GtkListItemManager     *self,
                                                                 GtkListItemTracker     *tracker);


G_END_DECLS

