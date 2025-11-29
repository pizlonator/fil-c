/* gtkatspiaction.c: ATSPI Action implementation
 *
 * Copyright 2020  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkatspiactionprivate.h"

#include "gtkatspicontextprivate.h"
#include "gtkatcontextprivate.h"

#include "a11y/atspi/atspi-action.h"

#include "gtkactionable.h"
#include "gtkactionmuxerprivate.h"
#include "gtkbutton.h"
#include "gtkcolorswatchprivate.h"
#include "gtkentryprivate.h"
#include "gtkexpander.h"
#include "gtkmodelbuttonprivate.h"
#include "gtkpasswordentryprivate.h"
#include "gtksearchentry.h"
#include "gtkswitch.h"
#include "gtkwidgetprivate.h"

#include <glib/gi18n-lib.h>

typedef struct _Action  Action;

struct _Action
{
  const char *name;
  const char *localized_name;
  const char *description;
  const char *keybinding;

  gboolean (* is_enabled) (GtkAtSpiContext *context);
  gboolean (* activate) (GtkAtSpiContext *context);
};

static void
action_handle_method (GtkAtSpiContext        *self,
                      const char             *method_name,
                      GVariant               *parameters,
                      GDBusMethodInvocation  *invocation,
                      const Action           *actions,
                      int                     n_actions)
{
  if (g_strcmp0 (method_name, "GetName") == 0)
    {
      int idx = -1;

      g_variant_get (parameters, "(i)", &idx);

      if (idx >= 0 && idx < n_actions)
        g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", actions[idx].name));
      else
        g_dbus_method_invocation_return_error (invocation,
                                               G_IO_ERROR,
                                               G_IO_ERROR_INVALID_ARGUMENT,
                                               "Unknown action %d",
                                               idx);
    }
  else if (g_strcmp0 (method_name, "GetLocalizedName") == 0)
    {
      int idx = -1;

      g_variant_get (parameters, "(i)", &idx);

      if (idx >= 0 && idx < n_actions)
        {
          const Action *action = &actions[idx];
          const char *s = g_dpgettext2 (GETTEXT_PACKAGE, "accessibility", action->localized_name);

          g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", s));
        }
      else
        {
          g_dbus_method_invocation_return_error (invocation,
                                                 G_IO_ERROR,
                                                 G_IO_ERROR_INVALID_ARGUMENT,
                                                 "Unknown action %d",
                                                 idx);
        }
    }
  else if (g_strcmp0 (method_name, "GetDescription") == 0)
    {
      int idx = -1;

      g_variant_get (parameters, "(i)", &idx);

      if (idx >= 0 && idx < n_actions)
        {
          const Action *action = &actions[idx];
          const char *s = g_dpgettext2 (GETTEXT_PACKAGE, "accessibility", action->description);

          g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", s));
        }
      else
        {
          g_dbus_method_invocation_return_error (invocation,
                                                 G_IO_ERROR,
                                                 G_IO_ERROR_INVALID_ARGUMENT,
                                                 "Unknown action %d",
                                                 idx);
        }
    }
  else if (g_strcmp0 (method_name, "GetKeyBinding") == 0)
    {
      int idx = -1;

      g_variant_get (parameters, "(i)", &idx);

      if (idx >= 0 && idx < n_actions)
        g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", actions[idx].keybinding));
      else
        g_dbus_method_invocation_return_error (invocation,
                                               G_IO_ERROR,
                                               G_IO_ERROR_INVALID_ARGUMENT,
                                               "Unknown action %d",
                                               idx);
    }
  else if (g_strcmp0 (method_name, "GetActions") == 0)
    {
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a(sss)"));

      for (int i = 0; i < n_actions; i++)
        {
          const Action *action = &actions[i];

          if (action->is_enabled != NULL && !action->is_enabled (self))
            continue;

          g_variant_builder_add (&builder, "(sss)",
                                 g_dpgettext2 (GETTEXT_PACKAGE, "accessibility", action->localized_name),
                                 g_dpgettext2 (GETTEXT_PACKAGE, "accessibility", action->description),
                                 action->keybinding);
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(a(sss))", &builder));
    }
  else if (g_strcmp0 (method_name, "DoAction") == 0)
    {
      GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));
      GtkWidget *widget = GTK_WIDGET (accessible);
      int idx = -1;

      if (!gtk_widget_is_sensitive (widget) || !gtk_widget_get_visible (widget))
        {
          g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", FALSE));
          return;
        }

      g_variant_get (parameters, "(i)", &idx);

      if (idx >= 0 && idx < n_actions)
        {
          const Action *action = &actions[idx];

          if (action->is_enabled == NULL || action->is_enabled (self))
            {
              gboolean res = TRUE;

              if (action->activate == NULL)
                {
                  gtk_widget_activate (widget);
                }
              else
                {
                  res = action->activate (self);
                }

              g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", res));
            }
          else
            {
              g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", FALSE));
            }
        }
      else
        {
          g_dbus_method_invocation_return_error (invocation,
                                                 G_IO_ERROR,
                                                 G_IO_ERROR_INVALID_ARGUMENT,
                                                 "Unknown action %d",
                                                 idx);
        }
    }
}

static GVariant *
action_handle_get_property (GtkAtSpiContext  *self,
                            const char       *property_name,
                            GError          **error,
                            const Action     *actions,
                            int               n_actions)
{
  if (g_strcmp0 (property_name, "NActions") == 0)
    {
      int n_valid_actions = 0;

      for (int i = 0; i < n_actions; i++)
        {
          const Action *action = &actions[i];

          if (action->is_enabled == NULL || action->is_enabled (self))
            n_valid_actions += 1;
        }

      return g_variant_new_int32 (n_valid_actions);
    }

  g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
               "Unknown property '%s'", property_name);

  return NULL;
}

/* {{{ GtkButton */
static Action button_actions[] = {
  {
    .name = "click",
    .localized_name = NC_("accessibility", "Click"),
    .description = NC_("accessibility", "Clicks the button"),
    .keybinding = "<Space>",
  },
};

