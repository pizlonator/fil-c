/*
 * Copyright (C) 1994-2005 Jeff Tranter (tranter@pobox.com)
 * Copyright (C) 2012 Karel Zak <kzak@redhat.com>
 * Copyright (C) Michal Luscon <mluscon@redhat.com>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <err.h>
#include <stdarg.h>

#include <getopt.h>
#include <errno.h>
#include <regex.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/mtio.h>
#include <linux/cdrom.h>
#include <linux/fd.h>
#include <sys/mount.h>
#include <scsi/scsi.h>
#include <scsi/sg.h>
#include <scsi/scsi_ioctl.h>
#include <sys/time.h>

#include <libmount.h>

#include "c.h"
#include "closestream.h"
#include "nls.h"
#include "strutils.h"
#include "xalloc.h"
#include "pathnames.h"
#include "sysfs.h"
#include "monotonic.h"

/*
 * sg_io_hdr_t driver_status -- see kernel include/scsi/sg.h
 */
#ifndef DRIVER_SENSE
# define DRIVER_SENSE	0x08
#endif


#define EJECT_DEFAULT_DEVICE "/dev/cdrom"


/* Used by the toggle_tray() function. If ejecting the tray takes this
 * time or less, the tray was probably already ejected, so we close it
 * again.
 */
#define TRAY_WAS_ALREADY_OPEN_USECS  200000	/* about 0.2 seconds */

struct eject_control {
	struct libmnt_table *mtab;
	char *device;			/* device or mount point to be ejected */
	int fd;				/* file descriptor for device */
	unsigned int 			/* command flags and arguments */
		a_option:1,
		c_option:1,
		d_option:1,
		F_option:1,
		f_option:1,
		i_option:1,
		M_option:1,
		m_option:1,
		n_option:1,
		p_option:1,
		q_option:1,
		r_option:1,
		s_option:1,
		T_option:1,
		t_option:1,
		v_option:1,
		X_option:1,
		x_option:1,
		a_arg:1,
		i_arg:1;

	unsigned int force_exclusive;	/* use O_EXCL */

	long int c_arg;			/* changer slot number */
	long int x_arg;			/* cd speed */
};

static void vinfo(const char *fmt, va_list va)
{
	fprintf(stdout, "%s: ", program_invocation_short_name);
	vprintf(fmt, va);
	fputc('\n', stdout);
}

static inline void __attribute__ ((__format__ (__printf__, 2, 3)))
	verbose(const struct eject_control *ctl, const char *fmt, ...)
{
	va_list va;

	if (!ctl->v_option)
		return;

	va_start(va, fmt);
	vinfo(fmt, va);
	va_end(va);
}

static inline __attribute__ ((__format__ (__printf__, 1, 2)))
	void info(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	vinfo(fmt, va);
	va_end(va);
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out,
		_(" %s [options] [<device>|<mountpoint>]\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Eject removable media.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -a, --auto <on|off>         turn auto-eject feature on or off\n"
		" -c, --changerslot <slot>    switch discs on a CD-ROM changer\n"
		" -d, --default               display default device\n"
		" -f, --floppy                eject floppy\n"
		" -F, --force                 don't care about device type\n"
		" -i, --manualeject <on|off>  toggle manual eject protection on/off\n"
		" -m, --no-unmount            do not unmount device even if it is mounted\n"
		" -M, --no-partitions-unmount do not unmount another partitions\n"
		" -n, --noop                  don't eject, just show device found\n"
		" -p, --proc                  use /proc/mounts instead of /etc/mtab\n"
		" -q, --tape                  eject tape\n"
		" -r, --cdrom                 eject CD-ROM\n"
		" -s, --scsi                  eject SCSI device\n"
		" -t, --trayclose             close tray\n"
		" -T, --traytoggle            toggle tray\n"
		" -v, --verbose               enable verbose output\n"
		" -x, --cdspeed <speed>       set CD-ROM max speed\n"
		" -X, --listspeed             list CD-ROM available speeds\n"),
		out);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(29));

	fputs(_("\nBy default tries -r, -s, -f, and -q in order until success.\n"), out);
	fprintf(out, USAGE_MAN_TAIL("eject(1)"));

	exit(EXIT_SUCCESS);
}


