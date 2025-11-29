/*
 * Copyright (C) 2015 Red Hat, Inc
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
 *
 * Author: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include <gio/gio.h>

#include <gdk/gdk.h>

#include "gtksearchenginemodelprivate.h"
#include "gtkfilechooserutils.h"
#include "gtkprivate.h"

#include <string.h>

struct _GtkSearchEngineModel
{
  GtkSearchEngine parent;

  GtkFileSystemModel *model;
  GtkQuery *query;

  guint idle;
};

struct _GtkSearchEngineModelClass
{
  GtkSearchEngineClass parent_class;
};

G_DEFINE_TYPE (GtkSearchEngineModel, _gtk_search_engine_model, GTK_TYPE_SEARCH_ENGINE)

static void
gtk_search_engine_model_dispose (GObject *object)
{
  GtkSearchEngineModel *model = GTK_SEARCH_ENGINE_MODEL (object);

  g_clear_object (&model->query);
  g_clear_object (&model->model);

  G_OBJECT_CLASS (_gtk_search_engine_model_parent_class)->dispose (object);
}

static gboolean
info_matches_query (GtkQuery  *query,
                    GFileInfo *info)
{
  const char *display_name;

  display_name = g_file_info_get_display_name (info);
  if (display_name == NULL)
    return FALSE;

  if (g_file_info_get_is_hidden (info))
    return FALSE;

  if (!gtk_query_matches_string (query, display_name))
    return FALSE;

  return TRUE;
}

static gboolean
do_search (gpointer data)
{
  GtkSearchEngineModel *model = data;
  GList *hits = NULL;
  gboolean got_results = FALSE;

  for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (model->model)); i++)
    {
      GFileInfo *info = g_list_model_get_item (G_LIST_MODEL (model->model), i);

      if (info_matches_query (model->query, info))
        {
          GFile *file;
          GtkSearchHit *hit;

          file = _gtk_file_info_get_file (info);
          hit = g_new (GtkSearchHit, 1);
          hit->file = g_object_ref (file);
          hit->info = g_object_ref (info);
          hits = g_list_prepend (hits, hit);
        }

      g_clear_object (&info);
    }

  if (hits)
    {
      _gtk_search_engine_hits_added (GTK_SEARCH_ENGINE (model), hits);
      g_list_free_full (hits, (GDestroyNotify)_gtk_search_hit_free);
      got_results = TRUE;
    }

  model->idle = 0;

  _gtk_search_engine_finished (GTK_SEARCH_ENGINE (model), got_results);

  return G_SOURCE_REMOVE;
}

static void
gtk_search_engine_model_start (GtkSearchEngine *engine)
{
  GtkSearchEngineModel *model;

  model = GTK_SEARCH_ENGINE_MODEL (engine);

  if (model->query == NULL)
    return;

  model->idle = g_idle_add (do_search, engine);
  gdk_source_set_static_name_by_id (model->idle, "[gtk] gtk_search_engine_model_start");
}

static void
gtk_search_engine_model_stop (GtkSearchEngine *engine)
{
  GtkSearchEngineModel *model;

  model = GTK_SEARCH_ENGINE_MODEL (engine);

  if (model->idle != 0)
    {
      g_source_remove (model->idle);
      model->idle = 0;
    }
}

static void
gtk_search_engine_model_set_query (GtkSearchEngine *engine,
                                   GtkQuery        *query)
{
  GtkSearchEngineModel *model = GTK_SEARCH_ENGINE_MODEL (engine);

  g_set_object (&model->query, query);
}

static void
_gtk_search_engine_model_class_init (GtkSearchEngineModelClass *class)
{
  GObjectClass *gobject_class;
  GtkSearchEngineClass *engine_class;

  gobject_class = G_OBJECT_CLASS (class);
  gobject_class->dispose = gtk_search_engine_model_dispose;

  engine_class = GTK_SEARCH_ENGINE_CLASS (class);
  engine_class->set_query = gtk_search_engine_model_set_query;
  engine_class->start = gtk_search_engine_model_start;
  engine_class->stop = gtk_search_engine_model_stop;
}

static void
_gtk_search_engine_model_init (GtkSearchEngineModel *engine)
{
}

GtkSearchEngine *
_gtk_search_engine_model_new (GtkFileSystemModel *model)
{
  GtkSearchEngineModel *engine;

  engine = GTK_SEARCH_ENGINE_MODEL (g_object_new (GTK_TYPE_SEARCH_ENGINE_MODEL, NULL));
  engine->model = g_object_ref (model);

  return GTK_SEARCH_ENGINE (engine);
}
