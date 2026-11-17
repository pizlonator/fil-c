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

/**
 * SECTION:cc-shell
 * @short_description: Interface representing the Control Center shell
 *
 * CcShell is an interface that represents an instance of a control
 * center shell. It provides access to some of the properties of the shell
 * that panels will need to read or change. When a panel is created it has an
 * instance of CcShell available that represents the current shell.
 */


#include "cc-shell.h"
#include "cc-panel.h"

G_DEFINE_INTERFACE (CcShell, cc_shell, GTK_TYPE_WIDGET)

static void
cc_shell_default_init (CcShellInterface *iface)
{
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("active-panel",
                                                            "active panel",
                                                            "The currently active Panel",
                                                            CC_TYPE_PANEL,
                                                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

/**
 * cc_shell_get_active_panel:
 * @shell: A #CcShell
 *
 * Get the current active panel
 *
 * Returns: a #CcPanel or NULL if no panel is active
 */
CcPanel *
cc_shell_get_active_panel (CcShell *shell)
{
  CcPanel *panel = NULL;

  g_return_val_if_fail (CC_IS_SHELL (shell), NULL);

  g_object_get (shell, "active-panel", &panel, NULL);

  return panel;
}

/**
 * cc_shell_set_active_panel:
 * @shell: A #CcShell
 * @panel: A #CcPanel
 *
 * Set the current active panel. If @panel is NULL, then the shell is returned
 * to a state where no panel is being displayed (for example, the list of panels
 * may be shown instead).
 *
 */
void
cc_shell_set_active_panel (CcShell *shell,
                           CcPanel *panel)
{
  g_return_if_fail (CC_IS_SHELL (shell));
  g_return_if_fail (panel == NULL || CC_IS_PANEL (panel));

  g_object_set (shell, "active-panel", panel, NULL);
}

/**
 * cc_shell_set_active_panel_from_id:
 * @shell: A #CcShell
 * @id: The ID of the panel to set as active
 * @parameters: A #GVariant with additional parameters
 * @error: A #GError
 *
 * Find a panel corresponding to the specified id and set it as active.
 *
 * Returns: #TRUE if the panel was found and set as the active panel
 */
gboolean
cc_shell_set_active_panel_from_id (CcShell      *shell,
                                   const gchar  *id,
                                   GVariant     *parameters,
                                   GError      **error)
{
  CcShellInterface *iface;

  g_return_val_if_fail (CC_IS_SHELL (shell), FALSE);

  iface = CC_SHELL_GET_IFACE (shell);

  if (!iface->set_active_panel_from_id)
    {
      g_warning ("Object of type \"%s\" does not implement required interface"
                 " method \"set_active_panel_from_id\",",
                 G_OBJECT_TYPE_NAME (shell));
      return FALSE;
    }
  else
    {
      return iface->set_active_panel_from_id (shell, id, parameters, error);
    }
}

/**
 * cc_shell_get_toplevel:
 * @shell: A #CcShell
 *
 * Gets the toplevel window of the shell.
 *
 * Returns: The #GtkWidget of the shell window, or #NULL on error.
 */
GtkWidget *
cc_shell_get_toplevel (CcShell *shell)
{
  CcShellInterface *iface;

  g_return_val_if_fail (CC_IS_SHELL (shell), NULL);

  iface = CC_SHELL_GET_IFACE (shell);

  if (iface->get_toplevel)
    {
        return iface->get_toplevel (shell);
    }

  g_warning ("Object of type \"%s\" does not implement required interface"
             " method \"get_toplevel\",",
             G_OBJECT_TYPE_NAME (shell));

  return NULL;
}
