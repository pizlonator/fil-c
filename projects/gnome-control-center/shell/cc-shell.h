/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
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

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_SHELL (cc_shell_get_type())
#define CC_SHELL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), CC_TYPE_SHELL, CcShell))
#define CC_IS_SHELL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CC_TYPE_SHELL))
#define CC_SHELL_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), CC_TYPE_SHELL, CcShellInterface))

#define CC_SHELL_PANEL_EXTENSION_POINT "control-center-1"

typedef struct _CcShell CcShell;
typedef struct _CcShellInterface CcShellInterface;

/* cc-panel.h requires CcShell, so make sure they are defined first */
#include "cc-panel.h"

/**
 * CcShellInterface:
 * @set_active_panel_from_id: virtual function to set the active panel from an
 *                            id string
 *
 */
struct _CcShellInterface
{
  GTypeInterface g_iface;

  /* methods */
  gboolean    (*set_active_panel_from_id) (CcShell      *shell,
                                           const gchar  *id,
                                           GVariant     *parameters,
                                           GError      **error);
  GtkWidget * (*get_toplevel)             (CcShell      *shell);
};

GType           cc_shell_get_type                 (void) G_GNUC_CONST;

CcPanel *       cc_shell_get_active_panel         (CcShell      *shell);
void            cc_shell_set_active_panel         (CcShell      *shell,
                                                   CcPanel      *panel);
gboolean        cc_shell_set_active_panel_from_id (CcShell      *shell,
                                                   const gchar  *id,
                                                   GVariant     *parameters,
                                                   GError      **error);
GtkWidget *     cc_shell_get_toplevel             (CcShell      *shell);

G_END_DECLS
