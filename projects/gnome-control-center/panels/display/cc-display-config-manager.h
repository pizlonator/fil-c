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

#pragma once

#include <glib-object.h>

#include "cc-display-config.h"

G_BEGIN_DECLS

#define CC_TYPE_DISPLAY_CONFIG_MANAGER (cc_display_config_manager_get_type ())
G_DECLARE_DERIVABLE_TYPE (CcDisplayConfigManager, cc_display_config_manager,
                          CC, DISPLAY_CONFIG_MANAGER, GObject)

struct _CcDisplayConfigManagerClass
{
  GObjectClass parent_class;

  CcDisplayConfig * (*get_current) (CcDisplayConfigManager *self);
  gboolean (* get_apply_allowed) (CcDisplayConfigManager *self);
  gboolean (* get_night_light_supported) (CcDisplayConfigManager *self);
};

CcDisplayConfig * cc_display_config_manager_get_current (CcDisplayConfigManager *self);

gboolean cc_display_config_manager_get_apply_allowed (CcDisplayConfigManager *self);

gboolean cc_display_config_manager_get_night_light_supported (CcDisplayConfigManager *self);

void _cc_display_config_manager_emit_changed (CcDisplayConfigManager *self);

G_END_DECLS
