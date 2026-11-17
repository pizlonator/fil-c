/*
 * Copyright (C) 2023 Marco Melorio
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <adwaita.h>
#include <gvc-mixer-control.h>

G_BEGIN_DECLS

#define CC_TYPE_VOLUME_LEVELS_WINDOW (cc_volume_levels_window_get_type ())
G_DECLARE_FINAL_TYPE (CcVolumeLevelsWindow, cc_volume_levels_window, CC, VOLUME_LEVELS_WINDOW, AdwWindow)

CcVolumeLevelsWindow *cc_volume_levels_window_new (GvcMixerControl *mixer_control);

G_END_DECLS
