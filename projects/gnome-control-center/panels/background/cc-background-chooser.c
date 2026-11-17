/* cc-background-chooser.c
 *
 * Copyright 2019 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-background-chooser"

#include <glib/gi18n.h>
#include <libgnome-desktop/gnome-desktop-thumbnail.h>

#include "bg-recent-source.h"
#include "bg-wallpapers-source.h"
#include "cc-background-chooser.h"
#include "cc-background-paintable.h"
#include "cc-background-item.h"

#define THUMBNAIL_WIDTH 144
#define THUMBNAIL_HEIGHT (THUMBNAIL_WIDTH * 3 / 4)

struct _CcBackgroundChooser
{
  GtkBox              parent;

  GtkFlowBox         *flowbox;
  GtkWidget          *recent_box;
  GtkFlowBox         *recent_flowbox;

  gboolean            recent_selected;

  BgWallpapersSource *wallpapers_source;
  BgRecentSource     *recent_source;

  CcBackgroundItem   *active_item;

  GnomeDesktopThumbnailFactory *thumbnail_factory;
};

G_DEFINE_TYPE (CcBackgroundChooser, cc_background_chooser, GTK_TYPE_BOX)

enum
{
  BACKGROUND_CHOSEN,
  N_SIGNALS,
};

static guint signals [N_SIGNALS];

static void
emit_background_chosen (CcBackgroundChooser *self)
{
  g_autoptr(GList) list = NULL;
  CcBackgroundItem *item;
  GtkFlowBox *flowbox;

  flowbox = self->recent_selected ? self->recent_flowbox : self->flowbox;
  list = gtk_flow_box_get_selected_children (flowbox);
  g_assert (g_list_length (list) == 1);

  item = g_object_get_data (list->data, "item");

  g_signal_emit (self, signals[BACKGROUND_CHOSEN], 0, item);
}

static void
on_delete_background_clicked_cb (GtkButton *button,
                                 BgRecentSource  *source)
{
  GtkWidget *parent;
  CcBackgroundItem *item;

  parent = gtk_widget_get_parent (gtk_widget_get_parent (GTK_WIDGET (button)));
  g_assert (GTK_IS_FLOW_BOX_CHILD (parent));

  item = g_object_get_data (G_OBJECT (parent), "item");

  bg_recent_source_remove_item (source, item);
}

static void
direction_changed_cb (GtkWidget        *widget,
                      GtkTextDirection *previous_direction,
                      GdkPaintable     *paintable)
{
  g_object_set (paintable,
                "text-direction", gtk_widget_get_direction (widget),
                NULL);
}

static GtkWidget*
create_widget_func (gpointer model_item,
                    gpointer user_data)
{
  g_autoptr(CcBackgroundPaintable) paintable = NULL;
  CcBackgroundChooser *self;
  CcBackgroundItem *item;
  GtkWidget *overlay;
  GtkWidget *child;
  GtkWidget *picture;
  GtkWidget *icon;
  GtkWidget *check;
  GtkWidget *button = NULL;
  BgSource *source;

  source = BG_SOURCE (user_data);
  item = CC_BACKGROUND_ITEM (model_item);

  self = g_object_get_data (G_OBJECT (source), "background-chooser");

  paintable = cc_background_paintable_new (self->thumbnail_factory,
                                           item,
                                           CC_BACKGROUND_PAINT_LIGHT_DARK,
                                           THUMBNAIL_WIDTH,
                                           THUMBNAIL_HEIGHT,
                                           gtk_widget_get_scale_factor (GTK_WIDGET (self)));

  picture = gtk_picture_new_for_paintable (GDK_PAINTABLE (paintable));
  gtk_picture_set_can_shrink (GTK_PICTURE (picture), FALSE);

  g_object_bind_property (picture, "scale-factor",
                          paintable, "scale-factor", G_BINDING_SYNC_CREATE);
  g_signal_connect_object (picture, "direction-changed",
                           G_CALLBACK (direction_changed_cb), paintable, 0);

  icon = gtk_image_new_from_icon_name ("slideshow-symbolic");
  gtk_widget_set_halign (icon, GTK_ALIGN_START);
  gtk_widget_set_valign (icon, GTK_ALIGN_END);
  gtk_widget_set_visible (icon, cc_background_item_changes_with_time (item));
  gtk_widget_add_css_class (icon, "slideshow-icon");

  check = gtk_image_new_from_icon_name ("background-selected-symbolic");
  gtk_widget_set_halign (check, GTK_ALIGN_END);
  gtk_widget_set_valign (check, GTK_ALIGN_END);
  gtk_widget_add_css_class (check, "selected-check");

  if (BG_IS_RECENT_SOURCE (source))
    {
      button = gtk_button_new_from_icon_name ("window-close-symbolic");
      gtk_widget_set_halign (button, GTK_ALIGN_END);
      gtk_widget_set_valign (button, GTK_ALIGN_START);

      gtk_widget_add_css_class (button, "osd");
      gtk_widget_add_css_class (button, "circular");
      gtk_widget_add_css_class (button, "remove-button");

     gtk_widget_set_tooltip_text (GTK_WIDGET (button), _("Remove Background"));

      g_signal_connect (button,
                        "clicked",
                        G_CALLBACK (on_delete_background_clicked_cb),
                        source);
    }

  overlay = gtk_overlay_new ();
  gtk_widget_set_overflow (overlay, GTK_OVERFLOW_HIDDEN);
  gtk_widget_add_css_class (overlay, "background-thumbnail");
  gtk_overlay_set_child (GTK_OVERLAY (overlay), picture);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), icon);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), check);
  if (button)
    gtk_overlay_add_overlay (GTK_OVERLAY (overlay), button);
  gtk_accessible_update_property (GTK_ACCESSIBLE (overlay),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL,
                                  cc_background_item_get_name (item),
                                  -1);


  child = gtk_flow_box_child_new ();
  gtk_widget_set_halign (child, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (child, GTK_ALIGN_CENTER);
  gtk_flow_box_child_set_child (GTK_FLOW_BOX_CHILD (child), overlay);

  g_object_set_data_full (G_OBJECT (child), "item", g_object_ref (item), g_object_unref);

  if (self->active_item && cc_background_item_compare (item, self->active_item))
    gtk_widget_add_css_class (GTK_WIDGET (child), "active-item");

  return child;
}

static void
update_recent_visibility (CcBackgroundChooser *self)
{
  GListStore *store;
  gboolean has_items;

  store = bg_source_get_liststore (BG_SOURCE (self->recent_source));
  has_items = g_list_model_get_n_items (G_LIST_MODEL (store)) != 0;

  gtk_widget_set_visible (self->recent_box, has_items);
}

static void
setup_flowbox (CcBackgroundChooser *self)
{
  GListStore *store;

  store = bg_source_get_liststore (BG_SOURCE (self->wallpapers_source));

  gtk_flow_box_bind_model (self->flowbox,
                           G_LIST_MODEL (store),
                           create_widget_func,
                           self->wallpapers_source,
                           NULL);

  store = bg_source_get_liststore (BG_SOURCE (self->recent_source));

  gtk_flow_box_bind_model (self->recent_flowbox,
                           G_LIST_MODEL (store),
                           create_widget_func,
                           self->recent_source,
                           NULL);

  update_recent_visibility (self);
  g_signal_connect_object (store,
                           "items-changed",
                           G_CALLBACK (update_recent_visibility),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
on_item_activated_cb (CcBackgroundChooser *self,
                      GtkFlowBoxChild     *child,
                      GtkFlowBox          *flowbox)
{
  self->recent_selected = flowbox == self->recent_flowbox;
  if (self->recent_selected)
    gtk_flow_box_unselect_all (self->flowbox);
  else
    gtk_flow_box_unselect_all (self->recent_flowbox);
  emit_background_chosen (self);
}

static void
file_dialog_open_cb (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
  CcBackgroundChooser *self = CC_BACKGROUND_CHOOSER (user_data);
  GtkFileDialog *file_dialog = GTK_FILE_DIALOG (source_object);
  g_autoptr(GListModel) files = NULL;
  g_autoptr(GError) error = NULL;
  guint i;

  files = gtk_file_dialog_open_multiple_finish (file_dialog, res, &error);

  if (error != NULL)
    {
     g_warning ("Failed to pick backgrounds: %s", error->message);
     return;
    }

  for (i = 0; i < g_list_model_get_n_items (files); i++)
    {
      g_autoptr(GFile) file = g_list_model_get_item (files, i);
      g_autofree gchar *filename = g_file_get_path (file);

      bg_recent_source_add_file (self->recent_source, filename);
    }
}

/* GObject overrides */

