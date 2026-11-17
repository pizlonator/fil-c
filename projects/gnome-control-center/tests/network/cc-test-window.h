/*
 * Copyright (c) 2010 Intel, Inc.
 * Copyright (c) 2018 Red Hat, Inc.
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
 * Author: Benjamin Berg <bberg@redhat.com>
 */

#pragma once

#include <glib-object.h>
#include "shell/cc-shell.h"

G_BEGIN_DECLS

#define CC_TYPE_TEST_WINDOW (cc_test_window_get_type ())

G_DECLARE_FINAL_TYPE (CcTestWindow, cc_test_window, CC, TEST_WINDOW, GtkWindow)

CcTestWindow *cc_test_window_new (void);

G_END_DECLS
