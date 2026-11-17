/* cc-permission-infobar.h
 *
 * Copyright (C) 2020 Red Hat, Inc
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
 *
 * Author(s):
 *   Felipe Borges <felipeborges@gnome.org>
 *
 */
#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define CC_TYPE_PERMISSION_INFOBAR (cc_permission_infobar_get_type())
G_DECLARE_FINAL_TYPE (CcPermissionInfobar, cc_permission_infobar, CC, PERMISSION_INFOBAR, AdwBin)

void            cc_permission_infobar_set_permission (CcPermissionInfobar *self,
                                                      GPermission         *permission);

void            cc_permission_infobar_set_title      (CcPermissionInfobar *self,
                                                      const gchar         *title);

G_END_DECLS
