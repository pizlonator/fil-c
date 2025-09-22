/* hardlink.c - Link multiple identical files together
 *
 * Copyright (C) 2008 - 2014 Julian Andres Klode <jak@jak-linux.org>
 * Copyright (C) 2021 Karel Zak <kzak@redhat.com>
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#define _POSIX_C_SOURCE 200112L	/* POSIX functions */
#define _XOPEN_SOURCE      600	/* nftw() */

#include <sys/types.h>		/* stat */
#include <sys/stat.h>		/* stat */
#include <sys/time.h>		/* getrlimit, getrusage */
#include <sys/resource.h>	/* getrlimit, getrusage */
#include <fcntl.h>		/* posix_fadvise */
#include <ftw.h>		/* ftw */
#include <search.h>		/* tsearch() and friends */
#include <signal.h>		/* SIG*, sigaction */
#include <getopt.h>		/* getopt_long() */
#include <ctype.h>		/* tolower() */
#include <sys/ioctl.h>

#if defined(HAVE_LINUX_FIEMAP_H) && defined(HAVE_SYS_VFS_H)
# include <linux/fs.h>
# include <linux/fiemap.h>
# ifdef FICLONE
#  define USE_REFLINK 1
# endif
#endif

#include "nls.h"
#include "c.h"
#include "xalloc.h"
#include "strutils.h"
#include "monotonic.h"
#include "optutils.h"
#include "fileeq.h"

#ifdef USE_REFLINK
# include "statfs_magic.h"
#endif

#include <regex.h>		/* regcomp(), regexec() */

#if defined(HAVE_SYS_XATTR_H) && defined(HAVE_LLISTXATTR) && defined(HAVE_LGETXATTR)
# include <sys/xattr.h>
# define USE_XATTR 1
#endif

static int quiet;		/* don't print anything */
static int rootbasesz;		/* size of the directory for nftw() */

#ifdef USE_REFLINK
enum {
	REFLINK_NEVER  = 0,
	REFLINK_AUTO,
	REFLINK_ALWAYS
};
static int reflink_mode = REFLINK_NEVER;
static int reflinks_skip;
#endif

static struct ul_fileeq fileeq;

/**
 * struct file - Information about a file
 * @st:       The stat buffer associated with the file
 * @next:     Next file with the same size
 * @basename: The offset off the basename in the filename
 * @path:     The path of the file
 *
 * This contains all information we need about a file.
 */
struct file {
	struct stat st;
	struct ul_fileeq_data data;

	struct file *next;
	struct link {
		struct link *next;
		int basename;
		int dirname;
#if __STDC_VERSION__ >= 199901L
		char path[];
#elif __GNUC__
		char path[0];
#else
		char path[1];
#endif
	} *links;
};

/**
 * enum log_level - Logging levels
 * @JLOG_SUMMARY: Default log level
 * @JLOG_INFO:    Verbose logging (verbose == 1)
 * @JLOG_VERBOSE1:  Verbosity 2
 * @JLOG_VERBOSE2:  Verbosity 3
 */
enum log_level {
	JLOG_SUMMARY,
	JLOG_INFO,
	JLOG_VERBOSE1,
	JLOG_VERBOSE2
};

/**
 * struct statistic - Statistics about the file
 * @started: Whether we are post command-line processing
 * @files: The number of files worked on
 * @linked: The number of files replaced by a hardlink to a master
 * @xattr_comparisons: The number of extended attribute comparisons
 * @comparisons: The number of comparisons
 * @saved: The (exaggerated) amount of space saved
 * @start_time: The time we started at
 */
static struct statistics {
	int started;
	size_t files;
	size_t linked;
	size_t xattr_comparisons;
	size_t comparisons;
	size_t ignored_reflinks;
	double saved;
	struct timeval start_time;
} stats;


struct hdl_regex {
	regex_t re;		/* POSIX compatible regex handler */

	struct hdl_regex *next;
};

/**
 * struct options - Processed command-line options
 * @include: A linked list of regular expressions for the --include option
 * @exclude: A linked list of regular expressions for the --exclude option
 * @verbosity: The verbosity. Should be one of #enum log_level
 * @respect_mode: Whether to respect file modes (default = TRUE)
 * @respect_owner: Whether to respect file owners (uid, gid; default = TRUE)
 * @respect_name: Whether to respect file names (default = FALSE)
 * @respect_time: Whether to respect file modification times (default = TRUE)
 * @respect_xattrs: Whether to respect extended attributes (default = FALSE)
 * @maximise: Chose the file with the highest link count as master
 * @minimise: Chose the file with the lowest link count as master
 * @keep_oldest: Choose the file with oldest timestamp as master (default = FALSE)
 * @dry_run: Specifies whether hardlink should not link files (default = FALSE)
 * @min_size: Minimum size of files to consider. (default = 1 byte)
 * @max_size: Maximum size of files to consider, 0 means umlimited. (default = 0 byte)
 */
static struct options {
	struct hdl_regex *include;
	struct hdl_regex *exclude;

	const char *method;
	signed int verbosity;
	unsigned int respect_mode:1;
	unsigned int respect_owner:1;
	unsigned int respect_name:1;
	unsigned int respect_dir:1;
	unsigned int respect_time:1;
	unsigned int respect_xattrs:1;
	unsigned int maximise:1;
	unsigned int minimise:1;
	unsigned int keep_oldest:1;
	unsigned int dry_run:1;
	uintmax_t min_size;
	uintmax_t max_size;
	size_t io_size;
	size_t cache_size;
} opts = {
	/* default setting */
#ifdef USE_FILEEQ_CRYPTOAPI
	.method = "sha256",
#else
	.method = "memcmp",
#endif
	.respect_mode = TRUE,
	.respect_owner = TRUE,
	.respect_time = TRUE,
	.respect_xattrs = FALSE,
	.keep_oldest = FALSE,
	.min_size = 1,
	.cache_size = 10*1024*1024
};

