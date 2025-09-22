/*
 * lsfd-unkn.c - handle associations opening unknown objects
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

#include <linux/bpf.h>
#include <sys/syscall.h>
#include <sys/timerfd.h>
#include <time.h>

#include "signames.h"
#include "timeutils.h"

#include "lsfd.h"
#include "lsfd-pidfd.h"

#define offsetofend(TYPE, MEMBER)				\
	(offsetof(TYPE, MEMBER)	+ sizeof_member(TYPE, MEMBER))

struct unkn {
	struct file file;
	const struct anon_ops *anon_ops;
	void *anon_data;
};

struct anon_ops {
	const char *class;
	bool (*probe)(const char *);
	char * (*get_name)(struct unkn *);
	/* Return true is handled the column. */
	bool (*fill_column)(struct proc *,
			    struct unkn *,
			    struct libscols_line *,
			    int,
			    size_t,
			    char **str);
	void (*init)(struct unkn *);
	void (*free)(struct unkn *);
	int (*handle_fdinfo)(struct unkn *, const char *, const char *);
	void (*attach_xinfo)(struct unkn *);
	const struct ipc_class *ipc_class;
};

static const struct anon_ops *anon_probe(const char *);

static char * anon_get_class(struct unkn *unkn)
{
	char *name;

	if (unkn->anon_ops->class)
		return xstrdup(unkn->anon_ops->class);

	/* See unkn_init_content() */
	name = ((struct file *)unkn)->name + 11;
	/* Does it have the form anon_inode:[class]? */
	if (*name == '[') {
		size_t len = strlen(name + 1);
		if (*(name + 1 + len - 1) == ']')
			return strndup(name + 1, len - 1);
	}

	return xstrdup(name);
}

static bool unkn_fill_column(struct proc *proc,
			     struct file *file,
			     struct libscols_line *ln,
			     int column_id,
			     size_t column_index)
{
	char *str = NULL;
	struct unkn *unkn = (struct unkn *)file;

	switch(column_id) {
	case COL_NAME:
		if (unkn->anon_ops && unkn->anon_ops->get_name) {
			str = unkn->anon_ops->get_name(unkn);
			if (str)
				break;
		}
		return false;
	case COL_TYPE:
		if (!unkn->anon_ops)
			return false;
		/* FALL THROUGH */
	case COL_AINODECLASS:
		if (unkn->anon_ops) {
			str = anon_get_class(unkn);
			break;
		}
		return false;
	case COL_SOURCE:
		if (unkn->anon_ops) {
			str = xstrdup("anon_inodefs");
			break;
		}
		return false;
	default:
		if (unkn->anon_ops && unkn->anon_ops->fill_column) {
			if (unkn->anon_ops->fill_column(proc, unkn, ln,
							column_id, column_index, &str))
				break;
		}
		return false;
	}

	if (!str)
		err(EXIT_FAILURE, _("failed to add output data"));
	if (scols_line_refer_data(ln, column_index, str))
		err(EXIT_FAILURE, _("failed to add output data"));
	return true;
}

static void unkn_attach_xinfo(struct file *file)
{
	struct unkn *unkn = (struct unkn *)file;
	if (unkn->anon_ops && unkn->anon_ops->attach_xinfo)
		unkn->anon_ops->attach_xinfo(unkn);
}

static const struct ipc_class *unkn_get_ipc_class(struct file *file)
{
	struct unkn *unkn = (struct unkn *)file;

	if (unkn->anon_ops && unkn->anon_ops->ipc_class)
		return unkn->anon_ops->ipc_class;
	return NULL;
}

static void unkn_init_content(struct file *file)
{
	struct unkn *unkn = (struct unkn *)file;

	assert(file);
	unkn->anon_ops = NULL;
	unkn->anon_data = NULL;

	if (major(file->stat.st_dev) == 0
	    && strncmp(file->name, "anon_inode:", 11) == 0) {
		const char *rest = file->name + 11;

		unkn->anon_ops = anon_probe(rest);

		if (unkn->anon_ops->init)
			unkn->anon_ops->init(unkn);
	}
}

static void unkn_content_free(struct file *file)
{
	struct unkn *unkn = (struct unkn *)file;

	assert(file);
	if (unkn->anon_ops && unkn->anon_ops->free)
		unkn->anon_ops->free((struct unkn *)file);
}

static int unkn_handle_fdinfo(struct file *file, const char *key, const char *value)
{
	struct unkn *unkn = (struct unkn *)file;

	assert(file);
	if (unkn->anon_ops && unkn->anon_ops->handle_fdinfo)
		return unkn->anon_ops->handle_fdinfo(unkn, key, value);
	return 0;		/* Should be handled in parents */
}

/*
 * pidfd
 */

static bool anon_pidfd_probe(const char *str)
{
	return strncmp(str, "[pidfd]", 7) == 0;
}

static char *anon_pidfd_get_name(struct unkn *unkn)
{
	struct pidfd_data *data = (struct pidfd_data *)unkn->anon_data;

	return pidfd_get_name(data);
}

static void anon_pidfd_init(struct unkn *unkn)
{
	unkn->anon_data = xcalloc(1, sizeof(struct pidfd_data));
}

static void anon_pidfd_free(struct unkn *unkn)
{
	struct pidfd_data *data = (struct pidfd_data *)unkn->anon_data;

	pidfd_free(data);
	free(data);
}

static int anon_pidfd_handle_fdinfo(struct unkn *unkn, const char *key, const char *value)
{
	return pidfd_handle_fdinfo((struct pidfd_data *)unkn->anon_data,
				   key, value);
}

