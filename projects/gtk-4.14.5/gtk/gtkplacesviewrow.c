/* gtkplacesviewrow.c
 *
 * Copyright (C) 2015 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gio/gio.h>

#include "gtkplacesviewrowprivate.h"

/* As this widget is shared with Nautilus, we use this guard to
 * ensure that internally we only include the files that we need
 * instead of including gtk.h
 */
#ifdef GTK_COMPILATION
#include "gtkbutton.h"
#include "gtkgesture.h"
#include "gtkimage.h"
#include <glib/gi18n-lib.h>
#include "gtklabel.h"
#include "gtkspinner.h"
#include "gtkstack.h"
#include "gtktypebuiltins.h"
#include "gtknative.h"
#include "gtkpopover.h"
#else
#include <gtk/gtk.h>
#endif

struct _GtkPlacesViewRow
{
  GtkListBoxRow  parent_instance;

  GtkLabel      *available_space_label;
  GtkStack      *mount_stack;
  GtkSpinner    *busy_spinner;
  GtkButton     *eject_button;
  GtkImage      *eject_icon;
  GtkImage      *icon_image;
  GtkLabel      *name_label;
  GtkLabel      *path_label;

  GVolume       *volume;
  GMount        *mount;
  GFile         *file;

  GCancellable  *cancellable;

  int            is_network : 1;
};

G_DEFINE_TYPE (GtkPlacesViewRow, gtk_places_view_row, GTK_TYPE_LIST_BOX_ROW)

enum {
  PROP_0,
  PROP_ICON,
  PROP_NAME,
  PROP_PATH,
  PROP_VOLUME,
  PROP_MOUNT,
  PROP_FILE,
  PROP_IS_NETWORK,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
measure_available_space_finished (GObject      *object,
                                  GAsyncResult *res,
                                  gpointer      user_data)
{
  GtkPlacesViewRow *row = user_data;
  GFileInfo *info;
  GError *error;
  guint64 free_space;
  guint64 total_space;
  char *formatted_free_size;
  char *formatted_total_size;
  char *label;
  guint plural_form;

  error = NULL;

  info = g_file_query_filesystem_info_finish (G_FILE (object),
                                              res,
                                              &error);

  if (error)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
          !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_MOUNTED))
        {
          g_warning ("Failed to measure available space: %s", error->message);
        }

      g_clear_error (&error);
      goto out;
    }

  if (!g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE) ||
      !g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_FILESYSTEM_SIZE))
    {
      g_object_unref (info);
      goto out;
    }

  free_space = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
  total_space = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);

  formatted_free_size = g_format_size (free_space);
  formatted_total_size = g_format_size (total_space);

  /* read g_format_size code in glib for further understanding */
  plural_form = free_space < 1000 ? free_space : free_space % 1000 + 1000;

  /* Translators: respectively, free and total space of the drive. The plural form
   * should be based on the free space available.
   * i.e. 1 GB / 24 GB available.
   */
  label = g_strdup_printf (dngettext (GETTEXT_PACKAGE, "%s / %s available", "%s / %s available", plural_form),
                           formatted_free_size, formatted_total_size);

  gtk_label_set_label (row->available_space_label, label);

  g_object_unref (info);
  g_free (formatted_total_size);
  g_free (formatted_free_size);
  g_free (label);
out:
  g_object_unref (object);
}

static void
measure_available_space (GtkPlacesViewRow *row)
{
  gboolean should_measure;

  should_measure = (!row->is_network && (row->volume || row->mount || row->file));

  gtk_label_set_label (row->available_space_label, "");
  gtk_widget_set_visible (GTK_WIDGET (row->available_space_label), should_measure);

  if (should_measure)
    {
      GFile *file = NULL;

      if (row->file)
        {
          file = g_object_ref (row->file);
        }
      else if (row->mount)
        {
          file = g_mount_get_root (row->mount);
        }
      else if (row->volume)
        {
          GMount *mount;

          mount = g_volume_get_mount (row->volume);

          if (mount)
            file = g_mount_get_root (row->mount);

          g_clear_object (&mount);
        }

      if (file)
        {
          g_cancellable_cancel (row->cancellable);
          g_clear_object (&row->cancellable);
          row->cancellable = g_cancellable_new ();

          g_file_query_filesystem_info_async (file,
                                              G_FILE_ATTRIBUTE_FILESYSTEM_FREE "," G_FILE_ATTRIBUTE_FILESYSTEM_SIZE,
                                              G_PRIORITY_DEFAULT,
                                              row->cancellable,
                                              measure_available_space_finished,
                                              row);
        }
    }
}

