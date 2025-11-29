/* gtkfilechooserwidgetprivate.h
 *
 * Copyright (C) 2015 Red Hat
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen
 */

#pragma once

#include <glib.h>
#include "deprecated/gtkfilechooserwidget.h"
#include "gtkselectionmodel.h"

G_BEGIN_DECLS

void
gtk_file_chooser_widget_set_save_entry (GtkFileChooserWidget *chooser,
                                        GtkWidget            *entry);

gboolean
gtk_file_chooser_widget_should_respond (GtkFileChooserWidget *chooser);

void
gtk_file_chooser_widget_initial_focus  (GtkFileChooserWidget *chooser);

GSList *
gtk_file_chooser_widget_get_selected_files (GtkFileChooserWidget *impl);

GtkSelectionModel *
gtk_file_chooser_widget_get_selection_model (GtkFileChooserWidget *chooser);

G_END_DECLS

