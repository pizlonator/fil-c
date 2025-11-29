/* GTK - The GIMP Toolkit
 * Copyright (C) 2015 Benjamin Otte <otte@gnome.org>
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

#include "gtkcssnodestylecacheprivate.h"

#include "gtkdebug.h"
#include "gtkcssstaticstyleprivate.h"

struct _GtkCssNodeStyleCache {
  guint        ref_count;
  GtkCssStyle *style;
  GHashTable  *children;
};

#define UNPACK_DECLARATION(packed) ((GtkCssNodeDeclaration *) (GPOINTER_TO_SIZE (packed) & ~0x3))
#define UNPACK_FLAGS(packed) (GPOINTER_TO_SIZE (packed) & 0x3)
#define PACK(decl, first_child, last_child) GSIZE_TO_POINTER (GPOINTER_TO_SIZE (decl) | ((first_child) ? 0x2 : 0) | ((last_child) ? 0x1 : 0))

GtkCssNodeStyleCache *
gtk_css_node_style_cache_new (GtkCssStyle *style)
{
  GtkCssNodeStyleCache *result;

  result = g_new0 (GtkCssNodeStyleCache, 1);

  result->ref_count = 1;
  result->style = g_object_ref (style);

  return result;
}

GtkCssNodeStyleCache *
gtk_css_node_style_cache_ref (GtkCssNodeStyleCache *cache)
{
  cache->ref_count++; 

  return cache;
}

void
gtk_css_node_style_cache_unref (GtkCssNodeStyleCache *cache)
{
  cache->ref_count--; 

  if (cache->ref_count > 0)
    return;

  g_object_unref (cache->style);
  if (cache->children)
    g_hash_table_unref (cache->children);

  g_free (cache);
}

GtkCssStyle *
gtk_css_node_style_cache_get_style (GtkCssNodeStyleCache *cache)
{
  return cache->style;
}

static gboolean
may_be_stored_in_cache (GtkCssStyle *style)
{
  GtkCssChange change;

  /* If you run your application with
   *   GTK_DEBUG=no-css-cache
   * no caching will happen. This is slow (in particular
   * when animating), but useful for figuring out bugs.
   *
   * We achieve that by disallowing any inserts into caches here.
   */
  if (GTK_DEBUG_CHECK (NO_CSS_CACHE))
    return FALSE;

  if (!GTK_IS_CSS_STATIC_STYLE (style))
    return FALSE;

  change = gtk_css_static_style_get_change (GTK_CSS_STATIC_STYLE (style));

  /* The cache is shared between all children of the parent, so if a
   * style depends on a sibling it is not independent of the child.
   */
  if (change & GTK_CSS_CHANGE_ANY_SIBLING)
    return FALSE;

  /* Again, the cache is shared between all children of the parent.
   * If the position is relevant, no child has the same style.
   */
  if (change & (GTK_CSS_CHANGE_NTH_CHILD | GTK_CSS_CHANGE_NTH_LAST_CHILD))
    return FALSE;

  return TRUE;
}

static guint
gtk_css_node_style_cache_decl_hash (gconstpointer item)
{
  return gtk_css_node_declaration_hash (UNPACK_DECLARATION (item)) << 2
    | UNPACK_FLAGS (item);
}

static gboolean
gtk_css_node_style_cache_decl_equal (gconstpointer item1,
                                     gconstpointer item2)
{
  if (UNPACK_FLAGS (item1) != UNPACK_FLAGS (item2))
    return FALSE;

  return gtk_css_node_declaration_equal (UNPACK_DECLARATION (item1),
                                         UNPACK_DECLARATION (item2));
}

static void
gtk_css_node_style_cache_decl_free (gpointer item)
{
  gtk_css_node_declaration_unref (UNPACK_DECLARATION (item));
}

GtkCssNodeStyleCache *
gtk_css_node_style_cache_insert (GtkCssNodeStyleCache   *parent,
                                 GtkCssNodeDeclaration  *decl,
                                 gboolean                is_first,
                                 gboolean                is_last,
                                 GtkCssStyle            *style)
{
  GtkCssNodeStyleCache *result;

  if (!may_be_stored_in_cache (style))
    return NULL;

  if (parent->children == NULL)
    parent->children = g_hash_table_new_full (gtk_css_node_style_cache_decl_hash,
                                              gtk_css_node_style_cache_decl_equal,
                                              gtk_css_node_style_cache_decl_free,
                                              (GDestroyNotify) gtk_css_node_style_cache_unref);

  result = gtk_css_node_style_cache_new (style);

  g_hash_table_insert (parent->children,
                       PACK (gtk_css_node_declaration_ref (decl), is_first, is_last),
                       gtk_css_node_style_cache_ref (result));

  return result;
}

GtkCssNodeStyleCache *
gtk_css_node_style_cache_lookup (GtkCssNodeStyleCache        *parent,
                                 const GtkCssNodeDeclaration *decl,
                                 gboolean                     is_first,
                                 gboolean                     is_last)
{
  GtkCssNodeStyleCache *result;

  if (parent->children == NULL)
    return NULL;

  result = g_hash_table_lookup (parent->children, PACK (decl, is_first, is_last));
  if (result == NULL)
    return NULL;

  return gtk_css_node_style_cache_ref (result);
}