/*
 * files
 *
 * A binary tree of files, managed using tsearch(). To see which nodes
 * are considered equal, see compare_nodes()
 */
static void *files;
static void *files_by_ino;

/*
 * last_signal
 *
 * The last signal we received. We store the signal here in order to be able
 * to break out of loops gracefully and to return from our nftw() handler.
 */
static volatile sig_atomic_t last_signal;


#define is_log_enabled(_level)  (quiet == 0 && (_level) <= (unsigned int)opts.verbosity)

/**
 * jlog - Logging for hardlink
 * @level: The log level
 * @format: A format string for printf()
 */
__attribute__((format(printf, 2, 3)))
static void jlog(enum log_level level, const char *format, ...)
{
	va_list args;

	if (!is_log_enabled(level))
		return;

	va_start(args, format);
	vfprintf(stdout, format, args);
	va_end(args);
	fputc('\n', stdout);
}

/**
 * CMP - Compare two numerical values, return 1, 0, or -1
 * @a: First value
 * @b: Second value
 *
 * Used to compare two integers of any size while avoiding overflow.
 */
#define CMP(a, b) ((a) > (b) ? 1 : ((a) < (b) ? -1 : 0))

/**
 * register_regex - Compile and insert a regular expression into list
 * @pregs: Pointer to a linked list of regular expressions
 * @regex: String containing the regular expression to be compiled
 */
static void register_regex(struct hdl_regex **pregs, const char *regex)
{
	struct hdl_regex *link;
	int err;

	link = xmalloc(sizeof(*link));

	if ((err = regcomp(&link->re, regex, REG_NOSUB | REG_EXTENDED)) != 0) {
		size_t size = regerror(err, &link->re, NULL, 0);
		char *buf = xmalloc(size + 1);

		regerror(err, &link->re, buf, size);

		errx(EXIT_FAILURE, _("could not compile regular expression %s: %s"),
				regex, buf);
	}
	link->next = *pregs; *pregs = link;
}

/**
 * match_any_regex - Match against multiple regular expressions
 * @pregs: A linked list of regular expressions
 * @what:  The string to match against
 *
 * Checks whether any of the regular expressions in the list matches the
 * string.
 */
static int match_any_regex(struct hdl_regex *pregs, const char *what)
{
	for (; pregs != NULL; pregs = pregs->next) {
		if (regexec(&pregs->re, what, 0, NULL, 0) == 0)
			return TRUE;
	}
	return FALSE;
}

/**
 * compare_nodes - Node comparison function
 * @_a: The first node (a #struct file)
 * @_b: The second node (a #struct file)
 *
 * Compare the two nodes for the binary tree.
 */
static int compare_nodes(const void *_a, const void *_b)
{
	const struct file *a = _a;
	const struct file *b = _b;
	int diff = 0;

	if (diff == 0)
		diff = CMP(a->st.st_dev, b->st.st_dev);
	if (diff == 0)
		diff = CMP(a->st.st_size, b->st.st_size);

	return diff;
}

/* Compare only filenames */
static inline int filename_strcmp(const struct file *a, const struct file *b)
{
	return strcmp(	a->links->path + a->links->basename,
			b->links->path + b->links->basename);
}

/**
 * Compare only directory names (ignores root directory and basename (filename))
 *
 * The complete path conrains three fragments:
 *
 * <rootdir> is specified on hardlink command line
 * <dirname> is all betweehn rootdir and filename
 * <filename> is last component (aka basename)
 */
static inline int dirname_strcmp(const struct file *a, const struct file *b)
{
	int diff = 0;
	int asz = a->links->basename - a->links->dirname,
	    bsz = b->links->basename - b->links->dirname;

	diff = CMP(asz, bsz);

	if (diff == 0) {
		const char *a_start, *b_start;

		a_start = a->links->path + a->links->dirname;
		b_start = b->links->path + b->links->dirname;

		diff = strncmp(a_start, b_start, asz);
	}
	return diff;
}

/**
 * compare_nodes_ino - Node comparison function
 * @_a: The first node (a #struct file)
 * @_b: The second node (a #struct file)
 *
 * Compare the two nodes for the binary tree.
 */
static int compare_nodes_ino(const void *_a, const void *_b)
{
	const struct file *a = _a;
	const struct file *b = _b;
	int diff = 0;

	if (diff == 0)
		diff = CMP(a->st.st_dev, b->st.st_dev);
	if (diff == 0)
		diff = CMP(a->st.st_ino, b->st.st_ino);

	/* If opts.respect_name is used, we will restrict a struct file to
	 * contain only links with the same basename to keep the rest simple.
	 */
	if (diff == 0 && opts.respect_name)
		diff = filename_strcmp(a, b);
	if (diff == 0 && opts.respect_dir)
		diff = dirname_strcmp(a, b);

	return diff;
}

/**
 * print_stats - Print statistics to stdout
 */
static void print_stats(void)
{
	struct timeval end = { 0, 0 }, delta = { 0, 0 };
	char *ssz;

	gettime_monotonic(&end);
	timersub(&end, &stats.start_time, &delta);

	jlog(JLOG_SUMMARY, "%-25s %s", _("Mode:"),
	     opts.dry_run ? _("dry-run") : _("real"));
	jlog(JLOG_SUMMARY, "%-25s %s", _("Method:"), opts.method);
	jlog(JLOG_SUMMARY, "%-25s %zu", _("Files:"), stats.files);
	jlog(JLOG_SUMMARY, _("%-25s %zu files"), _("Linked:"), stats.linked);

#ifdef USE_XATTR
	jlog(JLOG_SUMMARY, _("%-25s %zu xattrs"), _("Compared:"),
	     stats.xattr_comparisons);
#endif
	jlog(JLOG_SUMMARY, _("%-25s %zu files"), _("Compared:"),
	     stats.comparisons);
#ifdef USE_REFLINK
	if (reflinks_skip)
		jlog(JLOG_SUMMARY, _("%-25s %zu files"), _("Skipped reflinks:"),
		     stats.ignored_reflinks);
#endif
	ssz = size_to_human_string(SIZE_SUFFIX_3LETTER |
				   SIZE_SUFFIX_SPACE |
				   SIZE_DECIMAL_2DIGITS, stats.saved);

	jlog(JLOG_SUMMARY, "%-25s %s", _("Saved:"), ssz);
	free(ssz);

	jlog(JLOG_SUMMARY, _("%-25s %"PRId64".%06"PRId64" seconds"), _("Duration:"),
	     (int64_t)delta.tv_sec, (int64_t)delta.tv_usec);
}

/**
 * handle_interrupt - Handle a signal
 *
 * Returns: %TRUE on SIGINT, SIGTERM; %FALSE on all other signals.
 */
static int handle_interrupt(void)
{
	switch (last_signal) {
	case SIGINT:
	case SIGTERM:
		return TRUE;
	case SIGUSR1:
		print_stats();
		putchar('\n');
		break;
	}

	last_signal = 0;
	return FALSE;
}

#ifdef USE_XATTR

/**
 * llistxattr_or_die - Wrapper for llistxattr()
 *
 * This does the same thing as llistxattr() except that it aborts if any error
 * other than "not supported" is detected.
 */
static ssize_t llistxattr_or_die(const char *path, char *list, size_t size)
{
	ssize_t len = llistxattr(path, list, size);

	if (len < 0 && errno != ENOTSUP)
		err(EXIT_FAILURE, _("cannot get xattr names for %s"), path);

	return len;
}

/**
 * lgetxattr_or_die - Wrapper for lgetxattr()
 *
 * This does the same thing as lgetxattr() except that it aborts upon error.
 */
static ssize_t lgetxattr_or_die(const char *path,
			const char *name, void *value, size_t size)
{
	ssize_t len = lgetxattr(path, name, value, size);

	if (len < 0)
		err(EXIT_FAILURE, _("cannot get xattr value of %s for %s"),
		    name, path);

	return len;
}

/**
 * get_xattr_name_count - Count the number of xattr names
 * @names: a non-empty table of concatenated, null-terminated xattr names
 * @len: the total length of the table
 *
 * @Returns the number of xattr names
 */
static int get_xattr_name_count(const char *const names, ssize_t len)
{
	int count = 0;
	const char *name;

	for (name = names; name < (names + len); name += strlen(name) + 1)
		count++;

	return count;
}

/**
 * cmp_xattr_name_ptrs - Compare two pointers to xattr names by comparing
 * the names they point to.
 */
static int cmp_xattr_name_ptrs(const void *ptr1, const void *ptr2)
{
	return strcmp(*(char *const *)ptr1, *(char *const *)ptr2);
}

/**
 * get_sorted_xattr_name_table - Create a sorted table of xattr names.
 * @names - table of concatenated, null-terminated xattr names
 * @n - the number of names
 *
 * @Returns allocated table of pointers to the names, sorted alphabetically
 */
static const char **get_sorted_xattr_name_table(const char *names, int n)
{
	const char **table = xmalloc(n * sizeof(char *));
	int i;

	for (i = 0; i < n; i++) {
		table[i] = names;
		names += strlen(names) + 1;
	}

	qsort(table, n, sizeof(char *), cmp_xattr_name_ptrs);

	return table;
}

/**
 * file_xattrs_equal - Compare the extended attributes of two files
 * @a: The first file
 * @b: The second file
 *
 * @Returns: %TRUE if and only if extended attributes are equal
 */
