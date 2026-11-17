/* -*- mode: c; style: linux -*-
 * 
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * Written by: Ondrej Holy <oholy@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define CC_TYPE_MOUSE_TEST (cc_mouse_test_get_type ())
G_DECLARE_FINAL_TYPE (CcMouseTest, cc_mouse_test, CC, MOUSE_TEST, AdwWindow)

GtkWidget *cc_mouse_test_new (void);

G_END_DECLS
