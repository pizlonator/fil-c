/*
 * lsfd-cdev.c - handle associations opening character devices
 *
 * Copyright (C) 2021 Red Hat, Inc. All rights reserved.
 * Written by Masatake YAMATO <yamato@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "lsfd.h"

static struct list_head miscdevs;
static struct list_head ttydrvs;

struct miscdev {
	struct list_head miscdevs;
	unsigned long minor;
	char *name;
};

struct ttydrv {
	struct list_head ttydrvs;
	unsigned long major;
	unsigned long minor_start, minor_end;
	char *name;
	unsigned int is_ptmx: 1;
	unsigned int is_pts: 1;
};

struct cdev {
	struct file file;
	const char *devdrv;
	const struct cdev_ops *cdev_ops;
	void *cdev_data;
};

struct cdev_ops {
	const struct cdev_ops *parent;
	bool (*probe)(struct cdev *);
	char * (*get_name)(struct cdev *);
	bool (*fill_column)(struct proc *,
			    struct cdev *,
			    struct libscols_line *,
			    int,
			    size_t,
			    char **);
	void (*init)(const struct cdev *);
	void (*free)(const struct cdev *);
	void (*attach_xinfo)(struct cdev *);
	int (*handle_fdinfo)(struct cdev *, const char *, const char *);
	const struct ipc_class * (*get_ipc_class)(struct cdev *);
};

static bool cdev_fill_column(struct proc *proc __attribute__((__unused__)),
			     struct file *file,
			     struct libscols_line *ln,
			     int column_id,
			     size_t column_index)
{
	struct cdev *cdev = (struct cdev *)file;
	const struct cdev_ops *ops = cdev->cdev_ops;
	char *str = NULL;

	switch(column_id) {
	case COL_NAME:
		if (cdev->cdev_ops->get_name) {
			str = cdev->cdev_ops->get_name(cdev);
			if (str)
				break;
		}
		return false;
	case COL_TYPE:
		if (scols_line_set_data(ln, column_index, "CHR"))
			err(EXIT_FAILURE, _("failed to add output data"));
		return true;
	case COL_DEVTYPE:
		if (scols_line_set_data(ln, column_index,
					"char"))
			err(EXIT_FAILURE, _("failed to add output data"));
		return true;
	case COL_CHRDRV:
		if (cdev->devdrv)
			str = xstrdup(cdev->devdrv);
		else
			xasprintf(&str, "%u",
				  major(file->stat.st_rdev));
		break;
	default:
		while (ops) {
			if (ops->fill_column
			    && ops->fill_column(proc, cdev, ln,
						column_id, column_index, &str))
				goto out;
			ops = ops->parent;
		}
		return false;
	}

 out:
	if (!str)
		err(EXIT_FAILURE, _("failed to add output data"));
	if (scols_line_refer_data(ln, column_index, str))
		err(EXIT_FAILURE, _("failed to add output data"));
	return true;
}

static struct miscdev *new_miscdev(unsigned long minor, const char *name)
{
	struct miscdev *miscdev = xcalloc(1, sizeof(*miscdev));

	INIT_LIST_HEAD(&miscdev->miscdevs);

	miscdev->minor = minor;
	miscdev->name = xstrdup(name);

	return miscdev;
}

static void free_miscdev(struct miscdev *miscdev)
{
	free(miscdev->name);
	free(miscdev);
}

static void read_misc(struct list_head *miscdevs_list, FILE *misc_fp)
{
	unsigned long minor;
	char line[256];
	char name[sizeof(line)];

	while (fgets(line, sizeof(line), misc_fp)) {
		struct miscdev *miscdev;

		if (sscanf(line, "%lu %s", &minor, name) != 2)
			continue;

		miscdev = new_miscdev(minor, name);
		list_add_tail(&miscdev->miscdevs, miscdevs_list);
	}
}

#define TTY_DRIVERS_LINE_LEN0 1023
#define TTY_DRIVERS_LINE_LEN  (TTY_DRIVERS_LINE_LEN0 + 1)
static struct ttydrv *new_ttydrv(unsigned int major,
				 unsigned int minor_start, unsigned int minor_end,
				 const char *name)
{
	struct ttydrv *ttydrv = xmalloc(sizeof(*ttydrv));

	INIT_LIST_HEAD(&ttydrv->ttydrvs);

	ttydrv->major = major;
	ttydrv->minor_start = minor_start;
	ttydrv->minor_end = minor_end;
	ttydrv->name = xstrdup(name);
	ttydrv->is_ptmx = 0;
	if (strcmp(name, "ptmx") == 0)
		ttydrv->is_ptmx = 1;
	ttydrv->is_pts = 0;
	if (strcmp(name, "pts") == 0)
		ttydrv->is_pts = 1;

	return ttydrv;
}

static void free_ttydrv(struct ttydrv *ttydrv)
{
	free(ttydrv->name);
	free(ttydrv);
}

static bool is_pty(const struct ttydrv *ttydrv)
{
	return ttydrv->is_ptmx || ttydrv->is_pts;
}

static struct ttydrv *read_ttydrv(const char *line)
{
	const char *p;
	char name[TTY_DRIVERS_LINE_LEN];
	unsigned long major;
	unsigned long minor_range[2];

	p = strchr(line, ' ');
	if (p == NULL)
		return NULL;

	p = strstr(p, "/dev/");
	if (p == NULL)
		return NULL;
	p += (sizeof("/dev/") - 1); /* Ignore the last null byte. */

	if (sscanf(p, "%" stringify_value(TTY_DRIVERS_LINE_LEN0) "[^ ]", name) != 1)
		return NULL;

	p += strlen(name);
	if (sscanf(p, " %lu %lu-%lu ", &major,
		   minor_range, minor_range + 1) != 3) {
		if (sscanf(p, " %lu %lu ", &major, minor_range) == 2)
			minor_range[1] = minor_range[0];
		else
			return NULL;
	}

	return new_ttydrv(major, minor_range[0], minor_range[1], name);
}

