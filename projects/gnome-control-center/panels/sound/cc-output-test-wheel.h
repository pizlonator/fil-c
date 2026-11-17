/*
 * Copyright (C) 2022 Marco Melorio
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
#include <gvc-mixer-stream.h>

G_BEGIN_DECLS

#define CC_TYPE_OUTPUT_TEST_WHEEL (cc_output_test_wheel_get_type ())
G_DECLARE_FINAL_TYPE (CcOutputTestWheel, cc_output_test_wheel, CC, OUTPUT_TEST_WHEEL, GtkWidget)

void cc_output_test_wheel_set_stream (CcOutputTestWheel *self,
                                      GvcMixerStream    *stream);

G_END_DECLS
