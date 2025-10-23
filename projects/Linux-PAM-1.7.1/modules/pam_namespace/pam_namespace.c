/******************************************************************************
 * A module for Linux-PAM that will set the default namespace after
 * establishing a session via PAM.
 *
 * (C) Copyright IBM Corporation 2005
 * (C) Copyright Red Hat, Inc. 2006, 2008
 * All Rights Reserved.
 *
 * Written by: Janak Desai <janak@us.ibm.com>
 * With Revisions by: Steve Grubb <sgrubb@redhat.com>
 * Contributions by: Xavier Toth <txtoth@gmail.com>,
 *                   Tomas Mraz <tmraz@redhat.com>
 * Derived from a namespace setup patch by Chad Sellers <cdselle@tycho.nsa.gov>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
 * IBM AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#define _ATFILE_SOURCE

#include "config.h"
#include <stdint.h>
#include "pam_cc_compat.h"
#include "pam_inline.h"
#include "pam_namespace.h"
#include "argv_parse.h"

#define MAGIC_LNK_FD_SIZE 64

/* --- evaluating all files in VENDORDIR/security/namespace.d and /etc/security/namespace.d --- */
static const char *base_name(const char *path)
{
    const char *base = strrchr(path, '/');
    return base ? base+1 : path;
}

static int
compare_filename(const void *a, const void *b)
{
	return strcmp(base_name(* (char * const *) a),
		      base_name(* (char * const *) b));
}

static void close_fds_pre_exec(struct instance_data *idata)
{
	if (pam_modutil_sanitize_helper_fds(idata->pamh, PAM_MODUTIL_IGNORE_FD,
			PAM_MODUTIL_IGNORE_FD, PAM_MODUTIL_IGNORE_FD) < 0) {
		_exit(1);
	}
}

static void
strip_trailing_slashes(char *str)
{
	char *p = str + strlen(str);

	while (--p > str && *p == '/')
		*p = '\0';
}

static int protect_mount(int dfd, const char *path, struct instance_data *idata)
{
	struct protect_dir_s *dir = idata->protect_dirs;
	char tmpbuf[MAGIC_LNK_FD_SIZE];

	while (dir != NULL) {
		if (strcmp(path, dir->dir) == 0) {
			return 0;
		}
		dir = dir->next;
	}

	if (pam_sprintf(tmpbuf, "/proc/self/fd/%d", dfd) < 0)
		return -1;

	dir = calloc(1, sizeof(*dir));

	if (dir == NULL) {
		return -1;
	}

	dir->dir = strdup(path);

	if (dir->dir == NULL) {
		free(dir);
		return -1;
	}

	if (idata->flags & PAMNS_DEBUG) {
		pam_syslog(idata->pamh, LOG_INFO,
			"Protect mount of %s over itself", path);
	}

	if (mount(tmpbuf, tmpbuf, NULL, MS_BIND, NULL) != 0) {
		int save_errno = errno;
		pam_syslog(idata->pamh, LOG_ERR,
			"Protect mount of %s failed: %m", tmpbuf);
		free(dir->dir);
		free(dir);
		errno = save_errno;
		return -1;
	}

	dir->next = idata->protect_dirs;
	idata->protect_dirs = dir;

	return 0;
}

/*
 * Returns a fd to the given absolute path, acquired securely. This means:
 * - iterating on each segment of the path,
 * - not following user symlinks,
 * - using race-free operations.
 *
 * Takes a bit mask to specify the operation mode:
 * - SECURE_OPENDIR_PROTECT: call protect_mount() on each unsafe segment of path
 * - SECURE_OPENDIR_MKDIR: create last segment of path if does not exist
 * - SECURE_OPENDIR_FULL_FD: open the directory with O_RDONLY instead of O_PATH,
 *     allowing more operations to be done with the returned fd
 *
 * Be aware that using SECURE_OPENDIR_PROTECT:
 * - will modify some external state (global structure...) and should not be
 *   called in cleanup code paths. See wrapper secure_opendir_stateless()
 * - need a non-NULL idata to call protect_mount()
 */
static int secure_opendir(const char *path, int opm, mode_t mode,
	struct instance_data *idata)
{
	char *p;
	char *d;
	char *dir;
	int dfd = -1;
	int dfd_next;
	int save_errno;
	int flags = O_DIRECTORY | O_CLOEXEC;
	int rv = -1;
	struct stat st;

	if (opm & SECURE_OPENDIR_FULL_FD)
		flags |= O_RDONLY;
	else
		flags |= O_PATH;

	/* Check for args consistency */
	if ((opm & SECURE_OPENDIR_PROTECT) && idata == NULL)
		return -1;

	/* Accept only absolute paths */
	if (*path != '/')
		return -1;

	dir = p = strdup(path);
	if (p == NULL)
		return -1;

	/* Assume '/' is safe */
	dfd = open("/", flags);
	if (dfd == -1)
		goto error;

	/* Needed to not loop too far and call openat() on NULL */
	strip_trailing_slashes(p);

	dir++;

	/* In case path is '/' */
	if (*dir == '\0') {
		free(p);
		return dfd;
	}

	while ((d=strchr(dir, '/')) != NULL) {
		*d = '\0';

		dfd_next = openat(dfd, dir, flags);
		if (dfd_next == -1)
			goto error;

		if (fstat(dfd_next, &st) != 0) {
			close(dfd_next);
			goto error;
		}

		if ((flags & O_NOFOLLOW) && (opm & SECURE_OPENDIR_PROTECT)) {
			/* we are inside user-owned dir - protect */
			if (protect_mount(dfd_next, p, idata) == -1) {
				close(dfd_next);
				goto error;
			}
			/*
			 * Reopen the directory to obtain a new descriptor
			 * after protect_mount(), this is necessary in cases
			 * when another directory is going to be mounted over
			 * the given path.
			 */
			close(dfd_next);
			dfd_next = openat(dfd, dir, flags);
			if (dfd_next == -1)
				goto error;
		} else if (st.st_uid != 0
		           || (st.st_mode & (S_IWGRP|S_IWOTH))) {
			/* do not follow symlinks on subdirectories */
			flags |= O_NOFOLLOW;
		}

		close(dfd);
		dfd = dfd_next;

		*d = '/';
		dir = d + 1;
	}

	rv = openat(dfd, dir, flags);

	if (rv == -1) {
		if ((opm & SECURE_OPENDIR_MKDIR) && mkdirat(dfd, dir, mode) == 0)
			rv = openat(dfd, dir, flags);

		if (rv == -1)
			goto error;
	}

	if ((flags & O_NOFOLLOW) && (opm & SECURE_OPENDIR_PROTECT)) {
		/* we are inside user-owned dir - protect */
		if (protect_mount(rv, p, idata) == -1) {
			save_errno = errno;
			close(rv);
			rv = -1;
			errno = save_errno;
		}
		/*
		 * Reopen the directory to obtain a new descriptor after
		 * protect_mount(), this is necessary in cases when another
		 * directory is going to be mounted over the given path.
		 */
		close(rv);
		rv = openat(dfd, dir, flags);
	}

error:
	save_errno = errno;
	free(p);
	if (dfd >= 0)
		close(dfd);
	errno = save_errno;

	return rv;
}

/*
 * Returns a fd to the given path, acquired securely.
 * It can be called in all situations, including in cleanup code paths, as
 * it does not modify external state (no access to global structures...).
 */
static int secure_opendir_stateless(const char *path)
{
	return secure_opendir(path, 0, 0, NULL);
}

/*
 * Umount securely the given path, even if the directories along
 * the path are under user control. It should protect against
 * symlinks attacks and race conditions.
 */
static int secure_umount(const char *path)
{
	int save_errno;
	int rv = -1;
	int dfd = -1;
	char s_path[MAGIC_LNK_FD_SIZE];

	dfd = secure_opendir_stateless(path);
	if (dfd == -1)
		return rv;

	if (pam_sprintf(s_path, "/proc/self/fd/%d", dfd) < 0)
		goto error;

	/*
	* We still have a fd open to path itself,
	* so we need to do a lazy umount.
	*/
	rv = umount2(s_path, MNT_DETACH);

error:
	save_errno = errno;
	close(dfd);
	errno = save_errno;
	return rv;
}

/*
 * Rmdir the given path securely, protecting against symlinks attacks
 * and race conditions.
 * This function is currently called only in cleanup code paths where
 * any errors returned are not handled, so do not handle them either.
 * Basically, try to rmdir the path on a best-effort basis.
 */
static void secure_try_rmdir(const char *path)
{
	int dfd;
	char *buf;
	char *parent;

	buf = strdup(path);
	if (buf == NULL)
		return;

	parent = dirname(buf);

	dfd = secure_opendir_stateless(parent);
	if (dfd >= 0) {
		unlinkat(dfd, base_name(path), AT_REMOVEDIR);
		close(dfd);
	}

	free(buf);
}

/* Evaluating a list of files which have to be parsed in the right order:
 *
 * - If etc/security/namespace.d/@filename@.conf exists, then
 *   %vendordir%/security/namespace.d/@filename@.conf should not be used.
 * - All files in both namespace.d directories are sorted by their @filename@.conf in
 *   lexicographic order regardless of which of the directories they reside in. */
static char **read_namespace_dir(struct instance_data *idata)
{
	glob_t globbuf;
	size_t i=0;
	int glob_rv = glob(NAMESPACE_D_GLOB, GLOB_ERR | GLOB_NOSORT, NULL, &globbuf);
	char **file_list;
	size_t file_list_size = glob_rv == 0 ? globbuf.gl_pathc : 0;

#ifdef VENDOR_NAMESPACE_D_GLOB
	glob_t globbuf_vendor;
	int glob_rv_vendor = glob(VENDOR_NAMESPACE_D_GLOB, GLOB_ERR | GLOB_NOSORT, NULL, &globbuf_vendor);
	if (glob_rv_vendor == 0)
	    file_list_size += globbuf_vendor.gl_pathc;
#endif
	file_list = malloc((file_list_size + 1) * sizeof(char*));
	if (file_list == NULL) {
	    pam_syslog(idata->pamh, LOG_ERR, "Cannot allocate memory for file list: %m");
#ifdef VENDOR_NAMESPACE_D_GLOB
	    if (glob_rv_vendor == 0)
		globfree(&globbuf_vendor);
#endif
	    if (glob_rv == 0)
		globfree(&globbuf);
	    return NULL;
	}

	if (glob_rv == 0) {
	    for (i = 0; i < globbuf.gl_pathc; i++) {
		file_list[i] = strdup(globbuf.gl_pathv[i]);
		if (file_list[i] == NULL) {
		    pam_syslog(idata->pamh, LOG_ERR, "strdup failed: %m");
		    break;
		}
	    }
	}
#ifdef VENDOR_NAMESPACE_D_GLOB
	if (glob_rv_vendor == 0) {
	    for (size_t j = 0; j < globbuf_vendor.gl_pathc; j++) {
		if (glob_rv == 0 && globbuf.gl_pathc > 0) {
		    int double_found = 0;
		    for (size_t k = 0; k < globbuf.gl_pathc; k++) {
			if (strcmp(base_name(globbuf.gl_pathv[k]),
				   base_name(globbuf_vendor.gl_pathv[j])) == 0) {
				double_found = 1;
				break;
			}
		    }
		    if (double_found)
			continue;
		}
		file_list[i] = strdup(globbuf_vendor.gl_pathv[j]);
		if (file_list[i] == NULL) {
		    pam_syslog(idata->pamh, LOG_ERR, "strdup failed: %m");
		    break;
		}
		i++;
	    }
	    globfree(&globbuf_vendor);
	}
#endif
	file_list[i] = NULL;
	qsort(file_list, i, sizeof(char *), compare_filename);
	if (glob_rv == 0)
	    globfree(&globbuf);

	return file_list;
}