static void read_tty_drivers(struct list_head *ttydrvs_list, FILE *ttydrvs_fp)
{
	char line[TTY_DRIVERS_LINE_LEN];

	while (fgets(line, sizeof(line), ttydrvs_fp)) {
		struct ttydrv *ttydrv = read_ttydrv(line);
		if (ttydrv)
			list_add_tail(&ttydrv->ttydrvs, ttydrvs_list);
	}
}

static void cdev_class_initialize(void)
{
	FILE *misc_fp;
	FILE *ttydrvs_fp;

	INIT_LIST_HEAD(&miscdevs);
	INIT_LIST_HEAD(&ttydrvs);

	misc_fp = fopen("/proc/misc", "r");
	if (misc_fp) {
		read_misc(&miscdevs, misc_fp);
		fclose(misc_fp);
	}

	ttydrvs_fp = fopen("/proc/tty/drivers", "r");
	if (ttydrvs_fp) {
		read_tty_drivers(&ttydrvs, ttydrvs_fp);
		fclose(ttydrvs_fp);
	}
}

static void cdev_class_finalize(void)
{
	list_free(&miscdevs, struct miscdev, miscdevs, free_miscdev);
	list_free(&ttydrvs,  struct ttydrv,  ttydrvs,  free_ttydrv);
}

const char *get_miscdev(unsigned long minor)
{
	struct list_head *c;
	list_for_each(c, &miscdevs) {
		struct miscdev *miscdev = list_entry(c, struct miscdev, miscdevs);
		if (miscdev->minor == minor)
			return miscdev->name;
	}
	return NULL;
}

static const struct ttydrv *get_ttydrv(unsigned long major,
				       unsigned long minor)
{
	struct list_head *c;

	list_for_each(c, &ttydrvs) {
		struct ttydrv *ttydrv = list_entry(c, struct ttydrv, ttydrvs);
		if (ttydrv->major == major
		    && ttydrv->minor_start <= minor
		    && minor <= ttydrv->minor_end)
			return ttydrv;
	}

	return NULL;
}


/*
 * generic (fallback implementation)
 */
static bool cdev_generic_probe(struct cdev *cdev __attribute__((__unused__))) {
	return true;
}

