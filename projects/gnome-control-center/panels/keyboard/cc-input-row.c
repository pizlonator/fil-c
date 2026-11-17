/*
 * Copyright © 2018 Canonical Ltd.
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
 */

#include <config.h>
#include "cc-input-row.h"
#include "cc-input-source-ibus.h"

struct _CcInputRow
{
  AdwActionRow     parent_instance;

  CcInputSource   *source;

  GtkListBox      *drag_widget;

  GtkDragSource   *drag_source;
  gdouble          drag_x;
  gdouble          drag_y;
};

G_DEFINE_TYPE (CcInputRow, cc_input_row, ADW_TYPE_ACTION_ROW)

enum
{
  SIGNAL_SHOW_SETTINGS,
  SIGNAL_SHOW_LAYOUT,
  SIGNAL_MOVE_ROW,
  SIGNAL_REMOVE_ROW,
  SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0, };

static GdkContentProvider *
drag_prepare_cb (CcInputRow    *self,
                 double         x,
                 double         y)
{
  self->drag_x = x;
  self->drag_y = y;

  return gdk_content_provider_new_typed (CC_TYPE_INPUT_ROW, self);
}

static void
drag_begin_cb (CcInputRow    *self,
               GdkDrag       *drag)
{
  GtkAllocation alloc;
  CcInputRow *drag_row;
  GtkWidget *drag_icon;

  gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);

  self->drag_widget = GTK_LIST_BOX (gtk_list_box_new ());
  gtk_widget_set_size_request (GTK_WIDGET (self->drag_widget), alloc.width, alloc.height);

  drag_row = cc_input_row_new (self->source);
  gtk_list_box_append (self->drag_widget, GTK_WIDGET (drag_row));
  gtk_list_box_drag_highlight_row (self->drag_widget, GTK_LIST_BOX_ROW (drag_row));

  drag_icon = gtk_drag_icon_get_for_drag (drag);
  gtk_drag_icon_set_child (GTK_DRAG_ICON (drag_icon), GTK_WIDGET (self->drag_widget));
  gdk_drag_set_hotspot (drag, self->drag_x, self->drag_y);
}

static gboolean
drop_cb (CcInputRow    *self,
         const GValue  *value,
         gdouble        x,
         gdouble        y)
{
  CcInputRow *source;

  if (!G_VALUE_HOLDS (value, CC_TYPE_INPUT_ROW))
    return FALSE;

  source = g_value_get_object (value);

  g_signal_emit (source,
                 signals[SIGNAL_MOVE_ROW],
                 0,
                 self);

  return TRUE;
}

static void
move_up_cb (GtkWidget  *widget,
            const char *action_name,
            GVariant   *parameter)
{
  CcInputRow *self = CC_INPUT_ROW (widget);
  GtkListBox *list_box = GTK_LIST_BOX (gtk_widget_get_parent (GTK_WIDGET (self)));
  gint previous_idx = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (self)) - 1;
  GtkListBoxRow *previous_row = gtk_list_box_get_row_at_index (list_box, previous_idx);

  if (previous_row == NULL || !CC_IS_INPUT_ROW (previous_row))
    {
      gtk_widget_action_set_enabled (widget, "row.move-up", FALSE);
      gtk_widget_action_set_enabled (GTK_WIDGET (gtk_list_box_get_row_at_index (list_box, 0)), "row.move-up", TRUE);
      return;
    }

  g_signal_emit (self,
                 signals[SIGNAL_MOVE_ROW],
                 0,
                 previous_row);
}

static void
move_down_cb (GtkWidget  *widget,
              const char *action_name,
              GVariant   *parameter)
{
  CcInputRow *self = CC_INPUT_ROW (widget);
  GtkListBox *list_box = GTK_LIST_BOX (gtk_widget_get_parent (GTK_WIDGET (self)));
  gint next_idx = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (self)) + 1;
  GtkListBoxRow *next_row = gtk_list_box_get_row_at_index (list_box, next_idx);

  if (next_row == NULL || !CC_IS_INPUT_ROW (next_row))
    {
      gtk_widget_action_set_enabled (widget, "row.move-down", FALSE);
      gtk_widget_action_set_enabled (GTK_WIDGET (gtk_list_box_get_row_at_index (list_box, next_idx-1)), "row.move-down", TRUE);
      return;
    }

  g_signal_emit (next_row,
                 signals[SIGNAL_MOVE_ROW],
                 0,
                 self);
}

