/* cc-applications-row.c
 *
 * Copyright 2018 Matthias Clasen <matthias.clasen@gmail.com>
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

#include <config.h>
#include <glib/gi18n.h>

#include "cc-applications-row.h"
#include "cc-applications-resources.h"

struct _CcApplicationsRow
{
  AdwActionRow  parent;

  GAppInfo     *info;
};

G_DEFINE_TYPE (CcApplicationsRow, cc_applications_row, ADW_TYPE_ACTION_ROW)

static void
cc_applications_row_finalize (GObject *object)
{
  CcApplicationsRow *self = CC_APPLICATIONS_ROW (object);

  g_object_unref (self->info);

  G_OBJECT_CLASS (cc_applications_row_parent_class)->finalize (object);
}

static void
cc_applications_row_class_init (CcApplicationsRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_applications_row_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/applications/cc-applications-row.ui");
}

static void
cc_applications_row_init (CcApplicationsRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

CcApplicationsRow *
cc_applications_row_new (GAppInfo *info)
{
  CcApplicationsRow *self;
  g_autoptr(GIcon) icon = NULL;
  GtkWidget *w;

  self = g_object_new (CC_TYPE_APPLICATIONS_ROW, NULL);

  self->info = g_object_ref (info);

  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (self), TRUE);
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self),
                                 g_markup_escape_text (g_app_info_get_display_name (info), -1));

  icon = g_app_info_get_icon (info);
  if (icon != NULL)
    g_object_ref (icon);
  else
    icon = g_themed_icon_new ("application-x-executable");
  w = gtk_image_new_from_gicon (icon);
  gtk_style_context_add_class (gtk_widget_get_style_context (w), "lowres-icon");
  gtk_image_set_icon_size (GTK_IMAGE (w), GTK_ICON_SIZE_LARGE);
  adw_action_row_add_prefix (ADW_ACTION_ROW (self), w);

  return self;
}

GAppInfo *
cc_applications_row_get_info (CcApplicationsRow *self)
{
  return self->info;
}
