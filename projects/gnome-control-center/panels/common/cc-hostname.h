/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* cc-hostname.h
 *
 * Copyright 2023 Red Hat Inc
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define CC_TYPE_HOSTNAME (cc_hostname_get_type())
G_DECLARE_FINAL_TYPE (CcHostname, cc_hostname, CC, HOSTNAME, GObject)

CcHostname  *cc_hostname_get_default          (void);

gchar       *cc_hostname_get_display_hostname (CcHostname *self);

gchar       *cc_hostname_get_static_hostname  (CcHostname *self);

void         cc_hostname_set_hostname         (CcHostname *self, const gchar *hostname);

gchar       *cc_hostname_get_property         (CcHostname *self, const gchar *property);

gchar       *cc_hostname_get_chassis_type     (CcHostname *self);

gboolean     cc_hostname_is_vm_chassis        (CcHostname *self);

G_END_DECLS