static bool cdev_generic_fill_column(struct proc *proc  __attribute__((__unused__)),
				     struct cdev *cdev,
				  struct libscols_line *ln __attribute__((__unused__)),
				  int column_id,
				  size_t column_index __attribute__((__unused__)),
				  char **str)
{
	struct file *file = &cdev->file;

	switch(column_id) {
	case COL_SOURCE:
		if (cdev->devdrv) {
			xasprintf(str, "%s:%u", cdev->devdrv,
				  minor(file->stat.st_rdev));
			return true;
		}
		/* FALL THROUGH */
	case COL_MAJMIN:
		xasprintf(str, "%u:%u",
			  major(file->stat.st_rdev),
			  minor(file->stat.st_rdev));
		return true;
	default:
		return false;
	}
}

static struct cdev_ops cdev_generic_ops = {
	.probe = cdev_generic_probe,
	.fill_column = cdev_generic_fill_column,
};

/*
 * misc device driver
 */
static bool cdev_misc_probe(struct cdev *cdev) {
	return cdev->devdrv && strcmp(cdev->devdrv, "misc") == 0;
}

static bool cdev_misc_fill_column(struct proc *proc  __attribute__((__unused__)),
				  struct cdev *cdev,
				  struct libscols_line *ln __attribute__((__unused__)),
				  int column_id,
				  size_t column_index __attribute__((__unused__)),
				  char **str)
{
	struct file *file = &cdev->file;
	const char *miscdev;

	switch(column_id) {
	case COL_MISCDEV:
		miscdev = get_miscdev(minor(file->stat.st_rdev));
		if (miscdev)
			*str = xstrdup(miscdev);
		else
			xasprintf(str, "%u",
				  minor(file->stat.st_rdev));
		return true;
	case COL_SOURCE:
		miscdev = get_miscdev(minor(file->stat.st_rdev));
		if (miscdev)
			xasprintf(str, "misc:%s", miscdev);
		else
			xasprintf(str, "misc:%u",
				  minor(file->stat.st_rdev));
		return true;
	}
	return false;
}

static struct cdev_ops cdev_misc_ops = {
	.parent = &cdev_generic_ops,
	.probe = cdev_misc_probe,
	.fill_column = cdev_misc_fill_column,
};

/*
 * tun devcie driver
 */
static bool cdev_tun_probe(struct cdev *cdev)
{
	const char *miscdev;

	if ((!cdev->devdrv) || strcmp(cdev->devdrv, "misc"))
		return false;

	miscdev = get_miscdev(minor(cdev->file.stat.st_rdev));
	if (miscdev && strcmp(miscdev, "tun") == 0)
		return true;
	return false;
}

static void cdev_tun_free(const struct cdev *cdev)
{
	if (cdev->cdev_data)
		free(cdev->cdev_data);
}

static char * cdev_tun_get_name(struct cdev *cdev)
{
	char *str = NULL;

	if (cdev->cdev_data == NULL)
		return NULL;

	xasprintf(&str, "iface=%s", (const char *)cdev->cdev_data);
	return str;
}

static bool cdev_tun_fill_column(struct proc *proc  __attribute__((__unused__)),
				 struct cdev *cdev,
				 struct libscols_line *ln __attribute__((__unused__)),
				 int column_id,
				 size_t column_index __attribute__((__unused__)),
				 char **str)
{
	switch(column_id) {
	case COL_MISCDEV:
		*str = xstrdup("tun");
		return true;
	case COL_SOURCE:
		*str = xstrdup("misc:tun");
		return true;
	case COL_TUN_IFACE:
		if (cdev->cdev_data) {
			*str = xstrdup(cdev->cdev_data);
			return true;
		}
	}
	return false;
}

static int cdev_tun_handle_fdinfo(struct cdev *cdev, const char *key, const char *val)
{
	if (strcmp(key, "iff") == 0 && cdev->cdev_data == NULL) {
		cdev->cdev_data = xstrdup(val);
		return 1;
	}
	return false;
}

static struct cdev_ops cdev_tun_ops = {
	.parent = &cdev_misc_ops,
	.probe = cdev_tun_probe,
	.free  = cdev_tun_free,
	.get_name = cdev_tun_get_name,
	.fill_column = cdev_tun_fill_column,
	.handle_fdinfo = cdev_tun_handle_fdinfo,
};

/*
 * tty devices
 */
struct ttydata {
	struct cdev *cdev;
	const struct ttydrv *drv;
#define NO_TTY_INDEX -1
	int tty_index;		/* used only in ptmx devices */
	struct ipc_endpoint endpoint;
};

static bool cdev_tty_probe(struct cdev *cdev) {
	const struct ttydrv *ttydrv = get_ttydrv(major(cdev->file.stat.st_rdev),
						 minor(cdev->file.stat.st_rdev));
	struct ttydata *data;

	if (!ttydrv)
		return false;

	data = xmalloc(sizeof(struct ttydata));
	data->cdev = cdev;
	data->drv = ttydrv;
	data->tty_index = NO_TTY_INDEX;
	cdev->cdev_data = data;

	return true;
}

static void cdev_tty_free(const struct cdev *cdev)
{
	if (cdev->cdev_data)
		free(cdev->cdev_data);
}

static char * cdev_tty_get_name(struct cdev *cdev)
{
	struct ttydata *data = cdev->cdev_data;
	char *str = NULL;

	if (!data->drv->is_ptmx)
		return NULL;

	if (data->tty_index == NO_TTY_INDEX)
		str = xstrdup("tty-index=");
	else
		xasprintf(&str, "tty-index=%d", data->tty_index);
	return str;
}

static inline char *cdev_tty_xstrendpoint(struct file *file)
{
	char *str = NULL;
	xasprintf(&str, "%d,%s,%d%c%c",
		  file->proc->pid, file->proc->command, file->association,
		  (file->mode & S_IRUSR)? 'r': '-',
		  (file->mode & S_IWUSR)? 'w': '-');
	return str;
}

static bool cdev_tty_fill_column(struct proc *proc  __attribute__((__unused__)),
				 struct cdev *cdev,
				 struct libscols_line *ln __attribute__((__unused__)),
				 int column_id,
				 size_t column_index __attribute__((__unused__)),
				 char **str)
{
	struct file *file = &cdev->file;
	struct ttydata *data = cdev->cdev_data;

	switch(column_id) {
	case COL_SOURCE:
		if (data->drv->minor_start == data->drv->minor_end)
			*str = xstrdup(data->drv->name);
		else
			xasprintf(str, "%s:%u", data->drv->name,
				  minor(file->stat.st_rdev));
		return true;
	case COL_PTMX_TTY_INDEX:
		if (data->drv->is_ptmx) {
			xasprintf(str, "%d", data->tty_index);
			return true;
		}
		return false;
	case COL_ENDPOINTS:
		if (is_pty(data->drv)) {
			struct ttydata *this = data;
			struct list_head *e;
			foreach_endpoint(e, data->endpoint) {
				char *estr;
				struct ttydata *other = list_entry(e, struct ttydata, endpoint.endpoints);
				if (this == other)
					continue;

				if ((this->drv->is_ptmx && !other->drv->is_pts)
				    || (this->drv->is_pts && !other->drv->is_ptmx))
					continue;

				if (*str)
					xstrputc(str, '\n');
				estr = cdev_tty_xstrendpoint(&other->cdev->file);
				xstrappend(str, estr);
				free(estr);
			}
			if (*str)
				return true;
		}
		return false;
	default:
		return false;
	}
}

static int cdev_tty_handle_fdinfo(struct cdev *cdev, const char *key, const char *val)
{
	struct ttydata *data = cdev->cdev_data;

	if (!data->drv->is_ptmx)
		return 0;

	if (strcmp(key, "tty-index") == 0) {
		errno = 0;
		data->tty_index = (int)strtol(val, NULL, 10);
		if (errno) {
			data->tty_index = NO_TTY_INDEX;
			return 0;
		}
		return 1;
	}

	return 0;
}

struct cdev_pty_ipc {
	struct ipc ipc;
	int tty_index;
};

static unsigned int cdev_pty_get_hash(struct file *file)
{
	struct cdev *cdev = (struct cdev *)file;
	struct ttydata *data = cdev->cdev_data;

	return data->drv->is_ptmx?
		(unsigned int)data->tty_index:
		(unsigned int)minor(file->stat.st_rdev);
}

static bool cdev_pty_is_suitable_ipc(struct ipc *ipc, struct file *file)
{
	struct cdev *cdev = (struct cdev *)file;
	struct ttydata *data = cdev->cdev_data;
	struct cdev_pty_ipc *cdev_pty_ipc = (struct cdev_pty_ipc *)ipc;

	return (data->drv->is_ptmx)?
		cdev_pty_ipc->tty_index == (int)data->tty_index:
		cdev_pty_ipc->tty_index == (int)minor(file->stat.st_rdev);
}

static const struct ipc_class *cdev_tty_get_ipc_class(struct cdev *cdev)
{
	static const struct ipc_class cdev_pty_ipc_class = {
		.size = sizeof(struct cdev_pty_ipc),
		.get_hash = cdev_pty_get_hash,
		.is_suitable_ipc = cdev_pty_is_suitable_ipc,
	};

	struct ttydata *data = cdev->cdev_data;

	if (is_pty(data->drv))
		return &cdev_pty_ipc_class;

	return NULL;
}

static void cdev_tty_attach_xinfo(struct cdev *cdev)
{
	struct ttydata *data = cdev->cdev_data;
	struct ipc *ipc;
	unsigned int hash;


	if (! is_pty(data->drv))
		return;

	init_endpoint(&data->endpoint);
	ipc = get_ipc(&cdev->file);
	if (ipc)
		goto link;

	ipc = new_ipc(cdev_tty_get_ipc_class(cdev));
	hash = cdev_pty_get_hash(&cdev->file);
	((struct cdev_pty_ipc *)ipc)->tty_index = (int)hash;

	add_ipc(ipc, hash);
 link:
	add_endpoint(&data->endpoint, ipc);
}

static struct cdev_ops cdev_tty_ops = {
	.parent = &cdev_generic_ops,
	.probe = cdev_tty_probe,
	.free = cdev_tty_free,
	.get_name = cdev_tty_get_name,
	.fill_column = cdev_tty_fill_column,
	.attach_xinfo  = cdev_tty_attach_xinfo,
	.handle_fdinfo = cdev_tty_handle_fdinfo,
	.get_ipc_class = cdev_tty_get_ipc_class,
};

static const struct cdev_ops *cdev_ops[] = {
	&cdev_tun_ops,
	&cdev_misc_ops,
	&cdev_tty_ops,
	&cdev_generic_ops		  /* This must be at the end. */
};

static const struct cdev_ops *cdev_probe(struct cdev *cdev)
{
	const struct cdev_ops *r = NULL;

	for (size_t i = 0; i < ARRAY_SIZE(cdev_ops); i++) {
		if (cdev_ops[i]->probe(cdev)) {
			r = cdev_ops[i];
			break;
		}
	}

	assert(r);
	return r;
}

static void init_cdev_content(struct file *file)
{
	struct cdev *cdev = (struct cdev *)file;

	cdev->devdrv = get_chrdrv(major(file->stat.st_rdev));

	cdev->cdev_data = NULL;
	cdev->cdev_ops = cdev_probe(cdev);
	if (cdev->cdev_ops->init)
		cdev->cdev_ops->init(cdev);
}

static void free_cdev_content(struct file *file)
{
	struct cdev *cdev = (struct cdev *)file;

	if (cdev->cdev_ops->free)
		cdev->cdev_ops->free(cdev);
}

static void cdev_attach_xinfo(struct file *file)
{
	struct cdev *cdev = (struct cdev *)file;

	if (cdev->cdev_ops->attach_xinfo)
		cdev->cdev_ops->attach_xinfo(cdev);
}

static int cdev_handle_fdinfo(struct file *file, const char *key, const char *value)
{
	struct cdev *cdev = (struct cdev *)file;

	if (cdev->cdev_ops->handle_fdinfo)
		return cdev->cdev_ops->handle_fdinfo(cdev, key, value);
	return 0;		/* Should be handled in parents */
}

static const struct ipc_class *cdev_get_ipc_class(struct file *file)
{
	struct cdev *cdev = (struct cdev *)file;

	if (cdev->cdev_ops->get_ipc_class)
		return cdev->cdev_ops->get_ipc_class(cdev);
	return NULL;
}

const struct file_class cdev_class = {
	.super = &file_class,
	.size = sizeof(struct cdev),
	.initialize_class = cdev_class_initialize,
	.finalize_class = cdev_class_finalize,
	.fill_column = cdev_fill_column,
	.initialize_content = init_cdev_content,
	.free_content = free_cdev_content,
	.attach_xinfo = cdev_attach_xinfo,
	.handle_fdinfo = cdev_handle_fdinfo,
	.get_ipc_class = cdev_get_ipc_class,
};
