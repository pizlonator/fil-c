/* cc-panel-list.c
 *
 * Copyright (C) 2016 Endless, Inc
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
 * Author: Georges Basile Stavracas Neto <gbsneto@gnome.org>
 */

#pragma once

#include <glib-object.h>

#include "cc-panel.h"
#include "cc-shell-model.h"

G_BEGIN_DECLS

typedef enum
{
  CC_PANEL_LIST_MAIN,
  CC_PANEL_LIST_SEARCH
} CcPanelListView;

#define CC_TYPE_PANEL_LIST (cc_panel_list_get_type())

G_DECLARE_FINAL_TYPE (CcPanelList, cc_panel_list, CC, PANEL_LIST, AdwBin)

GtkWidget*           cc_panel_list_new                           (void);

gboolean             cc_panel_list_activate                      (CcPanelList        *self);

const gchar*         cc_panel_list_get_search_query              (CcPanelList        *self);

void                 cc_panel_list_set_search_query              (CcPanelList        *self,
                                                                  const gchar        *search);

CcPanelListView      cc_panel_list_get_view                      (CcPanelList        *self);

void                 cc_panel_list_add_panel                     (CcPanelList        *self,
                                                                  CcPanelCategory     category,
                                                                  const gchar        *id,
                                                                  const gchar        *title,
                                                                  const gchar        *description,
                                                                  const GStrv         keywords,
                                                                  const gchar        *icon,
                                                                  CcPanelVisibility   visibility);

const gchar*         cc_panel_list_get_current_panel             (CcPanelList        *self);

void                 cc_panel_list_set_active_panel               (CcPanelList       *self,
                                                                   const gchar       *id);

void                 cc_panel_list_set_panel_visibility          (CcPanelList        *self,
                                                                  const gchar        *id,
                                                                  CcPanelVisibility   visibility);

void                 cc_panel_list_set_selection_mode            (CcPanelList        *self,
                                                                  GtkSelectionMode    selection_mode);
void                 cc_panel_list_center_activated_row          (CcPanelList        *self,
                                                                  gboolean            val);

G_END_DECLS
