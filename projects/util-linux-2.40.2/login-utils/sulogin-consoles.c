/*
 * consoles.c	    Routines to detect the system consoles
 *
 * Copyright (c) 2011 SuSE LINUX Products GmbH, All rights reserved.
 * Copyright (C) 2012 Karel Zak <kzak@redhat.com>
 * Copyright (C) 2012 Werner Fink <werner@suse.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see the file COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 * Author: Werner Fink <werner@suse.de>
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#ifdef __linux__
# include <sys/vt.h>
# include <sys/kd.h>
# include <linux/serial.h>
# include <linux/major.h>
#endif
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#if defined(USE_SULOGIN_EMERGENCY_MOUNT)
# include <sys/mount.h>
# ifndef MS_RELATIME
#  define MS_RELATIME  (1<<21)
# endif
# ifndef MNT_DETACH
#  define MNT_DETACH   2
# endif
#endif

#include "c.h"
#include "canonicalize.h"
#include "sulogin-consoles.h"

#if !defined(__STDC_VERSION__) || (__STDC_VERSION__ < 199901L)
# ifndef  typeof
#  define typeof		__typeof__
# endif
# ifndef  restrict
#  define restrict		__restrict__
# endif
#endif

#define alignof(type)		((sizeof(type)+(sizeof(void*)-1)) & ~(sizeof(void*)-1))
#define strsize(string)		(strlen((string))+1)

static int consoles_debug;
#define DBG(x)	do { \
			if (consoles_debug) { \
				fputs("consoles debug: ", stderr); \
				x;			     \
			} \
		} while (0)

static inline void __attribute__ ((__format__ (__printf__, 1, 2)))
dbgprint(const char * const mesg, ...)
{
	va_list ap;
	va_start(ap, mesg);
	vfprintf(stderr, mesg, ap);
	va_end(ap);
	fputc('\n', stderr);
}

#ifdef USE_SULOGIN_EMERGENCY_MOUNT
/*
 * Make C library standard calls such like ttyname(3) work
 * even if the system does not show any of the standard
 * directories.
 */

static uint32_t emergency_flags;
# define MNT_PROCFS    0x0001
# define MNT_DEVTMPFS  0x0002

void emergency_do_umounts(void)
{
	if (emergency_flags & MNT_DEVTMPFS)
		umount2("/dev", MNT_DETACH);
	if (emergency_flags & MNT_PROCFS)
		umount2("/proc", MNT_DETACH);
}

void emergency_do_mounts(void)
{
	struct stat rt, xt;

	if (emergency_flags) {
		emergency_flags = 0;
		return;
	}

	if (stat("/", &rt) != 0) {
		warn("cannot get file status of root file system\n");
		return;
	}

	if (stat("/proc", &xt) == 0
	    && rt.st_dev == xt.st_dev
	    && mount("proc", "/proc", "proc", MS_RELATIME, NULL) == 0)
		emergency_flags |= MNT_PROCFS;

	if (stat("/dev", &xt) == 0
	    && rt.st_dev == xt.st_dev
	    && mount("devtmpfs", "/dev", "devtmpfs",
		     MS_RELATIME, "mode=0755,nr_inodes=0") == 0) {

		emergency_flags |= MNT_DEVTMPFS;
		mknod("/dev/console", S_IFCHR|S_IRUSR|S_IWUSR,
					makedev(TTYAUX_MAJOR, 1));

		if (symlink("/proc/self/fd", "/dev/fd") == 0) {
			ignore_result( symlink("fd/0", "/dev/stdin") );
			ignore_result( symlink("fd/1", "/dev/stdout") );
			ignore_result( symlink("fd/2", "/dev/stderr") );
		}
	}
}

#else /* !USE_SULOGIN_EMERGENCY_MOUNT */

void emergency_do_umounts(void) { }
void emergency_do_mounts(void) { }

#endif /* USE_SULOGIN_EMERGENCY_MOUNT */

/*
 * Read and allocate one line from file,
 * the caller has to free the result
 */
static __attribute__((__nonnull__))
char *oneline(const char * const file)
{
	FILE *fp;
	char *ret = NULL;
	size_t dummy = 0;
	ssize_t len;

	DBG(dbgprint("reading %s", file));

	if (!(fp = fopen(file, "r" UL_CLOEXECSTR)))
		return NULL;
	len = getline(&ret, &dummy, fp);
	if (len >= 0) {
		char *nl;

		if (len)
			ret[len-1] = '\0';
		if ((nl = strchr(ret, '\n')))
			*nl = '\0';
	}

	fclose(fp);
	return ret;
}

#ifdef __linux__
/*
 *  Read and determine active attribute for tty below
 *  /sys/class/tty, the caller has to free the result.
 */
static __attribute__((__malloc__))
char *actattr(const char * const tty)
{
	char *ret, *path;

	if (!tty || !*tty)
		return NULL;
	if (asprintf(&path, "/sys/class/tty/%s/active", tty) < 0)
		return NULL;

	ret = oneline(path);
	free(path);
	return ret;
}

/*
 * Read and determine device attribute for tty below
 * /sys/class/tty.
 */
static
dev_t devattr(const char * const tty)
{
	dev_t dev = 0;
	char *path, *value;

	if (!tty || !*tty)
		return 0;
	if (asprintf(&path, "/sys/class/tty/%s/dev", tty) < 0)
		return 0;

	value = oneline(path);
	if (value) {
		unsigned int maj, min;

		if (sscanf(value, "%u:%u", &maj, &min) == 2)
			dev = makedev(maj, min);
		free(value);
	}

	free(path);
	return dev;
}
#endif /* __linux__ */

/*
 * Search below /dev for the character device in `dev_t comparedev' variable.
 * Note that realpath(3) is used here to avoid not existent devices due the
 * strdup(3) used in our canonicalize_path()!
 */
static
#ifdef __GNUC__
__attribute__((__nonnull__,__malloc__,__hot__))
#endif
char* scandev(DIR *dir, const dev_t comparedev)
{
	char path[PATH_MAX];
	char *name = NULL;
	const struct dirent *dent;
	int len, fd;

	DBG(dbgprint("scanning /dev for %u:%u", major(comparedev), minor(comparedev)));

	/*
	 * Try udev links on character devices first.
	 */
	if ((len = snprintf(path, sizeof(path),
			    "/dev/char/%u:%u", major(comparedev), minor(comparedev))) > 0 &&
	    (size_t)len < sizeof(path)) {

	    name = realpath(path, NULL);
	    if (name)
		    goto out;
	}

	fd = dirfd(dir);
	rewinddir(dir);
	while ((dent = readdir(dir))) {
		struct stat st;

#ifdef _DIRENT_HAVE_D_TYPE
		if (dent->d_type != DT_UNKNOWN && dent->d_type != DT_CHR)
			continue;
#endif
		if (fstatat(fd, dent->d_name, &st, 0) < 0)
			continue;
		if (!S_ISCHR(st.st_mode))
			continue;
		if (comparedev != st.st_rdev)
			continue;
		if ((len = snprintf(path, sizeof(path), "/dev/%s", dent->d_name)) < 0 ||
		    (size_t)len >= sizeof(path))
			continue;

		name = realpath(path, NULL);
		if (name)
			goto out;
	}

#ifdef USE_SULOGIN_EMERGENCY_MOUNT
	/*
	 * There was no /dev mounted hence and no device was found hence we create our own.
	 */
	if (!name && (emergency_flags & MNT_DEVTMPFS)) {

		if ((len = snprintf(path, sizeof(path),
				    "/dev/tmp-%u:%u", major(comparedev), minor(comparedev))) < 0 ||
		    (size_t)len >= sizeof(path))
			goto out;

		if (mknod(path, S_IFCHR|S_IRUSR|S_IWUSR, comparedev) < 0 && errno != EEXIST)
			goto out;

		name = realpath(path, NULL);
	}
#endif
out:
	return name;
}

/*
 * Default control characters for an unknown terminal line.
 */

/*
 * Allocate an aligned `struct console' memory area,
 * initialize its default values, and append it to
 * the global linked list.
 */
static
#ifdef __GNUC__
__attribute__((__hot__))
#endif
int append_console(struct list_head *consoles, const char * const name)
{
	struct console *restrict tail;
	const struct console *last = NULL;

	DBG(dbgprint("appending %s", name));

	if (!list_empty(consoles))
		last = list_last_entry(consoles, struct console, entry);

	if (posix_memalign((void *) &tail, sizeof(void *),
			   alignof(struct console) + strsize(name)) != 0)
		return -ENOMEM;

	INIT_LIST_HEAD(&tail->entry);
	INIT_CHARDATA(&tail->cp);

	list_add_tail(&tail->entry, consoles);
	tail->tty = ((char *) tail) + alignof(struct console);
	strcpy(tail->tty, name);

	tail->file = (FILE*)0;
	tail->flags = 0;
	tail->fd = -1;
	tail->id = last ? last->id + 1 : 0;
	tail->pid = -1;
	memset(&tail->tio, 0, sizeof(tail->tio));
#ifdef HAVE_LIBSELINUX
	tail->reset_tty_context = NULL;
	tail->user_tty_context = NULL;
#endif

	return 0;
}

#ifdef __linux__
/*
 * return codes:
 *	< 0	- fatal error (no mem or so... )
 *	  0	- success
 *	  1	- recoverable error
 *	  2	- detection not available
 */
static int detect_consoles_from_proc(struct list_head *consoles)
{
	char fbuf[16 + 1];
	DIR *dir = NULL;
	FILE *fc = NULL;
	int maj, min, rc = 1, matches;

	DBG(dbgprint("trying /proc"));

	fc = fopen("/proc/consoles", "r" UL_CLOEXECSTR);
	if (!fc) {
		rc = 2;
		goto done;
	}
	dir = opendir("/dev");
	if (!dir)
		goto done;

	while ((matches = fscanf(fc, "%*s %*s (%16[^)]) %d:%d", fbuf, &maj, &min)) >= 1) {
		char *name;
		dev_t comparedev;

		if (matches != 3)
			continue;
		if (!strchr(fbuf, 'E'))
			continue;
		comparedev = makedev(maj, min);
		name = scandev(dir, comparedev);
		if (!name)
			continue;
		rc = append_console(consoles, name);
		free(name);
		if (rc < 0)
			goto done;
	}

	rc = list_empty(consoles) ? 1 : 0;
done:
	if (dir)
		closedir(dir);
	if (fc)
		fclose(fc);
	DBG(dbgprint("[/proc rc=%d]", rc));
	return rc;
}

/*
 * return codes:
 *	< 0	- fatal error (no mem or so... )
 *	  0	- success
 *	  1	- recoverable error
 *	  2	- detection not available
 */
static int detect_consoles_from_sysfs(struct list_head *consoles)
{
	char *attrib = NULL, *words, *token;
	DIR *dir = NULL;
	int rc = 1;

	DBG(dbgprint("trying /sys"));

	attrib = actattr("console");
	if (!attrib) {
		rc = 2;
		goto done;
	}

	words = attrib;

	dir = opendir("/dev");
	if (!dir)
		goto done;

	while ((token = strsep(&words, " \t\r\n"))) {
		char *name;
		dev_t comparedev;

		if (*token == '\0')
			continue;

		comparedev = devattr(token);
		if (comparedev == makedev(TTY_MAJOR, 0)) {
			char *tmp = actattr(token);
			if (!tmp)
				continue;
			comparedev = devattr(tmp);
			free(tmp);
		}

		name = scandev(dir, comparedev);
		if (!name)
			continue;
		rc = append_console(consoles, name);
		free(name);
		if (rc < 0)
			goto done;
	}

	rc = list_empty(consoles) ? 1 : 0;
done:
	free(attrib);
	if (dir)
		closedir(dir);
	DBG(dbgprint("[/sys rc=%d]", rc));
	return rc;
}


static int detect_consoles_from_cmdline(struct list_head *consoles)
{
	char *cmdline, *words, *token;
	dev_t comparedev;
	DIR *dir = NULL;
	int rc = 1, fd;

	DBG(dbgprint("trying kernel cmdline"));

	cmdline = oneline("/proc/cmdline");
	if (!cmdline) {
		rc = 2;
		goto done;
	}

	words = cmdline;
	dir = opendir("/dev");
	if (!dir)
		goto done;

	while ((token = strsep(&words, " \t\r\n"))) {
#ifdef TIOCGDEV
		unsigned int devnum;
#else
		struct vt_stat vt;
		struct stat st;
#endif
		char *colon, *name;

		if (*token != 'c')
			continue;
		if (strncmp(token, "console=", 8) != 0)
			continue;
		token += 8;

		if (strcmp(token, "brl") == 0)
			token += 4;
		if ((colon = strchr(token, ',')))
			*colon = '\0';

		if (asprintf(&name, "/dev/%s", token) < 0)
			continue;
		if ((fd = open(name, O_RDWR|O_NONBLOCK|O_NOCTTY|O_CLOEXEC)) < 0) {
			free(name);
			continue;
		}
		free(name);
#ifdef TIOCGDEV
		if (ioctl (fd, TIOCGDEV, &devnum) < 0) {
			close(fd);
			continue;
		}
		comparedev = (dev_t) devnum;
#else
		if (fstat(fd, &st) < 0) {
			close(fd);
			continue;
		}
		comparedev = st.st_rdev;
		if (comparedev == makedev(TTY_MAJOR, 0)) {
			if (ioctl(fd, VT_GETSTATE, &vt) < 0) {
				close(fd);
				continue;
			}
			comparedev = makedev(TTY_MAJOR, (int)vt.v_active);
		}
#endif
		close(fd);

		name = scandev(dir, comparedev);
		if (!name)
			continue;
		rc = append_console(consoles, name);
		free(name);
		if (rc < 0)
			goto done;
	}

	rc = list_empty(consoles) ? 1 : 0;
done:
	if (dir)
		closedir(dir);
	free(cmdline);
	DBG(dbgprint("[kernel cmdline rc=%d]", rc));
	return rc;
}

#ifdef TIOCGDEV
static int detect_consoles_from_tiocgdev(struct list_head *consoles,
					const int fallback,
					const char *device)
{
	unsigned int devnum;
	char *name;
	int rc = 1, fd = -1;
	dev_t comparedev;
	DIR *dir = NULL;
	struct console *console;

	DBG(dbgprint("trying tiocgdev"));

	if (!device || !*device)
		fd = dup(fallback);
	else
		fd = open(device, O_RDWR|O_NONBLOCK|O_NOCTTY|O_CLOEXEC);

	if (fd < 0)
		goto done;
	if (ioctl (fd, TIOCGDEV, &devnum) < 0)
		goto done;

	comparedev = (dev_t) devnum;
	dir = opendir("/dev");
	if (!dir)
		goto done;

	name = scandev(dir, comparedev);
	closedir(dir);

	if (!name) {
		name = (char *) (device && *device ? device : ttyname(fallback));
		if (!name)
			name = "/dev/tty1";

		name = strdup(name);
		if (!name) {
			rc = -ENOMEM;
			goto done;
		}
	}
	rc = append_console(consoles, name);
	free(name);
	if (rc < 0)
		goto done;
	if (list_empty(consoles)) {
		rc = 1;
		goto done;
	}
	console = list_last_entry(consoles, struct console, entry);
	if (console &&  (!device || !*device))
		console->fd = fallback;
done:
	if (fd >= 0)
		close(fd);
	DBG(dbgprint("[tiocgdev rc=%d]", rc));
	return rc;
}
#endif /* TIOCGDEV */
#endif /* __linux__ */

/*
 * Try to detect the real device(s) used for the system console
 * /dev/console if but only if /dev/console is used.  On Linux
 * this can be more than one device, e.g. a serial line as well
 * as a virtual console as well as a simple printer.
 *
 * Returns 1 if stdout and stderr should be reconnected and 0
 * otherwise or less than zero on error.
 */
int detect_consoles(const char *device, const int fallback, struct list_head *consoles)
{
	int fd, reconnect = 0, rc;
	dev_t comparedev = 0;

	consoles_debug = getenv("CONSOLES_DEBUG") ? 1 : 0;

	if (!device || !*device)
		fd = fallback >= 0 ? dup(fallback) : - 1;
	else {
		fd = open(device, O_RDWR|O_NONBLOCK|O_NOCTTY|O_CLOEXEC);
		reconnect = 1;
	}

	DBG(dbgprint("detection started [device=%s, fallback=%d]",
				device, fallback));

	if (fd >= 0) {
		DIR *dir;
		char *name;
		struct stat st;
#ifdef TIOCGDEV
		unsigned int devnum;
#endif
#ifdef __GNU__
		/*
		 * The Hurd always gives st_rdev as 0, which causes this
		 * method to select the first terminal it finds.
		 */
		close(fd);
		goto fallback;
#endif
		DBG(dbgprint("trying device/fallback file descriptor"));

		if (fstat(fd, &st) < 0) {
			close(fd);
			goto fallback;
		}
		comparedev = st.st_rdev;

		if (reconnect &&
		    (fstat(fallback, &st) < 0 || comparedev != st.st_rdev))
			dup2(fd, fallback);
#ifdef __linux__
		/*
		 * Check if the device detection for Linux system console should be used.
		 */
		if (comparedev == makedev(TTYAUX_MAJOR, 0)) {	/* /dev/tty	*/
			close(fd);
			device = "/dev/tty";
			goto fallback;
		}
		if (comparedev == makedev(TTYAUX_MAJOR, 1)) {	/* /dev/console */
			close(fd);
			goto console;
		}
		if (comparedev == makedev(TTYAUX_MAJOR, 2)) {	/* /dev/ptmx	*/
			close(fd);
			device = "/dev/tty";
			goto fallback;
		}
		if (comparedev == makedev(TTY_MAJOR, 0)) {	/* /dev/tty0	*/
			struct vt_stat vt;
			if (ioctl(fd, VT_GETSTATE, &vt) < 0) {
				close(fd);
				goto fallback;
			}
			comparedev = makedev(TTY_MAJOR, (int)vt.v_active);
		}
#endif
#ifdef TIOCGDEV
		if (ioctl (fd, TIOCGDEV, &devnum) < 0) {
			close(fd);
			goto fallback;
		}
		comparedev = (dev_t)devnum;
#endif
		close(fd);
		dir = opendir("/dev");
		if (!dir)
			goto fallback;
		name = scandev(dir, comparedev);
		closedir(dir);

		if (name) {
			rc = append_console(consoles, name);
			free(name);
			if (rc < 0)
				return rc;
		}
		if (list_empty(consoles))
			goto fallback;

		DBG(dbgprint("detection success [rc=%d]", reconnect));
		return reconnect;
	}
#ifdef __linux__
console:
	/*
	 * Detection of devices used for Linux system console using
	 * the /proc/consoles API with kernel 2.6.38 and higher.
	 */
	rc = detect_consoles_from_proc(consoles);
	if (rc == 0)
		return reconnect;	/* success */
	if (rc < 0)
		return rc;		/* fatal error */

	/*
	 * Detection of devices used for Linux system console using
	 * the sysfs /sys/class/tty/ API with kernel 2.6.37 and higher.
	 */
	rc = detect_consoles_from_sysfs(consoles);
	if (rc == 0)
		return reconnect;	/* success */
	if (rc < 0)
		return rc;		/* fatal error */

	/*
	 * Detection of devices used for Linux system console using
	 * kernel parameter on the kernels command line.
	 */
	rc = detect_consoles_from_cmdline(consoles);
	if (rc == 0)
		return reconnect;	/* success */
	if (rc < 0)
		return rc;		/* fatal error */

	/*
	 * Detection of the device used for Linux system console using
	 * the ioctl TIOCGDEV if available (e.g. official 2.6.38).
	 */
#ifdef TIOCGDEV
	rc = detect_consoles_from_tiocgdev(consoles, fallback, device);
	if (rc == 0)
		return reconnect;	/* success */
	if (rc < 0)
		return rc;		/* fatal error */
#endif
	if (!list_empty(consoles)) {
		DBG(dbgprint("detection success [rc=%d]", reconnect));
		return reconnect;
	}

#endif /* __linux __ */

fallback:
	if (fallback >= 0) {
		const char *name;
	        char *n;
		struct console *console;

		if (device && *device != '\0')
			name = device;
		else	name = ttyname(fallback);

		if (!name)
			name = "/dev/tty";

		n = strdup(name);
		if (!n)
			return -ENOMEM;
		rc = append_console(consoles, n);
		free(n);
		if (rc < 0)
			return rc;
		if (list_empty(consoles))
			return 1;
		console = list_last_entry(consoles, struct console, entry);
		if (console)
			console->fd = fallback;
	}

	DBG(dbgprint("detection done by fallback [rc=%d]", reconnect));
	return reconnect;
}


#ifdef TEST_PROGRAM
int main(int argc, char *argv[])
{
	char *name = NULL;
	int fd, re;
	struct list_head *p, consoles;

	if (argc == 2) {
		name = argv[1];
		fd = open(name, O_RDWR);
	} else {
		name = ttyname(STDIN_FILENO);
		fd = STDIN_FILENO;
	}

	if (!name)
		errx(EXIT_FAILURE, "usage: %s [<tty>]\n", program_invocation_short_name);

	INIT_LIST_HEAD(&consoles);
	re = detect_consoles(name, fd, &consoles);

	list_for_each(p, &consoles) {
		struct console *c = list_entry(p, struct console, entry);
		printf("%s: id=%d %s\n", c->tty, c->id, re ? "(reconnect) " : "");
	}

	return 0;
}
#endif
