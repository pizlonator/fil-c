/* gtkfilethumbnail.c
 *
 * Copyright 2022 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "config.h"

#include "gtkfilethumbnail.h"

#include "gtkbinlayout.h"
#include "gtkfilechooserutils.h"
#include "gtkimage.h"
#include "gtkprivate.h"
#include "gtkwidget.h"

#define ICON_SIZE 16

struct _GtkFileThumbnail
{
  GtkWidget parent;

  GtkWidget *image;
  int icon_size;

  GCancellable *cancellable;
  GFileInfo *info;
};

typedef struct
{
  GtkWidgetClass parent;
} GtkFileThumbnailClass;

G_DEFINE_FINAL_TYPE (GtkFileThumbnail, _gtk_file_thumbnail, GTK_TYPE_WIDGET)

enum {
  PROP_0,
  PROP_ICON_SIZE,
  PROP_INFO,
  N_PROPS,
};

static GParamSpec *properties [N_PROPS];

static void
copy_attribute (GFileInfo  *to,
                GFileInfo  *from,
                const char *attribute)
{
  GFileAttributeType type;
  gpointer value;

  if (g_file_info_get_attribute_data (from, attribute, &type, &value, NULL))
    g_file_info_set_attribute (to, attribute, type, value);
}

static gboolean
update_image (GtkFileThumbnail *self)
{
  GtkIconTheme *icon_theme;
  GIcon *icon;
  int icon_size;
  int scale;

  if (!g_file_info_has_attribute (self->info, G_FILE_ATTRIBUTE_STANDARD_ICON))
    {
      gtk_image_clear (GTK_IMAGE (self->image));
      return FALSE;
    }

  scale = gtk_widget_get_scale_factor (GTK_WIDGET (self));
  icon_theme = gtk_icon_theme_get_for_display (gtk_widget_get_display (GTK_WIDGET (self)));

  icon_size = self->icon_size != -1 ? self->icon_size : ICON_SIZE;
  icon = _gtk_file_info_get_icon (self->info, icon_size, scale, icon_theme);

  gtk_image_set_from_gicon (GTK_IMAGE (self->image), icon);

  g_object_unref (icon);

  return TRUE;
}

static void
thumbnail_queried_cb (GObject      *object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  GtkFileThumbnail *self = user_data; /* might be unreffed if operation was cancelled */
  GFile *file = G_FILE (object);
  GFileInfo *queried;
  GError *error = NULL;

  queried = g_file_query_info_finish (file, result, &error);

  if (error)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_file_info_set_attribute_boolean (self->info, "filechooser::queried", TRUE);
      g_clear_error (&error);
      return;
    }

  g_file_info_set_attribute_boolean (self->info, "filechooser::queried", TRUE);

  copy_attribute (self->info, queried, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);
  copy_attribute (self->info, queried, G_FILE_ATTRIBUTE_THUMBNAILING_FAILED);
  copy_attribute (self->info, queried, G_FILE_ATTRIBUTE_STANDARD_ICON);

  update_image (self);

  g_clear_object (&queried);

  g_clear_object (&self->cancellable);
}

static void
cancel_thumbnail (GtkFileThumbnail *self)
{
  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
}

static void
get_thumbnail (GtkFileThumbnail *self)
{
  if (!self->info)
    {
      gtk_image_clear (GTK_IMAGE (self->image));
      return;
    }

  if (!update_image (self))
    {
      GFile *file;

      if (g_file_info_has_attribute (self->info, "filechooser::queried"))
        return;

      g_assert (self->cancellable == NULL);
      self->cancellable = g_cancellable_new ();

      file = _gtk_file_info_get_file (self->info);
      g_file_query_info_async (file,
                               G_FILE_ATTRIBUTE_THUMBNAIL_PATH ","
                               G_FILE_ATTRIBUTE_THUMBNAILING_FAILED ","
                               G_FILE_ATTRIBUTE_STANDARD_ICON,
                               G_FILE_QUERY_INFO_NONE,
                               G_PRIORITY_DEFAULT,
                               self->cancellable,
                               thumbnail_queried_cb,
                               self);
    }
}

static void
_gtk_file_thumbnail_dispose (GObject *object)
{
  GtkFileThumbnail *self = (GtkFileThumbnail *)object;

  _gtk_file_thumbnail_set_info (self, NULL);

  g_clear_pointer (&self->image, gtk_widget_unparent);

  G_OBJECT_CLASS (_gtk_file_thumbnail_parent_class)->dispose (object);
}

static void
_gtk_file_thumbnail_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GtkFileThumbnail *self = GTK_FILE_THUMBNAIL (object);

  switch (prop_id)
    {
    case PROP_ICON_SIZE:
      g_value_set_int (value, self->icon_size);
      break;

    case PROP_INFO:
      g_value_set_object (value, self->info);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
_gtk_file_thumbnail_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GtkFileThumbnail *self = GTK_FILE_THUMBNAIL (object);

  switch (prop_id)
    {
    case PROP_ICON_SIZE:
      _gtk_file_thumbnail_set_icon_size (self, g_value_get_int (value));
      break;

    case PROP_INFO:
      _gtk_file_thumbnail_set_info (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
_gtk_file_thumbnail_class_init (GtkFileThumbnailClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = _gtk_file_thumbnail_dispose;
  object_class->get_property = _gtk_file_thumbnail_get_property;
  object_class->set_property = _gtk_file_thumbnail_set_property;

  properties[PROP_ICON_SIZE] =
    g_param_spec_int ("icon-size", NULL, NULL,
                      -1, G_MAXINT, -1,
                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_INFO] =
    g_param_spec_object ("file-info", NULL, NULL,
                         G_TYPE_FILE_INFO,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, I_("filethumbnail"));

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

static void
_gtk_file_thumbnail_init (GtkFileThumbnail *self)
{
  self->icon_size = -1;
  self->image = gtk_image_new ();
  gtk_widget_set_parent (self->image, GTK_WIDGET (self));
}

GFileInfo *
_gtk_file_thumbnail_get_info (GtkFileThumbnail *self)
{
  g_assert (GTK_IS_FILE_THUMBNAIL (self));

  return self->info;
}

void
_gtk_file_thumbnail_set_info (GtkFileThumbnail *self,
                              GFileInfo        *info)
{
  g_assert (GTK_IS_FILE_THUMBNAIL (self));
  g_assert (info == NULL || G_IS_FILE_INFO (info));

  if (g_set_object (&self->info, info))
    {
      cancel_thumbnail (self);
      get_thumbnail (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INFO]);
    }
}

int
_gtk_file_thumbnail_get_icon_size (GtkFileThumbnail *self)
{
  g_assert (GTK_IS_FILE_THUMBNAIL (self));

  return self->icon_size;
}

void
_gtk_file_thumbnail_set_icon_size (GtkFileThumbnail *self,
                                   int               icon_size)
{
  g_assert (GTK_IS_FILE_THUMBNAIL (self));
  g_assert (icon_size == -1 || icon_size > 0);

  if (self->icon_size == icon_size)
    return;

  self->icon_size = icon_size;
  if (self->icon_size == -1)
    gtk_image_set_pixel_size (GTK_IMAGE (self->image), ICON_SIZE);
  else
    gtk_image_set_pixel_size (GTK_IMAGE (self->image), icon_size);

  cancel_thumbnail (self);
  get_thumbnail (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON_SIZE]);
}