/* Handle command line options. */
static void parse_args(struct eject_control *ctl, int argc, char **argv)
{
	static const struct option long_opts[] =
	{
		{"auto",	required_argument, NULL, 'a'},
		{"cdrom",	no_argument,	   NULL, 'r'},
		{"cdspeed",	required_argument, NULL, 'x'},
		{"changerslot", required_argument, NULL, 'c'},
		{"default",	no_argument,	   NULL, 'd'},
		{"floppy",	no_argument,	   NULL, 'f'},
		{"force",       no_argument,       NULL, 'F'},
		{"help",	no_argument,	   NULL, 'h'},
		{"listspeed",   no_argument,       NULL, 'X'},
		{"manualeject", required_argument, NULL, 'i'},
		{"noop",	no_argument,	   NULL, 'n'},
		{"no-unmount",	no_argument,	   NULL, 'm'},
		{"no-partitions-unmount", no_argument, NULL, 'M' },
		{"proc",	no_argument,	   NULL, 'p'},
		{"scsi",	no_argument,	   NULL, 's'},
		{"tape",	no_argument,	   NULL, 'q'},
		{"trayclose",	no_argument,	   NULL, 't'},
		{"traytoggle",	no_argument,	   NULL, 'T'},
		{"verbose",	no_argument,	   NULL, 'v'},
		{"version",	no_argument,	   NULL, 'V'},
		{NULL, 0, NULL, 0}
	};
	int c;

	while ((c = getopt_long(argc, argv,
				"a:c:i:x:dfFhnqrstTXvVpmM", long_opts, NULL)) != -1) {
		switch (c) {
		case 'a':
			ctl->a_option = 1;
			ctl->a_arg = parse_switch(optarg, _("argument error"),
						"on", "off",  "1", "0",  NULL);
			break;
		case 'c':
			ctl->c_option = 1;
			ctl->c_arg = strtoul_or_err(optarg, _("invalid argument to --changerslot/-c option"));
			break;
		case 'x':
			ctl->x_option = 1;
			ctl->x_arg = strtoul_or_err(optarg, _("invalid argument to --cdspeed/-x option"));
			break;
		case 'd':
			ctl->d_option = 1;
			break;
		case 'f':
			ctl->f_option = 1;
			break;
		case 'F':
			ctl->F_option = 1;
			break;
		case 'i':
			ctl->i_option = 1;
			ctl->i_arg = parse_switch(optarg, _("argument error"),
						"on", "off",  "1", "0",  NULL);
			break;
		case 'm':
			ctl->m_option = 1;
			break;
		case 'M':
			ctl->M_option = 1;
			break;
		case 'n':
			ctl->n_option = 1;
			break;
		case 'p':
			ctl->p_option = 1;
			break;
		case 'q':
			ctl->q_option = 1;
			break;
		case 'r':
			ctl->r_option = 1;
			break;
		case 's':
			ctl->s_option = 1;
			break;
		case 't':
			ctl->t_option = 1;
			break;
		case 'T':
			ctl->T_option = 1;
			break;
		case 'X':
			ctl->X_option = 1;
			break;
		case 'v':
			ctl->v_option = 1;
			break;

		case 'h':
			usage();
		case 'V':
			print_version(EXIT_SUCCESS);
		default:
			errtryhelp(EXIT_FAILURE);
			break;
		}
	}

	/* check for a single additional argument */
	if ((argc - optind) > 1)
		errx(EXIT_FAILURE, _("too many arguments"));

	if ((argc - optind) == 1)
		ctl->device = xstrdup(argv[optind]);
}

/*
 * Given name, such as foo, see if any of the following exist:
 *
 * foo (if foo starts with '.' or '/')
 * /dev/foo
 *
 * If found, return the full path. If not found, return 0.
 * Returns pointer to dynamically allocated string.
 */
