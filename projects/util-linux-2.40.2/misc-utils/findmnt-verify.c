#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libmount.h>
#include <blkid.h>
#include <sys/utsname.h>

#include "nls.h"
#include "c.h"
#include "strutils.h"
#include "xalloc.h"
#include "pathnames.h"
#include "match.h"

#include "findmnt.h"

struct verify_context {
	struct libmnt_fs	*fs;
	struct libmnt_table	*tb;

	char	**fs_ary;
	size_t	fs_num;
	size_t  fs_alloc;

	int	nwarnings;
	int	nerrors;

	unsigned int	target_printed : 1,
			no_fsck : 1;
};

static void __attribute__ ((__format__ (__printf__, 3, 0)))
	verify_mesg(struct verify_context *vfy, char type, const char *fmt, va_list ap)
{
	if (!vfy->target_printed && vfy->fs) {
		fprintf(stdout, "%s\n", mnt_fs_get_target(vfy->fs));
		vfy->target_printed = 1;
	}

	fprintf(stdout, "   [%c] ", type);
	vfprintf(stdout, fmt, ap);
	fputc('\n', stdout);
}

static int __attribute__ ((__format__ (__printf__, 2, 3)))
	verify_warn(struct verify_context *vfy, const char *fmt, ...)
{
	va_list ap;
	vfy->nwarnings++;
	va_start(ap, fmt);
	verify_mesg(vfy, 'W', fmt, ap);
	va_end(ap);
	return 0;
}

static int __attribute__ ((__format__ (__printf__, 2, 3)))
	verify_err(struct verify_context *vfy, const char *fmt, ...)
{
	va_list ap;
	vfy->nerrors++;
	va_start(ap, fmt);
	verify_mesg(vfy, 'E', fmt, ap);
	va_end(ap);
	return 0;
}

static int __attribute__ ((__format__ (__printf__, 2, 3)))
	verify_ok(struct verify_context *vfy __attribute__((unused)),
		      const char *fmt, ...)
{
	va_list ap;

	if (!(flags & FL_VERBOSE))
		return 0;

	va_start(ap, fmt);
	verify_mesg(vfy, ' ', fmt, ap);
	va_end(ap);
	return 0;
}

static int verify_order(struct verify_context *vfy)
{
	struct libmnt_iter *itr = NULL;
	struct libmnt_fs *next;
	const char *tgt;

	tgt = mnt_fs_get_target(vfy->fs);
	if (tgt && !(flags & FL_NOCACHE))
		tgt  = mnt_resolve_target(tgt, cache);
	else if (!tgt)
		return 0;

	itr = mnt_new_iter(MNT_ITER_FORWARD);
	if (!itr) {
		warn(_("failed to initialize libmount iterator"));
		goto done;
	}

	/* set iterator position to 'fs' */
	mnt_table_set_iter(vfy->tb, itr, vfy->fs);

	if (mnt_table_next_fs(vfy->tb, itr, &next) != 0)
		goto done;

	/* scan all next filesystems */
	while (mnt_table_next_fs(vfy->tb, itr, &next) == 0) {
		const char *n_tgt;
		size_t len;

		n_tgt = mnt_fs_get_target(next);
		if (n_tgt && !(flags & FL_NOCACHE))
			n_tgt  = mnt_resolve_target(n_tgt, cache);
		else if (!n_tgt)
			continue;
		len = strlen(n_tgt);

		if (strncmp(n_tgt, tgt, len) == 0) {
			if (*(tgt + len) == '\0')
				verify_warn(vfy, _("target specified more than once"));
			else if (*(tgt + len) == '/')
				verify_err(vfy, _("wrong order: %s specified before %s"), tgt, n_tgt);
		}
	}
done:
	mnt_free_iter(itr);
	return 0;
}

static int verify_target(struct verify_context *vfy)
{
	const char *tgt = mnt_fs_get_target(vfy->fs);
	struct stat sb;

	if (!tgt)
		return verify_err(vfy, _("undefined target (fs_file)"));

	if (!(flags & FL_NOCACHE)) {
		const char *cn = mnt_resolve_target(tgt, cache);
		if (!cn)
			return -ENOMEM;
		if (strcmp(cn, tgt) != 0)
			verify_warn(vfy, _("non-canonical target path (real: %s)"), cn);
		tgt = cn;
	}
	if (stat(tgt, &sb) != 0) {
		if (mnt_fs_get_option(vfy->fs, "noauto", NULL, NULL) == 1)
			verify_err(vfy, _("unreachable on boot required target: %m"));
		else
			verify_warn(vfy, _("unreachable target: %m"));

	} else if (!S_ISDIR(sb.st_mode)
		 && mnt_fs_get_option(vfy->fs, "bind", NULL, NULL) == 1) {
		verify_err(vfy, _("target is not a directory"));
	} else
		verify_ok(vfy, _("target exists"));

	return 0;
}