static bool anon_pidfd_fill_column(struct proc *proc  __attribute__((__unused__)),
				   struct unkn *unkn,
				   struct libscols_line *ln __attribute__((__unused__)),
				   int column_id,
				   size_t column_index __attribute__((__unused__)),
				   char **str)
{
	return pidfd_fill_column((struct pidfd_data *)unkn->anon_data,
				 column_id,
				 str);
}

static const struct anon_ops anon_pidfd_ops = {
	.class = "pidfd",
	.probe = anon_pidfd_probe,
	.get_name = anon_pidfd_get_name,
	.fill_column = anon_pidfd_fill_column,
	.init = anon_pidfd_init,
	.free = anon_pidfd_free,
	.handle_fdinfo = anon_pidfd_handle_fdinfo,
};

/*
 * eventfd
 */
struct anon_eventfd_data {
	int id;
	struct unkn *backptr;
	struct ipc_endpoint endpoint;
};

struct eventfd_ipc {
	struct ipc ipc;
	int id;
};

static unsigned int anon_eventfd_get_hash(struct file *file)
{
	struct unkn *unkn = (struct unkn *)file;
	struct anon_eventfd_data *data = (struct anon_eventfd_data *)unkn->anon_data;

	return (unsigned int)data->id;
}

static bool anon_eventfd_is_suitable_ipc(struct ipc *ipc, struct file *file)
{
	struct unkn *unkn = (struct unkn *)file;
	struct anon_eventfd_data *data = (struct anon_eventfd_data *)unkn->anon_data;

	return ((struct eventfd_ipc *)ipc)->id == data->id;
}

static const struct ipc_class anon_eventfd_ipc_class = {
	.size = sizeof(struct eventfd_ipc),
	.get_hash = anon_eventfd_get_hash,
	.is_suitable_ipc = anon_eventfd_is_suitable_ipc,
	.free = NULL,
};

static bool anon_eventfd_probe(const char *str)
{
	return strncmp(str, "[eventfd]", 9) == 0;
}

static char *anon_eventfd_get_name(struct unkn *unkn)
{
	char *str = NULL;
	struct anon_eventfd_data *data = (struct anon_eventfd_data *)unkn->anon_data;

	xasprintf(&str, "id=%d", data->id);
	return str;
}

static void anon_eventfd_init(struct unkn *unkn)
{
	struct anon_eventfd_data *data = xcalloc(1, sizeof(struct anon_eventfd_data));
	init_endpoint(&data->endpoint);
	data->backptr = unkn;
	unkn->anon_data = data;
}

static void anon_eventfd_free(struct unkn *unkn)
{
	free(unkn->anon_data);
}

static void anon_eventfd_attach_xinfo(struct unkn *unkn)
{
	struct anon_eventfd_data *data = (struct anon_eventfd_data *)unkn->anon_data;
	unsigned int hash;
	struct ipc *ipc = get_ipc(&unkn->file);
	if (ipc)
		goto link;

	ipc = new_ipc(&anon_eventfd_ipc_class);
	((struct eventfd_ipc *)ipc)->id = data->id;

	hash = anon_eventfd_get_hash(&unkn->file);
	add_ipc(ipc, hash);

 link:
	add_endpoint(&data->endpoint, ipc);
}

static int anon_eventfd_handle_fdinfo(struct unkn *unkn, const char *key, const char *value)
{
	if (strcmp(key, "eventfd-id") == 0) {
		int64_t id;

		int rc = ul_strtos64(value, &id, 10);
		if (rc < 0)
			return 0;
		((struct anon_eventfd_data *)unkn->anon_data)->id = (int)id;
		return 1;
	}
	return 0;
}

static inline char *anon_eventfd_data_xstrendpoint(struct file *file)
{
	char *str = NULL;
	xasprintf(&str, "%d,%s,%d",
		  file->proc->pid, file->proc->command, file->association);
	return str;
}

static bool anon_eventfd_fill_column(struct proc *proc  __attribute__((__unused__)),
				     struct unkn *unkn,
				     struct libscols_line *ln __attribute__((__unused__)),
				     int column_id,
				     size_t column_index __attribute__((__unused__)),
				     char **str)
{
	struct anon_eventfd_data *data = (struct anon_eventfd_data *)unkn->anon_data;

	switch(column_id) {
	case COL_EVENTFD_ID:
		xasprintf(str, "%d", data->id);
		return true;
	case COL_ENDPOINTS: {
		struct list_head *e;
		char *estr;
		foreach_endpoint(e, data->endpoint) {
			struct anon_eventfd_data *other = list_entry(e,
								     struct anon_eventfd_data,
								     endpoint.endpoints);
			if (data == other)
				continue;
			if (*str)
				xstrputc(str, '\n');
			estr = anon_eventfd_data_xstrendpoint(&other->backptr->file);
			xstrappend(str, estr);
			free(estr);
		}
		if (!*str)
			return false;
		return true;
	}
	default:
		return false;
	}
}

static const struct anon_ops anon_eventfd_ops = {
	.class = "eventfd",
	.probe = anon_eventfd_probe,
	.get_name = anon_eventfd_get_name,
	.fill_column = anon_eventfd_fill_column,
	.init = anon_eventfd_init,
	.free = anon_eventfd_free,
	.handle_fdinfo = anon_eventfd_handle_fdinfo,
	.attach_xinfo = anon_eventfd_attach_xinfo,
	.ipc_class = &anon_eventfd_ipc_class,
};

/*
 * eventpoll
 */
struct anon_eventpoll_data {
	size_t count;
	int *tfds;
	struct list_head siblings;
};

static bool anon_eventpoll_probe(const char *str)
{
	return strncmp(str, "[eventpoll]", 11) == 0;
}