static void
cc_background_chooser_finalize (GObject *object)
{
  CcBackgroundChooser *self = (CcBackgroundChooser *)object;

  g_clear_object (&self->recent_source);
  g_clear_object (&self->wallpapers_source);
  g_clear_object (&self->thumbnail_factory);

  G_OBJECT_CLASS (cc_background_chooser_parent_class)->finalize (object);
}

static void
cc_background_chooser_class_init (CcBackgroundChooserClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_background_chooser_finalize;

  signals[BACKGROUND_CHOSEN] = g_signal_new ("background-chosen",
                                             CC_TYPE_BACKGROUND_CHOOSER,
                                             G_SIGNAL_RUN_FIRST,
                                             0, NULL, NULL, NULL,
                                             G_TYPE_NONE,
                                             1,
                                             CC_TYPE_BACKGROUND_ITEM);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/background/cc-background-chooser.ui");

  gtk_widget_class_bind_template_child (widget_class, CcBackgroundChooser, flowbox);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundChooser, recent_box);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundChooser, recent_flowbox);

  gtk_widget_class_bind_template_callback (widget_class, on_item_activated_cb);
}

static void
cc_background_chooser_init (CcBackgroundChooser *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->recent_source = bg_recent_source_new ();
  self->wallpapers_source = bg_wallpapers_source_new ();

  self->thumbnail_factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);
  g_object_set_data (G_OBJECT (self->recent_source), "background-chooser", self);
  g_object_set_data (G_OBJECT (self->wallpapers_source), "background-chooser", self);

  setup_flowbox (self);
}

