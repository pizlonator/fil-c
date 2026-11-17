/*
 * Copyright © 2018 Red Hat, Inc.
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
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#pragma once

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_CLOCK (cc_clock_get_type ())

G_DECLARE_FINAL_TYPE (CcClock, cc_clock, CC, CLOCK, GtkWidget)

GtkWidget * cc_clock_new          (guint duration);

void        cc_clock_reset        (CcClock *clock);

void        cc_clock_set_duration (CcClock *clock,
                                   guint    duration);
guint       cc_clock_get_duration (CcClock *clock);

GType       cc_clock_get_type     (void);

G_END_DECLS
