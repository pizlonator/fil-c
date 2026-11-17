/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* cc-time-entry.h
 *
 * Copyright 2020 Purism SPC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
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
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_TIME_ENTRY (cc_time_entry_get_type ())
G_DECLARE_FINAL_TYPE (CcTimeEntry, cc_time_entry, CC, TIME_ENTRY, GtkWidget)

GtkWidget *cc_time_entry_new        (void);
void       cc_time_entry_set_time   (CcTimeEntry *self,
                                     guint        hour,
                                     guint        minute);
guint      cc_time_entry_get_minute (CcTimeEntry *self);
guint      cc_time_entry_get_hour   (CcTimeEntry *self);
gboolean   cc_time_entry_get_is_am  (CcTimeEntry *self);
void       cc_time_entry_set_is_am  (CcTimeEntry *self,
                                     gboolean     is_am);
gboolean   cc_time_entry_get_am_pm  (CcTimeEntry *self);
void       cc_time_entry_set_am_pm  (CcTimeEntry *self,
                                     gboolean     is_am_pm);

G_END_DECLS