void
cc_background_chooser_select_file (CcBackgroundChooser *self)
{
  g_autoptr(GFile) pictures_folder = NULL;
  GtkFileFilter *filter;
  GtkFileDialog *file_dialog;
  GtkWindow *toplevel;
  GListStore *filters;

  g_return_if_fail (CC_IS_BACKGROUND_CHOOSER (self));

  toplevel = (GtkWindow*) gtk_widget_get_native (GTK_WIDGET (self));

  file_dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (file_dialog, _("Select a picture"));
  gtk_file_dialog_set_modal (file_dialog, TRUE);

  filter = gtk_file_filter_new ();
  gtk_file_filter_add_pixbuf_formats (filter);

  filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
  g_list_store_append (filters, filter);
  gtk_file_dialog_set_filters (file_dialog, G_LIST_MODEL (filters));

  pictures_folder = g_file_new_for_path (g_get_user_special_dir (G_USER_DIRECTORY_PICTURES));
  gtk_file_dialog_set_initial_folder (file_dialog, pictures_folder);

  gtk_file_dialog_open_multiple (file_dialog,
                                 toplevel,
                                 NULL,
                                 file_dialog_open_cb,
                                 self);
}

static void flow_box_set_active_item (GtkFlowBox *flowbox, CcBackgroundItem *active_item)
{
  GtkFlowBoxChild *child = NULL;
  CcBackgroundItem *item;
  int idx = 0;

  while ((child = gtk_flow_box_get_child_at_index (flowbox, idx++)))
    {
      item = g_object_get_data (G_OBJECT (child), "item");

      if (cc_background_item_compare (item, active_item))
        gtk_widget_add_css_class (GTK_WIDGET (child), "active-item");
      else
        gtk_widget_remove_css_class (GTK_WIDGET (child), "active-item");
    }
}

void
cc_background_chooser_set_active_item (CcBackgroundChooser *self, CcBackgroundItem *active_item)
{
  g_return_if_fail (CC_IS_BACKGROUND_CHOOSER (self));
  g_return_if_fail (CC_IS_BACKGROUND_ITEM (active_item));

  self->active_item = active_item;

  flow_box_set_active_item (self->flowbox, active_item);
  flow_box_set_active_item (self->recent_flowbox, active_item);
}
