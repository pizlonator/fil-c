/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright 2022 Christopher Davis <christopherdavis@gnome.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "cc-default-apps-row.h"

struct _CcDefaultAppsRow
{
  AdwComboRow parent_instance;

  char *content_type;
  char *filters;

  GListStore *model;
};

G_DEFINE_FINAL_TYPE (CcDefaultAppsRow, cc_default_apps_row, ADW_TYPE_COMBO_ROW)

enum {
  PROP_0,
  PROP_CONTENT_TYPE,
  PROP_FILTERS,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static char *
get_app_display_name (GAppInfo *info)
{
  return g_strdup (g_app_info_get_display_name (info));
}

static void
cc_default_apps_row_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  CcDefaultAppsRow *self = CC_DEFAULT_APPS_ROW (object);

  switch (prop_id)
    {
    case PROP_CONTENT_TYPE:
      g_value_set_string (value, self->content_type);
      break;
    case PROP_FILTERS:
      g_value_set_string (value, self->filters);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_default_apps_row_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  CcDefaultAppsRow *self = CC_DEFAULT_APPS_ROW (object);

  switch (prop_id)
    {
    case PROP_CONTENT_TYPE:
      self->content_type = g_strdup (g_value_get_string (value));
      break;
    case PROP_FILTERS:
      self->filters = g_strdup (g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_default_apps_row_constructed (GObject *object)
{
  CcDefaultAppsRow *self;
  g_autoptr(GAppInfo) default_app = NULL;
  g_autolist(GAppInfo) recommended_apps = NULL;
  GList *l;
  g_autoptr(GtkExpression) name_expr = NULL;

  G_OBJECT_CLASS (cc_default_apps_row_parent_class)->constructed (object);

  self = CC_DEFAULT_APPS_ROW (object);
  default_app = g_app_info_get_default_for_type (self->content_type, FALSE);
  recommended_apps = g_app_info_get_recommended_for_type (self->content_type);
  self->model = g_list_store_new (G_TYPE_APP_INFO);

  /* Add the default separately because it may not be in the list of recommended apps */
  if (G_IS_APP_INFO (default_app))
    g_list_store_append (self->model, default_app);

  for (l = recommended_apps; l != NULL; l = l->next) {
    GAppInfo *app = l->data;

    if (!G_IS_APP_INFO (app) || (default_app != NULL && g_app_info_equal (app, default_app)))
      continue;

    g_list_store_append (self->model, app);
  }

  adw_combo_row_set_model (ADW_COMBO_ROW (self), G_LIST_MODEL (self->model));

  name_expr = gtk_cclosure_expression_new (G_TYPE_STRING, NULL,
                                           0, NULL,
                                           G_CALLBACK (get_app_display_name),
                                           NULL, NULL);
  adw_combo_row_set_expression (ADW_COMBO_ROW (self), name_expr);
}

static void
cc_default_apps_row_class_init (CcDefaultAppsRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = cc_default_apps_row_get_property;
  object_class->set_property = cc_default_apps_row_set_property;
  object_class->constructed = cc_default_apps_row_constructed;

  properties[PROP_CONTENT_TYPE] =
    g_param_spec_string ("content-type",
                         NULL, NULL, NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  properties[PROP_FILTERS] =
    g_param_spec_string ("filters",
                         NULL, NULL, NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
cc_default_apps_row_init (CcDefaultAppsRow *self)
{

}

void
cc_default_apps_row_update_default_app (CcDefaultAppsRow *self)
{
  GAppInfo *info;
  g_autoptr(GError) error = NULL;
  int i;

  info = G_APP_INFO (adw_combo_row_get_selected_item (ADW_COMBO_ROW (self)));

  if (!info)
    return;

  if (g_app_info_set_as_default_for_type (info, self->content_type, &error) == FALSE)
    {
      g_warning ("Failed to set '%s' as the default app for '%s': %s",
                 g_app_info_get_name (info), self->content_type, error->message);
    }
  else
    {
      g_debug ("Set '%s' as the default handler for '%s'",
               g_app_info_get_name (info), self->content_type);
    }

  if (self->filters)
    {
      g_auto(GStrv) entries = NULL;
      const char *const *mime_types;
      g_autoptr(GPtrArray) patterns = NULL;

      entries = g_strsplit (self->filters, ";", -1);
      patterns = g_ptr_array_new_with_free_func ((GDestroyNotify) g_pattern_spec_free);
      for (i = 0; entries[i] != NULL; i++)
        {
          GPatternSpec *pattern = g_pattern_spec_new (entries[i]);
          g_ptr_array_add (patterns, pattern);
        }

      mime_types = g_app_info_get_supported_types (info);
      for (i = 0; mime_types && mime_types[i]; i++)
        {
          int j;
          gboolean matched = FALSE;
          g_autoptr(GError) local_error = NULL;

          for (j = 0; j < patterns->len; j++)
            {
              GPatternSpec *pattern = g_ptr_array_index (patterns, j);
              if (g_pattern_spec_match_string (pattern, mime_types[i]))
                matched = TRUE;
            }
          if (!matched)
            continue;

          if (g_app_info_set_as_default_for_type (info, mime_types[i], &local_error) == FALSE)
            {
              g_warning ("Failed to set '%s' as the default app for secondary "
                         "content type '%s': %s",
                         g_app_info_get_name (info), mime_types[i], local_error->message);
            }
          else
            {
              g_debug ("Set '%s' as the default handler for '%s'",
              g_app_info_get_name (info), mime_types[i]);
            }
        }
    }
}