static char *find_device(const char *name)
{
	if (!name)
		return NULL;

	if ((*name == '.' || *name == '/') && access(name, F_OK) == 0)
		return xstrdup(name);

	char buf[PATH_MAX];

	snprintf(buf, sizeof(buf), "/dev/%s", name);
	if (access(buf, F_OK) == 0)
		return xstrdup(buf);

	return NULL;
}

/* Set or clear auto-eject mode. */
static void auto_eject(const struct eject_control *ctl)
{
	int status = -1;

#if defined(CDROM_SET_OPTIONS) && defined(CDROM_CLEAR_OPTIONS)
	if (ctl->a_arg)
		status = ioctl(ctl->fd, CDROM_SET_OPTIONS, CDO_AUTO_EJECT);
	else
		status = ioctl(ctl->fd, CDROM_CLEAR_OPTIONS, CDO_AUTO_EJECT);
#else
	errno = ENOSYS;
#endif
	if (status < 0)
		err(EXIT_FAILURE,_("CD-ROM auto-eject command failed"));
}

/*
 * Stops CDROM from opening on manual eject button press.
 * This can be useful when you carry your laptop
 * in your bag while it's on and no CD inserted in it's drive.
 * Implemented as found in Documentation/userspace-api/ioctl/cdrom.rst
 */
static void manual_eject(const struct eject_control *ctl)
{
	if (ioctl(ctl->fd, CDROM_LOCKDOOR, ctl->i_arg) < 0) {
		switch (errno) {
		case EDRIVE_CANT_DO_THIS:
			errx(EXIT_FAILURE, _("CD-ROM door lock is not supported"));
		case EBUSY:
			errx(EXIT_FAILURE, _("other users have the drive open and not CAP_SYS_ADMIN"));
		default:
			err(EXIT_FAILURE, _("CD-ROM lock door command failed"));
		}
	}

	if (ctl->i_arg)
		info(_("CD-Drive may NOT be ejected with device button"));
	else
		info(_("CD-Drive may be ejected with device button"));
}

/*
 * Changer select. CDROM_SELECT_DISC is preferred, older kernels used
 * CDROMLOADFROMSLOT.
 */
static void changer_select(const struct eject_control *ctl)
{
#ifdef CDROM_SELECT_DISC
	if (ioctl(ctl->fd, CDROM_SELECT_DISC, ctl->c_arg) < 0)
		err(EXIT_FAILURE, _("CD-ROM select disc command failed"));

#elif defined CDROMLOADFROMSLOT
	if (ioctl(ctl->fd, CDROMLOADFROMSLOT, ctl->c_arg) != 0)
		err(EXIT_FAILURE, _("CD-ROM load from slot command failed"));
#else
	warnx(_("IDE/ATAPI CD-ROM changer not supported by this kernel\n") );
#endif
}

/*
 * Close tray. Not supported by older kernels.
 */
static void close_tray(int fd)
{
	int status;

#if defined(CDROMCLOSETRAY) || defined(CDIOCCLOSE)
#if defined(CDROMCLOSETRAY)
	status = ioctl(fd, CDROMCLOSETRAY);
#elif defined(CDIOCCLOSE)
	status = ioctl(fd, CDIOCCLOSE);
#endif
	if (status != 0)
		err(EXIT_FAILURE, _("CD-ROM tray close command failed"));
#else
	warnx(_("CD-ROM tray close command not supported by this kernel\n"));
#endif
}

/*
 * Eject using CDROMEJECT ioctl.
 */
static int eject_cdrom(int fd)
{
#if defined(CDROMEJECT)
	int ret = ioctl(fd, CDROM_LOCKDOOR, 0);
	if (ret < 0)
		return 0;
	return ioctl(fd, CDROMEJECT) >= 0;
#elif defined(CDIOCEJECT)
	return ioctl(fd, CDIOCEJECT) >= 0;
#else
	warnx(_("CD-ROM eject unsupported"));
	errno = ENOSYS;
	return 0;
#endif
}

/*
 * Toggle tray.
 *
 * Written by Benjamin Schwenk <benjaminschwenk@yahoo.de> and
 * Sybren Stuvel <sybren@thirdtower.com>
 *
 * Not supported by older kernels because it might use
 * CloseTray().
 *
 */
static void toggle_tray(int fd)
{
#ifdef CDROM_DRIVE_STATUS
	/* First ask the CDROM for info, otherwise fall back to manual.  */
	switch (ioctl(fd, CDROM_DRIVE_STATUS)) {
	case CDS_TRAY_OPEN:
		close_tray(fd);
		return;

	case CDS_NO_DISC:
	case CDS_DISC_OK:
		if (!eject_cdrom(fd))
			err(EXIT_FAILURE, _("CD-ROM eject command failed"));
		return;
	case CDS_NO_INFO:
		warnx(_("no CD-ROM information available"));
		return;
	case CDS_DRIVE_NOT_READY:
		warnx(_("CD-ROM drive is not ready"));
		return;
	default:
		err(EXIT_FAILURE, _("CD-ROM status command failed"));
	}
#else
	struct timeval time_start, time_stop;
	int time_elapsed;

	/* Try to open the CDROM tray and measure the time therefore
	 * needed.  In my experience the function needs less than 0.05
	 * seconds if the tray was already open, and at least 1.5 seconds
	 * if it was closed.  */
	gettime_monotonic(&time_start);

	/* Send the CDROMEJECT command to the device. */
	if (!eject_cdrom(fd))
		err(EXIT_FAILURE, _("CD-ROM eject command failed"));

	/* Get the second timestamp, to measure the time needed to open
	 * the tray.  */
	gettime_monotonic(&time_stop);

	time_elapsed = (time_stop.tv_sec * 1000000 + time_stop.tv_usec) -
		(time_start.tv_sec * 1000000 + time_start.tv_usec);

	/* If the tray "opened" too fast, we can be nearly sure, that it
	 * was already open. In this case, close it now. Else the tray was
	 * closed before. This would mean that we are done.  */
	if (time_elapsed < TRAY_WAS_ALREADY_OPEN_USECS)
		close_tray(fd);
#endif
}

/*
 * Select Speed of CD-ROM drive.
 * Thanks to Roland Krivanek (krivanek@fmph.uniba.sk)
 * http://dmpc.dbp.fmph.uniba.sk/~krivanek/cdrom_speed/
 */
static void select_speed(const struct eject_control *ctl)
{
#ifdef CDROM_SELECT_SPEED
	if (ioctl(ctl->fd, CDROM_SELECT_SPEED, ctl->x_arg) != 0)
		err(EXIT_FAILURE, _("CD-ROM select speed command failed"));
#else
	warnx(_("CD-ROM select speed command not supported by this kernel"));
#endif
}

/*
 * Read Speed of CD-ROM drive. From Linux 2.6.13, the current speed
 * is correctly reported
 */
static int read_speed(const char *devname)
{
	int drive_number = -1;
	char *name;
	FILE *f;

	f = fopen(_PATH_PROC_CDROMINFO, "r");
	if (!f)
		err(EXIT_FAILURE, _("cannot open %s"), _PATH_PROC_CDROMINFO);

	name = strrchr(devname, '/') + 1;

	while (name && !feof(f)) {
		char line[512];
		char *str;

		if (!fgets(line, sizeof(line), f))
			break;

		/* find drive number in line "drive name" */
		if (drive_number == -1) {
			if (strncmp(line, "drive name:", 11) == 0) {
				str = strtok(&line[11], "\t ");
				drive_number = 0;
				while (str && strncmp(name, str, strlen(name)) != 0) {
					drive_number++;
					str = strtok(NULL, "\t ");
					if (!str)
						errx(EXIT_FAILURE,
						     _("%s: failed to finding CD-ROM name"),
						     _PATH_PROC_CDROMINFO);
				}
			}
		/* find line "drive speed" and read the correct speed */
		} else {
			if (strncmp(line, "drive speed:", 12) == 0) {
				int n;

				fclose(f);

				str = line + 12;
				normalize_whitespace((unsigned char *) str);

				if (ul_strtos32(str, &n, 10) == 0)
					return n;

				errx(EXIT_FAILURE, _("%s: failed to read speed"),
						_PATH_PROC_CDROMINFO);
			}
		}
	}

	errx(EXIT_FAILURE, _("failed to read speed"));
}

/*
 * List Speed of CD-ROM drive.
 */
static void list_speeds(struct eject_control *ctl)
{
	int max_speed, curr_speed = 0;

	select_speed(ctl);
	max_speed = read_speed(ctl->device);

	while (curr_speed < max_speed) {
		ctl->x_arg = curr_speed + 1;
		select_speed(ctl);
		curr_speed = read_speed(ctl->device);
		if (ctl->x_arg < curr_speed)
			printf("%d ", curr_speed);
		else
			curr_speed = ctl->x_arg + 1;
	}

	printf("\n");
}

/*
 * Eject using SCSI SG_IO commands. Return 1 if successful, 0 otherwise.
 */
static int eject_scsi(const struct eject_control *ctl)
{
	int status, k;
	sg_io_hdr_t io_hdr;
	unsigned char allowRmBlk[6] = {ALLOW_MEDIUM_REMOVAL, 0, 0, 0, 0, 0};
	unsigned char startStop1Blk[6] = {START_STOP, 0, 0, 0, 1, 0};
	unsigned char startStop2Blk[6] = {START_STOP, 0, 0, 0, 2, 0};
	unsigned char inqBuff[2];
	unsigned char sense_buffer[32];

	if ((ioctl(ctl->fd, SG_GET_VERSION_NUM, &k) < 0) || (k < 30000)) {
		verbose(ctl, _("not an sg device, or old sg driver"));
		return 0;
	}

	memset(&io_hdr, 0, sizeof(sg_io_hdr_t));
	io_hdr.interface_id = 'S';
	io_hdr.cmd_len = 6;
	io_hdr.mx_sb_len = sizeof(sense_buffer);
	io_hdr.dxfer_direction = SG_DXFER_NONE;
	io_hdr.dxfer_len = 0;
	io_hdr.dxferp = inqBuff;
	io_hdr.sbp = sense_buffer;
	io_hdr.timeout = 10000;

	io_hdr.cmdp = allowRmBlk;
	status = ioctl(ctl->fd, SG_IO, (void *)&io_hdr);
	if (status < 0 || io_hdr.host_status || io_hdr.driver_status)
		return 0;

	io_hdr.cmdp = startStop1Blk;
	status = ioctl(ctl->fd, SG_IO, (void *)&io_hdr);
	if (status < 0 || io_hdr.host_status)
		return 0;

	/* Ignore errors when there is not medium -- in this case driver sense
	 * buffer sets MEDIUM NOT PRESENT (3a) bit. For more details see:
	 * http://www.tldp.org/HOWTO/archived/SCSI-Programming-HOWTO/SCSI-Programming-HOWTO-22.html#sec-sensecodes
	 * -- kzak Jun 2013
	 */
	if (io_hdr.driver_status != 0 &&
	    !(io_hdr.driver_status == DRIVER_SENSE && io_hdr.sbp &&
		                                      io_hdr.sbp[12] == 0x3a))
		return 0;

	io_hdr.cmdp = startStop2Blk;
	status = ioctl(ctl->fd, SG_IO, (void *)&io_hdr);
	if (status < 0 || io_hdr.host_status || io_hdr.driver_status)
		return 0;

	/* force kernel to reread partition table when new disc inserted */
	ioctl(ctl->fd, BLKRRPART);
	return 1;
}

/*
 * Eject using FDEJECT ioctl. Return 1 if successful, 0 otherwise.
 */
static int eject_floppy(int fd)
{
	return ioctl(fd, FDEJECT) >= 0;
}


/*
 * Rewind and eject using tape ioctl. Return 1 if successful, 0 otherwise.
 */
static int eject_tape(int fd)
{
	struct mtop op = { .mt_op = MTOFFL, .mt_count = 0 };

	return ioctl(fd, MTIOCTOP, &op) >= 0;
}


/* umount a device. */
static void umount_one(const struct eject_control *ctl, const char *name)
{
	int status;

	if (!name)
		return;

	verbose(ctl, _("%s: unmounting"), name);

	switch (fork()) {
	case 0: /* child */
		if (drop_permissions() != 0)
			err(EXIT_FAILURE, _("drop permissions failed"));
		if (ctl->p_option)
			execl("/bin/umount", "/bin/umount", name, "-n", (char *)NULL);
		else
			execl("/bin/umount", "/bin/umount", name, (char *)NULL);

		errexec("/bin/umount");

	case -1:
		warn( _("unable to fork"));
		break;

	default: /* parent */
		wait(&status);
		if (WIFEXITED(status) == 0)
			errx(EXIT_FAILURE,
			     _("unmount of `%s' did not exit normally"), name);

		if (WEXITSTATUS(status) != 0)
			errx(EXIT_FAILURE, _("unmount of `%s' failed\n"), name);
		break;
	}
}

/* Open a device file. */
static void open_device(struct eject_control *ctl)
{
	int extra = ctl->F_option == 0 &&		/* never use O_EXCL on --force */
		    ctl->force_exclusive ? O_EXCL : 0;

	ctl->fd = open(ctl->device, O_RDWR | O_NONBLOCK | extra);
	if (ctl->fd < 0)
		ctl->fd = open(ctl->device, O_RDONLY | O_NONBLOCK | extra);
	if (ctl->fd == -1)
		err(EXIT_FAILURE, _("cannot open %s"), ctl->device);
}

/*
 * See if device has been mounted by looking in mount table.  If so, set
 * device name and mount point name, and return 1, otherwise return 0.
 */
static int device_get_mountpoint(struct eject_control *ctl, char **devname, char **mnt)
{
	struct libmnt_fs *fs;
	int rc;

	*mnt = NULL;

	if (!ctl->mtab) {
		struct libmnt_cache *cache;

		ctl->mtab = mnt_new_table();
		if (!ctl->mtab)
			err(EXIT_FAILURE, _("failed to initialize libmount table"));

		cache = mnt_new_cache();
		mnt_table_set_cache(ctl->mtab, cache);
		mnt_unref_cache(cache);

		if (ctl->p_option)
			rc = mnt_table_parse_file(ctl->mtab, _PATH_PROC_MOUNTINFO);
		else
			rc = mnt_table_parse_mtab(ctl->mtab, NULL);
		if (rc)
			err(EXIT_FAILURE, _("failed to parse mount table"));
	}

	fs = mnt_table_find_source(ctl->mtab, *devname, MNT_ITER_BACKWARD);
	if (!fs) {
		/* maybe 'devname' is mountpoint rather than a real device */
		fs = mnt_table_find_target(ctl->mtab, *devname, MNT_ITER_BACKWARD);
		if (fs) {
			free(*devname);
			*devname = xstrdup(mnt_fs_get_source(fs));
		}
	}

	if (fs)
		*mnt = xstrdup(mnt_fs_get_target(fs));
	return *mnt ? 0 : -1;
}

static char *get_disk_devname(const char *device)
{
	struct stat st;
	dev_t diskno = 0;
	char diskname[128];

	if (stat(device, &st) != 0)
		return NULL;

	/* get whole-disk devno */
	if (sysfs_devno_to_wholedisk(st.st_rdev, diskname,
				sizeof(diskname), &diskno) != 0)
		return NULL;

	return st.st_rdev == diskno ? NULL : find_device(diskname);
}

/* umount all partitions if -M not specified, otherwise returns
 * number of the mounted partitions only.
 */
static int umount_partitions(struct eject_control *ctl)
{
	struct path_cxt *pc = NULL;
	dev_t devno;
	DIR *dir = NULL;
	struct dirent *d;
	int count = 0;

	devno = sysfs_devname_to_devno(ctl->device);
	if (devno)
		pc = ul_new_sysfs_path(devno, NULL, NULL);
	if (!pc)
		return 0;

	/* open /sys/block/<wholedisk> */
	if (!(dir = ul_path_opendir(pc, NULL)))
		goto done;

	/* scan for partition subdirs */
	while ((d = readdir(dir))) {
		if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
			continue;

		if (sysfs_blkdev_is_partition_dirent(dir, d, ctl->device)) {
			char *mnt = NULL;
			char *dev = find_device(d->d_name);

			if (dev && device_get_mountpoint(ctl, &dev, &mnt) == 0) {
				verbose(ctl, _("%s: mounted on %s"), dev, mnt);
				if (!ctl->M_option)
					umount_one(ctl, mnt);
				count++;
			}
			free(dev);
			free(mnt);
		}
	}

done:
	if (dir)
		closedir(dir);
	ul_unref_path(pc);

	return count;
}

