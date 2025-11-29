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

#include "variant-editor.h"

#include "gtksizegroup.h"
#include "gtkcheckbutton.h"
#include "gtkentry.h"
#include "gtklabel.h"
#include "gtkbox.h"
#include "gtkbinlayout.h"


struct _GtkInspectorVariantEditor
{
  GtkWidget parent;

  const GVariantType *type;

  GtkWidget *editor;
  GtkInspectorVariantEditorChanged callback;
  gpointer   data;
};

typedef struct
{
  GtkWidgetClass parent;
} GtkInspectorVariantEditorClass;

static void
variant_editor_changed_cb (GObject                   *obj,
                           GParamSpec                *pspec,
                           GtkInspectorVariantEditor *self)
{
  self->callback (GTK_WIDGET (self), self->data);
}

G_DEFINE_TYPE (GtkInspectorVariantEditor, gtk_inspector_variant_editor, GTK_TYPE_WIDGET)

static void
gtk_inspector_variant_editor_init (GtkInspectorVariantEditor *editor)
{
}


static void
dispose (GObject *object)
{
  GtkInspectorVariantEditor *self = GTK_INSPECTOR_VARIANT_EDITOR (object);

  if (self->editor)
   {
      g_signal_handlers_disconnect_by_func (self->editor, variant_editor_changed_cb, self->data);

      gtk_widget_unparent (self->editor);
      self->editor = NULL;
    }

  G_OBJECT_CLASS (gtk_inspector_variant_editor_parent_class)->dispose (object);
}

static void
gtk_inspector_variant_editor_class_init (GtkInspectorVariantEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = dispose;

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

static void
ensure_editor (GtkInspectorVariantEditor *self,
               const GVariantType        *type)
{
  if (self->type &&
      g_variant_type_equal (self->type, type))
    return;

  self->type = type;

  if (g_variant_type_equal (type, G_VARIANT_TYPE_BOOLEAN))
    {
      if (self->editor)
        gtk_widget_unparent (self->editor);

      self->editor = gtk_check_button_new ();
      g_signal_connect (self->editor, "notify::active",
                        G_CALLBACK (variant_editor_changed_cb), self);

      gtk_widget_set_parent (self->editor, GTK_WIDGET (self));
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_STRING))
    {
      if (self->editor)
        gtk_widget_unparent (self->editor);

      self->editor = gtk_entry_new ();
      gtk_editable_set_width_chars (GTK_EDITABLE (self->editor), 10);
      g_signal_connect (self->editor, "notify::text",
                        G_CALLBACK (variant_editor_changed_cb), self);

      gtk_widget_set_parent (self->editor, GTK_WIDGET (self));
    }
  else if (!GTK_IS_BOX (self->editor))
    {
      GtkWidget *entry, *label;

      if (self->editor)
        gtk_widget_unparent (self->editor);

      self->editor = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      entry = gtk_entry_new ();
      gtk_editable_set_width_chars (GTK_EDITABLE (entry), 10);
      gtk_box_append (GTK_BOX (self->editor), entry);
      label = gtk_label_new (g_variant_type_peek_string (type));
      gtk_box_append (GTK_BOX (self->editor), label);
      g_signal_connect (entry, "notify::text",
                        G_CALLBACK (variant_editor_changed_cb), self);

      gtk_widget_set_parent (self->editor, GTK_WIDGET (self));
    }
}

GtkWidget *
gtk_inspector_variant_editor_new (const GVariantType               *type,
                                  GtkInspectorVariantEditorChanged  callback,
                                  gpointer                          data)
{
  GtkInspectorVariantEditor *self;

  self = g_object_new (GTK_TYPE_INSPECTOR_VARIANT_EDITOR, NULL);

  self->callback = callback;
  self->data = data;

  if (type)
    ensure_editor (self, type);

  return GTK_WIDGET (self);
}

void
gtk_inspector_variant_editor_set_type (GtkWidget          *editor,
                                       const GVariantType *type)
{
  GtkInspectorVariantEditor *self = GTK_INSPECTOR_VARIANT_EDITOR (editor);

  ensure_editor (self, type);
}

void
gtk_inspector_variant_editor_set_value (GtkWidget *editor,
                                        GVariant  *value)
{
  GtkInspectorVariantEditor *self = GTK_INSPECTOR_VARIANT_EDITOR (editor);

  ensure_editor (self, g_variant_get_type (value));

  g_signal_handlers_block_by_func (self->editor, variant_editor_changed_cb, self);

  if (g_variant_type_equal (self->type, G_VARIANT_TYPE_BOOLEAN))
    {
      GtkCheckButton *b = GTK_CHECK_BUTTON (self->editor);

      if (gtk_check_button_get_active (b) != g_variant_get_boolean (value))
        gtk_check_button_set_active (b, g_variant_get_boolean (value));
    }
  else if (g_variant_type_equal (self->type, G_VARIANT_TYPE_STRING))
    {
      GtkEntry *entry = GTK_ENTRY (self->editor);

      gtk_editable_set_text (GTK_EDITABLE (entry),
                             g_variant_get_string (value, NULL));
    }
  else
    {
      GtkWidget *entry;
      char *text;

      entry = gtk_widget_get_first_child (self->editor);

      text = g_variant_print (value, FALSE);
      gtk_editable_set_text (GTK_EDITABLE (entry), text);
      g_free (text);
    }

  g_signal_handlers_unblock_by_func (self->editor, variant_editor_changed_cb, self);
}

GVariant *
gtk_inspector_variant_editor_get_value (GtkWidget *editor)
{
  GtkInspectorVariantEditor *self = GTK_INSPECTOR_VARIANT_EDITOR (editor);
  GVariant *value;

  if (self->type == NULL)
    return NULL;

  if (g_variant_type_equal (self->type, G_VARIANT_TYPE_BOOLEAN))
    {
      GtkCheckButton *b = GTK_CHECK_BUTTON (self->editor);
      value = g_variant_new_boolean (gtk_check_button_get_active (b));
    }
  else if (g_variant_type_equal (self->type, G_VARIANT_TYPE_STRING))
    {
      GtkEntry *entry = GTK_ENTRY (self->editor);
      value = g_variant_new_string (gtk_editable_get_text (GTK_EDITABLE (entry)));
    }
  else
    {
      GtkWidget *entry;
      const char *text;

      entry = gtk_widget_get_first_child (self->editor);
      text = gtk_editable_get_text (GTK_EDITABLE (entry));

      value = g_variant_parse (self->type, text, NULL, NULL, NULL);
    }

  return value;
}
