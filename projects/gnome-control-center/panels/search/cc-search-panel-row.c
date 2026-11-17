/*
 * Copyright © 2019 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Felipe Borges <felipeborges@gnome.org>
 */

#include "cc-search-panel-row.h"

struct _CcSearchPanelRow
{
  AdwActionRow   parent_instance;

  GAppInfo      *app_info;

  GtkImage      *icon;
  GtkSwitch     *switcher;

  GtkListBox    *drag_widget;
  gdouble        drag_x;
  gdouble        drag_y;
};

G_DEFINE_TYPE (CcSearchPanelRow, cc_search_panel_row, ADW_TYPE_ACTION_ROW)

enum
{
  SIGNAL_MOVE_ROW,
  SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0, };

static void
update_move_actions_after_row_moved_up (CcSearchPanelRow *self)
{
  GtkListBox *list_box = GTK_LIST_BOX (gtk_widget_get_parent (GTK_WIDGET (self)));
  gint previous_idx = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (self)) - 1;
  GtkListBoxRow *previous_row = gtk_list_box_get_row_at_index (list_box, previous_idx);

  if (gtk_list_box_get_row_at_index (list_box, previous_idx - 1) == NULL)
    {
      gtk_widget_action_set_enabled (GTK_WIDGET (self), "row.move-up", FALSE);
    }

  gtk_widget_action_set_enabled (GTK_WIDGET (previous_row), "row.move-up", TRUE);
  gtk_widget_action_set_enabled (GTK_WIDGET (previous_row), "row.move-down", gtk_widget_get_next_sibling (GTK_WIDGET (self)) != NULL);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "row.move-down", TRUE);
}

static void
update_move_actions_after_row_moved_down (CcSearchPanelRow *self)
{
  GtkListBox *list_box = GTK_LIST_BOX (gtk_widget_get_parent (GTK_WIDGET (self)));
  gint next_idx = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (self)) + 1;
  GtkListBoxRow *next_row = gtk_list_box_get_row_at_index (list_box, next_idx);

  if (gtk_widget_get_next_sibling (GTK_WIDGET (next_row)) == NULL)
    {
      gtk_widget_action_set_enabled (GTK_WIDGET (self), "row.move-down", FALSE);
    }

  gtk_widget_action_set_enabled (GTK_WIDGET (next_row), "row.move-up", next_idx-1 != 0);
  gtk_widget_action_set_enabled (GTK_WIDGET (next_row), "row.move-down", TRUE);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "row.move-up", TRUE);
}

static void
move_up_cb (GSimpleAction *action,
            GVariant      *parameter,
            gpointer       user_data)
{
  CcSearchPanelRow *self = CC_SEARCH_PANEL_ROW (user_data);
  GtkListBox *list_box = GTK_LIST_BOX (gtk_widget_get_parent (GTK_WIDGET (self)));
  gint previous_idx = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (self)) - 1;
  GtkListBoxRow *previous_row = gtk_list_box_get_row_at_index (list_box, previous_idx);

  if (previous_row == NULL)
    return;

  update_move_actions_after_row_moved_up (self);

  g_signal_emit (self,
                 signals[SIGNAL_MOVE_ROW],
                 0,
                 previous_row);
}

static void
move_down_cb (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
  CcSearchPanelRow *self = CC_SEARCH_PANEL_ROW (user_data);
  GtkListBox *list_box = GTK_LIST_BOX (gtk_widget_get_parent (GTK_WIDGET (self)));
  gint next_idx = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (self)) + 1;
  GtkListBoxRow *next_row = gtk_list_box_get_row_at_index (list_box, next_idx);

  if (next_row == NULL)
    return;

  update_move_actions_after_row_moved_down (self);

  g_signal_emit (next_row,
                 signals[SIGNAL_MOVE_ROW],
                 0,
                 self);
}

static GdkContentProvider *
drag_prepare_cb (CcSearchPanelRow *self,
                 double            x,
                 double            y)
{
  self->drag_x = x;
  self->drag_y = y;

  return gdk_content_provider_new_typed (CC_TYPE_SEARCH_PANEL_ROW, self);
}