static void
button_handle_method (GDBusConnection       *connection,
                      const gchar           *sender,
                      const gchar           *object_path,
                      const gchar           *interface_name,
                      const gchar           *method_name,
                      GVariant              *parameters,
                      GDBusMethodInvocation *invocation,
                      gpointer               user_data)
{
  GtkAtSpiContext *self = user_data;

  action_handle_method (self, method_name, parameters, invocation,
                        button_actions,
                        G_N_ELEMENTS (button_actions));
}

static GVariant *
button_handle_get_property (GDBusConnection  *connection,
                            const gchar      *sender,
                            const gchar      *object_path,
                            const gchar      *interface_name,
                            const gchar      *property_name,
                            GError          **error,
                            gpointer          user_data)
{
  GtkAtSpiContext *self = user_data;

  return action_handle_get_property (self, property_name, error,
                                     button_actions,
                                     G_N_ELEMENTS (button_actions));
}

static const GDBusInterfaceVTable button_action_vtable = {
  button_handle_method,
  button_handle_get_property,
  NULL,
};

/* }}} */
/* {{{ GtkSwitch */

static const Action switch_actions[] = {
  {
    .name = "toggle",
    .localized_name = NC_("accessibility", "Toggle"),
    .description = NC_("accessibility", "Toggles the switch"),
    .keybinding = "<Space>",
  },
};

static void
switch_handle_method (GDBusConnection       *connection,
                      const gchar           *sender,
                      const gchar           *object_path,
                      const gchar           *interface_name,
                      const gchar           *method_name,
                      GVariant              *parameters,
                      GDBusMethodInvocation *invocation,
                      gpointer               user_data)
{
  GtkAtSpiContext *self = user_data;

  action_handle_method (self, method_name, parameters, invocation,
                        switch_actions,
                        G_N_ELEMENTS (switch_actions));
}

static GVariant *
switch_handle_get_property (GDBusConnection  *connection,
                            const gchar      *sender,
                            const gchar      *object_path,
                            const gchar      *interface_name,
                            const gchar      *property_name,
                            GError          **error,
                            gpointer          user_data)
{
  GtkAtSpiContext *self = user_data;

  return action_handle_get_property (self, property_name, error,
                                     switch_actions,
                                     G_N_ELEMENTS (switch_actions));
}