static char *verify_tag(struct verify_context *vfy, const char *name,
		      const char *value)
{
	char *src = NULL;

	if (!(flags & FL_NOCACHE))
		src = mnt_resolve_tag(name, value, cache);

	if (!src) {
		if (mnt_fs_get_option(vfy->fs, "noauto", NULL, NULL) == 1)
			verify_err(vfy, _("unreachable on boot required source: %s=%s"), name, value);
		else
			verify_warn(vfy, _("unreachable: %s=%s"), name, value);
	} else
		verify_ok(vfy, _("%s=%s translated to %s"), name, value, src);

	return src;
}

/* Note that mount source is very FS specific and we should not
 * interpret unreachable source as error. The exception is only
 * NAME=value, this has to be convertible to device name.
 */
static int verify_source(struct verify_context *vfy)
{
	const char *src = mnt_fs_get_srcpath(vfy->fs);
	char *t = NULL, *v = NULL;
	struct stat sb;
	int isbind, rc = 0;

	/* source is NAME=value tag */
	if (!src) {
		const char *tag = NULL, *val = NULL;

		if (mnt_fs_get_tag(vfy->fs, &tag, &val) != 0)
			return verify_err(vfy, _("undefined source (fs_spec)"));

		src = verify_tag(vfy, tag, val);
		if (!src)
			goto done;

	/* blkid is able to parse it, but libmount does not see it as a tag --
	 * it means unsupported tag */
	} else if (blkid_parse_tag_string(src, &t, &v) == 0 && stat(src, &sb) != 0) {
		rc = verify_err(vfy, _("unsupported source tag: %s"), src);
		goto done;
	}
	isbind = mnt_fs_get_option(vfy->fs, "bind", NULL, NULL) == 0;

	/* source is path */
	if (mnt_fs_is_pseudofs(vfy->fs) || mnt_fs_is_netfs(vfy->fs))
		verify_ok(vfy, _("do not check %s source (pseudo/net)"), src);

	else if (stat(src, &sb) != 0)
		verify_warn(vfy, _("unreachable source: %s: %m"), src);

	else if ((S_ISDIR(sb.st_mode) || S_ISREG(sb.st_mode)) && !isbind)
		verify_warn(vfy, _("non-bind mount source %s is a directory or regular file"), src);

	else if (!S_ISBLK(sb.st_mode) && !isbind)
		verify_warn(vfy, _("source %s is not a block device"), src);
	else
		verify_ok(vfy, _("source %s exists"), src);
done:
	free(t);
	free(v);
	return rc;
}

static int verify_options(struct verify_context *vfy)
{
	const char *opts;

	opts = mnt_fs_get_vfs_options(vfy->fs);
	if (opts)
		verify_ok(vfy, _("VFS options: %s"), opts);

	opts = mnt_fs_get_fs_options(vfy->fs);
	if (opts)
		verify_ok(vfy, _("FS options: %s"), opts);

	opts = mnt_fs_get_user_options(vfy->fs);
	if (opts)
		verify_ok(vfy, _("userspace options: %s"), opts);

	return 0;
}

static int verify_swaparea(struct verify_context *vfy)
{
	char *arg;
	size_t argsz = 0;

	if (mnt_fs_get_option(vfy->fs, "discard", &arg, &argsz) == 0
	    && arg
	    && strncmp(arg, "once", argsz) != 0
	    && strncmp(arg, "pages", argsz) != 0)
		verify_err(vfy, _("unsupported swaparea discard policy: %s"), arg);

	if (mnt_fs_get_option(vfy->fs, "pri", &arg, &argsz) == 0 && arg) {
		char *p = arg;
		if (*p == '-')
			p++;
		for (; p < arg + argsz; p++) {
			if (!isdigit((unsigned char) *p)) {
				verify_err(vfy, _("failed to parse swaparea priority option"));
				break;
			}
		}
	}

	return 0;
}

