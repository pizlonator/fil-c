/*
 * Copyright (C) 2016  Red Hat, Inc.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "cc-display-config-manager.h"

G_DEFINE_TYPE (CcDisplayConfigManager,
               cc_display_config_manager,
               G_TYPE_OBJECT)

enum
{
  CONFIG_MANAGER_CHANGED,
  N_CONFIG_MANAGER_SIGNALS,
};

static guint config_manager_signals[N_CONFIG_MANAGER_SIGNALS] = { 0 };

static void
cc_display_config_manager_init (CcDisplayConfigManager *self)
{
}

static void
cc_display_config_manager_class_init (CcDisplayConfigManagerClass *klass)
{
  config_manager_signals[CONFIG_MANAGER_CHANGED] =
    g_signal_new ("changed",
                  CC_TYPE_DISPLAY_CONFIG_MANAGER,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

void
_cc_display_config_manager_emit_changed (CcDisplayConfigManager *self)
{
  g_signal_emit (self, config_manager_signals[CONFIG_MANAGER_CHANGED], 0);
}

CcDisplayConfig *
cc_display_config_manager_get_current (CcDisplayConfigManager *self)
{
  return CC_DISPLAY_CONFIG_MANAGER_GET_CLASS (self)->get_current (self);
}

gboolean
cc_display_config_manager_get_apply_allowed (CcDisplayConfigManager *self)
{
  return CC_DISPLAY_CONFIG_MANAGER_GET_CLASS (self)->get_apply_allowed (self);
}

gboolean
cc_display_config_manager_get_night_light_supported (CcDisplayConfigManager *self)
{
  return CC_DISPLAY_CONFIG_MANAGER_GET_CLASS (self)->get_night_light_supported (self);
}
