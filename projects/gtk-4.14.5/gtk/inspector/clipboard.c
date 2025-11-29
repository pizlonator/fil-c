/*
 * Copyright (c) 2021 Benjamin Otte
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

#include "clipboard.h"
#include "gtkdataviewer.h"
#include "window.h"

#include "gtkbinlayout.h"
#include "gtkbox.h"
#include "gtkdebug.h"
#include "gtkdropcontrollermotion.h"
#include "gtklabel.h"
#include "gtklistbox.h"
#include "gtktogglebutton.h"

struct _GtkInspectorClipboard
{
  GtkWidget parent;

  GdkDisplay *display;

  GtkWidget *swin;

  GtkWidget *dnd_formats;
  GtkWidget *dnd_info;

  GtkWidget *clipboard_formats;
  GtkWidget *clipboard_info;

  GtkWidget *primary_formats;
  GtkWidget *primary_info;
};

typedef struct _GtkInspectorClipboardClass
{
  GtkWidgetClass parent_class;
} GtkInspectorClipboardClass;

G_DEFINE_TYPE (GtkInspectorClipboard, gtk_inspector_clipboard, GTK_TYPE_WIDGET)

static void
load_gtype_value (GObject      *source,
                  GAsyncResult *res,
                  gpointer      data)
{
  GtkDataViewer *viewer = data;
  const GValue *value;
  GError *error = NULL;

  if (GDK_IS_CLIPBOARD (source))
    value = gdk_clipboard_read_value_finish (GDK_CLIPBOARD (source), res, &error);
  else if (GDK_IS_DROP (source))
    value = gdk_drop_read_value_finish (GDK_DROP (source), res, &error);
  else
    g_assert_not_reached ();

  if (value == NULL)
    gtk_data_viewer_load_error (viewer, error);
  else
    gtk_data_viewer_load_value (viewer, value);

  g_object_unref (viewer);
}

static gboolean
load_gtype (GtkDataViewer *viewer,
            GCancellable  *cancellable,
            gpointer       gtype)
{
  GObject *data_source = g_object_get_data (G_OBJECT (viewer), "data-source");

  if (GDK_IS_CLIPBOARD (data_source))
    {
      gdk_clipboard_read_value_async (GDK_CLIPBOARD (data_source),
                                      GPOINTER_TO_SIZE (gtype),
                                      G_PRIORITY_DEFAULT,
                                      cancellable,
                                      load_gtype_value,
                                      g_object_ref (viewer));
    }
  else if (GDK_IS_DROP (data_source))
    {
      gdk_drop_read_value_async (GDK_DROP (data_source),
                                 GPOINTER_TO_SIZE (gtype),
                                 G_PRIORITY_DEFAULT,
                                 cancellable,
                                 load_gtype_value,
                                 g_object_ref (viewer));
    }
  else
    {
      g_assert_not_reached ();
    }

  return TRUE;
}

static void
load_mime_type_stream (GObject      *source,
                       GAsyncResult *res,
                       gpointer      data)
{
  GtkDataViewer *viewer = data;
  GInputStream *stream;
  GError *error = NULL;
  const char *mime_type;

  if (GDK_IS_CLIPBOARD (source))
    stream = gdk_clipboard_read_finish (GDK_CLIPBOARD (source), res, &mime_type, &error);
  else if (GDK_IS_DROP (source))
    stream = gdk_drop_read_finish (GDK_DROP (source), res, &mime_type, &error);
  else
    g_assert_not_reached ();

  if (stream == NULL)
    gtk_data_viewer_load_error (viewer, error);
  else
    gtk_data_viewer_load_stream (viewer, stream, mime_type);

  g_object_unref (viewer);
}

static gboolean
load_mime_type (GtkDataViewer *viewer,
                GCancellable  *cancellable,
                gpointer       mime_type)
{
  GObject *data_source = g_object_get_data (G_OBJECT (viewer), "data-source");

  if (GDK_IS_CLIPBOARD (data_source))
    {
      gdk_clipboard_read_async (GDK_CLIPBOARD (data_source),
                                (const char *[2]) { mime_type, NULL },
                                G_PRIORITY_DEFAULT,
                                cancellable,
                                load_mime_type_stream,
                                g_object_ref (viewer));
    }
  else if (GDK_IS_DROP (data_source))
    {
      gdk_drop_read_async (GDK_DROP (data_source),
                           (const char *[2]) { mime_type, NULL },
                           G_PRIORITY_DEFAULT,
                           cancellable,
                           load_mime_type_stream,
                           g_object_ref (viewer));
    }
  else
    {
      g_assert_not_reached ();
    }

  return TRUE;
}

static void
on_drop_row_enter (GtkDropControllerMotion *motion,
                   double                   x,
                   double                   y,
                   GtkWidget               *viewer)
{
  gtk_widget_set_visible (viewer, TRUE);
}

static void
add_content_type_row (GtkInspectorClipboard *self,
                      GtkListBox            *list,
                      const char            *type_name,
                      GObject               *data_source,
                      GCallback              load_func,
                      gpointer               load_func_data)
{
  GtkWidget *row, *vbox, *hbox, *label, *viewer, *button;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 40);
  gtk_box_append (GTK_BOX (vbox), hbox);

  label = gtk_label_new (type_name);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_BASELINE_FILL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_box_append (GTK_BOX (hbox), label);

  viewer = gtk_data_viewer_new ();
  g_signal_connect (viewer, "load", load_func, load_func_data);
  g_object_set_data (G_OBJECT (viewer), "data-source", data_source);
  gtk_box_append (GTK_BOX (vbox), viewer);

  if (GDK_IS_CLIPBOARD (data_source))
    {
      button = gtk_toggle_button_new_with_label (_("Show"));
      gtk_widget_set_halign (button, GTK_ALIGN_END);
      gtk_widget_set_valign (button, GTK_ALIGN_BASELINE_FILL);
      gtk_box_append (GTK_BOX (hbox), button);

      g_object_bind_property (G_OBJECT (button), "active",
                              G_OBJECT (viewer), "visible",
                              G_BINDING_SYNC_CREATE);
    }
  else
    {
      GtkEventController *controller = gtk_drop_controller_motion_new ();
      g_signal_connect (controller, "enter", G_CALLBACK (on_drop_row_enter), viewer);
      gtk_widget_add_controller (vbox, controller);

      gtk_widget_set_visible (viewer, FALSE);

      label = gtk_label_new (_("Hover to load"));
      g_object_bind_property (G_OBJECT (viewer), "visible",
                              G_OBJECT (label), "visible",
                              G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
      gtk_widget_set_halign (label, GTK_ALIGN_END);
      gtk_widget_set_valign (label, GTK_ALIGN_BASELINE_FILL);
      gtk_box_append (GTK_BOX (hbox), label);
    }

  row = gtk_list_box_row_new ();
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), vbox);
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);

  gtk_list_box_insert (list, row, -1);
}

static void
init_formats (GtkInspectorClipboard *self,
              GtkListBox            *list,
              GdkContentFormats     *formats,
              GObject               *data_source)
{
  GtkListBoxRow *row;
  const char * const *mime_types;
  const GType *gtypes;
  gsize i, n;

  while ((row = gtk_list_box_get_row_at_index (list, 1)))
    gtk_list_box_remove (list, GTK_WIDGET (row));

  gtypes = gdk_content_formats_get_gtypes (formats, &n);
  for (i = 0; i < n; i++)
    add_content_type_row (self, list, g_type_name (gtypes[i]), data_source, G_CALLBACK (load_gtype), GSIZE_TO_POINTER (gtypes[i]));

  mime_types = gdk_content_formats_get_mime_types (formats, &n);
  for (i = 0; i < n; i++)
    add_content_type_row (self, list, mime_types[i], data_source, G_CALLBACK (load_mime_type), (gpointer) mime_types[i]);
}

static void
init_info (GtkInspectorClipboard *self,
           GtkLabel              *label,
           GdkClipboard          *clipboard)
{
  GdkContentFormats *formats;

  formats = gdk_clipboard_get_formats (clipboard);
  if (gdk_content_formats_get_gtypes (formats, NULL) == NULL &&
      gdk_content_formats_get_mime_types (formats, NULL) == NULL)
    {
      gtk_label_set_text (label, C_("clipboard", "empty"));
      return;
    }

  if (gdk_clipboard_is_local (clipboard))
    gtk_label_set_text (label, C_("clipboard", "local"));
  else
    gtk_label_set_text (label, C_("clipboard", "remote"));
}

static void
clipboard_notify (GdkClipboard          *clipboard,
                  GParamSpec            *pspec,
                  GtkInspectorClipboard *self)
{
  if (g_str_equal (pspec->name, "formats"))
    {
      init_formats (self, GTK_LIST_BOX (self->clipboard_formats), gdk_clipboard_get_formats (clipboard), G_OBJECT (clipboard));
    }

  init_info (self, GTK_LABEL (self->clipboard_info), clipboard);
}

static void
primary_notify (GdkClipboard          *clipboard,
                GParamSpec            *pspec,
                GtkInspectorClipboard *self)
{
  if (g_str_equal (pspec->name, "formats"))
    {
      init_formats (self, GTK_LIST_BOX (self->primary_formats), gdk_clipboard_get_formats (clipboard), G_OBJECT (clipboard));
    }

  init_info (self, GTK_LABEL (self->primary_info), clipboard);
}

static void
on_drop_enter (GtkDropControllerMotion *motion,
               double                   x,
               double                   y,
               GtkInspectorClipboard   *self)
{
  GdkDrop *drop = gtk_drop_controller_motion_get_drop (motion);

  init_formats (self, GTK_LIST_BOX (self->dnd_formats), gdk_drop_get_formats (drop), G_OBJECT (drop));

  if (gdk_drop_get_drag (drop))
    gtk_label_set_text (GTK_LABEL (self->dnd_info), C_("clipboard", "local"));
  else
    gtk_label_set_text (GTK_LABEL (self->dnd_info), C_("clipboard", "remote"));
}

static void
gtk_inspector_clipboard_unset_display (GtkInspectorClipboard *self)
{
  GdkClipboard *clipboard;

  if (self->display == NULL)
    return;

  clipboard = gdk_display_get_clipboard (self->display);
  g_signal_handlers_disconnect_by_func (clipboard, clipboard_notify, self);

  clipboard = gdk_display_get_primary_clipboard (self->display);
  g_signal_handlers_disconnect_by_func (clipboard, primary_notify, self);
}

static void
gtk_inspector_clipboard_init (GtkInspectorClipboard *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
gtk_inspector_clipboard_dispose (GObject *object)
{
  GtkInspectorClipboard *self = GTK_INSPECTOR_CLIPBOARD (object);

  gtk_inspector_clipboard_unset_display (self);

  gtk_widget_dispose_template (GTK_WIDGET (self), GTK_TYPE_INSPECTOR_CLIPBOARD);

  G_OBJECT_CLASS (gtk_inspector_clipboard_parent_class)->dispose (object);
}

static void
gtk_inspector_clipboard_class_init (GtkInspectorClipboardClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gtk_inspector_clipboard_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/clipboard.ui");
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorClipboard, swin);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorClipboard, dnd_formats);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorClipboard, dnd_info);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorClipboard, clipboard_formats);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorClipboard, clipboard_info);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorClipboard, primary_formats);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorClipboard, primary_info);

  gtk_widget_class_bind_template_callback (widget_class, on_drop_enter);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

void
gtk_inspector_clipboard_set_display (GtkInspectorClipboard *self,
                                     GdkDisplay            *display)
{
  GdkClipboard *clipboard;

  gtk_inspector_clipboard_unset_display (self);

  self->display = display;

  if (display == NULL)
    return;

  clipboard = gdk_display_get_clipboard (display);
  g_signal_connect (clipboard, "notify", G_CALLBACK (clipboard_notify), self);
  init_formats (self, GTK_LIST_BOX (self->clipboard_formats), gdk_clipboard_get_formats (clipboard), G_OBJECT (clipboard));
  init_info (self, GTK_LABEL (self->clipboard_info), clipboard);

  clipboard = gdk_display_get_primary_clipboard (display);
  g_signal_connect (clipboard, "notify", G_CALLBACK (primary_notify), self);
  init_formats (self, GTK_LIST_BOX (self->primary_formats), gdk_clipboard_get_formats (clipboard), G_OBJECT (clipboard));
  init_info (self, GTK_LABEL (self->primary_info), clipboard);
}

