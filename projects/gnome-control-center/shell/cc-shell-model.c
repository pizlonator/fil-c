/*
 * Copyright (c) 2009, 2010 Intel, Inc.
 * Copyright (c) 2010 Red Hat, Inc.
 *
 * The Control Center is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * The Control Center is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with the Control Center; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Thomas Wood <thos@gnome.org>
 */

#include "cc-shell-model.h"
#include "cc-util.h"

#include <string.h>

#include <gio/gdesktopappinfo.h>

struct _CcShellModel
{
  GtkListStore parent;

  GStrv        sort_terms;
};

G_DEFINE_TYPE (CcShellModel, cc_shell_model, GTK_TYPE_LIST_STORE)

static gint
sort_by_name (GtkTreeModel *model,
              GtkTreeIter  *a,
              GtkTreeIter  *b)
{
  g_autofree gchar *a_name = NULL;
  g_autofree gchar *b_name = NULL;

  gtk_tree_model_get (model, a, COL_CASEFOLDED_NAME, &a_name, -1);
  gtk_tree_model_get (model, b, COL_CASEFOLDED_NAME, &b_name, -1);

  return g_strcmp0 (a_name, b_name);
}

static gint
sort_by_name_with_terms (GtkTreeModel  *model,
                         GtkTreeIter   *a,
                         GtkTreeIter   *b,
                         gchar        **terms)
{
  gboolean a_match, b_match;
  g_autofree gchar *a_name = NULL;
  g_autofree gchar *b_name = NULL;
  gint i;

  gtk_tree_model_get (model, a, COL_CASEFOLDED_NAME, &a_name, -1);
  gtk_tree_model_get (model, b, COL_CASEFOLDED_NAME, &b_name, -1);

  for (i = 0; terms[i]; ++i)
    {
      a_match = strstr (a_name, terms[i]) != NULL;
      b_match = strstr (b_name, terms[i]) != NULL;

      if (a_match && !b_match)
        return -1;
      else if (!a_match && b_match)
        return 1;
    }

  return 0;
}

static gint
count_matches (gchar **keywords,
               gchar **terms)
{
  gint i, j, c;

  if (!keywords || !terms)
    return 0;

  c = 0;

  for (i = 0; terms[i]; ++i)
    for (j = 0; keywords[j]; ++j)
      if (strstr (keywords[j], terms[i]))
        c += 1;

  return c;
}

static gint
sort_by_keywords_with_terms (GtkTreeModel  *model,
                             GtkTreeIter   *a,
                             GtkTreeIter   *b,
                             gchar        **terms)
{
  gint a_matches, b_matches;
  g_auto(GStrv) a_keywords = NULL;
  g_auto(GStrv) b_keywords = NULL;

  gtk_tree_model_get (model, a, COL_KEYWORDS, &a_keywords, -1);
  gtk_tree_model_get (model, b, COL_KEYWORDS, &b_keywords, -1);

  a_matches = count_matches (a_keywords, terms);
  b_matches = count_matches (b_keywords, terms);

  if (a_matches > b_matches)
    return -1;
  else if (a_matches < b_matches)
    return 1;

  return 0;
}

static gint
sort_by_description_with_terms (GtkTreeModel  *model,
                                GtkTreeIter   *a,
                                GtkTreeIter   *b,
                                gchar        **terms)
{
  gint a_matches, b_matches;
  g_autofree gchar *a_description = NULL;
  g_autofree gchar *b_description = NULL;
  g_auto(GStrv) a_description_split = NULL;
  g_auto(GStrv) b_description_split = NULL;

  gtk_tree_model_get (model, a, COL_DESCRIPTION, &a_description, -1);
  gtk_tree_model_get (model, b, COL_DESCRIPTION, &b_description, -1);

  if (a_description && !b_description)
    return -1;
  else if (!a_description && b_description)
    return 1;
  else if (!a_description && !b_description)
    return 0;

  a_description_split = g_strsplit (a_description, " ", -1);
  b_description_split = g_strsplit (b_description, " ", -1);

  a_matches = count_matches (a_description_split, terms);
  b_matches = count_matches (b_description_split, terms);

  if (a_matches > b_matches)
    return -1;
  else if (a_matches < b_matches)
    return 1;

  return 0;
}

