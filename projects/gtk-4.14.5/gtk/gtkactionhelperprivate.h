/*
 * Copyright © 2012 Canonical Limited
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * licence or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Ryan Lortie <desrt@desrt.ca>
 */

#pragma once

#include <gtk/gtkapplication.h>
#include <gtk/gtkactionable.h>
#include <gtk/gtkmodelbuttonprivate.h>

#define GTK_TYPE_ACTION_HELPER                              (gtk_action_helper_get_type ())
#define GTK_ACTION_HELPER(inst)                             (G_TYPE_CHECK_INSTANCE_CAST ((inst),                      \
                                                             GTK_TYPE_ACTION_HELPER, GtkActionHelper))
#define GTK_IS_ACTION_HELPER(inst)                          (G_TYPE_CHECK_INSTANCE_TYPE ((inst),                      \
                                                             GTK_TYPE_ACTION_HELPER))

typedef struct _GtkActionHelper                             GtkActionHelper;

GType                   gtk_action_helper_get_type                      (void);

GtkActionHelper *       gtk_action_helper_new                           (GtkActionable   *widget);

void                    gtk_action_helper_set_action_name               (GtkActionHelper *helper,
                                                                         const char      *action_name);
void                    gtk_action_helper_set_action_target_value       (GtkActionHelper *helper,
                                                                         GVariant        *action_target);
const char *           gtk_action_helper_get_action_name               (GtkActionHelper *helper);
GVariant *              gtk_action_helper_get_action_target_value       (GtkActionHelper *helper);

gboolean                gtk_action_helper_get_enabled                   (GtkActionHelper *helper);
gboolean                gtk_action_helper_get_active                    (GtkActionHelper *helper);

void                    gtk_action_helper_activate                      (GtkActionHelper *helper);

GtkButtonRole           gtk_action_helper_get_role                      (GtkActionHelper *helper);


