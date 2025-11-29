/*
 * Copyright © 2014 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Matthias Clasen
 */

#pragma once

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_MODEL_BUTTON         (gtk_model_button_get_type ())
#define GTK_MODEL_BUTTON(inst)        (G_TYPE_CHECK_INSTANCE_CAST ((inst),                      \
                                      GTK_TYPE_MODEL_BUTTON, GtkModelButton))
#define GTK_IS_MODEL_BUTTON(inst)     (G_TYPE_CHECK_INSTANCE_TYPE ((inst),                      \
                                      GTK_TYPE_MODEL_BUTTON))

typedef struct _GtkModelButton        GtkModelButton;

/**
 * GtkButtonRole:
 * @GTK_BUTTON_ROLE_NORMAL: A plain button
 * @GTK_BUTTON_ROLE_CHECK: A check button
 * @GTK_BUTTON_ROLE_RADIO: A radio button
 * @GTK_BUTTON_ROLE_TITLE: The title for a submenu
 *
 * The role specifies the desired appearance of a GtkModelButton.
 */
typedef enum {
  GTK_BUTTON_ROLE_NORMAL,
  GTK_BUTTON_ROLE_CHECK,
  GTK_BUTTON_ROLE_RADIO,
  GTK_BUTTON_ROLE_TITLE
} GtkButtonRole;

#define GTK_TYPE_BUTTON_ROLE         (gtk_button_role_get_type ())
GType       gtk_button_role_get_type (void) G_GNUC_CONST;

GType       gtk_model_button_get_type (void) G_GNUC_CONST;

GtkWidget * gtk_model_button_new      (void);

G_END_DECLS

