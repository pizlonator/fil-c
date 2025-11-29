/* GTK - The GIMP Toolkit
 * Copyright (C) 2014 Benjamin Otte <otte@gnome.org>
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

#include "config.h"

#include "gtkcsstransientnodeprivate.h"
#include "gtkprivate.h"

G_DEFINE_TYPE (GtkCssTransientNode, gtk_css_transient_node, GTK_TYPE_CSS_NODE)

static GtkCssStyle *
gtk_css_transient_node_update_style (GtkCssNode                   *cssnode,
                                     const GtkCountingBloomFilter *filter,
                                     GtkCssChange                  change,
                                     gint64                        timestamp,
                                     GtkCssStyle                  *style)
{
  /* This should get rid of animations */
  return GTK_CSS_NODE_CLASS (gtk_css_transient_node_parent_class)->update_style (cssnode, filter, change, 0, style);
}

static void
gtk_css_transient_node_class_init (GtkCssTransientNodeClass *klass)
{
  GtkCssNodeClass *node_class = GTK_CSS_NODE_CLASS (klass);

  node_class->update_style = gtk_css_transient_node_update_style;
}

static void
gtk_css_transient_node_init (GtkCssTransientNode *cssnode)
{
  gtk_css_node_set_visible (GTK_CSS_NODE (cssnode), FALSE);
}

GtkCssNode *
gtk_css_transient_node_new (GtkCssNode *parent)
{
  GtkCssNode *result;

  gtk_internal_return_val_if_fail (GTK_IS_CSS_NODE (parent), NULL);

  result = g_object_new (GTK_TYPE_CSS_TRANSIENT_NODE, NULL);
  gtk_css_node_declaration_unref (result->decl);
  result->decl = gtk_css_node_declaration_ref (parent->decl);

  return result;
}