static gint
sort_with_terms (GtkTreeModel  *model,
                 GtkTreeIter   *a,
                 GtkTreeIter   *b,
                 gchar        **terms)
{
  gint rval;

  rval = sort_by_name_with_terms (model, a, b, terms);
  if (rval)
    return rval;

  rval = sort_by_keywords_with_terms (model, a, b, terms);
  if (rval)
    return rval;

  rval = sort_by_description_with_terms (model, a, b, terms);
  if (rval)
    return rval;

  return sort_by_name (model, a, b);
}

static gint
cc_shell_model_sort_func (GtkTreeModel *model,
                          GtkTreeIter  *a,
                          GtkTreeIter  *b,
                          gpointer      data)
{
  CcShellModel *self = data;

  if (!self->sort_terms || !self->sort_terms[0])
    return sort_by_name (model, a, b);
  else
    return sort_with_terms (model, a, b, self->sort_terms);
}

static void
cc_shell_model_finalize (GObject *object)
{
  CcShellModel *self = CC_SHELL_MODEL (object);

  g_clear_pointer (&self->sort_terms, g_strfreev);

  G_OBJECT_CLASS (cc_shell_model_parent_class)->finalize (object);
}

static void
cc_shell_model_class_init (CcShellModelClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = cc_shell_model_finalize;
}

static void
cc_shell_model_init (CcShellModel *self)
{
  GType types[] = {G_TYPE_STRING, G_TYPE_STRING, G_TYPE_APP_INFO, G_TYPE_STRING, G_TYPE_UINT,
                   G_TYPE_STRING, G_TYPE_STRING, G_TYPE_ICON, G_TYPE_STRV, G_TYPE_UINT, G_TYPE_BOOLEAN };

  gtk_list_store_set_column_types (GTK_LIST_STORE (self),
                                   N_COLS, types);

  gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (self),
                                           cc_shell_model_sort_func,
                                           self, NULL);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (self),
                                        GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
                                        GTK_SORT_ASCENDING);
}

CcShellModel *
cc_shell_model_new (void)
{
  return g_object_new (CC_TYPE_SHELL_MODEL, NULL);
}

static char **
get_casefolded_keywords (GAppInfo *appinfo)
{
  const char * const * keywords;
  char **casefolded_keywords;
  int i, n;

  keywords = g_desktop_app_info_get_keywords (G_DESKTOP_APP_INFO (appinfo));
  n = keywords ? g_strv_length ((char**) keywords) : 0;
  casefolded_keywords = g_new (char*, n+1);

  for (i = 0; i < n; i++)
    casefolded_keywords[i] = cc_util_normalize_casefold_and_unaccent (keywords[i]);
  casefolded_keywords[n] = NULL;

  return casefolded_keywords;
}

static GIcon *
symbolicize_g_icon (GIcon *gicon)
{
  const gchar * const *names;
  g_autofree gchar *new_name = NULL;

  if (!G_IS_THEMED_ICON (gicon))
    return g_object_ref (gicon);

  names = g_themed_icon_get_names (G_THEMED_ICON (gicon));

  if (g_str_has_suffix (names[0], "-symbolic"))
    return g_object_ref (gicon);

  new_name = g_strdup_printf ("%s-symbolic", names[0]);
  return g_themed_icon_new_with_default_fallbacks (new_name);
}