static void anon_eventpoll_init(struct unkn *unkn)
{
	struct anon_eventpoll_data *data = xcalloc(1, sizeof(struct anon_eventpoll_data));
	INIT_LIST_HEAD(&data->siblings);
	unkn->anon_data = data;
}

static void anon_eventpoll_free(struct unkn *unkn)
{
	struct anon_eventpoll_data *data = unkn->anon_data;
	free(data->tfds);
	free(data);
}

static int anon_eventpoll_handle_fdinfo(struct unkn *unkn, const char *key, const char *value)
{
	struct anon_eventpoll_data *data;
	if (strcmp(key, "tfd") == 0) {
		unsigned long tfd;
		char *end = NULL;

		errno = 0;
		tfd = strtoul(value, &end, 0);
		if (errno != 0)
			return 0; /* ignore -- parse failed */

		data = (struct anon_eventpoll_data *)unkn->anon_data;
		data->tfds = xreallocarray(data->tfds, ++data->count, sizeof(int));
		data->tfds[data->count - 1] = (int)tfd;
		return 1;
	}
	return 0;
}

static int intcmp(const void *a, const void *b)
{
	int ai = *(int *)a;
	int bi = *(int *)b;

	return ai - bi;
}

static void anon_eventpoll_attach_xinfo(struct unkn *unkn)
{
	struct anon_eventpoll_data *data = (struct anon_eventpoll_data *)unkn->anon_data;
	if (data->count > 0) {
		qsort(data->tfds, data->count, sizeof(data->tfds[0]),
		      intcmp);
		list_add_tail(&data->siblings,
			      &unkn->file.proc->eventpolls);
	}
}

static char *anon_eventpoll_make_tfds_string(struct anon_eventpoll_data *data,
					     const char *prefix,
					     const char sep)
{
	char *str = prefix? xstrdup(prefix): NULL;

	char buf[256];
	for (size_t i = 0; i < data->count; i++) {
		size_t offset = 0;

		if (i > 0) {
			buf[0] = sep;
			offset = 1;
		}
		snprintf(buf + offset, sizeof(buf) - offset, "%d", data->tfds[i]);
		xstrappend(&str, buf);
	}
	return str;
}

static char *anon_eventpoll_get_name(struct unkn *unkn)
{
	return anon_eventpoll_make_tfds_string((struct anon_eventpoll_data *)unkn->anon_data,
					       "tfds=", ',');
}

static bool anon_eventpoll_fill_column(struct proc *proc  __attribute__((__unused__)),
				       struct unkn *unkn,
				       struct libscols_line *ln __attribute__((__unused__)),
				       int column_id,
				       size_t column_index __attribute__((__unused__)),
				       char **str)
{
	struct anon_eventpoll_data *data = (struct anon_eventpoll_data *)unkn->anon_data;

	switch(column_id) {
	case COL_EVENTPOLL_TFDS:
		*str =anon_eventpoll_make_tfds_string(data, NULL, '\n');
		if (*str)
			return true;
		break;
	}

	return false;
}

static const struct anon_ops anon_eventpoll_ops = {
	.class = "eventpoll",
	.probe = anon_eventpoll_probe,
	.get_name = anon_eventpoll_get_name,
	.fill_column = anon_eventpoll_fill_column,
	.init = anon_eventpoll_init,
	.free = anon_eventpoll_free,
	.handle_fdinfo = anon_eventpoll_handle_fdinfo,
	.attach_xinfo = anon_eventpoll_attach_xinfo,
};

static int numcomp(const void *a, const void *b)
{
	return *(int *)a - *(int *)b;
}

bool is_multiplexed_by_eventpoll(int fd, struct list_head *eventpolls)
{
	struct list_head *t;
	list_for_each (t, eventpolls) {
		struct anon_eventpoll_data *data = list_entry(t, struct anon_eventpoll_data, siblings);
		if (data->count) {
			if (bsearch(&fd, data->tfds,
				    data->count, sizeof(data->tfds[0]),
				    numcomp))
			    return true;
		}
	}
	return false;
}

/*
 * timerfd
 */
struct anon_timerfd_data {
	int clockid;
	struct itimerspec itimerspec;
};

static bool anon_timerfd_probe(const char *str)
{
	return strncmp(str, "[timerfd]", 9) == 0;
}

static void anon_timerfd_init(struct unkn *unkn)
{
	unkn->anon_data = xcalloc(1, sizeof(struct anon_timerfd_data));
}

static void anon_timerfd_free(struct unkn *unkn)
{
	struct anon_timerfd_data *data = unkn->anon_data;
	free(data);
}

static int anon_timerfd_handle_fdinfo(struct unkn *unkn, const char *key, const char *value)
{
	struct anon_timerfd_data *data = (struct anon_timerfd_data *)unkn->anon_data;

	if (strcmp(key, "clockid") == 0) {
		unsigned long clockid;
		char *end = NULL;

		errno = 0;
		clockid = strtoul(value, &end, 0);
		if (errno != 0)
			return 0; /* ignore -- parse failed */
		if (*end != '\0')
			return 0; /* ignore -- garbage remains. */

		data->clockid = clockid;
		return 1;
	} else {
		struct timespec *t;
		uint64_t tv_sec;
		uint64_t tv_nsec;

		if (strcmp(key, "it_value") == 0)
			t = &data->itimerspec.it_value;
		else if (strcmp(key, "it_interval") == 0)
			t = &data->itimerspec.it_interval;
		else
			return 0;

		if (sscanf(value, "(%"SCNu64", %"SCNu64")",
			   &tv_sec, &tv_nsec) == 2) {
			t->tv_sec = (time_t)tv_sec;
			t->tv_nsec = (long)tv_nsec;
			return 1;
		}

		return 0;
	}
}

static const char *anon_timerfd_decode_clockid(int clockid)
{
	switch (clockid) {
	case CLOCK_REALTIME:
		return "realtime";
	case CLOCK_MONOTONIC:
		return "monotonic";
	case CLOCK_BOOTTIME:
		return "boottime";
	case CLOCK_REALTIME_ALARM:
		return "realtime-alarm";
	case CLOCK_BOOTTIME_ALARM:
		return "boottime-alarm";
	default:
		return "unknown";
	}
}

static void anon_timerfd_render_timespec_string(char *buf, size_t size,
						const char *prefix,
						const struct timespec *t)
{
	snprintf(buf, size, "%s%llu.%09ld",
		 prefix? prefix: "",
		 (unsigned long long)t->tv_sec, t->tv_nsec);
}

static char *anon_timerfd_get_name(struct unkn *unkn)
{
	char *str = NULL;

	struct anon_timerfd_data *data = (struct anon_timerfd_data *)unkn->anon_data;
	const struct timespec *exp;
	const struct timespec *ival;

	const char *clockid_name;
	char exp_buf[BUFSIZ] = {'\0'};
	char ival_buf[BUFSIZ] = {'\0'};

	clockid_name = anon_timerfd_decode_clockid(data->clockid);

	exp = &data->itimerspec.it_value;
	if (is_timespecset(exp))
		anon_timerfd_render_timespec_string(exp_buf, sizeof(exp_buf),
						    " remaining=", exp);

	ival = &data->itimerspec.it_interval;
	if (is_timespecset(ival))
		anon_timerfd_render_timespec_string(ival_buf, sizeof(ival_buf),
						    " interval=", ival);

	xasprintf(&str, "clockid=%s%s%s", clockid_name, exp_buf, ival_buf);
	return str;
}

static bool anon_timerfd_fill_column(struct proc *proc  __attribute__((__unused__)),
				     struct unkn *unkn,
				     struct libscols_line *ln __attribute__((__unused__)),
				     int column_id,
				     size_t column_index __attribute__((__unused__)),
				     char **str)
{
	struct anon_timerfd_data *data = (struct anon_timerfd_data *)unkn->anon_data;
	char buf[BUFSIZ] = {'\0'};

	switch(column_id) {
	case COL_TIMERFD_CLOCKID:
		*str = xstrdup(anon_timerfd_decode_clockid(data->clockid));
		return true;
	case COL_TIMERFD_INTERVAL:
		anon_timerfd_render_timespec_string(buf, sizeof(buf), NULL,
						    &data->itimerspec.it_interval);
		*str = xstrdup(buf);
		return true;
	case COL_TIMERFD_REMAINING:
		anon_timerfd_render_timespec_string(buf, sizeof(buf), NULL,
						    &data->itimerspec.it_value);
		*str = xstrdup(buf);
		return true;
	}

	return false;
}

static const struct anon_ops anon_timerfd_ops = {
	.class = "timerfd",
	.probe = anon_timerfd_probe,
	.get_name = anon_timerfd_get_name,
	.fill_column = anon_timerfd_fill_column,
	.init = anon_timerfd_init,
	.free = anon_timerfd_free,
	.handle_fdinfo = anon_timerfd_handle_fdinfo,
};

/*
 * signalfd
 */
struct anon_signalfd_data {
	uint64_t sigmask;
};

static bool anon_signalfd_probe(const char *str)
{
	return strncmp(str, "[signalfd]", 10) == 0;
}

static void anon_signalfd_init(struct unkn *unkn)
{
	unkn->anon_data = xcalloc(1, sizeof(struct anon_signalfd_data));
}

static void anon_signalfd_free(struct unkn *unkn)
{
	struct anon_signalfd_data *data = unkn->anon_data;
	free(data);
}

static int anon_signalfd_handle_fdinfo(struct unkn *unkn, const char *key, const char *value)
{
	struct anon_signalfd_data *data = (struct anon_signalfd_data *)unkn->anon_data;

	if (strcmp(key, "sigmask") == 0) {
		if (ul_strtou64(value, &data->sigmask, 16) < 0) {
			data->sigmask = 0;
			return 0;
		}
	}
	return 0;
}

static char *anon_signalfd_make_mask_string(const char* prefix, uint64_t sigmask)
{
	char *str = NULL;

	for (size_t i = 0; i < sizeof(sigmask) * 8; i++) {
		if ((((uint64_t)0x1) << i) & sigmask) {
			const int signum = i + 1;
			const char *signame = signum_to_signame(signum);

			if (str)
				xstrappend(&str, ",");
			else if (prefix)
				xstrappend(&str, prefix);

			if (signame) {
				xstrappend(&str, signame);
			} else {
				char buf[BUFSIZ];
				snprintf(buf, sizeof(buf), "%d", signum);
				xstrappend(&str, buf);
			}
		}
	}

	return str;
}

static char *anon_signalfd_get_name(struct unkn *unkn)
{
	struct anon_signalfd_data *data = (struct anon_signalfd_data *)unkn->anon_data;
	return anon_signalfd_make_mask_string("mask=", data->sigmask);
}

static bool anon_signalfd_fill_column(struct proc *proc  __attribute__((__unused__)),
				      struct unkn *unkn,
				      struct libscols_line *ln __attribute__((__unused__)),
				      int column_id,
				      size_t column_index __attribute__((__unused__)),
				      char **str)
{
	struct anon_signalfd_data *data = (struct anon_signalfd_data *)unkn->anon_data;

	switch(column_id) {
	case COL_SIGNALFD_MASK:
		*str = anon_signalfd_make_mask_string(NULL, data->sigmask);
		return true;
	default:
		return false;
	}
}

static const struct anon_ops anon_signalfd_ops = {
	.class = "signalfd",
	.probe = anon_signalfd_probe,
	.get_name = anon_signalfd_get_name,
	.fill_column = anon_signalfd_fill_column,
	.init = anon_signalfd_init,
	.free = anon_signalfd_free,
	.handle_fdinfo = anon_signalfd_handle_fdinfo,
};

/*
 * inotify
 */
struct anon_inotify_data {
	struct list_head inodes;
};

struct anon_inotify_inode {
	ino_t ino;
	dev_t sdev;
	struct list_head inodes;
};

static bool anon_inotify_probe(const char *str)
{
	return strncmp(str, "inotify", 7) == 0;
}

/* A device number appeared in fdinfo of an inotify file uses the kernel
 * internal representation. It is different from what we are familiar with;
 * major(3) and minor(3) don't work with the representation.
 * See linux/include/linux/kdev_t.h. */
#define ANON_INOTIFY_MINORBITS	20
#define ANON_INOTIFY_MINORMASK	((1U << ANON_INOTIFY_MINORBITS) - 1)

#define ANON_INOTIFY_MAJOR(dev)	((unsigned int) ((dev) >> ANON_INOTIFY_MINORBITS))
#define ANON_INOTIFY_MINOR(dev)	((unsigned int) ((dev) &  ANON_INOTIFY_MINORMASK))

static char *anon_inotify_make_inodes_string(const char *prefix,
					     const char *sep,
					     enum decode_source_level decode_level,
					     struct anon_inotify_data *data)
{
	char *str = NULL;
	char buf[BUFSIZ] = {'\0'};
	bool first_element = true;

	struct list_head *i;
	list_for_each(i, &data->inodes) {
		char source[BUFSIZ/2] = {'\0'};
		struct anon_inotify_inode *inode = list_entry(i,
							      struct anon_inotify_inode,
							      inodes);

		decode_source(source, sizeof(source),
			      ANON_INOTIFY_MAJOR(inode->sdev), ANON_INOTIFY_MINOR(inode->sdev),
			      decode_level);
		snprintf(buf, sizeof(buf), "%s%llu@%s", first_element? prefix: sep,
			 (unsigned long long)inode->ino, source);
		first_element = false;

		xstrappend(&str, buf);
	}

	return str;
}

static char *anon_inotify_get_name(struct unkn *unkn)
{
	return anon_inotify_make_inodes_string("inodes=", ",", DECODE_SOURCE_FULL,
					       (struct anon_inotify_data *)unkn->anon_data);
}

static void anon_inotify_init(struct unkn *unkn)
{
	struct anon_inotify_data *data = xcalloc(1, sizeof(struct anon_inotify_data));
	INIT_LIST_HEAD (&data->inodes);
	unkn->anon_data = data;
}

static void anon_inotify_free(struct unkn *unkn)
{
	struct anon_inotify_data *data = unkn->anon_data;

	list_free(&data->inodes, struct anon_inotify_inode, inodes,
		  free);
	free(data);
}

static void add_inode(struct anon_inotify_data *data, ino_t ino, dev_t sdev)
{
	struct anon_inotify_inode *inode = xmalloc(sizeof(*inode));

	INIT_LIST_HEAD (&inode->inodes);
	inode->ino = ino;
	inode->sdev = sdev;

	list_add_tail(&inode->inodes, &data->inodes);
}

static int anon_inotify_handle_fdinfo(struct unkn *unkn, const char *key, const char *value)
{
	struct anon_inotify_data *data = (struct anon_inotify_data *)unkn->anon_data;

	if (strcmp(key, "inotify wd") == 0) {
		unsigned long long ino;
		unsigned long long sdev;

		if (sscanf(value, "%*d ino:%llx sdev:%llx %*s", &ino, &sdev) == 2) {
			add_inode(data, (ino_t)ino, (dev_t)sdev);
			return 1;
		}
	}
	return 0;
}

static bool anon_inotify_fill_column(struct proc *proc  __attribute__((__unused__)),
				     struct unkn *unkn,
				     struct libscols_line *ln __attribute__((__unused__)),
				     int column_id,
				     size_t column_index __attribute__((__unused__)),
				     char **str)
{
	struct anon_inotify_data *data = (struct anon_inotify_data *)unkn->anon_data;

	switch(column_id) {
	case COL_INOTIFY_INODES:
		*str = anon_inotify_make_inodes_string("", "\n", DECODE_SOURCE_FULL,
						       data);
		if (*str)
			return true;
		break;
	case COL_INOTIFY_INODES_RAW:
		*str = anon_inotify_make_inodes_string("", "\n", DECODE_SOURCE_MAJMIN,
						       data);
		if (*str)
			return true;
		break;
	}

	return false;
}

static const struct anon_ops anon_inotify_ops = {
	.class = "inotify",
	.probe = anon_inotify_probe,
	.get_name = anon_inotify_get_name,
	.fill_column = anon_inotify_fill_column,
	.init = anon_inotify_init,
	.free = anon_inotify_free,
	.handle_fdinfo = anon_inotify_handle_fdinfo,
};

/*
 * bpf-prog
 *
 * Generally, we use "-" as the word separators in lsfd's output.
 * However, about bpf*, we use "_" because bpftool uses "_".
 */
static const char *bpf_prog_type_table[] = {
	[0] = "unspec",		   /* BPF_PROG_TYPE_UNSPEC*/
	[1] = "socket_filter",	   /* BPF_PROG_TYPE_SOCKET_FILTER*/
	[2] = "kprobe",		   /* BPF_PROG_TYPE_KPROBE*/
	[3] = "sched_cls",	   /* BPF_PROG_TYPE_SCHED_CLS*/
	[4] = "sched_act",	   /* BPF_PROG_TYPE_SCHED_ACT*/
	[5] = "tracepoint",	   /* BPF_PROG_TYPE_TRACEPOINT*/
	[6] = "xdp",		   /* BPF_PROG_TYPE_XDP*/
	[7] = "perf_event",	   /* BPF_PROG_TYPE_PERF_EVENT*/
	[8] = "cgroup_skb",	   /* BPF_PROG_TYPE_CGROUP_SKB*/
	[9] = "cgroup_sock",	   /* BPF_PROG_TYPE_CGROUP_SOCK*/
	[10] = "lwt_in",	   /* BPF_PROG_TYPE_LWT_IN*/
	[11] = "lwt_out",	   /* BPF_PROG_TYPE_LWT_OUT*/
	[12] = "lwt_xmit",	   /* BPF_PROG_TYPE_LWT_XMIT*/
	[13] = "sock_ops",	   /* BPF_PROG_TYPE_SOCK_OPS*/
	[14] = "sk_skb",	   /* BPF_PROG_TYPE_SK_SKB*/
	[15] = "cgroup_device",	   /* BPF_PROG_TYPE_CGROUP_DEVICE*/
	[16] = "sk_msg",	   /* BPF_PROG_TYPE_SK_MSG*/
	[17] = "raw_tracepoint",   /* BPF_PROG_TYPE_RAW_TRACEPOINT*/
	[18] = "cgroup_sock_addr", /* BPF_PROG_TYPE_CGROUP_SOCK_ADDR*/
	[19] = "lwt_seg6local",	   /* BPF_PROG_TYPE_LWT_SEG6LOCAL*/
	[20] = "lirc_mode2",	   /* BPF_PROG_TYPE_LIRC_MODE2*/
	[21] = "sk_reuseport",	   /* BPF_PROG_TYPE_SK_REUSEPORT*/
	[22] = "flow_dissector",   /* BPF_PROG_TYPE_FLOW_DISSECTOR*/
	[23] = "cgroup_sysctl",	   /* BPF_PROG_TYPE_CGROUP_SYSCTL*/
	[24] = "raw_tracepoint_writable", /* BPF_PROG_TYPE_RAW_TRACEPOINT_WRITABLE*/
	[25] = "cgroup_sockopt", /* BPF_PROG_TYPE_CGROUP_SOCKOPT*/
	[26] = "tracing",	 /* BPF_PROG_TYPE_TRACING*/
	[27] = "struct_ops",	 /* BPF_PROG_TYPE_STRUCT_OPS*/
	[28] = "ext",		 /* BPF_PROG_TYPE_EXT*/
	[29] = "lsm",		 /* BPF_PROG_TYPE_LSM*/
	[30] = "sk_lookup",	 /* BPF_PROG_TYPE_SK_LOOKUP*/
	[31] = "syscall",	 /* BPF_PROG_TYPE_SYSCALL*/
};

struct anon_bpf_prog_data {
	int type;
	int id;
	char name[BPF_OBJ_NAME_LEN + 1];
};

static bool anon_bpf_prog_probe(const char *str)
{
	return strncmp(str, "bpf-prog", 8) == 0;
}

static const char *anon_bpf_prog_get_prog_type_name(int type)
{
	if (0 <= type && type < (int)ARRAY_SIZE(bpf_prog_type_table))
		return bpf_prog_type_table[type];
	return NULL;
}

static bool anon_bpf_prog_fill_column(struct proc *proc  __attribute__((__unused__)),
				      struct unkn *unkn,
				      struct libscols_line *ln __attribute__((__unused__)),
				      int column_id,
				      size_t column_index __attribute__((__unused__)),
				     char **str)
{
	struct anon_bpf_prog_data *data = (struct anon_bpf_prog_data *)unkn->anon_data;
	const char *t;

	switch(column_id) {
	case COL_BPF_PROG_ID:
		xasprintf(str, "%d", data->id);
		return true;
	case COL_BPF_PROG_TYPE_RAW:
		xasprintf(str, "%d", data->type);
		return true;
	case COL_BPF_PROG_TYPE:
		t = anon_bpf_prog_get_prog_type_name(data->type);
		if (t)
			*str = xstrdup(t);
		else
			xasprintf(str, "UNKNOWN(%d)", data->type);
		return true;
	case COL_BPF_NAME:
		*str = xstrdup(data->name);
		return true;
	default:
		return false;
	}
}

static char *anon_bpf_prog_get_name(struct unkn *unkn)
{
	const char *t;
	char *str = NULL;
	struct anon_bpf_prog_data *data = (struct anon_bpf_prog_data *)unkn->anon_data;

	t = anon_bpf_prog_get_prog_type_name(data->type);
	if (t)
		xasprintf(&str, "id=%d type=%s", data->id, t);
	else
		xasprintf(&str, "id=%d type=UNKNOWN(%d)", data->id, data->type);

	if (*data->name)
		xstrfappend(&str, " name=%s", data->name);

	return str;
}


static void anon_bpf_prog_init(struct unkn *unkn)
{
	struct anon_bpf_prog_data *data = xmalloc(sizeof(*data));
	data->type = -1;
	data->id = -1;
	data->name[0] = '\0';
	unkn->anon_data = data;
}

static void anon_bpf_prog_free(struct unkn *unkn)
{
	struct anon_bpf_prog_data *data = (struct anon_bpf_prog_data *)unkn->anon_data;
	free(data);
}

static void anon_bpf_prog_get_more_info(struct anon_bpf_prog_data *prog_data)
{
	union bpf_attr attr = {
		.prog_id = (int32_t)prog_data->id,
		.next_id = 0,
		.open_flags = 0,
	};
	struct bpf_prog_info info = { 0 };
	union bpf_attr info_attr = {
		.info.info_len = sizeof(info),
		.info.info = (uint64_t)(uintptr_t)&info,
	};

	int bpf_fd = syscall(SYS_bpf, BPF_PROG_GET_FD_BY_ID, &attr, sizeof(attr));
	if (bpf_fd < 0)
		return;

	info_attr.info.bpf_fd = bpf_fd;
	if (syscall(SYS_bpf, BPF_OBJ_GET_INFO_BY_FD, &info_attr, offsetofend(union bpf_attr, info)) == 0) {
		memcpy(prog_data->name,
		       info.name,
		       BPF_OBJ_NAME_LEN);
		prog_data->name[BPF_OBJ_NAME_LEN] = '\0';
	}
	close(bpf_fd);
}

static int anon_bpf_prog_handle_fdinfo(struct unkn *unkn, const char *key, const char *value)
{
	if (strcmp(key, "prog_id") == 0) {
		int32_t t = -1;
		int rc = ul_strtos32(value, &t, 10);
		if (rc < 0)
			return 0; /* ignore -- parse failed */
		((struct anon_bpf_prog_data *)unkn->anon_data)->id = (int)t;
		anon_bpf_prog_get_more_info((struct anon_bpf_prog_data *)unkn->anon_data);
		return 1;
	}

	if (strcmp(key, "prog_type") == 0) {
		int32_t t = -1;
		int rc = ul_strtos32(value, &t, 10);
		if (rc < 0)
			return 0; /* ignore -- parse failed */
		((struct anon_bpf_prog_data *)unkn->anon_data)->type = (int)t;
		return 1;
	}

	return 0;
}

static const struct anon_ops anon_bpf_prog_ops = {
	.class = "bpf-prog",
	.probe = anon_bpf_prog_probe,
	.get_name = anon_bpf_prog_get_name,
	.fill_column = anon_bpf_prog_fill_column,
	.init = anon_bpf_prog_init,
	.free = anon_bpf_prog_free,
	.handle_fdinfo = anon_bpf_prog_handle_fdinfo,
};

/*
 * bpf-map
 */
static const char *bpf_map_type_table[] = {
	[0] = "unspec",		  /* BPF_MAP_TYPE_UNSPEC */
	[1] = "hash",		  /* BPF_MAP_TYPE_HASH */
	[2] = "array",		  /* BPF_MAP_TYPE_ARRAY */
	[3] = "prog-array",	  /* BPF_MAP_TYPE_PROG_ARRAY */
	[4] = "perf-event-array", /* BPF_MAP_TYPE_PERF_EVENT_ARRAY */
	[5] = "percpu-hash",	  /* BPF_MAP_TYPE_PERCPU_HASH */
	[6] = "percpu-array",	  /* BPF_MAP_TYPE_PERCPU_ARRAY */
	[7] = "stack-trace",	  /* BPF_MAP_TYPE_STACK_TRACE */
	[8] = "cgroup-array",	  /* BPF_MAP_TYPE_CGROUP_ARRAY */
	[9] = "lru-hash",	  /* BPF_MAP_TYPE_LRU_HASH */
	[10] = "lru-percpu-hash", /* BPF_MAP_TYPE_LRU_PERCPU_HASH */
	[11] = "lpm-trie",	  /* BPF_MAP_TYPE_LPM_TRIE */
	[12] = "array-of-maps",	  /* BPF_MAP_TYPE_ARRAY_OF_MAPS */
	[13] = "hash-of-maps",	  /* BPF_MAP_TYPE_HASH_OF_MAPS */
	[14] = "devmap",	  /* BPF_MAP_TYPE_DEVMAP */
	[15] = "sockmap",	  /* BPF_MAP_TYPE_SOCKMAP */
	[16] = "cpumap",	  /* BPF_MAP_TYPE_CPUMAP */
	[17] = "xskmap",	  /* BPF_MAP_TYPE_XSKMAP */
	[18] = "sockhash",	  /* BPF_MAP_TYPE_SOCKHASH */
	[19] = "cgroup-storage",  /* BPF_MAP_TYPE_CGROUP_STORAGE */
	[20] = "reuseport-sockarray", /* BPF_MAP_TYPE_REUSEPORT_SOCKARRAY */
	[21] = "percpu-cgroup-storage", /* BPF_MAP_TYPE_PERCPU_CGROUP_STORAGE */
	[22] = "queue",			/* BPF_MAP_TYPE_QUEUE */
	[23] = "stack",			/* BPF_MAP_TYPE_STACK */
	[24] = "sk-storage",		/* BPF_MAP_TYPE_SK_STORAGE */
	[25] = "devmap-hash",		/* BPF_MAP_TYPE_DEVMAP_HASH */
	[26] = "struct-ops",		/* BPF_MAP_TYPE_STRUCT_OPS */
	[27] = "ringbuf",		/* BPF_MAP_TYPE_RINGBUF */
	[28] = "inode-storage", /* BPF_MAP_TYPE_INODE_STORAGE */
	[29] = "task-storage",	/* BPF_MAP_TYPE_TASK_STORAGE */
	[30] = "bloom-filter",	/* BPF_MAP_TYPE_BLOOM_FILTER */
	[31] = "user-ringbuf",	/* BPF_MAP_TYPE_USER_RINGBUF */
	[32] = "cgrp-storage",	/* BPF_MAP_TYPE_CGRP_STORAGE */
};

struct anon_bpf_map_data {
	int type;
	int id;
	char name[BPF_OBJ_NAME_LEN + 1];
};

static bool anon_bpf_map_probe(const char *str)
{
	return strncmp(str, "bpf-map", 8) == 0;
}

static const char *anon_bpf_map_get_map_type_name(int type)
{
	if (0 <= type && type < (int)ARRAY_SIZE(bpf_map_type_table))
		return bpf_map_type_table[type];
	return NULL;
}

static bool anon_bpf_map_fill_column(struct proc *proc  __attribute__((__unused__)),
				     struct unkn *unkn,
				     struct libscols_line *ln __attribute__((__unused__)),
				     int column_id,
				     size_t column_index __attribute__((__unused__)),
				     char **str)
{
	struct anon_bpf_prog_data *data = (struct anon_bpf_prog_data *)unkn->anon_data;
	const char *t;

	switch(column_id) {
	case COL_BPF_MAP_ID:
		xasprintf(str, "%d", data->id);
		return true;
	case COL_BPF_MAP_TYPE_RAW:
		xasprintf(str, "%d", data->type);
		return true;
	case COL_BPF_MAP_TYPE:
		t = anon_bpf_map_get_map_type_name(data->type);
		if (t)
			*str = xstrdup(t);
		else
			xasprintf(str, "UNKNOWN(%d)", data->type);
		return true;
	case COL_BPF_NAME:
		*str = xstrdup(data->name);
		return true;
	default:
		return false;
	}
}

static char *anon_bpf_map_get_name(struct unkn *unkn)
{
	const char *t;
	char *str = NULL;
	struct anon_bpf_map_data *data = (struct anon_bpf_map_data *)unkn->anon_data;

	t = anon_bpf_map_get_map_type_name(data->type);
	if (t)
		xasprintf(&str, "id=%d type=%s", data->id, t);
	else
		xasprintf(&str, "id=%d type=UNKNOWN(%d)", data->id, data->type);

	if (*data->name)
		xstrfappend(&str, " name=%s", data->name);

	return str;
}

static void anon_bpf_map_init(struct unkn *unkn)
{
	struct anon_bpf_map_data *data = xmalloc(sizeof(*data));
	data->type = -1;
	data->id = -1;
	data->name[0] = '\0';
	unkn->anon_data = data;
}

static void anon_bpf_map_free(struct unkn *unkn)
{
	struct anon_bpf_map_data *data = (struct anon_bpf_map_data *)unkn->anon_data;
	free(data);
}

static void anon_bpf_map_get_more_info(struct anon_bpf_map_data *map_data)
{
	union bpf_attr attr = {
		.map_id = (int32_t)map_data->id,
		.next_id = 0,
		.open_flags = 0,
	};
	struct bpf_map_info info = { 0 };
	union bpf_attr info_attr = {
		.info.info_len = sizeof(info),
		.info.info = (uint64_t)(uintptr_t)&info,
	};

	int bpf_fd = syscall(SYS_bpf, BPF_MAP_GET_FD_BY_ID, &attr, sizeof(attr));
	if (bpf_fd < 0)
		return;

	info_attr.info.bpf_fd = bpf_fd;
	if (syscall(SYS_bpf, BPF_OBJ_GET_INFO_BY_FD, &info_attr, offsetofend(union bpf_attr, info)) == 0) {
		memcpy(map_data->name,
		       info.name,
		       BPF_OBJ_NAME_LEN);
		map_data->name[BPF_OBJ_NAME_LEN] = '\0';
	}
	close(bpf_fd);
}

static int anon_bpf_map_handle_fdinfo(struct unkn *unkn, const char *key, const char *value)
{
	if (strcmp(key, "map_id") == 0) {
		int32_t t = -1;
		int rc = ul_strtos32(value, &t, 10);
		if (rc < 0)
			return 0; /* ignore -- parse failed */
		((struct anon_bpf_map_data *)unkn->anon_data)->id = (int)t;
		anon_bpf_map_get_more_info((struct anon_bpf_map_data *)unkn->anon_data);
		return 1;
	}

	if (strcmp(key, "map_type") == 0) {
		int32_t t = -1;
		int rc = ul_strtos32(value, &t, 10);
		if (rc < 0)
			return 0; /* ignore -- parse failed */
		((struct anon_bpf_map_data *)unkn->anon_data)->type = (int)t;
		return 1;
	}

	return 0;
}

static const struct anon_ops anon_bpf_map_ops = {
	.class = "bpf-map",
	.probe = anon_bpf_map_probe,
	.get_name = anon_bpf_map_get_name,
	.fill_column = anon_bpf_map_fill_column,
	.init = anon_bpf_map_init,
	.free = anon_bpf_map_free,
	.handle_fdinfo = anon_bpf_map_handle_fdinfo,
};

/*
 * generic (fallback implementation)
 */
static const struct anon_ops anon_generic_ops = {
	.class = NULL,
	.get_name = NULL,
	.fill_column = NULL,
	.init = NULL,
	.free = NULL,
	.handle_fdinfo = NULL,
};

static const struct anon_ops *anon_ops[] = {
	&anon_pidfd_ops,
	&anon_eventfd_ops,
	&anon_eventpoll_ops,
	&anon_timerfd_ops,
	&anon_signalfd_ops,
	&anon_inotify_ops,
	&anon_bpf_prog_ops,
	&anon_bpf_map_ops,
};

static const struct anon_ops *anon_probe(const char *str)
{
	for (size_t i = 0; i < ARRAY_SIZE(anon_ops); i++)
		if (anon_ops[i]->probe(str))
			return anon_ops[i];
	return &anon_generic_ops;
}

const struct file_class unkn_class = {
	.super = &file_class,
	.size = sizeof(struct unkn),
	.fill_column = unkn_fill_column,
	.initialize_content = unkn_init_content,
	.free_content = unkn_content_free,
	.handle_fdinfo = unkn_handle_fdinfo,
	.attach_xinfo = unkn_attach_xinfo,
	.get_ipc_class = unkn_get_ipc_class,
};
