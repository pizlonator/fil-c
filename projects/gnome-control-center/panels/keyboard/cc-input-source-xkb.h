/*
 * Copyright © 2018 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-xkb-info.h>

#include "cc-input-source.h"

G_BEGIN_DECLS

#define CC_TYPE_INPUT_SOURCE_XKB (cc_input_source_xkb_get_type ())
G_DECLARE_FINAL_TYPE (CcInputSourceXkb, cc_input_source_xkb, CC, INPUT_SOURCE_XKB, CcInputSource)

CcInputSourceXkb *cc_input_source_xkb_new         (GnomeXkbInfo     *xkb_info,
                                                   const gchar      *layout,
                                                   const gchar      *variant);

CcInputSourceXkb *cc_input_source_xkb_new_from_id (GnomeXkbInfo     *xkb_info,
                                                   const gchar      *id);

gchar            *cc_input_source_xkb_get_id      (CcInputSourceXkb *source);

G_END_DECLS
