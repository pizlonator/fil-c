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
 * Author: Matthias Clasen
 */

#pragma once

#include "gtksearchengineprivate.h"
#include "gtkfilesystemmodelprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_SEARCH_ENGINE_MODEL		(_gtk_search_engine_model_get_type ())
#define GTK_SEARCH_ENGINE_MODEL(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SEARCH_ENGINE_MODEL, GtkSearchEngineModel))
#define GTK_SEARCH_ENGINE_MODEL_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_SEARCH_ENGINE_MODEL, GtkSearchEngineModelClass))
#define GTK_IS_SEARCH_ENGINE_MODEL(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SEARCH_ENGINE_MODEL))
#define GTK_IS_SEARCH_ENGINE_MODEL_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SEARCH_ENGINE_MODEL))
#define GTK_SEARCH_ENGINE_MODEL_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_SEARCH_ENGINE_MODEL, GtkSearchEngineModelClass))

typedef struct _GtkSearchEngineModel GtkSearchEngineModel;
typedef struct _GtkSearchEngineModelClass GtkSearchEngineModelClass;

GType            _gtk_search_engine_model_get_type (void);

GtkSearchEngine *_gtk_search_engine_model_new      (GtkFileSystemModel *model);

G_END_DECLS

