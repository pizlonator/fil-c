/* gtkaccessibleattributesetprivate.h: Accessible attribute container
 *
 * Copyright 2020  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "gtkaccessible.h"
#include "gtkaccessiblevalueprivate.h"

G_BEGIN_DECLS

typedef struct _GtkAccessibleAttributeSet       GtkAccessibleAttributeSet;

typedef const char *(* GtkAccessibleAttributeNameFunc) (int attribute);
typedef GtkAccessibleValue *(* GtkAccessibleAttributeDefaultFunc) (int attribute);

GtkAccessibleAttributeSet *     gtk_accessible_attribute_set_new                (gsize                             n_attributes,
                                                                                 GtkAccessibleAttributeNameFunc    name_func,
                                                                                 GtkAccessibleAttributeDefaultFunc default_func);
GtkAccessibleAttributeSet *     gtk_accessible_attribute_set_ref                (GtkAccessibleAttributeSet  *self);
void                            gtk_accessible_attribute_set_unref              (GtkAccessibleAttributeSet  *self);

gsize                           gtk_accessible_attribute_set_get_length         (GtkAccessibleAttributeSet  *self);

gboolean                        gtk_accessible_attribute_set_add                (GtkAccessibleAttributeSet  *self,
                                                                                 int                         attribute,
                                                                                 GtkAccessibleValue         *value);
gboolean                        gtk_accessible_attribute_set_remove             (GtkAccessibleAttributeSet  *self,
                                                                                 int                         state);
gboolean                        gtk_accessible_attribute_set_contains           (GtkAccessibleAttributeSet  *self,
                                                                                 int                         state);
GtkAccessibleValue *            gtk_accessible_attribute_set_get_value          (GtkAccessibleAttributeSet  *self,
                                                                                 int                         state);

guint                           gtk_accessible_attribute_set_get_changed        (GtkAccessibleAttributeSet   *self);

void                            gtk_accessible_attribute_set_print              (GtkAccessibleAttributeSet  *self,
                                                                                 gboolean                    only_set,
                                                                                 GString                    *string);
char *                          gtk_accessible_attribute_set_to_string          (GtkAccessibleAttributeSet  *self);

G_END_DECLS