/*
 * Adds an entry for a polyinstantiated directory to the linked list of
 * polyinstantiated directories. It is called from process_line() while
 * parsing the namespace configuration file.
 */
static void add_polydir_entry(struct instance_data *idata,
	struct polydir_s *ent)
{
    /* Now attach to linked list */
    ent->next = NULL;
    if (idata->polydirs_ptr == NULL)
        idata->polydirs_ptr = ent;
    else {
        struct polydir_s *tail;

        tail = idata->polydirs_ptr;
        while (tail->next)
            tail = tail->next;
        tail->next = ent;
    }
}

static void del_polydir(struct polydir_s *poly)
{
	if (poly) {
		free(poly->uid);
		free(poly->init_script);
		free(poly->mount_opts);
		free(poly);
	}
}

/*
 * Deletes all the entries in the linked list.
 */
static void del_polydir_list(struct polydir_s *polydirs_ptr)
{
        struct polydir_s *dptr = polydirs_ptr;

	while (dptr) {
		struct polydir_s *tptr = dptr;
		dptr = dptr->next;
		del_polydir(tptr);
	}
}

static void unprotect_dirs(struct protect_dir_s *dir)
{
	struct protect_dir_s *next;

	while (dir != NULL) {
		secure_umount(dir->dir);
		free(dir->dir);
		next = dir->next;
		free(dir);
		dir = next;
	}
}

static void cleanup_polydir_data(pam_handle_t *pamh UNUSED , void *data, int err UNUSED)
{
	del_polydir_list(data);
}

static void cleanup_protect_data(pam_handle_t *pamh UNUSED , void *data, int err UNUSED)
{
	unprotect_dirs(data);
}

static char *expand_variables(const char *orig, const char *const var_names[], const char *var_values[])
{
	const char *src = orig;
	char *dst;
	char *expanded;
	char c;
	size_t dstlen = 0;
	while (*src) {
		if (*src == '$') {
			int i;
			for (i = 0; var_names[i]; i++) {
				size_t namelen = strlen(var_names[i]);
				if (strncmp(var_names[i], src+1, namelen) == 0) {
					dstlen += strlen(var_values[i]) - 1; /* $ */
					src += namelen;
					break;
				}
			}
		}
		++dstlen;
		++src;
	}
	if ((dst=expanded=malloc(dstlen + 1)) == NULL)
		return NULL;
	src = orig;
	while ((c=*src) != '\0') {
		if (c == '$') {
			int i;
			for (i = 0; var_names[i]; i++) {
				size_t namelen = strlen(var_names[i]);
				if (strncmp(var_names[i], src+1, namelen) == 0) {
					dst = stpcpy(dst, var_values[i]);
					--dst;
					c = *dst; /* replace $ */
					src += namelen;
					break;
				}
			}
		}
		*dst = c;
		++dst;
		++src;
	}
	*dst = '\0';
	return expanded;
}

static int parse_create_params(char *params, struct polydir_s *poly)
{
    char *next;
    struct passwd *pwd = NULL;
    struct group *grp;

    poly->mode = (mode_t)ULONG_MAX;
    poly->owner = (uid_t)ULONG_MAX;
    poly->group = (gid_t)ULONG_MAX;

    if (*params != '=')
	return 0;
    params++;

    next = strchr(params, ',');
    if (next != NULL) {
	*next = '\0';
	next++;
    }

    if (*params != '\0') {
	errno = 0;
	poly->mode = (mode_t)strtoul(params, NULL, 0);
	if (errno != 0) {
	    poly->mode = (mode_t)ULONG_MAX;
	}
    }

    params = next;
    if (params == NULL)
	return 0;
    next = strchr(params, ',');
    if (next != NULL) {
	*next = '\0';
	next++;
    }

    if (*params != '\0') {
	pwd = getpwnam(params); /* session modules are not reentrant */
	if (pwd == NULL)
	    return -1;
	poly->owner = pwd->pw_uid;
    }

    params = next;
    if (params == NULL || *params == '\0') {
	if (pwd != NULL)
	    poly->group = pwd->pw_gid;
	return 0;
    }
    grp = getgrnam(params);
    if (grp == NULL)
	return -1;
    poly->group = grp->gr_gid;

    return 0;
}

static int parse_iscript_params(char *params, struct polydir_s *poly)
{
    if (*params != '=')
	return 0;
    params++;

    if (*params != '\0') {
	if (*params != '/') { /* path is relative to NAMESPACE_D_DIR */
		poly->init_script = pam_asprintf("%s%s", NAMESPACE_D_DIR, params);
	} else {
		poly->init_script = strdup(params);
	}
	if (poly->init_script == NULL)
		return -1;
    }
    return 0;
}

struct mntflag {
    const char *name;
    size_t len;
    unsigned long flag;
};

#define LITERAL_AND_LEN(x) x, sizeof(x) - 1

static const struct mntflag mntflags[] = {
	{ LITERAL_AND_LEN("noexec"), MS_NOEXEC },
	{ LITERAL_AND_LEN("nosuid"), MS_NOSUID },
	{ LITERAL_AND_LEN("nodev"), MS_NODEV }
    };

static int filter_mntopts(const char *opts, char **filtered,
		unsigned long *mountflags)
{
    size_t origlen = strlen(opts);
    const char *end;
    char *dest;

    dest = *filtered = NULL;
    *mountflags = 0;

    if (origlen == 0)
	return 0;

    do {
	size_t len;
	unsigned int i;

	end = strchr(opts, ',');
	if (end == NULL) {
	    len = strlen(opts);
	} else {
	    len = end - opts;
	}

	for (i = 0; i < PAM_ARRAY_SIZE(mntflags); i++) {
	    if (mntflags[i].len != len)
		continue;
	    if (memcmp(mntflags[i].name, opts, len) == 0) {
		*mountflags |= mntflags[i].flag;
		opts = end;
		break;
	    }
	}

	if (opts != end) {
	    if (dest != NULL) {
		*dest = ',';
		++dest;
	    } else {
		dest = *filtered = calloc(1, origlen + 1);
		if (dest == NULL)
		    return -1;
	    }
	    memcpy(dest, opts, len);
	    dest += len;
	}

	opts = end + 1;
    } while (end != NULL);

    return 0;
}

static int parse_method(char *method, struct polydir_s *poly,
		struct instance_data *idata)
{
    enum polymethod pm;
    char *sptr = NULL;
    static const char *const method_names[] = { "user", "context", "level", "tmpdir",
	"tmpfs", NULL };
    static const char *const flag_names[] = { "create", "noinit", "iscript",
	"shared", "mntopts", NULL };
    static const unsigned int flag_values[] = { POLYDIR_CREATE, POLYDIR_NOINIT,
	POLYDIR_ISCRIPT, POLYDIR_SHARED, POLYDIR_MNTOPTS };
    int i;
    char *flag;

    method = strtok_r(method, ":", &sptr);
    pm = NONE;

    for (i = 0; method_names[i]; i++) {
	if (strcmp(method, method_names[i]) == 0) {
		pm = i + 1; /* 0 = NONE */
	}
    }

    if (pm == NONE) {
        pam_syslog(idata->pamh, LOG_NOTICE, "Unknown method");
        return -1;
    }

    poly->method = pm;

    while ((flag=strtok_r(NULL, ":", &sptr)) != NULL) {
	for (i = 0; flag_names[i]; i++) {
		size_t namelen = strlen(flag_names[i]);

		if (strncmp(flag, flag_names[i], namelen) == 0) {
			poly->flags |= flag_values[i];
			switch (flag_values[i]) {
			    case POLYDIR_CREATE:
				if (parse_create_params(flag+namelen, poly) != 0) {
				        pam_syslog(idata->pamh, LOG_CRIT, "Invalid create parameters");
					return -1;
				}
				break;

			    case POLYDIR_ISCRIPT:
				if (parse_iscript_params(flag+namelen, poly) != 0) {
				        pam_syslog(idata->pamh, LOG_CRIT, "Memory allocation error");
					return -1;
				};
				break;

			    case POLYDIR_MNTOPTS:
				if (flag[namelen] != '=')
					break;
				if (poly->method != TMPFS) {
					pam_syslog(idata->pamh, LOG_WARNING, "Mount options applicable only to tmpfs method");
					break;
				}
				free(poly->mount_opts); /* if duplicate mntopts specified */
				poly->mount_opts = NULL;
				if (filter_mntopts(flag+namelen+1, &poly->mount_opts, &poly->mount_flags) != 0) {
					pam_syslog(idata->pamh, LOG_CRIT, "Memory allocation error");
					return -1;
				}
				break;
			}
		}
	}
    }

    return 0;
}

/*
 * Called from parse_config_file, this function processes a single line
 * of the namespace configuration file. It skips over comments and incomplete
 * or malformed lines. It processes a valid line with information on
 * polyinstantiating a directory by populating appropriate fields of a
 * polyinstantiated directory structure and then calling add_polydir_entry to
 * add that entry to the linked list of polyinstantiated directories.
 */
static int process_line(char *line, const char *home, const char *rhome,
			struct instance_data *idata)
{
    char *dir = NULL, *instance_prefix = NULL, *rdir = NULL;
    const char *config_dir, *config_instance_prefix;
    char *method, *uids;
    char *tptr;
    struct polydir_s *poly;
    int retval = 0;
    char **config_options = NULL;
    static const char *const var_names[] = {"HOME", "USER", NULL};
    const char *var_values[] = {home, idata->user};
    const char *rvar_values[] = {rhome, idata->ruser};

    /*
     * skip the leading white space
     */
    while (*line && isspace((unsigned char)*line))
        line++;

    /*
     * Rip off the comments
     */
    tptr = strchr(line,'#');
    if (tptr)
        *tptr = '\0';

    /*
     * Rip off the newline char
     */
    tptr = strchr(line,'\n');
    if (tptr)
        *tptr = '\0';

