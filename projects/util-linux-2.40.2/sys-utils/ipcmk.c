/*
 *  ipcmk.c - used to create ad-hoc IPC segments
 *
 *  Copyright (C) 2008 Hayden A. James (hayden.james@gmail.com)
 *  Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <errno.h>
#include <getopt.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/time.h>

#include "c.h"
#include "nls.h"
#include "randutils.h"
#include "strutils.h"
#include "closestream.h"

static int create_shm(size_t size, int permission)
{
	key_t key;

	ul_random_get_bytes(&key, sizeof(key));
	return shmget(key, size, permission | IPC_CREAT);
}

static int create_msg(int permission)
{
	key_t key;

	ul_random_get_bytes(&key, sizeof(key));
	return msgget(key, permission | IPC_CREAT);
}

static int create_sem(int nsems, int permission)
{
	key_t key;

	ul_random_get_bytes(&key, sizeof(key));
	return semget(key, nsems, permission | IPC_CREAT);
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options]\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Create various IPC resources.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -M, --shmem <size>       create shared memory segment of size <size>\n"), out);
	fputs(_(" -S, --semaphore <number> create semaphore array with <number> elements\n"), out);
	fputs(_(" -Q, --queue              create message queue\n"), out);
	fputs(_(" -p, --mode <mode>        permission for the resource (default is 0644)\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(26));

	fputs(USAGE_ARGUMENTS, out);
	fprintf(out, USAGE_ARG_SIZE(_("<size>")));

	fprintf(out, USAGE_MAN_TAIL("ipcmk(1)"));

	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int permission = 0644;
	int opt;
	size_t size = 0;
	int nsems = 0;
	int ask_shm = 0, ask_msg = 0, ask_sem = 0;
	static const struct option longopts[] = {
		{"shmem", required_argument, NULL, 'M'},
		{"semaphore", required_argument, NULL, 'S'},
		{"queue", no_argument, NULL, 'Q'},
		{"mode", required_argument, NULL, 'p'},
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	while((opt = getopt_long(argc, argv, "hM:QS:p:Vh", longopts, NULL)) != -1) {
		switch(opt) {
		case 'M':
			size = strtosize_or_err(optarg, _("failed to parse size"));
			ask_shm = 1;
			break;
		case 'Q':
			ask_msg = 1;
			break;
		case 'S':
			nsems = strtos32_or_err(optarg, _("failed to parse elements"));
			ask_sem = 1;
			break;
		case 'p':
		{
			char *end = NULL;
			errno = 0;
			permission = strtoul(optarg, &end, 8);
			if (errno || optarg == end || (end && *end))
				err(EXIT_FAILURE, _("failed to parse mode"));
			break;
		}
		case 'h':
			usage();
		case 'V':
			print_version(EXIT_SUCCESS);
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if(!ask_shm && !ask_msg && !ask_sem) {
		warnx(_("bad usage"));
		errtryhelp(EXIT_FAILURE);
	}
	if (ask_shm) {
		int shmid;
		if (-1 == (shmid = create_shm(size, permission)))
			err(EXIT_FAILURE, _("create share memory failed"));
		else
			printf(_("Shared memory id: %d\n"), shmid);
	}

	if (ask_msg) {
		int msgid;
		if (-1 == (msgid = create_msg(permission)))
			err(EXIT_FAILURE, _("create message queue failed"));
		else
			printf(_("Message queue id: %d\n"), msgid);
	}

	if (ask_sem) {
		int semid;
		if (-1 == (semid = create_sem(nsems, permission)))
			err(EXIT_FAILURE, _("create semaphore failed"));
		else
			printf(_("Semaphore id: %d\n"), semid);
	}

	return EXIT_SUCCESS;
}
