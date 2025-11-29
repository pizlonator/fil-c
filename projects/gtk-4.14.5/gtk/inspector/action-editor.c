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

#include "action-editor.h"
#include "variant-editor.h"

#include "gtktogglebutton.h"
#include "gtkentry.h"
#include "gtklabel.h"
#include "gtkbox.h"
#include "gtkboxlayout.h"
#include "gtkorientable.h"
#include "gtkactionmuxerprivate.h"

struct _GtkInspectorActionEditor
{
  GtkWidget parent;

  GObject *owner;
  char *name;
  gboolean enabled;
  const GVariantType *parameter_type;
  GVariantType *state_type;
  GVariant *state;
  GtkWidget *activate_button;
  GtkWidget *parameter_entry;
  GtkWidget *state_entry;
  GtkWidget *state_editor;
};

typedef struct
{
  GtkWidgetClass parent;
} GtkInspectorActionEditorClass;

enum
{
  PROP_0,
  PROP_OWNER,
  PROP_NAME
};

G_DEFINE_TYPE (GtkInspectorActionEditor, gtk_inspector_action_editor, GTK_TYPE_WIDGET)

static void update_widgets (GtkInspectorActionEditor *r);

static void
activate_action (GtkWidget                *button,
                 GtkInspectorActionEditor *r)
{
  GVariant *parameter = NULL;

  if (r->parameter_entry)
    parameter = gtk_inspector_variant_editor_get_value (r->parameter_entry);
  if (G_IS_ACTION_GROUP (r->owner))
    g_action_group_activate_action (G_ACTION_GROUP (r->owner), r->name, parameter);
  else if (GTK_IS_ACTION_MUXER (r->owner))
    gtk_action_muxer_activate_action (GTK_ACTION_MUXER (r->owner), r->name, parameter);

  update_widgets (r);
}

static void
parameter_changed (GtkWidget *editor,
                   gpointer   data)
{
  GtkInspectorActionEditor *r = data;
  GVariant *value;

  value = gtk_inspector_variant_editor_get_value (editor);
  gtk_widget_set_sensitive (r->activate_button, r->enabled && value != NULL);
  if (value)
    g_variant_unref (value);
}

static void
state_changed (GtkWidget *editor,
               gpointer   data)
{
  GtkInspectorActionEditor *r = data;
  GVariant *value;

  value = gtk_inspector_variant_editor_get_value (editor);
  if (!value)
    return;

  g_variant_ref_sink (value);
  if (g_variant_equal (value, r->state))
    {
      g_variant_unref (value);
      return;
    }

  if (G_IS_ACTION_GROUP (r->owner))
    g_action_group_change_action_state (G_ACTION_GROUP (r->owner), r->name, value);
  else if (GTK_IS_ACTION_MUXER (r->owner))
    gtk_action_muxer_change_action_state (GTK_ACTION_MUXER (r->owner), r->name, value);

  g_variant_unref (value);
}

static void
gtk_inspector_action_editor_init (GtkInspectorActionEditor *r)
{
  GtkBoxLayout *layout;
  GtkWidget *row, *activate, *label;

  layout = GTK_BOX_LAYOUT (gtk_widget_get_layout_manager (GTK_WIDGET (r)));
  gtk_orientable_set_orientation (GTK_ORIENTABLE (layout), GTK_ORIENTATION_HORIZONTAL);
  gtk_box_layout_set_spacing (layout, 10);

  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  activate = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_box_append (GTK_BOX (row), activate);

  r->activate_button = gtk_button_new_with_label (_("Activate"));
  g_signal_connect (r->activate_button, "clicked", G_CALLBACK (activate_action), r);

  gtk_box_append (GTK_BOX (activate), r->activate_button);

  r->parameter_entry = gtk_inspector_variant_editor_new (NULL, parameter_changed, r);
  gtk_widget_set_visible (r->parameter_entry, FALSE);
  gtk_box_append (GTK_BOX (activate), r->parameter_entry);

  gtk_widget_set_parent (row, GTK_WIDGET (r));

  r->state_editor = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  label = gtk_label_new (_("Set State"));
  gtk_box_append (GTK_BOX (r->state_editor), label);
  r->state_entry = gtk_inspector_variant_editor_new (NULL, state_changed, r);
  gtk_box_append (GTK_BOX (r->state_editor), r->state_entry);
  gtk_widget_set_parent (r->state_editor, GTK_WIDGET (r));
  gtk_widget_set_visible (r->state_editor, FALSE);
}

