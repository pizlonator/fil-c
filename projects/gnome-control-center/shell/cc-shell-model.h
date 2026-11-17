/*
 * Copyright (c) 2010 Intel, Inc.
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

#pragma once

#include "cc-panel.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_SHELL_MODEL cc_shell_model_get_type()

G_DECLARE_FINAL_TYPE (CcShellModel, cc_shell_model, CC, SHELL_MODEL, GtkListStore)

typedef enum
{
  CC_CATEGORY_CONNECTIVITY,
  CC_CATEGORY_PERSONALIZATION,
  CC_CATEGORY_ACCOUNT,
  CC_CATEGORY_HARDWARE,
  CC_CATEGORY_DEVICES,
  CC_CATEGORY_DETAILS,
  CC_CATEGORY_SYSTEM,
  CC_CATEGORY_PRIVACY,
  CC_CATEGORY_LAST
} CcPanelCategory;

enum
{
  COL_NAME,
  COL_CASEFOLDED_NAME,
  COL_APP,
  COL_ID,
  COL_CATEGORY,
  COL_DESCRIPTION,
  COL_CASEFOLDED_DESCRIPTION,
  COL_GICON,
  COL_KEYWORDS,
  COL_VISIBILITY,

  N_COLS
};


CcShellModel* cc_shell_model_new                 (void);

void          cc_shell_model_add_item            (CcShellModel       *model,
                                                  CcPanelCategory     category,
                                                  GAppInfo           *appinfo,
                                                  const char         *id);

gboolean      cc_shell_model_has_panel           (CcShellModel       *model,
                                                  const char         *id);

gboolean      cc_shell_model_iter_matches_search (CcShellModel       *model,
                                                  GtkTreeIter        *iter,
                                                  const char         *term);

void          cc_shell_model_set_sort_terms       (CcShellModel      *model,
                                                   GStrv              terms);

void          cc_shell_model_set_panel_visibility (CcShellModel      *self,
                                                   const gchar       *id,
                                                   CcPanelVisibility  visible);

G_END_DECLS
