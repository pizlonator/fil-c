/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (c) 2012 Giovanni Campagna <scampa.giovanni@gmail.com>
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
 */

#pragma once

#include "config.h"

#include <glib.h>
#include <glib-object.h>
#include <shell/cc-panel.h>
#include <shell/cc-shell-model.h>

G_BEGIN_DECLS

typedef struct
{
  const gchar           *name;

#ifndef CC_PANEL_LOADER_NO_GTYPES
  GType                (*get_type)(void);
  CcPanelStaticInitFunc static_init_func;
#endif
} CcPanelLoaderVtable;

void     cc_panel_loader_fill_model     (CcShellModel  *model);
void     cc_panel_loader_list_panels    (void);
CcPanel *cc_panel_loader_load_by_name   (CcShell       *shell,
                                         const char    *name,
                                         const gchar   *title,
                                         GVariant      *parameters);

void    cc_panel_loader_override_vtable (CcPanelLoaderVtable *override_vtable,
                                         gsize                n_elements);

G_END_DECLS

