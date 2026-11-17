/*
 * Copyright © 2008, 2010, 2022 Christian Persch
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define TERMINAL_TYPE_SETTINGS_BRIDGE_IMPL         (terminal_settings_bridge_impl_get_type ())
#define TERMINAL_SETTINGS_BRIDGE_IMPL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TERMINAL_TYPE_SETTINGS_BRIDGE_IMPL, TerminalSettingsBridgeImpl))
#define TERMINAL_SETTINGS_BRIDGE_IMPL_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), TERMINAL_TYPE_SETTINGS_BRIDGE_IMPL, TerminalSettingsBridgeImplClass))
#define TERMINAL_IS_SETTINGS_BRIDGE_IMPL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TERMINAL_TYPE_SETTINGS_BRIDGE_IMPL))
#define TERMINAL_IS_SETTINGS_BRIDGE_IMPL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TERMINAL_TYPE_SETTINGS_BRIDGE_IMPL))
#define TERMINAL_SETTINGS_BRIDGE_IMPL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TERMINAL_TYPE_SETTINGS_BRIDGE_IMPL, TerminalSettingsBridgeImplClass))

typedef struct _TerminalSettingsBridgeImpl        TerminalSettingsBridgeImpl;
typedef struct _TerminalSettingsBridgeImplClass   TerminalSettingsBridgeImplClass;

GType terminal_settings_bridge_impl_get_type(void);

TerminalSettingsBridgeImpl* terminal_settings_bridge_impl_new(GSettingsBackend* backend);

G_END_DECLS
