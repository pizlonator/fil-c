/*
 * Copyright (C) 2018 Canonical Ltd.
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

#include <gtk/gtk.h>
#include <pulse/pulseaudio.h>
#include <gvc-mixer-control.h>
#include <gvc-mixer-ui-device.h>

G_BEGIN_DECLS

#define CC_TYPE_PROFILE_COMBO_BOX (cc_profile_combo_box_get_type ())
G_DECLARE_FINAL_TYPE (CcProfileComboBox, cc_profile_combo_box, CC, PROFILE_COMBO_BOX, GtkComboBox)

void                cc_profile_combo_box_set_device        (CcProfileComboBox *combo_box,
                                                            GvcMixerControl   *mixer_control,
                                                            GvcMixerUIDevice  *device);

gint                cc_profile_combo_box_get_profile_count (CcProfileComboBox *combo_box);

G_END_DECLS