static void
show_settings_cb (GtkWidget  *widget,
                  const char *action_name,
                  GVariant   *parameter)
{
  CcInputRow *self = CC_INPUT_ROW (widget);
  g_signal_emit (self,
                 signals[SIGNAL_SHOW_SETTINGS],
                 0);
}

static void
show_layout_cb (GtkWidget  *widget,
                const char *action_name,
                GVariant   *parameter)
{
  CcInputRow *self = CC_INPUT_ROW (widget);
  g_signal_emit (self,
                 signals[SIGNAL_SHOW_LAYOUT],
                 0);
}

static void
remove_cb (GtkWidget  *widget,
           const char *action_name,
           GVariant   *parameter)
{
  CcInputRow *self = CC_INPUT_ROW (widget);
  g_signal_emit (self,
                 signals[SIGNAL_REMOVE_ROW],
                 0);
}

static void
cc_input_row_dispose (GObject *object)
{
  CcInputRow *self = CC_INPUT_ROW (object);

  g_clear_object (&self->source);

  G_OBJECT_CLASS (cc_input_row_parent_class)->dispose (object);
}

void
cc_input_row_class_init (CcInputRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_input_row_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/keyboard/cc-input-row.ui");

  signals[SIGNAL_SHOW_SETTINGS] =
    g_signal_new ("show-settings",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

  signals[SIGNAL_SHOW_LAYOUT] =
    g_signal_new ("show-layout",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

  signals[SIGNAL_MOVE_ROW] =
    g_signal_new ("move-row",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1, CC_TYPE_INPUT_ROW);

  signals[SIGNAL_REMOVE_ROW] =
    g_signal_new ("remove-row",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

  gtk_widget_class_install_action (widget_class, "row.move-up", NULL, move_up_cb);
  gtk_widget_class_install_action (widget_class, "row.move-down", NULL, move_down_cb);
  gtk_widget_class_install_action (widget_class, "row.show-layout", NULL, show_layout_cb);
  gtk_widget_class_install_action (widget_class, "row.show-settings", NULL, show_settings_cb);
  gtk_widget_class_install_action (widget_class, "row.remove", NULL, remove_cb);
}

void
cc_input_row_init (CcInputRow *self)
{
  GtkDropTarget *drop_target;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->drag_source = gtk_drag_source_new ();
  gtk_drag_source_set_actions (self->drag_source, GDK_ACTION_MOVE);
  g_signal_connect_swapped (self->drag_source, "prepare", G_CALLBACK (drag_prepare_cb), self);
  g_signal_connect_swapped (self->drag_source, "drag-begin", G_CALLBACK (drag_begin_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (self->drag_source));

  drop_target = gtk_drop_target_new (CC_TYPE_INPUT_ROW, GDK_ACTION_MOVE);
  gtk_drop_target_set_preload (drop_target, TRUE);
  g_signal_connect_swapped (drop_target, "drop", G_CALLBACK (drop_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (drop_target));
}

static void
label_changed_cb (CcInputRow *self)
{
  g_autofree gchar *label = cc_input_source_get_label (self->source);
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self), label);
}

CcInputRow *
cc_input_row_new (CcInputSource *source)
{
  CcInputRow *self;

  self = g_object_new (CC_TYPE_INPUT_ROW, NULL);
  self->source = g_object_ref (source);

  g_signal_connect_object (source, "label-changed", G_CALLBACK (label_changed_cb), self, G_CONNECT_SWAPPED);
  label_changed_cb (self);

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "row.show-settings", CC_IS_INPUT_SOURCE_IBUS (source));

  return self;
}

CcInputSource *
cc_input_row_get_source (CcInputRow *self)
{
  g_return_val_if_fail (CC_IS_INPUT_ROW (self), NULL);
  return self->source;
}

void
cc_input_row_set_removable (CcInputRow *self,
                            gboolean    removable)
{
  g_return_if_fail (CC_IS_INPUT_ROW (self));
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "row.remove", removable);
}

void
cc_input_row_set_draggable (CcInputRow *self,
                            gboolean    draggable)
{
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (self->drag_source),
                                              draggable ? GTK_PHASE_BUBBLE : GTK_PHASE_NONE);
}

