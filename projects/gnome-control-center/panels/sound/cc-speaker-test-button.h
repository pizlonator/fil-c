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

#include <gsound.h>
#include <gtk/gtk.h>
#include <pulse/pulseaudio.h>

G_BEGIN_DECLS

#define CC_TYPE_SPEAKER_TEST_BUTTON (cc_speaker_test_button_get_type ())
G_DECLARE_FINAL_TYPE (CcSpeakerTestButton, cc_speaker_test_button, CC, SPEAKER_TEST_BUTTON, GtkButton)

GtkWidget *cc_speaker_test_button_new (GSoundContext         *context,
                                       pa_channel_position_t  position);

void       cc_speaker_test_button_set_channel_position (CcSpeakerTestButton   *button,
                                                        pa_channel_position_t  position);

G_END_DECLS
