/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * test_tiocsti - test security of TIOCSTI
 *
 * Written by Federico Bento <up201407890@alunos.dcc.fc.up.pt>
 */

#include <stdlib.h>
#include <sys/ioctl.h>

int main(void)
{
	int rc = 0;
	char *cmd = "id -u -n\n";

	while(*cmd)
		rc += ioctl(0, TIOCSTI, cmd++);

	exit(rc ? EXIT_FAILURE : EXIT_SUCCESS);
}