static const GDBusInterfaceVTable switch_action_vtable = {
  switch_handle_method,
  switch_handle_get_property,
  NULL,
};

/* }}} */
/* {{{ GtkColorSwatch */
static gboolean
color_swatch_select (GtkAtSpiContext *self)
{
  GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));
  gtk_color_swatch_select (GTK_COLOR_SWATCH (accessible));
  return TRUE;
}

static gboolean
color_swatch_activate (GtkAtSpiContext *self)
{
  GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));
  gtk_color_swatch_activate (GTK_COLOR_SWATCH (accessible));
  return TRUE;
}

static gboolean
color_swatch_customize (GtkAtSpiContext *self)
{
  GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));
  gtk_color_swatch_customize (GTK_COLOR_SWATCH (accessible));
  return TRUE;
}

static gboolean
color_swatch_is_enabled (GtkAtSpiContext *self)
{
  GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));
  return gtk_color_swatch_get_selectable (GTK_COLOR_SWATCH (accessible));
}

static const Action color_swatch_actions[] = {
  {
    .name = "select",
    .localized_name = NC_("accessibility", "Select"),
    .description = NC_("accessibility", "Selects the color"),
    .keybinding = "<Return>",
    .activate = color_swatch_select,
    .is_enabled = color_swatch_is_enabled,
  },
  {
    .name = "activate",
    .localized_name = NC_("accessibility", "Activate"),
    .description = NC_("accessibility", "Activates the color"),
    .keybinding = "<VoidSymbol>",
    .activate = color_swatch_activate,
    .is_enabled = color_swatch_is_enabled,
  },
  {
    .name = "customize",
    .localized_name = NC_("accessibility", "Customize"),
    .description = NC_("accessibility", "Customizes the color"),
    .keybinding = "<VoidSymbol>",
    .activate = color_swatch_customize,
    .is_enabled = color_swatch_is_enabled,
  },
};

static void
color_swatch_handle_method (GDBusConnection       *connection,
                            const gchar           *sender,
                            const gchar           *object_path,
                            const gchar           *interface_name,
                            const gchar           *method_name,
                            GVariant              *parameters,
                            GDBusMethodInvocation *invocation,
                            gpointer               user_data)
{
  GtkAtSpiContext *self = user_data;

  action_handle_method (self, method_name, parameters, invocation,
                        color_swatch_actions,
                        G_N_ELEMENTS (color_swatch_actions));
}

static GVariant *
color_swatch_handle_get_property (GDBusConnection  *connection,
                                  const gchar      *sender,
                                  const gchar      *object_path,
                                  const gchar      *interface_name,
                                  const gchar      *property_name,
                                  GError          **error,
                                  gpointer          user_data)
{
  GtkAtSpiContext *self = user_data;

  return action_handle_get_property (self, property_name, error,
                                     color_swatch_actions,
                                     G_N_ELEMENTS (color_swatch_actions));
}

static const GDBusInterfaceVTable color_swatch_action_vtable = {
  color_swatch_handle_method,
  color_swatch_handle_get_property,
  NULL,
};
/* }}} */
/* {{{ GtkExpander */

static const Action expander_actions[] = {
  {
    .name = "activate",
    .localized_name = NC_("accessibility", "Activate"),
    .description = NC_("accessibility", "Activates the expander"),
    .keybinding = "<Space>",
  },
};

static void
expander_handle_method (GDBusConnection       *connection,
                        const gchar           *sender,
                        const gchar           *object_path,
                        const gchar           *interface_name,
                        const gchar           *method_name,
                        GVariant              *parameters,
                        GDBusMethodInvocation *invocation,
                        gpointer               user_data)
{
  GtkAtSpiContext *self = user_data;

  action_handle_method (self, method_name, parameters, invocation,
                        expander_actions,
                        G_N_ELEMENTS (expander_actions));
}

