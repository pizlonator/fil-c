
/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Red Hat, Inc.
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

#include <gdk/gdk.h>

#include "gtkwindow.h"
#include "gtkpointerfocusprivate.h"

G_BEGIN_DECLS

void            _gtk_window_group_add_grab    (GtkWindowGroup *window_group,
                                               GtkWidget      *widget);
void            _gtk_window_group_remove_grab (GtkWindowGroup *window_group,
                                               GtkWidget      *widget);

void            _gtk_window_unset_focus_and_default (GtkWindow *window,
                                                     GtkWidget *widget);

void            _gtk_window_update_focus_visible (GtkWindow       *window,
                                                  guint            keyval,
                                                  GdkModifierType  state,
                                                  gboolean         visible);

void            _gtk_window_set_allocation         (GtkWindow     *window,
                                                    int            width,
                                                    int            height,
                                                    GtkAllocation *allocation_out);

typedef void (*GtkWindowKeysForeachFunc) (GtkWindow      *window,
                                          guint           keyval,
                                          GdkModifierType modifiers,
                                          gpointer        data);

gboolean gtk_window_emit_close_request (GtkWindow *window);

/* --- internal (GtkAcceleratable) --- */
void            _gtk_window_schedule_mnemonics_visible (GtkWindow *window);

void            _gtk_window_notify_keys_changed (GtkWindow *window);

void            _gtk_window_toggle_maximized (GtkWindow *window);

void            _gtk_window_request_csd (GtkWindow *window);

/* Window groups */

GtkWindowGroup *_gtk_window_get_window_group (GtkWindow *window);

void            _gtk_window_set_window_group (GtkWindow      *window,
                                              GtkWindowGroup *group);


GdkPaintable *    gtk_window_get_icon_for_size (GtkWindow *window,
                                                int        size);

/* Exported handles */

typedef void (*GtkWindowHandleExported)  (GtkWindow               *window,
                                          const char              *handle,
                                          gpointer                 user_data);

gboolean      gtk_window_export_handle   (GtkWindow               *window,
                                          GtkWindowHandleExported  callback,
                                          gpointer                 user_data);
void          gtk_window_unexport_handle (GtkWindow               *window,
                                          const char              *handle);

GtkWidget *      gtk_window_lookup_pointer_focus_widget (GtkWindow        *window,
                                                         GdkDevice        *device,
                                                         GdkEventSequence *sequence);
GtkWidget *      gtk_window_lookup_effective_pointer_focus_widget (GtkWindow        *window,
                                                                   GdkDevice        *device,
                                                                   GdkEventSequence *sequence);
GtkWidget *      gtk_window_lookup_pointer_focus_implicit_grab (GtkWindow        *window,
                                                                GdkDevice        *device,
                                                                GdkEventSequence *sequence);

void             gtk_window_update_pointer_focus (GtkWindow        *window,
                                                  GdkDevice        *device,
                                                  GdkEventSequence *sequence,
                                                  GtkWidget        *target,
                                                  double            x,
                                                  double            y);
void             gtk_window_set_pointer_focus_grab (GtkWindow        *window,
                                                    GdkDevice        *device,
                                                    GdkEventSequence *sequence,
                                                    GtkWidget        *grab_widget);

void             gtk_window_update_pointer_focus_on_state_change (GtkWindow *window,
                                                                  GtkWidget *widget);

void             gtk_window_maybe_revoke_implicit_grab (GtkWindow *window,
                                                        GdkDevice *device,
                                                        GtkWidget *grab_widget);
void             gtk_window_maybe_update_cursor (GtkWindow *window,
                                                 GtkWidget *widget,
                                                 GdkDevice *device);
GtkWidget *      gtk_window_pick_popover (GtkWindow   *window,
                                          double       x,
                                          double       y,
                                          GtkPickFlags flags);
GdkDevice** gtk_window_get_foci_on_widget (GtkWindow *window,
                                           GtkWidget *widget,
                                           guint     *n_devices);
void gtk_window_grab_notify (GtkWindow *window,
                             GtkWidget *old_grab_widget,
                             GtkWidget *new_grab_widget,
                             gboolean   from_grab);

G_END_DECLS