    /*
     * Anything left ?
     */
    if (line[0] == 0)
        return 0;

    poly = calloc(1, sizeof(*poly));
    if (poly == NULL)
	goto erralloc;

    /*
     * Initialize and scan the five strings from the line from the
     * namespace configuration file.
     */
    retval = argv_parse(line, NULL, &config_options);
    if (retval != 0) {
        goto erralloc;
    }

    config_dir = config_options[0];
    if (config_dir == NULL) {
        pam_syslog(idata->pamh, LOG_NOTICE, "Invalid line missing polydir");
        goto skipping;
    }
    config_instance_prefix = config_options[1];
    if (config_instance_prefix == NULL) {
        pam_syslog(idata->pamh, LOG_NOTICE, "Invalid line missing instance_prefix");
        goto skipping;
    }
    method = config_options[2];
    if (method == NULL) {
        pam_syslog(idata->pamh, LOG_NOTICE, "Invalid line missing method");
        goto skipping;
    }

    /*
     * Only the uids field is allowed to be blank, to indicate no
     * override users for polyinstantiation of that directory. If
     * any of the other fields are blank, the line is incomplete so
     * skip it.
     */
    uids = config_options[3];

    /*
     * Expand $HOME and $USER in poly dir and instance dir prefix
     */
    if ((rdir = expand_variables(config_dir, var_names, rvar_values)) == NULL) {
	    goto erralloc;
    }

    if ((dir = expand_variables(config_dir, var_names, var_values)) == NULL) {
	    goto erralloc;
    }

    if ((instance_prefix = expand_variables(config_instance_prefix,
					    var_names, var_values)) == NULL) {
	    goto erralloc;
    }

    if (idata->flags & PAMNS_DEBUG) {
	    pam_syslog(idata->pamh, LOG_DEBUG, "Expanded polydir: '%s'", dir);
	    pam_syslog(idata->pamh, LOG_DEBUG, "Expanded ruser polydir: '%s'", rdir);
	    pam_syslog(idata->pamh, LOG_DEBUG, "Expanded instance prefix: '%s'", instance_prefix);
    }

    strip_trailing_slashes(dir);
    strip_trailing_slashes(rdir);

    if (dir[0] == '\0' || rdir[0] == '\0') {
	    pam_syslog(idata->pamh, LOG_NOTICE, "Invalid polydir");
	    goto skipping;
    }

    /*
     * Populate polyinstantiated directory structure with appropriate
     * pathnames and the method with which to polyinstantiate.
     */
    if (parse_method(method, poly, idata) != 0) {
	    goto skipping;
    }

    if (pam_sprintf(poly->dir, "%s", dir) < 0
	|| pam_sprintf(poly->rdir, "%s", rdir) < 0
	|| pam_sprintf(poly->instance_prefix, "%s", instance_prefix) < 0) {
	pam_syslog(idata->pamh, LOG_NOTICE, "Pathnames too long");
	goto skipping;
    }

    /*
     * Ensure that all pathnames are absolute path names.
     */
    if ((poly->dir[0] != '/') || (poly->method != TMPFS && poly->instance_prefix[0] != '/')) {
        pam_syslog(idata->pamh, LOG_NOTICE, "Pathnames must start with '/'");
        goto skipping;
    }
    if (strstr(dir, "..") || strstr(poly->instance_prefix, "..")) {
        pam_syslog(idata->pamh, LOG_NOTICE, "Pathnames must not contain '..'");
        goto skipping;
    }

    /*
     * If the line in namespace.conf for a directory to polyinstantiate
     * contains a list of override users (users for whom polyinstantiation
     * is not performed), read the user ids, convert names into uids, and
     * add to polyinstantiated directory structure.
     */
    if (uids) {
        uid_t *uidptr;
        const char *ustr, *sstr;
        size_t count, i;

	if (*uids == '~') {
		poly->flags |= POLYDIR_EXCLUSIVE;
		uids++;
	}
        for (count = 0, ustr = sstr = uids; sstr; ustr = sstr + 1, count++)
           sstr = strchr(ustr, ',');

        if (count > UINT_MAX || count > SIZE_MAX / sizeof(uid_t)) {
            pam_syslog(idata->pamh, LOG_ERR, "Too many uids encountered in configuration");
            goto skipping;
        }

        poly->num_uids = count;
        poly->uid = malloc(count * sizeof (uid_t));
        uidptr = poly->uid;
        if (uidptr == NULL) {
            goto erralloc;
        }

        ustr = uids;
        for (i = 0; i < count; i++) {
            struct passwd *pwd;

            tptr = strchr(ustr, ',');
            if (tptr)
                *tptr = '\0';

            pwd = pam_modutil_getpwnam(idata->pamh, ustr);
            if (pwd == NULL) {
		pam_syslog(idata->pamh, LOG_ERR, "Unknown user %s in configuration", ustr);
		poly->num_uids--;
            } else {
                *uidptr = pwd->pw_uid;
                uidptr++;
            }
            ustr = tptr + 1;
        }
    }

    /*
     * Add polyinstantiated directory structure to the linked list
     * of all polyinstantiated directory structures.
     */
    add_polydir_entry(idata, poly);

    goto out;

erralloc:
    pam_syslog(idata->pamh, LOG_CRIT, "Memory allocation error");

skipping:
    if (idata->flags & PAMNS_IGN_CONFIG_ERR)
        retval = 0;
    else
        retval = PAM_SERVICE_ERR;
    del_polydir(poly);
out:
    free(rdir);
    free(dir);
    free(instance_prefix);
    argv_free(config_options);
    return retval;
}


/*
 * Parses /etc/security/namespace.conf file to build a linked list of
 * polyinstantiated directory structures of type polydir_s. Each entry
 * in the linked list contains information needed to polyinstantiate
 * one directory.
 */
static int parse_config_file(struct instance_data *idata)
{
    FILE *fil;
    char *home, *rhome;
    const char *confname;
    struct passwd *cpwd;
    char *line;
    int retval;
    size_t len = 0;
    size_t n;

    /*
     * Extract the user's home directory to resolve $HOME entries
     * in the namespace configuration file.
     */
    cpwd = pam_modutil_getpwnam(idata->pamh, idata->user);
    if (!cpwd) {
        pam_syslog(idata->pamh, LOG_ERR,
               "Error getting home dir for '%s'", idata->user);
        return PAM_SESSION_ERR;
    }
    if ((home=strdup(cpwd->pw_dir)) == NULL) {
	pam_syslog(idata->pamh, LOG_CRIT,
		"Memory allocation error");
	return PAM_SESSION_ERR;
    }

    cpwd = pam_modutil_getpwnam(idata->pamh, idata->ruser);
    if (!cpwd) {
	pam_syslog(idata->pamh, LOG_ERR,
	        "Error getting home dir for '%s'", idata->ruser);
	free(home);
	return PAM_SESSION_ERR;
    }

    if ((rhome=strdup(cpwd->pw_dir)) == NULL) {
	pam_syslog(idata->pamh, LOG_CRIT,
		"Memory allocation error");
	free(home);
	return PAM_SESSION_ERR;
    }

    /*
     * Open configuration file, read one line at a time and call
     * process_line to process each line.
     */

    confname = PAM_NAMESPACE_CONFIG;
#ifdef VENDOR_PAM_NAMESPACE_CONFIG
    /* Check whether PAM_NAMESPACE_CONFIG file is available.
     * If it does not exist, fall back to VENDOR_PAM_NAMESPACE_CONFIG file. */
    struct stat buffer;
    if (stat(confname, &buffer) != 0 && errno == ENOENT) {
	confname = VENDOR_PAM_NAMESPACE_CONFIG;
    }
#endif
    char **filename_list = read_namespace_dir(idata);
    n = 0;
    for (;;) {
	if (idata->flags & PAMNS_DEBUG)
		pam_syslog(idata->pamh, LOG_DEBUG, "Parsing config file %s",
			confname);
	fil = fopen(confname, "r");
	if (fil == NULL) {
	    pam_syslog(idata->pamh, LOG_ERR, "Error opening config file %s",
		confname);
	    free(rhome);
	    free(home);
	    return PAM_SERVICE_ERR;
	}

	/* Use unlocked IO */
	__fsetlocking(fil, FSETLOCKING_BYCALLER);

	line = NULL;
	/* loop reading the file */
	while (getline(&line, &len, fil) > 0) {
	    retval = process_line(line, home, rhome, idata);
	    if (retval) {
		pam_syslog(idata->pamh, LOG_ERR,
		"Error processing conf file %s line %s", confname, line);
	        fclose(fil);
	        free(line);
	        free(rhome);
	        free(home);
	        return PAM_SERVICE_ERR;
	    }
	}
	fclose(fil);
	free(line);

	if (filename_list == NULL || filename_list[n] == NULL)
	    break;

	confname = filename_list[n++];
    }

    if (filename_list != NULL) {
	for (size_t i = 0; filename_list[i] != NULL; i++)
	    free(filename_list[i]);
	free(filename_list);
    }

    free(rhome);
    free(home);

    /* All done...just some debug stuff */
    if (idata->flags & PAMNS_DEBUG) {
        struct polydir_s *dptr = idata->polydirs_ptr;
        uid_t *iptr;
        uid_t i;

        pam_syslog(idata->pamh, LOG_DEBUG,
	    dptr?"Configured poly dirs:":"No configured poly dirs");
        while (dptr) {
            pam_syslog(idata->pamh, LOG_DEBUG, "dir='%s' iprefix='%s' meth=%d",
		   dptr->dir, dptr->instance_prefix, dptr->method);
            for (i = 0, iptr = dptr->uid; i < dptr->num_uids; i++, iptr++)
                pam_syslog(idata->pamh, LOG_DEBUG, "override user %d ", *iptr);
            dptr = dptr->next;
        }
    }

    return PAM_SUCCESS;
}


/*
 * This function returns true if a given uid is present in the polyinstantiated
 * directory's list of override uids. If the uid is one of the override
 * uids for the polyinstantiated directory, polyinstantiation is not
 * performed for that user for that directory.
 * If exclusive is set the returned values are opposite.
 */
static int ns_override(struct polydir_s *polyptr, struct instance_data *idata,
		uid_t uid)
{
    unsigned int i;

    if (idata->flags & PAMNS_DEBUG)
	pam_syslog(idata->pamh, LOG_DEBUG,
		"Checking for ns override in dir %s for uid %d",
		polyptr->dir, uid);

    for (i = 0; i < polyptr->num_uids; i++) {
        if (uid == polyptr->uid[i]) {
            return !(polyptr->flags & POLYDIR_EXCLUSIVE);
        }
    }

    return !!(polyptr->flags & POLYDIR_EXCLUSIVE);
}

/*
 * md5hash generates a hash of the passed in instance directory name.
 */
static char *md5hash(const char *instname, struct instance_data *idata)
{
    int i;
    char *md5inst = NULL;
    char *to;
    unsigned char inst_digest[MD5_DIGEST_LENGTH];

    /*
     * Create MD5 hashes for instance pathname.
     */

    MD5((const unsigned char *)instname, strlen(instname), inst_digest);

    if ((md5inst = malloc(MD5_DIGEST_LENGTH * 2 + 1)) == NULL) {
        pam_syslog(idata->pamh, LOG_CRIT, "Unable to allocate buffer");
        return NULL;
    }

    to = md5inst;
    for (i = 0; i < MD5_DIGEST_LENGTH; i++) {
        snprintf(to, 3, "%02x", (unsigned int)inst_digest[i]);
        to += 2;
    }

    return md5inst;
}

#ifdef WITH_SELINUX
static char *secure_getfilecon(pam_handle_t *pamh, const char *dir)
{
	char *ctx = NULL;
	int dfd = secure_opendir(dir, SECURE_OPENDIR_FULL_FD, 0, NULL);
	if (dfd < 0) {
		pam_syslog(pamh, LOG_ERR, "Error getting fd to %s: %m", dir);
		return NULL;
	}
	if (fgetfilecon(dfd, &ctx) < 0)
		ctx = NULL;
	if (ctx == NULL)
		pam_syslog(pamh, LOG_ERR,
			   "Error getting poly dir context for %s: %m", dir);
	close(dfd);
	return ctx;
}

static int form_context(const struct polydir_s *polyptr,
		char **i_context, char **origcon,
		struct instance_data *idata)
{
	int rc = PAM_SUCCESS;
	char *scon = NULL;
	security_class_t tclass;

	/*
	 * Get the security context of the directory to polyinstantiate.
	 */
	*origcon = secure_getfilecon(idata->pamh, polyptr->dir);
	if (*origcon == NULL)
		return PAM_SESSION_ERR;

	if (polyptr->method == USER) return PAM_SUCCESS;

	if (idata->flags & PAMNS_USE_CURRENT_CONTEXT) {
		rc = getcon(&scon);
	} else if (idata->flags & PAMNS_USE_DEFAULT_CONTEXT) {
		char *seuser = NULL, *level = NULL;

		if ((rc=getseuserbyname(idata->user, &seuser, &level)) == 0) {
			rc = get_default_context_with_level(seuser, level, NULL, &scon);
			free(seuser);
			free(level);
		}
	} else {
		rc = getexeccon(&scon);
	}
	if (rc < 0 || scon == NULL) {
		pam_syslog(idata->pamh, LOG_ERR,
			   "Error getting exec context, %m");
		return PAM_SESSION_ERR;
	}

	/*
	 * If polyinstantiating based on security context, get current
	 * process security context, get security class for directories,
	 * and ask the policy to provide security context of the
	 * polyinstantiated instance directory.
	 */

	if (polyptr->method == CONTEXT) {
		tclass = string_to_security_class("dir");
		if (tclass == 0) {
			pam_syslog(idata->pamh, LOG_ERR,
				   "Error getting dir security class");
			freecon(scon);
			return PAM_SESSION_ERR;
		}

		if (security_compute_member(scon, *origcon, tclass,
					i_context) < 0) {
			pam_syslog(idata->pamh, LOG_ERR,
					"Error computing poly dir member context");
			freecon(scon);
			return PAM_SESSION_ERR;
		} else if (idata->flags & PAMNS_DEBUG)
			pam_syslog(idata->pamh, LOG_DEBUG,
					"member context returned by policy %s", *i_context);
		freecon(scon);
		return PAM_SUCCESS;
	}

	/*
	 * If polyinstantiating based on security level, get current
	 * process security context, get security class for directories,
	 * and change the directories MLS Level to match process.
	 */

	if (polyptr->method == LEVEL) {
		context_t scontext = NULL;
		context_t fcontext = NULL;
		rc = PAM_SESSION_ERR;

		scontext = context_new(scon);
		if (! scontext) {
			pam_syslog(idata->pamh, LOG_CRIT, "out of memory");
			goto fail;
		}
		fcontext = context_new(*origcon);
		if (! fcontext) {
			pam_syslog(idata->pamh, LOG_CRIT, "out of memory");
			goto fail;
		}
		if (context_range_set(fcontext, context_range_get(scontext)) != 0) {
			pam_syslog(idata->pamh, LOG_ERR, "Unable to set MLS Component of context");
			goto fail;
		}
		*i_context=strdup(context_str(fcontext));
		if (! *i_context) {
			pam_syslog(idata->pamh, LOG_CRIT, "out of memory");
			goto fail;
		}

		rc = PAM_SUCCESS;
 fail:
		context_free(scontext);
		context_free(fcontext);
		freecon(scon);
		return rc;
	}
	/* Should never get here */
	freecon(scon);
	return PAM_SUCCESS;
}
#endif

/*
 * From the instance differentiation string, set in the polyptr structure:
 * - the absolute path to the instance dir,
 * - the absolute path to the previous dir (parent),
 * - the instance name (may be different than the instance differentiation string)
 */
static int set_polydir_paths(struct polydir_s *polyptr, const char *inst_differentiation)
{
	char *tmp;

	if (pam_sprintf(polyptr->instance_absolute, "%s%s",
	                polyptr->instance_prefix, inst_differentiation) < 0)
		return -1;

	polyptr->instname = strrchr(polyptr->instance_absolute, '/') + 1;

	if (pam_sprintf(polyptr->instance_parent, "%s", polyptr->instance_absolute) < 0)
		return -1;

	tmp = strrchr(polyptr->instance_parent, '/') + 1;
	*tmp = '\0';

	return 0;
}

/*
 * Set the name of the polyinstantiated instance directory
 * based on the method used for polyinstantiation (user, context or level)
 * In addition, the function also returns the security contexts of the
 * original directory to polyinstantiate and the polyinstantiated instance
 * directory.
 */
#ifdef WITH_SELINUX
static int poly_name(struct polydir_s *polyptr, char **i_context,
        char **origcon, struct instance_data *idata)
#else
static int poly_name(struct polydir_s *polyptr, struct instance_data *idata)
#endif
{
    int rc;
    char *inst_differentiation = NULL;
    char *hash = NULL;
    enum polymethod pm;
#ifdef WITH_SELINUX
    char *rawcon = NULL;
#endif

#ifdef WITH_SELINUX
    *i_context = NULL;
    *origcon = NULL;
    if ((idata->flags & PAMNS_SELINUX_ENABLED) &&
	(rc=form_context(polyptr, i_context, origcon, idata)) != PAM_SUCCESS) {
	    return rc;
    }
#endif

    rc = PAM_SESSION_ERR;
    /*
     * Set the name of the polyinstantiated instance dir based on the
     * polyinstantiation method.
     */

    pm = polyptr->method;
    if (pm == LEVEL || pm == CONTEXT)
#ifdef WITH_SELINUX
        if (!(idata->flags & PAMNS_CTXT_BASED_INST)) {
#else
    {
	pam_syslog(idata->pamh, LOG_NOTICE,
		"Context and level methods not available, using user method");
#endif
	if (polyptr->flags & POLYDIR_SHARED) {
		rc = PAM_IGNORE;
		goto fail;
	}
        pm = USER;
    }

    switch (pm) {
        case USER:
	    if ((inst_differentiation = strdup(idata->user)) == NULL)
		goto fail;
	    break;

#ifdef WITH_SELINUX
	case LEVEL:
        case CONTEXT:
	    if (selinux_trans_to_raw_context(*i_context, &rawcon) < 0) {
		pam_syslog(idata->pamh, LOG_ERR, "Error translating directory context");
		goto fail;
	    }
	    if (polyptr->flags & POLYDIR_SHARED)
		inst_differentiation = strdup(rawcon);
	    else
		inst_differentiation = pam_asprintf("%s_%s", rawcon, idata->user);
	    if (inst_differentiation == NULL)
		goto fail;
	    break;

#endif /* WITH_SELINUX */

	case TMPDIR:
	    if ((inst_differentiation = strdup("XXXXXX")) == NULL)
		goto fail;
	    goto success;

	case TMPFS:
	    if ((inst_differentiation=strdup("")) == NULL)
		goto fail;
	    goto success;

	default:
	    if (idata->flags & PAMNS_DEBUG)
	        pam_syslog(idata->pamh, LOG_ERR, "Unknown method");
	    goto fail;
    }

    if (idata->flags & PAMNS_DEBUG)
        pam_syslog(idata->pamh, LOG_DEBUG, "poly_name %s", inst_differentiation);

    if ((idata->flags & PAMNS_GEN_HASH) || strlen(inst_differentiation) > NAMESPACE_MAX_DIR_LEN) {
        hash = md5hash(inst_differentiation, idata);
        if (hash == NULL) {
	    goto fail;
        }
        if (idata->flags & PAMNS_GEN_HASH) {
	    free(inst_differentiation);
	    inst_differentiation = hash;
	    hash = NULL;
        } else {
	    char *newname =
		pam_asprintf("%.*s_%s",
			     NAMESPACE_MAX_DIR_LEN - 1 - (int)strlen(hash),
			     inst_differentiation, hash);
	    if (newname == NULL)
		goto fail;
	    free(inst_differentiation);
	    inst_differentiation = newname;
        }
    }

success:
    if (set_polydir_paths(polyptr, inst_differentiation) == -1)
        goto fail;

    rc = PAM_SUCCESS;
fail:
    free(hash);
    free(inst_differentiation);
#ifdef WITH_SELINUX
    freecon(rawcon);
#endif
    if (rc != PAM_SUCCESS) {
#ifdef WITH_SELINUX
	freecon(*i_context);
	*i_context = NULL;
	freecon(*origcon);
	*origcon = NULL;
#endif
    }
    return rc;
}

static int check_inst_parent(int dfd, struct instance_data *idata)
{
	struct stat instpbuf;

	/*
	 * Stat the instance parent directory to make sure it's writable by
	 * root only (unless the admin explicitly instructs to ignore the
	 * instance parent mode by the "ignore_instance_parent_mode" argument).
	 */

	if (idata->flags & PAMNS_IGN_INST_PARENT_MODE)
		return PAM_SUCCESS;

	if (fstat(dfd, &instpbuf) < 0) {
		pam_syslog(idata->pamh, LOG_ERR,
			"Error accessing instance parent, %m");
		return PAM_SESSION_ERR;
	}

	if ((instpbuf.st_mode & (S_IRWXU|S_IRWXG|S_IRWXO)) || instpbuf.st_uid != 0) {
		pam_syslog(idata->pamh, LOG_ERR,
			"Mode of inst parent not 000 or owner not root");
		return PAM_SESSION_ERR;
	}

	return PAM_SUCCESS;
}

/*
 * Check for a given absolute path that all segments except the last one are:
 * 1. a directory owned by root and not writable by group or others
 * 2. a symlink owned by root and referencing a directory respecting 1.
 * Returns 0 if safe, -1 is unsafe.
 * If the path is not accessible (does not exist, hidden under a mount...),
 * returns -1 (unsafe).
 */
static int check_safe_path(const char *path, struct instance_data *idata)
{
    char *p = strdup(path);
    char *d;
    char *dir = p;
    struct stat st;

    if (p == NULL)
        return -1;

    /* Check path is absolute */
    if (p[0] != '/')
        goto error;

    strip_trailing_slashes(p);

    /* Last segment of the path may be owned by the user */
    if ((d = strrchr(dir, '/')) != NULL)
        *d = '\0';

    while ((d=strrchr(dir, '/')) != NULL) {

        /* Do not follow symlinks */
        if (lstat(dir, &st) != 0)
            goto error;

        if (S_ISLNK(st.st_mode)) {
            if (st.st_uid != 0) {
                if (idata->flags & PAMNS_DEBUG)
                    pam_syslog(idata->pamh, LOG_DEBUG,
                            "Path deemed unsafe: Symlink %s should be owned by root", dir);
                goto error;
            }

            /* Follow symlinks */
            if (stat(dir, &st) != 0)
                goto error;
        }

        if (!S_ISDIR(st.st_mode)) {
                if (idata->flags & PAMNS_DEBUG)
                    pam_syslog(idata->pamh, LOG_DEBUG,
                        "Path deemed unsafe: %s is expected to be a directory", dir);
            goto error;
        }

        if (st.st_uid != 0 ||
            ((st.st_mode & (S_IWGRP|S_IWOTH)) && !(st.st_mode & S_ISVTX))) {
                if (idata->flags & PAMNS_DEBUG)
                    pam_syslog(idata->pamh, LOG_DEBUG,
                        "Path deemed unsafe: %s should be owned by root, and not be writable by group or others", dir);
            goto error;
        }

        *d = '\0';
    }

    free(p);
    return 0;

error:
    free(p);
    return -1;
}

/*
* Check to see if there is a namespace initialization script in
* the /etc/security directory. If such a script exists
* execute it and pass directory to polyinstantiate and instance
* directory as arguments.
*/
static int inst_init(const struct polydir_s *polyptr, const char *ipath,
	   struct instance_data *idata, int newdir)
{
	pid_t rc, pid;
	int status;
	const char *init_script = NAMESPACE_INIT_SCRIPT;

#ifdef VENDOR_NAMESPACE_INIT_SCRIPT
	/* Check whether NAMESPACE_INIT_SCRIPT file is available.
	 * If it does not exist, fall back to VENDOR_NAMESPACE_INIT_SCRIPT file. */
	struct stat buffer;
	if (stat(init_script, &buffer) != 0 && errno == ENOENT) {
		init_script = VENDOR_NAMESPACE_INIT_SCRIPT;
	}
#endif

	if ((polyptr->flags & POLYDIR_ISCRIPT) && polyptr->init_script)
		init_script = polyptr->init_script;

	if (access(init_script, F_OK) != 0)
		return PAM_SUCCESS;

	if (access(init_script, X_OK) < 0) {
		if (idata->flags & PAMNS_DEBUG)
			pam_syslog(idata->pamh, LOG_ERR,
					"Namespace init script not executable");
		return PAM_SESSION_ERR;
	}

	struct sigaction newsa, oldsa;

	memset(&newsa, '\0', sizeof(newsa));
	newsa.sa_handler = SIG_DFL;
	if (sigaction(SIGCHLD, &newsa, &oldsa) == -1) {
		pam_syslog(idata->pamh, LOG_ERR, "failed to reset SIGCHLD handler");
		return PAM_SESSION_ERR;
	}

	pid = fork();
	if (pid == 0) {
		static char *envp[] = { NULL };
#ifdef WITH_SELINUX
		if (idata->flags & PAMNS_SELINUX_ENABLED) {
			if (setexeccon(NULL) < 0)
				_exit(1);
		}
#endif
		/* Pass maximum privs when we exec() */
		if (setuid(geteuid()) < 0) {
			/* ignore failures, they don't matter */
		}

		close_fds_pre_exec(idata);

		execle(init_script, init_script,
		       polyptr->dir, ipath,
		       newdir ? "1":"0", idata->user,
		       (check_safe_path(polyptr->dir, idata) == -1) ? "0":"1",
		       (check_safe_path(ipath, idata) == -1) ? "0":"1",
		       NULL, envp);
		_exit(1);
	} else if (pid > 0) {
		while (((rc = waitpid(pid, &status, 0)) == (pid_t)-1) &&
				(errno == EINTR));
		if (rc == (pid_t)-1) {
			pam_syslog(idata->pamh, LOG_ERR, "waitpid failed- %m");
			rc = PAM_SESSION_ERR;
			goto out;
		}
		if (!WIFEXITED(status) || WIFSIGNALED(status) > 0) {
			pam_syslog(idata->pamh, LOG_ERR,
					"Error initializing instance");
			rc = PAM_SESSION_ERR;
			goto out;
		}
	} else if (pid < 0) {
		pam_syslog(idata->pamh, LOG_ERR,
				"Cannot fork to run namespace init script, %m");
		rc = PAM_SESSION_ERR;
		goto out;
	}
	rc = PAM_SUCCESS;
out:
	(void) sigaction(SIGCHLD, &oldsa, NULL);
	return rc;
}

static int create_polydir(struct polydir_s *polyptr,
	struct instance_data *idata)
{
    mode_t mode;
    int rc;
#ifdef WITH_SELINUX
    char *dircon_raw, *oldcon_raw = NULL;
    struct selabel_handle *label_handle;
#endif
    const char *dir = polyptr->dir;
    uid_t uid;
    gid_t gid;

    if (polyptr->mode != (mode_t)ULONG_MAX)
            mode = polyptr->mode;
    else
            mode = 0777;

#ifdef WITH_SELINUX
    if (idata->flags & PAMNS_SELINUX_ENABLED) {
	if (getfscreatecon_raw(&oldcon_raw) != 0)
	    pam_syslog(idata->pamh, LOG_NOTICE,
	               "Error retrieving fs create context: %m");

	label_handle = selabel_open(SELABEL_CTX_FILE, NULL, 0);
	if (!label_handle) {
	    pam_syslog(idata->pamh, LOG_NOTICE,
                       "Unable to initialize SELinux labeling handle: %m");
	} else {
	    rc = selabel_lookup_raw(label_handle, &dircon_raw, dir, S_IFDIR);
	    if (rc) {
		pam_syslog(idata->pamh, LOG_NOTICE,
                       "Unable to get default context for directory %s, check your policy: %m", dir);
	    } else {
		if (idata->flags & PAMNS_DEBUG)
		    pam_syslog(idata->pamh, LOG_DEBUG,
                               "Polydir %s context: %s", dir, dircon_raw);
		if (setfscreatecon_raw(dircon_raw) != 0)
		    pam_syslog(idata->pamh, LOG_NOTICE,
                               "Error setting context for directory %s: %m", dir);
		freecon(dircon_raw);
	    }
	    selabel_close(label_handle);
	}
    }
#endif

    rc = secure_opendir(dir,
            SECURE_OPENDIR_PROTECT | SECURE_OPENDIR_MKDIR | SECURE_OPENDIR_FULL_FD,
            mode, idata);
    if (rc == -1) {
            pam_syslog(idata->pamh, LOG_ERR,
                       "Error creating directory %s: %m", dir);
#ifdef WITH_SELINUX
            freecon(oldcon_raw);
#endif
            return -1;
    }

#ifdef WITH_SELINUX
    if (idata->flags & PAMNS_SELINUX_ENABLED) {
        if (setfscreatecon_raw(oldcon_raw) != 0)
		pam_syslog(idata->pamh, LOG_NOTICE,
                       "Error resetting fs create context: %m");
        freecon(oldcon_raw);
    }
#endif

    if (idata->flags & PAMNS_DEBUG)
            pam_syslog(idata->pamh, LOG_DEBUG, "Created polydir %s", dir);

    if (polyptr->mode != (mode_t)ULONG_MAX) {
	/* explicit mode requested */
	if (fchmod(rc, mode) != 0) {
		pam_syslog(idata->pamh, LOG_ERR,
			   "Error changing mode of directory %s: %m", dir);
                close(rc);
		secure_umount(dir); /* undo the eventual protection bind mount */
		secure_try_rmdir(dir);
		return -1;
	}
    }

    if (polyptr->owner != (uid_t)ULONG_MAX)
	uid = polyptr->owner;
    else
	uid = idata->uid;

    if (polyptr->group != (gid_t)ULONG_MAX)
	gid = polyptr->group;
    else
	gid = idata->gid;

    if (fchown(rc, uid, gid) != 0) {
        pam_syslog(idata->pamh, LOG_ERR,
                   "Unable to change owner on directory %s: %m", dir);
        close(rc);
	secure_umount(dir); /* undo the eventual protection bind mount */
	secure_try_rmdir(dir);
	return -1;
    }

    if (idata->flags & PAMNS_DEBUG)
	pam_syslog(idata->pamh, LOG_DEBUG,
	           "Polydir owner %u group %u", uid, gid);

    return rc;
}

/*
 * Create polyinstantiated instance directory.
 * To protect against races, changes are done on a fd to the parent of the
 * instance directory (dfd_iparent) and a relative path (polyptr->instname).
 * The absolute path (polyptr->instance_absolute) is only updated when creating
 * a tmpdir and used for logging purposes.
 */
#ifdef WITH_SELINUX
static int create_instance(struct polydir_s *polyptr, int dfd_iparent,
        struct stat *statbuf, const char *icontext, const char *ocontext,
        struct instance_data *idata)
#else
static int create_instance(struct polydir_s *polyptr, int dfd_iparent,
        struct stat *statbuf, struct instance_data *idata)
#endif
{
    struct stat newstatbuf;
    int fd;

    /*
     * Create instance directory and set its security context to the context
     * returned by the security policy. Set its mode and ownership
     * attributes to match that of the original directory that is being
     * polyinstantiated.
     */

    if (polyptr->method == TMPDIR) {
        char s_path[PATH_MAX];
        /*
         * Create the template for mkdtemp() as a magic link based on
         * our existing fd to avoid symlink attacks and races.
         */
        if (pam_sprintf(s_path, "/proc/self/fd/%d/%s", dfd_iparent, polyptr->instname) < 0
            || mkdtemp(s_path) == NULL) {
            pam_syslog(idata->pamh, LOG_ERR,
                       "Error creating temporary instance dir %s, %m",
                       polyptr->instance_absolute);
            polyptr->method = NONE; /* do not clean up! */
            return PAM_SESSION_ERR;
        }

        /* Copy the actual directory name to polyptr->instname */
        strcpy(polyptr->instname, base_name(s_path));
    } else if (mkdirat(dfd_iparent, polyptr->instname, S_IRUSR) < 0) {
        if (errno == EEXIST)
            return PAM_IGNORE;
        else {
            pam_syslog(idata->pamh, LOG_ERR, "Error creating %s, %m",
                       polyptr->instance_absolute);
            return PAM_SESSION_ERR;
        }
    }

    /* Open a descriptor to prevent races, based on our existing fd. */
    fd = openat(dfd_iparent, polyptr->instname,
            O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
	pam_syslog(idata->pamh, LOG_ERR, "Error opening %s, %m",
                polyptr->instance_absolute);
	unlinkat(dfd_iparent, polyptr->instname, AT_REMOVEDIR);
	return PAM_SESSION_ERR;
    }
#ifdef WITH_SELINUX
    /* If SE Linux is disabled, no need to label it */
    if (idata->flags & PAMNS_SELINUX_ENABLED) {
        /* If method is USER, icontext is NULL */
        if (icontext) {
            if (fsetfilecon(fd, icontext) < 0) {
                pam_syslog(idata->pamh, LOG_ERR,
			"Error setting context of %s to %s",
                        polyptr->instance_absolute, icontext);
                close(fd);
                unlinkat(dfd_iparent, polyptr->instname, AT_REMOVEDIR);
                return PAM_SESSION_ERR;
            }
        } else {
            if (fsetfilecon(fd, ocontext) < 0) {
                pam_syslog(idata->pamh, LOG_ERR,
			"Error setting context of %s to %s",
                        polyptr->instance_absolute, ocontext);
		close(fd);
                unlinkat(dfd_iparent, polyptr->instname, AT_REMOVEDIR);
                return PAM_SESSION_ERR;
            }
        }
    }
#endif
    if (fstat(fd, &newstatbuf) < 0) {
        pam_syslog(idata->pamh, LOG_ERR, "Error stating %s, %m",
		polyptr->instance_absolute);
	close(fd);
        unlinkat(dfd_iparent, polyptr->instname, AT_REMOVEDIR);
        return PAM_SESSION_ERR;
    }
    if (newstatbuf.st_uid != statbuf->st_uid ||
			 newstatbuf.st_gid != statbuf->st_gid) {
        if (fchown(fd, statbuf->st_uid, statbuf->st_gid) < 0) {
            pam_syslog(idata->pamh, LOG_ERR,
			"Error changing owner for %s, %m",
			polyptr->instance_absolute);
	    close(fd);
            unlinkat(dfd_iparent, polyptr->instname, AT_REMOVEDIR);
            return PAM_SESSION_ERR;
        }
    }
    if (fchmod(fd, statbuf->st_mode & 07777) < 0) {
        pam_syslog(idata->pamh, LOG_ERR, "Error changing mode for %s, %m",
			polyptr->instance_absolute);
	close(fd);
        unlinkat(dfd_iparent, polyptr->instname, AT_REMOVEDIR);
        return PAM_SESSION_ERR;
    }
    close(fd);
    return PAM_SUCCESS;
}


/*
 * This function performs the namespace setup for a particular directory
 * that is being polyinstantiated. It calls poly_name to create name of instance
 * directory, calls create_instance to mkdir it with appropriate
 * security attributes, and performs bind mount to setup the process
 * namespace.
 */
static int ns_setup(struct polydir_s *polyptr,
	struct instance_data *idata)
{
    int retval;
    int dfd_iparent = -1;
    int dfd_ipath = -1;
    int dfd_pptrdir = -1;
    int newdir = 1;
    char s_ipath[MAGIC_LNK_FD_SIZE];
    char s_pptrdir[MAGIC_LNK_FD_SIZE];
    struct stat statbuf;
#ifdef WITH_SELINUX
    char *instcontext = NULL, *origcontext = NULL;
#endif

    if (idata->flags & PAMNS_DEBUG)
        pam_syslog(idata->pamh, LOG_DEBUG,
               "Set namespace for directory %s", polyptr->dir);

    dfd_pptrdir = secure_opendir(polyptr->dir, SECURE_OPENDIR_PROTECT, 0, idata);

    if (dfd_pptrdir < 0) {
        if (errno != ENOENT || !(polyptr->flags & POLYDIR_CREATE)) {
            pam_syslog(idata->pamh, LOG_ERR, "Polydir %s access error: %m",
                    polyptr->dir);
            return PAM_SESSION_ERR;
        }
        dfd_pptrdir = create_polydir(polyptr, idata);
        if (dfd_pptrdir < 0)
            return PAM_SESSION_ERR;
    }

    if (polyptr->method == TMPFS) {
        /*
         * There is no function mount() that operate on a fd, so instead, we
         * get the magic link corresponding to the fd and give it to mount().
         * This protects against potential races exploitable by an unpriv user.
         */
        if (pam_sprintf(s_pptrdir, "/proc/self/fd/%d", dfd_pptrdir) < 0) {
            pam_syslog(idata->pamh, LOG_ERR, "Error pam_sprintf s_pptrdir");
            goto error_out;
        }

        if (mount("tmpfs", s_pptrdir, "tmpfs", polyptr->mount_flags, polyptr->mount_opts) < 0) {
            pam_syslog(idata->pamh, LOG_ERR, "Error mounting tmpfs on %s, %m",
                       polyptr->dir);
            goto error_out;
        }

        if (polyptr->flags & POLYDIR_NOINIT) {
            retval = PAM_SUCCESS;
            goto cleanup;
        }

        retval = inst_init(polyptr, "tmpfs", idata, 1);
        goto cleanup;
    }

    if (fstat(dfd_pptrdir, &statbuf) < 0) {
        pam_syslog(idata->pamh, LOG_ERR, "Error stating %s: %m", polyptr->dir);
        goto error_out;
    }

    /*
     * Obtain the name of instance pathname based on the
     * polyinstantiation method and instance context returned by
     * security policy.
     */
#ifdef WITH_SELINUX
    retval = poly_name(polyptr, &instcontext, &origcontext, idata);
#else
    retval = poly_name(polyptr, idata);
#endif

    if (retval != PAM_SUCCESS) {
	if (retval != PAM_IGNORE) {
		pam_syslog(idata->pamh, LOG_ERR, "Error getting instance name");
                goto error_out;
        }
        goto cleanup;
    } else {
#ifdef WITH_SELINUX
        if ((idata->flags & PAMNS_DEBUG) &&
            (idata->flags & PAMNS_SELINUX_ENABLED))
            pam_syslog(idata->pamh, LOG_DEBUG, "Inst ctxt %s Orig ctxt %s",
		 instcontext, origcontext);
#endif
    }

    /*
     * Gets a fd in a secure manner (we may be operating on a path under
     * user control), and check it's compliant.
     * Then, we should *always* operate on *this* fd and a relative path
     * to be protected against race conditions.
     */
    dfd_iparent = secure_opendir(polyptr->instance_parent,
            SECURE_OPENDIR_PROTECT | SECURE_OPENDIR_MKDIR, 0, idata);
    if (dfd_iparent == -1) {
	pam_syslog(idata->pamh, LOG_ERR,
                "polyptr->instance_parent %s access error",
                polyptr->instance_parent);
        goto error_out;
    }
    if (check_inst_parent(dfd_iparent, idata)) {
        goto error_out;
    }

    /*
     * Create instance directory with appropriate security
     * contexts, owner, group and mode bits.
     */
#ifdef WITH_SELINUX
    retval = create_instance(polyptr, dfd_iparent, &statbuf, instcontext,
                             origcontext, idata);
#else
    retval = create_instance(polyptr, dfd_iparent, &statbuf, idata);
#endif

    if (retval == PAM_IGNORE) {
	newdir = 0;
	retval = PAM_SUCCESS;
    }

    if (retval != PAM_SUCCESS) {
        goto error_out;
    }

    /*
     * Instead of getting a new secure fd, we reuse the fd opened on directory
     * polyptr->instance_parent to ensure we are working on the same dir as
     * previously, and thus ensure that previous checks (e.g. check_inst_parent())
     * are still relevant.
     */
    dfd_ipath = openat(dfd_iparent, polyptr->instname,
            O_PATH | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (dfd_ipath == -1) {
        pam_syslog(idata->pamh, LOG_ERR, "Error openat on %s, %m",
                polyptr->instname);
        goto error_out;
    }

    if (pam_sprintf(s_ipath, "/proc/self/fd/%d", dfd_ipath) < 0) {
        pam_syslog(idata->pamh, LOG_ERR, "Error pam_sprintf s_ipath");
        goto error_out;
    }

    if (pam_sprintf(s_pptrdir, "/proc/self/fd/%d", dfd_pptrdir) < 0) {
        pam_syslog(idata->pamh, LOG_ERR, "Error pam_sprintf s_pptrdir");
        goto error_out;
    }

    /*
     * Bind mount instance directory on top of the polyinstantiated
     * directory to provide an instance of polyinstantiated directory
     * based on polyinstantiated method.
     *
     * Operates on magic links created from two fd obtained securely
     * to protect against race conditions and symlink attacks. Indeed,
     * the source and destination can be in a user controled path.
     */
    if(mount(s_ipath, s_pptrdir, NULL, MS_BIND, NULL) < 0) {
        pam_syslog(idata->pamh, LOG_ERR,
                "Error mounting %s on %s (%s on %s), %m",
                   s_ipath, s_pptrdir, polyptr->instance_absolute, polyptr->dir);
        goto error_out;
    }

    if (!(polyptr->flags & POLYDIR_NOINIT))
	retval = inst_init(polyptr, polyptr->instance_absolute, idata, newdir);

    goto cleanup;

    /*
     * various error exit points. Free allocated memory and set return
     * value to indicate a pam session error.
     */
error_out:
    retval = PAM_SESSION_ERR;

cleanup:
    if (dfd_iparent != -1)
        close(dfd_iparent);
    if (dfd_ipath != -1)
        close(dfd_ipath);
    if (dfd_pptrdir != -1)
        close(dfd_pptrdir);
#ifdef WITH_SELINUX
    freecon(instcontext);
    freecon(origcontext);
#endif
    return retval;
}


/*
 * This function checks to see if the current working directory is
 * inside the directory passed in as the first argument.
 */
static int cwd_in(char *dir, struct instance_data *idata)
{
    int retval = 0;
    char cwd[PATH_MAX];

    if (getcwd(cwd, PATH_MAX) == NULL) {
        pam_syslog(idata->pamh, LOG_ERR, "Can't get current dir, %m");
        return -1;
    }

    if (strncmp(cwd, dir, strlen(dir)) == 0) {
        if (idata->flags & PAMNS_DEBUG)
            pam_syslog(idata->pamh, LOG_DEBUG, "cwd is inside %s", dir);
        retval = 1;
    } else {
        if (idata->flags & PAMNS_DEBUG)
            pam_syslog(idata->pamh, LOG_DEBUG, "cwd is outside %s", dir);
    }

    return retval;
}

static int cleanup_tmpdirs(struct instance_data *idata)
{
    struct polydir_s *pptr;
    pid_t rc, pid;
    int dfd = -1;
    struct sigaction newsa, oldsa;
    int status;

    memset(&newsa, '\0', sizeof(newsa));
    newsa.sa_handler = SIG_DFL;
    if (sigaction(SIGCHLD, &newsa, &oldsa) == -1) {
	pam_syslog(idata->pamh, LOG_ERR, "Cannot set signal value");
	return PAM_SESSION_ERR;
    }

    for (pptr = idata->polydirs_ptr; pptr; pptr = pptr->next) {
	if (pptr->method == TMPDIR) {

            dfd = secure_opendir_stateless(pptr->instance_parent);
            if (dfd == -1)
                continue;

            if (faccessat(dfd, pptr->instname, F_OK, AT_SYMLINK_NOFOLLOW) != 0) {
                close(dfd);
                continue;
            }

	    pid = fork();
	    if (pid == 0) {
		static char *envp[] = { NULL };
#ifdef WITH_SELINUX
		if (idata->flags & PAMNS_SELINUX_ENABLED) {
		    if (setexeccon(NULL) < 0)
			_exit(1);
		}
#endif
                if (fchdir(dfd) == -1) {
                    pam_syslog(idata->pamh, LOG_ERR, "Failed fchdir to %s: %m",
                            pptr->instance_absolute);
                    _exit(1);
                }

		close_fds_pre_exec(idata);

		execle("/bin/rm", "/bin/rm", "-rf", pptr->instname, NULL, envp);
		_exit(1);
	    } else if (pid > 0) {

                if (dfd != -1)
                    close(dfd);

		while (((rc = waitpid(pid, &status, 0)) == (pid_t)-1) &&
		    (errno == EINTR));
		if (rc == (pid_t)-1) {
		    pam_syslog(idata->pamh, LOG_ERR, "waitpid failed: %m");
		    rc = PAM_SESSION_ERR;
		    goto out;
		}
		if (!WIFEXITED(status) || WIFSIGNALED(status) > 0) {
		    pam_syslog(idata->pamh, LOG_ERR,
			"Error removing %s", pptr->instance_prefix);
		}
	    } else if (pid < 0) {

                if (dfd != -1)
                    close(dfd);

		pam_syslog(idata->pamh, LOG_ERR,
			"Cannot fork to cleanup temporary directory, %m");
		rc = PAM_SESSION_ERR;
		goto out;
	    }
        }
    }

    rc = PAM_SUCCESS;
out:
    sigaction(SIGCHLD, &oldsa, NULL);
    return rc;
}

/*
 * This function checks to see if polyinstantiation is needed for any
 * of the directories listed in the configuration file. If needed,
 * cycles through all polyinstantiated directory entries and calls
 * ns_setup to setup polyinstantiation for each one of them.
 */
static int setup_namespace(struct instance_data *idata, enum unmnt_op unmnt)
{
    int retval = 0, need_poly = 0, changing_dir = 0;
    int dfd = -1;
    char *cptr, *fptr, poly_parent[PATH_MAX];
    struct polydir_s *pptr;

    if (idata->flags & PAMNS_DEBUG)
        pam_syslog(idata->pamh, LOG_DEBUG, "Set up namespace for pid %d",
		getpid());

    /*
     * Cycle through all polyinstantiated directory entries to see if
     * polyinstantiation is needed at all.
     */
    for (pptr = idata->polydirs_ptr; pptr; pptr = pptr->next) {
        if (ns_override(pptr, idata, idata->uid)) {
	    if (unmnt == NO_UNMNT || ns_override(pptr, idata, idata->ruid)) {
		if (idata->flags & PAMNS_DEBUG)
		    pam_syslog(idata->pamh, LOG_DEBUG,
			"Overriding poly for user %d for dir %s",
			idata->uid, pptr->dir);
	    } else {
		if (idata->flags & PAMNS_DEBUG)
		    pam_syslog(idata->pamh, LOG_DEBUG,
			"Need unmount ns for user %d for dir %s",
			idata->ruid, pptr->dir);
		need_poly = 1;
		break;
	    }
            continue;
        } else {
            if (idata->flags & PAMNS_DEBUG)
                pam_syslog(idata->pamh, LOG_DEBUG,
			"Need poly ns for user %d for dir %s",
			idata->uid, pptr->dir);
            need_poly = 1;
            break;
        }
    }

    /*
     * If polyinstantiation is needed, call the unshare system call to
     * disassociate from the parent namespace.
     */
    if (need_poly) {
        if (unshare(CLONE_NEWNS) < 0) {
		pam_syslog(idata->pamh, LOG_ERR,
		"Unable to unshare from parent namespace, %m");
            return PAM_SESSION_ERR;
        }
	if (idata->flags & PAMNS_MOUNT_PRIVATE) {
	    /*
	     * Remount / as SLAVE so that nothing mounted in the namespace
	     * shows up in the parent
	     */
	    if (mount("/", "/", NULL, MS_SLAVE | MS_REC , NULL) < 0) {
		pam_syslog(idata->pamh, LOG_ERR,
			"Failed to mark / as a slave mount point, %m");
		return PAM_SESSION_ERR;
	    }
	    if (idata->flags & PAMNS_DEBUG)
		pam_syslog(idata->pamh, LOG_DEBUG,
			"The / mount point was marked as slave");
	}
    } else {
	del_polydir_list(idata->polydirs_ptr);
        return PAM_SUCCESS;
    }

    /*
     * Again cycle through all polyinstantiated directories, this time,
     * call ns_setup to setup polyinstantiation for a particular entry.
     */
    for (pptr = idata->polydirs_ptr; pptr; pptr = pptr->next) {
	enum unmnt_op dir_unmnt = unmnt;

	if (ns_override(pptr, idata, idata->ruid)) {
	    dir_unmnt = NO_UNMNT;
	}
	if (ns_override(pptr, idata, idata->uid)) {
	    if (dir_unmnt == NO_UNMNT) {
		continue;
	    } else {
		dir_unmnt = UNMNT_ONLY;
	    }
	}

	if (idata->flags & PAMNS_DEBUG)
                pam_syslog(idata->pamh, LOG_DEBUG,
			"Setting poly ns for user %d for dir %s",
                      idata->uid, pptr->dir);

	if ((dir_unmnt == UNMNT_REMNT) || (dir_unmnt == UNMNT_ONLY)) {
                /*
                 * Check to see if process current directory is in the
                 * bind mounted instance_parent directory that we are trying to
                 * umount
                 */
                if ((changing_dir = cwd_in(pptr->rdir, idata)) < 0) {
                    retval = PAM_SESSION_ERR;
                    goto out;
                } else if (changing_dir) {
                    if (idata->flags & PAMNS_DEBUG)
                        pam_syslog(idata->pamh, LOG_DEBUG, "changing cwd");

                    /*
                     * Change current working directory to the parent of
                     * the mount point, that is parent of the orig
                     * directory where original contents of the polydir
                     * are available from
                     */
                    strcpy(poly_parent, pptr->rdir);
		    fptr = strchr(poly_parent, '/');
		    cptr = strrchr(poly_parent, '/');
		    if (fptr && cptr && (fptr == cptr))
			strcpy(poly_parent, "/");
		    else if (cptr)
			*cptr = '\0';

                    dfd = secure_opendir_stateless(poly_parent);
                    if (dfd == -1) {
                        pam_syslog(idata->pamh, LOG_ERR,
                            "Failed opening %s to fchdir: %m", poly_parent);
                    }
                    else if (fchdir(dfd) == -1) {
                        pam_syslog(idata->pamh, LOG_ERR,
                            "Failed fchdir to %s: %m", poly_parent);
                    }
                    if (dfd != -1)
                        close(dfd);
                }

            if (secure_umount(pptr->rdir) < 0) {
		    int saved_errno = errno;
		    pam_syslog(idata->pamh, LOG_ERR, "Unmount of %s failed, %m",
			pptr->rdir);
		    if (saved_errno != EINVAL) {
			retval = PAM_SESSION_ERR;
			goto out;
                    }
                } else if (idata->flags & PAMNS_DEBUG)
                    pam_syslog(idata->pamh, LOG_DEBUG, "Umount succeeded %s",
				pptr->rdir);
	}

	if (dir_unmnt != UNMNT_ONLY) {
                retval = ns_setup(pptr, idata);
                if (retval == PAM_IGNORE)
                     retval = PAM_SUCCESS;
                if (retval != PAM_SUCCESS)
                     break;
        }
    }
out:
    if (retval != PAM_SUCCESS) {
	cleanup_tmpdirs(idata);
	unprotect_dirs(idata->protect_dirs);
    } else if (pam_set_data(idata->pamh, NAMESPACE_PROTECT_DATA, idata->protect_dirs,
		cleanup_protect_data) != PAM_SUCCESS) {
	pam_syslog(idata->pamh, LOG_ERR, "Unable to set namespace protect data");
	cleanup_tmpdirs(idata);
	unprotect_dirs(idata->protect_dirs);
	return PAM_SYSTEM_ERR;
    } else if (pam_set_data(idata->pamh, NAMESPACE_POLYDIR_DATA, idata->polydirs_ptr,
		cleanup_polydir_data) != PAM_SUCCESS) {
	pam_syslog(idata->pamh, LOG_ERR, "Unable to set namespace polydir data");
	cleanup_tmpdirs(idata);
	pam_set_data(idata->pamh, NAMESPACE_PROTECT_DATA, NULL, NULL);
	idata->protect_dirs = NULL;
	return PAM_SYSTEM_ERR;
    }
    return retval;
}


/*
 * Orig namespace. This function is called from when closing a pam
 * session. If authorized, it unmounts instance directory.
 */
static int orig_namespace(struct instance_data *idata)
{
    struct polydir_s *pptr;

    if (idata->flags & PAMNS_DEBUG)
        pam_syslog(idata->pamh, LOG_DEBUG, "orig namespace for pid %d",
		getpid());

    /*
     * Cycle through all polyinstantiated directories from the namespace
     * configuration file to see if polyinstantiation was performed for
     * this user for each of the entry. If it was, try and unmount
     * appropriate polyinstantiated instance directories.
     */
    for (pptr = idata->polydirs_ptr; pptr; pptr = pptr->next) {
        if (ns_override(pptr, idata, idata->uid))
            continue;
        else {
            if (idata->flags & PAMNS_DEBUG)
                pam_syslog(idata->pamh, LOG_DEBUG,
			"Unmounting instance dir for user %d & dir %s",
                       idata->uid, pptr->dir);

            if (secure_umount(pptr->dir) < 0) {
                pam_syslog(idata->pamh, LOG_ERR, "Unmount of %s failed, %m",
                       pptr->dir);
                return PAM_SESSION_ERR;
            } else if (idata->flags & PAMNS_DEBUG)
                pam_syslog(idata->pamh, LOG_DEBUG, "Unmount of %s succeeded",
			pptr->dir);
	}
    }

    cleanup_tmpdirs(idata);
    return 0;
}


#ifdef WITH_SELINUX
/*
 * This function checks if the calling program has requested context
 * change by calling setexeccon(). If context change is not requested
 * then it does not make sense to polyinstantiate based on context.
 * The return value from this function is used when selecting the
 * polyinstantiation method. If context change is not requested then
 * the polyinstantiation method is set to USER, even if the configuration
 * file lists the method as "context" or "level".
 */
static int ctxt_based_inst_needed(void)
{
    char *scon = NULL;
    int rc = 0;

    rc = getexeccon(&scon);
    if (rc < 0 || scon == NULL)
        return 0;
    else {
        freecon(scon);
        return 1;
    }
}
#endif

static int root_shared(void)
{
    FILE *f;
    char *line = NULL;
    size_t n = 0;
    int rv = 0;

    f = fopen("/proc/self/mountinfo", "r");

    if (f == NULL)
        return 0;

    while(getline(&line, &n, f) != -1) {
        char *l;
        char *sptr;
        int i;

        l = line;
        sptr = NULL;
        for (i = 0; i < 7; i++) {
             char *tok;

             tok = strtok_r(l, " ", &sptr);
             l = NULL;
             if (tok == NULL)
                 /* next mountinfo line */
                 break;

             if (i == 4 && strcmp(tok, "/") != 0)
                 /* next mountinfo line */
                 break;

             if (i == 6) {
                if (pam_str_skip_prefix(tok, "shared:") != NULL)
                 /* there might be more / mounts, the last one counts */
                    rv = 1;
                else
                    rv = 0;
             }
        }
    }

    free(line);
    fclose(f);

    return rv;
}

static int get_user_data(struct instance_data *idata)
{
    int retval;
    char *user_name;
    struct passwd *pwd;
    /*
     * Lookup user and fill struct items
     */
    retval = pam_get_item(idata->pamh, PAM_USER, (void*) &user_name );
    if ( user_name == NULL || retval != PAM_SUCCESS ) {
        pam_syslog(idata->pamh, LOG_ERR, "Error recovering pam user name");
        return PAM_SESSION_ERR;
    }

    pwd = pam_modutil_getpwnam(idata->pamh, user_name);
    if (!pwd) {
        pam_syslog(idata->pamh, LOG_ERR, "user unknown '%s'", user_name);
        return PAM_USER_UNKNOWN;
    }

    /*
     * Add the user info to the instance data so we can refer to them later.
     */
    idata->user[0] = 0;
    strncat(idata->user, user_name, sizeof(idata->user) - 1);
    idata->uid = pwd->pw_uid;
    idata->gid = pwd->pw_gid;

    /* Fill in RUSER too */
    retval = pam_get_item(idata->pamh, PAM_RUSER, (void*) &user_name );
    if ( user_name != NULL && retval == PAM_SUCCESS && user_name[0] != '\0' ) {
	strncat(idata->ruser, user_name, sizeof(idata->ruser) - 1);
	pwd = pam_modutil_getpwnam(idata->pamh, user_name);
    } else {
	pwd = pam_modutil_getpwuid(idata->pamh, getuid());
    }
    if (!pwd) {
	pam_syslog(idata->pamh, LOG_ERR, "user unknown '%s'", user_name);
	return PAM_USER_UNKNOWN;
    }
    user_name = pwd->pw_name;

    idata->ruser[0] = 0;
    strncat(idata->ruser, user_name, sizeof(idata->ruser) - 1);
    idata->ruid = pwd->pw_uid;

    return PAM_SUCCESS;
}

/*
 * Entry point from pam_open_session call.
 */
int pam_sm_open_session(pam_handle_t *pamh, int flags UNUSED,
                                   int argc, const char **argv)
{
    int i, retval;
    struct instance_data idata;
    enum unmnt_op unmnt = NO_UNMNT;

    /* init instance data */
    idata.flags = 0;
    idata.polydirs_ptr = NULL;
    idata.protect_dirs = NULL;
    idata.pamh = pamh;
#ifdef WITH_SELINUX
    if (is_selinux_enabled())
        idata.flags |= PAMNS_SELINUX_ENABLED;
    if (ctxt_based_inst_needed())
        idata.flags |= PAMNS_CTXT_BASED_INST;
#endif

    /* Parse arguments. */
    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "debug") == 0)
            idata.flags |= PAMNS_DEBUG;
        if (strcmp(argv[i], "gen_hash") == 0)
            idata.flags |= PAMNS_GEN_HASH;
        if (strcmp(argv[i], "ignore_config_error") == 0)
            idata.flags |= PAMNS_IGN_CONFIG_ERR;
        if (strcmp(argv[i], "ignore_instance_parent_mode") == 0)
            idata.flags |= PAMNS_IGN_INST_PARENT_MODE;
        if (strcmp(argv[i], "use_current_context") == 0) {
            idata.flags |= PAMNS_USE_CURRENT_CONTEXT;
            idata.flags |= PAMNS_CTXT_BASED_INST;
        }
        if (strcmp(argv[i], "use_default_context") == 0) {
            idata.flags |= PAMNS_USE_DEFAULT_CONTEXT;
            idata.flags |= PAMNS_CTXT_BASED_INST;
        }
        if (strcmp(argv[i], "mount_private") == 0) {
            idata.flags |= PAMNS_MOUNT_PRIVATE;
        }
        if (strcmp(argv[i], "unmnt_remnt") == 0)
            unmnt = UNMNT_REMNT;
        if (strcmp(argv[i], "unmnt_only") == 0)
            unmnt = UNMNT_ONLY;
	if (strcmp(argv[i], "require_selinux") == 0) {
		if (!(idata.flags & PAMNS_SELINUX_ENABLED)) {
			pam_syslog(idata.pamh, LOG_ERR,
		    "selinux_required option given and selinux is disabled");
			return PAM_SESSION_ERR;
		}
	}
    }
    if (idata.flags & PAMNS_DEBUG)
        pam_syslog(idata.pamh, LOG_DEBUG, "open_session - start");

    retval = get_user_data(&idata);
    if (retval != PAM_SUCCESS)
	return retval;

    if (root_shared()) {
	idata.flags |= PAMNS_MOUNT_PRIVATE;
    }

    /*
     * Parse namespace configuration file which lists directories to
     * polyinstantiate, directory where instance directories are to
     * be created and the method used for polyinstantiation.
     */
    retval = parse_config_file(&idata);
    if (retval != PAM_SUCCESS) {
	del_polydir_list(idata.polydirs_ptr);
        return PAM_SESSION_ERR;
    }

    if (idata.polydirs_ptr) {
        retval = setup_namespace(&idata, unmnt);
        if (idata.flags & PAMNS_DEBUG) {
            if (retval)
                pam_syslog(idata.pamh, LOG_DEBUG,
			"namespace setup failed for pid %d", getpid());
            else
                pam_syslog(idata.pamh, LOG_DEBUG,
			"namespace setup ok for pid %d", getpid());
        }
    } else if (idata.flags & PAMNS_DEBUG)
        pam_syslog(idata.pamh, LOG_DEBUG, "Nothing to polyinstantiate");

    if (retval != PAM_SUCCESS)
	del_polydir_list(idata.polydirs_ptr);
    return retval;
}


/*
 * Entry point from pam_close_session call.
 */
int pam_sm_close_session(pam_handle_t *pamh, int flags UNUSED,
                                    int argc, const char **argv)
{
    int i, retval;
    struct instance_data idata;
    const void *polyptr;

    /* init instance data */
    idata.flags = 0;
    idata.polydirs_ptr = NULL;
    idata.pamh = pamh;
#ifdef WITH_SELINUX
    if (is_selinux_enabled())
        idata.flags |= PAMNS_SELINUX_ENABLED;
    if (ctxt_based_inst_needed())
        idata.flags |= PAMNS_CTXT_BASED_INST;
#endif

    /* Parse arguments. */
    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "debug") == 0)
            idata.flags |= PAMNS_DEBUG;
        if (strcmp(argv[i], "ignore_config_error") == 0)
            idata.flags |= PAMNS_IGN_CONFIG_ERR;
        if (strcmp(argv[i], "unmount_on_close") == 0)
            idata.flags |= PAMNS_UNMOUNT_ON_CLOSE;
    }

    if (idata.flags & PAMNS_DEBUG)
        pam_syslog(idata.pamh, LOG_DEBUG, "close_session - start");

    /*
     * Normally the unmount is implicitly done when the last
     * process in the private namespace exits.
     * If it is ensured that there are no child processes left in
     * the private namespace by other means and if there are
     * multiple sessions opened and closed sequentially by the
     * same process, the "unmount_on_close" option might be
     * used to unmount the polydirs explicitly.
     */
    if (!(idata.flags & PAMNS_UNMOUNT_ON_CLOSE)) {
	pam_set_data(idata.pamh, NAMESPACE_POLYDIR_DATA, NULL, NULL);
	pam_set_data(idata.pamh, NAMESPACE_PROTECT_DATA, NULL, NULL);

	if (idata.flags & PAMNS_DEBUG)
	    pam_syslog(idata.pamh, LOG_DEBUG, "close_session - successful");
        return PAM_SUCCESS;
    }

    retval = get_user_data(&idata);
    if (retval != PAM_SUCCESS)
	return retval;

    retval = pam_get_data(idata.pamh, NAMESPACE_POLYDIR_DATA, &polyptr);
    if (retval != PAM_SUCCESS || polyptr == NULL)
	/* nothing to reset */
	return PAM_SUCCESS;

    DIAG_PUSH_IGNORE_CAST_QUAL;
    idata.polydirs_ptr = (void *)polyptr;
    DIAG_POP_IGNORE_CAST_QUAL;

    if (idata.flags & PAMNS_DEBUG)
        pam_syslog(idata.pamh, LOG_DEBUG, "Resetting namespace for pid %d",
		getpid());

    retval = orig_namespace(&idata);
    if (idata.flags & PAMNS_DEBUG) {
        if (retval)
            pam_syslog(idata.pamh, LOG_DEBUG,
		"resetting namespace failed for pid %d", getpid());
        else
            pam_syslog(idata.pamh, LOG_DEBUG,
		"resetting namespace ok for pid %d", getpid());
    }

    pam_set_data(idata.pamh, NAMESPACE_POLYDIR_DATA, NULL, NULL);
    pam_set_data(idata.pamh, NAMESPACE_PROTECT_DATA, NULL, NULL);

    return PAM_SUCCESS;
}
