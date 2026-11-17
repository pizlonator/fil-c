/*
 * Copyright (C) 2013 Intel, Inc
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
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */

#pragma once

#include <shell/cc-shell.h>

G_BEGIN_DECLS

#define CC_TYPE_SHARING_PANEL (cc_sharing_panel_get_type ())
G_DECLARE_FINAL_TYPE (CcSharingPanel, cc_sharing_panel, CC, SHARING_PANEL, CcPanel)

CcSharingPanel *cc_sharing_panel_new (void);
void            cc_sharing_panel_static_init_func (void);

G_END_DECLS
