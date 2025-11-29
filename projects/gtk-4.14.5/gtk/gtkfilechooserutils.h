/* GTK - The GIMP Toolkit
 * gtkfilechooserutils.h: Private utility functions useful for
 *                        implementing a GtkFileChooser interface
 * Copyright (C) 2003, Red Hat, Inc.
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

#pragma once

#include "gtkfilechooserprivate.h"
#include "gtkicontheme.h"

G_BEGIN_DECLS

#define GTK_FILE_CHOOSER_DELEGATE_QUARK	  (_gtk_file_chooser_delegate_get_quark ())

typedef enum {
  GTK_FILE_CHOOSER_PROP_FIRST                  = 0x1000,
  GTK_FILE_CHOOSER_PROP_ACTION                 = GTK_FILE_CHOOSER_PROP_FIRST,
  GTK_FILE_CHOOSER_PROP_FILTER,
  GTK_FILE_CHOOSER_PROP_SELECT_MULTIPLE,
  GTK_FILE_CHOOSER_PROP_CREATE_FOLDERS,
  GTK_FILE_CHOOSER_PROP_FILTERS,
  GTK_FILE_CHOOSER_PROP_SHORTCUT_FOLDERS,
  GTK_FILE_CHOOSER_PROP_LAST                   = GTK_FILE_CHOOSER_PROP_SHORTCUT_FOLDERS
} GtkFileChooserProp;

void _gtk_file_chooser_install_properties (GObjectClass *klass);

void _gtk_file_chooser_delegate_iface_init (GtkFileChooserIface *iface);
void _gtk_file_chooser_set_delegate        (GtkFileChooser *receiver,
					    GtkFileChooser *delegate);

GQuark _gtk_file_chooser_delegate_get_quark (void) G_GNUC_CONST;

GSettings *_gtk_file_chooser_get_settings_for_widget (GtkWidget *widget);

char * _gtk_file_chooser_label_for_file (GFile *file);

gboolean        _gtk_file_info_consider_as_directory (GFileInfo *info);
gboolean        _gtk_file_has_native_path (GFile *file);
gboolean        _gtk_file_consider_as_remote (GFile *file);
GIcon *         _gtk_file_info_get_icon    (GFileInfo    *info,
                                            int           icon_size,
                                            int           scale,
                                            GtkIconTheme *icon_theme);

GFile *         _gtk_file_info_get_file (GFileInfo *info);

G_END_DECLS

