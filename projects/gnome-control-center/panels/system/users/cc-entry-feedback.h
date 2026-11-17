/*
 * cc-entry-feedback.h
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
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author(s):
 *  Felipe Borges <felipeborges@gnome.org>
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_ENTRY_FEEDBACK (cc_entry_feedback_get_type ())

G_DECLARE_FINAL_TYPE (CcEntryFeedback, cc_entry_feedback, CC, ENTRY_FEEDBACK, GtkBox)

void    cc_entry_feedback_reset  (CcEntryFeedback *self);

void    cc_entry_feedback_update (CcEntryFeedback *self,
                                  const gchar     *icon_name,
                                  const gchar     *text);

G_END_DECLS