void
cc_shell_model_add_item (CcShellModel    *model,
                         CcPanelCategory  category,
                         GAppInfo        *appinfo,
                         const char      *id)
{
  g_autoptr(GIcon) icon = NULL;
  const gchar *name = g_app_info_get_name (appinfo);
  const gchar *comment = g_app_info_get_description (appinfo);
  g_auto(GStrv) keywords = NULL;
  g_autofree gchar *casefolded_name = NULL;
  g_autofree gchar *casefolded_description = NULL;

  casefolded_name = cc_util_normalize_casefold_and_unaccent (name);
  casefolded_description = cc_util_normalize_casefold_and_unaccent (comment);
  keywords = get_casefolded_keywords (appinfo);
  icon = symbolicize_g_icon (g_app_info_get_icon (appinfo));

  gtk_list_store_insert_with_values (GTK_LIST_STORE (model), NULL, 0,
                                     COL_NAME, name,
                                     COL_CASEFOLDED_NAME, casefolded_name,
                                     COL_APP, appinfo,
                                     COL_ID, id,
                                     COL_CATEGORY, category,
                                     COL_DESCRIPTION, comment,
                                     COL_CASEFOLDED_DESCRIPTION, casefolded_description,
                                     COL_GICON, icon,
                                     COL_KEYWORDS, keywords,
                                     COL_VISIBILITY, CC_PANEL_VISIBLE,
                                     -1);
}

gboolean
cc_shell_model_has_panel (CcShellModel *model,
                          const char   *id)
{
  GtkTreeIter iter;
  gboolean valid;

  g_assert (id);

  valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (model), &iter);
  while (valid)
    {
      g_autofree gchar *panel_id = NULL;

      gtk_tree_model_get (GTK_TREE_MODEL (model), &iter, COL_ID, &panel_id, -1);
      if (g_str_equal (id, panel_id))
        return TRUE;

      valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (model), &iter);
    }

  return FALSE;
}

gboolean
cc_shell_model_iter_matches_search (CcShellModel *model,
                                    GtkTreeIter  *iter,
                                    const char   *term)
{
  g_autofree gchar *name = NULL;
  g_autofree gchar *description = NULL;
  gboolean result;
  g_auto(GStrv) keywords = NULL;

  gtk_tree_model_get (GTK_TREE_MODEL (model), iter,
                      COL_CASEFOLDED_NAME, &name,
                      COL_CASEFOLDED_DESCRIPTION, &description,
                      COL_KEYWORDS, &keywords,
                      -1);

  result = (strstr (name, term) != NULL);

  if (!result && description)
    result = (strstr (description, term) != NULL);

  if (!result && keywords)
    {
      gint i;

      for (i = 0; !result && keywords[i]; i++)
        result = (strstr (keywords[i], term) == keywords[i]);
    }

  return result;
}

void
cc_shell_model_set_sort_terms (CcShellModel  *self,
                               gchar        **terms)
{
  g_return_if_fail (CC_IS_SHELL_MODEL (self));

  g_clear_pointer (&self->sort_terms, g_strfreev);
  self->sort_terms = g_strdupv (terms);

  /* trigger a re-sort */
  gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (self),
                                           cc_shell_model_sort_func,
                                           self,
                                           NULL);
}

void
cc_shell_model_set_panel_visibility (CcShellModel      *self,
                                     const gchar       *id,
                                     CcPanelVisibility  visibility)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean valid;

  g_return_if_fail (CC_IS_SHELL_MODEL (self));

  model = GTK_TREE_MODEL (self);

  /* Find the iter for the panel with the given id */
  valid = gtk_tree_model_get_iter_first (model, &iter);

  while (valid)
    {
      g_autofree gchar *item_id = NULL;

      gtk_tree_model_get (model, &iter, COL_ID, &item_id, -1);

      /* Found the iter */
      if (g_str_equal (id, item_id))
        break;

      /* If not found, continue */
      valid = gtk_tree_model_iter_next (model, &iter);
    }

  /* If we don't find any panel with the given id, we'll iterate until
   * valid == FALSE, so we can use this variable to determine if the
   * panel was found or not. It is a programming error to try to set
   * the visibility of a non-existent panel.
   */
  g_assert (valid);

  gtk_list_store_set (GTK_LIST_STORE (self), &iter, COL_VISIBILITY, visibility, -1);
}
