/*
 * Copyright (c) 2014 Red Hat, Inc.
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

#include "config.h"
#include <glib/gi18n-lib.h>

#include "size-groups.h"

#include "highlightoverlay.h"
#include "window.h"

#include "gtkbinlayout.h"
#include "gtkbox.h"
#include "gtkdropdown.h"
#include "gtkframe.h"
#include "gtklabel.h"
#include "gtklistbox.h"
#include "gtksizegroup.h"
#include "gtkswitch.h"
#include "gtkwidgetprivate.h"
#include "gtkstack.h"
#include "gtkstringlist.h"
#include "gtkscrolledwindow.h"


typedef struct {
  GtkListBoxRow parent;
  GtkInspectorOverlay *highlight;
  GtkWidget *widget;
} SizeGroupRow;

typedef struct {
  GtkListBoxRowClass parent;
} SizeGroupRowClass;

enum {
  PROP_WIDGET = 1,
  LAST_PROPERTY
};

GParamSpec *properties[LAST_PROPERTY] = { NULL };

GType size_group_row_get_type (void);

G_DEFINE_TYPE (SizeGroupRow, size_group_row, GTK_TYPE_LIST_BOX_ROW)

static void
size_group_row_init (SizeGroupRow *row)
{
}

static void
size_group_row_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  SizeGroupRow *row = (SizeGroupRow*)object;

  switch (property_id)
    {
    case PROP_WIDGET:
      g_value_set_pointer (value, row->widget);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
size_group_row_widget_destroyed (GtkWidget *widget, SizeGroupRow *row)
{
  GtkWidget *parent;

  parent = gtk_widget_get_parent (GTK_WIDGET (row));
  if (parent)
    gtk_box_remove (GTK_BOX (parent), GTK_WIDGET (row));
}

static void
set_widget (SizeGroupRow *row, GtkWidget *widget)
{
  if (row->widget)
    g_signal_handlers_disconnect_by_func (row->widget,
                                          size_group_row_widget_destroyed, row);

  row->widget = widget;

  if (row->widget)
    g_signal_connect (row->widget, "destroy",
                      G_CALLBACK (size_group_row_widget_destroyed), row);
}

static void
size_group_row_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  SizeGroupRow *row = (SizeGroupRow*)object;

  switch (property_id)
    {
    case PROP_WIDGET:
      set_widget (row, g_value_get_pointer (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}
static void
size_group_row_finalize (GObject *object)
{
  SizeGroupRow *row = (SizeGroupRow *)object;

  set_widget (row, NULL);

  G_OBJECT_CLASS (size_group_row_parent_class)->finalize (object);
}

static void
size_group_state_flags_changed (GtkWidget     *widget,
                                GtkStateFlags  old_state)
{
  SizeGroupRow *row = (SizeGroupRow*)widget;
  GtkStateFlags state;

  if (!row->widget)
    return;

  state = gtk_widget_get_state_flags (widget);
  if ((state & GTK_STATE_FLAG_PRELIGHT) != (old_state & GTK_STATE_FLAG_PRELIGHT))
    {
      GtkInspectorWindow *iw = GTK_INSPECTOR_WINDOW (gtk_widget_get_root (widget));

      if (state & GTK_STATE_FLAG_PRELIGHT)
        {
          row->highlight = gtk_highlight_overlay_new (row->widget);
          gtk_inspector_window_add_overlay (iw, row->highlight);
        }
      else
        {
          gtk_inspector_window_remove_overlay (iw, row->highlight);
          g_clear_object (&row->highlight);
        }
    }
}

static void
size_group_row_class_init (SizeGroupRowClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->finalize = size_group_row_finalize;
  object_class->get_property = size_group_row_get_property;
  object_class->set_property = size_group_row_set_property;

  widget_class->state_flags_changed = size_group_state_flags_changed;

  properties[PROP_WIDGET] =
    g_param_spec_pointer ("widget", NULL, NULL, G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROPERTY, properties);

}

G_DEFINE_TYPE (GtkInspectorSizeGroups, gtk_inspector_size_groups, GTK_TYPE_WIDGET)

static void
clear_view (GtkInspectorSizeGroups *sl)
{
  GtkWidget *child;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (sl->groups))))
    gtk_box_remove (GTK_BOX (sl->groups), child);
}

static void
add_widget (GtkInspectorSizeGroups *sl,
            GtkListBox             *listbox,
            GtkWidget              *widget)
{
  GtkWidget *row;
  GtkWidget *label;
  char *text;

  row = g_object_new (size_group_row_get_type (), "widget", widget, NULL);
  text = g_strdup_printf ("%p (%s)", widget, g_type_name_from_instance ((GTypeInstance*)widget));
  label = gtk_label_new (text);
  g_free (text);
  gtk_widget_set_margin_start (label, 10);
  gtk_widget_set_margin_end (label, 10);
  gtk_widget_set_margin_top (label, 10);
  gtk_widget_set_margin_bottom (label, 10);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_BASELINE_FILL);
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), label);
  gtk_list_box_insert (listbox, row, -1);
}

static void
add_size_group (GtkInspectorSizeGroups *sl,
                GtkSizeGroup           *group)
{
  GtkWidget *frame, *box, *box2;
  GtkWidget *label, *dropdown;
  GSList *widgets, *l;
  GtkWidget *listbox;
  const char *modes[5];

  modes[0] = C_("sizegroup mode", "None");
  modes[1] = C_("sizegroup mode", "Horizontal");
  modes[2] = C_("sizegroup mode", "Vertical");
  modes[3] = C_("sizegroup mode", "Both");
  modes[4] = NULL;

  frame = gtk_frame_new (NULL);
  gtk_box_append (GTK_BOX (sl->groups), frame);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_add_css_class (box, "view");
  gtk_frame_set_child (GTK_FRAME (frame), box);

  box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_box_append (GTK_BOX (box), box2);

  label = gtk_label_new (_("Mode"));
  gtk_widget_set_margin_start (label, 10);
  gtk_widget_set_margin_end (label, 10);
  gtk_widget_set_margin_top (label, 10);
  gtk_widget_set_margin_bottom (label, 10);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_BASELINE_FILL);
  gtk_box_append (GTK_BOX (box2), label);

  dropdown = gtk_drop_down_new_from_strings (modes);
  g_object_set (dropdown,
                "margin-start", 10,
                "margin-end", 10,
                "margin-top", 10,
                "margin-bottom", 10,
                NULL);
  gtk_widget_set_halign (dropdown, GTK_ALIGN_END);
  gtk_widget_set_valign (dropdown, GTK_ALIGN_BASELINE_FILL);
  g_object_bind_property (group, "mode",
                          dropdown, "selected",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_box_append (GTK_BOX (box2), dropdown);

  listbox = gtk_list_box_new ();
  gtk_box_append (GTK_BOX (box), listbox);
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (listbox), GTK_SELECTION_NONE);

  widgets = gtk_size_group_get_widgets (group);
  for (l = widgets; l; l = l->next)
    add_widget (sl, GTK_LIST_BOX (listbox), GTK_WIDGET (l->data));
}

void
gtk_inspector_size_groups_set_object (GtkInspectorSizeGroups *sl,
                                      GObject                *object)
{
  GSList *groups, *l;
  GtkWidget *stack;
  GtkStackPage *page;

  stack = gtk_widget_get_parent (GTK_WIDGET (sl));
  page = gtk_stack_get_page (GTK_STACK (stack), GTK_WIDGET (sl));

  g_object_set (page, "visible", FALSE, NULL);

  clear_view (sl);

  if (!GTK_IS_WIDGET (object))
    return;

  groups = _gtk_widget_get_sizegroups (GTK_WIDGET (object));
  if (groups)
    g_object_set (page, "visible", TRUE, NULL);
  for (l = groups; l; l = l->next)
    {
      GtkSizeGroup *group = l->data;
      add_size_group (sl, group);
    }
}

static void
gtk_inspector_size_groups_init (GtkInspectorSizeGroups *sl)
{
  sl->sw = gtk_scrolled_window_new ();
  gtk_widget_set_parent (sl->sw, GTK_WIDGET (sl));
  sl->groups = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  g_object_set (sl->groups,
                "margin-start", 60,
                "margin-end", 60,
                "margin-top", 30,
                "margin-bottom", 30,
                NULL);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sl->sw), sl->groups);
}

static void
gtk_inspector_size_groups_dispose (GObject *object)
{
  GtkInspectorSizeGroups *sl = GTK_INSPECTOR_SIZE_GROUPS (object);

  g_clear_pointer (&sl->sw, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_inspector_size_groups_parent_class)->dispose (object);
}

static void
gtk_inspector_size_groups_class_init (GtkInspectorSizeGroupsClass *klass)
{
  G_OBJECT_CLASS (klass)->dispose = gtk_inspector_size_groups_dispose;

  gtk_widget_class_set_layout_manager_type (GTK_WIDGET_CLASS (klass), GTK_TYPE_BIN_LAYOUT);
}

// vim: set et sw=2 ts=2:
