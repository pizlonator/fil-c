/*
 *  Copyright © 2022 Christian Persch
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

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define TERMINAL_TYPE_PREFS_PROCESS         (terminal_prefs_process_get_type ())
#define TERMINAL_PREFS_PROCESS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TERMINAL_TYPE_PREFS_PROCESS, TerminalPrefsProcess))
#define TERMINAL_PREFS_PROCESS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), TERMINAL_TYPE_PREFS_PROCESS, TerminalPrefsProcessClass))
#define TERMINAL_IS_PREFS_PROCESS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TERMINAL_TYPE_PREFS_PROCESS))
#define TERMINAL_IS_PREFS_PROCESS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TERMINAL_TYPE_PREFS_PROCESS))
#define TERMINAL_PREFS_PROCESS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TERMINAL_TYPE_PREFS_PROCESS, TerminalPrefsProcessClass))

typedef struct _TerminalPrefsProcess        TerminalPrefsProcess;
typedef struct _TerminalPrefsProcessClass   TerminalPrefsProcessClass;

GType terminal_prefs_process_get_type(void);

void terminal_prefs_process_new_async(GCancellable* cancellable,
                                      GAsyncReadyCallback callback,
                                      void* user_data);

TerminalPrefsProcess* terminal_prefs_process_new_finish(GAsyncResult* result,
                                                        GError** error);

TerminalPrefsProcess* terminal_prefs_process_new_sync(GCancellable* cancellable,
                                                      GError** error);

void terminal_prefs_process_abort(TerminalPrefsProcess* process);

void terminal_prefs_process_show(TerminalPrefsProcess* process,
                                 char const* profile_uuid,
                                 char const* hint,
                                 unsigned timestamp);

G_END_DECLS