static GVariant *
expander_handle_get_property (GDBusConnection  *connection,
                              const gchar      *sender,
                              const gchar      *object_path,
                              const gchar      *interface_name,
                              const gchar      *property_name,
                              GError          **error,
                              gpointer          user_data)
{
  GtkAtSpiContext *self = user_data;

  return action_handle_get_property (self, property_name, error,
                                     expander_actions,
                                     G_N_ELEMENTS (expander_actions));
}

static const GDBusInterfaceVTable expander_action_vtable = {
  expander_handle_method,
  expander_handle_get_property,
  NULL,
};

/* }}} */
/* {{{ GtkEntry */

static gboolean is_primary_icon_enabled (GtkAtSpiContext *self);
static gboolean is_secondary_icon_enabled (GtkAtSpiContext *self);
static gboolean activate_primary_icon (GtkAtSpiContext *self);
static gboolean activate_secondary_icon (GtkAtSpiContext *self);

static const Action entry_actions[] = {
  {
    .name = "activate",
    .localized_name = NC_("accessibility", "Activate"),
    .description = NC_("accessibility", "Activates the entry"),
    .keybinding = "<Return>",
    .is_enabled = NULL,
    .activate = NULL,
  },
  {
    .name = "activate-primary-icon",
    .localized_name = NC_("accessibility", "Activate primary icon"),
    .description = NC_("accessibility", "Activates the primary icon of the entry"),
    .keybinding = "<VoidSymbol>",
    .is_enabled = is_primary_icon_enabled,
    .activate = activate_primary_icon,
  },
  {
    .name = "activate-secondary-icon",
    .localized_name = NC_("accessibility", "Activate secondary icon"),
    .description = NC_("accessibility", "Activates the secondary icon of the entry"),
    .keybinding = "<VoidSymbol>",
    .is_enabled = is_secondary_icon_enabled,
    .activate = activate_secondary_icon,
  },
};

static gboolean
is_primary_icon_enabled (GtkAtSpiContext *self)
{
  GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));
  GtkEntry *entry = GTK_ENTRY (accessible);

  return gtk_entry_get_icon_storage_type (entry, GTK_ENTRY_ICON_PRIMARY) != GTK_IMAGE_EMPTY;
}

static gboolean
activate_primary_icon (GtkAtSpiContext *self)
{
  GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));
  GtkEntry *entry = GTK_ENTRY (accessible);

  return gtk_entry_activate_icon (entry, GTK_ENTRY_ICON_PRIMARY);
}

static gboolean
is_secondary_icon_enabled (GtkAtSpiContext *self)
{
  GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));
  GtkEntry *entry = GTK_ENTRY (accessible);

  return gtk_entry_get_icon_storage_type (entry, GTK_ENTRY_ICON_SECONDARY) != GTK_IMAGE_EMPTY;
}

static gboolean
activate_secondary_icon (GtkAtSpiContext *self)
{
  GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));
  GtkEntry *entry = GTK_ENTRY (accessible);

  return gtk_entry_activate_icon (entry, GTK_ENTRY_ICON_SECONDARY);
}

static void
entry_handle_method (GDBusConnection       *connection,
                     const gchar           *sender,
                     const gchar           *object_path,
                     const gchar           *interface_name,
                     const gchar           *method_name,
                     GVariant              *parameters,
                     GDBusMethodInvocation *invocation,
                     gpointer               user_data)
{
  GtkAtSpiContext *self = user_data;

  action_handle_method (self, method_name, parameters, invocation,
                        entry_actions,
                        G_N_ELEMENTS (entry_actions));
}

static GVariant *
entry_handle_get_property (GDBusConnection  *connection,
                           const gchar      *sender,
                           const gchar      *object_path,
                           const gchar      *interface_name,
                           const gchar      *property_name,
                           GError          **error,
                           gpointer          user_data)
{
  GtkAtSpiContext *self = user_data;

  return action_handle_get_property (self, property_name, error,
                                     entry_actions,
                                     G_N_ELEMENTS (entry_actions));
}

static const GDBusInterfaceVTable entry_action_vtable = {
  entry_handle_method,
  entry_handle_get_property,
  NULL,
};

/* }}} */
/* {{{ GtkPasswordEntry */

static gboolean is_peek_enabled (GtkAtSpiContext *self);
static gboolean activate_peek (GtkAtSpiContext *self);

static const Action password_entry_actions[] = {
  {
    .name = "activate",
    .localized_name = NC_("accessibility", "Activate"),
    .description = NC_("accessibility", "Activates the entry"),
    .keybinding = "<Return>",
    .is_enabled = NULL,
    .activate = NULL,
  },
  {
    .name = "peek",
    .localized_name = NC_("accessibility", "Peek"),
    .description = NC_("accessibility", "Shows the contents of the password entry"),
    .keybinding = "<VoidSymbol>",
    .is_enabled = is_peek_enabled,
    .activate = activate_peek,
  },
};

static gboolean
is_peek_enabled (GtkAtSpiContext *self)
{
  GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));
  GtkPasswordEntry *entry = GTK_PASSWORD_ENTRY (accessible);

  if (!gtk_password_entry_get_show_peek_icon (entry))
    return FALSE;

  return TRUE;
}

static gboolean
activate_peek (GtkAtSpiContext *self)
{
  GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));
  GtkPasswordEntry *entry = GTK_PASSWORD_ENTRY (accessible);

  gtk_password_entry_toggle_peek (entry);

  return TRUE;
}

static void
password_entry_handle_method (GDBusConnection       *connection,
                              const gchar           *sender,
                              const gchar           *object_path,
                              const gchar           *interface_name,
                              const gchar           *method_name,
                              GVariant              *parameters,
                              GDBusMethodInvocation *invocation,
                              gpointer               user_data)
{
  GtkAtSpiContext *self = user_data;

  action_handle_method (self, method_name, parameters, invocation,
                        password_entry_actions,
                        G_N_ELEMENTS (password_entry_actions));
}

static GVariant *
password_entry_handle_get_property (GDBusConnection  *connection,
                                    const gchar      *sender,
                                    const gchar      *object_path,
                                    const gchar      *interface_name,
                                    const gchar      *property_name,
                                    GError          **error,
                                    gpointer          user_data)
{
  GtkAtSpiContext *self = user_data;

  return action_handle_get_property (self, property_name, error,
                                     password_entry_actions,
                                     G_N_ELEMENTS (password_entry_actions));
}

static const GDBusInterfaceVTable password_entry_action_vtable = {
  password_entry_handle_method,
  password_entry_handle_get_property,
  NULL,
};

/* }}} */
/* {{{ GtkSearchEntry */

static gboolean is_clear_enabled (GtkAtSpiContext *self);
static gboolean activate_clear (GtkAtSpiContext *self);

static const Action search_entry_actions[] = {
  {
    .name = "activate",
    .localized_name = NC_("accessibility", "Activate"),
    .description = NC_("accessibility", "Activates the entry"),
    .keybinding = "<Return>",
    .is_enabled = NULL,
    .activate = NULL,
  },
  {
    .name = "clear",
    .localized_name = NC_("accessibility", "Clear"),
    .description = NC_("accessibility", "Clears the contents of the entry"),
    .keybinding = "<VoidSymbol>",
    .is_enabled = is_clear_enabled,
    .activate = activate_clear,
  },
};

static gboolean
is_clear_enabled (GtkAtSpiContext *self)
{
  GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));
  GtkSearchEntry *entry = GTK_SEARCH_ENTRY (accessible);

  const char *str = gtk_editable_get_text (GTK_EDITABLE (entry));

  if (str == NULL || *str == '\0')
    return FALSE;

  return TRUE;
}

static gboolean
activate_clear (GtkAtSpiContext *self)
{
  GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));

  gtk_editable_set_text (GTK_EDITABLE (accessible), "");

  return TRUE;
}

static void
search_entry_handle_method (GDBusConnection       *connection,
                            const gchar           *sender,
                            const gchar           *object_path,
                            const gchar           *interface_name,
                            const gchar           *method_name,
                            GVariant              *parameters,
                            GDBusMethodInvocation *invocation,
                            gpointer               user_data)
{
  GtkAtSpiContext *self = user_data;

  action_handle_method (self, method_name, parameters, invocation,
                        search_entry_actions,
                        G_N_ELEMENTS (search_entry_actions));
}

