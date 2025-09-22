/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Original author unknown, may be "krishna balasub@cis.ohio-state.edu"
 *
 * Copyright (C) 1995 ike Jagdis <jaggy@purplet.demon.co.uk>
 *               1996 janl@math.uio.no
 * Copyright (C) 2006-2023 Karel Zak <kzak@redhat.com>
 *
 *
 * Patches from Mike Jagdis (jaggy@purplet.demon.co.uk) applied Wed Feb 8
 * 12:12:21 1995 by faith@cs.unc.edu to print numeric uids if no passwd file
 * entry.
 *
 * Patch from arnolds@ifns.de (Heinz-Ado Arnolds) applied Mon Jul 1 19:30:41
 * 1996 by janl@math.uio.no to add code missing in case PID: clauses.
 *
 * Patched to display the key field -- hy@picksys.com 12/18/96
 *
 * 1999-02-22 Arkadiusz Miśkiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 */
#include <errno.h>
#include <getopt.h>

#include "c.h"
#include "nls.h"
#include "closestream.h"
#include "timeutils.h"
#include "strutils.h"

#include "ipcutils.h"

enum output_formats {
	NOTSPECIFIED,
	LIMITS,
	STATUS,
	CREATOR,
	TIME,
	PID
};
enum {
	OPT_HUMAN = CHAR_MAX + 1
};

static void do_shm (char format, int unit);
static void print_shm (int id, int unit);
static void do_sem (char format);
static void print_sem (int id);
static void do_msg (char format, int unit);
static void print_msg (int id, int unit);

static inline char *ctime64(int64_t *t)
{
	static char buf[CTIME_BUFSIZ];

	/* we read time as int64_t from /proc, so cast... */
	ctime_r((time_t *)t, buf);
	return buf;
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %1$s [resource-option...] [output-option]\n"
		       " %1$s -m|-q|-s -i <id>\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Show information on IPC facilities.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -i, --id <id>  print details on resource identified by <id>\n"), out);
	fprintf(out, USAGE_HELP_OPTIONS(16));

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Resource options:\n"), out);
	fputs(_(" -m, --shmems      shared memory segments\n"), out);
	fputs(_(" -q, --queues      message queues\n"), out);
	fputs(_(" -s, --semaphores  semaphores\n"), out);
	fputs(_(" -a, --all         all (default)\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Output options:\n"), out);
	fputs(_(" -t, --time        show attach, detach and change times\n"), out);
	fputs(_(" -p, --pid         show PIDs of creator and last operator\n"), out);
	fputs(_(" -c, --creator     show creator and owner\n"), out);
	fputs(_(" -l, --limits      show resource limits\n"), out);
	fputs(_(" -u, --summary     show status summary\n"), out);
	fputs(_("     --human       show sizes in human-readable format\n"), out);
	fputs(_(" -b, --bytes       show sizes in bytes\n"), out);
	fprintf(out, USAGE_MAN_TAIL("ipcs(1)"));

	exit(EXIT_SUCCESS);
}

int main (int argc, char **argv)
{
	int opt, msg = 0, shm = 0, sem = 0, id = 0, specific = 0;
	char format = NOTSPECIFIED;
	int unit = IPC_UNIT_DEFAULT;
	static const struct option longopts[] = {
		{"id", required_argument, NULL, 'i'},
		{"queues", no_argument, NULL, 'q'},
		{"shmems", no_argument, NULL, 'm'},
		{"semaphores", no_argument, NULL, 's'},
		{"all", no_argument, NULL, 'a'},
		{"time", no_argument, NULL, 't'},
		{"pid", no_argument, NULL, 'p'},
		{"creator", no_argument, NULL, 'c'},
		{"limits", no_argument, NULL, 'l'},
		{"summary", no_argument, NULL, 'u'},
		{"human", no_argument, NULL, OPT_HUMAN},
		{"bytes", no_argument, NULL, 'b'},
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};
	char options[] = "i:qmsatpclubVh";

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	while ((opt = getopt_long(argc, argv, options, longopts, NULL)) != -1) {
		switch (opt) {
		case 'i':
			id = strtos32_or_err(optarg, _("failed to parse id argument"));
			specific = 1;
			break;
		case 'a':
			msg = shm = sem = 1;
			break;
		case 'q':
			msg = 1;
			break;
		case 'm':
			shm = 1;
			break;
		case 's':
			sem = 1;
			break;
		case 't':
			format = TIME;
			break;
		case 'c':
			format = CREATOR;
			break;
		case 'p':
			format = PID;
			break;
		case 'l':
			format = LIMITS;
			break;
		case 'u':
			format = STATUS;
			break;
		case OPT_HUMAN:
			unit = IPC_UNIT_HUMAN;
			break;
		case 'b':
			unit = IPC_UNIT_BYTES;
			break;

		case 'h':
			usage();
		case 'V':
			print_version(EXIT_SUCCESS);
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (specific && (msg + shm + sem != 1))
		errx (EXIT_FAILURE,
		      _("when using an ID, a single resource must be specified"));
	if (specific) {
		if (msg)
			print_msg (id, unit);
		if (shm)
			print_shm (id, unit);
		if (sem)
			print_sem (id);
	} else {
		if (!msg && !shm && !sem)
			msg = shm = sem = 1;
		printf ("\n");
		if (msg) {
			do_msg (format, unit);
			printf ("\n");
		}
		if (shm) {
			do_shm (format, unit);
			printf ("\n");
		}
		if (sem) {
			do_sem (format);
			printf ("\n");
		}
	}
	return EXIT_SUCCESS;
}

static void do_shm (char format, int unit)
{
	struct passwd *pw;
	struct shm_data *shmds, *shmdsp;

	switch (format) {
	case LIMITS:
	{
		struct ipc_limits lim;
		uint64_t tmp, pgsz = getpagesize();

		if (ipc_shm_get_limits(&lim)) {
			printf (_("unable to fetch shared memory limits\n"));
			return;
		}
		printf (_("------ Shared Memory Limits --------\n"));
		printf (_("max number of segments = %ju\n"), lim.shmmni);
		ipc_print_size(unit == IPC_UNIT_DEFAULT ? IPC_UNIT_KB : unit,
			       _("max seg size"), lim.shmmax, "\n", 0);

		if (unit == IPC_UNIT_KB || unit == IPC_UNIT_DEFAULT) {
			tmp = (uint64_t) lim.shmall * (pgsz / 1024);
			if (lim.shmall != 0 && tmp / lim.shmall != pgsz / 1024)
				tmp = UINT64_MAX - (UINT64_MAX % (pgsz / 1024));

			ipc_print_size(IPC_UNIT_DEFAULT, _("max total shared memory (kbytes)"), tmp, "\n", 0);
		}
		else {
			tmp = (uint64_t) lim.shmall * pgsz;
			/* overflow handling, at least we don't print ridiculous small values */
			if (lim.shmall != 0 && tmp / lim.shmall != pgsz)
			        tmp = UINT64_MAX - (UINT64_MAX % pgsz);

			ipc_print_size(unit, _("max total shared memory"), tmp, "\n", 0);
		}
		ipc_print_size(unit == IPC_UNIT_DEFAULT ? IPC_UNIT_BYTES : unit,
			       _("min seg size"), lim.shmmin, "\n", 0);
		return;
	}
	case STATUS:
	{
		int maxid;
		struct shmid_ds shmbuf;
		struct shm_info *shm_info;

		maxid = shmctl (0, SHM_INFO, &shmbuf);
		shm_info =  (struct shm_info *) &shmbuf;
		if (maxid < 0) {
			printf (_("kernel not configured for shared memory\n"));
			return;
		}

		printf (_("------ Shared Memory Status --------\n"));
		/*
		 * TRANSLATORS: This output format is maintained for backward
		 * compatibility as ipcs is used in scripts. For consistency
		 * with the rest, the translated form can follow this model:
		 *
		 * "segments allocated = %d\n"
		 * "pages allocated = %ld\n"
		 * "pages resident = %ld\n"
		 * "pages swapped = %ld\n"
		 * "swap performance = %ld attempts, %ld successes\n"
		 */
		printf (_("segments allocated %d\n"
			  "pages allocated %ld\n"
			  "pages resident  %ld\n"
			  "pages swapped   %ld\n"
			  "Swap performance: %ld attempts\t %ld successes\n"),
			shm_info->used_ids,
			shm_info->shm_tot,
			shm_info->shm_rss,
			shm_info->shm_swp,
			shm_info->swap_attempts, shm_info->swap_successes);
		return;
	}

	/*
	 * Headers only
	 */
	case CREATOR:
		printf (_("------ Shared Memory Segment Creators/Owners --------\n"));
		printf ("%-10s %-10s %-10s %-10s %-10s %-10s\n",
			_("shmid"),_("perms"),_("cuid"),_("cgid"),_("uid"),_("gid"));
		break;

	case TIME:
		printf (_("------ Shared Memory Attach/Detach/Change Times --------\n"));
		printf ("%-10s %-10s %-20s %-20s %-20s\n",
			_("shmid"),_("owner"),_("attached"),_("detached"),
			_("changed"));
		break;

	case PID:
		printf (_("------ Shared Memory Creator/Last-op PIDs --------\n"));
		printf ("%-10s %-10s %-10s %-10s\n",
			_("shmid"),_("owner"),_("cpid"),_("lpid"));
		break;

	default:
		printf (_("------ Shared Memory Segments --------\n"));
		printf ("%-10s %-10s %-10s %-10s %-10s %-10s %-12s\n",
			_("key"),_("shmid"),_("owner"),_("perms"),
			unit == IPC_UNIT_HUMAN ? _("size") : _("bytes"),
			_("nattch"),_("status"));
		break;
	}

	/*
	 * Print data
	 */
	if (ipc_shm_get_info(-1, &shmds) < 1)
		return;

	for (shmdsp = shmds; shmdsp->next != NULL; shmdsp = shmdsp->next) {
		if (format == CREATOR)  {
			ipc_print_perms(stdout, &shmdsp->shm_perm);
			continue;
		}
		pw = getpwuid(shmdsp->shm_perm.uid);
		switch (format) {
		case TIME:
			if (pw)
				printf ("%-10d %-10.10s", shmdsp->shm_perm.id, pw->pw_name);
			else
				printf ("%-10d %-10u", shmdsp->shm_perm.id, shmdsp->shm_perm.uid);
			/* ctime uses static buffer: use separate calls */
			printf(" %-20.16s", shmdsp->shm_atim
			       ? ctime64(&shmdsp->shm_atim) + 4 : _("Not set"));
			printf(" %-20.16s", shmdsp->shm_dtim
			       ? ctime64(&shmdsp->shm_dtim) + 4 : _("Not set"));
			printf(" %-20.16s\n", shmdsp->shm_ctim
			       ? ctime64(&shmdsp->shm_ctim) + 4 : _("Not set"));
			break;
		case PID:
			if (pw)
				printf ("%-10d %-10.10s", shmdsp->shm_perm.id, pw->pw_name);
			else
				printf ("%-10d %-10u", shmdsp->shm_perm.id, shmdsp->shm_perm.uid);
			printf (" %-10u %-10u\n",
				shmdsp->shm_cprid, shmdsp->shm_lprid);
			break;

		default:
			printf("0x%08x ", shmdsp->shm_perm.key);
			if (pw)
				printf ("%-10d %-10.10s", shmdsp->shm_perm.id, pw->pw_name);
			else
				printf ("%-10d %-10u", shmdsp->shm_perm.id, shmdsp->shm_perm.uid);
			printf (" %-10o ", shmdsp->shm_perm.mode & 0777);

			if (unit == IPC_UNIT_HUMAN)
				ipc_print_size(unit, NULL, shmdsp->shm_segsz, "    ", 6);
			else
				ipc_print_size(unit, NULL, shmdsp->shm_segsz, NULL, -10);

			printf (" %-10ju %-6s %-6s\n",
				shmdsp->shm_nattch,
				shmdsp->shm_perm.mode & SHM_DEST ? _("dest") : " ",
				shmdsp->shm_perm.mode & SHM_LOCKED ? _("locked") : " ");
			break;
		}
	}

	ipc_shm_free_info(shmds);
}

static void do_sem (char format)
{
	struct passwd *pw;
	struct sem_data *semds, *semdsp;

	switch (format) {
	case LIMITS:
	{
		struct ipc_limits lim;

		if (ipc_sem_get_limits(&lim)) {
			printf (_("unable to fetch semaphore limits\n"));
			return;
		}
		printf (_("------ Semaphore Limits --------\n"));
		printf (_("max number of arrays = %d\n"), lim.semmni);
		printf (_("max semaphores per array = %d\n"), lim.semmsl);
		printf (_("max semaphores system wide = %d\n"), lim.semmns);
		printf (_("max ops per semop call = %d\n"), lim.semopm);
		printf (_("semaphore max value = %u\n"), lim.semvmx);
		return;
	}
	case STATUS:
	{
		struct seminfo seminfo;
		union semun arg;
		arg.array = (ushort *)  (void *) &seminfo;
		if (semctl (0, 0, SEM_INFO, arg) < 0) {
			printf (_("kernel not configured for semaphores\n"));
			return;
		}
		printf (_("------ Semaphore Status --------\n"));
		printf (_("used arrays = %d\n"), seminfo.semusz);
		printf (_("allocated semaphores = %d\n"), seminfo.semaem);
		return;
	}

	case CREATOR:
		printf (_("------ Semaphore Arrays Creators/Owners --------\n"));
		printf ("%-10s %-10s %-10s %-10s %-10s %-10s\n",
			_("semid"),_("perms"),_("cuid"),_("cgid"),_("uid"),_("gid"));
		break;

	case TIME:
		printf (_("------ Semaphore Operation/Change Times --------\n"));
		printf ("%-8s %-10s %-26.24s %-26.24s\n",
			_("semid"),_("owner"),_("last-op"),_("last-changed"));
		break;

	case PID:
		break;

	default:
		printf (_("------ Semaphore Arrays --------\n"));
		printf ("%-10s %-10s %-10s %-10s %-10s\n",
			_("key"),_("semid"),_("owner"),_("perms"),_("nsems"));
		break;
	}

	/*
	 * Print data
	 */
	if (ipc_sem_get_info(-1, &semds) < 1)
		return;

	for (semdsp = semds; semdsp->next != NULL; semdsp = semdsp->next) {
		if (format == CREATOR)  {
			ipc_print_perms(stdout, &semdsp->sem_perm);
			continue;
		}
		pw = getpwuid(semdsp->sem_perm.uid);
		switch (format) {
		case TIME:
			if (pw)
				printf ("%-8d %-10.10s", semdsp->sem_perm.id, pw->pw_name);
			else
				printf ("%-8d %-10u", semdsp->sem_perm.id, semdsp->sem_perm.uid);
			printf ("  %-26.24s", semdsp->sem_otime
				? ctime64(&semdsp->sem_otime) : _("Not set"));
			printf (" %-26.24s\n", semdsp->sem_ctime
				? ctime64( &semdsp->sem_ctime) : _("Not set"));
			break;
		case PID:
			break;

		default:
			printf("0x%08x ", semdsp->sem_perm.key);
			if (pw)
				printf ("%-10d %-10.10s", semdsp->sem_perm.id, pw->pw_name);
			else
				printf ("%-10d %-10u", semdsp->sem_perm.id, semdsp->sem_perm.uid);
			printf (" %-10o %-10ju\n",
				semdsp->sem_perm.mode & 0777,
				semdsp->sem_nsems);
			break;
		}
	}

	ipc_sem_free_info(semds);
}

static void do_msg (char format, int unit)
{
	struct passwd *pw;
	struct msg_data *msgds, *msgdsp;

	switch (format) {
	case LIMITS:
	{
		struct ipc_limits lim;

		if (ipc_msg_get_limits(&lim)) {
			printf (_("unable to fetch message limits\n"));
			return;
		}
		printf (_("------ Messages Limits --------\n"));
		printf (_("max queues system wide = %d\n"), lim.msgmni);
		ipc_print_size(unit == IPC_UNIT_DEFAULT ? IPC_UNIT_BYTES : unit,
			       _("max size of message"), lim.msgmax, "\n", 0);
		ipc_print_size(unit == IPC_UNIT_DEFAULT ? IPC_UNIT_BYTES : unit,
			       _("default max size of queue"), lim.msgmnb, "\n", 0);
		return;
	}
	case STATUS:
	{
		struct msginfo msginfo;
		if (msgctl (0, MSG_INFO, (struct msqid_ds *) (void *) &msginfo) < 0) {
			printf (_("kernel not configured for message queues\n"));
			return;
		}
		printf (_("------ Messages Status --------\n"));
#ifndef __FreeBSD_kernel__
		printf (_("allocated queues = %d\n"), msginfo.msgpool);
		printf (_("used headers = %d\n"), msginfo.msgmap);
#endif
		ipc_print_size(unit, _("used space"), msginfo.msgtql,
			       unit == IPC_UNIT_DEFAULT ? _(" bytes\n") : "\n", 0);
		return;
	}
	case CREATOR:
		printf (_("------ Message Queues Creators/Owners --------\n"));
		printf ("%-10s %-10s %-10s %-10s %-10s %-10s\n",
			_("msqid"),_("perms"),_("cuid"),_("cgid"),_("uid"),_("gid"));
		break;

	case TIME:
		printf (_("------ Message Queues Send/Recv/Change Times --------\n"));
		printf ("%-8s %-10s %-20s %-20s %-20s\n",
			_("msqid"),_("owner"),_("send"),_("recv"),_("change"));
		break;

	case PID:
		printf (_("------ Message Queues PIDs --------\n"));
		printf ("%-10s %-10s %-10s %-10s\n",
			_("msqid"),_("owner"),_("lspid"),_("lrpid"));
		break;

	default:
		printf (_("------ Message Queues --------\n"));
		printf ("%-10s %-10s %-10s %-10s %-12s %-12s\n",
			_("key"), _("msqid"), _("owner"), _("perms"),
			unit == IPC_UNIT_HUMAN ? _("size") : _("used-bytes"),
			_("messages"));
		break;
	}

	/*
	 * Print data
	 */
	if (ipc_msg_get_info(-1, &msgds) < 1)
		return;

	for (msgdsp = msgds; msgdsp->next != NULL; msgdsp = msgdsp->next) {
		if (format == CREATOR) {
			ipc_print_perms(stdout, &msgdsp->msg_perm);
			continue;
		}
		pw = getpwuid(msgdsp->msg_perm.uid);
		switch (format) {
		case TIME:
			if (pw)
				printf ("%-8d %-10.10s", msgdsp->msg_perm.id, pw->pw_name);
			else
				printf ("%-8d %-10u", msgdsp->msg_perm.id, msgdsp->msg_perm.uid);
			printf (" %-20.16s", msgdsp->q_stime
				? ctime64(&msgdsp->q_stime) + 4 : _("Not set"));
			printf (" %-20.16s", msgdsp->q_rtime
				? ctime64(&msgdsp->q_rtime) + 4 : _("Not set"));
			printf (" %-20.16s\n", msgdsp->q_ctime
				? ctime64(&msgdsp->q_ctime) + 4 : _("Not set"));
			break;
		case PID:
			if (pw)
				printf ("%-8d %-10.10s", msgdsp->msg_perm.id, pw->pw_name);
			else
				printf ("%-8d %-10u", msgdsp->msg_perm.id, msgdsp->msg_perm.uid);
			printf ("  %5d     %5d\n",
				msgdsp->q_lspid, msgdsp->q_lrpid);
			break;

		default:
			printf( "0x%08x ",msgdsp->msg_perm.key );
			if (pw)
				printf ("%-10d %-10.10s", msgdsp->msg_perm.id, pw->pw_name);
			else
				printf ("%-10d %-10u", msgdsp->msg_perm.id, msgdsp->msg_perm.uid);
			printf (" %-10o ", msgdsp->msg_perm.mode & 0777);

			if (unit == IPC_UNIT_HUMAN)
				ipc_print_size(unit, NULL, msgdsp->q_cbytes, "      ", 6);
			else
				ipc_print_size(unit, NULL, msgdsp->q_cbytes, NULL, -12);

			printf (" %-12ju\n", msgdsp->q_qnum);
			break;
		}
	}

	ipc_msg_free_info(msgds);
}

static void print_shm(int shmid, int unit)
{
	struct shm_data *shmdata;

	if (ipc_shm_get_info(shmid, &shmdata) < 1) {
		warnx(_("id %d not found"), shmid);
		return;
	}

	printf(_("\nShared memory Segment shmid=%d\n"), shmid);
	printf(_("uid=%u\tgid=%u\tcuid=%u\tcgid=%u\n"),
	       shmdata->shm_perm.uid, shmdata->shm_perm.gid,
	       shmdata->shm_perm.cuid, shmdata->shm_perm.cgid);
	printf(_("mode=%#o\taccess_perms=%#o\n"), shmdata->shm_perm.mode,
	       shmdata->shm_perm.mode & 0777);
	ipc_print_size(unit, unit == IPC_UNIT_HUMAN ? _("size=") : _("bytes="),
		       shmdata->shm_segsz, "\t", 0);
	printf(_("lpid=%u\tcpid=%u\tnattch=%jd\n"),
	       shmdata->shm_lprid, shmdata->shm_cprid,
	       shmdata->shm_nattch);
	printf(_("att_time=%-26.24s\n"),
	       shmdata->shm_atim ? ctime64(&(shmdata->shm_atim)) : _("Not set"));
	printf(_("det_time=%-26.24s\n"),
	       shmdata->shm_dtim ? ctime64(&shmdata->shm_dtim) : _("Not set"));
	printf(_("change_time=%-26.24s\n"), ctime64(&shmdata->shm_ctim));
	printf("\n");

	ipc_shm_free_info(shmdata);
}

static void print_msg(int msgid, int unit)
{
	struct msg_data *msgdata;

	if (ipc_msg_get_info(msgid, &msgdata) < 1) {
		warnx(_("id %d not found"), msgid);
		return;
	}

	printf(_("\nMessage Queue msqid=%d\n"), msgid);
	printf(_("uid=%u\tgid=%u\tcuid=%u\tcgid=%u\tmode=%#o\n"),
	       msgdata->msg_perm.uid, msgdata->msg_perm.gid,
	       msgdata->msg_perm.cuid, msgdata->msg_perm.cgid,
	       msgdata->msg_perm.mode);
	ipc_print_size(unit, unit == IPC_UNIT_HUMAN ? _("csize=") : _("cbytes="),
		       msgdata->q_cbytes, "\t", 0);
	ipc_print_size(unit, unit == IPC_UNIT_HUMAN ? _("qsize=") : _("qbytes="),
		       msgdata->q_qbytes, "\t", 0);
	printf("qnum=%jd\tlspid=%d\tlrpid=%d\n",
	       msgdata->q_qnum,
	       msgdata->q_lspid, msgdata->q_lrpid);
	printf(_("send_time=%-26.24s\n"),
	       msgdata->q_stime ? ctime64(&msgdata->q_stime) : _("Not set"));
	printf(_("rcv_time=%-26.24s\n"),
	       msgdata->q_rtime ? ctime64(&msgdata->q_rtime) : _("Not set"));
	printf(_("change_time=%-26.24s\n"),
	       msgdata->q_ctime ? ctime64(&msgdata->q_ctime) : _("Not set"));
	printf("\n");

	ipc_msg_free_info(msgdata);
}

static void print_sem(int semid)
{
	struct sem_data *semdata;
	size_t i;

	if (ipc_sem_get_info(semid, &semdata) < 1) {
		warnx(_("id %d not found"), semid);
		return;
	}

	printf(_("\nSemaphore Array semid=%d\n"), semid);
	printf(_("uid=%u\t gid=%u\t cuid=%u\t cgid=%u\n"),
	       semdata->sem_perm.uid, semdata->sem_perm.gid,
	       semdata->sem_perm.cuid, semdata->sem_perm.cgid);
	printf(_("mode=%#o, access_perms=%#o\n"),
	       semdata->sem_perm.mode, semdata->sem_perm.mode & 0777);
	printf(_("nsems = %ju\n"), semdata->sem_nsems);
	printf(_("otime = %-26.24s\n"),
	       semdata->sem_otime ? ctime64(&semdata->sem_otime) : _("Not set"));
	printf(_("ctime = %-26.24s\n"), ctime64(&semdata->sem_ctime));

	printf("%-10s %-10s %-10s %-10s %-10s\n",
	       _("semnum"), _("value"), _("ncount"), _("zcount"), _("pid"));

	for (i = 0; i < semdata->sem_nsems; i++) {
		struct sem_elem *e = &semdata->elements[i];
		printf("%-10zu %-10d %-10d %-10d %-10d\n",
		       i, e->semval, e->ncount, e->zcount, e->pid);
	}
	printf("\n");
	ipc_sem_free_info(semdata);
}
