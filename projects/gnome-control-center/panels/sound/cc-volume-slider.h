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
#include <gvc-mixer-stream.h>

#include "cc-sound-enums.h"

G_BEGIN_DECLS

#define CC_TYPE_VOLUME_SLIDER (cc_volume_slider_get_type ())
G_DECLARE_FINAL_TYPE (CcVolumeSlider, cc_volume_slider, CC, VOLUME_SLIDER, GtkWidget)

void cc_volume_slider_set_mixer_control (CcVolumeSlider  *slider,
                                         GvcMixerControl *mixer_control);

void cc_volume_slider_set_stream        (CcVolumeSlider  *slider,
                                         GvcMixerStream  *stream,
                                         CcStreamType     type);

void cc_volume_slider_set_is_amplified  (CcVolumeSlider  *slider,
                                         gboolean         is_amplified);

GvcMixerStream *cc_volume_slider_get_stream (CcVolumeSlider *slider);

G_END_DECLS