static int file_xattrs_equal(const struct file *a, const struct file *b)
{
	ssize_t len_a;
	ssize_t len_b;
	char *names_a = NULL;
	char *names_b = NULL;
	int n_a;
	int n_b;
	const char **name_ptrs_a = NULL;
	const char **name_ptrs_b = NULL;
	void *value_a = NULL;
	void *value_b = NULL;
	int ret = FALSE;
	int i;

	assert(a->links != NULL);
	assert(b->links != NULL);

	jlog(JLOG_VERBOSE1, _("Comparing xattrs of %s to %s"), a->links->path,
	     b->links->path);

	stats.xattr_comparisons++;

	len_a = llistxattr_or_die(a->links->path, NULL, 0);
	len_b = llistxattr_or_die(b->links->path, NULL, 0);

	if (len_a <= 0 && len_b <= 0)
		return TRUE;	// xattrs not supported or neither file has any

	if (len_a != len_b)
		return FALSE;	// total lengths of xattr names differ

	names_a = xmalloc(len_a);
	names_b = xmalloc(len_b);

	len_a = llistxattr_or_die(a->links->path, names_a, len_a);
	len_b = llistxattr_or_die(b->links->path, names_b, len_b);
	assert((len_a > 0) && (len_a == len_b));

	n_a = get_xattr_name_count(names_a, len_a);
	n_b = get_xattr_name_count(names_b, len_b);

	if (n_a != n_b)
		goto exit;	// numbers of xattrs differ

	name_ptrs_a = get_sorted_xattr_name_table(names_a, n_a);
	name_ptrs_b = get_sorted_xattr_name_table(names_b, n_b);

	// We now have two sorted tables of xattr names.

	for (i = 0; i < n_a; i++) {
		if (handle_interrupt())
			goto exit;	// user wants to quit

		if (strcmp(name_ptrs_a[i], name_ptrs_b[i]) != 0)
			goto exit;	// names at same slot differ

		len_a =
		    lgetxattr_or_die(a->links->path, name_ptrs_a[i], NULL, 0);
		len_b =
		    lgetxattr_or_die(b->links->path, name_ptrs_b[i], NULL, 0);

		if (len_a != len_b)
			goto exit;	// xattrs with same name, different value lengths

		value_a = xmalloc(len_a);
		value_b = xmalloc(len_b);

		len_a = lgetxattr_or_die(a->links->path, name_ptrs_a[i],
					 value_a, len_a);
		len_b = lgetxattr_or_die(b->links->path, name_ptrs_b[i],
					 value_b, len_b);
		assert((len_a >= 0) && (len_a == len_b));

		if (memcmp(value_a, value_b, len_a) != 0)
			goto exit;	// xattrs with same name, different values

		free(value_a);
		free(value_b);
		value_a = NULL;
		value_b = NULL;
	}

	ret = TRUE;

 exit:
	free(names_a);
	free(names_b);
	free(name_ptrs_a);
	free(name_ptrs_b);
	free(value_a);
	free(value_b);
	return ret;
}
#else /* !USE_XATTR */
static int file_xattrs_equal(const struct file *a, const struct file *b)
{
	return TRUE;
}
#endif /* USE_XATTR */

/**
 * file_may_link_to - Check whether a file may replace another one
 * @a: The first file
 * @b: The second file
 *
 * Check whether the two files are considered equal attributes and can be
 * linked. This function does not compare content od the files!
 */
static int file_may_link_to(const struct file *a, const struct file *b)
{
	return (a->st.st_size != 0 &&
		a->st.st_size == b->st.st_size &&
		a->links != NULL && b->links != NULL &&
		a->st.st_dev == b->st.st_dev &&
		a->st.st_ino != b->st.st_ino &&
		(!opts.respect_mode || a->st.st_mode == b->st.st_mode) &&
		(!opts.respect_owner || a->st.st_uid == b->st.st_uid) &&
		(!opts.respect_owner || a->st.st_gid == b->st.st_gid) &&
		(!opts.respect_time || a->st.st_mtime == b->st.st_mtime) &&
		(!opts.respect_name || filename_strcmp(a, b) == 0) &&
		(!opts.respect_dir || dirname_strcmp(a, b) == 0) &&
		(!opts.respect_xattrs || file_xattrs_equal(a, b)));
}

/**
 * file_compare - Compare two files to decide which should be master
 * @a: The first file
 * @b: The second file
 *
 * Check which of the files should be considered greater and thus serve
 * as the master when linking (the master is the file that all equal files
 * will be replaced with).
 */
static int file_compare(const struct file *a, const struct file *b)
{
	int res = 0;
	if (a->st.st_dev == b->st.st_dev && a->st.st_ino == b->st.st_ino)
		return 0;

	if (res == 0 && opts.maximise)
		res = CMP(a->st.st_nlink, b->st.st_nlink);
	if (res == 0 && opts.minimise)
		res = CMP(b->st.st_nlink, a->st.st_nlink);
	if (res == 0)
		res = opts.keep_oldest ? CMP(b->st.st_mtime, a->st.st_mtime)
		    : CMP(a->st.st_mtime, b->st.st_mtime);
	if (res == 0)
		res = CMP(b->st.st_ino, a->st.st_ino);

	return res;
}

#ifdef USE_REFLINK
static inline int do_link(struct file *a, struct file *b,
			  const char *new_name, int reflink)
{
	if (reflink) {
		int dest = -1, src = -1;

		dest = open(new_name, O_CREAT|O_WRONLY|O_TRUNC, 0600);
		if (dest < 0)
			goto fallback;
		if (fchmod(dest, b->st.st_mode) != 0)
			goto fallback;
		if (fchown(dest, b->st.st_uid, b->st.st_gid) != 0)
			goto fallback;
		src = open(a->links->path, O_RDONLY);
		if (src < 0)
			goto fallback;
		if (ioctl(dest, FICLONE, src) != 0)
			goto fallback;
		close(dest);
		close(src);
		return 0;
fallback:
		if (dest >= 0) {
			close(dest);
			unlink(new_name);
		}
		if (src >= 0)
			close(src);

		if (reflink_mode == REFLINK_ALWAYS)
			return -errno;
		jlog(JLOG_VERBOSE2,_("Reflinking failed, fallback to hardlinking"));
	}

	return link(a->links->path, new_name);
}
#else
static inline int do_link(struct file *a,
			  struct file *b __attribute__((__unused__)),
			  const char *new_name,
			  int reflink __attribute__((__unused__)))
{
	return link(a->links->path, new_name);
}
#endif /* USE_REFLINK */

/**
 * file_link - Replace b with a link to a
 * @a: The first file
 * @b: The second file
 *
 * Link the file, replacing @b with the current one. The file is first
 * linked to a temporary name, and then renamed to the name of @b, making
 * the replace atomic (@b will always exist).
 */
static int file_link(struct file *a, struct file *b, int reflink)
{

 file_link:
	assert(a->links != NULL);
	assert(b->links != NULL);

	if (is_log_enabled(JLOG_INFO)) {
		char *ssz = size_to_human_string(SIZE_SUFFIX_3LETTER |
				   SIZE_SUFFIX_SPACE |
				   SIZE_DECIMAL_2DIGITS, a->st.st_size);
		jlog(JLOG_INFO, _("%s%sLinking %s to %s (-%s)"),
		     opts.dry_run ? _("[DryRun] ") : "",
		     reflink ? "Ref" : "",
		     a->links->path, b->links->path,
		     ssz);
		free(ssz);
	}

	if (!opts.dry_run) {
		char *new_path;
		int failed = 1;

		xasprintf(&new_path, "%s.hardlink-temporary", b->links->path);

		if (do_link(a, b, new_path, reflink) != 0)
			warn(_("cannot link %s to %s"), a->links->path, new_path);

		else if (rename(new_path, b->links->path) != 0) {
			warn(_("cannot rename %s to %s"), a->links->path, new_path);
			unlink(new_path);
		} else
			failed = 0;

		free(new_path);
		if (failed)
			return FALSE;
	}

	/* Update statistics */
	stats.linked++;

	/* Increase the link count of this file, and set stat() of other file */
	a->st.st_nlink++;
	b->st.st_nlink--;

	if (b->st.st_nlink == 0)
		stats.saved += a->st.st_size;

	/* Move the link from file b to a */
	{
		struct link *new_link = b->links;

		b->links = b->links->next;
		new_link->next = a->links->next;
		a->links->next = new_link;
	}

	/* Do it again */
	if (b->links)
		goto file_link;

	return TRUE;
}

static int has_fpath(struct file *node, const char *path)
{
	struct link *l;

	for (l = node->links; l; l = l->next) {
		if (strcmp(l->path, path) == 0)
			return 1;
	}

	return 0;
}


/**
 * inserter - Callback function for nftw()
 * @fpath: The path of the file being visited
 * @sb:    The stat information of the file
 * @typeflag: The type flag
 * @ftwbuf:   Contains current level of nesting and offset of basename
 *
 * Called by nftw() for the files. See the manual page for nftw() for
 * further information.
 */
static int inserter(const char *fpath, const struct stat *sb,
		    int typeflag, struct FTW *ftwbuf)
{
	struct file *fil;
	struct file **node;
	size_t pathlen;
	int included;
	int excluded;

	if (handle_interrupt())
		return 1;
	if (typeflag == FTW_DNR || typeflag == FTW_NS)
		warn(_("cannot read %s"), fpath);
	if (typeflag != FTW_F || !S_ISREG(sb->st_mode))
		return 0;

	included = match_any_regex(opts.include, fpath);
	excluded = match_any_regex(opts.exclude, fpath);

	if ((opts.exclude && excluded && !included) ||
	    (!opts.exclude && opts.include && !included))
		return 0;

	stats.files++;

	if ((uintmax_t) sb->st_size < opts.min_size) {
		jlog(JLOG_VERBOSE1,
		     _("Skipped %s (smaller than configured size)"), fpath);
		return 0;
	}

	jlog(JLOG_VERBOSE2, " %5zu: [%" PRIu64 "/%" PRIu64 "/%zu] %s",
			stats.files, sb->st_dev, sb->st_ino,
			(size_t) sb->st_nlink, fpath);

	if ((opts.max_size > 0) && ((uintmax_t) sb->st_size > opts.max_size)) {
		jlog(JLOG_VERBOSE1,
		     _("Skipped %s (greater than configured size)"), fpath);
		return 0;
	}

	pathlen = strlen(fpath) + 1;

	fil = xcalloc(1, sizeof(*fil));
	fil->links = xcalloc(1, sizeof(struct link) + pathlen);

	fil->st = *sb;
	fil->links->basename = ftwbuf->base;
	fil->links->dirname = rootbasesz;
	fil->links->next = NULL;

	memcpy(fil->links->path, fpath, pathlen);

	node = tsearch(fil, &files_by_ino, compare_nodes_ino);

	if (node == NULL)
		goto fail;

	if (*node != fil) {
		/* Already known inode, add link to inode information */
		assert((*node)->st.st_dev == sb->st_dev);
		assert((*node)->st.st_ino == sb->st_ino);

		if (has_fpath(*node, fpath)) {
			jlog(JLOG_VERBOSE1,
				_("Skipped %s (specified more than once)"), fpath);
			free(fil->links);
		} else {
			fil->links->next = (*node)->links;
			(*node)->links = fil->links;
		}

		free(fil);
	} else {
		/* New inode, insert into by-size table */
		node = tsearch(fil, &files, compare_nodes);

		if (node == NULL)
			goto fail;

		if (*node != fil) {
			struct file *l;

			if (file_compare(fil, *node) >= 0) {
				fil->next = *node;
				*node = fil;
			} else {
				for (l = *node; l != NULL; l = l->next) {
					if (l->next != NULL
					    && file_compare(fil, l->next) < 0)
						continue;

					fil->next = l->next;
					l->next = fil;

					break;
				}
			}
		}
	}

	return 0;

 fail:
	warn(_("cannot continue"));	/* probably ENOMEM */
	return 0;
}

#ifdef USE_REFLINK
static int is_reflink_compatible(dev_t devno, const char *filename)
{
	static dev_t last_dev = 0;
	static int last_status = 0;

	if (last_dev != devno) {
		struct statfs vfs;

		if (statfs(filename, &vfs) != 0)
			return 0;

		last_dev = devno;
		switch (vfs.f_type) {
			case STATFS_BTRFS_MAGIC:
			case STATFS_XFS_MAGIC:
				last_status = 1;
				break;
			default:
				last_status = 0;
				break;
		}
	}

	return last_status;
}

static int is_reflink(struct file *xa, struct file *xb)
{
	int last = 0, rc = 0;
	char abuf[BUFSIZ] = { 0 },
	     bbuf[BUFSIZ] = { 0 };

	struct fiemap *amap = (struct fiemap *) abuf,
		      *bmap = (struct fiemap *) bbuf;

	int af = open(xa->links->path, O_RDONLY),
	    bf = open(xb->links->path, O_RDONLY);

	if (af < 0 || bf < 0)
		goto done;

	do {
		size_t i;

		amap->fm_length = ~0ULL;
		amap->fm_flags = FIEMAP_FLAG_SYNC;
		amap->fm_extent_count =	(sizeof(abuf) - sizeof(*amap)) / sizeof(struct fiemap_extent);

		bmap->fm_length = ~0ULL;
		bmap->fm_flags = FIEMAP_FLAG_SYNC;
		bmap->fm_extent_count =	(sizeof(bbuf) - sizeof(*bmap)) / sizeof(struct fiemap_extent);

		if (ioctl(af, FS_IOC_FIEMAP, (unsigned long) amap) < 0)
			goto done;
		if (ioctl(bf, FS_IOC_FIEMAP, (unsigned long) bmap) < 0)
			goto done;

		if (amap->fm_mapped_extents == 0 ||
		    amap->fm_mapped_extents != bmap->fm_mapped_extents)
			goto done;

		for (i = 0; i < amap->fm_mapped_extents; i++) {
			struct fiemap_extent *a = &amap->fm_extents[i];
			struct fiemap_extent *b = &bmap->fm_extents[i];

			if (a->fe_logical != b->fe_logical ||
			    a->fe_length !=  b->fe_length ||
			    a->fe_physical != b->fe_physical)
				goto done;
			if (!(a->fe_flags & FIEMAP_EXTENT_SHARED) ||
			    !(b->fe_flags & FIEMAP_EXTENT_SHARED))
				goto done;
			if (a->fe_flags & FIEMAP_EXTENT_LAST)
				last = 1;
		}

		bmap->fm_start = amap->fm_start =
			amap->fm_extents[amap->fm_mapped_extents - 1].fe_logical +
			amap->fm_extents[amap->fm_mapped_extents - 1].fe_length;
	} while (last == 0);

	rc = 1;
done:
	if (af >= 0)
		close(af);
	if (bf >= 0)
		close(bf);
	return rc;
}
#endif /* USE_REFLINK */

static inline size_t count_nodes(struct file *x)
{
	size_t ct = 0;

	for ( ; x !=  NULL; x = x->next)
		ct++;

	return ct;
}

/**
 * visitor - Callback for twalk()
 * @nodep: Pointer to a pointer to a #struct file
 * @which: At which point this visit is (preorder, postorder, endorder)
 * @depth: The depth of the node in the tree
 *
 * Visit the nodes in the binary tree. For each node, call hardlinker()
 * on each #struct file in the linked list of #struct file instances located
 * at that node.
 */
static void visitor(const void *nodep, const VISIT which, const int depth)
{
	struct file *master = *(struct file **)nodep;
	struct file *begin = master;
	struct file *other;

	(void)depth;

	if (which != leaf && which != endorder)
		return;

	for (; master != NULL; master = master->next) {
		size_t nnodes, memsiz;
		int may_reflink = 0;

		if (handle_interrupt())
			exit(EXIT_FAILURE);
		if (master->links == NULL)
			continue;

		/* calculate per file max memory use */
		nnodes = count_nodes(master);
		if (!nnodes)
			continue;

		/* per-file cache size */
		memsiz = opts.cache_size / nnodes;
		/*                                filesiz,      readsiz,      memsiz */
		ul_fileeq_set_size(&fileeq, master->st.st_size, opts.io_size, memsiz);

#ifdef USE_REFLINK
		if (reflink_mode || reflinks_skip) {
			may_reflink =
				reflink_mode == REFLINK_ALWAYS ? 1 :
				is_reflink_compatible(master->st.st_dev,
							    master->links->path);
		}
#endif
		for (other = master->next; other != NULL; other = other->next) {
			int eq;

			if (handle_interrupt())
				exit(EXIT_FAILURE);

			assert(other != other->next);
			assert(other->st.st_size == master->st.st_size);

			if (!other->links)
				continue;

			/* check file attributes, etc. */
			if (!file_may_link_to(master, other)) {
				jlog(JLOG_VERBOSE2,
				     _("Skipped (attributes mismatch) %s"), other->links->path);
				continue;
			}
#ifdef USE_REFLINK
			if (may_reflink && reflinks_skip && is_reflink(master, other)) {
				jlog(JLOG_VERBOSE2,
				     _("Skipped (already reflink) %s"), other->links->path);
				stats.ignored_reflinks++;
				continue;
			}
#endif
			/* initialize content comparison */
			if (!ul_fileeq_data_associated(&master->data))
				ul_fileeq_data_set_file(&master->data, master->links->path);
			if (!ul_fileeq_data_associated(&other->data))
				ul_fileeq_data_set_file(&other->data, other->links->path);

			/* compare files */
			eq = ul_fileeq(&fileeq, &master->data, &other->data);

			/* reduce number of open files, keep only master open */
			ul_fileeq_data_close_file(&other->data);

			stats.comparisons++;

			if (!eq) {
				jlog(JLOG_VERBOSE2,
				     _("Skipped (content mismatch) %s"), other->links->path);
				continue;
			}

			/* link files */
			if (!file_link(master, other, may_reflink) && errno == EMLINK) {
				ul_fileeq_data_deinit(&master->data);
				master = other;
			}
		}

		/* don't keep master data in memory */
		ul_fileeq_data_deinit(&master->data);
	}

	/* final cleanup */
	for (other = begin; other != NULL; other = other->next) {
		if (ul_fileeq_data_associated(&other->data))
			ul_fileeq_data_deinit(&other->data);
	}
}

/**
 * usage - Print the program help and exit
 */
static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;

	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options] <directory>|<file> ...\n"),
		program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Consolidate duplicate files using hardlinks.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -c, --content              compare only file contents, same as -pot\n"), out);
	fputs(_(" -b, --io-size <size>       I/O buffer size for file reading\n"
	        "                              (speedup, using more RAM)\n"), out);
	fputs(_(" -d, --respect-dir          directory names have to be identical\n"), out);
	fputs(_(" -f, --respect-name         filenames have to be identical\n"), out);
	fputs(_(" -i, --include <regex>      regular expression to include files/dirs\n"), out);
	fputs(_(" -m, --maximize             maximize the hardlink count, remove the file with\n"
	        "                              lowest hardlink count\n"), out);
	fputs(_(" -M, --minimize             reverse the meaning of -m\n"), out);
	fputs(_(" -n, --dry-run              don't actually link anything\n"), out);
	fputs(_(" -o, --ignore-owner         ignore owner changes\n"), out);
	fputs(_(" -O, --keep-oldest          keep the oldest file of multiple equal files\n"
		"                              (lower precedence than minimize/maximize)\n"), out);
	fputs(_(" -p, --ignore-mode          ignore changes of file mode\n"), out);
	fputs(_(" -q, --quiet                quiet mode - don't print anything\n"), out);
	fputs(_(" -r, --cache-size <size>    memory limit for cached file content data\n"), out);
	fputs(_(" -s, --minimum-size <size>  minimum size for files.\n"), out);
	fputs(_(" -S, --maximum-size <size>  maximum size for files.\n"), out);
	fputs(_(" -t, --ignore-time          ignore timestamps (when testing for equality)\n"), out);
	fputs(_(" -v, --verbose              verbose output (repeat for more verbosity)\n"), out);
	fputs(_(" -x, --exclude <regex>      regular expression to exclude files\n"), out);
#ifdef USE_XATTR
	fputs(_(" -X, --respect-xattrs       respect extended attributes\n"), out);
#endif
	fputs(_(" -y, --method <name>        file content comparison method\n"), out);

#ifdef USE_REFLINK
	fputs(_("     --reflink[=<when>]     create clone/CoW copies (auto, always, never)\n"), out);
	fputs(_("     --skip-reflinks        skip already cloned files (enabled on --reflink)\n"), out);
#endif
	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(28));
	fprintf(out, USAGE_MAN_TAIL("hardlink(1)"));

	exit(EXIT_SUCCESS);
}

/**
 * parse_options - Parse the command line options
 * @argc: Number of options
 * @argv: Array of options
 */
static int parse_options(int argc, char *argv[])
{
	enum {
		OPT_REFLINK = CHAR_MAX + 1,
		OPT_SKIP_RELINKS
	};
	static const char optstr[] = "VhvndfpotXcmMOx:y:i:r:S:s:b:q";
	static const struct option long_options[] = {
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{"verbose", no_argument, NULL, 'v'},
		{"dry-run", no_argument, NULL, 'n'},
		{"respect-name", no_argument, NULL, 'f'},
		{"respect-dir", no_argument, NULL, 'd'},
		{"ignore-mode", no_argument, NULL, 'p'},
		{"ignore-owner", no_argument, NULL, 'o'},
		{"ignore-time", no_argument, NULL, 't'},
		{"respect-xattrs", no_argument, NULL, 'X'},
		{"maximize", no_argument, NULL, 'm'},
		{"minimize", no_argument, NULL, 'M'},
		{"keep-oldest", no_argument, NULL, 'O'},
		{"exclude", required_argument, NULL, 'x'},
		{"include", required_argument, NULL, 'i'},
		{"method", required_argument, NULL, 'y' },
		{"minimum-size", required_argument, NULL, 's'},
		{"maximum-size", required_argument, NULL, 'S'},
#ifdef USE_REFLINK
		{"reflink", optional_argument, NULL, OPT_REFLINK },
		{"skip-reflinks", no_argument, NULL, OPT_SKIP_RELINKS },
#endif
		{"io-size", required_argument, NULL, 'b'},
		{"content", no_argument, NULL, 'c'},
		{"quiet", no_argument, NULL, 'q'},
		{"cache-size", required_argument, NULL, 'r'},
		{NULL, 0, NULL, 0}
	};
	static const ul_excl_t excl[] = {
		{'q', 'v'},
		{0}
	};
	int excl_st[ARRAY_SIZE(excl)] = UL_EXCL_STATUS_INIT;
	int c, content_only = 0;

	while ((c = getopt_long(argc, argv, optstr, long_options, NULL)) != -1) {

		err_exclusive_options(c, long_options, excl, excl_st);

		switch (c) {
		case 'p':
			opts.respect_mode = FALSE;
			break;
		case 'o':
			opts.respect_owner = FALSE;
			break;
		case 't':
			opts.respect_time = FALSE;
			break;
		case 'X':
			opts.respect_xattrs = TRUE;
			break;
		case 'm':
			opts.maximise = TRUE;
			break;
		case 'M':
			opts.minimise = TRUE;
			break;
		case 'O':
			opts.keep_oldest = TRUE;
			break;
		case 'f':
			opts.respect_name = TRUE;
			break;
		case 'd':
			opts.respect_dir = TRUE;
			break;
		case 'v':
			opts.verbosity++;
			break;
		case 'q':
			quiet = TRUE;
			break;
		case 'c':
			content_only = 1;
			break;
		case 'n':
			opts.dry_run = 1;
			break;
		case 'x':
			register_regex(&opts.exclude, optarg);
			break;
		case 'y':
			opts.method = optarg;
			break;
		case 'i':
			register_regex(&opts.include, optarg);
			break;
		case 's':
			opts.min_size = strtosize_or_err(optarg, _("failed to parse minimum size"));
			break;
		case 'S':
			opts.max_size = strtosize_or_err(optarg, _("failed to parse maximum size"));
			break;
		case 'r':
			opts.cache_size = strtosize_or_err(optarg, _("failed to parse cache size"));
			break;
		case 'b':
			opts.io_size = strtosize_or_err(optarg, _("failed to parse I/O size"));
			break;
#ifdef USE_REFLINK
		case OPT_REFLINK:
			reflink_mode = REFLINK_AUTO;
			if (optarg) {
				if (strcmp(optarg, "auto") == 0)
					reflink_mode = REFLINK_AUTO;
				else if (strcmp(optarg, "always") == 0)
					reflink_mode = REFLINK_ALWAYS;
				else if (strcmp(optarg, "never") == 0)
					reflink_mode = REFLINK_NEVER;
				else
					errx(EXIT_FAILURE, _("unsupported reflink mode; %s"), optarg);
			}
			if (reflink_mode != REFLINK_NEVER)
				reflinks_skip = 1;
			break;
		case OPT_SKIP_RELINKS:
			reflinks_skip = 1;
			break;
#endif
		case 'h':
			usage();
		case 'V':
		{
			static const char *features[] = {
#ifdef USE_REFLINK
				"reflink",
#endif
#ifdef USE_FILEEQ_CRYPTOAPI
				"cryptoapi",
#endif
				NULL
			};
			print_version_with_features(EXIT_SUCCESS, features);
		}
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (content_only) {
		opts.respect_mode = FALSE;
		opts.respect_name = FALSE;
		opts.respect_dir = FALSE;
		opts.respect_owner = FALSE;
		opts.respect_time = FALSE;
		opts.respect_xattrs = FALSE;
	}
	return 0;
}

/**
* to_be_called_atexit - Cleanup handler, also prints statistics.
*/
static void to_be_called_atexit(void)
{
	if (stats.started)
		print_stats();
}

/**
* sighandler - Signal handler, sets the global last_signal variable
* @i: The signal number
*/
static void sighandler(int i)
{
	if (last_signal != SIGINT)
		last_signal = i;
	if (i == SIGINT)
		/* can't use stdio on signal handler */
		ignore_result(write(STDOUT_FILENO, "\n", sizeof("\n")-1));
}

int main(int argc, char *argv[])
{
	struct sigaction sa;
	int rc;

	sa.sa_handler = sighandler;
	sa.sa_flags = SA_RESTART;
	sigfillset(&sa.sa_mask);

	/* If we receive a SIGINT, end the processing */
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGUSR1, &sa, NULL);

	/* Localize messages, number formatting, and anything else. */
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	if (atexit(to_be_called_atexit) != 0)
		err(EXIT_FAILURE, _("cannot register exit handler"));

	parse_options(argc, argv);

	if (optind == argc)
		errx(EXIT_FAILURE, _("no directory or file specified"));

	gettime_monotonic(&stats.start_time);

	rc = ul_fileeq_init(&fileeq, opts.method);
	if (rc != 0 && strcmp(opts.method, "memcmp") != 0) {
		jlog(JLOG_INFO, _("cannot initialize %s method, use 'memcmp' fallback"), opts.method);
		opts.method = "memcmp";
		rc = ul_fileeq_init(&fileeq, opts.method);
	}
	if (rc < 0)
		err(EXIT_FAILURE, _("failed to initialize files comparior"));

	/* defautl I/O size */
	if (!opts.io_size) {
		if (strcmp(opts.method, "memcmp") == 0)
			opts.io_size = 8*1024;
		else
			opts.io_size = 1024*1024;
	}

	stats.started = TRUE;

	jlog(JLOG_VERBOSE2, _("Scanning [device/inode/links]:"));
	for (; optind < argc; optind++) {
		char *path = realpath(argv[optind], NULL);

		if (!path) {
			warn(_("cannot get realpath: %s"), argv[optind]);
			continue;
		}
		if (opts.respect_dir)
			rootbasesz = strlen(path);
		if (nftw(path, inserter, 20, FTW_PHYS) == -1)
			warn(_("cannot process %s"), path);
		free(path);
		rootbasesz = 0;
	}

	twalk(files, visitor);

	ul_fileeq_deinit(&fileeq);
	return 0;
}
