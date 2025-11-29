/*
 * Copyright (c) 2015 Red Hat, Inc.
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

#include "strv-editor.h"
#include "gtkaccessible.h"
#include "gtkbutton.h"
#include "gtkentry.h"
#include "gtkbox.h"
#include "gtkorientable.h"
#include "gtkmarshalers.h"

enum
{
  CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE (GtkInspectorStrvEditor, gtk_inspector_strv_editor, GTK_TYPE_BOX);

static void
emit_changed (GtkInspectorStrvEditor *editor)
{
  if (editor->blocked)
    return;

  g_signal_emit (editor, signals[CHANGED], 0);
}

static void
remove_string (GtkButton              *button,
               GtkInspectorStrvEditor *editor)
{
  GtkWidget *row;

  row = gtk_widget_get_parent (GTK_WIDGET (button));
  gtk_box_remove (GTK_BOX (gtk_widget_get_parent (row)), row);
  emit_changed (editor);
}

static void
add_string (GtkInspectorStrvEditor *editor,
            const char             *str)
{
  GtkWidget *box;
  GtkWidget *entry;
  GtkWidget *button;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_add_css_class (box, "linked");

  entry = gtk_entry_new ();
  gtk_editable_set_text (GTK_EDITABLE (entry), str);
  gtk_accessible_update_property (GTK_ACCESSIBLE (entry),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, _("Value"),
                                  -1);
  gtk_box_append (GTK_BOX (box), entry);
  g_object_set_data (G_OBJECT (box), "entry", entry);
  g_signal_connect_swapped (entry, "notify::text", G_CALLBACK (emit_changed), editor);

  button = gtk_button_new_from_icon_name ("user-trash-symbolic");
  gtk_widget_add_css_class (button, "image-button");
  gtk_accessible_update_property (GTK_ACCESSIBLE (button),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL,
                                  g_strdup_printf (_("Remove %s"), str),
                                  -1);
  gtk_box_append (GTK_BOX (box), button);
  g_signal_connect (button, "clicked", G_CALLBACK (remove_string), editor);

  gtk_box_append (GTK_BOX (editor->box), box);

  gtk_widget_grab_focus (entry);

  emit_changed (editor);
}

static void
add_cb (GtkButton              *button,
        GtkInspectorStrvEditor *editor)
{
  add_string (editor, "");
}

static void
gtk_inspector_strv_editor_init (GtkInspectorStrvEditor *editor)
{
  gtk_box_set_spacing (GTK_BOX (editor), 6);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (editor), GTK_ORIENTATION_VERTICAL);
  editor->box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_set_visible (editor->box, TRUE);

  editor->button = gtk_button_new_from_icon_name ("list-add-symbolic");
  gtk_widget_add_css_class (editor->button, "image-button");
  gtk_widget_set_focus_on_click (editor->button, FALSE);
  gtk_widget_set_halign (editor->button, GTK_ALIGN_END);
  gtk_accessible_update_property (GTK_ACCESSIBLE (editor->button),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, _("Add"),
                                  -1);
  g_signal_connect (editor->button, "clicked", G_CALLBACK (add_cb), editor);

  gtk_box_append (GTK_BOX (editor), editor->box);
  gtk_box_append (GTK_BOX (editor), editor->button);
}

static void
gtk_inspector_strv_editor_class_init (GtkInspectorStrvEditorClass *class)
{
  signals[CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkInspectorStrvEditorClass, changed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
}

void
gtk_inspector_strv_editor_set_strv (GtkInspectorStrvEditor  *editor,
                                    char                   **strv)
{
  GtkWidget *child;
  int i;

  editor->blocked = TRUE;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (editor->box))))
    gtk_box_remove (GTK_BOX (editor->box), child);

  if (strv)
    {
      for (i = 0; strv[i]; i++)
        add_string (editor, strv[i]);
    }

  editor->blocked = FALSE;

  emit_changed (editor);
}

char **
gtk_inspector_strv_editor_get_strv (GtkInspectorStrvEditor *editor)
{
  GtkWidget *child;
  GPtrArray *p;

  p = g_ptr_array_new ();

  for (child = gtk_widget_get_first_child (editor->box);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      GtkEntry *entry;

      entry = GTK_ENTRY (g_object_get_data (G_OBJECT (child), "entry"));
      g_ptr_array_add (p, g_strdup (gtk_editable_get_text (GTK_EDITABLE (entry))));
    }

  g_ptr_array_add (p, NULL);

  return (char **)g_ptr_array_free (p, FALSE);
}
