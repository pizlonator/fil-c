/*
 * pam_motd module
 *
 * Modified for pam_motd by Ben Collins <bcollins@debian.org>
 * Written by Michael K. Johnson <johnsonm@redhat.com> 1996/10/24
 */

#include "config.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <syslog.h>
#include <errno.h>

#include <security/_pam_macros.h>
#include <security/pam_ext.h>
#include <security/pam_modules.h>
#include <security/pam_modutil.h>
#include "pam_inline.h"

#define DEFAULT_MOTD	"/etc/motd:/run/motd:/usr/lib/motd"
#define DEFAULT_MOTD_D	"/etc/motd.d:/run/motd.d:/usr/lib/motd.d"

/* --- session management functions (only) --- */

int
pam_sm_close_session (pam_handle_t *pamh UNUSED, int flags UNUSED,
		      int argc UNUSED, const char **argv UNUSED)
{
     return PAM_IGNORE;
}

static const char default_motd[] = DEFAULT_MOTD;
static const char default_motd_dir[] = DEFAULT_MOTD_D;

static void try_to_display_fd(pam_handle_t *pamh, int fd)
{
    struct stat st;
    char *mtmp = NULL;

    /* fill in message buffer with contents of motd */
    if ((fstat(fd, &st) < 0) || !st.st_size || st.st_size > 0x10000)
	return;

    if (!(mtmp = malloc(st.st_size+1)))
	return;

    if (pam_modutil_read(fd, mtmp, st.st_size) == st.st_size) {
	if (mtmp[st.st_size-1] == '\n')
	    mtmp[st.st_size-1] = '\0';
	else
	    mtmp[st.st_size] = '\0';

	pam_info (pamh, "%s", mtmp);
    }

    _pam_drop(mtmp);
}

/*
 * Split a DELIM-separated string ARG into an array.
 * Outputs a newly allocated array of strings OUT_ARG_SPLIT
 * and the number of strings OUT_NUM_STRS.
 * Returns 0 in case of error, 1 in case of success.
 */
static int pam_split_string(const pam_handle_t *pamh, char *arg, char delim,
			    char ***out_arg_split, size_t *out_num_strs)
{
    char *arg_extracted = NULL;
    const char *arg_ptr = arg;
    char **arg_split = NULL;
    char delim_str[2];
    size_t i = 0;
    size_t num_strs = 0;
    int retval = 0;

    delim_str[0] = delim;
    delim_str[1] = '\0';

    if (arg == NULL) {
	goto out;
    }

    while (arg_ptr != NULL) {
	num_strs++;
	arg_ptr = strchr(arg_ptr + sizeof(const char), delim);
    }

    arg_split = calloc(num_strs, sizeof(*arg_split));
    if (arg_split == NULL) {
	pam_syslog(pamh, LOG_CRIT, "failed to allocate string array");
	goto out;
    }

    arg_extracted = strtok_r(arg, delim_str, &arg);
    while (arg_extracted != NULL && i < num_strs) {
	arg_split[i++] = arg_extracted;
	arg_extracted = strtok_r(NULL, delim_str, &arg);
    }

    retval = 1;

  out:
    *out_num_strs = num_strs;
    *out_arg_split = arg_split;

    return retval;
}

/* Join A_STR and B_STR, inserting a "/" between them if one is not already trailing
 * in A_STR or beginning B_STR.
 * Returns NULL in case of an error,
 * or a newly allocated string holding the joined string in case of success. */
static char *join_dir_strings(const char *a_str, const char *b_str)
{
    int has_sep;

    if (a_str == NULL || b_str == NULL || strlen(a_str) == 0)
	return NULL;

    has_sep = (a_str[strlen(a_str) - 1] == '/') || (b_str[0] == '/');

    return pam_asprintf("%s%s%s", a_str, (has_sep == 1) ? "" : "/", b_str);
}

static int compare_strings(const void *a, const void *b)
{
    const char *a_str = *(const char * const *)a;
    const char *b_str = *(const char * const *)b;

    if (a_str == NULL && b_str == NULL) {
        return 0;
    }
    else if (a_str == NULL) {
	return -1;
    }
    else if (b_str == NULL) {
	return 1;
    }
    else {
	return strcmp(a_str, b_str);
    }
}

