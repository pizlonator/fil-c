/*
 * Copyright © 2022 Red Hat, Inc.
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtkfilechoosercellprivate.h"

#include "gtkprivate.h"
#include "gtkbinlayout.h"
#include "gtkdragsource.h"
#include "gtkgestureclick.h"
#include "gtkgesturelongpress.h"
#include "gtkicontheme.h"
#include "gtklistitem.h"
#include "gtkselectionmodel.h"
#include "gtkfilechooserutils.h"
#include "gtkfilechooserwidgetprivate.h"

struct _GtkFileChooserCell
{
  GtkWidget parent_instance;

  GFileInfo *item;
  GtkListItem *list_item;
};

struct _GtkFileChooserCellClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (GtkFileChooserCell, gtk_file_chooser_cell, GTK_TYPE_WIDGET)

enum
{
  PROP_POSITION = 1,
  PROP_ITEM,
  PROP_LIST_ITEM,
};

#define ICON_SIZE 16

static void
popup_menu (GtkFileChooserCell *self,
            double              x,
            double              y)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkWidget *impl;
  graphene_point_t p;

  gtk_widget_activate_action (GTK_WIDGET (self), "listitem.select", "(bb)", FALSE, FALSE);

  impl = gtk_widget_get_ancestor (widget, GTK_TYPE_FILE_CHOOSER_WIDGET);

  if (!gtk_widget_compute_point (widget, GTK_WIDGET (impl),
                                 &GRAPHENE_POINT_INIT (x, y), &p))
    return;

  if (self->list_item)
    gtk_widget_activate_action (widget, "item.popup-file-list-menu",
                                "(udd)", gtk_list_item_get_position (self->list_item), p.x, p.y);
}

static void
file_chooser_cell_clicked (GtkEventController *controller,
                           int                 n_press,
                           double              x,
                           double              y)
{
  GtkWidget *widget = gtk_event_controller_get_widget (controller);
  GtkFileChooserCell *self = GTK_FILE_CHOOSER_CELL (widget);

  gtk_gesture_set_state (GTK_GESTURE (controller), GTK_EVENT_SEQUENCE_CLAIMED);
  popup_menu (self, x, y);
}

static void
file_chooser_cell_long_pressed (GtkEventController *controller,
                                double              x,
                                double              y)
{
  GtkWidget *widget = gtk_event_controller_get_widget (controller);
  GtkFileChooserCell *self = GTK_FILE_CHOOSER_CELL (widget);

  gtk_gesture_set_state (GTK_GESTURE (controller), GTK_EVENT_SEQUENCE_CLAIMED);
  popup_menu (self, x, y);
}

static GdkContentProvider *
drag_prepare_cb (GtkDragSource *source,
                 double         x,
                 double         y,
                 gpointer       user_data)
{
  GdkContentProvider *provider;
  GSList *selection;
  GtkFileChooserWidget *impl;
  GtkIconTheme *icon_theme;
  GIcon *icon;
  int scale;
  GtkIconPaintable *paintable;
  GtkFileChooserCell *self = user_data;

  impl = GTK_FILE_CHOOSER_WIDGET (gtk_widget_get_ancestor (GTK_WIDGET (self),
                                                           GTK_TYPE_FILE_CHOOSER_WIDGET));

  if (self->list_item && !gtk_list_item_get_selected (self->list_item))
    {
      gtk_widget_activate_action (GTK_WIDGET (self), "listitem.select", "(bb)", FALSE, FALSE);
    }

  selection = gtk_file_chooser_widget_get_selected_files (impl);
  if (!selection)
    return NULL;

  scale = gtk_widget_get_scale_factor (GTK_WIDGET (self));
  icon_theme = gtk_icon_theme_get_for_display (gtk_widget_get_display (GTK_WIDGET (self)));

  icon = _gtk_file_info_get_icon (self->item, ICON_SIZE, scale, icon_theme);

  paintable = gtk_icon_theme_lookup_by_gicon (icon_theme,icon, ICON_SIZE, scale, GTK_TEXT_DIR_NONE, 0);

  gtk_drag_source_set_icon (source, GDK_PAINTABLE (paintable), x, y);

  provider = gdk_content_provider_new_typed (GDK_TYPE_FILE_LIST, selection);
  g_slist_free_full (selection, g_object_unref);
  g_object_unref (paintable);
  g_object_unref (icon);

  return provider;
}

static void
gtk_file_chooser_cell_init (GtkFileChooserCell *self)
{
  GtkGesture *gesture;
  GtkDragSource *drag_source;

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_SECONDARY);
  g_signal_connect (gesture, "pressed", G_CALLBACK (file_chooser_cell_clicked), NULL);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (gesture));

  gesture = gtk_gesture_long_press_new ();
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture), TRUE);
  g_signal_connect (gesture, "pressed", G_CALLBACK (file_chooser_cell_long_pressed), NULL);

  drag_source = gtk_drag_source_new ();
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (drag_source));
  g_signal_connect (drag_source, "prepare", G_CALLBACK (drag_prepare_cb), self);
}

static void
gtk_file_chooser_cell_dispose (GObject *object)
{
  GtkWidget *child;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (object))))
    gtk_widget_unparent (child);

  G_OBJECT_CLASS (gtk_file_chooser_cell_parent_class)->dispose (object);
}

static gboolean
get_selectable (GtkFileChooserCell *self)
{
  if (self->item)
    return g_file_info_get_attribute_boolean (self->item, "filechooser::selectable");

  return TRUE;
}

static void
gtk_file_chooser_cell_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GtkFileChooserCell *self = GTK_FILE_CHOOSER_CELL (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      self->item = g_value_get_object (value);

      if (get_selectable (self))
        gtk_widget_remove_css_class (GTK_WIDGET (self), "dim-label");
      else
        gtk_widget_add_css_class (GTK_WIDGET (self), "dim-label");

      break;

    case PROP_LIST_ITEM:
      self->list_item = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_file_chooser_cell_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GtkFileChooserCell *self = GTK_FILE_CHOOSER_CELL (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      g_value_set_object (value, self->item);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
gtk_file_chooser_cell_class_init (GtkFileChooserCellClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_file_chooser_cell_dispose;
  object_class->set_property = gtk_file_chooser_cell_set_property;
  object_class->get_property = gtk_file_chooser_cell_get_property;

  g_object_class_install_property (object_class, PROP_ITEM,
                                   g_param_spec_object ("item", NULL, NULL,
                                                        G_TYPE_FILE_INFO,
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_LIST_ITEM,
                                   g_param_spec_object ("list-item", NULL, NULL,
                                                        GTK_TYPE_LIST_ITEM,
                                                        GTK_PARAM_WRITABLE));

  gtk_widget_class_set_css_name (widget_class, I_("filelistcell"));
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

GtkFileChooserCell *
gtk_file_chooser_cell_new (void)
{
  return g_object_new (GTK_TYPE_FILE_CHOOSER_CELL, NULL);
}
