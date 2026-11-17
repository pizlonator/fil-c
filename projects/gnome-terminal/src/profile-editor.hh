/*
 * Copyright © 2002 Havoc Pennington
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
 */

#ifndef TERMINAL_PROFILE_EDITOR_H
#define TERMINAL_PROFILE_EDITOR_H

#include <gio/gio.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

void profile_prefs_init (void);

void profile_prefs_destroy (void);

void profile_prefs_unload (void);

void profile_prefs_load (const char *uuid,
                         GSettings *profile);

G_END_DECLS

#endif /* TERMINAL_PROFILE_EDITOR_H */