static void try_to_display_directories_with_overrides(pam_handle_t *pamh,
	char **motd_dir_path_split, size_t num_motd_dirs, int report_missing)
{
    struct dirent ***dirscans = NULL;
    unsigned int *dirscans_sizes = NULL;
    unsigned int dirscans_size_total = 0;
    char **dirnames_all = NULL;
    size_t i;
    unsigned int i_dirnames = 0;

    if (pamh == NULL || motd_dir_path_split == NULL) {
	goto out;
    }
    if (num_motd_dirs < 1) {
	goto out;
    }

    if ((dirscans = calloc(num_motd_dirs, sizeof(*dirscans))) == NULL) {
	pam_syslog(pamh, LOG_CRIT, "failed to allocate dirent arrays");
	goto out;
    }
    if ((dirscans_sizes = calloc(num_motd_dirs, sizeof(*dirscans_sizes))) == NULL) {
	pam_syslog(pamh, LOG_CRIT, "failed to allocate dirent array sizes");
	goto out;
    }

    for (i = 0; i < num_motd_dirs; i++) {
	int rv;
	rv = scandir(motd_dir_path_split[i], &(dirscans[i]), NULL, NULL);
	if (rv < 0) {
	    if (errno != ENOENT || report_missing) {
		pam_syslog(pamh, LOG_ERR, "error scanning directory %s: %m",
		    motd_dir_path_split[i]);
	    }
	} else {
	    dirscans_sizes[i] = rv;
	}
	if (dirscans_size_total > UINT_MAX - dirscans_sizes[i]) {
	    pam_syslog(pamh, LOG_CRIT, "encountered too many motd files");
	    goto out;
	}
	dirscans_size_total += dirscans_sizes[i];
    }

    if (dirscans_size_total == 0)
        goto out;

    /* filter out unwanted names, directories, and complement data with lstat() */
    for (i = 0; i < num_motd_dirs; i++) {
	struct dirent **d = dirscans[i];
	for (unsigned int j = 0; j < dirscans_sizes[i]; j++) {
	    int rc;
	    char *fullpath;
	    struct stat s;

	    switch(d[j]->d_type) {    /* the filetype determines how to proceed */
	    case DT_REG:              /* regular files and     */
	    case DT_LNK:              /* symlinks              */
		continue;             /* are good.             */
	    case DT_UNKNOWN:   /* for file systems that do not provide */
			       /* a filetype, we use lstat()           */
		if ((fullpath = join_dir_strings(motd_dir_path_split[i],
						 d[j]->d_name)) == NULL)
		    break;
		rc = lstat(fullpath, &s);
		_pam_drop(fullpath);  /* free the memory alloc'ed by join_dir_strings */
		if (rc != 0)          /* if the lstat() somehow failed */
		    break;

		if (S_ISREG(s.st_mode) ||          /* regular files and  */
		    S_ISLNK(s.st_mode)) continue;  /* symlinks are good  */
		break;
	    case DT_DIR:          /* We don't want directories     */
	    default:              /* nor anything else             */
		break;
	    }
	    _pam_drop(d[j]);  /* free memory                   */
	    d[j] = NULL;      /* indicate this one was dropped */
	    dirscans_size_total--;
	}
    }

    /* Allocate space for all file names found in the directories, including duplicates. */
    if ((dirnames_all = calloc(dirscans_size_total, sizeof(*dirnames_all))) == NULL) {
	pam_syslog(pamh, LOG_CRIT, "failed to allocate dirname array");
	goto out;
    }

    for (i = 0; i < num_motd_dirs; i++) {
	unsigned int j;

	for (j = 0; j < dirscans_sizes[i]; j++) {
	    if (NULL != dirscans[i][j]) {
	        dirnames_all[i_dirnames] = dirscans[i][j]->d_name;
	        i_dirnames++;
	    }
	}
    }

    qsort(dirnames_all, dirscans_size_total,
	    sizeof(const char *), compare_strings);

    for (i = 0; i < dirscans_size_total; i++) {
	unsigned int j;

	if (dirnames_all[i] == NULL) {
	    continue;
	}

	/* Skip duplicate file names. */
	if (i > 0 && strcmp(dirnames_all[i], dirnames_all[i - 1]) == 0) {
	    continue;
	}

	for (j = 0; j < num_motd_dirs; j++) {
	    char *abs_path;
	    int fd;

	    if ((abs_path = join_dir_strings(motd_dir_path_split[j],
					     dirnames_all[i])) == NULL) {
		continue;
	    }

	    fd = open(abs_path, O_RDONLY, 0);
	    _pam_drop(abs_path);

	    if (fd >= 0) {
		try_to_display_fd(pamh, fd);
		close(fd);

		/* We displayed a file, skip to the next file name. */
		break;
	    }
	}
    }

  out:
    _pam_drop(dirnames_all);
    if (dirscans_sizes != NULL) {
	for (i = 0; i < num_motd_dirs; i++) {
	    unsigned int j;

	    for (j = 0; j < dirscans_sizes[i]; j++)
		_pam_drop(dirscans[i][j]);
	    _pam_drop(dirscans[i]);
	}
	_pam_drop(dirscans_sizes);
    }
    _pam_drop(dirscans);
}

