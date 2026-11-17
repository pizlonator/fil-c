/*
 * Copyright 2020 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "cc-cursor-size-dialog.h"

#define INTERFACE_SETTINGS           "org.gnome.desktop.interface"
#define KEY_MOUSE_CURSOR_SIZE        "cursor-size"

struct _CcCursorSizeDialog
{
  AdwWindow parent;

  GtkBox *cursor_box;

  GSettings *interface_settings;
};

G_DEFINE_TYPE (CcCursorSizeDialog, cc_cursor_size_dialog, ADW_TYPE_WINDOW);

static void
cursor_size_toggled (CcCursorSizeDialog *self, GtkWidget *button)
{
  guint cursor_size;

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    return;

  cursor_size = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (button), "cursor-size"));
  g_settings_set_int (self->interface_settings, KEY_MOUSE_CURSOR_SIZE, cursor_size);
  g_debug ("Setting cursor size to %d", cursor_size);
}

static void
cc_cursor_size_dialog_dispose (GObject *object)
{
  CcCursorSizeDialog *self = CC_CURSOR_SIZE_DIALOG (object);

  g_clear_object (&self->interface_settings);

  G_OBJECT_CLASS (cc_cursor_size_dialog_parent_class)->dispose (object);
}

static void
cc_cursor_size_dialog_class_init (CcCursorSizeDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_cursor_size_dialog_dispose;

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/universal-access/cc-cursor-size-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcCursorSizeDialog, cursor_box);
}

static void
cc_cursor_size_dialog_init (CcCursorSizeDialog *self)
{
  guint cursor_sizes[] = { 24, 32, 48, 64, 96 };
  guint current_cursor_size, i;
  GtkWidget *last_radio_button = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->interface_settings = g_settings_new (INTERFACE_SETTINGS);

  current_cursor_size = g_settings_get_int (self->interface_settings,
                                            KEY_MOUSE_CURSOR_SIZE);

  for (i = 0; i < G_N_ELEMENTS(cursor_sizes); i++)
    {
      GtkWidget *image, *button;
      g_autofree gchar *cursor_image_name = NULL;

      cursor_image_name = g_strdup_printf ("/org/gnome/control-center/universal-access/left_ptr_%dpx.png", cursor_sizes[i]);
      image = gtk_picture_new_for_resource (cursor_image_name);
      gtk_picture_set_content_fit (GTK_PICTURE (image), GTK_CONTENT_FIT_SCALE_DOWN);

      button = gtk_toggle_button_new ();
      gtk_toggle_button_set_group (GTK_TOGGLE_BUTTON (button), GTK_TOGGLE_BUTTON (last_radio_button));
      last_radio_button = button;
      g_object_set_data (G_OBJECT (button), "cursor-size", GUINT_TO_POINTER (cursor_sizes[i]));

      gtk_button_set_child (GTK_BUTTON (button), image);
      gtk_box_append (self->cursor_box, button);

      g_signal_connect_object (button, "toggled",
                               G_CALLBACK (cursor_size_toggled), self, G_CONNECT_SWAPPED);

      if (current_cursor_size == cursor_sizes[i])
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
    }
}

CcCursorSizeDialog *
cc_cursor_size_dialog_new (void)
{
  return g_object_new (cc_cursor_size_dialog_get_type (), NULL);
}