static void
drag_begin_cb (CcSearchPanelRow *self,
               GdkDrag          *drag)
{
  CcSearchPanelRow *panel_row;
  GtkAllocation alloc;
  GtkWidget *drag_icon;

  gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);

  self->drag_widget = GTK_LIST_BOX (gtk_list_box_new ());
  gtk_widget_set_size_request (GTK_WIDGET (self->drag_widget), alloc.width, alloc.height);

  panel_row = cc_search_panel_row_new (self->app_info);
  gtk_switch_set_active (panel_row->switcher, gtk_switch_get_active (self->switcher));

  gtk_list_box_append (GTK_LIST_BOX (self->drag_widget), GTK_WIDGET (panel_row));
  gtk_list_box_drag_highlight_row (self->drag_widget, GTK_LIST_BOX_ROW (panel_row));

  drag_icon = gtk_drag_icon_get_for_drag (drag);
  gtk_drag_icon_set_child (GTK_DRAG_ICON (drag_icon), GTK_WIDGET (self->drag_widget));
  gdk_drag_set_hotspot (drag, self->drag_x, self->drag_y);

}

static gboolean
drop_cb (CcSearchPanelRow *self,
         const GValue     *value,
         gdouble           x,
         gdouble           y)
{
  CcSearchPanelRow *source;

  g_message ("Drop");

  if (!G_VALUE_HOLDS (value, CC_TYPE_SEARCH_PANEL_ROW))
    return FALSE;

  source = g_value_get_object (value);

  g_signal_emit (source,
                 signals[SIGNAL_MOVE_ROW],
                 0,
                 self);

  return TRUE;
}

static void
cc_search_panel_row_finalize (GObject *object)
{
  CcSearchPanelRow  *self = CC_SEARCH_PANEL_ROW (object);

  g_clear_object (&self->app_info);

  G_OBJECT_CLASS (cc_search_panel_row_parent_class)->finalize (object);
}

static void
cc_search_panel_row_class_init (CcSearchPanelRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_search_panel_row_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/search/cc-search-panel-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcSearchPanelRow, icon);
  gtk_widget_class_bind_template_child (widget_class, CcSearchPanelRow, switcher);

  signals[SIGNAL_MOVE_ROW] =
    g_signal_new ("move-row",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1, CC_TYPE_SEARCH_PANEL_ROW);
}

const GActionEntry row_entries[] = {
  { "move-up", move_up_cb, NULL, NULL, NULL, { 0 }  },
  { "move-down", move_down_cb, NULL, NULL, NULL, { 0 } }
};

static void
cc_search_panel_row_init (CcSearchPanelRow *self)
{
  GtkDragSource *drag_source;
  GtkDropTarget *drop_target;
  GSimpleActionGroup *group;

  gtk_widget_init_template (GTK_WIDGET (self));

  drag_source = gtk_drag_source_new ();
  gtk_drag_source_set_actions (drag_source, GDK_ACTION_MOVE);
  g_signal_connect_swapped (drag_source, "prepare", G_CALLBACK (drag_prepare_cb), self);
  g_signal_connect_swapped (drag_source, "drag-begin", G_CALLBACK (drag_begin_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (drag_source));

  drop_target = gtk_drop_target_new (CC_TYPE_SEARCH_PANEL_ROW, GDK_ACTION_MOVE);
  gtk_drop_target_set_preload (drop_target, TRUE);
  g_signal_connect_swapped (drop_target, "drop", G_CALLBACK (drop_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (drop_target));

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group),
                                   row_entries,
                                   G_N_ELEMENTS (row_entries),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "row", G_ACTION_GROUP (group));
}

CcSearchPanelRow *
cc_search_panel_row_new (GAppInfo *app_info)
{
  CcSearchPanelRow *self;
  g_autoptr(GIcon) gicon = NULL;

  self = g_object_new (CC_TYPE_SEARCH_PANEL_ROW, NULL);
  self->app_info = g_object_ref (app_info);

  gicon = g_app_info_get_icon (app_info);
  if (gicon == NULL)
    gicon = g_themed_icon_new ("application-x-executable");
  else
    g_object_ref (gicon);
  gtk_image_set_from_gicon (self->icon, gicon);

  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self),
                                 g_app_info_get_name (app_info));

  return self;
}

GAppInfo *
cc_search_panel_row_get_app_info (CcSearchPanelRow *self)
{
  return self->app_info;
}

GtkWidget *
cc_search_panel_row_get_switch (CcSearchPanelRow *self)
{
  return GTK_WIDGET (self->switcher);
}