static int drop_privileges(pam_handle_t *pamh, struct pam_modutil_privs *privs)
{
    struct passwd *pw;
    const char *username;
    int retval;

    retval = pam_get_user(pamh, &username, NULL);

    if (retval == PAM_SUCCESS) {
        pw = pam_modutil_getpwnam (pamh, username);
    } else {
        return PAM_SESSION_ERR;
    }

    if (pw == NULL || pam_modutil_drop_priv(pamh, privs, pw)) {
        return PAM_SESSION_ERR;
    }

    return PAM_SUCCESS;
}

static int try_to_display(pam_handle_t *pamh, char **motd_path_split,
                          size_t num_motd_paths, char **motd_dir_path_split,
                          size_t num_motd_dir_paths, int report_missing)
{
    PAM_MODUTIL_DEF_PRIVS(privs);

    if (drop_privileges(pamh, &privs) != PAM_SUCCESS) {
        pam_syslog(pamh, LOG_ERR, "Unable to drop privileges");
        return PAM_SESSION_ERR;
    }

    if (motd_path_split != NULL) {
        size_t i;

        for (i = 0; i < num_motd_paths; i++) {
            int fd = open(motd_path_split[i], O_RDONLY, 0);

            if (fd >= 0) {
                try_to_display_fd(pamh, fd);
                close(fd);

                /* We found and displayed a file,
                    * move onto next filename.
                    */
                break;
            }
        }
    }

    if (motd_dir_path_split != NULL) {
        try_to_display_directories_with_overrides(pamh,
                                                    motd_dir_path_split,
                                                    num_motd_dir_paths,
                                                    report_missing);
    }

    if (pam_modutil_regain_priv(pamh, &privs)) {
        pam_syslog(pamh, LOG_ERR, "Unable to regain privileges");
        return PAM_SESSION_ERR;
    }

    return PAM_SUCCESS;
}

int pam_sm_open_session(pam_handle_t *pamh, int flags,
			int argc, const char **argv)
{
    int retval = PAM_IGNORE;
    const char *motd_path = NULL;
    char *motd_path_copy = NULL;
    size_t num_motd_paths = 0;
    char **motd_path_split = NULL;
    const char *motd_dir_path = NULL;
    char *motd_dir_path_copy = NULL;
    size_t num_motd_dir_paths = 0;
    char **motd_dir_path_split = NULL;
    int report_missing;

    if (flags & PAM_SILENT) {
	return retval;
    }

    for (; argc-- > 0; ++argv) {
	const char *str;
	if ((str = pam_str_skip_prefix(*argv, "motd=")) != NULL) {

            motd_path = str;
            if (*motd_path != '\0') {
                D(("set motd path: %s", motd_path));
	    } else {
		motd_path = NULL;
		pam_syslog(pamh, LOG_ERR,
			   "motd= specification missing argument - ignored");
	    }
	}
	else if ((str = pam_str_skip_prefix(*argv, "motd_dir=")) != NULL) {

            motd_dir_path = str;
            if (*motd_dir_path != '\0') {
                D(("set motd.d path: %s", motd_dir_path));
	    } else {
		motd_dir_path = NULL;
		pam_syslog(pamh, LOG_ERR,
			   "motd_dir= specification missing argument - ignored");
	    }
	}
	else
	    pam_syslog(pamh, LOG_ERR, "unknown option: %s", *argv);
    }

    if (motd_path == NULL && motd_dir_path == NULL) {
	motd_path = default_motd;
	motd_dir_path = default_motd_dir;
	report_missing = 0;
    } else {
	report_missing = 1;
    }

    if (motd_path != NULL) {
	motd_path_copy = strdup(motd_path);
    }

    if (motd_path_copy != NULL) {
	if (pam_split_string(pamh, motd_path_copy, ':',
		&motd_path_split, &num_motd_paths) == 0) {
	    goto out;
	}
    }

    if (motd_dir_path != NULL) {
	motd_dir_path_copy = strdup(motd_dir_path);
    }

    if (motd_dir_path_copy != NULL) {
	if (pam_split_string(pamh, motd_dir_path_copy, ':',
		&motd_dir_path_split, &num_motd_dir_paths) == 0) {
	    goto out;
	}
    }

    retval = try_to_display(pamh, motd_path_split, num_motd_paths,
                            motd_dir_path_split, num_motd_dir_paths,
                            report_missing);

  out:
    _pam_drop(motd_path_copy);
    _pam_drop(motd_path_split);
    _pam_drop(motd_dir_path_copy);
    _pam_drop(motd_dir_path_split);

    if (retval == PAM_SUCCESS) {
        retval = pam_putenv(pamh, "MOTD_SHOWN=pam");
        return retval == PAM_SUCCESS ? PAM_IGNORE : retval;
    } else {
        return retval;
    }
}

/* end of module definition */