static GVariant *
search_entry_handle_get_property (GDBusConnection  *connection,
                                  const gchar      *sender,
                                  const gchar      *object_path,
                                  const gchar      *interface_name,
                                  const gchar      *property_name,
                                  GError          **error,
                                  gpointer          user_data)
{
  GtkAtSpiContext *self = user_data;

  return action_handle_get_property (self, property_name, error,
                                     search_entry_actions,
                                     G_N_ELEMENTS (search_entry_actions));
}

static const GDBusInterfaceVTable search_entry_action_vtable = {
  search_entry_handle_method,
  search_entry_handle_get_property,
  NULL,
};
/* }}} */

static gboolean
is_valid_action (GtkActionMuxer *muxer,
                 const char     *action_name)
{
  const GVariantType *param_type = NULL;
  gboolean enabled = FALSE;

  /* Skip disabled or parametrized actions */
  if (!gtk_action_muxer_query_action (muxer, action_name,
                                      &enabled,
                                      &param_type, NULL,
                                      NULL, NULL))
    return FALSE;

  if (!enabled || param_type != NULL)
    return FALSE;

  return TRUE;
}

static void
add_muxer_actions (GtkActionMuxer   *muxer,
                   char            **actions,
                   int               n_actions,
                   GVariantBuilder  *builder)
{
  for (int i = 0; i < n_actions; i++)
    {
      if (!is_valid_action (muxer, actions[i]))
        continue;

      g_variant_builder_add (builder, "(sss)",
                             actions[i],
                             actions[i],
                             "<VoidSymbol>");
    }
}

static const char *
get_action_at_index (GtkActionMuxer  *muxer,
                     char           **actions,
                     int              n_actions,
                     int              pos)
{
  int real_pos = 0;

  for (int i = 0; i < n_actions; i++)
    {
      if (!is_valid_action (muxer, actions[i]))
        continue;

      if (real_pos == pos)
        return actions[i];

      real_pos += 1;
    }

  return NULL;
}

static int
get_valid_actions (GtkActionMuxer  *muxer,
                   char           **actions,
                   int              n_actions)
{
  int n_enabled_actions = 0;

  for (int i = 0; i < n_actions; i++)
    {
      if (!is_valid_action (muxer, actions[i]))
        continue;

      n_enabled_actions += 1;
    }

  return n_enabled_actions;
}