static int is_supported_filesystem(struct verify_context *vfy, const char *name)
{
	size_t n;

	if (!vfy->fs_num)
		return 0;

	for (n = 0; n < vfy->fs_num; n++ ) {
		if (match_fstype(vfy->fs_ary[n], name))
			return 1;
	}

	return 0;
}

static int add_filesystem(struct verify_context *vfy, const char *name)
{
	#define MYCHUNK	16

	if (is_supported_filesystem(vfy, name))
		return 0;

	if (vfy->fs_alloc == 0 || vfy->fs_num + 1 <= vfy->fs_alloc) {
		vfy->fs_alloc = ((vfy->fs_alloc + 1 + MYCHUNK) / MYCHUNK) * MYCHUNK;
		vfy->fs_ary = xreallocarray(vfy->fs_ary, vfy->fs_alloc, sizeof(char *));
	}

	vfy->fs_ary[vfy->fs_num] = xstrdup(name);
	vfy->fs_num++;

	return 0;
}

static int read_proc_filesystems(struct verify_context *vfy)
{
	int rc = 0;
	FILE *f;
	char buf[80], *cp, *t;

	f = fopen("/proc/filesystems", "r");
	if (!f)
		return -errno;

	while (!feof(f)) {
		if (!fgets(buf, sizeof(buf), f))
			break;
		cp = buf;
		if (!isspace(*cp)) {
			while (*cp && !isspace(*cp))
				cp++;
		}
		while (*cp && isspace(*cp))
			cp++;
		if ((t = strchr(cp, '\n')) != NULL)
			*t = 0;
		if ((t = strchr(cp, '\t')) != NULL)
			*t = 0;
		if ((t = strchr(cp, ' ')) != NULL)
			*t = 0;

		rc = add_filesystem(vfy, cp);
		if (rc)
			break;
	}
	fclose(f);
	return rc;
}

static void free_proc_filesystems(struct verify_context *vfy)
{
	size_t n;

	if (!vfy->fs_ary)
		return;

	for (n = 0; n < vfy->fs_num; n++ )
		free(vfy->fs_ary[n]);
	free(vfy->fs_ary);
}

static int read_kernel_filesystems(struct verify_context *vfy)
{
	int rc = 0;
#ifdef __linux__
	struct utsname uts;
	FILE *f;
	char buf[1024];

	if (uname(&uts))
		return 0;
	snprintf(buf, sizeof(buf), "/lib/modules/%s/modules.dep", uts.release);

	f = fopen(buf, "r");
	if (!f)
		return 0;

	while (!feof(f)) {
		char *p, *name;

		if (!fgets(buf, sizeof(buf), f))
			break;

		if (strncmp("kernel/fs/", buf, 10) != 0 ||
		    strncmp("kernel/fs/nls/", buf, 14) == 0)
			continue;

		p = strchr(buf, ':');
		if (!p)
			continue;
		*p = '\0';

		name = strrchr(buf, '/');
		if (!name)
			continue;
		name++;

		p = strstr(name, ".ko");
		if (!p)
			continue;
		*p = '\0';

		rc = add_filesystem(vfy, name);
		if (rc)
			break;
	}
	fclose(f);
#endif /* __linux__ */
	return rc;
}

static int verify_fstype(struct verify_context *vfy)
{
	char *src = mnt_resolve_spec(mnt_fs_get_source(vfy->fs), cache);
	char *realtype = NULL;
	const char *type;
	int ambi = 0, isauto = 0, isswap = 0;

	if (!src)
		return 0;

	if (mnt_fs_is_pseudofs(vfy->fs) || mnt_fs_is_netfs(vfy->fs)) {
		verify_ok(vfy, _("do not check %s FS type (pseudo/net)"), src);
		goto done;
	}

	type = mnt_fs_get_fstype(vfy->fs);

	if (type) {
		int none = strcmp(type, "none") == 0;

		if (none
		    && mnt_fs_get_option(vfy->fs, "bind", NULL, NULL) == 1
		    && mnt_fs_get_option(vfy->fs, "move", NULL, NULL) == 1) {
			verify_warn(vfy, _("\"none\" FS type is recommended for bind or move oprations only"));
			goto done;
		}

		if (strcmp(type, "auto") == 0)
			isauto = 1;
		else if (strcmp(type, "swap") == 0)
			isswap = 1;
		else if (strcmp(type, "xfs") == 0 || strcmp(type, "btrfs") == 0)
			vfy->no_fsck = 1;

		if (!isswap && !isauto && !none && !is_supported_filesystem(vfy, type))
			verify_warn(vfy, _("%s seems unsupported by the current kernel"), type);
	}

	errno = 0;
	realtype = mnt_get_fstype(src, &ambi, cache);

	if (!realtype) {
		const char *reson = errno ? strerror(errno) : _("reason unknown");

		if (isauto)
			verify_err(vfy, _("cannot detect on-disk filesystem type (%s)"), reson);
		else
			verify_warn(vfy, _("cannot detect on-disk filesystem type (%s)"), reson);
		goto done;
	}

	if (realtype) {
		isswap = strcmp(realtype, "swap") == 0;
		vfy->no_fsck = strcmp(realtype, "xfs") == 0
				|| strcmp(realtype, "btrfs") == 0;

		if (type && !isauto && strcmp(type, realtype) != 0) {
			verify_warn(vfy, _("%s does not match with on-disk %s"), type, realtype);
			goto done;
		}
		if (!isswap && !is_supported_filesystem(vfy, realtype)) {
			verify_warn(vfy, _("on-disk %s seems unsupported by the current kernel"), realtype);
			goto done;
		}

		verify_ok(vfy, _("FS type is %s"), realtype);
	}

done:
	if (!cache) {
		free(src);
		free(realtype);
	}
	return 0;
}