static void
gtk_places_view_row_finalize (GObject *object)
{
  GtkPlacesViewRow *self = GTK_PLACES_VIEW_ROW (object);

  g_cancellable_cancel (self->cancellable);

  g_clear_object (&self->volume);
  g_clear_object (&self->mount);
  g_clear_object (&self->file);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (gtk_places_view_row_parent_class)->finalize (object);
}

static void
gtk_places_view_row_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GtkPlacesViewRow *self;

  self = GTK_PLACES_VIEW_ROW (object);

  switch (prop_id)
    {
    case PROP_ICON:
      g_value_set_object (value, gtk_image_get_gicon (self->icon_image));
      break;

    case PROP_NAME:
      g_value_set_string (value, gtk_label_get_label (self->name_label));
      break;

    case PROP_PATH:
      g_value_set_string (value, gtk_label_get_label (self->path_label));
      break;

    case PROP_VOLUME:
      g_value_set_object (value, self->volume);
      break;

    case PROP_MOUNT:
      g_value_set_object (value, self->mount);
      break;

    case PROP_FILE:
      g_value_set_object (value, self->file);
      break;

    case PROP_IS_NETWORK:
      g_value_set_boolean (value, self->is_network);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_places_view_row_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GtkPlacesViewRow *self = GTK_PLACES_VIEW_ROW (object);

  switch (prop_id)
    {
    case PROP_ICON:
      gtk_image_set_from_gicon (self->icon_image, g_value_get_object (value));
      break;

    case PROP_NAME:
      gtk_label_set_label (self->name_label, g_value_get_string (value));
      break;

    case PROP_PATH:
      gtk_label_set_label (self->path_label, g_value_get_string (value));
      break;

    case PROP_VOLUME:
      g_set_object (&self->volume, g_value_get_object (value));
      break;

    case PROP_MOUNT:
      g_set_object (&self->mount, g_value_get_object (value));
      if (self->mount != NULL)
        {
          gtk_stack_set_visible_child (self->mount_stack, GTK_WIDGET (self->eject_button));
          gtk_widget_set_child_visible (GTK_WIDGET (self->mount_stack), TRUE);
        }
      else
        {
          gtk_widget_set_child_visible (GTK_WIDGET (self->mount_stack), FALSE);
        }
      measure_available_space (self);
      break;

    case PROP_FILE:
      g_set_object (&self->file, g_value_get_object (value));
      measure_available_space (self);
      break;

    case PROP_IS_NETWORK:
      gtk_places_view_row_set_is_network (self, g_value_get_boolean (value));
      measure_available_space (self);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_places_view_row_size_allocate (GtkWidget *widget,
                                   int        width,
                                   int        height,
                                   int        baseline)
{
  GtkWidget *menu = GTK_WIDGET (g_object_get_data (G_OBJECT (widget), "menu"));

  GTK_WIDGET_CLASS (gtk_places_view_row_parent_class)->size_allocate (widget, width, height, baseline);
  if (menu)
    gtk_popover_present (GTK_POPOVER (menu));
}

static void
gtk_places_view_row_class_init (GtkPlacesViewRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtk_places_view_row_finalize;
  object_class->get_property = gtk_places_view_row_get_property;
  object_class->set_property = gtk_places_view_row_set_property;

  widget_class->size_allocate = gtk_places_view_row_size_allocate;

  properties[PROP_ICON] =
          g_param_spec_object ("icon", NULL, NULL,
                               G_TYPE_ICON,
                               G_PARAM_READWRITE);

  properties[PROP_NAME] =
          g_param_spec_string ("name", NULL, NULL,
                               "",
                               G_PARAM_READWRITE);

  properties[PROP_PATH] =
          g_param_spec_string ("path", NULL, NULL,
                               "",
                               G_PARAM_READWRITE);

  properties[PROP_VOLUME] =
          g_param_spec_object ("volume", NULL, NULL,
                               G_TYPE_VOLUME,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  properties[PROP_MOUNT] =
          g_param_spec_object ("mount", NULL, NULL,
                               G_TYPE_MOUNT,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  properties[PROP_FILE] =
          g_param_spec_object ("file", NULL, NULL,
                               G_TYPE_FILE,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  properties[PROP_IS_NETWORK] =
          g_param_spec_boolean ("is-network", NULL, NULL,
                                FALSE,
                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtkplacesviewrow.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkPlacesViewRow, available_space_label);
  gtk_widget_class_bind_template_child (widget_class, GtkPlacesViewRow, mount_stack);
  gtk_widget_class_bind_template_child (widget_class, GtkPlacesViewRow, busy_spinner);
  gtk_widget_class_bind_template_child (widget_class, GtkPlacesViewRow, eject_button);
  gtk_widget_class_bind_template_child (widget_class, GtkPlacesViewRow, eject_icon);
  gtk_widget_class_bind_template_child (widget_class, GtkPlacesViewRow, icon_image);
  gtk_widget_class_bind_template_child (widget_class, GtkPlacesViewRow, name_label);
  gtk_widget_class_bind_template_child (widget_class, GtkPlacesViewRow, path_label);
}

static void
gtk_places_view_row_init (GtkPlacesViewRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget*
gtk_places_view_row_new (GVolume *volume,
                         GMount  *mount)
{
  return g_object_new (GTK_TYPE_PLACES_VIEW_ROW,
                       "volume", volume,
                       "mount", mount,
                       NULL);
}

GMount*
gtk_places_view_row_get_mount (GtkPlacesViewRow *row)
{
  g_return_val_if_fail (GTK_IS_PLACES_VIEW_ROW (row), NULL);

  return row->mount;
}

GVolume*
gtk_places_view_row_get_volume (GtkPlacesViewRow *row)
{
  g_return_val_if_fail (GTK_IS_PLACES_VIEW_ROW (row), NULL);

  return row->volume;
}

GFile*
gtk_places_view_row_get_file (GtkPlacesViewRow *row)
{
  g_return_val_if_fail (GTK_IS_PLACES_VIEW_ROW (row), NULL);

  return row->file;
}

GtkWidget*
gtk_places_view_row_get_eject_button (GtkPlacesViewRow *row)
{
  g_return_val_if_fail (GTK_IS_PLACES_VIEW_ROW (row), NULL);

  return GTK_WIDGET (row->eject_button);
}

void
gtk_places_view_row_set_busy (GtkPlacesViewRow *row,
                              gboolean          is_busy)
{
  g_return_if_fail (GTK_IS_PLACES_VIEW_ROW (row));

  if (is_busy)
    {
      gtk_stack_set_visible_child (row->mount_stack, GTK_WIDGET (row->busy_spinner));
      gtk_widget_set_child_visible (GTK_WIDGET (row->mount_stack), TRUE);
      gtk_spinner_start (row->busy_spinner);
    }
  else
    {
      gtk_widget_set_child_visible (GTK_WIDGET (row->mount_stack), FALSE);
      gtk_spinner_stop (row->busy_spinner);
    }
}

gboolean
gtk_places_view_row_get_is_network (GtkPlacesViewRow *row)
{
  g_return_val_if_fail (GTK_IS_PLACES_VIEW_ROW (row), FALSE);

  return row->is_network;
}

void
gtk_places_view_row_set_is_network (GtkPlacesViewRow *row,
                                    gboolean          is_network)
{
  if (row->is_network != is_network)
    {
      row->is_network = is_network;

      gtk_image_set_from_icon_name (row->eject_icon, "media-eject-symbolic");
      gtk_widget_set_tooltip_text (GTK_WIDGET (row->eject_button), is_network ? _("Disconnect") : _("Unmount"));
    }
}

void
gtk_places_view_row_set_path_size_group (GtkPlacesViewRow *row,
                                         GtkSizeGroup     *group)
{
  if (group)
    gtk_size_group_add_widget (group, GTK_WIDGET (row->path_label));
}

void
gtk_places_view_row_set_space_size_group (GtkPlacesViewRow *row,
                                          GtkSizeGroup     *group)
{
  if (group)
    gtk_size_group_add_widget (group, GTK_WIDGET (row->available_space_label));
}
