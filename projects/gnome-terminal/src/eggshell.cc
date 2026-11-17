/*
 * Copyright (C) 1997, 1998, 1999, 2000 Free Software Foundation
 * Copyright (C) 1999, 2000 Red Hat, Inc.
 * All rights reserved.
 *
 * This file is part of the Gnome Library.
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
/*
  @NOTATION@
 */

/*
 *
 * Gnome utility routines.
 * (C)  1997, 1998, 1999 the Free Software Foundation.
 *
 * Author: Miguel de Icaza,
 */

#include <config.h>

#include "eggshell.hh"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#ifndef G_OS_WIN32
#include <pwd.h>
#endif

#include <glib.h>

/**
 * egg_shell:
 * @shell: the value of the SHELL env variable
 *
 * Retrieves the user's preferred shell.
 *
 * Returns: A newly allocated string that is the path to the shell.
 */
char *
egg_shell (const char *shell)
{
#ifndef G_OS_WIN32
	struct passwd *pw;
	int i;
	static const char shells [][14] = {
		/* Note that on some systems shells can also
		 * be installed in /usr/bin */
		"/bin/bash", "/usr/bin/bash",
		"/bin/zsh", "/usr/bin/zsh",
		"/bin/tcsh", "/usr/bin/tcsh",
		"/bin/ksh", "/usr/bin/ksh",
		"/bin/csh", "/bin/sh"
	};

	if (geteuid () == getuid () &&
	    getegid () == getgid ()) {
		/* only in non-setuid */
		if (shell != nullptr) {
			if (access (shell, X_OK) == 0) {
				return g_strdup (shell);
			}
		}
	}
	pw = getpwuid(getuid());
	if (pw && pw->pw_shell) {
		if (access (pw->pw_shell, X_OK) == 0) {
			return g_strdup (pw->pw_shell);
		}
	}

	for (i = 0; i != G_N_ELEMENTS (shells); i++) {
		if (access (shells [i], X_OK) == 0) {
			return g_strdup (shells[i]);
		}
	}

	/* If /bin/sh doesn't exist, your system is truly broken.  */
	g_assert_not_reached ();

	/* Placate compiler.  */
	return nullptr;
#else
	/* g_find_program_in_path() always looks also in the Windows
	 * and System32 directories, so it should always find either cmd.exe
	 * or command.com.
	 */
	char *retval = g_find_program_in_path ("cmd.exe");

	if (retval == nullptr)
		retval = g_find_program_in_path ("command.com");

	g_assert (retval != nullptr);

	return retval;
#endif
}
