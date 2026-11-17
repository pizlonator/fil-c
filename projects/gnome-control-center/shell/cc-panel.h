/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2010 Red Hat, Inc.
 * Copyright (C) 2010 Intel, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: William Jon McCann <jmccann@redhat.com>
 *          Thomas Wood <thomas.wood@intel.com>
 */

#pragma once

#include <adwaita.h>

/**
 * Utility macro used to register panels
 *
 * use: CC_PANEL_REGISTER (PluginName, plugin_name)
 */
#define CC_PANEL_REGISTER(PluginName, plugin_name) G_DEFINE_TYPE (PluginName, plugin_name, CC_TYPE_PANEL)

/**
 * CcPanelStaticInitFunc:
 *
 * Function that statically allocates resources and initializes
 * any data that the panel will make use of during runtime.
 *
 * If panels represent hardware that can potentially not exist,
 * e.g. the Wi-Fi panel, these panels can use this function to
 * show or hide themselves without needing to have an instance
 * created and running.
 */
typedef void (*CcPanelStaticInitFunc) (void);


#define CC_TYPE_PANEL (cc_panel_get_type())
G_DECLARE_DERIVABLE_TYPE (CcPanel, cc_panel, CC, PANEL, AdwNavigationPage)

/**
 * CcPanelVisibility:
 *
 * @CC_PANEL_HIDDEN: Panel is hidden from search and sidebar, and not reachable.
 * @CC_PANEL_VISIBLE_IN_SEARCH: Panel is hidden from main view, but can be accessed from search.
 * @CC_PANEL_VISIBLE: Panel is visible everywhere.
 */
typedef enum
{
  CC_PANEL_HIDDEN,
  CC_PANEL_VISIBLE_IN_SEARCH,
  CC_PANEL_VISIBLE,
} CcPanelVisibility;

/* cc-shell.h requires CcPanel, so make sure it is defined first */
#include "cc-shell.h"

G_BEGIN_DECLS

/**
 * CcPanelClass:
 *
 * The contents of this struct are private and should not be accessed directly.
 */
struct _CcPanelClass
{
  /*< private >*/
  AdwNavigationPageClass parent_class;

  const gchar* (*get_help_uri)       (CcPanel *panel);
};

CcShell*      cc_panel_get_shell          (CcPanel     *panel);

GPermission*  cc_panel_get_permission     (CcPanel     *panel);

const gchar*  cc_panel_get_help_uri       (CcPanel     *panel);

GCancellable *cc_panel_get_cancellable    (CcPanel     *panel);

void          cc_panel_deactivate         (CcPanel     *panel);

G_END_DECLS
