/*
 * Copyright © 2019 Benjamin Otte
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#pragma once

#include <gtk/gtkbuilderscope.h>

G_BEGIN_DECLS

GType                   gtk_builder_scope_get_type_from_name    (GtkBuilderScope        *self,
                                                                 GtkBuilder             *builder,
                                                                 const char             *type_name);
GType                   gtk_builder_scope_get_type_from_function(GtkBuilderScope        *self,
                                                                 GtkBuilder             *builder,
                                                                 const char             *function_name);
GClosure *              gtk_builder_scope_create_closure        (GtkBuilderScope        *self,
                                                                 GtkBuilder             *builder,
                                                                 const char             *function_name,
                                                                 GtkBuilderClosureFlags  flags,
                                                                 GObject                *object,
                                                                 GError                **error);


G_END_DECLS

