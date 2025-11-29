/*
 * Copyright (c) 2008-2009  Christian Hammond
 * Copyright (c) 2008-2009  David Trowbridge
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <glib.h>

#include "init.h"

#include "a11y.h"
#include "actions.h"
#include "graphrenderer.h"
#include "clipboard.h"
#include "controllers.h"
#include "css-editor.h"
#include "css-node-tree.h"
#include "general.h"
#include "graphdata.h"
#include "list-data.h"
#include "logs.h"
#include "magnifier.h"
#include "measuregraph.h"
#include "menu.h"
#include "misc-info.h"
#include "object-tree.h"
#include "prop-list.h"
#include "recorder.h"
#include "resource-list.h"
#include "shortcuts.h"
#include "size-groups.h"
#include "statistics.h"
#include "tree-data.h"
#include "visual.h"
#include "window.h"

#include "gtkmagnifierprivate.h"
#include "gtkcssproviderprivate.h"

#include "gtkmodulesprivate.h"

static GIOExtensionPoint *extension_point = NULL;

void
gtk_inspector_init (void)
{
  GIOModuleScope *scope;
  char **paths;
  int i;

  g_type_ensure (G_TYPE_LIST_STORE);

  g_type_ensure (graph_data_get_type ());
  g_type_ensure (graph_renderer_get_type ());
  g_type_ensure (GTK_TYPE_INSPECTOR_A11Y);
  g_type_ensure (GTK_TYPE_INSPECTOR_ACTIONS);
  g_type_ensure (GTK_TYPE_INSPECTOR_CLIPBOARD);
  g_type_ensure (GTK_TYPE_INSPECTOR_CONTROLLERS);
  g_type_ensure (GTK_TYPE_INSPECTOR_CSS_EDITOR);
  g_type_ensure (GTK_TYPE_INSPECTOR_CSS_NODE_TREE);
  g_type_ensure (GTK_TYPE_INSPECTOR_GENERAL);
  g_type_ensure (GTK_TYPE_INSPECTOR_LIST_DATA);
  g_type_ensure (GTK_TYPE_INSPECTOR_LOGS);
  g_type_ensure (GTK_TYPE_MAGNIFIER);
  g_type_ensure (GTK_TYPE_INSPECTOR_MAGNIFIER);
  g_type_ensure (GTK_TYPE_INSPECTOR_MEASURE_GRAPH);
  g_type_ensure (GTK_TYPE_INSPECTOR_MENU);
  g_type_ensure (GTK_TYPE_INSPECTOR_MISC_INFO);
  g_type_ensure (GTK_TYPE_INSPECTOR_OBJECT_TREE);
  g_type_ensure (GTK_TYPE_INSPECTOR_PROP_LIST);
  g_type_ensure (GTK_TYPE_INSPECTOR_RECORDER);
  g_type_ensure (GTK_TYPE_INSPECTOR_RESOURCE_LIST);
  g_type_ensure (GTK_TYPE_INSPECTOR_SHORTCUTS);
  g_type_ensure (GTK_TYPE_INSPECTOR_SIZE_GROUPS);
  g_type_ensure (GTK_TYPE_INSPECTOR_STATISTICS);
  g_type_ensure (GTK_TYPE_INSPECTOR_TREE_DATA);
  g_type_ensure (GTK_TYPE_INSPECTOR_VISUAL);
  g_type_ensure (GTK_TYPE_INSPECTOR_WINDOW);

  paths = _gtk_get_module_path ("inspector");
  scope = g_io_module_scope_new (G_IO_MODULE_SCOPE_BLOCK_DUPLICATES);

  for (i = 0; paths[i] != NULL; i++)
    g_io_modules_load_all_in_directory_with_scope (paths[i], scope);

  g_strfreev (paths);
  g_io_module_scope_free (scope);

  gtk_css_provider_set_keep_css_sections ();
}

void
gtk_inspector_register_extension (void)
{
  if (extension_point == NULL)
    {
      extension_point = g_io_extension_point_register ("gtk-inspector-page");
      g_io_extension_point_set_required_type (extension_point, GTK_TYPE_WIDGET);
    }
}

// vim: set et sw=2 ts=2:
