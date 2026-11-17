/*
 * Copyright © 2013 Christian Persch
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

#ifndef TERMINAL_PREFS_H
#define TERMINAL_PREFS_H

#include <gtk/gtk.h>

#include "terminal-profiles-list.hh"

G_BEGIN_DECLS

/* FIXME move back to the .c file if profile-editor.c is also merged there,
 * also remove the terminal-profiles-list.h incude above. */
/* FIXME PrefData is a very bad name, rename to PrefsDialog maybe? */

/* Everything about a preferences dialog */
typedef struct {
  TerminalSettingsList *profiles_list;

  GSettings *selected_profile;
  GtkListBoxRow *selected_list_box_row;
  char *selected_profile_uuid;  /* a copy thereof, to survive changes to profiles_list */

  GtkBuilder *builder;
  GtkWidget *dialog;
  GtkListBox *listbox;
  GtkWidget *new_profile_button;
  GtkWidget *stack;

  GArray *profile_signals;
  GArray *profile_bindings;
} PrefData;

extern PrefData *the_pref_data;  /* global */

void terminal_prefs_show_preferences(GSettings* profile,
                                     char const* widget_name,
                                     unsigned timestamp);

G_END_DECLS

#endif /* TERMINAL_PREFS_H */
