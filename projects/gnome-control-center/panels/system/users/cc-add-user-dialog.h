/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
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
 * Written by: Matthias Clasen <mclasen@redhat.com>
 */

#pragma once

#include <act/act.h>
#include <adwaita.h>

G_BEGIN_DECLS

#define CC_TYPE_ADD_USER_DIALOG (cc_add_user_dialog_get_type ())
G_DECLARE_FINAL_TYPE (CcAddUserDialog, cc_add_user_dialog, CC, ADD_USER_DIALOG, AdwWindow)

CcAddUserDialog *cc_add_user_dialog_new      (GPermission         *permission);
ActUser         *cc_add_user_dialog_get_user (CcAddUserDialog     *dialog);

G_END_DECLS