static void
update_widgets (GtkInspectorActionEditor *r)
{
  g_clear_pointer (&r->state, g_variant_unref);

  if (G_IS_ACTION_GROUP (r->owner))
    {
      if (!g_action_group_query_action (G_ACTION_GROUP (r->owner), r->name,
                                        &r->enabled, &r->parameter_type, NULL, NULL,
                                        &r->state))
        {
          r->enabled = FALSE;
          r->parameter_type = NULL;
          r->state = NULL;
        }
    }
  else if (GTK_IS_ACTION_MUXER (r->owner))
    {
      if (!gtk_action_muxer_query_action (GTK_ACTION_MUXER (r->owner), r->name,
                                          &r->enabled, &r->parameter_type, NULL, NULL,
                                          &r->state))
        {
          r->enabled = FALSE;
          r->parameter_type = NULL;
          r->state = NULL;
        }
    }
  else
    {
      r->enabled = FALSE;
      r->parameter_type = NULL;
      r->state = NULL;
    }

  gtk_widget_set_sensitive (r->activate_button, r->enabled);
  gtk_widget_set_sensitive (r->parameter_entry, r->enabled);
  gtk_widget_set_visible (r->parameter_entry, r->parameter_type != NULL);
  if (r->parameter_type)
    gtk_inspector_variant_editor_set_type (r->parameter_entry, r->parameter_type);

  gtk_widget_set_visible (r->state_editor, r->state != NULL);
  if (r->state)
    {
      if (r->state_type)
        g_variant_type_free (r->state_type);
      r->state_type = g_variant_type_copy (g_variant_get_type (r->state));
      gtk_inspector_variant_editor_set_value (r->state_entry, r->state);
    }
}

static void
dispose (GObject *object)
{
  GtkInspectorActionEditor *r = GTK_INSPECTOR_ACTION_EDITOR (object);
  GtkWidget *child;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (r))))
    gtk_widget_unparent (child);

  g_clear_pointer (&r->name, g_free);
  g_clear_pointer (&r->state_type, g_variant_type_free);
  g_clear_pointer (&r->state, g_variant_unref);

  G_OBJECT_CLASS (gtk_inspector_action_editor_parent_class)->dispose (object);
}

static void
get_property (GObject    *object,
              guint       param_id,
              GValue     *value,
              GParamSpec *pspec)
{
  GtkInspectorActionEditor *r = GTK_INSPECTOR_ACTION_EDITOR (object);

  switch (param_id)
    {
    case PROP_OWNER:
      g_value_set_object (value, r->owner);
      break;

    case PROP_NAME:
      g_value_set_string (value, r->name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
set_property (GObject      *object,
              guint         param_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  GtkInspectorActionEditor *r = GTK_INSPECTOR_ACTION_EDITOR (object);

  switch (param_id)
    {
    case PROP_OWNER:
      r->owner = g_value_get_object (value);
      g_assert (r->owner == NULL ||
                G_IS_ACTION_GROUP (r->owner) ||
                GTK_IS_ACTION_MUXER (r->owner));
      break;

    case PROP_NAME:
      g_free (r->name);
      r->name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
      break;
    }
}

static void
gtk_inspector_action_editor_class_init (GtkInspectorActionEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = dispose;
  object_class->get_property = get_property;
  object_class->set_property = set_property;

  g_object_class_install_property (object_class, PROP_OWNER,
      g_param_spec_object ("owner", NULL, NULL,
                           G_TYPE_OBJECT, G_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_NAME,
      g_param_spec_string ("name", NULL, NULL,
                           NULL, G_PARAM_READWRITE));

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
}

GtkWidget *
gtk_inspector_action_editor_new (void)
{
  return g_object_new (GTK_TYPE_INSPECTOR_ACTION_EDITOR, NULL);
}

void
gtk_inspector_action_editor_set (GtkInspectorActionEditor *self,
                                 GObject                  *owner,
                                 const char               *name)
{
  g_object_set (self, "owner", owner, "name", name, NULL);
  update_widgets (self);
}

void
gtk_inspector_action_editor_update (GtkInspectorActionEditor *self)
{
  update_widgets (self);
}
