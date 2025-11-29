/*
 * Copyright © 2014 Benjamin Otte <otte@gnome.org>
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
 */

#pragma once

#include "gtkcountingbloomfilterprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkenums.h"

G_BEGIN_DECLS

GtkCssNodeDeclaration * gtk_css_node_declaration_new                    (void);
GtkCssNodeDeclaration * gtk_css_node_declaration_ref                    (GtkCssNodeDeclaration         *decl);
void                    gtk_css_node_declaration_unref                  (GtkCssNodeDeclaration         *decl);

gboolean                gtk_css_node_declaration_set_name               (GtkCssNodeDeclaration        **decl,
                                                                         GQuark                         name);
GQuark                  gtk_css_node_declaration_get_name               (const GtkCssNodeDeclaration   *decl);
gboolean                gtk_css_node_declaration_set_id                 (GtkCssNodeDeclaration        **decl,
                                                                         GQuark                         id);
GQuark                  gtk_css_node_declaration_get_id                 (const GtkCssNodeDeclaration   *decl);
gboolean                gtk_css_node_declaration_set_state              (GtkCssNodeDeclaration        **decl,
                                                                         GtkStateFlags                  flags);
GtkStateFlags           gtk_css_node_declaration_get_state              (const GtkCssNodeDeclaration   *decl);

gboolean                gtk_css_node_declaration_add_class              (GtkCssNodeDeclaration        **decl,
                                                                         GQuark                         class_quark);
gboolean                gtk_css_node_declaration_remove_class           (GtkCssNodeDeclaration        **decl,
                                                                         GQuark                         class_quark);
gboolean                gtk_css_node_declaration_clear_classes          (GtkCssNodeDeclaration        **decl);
gboolean                gtk_css_node_declaration_has_class              (const GtkCssNodeDeclaration   *decl,
                                                                         GQuark                         class_quark);
const GQuark *          gtk_css_node_declaration_get_classes            (const GtkCssNodeDeclaration   *decl,
                                                                         guint                         *n_classes);

void                    gtk_css_node_declaration_add_bloom_hashes       (const GtkCssNodeDeclaration   *decl,
                                                                         GtkCountingBloomFilter        *filter);
void                    gtk_css_node_declaration_remove_bloom_hashes    (const GtkCssNodeDeclaration   *decl,
                                                                         GtkCountingBloomFilter        *filter);

guint                   gtk_css_node_declaration_hash                   (gconstpointer                  elem);
gboolean                gtk_css_node_declaration_equal                  (gconstpointer                  elem1,
                                                                         gconstpointer                  elem2);

void                    gtk_css_node_declaration_print                  (const GtkCssNodeDeclaration   *decl,
                                                                         GString                       *string);

char *                  gtk_css_node_declaration_to_string              (const GtkCssNodeDeclaration   *decl);

G_END_DECLS

