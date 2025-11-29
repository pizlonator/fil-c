/*
 * Copyright (C) 2005 Novell, Inc.
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
 * Author: Anders Carlsson <andersca@imendio.com> 
 *
 * Based on nautilus-search-engine.h
 */

#pragma once

#include "gtkquery.h"
#include "gtkfilesystemmodelprivate.h"
#include <gio/gio.h>

G_BEGIN_DECLS

#define GTK_TYPE_SEARCH_ENGINE		(_gtk_search_engine_get_type ())
#define GTK_SEARCH_ENGINE(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SEARCH_ENGINE, GtkSearchEngine))
#define GTK_SEARCH_ENGINE_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_SEARCH_ENGINE, GtkSearchEngineClass))
#define GTK_IS_SEARCH_ENGINE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SEARCH_ENGINE))
#define GTK_IS_SEARCH_ENGINE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SEARCH_ENGINE))
#define GTK_SEARCH_ENGINE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_SEARCH_ENGINE, GtkSearchEngineClass))

typedef struct _GtkSearchEngine GtkSearchEngine;
typedef struct _GtkSearchEngineClass GtkSearchEngineClass;
typedef struct _GtkSearchEnginePrivate GtkSearchEnginePrivate;
typedef struct _GtkSearchHit GtkSearchHit;

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GtkSearchEngine, g_object_unref)

struct _GtkSearchHit
{
  GFile *file;
  GFileInfo *info; /* may be NULL */
};

struct _GtkSearchEngine
{
  GObject parent;

  GtkSearchEnginePrivate *priv;
};

struct _GtkSearchEngineClass 
{
  GObjectClass parent_class;
  
  /* VTable */
  void     (*set_query)       (GtkSearchEngine *engine, 
			       GtkQuery        *query);
  void     (*start)           (GtkSearchEngine *engine);
  void     (*stop)            (GtkSearchEngine *engine);
  
  /* Signals */
  void     (*hits_added)      (GtkSearchEngine *engine, 
			       GList           *hits);
  void     (*finished)        (GtkSearchEngine *engine);
  void     (*error)           (GtkSearchEngine *engine, 
			       const char      *error_message);
};

GType            _gtk_search_engine_get_type        (void);

GtkSearchEngine* _gtk_search_engine_new             (void);

void             _gtk_search_engine_set_query       (GtkSearchEngine *engine, 
                                                     GtkQuery        *query);
void	         _gtk_search_engine_start           (GtkSearchEngine *engine);
void	         _gtk_search_engine_stop            (GtkSearchEngine *engine);

void	         _gtk_search_engine_hits_added      (GtkSearchEngine *engine, 
						     GList           *hits);
void             _gtk_search_engine_finished        (GtkSearchEngine *engine,
                                                     gboolean         got_results);
void	         _gtk_search_engine_error           (GtkSearchEngine *engine, 
						     const char      *error_message);

void             _gtk_search_hit_free (GtkSearchHit *hit);
GtkSearchHit    *_gtk_search_hit_dup (GtkSearchHit *hit);

void             _gtk_search_engine_set_model       (GtkSearchEngine    *engine,
                                                     GtkFileSystemModel *model);

G_END_DECLS