static int verify_passno(struct verify_context *vfy)
{
	int passno = mnt_fs_get_passno(vfy->fs);
	const char *tgt = mnt_fs_get_target(vfy->fs);

	if (tgt && strcmp("/", tgt) == 0 && passno != 1 && !vfy->no_fsck)
		return verify_warn(vfy, _("recommended root FS passno is 1 (current is %d)"), passno);

	return 0;
}

static int verify_filesystem(struct verify_context *vfy)
{
	int rc = 0;

	if (mnt_fs_is_swaparea(vfy->fs))
		rc = verify_swaparea(vfy);
	else {
		rc = verify_target(vfy);
		if (!rc)
			rc = verify_options(vfy);
	}

	if (!rc)
		rc = verify_source(vfy);
	if (!rc)
		rc = verify_fstype(vfy);
	if (!rc)
		rc = verify_passno(vfy);	/* depends on verify_fstype() */

	return rc;
}

int verify_table(struct libmnt_table *tb)
{
	struct verify_context vfy = { .nerrors = 0 };
	struct libmnt_iter *itr;
	int rc = 0;		/* overall return code (alloc errors, etc.) */
	int check_order = is_listall_mode();
	static int has_read_fs = 0;

	itr = mnt_new_iter(MNT_ITER_FORWARD);
	if (!itr) {
		warn(_("failed to initialize libmount iterator"));
		goto done;
	}

	vfy.tb = tb;

	if (has_read_fs == 0) {
		read_proc_filesystems(&vfy);
		read_kernel_filesystems(&vfy);
		has_read_fs = 1;
	}

	while (rc == 0 && (vfy.fs = get_next_fs(tb, itr))) {
		vfy.target_printed = 0;
		vfy.no_fsck = 0;

		if (check_order)
			rc = verify_order(&vfy);
		if (!rc)
			rc = verify_filesystem(&vfy);

		if (flags & FL_FIRSTONLY)
			break;
		flags |= FL_NOSWAPMATCH;
	}

#ifdef USE_SYSTEMD
	{
		struct stat a, b;

		if (stat(_PATH_SD_UNITSLOAD, &a) == 0 &&
		    stat(_PATH_MNTTAB, &b) == 0 &&
		    cmp_stat_mtime(&a, &b, <))
			verify_warn(&vfy, _(
	"your fstab has been modified, but systemd still uses the old version;\n"
	"       use 'systemctl daemon-reload' to reload"));
	}
#endif

done:
	mnt_free_iter(itr);

	/* summary */
	if (vfy.nerrors || parse_nerrors || vfy.nwarnings) {
		fputc('\n', stderr);
		fprintf(stderr, P_("%d parse error", "%d parse errors", parse_nerrors), parse_nerrors);
		fprintf(stderr, P_(", %d error",     ", %d errors", vfy.nerrors), vfy.nerrors);
		fprintf(stderr, P_(", %d warning",   ", %d warnings", vfy.nwarnings), vfy.nwarnings);
		fputc('\n', stderr);
	} else
		fprintf(stdout, _("Success, no errors or warnings detected\n"));


	free_proc_filesystems(&vfy);

	return rc != 0 ? rc : vfy.nerrors + parse_nerrors;
}