static void
widget_handle_method (GDBusConnection       *connection,
                      const gchar           *sender,
                      const gchar           *object_path,
                      const gchar           *interface_name,
                      const gchar           *method_name,
                      GVariant              *parameters,
                      GDBusMethodInvocation *invocation,
                      gpointer               user_data)
{
  GtkAtSpiContext *self = user_data;
  GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));
  GtkWidget *widget = GTK_WIDGET (accessible);
  GtkWidget *parent = gtk_widget_get_parent (widget);
  GtkActionMuxer *muxer = _gtk_widget_get_action_muxer (widget, FALSE);
  GtkActionMuxer *parent_muxer = parent ? _gtk_widget_get_action_muxer (parent, FALSE) : NULL;

  if (muxer == NULL)
    return;

  char **actions = NULL;

  if (muxer != parent_muxer)
    actions = gtk_action_muxer_list_actions (muxer, TRUE);

  int n_actions = actions != NULL ? g_strv_length (actions) : 0;

  /* XXX: We need more fields in the action API */
  if (g_strcmp0 (method_name, "GetName") == 0 ||
      g_strcmp0 (method_name, "GetLocalizedName") == 0 ||
      g_strcmp0 (method_name, "GetDescription") == 0)
    {
      int action_idx;

      g_variant_get (parameters, "(i)", &action_idx);

      const char *action = get_action_at_index (muxer, actions, n_actions, action_idx);

      if (action != NULL && gtk_widget_is_sensitive (widget))
        g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", action));
      else
        g_dbus_method_invocation_return_error (invocation,
                                               G_IO_ERROR,
                                               G_IO_ERROR_INVALID_ARGUMENT,
                                               "No action with index %d",
                                               action_idx);
    }
  else if (g_strcmp0 (method_name, "DoAction") == 0)
    {
      int action_idx;

      g_variant_get (parameters, "(i)", &action_idx);

      const char *action = get_action_at_index (muxer, actions, n_actions, action_idx);

      if (action != NULL && gtk_widget_is_sensitive (widget))
        {
          gboolean res = gtk_widget_activate_action_variant (widget, action, NULL);

          g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", res));
        }
      else
        {
          g_dbus_method_invocation_return_error (invocation,
                                                 G_IO_ERROR,
                                                 G_IO_ERROR_INVALID_ARGUMENT,
                                                 "No action with index %d",
                                                 action_idx);
        }
    }
  else if (g_strcmp0 (method_name, "GetKeyBinding") == 0)
    {
      int action_idx;

      g_variant_get (parameters, "(i)", &action_idx);

      const char *action = get_action_at_index (muxer, actions, n_actions, action_idx);

      if (action != NULL && gtk_widget_is_sensitive (widget))
        g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", "<VoidSymbol>"));
      else
        g_dbus_method_invocation_return_error (invocation,
                                               G_IO_ERROR,
                                               G_IO_ERROR_INVALID_ARGUMENT,
                                               "No action with index %d",
                                               action_idx);
    }
  else if (g_strcmp0 (method_name, "GetActions") == 0)
    {
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a(sss)"));

      if (n_actions >= 0 && gtk_widget_is_sensitive (widget))
        add_muxer_actions (muxer, actions, n_actions, &builder);

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(a(sss))", &builder));
    }

  g_strfreev (actions);
}

static GVariant *
widget_handle_get_property (GDBusConnection  *connection,
                            const gchar      *sender,
                            const gchar      *object_path,
                            const gchar      *interface_name,
                            const gchar      *property_name,
                            GError          **error,
                            gpointer          user_data)
{
  GtkAtSpiContext *self = user_data;
  GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));
  GtkWidget *widget = GTK_WIDGET (accessible);
  GtkWidget *parent = gtk_widget_get_parent (widget);
  GtkActionMuxer *muxer = _gtk_widget_get_action_muxer (widget, FALSE);
  GtkActionMuxer *parent_muxer = parent ? _gtk_widget_get_action_muxer (parent, FALSE) : NULL;
  GVariant *res = NULL;

  if (muxer == NULL)
    return res;

  char **actions = NULL;

  if (muxer != parent_muxer)
    actions = gtk_action_muxer_list_actions (muxer, TRUE);

  int n_actions = actions != NULL ? g_strv_length (actions) : 0;

  if (g_strcmp0 (property_name, "NActions") == 0)
    res = g_variant_new ("i", get_valid_actions (muxer, actions, n_actions));
  else
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                 "Unknown property '%s'", property_name);

  return res;
}

static const GDBusInterfaceVTable widget_action_vtable = {
  widget_handle_method,
  widget_handle_get_property,
  NULL,
};

const GDBusInterfaceVTable *
gtk_atspi_get_action_vtable (GtkAccessible *accessible)
{
  if (GTK_IS_BUTTON (accessible) ||
      GTK_IS_MODEL_BUTTON (accessible))
    return &button_action_vtable;
  else if (GTK_IS_ENTRY (accessible))
    return &entry_action_vtable;
  else if (GTK_IS_EXPANDER (accessible))
    return &expander_action_vtable;
  else if (GTK_IS_PASSWORD_ENTRY (accessible))
    return &password_entry_action_vtable;
  else if (GTK_IS_SEARCH_ENTRY (accessible))
    return &search_entry_action_vtable;
  else if (GTK_IS_SWITCH (accessible))
    return &switch_action_vtable;
  else if (GTK_IS_COLOR_SWATCH (accessible))
    return &color_swatch_action_vtable;
  else if (GTK_IS_WIDGET (accessible))
    return &widget_action_vtable;

  return NULL;
}

/* vim:set foldmethod=marker expandtab: */