static int is_ejectable(const struct eject_control *ctl)
{
	struct path_cxt *pc = NULL;
	dev_t devno;
	int rc = 0;

	devno = sysfs_devname_to_devno(ctl->device);
	if (devno)
		pc = ul_new_sysfs_path(devno, NULL, NULL);
	if (!pc)
		return 0;

	rc = sysfs_blkdev_is_hotpluggable(pc) || sysfs_blkdev_is_removable(pc);
	ul_unref_path(pc);
	return rc;
}


/* handle -x option */
static void set_device_speed(struct eject_control *ctl)
{
	if (!ctl->x_option)
		return;

	if (ctl->x_arg == 0)
		verbose(ctl, _("setting CD-ROM speed to auto"));
	else
		verbose(ctl, _("setting CD-ROM speed to %ldX"), ctl->x_arg);

	open_device(ctl);
	select_speed(ctl);
	exit(EXIT_SUCCESS);
}


/* main program */
int main(int argc, char **argv)
{
	char *disk = NULL;
	char *mountpoint = NULL;
	int worked = 0;    /* set to 1 when successfully ejected */
	struct eject_control ctl = { .fd = -1 };

	setlocale(LC_ALL,"");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	/* parse the command line arguments */
	parse_args(&ctl, argc, argv);

	/* handle -d option */
	if (ctl.d_option) {
		info(_("default device: `%s'"), EJECT_DEFAULT_DEVICE);
		return EXIT_SUCCESS;
	}

	if (!ctl.device) {
		ctl.device = mnt_resolve_path(EJECT_DEFAULT_DEVICE, NULL);
		verbose(&ctl, _("using default device `%s'"), ctl.device);
	} else {
		char *p;

		if (ctl.device[strlen(ctl.device) - 1] == '/')
			ctl.device[strlen(ctl.device) - 1] = '\0';

		/* figure out full device or mount point name */
		p = find_device(ctl.device);
		if (p)
			free(ctl.device);
		else
			p = ctl.device;

		ctl.device = mnt_resolve_spec(p, NULL);
		free(p);
	}

	if (!ctl.device)
		errx(EXIT_FAILURE, _("unable to find device"));

	verbose(&ctl, _("device name is `%s'"), ctl.device);

	device_get_mountpoint(&ctl, &ctl.device, &mountpoint);
	if (mountpoint)
		verbose(&ctl, _("%s: mounted on %s"), ctl.device, mountpoint);
	else
		verbose(&ctl, _("%s: not mounted"), ctl.device);

	disk = get_disk_devname(ctl.device);
	if (disk) {
		verbose(&ctl, _("%s: disc device: %s (disk device will be used for eject)"), ctl.device, disk);
		free(ctl.device);
		ctl.device = disk;
		disk = NULL;
	} else {
		struct stat st;

		if (stat(ctl.device, &st) != 0 || !S_ISBLK(st.st_mode))
			errx(EXIT_FAILURE, _("%s: not found mountpoint or device "
					"with the given name"), ctl.device);

		verbose(&ctl, _("%s: is whole-disk device"), ctl.device);
	}

	if (ctl.F_option == 0 && is_ejectable(&ctl) == 0)
		errx(EXIT_FAILURE, _("%s: is not ejectable device"), ctl.device);

	/* handle -n option */
	if (ctl.n_option) {
		info(_("device is `%s'"), ctl.device);
		verbose(&ctl, _("exiting due to -n/--noop option"));
		goto done;
	}

	/* handle -i option */
	if (ctl.i_option) {
		open_device(&ctl);
		manual_eject(&ctl);
		goto done;
	}

	/* handle -a option */
	if (ctl.a_option) {
		if (ctl.a_arg)
			verbose(&ctl, _("%s: enabling auto-eject mode"), ctl.device);
		else
			verbose(&ctl, _("%s: disabling auto-eject mode"), ctl.device);
		open_device(&ctl);
		auto_eject(&ctl);
		goto done;
	}

	/* handle -t option */
	if (ctl.t_option) {
		verbose(&ctl, _("%s: closing tray"), ctl.device);
		open_device(&ctl);
		close_tray(ctl.fd);
		set_device_speed(&ctl);
		goto done;
	}

	/* handle -T option */
	if (ctl.T_option) {
		verbose(&ctl, _("%s: toggling tray"), ctl.device);
		open_device(&ctl);
		toggle_tray(ctl.fd);
		set_device_speed(&ctl);
		goto done;
	}

	/* handle -X option */
	if (ctl.X_option) {
		verbose(&ctl, _("%s: listing CD-ROM speed"), ctl.device);
		open_device(&ctl);
		list_speeds(&ctl);
		goto done;
	}

	/* handle -x option only */
	if (!ctl.c_option)
		set_device_speed(&ctl);


	/*
	 * Unmount all partitions if -m is not specified; or umount given
	 * mountpoint if -M is specified, otherwise print error of another
	 * partition is mounted.
	 */
	if (!ctl.m_option) {
		int ct = umount_partitions(&ctl); /* umount all, or count mounted on -M */

		if (ct == 0 && mountpoint)
			umount_one(&ctl, mountpoint); /* probably whole-device */

		if (ctl.M_option) {
			if (ct == 1 && mountpoint)
				umount_one(&ctl, mountpoint);
			else if (ct)
				errx(EXIT_FAILURE, _("error: %s: device in use"), ctl.device);
		}
		/* Now, we assume the device is no more used, use O_EXCL to be
		 * resistant against our bugs and possible races (someone else
		 * remounted the device).
		 */
		ctl.force_exclusive = 1;
	}

	/* handle -c option */
	if (ctl.c_option) {
		verbose(&ctl, _("%s: selecting CD-ROM disc #%ld"), ctl.device, ctl.c_arg);
		open_device(&ctl);
		changer_select(&ctl);
		set_device_speed(&ctl);
		goto done;
	}

	/* if user did not specify type of eject, try all four methods */
	if (ctl.r_option + ctl.s_option + ctl.f_option + ctl.q_option == 0)
		ctl.r_option = ctl.s_option = ctl.f_option = ctl.q_option = 1;

	/* open device */
	open_device(&ctl);

	/* try various methods of ejecting until it works */
	if (ctl.r_option) {
		verbose(&ctl, _("%s: trying to eject using CD-ROM eject command"), ctl.device);
		worked = eject_cdrom(ctl.fd);
		verbose(&ctl, worked ? _("CD-ROM eject command succeeded") :
				 _("CD-ROM eject command failed"));
	}

	if (ctl.s_option && !worked) {
		verbose(&ctl, _("%s: trying to eject using SCSI commands"), ctl.device);
		worked = eject_scsi(&ctl);
		verbose(&ctl, worked ? _("SCSI eject succeeded") :
				 _("SCSI eject failed"));
	}

	if (ctl.f_option && !worked) {
		verbose(&ctl, _("%s: trying to eject using floppy eject command"), ctl.device);
		worked = eject_floppy(ctl.fd);
		verbose(&ctl, worked ? _("floppy eject command succeeded") :
				 _("floppy eject command failed"));
	}

	if (ctl.q_option && !worked) {
		verbose(&ctl, _("%s: trying to eject using tape offline command"), ctl.device);
		worked = eject_tape(ctl.fd);
		verbose(&ctl, worked ? _("tape offline command succeeded") :
				 _("tape offline command failed"));
	}

	if (!worked)
		errx(EXIT_FAILURE, _("unable to eject"));

done:
	/* cleanup */
	if (ctl.fd >= 0)
		close(ctl.fd);

	free(ctl.device);
	free(mountpoint);

	mnt_unref_table(ctl.mtab);

	return EXIT_SUCCESS;
}
