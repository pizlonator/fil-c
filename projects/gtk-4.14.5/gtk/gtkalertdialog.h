/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2022 Red Hat, Inc.
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

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gdk/gdk.h>
#include <gtk/gtkwindow.h>

G_BEGIN_DECLS

#define GTK_TYPE_ALERT_DIALOG (gtk_alert_dialog_get_type ())

GDK_AVAILABLE_IN_4_10
G_DECLARE_FINAL_TYPE (GtkAlertDialog, gtk_alert_dialog, GTK, ALERT_DIALOG, GObject)

GDK_AVAILABLE_IN_4_10
GtkAlertDialog *gtk_alert_dialog_new               (const char           *format,
                                                    ...) G_GNUC_PRINTF (1, 2);

GDK_AVAILABLE_IN_4_10
gboolean        gtk_alert_dialog_get_modal          (GtkAlertDialog      *self);

GDK_AVAILABLE_IN_4_10
void            gtk_alert_dialog_set_modal          (GtkAlertDialog      *self,
                                                     gboolean             modal);

GDK_AVAILABLE_IN_4_10
const char *    gtk_alert_dialog_get_message        (GtkAlertDialog      *self);

GDK_AVAILABLE_IN_4_10
void            gtk_alert_dialog_set_message        (GtkAlertDialog      *self,
                                                     const char          *message);

GDK_AVAILABLE_IN_4_10
const char *    gtk_alert_dialog_get_detail         (GtkAlertDialog      *self);

GDK_AVAILABLE_IN_4_10
void            gtk_alert_dialog_set_detail         (GtkAlertDialog      *self,
                                                     const char          *detail);

GDK_AVAILABLE_IN_4_10
const char * const *
                gtk_alert_dialog_get_buttons        (GtkAlertDialog      *self);

GDK_AVAILABLE_IN_4_10
void            gtk_alert_dialog_set_buttons        (GtkAlertDialog      *self,
                                                     const char * const  *labels);

GDK_AVAILABLE_IN_4_10
int             gtk_alert_dialog_get_cancel_button  (GtkAlertDialog      *self);

GDK_AVAILABLE_IN_4_10
void            gtk_alert_dialog_set_cancel_button  (GtkAlertDialog      *self,
                                                     int                  button);
GDK_AVAILABLE_IN_4_10
int             gtk_alert_dialog_get_default_button (GtkAlertDialog      *self);

GDK_AVAILABLE_IN_4_10
void            gtk_alert_dialog_set_default_button (GtkAlertDialog      *self,
                                                     int                  button);

GDK_AVAILABLE_IN_4_10
void            gtk_alert_dialog_choose             (GtkAlertDialog      *self,
                                                     GtkWindow           *parent,
                                                     GCancellable        *cancellable,
                                                     GAsyncReadyCallback  callback,
                                                     gpointer             user_data);

GDK_AVAILABLE_IN_4_10
int             gtk_alert_dialog_choose_finish      (GtkAlertDialog      *self,
                                                     GAsyncResult        *result,
                                                     GError             **error);

GDK_AVAILABLE_IN_4_10
void            gtk_alert_dialog_show               (GtkAlertDialog      *self,
                                                     GtkWindow           *parent);

G_END_DECLS
