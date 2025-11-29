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

#include <gtk/gtk.h>

#include "node-editor-application.h"

#define NODE_EDITOR_WINDOW_TYPE (node_editor_window_get_type ())
#define NODE_EDITOR_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), NODE_EDITOR_WINDOW_TYPE, NodeEditorWindow))


typedef struct _NodeEditorWindow         NodeEditorWindow;
typedef struct _NodeEditorWindowClass    NodeEditorWindowClass;


GType                   node_editor_window_get_type     (void);

NodeEditorWindow *      node_editor_window_new          (NodeEditorApplication  *application);

gboolean                node_editor_window_load         (NodeEditorWindow       *self,
                                                         GFile                  *file);

char *                  get_autosave_path               (const char             *suffix);
